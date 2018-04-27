// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModelCompiler.h"
#include "Model.h"
#include "Util/Undo.h"
#include "Core/EdgesSharpnessManager.h"
#include "Core/UVIslandManager.h"
#include "Core/SmoothingGroupManager.h"
#include "LoopPolygons.h"
#include "HalfEdgeMesh.h"
#include "ModelDB.h"

namespace Designer
{

void Model::Init()
{
	m_Flags = 0;
	m_ShelfID = eShelf_Base;
	m_MirrorPlane = BrushPlane(BrushVec3(0, 0, 0), 0);
	m_SubdivisionLevel = 0;
	m_pDB = new ModelDB;
	m_pDB->AddRef();
	m_pSmoothingGroupMgr = new SmoothingGroupManager;
	m_pSmoothingGroupMgr->AddRef();
	m_pEdgeSharpnessMgr = new EdgeSharpnessManager;
	m_pEdgeSharpnessMgr->AddRef();
	m_pUVIslandMgr = new UVIslandManager;
	m_pUVIslandMgr->AddRef();
	m_pSubdividionResult = NULL;
	m_bSmoothingGroupForSubdividedSurfaces = false;
}

Model::Model()
{
	Init();
}

Model::~Model()
{
	if (m_pDB)
		m_pDB->Release();
	if (m_pSmoothingGroupMgr)
		m_pSmoothingGroupMgr->Release();
	if (m_pEdgeSharpnessMgr)
		m_pEdgeSharpnessMgr->Release();
	if (m_pSubdividionResult)
		m_pSubdividionResult->Release();
	if (m_pUVIslandMgr)
		m_pUVIslandMgr->Release();
}

Model::Model(const std::vector<PolygonPtr>& polygonList)
{
	Init();
	m_Polygons[0] = polygonList;
}

Model::Model(const Model& model)
{
	Init();
	operator=(model);
}

Model& Model::operator=(const Model& model)
{
	for (int shelfID = eShelf_Base; shelfID < cShelfMax; ++shelfID)
	{
		SetShelf(static_cast<ShelfID>(shelfID));
		Clear();
	}

	for (int shelfID = eShelf_Base; shelfID < cShelfMax; ++shelfID)
	{
		int nPolygonSize(model.m_Polygons[shelfID].size());
		m_Polygons[shelfID].reserve(nPolygonSize);
		for (int i = 0; i < nPolygonSize; ++i)
			m_Polygons[shelfID].push_back(new Polygon(*model.m_Polygons[shelfID][i]));
	}

	m_MirrorPlane = model.m_MirrorPlane;
	m_Flags = model.m_Flags;
	m_SubdivisionLevel = model.m_SubdivisionLevel;

	*m_pDB = *model.m_pDB;
	*m_pEdgeSharpnessMgr = *model.m_pEdgeSharpnessMgr;
	*m_pUVIslandMgr = *model.m_pUVIslandMgr;
	m_pSmoothingGroupMgr->CopyFromModel(this, &model);

	m_pUVIslandMgr->Reset(this);

	m_ShelfID = eShelf_Base;

	return *this;
}

bool Model::QueryPosition(
  const BrushPlane& plane,
  const BrushRay& ray,
  BrushVec3& outPosition,
  BrushFloat* outDist,
  PolygonPtr* outPolygon) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	BrushFloat dist;
	if (plane.HitTest(ray, &dist, &outPosition) == false)
		return false;
	int nPolygon(-1);
	if (outPolygon && QueryPolygon(plane, ray, nPolygon))
		*outPolygon = m_Polygons[m_ShelfID][nPolygon];
	if (outDist)
		*outDist = dist;
	return true;
}

bool Model::QueryPosition(
  const BrushRay& ray,
  BrushVec3& outPosition,
  BrushPlane* outPlane,
  BrushFloat* outDist,
  PolygonPtr* outPolygon) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	int nPolygon(-1);
	if (QueryPolygon(ray, nPolygon) == false)
		return false;

	PolygonPtr pPolygon(m_Polygons[m_ShelfID][nPolygon]);
	BrushFloat dist;
	if (pPolygon->GetPlane().HitTest(ray, &dist, &outPosition) == false)
		return false;

	if (outPolygon)
		*outPolygon = pPolygon;
	if (outPlane)
		*outPlane = pPolygon->GetPlane();
	if (outDist)
		*outDist = dist;

	return true;
}

void Model::QueryPolygonsHavingEdge(
  const BrushEdge3D& edge,
  std::vector<PolygonPtr>& outPolygons) const
{
	for (int i = 0, iPolygonCount(GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = GetPolygon(i);
		if (polygon->HasEdge(edge))
			outPolygons.push_back(polygon);
	}
}

bool Model::QueryEdgesWithPos(
  const BrushVec3& pos,
  std::vector<BrushEdge3D>& outEdges) const
{
	bool bFound = false;

	for (int i = 0, iPolygonCount(GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = GetPolygon(i);

		std::vector<int> edgeIndices;
		if (!pPolygon->QueryEdgesWithPos(pos, edgeIndices))
			continue;

		bFound = true;
		for (int k = 0, iEdgeCount(edgeIndices.size()); k < iEdgeCount; ++k)
			outEdges.push_back(pPolygon->GetEdge(edgeIndices[k]));
	}

	return bFound;
}

void Model::QueryOpenPolygons(
  const BrushRay& ray,
  std::vector<PolygonPtr>& outPolygons) const
{
	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		if (!m_Polygons[m_ShelfID][i]->IsOpen())
			continue;
		Vec3 vHitPos;
		AABB aabb = m_Polygons[m_ShelfID][i]->GetExpandedBoundBox(0.01f);
		if (Intersect::Ray_AABB(ToVec3(ray.origin), ToVec3(ray.direction), aabb, vHitPos))
			outPolygons.push_back(m_Polygons[m_ShelfID][i]);
	}
}

bool Model::AddPolygon(
  PolygonPtr pPolygon,
  bool bResetUVs)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	if (!pPolygon)
		return false;

	if (pPolygon->IsOpen())
	{
		m_Polygons[m_ShelfID].push_back(pPolygon);
		return true;
	}

	AddIslands(pPolygon, false, bResetUVs);
	return true;
}

bool Model::AddUnionPolygon(PolygonPtr pPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon || pPolygon->IsOpen())
		return false;

	AABB aabb = pPolygon->GetExpandedBoundBox();

	PolygonPtr pNewPolygon = pPolygon->Clone();
	std::set<PolygonPtr> deletedPolygons;
	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->IsOpen())
			continue;

		if (pPolygon->CheckFlags(ePolyFlag_Mirrored) != m_Polygons[m_ShelfID][i]->CheckFlags(ePolyFlag_Mirrored))
			continue;

		if (!aabb.IsIntersectBox(m_Polygons[m_ShelfID][i]->GetExpandedBoundBox()))
			continue;

		if (Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pPolygon) != eIT_None)
		{
			PolygonPtr pBackupPolygon = pNewPolygon->Clone();

			if (pNewPolygon->Union(m_Polygons[m_ShelfID][i]))
				deletedPolygons.insert(m_Polygons[m_ShelfID][i]);
			else
				return false;
		}
	}

	if (!pNewPolygon->IsValid() || pNewPolygon->IsOpen())
		return false;

	DeletePolygons(deletedPolygons);
	pNewPolygon->ResetUVs();
	AddIslands(pNewPolygon);
	InvalidateAABB(m_ShelfID);

	return true;
}

bool Model::AddSubtractedPolygon(PolygonPtr pPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon || pPolygon->IsOpen())
		return false;

	AABB aabb = pPolygon->GetExpandedBoundBox();

	PolygonPtr pNewPolygon = pPolygon->Clone();
	bool bIntersetion(false);

	std::vector<PolygonPtr> copiedPolygons(m_Polygons[m_ShelfID]);
	auto ii = copiedPolygons.begin();

	for (; ii != copiedPolygons.end(); ++ii)
	{
		if ((*ii)->IsOpen())
			continue;
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored) != (*ii)->CheckFlags(ePolyFlag_Mirrored))
			continue;
		if (!aabb.IsIntersectBox((*ii)->GetExpandedBoundBox()))
			continue;
		if ((*ii)->Subtract(pNewPolygon))
		{
			bIntersetion = true;
			auto ioriginal = m_Polygons[m_ShelfID].begin();
			for (; ioriginal != m_Polygons[m_ShelfID].end(); ++ioriginal)
			{
				if (*ioriginal == *ii)
				{
					ioriginal = m_Polygons[m_ShelfID].erase(ioriginal);
					break;
				}
			}
			if ((*ii)->IsValid())
			{
				(*ii)->ResetUVs();
				AddIslands(*ii);
			}
		}
	}

	if (bIntersetion == false)
	{
		pNewPolygon->ResetUVs();
		AddIslands(pNewPolygon);
	}

	InvalidateAABB(m_ShelfID);

	return true;
}

