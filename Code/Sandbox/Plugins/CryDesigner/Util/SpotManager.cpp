// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Viewport.h"
#include "SpotManager.h"
#include "Tools/BaseTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "Grid.h"
#include "SurfaceInfoPicker.h"

namespace Designer
{
void SpotManager::DrawCurrentSpot(DisplayContext& dc, const BrushMatrix34& worldTM) const
{
	static const ColorB edgeCenterColor(100, 255, 100, 255);
	static const ColorB polygonCenterColor(100, 255, 100, 255);
	static const ColorB eitherPointColor(255, 100, 255, 255);
	static const ColorB normalColor(100, 100, 100, 255);
	static const ColorB edgeColor(100, 100, 255, 255);
	static const ColorB startPointColor(255, 255, 100, 255);

	if (m_CurrentSpot.m_PosState == eSpotPosState_Edge || m_CurrentSpot.m_PosState == eSpotPosState_OnVirtualLine)
		DrawSpot(dc, worldTM, m_CurrentSpot.m_Pos, edgeColor);
	else if (m_CurrentSpot.m_PosState == eSpotPosState_CenterOfEdge)
		DrawSpot(dc, worldTM, m_CurrentSpot.m_Pos, edgeCenterColor);
	else if (m_CurrentSpot.m_PosState == eSpotPosState_CenterOfPolygon)
		DrawSpot(dc, worldTM, m_CurrentSpot.m_Pos, polygonCenterColor);
	else if (m_CurrentSpot.m_PosState == eSpotPosState_EitherPointOfEdge || m_CurrentSpot.IsAtEndPoint())
		DrawSpot(dc, worldTM, m_CurrentSpot.m_Pos, eitherPointColor);
	else if (m_CurrentSpot.m_PosState == eSpotPosState_AtFirstSpot)
		DrawSpot(dc, worldTM, m_CurrentSpot.m_Pos, startPointColor);
	else
		DrawSpot(dc, worldTM, m_CurrentSpot.m_Pos, normalColor);
}

void SpotManager::DrawPolyline(DisplayContext& dc) const
{
	if (m_SpotList.empty())
		return;

	for (int i = 0, iSpotSize(m_SpotList.size() - 1); i < iSpotSize; ++i)
	{
		if (m_SpotList[i].m_bProcessed)
			continue;
		const BrushVec3& currPos(m_SpotList[i].m_Pos);
		const BrushVec3& nextPos(m_SpotList[i + 1].m_Pos);
		dc.DrawLine(currPos, nextPos);
	}
}

bool SpotManager::AddPolygonToDesignerFromSpotList(Model* pModel, const SpotList& spotList)
{
	if (pModel == NULL || spotList.empty())
		return false;

	SpotList copiedSpotList(spotList);

	Spot firstSpot = copiedSpotList[0];
	Spot secondSpot = copiedSpotList.size() >= 2 ? copiedSpotList[1] : copiedSpotList[0];
	Spot lastSpot = copiedSpotList[copiedSpotList.size() - 1];

	if (firstSpot.IsEquivalentPos(lastSpot))
	{
		SpotList spotListWithoutBeginning(copiedSpotList);
		spotListWithoutBeginning.erase(spotListWithoutBeginning.begin());
		CreatePolygonFromSpots(true, spotListWithoutBeginning);
	}
	else if (firstSpot.IsAtEndPoint() && lastSpot.IsAtEndPoint())
	{
		PolygonPtr pPolygon0 = firstSpot.m_pPolygon;
		PolygonPtr pPolygon1 = lastSpot.m_pPolygon;
		if (pPolygon0)
		{
			for (int i = 0, nSpotSize(copiedSpotList.size() - 1); i < nSpotSize; ++i)
			{
				if (firstSpot.m_PosState == eSpotPosState_FirstPointOfPolygon)
					pPolygon0->AddEdge(BrushEdge3D(copiedSpotList[i + 1].m_Pos, copiedSpotList[i].m_Pos));
				else if (firstSpot.m_PosState == eSpotPosState_LastPointOfPolygon)
					pPolygon0->AddEdge(BrushEdge3D(copiedSpotList[i].m_Pos, copiedSpotList[i + 1].m_Pos));
			}
			if (pPolygon1 && pPolygon0 != pPolygon1)
			{
				if (pPolygon0->Concatenate(pPolygon1))
					pModel->RemovePolygon(pPolygon1);
			}

			BrushVec3 firstVertex;
			BrushVec3 lastVertex;
			pPolygon0->GetFirstVertex(firstVertex);
			pPolygon0->GetLastVertex(lastVertex);

			if (!pPolygon0->IsOpen())
			{
				pModel->RemovePolygon(pPolygon0);
				pPolygon0->ModifyOrientation();
				pModel->AddSplitPolygon(pPolygon0);
			}
			else if (IsVertexOnEdgeInModel(pModel, pPolygon0->GetPlane(), firstVertex, pPolygon0) &&
			         IsVertexOnEdgeInModel(pModel, pPolygon0->GetPlane(), lastVertex, pPolygon0))
			{
				pModel->RemovePolygon(pPolygon0);
				pModel->AddSplitPolygon(pPolygon0);
			}
		}
	}
	else if (firstSpot.IsAtEndPoint() || lastSpot.IsAtEndPoint())
	{
		if (!firstSpot.IsAtEndPoint())
		{
			std::swap(firstSpot, lastSpot);
			int iSize(copiedSpotList.size());
			for (int i = 0, iHalfSize(iSize / 2); i < iHalfSize; ++i)
				std::swap(copiedSpotList[i], copiedSpotList[iSize - i - 1]);
		}

		PolygonPtr pPolygon0 = firstSpot.m_pPolygon;
		if (!pPolygon0)
			return true;

		pModel->RemovePolygon(pPolygon0);

		for (int i = 0, nSpotSize(copiedSpotList.size() - 1); i < nSpotSize; ++i)
		{
			if (firstSpot.m_PosState == eSpotPosState_FirstPointOfPolygon)
				pPolygon0->AddEdge(BrushEdge3D(copiedSpotList[i + 1].m_Pos, copiedSpotList[i].m_Pos));
			else if (firstSpot.m_PosState == eSpotPosState_LastPointOfPolygon)
				pPolygon0->AddEdge(BrushEdge3D(copiedSpotList[i].m_Pos, copiedSpotList[i + 1].m_Pos));
		}

		if (!pPolygon0->IsOpen())
		{
			pModel->AddSplitPolygon(pPolygon0);
		}
		else
		{
			BrushVec3 firstVertex;
			BrushVec3 lastVertex;
			bool bOnlyAdd = false;

			bool bFirstOnEdge =
			  pPolygon0->GetFirstVertex(firstVertex) &&
			  IsVertexOnEdgeInModel(pModel, pPolygon0->GetPlane(), firstVertex, pPolygon0);
			bool bLastOnEdge =
			  pPolygon0->GetLastVertex(lastVertex) &&
			  IsVertexOnEdgeInModel(pModel, pPolygon0->GetPlane(), lastVertex, pPolygon0);

			if (bFirstOnEdge && bLastOnEdge)
				pModel->AddSplitPolygon(pPolygon0);
			else
				pModel->AddPolygon(pPolygon0);
		}
	}
	else if (firstSpot.IsOnEdge() && lastSpot.IsOnEdge())
	{
		BrushPlane plane;
		if (FindBestPlane(pModel, firstSpot, secondSpot, plane))
		{
			std::vector<BrushVec3> vList;
			GenerateVertexListFromSpotList(copiedSpotList, vList);
			PolygonPtr pOpenPolygon = new Polygon(vList, plane, 0, NULL, false);
			pModel->AddSplitPolygon(pOpenPolygon);
		}
	}
	else if (firstSpot.IsInPolygon() || lastSpot.IsInPolygon())
	{
		BrushPlane plane;
		if (FindBestPlane(pModel, firstSpot, secondSpot, plane))
		{
			std::vector<BrushVec3> vList;
			GenerateVertexListFromSpotList(copiedSpotList, vList);
			PolygonPtr pPolygon = new Polygon(vList, plane, 0, NULL, false);
			pModel->AddPolygon(pPolygon);
		}
	}
	else if (firstSpot.m_PosState == eSpotPosState_OutsideDesigner && lastSpot.m_PosState == eSpotPosState_OutsideDesigner)
	{
		if (firstSpot.m_Plane.IsEquivalent(lastSpot.m_Plane))
		{
			std::vector<BrushVec3> vList;
			GenerateVertexListFromSpotList(copiedSpotList, vList);
			PolygonPtr pPolygon = new Polygon(vList, firstSpot.m_Plane, 0, NULL, false);
			pModel->AddPolygon(pPolygon);
		}
	}
	else
	{
		ResetAllSpots();
		return false;
	}

	if (pModel)
		pModel->ResetDB(eDBRF_ALL);

	return true;
}

void SpotManager::GenerateVertexListFromSpotList(const SpotList& spotList, std::vector<BrushVec3>& outVList)
{
	int iSpotSize(spotList.size());
	outVList.reserve(iSpotSize);
	for (int i = 0; i < iSpotSize; ++i)
		outVList.push_back(spotList[i].m_Pos);
}

void SpotManager::RegisterSpotList(Model* pModel, const SpotList& spotList)
{
	if (pModel == NULL)
		return;

	if (spotList.size() <= 1)
	{
		ResetAllSpots();
		return;
	}

	std::vector<SpotList> splittedSpotList;
	SplitSpotList(pModel, spotList, splittedSpotList);

	if (!splittedSpotList.empty())
	{
		for (int i = 0, spotListCount(splittedSpotList.size()); i < spotListCount; ++i)
			AddPolygonToDesignerFromSpotList(pModel, splittedSpotList[i]);
	}
}

void SpotManager::SplitSpotList(Model* pModel, const SpotList& spotList, std::vector<SpotList>& outSpotLists)
{
	if (pModel == NULL)
		return;

	SpotList partSpotList;

	for (int k = 0, iSpotListCount(spotList.size()); k < iSpotListCount - 1; ++k)
	{
		const Spot& currentSpot = spotList[k];
		const Spot& nextSpot = spotList[k + 1];

		SpotPairList splittedSpotPairs;
		SplitSpot(pModel, SpotPair(currentSpot, nextSpot), splittedSpotPairs);

		if (splittedSpotPairs.empty())
			continue;

		std::map<BrushFloat, SpotPair> sortedSpotPairs;
		for (int i = 0, iSpotPairCount(splittedSpotPairs.size()); i < iSpotPairCount; ++i)
		{
			BrushFloat fDistance = currentSpot.m_Pos.GetDistance(splittedSpotPairs[i].m_Spot[0].m_Pos);
			sortedSpotPairs[fDistance] = splittedSpotPairs[i];
		}

		auto ii = sortedSpotPairs.begin();
		for (; ii != sortedSpotPairs.end(); ++ii)
		{
			const SpotPair& spotPair = ii->second;

			if (partSpotList.empty())
				partSpotList.push_back(spotPair.m_Spot[0]);

			partSpotList.push_back(spotPair.m_Spot[1]);
			if (!spotPair.m_Spot[1].IsEquivalentPos(nextSpot))
			{
				outSpotLists.push_back(partSpotList);
				partSpotList.clear();
			}
		}
	}
	if (!partSpotList.empty())
		outSpotLists.push_back(partSpotList);
}

void SpotManager::SplitSpot(Model* pModel, const SpotPair& spotPair, SpotPairList& outSpotPairs)
{
	if (pModel == NULL)
		return;

	bool bSubtracted = false;
	BrushEdge3D inputEdge(spotPair.m_Spot[0].m_Pos, spotPair.m_Spot[1].m_Pos);
	BrushEdge3D invInputEdge(spotPair.m_Spot[1].m_Pos, spotPair.m_Spot[0].m_Pos);

	BrushPlane plane;
	if (!FindBestPlane(pModel, spotPair.m_Spot[0], spotPair.m_Spot[1], plane))
		return;

	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		if (pPolygon == NULL)
			continue;

		if (!pPolygon->GetPlane().IsEquivalent(plane))
			continue;

		std::vector<BrushEdge3D> subtractedEdges;
		bSubtracted = pPolygon->SubtractEdge(inputEdge, subtractedEdges);

		if (!subtractedEdges.empty())
		{
			auto ii = subtractedEdges.begin();
			for (; ii != subtractedEdges.end(); )
			{
				if ((*ii).IsEquivalent(inputEdge) || (*ii).IsEquivalent(invInputEdge))
					ii = subtractedEdges.erase(ii);
				else
					++ii;
			}
			if (subtractedEdges.empty())
				bSubtracted = false;
		}

		if (subtractedEdges.empty())
		{
			if (bSubtracted)
				break;
			continue;
		}

		if (subtractedEdges.size() == 1)
		{
			auto ii = subtractedEdges.begin();
			if (Comparison::IsEquivalent((*ii).m_v[0], (*ii).m_v[1]))
				continue;
		}

		int iSubtractedEdgeCount(subtractedEdges.size());

		for (int k = 0; k < iSubtractedEdgeCount; ++k)
		{
			BrushEdge3D subtractedEdge = subtractedEdges[k];
			SpotPair subtractedSpotPair;

			for (int a = 0; a < 2; ++a)
			{
				subtractedSpotPair.m_Spot[a].m_Pos = subtractedEdge.m_v[a];

				if (pPolygon->HasPosition(subtractedEdge.m_v[a]))
				{
					subtractedSpotPair.m_Spot[a].m_PosState = eSpotPosState_EitherPointOfEdge;
					subtractedSpotPair.m_Spot[a].m_pPolygon = pPolygon;
				}
				else if (pPolygon->IsPositionOnBoundary(subtractedEdge.m_v[a]))
				{
					subtractedSpotPair.m_Spot[a].m_PosState = eSpotPosState_Edge;
					subtractedSpotPair.m_Spot[a].m_pPolygon = pPolygon;
				}
			}

			SplitSpot(pModel, subtractedSpotPair, outSpotPairs);
		}
		break;
	}

