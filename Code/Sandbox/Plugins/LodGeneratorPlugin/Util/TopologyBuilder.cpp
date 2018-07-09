#include "StdAfx.h"
#include "TopologyBuilder.h"

namespace LODGenerator 
{
	struct FacetKey 
	{
		FacetKey(CVertex* a1, CVertex* a2, CVertex* a3) 
		{
			v1 = a1; v2 = a2; v3 = a3;
		}
		bool operator<(const FacetKey& rhs) const 
		{
			if(v1 < rhs.v1) { return true; }
			if(v1 > rhs.v1) { return false; }
			if(v2 < rhs.v2) { return true; }
			if(v2 > rhs.v2) { return false; }
			return (v3 < rhs.v3); 
		}
		CVertex* v1;
		CVertex* v2;
		CVertex* v3;
	};

	static std::set<FacetKey>* all_facet_keys;
	void CTopologyBuilder::begin_surface() 
	{
		transition(eS_Initial, eS_Surface);

		nb_non_manifold_v_ = 0;
		nb_duplicate_e_    = 0;
		nb_isolated_v_     = 0;
		all_facet_keys = new std::set<FacetKey>;
	}

	void CTopologyBuilder::clear()
	{
		vertex_.clear();
		tex_vertex_.clear();
	}

	void CTopologyBuilder::end_surface(bool remove) 
	{
		delete all_facet_keys; all_facet_keys = NULL;
		transition(eS_Surface, eS_Final);
		terminate_surface();
		if(remove)
		{
			vertex_.clear();
			tex_vertex_.clear();
		}

		if(!quiet_ && (nb_non_manifold_v_ != 0)) 
		{
			CryLog("MapBuilder Encountered non-manifold vertices (fixed)");
		}
		if(!quiet_ && (nb_duplicate_e_ != 0)) 
		{
			CryLog("MapBuilder Encountered duplicated edges (fixed)");
		}
		if(!quiet_ && (nb_isolated_v_ != 0)) 
		{
			CryLog("MapBuilder Encountered isolated vertices (removed)");
		}
	}

	void CTopologyBuilder::reset() 
	{
		transition(eS_Final, eS_Initial);
	}

	void CTopologyBuilder::add_vertex(unsigned int id, const Vec3d& p) 
	{
		transition(eS_Surface, eS_Surface);
		add_vertex_internal(id, p);
	}

	void CTopologyBuilder::add_vertex(const Vec3d& p) 
	{
		transition(eS_Surface, eS_Surface);
		add_vertex_internal(p);
	}

	void CTopologyBuilder::add_tex_vertex(const Vec2d& p) 
	{
		add_tex_vertex(tex_vertex_.size(), p);
	}

	void CTopologyBuilder::add_tex_vertex(unsigned int id, const Vec2d& p) 
	{
		transition(eS_Surface, eS_Surface);
		std::shared_ptr<CTexVertex>& new_tv = std::shared_ptr<CTexVertex>(target_->new_tex_vertex());
		new_tv-> set_tex_coord(p);
		while(tex_vertex_.size() <= id) {
			tex_vertex_.push_back(NULL);
		}
		tex_vertex_[id] = new_tv;
	}

	void CTopologyBuilder::add_vertex_internal(unsigned int id, const Vec3d& p) 
	{
		CVertex* new_v = target_->new_vertex();
		new_v-> set_point(p);
		while(vertex_.size() <= id) {
			vertex_.push_back(NULL);
		}
		vertex_[id] = new_v;
	}


	void CTopologyBuilder::add_vertex_internal(const Vec3d& p) 
	{
		add_vertex_internal(vertex_.size(), p);
	}

	void CTopologyBuilder::begin_facet() {
		facet_vertex_.clear();
		facet_tex_vertex_.clear();
	}

