// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef GRAPHSTRUCTURES_H
#define GRAPHSTRUCTURES_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "AutoTypeStructs.h"

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <float.h>
#endif

#ifdef _DEBUG
	#define DEBUG_GRAPHNODE_IDS
#endif

class ObstacleIndexVector
{
public:
	typedef const int* const_iterator;
	typedef int*       iterator;
	ObstacleIndexVector() { clear(); }
	const_iterator begin() const                  { return &m_idx[0]; }
	const_iterator end() const                    { return &m_idx[size()]; }
	iterator       begin()                        { return &m_idx[0]; }
	iterator       end()                          { return &m_idx[size()]; }
	size_t         size() const                   { return (m_idx[2] < 0) ? (size_t)(-m_idx[2] - 1) : 3; }
	size_t         capacity() const               { return 3; }
	bool           empty() const                  { return m_idx[2] == -1; }
	void           reserve(size_t size)           { assert(size <= 3); }
	void           clear()                        { m_idx[0] = -1; m_idx[1] = -1; m_idx[2] = -1; }
	void           swap(ObstacleIndexVector& rhs) { for (int i = 0; i < 3; i++) { int tmp = m_idx[i]; m_idx[i] = rhs.m_idx[i]; rhs.m_idx[i] = tmp; } }
	void           push_back(int val)             { assert(val >= 0); assert(m_idx[2] < 0); m_idx[2]--; m_idx[size() - 1] = val; }
	int            operator[](int idx) const      { assert(idx >= 0 && idx < (int)size()); return m_idx[idx]; }
private:
	int m_idx[3];
};

struct GraphNode;
class CGraphNodeManager;
class CSmartObjectClass;
class CSmartObject;
struct SmartObjectHelper;

namespace NavGraphUtils
{
ILINE int16 InCentimeters(float distance)
{
	return (int16)(distance * 100.0f);
}

ILINE float InMeters(int16 distanceInCm)
{
	return (float)distanceInCm * 0.01f;
}
}

//====================================================================
// GraphLinkBidirectionalData
//====================================================================

struct GraphLinkBidirectionalData
{
	friend class CGraphLinkManager;

	GraphLinkBidirectionalData()
		: fExposure(0.0f), vEdgeCenter(0.0f, 0.0f, 0.0f)
	{
		directionalData[0].nextLinkIndex = directionalData[1].nextLinkIndex = 0;
		directionalData[0].nextNodeIndex = directionalData[1].nextNodeIndex = 0;
		directionalData[0].waterDepthInCmHigh = directionalData[1].waterDepthInCmHigh = 0;
		directionalData[0].waterDepthInCmLow = directionalData[1].waterDepthInCmLow = 0;
		directionalData[0].maxRadiusInCm = directionalData[1].maxRadiusInCm = 0;
		directionalData[0].origMaxRadiusInCm = directionalData[1].origMaxRadiusInCm = 0;
		directionalData[0].startIndex = directionalData[1].startIndex = 0;
		directionalData[0].endIndex = directionalData[1].endIndex = 0;
		directionalData[0].simplePassabilityCheck = directionalData[1].simplePassabilityCheck = 0;
	}

private:
	struct DirectionalData
	{
		// maximum size sphere that can pass trough this edge
		signed maxRadiusInCm : 16;

		// links may get modified at run-time - store a copy of their original pass radius
		signed origMaxRadiusInCm : 16;

		// Links are stored in a linked-list - this is the index of the next link for the nodes at each end.
		unsigned nextLinkIndex : 23;

		// Indicates if a human waypoint link can use simplified walkability check.  For other link
		// types is not applicable.
		//
		// NOTE Apr 26, 2007: <pvl> in order not to undo memory optimization done earlier this
		// member is used in a very tricky way.  In directionalData[0] it says whether this
		// link is "simple" while in directionalData[1] it indicates if the link "simplicity"
		// has already been computed at all (so when it's 'false' then
		// directionalData[0].simplePassabilityCheck cannot be considered valid).
		// This ugliness is encapsulated in GraphLinkManager but anyway ...
		//
		// By doing all of this, I sold my soul to MichaelS.
		unsigned simplePassabilityCheck : 1;

