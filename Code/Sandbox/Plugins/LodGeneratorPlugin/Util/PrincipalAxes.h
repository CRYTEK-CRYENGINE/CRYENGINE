#pragma once

namespace LODGenerator 
{
    class CPrincipalAxes3d 
	{
    public:
        CPrincipalAxes3d();
        void begin_points();
        void end_points();
        void point(const Vec3d& p, double weight = 1.0);
        
        Vec3d center() const;
        const Vec3d& axis(int i) const;
        double eigen_value(int i) const; 
        
        
    private:
        double center_[3];
        Vec3d axis_[3];
        double eigen_value_[3];
        
        double M_[6];
        int nb_points_;
        double sum_weights_;
    };

    class CPrincipalAxes2d 
	{
    public:
        CPrincipalAxes2d();
        void begin_points();
        void end_points();
        void point(const Vec2d& p, double weight = 1.0);
        
        Vec2d center() const;
        const Vec2d& axis(int i) const;
        double eigen_value(int i) const; 
        
        
    private:
        double center_[2];
        Vec2d axis_[2];
        double eigen_value_[2];
        
        double M_[3];
        int nb_points_;
        double sum_weights_;
    };
}


