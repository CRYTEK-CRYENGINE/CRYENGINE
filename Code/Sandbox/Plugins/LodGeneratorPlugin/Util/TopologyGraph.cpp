#include "StdAfx.h"
#include "TopologyGraph.h"
#include "MathTool.h"

namespace LODGenerator 
{
	
	void vertex_tangent(const CVertex* v, Vec3d normal, Vec3d& tangent, Vec3d& binormal);
	//-------------------------------------------------------------------------

	bool CVertex::is_valid() const 
	{
		return halfedge()->vertex() == this;
	}

	void CVertex::assert_is_valid() const 
	{
		assert(halfedge()->vertex() == this);
	}

	int CVertex::degree() const 
	{
		int result = 0;
		CHalfedge* it = halfedge();
		do {
			result++;
			it = it->next_around_vertex();
		} while(it != halfedge());
		return result;
	}

	bool CVertex::is_on_border() const 
	{
		CHalfedge* it = halfedge();
		do {
			if(it->is_border()) 
			{
				return true;
			}
			it = it->next_around_vertex();
		} while(it != halfedge());
		return false;
	}

	Vec2d CVertex::vertex_barycenter2d() 
	{
		double x = 0 ;
		double y = 0 ;
		double nb = 0 ;
		CHalfedge* it = this->halfedge() ;
		do 
		{
			const Vec2d& p = it->tex_coord();
			x += p.x ;
			y += p.y ;
			nb++ ;
			it = it->next_around_vertex() ;
		} 
		while(it != this->halfedge()) ;
		return Vec2d(x/nb, y/nb) ;
	}

	Vec3d CVertex::vertex_normal() 
	{
		Vec3d result(0,0,0);
		CHalfedge* cir = this->halfedge();
		unsigned int count = 0;
		do 
		{
			if (!cir->is_border()) 
			{
				count++;
				Vec3d v0 = cir->next()->direct();
				Vec3d v1 = cir->opposite()->direct();
				Vec3d n = v0.Cross(v1);
				result = result + n;
			}
			cir = cir->next_around_vertex();
		} 
		while (cir != this->halfedge());
		result = result.normalize();
		return result;
	}

	//-------------------------------------------------------------------------

	bool CHalfedge::is_valid() const 
	{
		return (
			(opposite()->opposite() == this) &&
			(next()->prev() == this) &&
			(prev()->next() == this) 
			);
	}

	void CHalfedge::assert_is_valid() const 
	{
		assert(opposite()->opposite() == this);
		assert(next()->prev() == this);
		assert(prev()->next() == this);
	}

	bool CHalfedge::is_facet_key() const 
	{
		return (facet_->halfedge() == this) ;
	}

	bool CHalfedge::is_vertex_key() const 
	{
		return (vertex_->halfedge() == this) ;
	}

	bool CHalfedge::is_edge_key() const 
	{
		// TODO: if the GarbageCollector is added, 
		// watch out, the edge keys can change...
		return (this < opposite_) ;
	}

	//-------------------------------------------------------------------------

	int CFacet::degree() const 
	{
		int result = 0;
		CHalfedge* it = halfedge();
		do {
			result++;
			it = it->next();
		} while(it != halfedge());
		return result;
	}

	Vec3d CFacet::facet_normal() 
	{
		Vec3d result(0,0,0);
		CHalfedge* cir = this->halfedge();
		do {
			Vec3d v0 = cir->direct();
			Vec3d v1 = cir->prev()->opposite()->direct();
			Vec3d n = v0.Cross(v1);
			result = result + n;
			cir = cir->next();
		} while(cir != this->halfedge());
		result = result.normalize();
		return result;
	}

	double CFacet::facet_area() 
	{
		double result = 0 ;
		CHalfedge* h = this->halfedge() ;
		const Vec3d& p = h->vertex()->point() ;
		h = h->next() ;
		do 
		{
			result += triangle_area(
				p,
				h->vertex()->point(),
				h->next()->vertex()->point() 
				) ;
			h = h->next() ;
		} 
		while(h != this->halfedge()) ;
		return result ;
	}

