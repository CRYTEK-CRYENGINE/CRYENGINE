// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAMELOBBYCVARS_H__
#define __GAMELOBBYCVARS_H__

#include "GameLobbyData.h"
#include "GameBrowser.h"	// for USE_SESSION_SEARCH_SIMULATOR

class CGameLobbyCVars
{
	public:

		CGameLobbyCVars();
		~CGameLobbyCVars();

		static CGameLobbyCVars* Get()			{ return m_pThis; }

	public:

		float gl_initialTime;
		float gl_findGameTimeoutBase;
		float gl_findGameTimeoutPerPlayer;
		float gl_findGameTimeoutRandomRange;
		float gl_leaveGameTimeout;

		int gl_ignoreBadServers;
		int gl_allowLobbyMerging;
		int gl_allowEnsureBestHostCalls;

		float gl_timeTillEndOfGameForNoMatchMaking;
		float gl_timeBeforeStartOfGameForNoMatchMaking;
		float gl_skillChangeUpdateDelayTime;
		float gl_gameTimeRemainingRestrictLoad;
		float gl_startTimerMinTimeAfterPlayerJoined;

		int gl_startTimerMaxPlayerJoinResets;
		int gl_findGameNumJoinRetries;

		float gl_findGamePingMultiplier;
		float gl_findGamePlayerMultiplier;
		float gl_findGameLobbyMultiplier;
		float gl_findGameSkillMultiplier;
		float gl_findGameLanguageMultiplier;
		float gl_findGameRandomMultiplier;
		float gl_findGameStandardVariantMultiplier;
		float gl_findGamePingScale;
		float gl_findGameIdealPlayerCount;

#if GAMELOBBY_USE_COUNTRY_FILTERING
		float gl_findGameExpandSearchTime;
#endif

		float gl_hostMigrationEnsureBestHostDelayTime;
		float gl_hostMigrationEnsureBestHostOnStartCountdownDelayTime;
		float gl_hostMigrationEnsureBestHostOnReturnToLobbyDelayTime;
		float gl_hostMigrationEnsureBestHostGameStartMinimumTime;

		int gl_precachePaks;

		float gl_slotReservationTimeout;

#ifndef _RELEASE
		int gl_debug;
		int gl_voteDebug;
		int gl_voip_debug;
		int gl_skipPreSearch;
		int gl_dummyUserlist;
		int gl_dummyUserlistNumTeams;
		int gl_debugBadServersList;
		int gl_debugBadServersTestPerc;
		int gl_debugForceLobbyMigrations;
		int gl_debugLobbyRejoin;

		float gl_debugForceLobbyMigrationsTimer;
		float gl_debugLobbyRejoinTimer;
		float gl_debugLobbyRejoinRandomTimer;

		int gl_lobbyForceShowTeams;
		int gl_debugLobbyBreaksGeneral;
		int gl_debugLobbyHMAttempts;
		int gl_debugLobbyHMTerminations;
		int gl_debugLobbyBreaksHMShard;
		int gl_debugLobbyBreaksHMHints;
		int gl_debugLobbyBreaksHMTasks;
		int gl_resetWrongVersionProfiles;
		int gl_enableOfflinePlaylistVoting;
		int gl_hostMigrationUseAutoLobbyMigrateInPrivateGames;
	  int gl_allowDevLevels;
#endif

#ifdef USE_SESSION_SEARCH_SIMULATOR
		int gl_searchSimulatorEnabled;
#endif

		int		gl_skip;
		int		gl_experimentalPlaylistRotationAdvance;
		int		gl_enablePlaylistVoting;
		int		gl_minPlaylistSizeToEnableVoting;
		int		gl_precacheLogging;

		float gl_votingCloseTimeBeforeStart;
		float gl_time;
		float gl_checkDLCBeforeStartTime;
		float gl_maxSessionNameTimeWithoutConnection;
		
		int gl_getServerFromDedicatedServerArbitrator;
		int gl_serverIsAllocatedFromDedicatedServerArbitrator;

#if defined(DEDICATED_SERVER)
		int   g_pingLimit;
		float g_pingLimitTimer;
#endif


	private:

		static CGameLobbyCVars *m_pThis;
};

#endif // __GAMELOBBYCVARS_H__
