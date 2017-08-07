#include "StdAfx.h"
#include "ABF.h"
#include "AnglesToUV.h"
#include "Component.h"
#include "MathTool.h"

#include <fstream>

namespace LODGenerator 
{
//____________________________________________________________________________
    
    CABF::CABF() : epsilon_(1e-5),newton_tolf_(1.0),newton_tolx_(1.0),max_newton_iter_(10),positive_angle_ro_(1.2),step_length_factor_(1.0) 
	{
        log_mode_ = false;
    }

    void CABF::allocate_variables() 
	{

        // ------- sizes & indexes ------
        nf_ = nb_triangles(map_);
        nalpha_ = 3*nf_;
        nint_ = nb_interior_vertices(map_);
        nlambda_ = nf_ + 2*nint_;
        ntot_ = nalpha_ + nlambda_;

        // ------- ABF variables --------
        alpha_.resize(nalpha_);
        lambda_.resize(nlambda_);
        beta_.resize(nalpha_);
        w_.resize(nalpha_);

        // ------- Step vectors ---------
        dalpha_.resize(nalpha_);
        dlambda1_.resize(nf_);
        dlambda2_.resize(2*nint_);

        // ------- Gradients ------------
        b1_.resize(nalpha_);
        b2_.resize(nlambda_);

        // ------- Jacobian -------------
        J2_.allocate(2*nint_, nalpha_, CSparseMatrix::eS_COLUMNS);

        // ------- ABF++ ----------------
        J_star_.allocate(2*nint_, nf_, CSparseMatrix::eS_COLUMNS);
        M_.allocate(2*nint_, 2*nint_, CSparseMatrix::eS_ROWS);

		angle_index_.clear();
		angle_angle_.clear();
    }

    void CABF::deallocate_variables() 
	{
        // ------- ABF variables --------
        alpha_.clear();
        lambda_.clear();
        beta_.clear();
        w_.clear();

        // ------- Step vectors ---------
        dalpha_.clear();
        dlambda1_.clear();
        dlambda2_.clear();

        // ------- Gradients ------------
        b1_.clear();
        b2_.clear();

        J2_.deallocate();

        // ------- ABF++ ----------------
        J_star_.deallocate();
        M_.deallocate();
    }

	bool CABF::parameterize_disc(CComponent* disc) 
	{
		std::map<CVertex*,CVertex*> orig_vertex;
		CTopologyGraph* map = disc->component_to_topology(orig_vertex);
		bool result = do_parameterize_disc(map);
		map->update_from_map(orig_vertex);
		delete map;
		return result;
	}

    bool CABF::do_parameterize_disc(CTopologyGraph* map) 
	{
        map_ = map;
        allocate_variables();

        // Sanity check.
        FOR_EACH_FACET(CTopologyGraph, map_, it) 
		{
            assert((*it)->is_triangle());
        }

        enumerate_angles();
        compute_beta();

        if(solve_angles()) 
		{
            FOR_EACH_HALFEDGE(CTopologyGraph, map_, it) 
			{
                angle_angle_[*it] = alpha_(angle_index_[*it]);
            }
        } 
		else 
		{
            CryLog("ABF++ Did not converge.");
			CryLog("ABF++ Switching to LSCM."); 
            // Note: AnglesToUV with angles measured on the mesh (i.e. beta's) = LSCM !!!
            FOR_EACH_HALFEDGE(CTopologyGraph, map_, it) 
			{
                angle_angle_[*it] = beta_(angle_index_[*it]);
            }
        } 

        deallocate_variables();

        CAnglesToUV a_to_uv;
        a_to_uv.do_parameterize_disc(map_,&angle_angle_);
        map_ = NULL;
        return true;
    }

    int CABF::nb_interior_vertices(CTopologyGraph* map) 
	{
        int result = 0;
        FOR_EACH_VERTEX(CTopologyGraph, map, it) 
		{
            if(!(*it)->is_on_border()) 
			{
                result++;
            }
        }
        return result;
    }

    int CABF::nb_triangles(CTopologyGraph* map) 
	{
        int result = 0;
        FOR_EACH_FACET(CTopologyGraph, map, it) 
		{
            result += (*it)->degree() - 2;
        }
        return result;
    }
    

