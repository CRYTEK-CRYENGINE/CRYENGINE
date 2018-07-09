// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IslandConnections.h"
#include <CryAISystem/IPathfinder.h>
#include "../NavigationSystem/NavigationSystem.h"
#include "OffGridLinks.h"
#ifdef CRYAISYSTEM_DEBUG
	#include "DebugDrawContext.h"
#endif

void MNM::IslandConnections::SetOneWayOffmeshConnectionBetweenIslands(const MNM::GlobalIslandID fromIsland, const Link& link)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
	AILogCommentID("<MNM:OffMeshLink>", "IslandConnections::SetOneWayConnectionBetweenIsland from %u to %u, linkId %u", fromIsland, link.toIsland, link.offMeshLinkID);
#endif

	TLinksVector& links = m_islandConnections[fromIsland];
	stl::push_back_unique(links, link);
}

void MNM::IslandConnections::SetTwoWayConnectionBetweenIslands(const MNM::GlobalIslandID islandId1, const MNM::AreaAnnotation islandAnnotation1, const MNM::GlobalIslandID islandId2, const MNM::AreaAnnotation islandAnnotation2, const int connectionsChange)
{
	TLinksVector& firstIslandLinks = m_islandConnections[islandId1];
	TLinksVector::iterator findIt = std::find(firstIslandLinks.begin(), firstIslandLinks.end(), Link(0, 0, islandId2, islandAnnotation2, 0, 0));
	if (findIt != firstIslandLinks.end())
	{
		Link& link = *findIt;
		link.connectionsCount += connectionsChange;
		CRY_ASSERT(link.connectionsCount >= 0);
		if (link.connectionsCount == 0)
		{
			firstIslandLinks.erase(findIt);
		}
		else
		{
			link.toIslandAnnotation = islandAnnotation2;
		}
	}
	else
	{
		CRY_ASSERT(connectionsChange > 0);
		firstIslandLinks.emplace_back(Link(0, 0, islandId2, islandAnnotation2, 0, connectionsChange));
	}
	
	TLinksVector& secondIslandLinks = m_islandConnections[islandId2];
	findIt = std::find(secondIslandLinks.begin(), secondIslandLinks.end(), Link(0, 0, islandId1, islandAnnotation1, 0, 0));
	if (findIt != secondIslandLinks.end())
	{
		Link& link = *findIt;
		link.connectionsCount += connectionsChange;
		CRY_ASSERT(link.connectionsCount >= 0);
		if (link.connectionsCount == 0)
		{
			secondIslandLinks.erase(findIt);
		}
		else
		{
			link.toIslandAnnotation = islandAnnotation1;
		}
	}
	else
	{
		CRY_ASSERT(connectionsChange > 0);
		secondIslandLinks.emplace_back(Link(0, 0, islandId1, islandAnnotation1, 0, connectionsChange));
	}
}

void MNM::IslandConnections::RemoveOneWayConnectionBetweenIslands(const MNM::GlobalIslandID fromIsland, const Link& link)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
	AILogCommentID("<MNM:OffMeshLink>", "remove link %u of object %x", link.offMeshLinkID, link.objectIDThatCreatesTheConnection);
#endif

	TIslandConnectionsMap::iterator linksIt = m_islandConnections.find(fromIsland);
	if (linksIt != m_islandConnections.end())
	{
		TLinksVector& links = linksIt->second;
		if (!links.empty())
		{
#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
			for (const Link& l : links)
			{
				if (l == link)
				{
					AILogCommentID("<MNM:OffMeshLink>", "object %x : from %u to %u, linkId %u", l.objectIDThatCreatesTheConnection, linksIt->first.GetStaticIslandID(), l.toIsland, l.offMeshLinkID);
				}
			}
#endif

			links.erase(std::remove(links.begin(), links.end(), link), links.end());
		}

		if (links.empty())
		{
			m_islandConnections.erase(linksIt);
		}
	}
}

struct IsLinkAssociatedWithObjectPredicate
{
	IsLinkAssociatedWithObjectPredicate(const uint32 objectId)
		: m_objectId(objectId)
	{}

	bool operator()(const MNM::IslandConnections::Link& linkToEvaluate)
	{
		return linkToEvaluate.objectIDThatCreatesTheConnection == m_objectId;
	}

	uint32 m_objectId;
};

void MNM::IslandConnections::RemoveAllIslandConnectionsForObject(const NavigationMeshID& meshID, const uint32 objectId)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
	AILogCommentID("<MNM:OffMeshLink>", "remove all links of object %x", objectId);
#endif

	TIslandConnectionsMap::iterator linksIt = m_islandConnections.begin();
	TIslandConnectionsMap::iterator linksEnd = m_islandConnections.end();
	for (; linksIt != linksEnd; )
	{
		if (NavigationMeshID(linksIt->first.GetNavigationMeshIDAsUint32()) == meshID)
		{
			TLinksVector& links = linksIt->second;

#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
			for (const Link& l : links)
			{
				if (IsLinkAssociatedWithObjectPredicate(objectId)(l))
				{
					AILogCommentID("<MNM:OffMeshLink>", "object %x : from %u to %u, linkId %u", l.objectIDThatCreatesTheConnection, linksIt->first.GetStaticIslandID(), l.toIsland, l.offMeshLinkID);
				}
			}
#endif

			links.erase(std::remove_if(links.begin(), links.end(), IsLinkAssociatedWithObjectPredicate(objectId)), links.end());
			if (links.empty())
			{
				linksIt = m_islandConnections.erase(linksIt);
				continue;
			}
		}

		++linksIt;
	}
}

