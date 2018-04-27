#pragma once

#include "SparseMatrix.h"
#include <map>

namespace LODGenerator 
{
    class CSparseMatrix ;
	class CSolver
	{
	public:
		CSolver();
		virtual ~CSolver() ;

		bool solve(const CSparseMatrix& A, CVectorD& x, const CVectorD& b) ;
		bool needs_rows() const ;                             
		bool needs_columns() const ;                       
		bool supports_symmetric_storage() const ; 
	} ;
}


