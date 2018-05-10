// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OffsetManipulator.h"
#include "Core/Model.h"
#include "Grid.h"
#include "Viewport.h"
#include "DesignerEditor.h"
#include "Util/Display.h"
#include "Core/LoopPolygons.h"
#include "Objects/DisplayContext.h"

namespace Designer
{
OffsetManipulator s_OffsetManipulator;

void OffsetManipulator::Init(PolygonPtr pPolygon, Model* pModel, bool bIncludeHoles)
{
	m_pPolygon = pPolygon;
	m_bValid = false;

	BrushVec3 pickedPos;
	pModel->QueryPosition(m_pPolygon->GetPlane(), GetDesigner()->GetRay(), pickedPos);

	BrushEdge3D nearestEdge;
	BrushVec3 nearestPos;

	std::vector<PolygonPtr> outerPolygons = m_pPolygon->GetLoops()->GetOuterClones();
	if (!outerPolygons.empty() && outerPolygons[0]->QueryNearestEdge(pickedPos, nearestEdge, nearestPos))
	{
		m_Holes = m_pPolygon->GetLoops()->GetHoleClones();
		BrushVec3 edgeDir = (nearestEdge.m_v[1] - nearestEdge.m_v[0]).GetNormalized();
		m_Origin = nearestPos;
		m_Plane = BrushPlane(nearestEdge.m_v[0], nearestEdge.m_v[1], nearestEdge.m_v[0] - m_pPolygon->GetPlane().Normal());
		if (!bIncludeHoles)
			m_pPolygon = outerPolygons[0];
		m_pScaledPolygon = m_pPolygon->Clone();
		m_bValid = true;
	}
}

void OffsetManipulator::UpdateOffset(CViewport* view, CPoint point)
{
	if (!IsValid())
		return;

	BrushVec3 hitPos;
	if (m_pPolygon->GetPlane().HitTest(GetDesigner()->GetRay(), NULL, &hitPos))
	{
		m_pScaledPolygon = m_pPolygon->Clone();
		m_Scale = m_Plane.Distance(hitPos);

		if (gSnappingPreferences.gridSnappingEnabled())
			m_Scale = Snap(m_Scale);

		m_Scale = ApplyScaleCheckingBoundary(m_pScaledPolygon, m_Scale);
		m_HitPos = m_Origin + m_Plane.Normal() * m_Scale;
	}
}

void OffsetManipulator::UpdateOffset(BrushFloat scale)
{
	if (!IsValid())
		return;
	m_pScaledPolygon = m_pPolygon->Clone();
	m_Scale = ApplyScaleCheckingBoundary(m_pScaledPolygon, scale);
}

void OffsetManipulator::Display(DisplayContext& dc, bool bDisplayDistanceLine)
{
	if (!m_bValid)
		return;

	dc.SetFillMode(e_FillModeSolid);

	int oldThickness = dc.GetLineWidth();
	dc.SetLineWidth(kLineThickness);

	dc.SetColor(kResizedPolygonColor);
	Display::DisplayPolygon(dc, m_pScaledPolygon);

	if (bDisplayDistanceLine)
	{
		dc.SetColor(ColorB(160, 160, 160, 255));
		dc.DrawLine(m_Origin, m_HitPos);
	}

	dc.SetLineWidth(oldThickness);
}
}

