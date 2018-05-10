// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlaneTranslateGizmo.h"
#include "IDisplayViewport.h"
#include "Grid.h"
#include "Gizmos/AxisHelper.h"

CPlaneTranslateGizmo::CPlaneTranslateGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
	, m_xOffset(0.0f)
	, m_yOffset(0.0f)
{
}

CPlaneTranslateGizmo::~CPlaneTranslateGizmo()
{
}

void CPlaneTranslateGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CPlaneTranslateGizmo::SetXVector(Vec3 dir)
{
	m_xVector = dir;
	m_xVector.Normalize();
}

void CPlaneTranslateGizmo::SetYVector(Vec3 dir)
{
	m_yVector = dir;
	m_yVector.Normalize();
}

void CPlaneTranslateGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CPlaneTranslateGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CPlaneTranslateGizmo::SetXOffset(float offset)
{
	m_xOffset = offset;
}

void CPlaneTranslateGizmo::SetYOffset(float offset)
{
	m_yOffset = offset;
}

void CPlaneTranslateGizmo::DrawPlane(DisplayContext& dc, Vec3 position)
{
	IDisplayViewport* view = dc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;

	Vec3 start = position + (m_xVector * m_xOffset + m_yVector * m_yOffset) * scale;

	Vec3 dx = m_xVector * scaleCustom;
	Vec3 dy = m_yVector * scaleCustom;

	Vec3 cornerx = start + dx;
	Vec3 cornery = start + dy;

	// either this or cornery + dx will do
	Vec3 end = cornerx + dy;

	uint32 prevflags = dc.GetState();

	dc.DepthTestOff();

	dc.DrawLine(start, cornerx);
	dc.DrawLine(start, cornery);
	dc.DrawLine(end, cornerx);
	dc.DrawLine(end, cornery);

	// disable culling before drawing the quad so it draws always
	dc.SetAlpha(0.5f);
	dc.CullOff();
	dc.DrawQuad(start, cornerx, end, cornery);

	dc.SetState(prevflags);
}

void CPlaneTranslateGizmo::Display(DisplayContext& dc)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		dc.SetColor(m_color);
		dc.DrawLine(m_initPosition - 10000.0f * m_xVector, m_initPosition + 10000.0f * m_xVector);
		dc.DrawLine(m_initPosition - 10000.0f * m_yVector, m_initPosition + 10000.0f * m_yVector);
	}

	if (GetFlag(EGIZMO_HIGHLIGHTED))
	{
		dc.SetColor(Vec3(1.0f, 1.0f, 1.0f));
	}
	else
	{
		dc.SetColor(m_color);
	}
	if (GetFlag(EGIZMO_INTERACTING))
	{
		DrawPlane(dc, m_initPosition + m_interactionOffset);
	}
	else
	{
		DrawPlane(dc, m_position);
	}
}

bool CPlaneTranslateGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				Vec3 raySrc, rayDir;
				view->ViewToWorldRay(point, raySrc, rayDir);

				// intersect with the plane
				Vec3 planeVector = m_xVector ^ m_yVector;

				// factor along ray vector to intersect with plane
				float fac = (m_initPosition - raySrc) * planeVector / (rayDir * planeVector);

				// find the world space vector on the plane
				m_interactionOffset = raySrc + fac * rayDir - m_initOffset;
				if (gSnappingPreferences.gridSnappingEnabled())
				{
					m_interactionOffset = gSnappingPreferences.Snap(m_interactionOffset);
				}
				signalDragging(view, this, m_interactionOffset, point, nFlags);
				break;
			}

		case eMouseLUp:
			signalEndDrag(view, this, point, nFlags);
			UnsetFlag(EGIZMO_INTERACTING);
			break;
		}
		return true;
	}
	else
	{
		if (event == eMouseLDown)
		{
			SetFlag(EGIZMO_INTERACTING);

			// save initial values
			m_initPosition = m_position;
			Vec3 raySrc, rayDir;
			view->ViewToWorldRay(point, raySrc, rayDir);

			// intersect with the plane
			Vec3 planeVector = m_xVector ^ m_yVector;

			// factor along ray vector to intersect with plane
			float fac = (m_initPosition - raySrc) * planeVector / (rayDir * planeVector);

			// find the world space vector on the plane
			m_initOffset = raySrc + fac * rayDir;
			m_interactionOffset = Vec3(0.0f, 0.0f, 0.0f);
			signalBeginDrag(view, this, m_position, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CPlaneTranslateGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CPlaneTranslateGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;

	// find start vector in world space
	Vec3 start = (m_xVector * m_xOffset + m_yVector * m_yOffset) * scale + m_position;

	// Ray in world space.
	hc.view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	// cross product gives us the plane vector of the plane. We should check if it's zero, but let's trust the user for now
	Vec3 planeVector = m_xVector ^ m_yVector;

	// factor along ray vector to intersect with plane
	float fac = (start - hc.raySrc) * planeVector / (hc.rayDir * planeVector);

	// find the world space vector on the plane
	Vec3 planePosition = hc.raySrc + fac * hc.rayDir;

	// find the difference from the start of the plane
	planePosition -= start;

	// now check if barycentric coordinates of vector in the plane are within size limits
	// remember, x/y vectors are normalized
	float fac1 = m_xVector * planePosition;
	float fac2 = m_yVector * planePosition;

	if (fac1 > 0.0f && fac2 > 0.0f && fac1 < scaleCustom && fac2 < scaleCustom)
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

