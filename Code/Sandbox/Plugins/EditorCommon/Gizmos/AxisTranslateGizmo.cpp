// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AxisTranslateGizmo.h"
#include "IDisplayViewport.h"
#include "Grid.h"
#include "Objects/DisplayContext.h"
#include "Gizmos/AxisHelper.h"

#define HIT_RADIUS (8)

CAxisTranslateGizmo::CAxisTranslateGizmo(bool bDrawArrowTip/* = true*/)
	: m_color(1.0f, 1.0f, 1.0f)
	, m_scale(1.0f)
	, m_offset(0.0f)
	, m_bDrawArrowTip(bDrawArrowTip)
{
}

CAxisTranslateGizmo::~CAxisTranslateGizmo()
{
}

void CAxisTranslateGizmo::SetName(const char* sName)
{
	m_name = sName;
}

const char* CAxisTranslateGizmo::GetName()
{
	return m_name.c_str();
}

void CAxisTranslateGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CAxisTranslateGizmo::SetDirection(Vec3 dir)
{
	m_direction = dir;
	m_direction.Normalize();
}

void CAxisTranslateGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CAxisTranslateGizmo::SetOffset(float offset)
{
	m_offset = offset;
}

void CAxisTranslateGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CAxisTranslateGizmo::DrawArrow(DisplayContext& dc, Vec3 position, Vec3 direction)
{
	IDisplayViewport* view = dc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;

	// create a translation matrix and draw arrows with it
	Matrix34 tm = Matrix34::CreateTranslationMat(position);
	Vec3 start = scale * m_offset * direction;
	Vec3 tip = scaleCustom * direction;

	dc.PushMatrix(tm);

	float lineWidth = dc.GetLineWidth();
	dc.SetLineWidth(4.0f);
	dc.DrawLine(start, tip);
	dc.SetLineWidth(lineWidth);

	if (m_bDrawArrowTip)
		dc.DrawArrow(tip, 1.1f * tip, scaleCustom * 0.75f);

	dc.PopMatrix();
}

void CAxisTranslateGizmo::Display(DisplayContext& dc)
{
	uint32 curflags = dc.GetState();
	dc.DepthTestOff();

	if (GetFlag(EGIZMO_INTERACTING))
	{
		IDisplayViewport* view = dc.view;

		// this could probably go in a "prepare" step, but for now keep it here
		float scale = view->GetScreenScaleFactor(m_initPosition) * gGizmoPreferences.axisGizmoSize;
		float textSize = 1.4f;

		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		dc.DrawDottedLine(m_initPosition + m_offset * m_initDirection, m_initPosition + m_interactionOffset, 0.2f * scale);

		float translationLen = m_interactionOffset.len();

		string msg;
		msg.Format("%.2f units", translationLen);

		dc.DrawTextLabel(ConvertToTextPos(m_position, Matrix34::CreateIdentity(), view, dc.flags & DISPLAY_2D), textSize, (LPCSTR)msg, true);
	}

	// draw a shadow arrow at the initial position
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
		DrawArrow(dc, m_initPosition + m_interactionOffset, m_initDirection);
	}
	else
	{
		DrawArrow(dc, m_position, m_direction);
	}

	dc.SetState(curflags);
}

bool CAxisTranslateGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{

	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				m_interactionOffset = view->ViewToAxisConstraint(point, m_initDirection, m_initPosition) - m_initOffset;
				if (gSnappingPreferences.gridSnappingEnabled())
				{
					float fDist = m_interactionOffset.len();
					m_interactionOffset = gSnappingPreferences.SnapLength(fDist) * m_interactionOffset.GetNormalized();
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

			// save initial values to display shadow
			m_initOffset = view->ViewToAxisConstraint(point, m_direction, m_position);
			m_initPosition = m_position;
			m_initDirection = m_direction;
			m_interactionOffset = Vec3(0.0f, 0.0f, 0.0f);
			signalBeginDrag(view, this, m_initPosition, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CAxisTranslateGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CAxisTranslateGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;

	Vec3 start = scale * m_offset * m_direction + m_position;
	Vec3 tip = scaleCustom * m_direction + m_position;

	if (view->HitTestLine(start, tip, hc.point2d, HIT_RADIUS))
	{
		hc.dist = 0;
		return true;
	}

	Vec3 intPoint;

	// Ray in world space.
	view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	// magic numbers here try to mimic sizing logic around and inside DrawArrow. Needs better implementation to get rid of this
	float arrowSize = scaleCustom * 0.4f * 0.75;
	float arrowRadius = scaleCustom * 0.1f * 0.75;

	Ray ray(hc.raySrc, hc.rayDir);

	Cone xCone(1.1f * tip - 0.1f * m_position, -m_direction, arrowSize, arrowRadius);
	if (Intersect::Ray_Cone(ray, xCone, intPoint))
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

