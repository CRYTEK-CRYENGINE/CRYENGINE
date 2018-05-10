// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Core/Model.h"
#include "Core/ModelDB.h"
#include "Core/Helper.h"
#include "DesignerEditor.h"
#include "BevelTool.h"
#include "Core/LoopPolygons.h"

namespace Designer
{
int GetDifferentVerticesCount(const std::vector<BrushVec3>& vList)
{
	std::set<BrushVec3, Comparison::less_BrushVec3> vSet;
	for (int i = 0, iSize(vList.size()); i < iSize; ++i)
		vSet.insert(vList[i]);
	return vSet.size();
}

void BevelTool::Enter()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (pSelected->IsEmpty())
	{
		GetDesigner()->SwitchToPrevTool();
		return;
	}

	m_BevelMode = eBevelMode_Nothing;
	m_nMousePrevY = 0;
	m_fDelta = 0;

	if (m_BevelMode == eBevelMode_Nothing)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		GetModel()->RecordUndo("Designer : Bevel", GetBaseObject());
		PP0_Initialize();
	}
}

void BevelTool::Leave()
{
	if (m_BevelMode == eBevelMode_Done)
	{
		MODEL_SHELF_RECONSTRUCTOR(GetModel());
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		GetModel()->SetShelf(eShelf_Construction);
		GetModel()->Clear();
		ApplyPostProcess();
		GetIEditor()->GetIUndoManager()->Accept("Designer : Bevel");
	}
	else
	{
		MODEL_SHELF_RECONSTRUCTOR(GetModel());

		GetModel()->SetShelf(eShelf_Construction);
		GetModel()->Clear();
		CompileShelf(eShelf_Construction);
		GetIEditor()->GetIUndoManager()->Cancel();
	}
	m_BevelMode = eBevelMode_Nothing;
}

bool BevelTool::PP0_Initialize(bool bSpreadEdge)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (pSelected->IsEmpty())
		return false;

	if (!m_pOriginalModel)
		m_pOriginalModel = new Model;
	(*m_pOriginalModel) = *GetModel();

	ModelDB::QueryResult queryResult;
	OrganizedQueryResults organizedQueryResult;

	m_OriginalPolygons.clear();
	m_OriginalSelectedElements.Clear();

	for (int i = 0, iElementSize(pSelected->GetCount()); i < iElementSize; ++i)
	{
		if ((*pSelected)[i].IsEdge())
		{
			if (m_OriginalSelectedElements.Has((*pSelected)[i]))
				continue;
			m_OriginalSelectedElements.Add((*pSelected)[i]);
			for (int k = 0, iVertexSize((*pSelected)[i].m_Vertices.size()); k < iVertexSize; ++k)
				GetModel()->GetDB()->QueryAsVertex((*pSelected)[i].m_Vertices[k], queryResult);
		}
		else if ((*pSelected)[i].IsPolygon())
		{
			for (int k = 0, iEdgeSize((*pSelected)[i].m_pPolygon->GetEdgeCount()); k < iEdgeSize; ++k)
			{
				BrushEdge3D edge = (*pSelected)[i].m_pPolygon->GetEdge(k);
				Element elementInfo;
				elementInfo.m_Vertices.push_back(edge.m_v[0]);
				elementInfo.m_Vertices.push_back(edge.m_v[1]);
				elementInfo.m_pObject = (*pSelected)[i].m_pObject;
				elementInfo.m_pPolygon = NULL;
				if (m_OriginalSelectedElements.Has(elementInfo))
					continue;
				m_OriginalSelectedElements.Add(elementInfo);
				GetModel()->GetDB()->QueryAsVertex(edge.m_v[0], queryResult);
				GetModel()->GetDB()->QueryAsVertex(edge.m_v[1], queryResult);
			}
		}
	}

	if (m_OriginalSelectedElements.IsEmpty())
		return false;

	pSelected->Clear();

	GetModel()->SetShelf(eShelf_Base);
	organizedQueryResult = SelectTool::CreateOrganizedResultsAroundPolygonFromQueryResults(queryResult);

	std::vector<PolygonPtr> removedPolygons;
	auto ii = organizedQueryResult.begin();
	for (; ii != organizedQueryResult.end(); ++ii)
	{
		PolygonPtr pPolygon = ii->first;
		removedPolygons.push_back(pPolygon);
		m_OriginalPolygons.push_back(pPolygon);
	}

	for (int i = 0, iRemovedPolygonSize(removedPolygons.size()); i < iRemovedPolygonSize; ++i)
	{
		GetModel()->RemovePolygon(removedPolygons[i]);
		MirrorUtil::RemoveMirroredPolygon(GetModel(), removedPolygons[i]);
	}

	PP0_SpreadEdges(0, bSpreadEdge);
	ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SmoothingGroup);

	m_BevelMode = eBevelMode_Spread;

	return true;
}

