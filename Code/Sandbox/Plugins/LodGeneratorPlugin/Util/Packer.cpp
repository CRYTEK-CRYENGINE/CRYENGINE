#include "StdAfx.h"
#include <algorithm>
#include "Packer.h"
#include "MathTool.h"

namespace LODGenerator 
{
    class CComponentBBox
	{
    public:

        CComponentBBox(CComponent* surf, const Vec2d& min, const Vec2d& max) : min_(min), max_(max), surf_(surf), min_func_(NULL), max_func_(NULL), nb_steps_(0) 
		{
        }

        ~CComponentBBox()
		{
            free();
            surf_ = NULL;
        }

        CComponentBBox(const CComponentBBox& rhs) 
		{
            copy(rhs);
        }
        
        CComponentBBox& operator=(const CComponentBBox& rhs) 
		{
            if(&rhs != this) {
                free();
                copy(rhs);
            }
            return *this;    
        }

        void init_max_and_min_func( double step, double margin, int margin_width_in_pixels) 
		{
            if(min_func_ != NULL)
			{
                delete[] min_func_;
            }
            if(max_func_ != NULL) 
			{
                delete[] max_func_;
            }

            nb_steps_ = int(1.0 + (max_.x - min_.x) / step );
            min_func_ = new double[nb_steps_];
            max_func_ = new double[nb_steps_];
            for (int i=0;i<nb_steps_;i++){
                min_func(i) = max_.y - min_.y;
                max_func(i) = 0;
            }
            { // TODO:  Would be easier to iterate on facets (now we can)...
                FOR_EACH_VERTEX(CComponent, surf_, vi) 
				{
                    CVertex *v = *vi;
                    double maxX=0;
                    double minX=max_.x;
                    double maxY=0;
                    double minY=max_.y;
                    CHalfedge* cir = v->halfedge();
                    do 
					{
                        Vec2d pt = cir->opposite()->tex_coord();
                        maxX = MAX(maxX, pt.x);
                        minX = MIN(minX, pt.x);
                        maxY = MAX(maxY, pt.y);
                        minY = MIN(minY, pt.y);
                        cir = cir->next_around_vertex();
                    } 
					while (cir != v->halfedge());

                    int posx_min = int(double((minX-min_.x) / step) -  margin_width_in_pixels);

                    int posx_max = int(double((maxX-min_.x) / step) +  margin_width_in_pixels);
                    for (int posx = posx_min; posx < posx_max; posx++) 
					{
                        if (posx>=0 && posx<nb_steps_) {
                            min_func(posx) = MIN(min_func(posx), minY - margin);
                            max_func(posx) = MAX(max_func(posx), maxY + margin);
                        }
                    }
                }
            }
            
        }

        double& max_func(int i) 
		{
            assert(i>=0 && i<nb_steps_);
            return max_func_[i];
        }

        double& min_func(int i) 
		{
            assert(i>=0 && i<nb_steps_);
            return min_func_[i];
        }

        Vec2d size() const 
		{
            return max_-min_;
        }

        double area() 
		{
            Vec2d s = size();
            return s.x*s.y;
        }
        
        void translate(const Vec2d& v)
		{
            min_=min_+v;
            max_=max_+v;
			surf_->translate2d(v);
        }
        
        bool operator<(const CComponentBBox& r) const 
		{
            return size().y > r.size().y;
        }

        const Vec2d& min() const { return min_; }
        const Vec2d& max() const { return max_; }
        
        Vec2d& min() { return min_; }
        Vec2d& max() { return max_; }

        CComponent* surface() { return surf_; }


    protected:
     
        void copy(const CComponentBBox& rhs) {
            surf_ = rhs.surf_;
            nb_steps_ = rhs.nb_steps_;

            if(rhs.min_func_ != NULL) {
                min_func_ = new double[nb_steps_];
                max_func_ = new double[nb_steps_];
                for(int i=0; i<nb_steps_; i++) {
                    min_func_[i] = rhs.min_func_[i];
                    max_func_[i] = rhs.max_func_[i];
                }
            } else {
                assert(rhs.max_func_ == NULL);
                min_func_ = NULL;
                max_func_ = NULL;
            }
            min_ = rhs.min_;
            max_ = rhs.max_;
        }
        
        void free() {
            delete[] min_func_;
            delete[] max_func_;
            min_func_ = NULL;
            max_func_ = NULL;
            nb_steps_ = 0;
        }