bool Model::AddIntersectedPolygon(PolygonPtr pPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon || pPolygon->IsOpen())
		return false;

	AABB aabb = pPolygon->GetExpandedBoundBox();

	PolygonPtr pNewPolygon = pPolygon->Clone();
	std::vector<PolygonPtr> intersectedPolygons;
	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->IsOpen())
			continue;
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored) != m_Polygons[m_ShelfID][i]->CheckFlags(ePolyFlag_Mirrored))
			continue;
		if (!aabb.IsIntersectBox(m_Polygons[m_ShelfID][i]->GetExpandedBoundBox()))
			continue;
		if (!m_Polygons[m_ShelfID][i]->Intersect(pNewPolygon, eICEII_IncludeCoSame))
			return false;
		intersectedPolygons.push_back(m_Polygons[m_ShelfID][i]);
	}

	if (intersectedPolygons.empty())
	{
		AddPolygon(pNewPolygon);
	}
	else
	{
		for (int i = 0, iPolygonCount(intersectedPolygons.size()); i < iPolygonCount; ++i)
		{
			if (!intersectedPolygons[i]->IsValid())
			{
				RemovePolygon(intersectedPolygons[i]);
			}
			else
			{
				intersectedPolygons[i]->ResetUVs();
				AddIslands(intersectedPolygons[i], true);
			}
		}
	}

	InvalidateAABB(m_ShelfID);

	return true;
}

bool Model::AddXORPolygon(
  PolygonPtr pPolygon,
  bool bApplyUnionToAdjancentPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon || pPolygon->IsOpen())
		return false;

	AABB aabb = pPolygon->GetExpandedBoundBox();

	std::vector<PolygonPtr> overlappedPolygons;
	std::vector<PolygonPtr> overlappedReplicatedPolygons;
	std::vector<PolygonPtr> touchedPolygons;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->IsOpen())
			continue;
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored) != m_Polygons[m_ShelfID][i]->CheckFlags(ePolyFlag_Mirrored))
			continue;
		if (!aabb.IsIntersectBox(m_Polygons[m_ShelfID][i]->GetExpandedBoundBox()))
			continue;
		if (!pPolygon->GetPlane().IsEquivalent(m_Polygons[m_ShelfID][i]->GetPlane()))
			continue;

		EIntersectionType it = Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pPolygon);
		if (it == eIT_Intersection || it == eIT_JustTouch && m_Polygons[m_ShelfID][i]->HasBridgeEdges())
		{
			overlappedPolygons.push_back(m_Polygons[m_ShelfID][i]);
			overlappedReplicatedPolygons.push_back(m_Polygons[m_ShelfID][i]->Clone());
		}
		else if (it == eIT_JustTouch)
		{
			touchedPolygons.push_back(m_Polygons[m_ShelfID][i]);
		}
	}

	int iOverlappedPolygonCount = overlappedPolygons.size();
	if (iOverlappedPolygonCount == 0)
	{
		if (!touchedPolygons.empty())
		{
			if (bApplyUnionToAdjancentPolygon)
			{
				touchedPolygons[0]->Union(pPolygon);
				for (int i = 1, iTouchedPolygonCount(touchedPolygons.size()); i < iTouchedPolygonCount; ++i)
				{
					touchedPolygons[0]->Union(touchedPolygons[i]);
					RemovePolygon(touchedPolygons[i]);
				}
			}
			else
			{
				AddPolygon(pPolygon);
			}
			touchedPolygons[0]->ResetUVs();
		}
		else
		{
			AddPolygon(pPolygon->Clone());
		}
		InvalidateAABB(m_ShelfID);
		return true;
	}
	else if (iOverlappedPolygonCount == 1 && pPolygon->IncludeAllEdges(overlappedPolygons[0]))
	{
		PolygonPtr pInputPolygon = pPolygon->Clone();
		pInputPolygon->Subtract(overlappedPolygons[0]);
		if (pInputPolygon->IsValid())
		{
			pInputPolygon->Flip();
			*overlappedPolygons[0] = *pInputPolygon;
			overlappedPolygons[0]->ResetUVs();
			AddIslands(overlappedPolygons[0], true);
		}
		else
		{
			auto ii = m_Polygons[m_ShelfID].begin();
			for (; ii != m_Polygons[m_ShelfID].end(); ++ii)
			{
				if (*ii == overlappedPolygons[0])
				{
					m_Polygons[m_ShelfID].erase(ii);
					break;
				}
			}
		}
		InvalidateAABB(m_ShelfID);
		return true;
	}
	else
	{
		for (int i = 0; i < iOverlappedPolygonCount; ++i)
		{
			if (pPolygon->IncludeAllEdges(overlappedPolygons[i]))
			{
				auto ii = m_Polygons[m_ShelfID].begin();
				for (; ii != m_Polygons[m_ShelfID].end(); ++ii)
				{
					if (*ii == overlappedPolygons[i])
					{
						m_Polygons[m_ShelfID].erase(ii);
						InvalidateAABB(m_ShelfID);
						break;
					}
				}
			}
			else
			{
				PolygonPtr pInputPolygon(pPolygon->Clone());
				for (int k = 0; k < iOverlappedPolygonCount; ++k)
				{
					if (i == k)
						continue;
					pInputPolygon->Subtract(overlappedReplicatedPolygons[k]);
				}

				overlappedPolygons[i]->Subtract(pInputPolygon);
				overlappedPolygons[i]->ResetUVs();
				AddIslands(overlappedPolygons[i], true);

				pInputPolygon->Subtract(overlappedReplicatedPolygons[i]);
				if (pInputPolygon->IsValid())
				{
					pInputPolygon->Flip();
					pInputPolygon->ResetUVs();
					AddIslands(pInputPolygon);
				}

				InvalidateAABB(m_ShelfID);
			}
		}
	}

	return true;
}

void Model::Replace(
  int nIndex,
  PolygonPtr pPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return;
	if (nIndex < 0 || nIndex >= m_Polygons[m_ShelfID].size())
		return;

	*(m_Polygons[m_ShelfID][nIndex]) = *pPolygon;
	InvalidateAABB(m_ShelfID);
}

bool Model::AddSplitPolygon(PolygonPtr pPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return false;

	if (pPolygon->IsOpen())
	{
		SplitPolygonsByOpenPolygon(pPolygon);
		return true;
	}

	AABB aabb = pPolygon->GetExpandedBoundBox();

	PolygonPtr pEnteredPolygon = pPolygon->Clone();

	std::set<PolygonPtr> deletedPolygons;
	std::vector<PolygonPtr> spannedPolygons;
	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->IsOpen())
			continue;
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored) != m_Polygons[m_ShelfID][i]->CheckFlags(ePolyFlag_Mirrored))
			continue;
		if (!aabb.IsIntersectBox(m_Polygons[m_ShelfID][i]->GetExpandedBoundBox()))
			continue;
		if (!pPolygon->GetPlane().IsEquivalent(m_Polygons[m_ShelfID][i]->GetPlane()))
			continue;
		if (Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pEnteredPolygon) == eIT_Intersection)
		{
			spannedPolygons.push_back(m_Polygons[m_ShelfID][i]->Clone());
			deletedPolygons.insert(m_Polygons[m_ShelfID][i]);
		}
	}

	if (spannedPolygons.empty())
	{
		if (pEnteredPolygon)
			AddPolygon(pEnteredPolygon);
		return true;
	}

	for (int i = 0, iSize(spannedPolygons.size()); i < iSize; ++i)
	{
		if (!pEnteredPolygon->Subtract(spannedPolygons[i]))
			break;
	}

	std::vector<PolygonPtr> intersectedPolygons;
	for (int i = 0, iSize(spannedPolygons.size()); i < iSize; ++i)
	{
		PolygonPtr intersectedPolygon = spannedPolygons[i]->Clone();
		if (intersectedPolygon->Intersect(pPolygon, eICEII_IncludeCoSame))
			intersectedPolygons.push_back(intersectedPolygon);
	}

	std::vector<PolygonPtr> subtractedPolygons(spannedPolygons);
	for (int i = 0, iSize(subtractedPolygons.size()); i < iSize; ++i)
	{
		if (!subtractedPolygons[i]->Subtract(pPolygon))
			return false;
	}

	DeletePolygons(deletedPolygons);

	for (int i = 0, iSize(subtractedPolygons.size()); i < iSize; ++i)
	{
		subtractedPolygons[i]->ResetUVs();
		AddIslands(subtractedPolygons[i]);
	}

	for (int i = 0, iSize(intersectedPolygons.size()); i < iSize; ++i)
	{
		intersectedPolygons[i]->ResetUVs();
		AddIslands(intersectedPolygons[i]);
	}

	if (pEnteredPolygon && pEnteredPolygon->IsValid())
	{
		pEnteredPolygon->ResetUVs();
		AddIslands(pEnteredPolygon);
	}

	return true;
}

void Model::AddIslands(
  PolygonPtr pPolygon,
  bool bAddedOnlyIslands,
  bool bResetUV)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon->IsValid())
		return;

	const std::vector<PolygonPtr>& islands = pPolygon->GetLoops()->GetIslands();
	if (islands.size() >= 2)
	{
		for (int k = 0, iIslandCount(islands.size()); k < iIslandCount; ++k)
		{
			DESIGNER_ASSERT(islands[k]->IsValid());
			if (bResetUV)
				AddPolygonAfterResetingUV(m_ShelfID, islands[k]);
			else
				m_Polygons[m_ShelfID].push_back(islands[k]);
			RemovePolygon(pPolygon);
		}
	}
	else if (!bAddedOnlyIslands)
	{
		if (bResetUV)
			AddPolygonAfterResetingUV(m_ShelfID, pPolygon);
		else
			m_Polygons[m_ShelfID].push_back(pPolygon);
	}
}

