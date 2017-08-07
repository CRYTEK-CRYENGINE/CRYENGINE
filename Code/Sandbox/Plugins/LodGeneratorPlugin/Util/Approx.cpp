#include "StdAfx.h"
#include "Approx.h"
#include "Splitter.h"
#include "Component.h"
#include "MathTool.h"

namespace LODGenerator 
{

    double CL12LinearProxy::compactness_importance_ = 0.0 ;
    double CL12LinearProxy::angular_threshold_ = 1.0 ;
    bool CL12LinearProxy::n_mode_ = false ;

	Vec3d facet_barycenter(const CFacet* f)
	{
		double x=0 ; double y=0 ; double z=0 ;
		int nb_vertex = 0 ;
		CHalfedge* cir = f->halfedge();
		do {
			nb_vertex++;
			x+= cir->vertex()->point().x ;
			y+= cir->vertex()->point().y ;
			z+= cir->vertex()->point().z ;
			cir = cir->next();
		} while (cir != f->halfedge());
		return  Vec3d(
			x / double(nb_vertex),
			y / double(nb_vertex),
			z / double(nb_vertex)
			) ;
	}

	CL12LinearProxy::CL12LinearProxy() : N_(0,0,0), G_(0,0,0) 
	{

	}
	CL12LinearProxy::CL12LinearProxy(const Vec3d& v, const Vec3d& p) : N_(v), G_(p) 
	{

	}
	CL12LinearProxy::CL12LinearProxy(CFacet* f) : N_(f->facet_normal()), G_(facet_barycenter(f)) 
	{

	}

	double CL12LinearProxy::distance_3(CFacet* f, double area) const 
	{

		CVertex* v[3] ;
		v[0]=f->halfedge()->vertex() ;
		v[1]=f->halfedge()->next()->vertex() ;
		v[2]=f->halfedge()->next()->next()->vertex() ;
		double result = 0 ;
		double d[3] ;
		for(int i=0; i<3; i++) {
			Vec3d V = v[i]->point() - G_ ;
			V = V - V.Dot(N_) * N_ ;
			d[i] = V.len2() ;
			result += d[i] ;
			d[i] = ::sqrt(d[i]) ;
		}
		result += d[0] * d[1] ;
		result += d[1] * d[2] ;
		result += d[2] * d[0] ;
		result /= 6.0 ;
		result *= area ;
		return result ;
	}

	double CL12LinearProxy::deviation(CFacet* f) const 
	{
		double a = f->facet_area() ;
		Vec3d n = f->facet_normal() ;
		double result =  a * (n - N_).len2() ; 
		if(compactness_importance_ != 0.0) {
			result += compactness_importance_ * distance_3(f,a) ;
		}
		return result ;
	}

	double CL12LinearProxy::angle_deviation(CFacet* f) const 
	{
		Vec3d n = f->facet_normal() ;
		return (n.Cross(N_)).len() ;
	}

	void CL12LinearProxyFitter::begin() 
	{ 
		N_ = Vec3d(0,0,0) ; 
		G_ = Vec3d(0,0,0) ; 
		A_ = 0.0 ; 
	}
	void CL12LinearProxyFitter::end() 
	{ 
		N_.Normalize() ; 
		if(CL12LinearProxy::compactness_importance() != 0.0 && A_ != 0.0) {
			G_.x = G_.x / A_ ;
			G_.y = G_.y / A_ ;
			G_.z = G_.z / A_ ;
		}
	}
	CL12LinearProxy CL12LinearProxyFitter::proxy() 
	{ 
		return CL12LinearProxy(N_, G_) ; 
	}
	void CL12LinearProxyFitter::add_facet(CFacet* f) 
	{
		double a = f->facet_area() ;
		N_ = N_ + a * f->facet_normal() ;
		if(CL12LinearProxy::compactness_importance() != 0.0) 
		{
			Vec3d g = facet_barycenter(f) ;
			A_ += a ;
			G_.x = G_.x + a*g.x ;
			G_.y = G_.y + a*g.y ;
			G_.z = G_.z + a*g.z ;
		}
	}

