#pragma once
#pragma warning( disable:4005)

namespace LODGenerator 
{
	class CHalfedge;
	class CFacet;
	class CVertex
	{
	public:
		CVertex() : halfedge_(NULL) {  }
		CVertex(const Vec3d& p) : halfedge_(NULL), point_(p) {  }
		~CVertex() { halfedge_ = NULL; }

		const Vec3d& point() const     { return point_; }
		Vec3d& point()                 { return point_; }
		void set_point(const Vec3d& p) { point_ = p;    }

		CHalfedge* halfedge() const { return halfedge_; }

		bool is_valid() const;
		void assert_is_valid() const;

		int degree() const;
		bool is_on_border() const;

		Vec2d vertex_barycenter2d();
		Vec3d vertex_normal();

	public:
		void set_halfedge(CHalfedge* h) { halfedge_ = h; }

	private:
		CHalfedge* halfedge_;
		Vec3d point_;

	};

	class CTexVertex
	{
	public:
		CTexVertex(){}	
		CTexVertex(const CTexVertex* rhs) : tex_coord_(rhs->tex_coord()) {}
		~CTexVertex() { }
		const Vec2d& tex_coord() const  { return tex_coord_; }
		Vec2d& tex_coord()              { return tex_coord_; }
		void set_tex_coord(const Vec2d& p) { tex_coord_ = p; }

		const Vec3d& normal() const     { return normal_; }
		Vec3d& normal()                 { return normal_; }
		void set_normal(const Vec3d& p) { normal_ = p;    }

		const Vec3d& binormal() const     { return binormal_; }
		Vec3d& binormal()                 { return binormal_; }
		void set_binormal(const Vec3d& p) { binormal_ = p;    }

		const Vec3d& tangent() const     { return tangent_; }
		Vec3d& tangent()                 { return tangent_; }
		void set_tangent(const Vec3d& p) { tangent_ = p;    }

	private:
		Vec2d tex_coord_;
		Vec3d normal_;
		Vec3d binormal_;
		Vec3d tangent_;
	};

	class CHalfedge
	{
	public:
		CHalfedge() : 
			opposite_(NULL), next_(NULL), 
			prev_(NULL), facet_(NULL), vertex_(NULL) ,tex_vertex_(NULL){
		}
		~CHalfedge() 
		{ 
			opposite_ = NULL; next_ = NULL;
			prev_ = NULL; facet_ = NULL; vertex_ = NULL;
		}

		std::shared_ptr<CTexVertex> tex_vertex() const { return tex_vertex_; }

		const Vec2d& tex_coord() const 
		{ 
			return tex_vertex()->tex_coord(); 
		}

		Vec2d& tex_coord() 
		{ 
			return tex_vertex()->tex_coord(); 
		}

		const Vec3d& normal() const 
		{ 
			return tex_vertex()->normal(); 
		}

		Vec3d& normal() 
		{ 
			return tex_vertex()->normal(); 
		}

		const Vec3d& tangent() const 
		{ 
			return tex_vertex()->tangent(); 
		}

		Vec3d& tangent() 
		{ 
			return tex_vertex()->tangent(); 
		}

		const Vec3d& binormal() const 
		{ 
			return tex_vertex()->binormal(); 
		}

		Vec3d& binormal()
		{ 
			return tex_vertex()->binormal(); 
		}

		void set_tex_coord(const Vec2d& tex_coord_in) 
		{
			tex_vertex()->set_tex_coord(tex_coord_in); 
		}

		void set_normal(const Vec3d& normal_in) 
		{
			tex_vertex()->set_normal(normal_in); 
		}

		void set_tangent(const Vec3d& tangent_in) 
		{
			tex_vertex()->set_tangent(tangent_in); 
		}

		void set_binormal(const Vec3d& binormal_in) 
		{
			tex_vertex()->set_binormal(binormal_in); 
		}

		CHalfedge* opposite() const { return opposite_; }
		CHalfedge* next() const { return next_; }
		CHalfedge* prev() const { return prev_; }
		CHalfedge* next_around_vertex() const {
			return opposite()->prev();
		}
		CHalfedge* prev_around_vertex() const {
			return next()->opposite();
		}