		// Minimum/maximum (depending on which direction we are talking) water depth over this link in cm.
		// Has to be split up into two to make the bitfields add up to 32 bit blocks (it doesnt seem to pack properly
		// otherwise).
		unsigned waterDepthInCmLow  : 8;
		signed   waterDepthInCmHigh : 8;

		// next graph node this way
		unsigned nextNodeIndex : 20;

		// indices of the edge vertices of this edge - only valid for 2D navigation
		// types (triangle and waypoint).
		unsigned startIndex : 2;
		unsigned endIndex   : 2;
	};

	DirectionalData directionalData[2];

	/// The exposure associated with this link
	float fExposure;
	Vec3  vEdgeCenter;
};

#include "GraphLinkManager.h"

struct SCachedPassabilityResult
{
	SCachedPassabilityResult()
		: spatialHash(0)
		, walkableResult(false)
	{
	}

	SCachedPassabilityResult(size_t hash, bool walkable)
		: spatialHash(hash)
		, walkableResult(walkable)
	{
	}

	void Reset(size_t hash, bool walkable)
	{
		spatialHash = hash;
		walkableResult = walkable;
	}

	size_t spatialHash;
	bool   walkableResult;
};

struct IVisArea;

struct STriangularNavData
{
	STriangularNavData() : isForbidden(0), isForbiddenDesigner(0) {}
	ObstacleIndexVector vertices;
	/// isForbidden is set for navigation nodes that lie inside a forbidden area
	uint8               isForbidden         : 1;
	/// isForbiddenDesigner is only set for nav nodes inside designer forbidden areas
	uint8               isForbiddenDesigner : 1;

	real GetDegeneracyValue();
	void MakeAntiClockwise();
	bool IsAntiClockwise();
	real GetCross(CGraphLinkManager& linkManager, const Vec3r& vCutStart, const Vec3r& vDir, unsigned theLink);
};

struct SWaypointNavData
{
	IVisArea*                pArea;

	static EWaypointLinkType GetLinkTypeFromRadius(float radius)
	{
		if (abs(radius - WLT_AUTO_PASS) < 0.001f)
			return WLT_AUTO_PASS;
		else if (abs(radius - WLT_EDITOR_PASS) < 0.001f)
			return WLT_EDITOR_PASS;
		else if (abs(radius - WLT_EDITOR_IMPASS) < 0.001f)
			return WLT_EDITOR_IMPASS;
		else if (abs(radius - WLT_AUTO_IMPASS) < 0.001f)
			return WLT_AUTO_IMPASS;
		else
			return WLT_UNKNOWN_TYPE;
	}

	static EWaypointLinkType GetLinkTypeFromRadiusInCm(int16 radius)
	{
		if (radius == (WLT_AUTO_PASS * 100))
			return WLT_AUTO_PASS;
		else if (radius == WLT_EDITOR_PASS * 100)
			return WLT_EDITOR_PASS;
		else if (radius == WLT_EDITOR_IMPASS * 100)
			return WLT_EDITOR_IMPASS;
		else if (radius == WLT_AUTO_IMPASS * 100)
			return WLT_AUTO_IMPASS;
		else
			return WLT_UNKNOWN_TYPE;
	}

	int               nBuildingID;
	EWaypointNodeType type;

	// if it's a hide point then store the direction the hide point faces.
	Vec3 dir;
	// Stores the local up (z) axis of the point - used in nav regions where type = 1 (3D surface)
	Vec3 up;

	SWaypointNavData() : pArea(0), nBuildingID(-1), type(WNT_UNSET), dir(ZERO), up(0, 0, 1) {}
};

struct SFlightNavData
{
	int nSpanIdx;
	//  SFlightNavData() : nSpanIdx(-1) {}
};

struct SVolumeNavData
{
	int nVolimeIdx;
	//  SVolumeNavData() : nVolimeIdx(-1) {}
};

struct SRoadNavData
{
	/// total width of the road at this point
	float fRoadWidth;
	/// offset for driving (+ve is on the right side of the road)

