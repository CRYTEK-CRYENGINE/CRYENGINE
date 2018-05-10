// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVIsland.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "Core/Model.h"

namespace Designer
{

UVIsland::UVIsland() : m_pUVBoundBox(NULL)
{
	m_Guid = CryGUID::Create();
}

UVIsland::~UVIsland()
{
	if (m_pUVBoundBox)
		delete m_pUVBoundBox;
}

UVIsland::UVIsland(const UVIsland& uvIsland) : m_pUVBoundBox(NULL)
{
	*this = uvIsland;
}

UVIsland& UVIsland::operator=(const UVIsland& uvIsland)
{
	auto ii = uvIsland.m_Polygons.begin();
	for (; ii != uvIsland.m_Polygons.end(); ++ii)
		AddPolygon(new Polygon(**ii));
	m_Guid = uvIsland.m_Guid;
	return *this;
}

void UVIsland::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel)
{
	if (bLoading)
	{
		Clear();
		for (int i = 0, iCount(pModel->GetPolygonCount()); i < iCount; ++i)
		{
			QString attr = QString("Polygon%1").arg(i);
			CryGUID guid;
			if (!xmlNode->getAttr(attr.toStdString().c_str(), guid))
				break;
			PolygonPtr polygon = pModel->QueryPolygon(guid);
			if (polygon)
				m_Polygons.push_back(polygon);
		}
	}
	else
	{
		for (int i = 0, iCount(m_Polygons.size()); i < iCount; ++i)
		{
			QString attr = QString("Polygon%1").arg(i);
			xmlNode->setAttr(attr.toStdString().c_str(), m_Polygons[i]->GetGUID());
		}
	}
}

void UVIsland::AddPolygon(PolygonPtr polygon)
{
	if (HasPolygon(polygon))
		return;
	Invalidate();
	m_Polygons.push_back(polygon);
}

bool UVIsland::HasPolygon(PolygonPtr polygon)
{
	auto ii = m_Polygons.begin();
	for (; ii != m_Polygons.end(); ++ii)
	{
		if (*ii == polygon)
			return true;
	}
	return false;
}

void UVIsland::DeletePolygon(PolygonPtr polygon)
{
	bool bDeleted = false;
	auto ii = m_Polygons.begin();
	for (; ii != m_Polygons.end(); )
	{
		if (*ii == polygon)
		{
			bDeleted = true;
			ii = m_Polygons.erase(ii);
		}
		else
		{
			++ii;
		}
	}
	if (bDeleted)
		Invalidate();
}

