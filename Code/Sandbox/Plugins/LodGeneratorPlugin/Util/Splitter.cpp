#include "StdAfx.h"
#include "Splitter.h"
#include "Vector.h"
#include "Approx.h"
#include "Component.h"

namespace LODGenerator 
{
	CSplitter::CSplitter(CComponent* target) : target_(target) 
	{
		error_decrease_factor_ = 0.75;
		max_components_  = 10;
		smooth_ = false;
		smooth_ = true;
	}

	CSplitter::~CSplitter() 
	{ 
		target_ = NULL; 
	}

	class SmoothVertex 
	{
	public:
		SmoothVertex(CVertex* v, std::map<CFacet*,int>& chart) ;

		bool operator<(const SmoothVertex& rhs) const {
			return (delta_len_ < rhs.delta_len_) ;
		}

		bool apply( std::map<CFacet*,int>& chart,std::map<CVertex*,bool>& locked) ;
		bool is_valid() const { return is_valid_ ; }

	private:
		CVertex* vertex_ ;
		int chart_id_ ;
		double delta_len_ ;
		bool is_valid_ ;
	} ;

	template <class MAP> void smooth_partition(MAP* map, std::map<CFacet*,int>& chart, std::map<CVertex*,bool>& locked,int max_iter) 
	{
		std::vector<SmoothVertex> smooth_vertices ;
		for(int iter = 0; iter < max_iter; iter++) 
		{
			smooth_vertices.clear() ;
			FOR_EACH_VERTEX(MAP, map, it) 
			{
				locked[(*it)] = false ;
				SmoothVertex sv(*it, chart) ;
				if(sv.is_valid()) 
				{
					smooth_vertices.push_back(sv) ;
				}
			}
			bool changed = false ;
			std::sort(smooth_vertices.begin(), smooth_vertices.end()) ;
			for(unsigned int i=0; i<smooth_vertices.size(); i++) 
			{
				if(smooth_vertices[i].apply(chart, locked)) 
					changed = true ;
			}
			if(!changed) 
				break ;
		}
	}

	SmoothVertex::SmoothVertex(CVertex* v, std::map<CFacet*,int>& chart) 
	{

		chart_id_  = -1 ;
		delta_len_ = 0.0 ;

		CHalfedge* h = v->halfedge() ;
		do 
		{
			// Check if vertex is on border
			if(h->facet() == NULL) 
			{
				is_valid_ = false ; return ; 
			}
			// Check if vertex's star is adjacent to the border
			if(h->prev()->opposite()->facet() == NULL) 
			{
				is_valid_ = false ; return ; 
			}
			h = h->next_around_vertex() ;
		} 
		while(h != v->halfedge()) ;


		// Check if we got two and only two different chart ids 
		// in star (and get them)

		int chart1 = chart[h->facet()] ;
		int nb_chart1 = 0 ;
		int chart2 = -1 ;
		int nb_chart2 = 0 ;

		h = h->next_around_vertex() ;
		do 
		{
			if(chart[h->facet()] == chart1) 
			{
				nb_chart1++ ;
			} else {
				if(chart2 == -1) 
				{
					chart2 = chart[h->facet()] ;
				}
				if(chart[h->facet()] != chart2) 
				{
					is_valid_ = false ; return ; 
				}
				if(chart[h->prev()->opposite()->facet()] != chart2) 
				{
					is_valid_ = false ; return ; 
				}
				nb_chart2++ ;
			}
			h = h->next_around_vertex() ;
		} 
		while(h != v->halfedge()) ;

		if(chart2 == -1) 
		{
			is_valid_ = false ; return ; 
		}

		if(nb_chart1 > nb_chart2) 
		{
			chart_id_ = chart1 ;
		} 
		else 
		{
			chart_id_ = chart2 ;
		}

		// Check that the partition is manifold at vertex v
		int nb_change = 0 ;
		do 
		{
			if(chart[h->next_around_vertex()->facet()] !=chart[h->facet()]) 
			{
				nb_change++ ;
			}
			h = h->next_around_vertex() ;
		} 
		while(h != v->halfedge()) ;

		if(nb_change != 2) 
		{
			is_valid_ = false ; return ; 
		}

		// Compute difference of frontier length
		delta_len_ = 0.0 ;
		do 
		{
			if(chart[h->facet()] != chart[h->opposite()->facet()]) 
			{
				delta_len_ += h->direct().len() ;
			}
			if(chart[h->facet()] != chart_id_) 
			{
				delta_len_ -= h->prev()->direct().len() ;
			}
			h = h->next_around_vertex() ;
		} 
		while(h != v->halfedge()) ;


		if(delta_len_ < 0) 
		{
			is_valid_ = false ; return ; 
		}

		vertex_ = v ;
		is_valid_ = true ;
	}

