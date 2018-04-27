// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovePipeline.h"
#include "Core/Model.h"
#include "Core/PolygonDecomposer.h"
#include "Core/Helper.h"
#include "Core/ModelCompiler.h"
#include "DesignerEditor.h"
#include "Core/UVIslandManager.h"
#include "Util/ElementSet.h"

namespace Designer
{
void MovePipeline::TransformSelections(MainContext& mc, const BrushMatrix34& offsetTM)
{
	ComputeIntermediatePositionsBasedOnInitQueryResults(offsetTM);
	CreateOrganizedResultsAroundPolygonFromQueryResults();

	if (!ExcutedAdditionPass())
	{
		if (VertexAdditionFirstPass())
		{
			ComputeIntermediatePositionsBasedOnInitQueryResults(offsetTM);
			CreateOrganizedResultsAroundPolygonFromQueryResults();
		}

		if (VertexAdditionSecondPass())
		{
			ComputeIntermediatePositionsBasedOnInitQueryResults(offsetTM);
			CreateOrganizedResultsAroundPolygonFromQueryResults();
		}

		SetExcutedAdditionPass(true);
	}

	if (SubdivisionPass())
	{
		ApplyPostProcess(mc, ePostProcess_DataBase);
		SetQueryResultsFromSelectedElements(*mc.pSelected);
		ComputeIntermediatePositionsBasedOnInitQueryResults(offsetTM);
		CreateOrganizedResultsAroundPolygonFromQueryResults();
	}

	TransformationPass();

	mc.pSelected->Set(m_InitSelection);
	mc.pSelected->Transform(offsetTM);
}

void MovePipeline::SetQueryResultsFromSelectedElements(const ElementSet& selectedElements)
{
	if (selectedElements.IsEmpty())
		return;
	m_QueryResult = selectedElements.QueryFromElements(GetModel());
	DESIGNER_ASSERT(!m_QueryResult.empty());
}

void MovePipeline::CreateOrganizedResultsAroundPolygonFromQueryResults()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	m_OrganizedQueryResult = SelectTool::CreateOrganizedResultsAroundPolygonFromQueryResults(m_QueryResult);
}

void MovePipeline::ComputeIntermediatePositionsBasedOnInitQueryResults(const BrushMatrix34& offsetTM)
{
	m_IntermediateTransQueryPos.clear();

	int iQueryResultSize(m_InitQueryResult.size());
	m_IntermediateTransQueryPos.reserve(iQueryResultSize);

	DESIGNER_ASSERT(iQueryResultSize);

	for (int i = 0; i < iQueryResultSize; ++i)
	{
		const ModelDB::Vertex& v = m_InitQueryResult[i];
		m_IntermediateTransQueryPos.push_back(offsetTM.TransformPoint(v.m_Pos));
	}

	SnappedToMirrorPlane();
}

