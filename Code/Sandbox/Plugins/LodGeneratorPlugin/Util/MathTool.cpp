#include "StdAfx.h"
#include "MathTool.h"

//#include <SUPERLU/dsp_defs.h>
//#include <SUPERLU/util.h>

namespace LODGenerator 
{
	static const double EPS = 0.00001;
	static int MAX_ITER = 100;

	void semi_definite_symmetric_eigen(const double *mat, int n, double *eigen_vec, double *eigen_val) 
	{
		double *a,*v;
		double a_norm,a_normEPS,thr,thr_nn;
		int nb_iter = 0;
		int jj;
		int i,j,k,ij,ik,l,m,lm,mq,lq,ll,mm,imv,im,iq,ilv,il,nn;
		int *index;
		double a_ij,a_lm,a_ll,a_mm,a_im,a_il;
		double a_lm_2;
		double v_ilv,v_imv;
		double x;
		double sinx,sinx_2,cosx,cosx_2,sincos;
		double delta;

		// Number of entries in mat

		nn = (n*(n+1))/2;

		// Step 1: Copy mat to a

		a = new double[nn];

		for( ij=0; ij<nn; ij++ ) {
			a[ij] = mat[ij];
		}

		// Ugly Fortran-porting trick: indices for a are between 1 and n
		a--;

		// Step 2 : Init diagonalization matrix as the unit matrix
		v = new double[n*n];

		ij = 0;
		for( i=0; i<n; i++ ) {
			for( j=0; j<n; j++ ) {
				if( i==j ) {
					v[ij++] = 1.0;
				} else {
					v[ij++] = 0.0;
				}
			}
		}

		// Ugly Fortran-porting trick: indices for v are between 1 and n
		v--;

		// Step 3 : compute the weight of the non diagonal terms 
		ij     = 1 ;
		a_norm = 0.0;
		for( i=1; i<=n; i++ ) {
			for( j=1; j<=i; j++ ) {
				if( i!=j ) {
					a_ij    = a[ij];
					a_norm += a_ij*a_ij;
				}
				ij++;
			}
		}

		if( a_norm != 0.0 ) {

			a_normEPS = a_norm*EPS;
			thr       = a_norm   ;

			// Step 4 : rotations
			while( thr > a_normEPS   &&   nb_iter < MAX_ITER ) {

				nb_iter++;
				thr_nn = thr / nn;

				for( l=1 ; l< n; l++ ) {
					for( m=l+1; m<=n; m++ ) {

						// compute sinx and cosx 

						lq   = (l*l-l)/2;
						mq   = (m*m-m)/2;

						lm     = l+mq;
						a_lm   = a[lm];
						a_lm_2 = a_lm*a_lm;

						if( a_lm_2 < thr_nn ) {
							continue;
						}

						ll   = l+lq;
						mm   = m+mq;
						a_ll = a[ll];
						a_mm = a[mm];

						delta = a_ll - a_mm;

						if( delta == 0.0 ) {
							x = - g_PI/4;
						} else {
							x = - atan( (a_lm+a_lm) / delta ) / 2.0;
						}

						sinx    = sin(x)  ;
						cosx    = cos(x)  ;
						sinx_2  = sinx*sinx;
						cosx_2  = cosx*cosx;
						sincos  = sinx*cosx;

						// rotate L and M columns 

						ilv = n*(l-1);
						imv = n*(m-1);

						for( i=1; i<=n;i++ ) {
							if( (i!=l) && (i!=m) ) {
								iq = (i*i-i)/2;

								if( i<m )  { 
									im = i + mq; 
								} else {
									im = m + iq;
								}
								a_im = a[im];

								if( i<l ) {
									il = i + lq; 
								} else {
									il = l + iq;
								}
								a_il = a[il];

								a[il] =  a_il*cosx - a_im*sinx;
								a[im] =  a_il*sinx + a_im*cosx;
							}

							ilv++;
							imv++;

							v_ilv = v[ilv];
							v_imv = v[imv];

							v[ilv] = cosx*v_ilv - sinx*v_imv;
							v[imv] = sinx*v_ilv + cosx*v_imv;
						} 

						x = a_lm*sincos; x+=x;

						a[ll] =  a_ll*cosx_2 + a_mm*sinx_2 - x;
						a[mm] =  a_ll*sinx_2 + a_mm*cosx_2 + x;
						a[lm] =  0.0;

						thr = fabs( thr - a_lm_2 );
					}
				}
			}         
		}

		// Step 5: index conversion and copy eigen values 

		// back from Fortran to C++
		a++;

		for( i=0; i<n; i++ ) {
			k = i + (i*(i+1))/2;
			eigen_val[i] = a[k];
		}

		delete[] a;

		// Step 6: sort the eigen values and eigen vectors 

		index = new int[n];
		for( i=0; i<n; i++ ) {
			index[i] = i;
		}

		for( i=0; i<(n-1); i++ ) {
			x = eigen_val[i];
			k = i;

			for( j=i+1; j<n; j++ ) {
				if( x < eigen_val[j] ) {
					k = j;
					x = eigen_val[j];
				}
			}

			eigen_val[k] = eigen_val[i];
			eigen_val[i] = x;

			jj       = index[k];
			index[k] = index[i];
			index[i] = jj;
		}


		// Step 7: save the eigen vectors 

		v++; // back from Fortran to to C++

		ij = 0;
		for( k=0; k<n; k++ ) {
			ik = index[k]*n;
			for( i=0; i<n; i++ ) {
				eigen_vec[ij++] = v[ik++];
			}
		}

		delete[] v   ;
		delete[] index;
		return;
	}

