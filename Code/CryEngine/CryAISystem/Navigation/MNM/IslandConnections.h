// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IslandConnections_h__
#define __IslandConnections_h__

#pragma once

#include "OpenList.h"

namespace MNM
{
struct OffMeshNavigation;


//IslandConnections is represented by directed graph that consists of vertices (SIslandNode) and directed edges between them (SLink).
class IslandConnections
{
public:
	typedef LocalDynArray<MNM::StaticIslandID, 1024> ConnectedIslandsArray;

	IslandConnections() {}

	void Reset();
	void ResetForMesh(const NavigationMeshID meshId);

	void SetOneWayOffmeshConnectionBetweenIslands(const MNM::GlobalIslandID fromIslandId, const MNM::AreaAnnotation fromIslandAnnotation, const MNM::GlobalIslandID toIslandId, const MNM::AreaAnnotation toIslandAnnotation, const OffMeshLinkID offMeshLinkId, const TriangleID toTriangleId, const uint32 connectionObjectOwnerId);
	void RemoveOffMeshLinkConnection(const OffMeshLinkID offMeshLinkId);

	void SetTwoWayConnectionBetweenIslands(const MNM::GlobalIslandID islandId1, const MNM::AreaAnnotation islandAnnotation1, const MNM::GlobalIslandID islandId2, const MNM::AreaAnnotation islandAnnotation2, const int connectionChange);
	
	bool CanNavigateBetweenIslands(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const INavMeshQueryFilter* pFilter) const;

	void GetConnectedIslands(const MNM::GlobalIslandID seedIslandId, ConnectedIslandsArray& connectedIslandsArray) const;

#ifdef CRYAISYSTEM_DEBUG
	void DebugDraw() const;
#endif

private:

	/*
	SIslandLink is structure representing directed edge between island and target island. If there is any connection between two islands created, there are always two links between them -
	one going from one island to other and vice-versa. In the case of off-mesh link, outConnectionsCount or inConnectionsCount is always exactly 1. 
	*/ 
	struct SIslandLink
	{
		SIslandLink(const MNM::StaticIslandID toIsland, uint32 offMeshLinkID, MNM::AreaAnnotation toIslandAnnotation = 0, uint16 outConnectionsCount = 0, uint16 inConnectionsCount = 0)
			: toIsland(toIsland)
			, toIslandAnnotation(toIslandAnnotation)
			, outConnectionsCount(outConnectionsCount)
			, inConnectionsCount(inConnectionsCount)
			, offMeshLinkID(offMeshLinkID)
		{
		}

		inline bool operator==(const SIslandLink& rhs) const
		{
			return toIsland == rhs.toIsland && offMeshLinkID == rhs.offMeshLinkID;
		}

		inline bool IsOutgoing() const { return outConnectionsCount > 0; }
		inline bool IsIngoing() const { return inConnectionsCount > 0; }

		MNM::StaticIslandID toIsland;            // target island static id
		MNM::AreaAnnotation toIslandAnnotation;  // target island annotation for early filtering out target island node, so it doesn't need to be accessed and opened. The same value is stored in respective island node.
		uint16              outConnectionsCount; // number of triangle-triangle connections going from source island to target island
		uint16              inConnectionsCount;  // number of triangle-triangle connections going from target island to source island
		OffMeshLinkID       offMeshLinkID;       // non-zero only when the link is representing off-mesh link
	};

	// SIslandNode is a structure representing vertex in directed graph of islands connectivity.
	struct SIslandNode
	{
		SIslandNode()
			: outLinksCount(0)
			, inLinksCount(0)
		{}

		std::vector<SIslandLink> links; // links to all other islands that are connected (outgoing, ingoing or both)
		MNM::AreaAnnotation annotation; // island's annotation, used in reversed search before expanding the node 
		uint16 outLinksCount;           // number of outgoing links
		uint16 inLinksCount;            // number of ingoing links
	};

	// Structure used for storing additional off-mesh links data
	struct SOffMeshLinkData
	{
		SOffMeshLinkData(const MNM::GlobalIslandID fromIslandId, const TriangleID toTriangleId, const uint32 connectionObjectOwnerId)
			: fromIslandId(fromIslandId)
			, toTriangleId(toTriangleId)
			, connectionObjectOwnerId(connectionObjectOwnerId)
		{}

		MNM::GlobalIslandID fromIslandId; // global id of island, where the link starts
		TriangleID toTriangleId;          // target triangle id (currently not used, but kept for now)
		uint32 connectionObjectOwnerId;   // id of the object that created the link, entity id (currently not used, but kept for now)
	};

	struct GlobalIslandIDHasher
	{
		size_t operator()(const MNM::GlobalIslandID& value) const
		{
			std::hash<uint64> hasher;
			return hasher(value.id);
		}
	};

	enum EIslandNodeState : uint8
	{
		// OpenedForward and OpenedReversed are also used as indices to arrays
		OpenedForward   = 0,
		OpenedReversed  = 1,
		Closed          = 2,
		None            = 0xff,
	};

	typedef stl::STLPoolAllocator_ManyElems<MNM::GlobalIslandID, stl::PoolAllocatorSynchronizationSinglethreaded> TIslandOpenListAllocator;

	typedef std::deque<MNM::GlobalIslandID, TIslandOpenListAllocator>                   TIslandOpenList;
	typedef std::unordered_map<MNM::GlobalIslandID, SIslandNode, GlobalIslandIDHasher>  TIslandConnectionsMap;
	typedef std::unordered_map<OffMeshLinkID, SOffMeshLinkData>					        TIslandOffMeshLinkDataMap;
	typedef std::vector<EIslandNodeState>                                               TIslandNodeStatesArray;

	template <typename TFilter>
	bool CanNavigateBetweenIslandsInternal(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const TFilter& filter) const;
	void ModifyConnectionToIsland(SIslandNode& islandNode, const MNM::StaticIslandID toIslandId, const MNM::AreaAnnotation toIslandAnnotation, const OffMeshLinkID offMeshLinkId, const int outConnectionsCountChange, const int inConnectionsCountChange);

	TIslandNodeStatesArray& PrepareIslandNodeStatesArray(const MNM::GlobalIslandID fromIsland) const;

	TIslandConnectionsMap m_islandConnections;
	TIslandOffMeshLinkDataMap m_offMeshLinksMap;

	// Cached array of island node states used in CanNavigateBetweenIslands function. Made as member variable to prevent allocation
	// of possibly big sized array every time the function is called.
	mutable TIslandNodeStatesArray m_islandNodeStatesArrayCache;
};
}

#endif // __IslandConnectionsManager_h__
