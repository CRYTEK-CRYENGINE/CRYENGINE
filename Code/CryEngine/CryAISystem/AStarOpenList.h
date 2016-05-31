// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ASTAROPENLIST_H
#define ASTAROPENLIST_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include "GraphStructures.h"
#include <vector>
#include <set>
#include <algorithm>
#include <CryMemory/STLPoolAllocator.h>

// If this isn't on the OpenListMonitor isn't even mentioned in AStarOpenList.
//#define MONITOR_OPEN_LIST
/**
 * Useful for gathering info about how A* open list operates.
 *
 * Receives an event for every node that's pushed to the open list and every
 * node that popped from there.  Based on that it computes various statistics.
 *
 * TODO Mai 22, 2007: <pvl> no reasonable way of outputting the gathered stats
 * at the moment.  In fact, mainly intended to be used "manually" under
 * debugger ... :)
 */
class OpenListMonitor
{
	/// Holds any info we care to hold per every node currently on the open list.
	struct NodeInfo
	{
		CTimeValue m_timestamp;
		int        m_frame;
		NodeInfo(const CTimeValue& time, int frame) : m_timestamp(time), m_frame(frame)
		{}
	};
	/// Uses graph node index as a unique node ID and maps that to corresponding node info.
	typedef std::map<unsigned int, NodeInfo> NodeInfoMap;
	NodeInfoMap m_nodeInfoMap;

	/// The actual open list statistics.
	float sMin;
	float sMax;
	float sAvg;
	int   sNumSamples;
	float sMinFrames;
	float sMaxFrames;
	float sAvgFrames;
	/// For each frame that's leaving the open list, this is called with the time
	/// and a number of frames that the node spent on the open list.
	void UpdateStats(float t, int frames)
	{
		if (t < sMin) sMin = t;
		if (t > sMax) sMax = t;
		sAvg = (sAvg * sNumSamples + t) / (sNumSamples + 1);

		if (t < sMinFrames) sMinFrames = (float)frames;
		if (t > sMaxFrames) sMaxFrames = (float)frames;
		sAvgFrames = (sAvgFrames * sNumSamples + frames) / (sNumSamples + 1);

		++sNumSamples;
	}
public:
	OpenListMonitor()
		: sMin(std::numeric_limits<float>::max())
		, sMax(std::numeric_limits<float>::min())
		, sAvg(0.0f)
		, sNumSamples(0)
		, sMinFrames(std::numeric_limits<float>::max())
		, sMaxFrames(std::numeric_limits<float>::min())
		, sAvgFrames(0.0f)
	{}
	void NodePushed(unsigned int nodeIndex)
	{
		// NOTE Mai 22, 2007: <pvl> we could filter incoming nodes here if we're
		// only interested in stats for a certain node class
		/*
		    GraphNode* nodeptr = nodeManager.GetNode(node);
		    if (nodeptr->navType != IAISystem::NAV_WAYPOINT_HUMAN)
		      return;
		 */
		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		int currFrame = gEnv->pRenderer->GetFrameID();
		m_nodeInfoMap.insert(std::make_pair(nodeIndex, NodeInfo(now, currFrame)));
	}
	void NodePopped(unsigned int nodeIndex)
	{
		NodeInfoMap::iterator infoIt = m_nodeInfoMap.find(nodeIndex);
		if (infoIt == m_nodeInfoMap.end())
		{
			// NOTE Mai 22, 2007: <pvl> can happen if we filter nodes in NodePushed()
			return;
		}

		CTimeValue timeWhenPushed = infoIt->second.m_timestamp;
		int frameWhenPushed = infoIt->second.m_frame;

		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		int currFrame = gEnv->pRenderer->GetFrameID();

		float timeSpentInList = (now - timeWhenPushed).GetMilliSeconds();
		int framesSpentInList = currFrame - frameWhenPushed;
		UpdateStats(timeSpentInList, framesSpentInList);
	}
};

struct AStarSearchNode
{
	float            fCostFromStart;
	float            fEstimatedCostToGoal;
	AStarSearchNode* prevPathNodeIndex;
	GraphNode*       graphNode;
	uint32           nodeIndex : 31;
	bool             tagged    : 1;

	AStarSearchNode()
		: fCostFromStart(fInvalidCost)
		, fEstimatedCostToGoal(fInvalidCost)
		, prevPathNodeIndex(0)
		, graphNode(0)
		, nodeIndex(0)
		, tagged(false)
	{
	}

	bool IsTagged()
	{
		return tagged != false;
	}

	void Tag()
	{
		tagged = true;
	}

	// Used as a flag for costs that aren't calculated yet
	static const float fInvalidCost;
private:
	friend class CAStarNodeListManager;

	AStarSearchNode(uint32 originalNodeIndex)
		: fCostFromStart(fInvalidCost)
		, fEstimatedCostToGoal(fInvalidCost)
		, prevPathNodeIndex(0)
		, graphNode(0)
		, nodeIndex(originalNodeIndex)
		, tagged(false)
	{
	}
};

/// Helper class to sort the node lists
struct NodeCompareCost
{
	NodeCompareCost()
	{
	}

	bool operator()(const AStarSearchNode* node1, const AStarSearchNode* node2) const
	{
		return ((node1->fCostFromStart + node1->fEstimatedCostToGoal) > (node2->fCostFromStart + node2->fEstimatedCostToGoal));
	}
};