    void CABF::enumerate_angles() 
	{
        int cur = 0;
        FOR_EACH_FACET(CTopologyGraph, map_, f) 
		{
            CHalfedge* h = (*f)->halfedge();
            do 
			{
                angle_index_[h] = cur;
                cur++;
                h = h->next();
            } while(h != (*f)->halfedge());
        }
        // sanity check
        assert(cur == int(map_->size_of_facets()) * 3);
    }

    // Had a hard time finding what was wrong with my code:
    // OK, got it, use h->next() instead of h->next_around_vertex()
    // because h->next_around_vertex() does not return the same
    // angle.
    static inline double corner_angle(CHalfedge* h) 
	{	
        double result = g_PI - angle(h->direct(),h->next()->direct());
        result = max(result, 2.0 * g_PI / 360.0);
        return result;
    }
    
    void CABF::compute_beta() {
        
        FOR_EACH_VERTEX(CTopologyGraph, map_, vit) 
		{
            // Compute sum_angles
            double sum_angle = 0.0;
            {
                CHalfedge* h = (*vit)->halfedge();
                do 
				{
                    double angle = corner_angle(h);
                    if( angle < 5.*g_PI/180 )
					{
//                        Logger::out("ABF++") << "Small angle encountered: "
//                                        << angle*180.0/g_PI << std::endl; 
                    }
					if( angle > 170.*g_PI/180 )
					{
//                        Logger::out("ABF++") << "Large angle encountered: "
//                                        << angle*180.0/g_PI << std::endl; 
                    }

                    sum_angle += angle;
                    h = h->next_around_vertex();
                } while(h != (*vit)->halfedge()); 
            }

            double ratio = 1.0;
        
            if(!(*vit)->is_on_border()) {
                ratio =  2.0 * g_PI / sum_angle;
            }
            
            {
                CHalfedge* h = (*vit)->halfedge();
                do {
                    if(!h->is_border()) {
                        int angle_ind = angle_index_[h];
                        beta_(angle_ind) = corner_angle(h) * ratio;
                        if( beta_(angle_ind) < 3.*g_PI/180. ) { 
                            beta_(angle_ind) =  3.*g_PI/180.;
                        } else if( beta_(angle_ind) > 175.*g_PI/180. ) { 
                            beta_(angle_ind) =  175.*g_PI/180.;
                        }
                    }
                    h = h->next_around_vertex();
                } while(h != (*vit)->halfedge()); 
            }
        }
    }


    bool CABF::solve_angles() 
	{

        bool is_grob = false; // (dynamic_cast<Grob*>(map_) != NULL);
        
        // Initial values

        perm_.clear();
        lambda_.zero();
        for(int i=0; i<nalpha_; i++) 
		{
            alpha_(i) = beta_(i);
            w_(i) = 1.0 / (beta_(i) * beta_(i));
        }

        double errx_k_m_1 = big_double;

        for(int k=0; k<max_newton_iter_; k++) 
		{
            // Compute Jacobian
            J2_.zero();
            add_JC2();
            add_JC3();

            // Compute rhs
            b1_.zero();
            b2_.zero();
            sub_grad_F();
            sub_grad_C1();
            sub_grad_C2();
            sub_grad_C3();

            double errf_k = errf();

			CryLog("ABF++ iter= %d errf=%f",k,errf_k);
/*
            if(_isnan(errf_k) || errf_k > 1e9) 
			{
                return false;
            }
*/
            if(_isnan(errf_k) || errf_k > 1e18) 
			{
                return false;
            }


            if(errf_k <= newton_tolf_) 
			{
				CryLog("ABF++ converged");
                return true;
            }

            solve_current_iteration();

            double errx;
            double s = compute_step_length_and_update_weights();
            s *= step_length_factor_;
            errx = compute_errx_and_update_x(s);

            // TODO: since errx is dependent on the size of the mesh,
            // weight the threshold by the size of the mesh.
/*
            if(_isnan(errx) || errx > 1e6) {
                Logger::err("ABF++") << "errx: " << errx << std::endl;
                return false;
            }
*/
            if(_isnan(errx) || errx > 1e15) 
			{
				CryLog("ABF++ errx: %f",errx);
                return false;
            }

			CryLog("ABF++ iter= %d errx=%f",k,errx);

            if(errx <= newton_tolx_) 
			{
				CryLog("ABF++ converged");
                return true;
            }

            errx_k_m_1 = errx;

            if(is_grob) 
			{
                FOR_EACH_HALFEDGE(CTopologyGraph, map_, it) 
				{
                    angle_angle_[*it] = alpha_(angle_index_[*it]);
                }
                CAnglesToUV a_to_uv;
                a_to_uv.do_parameterize_disc(map_,&angle_angle_);
            }
        }

		CryLog("ABF++ ran out of Newton iters");
        return true;
    }