	Vec3d CFacet::facet_barycenter()
	{
		double x=0 ; double y=0 ; double z=0 ;
		int nb_vertex = 0 ;
		CHalfedge* cir = this->halfedge();
		do {
			nb_vertex++;
			x+= cir->vertex()->point().x ;
			y+= cir->vertex()->point().y ;
			z+= cir->vertex()->point().z ;
			cir = cir->next();
		} while (cir != this->halfedge());
		return  Vec3d(
			x / double(nb_vertex),
			y / double(nb_vertex),
			z / double(nb_vertex)
			) ;
	}

	bool CFacet::is_triangle() const {
		return ( halfedge()->next()->next()->next() == halfedge() );
	}

	bool CFacet::is_on_border() const {
		CHalfedge* it = halfedge();
		do {
			if(it->opposite()->is_border()) 
			{
				return true;
			}
			it = it->next();
		} while(it != halfedge());
		return false;
	}

	bool CFacet::is_valid() const 
	{
		return (halfedge()->facet() == this && degree() > 2);
	}

	void CFacet::assert_is_valid() const 
	{
		assert(halfedge()->facet() == this);
		assert(nb_edges() > 2);
	}

	double CFacet::facet_area2d()
	{
		return abs(facet_signed_area2d());
	}

	double CFacet::facet_signed_area2d() 
	{
		double result = 0 ;
		CHalfedge* it = this->halfedge() ;
		do 
		{
			const Vec2d& t1 = it-> tex_coord() ;
			const Vec2d& t2 = it-> next()-> tex_coord() ;
			result += t1.x * t2.y - t2.x * t1.y ;
			it = it->next() ;
		} 
		while(it != this->halfedge()) ;
		result /= 2.0 ;
		return result ;
	}

	//-------------------------------------------------------------------------	

    CTopologyGraph::CTopologyGraph() 
	{ 
    }
    
    CTopologyGraph::~CTopologyGraph() 
	{
        clear();
    } 

    void CTopologyGraph::compute_normals() 
	{
        compute_vertex_normals();
    }
    
	void CTopologyGraph::compue_vetex_minmax(Vec3d& vmin,Vec3d& vmax,Vec3d vcenter,float& vradius)
	{
		vmin = Vec3d(FLT_MAX,FLT_MAX,FLT_MAX);
		vmax = Vec3d(-FLT_MAX,-FLT_MAX,-FLT_MAX);

		FOR_EACH_VERTEX(CTopologyGraph, this, it) 
		{
			Vec3d pos = (*it)->point();
			vmin.x = min(vmin.x,pos.x);
			vmin.y = min(vmin.y,pos.y);
			vmin.z = min(vmin.z,pos.z);

			vmax.x = max(vmax.x,pos.x);
			vmax.y = max(vmax.y,pos.y);
			vmax.z = max(vmax.z,pos.z);
		}

		vcenter = (vmin + vmax)/2.0;
		vradius = 0;
		FOR_EACH_VERTEX(CTopologyGraph, this, it) 
		{
			Vec3d pos = (*it)->point();
			float dis = (pos - vcenter).len();
			vradius = max(dis,vradius);
		}

	}

	void CTopologyGraph::compute_vertex_tangent(bool smooth)
	{
		FOR_EACH_VERTEX(CTopologyGraph, this, it) 
		{
			CHalfedge* jt = (*it)->halfedge();
			Vec3d normal = jt->normal();

			Vec3d tangent,binormal;
			vertex_tangent(*it,normal,tangent,binormal);
			CHalfedge* h = (*it)->halfedge();
			do {
				h->set_tangent(tangent);
				h->set_binormal(binormal);
				h = h->next_around_vertex();
			} while(h != (*it)->halfedge());
		}
	}

