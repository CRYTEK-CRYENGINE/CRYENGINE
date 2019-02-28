// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVCluster.h"
#include "DesignerEditor.h"

namespace Designer {
namespace UVMapping
{

void UVCluster::Update()
{
	if (!IsEmpty())
		return;

	UVElementSetPtr pUVElementSet = m_pElementSet == NULL ? GetUVEditor()->GetElementSet() : m_pElementSet;
	if (pUVElementSet->IsEmpty())
		return;

	std::set<UVIslandPtr> uvIslands;
	for (int k = 0, iElementCount(pUVElementSet->GetCount()); k < iElementCount; ++k)
	{
		const UVElement& element = pUVElementSet->GetElement(k);

		if (element.IsUVIsland())
		{
			m_uvIslandCluster.insert(element.m_pUVIsland);
		}
		else
		{
			for (int i = 0, iCount(element.m_UVVertices.size()); i < iCount; ++i)
			{
				const Vec2& uv = element.m_UVVertices[i].GetUV();
				m_uvCluster[uv].uv = uv;
				m_uvCluster[uv].uvSet.insert(element.m_UVVertices[i]);
			}
			uvIslands.insert(element.m_pUVIsland);
		}
	}

	auto uvIsland = uvIslands.begin();
	for (; uvIsland != uvIslands.end(); ++uvIsland)
	{
		for (int i = 0, iCount((*uvIsland)->GetCount()); i < iCount; ++i)
		{
			PolygonPtr polygon = (*uvIsland)->GetPolygon(i);

			for (int k = 0, iVertexCount(polygon->GetVertexCount()); k < iVertexCount; ++k)
			{
				Vec2 uv = polygon->GetUV(k);
				auto iExisting = m_uvCluster.find(uv);
				if (iExisting != m_uvCluster.end())
					iExisting->second.uvSet.insert(UVVertex(*uvIsland, polygon, k));
			}
		}
	}

	m_AffectedSmoothingGroups.clear();
	FindSmoothingGroupAffected(m_AffectedSmoothingGroups);
	InvalidateSmoothingGroups();

	Shelve();
}

void UVCluster::Transform(const Matrix33& tm, bool bUpdateShelf)
{
	if (IsEmpty())
		Update();

	auto iCluster = m_uvCluster.begin();

	for (; iCluster != m_uvCluster.end(); ++iCluster)
	{
		Vec3 transformedPos = tm.TransformVector(Vec3(iCluster->second.uv.x, iCluster->second.uv.y, 1.0f));
		auto iVertex = iCluster->second.uvSet.begin();
		for (; iVertex != iCluster->second.uvSet.end(); ++iVertex)
		{
			(*iVertex).pPolygon->SetUV((*iVertex).vIndex, Vec2(transformedPos.x, transformedPos.y));
			(*iVertex).pUVIsland->Invalidate();
		}
		iCluster->second.uv.x = transformedPos.x;
		iCluster->second.uv.y = transformedPos.y;
	}

	auto iIsland = m_uvIslandCluster.begin();
	for (; iIsland != m_uvIslandCluster.end(); ++iIsland)
		(*iIsland)->Transform(tm);

	InvalidateSmoothingGroups();

	if (m_bShelf && bUpdateShelf)
		GetUVEditor()->CompileShelf(eShelf_Construction);
}

void UVCluster::AlignToAxis(EPrincipleAxis axis, float value)
{
	if (IsEmpty())
		Update();

	auto iCluster = m_uvCluster.begin();
	for (; iCluster != m_uvCluster.end(); ++iCluster)
	{
		Vec3 transformedPos = (axis == ePrincipleAxis_X) ? Vec3(iCluster->second.uv.x, value, 1.0f) : Vec3(value, iCluster->second.uv.y, 1.0f);
		auto iVertex = iCluster->second.uvSet.begin();
		for (; iVertex != iCluster->second.uvSet.end(); ++iVertex)
		{
			(*iVertex).pPolygon->SetUV((*iVertex).vIndex, Vec2(transformedPos.x, transformedPos.y));
			(*iVertex).pUVIsland->Invalidate();
		}
		iCluster->second.uv.x = transformedPos.x;
		iCluster->second.uv.y = transformedPos.y;
	}

	InvalidateSmoothingGroups();

	if (m_bShelf)
		GetUVEditor()->CompileShelf(eShelf_Construction);
}

void UVCluster::RemoveUVs()
{
	if (IsEmpty())
		Update();

	auto iter = m_uvCluster.begin();
	for (; iter != m_uvCluster.end(); ++iter)
	{
		auto iUVSet = iter->second.uvSet.begin();
		for (; iUVSet != iter->second.uvSet.end(); ++iUVSet)
			iUVSet->pUVIsland->DeletePolygon(iUVSet->pPolygon);
	}

	InvalidateSmoothingGroups();

	if (m_bShelf)
		GetUVEditor()->CompileShelf(eShelf_Construction);
}

void UVCluster::Flip(const Vec2& pivot, const Vec2& normal)
{
	if (IsEmpty())
		Update();

	SBrushPlane<float> mirrorPlane(normal, -normal.Dot(pivot));

	auto iCluster = m_uvCluster.begin();
	for (; iCluster != m_uvCluster.end(); ++iCluster)
	{
		Vec3 mirroredPos = ToVec3(mirrorPlane.MirrorVertex(ToBrushVec3(iCluster->second.uv)));
		auto iVertex = iCluster->second.uvSet.begin();
		for (; iVertex != iCluster->second.uvSet.end(); ++iVertex)
		{
			(*iVertex).pPolygon->SetUV((*iVertex).vIndex, Vec2(mirroredPos.x, mirroredPos.y));
			(*iVertex).pUVIsland->Invalidate();
		}
		iCluster->second.uv.x = mirroredPos.x;
		iCluster->second.uv.y = mirroredPos.y;
	}

	auto iIsland = m_uvIslandCluster.begin();
	for (; iIsland != m_uvIslandCluster.end(); ++iIsland)
	{
		for (int i = 0, iPolygonCount((*iIsland)->GetCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr polygon = (*iIsland)->GetPolygon(i);
			for (int k = 0, iVertexCount(polygon->GetVertexCount()); k < iVertexCount; ++k)
			{
				BrushVec3 mirroredUV = mirrorPlane.MirrorVertex(ToBrushVec3(polygon->GetUV(k)));
				polygon->SetUV(k, Vec2((float)mirroredUV.x, (float)mirroredUV.y));
			}
		}
	}

	InvalidateSmoothingGroups();

	if (m_bShelf)
		GetUVEditor()->CompileShelf(eShelf_Construction);
}

void UVCluster::GetPolygons(std::set<PolygonPtr>& outPolygons)
{
	if (IsEmpty())
		Update();

	auto iter = m_uvCluster.begin();
	for (; iter != m_uvCluster.end(); ++iter)
	{
		auto iUVSet = iter->second.uvSet.begin();
		for (; iUVSet != iter->second.uvSet.end(); ++iUVSet)
			outPolygons.insert(iUVSet->pPolygon);
	}

	auto iterUVIsland = m_uvIslandCluster.begin();
	for (; iterUVIsland != m_uvIslandCluster.end(); ++iterUVIsland)
	{
		for (int i = 0, iCount((*iterUVIsland)->GetCount()); i < iCount; ++i)
			outPolygons.insert((*iterUVIsland)->GetPolygon(i));
	}
}

void UVCluster::Shelve()
{
	if (!m_bShelf)
		return;

	Designer::Model* pModel = GetUVEditor()->GetModel();
	if (pModel == NULL)
		return;

	std::set<PolygonPtr> polygons;

	auto iCluster = m_uvCluster.begin();
	for (; iCluster != m_uvCluster.end(); ++iCluster)
	{
		auto iUV = iCluster->second.uvSet.begin();
		for (; iUV != iCluster->second.uvSet.end(); ++iUV)
			polygons.insert((*iUV).pPolygon);
	}

	auto iUVIslandCluster = m_uvIslandCluster.begin();
	for (; iUVIslandCluster != m_uvIslandCluster.end(); ++iUVIslandCluster)
	{
		for (int i = 0, iPolygonCount((*iUVIslandCluster)->GetCount()); i < iPolygonCount; ++i)
			polygons.insert((*iUVIslandCluster)->GetPolygon(i));
	}

	auto iPolygon = polygons.begin();
	for (; iPolygon != polygons.end(); ++iPolygon)
		pModel->MovePolygonBetweenShelves(*iPolygon, eShelf_Base, eShelf_Construction);

	GetUVEditor()->ApplyPostProcess();
}

void UVCluster::Unshelve()
{
	if (!m_bShelf)
		return;
	Designer::Model* pModel = GetUVEditor()->GetModel();
	if (pModel == NULL)
		return;
	if (!pModel->IsEmpty(eShelf_Construction))
		pModel->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	GetUVEditor()->ApplyPostProcess();
}

void UVCluster::FindSmoothingGroupAffected(std::set<SmoothingGroupPtr>& outAffectedSmoothingGroups)
{
	if (GetUVEditor()->GetModel())
		return;

	UVElementSetPtr pUVElementSet = m_pElementSet == NULL ? GetUVEditor()->GetElementSet() : m_pElementSet;
	if (pUVElementSet->IsEmpty())
		return;

	SmoothingGroupManager* pSmoothingMgr = GetUVEditor()->GetModel()->GetSmoothingGroupMgr();
	for (auto iUVIsland = m_uvIslandCluster.begin(); iUVIsland != m_uvIslandCluster.end(); ++iUVIsland)
	{
		for (int i = 0, iPolygonCount((*iUVIsland)->GetCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr polygon = (*iUVIsland)->GetPolygon(i);
			SmoothingGroupPtr pSmoothingGroup = pSmoothingMgr->FindSmoothingGroup(polygon);
			if (pSmoothingGroup)
				outAffectedSmoothingGroups.insert(pSmoothingGroup);
		}
	}

	auto ii = m_uvCluster.begin();
	for (; ii != m_uvCluster.end(); ++ii)
	{
		auto iUVSet = ii->second.uvSet.begin();
		for (; iUVSet != ii->second.uvSet.end(); ++iUVSet)
		{
			SmoothingGroupPtr pSmoothingGroup = pSmoothingMgr->FindSmoothingGroup(iUVSet->pPolygon);
			if (pSmoothingGroup)
				outAffectedSmoothingGroups.insert(pSmoothingGroup);
		}
	}
}

void UVCluster::InvalidateSmoothingGroups()
{
	for (auto ii = m_AffectedSmoothingGroups.begin(); ii != m_AffectedSmoothingGroups.end(); ++ii)
		(*ii)->Invalidate();
}

}
}

