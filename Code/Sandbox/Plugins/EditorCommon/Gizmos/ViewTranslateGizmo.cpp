// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ViewTranslateGizmo.h"
#include "IDisplayViewport.h"
#include "Grid.h"
#include "Gizmos/AxisHelper.h"

#define HIT_RADIUS (8)

CViewTranslateGizmo::CViewTranslateGizmo()
	: m_color(1.0f, 1.0f, 1.0f)
	, m_scale(1.0f)
{
}

CViewTranslateGizmo::~CViewTranslateGizmo()
{
}

void CViewTranslateGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CViewTranslateGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CViewTranslateGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CViewTranslateGizmo::Display(DisplayContext& dc)
{
	IDisplayViewport* view = dc.view;
	Vec3 position;

	if (GetFlag(EGIZMO_INTERACTING))
	{
		position = m_initPosition + m_interactionOffset;
	}
	else
	{
		position = m_position;
	}

	// this could probably go in a "prepare" step, but for now keep it here
	float scale = view->GetScreenScaleFactor(position) * gGizmoPreferences.axisGizmoSize * m_scale;

	if (GetFlag(EGIZMO_HIGHLIGHTED))
	{
		dc.SetColor(Vec3(1.0f, 1.0f, 1.0f));
	}
	else
	{
		dc.SetColor(m_color);
	}

	// construct a view aligned matrix
	uint32 curstate = dc.GetState();
	dc.DepthTestOff();
	dc.DrawBall(position, scale);
	dc.SetState(curstate);
}

bool CViewTranslateGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				Vec3 raySrc, rayDir;
				Vec3 vDir = view->ViewDirection();

				view->ViewToWorldRay(point, raySrc, rayDir);

				m_interactionOffset = raySrc + ((m_initPosition - raySrc) * vDir) / (vDir * rayDir) * rayDir - m_initPosition - m_initOffset;
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
			// save initial values to display shadow
			Vec3 raySrc, rayDir;
			Vec3 vDir = view->ViewDirection();
			view->ViewToWorldRay(point, raySrc, rayDir);

			m_initOffset = raySrc + ((m_position - raySrc) * vDir) / (vDir * rayDir) * rayDir - m_position;
			m_initPosition = m_position;
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

void CViewTranslateGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CViewTranslateGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

	// Ray in world space.
	hc.view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	// find nearest point to position, check if distance to radius smaller than threshold
	Vec3 dist = hc.raySrc + ((m_position - hc.raySrc) * hc.rayDir) * hc.rayDir - m_position;

	if (dist.len() < scale)
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

