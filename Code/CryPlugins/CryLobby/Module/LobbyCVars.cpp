// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryLobby.h"
#include "LobbyCVars.h"
#include "CryMatchMaking.h"
#include "LAN/CryLANLobby.h"
#include "CryDedicatedServerArbitrator.h"

#if USE_STEAM
	#define STEAM_GAME_DEFAULT_PORT (64100)
#endif

#define LOBBY_DEFAULT_PORT (64090)

CLobbyCVars* CLobbyCVars::m_pThis = NULL;

CLobbyCVars::CLobbyCVars()
	: minMicrophonNotificationInterval(0.f)
	, serverPingNotificationInterval(0.f)
#if CRY_PLATFORM_ORBIS
	, psnBestWorldNumRoomsPivot(0.f)
	, psnBestWorldPlayersPerRoomPivot(0.f)
#endif
{
	// cppcheck-suppress memsetClassFloat
	memset(this, 0, sizeof(*this));

	m_pThis = this;

	REGISTER_CVAR2_DEV_ONLY("net_minMicrophoneNotificationInterval", &minMicrophonNotificationInterval, 1.f, 0, "minimum interval between microphone status notification send/requests");
	REGISTER_CVAR2_DEV_ONLY("net_serverpingnotificationinterval", &serverPingNotificationInterval, 10.0f, 0, "minimum interval between server ping notification sends");
	REGISTER_CVAR2_DEV_ONLY("net_show_matchmaking_tasks", &showMatchMakingTasks, 0, VF_DUMPTODISK, "Toggles whether active matchmaking tasks are displayed on screen");
	REGISTER_CVAR2_DEV_ONLY("net_fullStatusOnClient", &fullStatusOnClient, 0, VF_NULL, "if set, allows clients to view all connected users with status command");
	REGISTER_CVAR2("net_lobby_default_port", &lobbyDefaultPort, LOBBY_DEFAULT_PORT, VF_DUMPTODISK, "Modify the default port for lobbies (effects LAN Lobby currently)");

#if NETWORK_HOST_MIGRATION

	#ifdef _RELEASE
	REGISTER_CVAR2("net_migrate_timeout", &hostMigrationTimeout, 30.0f, VF_DUMPTODISK, "timeout for host migration failure");
	#else
	REGISTER_CVAR2("net_migrate_timeout", &hostMigrationTimeout, 45.0f, VF_DUMPTODISK, "timeout for host migration failure");
	#endif
	REGISTER_CVAR2("net_automigrate_host", &netAutoMigrateHost, 1, VF_DUMPTODISK, "Toggles whether server migration is automatically attempted");
	REGISTER_CVAR2("net_show_new_host_hinting", &showNewHostHinting, 0, VF_DUMPTODISK, "Toggles whether new host hinting displayed on screen");
	REGISTER_CVAR2("net_show_host_identification", &showHostIdentification, 0, VF_DUMPTODISK, "Toggles whether host identification is displayed on screen");
	REGISTER_CVAR2("net_hostHintingNATTypeOverride", &netHostHintingNATTypeOverride, 0, VF_DUMPTODISK, "Override the reported NAT type");
	REGISTER_CVAR2("net_hostHintingActiveConnectionsOverride", &netHostHintingActiveConnectionsOverride, 0, VF_DUMPTODISK, "Override the reported number of active connections");
	REGISTER_CVAR2("net_hostHintingPingOverride", &netHostHintingPingOverride, 0, VF_DUMPTODISK, "Override the reported ping for all active connections");
	REGISTER_CVAR2("net_hostHintingUpstreamBPSOverride", &netHostHintingUpstreamBPSOverride, 0, VF_DUMPTODISK, "Override the reported upstream bits per second");
	REGISTER_CVAR2("net_pruneConnectionsAfterHostMigration", &netPruneConnectionsAfterHostMigration, 1, VF_DUMPTODISK, "Prune connections from players who fail to migrate");
	REGISTER_CVAR2("net_anticipated_new_host_leaves", &netAnticipatedNewHostLeaves, 0, VF_DUMPTODISK, "If this machine is the anticipated new host, it leaves during host migration");
	REGISTER_CVAR2("net_alternate_deferred_disconnect", &netAlternateDeferredDisconnect, 1, VF_DUMPTODISK, "Enables alternate deferred disconnect code");
	#if ENABLE_HOST_MIGRATION_STATE_CHECK
	REGISTER_CVAR2("net_do_host_migration_state_check", &doHostMigrationStateCheck, 0, VF_DUMPTODISK, "Enables state checking after host migration. ");
	#endif

	REGISTER_COMMAND("net_ensure_best_host_for_session", EnsureBestHostForSessionCmd, 0, "net_ensure_best_host_for_session <session handle>");
#endif

#if ENABLE_CRYLOBBY_DEBUG_TESTS
	REGISTER_CVAR2("cld_enable", &cldEnable, 0, VF_CHEAT, "Enable cry lobby debug tests");
	REGISTER_CVAR2("cld_error_percentage", &cldErrorPercentage, 10, VF_CHEAT, "How often a cry lobby task will return with a time out error");
	REGISTER_CVAR2("cld_min_delay_time", &cldMinDelayTime, 2000, VF_CHEAT, "The minimum time a cry lobby task will be delayed before processing in milliseconds");
	REGISTER_CVAR2("cld_max_delay_time", &cldMaxDelayTime, 5000, VF_CHEAT, "The maximum time a cry lobby task will be delayed before processing in milliseconds");
#endif

#if CRY_PLATFORM_DURANGO
	REGISTER_CVAR2("net_p2p_showconnectionstatus", &p2pShowConnectionStatus, 0, VF_DUMPTODISK, "show peer to peer connection status");
#endif

#if CRY_PLATFORM_ORBIS
	REGISTER_CVAR2("net_psnForceWorld", &psnForceWorld, 0, VF_DUMPTODISK, "Connect to specific PSN World on current server (default is 0 for random).");
	REGISTER_CVAR2("net_psnBestWorldMaxRooms", &psnBestWorldMaxRooms, 2500, VF_CHEAT, "Maximum number of rooms per world");
	REGISTER_CVAR2("net_psnBestWorldNumRoomsBonus", &psnBestWorldNumRoomsBonus, 295, VF_CHEAT, "Number of rooms threshold bonus");
	REGISTER_CVAR2("net_psnBestWorldNumRoomsPivot", &psnBestWorldNumRoomsPivot, 0.9f, VF_CHEAT, "Optimal pivot highpoint in number of rooms range");
	REGISTER_CVAR2("net_psnBestWorldPlayersPerRoomBonus", &psnBestWorldPlayersPerRoomBonus, 500, VF_CHEAT, "Players-per-room threshold bonus");
	REGISTER_CVAR2("net_psnBestWorldPlayersPerRoomPivot", &psnBestWorldPlayersPerRoomPivot, 1.0f, VF_CHEAT, "Optimal pivot highpoint in PPR range");
	REGISTER_CVAR2("net_psnBestWorldRandomBonus", &psnBestWorldRandomBonus, 500, VF_CHEAT, "Random bonus for best world calculation");
	REGISTER_CVAR2("net_psnShowRooms", &psnShowRooms, 0, VF_CHEAT, "Display PSN server/world/room ID for active sessions");
#endif

#if USE_STEAM
	REGISTER_CVAR2_DEV_ONLY("net_lobby_steam_online_port", &lobbySteamOnlinePort, STEAM_GAME_DEFAULT_PORT, VF_NULL, "Modify the online port for Steam");
	REGISTER_CVAR2("net_useSteamAsOnlineLobby", &useSteamAsOnlineLobby, 0, VF_REQUIRE_APP_RESTART, "Defines if steam should be used or not");
	REGISTER_CVAR2("net_steam_resetAchievements", &resetSteamAchievementsOnBoot, 0, VF_REQUIRE_APP_RESTART, "if set to 1, it will reset achievements for Steam on app start");
#endif // USE_STEAM

#if defined(DEDICATED_SERVER)
	REGISTER_CVAR2("net_reserved_slot_system", &ReservedSlotsActive, 0, VF_DUMPTODISK, "Turns reserved slot system on/off");
#endif // defined(DEDICATED_SERVER)

	pDedicatedServerArbitratorIP = REGISTER_STRING("net_dedicatedServerArbitratorIP", "", VF_NULL, "");
	REGISTER_CVAR2("net_dedicatedServerArbitratorPort", &dedicatedServerArbitratorPort, 0, VF_NULL, "");

#if USE_CRY_DEDICATED_SERVER_ARBITRATOR
	REGISTER_COMMAND("net_dedicatedServerArbitratorListFreeServers", DedicatedServerArbitratorListFreeServersCmd, VF_NULL, "");
#endif
}

