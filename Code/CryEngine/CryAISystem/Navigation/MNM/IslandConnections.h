// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IslandConnections_h__
#define __IslandConnections_h__

#pragma once

#include "MNM.h"
#include "OpenList.h"

namespace MNM
{
struct OffMeshNavigation;

class IslandConnections
{
public:
	struct Link
	{
		Link(const MNM::TriangleID _toTriangleID, const MNM::OffMeshLinkID _offMeshLinkID, const MNM::GlobalIslandID _toIsland, const MNM::AreaAnnotation toIslandAnnotation, uint32 _objectIDThatCreatesTheConnection, uint32 connectionsCount)
			: toTriangleID(_toTriangleID)
			, offMeshLinkID(_offMeshLinkID)
			, toIsland(_toIsland)
			, toIslandAnnotation(toIslandAnnotation)
			, objectIDThatCreatesTheConnection(_objectIDThatCreatesTheConnection)
			, connectionsCount(connectionsCount)
		{
		}

		inline bool operator==(const Link& rhs) const
		{
			return toIsland == rhs.toIsland && toTriangleID == rhs.toTriangleID && offMeshLinkID == rhs.offMeshLinkID &&
			       objectIDThatCreatesTheConnection == rhs.objectIDThatCreatesTheConnection;
		}

		MNM::GlobalIslandID toIsland;
		MNM::AreaAnnotation toIslandAnnotation;
		uint32              connectionsCount;

		MNM::TriangleID     toTriangleID;
		MNM::OffMeshLinkID  offMeshLinkID;
		uint32              objectIDThatCreatesTheConnection;
	};

	IslandConnections() {}

	void Reset();

	void SetOneWayOffmeshConnectionBetweenIslands(const MNM::GlobalIslandID fromIsland, const Link& link);
	void RemoveOneWayConnectionBetweenIslands(const MNM::GlobalIslandID fromIsland, const Link& link);
	void RemoveAllIslandConnectionsForObject(const NavigationMeshID& meshID, const uint32 objectId);

	void SetTwoWayConnectionBetweenIslands(const MNM::GlobalIslandID islandId1, const MNM::AreaAnnotation islandAnnotation1, const MNM::GlobalIslandID islandId2, const MNM::AreaAnnotation islandAnnotation2, const int connectionChange);
	
	bool CanNavigateBetweenIslands(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const INavMeshQueryFilter* pFilter) const;

#ifdef CRYAISYSTEM_DEBUG
	void DebugDraw() const;
#endif

private:

	struct GlobalIslandIDHasher
	{
		size_t operator()(const MNM::GlobalIslandID& value) const
		{
			std::hash<uint64> hasher;
			return hasher(value.id);
		}
	};

	enum class IslandNodeState : uint8
	{
		None = 0x00,
		Opened = 0x01,
		Closed = 0x02,
	};

	typedef stl::STLPoolAllocator_ManyElems<MNM::GlobalIslandID, stl::PoolAllocatorSynchronizationSinglethreaded> TIslandOpenListAllocator;

	typedef std::deque<MNM::GlobalIslandID, TIslandOpenListAllocator>                   TIslandOpenList;
	typedef std::vector<Link>                                                           TLinksVector;
	typedef std::unordered_map<MNM::GlobalIslandID, TLinksVector, GlobalIslandIDHasher> TIslandConnectionsMap;
	typedef std::vector<IslandNodeState>                                                TIslandNodeStatesArray;

	template <typename TFilter>
	bool CanNavigateBetweenIslandsInternal(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const TFilter& filter) const;

	TIslandNodeStatesArray& PrepareIslandNodeStatesArray(const MNM::GlobalIslandID fromIsland) const;

	TIslandConnectionsMap m_islandConnections;

	// Cached array of island node states used in CanNavigateBetweenIslands function. Made as member variable to prevent allocation
	// of possibly big sized array every time the function is called.
	mutable TIslandNodeStatesArray m_islandNodeStatesArrayCache;
};
}

#endif // __IslandConnectionsManager_h__
