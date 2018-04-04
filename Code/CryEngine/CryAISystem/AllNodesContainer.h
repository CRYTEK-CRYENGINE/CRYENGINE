// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef ALLNODESCONTAINER_H
#define ALLNODESCONTAINER_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include "AILog.h"
#include "HashSpace.h"
#include <CryCore/Containers/VectorSet.h>
#include "GraphNodeManager.h"

#include <vector>
#include <algorithm>

/// Container for pointers to nodes so that it's quick/convenient to:
/// 1. Insert nodes of any type
/// 2. remove nodes of any type
/// 3. Iterate over all nodes
/// 4. Iterate over all nodes of a particular type
/// 5. Find a node (e.g. for validation)
class CAllNodesContainer
{
public:
	/// Node that this iterator becomes invalid as soon as the container
	/// gets modified
	class Iterator
	{
	public:
		/// Create an iterator, starting at the beginning
		Iterator(const CAllNodesContainer& container, IAISystem::tNavCapMask navTypeMask = IAISystem::NAVMASK_ALL);
		~Iterator();
		/// increment and return the current value
		unsigned Increment();
		/// just return the current value
		unsigned GetNode() const;
		/// Set to the beginning. Note that this can get called by the container if it is modified
		/// in such a way that we might be invalid.
		void Reset();
	private:
		friend class CAllNodesContainer;
		/// Called by the container destructor
		void ContainerDeleted() { m_container = 0; }
		IAISystem::tNavCapMask                            m_navTypeMask;
		/// Current iterator over the containers types
		std::map<unsigned, VectorSet<unsigned>>::iterator m_currentAllNodesIt;
		/// our current iterator over the container's nodes of our current type
		VectorSet<unsigned>::iterator                     m_it;
		/// end iterator for the container's nodes of our current type
		VectorSet<unsigned>::iterator                     m_currentEndIt;
		/// The container we're associated with - always non-zero unless the container got deleted
		CAllNodesContainer*                               m_container;
	};

	struct SGraphNodeRecord
	{
		SGraphNodeRecord(unsigned nodeIndex) : nodeIndex(nodeIndex) {}
		const Vec3& GetPos(CGraphNodeManager& nodeManager) const  { return nodeManager.GetNode(nodeIndex)->GetPos(); }
		bool        operator==(const SGraphNodeRecord& rhs) const { return rhs.nodeIndex == nodeIndex; }
		unsigned    nodeIndex;
	};

	class GraphNodeHashSpaceTraits
	{
	public:
		GraphNodeHashSpaceTraits(CGraphNodeManager& nodeManager) : nodeManager(nodeManager) {}

		const Vec3& operator()(const SGraphNodeRecord& item) const { return item.GetPos(nodeManager); }

		CGraphNodeManager& nodeManager;
	};

	CAllNodesContainer(CGraphNodeManager& nodeManager);

	~CAllNodesContainer();

	/// Add a node to this container - uses the position to store it in a spatial structure
	void AddNode(unsigned nodeIndex);

	void Reserve(IAISystem::ENavigationType type, size_t size);

	/// Remove a node from this container (invalidates all iterators)
	void RemoveNode(unsigned nodeIndex);

	/// Indicates if the node is in this container
	bool DoesNodeExist(unsigned nodeIndex) const;

	/// Returns unsorted all nodes of a particular type within range of the position passed in, together
	/// with the squared distance to the node (unsorted)
	void GetAllNodesWithinRange(std::vector<std::pair<float, unsigned>>& nodesOut, const Vec3& pos, float range, IAISystem::tNavCapMask navTypeMask) const;

	/// Returns one node within range of the position passed in together with the squared distance
	/// NOTE: it just returns the first node it finds in range, not the closest one
	bool GetNodeWithinRange(std::pair<float, unsigned>& nodeOut, const Vec3& pos, float range, IAISystem::tNavCapMask navTypeMask, CGraph* graph) const;

	/// Returns the hash space for other queries
	const CHashSpace<SGraphNodeRecord, GraphNodeHashSpaceTraits>& GetHashSpace() const { return m_hashSpace; }

	/// Remove the iterator passed in and get an iterator representing the next
	/// node
	//Iterator Erase(const Iterator& it);

