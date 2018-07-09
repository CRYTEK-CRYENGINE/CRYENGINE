// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef GENERICASTARSOLVER_H
#define GENERICASTARSOLVER_H

//GenericAStarSolver.h
//By Jeremy Gross

#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <float.h>

/////////////////////////////////////////////////////////////////////////////
//DISCLAIMER/////////////////////////////////////////////////////////////////
//This is not really THAT generic of an A*.  Is it missing your needed feature?
//	=>Likely!!
//Please feel encouraged as reader/user of this file to make it more generic.
//
//Only Requests:
//KEEP IT CLEAN!
//	(And make sure it stays backwards compatible for whoever is using)
//
//DISCLAIMER/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//Static polymorphic interface: GraphTraits//////////////////////////////////
template<
  typename YourGraph
  , typename YourReferencePoint
  , typename YourIndexType//	<=HIGHLY recommend this be int
  >
class GraphTraits
{
public:
	typedef YourReferencePoint ReferencePointType;
	typedef YourIndexType      IndexType;

	IndexType          GetNumNodes() const;

	IndexType          GetBestNodeIndex(ReferencePointType& referencePoint) const;
	ReferencePointType GetNodeReferencePoint(IndexType nodeIndex) const;

	int                GetNumLinks(IndexType nodeIndex) const;
	IndexType          GetNextNodeIndex(IndexType graphNodeIndex, int linkIndex) const;
};
//Static polymorphic interfaces: GraphTraits/////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//Static polymorphic interface: HeuristicTraits//////////////////////////////
template<typename GraphTraits, typename IndexType = typename GraphTraits::IndexType>
class HeuristicTraits
{
public:
	float H(const GraphTraits* graph, IndexType currentNodeIndex, IndexType endNodeIndex) const;
};
//Static polymorphic interface: HeuristicTraits//////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//Static polymorphic interface: LinkTraversalCostCalculatorTraits////////////
template<typename GraphTraits, typename IndexType = typename GraphTraits::IndexType>
class LinkTraversalCostCalculatorTraits
{
public:
	float GetCost(const GraphTraits* graph, IndexType currentNodeIndex, IndexType nextNodeIndex) const;
};
//Static polymorphic interface: LinkTraversalCostCalculatorTraits////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//AStar OpenList Node////////////////////////////////////////////////////////
template<typename IndexType = int>
struct AStarNode
{
	float                       G, F;
	const IndexType             graphNodeIndex;
	const AStarNode<IndexType>* parentNode;

	AStarNode(IndexType theGraphNodeIndex)
		: G(FLT_MAX)
		, F(FLT_MAX)
		, graphNodeIndex(theGraphNodeIndex)
		, parentNode(NULL)
	{}

	bool operator<(const AStarNode& other) const
	{
		return F < other.F;
	}

	bool operator>=(const AStarNode& other) const
	{
		return F >= other.F;
	}
};
//AStar OpenList Node////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//Optional OpenList Imp: List////////////////////////////////////////////////
template<typename IndexType = int>
class DefaultOpenList
{
	std::list<const AStarNode<IndexType>*> openList;

public:

	void Add(const AStarNode<IndexType>* node)
	{
		typename std::list<const AStarNode<IndexType>*>::iterator iter = openList.begin();
		while (iter != openList.end())
		{
			if (**iter >= *node)
			{
				break;
			}
			++iter;
		}
		openList.insert(iter, node);
	}

	const AStarNode<IndexType>* GetNext()
	{
		const AStarNode<IndexType>* node = openList.front();
		openList.pop_front();
		return node;
	}

	void Clear()
	{
		openList.clear();
	}

	bool IsEmpty() const
	{
		return openList.empty() != 0;
	}

	//For users debug benefit they may template specialize this function:
	void Print() const;
};
//Optional OpenList Imp: List////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//DefaultClosedList//////////////////////////////////////////////////////////
#define BITS_PER_BYTE 8
#define ROUND_TO_MIN_BYTES(theBits)        (((theBits) + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE)
#define BYTES_PER_INT sizeof(int)
#define ROUND_TO_MIN_INTS(theBits)         ((ROUND_TO_MIN_BYTES(theBits) + (BYTES_PER_INT - 1)) / BYTES_PER_INT)
#define TO_BIT_ARRAY_VALUE_SHIFT(theValue) ((theValue) & (BYTES_PER_INT * BITS_PER_BYTE - 1))
#define TO_BIT_ARRAY_INDEX(theValue)       ((theValue) >> (BYTES_PER_INT * BITS_PER_BYTE - 1))