	if (!bSubtracted)
		outSpotPairs.push_back(spotPair);
}

bool SpotManager::FindSpotNearAxisAlignedLine(IDisplayViewport* pViewport, PolygonPtr pPolygon, const BrushMatrix34& worldTM, Spot& outSpot)
{
	if (m_CurrentSpot.IsOnEdge() || !pPolygon->Include(m_CurrentSpot.m_Pos))
		return false;

	std::vector<BrushLine> axisAlignedLines;
	if (!pPolygon->QueryAxisAlignedLines(axisAlignedLines))
		return false;

	BrushVec2 vCurrentPos2D = pPolygon->GetPlane().W2P(m_CurrentSpot.m_Pos);
	for (int i = 0, iLineCount(axisAlignedLines.size()); i < iLineCount; ++i)
	{
		BrushVec2 vHitPos;
		if (!axisAlignedLines[i].HitTest(vCurrentPos2D, vCurrentPos2D + axisAlignedLines[i].m_Normal, 0, &vHitPos))
			continue;

		BrushFloat fLength(0);
		if (AreTwoPositionsClose(pPolygon->GetPlane().P2W(vHitPos), m_CurrentSpot.m_Pos, worldTM, pViewport, kLimitForMagnetic, &fLength))
		{
			outSpot.Reset();
			outSpot.m_pPolygon = pPolygon;
			outSpot.m_PosState = eSpotPosState_OnVirtualLine;
			outSpot.m_Plane = pPolygon->GetPlane();
			outSpot.m_Pos = outSpot.m_Plane.P2W(vHitPos);
			return true;
		}
	}

	return false;
}

