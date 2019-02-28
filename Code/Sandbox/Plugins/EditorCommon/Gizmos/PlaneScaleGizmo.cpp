// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlaneScaleGizmo.h"
#include "IDisplayViewport.h"
#include "Gizmos/AxisHelper.h"
#include "Grid.h"

CPlaneScaleGizmo::CPlaneScaleGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
	, m_offsetInner(0.0f)
	, m_offsetFromCenter(0.0f)
{
}

CPlaneScaleGizmo::~CPlaneScaleGizmo()
{
}

void CPlaneScaleGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CPlaneScaleGizmo::SetXVector(Vec3 dir)
{
	m_xVector = dir;
	m_xVector.Normalize();
}

void CPlaneScaleGizmo::SetYVector(Vec3 dir)
{
	m_yVector = dir;
	m_yVector.Normalize();
}

void CPlaneScaleGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CPlaneScaleGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CPlaneScaleGizmo::SetOffsetInner(float offset)
{
	m_offsetInner = offset;
}

void CPlaneScaleGizmo::SetOffsetFromCenter(float offset)
{
	m_offsetFromCenter = offset;
}

void CPlaneScaleGizmo::DrawPlane(DisplayContext& dc, Vec3 position)
{
	IDisplayViewport* view = dc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;

	if (GetFlag(EGIZMO_INTERACTING))
	{
		scaleCustom *= m_interactionScale;
	}

	Vec3 start = position + (m_xVector + m_yVector) * m_offsetFromCenter * scale;

	Vec3 startx = start + m_xVector * m_offsetInner * scaleCustom;
	Vec3 starty = start + m_yVector * m_offsetInner * scaleCustom;

	Vec3 cornerx = start + m_xVector * scaleCustom;
	Vec3 cornery = start + m_yVector * scaleCustom;

	uint32 prevflags = dc.GetState();

	dc.DepthTestOff();

	dc.DrawLine(startx, cornerx);
	dc.DrawLine(starty, cornery);
	dc.DrawLine(cornery, cornerx);
	dc.DrawLine(startx, starty);

	// disable culling before drawing the quad so it draws always
	dc.SetAlpha(0.5f);
	dc.CullOff();
	dc.DrawQuad(startx, starty, cornery, cornerx);

	dc.SetState(prevflags);
}

void CPlaneScaleGizmo::Display(DisplayContext& dc)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		uint32 prevflags = dc.GetState();

		dc.DepthTestOff();
		dc.SetColor(m_color);
		dc.DrawLine(m_position - 10000.0f * m_xVector, m_position + 10000.0f * m_xVector);
		dc.DrawLine(m_position - 10000.0f * m_yVector, m_position + 10000.0f * m_yVector);

		dc.SetState(prevflags);

		float textSize = 1.4f;
		string msg;
		msg.Format("Scale %.1f", m_interactionScale);
		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		dc.DrawTextLabel(ConvertToTextPos(m_position, Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), textSize, (LPCSTR)msg, true);
	}

	if (GetFlag(EGIZMO_HIGHLIGHTED) && !GetFlag(EGIZMO_INTERACTING))
	{
		dc.SetColor(Vec3(1.0f, 1.0f, 1.0f));
	}
	else
	{
		dc.SetColor(m_color);
	}

	DrawPlane(dc, m_position);
}

bool CPlaneScaleGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				Vec3 direction = m_xVector + m_yVector;
				direction.Normalize();
				Vec3 offset = view->ViewToAxisConstraint(point, direction, m_position + m_initLen * direction);

				if (offset * direction < 0.0f)
				{
					m_interactionScale = m_initLen / (offset.len() + m_initLen);
				}
				else
				{
					m_interactionScale = (offset.len() + m_initLen) / m_initLen;
				}

				if (gSnappingPreferences.scaleSnappingEnabled())
				{
					m_interactionScale = gSnappingPreferences.SnapScale(m_interactionScale);
				}

				signalDragging(view, this, m_interactionScale, point, nFlags);
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
			m_initLen = view->ViewToAxisConstraint(point, m_xVector + m_yVector, m_position).len();
			m_interactionScale = 1.0f;
			signalBeginDrag(view, this, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CPlaneScaleGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CPlaneScaleGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;

	// find start vector in world space
	Vec3 start = (m_xVector + m_yVector) * m_offsetFromCenter * scale + m_position;

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

	if (fac1 > 0.0f && fac2 > 0.0f && (fac1 + fac2) < scaleCustom && (fac1 + fac2) > (scaleCustom * m_offsetInner))
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