CLobbyCVars::~CLobbyCVars()
{
	m_pThis = NULL;
}

#if NETWORK_HOST_MIGRATION
//------------------------------------------------------------------------
void CLobbyCVars::EnsureBestHostForSessionCmd(IConsoleCmdArgs* pArgs)
{
	int argCount = pArgs->GetArgCount();
	if (argCount == 2)
	{
		CryLobbySessionHandle h = atoi(pArgs->GetArg(1));
		if (h != CryLobbyInvalidSessionHandle)
		{
			CCryLobby* pLobby = (CCryLobby*)CCryLobby::GetLobby();
			if (pLobby != NULL)
			{
				CCryMatchMaking* pMatchMaking = (CCryMatchMaking*)pLobby->GetMatchMaking();
				if (pMatchMaking != NULL)
				{
					CrySessionHandle gh = pMatchMaking->CreateGameSessionHandle(h, CryMatchMakingInvalidConnectionUID);
					pMatchMaking->SessionEnsureBestHost(gh, NULL, NULL, NULL);
				}
			}
		}
	}
	else
	{
		CryLog("Bad arguments for net_ensure_best_host_for_session");
	}
}
#endif

#if USE_CRY_DEDICATED_SERVER_ARBITRATOR
void CLobbyCVars::DedicatedServerArbitratorListFreeServersCmd(IConsoleCmdArgs* pArgs)
{
	if (gEnv->pLobby)
	{
		CCryLANLobbyService* pLobbyService = (CCryLANLobbyService*)gEnv->pLobby->GetLobbyService(eCLS_LAN);

		if (pLobbyService)
		{
			CCryDedicatedServerArbitrator* pArbitrator = pLobbyService->GetDedicatedServerArbitrator();

			if (pArbitrator)
			{
				pArbitrator->ListFreeServers();
			}
		}
	}
}
#endif