    private: 
        Vec2d min_, max_;
        CComponent* surf_;
        double* min_func_;
        double* max_func_;
        int nb_steps_;
    };


    class TetrisPacker {
    public :
        
        TetrisPacker() {
            nb_xpos_ = 1024;
            height_ = new double[nb_xpos_];
            image_size_in_pixels_  = 1024;
            margin_width_in_pixels_ = 4;
        }
        
        ~TetrisPacker() {
            delete[] height_;
            height_ = NULL;
        }

        void set_image_size_in_pixels(int size) {
            image_size_in_pixels_ = size;
        }

        int margin_width_in_pixels() const { return margin_width_in_pixels_; }

        void set_margin_width_in_pixels(int width) {
            margin_width_in_pixels_ = width;
        } 
        
        void add(const CComponentBBox& r){
            data_.push_back(r);
        }

        // function used to order the CComponentBBoxes
        static bool compare(
            const CComponentBBox& b0, const CComponentBBox& b1
        ) {
            return  b0.size().y > b1.size().y;
        }
        
        void add_margin(double margin_size) {
            for (unsigned int i=0;i<data_.size();i++){
                data_[i].translate(
                    Vec2d(
                        -data_[i].min().x + margin_size,
                        -data_[i].min().y + margin_size
                    )
                );
                data_[i].max() = data_[i].max() + 
                    Vec2d( 2.0 * margin_size, 2.0 * margin_size);
                assert(data_[i].max().x>0);
                assert(data_[i].max().y>0);
                data_[i].min() = Vec2d(0,0);
            }
        }
        
        double max_height() {
            double result = 0;
            for (unsigned int i=0;i<data_.size();i++) 
			{
                result = MAX(result, data_[i].max().y);
            }
            return result;
        }

        void recursive_apply() {
            // compute margin
            double area = 0;      
            for (unsigned int numrect = 0; numrect <data_.size();numrect++ ) 
			{
                area += data_[numrect].area();
            }
            double margin = 
                (::sqrt(area) / image_size_in_pixels_) * 
                margin_width_in_pixels_;
            add_margin(margin); 

            // find a first solution
            apply(margin);
            double scoreSup = max_height();
            double borneSup = width_;
            double decalborne = 0.5 * ::sqrt(scoreSup * width_);

            // dichotomy
            for  (int i=0;i<10;i++ )
			{
                double new_borne = borneSup - decalborne;
                apply(margin,new_borne);
                double max = max_height();
                if (max < new_borne){
                    borneSup = borneSup - decalborne;
                }
                decalborne/=2;
            }
            apply(margin, borneSup);
        }


        void apply(double margin, double width =-1) {
            width_ = width;
            for (unsigned int i=0; i<data_.size(); i++) {
                data_[i].translate(
                    Vec2d(
                        -data_[i].min().x,
                        -data_[i].min().y
		    )
                );
            }

            // sort CComponentBBoxes by their heights ...
            std::sort(data_.begin(),data_.end(),compare);            
            
            {//find the best width
                double max_bbox_width = 0;
                for (unsigned int i=0; i<data_.size(); i++) {
                    max_bbox_width= MAX(
                        max_bbox_width, data_[i].size().x
                    );
                }
                
                if ( width == -1){
                    // try to have a square : width_ = sqrt(area);
                    double area =0;
                
                    for (unsigned int i=0; i<data_.size(); i++){
                        area += data_[i].area();
                    }

                    width_ = ::sqrt(area) *1.1;
                

                    // resize if a square seems to be too bad 
                    // (the first piece is too high)

                    if (data_[0].size().y > width_) {
                        width_ = area / data_[0].size().y;
                    }
                }

                // be sure all surface can fit in the width
                width_ = MAX(
                    max_bbox_width * (double(nb_xpos_ + 2) / double(nb_xpos_)),
                    width_
                );	
            }
            // set the step depending on the width and the discretisation
            step_ = width_ / nb_xpos_;


            // init local min and max height functions
            {
                for (
                    unsigned int numrect = 0; numrect <data_.size(); numrect++
                ) {
                data_[numrect].init_max_and_min_func(
                    step_, margin, margin_width_in_pixels_
                );
                }
            }

            // init global height function
            { for (int x=0; x<nb_xpos_; x++) {
                height(x)=0;
            }}
            
            {for (unsigned int i=0;i<data_.size();i++) {
                place(data_[i]);
            }}
        }
        
        
    private:

