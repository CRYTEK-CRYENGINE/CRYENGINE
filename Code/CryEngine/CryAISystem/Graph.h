// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Graph.h
   $Id$
   Description: interface for the CGraph class.

   -------------------------------------------------------------------------
   History:
   - ?

 *********************************************************************/

#if !defined(AFX_GRAPH_H__6D059D2E_5A74_4352_B3BF_2C88D446A2E1__INCLUDED_)
#define AFX_GRAPH_H__6D059D2E_5A74_4352_B3BF_2C88D446A2E1__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include "NavPath.h"
#include "AIHash.h"
#include "AILog.h"
#include "AllNodesContainer.h"
#include "AutoTypeStructs.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <CryCore/Containers/CryArray.h>
#include <CryCore/Containers/VectorMap.h>

class CCryFile;
class CGraphLinkManager;

enum EPathfinderResult
{
	PATHFINDER_STILLFINDING,
	PATHFINDER_BEAUTIFYINGPATH,
	PATHFINDER_POPNEWREQUEST,
	PATHFINDER_PATHFOUND,
	PATHFINDER_NOPATH,
	PATHFINDER_ABORT,
	PATHFINDER_MAXVALUE
};
class CAISystem;
struct IStatObj;
class ICrySizer;
class CAIObject;
class CVolumeNavRegion;
class CFlightNavRegion;
class CGraphNodeManager;

class CSmartObject;
struct CCondition;

struct IVisArea;

inline int TypeIndexFromType(IAISystem::tNavCapMask type)
{
	int typeIndex;
	for (typeIndex = IAISystem::NAV_TYPE_COUNT - 1; typeIndex >= 0 && ((1 << typeIndex) & type) == 0; --typeIndex)
		;
	return typeIndex;
}

inline const char* StringFromTypeIndex(int typeIndex)
{
	static const char* navTypeStrings[] = {
		"NAV_UNSET",
		"NAV_TRIANGULAR",
		"NAV_WAYPOINT_HUMAN",
		"NAV_WAYPOINT_3DSURFACE",
		"NAV_FLIGHT",
		"NAV_VOLUME",
		"NAV_ROAD",
		"NAV_SMARTOBJECT",
		"NAV_FREE_2D",
		"NAV_CUSTOM_NAVIGATION",
	};
	const int STRING_COUNT = CRY_ARRAY_COUNT(navTypeStrings);

	static_assert(STRING_COUNT == static_cast<int>(IAISystem::NAV_TYPE_COUNT), "Unexpected array size!");

	if (typeIndex < 0)
		return "<Invalid Nav Type>";
	else
		return navTypeStrings[typeIndex];
}

inline const char* StringFromType(IAISystem::ENavigationType type)
{
	return StringFromTypeIndex(TypeIndexFromType(type));
}

//====================================================================
// CObstacleRef
//====================================================================
class CObstacleRef
{
protected:
	CWeakRef<CAIObject> m_refAnchor;   // designer defined hiding point
	int                 m_vertexIndex; // index of vertex
	unsigned            m_nodeIndex;   // for indoors nodes could be hide points
	GraphNode*          m_pNode;
	CSmartObject*       m_pSmartObject; // pointer to smart object to be used for hiding
	CCondition*         m_pRule;        // pointer to smart object rule to be used for hiding

public:
	CAIObject*        GetAnchor() const      { return m_refAnchor.GetAIObject(); }
	int               GetVertex() const      { return m_vertexIndex; }
	const unsigned    GetNodeIndex() const   { return m_nodeIndex; }
	const GraphNode*  GetNode() const        { return m_pNode; }
	CSmartObject*     GetSmartObject() const { return m_pSmartObject; }
	const CCondition* GetRule() const        { return m_pRule; }

