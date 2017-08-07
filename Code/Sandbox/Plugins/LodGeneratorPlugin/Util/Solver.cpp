#include "StdAfx.h"
#include "Solver.h"
//#include <CHOLMOD/cholmod.h>

namespace LODGenerator 
{
	CSolver::CSolver() { 
	}

	CSolver::~CSolver() { 
	}        

	bool CSolver::solve(const CSparseMatrix& A_in, CVectorD& x_out, const CVectorD& b_in) {
		// To support this function, a massive amount of third party code would have to be added to the project,
		// which is not desirable. There is already one custom UV unwrapping function, which does an OK job ...
#if 0
		assert(A_in.n() == A_in.m()) ;
		assert(A_in.has_symmetric_storage()) ;

		// Step 1: initialize CHOLMOD library
		//----------------------------------------------------
		cholmod_common c ;
		cholmod_start(&c) ;

		int N = A_in.n() ;
		int NNZ = A_in.nnz() ;

		// Step 2: translate sparse matrix into cholmod format
		//---------------------------------------------------------------------------
		cholmod_sparse* A = cholmod_allocate_sparse(N, N, NNZ, false, true, -1, CHOLMOD_REAL, &c);

		int* colptr = static_cast<int*>(A->p) ;
		int* rowind = static_cast<int*>(A->i) ;
		double* a = static_cast<double*>(A->x) ;

		// Convert Graphite Matrix into CHOLMOD Matrix
		int count = 0 ;
		for(int j=0; j<N; j++) {
			const CSparseMatrix::Column& Cj = A_in.column(j) ;
			colptr[j] = count ;
			for(int ii=0; ii<Cj.nb_coeffs(); ii++) {
				a[count]    = Cj.coeff(ii).a ;
				rowind[count] = Cj.coeff(ii).index ;
				count++ ;
			}
		}
		colptr[N] = NNZ ;


		// Step 2: construct right-hand side
		cholmod_dense* b = cholmod_allocate_dense(N, 1, N, CHOLMOD_REAL, &c) ;
		memcpy(b->x, b_in.data(), N * sizeof(double)) ;

		// Step 3: factorize
		cholmod_factor* L = cholmod_analyze(A, &c) ;
		cholmod_factorize(A, L, &c) ;
		cholmod_dense* x = cholmod_solve(CHOLMOD_A, L, b, &c) ;
		memcpy(x_out.data(), x->x, N * sizeof(double)) ;

		// Step 5: cleanup
		cholmod_free_factor(&L, &c) ;
		cholmod_free_sparse(&A, &c) ;
		cholmod_free_dense(&x, &c) ;
		cholmod_free_dense(&b, &c) ;
		cholmod_finish(&c) ;
		return true;
#else
		return false;
#endif
	}

	bool CSolver::needs_rows() const {
		return false ;
	}  

	bool CSolver::needs_columns() const {
		return true ;
	} 

	bool CSolver::supports_symmetric_storage() const {
		return true ;
	} 
}