        double& height(int x) {
            assert(x >= 0 && x < nb_xpos_);
            return height_[x];
        }

        const double& height(int x) const {
            assert(x >= 0 && x < nb_xpos_);
            return height_[x];
        }
        
        // place() manages the insertion of a new block

        void place(CComponentBBox& rect){

            const int width_in_pas = int ( (rect.size().x / step_) + 1 );
            
            // find the best position
            int bestXPos=0;
            double bestHeight = big_double;
            double bestFreeArea = big_double;
            
            for (int x=0;x<nb_xpos_ - width_in_pas;x++) {
                
                double localHeight = max_height(x,width_in_pas,rect);
                double localFreeArea = 
                    free_area(x,width_in_pas,localHeight)
                    + localHeight * rect.size().x;
                
                // the best position criterion is the position that 
                // minimize area lost
                // area could be lost under and upper the bbox
                if (localFreeArea < bestFreeArea){
                    bestXPos = x;
                    bestHeight = localHeight;
                    bestFreeArea = localFreeArea;
                }
            }

            // be sure we have a solution
            assert(bestHeight != big_double);
            
            // place the rectangle
            rect.translate(Vec2d(bestXPos*step_,bestHeight));
            for (int i=bestXPos;i<bestXPos + width_in_pas;i++){
                height(i) =  rect.min().y + rect.max_func(i-bestXPos);
            }
        }
        
        // find the max heigth in the range  [xpos,  xpos + width]
        double max_height(int xpos, int width,CComponentBBox& rect){
            double result = 0;
            for (int i=xpos; i<xpos+width; i++) {
                result = MAX( result, height(i)-rect.min_func(i-xpos) );
            }
            return result;
        }
        
        // find the free area in the range under height_max in range
        // [xpos,  xpos + width]

        double free_area(int xpos, int width, double height_max) {
            double result =0;
            for (int i=xpos; i<xpos+width; i++) {
                result += step_ * (height_max - height(i));
            }
            return result;
        }

    private:

        int image_size_in_pixels_;
        int margin_width_in_pixels_;

        int nb_xpos_;
        double width_;
        double step_;
        double *height_;

        std::vector<CComponentBBox> data_;
    };


//_______________________________________________________________________


    CPacker::CPacker() {
        image_size_in_pixels_ = 1024;
        margin_width_in_pixels_ = 4;
    }
  

    // There where some problems with nan (Not a number),
    // this code is used to track such problems (seems to
    // be ok now)
    static bool map_component_is_ok(CComponent* comp) 
	{
        FOR_EACH_VERTEX(CComponent, comp, it) 
		{
            const Vec2d& p = (*it)->halfedge()->tex_coord();
            if(_isnan(p.x)) 
                return false;

            if(_isnan(p.y))
                return false;
        }
        return true;
    }

    void CPacker::pack_map(CTopologyGraph* surface) 
	{
        CComponentExtractor splitter;
        ComponentList components = splitter.extract_components( surface);

        for(ComponentList::iterator it = components.begin();it != components.end(); it++) 
		{
            if(!map_component_is_ok(*it)) {
                FOR_EACH_HALFEDGE(CComponent, *it, jt) 
				{
                    (*jt)->set_tex_coord(Vec2d(0.0, 0.0));
                }
            }
        }

		if (components.size() <= 0)
			return;

        pack_map_components(components);

		for( ComponentList::iterator it = components.begin();it != components.end(); it++ ) 
		{
			delete *it;
		}
		components.clear();

        normalize_tex_coords(surface);
    }
    
	void CPacker::normalize_tex_coords(CTopologyGraph* surface) 
	{
		double u_min =  big_double;
		double v_min =  big_double;
		double u_max = -big_double;
		double v_max = -big_double;

		FOR_EACH_VERTEX(CTopologyGraph, surface, it) 
		{
			double u = (*it)->halfedge()->tex_coord().x;
			double v = (*it)->halfedge()->tex_coord().y;
			u_min = MIN(u_min, u);
			v_min = MIN(v_min, v);
			u_max = MAX(u_max, u);
			v_max = MAX(v_max, v);
		}

		if((u_max - u_min > 1e-6) && (v_max - v_min > 1e-6)) 
		{
			double delta = MAX(u_max - u_min, v_max - v_min);
			FOR_EACH_VERTEX(CTopologyGraph, surface, it) 
			{
				double u = (*it)->halfedge()->tex_coord().x;
				double v = (*it)->halfedge()->tex_coord().y;
				u = (u - u_min) / delta;
				v = (v - v_min) / delta;
				(*it)->halfedge()->set_tex_coord(Vec2d(u,v));
			}
		}
	}

