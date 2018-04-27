#include "StdAfx.h"
#include "Component.h"
#include <stack>
#include "TopologyGraph.h"
#include "TopologyBuilder.h"

namespace LODGenerator 
{
	CComponent::CComponent(CTopologyGraph* map) : map_(map) 
	{

	}

	CComponent::Vertex_iterator CComponent::vertices_begin() 
	{ 
		return vertices_.begin(); 
	}

	CComponent::Vertex_iterator CComponent::vertices_end() 
	{ 
		return vertices_.end(); 
	}

	CComponent::Halfedge_iterator CComponent::halfedges_begin() 
	{ 
		return halfedges_.begin(); 
	}

	CComponent::Halfedge_iterator CComponent::halfedges_end() 
	{ 
		return halfedges_.end(); 
	}

	CComponent::Facet_iterator CComponent::facets_begin() 
	{ 
		return facets_.begin(); 
	}

	CComponent::Facet_iterator CComponent::facets_end() 
	{ 
		return facets_.end(); 
	}

	CComponent::Vertex_const_iterator CComponent::vertices_begin() const 
	{ 
		return vertices_.begin(); 
	}

	CComponent::Vertex_const_iterator CComponent::vertices_end() const 
	{ 
		return vertices_.end(); 
	}

	CComponent::Halfedge_const_iterator CComponent::halfedges_begin() const 
	{ 
		return halfedges_.begin(); 
	}

	CComponent::Halfedge_const_iterator CComponent::halfedges_end() const 
	{ 
		return halfedges_.end(); 
	}

	CComponent::Facet_const_iterator CComponent::facets_begin() const 
	{ 
		return facets_.begin(); 
	}

	CComponent::Facet_const_iterator CComponent::facets_end() const 
	{ 
		return facets_.end(); 
	}

	unsigned int CComponent::size_of_vertices() const 
	{
		return (unsigned int) vertices_.size();
	}

	unsigned int CComponent::size_of_halfedges() const 
	{
		return (unsigned int) halfedges_.size();
	}

	unsigned int CComponent::size_of_facets() const 
	{
		return (unsigned int) facets_.size();
	}

	CTopologyGraph* CComponent::map() const 
	{ 
		return map_; 
	}

	std::vector<CVertex*>& CComponent::vertices() 
	{ 
		return vertices_; 
	}
	std::vector<CHalfedge*>& CComponent::halfedges()
	{ 
		return halfedges_; 
	}
	std::vector<CFacet*>& CComponent::facets() 
	{ 
		return facets_; 
	}

	double CComponent::component_area() 
	{
		double result = 0;
		FOR_EACH_FACET_CONST(CComponent, this, it) 
		{
			result += (*it)->facet_area();
		}
		return result;
	}  

	double CComponent::map_area() 
	{
		double result = 0;
		FOR_EACH_FACET_CONST(CTopologyGraph, this, it) 
		{
			result += (*it)->facet_area();
		}
		return result;
	}   

	double CComponent::component_border_length() 
	{
		double result = 0.0;
		FOR_EACH_HALFEDGE_CONST(CComponent, this, it) 
		{
			if((*it)->is_border()) 
			{
				result += (*it)->direct().len();
			}
		}

		return result;
	}

	AABB CComponent::bbox2d() 
	{
		AABB result;
		result.Reset();
		FOR_EACH_HALFEDGE_CONST(CComponent, this, it) 
		{
			result.Add((*it)->tex_vertex()->tex_coord());
		}
		return result;
	}

	double CComponent::signed_area2d() 
	{
		double result = 0;
		FOR_EACH_FACET_CONST(CComponent, this, it) 
		{
			result += (*it)->facet_signed_area2d();
		}
		return result;
	} 

	double CComponent::area2d()
	{
		return fabs(signed_area2d());
	}

	double CComponent::area() 
	{
		double result = 0;
		FOR_EACH_FACET_CONST(CComponent, this, it) 
		{
			result += (*it)->facet_area();
		}
		return result;
	}  

	void CComponent::translate(const Vec3d& v) 
	{
		FOR_EACH_VERTEX(CComponent, this, it) 
		{
			Vec3d new_p = (*it)->point() + v;
			(*it)->set_point(new_p);
		}
	}

	void CComponent::translate2d(const Vec2d& v) 
	{
		FOR_EACH_VERTEX(CComponent, this, it) 
		{
			Vec2d new_p = (*it)->halfedge()->tex_coord() + v;
			(*it)->halfedge()->set_tex_coord(new_p);
		}
	}