void Model::RecordUndo(
  const char* sUndoDescription,
  CBaseObject* pObject) const
{
	if (pObject == NULL)
		return;
	if (CUndo::IsRecording())
		CUndo::Record(new DesignerUndo(pObject, this, sUndoDescription));
}

bool Model::EraseEdge(const BrushEdge3D& edge)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	std::vector<PolygonPtr> neighbourPolygons;

	QueryNeighbourPolygonsByEdge(edge, neighbourPolygons);
	if (neighbourPolygons.empty())
		return false;

	if (neighbourPolygons.size() == 1 && neighbourPolygons[0]->IsOpen())
	{
		int edgeIndex;
		if (!neighbourPolygons[0]->IsEdgeOverlappedByBoundary(edge, &edgeIndex))
			return false;

		std::vector<PolygonPtr> dividedPolygons;
		neighbourPolygons[0]->DivideByEdge(edgeIndex, dividedPolygons);
		RemovePolygon(neighbourPolygons[0]);
		if (dividedPolygons.empty())
			return true;
		for (int i = 0, iPolygonSize(dividedPolygons.size()); i < iPolygonSize; ++i)
			AddPolygon(dividedPolygons[i]);
	}
	else if (neighbourPolygons.size() == 1 &&
	         neighbourPolygons[0]->HasEdge(edge, true) &&
	         neighbourPolygons[0]->HasEdge(edge.GetInverted(), true))
	{
		neighbourPolygons[0]->RemoveEdge(edge);
		neighbourPolygons[0]->RemoveEdge(edge.GetInverted());
	}
	else if (neighbourPolygons.size() == 2)
	{
		if (neighbourPolygons[0]->IncludeAllEdges(neighbourPolygons[1]))
		{
			neighbourPolygons[0]->Union(neighbourPolygons[1]);
			neighbourPolygons[0]->ResetUVs();
			neighbourPolygons[1]->RemoveEdge(edge);
		}
		else if (neighbourPolygons[1]->IncludeAllEdges(neighbourPolygons[0]))
		{
			neighbourPolygons[1]->Union(neighbourPolygons[0]);
			neighbourPolygons[1]->ResetUVs();
			neighbourPolygons[0]->RemoveEdge(edge);
		}
		else if (!neighbourPolygons[0]->GetPlane().IsEquivalent(neighbourPolygons[1]->GetPlane()))
		{
			RemovePolygon(neighbourPolygons[0]);
			RemovePolygon(neighbourPolygons[1]);
		}
		else
		{
			PolygonPtr pClone0 = neighbourPolygons[0]->Clone();
			neighbourPolygons[0]->Union(neighbourPolygons[1]);
			neighbourPolygons[0]->ResetUVs();

			RemovePolygon(neighbourPolygons[1]);

			neighbourPolygons[1]->Intersect(pClone0, eICEII_IncludeCoDiff);

			std::vector<PolygonPtr> isolatedPaths;
			neighbourPolygons[1]->GetIsolatedPaths(isolatedPaths);

			for (int k = 0, iUnconnectedSize(isolatedPaths.size()); k < iUnconnectedSize; ++k)
			{
				int edgeIndex;

				if (isolatedPaths[k]->IsEdgeOverlappedByBoundary(edge, &edgeIndex))
				{
					std::vector<PolygonPtr> dividedPolygons;
					isolatedPaths[k]->DivideByEdge(edgeIndex, dividedPolygons);
					for (int i = 0, iSize(dividedPolygons.size()); i < iSize; ++i)
						AddPolygon(dividedPolygons[i]);
				}
				else
				{
					AddPolygon(isolatedPaths[k]);
				}
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool Model::EraseVertex(const BrushVec3& position)
{
	ModelDB::QueryResult qlist;
	GetDB()->QueryAsVertex(position, qlist);
	if (qlist.empty())
		return false;

	for (int k = 0, iQListCount(qlist.size()); k < iQListCount; ++k)
	{
		for (int a = 0, iMarkCount(qlist[k].m_MarkList.size()); a < iMarkCount; ++a)
			RemovePolygon(qlist[k].m_MarkList[a].m_pPolygon);
	}

	return true;
}

void Model::QueryNeighbourPolygonsByEdge(
  const BrushEdge3D& edge,
  PolygonList& outPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	BrushVec3 edgeDir = (edge.m_v[1] - edge.m_v[0]).GetNormalized();

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		const PolygonPtr& pPolygon(m_Polygons[m_ShelfID][i]);
		if (pPolygon == NULL)
			continue;
		if (!pPolygon->IsOpen() && std::abs(pPolygon->GetPlane().Normal().Dot(edgeDir)) > kDesignerLooseEpsilon)
			continue;
		if (!pPolygon->IsEdgeOverlappedByBoundary(edge))
			continue;
		outPolygons.push_back(pPolygon);
	}
}

void Model::QueryPolygonsSharingEdge(
  const BrushEdge3D& edge,
  PolygonList& outPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	AABB edgeAABB;
	edgeAABB.Reset();
	edgeAABB.Add(ToVec3(edge.m_v[0]));
	edgeAABB.Add(ToVec3(edge.m_v[1]));
	edgeAABB.Expand(Vec3(0.001, 0.001f, 0.001f));

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		const PolygonPtr& pPolygon(m_Polygons[m_ShelfID][i]);
		if (pPolygon == NULL)
			continue;
		if (!pPolygon->IntersectedBetweenAABBs(edgeAABB))
			continue;
		if (pPolygon->HasEdge(edge) && !pPolygon->IsOpen())
			outPolygons.push_back(pPolygon);
	}
}

bool Model::HasIntersection(
  PolygonPtr pPolygon,
  bool bStrongCheck) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return false;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (pPolygon == m_Polygons[m_ShelfID][i])
			continue;
		if (bStrongCheck)
		{
			if (Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pPolygon) == eIT_Intersection)
				return true;
		}
		else
		{
			if (Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pPolygon) != eIT_None)
				return true;
		}
	}
	return false;
}

bool Model::HasTouched(PolygonPtr pPolygon) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return false;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (pPolygon == m_Polygons[m_ShelfID][i])
			continue;
		if (Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pPolygon) == eIT_JustTouch)
			return true;
	}
	return false;
}

bool Model::HasEdge(const BrushEdge3D& edge) const
{
	for (int i = 0, iPolygonCount(GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = GetPolygon(i);
		if (pPolygon->HasEdge(edge))
			return true;
	}
	return false;
}

bool Model::HasPolygon(const PolygonPtr polygon) const
{
	for (int i = 0, iCount(GetPolygonCount()); i < iCount; ++i)
	{
		if (GetPolygon(i) == polygon)
			return true;
	}
	return false;
}

bool Model::RemovePolygon(
  int nPolygonIndex,
  bool bLeaveFrame)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	PolygonPtr pPolygon(GetPolygonPtr(nPolygonIndex));
	if (pPolygon == NULL)
		return false;

	if (bLeaveFrame && (m_Flags & eModelFlag_LeaveFrameAfterPolygonRemoved))
	{
		ESurroundType surroundType = QuerySurroundType(pPolygon);
		if (surroundType == eSurroundType_None ||
		    surroundType == eSurroundType_SurroundsOtherPolygons ||
		    surroundType == eSurroundType_SurroundedPartly)
		{
			PolygonPtr pScaledPolygon = pPolygon->Clone();
			pScaledPolygon->RemoveAllHoles();
			pScaledPolygon->Scale((BrushFloat)0.1);
			AddSubtractedPolygon(pScaledPolygon);
			return true;
		}
	}

	m_Polygons[m_ShelfID].erase(m_Polygons[m_ShelfID].begin() + nPolygonIndex);
	InvalidateAABB(m_ShelfID);

	return true;
}

bool Model::RemovePolygon(
  PolygonPtr pPolygon,
  bool bLeaveFrame)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i] == pPolygon)
			return RemovePolygon(i, bLeaveFrame);
	}

	return false;
}

void Model::RemovePolygonsWithSpecificFlagsAndPlane(
  int nPolygonFlags,
  const BrushPlane* pPlane)
{
	auto ii = m_Polygons[m_ShelfID].begin();
	for (; ii != m_Polygons[m_ShelfID].end(); )
	{
		if (pPlane && !pPlane->IsEquivalent((*ii)->GetPlane()))
		{
			++ii;
			continue;
		}

		if ((*ii)->CheckFlags(nPolygonFlags))
			ii = m_Polygons[m_ShelfID].erase(ii);
		else
			++ii;
	}
	InvalidateAABB(m_ShelfID);
}

PolygonPtr Model::QueryPolygon(CryGUID guid) const
{
	for (int i = eShelf_Base; i < cShelfMax; ++i)
	{
		for (int k = 0, iPolygonCount(m_Polygons[i].size()); k < iPolygonCount; ++k)
		{
			if (m_Polygons[i][k]->GetGUID() == guid)
				return m_Polygons[i][k];
		}
	}
	return NULL;
}