bool MovePipeline::VertexAdditionFirstPass()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	std::vector<ModelDB::Vertex> newVertices;
	for (int i = 0, iQuerySize(m_InitQueryResult.size()); i < iQuerySize; ++i)
	{
		ModelDB::Vertex& v = m_InitQueryResult[i];
		int vertexMarkListSize(v.m_MarkList.size());

		for (int k = 0; k < vertexMarkListSize; ++k)
		{
			ModelDB::Mark& mark = v.m_MarkList[k];
			PolygonPtr pPolygon = mark.m_pPolygon;
			if (pPolygon == NULL)
				continue;

			BrushVec3 nextVertex;
			if (!pPolygon->GetNextVertex(mark.m_VertexIndex, nextVertex))
				continue;

			BrushVec3 prevVertex;
			if (!pPolygon->GetPrevVertex(mark.m_VertexIndex, prevVertex))
				continue;

			BrushVec3 nextPrevVertices[2] = { nextVertex, prevVertex };
			bool bValid[2] = { true, true };
			for (int b = 0; b < 2; ++b)
			{
				for (int a = 0; a < iQuerySize; ++a)
				{
					if (a == i)
						continue;

					if (Comparison::IsEquivalent(m_QueryResult[a].m_Pos, nextPrevVertices[b]))
					{
						bValid[b] = false;
						break;
					}
				}
			}

			ModelDB::QueryResult qResult[2];
			if (!GetModel()->GetDB()->QueryAsVertex(nextVertex, qResult[0]))
				continue;
			if (!GetModel()->GetDB()->QueryAsVertex(prevVertex, qResult[1]))
				continue;

			for (int a = 0; a < 2; ++a)
			{
				if (bValid[a] == false)
					continue;

				if (qResult[a].size() != 1)
					continue;

				GetModel()->SetShelf(eShelf_Construction);
				int nAdjacentPolygonIndex(-1);
				PolygonPtr pAdjacentPolygon = FindAdjacentPolygon(pPolygon, nextPrevVertices[a], nAdjacentPolygonIndex);
				if (!pAdjacentPolygon)
					continue;

				ModelDB::Mark newMark;
				newMark.m_VertexIndex = pAdjacentPolygon->GetVertexCount();
				newMark.m_pPolygon = GetModel()->GetPolygon(nAdjacentPolygonIndex);

				bool bExistVertex(false);
				for (int b = 0, iNewVertexSize(newVertices.size()); b < iNewVertexSize; ++b)
				{
					if (Comparison::IsEquivalent(newVertices[b].m_Pos, nextPrevVertices[a]))
					{
						bExistVertex = true;
						newVertices[b].m_MarkList.push_back(newMark);
						break;
					}
				}

				if (!bExistVertex)
				{
					ModelDB::Vertex v;
					v.m_Pos = nextPrevVertices[a];
					v.m_MarkList.push_back(newMark);
					newVertices.push_back(v);
				}
			}
		}
	}

	for (int i = 0, iNewVertexSize(newVertices.size()); i < iNewVertexSize; ++i)
	{
		ModelDB::Vertex& v = newVertices[i];
		for (int k = 0, iMarkListSize(v.m_MarkList.size()); k < iMarkListSize; ++k)
		{
			Polygon* pPolygon = v.m_MarkList[k].m_pPolygon;
			if (pPolygon == NULL)
				continue;

			if (pPolygon->InsertVertex(v.m_Pos, &v.m_MarkList[k].m_VertexIndex, NULL))
				GetModel()->GetDB()->AddMarkToVertex(v.m_Pos, v.m_MarkList[k]);
		}
	}

	return !newVertices.empty();
}
bool MovePipeline::VertexAdditionSecondPass()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	bool bAdded = false;

	for (int i = 0, iQuerySize(m_InitQueryResult.size()); i < iQuerySize; ++i)
	{
		ModelDB::Vertex& v = m_InitQueryResult[i];
		int markListSize(v.m_MarkList.size());

		if (markListSize > 2)
			continue;

		PolygonPtr pPolygon = v.m_MarkList[0].m_pPolygon;
		if (pPolygon == NULL)
			continue;

		for (int k = eShelf_Base; k < cShelfMax; ++k)
		{
			GetModel()->SetShelf(static_cast<ShelfID>(k));
			int nAdjacentPolygonIndex(-1);
			PolygonPtr pAdjacentPolygon = FindAdjacentPolygon(pPolygon, v.m_Pos, nAdjacentPolygonIndex);
			if (pAdjacentPolygon)
			{
				PolygonPtr pOldAdjacentPolygon = pAdjacentPolygon->Clone();
				ModelDB::Mark newMark;
				if (!pAdjacentPolygon->InsertVertex(v.m_Pos, &newMark.m_VertexIndex, NULL))
					continue;
				if (k != 1)
					GetModel()->RemovePolygon(nAdjacentPolygonIndex);
				MirrorUtil::RemoveMirroredPolygon(GetModel(), pOldAdjacentPolygon);

				if (k != 1)
				{
					GetModel()->SetShelf(eShelf_Construction);
					nAdjacentPolygonIndex = GetModel()->GetPolygonCount();
					GetModel()->AddPolygon(pAdjacentPolygon->Clone());
				}
				newMark.m_pPolygon = GetModel()->GetPolygon(nAdjacentPolygonIndex);
				v.m_MarkList.push_back(newMark);

				bAdded = true;
			}
		}
	}

	if (bAdded)
		m_QueryResult = m_InitQueryResult;

	return bAdded;
}

