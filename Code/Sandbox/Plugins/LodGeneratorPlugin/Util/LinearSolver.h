#pragma once

#include "SparseMatrix.h"
#include "Vector.h"

namespace LODGenerator 
{
	class CSolverVariable 
	{
	public:
		CSolverVariable() : x_(0), index_(-1), locked_(false) { }

		double value() const { return x_; }
		void set_value(double x_in) { x_ = x_in; }
		void lock()   { locked_ = true; }
		void unlock() { locked_ = false; }
		bool is_locked() const { return locked_; }

		int index() const 
		{
			assert(index_ != -1);
			return index_;
		}

		void set_index(int index_in) 
		{
			index_ = index_in; 
		}

	private:
		double x_;
		int index_;
		bool locked_;
	};


	class CSolver;
    class CLinearSolver
	{
    public:
        CLinearSolver(int nb_variables);
        ~CLinearSolver();

        bool solve();
        void restart();

        bool least_squares() const { return least_squares_;  }
        void set_least_squares(bool b) {
            least_squares_ = b;
            if(b) {
                symmetric_ =  true;
            }
        }
        bool symmetric() const  { return symmetric_; }
        void set_symmetric(bool b) { symmetric_ = b; }

        virtual void set_quiet(bool x);
        CSparseMatrix * get_matrix(){ return &G_; }

        void begin_system();
        void end_system();
        
        void begin_row();
        void set_right_hand_side(double b);
        void add_coefficient(int iv, double a);
        void normalize_row(double weight = 1.0);
        void scale_row(double s);
        void end_row();
            
        virtual void update_variables();

		CSolverVariable& variable(int idx) 
		{ 
			assert(idx >= 0 && idx < nb_variables_) ;
			return variable_[idx] ;
		}

		const CSolverVariable& variable(int idx) const 
		{
			assert(idx >= 0 && idx < nb_variables_) ;
			return variable_[idx] ;
		}
		int nb_variables() const { return nb_variables_ ; }


    protected:
        
        void vector_to_variables(const CVectorD& x);
        void variables_to_vector(CVectorD& x);
		void set_variable_index(CSolverVariable& var, int index) 
		{
			var.set_index(index) ;
		}

    private:
        
        // Internal representation
        CSolver* system_solver_;
        CVectorD x_; 
        CSparseMatrix G_;
        CVectorD b_;
        
        // Parameters 
        bool least_squares_;
        bool symmetric_;
        
        // Current row of A
        std::vector<double> af_;
        std::vector<int>    if_;
        std::vector<double> al_;
        std::vector<double> xl_;
        double bk_;
        bool negative_row_scaling_;

        int current_row_;
        
        // Iterative refinement
        bool update_;
        CVectorI perm_;
        CSparseMatrix M_inv_;
		bool quiet_;
		CSolverVariable* variable_;
		int nb_variables_ ;
	};
}


