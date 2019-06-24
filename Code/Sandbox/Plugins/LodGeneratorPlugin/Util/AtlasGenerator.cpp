#include "StdAfx.h"

#include "AtlasGenerator.h"
#include <Cry3DEngine/IIndexedMesh.h>
#include "TopologyTransform.h"
#include "Packer.h"
#include "TopologyGraph.h"
#include "UVGenerator.h"

namespace LODGenerator
{
	CAtlasGenerator::CAtlasGenerator()
	{
		m_MeshMap = NULL;
	}

	CAtlasGenerator::~CAtlasGenerator()
	{
		if(m_MeshMap != NULL)
			delete m_MeshMap;
	}


	CTopologyGraph* CAtlasGenerator::surface()
	{
		return m_MeshMap;
	}

	void CAtlasGenerator::pack_in_texture_space() 
	{
		CPacker packer;
		packer.pack_map(surface());
	}

	void CAtlasGenerator::generate_texture_atlas(bool auto_cut,bool auto_cut_cylinders,double max_overlap_ratio,double max_scaling, double min_fill_ratio)
	{
		if (surface() == NULL)
			return;
		
		CTopologyGraphMutator editor(surface());
		FOR_EACH_VERTEX(CTopologyGraph, surface(), it) 
		{
			editor.merge_tex_vertices(*it);
		}
		CUVGenerator generator(surface());
		generator.set_auto_cut(auto_cut);
		generator.set_auto_cut_cylinders(auto_cut_cylinders);
		generator.set_max_overlap_ratio(max_overlap_ratio);
		generator.set_max_scaling(max_scaling);
		generator.set_min_fill_ratio(min_fill_ratio);
		generator.apply();
		pack_in_texture_space();
	}


	void CAtlasGenerator::setSurface(IStatObj *pObject)
	{
		m_MeshMap = CTopologyTransform::TransformToTopology(pObject);
	}

	void CAtlasGenerator::setSurface(CTopologyGraph* meshMap)
	{
		m_MeshMap = meshMap;
	}
};