    void CTopologyGraph::compute_vertex_normals(bool smooth) 
	{
        FOR_EACH_VERTEX(CTopologyGraph, this, it) 
		{
            Vec3d n = (*it)->vertex_normal();
            CHalfedge* h = (*it)->halfedge();
            do {
				h->set_normal(n);
                h = h->next_around_vertex();
            } while(h != (*it)->halfedge());
        }
    }

    void CTopologyGraph::compute_vertex_normal(CVertex* v) 
	{
        Vec3d n = v->vertex_normal();
        CHalfedge* h = v->halfedge();
        do {
           h->set_normal(n);
            h = h->next_around_vertex();
        } while(h != v->halfedge());
    }
   

    bool CTopologyGraph::is_triangulated() const 
	{
        FOR_EACH_FACET_CONST(CTopologyGraph, this, it) 
		{
            if(!(*it)->is_triangle()) 
			{
                return false;
            }
        }
        return true;
    }
  
	double CTopologyGraph::map_area() 
	{
		double result = 0 ;
		FOR_EACH_FACET_CONST(CTopologyGraph, this, it) 
		{
			result += (*it)->facet_area() ;
		}
		return result ;
	}

    void CTopologyGraph::clear() 
	{
		FOR_EACH_VERTEX(CTopologyGraph, this, it)
		{
			if(*it != NULL)
				delete (*it);
		}

		FOR_EACH_HALFEDGE(CTopologyGraph, this, it)
		{
			if(*it != NULL)
				delete (*it);
		}

		FOR_EACH_FACET(CTopologyGraph, this, it)
		{
			if(*it != NULL)
				delete (*it);
		}

        vertices_.clear();
        halfedges_.clear();
        facets_.clear();     
    }

    void CTopologyGraph::erase_all() 
	{
        clear();
    }

    CHalfedge* CTopologyGraph::new_edge() 
	{
        CHalfedge* h1 = new_halfedge();
        CHalfedge* h2 = new_halfedge();
        h1->set_opposite(h2);
        h2->set_opposite(h1);
        h1->set_next(h2);
        h2->set_next(h1);
        h1->set_prev(h2);
        h2->set_prev(h1);
        return h1;
    }

    void CTopologyGraph::delete_edge(CHalfedge* h) 
	{
        delete_halfedge(h->opposite());
        delete_halfedge(h);
    }

    CVertex* CTopologyGraph::new_vertex() 
	{
        CVertex* result = new CVertex();
		vertices_.push_back(result);
        return result;
    }
    
    CVertex* CTopologyGraph::new_vertex(const CVertex* rhs) 
	{
        CVertex* result = new CVertex();
        result->set_point(rhs->point());
		vertices_.push_back(result);
        return result;
    }

    CHalfedge* CTopologyGraph::new_halfedge() 
	{
        CHalfedge* result = new CHalfedge();
		halfedges_.push_back(result);

        return result;
    }

    CHalfedge* CTopologyGraph::new_halfedge(const CHalfedge* rhs) 
	{
        CHalfedge* result = new CHalfedge();
		halfedges_.push_back(result);
        return result;
    }
    
    CFacet* CTopologyGraph::new_facet() 
	{
        CFacet* result = new CFacet();
		facets_.push_back(result);
        return result;
    }

    CFacet* CTopologyGraph::new_facet(const CFacet* rhs) 
	{
        CFacet* result = new CFacet();
		facets_.push_back(result);
        return result;
    }

    CTexVertex* CTopologyGraph::new_tex_vertex() 
	{
        CTexVertex* result = new CTexVertex();
        return result;
    }

    CTexVertex* CTopologyGraph::new_tex_vertex(const CTexVertex* rhs) 
	{
        CTexVertex* result = new CTexVertex(rhs);
        return result;
    }
    
    void CTopologyGraph::delete_vertex(CVertex* v) 
	{
		std::vector<CVertex*>::iterator iter = std::find(vertices_.begin(),vertices_.end(),v);
		if(iter != vertices_.end())
			vertices_.erase(iter);

		delete v;
    }
    