    void CPacker::pack_map_components(ComponentList& surfaces) 
	{
        CryLog("Packer nb components: %d",surfaces.size()); 
  
        CTopologyGraph* map = surfaces[0]->map();
        total_area_3d_ = map->map_area();
        normalize_surface_components(surfaces);

        TetrisPacker pack;  
        pack.set_image_size_in_pixels(image_size_in_pixels());
        pack.set_margin_width_in_pixels(margin_width_in_pixels());
        double area = 0;
        for( ComponentList::iterator it = surfaces.begin();it != surfaces.end(); it++ ) 
		{
            assert(map_component_is_ok(*it));
            AABB box = (*it)->bbox2d();
            double u_min = box.min.x;
            double v_min = box.min.y;
            double u_max = box.max.x;
            double v_max = box.max.y;
            
            assert(!_isnan(u_min));
            assert(!_isnan(v_min));
            assert(!_isnan(u_max));
            assert(!_isnan(v_max));
            assert(u_max >= u_min);
            assert(v_max >= v_min);
            
            area += (v_max - v_min) * (u_max - u_min);
            
            CComponentBBox r(
                *it,
                Vec2d(u_min,v_min) ,
                Vec2d(u_max,v_max)
            );
            pack.add(r);
        }
        
        
        pack.recursive_apply();

        double total_area = 0;
        for( ComponentList::iterator it = surfaces.begin();it != surfaces.end(); it++) 
		{
            total_area += abs((*it)->signed_area2d());
        }

        {
            AABB box;
            
            for(ComponentList::iterator it = surfaces.begin();it != surfaces.end(); it++ ) 
			{
                box.Add((*it)->bbox2d());
            }
            
            double bbox_area = box.GetSize().x * box.GetSize().y; 
            double filling_ratio = total_area / bbox_area;

			CryLog("Packer BBox area: %d",bbox_area);
			CryLog("Packer Filling ratio: %d",filling_ratio);
        } 
    }
    
    CHalfedge* CPacker::largest_border(CComponent* component) 
	{
        FOR_EACH_HALFEDGE(CComponent, component, it) 
		{
            is_visited_[*it] = false;
        }
        int largest_size = 0;
        CHalfedge* largest = NULL;
        FOR_EACH_HALFEDGE(CComponent, component, it) 
		{
            if((*it)->is_border() && !is_visited_[*it]) 
			{
                int cur_size = 0;
                CHalfedge* h = *it;
                do 
				{
                    is_visited_[h] = true;
                    h = h->next();
                    cur_size++;
                } 
				while(h != *it);
                if(cur_size > largest_size) 
				{
                    largest_size = cur_size;
                    largest = *it;
                }
            }
        }        
        return largest;
    }

    void CPacker::normalize_surface_components( ComponentList& surfaces) 
	{
        if(surfaces.size() == 0)
			return;

        for(ComponentList::iterator it = surfaces.begin();it != surfaces.end(); it++) 
		{
            assert(map_component_is_ok(*it));
            normalize_surface_component(*it);
            assert(map_component_is_ok(*it));
        }
    }
    
    static double extent(const std::vector<Vec2d>& P, const Vec2d& v) {
        double x_min =  1e30;
        double x_max = -1e30;
        for(unsigned int i=0; i<P.size(); i++) {
            double x = P[i].Dot(v);
            x_min = MIN(x_min, x);
            x_max = MAX(x_max, x);
        }
        return (x_max - x_min);
    }


    static Vec2d longuest_edge(const std::vector<Vec2d>& P) 
	{
        Vec2d result(0,0);
        for(unsigned int i=0; i<P.size(); i++) 
		{
            int j = (i+1)%P.size();
            Vec2d cur = P[j] - P[i];
            if(cur.GetLength2() > result.GetLength2()) 
                result = cur;
        }
        return result;
    }


