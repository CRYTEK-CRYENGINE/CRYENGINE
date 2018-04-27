// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PolygonMesh.h"
#include "PolygonDecomposer.h"
#include "Material/Material.h"
#include "Material/MaterialManager.h"

namespace Designer
{

PolygonMesh::PolygonMesh()
{
	m_pStatObj = NULL;
	m_pRenderNode = NULL;
	CMaterial* pMat = GetIEditor()->GetMaterialManager()->LoadMaterial("%EDITOR%/Materials/crydesigner_selection");
	if (pMat)
		m_pMaterial = pMat->GetMatInfo();
	else
		m_pMaterial = NULL;
}

PolygonMesh::~PolygonMesh()
{
	ReleaseResources();
}

void PolygonMesh::ReleaseResources()
{
	if (m_pStatObj)
	{
		m_pStatObj->Release();
		m_pStatObj = NULL;
	}

	ReleaseRenderNode();
}

void PolygonMesh::ReleaseRenderNode()
{
	if (m_pRenderNode)
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = NULL;
	}
}

void PolygonMesh::CreateRenderNode()
{
	ReleaseRenderNode();
	m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode(eERType_Brush);
}

void PolygonMesh::SetPolygon(PolygonPtr pPolygon, bool bForce, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask)
{
	if (m_pPolygons.size() == 1 && m_pPolygons[0] == pPolygon && !bForce)
		return;

	std::vector<PolygonPtr> polygons;
	polygons.push_back(pPolygon);

	SetPolygons(polygons, bForce, worldTM, dwRndFlags, nViewDistRatio, nMinSpec, materialLayerMask);
}

void PolygonMesh::SetPolygons(const std::vector<PolygonPtr>& polygonList, bool bForce, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask)
{
	m_pPolygons = polygonList;
	ReleaseResources();

	FlexibleMesh mesh;

	for (int i = 0, iPolygonSize(polygonList.size()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = polygonList[i];
		if (!pPolygon || pPolygon->IsOpen())
			continue;

		PolygonDecomposer decomposer;
		FlexibleMesh triangulatedMesh;
		if (!decomposer.TriangulatePolygon(pPolygon, triangulatedMesh))
			continue;

		mesh.Join(triangulatedMesh);
	}

	if (mesh.IsValid())
		UpdateStatObjAndRenderNode(mesh, worldTM, dwRndFlags, nViewDistRatio, nMinSpec, materialLayerMask);
}

void PolygonMesh::UpdateStatObjAndRenderNode(const FlexibleMesh& mesh, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask)
{
	CreateRenderNode();

	if (!m_pStatObj)
	{
		m_pStatObj = GetIEditor()->Get3DEngine()->CreateStatObj();
		m_pStatObj->AddRef();
	}

	IIndexedMesh* pMesh = m_pStatObj->GetIndexedMesh();
	if (!pMesh)
		return;

	TexInfo texInfo;
	mesh.FillIndexedMesh(pMesh);

	pMesh->Optimize();
	pMesh->RestoreFacesFromIndices();

	m_pStatObj->Invalidate(false);

	Matrix34 identityTM = Matrix34::CreateIdentity();
	m_pRenderNode->SetEntityStatObj(m_pStatObj, &identityTM);

	m_pRenderNode->SetMatrix(worldTM);
	m_pRenderNode->SetRndFlags(dwRndFlags | ERF_RENDER_ALWAYS);
	m_pRenderNode->SetViewDistRatio(nViewDistRatio);
	m_pRenderNode->SetMinSpec(nMinSpec);
	m_pRenderNode->SetMaterialLayers(materialLayerMask);

	ApplyMaterial();
}

void PolygonMesh::SetWorldTM(const Matrix34& worldTM)
{
	SetPolygons(m_pPolygons, true, worldTM);
}

void PolygonMesh::SetMaterial(IMaterial* pMaterial)
{
	m_pMaterial = pMaterial;
	ApplyMaterial();
	if (m_pStatObj)
		m_pStatObj->Invalidate(true);
}

void PolygonMesh::ApplyMaterial()
{
	if (!m_pStatObj || !m_pRenderNode)
		return;

	m_pStatObj->SetMaterial(m_pMaterial);
	m_pRenderNode->SetMaterial(m_pMaterial);
}

};

