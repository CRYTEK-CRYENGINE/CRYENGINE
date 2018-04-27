#pragma once

#include "TopologyGraph.h"
#include "Component.h"

namespace LODGenerator 
{
	class CAverageDirection2d 
	{
	public:
		CAverageDirection2d();
		void begin();
		void add_vector(const Vec2d& V);
		void end();
		const Vec2d& average_direction() const 
		{ 
			assert(result_is_valid_);
			return result_; 
		}
	private:
		double M_[3];
		Vec2d result_;
		bool result_is_valid_;
	};

    class CPacker 
	{
    public:
        CPacker();
        void pack_map_components(ComponentList& surfaces);
        void pack_map(CTopologyGraph* surface);
        int image_size_in_pixels() const { return image_size_in_pixels_; }
        void set_image_size_in_pixels(int size) { image_size_in_pixels_ = size;}
        int margin_width_in_pixels() const { return margin_width_in_pixels_; }
        void set_margin_width_in_pixels(int width) { margin_width_in_pixels_ = width;} 
		void normalize_tex_coords(CTopologyGraph* surface);

    protected:
        void normalize_surface_components(ComponentList& surfaces);
        void normalize_surface_component(CComponent* component);
        CHalfedge* largest_border(CComponent* component);

    private:

        double total_area_3d_;
        int image_size_in_pixels_;
        int margin_width_in_pixels_;
		std::map<CHalfedge*,bool> is_visited_;
		std::map<CHalfedge*,int> seam_type_;

        CPacker(const CPacker& rhs);
        CPacker& operator=(const CPacker& rhs);
    };
}

