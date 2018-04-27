// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IslandConnectionsManager_h__
#define __IslandConnectionsManager_h__

#pragma once

#include <CryAISystem/INavigationSystem.h>
#include "../MNM/IslandConnections.h"

class IslandConnectionsManager
{
public:
	IslandConnectionsManager();
	void                    Reset();

	MNM::IslandConnections& GetIslandConnections();

	void                    SetOneWayConnectionBetweenIsland(const MNM::GlobalIslandID fromIsland, const MNM::IslandConnections::Link& link);

	bool                    AreIslandsConnected(const IEntity* pEntityToTestOffGridLinks, const MNM::GlobalIslandID startIsland, const MNM::GlobalIslandID endIsland, const INavMeshQueryFilter* pFilter) const;

#ifdef CRYAISYSTEM_DEBUG
	void DebugDraw() const;
#endif

private:
	MNM::IslandConnections m_globalIslandConnections;
};

#endif // __IslandConnectionsManager_h__
