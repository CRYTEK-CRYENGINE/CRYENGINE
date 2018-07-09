// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVMappingEditorCommon.h"
#include "UVMappingEditor.h"
#include "QViewport.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "Core/FlexibleMesh.h"
#include "Util/ExcludedEdgeManager.h"

namespace Designer {
namespace UVMapping
{
void AddUVPolygon(std::vector<UVPolygon>& uvPolygons, PolygonPtr polygon, UVIslandPtr pUVIsland)
{
	std::vector<UVVertex> uvs;
	for (int i = 0, iVertexCount(polygon->GetVertexCount()); i < iVertexCount; ++i)
		uvs.push_back(UVVertex(pUVIsland, polygon, i));
	uvPolygons.push_back(UVPolygon(pUVIsland, uvs));
}

Vec3 SearchUtil::FindHitPointToUVPlane(QViewport* viewport, int x, int y)
{
	Ray ray;
	viewport->ScreenToWorldRay(&ray, x, y);
	SBrushPlane<float> plane(Vec3(0, 0, 1), 0);
	Vec3 vHit;
	plane.HitTest(SBrushRay<float>(ray.origin, ray.direction), 0, &vHit);
	vHit.z = 0;
	return vHit;
}

bool SearchUtil::FindUVIslands(const Ray& ray, std::vector<UVIslandPtr>& outFoundIslands)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	bool bFound = false;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;
		if (pUVIsland->IsPicked(ray))
		{
			outFoundIslands.push_back(pUVIsland);
			bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindUVIslands(const AABB& aabb, std::vector<UVIslandPtr>& outFoundIslands)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	bool bFound = false;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;
		if (pUVIsland->IsOverlapped(aabb))
		{
			outFoundIslands.push_back(pUVIsland);
			bFound = true;
		}
	}
	return bFound;
}

bool SearchUtil::FindPolygons(const Ray& ray, std::vector<UVPolygon>& outFoundPolygons)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	bool bFound = false;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;
		if (FindPolygons(ray, pUVIsland, outFoundPolygons))
			bFound = true;
	}
	return bFound;
}

bool SearchUtil::FindPolygons(const AABB& aabb, std::vector<UVPolygon>& outFoundPolygons)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	bool bFound = false;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;
		if (FindPolygons(aabb, pUVIsland, outFoundPolygons))
			bFound = true;
	}
	return bFound;
}

