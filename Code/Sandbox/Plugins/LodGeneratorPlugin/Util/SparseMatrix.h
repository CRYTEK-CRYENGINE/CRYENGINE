#pragma once

#include "assert.h"
#include "Allocator.h"
#include "Vector.h"
namespace LODGenerator 
{
    class  CCoeff 
	{
    public:
        double a;
        int index;
    };

    class  CSparseLine 
	{
		friend class CSparseMatrix;
    public:
        CSparseLine() {
            coeff_ = CAllocator<CCoeff>::allocate(2);
            nb_coeffs_ = 0;
            capacity_ = 2;
        }

        ~CSparseLine() {
            CAllocator<CCoeff>::deallocate(coeff_);
        }

        int nb_coeffs() const { return nb_coeffs_; }

        CCoeff& coeff(int ii) { 
            assert(ii >= 0 && ii < nb_coeffs_);
            return coeff_[ii];
        }
            
        const CCoeff& coeff(int ii) const {
			assert(ii >= 0 && ii < nb_coeffs_);
            return coeff_[ii];
        }        

        void add(int index, double val);

        void set(int index, double val);

        double get(int index) const;

		void push_element(int index, double val); 

		void reserve(int nb_elements);

		int find(int i);

		int findSorted(int i);

		void erase_element(int index);
		
        void sort();

		void removeDuplicates();

        void clear() { 
            CAllocator<CCoeff>::deallocate(coeff_);
            coeff_ = CAllocator<CCoeff>::allocate(2);
            nb_coeffs_ = 0;
            capacity_ = 2;
        }

        void zero() { nb_coeffs_ = 0; }

		int nb_coeffs_; 
    protected:
        void grow();
            
    private:
        CCoeff* coeff_;        
        int capacity_;
    };
        

    class  CSparseMatrix 
	{
    public:

        typedef CSparseLine Row;
        typedef CSparseLine Column;

        enum EStorage { eS_NONE, eS_ROWS, eS_COLUMNS, eS_ROWS_AND_COLUMNS};

        CSparseMatrix(int m, int n, EStorage storage = eS_ROWS);
        CSparseMatrix(int n, EStorage storage, bool symmetric_storage);
		CSparseMatrix(const CSparseMatrix& rhs);
		CSparseMatrix& operator=(const CSparseMatrix& rhs);		        
        CSparseMatrix();

        ~CSparseMatrix();

        int m() const {
            return m_;
        }

        int n() const {
            return n_;
        }

        int diag_size() const {
            return diag_size_;
        }

        int nnz() const;

        bool rows_are_stored() const {
            return rows_are_stored_;
        }

        bool columns_are_stored() const {
            return columns_are_stored_;
        }

        EStorage storage() const {
            return storage_;
        }

        bool has_symmetric_storage() const {
            return symmetric_storage_;
        }

        bool is_square() const {
            return (m_ == n_);
        }

        bool is_symmetric() const {
            return (symmetric_storage_ || symmetric_tag_);
        }

        void set_symmetric_tag(bool x) {
            symmetric_tag_ = x;
        }

        Row& row(int i) {
            assert(i >= 0 && i < m_);
            assert(rows_are_stored_);
            return row_[i];
        }

        const Row& row(int i) const {
            assert(i >= 0 && i < m_);
            assert(rows_are_stored_);
            return row_[i];
        }

        Column& column(int j) {
            assert(j >= 0 && j < n_);
            assert(columns_are_stored_);
            return column_[j];
        }

        const Column& column(int j) const {
            assert(j >= 0 && j < n_);
            assert(columns_are_stored_);
            return column_[j];
        }
        
        double diag(int i) const {
            assert(i >= 0 && i < diag_size_);
            return diag_[i];
        }

        void low_level_set_diag(int i, double val) {
            assert(i >= 0 && i < diag_size_);
            diag_[i] = val;
        }

        void add(int i, int j, double val) {
            assert(i >= 0 && i < m_);
            assert(j >= 0 && j < n_);
            if(symmetric_storage_ && j > i) {
                return;
            }
            if(i == j) {
                diag_[i] += val;
            } 
            if(rows_are_stored_) {
                row(i).add(j, val);
            }
            if(columns_are_stored_) {
                column(j).add(i, val);
            }
        }

        void set(int i, int j, double val) {
            assert(i >= 0 && i < m_);
            assert(j >= 0 && j < n_);
            if(symmetric_storage_ && j > i) {
                return;
            }
            if(i == j) {
                diag_[i] = val;
            } 
            if(rows_are_stored_) {
                row(i).set(j, val);
            }
            if(columns_are_stored_) {
                column(j).set(i, val);
            }
        }

		void push_element(int i, int j, double val)
		{
            assert(i >= 0 && i < m_);
            assert(j >= 0 && j < n_);
            if(symmetric_storage_ && j > i) {
                return;
            }
            if(i == j) {
                diag_[i] = val;
            } 
            if(rows_are_stored_) {
                row(i).push_element(j, val);
            }
            if(columns_are_stored_) {
                column(j).push_element(i, val);
            }
		}

		void reserve(int i, int nb_elements)
		{
            if(rows_are_stored_) {
                row(i).reserve(nb_elements);
            }
            if(columns_are_stored_) {
                column(i).reserve(nb_elements);
            }
		}
		double get(int i, int j) const
		{
			assert(i >= 0 && i < m_);
            assert(j >= 0 && j < n_);
            if(symmetric_storage_ && j > i) {
				std::swap(i,j);
            }
            if(i == j) {
                return diag_[i];
            } 
            if(rows_are_stored_) {
                return row(i).get(j);
            }
            if(columns_are_stored_) {
                return column(j).get(i);
            }
			return 0;
		}
            
        void sort();
            
        void clear();

        void zero();

        void allocate(
            int m, int n, EStorage storage, bool symmetric = false
        );

        void deallocate();

    private:
        int m_;
        int n_;
        int diag_size_;

        CSparseLine* row_;
        CSparseLine* column_;
        double* diag_;

        EStorage storage_;
        bool rows_are_stored_;
        bool columns_are_stored_;
        bool symmetric_storage_;
        bool symmetric_tag_;
		
    };

	
    void  mult(const CSparseMatrix& A, const double* x, double* y);    

    inline void mult(const CSparseMatrix& A, const CVectorD& x, CVectorD& y) 
	{
        assert(x.size() == static_cast<unsigned int>(A.n()));
        assert(y.size() == static_cast<unsigned int>(A.m()));
        mult(A, x.data(), y.data());
    }


    void  mult_transpose(const CSparseMatrix& A, const double* x, double* y);    

    inline void mult_transpose(const CSparseMatrix& A, const CVectorD& x, CVectorD& y) 
	{
        assert(x.size() == static_cast<unsigned int>(A.m()));
        assert(y.size() == static_cast<unsigned int>(A.n()));
        mult_transpose(A, x.data(), y.data());
    }
    
}


