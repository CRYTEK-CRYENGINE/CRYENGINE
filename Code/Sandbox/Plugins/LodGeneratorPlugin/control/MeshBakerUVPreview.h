#pragma once

#include <QWidget>
#include <Cry3DEngine/IIndexedMesh.h>
#include "QViewport.h"
#include "../Util/PolygonMesh.h"
#include "../Util/TopologyGraph.h"

using namespace LODGenerator;

class QViewport;
class CMaterial;
class CDisplayViewportAdapter;

struct EdgeStyle 
{
	EdgeStyle() : visible(false), color(0.0,0.0,0.0,1.0), width(1.0) {}
	bool visible ;
	Vec4 color ;
	float width ;
} ;

struct PointStyle 
{
	PointStyle() : visible(false), color(0.0,0.0,0.0,1.0), size(4.0) {}
	bool visible ;
	Vec4 color ;
	float size ;
} ;




class CMeshBakerUVPreview
{
public:
	CMeshBakerUVPreview(QViewport * viewPort);
	virtual ~CMeshBakerUVPreview();

	void SetMeshMap(CTopologyGraph* pMeshMap);
	void UpdateMeshMap();

	void SetMaterial(IMaterial* pMaterial);

	void OnRender(const SRenderContext& rc);
	void OnMouseEvent(const SMouseEvent& sMouseEvent);

	void SetRenderMesh(bool bRenderMesh)
	{
		m_bRenderMesh = bRenderMesh;
	}

private:
	
	void RenderUV(DisplayContext& dc,const  SRenderContext& rc);
	void RenderUVEdges(DisplayContext& dc,const  SRenderContext& rc);
	void RenderUVBorders(DisplayContext& dc,const  SRenderContext& rc);

	void RenderMesh(DisplayContext& dc,const  SRenderContext& rc);
	void RenderSurface(DisplayContext& dc,const  SRenderContext& rc);

	void AutoSeamPrepare(bool glue);

private:


	QViewport * m_ViewPort;
	std::shared_ptr<CDisplayViewportAdapter> m_pViewportAdapter;

	bool m_bRenderMesh;

	EdgeStyle seam_style_ ;
	EdgeStyle cut_seam_style_ ;
	EdgeStyle straight_seam_style_ ;
	PointStyle seam_anchor_style_ ;
	PointStyle angle_constraints_style_ ;

	EdgeStyle  mesh_style_ ;
	PointStyle vertices_style_ ;

	bool show_overlaps_ ;
	bool has_selection_ ;
	bool points_as_spheres_;
	bool smooth_;

	double overlap_u_min_ ;
	double overlap_v_min_ ;
	double overlap_u_max_ ;
	double overlap_v_max_ ;

	float texture_repeat_;

	float extendVetexLength;

	LODGenerator::CTopologyGraph* m_MeshMap;
	LODGenerator::CPolygonMesh* m_pMatPolygonMesh;
	bool m_Ctrl;

	std::vector<CDLight> m_lights;
};
