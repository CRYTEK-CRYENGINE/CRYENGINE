// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HeightManipulator.h"
#include "ExtrusionSnappingHelper.h"
#include "Viewport.h"

namespace Designer
{
HeightManipulator s_HeightManipulator;

void HeightManipulator::Init(const BrushPlane& floorPlane, const BrushVec3& vPivot, bool bDisplayPole)
{
	m_bValid = true;
	m_FloorPlane = floorPlane;
	m_vPivot = vPivot;
	m_bDisplayPole = bDisplayPole;
}

BrushFloat HeightManipulator::Update(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray)
{
	if (!m_bValid)
		return 0;

	BrushPlane helperPlane = CalculateHelperPlane(worldTM, view);

	BrushVec3 vHit;
	BrushFloat t;
	if (!helperPlane.HitTest(ray, &t, &vHit) || t < 0)
		return 0;

	m_Offset = SnapGrid(m_FloorPlane.Distance(vHit));
	return m_Offset;
}

BrushPlane HeightManipulator::CalculateHelperPlane(const BrushMatrix34& worldTM, CViewport* view)
{
	BrushMatrix34 invWorldTM = worldTM.GetInverted();
	BrushVec3 vThirdDirs[] = {
		invWorldTM.TransformVector(view->GetViewTM().GetColumn0()),
		invWorldTM.TransformVector(view->GetViewTM().GetColumn2())
	};
	int nThirdIdx = 0;

	BrushFloat fDotNearestZero = (BrushFloat)1.0f;
	for (int i = 0, iCount(sizeof(vThirdDirs) / sizeof(*vThirdDirs)); i < iCount; ++i)
	{
		BrushFloat fDot = m_FloorPlane.Normal().Dot(vThirdDirs[i]);
		if (std::abs(fDot) < fDotNearestZero)
		{
			fDotNearestZero = fDot;
			nThirdIdx = i;
		}
	}

	BrushVec3 v0 = m_vPivot;
	BrushVec3 v1 = m_vPivot + m_FloorPlane.Normal();
	BrushVec3 v2 = m_vPivot + vThirdDirs[nThirdIdx];

	return BrushPlane(v0, v1, v2);
}

bool HeightManipulator::Start(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray)
{
	if (!m_bValid)
		return false;

	m_bStartedManipulation = false;

	BrushVec3 vProjectedPos;
	if (!GetProjectedPosToPole(worldTM, view, ray, vProjectedPos))
		return false;

	m_vPivot = vProjectedPos;
	m_bStartedManipulation = true;

	m_FloorPlane = BrushPlane(m_FloorPlane.Normal(), -m_FloorPlane.Normal().Dot(m_vPivot));

	return true;
}

bool HeightManipulator::GetProjectedPosToPole(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray, BrushVec3& outProjectedPos)
{
	BrushPlane helperPlane = CalculateHelperPlane(worldTM, view);

	BrushVec3 vHit;
	BrushFloat t;
	if (!helperPlane.HitTest(ray, &t, &vHit) || t < 0)
		return false;

	BrushLine3D line(m_vPivot, m_vPivot + m_FloorPlane.Normal());
	BrushVec3 vProjectedPos = m_vPivot + line.GetProjectedVector(vHit);

	bool bClose = AreTwoPositionsClose(vHit, vProjectedPos, worldTM, view, 12.0f);
	outProjectedPos = vProjectedPos;

	return bClose;
}

bool HeightManipulator::IsCursorCloseToPole(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray)
{
	if (!m_bValid)
		return false;

	BrushVec3 projectedPos;
	return GetProjectedPosToPole(worldTM, view, ray, projectedPos);
}

void HeightManipulator::Display(DisplayContext& dc)
{
	if (!m_bValid)
		return;

	if (m_bDisplayPole)
	{
		ColorF poleColor = m_bStartedManipulation || m_bHighlightPole ? ColorF(1.0f, 1.0f, 1.0f) : ColorF(0.2f, 0.2f, 0.2f);

		dc.SetColor(poleColor);
		dc.SetLineWidth(1.0f);
		dc.DrawLine(ToVec3(m_vPivot - m_FloorPlane.Normal() * 10000.0f), ToVec3(m_vPivot + m_FloorPlane.Normal() * 10000.0f));

		if (m_bStartedManipulation)
		{
			Vec3 pos = m_vPivot + m_FloorPlane.Normal() * m_Offset;
			Vec3 size = Vec3(1.0f, 1.0f, 1.0f) * dc.view->GetScreenScaleFactor(dc.GetMatrix().TransformPoint(pos)) * 0.015f;
			dc.DrawWireBox(pos - size, pos + size);
		}
	}
}
}