template<typename IndexType>
class DefaultClosedList
{
	std::set<IndexType> closedList;
public:

	void Resize(IndexType size)
	{
		closedList.clear();
	}

	void Close(IndexType index)
	{
		closedList.insert(index);
	}

	bool IsClosed(IndexType index) const
	{
		return closedList.find(index) != closedList.end();
	}

	//For users debug benefit they may template specialize this function:
	void Print() const;
};

template<>
class DefaultClosedList<int>
{
	std::vector<int> BitArray;
	int              ArraySize;
public:

	void Resize(int size)
	{
		BitArray.clear();
		BitArray.resize(ROUND_TO_MIN_INTS(size), 0);
		ArraySize = size;
	}

	void Close(int index)
	{
		BitArray[TO_BIT_ARRAY_INDEX(index)] |= (1 << TO_BIT_ARRAY_VALUE_SHIFT(index));
	}

	bool IsClosed(int index) const
	{
		return (BitArray[TO_BIT_ARRAY_INDEX(index)] & (1 << TO_BIT_ARRAY_VALUE_SHIFT(index))) != 0;
	}

	void Print() const
	{
		printf("Closed List:\n");
		for (int i = 0; i < ArraySize; ++i)
		{
			if (IsClosed(i))
			{
				printf("1 ");
			}
			else
			{
				printf("0 ");
			}
		}
		printf("\n");
	}
};
//DefaultClosedList//////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//DefaultNodeContainer///////////////////////////////////////////////////////
template<typename IndexType = int>
class DefaultNodeContainer
{
	struct AStarNodeCompare
	{
		bool operator()(const AStarNode<IndexType>& LHS, const AStarNode<IndexType>& RHS) const
		{
			return LHS.graphNodeIndex < RHS.graphNodeIndex;
		}
	};

	std::set<AStarNode<IndexType>, AStarNodeCompare> aStarNodes;

public:

	AStarNode<IndexType>* GetAStarNode(IndexType nodeIndex)
	{
		std::pair<typename std::set<AStarNode<IndexType>, AStarNodeCompare>::iterator, bool> result =
		  aStarNodes.insert(AStarNode<IndexType>(nodeIndex));

		return const_cast<AStarNode<IndexType>*>(&*result.first);
	}

	void Clear()
	{
		aStarNodes.clear();
	}
};
//DefaultNodeContainer///////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//GenericAStarSolver/////////////////////////////////////////////////////////
template<
  typename GraphTraits
  , typename Heuristic = HeuristicTraits<GraphTraits>
  , typename LinkTraversalCostCalculator = LinkTraversalCostCalculatorTraits<GraphTraits>

  , typename OpenList = DefaultOpenList<typename GraphTraits::IndexType>
  , typename ClosedList = DefaultClosedList<typename GraphTraits::IndexType>
  , typename NodeContainer = DefaultNodeContainer<typename GraphTraits::IndexType>

  , typename ReferencePointType = typename GraphTraits::ReferencePointType
  , typename IndexType = typename GraphTraits::IndexType
  >
class GenericAStarSolver
{
	const GraphTraits*                 graph;
	const Heuristic*                   heuristic;
	const LinkTraversalCostCalculator* costCalculator;

	OpenList                           openList;
	ClosedList                         closedList;
	NodeContainer                      nodeContainer;

	std::vector<IndexType>             finalPathNodeIndices;
	IndexType                          goalNodeIndex;
	ReferencePointType                 PathEndPoint;

public:

	void StartPathFind(
	  const GraphTraits* theGraph
	  , const Heuristic* theHeuristic
	  , const LinkTraversalCostCalculator* theCostCalculator
	  , ReferencePointType theStart
	  , ReferencePointType theEnd)
	{
		graph = theGraph;
		heuristic = theHeuristic;
		costCalculator = theCostCalculator;

		closedList.Resize(graph->GetNumNodes());
		nodeContainer.Clear();
		openList.Clear();
		finalPathNodeIndices.clear();

		PathEndPoint = theEnd;

		IndexType theStartNodeIndex = theGraph->GetBestNodeIndex(theStart);
		goalNodeIndex = theGraph->GetBestNodeIndex(theEnd);

		const float G = 0;
		float F = G + heuristic->H(graph, theStartNodeIndex, goalNodeIndex);

		AStarNode<IndexType>* startNodePtr = nodeContainer.GetAStarNode(theStartNodeIndex);
		startNodePtr->G = G;
		startNodePtr->F = F;
		startNodePtr->parentNode = NULL;

		openList.Add(startNodePtr);
	}

