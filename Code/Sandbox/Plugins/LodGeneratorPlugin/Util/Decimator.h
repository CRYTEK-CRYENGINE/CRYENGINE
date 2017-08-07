#pragma once

namespace LODGenerator
{
	class CTopologyGraph;
	class CHalfedge;
	class CVertex;
    class CDecimator 
	{
    public:
        enum EStrategy 
		{
            eS_LENGHT_BASED,
            eS_VOLUME_AND_BORDER_BASED,
            eS_ANGULAR_DEFAULT
        } ;
    
        CDecimator(CTopologyGraph* s);
        virtual ~CDecimator() ;
        virtual void collapse_edge(CHalfedge* h);
        void set_proportion_to_remove(double d);
        void set_threshold(double threshold);
        void set_strategy(EStrategy strat) { strategy_=strat; }    
        void apply();

    protected:
        double edge_collapse_importance(CHalfedge* h);
        virtual bool can_collapse(CHalfedge* h);
        double compute_vertex_importance(CVertex* v);


        double delta_volume_cause_by_collapse(CHalfedge* h);
        double delta_area_on_border_cause_by_collapse(CHalfedge* h);
        double delta_shape_cause_by_collapse(CHalfedge* h);
        double delta_angular_default_cause_by_collapse(CHalfedge* h);


        double threshold_;
        int nb_vertex_to_remove_;
        std::map<CVertex*,double> importance_;
        std::map<CVertex*,CHalfedge*> halfedge_to_collapse_;
        std::map<CVertex*,double> accumulated_cost_;
        std::map<CVertex*,double> vertex_density_ ;
        CTopologyGraph* map_;
        EStrategy strategy_;
    } ;
 
}

