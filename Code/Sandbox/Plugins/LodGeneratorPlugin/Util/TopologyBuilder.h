#pragma once

#include "TopologyGraph.h"
namespace LODGenerator 
{
	class CTopologyBuilder
	{
	private:
		enum EState {eS_Initial, eS_Surface, eS_Facet, eS_Final} ;

	public:
		CTopologyBuilder(CTopologyGraph* target = NULL) : target_(target), state_(eS_Initial) 
		{
			quiet_ = false ;
		}

		void begin_surface() ;
		void end_surface(bool remove = true) ;
		void clear() ;
		void reset() ;

		void add_vertex(const Vec3d& p) ;
		void add_tex_vertex(const Vec2d& p) ;

		void add_vertex(unsigned int id, const Vec3d& p) ;
		void add_tex_vertex(unsigned int id, const Vec2d& p) ;

		void begin_facet() ;
		void end_facet() ;
		void add_vertex_to_facet(int i) ;
		void set_corner_tex_vertex(int i) ;

		void lock_vertex(int i) ;
		void unlock_vertex(int i) ;

		void set_vertex(unsigned int v, const Vec3d& p) ;

		void create_vertices(unsigned int nb_vertices, bool with_colors = false) ;

		CVertex* current_vertex() ;
		CVertex* vertex(int i) ;
		CFacet* current_facet() ;
		std::shared_ptr<CTexVertex>& current_tex_vertex() ;        
		std::shared_ptr<CTexVertex>& tex_vertex(int i) ;

		void set_quiet(bool quiet) { quiet_ = quiet ; }

	protected:

		void add_vertex_internal(unsigned int id, const Vec3d& p) ;	
		void add_vertex_internal(const Vec3d& p) ;

		void begin_facet_internal() ;
		void end_facet_internal() ;
		void add_vertex_to_facet_internal(CVertex* v) ;
		void set_corner_tex_vertex_internal(std::shared_ptr<CTexVertex>& tv) ;

		CVertex* copy_vertex(CVertex* from_vertex) ;

		std::vector<CVertex*>    facet_vertex_ ;
		std::vector<std::shared_ptr<CTexVertex>> facet_tex_vertex_ ;

	private:

		CHalfedge* new_halfedge_between(CVertex* from, CVertex* to) ;
		CHalfedge* find_halfedge_between(CVertex* from, CVertex* to) ;
		bool vertex_is_manifold(CVertex* v) ;
		bool split_non_manifold_vertex(CVertex* v) ;
		void disconnect_vertex(
			CHalfedge* start, CVertex* v, 
			std::set<CHalfedge*>& star
			) ;

		void terminate_surface() ;
		friend class MapSerializer_eobj ;

		void transition(EState from, EState to) ;
		void check_state(EState s) ;
		std::string state_to_string(EState s) ;

		EState state_ ;
		std::vector<CVertex*> vertex_ ;
		std::vector<std::shared_ptr<CTexVertex>> tex_vertex_ ;

		CFacet* current_facet_ ;
		CVertex* current_vertex_ ;
		CVertex* first_vertex_in_facet_ ;
		CHalfedge* first_halfedge_in_facet_ ;
		CHalfedge* current_halfedge_ ;
		std::shared_ptr<CTexVertex> first_tex_vertex_in_facet_ ;

		typedef std::vector<CHalfedge*> Star ;
		std::map<CVertex*,Star> star_ ;
		std::map<CVertex*,bool> is_locked_;

		int nb_isolated_v_ ;
		int nb_non_manifold_v_ ;
		int nb_duplicate_e_ ;
		bool quiet_ ;

		CTopologyGraph* target_;
		std::map<CVertex*,bool> is_locked;
	} ;
}