    void CABF::solve_current_iteration() 
	{       
        Delta_inv_.resize(nalpha_);
        if(log_mode_) 
		{
            {for(int i=0; i<nalpha_; i++) 
			{
                Delta_inv_(i) = 2.0 * w_(i);
            }}
/*  
    // Note: the Hessian terms coming from the modified wheel
    // compatibility constraint can make the coefficients of
    // Delta equal to zero ... commented out.
            {FOR_EACH_VERTEX(CTopologyGraph, map_, it) {
                if(it->is_on_border()) {
                    continue;
                }
                CHalfedge* h = it->halfedge();
                do {
                    int i = angle_index_[h->next()];
                    Delta_inv_(i) -= lambda_(i) / ogf_sqr(sin(alpha_(i)));
                    i = angle_index_[h->prev()];
                    Delta_inv_(i) += lambda_(i) / ogf_sqr(sin(alpha_(i)));
                    h = h->next_around_vertex();
                } while(h != it->halfedge());
            }}
*/
            for(int i=0; i<nalpha_; i++) 
			{
                assert(fabs(Delta_inv_(i)) > 1e-30);
                Delta_inv_(i) = 1.0 / Delta_inv_(i);
            }
        } 
		else 
		{
            for(int i=0; i<nalpha_; i++) 
			{
                Delta_inv_(i) = 1.0 / (2.0 * w_(i));
            }
        }

        
        // 1) Create the pieces of J.Delta^-1.Jt
        // 1.1) Diagonal part: Delta*^-1
        Delta_star_inv_.resize(nf_);
        {for(int i=0; i<nf_; i++) 
		{
            Delta_star_inv_(i) =  1.0 / (Delta_inv_(3*i) + Delta_inv_(3*i+1) + Delta_inv_(3*i+2) );
        }}

        // 1.2) J*  = J2.Delta^-1.J1^t
        J_star_.zero();
        for(int j=0; j<nalpha_; j++) 
		{
            const CSparseMatrix::Column& Cj = J2_.column(j);
            for(int ii=0; ii<Cj.nb_coeffs(); ii++) 
			{
                const CCoeff& c = Cj.coeff(ii);
                J_star_.add(c.index, j / 3, c.a * Delta_inv_(j));
            }
        }
        // Note: J** does not need to be built, it is directly added to M.

        // 2) Right hand side: b1* and b2*

        // 2.1) b1* = J1.Delta^-1.b1 - b2[1..nf]
        b1_star_.resize(nf_);
        for(int i=0; i<nf_; i++) 
		{
            b1_star_(i) = 
                Delta_inv_(3*i  ) * b1_(3*i  ) + 
                Delta_inv_(3*i+1) * b1_(3*i+1) + 
                Delta_inv_(3*i+2) * b1_(3*i+2) - b2_(i);
        }
        // 2.2) b2* = J2.Delta^-1.b1 - b2[nf+1 .. nf+2.nint-1]
        b2_star_.resize(2*nint_);
        b2_star_.zero();
        add_J_D_x(b2_star_, J2_, Delta_inv_, b1_);
        for(int i=0; i<2*nint_; i++) 
		{
            b2_star_(i) -= b2_(nf_+i);
        }


        // 3) create final linear system 
        
        // 3.1) M = J*.Delta*^-1.J*^t - J**
        //       where J** = J2.Delta^-1.J2^t
        M_.zero();
        add_J_D_Jt(M_, J_star_, Delta_star_inv_);
        sub_J_D_Jt(M_, J2_, Delta_inv_);

        // 3.2) r = J*.Delta*^-1.b1* - b2*
        r_.resize(2*nint_);
        r_.zero();
        add_J_D_x(r_, J_star_, Delta_star_inv_, b1_star_);
        r_ -= b2_star_;
        
        // First call initializes perm_
        solve_super_lu(M_, r_, dlambda2_, perm_, true);

        // 4) compute dlambda1 and dalpha in function of dlambda2
        
        // 4.1) dlambda1 = Delta*^-1 ( b1* - J*^t dlambda2 )
        mult_transpose(J_star_, dlambda2_, dlambda1_);
        for(int i=0; i<nf_; i++) 
		{
            dlambda1_(i) = Delta_star_inv_(i) * (b1_star_(i) - dlambda1_(i));
        }
        
        // 4.2) Compute dalpha in function of dlambda:
        // dalpha = Delta^-1( b1 -  J^t.dlambda                    )
        //        = Delta^-1( b1 - (J1^t.dlambda1 + J2^t.dlambda2) )
        mult_transpose(J2_, dlambda2_, dalpha_);
        for(int i=0; i<nf_; i++) 
		{
            dalpha_(3*i)   += dlambda1_(i);
            dalpha_(3*i+1) += dlambda1_(i);
            dalpha_(3*i+2) += dlambda1_(i);
        }
        for(int i=0; i<nalpha_; i++) 
		{
            dalpha_(i) = Delta_inv_(i) * (b1_(i) - dalpha_(i));
        }

        // Release as much memory as possible.
        Delta_inv_.clear();
        Delta_star_inv_.clear();
        J_star_.clear();
        b1_star_.clear();
        b2_star_.clear();
        // Note: M_ already cleared by SuperLU wrapper when in low-mem mode
        r_.clear();
    }