bool SpotManager::FindNicestSpot(IDisplayViewport* pViewport, const std::vector<CandidateInfo>& candidates, const Model* pModel, const BrushMatrix34& worldTM, const BrushVec3& pickedPos, PolygonPtr pPickedPolygon, const BrushPlane& plane, Spot& outSpot) const
{
	if (!pModel)
		return false;

	BrushFloat fMinimumLength(3e10f);

	for (int i = 0; i < candidates.size(); ++i)
	{
		BrushFloat fLength(0);
		if (!AreTwoPositionsClose(pickedPos, candidates[i].m_Pos, worldTM, pViewport, kLimitForMagnetic, &fLength))
			continue;

		if (fLength >= fMinimumLength)
			continue;

		fMinimumLength = fLength;

		outSpot.m_Pos = candidates[i].m_Pos;
		outSpot.m_PosState = candidates[i].m_SpotPosState;
		if (pPickedPolygon)
		{
			outSpot.m_pPolygon = pPickedPolygon;
			outSpot.m_Plane = pPickedPolygon->GetPlane();
		}
		else
		{
			outSpot.m_pPolygon = NULL;
			outSpot.m_Plane = plane;
		}

		if (outSpot.m_PosState != eSpotPosState_EitherPointOfEdge)
			continue;

		bool bFirstPoint = false;
		if (!pPickedPolygon || !pPickedPolygon->IsEndPoint(outSpot.m_Pos, &bFirstPoint))
			continue;

		if (bFirstPoint)
			outSpot.m_PosState = eSpotPosState_FirstPointOfPolygon;
		else
			outSpot.m_PosState = eSpotPosState_LastPointOfPolygon;
	}

	return true;
}

