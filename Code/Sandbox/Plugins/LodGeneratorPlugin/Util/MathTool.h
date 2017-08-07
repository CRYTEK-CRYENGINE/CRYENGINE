#pragma once

#include "SparseMatrix.h"
#include "Vector.h"

namespace LODGenerator 
{
	void semi_definite_symmetric_eigen(const double *mat, int n, double *eigen_vec, double *eigen_val);

	bool solve_super_lu(CSparseMatrix& M, const double* b, double* x,CVectorI& perm, bool clear_M = false, bool symmetric = false );              
	bool solve_super_lu(const CSparseMatrix& M, const double* b, double* x,bool perm = true, bool symmetric = false );           
	bool solve_super_lu(CSparseMatrix& M, const CVectorD& b, CVectorD& x,CVectorI& perm, bool clear_M = false, bool symmetric = false);   
	bool solve_super_lu(const CSparseMatrix& M, const CVectorD& b, CVectorD& x,bool perm = true, bool symmetric = false );

	const float big_float = 1e10f;
	const float small_float = 1e-10f;
	const double big_double = 1e20;
	const double small_double = 1e-20;

	double triangle_area(const Vec3d& p1, const Vec3d& p2, const Vec3d& p3);

	double cos_angle(const Vec3d& a, const Vec3d& b);
	double angle(const Vec3d& a, const Vec3d& b);
}