PolygonPtr Model::GetPolygonPtr(int nIndex) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (nIndex >= m_Polygons[m_ShelfID].size() || nIndex < 0)
		return NULL;
	return m_Polygons[m_ShelfID][nIndex];
}

bool Model::QueryPolygon(
  const BrushPlane& plane,
  const BrushRay& ray,
  int& nOutIndex) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	BrushPlane invPlane = plane.GetInverted();

	BrushFloat fShortestDist(1000000.0f);
	nOutIndex = -1;
	for (int i = 0, polygonSize(m_Polygons[m_ShelfID].size()); i < polygonSize; ++i)
	{
		PolygonPtr polygon = m_Polygons[m_ShelfID][i];
		BrushFloat t = 0;
		if (!polygon->GetPlane().IsEquivalent(plane) && !polygon->GetPlane().IsEquivalent(invPlane))
			continue;
		if (polygon->CheckFlags(ePolyFlag_Hidden))
			continue;
		if (polygon->IsPassed(ray, t))
		{
			if (t < fShortestDist || std::abs(t - fShortestDist) < kDesignerEpsilon && polygon->IsOpen())
			{
				fShortestDist = t;
				nOutIndex = i;
			}
		}
	}
	return nOutIndex != -1;
}

bool Model::QueryPolygons(
  const BrushPlane& plane,
  std::vector<PolygonPtr>& outPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	bool bAdded = false;
	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->GetPlane().IsEquivalent(plane))
		{
			bAdded = true;
			outPolygons.push_back(m_Polygons[m_ShelfID][i]);
		}
	}
	return bAdded;
}

bool Model::QueryIntersectedPolygonsByAABB(
  const AABB& aabb,
  PolygonList& outPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	bool bAdded = false;
	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->IntersectedBetweenAABBs(aabb))
		{
			bAdded = true;
			outPolygons.push_back(m_Polygons[m_ShelfID][i]);
		}
	}
	return bAdded;
}

bool Model::QueryPolygon(
  const BrushRay& ray,
  int& nOutIndex) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	BrushFloat fShortestDist(1000000.0f);
	nOutIndex = -1;
	for (int i = 0, polygonSize(m_Polygons[m_ShelfID].size()); i < polygonSize; ++i)
	{
		PolygonPtr pPolygon = m_Polygons[m_ShelfID][i];

		if (!pPolygon->IsValid() || pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		BrushFloat t = 0;
		if (pPolygon->IsPassed(ray, t))
		{
			BrushFloat fDistanceFromPlane = pPolygon->GetPlane().Distance(ray.origin);
			BrushFloat fAbsShortestDistance = std::abs(fShortestDist);
			if (std::abs(t - fAbsShortestDistance) < kDesignerEpsilon && fDistanceFromPlane >= 0 || t < fAbsShortestDistance)
			{
				fShortestDist = t;
				if (fDistanceFromPlane < 0)
					fShortestDist = -fShortestDist;
				nOutIndex = i;
			}
		}
	}
	return nOutIndex != -1;
}

bool Model::QueryCenterOfPolygon(
  const BrushRay& ray,
  BrushVec3& outCenterOfPos) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	int nPolygonIndex(0);
	if (!QueryPolygon(ray, nPolygonIndex))
		return false;

	outCenterOfPos = m_Polygons[m_ShelfID][nPolygonIndex]->GetBoundBox().GetCenter();
	return true;
}

bool Model::QueryNearestPos(
  const BrushVec3& pos,
  BrushVec3& outNearestPos) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	BrushFloat fNearestDistance(kEnoughBigNumber);
	BrushVec3 NearestPos;
	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		PolygonPtr polygon(m_Polygons[m_ShelfID][i]);
		BrushVec3 vNearestPos;

		if (!m_Polygons[m_ShelfID][i]->QueryNearestPos(pos, vNearestPos))
			continue;

		BrushFloat fSqDistance((vNearestPos - pos).GetLengthSquared());
		if (fSqDistance < fNearestDistance)
		{
			fNearestDistance = fSqDistance;
			outNearestPos = vNearestPos;
		}
	}
	return fNearestDistance < kEnoughBigNumber;
}

Model::ESurroundType Model::QuerySurroundType(PolygonPtr polygon) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	PolygonPtr pUnionPolygon = new Polygon;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (polygon == m_Polygons[m_ShelfID][i])
			continue;
		if (!polygon->IsPlaneEquivalent(m_Polygons[m_ShelfID][i]))
			continue;
		if (Polygon::HasIntersection(polygon, m_Polygons[m_ShelfID][i]) != eIT_JustTouch)
			continue;
		pUnionPolygon->Union(m_Polygons[m_ShelfID][i]);
	}

	if (!pUnionPolygon->IsValid())
		return eSurroundType_None;

	pUnionPolygon->RemoveAllHoles();

	if (pUnionPolygon->IncludeAllEdges(polygon))
		return eSurroundType_SurroundedByOtherPolygons;

	PolygonPtr pInputPolygon = polygon->Clone();
	pInputPolygon->RemoveAllHoles();
	if (pInputPolygon->IncludeAllEdges(pUnionPolygon))
		return eSurroundType_SurroundsOtherPolygons;

	return eSurroundType_SurroundedPartly;
}

bool Model::QueryNearestEdges(
  const BrushPlane& plane,
  const BrushRay& ray,
  BrushVec3& outPos,
  BrushVec3& outPosOnEdge,
  std::vector<QueryEdgeResult>& outEdges) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	float fShortestDist = 3e10f;
	BrushVec3 posOnEdge;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		const PolygonPtr pPolygon(m_Polygons[m_ShelfID][i]);
		if (pPolygon == NULL ||
		    !pPolygon->GetPlane().IsEquivalent(plane))
		{
			continue;
		}
		BrushPlane p;
		BrushEdge3D edge;
		if (!QueryNearestEdge(i, ray, outPos, posOnEdge, p, edge))
			continue;
		BrushFloat fDistance = outPos.GetDistance(posOnEdge);
		if (std::abs(fDistance - fShortestDist) < kDesignerEpsilon)
		{
			outEdges.push_back(QueryEdgeResult(pPolygon, edge));
		}
		else if (fDistance < fShortestDist)
		{
			fShortestDist = fDistance;
			outPosOnEdge = posOnEdge;
			outEdges.clear();
			outEdges.push_back(QueryEdgeResult(pPolygon, edge));
		}
	}
	return fShortestDist < 3e9f;
}

bool Model::QueryNearestEdges(
  const BrushPlane& plane,
  const BrushVec3& position,
  BrushVec3& outPosOnEdge,
  std::vector<QueryEdgeResult>& outEdges) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	float fShorestDist = 3e10f;
	BrushVec3 posOnEdge;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		const PolygonPtr pPolygon(m_Polygons[m_ShelfID][i]);
		if (pPolygon == NULL)
			continue;
		if (!pPolygon->GetPlane().IsEquivalent(plane))
			continue;
		BrushPlane p;
		BrushEdge3D edge;
		if (!QueryNearestEdge(i, position, posOnEdge, p, edge))
			continue;
		BrushFloat fDistance = position.GetDistance(posOnEdge);
		if (std::abs(fDistance - fShorestDist) < kDesignerEpsilon)
		{
			outEdges.push_back(QueryEdgeResult(pPolygon, edge));
		}
		else if (fDistance < fShorestDist)
		{
			fShorestDist = fDistance;
			outPosOnEdge = posOnEdge;
			outEdges.clear();
			outEdges.push_back(QueryEdgeResult(pPolygon, edge));
		}
	}
	return fShorestDist < 3e9f;
}

bool Model::QueryNearestEdges(
  const BrushRay& ray,
  BrushVec3& outPos,
  BrushVec3& outPosOnEdge,
  BrushPlane& outPlane,
  std::vector<QueryEdgeResult>& outEdges) const
{
	int nPolygonIndex(-1);
	if (QueryPolygon(ray, nPolygonIndex) == false)
		return false;
	outPlane = GetPolygon(nPolygonIndex)->GetPlane();
	return QueryNearestEdges(outPlane, ray, outPos, outPosOnEdge, outEdges);
}

bool Model::QueryNearestEdge(
  int nPolygonIndex,
  const BrushRay& ray,
  BrushVec3& outPos,
  BrushVec3& outPosOnEdge,
  BrushPlane& outPlane,
  BrushEdge3D& outEdge) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	PolygonPtr pPolygon(m_Polygons[m_ShelfID][nPolygonIndex]);
	outPlane = pPolygon->GetPlane();

	if (!outPlane.HitTest(ray, NULL, &outPos))
		return false;

	BrushVec3 posOnEdge;
	BrushEdge3D edge;
	if (pPolygon->QueryNearestEdge(outPos, edge, posOnEdge) == false)
		return false;

	outEdge = edge;
	outPosOnEdge = posOnEdge;

	return true;
}

