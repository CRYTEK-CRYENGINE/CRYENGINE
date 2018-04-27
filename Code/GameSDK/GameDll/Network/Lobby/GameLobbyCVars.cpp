// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameLobbyCVars.h"

#define GAME_LOBBY_IGNORE_BAD_SERVERS_LIST 0

CGameLobbyCVars* CGameLobbyCVars::m_pThis = NULL;

CGameLobbyCVars::CGameLobbyCVars()
{
	m_pThis = this;

	REGISTER_CVAR(gl_initialTime, 10.f, 0, "How long you spend in the lobby on a newly created lobby");
	REGISTER_CVAR(gl_findGameTimeoutBase, 3.f, 0, "How long to wait for results when finding a game");
	REGISTER_CVAR(gl_findGameTimeoutPerPlayer, 5.f, 0, "Extension to findGameTimeout for each player in session");
	REGISTER_CVAR(gl_findGameTimeoutRandomRange, 5.f, 0, "Randomization for the findGameTimeout");
	REGISTER_CVAR(gl_leaveGameTimeout, 10.f, 0, "Timeout for waiting for other players to leave the game before leaving ourselves");
	REGISTER_CVAR(gl_ignoreBadServers, GAME_LOBBY_IGNORE_BAD_SERVERS_LIST, 0, "Don't ignore bad servers (ones we have failed to connect to before)");
	REGISTER_CVAR(gl_allowLobbyMerging, 1, 0, "Set to 0 to stop matchmaking games from attempting to merge");
	REGISTER_CVAR(gl_allowEnsureBestHostCalls, 1, 0, "Set to 0 to stop the game doing pushed lobby-migrations");
	REGISTER_CVAR(gl_timeTillEndOfGameForNoMatchMaking, 120.f, 0, "Amount of game time remaining in which no matchmaking should occur");
	REGISTER_CVAR(gl_timeBeforeStartOfGameForNoMatchMaking, 5.f, 0, "Amount of time at the end of the start countdown before we disable matchmaking (enabled after InGame is hit)");
	REGISTER_CVAR(gl_skillChangeUpdateDelayTime, 3.f, 0, "Amount of time after detecting a change in skill ranking, before we call SessionUpdate");
	REGISTER_CVAR(gl_gameTimeRemainingRestrictLoad, 30.f, 0, "Don't start loading a level if there's only a limited amount of time remaining");
	REGISTER_CVAR(gl_startTimerMinTimeAfterPlayerJoined, 10.f, 0, "Minimum time before a game can start after a player has joined");
	REGISTER_CVAR(gl_startTimerMaxPlayerJoinResets, 8, 0, "Amount of times the start timer can be reset due to players joining before they are ignored");

	REGISTER_CVAR(gl_findGameNumJoinRetries, 2, 0, "Number of times to retry joining before creating our own game");
	REGISTER_CVAR(gl_findGamePingMultiplier, 2.f, 0, "Multiplier for ping submetric");
	REGISTER_CVAR(gl_findGamePlayerMultiplier, 1.f, 0, "Multiplier for player submetric");
	REGISTER_CVAR(gl_findGameLobbyMultiplier, 0.25f, 0, "Multiplier for lobby state submetric");
	REGISTER_CVAR(gl_findGameSkillMultiplier, 1.f, 0, "Multiplier for skill submetric");
	REGISTER_CVAR(gl_findGameLanguageMultiplier, 0.5f, 0, "Multiplier for language submetric");
	REGISTER_CVAR(gl_findGameRandomMultiplier, 0.5f, 0, "Multiplier for random submetric");
	REGISTER_CVAR(gl_findGameStandardVariantMultiplier, 4.f, 0, "Multiplier for variant submetric if standard variant");
	REGISTER_CVAR(gl_findGamePingScale, 300.f, 0, "Amount to divide the ping by before clamping to 0->1");
	REGISTER_CVAR(gl_findGameIdealPlayerCount, 6.f, 0, "Minimum number of players required for full score in the player submetric");

#if GAMELOBBY_USE_COUNTRY_FILTERING
	REGISTER_CVAR(gl_findGameExpandSearchTime, 30.f, 0, "Amount of time to search the local region before expanding the search to all regions");
#endif

	REGISTER_CVAR(gl_hostMigrationEnsureBestHostDelayTime, 10.f, 0, "Time after a player joins before we call ensurebesthost");
	REGISTER_CVAR(gl_hostMigrationEnsureBestHostOnStartCountdownDelayTime, 3.f, 0, "Time after the game countdown starts before we call ensurebesthost");
	REGISTER_CVAR(gl_hostMigrationEnsureBestHostOnReturnToLobbyDelayTime, 5.f, 0, "Time after the game countdown starts before we call ensurebesthost");
	REGISTER_CVAR(gl_hostMigrationEnsureBestHostGameStartMinimumTime, 4.f, 0, "Minimum amount of time before the game starts that we're allowed to check for a better host");

	REGISTER_CVAR(gl_precachePaks, 1, 0, "Precache pak files in lobby");
	REGISTER_CVAR(gl_slotReservationTimeout, 5.f, 0, "How long it takes for slot reservations to time out");

#ifndef _RELEASE
	REGISTER_CVAR(gl_debug, 0, 0, "Turn on some debugging");
	REGISTER_CVAR(gl_voteDebug, 0, 0, "Turn on some map vote debugging");
	REGISTER_CVAR(gl_voip_debug, 0, 0, "Turn on game lobby voice debug");
	REGISTER_CVAR(gl_skipPreSearch, 0, 0, "Just create game before start searching");
	REGISTER_CVAR(gl_dummyUserlist, 0, 0, "Number of debug dummy users to display.");
	REGISTER_CVAR(gl_dummyUserlistNumTeams, 3, 0, "Sets the number of teams to split dummy players across, 0: Team 0,  1: Team 1,  2: Team 1&2,  3: Team 0,1&2.");
	REGISTER_CVAR(gl_debugBadServersList, 0, 0, "Set alternate servers to be bad");
	REGISTER_CVAR(gl_debugBadServersTestPerc, 50, 0, "Percentage chance of setting a bad server (if gl_debugBadServersList=1)");
	REGISTER_CVAR(gl_debugForceLobbyMigrations, 0, 0, "1=Force a lobby migration every gl_debugForceLobbyMigrationsTimer seconds");
	REGISTER_CVAR(gl_debugLobbyRejoin, 0, 0, "1=Leave the lobby and rejoin after gl_debugLobbyRejoinTimer seconds");
	REGISTER_CVAR(gl_debugForceLobbyMigrationsTimer, 12.5f, 0, "Time between forced lobby migrations, 2=Start the leave lobby timer after a host migration starts");
	REGISTER_CVAR(gl_debugLobbyRejoinTimer, 25.f, 0, "Time till auto leaving the session");
	REGISTER_CVAR(gl_debugLobbyRejoinRandomTimer, 2.f, 0, "Random element to leave game timer");
	REGISTER_CVAR(gl_lobbyForceShowTeams, 0, 0, "Force the lobby to display teams all the time");
	REGISTER_CVAR(gl_debugLobbyBreaksGeneral, 0, 0, "Counter: General lobby breaks");
	REGISTER_CVAR(gl_debugLobbyHMAttempts, 0, 0, "Counter: host migration attempts");
	REGISTER_CVAR(gl_debugLobbyHMTerminations, 0, 0, "Counter: host migration terminations");
	REGISTER_CVAR(gl_debugLobbyBreaksHMShard, 0, 0, "Counter: Host Migration sharding detected in lobby");
	REGISTER_CVAR(gl_debugLobbyBreaksHMHints, 0, 0, "Counter: Host Migration hinting error detected");
	REGISTER_CVAR(gl_debugLobbyBreaksHMTasks, 0, 0, "Counter: Host Migration task error detected");
	REGISTER_CVAR(gl_resetWrongVersionProfiles, 1, 0, "Reset player profiles rather than kicking players if the version doesn't match");
	REGISTER_CVAR(gl_enableOfflinePlaylistVoting, 0, VF_CHEAT, "Enabled voting on offline games, just to make testing easier. Requires gl_enablePlaylistVoting to also be set");
	REGISTER_CVAR(gl_hostMigrationUseAutoLobbyMigrateInPrivateGames, 0, VF_CHEAT, "1=Make calls to EnsureBestHost when in private games");
	REGISTER_CVAR(gl_allowDevLevels, 1, VF_CHEAT, "Allow levels which aren't on the standard level list or dlc");
#endif

#ifdef USE_SESSION_SEARCH_SIMULATOR
	REGISTER_CVAR(gl_searchSimulatorEnabled, 0, 0, "Enable/Disable the Session Search Simulator for testing Matchmaking");
	REGISTER_STRING("gl_searchSimulatorFilepath", NULL, 0, "Set the source XML file for the Session Search Simulator");
#endif

	REGISTER_CVAR(gl_skip, 0, 0, "Skips the game lobby");
	REGISTER_CVAR(gl_votingCloseTimeBeforeStart, 5.f, 0, "Playlist voting will close this many secs before game start countdown finishes");
	REGISTER_CVAR(gl_experimentalPlaylistRotationAdvance, 1, 0, "Testing code that gets the first level from the playlist and advances it when the server enters the lobby. Here so can be easily disabled! Currently needed for gl_enablePlaylistVoting");
	REGISTER_CVAR(gl_enablePlaylistVoting, 1, 0, "Enabled voting on which level to play next within playlists that support it. Currently also needs gl_experimentalPlaylistRotationAdvance");
	REGISTER_CVAR(gl_minPlaylistSizeToEnableVoting, 2, 0, "A playlist must contain at least this many levels for voting to be enabled, even when gl_enablePlaylistVoting is set. Note that setting it to 1 and having a playlist with 1 level in it will still work, but it'll present a vote with the same level as both voting candidates");
	REGISTER_CVAR(gl_time, 45.f, 0, "Time in lobby between games");
	REGISTER_CVAR(gl_checkDLCBeforeStartTime, 20.f, 0, "When the server game start countdown gets to this time between games it will do DLC checks to make sure it has the DLC needed for both the levels being voted on");
	REGISTER_CVAR(gl_maxSessionNameTimeWithoutConnection, 5.f, 0, "Time a session name is allowed to persist before being removed");
	REGISTER_CVAR_DEV_ONLY(gl_precacheLogging, 0, 0, "Debug logging for the pre-caching of pak files in lobby");

	REGISTER_CVAR(gl_getServerFromDedicatedServerArbitrator, 0, 0, "Should game lobby request server from dedicated server arbitrator");
	REGISTER_CVAR(gl_serverIsAllocatedFromDedicatedServerArbitrator, 0, 0, "Indicates if this server should be allocated by the dedicated arbitrator");

#if defined(DEDICATED_SERVER)
	REGISTER_CVAR(g_pingLimit, 0, 0, "Max ping a player can have before being kicked (0=disabled)");
	REGISTER_CVAR(g_pingLimitTimer, 15.f, 0, "Time after which a player will be kicked if they are over the specified ping limit");
#endif
}