		Vec3d direct() const
		{
			return vertex()->point() - prev()->vertex()->point();
		}

		CFacet* facet() const { return facet_; }
		CVertex* vertex() const { return vertex_; }
		bool is_border() const { return facet_ == NULL; }
		bool is_border_edge() const { 
			return is_border() || opposite()->is_border(); 
		}

		bool is_facet_key() const;

		bool is_vertex_key() const;

		bool is_edge_key() const;
		CHalfedge* edge_key() const { 
			return is_edge_key() ? const_cast<CHalfedge*>(this) : opposite(); 
		}

		bool is_valid() const;
		void assert_is_valid() const;

	public:
		void set_opposite(CHalfedge* h) { opposite_ = h; }
		void set_next(CHalfedge* h) { next_ = h; }
		void set_prev(CHalfedge* h) { prev_ = h; }
		void set_facet(CFacet* f) { facet_ = f; }
		void set_vertex(CVertex* v) { vertex_ = v; }
		void set_tex_vertex(std::shared_ptr<CTexVertex> t) { tex_vertex_ = t; }

	private:
		CHalfedge* opposite_;
		CHalfedge* next_;
		CHalfedge* prev_;
		CFacet* facet_;
		CVertex* vertex_;
		//TexVertex* tex_vertex_;
		std::shared_ptr<CTexVertex> tex_vertex_;
	};


	class CFacet
	{
	public:
		CFacet() : halfedge_(NULL) { }
		~CFacet() { halfedge_ = NULL; }
		CHalfedge* halfedge() const { return halfedge_; }
		int degree() const;
		int nb_edges() const { return degree(); }
		int nb_vertices() const { return degree(); }
		bool is_on_border() const;
		bool is_triangle() const;
		bool is_valid() const;
		void assert_is_valid() const;
		Vec3d facet_normal();
		double facet_area();
		Vec3d facet_barycenter();
		double facet_signed_area2d();
		double facet_area2d();

	public:
		void set_halfedge(CHalfedge* h) { halfedge_ = h; }

	private:
		CHalfedge* halfedge_;
	};

	class CTopologyGraph 
	{
	public:

		typedef std::vector<CVertex*>::iterator Vertex_iterator;
		typedef std::vector<CHalfedge*>::iterator Halfedge_iterator;
		typedef std::vector<CFacet*>::iterator Facet_iterator;

		typedef std::vector<CVertex*>::const_iterator Vertex_const_iterator;
		typedef std::vector<CHalfedge*>::const_iterator Halfedge_const_iterator;
		typedef std::vector<CFacet*>::const_iterator Facet_const_iterator;     

		CTopologyGraph();
		virtual ~CTopologyGraph();

		Vertex_iterator vertices_begin()    { return vertices_.begin();  }
		Vertex_iterator vertices_end()      { return vertices_.end();    }
		Halfedge_iterator halfedges_begin() { return halfedges_.begin(); }
		Halfedge_iterator halfedges_end()   { return halfedges_.end();   }
		Facet_iterator facets_begin()       { return facets_.begin();    }
		Facet_iterator facets_end()         { return facets_.end();      } 

		Vertex_const_iterator vertices_begin() const { 
			return vertices_.begin();  
		}

		Vertex_const_iterator vertices_end() const { 
			return vertices_.end();    
		}

		Halfedge_const_iterator halfedges_begin() const { 
			return halfedges_.begin(); 
		}
		Halfedge_const_iterator halfedges_end() const { 
			return halfedges_.end();   
		}
		Facet_const_iterator facets_begin() const { 
			return facets_.begin();    
		}
		Facet_const_iterator facets_end() const { 
			return facets_.end();      
		} 

		int size_of_vertices() const  { return vertices_.size();  }
		int size_of_halfedges() const { return halfedges_.size(); }
		int size_of_facets() const    { return facets_.size();    }       

		double map_area();

		void clear();
		void erase_all();

		void compue_vetex_minmax(Vec3d& vmin,Vec3d& vmax,Vec3d vcenter,float& vradius);

		void compute_vertex_tangent(bool smooth = true);