bool Model::QueryNearestEdge(
  int nPolygonIndex,
  const BrushVec3& position,
  BrushVec3& outPosOnEdge,
  BrushPlane& outPlane,
  BrushEdge3D& outEdge) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	PolygonPtr pPolygon(m_Polygons[m_ShelfID][nPolygonIndex]);
	outPlane = pPolygon->GetPlane();

	BrushVec3 posOnEdge;
	BrushEdge3D edge;
	if (pPolygon->QueryNearestEdge(position, edge, posOnEdge) == false)
		return false;

	outEdge = edge;
	outPosOnEdge = posOnEdge;

	return true;
}

void Model::QueryAdjacentPerpendicularPolygons(
  PolygonPtr pPolygon,
  std::vector<PolygonPtr>& outPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return;

	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		PolygonPtr pCandidatePolygon = m_Polygons[m_ShelfID][i];
		if (std::abs(pPolygon->GetPlane().Normal().Dot(pCandidatePolygon->GetPlane().Normal())) > kDesignerEpsilon)
			continue;
		for (int a = 0, iCandidateEdgeCount(pCandidatePolygon->GetEdgeCount()); a < iCandidateEdgeCount; ++a)
		{
			BrushEdge3D candidateEdge3D = pCandidatePolygon->GetEdge(a);
			if (pPolygon->IsEdgeOverlappedByBoundary(candidateEdge3D))
			{
				outPolygons.push_back(pCandidatePolygon);
				break;
			}
		}
	}
}

void Model::QueryPerpendicularPolygons(
  PolygonPtr pPolygon,
  std::vector<PolygonPtr>& outPolygons,
  BrushVec3* pNormal) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return;

	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		PolygonPtr pCandidatePolygon = m_Polygons[m_ShelfID][i];
		if (pCandidatePolygon == pPolygon || pCandidatePolygon->IsOpen())
			continue;

		BrushVec3 normal = pNormal ? *pNormal : pPolygon->GetPlane().Normal();

		BrushFloat normalDot = std::abs(normal.Dot(pCandidatePolygon->GetPlane().Normal()));
		if (normalDot > kDesignerEpsilon)
			continue;

		for (int a = 0, iVertexCount(pPolygon->GetVertexCount()); a < iVertexCount; ++a)
		{
			const BrushVec3& v = pPolygon->GetPos(a);
			BrushFloat fAbsDist = std::abs(pCandidatePolygon->GetPlane().Distance(v));
			if (fAbsDist < kDistanceLimitation)
			{
				outPolygons.push_back(pCandidatePolygon);
				break;
			}
		}
	}
}

void Model::Clear()
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	m_Polygons[m_ShelfID].clear();
	InvalidateAABB();
}

void Model::DeletePolygons(std::set<PolygonPtr>& deletedPolygons)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!deletedPolygons.empty())
	{
		auto ii = m_Polygons[m_ShelfID].begin();
		for (; ii != m_Polygons[m_ShelfID].end(); )
		{
			if (deletedPolygons.find(*ii) != deletedPolygons.end())
				ii = m_Polygons[m_ShelfID].erase(ii);
			else
				++ii;
		}
		InvalidateAABB(m_ShelfID);
	}
}

bool Model::GetPolygonPlane(
  int nPolygonIndex,
  BrushPlane& outPlane) const
{
	PolygonPtr pPolygon(GetPolygonPtr(nPolygonIndex));
	if (pPolygon == NULL)
		return false;

	outPlane = pPolygon->GetPlane();
	return true;
}

void Model::Serialize(
  XmlNodeRef& xmlNode,
  bool bLoading,
  bool bUndo)
{
	if (bLoading)
	{
		int childNum = xmlNode->getChildCount();

		m_Polygons[0].clear();
		m_Polygons[1].clear();
		m_Polygons[0].reserve(childNum);

		for (int i = 0; i < childNum; ++i)
		{
			XmlNodeRef pChildNode = xmlNode->getChild(i);
			DESIGNER_ASSERT(pChildNode);
			if (!pChildNode)
				continue;
			if (!strcmp(pChildNode->getTag(), "Polygon") || !strcmp(pChildNode->getTag(), "Region"))
			{
				unsigned int nPolygonFlag = 0;
				pChildNode->getAttr("Flags", nPolygonFlag);
				static const int kBackFaceFlag = BIT(2);
				if (!(nPolygonFlag & kBackFaceFlag))
				{
					PolygonPtr pPolygon = new Polygon;
					pPolygon->Serialize(pChildNode, bLoading, bUndo);
					m_Polygons[0].push_back(pPolygon);
				}
			}
			else if (!strcmp(pChildNode->getTag(), "SmoothingGroups"))
			{
				GetSmoothingGroupMgr()->Serialize(pChildNode, bLoading, bUndo, this);
			}
			else if (!strcmp(pChildNode->getTag(), "SemiSharpCrease"))
			{
				GetEdgeSharpnessMgr()->Serialize(pChildNode, bLoading, bUndo);
			}
			else if (!strcmp(pChildNode->getTag(), "UVIslands"))
			{
				GetUVIslandMgr()->Serialize(pChildNode, bLoading, bUndo, this);
			}
		}

		xmlNode->getAttr("DesignerModeFlags", m_Flags);
		xmlNode->getAttr("SubdivisionLevel", m_SubdivisionLevel);
		xmlNode->getAttr("SubdivisionSmoothingGroup", m_bSmoothingGroupForSubdividedSurfaces);

		BrushVec3 mirrorPlaneNormal;
		BrushFloat mirrorPlaneDistance;
		if (xmlNode->getAttr("MirrorPlaneNormal", mirrorPlaneNormal) && xmlNode->getAttr("MirrorPlaneDistance", mirrorPlaneDistance))
			m_MirrorPlane = BrushPlane(mirrorPlaneNormal, mirrorPlaneDistance);

		InvalidateAABB();
	}
	else
	{
		for (int i = 0, iSize(m_Polygons[0].size()); i < iSize; ++i)
		{
			if (m_Polygons[0][i])
			{
				XmlNodeRef polygonNode(xmlNode->newChild("Polygon"));
				m_Polygons[0][i]->Serialize(polygonNode, bLoading, bUndo);
			}
			else
			{
				DESIGNER_ASSERT(0);
			}
		}

		XmlNodeRef smoothingGroupsNode(xmlNode->newChild("SmoothingGroups"));
		GetSmoothingGroupMgr()->Serialize(smoothingGroupsNode, bLoading, bUndo, this);

		XmlNodeRef semiSharpCreaseNode(xmlNode->newChild("SemiSharpCrease"));
		GetEdgeSharpnessMgr()->Serialize(semiSharpCreaseNode, bLoading, bUndo);

		XmlNodeRef uvIslandNode(xmlNode->newChild("UVIslands"));
		GetUVIslandMgr()->Serialize(uvIslandNode, bLoading, bUndo, this);

		xmlNode->setAttr("DesignerModeFlags", m_Flags);
		xmlNode->setAttr("SubdivisionLevel", m_SubdivisionLevel);
		xmlNode->setAttr("SubdivisionSmoothingGroup", m_bSmoothingGroupForSubdividedSurfaces);
		xmlNode->setAttr("MirrorPlaneNormal", m_MirrorPlane.Normal());
		xmlNode->setAttr("MirrorPlaneDistance", m_MirrorPlane.Distance());
	}
}

PolygonPtr Model::GetPolygon(int nPolygonIndex) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (nPolygonIndex < 0 || nPolygonIndex >= m_Polygons[m_ShelfID].size())
		return NULL;
	return m_Polygons[m_ShelfID][nPolygonIndex];
}

int Model::GetPolygonIndex(PolygonPtr pPolygon) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		if (m_Polygons[m_ShelfID][i] == pPolygon)
			return i;
	}

	return -1;
}