	CObstacleRef() : m_vertexIndex(-1), m_nodeIndex(0), m_pNode(0), m_pSmartObject(NULL), m_pRule(NULL) {}
	CObstacleRef(const CObstacleRef& other) : m_refAnchor(other.m_refAnchor), m_vertexIndex(other.m_vertexIndex), m_nodeIndex(other.m_nodeIndex), m_pNode(other.m_pNode),
		m_pSmartObject(other.m_pSmartObject), m_pRule(other.m_pRule) {}
	CObstacleRef(CWeakRef<CAIObject> refAnchor) : m_refAnchor(refAnchor), m_vertexIndex(-1), m_nodeIndex(0), m_pNode(0), m_pSmartObject(NULL), m_pRule(NULL) {}
	CObstacleRef(int vertexIndex) : m_vertexIndex(vertexIndex), m_nodeIndex(0), m_pNode(0), m_pSmartObject(NULL), m_pRule(NULL) {}
	CObstacleRef(unsigned nodeIndex, GraphNode* pNode) : m_vertexIndex(-1), m_nodeIndex(nodeIndex), m_pNode(pNode), m_pSmartObject(NULL), m_pRule(NULL) {}
	CObstacleRef(CSmartObject* pSmartObject, CCondition* pRule) : m_vertexIndex(-1), m_nodeIndex(0), m_pNode(0)
		, m_pSmartObject(pSmartObject), m_pRule(pRule) {}

	Vec3                GetPos() const;
	float               GetApproxRadius() const;
	const CObstacleRef& operator=(const CObstacleRef& other)
	{
		m_refAnchor = other.m_refAnchor;
		m_vertexIndex = other.m_vertexIndex;
		m_nodeIndex = other.m_nodeIndex;
		m_pNode = other.m_pNode;
		m_pSmartObject = other.m_pSmartObject;
		m_pRule = other.m_pRule;
		return *this;
	}

	bool operator==(const CObstacleRef& other) const
	{
		return m_refAnchor == other.m_refAnchor && m_vertexIndex == other.m_vertexIndex && m_nodeIndex == other.m_nodeIndex
		       && m_pNode == other.m_pNode && m_pSmartObject == other.m_pSmartObject && m_pRule == other.m_pRule;
	}
	bool operator!=(const CObstacleRef& other) const
	{
		return m_refAnchor != other.m_refAnchor || m_vertexIndex != other.m_vertexIndex || m_nodeIndex != other.m_nodeIndex
		       || m_pNode != other.m_pNode || m_pSmartObject != other.m_pSmartObject || m_pRule != other.m_pRule;
	}
	bool operator<(const CObstacleRef& other) const
	{
		return
		  m_nodeIndex < other.m_nodeIndex || m_nodeIndex == other.m_nodeIndex &&
		  (m_refAnchor < other.m_refAnchor || m_refAnchor == other.m_refAnchor &&
		   (m_vertexIndex < other.m_vertexIndex || m_vertexIndex == other.m_vertexIndex &&
		    (m_pSmartObject < other.m_pSmartObject || m_pSmartObject < other.m_pSmartObject &&
		     m_pRule < other.m_pRule)));
	}
	operator bool() const
	{
		return m_refAnchor.IsValid() || m_vertexIndex >= 0 || m_nodeIndex || m_pSmartObject && m_pRule;
	}
	bool operator!() const
	{
		return !m_refAnchor.IsValid() && m_vertexIndex < 0 && !m_nodeIndex && (!m_pSmartObject || !m_pRule);
	}

private:
	operator int() const
	{
		// it is illegal to cast CObstacleRef to an int!!!
		// are you still using old code?
		AIAssert(0);
		return 0;
	}
};

// NOTE: int64 here avoids a tiny performance impact on 32-bit platform
// for the cost of loss of full compatibility: 64-bit generated BAI files
// can't be used on 32-bit platform safely. Change the key to int64 to
// make it fully compatible. The code that uses this map will be recompiled
// to use the full 64-bit key on both 32-bit and 64-bit platforms.
typedef std::multimap<int64, unsigned>                EntranceMap;
typedef std::vector<Vec3>                             ListPositions;
typedef std::vector<ObstacleData>                     ListObstacles;
typedef std::multimap<float, ObstacleData>            MultimapRangeObstacles;
typedef std::vector<NodeDescriptor>                   NodeDescBuffer;
typedef std::vector<LinkDescriptor>                   LinkDescBuffer;
typedef std::list<unsigned>                           ListNodeIds;
typedef std::set<GraphNode*>                          SetNodes;
typedef std::set<const GraphNode*>                    SetConstNodes;
typedef std::list<const GraphNode*>                   ListConstNodes;
typedef std::vector<const GraphNode*>                 VectorConstNodes;
typedef std::vector<unsigned>                         VectorConstNodeIndices;
typedef std::multimap<float, GraphNode*>              CandidateMap;
typedef std::multimap<float, unsigned>                CandidateIdMap;
typedef std::set<CObstacleRef>                        SetObstacleRefs;
typedef VectorMap<unsigned, SCachedPassabilityResult> PassabilityCache;