bool BevelTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	m_nMousePrevY = point.y;

	if (m_BevelMode == eBevelMode_Spread)
	{
		MODEL_SHELF_RECONSTRUCTOR(GetModel());
		GetModel()->SetShelf(eShelf_Construction);

		for (int i = 0, iPolygonSize(m_ResultForSecondPhase.middlePhaseEdgePolygons.size()); i < iPolygonSize; ++i)
			GetModel()->RemovePolygon(m_ResultForSecondPhase.middlePhaseEdgePolygons[i]);

		for (int i = 0, iPolygonSize(m_ResultForSecondPhase.middlePhaseSidePolygons.size()); i < iPolygonSize; ++i)
			GetModel()->RemovePolygon(m_ResultForSecondPhase.middlePhaseSidePolygons[i]);

		for (int i = 0, iPolygonSize(m_ResultForSecondPhase.middlePhaseBottomPolygons.size()); i < iPolygonSize; ++i)
			GetModel()->RemovePolygon(m_ResultForSecondPhase.middlePhaseBottomPolygons[i]);

		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);

		for (int i = 0, iPolygonSize(m_ResultForSecondPhase.middlePhaseEdgePolygons.size()); i < iPolygonSize; ++i)
			GetModel()->AddPolygon(m_ResultForSecondPhase.middlePhaseEdgePolygons[i]);

		for (int i = 0, iPolygonSize(m_ResultForSecondPhase.middlePhaseSidePolygons.size()); i < iPolygonSize; ++i)
		{
			if (GetModel()->QueryEquivalentPolygon(m_ResultForSecondPhase.middlePhaseSidePolygons[i]))
				continue;
			GetModel()->AddPolygon(m_ResultForSecondPhase.middlePhaseSidePolygons[i]);
		}

		for (int i = 0, iPolygonSize(m_ResultForSecondPhase.middlePhaseBottomPolygons.size()); i < iPolygonSize; ++i)
			GetModel()->AddPolygon(m_ResultForSecondPhase.middlePhaseBottomPolygons[i]);

		ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SmoothingGroup);

		m_nDividedNumber = 0;

		m_BevelMode = eBevelMode_Divide;
	}
	else if (m_BevelMode == eBevelMode_Divide)
	{
		GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
		UpdateSurfaceInfo();
		ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SmoothingGroup);
		m_BevelMode = eBevelMode_Nothing;
		GetIEditor()->GetIUndoManager()->Accept("Designer : Bevel");
		m_fDelta = 0;

		m_BevelMode = eBevelMode_Done;
	}

	return true;
}

bool AreAllCorrespondingEdgesInSameDirection(PolygonPtr pPolygon0, PolygonPtr pPolygon1)
{
	int nEdgeCount0 = pPolygon0->GetVertexCount();
	int nEdgeCount1 = pPolygon1->GetVertexCount();

	if (nEdgeCount0 != nEdgeCount1)
		return false;

	for (int i = 0; i < nEdgeCount0; ++i)
	{
		BrushEdge3D e0 = pPolygon0->GetEdge(i);
		BrushEdge3D e1 = pPolygon1->GetEdge(i);

		BrushVec3 vDir0 = (e0.m_v[1] - e0.m_v[0]).GetNormalized();
		BrushVec3 vDir1 = (e1.m_v[1] - e1.m_v[0]).GetNormalized();

		if (vDir0.Dot(vDir1) < 0)
			return false;
	}

	return true;
}

bool HasCrossEdges(PolygonPtr pPolygon)
{
	if (pPolygon->IsOpen())
		return true;

	std::vector<Vertex> vList;
	pPolygon->GetLinkedVertices(vList);

	std::vector<BrushEdge> eList;
	for (int i = 0, iVertexCount(vList.size()); i < iVertexCount; ++i)
		eList.push_back(BrushEdge(pPolygon->GetPlane().W2P(vList[i].pos), pPolygon->GetPlane().W2P(vList[(i + 1) % iVertexCount].pos)));

	for (int i = 0, iEdgeCount(eList.size()); i < iEdgeCount; ++i)
	{
		const BrushEdge& e0 = eList[i];
		for (int k = 0; k < iEdgeCount; ++k)
		{
			if (i == k || i == (k + 1) % iEdgeCount || (i + 1) % iEdgeCount == k)
				continue;

			const BrushEdge& e1 = eList[k];

			if (e0.IsIntersect(e1))
				return true;
		}
	}

	return false;
}

