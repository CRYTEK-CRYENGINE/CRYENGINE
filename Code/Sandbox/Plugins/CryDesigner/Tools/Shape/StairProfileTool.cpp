// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StairProfileTool.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Viewport.h"
#include "DesignerEditor.h"
#include "Objects/DisplayContext.h"

namespace Designer
{
void StairProfileTool::Enter()
{
	__super::Enter();
	m_SideStairMode = eSideStairMode_PlaceFirstPoint;
}

void StairProfileTool::Serialize(Serialization::IArchive& ar)
{
	__super::Serialize(ar);
	m_StairProfileParameter.Serialize(ar);
}

void StairProfileTool::Display(DisplayContext& dc)
{
	if (m_SideStairMode == eSideStairMode_PlaceFirstPoint || m_SideStairMode == eSideStairMode_DrawDiagonal)
	{
		DrawCurrentSpot(dc, GetWorldTM());
		if (m_SideStairMode == eSideStairMode_DrawDiagonal)
		{
			dc.SetColor(PolygonLineColor);
			dc.DrawLine(GetCurrentSpotPos(), GetStartSpotPos());
		}
	}
	else if (m_SideStairMode == eSideStairMode_SelectDirection)
	{
		for (int i = 0; i < 2; ++i)
		{
			if (m_nSelectedCandidate == i)
			{
				dc.SetLineWidth(kChosenLineThickness);
				DrawCandidateStair(dc, i, PolygonLineColor);
			}
			else
			{
				dc.SetLineWidth(kLineThickness);
				DrawCandidateStair(dc, i, ColorB(100, 100, 100, 255));
			}
		}
	}
}

void StairProfileTool::DrawCandidateStair(DisplayContext& dc, int nIndex, const ColorB& color)
{
	if (m_CandidateStairs[nIndex].empty())
		return;

	dc.SetColor(color);

	int nSize = m_CandidateStairs[nIndex].size();
	if (nSize == 0)
		return;
	std::vector<Vec3> vList;
	for (int i = 0; i < nSize; ++i)
		vList.push_back(m_CandidateStairs[nIndex][i].m_Pos);
	if (vList.size() >= 2)
		dc.DrawPolyLine(&vList[0], vList.size(), false);
}

bool StairProfileTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		if (m_SideStairMode == eSideStairMode_SelectDirection)
		{
			SetCurrentSpot(GetStartSpot());
			m_SideStairMode = eSideStairMode_DrawDiagonal;
		}
		else if (m_SideStairMode == eSideStairMode_DrawDiagonal)
		{
			m_SideStairMode = eSideStairMode_PlaceFirstPoint;
			ResetAllSpots();
		}
		else if (m_SideStairMode == eSideStairMode_PlaceFirstPoint)
		{
			return GetDesigner()->EndCurrentDesignerTool();
		}
	}

	return true;
}

bool StairProfileTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_SideStairMode == eSideStairMode_PlaceFirstPoint)
	{
		if (ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, false))
		{
			SetPlane(GetCurrentSpot().m_Plane);
			SetStartSpot(GetCurrentSpot());
			m_SideStairMode = eSideStairMode_DrawDiagonal;
		}
	}
	else if (m_SideStairMode == eSideStairMode_DrawDiagonal)
	{
		CreateCandidates();
		m_SideStairMode = eSideStairMode_SelectDirection;
	}
	else if (m_SideStairMode == eSideStairMode_SelectDirection)
	{
		CUndo undo("Designer : Add Stair");
		GetModel()->RecordUndo("Add Stair", GetBaseObject());

		std::vector<Spot>& selectedStairs = m_CandidateStairs[m_nSelectedCandidate];
		int nStairSize = selectedStairs.size();

		DESIGNER_ASSERT(nStairSize >= 3);

		if (nStairSize >= 3)
		{
			if (m_StairProfileParameter.m_bClosedProfile)
			{
				BrushVec2 v0 = GetPlane().W2P(selectedStairs[0].m_Pos);
				BrushVec2 v1 = GetPlane().W2P(selectedStairs[1].m_Pos);
				BrushVec2 v2 = GetPlane().W2P(selectedStairs[2].m_Pos);

				BrushVec2 v3 = GetPlane().W2P(selectedStairs[nStairSize - 3].m_Pos);
				BrushVec2 v4 = GetPlane().W2P(selectedStairs[nStairSize - 2].m_Pos);
				BrushVec2 v5 = GetPlane().W2P(selectedStairs[nStairSize - 1].m_Pos);

				BrushLine l0(v0, v0 + (v1 - v2));
				BrushLine l1(v5, v5 + (v4 - v3));

				BrushVec2 intersection;
				if (l0.Intersect(l1, intersection))
				{
					selectedStairs.push_back(Spot(GetPlane().P2W(intersection)));
					std::vector<BrushVec3> vList;
					GenerateVertexListFromSpotList(selectedStairs, vList);
					PolygonPtr pPolygon = new Polygon(vList, GetPlane(), GetMatID(), &GetTexInfo(), true);
					pPolygon->ModifyOrientation();
					pPolygon->SetSubMatID(DesignerSession::GetInstance()->GetCurrentSubMatID());
					GetModel()->AddSplitPolygon(pPolygon);
				}
			}
			else
			{
				RegisterSpotList(GetModel(), selectedStairs);
			}
		}

		MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), GetPlane());
		ResetAllSpots();
		ApplyPostProcess(ePostProcess_ExceptMirror);
		m_SideStairMode = eSideStairMode_PlaceFirstPoint;
	}

	return true;
}

