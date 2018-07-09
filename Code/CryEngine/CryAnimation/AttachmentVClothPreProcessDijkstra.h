// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include <iostream>
#include <list>
#include <utility>
#include <queue>
#include <functional>

//////////////////////////////////////////////////////////////////////////
/// @cond DEVELOPER
/** Usage example, see bottom of this file */
/// @endcond

//////////////////////////////////////////////////////////////////////////
//! Dijkstra's Algorithm, supporting single and multiple target nodes
/*! 
	Dijkstra's Algorithm
	Find the shortest path between source nodes in a graph and target nodes
	Template type argument needs std::greater<> specialization

	Input:  - List of edges and corresponding weights
			- Target NodeId or in case of multiple targets: list of Target NodeIds

	Output: For each node: 
			- Weight to target node
			- Weight to nearest neighbor along shortest path
			- NodeId of nearest neighbor in shortest path direction
*/
template<class T>
class GraphDijkstra
{

	typedef std::pair<uint32, T> tEdgeTo;

public:

	GraphDijkstra(
		uint32 nNodes,                                                     //!< Maximum number of nodes in graph (nodeIds must be within this range).
		T weightInit = T(0),                                               //!< Initial weight, e.g. for distances start with 0.0.
		uint32 targetNodeLinkId = std::numeric_limits<uint32>::max() - 1,  //!< NodeId-value, which is set as neighbor id for target nodes.
		uint32 islandNodeLinkId = std::numeric_limits<uint32>::max()       //!< NodeId-value, which is set as neighbor id for island nodes (i.e., no connection to any target node exists).
	);

	void AddEdge(uint32 nodeId0, uint32 nodeId1, T weight);                //!< Add edge to graph with given weight.
	void FindShortestPaths(uint32 targetNodeId);                           //!< Do dijkstra's algorithm with a single target
	void FindShortestPaths(std::vector<uint32> const& listTargetNodes);    //!< Do dijkstra's algorithm with multiple targets

	bool IsTargetNode(uint32 idx) const;  //!< True, if node is target node; false otherwise.
	bool IsIslandNode(uint32 idx) const;  //!< True, if node has connetion to any target node; false otherwise (island).

	struct ResultPerNode
	{
		uint32 nextNodeIdOnShortestPath;  //!< Neighbor node in shortest path direction.
		T weightTotal;                    //!< Weight to target node.
		T weightToNextNodeOnShortestPath; //!> Weight to closest neighbor on shortest path.

		ResultPerNode()
			: nextNodeIdOnShortestPath(std::numeric_limits<uint32>::max())
			, weightToNextNodeOnShortestPath(std::numeric_limits<T>::max())
			, weightTotal(std::numeric_limits<T>::max())
		{}
		ResultPerNode(uint32 nextNodeIdOnShortestPath, T weightToNextNodeOnShortestPath, T weightTotal)
			: nextNodeIdOnShortestPath(nextNodeIdOnShortestPath)
			, weightToNextNodeOnShortestPath(weightToNextNodeOnShortestPath)
			, weightTotal(weightTotal)
		{}
	};

	std::vector<ResultPerNode> const& Results() const { return m_results; }
	ResultPerNode const& Results(uint32 idx) const    { assert(idx < m_nNodes); return m_results[idx]; }

private:

	uint32 m_nNodes;                             //!< Number of nodes.
	std::vector< std::list< tEdgeTo > > m_edges; //!< Adjacent edges of each node.

	std::vector<ResultPerNode> m_results;        //!< Result data.