bool SpotManager::FindSnappedSpot(const BrushMatrix34& worldTM, PolygonPtr pPickedPolygon, const BrushVec3& pickedPos, Spot& outSpot) const
{
	bool bEnableSnap = IsSnapEnabled();
	if (!bEnableSnap)
		return false;

	if (!pPickedPolygon || pPickedPolygon->IsOpen())
		return false;

	BrushPlane pickedPlane(pPickedPolygon->GetPlane());
	BrushVec3 snappedPlanePos = Snap(pickedPos);
	outSpot.m_Pos = pickedPlane.P2W(pickedPlane.W2P(snappedPlanePos));
	outSpot.m_pPolygon = pPickedPolygon;
	outSpot.m_Plane = pPickedPolygon->GetPlane();
	return true;
}

bool SpotManager::UpdateCurrentSpotPosition(
  Model* pModel,
  const BrushMatrix34& worldTM,
  const BrushPlane& plane,
  IDisplayViewport* view,
  CPoint point,
  bool bKeepInitialPlane,
  bool bSearchAllShelves)
{
	if (pModel == NULL)
		return false;

	BrushPlane pickedPlane(plane);
	BrushVec3 pickedPos(m_CurrentSpot.m_Pos);
	bool bSuccessQuery(false);
	PolygonPtr pPickedPolygon;
	ShelfID nShelf = eShelf_Any;

	ResetCurrentSpotWeakly();

	if (!bSearchAllShelves)
	{
		if (bKeepInitialPlane)
			bSuccessQuery = pModel->QueryPosition(pickedPlane, GetDesigner()->GetRay(), pickedPos, NULL, &pPickedPolygon);
		else
			bSuccessQuery = pModel->QueryPosition(GetDesigner()->GetRay(), pickedPos, &pickedPlane, NULL, &pPickedPolygon);
	}
	else
	{
		PolygonPtr pPolygons[2] = { NULL, NULL };
		bool bSuccessShelfQuery[2] = { false, false };
		MODEL_SHELF_RECONSTRUCTOR(pModel);
		for (int i = eShelf_Base; i < cShelfMax; ++i)
		{
			pModel->SetShelf(static_cast<ShelfID>(i));
			if (bKeepInitialPlane)
				bSuccessShelfQuery[i] = pModel->QueryPosition(pickedPlane, GetDesigner()->GetRay(), pickedPos, NULL, &(pPolygons[i]));
			else
				bSuccessShelfQuery[i] = pModel->QueryPosition(GetDesigner()->GetRay(), pickedPos, &pickedPlane, NULL, &(pPolygons[i]));
		}

		BrushFloat ts[2] = { 3e10, 3e10 };
		for (int i = 0; i < 2; ++i)
		{
			if (pPolygons[i])
				pPolygons[i]->IsPassed(GetDesigner()->GetRay(), ts[i]);
		}

		if (ts[0] < ts[1])
		{
			pPickedPolygon = pPolygons[0];
			bSuccessQuery = bSuccessShelfQuery[0];
			nShelf = eShelf_Base;
		}
		else
		{
			pPickedPolygon = pPolygons[1];
			bSuccessQuery = bSuccessShelfQuery[1];
			nShelf = eShelf_Construction;
		}
	}

	if (pPickedPolygon && pPickedPolygon->CheckFlags(ePolyFlag_Mirrored | ePolyFlag_Hidden))
	{
		m_CurrentSpot.m_pPolygon = NULL;
		return false;
	}

	if (!bSuccessQuery || bKeepInitialPlane && !pPickedPolygon)
	{
		if (!bSuccessQuery)
		{
			if (!GetPosAndPlaneFromWorld(view, point, worldTM, pickedPos, pickedPlane))
				return false;
		}
		m_CurrentSpot.m_pPolygon = NULL;
		m_CurrentSpot.m_Pos = pickedPos;
		m_CurrentSpot.m_PosState = eSpotPosState_OutsideDesigner;
		m_CurrentSpot.m_Plane = pickedPlane;

		Spot niceSpot(m_CurrentSpot);
		std::vector<CandidateInfo> candidates;
		if (GetSpotListCount() > 0)
			candidates.push_back(CandidateInfo(GetSpot(0).m_Pos, eSpotPosState_AtFirstSpot));

		std::vector<PolygonPtr> penetratedOpenPolygons;
		pModel->QueryOpenPolygons(GetDesigner()->GetRay(), penetratedOpenPolygons);
		PolygonPtr pConnectedPolygon = NULL;
		for (int i = 0, iCount(penetratedOpenPolygons.size()); i < iCount; ++i)
		{
			if (GetSpotListCount() > 0 && !GetSpot(0).m_Plane.IsEquivalent(penetratedOpenPolygons[i]->GetPlane()))
				continue;
			std::vector<Vertex> vList;
			penetratedOpenPolygons[i]->GetLinkedVertices(vList);
			pConnectedPolygon = penetratedOpenPolygons[i];
			for (int k = 0, iVCount(vList.size()); k < iVCount; ++k)
			{
				if (k == 0)
					candidates.push_back(CandidateInfo(vList[k].pos, eSpotPosState_FirstPointOfPolygon));
				else if (k == iVCount - 1)
					candidates.push_back(CandidateInfo(vList[k].pos, eSpotPosState_LastPointOfPolygon));
				else
					candidates.push_back(CandidateInfo(vList[k].pos, eSpotPosState_EitherPointOfEdge));
				if (k < iVCount - 1)
				{
					candidates.push_back(CandidateInfo((vList[k].pos + vList[k + 1].pos) * 0.5f, eSpotPosState_CenterOfEdge));
					BrushEdge3D edge(vList[k].pos, vList[k + 1].pos);
					BrushVec3 posOnEdge;
					bool bInEdge = false;
					if (edge.CalculateNearestVertex(pickedPos, posOnEdge, bInEdge) && bInEdge && AreTwoPositionsClose(pickedPos, posOnEdge, worldTM, view, kLimitForMagnetic))
					{
						if (!Comparison::IsEquivalent(posOnEdge, vList[k].pos) && !Comparison::IsEquivalent(posOnEdge, vList[k + 1].pos))
							candidates.push_back(CandidateInfo(posOnEdge, eSpotPosState_Edge));
					}
				}
			}
		}
		if (!candidates.empty() && FindNicestSpot(view, candidates, pModel, worldTM, pickedPos, pConnectedPolygon, plane, niceSpot))
			m_CurrentSpot = niceSpot;

		if (!m_CurrentSpot.m_pPolygon)
			m_CurrentSpot.m_Pos = Snap(m_CurrentSpot.m_Pos);
		return true;
	}

	m_CurrentSpot.m_pPolygon = pPickedPolygon;

	bool bEnableSnap = IsSnapEnabled();
	if (!m_bEnableMagnetic && !bEnableSnap)
	{
		if (bSuccessQuery)
		{
			m_CurrentSpot.m_Pos = pickedPos;
			m_CurrentSpot.m_Plane = pickedPlane;
		}
		return true;
	}

	if (bEnableSnap && pPickedPolygon == NULL)
	{
		m_CurrentSpot.m_Plane = pickedPlane;
		m_CurrentSpot.m_Pos = Snap(pickedPos);
		m_CurrentSpot.m_pPolygon = NULL;
		return true;
	}

	BrushVec3 nearestEdge[2];
	BrushVec3 posOnEdge;
	std::vector<QueryEdgeResult> queryResults;
	std::vector<BrushEdge3D> queryEdges;

	{
		MODEL_SHELF_RECONSTRUCTOR(pModel);
		if (nShelf != eShelf_Any)
			pModel->SetShelf(nShelf);
		bSuccessQuery = pModel->QueryNearestEdges(pickedPlane, GetDesigner()->GetRay(), pickedPos, posOnEdge, queryResults);
	}

	if (bKeepInitialPlane && !bSuccessQuery)
	{
		if (!bEnableSnap)
			m_CurrentSpot.m_Pos = pickedPos;
		m_CurrentSpot.m_Plane = pickedPlane;
		return true;
	}

	if (!bSuccessQuery)
		return false;

	for (int i = 0, iQueryResultsCount(queryResults.size()); i < iQueryResultsCount; ++i)
		queryEdges.push_back(queryResults[i].m_Edge);

	if (m_bEnableMagnetic && AreTwoPositionsClose(pickedPos, posOnEdge, worldTM, view, kLimitForMagnetic))
	{
		m_CurrentSpot.m_Pos = posOnEdge;
		m_CurrentSpot.m_PosState = eSpotPosState_Edge;
	}
	else if (!bEnableSnap)
	{
		m_CurrentSpot.m_Pos = pickedPos;
	}
	m_CurrentSpot.m_Plane = pickedPlane;

	int nShortestIndex = FindShortestEdge(queryEdges);
	pPickedPolygon = queryResults[nShortestIndex].m_pPolygon;
	const BrushEdge3D& edge = queryEdges[nShortestIndex];

	Spot niceSpot(m_CurrentSpot);
	std::vector<CandidateInfo> candidates;
	candidates.push_back(CandidateInfo(edge.m_v[0], eSpotPosState_EitherPointOfEdge));
	candidates.push_back(CandidateInfo(edge.m_v[1], eSpotPosState_EitherPointOfEdge));
	candidates.push_back(CandidateInfo((edge.m_v[0] + edge.m_v[1]) * 0.5f, eSpotPosState_CenterOfEdge));
	if (GetSpotListCount() > 0)
		candidates.push_back(CandidateInfo(GetSpot(0).m_Pos, eSpotPosState_AtFirstSpot));
	BrushVec3 centerPolygonPos(m_CurrentSpot.m_Pos);
	{
		MODEL_SHELF_RECONSTRUCTOR(pModel);
		if (nShelf != eShelf_Any)
			pModel->SetShelf(nShelf);
		if (pModel->QueryCenterOfPolygon(GetDesigner()->GetRay(), centerPolygonPos))
			candidates.push_back(CandidateInfo(centerPolygonPos, eSpotPosState_CenterOfPolygon));
	}
	if (FindNicestSpot(view, candidates, pModel, worldTM, pickedPos, pPickedPolygon, plane, niceSpot))
		m_CurrentSpot = niceSpot;

	Spot snappedSpot;
	if (FindSnappedSpot(worldTM, m_CurrentSpot.m_pPolygon, pickedPos, snappedSpot))
	{
		m_CurrentSpot = snappedSpot;
		queryResults.clear();
		MODEL_SHELF_RECONSTRUCTOR(pModel);
		pModel->SetShelf(eShelf_Base);
		pModel->QueryNearestEdges(pickedPlane, m_CurrentSpot.m_Pos, posOnEdge, queryResults);
		if (Comparison::IsEquivalent(posOnEdge, m_CurrentSpot.m_Pos))
			m_CurrentSpot.m_PosState = eSpotPosState_Edge;
		else if (bSearchAllShelves)
		{
			pModel->SetShelf(eShelf_Construction);
			pModel->QueryNearestEdges(pickedPlane, m_CurrentSpot.m_Pos, posOnEdge, queryResults);
			if (Comparison::IsEquivalent(posOnEdge, m_CurrentSpot.m_Pos))
				m_CurrentSpot.m_PosState = eSpotPosState_Edge;
		}
		return true;
	}

	Spot nearSpot(m_CurrentSpot);
	if (FindSpotNearAxisAlignedLine(view, pPickedPolygon, worldTM, nearSpot))
		m_CurrentSpot = nearSpot;

	return bSuccessQuery;
}