bool StairProfileTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_SideStairMode == eSideStairMode_SelectDirection)
	{
		BrushVec3 vPosition;
		if (!GetModel()->QueryPosition(GetPlane(), GetDesigner()->GetRay(), vPosition))
			return true;

		DESIGNER_ASSERT(!m_CandidateStairs[0].empty());
		if (!m_CandidateStairs[0].empty())
		{
			BrushFloat dist2Pos = m_BorderLine.Distance(GetPlane().W2P(vPosition));
			BrushFloat dist2Candidate0 = m_BorderLine.Distance(GetPlane().W2P(m_CandidateStairs[0][1].m_Pos));

			if (dist2Pos * dist2Candidate0 > 0)
				m_nSelectedCandidate = 0;
			else
				m_nSelectedCandidate = 1;
		}
	}
	else
	{
		ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, m_SideStairMode == eSideStairMode_DrawDiagonal);
	}

	return true;
}

void StairProfileTool::CreateCandidates()
{
	int nStepNumber = 0;
	float fStepRise = m_StairProfileParameter.m_StepRise;
	EStairHeightCalculationWayMode heightMode = eStairHeightCalculationWay_StepRise;

	BrushVec2 vStartPoint = GetPlane().W2P(GetStartSpotPos());
	BrushVec2 vEndPoint = GetPlane().W2P(GetCurrentSpotPos());
	if (vStartPoint.y > vEndPoint.y)
	{
		std::swap(vStartPoint, vEndPoint);
		SwapCurrentAndStartSpots();
	}
	BrushVec2 vShrinkedEndPoint = vEndPoint;

	if (heightMode == eStairHeightCalculationWay_StepRise)
	{
		BrushVec3 vX3D = GetWorldTM().TransformVector(GetPlane().P2W(BrushVec2(1, 0)));
		BrushVec3 vY3D = GetWorldTM().TransformVector(GetPlane().P2W(BrushVec2(0, 1)));
		const BrushVec3 vZ(0, 0, 1);
		int nElement = std::abs(vZ.Dot(vX3D)) > std::abs(vZ.Dot(vY3D)) ? 0 : 1;

		BrushFloat fFullRise = std::abs(vEndPoint[nElement] - vStartPoint[nElement]);
		BrushFloat fFullLength = (vEndPoint - vStartPoint).GetLength();

		nStepNumber = fFullRise / fStepRise;
		vShrinkedEndPoint = vStartPoint + ((vEndPoint - vStartPoint).GetNormalized() * ((fStepRise * fFullLength) / fFullRise)) * nStepNumber;
	}

	BrushVec2 vStart2End = vShrinkedEndPoint - vStartPoint;

	if (fabs(vStart2End.x) < kDesignerEpsilon || fabs(vStart2End.y) < kDesignerEpsilon)
		return;

	m_BorderLine = BrushLine(vStartPoint, vEndPoint);

	BrushVec2 vOneStep = vStart2End / nStepNumber;
	BrushVec2 vCurrentShotDir(0, 1);
	BrushVec2 vNextShotDir(1, 0);

	for (int k = 0; k < 2; ++k)
	{
		BrushVec2 vCurrentPoint = vStartPoint;
		BrushVec2 vNextPoint = vCurrentPoint;

		m_CandidateStairs[k].clear();
		m_CandidateStairs[k].push_back(Convert2Spot(GetModel(), GetPlane(), vCurrentPoint));

		for (int i = 0; i < nStepNumber + 1; ++i)
		{
			vNextPoint += vOneStep;
			if (i == nStepNumber)
			{
				if (heightMode == eStairHeightCalculationWay_StepNumber)
					break;
				vNextPoint = vEndPoint;
			}

			BrushLine currentLine(vCurrentPoint, vCurrentPoint + vCurrentShotDir);
			BrushLine nextLine(vNextPoint, vNextPoint + vNextShotDir);

			BrushVec2 vIntersection;
			if (!currentLine.Intersect(nextLine, vIntersection))
			{
				GetIEditor()->GetIUndoManager()->Cancel();
				DESIGNER_ASSERT(0);
				return;
			}

			m_CandidateStairs[k].push_back(Convert2Spot(GetModel(), GetPlane(), vIntersection));
			m_CandidateStairs[k].push_back(Convert2Spot(GetModel(), GetPlane(), vNextPoint));

			vCurrentPoint = vNextPoint;
		}

		std::swap(vCurrentShotDir, vNextShotDir);

		m_CandidateStairs[k][0] = GetStartSpot();
		m_CandidateStairs[k][m_CandidateStairs[k].size() - 1] = GetCurrentSpot();
	}
}

Spot StairProfileTool::Convert2Spot(Model* pModel, const BrushPlane& plane, const BrushVec2& pos) const
{
	Spot spot(plane.P2W(pos));
	spot.m_Plane = plane;

	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		if (!pPolygon || !plane.IsEquivalent(pPolygon->GetPlane()))
			continue;

		if (pPolygon->HasPosition(spot.m_Pos))
		{
			spot.m_PosState = eSpotPosState_EitherPointOfEdge;
			spot.m_pPolygon = pPolygon;
		}
		else if (pPolygon->IsPositionOnBoundary(spot.m_Pos))
		{
			spot.m_PosState = eSpotPosState_Edge;
			spot.m_pPolygon = pPolygon;
		}
	}

	return spot;
}

bool StairProfileTool::IsPhaseFirstStepOnPrimitiveCreation() const
{
	return m_SideStairMode == eSideStairMode_PlaceFirstPoint;
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_StairProfile, eToolGroup_Shape, "Stair Profile", StairProfileTool,
                                                           stairprofile, "runs stair profile tool", "designer.stairprofile")