struct SAcceptAllIslandsFilter
{
	inline bool PassFilter(const MNM::AreaAnnotation&) const { return true; }
};

bool MNM::IslandConnections::CanNavigateBetweenIslands(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const INavMeshQueryFilter* pFilter, TIslandsWay& way) const
{
	if (pFilter)
	{
		return CanNavigateBetweenIslandsInternal(pEntityToTestOffGridLinks, fromIsland, toIsland, *pFilter, way);
	}
	else
	{
		SAcceptAllIslandsFilter filter;
		return CanNavigateBetweenIslandsInternal(pEntityToTestOffGridLinks, fromIsland, toIsland, filter, way);
	}
}

template <typename TFilter>
bool MNM::IslandConnections::CanNavigateBetweenIslandsInternal(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const TFilter& filter, TIslandsWay& way) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const static MNM::GlobalIslandID invalidID(MNM::Constants::eGlobalIsland_InvalidIslandID);
	IF_UNLIKELY (fromIsland == invalidID || toIsland == invalidID)
		return false;

	if (fromIsland == toIsland)
		return true;

	const OffMeshNavigationManager* offMeshNavigationManager = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager();
	assert(offMeshNavigationManager);

	const size_t maxConnectedIsland = m_islandConnections.size();

	TIslandClosedSet closedSet;
	closedSet.reserve(maxConnectedIsland);
	IslandOpenList openList(maxConnectedIsland);
	openList.InsertElement(IslandNode(fromIsland, 0));
	TCameFromMap cameFrom;

	while (!openList.IsEmpty())
	{
		IslandNode currentItem(openList.PopBestElement());

		if (currentItem.id == toIsland)
		{
			ReconstructWay(cameFrom, fromIsland, toIsland, way);
			return true;
		}

		closedSet.push_back(currentItem.id);

		TIslandConnectionsMap::const_iterator currentIslandConnectionsIt = m_islandConnections.find(currentItem.id);
		if(currentIslandConnectionsIt == m_islandConnections.end())
			continue;

		const TLinksVector& links = currentIslandConnectionsIt->second;
		for (const Link& link : links)
		{
			if (!filter.PassFilter(link.toIslandAnnotation))
				continue;
			
			if (std::find(closedSet.begin(), closedSet.end(), link.toIsland) != closedSet.end())
				continue;

			if (link.offMeshLinkID != 0)
			{
				const OffMeshLink* offmeshLink = offMeshNavigationManager->GetOffMeshLink(link.offMeshLinkID);
				const bool canUseLink = pEntityToTestOffGridLinks ? (offmeshLink && offmeshLink->CanUse(pEntityToTestOffGridLinks, nullptr)) : true;

				if (!canUseLink)
					continue;
			}
			
			IslandNode nextIslandNode(link.toIsland, currentItem.cost + 1.0f);

			// At this point, if we have multiple connections to the same neighbour island,
			// we cannot detect which one is the one that allows us to have the actual shortest path
			// so to keep the code simple we will keep the first one

			if (cameFrom.find(nextIslandNode) != cameFrom.end())
				continue;

			cameFrom[nextIslandNode] = currentItem;
			openList.InsertElement(nextIslandNode);
		}
	}

	way.clear();
	return false;
}

void MNM::IslandConnections::Reset()
{
	m_islandConnections.clear();
}

void MNM::IslandConnections::ReconstructWay(const TCameFromMap& cameFromMap, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, TIslandsWay& way) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IslandNode currentIsland(toIsland, .0f);
	way.push_front(currentIsland.id);

	TCameFromMap::const_iterator element = cameFromMap.find(currentIsland);
	while (element != cameFromMap.end())
	{
		currentIsland = element->second;
		way.push_front(currentIsland.id);
		element = cameFromMap.find(currentIsland);
	}
}

#ifdef CRYAISYSTEM_DEBUG

void MNM::IslandConnections::DebugDraw() const
{
	const size_t totalGlobalIslandsRegisterd = m_islandConnections.size();
	const size_t memoryUsedByALink = sizeof(Link);
	size_t totalUsedMemory, totalEffectiveMemory;
	totalUsedMemory = totalEffectiveMemory = 0;
	TIslandConnectionsMap::const_iterator linksIt = m_islandConnections.begin();
	TIslandConnectionsMap::const_iterator linksEnd = m_islandConnections.end();
	for (; linksIt != linksEnd; ++linksIt)
	{
		const TLinksVector& links = linksIt->second;
		totalUsedMemory += links.size() * memoryUsedByALink;
		totalEffectiveMemory += links.capacity() * memoryUsedByALink;
	}

	const float conversionValue = 1024.0f;
	CDebugDrawContext dc;
	dc->Draw2dLabel(10.0f, 5.0f, 1.6f, Col_White, false, "Amount of global islands registered into the system %d", (int)totalGlobalIslandsRegisterd);
	dc->Draw2dLabel(10.0f, 25.0f, 1.6f, Col_White, false, "MNM::IslandConnections total used memory %.2fKB", totalUsedMemory / conversionValue);
	dc->Draw2dLabel(10.0f, 45.0f, 1.6f, Col_White, false, "MNM::IslandConnections total effective memory %.2fKB", totalEffectiveMemory / conversionValue);
}

#endif