void SpotManager::AddSpotToSpotList(const Spot& spot)
{
	bool bExistSameSpot(false);
	for (int i = 0, iSpotSize(GetSpotListCount()); i < iSpotSize; ++i)
	{
		if (GetSpot(i).IsEquivalentPos(spot) && !GetSpot(i).m_bProcessed)
		{
			bExistSameSpot = true;
			break;
		}
	}

	if (!bExistSameSpot)
		m_SpotList.push_back(spot);
}

void SpotManager::ReplaceSpotList(const SpotList& spotList)
{
	m_SpotList = spotList;
}

bool SpotManager::GetPlaneBeginEndPoints(const BrushPlane& plane, BrushVec2& outProjectedStartPt, BrushVec2& outProjectedEndPt) const
{
	outProjectedStartPt = plane.W2P(GetStartSpotPos());
	outProjectedEndPt = plane.W2P(GetCurrentSpotPos());

	const BrushFloat SmallestPolygonSize(0.01f);
	if (std::abs(outProjectedEndPt.x - outProjectedStartPt.x) < SmallestPolygonSize || std::abs(outProjectedEndPt.y - outProjectedStartPt.y) < SmallestPolygonSize)
		return false;

	const BrushVec2 vecMinimum(-1000.0f, -1000.0f);
	const BrushVec2 vecMaximum(1000.0f, 1000.0f);

	outProjectedStartPt.x = std::max(outProjectedStartPt.x, vecMinimum.x);
	outProjectedStartPt.y = std::max(outProjectedStartPt.y, vecMinimum.y);
	outProjectedEndPt.x = std::min(outProjectedEndPt.x, vecMaximum.x);
	outProjectedEndPt.y = std::min(outProjectedEndPt.y, vecMaximum.y);

	return true;
}

