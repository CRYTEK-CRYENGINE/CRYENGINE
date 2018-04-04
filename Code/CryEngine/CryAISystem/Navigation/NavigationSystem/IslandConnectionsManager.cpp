// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "IslandConnectionsManager.h"

IslandConnectionsManager::IslandConnectionsManager()
{
}

void IslandConnectionsManager::Reset()
{
	m_globalIslandConnections.Reset();
}

MNM::IslandConnections& IslandConnectionsManager::GetIslandConnections()
{
	return m_globalIslandConnections;
}

void IslandConnectionsManager::SetOneWayConnectionBetweenIsland(const MNM::GlobalIslandID fromIsland, const MNM::IslandConnections::Link& link)
{
	m_globalIslandConnections.SetOneWayConnectionBetweenIsland(fromIsland, link);
}

bool IslandConnectionsManager::AreIslandsConnected(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID startIsland, const MNM::GlobalIslandID endIsland) const
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	MNM::IslandConnections::TIslandsWay way;
	return m_globalIslandConnections.CanNavigateBetweenIslands(pEntityToTestOffGridLinks, startIsland, endIsland, way);
}

#ifdef CRYAISYSTEM_DEBUG

void IslandConnectionsManager::DebugDraw() const
{
	m_globalIslandConnections.DebugDraw();
}

#endif