	CApprox::CApprox(CComponent* map,std::map<CFacet*,int>& chart) : map_(map),chart_(chart) ,max_error_(-1) 
	{
	}

	void CApprox::init(int nb_proxies, int nb_iter, double min_err, int min_nb_proxies) 
	{
		init_one_proxy_per_component();
		for(int i=proxy_.size(); i<=nb_proxies; i++) 
		{
			double err = optimize(nb_iter);
			if(err < min_err && int(proxy_.size()) >= min_nb_proxies) 
				return;
			if(int(proxy_.size()) >= int(map_->size_of_facets())) 
				return;
			CFacet* f = new_chart();
			chart_[f] = proxy_.size();
		}
		if(CL12LinearProxy::n_mode())
			fill_holes();
	}

	double CApprox::optimize(int nb_iter) 
	{
		double result = 0;
		nb_iter = max(1, nb_iter);
		for(int i = 0; i<nb_iter; i++) 
		{
			bool get_uninit = (i == nb_iter / 2);
			get_proxies(get_uninit);
			get_seeds();
			flood_fill();
			result = error();
			CryLog("Partition error = %f",result);
		}
		return result;
	}


	void CApprox::add_charts(int nb_charts, int nb_iter, double min_err, int min_nb_charts) 
	{
		for(int i=0; i<nb_charts; i++) 
		{
			get_proxies();
			if(int(proxy_.size()) == int(map_->size_of_facets())) 
			{
				CryLog("Partition All facets are separated");
				break;
			}
			CFacet* f = new_chart();
			chart_[f] = proxy_.size();
			double err = optimize(nb_iter);
			if(err < min_err && int(proxy_.size()) >= min_nb_charts) 
				return;
			CryLog("Partition error = %f",err);
		}
		if(CL12LinearProxy::n_mode()) 
		{
			fill_holes();
		}
	}

