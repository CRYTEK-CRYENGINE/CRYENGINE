#include "StdAfx.h"

#include "Validator.h"
#include "Component.h"
#include "TopologyGraph.h"

namespace LODGenerator 
{
    CValidator::CValidator() 
	{
        graph_size_ = 1024;
        graph_mem_ = new uint8[graph_size_ * graph_size_];
        x_left_  = new int[graph_size_];
        x_right_ = new int[graph_size_];
        max_overlap_ratio_ = 0.005;
        max_scaling_ = 20.0;
        min_fill_ratio_ = 0.25;
    }

    CValidator::~CValidator() 
	{
        delete[] graph_mem_; graph_mem_ = NULL;
        delete[] x_left_;  x_left_ = NULL;
        delete[] x_right_; x_right_ = NULL;
    }

	bool CValidator::component_is_valid(CComponent* comp) 
	{
		FOR_EACH_HALFEDGE(CComponent, comp, it) 
		{
			if(_isnan((*it)->tex_coord().x) || _isnan((*it)->tex_coord().y)) 
			{
				CryLog("UVGenerator NaN detected in tex coords");
				return false;
			}
		}

		//   Check global overlaps and "wire-like" charts
		// (wasting parameter space)
		compute_fill_and_overlap_ratio(comp);
		CryLog("UVGenerator Fill ratio = %f",fill_ratio());
		CryLog("UVGenerator Overlap ratio =  %f",overlap_ratio());

		double comp_scaling = component_scaling(comp);
		CryLog("UVGenerator Scaling = %f",comp_scaling);

		// Ignore problems for small components.
		// TODO: check if we can remove that
		// (unfortunately, does not seems so...)
		if(comp->size_of_facets() <= 10) 
		{
			CryLog("UVGenerator ----> PASS: small component, #facets= %d",comp->size_of_facets());
			return true;
		}

		// If more than 'min_fill_ratio_' of the parameter space is empty, 
		// reject chart.
		if(_isnan(fill_ratio()) || fill_ratio() < min_fill_ratio_) 
		{
			CryLog("UVGenerator  ----> REJECT: filling ratio");
			return false;
		}

		// If more than 'max_overlap_ratio_' of the pixels correspond to more than one
		// facet, reject chart.
		if(_isnan(overlap_ratio()) || overlap_ratio() > max_overlap_ratio_) 
		{
			CryLog("UVGenerator ----> REJECT: overlap ratio");
			return false;
		}

		if(_isnan(comp_scaling) || comp_scaling > max_scaling_) 
		{
			CryLog("UVGenerator ----> REJECT: scaling");
			return false;
		}

		CryLog("UVGenerator ----> PASS.");
		return true;
	}

	double CValidator::component_scaling(CComponent* comp) 
	{

		// Compute largest facet area.
		double max_area = 0;
		FOR_EACH_FACET(CComponent, comp, it) 
		{
			max_area = MAX((*it)->facet_area(), max_area);
		}

		// Ignore facets smaller than 1% of the largest facet.
		double area_treshold = 0.001 * max_area;

		std::vector<double> facet_scaling;
		facet_scaling.reserve(comp->size_of_facets());

		FOR_EACH_FACET(CComponent, comp, it) 
		{
			double area   = (*it)->facet_area();
			double area2d = (*it)->facet_area2d();
			if(area > area_treshold) {
				facet_scaling.push_back(area2d / area);
			} 
		}

		// Ignore 1% of the values at each end.
		std::sort(facet_scaling.begin(), facet_scaling.end());
		int offset = int(double(facet_scaling.size()) * 0.01);
		int begin = offset;
		int end = facet_scaling.size() - 1 - offset;
		return facet_scaling[end] / facet_scaling[begin];
	}

	void CValidator::compute_fill_and_overlap_ratio(CComponent* comp)
	{
		begin_rasterizer(comp);
		FOR_EACH_FACET(CComponent,comp,f) 
		{
			CHalfedge* cur = (*f)-> halfedge();
			CHalfedge* h0 = cur;
			cur = cur-> next();
			CHalfedge* h1 = cur;
			cur = cur-> next();
			CHalfedge* h2 = cur;
			do 
			{
				rasterize_triangle(h0->tex_coord(), h1->tex_coord(), h2->tex_coord());
				h1 = cur;
				cur = cur-> next();
				h2 = cur;
			} 
			while (h2 != h0);
		}
		end_rasterizer();
	}