	void CTopologyBuilder::end_facet() 
	{
		assert(facet_vertex_.size() == facet_tex_vertex_.size());
		int nb_vertices = facet_vertex_.size();

		if(nb_vertices < 3) 
		{
			if(!quiet_) 
			{
				CryLog("MapBuilder Facet with less than 3 vertices");
			}
			return;
		}

		if(false && nb_vertices == 3) 
		{
			std::vector<CVertex*> W = facet_vertex_;
			std::sort(W.begin(), W.end());
			FacetKey k(W[0], W[1],W[2]);
			if(all_facet_keys->find(k) != all_facet_keys->end()) 
			{
				//std::cerr << '.' << std::flush;
				return;
			}
			all_facet_keys->insert(k);
		}


		// Detect duplicated vertices
		for(int i=0; i<nb_vertices; i++) 
		{
			for(int j=i+1; j<nb_vertices; j++) 
			{
				if(facet_vertex_[i] == facet_vertex_[j]) 
				{
					if(!quiet_) 
					{
						CryLog("MapBuilder Duplicated vertex in facet, ignoring facet");
					}
					return;
				}
			}
		}

		// Detect and remove non-manifold edges by duplicating
		//   the corresponding vertices.
		for(int from=0; from<nb_vertices; from++) 
		{
			int to=((from+1) % nb_vertices);
			if(find_halfedge_between(facet_vertex_[from], facet_vertex_[to]) != NULL ) 
			{
				nb_duplicate_e_++;
				facet_vertex_[from] = copy_vertex(facet_vertex_[from]);
				facet_vertex_[to] = copy_vertex(facet_vertex_[to]);
			}
		}


		begin_facet_internal();
		for(int i=0; i<nb_vertices; i++) 
		{
			add_vertex_to_facet_internal(facet_vertex_[i]);
			if(facet_tex_vertex_[i] != NULL) 
			{
				set_corner_tex_vertex_internal(facet_tex_vertex_[i]);
			}
		}
		end_facet_internal();
	}


	CVertex* CTopologyBuilder::copy_vertex(CVertex* from) 
	{
		// Note: tex coords are not copied, since 
		//  an halfedge does not exist in the copy to
		//  store the texvertex pointer.

		CVertex* result = target_->new_vertex();
		result->set_point(from->point());
		if(is_locked_[from]) 
		{
			is_locked_[result] = true;
		}
		return result;
	}

	void CTopologyBuilder::add_vertex_to_facet(int i) 
	{
		if(i < 0 || i >= int(vertex_.size())) 
		{
			if(!quiet_) 
			{
				CryLog("MapBuilder vertex index %d out of range",i);
			}
			return;
		}
		facet_vertex_.push_back(vertex_[i]);
		facet_tex_vertex_.push_back(NULL);
	}

	void CTopologyBuilder::set_corner_tex_vertex(int i) 
	{
		if(i < 0 || i >= int(tex_vertex_.size())) 
		{
			if(!quiet_) {
				//                CryLog("MapBuilder") << "texvertex index " 
				//                                          << i << " out of range"
				//                                          << std::endl;
			}
			return;
		}
		*(facet_tex_vertex_.rbegin()) = tex_vertex_[i];
	}

	void CTopologyBuilder::lock_vertex(int i) 
	{
		if(i < 0 || i >= int(vertex_.size())) 
		{
			if(!quiet_) 
			{
				CryLog("MapBuilder vertex index %d out of range",i);
			}
			return;
		}
		is_locked_[vertex(i)] = true;
	}

	void CTopologyBuilder::unlock_vertex(int i) 
	{
		if(i < 0 || i >= int(vertex_.size())) 
		{
			if(!quiet_) 
			{
				CryLog("MapBuilder vertex index %d out of range",i);
			}
			return;
		}
		is_locked_[vertex(i)] = false;
	}

	void CTopologyBuilder::begin_facet_internal() 
	{
		transition(eS_Surface, eS_Facet);
		current_facet_ = target_->new_facet();
		first_vertex_in_facet_ = NULL;
		current_vertex_ = NULL;
		first_halfedge_in_facet_ = NULL;
		current_halfedge_ = NULL;
		first_tex_vertex_in_facet_ = NULL;
	}

	void CTopologyBuilder::end_facet_internal() 
	{
		transition(eS_Facet, eS_Surface);
		CHalfedge* h = new_halfedge_between(current_vertex_, first_vertex_in_facet_);

		CTopologyGraphMutator::link(current_halfedge_, h, 1);
		CTopologyGraphMutator::link(h, first_halfedge_in_facet_, 1);
		if(first_tex_vertex_in_facet_ != NULL) 
		{
			CTopologyGraphMutator::set_halfedge_tex_vertex(h, std::shared_ptr<CTexVertex>(first_tex_vertex_in_facet_)); 
		}
	}

	void CTopologyBuilder::add_vertex_to_facet_internal(CVertex* v) 
	{
		transition(eS_Facet, eS_Facet);

		if(first_vertex_in_facet_ == NULL) 
		{
			first_vertex_in_facet_ = v;
		} 
		else 
		{
			CHalfedge* new_halfedge = new_halfedge_between(current_vertex_, v);

			if(first_halfedge_in_facet_ == NULL) 
			{
				first_halfedge_in_facet_ = new_halfedge;
				CTopologyGraphMutator::make_facet_key(first_halfedge_in_facet_);
			} 
			else 
			{
				CTopologyGraphMutator::link(current_halfedge_, new_halfedge, 1);
			}
			current_halfedge_ = new_halfedge;
		}
		current_vertex_ = v;
	}