    void CTopologyGraph::delete_halfedge(CHalfedge* h) 
	{
		std::vector<CHalfedge*>::iterator iter = std::find(halfedges_.begin(),halfedges_.end(),h);
		if(iter != halfedges_.end())
			halfedges_.erase(iter);

		delete h;
    }
    
    void CTopologyGraph::delete_facet(CFacet* f) 
	{
		std::vector<CFacet*>::iterator iter = std::find(facets_.begin(),facets_.end(),f);
		if(iter != facets_.end())
			facets_.erase(iter);

		delete f;
    }

    void CTopologyGraph::delete_tex_vertex(CTexVertex* tv) 
	{
        delete tv;
    }

    bool CTopologyGraph::is_valid() const {
        bool ok = true;
        { FOR_EACH_VERTEX_CONST(CTopologyGraph, this, it) 
		{
            ok = ok && (*it)->is_valid();
        }}
        { FOR_EACH_HALFEDGE_CONST(CTopologyGraph, this, it) 
		{
            ok = ok && (*it)->is_valid();
        }}
        { FOR_EACH_FACET_CONST(CTopologyGraph, this, it) 
		{
            ok = ok && (*it)->is_valid();
        }}
        return ok;
    }


    void CTopologyGraph::assert_is_valid() const {
        FOR_EACH_VERTEX_CONST(CTopologyGraph, this, it) 
		{
            (*it)->assert_is_valid();
        }
        FOR_EACH_HALFEDGE_CONST(CTopologyGraph, this, it) 
		{
            (*it)->assert_is_valid();
        }
        FOR_EACH_FACET_CONST(CTopologyGraph, this, it) 
		{
            (*it)->assert_is_valid();
        }
    }

	AABB CTopologyGraph::bbox() 
	{
		AABB result;
		result.Reset();
		FOR_EACH_VERTEX_CONST(CTopologyGraph, this, it) 
		{
			result.Add((*it)->point());
		}
		return result;
	}

	void CTopologyGraph::update_from_map(std::map<CVertex*,CVertex*>& orig_vertex) 
	{
		FOR_EACH_VERTEX(CTopologyGraph, this, it) 
		{
			CVertex* v = orig_vertex[*it] ;
			if(v != NULL) 
			{
				v->halfedge()->set_tex_coord((*it)->halfedge()->tex_coord()) ;
			} 
			else 
			{
				CryLog("nil orig_vertex !");;
			}
		}
	}

	//-------------------------------------------------------------------------

    CTopologyGraphMutator::~CTopologyGraphMutator() 
	{ 
		target_ = NULL;  
	}

    void CTopologyGraphMutator::set_target(CTopologyGraph* m) 
	{ 
		target_ = m; 
	}

    void CTopologyGraphMutator::link(CHalfedge* h1, CHalfedge* h2, int dim) 
	{
        switch(dim) 
		{
        case 1:
            h1->set_next(h2);
            h2->set_prev(h1);
            break;
        case 2:
            h1->set_opposite(h2);
            h2->set_opposite(h1);
            break;
        }
    }

    CHalfedge* CTopologyGraphMutator::new_edge(CHalfedge* rhs) 
	{
        CHalfedge* h1 = new_halfedge(rhs);
        CHalfedge* h2 = new_halfedge(rhs->opposite());
        h1->set_opposite(h2);
        h2->set_opposite(h1);
        h1->set_next(h2);
        h2->set_next(h1);
        h1->set_prev(h2);
        h2->set_prev(h1);
        return h1;
    }
        
    void CTopologyGraphMutator::set_vertex_on_orbit(CHalfedge* h, CVertex* v) 
	{
        CHalfedge* it = h;
        do 
		{
            it->set_vertex(v);
            it = it->next_around_vertex();
        } while(it != h);
    }
    

