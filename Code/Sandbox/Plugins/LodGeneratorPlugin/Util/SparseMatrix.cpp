#include "StdAfx.h"
#include "SparseMatrix.h"

namespace LODGenerator {


    void CSparseLine::add(int index, double val) {
        CCoeff* coeff = NULL;

        for(int ii=0; ii < nb_coeffs_; ii++) {
            if(coeff_[ii].index == index) {
                coeff = &(coeff_[ii]);
                break;
            }
        }
        if(coeff != NULL) {
            coeff->a += val;
        } else {
            nb_coeffs_++;
            if(nb_coeffs_ > capacity_) {
                grow();
            }
            coeff = &(coeff_[nb_coeffs_ - 1]);
            coeff->a     = val;
            coeff->index = index;
        }
    }

    void CSparseLine::set(int index, double val) {
        CCoeff* coeff = NULL;

        for(int ii=0; ii < nb_coeffs_; ii++) {
            if(coeff_[ii].index == index) {
                coeff = &(coeff_[ii]);
                break;
            }
        }
        if(coeff != NULL) {
            coeff->a = val;
        } else {
            nb_coeffs_++;
            if(nb_coeffs_ > capacity_) {
                grow();
            }
            coeff = &(coeff_[nb_coeffs_ - 1]);
            coeff->a     = val;
            coeff->index = index;
        }
    }

	int CSparseLine::find(int i)
	{		

        for(int ii=0; ii < nb_coeffs_; ii++) {
            if(coeff_[ii].index == i) {
                return ii;                
            }
        }
		return -1;
	}

	int CSparseLine::findSorted(int i)
	{		
		if(i<coeff_[0].index)
			return -1;
		if(i>coeff_[nb_coeffs_-1].index)
			return -1;

		int left=0, right=nb_coeffs_-1;
		if(coeff_[left].index == i)
			return left;
		if(coeff_[right].index == i)
			return right;
		int middle = (left+right)/2;

		while (left!=middle)
		{			
			if(coeff_[middle].index == i)
				return middle;
			if(coeff_[middle].index < i)
				left = middle;
			else
				right = middle;
			middle = (left+right)/2;
		}
		return -1;
	}

	void CSparseLine::push_element(int index, double val)
	{
		nb_coeffs_++;
		if(nb_coeffs_ > capacity_) {
			grow();
		}
		CCoeff* coeff = &(coeff_[nb_coeffs_ - 1]);
		coeff->a     = val;
		coeff->index = index;
	}

	void CSparseLine::reserve(int nb_elements)
	{
		if (nb_elements<capacity_) return;

		int old_capacity = capacity_;
		capacity_ = nb_elements;
        CAllocator<CCoeff>::reallocate(
            coeff_, old_capacity, capacity_
        );
	}

	void CSparseLine::erase_element(int index)
	{
		if (index!=nb_coeffs_-1)
			memmove(&coeff_[index], &coeff_[index+1], (nb_coeffs_-index-1)*sizeof(coeff_[0]));		
		nb_coeffs_--;
	}

    double CSparseLine::get(int index) const {
        CCoeff* coeff = NULL;
        // Search for a_{index}
        for(int ii=0; ii < nb_coeffs_; ii++) {
            if(coeff_[ii].index == index) {
                coeff = &(coeff_[ii]);
                break;
            }
        }
        if(coeff != NULL) {
            return coeff->a ;
        } else {
            return 0;
        }
    }
    
    void CSparseLine::grow() {
        int old_capacity = capacity_;
		if (capacity_>1000) {
			capacity_ = (int)(capacity_ * 1.2); 
		} else {
			capacity_ = capacity_ * 2;
		}
        CAllocator<CCoeff>::reallocate(
            coeff_, old_capacity, capacity_
        );
    }

    class CCoeffIndexCompare {
    public:
        bool operator()(const CCoeff& c1, const CCoeff& c2) {
            return c1.index < c2.index;
        }
    };