// [Mikko] Note: The Vector map is faster when traversing, and the normal map with pool allocator seems
// to be a little faster in CGraph.GetNodesInRange. Both are faster than normal std::map.
//typedef stl::STLPoolAllocator< std::pair<const GraphNode*, float> > NodeMapAllocator;
//typedef std::map<const GraphNode*, float, std::less<const GraphNode*>, NodeMapAllocator> MapConstNodesDistance;
typedef VectorMap<const GraphNode*, float> MapConstNodesDistance;

//====================================================================
// CGraph
//====================================================================
class CGraph
{
public:
	CGraph();
	~CGraph();

	CGraphLinkManager&       GetLinkManager()       { return *m_pGraphLinkManager; }
	const CGraphLinkManager& GetLinkManager() const { return *m_pGraphLinkManager; }

	CGraphNodeManager&       GetNodeManager()       { return *m_pGraphNodeManager; }
	const CGraphNodeManager& GetNodeManager() const { return *m_pGraphNodeManager; }

	/// Restores the graph to the initial state (i.e. restores pass radii etc).
	void Reset();

	/// Calls CGraphNode::ResetIDs() with the correct arguments for this graph
	void ResetIDs();

	/// removes all nodes and stuff associated with navTypes matching the bitmask
	void Clear(IAISystem::tNavCapMask navTypeMask);

	/// Connects (two-way) two nodes, optionally returning pointers to the new links
	void ConnectInCm(unsigned oneIndex, unsigned twoIndex, int16 radiusOneToTwoCm = 10000, int16 radiusTwoToOneCm = 10000,
	                 unsigned* pLinkOneTwo = 0, unsigned* pLinkTwoOne = 0);

	/// Connects (two-way) two nodes, optionally returning pointers to the new links
	void Connect(unsigned oneIndex, unsigned twoIndex, float radiusOneToTwo = 100.0f, float radiusTwoToOne = 100.0f,
	             unsigned* pLinkOneTwo = 0, unsigned* pLinkTwoOne = 0);

	/// Disconnects a node from its neighbours. if bDelete then pNode will be deleted. Note that
	/// the previously connected nodes will not be deleted, even if they
	/// end up with no nodes.
	void Disconnect(unsigned nodeIndex, bool bDelete = true);

	/// Removes an individual link from a node (and removes the reciprocal link) -
	/// doesn't delete it.
	void Disconnect(unsigned nodeIndex, unsigned linkId);

	/// Checks the graph is OK (as far as possible). Asserts if not, and then
	/// returns true/false to indicate if it's OK
	/// msg should indicate where this is being called from (for writing error msgs)
	bool Validate(const char* msg, bool checkPassable) const;

	/// Checks that a node exists (should be quick). If fullCheck is true it will do some further
	/// checks which will be slower
	bool ValidateNode(unsigned nodeIndex, bool fullCheck) const;
	bool ValidateHashSpace() { return m_allNodes.ValidateHashSpace(); }

	/// Restores all node/links
	void RestoreAllNavigation();

	/// Reads the AI graph from a specified file
	bool ReadFromFile(const char* szName);

	/// Returns all nodes that are in the graph - not all nodes will be
	/// connected (even indirectly) to each other
	CAllNodesContainer&       GetAllNodes()       { return m_allNodes; }
	const CAllNodesContainer& GetAllNodes() const { return m_allNodes; }

	/// Checks that the graph is empty. Pass in a bitmask of IAISystem::ENavigationType to
	/// specify the types to check
	bool CheckForEmpty(IAISystem::tNavCapMask navTypeMask = IAISystem::NAVMASK_ALL) const;

	/// uses mark for internal graph operation without disturbing the pathfinder
	void MarkNode(unsigned nodeIndex) const;
	/// clears the marked nodes
	void ClearMarks() const;

	// defines bounding rectangle of this graph
	void SetBBox(const Vec3& min, const Vec3& max);
	// how is that for descriptive naming of functions ??
	bool InsideOfBBox(const Vec3& pos) const;  // returns true if pos is inside of bbox (but not on boundaries)