CGameLobbyCVars::~CGameLobbyCVars()
{
	gEnv->pConsole->UnregisterVariable("gl_initialTime");
	gEnv->pConsole->UnregisterVariable("gl_findGameTimeoutBase");
	gEnv->pConsole->UnregisterVariable("gl_findGameTimeoutPerPlayer");
	gEnv->pConsole->UnregisterVariable("gl_findGameTimeoutRandomRange");
	gEnv->pConsole->UnregisterVariable("gl_leaveGameTimeout");
	gEnv->pConsole->UnregisterVariable("gl_ignoreBadServers");
	gEnv->pConsole->UnregisterVariable("gl_allowLobbyMerging");
	gEnv->pConsole->UnregisterVariable("gl_allowEnsureBestHostCalls");
	gEnv->pConsole->UnregisterVariable("gl_timeTillEndOfGameForNoMatchMaking");
	gEnv->pConsole->UnregisterVariable("gl_timeBeforeStartOfGameForNoMatchMaking");
	gEnv->pConsole->UnregisterVariable("gl_skillChangeUpdateDelayTime");
	gEnv->pConsole->UnregisterVariable("gl_gameTimeRemainingRestrictLoad");
	gEnv->pConsole->UnregisterVariable("gl_startTimerMinTimeAfterPlayerJoined");
	gEnv->pConsole->UnregisterVariable("gl_startTimerMaxPlayerJoinResets");

	gEnv->pConsole->UnregisterVariable("gl_findGameNumJoinRetries");
	gEnv->pConsole->UnregisterVariable("gl_findGamePingMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGamePlayerMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGameLobbyMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGameSkillMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGameLanguageMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGameRandomMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGameStandardVariantMultiplier");
	gEnv->pConsole->UnregisterVariable("gl_findGamePingScale");
	gEnv->pConsole->UnregisterVariable("gl_findGameIdealPlayerCount");

#if GAMELOBBY_USE_COUNTRY_FILTERING
	gEnv->pConsole->UnregisterVariable("gl_findGameExpandSearchTime");
#endif

	gEnv->pConsole->UnregisterVariable("gl_hostMigrationEnsureBestHostDelayTime");
	gEnv->pConsole->UnregisterVariable("gl_hostMigrationEnsureBestHostOnStartCountdownDelayTime");
	gEnv->pConsole->UnregisterVariable("gl_hostMigrationEnsureBestHostOnReturnToLobbyDelayTime");
	gEnv->pConsole->UnregisterVariable("gl_hostMigrationEnsureBestHostGameStartMinimumTime");

	gEnv->pConsole->UnregisterVariable("gl_precachePaks");
	gEnv->pConsole->UnregisterVariable("gl_slotReservationTimeout");

#ifndef _RELEASE
	gEnv->pConsole->UnregisterVariable("gl_debug");
	gEnv->pConsole->UnregisterVariable("gl_voteDebug");
	gEnv->pConsole->UnregisterVariable("gl_voip_debug");
	gEnv->pConsole->UnregisterVariable("gl_skipPreSearch");
	gEnv->pConsole->UnregisterVariable("gl_dummyUserlist");
	gEnv->pConsole->UnregisterVariable("gl_dummyUserlistNumTeams");
	gEnv->pConsole->UnregisterVariable("gl_debugBadServersList");
	gEnv->pConsole->UnregisterVariable("gl_debugBadServersTestPerc");
	gEnv->pConsole->UnregisterVariable("gl_debugForceLobbyMigrations");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyRejoin");
	gEnv->pConsole->UnregisterVariable("gl_debugForceLobbyMigrationsTimer");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyRejoinTimer");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyRejoinRandomTimer");
	gEnv->pConsole->UnregisterVariable("gl_lobbyForceShowTeams");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyBreaksGeneral");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyHMAttempts");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyHMTerminations");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyBreaksHMShard");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyBreaksHMHints");
	gEnv->pConsole->UnregisterVariable("gl_debugLobbyBreaksHMTasks");
	gEnv->pConsole->UnregisterVariable("gl_resetWrongVersionProfiles");
	gEnv->pConsole->UnregisterVariable("gl_enableOfflinePlaylistVoting");
	gEnv->pConsole->UnregisterVariable("gl_hostMigrationUseAutoLobbyMigrateInPrivateGames");
	gEnv->pConsole->UnregisterVariable("gl_allowDevLevels");
