#pragma once

#include <complex>

namespace LODGenerator 
{
	typedef std::complex<double> ComplexF;

	class CTopologyGraph;
	class CComponent;
	class CVertex;
	class CHalfedge;
	class CFacet;
	class CLinearSolver;
	class CSolver;
	class CAnglesToUV
	{
	public:
		CAnglesToUV();
		bool do_parameterize_disc(CTopologyGraph* map,std::map<CHalfedge*,double>* angle);
		ComplexF ComplexF_angle(CHalfedge* h0, CHalfedge* h1, CHalfedge* h2);
		void setup_conformal_map_relations(CLinearSolver& solver, CHalfedge* h0,CHalfedge* h1,CHalfedge* h2);

	public:
		void set_user_locks(bool x) { user_locks_ = x; }

	private:
		void get_bounding_box();
		void project_on_principal_plane();
		void principal_axes(CHalfedge* h, Vec3d& p, Vec3d& v1, Vec3d& v2);
		void solver_to_map(const CLinearSolver& solver);
		void principal_axes(Vec3d& p, Vec3d& v1, Vec3d& v2);
		void normalize_uv(Vec3d& p);
		void box_axes(const AABB& box_in, Vec3d& center, Vec3d& v1, Vec3d& v2);
		void get_border_extrema(CHalfedge* h, const Vec3d& center, const Vec3d& V,CVertex*& vx_min, CVertex*& vx_max);
		CHalfedge* largest_border();
		void setup_conformal_map_relations(CLinearSolver& solver );
		void setup_conformal_map_relations(CLinearSolver& solver, CFacet* f);
		bool halfedge_is_discarded(CHalfedge* h);
		bool facet_is_discarded(CFacet* f);
	private:
		void begin_parameterization(CTopologyGraph* map);
		void end_parameterization();
		void enumerate_vertices();
	private:
		std::map<CHalfedge*,double>* angle_;

	private:
		bool user_locks_;
		int	nb_distinct_vertices_;

	private:
		std::map<CVertex*,int> vertex_id_;
		Vec3d  center_;
		double   radius_;
		CTopologyGraph* map_;
	};
}