    void CSparseLine::sort() {
        CCoeff* begin = coeff_;
        CCoeff* end   = coeff_ + nb_coeffs_;
        std::sort(begin, end, CCoeffIndexCompare());
    }

    void CSparseLine::removeDuplicates() { 

		if (nb_coeffs_==0) return;

		int nbDifferentCCoeffs = 1;
		for (int i=1; i<nb_coeffs_; i++)
			if (coeff_[i-1].index!=coeff_[i].index)
				nbDifferentCCoeffs++;					

		CCoeff* newCCoeffs = CAllocator<CCoeff>::allocate(nbDifferentCCoeffs);		
		newCCoeffs[0] = coeff_[0];
		int k=1;
		for (int i=1; i<nb_coeffs_; i++)
		{
			if (coeff_[i-1].index!=coeff_[i].index)
			{
				newCCoeffs[k] = coeff_[i];
				k++;
			}
			else
				newCCoeffs[k-1].a = min(coeff_[i].a, newCCoeffs[k-1].a);
		}       
		CAllocator<CCoeff>::deallocate(coeff_);
		coeff_ = newCCoeffs;
		nb_coeffs_ = nbDifferentCCoeffs;
		capacity_ = nbDifferentCCoeffs;
    }

    CSparseMatrix::CSparseMatrix(int m, int n, EStorage storage) {
        storage_ = eS_NONE;
        allocate(m,n,storage,false);
    }        

    CSparseMatrix::CSparseMatrix(
        int n, EStorage storage, bool symmetric_storage
    ) {
        storage_ = eS_NONE;
        allocate(n,n,storage,symmetric_storage);
    }        

	CSparseMatrix::CSparseMatrix(const CSparseMatrix& rhs)
	{
		storage_ = eS_NONE;
		allocate(rhs.m(), rhs.n(), rhs.storage(), rhs.is_symmetric());
		memcpy(diag_, rhs.diag_, diag_size_*sizeof(diag_[0]));

		if (rows_are_stored_)
		{
			for (int i=0; i<m_; i++)
			{
				row_[i].coeff_ = new CCoeff[std::max(2,rhs.row_[i].nb_coeffs())];
				memcpy(row_[i].coeff_, rhs.row_[i].coeff_, rhs.row_[i].nb_coeffs()*sizeof(row_[i].coeff_[0]));
				row_[i].nb_coeffs_ = rhs.row_[i].nb_coeffs();
				row_[i].capacity_ = std::max(2, rhs.row_[i].nb_coeffs());					
			}
		}
		if (columns_are_stored_)
		{
			for (int i=0; i<n_; i++)
			{
				column_[i].coeff_ = new CCoeff[std::max(2,rhs.column_[i].nb_coeffs())];
				memcpy(column_[i].coeff_, rhs.column_[i].coeff_, rhs.column_[i].nb_coeffs()*sizeof(column_[i].coeff_[0]));
				column_[i].nb_coeffs_ = rhs.column_[i].nb_coeffs();
				column_[i].capacity_ = std::max(2, rhs.column_[i].nb_coeffs());					
			}
		}						
	}