	int Update(int theMaxNumNodesToProcess)
	{
		for (; 0 < theMaxNumNodesToProcess; --theMaxNumNodesToProcess)
		{
			if (openList.IsEmpty())
			{
				break;
			}

			const AStarNode<IndexType>* currentAStarNode = openList.GetNext();
			if (closedList.IsClosed(currentAStarNode->graphNodeIndex))
				continue;

			closedList.Close(currentAStarNode->graphNodeIndex);

			if (currentAStarNode->graphNodeIndex != goalNodeIndex)
			{
				int numLinks = graph->GetNumLinks(currentAStarNode->graphNodeIndex);
				for (int linkIndex = 0; linkIndex < numLinks; ++linkIndex)
				{
					IndexType nextNodeIndex = graph->GetNextNodeIndex(currentAStarNode->graphNodeIndex, linkIndex);
					if (closedList.IsClosed(nextNodeIndex))
					{
						//this next node is already visited, ignore it.
						continue;
					}

					float H = heuristic->H(graph, nextNodeIndex, goalNodeIndex);
					float G = currentAStarNode->G + costCalculator->GetCost(graph, currentAStarNode->graphNodeIndex, nextNodeIndex);
					float F = G + H;

					AStarNode<IndexType>* nextNodePtr = nodeContainer.GetAStarNode(nextNodeIndex);

					//only insert this if its cheaper than what was there already (or its not there)
					if (F < nextNodePtr->F)
					{
						nextNodePtr->F = F;
						nextNodePtr->G = G;
						nextNodePtr->parentNode = currentAStarNode;
						openList.Add(nextNodePtr);
					}
				}
			}
			else
			{
				//Goal found, save the path and quit:
				while (currentAStarNode != NULL)
				{
					finalPathNodeIndices.push_back(currentAStarNode->graphNodeIndex);
					currentAStarNode = currentAStarNode->parentNode;
				}
				break;
			}
		}
		return theMaxNumNodesToProcess;
	}

	void GetPath(std::vector<ReferencePointType>& pathReferencePoints)
	{
		pathReferencePoints.reserve(finalPathNodeIndices.size());
		for (typename std::vector<IndexType>::reverse_iterator iter = finalPathNodeIndices.rbegin()
		     ; iter != finalPathNodeIndices.rend()
		     ; ++iter)
		{
			IndexType index = *iter;
			pathReferencePoints.push_back(graph->GetNodeReferencePoint(index));
		}
		pathReferencePoints.push_back(PathEndPoint);
	}

	void GetPathReversed(std::vector<ReferencePointType>& pathReferencePoints) const
	{
		pathReferencePoints.reserve(finalPathNodeIndices.size());
		for (typename std::vector<IndexType>::const_iterator iter = finalPathNodeIndices.begin()
		     ; iter != finalPathNodeIndices.end()
		     ; ++iter)
		{
			IndexType index = *iter;
			pathReferencePoints.push_back(graph->GetNodeReferencePoint(index));
		}
		pathReferencePoints.push_back(PathEndPoint);
	}

	void GetPath(std::vector<IndexType>& pathNodeIndices)
	{
		pathNodeIndices.reserve(finalPathNodeIndices.size());
		for (typename std::vector<IndexType>::reverse_iterator iter = finalPathNodeIndices.rbegin()
		     ; iter != finalPathNodeIndices.rend()
		     ; ++iter)
		{
			pathNodeIndices.push_back(*iter);
		}
	}

	void GetPathReversed(std::vector<IndexType>& pathNodeIndices) const
	{
		pathNodeIndices.reserve(finalPathNodeIndices.size());
		for (typename std::vector<IndexType>::const_iterator iter = finalPathNodeIndices.begin()
		     ; iter != finalPathNodeIndices.end()
		     ; ++iter)
		{
			pathNodeIndices.push_back(*iter);
		}
	}

	//For users debug benefit
	const OpenList& GetOpenList()
	{
		return openList;
	}

	//For users debug benefit
	const ClosedList& GetClosedList()
	{
		return closedList;
	}

	void Reset()
	{
		stl::free_container(openList);
		stl::free_container(closedList);
		stl::free_container(nodeContainer);
		stl::free_container(finalPathNodeIndices);
	}
};
//GenericAStarSolver/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#endif //GENERICASTARSOLVER_H
