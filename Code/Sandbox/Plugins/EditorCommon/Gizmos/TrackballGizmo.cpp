// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TrackballGizmo.h"
#include "IDisplayViewport.h"
#include "Grid.h"
#include "Gizmos/AxisHelper.h"

#define HIT_RADIUS (8)

CTrackballGizmo::CTrackballGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
{
}

CTrackballGizmo::~CTrackballGizmo()
{
}

void CTrackballGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CTrackballGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CTrackballGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CTrackballGizmo::Display(DisplayContext& dc)
{
	if (!GetFlag(EGIZMO_INTERACTING) && GetFlag(EGIZMO_HIGHLIGHTED))
	{
		IDisplayViewport* view = dc.view;

		// this could probably go in a "prepare" step, but for now keep it here
		float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

		dc.SetColor(m_color);
		dc.SetAlpha(0.2f);
		// construct a view aligned matrix
		uint32 curstate = dc.GetState();
		dc.DepthTestOff();
		dc.DrawBall(m_position, scale);
		dc.SetState(curstate);
	}
}

bool CTrackballGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				Vec3 raySrc, rayDir;
				Vec3 zDir = view->CameraToWorld(m_initPosition);
				zDir.Normalize();
				Vec3 yDir = view->UpViewDirection();
				Vec3 xDir = yDir ^ zDir;
				// redo the yDirection to align it to the plane
				yDir = zDir ^ xDir;

				// don't use the gizmo scale for interaction to keep the precision higher
				float scale = view->GetScreenScaleFactor(m_initPosition) * gGizmoPreferences.axisGizmoSize;

				view->ViewToWorldRay(point, raySrc, rayDir);

				Vec3 offset = raySrc + (((m_initPosition - scale * zDir) - raySrc) * zDir) / (zDir * rayDir) * rayDir - m_initPosition;

				float xradians = (offset * xDir - m_initOffset.x) / scale;
				float yradians = (offset * yDir - m_initOffset.y) / scale;

				if (gSnappingPreferences.angleSnappingEnabled())
				{
					float xangle = RAD2DEG(xradians);
					xangle = gSnappingPreferences.SnapAngle(xangle);
					xradians = DEG2RAD(xangle);

					float yangle = RAD2DEG(yradians);
					yangle = gSnappingPreferences.SnapAngle(yangle);
					yradians = DEG2RAD(yangle);
				}

				Quat xrot = Quat::CreateRotationAA(-xradians, yDir);
				Quat yrot = Quat::CreateRotationAA(yradians, xDir);

				// arbitrary ordering, apply xrotation first, then yrotation
				Quat finalQuat = yrot * xrot;
				AngleAxis finalAngle(finalQuat);

				signalDragging(view, this, finalAngle, point, nFlags);
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
			Vec3 zDir = view->CameraToWorld(m_position);
			zDir.Normalize();
			Vec3 yDir = view->UpViewDirection();
			Vec3 xDir = yDir ^ zDir;
			// redo the yDirection to align it to the plane
			yDir = zDir ^ xDir;

			float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

			view->ViewToWorldRay(point, raySrc, rayDir);

			Vec3 offset = raySrc + (((m_position - scale * zDir) - raySrc) * zDir) / (zDir * rayDir) * rayDir - m_position;
			// offset is in world space, transform it to the local plane space
			m_initOffset.x = offset * xDir;
			m_initOffset.y = offset * yDir;

			m_initPosition = m_position;

			signalBeginDrag(view, this, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CTrackballGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CTrackballGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

	// Ray in world space.
	hc.view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	// find nearest point to position, check if distance to radius smaller than threshold
	Vec3 dist = hc.raySrc + ((m_position - hc.raySrc) * hc.rayDir) * hc.rayDir - m_position;

	if (dist.GetLengthSquared() < scale * scale)
	{
		hc.dist = 0;
		return true;
	}

	return false;
}

