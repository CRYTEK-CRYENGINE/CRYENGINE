#include "StdAfx.h"
#include <Cry3DEngine/IStatObj.h>
#include "MeshBakerUVPreview.h"
#include "DisplayViewportAdapter.h"
#include "Material/Material.h"

CMeshBakerUVPreview::CMeshBakerUVPreview(QViewport * viewPort):m_ViewPort(viewPort)
{
	m_pViewportAdapter.reset(new CDisplayViewportAdapter(m_ViewPort));

	m_MeshMap = NULL;
	m_bRenderMesh = true;

	//-------------------------------------------------------------------------------
	seam_style_.visible = true ;
	seam_style_.color = Vec4(0.5, 0.5, 0.5, 1.0) ;
	seam_style_.width = 0.01 ;

	cut_seam_style_.visible = true ;
	cut_seam_style_.color = Vec4(1.0, 1.0, 0.0, 1.0) ;
	cut_seam_style_.width = 0.01 ;

	straight_seam_style_.visible = true ;
	straight_seam_style_.color = Vec4(0.0, 0.0, 1.0, 1.0) ;
	straight_seam_style_.width = 1.0 ;

	seam_anchor_style_.visible = true ;
	seam_anchor_style_.color = Vec4(1.0, 0.0, 0.0, 1.0) ;
	seam_anchor_style_.size = 0.02;

	angle_constraints_style_.visible = true ;
	angle_constraints_style_.color = Vec4(1.0, 1.0, 0.0, 1.0) ;
	angle_constraints_style_.size = 0.03 ;

// 	anchors_style_.visible = false ;
// 
	mesh_style_.visible = true ;
	mesh_style_.color = Vec4(0.5, 0.5, 0.5, 0.5) ;
	mesh_style_.width = 0.005;

//	smooth_ = false ;

	show_overlaps_ = false ;

	//-------------------------------------------------------------------------------
	m_pMatPolygonMesh = new LODGenerator::CPolygonMesh;

	smooth_ = false;
	texture_repeat_ = 1.0;

	extendVetexLength = 0.01;

	m_Ctrl = false;

	CDLight l;

	float L = 1.0f;
	l.m_fRadius = 10000;
	l.m_Flags |= DLF_SUN | DLF_DIRECTIONAL;
	l.SetLightColor(ColorF(L,L,L,1));
	l.SetPosition( Vec3(100,100,100) );
	m_lights.push_back( l );
}

CMeshBakerUVPreview::~CMeshBakerUVPreview()
{
	if (m_pMatPolygonMesh != NULL)
	{
		delete m_pMatPolygonMesh;
	}
}


void CMeshBakerUVPreview::OnMouseEvent(const SMouseEvent& sMouseEvent)
{

}

void CMeshBakerUVPreview::UpdateMeshMap()
{
	m_pMatPolygonMesh->SetPolygon(m_MeshMap,true);
}

void CMeshBakerUVPreview::SetMaterial(IMaterial* pMaterial)
{
	m_pMatPolygonMesh->SetMaterial(pMaterial);
}

void CMeshBakerUVPreview::SetMeshMap(CTopologyGraph * pMeshMap)
{
	if (pMeshMap == NULL)
		return;

	m_MeshMap = pMeshMap;
	m_pMatPolygonMesh->SetPolygon(m_MeshMap,true);


}

void CMeshBakerUVPreview::OnRender(const SRenderContext& rc)
{
	if( !m_MeshMap )
		return;

	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();

	DisplayContext dc;	
	m_pViewportAdapter->EnableXYViewport(true);
	dc.SetView(m_pViewportAdapter.get());

	if (m_bRenderMesh)
	{
		RenderMesh(dc,rc);
	} 
	else
	{
		RenderUV(dc,rc);
	}
}

void CMeshBakerUVPreview::RenderMesh(DisplayContext& dc,const  SRenderContext& rc)
{
	if( !m_MeshMap )
		return;

	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();
	aux->SetRenderFlags(e_Mode3D|e_FillModeSolid|e_CullModeNone|e_DepthTestOn);

	RenderSurface(dc,rc);
}

void CMeshBakerUVPreview::RenderUV(DisplayContext& dc,const  SRenderContext& rc)
{
	if( !m_MeshMap )
		return;

	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();
	aux->SetRenderFlags(e_Mode3D|e_AlphaBlended|e_FillModeSolid|e_CullModeNone|e_DepthWriteOff|e_DepthTestOn);

	dc.DepthTestOff();

	RenderUVEdges(dc,rc);
	RenderUVBorders(dc,rc);

	dc.DepthTestOn();
}

void CMeshBakerUVPreview::RenderUVEdges(DisplayContext& dc,const  SRenderContext& rc)
{
	if (m_MeshMap == NULL)
		return;

	ColorB color = ColorB(255,255,255);
	dc.SetColor(color);
	dc.SetLineWidth(0.1);

	for( CTopologyGraph::Halfedge_iterator it=(m_MeshMap)->halfedges_begin();it!=(m_MeshMap)->halfedges_end(); it++)
	//FOR_EACH_HALFEDGE(TopologyGraph, m_MeshMap, it) 
	{
		if(true/*is_selected(it)*/) 
		{
			//int index = color_index(it, seam_type) ;
			//if(index >= 0) {
			//pipeline->color(colormap[index]) ;   

			Vec2 uv0 = (*it)->tex_coord() ;
			Vec2 uv1 = (*it)->prev()->tex_coord();

			dc.DrawLine(Vec3(uv0.x,uv0.y,0.01f), Vec3(uv1.x,uv1.y,0.01f));
		}
	}
}

void CMeshBakerUVPreview::RenderUVBorders(DisplayContext& dc,const  SRenderContext& rc)
{
	if (m_MeshMap == NULL)
		return;

	ColorB color = ColorB(255,255,0);
	dc.SetColor(color);

	for( CTopologyGraph::Halfedge_iterator it=(m_MeshMap)->halfedges_begin();it!=(m_MeshMap)->halfedges_end(); it++)
	//FOR_EACH_HALFEDGE(TopologyGraph, m_MeshMap, it) 
	{
		if((*it)->facet() == NULL ) 
		{
			Vec2 uv0 = (*it)->prev()->tex_vertex()->tex_coord();
			Vec2 uv1 = (*it)->tex_vertex()->tex_coord();

			dc.DrawLine(Vec3(uv0.x,uv0.y,0.01f), Vec3(uv1.x,uv1.y,0.01f));
		}
	}
}


void CMeshBakerUVPreview::RenderSurface(DisplayContext& dc,const  SRenderContext& rc) 
{
	IRenderer* renderer = gEnv->pRenderer;

	if( m_MeshMap )
		m_pMatPolygonMesh->GetRenderNode()->Render(*rc.renderParams, *rc.passInfo);
}
