#pragma once

namespace LODGenerator 
{
	class CVertex;
	class CSplitter;
	class CABF;
	class CComponent;
	class CTopologyGraph;
    class CUVGenerator 
	{
    public :
        CUVGenerator(CTopologyGraph* map);
		~CUVGenerator();
        void apply();

        double get_max_overlap_ratio() const { return max_overlap_ratio_; }
        void set_max_overlap_ratio(double x) { max_overlap_ratio_ = x; }
        double get_max_scaling() const { return max_scaling_; }
        void set_max_scaling(double x) { max_scaling_ = x; }
        double get_min_fill_ratio() const { return min_fill_ratio_; }
        void set_min_fill_ratio(double x) { min_fill_ratio_ = x; }

        bool get_auto_cut() const { return auto_cut_; }
        void set_auto_cut(bool x) { auto_cut_ = x; }
        
        bool get_auto_cut_cylinders() const { return auto_cut_cylinders_; }
        void set_auto_cut_cylinders(bool x) { auto_cut_cylinders_ = x; }

    protected:

        void get_components();
        struct SActiveComponent 
		{
            SActiveComponent(int i = 0) : iter(i) { }
            int iter;
            CComponent* component;
        };

        void activate_component(CComponent* comp, int iter = 0);
        void split_component(CComponent* comp, int iter = 0);
        void split_thin_component(CComponent* comp, int iter = 0);

        CTopologyGraph* map_;
        CABF* parameterizer_;

        std::deque<SActiveComponent> active_components_;

        int total_faces_;
        int parameterized_faces_;
     
        double max_overlap_ratio_;
        double max_scaling_;
        double min_fill_ratio_;
 
        bool auto_cut_;
        bool auto_cut_cylinders_;

        CSplitter* splitter_;
    };
}
