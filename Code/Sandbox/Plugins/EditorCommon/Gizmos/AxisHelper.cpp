// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AxisHelper.h"

#include "Objects/DisplayContext.h"
#include "IDisplayViewport.h"
#include "HitContext.h"
#include "Util/Math.h"
#include <CryMath/Cry_Geo.h>

#define PLANE_SCALE  (0.3f)
#define HIT_RADIUS   (8)
#define BOLD_LINE_3D (4)
#define BOLD_LINE_2D (2)

const float kSelectionBallScale = 0.05f;
const float kRotateCircleRadiusScale = 0.75f;

//////////////////////////////////////////////////////////////////////////
CAxisHelper::CAxisHelper()
	: m_matrix(IDENTITY)
{
	m_nModeFlags = MOVE_FLAG;
	m_highlightMode = MOVE_MODE;
	m_highlightAxis = 0;
	m_UnchangedAxis = eAxis_X;

	m_bNeedX = true;
	m_bNeedY = true;
	m_bNeedZ = true;
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelper::SetMode(int nModeFlags)
{
	m_nModeFlags = nModeFlags;
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelper::Prepare(const Matrix34& worldTM, const SGizmoPreferences& setup, IDisplayViewport* view, float fScaleRatio)
{
	RefCoordSys refCoordSys = setup.referenceCoordSys;

	m_fScreenScale = view->GetScreenScaleFactor(worldTM.GetTranslation()) * fScaleRatio;
	m_size = setup.axisGizmoSize * m_fScreenScale;

	m_bNeedX = true;
	m_bNeedY = true;
	m_bNeedZ = true;

	IDisplayViewport::EAxis axis = IDisplayViewport::AXIS_NONE;
	m_b2D = false;

	view->GetPerpendicularAxis(&axis, &m_b2D);

	// Disable axis if needed (we could be smarter here and just disable based on dot product between gizmo axes and view direction)
	if (m_b2D && refCoordSys == COORDS_WORLD)
	{
		switch (axis)
		{
		case AXIS_X:
			m_bNeedX = false;
			break;
		case AXIS_Y:
			m_bNeedY = false;
			break;
		case AXIS_Z:
			m_bNeedZ = false;
			break;
		}
	}

	// use correct matrices
	if (m_b2D && refCoordSys == COORDS_VIEW)
	{
		m_matrix = view->GetViewTM();
		m_matrix.SetTranslation(worldTM.GetTranslation());

		m_bNeedZ = false;
	}
	else
	{
		m_matrix = worldTM;
	}

	m_matrix.OrthonormalizeFast();
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelper::DrawAxis(const Matrix34& worldTM, const SGizmoPreferences& setup, DisplayContext& dc, float fScaleRatio)
{
	Prepare(worldTM, setup, dc.view, fScaleRatio);

	Vec3 x(m_size, 0, 0);
	Vec3 y(0, m_size, 0);
	Vec3 z(0, 0, m_size);

	int prevRState = dc.GetState();

	if (!(dc.flags & DISPLAY_2D))
		dc.DepthTestOff();

	dc.PushMatrix(m_matrix);
	dc.SetDrawInFrontMode(true);

	Vec3 colSelected(1, 1, 0);
	Vec3 axisColor(1, 1, 1);
	Vec3 disabledColor(0.75, 0.75, 0.75);

	Matrix34 worldTMWithoutScale(worldTM);
	worldTMWithoutScale.OrthonormalizeFast();
	float textSize = 1.4f;

	dc.SetColor(setup.enabled ? axisColor : disabledColor);
	if (m_bNeedX && setup.axisGizmoText)
		dc.DrawTextLabel(ConvertToTextPos(x, worldTMWithoutScale, dc.view, dc.flags & DISPLAY_2D), textSize, "x");
	if (m_bNeedY && setup.axisGizmoText)
		dc.DrawTextLabel(ConvertToTextPos(y, worldTMWithoutScale, dc.view, dc.flags & DISPLAY_2D), textSize, "y");
	if (m_bNeedZ && setup.axisGizmoText)
		dc.DrawTextLabel(ConvertToTextPos(z, worldTMWithoutScale, dc.view, dc.flags & DISPLAY_2D), textSize, "z");

	int axis = setup.axisConstraint;
	if (m_highlightAxis)
		axis = m_highlightAxis;

	int nBoldWidth = BOLD_LINE_3D;
	if (dc.flags & DISPLAY_2D)
	{
		nBoldWidth = BOLD_LINE_2D;
	}

	float linew[3];
	linew[0] = linew[1] = linew[2] = 0;
	Vec3 colX, colY, colZ;
	if (setup.enabled)
	{
		colX = Vec3(1, 0, 0);
		colY = Vec3(0, 1, 0);
		colZ = Vec3(0, 0, 1);
	}
	else
	{
		colX = colY = colZ = disabledColor;
	}
	Vec3 colXArrow = colX, colYArrow = colY, colZArrow = colZ;
	if (axis)
	{
		float col[4] = { 1, 0, 0, 1 };
		if (axis == AXIS_X || axis == AXIS_XY || axis == AXIS_XZ || axis == AXIS_XYZ)
		{
			colX = colSelected;
			dc.SetColor(colSelected);
			if (m_bNeedX && setup.axisGizmoText)
				dc.DrawTextLabel(ConvertToTextPos(x, worldTMWithoutScale, dc.view, dc.flags & DISPLAY_2D), textSize, "x");
			linew[0] = float(nBoldWidth);
		}
		if (axis == AXIS_Y || axis == AXIS_XY || axis == AXIS_YZ || axis == AXIS_XYZ)
		{
			colY = colSelected;
			dc.SetColor(colSelected);
			if (m_bNeedY && setup.axisGizmoText)
				dc.DrawTextLabel(ConvertToTextPos(y, worldTMWithoutScale, dc.view, dc.flags & DISPLAY_2D), textSize, "y");
			linew[1] = float(nBoldWidth);
		}
		if (axis == AXIS_Z || axis == AXIS_XZ || axis == AXIS_YZ || axis == AXIS_XYZ)
		{
			colZ = colSelected;
			dc.SetColor(colSelected);
			if (m_bNeedZ && setup.axisGizmoText)
				dc.DrawTextLabel(ConvertToTextPos(z, worldTMWithoutScale, dc.view, dc.flags & DISPLAY_2D), textSize, "z");
			linew[2] = float(nBoldWidth);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Calc sizes.
	//////////////////////////////////////////////////////////////////////////
	float headOfs[MAX_HELPER_MODES];
	float headScl[MAX_HELPER_MODES];

	headOfs[MOVE_MODE] = 0;
	headScl[MOVE_MODE] = m_size;
	headOfs[ROTATE_MODE] = 0;
	headScl[ROTATE_MODE] = m_size;
	headOfs[SCALE_MODE] = 0;
	headScl[SCALE_MODE] = m_size;
	if (m_nModeFlags == (MOVE_FLAG | ROTATE_FLAG | SCALE_FLAG))
	{
		headOfs[ROTATE_MODE] += 0.05f * m_size;
		headOfs[SCALE_MODE] += 0.1f * m_size;
	}
	//////////////////////////////////////////////////////////////////////////

	if (m_bNeedX)
	{
		dc.SetColor(colX);
		dc.SetLineWidth(linew[0]);
		dc.DrawLine(Vec3(0, 0, 0), x + Vec3(headOfs[SCALE_MODE], 0, 0));
	}
	if (m_bNeedY)
	{
		dc.SetColor(colY);
		dc.SetLineWidth(linew[1]);
		dc.DrawLine(Vec3(0, 0, 0), y + Vec3(0, headOfs[SCALE_MODE], 0));
	}
	if (m_bNeedZ)
	{
		dc.SetColor(colZ);
		dc.SetLineWidth(linew[2]);
		dc.DrawLine(Vec3(0, 0, 0), z + Vec3(0, 0, headOfs[SCALE_MODE]));
	}

	//////////////////////////////////////////////////////////////////////////
	// Draw Move Arrows.
	//////////////////////////////////////////////////////////////////////////
	if (m_nModeFlags & MOVE_FLAG)
	{
		if (m_bNeedX)
		{
			if (m_highlightMode == MOVE_MODE) dc.SetColor(colX); else dc.SetColor(colXArrow);
			dc.DrawArrow(x, 1.1f * x, headScl[MOVE_MODE] * 0.75f);
		}
		if (m_bNeedY)
		{
			if (m_highlightMode == MOVE_MODE) dc.SetColor(colY); else dc.SetColor(colYArrow);
			dc.DrawArrow(y, 1.1f * y, headScl[MOVE_MODE] * 0.75f);
		}
		if (m_bNeedZ)
		{
			if (m_highlightMode == MOVE_MODE) dc.SetColor(colZ); else dc.SetColor(colZArrow);
			dc.DrawArrow(z, 1.1f * z, headScl[MOVE_MODE] * 0.75f);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Draw Rotate Spheres.
	//////////////////////////////////////////////////////////////////////////
	if (m_nModeFlags & ROTATE_FLAG)
	{
		dc.SetLineWidth(4.0f);
		float ball_radius = headScl[ROTATE_MODE] * 0.1f;
		if (m_bNeedX)
		{
			if (m_highlightMode == ROTATE_MODE) dc.SetColor(colX); else dc.SetColor(colXArrow);
			dc.DrawBall(x + Vec3(ball_radius, 0, 0), ball_radius);
		}
		if (m_bNeedY)
		{
			if (m_highlightMode == ROTATE_MODE) dc.SetColor(colY); else dc.SetColor(colYArrow);
			dc.DrawBall(y + Vec3(0, ball_radius, 0), ball_radius);
		}
		if (m_bNeedZ)
		{
			if (m_highlightMode == ROTATE_MODE) dc.SetColor(colZ); else dc.SetColor(colZArrow);
			dc.DrawBall(z + Vec3(0, 0, ball_radius), ball_radius);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Draw Scale Boxes.
	//////////////////////////////////////////////////////////////////////////
	if (m_nModeFlags & SCALE_FLAG)
	{
		if (axis == AXIS_XYZ) dc.SetColor(colSelected); else dc.SetColor(RGB(128, 128, 0));
		float size_scale = headScl[SCALE_MODE] * 0.1f;
		Vec3 boxsz = Vec3(size_scale, size_scale, size_scale);
		dc.DrawSolidBox(-boxsz, boxsz);
		if (m_bNeedX)
		{
			if (m_highlightMode == SCALE_MODE) dc.SetColor(colX); else dc.SetColor(colXArrow);
			dc.DrawSolidBox(x - boxsz + Vec3(size_scale, 0, 0), x + boxsz + Vec3(size_scale, 0, 0));
		}
		if (m_bNeedY)
		{
			if (m_highlightMode == SCALE_MODE) dc.SetColor(colY); else dc.SetColor(colYArrow);
			dc.DrawSolidBox(y - boxsz + Vec3(0, size_scale, 0), y + boxsz + Vec3(0, size_scale, 0));
		}
		if (m_bNeedZ)
		{
			if (m_highlightMode == SCALE_MODE) dc.SetColor(colZ); else dc.SetColor(colZArrow);
			dc.DrawSolidBox(z - boxsz + Vec3(0, 0, size_scale), z + boxsz + Vec3(0, 0, size_scale));
		}
	}
	//////////////////////////////////////////////////////////////////////////

	dc.SetLineWidth(0);

	// If only in move mode.
	if (m_nModeFlags == MOVE_FLAG)
	{
		//////////////////////////////////////////////////////////////////////////
		// Draw axis planes.
		//////////////////////////////////////////////////////////////////////////
		Vec3 colXY[2];
		Vec3 colXZ[2];
		Vec3 colYZ[2];

		if (setup.enabled)
		{
			colX = Vec3(1, 0, 0);
			colY = Vec3(0, 1, 0);
			colZ = Vec3(0, 0, 1);
		}
		else
		{
			colX = colY = colZ = disabledColor;
		}

		colXY[0] = colY;
		colXY[1] = colX;
		colXZ[0] = colZ;
		colXZ[1] = colX;
		colYZ[0] = colZ;
		colYZ[1] = colY;

		linew[0] = linew[1] = linew[2] = 0;
		if (axis)
		{
			if (axis == AXIS_XY)
			{
				colXY[0] = colSelected;
				colXY[1] = colSelected;
				linew[0] = float(nBoldWidth);
			}
			else if (axis == AXIS_XZ)
			{
				colXZ[0] = colSelected;
				colXZ[1] = colSelected;
				linew[1] = float(nBoldWidth);
			}
			else if (axis == AXIS_YZ)
			{
				colYZ[0] = colSelected;
				colYZ[1] = colSelected;
				linew[2] = float(nBoldWidth);
			}
		}

		if (!(dc.flags & DISPLAY_2D))
		{
			dc.SetColor(RGB(128, 32, 32), 0.4f);
			Vec3 org = worldTM.GetTranslation();
			dc.DrawBall(Vec3(0.0f), m_size * kSelectionBallScale);
		}

		dc.SetColor(RGB(255, 255, 0), 0.5f);

		float sz = m_size * PLANE_SCALE;
		Vec3 p1(sz, sz, 0);
		Vec3 p2(sz, 0, sz);
		Vec3 p3(0, sz, sz);

		float colAlpha = 1.0f;
		x *= PLANE_SCALE;
		y *= PLANE_SCALE;
		z *= PLANE_SCALE;

		// XY
		if (m_bNeedX && m_bNeedY)
		{
			dc.SetLineWidth(linew[0]);
			dc.SetColor(colXY[0], colAlpha);
			dc.DrawLine(p1, p1 - x);
			dc.SetColor(colXY[1], colAlpha);
			dc.DrawLine(p1, p1 - y);
		}

		// XZ
		if (m_bNeedX && m_bNeedZ)
		{
			dc.SetLineWidth(linew[1]);
			dc.SetColor(colXZ[0], colAlpha);
			dc.DrawLine(p2, p2 - x);
			dc.SetColor(colXZ[1], colAlpha);
			dc.DrawLine(p2, p2 - z);
		}

		// YZ
		if (m_bNeedY && m_bNeedZ)
		{
			dc.SetLineWidth(linew[2]);
			dc.SetColor(colYZ[0], colAlpha);
			dc.DrawLine(p3, p3 - y);
			dc.SetColor(colYZ[1], colAlpha);
			dc.DrawLine(p3, p3 - z);
		}

		dc.SetLineWidth(0);

		colAlpha = 0.25f;

		if (axis == AXIS_XY && m_bNeedX && m_bNeedY)
		{
			dc.CullOff();
			dc.SetColor(colSelected, colAlpha);
			dc.DrawQuad(p1, p1 - x, p1 - x - y, p1 - y);
			dc.CullOn();
		}
		else if (axis == AXIS_XZ && m_bNeedX && m_bNeedZ)
		{
			dc.CullOff();
			dc.SetColor(colSelected, colAlpha);
			dc.DrawQuad(p2, p2 - x, p2 - x - z, p2 - z);
			dc.CullOn();
		}
		else if (axis == AXIS_YZ && m_bNeedY && m_bNeedZ)
		{
			dc.CullOff();
			dc.SetColor(colSelected, colAlpha);
			dc.DrawQuad(p3, p3 - y, p3 - y - z, p3 - z);
			dc.CullOn();
		}
	}

	dc.PopMatrix();
	if (!(dc.flags & DISPLAY_2D))
		dc.DepthTestOn();

	dc.SetState(prevRState);
}

//////////////////////////////////////////////////////////////////////////
bool CAxisHelper::HitTest(const Matrix34& worldTM, const SGizmoPreferences& setup, HitContext& hc, EHelperMode* pManipulatorMode, float fScaleRatio)
{
	EHelperMode manipulatorMode = HELPER_MODE_NONE;

	if (hc.distanceTolerance != 0)
		return 0;

	Prepare(worldTM, setup, hc.view, fScaleRatio);

	// Get inverse matrix (world to gizmo) (useful to do collision detection in Box/Cone object space)
	// CAxisHelper::Prepare makes sure our matrix is orthonormal so we can use fast inversion
	Matrix34 inverseMatrix = m_matrix.GetInvertedFast();

	Vec3 x(m_size, 0, 0);
	Vec3 y(0, m_size, 0);
	Vec3 z(0, 0, m_size);

	float hitDist = 0.01f * m_fScreenScale;

	Vec3 pos = m_matrix.GetTranslation();

	Vec3 intPoint;
	hc.view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	Vec3 rayObjSrc = inverseMatrix.TransformPoint(hc.raySrc);
	Vec3 rayObjDir = inverseMatrix.TransformVector(hc.rayDir);

	// Ray in world space
	Ray ray(hc.raySrc, hc.rayDir);
	// Ray in object space
	Ray rayObj(rayObjSrc, rayObjDir);

	Sphere sphere(pos, m_size + 0.1f * m_fScreenScale);
	if (!Intersect::Ray_SphereFirst(ray, sphere, intPoint))
	{
		m_highlightAxis = 0;
		return false;
	}

	x = m_matrix.TransformVector(x);
	y = m_matrix.TransformVector(y);
	z = m_matrix.TransformVector(z);

	float sz = m_size * PLANE_SCALE;
	Vec3 p1(sz, sz, 0);
	Vec3 p2(sz, 0, sz);
	Vec3 p3(0, sz, sz);

	p1 = m_matrix.TransformPoint(p1);
	p2 = m_matrix.TransformPoint(p2);
	p3 = m_matrix.TransformPoint(p3);

	Vec3 planeX = x * PLANE_SCALE;
	Vec3 planeY = y * PLANE_SCALE;
	Vec3 planeZ = z * PLANE_SCALE;

	int hitRadius = HIT_RADIUS;
	int axis = 0;

	if (hc.view->HitTestLine(pos, pos + x, hc.point2d, hitRadius))
		axis = AXIS_X;
	else if (hc.view->HitTestLine(pos, pos + y, hc.point2d, hitRadius))
		axis = AXIS_Y;
	else if (hc.view->HitTestLine(pos, pos + z, hc.point2d, hitRadius))
		axis = AXIS_Z;
	else if (m_nModeFlags == MOVE_FLAG)
	{
		// If only in move mode.
		if (hc.view->HitTestLine(p1, p1 - planeX, hc.point2d, hitRadius))
			axis = AXIS_XY;
		else if (hc.view->HitTestLine(p1, p1 - planeY, hc.point2d, hitRadius))
			axis = AXIS_XY;
		else if (hc.view->HitTestLine(p2, p2 - planeX, hc.point2d, hitRadius))
			axis = AXIS_XZ;
		else if (hc.view->HitTestLine(p2, p2 - planeZ, hc.point2d, hitRadius))
			axis = AXIS_XZ;
		else if (hc.view->HitTestLine(p3, p3 - planeY, hc.point2d, hitRadius))
			axis = AXIS_YZ;
		else if (hc.view->HitTestLine(p3, p3 - planeZ, hc.point2d, hitRadius))
			axis = AXIS_YZ;
		if (axis != 0)
			manipulatorMode = MOVE_MODE;
	}

	//////////////////////////////////////////////////////////////////////////
	// Calc sizes.
	//////////////////////////////////////////////////////////////////////////
	float headOfs[MAX_HELPER_MODES];
	float headScl[MAX_HELPER_MODES];

	headOfs[MOVE_MODE] = 0;
	headScl[MOVE_MODE] = m_size;
	headOfs[ROTATE_MODE] = 0;
	headScl[ROTATE_MODE] = m_size;
	headOfs[SCALE_MODE] = 0;
	headScl[SCALE_MODE] = m_size;
	if (m_nModeFlags == (MOVE_FLAG | ROTATE_FLAG | SCALE_FLAG))
	{
		headOfs[ROTATE_MODE] += 0.05f * m_fScreenScale;
		headOfs[SCALE_MODE] += 0.1f * m_fScreenScale;
	}
	//////////////////////////////////////////////////////////////////////////
	if (axis == 0 && (m_nModeFlags & ROTATE_FLAG))
	{
		float ball_radius = headScl[ROTATE_MODE] * 0.1f;

		Vec3 x(m_size + headOfs[ROTATE_MODE] + ball_radius, 0, 0);
		Vec3 y(0, m_size + headOfs[ROTATE_MODE] + ball_radius, 0);
		Vec3 z(0, 0, m_size + headOfs[ROTATE_MODE] + ball_radius);

		if (m_bNeedX)
		{
			Sphere xsphere(x, ball_radius);

			if (Intersect::Ray_SphereFirst(rayObj, xsphere, intPoint))
			{
				axis = AXIS_X;
				manipulatorMode = ROTATE_MODE;
			}
		}
		if (m_bNeedY)
		{
			Sphere ysphere(y, ball_radius);

			if (Intersect::Ray_SphereFirst(rayObj, ysphere, intPoint))
			{
				axis = AXIS_Y;
				manipulatorMode = ROTATE_MODE;
			}
		}
		if (m_bNeedZ)
		{
			Sphere zsphere(z, ball_radius);

			if (Intersect::Ray_SphereFirst(rayObj, zsphere, intPoint))
			{
				axis = AXIS_Z;
				manipulatorMode = ROTATE_MODE;
			}
		}
	}
	if (m_nModeFlags & SCALE_FLAG)
	{
		float cubeSize = headScl[SCALE_MODE] * 0.1f;
		AABB centerBox(cubeSize);

		if (Intersect::Ray_AABB(rayObj, centerBox, intPoint))
		{
			axis = AXIS_XYZ;
			manipulatorMode = SCALE_MODE;
		}
		if (axis == 0)
		{
			if (m_bNeedX)
			{
				AABB xBox(Vec3(m_size + cubeSize, 0.0f, 0.0f), cubeSize);
				if (Intersect::Ray_AABB(rayObj, xBox, intPoint))
				{
					axis = AXIS_X;
					manipulatorMode = SCALE_MODE;
				}
			}
			if (m_bNeedY)
			{
				AABB yBox(Vec3(0.0f, m_size + cubeSize, 0.0f), cubeSize);
				if (Intersect::Ray_AABB(rayObj, yBox, intPoint))
				{
					axis = AXIS_Y;
					manipulatorMode = SCALE_MODE;
				}
			}
			if (m_bNeedZ)
			{
				AABB zBox(Vec3(0.0f, 0.0f, m_size + cubeSize), cubeSize);
				if (Intersect::Ray_AABB(rayObj, zBox, intPoint))
				{
					axis = AXIS_Z;
					manipulatorMode = SCALE_MODE;
				}
			}
		}
	}
	if (m_nModeFlags & MOVE_FLAG)
	{
		// magic numbers here try to mimic sizing logic around and inside DrawArrow
		float arrowSize = headScl[MOVE_MODE] * 0.4f * 0.75;
		float arrowRadius = headScl[MOVE_MODE] * 0.1f * 0.75;

		if (m_b2D)
		{
			float fac2d = hc.view->GetViewTM().TransformVector(Vec3(1.0, 0.0, 0.0)).GetLength() * 1.2;
			arrowRadius *= fac2d;
			arrowSize *= fac2d;
		}

		if (axis == 0)
		{
			if (m_bNeedX)
			{
				Cone xCone(Vec3(1.1f * m_size, 0.0f, 0.0f), Vec3(-1.0, 0.0f, 0.0f), arrowSize, arrowRadius);
				if (Intersect::Ray_Cone(rayObj, xCone, intPoint))
				{
					axis = AXIS_X;
					manipulatorMode = MOVE_MODE;
				}
			}
			if (m_bNeedY)
			{
				Cone yCone(Vec3(0.0f, 1.1f * m_size, 0.0f), Vec3(0.0, -1.0f, 0.0f), arrowSize, arrowRadius);

				if (Intersect::Ray_Cone(rayObj, yCone, intPoint))
				{
					axis = AXIS_Y;
					manipulatorMode = MOVE_MODE;
				}
			}
			if (m_bNeedZ)
			{
				Cone zCone(Vec3(0.0f, 0.0f, 1.1f * m_size), Vec3(0.0, 0.0f, -1.0f), arrowSize, arrowRadius);

				if (Intersect::Ray_Cone(rayObj, zCone, intPoint))
				{
					axis = AXIS_Z;
					manipulatorMode = MOVE_MODE;
				}
			}
		}
	}

	if (axis != 0)
	{
		if (manipulatorMode == HELPER_MODE_NONE)
		{
			if (m_nModeFlags & MOVE_FLAG)
				manipulatorMode = MOVE_MODE;
			else if (m_nModeFlags & ROTATE_FLAG)
				manipulatorMode = ROTATE_MODE;
			else if (m_nModeFlags & SCALE_FLAG)
				manipulatorMode = SCALE_MODE;
			else if (m_nModeFlags & SELECT_FLAG)
				manipulatorMode = SELECT_MODE;
		}
		hc.axis = axis;
		hc.dist = 0;
		m_highlightMode = manipulatorMode;
	}

	if (pManipulatorMode)
	{
		*pManipulatorMode = manipulatorMode;
	}

	m_highlightAxis = axis;

	return axis != 0;
}

//////////////////////////////////////////////////////////////////////////
float CAxisHelper::GetDistance2D(IDisplayViewport* view, CPoint p, Vec3& wp)
{
	CPoint vp = view->WorldToView(wp);
	return sqrtf(float((p.x - vp.x) * (p.x - vp.x) + (p.y - vp.y) * (p.y - vp.y)));
}