bool IsPolygonValid(PolygonPtr pPolygon, PolygonPtr pOriginalPolygon)
{
	if (pOriginalPolygon->HasHoles())
	{
		std::vector<PolygonPtr> innerPolygons = pPolygon->GetLoops()->GetHoleClones();
		std::vector<PolygonPtr> outerPolygons = pPolygon->GetLoops()->GetOuterPolygons();

		if (outerPolygons.size() != 1)
			return false;

		for (int k = 0, iInnterPolygonCount(innerPolygons.size()); k < iInnterPolygonCount; ++k)
		{
			innerPolygons[k]->ReverseEdges();
			if (!outerPolygons[0]->IncludeAllEdges(innerPolygons[k]))
				return false;
		}

		if (!outerPolygons[0]->IsCCW())
			return false;
	}
	else
	{
		for (int k = 0, iEdgeCount(pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
		{
			BrushEdge3D e = pPolygon->GetEdge(k);
			if (e.GetLength() < kDesignerLooseEpsilon)
				return false;
		}
		if (!pPolygon->IsCCW() || !AreAllCorrespondingEdgesInSameDirection(pPolygon, pOriginalPolygon))
			return false;
	}

	return true;
}

bool BevelTool::PP1_PushEdgesAndVerticesOut(ResultForNextPhase& outResultForNextPhase, MappingInfo& outMappingInfo)
{
	std::vector<int> vertices;
	std::vector<int> selectedElements;

	std::vector<PolygonPtr> updatedPolygons;

	for (int i = 0, iPolygonSize(m_OriginalPolygons.size()); i < iPolygonSize; ++i)
	{
		PolygonPtr pOriginalPolygon = m_OriginalPolygons[i];
		PolygonPtr pPolygon = pOriginalPolygon->Clone();
		updatedPolygons.push_back(pPolygon);

		std::set<int> edgeSetMatchingSelectedElement;
		std::map<int, int> mapBetweenEdgeIndexAndElement;

		for (int k = 0, iElementSize(m_OriginalSelectedElements.GetCount()); k < iElementSize; ++k)
		{
			BrushEdge3D edge(m_OriginalSelectedElements[k].m_Vertices[0], m_OriginalSelectedElements[k].m_Vertices[1]);
			int nEdgeIndex = -1;
			if (!pPolygon->HasEdge(edge, false, &nEdgeIndex))
				continue;
			edgeSetMatchingSelectedElement.insert(nEdgeIndex);
			mapBetweenEdgeIndexAndElement[nEdgeIndex] = k;
		}

		if (!edgeSetMatchingSelectedElement.empty())
		{
			auto iEdgeMatchingSelectedElement = edgeSetMatchingSelectedElement.begin();
			for (; iEdgeMatchingSelectedElement != edgeSetMatchingSelectedElement.end(); ++iEdgeMatchingSelectedElement)
			{
				int nPrevEdgeIndex = -1;
				int nNextEdgeIndex = -1;
				if (m_OriginalPolygons[i]->GetAdjacentEdgesByEdgeIndex(*iEdgeMatchingSelectedElement, &nPrevEdgeIndex, &nNextEdgeIndex))
				{
					BrushEdge3D originalEdge = m_OriginalPolygons[i]->GetEdge(*iEdgeMatchingSelectedElement);
					const IndexPair& originalEdgeIndexPair = m_OriginalPolygons[i]->GetEdgeIndexPair(*iEdgeMatchingSelectedElement);

					BrushVec3 vUpdatedPos[2] = { originalEdge.m_v[0], originalEdge.m_v[1] };
					BrushVec3 vOriginalEdgeDir = (originalEdge.m_v[1] - originalEdge.m_v[0]).GetNormalized();

					BrushEdge3D prevEdge = m_OriginalPolygons[i]->GetEdge(nPrevEdgeIndex);
					BrushVec3 vPrevEdgeDir = (prevEdge.m_v[0] - prevEdge.m_v[1]).GetNormalized();

					if (edgeSetMatchingSelectedElement.find(nPrevEdgeIndex) == edgeSetMatchingSelectedElement.end())
					{
						if (GetEdgeCountHavingVertexInElementList(prevEdge.m_v[1], m_OriginalSelectedElements) == 1)
						{
							BrushFloat fTheta = std::acos(vOriginalEdgeDir.Dot(-vPrevEdgeDir));
							vUpdatedPos[0] = prevEdge.m_v[1] + vPrevEdgeDir * (m_fDelta / std::sin(fTheta));
						}
						else
						{
							vUpdatedPos[0] = prevEdge.m_v[1] + vPrevEdgeDir * m_fDelta;
						}

						pPolygon->SetPos(originalEdgeIndexPair.m_i[0], vUpdatedPos[0]);
					}
					else
					{
						BrushVec3 vInclinedDir = (vOriginalEdgeDir + vPrevEdgeDir).GetNormalized();
						BrushFloat fTheta = std::acos(vOriginalEdgeDir.Dot(vInclinedDir));
						if (vOriginalEdgeDir.Cross(vPrevEdgeDir).Dot(pPolygon->GetPlane().Normal()) < 0)
							vInclinedDir = -vInclinedDir;
						vUpdatedPos[0] = prevEdge.m_v[1] + vInclinedDir * (m_fDelta / std::sin(fTheta));
						pPolygon->SetPos(originalEdgeIndexPair.m_i[0], vUpdatedPos[0]);
					}

					BrushEdge3D nextEdge = m_OriginalPolygons[i]->GetEdge(nNextEdgeIndex);
					if (edgeSetMatchingSelectedElement.find(nNextEdgeIndex) == edgeSetMatchingSelectedElement.end())
					{
						BrushVec3 vNextEdgeDir = (nextEdge.m_v[1] - nextEdge.m_v[0]).GetNormalized();
						if (GetEdgeCountHavingVertexInElementList(nextEdge.m_v[0], m_OriginalSelectedElements) == 1)
						{
							BrushFloat fTheta = std::acos(vNextEdgeDir.Dot(vOriginalEdgeDir));
							vUpdatedPos[1] = nextEdge.m_v[0] + vNextEdgeDir * (m_fDelta / std::sin(fTheta));
						}
						else
						{
							vUpdatedPos[1] = nextEdge.m_v[0] + vNextEdgeDir * m_fDelta;
						}
						pPolygon->SetPos(originalEdgeIndexPair.m_i[1], vUpdatedPos[1]);
					}

					for (int k = 0, iElementSize(m_OriginalSelectedElements.GetCount()); k < iElementSize; ++k)
					{
						if (Comparison::IsEquivalent(originalEdge.m_v[0], m_OriginalSelectedElements[k].m_Vertices[0]))
							outMappingInfo.mapSpreadedVertex2Apex.push_back(std::pair<BrushVec3, BrushVec3>(vUpdatedPos[0], m_OriginalSelectedElements[k].m_Vertices[0]));
						else if (Comparison::IsEquivalent(originalEdge.m_v[1], m_OriginalSelectedElements[k].m_Vertices[0]))
							outMappingInfo.mapSpreadedVertex2Apex.push_back(std::pair<BrushVec3, BrushVec3>(vUpdatedPos[1], m_OriginalSelectedElements[k].m_Vertices[0]));

						if (Comparison::IsEquivalent(originalEdge.m_v[1], m_OriginalSelectedElements[k].m_Vertices[1]))
							outMappingInfo.mapSpreadedVertex2Apex.push_back(std::pair<BrushVec3, BrushVec3>(vUpdatedPos[1], m_OriginalSelectedElements[k].m_Vertices[1]));
						else if (Comparison::IsEquivalent(originalEdge.m_v[0], m_OriginalSelectedElements[k].m_Vertices[1]))
							outMappingInfo.mapSpreadedVertex2Apex.push_back(std::pair<BrushVec3, BrushVec3>(vUpdatedPos[0], m_OriginalSelectedElements[k].m_Vertices[1]));
					}
				}
			}

			for (iEdgeMatchingSelectedElement = edgeSetMatchingSelectedElement.begin(); iEdgeMatchingSelectedElement != edgeSetMatchingSelectedElement.end(); ++iEdgeMatchingSelectedElement)
			{
				BrushEdge3D edgeMatchingSelectedElement = pPolygon->GetEdge(*iEdgeMatchingSelectedElement);
				outMappingInfo.mapElementIdx2Edges[mapBetweenEdgeIndexAndElement[*iEdgeMatchingSelectedElement]].push_back(edgeMatchingSelectedElement.GetInverted());
				outMappingInfo.mapElementIdx2OriginalPolygon[mapBetweenEdgeIndexAndElement[*iEdgeMatchingSelectedElement]] = m_OriginalPolygons[i];
			}

			pPolygon->Optimize();

			if (!IsPolygonValid(pPolygon, pOriginalPolygon))
				return false;
		}

		for (int k = 0, iElementSize(m_OriginalSelectedElements.GetCount()); k < iElementSize; ++k)
		{
			BrushEdge3D edge(m_OriginalSelectedElements[k].m_Vertices[0], m_OriginalSelectedElements[k].m_Vertices[1]);
			int nVertexIndex = -1;
			bool bSuccess0 = pPolygon->HasPosition(edge.m_v[0], &nVertexIndex);
			bool bSuccess1 = pPolygon->HasPosition(edge.m_v[1], &nVertexIndex);
			if (bSuccess0 == bSuccess1)
				continue;
			vertices.push_back(nVertexIndex);
			selectedElements.push_back(k);
			outResultForNextPhase.middlePhaseSidePolygons.push_back(pPolygon);
		}
	}

	for (int i = 0, iSize(outResultForNextPhase.middlePhaseSidePolygons.size()); i < iSize; ++i)
	{
		PolygonPtr pPolygon = outResultForNextPhase.middlePhaseSidePolygons[i];

		bool bTouched = false;
		for (int k = 0; k < iSize; ++k)
		{
			if (i == k || pPolygon == outResultForNextPhase.middlePhaseSidePolygons[k] || selectedElements[i] != selectedElements[k])
				continue;

			if (pPolygon->HasOverlappedEdges(outResultForNextPhase.middlePhaseSidePolygons[k]))
			{
				bTouched = true;
				break;
			}
		}

		if (!bTouched)
		{
			if (GetEdgeCountHavingVertexInElementList(pPolygon->GetPos(vertices[i]), m_OriginalSelectedElements) == 1)
			{
				BrushEdge3D baseEdge(m_OriginalSelectedElements[selectedElements[i]].m_Vertices[0], m_OriginalSelectedElements[selectedElements[i]].m_Vertices[1]);
				pPolygon->BroadenVertex(m_fDelta, vertices[i], &baseEdge);
			}
			else
			{
				pPolygon->BroadenVertex(m_fDelta, vertices[i], NULL);
			}
		}
		else
		{
			outMappingInfo.vertexSetToMakePolygon.insert(pPolygon->GetPos(vertices[i]));
		}

		if (HasCrossEdges(pPolygon))
			return false;
	}

	for (int i = 0, iUpdatedPolygonCount(updatedPolygons.size()); i < iUpdatedPolygonCount; ++i)
		GetModel()->AddPolygon(updatedPolygons[i]);

	return true;
}

void BevelTool::PP1_MakeEdgePolygons(const MappingInfo& mappingInfo, ResultForNextPhase& outResultForNextPhase)
{
	auto ii = mappingInfo.mapElementIdx2Edges.begin();
	for (; ii != mappingInfo.mapElementIdx2Edges.end(); ++ii)
	{
		std::vector<BrushEdge3D> edges = ii->second;
		if (edges.size() == 2)
		{
			edges.push_back(BrushEdge3D(edges[0].m_v[1], edges[1].m_v[0]));
			edges.push_back(BrushEdge3D(edges[1].m_v[1], edges[0].m_v[0]));
		}

		if (edges.size() != 4)
			continue;

		std::set<int> usedEdges;
		std::vector<BrushVec3> vList;

		int nIndex = 0;
		while (vList.size() < 4)
		{
			bool bFound = false;
			for (int i = 0; i < 4; ++i)
			{
				if (i == nIndex)
					continue;
				if (Comparison::IsEquivalent(edges[nIndex].m_v[1], edges[i].m_v[0]))
				{
					nIndex = i;
					vList.push_back(edges[i].m_v[0]);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				DESIGNER_ASSERT(0);
				vList.clear();
				break;
			}
		}
		if (vList.size() == 4)
		{
			if (GetDifferentVerticesCount(vList) == 4)
			{
				BrushPlane plane(vList[0], vList[1], vList[2]);
				PolygonPtr pEdgePolygon = new Polygon(vList, plane, 0, NULL, true);
				auto iOriginalPolygon = mappingInfo.mapElementIdx2OriginalPolygon.find(ii->first);
				if (iOriginalPolygon != mappingInfo.mapElementIdx2OriginalPolygon.end())
				{
					pEdgePolygon->SetTexInfo(iOriginalPolygon->second->GetTexInfo());
					pEdgePolygon->SetSubMatID(iOriginalPolygon->second->GetSubMatID());
				}
				outResultForNextPhase.middlePhaseEdgePolygons.push_back(pEdgePolygon);
				GetModel()->AddPolygon(pEdgePolygon, false);

				PP2_MapBetweenEdgeIdToApexPos(mappingInfo, pEdgePolygon, edges[2], edges[3], outResultForNextPhase);
			}
		}
	}
}

void BevelTool::PP2_MapBetweenEdgeIdToApexPos(const MappingInfo& mappingInfo,
                                              PolygonPtr pEdgePolygon,
                                              const BrushEdge3D& sideEdge0,
                                              const BrushEdge3D& sideEdge1,
                                              ResultForNextPhase& outResultForNextPhase)
{
	for (int a = 0, iEdgeSize(pEdgePolygon->GetEdgeCount()); a < iEdgeSize; ++a)
	{
		BrushEdge3D edge = pEdgePolygon->GetEdge(a);
		if (!sideEdge0.IsEquivalent(edge) && !sideEdge1.IsEquivalent(edge))
			continue;

		const std::pair<BrushVec3, BrushVec3>* iApex = NULL;
		for (int b = 0, iVertex2ApexCount(mappingInfo.mapSpreadedVertex2Apex.size()); b < iVertex2ApexCount; ++b)
		{
			if (Comparison::IsEquivalent(mappingInfo.mapSpreadedVertex2Apex[b].first, edge.m_v[0]))
			{
				iApex = &mappingInfo.mapSpreadedVertex2Apex[b];
				break;
			}
		}

		if (iApex == NULL)
		{
			DESIGNER_ASSERT(0 && "iApex == NULL");
			continue;
		}

		BrushVec3 apexPos = iApex->second;
		BrushFloat fDistance = 0;

		int nCount = GetEdgeCountHavingVertexInElementList(apexPos, m_OriginalSelectedElements);
		if (nCount < 3)
		{
			outResultForNextPhase.mapBetweenEdgeIdToApex[EdgeIdentifier(pEdgePolygon, a)] = apexPos;
			if (mappingInfo.vertexSetToMakePolygon.find(apexPos) != mappingInfo.vertexSetToMakePolygon.end())
			{
				std::vector<BrushVec3> vList;
				vList.push_back(apexPos);
				vList.push_back(edge.m_v[1]);
				vList.push_back(edge.m_v[0]);
				BrushPlane plane(vList[0], vList[1], vList[2]);
				PolygonPtr pApexPolygon = new Polygon(vList, plane, pEdgePolygon->GetSubMatID(), &(pEdgePolygon->GetTexInfo()), true);
				GetModel()->AddPolygon(pApexPolygon);
				outResultForNextPhase.middlePhaseBottomPolygons.push_back(pApexPolygon);

				outResultForNextPhase.mapBetweenEdgeIdToVertex[EdgeIdentifier(pEdgePolygon, a)] = apexPos;
			}
		}
		else if (pEdgePolygon->GetPlane().HitTest(BrushRay(apexPos, -pEdgePolygon->GetPlane().Normal()), &fDistance, NULL))
		{
			outResultForNextPhase.mapBetweenEdgeIdToApex[EdgeIdentifier(pEdgePolygon, a)] = edge.GetCenter() + pEdgePolygon->GetPlane().Normal() * fDistance;
		}
	}
}

void BevelTool::PP1_MakeApexPolygons(const MappingInfo& mappingInfo, ResultForNextPhase& outResultForNextPhase)
{
	std::vector<BrushEdge3D> apexEdges;
	auto ii = mappingInfo.mapElementIdx2Edges.begin();
	std::vector<PolygonPtr> corrspondingPolygons;
	for (; ii != mappingInfo.mapElementIdx2Edges.end(); ++ii)
	{
		std::vector<BrushEdge3D> edges = ii->second;
		if (edges.size() == 2)
		{
			apexEdges.push_back(BrushEdge3D(edges[0].m_v[0], edges[1].m_v[1]));
			apexEdges.push_back(BrushEdge3D(edges[1].m_v[0], edges[0].m_v[1]));
			auto iOriginalPolygon = mappingInfo.mapElementIdx2OriginalPolygon.find(ii->first);
			if (iOriginalPolygon != mappingInfo.mapElementIdx2OriginalPolygon.end())
			{
				corrspondingPolygons.push_back(iOriginalPolygon->second);
				corrspondingPolygons.push_back(iOriginalPolygon->second);
			}
		}
	}

	if (apexEdges.size() < 3)
		return;

	std::set<int> usedEdges;
	int nIndex = -1;
	std::vector<BrushVec3> vList;
	bool bFoundNextEdge = false;
	bool bFoundApexPolygon = false;
	int iApexEdgeSize(apexEdges.size());

	while (usedEdges.size() < iApexEdgeSize || usedEdges.size() == iApexEdgeSize && bFoundApexPolygon)
	{
		if (bFoundApexPolygon)
		{
			DESIGNER_ASSERT(vList.size() >= 3);
			if (GetDifferentVerticesCount(vList) >= 3)
			{
				BrushPlane plane(vList[0], vList[1], vList[2]);
				PolygonPtr pApexPolygon = new Polygon(vList, plane, 0, NULL, true);
				if (nIndex != -1)
				{
					pApexPolygon->SetTexInfo(corrspondingPolygons[nIndex]->GetTexInfo());
					pApexPolygon->SetSubMatID(corrspondingPolygons[nIndex]->GetSubMatID());
				}
				outResultForNextPhase.middlePhaseApexPolygons.push_back(pApexPolygon);
				GetModel()->AddPolygon(pApexPolygon);
				vList.clear();
				nIndex = -1;
				if (usedEdges.size() == iApexEdgeSize)
					break;
			}
			else
			{
				break;
			}
		}
		else if (!bFoundNextEdge)
		{
			vList.clear();
			if (nIndex != -1)
				usedEdges.insert(nIndex);
			nIndex = -1;
		}

		if (nIndex == -1)
		{
			for (int i = 0; i < iApexEdgeSize; ++i)
			{
				if (usedEdges.find(i) == usedEdges.end())
				{
					nIndex = i;
					break;
				}
			}
		}

		if (nIndex == -1)
			break;

		vList.push_back(apexEdges[nIndex].m_v[0]);

		bFoundNextEdge = false;
		bFoundApexPolygon = false;

		for (int i = 0, iApexEdgeSize(apexEdges.size()); i < iApexEdgeSize; ++i)
		{
			if (i == nIndex)
				continue;

			bool bEdgeConnected = Comparison::IsEquivalent(apexEdges[nIndex].m_v[1], apexEdges[i].m_v[0]);
			if (!bEdgeConnected)
				continue;

			if (vList.size() >= 3 && Comparison::IsEquivalent(vList[0], apexEdges[i].m_v[0]))
			{
				usedEdges.insert(nIndex);
				bFoundApexPolygon = true;
				break;
			}

			if (usedEdges.find(i) != usedEdges.end())
				continue;

			usedEdges.insert(nIndex);
			nIndex = i;
			bFoundNextEdge = true;
			break;
		}
	}
}

void BevelTool::PP0_SpreadEdges(int offset, bool bSpreadEdge)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	BrushFloat fPrevDelta = m_fDelta;
	if (bSpreadEdge)
	{
		m_fDelta -= (float)offset * 0.01f;
		if (m_fDelta <= 0)
			m_fDelta = 0;
	}
	else
	{
		m_fDelta = 0;
	}

	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	m_ResultForSecondPhase.Reset();
	MappingInfo mappingInfo;
	int nCount = 0;
	while (!PP1_PushEdgesAndVerticesOut(m_ResultForSecondPhase, mappingInfo) && ++nCount < 100)
	{
		m_fDelta = (m_fDelta + fPrevDelta) * (BrushFloat)0.5;
		mappingInfo.Reset();
		m_ResultForSecondPhase.Reset();
	}

	DESIGNER_ASSERT(nCount < 100);

	PP1_MakeEdgePolygons(mappingInfo, m_ResultForSecondPhase);
	PP1_MakeApexPolygons(mappingInfo, m_ResultForSecondPhase);

	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);
}

void BevelTool::PP0_SubdivideSpreadedEdge(int nSubdivideNum)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	ResultForNextPhase& rp = m_ResultForSecondPhase;

	if (nSubdivideNum == 1)
	{
		for (int i = 0, iSize(rp.middlePhaseEdgePolygons.size()); i < iSize; ++i)
			GetModel()->AddPolygon(rp.middlePhaseEdgePolygons[i]);

		for (int i = 0, iSidePolygonSize(rp.middlePhaseSidePolygons.size()); i < iSidePolygonSize; ++i)
		{
			if (GetModel()->QueryEquivalentPolygon(rp.middlePhaseSidePolygons[i]))
				continue;
			GetModel()->AddPolygon(rp.middlePhaseSidePolygons[i]);
		}

		for (int i = 0, iFlatPolygonSize(rp.middlePhaseBottomPolygons.size()); i < iFlatPolygonSize; ++i)
			GetModel()->AddPolygon(rp.middlePhaseBottomPolygons[i]);

		ApplyPostProcess(ePostProcess_Mirror);
		CompileShelf(eShelf_Construction);
		return;
	}

	std::vector<InfoForSubdivingApexPolygon> infoForSubdividingApexPolygonList;

	std::vector<PolygonPtr> middleSidePolygons;
	for (int i = 0, iMiddleSidePolygonSize(rp.middlePhaseSidePolygons.size()); i < iMiddleSidePolygonSize; ++i)
	{
		if (GetModel()->QueryEquivalentPolygon(rp.middlePhaseSidePolygons[i]))
			continue;
		PolygonPtr pMiddleSidePolygon = rp.middlePhaseSidePolygons[i]->Clone();
		middleSidePolygons.push_back(pMiddleSidePolygon);
		GetModel()->AddPolygon(pMiddleSidePolygon);
	}

	for (int k = 0, iPolygonSize(rp.middlePhaseEdgePolygons.size()); k < iPolygonSize; ++k)
	{
		std::vector<BrushVec2> sidePoints[2];
		std::vector<BrushVec3> sideVertices[2];
		int nCount = 0;
		PolygonPtr pEdgePolygon = rp.middlePhaseEdgePolygons[k];
		for (int i = 0; i < 4; ++i)
		{
			EdgeIdentifier edgeId(pEdgePolygon, i);
			if (rp.mapBetweenEdgeIdToApex.find(edgeId) == rp.mapBetweenEdgeIdToApex.end())
				continue;

			DESIGNER_ASSERT(nCount < 2);

			int nEdgeIndex = -1;
			BrushEdge3D sideEdge = pEdgePolygon->GetEdge(i);

			BrushVec3 vApexPos = rp.mapBetweenEdgeIdToApex[edgeId];
			BrushPlane sidePlane(sideEdge.m_v[0], sideEdge.m_v[1], vApexPos);

			if (nCount == 1)
				sideEdge.Invert();

			BrushVec3 vDir = (vApexPos - sideEdge.GetCenter()).GetNormalized();
			PolygonPtr pSidePolygon;
			for (int a = 0, iSidePolygonSize(middleSidePolygons.size()); a < iSidePolygonSize; ++a)
			{
				if (middleSidePolygons[a]->GetPlane().IsEquivalent(sidePlane) || middleSidePolygons[a]->GetPlane().IsEquivalent(sidePlane.GetInverted()))
				{
					pSidePolygon = middleSidePolygons[a];
					break;
				}
			}

			vApexPos = sideEdge.GetCenter() + 0.35f * (vApexPos - sideEdge.GetCenter());

			BrushVec2 vOutsideVtx = sidePlane.W2P(vApexPos);
			BrushVec2 vBaseVtx0 = sidePlane.W2P(sideEdge.m_v[0]);
			BrushVec2 vBaseVtx1 = sidePlane.W2P(sideEdge.m_v[1]);
			if (MakeListConsistingOfArc(vOutsideVtx, vBaseVtx0, vBaseVtx1, nSubdivideNum + 1, sidePoints[nCount]))
			{
				sideVertices[nCount].push_back(sideEdge.m_v[1]);
				for (int a = 1, iSize(sidePoints[nCount].size()); a < iSize; ++a)
				{
					BrushVec3 vertex = sidePlane.P2W(sidePoints[nCount][a]);
					sideVertices[nCount].push_back(vertex);
					if (pSidePolygon)
					{
						pSidePolygon->InsertVertex(vertex);
						pSidePolygon->ResetUVs();
					}
				}
				sideVertices[nCount][sideVertices[nCount].size() - 1] = sideEdge.m_v[0];

				if (rp.mapBetweenEdgeIdToVertex.find(edgeId) != rp.mapBetweenEdgeIdToVertex.end())
				{
					std::vector<BrushVec3> vList;
					vList.push_back(rp.mapBetweenEdgeIdToVertex[edgeId]);
					for (int a = 0, iSideVertexCount(sideVertices[nCount].size()); a < iSideVertexCount; ++a)
					{
						if (nCount == 0)
							vList.push_back(sideVertices[nCount][a]);
						else
							vList.push_back(sideVertices[nCount][iSideVertexCount - a - 1]);
					}
					if (vList.size() >= 3)
					{
						BrushPlane plane(vList[0], vList[1], vList[2]);
						PolygonPtr pPolygon = new Polygon(vList, plane, pEdgePolygon->GetSubMatID(), &(pEdgePolygon->GetTexInfo()), true);
						GetModel()->AddPolygon(pPolygon);
					}
				}
			}
			++nCount;
		}

		DESIGNER_ASSERT(nCount == 2);
		DESIGNER_ASSERT(sideVertices[0].size() == sideVertices[1].size());
		DESIGNER_ASSERT(!sideVertices[0].empty());

		if (nCount == 2 && !sideVertices[0].empty() && sideVertices[0].size() == sideVertices[1].size())
		{
			InfoForSubdivingApexPolygon isar0;
			InfoForSubdivingApexPolygon isar1;

			isar0.edge = BrushEdge3D(sideVertices[0][0], sideVertices[0][sideVertices[0].size() - 1]);
			isar1.edge = BrushEdge3D(sideVertices[1][sideVertices[1].size() - 1], sideVertices[1][0]);

			int nValidDivideNum = sideVertices[0].size();
			for (int i = 0; i < nValidDivideNum - 1; ++i)
			{
				std::vector<BrushVec3> vList;
				vList.push_back(sideVertices[0][i]);
				vList.push_back(sideVertices[1][i]);
				vList.push_back(sideVertices[1][i + 1]);
				vList.push_back(sideVertices[0][i + 1]);

				BrushPlane plane(sideVertices[0][i], sideVertices[1][i], sideVertices[1][i + 1]);

				isar0.vIntermediate.push_back(std::pair<BrushVec3, BrushVec3>(sideVertices[0][i], plane.Normal()));
				isar1.vIntermediate.push_back(std::pair<BrushVec3, BrushVec3>(sideVertices[1][nValidDivideNum - i - 1], plane.Normal()));

				PolygonPtr pDividedPolygon = new Polygon(vList, plane, pEdgePolygon->GetSubMatID(), &(pEdgePolygon->GetTexInfo()), true);
				GetModel()->AddPolygon(pDividedPolygon);
			}

			isar0.vIntermediate.push_back(std::pair<BrushVec3, BrushVec3>(sideVertices[0][nValidDivideNum - 1], isar0.vIntermediate[isar0.vIntermediate.size() - 1].second));
			isar1.vIntermediate.push_back(std::pair<BrushVec3, BrushVec3>(sideVertices[1][0], isar1.vIntermediate[isar1.vIntermediate.size() - 1].second));

			infoForSubdividingApexPolygonList.push_back(isar0);
			infoForSubdividingApexPolygonList.push_back(isar1);
		}
	}

	PP1_SubdivideApexPolygon(nSubdivideNum, infoForSubdividingApexPolygonList);

	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);
}

