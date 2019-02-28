// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UniformScaleGizmo.h"
#include "IDisplayViewport.h"
#include "Gizmos/AxisHelper.h"
#include "Grid.h"

#define HIT_RADIUS (8)

CUniformScaleGizmo::CUniformScaleGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
{
}

CUniformScaleGizmo::~CUniformScaleGizmo()
{
}

void CUniformScaleGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CUniformScaleGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CUniformScaleGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CUniformScaleGizmo::SetXVector(Vec3 dir)
{
	m_xAxis = dir;
	m_xAxis.Normalize();
}

void CUniformScaleGizmo::SetYVector(Vec3 dir)
{
	m_yAxis = dir;
	m_yAxis.Normalize();
}

void CUniformScaleGizmo::Display(DisplayContext& dc)
{
	IDisplayViewport* view = dc.view;
	Vec3 position;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

	if (GetFlag(EGIZMO_INTERACTING))
	{
		scale *= m_interactionScale;

		float textSize = 1.4f;
		string msg;
		msg.Format("Scale %.1f", m_interactionScale);
		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		dc.DrawTextLabel(ConvertToTextPos(m_position, Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), textSize, (LPCSTR)msg, true);
	}

	if (GetFlag(EGIZMO_HIGHLIGHTED))
	{
		dc.SetColor(Vec3(1.0f, 1.0f, 1.0f));
	}
	else
	{
		dc.SetColor(m_color);
	}

	Vec3 zAxis = m_xAxis ^ m_yAxis;
	Matrix34 m = Matrix34::CreateFromVectors(m_xAxis, m_yAxis, zAxis, m_position);

	dc.PushMatrix(m);

	// construct a view aligned matrix
	uint32 curstate = dc.GetState();
	dc.DepthTestOff();
	Vec3 vScale(scale, scale, scale);
	if (GetFlag(EGIZMO_INTERACTING))
	{
		dc.DrawWireBox(-vScale, vScale);
	}
	else
	{
		dc.DrawSolidBox(-vScale, vScale);
	}
	dc.SetState(curstate);

	dc.PopMatrix();
}

bool CUniformScaleGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				float fDiff = (m_initPoint.y - point.y) / 50.0f;
				if (fDiff > 0.0f)
				{
					m_interactionScale = 1.0f + fDiff;
				}
				else
				{
					m_interactionScale = 1.0f / (1.0f - fDiff);
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
			// save initial values to display shadow
			Vec3 raySrc, rayDir;
			Vec3 vDir = view->GetViewTM().GetColumn1();
			view->ViewToWorldRay(point, raySrc, rayDir);

			m_interactionScale = 1.0f;
			m_initPoint = point;
			signalBeginDrag(view, this, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CUniformScaleGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CUniformScaleGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

	Vec3 intPoint;
	view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	Vec3 zAxis = m_xAxis ^ m_yAxis;
	Matrix33 m = Matrix33::CreateFromVectors(m_xAxis, m_yAxis, zAxis);
	m.Transpose();

	Vec3 raySrc = m * (hc.raySrc - m_position);
	Vec3 rayDir = m * hc.rayDir;

	Ray ray(raySrc, rayDir);

	AABB box(scale);

	if (Intersect::Ray_AABB(ray, box, intPoint))
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

