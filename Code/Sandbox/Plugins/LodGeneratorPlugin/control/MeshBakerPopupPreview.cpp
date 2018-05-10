#include "StdAfx.h"
#include <Cry3DEngine/IStatObj.h>
#include "MeshBakerPopupPreview.h"
#include "DisplayViewportAdapter.h"
#include "Material/Material.h"


CCMeshBakerPopupPreview::CCMeshBakerPopupPreview(QViewport * viewPort):m_ViewPort(viewPort)
{
	m_pViewportAdapter.reset(new CDisplayViewportAdapter(m_ViewPort));
	m_pObject = NULL;
	m_pMat = NULL;

	m_bPrecacheMaterial = false;
	m_bDrawWireFrame = false;
	m_bRotate = false;
	m_rotateAngle = 0;

	CDLight l;

	float L = 1.0f;
	l.m_fRadius = 10000;
	l.m_Flags |= DLF_SUN | DLF_DIRECTIONAL;
	l.SetLightColor(ColorF(L,L,L,1));
	l.SetPosition( Vec3(100,100,100) );
	m_lights.push_back( l );

	m_bShowObject = true;

	m_clearColor.set(0.5f, 0.5f, 0.5f, 1.0f);
	m_ambientColor.set(1.0f, 1.0f, 1.0f, 1.0f);
	m_ambientMultiplier = 0.5f;
}


void CCMeshBakerPopupPreview::OnKeyEvent(const SKeyEvent& sKeyEvent)
{

}

void CCMeshBakerPopupPreview::OnMouseEvent(const SMouseEvent& sMouseEvent)
{

}



void CCMeshBakerPopupPreview::SetObject(IStatObj * pObj)
{
	m_pObject = pObj;
}

void CCMeshBakerPopupPreview::ReleaseObject()
{
	if (m_pObject != NULL)
	{
		m_pObject = NULL;
	}
}

void CCMeshBakerPopupPreview::SetMaterial(IMaterial* pMat)
{
	m_pMat = (CMaterial*)pMat;
}

void CCMeshBakerPopupPreview::SetRotate(bool rotate)
{
	m_Rotate = rotate;
}

void CCMeshBakerPopupPreview::SetWireframe(bool wireframe)
{
	m_bDrawWireFrame = wireframe;
}

void CCMeshBakerPopupPreview::SetGrid(bool grid)
{
	m_Grid = grid;
}


void CCMeshBakerPopupPreview::OnRender(const SRenderContext& rc)
{
	if (m_bDrawWireFrame)
	{
		NoMaterialRender(rc);
	}
	else
	{
		MaterialRender(rc);
	}
}

bool CCMeshBakerPopupPreview::NoMaterialRender(const SRenderContext& rc)
{
	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();

	DisplayContext dc;	
	m_pViewportAdapter->EnableXYViewport(true);
	dc.SetView(m_pViewportAdapter.get());

	aux->SetRenderFlags(e_Mode3D|e_AlphaBlended|e_FillModeSolid|e_CullModeNone|e_DepthWriteOff|e_DepthTestOn);

	dc.DepthTestOff();

	RenderUnwrappedMesh(dc,rc);

	dc.DepthTestOn();

	return true;
}

void CCMeshBakerPopupPreview::RenderUnwrappedMesh(DisplayContext& dc,const SRenderContext& rc)
{
	if( !m_pObject )
		return;

// 	CEdGeometry* pCEdGeometry = m_pObject->GetGeometry();m_pObject->GetIndexedMesh(true);
// 	if (NULL == pCEdGeometry)
// 		return;
// 	IIndexedMesh* pIIndexedMesh = pCEdGeometry->GetIndexedMesh();


	IIndexedMesh* pIIndexedMesh = m_pObject->GetIndexedMesh(true);
	if (NULL == pIIndexedMesh)
		return;

	RenderUnwrappedLodPolygon(dc, pIIndexedMesh,rc);
}


void CCMeshBakerPopupPreview::RenderUnwrappedLodPolygon(DisplayContext& dc, IIndexedMesh* pIIndexedMesh,const SRenderContext& rc)
{
	if (pIIndexedMesh->GetFaceCount() == 0)
	{
		pIIndexedMesh->RestoreFacesFromIndices();
	}

	IIndexedMesh::SMeshDescription sMeshDescription;
	pIIndexedMesh->GetMeshDescription(sMeshDescription);


	ColorB color = ColorB(255,0,0);
	dc.SetColor(color);

	for (int i=0; i<sMeshDescription.m_nFaceCount; i++)
	{
		const SMeshFace* pFace = &(sMeshDescription.m_pFaces[i]);

		const Vec3* pSMeshVert0 = &sMeshDescription.m_pVerts[pFace->v[0]];
		const Vec3* pSMeshVert1 = &sMeshDescription.m_pVerts[pFace->v[1]];
		const Vec3* pSMeshVert2 = &sMeshDescription.m_pVerts[pFace->v[2]];
		dc.DrawLine(*pSMeshVert0, *pSMeshVert1);
		dc.DrawLine(*pSMeshVert1, *pSMeshVert2);
		dc.DrawLine(*pSMeshVert0, *pSMeshVert2);

	}

}