	// As soon as we decided use only Width not needed store this structure separate
	//float fRoadOffset;
	//SRoadNavData() : fRoadWidth(0.0f), fRoadOffset(0.0f) {}
};

struct SSmartObjectNavData
{
	CSmartObjectClass* pClass;
	SmartObjectHelper* pHelper;
	CSmartObject*      pSmartObject;
};

struct SLayeredMeshNavData
{
	static const unsigned s_polyIndexWidth = 20;
	static const unsigned s_modifIndexWidth = 8;
	static const unsigned s_agentTypeWidth = 4;

	static const unsigned s_polyIndexShift = 0;
	static const unsigned s_modifIndexShift = s_polyIndexWidth;
	static const unsigned s_agentTypeShift = s_polyIndexWidth + s_modifIndexWidth;

	static const unsigned s_polyIndexMask = (1 << s_polyIndexWidth) - 1;
	static const unsigned s_modifIndexMask = (1 << s_modifIndexWidth) - 1;
	static const unsigned s_agentTypeMask = (1 << s_agentTypeWidth) - 1;

	unsigned int          polygonIndex: s_polyIndexWidth;
	unsigned int          navModifIndex: s_modifIndexWidth;
	unsigned int          agentType: s_agentTypeWidth;

	// NOTE Feb 18, 2009: <pvl> unpack members from a single int (useful for deserialisation)
	void SetFromPacked(unsigned packed)
	{
		polygonIndex = (packed >> s_polyIndexShift) & s_polyIndexMask;
		navModifIndex = (packed >> s_modifIndexShift) & s_modifIndexMask;
		agentType = (packed >> s_agentTypeShift) & s_agentTypeMask;
	}
};

struct PathfindingHeuristicProperties;

struct SCustomNavData
{
	typedef float (* CostFunction)(const void* node1Data, const void* node2Data, const PathfindingHeuristicProperties& pathFindProperties);

	void*        pCustomData;
	CostFunction pCostFunction;
	uint16       customId;

	SCustomNavData() : pCustomData(0), pCostFunction(0), customId(0)
	{}
};

//====================================================================
// GraphNode
// In principle this is a constant structure in that the pathfinder
// doesn't modify it. However, in practice during pathfinding it's
// convenient to cache some info along with the node - hence the
// mutable members
// This structure needs to represent different types of nodes - the basic
// info should just represent the generic navigation info, and the data
// specific to the navigation region lives in navData
//====================================================================
struct GraphNode
{
	/// navType is const so that nodes can be stored in lists of their respective types
	const IAISystem::ENavigationType navType;

	/// The position of the node The position can only be set indirectly so that
	/// the AI system can update any spatial structures containing this node
	const Vec3&                GetPos() const { return pos; }

	STriangularNavData*        GetTriangularNavData();
	SWaypointNavData*          GetWaypointNavData();
	SFlightNavData*            GetFlightNavData();
	SVolumeNavData*            GetVolumeNavData();
	SRoadNavData*              GetRoadNavData();
	SSmartObjectNavData*       GetSmartObjectNavData();
	SLayeredMeshNavData*       GetLayeredMeshNavData();

	const STriangularNavData*  GetTriangularNavData() const;
	const SWaypointNavData*    GetWaypointNavData() const;
	const SFlightNavData*      GetFlightNavData() const;
	const SVolumeNavData*      GetVolumeNavData() const;
	const SRoadNavData*        GetRoadNavData() const;
	const SSmartObjectNavData* GetSmartObjectNavData() const;
	const SLayeredMeshNavData* GetLayeredMeshNavData() const;

	/// Used as a flag for costs that aren't calculated yet
	static const float fInvalidCost;

	// Returns the link index that goes to destNode. -1 if no link found
	int      GetLinkIndex(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* destNode) const;
	unsigned GetLinkTo(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* destNode) const;
	int      GetLinkCount(CGraphLinkManager& linkManager) const;
	void     RemoveLinkTo(CGraphLinkManager& linkManager, unsigned nodeIndex);

