// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LOBBYCVARS_H__
#define __LOBBYCVARS_H__

#pragma once

#include <CryLobby/CommonICryLobby.h>
#include <CrySystem/IConsole.h>

class CLobbyCVars
{
public:
	CLobbyCVars();
	~CLobbyCVars();

	static CLobbyCVars& Get() { return *m_pThis; }

	float minMicrophonNotificationInterval;
	float serverPingNotificationInterval;
	int   showMatchMakingTasks;
	int   fullStatusOnClient;
	int   lobbyDefaultPort;

#if NETWORK_HOST_MIGRATION
	float hostMigrationTimeout;
	int   netAutoMigrateHost;
	int   showNewHostHinting;
	int   showHostIdentification;
	int   netHostHintingNATTypeOverride;
	int   netHostHintingActiveConnectionsOverride;
	int   netHostHintingPingOverride;
	int   netHostHintingUpstreamBPSOverride;
	int   netPruneConnectionsAfterHostMigration;
	int   netAnticipatedNewHostLeaves;
	int   netAlternateDeferredDisconnect;
	#if ENABLE_HOST_MIGRATION_STATE_CHECK
	int   doHostMigrationStateCheck;
	#endif
#endif

#if ENABLE_CRYLOBBY_DEBUG_TESTS
	int cldErrorPercentage;
	int cldMinDelayTime;
	int cldMaxDelayTime;
	int cldEnable;
#endif

#if CRY_PLATFORM_DURANGO
	int p2pShowConnectionStatus;
#endif

#if CRY_PLATFORM_ORBIS
	int psnForceWorld;
	int psnBestWorldMaxRooms;
	int psnBestWorldNumRoomsBonus;
	f32 psnBestWorldNumRoomsPivot;
	int psnBestWorldPlayersPerRoomBonus;
	f32 psnBestWorldPlayersPerRoomPivot;
	int psnBestWorldRandomBonus;
	int psnShowRooms;
#endif

#if USE_STEAM
	int useSteamAsOnlineLobby;
	int resetSteamAchievementsOnBoot;
	int lobbySteamOnlinePort;
#endif // USE_STEAM

#if defined(DEDICATED_SERVER)
	int ReservedSlotsActive;
#endif

	ICVar* pDedicatedServerArbitratorIP;
	int    dedicatedServerArbitratorPort;

private:
#if NETWORK_HOST_MIGRATION
	static void EnsureBestHostForSessionCmd(IConsoleCmdArgs* pArgs);
#endif

#if USE_CRY_DEDICATED_SERVER_ARBITRATOR
	static void DedicatedServerArbitratorListFreeServersCmd(IConsoleCmdArgs* pArgs);
#endif

	static CLobbyCVars * m_pThis;
};

#endif // __LOBBYCVARS_H__