	/// Returns the memory usage in bytes
	size_t MemStats() const;

	/// For debugging - finds out if our HashSpace is consistent within itself and with AllNodesContainer.
	bool ValidateHashSpace() const;

	/// Compact the memory usage
	void Compact();

private:
	friend class Iterator;

	/// Called by the Iterator ctor
	void AttachIterator(Iterator* pIt) { m_attachedIterators.insert(pIt); }
	/// Called by the Iterator dtor
	void DetachIterator(Iterator* pIt) { m_attachedIterators.erase(pIt); }

	void ResetIterators(IAISystem::tNavCapMask m_navTypeMask);

	/// A set of nodes that are all of the same type
	/// [MichaelS 29/1/2007] Changed to use VectorSet to save memory. This has slower insertion
	/// time, but faster lookup time. Since insertion only happens at load-time, this should be
	/// fast enough, and save quite a lot of memory.
	typedef VectorSet<unsigned> tNodes;

	/// All nodes split into sets all containing the same type
	typedef std::map<unsigned, tNodes> tAllNodes;
	tAllNodes m_allNodes;

	/// Our spatial structure
	CHashSpace<SGraphNodeRecord, GraphNodeHashSpaceTraits> m_hashSpace;

	struct SNodeCollector
	{
		SNodeCollector(CGraphNodeManager& nodeManager, std::vector<std::pair<float, unsigned>>& nodes, IAISystem::tNavCapMask navTypeMask)
			: nodeManager(nodeManager), nodes(nodes), navTypeMask(navTypeMask) {}
		void operator()(const SGraphNodeRecord& record, float distSq)
		{
			GraphNode* pNode = nodeManager.GetNode(record.nodeIndex);
			if (pNode->navType & navTypeMask)
				nodes.push_back(std::make_pair(distSq, record.nodeIndex));
		}

		CGraphNodeManager&                       nodeManager;
		std::vector<std::pair<float, unsigned>>& nodes;
		IAISystem::tNavCapMask                   navTypeMask;
	};

	// as the name implies this collector just accepts one element, then returns true
	struct SOneNodeCollector
	{
		SOneNodeCollector(CGraphNodeManager& nodeManager, CGraph* graph, const Vec3& pos, std::pair<float, unsigned>& inNode, IAISystem::tNavCapMask navTypeMask)
			: nodeManager(nodeManager), m_Graph(graph), m_Pos(pos), node(inNode), navTypeMask(navTypeMask) {}

		bool operator()(const SGraphNodeRecord& record, float distSq);

		CGraphNodeManager&          nodeManager;
		std::pair<float, unsigned>& node;
		IAISystem::tNavCapMask      navTypeMask;

		CGraph*                     m_Graph;
		Vec3                        m_Pos;
	};

	// attached iterators that we invalidate when things change
	std::set<Iterator*> m_attachedIterators;
	CGraphNodeManager&  m_nodeManager;
};

//====================================================================
// MemStats
//====================================================================
inline size_t CAllNodesContainer::MemStats() const
{
	size_t result = sizeof(*this);
	size_t nodeSize = sizeof(tNodes::reference) /* + sizeof(void*)*3 + sizeof(int)*/;
	for (tAllNodes::const_iterator it = m_allNodes.begin(); it != m_allNodes.end(); ++it)
	{
		result += //sizeof(*it) +
		          sizeof(bool) + sizeof(void*) * 3 + sizeof(char) * 2 +
		          sizeof(it->first) + it->second.capacity() * nodeSize;
	}
	result += m_hashSpace.MemStats();
	result += m_attachedIterators.size() * (sizeof(Iterator*));
	return result;
}

//====================================================================
// Reset: Moves iterator to point at first node with matching NavType
//====================================================================
inline void CAllNodesContainer::Iterator::Reset()
{
	const unsigned navTypeMask = m_navTypeMask;

	for (m_currentAllNodesIt = m_container->m_allNodes.begin();
	     m_currentAllNodesIt != m_container->m_allNodes.end();
	     ++m_currentAllNodesIt)
	{
		const unsigned navType = m_currentAllNodesIt->first;
		tNodes& nodes = m_currentAllNodesIt->second;

		// If navType matches and node(s) exist for NavType
		if ((navType & navTypeMask) && !nodes.empty())
		{
			// Select first node of this NavType
			m_it = nodes.begin();
			m_currentEndIt = nodes.end();
			return;
		}
	}
	// container will always have an entry for nav mask 0
	m_it = m_container->m_allNodes.begin()->second.begin();
	m_currentEndIt = m_container->m_allNodes.begin()->second.end();
	m_currentAllNodesIt = m_container->m_allNodes.begin();
}