bool CCMeshBakerPopupPreview::MaterialRender(const SRenderContext& rc)
{
	if (m_pMat == NULL)
		return false;

	IRenderer* renderer = gEnv->pRenderer;
	int width = rc.viewport->width();
	int height = rc.viewport->height();
	if (height < 2 || width < 2)
		return false;

	SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(*rc.camera, SRenderingPassInfo::DEFAULT_FLAGS, true);

	renderer->ChangeViewport(0,0,width,height);
	renderer->SetCamera( *rc.camera );

	CScopedWireFrameMode scopedWireFrame(renderer, m_bDrawWireFrame?R_WIREFRAME_MODE:R_SOLID_MODE);	

	// Add lights.
	for (size_t i = 0; i < m_lights.size(); i++)
	{
		renderer->EF_ADDDlight(&m_lights[i], passInfo);
	}

	if(m_pMat)
		m_pMat->DisableHighlight();

	_smart_ptr<IMaterial> pMaterial;
	if (m_pMat)
		pMaterial = m_pMat->GetMatInfo();

	if (m_bPrecacheMaterial)
	{
		IMaterial *pCurMat=pMaterial;
		if (!pCurMat)
		{
			if (m_pObject)
				pCurMat=m_pObject->GetMaterial();
			// 				else if (m_pEntity)
			// 					pCurMat=m_pEntity->GetMaterial();
			// 				else if (m_pCharacter)
			// 					pCurMat=m_pCharacter->GetIMaterial();
			// 				else if (m_pEmitter)
			// 					pCurMat=m_pEmitter->GetMaterial();
		}
		if (pCurMat)
		{
			pCurMat->PrecacheMaterial(0.0f, NULL, true, true);
		}
	}

	{ // activate shader item
		IMaterial *pCurMat=pMaterial;
		if (!pCurMat)
		{
			if (m_pObject)
				pCurMat=m_pObject->GetMaterial();
			// 				else if (m_pEntity)
			// 					pCurMat=m_pEntity->GetMaterial();
			// 				else if (m_pCharacter)
			// 					pCurMat=m_pCharacter->GetIMaterial();
			// 				else if (m_pEmitter)
			// 					pCurMat=m_pEmitter->GetMaterial();
		}

	}

	if (m_bShowObject)
		RenderObject(pMaterial, passInfo);

	
	

	return true;
}


void CCMeshBakerPopupPreview::RenderObject( IMaterial* pMaterial, SRenderingPassInfo& passInfo )
{
	if (pMaterial == NULL)
		return;

	SRendParams rp;
	rp.dwFObjFlags = 0;
	//rp.nDLightMask = (1<<m_lights.size())-1;
	rp.AmbientColor = m_ambientColor * m_ambientMultiplier;
	rp.dwFObjFlags |= FOB_TRANS_MASK /*| FOB_GLOBAL_ILLUMINATION*/ | FOB_NO_FOG /*| FOB_ZPREPASS*/;
	rp.pMaterial = pMaterial;

	Matrix34 tm;
	tm.SetIdentity();
	rp.pMatrix = &tm;

	if (m_bRotate)
	{
		tm.SetRotationXYZ(Ang3( 0,0,m_rotateAngle ));
		m_rotateAngle += 0.1f;
	}

	// visn test
// 	if (m_pObject)
// 		m_pObject->Render( rp, passInfo );

// 	if (m_pEntity)
// 		m_pEntity->Render( rp, passInfo );
// 
// 	if (m_pCharacter)
// 		m_pCharacter->Render( rp, QuatTS(IDENTITY), passInfo );
// 
// 	if (m_pEmitter)
// 	{
// 		m_pEmitter->Update();
// 		m_pEmitter->Render( rp, passInfo );
// 	}
}


void CCMeshBakerPopupPreview::RenderUnwrappedLodPolygonUV(DisplayContext& dc, IIndexedMesh* pIIndexedMesh)
{
	if (pIIndexedMesh->GetFaceCount() == 0)
	{
		pIIndexedMesh->RestoreFacesFromIndices();
	}

	IIndexedMesh::SMeshDescription sMeshDescription;
	pIIndexedMesh->GetMeshDescription(sMeshDescription);

	ColorB color = ColorB(255,255,255);
	dc.SetColor(color);

	for (int i=0; i<sMeshDescription.m_nFaceCount; i++)
	{
		const SMeshFace* pFace = &(sMeshDescription.m_pFaces[i]);
		for (int j=0; j<3; j++)
		{
			const SMeshTexCoord* pSMeshTexCoord0 = &sMeshDescription.m_pTexCoord[pFace->v[0]];
			const SMeshTexCoord* pSMeshTexCoord1 = &sMeshDescription.m_pTexCoord[pFace->v[1]];
			const SMeshTexCoord* pSMeshTexCoord2 = &sMeshDescription.m_pTexCoord[pFace->v[2]];
			Vec2 uv0 = pSMeshTexCoord0->GetUV();
			Vec2 uv1 = pSMeshTexCoord1->GetUV();
			Vec2 uv2 = pSMeshTexCoord2->GetUV();
			dc.DrawLine(Vec3(uv0.x,uv0.y,0.01f), Vec3(uv1.x,uv1.y,0.01f));
			dc.DrawLine(Vec3(uv0.x,uv0.y,0.01f), Vec3(uv2.x,uv2.y,0.01f));
			dc.DrawLine(Vec3(uv1.x,uv1.y,0.01f), Vec3(uv2.x,uv2.y,0.01f));
		}

	}

}