const AABB& UVIsland::GetUVBoundBox() const
{
	if (m_pUVBoundBox)
		return *m_pUVBoundBox;

	m_pUVBoundBox = new AABB;
	m_pUVBoundBox->Reset();

	for (int i = 0, iPolygonCount(m_Polygons.size()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = m_Polygons[i];
		for (int k = 0, iVertexCount(polygon->GetVertexCount()); k < iVertexCount; ++k)
			m_pUVBoundBox->Add(polygon->GetUV(k));
	}

	m_pUVBoundBox->Expand(Vec3(0.01f, 0.01f, 0.01f));

	return *m_pUVBoundBox;
}

void UVIsland::Invalidate() const
{
	if (!m_pUVBoundBox)
		return;
	delete m_pUVBoundBox;
	m_pUVBoundBox = NULL;
}

void UVIsland::ResetPolygonsInModel(Model* pModel)
{
	for (int k = 0, iPolygonCount(GetCount()); k < iPolygonCount; ++k)
	{
		PolygonPtr polygonInIsland = GetPolygon(k);
		PolygonPtr polygonInModel = pModel->QueryPolygon(polygonInIsland->GetGUID());
		if (polygonInModel && polygonInIsland != polygonInModel)
		{
			*polygonInModel = *polygonInIsland;
			SetPolygon(k, polygonInModel);
		}
	}
}

void UVIsland::ResetPolygonsInUVIslands(Model* pModel)
{
	for (int k = 0, iPolygonCount(GetCount()); k < iPolygonCount; ++k)
	{
		PolygonPtr polygonInIsland = GetPolygon(k);
		PolygonPtr polygonInModel = pModel->QueryPolygon(polygonInIsland->GetGUID());
		if (polygonInModel && polygonInIsland != polygonInModel)
			SetPolygon(k, polygonInModel);
	}
}

void UVIsland::Normalize(const AABB& range)
{
	const AABB& boundbox = GetUVBoundBox();

	float width = boundbox.max.x - boundbox.min.x;
	float height = boundbox.max.y - boundbox.min.y;

	float dest_width = range.max.x - range.min.x;
	float dest_height = range.max.y - range.min.y;

	float ratio = (width > height) ? (dest_width / width) : (dest_height / height);

	float offset_x = (width >= height) ? range.min.x : range.min.x + (dest_width - width * ratio) * 0.5f;
	float offset_y = (width <= height) ? range.min.y : range.min.y + (dest_height - height * ratio) * 0.5f;

	auto iPoly = m_Polygons.begin();
	for (; iPoly != m_Polygons.end(); ++iPoly)
	{
		for (int i = 0, iVertexCount((*iPoly)->GetVertexCount()); i < iVertexCount; ++i)
		{
			Vec2 uv = (*iPoly)->GetUV(i);
			uv.x = (uv.x - boundbox.min.x) * ratio + offset_x;
			uv.y = (uv.y - boundbox.min.y) * ratio + offset_y;
			(*iPoly)->SetUV(i, uv);
		}
	}

	Invalidate();
}

bool UVIsland::HasOverlappedEdges(PolygonPtr polygon) const
{
	for (int i = 0, iPolygonCount(GetCount()); i < iPolygonCount; ++i)
	{
		if (GetPolygon(i)->HasOverlappedEdges(polygon))
			return true;
	}
	return false;
}

void UVIsland::Clean(Model* pModel)
{
	MODEL_SHELF_RECONSTRUCTOR(pModel);

	auto ii = m_Polygons.begin();
	for (; ii != m_Polygons.end(); )
	{
		bool bValidPolygon = false;
		for (int iShelf = eShelf_Base; iShelf < cShelfMax; ++iShelf)
		{
			pModel->SetShelf(static_cast<ShelfID>(iShelf));
			if (pModel->HasPolygon(*ii))
			{
				bValidPolygon = true;
				break;
			}
		}

		if (bValidPolygon)
		{
			++ii;
		}
		else
		{
			(*ii)->ResetUVs();
			ii = m_Polygons.erase(ii);
		}
	}
}

void UVIsland::Transform(const Matrix33& tm)
{
	for (int i = 0, iCount(m_Polygons.size()); i < iCount; ++i)
	{
		PolygonPtr polygon = m_Polygons[i];
		polygon->TransformUV(tm);
	}
	Invalidate();
}

bool UVIsland::IsPicked(const Ray& ray) const
{
	Vec3 out;
	if (Intersect::Ray_AABB(ray, GetUVBoundBox(), out) == 0)
		return false;

	for (auto ii = m_Polygons.begin(); ii != m_Polygons.end(); ++ii)
	{
		if ((*ii)->CheckFlags(ePolyFlag_Hidden))
			continue;

		FlexibleMesh* pMesh = (*ii)->GetTriangles();
		if (pMesh->IsPassedUV(ray))
			return true;
	}

	return false;
}

bool UVIsland::IsOverlapped(const AABB& aabb) const
{
	if (!aabb.IsIntersectBox(GetUVBoundBox()))
		return false;

	for (auto ii = m_Polygons.begin(); ii != m_Polygons.end(); ++ii)
	{
		FlexibleMesh* pMesh = (*ii)->GetTriangles();
		if (pMesh->IsOverlappedUV(aabb))
			return true;
	}

	return false;
}

void UVIsland::Join(UVIsland* pUVIsland)
{
	for (int i = 0, iCount(pUVIsland->GetCount()); i < iCount; ++i)
		m_Polygons.push_back(pUVIsland->GetPolygon(i));

	pUVIsland->Clear();
	Invalidate();
}

UVIsland* UVIsland::Clone() const
{
	UVIsland* pClone = new UVIsland(*this);
	pClone->m_Guid = CryGUID::Create();
	return pClone;
}

int UVIsland::GetSubMatID() const
{
	if (m_Polygons.empty())
		return 0;
	return m_Polygons[0]->GetSubMatID();
}

bool UVIsland::HasSubMatID(int nSubMatID) const
{
	auto ii = m_Polygons.begin();
	for (; ii != m_Polygons.end(); ++ii)
	{
		if ((*ii)->GetSubMatID() == nSubMatID)
			return true;
	}
	return false;
}

bool UVIsland::FindIsolatedAreas(std::vector<UVIslandPtr>& outIsolatedAreas)
{
	if (IsEmpty())
		return false;

	int polygonCount = (int)m_Polygons.size();

	std::vector<bool> usedPolygons(polygonCount, false);
	std::vector<PolygonPtr> seedPolygons;

	seedPolygons.push_back(m_Polygons[0]);
	usedPolygons[0] = true;

	while (!seedPolygons.empty())
	{
		UVIslandPtr pUVIsland = new UVIsland();

		while (!seedPolygons.empty())
		{
			PolygonPtr seed = seedPolygons[seedPolygons.size() - 1];
			seedPolygons.pop_back();
			pUVIsland->AddPolygon(seed);

			for (int i = 0; i < polygonCount; ++i)
			{
				PolygonPtr polygon = m_Polygons[i];
				if (usedPolygons[i])
					continue;

				if (seed->HasOverlappedUVEdges(polygon))
				{
					seedPolygons.push_back(polygon);
					usedPolygons[i] = true;
				}
			}
		}

		if (!pUVIsland->IsEmpty())
			outIsolatedAreas.push_back(pUVIsland);

		for (int i = 0; i < polygonCount; ++i)
		{
			if (usedPolygons[i])
				continue;
			seedPolygons.push_back(m_Polygons[i]);
			break;
		}
	}

	return !outIsolatedAreas.empty();
}

}

