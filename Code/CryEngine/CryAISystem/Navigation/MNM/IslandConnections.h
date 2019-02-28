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
	typedef stl::STLPoolAllocator<MNM::GlobalIslandID, stl::PoolAllocatorSynchronizationSinglethreaded> TIslandsWayAllocator;
	typedef std::list<MNM::GlobalIslandID, TIslandsWayAllocator>                                        TIslandsWay;

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
	bool CanNavigateBetweenIslands(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const INavMeshQueryFilter* pFilter, TIslandsWay& way) const;


	template <typename TFilter>
	bool CanNavigateBetweenIslandsInternal(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const TFilter& filter, TIslandsWay& way) const;

#ifdef CRYAISYSTEM_DEBUG
	void DebugDraw() const;
#endif

private:

	struct IslandNode
	{
		IslandNode()
			: id(MNM::Constants::eGlobalIsland_InvalidIslandID)
			, cost(.0f)
		{}

		IslandNode(const MNM::GlobalIslandID _id, const float _cost)
			: id(_id)
			, cost(_cost)
		{}

		IslandNode(const IslandNode& other)
			: id(other.id)
			, cost(other.cost)
		{}

		ILINE void operator=(const IslandNode& other)
		{
			id = other.id;
			cost = other.cost;
		}

		ILINE bool operator<(const IslandNode& other) const
		{
			return id < other.id;
		}

		ILINE bool operator==(const IslandNode& other) const
		{
			return id == other.id;
		}

		MNM::GlobalIslandID id;
		float               cost;
	};

	struct IsNodeLessCostlyPredicate
	{
		bool operator()(const IslandNode& firstNode, const IslandNode& secondNode) { return firstNode.cost < secondNode.cost; }
	};

	typedef OpenList<IslandNode, IsNodeLessCostlyPredicate>                                    IslandOpenList;

	typedef stl::STLPoolAllocator<std::pair<const IslandNode, IslandNode>, stl::PoolAllocatorSynchronizationSinglethreaded> TCameFromMapAllocator;
	typedef std::map<IslandNode, IslandNode, std::less<IslandNode>, TCameFromMapAllocator>     TCameFromMap;
	typedef std::vector<MNM::GlobalIslandID>                                                   TIslandClosedSet;

	void ReconstructWay(const TCameFromMap& cameFromMap, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, TIslandsWay& way) const;

	typedef std::vector<Link>                           TLinksVector;
	typedef std::map<MNM::GlobalIslandID, TLinksVector> TIslandConnectionsMap;
	TIslandConnectionsMap m_islandConnections;
};
}

#endif // __IslandConnectionsManager_h__