	/// Returns the maximu radius from all the links
	float GetMaxLinkRadius(CGraphLinkManager& linkManager) const
	{
		float r = -FLT_MAX;
		//for (unsigned i = links.size() ; i-- != 0 ;)
		for (unsigned link = firstLinkIndex; link; link = linkManager.GetNextLink(link))
			if (linkManager.GetRadius(link) > r)
				r = linkManager.GetRadius(link);
		return r;
	}

	unsigned FindNewLink(CGraphLinkManager& linkManager);

	bool     Release()
	{
		assert(nRefCount >= 1);
		--nRefCount;
		if (nRefCount == 0)
			return true;
		return false;
	}

	void AddRef()
	{
		assert(nRefCount < std::numeric_limits<TRefCount>::max());
		++nRefCount;
	}

	// this resets our IDs and assigns a unique ID to all the nodes in the container.
	// Should just be called on navigation graph saving. pNodeForID1 is a special node
	// that will have ID 1 assigned to it
	static void ResetIDs(CGraphNodeManager & nodeManager, class CAllNodesContainer & allNodes, GraphNode * pNodeForID1);

	size_t      MemStats();

	static void ClearFreeIDs() { stl::free_container(freeIDs); }

	// This must ONLY be called via the graph node deletion function
	~GraphNode();

protected:
	/// This must ONLY be called via the graph node creation function.
	/// If _ID == 0 then an ID will be assigned automatically
	GraphNode(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID);

private:
	friend class CGraph;
	friend struct GraphNodesPool;

	void SetPos(const Vec3& newPos) { pos = newPos; }

public:
	// Links are stored in a linked-list - this is the index of the next link for the nodes at each end.
	unsigned firstLinkIndex;

private:
	// stuff below here is basically constant (i.e. not modified by pathfinding), unless
	// the world changes.
	// unique ID - preserved when saving.
	unsigned int                     ID;

	Vec3                             pos;
	/// The pool of free IDs - populated when nodes are deleted
	static std::vector<unsigned int> freeIDs;
	/// The highest ID currently in use (0 is invalid), so maxID+1 is a valid unique ID
#ifdef DEBUG_GRAPHNODE_IDS
	static std::vector<unsigned int> usedIds;
#endif

	static unsigned int maxID;
public:
	typedef unsigned short TRefCount;
	TRefCount nRefCount;

	/// General marker used to help traversing the graph of nodes. If you use this
	/// be careful about calling other functions that use it! Not used by pathfinder
	mutable unsigned char mark;//:1;

};