EResultOfFindingOppositePolygon Model::QueryOppositePolygon(
  PolygonPtr pPolygon,
  EFindOppositeFlag nFlag,
  BrushFloat fScale,
  PolygonPtr& outPolygon,
  BrushFloat& outDistance) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	if (!pPolygon)
		return eFindOppositePolygon_None;

	BrushPlane plane(pPolygon->GetPlane());
	BrushPlane invertedPlane(plane);
	invertedPlane.Invert();

	BrushVec3 centerOfPolygon(pPolygon->GetCenterPosition());

	BrushFloat nearestDistance(3e10f);
	PolygonPtr pNearestPolygon;

	for (int i = 0, nPolygonSize(m_Polygons[m_ShelfID].size()); i < nPolygonSize; ++i)
	{
		const BrushPlane& oppositePlane(m_Polygons[m_ShelfID][i]->GetPlane());

		if (oppositePlane.IsEquivalent(plane) || oppositePlane.IsEquivalent(invertedPlane))
			continue;

		if (!m_Polygons[m_ShelfID][i]->IsValid() || m_Polygons[m_ShelfID][i]->IsOpen())
			continue;

		BrushVec3 v(m_Polygons[m_ShelfID][i]->GetPos(0));
		BrushFloat dist(0);
		if (!plane.HitTest(BrushRay(v, oppositePlane.Normal()), &dist))
			continue;

		if (std::abs(fScale) < kDesignerEpsilon)
		{
			if (oppositePlane.Normal().Dot(plane.Normal()) > -kDesignerEpsilon)
				continue;

			if (nFlag == eFOF_PushDirection)
			{
				if (dist > 0)
					continue;
			}
			else
			{
				if (dist < 0)
					continue;
			}
		}
		else
		{
			if (oppositePlane.Normal().Dot(plane.Normal()) < 1 - kDesignerEpsilon)
				continue;

			if (nFlag == eFOF_PushDirection)
			{
				if (dist < 0)
					continue;
			}
			else
			{
				if (dist > 0)
					continue;
			}
		}

		PolygonPtr pIntersectionPolygon = m_Polygons[m_ShelfID][i]->Clone();

		if (!pIntersectionPolygon->UpdatePlane(pPolygon->GetPlane()))
			continue;

		if (std::abs(fScale) > kDesignerEpsilon)
		{
			if (!pIntersectionPolygon->Scale(-fScale))
				continue;
		}

		pIntersectionPolygon->Intersect(pPolygon, eICEII_IncludeCoSame);
		if (pIntersectionPolygon->IsValid() == false)
			continue;

		BrushVec3 direction = (nFlag == eFOF_PushDirection) ? pPolygon->GetPlane().Normal() : -pPolygon->GetPlane().Normal();
		BrushFloat distance(0);
		if (!pIntersectionPolygon->UpdatePlane(m_Polygons[m_ShelfID][i]->GetPlane(), direction))
			continue;
		distance = pPolygon->GetNearestDistance(pIntersectionPolygon, direction);

		if (distance >= 0 && distance < nearestDistance)
		{
			nearestDistance = distance;
			pNearestPolygon = pIntersectionPolygon;
		}
	}

	if (nearestDistance == 3e10f)
	{
		outPolygon = NULL;
		return eFindOppositePolygon_None;
	}

	outDistance = nearestDistance;
	outPolygon = pNearestPolygon;

	if (std::abs(outDistance) < kDesignerEpsilon * 10.0f)
		return eFindOppositePolygon_ZeroDistance;

	if (std::abs(plane.Normal().Dot(outPolygon->GetPlane().Normal())) < 1 - kDesignerEpsilon)
		outDistance = nearestDistance - 0.01f;

	return eFindOppositePolygon_Intersection;
}

void Model::QueryIntersectionByPolygon(
  PolygonPtr pPolygon,
  std::vector<PolygonPtr>& outIntersetionPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	if (!pPolygon)
		return;

	AABB bb = pPolygon->GetExpandedBoundBox();

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->CheckFlags(ePolyFlag_Mirrored))
			continue;
		if (!m_Polygons[m_ShelfID][i]->GetExpandedBoundBox().IsIntersectBox(bb))
			continue;
		if (!m_Polygons[m_ShelfID][i]->GetPlane().IsEquivalent(pPolygon->GetPlane()))
			continue;
		if (Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pPolygon) == eIT_Intersection)
			outIntersetionPolygons.push_back(m_Polygons[m_ShelfID][i]);
	}
}

void Model::QueryIntersectionPolygonsWith2DRect(
  IDisplayViewport* pView,
  const BrushMatrix34& worldTM,
  PolygonPtr p2DRectPolygonOnScreenSpace,
  bool bExcludeBackFace,
  PolygonList& outIntersectionPolygons) const
{
	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = m_Polygons[m_ShelfID][i];
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored | ePolyFlag_Hidden))
			continue;
		if (pPolygon->IsIntersectedBy2DRect(pView, worldTM, p2DRectPolygonOnScreenSpace, bExcludeBackFace))
			outIntersectionPolygons.push_back(pPolygon);
	}
}

void Model::QueryIntersectionEdgesWith2DRect(
  IDisplayViewport* pView,
  const BrushMatrix34& worldTM,
  PolygonPtr p2DRectPolygonOnScreenSpace,
  bool bExcludeBackFace,
  EdgeQueryResult& outIntersectionEdges) const
{
	for (int i = 0, iPolygonCount(m_Polygons[m_ShelfID].size()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = m_Polygons[m_ShelfID][i];
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored | ePolyFlag_Hidden))
			continue;
		pPolygon->QueryIntersectionEdgesWith2DRect(
		  pView,
		  worldTM,
		  p2DRectPolygonOnScreenSpace,
		  bExcludeBackFace,
		  outIntersectionEdges);
	}
}

void Model::QueryIntersectionByEdge(
  const BrushEdge3D& edge,
  std::vector<IntersectionPairBetweenTwoEdges>& outIntersections) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	std::map<BrushFloat, IntersectionPairBetweenTwoEdges> sortedIntersections;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->CheckFlags(ePolyFlag_Mirrored))
			continue;
		std::map<BrushFloat, BrushVec3> polygonIntersections;
		m_Polygons[m_ShelfID][i]->QueryIntersectionsByEdge(edge, polygonIntersections);

		auto ii = polygonIntersections.begin();
		for (; ii != polygonIntersections.end(); ++ii)
		{
			auto iSorted = sortedIntersections.begin();
			bool bIdenticalExist = false;
			for (; iSorted != sortedIntersections.end(); ++iSorted)
			{
				const IntersectionPairBetweenTwoEdges& intersection = iSorted->second;
				if (Comparison::IsEquivalent(intersection.second, ii->second))
				{
					bIdenticalExist = true;
					break;
				}
			}
			if (!bIdenticalExist)
				sortedIntersections[ii->first] = IntersectionPairBetweenTwoEdges(m_Polygons[m_ShelfID][i], ii->second);
		}
	}

	auto iter = sortedIntersections.begin();
	for (; iter != sortedIntersections.end(); ++iter)
		outIntersections.push_back(iter->second);
}

void Model::Translate(const BrushVec3& offset)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		m_Polygons[m_ShelfID][i]->Translate(offset);
		GetSmoothingGroupMgr()->InvalidateSmoothingGroup(m_Polygons[m_ShelfID][i]);
	}

	InvalidateAABB(m_ShelfID);
}

void Model::Transform(const BrushMatrix34& tm)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		m_Polygons[m_ShelfID][i]->Transform(tm);
		GetSmoothingGroupMgr()->InvalidateSmoothingGroup(m_Polygons[m_ShelfID][i]);
	}

	InvalidateAABB(m_ShelfID);
}

void Model::GetPolygonList(
  std::vector<PolygonPtr>& outPolygonList,
  bool bIncludeOpenPolygons) const
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);
	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		if (!bIncludeOpenPolygons && m_Polygons[m_ShelfID][i]->IsOpen())
			continue;
		outPolygonList.push_back(m_Polygons[m_ShelfID][i]);
	}
}

void Model::MovePolygonsBetweenShelves(
  ShelfID sourceShelfID,
  ShelfID destShelfID)
{
	for (int i = 0, iPolygonCount(m_Polygons[sourceShelfID].size()); i < iPolygonCount; ++i)
		m_Polygons[destShelfID].push_back(m_Polygons[sourceShelfID][i]);
	m_Polygons[sourceShelfID].clear();
	InvalidateAABB();
}

void Model::MovePolygonBetweenShelves(
  PolygonPtr polygon,
  ShelfID sourceShelfID,
  ShelfID destShelfID)
{
	MODEL_SHELF_RECONSTRUCTOR(this);
	SetShelf(sourceShelfID);
	if (!RemovePolygon(polygon))
		return;
	SetShelf(destShelfID);
	AddPolygon(polygon, false);
	InvalidateAABB();
}