		void compute_normals();
		void compute_vertex_normals(bool smooth = true);
		void compute_vertex_normal(CVertex* v);

		bool is_triangulated() const;

		bool is_valid() const;

		void assert_is_valid() const;

		AABB bbox();

		void update_from_map(std::map<CVertex*,CVertex*>& orig_vertex);

	public:
		CHalfedge* new_edge();
		void delete_edge(CHalfedge* h);

		CVertex* new_vertex();
		CTexVertex* new_tex_vertex();
		CHalfedge* new_halfedge();
		CFacet* new_facet();

		CVertex* new_vertex(const CVertex* rhs);
		CTexVertex* new_tex_vertex(const CTexVertex* rhs);        
		CHalfedge* new_halfedge(const CHalfedge* rhs);
		CFacet* new_facet(const CFacet* rhs);

		void delete_vertex(CVertex* v);
		void delete_halfedge(CHalfedge* h);
		void delete_facet(CFacet* f);
		void delete_tex_vertex(CTexVertex* tv);

	protected:
		void reorient_facet( CHalfedge* first);


	protected:
		void notify_add_vertex(CVertex* v);
		void notify_remove_vertex(CVertex* v);
		void notify_add_halfedge(CHalfedge* h);
		void notify_remove_halfedge(CHalfedge* h);
		void notify_add_facet(CFacet* f);
		void notify_remove_facet(CFacet* f);

	private:
		std::vector<CVertex*> vertices_;
		std::vector<CHalfedge*> halfedges_;
		std::vector<CFacet*> facets_;
	};

	class CTopologyGraphMutator 
	{
	public:

		typedef CTopologyGraph::Vertex_iterator Vertex_iterator;
		typedef CTopologyGraph::Halfedge_iterator Halfedge_iterator;
		typedef CTopologyGraph::Facet_iterator Facet_iterator;

		typedef CTopologyGraph::Vertex_const_iterator Vertex_const_iterator;
		typedef CTopologyGraph::Halfedge_const_iterator Halfedge_const_iterator;
		typedef CTopologyGraph::Facet_const_iterator Facet_const_iterator;

		CTopologyGraphMutator(CTopologyGraph* target = NULL) : target_(target) { }
		virtual ~CTopologyGraphMutator(); 
		CTopologyGraph* target() const { return target_; }
		virtual void set_target(CTopologyGraph* m); 


		bool can_unglue(CHalfedge* h);
		bool unglue(CHalfedge* h0, bool check);

	protected:

		
		void set_vertex_on_orbit(CHalfedge* h, CVertex* v);
		void set_tex_vertex_on_orbit(CHalfedge* h, std::shared_ptr<CTexVertex> tv);
		void set_facet_on_orbit(CHalfedge* h, CFacet* f);

		void make_facet_key(CHalfedge* h, CFacet* f) {
			f->set_halfedge(h);
			h->set_facet(f);
		}


		CHalfedge* new_edge() { return target_->new_edge(); }
		void delete_edge(CHalfedge* h) { target_->delete_edge(h); }

		CVertex*    new_vertex()     { return target_->new_vertex();     }
		CTexVertex* new_tex_vertex() { return target_->new_tex_vertex(); }
		CHalfedge*  new_halfedge()   { return target_->new_halfedge();   }
		CFacet*     new_facet()      { return target_->new_facet();      }

		CVertex* new_vertex(const CVertex* rhs) { 
			return target_->new_vertex(rhs);      
		}
		CTexVertex* new_tex_vertex(const CTexVertex* rhs) { 
			return target_->new_tex_vertex(rhs); 
		}
		CHalfedge* new_halfedge(const CHalfedge* rhs) { 
			return target_->new_halfedge(rhs);
		}
		CHalfedge* new_edge(CHalfedge* rhs);
		CFacet* new_facet(const CFacet* rhs) { 
			return target_->new_facet(rhs);      
		}


		void delete_vertex(CVertex* v) { target_->delete_vertex(v); }
		void delete_tex_vertex(CTexVertex* tv) { 
			target_->delete_tex_vertex(tv);
		}
		void delete_halfedge(CHalfedge* h) { 
			target_->delete_halfedge(h); 
		}
		void delete_facet(CFacet* f) { target_->delete_facet(f); }

