// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IslandConnections.h"
#include <CryAISystem/IPathfinder.h>
#include "../NavigationSystem/NavigationSystem.h"
#include "OffGridLinks.h"
#ifdef CRYAISYSTEM_DEBUG
	#include "DebugDrawContext.h"
#endif

namespace MNM
{

void IslandConnections::SetOneWayOffmeshConnectionBetweenIslands(const MNM::GlobalIslandID fromIsland, const Link& link)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
	AILogCommentID("<MNM:OffMeshLink>", "IslandConnections::SetOneWayConnectionBetweenIsland from %u to %u, linkId %u", fromIsland, link.toIsland, link.offMeshLinkID);
#endif

	TLinksVector& links = m_islandConnections[fromIsland];
	stl::push_back_unique(links, link);
}

void IslandConnections::SetTwoWayConnectionBetweenIslands(const MNM::GlobalIslandID islandId1, const MNM::AreaAnnotation islandAnnotation1, const MNM::GlobalIslandID islandId2, const MNM::AreaAnnotation islandAnnotation2, const int connectionsChange)
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

void IslandConnections::RemoveOneWayConnectionBetweenIslands(const MNM::GlobalIslandID fromIsland, const Link& link)
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

	bool operator()(const IslandConnections::Link& linkToEvaluate)
	{
		return linkToEvaluate.objectIDThatCreatesTheConnection == m_objectId;
	}

	uint32 m_objectId;
};

void IslandConnections::RemoveAllIslandConnectionsForObject(const NavigationMeshID& meshID, const uint32 objectId)
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

bool IslandConnections::CanNavigateBetweenIslands(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const INavMeshQueryFilter* pFilter) const
{
	if (pFilter)
	{
		return CanNavigateBetweenIslandsInternal(pEntityToTestOffGridLinks, fromIsland, toIsland, *pFilter);
	}
	else
	{
		SAcceptAllIslandsFilter filter;
		return CanNavigateBetweenIslandsInternal(pEntityToTestOffGridLinks, fromIsland, toIsland, filter);
	}
}

template <typename TFilter>
bool IslandConnections::CanNavigateBetweenIslandsInternal(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID fromIsland, const MNM::GlobalIslandID toIsland, const TFilter& filter) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IF_UNLIKELY(fromIsland == MNM::Constants::eGlobalIsland_InvalidIslandID || toIsland == MNM::Constants::eGlobalIsland_InvalidIslandID)
		return false;

	if (fromIsland == toIsland)
		return true;

	const OffMeshNavigationManager* offMeshNavigationManager = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager();
	CRY_ASSERT(offMeshNavigationManager);

	CRY_ASSERT_MESSAGE(fromIsland.GetNavigationMeshIDAsUint32() == toIsland.GetNavigationMeshIDAsUint32(), "Island connections between two different meshes isn't supported.");

	TIslandNodeStatesArray& islandNodeStates = PrepareIslandNodeStatesArray(fromIsland);
	
	TIslandOpenList openList;
	openList.push_back(fromIsland);
	islandNodeStates[fromIsland.GetStaticIslandID()] = IslandNodeState::Opened;

	while (!openList.empty())
	{
		MNM::GlobalIslandID currentGlobalID = openList.front();
		openList.pop_front();

		const MNM::StaticIslandID currentStaticID = currentGlobalID.GetStaticIslandID();
		
		islandNodeStates[currentStaticID] = IslandNodeState::Closed;

		TIslandConnectionsMap::const_iterator currentIslandConnectionsIt = m_islandConnections.find(currentGlobalID);
		if (currentIslandConnectionsIt == m_islandConnections.end())
			continue;

		const TLinksVector& links = currentIslandConnectionsIt->second;
		for (const Link& link : links)
		{
			const MNM::StaticIslandID toIslandStaticID = link.toIsland.GetStaticIslandID();
			
			if (islandNodeStates[toIslandStaticID] != IslandNodeState::None)
				continue;
			
			if (!filter.PassFilter(link.toIslandAnnotation))
				continue;
			
			if (link.offMeshLinkID != 0)
			{
				const OffMeshLink* offmeshLink = offMeshNavigationManager->GetOffMeshLink(link.offMeshLinkID);
				const bool canUseLink = pEntityToTestOffGridLinks ? (offmeshLink && offmeshLink->CanUse(pEntityToTestOffGridLinks, nullptr)) : true;

				if (!canUseLink)
					continue;
			}

			if (link.toIsland == toIsland)
			{
				return true;
			}

			openList.push_back(link.toIsland);
			islandNodeStates[toIslandStaticID] = IslandNodeState::Opened;
		}
	}
	return false;
}

IslandConnections::TIslandNodeStatesArray& IslandConnections::PrepareIslandNodeStatesArray(const MNM::GlobalIslandID fromIsland) const
{
	const NavigationMeshID fromMeshId(fromIsland.GetNavigationMeshIDAsUint32());
	const uint32 maxIslandIdx = gAIEnv.pNavigationSystem->GetMesh(fromMeshId).navMesh.GetIslands().GetTotalIslands() + 1;

	if (m_islandNodeStatesArrayCache.size() < maxIslandIdx)
	{
		m_islandNodeStatesArrayCache.resize(maxIslandIdx);
	}
	std::fill(m_islandNodeStatesArrayCache.begin(), m_islandNodeStatesArrayCache.begin() + maxIslandIdx - 1, IslandNodeState::None);

	return m_islandNodeStatesArrayCache;
}

void IslandConnections::Reset()
{
	m_islandConnections.clear();
	m_islandNodeStatesArrayCache.clear();
}

#ifdef CRYAISYSTEM_DEBUG

void IslandConnections::DebugDraw() const
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
	totalUsedMemory += sizeof(TIslandNodeStatesArray::value_type) * m_islandNodeStatesArrayCache.size();
	totalEffectiveMemory += sizeof(TIslandNodeStatesArray::value_type) * m_islandNodeStatesArrayCache.capacity();

	const float conversionValue = 1024.0f;
	CDebugDrawContext dc;
	dc->Draw2dLabel(10.0f, 5.0f, 1.6f, Col_White, false, "Amount of global islands registered into the system %d", (int)totalGlobalIslandsRegisterd);
	dc->Draw2dLabel(10.0f, 25.0f, 1.6f, Col_White, false, "MNM::IslandConnections total used memory %.2fKB", totalUsedMemory / conversionValue);
	dc->Draw2dLabel(10.0f, 45.0f, 1.6f, Col_White, false, "MNM::IslandConnections total effective memory %.2fKB", totalEffectiveMemory / conversionValue);
}

#endif

} //namespace MNM
