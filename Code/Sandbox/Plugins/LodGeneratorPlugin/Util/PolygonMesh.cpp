#include "StdAfx.h"
#include "PolygonMesh.h"
#include "Material/Material.h"
#include "Material/MaterialManager.h"
#include "TopologyTransform.h"

namespace LODGenerator
{

CPolygonMesh::CPolygonMesh()
{
	m_pStatObj = NULL;
	m_pRenderNode = NULL;
	CMaterial* pMat = GetIEditor()->GetMaterialManager()->LoadMaterial("%EDITOR%/Materials/lodgenerator");
	if(pMat)
		m_pMaterial = pMat->GetMatInfo();
	else
		m_pMaterial = NULL;
}

CPolygonMesh::~CPolygonMesh()
{
	ReleaseResources();
}

void CPolygonMesh::ReleaseResources()
{
	if( m_pStatObj )
	{
		m_pStatObj->Release();
		m_pStatObj = NULL;
	}

	ReleaseRenderNode();
}

void CPolygonMesh::ReleaseRenderNode()
{
	if( m_pRenderNode )
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = NULL;
	}
}

void CPolygonMesh::CreateRenderNode()
{
	ReleaseRenderNode();
	m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode(eERType_Brush);
}

void CPolygonMesh::SetPolygon( CTopologyGraph* pPolygon, bool bForce, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask )
{
	m_pPolygon = pPolygon;
	ReleaseResources();

	UpdateStatObjAndRenderNode( pPolygon, worldTM, dwRndFlags, nViewDistRatio, nMinSpec, materialLayerMask );
}




void CPolygonMesh::UpdateStatObjAndRenderNode( CTopologyGraph* pPolygon, const Matrix34& worldTM, int dwRndFlags, int nViewDistRatio, int nMinSpec, uint8 materialLayerMask )
{
	CreateRenderNode();

	if( !m_pStatObj )
	{
		m_pStatObj = GetIEditor()->Get3DEngine()->CreateStatObj();
		m_pStatObj->AddRef();
	}

	LODGenerator::CTopologyTransform::FillMesh( pPolygon, m_pStatObj );

	Matrix34 identityTM = Matrix34::CreateIdentity();
	m_pRenderNode->SetEntityStatObj(m_pStatObj,&identityTM);	

	//m_pStatObj->Invalidate(false);

	m_pRenderNode->SetMatrix(worldTM);
	m_pRenderNode->SetRndFlags(dwRndFlags|ERF_RENDER_ALWAYS);
	m_pRenderNode->SetViewDistRatio(nViewDistRatio);
	m_pRenderNode->SetMinSpec(nMinSpec);
	m_pRenderNode->SetMaterialLayers(materialLayerMask);

	ApplyMaterial();
}

void CPolygonMesh::SetWorldTM( const Matrix34& worldTM )
{
	SetPolygon(m_pPolygon, true, worldTM);
}

void CPolygonMesh::SetMaterial( IMaterial* pMaterial )
{
	m_pMaterial = pMaterial;
	ApplyMaterial();

}

void CPolygonMesh::ApplyMaterial()
{
	if( !m_pStatObj || !m_pRenderNode )
		return;

	m_pStatObj->SetMaterial(m_pMaterial);
	m_pRenderNode->SetMaterial(m_pMaterial);

	if( m_pStatObj )
		m_pStatObj->Invalidate(true);

}

};