int BevelTool::FindCorrespondingEdge(const BrushEdge3D& e, const std::vector<InfoForSubdivingApexPolygon>& infoForSubdividingApexPolygonList) const
{
	for (int i = 0, iCount(infoForSubdividingApexPolygonList.size()); i < iCount; ++i)
	{
		if (infoForSubdividingApexPolygonList[i].edge.IsEquivalent(e))
			return i;
	}
	return -1;
}

void BevelTool::PP1_SubdivideApexPolygon(int nSubdivideNum, const std::vector<InfoForSubdivingApexPolygon>& infoForSubdividingApexPolygonList)
{
	ResultForNextPhase& rp = m_ResultForSecondPhase;

	for (int i = 0, iCount(rp.middlePhaseApexPolygons.size()); i < iCount; ++i)
	{
		PolygonPtr pPolygon = rp.middlePhaseApexPolygons[i];

		std::vector<Vertex> vList;
		pPolygon->GetLinkedVertices(vList);

		std::vector<const InfoForSubdivingApexPolygon*> sortedInfos;

		for (int k = 0, iVListCount(vList.size()); k < iVListCount; ++k)
		{
			BrushEdge3D e(vList[k].pos, vList[(k + 1) % iVListCount].pos);
			int nCorrespondingEdgeIndex = FindCorrespondingEdge(e, infoForSubdividingApexPolygonList);
			DESIGNER_ASSERT(nCorrespondingEdgeIndex != -1);
			if (nCorrespondingEdgeIndex == -1)
				continue;
			sortedInfos.push_back(&infoForSubdividingApexPolygonList[nCorrespondingEdgeIndex]);
		}

		std::vector<PolygonPtr> initialPolygons;
		if ((nSubdivideNum % 2) == 0)
			initialPolygons = CreateFirstOddSubdividedApexPolygons(sortedInfos);
		else if (sortedInfos.size() >= 3)
			initialPolygons = CreateFirstEvenSubdividedApexPolygons(sortedInfos);

		if (!initialPolygons.empty())
		{
			GetModel()->RemovePolygon(pPolygon);
			for (int k = 0, iInitialPolygonCount(initialPolygons.size()); k < iInitialPolygonCount; ++k)
				GetModel()->AddPolygon(initialPolygons[k]);
		}
	}
}