	public:

		void remove_null_face(CHalfedge* f0);
		bool can_collapse_edge(CHalfedge* h);
		bool collapse_edge(CHalfedge* h);


		static void set_vertex_halfedge(CVertex* v, CHalfedge* h) 
		{ 
			v->set_halfedge(h); 
		}

		static void set_halfedge_opposite(CHalfedge* h1, CHalfedge* h2) 
		{ 
			h1->set_opposite(h2);
		}

		static void set_halfedge_next(CHalfedge* h1, CHalfedge* h2) 
		{ 
			h1->set_next(h2);
		}

		static void set_halfedge_prev(CHalfedge* h1, CHalfedge* h2) 
		{ 
			h1->set_prev(h2);
		}

		static void set_halfedge_facet(CHalfedge* h, CFacet* f) 
		{ 
			h->set_facet(f);
		}

		static void set_halfedge_vertex(CHalfedge* h, CVertex* v) 
		{ 
			h->set_vertex(v);
		}

		static void set_facet_halfedge(CFacet* f, CHalfedge* h) 
		{ 
			f->set_halfedge(h);
		}

		static void set_halfedge_tex_vertex(CHalfedge* h, std::shared_ptr<CTexVertex> t) 
		{
			h->set_tex_vertex(t);
		}

		static void link(CHalfedge* h1, CHalfedge* h2, int dim);

		static void make_vertex_key(CHalfedge* h) 
		{ 
			h->vertex()->set_halfedge(h); 
		}

		static void make_vertex_key(CHalfedge* h, CVertex* v) {

			v->set_halfedge(h);
			h->set_vertex(v);
		}

		static void make_facet_key(CHalfedge* h) 
		{
			h->facet()->set_halfedge(h);
		}

		void merge_tex_vertices(const CVertex* v_in) 
		{
			CVertex* v = const_cast<CVertex*>(v_in) ;
			Vec2d u = v->vertex_barycenter2d() ;
			set_tex_vertex_on_orbit(v-> halfedge(), std::shared_ptr<CTexVertex>(new_tex_vertex())) ;
			v->halfedge()->set_tex_coord(u) ;
		}



	private:
		CTopologyGraph* target_;
	};

#define FOR_EACH_VERTEX(T,map,it)                         \
	for(                                                  \
	T::Vertex_iterator it=(map)->vertices_begin();    \
	it!=(map)->vertices_end(); it++                   \
	)

#define FOR_EACH_HALFEDGE(T,map,it)                          \
	for(                                                     \
	T::Halfedge_iterator it=(map)->halfedges_begin();    \
	it!=(map)->halfedges_end(); it++                     \
	)

#define FOR_EACH_EDGE(T,map,it)                              \
	for(                                                     \
	T::Halfedge_iterator it=(map)->halfedges_begin();    \
	it!=(map)->halfedges_end(); it++                     \
	) if((*it)->is_edge_key())                                  \


#define FOR_EACH_FACET(T,map,it)                       \
	for(                                               \
	T::Facet_iterator it=(map)->facets_begin();    \
	it!=(map)->facets_end(); it++                  \
	)

#define FOR_EACH_VERTEX_CONST(T,map,it)                       \
	for(                                                      \
	T::Vertex_const_iterator it=map->vertices_begin();    \
	it!=map->vertices_end(); it++                         \
	)

#define FOR_EACH_HALFEDGE_CONST(T,map,it)                        \
	for(                                                         \
	T::Halfedge_const_iterator it=map->halfedges_begin();    \
	it!=map->halfedges_end(); it++                           \
	)

#define FOR_EACH_EDGE_CONST(T,map,it)                           \
	for(                                                        \
	T::Halfedge_const_iterator it=(map)->halfedges_begin(); \
	it!=(map)->halfedges_end(); it++                        \
	) if(it->is_edge_key())                                     \


#define FOR_EACH_FACET_CONST(T,map,it)                     \
	for(                                                   \
	T::Facet_const_iterator it=map->facets_begin();    \
	it!=map->facets_end(); it++                        \
	)
}