	double CApprox::error() 
	{
		double result = 0.0;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(chart != -1)
				result += proxy_[chart].deviation(*it);
		}
		result /= map_->map_area();
		return result;
	}


	CFacet* CApprox::new_chart() 
	{
		CFacet* result = NULL;
		result = largest_uninitialized_chart();
		if(result == NULL) 
		{
			result = worst_facet_in_worst_chart();
		}
		return result;
	}

	void CApprox::compute_chart_sizes() 
	{
		chart_size_.clear();
		int nb_proxies = 0;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			nb_proxies = max(chart_[*it], nb_proxies);
		}
		nb_proxies++;
		for(int i=0; i<nb_proxies; i++) 
			chart_size_.push_back(0);

		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(chart >= 0) 
				chart_size_[chart]++;
		}
	}

	void CApprox::check_facets() 
	{
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			assert(*it != NULL);
		}
	}

	CFacet* CApprox::worst_facet() 
	{
		compute_chart_sizes();
		double e = -1.0;
		CFacet* result = NULL;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(chart >= 0 && chart_size_[chart] > 1) 
			{
				assert(chart < int(proxy_.size()));
				double cur_e = proxy_[chart].deviation(*it);
				if(cur_e > e) 
				{
					e = cur_e;
					result = *it;
				}
			}
		}
		assert(result != NULL);
		return result;
	}

	int CApprox::worst_chart() 
	{
		compute_chart_sizes();
		std::vector<double> E;
		for(unsigned int i=0; i<proxy_.size(); i++) 
			E.push_back(0);

		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(!CL12LinearProxy::n_mode()) 
			{

				assert(chart >= 0 && chart<= int(proxy_.size()) - 1);
			}
			if(chart >= 0) {
				E[chart] += proxy_[chart].deviation(*it);
			}
		}
		double worst_e = -1.0;
		int result = -1;
		for(unsigned int i=0; i<proxy_.size(); i++) 
		{
			if(E[i] > worst_e && chart_size_[i] > 1) 
			{
				worst_e = E[i];
				result = i;
			}
		}
		assert(result != -1);
		return result;
	}

	CFacet* CApprox::worst_facet_in_worst_chart() 
	{
		int proxy = worst_chart();
		double e = -1.0;
		CFacet* result = NULL;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(chart == proxy) 
			{
				double cur_e = proxy_[chart].deviation(*it);
				if(cur_e > e) 
				{
					e = cur_e;
					result = *it;
				}
			}
		}
		assert(result != NULL);
		return result;
	}

	void CApprox::init_one_proxy() 
	{
		CFacet* f = *(map_->facets_begin());
		proxy_.clear();
		seed_.clear();
		proxy_.push_back(CL12LinearProxy(f));
		seed_.push_back(f);
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			chart_[*it] = 0;
		}
	}

	void CApprox::init_one_proxy_per_component() 
	{
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			chart_[*it] = -1;
		}
		proxy_.clear();
		seed_.clear();
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			if(chart_[*it] == -1) 
			{
				proxy_.push_back(CL12LinearProxy(*it));
				seed_.push_back(*it);
				init_one_proxy_from_facet(*it, proxy_.size() - 1);
			}
		}
	}

	/** returns the number of facets in the chart */
	int CApprox::init_one_proxy_from_facet(CFacet* f, int chart_id, int background_id) 
	{
		int result = 0;
		std::stack<CFacet*> S;
		S.push(f);
		while(!S.empty()) 
		{
			CFacet* cur = S.top();
			S.pop();
			if(chart_[cur] != chart_id) 
			{
				chart_[cur] = chart_id;
				result++;
			}
			CHalfedge* h = cur->halfedge();
			do 
			{
				CFacet* neigh = h->opposite()->facet();
				if(neigh != NULL && chart_[neigh] == background_id) 
				{
					S.push(neigh);
				}
				h = h->next();
			} 
			while(h != cur->halfedge());
		}
		return result;
	}

	int CApprox::compute_nb_proxies() 
	{
		int nb_proxies = 0;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			nb_proxies = max(nb_proxies, chart_[*it]);
		}
		nb_proxies++;
		return nb_proxies;
	}

	/** returns the total numer of charts */
	int CApprox::get_uninitialized_charts() 
	{
		int nb_proxies = compute_nb_proxies();
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			if(chart_[*it] < 0) 
			{
				init_one_proxy_from_facet(*it, nb_proxies);
				nb_proxies++;
			}
		}
		return nb_proxies;
	}

	void CApprox::add_uninitialized_charts(int min_size) 
	{
		std::vector<int> remap;
		remap.push_back(0);
		remap.push_back(0);
		int cur_remap = compute_nb_proxies();
		int cur_id = -2;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			if(chart_[*it] == -1) 
			{
				int cur_size = init_one_proxy_from_facet(*it, cur_id, chart_[*it]);
				cur_id--;
				if(cur_size > min_size) 
				{
					remap.push_back(cur_remap); cur_remap++;
				} 
				else 
				{
					remap.push_back(-1);
				}
			}
		}
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			if(chart_[*it] < 0) 
			{
				chart_[*it] = remap[-chart_[*it]];
			}
		}
	}

	CFacet* CApprox::largest_uninitialized_chart() 
	{
		int cur_id = -2;
		int max_size = -1;
		CFacet* result = NULL;
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			if(chart_[*it] == -1) 
			{
				int cur_size = init_one_proxy_from_facet(*it, cur_id, chart_[*it]);
				cur_id--;
				if(cur_size > max_size) 
				{
					max_size = cur_size;
					result = *it;
				}
			}
		}
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			if(chart_[*it] < 0) 
			{
				chart_[*it] = -1;
			}
		}
		return (max_size > 100) ? result : NULL;
	}

	void CApprox::get_proxies(bool get_uninit)
	{
		std::vector<CL12LinearProxyFitter> fitter;
		if(get_uninit) 
		{
			add_uninitialized_charts(100);
		}
		int nb_proxies = compute_nb_proxies();
		proxy_.clear();
		for(int i=0; i<nb_proxies; i++) 
		{
			proxy_.push_back(CL12LinearProxy());
			fitter.push_back(CL12LinearProxyFitter());
			fitter[i].begin();
		}
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(chart >= 0) 
			{
				assert(chart < int(proxy_.size()));
				fitter[chart].add_facet(*it);
			} 
			else 
			{
				//                    std::cerr << "get_proxies(): NUL proxy" << std::endl;
			}
		}
		for(int i=0; i<nb_proxies; i++) 
		{
			assert(i < int(proxy_.size()));
			fitter[i].end();
			proxy_[i] = fitter[i].proxy();
		}
	}

	void CApprox::get_seeds() 
	{
		seed_.clear();
		std::vector<double> E;
		for(unsigned int i=0; i<proxy_.size(); i++) 
		{
			E.push_back(big_double);
			seed_.push_back(NULL);
		}
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			int chart = chart_[*it];
			if(chart >= 0) 
			{
				double d = proxy_[chart].deviation(*it);
				if(d < E[chart]) 
				{
					E[chart] = d;
					seed_[chart] = *it;
				}
			} 
			else 
			{
				//                    std::cerr << "get_seeds(): NUL proxy" << std::endl;
			}
		}
		for(unsigned int i=0; i<seed_.size(); i++) 
		{
			if(seed_[i] == NULL) 
			{
				//                    std::cerr << "NULL seed for chart id" << i << std::endl;
			}
		}
	}

	void CApprox::insert_neighbors(CFacet* seed, AddFacetToChartQueue& q) 
	{
		CHalfedge* h = seed->halfedge();
		do 
		{
			CFacet* f = h->opposite()->facet();
			if(f != NULL && chart_[f] == -1) 
			{
				CHalfedge* hh = f->halfedge();
				do 
				{
					CFacet* ff = hh->opposite()->facet();
					if(ff != NULL) 
					{
						int chart = chart_[ff];
						if(chart != -1) 
						{
							assert(chart >=0 && chart <= int(proxy_.size()) - 1);
							q.push(SAddFacetToChart(f, chart, proxy_[chart].deviation(f)));
						}
					}
					hh = hh->next();
				}
				while(hh != f->halfedge());
			}
			h = h->next();
		}
		while(h != seed->halfedge());
	}


	void CApprox::flood_fill() 
	{
		FOR_EACH_FACET(CComponent, map_, it) 
		{
			chart_[*it] = -1;
		}

		for(unsigned int i=0; i<proxy_.size(); i++) 
		{
			assert(i < seed_.size());
			if(seed_[i] != NULL) 
			{
				chart_[seed_[i]] = i;
			}
		}

		AddFacetToChartQueue q;
		for(unsigned int i=0; i<proxy_.size(); i++) 
		{
			assert(i < seed_.size());
			if(seed_[i] != NULL) 
			{
				insert_neighbors(seed_[i], q);
			}
		}

		while (!q.empty()) 
		{
			SAddFacetToChart op = q.top();
			q.pop();
			if(chart_[op.facet] == -1) 
			{
				assert(op.chart < int(proxy_.size()));
				if(
					!CL12LinearProxy::n_mode() || 
					(proxy_[op.chart].angle_deviation(op.facet) < CL12LinearProxy::angular_threshold())
					) 
				{
					chart_[op.facet] = op.chart;
					insert_neighbors(op.facet, q);
				}
			}
		}
	}

	void CApprox::fill_holes() 
	{
		std::stack<CHalfedge*> S;
		FOR_EACH_HALFEDGE(CComponent, map_, it) 
		{
			CFacet* f1 = (*it)->facet();
			CFacet* f2 = (*it)->opposite()->facet();
			if(f1 != NULL && f2 != NULL && chart_[f1] >= 0 && chart_[f2] < 0) 
			{
				S.push(*it);
			}
		}
		while(!S.empty()) 
		{
			CHalfedge* h = S.top();
			S.pop();
			CFacet* f1 = h->facet();
			CFacet* f2 = h->opposite()->facet();
			if(chart_[f2] < 0) {
				chart_[f2] = chart_[f1];
				CHalfedge* hh = f2->halfedge();
				do 
				{
					CFacet* f3 = hh->opposite()->facet();
					if(f3 != NULL && chart_[f3] < 0) 
					{
						S.push(hh);
					}
					hh = hh->next();
				} 
				while(hh != f2->halfedge());
			}
		}
	}
}