std::vector<PolygonPtr> BevelTool::CreateFirstOddSubdividedApexPolygons(const std::vector<const InfoForSubdivingApexPolygon*>& subdividedEdges)
{
	std::vector<PolygonPtr> subdividedPolygons;

	int nSubdivisionCount = subdividedEdges[0]->vIntermediate.size();
	int nMiddleVertexIndex = nSubdivisionCount / 2;

	BrushVec3 vCenter(0, 0, 0);
	int nSubdividedEdgeCount(subdividedEdges.size());
	for (int i = 0; i < nSubdividedEdgeCount; ++i)
		vCenter += subdividedEdges[i]->vIntermediate[nMiddleVertexIndex].first;
	vCenter /= nSubdividedEdgeCount;

	for (int i = 0; i < nSubdividedEdgeCount; ++i)
	{
		std::vector<BrushVec3> vList(4);
		vList[0] = subdividedEdges[i]->vIntermediate[nMiddleVertexIndex].first;
		vList[1] = subdividedEdges[i]->edge.m_v[1];
		vList[2] = subdividedEdges[(i + 1) % nSubdividedEdgeCount]->vIntermediate[nMiddleVertexIndex].first;
		if (i == 0)
		{
			BrushPlane p(vList[0], vList[1], vList[2]);
			p.HitTest(BrushRay(vCenter, p.Normal()), NULL, &vCenter);
		}
		vList[3] = vCenter;
		subdividedPolygons.push_back(new Polygon(vList));
	}

	return subdividedPolygons;
}

