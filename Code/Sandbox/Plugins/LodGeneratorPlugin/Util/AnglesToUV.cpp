#include "StdAfx.h"
#include "AnglesToUV.h"
#include "LinearSolver.h"
#include "PrincipalAxes.h"
#include <stack>
#include "Solver.h"
#include "TopologyGraph.h"
#include "MathTool.h"

namespace LODGenerator 
{
	bool is_nan(Vec2d v)
	{
		if (_isnan(v.x) || _isnan(v.y))
		{
			return true;
		}
		return false;
	}

	bool is_nan(Vec3d v)
	{
		if (_isnan(v.x) || _isnan(v.y) || _isnan(v.z))
		{
			return true;
		}
		return false;
	}

	inline Vec3d barycenter(const Vec3d& p1, const Vec3d& p2) 
	{
		return Vec3d(
			0.5 * (p1.x + p2.x),
			0.5 * (p1.y + p2.y),
			0.5 * (p1.z + p2.z)
			);
	}

	void CAnglesToUV::normalize_uv(Vec3d& p) 
	{
		Vec3d v = 1.0 / radius_ * (p - center_);
		p = Vec3d(v.x, v.y, v.z);
	}

    CAnglesToUV::CAnglesToUV() 
	{
		user_locks_ = false;
		angle_ = NULL;
    }

	void CAnglesToUV::get_bounding_box() 
	{
		// Compute center
		double x = 0;
		double y = 0;
		double z = 0;
		FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
			x += (*it)->point().x;
			y += (*it)->point().y;
			z += (*it)->point().z;
		}
		x /= double(map_->size_of_vertices());
		y /= double(map_->size_of_vertices());
		z /= double(map_->size_of_vertices());
		center_ = Vec3d(x,y,z);