    static void rectangle(const std::vector<Vec2d>& P, Vec2d& v1, Vec2d& v2) 
	{
        Vec2d up = longuest_edge(P).Normalize(); 
        CAverageDirection2d dir;
        dir.begin();
        for(unsigned int i=0; i<P.size(); i++) 
		{
            unsigned int j = (i + 1) % P.size();
            unsigned int k = (i + 2) % P.size();
            unsigned int l = (i + 3) % P.size();

            Vec2d v = P[k] - P[j];
            Vec2d v_pred = P[j] - P[i];
            Vec2d v_succ = P[l] - P[k];

            double cos_pred = cos_angle(v_pred, v);
            double cos_succ = cos_angle(v, v_succ);

            double min_cos = min(::fabs(cos_pred), ::fabs(cos_succ));
            v *= (1.0 - min_cos);

            double cos_alpha = v.Dot(up) / v.GetLength();
            double test = ::fabs(cos_alpha) - 0.5 * sqrt(2.0);
            if(test < 0.0) {
                v = Vec2d(-v.y, v.x);
            }
            if(fabs(test) < 0.005) 
			{
                v = Vec2d(0,0);
                double n = v.GetLength();
                v = up.Normalize();
                v *= 0.5 * (1.0 - min_cos) * n;
            }
            dir.add_vector(v);
        }
        dir.end();
        v1 = dir.average_direction();
		v1.Normalize();
        v2 = Vec2d(-v1.y, v1.x);
        if(extent(P, v1) > extent(P,v2)) 
		{
			Vec2d v3 = v1;
			v1 = v2;
			v2 = v3;
        }
    }

    void CPacker::normalize_surface_component(CComponent* S) 
	{       
        bool has_straight_seams = false;

        std::vector<Vec2d> PList;
        {
            CHalfedge* b = largest_border(S);
			if (!b)
				return;

            CHalfedge* h = b;
            do 
			{
                PList.push_back(h->tex_coord());
                h = h->next();
            } 
			while(h != b);
        }

        Vec2d v1;
        Vec2d v2;
        Vec2d center = PList[0];

        if(has_straight_seams) 
		{
            v1 = Vec2d(1.0,0.0);
            v2 = Vec2d(0.0,1.0);
        } 
		else 
		{
			rectangle(PList, v1, v2);
        }
        assert(!_isnan(center.x));
        assert(!_isnan(center.y));
        assert(!_isnan(v1.x));
        assert(!_isnan(v1.y));
        assert(!_isnan(v2.x));
        assert(!_isnan(v2.y));

        v1 = v1.Normalize();
        v2 = v2.Normalize();
        
        FOR_EACH_VERTEX(CComponent, S, it) 
		{
            const Vec2d& old_uv = (*it)->halfedge()->tex_coord();
            double new_u = (old_uv - center).Dot(v1);
            double new_v = (old_uv - center).Dot(v2);
            (*it)->halfedge()->set_tex_coord(Vec2d(new_u, new_v));
        }

        double area3d = S->area();
        double area2d = S->area2d();
        double factor = 1.0;
        if(fabs(area2d) > 1e-30) 
		{
            factor = sqrt(area3d/area2d);
        } 
		else 
		{
            factor = 0.0;
        }

        assert(!_isnan(area2d));
        assert(!_isnan(area3d));
        assert(!_isnan(factor));

        FOR_EACH_VERTEX(CComponent, S, it) 
		{
            const Vec2d& old_uv = (*it)->halfedge()->tex_coord();
            double new_u = old_uv.x * factor;
            double new_v = old_uv.y * factor;
            (*it)->halfedge()->set_tex_coord(Vec2d(new_u, new_v));
        }   
    }

	CAverageDirection2d::CAverageDirection2d() 
	{
		result_is_valid_ = false;
	}

	void CAverageDirection2d::begin() 
	{
		for(int i=0; i<3; i++) 
			M_[i] = 0.0;
	}

	void CAverageDirection2d::add_vector(const Vec2d& e) 
	{
		M_[0] += e.x * e.x;
		M_[1] += e.x * e.y;
		M_[2] += e.y * e.y;
	}

	void CAverageDirection2d::end() 
	{
		double eigen_vectors[4];
		double eigen_values[2];
		semi_definite_symmetric_eigen(M_, 2, eigen_vectors, eigen_values);
		int k = 0;
		result_ = Vec2d(
			eigen_vectors[2*k],
			eigen_vectors[2*k+1]
		);
		result_is_valid_ = true;
	}

}