bool MovePipeline::SubdivisionPass()
{
	if (!m_SubdividedPolygons.empty())
		return false;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);

	std::vector<PolygonPtr> removedPolygons;
	std::vector<PolygonPtr> addedPolygons;

	m_UnsubdividedPolygons.clear();
	m_SubdividedPolygons.clear();

	auto ii = m_OrganizedQueryResult.begin();
	for (; ii != m_OrganizedQueryResult.end(); ++ii)
	{
		PolygonPtr pPolygon = ii->first;
		if (pPolygon == NULL)
			continue;

		if (pPolygon->IsOpen() || pPolygon->GetVertexCount() == 3)
			continue;

		SelectTool::QueryInputs queryInputs(ii->second);
		int iQuerySize(queryInputs.size());

		if (iQuerySize == 0 || iQuerySize == pPolygon->GetVertexCount())
			continue;

		int i = 0;
		for (; i < iQuerySize; ++i)
		{
			int nQueryIndex(queryInputs[i].first);
			// check if a position of the transformed vertex in a polygon gets out of the plane of the polygon,
			if (std::abs(pPolygon->GetPlane().Distance(m_IntermediateTransQueryPos[nQueryIndex])) > kDesignerLooseEpsilon)
				break;
		}

		if (i == iQuerySize)
		{
			m_UnsubdividedPolygons.insert(pPolygon);
			continue;
		}

		if (pPolygon->IsQuad())
		{
			pPolygon->SetFlag(ePolyFlag_NonplanarQuad);
			m_UnsubdividedPolygons.insert(pPolygon);
			continue;
		}

		std::vector<PolygonPtr> trianglePolygons;
		PolygonDecomposer decomposer(eDF_SkipOptimizationOfPolygonResults);
		if (!decomposer.TriangulatePolygon(pPolygon, trianglePolygons))
			continue;
		if (trianglePolygons.size() == 1)
			continue;

		for (int i = 0, iTrianglePolygonSize(trianglePolygons.size()); i < iTrianglePolygonSize; ++i)
			m_SubdividedPolygons[pPolygon].push_back(trianglePolygons[i]);

		addedPolygons.insert(addedPolygons.end(), trianglePolygons.begin(), trianglePolygons.end());
		removedPolygons.push_back(pPolygon);
	}

	for (int i = 0, iRemovedPolygonSize(removedPolygons.size()); i < iRemovedPolygonSize; ++i)
		GetModel()->RemovePolygon(removedPolygons[i]);

	for (int i = 0, iPolygonSize(addedPolygons.size()); i < iPolygonSize; ++i)
		GetModel()->AddPolygon(addedPolygons[i], false);

	return removedPolygons.empty() ? false : true;
}

void MovePipeline::TransformationPass()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);

	auto ii = m_OrganizedQueryResult.begin();
	for (; ii != m_OrganizedQueryResult.end(); ++ii)
	{
		PolygonPtr pPolygon = ii->first;
		if (pPolygon == NULL)
			continue;

		const SelectTool::QueryInputs& queryInputs = ii->second;
		int iQuerySize(queryInputs.size());

		for (int i = 0; i < iQuerySize; ++i)
		{
			int nQueryIndexInQueryInputs(queryInputs[i].first);
			pPolygon->SetPos(GetMark(queryInputs[i]).m_VertexIndex, m_IntermediateTransQueryPos[nQueryIndexInQueryInputs]);
		}

		if (m_UnsubdividedPolygons.find(pPolygon) == m_UnsubdividedPolygons.end() || pPolygon->CheckFlags(ePolyFlag_NonplanarQuad))
		{
			BrushPlane computedPlane;
			if (pPolygon->GetComputedPlane(computedPlane))
				pPolygon->SetPlane(computedPlane);
		}

		if (m_PolygonsInUVIslands.find(pPolygon) == m_PolygonsInUVIslands.end())
			pPolygon->ResetUVs();
	}

	GetModel()->InvalidateAABB(eShelf_Construction);
}

bool MovePipeline::MergeCoplanarPass()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);

	auto ii = m_SubdividedPolygons.begin();
	bool bMergeHappened = false;

	for (; ii != m_SubdividedPolygons.end(); ++ii)
	{
		PolygonList& polygonList = ii->second;
		int iPolygonSize(polygonList.size());
		if (iPolygonSize == 1)
			continue;

		for (int i = 0; i < iPolygonSize; ++i)
		{
			if (polygonList[i] == NULL)
				continue;

			for (int k = 0; k < iPolygonSize; ++k)
			{
				if (i == k || polygonList[k] == NULL)
					continue;

				if (Polygon::HasIntersection(polygonList[i], polygonList[k]) == eIT_JustTouch)
				{
					PolygonPtr previous = polygonList[i]->Clone();
					polygonList[i]->Union(polygonList[k]);
					DESIGNER_ASSERT(polygonList[i]->IsValid());

					GetModel()->RemovePolygon(polygonList[k]);
					polygonList[k] = NULL;
					bMergeHappened = true;
				}
			}
		}
	}

	m_SubdividedPolygons.clear();
	m_UnsubdividedPolygons.clear();

	return bMergeHappened;
}