	void CValidator::begin_rasterizer(CComponent* comp) 
	{
		::memset(graph_mem_, 0, graph_size_ * graph_size_);

		AABB box = comp->bbox2d();

		user_x_min_  = box.min.x;
		user_y_min_  = box.min.y;
		user_width_  = box.GetSize().x;
		user_height_ = box.GetSize().y;
		user_size_ = MAX(user_width_, user_height_);
	}

    
    void CValidator::transform(const Vec2d& p, int& x, int& y)
	{
        x = int( double(graph_size_-1) * (p.x - user_x_min_) / user_size_);
        y = int( double(graph_size_-1) * (p.y - user_y_min_) / user_size_);
		x = MIN(MAX(x,0),graph_size_-1);
		y = MIN(MAX(y,0),graph_size_-1);
    }
    
    void CValidator::rasterize_triangle( const Vec2d& p1, const Vec2d& p2, const Vec2d& p3) 
	{
        int x[3];
        int y[3];

        transform(p1,x[0],y[0]);
        transform(p2,x[1],y[1]);
        transform(p3,x[2],y[2]);

        int ymin = 32767;
        int ymax = -1;
        
        for(int i=0; i<3; i++) 
		{
            ymin = MIN(ymin, y[i]);
            ymax = MAX(ymax, y[i]);
        }

        int signed_area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        bool ccw = (signed_area < 0);

        if(ymin == ymax) 
		{
            return;
        }

        for(int i=0; i<3; i++) 
		{
            int j=(i+1)%3;
            int x1 = x[i];
            int y1 = y[i];
            int x2 = x[j];
            int y2 = y[j];
            if(y1 == y2) 
			{
                continue;
            }
            bool is_left = (y2 < y1) ^ ccw;

            // I want the set of lit pixels to be
            // independant from the order of the 
            // extremities.
            bool swp = false;
            if(y2 == y1) 
			{
                if(x1 > x2) 
				{
                    swp = 1;
                }
            } 
			else 
			{
                if(y1 > y2) 
				{
                    swp = 1;
                }
            }
            if(swp) 
			{
                int tmp;
                tmp = x2;
                x2 = x1;
                x1 = tmp;
                tmp = y2;
                y2 = y1;
                y1 = tmp;
            }

            // Bresenham algo.
            int dx = x2 - x1;
            int dy = y2 - y1;
            int sx = dx > 0 ? 1 : -1;
            int sy = dy > 0 ? 1 : -1;
            dx *= sx;
            dy *= sy;
            int x = x1;
            int y = y1;
            
            int* line_x = is_left ? x_left_ : x_right_;
            line_x[y] = x;
            
            int e = dy - 2 * dx;
            while(y < y2 - 1) 
			{             
                y += sy;
                e -= 2 * dx;
                
                while(e < 0) 
				{
                    x += sx;
                    e += 2 * dy;
                }
                
                line_x[y] = x;
            }
            
            line_x[y2] = x2;
        }
        
        for(int y = ymin; y < ymax; y++) 
		{
            for(int x = x_left_[y]; x < x_right_[y]; x++) 
			{
                graph_mem_[y * graph_size_ + x]++;
            }
        }
    }
    
    void CValidator::end_rasterizer() 
	{
        int nb_filled = 0;
        int nb_overlapped = 0;

        int width = 0;
        int height = 0;
        if(user_width_ > user_height_) 
		{
            width  = graph_size_;
            height = int((user_height_ * double(graph_size_)) / user_width_);
        } 
		else 
		{
            height = graph_size_;
            width  = int((user_width_ * double(graph_size_)) / user_height_);
        }

        for(int x=0; x<width; x++) 
		{
            for(int y=0; y<height; y++) 
			{
                uint8 pixel = graph_mem_[y * graph_size_ + x];
                if(pixel > 0) 
				{
                    nb_filled++;
                    if(pixel > 1) 
					{
                        nb_overlapped++;
                    }
                }
            }
        }

        fill_ratio_ = double(nb_filled) / double(width * height);
        overlap_ratio_ = double(nb_overlapped) / double(width * height);
    }
}