	CSparseMatrix& CSparseMatrix::operator=(const CSparseMatrix& rhs) 
	{
		storage_ = eS_NONE;
		allocate(rhs.m(), rhs.n(), rhs.storage(), rhs.is_symmetric());
		memcpy(diag_, rhs.diag_, diag_size_*sizeof(diag_[0]));

		if (rows_are_stored_)
		{
			for (int i=0; i<m_; i++)
			{
				row_[i].coeff_ = new CCoeff[std::max(2,rhs.row_[i].nb_coeffs())];
				memcpy(row_[i].coeff_, rhs.row_[i].coeff_, rhs.row_[i].nb_coeffs()*sizeof(row_[i].coeff_[0]));
				row_[i].nb_coeffs_ = rhs.row_[i].nb_coeffs();
				row_[i].capacity_ = std::max(2, rhs.row_[i].nb_coeffs());					
			}
		}
		if (columns_are_stored_)
		{
			for (int i=0; i<n_; i++)
			{
				column_[i].coeff_ = new CCoeff[std::max(2,rhs.column_[i].nb_coeffs())];
				memcpy(column_[i].coeff_, rhs.column_[i].coeff_, rhs.column_[i].nb_coeffs()*sizeof(column_[i].coeff_[0]));
				column_[i].nb_coeffs_ = rhs.column_[i].nb_coeffs();
				column_[i].capacity_ = std::max(2, rhs.column_[i].nb_coeffs());					
			}
		}	
		return *this;
	}

    CSparseMatrix::~CSparseMatrix() {
        deallocate();
    }

    CSparseMatrix::CSparseMatrix() {
        m_ = 0;
        n_ = 0;
        diag_size_ = 0;

        row_ = NULL;
        column_ = NULL;
        diag_ = NULL;

        storage_ = eS_NONE;
        rows_are_stored_ = false;
        columns_are_stored_ = false;
        symmetric_storage_ = false;
        symmetric_tag_ = false;
    }

    int CSparseMatrix::nnz() const {
        int result = 0;
        if(rows_are_stored()) {
            for(int i=0; i<m(); i++) {
                result += row(i).nb_coeffs();
            }
        } else if(columns_are_stored()) {
            for(int j=0; j<n(); j++) {
                result += column(j).nb_coeffs();
            }
        } else {
            assert(false);
        }
        return result;
    }
        
    void CSparseMatrix::deallocate() {
        m_ = 0;
        n_ = 0;
        diag_size_ = 0;

        delete[] row_;
        delete[] column_;
        delete[] diag_;
        row_ = NULL;
        column_ = NULL;
        diag_ = NULL;

        storage_ = eS_NONE;
        rows_are_stored_ = false;
        columns_are_stored_ = false;
        symmetric_storage_ = false;
    }

    void CSparseMatrix::allocate(
        int m, int n, EStorage storage, bool symmetric_storage
    ) {
        if(storage_ != eS_NONE) {
            deallocate();
        }

        m_ = m;
        n_ = n;
        diag_size_ = min(m,n);
        symmetric_storage_ = symmetric_storage;
        symmetric_tag_ = false;
        storage_ = storage;
        switch(storage) {
        case eS_NONE:
            assert(false);
            break;
        case eS_ROWS:
            rows_are_stored_    = true;
            columns_are_stored_ = false;
            break;
        case eS_COLUMNS:
            rows_are_stored_    = false;
            columns_are_stored_ = true;
            break;
        case eS_ROWS_AND_COLUMNS:
            rows_are_stored_    = true;
            columns_are_stored_ = true;
            break;
        }
        diag_ = new double[diag_size_];
        for(int i=0; i<diag_size_; i++) {
            diag_[i] = 0.0;
        }

        if(rows_are_stored_) {
            row_ = new Row[m];
        } else {
            row_ = NULL;
        }

        if(columns_are_stored_) {
            column_ = new Column[n];
        } else {
            column_ = NULL;
        }
    }

    void CSparseMatrix::sort() {
        if(rows_are_stored_) {
            for(int i=0; i<m_; i++) {
                row(i).sort();
            }
        }
        if(columns_are_stored_) {
            for(int j=0; j<n_; j++) {
                column(j).sort();
            }
        }
    }

    void CSparseMatrix::zero() {
        if(rows_are_stored_) {
            for(int i=0; i<m_; i++) {
                row(i).zero();
            }
        }
        if(columns_are_stored_) {
            for(int j=0; j<n_; j++) {
                column(j).zero();
            }
        }
        for(int i=0; i<diag_size_; i++) {
            diag_[i] = 0.0;
        }
    }