	/// Creates a new node of the specified type (which can't be subsequently changed). Note that
	/// to delete the node use the disconnect function.
	unsigned         CreateNewNode(IAISystem::tNavCapMask type, const Vec3& pos, unsigned ID = 0);
	GraphNode*       GetNode(unsigned index);
	const GraphNode* GetNode(unsigned index) const;

	/// Moves a node, updating spatial structures
	void MoveNode(unsigned nodeIndex, const Vec3& newPos);

	/// finds all nodes within range of startPos and their distance from vStart.
	/// pStart is just used as a hint.
	/// returns a reference to the input/output so it's easy to use in a test.
	/// traverseForbiddenHideLink should be true if you want to pass through
	/// links between hide waypoints that have been marked as impassable
	/// SmartObjects will only be considered if pRequester != 0
	MapConstNodesDistance& GetNodesInRange(MapConstNodesDistance& result, const Vec3& startPos, float maxDist,
	                                       IAISystem::tNavCapMask navCapMask, float passRadius, unsigned startNodeIndex = 0, const class CAIObject* pRequester = 0);

	// Returns memory usage not including nodes
	size_t MemStats();

	// Returns the memory usage for nodes of the type passed in (bitmask)
	size_t NodeMemStats(unsigned navTypeMask);

	void   GetMemoryStatistics(ICrySizer* pSizer);

	struct SBadGraphData
	{
		enum EType {BAD_PASSABLE, BAD_IMPASSABLE};
		EType mType;
		Vec3  mPos1, mPos2;
	};
	/// List of bad stuff we found during the last validation. mutable because it's
	/// debug - Validate(...) should really be a const method, since it wouldn't change
	/// any "real" data
	mutable std::vector<SBadGraphData> mBadGraphData;

	GraphNode*                         m_pSafeFirst;
	unsigned                           m_safeFirstIndex;

	CGraphNodeManager*                 m_pGraphNodeManager;

private:

	/// Finds nodes within a distance of pNode that can be accessed by something with
	/// passRadius. If pRequester != 0 then smart object links will be checked as well.
	void FindNodesWithinRange(MapConstNodesDistance& result, float curDist, float maxDist,
	                          const GraphNode* pNode, float passRadius, const class CAIObject* pRequester) const;

	bool DbgCheckList(ListNodeIds& nodesList) const;

public:
	/// deletes (disconnects too) all nodes with a type matching the bitmask
	void DeleteGraph(IAISystem::tNavCapMask navTypeMask);

private:
	GraphNode* GetEntrance(int nBuildingID, const Vec3& pos);
	bool       GetEntrances(int nBuildingID, const Vec3& pos, std::vector<unsigned>& nodes);
	// reads all the nodes in a map
	bool       ReadNodes(CCryFile& file);
	/// Deletes the node, which should have been disconnected first (warning if not)
	void       DeleteNode(unsigned nodeIndex);

	// helper called from ValidateNode only (to get ValidateNode to be inlined inside
	// Graph.cpp)
	bool ValidateNodeFullCheck(const GraphNode* pNode) const;

	unsigned   m_currentIndex;
	GraphNode* m_pCurrent;
	unsigned   m_firstIndex;
	GraphNode* m_pFirst;

	/// All the nodes we've marked
	mutable VectorConstNodeIndices m_markedNodes;
	/// All the nodes we've tagged
	mutable VectorConstNodeIndices m_taggedNodes;

	/// nodes are allocated/deleted via a single interface, so keep track of them
	/// all - for memory tracking and to allow quick iteration
	CAllNodesContainer m_allNodes;

	CGraphLinkManager* m_pGraphLinkManager;

	/// Bounding box of the triangular area
	AABB        m_triangularBBox;

	EntranceMap m_mapEntrances;
	EntranceMap m_mapExits;
	friend class CFlightNavRegion;
	friend class CVolumeNavRegion;
};

// Check whether a position is within a node's triangle
bool PointInTriangle(const Vec3& pos, GraphNode* pNode);

//====================================================================
// SMarkClearer
// Helper - the constructor and destructor clear marks
//====================================================================
struct SMarkClearer
{
	SMarkClearer(const CGraph* pGraph) : m_pGraph(pGraph) { m_pGraph->ClearMarks(); }
	~SMarkClearer() { m_pGraph->ClearMarks(); }
private:
	const CGraph* m_pGraph;
};

#endif // !defined(AFX_GRAPH_H__6D059D2E_5A74_4352_B3BF_2C88D446A2E1__INCLUDED_)