//====================================================================
// Iterator
// The const cast is evil... but without it then I have to implement
// a const iterator and that is a pain
//====================================================================
inline CAllNodesContainer::Iterator::Iterator(const CAllNodesContainer& container, IAISystem::tNavCapMask navTypeMask)
	: m_navTypeMask(navTypeMask), m_container(const_cast<CAllNodesContainer*>(&container))
{
	m_container->AttachIterator(this);
	Reset();
}

//===================================================================
// Iterator
//===================================================================
inline CAllNodesContainer::Iterator::~Iterator()
{
	if (m_container)
		m_container->DetachIterator(this);
}

//====================================================================
// Increment
//====================================================================
inline unsigned CAllNodesContainer::Iterator::Increment()
{
	AIAssert(m_container);
	unsigned nodeIndex = 0;

	if (m_it != m_currentEndIt)
	{
		nodeIndex = *m_it;
		// cppcheck-suppress assertWithSideEffect
		assert(m_container->m_nodeManager.GetNode(nodeIndex)->navType & m_navTypeMask);
		++m_it;
	}

	if (m_it == m_currentEndIt)
	{
		if (m_currentAllNodesIt == m_container->m_allNodes.end())
			return 0;
		for (++m_currentAllNodesIt; m_currentAllNodesIt != m_container->m_allNodes.end(); ++m_currentAllNodesIt)
		{
			if (m_currentAllNodesIt->first & m_navTypeMask)
			{
				tNodes& nodes = m_currentAllNodesIt->second;
				if (!nodes.empty())
				{
					m_it = nodes.begin();
					m_currentEndIt = nodes.end();
					if (0 == nodeIndex)
					{
						nodeIndex = *m_it;
						// cppcheck-suppress assertWithSideEffect
						AIAssert(m_container->m_nodeManager.GetNode(nodeIndex)->navType & m_navTypeMask);
						++m_it;
					}
					break;
				}
			}
		}
	}
	return nodeIndex;
}

//====================================================================
// GetNode
//====================================================================
inline unsigned CAllNodesContainer::Iterator::GetNode() const
{
	AIAssert(m_container);
	if (m_it != m_currentEndIt)
	{
		unsigned nodeIndex = *m_it;
		AIAssert(m_container->m_nodeManager.GetNode(nodeIndex)->navType & m_navTypeMask);
		return nodeIndex;
	}
	else
	{
		return 0;
	}
}

//====================================================================
// CAllNodesContainer
//====================================================================
inline CAllNodesContainer::CAllNodesContainer(CGraphNodeManager& nodeManager)
	:
	m_hashSpace(Vec3(7, 7, 7), 8192, GraphNodeHashSpaceTraits(nodeManager)),
	m_nodeManager(nodeManager)
{
	m_allNodes[0];
}

//===================================================================
// ~CAllNodesContainer
//===================================================================
inline CAllNodesContainer::~CAllNodesContainer()
{
	for (std::set<Iterator*>::iterator it = m_attachedIterators.begin(); it != m_attachedIterators.end(); ++it)
	{
		Iterator* pIt = *it;
		pIt->ContainerDeleted();
	}
}

//===================================================================
// ResetIterators
//===================================================================
inline void CAllNodesContainer::ResetIterators(IAISystem::tNavCapMask navTypeMask)
{
	for (std::set<Iterator*>::iterator it = m_attachedIterators.begin(); it != m_attachedIterators.end(); ++it)
	{
		Iterator* pIt = *it;
		if (pIt->m_navTypeMask | navTypeMask)
			pIt->Reset();
	}
}

