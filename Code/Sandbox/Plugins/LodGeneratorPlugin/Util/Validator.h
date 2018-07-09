#pragma once

namespace LODGenerator 
{
	class CComponent;
    class CValidator 
	{
    public:
        CValidator();
        ~CValidator();

        bool component_is_valid(CComponent* comp);
        double component_scaling(CComponent* comp);
        void compute_fill_and_overlap_ratio(CComponent* comp);
        double fill_ratio() const { return fill_ratio_; }
        double overlap_ratio() const { return overlap_ratio_; }

        double get_max_overlap_ratio() const { return max_overlap_ratio_; }
        void set_max_overlap_ratio(double x) { max_overlap_ratio_ = x; }
        double get_max_scaling() const { return max_scaling_; }
        void set_max_scaling(double x) { max_scaling_ = x; }
        double get_min_fill_ratio() const { return min_fill_ratio_; }
        void set_min_fill_ratio(double x) { min_fill_ratio_ = x; }

    protected:
        void begin_rasterizer(CComponent* comp);
        void end_rasterizer();

        void rasterize_triangle(
            const Vec2d& p1, const Vec2d& p2, const Vec2d& p3
        );
        void transform(const Vec2d& p, int& x, int& y);

    private:
        int graph_size_;
		uint8* graph_mem_;
        int* x_left_;
        int* x_right_;

        double user_x_min_;
        double user_y_min_;
        double user_width_;
        double user_height_;
        double user_size_;

        double fill_ratio_;
        double overlap_ratio_;

        double max_overlap_ratio_;
        double max_scaling_;
        double min_fill_ratio_;
    };

}