PolygonPtr MovePipeline::FindAdjacentPolygon(PolygonPtr pPolygon, const BrushVec3& vPos, int& outAdjacentPolygonIndex)
{
	for (int a = 0, iPolygonSize(GetModel()->GetPolygonCount()); a < iPolygonSize; ++a)
	{
		PolygonPtr pCandidatePolygon(GetModel()->GetPolygon(a));
		if (pPolygon == pCandidatePolygon)
			continue;
		if (std::abs(pCandidatePolygon->GetPlane().Distance(vPos)) > kDistanceLimitation)
			continue;
		BrushVec3 nearestPos;
		if (!pCandidatePolygon->QueryNearestPos(vPos, nearestPos))
			continue;
		if ((vPos - nearestPos).GetLength() > kDistanceLimitation)
			continue;
		if (pCandidatePolygon->HasPosition(vPos))
			continue;
		outAdjacentPolygonIndex = a;
		return pCandidatePolygon;
	}
	return NULL;
}

void MovePipeline::Initialize(const ElementSet& elements)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	SetQueryResultsFromSelectedElements(elements);
	SetExcutedAdditionPass(false);

	GetModel()->SetShelf(eShelf_Base);
	std::set<PolygonPtr> polygonSet;
	for (int i = 0, iQueryResult(m_QueryResult.size()); i < iQueryResult; ++i)
	{
		const ModelDB::Vertex& v = m_QueryResult[i];
		for (int k = 0, iMarkSize(v.m_MarkList.size()); k < iMarkSize; ++k)
		{
			PolygonPtr pPolygon = v.m_MarkList[k].m_pPolygon;
			if (!pPolygon->CheckFlags(ePolyFlag_Mirrored))
				polygonSet.insert(pPolygon);
		}
	}
	auto ii = polygonSet.begin();
	for (; ii != polygonSet.end(); ++ii)
	{
		GetModel()->MovePolygonBetweenShelves(*ii, eShelf_Base, eShelf_Construction);

		if (!GetModel()->CheckFlag(eModelFlag_Mirror))
			continue;

		GetModel()->SetShelf(eShelf_Base);

		PolygonPtr pMirroredPolygon = GetModel()->QueryEquivalentPolygon((*ii)->Clone()->Mirror(GetModel()->GetMirrorPlane()));
		DESIGNER_ASSERT(pMirroredPolygon);

		if (!pMirroredPolygon)
			continue;
		GetModel()->RemovePolygon(pMirroredPolygon);
	}

	GetModel()->ResetDB(eDBRF_ALL, eShelf_Construction);
	SetQueryResultsFromSelectedElements(elements);
	m_InitQueryResult = m_QueryResult;
	m_InitSelection = elements;

	InitPolygonsInUVIslands();
}

void MovePipeline::InitializeIsolated(ElementSet& elements)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	m_QueryResult.clear();

	std::map<PolygonPtr, std::vector<int>> polygons;

	for (int i = 0, iElementCount(elements.GetCount()); i < iElementCount; ++i)
	{
		if (elements[i].IsPolygon())
		{
			for (int k = 0, iVertexCount(elements[i].m_pPolygon->GetVertexCount()); k < iVertexCount; ++k)
				polygons[elements[i].m_pPolygon].push_back(k);
		}
		else if (elements[i].IsEdge())
		{
			PolygonList polygonsWithTheEdge;
			GetModel()->QueryNeighbourPolygonsByEdge(elements[i].GetEdge(), polygonsWithTheEdge);
			for (auto ii = polygonsWithTheEdge.begin(); ii != polygonsWithTheEdge.end(); ++ii)
			{
				for (int k = 0, iVertexCount((*ii)->GetVertexCount()); k < iVertexCount; ++k)
					polygons[*ii].push_back(k);
			}
		}
		else if (elements[i].IsVertex())
		{
			ModelDB::QueryResult qr;
			GetModel()->GetDB()->QueryAsVertex(elements[i].m_Vertices[0], qr);
			if (qr.empty())
				continue;
			for (int b = 0, iMarkCount(qr[0].m_MarkList.size()); b < iMarkCount; ++b)
			{
				PolygonPtr polygon = qr[0].m_MarkList[b].m_pPolygon;
				for (int k = 0, iVertexCount(polygon->GetVertexCount()); k < iVertexCount; ++k)
					polygons[polygon].push_back(k);
			}
		}
	}

	auto ii = polygons.begin();

	for (; ii != polygons.end(); ++ii)
	{
		PolygonPtr polygon = (*ii).first;

		GetModel()->SetShelf(eShelf_Base);
		GetModel()->RemovePolygon(polygon);

		if (GetModel()->CheckFlag(eModelFlag_Mirror))
		{
			PolygonPtr pMirroredPolygon = GetModel()->QueryEquivalentPolygon(polygon->Clone()->Mirror(GetModel()->GetMirrorPlane()));
			DESIGNER_ASSERT(pMirroredPolygon);
			if (pMirroredPolygon)
				GetModel()->RemovePolygon(pMirroredPolygon);
		}

		GetModel()->SetShelf(eShelf_Construction);
		GetModel()->AddPolygon(polygon, false);

		for (int i = 0, iVertexCount(ii->second.size()); i < iVertexCount; ++i)
		{
			ModelDB::Vertex v;
			ModelDB::Vertex* pV = &v;
			int vtxIndex = ii->second[i];

			BrushVec3 pos = polygon->GetPos(vtxIndex);
			for (int a = 0, iQueryResultCount(m_QueryResult.size()); a < iQueryResultCount; ++a)
			{
				if (Comparison::IsEquivalent(m_QueryResult[a].m_Pos, pos))
				{
					pV = &m_QueryResult[a];
					break;
				}
			}

			if (pV == &v)
				pV->m_Pos = pos;

			ModelDB::Mark m;
			m.m_pPolygon = polygon;
			m.m_VertexIndex = vtxIndex;
			pV->m_MarkList.push_back(m);
			if (pV == &v)
				m_QueryResult.push_back(v);
		}
	}

	m_InitQueryResult = m_QueryResult;
	m_InitSelection = elements;

	InitPolygonsInUVIslands();
}

