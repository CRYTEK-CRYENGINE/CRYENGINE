// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmoothingGroup.h"
#include "Core/Model.h"
#include "Core/ModelDB.h"
#include "Core/Helper.h"

namespace Designer
{
SmoothingGroup::SmoothingGroup() : m_ModelDB(new ModelDB)
{
	Invalidate();
}

SmoothingGroup::SmoothingGroup(const std::vector<PolygonPtr>& polygons) : m_ModelDB(new ModelDB)
{
	Invalidate();
	SetPolygons(polygons);
}

SmoothingGroup::~SmoothingGroup()
{
}

void SmoothingGroup::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel)
{
	if (bLoading)
	{
		for (int i = 0, iCount(pModel->GetPolygonCount()); i < iCount; ++i)
		{
			QString attrStr = QString("Polygon%1").arg(i);
			CryGUID guid;
			if (!xmlNode->getAttr(attrStr.toStdString().c_str(), guid))
				break;
			PolygonPtr pPolygon = pModel->QueryPolygon(guid);
			if (!pPolygon)
				continue;
			AddPolygon(pPolygon);
		}
	}
	else
	{
		for (int i = 0, iPolygonCount(GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr pPolygon = GetPolygon(i);
			QString attrStr = QString("Polygon%1").arg(i);
			xmlNode->setAttr(attrStr.toStdString().c_str(), pPolygon->GetGUID());
		}
	}
}

void SmoothingGroup::SetPolygons(const std::vector<PolygonPtr>& polygons)
{
	m_PolygonSet.clear();
	for (int i = 0, iPolygonCount(polygons.size()); i < iPolygonCount; ++i)
		m_PolygonSet.insert(polygons[i]);
	m_Polygons = polygons;
	Invalidate();
}

void SmoothingGroup::AddPolygon(PolygonPtr pPolygon)
{
	if (m_PolygonSet.find(pPolygon) != m_PolygonSet.end())
		return;
	m_PolygonSet.insert(pPolygon);
	m_Polygons.push_back(pPolygon);
	Invalidate();
}

bool SmoothingGroup::HasPolygon(PolygonPtr pPolygon) const
{
	return m_PolygonSet.find(pPolygon) != m_PolygonSet.end();
}

bool SmoothingGroup::CalculateNormal(const BrushVec3& vPos, BrushVec3& vOutNormal) const
{
	BrushVec3 vNormal(0, 0, 0);
	ModelDB::QueryResult qResult;
	if (m_ModelDB->QueryAsVertex(vPos, qResult) && qResult.size() == 1 && !qResult[0].m_MarkList.empty())
	{
		for (int i = 0, iCount(qResult[0].m_MarkList.size()); i < iCount; ++i)
			vNormal += qResult[0].m_MarkList[i].m_pPolygon->GetPlane().Normal();
		vOutNormal = vNormal.GetNormalized();
		return true;
	}
	return false;
}

int SmoothingGroup::GetPolygonCount() const
{
	return m_Polygons.size();
}

PolygonPtr SmoothingGroup::GetPolygon(int nIndex) const
{
	return m_Polygons[nIndex];
}

std::vector<PolygonPtr> SmoothingGroup::GetAll() const
{
	return m_Polygons;
}

void SmoothingGroup::UpdateMesh(Model* pModel, bool bGenerateBackFaces)
{
	m_ModelDB->Reset(m_Polygons, eDBRF_Vertex);
	m_FlexibleMesh.Clear();

	for (int i = 0, iPolygonCount(GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = GetPolygon(i);
		if (polygon->CheckFlags(ePolyFlag_Hidden) || !pModel->HasPolygon(polygon))
			continue;

		FlexibleMesh mesh;
		CreateMeshFacesFromPolygon(polygon, mesh, bGenerateBackFaces);
		for (int k = 0, iVertexCount(mesh.vertexList.size()); k < iVertexCount; ++k)
			CalculateNormal(mesh.vertexList[k].pos, mesh.normalList[k]);

		m_FlexibleMesh.Join(mesh);
	}
}

void SmoothingGroup::RemovePolygon(PolygonPtr pPolygon)
{
	m_PolygonSet.erase(pPolygon);
	for (auto ii = m_Polygons.begin(); ii != m_Polygons.end(); ++ii)
	{
		if (*ii == pPolygon)
		{
			m_Polygons.erase(ii);
			break;
		}
	}
	Invalidate();
}

const FlexibleMesh& SmoothingGroup::GetFlexibleMesh(Model* pModel, bool bGenerateBackFaces)
{
	if (!bGenerateBackFaces && !m_bValidmesh[0] || bGenerateBackFaces && !m_bValidmesh[1])
	{
		if (bGenerateBackFaces)
			m_bValidmesh[1] = true;
		else
			m_bValidmesh[0] = true;
		UpdateMesh(pModel, bGenerateBackFaces);
	}
	return m_FlexibleMesh;
}
}

