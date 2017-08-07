#include "StdAfx.h"
#include "LinearSolver.h"
#include "Solver.h"

namespace LODGenerator 
{
	CLinearSolver::CLinearSolver(int nb_variables)
	{
		least_squares_ = true;
		symmetric_ = true;
		update_ = false;

		system_solver_ = new CSolver();
		quiet_ = false;

		nb_variables_ = nb_variables;
		variable_ = new CSolverVariable[nb_variables] ;
	}

	CLinearSolver::~CLinearSolver() 
	{
		delete system_solver_;
		if (nb_variables_ > 0)
			delete[] variable_;
	}

    bool CLinearSolver::solve() 
	{
        bool ok_ = true;
		
		if(system_solver_ != NULL) 
		{
			int nb_iter = G_.n() * 5;            
			if(!quiet_)
				CryLog("LinearSolver Solving ... - dim=%d  - nb_iters=%d",G_.n(),nb_iter);
		} 
		ok_ = system_solver_->solve(G_, x_, b_); 
		if(!quiet_) 
		{
			CryLog("LinearSolver Solved - elapsed time = unknown");
		}
        
        vector_to_variables(x_);
        return ok_;
    }

    void CLinearSolver::restart() 
	{
        update_ = true;
    }

    void CLinearSolver::begin_system() 
	{
        current_row_ = 0;
        if(!update_) {
            // Enumerate free variables.
            int index = 0;
            for(int ii=0; ii < nb_variables(); ii++) {
                CSolverVariable& v = variable(ii);
                if(!v.is_locked()) {
                    set_variable_index(v,index);
                    index++;
                }
            }

            // Size of the system to solve.
            int n = index;

            CSparseMatrix::EStorage storage = CSparseMatrix::eS_ROWS;
            if(system_solver_->needs_columns()) 
			{
                storage = CSparseMatrix::eS_ROWS_AND_COLUMNS;
            }
            bool sym = system_solver_->supports_symmetric_storage() && symmetric();
            x_.resize(n);
            b_.resize(n);
            G_.allocate(n,n, storage, sym);
        }
        x_.zero();
        b_.zero();
        variables_to_vector(x_);
    }
    
    void CLinearSolver::end_system() 
	{
    }
    
    void CLinearSolver::begin_row()
	{
        af_.clear();
        if_.clear();
        al_.clear();
        xl_.clear();
        bk_ = 0.0;
        negative_row_scaling_ = false;
    }
    
    void CLinearSolver::set_right_hand_side(double b) 
	{
        bk_ = b;
    }
    
    void CLinearSolver::add_coefficient(int iv, double a) 
	{
        CSolverVariable& v = variable(iv);
        if(v.is_locked()) {
            al_.push_back(a);
            xl_.push_back(v.value());
        } else {
            af_.push_back(a);
            if_.push_back(v.index());
        }
    }
    
    void CLinearSolver::normalize_row(double weight) 
	{
        double norm = 0.0;
        int nf = af_.size();
        { for(int i=0; i<nf; i++) {
                norm += af_[i] * af_[i];
            }}
        int nl = al_.size();
        { for(int i=0; i<nl; i++) {
                norm += al_[i] * al_[i];
            }}
        norm = sqrt(norm);
        scale_row(weight / norm);
    }
    
    void CLinearSolver::scale_row(double s) 
	{
        negative_row_scaling_ = (s < 0);
        s = ::fabs(s);
        int nf = af_.size();
        { for(int i=0; i<nf; i++) {
                af_[i] *= s;
            }}
        int nl = al_.size();
        { for(int i=0; i<nl; i++) {
                al_[i] *= s;
            }}
        bk_ *= s; 
    }
    
    void CLinearSolver::end_row() 
	{     
        if(least_squares()) 
		{
            int nf = af_.size();
            int nl = al_.size();
                
            if(!update_) 
			{ 
                for(int i=0; i<nf; i++) 
				{
                    for(int j=0; j<nf; j++) 
					{
                        if (!negative_row_scaling_) 
						{
                            G_.add(if_[i], if_[j], af_[i] * af_[j]);
                        } 
						else 
						{
                            G_.add(if_[i], if_[j], -af_[i] * af_[j]);
                        }
                    }
                }
            }

            double S = - bk_;
                
            for(int j=0; j<nl; j++) 
			{
                S += al_[j] * xl_[j];
            }
                
            for(int i=0; i<nf; i++) 
			{
                if (!negative_row_scaling_) 
				{
                    b_(if_[i]) -= af_[i] * S;
                } 
				else 
				{
                    b_(if_[i]) += af_[i] * S;
                }
            }
        } 
		else 
		{
            int nf = af_.size();
            int nl = al_.size();
            if(!update_) 
			{ 
                for(int i=0; i<nf; i++) 
				{
                    G_.add(current_row_, if_[i], af_[i]);
                }
            }                
            b_(current_row_) = bk_;
            for(int i=0; i<nl; i++) 
			{
                 b_(current_row_) -= al_[i] * xl_[i];
            }
        }
        current_row_++;
    }

    void CLinearSolver::vector_to_variables(const CVectorD& x) 
	{
        for(int ii=0; ii < nb_variables(); ii++) 
		{
            CSolverVariable& v = variable(ii);
            if(!v.is_locked()) 
			{
                v.set_value(x(v.index()));
            }
        }
    }

    void CLinearSolver::variables_to_vector(CVectorD& x) 
	{
        for(int ii=0; ii < nb_variables(); ii++) 
		{
            CSolverVariable& v = variable(ii);
            if(!v.is_locked()) 
			{
                x(v.index()) = v.value();
            }
        }
    }

    void CLinearSolver::update_variables() 
	{
        vector_to_variables(x_);
    }

    void CLinearSolver::set_quiet(bool x) 
	{
		quiet_ = x;
    }
}