std::vector<PolygonPtr> BevelTool::CreateFirstEvenSubdividedApexPolygons(const std::vector<const InfoForSubdivingApexPolygon*>& subdividedEdges)
{
	std::vector<PolygonPtr> subdividedPolygons;

	BrushPlane p(subdividedEdges[0]->vIntermediate[1].first, subdividedEdges[1]->vIntermediate[1].first, subdividedEdges[2]->vIntermediate[1].first);

	BrushFloat fFirstEdgeDistance = subdividedEdges[0]->vIntermediate[0].first.GetDistance(subdividedEdges[0]->vIntermediate[1].first);
	std::vector<BrushVec3> vList;
	for (int i = 0, iEdgeCount(subdividedEdges.size()); i < iEdgeCount; ++i)
	{
		int nPrevIndex = i == 0 ? iEdgeCount - 1 : i - 1;
		int nCurrIndex = i;

		const BrushVec3& vPrev = subdividedEdges[nPrevIndex]->edge.m_v[0];
		BrushVec3 vCurr = subdividedEdges[nCurrIndex]->edge.m_v[0];
		const BrushVec3& vNext = subdividedEdges[nCurrIndex]->edge.m_v[1];

		BrushVec3 vDir = ((vPrev - vCurr).GetNormalized() + (vNext - vCurr).GetNormalized()).GetNormalized();

		vCurr += vDir * fFirstEdgeDistance;
		p.HitTest(BrushRay(vCurr, p.Normal()), NULL, &vCurr);
		vList.push_back(vCurr);
	}

	PolygonPtr pPolygon = new Polygon(vList);
	for (int i = 0, iEdgeCount(subdividedEdges.size()); i < iEdgeCount; ++i)
	{
		BrushEdge3D e = pPolygon->GetEdge(i);

		vList.clear();
		vList.push_back(subdividedEdges[i]->vIntermediate[1].first);
		vList.push_back(subdividedEdges[i]->vIntermediate[subdividedEdges[i]->vIntermediate.size() - 2].first);
		vList.push_back(e.m_v[1]);
		vList.push_back(e.m_v[0]);
		subdividedPolygons.push_back(new Polygon(vList));

		int nPrevIndex = i == 0 ? iEdgeCount - 1 : i - 1;
		vList.clear();
		vList.push_back(subdividedEdges[i]->vIntermediate[0].first);
		vList.push_back(subdividedEdges[i]->vIntermediate[1].first);
		vList.push_back(e.m_v[0]);
		vList.push_back(subdividedEdges[nPrevIndex]->vIntermediate[subdividedEdges[nPrevIndex]->vIntermediate.size() - 2].first);
		subdividedPolygons.push_back(new Polygon(vList));
	}

	subdividedPolygons.push_back(pPolygon);

	return subdividedPolygons;
}

