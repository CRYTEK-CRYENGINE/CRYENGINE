#pragma once

#include <vector>
#include <map>

namespace LODGenerator 
{
	class CComponent;
	class CVertex;
	class CHalfedge;
	class CFacet;
	class CSplitter 
	{
	public:
		CSplitter(CComponent* target = NULL);
		~CSplitter();

		CComponent* target() { return target_; }
		void set_target(CComponent* m) { target_ = m; }
		bool split_component(CComponent* component, std::vector<CComponent*>& charts);
		void set_error_decrease_factor(double x) { error_decrease_factor_ = x; }
		void set_max_components(int x) { max_components_ = x; }

	protected:
		int nb_charts(CComponent* component);
		void unglue_charts(CComponent* component);
		int chart(CHalfedge* h);
		void get_charts(CComponent* component, std::vector<CComponent*>& charts);

	protected:
		std::vector<CVertex*>& vertices();
		std::vector<CHalfedge*>& halfedges();
		std::vector<CFacet*>& facets();

	protected:
		double error_decrease_factor_;
		int max_components_;
		std::map<CVertex*,bool> is_locked_;
		bool smooth_;
	protected:
		std::map<CFacet*,int> chart_;

	private:
		CComponent* target_;
	};
};