	void CTopologyBuilder::set_corner_tex_vertex_internal(std::shared_ptr<CTexVertex>& tv) 
	{
		transition(eS_Facet, eS_Facet);
		if(current_halfedge_ == NULL) 
		{
			first_tex_vertex_in_facet_ = tv;
		} 
		else 
		{
			CTopologyGraphMutator::set_halfedge_tex_vertex(current_halfedge_, tv);
		}
	}


	CVertex* CTopologyBuilder::current_vertex() 
	{
		return vertex_[vertex_.size() - 1];
	}

	CVertex* CTopologyBuilder::vertex(int i) 
	{
		return vertex_[i];
	}

	std::shared_ptr<CTexVertex>& CTopologyBuilder::current_tex_vertex() 
	{
		return tex_vertex_[tex_vertex_.size() - 1];
	}

	std::shared_ptr<CTexVertex>& CTopologyBuilder::tex_vertex(int i) 
	{
		return tex_vertex_[i];
	}

	CFacet* CTopologyBuilder::current_facet() 
	{
		return current_facet_;
	}

	CHalfedge* CTopologyBuilder::new_halfedge_between(CVertex* from, CVertex* to) 
	{
		assert(from != to);

		// Non-manifold edges have been removed
		// by the higher level public functions.
		// Let us do a sanity check anyway ...
		assert(find_halfedge_between(from, to) == NULL);

		CHalfedge* result = target_->new_halfedge();
		CTopologyGraphMutator::set_halfedge_facet(result, current_facet_);
		CTopologyGraphMutator::set_halfedge_vertex(result, to);

		CHalfedge* opposite = find_halfedge_between(to, from);
		if(opposite != NULL) {
			CTopologyGraphMutator::link(result, opposite, 2);
		}

		star_[from].push_back(result);
		CTopologyGraphMutator::set_vertex_halfedge(to, result);

		return result;
	}

	CHalfedge* CTopologyBuilder::find_halfedge_between(CVertex* from, CVertex* to) 
	{
		Star& star = star_[from];
		for(Star::iterator it = star.begin(); it != star.end(); it++) 
		{
			CHalfedge* cur = *it;
			if(cur-> vertex() == to) 
				return cur;
		}
		return NULL;
	}

	bool CTopologyBuilder::vertex_is_manifold(CVertex* v) 
	{
		if(v->halfedge() == NULL) 
		{
			// Isolated vertex, removed in subsequent steps
			return true;
		}

		// A vertex is manifold if the stored and the 
		// computed stars match (the number of halfedges
		// are tested).
		// Note: this test is valid only if the borders
		// have been constructed.

		return (int(star_[v].size()) == v->degree());
	}

	bool CTopologyBuilder::split_non_manifold_vertex(CVertex* v) 
	{
		if(vertex_is_manifold(v)) 
		{
			return false;
		}

		std::set<CHalfedge*> star;
		for(unsigned int i=0; i<star_[v].size(); i++) 
		{
			star.insert(star_[v][i]);
		}

		// For the first wedge, reuse the vertex
		disconnect_vertex(v->halfedge()->opposite(), v, star);

		// There should be other wedges (else the vertex
		// would have been manifold)
		assert(!star.empty());

		{
			is_locked[v] = true;
			// Create the vertices for the remaining wedges.
			while(!star.empty()) 
			{
				CVertex* new_v = copy_vertex(v);
				is_locked[new_v] = true;
				CHalfedge* h = *(star.begin());
				disconnect_vertex(h, new_v, star);
			}
		}

		return true;
	}


	void CTopologyBuilder::disconnect_vertex(CHalfedge* start_in, CVertex* v, std::set<CHalfedge*>& star) 
	{
		CHalfedge* start = start_in;

		star_[v].clear();

		//   Important note: in this class, all the Stars correspond to the
		// set of halfedges radiating FROM a vertex (i.e. h->vertex() != v
		// if h belongs to Star(v) ).

		assert(star.find(start) != star.end());


		//   Note: the following code needs a special handling
		// of borders, since borders are not correctly connected
		// in a non-manifold vertex (the standard next_around_vertex
		// iteration does not traverse the whole star)

		while(!start->is_border()) 
		{
			start = start->prev()->opposite();
			if(start == start_in) 
				break;
		}
		CTopologyGraphMutator::set_vertex_halfedge(v,start->opposite());

		CHalfedge* cur = start;
		CTopologyGraphMutator::set_halfedge_vertex(cur->opposite(), v);
		star_[v].push_back(cur);
		std::set<CHalfedge*>::iterator it = star.find(cur);
		assert(it != star.end());
		star.erase(it);

		while(!cur->opposite()->is_border()) 
		{
			cur = cur->opposite()->next();
			if(cur == start) 
				break;
			CTopologyGraphMutator::set_halfedge_vertex(cur->opposite(), v);
			std::set<CHalfedge*>::iterator it = star.find(cur);
			assert(it != star.end());
			star.erase(it);
			star_[v].push_back(cur);
		} 

		if(start->is_border()) 
		{
			CTopologyGraphMutator::link(cur->opposite(),start,1);
		}
	}