bool BevelTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_BevelMode != eBevelMode_Nothing)
	{
		int offset = point.y - m_nMousePrevY;
		if (m_BevelMode == eBevelMode_Spread)
			PP0_SpreadEdges(offset);
		else if (m_BevelMode == eBevelMode_Divide)
		{
			if (offset > 0) ++m_nDividedNumber;
			if (offset < 0) --m_nDividedNumber;
			if (m_nDividedNumber >= 20)
				m_nDividedNumber = 20;
			if (m_nDividedNumber <= 1)
				m_nDividedNumber = 1;
			PP0_SubdivideSpreadedEdge(m_nDividedNumber);
		}
		m_nMousePrevY = point.y;
	}

	return true;
}

bool BevelTool::OnKeyDown(CViewport* view, uint32 nKeycode, uint32 nRepCnt, uint32 nFlags)
{
	if (nKeycode == Qt::Key_Escape)
	{
		if (m_BevelMode == eBevelMode_Nothing || m_BevelMode == eBevelMode_Done)
		{
			return GetDesigner()->EndCurrentDesignerTool();
		}
		else if (m_BevelMode == eBevelMode_Spread || m_BevelMode == eBevelMode_Divide)
		{
			if (m_pOriginalModel)
			{
				(*GetModel()) = *m_pOriginalModel;
				ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SmoothingGroup);
			}
			return GetDesigner()->EndCurrentDesignerTool();
		}
	}

	return true;
}

int BevelTool::GetEdgeCountHavingVertexInElementList(const BrushVec3& vertex, const ElementSet& elementList) const
{
	int nCount = 0;
	for (int i = 0, iElementCount(elementList.GetCount()); i < iElementCount; ++i)
	{
		const Element& elementInfo = elementList.Get(i);
		for (int k = 0, iVertexCount(elementInfo.m_Vertices.size()); k < iVertexCount; ++k)
		{
			if (Comparison::IsEquivalent(elementInfo.m_Vertices[k], vertex))
				++nCount;
		}
	}
	return nCount;
}

void BevelTool::Display(DisplayContext& dc)
{
	BaseTool::Display(dc);
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Display(GetBaseObject(), dc);
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Bevel, eToolGroup_Modify, "Bevel", BevelTool,
                                   bevel, "runs bevel tool", "designer.bevel")

