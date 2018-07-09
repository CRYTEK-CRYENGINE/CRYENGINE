#include "StdAfx.h"
#include "Heap.h"
#include "TopologyGraph.h"

namespace LODGenerator 
{
	CMapVertexHeap::CMapVertexHeap(CTopologyGraph* s,std::map<CVertex*,double>& cost) : CMapCellHeap<CVertex>(s,cost)
	{
		FOR_EACH_VERTEX(CTopologyGraph, map(), vi) 
		{
			pos_[*vi] = -1;
		}
	}  

	void CMapVertexHeap::init_with_all_surface_vertices() 
	{
		FOR_EACH_VERTEX(CTopologyGraph, map(), vi) 
		{
			push(*vi);
		}
	}
}

