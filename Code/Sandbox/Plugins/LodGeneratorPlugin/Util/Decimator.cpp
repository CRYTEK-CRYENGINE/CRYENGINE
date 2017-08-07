#include "StdAfx.h"
#include "Decimator.h"
#include "TopologyGraph.h"
#include "MathTool.h"
#include "Heap.h"

namespace LODGenerator
{
    CDecimator::CDecimator(CTopologyGraph* s) {
        map_ = s;
        nb_vertex_to_remove_ = int((map_->size_of_vertices()*50.0)/100.0); 
        threshold_ = 1;
        set_strategy(eS_VOLUME_AND_BORDER_BASED);
    }
    
    void CDecimator::set_proportion_to_remove(double d) {
        nb_vertex_to_remove_ = int(double (map_->size_of_vertices())*d) ;
    }
    
    void CDecimator::set_threshold(double threshold){
        threshold_ = threshold;
    }
    
    CDecimator::~CDecimator() {
    }

    void CDecimator::collapse_edge(CHalfedge* h){
        CTopologyGraphMutator editor(map_);
        editor.collapse_edge(h);
    }

    void CDecimator::apply(){

        FOR_EACH_FACET(CTopologyGraph,map_,ti) {
            assert((*ti)->is_triangle()) ;
        }

        FOR_EACH_VERTEX(CTopologyGraph,map_,vi) {
            compute_vertex_importance(*vi);
            accumulated_cost_[*vi]=0;
        }
        
        CMapVertexHeap heap(
            map_,importance_
        );

        heap.init_with_all_surface_vertices();
        
        int nb_vertex_to_remove_init = nb_vertex_to_remove_ ;

        int last_val = 0 ;
        while (!heap.empty() && (nb_vertex_to_remove_ > 0)) {
			CVertex*  top = heap.pop();

            if (
                halfedge_to_collapse_[top] != NULL &&
                can_collapse(halfedge_to_collapse_[top]) 
            ) {
                nb_vertex_to_remove_--;
                std::vector<CVertex*> neighbors;
                CHalfedge* cir = top->halfedge();
                do {
                    neighbors.push_back(cir->opposite()->vertex());
                    cir = cir->next_around_vertex() ;
                } while ( cir != top->halfedge());
                
                assert(
                    halfedge_to_collapse_[top]->opposite()->vertex() == top
                );
                
                CVertex* v = halfedge_to_collapse_[top]->vertex();
                accumulated_cost_[v] +=compute_vertex_importance(top);
                
                assert(can_collapse(halfedge_to_collapse_[top]));
                collapse_edge(halfedge_to_collapse_[top]);
                
                for (unsigned int i=0; i<neighbors.size();i++){
                    compute_vertex_importance(neighbors[i]);
                    if (heap.contains(neighbors[i])) {
                        heap.update_cost(neighbors[i]);
                    } else {
                        if (importance_[neighbors[i]] < threshold_)
                            heap.push(neighbors[i]);
                    }
                }
            } 
        }
    }
    

    double CDecimator::delta_volume_cause_by_collapse(CHalfedge* h){
        double delta_vol=0; 
        CHalfedge* cir = h->opposite();
        do {
            if (
                cir !=  h->opposite() && cir != h->prev() && !cir->is_border()
            ) {
                Vec3 v0 = 
                    cir->next()->vertex()->point() - h->vertex()->point();
                Vec3 v1 = 
                    cir->opposite()->vertex()->point() - h->vertex()->point();
                Vec3 v2 = h->opposite()->direct();
                
                delta_vol += ::fabs((v0.Cross(v1)).Dot(v2));
            }	
            cir = cir->next_around_vertex() ;
        } while (cir != h->opposite());
        return delta_vol;
    }
    
    double CDecimator::delta_area_on_border_cause_by_collapse(CHalfedge* h)
	{
        if (h->is_border())
            return ((h->direct()).Cross( h->prev()->opposite()->direct())).len();
        else if (h->opposite()->is_border())
            return ((h->opposite()->next()->direct()).Cross(h->opposite()->direct())).len();
        return 0; 
    }
    
    
    double CDecimator::delta_shape_cause_by_collapse(CHalfedge* h) {
        double delta_vol=0; 
        double area = 0;
        CHalfedge* cir = h->opposite();
        do
		{
            if (cir !=  h->opposite() && cir != h->prev() && !cir->is_border()) 
			{
                Vec3 v0 = cir->next()->vertex()->point() - h->vertex()->point();
                Vec3 v1 = cir->opposite()->vertex()->point() - h->vertex()->point();
                Vec3 v2 = h->opposite()->direct();
                
                area += (v0.Cross(v1)).len() / 2.0;
                delta_vol += ::fabs((v0.Cross(v1)).Dot(v2));
            }	
            cir = cir->next_around_vertex();
        } 
		while (cir != h->opposite());
        return  delta_vol / ::pow(area,1.5);
    }
    