//====================================================================
// AddNode
//====================================================================
inline void CAllNodesContainer::AddNode(unsigned nodeIndex)
{
	GraphNode* pNode = m_nodeManager.GetNode(nodeIndex);
	AIAssert(pNode);
	if (!pNode)
	{
		AIWarning("CAllNodesContainer: Attempting to add 0 node!");
		return;
	}
	unsigned type = pNode->navType;

	m_allNodes[type].insert(nodeIndex);

	m_hashSpace.AddObject(SGraphNodeRecord(nodeIndex));

	ResetIterators(type);
}

//====================================================================
// Reserve
//====================================================================
inline void CAllNodesContainer::Reserve(IAISystem::ENavigationType type, size_t size)
{
	m_allNodes[type].reserve(size);
}

//====================================================================
// RemoveNode
//====================================================================
inline void CAllNodesContainer::RemoveNode(unsigned nodeIndex)
{
	GraphNode* pNode = m_nodeManager.GetNode(nodeIndex);

	AIAssert(pNode);
	if (!pNode)
	{
		AIWarning("CAllNodesContainer: Attempting to remove 0 node!");
		return;
	}
	unsigned type = pNode->navType;
	tNodes& nodes = m_allNodes[type];

	tNodes::iterator it = nodes.find(nodeIndex); // why do I need to cast?!
	if (it == nodes.end())
	{
		AIWarning("CAllNodesContainer::RemoveNode Could not find node %p", pNode);
		return;
	}

	nodes.erase(it);

	m_hashSpace.RemoveObject(SGraphNodeRecord(nodeIndex));

	ResetIterators(type);
}

//====================================================================
// DoesNodeExist
// careful not to dereference the pointer, at least until we know it's
// valid
//====================================================================
inline bool CAllNodesContainer::DoesNodeExist(unsigned nodeIndex) const
{
	const GraphNode* pNode = m_nodeManager.GetNode(nodeIndex);
	if (!pNode)
	{
		AIWarning("CAllNodesContainer: Attempting to see if 0 node exists!");
		return false;
	}

	for (tAllNodes::const_iterator it = m_allNodes.begin();
	     it != m_allNodes.end();
	     ++it)
	{
		const tNodes& nodes = it->second;
		tNodes::const_iterator nodeIt = nodes.find(nodeIndex); // why do I need to cast?!
		if (nodeIt != nodes.end())
		{
			return true;
		}
	}
	return false;
}

////====================================================================
//// Erase
////====================================================================
//inline CAllNodesContainer::Iterator CAllNodesContainer::Erase(const Iterator& it)
//{
//  if (it.m_container != this)
//  {
//    AIWarning("CAllNodesContainer::Erase Asked to erase an iterator not associated with us");
//    return it;
//  }
//  if (const GraphNode *pNode = it.GetNode())
//  {
//    unsigned type = pNode->navType;
//    ResetIterators(type);
//  }
//  CAllNodesContainer::Iterator result(it);
//  if (result.m_it != result.m_currentEndIt)
//  {
//    m_hashSpace.RemoveObject(SGraphNodeRecord(it.GetNode()));
//		std::set<GraphNode*>::iterator itNext = result.m_it;
//		itNext++;
//		result.m_currentAllNodesIt->second.erase(result.m_it);
//    result.m_it = itNext;
//    if (result.m_it != result.m_currentEndIt)
//      return result;
//  }
//
//  result.Increment();
//  return result;
//}

//====================================================================
// GetAllNodesWithinRange
//====================================================================
inline void CAllNodesContainer::GetAllNodesWithinRange(std::vector<std::pair<float, unsigned>>& nodesOut, const Vec3& pos, float range, IAISystem::tNavCapMask navTypeMask) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);
	nodesOut.resize(0);
	SNodeCollector collector(m_nodeManager, nodesOut, navTypeMask);
	m_hashSpace.ProcessObjectsWithinRadius(pos, range, collector);
}

//====================================================================
// GetNodeWithinRange
//====================================================================
inline bool CAllNodesContainer::GetNodeWithinRange(std::pair<float, unsigned>& nodeOut, const Vec3& pos, float range, IAISystem::tNavCapMask navTypeMask, CGraph* graph) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	SOneNodeCollector collector(m_nodeManager, graph, pos, nodeOut, navTypeMask);
	return m_hashSpace.GetObjectWithinRadius(pos, range, collector);
}

#endif