    void CTopologyGraphMutator::set_tex_vertex_on_orbit(CHalfedge* h, std::shared_ptr<CTexVertex> tv) 
	{
        CHalfedge* it = h;
        do 
		{
            it->set_tex_vertex(tv);
            it = it->next_around_vertex();
        } while(it != h);
    }

    void CTopologyGraphMutator::set_facet_on_orbit(CHalfedge* h, CFacet* f) 
	{
        CHalfedge* it = h;
        do 
		{
            it->set_facet(f);
            it = it->next();
        } while(it != h);
    }

	//-------------------------------------------------------------------------
	void vertex_tangent(const CVertex* v, Vec3d normal, Vec3d& tangent, Vec3d& binormal) 
	{
		Vec3d resultT(0,0,0);
		Vec3d resultB(0,0,0);
		CHalfedge* cir = v->halfedge();

		bool bFound=false;
		do {
			if (!cir->is_border()) 
			{

				CHalfedge* curEdge = cir;
				CHalfedge* nextEdge = curEdge->next();
				CHalfedge* oppositeEdge = cir->opposite();

				Vec2d uvNextEdge = nextEdge->tex_coord() - curEdge->tex_coord();
				// rot90cw
				Vec2d uvOppositeEdge =  Vec2d(uvNextEdge.y,-uvNextEdge.x);

				Vec3d posNextEdge = nextEdge->vertex()->point() - curEdge->vertex()->point();
				Vec3d posOppositeEdge = posNextEdge.Cross(normal);

				Vec2d u(1.0f, 0.0f);
				Vec2d v(0.0f, 1.0f);

				resultT+=uvNextEdge.Dot(u)*posNextEdge+uvOppositeEdge.Dot(u)*posOppositeEdge;
				resultB+=uvNextEdge.Dot(v)*posNextEdge+uvOppositeEdge.Dot(v)*posOppositeEdge;

				bFound = true;
			}
			cir = cir->next_around_vertex();
		} while (cir != v->halfedge());

		if(bFound == false)
		{
			resultB=Vec3d(1,0,0);
			resultT=Vec3d(0,1,0);
		}

		resultT = resultT.normalize();
		resultB = resultB.normalize();
			
		if ((resultT.Cross(resultB)).Dot(normal) < 0)
		{
			Vec3d temp = resultB;
			resultB=resultT;
			resultT=temp;
		}

		tangent = resultT;
		binormal = resultB;
	}

	bool CTopologyGraphMutator::can_unglue(CHalfedge* h) 
	{
		if(h-> is_border_edge()) 
			return false ;
		return (h->vertex()->is_on_border() || h->opposite()->vertex()->is_on_border()) ;
	}


