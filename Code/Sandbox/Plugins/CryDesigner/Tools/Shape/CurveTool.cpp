// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CurveTool.h"
#include "DesignerEditor.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "ViewManager.h"
#include <CrySerialization/Enum.h>
#include "Objects/DisplayContext.h"

namespace Designer
{
void CurveTool::Leave()
{
	ResetAllSpots();
	m_ArcState = eArcState_ChooseFirstPoint;
	__super::Leave();
}

void CurveTool::Serialize(Serialization::IArchive& ar)
{
	__super::Serialize(ar);
	m_CurveParameter.Serialize(ar);
}

bool CurveTool::IsPhaseFirstStepOnPrimitiveCreation() const
{
	return GetSpotListCount() == 0 && m_ArcState == eArcState_ChooseFirstPoint;
}

bool CurveTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	if (m_ArcState == eArcState_ChooseFirstPoint)
	{
		if (GetModel()->IsEmpty() && GetSpotListCount() == 0)
		{
			ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, false);
			Spot spot = GetCurrentSpot();
			spot.m_Plane.Set(spot.m_Plane.Normal(), -spot.m_Plane.Normal().Dot(spot.m_Pos));
			SetPlane(spot.m_Plane);
			SetCurrentSpot(spot);
		}

		SetStartSpot(GetCurrentSpot());
		DESIGNER_ASSERT(GetStartSpot().m_PosState != eSpotPosState_Invalid);
		m_ArcState = eArcState_ChooseLastPoint;
		ResetCurrentSpot();
	}
	else if (m_ArcState == eArcState_ChooseLastPoint)
	{
		m_LastSpot = GetCurrentSpot();
		ResetCurrentSpot();
		m_ArcState = eArcState_ControlMiddlePoint;
	}
	else if (m_ArcState == eArcState_ControlMiddlePoint)
	{
		CUndo undo("Designer : Register Arc");
		GetModel()->RecordUndo("Designer:Arc", GetBaseObject());

		SpotList spotList;
		int iSpotListSize(GetSpotListCount());
		for (int i = 0; i < iSpotListSize - 1; ++i)
		{
			const Spot& spot0 = GetSpot(i);
			const Spot& spot1 = GetSpot(i + 1);
			std::vector<IntersectionPairBetweenTwoEdges> intersections;
			GetModel()->QueryIntersectionByEdge(BrushEdge3D(spot0.m_Pos, spot1.m_Pos), intersections);
			spotList.push_back(spot0);
			for (int k = 0, iSize(intersections.size()); k < iSize; ++k)
			{
				if (Comparison::IsEquivalent(intersections[k].second, spot0.m_Pos) || Comparison::IsEquivalent(intersections[k].second, spot1.m_Pos))
					continue;
				spotList.push_back(Spot(intersections[k].second, eSpotPosState_Edge, intersections[k].first));
			}
		}

		if (iSpotListSize - 1 >= 0 && iSpotListSize - 1 < GetSpotList().size())
			spotList.push_back(GetSpot(iSpotListSize - 1));

		ReplaceSpotList(spotList);
		RegisterSpotListAfterBreaking();
		RegisterEitherEndSpotList();
		MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), GetPlane());
		ResetAllSpots();
		ApplyPostProcess(ePostProcess_ExceptMirror);
		m_ArcState = eArcState_ChooseFirstPoint;
	}

	return true;
}

bool CurveTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_ArcState == eArcState_ControlMiddlePoint)
	{
		PrepareArcSpots(view, nFlags, point);
	}
	else
	{
		bool bKeepInitialPlane = (m_ArcState != eArcState_ChooseFirstPoint);
		if (!ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, bKeepInitialPlane))
			return true;

		SetPlane(GetCurrentSpot().m_Plane);

		if (m_ArcState != eArcState_ChooseLastPoint)
			return true;

		BrushVec3 outPos;
		m_LineState = GetAlienedPointWithAxis(GetStartSpotPos(), GetCurrentSpotPos(), GetPlane(), view, NULL, outPos);
		SetCurrentSpotPos(outPos);
	}

	return true;
}

void CurveTool::PrepareArcSpots(CViewport* view, UINT nFlags, CPoint point)
{
	if (!ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, true))
		return;

	BrushVec3 crossPoint = GetCurrentSpotPos();
	BrushVec2 vCrossPointOnPlane(GetPlane().W2P(crossPoint));
	BrushVec2 vFirstPointOnPlane(GetPlane().W2P(GetStartSpotPos()));
	BrushVec2 vLastPointOnPlane(GetPlane().W2P(m_LastSpot.m_Pos));

	int nEdgeCount = m_CurveParameter.m_NumOfSubdivision;
	std::vector<BrushVec2> arcLastVertexList;
	if (MakeListConsistingOfArc(vCrossPointOnPlane, vFirstPointOnPlane, vLastPointOnPlane, nEdgeCount, arcLastVertexList) && !arcLastVertexList.empty())
	{
		ClearSpotList();
		AddSpotToSpotList(m_LastSpot);
		for (int i = 0; i < nEdgeCount - 1; ++i)
			AddSpotToSpotList(Spot(GetPlane().P2W(arcLastVertexList[i]), eSpotPosState_InPolygon, GetPlane()));
		AddSpotToSpotList(GetStartSpot());
	}
}

void CurveTool::Display(DisplayContext& dc)
{
	int oldThickness = dc.GetLineWidth();

	if (m_ArcState == eArcState_ChooseFirstPoint || m_ArcState == eArcState_ChooseLastPoint)
	{
		dc.SetFillMode(e_FillModeSolid);
		DrawCurrentSpot(dc, GetWorldTM());
	}

	if (m_ArcState == eArcState_ControlMiddlePoint)
	{
		if (GetSpotListCount())
		{
			dc.SetColor(PolygonLineColor);
			DrawPolyline(dc);
		}
	}
	else if (m_ArcState == eArcState_ChooseLastPoint)
	{
		if (m_LineState == eLineState_ParallelToAxis)
			dc.SetColor(PolygonParallelToAxis);
		else
			dc.SetColor(PolygonLineColor);
		dc.DrawLine(GetStartSpotPos(), GetCurrentSpotPos());
	}

	dc.SetLineWidth(oldThickness);
}

bool CurveTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		return GetDesigner()->EndCurrentDesignerTool();

	return true;
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Curve, eToolGroup_Shape, "Curve", CurveTool,
                                                           curve, "runs curve tool", "designer.curve")

