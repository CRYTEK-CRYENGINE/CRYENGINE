// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AxisScaleGizmo.h"
#include "IDisplayViewport.h"
#include "Gizmos/AxisHelper.h"
#include "Grid.h"

#define HIT_RADIUS (8)

CAxisScaleGizmo::CAxisScaleGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
	, m_offset(0.0f)
{
}

CAxisScaleGizmo::~CAxisScaleGizmo()
{
}

void CAxisScaleGizmo::SetName(const char* sName)
{
	m_name = sName;
}

const char* CAxisScaleGizmo::GetName()
{
	return m_name.c_str();
}

void CAxisScaleGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CAxisScaleGizmo::SetDirection(Vec3 dir)
{
	m_direction = dir;
	m_direction.Normalize();
}

void CAxisScaleGizmo::SetUpAxis(Vec3 dir)
{
	m_upAxis = dir;
	m_upAxis.Normalize();
}

void CAxisScaleGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CAxisScaleGizmo::SetOffset(float offset)
{
	m_offset = offset;
}

void CAxisScaleGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CAxisScaleGizmo::DrawSquishBox(DisplayContext& dc, Vec3 position)
{
	IDisplayViewport* view = dc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;
	float boxScale;

	if (GetFlag(EGIZMO_INTERACTING))
	{
		scaleCustom *= m_scaleInteraction;
		boxScale = view->GetScreenScaleFactor(position + scaleCustom * m_direction) * gGizmoPreferences.axisGizmoSize * m_scale * 0.08;
	}
	else
	{
		boxScale = scaleCustom * 0.08;
	}

	Vec3 start(scale * m_offset, 0.0f, 0.0f);
	Vec3 tip(scaleCustom, 0.0f, 0.0f);

	Vec3 yaxis = m_direction ^ m_upAxis;

	// create a translation matrix and draw arrows with it
	Matrix34 tm = Matrix34::CreateFromVectors(m_direction, m_upAxis, yaxis, position);

	dc.PushMatrix(tm);

	float lineWidth = dc.GetLineWidth();
	dc.SetLineWidth(4.0f);
	dc.DrawLine(start, tip);
	dc.SetLineWidth(lineWidth);

	Vec3 boxDim(boxScale, boxScale, boxScale);
	dc.DrawSolidBox(tip - boxDim, tip + boxDim);

	dc.PopMatrix();
}

void CAxisScaleGizmo::Display(DisplayContext& dc)
{
	uint32 curflags = dc.GetState();
	dc.DepthTestOff();

	if (GetFlag(EGIZMO_INTERACTING))
	{
		float textSize = 1.4f;

		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		string msg;
		msg.Format("Scale %.1f", m_scaleInteraction);

		dc.DrawTextLabel(ConvertToTextPos(m_position, Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), textSize, (LPCSTR)msg, true);
	}

	// draw a shadow arrow at the initial position
	if (GetFlag(EGIZMO_HIGHLIGHTED) && !GetFlag(EGIZMO_INTERACTING))
	{
		dc.SetColor(Vec3(1.0f, 1.0f, 1.0f));
	}
	else
	{
		dc.SetColor(m_color);
	}

	DrawSquishBox(dc, m_position);

	dc.SetState(curflags);
}

bool CAxisScaleGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				// calculate the position starting from the original offset
				Vec3 offset = view->ViewToAxisConstraint(point, m_direction, m_position + m_origLenInteraction * m_direction);
				float l = offset.len();
				l += m_origScale;

				if (offset * m_direction < 0.0f)
				{
					m_scaleInteraction = m_origScale / max(l, 0.0f);
				}
				else
				{
					m_scaleInteraction = max(l, 0.0f) / m_origScale;
				}

				if (gSnappingPreferences.scaleSnappingEnabled())
				{
					m_scaleInteraction = gSnappingPreferences.SnapScale(m_scaleInteraction);
				}

				signalDragging(view, this, m_scaleInteraction, point, nFlags);
				break;
			}

		case eMouseLUp:
			UnsetFlag(EGIZMO_INTERACTING);
			signalEndDrag(view, this, point, nFlags);
			break;
		}
		return true;
	}
	else
	{
		if (event == eMouseLDown)
		{
			// this could probably go in a "prepare" step, but for now keep it here
			SetFlag(EGIZMO_INTERACTING);
			Vec3 offset = view->ViewToAxisConstraint(point, m_direction, m_position);
			m_origScale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;
			m_origLenInteraction = offset.len();
			m_scaleInteraction = 1.0f;
			signalBeginDrag(view, this, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CAxisScaleGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CAxisScaleGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;
	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize;
	float scaleCustom = scale * m_scale;
	float boxScale = scaleCustom * 0.08f;

	Vec3 start = scale * m_offset * m_direction + m_position;
	Vec3 tip = scaleCustom * m_direction + m_position;

	if (view->HitTestLine(start, tip, hc.point2d, HIT_RADIUS))
	{
		hc.dist = 0;
		return true;
	}

	Vec3 intPoint;

	// Ray in world space.
	hc.view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	Vec3 yaxis = m_direction ^ m_upAxis;
	Matrix33 m = Matrix33::CreateFromVectors(m_direction, m_upAxis, yaxis);
	m.Transpose();

	Vec3 raySrc = m * (hc.raySrc - m_position);
	Vec3 rayDir = m * hc.rayDir;

	Ray ray(raySrc, rayDir);

	AABB box(Vec3(scaleCustom, 0.0f, 0.0f), boxScale);

	if (Intersect::Ray_AABB(ray, box, intPoint))
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