bool Model::SplitPolygonsByOpenPolygon(PolygonPtr pOpenPolygon)
{
	DESIGNER_ASSERT(m_ShelfID >= 0 && m_ShelfID < cShelfMax);

	std::set<PolygonPtr> spannedPolygons;
	for (int i = 0, iSize(m_Polygons[m_ShelfID].size()); i < iSize; ++i)
	{
		if (m_Polygons[m_ShelfID][i]->IsOpen())
			continue;
		EIntersectionType intersectionType = Polygon::HasIntersection(m_Polygons[m_ShelfID][i], pOpenPolygon);
		if (intersectionType == eIT_Intersection)
			spannedPolygons.insert(m_Polygons[m_ShelfID][i]);
	}

	if (spannedPolygons.empty())
	{
		AddPolygon(pOpenPolygon->Clone());
		return true;
	}

	for (auto ii = m_Polygons[m_ShelfID].begin(); ii != m_Polygons[m_ShelfID].end(); )
	{
		if (spannedPolygons.find(*ii) != spannedPolygons.end())
			ii = m_Polygons[m_ShelfID].erase(ii);
		else
			++ii;
	}

	PolygonPtr pRearrangedOpenPolygon(pOpenPolygon->Clone());
	pRearrangedOpenPolygon->Rearrange();

	auto iPolygon = spannedPolygons.begin();
	for (; iPolygon != spannedPolygons.end(); ++iPolygon)
	{
		PolygonPtr pSpannedPolygon = (*iPolygon);
		PolygonPtr pSpannedPolygonWithoutBridgeEdges = (*iPolygon)->Clone();
		pSpannedPolygonWithoutBridgeEdges->RemoveBridgeEdges();

		bool bHasHoles = pSpannedPolygonWithoutBridgeEdges->HasHoles();

		PolygonPtr pCulledOpenPolygon(pRearrangedOpenPolygon->Clone());
		pCulledOpenPolygon->ClipOutside(pSpannedPolygon);

		if (!pCulledOpenPolygon->IsValid())
			continue;

		std::vector<PolygonPtr> isolatedPaths;
		pCulledOpenPolygon->GetIsolatedPaths(isolatedPaths);

		for (int i = 0, iUnconnectedPolygonSize(isolatedPaths.size()); i < iUnconnectedPolygonSize; ++i)
		{
			isolatedPaths[i]->Rearrange();

			const BrushVec3& vFirstVertex = isolatedPaths[i]->GetPos(0);
			const BrushVec3& vLastVertex = isolatedPaths[i]->GetPos(isolatedPaths[i]->GetVertexCount() - 1);

			DESIGNER_ASSERT(pSpannedPolygon->IsPositionOnBoundary(vFirstVertex));
			DESIGNER_ASSERT(pSpannedPolygon->IsPositionOnBoundary(vLastVertex));

			int nNewVtxIdx[2] = { -1, -1 };
			int nNewEdgeIdx[2] = { -1, -1 };

			std::set<int> newEdgeIndexCandidates[2];
			pSpannedPolygon->InsertVertex(vFirstVertex, &nNewVtxIdx[0], &newEdgeIndexCandidates[0]);
			pSpannedPolygon->InsertVertex(vLastVertex, &nNewVtxIdx[1], &newEdgeIndexCandidates[1]);

			for (int k = 0; k < 2; ++k)
			{
				if (newEdgeIndexCandidates[k].size() > 1)
				{
					EdgeIndexSet secondIndices;
					auto iter = newEdgeIndexCandidates[k].begin();
					for (; iter != newEdgeIndexCandidates[k].end(); ++iter)
						secondIndices.insert(pSpannedPolygon->GetEdgeIndexPair(*iter).m_i[1]);
					nNewEdgeIdx[k] = pSpannedPolygon->ChooseNextEdge(IndexPair(nNewVtxIdx[1 - k], nNewVtxIdx[k]), secondIndices);
				}
				else if (newEdgeIndexCandidates[k].size() == 1)
				{
					nNewEdgeIdx[k] = *newEdgeIndexCandidates[k].begin();
				}
				else
				{
					DESIGNER_ASSERT(0);
					return false;
				}
			}

			int iVertexSize = isolatedPaths[i]->GetVertexCount();

			std::vector<BrushVec3> vList;
			ETrackingResult result = pSpannedPolygon->TrackVertices(nNewEdgeIdx[0], nNewEdgeIdx[1], vList);

			if (result == eTrackingResult_FinishAtEndEdge)
			{
				for (int a = 0; a < 2; ++a)
				{
					if (a == 0)
					{
						for (int k = iVertexSize - 2; k >= 1; --k)
							vList.push_back(isolatedPaths[i]->GetPos(k));
					}
					else
					{
						vList.clear();
						result = pSpannedPolygon->TrackVertices(nNewEdgeIdx[1], nNewEdgeIdx[0], vList);
						DESIGNER_ASSERT(result == eTrackingResult_FinishAtEndEdge);
						for (int k = 1; k < iVertexSize - 1; ++k)
							vList.push_back(isolatedPaths[i]->GetPos(k));
					}

					PolygonPtr pNewPolygon = new Polygon(vList, pSpannedPolygon->GetPlane(), pSpannedPolygon->GetSubMatID(), &pSpannedPolygon->GetTexInfo(), true);
					pNewPolygon->SetFlag(pSpannedPolygon->GetFlag());
					if (!pNewPolygon->IsQuad())
						pNewPolygon->RemoveFlags(ePolyFlag_NonplanarQuad);
					if (pSpannedPolygon->CheckFlags(ePolyFlag_NonplanarQuad))
					{
						BrushPlane plane;
						if (pNewPolygon->GetComputedPlane(plane))
							pNewPolygon->SetPlane(plane);
					}

					if (pNewPolygon->IsValid())
					{
						if (bHasHoles)
							pNewPolygon->Intersect(pSpannedPolygonWithoutBridgeEdges);
						if (pNewPolygon->IsValid() && !pNewPolygon->IsOpen())
							AddPolygon(pNewPolygon);
					}
				}
			}
			else if (result == eTrackingResult_FinishAtStartEdge)
			{
				for (int k = 0; k < iVertexSize - 1; ++k)
				{
					const BrushVec3& vertex = isolatedPaths[i]->GetPos(k);
					const BrushVec3& nextVertex = isolatedPaths[i]->GetPos(k + 1);
					pSpannedPolygon->AddEdge(BrushEdge3D(vertex, nextVertex));
					pSpannedPolygon->AddEdge(BrushEdge3D(nextVertex, vertex));
				}
				bool bNonPlanarBefore = pSpannedPolygon->CheckFlags(ePolyFlag_NonplanarQuad);
				if (!pSpannedPolygon->IsQuad())
					pSpannedPolygon->RemoveFlags(ePolyFlag_NonplanarQuad);
				if (bNonPlanarBefore)
				{
					BrushPlane plane;
					if (pSpannedPolygon->GetComputedPlane(plane))
						pSpannedPolygon->SetPlane(plane);
				}

				if (i == iUnconnectedPolygonSize - 1)
					AddPolygon(pSpannedPolygon->Clone());
			}
		}
	}

	InvalidateAABB();
	return true;
}

PolygonPtr Model::QueryEquivalentPolygon(
  PolygonPtr pPolygon,
  int* pOutPolygonIndex) const
{
	return QueryEquivalentPolygon(m_ShelfID, pPolygon, pOutPolygonIndex);
}

PolygonPtr Model::QueryEquivalentPolygon(
  int nShelfID,
  PolygonPtr pPolygon,
  int* pOutPolygonIndex) const
{
	int iPolygonSize(m_Polygons[nShelfID].size());
	for (int i = 0; i < iPolygonSize; ++i)
	{
		if (m_Polygons[nShelfID][i] && m_Polygons[nShelfID][i] == pPolygon)
		{
			if (pOutPolygonIndex)
				*pOutPolygonIndex = i;
			return pPolygon;
		}
	}

	for (int i = 0; i < iPolygonSize; ++i)
	{
		if (m_Polygons[nShelfID][i] && m_Polygons[nShelfID][i]->IsEquivalent(pPolygon))
		{
			if (pOutPolygonIndex)
				*pOutPolygonIndex = i;
			return m_Polygons[nShelfID][i];
		}
	}
	return NULL;
}

void Model::AddPolygonAfterResetingUV(
  int nShelf,
  PolygonPtr pPolygon)
{
	bool bEquivalentExists = QueryEquivalentPolygon(nShelf, pPolygon) ? true : false;
	DESIGNER_ASSERT(!bEquivalentExists);
	DESIGNER_ASSERT(pPolygon->IsValid());
	if (!bEquivalentExists && pPolygon->IsValid())
	{
		pPolygon->ResetUVs();
		m_Polygons[nShelf].push_back(pPolygon);
		InvalidateAABB();
	}
}