struct GraphNode_Unset : public GraphNode
{
private:
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Unset(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Unset() {}
};

struct GraphNode_Triangular : public GraphNode
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Triangular(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Triangular() {}

	STriangularNavData navData;
};

struct GraphNode_Waypoint : public GraphNode
{
protected:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Waypoint(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Waypoint() {}

	SWaypointNavData navData;
};

struct GraphNode_WaypointHuman : public GraphNode_Waypoint
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_WaypointHuman(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode_Waypoint(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_WaypointHuman() {}
};

struct GraphNode_Waypoint3DSurface : public GraphNode_Waypoint
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Waypoint3DSurface(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode_Waypoint(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Waypoint3DSurface() {}
};

struct GraphNode_Flight : public GraphNode
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Flight(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Flight() {}

	SFlightNavData navData;
};

struct GraphNode_Volume : public GraphNode
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Volume(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Volume() {}

	SVolumeNavData navData;
};

struct GraphNode_Road : public GraphNode
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Road(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Road() {}

	SRoadNavData navData;
};

struct GraphNode_SmartObject : public GraphNode
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_SmartObject(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_SmartObject() {}

	SSmartObjectNavData navData;
};

struct GraphNode_Free2D : public GraphNode
{
private:
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_Free2D(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
		: GraphNode(type, inpos, _ID) {}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_Free2D() {}
};

struct GraphNode_CustomNav : public GraphNode
{
	friend struct GraphNode;
	friend class CGraph;
	friend struct GraphNodesPool;
	friend class CGraphNodeManager;

	GraphNode_CustomNav(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID) : GraphNode(type, inpos, _ID)
	{}

	// This must ONLY be called via the graph node deletion function
	~GraphNode_CustomNav()
	{}

	SCustomNavData navData;

public:

	void SetCustomData(void* customData)
	{
		navData.pCustomData = customData;
	}

	void SetCustomId(uint16 customId)
	{
		navData.customId = customId;
	}

	void* GetCustomData()
	{
		return navData.pCustomData;
	}

	const void* GetCustomData() const
	{
		return navData.pCustomData;
	}

	uint16 GetCustomId()
	{
		return navData.customId;
	}

	uint16 GetCustomId() const
	{
		return navData.customId;
	}

	void SetCostFunction(SCustomNavData::CostFunction pCostFunction)
	{
		navData.pCostFunction = pCostFunction;
	}

	float CustomLinkCostFactor(const void* node1Data, const void* node2Data, const PathfindingHeuristicProperties& pathFindProperties) const
	{
		float ret = FLT_MAX;

		if (navData.pCostFunction)
		{
			ret = navData.pCostFunction(node1Data, node2Data, pathFindProperties);
		}

		return ret;
	};
};

#include "GraphNodeManager.h"

inline STriangularNavData*        GraphNode::GetTriangularNavData()        { assert(navType == IAISystem::NAV_TRIANGULAR); return &((GraphNode_Triangular*)this)->navData; }
inline SWaypointNavData*          GraphNode::GetWaypointNavData()          { assert(navType == IAISystem::NAV_WAYPOINT_3DSURFACE || navType == IAISystem::NAV_WAYPOINT_HUMAN); return &((GraphNode_Waypoint*)this)->navData; }
inline SFlightNavData*            GraphNode::GetFlightNavData()            { assert(navType == IAISystem::NAV_FLIGHT); return &((GraphNode_Flight*)this)->navData; }
inline SVolumeNavData*            GraphNode::GetVolumeNavData()            { assert(navType == IAISystem::NAV_VOLUME); return &((GraphNode_Volume*)this)->navData; }
inline SRoadNavData*              GraphNode::GetRoadNavData()              { assert(navType == IAISystem::NAV_ROAD); return &((GraphNode_Road*)this)->navData; }
inline SSmartObjectNavData*       GraphNode::GetSmartObjectNavData()       { assert(navType == IAISystem::NAV_SMARTOBJECT); return &((GraphNode_SmartObject*)this)->navData; }

inline const STriangularNavData*  GraphNode::GetTriangularNavData() const  { assert(navType == IAISystem::NAV_TRIANGULAR); return &((GraphNode_Triangular*)this)->navData; }
inline const SWaypointNavData*    GraphNode::GetWaypointNavData() const    { assert(navType == IAISystem::NAV_WAYPOINT_3DSURFACE || navType == IAISystem::NAV_WAYPOINT_HUMAN); return &((GraphNode_Waypoint*)this)->navData; }
inline const SFlightNavData*      GraphNode::GetFlightNavData() const      { assert(navType == IAISystem::NAV_FLIGHT); return &((GraphNode_Flight*)this)->navData; }
inline const SVolumeNavData*      GraphNode::GetVolumeNavData() const      { assert(navType == IAISystem::NAV_VOLUME); return &((GraphNode_Volume*)this)->navData; }
inline const SRoadNavData*        GraphNode::GetRoadNavData() const        { assert(navType == IAISystem::NAV_ROAD); return &((GraphNode_Road*)this)->navData; }
inline const SSmartObjectNavData* GraphNode::GetSmartObjectNavData() const { assert(navType == IAISystem::NAV_SMARTOBJECT); return &((GraphNode_SmartObject*)this)->navData; }

enum EObstacleFlags
{
	OBSTACLE_COLLIDABLE = BIT(0),
	OBSTACLE_HIDEABLE   = BIT(1),
};

struct ObstacleData
{
	Vec3 vPos;
	Vec3 vDir;
	/// this radius is approximate - it is estimated during link generation. if -ve it means
	/// that it shouldn't be used (i.e. object is significantly non-circular)
	float       fApproxRadius;
	uint8       flags;
	uint8       approxHeight; // height in 4.4 fixed point format.

	inline void SetCollidable(bool state) { state ? (flags |= OBSTACLE_COLLIDABLE) : (flags &= ~OBSTACLE_COLLIDABLE); }
	inline void SetHideable(bool state)   { state ? (flags |= OBSTACLE_HIDEABLE) : (flags &= ~OBSTACLE_HIDEABLE); }
	inline bool IsCollidable() const      { return (flags & OBSTACLE_COLLIDABLE) != 0; }
	inline bool IsHideable() const        { return (flags & OBSTACLE_HIDEABLE) != 0; }

	/// Sets the approximate height and does the necessary conversion.
	inline void  SetApproxHeight(float h) { approxHeight = (uint8)(clamp_tpl(h * (1 << 4), 0.0f, 255.0f)); }
	/// Returns the approximate height and does the necessary conversion.
	inline float GetApproxHeight() const  { return (float)approxHeight / (float)(1 << 4); }
	/// Gets the navigation node that this obstacle is attached to (and caches the result). The result should be
	/// safe in game, but might be invalid in editor if the navigation gets modified whilst AI/physics is running
	/// so I advise against dereferencing the result (unless you validate it first).
	/// The calculation assumes that this obstacle is only attached to triangular nodes
	const std::vector<const GraphNode*>& GetNavNodes() const;
	void ClearNavNodes() { navNodes.clear(); }
	// mechanisms for setting the nav nodes on a temporary obstacle object
	void SetNavNodes(const std::vector<const GraphNode*>& nodes);
	void AddNavNode(const GraphNode* pNode);

	/// Note - this is used during triangulation where we can't have two objects
	/// sitting on top of each other - even if they have different directions etc
	bool operator==(const ObstacleData& other) const { return ((fabs(vPos.x - other.vPos.x) < 0.001f) && (fabs(vPos.y - other.vPos.y) < 0.001f)); }

	ObstacleData(const Vec3& pos = Vec3(0, 0, 0), const Vec3& dir = Vec3(0, 0, 0))
		: vPos(pos)
		, vDir(dir)
		, fApproxRadius(-1.0f)
		, flags(OBSTACLE_COLLIDABLE)
		, approxHeight(0)
		, needToEvaluateNavNodes(false)
	{}

	void Serialize(TSerialize ser);

private:
	// pointer to the triangular navigation nodes that contains us (result of GetEnclosing).
	mutable std::vector<const GraphNode*> navNodes;
	mutable bool                          needToEvaluateNavNodes;
};

#ifdef _DEBUG
typedef std::vector<ObstacleData> Obstacles;
#else
typedef DynArray<ObstacleData>    Obstacles;
#endif

inline int GraphNode::GetLinkIndex(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* destNode) const
{
	int linkIndexIndex = -1;
	int i = 0;
	for (unsigned link = firstLinkIndex; link; link = linkManager.GetNextLink(link), ++i)
		if (nodeManager.GetNode(linkManager.GetNextNode(link)) == destNode)
			linkIndexIndex = i;
	return linkIndexIndex;
}

inline unsigned GraphNode::GetLinkTo(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* destNode) const
{
	unsigned correctLink = 0;
	for (unsigned link = firstLinkIndex; link; link = linkManager.GetNextLink(link))
		if (nodeManager.GetNode(linkManager.GetNextNode(link)) == destNode)
			correctLink = link;
	return correctLink;
}

inline int GraphNode::GetLinkCount(CGraphLinkManager& linkManager) const
{
	int count = 0;
	for (unsigned link = firstLinkIndex; link; link = linkManager.GetNextLink(link))
		++count;
	return count;
}

inline void GraphNode::RemoveLinkTo(CGraphLinkManager& linkManager, unsigned nodeIndex)
{
	unsigned prevLink = 0;
	for (unsigned link = firstLinkIndex; link; )
	{
		unsigned nextLink = linkManager.GetNextLink(link);
		if (linkManager.GetNextNode(link) == nodeIndex)
		{
			if (!prevLink)
				firstLinkIndex = nextLink;
			else
				linkManager.SetNextLink(prevLink, nextLink);
		}
		prevLink = link;
		link = nextLink;
	}
}

#endif