void MovePipeline::End()
{
	MergeCoplanarPass();

	GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	GetModel()->ResetDB(eDBRF_ALL);

	if (m_QueryResult.size() == m_IntermediateTransQueryPos.size())
	{
		for (int i = 0, iQuerySize(m_QueryResult.size()); i < iQuerySize; ++i)
			m_QueryResult[i].m_Pos = m_IntermediateTransQueryPos[i];
	}
}

bool MovePipeline::GetAveragePos(BrushVec3& outAveragePos) const
{
	if (m_IntermediateTransQueryPos.empty())
		return false;
	BrushVec3 vAveragePos(0, 0, 0);
	int iQSize(m_IntermediateTransQueryPos.size());
	if (iQSize > 0)
	{
		for (int i = 0; i < iQSize; ++i)
			vAveragePos += m_IntermediateTransQueryPos[i];
		vAveragePos /= iQSize;
	}
	outAveragePos = vAveragePos;
	return true;
}

void MovePipeline::SnappedToMirrorPlane()
{
	if (m_IntermediateTransQueryPos.empty())
		return;

	if (!GetModel()->CheckFlag(eModelFlag_Mirror))
		return;

	int iIntermediateCount(m_IntermediateTransQueryPos.size());
	BrushVec3 mirrorNormal = GetModel()->GetMirrorPlane().Normal();
	int nFarthestPosIndex = -1;
	BrushFloat fFarthestDist = 0;
	for (int i = 0; i < iIntermediateCount; ++i)
	{
		BrushFloat distanceFromPosToMirrorPlane = GetModel()->GetMirrorPlane().Distance(m_IntermediateTransQueryPos[i]);
		if (distanceFromPosToMirrorPlane > 0 && distanceFromPosToMirrorPlane > fFarthestDist)
		{
			nFarthestPosIndex = i;
			fFarthestDist = distanceFromPosToMirrorPlane;
		}
	}

	if (nFarthestPosIndex != -1)
	{
		BrushVec3 vOffsetedPos;
		if (GetModel()->GetMirrorPlane().HitTest(BrushRay(m_IntermediateTransQueryPos[nFarthestPosIndex], mirrorNormal), NULL, &vOffsetedPos))
		{
			BrushVec3 vOffset = vOffsetedPos - m_IntermediateTransQueryPos[nFarthestPosIndex];
			for (int i = 0; i < iIntermediateCount; ++i)
				m_IntermediateTransQueryPos[i] += vOffset;
		}
	}
}

void MovePipeline::InitPolygonsInUVIslands()
{
	m_PolygonsInUVIslands.clear();
	UVIslandManager* pUVIslandMgr = m_pModel->GetUVIslandMgr();

	auto ii = m_QueryResult.begin();
	for (; ii != m_QueryResult.end(); ++ii)
	{
		auto iMark = ii->m_MarkList.begin();
		for (; iMark != ii->m_MarkList.end(); ++iMark)
		{
			PolygonPtr polygon = iMark->m_pPolygon;
			if (polygon)
			{
				std::vector<UVIslandPtr> outUVIslands;
				if (pUVIslandMgr->FindUVIslandsHavingPolygon(polygon, outUVIslands))
					m_PolygonsInUVIslands.insert(polygon);
			}
		}
	}
}
}