    // ----------------------------- Jacobian -----------------------------

    void CABF::add_JC2() 
	{
        int i = 0;
        FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
            if((*it)->is_on_border()) 
			{
                continue;
            }
            CHalfedge* h = (*it)->halfedge();
            do 
			{
                J2_.add(i, angle_index_[h], 1.0);
                h = h->next_around_vertex();
            } while(h != (*it)->halfedge());
            i++;
        }
    }
    
    void CABF::compute_product_sin_angles(CVertex* v, double& prod_prev_sin, double& prod_next_sin) 
	{
        prod_prev_sin = 1.0;
        prod_next_sin = 1.0;
        CHalfedge* h = v->halfedge();
        do 
		{
            prod_prev_sin *= sin(alpha_(angle_index_[h->prev()]));
            prod_next_sin *= sin(alpha_(angle_index_[h->next()]));
            h = h->next_around_vertex();
        } while(h != v->halfedge());
    }

    void CABF::compute_sum_log_sin_angles(CVertex* v, double& sum_prev_sin, double& sum_next_sin) 
	{
        sum_prev_sin = 0.0;
        sum_next_sin = 0.0;
        CHalfedge* h = v->halfedge();
        do 
		{
            sum_prev_sin += log(sin(alpha_(angle_index_[h->prev()])));
            sum_next_sin += log(sin(alpha_(angle_index_[h->next()])));
            h = h->next_around_vertex();
        } while(h != v->halfedge());
    }


    void CABF::add_JC3() 
	{
        if(log_mode_) {
            int i = nint_;
            FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
			{
                if((*it)->is_on_border()) {
                    continue;
                }
                CHalfedge* h = (*it)->halfedge();
                do {
                    int j = angle_index_[h->next()];
                    J2_.add(i,j, cos(alpha_(j)) / sin(alpha_(j)));
                    j = angle_index_[h->prev()];
                    J2_.add(i,j,-cos(alpha_(j)) / sin(alpha_(j)));
                    h = h->next_around_vertex();
                } while(h != (*it)->halfedge());
                i++;
            }
        } else {
            int i = nint_;
            FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
			{
                if((*it)->is_on_border()) {
                    continue;
                }
                double prod_prev_sin;
                double prod_next_sin;
                compute_product_sin_angles(*it, prod_prev_sin, prod_next_sin);
                CHalfedge* h = (*it)->halfedge();
                do 
				{
                    int j = angle_index_[h->next()];
                    J2_.add(i, j, prod_next_sin * cos(alpha_(j)) / sin(alpha_(j)));
                    j = angle_index_[h->prev()];
                    J2_.add(i, j,-prod_prev_sin * cos(alpha_(j)) / sin(alpha_(j)));
                    h = h->next_around_vertex();
                } while(h != (*it)->halfedge());
                i++;
            }
        }
    }


    // ----------------------- Right hand side -----------------------

    void CABF::sub_grad_F() 
	{
        for(int i = 0; i < nalpha_; i++ ) 
		{
            b1_(i) -= 2.0 * w_(i) * ( alpha_(i) - beta_(i) );
        }
    }
    
    // For each triangle: sum angles - PI
    void CABF::sub_grad_C1() 
	{
        for(int i=0; i < nf_; i++) 
		{
            for(int j=0; j<3; j++) 
			{
                b1_(3*i+j) -= lambda_(i);
            }
        }
        for(int i=0; i < nf_; i++) 
		{
            b2_(i) -= 
                alpha_(3*i) + alpha_(3*i+1) + alpha_(3*i+2) - g_PI;
        }
    }
    
    // For each vertex: sum incident angles - 2.PI
    void CABF::sub_grad_C2() 
	{
        int i = nf_;
        FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
		{
            if((*it)->is_on_border()) 
			{
                continue;
            }
            CHalfedge* h = (*it)->halfedge();
            do {
                b2_(i) -= alpha_(angle_index_[h]);
                b1_(angle_index_[h]) -= lambda_(i);
                h = h->next_around_vertex();
            } while(h != (*it)->halfedge());
            b2_(i) += 2.0 * g_PI;
            i++;
        }
    }
    
    // For each vertex: prod sin(next angle) - prod sin(prev angle)
    void CABF::sub_grad_C3() 
	{
        if(log_mode_) 
		{
            int i = nf_ + nint_;
            FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
			{
                if((*it)->is_on_border()) {
                    continue;
                }
                
                double sum_prev_sin;
                double sum_next_sin;
                compute_sum_log_sin_angles(*it, sum_prev_sin, sum_next_sin);
                
                b2_(i) -= (sum_next_sin - sum_prev_sin);
                
                CHalfedge* h = (*it)->halfedge();
                do 
				{
                    int j = angle_index_[h->next()];
                    b1_(j) -= lambda_(i) * cos(alpha_(j)) / sin(alpha_(j));
                    j = angle_index_[h->prev()];
                    b1_(j) += lambda_(i) * cos(alpha_(j)) / sin(alpha_(j));
                    
                    h = h->next_around_vertex();
                } 
				while(h != (*it)->halfedge());
                i++;
            }
        } else {
            int i = nf_ + nint_;
            FOR_EACH_VERTEX(CTopologyGraph, map_, it) 
			{
                if((*it)->is_on_border()) {
                    continue;
                }
                double prod_prev_sin;
                double prod_next_sin;
                compute_product_sin_angles(*it, prod_prev_sin, prod_next_sin);
                
                b2_(i) -= prod_next_sin - prod_prev_sin;
                
                CHalfedge* h = (*it)->halfedge();
                do 
				{
                    int j = angle_index_[h->next()];
                    b1_(j) -= 
                        lambda_(i) * prod_next_sin * cos(alpha_(j)) / sin(alpha_(j));
                    
                    j = angle_index_[h->prev()];
                    b1_(j) += 
                        lambda_(i) * prod_prev_sin * cos(alpha_(j)) / sin(alpha_(j));
                    
                    h = h->next_around_vertex();
                } 
				while(h != (*it)->halfedge());
                i++;
            }
        }
    }

    double CABF::compute_errx_and_update_x(double s) 
	{
        double result = 0;

        // alpha += s * dalpha 
        for(int i=0; i<nalpha_; i++) {
            double dai = s * dalpha_(i);
            alpha_(i) += dai;
            result += fabs(dai);
        }

        // lambda += s * dlambda
        for(int i=0; i<nf_; i++) 
		{
            double dai = s * dlambda1_(i);
            lambda_(i) += dai;
            result += fabs(dai);
        }
        for(int i=0; i<2*nint_; i++) 
		{
            double dai = s * dlambda2_(i);
            lambda_(nf_+i) += dai;
            result += fabs(dai);
        }

        return result;
    }
    
    // ______ Convergence control ____________________________________

    double CABF::compute_step_length_and_update_weights() 
	{
        double ratio = 1.0;

        for(int i=0; i<nalpha_; i++) 
		{
            if(alpha_(i) + dalpha_(i) < 10.0 * epsilon_) 
			{
                double r1 = -.5 * (alpha_(i) - 10.0 * epsilon_)/ dalpha_(i);
                ratio = min(ratio, r1);
                w_(i) *= positive_angle_ro_;
/*
                std::cerr << " w  ( at 0 ) " 
                          << i << " is " << w_(i) << std::endl;
                std::cerr << " val " 
                          << alpha_(i) + dalpha_(i) << " was " 
                          << alpha_(i) << std::endl;
*/
                
            } 
			else if(alpha_(i) + dalpha_(i) > g_PI - 10.0 * epsilon_) 
			{
                double r1 = .5*(g_PI - alpha_(i)+10.0 * epsilon_) / dalpha_(i);
//                ratio = min(ratio, r1);
                w_(i) *= positive_angle_ro_;
/*
                std::cerr << " w ( over 180) " << i << " is " 
                          << w_(i) << std::endl;
                std::cerr << " val " << alpha_(i) + dalpha_(i) << " was " 
                          << alpha_(i) << std::endl;
				*/           
			}
			
		}
        return ratio;
    }

    double CABF::errf() const 
	{
        double result = 0;
        {for(int i=0; i<nalpha_; i++) 
		{
            result += fabs(b1_(i));
        }}
        {for(int i=0; i<nlambda_; i++) 
		{
            result += fabs(b2_(i));
        }}
        return result;
    }

    //____________ Utilities _____________________________________________

    void CABF::add_J_D_x(CVectorD& y, const CSparseMatrix& J, CVectorD& D, CVectorD& x) 
	{
        assert(y.size() == J.m());
        assert(D.size() == J.n());
        assert(x.size() == J.n());

        for(unsigned int j=0; j<D.size(); j++) 
		{
            const CSparseMatrix::Column& Cj = J.column(j);
            for(int ii=0; ii<Cj.nb_coeffs(); ii++) 
			{
                const CCoeff& c = Cj.coeff(ii);
                y(c.index) += c.a * D(j) * x(j);
            }
        }
    }
    
    void CABF::add_J_D_Jt(CSparseMatrix& M, const CSparseMatrix& J, CVectorD& D) 
	{
        assert(M.m() == J.m());
        assert(M.n() == J.m());
        assert(D.size() == J.n());

        for(unsigned int j=0; j<D.size(); j++) 
		{
            const CSparseMatrix::Column& Cj = J.column(j);
            for(int ii1=0; ii1<Cj.nb_coeffs(); ii1++) 
			{        
                for(int ii2=0; ii2<Cj.nb_coeffs(); ii2++) 
				{                
                    M.add(
                        Cj.coeff(ii1).index, Cj.coeff(ii2).index,
                        Cj.coeff(ii1).a * Cj.coeff(ii2).a * D(j)
                    );
                }
            }
        }
    }

    void CABF::sub_J_D_Jt(CSparseMatrix& M, const CSparseMatrix& J, CVectorD& D) 
	{
        assert(M.m() == J.m());
        assert(M.n() == J.m());
        assert(D.size() == J.n());

        for(unsigned int j=0; j<D.size(); j++) 
		{
            const CSparseMatrix::Column& Cj = J.column(j);
            for(int ii1=0; ii1<Cj.nb_coeffs(); ii1++) 
			{        
                for(int ii2=0; ii2<Cj.nb_coeffs(); ii2++) 
				{                
                    M.add(
                        Cj.coeff(ii1).index, Cj.coeff(ii2).index,
                        - Cj.coeff(ii1).a * Cj.coeff(ii2).a * D(j)
                    );
                }
            }
        }
    }

    //___________________________________________________________________
}