    void CSparseMatrix::clear() {
        if(rows_are_stored_) {
            for(int i=0; i<m_; i++) {
                row(i).clear();
            }
        }
        if(columns_are_stored_) {
            for(int j=0; j<n_; j++) {
                column(j).clear();
            }
        }
        for(int i=0; i<diag_size_; i++) {
            diag_[i] = 0.0;
        }
    }

	static void mult_rows_symmetric(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        int m = A.m();
        for(int i=0; i<m; i++) {
            y[i] = 0;
            const CSparseMatrix::Row& Ri = A.row(i);
            for(int ij=0; ij<Ri.nb_coeffs(); ij++) {
                const CCoeff& c = Ri.coeff(ij);
                y[i]       += c.a * x[c.index];
                if(i != c.index) {
                    y[c.index] += c.a * x[i];
                }
            }
        }
    }

    static void mult_rows(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        int m = A.m();
        for(int i=0; i<m; i++) {
            y[i] = 0;
            const CSparseMatrix::Row& Ri = A.row(i);
            for(int ij=0; ij<Ri.nb_coeffs(); ij++) {
                const CCoeff& c = Ri.coeff(ij);
                y[i] += c.a * x[c.index];
            }
        }
    }


    static void mult_cols_symmetric(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        ::memset(y, 0, A.m() * sizeof(double));
        int n = A.n();
        for(int j=0; j<n; j++) {
            const CSparseMatrix::Column& Cj = A.column(j);
            for(int ii=0; ii<Cj.nb_coeffs(); ii++) {
                const CCoeff& c = Cj.coeff(ii);
                y[c.index] += c.a * x[j]      ;
                if(j != c.index) {
                    y[j]       += c.a * x[c.index];
                }
            }
        }
    }

    static void mult_cols(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        ::memset(y, 0, A.m() * sizeof(double));
        int n = A.n();
        for(int j=0; j<n; j++) {
            const CSparseMatrix::Column& Cj = A.column(j);
            for(int ii=0; ii<Cj.nb_coeffs(); ii++) {
                const CCoeff& c = Cj.coeff(ii);
                y[c.index] += c.a * x[j];
            }
        }
    }

    void mult(const CSparseMatrix& A, const double* x, double* y) {
        if(A.rows_are_stored()) {
            if(A.has_symmetric_storage()) {
                mult_rows_symmetric(A, x, y);
            } else {
                mult_rows(A, x, y);
            }
        } else {
            if(A.has_symmetric_storage()) {
                mult_cols_symmetric(A, x, y);
            } else {
                mult_cols(A, x, y);
            }
        }
    }

    static void mult_xpose_rows(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        ::memset(y, 0, A.n() * sizeof(double));
        int m = A.m();
        for(int i=0; i<m; i++) {
            const CSparseMatrix::Row& Ri = A.row(i);
            for(int ij=0; ij<Ri.nb_coeffs(); ij++) {
                const CCoeff& c = Ri.coeff(ij);
                y[c.index] += c.a * x[i];
            }
        }
    }

    static void mult_xpose_cols(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        int n = A.n();
        for(int j=0; j<n; j++) {
            y[j] = 0.0;
            const CSparseMatrix::Column& Cj = A.column(j);
            for(int ii=0; ii<Cj.nb_coeffs(); ii++) {
                const CCoeff& c = Cj.coeff(ii);
                y[j] += c.a * x[c.index];
            }
        }
    }

    void mult_transpose(
        const CSparseMatrix& A, const double* x, double* y
    ) {
        if(A.rows_are_stored()) {
            if(A.has_symmetric_storage()) {
                mult_rows_symmetric(A, x, y);
            } else {
                mult_xpose_rows(A, x, y);
            }
        } else {
            if(A.has_symmetric_storage()) {
                mult_cols_symmetric(A, x, y);
            } else {
                mult_xpose_cols(A, x, y);
            }
        }
    }

}