	bool solve_super_lu(const CSparseMatrix& M, const double* b, double* x, bool do_perm, bool symmetric) 
	{
		CVectorI perm;
		if(!do_perm) {
			perm.allocate(M.n());
			for(int i=0; i<M.n(); i++) {
				perm(i) = i;
			}
		}
		return solve_super_lu(const_cast<CSparseMatrix&>(M),b,x,perm, false, symmetric);
	}
	bool solve_super_lu(CSparseMatrix& M, const double* b, double* x, CVectorI& perm, bool clear_M, bool symmetric) 
	{
		// To support this function, a massive amount of third party code would have to be added to the project,
		// which is not desirable. There is already one custom UV unwrapping function, which does an OK job ...
#if 0
		assert(!M.has_symmetric_storage());
		assert(M.rows_are_stored());
		assert(M.m() == M.n());
		int n = M.n();

		int nnz = 0;
		for(int i=0; i<n; i++) {
			nnz += M.row(i).nb_coeffs();
		}

		// Allocate storage for sparse matrix and permutation
		int*     xa     = new int[n+1];
		double*  rhs    = new double[n];
		double*  a      = new double[nnz];
		int*     asub   = new int[nnz];
		int*     perm_r = new int[n];

		// Step 1: convert matrix M into SuperLU compressed column 
		//   representation.
		// -------------------------------------------------------
		{
			int count = 0;
			for(int i=0; i<n; i++) 
			{
				const CSparseMatrix::Row& r = M.row(i);
				xa[i] = count;
				for(int jj=0; jj<r.nb_coeffs(); jj++) {
					a[count]    = r.coeff(jj).a;
					asub[count] = r.coeff(jj).index; 
					count++;
				}
			}
			xa[n] = nnz;
		}

		// Save memory for SuperLU
		if(clear_M) 
		{
			M.clear();
		}

		// Rem: symmetric storage does not seem to work with
		//  SuperLU ... (->deactivated in main LinearSolver driver)
		SuperMatrix A;
		dCreate_CompCol_Matrix(
			&A, n, n, nnz, a, asub, xa, 
			SLU_NR,                  // Row_wise, no supernode
			SLU_D,                   // doubles
			SLU_GE                   // general storage
			);

		// Step 2: create vector
		// ---------------------
		SuperMatrix B;
		dCreate_Dense_Matrix(
			&B, n, 1, const_cast<double*>(b), n, 
			SLU_DN, // Fortran-type column-wise storage 
			SLU_D,  // doubles
			SLU_GE  // general
			);


		// Step 3: get permutation matrix 
		// ------------------------------
		// com_perm: NATURAL       -> no re-ordering
		//           MMD_ATA       -> re-ordering for A^t.A
		//           MMD_AT_PLUS_A -> re-ordering for A^t+A
		//           COLAMD        -> approximate minimum degree ordering
		if(perm.size() == 0) {
			perm.allocate(n);
			if(symmetric) {
				get_perm_c(MMD_AT_PLUS_A, &A, perm.data());
			} else {
				get_perm_c(COLAMD, &A, perm.data());
			}
		}

		// Step 4: call SuperLU main routine
		// ---------------------------------
		superlu_options_t options;
		SuperLUStat_t     stat;
		SuperMatrix L;
		SuperMatrix U;
		int info;

		set_default_options(&options);
		options.ColPerm = MY_PERMC;
		if(symmetric) {
			options.SymmetricMode = YES;
		}
		StatInit(&stat);

		dgssv(&options, &A, perm.data(), perm_r, &L, &U, &B, &stat, &info);

		// Step 5: get the solution
		// ------------------------
		// Fortran-type column-wise storage
		DNformat *vals = (DNformat*)B.Store;
		double *rvals = (double*)(vals->nzval);
		if(info == 0) {
			for(int i = 0; i <  n; i++){
				x[i] = rvals[i];
			}
		} 
		else 
		{
			CryLog("Solver SuperLU failed with: %d",info);
			CryLog("Solver size=%d non zero= %d",n,nnz);
		}

		//  For these two ones, only the "store" structure
		// needs to be deallocated (the arrays have been allocated
		// by us).
		Destroy_SuperMatrix_Store(&A);
		Destroy_SuperMatrix_Store(&B);

		//   These ones need to be fully deallocated (they have been
		// allocated by SuperLU).
		Destroy_SuperNode_Matrix(&L);
		Destroy_CompCol_Matrix(&U);

		// There are some dynamically allocated arrays in stat
		StatFree(&stat);

		delete[] xa    ;
		delete[] rhs   ;
		delete[] a     ;
		delete[] asub  ;
		delete[] perm_r;

		return (info == 0);
#else
		return false;
#endif
	}

	bool solve_super_lu(CSparseMatrix& M, const CVectorD& b, CVectorD& x,CVectorI& perm, bool clear_M, bool symmetric) 
	{
		return solve_super_lu(M, b.data(), x.data(), perm, clear_M, symmetric);
	}        
	bool solve_super_lu(const CSparseMatrix& M, const CVectorD& b, CVectorD& x,bool perm, bool symmetric) 
	{
		return solve_super_lu(M, b.data(), x.data(), perm, symmetric);
	}

	double triangle_area(const Vec3d& p1, const Vec3d& p2, const Vec3d& p3) 
	{
		return 0.5 * ((p2 - p1).Cross(p3 - p1)).len() ;
	}

	double cos_angle(const Vec3d& a, const Vec3d& b) 
	{
		double na2 = a.len2();
		double nb2 = b.len2();
		return a.Dot(b) / ::sqrt(na2 * nb2);
	}

	double angle(const Vec3d& a, const Vec3d& b) 
	{
		return acos(cos_angle(a,b));
	}
}