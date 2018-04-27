#pragma once

#include "TopologyGraph.h"
#include <stack>
#include <queue>

namespace LODGenerator 
{
    class CL12LinearProxy 
	{
    public:
        static void set_compactness_importance(double x) { compactness_importance_ = x; }
        static double compactness_importance() { return compactness_importance_; }
        static void set_angular_threshold(double x) { 
            angular_threshold_ = x; n_mode_ = (angular_threshold_ < 1.0); 
        }
        static double angular_threshold() { return angular_threshold_; }
        static bool n_mode() { return n_mode_; }

        CL12LinearProxy();
        CL12LinearProxy(const Vec3d& v, const Vec3d& p);
        CL12LinearProxy(CFacet* f);

        double distance_3(CFacet* f, double area) const;
        double deviation(CFacet* f) const;
        double angle_deviation(CFacet* f) const;

    private:
        Vec3d N_;
        Vec3d G_;
        static double compactness_importance_;
        static double angular_threshold_;
        static bool n_mode_;
    };

    class CL12LinearProxyFitter 
	{
    public:
        void begin();
        void end();
        CL12LinearProxy proxy();
        void add_facet(CFacet* f);

    private:
        Vec3d N_;
        Vec3d G_;
        double A_;
    };

    //____________________________________________________________________________________

    struct SAddFacetToChart 
	{
        SAddFacetToChart(CFacet* f, int c, double e) : facet(f), chart(c), E(e) {}
        CFacet* facet;
        int chart;
        double E;
    };

    class CAddFacetToChartCmp 
	{
    public:
        bool operator()( const SAddFacetToChart& op1, const SAddFacetToChart& op2) 
		{
            return (op1.E > op2.E);
        }
    };


    typedef std::priority_queue< 
        SAddFacetToChart,
        std::vector<SAddFacetToChart>,
        CAddFacetToChartCmp
    > AddFacetToChartQueue;
	
	class CComponent;
    class CApprox 
	{
    public:
        CApprox(CComponent* map, std::map<CFacet*,int>& chart);

        void init(int nb_proxies, int nb_iter, double min_err = 0.0, int min_nb_proxies = 0);
        double optimize(int nb_iter);
        void add_charts(int nb_charts, int nb_iter, double min_err = 0.0, int min_nb_charts = 0);
        double error();

    protected:

        CFacet* new_chart();
        void compute_chart_sizes();
        void check_facets();
        CFacet* worst_facet();
        int worst_chart();
        CFacet* worst_facet_in_worst_chart();
        void init_one_proxy();
        void init_one_proxy_per_component();
        int init_one_proxy_from_facet(CFacet* f, int chart_id, int background_id = -1);
        int compute_nb_proxies();
        int get_uninitialized_charts();
        void add_uninitialized_charts(int min_size);
        CFacet* largest_uninitialized_chart();
        void get_proxies(bool get_uninit = false);      
        void get_seeds();
        void insert_neighbors(CFacet* seed, AddFacetToChartQueue& q);
        void flood_fill();
        void fill_holes();

    private:
        CComponent* map_;
		std::map<CFacet*,int>& chart_;
        std::vector<CL12LinearProxy> proxy_;
        std::vector<CFacet*> seed_;
        std::vector<int> chart_size_;

        double max_error_;
    };

}