// Helper class to sort the node lists
struct NodeCompareIndex
{
	bool operator()(const AStarSearchNode& node1, const AStarSearchNode& node2) const
	{
		return node1.nodeIndex < node2.nodeIndex;
	}
};

typedef std::vector<AStarSearchNode*> AStarSearchNodeVector;

//====================================================================
// CAStarNodeListManager
//====================================================================
class CAStarNodeListManager
{
public:
	CAStarNodeListManager(CGraphNodeManager& nodeManager)
		: nodeManager(nodeManager)
	{
	}

	/// Gets the best node and removes it from the list. Returns 0 if
	/// the list is empty
	AStarSearchNode* PopBestNodeFromOpenList();

	/// Adds a node to the list (shouldn't already be in the list)
	void AddNodeToOpenList(AStarSearchNode*);

	/// If the node is in the list then its position in the list gets updated.
	/// If not the list isn't changed. Either way the node itself gets
	/// modified
	void UpdateNode(AStarSearchNode* node, float newCost, float newEstimatedCost);

	/// Indicates if the list is empty
	bool IsEmpty() const;

	/// Empties the list
	void Clear(bool freeMemory = false);

	// returns memory usage in bytes
	size_t MemStats();

	/// Reserves memory based on an estimated max list size
	void                         ReserveMemory(size_t estimatedMaxSize);

	const AStarSearchNodeVector& GetTaggedNodesVector()
	{
		return taggedNodesVector;
	}

	AStarSearchNode* GetAStarNode(uint32 index)
	{
		std::pair<AStarSearchNodes::iterator, bool> it = currentNodes.insert(AStarSearchNode(index));
		if (it.second)
		{
			AStarSearchNode& node = const_cast<AStarSearchNode&>(*it.first);
			node.graphNode = nodeManager.GetNode(index);
		}

		return const_cast<AStarSearchNode*>(&(*it.first));
	}

private:
	typedef stl::STLPoolAllocator<AStarSearchNode, stl::PSyncNone>         OpenListAllocator;
	typedef std::set<AStarSearchNode, NodeCompareIndex, OpenListAllocator> AStarSearchNodes;
	AStarSearchNodes currentNodes;

	/// the open list
	AStarSearchNodeVector openList;
	CGraphNodeManager&    nodeManager;
	AStarSearchNodeVector taggedNodesVector;

#ifdef MONITOR_OPEN_LIST
	OpenListMonitor m_monitor;
#endif // MONITOR_OPEN_LIST
};

//====================================================================
// Don't look below here - inline implementation
//====================================================================

//====================================================================
// ReserveMemory
//====================================================================
inline void CAStarNodeListManager::ReserveMemory(size_t estimatedMaxSize)
{
}

//====================================================================
// MemStats
//====================================================================
inline size_t CAStarNodeListManager::MemStats()
{
	return openList.capacity() * sizeof(unsigned);
}

//====================================================================
// IsEmpty
//====================================================================
inline bool CAStarNodeListManager::IsEmpty() const
{
	return openList.empty();
}

//====================================================================
// PopBestNode
//====================================================================
inline AStarSearchNode* CAStarNodeListManager::PopBestNodeFromOpenList()
{
	if (IsEmpty())
		return 0;

	AStarSearchNode* node = openList.front();
	// This "forgets about" the last node, and (partially) sorts the rest
	std::pop_heap(openList.begin(), openList.end(), NodeCompareCost());
	// remove the redundant element
	openList.pop_back();

#ifdef MONITOR_OPEN_LIST
	m_monitor.NodePopped(node->nodeIndex);
#endif // MONITOR_OPEN_LIST

	return node;
}

//====================================================================
// AddNode
//====================================================================
inline void CAStarNodeListManager::AddNodeToOpenList(AStarSearchNode* node)
{
	node->Tag();
	taggedNodesVector.push_back(node);

	openList.push_back(node);
	std::push_heap(openList.begin(), openList.end(), NodeCompareCost());

#ifdef MONITOR_OPEN_LIST
	m_monitor.NodePushed(node->nodeIndex);
#endif // MONITOR_OPEN_LIST
}

//====================================================================
// UpdateNode
//====================================================================
inline void CAStarNodeListManager::UpdateNode(AStarSearchNode* node, float newCost, float newEstimatedCost)
{
	const AStarSearchNodeVector::const_iterator end = openList.end();

	for (AStarSearchNodeVector::iterator it = openList.begin(); it != end; ++it)
	{
		if (*it == node)
		{
			node->fCostFromStart = newCost;
			node->fEstimatedCostToGoal = newEstimatedCost;
			std::push_heap(openList.begin(), it + 1, NodeCompareCost());

			return;
		}
	}

	node->fCostFromStart = newCost;
	node->fEstimatedCostToGoal = newEstimatedCost;
}

//====================================================================
// Clear
//====================================================================
inline void CAStarNodeListManager::Clear(bool freeMemory /* = false*/)
{
	if (freeMemory)
	{
		stl::free_container(openList);
		currentNodes.clear();
		stl::free_container(taggedNodesVector);
	}
	else
	{
		openList.resize(0);
		currentNodes.clear();
		taggedNodesVector.resize(0);
	}
}

#endif