bool SpotManager::GetPosAndPlaneFromWorld(IDisplayViewport* view, const CPoint& point, const BrushMatrix34& worldTM, BrushVec3& outPos, BrushPlane& outPlane)
{
	Vec3 vPickedPos;
	if (!PickPosFromWorld(view, point, vPickedPos))
		return false;
	Matrix34 invTM = worldTM.GetInverted();
	Vec3 p0 = invTM.TransformPoint(vPickedPos);
	outPlane = BrushPlane(BrushVec3(0, 0, 1), -p0.Dot(BrushVec3(0, 0, 1)));
	outPos = ToBrushVec3(p0);
	return true;
}

bool SpotManager::FindBestPlane(Model* pModel, const Spot& s0, const Spot& s1, BrushPlane& outPlane)
{
	if (s0.m_pPolygon && s0.m_pPolygon == s1.m_pPolygon)
	{
		outPlane = s0.m_pPolygon->GetPlane();
		return true;
	}

	BrushPlane candidatePlanes[] = { s0.m_Plane, s1.m_Plane };
	for (int i = 0, iCandidatePlaneCount(sizeof(candidatePlanes) / sizeof(*candidatePlanes)); i < iCandidatePlaneCount; ++i)
	{
		if (candidatePlanes[i].Normal().IsZero(kDesignerEpsilon))
			continue;
		BrushFloat d0 = candidatePlanes[i].Distance(s0.m_Pos);
		BrushFloat d1 = candidatePlanes[i].Distance(s1.m_Pos);
		if (std::abs(d0) < kDesignerEpsilon && std::abs(d1) < kDesignerEpsilon)
		{
			outPlane = candidatePlanes[i];
			return true;
		}
	}

	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		const BrushPlane& plane = pModel->GetPolygon(i)->GetPlane();
		BrushFloat d0 = plane.Distance(s0.m_Pos);
		BrushFloat d1 = plane.Distance(s1.m_Pos);
		if (std::abs(d0) < kDesignerEpsilon && std::abs(d1) < kDesignerEpsilon)
		{
			outPlane = plane;
			return true;
		}
	}

	DESIGNER_ASSERT(0);
	return false;
}