		// Compute radius
		radius_ = 0;
		FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
			double r = ((*it)->point() - center_).len();
			radius_ = max(radius_, r);
		}

		if(radius_ < 1e-20) {
			//            std::cerr << "Tiny component" << std::endl;
			radius_ = 1.0;
		}
	}
	AABB border_bbox(CHalfedge* h) 
	{
		AABB result;
		CHalfedge* cur = h;
		do 
		{
			result.Add(cur->vertex()->point());
			cur = cur->next();
		} 
		while(cur != h);
		return result;
	}

	void CAnglesToUV::box_axes(const AABB& box_in, Vec3d& center, Vec3d& v1, Vec3d& v2) 
	{
		
			Vec3d p_min = box_in.min;
			Vec3d p_max = box_in.max;

			normalize_uv(p_min);
			normalize_uv(p_max);

			AABB box;
			box.Add(p_min);
			box.Add(p_max);

			center = barycenter(box.min,box.max);

			double dx = box.max.x - box.min.x;
			double dy = box.max.y - box.min.y;
			double dz = box.max.z - box.min.z;
			if(dz > dy && dz > dx) {
				v1 = Vec3d(1,0,0);
				v2 = Vec3d(0,1,0);
			} else if(dy > dx && dy > dz) {
				v1 = Vec3d(0,0,1);
				v2 = Vec3d(1,0,0);
			} else {
				v1 = Vec3d(0,1,0);
				v2 = Vec3d(0,0,1);
			}
	}

	void CAnglesToUV::principal_axes(CHalfedge* h, Vec3d& center, Vec3d& v1, Vec3d& v2) 
	{
			assert(h->is_border());

			CPrincipalAxes3d axes;
			axes.begin_points();
			CHalfedge* cur = h;
			do 
			{
				Vec3d p = cur->vertex()->point();
				normalize_uv(p);
				axes.point(p);
				cur = cur->next();
			} 
			while(cur != h);
			axes.end_points();
			center = axes.center();
			v1 = axes.axis(0);
			v2 = axes.axis(1);

			// If numerical problems occured, project relative to the shortest
			// edge of the bounding box.
			if(is_nan(center) || is_nan(v1) || is_nan(v2)) 
			{
				AABB box = border_bbox(h);
				box_axes(box, center, v1, v2);
			}
	}

	void CAnglesToUV::project_on_principal_plane() 
	{
		Vec3d center;
		Vec3d v1;
		Vec3d v2;
		principal_axes(center, v1, v2);
		v1 = v1.normalize();
		v2 = v2.normalize();
		FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
			double u = ((*it)->point() - center).Dot(v1);
			double v = ((*it)->point() - center).Dot(v2);
			(*it)->halfedge()->set_tex_coord(Vec2d(u,v));
		}
	}


	Vec3d perpendicular(const Vec3d& V) 
	{
		int min_index = 0;
		double c = ::fabs(V[0]);
		double cur = ::fabs(V[1]);
		if(cur < c) {
			min_index = 1;
			c = cur;
		}
		cur = ::fabs(V[2]);
		if(cur < c) {
			min_index = 2;
		}
		Vec3d result;
		switch(min_index) {
		case 0:
			result = Vec3d(0, -V.z, V.y);
			break;
		case 1:
			result = Vec3d(V.z, 0, -V.x);
			break;
		case 2:
			result = Vec3d(-V.y, V.x, 0);
			break;
		}
		return result;
	}

	void CAnglesToUV::principal_axes(Vec3d& center, Vec3d& v1, Vec3d& v2) 
	{
		Vec3d average_normal(0,0,0);
		FOR_EACH_FACET(CTopologyGraph, map_, it) 
		{
			average_normal = average_normal + (*it)->facet_normal();
		}
		if( (average_normal).len() > 1e-30) 
		{
			average_normal = average_normal.normalize();
			double w_min =  1e30;
			double w_max = -1e30;
			FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
			{
				double w = (*it)->point().Dot(average_normal);
				w_min = min(w, w_min);
				w_max = max(w, w_max);
			}
			if((w_max - w_min) < 1e-6) 
			{
				v1 = perpendicular(average_normal).normalize();
				v2 = (average_normal.Cross(v1)).normalize();
				double cx = 0;
				double cy = 0;
				double cz = 0;
				FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
				{
					cx += (*it)->point().x;
					cy += (*it)->point().y;
					cz += (*it)->point().z;
				}
				center = Vec3d(
					cx / double(map_->size_of_vertices()),
					cy / double(map_->size_of_vertices()),
					cz / double(map_->size_of_vertices())
					);
				return;
			}
		}


		CPrincipalAxes3d axes;
		axes.begin_points();
		FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
			Vec3d p = (*it)->point();
			normalize_uv(p);
			axes.point(p);
		}
		axes.end_points();
		center = axes.center();
		v1 = axes.axis(0);
		v2 = axes.axis(1);

		// If numerical problems occured, project relative to the shortest
		// edge of the bounding box.
		if(is_nan(center) || is_nan(v1) || is_nan(v2)) 
		{
			AABB box = map_->bbox();
			box_axes(box, center, v1, v2);
		} 

		if(average_normal.len() > 1e-30 && (v1.Cross(v2)).Dot(average_normal) < 0) 
		{
			v2 = -1.0 * v2;
		}

	}

	void CAnglesToUV::solver_to_map(const CLinearSolver& solver) 
	{
		FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
			double u = solver.variable(2 * vertex_id_[*it]    ).value();
			double v = solver.variable(2 * vertex_id_[*it] + 1).value();
			(*it)->halfedge()->set_tex_coord(Vec2d(u,v));
		}
	}

	void CAnglesToUV::enumerate_vertices() 
	{
		int cur_id = 0;
		FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
			vertex_id_[*it] = cur_id;
			cur_id++;
		}        
	}

	void CAnglesToUV::begin_parameterization(CTopologyGraph* map) 
	{
		map_ = map;
		get_bounding_box();
		enumerate_vertices();
		nb_distinct_vertices_ = map_->size_of_vertices();
	}

	void CAnglesToUV::end_parameterization() 
	{
		map_ = NULL;
	}

	void CAnglesToUV::get_border_extrema(CHalfedge* h, const Vec3d& center, const Vec3d& V,CVertex*& vx_min, CVertex*& vx_max) 
	{
		vx_min = NULL;
		vx_max = NULL;
		CHalfedge* cur = h;
		double v_min =  big_double;
		double v_max = -big_double;
		do {
			Vec3d p = cur->vertex()->point();
			normalize_uv(p);
			double v = (p - center).Dot(V);

			if(v < v_min) {
				v_min = v;
				vx_min = cur->vertex();
			}

			if(v > v_max) {
				v_max = v;
				vx_max = cur->vertex();
			}

			cur = cur->next();
		} while(cur != h);
	}

	CHalfedge* CAnglesToUV::largest_border() 
	{
		CHalfedge* result = NULL;
		int max_size = 0;

		std::map<CHalfedge*,bool> is_visited;
		FOR_EACH_HALFEDGE(CTopologyGraph, map_, it) 
		{
			is_visited[*it] = false;
		}

		FOR_EACH_HALFEDGE(CTopologyGraph, map_, it) 
		{
			if((*it)->is_border() && !is_visited[*it]) 
			{
				int cur_size = 0;
				CHalfedge* cur = *it;
				do 
				{
					cur_size++;
					is_visited[cur] = true;
					cur = cur->next();
				} 
				while(cur != *it);
				if(cur_size > max_size) 
				{
					max_size = cur_size;
					result = *it;
				}
			}
		}
		return result;
	}

	void CAnglesToUV::setup_conformal_map_relations( CLinearSolver& solver ) 
	{
		FOR_EACH_FACET(CTopologyGraph, map_, it) 
		{
			setup_conformal_map_relations(solver,*it) ;            
		}
	}

	bool CAnglesToUV::halfedge_is_discarded(CHalfedge* h) 
	{
		return (vertex_id_[h->prev()->vertex()] == vertex_id_[h->vertex()]) ;
	}

	bool CAnglesToUV::facet_is_discarded(CFacet* f) 
	{
		CHalfedge* h = f->halfedge() ;
		do {
			if(!halfedge_is_discarded(h)) {
				return false ;
			}
			h = h->next() ;
		} while(h != f->halfedge()) ;
		return true ;
	}

	void CAnglesToUV::setup_conformal_map_relations(CLinearSolver& solver, CFacet* f) 
	{
			// Do not install relations for null facets.
			if(facet_is_discarded(f)) {
				return ;
			}

			// "virtually triangulate" the facet
			CHalfedge* cur = f-> halfedge() ;
			CHalfedge* h0 = cur ;
			cur = cur-> next() ;
			CHalfedge* h1 = cur ;
			cur = cur-> next() ;
			CHalfedge* h2 = cur ;

			do 
			{
				setup_conformal_map_relations(solver, h0, h1, h2) ;
				h1 = cur ;
				cur = cur-> next() ;
				h2 = cur ;
			} 
			while (h2 != h0) ;
	}

    bool CAnglesToUV::do_parameterize_disc(CTopologyGraph* map, std::map<CHalfedge*,double>* angle) 
	{
        angle_ = angle;

		map_ = map;
		std::map<CVertex*,bool> is_locked;

		// Filter the case where there is a single triangle
		// Note: flat parts could be also detected here.
		// TODO: something smarter: we do not need the principal plane
		//  etc..., just ortho-normalize the triangle's basis and project
		//  on it...
		if(map_->size_of_facets() < 2 || map_->size_of_vertices() < 4) 
		{
			get_bounding_box();
			project_on_principal_plane();
			return true;
		}

		// Step 0 : normalization, vertex numbering, attributes
		begin_parameterization(map);
		if(map_->size_of_facets() < 2 || nb_distinct_vertices_ < 4) {
			project_on_principal_plane();
			end_parameterization();
			return true;
		}

		// Step 1 : find the two vertices to pin
		CHalfedge* h = largest_border();
		assert(h != NULL); // Happens if the part is closed.

		Vec3d center;
		Vec3d V1,V2;
		principal_axes(h, center, V1, V2);

		CVertex* vx_min = NULL;
		CVertex* vx_max = NULL;
		get_border_extrema(h, center, V1, vx_min, vx_max);

		// Step 2 : construct a Sparse Least Squares solver
		CLinearSolver solver(2 * nb_distinct_vertices_);
		solver.set_least_squares(true);

		// Step 3 : Pin the two extrema vertices
		if(user_locks_) 
		{
			FOR_EACH_VERTEX(CTopologyGraph, map_, it) {
				if(is_locked[*it]) 
				{
					const Vec2d& t = (*it)->halfedge()->tex_coord();
					solver.variable(2*vertex_id_[(*it)]  ).lock();
					solver.variable(2*vertex_id_[(*it)]  ).set_value(t.x);
					solver.variable(2*vertex_id_[(*it)]+1).lock();
					solver.variable(2*vertex_id_[(*it)]+1).set_value(t.y);
				}
			}
		} else {
			solver.variable(2 * vertex_id_[vx_min]    ).lock();
			solver.variable(2 * vertex_id_[vx_min]    ).set_value(0);
			solver.variable(2 * vertex_id_[vx_min] + 1).lock();     
			solver.variable(2 * vertex_id_[vx_min] + 1).set_value(0);   
			solver.variable(2 * vertex_id_[vx_max]    ).lock();
			solver.variable(2 * vertex_id_[vx_max]    ).set_value(0);
			solver.variable(2 * vertex_id_[vx_max] + 1).lock();        
			solver.variable(2 * vertex_id_[vx_max] + 1).set_value(1);        
			is_locked[vx_min] = true;
			is_locked[vx_max] = true;
		}

		// Step 4 : build the linear system to solve
		solver.begin_system();
		setup_conformal_map_relations(solver);
		solver.end_system();

		// Step 5 : solve the system and 
		//   copy the solution to the texture coordinates.
		if(solver.solve()) 
		{
			solver_to_map(solver);
		} 
		else 
		{
			// If solving fails, try a projection.
			// Note that when used in the AtlasMaker,
			// if the result of the projection is invalid,
			// it will be rejected by the ParamValidator.
			project_on_principal_plane();
		}

		// Step 6 : unbind the attributes
		end_parameterization();

        return true;
    }



    ComplexF CAnglesToUV::ComplexF_angle(CHalfedge* h0,CHalfedge* h1,CHalfedge* h2) 
	{
        ComplexF Z;
        if((*angle_).size()>0) 
		{
            double alpha0 = (*angle_)[h0];
            double alpha1 = (*angle_)[h1];
            double alpha2 = (*angle_)[h2];
            double scaling = sin(alpha1) / sin(alpha2);
            Z = ComplexF(::cos(alpha0), ::sin(alpha0));
			Z *= scaling;
        }
        return Z;
    } 

    void CAnglesToUV::setup_conformal_map_relations(CLinearSolver& solver, CHalfedge* h0,CHalfedge* h1,CHalfedge* h2) 
	{
        
        CVertex* v0 = h0->vertex();
        CVertex* v1 = h1->vertex();
        CVertex* v2 = h2->vertex();

        int id0 = vertex_id_[v0];
        int id1 = vertex_id_[v1];
        int id2 = vertex_id_[v2];

        // Note  : 2*id + 0 --> u
        //         2*id + 1 --> v

        int u0_id = 2*id0 ;
        int v0_id = 2*id0 + 1;
        int u1_id = 2*id1 ;
        int v1_id = 2*id1 + 1;
        int u2_id = 2*id2 ;
        int v2_id = 2*id2 + 1;
        
        const Vec3d& p0 = h0->vertex()->point();
        const Vec3d& p1 = h1->vertex()->point();
        const Vec3d& p2 = h2->vertex()->point();

        double area = triangle_area(p0,p1,p2);
        double s = sqrt(area);

        ComplexF Z = ComplexF_angle(h0, h1, h2);

        double a = Z.real();
        double b = Z.imag();

        solver.begin_row();
        solver.set_right_hand_side(0.0);
        solver.add_coefficient(u0_id, 1-a);
        solver.add_coefficient(v0_id,  b);
        solver.add_coefficient(u1_id,  a);
        solver.add_coefficient(v1_id, -b);
        solver.add_coefficient(u2_id, -1);
        solver.scale_row(s);
        solver.end_row();

        solver.begin_row();
        solver.set_right_hand_side(0.0);
        solver.add_coefficient(u0_id, -b);
        solver.add_coefficient(v0_id, 1-a);
        solver.add_coefficient(u1_id,  b);
        solver.add_coefficient(v1_id,  a);
        solver.add_coefficient(v2_id, -1);
        solver.scale_row(s);
        solver.end_row();
    }
}