	void CTopologyBuilder::terminate_surface() 
	{

		// Step 1 : create the border halfedges, and setup the 'opposite'
		//   and 'vertex' pointers.
		std::vector<CHalfedge*> halfedgelist;
		FOR_EACH_HALFEDGE(CTopologyGraph, target_, it) 
		{
			halfedgelist.push_back(*it);
		}
		for (std::vector<CHalfedge*>::iterator it = halfedgelist.begin();it!= halfedgelist.end();it++)
		{
			if((*it)-> opposite() == NULL) 
			{
				CHalfedge* h = target_->new_halfedge();
				CTopologyGraphMutator::link(h, *it, 2);
				CTopologyGraphMutator::set_halfedge_vertex(h, (*it)-> prev()-> vertex());

				// Initialize texture coordinates
				CTopologyGraphMutator::set_halfedge_tex_vertex(h, (*it)-> prev()-> tex_vertex());

				// Used later to fix non-manifold vertices.
				star_[ (*it)-> vertex() ].push_back(h);
			}
		}

		// Step 2 : setup the 'next' and 'prev' pointers of the border.
		//
		FOR_EACH_HALFEDGE(CTopologyGraph, target_, it) 
		{
			if((*it)-> facet() == NULL) 
			{

				CHalfedge* next = (*it)-> opposite();
				while(next-> facet() != NULL) 
				{
					next = next-> prev()-> opposite();
				}
				CTopologyGraphMutator::set_halfedge_next(*it, next);

				CHalfedge* prev = (*it)-> opposite();
				while(prev-> facet() != NULL) 
				{
					prev = prev-> next()-> opposite();
				}
				CTopologyGraphMutator::set_halfedge_prev(*it, prev);
			}
		}

		// Step 3 : fix non-manifold vertices

		for(unsigned int i=0; i<vertex_.size(); i++) 
		{
			if(vertex_[i] == NULL) {
				continue;
			}
			if(split_non_manifold_vertex(vertex_[i])) 
			{
				nb_non_manifold_v_++;
			}
		}


		// Step 4 : create TexVertices

		FOR_EACH_HALFEDGE(CTopologyGraph, target_, it) 
		{
			if((*it)->tex_vertex() == NULL) 
			{
				std::shared_ptr<CTexVertex>& new_tv = std::shared_ptr<CTexVertex>(target_->new_tex_vertex());
				CHalfedge* cur = *it;
				do
				{
					CTopologyGraphMutator::set_halfedge_tex_vertex(cur, new_tv);
					cur = cur-> next()-> opposite();
				} 
				while(cur-> tex_vertex() == NULL);
				cur = (*it)-> opposite()-> prev();
				while((*it)-> tex_vertex() == NULL) 
				{
					CTopologyGraphMutator::set_halfedge_tex_vertex(cur, new_tv);
					cur = cur-> opposite()-> prev();
				}
			}
		}

		// Step 5 : check for isolated vertices
		for(unsigned int i=0; i<vertex_.size(); i++) 
		{
			if(vertex_[i] == NULL) 
			{
				continue;
			}
			if(star_[vertex_[i]].size() == 0) 
			{
				nb_isolated_v_++;
				target_->delete_vertex(vertex_[i]);
			}
		}

		target_->assert_is_valid();
	} 

	void CTopologyBuilder::transition(EState from, EState to) 
	{
		check_state(from);
		state_ = to;
	}

	void CTopologyBuilder::check_state(EState s) 
	{
		if(state_ != s) 
		{
			CryLog("MapBuilder invalid state: '%s' , expected '%s'",state_to_string(state_),state_to_string(s));
			bool ok = false;
			assert(ok);
		}
	}

	std::string CTopologyBuilder::state_to_string(EState s) 
	{
		switch(s) 
		{
		case eS_Initial:
			return "initial";
		case eS_Surface:
			return "surface";
		case eS_Facet: 
			return "facet";
		case eS_Final:
			return "final";
		default:
			return "unknown";
		}
		return "unknown";
	}

	void CTopologyBuilder::set_vertex(unsigned int v, const Vec3d& p) 
	{
		vertex(v)->set_point(p);
	}


	void CTopologyBuilder::create_vertices(unsigned int nb_v, bool with_colors) 
	{
		for(unsigned int i=0; i<nb_v; i++) 
		{
			add_vertex(Vec3d(0.0, 0.0, 0.0)) ;
		}
	}
}