    double sigma_angles(CVertex* v) 
	{
        CHalfedge* cir = v->halfedge();
        double angle=0;
        do 
		{
            Vec3 v0 = cir->opposite()->direct().normalize();
            Vec3 v1 = cir->next()->direct().normalize();
            angle += ::acos(v0.Dot(v1));
            cir = cir->next_around_vertex();
        } 
		while (cir !=  v->halfedge());
        return angle;
    }
    
    double CDecimator::delta_angular_default_cause_by_collapse(CHalfedge* h) 
	{
        CHalfedge* cir = h->opposite();
        double angle_=0;
        do 
		{
            CVertex* v=cir->opposite()->vertex();
            double original_angle=sigma_angles(v);
            if (v!=h->vertex())
			{
                original_angle -= angle( cir->direct(), cir->prev()->opposite()->direct());
                original_angle -= angle( cir->direct(), cir->opposite()->next()->direct());
                Vec3 v =  h->vertex()->point() - cir->opposite()->vertex()->point();
                original_angle += angle( v, cir->prev()->opposite()->direct()) ;
                original_angle += angle( v, cir->opposite()->next()->direct()) ;
            } 
			else 
			{
                CHalfedge* innercir = h->opposite();
                original_angle -= angle(h->opposite()->direct(), h->next()->direct()) ;
                original_angle -= angle(h->opposite()->direct(), h->opposite()->prev()->opposite()->direct() ) ;
                
                do 
				{
                    if (innercir!=h->opposite()  && innercir->next()!=h)
                        original_angle += 
                            angle(
                                innercir->opposite()->vertex()->point() -
                                h->vertex()->point(),
                                innercir->next()->vertex()->point() -
                                h->vertex()->point()
                            ) ;
                    innercir = innercir->next_around_vertex();
                } 
				while (innercir != h->opposite());
            }
            angle_ += ::fabs(2.0*g_PI - original_angle );
            cir = cir->next_around_vertex();
        }
		while (cir != h->opposite());
        return angle_;
    }
    
    double CDecimator::edge_collapse_importance(CHalfedge* h){
        if (strategy_ == eS_LENGHT_BASED){
            return h->direct().len();
        }
        if (strategy_ == eS_ANGULAR_DEFAULT){
            return delta_angular_default_cause_by_collapse(h);
        }
	
        double delta_vol=delta_volume_cause_by_collapse(h); 
        double delta_area=delta_area_on_border_cause_by_collapse(h);
        
        double min_angle_sin_2=1;
        CHalfedge* cir = h->opposite();
        do 
		{
            if (cir !=  h->opposite() && cir != h->prev() && !cir->is_border()) 
			{
                Vec3 v0 = (cir->next()->vertex()->point() - h->vertex()->point()).normalize();
                Vec3 v1 = (cir->opposite()->vertex()->point() - h->vertex()->point()).normalize();
                min_angle_sin_2 = MIN(min_angle_sin_2, (v0.Cross(v1)).len2());
            }
            cir = cir->next_around_vertex() ;
        } 
		while (cir != h->opposite());
        if (min_angle_sin_2<1e-10) 
		{
            return 1e20;
        }
        return ::pow(delta_vol,0.333) + 5.0 * ::pow(delta_area,0.5);
    }




    bool CDecimator::can_collapse(CHalfedge* h)
	{
        CTopologyGraphMutator editor(map_);
        if (!editor.can_collapse_edge(h)) {
            return false ;
        }
        
        if (
            !h->is_border() &&
            !h->opposite()->is_border() &&
            h->opposite()->vertex()->is_on_border()
        ) {
            return false ;
        }
        
        CHalfedge* cir = h->opposite();
        do {
            if(
                cir !=  h->opposite() && cir != h->prev() && !cir->is_border()
	    ) {
                Vec3 v0 =
                    (cir->next()->vertex()->point() - h->vertex()->point()).normalize();
                Vec3 v1 =
                    (cir->opposite()->vertex()->point() - h->vertex()->point()).normalize();
	  
                Vec3 vertex_normal = 
                    h->opposite()->vertex()->vertex_normal();

                Vec3 future_normal = (v0.Cross(v1)).normalize();
                double tmp = vertex_normal.Dot(future_normal);
                if ( 
                    (tmp < 0) || 
                    (v0.Dot(v1) < -0.999) || 
                    (v0.Dot(v1) > 0.999)
                ) {
                    return false; 
                }
            }	
            cir = cir->next_around_vertex() ;
        } while (cir != h->opposite());
        return true;
    }


    double CDecimator::compute_vertex_importance(CVertex* v){
        CHalfedge* cir = v->halfedge();
        importance_[v] = 10e20;
        halfedge_to_collapse_[v] = NULL ;
        do {
            if (can_collapse(cir->opposite())) {
                double imp = edge_collapse_importance(cir->opposite());
                if (imp < importance_[v]) {
                    importance_[v] = imp;
                    halfedge_to_collapse_[v] = cir->opposite();
                }
            }
            cir = cir->next_around_vertex();
        } while (cir != v->halfedge());

        if(vertex_density_.find(v) != vertex_density_.end()) {
            importance_[v] *= (1.0 + vertex_density_[v]) ;
        }

        double result = importance_[v] +  accumulated_cost_[v];
        return result ;
    }
  
}