	CTopologyGraph* CComponent::component_to_topology(std::map<CVertex*,CVertex*>& orig_vertex) 
	{
		orig_vertex.clear();

		std::map<CVertex*,int> vertex_id;
		{
			int id = 0;
			FOR_EACH_VERTEX(CComponent, this, it) 
			{
				vertex_id[*it] = id;
				id++;
			}
		}
		CTopologyGraph* result = new CTopologyGraph;

		CTopologyBuilder builder(result);
		builder.begin_surface();
		FOR_EACH_VERTEX(CComponent, this, it) 
		{
			builder.add_vertex((*it)->point());
			orig_vertex[builder.current_vertex()] = *it;
		}
		FOR_EACH_FACET(CComponent, this, it) 
		{
			std::vector<int> v;
			CHalfedge* h = (*it)->halfedge();
			do 
			{
				v.push_back(vertex_id[h->vertex()]);
				h = h->next();
			} 
			while(h != (*it)->halfedge());
		
			builder.begin_facet();
			for(unsigned int i=0; i<v.size(); i++) 
			{
				builder.add_vertex_to_facet(v[i]);
			}
			builder.end_facet();
			
		}
		builder.end_surface();

		return result;
	}

	int enumerate_connected_components(CTopologyGraph* map, std::map<CVertex*, int>& id);
	ComponentList CComponentExtractor::extract_components(CTopologyGraph* map) 
	{
		std::map<CVertex*,int> component_id;
		int nb_components = enumerate_connected_components(map, component_id);
		ComponentList result;
		for(int i=0; i<nb_components; i++) 
		{
			result.push_back(new CComponent(map));
		}

		FOR_EACH_VERTEX(CTopologyGraph, map, it) 
		{
			set_target(result[component_id[*it]]);
			vertices().push_back(*it);
		}

		FOR_EACH_HALFEDGE(CTopologyGraph, map, it) 
		{
			set_target(result[component_id[(*it)->vertex()]]);
			halfedges().push_back(*it);
		}

		FOR_EACH_FACET(CTopologyGraph, map, it) 
		{
			set_target(result[component_id[(*it)->halfedge()->vertex()]]);
			facets().push_back(*it);
		}

		return result;
	}

	static void propagate_connected_component(CTopologyGraph* map, std::map<CVertex*, int>& id,CVertex* v, int cur_id) 
	{
		std::stack<CVertex*> stack;
		stack.push(v);

		while(!stack.empty()) 
		{
			CVertex* top = stack.top();
			stack.pop();
			if(id[top] == -1) 
			{
				id[top] = cur_id;
				CHalfedge* it = top->halfedge();
				do 
				{
					CVertex* v = it-> opposite()-> vertex();
					if(id[v] == -1) 
						stack.push(v);
					it = it->next_around_vertex();
				} 
				while(it != top->halfedge());
			}
		}
	}

	int enumerate_connected_components(CTopologyGraph* map, std::map<CVertex*, int>& id) 
	{
		FOR_EACH_VERTEX(CTopologyGraph,map,it) 
		{
			id[*it] = -1;
		}
		int cur_id = 0;
		FOR_EACH_VERTEX(CTopologyGraph,map,it) 
		{
			if(id[*it] == -1) {
				propagate_connected_component(map, id, *it, cur_id);
				cur_id++;
			}
		}
		return cur_id;
	}

	CComponentTopology::CComponentTopology(const CComponent* comp) 
	{
		component_ = comp;

		// Compute number_of_borders_

		std::map<CHalfedge*,bool> is_marked;

		number_of_borders_ = 0;
		largest_border_size_ = 0;

		FOR_EACH_HALFEDGE_CONST(CComponent, component_, it) 
		{
			CHalfedge* cur = *it;
			if(cur->is_border() && !is_marked[cur]) 
			{
				number_of_borders_++;
				int border_size = 0;
				do {
					border_size++;
					is_marked[cur] = true;
					cur = cur->next();
				} while(cur != *it);
				largest_border_size_ = max(largest_border_size_, border_size);
			}
		}
	}

	int CComponentTopology::xi() const 
	{
		// xi = #vertices - #edges + #facets
		// #edges = #halfedges / 2 
		return int(component_->size_of_vertices()) - int(component_->size_of_halfedges() / 2) + int(component_->size_of_facets());
	}

	bool CComponentTopology::is_almost_closed(int max_border_size) const 
	{
		if(component_->size_of_facets() == 1)
			return false;
		return largest_border_size_ <= max_border_size;
	}
}