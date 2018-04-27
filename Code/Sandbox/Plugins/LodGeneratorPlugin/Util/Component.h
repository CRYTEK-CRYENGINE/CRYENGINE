#pragma once

#include <vector>
#include <map>
namespace LODGenerator 
{
	class CVertex;
	class CHalfedge;
	class CFacet;
	class CHalfedge;
	class CTopologyGraph;
	class CComponent
	{
public:
		CComponent(CTopologyGraph* map);

		typedef std::vector<CVertex*>::iterator Vertex_iterator;
		typedef std::vector<CHalfedge*>::iterator Halfedge_iterator;
		typedef std::vector<CFacet*>::iterator Facet_iterator;
		typedef std::vector<CVertex*>::const_iterator Vertex_const_iterator;
		typedef std::vector<CHalfedge*>::const_iterator Halfedge_const_iterator;
		typedef std::vector<CFacet*>::const_iterator Facet_const_iterator;

public:
		Vertex_iterator vertices_begin();
		Vertex_iterator vertices_end();
		Halfedge_iterator halfedges_begin();
		Halfedge_iterator halfedges_end();
		Facet_iterator facets_begin();
		Facet_iterator facets_end();
		Vertex_const_iterator vertices_begin() const;
		Vertex_const_iterator vertices_end() const;
		Halfedge_const_iterator halfedges_begin() const;
		Halfedge_const_iterator halfedges_end() const;
		Facet_const_iterator facets_begin() const;
		Facet_const_iterator facets_end() const;
		unsigned int size_of_vertices() const;
		unsigned int size_of_halfedges() const;
		unsigned int size_of_facets() const;
		CTopologyGraph* map() const;
		std::vector<CVertex*>& vertices();
		std::vector<CHalfedge*>& halfedges();
		std::vector<CFacet*>& facets();

public:
		double component_area();
		double map_area();   
		double component_border_length();
		AABB bbox2d();
		double signed_area2d();
		double area2d();
		double area();
		void translate(const Vec3d& v);
		void translate2d(const Vec2d& v);

		CTopologyGraph* CComponent::component_to_topology(std::map<CVertex*,CVertex*>& orig_CVertex);

		CTopologyGraph* map()
		{
			return map_;
		}

	private:
		CTopologyGraph* map_;
		std::vector<CVertex*> vertices_;
		std::vector<CHalfedge*> halfedges_;
		std::vector<CFacet*> facets_;
	};
	typedef std::vector<CComponent*> ComponentList;

	class CComponentMutator 
	{
	public:
		CComponentMutator(CComponent* target = NULL) : target_(target) { }
		~CComponentMutator() { target_ = NULL;  }
		CComponent* target() { return target_; }
		void set_target(CComponent* m) { target_ = m; }

	public:
		std::vector<CVertex*>& vertices() { return target_->vertices(); }
		std::vector<CHalfedge*>& halfedges() { return target_->halfedges(); }
		std::vector<CFacet*>& facets() { return target_->facets(); }

	private:
		CComponent* target_;
	};

	class CComponentExtractor : public CComponentMutator 
	{
	public:
		ComponentList extract_components(CTopologyGraph* map);
	};

	class CComponentTopology 
	{
    public:
        CComponentTopology(const CComponent* comp);
        
        /**
         * returns the Euler-Poincarre characteristic,
         * xi = 2 for a sphere, xi = 1 for a disc.
         */
        int xi() const;

        /** returns 0 for a closed surface. */
        int number_of_borders() const { return number_of_borders_; }

        bool is_closed() const { return number_of_borders_ == 0; }
        bool is_almost_closed(int max_border_size = 3) const;
        
        /** returns the number of edges in the largest border. */
        int largest_border_size() const { return largest_border_size_; }

        bool is_sphere() const {
            return (number_of_borders() == 0) && (xi() == 2);
        }
        
        bool is_disc() const {
            return (number_of_borders() == 1) && (xi() == 1);
        }

        bool is_cylinder() const {
            return (number_of_borders() == 2) && (xi() == 0);
        }

        bool is_torus() const {
            return (number_of_borders() == 0) && (xi() == 0);
        }


    private:
        const CComponent* component_;
        int number_of_borders_;
        int largest_border_size_;
    };
};