	T m_weightInit;                              //!< Initial value for weight (normally, i.e. 0, e.g., in case of distances).
	uint32 m_targetNodeLinkId;                   //!< Id which is set as neighbor id to target nodes.
	uint32 m_islandNodeLinkId;                   //!< Id which is set as neighbor id to target nodes.

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Implementation

template<class T>
GraphDijkstra<T>::GraphDijkstra(
	uint32 nNodes,
	T weightInit,
	uint32 targetNodeLinkId,
	uint32 islandNodeLinkId
	)
	: m_nNodes(nNodes)
	, m_weightInit(weightInit)
	, m_targetNodeLinkId(targetNodeLinkId)
	, m_islandNodeLinkId(islandNodeLinkId)
{
	// Create space for adjacency lists for all nodes within range [0,m_nNodes]
	m_edges.reserve(m_nNodes+1); // Reserve for one more element, just in case multiple targets are used later on (which would need one more additional bucket for the virtual node)
	m_edges.resize(m_nNodes);
}

template<class T>
void GraphDijkstra<T>::AddEdge(uint32 nodeId0, uint32 nodeId1, T weight)
{
	assert(nodeId0 < m_nNodes);
	assert(nodeId1 < m_nNodes);
	m_edges[nodeId0].push_back(std::make_pair(nodeId1, weight));
	m_edges[nodeId1].push_back(std::make_pair(nodeId0, weight));
}

template<class T>
void GraphDijkstra<T>::FindShortestPaths(uint32 targetNodeId)
{
	assert(targetNodeId < m_nNodes);

	// Priority queue to store indices; first: weight, second: nodeId - thus, sorting is ensured
	typedef std::pair<T, uint32> tPair;
	std::priority_queue< tPair, std::vector<tPair>, std::greater<tPair> > pq;

	// Init results to default
	m_results.assign(m_nNodes, ResultPerNode());

	// Insert source in priority queue and initialize targetNode
	pq.push(std::make_pair(m_weightInit, targetNodeId));
	m_results[targetNodeId] = ResultPerNode(m_targetNodeLinkId, m_weightInit, m_weightInit);

	// Loop through priority queue
	while (!pq.empty())
	{
		// Top item is minimum weight neighbor
		const int nodeId = pq.top().second;
		pq.pop();

		// Run through all adjacent nodes / i.e. all edges
		for (const auto& it : m_edges[nodeId])
		{
			const int nextNodeId = it.first;
			const T weight = it.second;

			auto& resultNext = m_results[nextNodeId];
			auto& resultActual = m_results[nodeId];

			// Check, if found weight/path is smaller
			if (resultNext.weightTotal > resultActual.weightTotal + weight)
			{
				// Update weight & add nearest neighbor on closest path
				resultNext.nextNodeIdOnShortestPath = nodeId;
				resultNext.weightToNextNodeOnShortestPath = weight;
				resultNext.weightTotal = resultActual.weightTotal + weight;

				// Add next node to queue
				pq.push(std::make_pair(resultNext.weightTotal, nextNodeId));
			}
		}
	}
}

template<class T>
void GraphDijkstra<T>::FindShortestPaths(std::vector<uint32> const& listTargetNodes)
{
	// Method to use multiple target nodes:
	// 1. Add one 'virtual' target node with weight 0, being connected to each target node
	// 2. Use this node for path finding using a basic single target dijkstra algorithm
	// 3. Thus, all nodes finally connected to the virtual target node are the sought-after target nodes

	// Allocate space for additional virtual node
	const int nNodesOriginal = m_nNodes;
	const int nNodesActual   = m_nNodes + 1;
	m_nNodes = nNodesActual;
	m_edges.resize(nNodesActual);

	// Add temporary/virtual node to allow several targets - using still a single target dijkstra
	// Each target is connected with weight 0 to the virtual node
	const int virtualTargetNodeId = GraphDijkstra<T>::m_nNodes - 1;
	for (const auto& it : listTargetNodes)
	{
		GraphDijkstra<T>::AddEdge(virtualTargetNodeId, it, 0);
	}

	// Do dijkstra for single target with target virtualTargetNode
	GraphDijkstra<T>::FindShortestPaths(virtualTargetNodeId);

	// Remove virtualTargetNodeId from data
	m_nNodes = nNodesOriginal;
	m_results.resize(nNodesOriginal);
	m_edges.resize(nNodesOriginal);

	// Set target node link id
	for (auto& it : GraphDijkstra<T>::m_results)
	{
		if (it.nextNodeIdOnShortestPath == virtualTargetNodeId) it.nextNodeIdOnShortestPath = m_targetNodeLinkId;
	}
}

template<class T>
bool GraphDijkstra<T>::IsTargetNode(uint32 idx) const
{
	assert(idx < GraphDijkstra<T>::m_nNodes);
	return GraphDijkstra<T>::Results(idx).nextNodeIdOnShortestPath == m_targetNodeLinkId;
}

template<class T>
bool GraphDijkstra<T>::IsIslandNode(uint32 idx) const
{
	assert(idx < GraphDijkstra<T>::m_nNodes);
	return GraphDijkstra<T>::Results(idx).nextNodeIdOnShortestPath == m_islandNodeLinkId;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Usage Example:

/*

typedef unsigned int uint32;

template<class T>
void OutputGraph(GraphDijkstra<T>& g)
{
	for (uint32 i = 0; i < g.Results().size(); ++i)
	{
		if (g.IsTargetNode(i))      std::cout << i << " -> isTarget\n";
		else if (g.IsIslandNode(i)) std::cout << i << " -> isIsland\n";
		else                        std::cout << i << " -> " << g.Results(i).nextNodeIdOnShortestPath << "\t(" << g.Results(i).weightToNextNodeOnShortestPath << ")\t" << g.Results(i).weightTotal << "\n";
	}
}

int main()
{
	// create graph
	const int nNodes = 11;
	GraphDijkstra<float> g(nNodes);

	g.AddEdge(0, 1, 4.2f); // test: two edges 0->1, different weight
	g.AddEdge(0, 1, 5.1f); // test: two edges 0->1, different weight
	g.AddEdge(0, 6, 8.1f);
	g.AddEdge(1, 3, 8.1f);
	g.AddEdge(1, 6, 11.1f);
	g.AddEdge(2, 4, 7.1f);
	g.AddEdge(2, 7, 2.1f);
	g.AddEdge(2, 6, 4.1f);
	g.AddEdge(3, 5, 9.1f);
	g.AddEdge(3, 4, 14.1f);
	g.AddEdge(4, 6, 10.1f);
	g.AddEdge(5, 7, 2.1f);
	g.AddEdge(6, 8, 1.1f);
	g.AddEdge(6, 5, 6.1f);
	g.AddEdge(7, 8, 7.1f);
	g.AddEdge(9, 10, 7.0f); // these two nodes are not connected to any other nodes

	bool bUseMultipleTargets = true;

	if (bUseMultipleTargets)
	{
		std::vector<uint32> listTargetIds;
		listTargetIds.push_back(2);
		listTargetIds.push_back(4);
		listTargetIds.push_back(8);
		g.FindShortestPaths(listTargetIds);
	}
	else
		g.FindShortestPaths(0);

	OutputGraph(g);
	return 0;
}

/* */

// OUTPUT:
//  0 -> 6  (8.1)   9.2
//  1 -> 6  (11.1)  12.2
//  2 -> isTarget
//  3 -> 5  (9.1)   13.3
//  4 -> isTarget
//  5 -> 7  (2.1)   4.2
//  6 -> 8  (1.1)   1.1
//  7 -> 2  (2.1)   2.1
//  8 -> isTarget
//  9 -> isIsland
//  10-> isIsland