	bool CTopologyGraphMutator::unglue(CHalfedge* h0, bool check) 
	{

		// TODO: tex vertices : if already dissociated,
		// do not create a new Texvertex.


		if(check && !can_unglue(h0)) 
		{
			return false ;
		}

		if(h0-> is_border_edge()) 
		{
			return false ;
		}

		CHalfedge* h1 = h0-> opposite() ;
		CVertex* v0 = h0-> vertex() ;
		CVertex* v1 = h1-> vertex() ;        

		bool v0_on_border = v0->is_on_border() ;
		bool v1_on_border = v1->is_on_border() ;

		assert(!check || (v0_on_border || v1_on_border)) ;

		CHalfedge* n0 = new_halfedge(h0) ;
		CHalfedge* n1 = new_halfedge(h1) ;
		link(n0, n1, 2) ;
		set_halfedge_next(n0,n1) ;
		set_halfedge_prev(n0,n1) ;
		set_halfedge_next(n1,n0) ;
		set_halfedge_prev(n1,n0) ;

		link(h0, n0, 2) ;
		link(h1, n1, 2) ;

		// If v1 is on the border, find the predecessor and
		// the successor of the newly created edges, and
		// split v1 into two vertices. 
		if(v1_on_border) 
		{
			CHalfedge* next0 = h0-> prev()-> opposite() ;
			while(!next0-> is_border()) 
			{
				next0 = next0-> prev()-> opposite() ;
			}
			assert(next0 != h0) ;
			CHalfedge* prev1 = h1-> next()-> opposite() ;
			while(!prev1-> is_border()) {
				prev1 = prev1-> next()-> opposite() ;
			}
			assert(prev1 != h1) ;
			assert(prev1-> vertex() == v1) ;
			assert(prev1-> next() == next0) ;
			link(n0, next0, 1) ;
			link(prev1, n1, 1) ;
			set_vertex_on_orbit(n0, new_vertex(v1)) ;

			// If the TexVertices are shared, create a new one.
			if(h1-> tex_vertex() == h0-> prev()-> tex_vertex()) 
			{
				set_tex_vertex_on_orbit(n0, std::shared_ptr<CTexVertex>(new_tex_vertex(h1->tex_vertex().get()))) ;
			} 
			else 
			{
				set_halfedge_tex_vertex(n0, h0-> prev()-> tex_vertex()) ;
			}

			make_vertex_key(n0) ;
			make_vertex_key(h1) ;
		} 
		else 
		{
			set_halfedge_vertex(n0, v1) ;
			set_halfedge_tex_vertex(n0, n0-> opposite()-> prev()-> tex_vertex()) ;
		}

		// If v0 is on the border, find the predecessor and
		// the successor of the newly created edges, and
		// split v0 into two vertices. 
		if(v0_on_border) 
		{
			CHalfedge* prev0 = h0-> next()-> opposite() ;
			while(!prev0-> is_border()) 
			{
				prev0 = prev0-> next()-> opposite() ;
			}
			assert(prev0 != h0) ;
			CHalfedge* next1 = h1-> prev()-> opposite() ;
			while(!next1-> is_border()) 
			{
				next1 = next1-> prev()-> opposite() ;
			}
			assert(next1 != h1) ;
			assert(prev0-> next() == next1) ;
			link(prev0, n0, 1) ;
			link(n1, next1, 1) ;
			set_vertex_on_orbit(n1, new_vertex(v0)) ;

			// If the TexVertices are shared, create a new one.
			if(h0-> tex_vertex() == h1-> prev()-> tex_vertex()) 
			{
				set_tex_vertex_on_orbit(n1, std::shared_ptr<CTexVertex>(new_tex_vertex(h0->tex_vertex().get()))) ;
			} 
			else 
			{
				set_halfedge_tex_vertex(n1,h1-> prev()-> tex_vertex()) ;
			}

			make_vertex_key(n1) ;
			make_vertex_key(h0) ;

		} 
		else 
		{
			set_halfedge_vertex(n1, v0) ;
			set_halfedge_tex_vertex(n1, n1-> opposite()-> prev()-> tex_vertex()) ;
		}

		return true ;
	}