EClipPolygonResult Model::Clip(
  const BrushPlane& clipPlane,
  bool bFillFacet,
  _smart_ptr<Model>& pOutFrontPart,
  _smart_ptr<Model>& pOutBackPart) const
{
	EClipPolygonResult clipResult = eCPR_SUCCESSED;

	_smart_ptr<Model> pFrontPart = new Model;
	_smart_ptr<Model> pBackPart = new Model;

	pFrontPart->m_Flags = pBackPart->m_Flags = m_Flags;
	std::vector<BrushEdge3D> facetEdges;

	for (int i = 0, iPolygonSize(m_Polygons[m_ShelfID].size()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygonWithoutBridges;
		if (m_Polygons[m_ShelfID][i]->HasBridgeEdges())
		{
			pPolygonWithoutBridges = m_Polygons[m_ShelfID][i]->Clone();
			pPolygonWithoutBridges->RemoveBridgeEdges();
		}
		else
		{
			pPolygonWithoutBridges = m_Polygons[m_ShelfID][i];
		}

		std::vector<PolygonPtr> pFrontPolygons;
		std::vector<PolygonPtr> pBackPolygons;
		int vertexCount = m_Polygons[m_ShelfID][i]->GetVertexCountAbovePlane(clipPlane);
		if (vertexCount == m_Polygons[m_ShelfID][i]->GetVertexCount())
			pFrontPolygons.push_back(m_Polygons[m_ShelfID][i]);
		else if (vertexCount == 0)
			pBackPolygons.push_back(m_Polygons[m_ShelfID][i]);
		else if (!m_Polygons[m_ShelfID][i]->ClipByPlane(clipPlane, pFrontPolygons, pBackPolygons, &facetEdges))
			continue;

		for (int k = 0; k < pFrontPolygons.size(); ++k)
		{
			if (!pFrontPart->QueryEquivalentPolygon(pFrontPolygons[k]))
				pFrontPart->AddPolygon(pFrontPolygons[k], false);
		}

		for (int k = 0; k < pBackPolygons.size(); ++k)
		{
			if (!pBackPart->QueryEquivalentPolygon(pBackPolygons[k]))
				pBackPart->AddPolygon(pBackPolygons[k], false);
		}
	}

	if (bFillFacet)
	{
		std::vector<PolygonPtr> facetPolygons;
		if (GeneratePolygonsFromEdgeList(facetEdges, clipPlane, facetPolygons))
		{
			for (int i = 0, iPolygonSize(facetPolygons.size()); i < iPolygonSize; ++i)
			{
				PolygonPtr pFacetPolygon = facetPolygons[i]->Clone();

				DESIGNER_ASSERT(!pFacetPolygon->IsOpen());
				if (pFacetPolygon->IsOpen())
					continue;

				pFacetPolygon->ResetUVs();

				if (!pBackPart->QueryEquivalentPolygon(pFacetPolygon))
					pBackPart->AddPolygon(pFacetPolygon, false);
				if (!pFrontPart->QueryEquivalentPolygon(pFacetPolygon))
					pFrontPart->AddPolygon(pFacetPolygon->Clone()->Flip(), false);
			}
		}
		else
		{
			clipResult = eCPR_CLIPSUCCESSED_BUT_FAILED_FILLFACET;
		}
	}

	pOutFrontPart = NULL;
	pOutBackPart = NULL;

	if (pFrontPart->GetPolygonCount() > 0)
		pOutFrontPart = pFrontPart;

	if (pBackPart->GetPolygonCount() > 0)
		pOutBackPart = pBackPart;

	return pOutFrontPart || pOutBackPart ? clipResult : eCPR_FAILED_CLIP;
}

bool Model::GeneratePolygonsFromEdgeList(
  std::vector<BrushEdge3D>& edgeList,
  const BrushPlane& plane,
  std::vector<PolygonPtr>& outPolygons)
{
	if (edgeList.empty())
		return false;

	std::set<int> usedIndices;
	int nEdgeCount(edgeList.size());
	BrushEdge3D currentEdge(edgeList[0]);

	std::vector<PolygonPtr> facetPolygons;
	PolygonPtr pFacetPolygon = new Polygon;
	pFacetPolygon->AddEdge(currentEdge);
	usedIndices.insert(0);
	facetPolygons.push_back(pFacetPolygon);

	while (usedIndices.size() < nEdgeCount)
	{
		bool bFoundNextEdge = false;
		int k = 0;

		for (; k < nEdgeCount; ++k)
		{
			if (usedIndices.find(k) != usedIndices.end())
				continue;
			if (Comparison::IsEquivalent(currentEdge.m_v[1], edgeList[k].m_v[0]))
			{
				currentEdge = edgeList[k];
				bFoundNextEdge = true;
				break;
			}
			else if (Comparison::IsEquivalent(currentEdge.m_v[1], edgeList[k].m_v[1]))
			{
				currentEdge = edgeList[k].GetInverted();
				bFoundNextEdge = true;
				break;
			}
		}

		if (bFoundNextEdge)
		{
			pFacetPolygon->AddEdge(currentEdge);
			usedIndices.insert(k);
		}
		else if (usedIndices.size() < nEdgeCount)
		{
			DESIGNER_ASSERT(!pFacetPolygon->IsOpen());
			if (pFacetPolygon->IsOpen())
				return false;

			pFacetPolygon = new Polygon;
			facetPolygons.push_back(pFacetPolygon);
			for (int i = 0; i < nEdgeCount; ++i)
			{
				if (usedIndices.find(i) == usedIndices.end())
				{
					currentEdge = edgeList[i];
					pFacetPolygon->AddEdge(currentEdge);
					usedIndices.insert(i);
					break;
				}
			}
		}
		else
		{
			DESIGNER_ASSERT(0);
			break;
		}
	}

	int ifacetPolygonsize(facetPolygons.size());

	for (int i = 0; i < ifacetPolygonsize; ++i)
	{
		pFacetPolygon = facetPolygons[i];

		std::vector<Vertex> polygon;
		pFacetPolygon->GetLinkedVertices(polygon);

		BrushFloat fSmallestY = 3e10f;
		int nSmallestIndex = 0;
		int iPtSize(polygon.size());
		for (int k = 0; k < iPtSize; ++k)
		{
			BrushVec2 pos2D = plane.W2P(polygon[k].pos);
			if (fSmallestY > pos2D.y)
			{
				nSmallestIndex = k;
				fSmallestY = pos2D.y;
			}
		}

		pFacetPolygon->SetPlane(BrushPlane(polygon[((nSmallestIndex - 1) + iPtSize) % iPtSize].pos, polygon[nSmallestIndex].pos, polygon[(nSmallestIndex + 1) % iPtSize].pos));

		if (!pFacetPolygon->GetPlane().IsSameFacing(plane))
			pFacetPolygon->Flip();

		pFacetPolygon->SetPlane(plane);
	}

	usedIndices.clear();
	for (int i = 0; i < ifacetPolygonsize; ++i)
	{
		if (usedIndices.find(i) != usedIndices.end())
			continue;
		bool bInclude = false;
		for (int k = 0; k < ifacetPolygonsize; ++k)
		{
			if (i == k)
				continue;
			if (usedIndices.find(k) != usedIndices.end())
				continue;
			if (facetPolygons[i]->IncludeAllEdges(facetPolygons[k]))
			{
				usedIndices.insert(k);
				facetPolygons[i]->Subtract(facetPolygons[k]);
				facetPolygons[k] = NULL;
				bInclude = true;
			}
		}
		if (bInclude)
		{
			usedIndices.insert(i);
		}
	}

	for (int i = 0; i < ifacetPolygonsize; ++i)
	{
		if (facetPolygons[i] == NULL)
			continue;
		outPolygons.push_back(facetPolygons[i]);
	}

	return true;
}

void Model::ResetAllPolygonsFromList(const std::vector<PolygonPtr>& polygonList)
{
	Clear();

	for (int i = 0, iPolygonCount(polygonList.size()); i < iPolygonCount; ++i)
		AddUnionPolygon(polygonList[i]);
}

AABB Model::GetBoundBox(ShelfID nShelf)
{
	MODEL_SHELF_RECONSTRUCTOR(this);

	for (int i = eShelf_Base; i < cShelfMax; ++i)
	{
		if (static_cast<ShelfID>(i) == nShelf || nShelf == eShelf_Any)
		{
			if (m_BoundBox[i].bValid)
			{
				if (i == nShelf)
					return m_BoundBox[i].aabb;
				continue;
			}

			m_BoundBox[i].bValid = true;
			m_BoundBox[i].aabb.Reset();

			SetShelf(static_cast<ShelfID>(i));
			for (int k = 0, iPolygonCount(GetPolygonCount()); k < iPolygonCount; ++k)
				m_BoundBox[i].aabb.Add(GetPolygon(k)->GetBoundBox());

			if (i == nShelf)
				return m_BoundBox[i].aabb;
		}
	}

	AABB aabb;
	aabb.Reset();
	if (nShelf == eShelf_Any)
	{
		aabb.Add(m_BoundBox[0].aabb);
		aabb.Add(m_BoundBox[1].aabb);
		return aabb;
	}

	DESIGNER_ASSERT(0 && "Invalid calling Model::GetBoundBox()");
	return aabb;
}

bool Model::IsEmpty(ShelfID nShelf) const
{
	if (nShelf == eShelf_Any)
		return m_Polygons[0].empty() && m_Polygons[1].empty();

	return m_Polygons[nShelf].empty();
}

bool Model::HasClosedPolygon(ShelfID nShelf) const
{
	for (int i = eShelf_Base; i < cShelfMax; ++i)
	{
		if (static_cast<ShelfID>(i) != nShelf && nShelf != eShelf_Any)
			continue;
		for (int k = 0, iPolygonCount(m_Polygons[i].size()); k < iPolygonCount; ++k)
		{
			if (!m_Polygons[i][k]->IsOpen())
				return true;
		}
	}
	return false;
}

void Model::ResetDB(
  int nFlag,
  ShelfID nValidShelfID)
{
	m_pDB->Reset(this, nFlag, nValidShelfID);
}

void Model::SetSubdivisionResult(HalfEdgeMesh* pSubdividedHalfMesh)
{
	if (m_pSubdividionResult)
	{
		m_pSubdividionResult->Release();
		m_pSubdividionResult = NULL;
	}

	m_pSubdividionResult = pSubdividedHalfMesh;
	if (m_pSubdividionResult)
		m_pSubdividionResult->AddRef();
}

void Model::SetSubMatID(int nSubMatID)
{
	for (int i = 0, iPolygonCount(GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = GetPolygon(i);
		if (pPolygon == NULL)
			continue;
		pPolygon->SetSubMatID(nSubMatID);
	}
}

void Model::SetShelf(ShelfID shelfID) const
{
	assert(shelfID != eShelf_Any);
	m_ShelfID = shelfID;
}

void Model::InvalidateAABB(ShelfID nShelf) const
{
	switch (nShelf)
	{
	case eShelf_Base:
	case eShelf_Construction:
		m_BoundBox[nShelf].bValid = false;
		break;

	case eShelf_Any:
		m_BoundBox[0].bValid = m_BoundBox[1].bValid = false;
		break;
	}
}

void Model::InvalidateSmoothingGroups() const
{
	GetSmoothingGroupMgr()->InvalidateAll();
}
}

