#include "StdAfx.h"
#include "UVGenerator.h"
#include "Splitter.h"
#include "ABF.h"
#include "Component.h"
#include "Packer.h"
#include "Validator.h"

namespace LODGenerator 
{
	static double default_sock_treshold = 5.0 / (4.0 * 4.0);

	static bool component_is_sock(CComponent* comp, double treshold = default_sock_treshold) 
	{
		double area      = comp->component_area();
		double perimeter = comp->component_border_length();
		if(::fabs(perimeter) < 1e-30) {
			return false;
		}
		// Note: perimeter is squared to be scale invariant 
		double ratio = area / (perimeter * perimeter);
		return (ratio > treshold);
	}

	CUVGenerator::CUVGenerator(CTopologyGraph* map) : map_(map)
	{ 
		max_overlap_ratio_ = 0.005;
		max_scaling_ = 20.0;
		min_fill_ratio_ = 0.25;
		auto_cut_ = true;
		auto_cut_cylinders_ = true;

		parameterizer_ = new CABF();
		splitter_ = new CSplitter();
	}

	CUVGenerator::~CUVGenerator()
	{
		delete parameterizer_;
		parameterizer_ = NULL;
		delete splitter_;
		splitter_ = NULL;
	}

	void CUVGenerator::activate_component(CComponent* comp, int iter) 
	{
			if(comp->size_of_facets() == 0) 
				return;
			
			SActiveComponent i(iter+1);
			active_components_.push_back(i);
			active_components_.back().component = comp;
	}

	void CUVGenerator::split_component(CComponent* component, int iter) 
	{
		std::vector<CComponent*> charts;
		if(splitter_->split_component(component, charts)) 
		{
			for(unsigned int i=0; i<charts.size(); i++) 
			{
				activate_component(charts[i],iter);
			}
		} 
		else 
		{
			CryLog("UVGenerator Split Component failed");
		}

		delete component;
	}


	void CUVGenerator::apply() 
	{
		parameterized_faces_ = 0;
		total_faces_ = map_->size_of_facets();

		get_components();

		FOR_EACH_HALFEDGE(CTopologyGraph, map_, it) 
		{
			(*it)->tex_vertex()->set_tex_coord(Vec2d(0,0));
		}

		while(!active_components_.empty()) 
		{
			int iter = active_components_.front().iter;
			CComponent* cur = active_components_.front().component;
			active_components_.pop_front();
			CryLog("UVGenerator Processing component, iter= %d size= %d",iter,cur->size_of_facets());

			bool is_valid = parameterizer_->parameterize_disc(cur);

			if(is_valid && auto_cut_) 
			{
				CValidator validator;
				validator.set_max_overlap_ratio(max_overlap_ratio_);
				validator.set_max_scaling(max_scaling_);
				validator.set_min_fill_ratio(min_fill_ratio_);
				is_valid = validator.component_is_valid(cur);
			}

			if(is_valid) 
			{
				parameterized_faces_ += cur->size_of_facets();
				delete cur;
				cur = NULL;
			} 
			else 
			{
				split_component(cur, iter);			
			}

			CryLog("UVGenerator ====> %d/%d parameterized (%d%%)",parameterized_faces_,total_faces_,int(double(parameterized_faces_) * 100.0 / double(total_faces_)));
		}

		{
			CPacker packer;
			packer.pack_map(map_);
		}        

		CryLog("UVGenerator total elapsed time: ??");
	}


	void CUVGenerator::get_components() 
	{
		CComponentExtractor extractor;
		ComponentList components = extractor.extract_components(map_);

		for(ComponentList::iterator it = components.begin(); it != components.end(); it++) 
		{
			CComponent* cur = (*it);
			CComponentTopology topology(cur);
			if( topology.is_disc() && !topology.is_almost_closed()) 
			{
				CryLog("UVGenerator component is disc");
				activate_component(cur);
			} 
			else 
			{
				CryLog("UVGenerator Processing non-disc component");
				CryLog("UVGenerator   xi=%d #borders=%d #vertices=%d #facets=%d largest border size=%d largest border size=%d",topology.xi(),topology.number_of_borders(),cur->size_of_vertices(),cur->size_of_facets(),topology.largest_border_size());
				if( (topology.is_almost_closed() || (topology.is_disc() && component_is_sock(cur)) ) ) 
				{
					CryLog("UVGenerator   Almost closed component, cutting ...");
					split_component(cur);
				} 
				else if (auto_cut_cylinders_ && topology.is_cylinder()) 
				{
					CryLog("UVGenerator   Cutting cylinder ...");
					split_component(cur);
				} 
				else 
				{
					CryLog("UVGenerator   topological monster, try to parameterize");
					activate_component(cur);
				}
			} 
		}
	}
}