	bool SmoothVertex::apply(std::map<CFacet*,int>& chart, std::map<CVertex*,bool>& locked) 
	{
			if(locked[vertex_]) 
			{
				return false ;
			}
			CHalfedge* h = vertex_->halfedge() ;
			do 
			{
				chart[h->facet()] = chart_id_ ;
				locked[h->opposite()->vertex()] = true ;
				h = h->next_around_vertex() ;
			} 
			while(h != vertex_->halfedge()) ;
			return true ;
	}

	bool CSplitter::split_component(CComponent* component, std::vector<CComponent*>& charts) 
	{
			charts.clear();
			CryLog("Splitting component ....");
			CApprox approx(component,chart_);
			approx.init(1,0);
			double initial_error = approx.optimize(1);
			double expected_error = initial_error * error_decrease_factor_;

			int max_components = max_components_;

			approx.add_charts(max_components, 4, expected_error, 2);
			smooth_partition(component, chart_, is_locked_, 100);

			while(nb_charts(component) < 2) 
			{
				CryLog("Did not manage to create charts, relaunching ...");
				expected_error *= 0.5;
				max_components++;
				approx.add_charts(max_components, 4, expected_error, 2);
				if(smooth_) {
					smooth_partition(component, chart_, is_locked_, 100);
				}
			}
			unglue_charts(component);
			get_charts(component, charts);
			CryLog("Created %d charts",charts.size());
			return true;
	}

	int CSplitter::nb_charts(CComponent* component) 
	{
		int max_chart = -1;
		FOR_EACH_FACET(CComponent, component, it) 
		{
			max_chart = max(max_chart, chart_[*it]);
		}
		CVectorI histo(max_chart + 1);
		histo.zero();
		FOR_EACH_FACET(CComponent, component, it) 
		{
			histo(chart_[*it])++;
		}            
		int result = 0;
		for(unsigned int i=0; i<histo.size(); i++) 
		{
			if(histo(i) != 0) {
				result++;
			}
		}
		return result;
	}

	int CSplitter::chart(CHalfedge* h) 
	{
		if(h->is_border()) 
		{
			h = h->opposite();
		}
		return chart_[h->facet()];
	}

	void CSplitter::unglue_charts(CComponent* component) 
	{
		CTopologyGraphMutator editor(component->map());
		std::vector<CHalfedge*> to_unglue;
		FOR_EACH_EDGE(CComponent, component, it) 
		{
			if(!(*it)->is_border_edge() && chart_[(*it)->facet()] != chart_[(*it)->opposite()->facet()]) 
			{
				to_unglue.push_back(*it);
			}
		}
		for(unsigned int i=0; i<to_unglue.size(); i++) 
		{
			CHalfedge* h = to_unglue[i];
			editor.unglue(h, false);
		}
	}

	void CSplitter::get_charts(CComponent* component, std::vector<CComponent*>& charts) 
	{
		charts.clear();
		int max_chart_id = -1;
		FOR_EACH_FACET(CComponent, component, it) 
		{
			max_chart_id = max(max_chart_id, chart_[*it]);
		}

		for(int i=0; i<=max_chart_id; i++) 
		{
			charts.push_back(new CComponent(component->map()));
		}

		std::set<CVertex*> border_vertices;

		FOR_EACH_FACET(CComponent, component, it) 
		{
			assert(*it != NULL);
			set_target(charts[chart_[(*it)]]);
			facets().push_back((*it));
			CHalfedge* h = (*it)->halfedge();
			do 
			{
				halfedges().push_back(h);
				if(h->opposite()->is_border()) 
				{
					halfedges().push_back(h->opposite());
					CVertex* v = h->vertex();
					if(border_vertices.find(v) == border_vertices.end()) 
					{
						border_vertices.insert(v);
						vertices().push_back(v);
					}
				}
				h = h->next();
			} 
			while(h != (*it)->halfedge());
		}

		FOR_EACH_VERTEX(CComponent, component, it) 
		{
			if(!(*it)->is_on_border()) {
				set_target(charts[chart((*it)->halfedge())]);
				vertices().push_back(*it);
			}
		}
	}

	std::vector<CVertex*>& CSplitter::vertices() 
	{ 
		return target_->vertices(); 
	}
	std::vector<CHalfedge*>& CSplitter::halfedges() 
	{ 
		return target_->halfedges(); 
	}
	std::vector<CFacet*>& CSplitter::facets() 
	{ 
		return target_->facets(); 
	}
}