#endif

#ifdef USE_SESSION_SEARCH_SIMULATOR
	gEnv->pConsole->UnregisterVariable("gl_searchSimulatorEnabled");
	gEnv->pConsole->UnregisterVariable("gl_searchSimulatorFilepath");
#endif

	gEnv->pConsole->UnregisterVariable("gl_skip");
	gEnv->pConsole->UnregisterVariable("gl_votingCloseTimeBeforeStart");
	gEnv->pConsole->UnregisterVariable("gl_experimentalPlaylistRotationAdvance");
	gEnv->pConsole->UnregisterVariable("gl_enablePlaylistVoting");
	gEnv->pConsole->UnregisterVariable("gl_minPlaylistSizeToEnableVoting");
	gEnv->pConsole->UnregisterVariable("gl_time");
	gEnv->pConsole->UnregisterVariable("gl_checkDLCBeforeStartTime");
	gEnv->pConsole->UnregisterVariable("gl_maxSessionNameTimeWithoutConnection");
	gEnv->pConsole->UnregisterVariable("gl_precacheLogging");

	gEnv->pConsole->UnregisterVariable("gl_getServerFromDedicatedServerArbitrator");
	gEnv->pConsole->UnregisterVariable("gl_serverIsAllocatedFromDedicatedServerArbitrator");

#if defined(DEDICATED_SERVER)
	gEnv->pConsole->UnregisterVariable("g_pingLimit");
	gEnv->pConsole->UnregisterVariable("g_pingLimitTimer");
#endif

	m_pThis = nullptr;
}

//-------------------------------------------------------------------------
#undef GAME_LOBBY_DO_LOBBY_MERGING
