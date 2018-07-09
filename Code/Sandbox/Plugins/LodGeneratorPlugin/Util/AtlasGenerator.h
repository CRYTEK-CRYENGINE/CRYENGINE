#pragma once

#include <string>

struct IStatObj;
namespace LODGenerator
{
	class CTopologyGraph;
	class CAtlasGenerator
	{
	public:
		CAtlasGenerator();
		~CAtlasGenerator();

	public:

		void generate_texture_atlas(bool auto_cut,bool auto_cut_cylinders,double max_overlap_ratio,double max_scaling, double min_fill_ratio);
		void pack_in_texture_space();

		CTopologyGraph* surface();

		void setSurface(IStatObj *pObject);
		void setSurface(CTopologyGraph* meshMap);

		void testGenerate_texture_atlas()
		{
			//generate_texture_atlas(false,true,true,"ABF++",0.005,120,0.25,true,false,"Spectral_max",-1);
			generate_texture_atlas(true,true,0.005,120,0.25);
			//pack_in_texture_space();

		}

	private:

		CTopologyGraph* m_MeshMap;
	};
};


