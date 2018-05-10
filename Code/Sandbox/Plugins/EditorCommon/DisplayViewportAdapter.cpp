// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DisplayViewportAdapter.h"
#include "QViewport.h"
#include <CryMath/Cry_Camera.h>
#include "Util/Math.h"

CDisplayViewportAdapter::CDisplayViewportAdapter(QViewport* viewport) : m_viewport(viewport), m_bXYViewport(false)
{
}

float CDisplayViewportAdapter::GetScreenScaleFactor(const Vec3& position) const
{
	float dist = m_viewport->Camera()->GetPosition().GetDistance(position);
	if (dist < m_viewport->Camera()->GetNearPlane())
		dist = m_viewport->Camera()->GetNearPlane();
	return dist;
}

bool CDisplayViewportAdapter::HitTestLine(const Vec3& lineP1, const Vec3& lineP2, const CPoint& hitpoint, int pixelRadius, float* pToCameraDistance) const
{
	CPoint p1 = WorldToView(lineP1);
	CPoint p2 = WorldToView(lineP2);

	float dist = PointToLineDistance2D(Vec3(float(p1.x), float(p1.y), 0), Vec3(float(p2.x), float(p2.y), 0), Vec3(float(hitpoint.x), float(hitpoint.y), 0));
	if (dist <= pixelRadius)
	{
		if (pToCameraDistance)
		{
			Vec3 raySrc, rayDir;
			ViewToWorldRay(hitpoint, raySrc, rayDir);
			Vec3 rayTrg = raySrc + rayDir * 10000.0f;

			Vec3 pa, pb;
			float mua, mub;
			LineLineIntersect(lineP1, lineP2, raySrc, rayTrg, pa, pb, mua, mub);
			*pToCameraDistance = mub;
		}

		return true;
	}

	return false;
}

void CDisplayViewportAdapter::GetPerpendicularAxis(EAxis* axis, bool* is2D) const
{
	if (m_bXYViewport)
	{
		*axis = AXIS_Z;
		*is2D = false;
	}
	else
	{
		*axis = AXIS_NONE;
		*is2D = false;
	}
}

const Matrix34& CDisplayViewportAdapter::GetViewTM() const
{
	m_viewMatrix = m_viewport->Camera()->GetViewMatrix();
	return m_viewMatrix;
}

POINT CDisplayViewportAdapter::WorldToView(const Vec3& worldPoint) const
{
	Vec3 out(0,0,0);
	m_viewport->Camera()->Project(worldPoint,out);
	POINT result;
	result.x = (int)out.x;
	result.y = (int)out.y;
	return result;
}

void CDisplayViewportAdapter::ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const
{
	Ray ray;
	// this can fail for number of reasons
	if (!m_viewport->ScreenToWorldRay(&ray, vp.x, vp.y))
	{
		// return some "safe" default that will not cause FPE
		raySrc = m_viewport->Camera()->GetPosition();
		rayDir = m_viewport->Camera()->GetViewdir();

		// the interface should be changed to accommodate for error
		return;
	}
	raySrc = ray.origin;
	rayDir = ray.direction;
}

Vec3 CDisplayViewportAdapter::ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const
{
	Vec3 raySrc, rayDir;
	Vec3 cameraPos = m_viewport->Camera()->GetPosition();

	ViewToWorldRay(point, raySrc, rayDir);

	// find plane between camera and initial position and direction
	Vec3 cameraToOrigin = cameraPos - origin;
	Vec3 lineViewPlane = cameraToOrigin ^ axis;
	lineViewPlane.Normalize();

	// Now we project the ray from origin to the source point to the screen space line plane
	Vec3 cameraToSrc = raySrc - cameraPos;
	cameraToSrc.Normalize();

	Vec3 perpPlane = (cameraToSrc ^ lineViewPlane);

	// finally, project along the axis to perpPlane
	return ((perpPlane * cameraToOrigin) / (perpPlane * axis)) * axis;
}

Vec3 CDisplayViewportAdapter::ViewDirection() const
{
	return m_viewport->Camera()->GetViewdir();
}

Vec3 CDisplayViewportAdapter::UpViewDirection() const
{
	return m_viewport->Camera()->GetUp();
}


Vec3 CDisplayViewportAdapter::CameraToWorld(Vec3 worldPoint) const
{
	return worldPoint - m_viewport->Camera()->GetPosition();
}

float CDisplayViewportAdapter::GetAspectRatio() const
{
	int w, h;
	GetDimensions(&w, &h);
	if (h != 0)
		return float(w) / h;
	else
		return 1.0f;
}

void CDisplayViewportAdapter::SetCursor(QCursor* hCursor)
{
	if (hCursor)
		m_viewport->setCursor(*hCursor);
	else
		m_viewport->setCursor(Qt::ArrowCursor);
}

void CDisplayViewportAdapter::GetDimensions(int* width, int* height) const
{
	if (width)
		*width = m_viewport->Width();
	if (height)
		*height = m_viewport->Height();
}