	bool CTopologyGraphMutator::can_collapse_edge(CHalfedge* h) {

		{// disable glueing problems

			if ((!h->is_border() &&
				h->next()->opposite()->facet() 
				== h->prev()->opposite()->facet())
				|| (!h->opposite()->is_border() &&
				h->opposite()->next()->opposite()->facet() 
				== h->opposite()->prev()->opposite()->facet())
				)
				return false;
		}
		{// don't remove more than one vertex
			if (// it's a triangle
				h->next()->next()->next()==h            
				// the vertex is alone on border
				&& h->next()->opposite()->is_border()  
				&& h->prev()->opposite()->is_border()   
				)
				return false ;

			// the same on the other face
			if (// it's a triangle
				h->opposite()->next()->next()->next()==h            
				// the vertex is alone on border
				&& h->opposite()->next()->opposite()->is_border()
				&& h->opposite()->prev()->opposite()->is_border()
				)
				return false ;
		}

		// don't do stupid holes
		{
			if (
				(h->is_border()  && h->next()->next()->next()==h) ||
				(
				h->opposite()->is_border() && 
				h->opposite()->next()->next()->next()==h->opposite()
				)
				) { 
					return false;
			}
		}

		// don't merge holes (i.e. don't split surface)
		{
			if (
				!h->is_border() &&
				!h->opposite()->is_border() &&
				h->vertex()->is_on_border() &&
				h->opposite()->vertex()->is_on_border()
				) {
					return false;
			}
		}

		// be carefull of the toblerone case (don't remove volumes)
		{
			CHalfedge* cir = h ;
			int nb_twice=0;

			std::set<CVertex*> marked ;

			// do { 
			//    cir->opposite()->vertex()->set_id(0);
			//    cir = cir->next_around_vertex();
			// } while ( cir != h);

			cir = h->opposite();
			do { 
				marked.insert(cir->opposite()->vertex()) ;
				cir = cir->next_around_vertex();
			} while ( cir != h->opposite());

			cir = h;
			do {
				if (
					marked.find(cir->opposite()->vertex()) != marked.end()
					) {
						nb_twice++;
				}
				marked.insert(cir->opposite()->vertex()) ;
				cir = cir->next_around_vertex();
			}while ( cir != h);

			if (h->next()->next()->next()==h)  {
				nb_twice--;
			}
			if (h->opposite()->next()->next()->next()==h->opposite()) { 
				nb_twice--;
			}

			if (nb_twice > 0) {
				//std::cerr<<" \nbe carefull of the toblerone case";
				return false;
			}
		}
		return true ;


		/*

		if(
		!h->next()->opposite()->is_border() ||
		!h->prev()->opposite()->is_border()
		) {
		return false ;
		}
		if(
		!h->opposite()->next()->opposite()->is_border() || 
		!h->opposite()->prev()->opposite()->is_border()
		) {
		return false ;
		}
		return true ;
		*/
	}

	bool CTopologyGraphMutator::collapse_edge(CHalfedge* h) {

		if(!can_collapse_edge(h)) {
			return false ;
		}

		CVertex* dest = h->vertex() ;

		// everyone has the same vertex
		{	
			CVertex* v = h->opposite()->vertex() ;
			CHalfedge* i ;
			CHalfedge* ref;
			i = ref = h->opposite();
			do {
				CHalfedge* local = i ;
				set_halfedge_vertex(local, dest) ;
				i = i->next_around_vertex() ;
			} while (i != ref);
			delete_vertex(v);
		}


		// remove links to current edge (facet & vertex)
		CHalfedge* hDle = h;
		if ( !h->is_border() ) {
			set_facet_halfedge( hDle->facet(), hDle->next() ) ;
		}

		if (!h->opposite()->is_border()) {
			set_facet_halfedge(
				hDle->opposite()->facet(), hDle->opposite()->next() 
				) ;
		}
		set_vertex_halfedge( dest, hDle->next()->opposite() ) ;

		CHalfedge* f0 = h->next() ;
		CHalfedge* f1 = h->opposite()->prev() ;

		// update prev/next links
		{ 
			CHalfedge* e = h->next() ;      
			link(hDle-> prev(), e, 1) ;
			e = hDle->opposite()->next() ;
			link(hDle->opposite()->prev(), e, 1) ;
		}

		// remove null faces
		if (f0->next()->next() == f0) {
			remove_null_face(f0);
		}

		if (f1->next()->next() == f1) {
			remove_null_face(f1);
		}

		delete_edge(hDle);

		return true ;
	}

	void CTopologyGraphMutator::remove_null_face(CHalfedge* f0) {
		CHalfedge* border = f0-> next()-> opposite() ;

		//remove facet
		delete_facet(f0-> facet());

		// set link (fake set_opposite) to others
		link(f0, border-> next(), 1) ;
		link(border-> prev(), f0, 1) ;
		set_halfedge_facet(f0, border-> facet()) ;

		// set links from others
		if (!f0->is_border()){
			make_facet_key(f0) ;
		}
		make_vertex_key(f0) ;
		make_vertex_key(f0-> opposite()) ;
		delete_edge(border); 
	}
}

