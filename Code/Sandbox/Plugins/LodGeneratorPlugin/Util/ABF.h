#pragma once

#include "TopologyGraph.h"
#include "Vector.h"
#include "SparseMatrix.h"

namespace LODGenerator 
{
	class CComponent;
    class CABF
	{
    public:
        CABF();

        bool do_parameterize_disc(CTopologyGraph* map);
		bool parameterize_disc(CComponent* disc);

        // _____________ Tuning parameters _____________________
        // Most users will not need to change them.


        // Enables Zayer et.al's modified wheel condition
        void set_log_mode(bool x) { log_mode_ = x; }
        bool get_log_mode() const { return log_mode_; }

        // Tolerance for the norm of the gradient
        double get_newton_tolf() const { return newton_tolf_; }
        void set_newton_tolf(double x) { newton_tolf_ = x; }

        // Tolerance for the norm of delta (step vector)
        double get_newton_tolx() const { return newton_tolx_; }
        void set_newton_tolx(double x) { newton_tolx_ = x; }

        // Maximum number of newton iterations (outer loop)
        int get_max_newton_iters() const { return max_newton_iter_; }
        void set_max_newton_iters(int n) { max_newton_iter_ = n; }

        double get_step_length_factor() const { return step_length_factor_; }
        void set_step_length_factor(double x) { step_length_factor_ = x; }

	public:
		CTopologyGraph* component_to_map(CComponent* comp);

    protected:

        void allocate_variables();
        void deallocate_variables();

        // takes into account polygonal facets
        static int nb_triangles(CTopologyGraph* map); 
        static int nb_interior_vertices(CTopologyGraph* map);

        // ------------------ Main driver & Utilities

        void enumerate_angles();
        void compute_beta();
        bool solve_angles();

        // ------------------ Jacobian of the constraints:

        // C1 : alpha_i1 + alpha_i2 + alpha_i3 - PI = 0
        //        where Ti = (i1,i2,i3)
        // C2 : Sum alpha_ij - 2.PI = 0
        //        for alpha_ij incident to vertex i
        // C3 : Prod sin(gamma_ij) - Prod sin(gamma'_ij) = 0
        //        for gamma_ij and gamma'_ij opposite incident to vertex i
        //

        void compute_product_sin_angles(
           CVertex* v, double& prod_prev_sin, double& prod_next_sin
        );

        void compute_sum_log_sin_angles(
           CVertex* v, double& sum_prev_sin, double& sum_next_sin
        );

        // JC1 is implicit.
        void add_JC2();
        void add_JC3();

        // ------------------  Right hand side: gradients

        // Gradient of the quadratic form
        void sub_grad_F();

        // For each triangle: sum angles - PI
        void sub_grad_C1();

        // For each vertex: sum incident angles - 2.PI
        void sub_grad_C2();

        // For each vertex: prod sin(next angle) - prod sin(prev angle)
        void sub_grad_C3();

        // ------------------ Solver for one iteration

        // computes dalpha, dlambda1 and dlambda2
        void solve_current_iteration();

        // ------------------ Convergence control

        // increases the weights of negative angles.
        double compute_step_length_and_update_weights();

        // returns the norm of the gradient (quadratic form + cnstrs).
        double errf() const;

        // returns the norm of the step vector.
        double compute_errx_and_update_x(double s);

        // ------------------ Utilities
        
        // y += J.D.x
        void add_J_D_x(CVectorD& y, const CSparseMatrix& J, CVectorD& D, CVectorD& x);

        // M += J.D.J^t
        void add_J_D_Jt(CSparseMatrix& M, const CSparseMatrix& J, CVectorD& D);

        // M -= J.D.J^t
        void sub_J_D_Jt(CSparseMatrix& M, const CSparseMatrix& J, CVectorD& D);

    private:

        CTopologyGraph* map_;

        int nf_;      // Number of facets
        int nalpha_;  // Number of angles (= 3.nf)
        int nint_;    // Number of interior nodes
        int nlambda_; // Number of constraints (= nf+2.nint)
        int ntot_;    // Total number of unknowns (= nalpha + nlamda)

		std::map<CHalfedge*,int> angle_index_;
		std::map<CHalfedge*,double> angle_angle_;

        // ------ ABF variables & Lagrange multipliers ----------------
        CVectorD alpha_;    // Unknown angles. size = nalpha
        CVectorD lambda_;   // Lagrange multipliers. size = nlambda
        CVectorD beta_;     // Optimum angles.  size = nalpha
        CVectorD w_;        // Weights.         size = nalpha

        // ------ Step vectors ----------------------------------------
        CVectorD dalpha_  ; // size = nalpha; angles
        CVectorD dlambda1_; // size = nf    ; C1 part
        CVectorD dlambda2_; // size = 2.nint; C2 and C3 part

        // ------ Right-hand side ( - gradients ) ---------------------
        CVectorD b1_;      // size = nalpha
        CVectorD b2_;      // size = nlambda

        // ------ Jacobian of the constraints -------------------------
        // J1 (Jacobian of constraint 1) is not stored, it is implicit
        CSparseMatrix J2_; // size = 2.nint * nalpha


        // ------ ABF++ variables -------------------------------------
        CVectorD Delta_inv_     ; // size = nalpha
        CVectorD Delta_star_inv_; // size = nf; 
        CSparseMatrix J_star_  ; // size = 2.nint * nf
        CVectorD b1_star_       ; // size = nf
        CVectorD b2_star_       ; // size = 2.nint

        // ------ Final linear system ---------------------------------
        CSparseMatrix M_       ; // size = 2.nint * 2.nint
        CVectorD       r_       ; // size = 2.nint

        CVectorI perm_;   // column permutations (used by SuperLU)

        //_______________ constants, tuning parameters ____________________

        const double epsilon_;    // threshold for small angles 

        double newton_tolf_;   // threshold for gradient norm (rhs)
        double newton_tolx_;  // threshold for delta norm 
        int max_newton_iter_; 
        const double positive_angle_ro_;
        double step_length_factor_;

        bool low_mem_; // If set, tries to reduce memory footprint
        
        bool log_mode_;
    };

    //_______________________________________________________________________
}