bool SearchUtil::FindPolygons(const Ray& ray, UVIslandPtr pUVIsland, std::vector<UVPolygon>& outFoundPolygons)
{
	Vec3 out;
	if (Intersect::Ray_AABB(ray, pUVIsland->GetUVBoundBox(), out) == 0)
		return false;

	bool bFound = false;
	for (int i = 0, iCount(pUVIsland->GetCount()); i < iCount; ++i)
	{
		PolygonPtr pPolygon = pUVIsland->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;
		FlexibleMesh* pMesh = pPolygon->GetTriangles();
		if (pMesh->IsPassedUV(ray))
		{
			AddUVPolygon(outFoundPolygons, pPolygon, pUVIsland);
			bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindPolygons(const AABB& aabb, UVIslandPtr pUVIsland, std::vector<UVPolygon>& outFoundPolygons)
{
	if (!aabb.IsIntersectBox(pUVIsland->GetUVBoundBox()))
		return false;

	bool bFound = false;
	for (int i = 0, iCount(pUVIsland->GetCount()); i < iCount; ++i)
	{
		PolygonPtr pPolygon = pUVIsland->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;
		FlexibleMesh* pMesh = pPolygon->GetTriangles();
		if (pMesh->IsOverlappedUV(aabb))
		{
			AddUVPolygon(outFoundPolygons, pPolygon, pUVIsland);
			bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindEdges(const Ray& ray, std::vector<UVEdge>& outFoundEdges)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	std::map<float, UVEdge> sortedEdges;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;
		Vec3 vOut;
		if (Intersect::Ray_AABB(ray, pUVIsland->GetUVBoundBox(), vOut) == 0)
			continue;

		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;
			FindEdges(ray, pUVIsland, polygon, sortedEdges);
		}
	}

	for (auto ii = sortedEdges.begin(); ii != sortedEdges.end(); ++ii)
		outFoundEdges.push_back(ii->second);

	return !sortedEdges.empty();
}

bool SearchUtil::FindEdges(const AABB& aabb, std::vector<UVEdge>& outFoundEdges)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	bool bFound = false;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;

		if (!aabb.IsIntersectBox(pUVIsland->GetUVBoundBox()))
			continue;

		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;

			if (FindEdges(aabb, pUVIsland, pUVIsland->GetPolygon(k), outFoundEdges))
				bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindEdges(const Ray& ray, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::map<float, UVEdge>& outFoundEdges)
{
	bool bFound = false;

	Vec3 vHit;
	SBrushPlane<float> plane(Vec3(0, 0, 1), 0);
	plane.HitTest(SBrushRay<float>(ray.origin, ray.direction), 0, &vHit);
	vHit.z = 0;

	for (int i = 0, iEdgeCount(pPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		const IndexPair& edge = pPolygon->GetEdgeIndexPair(i);

		Vec3 uv0 = pPolygon->GetUV(edge.m_i[0]);
		Vec3 uv1 = pPolygon->GetUV(edge.m_i[1]);

		SBrushEdge3D<float, Vec2> uv_edge(uv0, uv1);
		Vec3 closestPos;
		bool bInsideEdge = false;
		if (!uv_edge.CalculateNearestVertex(vHit, closestPos, bInsideEdge) || !bInsideEdge)
			continue;

		float distance = GetDistanceOnScreen(vHit, closestPos, GetUVEditor()->GetViewport());
		if (distance < 7.0f)
		{
			bFound = true;
			outFoundEdges[distance] = UVEdge(pUVIsland, UVVertex(pUVIsland, pPolygon, edge.m_i[0]), UVVertex(pUVIsland, pPolygon, edge.m_i[1]));
		}
	}

	return bFound;
}

bool SearchUtil::FindEdges(const AABB& aabb, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::vector<UVEdge>& outFoundEdges)
{
	if (!GetUVEditor()->AllIslandsViewed() && pPolygon->GetSubMatID() != GetUVEditor()->GetSubMatID())
		return false;

	bool bFound = false;

	for (int i = 0, iEdgeCount(pPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		const IndexPair& edge = pPolygon->GetEdgeIndexPair(i);

		Lineseg lineSeg(pPolygon->GetUV(edge.m_i[0]), pPolygon->GetUV(edge.m_i[1]));
		Vec3 vHit;
		if (Intersect::Lineseg_AABB(lineSeg, aabb, vHit) != 0)
		{
			outFoundEdges.push_back(UVEdge(pUVIsland, UVVertex(pUVIsland, pPolygon, edge.m_i[0]), UVVertex(pUVIsland, pPolygon, edge.m_i[1])));
			bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindVertices(const Ray& ray, std::vector<UVVertex>& outFoundVertices)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	std::map<float, UVVertex> sortedVertices;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;

		Vec3 vOut;
		if (Intersect::Ray_AABB(ray, pUVIsland->GetUVBoundBox(), vOut) == 0)
			continue;

		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;

			FindVertices(ray, pUVIsland, pUVIsland->GetPolygon(k), sortedVertices);
		}
	}

	for (auto ii = sortedVertices.begin(); ii != sortedVertices.end(); ++ii)
		outFoundVertices.push_back(ii->second);

	return !sortedVertices.empty();
}

bool SearchUtil::FindVertices(const AABB& aabb, std::vector<UVVertex>& outFoundVertices)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	bool bFound = false;
	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		if (!GetUVEditor()->AllIslandsViewed() && GetUVEditor()->GetSubMatID() != pUVIsland->GetSubMatID())
			continue;

		if (!aabb.IsIntersectBox(pUVIsland->GetUVBoundBox()))
			continue;

		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;

			if (FindVertices(aabb, pUVIsland, pUVIsland->GetPolygon(k), outFoundVertices))
				bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindVertices(const Ray& ray, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::map<float, UVVertex>& outFoundVertices)
{
	bool bFound = false;

	Vec3 vHit;
	SBrushPlane<float> plane(Vec3(0, 0, 1), 0);
	plane.HitTest(SBrushRay<float>(ray.origin, ray.direction), 0, &vHit);
	vHit.z = 0;

	for (int i = 0, iVertexCount(pPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		Vec3 uv = pPolygon->GetUV(i);
		float distance = GetDistanceOnScreen(uv, vHit, GetUVEditor()->GetViewport());
		if (distance < 7.0f)
		{
			outFoundVertices[distance] = UVVertex(pUVIsland, pPolygon, i);
			bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindVertices(const AABB& aabb, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::vector<UVVertex>& outFoundVertices)
{
	bool bFound = false;

	for (int i = 0, iVertexCount(pPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		if (aabb.IsContainPoint(pPolygon->GetUV(i)))
		{
			outFoundVertices.push_back(UVVertex(pUVIsland, pPolygon, i));
			bFound = true;
		}
	}

	return bFound;
}

bool SearchUtil::FindPolygonsSharingEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIsland, std::set<PolygonPtr>& outFoundPolygons)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	bool bFound = false;

	for (int i = 0, iUVIslandCount(pUVIslandMgr->GetCount()); i < iUVIslandCount; ++i)
	{
		pUVIsland = pUVIslandMgr->GetUVIsland(i);
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;

			for (int a = 0, iEdgeCount(polygon->GetEdgeCount()); a < iEdgeCount; ++a)
			{
				const IndexPair& e = polygon->GetEdgeIndexPair(a);
				const Vec2& uv0_ = polygon->GetUV(e.m_i[0]);
				const Vec2& uv1_ = polygon->GetUV(e.m_i[1]);
				if ((Comparison::IsEquivalent(uv0, uv0_) && Comparison::IsEquivalent(uv1, uv1_)) || (Comparison::IsEquivalent(uv0, uv1_) && Comparison::IsEquivalent(uv1, uv0_)))
				{
					outFoundPolygons.insert(polygon);
					bFound = true;
					break;
				}
			}
		}
	}

	return bFound;
}

bool SearchUtil::FindUVVerticesFromPos(const BrushVec3& pos, int subMatId, std::vector<UVVertex>& outUVVertices)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	bool bFound = false;

	for (int i = 0, iUVIslandCount(pUVIslandMgr->GetCount()); i < iUVIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;
			if (!GetUVEditor()->AllIslandsViewed() && polygon->GetSubMatID() != subMatId)
				continue;
			int vidx;
			if (polygon->GetVertexIndex(pos, vidx))
			{
				outUVVertices.push_back(UVVertex(pUVIsland, polygon, vidx));
				bFound = true;
			}
		}
	}

	return bFound;
}

bool SearchUtil::FindUVEdgesFromEdge(const BrushEdge3D& edge, int subMatId, std::vector<UVEdge>& outUVEdges)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	bool bFound = false;

	for (int i = 0, iUVIslandCount(pUVIslandMgr->GetCount()); i < iUVIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;
			if (!GetUVEditor()->AllIslandsViewed() && polygon->GetSubMatID() != subMatId)
				continue;
			int edgeIndex = polygon->GetEdgeIndex(edge);
			if (edgeIndex == -1)
				edgeIndex = polygon->GetEdgeIndex(edge.GetInverted());
			if (edgeIndex != -1)
			{
				const IndexPair& edge = polygon->GetEdgeIndexPair(edgeIndex);
				UVVertex uv0(pUVIsland, polygon, edge.m_i[0]);
				UVVertex uv1(pUVIsland, polygon, edge.m_i[1]);
				outUVEdges.push_back(UVEdge(pUVIsland, uv0, uv1));
				bFound = true;
			}
		}
	}

	return bFound;
}

bool SearchUtil::FindUVPolygonsFromPolygon(PolygonPtr inPolygon, int subMatId, std::vector<UVPolygon>& outUVPolygons)
{
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	bool bFound = false;

	for (int i = 0, iUVIslandCount(pUVIslandMgr->GetCount()); i < iUVIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (polygon->CheckFlags(ePolyFlag_Hidden))
				continue;
			if (!GetUVEditor()->AllIslandsViewed() && polygon->GetSubMatID() != subMatId)
				continue;
			if (polygon == inPolygon)
			{
				std::vector<UVVertex> uvVertices;
				for (int a = 0, iVertexCount(polygon->GetVertexCount()); a < iVertexCount; ++a)
					uvVertices.push_back(UVVertex(pUVIsland, polygon, a));
				outUVPolygons.push_back(UVPolygon(pUVIsland, uvVertices));
				bFound = true;
			}
		}
	}

	return bFound;
}

bool SearchUtil::FindConnectedEdges(
  const Vec2& uv0,
  const Vec2& uv1,
  UVIslandPtr pUVIsland,
  std::vector<UVEdge>& outPrevConnectedUVEdges,
  std::vector<UVEdge>& outNextConnectedUVEdges)
{
	bool bAdded = false;

	for (int i = 0, iPolygonCount(pUVIsland->GetCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pUVIsland->GetPolygon(i);
		if (polygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		for (int k = 0, iEdgeCount(polygon->GetEdgeCount()); k < iEdgeCount; ++k)
		{
			const IndexPair& edgeIndexPair = polygon->GetEdgeIndexPair(k);
			SBrushEdge<float> uvEdge(polygon->GetUV(edgeIndexPair.m_i[0]), polygon->GetUV(edgeIndexPair.m_i[1]));

			if (Comparison::IsEquivalent(uvEdge.m_v[0], uv1))
			{
				bAdded = true;
				outNextConnectedUVEdges.push_back(UVEdge(pUVIsland, UVVertex(pUVIsland, polygon, edgeIndexPair.m_i[0]), UVVertex(pUVIsland, polygon, edgeIndexPair.m_i[1])));
			}

			if (Comparison::IsEquivalent(uvEdge.m_v[1], uv0))
			{
				bAdded = true;
				outPrevConnectedUVEdges.push_back(UVEdge(pUVIsland, UVVertex(pUVIsland, polygon, edgeIndexPair.m_i[0]), UVVertex(pUVIsland, polygon, edgeIndexPair.m_i[1])));
			}
		}
	}

	return bAdded;
}

PolygonPtr SearchUtil::FindPolygonContainingEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIsland)
{
	for (int i = 0, iPolygonCount(pUVIsland->GetCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pUVIsland->GetPolygon(i);
		if (polygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		for (int k = 0, iEdgeCount(polygon->GetVertexCount()); k < iEdgeCount; ++k)
		{
			const IndexPair& edgeIndexPair = polygon->GetEdgeIndexPair(k);
			SBrushEdge<float> uvEdge(polygon->GetUV(edgeIndexPair.m_i[0]), polygon->GetUV(edgeIndexPair.m_i[1]));

			if (Comparison::IsEquivalent(uvEdge.m_v[0], uv0) && Comparison::IsEquivalent(uvEdge.m_v[1], uv1))
				return polygon;
		}
	}
	return NULL;
}

void SyncSelection()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	UVElementSetPtr pUVElementSet = GetUVEditor()->GetElementSet();
	ElementSet* pElements = pSession->GetSelectedElements();

	pElements->Clear();
	pSession->GetExcludedEdgeManager()->Clear();

	CBaseObject* pObject = pSession->GetBaseObject();

	for (int i = 0, iCount(pUVElementSet->GetCount()); i < iCount; ++i)
	{
		UVElement& element = pUVElementSet->GetElement(i);
		if (element.IsUVIsland())
		{
			for (int k = 0, iPolygonCount(element.m_pUVIsland->GetCount()); k < iPolygonCount; ++k)
				pElements->Add(Element(pObject, element.m_pUVIsland->GetPolygon(k)));
		}
		else if (element.IsPolygon())
		{
			pElements->Add(Element(pObject, element.m_UVVertices[0].pPolygon));
		}
		else if (element.IsEdge())
		{
			const BrushVec3& v0 = element.m_UVVertices[0].pPolygon->GetPos(element.m_UVVertices[0].vIndex);
			const BrushVec3& v1 = element.m_UVVertices[1].pPolygon->GetPos(element.m_UVVertices[1].vIndex);
			BrushEdge3D edge(v0, v1);
			pElements->Add(Element(pObject, edge));
			pSession->GetExcludedEdgeManager()->Add(edge);
		}
		else if (element.IsVertex())
		{
			const BrushVec3& v = element.m_UVVertices[0].pPolygon->GetPos(element.m_UVVertices[0].vIndex);
			pElements->Add(Element(pObject, v));
		}
	}

	GetUVEditor()->UpdateSharedSelection();

	pSession->UpdateSelectionMeshFromSelectedElements();

	DesignerEditor* pDesignerEditor = GetDesigner();
	if (pDesignerEditor)
	{
		pDesignerEditor->UpdateTMManipulatorBasedOnElements(pElements);
	}
}

void GetAllUVsFromUVIsland(UVIslandPtr pUVIsland, std::set<UVVertex>& outUVs)
{
	for (int i = 0, iPolygonCount(pUVIsland->GetCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pUVIsland->GetPolygon(i);
		for (int i = 0, iVertexCount(polygon->GetVertexCount()); i < iVertexCount; ++i)
			outUVs.insert(UVVertex(pUVIsland, polygon, i));
	}
}
}
}