bool SpotManager::IsSnapEnabled() const
{
	if (!m_bBuiltInSnap)
		return gSnappingPreferences.gridSnappingEnabled() | m_bBuiltInSnap;
	return true;
}

BrushVec3 SpotManager::Snap(const BrushVec3& vPos) const
{
	if (!m_bBuiltInSnap)
		return gSnappingPreferences.Snap(vPos);

	BrushVec3 snapped;
	snapped.x = std::floor((vPos.x / m_BuiltInSnapSize) + (BrushFloat)0.5) * m_BuiltInSnapSize;
	snapped.y = std::floor((vPos.y / m_BuiltInSnapSize) + (BrushFloat)0.5) * m_BuiltInSnapSize;
	snapped.z = std::floor((vPos.z / m_BuiltInSnapSize) + (BrushFloat)0.5) * m_BuiltInSnapSize;

	return snapped;
}

bool SpotManager::IsVertexOnEdgeInModel(Model* pModel, const BrushPlane& plane, const BrushVec3& vertex, PolygonPtr pExcludedPolygon) const
{
	for (int i = 0, iPolygonSize(pModel->GetPolygonCount()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		if (pPolygon == NULL)
			continue;
		if (pExcludedPolygon && pExcludedPolygon == pPolygon)
			continue;
		const BrushPlane& polygonPlane = pPolygon->GetPlane();
		if (!plane.IsEquivalent(polygonPlane))
			continue;
		BrushEdge3D nearestEdge;
		BrushVec3 hitPos;
		if (!pPolygon->QueryNearestEdge(vertex, nearestEdge, hitPos))
			continue;
		if ((vertex - hitPos).GetLength() < kDesignerEpsilon)
			return true;
	}
	return false;
}
}

