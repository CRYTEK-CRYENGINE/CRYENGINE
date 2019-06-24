#pragma once

#include <map>
namespace LODGenerator 
{
	class CTopologyGraph;
	class CVertex;
	template <class CELL> class CMapCombelObserver 
	{
	public:
		CMapCombelObserver(CTopologyGraph* m) : map_(m) { }
		virtual ~CMapCombelObserver() { }
		virtual void add(CELL* c) { }
		virtual void remove(CELL* c) { }

	protected:
		CTopologyGraph* map() { return map_; }

	private:
		CTopologyGraph* map_ ;
	} ;

	template<class CELL>
	class CMapCellHeap : public CMapCombelObserver<CELL> 
	{
	public :

		CMapCellHeap(CTopologyGraph *s,std::map<CELL*,double> &cost ) : CMapCombelObserver<CELL>(s) ,cost_(cost)
		{
		} 

		~CMapCellHeap() { }    

		CELL* pop(){
			assert(!heap_.empty()) ;
			CELL* result = heap_[0];
			remove(heap_[0]) ; 
			return result;
		}


		bool empty()      { return heap_.empty();  }
		unsigned int size() { return heap_.size(); }

		void update_cost(CELL *v) {
			assert(contains(v));
			place_up_in_heap(v);
			place_down_in_heap(v);
		}

		bool contains(CELL *v) { 
			return (v != NULL && pos_[v] >= 0 && pos_[v] < int(heap_.size()) && heap_[pos_[v]] == v) ; 
		}

		void push(CELL* v) {
			assert(!contains(v)) ;
			pos_[v] = int(heap_.size());
			heap_.push_back(v);
			place_up_in_heap(v);
		}

		virtual void add(CELL* v) {  }

		virtual void remove(CELL* v) {
			if(contains(v)) {
				int pos = pos_[v]; 
				if (pos != int(heap_.size())-1) {
					switch_elements(pos,int(heap_.size())-1);
				}
				heap_.pop_back();
				if (pos < int(heap_.size())) {
					update_cost(heap_[pos]);
				}
			}
		}

	private:    
		void switch_elements(int i,int j) {
			CELL* tmp;
			tmp = heap_[i];
			heap_[i] =  heap_[j];
			heap_[j] =  tmp;
			pos_[heap_[i] ]=i;
			pos_[heap_[j] ]=j;
		}

		inline long int father(long int i) const {
			return  (i+1)/2 - 1 ;
		}

		inline long int child0(long int i) const {
			return 2*(i+1)-1;
		}

		inline long int child1(long int i) const {
			return 2*(i+1)+1-1;
		}

		void place_up_in_heap(CELL *v) {
			if (pos_[v]!=0 && cost_[v] < cost_[heap_[father(pos_[v])] ]) {
				switch_elements(pos_[v],father(pos_[v]));
				place_up_in_heap(v);
			}
		}

		void place_down_in_heap(CELL *v) {
			assert(pos_[v] < int(heap_.size()));
			assert(pos_[v] >=0 );
			if (
				(child0(pos_[v]) < int(heap_.size())
				&& cost_[heap_[child0(pos_[v])]] < cost_[v])
				|| 
				(child1(pos_[v]) < int(heap_.size())
				&& cost_[heap_[child1(pos_[v])]] < cost_[v])
				) {
					if (child1(pos_[v]) == int(heap_.size())
						|| 
						cost_[heap_[child0(pos_[v])]] < 
						cost_[heap_[child1(pos_[v])]]
					) {
						switch_elements(pos_[v],child0(pos_[v]));
						place_down_in_heap(v);
					} else {
						switch_elements(pos_[v],child1(pos_[v]));
						place_down_in_heap(v);
					}
			}
		}

	private:

		CMapCellHeap(const CMapCellHeap& rhs) ;
		CMapCellHeap& operator=(const CMapCellHeap& rhs) ;

	protected :
		std::map<CELL*,int> pos_;
		std::map<CELL*,double>& cost_;
		std::vector<CELL*> heap_;
	} ;

	class CMapVertexHeap : public CMapCellHeap<CVertex> 
	{
	public :
		CMapVertexHeap(CTopologyGraph* s,std::map<CVertex*,double>& cost);
		void init_with_all_surface_vertices();
	} ;
}

