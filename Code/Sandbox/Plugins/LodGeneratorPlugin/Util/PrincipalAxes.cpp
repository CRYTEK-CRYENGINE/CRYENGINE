#include "StdAfx.h"
#include "PrincipalAxes.h"
#include "MathTool.h"

namespace LODGenerator 
{
	Vec3d CPrincipalAxes3d::center() const 
	{
		return Vec3d(center_[0], center_[1], center_[2]);
	}

	const Vec3d& CPrincipalAxes3d::axis(int i) const 
	{
		assert(i >= 0 && i < 3);
		return axis_[i];
	}

	double CPrincipalAxes3d::eigen_value(int i) const 
	{
		assert(i >= 0 && i < 3);
		return eigen_value_[i];
	}

	Vec2d CPrincipalAxes2d::center() const 
	{
		return Vec2d(center_[0], center_[1]);
	}

	const Vec2d& CPrincipalAxes2d::axis(int i) const 
	{
		assert(i >= 0 && i < 2);
		return axis_[i];
	}

	double CPrincipalAxes2d::eigen_value(int i) const 
	{
		assert(i >= 0 && i < 2);
		return eigen_value_[i];
	}
    
    CPrincipalAxes3d::CPrincipalAxes3d() 
	{
        
    }
    
    void CPrincipalAxes3d::begin_points() 
	{
        nb_points_ = 0;
        sum_weights_ = 0;
        center_[0] = center_[1] = center_[2] = 0;
        M_[0] = M_[1] = M_[2] = M_[3] = M_[4] = M_[5] = 0;
    }

    void CPrincipalAxes3d::end_points() 
	{
        center_[0] /= sum_weights_;
        center_[1] /= sum_weights_;
        center_[2] /= sum_weights_;

        // If the system is under-determined, 
        //   return the trivial basis.
        if(nb_points_ < 4) {
            axis_[0] = Vec3d(1,0,0);
            axis_[1] = Vec3d(0,1,0);
            axis_[2] = Vec3d(0,0,1);
            eigen_value_[0] = 1.0;
            eigen_value_[1] = 1.0;
            eigen_value_[2] = 1.0;
        } else {
            double x = center_[0];
            double y = center_[1];
            double z = center_[2];
            
            M_[0] = M_[0]/sum_weights_ - x*x;
            M_[1] = M_[1]/sum_weights_ - x*y;
            M_[2] = M_[2]/sum_weights_ - y*y;
            M_[3] = M_[3]/sum_weights_ - x*z;
            M_[4] = M_[4]/sum_weights_ - y*z;
            M_[5] = M_[5]/sum_weights_ - z*z;
            
            if( M_[0] <= 0 ) {
                M_[0] = 1.e-30; 
            }
            if( M_[2] <= 0 ) {
                M_[2] = 1.e-30; 
            }
            if( M_[5] <= 0 ) {
                M_[5] = 1.e-30; 
            }
            
            double eigen_vectors[9];
            semi_definite_symmetric_eigen(M_, 3, eigen_vectors, eigen_value_);
            
            axis_[0] = Vec3d(
                eigen_vectors[0], eigen_vectors[1], eigen_vectors[2]
            );
            
            axis_[1] = Vec3d(
                eigen_vectors[3], eigen_vectors[4], eigen_vectors[5]
            );
        
            axis_[2] = Vec3d(
                eigen_vectors[6], eigen_vectors[7], eigen_vectors[8]
            );
        
            // Normalize the eigen vectors
            
            for(int i=0; i<3; i++) {
                axis_[i] = axis_[i].normalize();
            }
        }

    }

    void CPrincipalAxes3d::point(const Vec3d& p, double weight) 
	{
        center_[0] += p.x * weight;
        center_[1] += p.y * weight;
        center_[2] += p.z * weight;
    
        double x = p.x;
        double y = p.y; 
        double z = p.z;
        
        M_[0] += weight * x*x;
        M_[1] += weight * x*y;
        M_[2] += weight * y*y;
        M_[3] += weight * x*z;
        M_[4] += weight * y*z;
        M_[5] += weight * z*z;
        
        nb_points_++;
        sum_weights_ += weight;
    }
    
//_________________________________________________________

    CPrincipalAxes2d::CPrincipalAxes2d() 
	{
        
    }
    
    void CPrincipalAxes2d::begin_points() 
	{
        nb_points_ = 0;
        sum_weights_ = 0;
        center_[0] = center_[1] = 0;
        M_[0] = M_[1] = M_[2] = 0;
    }

    void CPrincipalAxes2d::end_points() 
	{

        center_[0] /= sum_weights_;
        center_[1] /= sum_weights_;
        
        // If the system is under-determined, 
        //  return the trivial basis.
        if(nb_points_ < 3) {
            axis_[0] = Vec2d(1,0);
            axis_[1] = Vec2d(0,1);
            eigen_value_[0] = 1.0;
            eigen_value_[1] = 1.0;
        } else {
            double x = center_[0];
            double y = center_[1];
        
            M_[0] = M_[0]/sum_weights_ - x*x;
            M_[1] = M_[1]/sum_weights_ - x*y;
            M_[2] = M_[2]/sum_weights_ - y*y;
            
            if( M_[0] <= 0 ) {
                M_[0] = 1.e-30; 
            }
            
            if( M_[2] <= 0 ) {
                M_[2] = 1.e-30; 
            }
            
            double eigen_vectors[4];
            semi_definite_symmetric_eigen(M_, 2, eigen_vectors, eigen_value_);
            
            axis_[0] = Vec2d(eigen_vectors[0], eigen_vectors[1]);
            axis_[1] = Vec2d(eigen_vectors[2], eigen_vectors[3]);
        
            // Normalize the eigen vectors
            
            for(int i=0; i<2; i++) 
			{
                axis_[i] = axis_[i].Normalize();
            }
        }

    }

    void CPrincipalAxes2d::point(const Vec2d& p, double weight) 
	{

        double x = p.x;
        double y = p.y; 

        center_[0] += x * weight;
        center_[1] += y * weight;
            
        M_[0] += weight * x*x;
        M_[1] += weight * x*y;
        M_[2] += weight * y*y;
        
        nb_points_++;
        sum_weights_ += weight;
    }


}

