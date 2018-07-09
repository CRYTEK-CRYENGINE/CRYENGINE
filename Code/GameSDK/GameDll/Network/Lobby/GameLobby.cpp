// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "GameLobby.h"
#include "GameLobbyManager.h"
#include "GameBrowser.h"
#include "GameServerLists.h"

#include <CryLobby/ICryLobbyUI.h>
#include <CryLobby/ICryMatchMaking.h>
#include <CryLobby/ICryVoice.h>
#include <CryGame/IGameFramework.h>
#include "IGameRulesSystem.h"
#include "IGameSessionHandler.h"
#include <CryLobby/ICryStats.h>
#include <CrySystem/File/IResourceManager.h>

#include "Game.h"
#include "Network/Squad/SquadManager.h"
#include <CryCore/CryEndian.h>

#include "GameCVars.h"
#include <CryString/StringUtils.h>
#include "Utility/CryWatch.h"
#include "Utility/CryHash.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "PlayerProgression.h"
#include "GameRules.h"
#include "GameRulesModules/GameRulesModulesManager.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "UI/UIManager.h"
#include "UI/WarningsManager.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/UICVars.h"

#include "PlaylistManager.h"
#include "PlaylistActivityTracker.h"

#include <CryCore/TypeInfo_impl.h>

#include "TelemetryCollector.h"
#include "DLCManager.h"
#include "Network/GameNetworkUtils.h"
#include "PersistantStats.h"
#include "Player.h"
#include "MatchMakingTelemetry.h"
#include "MatchMakingHandler.h"
#include "RecordingSystem.h"

#include "GameLobbyCVars.h"
#include "UI/ProfileOptions.h"
#include "../../UI/Menu3dModels/FrontEndModelCache.h"
#include "UI/UILobbyMP.h"
#include "UI/UIMultiPlayer.h"


#define GAME_LOBBY_DO_ENSURE_BEST_HOST			0
#define GAME_LOBBY_DO_LOBBY_MERGING					1
#define GAME_LOBBY_ALLOW_MIGRATION 0
#define GAME_LOBBY_USE_GAMERULES_SEARCH			0
#define GAME_LOBBY_DEDICATED_SERVER_DOES_MERGING 0
#define MAX_WRITEUSERDATA_USERS 6
#define ORIGINAL_MATCHMAKING_DESC "GAME SDK Release Matchmaking"
#define VOTING_EXTRA_DEBUG_OUTPUT 0

#if VOTING_EXTRA_DEBUG_OUTPUT
	#define VOTING_DBG_LOG(...)		CryLog(__VA_ARGS__);
	#define VOTING_DBG_WATCH(...)	CryWatch(__VA_ARGS__);
#else
	#define VOTING_DBG_LOG(...)		{}
	#define VOTING_DBG_WATCH(...)	{}
#endif

#ifndef _RELEASE
//-------------------------------------------------------------------------
void CGameLobby::CmdTestMuteTeam(IConsoleCmdArgs *pArgs)
{
	if (pArgs->GetArgCount() == 3)
	{
		uint8 teamId = uint8(atoi(pArgs->GetArg(1)));
		bool mute = (atoi(pArgs->GetArg(2)) != 0);

		CGameLobby *pLobby = g_pGame->GetGameLobby();

		CryLog("%s all players on team %i", (mute ? "Muting" : "Un-Muting"), teamId);
		pLobby->MutePlayersOnTeam(teamId, mute);
	}
	else
	{
		CryLog("usage: g_testMuteTeam <teamId (1 or 2)> <mute (1 or 0)>");
	}
}

//-------------------------------------------------------------------------
void CGameLobby::CmdCallEnsureBestHost(IConsoleCmdArgs *pArgs)
{
#if GAME_LOBBY_DO_ENSURE_BEST_HOST
	CGameLobby *pLobby = g_pGame->GetGameLobby();
	if (pLobby)
	{
#ifndef _RELEASE
		if (!s_pGameLobbyCVars->gl_hostMigrationUseAutoLobbyMigrateInPrivateGames)
		{
			// Force set the gl_hostMigrationUseAutoLobbyMigrateInPrivateGames cvar so that the command works in private games
			s_pGameLobbyCVars->gl_hostMigrationUseAutoLobbyMigrateInPrivateGames = 1;
		}
#endif

		pLobby->m_taskQueue.AddTask(CLobbyTaskQueue::eST_EnsureBestHost, true);
	}
#endif
}

//-------------------------------------------------------------------------
void CGameLobby::CmdFillReservationSlots(IConsoleCmdArgs *pArgs)
{
	CGameLobby *pLobby = g_pGame->GetGameLobby();
	if (pLobby)
	{
		const float currentTime = gEnv->pTimer->GetAsyncCurTime();
		for (int i = 0; i < CRY_ARRAY_COUNT(pLobby->m_slotReservations); ++ i)
		{
			pLobby->m_slotReservations[i].m_con.m_uid = (i + 1);
			pLobby->m_slotReservations[i].m_con.m_sid = (i + 1);
			pLobby->m_slotReservations[i].m_timeStamp = currentTime;
		}
	}
}

#endif

//-------------------------------------------------------------------------

namespace GameLobbyUtils
{
	bool IsValidMap(CGameLobby *pGameLobby, ILevelInfo *pLevelInfo, const char *pGameRules, string &outResult)
	{
		outResult = PathUtil::GetFileName(pLevelInfo->GetName());

		CryFixedStringT<32> mapName = pGameLobby->GetValidMapForGameRules(outResult.c_str(), pGameRules, false);

		return (!mapName.empty());
	}
};

/// Used by console auto completion.
struct SGLLevelNameAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const
	{
		ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();

		const int numLevels = pLevelSystem->GetLevelCount();

		int numMPLevels = 0;

		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if (pGameLobby)
		{
			const char *pGameRules = pGameLobby->GetCurrentGameModeName(NULL);
			if (pGameRules)
			{
				for (int i = 0; i < numLevels; ++ i)
				{
					ILevelInfo* pLevelInfo = pLevelSystem->GetLevelInfo(i);
					static string strResult;
					if (GameLobbyUtils::IsValidMap(pGameLobby, pLevelInfo, pGameRules, strResult))
					{
						++ numMPLevels;
					}
				}
			}
		}

		return numMPLevels;
	}

	virtual const char* GetValue( int nIndex ) const
	{
		ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();

		const int numLevels = pLevelSystem->GetLevelCount();

		int numFoundMPLevels = 0;

		// This is slow but it's only used in response to the user hitting tab while doing a gl_map command on the console
		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if (pGameLobby)
		{
			const char *pGameRules = pGameLobby->GetCurrentGameModeName(NULL);
			if (pGameRules)
			{
				for (int i = 0; i < numLevels; ++ i)
				{
					ILevelInfo* pLevelInfo = pLevelSystem->GetLevelInfo(i);
					static string strResult;

					if (GameLobbyUtils::IsValidMap(pGameLobby, pLevelInfo, pGameRules, strResult))
					{
						++ numFoundMPLevels;
						if (numFoundMPLevels > nIndex)
						{
							return strResult.c_str();
						}
					}
				}
			}
		}
		return "";
	};
};
// definition and declaration must be separated for devirtualization
SGLLevelNameAutoComplete gl_LevelNameAutoComplete;

//-------------------------------------------------------------------------
void CGameLobby::CmdDumpValidMaps(IConsoleCmdArgs *pArgs)
{
	ICVar *pGameRulesCVar = gEnv->pConsole->GetCVar("sv_gamerules");
	if (pGameRulesCVar)
	{
		const char *pGameRules = pGameRulesCVar->GetString();
		IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();

		pGameRules = pGameRulesSystem->GetGameRulesName(pGameRules);
		if (pGameRules)
		{
			CGameLobby *pGameLobby = g_pGame->GetGameLobby();
			if (pGameLobby)
			{
				ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
				const int numLevels = pLevelSystem->GetLevelCount();

				for (int i = 0; i < numLevels; ++ i)
				{
					ILevelInfo* pLevelInfo = pLevelSystem->GetLevelInfo(i);
					static string strResult;

					if (GameLobbyUtils::IsValidMap(pGameLobby, pLevelInfo, pGameRules, strResult))
					{
						// This is a reply to a console command, should be CryLogAlways
						CryLogAlways("%s", strResult.c_str());
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------

#define ONLINE_STATS_FRAGMENTED_PACKET_SIZE 1175	// need breathing room, was getting fragmented packets again :(

//-------------------------------------------------------------------------
#if 0 // old frontend
void CGameLobby::SFlashChatMessage::SendChatMessageToFlash(IFlashPlayer *pFlashPlayer, bool isInit)
{
	if (m_message.empty()==false)
	{
		SFlashVarValue pArgs[] = {
			m_name.c_str(),
			m_message.c_str(),
			CHAT_MESSAGE_POSTFIX,
			m_local,
			m_squaddie,
			isInit };

			pFlashPlayer->Invoke(FRONTEND_SUBSCREEN_PATH_SET("AddChatMessage"), pArgs, CRY_ARRAY_COUNT(pArgs));
	}
}
#endif
//-------------------------------------------------------------------------


volatile bool CGameLobby::s_bShouldBeSearching = false;
int CGameLobby::s_currentMMSearchID = 0;

#if !defined(_RELEASE)
threadID CGameLobby::s_mainThreadHandle;
#endif

CGameLobbyCVars* CGameLobby::s_pGameLobbyCVars = NULL;

//-------------------------------------------------------------------------
CGameLobby::CGameLobby( CGameLobbyManager* pMgr )
{
	CryLog("CGameLobby::CGameLobby()");

#if !defined(_RELEASE)
	s_mainThreadHandle = CryGetCurrentThreadId();
#endif

	s_pGameLobbyCVars = CGameLobbyCVars::Get();

#ifndef _RELEASE
	REGISTER_COMMAND("g_testMuteTeam", CmdTestMuteTeam, 0, nullptr);
	REGISTER_COMMAND("g_callEnsureBestHost", CmdCallEnsureBestHost, 0, nullptr);
	REGISTER_COMMAND("g_fillReservationSlots", CmdFillReservationSlots, 0, nullptr);
#endif
	REGISTER_COMMAND("gl_advancePlaylist", CmdAdvancePlaylist, 0, nullptr);
	REGISTER_COMMAND("gl_dumpValidMaps", CmdDumpValidMaps, 0, nullptr);

	m_hasReceivedMapCommand = false;

	m_startTimer = s_pGameLobbyCVars->gl_initialTime;
	m_findGameTimeout = GetFindGameTimeout();
	m_lastUserListUpdateTime = 0.f;
	m_timeTillCallToEnsureBestHost = -1.f;
	m_hasReceivedSessionQueryCallback = false;
	m_hasReceivedStartCountdownPacket = false;
	m_hasReceivedPlaylistSync = false;
	m_gameHadStartedWhenPlaylistRotationSynced = false;
	m_initialStartTimerCountdown = false;
	m_joinCommand[0] = '\0';
	m_findGameResults.clear();
	m_currentSessionId = CrySessionInvalidID;
	m_nextSessionId = CrySessionInvalidID;
	m_autoVOIPMutingType = eLAVT_start;
	m_localVoteStatus = eLVS_awaitingCandidates;
	VOTING_DBG_LOG("[tlh] set m_localVoteStatus [1] to eLVS_awaitingCandidates");
	m_shouldFindGames = false;
	m_privateGame = false;
	m_passwordGame = false;
	CRY_ASSERT(pMgr);
	m_gameLobbyMgr = pMgr;
	m_reservationList = NULL;
	m_squadReservation = false;
	m_bMigratedSession = false;
	m_bSessionStarted = false;
	m_bCancelling = false;
	m_bQuitting = false;
	m_bNeedToSetAsElegibleForHostMigration = false;
	m_bPlaylistHasBeenAdvancedThroughConsole = false;
	m_networkedVoteStatus = eLNVS_NotVoted;
	m_lastActiveStatus = eAS_Lobby;
	m_timeTillUpdateSession = 0.f;
	m_bSkipCountdown = false;
	m_bServerUnloadRequired = false;
	m_bChoosingGamemode = false;
	m_bHasUserList = false;
	m_isMidGameLeaving = false;
	m_findGameNumRetries = 0;
	m_numPlayerJoinResets = 0;
	m_bRetryIfPassworded = false;
	m_quickMatchRanked = false;
	m_bCanAbortLoading = false;
	m_bAllowPakPrecaching = true;
#if defined(DEDICATED_SERVER)
	m_timeSinceLastPingCheck = 0.f;
#endif

	for (int i=0; i<CRY_ARRAY_COUNT(m_slotReservations); ++i)
	{
		m_slotReservations[i].m_con = CryMatchMakingInvalidConnectionUID;
		m_slotReservations[i].m_timeStamp = 0.f;
	}

	ClearChatMessages();

	m_votingFlashInfo.Reset();
	m_votingCandidatesFlashInfo.Reset();
	m_bMatchmakingSession = false;
	m_bWaitingForGameToFinish = false;
	m_allowRemoveUsers = true;
	m_bHasReceivedVoteOptions = false;

	// SInternalLobbyData
	m_server = false;
	m_state = eLS_None;
	m_requestedState = eLS_None;
	m_currentSession = CrySessionInvalidHandle;
	m_nameList.Clear();
	m_sessionFavouriteKeyId = INVALID_SESSION_FAVOURITE_ID;
	m_endGameResponses = 0;
	m_sessionUserDataDirty = false;
	m_squadDirty = false;
	m_leftVoteChoice.Reset();
	m_rightVoteChoice.Reset();
	m_highestLeadingVotesSoFar = 0;
	m_leftHadHighestLeadingVotes = false;
	m_votingEnabled = false;
	m_votingClosed = false;
	m_leftWinsVotes = false;
	m_isLeaving = false;
	m_pendingConnectSessionId = CrySessionInvalidID;
	m_leaveGameTimeout = 0.f;
	m_pendingReservationId = CryMatchMakingInvalidConnectionUID;
	m_currentTaskId = CryLobbyInvalidTaskID;
	m_stateHasChanged = false;
	m_playListSeed = 0;
	m_dedicatedserverip = 0;
	m_dedicatedserverport = 0;
	m_sessionHostOnly = false;
	m_bAllocatedServer = false;
	m_bIsAutoStartingGame = false;

	m_countdownStage = eCDS_WaitingForPlayers;
	m_teamBalancing.Init(&m_nameList);
	m_teamBalancing.SetLobbyPlayerCounts(MAX_PLAYER_LIMIT);		// Default to max lobby size and 0 spectators

	
	memset(&m_detailedServerInfo, 0, sizeof(m_detailedServerInfo));

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	pLobby->RegisterEventInterest(eCLSE_UserPacket, CGameLobby::MatchmakingSessionUserPacketCallback, this);
	pLobby->RegisterEventInterest(eCLSE_RoomOwnerChanged, CGameLobby::MatchmakingSessionRoomOwnerChangedCallback, this);
	pLobby->RegisterEventInterest(eCLSE_SessionUserJoin, CGameLobby::MatchmakingSessionJoinUserCallback, this);
	pLobby->RegisterEventInterest(eCLSE_SessionUserLeave, CGameLobby::MatchmakingSessionLeaveUserCallback, this);
	pLobby->RegisterEventInterest(eCLSE_SessionUserUpdate, CGameLobby::MatchmakingSessionUpdateUserCallback, this);
	pLobby->RegisterEventInterest(eCLSE_SessionClosed, CGameLobby::MatchmakingSessionClosedCallback, this);
	pLobby->RegisterEventInterest(eCLSE_KickedFromSession, CGameLobby::MatchmakingSessionKickedCallback, this);
	pLobby->RegisterEventInterest(eCLSE_KickedHighPing, CGameLobby::MatchmakingSessionKickedHighPingCallback, this);
	pLobby->RegisterEventInterest(eCLSE_KickedReservedUser, CGameLobby::MatchmakingSessionKickedReservedUserCallback, this);
	pLobby->RegisterEventInterest(eCLSE_ForcedFromRoom, CGameLobby::MatchmakingForcedFromRoomCallback, this);
	pLobby->RegisterEventInterest(eCLSE_SessionRequestInfo, CGameLobby::MatchmakingSessionDetailedInfoRequestCallback, this);
	pLobby->RegisterEventInterest(eCLSE_DedicatedServerSetup, CGameLobby::MatchmakingDedicatedServerSetup, this);
	pLobby->RegisterEventInterest(eCLSE_DedicatedServerRelease, CGameLobby::MatchmakingDedicatedServerRelease, this);
	pLobby->RegisterEventInterest(eCLSE_InviteAccepted, CGameLobby::InviteAcceptedCallback, this);
	gEnv->pNetwork->AddHostMigrationEventListener(this, "GameLobby", ELPT_PostEngine);

	// Register as System Event Listener.
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CGameLobby");

#if !defined(_RELEASE)
	m_timeTillAutoLeaveLobby = 0.f;
	m_failedSearchCount = 0;
#endif //!defined(_RELEASE)

#if ENABLE_CHAT_MESSAGES
	REGISTER_COMMAND("gl_say", CmdChatMessage, 0, "Send a chat message");
	REGISTER_COMMAND("gl_teamsay", CmdChatMessageTeam, 0, "Send a chat message to team");
#endif

	REGISTER_COMMAND("gl_StartGame", CmdStartGame, 0, "force start a game");
	REGISTER_COMMAND("gl_Map", CmdSetMap, 0, "Set map for the lobby");
	REGISTER_COMMAND("gl_GameRules", CmdSetGameRules, 0, "Set the game rules for the lobby");
	REGISTER_COMMAND("gl_Vote", CmdVote, 0, "Vote for next map in lobby (left or right)");
	gEnv->pConsole->RegisterAutoComplete("gl_Map", &gl_LevelNameAutoComplete);

	m_isTeamGame = false;

	if (CWarningsManager* pWarningsManager = g_pGame->GetWarnings())
		m_DLCServerStartWarningId = g_pGame->GetWarnings()->GetWarningId("DLCServerStartWarning");
	else
		m_DLCServerStartWarningId = INVALID_HUDWARNING_ID;

	m_badServersHead = 0;

	m_taskQueue.Init(CGameLobby::TaskStartedCallback, this);

	m_startTimerLength = s_pGameLobbyCVars->gl_initialTime;

	m_profanityTask = CryLobbyInvalidTaskID;

	m_isAsymmetricGame = false;
	m_replayMapWithTeamsSwitched = false;

	m_allocatedServerUID = CryMatchMakingInvalidConnectionUID;

#if !defined(_RELEASE)
	if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
	{
		gEnv->pConsole->GetCVar("net_hostHintingNATTypeOverride")->Set(1);
		gEnv->pConsole->GetCVar("net_hostHintingActiveConnectionsOverride")->Set(1);
		CryLog("setting net_hostHintingActiveConnectionsOverride to 1");
	}
#endif
}

//-------------------------------------------------------------------------
CGameLobby::~CGameLobby()
{
	CryLog("CGameLobby::~CGameLobby() canceling any pending tasks, Lobby: %p", this);
	CancelAllLobbyTasks();

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	pLobby->UnregisterEventInterest(eCLSE_UserPacket, CGameLobby::MatchmakingSessionUserPacketCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_RoomOwnerChanged, CGameLobby::MatchmakingSessionRoomOwnerChangedCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_SessionUserJoin, CGameLobby::MatchmakingSessionJoinUserCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_SessionUserLeave, CGameLobby::MatchmakingSessionLeaveUserCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_SessionUserUpdate, CGameLobby::MatchmakingSessionUpdateUserCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_SessionClosed, CGameLobby::MatchmakingSessionClosedCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_KickedFromSession, CGameLobby::MatchmakingSessionKickedCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_KickedHighPing, CGameLobby::MatchmakingSessionKickedHighPingCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_KickedReservedUser, CGameLobby::MatchmakingSessionKickedReservedUserCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_ForcedFromRoom, CGameLobby::MatchmakingForcedFromRoomCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_SessionRequestInfo, CGameLobby::MatchmakingSessionDetailedInfoRequestCallback, this);
	pLobby->UnregisterEventInterest(eCLSE_DedicatedServerSetup, CGameLobby::MatchmakingDedicatedServerSetup, this);
	pLobby->UnregisterEventInterest(eCLSE_DedicatedServerRelease, CGameLobby::MatchmakingDedicatedServerRelease, this);
	pLobby->UnregisterEventInterest(eCLSE_InviteAccepted, CGameLobby::InviteAcceptedCallback, this);
	gEnv->pNetwork->RemoveHostMigrationEventListener(this);

	// Unregister as System Event Listener.
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

	gEnv->pCodeCheckpointMgr->UnRegisterCheckpoint("GameLobby_JoinServer");

	m_gameLobbyMgr = NULL;
}

//-------------------------------------------------------------------------
void CGameLobby::SvFinishedGame(const float dt)
{
	m_bServerUnloadRequired = true;

	CRY_ASSERT(m_server);
	if(m_state == eLS_Game)
	{
		SetState(eLS_EndSession);
		m_startTimer = 0.0f;
	}
	else if(m_state == eLS_GameEnded)
	{
		if(m_startTimer > 10.0f)
		{
			gEnv->pConsole->ExecuteString("unload", false, true);
			SetState(eLS_PostGame);
		}
		else
		{
			m_startTimer += dt;
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SetState(ELobbyState state)
{
	if(state != m_state)
	{
		CryLog("CGameLobby(%p)::SetState %d", this, state);
		INDENT_LOG_DURING_SCOPE();
		EnterState(m_state, state);
		m_stateHasChanged = true;
	}
}

//-------------------------------------------------------------------------
void CGameLobby::Update(float dt)
{
#if !defined(_RELEASE)
	if(s_pGameLobbyCVars->gl_debug)
	{
		CryWatch("GameLobby state: %d, StartedGameContext: %d", (int)m_state, g_pGame->GetIGameFramework()->StartedGameContext());
		CryWatch("gameRules=%s, map=%s, private=%s passworded=%s", m_currentGameRules.c_str(), m_currentLevelName.c_str(), (IsPrivateGame()?"true":"false"), (IsPasswordedGame()?"true":"false"));
		if (CPlaylistManager* pPlaylistMan=g_pGame->GetPlaylistManager())
		{
			CryWatch("playlist set=%s, id=%u", (pPlaylistMan->HavePlaylistSet() ? "true" : "false"), (pPlaylistMan->GetCurrentPlaylist() ? pPlaylistMan->GetCurrentPlaylist()->id : 0));
		}
		else
		{
			CryWatch("playlist [manager=NULL]");
		}
		CryWatch("IsAutoStarting %d", m_bIsAutoStartingGame);
	}

	if (s_pGameLobbyCVars->gl_voip_debug)
	{
		switch (m_autoVOIPMutingType)
		{
			case eLAVT_off:					CryWatch("Current mute state eLAVT_off");											break;
			case eLAVT_allButParty:	CryWatch("Current mute state eLAVT_allButParty");							break;
			case eLAVT_all:					CryWatch("Current mute state eLAVT_all");											break;
			default:								CryWatch("Current mute state %d", (int)m_autoVOIPMutingType);	break;
		};
	}
#endif

#ifdef _DEBUG
	static float white[] = {1.0f,1.0f,1.0f,1.0f};
	float ypos = 200.0f;
	if (gEnv->pRenderer)
	{
		if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
		{
			IRenderAuxText::Draw2dLabel(100, ypos, 3.0f, white, false, "HOST MIGRATION:");
			ypos += 30.0f;
			IRenderAuxText::Draw2dLabel(125, ypos, 2.0f, white, false, "Attempts (%i) / Terminations (%i)", s_pGameLobbyCVars->gl_debugLobbyHMAttempts, s_pGameLobbyCVars->gl_debugLobbyHMTerminations);
			ypos += 20.0f;
		}

		if (s_pGameLobbyCVars->gl_debugLobbyBreaksGeneral)
		{
			IRenderAuxText::Draw2dLabel(125, ypos, 2.0f, white, false, "HOST MIGRATION BREAKS DETECTED (%i)", s_pGameLobbyCVars->gl_debugLobbyBreaksGeneral);
			ypos += 20.0f;
		}
		if (s_pGameLobbyCVars->gl_debugLobbyBreaksHMShard)
		{
			IRenderAuxText::Draw2dLabel(125, ypos, 2.0f, white, false, "HOST MIGRATION SHARD BREAKS DETECTED (%i)", s_pGameLobbyCVars->gl_debugLobbyBreaksHMShard);
			ypos += 20.0f;
		}
		if (s_pGameLobbyCVars->gl_debugLobbyBreaksHMHints)
		{
			IRenderAuxText::Draw2dLabel(125, ypos, 2.0f, white, false, "HOST MIGRATION HINTS BREAKS DETECTED (%i)", s_pGameLobbyCVars->gl_debugLobbyBreaksHMHints);
			ypos += 20.0f;
		}
		if (s_pGameLobbyCVars->gl_debugLobbyBreaksHMTasks)
		{
			IRenderAuxText::Draw2dLabel(125, ypos, 2.0f, white, false, "HOST MIGRATION TASKS BREAKS DETECTED (%i)", s_pGameLobbyCVars->gl_debugLobbyBreaksHMTasks);
			ypos += 20.0f;
		}
	}
#endif

	UpdateState();

	m_taskQueue.Update();

	m_nameList.Tick(dt);

#if !defined(_RELEASE)
	if(s_pGameLobbyCVars->gl_voip_debug)
	{
		const int nameSize = m_nameList.Size();
		for(int i = 0; i < nameSize; ++i)
		{
			CryWatch("\t%d - %s [uid:%u] mute:%d", i + 1, m_nameList.m_sessionNames[i].m_name, m_nameList.m_sessionNames[i].m_conId.m_uid, m_nameList.m_sessionNames[i].m_muted);
			CryUserID userId = m_nameList.m_sessionNames[i].m_userId;
			if (userId.IsValid())
			{
				ICryVoice *pCryVoice = gEnv->pNetwork->GetLobby()->GetVoice();
				uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();
				if (pCryVoice)
				{
					CryWatch("\t\tIsMuted %d IsExMuted %d", pCryVoice->IsMuted(userIndex, userId)?1:0, pCryVoice->IsMutedExternally(userIndex, userId)?1:0);
				}
			}
		}
	}
#endif

	if (m_isLeaving)
	{
		m_leaveGameTimeout -= dt;
		if (m_leaveGameTimeout <= 0.f)
		{
			CryLog("CGameLobby::Update() leave game timeout has occurred");
			LeaveSession(true, false);
		}
		return;
	}

	if(m_state == eLS_GameEnded)
	{
		if(m_endGameResponses >= (int)m_nameList.Size() - 1)
		{
			if (m_bServerUnloadRequired)
			{
				gEnv->pConsole->ExecuteString("unload", false, true);
			}
			SetState(eLS_PostGame);
		}
	}

	if (m_server)
	{
		if (g_pGame->GetHostMigrationState() == CGame::eHMS_NotMigrating)
		{
			const EActiveStatus currentStatus = GetActiveStatus(m_state);
			if (m_lastActiveStatus != currentStatus)
			{
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
				m_lastActiveStatus = currentStatus;
			}
		}

#if defined(DEDICATED_SERVER)
		UpdatePingKicker(dt);
#endif
	}

	if (g_pGame->GetIGameFramework()->StartedGameContext())
	{
		if( g_pGame->GetGameRules() != NULL && g_pGame->GetGameRules()->LevelNameCheckNeeded() && ReadyToCheckDLC())
		{
			CheckDLCRequirements();
			g_pGame->GetGameRules()->LevelNameCheckDone();
		}

		return;
	}

	if(m_state == eLS_PostGame)
	{
#if 0 // old frontend
		CFlashFrontEnd *pFlashFrontEnd = g_pGame->GetFlashMenu();
		if (pFlashFrontEnd)
		{
		pFlashFrontEnd->ScheduleInitialize(eFlM_Menu, eFFES_game_lobby, eFFES_game_lobby);
		}
#endif
		SetState(eLS_Lobby);
	}

	if(m_state == eLS_Lobby)
	{
		CRY_ASSERT(m_currentSession != CrySessionInvalidHandle);

		eHostMigrationState hostMigrationState = eHMS_Unknown;
		ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{

			ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
			if (pMatchmaking)
			{
				hostMigrationState = pMatchmaking->GetSessionHostMigrationState(m_currentSession);
			}
		}

#if !defined(_RELEASE)
		if(s_pGameLobbyCVars->gl_debug)
		{
			CryWatch("Session %d", m_currentSession);
			CryWatch("Server %d, Private %d, Online %d, Password %d", m_server, IsPrivateGame(), IsOnlineGame(), IsPasswordedGame());
			CryWatch("Hash %d", GameLobbyData::ConvertGameRulesToHash("HashTest"));
			if(!m_server)
			{
				CryWatch("JoinCommand %s", m_joinCommand);
			}

			if(m_userData[eLDI_Map].m_int32 != 0)
			{
				CryWatch("%s", GameLobbyData::GetMapFromHash(m_userData[eLDI_Map].m_int32));
			}

			if(m_userData[eLDI_Gamemode].m_int32 != 0)
			{
				CryWatch("%s", GameLobbyData::GetGameRulesFromHash(m_userData[eLDI_Gamemode].m_int32));
			}

			CryWatch("RequiredDLCs %d", m_userData[eLDI_RequiredDLCs].m_int32);
			CryWatch("Playlist %d", m_userData[eLDI_Playlist].m_int32);
			CryWatch("Variant %d", m_userData[eLDI_Variant].m_int32);
			CryWatch("Version %d", m_userData[eLDI_Version].m_int32);

			const int nameSize = m_nameList.Size();
			for(int i = 0; i < nameSize; ++i)
			{
				CryWatch("\t%d - %s [sid:%" PRIu64 " uid:%u]", i + 1, m_nameList.m_sessionNames[i].m_name, m_nameList.m_sessionNames[i].m_conId.m_sid, m_nameList.m_sessionNames[i].m_conId.m_uid);
			}
		}

		if ((s_pGameLobbyCVars->gl_debugLobbyRejoin == 1) || ((s_pGameLobbyCVars->gl_debugLobbyRejoin == 2) && m_migrationStarted))
		{
			m_timeTillAutoLeaveLobby -= dt;
			if (m_timeTillAutoLeaveLobby <= 0.f)
			{
				LeaveSession(true, false);
			}
		}
#endif

		if (m_squadDirty && (m_nameList.Size() > 0))
		{
			bool bCanUpdateUserData = false;
			uint32 squadLeaderUID = 0;
			CryUserID pSquadLeaderId = g_pGame->GetSquadManager()->GetSquadLeader();
			SCryMatchMakingConnectionUID squadLeaderConID;
			if (pSquadLeaderId == CryUserInvalidID)
			{
				bCanUpdateUserData = true;
			}
			else
			{
				// Need to make sure we know who our squad leader is before updating the user data
				if (GetConnectionUIDFromUserID(pSquadLeaderId, squadLeaderConID))
				{
					bCanUpdateUserData = true;
				}
			}
			if (bCanUpdateUserData)
			{
				m_squadDirty = false;
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetLocalUserData, true);
			}
		}
		const float prevStartTimer = m_startTimer;
		if (m_server)
		{
			bool  sendCountdownPacket = false;

			const int players = m_nameList.Size();
			const int playersNeeded = g_pGameCVars->g_minplayerlimit - players;

			ECountdownStage countdownStage = eCDS_WaitingForPlayers;

			if (s_pGameLobbyCVars->gl_skip || m_bSkipCountdown)
			{
				countdownStage = eCDS_Started;
			}
			else
			{
				if ((playersNeeded <= 0) && IsAutoStartingGame())
				{
					countdownStage = eCDS_Started;
#if USE_PC_PREMATCH
					if(!IsRankedGame())
#endif
					{
						if (UseLobbyTeamBalancing() && !IsGameBalanced())
						{
							if ((m_timeStartedWaitingForBalancedGame.GetValue() == 0LL) || (gEnv->pTimer->GetFrameStartTime().GetDifferenceInSeconds(m_timeStartedWaitingForBalancedGame) < g_pGameCVars->gl_waitForBalancedGameTime))
							{
								countdownStage = eCDS_WaitingForBalancedGame;
							}
							else
							{
								ForceBalanceTeams();
							}
						}
					}
				}
			}

			if(countdownStage != m_countdownStage)
			{
				m_countdownStage = countdownStage;

				if(countdownStage == eCDS_Started)
				{
					m_startTimerLength = s_pGameLobbyCVars->gl_initialTime;
					m_startTimer = m_startTimerLength;
					m_initialStartTimerCountdown = true;

					if (!CheckDLCRequirements())
					{
						return;
					}
				}
				else if (countdownStage == eCDS_WaitingForBalancedGame)
				{
					m_timeStartedWaitingForBalancedGame = gEnv->pTimer->GetFrameStartTime();
				}

				sendCountdownPacket = true;

				if (m_timeTillCallToEnsureBestHost < s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostOnStartCountdownDelayTime)
				{
					m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostOnStartCountdownDelayTime;
				}
			}

			if (s_pGameLobbyCVars->gl_skip || m_bSkipCountdown)
			{
				m_startTimer = -1.f;
				m_bSkipCountdown = false;

#if USE_PC_PREMATCH
				if (!IsRankedGame())
#endif
				{
					if (UseLobbyTeamBalancing() && !IsGameBalanced())
					{
						ForceBalanceTeams();
					}
				}
			}

			if (m_votingEnabled)
			{
				if ((countdownStage == eCDS_Started) && !m_votingClosed && ((m_startTimer - dt) <= s_pGameLobbyCVars->gl_votingCloseTimeBeforeStart))
				{
					SvCloseVotingAndDecideWinner();
					sendCountdownPacket = true;
				}
			}

			if (sendCountdownPacket)
			{
				SendPacket(eGUPD_LobbyStartCountdownTimer);
			}

			if(countdownStage == eCDS_Started)
			{
				// Detect if the timer cvars have been changed
				const float timerLength = (m_initialStartTimerCountdown ? s_pGameLobbyCVars->gl_initialTime : s_pGameLobbyCVars->gl_time);
				if (timerLength != m_startTimerLength)
				{
					const float diff = (timerLength - m_startTimerLength);
					m_startTimer += diff;
					m_startTimerLength = timerLength;
					SendPacket(eGUPD_UpdateCountdownTimer);
				}

				if (m_startTimer < 0.0f)
				{
					bool bCanStartSession = true;

					// Don't try to start the session mid-migration (delay call until after the migration has finished)
					if (hostMigrationState != eHMS_Idle)
					{
						bCanStartSession = false;
						CryLog("CGameLobby::Update() trying to start a game mid-migration, delaying (state=%u)", hostMigrationState);
					}

					if (bCanStartSession)
					{
						if (m_votingEnabled)
						{
							AdvanceLevelRotationAccordingToVotingResults();
						}

						SetState(eLS_JoinSession);
					}
				}
				else
				{
					m_startTimer -= dt;
				}
			}
		}
		else
		{
			if(m_countdownStage == eCDS_Started)
			{
				m_startTimer -= dt;
			}
		}

		if(m_countdownStage == eCDS_Started)
		{
			if (m_votingEnabled)
			{
				CRY_ASSERT(s_pGameLobbyCVars->gl_checkDLCBeforeStartTime < s_pGameLobbyCVars->gl_time);
				CRY_ASSERT(s_pGameLobbyCVars->gl_checkDLCBeforeStartTime > s_pGameLobbyCVars->gl_initialTime);
				const float  checkDLCReqsTime = std::max(std::min(s_pGameLobbyCVars->gl_checkDLCBeforeStartTime, s_pGameLobbyCVars->gl_time), s_pGameLobbyCVars->gl_initialTime);

				if ((m_startTimer < checkDLCReqsTime) && ((m_startTimer + dt) >= checkDLCReqsTime))
				{
					CryLog("CGameLobby::Update() calling CheckDLCRequirements() [2]");
					// 1. when host is between rounds and just waits on the scoreboard when the next rotation has unloaded DLC in the vote
					// 2. when client is between rounds and just waits on the scoreboard when the next rotation has unloaded DLC in the vote
					const bool  leftSessionDueToDLCRequirementsFail = !CheckDLCRequirements();
					if (leftSessionDueToDLCRequirementsFail)
					{
						return;
					}
				}
			}
		}

#ifndef _RELEASE
		if (m_votingEnabled && s_pGameLobbyCVars->gl_voteDebug)
		{
			if (m_votingCandidatesFlashInfo.tmpWatchInfoIsSet)
			{
				CryWatch("left candidate: %s (%s), num votes = %d", m_leftVoteChoice.m_levelName.c_str(), m_leftVoteChoice.m_gameRules.c_str(), m_leftVoteChoice.m_numVotes);
				CryWatch("right candidate: %s (%s), num votes = %d", m_rightVoteChoice.m_levelName.c_str(), m_rightVoteChoice.m_gameRules.c_str(), m_rightVoteChoice.m_numVotes);
				CryWatch("most votes so far: %d (%s)", m_highestLeadingVotesSoFar, (m_leftHadHighestLeadingVotes ? "left" : "right"));
			}
			if (m_votingFlashInfo.tmpWatchInfoIsSet)
			{
				if (!m_votingFlashInfo.localHasVoted)
				{
					if (!m_votingFlashInfo.votingClosed)
					{
						if (m_votingFlashInfo.localHasCandidates)
						{
							CryWatch("Cast your vote NOW!");
						}
						else
						{
							CryWatch("Awaiting candidate info...");
						}
					}
				}
				else
				{
					CryWatch("You voted %s", (m_votingFlashInfo.localVotedLeft ? "LEFT" : "RIGHT"));
				}
				if (m_votingFlashInfo.votingClosed)
				{
					if (m_votingFlashInfo.votingDrawn)
					{
						CryWatch("Voting was DRAWN, picking winner at RANDOM...");
					}
					CryWatch("Next map selected... Next: %s", (m_votingFlashInfo.leftWins ? "LEFT" : "RIGHT"));
				}
			}
			VOTING_DBG_WATCH("LOCAL vote status: %d", m_localVoteStatus);
		}
#endif

		REINST("needs verification!");
		/*const static float k_soundOffset = 0.5f;
		if(floorf(prevStartTimer + k_soundOffset) != floorf(m_startTimer + k_soundOffset))
		{
			const float time = m_initialStartTimerCountdown ? s_pGameLobbyCVars->gl_initialTime : s_pGameLobbyCVars->gl_time;

			if(time > 0.0f)
			{
				m_lobbyCountdown.Play(0, "timeLeft", clamp_tpl(m_startTimer/time, 0.0f, 1.0f));
			}
		}*/

#if !defined(_RELEASE)
		if(s_pGameLobbyCVars->gl_debug)
		{
			if (m_countdownStage == eCDS_Started)
			{
				CryWatch("Auto start %.0f", m_startTimer);
			}
			else if(m_countdownStage == eCDS_WaitingForPlayers)
			{
				CryWatch("Waiting for players");
			}
			else if(m_countdownStage == eCDS_WaitingForBalancedGame)
			{
				CryWatch("Waiting for balanced game");
			}
		}
#endif

#if GAME_LOBBY_ALLOW_MIGRATION
		if (!m_connectedToDedicatedServer)
		{
			if (m_bHasUserList && (m_nameList.Size() == 1) && (m_server == false) && (gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetSessionHostMigrationState(m_currentSession) == eHMS_Idle))
			{
				CryLog("GAME LOBBY HAS DETECTED A BROKEN STATE, BAILING (we're the only one in the lobby and we're not the host)");
#if !defined(_RELEASE)
				++(s_pGameLobbyCVars->gl_debugLobbyBreaksGeneral);
#endif

				LeaveSession(true, false);
				if (m_bMatchmakingSession && m_gameLobbyMgr->IsPrimarySession(this))
				{
					m_taskQueue.AddTask(CLobbyTaskQueue::eST_FindGame, false);
				}
			}
		}
#endif // GAME_LOBBY_ALLOW_MIGRATION

#if 0 // old frontend
		if(m_gameLobbyMgr->IsPrimarySession(this))
		{
			CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
			if (pFlashMenu && pFlashMenu->IsMenuActive(eFlM_Menu))
			{
				IFlashPlayer *pFlashPlayer = pFlashMenu->GetFlash();
				EFlashFrontEndScreens screen = pFlashMenu->GetCurrentMenuScreen();
				if (IsGameLobbyScreen(screen))
				{
					if (m_sessionUserDataDirty)
					{
						m_sessionUserDataDirty = false;

						if (!m_server)
						{
							CMPMenuHub*  pMPMenuHub = pFlashMenu->GetMPMenu();
							CUIScoreboard*  pScoreboard = (pMPMenuHub ? pMPMenuHub->GetScoreboardUI() : NULL);

							if (!pScoreboard || (pScoreboard->ShowGameLobbyEndGame() == false))
							{
								CryLog("CGameLobby::Update() calling CheckDLCRequirements() [5]");
								// 1. when between rounds, client hammers A to get to lobby whilst server is still on victory screen
								// 2. client joining a lobby with unloaded DLC in the vote
								// 3. client accepting invitation to a match using unloaded DLC (either from a host in a lobby or a host in a game)
								// 4. client being in a private lobby when the host changes map to a DLC map the client doesn't have loaded
								const bool  leftSessionDueToDLCRequirementsFail = !CheckDLCRequirements();
								if (leftSessionDueToDLCRequirementsFail)
								{
									return;
								}
							}
						}

						SendSessionDetailsToFlash(pFlashPlayer);
					}

					float currTime  = gEnv->pTimer->GetCurrTime();
					if (m_nameList.m_dirty || (m_lastUserListUpdateTime < currTime - 0.1f))
					{
						m_lastUserListUpdateTime = currTime;

						if (m_nameList.m_dirty)
							m_nameList.m_dirty = false;

						SendUserListToFlash(pFlashPlayer);
					}
				}

				UpdateStatusMessage(pFlashPlayer);
			}

#if GAME_LOBBY_DO_ENSURE_BEST_HOST
			if ((!gEnv->IsDedicated()) && m_server)
			{
				if (m_timeTillCallToEnsureBestHost > 0.f)
				{
					m_timeTillCallToEnsureBestHost -= dt;
					if (m_timeTillCallToEnsureBestHost <= 0.f)
					{
						m_taskQueue.AddTask(CLobbyTaskQueue::eST_EnsureBestHost, true);
					}
				}
			}
#endif
		}

#endif
		if (m_server && (hostMigrationState == eHMS_Idle))
		{
			if (m_timeTillUpdateSession > 0.f)
			{
				m_timeTillUpdateSession -= dt;
				if (m_timeTillUpdateSession <= 0.f)
				{
					CryLog("CGameLobby::Update() change in average skill ranking detected");
					m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
				}
			}
		}
	}
	else if(m_state == eLS_FindGame)
	{
#if 0 // old frontend
		if(m_gameLobbyMgr->IsPrimarySession(this))
		{
			CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
			if (pFlashMenu && pFlashMenu->IsMenuActive(eFlM_Menu))
			{
				IFlashPlayer *pFlashPlayer = pFlashMenu->GetFlash();
				UpdateStatusMessage(pFlashPlayer);
			}
		}
#endif
	}
}

void CGameLobby::UpdateDebugString()
{
	if (g_pGame->GetIGameFramework()->StartedGameContext())
	{
		g_pGame->AppendCrashDebugMessage(" Game");
		if (m_server)
		{
			g_pGame->AppendCrashDebugMessage(" Server");
		}
		g_pGame->AppendCrashDebugMessage(" ");
		g_pGame->AppendCrashDebugMessage(GetCurrentLevelName());
		g_pGame->AppendCrashDebugMessage(" ");
		g_pGame->AppendCrashDebugMessage(GetCurrentGameModeName());
		return;
	}
	if(m_state == eLS_Lobby)
	{
		g_pGame->AppendCrashDebugMessage(" Lobby");
	}
}

//-------------------------------------------------------------------------
// returns true if we're set up enough to perform the level check
bool CGameLobby::ReadyToCheckDLC()
{
	bool ready = true;

	if( !m_server && !m_hasReceivedSessionQueryCallback )
	{
		ready = false;
	}

	if( !g_pGame->GetIGameFramework()->StartedGameContext() )
	{
		ready = false;
	}

	if( g_pGame->GetIGameFramework()->GetLevelName() == NULL )
	{
		ready = false;
	}

	return ready;
}

//-------------------------------------------------------------------------
// if returns false then the user will have been returned to the main menu and the session will have been left
bool CGameLobby::CheckDLCRequirements()
{
#if !defined( _RELEASE )
	if( s_pGameLobbyCVars->gl_allowDevLevels )
	{
		return true;
	}
#endif //!defined( _RELEASE )

	bool  checkOk = true;

	CDLCManager*  pDLCManager = g_pGame->GetDLCManager();
	uint32  dlcRequirements = 0;

	if (m_votingEnabled)
	{
		if (m_hasReceivedStartCountdownPacket || m_server)
		{
			if (!m_votingCandidatesFlashInfo.leftLevelMapPath.empty() && !m_votingCandidatesFlashInfo.rightLevelMapPath.empty())
			{
				dlcRequirements |= pDLCManager->GetRequiredDLCsForLevel(m_votingCandidatesFlashInfo.leftLevelMapPath);
				dlcRequirements |= pDLCManager->GetRequiredDLCsForLevel(m_votingCandidatesFlashInfo.rightLevelMapPath);
			}
		}
	}
	else
	{
		if (!m_server && m_hasReceivedSessionQueryCallback)
		{
			dlcRequirements |= m_userData[eLDI_RequiredDLCs].m_int32 | g_pGame->GetDLCManager()->GetRequiredDLCs();
		}
	}

	if(dlcRequirements > 0)
	{
		if( !CDLCManager::MeetsDLCRequirements(dlcRequirements, pDLCManager->GetLoadedDLCs()) )
		{
			checkOk = false;
		}
	}
	else
	{
		//doesn't require DLC, so lets check it's core
		if (m_votingEnabled)
		{
			if (m_hasReceivedStartCountdownPacket || m_server)
			{
				if (!m_votingCandidatesFlashInfo.leftLevelMapPath.empty() && !m_votingCandidatesFlashInfo.rightLevelMapPath.empty())
				{
					const char* pTrimmedLevelName = strrchr(m_votingCandidatesFlashInfo.leftLevelMapPath, '/');
					if (pTrimmedLevelName)
					{
						++pTrimmedLevelName;
					}
					else
					{
						pTrimmedLevelName = m_votingCandidatesFlashInfo.leftLevelMapPath;
					}

					checkOk &= pDLCManager->IsLevelStandard( pTrimmedLevelName );

					pTrimmedLevelName = strrchr(m_votingCandidatesFlashInfo.rightLevelMapPath, '/');
					if (pTrimmedLevelName)
					{
						++pTrimmedLevelName;
					}
					else
					{
						pTrimmedLevelName = m_votingCandidatesFlashInfo.rightLevelMapPath;
					}

					checkOk &= pDLCManager->IsLevelStandard( pTrimmedLevelName );
				}
			}
		}
		else
		{
			if (!m_server && m_hasReceivedSessionQueryCallback && g_pGame->GetIGameFramework()->StartedGameContext() )
			{
				const char* pLevelName = g_pGame->GetIGameFramework()->GetLevelName();

				if( pLevelName != NULL )
				{
					// Strip off the first part from the level name (e.g. "Wars/")
					const char* pTrimmedLevelName = strrchr(pLevelName, '/');
					if (pTrimmedLevelName)
					{
						++pTrimmedLevelName;
					}
					else
					{
						pTrimmedLevelName = pLevelName;
					}

					checkOk = pDLCManager->IsLevelStandard( pTrimmedLevelName );
				}
			}
		}

	}

	if( checkOk == false )
	{
		if (m_state != eLS_Leaving)
		{
			if (CGameLobbyManager* pGameLobbyManager=g_pGame->GetGameLobbyManager())
			{
				pGameLobbyManager->LeaveGameSession(CGameLobbyManager::eLSR_Menu);
			}
		}

		g_pGame->AddGameWarning("DlcNotAvailable", NULL);
	}
	return checkOk;
}

//---------------------------------------
bool CGameLobby::ShouldCheckForBestHost()
{
	bool result = false;

#ifndef _RELEASE
	bool hostMigrationUseAutoLobbyMigrateInPrivateGames = (s_pGameLobbyCVars->gl_hostMigrationUseAutoLobbyMigrateInPrivateGames != 0);
#else
	bool hostMigrationUseAutoLobbyMigrateInPrivateGames = false;
#endif
	if ((!gEnv->IsDedicated()) && (IsMatchmakingGame() || hostMigrationUseAutoLobbyMigrateInPrivateGames) && s_pGameLobbyCVars->gl_allowEnsureBestHostCalls)
	{
		if ((!m_gameLobbyMgr->IsLobbyMerging()) && ((m_countdownStage != eCDS_Started) || (m_startTimer > s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostGameStartMinimumTime)))
		{
			ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
			if (pLobby)
			{
				ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
				if (pMatchmaking)
				{
					eHostMigrationState migrationState = pMatchmaking->GetSessionHostMigrationState(m_currentSession);

					if (migrationState == eHMS_Idle)
					{
						result = true;
					}
				}
			}
		}
	}

	return result;
}

//---------------------------------------
// [static]
void CGameLobby::MatchmakingEnsureBestHostCallback( CryLobbyTaskID taskID, ECryLobbyError error, void* arg )
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);

	CryLog("CGameLobby::MatchmakingEnsureBestHostCallback error %d Lobby: %p", (int)error, pLobby);
	// Should call this but we're not currently waiting for the callback
	pLobby->NetworkCallbackReceived(taskID, error);

#ifndef _RELEASE
	if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
	{
		ICryMatchMaking *pMatchMaking = gEnv->pNetwork->GetLobby()->GetMatchMaking();
		eHostMigrationState migrationState = pMatchMaking->GetSessionHostMigrationState(pLobby->m_currentSession);
		if ((migrationState == eHMS_Idle) && (pLobby->m_server))
		{
			pLobby->m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_debugForceLobbyMigrationsTimer;
		}
	}
#endif
}

#if 0 // old frontend
//-------------------------------------------------------------------------
bool CGameLobby::IsGameLobbyScreen(EFlashFrontEndScreens screen)
{
	if ( (screen == eFFES_game_lobby)
				|| (screen == eFFES_matchsettings)
				|| (screen == eFFES_matchsettings_change_map)
				|| (screen == eFFES_matchsettings_change_gametype) )
	{
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------
void CGameLobby::GetCountDownStageStatusMessage(CryFixedStringT<64> &statusString)
{
	if (m_countdownStage == eCDS_Started)
	{
		float timeToVote = m_startTimer - s_pGameLobbyCVars->gl_votingCloseTimeBeforeStart;
		const bool isVotingEnabled = m_votingEnabled && !m_votingClosed && timeToVote >= 0.f;

		float timeLeft = isVotingEnabled ? std::max(0.f, timeToVote + 1.f) : std::max(0.f, m_startTimer + 1.f);	// Round up, don't show 0 sec
		statusString.Format("%0.f", timeLeft);

		if (timeLeft < 2.f)
		{
			statusString = CHUDUtils::LocalizeString(isVotingEnabled ? "@ui_menu_gamelobby_votecountdown_single" : "@ui_menu_gamelobby_autostart_single", statusString.c_str());
		}
		else
		{
			statusString = CHUDUtils::LocalizeString(isVotingEnabled ? "@ui_menu_gamelobby_votecountdown" : "@ui_menu_gamelobby_autostart", statusString.c_str());
		}
	}
	else if (m_countdownStage == eCDS_WaitingForPlayers)
	{
		const int players = m_nameList.Size();
		const int playersNeeded = g_pGameCVars->g_minplayerlimit - players;

		if (playersNeeded > 1)
		{
			statusString.Format("%d", playersNeeded);
			statusString = CHUDUtils::LocalizeString("@ui_menu_gamelobby_waiting_for_x_players", statusString.c_str());
		}
		else
		{
			statusString = "@ui_menu_gamelobby_waiting_for_player";
		}
	}
	else
	{
		CRY_ASSERT(m_countdownStage == eCDS_WaitingForBalancedGame);
	}
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateStatusMessage(IFlashPlayer *pFlashPlayer)
{
	CRY_ASSERT(m_gameLobbyMgr->IsPrimarySession(this));

	if (!pFlashPlayer)
		return;

	CryFixedStringT<64> statusString;

	const bool bIsMatchMakingGame = IsMatchmakingGame();
	bool updateMatchmaking = false;
	bool bShowOnPlayOnlineScreen = false;

	switch (m_state)
	{
		case eLS_FindGame:
			{
				if(bIsMatchMakingGame)
				{
					statusString = "@ui_menu_gamelobby_searching";
					updateMatchmaking = true;
				}
				break;
			}
		case eLS_Initializing:
			{
				if(bIsMatchMakingGame)
				{
					statusString = "@ui_menu_gamelobby_joining";
					updateMatchmaking = true;
					bShowOnPlayOnlineScreen = true;
				}
				break;
			}
		case eLS_Leaving:
			{
				if(bIsMatchMakingGame)
				{
					statusString = "@ui_menu_gamelobby_joining";
					updateMatchmaking = true;
				}
				break;
			}
		case eLS_Lobby:						// Deliberate fall-through
		case eLS_EndSession:
			{
				if (!IsAutoStartingGame())
				{
					statusString = IsServer() ? "@ui_menu_gamelobby_select_start_match" : "@ui_menu_gamelobby_waiting_for_server";
				}
				else
				{
					GetCountDownStageStatusMessage(statusString);
				}
				break;
			}
		case eLS_JoinSession:			// Deliberate fall-through
		case eLS_Game:						// Deliberate fall-through
		case eLS_PreGame:
			{
				statusString = "@ui_menu_gamelobby_starting_game";
				break;
			}
	}

	CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
	if (pFlashMenu != NULL && pFlashMenu->IsMenuActive(eFlM_Menu))
	{
		if (IsGameLobbyScreen(pFlashMenu->GetCurrentMenuScreen()))
		{
			if (updateMatchmaking)
			{
				UpdateMatchmakingDetails(pFlashPlayer, statusString.c_str());
			}
			else
			{
				pFlashPlayer->Invoke1(FRONTEND_SUBSCREEN_PATH_SET("UpdateStatusMessage"), statusString.c_str());
			}
		}
		else
		{
			bool bShowMessage = bShowOnPlayOnlineScreen;
			if (!bShowMessage)
			{
				const EFlashFrontEndScreens currentScreen = pFlashMenu->GetCurrentMenuScreen();
				if ((currentScreen != eFFES_play_online) && (currentScreen != eFFES_play_lan))
				{
					bShowMessage = true;
				}
			}

			pFlashPlayer->Invoke1("_root.setTopRightCaption", bShowMessage ? statusString.c_str() : "");
		}
	}
}

//static------------------------------------------------------------------------
bool CGameLobby::SortPlayersByTeam(const SFlashLobbyPlayerInfo &elem1, const SFlashLobbyPlayerInfo &elem2)
{
	if (elem1.m_teamId!=0 && elem2.m_teamId==0)
	{
		return true;
	}
	else if (elem2.m_teamId!=0 && elem1.m_teamId==0)
	{
		return false;
	}
	else if (elem1.m_onLocalTeam==true && elem2.m_onLocalTeam==false)
	{
		return true;
	}
	else if (elem1.m_onLocalTeam==false && elem2.m_onLocalTeam==true)
	{
		return false;
	}
	else
	{
		if (elem1.m_teamId < elem2.m_teamId)
			return true;
		else
			return false;
	}
}
#endif

//-------------------------------------------------------------------------
void CGameLobby::ClearChatMessages()
{
#if ENABLE_CHAT_MESSAGES
	m_chatMessagesIndex = 0;

	for (int i=0; i<NUM_CHATMESSAGES_STORED; ++i)
	{
		m_chatMessagesArray[i].Clear();
	}
#endif
}

#if 0 // old frontend
//-------------------------------------------------------------------------
void CGameLobby::SendChatMessagesToFlash(IFlashPlayer *pFlashPlayer)
{
#if ENABLE_CHAT_MESSAGES

	IFlashVariableObject *textInputObj = NULL;
	FE_FLASHVAROBJ_REG(pFlashPlayer, FRONTEND_SUBSCREEN_PATH_SET("RightPane.TextChat.TextChatEntry"), textInputObj);

	SFlashVarValue maxChars(MAX_CHATMESSAGE_LENGTH-1);
	textInputObj->SetMember("maxChars", maxChars);

	FE_FLASHOBJ_SAFERELEASE(textInputObj);

	int index = m_chatMessagesIndex+1;
	if (index >= NUM_CHATMESSAGES_STORED)
		index = 0;

	int startIndex = index;

	do
	{
		m_chatMessagesArray[index].SendChatMessageToFlash(pFlashPlayer, true);

		++index;

		if (index >= NUM_CHATMESSAGES_STORED)
			index = 0;
	} while(index != startIndex);
#endif
}

//-------------------------------------------------------------------------
void CGameLobby::SendUserListToFlash(IFlashPlayer *pFlashPlayer)
{
	CRY_ASSERT(m_gameLobbyMgr->IsPrimarySession(this));

	if (!pFlashPlayer)
		return;

	CSquadManager* pSquadMgr = g_pGame->GetSquadManager();

	const SSessionNames *sessionNames = &m_nameList;
	int size = sessionNames->Size();

	bool useSquadUserlist = false;
	const ELobbyState lobbyState = m_state;

	if (m_bMatchmakingSession)
	{
		switch (lobbyState)
		{
			case eLS_FindGame:		// Deliberate fall-through
			case eLS_Initializing:
			case eLS_Leaving:
				{
					if (size == 0)
					{
						useSquadUserlist = true;
						sessionNames = pSquadMgr->GetSessionNames();
						size = sessionNames->Size();
					}
				}
				break;
		}
	}

	CryUserID pSquadLeaderId = pSquadMgr->GetSquadLeader();

	int localTeam = 0;

	bool useTeams = (g_pGameCVars->g_autoAssignTeams != 0) && (m_isTeamGame == true);
	if (!IsPrivateGame() || IsPasswordedGame())
	{
		if (m_countdownStage == eCDS_WaitingForPlayers || (m_votingEnabled && m_votingClosed==false))
		{
			useTeams = false;
		}
	}

#if !defined(_RELEASE)
	useTeams |= (s_pGameLobbyCVars->gl_lobbyForceShowTeams != 0);
#endif

	const int MAX_PLAYERS = MAX_PLAYER_LIMIT;
	CryFixedArray<SFlashLobbyPlayerInfo, MAX_PLAYERS> playerInfos;
	int numPushedPlayers = 0;

	for (int nameIdx(0); (nameIdx < size) && (numPushedPlayers < MAX_PLAYERS); ++nameIdx)
	{
		const SSessionNames::SSessionName *pPlayer = &sessionNames->m_sessionNames[nameIdx];

		if (pPlayer->m_isDedicated)
		{
			continue;
		}

		if (pPlayer->m_name[0] == 0)
		{
			continue;
		}

		SFlashLobbyPlayerInfo playerInfo;

		if (useSquadUserlist)	// If using the squad list,
		{
			uint32 channelId = pPlayer->m_conId.m_uid;
			bool isLocal = (nameIdx==0);

			CryUserID userId = pSquadMgr->GetUserIDFromChannelID(channelId);
			bool isSquadLeader = (pSquadLeaderId.IsValid() && (pSquadLeaderId == userId));

			playerInfo.m_rank = pPlayer->m_rank;
			playerInfo.m_reincarnations = pPlayer->m_reincarnations;
			pPlayer->GetDisplayName(playerInfo.m_nameString);

			playerInfo.m_teamId = 0;
			playerInfo.m_onLocalTeam = isLocal;
			playerInfo.m_conId = channelId; // Squad channel id, NOT game channel id
			playerInfo.m_isLocal = isLocal;	// Currently local player is always index 0
			playerInfo.m_isSquadMember = pSquadMgr->IsSquadMateByUserId(userId);
			playerInfo.m_isSquadLeader = isSquadLeader;
			playerInfo.m_voiceState = (uint8)eLVS_off;
			playerInfo.m_entryType = eLET_Squad;
		}
		else
		{
			uint32 channelId = pPlayer->m_conId.m_uid;
			bool isLocal = (nameIdx==0);
			if (isLocal)
			{
				localTeam = pPlayer->m_teamId;
			}

			CryUserID userId = GetUserIDFromChannelID(channelId);
			uint8 voiceState = (uint8)GetVoiceState(channelId);
			bool isSquadLeader = (pSquadLeaderId.IsValid() && (pSquadLeaderId == userId));

			playerInfo.m_rank = pPlayer->m_rank;
			playerInfo.m_reincarnations = pPlayer->m_reincarnations;
			pPlayer->GetDisplayName(playerInfo.m_nameString);

			if (useTeams)
			{
				playerInfo.m_teamId = pPlayer->m_teamId;
				playerInfo.m_onLocalTeam = (playerInfo.m_teamId == localTeam);
			}
			else
			{
				playerInfo.m_teamId = 0;
				playerInfo.m_onLocalTeam = isLocal;
			}

			playerInfo.m_conId = channelId;
			playerInfo.m_isLocal = isLocal;	// Currently local player is always index 0
			playerInfo.m_isSquadMember = pSquadMgr->IsSquadMateByUserId(userId);
			playerInfo.m_isSquadLeader = isSquadLeader;
			playerInfo.m_voiceState = voiceState;
			playerInfo.m_entryType = eLET_Lobby;
		}

		playerInfos.push_back(playerInfo);
		++ numPushedPlayers;
	}

#ifndef _RELEASE
	if (s_pGameLobbyCVars->gl_dummyUserlist)
	{
		const int useSize = std::min(size+s_pGameLobbyCVars->gl_dummyUserlist, MAX_PLAYERS);

		for (int nameIdx(size); (nameIdx<useSize); ++nameIdx)
		{
			SFlashLobbyPlayerInfo playerInfo;

			playerInfo.m_rank = nameIdx;
			playerInfo.m_reincarnations = (nameIdx%5) ? 0 : nameIdx;
			playerInfo.m_nameString.Format("WWWWWWWWWWWWWW%.2d WWWW ", nameIdx);


			if (useTeams)
			{
				if (s_pGameLobbyCVars->gl_dummyUserlistNumTeams==1)
					playerInfo.m_teamId = 1;
				else if (s_pGameLobbyCVars->gl_dummyUserlistNumTeams==2)
					playerInfo.m_teamId = 1 + (nameIdx % 2);
				else if (s_pGameLobbyCVars->gl_dummyUserlistNumTeams==3)
					playerInfo.m_teamId = (nameIdx % 3);
				else
					playerInfo.m_teamId = 0;
			}
			else
			{
				playerInfo.m_teamId = 0;
			}
			playerInfo.m_conId = 100+nameIdx;
			playerInfo.m_isLocal = false;
			playerInfo.m_isSquadMember = (nameIdx%3) ? false : true;
			playerInfo.m_isSquadLeader = ((nameIdx%3) || (nameIdx==3)) ? false : true;
			playerInfo.m_onLocalTeam = (playerInfo.m_teamId == localTeam);
			playerInfo.m_voiceState = nameIdx%6;
			playerInfo.m_entryType = eLET_Lobby;

			playerInfos.push_back(playerInfo);
		}
	}
#endif

	std::sort(playerInfos.begin(), playerInfos.end(), SortPlayersByTeam); // Sort players by team

	IFlashVariableObject *pPushArray = NULL, *pMaxTeamPlayers = NULL;
	FE_FLASHVAROBJ_REG(pFlashPlayer, FRONTEND_SUBSCREEN_PATH_SET("m_namesList"), pPushArray);
	FE_FLASHVAROBJ_REG(pFlashPlayer, FRONTEND_SUBSCREEN_PATH_SET("m_maxTeamPlayers"), pMaxTeamPlayers);

	pPushArray->ClearElements();

	int currentTeamCount = 0;
	int currentTeam = -1;

	int maxUIPlayersPerTeam = 6;

	SFlashVarValue maxTeamPlayersValue = pMaxTeamPlayers->ToVarValue();
	if (maxTeamPlayersValue.IsDouble())	// Flash var is a double
	{
		maxUIPlayersPerTeam = (int)maxTeamPlayersValue.GetDouble();	// PC UI might be able to show more..
	}

	size = playerInfos.size();
	for (int playerIdx(0); (playerIdx<size); ++playerIdx)
	{
		if (playerInfos[playerIdx].m_teamId != currentTeam)
		{
			currentTeam = playerInfos[playerIdx].m_teamId;
			currentTeamCount = 0;
		}

		++currentTeamCount;
		if ((currentTeam != 0) && (currentTeamCount > maxUIPlayersPerTeam))
		{
			continue;
		}

		IFlashVariableObject *pNewObject = NULL;
		if (pFlashPlayer->CreateObject("Object", NULL, 0, pNewObject))
		{
			const bool bRankValid = (playerInfos[playerIdx].m_rank > 0);
			SFlashVarValue rankInt(playerInfos[playerIdx].m_rank);
			SFlashVarValue rankNull("");

			pNewObject->SetMember("Name", playerInfos[playerIdx].m_nameString.c_str());
			pNewObject->SetMember("Rank", bRankValid ? rankInt : rankNull);
			pNewObject->SetMember("Reincarnations", playerInfos[playerIdx].m_reincarnations);
			pNewObject->SetMember("TeamId", playerInfos[playerIdx].m_teamId);
			pNewObject->SetMember("ConId", playerInfos[playerIdx].m_conId);
			pNewObject->SetMember("Local", playerInfos[playerIdx].m_isLocal);
			pNewObject->SetMember("SquadMember", playerInfos[playerIdx].m_isSquadMember);
			pNewObject->SetMember("SquadLeader", playerInfos[playerIdx].m_isSquadLeader);
			pNewObject->SetMember("VoiceState", playerInfos[playerIdx].m_voiceState);
			pNewObject->SetMember("EntryType", (int)playerInfos[playerIdx].m_entryType);
			pPushArray->PushBack(pNewObject);

			FE_FLASHOBJ_SAFERELEASE(pNewObject);
		}
	}
	FE_FLASHOBJ_SAFERELEASE(pPushArray);
	FE_FLASHOBJ_SAFERELEASE(pMaxTeamPlayers);

	pFlashPlayer->Invoke1(FRONTEND_SUBSCREEN_PATH_SET("UpdatePlayerListNames"), (useTeams ? localTeam : 0));
}
#endif

const char* CGameLobby::GetMapDescription(const char* levelFileName, CryFixedStringT<64>* pOutLevelDescription)
{
	CRY_ASSERT(pOutLevelDescription);

	ILevelInfo* pLevelInfo = g_pGame->GetIGameFramework()->GetILevelSystem()->GetLevelInfo(levelFileName);
	if(pLevelInfo)
	{
		CryFixedStringT<32> levelPath;
		string strLevelName(levelFileName);
		levelPath.Format("%s/%s.xml", pLevelInfo->GetPath(), PathUtil::GetFileName(strLevelName).c_str());

		XmlNodeRef mapInfo = GetISystem()->LoadXmlFromFile(levelPath.c_str());
		if(mapInfo)
		{
			XmlNodeRef imageNode = mapInfo->findChild("DescriptionText");
			if(imageNode)
			{
				if (const char* text = imageNode->getAttr("text"))
				{
					*pOutLevelDescription = text;
				}
			}
		}
	}

	return pOutLevelDescription->c_str();
}

const char* CGameLobby::GetMapImageName(const char* levelFileName, CryFixedStringT<128>* pOutLevelImageName)
{
	CRY_ASSERT(pOutLevelImageName);

	ILevelInfo* pLevelInfo = g_pGame->GetIGameFramework()->GetILevelSystem()->GetLevelInfo(levelFileName);
	if(pLevelInfo)
	{
		const char *pPreviewImage = pLevelInfo->GetPreviewImagePath();
		if (pPreviewImage != NULL && pPreviewImage[0])
		{
			CDLCManager*  pDLCManager = g_pGame->GetDLCManager();

			if( pDLCManager->GetRequiredDLCsForLevel( levelFileName ) != 0 )
			{
				//get the preview out of the DLC pak, path should be good
				pOutLevelImageName->Format("%s/%s", pLevelInfo->GetPath(), pPreviewImage);
			}
			else
			{
				//get the preview out of the textures pak
				pOutLevelImageName->Format("textures/%s/%s", pLevelInfo->GetPath(), pPreviewImage);
			}
		}
	}

	return pOutLevelImageName->c_str();
}
#if 0 // old frontend
//-------------------------------------------------------------------------
void CGameLobby::UpdateMatchmakingDetails(IFlashPlayer *pFlashPlayer, const char* statusMessage)
{
	// Show searching panel.
	pFlashPlayer->Invoke1(FRONTEND_SUBSCREEN_PATH_SET("UpdateMatchmakingDetails"), statusMessage);
}

//-------------------------------------------------------------------------
void CGameLobby::SendSessionDetailsToFlash(IFlashPlayer *pFlashPlayer, const char* levelOverride/*=0*/)
{
	CRY_ASSERT(m_gameLobbyMgr->IsPrimarySession(this));

	if (!pFlashPlayer)
		return;

	const ELobbyState lobbyState = m_state;

	if (m_bMatchmakingSession)
	{
		switch (lobbyState)
		{
			case eLS_FindGame:		// Deliberate fall-through
			case eLS_Leaving:
				{
					UpdateMatchmakingDetails(pFlashPlayer, "@ui_menu_gamelobby_searching");
					return;
				}
				break;
			case eLS_Initializing:
				{
					UpdateMatchmakingDetails(pFlashPlayer, "@ui_menu_gamelobby_joining");
					return;
				}
				break;
		};
	}

	bool votingEnabled = (m_votingEnabled && m_votingFlashInfo.localHasCandidates);
	if (votingEnabled==false)
	{
		// If we don't have any session details, or are matchmaking and don't have playlist details - show fetching info status instead of level details.
		if (!m_server && (!m_hasReceivedSessionQueryCallback || (m_bMatchmakingSession && (!m_hasReceivedPlaylistSync || !m_hasReceivedStartCountdownPacket))))
		{
			UpdateMatchmakingDetails(pFlashPlayer, "@ui_menu_gamelobby_fetching");
		}
		else
		{
			// No voting - display single map image with gamemode and variant.
			CryFixedWStringT<32> variantName;
			CryFixedWStringT<32> gameModeName;

			CryFixedStringT<32> tempString;

			CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
			if (pPlaylistManager)
			{
				const SGameVariant* pVariant = pPlaylistManager->GetVariant(pPlaylistManager->GetActiveVariantIndex());
				if (pVariant)
				{
					tempString = pVariant->m_localName.c_str();
				}
			}
			variantName = CHUDUtils::LocalizeStringW(tempString.c_str());

			if (m_currentGameRules.empty())
			{
				gameModeName.clear();
				CryLog("CGameLobby::SendSessionDetailsToFlash() We're in trouble, our currentGameRules is empty. Hide an invalid string ID, but nothing good is going to be onscreen");
			}
			else
			{
				tempString.Format("@ui_rules_%s", m_currentGameRules.c_str());
				gameModeName = CHUDUtils::LocalizeStringW(tempString.c_str());
			}

			if (levelOverride)
			{
				m_uiOverrideLevel = levelOverride;
			}

			CryFixedWStringT<64> fullModeName;
			CryFixedStringT<128> flashLevelImageName;
			CryFixedStringT<64> flashLevelDescription;
			CryFixedStringT<32> levelFileName = m_uiOverrideLevel.empty() ? m_currentLevelName.c_str() : m_uiOverrideLevel.c_str();
			GetMapImageName(levelFileName.c_str(), &flashLevelImageName);

			bool bUsingLevelOverride = m_uiOverrideLevel.empty()==false;
			if (bUsingLevelOverride)
			{
				GetMapDescription(levelFileName.c_str(), &flashLevelDescription);
				fullModeName.Format(L"%ls", gameModeName.c_str());
			}
			else
			{
				fullModeName.Format(L"%ls - %ls", gameModeName.c_str(), variantName.c_str());
			}

			levelFileName = PathUtil::GetFileName(levelFileName.c_str()).c_str();
			levelFileName = g_pGame->GetMappedLevelName(levelFileName.c_str());

			bool allowMapCycling = m_server && m_bMatchmakingSession==false && m_bChoosingGamemode==false && bUsingLevelOverride==false;

			SFlashVarValue pArgs[] = { fullModeName.c_str()
				, levelFileName.c_str()
					,	flashLevelImageName.c_str()
					, allowMapCycling
					, bUsingLevelOverride
					, flashLevelDescription.c_str() };

			pFlashPlayer->Invoke(FRONTEND_SUBSCREEN_PATH_SET("UpdateSessionDetails"), pArgs, CRY_ARRAY_COUNT(pArgs));
		}
	}
	else
	{
		// Voting enabled - either show voting options or if voting has finished, the winner.
		SFlashVarValue pArgs[] = { m_votingFlashInfo.votingClosed
			, m_votingFlashInfo.votingDrawn
				,	m_votingFlashInfo.leftWins
				, m_votingFlashInfo.localHasVoted
				, m_votingFlashInfo.localVotedLeft
				, m_votingFlashInfo.leftNumVotes
				, m_votingFlashInfo.rightNumVotes
				, m_votingCandidatesFlashInfo.leftLevelImage.c_str()
				, m_votingCandidatesFlashInfo.leftLevelName.c_str()
				, m_votingCandidatesFlashInfo.leftRulesName.c_str()
				, m_votingCandidatesFlashInfo.rightLevelImage.c_str()
				, m_votingCandidatesFlashInfo.rightLevelName.c_str()
				, m_votingCandidatesFlashInfo.rightRulesName.c_str()
				, m_votingFlashInfo.votingStatusMessage.c_str()
		};
		pFlashPlayer->Invoke(FRONTEND_SUBSCREEN_PATH_SET("UpdateSessionVotingDetails"), pArgs, CRY_ARRAY_COUNT(pArgs));
	}
}
#endif

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoQuerySession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoQuerySession() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Query);
	CRY_ASSERT(!m_server);

	ECryLobbyError result = pMatchMaking->SessionQuery(m_currentSession, &taskId, CGameLobby::MatchmakingSessionQueryCallback, this);

	return result;
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateState()
{
	//done in update to handle callbacks from network thread
	if(m_stateHasChanged)
	{
		if (m_state == eLS_PreGame)
		{
			SetState(eLS_Game);
		}

		m_stateHasChanged = false;
	}
}

//-------------------------------------------------------------------------
bool CGameLobby::CheckRankRestrictions()
{
	bool bAllowedToJoin = true;

	if (!gEnv->IsDedicated())
	{
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			int activeVariant = pPlaylistManager->GetActiveVariantIndex();
			const SGameVariant *pGameVariant = (activeVariant >= 0) ? pPlaylistManager->GetVariant(activeVariant) : NULL;
			const int localRank = CPlayerProgression::GetInstance()->GetData(EPP_Rank);
			const int reincarnations = CPlayerProgression::GetInstance()->GetData(EPP_Reincarnate);

			const char*  pWarning = NULL;  // a NULL warning means everything's OK and we're allowed to join

			{
				int restrictRank = pGameVariant ? pGameVariant->m_restrictRank : 0;
				if (restrictRank)
				{
					if (localRank > restrictRank || reincarnations > 0)
					{
						pWarning = "RankTooHigh";
					}
				}
			}

			if (!pWarning)
			{
				int requireRank = pGameVariant ? pGameVariant->m_requireRank : 0;
				if (requireRank)
				{
					if (localRank < requireRank)
					{
						pWarning = "RankTooLow";
					}
				}
			}

			// alright, all checks are done... time to see if we're allowed

			bAllowedToJoin = (pWarning == NULL);
			if (!bAllowedToJoin)
			{
				LeaveSession(true, false);
				g_pGame->AddGameWarning("RankRestriction", pWarning);
			}
		}
	}

	return bAllowedToJoin;
}

//-------------------------------------------------------------------------
void CGameLobby::EnterState(ELobbyState prevState, ELobbyState newState)
{
	bool isPrimarySession = m_gameLobbyMgr->IsPrimarySession(this);
	m_state = newState;

	CryLog("[CG] CGameLobby::EnterState() entering state %i from %i Lobby: %p", int(newState), int(prevState), this);
	INDENT_LOG_DURING_SCOPE();


	switch(newState)
	{
	case eLS_JoinSession:
		{
			bool bAllowedToJoin = CheckRankRestrictions();
			if (bAllowedToJoin)
			{
				if(m_sessionHostOnly)
				{
					m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetupDedicatedServer, false);
				}
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionStart, false);
				SetLocalVoteStatus(eLVS_awaitingCandidates);		// Reset our vote ready for the next round

#if defined(TRACK_MATCHMAKING)
				CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry();
				if( pMMTel != NULL && m_bMatchmakingSession && m_currentSessionId )
				{
					pMMTel->AddEvent( SMMLaunchGameEvent( m_currentSessionId ) );
				}
#endif
				if( CMatchMakingHandler* pMMhandler = m_gameLobbyMgr->GetMatchMakingHandler() )
				{
					pMMhandler->OnLeaveMatchMaking();
				}

				// Cancel the ongoing Query task that's initiated by entering the eLS_Lobby state
				// when client connects to the dedicated server with the game in progress.
				CancelLobbyTask(CLobbyTaskQueue::eST_Query);

				if( m_bMatchmakingSession )
				{
					//now 'seems' like a good place to say we're starting a game
					CPlaylistManager* pPlaylists = g_pGame->GetPlaylistManager();

					if( const SPlaylist* pCurrentPlaylist = pPlaylists->GetCurrentPlaylist() )
					{
						int variantIndex = pPlaylists->GetActiveVariantIndex();
						g_pGame->GetPlaylistActivityTracker()->CreatedGame( pCurrentPlaylist->id, variantIndex );
					}
				}
			}
			break;
		}
	case eLS_EndSession:
		{
			if (m_server)
			{
				SendPacket(eGUPD_LobbyEndGame);
			}

			if(m_sessionHostOnly && m_bAllocatedServer)
			{
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_ReleaseDedicatedServer, false);
			}
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionEnd, false);
			break;
		}
	case eLS_PreGame:
		{
			m_loadingGameRules = m_currentGameRules.c_str();
			m_loadingLevelName = m_currentLevelName.c_str();

			OnGameStarting();

			break;
		}
	case eLS_Game:
		{
			// Force godmode to be off!
			ICVar *pCVar = gEnv->pConsole->GetCVar("g_godmode");
			if (pCVar)
			{
				if (pCVar->GetIVal() != 0)
				{
					CryLog("CGameLobby::EnterState(eLS_Game), g_godmode is set! resetting to 0");
					pCVar->Set(0);
				}
			}

			if (m_server)
			{
				m_loadingGameRules = m_currentGameRules;
				m_loadingLevelName = m_currentLevelName;
			}

			// Because we are using the lobby, we can set a hint which allows the loading system to queue the client map load a lot sooner
			ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
			if (pLobby)
			{
				pLobby->SetCryEngineLoadHint(GetLoadingLevelName(), GetLoadingGameModeName());
			}

			if(m_server)
			{
				// does this need to check for uninitialised data if dedicated?
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);

				if(m_sessionHostOnly)
				{
					// session host means server will be allocated by an arbitrator, this means we need to inform
					// our session clients about the server thats been allocated to us
					SendPacket(eGUPD_SetupJoinCommand);
				}

				SendPacket(eGUPD_LobbyGameHasStarted);	//informs clients who are in the lobby straight away

				StartGame();
			}
			else
			{
				ICVar *pCVarMaxPlayers = gEnv->pConsole->GetCVar("sv_maxplayers");
				if (pCVarMaxPlayers)
				{
					uint32 maxPlayers = m_sessionData.m_numPublicSlots + m_sessionData.m_numPrivateSlots;
					pCVarMaxPlayers->Set((int) maxPlayers);
				}

				CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
				if (pPlaylistManager)
				{
					pPlaylistManager->SetModeOptions();
				}
				g_pGame->GetIGameFramework()->ExecuteCommandNextFrame(m_joinCommand);
				m_bCanAbortLoading = true;
			}
		}
		break;

	case eLS_PostGame:
		{
			if( gEnv && gEnv->pCryPak )
			{
				gEnv->pCryPak->DisableRuntimeFileAccess(false);
			}
		}
		break;

	case eLS_Lobby:
		{
			CryLog(" entered lobby state (eLS_Lobby), m_server=%s Lobby: %p", m_server ? "true" : "false", this);

			// Re-cache the Paks
			PrecachePaks(GetCurrentLevelName());

			CRY_ASSERT(prevState != eLS_GameEnded);

			ResetLevelOverride();

			if (m_bHasReceivedVoteOptions == false)
			{
				VOTING_DBG_LOG("[tlh] set m_localVoteStatus [2] to eLVS_awaitingCandidates");
				SetLocalVoteStatus(eLVS_awaitingCandidates);
				m_votingFlashInfo.Reset();
				m_votingCandidatesFlashInfo.Reset();
			}

			m_lobbyCountdown.SetSignal("LobbyCountdown");

			bool isRoundSwitch = false;

			if(prevState == eLS_PostGame)
			{
				isRoundSwitch = !m_votingEnabled && m_replayMapWithTeamsSwitched;
			}

			ClearChatMessages();
			m_teamBalancing.OnGameFinished(!isRoundSwitch ? CTeamBalancing::eUTT_Unlock : CTeamBalancing::eUTT_Switch);
			UpdateTeams();

			if(prevState == eLS_PostGame)
			{
				if(m_server)
				{
					if (m_votingEnabled)
					{
						SvResetVotingForNextElection();
					}

					if (m_timeTillCallToEnsureBestHost < s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostOnReturnToLobbyDelayTime)
					{
						m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostOnReturnToLobbyDelayTime;
					}
				}
			}

			if(!m_server)
			{
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_Query, true);
			}
			else
			{
				m_initialStartTimerCountdown = (prevState != eLS_PostGame);
				m_startTimerLength = m_initialStartTimerCountdown ? s_pGameLobbyCVars->gl_initialTime : s_pGameLobbyCVars->gl_time;
				m_startTimer = (s_pGameLobbyCVars->gl_skip == 0) ? m_startTimerLength : 0.f;
				SendPacket(eGUPD_LobbyStartCountdownTimer);
			}

			m_sessionUserDataDirty = true;
			m_nameList.m_dirty = true;

			if (prevState == eLS_Initializing)
			{
				m_hasReceivedSessionQueryCallback = false;
				m_hasReceivedStartCountdownPacket = false;
				m_hasReceivedPlaylistSync = false;
				m_gameHadStartedWhenPlaylistRotationSynced = false;

				if(!m_server)
				{
					if(isPrimarySession)
					{

#if 0 // old frontend
						CFlashFrontEnd *menu = g_pGame->GetFlashMenu();
						if(menu)
						{
							if (menu->IsScreenInStack("game_lobby") == false)
							{
								menu->Execute(CFlashFrontEnd::eFECMD_goto, "game_lobby");
							}
						}
#endif
					}
				}
				else
				{
					m_votingEnabled = false;
					CRY_ASSERT_MESSAGE((!s_pGameLobbyCVars->gl_enablePlaylistVoting || s_pGameLobbyCVars->gl_experimentalPlaylistRotationAdvance), "The gl_enablePlaylistVoting cvar currently requires gl_experimentalPlaylistRotationAdvance to also be set");

					if (s_pGameLobbyCVars->gl_experimentalPlaylistRotationAdvance)
					{
						SvInitialiseRotationAdvanceAtFirstLevel();  // (will set data->Get()->m_votingEnabled as required)
					}

					if (m_votingEnabled)
					{
						SvResetVotingForNextElection();
					}

					CryLog("CGameLobby::EnterState() eLS_Lobby: calling CheckDLCRequirements() [4]");
					// 1. when host starts a new playlist which has unloaded DLC on the first vote
					const bool  leftSessionDueToDLCRequirementsFail = !CheckDLCRequirements();
					if (leftSessionDueToDLCRequirementsFail)
					{
						break;  // (early break from eLS_Lobby case)
					}
				}
			}

			if(prevState == eLS_PostGame)
			{
				if(m_server)
				{
					if (!m_votingEnabled)
					{
						if(m_replayMapWithTeamsSwitched)
						{
							m_replayMapWithTeamsSwitched = false;
						}
						else
						{
							if(!UpdateLevelRotation())
							{
								// if there's no level rotation, then we need to update this manually
								m_replayMapWithTeamsSwitched = m_isAsymmetricGame;
							}
						}
						m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
					}
				}
			}

			// Un-mute everyone
			MutePlayersOnTeam(0, false);
			MutePlayersOnTeam(1, false);
			MutePlayersOnTeam(2, false);

			// Refresh squad user data incase progression information has updated
			if (CSquadManager *pSquadManager = g_pGame->GetSquadManager())
			{
				pSquadManager->LocalUserDataUpdated();
			}

			m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetLocalUserData, true);

#ifndef _RELEASE
			m_timeTillAutoLeaveLobby = s_pGameLobbyCVars->gl_debugLobbyRejoinTimer;
			float randomElement = ((float) (g_pGame->GetRandomNumber() & 0xffff)) / ((float) 0xffff);
			m_timeTillAutoLeaveLobby += (s_pGameLobbyCVars->gl_debugLobbyRejoinRandomTimer * randomElement);

			m_migrationStarted = false;
#endif

#if defined(DEDICATED_SERVER)
			CDownloadMgr *pMgr=g_pGame->GetDownloadMgr();
			if (pMgr)
			{
				pMgr->OnReturnToLobby();
			}
#endif
			// Got here by a map command
			if (m_hasReceivedMapCommand)
			{
				CryLog("  entered lobby state - has received map command, starting session");
				SetState(eLS_JoinSession);
			}
			else if (!m_pendingLevelName.empty())
			{
				CryLog("  entered lobby state - already have pending level name");
				SetCurrentLevelName(m_pendingLevelName.c_str());
				m_pendingLevelName.clear();

				ICVar *pCVar = gEnv->pConsole->GetCVar("sv_gamerules");
				if (pCVar)
				{
					pCVar->Set(m_currentGameRules.c_str());
				}

				CryFixedStringT<128> command;
				command.Format("map %s s nb", GetCurrentLevelName());
				gEnv->pConsole->ExecuteString(command.c_str(), false, true);
			}
		}
		break;

	case eLS_None:
		{
			if (isPrimarySession)
			{
				ILevelRotation *pLevelRotation=g_pGame->GetIGameFramework()->GetILevelSystem()->GetLevelRotation();
				if (pLevelRotation->GetLength() > 0)
				{
					pLevelRotation->Reset();
				}

				CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
				if (pPlaylistManager)
				{
					pPlaylistManager->ClearCurrentPlaylist();
				}
			}

			// Task queue should already be empty but it'll get updated once before we are actually deleted so make
			// sure nothing is going to start!
			m_taskQueue.Reset();

			ClearChatMessages();

			m_allowRemoveUsers = true;
			m_nameList.Clear();
			m_sessionFavouriteKeyId = INVALID_SESSION_FAVOURITE_ID;
			m_isMidGameLeaving = false;

			m_currentGameRules = "";
			SetCurrentLevelName("");
			UnloadPrecachedPaks("Enter State None");

			m_gameLobbyMgr->DeletedSession(this);
		}
		break;

	case eLS_FindGame:
		{
			CRY_ASSERT((prevState == eLS_None) || (prevState == eLS_Leaving) || (prevState == eLS_Initializing));
			CRY_ASSERT(m_currentSession == CrySessionInvalidHandle);

#if GAMELOBBY_USE_COUNTRY_FILTERING
			if (m_timeSearchStarted.GetValue() == 0LL)
			{
				m_timeSearchStarted = gEnv->pTimer->GetFrameStartTime();
			}
#endif

#if defined(TRACK_MATCHMAKING)
			if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
			{
				pMMTel->BeginMatchMakingTranscript( CMatchmakingTelemetry::eMMTelTranscript_QuickMatch );
			}
#endif
			FindGameEnter();
		}
		break;

	case eLS_GameEnded:
		{
			if (m_hasReceivedMapCommand)
			{
				// If we're ready to start the next game, don't wait to be told to continue
				SetState(eLS_Lobby);
			}
		}
		break;

	case eLS_Initializing:
		{
			ResetLocalVotingData();
		}
		break;

	case eLS_Leaving:
		{
			const char* pNextFrameCommand = g_pGame->GetIGameFramework()->GetNextFrameCommand();
			if(pNextFrameCommand)
			{
				if(!strcmp(pNextFrameCommand, m_joinCommand))
				{
					CryLog("[lobby] clear join command from CryAction");
					g_pGame->GetIGameFramework()->ClearNextFrameCommand();
				}
			}

			ResetLocalVotingData();
			CancelAllLobbyTasks();

#if !GAME_LOBBY_ALLOW_MIGRATION
			// only free the dedicated server on leaving session if we don't support migration,
			// otherwise we'll take the server down as we leave causing the migration to
			// ultimately fail anyway
			if(m_sessionHostOnly && m_bAllocatedServer)
			{
				// we may not want to do this if we allow session migration, but at present
				// it's best to at least try and free the server if the session host is leaving
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_ReleaseDedicatedServer, false);
			}
#endif

			if (m_bSessionStarted)
			{
				// Need to end the session before deleting it
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionEnd, false);
			}
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Delete, false);
		}
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "CGameLobby::EnterState Unknown state");
		break;
	}

	if (isPrimarySession)
	{
#if 0 // old frontend
		CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
		if (pFlashMenu != NULL && pFlashMenu->IsMenuActive(eFlM_Menu))
		{
			IFlashPlayer *pFlashPlayer = pFlashMenu->GetFlash();
			UpdateStatusMessage(pFlashPlayer);
		}
#endif
	}
}

//---------------------------------------
void CGameLobby::OnGameStarting()
{
	CryLog("[CG] CGameLobby::OnGameStarting() Lobby: %p", this);

	// Sort out voice muting
	CryUserID localUser = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetUserID(g_pGame->GetExclusiveControllerDeviceIndex());
	SCryMatchMakingConnectionUID localUID;
	if (localUser.IsValid() && GetConnectionUIDFromUserID(localUser, localUID))
	{
		int userIndex = m_nameList.Find(localUID);
		if (userIndex != SSessionNames::k_unableToFind)
		{
			const uint8 teamId = m_nameList.m_sessionNames[userIndex].m_teamId;
			CryLog("    we're on team %i Lobby: %p", teamId, this);
			if (teamId)
			{
				const uint8 otherTeamId = (3 - teamId);
				if(m_isTeamGame)
				{
					CryLog("    muting players on teams 0 and %i Lobby: %p", otherTeamId, this);
					// Mute players not on a team or on the other team, un-mute those on our team
					MutePlayersOnTeam(0, true);
					MutePlayersOnTeam(teamId, false);
					MutePlayersOnTeam(otherTeamId, true);
				}
				else
				{
					CryLog("    ummuting all players Lobby: %p", this);
					MutePlayersOnTeam(0, false);
					MutePlayersOnTeam(1, false);
					MutePlayersOnTeam(2, false);
				}
			}
			else
			{
				CryLog("    we're not on a team Lobby: %p", this);
				// Not been given a team, either we're a mid-game joiner or this isn't a team game
				if ((!m_hasValidGameRules) || m_isTeamGame)
				{
					CryLog("        this is either a team game or unknown gamemode, muting all players, will unmute when teams have been assigned Lobby: %p", this);
					// Team game, mute all players and unmute our team once we know what that is!
					MutePlayersOnTeam(0, true);
					MutePlayersOnTeam(1, true);
					MutePlayersOnTeam(2, true);
				}
				else
				{
					CryLog("        this is a non-team game Lobby: %p", this);
				}
			}
		}
		else
		{
			CryLog("    not received the users list yet Lobby: %p", this);
		}
	}
}

//-------------------------------------------------------------------------
bool CGameLobby::UpdateLevelRotation()
{
	CRY_ASSERT(m_server);
#if 0 // LEVEL ROTATION DISABLED FOR NOW
	ILevelRotation*  pLevelRotation = g_pGame->GetPlaylistManager()->GetLevelRotation();
	const int  len = pLevelRotation->GetLength();
	if (len > 0)
	{
		SetCurrentLevelName(pLevelRotation->GetNextLevel());

		// game modes can have aliases, so we get the correct name here
		IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
		const char *gameRulesName = pGameRulesSystem ? pGameRulesSystem->GetGameRulesName(pLevelRotation->GetNextGameRules()) : pLevelRotation->GetNextGameRules();

		if (stricmp(m_currentGameRules.c_str(), gameRulesName))
		{
			GameRulesChanged(gameRulesName);
		}

		// level rotation updated, reset bool
		if(m_isAsymmetricGame)
		{
			m_replayMapWithTeamsSwitched = true;
		}

		const int numSettings = pLevelRotation->GetNextSettingsNum();
		for(int i = 0; i < numSettings; ++i)
		{
			gEnv->pConsole->ExecuteString(pLevelRotation->GetNextSetting(i));
		}

		if (!pLevelRotation->Advance())
		{
			pLevelRotation->First();
		}

		return true;
	}
#endif
	return false;
}

//-------------------------------------------------------------------------
void CGameLobby::SvInitialiseRotationAdvanceAtFirstLevel()
{
	CryLog("CGameLobby::SvInitialiseRotationAdvanceAtFirstLevel()");

	CRY_ASSERT(m_server);
#if 0 // LEVEL ROTATION DISABLED FOR NOW
	if (CPlaylistManager* plMgr=g_pGame->GetPlaylistManager())
	{
		if (plMgr->HavePlaylistSet())
		{
			ILevelRotation*  pRot = plMgr->GetLevelRotation();
			CRY_ASSERT(pRot);

			bool offlineVotingAllowed = false;
#ifndef _RELEASE
			offlineVotingAllowed = (s_pGameLobbyCVars->gl_enableOfflinePlaylistVoting != 0);
#endif

			m_votingEnabled = (s_pGameLobbyCVars->gl_enablePlaylistVoting && (IsPrivateGame()==false || IsPasswordedGame()) && (IsOnlineGame() || offlineVotingAllowed) && (pRot->GetLength() >= s_pGameLobbyCVars->gl_minPlaylistSizeToEnableVoting));

			{
				pRot->First();

				CryFixedStringT<128>  consoleCmd;

				ChangeGameRules(pRot->GetNextGameRules());
				ChangeMap(pRot->GetNextLevel());

				// this block is a slightly modified version of pRot->ChangeLevel() - so other "settings" from the rotation level info entry are set, and the "next" index is updated. will also set sv_gamerules, etc. but we don't use those anymore
				{
					const int  numSettings = pRot->GetNextSettingsNum();
					for (int i=0; i<numSettings; ++i)
					{
						CRY_TODO(26,05,2010, "Need to think about the security of this. If it doesn't go through the general white-list filtering that system.cfg (etc.) settings go through, then it'll need its own white-list. Or it might be best to have its own anyway. (The PlaylistManager might be a good place for it.)");
						gEnv->pConsole->ExecuteString(pRot->GetNextSetting(i));
					}

					if (!m_votingEnabled)
					{
						if (!pRot->Advance())
						{
							pRot->First();
						}
					}
				}
			}
		}
	}
#endif
}

//-------------------------------------------------------------------------
void CGameLobby::SvResetVotingForNextElection()
{
	CryLog("CGameLobby::SvResetVotingForNextElection()");

	CRY_ASSERT(m_server);
	CRY_ASSERT(m_votingEnabled);

	CPlaylistManager*  plMgr = g_pGame->GetPlaylistManager();
	CRY_ASSERT(plMgr);
	CRY_ASSERT(plMgr->HavePlaylistSet());
	ILevelRotation*  pLevelRotation = plMgr->GetLevelRotation();
	CRY_ASSERT(pLevelRotation);

	m_votingClosed = false;
	m_leftVoteChoice.Reset();
	m_rightVoteChoice.Reset();
	m_highestLeadingVotesSoFar = 0;
	m_leftHadHighestLeadingVotes = false;

	SetLocalVoteStatus(eLVS_notVoted);
	VOTING_DBG_LOG("[tlh] set m_localVoteStatus [3] to eLVS_notVoted");

	UpdateVoteChoices();

	UpdateVotingCandidatesFlashInfo();
	UpdateVotingInfoFlashInfo();

}

//-------------------------------------------------------------------------
void CGameLobby::SvCloseVotingAndDecideWinner()
{
	CryLog("CGameLobby::SvCloseVotingAndDecideWinner()");

	CRY_ASSERT(m_server);
	CRY_ASSERT(m_votingEnabled);
	CRY_ASSERT(!m_votingClosed);

	m_votingClosed = true;

	if (m_leftVoteChoice.m_numVotes != m_rightVoteChoice.m_numVotes)
	{
		m_leftWinsVotes = (m_leftVoteChoice.m_numVotes > m_rightVoteChoice.m_numVotes);
	}
	else
	{
		CryLog(".. voting is drawn at %d a piece", m_leftVoteChoice.m_numVotes);

		if (m_highestLeadingVotesSoFar > 0)
		{
			CryLog(".... the [%s] choice was the first to get to %d, so setting that as the winner!", (m_leftHadHighestLeadingVotes ? "left" : "right"), m_highestLeadingVotesSoFar);
			m_leftWinsVotes = m_leftHadHighestLeadingVotes;
		}
		else
		{
			m_leftWinsVotes = (cry_random(0, 1) == 0);
			CryLog(".... neither choice was ever winning (there were probably no votes), so picking at random... chose [%s]", (m_leftWinsVotes ? "left" : "right"));
		}
	}

	UpdateRulesAndMapFromVoting();
}

//-------------------------------------------------------------------------
void CGameLobby::AdvanceLevelRotationAccordingToVotingResults()
{
	ILevelRotation*  pLevelRotation = g_pGame->GetPlaylistManager()->GetLevelRotation();
	CRY_ASSERT(pLevelRotation);

	if (m_server)
	{
		CRY_ASSERT(m_votingEnabled);
		// If the voting hasn't closed yet, close it now (possible if the start timer length was changed using the console)
		if (!m_votingClosed)
		{
			SvCloseVotingAndDecideWinner();
		}

		if (!m_leftWinsVotes)
		{
			// if right wins we need to advance the rotation BEFORE updating the rotation and session, so we skip over the losing (left) choice
			if (!pLevelRotation->Advance())
			{
				pLevelRotation->First();
			}
		}

		UpdateLevelRotation();

		if (m_leftWinsVotes)
		{
			// if left wins we need to advance the rotation AFTER updating the rotation, so we skip over the losing (right) choice next time
			if (!pLevelRotation->Advance())
			{
				pLevelRotation->First();
			}
		}

		m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
	}
	else
	{
		// Advance the rotation twice for the client so it moves onto the next pair of levels ready for the next vote.
		// Note that the server does actually Advance twice too (above), but one of those times is buried inside its call to UpdateLevelRotation().

		if (!pLevelRotation->Advance())
		{
			pLevelRotation->First();
		}

		if (!pLevelRotation->Advance())
		{
			pLevelRotation->First();
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::StartGame()
{
	CCCPOINT(GameLobby_StartGame);

	ICVar *pCVar = gEnv->pConsole->GetCVar("sv_gamerules");
	if (pCVar)
	{
		pCVar->Set(m_currentGameRules.c_str());
	}

	CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
	if (pPlaylistManager)
	{
		pPlaylistManager->SetModeOptions();

		pPlaylistManager->IgnoreCVarChanges(true);

		pCVar = gEnv->pConsole->GetCVar("sv_maxplayers");
		if (pCVar)
		{
			uint32 maxPlayers = m_sessionData.m_numPublicSlots + m_sessionData.m_numPrivateSlots;
			pCVar->Set((int) maxPlayers);
		}

		pPlaylistManager->IgnoreCVarChanges(false);
	}

	if(!m_sessionHostOnly)
	{
		CryFixedStringT<128> command;
		command.Format("map %s s nb", GetCurrentLevelName());
		gEnv->pConsole->ExecuteString(command.c_str());

		m_hasReceivedMapCommand = false;
	}
	else
	{
		// session host means join a server, not become one
		g_pGame->GetIGameFramework()->ExecuteCommandNextFrame(m_joinCommand);
	}
}

//-------------------------------------------------------------------------
bool CGameLobby::ShouldCallMapCommand( const char * pLevelName, const char *pGameRules )
{
	if(gEnv->IsDedicated())
	{
		// dedicated servers allocated by the arbitrator do not need to create a session
		// need a better way of determining if the server needs to create a session or just
		// let the map command through
		if(s_pGameLobbyCVars->gl_serverIsAllocatedFromDedicatedServerArbitrator != 0)
		{
			return true;
		}
	}

	const bool bIsInSession = (m_currentSession != CrySessionInvalidHandle);
	const bool bIsServer = (m_server);

	if (bIsInSession)
	{
		if (bIsServer)
		{
			if (m_bSessionStarted && (m_state == eLS_Game))
			{
				return true;
			}
			else
			{
				// We're the host of this session, begin starting the game
				if (m_state == eLS_Lobby)
				{
					SetState(eLS_JoinSession);
				}
			}
		}
		else
		{
			// We're in a session and we're not the host, leave this one and create our own
			LeaveSession(true, false);
			SetMatchmakingGame(false);
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Create, false);
		}
	}
	else
	{
		if(!m_taskQueue.HasTaskInProgress() || m_taskQueue.GetCurrentTask() != CLobbyTaskQueue::eST_Create)
		{
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Create, true);

			NOTIFY_UILOBBY_MP(ShowLoadingDialog("CreateSession"));

			if (gEnv->IsDedicated())
			{
				UpdatePrivatePasswordedGame();
			}
		}
		else
		{
			CryLog("create session is already in progress, not adding a new one");
		}
	}

	m_hasReceivedMapCommand = true;
	SetCurrentLevelName(pLevelName);
	ChangeGameRules(pGameRules);

	return false;
}

//-------------------------------------------------------------------------
void CGameLobby::OnMapCommandIssued()
{
	IGameFramework *pFramework = g_pGame->GetIGameFramework();
	if (pFramework->StartedGameContext() || pFramework->StartingGameContext())
	{
		// Already in a started session, need to end it before the next one loads
		gEnv->pConsole->ExecuteString("unload", false, false);
		SetState(eLS_EndSession);
		m_bServerUnloadRequired = false;
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SetupSessionData()
{
	if (!stricmp(m_currentGameRules.c_str(), "SinglePlayer"))
	{
		ChangeGameRules("TeamInstantAction"); // Default to TIA
	}

	m_sessionData.m_numData = 0;

	m_userData[eLDI_Gamemode].m_id = LID_MATCHDATA_GAMEMODE;
	m_userData[eLDI_Gamemode].m_type = eCLUDT_Int32;
	m_userData[eLDI_Gamemode].m_int32 = GameLobbyData::ConvertGameRulesToHash(m_currentGameRules.c_str());
	++m_sessionData.m_numData;

	m_userData[eLDI_Version].m_id = LID_MATCHDATA_VERSION;
	m_userData[eLDI_Version].m_type = eCLUDT_Int32;
	m_userData[eLDI_Version].m_int32 = GameLobbyData::GetVersion();
	++m_sessionData.m_numData;

	m_userData[eLDI_Playlist].m_id = LID_MATCHDATA_PLAYLIST;
	m_userData[eLDI_Playlist].m_type = eCLUDT_Int32;
	m_userData[eLDI_Playlist].m_int32 = GameLobbyData::GetPlaylistId();
	++m_sessionData.m_numData;

	m_userData[eLDI_Variant].m_id = LID_MATCHDATA_VARIANT;
	m_userData[eLDI_Variant].m_type = eCLUDT_Int32;
	m_userData[eLDI_Variant].m_int32 = GameLobbyData::GetVariantId();
	++m_sessionData.m_numData;

	m_userData[eLDI_RequiredDLCs].m_id = LID_MATCHDATA_REQUIRED_DLCS;
	m_userData[eLDI_RequiredDLCs].m_type = eCLUDT_Int32;
	m_userData[eLDI_RequiredDLCs].m_int32 = g_pGame->GetDLCManager()->GetRequiredDLCs();
	++m_sessionData.m_numData;

#if GAMELOBBY_USE_COUNTRY_FILTERING
	m_userData[eLDI_Country].m_id = LID_MATCHDATA_COUNTRY;
	m_userData[eLDI_Country].m_type = eCLUDT_Int32;
	m_userData[eLDI_Country].m_int32 = g_pGame->GetUserRegion();
	++m_sessionData.m_numData;
#endif

	m_userData[eLDI_Language].m_id = LID_MATCHDATA_LANGUAGE;
	m_userData[eLDI_Language].m_type = eCLUDT_Int32;
	m_userData[eLDI_Language].m_int32 = GetCurrentLanguageId();
	++m_sessionData.m_numData;

	m_userData[eLDI_Map].m_id = LID_MATCHDATA_MAP;
	m_userData[eLDI_Map].m_type = eCLUDT_Int32;
	m_userData[eLDI_Map].m_int32 = GameLobbyData::ConvertMapToHash(m_currentLevelName.c_str());
	++m_sessionData.m_numData;

	m_userData[eLDI_Skill].m_id = LID_MATCHDATA_SKILL;
	m_userData[eLDI_Skill].m_type = eCLUDT_Int32;
	m_userData[eLDI_Skill].m_int32 = CPlayerProgression::GetInstance()->GetData(EPP_SkillRank);
	++m_sessionData.m_numData;

	m_userData[eLDI_Active].m_id = LID_MATCHDATA_ACTIVE;
	m_userData[eLDI_Active].m_type = eCLUDT_Int32;
	m_userData[eLDI_Active].m_int32 = 0;
	++m_sessionData.m_numData;

	m_sessionData.m_data = m_userData;

	ICVar* pMaxPlayers = gEnv->pConsole->GetCVar("sv_maxplayers");

	if(!IsPrivateGame())
	{
		m_sessionData.m_numPublicSlots = pMaxPlayers ? pMaxPlayers->GetIVal() : MAX_PLAYER_LIMIT;
		m_sessionData.m_numPrivateSlots = 0;
	}
	else
	{
		m_sessionData.m_numPublicSlots = 0;
		m_sessionData.m_numPrivateSlots = pMaxPlayers ? pMaxPlayers->GetIVal() : MAX_PLAYER_LIMIT;
	}

	memset( m_sessionData.m_name, 0, MAX_SESSION_NAME_LENGTH );
	ICVar* pServerNameCVar = gEnv->pConsole->GetCVar("sv_servername");
	if (pServerNameCVar)
	{
		if (pServerNameCVar->GetFlags() & VF_MODIFIED)
		{
			cry_strcpy(m_sessionData.m_name, pServerNameCVar->GetString());
		}
		else
		{
			IPlatformOS::TUserName userName;

			IPlayerProfileManager *pProfileManager = g_pGame->GetIGameFramework()->GetIPlayerProfileManager();
			if (pProfileManager)
			{
				const uint32 userIndex = pProfileManager->GetExclusiveControllerDeviceIndex();

				IPlatformOS *pPlatformOS = GetISystem()->GetPlatformOS();
				if (!pPlatformOS->UserGetOnlineName(userIndex, userName))
				{
					pPlatformOS->UserGetName(userIndex, userName);
				}
			}

			cry_strcpy(m_sessionData.m_name, userName.c_str());
		}
	}
	else
	{
		cry_strcpy(m_sessionData.m_name, "default servername");
	}

	m_sessionData.m_ranked = false;

	if( CMatchMakingHandler* pMMHandler = m_gameLobbyMgr->GetMatchMakingHandler() )
	{
		pMMHandler->AdjustCreateSessionData( &m_sessionData, eLDI_Num );
	}

}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoCreateSession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoCreateSession() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Create);

	uint32 users = g_pGame->GetExclusiveControllerDeviceIndex();

	SetupSessionData();


	m_nameList.Clear();
	m_sessionFavouriteKeyId = INVALID_SESSION_FAVOURITE_ID;

	SetState(eLS_Initializing);
	ECryLobbyError result = pMatchMaking->SessionCreate(&users, 1, GetSessionCreateFlags(), &m_sessionData, &taskId, CGameLobby::MatchmakingSessionCreateCallback, this);
	CryLog("CGameLobby::CreateSession() error=%i, Lobby: %p", result, this);

	// If the error is too many tasks, we'll try again next frame so don't need to clear the dialog
	if ((result != eCLE_Success) && (result != eCLE_TooManyTasks))
	{
		if (!m_bMatchmakingSession)
		{
			NOTIFY_UILOBBY_MP(HideLoadingDialog("CreateSession"));
		}

		if( m_bMatchmakingSession )
		{
			if( CMatchMakingHandler* pMMHandler = m_gameLobbyMgr->GetMatchMakingHandler() )
			{
				pMMHandler->GameLobbyCreateFinished( result );
			}
		}
	}

	return result;
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoMigrateSession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoMigrateSession() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Migrate);

	uint32 users = g_pGame->GetExclusiveControllerDeviceIndex();

	ECryLobbyError result = pMatchMaking->SessionMigrate(m_currentSession, &users, 1, GetSessionCreateFlags(), &m_sessionData, &taskId, CGameLobby::MatchmakingSessionMigrateCallback, this);

	return result;
}

//-------------------------------------------------------------------------
uint32 CGameLobby::GetSessionCreateFlags() const
{
	uint32 flags = CRYSESSION_CREATE_FLAG_NUB | CRYSESSION_CREATE_FLAG_CAN_SEND_SERVER_PING;

#if GAME_LOBBY_ALLOW_MIGRATION
	if (!gEnv->IsDedicated() && s_pGameLobbyCVars->gl_getServerFromDedicatedServerArbitrator)
	{
		flags |= CRYSESSION_CREATE_FLAG_MIGRATABLE;
	}
#endif

	if (!IsPrivateGame() || IsPasswordedGame())
	{
		flags |= CRYSESSION_CREATE_FLAG_SEARCHABLE;
	}

	return flags;
}

//-------------------------------------------------------------------------
void CGameLobby::SetPrivateGame( bool enable )
{
	CryLog("CGameLobby::SetPrivateGame(enable=%s)", (enable?"true":"false"));
	m_privateGame = enable;
	m_gameLobbyMgr->SetPrivateGame(this, enable);
}

//-------------------------------------------------------------------------
CryUserID CGameLobby::GetHostUserId()
{
	CryUserID result = CryUserInvalidID;

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby)
	{
		ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
		if (pMatchmaking)
		{
			SCryMatchMakingConnectionUID hostUID = pMatchmaking->GetHostConnectionUID(m_currentSession);
			GetUserIDFromConnectionUID(hostUID, result);
		}
	}

	return result;
}

//-------------------------------------------------------------------------
const char* CGameLobby::GetCurrentGameModeName(const char* unknownStr/*="Unknown"*/)
{
	if (!m_currentGameRules.empty())
	{
		return m_currentGameRules.c_str();
	}
	return unknownStr;
}

//-------------------------------------------------------------------------
void CGameLobby::CancelLobbyTask(CLobbyTaskQueue::ESessionTask taskType)
{
	CryLog("CancelLobbyTask %d", taskType);

	// If this is the task that's running, might need to cancel the matchmaking task
	if (m_taskQueue.GetCurrentTask() == taskType)
	{
		CRY_ASSERT_MESSAGE(taskType != CLobbyTaskQueue::eST_Create && taskType != CLobbyTaskQueue::eST_Join && taskType != CLobbyTaskQueue::eST_Delete, "Trying to cancel a lobby task that shouldn't be canceled");

		CryLobbyTaskID networkTaskId = m_currentTaskId;
		CryLog("currentTaskId %d", m_currentTaskId);
		if (networkTaskId != CryLobbyInvalidTaskID)
		{
			ICryLobby *lobby = gEnv->pNetwork->GetLobby();
			if (lobby != NULL && lobby->GetLobbyService())
			{
				CryLog("CGameLobby::CancelLobbyTask taskType=%u, networkTaskId=%u Lobby: %p", taskType, networkTaskId, this);

				lobby->GetLobbyService()->GetMatchMaking()->CancelTask(networkTaskId);
				m_currentTaskId = CryLobbyInvalidTaskID;
			}
		}
	}

	// Now remove the task from our queue
	m_taskQueue.CancelTask(taskType);
}

//-------------------------------------------------------------------------
void CGameLobby::CancelAllLobbyTasks()
{
	CryLog("CGameLobby::CancelAllLobbyTasks Lobby: %p", this);

	if (m_taskQueue.HasTaskInProgress())
	{
		CancelLobbyTask(m_taskQueue.GetCurrentTask());
	}

	m_taskQueue.Reset();

	NOTIFY_UILOBBY_MP(SearchCompleted());

	if (m_gameLobbyMgr->IsPrimarySession(this))
	{
		CGameBrowser *pGameBrowser = g_pGame->GetGameBrowser();

		if ( pGameBrowser )
		{
			pGameBrowser->CancelSearching();
		}

		CGameLobby::s_bShouldBeSearching = true;
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SetCurrentSession(CrySessionHandle h)
{
	m_currentSession = h;

	if (h == CrySessionInvalidHandle)  // there's no point in holding on to an invalid id if the handle is being made invalid... (in fact not doing this was causing bugs such as a new squad member being dragged into a game that the leader was no longer in when re-joining the squad)
	{
		SetCurrentId(CrySessionInvalidID, false, false);
	}
	else
	{
		if (!m_server)
		{
			m_countdownStage = eCDS_WaitingForPlayers;
			SendPacket(eGUPD_ReservationClientIdentify);	//Always identify (otherwise need to know if we have a reservation)
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SetLocalUserData(uint8 * localUserData)
{
	CPlayerProgression* pPlayerProgression = CPlayerProgression::GetInstance();
	localUserData[eLUD_LoadedDLCs] = (uint8)g_pGame->GetDLCManager()->GetLoadedDLCs();

	// ClanTag
	CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
	if (pProfileOptions)
	{
		CryFixedStringT<32> option("MP/OperativeStatus/ClanTag");
		if(pProfileOptions->IsOption(option.c_str()))
		{
			CryFixedStringT<CLAN_TAG_LENGTH> clanTag(pProfileOptions->GetOptionValue(option.c_str()).c_str() ); // 4 chars + terminator
			char *str = clanTag.begin();
			int length = clanTag.length();

			for(int i = 0, idx = eLUD_ClanTag1; i < length; ++i, ++idx)
			{
				localUserData[idx] = str[i];
			}
		}
	}

	int skillRank = CPlayerProgression::GetInstance()->GetData(EPP_SkillRank);
	skillRank = CLAMP(skillRank, 0, 0xFFFF);		// Should be impossible to get outside this range, but just to be sure ...
	localUserData[eLUD_SkillRank1] = (skillRank & 0xFF);
	localUserData[eLUD_SkillRank2] = ((skillRank & 0xFF00) >> 8);
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoUpdateLocalUserData(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoUpdateLocalUserData() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SetLocalUserData);

	uint8 localUserData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES] = {0};

	SetLocalUserData(localUserData);

	localUserData[eLUD_VoteChoice] = (uint8)m_networkedVoteStatus;

	uint32 squadLeaderUID = 0;
	CryUserID pSquadLeaderId = g_pGame->GetSquadManager()->GetSquadLeader();
	if (pSquadLeaderId.IsValid())
	{
		SCryMatchMakingConnectionUID conId;
		if (GetConnectionUIDFromUserID(pSquadLeaderId, conId))
		{
			squadLeaderUID = conId.m_uid;
			m_squadDirty = false;
		}
	}
	else
	{
		m_squadDirty = false;
	}
	localUserData[eLUD_SquadId1] = (squadLeaderUID & 0xFF);
	localUserData[eLUD_SquadId2] = ((squadLeaderUID & 0xFF00) >> 8);

	ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
	uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();
	ECryLobbyError result = pMatchmaking->SessionSetLocalUserData(m_currentSession, &taskId, userIndex, localUserData, sizeof(localUserData), MatchmakingLocalUserDataCallback, this);

	return result;
}

//-------------------------------------------------------------------------
void CGameLobby::SetCurrentId(CrySessionID id, bool isCreator, bool isMigratedSession)
{
	CryLog("[tlh] CGameLobby::SetCurrentId(CrySessionID id = %p)", id.get());

	m_currentSessionId = id;
	m_bMigratedSession = isMigratedSession;
}

//-------------------------------------------------------------------------
bool CGameLobby::IsCurrentSessionId(CrySessionID id)
{
	bool retval = false;
	if( m_currentSessionId != CrySessionInvalidID )
	{
		retval = ( *m_currentSessionId.get() == *id.get() );
	}
	return retval;
}

//-------------------------------------------------------------------------
// Connect to a server
bool CGameLobby::JoinServer(CrySessionID sessionId, const char *sessionName, const SCryMatchMakingConnectionUID &reservationId, bool bRetryIfPassworded )
{
	CryLog("CGameLobby::JoinServer(CrySessionID sessionId = %p, sessionName = %s)", sessionId.get(), sessionName);

	if (!sessionId)
	{
		CryLog("  NULL sessionId requested");
		return false;
	}

	if (GameNetworkUtils::CompareCrySessionId(m_currentSessionId, sessionId))
	{
		CryLog("  already in requested session");
		return false;
	}

	m_pendingConnectSessionId = sessionId;
	m_pendingConnectSessionName = sessionName ? sessionName : " ";
	m_pendingReservationId = reservationId;

	m_bRetryIfPassworded = bRetryIfPassworded;

	// Show joining dialog regardless of whether we can join immediately
	if (m_gameLobbyMgr->IsPrimarySession(this) && !m_bMatchmakingSession)
	{
		CryLog("Trying to JoinServer %s", m_pendingConnectSessionName.c_str());

		NOTIFY_UILOBBY_MP(ShowLoadingDialog("JoinSession"));
	}

	if (m_currentSession != CrySessionInvalidHandle)
	{
		LeaveSession(false, false);
	}

	m_taskQueue.AddTask(CLobbyTaskQueue::eST_Join, false);

#if !defined(_RELEASE)
	if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
	{
		gEnv->pConsole->GetCVar("net_hostHintingActiveConnectionsOverride")->Set(2);
		CryLog("  setting net_hostHintingActiveConnectionsOverride to 2");
	}
#endif

	return true;
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoJoinServer(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoJoinServer() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Join);

	// Request a server join on the lobby system.
	CCCPOINT (GameLobby_JoinServer);

	uint32 user = g_pGame->GetExclusiveControllerDeviceIndex();

	SetState(eLS_Initializing);

	m_shouldFindGames = false;

	m_server = false;
	m_connectedToDedicatedServer = false;
	CryLog("JoinServer - clear the name list Lobby: %p", this);
	m_nameList.Clear();

	ECryLobbyError joinResult = pMatchMaking->SessionJoin(&user, 1, CRYSESSION_CREATE_FLAG_SEARCHABLE | CRYSESSION_CREATE_FLAG_NUB, m_pendingConnectSessionId, &taskId, CGameLobby::MatchmakingSessionJoinCallback, this);

	if ((joinResult != eCLE_Success) && (joinResult != eCLE_TooManyTasks))
	{
		CryLog("CGameLobby::DoJoinServer() failed to join server, error=%i", joinResult);
		JoinServerFailed(joinResult, m_pendingConnectSessionId);
		m_pendingConnectSessionId = CrySessionInvalidID;
		m_pendingConnectSessionName.clear();

		// Hide joining dialog
		if (m_gameLobbyMgr->IsPrimarySession(this) && !m_bMatchmakingSession)
		{
			NOTIFY_UILOBBY_MP(HideLoadingDialog("JoinSession"));
		}

		if( m_bMatchmakingSession )
		{
			if( CMatchMakingHandler* pMMHandler = m_gameLobbyMgr->GetMatchMakingHandler() )
			{
				pMMHandler->GameLobbyJoinFinished( joinResult );
			}
		}
	}

	return joinResult;
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoUpdateSession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoUpdateSession() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Update);

	CRY_ASSERT(m_currentSession != CrySessionInvalidHandle);
	CRY_ASSERT(m_server);

	const EActiveStatus activeStatus = GetActiveStatus(m_state);
	m_lastActiveStatus = activeStatus;
	const int averageSkill = CalculateAverageSkill();
	m_lastUpdatedAverageSkill = averageSkill;

	uint32	numData = 0;

	m_userData[eLDI_Gamemode].m_int32 = GameLobbyData::ConvertGameRulesToHash(m_currentGameRules.c_str());
	++numData;
	m_userData[eLDI_Map].m_int32 = GameLobbyData::ConvertMapToHash(m_currentLevelName.c_str());
	++numData;
	m_userData[eLDI_Active].m_int32 = (int32)activeStatus;
	++numData;
	m_userData[eLDI_Version].m_int32 = GameLobbyData::GetVersion();
	++numData;
	m_userData[eLDI_RequiredDLCs].m_int32 = g_pGame->GetDLCManager()->GetRequiredDLCs();
	++numData;
	m_userData[eLDI_Playlist].m_int32 = GameLobbyData::GetPlaylistId();
	++numData;
	m_userData[eLDI_Variant].m_int32 = GameLobbyData::GetVariantId();
	++numData;
	m_userData[eLDI_Skill].m_int32 = averageSkill;
	++numData;
	m_userData[eLDI_Language].m_int32 = GetCurrentLanguageId();
	++numData;
#if GAMELOBBY_USE_COUNTRY_FILTERING
	m_userData[eLDI_Country].m_int32 = g_pGame->GetUserRegion();
	++numData;
#endif

	if( CMatchMakingHandler* pMMHandler = m_gameLobbyMgr->GetMatchMakingHandler() )
	{
		pMMHandler->AdjustCreateSessionData( &m_sessionData, eLDI_Num );
	}

	int maxPlayers = m_sessionData.m_numPublicSlots + m_sessionData.m_numPrivateSlots;
	m_teamBalancing.SetLobbyPlayerCounts(maxPlayers);

	ECryLobbyError result = pMatchMaking->SessionUpdate(m_currentSession, &m_userData[0], numData, &taskId, CGameLobby::MatchmakingSessionUpdateCallback, this);
	return result;
}

//-------------------------------------------------------------------------
// Delete the current session.
ECryLobbyError CGameLobby::DoDeleteSession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId)
{
	CryLog("CGameLobby::DoDeleteSession() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Delete);

	// Request a server delete in the lobby system.
	ECryLobbyError result = pMatchMaking->SessionDelete(m_currentSession, &taskId, CGameLobby::MatchmakingSessionDeleteCallback, this);

	if ((result != eCLE_Success) && (result != eCLE_TooManyTasks))
	{
		FinishDelete(result);
	}

	return result;
}

//-------------------------------------------------------------------------
// Migrate callback.
void CGameLobby::MatchmakingSessionMigrateCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CryLog("MatchmakingSessionMigrateCallback pLobby %p error %d", arg, error);

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);
	if (pLobby)
	{
		if (pLobby->NetworkCallbackReceived(taskID, error))
		{
			CRY_ASSERT(pLobby->m_currentSession == h);

			pLobby->m_server = true;
			pLobby->m_connectedToDedicatedServer = false;
			pLobby->GameRulesChanged(pLobby->m_currentGameRules.c_str());

#if defined(TRACK_MATCHMAKING)
			if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
			{
				pMMTel->AddEvent( SMMBecomeHostEvent() );
			}
#endif
		}
	}
}

//-------------------------------------------------------------------------
// Create callback.
// Starts the server game.
void CGameLobby::MatchmakingSessionCreateCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);

	CryLog("CGameLobby::MatchmakingSessionCreateCallback() result=%i Lobby: %p", error, pLobby);
	if (pLobby->NetworkCallbackReceived(taskID, error))
	{
		ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
		CryLog("  calling SetCurrentSession() h=%u Lobby: %p", h, pLobby);

		pLobby->m_server = true;
		pLobby->m_connectedToDedicatedServer = false;
		pLobby->m_findGameNumRetries = 0;

		pLobby->SetCurrentSession(h);
		pLobby->SetCurrentId(pMatchmaking->SessionGetCrySessionIDFromCrySessionHandle(h), true, false);

		if(!pLobby->m_bCancelling)
		{
			// only non-dedicated server should request server from arbitrator
			pLobby->m_sessionHostOnly = !gEnv->IsDedicated() ? s_pGameLobbyCVars->gl_getServerFromDedicatedServerArbitrator !=  0 : false;

			pLobby->m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionSetLocalFlags, false);
			pLobby->m_bNeedToSetAsElegibleForHostMigration = false;

			if(pLobby->m_bMatchmakingSession)
			{
				//If this is not set, we haven't created a session from 'Find Game', and so
				//	don't want to search for more lobbies
				CryLog("  > Setting ShouldBeSearching to TRUE to look for further lobbies to merge / join");
				CGameLobby::s_bShouldBeSearching = true;
				pLobby->m_shouldFindGames = true;
			}
			else
			{
				CryLog("  > Setting ShouldBeSearching to FALSE as we're not a matchmaking session");
				CGameLobby::s_bShouldBeSearching = false;
			}

			{
				if(pLobby->IsPrivateGame())
				{
					CSquadManager *pSquadManager = g_pGame->GetSquadManager();
					if(pSquadManager)
					{
						pSquadManager->SessionChangeSlotType(CSquadManager::eSST_Private);
					}
				}
#if 0 // LEVEL ROTATION DISABLED FOR NOW
				CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
				uint32 playListSeed = (uint32)(gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetValue());
				playListSeed &= 0x7FFFFFFF;	// Make sure this doesn't become negative when converted to int32
				if (pPlaylistManager)
				{
					ILevelRotation* pLevelRotation = pPlaylistManager->GetLevelRotation();
					if (pLevelRotation != NULL && pLevelRotation->IsRandom())
					{
						pLevelRotation->Shuffle(playListSeed);
					}
				}
				pLobby->m_playListSeed = playListSeed;
#endif
				pLobby->SetState(eLS_Lobby);
				pLobby->GameRulesChanged(pLobby->m_currentGameRules.c_str());

#if 0 // LEVEL ROTATION DISABLED FOR NOW
				if (pPlaylistManager)
				{
					if (pPlaylistManager->GetActiveVariantIndex() == -1)
					{
						// Set default variant
						int defaultVariant = pPlaylistManager->GetDefaultVariant();
						if (defaultVariant != -1)
						{
							pPlaylistManager->ChooseVariant(defaultVariant);
						}
					}

					pPlaylistManager->SetModeOptions();
				}
#endif
				cry_strcpy(pLobby->m_detailedServerInfo.m_motd, g_pGameCVars->g_messageOfTheDay->GetString());
				cry_strcpy(pLobby->m_detailedServerInfo.m_url, g_pGameCVars->g_serverImageUrl->GetString());

#if defined(TRACK_MATCHMAKING)
				//we have made a session, record this successful decison
				if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
				{
					if( pMMTel->GetCurrentTranscriptType() == CMatchmakingTelemetry::eMMTelTranscript_QuickMatch )
					{
						pMMTel->AddEvent( SMMChosenSessionEvent( pLobby->m_sessionData.m_name, pLobby->m_currentSessionId, ORIGINAL_MATCHMAKING_DESC, true, pLobby->s_currentMMSearchID, pLobby->m_gameLobbyMgr->IsPrimarySession(pLobby) ) );
					}
				}
#endif

			}

#if 0 // old frontend
			if (mpMenuHub && !pLobby->m_bMatchmakingSession)
			{
				mpMenuHub->HideLoadingDialog("CreateSession");
			}

			if(menu != NULL && menu->IsScreenInStack("game_lobby")==false)
			{
				menu->Execute(CFlashFrontEnd::eFECMD_goto, "game_lobby");
			}
#endif

			pLobby->InformSquadManagerOfSessionId();
			pLobby->RefreshCustomiseEquipment();

			pLobby->m_bIsAutoStartingGame = pLobby->IsMatchmakingGame() || gEnv->IsDedicated();
		}
	}
	else
	{
		if (gEnv->IsDedicated())
		{
			if (error == eCLE_UserNotSignedIn)
			{
				CryFatalError("User not signed in - please check dedicated.cfg credentials, and sv_bind in system.cfg is correct (if needed)");
			}
			else
			{
				CryLogAlways("Failed to create a game, quitting");
				gEnv->pSystem->Quit();
			}
		}
		else if (error != eCLE_TimeOut)
		{
			NOTIFY_UILOBBY_MP(HideLoadingDialog("CreateSession"));
		}
	}

	if( pLobby->m_bMatchmakingSession )
	{
		if( CMatchMakingHandler* pMMHandler = pLobby->m_gameLobbyMgr->GetMatchMakingHandler() )
		{
			pMMHandler->GameLobbyCreateFinished( error );
		}
	}
}

//-------------------------------------------------------------------------
// Do the actual game-connect on successful find.
// arg is CGameLobby*
void CGameLobby::MatchmakingSessionJoinCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, uint32 ip, uint16 port, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);

	CryLog("CGameLobby::MatchmakingSessionJoinCallback() result=%i Lobby: %p", error, pLobby);
	if (pLobby->NetworkCallbackReceived(taskID, error))
	{
		CryLog("CGameLobby::MatchmakingSessionJoinCallback");
		CryLog("  calling SetCurrentSession() h=%u Lobby: %p", h, pLobby);

		ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();

		// set session info
		pLobby->SetCurrentSession(h);
		pLobby->SetCurrentId(pMatchmaking->SessionGetCrySessionIDFromCrySessionHandle(h), false, false);

		pLobby->m_allowRemoveUsers = true;

		if(!pLobby->m_bCancelling)
		{
			// need the name for loading screen
			pLobby->m_currentSessionName = pLobby->m_pendingConnectSessionName;

			// Clear pending join info
			pLobby->m_pendingConnectSessionId = CrySessionInvalidID;
			pLobby->m_pendingConnectSessionName.clear();

			pLobby->m_bNeedToSetAsElegibleForHostMigration = true;

			pLobby->SetState(eLS_Lobby);

			if(pLobby->m_gameLobbyMgr->IsNewSession(pLobby))
			{
				CGameLobby *pLobby2 = g_pGame->GetGameLobby();
				SSessionNames *pSessionNames = &pLobby2->m_nameList;
				if (pSessionNames->Size() > 1)
				{
					CryLog("    making reservations Lobby: %p", pLobby);
					//Make reservations for our primary (the one we are hosting) namelist
					pLobby->MakeReservations(pSessionNames, false);
				}
				else
				{
					CryLog("    no need to make reservations, continuing with merge Lobby: %p", pLobby);
					pLobby->m_gameLobbyMgr->CompleteMerge(pLobby->m_currentSessionId);
				}
			}

			cry_sprintf(pLobby->m_joinCommand, sizeof(pLobby->m_joinCommand), "connect <session>%08X,%d.%d.%d.%d:%d", h, ((uint8*)&ip)[0], ((uint8*)&ip)[1], ((uint8*)&ip)[2], ((uint8*)&ip)[3], port);

			pLobby->InformSquadManagerOfSessionId();

			pLobby->CancelDetailedServerInfoRequest();	// don't want this anymore
		}

		pLobby->m_findGameNumRetries = 0;
	}
	else if (error != eCLE_TimeOut)
	{
		pLobby->JoinServerFailed(error, pLobby->m_pendingConnectSessionId);

		pLobby->m_allowRemoveUsers = true;
	}
	pLobby->m_bRetryIfPassworded = false;

	if( pLobby->m_bMatchmakingSession )
	{
		if( CMatchMakingHandler* pMMHandler = pLobby->m_gameLobbyMgr->GetMatchMakingHandler() )
		{
			pMMHandler->GameLobbyJoinFinished( error );
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::JoinServerFailed(ECryLobbyError error, CrySessionID serverSessionId)
{
	CryLog("GameLobby session join error %d", (int)error);

	// don't add full servers to the bad list, they won't be returned
	// if still full the next time we search, it was likely that we just tried to
	// join at the same time as someone else into the remaining slot,
	// if its empty next time, then we might still have a chance
	// of joining
	if(error != eCLE_SessionFull)
	{
		if(serverSessionId != CrySessionInvalidID)
		{
			if((int)m_badServers.size() > m_badServersHead)
			{
				m_badServers[m_badServersHead] = serverSessionId;
			}
			else
			{
				m_badServers.push_back(serverSessionId);
			}
			m_badServersHead = (m_badServersHead + 1) % k_maxBadServers;
#if defined(TRACK_MATCHMAKING)
			if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
			{
				pMMTel->AddEvent( SMMServerConnectFailedEvent( serverSessionId, m_gameLobbyMgr->IsPrimarySession(this), error ) );
			}
#endif
		}
	}

	ICVar *pCVar = gEnv->pConsole->GetCVar("sv_password");
	if (pCVar != NULL && ((pCVar->GetFlags() & VF_WASINCONFIG) == 0))
	{
		pCVar->Set("");
	}

	if(m_gameLobbyMgr->IsNewSession(this))
	{
		m_gameLobbyMgr->NewSessionResponse(this, CrySessionInvalidID);
	}
	else //if IsPrimarySession
	{
		if (!m_bCancelling)
		{
			CSquadManager *pSquadManager = g_pGame->GetSquadManager();
			bool isSquadMember = pSquadManager != NULL && pSquadManager->InSquad() && !pSquadManager->InCharge() && !pSquadManager->IsLeavingSquad();

			if(isSquadMember)
			{
				CryLog("Failed to join GAME session our squad is in and we're a squad member, leaving squad...");
				pSquadManager->RequestLeaveSquad();
				ShowErrorDialog(error, NULL, NULL, this);	//auto removes Join Session Dialog
				LeaveSession(false, false);
#if 0 // old frontend
				CFlashFrontEnd *menu = g_pGame->GetFlashMenu();
				if(menu != NULL && menu->IsScreenInStack("game_lobby")==true)
				{
					if (CMPMenuHub *pMPMenu = CMPMenuHub::GetMPMenuHub())
					{
						pMPMenu->GoToCurrentLobbyServiceScreen(); // go to correct lobby service screen - play_online or play_lan
					}
				}
#endif
			}
			else
			{
				if( ! m_bMatchmakingSession )
				{
					ShowErrorDialog(error, NULL, NULL, this);	//auto removes Join Session Dialog
					LeaveSession(false, false); // don't clear pending tasks, depending on network conditions, it's possible we could have a valid create/join task in the queue to process next
				}
			}
		}
	}
	
	// Clear pending join info
	m_pendingConnectSessionId = CrySessionInvalidID;
	m_pendingConnectSessionName.clear();
}

//-------------------------------------------------------------------------
bool CGameLobby::IsBadServer(CrySessionID sessionId)
{
#ifndef _RELEASE
	if (s_pGameLobbyCVars->gl_debugLobbyRejoin)
	{
		// Ignore bad servers if we're doing a rejoin test
		return false;
	}
#endif

	if(!s_pGameLobbyCVars->gl_ignoreBadServers)
	{
		const int size = m_badServers.size();
		for(int i = 0; i < size; ++i)
		{
			//Compare the CrySessionId (not the smart pointers)
			if((*m_badServers[i].get()) == (*sessionId.get()))
			{
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Callback from deleting session
// arg is CGameLobby*
void CGameLobby::MatchmakingSessionDeleteCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);
	pLobby->NetworkCallbackReceived(taskID, error);

	if (error != eCLE_TimeOut)
	{
		pLobby->FinishDelete(error);

		CSquadManager *pSquadManager = g_pGame->GetSquadManager();
		if(pSquadManager)
		{
			pSquadManager->SessionChangeSlotType(CSquadManager::eSST_Public);	// make all slots public again if necessary
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::FinishDelete(ECryLobbyError result)
{
	CryLog("CGameLobby::FinishDelete() lobby:%p result from delete = %d", this, (int) result);

	CrySessionID oldSessionID = m_currentSessionId;

	SetCurrentSession(CrySessionInvalidHandle);
	m_server = false;
	m_connectedToDedicatedServer = false;
	m_votingEnabled = false;
	m_bWaitingForGameToFinish = false;
	m_isLeaving = false;
	m_bHasUserList = false;
	m_bHasReceivedVoteOptions = false;

	m_reservationList = NULL;
	m_squadReservation = false;
	for (int i = 0; i < MAX_RESERVATIONS; ++ i)
	{
		m_slotReservations[i].m_con = CryMatchMakingInvalidConnectionUID;
		m_slotReservations[i].m_timeStamp = 0.f;
	}

	m_nameList.Clear();
	m_sessionFavouriteKeyId = INVALID_SESSION_FAVOURITE_ID;

	m_teamBalancing.Reset();

	m_shouldFindGames = false;

	if (!m_gameLobbyMgr->IsMultiplayer())
	{
		m_taskQueue.Reset();
	}

	CryLog("Clearing internal m_userData and m_sessionData in CGameLobby::FinishDelete()");
	for (int i = 0; i < eLDI_Num; i++)
	{
		new (&m_userData[i]) SCryLobbyUserData();
	}
	new (&m_sessionData) SCrySessionData;

	ResetFlashInfos();

	m_countdownStage = eCDS_WaitingForPlayers;
	m_initialStartTimerCountdown = false;
	m_bCancelling = false;
	m_bQuitting = false;
	m_timeStartedWaitingForBalancedGame.SetValue(0LL);

	m_findGameNumRetries = 0;
	m_numPlayerJoinResets = 0;
#if GAMELOBBY_USE_COUNTRY_FILTERING
	m_timeSearchStarted.SetValue(0LL);
#endif

	m_dedicatedserverip = 0;
	m_dedicatedserverport = 0;
	m_sessionHostOnly = false;
	m_bAllocatedServer = false;
	m_bIsAutoStartingGame = false;

	m_allocatedServerUID = CryMatchMakingInvalidConnectionUID;

	if (!m_gameLobbyMgr->IsLobbyMerging())
	{
		if (CPlaylistManager* pPlaylistManager=g_pGame->GetPlaylistManager())
		{
			if (ILevelRotation* pLevelRotation=pPlaylistManager->GetLevelRotation())
			{
				pLevelRotation->First();  // leave level rotation in a sensible state after leaving in case we decide to head right back into the lobby to start a new session on this same 'list
			}
		}
	}

	// Remove any tasks that require a valid session handle (since we don't have one any more)
	m_taskQueue.ClearInternalSessionTasks();


	const int numTasks = m_taskQueue.GetCurrentQueueSize();
	if ((numTasks == 0) || ((numTasks == 1) && (m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Delete)))
	{
#ifndef _RELEASE
		if (s_pGameLobbyCVars->gl_debugLobbyRejoin)
		{
			SetState(eLS_FindGame);
		}
		else
#endif
		{
			SetState(eLS_None);

			//reasons for lobby delete are that we've started a match, abandoned the session, or are finishing a merge. Check it's not the last one
			if( m_bMatchmakingSession && !m_gameLobbyMgr->IsLobbyMerging() )
			{
				if( CMatchMakingHandler* pMMhandler = m_gameLobbyMgr->GetMatchMakingHandler() )
				{
					pMMhandler->OnLeaveMatchMaking();
				}

#if defined(TRACK_MATCHMAKING)
				CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry();

				if( pMMTel != NULL && pMMTel->GetCurrentTranscriptType() == CMatchmakingTelemetry::eMMTelTranscript_QuickMatch )
				{
					CTelemetryCollector		*pTC=static_cast<CTelemetryCollector*>( g_pGame->GetITelemetryCollector() );
					pTC->SetAbortedSessionId( m_currentSessionName.c_str(), oldSessionID );

					pMMTel->AddEvent( SMMLeaveMatchMakingEvent() );
					pMMTel->EndMatchmakingTranscript( CMatchmakingTelemetry::eMMTelTranscript_QuickMatch, true );
				}
#endif
			}


		}
	}

	m_currentSessionName.clear();	// don't need this anymore


	m_hasReceivedMapCommand = false;
	UnloadPrecachedPaks("Leaving Lobby");
}

//static-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionUpdateCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CRY_ASSERT(error == eCLE_Success);

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);
	if (pLobby->NetworkCallbackReceived(taskID, error))
	{
		pLobby->m_sessionUserDataDirty = true;

		// Tell clients that we've updated the session info
		pLobby->SendPacket(eGUPD_LobbyUpdatedSessionInfo);
	}
}

//-------------------------------------------------------------------------
void CGameLobby::InsertUser(SCryUserInfoResult* user)
{
	CryLog("CGameLobby::InsertUser() new player %s, uid=%u, sid=%llX, lobby:%p", user->m_userName, user->m_conID.m_uid, user->m_conID.m_sid, this);

	if (user->m_isDedicated)
	{
		m_connectedToDedicatedServer = true;
		CryLog("  user is dedicated, ignoring");
		return;
	}

	CAudioSignalPlayer::JustPlay("LobbyPlayerJoin");

	if (m_state == eLS_Leaving)
	{
		CryLog("CGameLobby::InsertUser() '%s' joining while we're in leaving state, ignoring, lobby=%p", user->m_userName, this);
		return;
	}

	// We might already have a dummy entry for this player (possible if we receive a SetTeam message before the user joined callback)
	bool foundByUserId = false;
	int playerIndex = m_nameList.Find(user->m_conID);

	if(playerIndex == SSessionNames::k_unableToFind)
	{
		CryLog("[GameLobby] Could not find user %s by ConnectionID, checking for CryUserID", user->m_userName);

		playerIndex = m_nameList.FindByUserId(user->m_userID);
		if(playerIndex != SSessionNames::k_unableToFind)
		{
			CryLog("[GameLobby] Found %s from CryUserID", user->m_userName);
			foundByUserId = true;
		}
	}

	if (playerIndex == SSessionNames::k_unableToFind)
	{
		if(m_nameList.Size() >= MAX_SESSION_NAMES)
		{
			CryLog("[GameLobby] More players than supported, possibly because of a merge, try to remove an invalid entry");
			bool success = m_nameList.RemoveEntryWithInvalidConnection();
			if(!success)
			{
				CRY_ASSERT_MESSAGE(0, string().Format("No players to remove, we have maxed out the session list with %d players", m_nameList.Size()));
			}
		}

		CryLog("    unknown player, adding");
		playerIndex = m_nameList.Insert(user->m_userID, user->m_conID, user->m_userName, user->m_userData, user->m_isDedicated);

		SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[playerIndex];
		if (pPlayer)
		{
			m_teamBalancing.AddPlayer(pPlayer);
			UpdateTeams();

			// if applicable, inform the allocated server of the addition
			if(m_allocatedServerUID != CryMatchMakingInvalidConnectionUID)
			{
				CCryLobbyPacket packet;
				GameUserPacketDefinitions packetType = eGUPD_TeamBalancingAddPlayer;
				m_teamBalancing.WritePacket(&packet, packetType, user->m_conID);
				SendPacket(&packet, packetType, m_allocatedServerUID);
			}
		}
	}
	else
	{
		CryLog("    already had a dummy entry for this player");
		CRY_ASSERT(!m_server);		// Shouldn't be possible to get here on the server
		m_nameList.Update(user->m_userID, user->m_conID, user->m_userName, user->m_userData, user->m_isDedicated, foundByUserId);
	}

	// Sort out voice
	if (user->m_userID.IsValid())
	{
		SSessionNames::SSessionName *pPlayer2 = m_nameList.GetSessionNameByUserId(user->m_userID);
		if (pPlayer2)
		{
			CryUserID localUser = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetUserID(g_pGame->GetExclusiveControllerDeviceIndex());
			const bool bIsLocalUser = localUser.IsValid() && (localUser == user->m_userID);
			if (bIsLocalUser==false)
			{
				SetAutomaticMutingStateForPlayer(pPlayer2, m_autoVOIPMutingType);
			}

			if (m_state == eLS_Game)
			{
				if (m_isTeamGame || !m_hasValidGameRules)
				{
					// If we're a team game, mute the player till we know what team they're on
					MutePlayerBySessionName(pPlayer2, true, SSessionNames::SSessionName::MUTE_REASON_WRONG_TEAM);
				}
			}
		}
	}

	// Sort out squads
	CSquadManager *pSquadMgr = g_pGame->GetSquadManager();
	if(pSquadMgr)
	{
		if(IsPrivateGame() && IsServer())
		{
			if(pSquadMgr->InSquad() && pSquadMgr->InCharge() && pSquadMgr->HaveSquadMates())
			{
				const SSessionNames *pSessionNames = pSquadMgr->GetSessionNames();
				if(pSessionNames)
				{
					const SSessionNames::SSessionName *pSessionName = pSessionNames->GetSessionNameByUserId(user->m_userID);
					if(pSessionName)
					{
						SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[playerIndex];
						CryLog("  setting user %s game lobby user data from squad data as intrim measure", user->m_userName);
						pPlayer->SetUserData(pSessionName->m_userData);
					}
				}
			}
		}

		CryUserID squadLeader = pSquadMgr->GetSquadLeader();
		if ((m_state == eLS_Lobby) && squadLeader.IsValid() && (squadLeader == user->m_userID))
		{
			// User joining is our squad leader, mark ourselves as in a squad
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetLocalUserData, true);
		}
	}

	//if anyone joins - don't want to move session
	if(m_gameLobbyMgr->IsPrimarySession(this))
	{
		if (m_server)
		{
			// Reset the "search for better host" timer to give the network a chance to get NAT details
			m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostDelayTime;

#if !defined(_RELEASE)
			if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
			{
				// Make sure the active connections is set to 1
				gEnv->pConsole->GetCVar("net_hostHintingActiveConnectionsOverride")->Set(1);
				CryLog("  setting net_hostHintingActiveConnectionsOverride to 1");
			}
#endif

		}
	}

	CheckForSkillChange();

	// If we're the server then we only need 1 player, clients should have 2
	if (m_server || (m_nameList.Size() > 1))
	{
		m_bHasUserList = true;
	}

	if (IsGameStarting() && (m_numPlayerJoinResets < s_pGameLobbyCVars->gl_startTimerMaxPlayerJoinResets))
	{
		m_startTimer = max(m_startTimer, s_pGameLobbyCVars->gl_startTimerMinTimeAfterPlayerJoined);
		m_numPlayerJoinResets ++;
	}

#if defined(DEDICATED_SERVER)
	SPlayerPingInfo info;
	info.m_bPingExceedsLimit = false;
	info.m_channelId = (int) user->m_conID.m_uid;
	m_playerPingInfo.push_back(info);
#endif

#if defined(TRACK_MATCHMAKING)
	if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
	{
		if( pMMTel->GetCurrentTranscriptType() == CMatchmakingTelemetry::eMMTelTranscript_QuickMatch )
		{
			uint32 currentUserIndex = g_pGame->GetExclusiveControllerDeviceIndex();

			CryUserID localUserId = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetUserID( currentUserIndex );
			bool isLocal = (user->m_userID == localUserId);
			pMMTel->AddEvent( SMMPlayerJoinedMMEvent( user, m_currentSessionId, m_nameList.Size(), isLocal ) );
		}
	}
#endif
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateUser( SCryUserInfoResult* user )
{
	CryLog("CGameLobby::UpdateUser() player '%s' uid=%u has updated their user data Lobby: %p", user->m_userName, user->m_conID.m_uid, this);

	if (user->m_isDedicated)
	{
		CryLog("  user is dedicated, ignoring");
		return;
	}

	SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionName(user->m_conID, true);
	if(pPlayer)
	{
		const uint16 previousSkill = pPlayer->GetSkillRank();

		m_nameList.Update(user->m_userID, user->m_conID, user->m_userName, user->m_userData, user->m_isDedicated, false);

		m_teamBalancing.UpdatePlayer(pPlayer, previousSkill);
		UpdateTeams();

		// if applicable, inform the allocated server of the update
		if(m_allocatedServerUID != CryMatchMakingInvalidConnectionUID)
		{
			CCryLobbyPacket packet;
			GameUserPacketDefinitions packetType = eGUPD_TeamBalancingUpdatePlayer;
			m_teamBalancing.WritePacket(&packet, packetType, user->m_conID);
			SendPacket(&packet, packetType, m_allocatedServerUID);
		}

		CheckForVotingChanges(true);

		CheckForSkillChange();
	}
}

//-------------------------------------------------------------------------
void CGameLobby::RemoveUser(SCryUserInfoResult* user)
{
	CryLog("CGameLobby::RemoveUser() user=%s, uid=%u, sid=%llX allowRemove %d lobby:%p", user->m_userName, user->m_conID.m_uid, user->m_conID.m_sid, m_allowRemoveUsers, this);

	if(m_allowRemoveUsers)
	{
		CAudioSignalPlayer::JustPlay("LobbyPlayerLeave");

		if (m_state == eLS_Game)
		{
			IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(user->m_conID.m_uid);
			if (pActor)
			{
				SHUDEvent hudEvent_lobbyRemoteLeaveGame(eHUDEvent_OnLobbyRemove_RemotePlayer);
				hudEvent_lobbyRemoteLeaveGame.AddData(static_cast<int>(pActor->GetEntityId()));
				CHUDEventDispatcher::CallEvent(hudEvent_lobbyRemoteLeaveGame);
			}
		}

		// Remove from teams before names list but don't update our teams until after we're removed from the session names
		m_teamBalancing.RemovePlayer(user->m_conID);
		m_nameList.Remove(user->m_conID);
		UpdateTeams();

		if(m_allocatedServerUID != CryMatchMakingInvalidConnectionUID)
		{
			CCryLobbyPacket packet;
			GameUserPacketDefinitions packetType = eGUPD_TeamBalancingRemovePlayer;
			m_teamBalancing.WritePacket(&packet, packetType, user->m_conID);
			SendPacket(&packet, packetType, m_allocatedServerUID);
		}

		CheckForVotingChanges(true);

		// If we've currently got a pending call to SessionEnsureBestHost, delay it to let the host hints settle
		if (m_timeTillCallToEnsureBestHost > 0.f)
		{
			m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_hostMigrationEnsureBestHostDelayTime;
		}

		CheckForSkillChange();
	}
	else
	{
		SSessionNames::SSessionName *pSessionName = m_nameList.GetSessionNameByUserId(user->m_userID);
		if (pSessionName)
		{
			pSessionName->m_conId = CryMatchMakingInvalidConnectionUID;
		}
	}

	if (m_isLeaving)
	{
		CheckCanLeave();
	}
	else if (m_state == eLS_Game)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			int channelId = (int) user->m_conID.m_uid;
			pGameRules->OnUserLeftLobby(channelId);
		}
	}

#if defined(DEDICATED_SERVER)
	int channelId = (int) user->m_conID.m_uid;
	int numPlayerPings = m_playerPingInfo.size();
	for (int i = 0; i < numPlayerPings; ++ i)
	{
		if (m_playerPingInfo[i].m_channelId == channelId)
		{
			m_playerPingInfo.removeAt(i);
			break;
		}
	}
#endif

#if defined(TRACK_MATCHMAKING)
	if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
	{
		if( pMMTel->GetCurrentTranscriptType() == CMatchmakingTelemetry::eMMTelTranscript_QuickMatch )
		{
			uint32 currentUserIndex = g_pGame->GetExclusiveControllerDeviceIndex();

			CryUserID localUserId = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetUserID( currentUserIndex );
			bool isLocal = (user->m_userID == localUserId);
			pMMTel->AddEvent( SMMPlayerLeftMMEvent( user, m_currentSessionId, m_nameList.Size(), isLocal ) );
		}
	}
#endif
}

//-------------------------------------------------------------------------
void CGameLobby::CheckCanLeave()
{
	CryLog("CGameLobby::CheckCanLeave() lobby:%p", this);

	int numPlayersThatNeedToLeaveFirst = 0;
	// Check if we're able to leave yet
	const int numRemainingPlayers = m_nameList.Size();
	// Start from 1 since we don't include ourselves
	for (int i = 1; i < numRemainingPlayers; ++ i)
	{
		SSessionNames::SSessionName &player = m_nameList.m_sessionNames[i];
		if (player.m_bMustLeaveBeforeServer && (player.m_conId != CryMatchMakingInvalidConnectionUID))
		{
			CryLog("    waiting for player %s to leave first", player.m_name);
			++ numPlayersThatNeedToLeaveFirst;
		}
	}
	if (!numPlayersThatNeedToLeaveFirst)
	{
		CryLog("    don't need to wait anymore, leaving next frame");
		m_leaveGameTimeout = 0.f;
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionQueryCallback(CryLobbyTaskID taskID, ECryLobbyError error, SCrySessionSearchResult* session, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pLobby = static_cast<CGameLobby *>(arg);
	CryLog("CGameLobby::MatchmakingSessionQueryCallback() error %d Lobby: %p", error, pLobby);

	if (pLobby->NetworkCallbackReceived(taskID, error))
	{
		pLobby->RecordReceiptOfSessionQueryCallback();

		if (!pLobby->m_bMatchmakingSession)
		{
			NOTIFY_UILOBBY_MP(HideLoadingDialog("JoinSession"));
		}

		if(session)
		{
			pLobby->m_sessionData = session->m_data;
			for (int i = 0; i < eLDI_Num; i++)
			{
				pLobby->m_userData[i] = session->m_data.m_data[i];
			}

			CryLog("CGameLobby::MatchmakingSessionQueryCallback() got user data Lobby: %p", pLobby);
			for (int i = 0; i < eLDI_Num; ++ i)
			{
				SCryLobbyUserData &userData = pLobby->m_userData[i];
				CryLog("  i=%i, id=%s, data=%i Lobby: %p", i, userData.m_id.c_str(), userData.m_int32, pLobby);
			}

			pLobby->m_sessionData.m_data = pLobby->m_userData;
			pLobby->m_sessionUserDataDirty = true;

			const bool bPreviousAllowLoadout = pLobby->AllowCustomiseEquipment();

			const char *pGameRules = GameLobbyData::GetGameRulesFromHash(pLobby->m_userData[eLDI_Gamemode].m_int32);
			pLobby->GameRulesChanged(pGameRules);

			const bool bNewAllowLoadout = pLobby->AllowCustomiseEquipment();

			const char *pCurrentLevelName = GameLobbyData::GetMapFromHash(pLobby->m_userData[eLDI_Map].m_int32, "");
			pLobby->SetCurrentLevelName(pCurrentLevelName);

			CSquadManager *pSquadManager = g_pGame->GetSquadManager();
			if(pSquadManager)
			{
				pSquadManager->SessionChangeSlotType(pLobby->IsPrivateGame() ? CSquadManager::eSST_Private : CSquadManager::eSST_Public);
			}

			if((!pLobby->m_bMatchmakingSession) && (bPreviousAllowLoadout != bNewAllowLoadout))
			{
				pLobby->RefreshCustomiseEquipment();
			}

			int maxPlayers = pLobby->m_sessionData.m_numPublicSlots + pLobby->m_sessionData.m_numPrivateSlots;
			pLobby->m_teamBalancing.SetLobbyPlayerCounts(maxPlayers);

#if IMPLEMENT_PC_BLADES
			CGameServerLists *pGameServerList = g_pGame->GetGameServerLists();
			if(pGameServerList)
			{
				pGameServerList->Add(CGameServerLists::eGSL_Recent, pLobby->m_sessionData.m_name, pLobby->m_sessionFavouriteKeyId, false);
			}
#endif
		}
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionUserPacketCallback(UCryLobbyEventData eventData, void *arg)
{
	ENSURE_ON_MAIN_THREAD;

	if(eventData.pUserPacketData)
	{
		CGameLobby *lobby = static_cast<CGameLobby *>(arg);

		if(lobby->m_currentSession == eventData.pUserPacketData->session && eventData.pUserPacketData->session != CrySessionInvalidHandle)
		{
			lobby->ReadPacket(&eventData.pUserPacketData);
		}
		else
		{
			CryLog("User packet dropped 'sessionIDs don't match': %i and %i", lobby->m_currentSession, eventData.pUserPacketData->session);
		}
	}
}

static int GetCryLobbyUserTypeSize(ECryLobbyUserDataType type)
{
	switch(type)
	{
		case eCLUDT_Int8:
			return CryLobbyPacketUINT8Size;
		case eCLUDT_Int16:
			return CryLobbyPacketUINT16Size;
		case eCLUDT_Int32:
		case eCLUDT_Float32:
			return CryLobbyPacketUINT32Size;
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("Unknown data type %d", type));
			break;
	}

	return 0;
}

//static-------------------------------------------------------------------------
void CGameLobby::WriteOnlineAttributeData(CGameLobby *pLobby, CCryLobbyPacket *pPacket, GameUserPacketDefinitions packetType, SCryLobbyUserData *pData, int32 numData, SCryMatchMakingConnectionUID connnectionUID)
{
	CRY_ASSERT(pData);
	CRY_ASSERT(numData >= 0);


	int numSent = 0;
	int i = 0;

	while(numSent < numData)
	{
		int bufferSz = CryLobbyPacketHeaderSize;
		for(i = numSent; i < numData; ++i)
		{
			if(bufferSz >= ONLINE_STATS_FRAGMENTED_PACKET_SIZE)
			{
				break;
			}

			bufferSz += GetCryLobbyUserTypeSize(pData[i].m_type);
		}

		// CreateWriteBuffer will free and recreate each time
		if(pPacket->CreateWriteBuffer(bufferSz))
		{
			pPacket->StartWrite(packetType, true);
			for(; numSent < i; ++numSent)
			{
				pPacket->WriteCryLobbyUserData(&pData[numSent]);
			}

			pLobby->SendPacket(pPacket, packetType, connnectionUID);
		}
	}
}

//---------------------------------------
void CGameLobby::SendPacket(GameUserPacketDefinitions packetType, SCryMatchMakingConnectionUID connectionUID /*=CryMatchMakingInvalidConnectionUID*/)
{
#if DEBUG_TEAM_BALANCING
	if (s_testing)	// Used by the test function to make sure we don't try to send packets when we're not in a session!
	{
		return;
	}
#endif

	CCryLobbyPacket packet;

	switch(packetType)
	{
	case eGUPD_LobbyStartCountdownTimer:
		{
			CryLog("[tlh]   sending packet of type 'eGUPD_LobbyStartCountdownTimer'");
			CRY_ASSERT(m_server);

			VOTING_DBG_LOG("[tlh]     whilst vote status = %d", m_localVoteStatus);

			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize +
				(CryLobbyPacketBoolSize * 4) +
				(CryLobbyPacketUINT8Size * 3);
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				uint8 secondsRemaining = (uint8)max(0.f, g_pGameCVars->gl_waitForBalancedGameTime - gEnv->pTimer->GetFrameStartTime().GetDifferenceInSeconds(m_timeStartedWaitingForBalancedGame));

				packet.StartWrite(packetType, true);
				packet.WriteUINT8((uint8)m_countdownStage);
				packet.WriteUINT8((uint8)secondsRemaining);
				packet.WriteBool(m_initialStartTimerCountdown);
				packet.WriteUINT8((uint8) m_startTimer);
				packet.WriteBool(m_votingEnabled);
				packet.WriteBool(m_votingClosed);
				packet.WriteBool(m_leftWinsVotes);

				VOTING_DBG_LOG("[tlh]     SENT this data:");
				VOTING_DBG_LOG("[tlh]       m_countdownStage = %d", m_countdownStage);
				VOTING_DBG_LOG("[tlh]       m_startTimer = %d", (uint8) m_startTimer);
				VOTING_DBG_LOG("[tlh]       data->Get()->m_votingEnabled = %d", m_votingEnabled);
				VOTING_DBG_LOG("[tlh]       data->Get()->m_votingClosed = %d", m_votingClosed);
				VOTING_DBG_LOG("[tlh]       data->Get()->m_leftWinsVotes = %d", m_leftWinsVotes);
			}

			if (m_votingEnabled)
			{
				UpdateVotingInfoFlashInfo();
			}
		}
		break;
	case eGUPD_SetupJoinCommand:	// needs to be sent before LobbyGameHasStarted
		{
			CRY_ASSERT(m_server);
			CRY_ASSERT(m_sessionHostOnly);

			CryLog("   writing eGUPD_SetupJoinCommand");

			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize +  CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size;
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
				packet.WriteUINT32(m_dedicatedserverip);
				packet.WriteUINT16(m_dedicatedserverport);
			}
		}
		break;
	case eGUPD_SetupArbitratedLobby:
		{
			// this is really only meant as a debug feature until we get the online services up and running properly again,
			CRY_ASSERT(m_server);
			CRY_ASSERT(m_sessionHostOnly);

			ICVar *pArbitratorIP = gEnv->pConsole->GetCVar("net_dedicatedServerArbitratorIP");
			ICVar *pArbitratorPort = gEnv->pConsole->GetCVar("net_dedicatedServerArbitratorPort");

			CRY_ASSERT(pArbitratorIP);
			CRY_ASSERT(pArbitratorPort);

			stack_string ip = pArbitratorIP ? pArbitratorIP->GetString() : "";
			uint16 port = pArbitratorPort ? pArbitratorPort->GetIVal() : 0;
			uint8 ipSize = static_cast<uint8> (ip.size());

			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize +  CryLobbyPacketUINT8Size + CryLobbyPacketUINT16Size + (CryLobbyPacketUINT8Size * ipSize);
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
				packet.WriteUINT8(ipSize);
				packet.WriteString(ip.c_str(), ipSize+1); // +1 to include terminator
				packet.WriteUINT16(port);
			}
		}
		break;
	case eGUPD_LobbyGameHasStarted:
		{
			CRY_ASSERT(m_server);
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + (3 * CryLobbyPacketUINT32Size) + (CryLobbyPacketBoolSize * 2);
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
				CDLCManager* pDLCManager = g_pGame->GetDLCManager();
				packet.WriteUINT32(pDLCManager->GetRequiredDLCs());

				bool gameStillInProgress = true;
				CGameRules *pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
				{
					IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
					if (pStateModule)
					{
						IGameRulesStateModule::EGR_GameState gameState = pStateModule->GetGameState();
						if (gameState == IGameRulesStateModule::EGRS_PostGame)
						{
							gameStillInProgress = false;
						}
						else if (gameState == IGameRulesStateModule::EGRS_InGame)
						{
							if (pGameRules->IsTimeLimited() && (pGameRules->GetRemainingGameTime() < s_pGameLobbyCVars->gl_gameTimeRemainingRestrictLoad))
							{
								gameStillInProgress = false;

								IGameRulesRoundsModule *pRoundsModule = pGameRules->GetRoundsModule();
								if (pRoundsModule)
								{
									if (pRoundsModule->GetRoundsRemaining() != 0)
									{
										gameStillInProgress = true;
									}
								}
							}
						}
					}
				}

				packet.WriteBool(gameStillInProgress);
				packet.WriteBool(m_votingEnabled);

				const char *pCurrentGameRules = m_currentGameRules.c_str();
				const char *pCurrentLevelName = m_currentLevelName.c_str();

				uint32 gameRulesHash = GameLobbyData::ConvertGameRulesToHash(pCurrentGameRules);
				uint32 mapHash = GameLobbyData::ConvertMapToHash(pCurrentLevelName);

				packet.WriteUINT32(gameRulesHash);
				packet.WriteUINT32(mapHash);
			}
		}
		break;
	case eGUPD_LobbyEndGame:
		{
			CRY_ASSERT(m_server);
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize;
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
			}
		}
		break;
	case eGUPD_LobbyEndGameResponse:
		{
			CRY_ASSERT(!m_server);
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize;
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
			}
		}
		break;
	case eGUPD_LobbyUpdatedSessionInfo:
		{
			CRY_ASSERT(m_server);
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize;
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
			}
		}
		break;
	case eGUPD_LobbyMoveSession:
		{
			CRY_ASSERT(m_server);
			CRY_ASSERT(m_nextSessionId != CrySessionInvalidID);
			ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + pMatchmaking->GetSessionIDSizeInPacket();
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
				ECryLobbyError error = pMatchmaking->WriteSessionIDToPacket(m_nextSessionId, &packet);
				CRY_ASSERT(error == eCLE_Success);
			}
		}
		break;
	case eGUPD_SetTeamAndRank:
		{
			CRY_ASSERT(m_server);

			const uint32 numPlayers = uint32(m_nameList.Size());

			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + (numPlayers * (CryLobbyPacketUINT8Size * 2 + CryLobbyPacketConnectionUIDSize));
			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				packet.StartWrite(packetType, true);
				packet.WriteUINT8(uint8(numPlayers));
				for (uint32 i = 0; i < numPlayers; ++ i)
				{
					SSessionNames::SSessionName &player = m_nameList.m_sessionNames[i];
					packet.WriteConnectionUID(player.m_conId);
					packet.WriteUINT8(player.m_teamId);
					packet.WriteUINT8(player.m_rank);
				}
			}
			break;
		}
	case eGUPD_SendChatMessage:
		{
			CRY_ASSERT(!m_server);

#ifndef _RELEASE
			CryLog("CLIENT Send: eGUPD_SendChatMessage channel:%d team:%d, message:%s", m_chatMessageStore.conId.m_uid, m_chatMessageStore.teamId, m_chatMessageStore.message.c_str());
#endif

			uint8 messageLength = static_cast<uint8> (m_chatMessageStore.message.size());
			if (messageLength>0 && messageLength<MAX_CHATMESSAGE_LENGTH)
			{
				const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + CryLobbyPacketUINT8Size + messageLength;
				if (packet.CreateWriteBuffer(MaxBufferSize))
				{
					packet.StartWrite(packetType, true);
					packet.WriteUINT8(m_chatMessageStore.teamId);
					packet.WriteUINT8(messageLength + 1);  // +1 is null teminator
					packet.WriteString(m_chatMessageStore.message.c_str(), messageLength + 1);  // +1 is null teminator
				}
			}
			else
			{
				CryLog("Error, Cannot send empty chat message");
			}

			m_chatMessageStore.Clear();

			break;
		}
	case eGUPD_ChatMessage:
		{
			CRY_ASSERT(m_server);

#ifndef _RELEASE
			CryLog("SERVER SEND to %d: eGUPD_ChatMessage channel:%d team:%d, message:%s", connectionUID.m_uid, m_chatMessageStore.conId.m_uid, m_chatMessageStore.teamId, m_chatMessageStore.message.c_str());
#endif

			uint8 messageLength = static_cast<uint8> (m_chatMessageStore.message.size());
			if (messageLength>0 && messageLength<MAX_CHATMESSAGE_LENGTH)
			{
				const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT8Size + CryLobbyPacketUINT8Size + messageLength;
				if (packet.CreateWriteBuffer(MaxBufferSize))
				{
					packet.StartWrite(packetType, true);
					packet.WriteConnectionUID(m_chatMessageStore.conId);
					packet.WriteUINT8(m_chatMessageStore.teamId);
					packet.WriteUINT8(messageLength + 1);  // +1 is null teminator
					packet.WriteString(m_chatMessageStore.message.c_str(), messageLength + 1);  // +1 is null teminator
				}
			}
			else
			{
				CryLog("Error, Cannot send empty chat message");
			}

			break;
		}
	case eGUPD_ReservationRequest:
		{
			CryLog("  sending packet of type 'eGUPD_ReservationRequest'");

			CRY_ASSERT(!m_server);

			SCryMatchMakingConnectionUID  reservationRequests[MAX_RESERVATIONS];
			const int  numReservationsToRequest = BuildReservationsRequestList(reservationRequests, CRY_ARRAY_COUNT(reservationRequests), m_reservationList);

			const SSessionNames*  members = m_reservationList;

			const int  bufferSz = (CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + (numReservationsToRequest * CryLobbyPacketConnectionUIDSize));

			if (packet.CreateWriteBuffer(bufferSz))
			{

				packet.StartWrite(packetType, true);

				CryLog("    numReservationsToRequest = %d", numReservationsToRequest);
				packet.WriteUINT8(numReservationsToRequest);

				CryLog("    reservations needed:");
				for (int i=0; i<numReservationsToRequest; ++i)
				{
					CryLog("      %02d: {%" PRIu64 ",%d}", (i + 1), reservationRequests[i].m_sid, reservationRequests[i].m_uid);
					packet.WriteConnectionUID(reservationRequests[i]);
				}
			}

			break;
		}
	case eGUPD_ReservationClientIdentify:
		{
			CryLog("  sending packet of type 'eGUPD_ReservationClientIdentify', slot reserved as uid=%u, sid=%" PRIu64, m_pendingReservationId.m_uid, m_pendingReservationId.m_sid);
			CRY_ASSERT(!m_server);

			ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
			CRY_ASSERT(pMatchmaking);

			const int  bufferSz = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketBoolSize + pMatchmaking->GetSessionIDSizeInPacket();

			if (packet.CreateWriteBuffer(bufferSz))
			{
				packet.StartWrite(packetType, true);

				packet.WriteConnectionUID(m_pendingReservationId);
				m_pendingReservationId = CryMatchMakingInvalidConnectionUID;
#if 0 // LEVEL ROTATION DISABLED FOR NOW
				CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
				const bool bHavePlaylist = pPlaylistManager ? pPlaylistManager->HavePlaylistSet() : false;
				packet.WriteBool(bHavePlaylist);
#else
				const bool bHavePlaylist = false;
				packet.WriteBool(bHavePlaylist);
#endif
				CrySessionID sessionIdToSend = CrySessionInvalidID;

				if (m_gameLobbyMgr->IsNewSession(this))
				{
					CGameLobby *pPrimaryGameLobby = m_gameLobbyMgr->GetGameLobby();
					sessionIdToSend = pPrimaryGameLobby->m_currentSessionId;
					if (!sessionIdToSend)
					{
						sessionIdToSend = pPrimaryGameLobby->m_pendingConnectSessionId;
					}
				}

				pMatchmaking->WriteSessionIDToPacket(sessionIdToSend, &packet);
			}

			break;
		}
	case eGUPD_ReservationsMade:
		{
			CryLog("  sending packet of type 'eGUPD_ReservationsMade'");
			CRY_ASSERT(m_server);

			const int  bufferSz = (CryLobbyPacketHeaderSize);

			if (packet.CreateWriteBuffer(bufferSz))
			{
				packet.StartWrite(packetType, true);
			}

			break;
		}
	case eGUPD_ReservationFailedSessionFull:
		{
			CryLog("  sending packet of type 'eGUPD_ReservationFailedSessionFull'");
			CRY_ASSERT(m_server);

			const int  bufferSz = (CryLobbyPacketHeaderSize);

			if (packet.CreateWriteBuffer(bufferSz))
			{
				packet.StartWrite(packetType, true);
			}

			break;
		}
	case eGUPD_SetGameVariant:
		{
			CRY_ASSERT(m_server);
#if 0 // LEVEL ROTATION DISABLED FOR NOW
			CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
			if (pPlaylistManager)
			{
				pPlaylistManager->WriteSetVariantPacket(&packet);
			}
#endif
			break;
		}
	case eGUPD_SyncPlaylistRotation:
		{
			CryLog("[tlh]   sending packet of type 'eGUPD_SyncPlaylistRotation'");
			CRY_ASSERT(m_server);

			int  curNextIdx = 0;
#if 0 // LEVEL ROTATION DISABLED FOR NOW
			if (CPlaylistManager* pPlaylistManager=g_pGame->GetPlaylistManager())
			{
				CRY_ASSERT(pPlaylistManager->HavePlaylistSet());

				if (ILevelRotation* pRot=pPlaylistManager->GetLevelRotation())
				{
					curNextIdx = pRot->GetNext();
				}
			}
#endif
			const bool  gameHasStarted = (m_state == eLS_Game);

			const uint32  bufferSize = (CryLobbyPacketHeaderSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size + (2 * CryLobbyPacketBoolSize));

			if (packet.CreateWriteBuffer(bufferSize))
			{
				packet.StartWrite(eGUPD_SyncPlaylistRotation, true);
				CryLog("[tlh]     writing next=%d, started=%d, advancedThruConsole=%d seed=%d", curNextIdx, gameHasStarted, m_bPlaylistHasBeenAdvancedThroughConsole, m_playListSeed);
				packet.WriteUINT32(m_playListSeed);
				packet.WriteUINT8((uint8) curNextIdx);
				packet.WriteBool(gameHasStarted);
				packet.WriteBool(m_bPlaylistHasBeenAdvancedThroughConsole);
			}

			break;
		}
	case eGUPD_UpdateCountdownTimer:
		{
			const int bufferSz = (CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size);
			if (packet.CreateWriteBuffer(bufferSz))
			{
				packet.StartWrite(packetType, true);
				uint8 startTimer = (uint8) std::max(m_startTimer, 0.f);
				packet.WriteUINT8(startTimer);
			}
			break;
		}
	case eGUPD_RequestAdvancePlaylist:
		{
			const int bufferSz = (CryLobbyPacketHeaderSize);
			if (packet.CreateWriteBuffer(bufferSz))
			{
				packet.StartWrite(packetType, true);
			}
			break;
		}
	case eGUPD_SyncPlaylistContents:
		{
#if 0 // LEVEL ROTATION DISABLED FOR NOW
			const SPlaylist *pPlaylist = g_pGame->GetPlaylistManager()->GetCurrentPlaylist();

			const ILevelRotation::TExtInfoId playlistId = pPlaylist->id;

			ILevelRotation *pRotation = g_pGame->GetIGameFramework()->GetILevelSystem()->FindLevelRotationForExtInfoId(playlistId);
			CRY_ASSERT(pRotation);
			if (pRotation)
			{
				int numLevels = pRotation->GetLength();

				const uint32 bufferSize = (CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + (CryLobbyPacketUINT32Size * (2 * numLevels)));
				if (packet.CreateWriteBuffer(bufferSize))
				{
					packet.StartWrite(packetType, true);
					packet.WriteUINT8((uint8) numLevels);

					int originalNext = pRotation->GetNext();
					pRotation->First();
					for (int i = 0; i < numLevels; ++ i)
					{
						const char *pGameRules = pRotation->GetNextGameRules();
						const char *pLevel = pRotation->GetNextLevel();

						const uint32 gameRulesHash = GameLobbyData::ConvertGameRulesToHash(pGameRules);
						const uint32 levelHash = GameLobbyData::ConvertMapToHash(pLevel);

						packet.WriteUINT32(gameRulesHash);
						packet.WriteUINT32(levelHash);

						pRotation->Advance();
					}

					// Now we need to put the rotation back where it was
					pRotation->First();
					while (pRotation->GetNext() != originalNext)
					{
						pRotation->Advance();
					}
				}
			}
#endif
			break;
		}
	case eGUPD_SyncExtendedServerDetails:
		{
			CryLog("  writing eGUPD_SyncExtendedServerDetails");

			uint32 bufferSize = (CryLobbyPacketHeaderSize);
			bufferSize += (CryLobbyPacketBoolSize * 2);

			if (packet.CreateWriteBuffer(bufferSize))
			{
				packet.StartWrite(packetType, true);

				const bool  isPrivate = IsPrivateGame();
				const bool  isPassworded = IsPasswordedGame();
				CryLog("    writing bool isPrivate=%s", (isPrivate?"true":"false"));
				packet.WriteBool(isPrivate);
				CryLog("    writing bool isPassworded=%s", (isPassworded?"true":"false"));
				packet.WriteBool(isPassworded);
			}
			break;
		}
	case eGUPD_SetAutoStartingGame:
		{
			CRY_ASSERT(IsServer());

			const bool isAutoStartingGame = IsAutoStartingGame();
			CryLog("  writing eGUPD_SetAutoStartingGame %d", isAutoStartingGame);

			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketBoolSize;
			if (packet.CreateWriteBuffer(bufferSize))
			{
				packet.StartWrite(packetType, true);
				packet.WriteBool(isAutoStartingGame);
			}
			break;
		}
	case eGUPD_TeamBalancingSetup:
		{
			CryLog("  writing eGUPD_TeamBalancingSetup");
			m_teamBalancing.WritePacket(&packet, packetType, CryMatchMakingInvalidConnectionUID);
			break;
		}
	}

	SendPacket(&packet, packetType, connectionUID);
}

void CGameLobby::SendPacket(CCryLobbyPacket *pPacket, GameUserPacketDefinitions packetType, SCryMatchMakingConnectionUID connectionUID)
{
	CRY_ASSERT_TRACE(pPacket->GetWriteBuffer() != NULL, ("Haven't written any data, packetType '%d'", packetType));
	CRY_ASSERT_TRACE(pPacket->GetWriteBufferPos() == pPacket->GetReadBufferSize(), ("Packet size doesn't match data size, packetType '%d'", packetType));
	CRY_ASSERT_TRACE(pPacket->GetReliable(), ("Unreliable packet sent, packetType '%d'", packetType));

	ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
	const int packetSize = pPacket->GetWriteBufferPos();
	if (packetSize > 0)
	{
		ECryLobbyError result = eCLE_Success;
		if(m_server)
		{
			if(connectionUID != CryMatchMakingInvalidConnectionUID)
			{
				CryLog("Send packet of type to clients(%d) '%d'", connectionUID.m_uid, packetType);
				result = pMatchmaking->SendToClient(pPacket, m_currentSession, connectionUID);
			}
			else
			{
				CryLog("Send packet of type to all clients '%d'", packetType);
				result = GameNetworkUtils::SendToAll(pPacket, m_currentSession, m_nameList, true);
			}
		}
		else
		{
			CryLog("Send packet of type to server '%d'", packetType);
			result = pMatchmaking->SendToServer(pPacket, m_currentSession);
		}
		if (result != eCLE_Success)
		{
			CryLog("GameLobby::SendPacket ERROR sending the packet %d type %d",(int)result,(int)packetType);
		}
	}
	else
	{
		CryLog("GameLobby::SendPacket ERROR trying to send an invalid sized packet %d typed %d",packetSize,(int)packetType);
	}
}

//---------------------------------------
void CGameLobby::ReadPacket(SCryLobbyUserPacketData** ppPacketData)
{
	if (m_bCancelling || (GetState() == eLS_Leaving))
	{
		CryLog("CGameLobby::ReadPacket() Received a packet while cancelling, ignoring");
		return;
	}

	SCryLobbyUserPacketData* pPacketData = (*ppPacketData);
	CCryLobbyPacket* pPacket = pPacketData->pPacket;
	CRY_ASSERT_MESSAGE(pPacket->GetReadBuffer() != NULL, "No packet data");

	uint32 packetType = pPacket->StartRead();
	CryLog("Read packet of type '%d' lobby=%p", packetType, this);

	switch(packetType)
	{
	case eGUPD_LobbyStartCountdownTimer:
		{
			CryLog("[tlh]   reading packet of type 'eGUPD_LobbyStartCountdownTimer'");
			CRY_ASSERT(!m_server);

			VOTING_DBG_LOG("[tlh]     whilst vote status = %d", m_localVoteStatus);

			m_hasReceivedStartCountdownPacket = true;

			ECountdownStage previousStage = m_countdownStage;
			uint8 timeToWaitForOpponents = 0;

			m_countdownStage = (ECountdownStage) pPacket->ReadUINT8();
			timeToWaitForOpponents = pPacket->ReadUINT8();
			m_initialStartTimerCountdown = pPacket->ReadBool();
			m_startTimer = (float) pPacket->ReadUINT8();
			m_votingEnabled = pPacket->ReadBool();
			m_votingClosed = pPacket->ReadBool();
			m_leftWinsVotes = pPacket->ReadBool();

			m_timeStartedWaitingForBalancedGame = gEnv->pTimer->GetFrameStartTime() - CTimeValue(g_pGameCVars->gl_waitForBalancedGameTime - timeToWaitForOpponents);
			m_startTimerLength = m_initialStartTimerCountdown ? s_pGameLobbyCVars->gl_initialTime : s_pGameLobbyCVars->gl_time;

			VOTING_DBG_LOG("[tlh]     READ this data:");
			VOTING_DBG_LOG("[tlh]       m_countdownStage = %d", m_countdownStage);
			VOTING_DBG_LOG("[tlh]       m_startTimer = %d", (uint8) m_startTimer);
			VOTING_DBG_LOG("[tlh]       m_votingEnabled = %d", m_votingEnabled);
			VOTING_DBG_LOG("[tlh]       m_votingClosed = %d", m_votingClosed);
			VOTING_DBG_LOG("[tlh]       m_leftWinsVotes = %d", m_leftWinsVotes);

			if (m_votingEnabled)
			{
				CRY_ASSERT(s_pGameLobbyCVars->gl_enablePlaylistVoting);

				if (m_localVoteStatus == eLVS_awaitingCandidates)
				{
					SetLocalVoteStatus(eLVS_notVoted);
					VOTING_DBG_LOG("[tlh] set m_localVoteStatus [4] to eLVS_notVoted");
				}

				UpdateVoteChoices();

#if 0 // old frontend
				UpdateVotingCandidatesFlashInfo();
#endif
				CheckForVotingChanges(false);

				UpdateVotingInfoFlashInfo();

				if (m_votingClosed)
				{
					UpdateRulesAndMapFromVoting();
				}

				m_bHasReceivedVoteOptions = true;
			}

			m_sessionUserDataDirty = true;

			if (m_bNeedToSetAsElegibleForHostMigration)
			{
				m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionSetLocalFlags, false);
				m_bNeedToSetAsElegibleForHostMigration = false;
			}

			if ((m_countdownStage != previousStage) && ((m_countdownStage == eCDS_WaitingForBalancedGame) || (previousStage == eCDS_WaitingForBalancedGame)))
			{
				// Switching in or out of the waiting for balanced game state will toggle showing the teams, need to refresh flash
				m_nameList.m_dirty = true;
			}
		}
		break;
	case eGUPD_SetupJoinCommand:	// needs to be processed before LobbyGameHasStarted
		{
			CRY_ASSERT(!m_server);

			m_dedicatedserverip = pPacket->ReadUINT32();
			m_dedicatedserverport = pPacket->ReadUINT16();

			CryLog("   reading eGUPD_SetupJoinCommand");

			cry_sprintf(m_joinCommand, sizeof(m_joinCommand), "connect <session>%08X,%d.%d.%d.%d:%d", m_currentSession, ((uint8*)&m_dedicatedserverip)[0], ((uint8*)&m_dedicatedserverip)[1], ((uint8*)&m_dedicatedserverip)[2], ((uint8*)&m_dedicatedserverip)[3], m_dedicatedserverport);
		}
		break;
	case eGUPD_SetupArbitratedLobby:
		{
			CryLog("   reading eGUPD_SetupArbitratedLobby");

			uint8 strSize = pPacket->ReadUINT8();
			const int bufferSize = 255;

			if(strSize < bufferSize)
			{
				char strBuffer[bufferSize] = {0};
				uint16 port = 0;

				pPacket->ReadString(strBuffer, strSize+1); // +1 to include terminator
				port = pPacket->ReadUINT16();

				s_pGameLobbyCVars->gl_getServerFromDedicatedServerArbitrator = 1;

				if(ICVar *pArbitratorIP = gEnv->pConsole->GetCVar("net_dedicatedServerArbitratorIP"))
				{
					pArbitratorIP->Set(strBuffer);
				}

				if(ICVar *pArbitratorPort = gEnv->pConsole->GetCVar("net_dedicatedServerArbitratorPort"))
				{
					pArbitratorPort->Set(port);
				}

				CryLog("      read arbitrator ip/port %s:%d ", strBuffer, port);
			}
		}
		break;
	case eGUPD_LobbyGameHasStarted:
		{
			CRY_ASSERT(!m_server);
			uint32 requiredDLCs = pPacket->ReadUINT32();
			bool gameStillInProgress = pPacket->ReadBool();
			m_votingEnabled = pPacket->ReadBool();

			uint32 gameRulesHash = pPacket->ReadUINT32();
			uint32 mapHash = pPacket->ReadUINT32();

			m_loadingGameRules = m_currentGameRules = GameLobbyData::GetGameRulesFromHash(gameRulesHash, "");
			m_loadingLevelName = m_currentLevelName = GameLobbyData::GetMapFromHash(mapHash, "");
			
			if(!IsQuitting())
			{
				if ((!m_bSessionStarted) && (GetState() != eLS_JoinSession))
				{
					CDLCManager* pDLCManager = g_pGame->GetDLCManager();
					bool localPlayerMeetsDLCRequirements = CDLCManager::MeetsDLCRequirements(requiredDLCs, pDLCManager->GetLoadedDLCs());

#if !defined( _RELEASE )
					if( !localPlayerMeetsDLCRequirements && !s_pGameLobbyCVars->gl_allowDevLevels )
#else
					if (!localPlayerMeetsDLCRequirements)
#endif //!defined( _RELEASE )
					{
						// Unfortunately the player doesn't meet the DLC requirements so he can't join the game
#if 0 // old frontend
						if (CMPMenuHub* pMPMenu=CMPMenuHub::GetMPMenuHub())
						{
							pMPMenu->GoToCurrentLobbyServiceScreen();
						}
#endif
						LeaveSession(true, false);
						g_pGame->AddGameWarning("DlcNotAvailable", NULL);
					}
					else
					{
						if (gameStillInProgress)
						{
							if (m_votingEnabled && !m_gameHadStartedWhenPlaylistRotationSynced)
							{
								AdvanceLevelRotationAccordingToVotingResults();
							}

							SetState(eLS_JoinSession);
						}
						else
						{
							m_bWaitingForGameToFinish = true;
						}
					}
				}
				else
				{
					CryLog("  received duplicate eGUPD_LobbyGameHasStarted packet, probably due to a host migration");
				}
			}
			else
			{
				CryLog("  in the process quitting, ignoring eGUPD_LobbyGameHasStarted");
			}
		}
		break;

	case eGUPD_LobbyEndGame:
		{
			CRY_ASSERT(!m_server);

			if(!IsQuitting())
			{
				if (m_bWaitingForGameToFinish)
				{
					m_bWaitingForGameToFinish = false;
				}
				else
				{
					m_bHasReceivedVoteOptions = false;
					SetState(eLS_EndSession);
				}
			}
			else
			{
				CryLog("  ignoring lobby end game as in the process of leaving session");
			}
		}
		break;
	case eGUPD_LobbyEndGameResponse:
		{
			CRY_ASSERT(m_server);
			++m_endGameResponses;

			CRY_ASSERT(m_state != eLS_Game);
			CryLog("[tlh] calling SendPacket(eGUPD_SyncPlaylistRotation) [3]");
			SendPacket(eGUPD_SyncPlaylistRotation, pPacketData->connection);
		}
		break;
	case eGUPD_LobbyUpdatedSessionInfo:
		{
			CRY_ASSERT(!m_server);
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Query, true);
		}
		break;
	case eGUPD_LobbyMoveSession:
		{
			CRY_ASSERT(!m_server);
			CRY_ASSERT(m_gameLobbyMgr->IsPrimarySession(this));

			ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
			CrySessionID session = pMatchmaking->ReadSessionIDFromPacket(pPacket);

			if(!IsQuitting())
			{
				SSessionNames &playerList = m_nameList;
				unsigned int size = playerList.Size();

				if (size > 0)
				{
					SSessionNames::SSessionName &localPlayer = playerList.m_sessionNames[0];
					SCryMatchMakingConnectionUID localConnection = localPlayer.m_conId;

					m_allowRemoveUsers = false;

					for(unsigned int i = 0; i < size; ++i)
					{
						// because we are preserving the list bewteen servers, need to invalidate the
						// connection ids so no the client does not get confused with potential
						// duplication ids
						playerList.m_sessionNames[i].m_conId = CryMatchMakingInvalidConnectionUID;
					}

					// Reservation will have been made using the old session connection UID so we need to save it before leaving
					JoinServer(session, "Merging Session", localConnection, false);

#if defined(TRACK_MATCHMAKING)
					if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
					{
						pMMTel->AddEvent( SMMMergeRequestedEvent( session ) );
					}
#endif
				}
			}
			else
			{
				CryLog("  not moving to new session as in the process of quitting");
			}
		}
		break;
	case eGUPD_SetTeamAndRank:
		{
			CRY_ASSERT(!m_server);

			// Remove any previous placeholder entries
			m_nameList.RemoveBlankEntries();

			const uint32 numPlayersInPacket = uint32(pPacket->ReadUINT8());
			for (uint32 i = 0; i < numPlayersInPacket; ++ i)
			{
				SCryMatchMakingConnectionUID conId = pPacket->ReadConnectionUID();
				uint8 teamId = pPacket->ReadUINT8();
				uint8 rank = pPacket->ReadUINT8();

				int playerIndex = m_nameList.Find(conId);
				if (playerIndex == SSessionNames::k_unableToFind)
				{
					CryLog("eGUPD_SetTeamAndRank Packet telling about player who doesn't exist yet, uid=%u", conId.m_uid);
					playerIndex = m_nameList.Insert(CryUserInvalidID, conId, "", NULL, false);

					SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[playerIndex];
					m_teamBalancing.AddPlayer(pPlayer);
					UpdateTeams();
				}
				m_nameList.m_sessionNames[playerIndex].m_teamId = teamId;
				m_nameList.m_sessionNames[playerIndex].m_rank = rank;
			}
			m_nameList.m_dirty = true;
			break;
		}
	case eGUPD_SendChatMessage:
		{
			CRY_ASSERT(m_server);

			m_chatMessageStore.Clear();
			m_chatMessageStore.teamId = pPacket->ReadUINT8();

			uint8 messageSize = pPacket->ReadUINT8(); // includes +1 for null terminator
			CRY_ASSERT(messageSize <= MAX_CHATMESSAGE_LENGTH);

			if (messageSize <= MAX_CHATMESSAGE_LENGTH)
			{
				char buffer[MAX_CHATMESSAGE_LENGTH];
				pPacket->ReadString(buffer, messageSize);

				m_chatMessageStore.message = buffer;
				m_chatMessageStore.conId = pPacketData->connection;

				SendChatMessageToClients();
			}
			break;
		}
	case eGUPD_ChatMessage:
		{
			CRY_ASSERT(!m_server);

			SChatMessage chatMessage;
			chatMessage.conId = pPacket->ReadConnectionUID();
			chatMessage.teamId = pPacket->ReadUINT8();

			uint8 messageSize = pPacket->ReadUINT8(); // includes +1 for null terminator
			CRY_ASSERT(messageSize <= MAX_CHATMESSAGE_LENGTH);

			if (messageSize <= MAX_CHATMESSAGE_LENGTH)
			{
				char buffer[MAX_CHATMESSAGE_LENGTH];
				pPacket->ReadString(buffer, messageSize);

				chatMessage.message = buffer;

				RecievedChatMessage(&chatMessage);
			}

			break;
		}
	case eGUPD_ReservationRequest:
		{
			CryLog("  reading packet of type 'eGUPD_ReservationRequest'");
			CRY_ASSERT(m_server);

			int  numReservationsRequested = pPacket->ReadUINT8();
			CRY_ASSERT(numReservationsRequested <= MAX_RESERVATIONS);
			numReservationsRequested = std::min(numReservationsRequested, (int)MAX_RESERVATIONS);
			CryLog("    numReservationsRequested = %d", numReservationsRequested);

			SCryMatchMakingConnectionUID  requestedReservations[MAX_RESERVATIONS];
			for (int i=0; i<numReservationsRequested; ++i)
			{
				requestedReservations[i] = pPacket->ReadConnectionUID();
			}

			const EReservationResult  resres = DoReservations(numReservationsRequested, requestedReservations);
			switch (resres)
			{
			case eRR_Success:
				{
					CryLog("    sending eGUPD_ReservationsMade packet to leader {%" PRIu64 ",%d}", pPacketData->connection.m_sid, pPacketData->connection.m_uid);
					SendPacket(eGUPD_ReservationsMade, pPacketData->connection);
					break;
				}
			case eRR_Fail:
				{
					CryLog("    sending eGUPD_ReservationFailedSessionFull packet to leader {%" PRIu64 ",%d} because not enough space for him and his squad", pPacketData->connection.m_sid, pPacketData->connection.m_uid);
					SendPacket(eGUPD_ReservationFailedSessionFull, pPacketData->connection);
					break;
				}
			case eRR_NoneNeeded:
				{
					CryLog("    NO RESERVATIONS REQUIRED, sending SUCCESS but this SHOULD NOT HAPPEN");
					SendPacket(eGUPD_ReservationsMade, pPacketData->connection);
					break;
				}
			default:
				{
					CRY_ASSERT(0);
					break;
				}
			}

			break;
		}
	case eGUPD_ReservationClientIdentify:
		{
			CryLog("  reading packet of type 'eGUPD_ReservationClientIdentify'");
			CRY_ASSERT(m_server);

			const SCryMatchMakingConnectionUID requestedUID = pPacket->ReadConnectionUID();
			CryLog("    client identifying itself as uid=%u, sid=%" PRIu64, requestedUID.m_uid, requestedUID.m_sid);

			const bool bHasPlaylist = pPacket->ReadBool();

			ICryMatchMaking *pMatchmaking = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetMatchMaking();
			CRY_ASSERT(pMatchmaking);
			CrySessionID joinersSessionId = pMatchmaking->ReadSessionIDFromPacket(pPacket);

			// Are two sessions trying to merge into each other? If so, we need to cancel the merge on one side.
			if (BidirectionalMergingRequiresCancel(joinersSessionId))
			{
				FindGameCancelMove();
			}

			const float  timeNow = gEnv->pTimer->GetAsyncCurTime();
			int  reservedCount = 0;
			bool  hadReservation = false;

			for (int i=0; i<CRY_ARRAY_COUNT(m_slotReservations); ++i)
			{
				SSlotReservation*  res = &m_slotReservations[i];

				if (res->m_con != CryMatchMakingInvalidConnectionUID)
				{
					CryLog("    slot %i reserved for uid=%u, sid=%" PRIu64, i, res->m_con.m_uid, res->m_con.m_sid);
					if (res->m_con == requestedUID)
					{
						res->m_con = CryMatchMakingInvalidConnectionUID;
						hadReservation = true;
						CryLog("      removed reservation as it's for this client!");
					}
					else if ((timeNow - res->m_timeStamp) > s_pGameLobbyCVars->gl_slotReservationTimeout)
					{
						res->m_con = CryMatchMakingInvalidConnectionUID;
						CryLog("      removed reservation as it's expired");
					}
					else
					{
						++reservedCount;
						CryLog("      reservation valid, will timeout in %f seconds", (timeNow - res->m_timeStamp));
					}
				}
			}

			const int  numPrivate = m_sessionData.m_numPrivateSlots;
			const int  numPublic = m_sessionData.m_numPublicSlots;
			const int  numFilledExc = (m_nameList.Size() - 1);  // NOTE -1 because client in question will be in list already but we don't want them to be included in the calculations
			const int  numEmptyExc = ((numPrivate + numPublic) - numFilledExc);

			CRY_ASSERT(numFilledExc >= 0);

			CryLog("    nums private = %d, public = %d, filled (exc. client) = %d, empty (exc. client) = %d, reserved = %d", numPrivate, numPublic, numFilledExc, numEmptyExc, reservedCount);

			bool  enoughSpace = false;

			if (numEmptyExc > 0)
			{
				if (hadReservation || (numEmptyExc > reservedCount))
				{
					enoughSpace = true;
				}
				else
				{
					CryLog("      client being kicked because all empty spaces have been reserved and they didn't have a reservation");
				}
			}
			else
			{
				CryLog("      client being kicked because there are no empty spaces");
			}

			if (enoughSpace)
			{
				SSessionNames &names = m_nameList;
				int index = names.Find(pPacketData->connection);
				if (index != SSessionNames::k_unableToFind)
				{
					names.m_sessionNames[index].m_bFullyConnected = true;
				}

				SendPacket(eGUPD_SyncExtendedServerDetails, pPacketData->connection);

#if 0 // LEVEL ROTATION DISABLED FOR NOW
				// Check if we need to sync the playlist
				if (g_pGame->GetPlaylistManager()->HavePlaylistSet() && (!bHasPlaylist))
				{
					SendPacket(eGUPD_SyncPlaylistContents, pPacketData->connection);
				}

				// Send variant info before potentially starting the game
				SendPacket(eGUPD_SetGameVariant, pPacketData->connection);
#endif

				// For status messages
				SendPacket(eGUPD_SetAutoStartingGame, pPacketData->connection);

				if (CPlaylistManager* plMgr=g_pGame->GetPlaylistManager())
				{
					if (plMgr->HavePlaylistSet())
					{
						m_bPlaylistHasBeenAdvancedThroughConsole = false;
						CryLog("[tlh] calling SendPacket(eGUPD_SyncPlaylistRotation) [1]");
						SendPacket(eGUPD_SyncPlaylistRotation, pPacketData->connection);
					}
				}

				if(m_sessionHostOnly)
				{
					SendPacket(eGUPD_SetupArbitratedLobby);
				}

				if(m_state == eLS_Game)
				{
					if(m_sessionHostOnly)
					{
						SendPacket(eGUPD_SetupJoinCommand, pPacketData->connection);
					}
					SendPacket(eGUPD_LobbyGameHasStarted, pPacketData->connection);
				}
				else if(m_state == eLS_Lobby)
				{
					SendPacket(eGUPD_LobbyStartCountdownTimer, pPacketData->connection);
					SendPacket(eGUPD_SetTeamAndRank, pPacketData->connection);		// Temporary fix for team message being lost due to arriving before session join callback
				}
			}
			else
			{
				CryLog("    sending eGUPD_ReservationFailedSessionFull packet to {%" PRIu64 ",%d}", pPacketData->connection.m_sid, pPacketData->connection.m_uid);
				SendPacket(eGUPD_ReservationFailedSessionFull, pPacketData->connection);
			}

			break;
		}
	case eGUPD_ReservationsMade:
		{
			CryLog("  reading packet of type 'eGUPD_ReservationsMade'");
			CRY_ASSERT(!m_server);

			if(m_squadReservation)
			{
				CSquadManager *pSquadManager = g_pGame->GetSquadManager();
				if(pSquadManager->SquadsSupported())
				{
					CryLog("    sending eGUPD_SquadJoinGame packet!");
					pSquadManager->SendSquadPacket(eGUPD_SquadJoinGame);
				}
				else
				{
					CryLog("    squads not supported, sending eGUPD_SquadNotSupported packet!");
					pSquadManager->SendSquadPacket(eGUPD_SquadNotSupported);
				}
			}
			else
			{
				CRY_ASSERT(m_gameLobbyMgr->IsNewSession(this));
				m_gameLobbyMgr->NewSessionResponse(this, m_currentSessionId);
			}
			break;
		}
	case eGUPD_ReservationFailedSessionFull:
		{
			CryLog("  reading packet of type 'eGUPD_ReservationFailedSessionFull'");
			CRY_ASSERT(!m_server);

			if(m_squadReservation)
			{
				if (m_bMatchmakingSession)
				{
					// fix for DT #31667 and #31781
					CryLog("    ... session WAS a matchmaking (Quick Game) session, so leaving session and adding a new Create task to try again WITHOUT showing an error");
					LeaveSession(true, false);
					//TODO: handle this correctly with new Lua matchmaking
					m_taskQueue.AddTask(CLobbyTaskQueue::eST_Create, true);
				}
				else
				{
					CryLog("    ... session WASN'T a matchmaking (Quick Game) session, so showing error, deleting session and returning to main menu");
					CSquadManager::HandleCustomError("SquadManagerError", "@ui_menu_squad_error_not_enough_room", true, true);
				}
			}
			else
			{
				if (m_gameLobbyMgr->IsNewSession(this))
				{
					m_gameLobbyMgr->NewSessionResponse(this, CrySessionInvalidID);
				}
				else
				{
					// Tried to join a game where all remaining slots were reserved
					ShowErrorDialog(eCLE_SessionFull, NULL, NULL, NULL);
#if 0 // old frontend
					if (CMPMenuHub *pMPMenu = CMPMenuHub::GetMPMenuHub())
					{
						pMPMenu->GoToCurrentLobbyServiceScreen();	// Go back to main menu screen
					}
#endif
					LeaveSession(true, false);
				}
			}
			break;
		}
	case eGUPD_SetGameVariant:
		{
			CRY_ASSERT(!m_server);

			CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
			if (pPlaylistManager)
			{
				pPlaylistManager->ReadSetVariantPacket(pPacket);

				if(pPlaylistManager->IsUsingCustomVariant())
				{
					//Logically CustomVariants are the same as private games for external systems. (Should be renamed to UnrankedGame)
					m_gameLobbyMgr->SetPrivateGame(this, true);
				}
			}

			// Need to refresh flash
			m_sessionUserDataDirty = true;

			break;
		}
	case eGUPD_SyncPlaylistRotation:
		{
			CryLog("[tlh]   reading packet of type 'eGUPD_SyncPlaylistRotation'");
			CRY_ASSERT(!m_server);

			const uint32  seed = pPacket->ReadUINT32();
			m_playListSeed = seed;
			const int  curNextIdx = (int) pPacket->ReadUINT8();
			const bool  gameHasStarted = pPacket->ReadBool();
			const bool bAdvancedThroughConsole = pPacket->ReadBool();

			CryLog("[tlh]     reading next=%d, started=%d, advancedThruConsole=%d seed=%d", curNextIdx, gameHasStarted, bAdvancedThroughConsole, seed);
#if 0 // LEVEL ROTATION DISABLED FOR NOW
			if (CPlaylistManager* pPlaylistManager=g_pGame->GetPlaylistManager())
			{
				CRY_ASSERT(pPlaylistManager->HavePlaylistSet());

				if (pPlaylistManager->GetLevelRotation()->IsRandom())
				{
					pPlaylistManager->GetLevelRotation()->Shuffle(seed);
				}
				pPlaylistManager->AdvanceRotationUntil(curNextIdx);
				m_gameHadStartedWhenPlaylistRotationSynced = gameHasStarted;
				m_hasReceivedPlaylistSync = true;

				// Need to refresh flash
				m_sessionUserDataDirty = true;

				if (bAdvancedThroughConsole)
				{
					m_votingClosed = false;
					m_leftVoteChoice.Reset();
					m_rightVoteChoice.Reset();
					m_highestLeadingVotesSoFar = 0;
					m_leftHadHighestLeadingVotes = false;

					SetLocalVoteStatus(eLVS_notVoted);
				}

				UpdateVoteChoices();

				UpdateVotingCandidatesFlashInfo();
				UpdateVotingInfoFlashInfo();

				RefreshCustomiseEquipment();
			}
#endif
			break;
		}
	case eGUPD_UpdateCountdownTimer:
		{
			m_startTimer = (float) pPacket->ReadUINT8();
			break;
		}
	case eGUPD_RequestAdvancePlaylist:
		{
			DebugAdvancePlaylist();
			break;
		}
	case eGUPD_SyncPlaylistContents:
		{
			CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
			const ILevelRotation::TExtInfoId playlistId = pPlaylistManager->CreateCustomPlaylist(PLAYLIST_MANAGER_CUSTOM_NAME);

			ILevelRotation *pRotation = g_pGame->GetIGameFramework()->GetILevelSystem()->CreateNewRotation(playlistId);

			const int numLevels = (int) pPacket->ReadUINT8();
			for (int i = 0; i < numLevels; ++ i)
			{
				const uint32 gameRulesHash = pPacket->ReadUINT32();
				const uint32 levelHash = pPacket->ReadUINT32();

				const char *pGameRules = GameLobbyData::GetGameRulesFromHash(gameRulesHash);
				const char *pLevel = GameLobbyData::GetMapFromHash(levelHash);

				pRotation->AddLevel(pLevel, pGameRules);
			}

			pPlaylistManager->ChoosePlaylistById(playlistId);

			break;
		}
	case eGUPD_SyncExtendedServerDetails:
		{
			CryLog("  reading eGUPD_SyncExtendedServerDetails");

			const bool  isPrivate = pPacket->ReadBool();
			CryLog("    read isPrivate=%s, calling SetPrivateGame()", (isPrivate?"true":"false"));
			SetPrivateGame(isPrivate);

			const bool  isPassworded = pPacket->ReadBool();
			CryLog("    read isPassworded=%s, calling SetPasswordedGame()", (isPassworded?"true":"false"));
			SetPasswordedGame(isPassworded);

			break;
		}
	case eGUPD_SetAutoStartingGame:
		{
			m_bIsAutoStartingGame = pPacket->ReadBool();
			CryLog("  reading eGUPD_SetAutoStartingGame %d", m_bIsAutoStartingGame);
			break;
		}
	case eGUPD_TeamBalancingSetup:	// deliberate fall-through
	case eGUPD_TeamBalancingAddPlayer:
	case eGUPD_TeamBalancingRemovePlayer:
	case eGUPD_TeamBalancingUpdatePlayer:
		{
			m_teamBalancing.ReadPacket(pPacket, packetType);
			break;
		}
	default:
		{
			CRY_ASSERT_MESSAGE(0, "Got packet another something - just no idea what");
		}
		break;
	}

	CRY_ASSERT_MESSAGE(pPacket->GetReadBufferSize() == pPacket->GetReadBufferPos(), "Haven't read all the data");
}


//static------------------------------------------------------------------------
const char* CGameLobby::GetValidGameRules(const char* gameRules, bool returnBackup/*=false*/)
{
	if (!gameRules)
		return "";

	bool validRules = false;
	const char * fullGameRulesName = g_pGame->GetIGameFramework()->GetIGameRulesSystem()->GetGameRulesName(gameRules);
	const char *backupGameRules = 0;

	CGameRulesModulesManager *pGameRulesModulesManager = CGameRulesModulesManager::GetInstance();
	const int rulesCount = pGameRulesModulesManager->GetRulesCount();
	for(int i = 0; i < rulesCount; ++i)
	{
		const char* name = pGameRulesModulesManager->GetRules(i);

		if (!backupGameRules)
			backupGameRules = name;

		assert(name);
		PREFAST_ASSUME(name);

		if(fullGameRulesName != NULL && (stricmp(name, fullGameRulesName)==0))
		{
			validRules = true;
			break;
		}
	}

	if (validRules)
	{
		return fullGameRulesName;
	}
	else if (returnBackup && backupGameRules)
	{
		CryLog("Failed to find valid gamemode (tried %s), using backup %s", fullGameRulesName?fullGameRulesName:"<null> ", backupGameRules);
		return backupGameRules;
	}

	return "";
}

//static ------------------------------------------------------------------------
CryFixedStringT<32> CGameLobby::GetValidMapForGameRules(const char* inLevel, const char* gameRules, bool returnBackup/*=false*/)
{
	if (!gameRules)
		return "";

	CryFixedStringT<32> levelName = inLevel;

	const char *backupLevelName = 0;
	bool validLevel = false;

	ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
	if (levelName.find("/")==string::npos)
	{
		int j=0;
		const char *loc=0;
		CryFixedStringT<32> tmp;
		IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
		while(loc=pGameRulesSystem->GetGameRulesLevelLocation(gameRules, ++j))
		{
			tmp=loc;
			tmp.append(levelName);

			if (pLevelSystem->GetLevelInfo(tmp.c_str()))
			{
				levelName=tmp;
				break;
			}
		}
	}

	const int levelCount = pLevelSystem->GetLevelCount();
	for(int i = 0; i < levelCount; ++i)
	{
		ILevelInfo *pLevelInfo = pLevelSystem->GetLevelInfo(i);
		if (pLevelInfo->SupportsGameType(gameRules))
		{
			const char* name = pLevelInfo->GetName();

			assert(name);
			PREFAST_ASSUME(name);

			if (!backupLevelName)
				backupLevelName = name;

			if(stricmp(name, levelName.c_str())==0)
			{
				validLevel = true;
				break;
			}
		}
	}

	if (validLevel)
	{
		return levelName;
	}
	else if (returnBackup && backupLevelName)
	{
		CryLog("Failed to find valid level (tried %s) for %s gamemode, using backup %s Lobby: ", levelName.c_str(), gameRules, backupLevelName);
		return backupLevelName;
	}

	return "";
}

void CGameLobby::RecievedChatMessage(const SChatMessage* message)
{
	SSessionNames::SSessionName *player = m_nameList.GetSessionName(message->conId, false);
	if (player)
	{
		CryFixedStringT<DISPLAY_NAME_LENGTH> displayName;
		player->GetDisplayName(displayName);
		if ((m_state == eLS_Game) && g_pGame->GetGameRules())
		{
			CryLog("%s %s%s%s", CHUDUtils::LocalizeString((message->teamId!=0)?"@ui_chattype_team":"@ui_chattype_all"), displayName.c_str(),CHAT_MESSAGE_POSTFIX, message->message.c_str());

			IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(message->conId.m_uid);
			if (pActor)
			{
				CUIMultiPlayer* pUIEvt = UIEvents::Get<CUIMultiPlayer>();
				if (pUIEvt)
					pUIEvt->OnChatReceived(pActor->GetEntityId(), message->teamId, message->message.c_str());

			}
			else
			{
				CryLog("Failed to find actor for chat message");
			}
		}
		else
		{
#if ENABLE_CHAT_MESSAGES
			CryLog("%s> %s", displayName.c_str(), message->message.c_str());

			++m_chatMessagesIndex;
			if (m_chatMessagesIndex >= NUM_CHATMESSAGES_STORED)
			{
				m_chatMessagesIndex = 0;
			}

			bool bIsSquaddie = false;
			if(CSquadManager* pSM = g_pGame->GetSquadManager())
			{
				bIsSquaddie = pSM->IsSquadMateByUserId( player->m_userId );
			}

			SSessionNames::SSessionName *localPlayer = &m_nameList.m_sessionNames[0];
			m_chatMessagesArray[m_chatMessagesIndex].Set(displayName.c_str(), message->message.c_str(), player == localPlayer, bIsSquaddie);

#if 0 // old frontend
			CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
			if (pFlashMenu && pFlashMenu->IsMenuActive(eFlM_Menu))
			{
				if (IFlashPlayer *pFlashPlayer = pFlashMenu->GetFlash())
				{
					EFlashFrontEndScreens screen = pFlashMenu->GetCurrentMenuScreen();
					if (screen == eFFES_game_lobby)
					{
						m_chatMessagesArray[m_chatMessagesIndex].SendChatMessageToFlash(pFlashPlayer, false);
					}
				}
			}
#endif
#endif
		}
	}
	else if(message->conId == CryMatchMakingInvalidConnectionUID)
	{
		if ((m_state == eLS_Game) && g_pGame->GetGameRules())
		{
			CUIMultiPlayer* pUIEvt = UIEvents::Get<CUIMultiPlayer>();
			if (pUIEvt)
				pUIEvt->OnChatReceived(0, message->teamId, message->message.c_str());

		}
	}
	else
	{
		CryLog("[Unknown] %s> %s", (message->teamId==0)?"":"[team] ", message->message.c_str());
	}
}

void CGameLobby::SendChatMessageToClients()
{
	CRY_ASSERT(m_server);

#ifndef _RELEASE
	CryLog("SERVER SendChatMessageToClients: channel:%d  team:%d, message:%s", m_chatMessageStore.conId.m_uid, m_chatMessageStore.teamId, m_chatMessageStore.message.c_str());
#endif

	const int nameSize = m_nameList.Size();
	for (int i=0; i<nameSize; ++i)
	{
		SSessionNames::SSessionName &player = m_nameList.m_sessionNames[i];

		int playerTeam = player.m_teamId;
		if ((m_state == eLS_Game) && g_pGame->GetGameRules())
		{
			IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(player.m_conId.m_uid);
			if (pActor)
			{
				playerTeam = g_pGame->GetGameRules()->GetTeam(pActor->GetEntityId());
			}
		}

		if (m_chatMessageStore.teamId==0 || m_chatMessageStore.teamId==playerTeam)
		{
			if (!gEnv->IsDedicated() && i==0) // Assume the server at this point is 0 - only on non-dedicated
			{
				RecievedChatMessage(&m_chatMessageStore);
			}
			else
			{
				SendPacket(eGUPD_ChatMessage, player.m_conId);
			}
		}
	}

	m_chatMessageStore.Clear();
}

#if ENABLE_CHAT_MESSAGES
//static-------------------------------------------------------------------------
void CGameLobby::CmdChatMessage(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArg(1))
	{
		CGameLobby* pLobby = g_pGame->GetGameLobby();
		if(pLobby)
		{
			pLobby->SendChatMessage(false, pCmdArgs->GetArg(1));
		}
	}
	else
	{
		CryLog("gl_say : you must provide a message");
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::CmdChatMessageTeam(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArg(1))
	{
		CGameLobby* pLobby = g_pGame->GetGameLobby();
		if(pLobby)
		{
			pLobby->SendChatMessage(true, pCmdArgs->GetArg(1));
		}
	}
	else
	{
		CryLog("gl_teamsay : you must provide a message");
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::SendChatMessageTeamCheckProfanityCallback( CryLobbyTaskID taskID, ECryLobbyError error, const char* pString, bool isProfanity, void* pArg )
{
	CGameLobby* pLobby = g_pGame->GetGameLobby();
	if(pLobby)
	{
		pLobby->SendChatMessageCheckProfanityCallback(true, taskID, error, pString, isProfanity, pArg );
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::SendChatMessageAllCheckProfanityCallback( CryLobbyTaskID taskID, ECryLobbyError error, const char* pString, bool isProfanity, void* pArg )
{
	CGameLobby* pLobby = g_pGame->GetGameLobby();
	if(pLobby)
	{
		pLobby->SendChatMessageCheckProfanityCallback(false, taskID, error, pString, isProfanity, pArg );
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SendChatMessageCheckProfanityCallback( const bool team, CryLobbyTaskID taskID, ECryLobbyError error, const char* pString, bool isProfanity, void* pArg )
{
	CryLog("CGameLobby::SendChatMessageCheckProfanityCallback()");
	INDENT_LOG_DURING_SCOPE();
	m_profanityTask = CryLobbyInvalidTaskID;

	if(!isProfanity)
	{
		CryLog("Profanity Check ok for '%s'", pString);

		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if (pString && pGameLobby)
		{
			if (pGameLobby->IsCurrentlyInSession() && m_nameList.Size()>0)
			{
				SSessionNames::SSessionName &localPlayer = m_nameList.m_sessionNames[0];

#if !defined(DEDICATED_SERVER)
				int teamId = 0;
				if (team)
				{
					teamId = localPlayer.m_teamId;

					if ((m_state == eLS_Game) && g_pGame->GetGameRules())
					{
						IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(localPlayer.m_conId.m_uid);
						if (pActor)
						{
							teamId = g_pGame->GetGameRules()->GetTeam(pActor->GetEntityId());
						}
					}

					if (teamId == 0)
					{
						CryLog("Send team message : not on a team so sending to all");
					}
				}

				pGameLobby->m_chatMessageStore.Set(localPlayer.m_conId, teamId, pString);
#else
				pGameLobby->m_chatMessageStore.Set(CryMatchMakingInvalidConnectionUID, 0, pString);
#endif

				if (pGameLobby->m_chatMessageStore.message.size()>0)
				{
					CryLog("Sending '%s'", pString);
					if (m_server)
					{
						pGameLobby->SendChatMessageToClients();
					}
					else
					{
						pGameLobby->SendPacket(eGUPD_SendChatMessage);
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SendChatMessage(bool team, const char* message)
{
	CryLog("CGameLobby::SendChatMessage()");
	INDENT_LOG_DURING_SCOPE();

	if(m_profanityTask != CryLobbyInvalidTaskID)
	{
		CryLog("CGameLobby::SendChatMessage() :: Can't send chat message still waiting for profanity check to return!");
		return;
	}

	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (message && pGameLobby)
	{
		if (pGameLobby->IsCurrentlyInSession() && m_nameList.Size()>0)
		{
			SSessionNames::SSessionName &localPlayer = m_nameList.m_sessionNames[0];

			int teamId = 0;
			if (team)
			{
				teamId = localPlayer.m_teamId;

				if ((m_state == eLS_Game) && g_pGame->GetGameRules())
				{
					IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(localPlayer.m_conId.m_uid);
					if (pActor)
					{
						teamId = g_pGame->GetGameRules()->GetTeam(pActor->GetEntityId());
					}
				}

				if (teamId == 0)
				{
					CryLog("Send team message : not on a team so sending to all");
					team = false;
				}
			}

			CryLobbyCheckProfanityCallback whichFunctor;
			if(team)
			{
				whichFunctor = SendChatMessageTeamCheckProfanityCallback;
			}
			else
			{
				whichFunctor = SendChatMessageAllCheckProfanityCallback;
			}

			whichFunctor( CryLobbyInvalidTaskID, eCLE_Success, message, false, NULL );
		}
	}
}
#endif

//static-------------------------------------------------------------------------
void CGameLobby::CmdStartGame(IConsoleCmdArgs* pCmdArgs)
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		if(pGameLobby->m_server)
		{
			// Check if any players are connected without the required DLC
			const int nameSize = pGameLobby->m_nameList.Size();

			stack_string playersMissingDLC;
			uint32 requiredDLCs = g_pGame->GetDLCManager()->GetRequiredDLCs();
			for (int i=0; i<nameSize; ++i)
			{
				SSessionNames::SSessionName &player = pGameLobby->m_nameList.m_sessionNames[i];
				uint32 loadedDLC = (uint32)player.m_userData[eLUD_LoadedDLCs];
				if (!CDLCManager::MeetsDLCRequirements(requiredDLCs, loadedDLC))
				{
					CryLog("CGameLobby::CmdStartGame: %s does not meet DLC requirements for this game", player.m_name);
					if (!playersMissingDLC.empty())
					{
						playersMissingDLC.append(", ");
					}
					playersMissingDLC.append(player.m_name);
				}
			}
#if !defined( _RELEASE )
			if( !playersMissingDLC.empty() && !s_pGameLobbyCVars->gl_allowDevLevels )
#else
			if (!playersMissingDLC.empty())
#endif //!defined( _RELEASE )
			{
				g_pGame->AddGameWarning("DlcNotAvailable", "a player does not meet DLC requirement");
			}
			else
			{
				pGameLobby->m_bSkipCountdown = true;
			}
		}
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::CmdSetMap(IConsoleCmdArgs* pCmdArgs)
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		if(pCmdArgs->GetArgCount() == 2)
		{
			if(!pGameLobby->m_server)
			{
				CryLog("Server only command");
				return;
			}

			const char *gameRules = pGameLobby->GetCurrentGameModeName();

			CryFixedStringT<32> levelName = CGameLobby::GetValidMapForGameRules(pCmdArgs->GetArg(1), gameRules, false);

			if (levelName.empty()==false)
			{
				pGameLobby->SetCurrentLevelName(levelName.c_str());

				if (g_pGame->GetIGameFramework()->StartedGameContext() || g_pGame->GetIGameFramework()->StartingGameContext())
				{
					gEnv->pConsole->ExecuteString("unload", false, true);
					pGameLobby->SetState(eLS_EndSession);
					pGameLobby->m_bServerUnloadRequired = false;

					pGameLobby->m_pendingLevelName = levelName.c_str();
				}
				else
				{
					ICVar *pCVar = gEnv->pConsole->GetCVar("sv_gamerules");
					if (pCVar)
					{
						pCVar->Set(pGameLobby->m_currentGameRules.c_str());
					}

					CryFixedStringT<128> command;
					command.Format("map %s s nb", levelName.c_str());
					gEnv->pConsole->ExecuteString(command.c_str(), false, true);
					pGameLobby->m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
				}
			}
			else
			{
				//This is a console command, CryLogAlways usage is correct
				CryLogAlways("Failed to find level or level not supported on current gamerules");
			}
		}
		else
		{
			//This is a console command, CryLogAlways usage is correct
			CryLogAlways("Usage: gl_map <mapname>");
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::ChangeMap( const char *pMapName )
{
	const char *gameRules = GetCurrentGameModeName();

	CryFixedStringT<32> levelName = CGameLobby::GetValidMapForGameRules(pMapName, gameRules, false);

	if (levelName.empty() == false)
	{
		SetCurrentLevelName(levelName.c_str());
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
	}

	//PrecachePaks(pMapName);
}

//static-------------------------------------------------------------------------
void CGameLobby::CmdSetGameRules(IConsoleCmdArgs* pCmdArgs)
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		if(pCmdArgs->GetArgCount() == 2)
		{
			if(!pGameLobby->m_server)
			{
				CryLog("Server only command");
				return;
			}

			CryFixedStringT<32> validGameRules = CGameLobby::GetValidGameRules(pCmdArgs->GetArg(1),false);

			if(validGameRules.empty()==false)
			{
				pGameLobby->m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
				pGameLobby->GameRulesChanged(validGameRules.c_str());
			}
			else
			{
				//This is a console command, CryLogAlways usage is correct
				CryLogAlways("Failed to find rules");
			}
		}
		else
		{
			//This is a console command, CryLogAlways usage is correct
			CryLogAlways("Usage: gl_GameRules <substr of gamerules>");
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::ChangeGameRules( const char *pGameRulesName )
{
	CryFixedStringT<32> validGameRules = CGameLobby::GetValidGameRules(pGameRulesName, false);

	if (validGameRules.empty()==false)
	{
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
		CryLog("Set GameRules '%s'", validGameRules.c_str());
		GameRulesChanged(validGameRules.c_str());
	}
}

//static-------------------------------------------------------------------------
void CGameLobby::CmdVote(IConsoleCmdArgs* pCmdArgs)
{
	CryLog("CGameLobby::CmdVote()");

	if (CGameLobby* gl=g_pGame->GetGameLobby())
	{
		if (gl->m_state == eLS_Lobby)
		{
			if (gl->m_votingEnabled)
			{
				if (gl->m_localVoteStatus == eLVS_notVoted)
				{
					bool  showUsage = true;

					if (pCmdArgs->GetArgCount() == 2)
					{
						if (const char* voteArg=pCmdArgs->GetArg(1))
						{
							if (!stricmp("left", voteArg))
							{
								gl->SetLocalVoteStatus(eLVS_votedLeft);
								VOTING_DBG_LOG("[tlh] set m_localVoteStatus [5] to eLVS_votedLeft");
							}
							else if (!stricmp("right", voteArg))
							{
								gl->SetLocalVoteStatus(eLVS_votedRight);
								VOTING_DBG_LOG("[tlh] set m_localVoteStatus [6] to eLVS_votedRight");
							}

							CryLog("gl_Vote: \"%s\" vote cast", voteArg);
						}
					}

					if (showUsage)
					{
						CryLog("Usage: gl_Vote <left/right>");
					}
				}
				else
				{
					CryLog("gl_Vote: can only vote once!");
				}
			}
			else
			{
				CryLog("gl_Vote: voting is not enabled for this lobby session!");
			}
		}
		else
		{
			CryLog("gl_Vote: lobby not in correct state for voting!");
		}
	}
}

//---------------------------------------
void CGameLobby::MatchmakingSessionRoomOwnerChangedCallback( UCryLobbyEventData eventData, void *userParam )
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *lobby = static_cast<CGameLobby *>(userParam);
	SCryLobbyRoomOwnerChanged	*pRoomOwnerChangedData = eventData.pRoomOwnerChanged;

	if (lobby->m_currentSession == pRoomOwnerChangedData->m_session)
	{
		uint32 ip = pRoomOwnerChangedData->m_ip;
		uint16 port = pRoomOwnerChangedData->m_port;
		cry_sprintf(lobby->m_joinCommand, sizeof(lobby->m_joinCommand), "connect <session>%08X,%d.%d.%d.%d:%d", pRoomOwnerChangedData->m_session, ((uint8*)&ip)[0], ((uint8*)&ip)[1], ((uint8*)&ip)[2], ((uint8*)&ip)[3], port);
		CryLog("CGameLobby::MatchmakingSessionRoomOwnerChangedCallback() setting join command to %s", lobby->m_joinCommand);
	}
	else
	{
		CryLog("CGameLobby::MatchmakingSessionRoomOwnerChangedCallback() on wrong session, ignoring");
	}
}


//---------------------------------------
void CGameLobby::MatchmakingSessionJoinUserCallback( UCryLobbyEventData eventData, void *userParam )
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *lobby = static_cast<CGameLobby *>(userParam);
	if(eventData.pSessionUserData->session == lobby->m_currentSession)
	{
		lobby->InsertUser(&eventData.pSessionUserData->data);
	}
}

//---------------------------------------
void CGameLobby::MatchmakingSessionLeaveUserCallback( UCryLobbyEventData eventData, void *userParam )
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *lobby = static_cast<CGameLobby *>(userParam);
	if(eventData.pSessionUserData->session == lobby->m_currentSession)
	{
		lobby->RemoveUser(&eventData.pSessionUserData->data);
	}
}

//---------------------------------------
void CGameLobby::MatchmakingSessionUpdateUserCallback( UCryLobbyEventData eventData, void *userParam )
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *lobby = static_cast<CGameLobby *>(userParam);
	if(eventData.pSessionUserData->session == lobby->m_currentSession)
	{
		lobby->UpdateUser(&eventData.pSessionUserData->data);
	}
}

//---------------------------------------
void CGameLobby::MatchmakingLocalUserDataCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* arg)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(arg);
	pGameLobby->NetworkCallbackReceived(taskID, error);
}

//---------------------------------------
void CGameLobby::SetCurrentLevelName(const char *pCurrentLevelName, bool preCacheLevel)
{
	CryLog("SetCurrentLevelName %s pLobby %p preCacheLevel = %s", pCurrentLevelName, this, preCacheLevel ? "True" : "False");
	m_currentLevelName = pCurrentLevelName;

	if(preCacheLevel)
	{
		PrecachePaks(pCurrentLevelName);
	}
}

//---------------------------------------
void CGameLobby::InviteAcceptedCallback( UCryLobbyEventData eventData, void *userParam )
{
	SCryLobbyInviteAcceptedData* data = eventData.pInviteAcceptedData;

	// set everything to multiplayer and join the requested game
	CGameLobby *pLobby = static_cast<CGameLobby *>(userParam);
	CGameLobbyManager *pGameLobbyMgr = g_pGame->GetGameLobbyManager();
	CSquadManager *pSquadMgr = g_pGame->GetSquadManager();

	gEnv->pGameFramework->InitGameType(true, false);

	if(pGameLobbyMgr)
	{
		pGameLobbyMgr->SetMultiplayer(true);
	}

	if(pSquadMgr)
	{
		pSquadMgr->SetMultiplayer(true);
	}

	CGameLobby::SetLobbyService(data->m_service);

	pLobby->JoinServer(data->m_id, "FROM INVITE", CryMatchMakingInvalidConnectionUID, false);
}

//---------------------------------------
void CGameLobby::GameRulesChanged( const char *pGameRules )
{
#if 0 // LEVEL ROTATION DISABLED FOR NOW
	CGameRulesModulesManager *pGameRulesModulesManager = CGameRulesModulesManager::GetInstance();

	m_hasValidGameRules = pGameRulesModulesManager->IsValidGameRules(pGameRules);
	if(m_hasValidGameRules)
	{
		bool isTeamGame = pGameRulesModulesManager->UsesLobbyTeamBalancing(pGameRules);
		if (m_isTeamGame != isTeamGame)
		{
			m_isTeamGame = isTeamGame;
			if(!m_isTeamGame)
			{
				CryLog("    GameRulesChanged::ummuting all players Lobby: %p", this);
				MutePlayersOnTeam(0, false);
				MutePlayersOnTeam(1, false);
				MutePlayersOnTeam(2, false);
			}
		}

		m_isAsymmetricGame = pGameRulesModulesManager->IsAsymmetric(pGameRules);
		m_replayMapWithTeamsSwitched = m_isAsymmetricGame;

#else

	if (gEnv->pGameFramework->GetIGameRulesSystem()->HaveGameRules(pGameRules))
	{
		ICVar *pCVar = gEnv->pConsole->GetCVar("sv_gamerules");
		if (pCVar)
		{
			pCVar->Set(pGameRules);
		}

		m_currentGameRules = pGameRules;
	}

#endif
	CryLog("[CG] CGameLobby::GameRulesChanged() newRules=%s, hasValidRules=%i, isTeamGame=%i Lobby: %p", pGameRules, m_hasValidGameRules?1:0, m_isTeamGame?1:0, this);
}

//------------------------------------------------------------------------
void CGameLobby::GetProgressionInfoByChannel(int channelId, uint8 &rank, uint8 &reincarnations)
{
	CrySessionHandle hdl = m_currentSession;

	if(hdl != CrySessionInvalidHandle)
	{
		SCryMatchMakingConnectionUID temp = gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetConnectionUIDFromGameSessionHandleAndChannelID(hdl, channelId);
		SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionName(temp, true);
		if (pPlayer)
		{
			rank = pPlayer->m_rank;
			reincarnations = pPlayer->m_reincarnations;
		}
	}
}

//------------------------------------------------------------------------
void CGameLobby::SetProgressionInfoByChannel(int channelId, uint8 rank, uint8 reincarnations)
{
	assert(m_server);

	CrySessionHandle hdl = m_currentSession;

	if(hdl != CrySessionInvalidHandle)
	{
		SCryMatchMakingConnectionUID connectionUid = gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetConnectionUIDFromGameSessionHandleAndChannelID(hdl, channelId);
		SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionName(connectionUid, true);
		if (pPlayer)
		{
			if (pPlayer->m_rank != rank || pPlayer->m_reincarnations != reincarnations)
			{
				pPlayer->m_rank = rank;
				pPlayer->m_reincarnations = reincarnations;
				m_nameList.m_dirty = true;
				SendPacket(eGUPD_SetTeamAndRank);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameLobby::GetPlayerNameFromChannelId(int channelId, CryFixedStringT<CRYLOBBY_USER_NAME_LENGTH> &name)
{
	SCryMatchMakingConnectionUID temp = gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetConnectionUIDFromGameSessionHandleAndChannelID(m_currentSession, channelId);
	SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionName(temp, true);

	if (pPlayer)
	{
		name = pPlayer->m_name;
	}
}

void CGameLobby::GetClanTagFromChannelId(int channelId, CryFixedStringT<CLAN_TAG_LENGTH> &name)
{
	SCryMatchMakingConnectionUID temp = gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetConnectionUIDFromGameSessionHandleAndChannelID(m_currentSession, channelId);
	SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionName(temp, true);
	if (pPlayer)
	{
		pPlayer->GetClanTagName(name);
	}
}

void CGameLobby::LocalUserDataUpdated()
{
	if (IsCurrentlyInSession())
	{
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetLocalUserData, true);
	}
}

//---------------------------------------
bool CGameLobby::IsLocalChannelUser(int channelId)
{
	SSessionNames& snames = m_nameList;
	if( snames.Size() )
	{
		SCryMatchMakingConnectionUID temp = gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetConnectionUIDFromGameSessionHandleAndChannelID(m_currentSession, channelId);
		SSessionNames::SSessionName *pPlayer = snames.GetSessionName(temp, true);
		// Assume local player is sessionName 0, compare the channel id
		if (pPlayer && (pPlayer->m_conId.m_uid == snames.m_sessionNames[0].m_conId.m_uid))
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------
CryUserID CGameLobby::GetLocalUserId( void )
{
	SSessionNames& snames = m_nameList;
	if( snames.Size() )
	{
		return snames.m_sessionNames[0].m_userId;
	}

	return NULL;
}


//---------------------------------------
void CGameLobby::GetLocalUserDisplayName(CryFixedStringT<DISPLAY_NAME_LENGTH> &displayName)
{
	SSessionNames& snames = m_nameList;
	if( snames.Size() )
	{
		SSessionNames::SSessionName& sessionName = snames.m_sessionNames[0];
		sessionName.GetDisplayName(displayName);
	}
}

//---------------------------------------
bool CGameLobby::GetPlayerDisplayNameFromEntity(EntityId entityId, CryFixedStringT<DISPLAY_NAME_LENGTH> &displayName)
{
	bool bFound = false;

	IGameObject	*pGameObject = g_pGame->GetIGameFramework()->GetGameObject(entityId);
	const int channelId = pGameObject ? pGameObject->GetChannelId() : 0;
	if (channelId)
	{
		bFound = GetPlayerDisplayNameFromChannelId(channelId, displayName);
	}

#if !defined(_RELEASE)
	if (!bFound)
	{
		if (IEntity * pWho = gEnv->pEntitySystem->GetEntity(entityId))
		{
			static IEntityClass* sDummyPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer");
			IEntityClass* pEntityClass = pWho->GetClass();

			if (pEntityClass == sDummyPlayerClass)
			{
				displayName = pWho->GetName();
				return true;
			}
			else
			{
				GameWarning("failed to get display name for %s(%d) with channel id %d, it's not a dummy player and isn't in the session names list", pWho->GetName(), entityId, channelId);
			}
		}
		else
		{
			GameWarning("failed to get display name for entity %d, it doesn't appear to exist", entityId);
		}

		displayName = "ERROR: FAILED NAME LOOK-UP";
	}
#endif
	return bFound;
}

//---------------------------------------
bool CGameLobby::GetPlayerDisplayNameFromChannelId(int channelId, CryFixedStringT<DISPLAY_NAME_LENGTH> &displayName)
{
	CrySessionHandle mySession = m_currentSession;

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby != NULL && mySession != CrySessionInvalidHandle)
	{
		ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
		if (pMatchmaking != NULL)
		{
			SCryMatchMakingConnectionUID conID = pMatchmaking->GetConnectionUIDFromGameSessionHandleAndChannelID(mySession, channelId);

			SSessionNames::SSessionName *pSessionName = m_nameList.GetSessionName(conID, false);
			if (pSessionName)
			{
				pSessionName->GetDisplayName(displayName);
				return true;
			}
		}
	}
	return false;
}

//---------------------------------------
bool CGameLobby::CanShowGamercard(CryUserID userId)
{
	return false;
}

//---------------------------------------
void CGameLobby::ShowGamercardByUserId(CryUserID userId)
{
}

//---------------------------------------
int CGameLobby::GetTeamByChannelId( int channelId )
{
	int teamId = 0;

	CrySessionHandle mySession = m_currentSession;

	if ((g_pGameCVars->g_autoAssignTeams != 0) && (m_isTeamGame) && (mySession != CrySessionInvalidHandle))
	{
		// For now just assign teams alternately
		teamId = GetTeamByConnectionUID(GetConnectionUIDFromChannelID(channelId));
	}

	return teamId;
}

//---------------------------------------
int CGameLobby::GetTeamByConnectionUID( SCryMatchMakingConnectionUID uid )
{
	return m_teamBalancing.GetTeamId(uid);
}

//---------------------------------------
SCryMatchMakingConnectionUID CGameLobby::GetConnectionUIDFromChannelID(int channelId)
{
	SCryMatchMakingConnectionUID temp;

	CrySessionHandle mySession = m_currentSession;

	ICryMatchMaking* pMatchMaking = NULL;
	if (gEnv->pNetwork && mySession != CrySessionInvalidHandle)
	{
		ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{
			pMatchMaking = pLobby->GetMatchMaking();
			if (pMatchMaking)
			{
				temp = pMatchMaking->GetConnectionUIDFromGameSessionHandleAndChannelID(mySession, channelId);
			}
		}
	}

	return temp;
}

//---------------------------------------
CryPing CGameLobby::GetCurrentPingToHost()
{
	if( ICryMatchMaking *pMatchMaking = gEnv->pNetwork->GetLobby()->GetMatchMaking() )
	{
		CryPing ping;
		SCryMatchMakingConnectionUID localUID = m_nameList.m_sessionNames[0].m_conId;
		ECryLobbyError result = pMatchMaking->GetSessionPlayerPing( localUID, &ping );

		if( result == eCLE_Success )
		{
			return ping;
		}
	}
	return 0;
}

//---------------------------------------
void CGameLobby::UpdatePrivatePasswordedGame()
{
	assert(gEnv->IsDedicated());

	bool privateGame = false;
	bool passworded = false;
	ICVar *pCVar = gEnv->pConsole->GetCVar("sv_password");
	if (pCVar)
	{
		const char* password = pCVar->GetString();
		if(strcmp(password, "") != 0)
		{
			privateGame = true;
			passworded = true;
		}
	}

	SetPasswordedGame(passworded);
	SetPrivateGame(privateGame);
}

//---------------------------------------
void CGameLobby::FindGameEnter()
{
	m_shouldFindGames = true;
	CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
#if USE_DEDICATED_LEVELROTATION
	if (gEnv->IsDedicated())
	{
		// Don't try to merge if we're using a custom rotation
		m_shouldFindGames = !pPlaylistManager->IsUsingCustomRotation();
	}
#endif

	SetMatchmakingGame(true);

	if (gEnv->IsDedicated())
	{
		UpdatePrivatePasswordedGame();
	}
	else
	{
		SetPrivateGame(false); // Can't matchmake private games.
	}

#if GAME_LOBBY_USE_GAMERULES_SEARCH
	if (gEnv->IsDedicated())
	{
		CryLog("CGameLobby::FindGameEnter() calling FindGameCreateGame() Lobby: %p", this);
		FindGameCreateGame();
	}
	else if (!m_quickMatchGameRules.empty())
	{
		InitGameMatchmaking();
	}
	else
	{
		CryLog("CGameLobby::FindGameEnter() ERROR: No gamerules set");
	}
#else
	ILevelRotation *pLevelRotation = pPlaylistManager->GetLevelRotation();
	CRY_ASSERT(pLevelRotation->GetLength() > 0);

	if (pLevelRotation->GetLength() > 0)
	{
		const char *nextLevel = pLevelRotation->GetNextLevel();
		const char *nextGameRules = pLevelRotation->GetNextGameRules();

		//Set params prematurely for the lobby so find game screen looks acceptable
		m_userData[eLDI_Map].m_int32 = GameLobbyData::ConvertMapToHash(nextLevel);
		m_userData[eLDI_Gamemode].m_int32 = GameLobbyData::ConvertGameRulesToHash(nextGameRules);

		UpdateRulesAndLevel(nextGameRules, nextLevel);

		//Technically we should let the new system decide if it cares about level rotations
		//look to move this out of here, and rename this function to "prepare screen for search" or something

		if( gEnv->IsDedicated() == false )
		{
			InitGameMatchmaking();
		}
		else
		{
			CryLog("CGameLobby::FindGameEnter() callingFindGameCreateGame() Lobby: %p", this);
			FindGameCreateGame();
		}

	}
	else
	{
		CryLog("CGameLobby::FindGameEnter() ERROR: Current rotation has no levels in it");
	}
#endif
}

//---------------------------------------
void CGameLobby::InitGameMatchmaking()
{
	if( m_bMatchmakingSession)
	{
		if( CMatchMakingHandler* pMMhandler = m_gameLobbyMgr->GetMatchMakingHandler() )
		{
			pMMhandler->OnEnterMatchMaking();
		}
	}
}


//---------------------------------------
bool CGameLobby::MergeToServer( CrySessionID sessionId )
{
	if( sessionId == CrySessionInvalidID )
	{
		return false;
	}
	return m_gameLobbyMgr->NewSessionRequest(this, sessionId);
}


//---------------------------------------
void CGameLobby::FindGameMoveSession(CrySessionID sessionId)
{
	//means networking has ok-ed the start of the merge and we should complete it
	//think we can leave this in the lobby
	CRY_ASSERT(sessionId != CrySessionInvalidID);
	m_nextSessionId = sessionId;

	const int numPlayers = m_nameList.Size();

	CryLog("CGameLobby::FindGameMoveSession() numPlayers=%i", numPlayers);

	SendPacket(eGUPD_LobbyMoveSession);

	if (gEnv->IsDedicated())
	{
		m_gameLobbyMgr->CancelMoveSession(this);
	}
	else
	{
		// Everyone else has to leave before us or we end up in host migration horribleness
		if (numPlayers > 1)
		{
			m_gameLobbyMgr->MoveUsers(this);
			m_allowRemoveUsers = false;	// not allowed to remove users, we need them to stay until we switch lobbies

			CryLog("  CGameLobby::FindGameMoveSession can't leave yet");
			// Start from 1 since we're always the first person in the list
			for (int i = 1; i < numPlayers; ++ i)
			{
				SSessionNames::SSessionName &player = m_nameList.m_sessionNames[i];
				player.m_bMustLeaveBeforeServer = true;
			}
			m_isLeaving = true;
			m_leaveGameTimeout = s_pGameLobbyCVars->gl_leaveGameTimeout;

#if defined(TRACK_MATCHMAKING)
			//we're asking our clients to merge to a different session, log this
			if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
			{
				pMMTel->AddEvent( SMMServerRequestingMerge( m_currentSessionId, sessionId ) );
			}
#endif //defined (TRACK_MATCHMAKING)
		}
		else
		{
			CryLog("  CGameLobby::FindGameMoveSession leaving now");
			LeaveSession(true, false);		//end current session (when callback is finished should join nextSession)
		}
	}
}


//---------------------------------------
bool CGameLobby::BidirectionalMergingRequiresCancel(CrySessionID other)
{
	//-- if we are in a merging state, test the CrySessionID of the session attempting to merge into us
	CGameLobby* pNextLobby = m_gameLobbyMgr->GetNextGameLobby();

	CrySessionID sessionIdToCheck = CrySessionInvalidID;

	if (pNextLobby)
	{
		sessionIdToCheck = pNextLobby->m_currentSessionId;
		if (!sessionIdToCheck)
		{
			sessionIdToCheck = pNextLobby->m_pendingConnectSessionId;
		}
	}

	if (sessionIdToCheck && (GameNetworkUtils::CompareCrySessionId(sessionIdToCheck, other)))
	{
		CryLog("[LOBBYMERGE] Bidirectional Lobby merge detected!");

		//-- We have a bidirectional merge in progress.
		//-- In this case the CURRENT session with the "lowest" ID must cancel its merge,
		//-- which means the higher sessionID players always join the lower sessionID session.

		if (*m_currentSessionId < *other)
		{
			CryLog("[LOBBYMERGE] Aborting merge at this end.");
			return true;
		}

		CryLog("[LOBBYMERGE] Continuing merge at this end.");
		return false;
	}

	CryLog("[LOBBYMERGE] Aborting merge, not sure what state we're in. (pNextLobby=%p, next->id=%p, other=%p)", pNextLobby, pNextLobby ? pNextLobby->m_currentSessionId.get() : NULL, other.get());
	return true;
}

//---------------------------------------
void CGameLobby::FindGameCancelMove()
{
	//can stay in lobby, may need to inform matchmaking handler
	CryLog("CGameLobby::FindGameCancelMove() lobby=%p", this);
	if (m_gameLobbyMgr->IsPrimarySession(this) && !m_gameLobbyMgr->IsMergingComplete())
	{
		m_findGameTimeout = GetFindGameTimeout();	//timeout to search again
		m_gameLobbyMgr->CancelMoveSession(this);

		m_allowRemoveUsers = true;

		// Canceled the move, need to start searching again
		CGameLobby::s_bShouldBeSearching = true;

		CGameBrowser *pGameBrowser = g_pGame->GetGameBrowser();
		if (pGameBrowser)
		{
			pGameBrowser->CancelSearching();
		}
	}
}

//---------------------------------------
float CGameLobby::GetFindGameTimeout()
{
	//not part of the lobbies anymore, but may be a useful utility
	float randf = (((float) (g_pGame->GetRandomNumber() % 1000))/1000.0f);

	uint32 numPlayers = m_nameList.Size();

	return s_pGameLobbyCVars->gl_findGameTimeoutBase + (s_pGameLobbyCVars->gl_findGameTimeoutPerPlayer * numPlayers) + (randf * s_pGameLobbyCVars->gl_findGameTimeoutRandomRange);
}

//---------------------------------------
void CGameLobby::MakeReservations(SSessionNames* nameList, bool squadReservation)
{
	CRY_ASSERT(nameList->Size() > 0);
	CryLog("CGameLobby::MakeReservations() lobby:%p, nameListSize=%u, m_server=%s, squadReservation=%s", this, nameList->Size(), m_server ? "true" : "false", squadReservation ? "true" : "false");
	if (m_server)
	{
		SCryMatchMakingConnectionUID  reservationRequests[MAX_RESERVATIONS];
		int numReservationsToRequest = BuildReservationsRequestList(reservationRequests, CRY_ARRAY_COUNT(reservationRequests), nameList);

		const EReservationResult result = DoReservations(numReservationsToRequest, reservationRequests);
		if (squadReservation)
		{
			g_pGame->GetSquadManager()->ReservationsFinished(result);
		}
	}
	else
	{
		m_reservationList = nameList;
		m_squadReservation = squadReservation;

		SendPacket(eGUPD_ReservationRequest);
	}
}

//---------------------------------------
int CGameLobby::BuildReservationsRequestList(SCryMatchMakingConnectionUID reservationRequests[], const int maxRequests, const SSessionNames*  members)
{
	const int  numMembers = members->Size();
	CRY_ASSERT(numMembers > 0);

	int  numReservationsToRequest = (numMembers - 1);
	CRY_ASSERT(numReservationsToRequest <= maxRequests);
	numReservationsToRequest = std::min(numReservationsToRequest, maxRequests);

	CryLog("[tlh]   reservations needed:");
	for (int i=0; i<numReservationsToRequest; ++i)
	{
		CRY_ASSERT((i + 1) < (int)members->m_sessionNames.size());
		reservationRequests[i] = members->m_sessionNames[i + 1].m_conId;  // NOTE assuming that the local user's connection id is always first in the list (hence [i + 1], as local user is not needed for reservations)

		CryLog("[tlh]     %02d: {%llu,%d}", (i + 1), reservationRequests[i].m_sid, reservationRequests[i].m_uid);
	}

#if SQUADMGR_DBG_ADD_FAKE_RESERVATION
	if ((numReservationsToRequest + 1) <= maxRequests)
	{
		SCryMatchMakingConnectionUID  fake = members->m_sessionNames[0].m_conId;
		fake.m_uid = 666;
		CryLog("[tlh]     ADDING FAKE RESERVATION! %02d: {%" PRIu64 ",%d}", (numReservationsToRequest + 1), fake.m_sid, fake.m_uid);
		reservationRequests[numReservationsToRequest] = fake;
		++numReservationsToRequest;
	}
#endif

	return numReservationsToRequest;
}

//---------------------------------------
EReservationResult CGameLobby::DoReservations(const int numReservationsRequested, const SCryMatchMakingConnectionUID requestedReservations[])
{
	EReservationResult  result = eRR_NoneNeeded;

	CGameLobby*  lobby = g_pGame->GetGameLobby();

	CRY_ASSERT(m_server);

	CRY_ASSERT(numReservationsRequested <= MAX_RESERVATIONS);
	CryLog("CGameLobby::DoReservations() numReservationsRequested=%d", numReservationsRequested);

	const float  timeNow = gEnv->pTimer->GetAsyncCurTime();

	CryLog("  counting (and refreshing) reservations:");
	int  reservedCount = 0;
	for (int i=0; i<CRY_ARRAY_COUNT(m_slotReservations); ++i)
	{
		SSlotReservation*  res = &m_slotReservations[i];
		CryLog("  %02d: con = {%" PRIu64 ",%d}, stamp = %f", (i + 1), res->m_con.m_sid, res->m_con.m_uid, res->m_timeStamp);

		if (res->m_con != CryMatchMakingInvalidConnectionUID)
		{
			if ((timeNow - res->m_timeStamp) > s_pGameLobbyCVars->gl_slotReservationTimeout)
			{
				CryLog("    slot has expired, so invalidating it");
				res->m_con = CryMatchMakingInvalidConnectionUID;
			}
			else
			{
				++reservedCount;
			}
		}
	}

	const int  numPrivate = lobby->GetNumPrivateSlots();
	const int  numPublic = lobby->GetNumPublicSlots();
	const int  numFilledExc = std::max<uint32>(0, (lobby->GetSessionNames().Size() - 1));  // NOTE -1 because client in question will be in list already but we don't want them to be included in the calculations
	const int  numEmptyExc = ((numPrivate + numPublic) - numFilledExc);

	CryLog("  nums private = %d, public = %d, filled (exc. leader) = %d, empty (exc. leader) = %d, reserved = %d", numPrivate, numPublic, numFilledExc, numEmptyExc, reservedCount);

	if ((numReservationsRequested + 1) <= (numEmptyExc - reservedCount))  // NOTE the +1 is for the leader
	{
		if (numReservationsRequested > 0)
		{
			CryLog("    got enough space left, processing requested reservations:");
			int  numDone = 0;
			for (int i=0; i<CRY_ARRAY_COUNT(m_slotReservations); ++i)
			{
				SSlotReservation*  res = &m_slotReservations[i];
				CryLog("       (existing) %02d: con = {%" PRIu64 ",%d}, stamp = %f", (i + 1), res->m_con.m_sid, res->m_con.m_uid, res->m_timeStamp);

				if (res->m_con == CryMatchMakingInvalidConnectionUID)
				{
					const SCryMatchMakingConnectionUID*  req = &requestedReservations[numDone];
					CryLog("        (new) %02d: {%" PRIu64 ",%d}, setting at index %d", (numDone + 1), req->m_sid, req->m_uid, i);

					res->m_con = (*req);
					res->m_timeStamp = timeNow;
					++numDone;

					if (numDone >= numReservationsRequested)
					{
						break;
					}
				}
			}
			result = eRR_Success;
		}
	}
	else
	{
		result = eRR_Fail;
	}

	return result;
}

//---------------------------------------
void CGameLobby::FindGameCreateGame()
{
	//not sure if this remains responsibility of lobby
#ifndef _RELEASE
	if (s_pGameLobbyCVars->gl_debugLobbyRejoin)
	{
		++ m_failedSearchCount;
		CryLog("CGameLobby::FindGameCreateGame() didn't find a game, counter = %i", m_failedSearchCount);
		FindGameEnter();
		return;
	}
#endif

	CRY_ASSERT_MESSAGE( CMatchMakingHandler::AllowedToCreateGame(), "[GameLobby] Trying to create a session when we're not allowed to!");

	m_taskQueue.AddTask(CLobbyTaskQueue::eST_Create, true);
}

//---------------------------------------
bool CGameLobby::GetConnectionUIDFromUserID( const CryUserID userId, SCryMatchMakingConnectionUID &result )
{
	CRY_ASSERT(userId.IsValid());
	if (userId.IsValid())
	{
		SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionNameByUserId(userId);
		if (pPlayer)
		{
			result = pPlayer->m_conId;
			return true;
		}
	}
	return false;
}

//---------------------------------------
bool CGameLobby::GetUserIDFromConnectionUID( const SCryMatchMakingConnectionUID &conId, CryUserID &result )
{
	if (conId != CryMatchMakingInvalidConnectionUID)
	{
		SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionName(conId, true);
		if (pPlayer)
		{
			result = pPlayer->m_userId;
			return true;
		}
	}
	return false;
}

//---------------------------------------
CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> CGameLobby::GetGUIDFromActorID(EntityId actorId)
{
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);
	if(pActor)
	{
		CryUserID userId = GetUserIDFromChannelID(pActor->GetChannelId());
		if(userId.IsValid())
		{
			return userId.get()->GetGUIDAsString();
		}
#if (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO) && !defined(_RELEASE)
		if(gEnv->pNetwork->GetLobby()->GetLobbyServiceType() == eCLS_LAN)
		{
			if (actorId==g_pGame->GetIGameFramework()->GetClientActorId())
			{
				static CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> s_nameBuffer;
				const char		*pLocalUser=g_pGame->GetIGameFramework()->GetISystem()->GetUserName();
				s_nameBuffer.Format("pc_lan_%s", pLocalUser);
				return s_nameBuffer;
			}
		}
#endif
	}

	return "";
}

//---------------------------------------
CryUserID CGameLobby::GetUserIDFromChannelID(int channelId)
{
	CryUserID userId = CryUserInvalidID;

	if (gEnv->pNetwork && gEnv->pNetwork->GetLobby())
	{
		if (ICryMatchMaking* pMatchMaking = gEnv->pNetwork->GetLobby()->GetMatchMaking())
		{
			CrySessionHandle sessionHandle = m_currentSession;
			if (sessionHandle != CrySessionInvalidHandle)
			{
				SCryMatchMakingConnectionUID conId = pMatchMaking->GetConnectionUIDFromGameSessionHandleAndChannelID(sessionHandle, channelId);
				GetUserIDFromConnectionUID(conId, userId);
			}
		}
	}

	return userId;
}

//---------------------------------------
void CGameLobby::MutePlayerByChannelId( int channelId, bool mute, int reason )
{
	CryUserID playerId = GetUserIDFromChannelID(channelId);
	SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionNameByUserId(playerId);
	if (pPlayer)
	{
		MutePlayerBySessionName(pPlayer, mute, reason);
	}
	else
	{
		CryLog("[CG] CGameLobby::MutePlayerByChannelId() failed to find player for channelId %i Lobby: %p", channelId, this);
	}
}

//---------------------------------------
void CGameLobby::MutePlayerBySessionName( SSessionNames::SSessionName *pUser, bool mute, int reason )
{
	if (pUser && pUser->m_userId.IsValid())
	{
		ICryVoice *pCryVoice = gEnv->pNetwork->GetLobby()->GetVoice();
		uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();
		if (pCryVoice)
		{
			if (reason & SSessionNames::SSessionName::MUTE_REASON_MANUAL)
			{
				pCryVoice->MuteExternally(userIndex, pUser->m_userId, mute);
				reason &= ~SSessionNames::SSessionName::MUTE_REASON_MANUAL;
			}

			bool isMuted = (pUser->m_muted != 0);
			if (mute)
			{
				pUser->m_muted |= reason;
			}
			else
			{
				pUser->m_muted &= ~reason;
			}
			bool shouldBeMuted = (pUser->m_muted != 0);
			//if (isMuted != shouldBeMuted)
			{
				CryLog("[CG] CGameLobby::MutePlayerBySessionName() Setting player '%s' to %s Lobby: %p", pUser->m_name, (mute ? "muted" : "un-muted"), this);
				pCryVoice->Mute(userIndex, pUser->m_userId, shouldBeMuted);
			}
		}
	}
}

//---------------------------------------
void CGameLobby::MutePlayersOnTeam( uint8 teamId, bool mute )
{
	const int numPlayers = m_nameList.Size();
	for (int i = 0; i < numPlayers; ++ i)
	{
		SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[i];
		if (pPlayer->m_teamId == teamId)
		{
			MutePlayerBySessionName(pPlayer, mute, SSessionNames::SSessionName::MUTE_REASON_WRONG_TEAM);
		}
	}
}

//---------------------------------------
int CGameLobby::GetPlayerMuteReason(CryUserID userId)
{
	int result = 0;

	if (userId.IsValid())
	{
		SSessionNames::SSessionName *pPlayer = m_nameList.GetSessionNameByUserId(userId);
		if (pPlayer)
		{
			result = pPlayer->m_muted;

			ICryVoice *pCryVoice = gEnv->pNetwork->GetLobby()->GetVoice();
			uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();
			if (pCryVoice != NULL && pCryVoice->IsMutedExternally(userIndex, userId))
			{
				result |= SSessionNames::SSessionName::MUTE_REASON_MANUAL;
			}
		}
	}

	return result;
}

//---------------------------------------
ELobbyVOIPState CGameLobby::GetVoiceState(int channelId)
{
	ELobbyVOIPState voiceState = eLVS_off;

	CryUserID playerId = GetUserIDFromChannelID(channelId);
	if (playerId.IsValid())
	{
		uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();

		int muteReason = GetPlayerMuteReason(playerId);
		if (muteReason & SSessionNames::SSessionName::MUTE_REASON_WRONG_TEAM)
		{
			voiceState = eLVS_mutedWrongTeam;
		}
		else if (muteReason != 0)
		{
			voiceState = eLVS_muted;
		}
		else
		{
			ICryVoice *pCryVoice = gEnv->pNetwork->GetLobby()->GetVoice();
			if (pCryVoice != NULL && pCryVoice->IsMicrophoneConnected(playerId))
			{
				voiceState = eLVS_on;

				if (pCryVoice->IsSpeaking(userIndex, playerId))
				{
					voiceState = eLVS_speaking;
				}
			}
		}
	}

	return voiceState;
}

//---------------------------------------
void CGameLobby::CycleAutomaticMuting()
{
	int newType = m_autoVOIPMutingType + 1;
	if (newType == (int)eLAVT_end)
	{
		newType = (int)eLAVT_start;
	}

	SetAutomaticMutingState((ELobbyAutomaticVOIPType)newType);
}

void CGameLobby::SetAutomaticMutingStateForPlayer(SSessionNames::SSessionName *pPlayer, ELobbyAutomaticVOIPType newType)
{
	if (!pPlayer)
		return;

	switch (newType)
	{
	case eLAVT_off:
		{
			MutePlayerBySessionName(pPlayer, false, (SSessionNames::SSessionName::MUTE_PLAYER_AUTOMUTE_REASONS));
#if !defined(_RELEASE)
			if(s_pGameLobbyCVars->gl_voip_debug)
			{
				CryLog("Trying to un-mute %s Lobby: %p", pPlayer->m_name, this);
			}
#endif
		}
		break;
	case eLAVT_allButParty:
		{
			if (g_pGame->GetSquadManager() && g_pGame->GetSquadManager()->IsSquadMateByUserId(pPlayer->m_userId) == false)
			{
				MutePlayerBySessionName(pPlayer, true, SSessionNames::SSessionName::MUTE_REASON_NOT_IN_SQUAD);
#if !defined(_RELEASE)
				if(s_pGameLobbyCVars->gl_voip_debug)
				{
					CryLog("Trying to mute %s Lobby: %p", pPlayer->m_name, this);
				}
#endif
			}
			MutePlayerBySessionName(pPlayer, false, SSessionNames::SSessionName::MUTE_REASON_MUTE_ALL);
		}
		break;
	case eLAVT_all:
		{
			MutePlayerBySessionName(pPlayer, true, SSessionNames::SSessionName::MUTE_REASON_MUTE_ALL);
			MutePlayerBySessionName(pPlayer, false, SSessionNames::SSessionName::MUTE_REASON_NOT_IN_SQUAD);
#if !defined(_RELEASE)
			if(s_pGameLobbyCVars->gl_voip_debug)
			{
				CryLog("Trying to mute %s Lobby: %p", pPlayer->m_name, this);
			}
#endif
		}
		break;
	default:
		CRY_ASSERT_MESSAGE(0, "Unknown automatic voice muting type");
		break;
	}

}

void CGameLobby::SetAutomaticMutingState(ELobbyAutomaticVOIPType newType)
{
#if !defined(_RELEASE)
	if(s_pGameLobbyCVars->gl_voip_debug)
	{
		CryLog("SetAutomaticMutingState %d -> %d Lobby: %p", (int)m_autoVOIPMutingType, (int)newType, this);

		switch (newType)
		{
		case eLAVT_off:					CryLog("Automatic muting off Lobby: %p", this);												break;
		case eLAVT_allButParty:	CryLog("Automatic muting all but party Lobby: %p", this);							break;
		case eLAVT_all:					CryLog("Automatic muting all Lobby: %p", this);												break;
		default:								CryLog("Unknown automatic muting %d Lobby: %p", (int)newType, this);	break;
		};
	}
#endif

	m_autoVOIPMutingType = (ELobbyAutomaticVOIPType)newType;

	const int numPlayers = m_nameList.Size();

	for (int i = 0; i < numPlayers; ++ i)
	{
		SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[i];
		if(pPlayer)
		{
			CryUserID localUser = gEnv->pNetwork->GetLobby()->GetLobbyService()->GetUserID(g_pGame->GetExclusiveControllerDeviceIndex());
			const bool bIsLocalUser = localUser.IsValid() && (localUser == pPlayer->m_userId);
			if (!bIsLocalUser)
			{
				SetAutomaticMutingStateForPlayer(pPlayer, m_autoVOIPMutingType);
			}
		}
	}
}

//---------------------------------------
void CGameLobby::MatchmakingSessionStartCallback( CryLobbyTaskID taskID, ECryLobbyError error, void* pArg )
{
	ENSURE_ON_MAIN_THREAD;

	CryLog("[GameLobby] MatchmakingSessionStartCallback %s %d Lobby: %p", error == eCLE_Success ? "succeeded with error" : "failed with error", error, pArg);
	INDENT_LOG_DURING_SCOPE();

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);
	if (pGameLobby->NetworkCallbackReceived(taskID, error))
	{
		pGameLobby->m_bSessionStarted = true;

		// Tell the IGameSessionHandler
		g_pGame->GetIGameFramework()->GetIGameSessionHandler()->StartSession();

		pGameLobby->SetState(eLS_PreGame);

		CSquadManager *pSquadManager = g_pGame->GetSquadManager();
		if (pSquadManager)
		{
			pSquadManager->OnGameSessionStarted();
		}
	}
	else if (error != eCLE_TimeOut)
	{
		pGameLobby->SessionStartFailed(error);
	}
}

void CGameLobby::SessionEndCleanup()
{
	m_bSessionStarted = false;

	// Tell the IGameSessionHandler
	g_pGame->GetIGameFramework()->GetIGameSessionHandler()->EndSession();

	if (m_state != eLS_Leaving)
	{
		if(m_server)
		{
			SetState(eLS_GameEnded);
		}
		else
		{
			if(!IsQuitting())
			{
				//-- remove the loading screen.
#if 0 // old frontend
				CFlashFrontEnd *pFFE = g_pGame->GetFlashMenu();
				if(pFFE != NULL && pFFE->IsFlashRenderingDuringLoad())
				{
					pFFE->StopFlashRenderingDuringLoading();
				}
#endif

				//-- enable action mapping again, loading screen will have disabled it
				g_pGame->GetIGameFramework()->GetIActionMapManager()->Enable(true);

				gEnv->pConsole->ExecuteString("unload", false, true);
				SetState(eLS_PostGame);
				SendPacket(eGUPD_LobbyEndGameResponse);
			}
			else
			{
				CryLog("  delaying unload as we are quitting");
			}
		}
	}

	CSquadManager *pSquadManager = g_pGame->GetSquadManager();
	if (pSquadManager)
	{
		pSquadManager->OnGameSessionEnded();
	}
}

//---------------------------------------
void CGameLobby::MatchmakingSessionEndCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg)
{
	ENSURE_ON_MAIN_THREAD;

	CryLog("[GameLobby] MatchmakingSessionEndCallback %s %d Lobby: %p", error == eCLE_Success ? "succeeded with error" : "failed with error", error, pArg);

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);
	if (pGameLobby->NetworkCallbackReceived(taskID, error))
	{
		pGameLobby->SessionEndCleanup();
	}
	else if (error != eCLE_TimeOut)
	{
		pGameLobby->SessionEndFailed(error);
	}
}

//---------------------------------------
void CGameLobby::SessionStartFailed(ECryLobbyError error)
{
	CryLog ("Session start failed (error=%d)", error);
	INDENT_LOG_DURING_SCOPE();

	LeaveSession(true,false);
	CFrontEndModelCache::Allow3dFrontEndAssets(true, true);
}

//---------------------------------------
void CGameLobby::SessionEndFailed(ECryLobbyError error)
{
	if(m_gameLobbyMgr->IsPrimarySession(this))
	{
		CErrorHandling *pErrorHandling = CErrorHandling::GetInstance();
		if (pErrorHandling)
		{
			pErrorHandling->OnNonFatalError(CErrorHandling::eNFE_SessionEndFailed, true);
		}
	}

	// Failed to do a session end - this is a disconnect case so we need to reset the flags (and do the same for the squad)
	m_bSessionStarted = false;
	CSquadManager *pSquadManager = g_pGame->GetSquadManager();
	if (pSquadManager)
	{
		pSquadManager->OnGameSessionEnded();
	}
}

//-------------------------------------------------------------------------
IHostMigrationEventListener::EHostMigrationReturn CGameLobby::OnInitiate( SHostMigrationInfo& hostMigrationInfo, HMStateType& state )
{
	if (hostMigrationInfo.m_session == m_currentSession)
	{
		if (m_gameLobbyMgr->IsNewSession(this))
		{
			ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
			if (pLobby)
			{
				ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
				if (pMatchmaking)
				{
					pMatchmaking->TerminateHostMigration(m_currentSession);
				}
			}
		}

		if (m_server && m_bMatchmakingSession)
		{
			CryLog("CGameLobby::OnInitiate(), aborting lobby merging and searches, lobby: %p", this);

			FindGameCancelMove();

			if (m_gameLobbyMgr->IsPrimarySession(this))
			{
				CGameBrowser *pGameBrowser = g_pGame->GetGameBrowser();
				if (pGameBrowser)
				{
					pGameBrowser->CancelSearching();
				}
			}
		}

		if(m_taskQueue.HasTaskInProgress())
		{
			CLobbyTaskQueue::ESessionTask currentTask = m_taskQueue.GetCurrentTask();

			if(currentTask == CLobbyTaskQueue::eST_Query)
			{
				CryLog("has query task in progress, cancelling...");
				CancelLobbyTask(currentTask);
			}
		}

#if defined(TRACK_MATCHMAKING)
		if( m_bMatchmakingSession )
		{
			if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
			{
				pMMTel->AddEvent( SMMMigrateHostLobbyEvent() );
			}
		}
#endif
#ifndef _RELEASE
	m_migrationStarted = true;
#endif
	}

	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	if (GetState()!=eLS_Game)
	{
		CryLogAlways("[Host Migration][GameLobby]Not in game state - unable to proceed with host migration!");
		return IHostMigrationEventListener::Listener_Terminate;
	}

	return IHostMigrationEventListener::Listener_Done;
}

//---------------------------------------
IHostMigrationEventListener::EHostMigrationReturn CGameLobby::OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (hostMigrationInfo.m_session == m_currentSession)
	{
#if defined(TRACK_MATCHMAKING)
		if( m_server )
		{
			//we were the server and we have been made a client;
			if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
			{
				pMMTel->AddEvent( SMMDemotedToClientEvent() );
			}
		}
#endif

		m_server = false;
		m_connectedToDedicatedServer = false;
		CryLog("CGameLobby::OnDemoteToClient() isNewServer=false Lobby: %p", this);
	}

	return IHostMigrationEventListener::Listener_Done;
}

//---------------------------------------
IHostMigrationEventListener::EHostMigrationReturn CGameLobby::OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (hostMigrationInfo.m_session == m_currentSession)
	{
		m_server = true;
		m_connectedToDedicatedServer = false;
		CryLog("CGameLobby::OnPromoteToServer() isNewServer=true Lobby: %p", this);

		// Mark all connections as fully connected
		SSessionNames &names = m_nameList;
		// Remove all placeholder entries, if we don't know about the player at this point, they aren't
		// going to manage the migration
		names.RemoveBlankEntries();
		const int numNames = names.Size();
		for (int i = 0; i < numNames; ++ i)
		{
			names.m_sessionNames[i].m_bFullyConnected = true;
		}

		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			pPlaylistManager->OnPromoteToServer();
		}

		m_bHasReceivedVoteOptions = false;

#if defined(TRACK_MATCHMAKING)
		if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
		{
			pMMTel->AddEvent( SMMBecomeHostEvent() );
		}
#endif
	}

	return IHostMigrationEventListener::Listener_Done;
}

//---------------------------------------
IHostMigrationEventListener::EHostMigrationReturn CGameLobby::OnFinalise( SHostMigrationInfo& hostMigrationInfo, HMStateType& state )
{
	if (hostMigrationInfo.m_session != m_currentSession)
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	const bool isNewServer = hostMigrationInfo.IsNewHost();
	CryLog("CGameLobby::OnFinalise() isNewServer=%s Lobby: %p", isNewServer ? "true" : "false", this);
	CryLog("  isPrimarySession=%s, numPlayers=%i", m_gameLobbyMgr->IsPrimarySession(this) ? "true" : "false", m_nameList.Size());

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby)
	{
		ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
		if (pMatchmaking)
		{
			CrySessionID sessionId = pMatchmaking->SessionGetCrySessionIDFromCrySessionHandle(m_currentSession);
			SetCurrentId(sessionId, true, true);
			InformSquadManagerOfSessionId();
		}
	}

	if (isNewServer)
	{
		m_sessionHostOnly = (s_pGameLobbyCVars->gl_getServerFromDedicatedServerArbitrator !=  0);
		m_bAllocatedServer = (m_dedicatedserverip != 0);

		ICryMatchMaking *pMatchMaking = gEnv->pLobby->GetMatchMaking();
		m_allocatedServerUID = pMatchMaking->GetHostConnectionUID(m_currentSession);
		m_allocatedServerUID.m_uid = 0;

		if (m_bMatchmakingSession)
		{
			CryLog("  this is a matchmaking session so setting m_shouldFindGames = TRUE");
			CGameLobby::s_bShouldBeSearching = true;
			m_shouldFindGames = true;
		}

		if (m_bSessionStarted)
		{
			SendPacket(eGUPD_LobbyGameHasStarted);
		}

		m_lastActiveStatus = GetActiveStatus(m_state);
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_Migrate, false);
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);

#if !defined(_RELEASE)
		if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
		{
			gEnv->pConsole->GetCVar("net_hostHintingActiveConnectionsOverride")->Set(1);
			CryLog("  setting net_hostHintingActiveConnectionsOverride to 1");
			m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_debugForceLobbyMigrationsTimer;
		}
#endif
	}
#if !defined(_RELEASE)
	else
	{
		if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
		{
			int numConnections = (int) (gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64() % 127);
			numConnections = numConnections + 2;
			CryLog("  setting net_hostHintingActiveConnectionsOverride to %i", numConnections);
			gEnv->pConsole->GetCVar("net_hostHintingActiveConnectionsOverride")->Set(numConnections);
		}
	}
#endif

	if( m_bMatchmakingSession )
	{
		if( CMatchMakingHandler* pMatchmakingHandler = m_gameLobbyMgr->GetMatchMakingHandler() )
		{
			pMatchmakingHandler->OnHostMigrationFinished( true, isNewServer );
		}
	}

	if (m_state == eLS_Lobby)
	{
#if 0 // old frontend
		CFlashFrontEnd *pFrontEnd = g_pGame->GetFlashMenu();
		if (pFrontEnd)
		{
			pFrontEnd->RefreshCurrentScreen();
		}
#endif
	}

#if defined(TRACK_MATCHMAKING)
	if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
	{
		ICryLobbyService* pLobbyService = pLobby ? pLobby->GetLobbyService( eCLS_Online ) : NULL;
		ICryMatchMaking* pMatchMaking = pLobbyService ? pLobbyService->GetMatchMaking(): NULL;

		SCryMatchMakingConnectionUID conUID = pMatchMaking ? pMatchMaking->GetHostConnectionUID( hostMigrationInfo.m_session ) : SCryMatchMakingConnectionUID();

		CryFixedStringT<DISPLAY_NAME_LENGTH> displayName;

		SSessionNames::SSessionName *pSessionName = m_nameList.GetSessionName( conUID, false );
		if( pSessionName )
		{
			pSessionName->GetDisplayName( displayName );
		}

		pMMTel->AddEvent( SMMMigrateCompletedEvent( displayName, m_currentSessionId ) );
	}
#endif //defined(TRACK_MATCHMAKING)

	return IHostMigrationEventListener::Listener_Done;
}

//---------------------------------------
bool CGameLobby::OnWarningReturn(THUDWarningId id, const char* returnValue)
{
	if (id == m_DLCServerStartWarningId)
	{
		if(stricmp(returnValue, "yes")==0)
		{
			// Start the server
			m_bSkipCountdown = true;
		}
	}
	else if(id == g_pGame->GetWarnings()->GetWarningId("ServerListPassword"))
	{
		if (returnValue && returnValue[0] && strcmp(returnValue, "*cancel*"))
		{
			ICVar *pCVar = gEnv->pConsole->GetCVar("sv_password");
			if (pCVar)
			{
				pCVar->Set(returnValue);
			}

			JoinServer(m_pendingConnectSessionId, NULL, CryMatchMakingInvalidConnectionUID, true);
		}
	}
	else if (id == g_pGame->GetWarnings()->GetWarningId("NoServersFound"))
	{
		if (returnValue)
		{
			if (!stricmp(returnValue, "retry"))
			{
				CryLog("retrying");
				StartFindGame();
			}
			else if (!stricmp(returnValue, "cancel"))
			{
#if 0 // old frontend
				CMPMenuHub *pMPMenuHub = CMPMenuHub::GetMPMenuHub();
				if (pMPMenuHub)
				{
					pMPMenuHub->GoToCurrentLobbyServiceScreen();
				}
#endif
			}
		}
	}
	return true;
}

//---------------------------------------
void CGameLobby::OnWarningRemoved(THUDWarningId id)
{
}

//---------------------------------------
void CGameLobby::SwitchToPrimaryLobby()
{
	CRY_ASSERT(m_gameLobbyMgr->IsPrimarySession(this));
	g_pGame->GetSquadManager()->GameSessionIdChanged(CSquadManager::eGSC_LobbyMerged, m_currentSessionId);
}

//---------------------------------------
void CGameLobby::OnOptionsChanged()
{
	CRY_ASSERT(m_gameLobbyMgr->IsPrimarySession(this));
	if (m_server && (m_state != eLS_Leaving))
	{
#if 0 // LEVEL ROTATION DISABLED FOR NOW
		SendPacket(eGUPD_SetGameVariant);
#endif
		m_nameList.m_dirty = true;		// Options may have changed the names list, need to update it
	}
}

//---------------------------------------
void CGameLobby::SetLobbyService(ECryLobbyService lobbyService)
{
	if ((lobbyService != eCLS_Online) && (lobbyService != eCLS_LAN))
	{
		GameWarning("Unknown lobby service %d expecting eCLS_Online or eCLS_LAN", (int)(lobbyService));
		return;
	}

	const bool allowJoinMultipleSessions = gEnv->pNetwork->GetLobby()->GetLobbyServiceFlag( lobbyService, eCLSF_AllowMultipleSessions );
	g_pGame->GetSquadManager()->Enable( allowJoinMultipleSessions, true );
	gEnv->pNetwork->GetLobby()->SetLobbyService(lobbyService);
}

//---------------------------------------
void CGameLobby::UpdateVotingInfoFlashInfo()
{
	CryLog("CGameLobby::UpdateVotingInfoFlashInfo()");

#ifndef _RELEASE
	m_votingFlashInfo.tmpWatchInfoIsSet = false;
#endif

	if (m_votingEnabled)
	{
		m_votingFlashInfo.leftNumVotes = m_leftVoteChoice.m_numVotes;
		m_votingFlashInfo.rightNumVotes = m_rightVoteChoice.m_numVotes;

		m_votingFlashInfo.votingClosed = m_votingClosed;
		m_votingFlashInfo.votingDrawn = (m_leftVoteChoice.m_numVotes == m_rightVoteChoice.m_numVotes);
		m_votingFlashInfo.leftWins = m_leftWinsVotes;

		m_votingFlashInfo.localHasCandidates = (m_localVoteStatus != eLVS_awaitingCandidates);
		m_votingFlashInfo.localHasVoted = ((m_localVoteStatus == eLVS_votedLeft) || (m_localVoteStatus == eLVS_votedRight));
		m_votingFlashInfo.localVotedLeft = (m_localVoteStatus == eLVS_votedLeft);


		// Vote status message
		m_votingFlashInfo.votingStatusMessage.clear();
		if (m_votingFlashInfo.localHasCandidates)
		{
			if (m_votingFlashInfo.votingClosed)
			{
				const char* winningMapName = m_votingFlashInfo.leftWins ? m_votingCandidatesFlashInfo.leftLevelName.c_str() : m_votingCandidatesFlashInfo.rightLevelName.c_str();
				if (m_votingFlashInfo.votingDrawn)
				{
					m_votingFlashInfo.votingStatusMessage = CHUDUtils::LocalizeString("@ui_menu_gamelobby_vote_draw_winner", winningMapName);
				}
				else
				{
					m_votingFlashInfo.votingStatusMessage = CHUDUtils::LocalizeString("@ui_menu_gamelobby_vote_winner", winningMapName);
				}
			}
			else
			{
				if (m_votingFlashInfo.localHasVoted)
				{
					const char* votedMapName = m_votingFlashInfo.localVotedLeft ? m_votingCandidatesFlashInfo.leftLevelName.c_str() : m_votingCandidatesFlashInfo.rightLevelName.c_str();
					m_votingFlashInfo.votingStatusMessage = CHUDUtils::LocalizeString("@ui_menu_gamelobby_voted", votedMapName);
				}
				else
				{
					m_votingFlashInfo.votingStatusMessage = CHUDUtils::LocalizeString("@ui_menu_gamelobby_vote_now");
				}
			}
		}

		m_sessionUserDataDirty = true;

#ifndef _RELEASE
		m_votingFlashInfo.tmpWatchInfoIsSet = true;
#endif
	}
}

//---------------------------------------
void CGameLobby::UpdateVotingCandidatesFlashInfo()
{
	CryLog("CGameLobby::UpdateVotingCandidatesFlashInfo()");

#ifndef _RELEASE
	m_votingCandidatesFlashInfo.tmpWatchInfoIsSet = false;
#endif

	// Collect left choice info
	m_votingCandidatesFlashInfo.leftLevelMapPath = m_leftVoteChoice.m_levelName.c_str();
	m_votingCandidatesFlashInfo.leftRulesName.Format("@ui_rules_%s", m_leftVoteChoice.m_gameRules.c_str());
	GetMapImageName(m_votingCandidatesFlashInfo.leftLevelMapPath, &m_votingCandidatesFlashInfo.leftLevelImage);

	m_votingCandidatesFlashInfo.leftLevelName = PathUtil::GetFileName(m_votingCandidatesFlashInfo.leftLevelMapPath.c_str()).c_str();
	m_votingCandidatesFlashInfo.leftLevelName = g_pGame->GetMappedLevelName(m_votingCandidatesFlashInfo.leftLevelName.c_str());

	// Collect right choice info
	m_votingCandidatesFlashInfo.rightLevelMapPath = m_rightVoteChoice.m_levelName.c_str();
	m_votingCandidatesFlashInfo.rightRulesName.Format("@ui_rules_%s", m_rightVoteChoice.m_gameRules.c_str());
	GetMapImageName(m_votingCandidatesFlashInfo.rightLevelMapPath, &m_votingCandidatesFlashInfo.rightLevelImage);

	m_votingCandidatesFlashInfo.rightLevelName = PathUtil::GetFileName(m_votingCandidatesFlashInfo.rightLevelMapPath.c_str()).c_str();
	m_votingCandidatesFlashInfo.rightLevelName = g_pGame->GetMappedLevelName(m_votingCandidatesFlashInfo.rightLevelName.c_str());

	m_sessionUserDataDirty = true;

#ifndef _RELEASE
	m_votingCandidatesFlashInfo.tmpWatchInfoIsSet = true;
#endif
}

//---------------------------------------
void CGameLobby::SetLobbyTaskId( CryLobbyTaskID taskId )
{
	CryLog("CGameLobby::SetLobbyTaskId() this=%p, taskId=%u", this, taskId);
	m_currentTaskId = taskId;
}

//---------------------------------------
void CGameLobby::SetMatchmakingGame( bool bMatchmakingGame )
{
	CryLog("CGameLobby::SetMatchmakingGame() this=%p, bMatchmakingGame=%s", this, bMatchmakingGame ? "TRUE" : "FALSE");
	m_bMatchmakingSession = bMatchmakingGame;
}

//---------------------------------------
void CGameLobby::LeaveAfterSquadMembers()
{
	CryLog("CGameLobby::LeaveAfterSquadMembers()");

	eHostMigrationState hostMigrationState = eHMS_Unknown;
	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby)
	{
		ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
		if (pMatchmaking)
		{
			hostMigrationState = pMatchmaking->GetSessionHostMigrationState(m_currentSession);
		}
	}

	if (hostMigrationState != eHMS_Idle)
	{
		// Currently migrating, don't know if we should wait or not so just leave
		LeaveSession(true, false);
		return;
	}

	CSquadManager *pSquadManager = g_pGame->GetSquadManager();
	bool bFoundSquadMembers = false;
	if (pSquadManager)
	{
		SSessionNames &playerList = m_nameList;

		const int numPlayers = playerList.Size();
		// Start at 1 since we don't include ourselves
		for (int i = 1; i < numPlayers; ++ i)
		{
			SSessionNames::SSessionName &player = playerList.m_sessionNames[i];
			if (pSquadManager->IsSquadMateByUserId(player.m_userId))
			{
				player.m_bMustLeaveBeforeServer = true;
				bFoundSquadMembers = true;
			}
		}
	}
	if (bFoundSquadMembers)
	{
		CryLog("  found squad members in the lobby, start terminate host hinting task");
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_TerminateHostHinting, false);
	}
	else
	{
		CryLog("  failed to find squad members in the lobby, leave the squad");
		LeaveSession(true, false);
	}
}

//---------------------------------------
void CGameLobby::InformSquadManagerOfSessionId()
{
	if(m_gameLobbyMgr->IsPrimarySession(this) && (!m_gameLobbyMgr->IsLobbyMerging()))
	{
		CSquadManager *pSquadManager = g_pGame->GetSquadManager();
		if (m_currentSessionId == CrySessionInvalidID)
		{
			pSquadManager->GameSessionIdChanged(CSquadManager::eGSC_LeftSession, m_currentSessionId);
		}
		else
		{
			if (m_bMigratedSession)
			{
				pSquadManager->GameSessionIdChanged(CSquadManager::eGSC_LobbyMigrated, m_currentSessionId);
			}
			else
			{
				pSquadManager->GameSessionIdChanged(CSquadManager::eGSC_JoinedNewSession, m_currentSessionId);
			}
		}
	}
}

//---------------------------------------
// [static]
THUDWarningId CGameLobby::ShowErrorDialog(const ECryLobbyError error, const char* pDialogName, const char* pDialogParam, IGameWarningsListener* pWarningsListener)
{
	CryLog("CGameLobby::ShowErrorDialog(), error=%d, pDialogName='%s', pDialogParam='%s' pWarningsListener=0x%p", error, pDialogName, pDialogParam, pWarningsListener);
	if (!gEnv->bMultiplayer)
	{
		CryLog("  in SP, ignoring error");
		return 0;
	}

	CRY_ASSERT_MESSAGE((!pDialogParam || pDialogName), "A custom dialog param should only be provided if a custom dialog name is also provided.");

	CryFixedStringT<32>  name;
	CryFixedStringT<32>  param;

	bool handled = false;

	switch (error)
	{
	case eCLE_InternalError:
		{
			if (pDialogName)
			{
				name = pDialogName;

				if (pDialogParam)
				{
					param = pDialogParam;
				}
				else
				{
					param.Format("@ui_menu_lobby_internal_error");
				}
			}
			else
			{
				name = "LobbyInternalError";
			}

			break;
		}
	default:
		{
			bool  generateParam = false;

			if (pDialogName)
			{
				name = pDialogName;

				if (pDialogParam)
				{
					param = pDialogParam;
				}
				else
				{
					generateParam = true;
				}
			}
			else
			{
				name = "LobbyError";
				generateParam = true;
			}

			if (generateParam)
			{
				param.Format("@ui_menu_lobby_error_%d", (int)error);
			}

			break;
		}
	}

	if (handled)
		CryLog(" > handled warning (name='%s', param='%s'), no need to show now.", name.c_str(), param.c_str());
	else
		CryLog(" > trying to show warning (name='%s', param='%s')", name.c_str(), param.c_str());

	bool showError = !handled;

	if(!CGameLobby::IsSignInError(error))
	{
		CGameLobbyManager *pGameLobbyManager = g_pGame->GetGameLobbyManager();
		showError = !pGameLobbyManager || pGameLobbyManager->IsCableConnected(); // if cable is not connected, far worse has happened
	}

#ifndef _RELEASE
	// Ignore errors when doing autotesting
	ICVar* pAutoTest = gEnv->pConsole->GetCVar("autotest_enabled");
	if(pAutoTest != NULL && pAutoTest->GetIVal())
	{
		showError = false;
	}
#endif

	if(showError)
	{
		CryLog("   > showing warning directly though Warning Manager");
		g_pGame->AddGameWarning(name, param, pWarningsListener);
	}

	return g_pGame->GetWarnings()->GetWarningId(name.c_str());
}

bool CGameLobby::IsSignInError(ECryLobbyError error)
{
	bool isSignInError = false;

	switch(error)
	{
		case eCLE_UserNotSignedIn:
		case eCLE_NoOnlineAccount:
		case eCLE_InsufficientPrivileges:
			isSignInError = true;
			break;

		default:
			break;
	}

	return isSignInError;
}

//----------------------------------------------------------------------
void CGameLobby::LeaveSession(bool clearPendingTasks, bool bWasUserInitiated)
{
  CryLog("CGameLobby::LeaveSession() lobby:%p cancelling %d clearPendingTasks %d, wasUserInitiated %d", this, m_bCancelling, clearPendingTasks, bWasUserInitiated);
  INDENT_LOG_DURING_SCOPE();

	if(m_bCancelling)
	{
	  // need to make sure that any tasks added to the list since entering
	  // the new session are removed, the only ones we want left are the task in progress
	  // and the session delete
	  CancelSessionInit();
		return;
	}

	ICVar *pCVar = gEnv->pConsole->GetCVar("sv_password");
	if (pCVar != NULL && ((pCVar->GetFlags() & VF_WASINCONFIG) == 0))
	{
		pCVar->Set("");
	}

	IGameFramework *pFramework = g_pGame->GetIGameFramework();
	if (pFramework->StartedGameContext() || pFramework->StartingGameContext())
	{
		gEnv->pConsole->ExecuteString("unload", false, true);
		gEnv->pConsole->ExecuteString("disconnectchannel", false, true);

		if (!gEnv->IsDedicated())
		{
			AbortLoading();
		}

		TerminateHostMigration();
	}

	if ((m_currentSession != CrySessionInvalidHandle) && (m_state != eLS_Leaving))
	{
		if (clearPendingTasks)
		{
			CancelAllLobbyTasks();
		}

#if ENABLE_CHAT_MESSAGES
		ClearChatMessages();

#if 0 // old frontend
		CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
		if (pFlashMenu && pFlashMenu->IsMenuActive(eFlM_Menu))
		{
			if (IFlashPlayer *pFlashPlayer = pFlashMenu->GetFlash())
			{
				EFlashFrontEndScreens screen = pFlashMenu->GetCurrentMenuScreen();
				if (screen == eFFES_game_lobby)
				{
					pFlashPlayer->Invoke0(FRONTEND_SUBSCREEN_PATH_SET("ClearChatMessages"));
				}
			}
		}
#endif
#endif

		// Clear last match results display
#if 0 // old frontend
		CFlashFrontEnd *flashMenu = g_pGame->GetFlashMenu();
		CMPMenuHub *mpMenuHub = flashMenu ? flashMenu->GetMPMenu() : NULL;
		CUIScoreboard *scoreboard = mpMenuHub ? mpMenuHub->GetScoreboardUI() : NULL;
		if (scoreboard)
		{
			scoreboard->ClearLastMatchResults();
		}
#endif
		CryLog("  setting state eLS_Leaving");
		SetState(eLS_Leaving);
	}
	else if (m_state == eLS_Leaving)
	{
		CryLog("  already in eLS_Leaving state");
		m_taskQueue.ClearNonVitalTasks();
	}
	else if (m_state == eLS_None)
	{
		CryLog("  already in eLS_None state");
	}
	else if (m_state == eLS_Initializing)
	{
		CryLog("  currently initializing, cancel session init");
		CancelSessionInit();
	}
	else
	{
		CRY_ASSERT(m_state == eLS_FindGame);
		CryLog("  not in a session but we're not in eLS_Leaving or eLS_None states, state=%u", m_state);
		CancelAllLobbyTasks();
		SetState(eLS_None);
	}

	NOTIFY_UILOBBY_MP(HideLoadingDialog("JoinSession"));

#if defined(DEDICATED_SERVER)
	if (m_gameLobbyMgr->IsPrimarySession(this) && !bWasUserInitiated)
	{
		CryLogAlways("We've lost our session, bailing");
		gEnv->pConsole->ExecuteString("quit", false, true);
	}
#endif
}

//----------------------------------------------------------------------
void CGameLobby::StartFindGame()
{
	if (m_state != eLS_None)
	{
		LeaveSession(false, true);
	}
	else
	{
		if( m_bMatchmakingSession )
		{
			// Tell the playlist activity tracker we've joined the playlist
			if( CPlaylistActivityTracker* pTracker = g_pGame->GetPlaylistActivityTracker() )
			{
				pTracker->SetState( CPlaylistActivityTracker::eATS_OnPlaylist );
			}
		}
	}
	m_taskQueue.AddTask(CLobbyTaskQueue::eST_FindGame, true);

	// new quick match, starting from a clean slate
	// so clear the bad servers list too
	ClearBadServers();

	CPersistantStats *pStats = g_pGame->GetPersistantStats();
	if (pStats)
	{
		pStats->OnEnterFindGame();
	}
}

//----------------------------------------------------------------------
void CGameLobby::TaskStartedCallback( CLobbyTaskQueue::ESessionTask task, void *pArg )
{
	ENSURE_ON_MAIN_THREAD;

	CryLog("CGameLobby::TaskStartedCallback() task=%u, lobby=%p", task, pArg);
	INDENT_LOG_DURING_SCOPE();

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);
	CRY_ASSERT(pGameLobby);
	if (pGameLobby)
	{
		CRY_ASSERT(pGameLobby->m_currentTaskId == CryLobbyInvalidTaskID);

		ECryLobbyError result = eCLE_ServiceNotSupported;
		CryLobbyTaskID taskId = CryLobbyInvalidTaskID;
		bool bMatchMakingTaskStarted = false;
		bool bRestartTask = false;

		ICryLobby *pCryLobby = gEnv->pNetwork->GetLobby();
		if (pCryLobby)
		{
			ICryMatchMaking *pMatchMaking = pCryLobby->GetMatchMaking();
			if (pMatchMaking)
			{
				switch (task)
				{
				case CLobbyTaskQueue::eST_Create:
					{
						result = pGameLobby->DoCreateSession(pMatchMaking, taskId);
						bMatchMakingTaskStarted = true;
					}
					break;
				case CLobbyTaskQueue::eST_Migrate:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoMigrateSession(pMatchMaking, taskId);
							bMatchMakingTaskStarted = true;
						}
					}
					break;
				case CLobbyTaskQueue::eST_Join:
					{
						if (pGameLobby->m_pendingConnectSessionId != CrySessionInvalidID)
						{
							bool bAlreadyInSession = GameNetworkUtils::CompareCrySessionId(pGameLobby->m_currentSessionId, pGameLobby->m_pendingConnectSessionId);
							if (bAlreadyInSession == false)
							{
								result = pGameLobby->DoJoinServer(pMatchMaking, taskId);
								bMatchMakingTaskStarted = true;
							}
							else
							{
								if( pGameLobby->m_bMatchmakingSession )
								{
									if( CMatchMakingHandler* pMMHandler = pGameLobby->m_gameLobbyMgr->GetMatchMakingHandler() )
									{
										pMMHandler->GameLobbyJoinFinished( eCLE_AlreadyInSession );
									}
								}
								CryLog("  task not started, already in correct session");
							}
						}
						else
						{
							if( pGameLobby->m_bMatchmakingSession )
							{
								if( CMatchMakingHandler* pMMHandler = pGameLobby->m_gameLobbyMgr->GetMatchMakingHandler() )
								{
									pMMHandler->GameLobbyJoinFinished( eCLE_InvalidParam );
								}
							}
							CryLog("  task not started, invalid target sessionId");
						}
					}
					break;
				case CLobbyTaskQueue::eST_Delete:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoDeleteSession(pMatchMaking, taskId);
							bMatchMakingTaskStarted = true;
						}
						else
						{
							if (pGameLobby->m_bCancelling)
							{
								CryLog("CGameLobby::eST_Delete no session, resetting m_bCancelling");
								pGameLobby->m_bCancelling = false;
							}
						}
					}
					break;
				case CLobbyTaskQueue::eST_SetLocalUserData:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoUpdateLocalUserData(pMatchMaking, taskId);
							bMatchMakingTaskStarted = true;
						}
					}
					break;
				case CLobbyTaskQueue::eST_SessionStart:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoStartSession(pMatchMaking, taskId, bMatchMakingTaskStarted);
						}
					}
					break;
				case CLobbyTaskQueue::eST_SessionEnd:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoEndSession(pMatchMaking, taskId);
							bMatchMakingTaskStarted = true;
						}
					}
					break;
				case CLobbyTaskQueue::eST_Query:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoQuerySession(pMatchMaking, taskId);
							bMatchMakingTaskStarted = true;
						}
					}
					break;
				case CLobbyTaskQueue::eST_Update:
					{
						if (pGameLobby->m_server && (pGameLobby->m_currentSession != CrySessionInvalidHandle))
						{
							eHostMigrationState hostMigrationState = eHMS_Unknown;
							ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
							if (pLobby)
							{
								ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
								if (pMatchmaking)
								{
									hostMigrationState = pMatchmaking->GetSessionHostMigrationState(pGameLobby->m_currentSession);
								}
							}

							if (hostMigrationState == eHMS_Idle)
							{
								result = pGameLobby->DoUpdateSession(pMatchMaking, taskId);
								bMatchMakingTaskStarted = true;
							}
							else if (hostMigrationState == eHMS_Finalise)
							{
								// We add an Update task during OnFinalise callback but we can't run it until the migration has
								// fully completed, delay the start until we're ready
								CryLog("  can't do eST_Update yet as we're mid-migration");
								bRestartTask = true;
							}
						}
					}
					break;
				case CLobbyTaskQueue::eST_EnsureBestHost:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							if (pGameLobby->ShouldCheckForBestHost())
							{
								result = pGameLobby->DoEnsureBestHost(pMatchMaking, taskId);
								bMatchMakingTaskStarted = true;
							}
#if !defined(_RELEASE)
							else
							{
								if (s_pGameLobbyCVars->gl_debugForceLobbyMigrations)
								{
									pGameLobby->m_timeTillCallToEnsureBestHost = s_pGameLobbyCVars->gl_debugForceLobbyMigrationsTimer;
								}
							}
#endif
						}
					}
					break;
				case CLobbyTaskQueue::eST_FindGame:
					{
						pGameLobby->SetState(eLS_FindGame);
					}
					break;
				case CLobbyTaskQueue::eST_TerminateHostHinting:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoTerminateHostHintingForGroup(pMatchMaking, taskId, bMatchMakingTaskStarted);
						}
					}
					break;
				case CLobbyTaskQueue::eST_SessionSetLocalFlags:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoSessionSetLocalFlags(pMatchMaking, taskId);
							bMatchMakingTaskStarted = true;
						}
					}
					break;
				case CLobbyTaskQueue::eST_SessionRequestDetailedInfo:
					{
						pGameLobby->DoSessionDetailedInfo(pMatchMaking, taskId);
					}
					break;
				case CLobbyTaskQueue::eST_Unload:
					{
						pGameLobby->DoUnload();
					}
					break;
				case CLobbyTaskQueue::eST_SetupDedicatedServer:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoSetupDedicatedServer(pMatchMaking, taskId, bMatchMakingTaskStarted);
							bRestartTask = true;
						}
					}
					break;
				case CLobbyTaskQueue::eST_ReleaseDedicatedServer:
					{
						if(pGameLobby->m_currentSession != CrySessionInvalidHandle)
						{
							result = pGameLobby->DoReleaseDedicatedServer(pMatchMaking, taskId, bMatchMakingTaskStarted);
							bRestartTask = true;
						}
					}
					break;
				}
			}
		}

		if (bMatchMakingTaskStarted)
		{
			if (result == eCLE_Success)
			{
				pGameLobby->SetLobbyTaskId(taskId);
			}
			else if(result == eCLE_SuccessInvalidSession)
			{
				pGameLobby->m_taskQueue.TaskFinished();
			}
			else if (result == eCLE_TooManyTasks)
			{
				CryLog("  too many tasks, restarting next frame");
				pGameLobby->m_taskQueue.RestartTask();
			}
			else
			{
				ShowErrorDialog(result, NULL, NULL, NULL);
				pGameLobby->m_taskQueue.TaskFinished();
			}
		}
		else
		{
			if (bRestartTask)
			{
				pGameLobby->m_taskQueue.RestartTask();
			}
			else
			{
				// not completely convinced by this check, but in order to get here, queue size had to be at least 1 and now it's not.
				// in order for this to happen, m_taskQueue.Reset() was probably called, this can happen during the DoUnload task, which sets the lobby
				// state to none and does reset the queue as part of it's process
				if(pGameLobby->m_taskQueue.GetCurrentQueueSize() > 0)
				{
					// Either the task wasn't started (no longer valid given the lobby state) or we're not expecting a callback
					pGameLobby->m_taskQueue.TaskFinished();
				}
#if !defined(_RELEASE)
				else
				{
					CRY_ASSERT(!(pGameLobby->m_taskQueue.HasTaskInProgress()));
				}
#endif
			}
		}
	}
}

int CGameLobby::GetNumberOfExpectedClients() const
{
	const int numPlayers = m_nameList.Size();

	return numPlayers;
}

//-------------------------------------------------------------------------
bool CGameLobby::NetworkCallbackReceived( CryLobbyTaskID taskId, ECryLobbyError result )
{
	ENSURE_ON_MAIN_THREAD;

	if (result == eCLE_SuccessContinue)
	{
		// Network task has not finished yet but we need to process the results received so far
		return true;
	}
	CRY_ASSERT(m_currentTaskId == taskId);
	if (m_currentTaskId == taskId)
	{
		m_currentTaskId = CryLobbyInvalidTaskID;
	}
	else
	{
		CryLog("CGameLobby::NetworkCallbackReceived() received callback with an unexpected taskId=%d, expected=%d", taskId, m_currentTaskId);
	}

	// eCLE_NoServerAvailable should only happen if we're requesting a server
	// from the dedicated arbitrator and there are no servers available or the connection
	// to the allocated server fails on inital join for whatever reason (in which case we should try again)
	if (result == eCLE_TimeOut || result == eCLE_NoServerAvailable)
	{
		if(!m_bCancelling || m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Delete)
		{
			CryLog("CGameLobby::NetworkCallbackReceived() task timed out, restarting");
			m_taskQueue.RestartTask();
		}
		else
		{
			CryLog("CGameLobby::NetworkCallbackReceived() task timed out, finishing because of user initiated cancel");
			m_taskQueue.TaskFinished();
		}
	}
	else
	{
		if (result != eCLE_Success)
		{
			CryLog("CGameLobby::NetworkCallbackReceived() task unsuccessful, result=%u", result);
			if (!m_bMatchmakingSession)
			{
				if(!m_bCancelling)
				{
					if (!(m_bRetryIfPassworded && (result == eCLE_PasswordIncorrect)))
					{
						ShowErrorDialog(result, NULL, NULL, NULL);
					}
				}
			}
		}

		m_taskQueue.TaskFinished();
	}


	return (result == eCLE_Success);
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoStartSession( ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId, bool &bTaskStartedOut )
{
	CryLog("CGameLobby::DoStartSession() pLobby=%p", this);
	INDENT_LOG_DURING_SCOPE();

	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SessionStart);

	ECryLobbyError result = eCLE_Success;

	if (m_gameLobbyMgr->IsPrimarySession(this))
	{
		result = pMatchMaking->SessionStart(m_currentSession, &taskId, MatchmakingSessionStartCallback, this);

		if(result == eCLE_Success)
		{
			CFrontEndModelCache::Allow3dFrontEndAssets(false, true);
		}
		else if (result != eCLE_TooManyTasks)
		{
			CryLog("[GameLobby] Failed to start StartSession lobby task");
			SessionStartFailed(result);
		}

		bTaskStartedOut = true;
		m_numPlayerJoinResets  = 0;
	}
	else
	{
		CryLog("[GameLobby] Didn't start - waiting till we're the primary session");
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionStart, false);

		bTaskStartedOut = false;
	}

	CryLog("[GameLobby] DoStartSession returning %d (%s)", result, (result == eCLE_Success) ? "success" : "fail");

	return result;
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoEndSession( ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId )
{
	CryLog("CGameLobby::DoEndSession() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SessionEnd);

	ECryLobbyError result = pMatchMaking->SessionEnd(m_currentSession, &taskId, MatchmakingSessionEndCallback, this);

	if (result == eCLE_SuccessInvalidSession)
	{
		SessionEndCleanup();
	}
	else if ((result != eCLE_Success) && (result != eCLE_TooManyTasks))
	{
		CryLog("[GameLobby] Failed to start StartSession lobby task");
		SessionEndFailed(result);
	}

	return result;
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoEnsureBestHost( ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId )
{
	CryLog("CGameLobby::DoEnsureBestHost() pLobby=%p", this);
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_EnsureBestHost);

	ECryLobbyError result = pMatchMaking->SessionEnsureBestHost(m_currentSession, &taskId, MatchmakingEnsureBestHostCallback, this);

	return result;
}

//-------------------------------------------------------------------------
void CGameLobby::CancelSessionInit()
{
	m_taskQueue.ClearNotStartedTasks();	// always clear not started tasks

	if(m_taskQueue.HasTaskInProgress())
	{
		CLobbyTaskQueue::ESessionTask currentTask = m_taskQueue.GetCurrentTask();

		CryLog("CancelSessionInit currentTask %d", currentTask);

		if(currentTask != CLobbyTaskQueue::eST_Create && currentTask != CLobbyTaskQueue::eST_Join && currentTask != CLobbyTaskQueue::eST_Delete)
		{
			CancelLobbyTask(currentTask);
		}

		if(currentTask != CLobbyTaskQueue::eST_Delete)
		{
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Delete, false);
		}

		m_bCancelling = true;
	}
	else
	{
		CryLog("CancelSessionInit currentSession %d", m_currentSession);

		if(m_currentSession != CrySessionInvalidHandle) // we have a session, get ready to delete it
		{
			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Delete, false);
			m_bCancelling = true;
		}
		else // no session, go to null state
		{
			SetState(eLS_None);
			m_bCancelling = false;
		}
	}

	m_allowRemoveUsers = true;	// cancelling session, user removal authorised
}

//-------------------------------------------------------------------------
bool CGameLobby::IsCreatingOrJoiningSession()
{
	return (m_taskQueue.HasTaskInProgress()) && (m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Create || m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Join);
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoTerminateHostHintingForGroup( ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId, bool &bTaskStartedOut )
{
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_TerminateHostHinting);

	// Find the players we need to disable hinting for
	int numPlayersThatNeedToLeaveFirst = 0;
	SCryMatchMakingConnectionUID users[MAX_PLAYER_LIMIT];

	const int numRemainingPlayers = m_nameList.Size();
	for (int i = 1; i < numRemainingPlayers; ++ i)
	{
		SSessionNames::SSessionName &player = m_nameList.m_sessionNames[i];
		if (player.m_bMustLeaveBeforeServer)
		{
			users[numPlayersThatNeedToLeaveFirst] = player.m_conId;
			++ numPlayersThatNeedToLeaveFirst;
		}
	}

	if (numPlayersThatNeedToLeaveFirst)
	{
		ECryLobbyError result = pMatchMaking->SessionTerminateHostHintingForGroup(m_currentSession, &users[0], numPlayersThatNeedToLeaveFirst, &taskId, MatchmakingSessionTerminateHostHintingForGroupCallback, this);
		bTaskStartedOut = true;
		return result;
	}
	else
	{
		// No need to do the task, mark it as finished
		taskId = CryLobbyInvalidTaskID;
		bTaskStartedOut = false;
		return eCLE_Success;
	}
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionTerminateHostHintingForGroupCallback( CryLobbyTaskID taskID, ECryLobbyError error, void* pArg )
{
	ENSURE_ON_MAIN_THREAD;

	CryLog("CGameLobby::MatchmakingSessionTerminateHostHintingForGroupCallback()");

	CGameLobby *pGameLobby = static_cast<CGameLobby *>(pArg);
	if (pGameLobby->NetworkCallbackReceived(taskID, error))
	{
		pGameLobby->m_isLeaving = true;
		pGameLobby->m_leaveGameTimeout = s_pGameLobbyCVars->gl_leaveGameTimeout;

		pGameLobby->CheckCanLeave();
	}
	else if (error != eCLE_TimeOut)
	{
		pGameLobby->LeaveSession(true, false);
	}
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionClosedCallback( UCryLobbyEventData eventData, void *userParam )
{
	MatchmakingSessionClosed(eventData, userParam, CErrorHandling::eNFE_HostMigrationFailed);
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionKickedCallback( UCryLobbyEventData eventData, void *userParam )
{
	MatchmakingSessionClosed(eventData, userParam, CErrorHandling::eNFE_Kicked);
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionKickedHighPingCallback( UCryLobbyEventData eventData, void *userParam )
{
	MatchmakingSessionClosed(eventData, userParam, CErrorHandling::eNFE_KickedHighPing);
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionKickedReservedUserCallback( UCryLobbyEventData eventData, void *userParam )
{
	MatchmakingSessionClosed(eventData, userParam, CErrorHandling::eNFE_KickedReservedUser);
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionClosed(UCryLobbyEventData eventData, void *userParam, CErrorHandling::ENonFatalError error)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(userParam);
	CRY_ASSERT(pGameLobby);
	if (pGameLobby)
	{
		SCryLobbySessionEventData *pEventData = eventData.pSessionEventData;

		if ((pGameLobby->m_currentSession == pEventData->session) && (pEventData->session != CrySessionInvalidHandle))
		{
			if (gEnv->IsDedicated())
			{
				CryLogAlways("CGameLobby::MatchmakingSessionClosed() received SessionClosed event, leaving session");
			}
			else
			{
				CryLog("CGameLobby::MatchmakingSessionClosed() received SessionClosed event, leaving session");
			}

			pGameLobby->ConnectionFailed(error);
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingForcedFromRoomCallback(UCryLobbyEventData eventData, void *pArg)
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);
	if (pGameLobby)
	{
		SCryLobbyForcedFromRoomData *pEventData = eventData.pForcedFromRoomData;

		CryLog("CGameLobby::ForcedFromRoomCallback session %d reason %d", (int)pEventData->m_session, pEventData->m_why);

		if (pEventData->m_session != CrySessionInvalidHandle && pGameLobby->m_currentSession == pEventData->m_session)
		{
			CryLog("[game] received eCLSE_ForcedFromRoom event with reason %d, leaving session", pEventData->m_why);

			// if in game, then this will effectively tell the user that the connection to
			// their game has been lost, if in the lobby, then it will try and find a new session
			pGameLobby->ConnectionFailed(CErrorHandling::eNFE_HostMigrationFailed);
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::RequestDetailedServerInfo(CrySessionID sessionId, EDetailedSessionInfoResponseFlags flags)
{
	memset(&m_detailedServerInfo, 0, sizeof(m_detailedServerInfo));
	m_detailedServerInfo.m_sessionId = sessionId;
	m_detailedServerInfo.m_taskID = CryLobbyInvalidTaskID;
	m_detailedServerInfo.m_flags = flags;

	m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionRequestDetailedInfo, true);
}

ECryLobbyError CGameLobby::DoSessionDetailedInfo( ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId )
{
	CryLog("CGameLobby::DoSessionDetailedInfo");

	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SessionRequestDetailedInfo);

	ECryLobbyError detailedInfoResult = eCLE_ServiceNotSupported;
	CCryLobbyPacket packet;
	const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size;

	if (packet.CreateWriteBuffer(bufferSize))
	{
		packet.StartWrite(eGUPD_DetailedServerInfoRequest, true);
		//-- eDSIRF_IncludePlayers means include player names in the response.
		//-- eDSIRF_IncludeCustomFields means include custom fields in response (but only if server is custom mode).
		//-- clear the flag to not return players.
		packet.WriteUINT8(m_detailedServerInfo.m_flags);

		detailedInfoResult = pMatchMaking->SessionSendRequestInfo(&packet, m_detailedServerInfo.m_sessionId, &taskId, CGameLobby::MatchmakingSessionDetailedInfoResponseCallback, this);
		if(detailedInfoResult != eCLE_Success)
		{
			taskId = CryLobbyInvalidTaskID;
		}
		else
		{
			m_detailedServerInfo.m_taskID = taskId;
		}
	}
	else
	{
		detailedInfoResult = eCLE_OutOfMemory;
	}

	CryLog("DoSessionInfo %s with error %d", detailedInfoResult == eCLE_Success ? "succeeded" : "failed", detailedInfoResult);

	return detailedInfoResult;
}

void CGameLobby::MatchmakingSessionDetailedInfoRequestCallback(UCryLobbyEventData eventData, void *pArg)
{
	ENSURE_ON_MAIN_THREAD;

	//-- Server receives eGUPD_DetailedServerInfoRequest packet and generates eCLSE_SessionRequestInfo event
	//-- which calls this function.
	//-- This function creates a eGUPD_DetailedServerInfoResponse response packet and punts the results back to the client.

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);
	if (pGameLobby)
	{
		SCryLobbySessionRequestInfo* pEventData = eventData.pSessionRequestInfo;

		CryLog("CGameLobby::MatchmakingSessionDetailedInfoRequestCallback session %d", (int)pEventData->session);

		if (pEventData->session != CrySessionInvalidHandle && pGameLobby->m_currentSession == pEventData->session)
		{
			CCryLobbyPacket* pRequest = pEventData->pPacket;
			if (pRequest)
			{
				pRequest->StartRead();
				uint8 flags = pRequest->ReadUINT8();

				uint8 playerCount = (uint8)std::min((uint8)DETAILED_SESSION_MAX_PLAYERS, (uint8)pGameLobby->m_nameList.Size());

				uint32 numCustoms = 0;
				CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
				if (pPlaylistManager)
				{
					CRY_ASSERT(pPlaylistManager->GetGameModeOptionCount() <= DETAILED_SESSION_MAX_CUSTOMS);
					numCustoms = std::min((uint32)DETAILED_SESSION_MAX_CUSTOMS, pPlaylistManager->GetGameModeOptionCount());
				}

				CCryLobbyPacket packet;
				uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size;
				bufferSize += CryLobbyPacketUINT8Size * DETAILED_SESSION_INFO_MOTD_SIZE;
				bufferSize += CryLobbyPacketUINT8Size * DETAILED_SESSION_INFO_URL_SIZE;
				if (flags & eDSIRF_IncludePlayers)
				{
					bufferSize += CryLobbyPacketUINT8Size;	//-- count
					bufferSize += CryLobbyPacketUINT8Size * playerCount * CRYLOBBY_USER_NAME_LENGTH; //-- names
				}
				if (flags & eDSIRF_IncludeCustomFields)
				{
					bufferSize += CryLobbyPacketUINT8Size;  //-- count
					bufferSize += CryLobbyPacketUINT16Size * numCustoms; //-- custom fields
				}

				if (packet.CreateWriteBuffer(bufferSize))
				{
					char motd[DETAILED_SESSION_INFO_MOTD_SIZE];
					char url[DETAILED_SESSION_INFO_URL_SIZE];

					cry_strcpy(motd, g_pGameCVars->g_messageOfTheDay->GetString());
					cry_strcpy(url, g_pGameCVars->g_serverImageUrl->GetString());

					packet.StartWrite(eGUPD_DetailedServerInfoResponse, true);
					packet.WriteUINT8(flags);
					packet.WriteString(motd, DETAILED_SESSION_INFO_MOTD_SIZE);
					packet.WriteString(url, DETAILED_SESSION_INFO_URL_SIZE);

					if (flags & eDSIRF_IncludePlayers)
					{
						CryLog("  Including %d Players...", playerCount);
						packet.WriteUINT8(playerCount);
						for (uint32 i = 0; i < playerCount; ++i)
						{
							packet.WriteString(pGameLobby->m_nameList.m_sessionNames[i].m_name, CRYLOBBY_USER_NAME_LENGTH);
						}
					}

					if (flags & eDSIRF_IncludeCustomFields)
					{
						if (pPlaylistManager)
						{
							CryLog("  Including %d Custom Fields...", numCustoms);
							packet.WriteUINT8((uint8)numCustoms);
							for (uint32 j = 0; j < numCustoms; ++j)
							{
								uint16 value = pPlaylistManager->PackCustomVariantOption(j);
								packet.WriteUINT16(value);
							}
						}
						else
						{
							packet.WriteUINT8(0);
						}
					}

					if (ICryMatchMaking* pMatchMaking = gEnv->pNetwork->GetLobby()->GetMatchMaking())
					{
						pMatchMaking->SessionSendRequestInfoResponse(&packet, pEventData->requester);
					}

					packet.FreeWriteBuffer();
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// Process a detailed server info packet
void CGameLobby::MatchmakingSessionDetailedInfoResponseCallback(CryLobbyTaskID taskID, ECryLobbyError error, CCryLobbyPacket* pPacket, void* pArg)
{
	ENSURE_ON_MAIN_THREAD;

	//-- Called on the client when eGUPD_DetailedServerInfoResponse packet is received.
	//-- Should put the response details into a cache somewhere for Flash to query

	CryLog("MatchmakingSessionDetailedInfoResponseCallback error %d packet %p", error, pPacket);

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);
	if (error == eCLE_Success)
	{
		if (pPacket)
		{
			CryLog("MatchmakingSessionDetailedInfoResponseCallback with valid packet!");

			if (pGameLobby)
			{
				SDetailedServerInfo* pDetails = &pGameLobby->m_detailedServerInfo;

				if(pDetails->m_taskID == taskID)
				{
					CryLog("processing detailed info response callback");

					pPacket->StartRead();
					uint8 flags = pPacket->ReadUINT8();
					pPacket->ReadString(pDetails->m_motd, DETAILED_SESSION_INFO_MOTD_SIZE);
					pDetails->m_motd[DETAILED_SESSION_INFO_MOTD_SIZE - 1] = 0;
					pPacket->ReadString(pDetails->m_url, DETAILED_SESSION_INFO_URL_SIZE);
					pDetails->m_url[DETAILED_SESSION_INFO_URL_SIZE - 1] = 0;

					CryLog("  MOTD: %s", pDetails->m_motd);
					CryLog("  URL: %s", pDetails->m_url);

					int nCustoms = 0;

					if (flags & eDSIRF_IncludePlayers)
					{
						uint32 nCount = (uint32)pPacket->ReadUINT8();
						nCount = std::min(nCount, (uint32)DETAILED_SESSION_MAX_PLAYERS);
						CryLog("  PlayerCount %d:", nCount);

						pDetails->m_namesCount = nCount;
						for (uint32 i = 0; i < nCount; ++i)
						{
							pPacket->ReadString(pDetails->m_names[i], CRYLOBBY_USER_NAME_LENGTH);
							pDetails->m_names[i][CRYLOBBY_USER_NAME_LENGTH - 1] = 0;

							CryLog("    Name %d: %s", i, pDetails->m_names[i]);
						}
					}
					if (flags & eDSIRF_IncludeCustomFields)
					{
						nCustoms = (uint32)pPacket->ReadUINT8();
						nCustoms = std::min<int>(nCustoms, DETAILED_SESSION_MAX_CUSTOMS);
						CryLog("  CustomCount %d:", nCustoms);

						for (uint32 i = 0; i < (uint32)nCustoms; ++i)
						{
							pDetails->m_customs[i] = pPacket->ReadUINT16();

							CryLog("    Custom %d: %d", i, pDetails->m_customs[i]);
						}

						CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
						if(pPlaylistManager)
						{
							pPlaylistManager->ReadDetailedServerInfo(pDetails->m_customs, nCustoms);
						}
					}
#if 0 // old frontend
					CFlashFrontEnd *pFlashMenu = g_pGame->GetFlashMenu();
					if (pFlashMenu != NULL && pFlashMenu->GetCurrentMenuScreen()==eFFES_lobby_browser)
					{
						CMPMenuHub *pMPMenu = CMPMenuHub::GetMPMenuHub();
						CUIServerList *pServerList = pMPMenu ? pMPMenu->GetServerListUI() : NULL;
						if (pServerList)
						{
							pServerList->UpdateExtendedInfoPopUp(pFlashMenu->GetFlash(), pDetails, nCustoms);
						}
					}
#endif
				}
				else
				{
					CryLog("ignoring detailed info response callback as taskIds do not match, assuming we no longer care about it");
				}
			}
		}
	}
	else
	{
		CryLog("Error on receiving SessionInfo response. (%d)", error);

		if (pGameLobby)
		{
			SDetailedServerInfo* pDetails = &pGameLobby->m_detailedServerInfo;

			if(pDetails->m_taskID == taskID)
			{
				g_pGame->AddGameWarning("MatchMakingTaskError", "Error on receiving SessionInfo response.");
			}
		}
	}
}

//-------------------------------------------------------------------------
IHostMigrationEventListener::EHostMigrationReturn CGameLobby::OnTerminate( SHostMigrationInfo& hostMigrationInfo, uint32& state )
{
	if (hostMigrationInfo.m_session == m_currentSession)
	{
		CryLog("CGameLobby::OnTerminate() host migration failed, leaving session");
		ConnectionFailed(CErrorHandling::eNFE_HostMigrationFailed);
	}

	if( m_bMatchmakingSession )
	{
		if( CMatchMakingHandler* pMatchmakingHandler = m_gameLobbyMgr->GetMatchMakingHandler() )
		{
			pMatchmakingHandler->OnHostMigrationFinished( false, false );
		}
	}

	return IHostMigrationEventListener::Listener_Done;
}

//-------------------------------------------------------------------------
IHostMigrationEventListener::EHostMigrationReturn CGameLobby::OnReset(SHostMigrationInfo& hostMigrationInfo, uint32& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

//-------------------------------------------------------------------------
void CGameLobby::ConnectionFailed(CErrorHandling::ENonFatalError error)
{
	const ELobbyState currentState = m_state;

	if (currentState == eLS_Leaving)
	{
		CryLog("CGameLobby::ConnectionFailed() already in leaving state, ignoring");
		return;
	}

	CryLog("CGameLobby::ConnectionFailed() error=%d", (int) error);

	if (!m_bMatchmakingSession && !IsQuitting() && (error != CErrorHandling::eNFE_None))
	{
		CErrorHandling *pErrorHandling = CErrorHandling::GetInstance();
		if (pErrorHandling)
		{
			pErrorHandling->OnNonFatalError(error, true);
		}
	}

	LeaveSession(true, false);

}

//-------------------------------------------------------------------------
void CGameLobby::ResetFlashInfos()
{
	m_votingFlashInfo.Reset();
	m_votingCandidatesFlashInfo.Reset();
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoSessionSetLocalFlags( ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId )
{
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SessionSetLocalFlags);

	ECryLobbyError result = pMatchMaking->SessionSetLocalFlags(m_currentSession, CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST, &taskId, MatchmakingSessionSetLocalFlagsCallback, this);
	return result;
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSessionSetLocalFlagsCallback( CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, uint32 flags, void* pArg )
{
	ENSURE_ON_MAIN_THREAD;

	CGameLobby *pGameLobby = static_cast<CGameLobby*>(pArg);

	CryLog("CGameLobby::MatchmakingSessionSetLocalFlagsCallback() error=%u, pGameLobby=%p", error, pGameLobby);

	pGameLobby->NetworkCallbackReceived(taskID, error);
}

//-------------------------------------------------------------------------
void CGameLobby::OnHaveLocalPlayer()
{
	CryLog("CGameLobby::OnHaveLocalPlayer() m_bNeedToSetAsElegibleForHostMigration=%s", m_bNeedToSetAsElegibleForHostMigration ? "true" : "false");
	if (m_bNeedToSetAsElegibleForHostMigration)
	{
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_SessionSetLocalFlags, false);
		m_bNeedToSetAsElegibleForHostMigration = false;
	}
}

//-------------------------------------------------------------------------
void CGameLobby::CmdAdvancePlaylist( IConsoleCmdArgs *pArgs )
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		pGameLobby->DebugAdvancePlaylist();
	}
}

//-------------------------------------------------------------------------
void CGameLobby::DebugAdvancePlaylist()
{
	if ((m_currentSession != CrySessionInvalidHandle) && (m_bMatchmakingSession))
	{
		if (m_server)
		{
			CryLog("CGameLobby::DebugAdvancePlaylist()");
			UpdateLevelRotation();
			SvResetVotingForNextElection();
			m_bPlaylistHasBeenAdvancedThroughConsole = true;		// This is nasty but we can't pass arguments into the SendPacket function :-(
			CryLog("[tlh] calling SendPacket(eGUPD_SyncPlaylistRotation) [2]");
			SendPacket(eGUPD_SyncPlaylistRotation);
			m_bPlaylistHasBeenAdvancedThroughConsole = false;
		}
		else
		{
			SendPacket(eGUPD_RequestAdvancePlaylist);
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::SetLocalVoteStatus( ELobbyVoteStatus state )
{
	if (m_localVoteStatus != state)
	{
		m_localVoteStatus = state;

		// Distinguish between networked vote status and local vote status so we don't send multiple updates
		// without actually changing anything ("waiting for candidates" to "not voted" for instance)
		ELobbyNetworkedVoteStatus networkedVoteState = eLNVS_NotVoted;
		if (state == eLVS_votedLeft)
		{
			networkedVoteState = eLNVS_VotedLeft;
		}
		else if (state == eLVS_votedRight)
		{
			networkedVoteState = eLNVS_VotedRight;
		}

		if (m_networkedVoteStatus != networkedVoteState)
		{
			m_networkedVoteStatus = networkedVoteState;
			if (m_currentSession != CrySessionInvalidHandle)
			{
				if (!m_bCancelling)
				{
					m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetLocalUserData, true);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
ELobbyNetworkedVoteStatus CGameLobby::GetVotingStateForPlayer(SSessionNames::SSessionName *pSessionName) const
{
	ELobbyNetworkedVoteStatus result = eLNVS_NotVoted;
	if (pSessionName)
	{
		result = (ELobbyNetworkedVoteStatus) pSessionName->m_userData[eLUD_VoteChoice];
	}
	return result;
}

//-------------------------------------------------------------------------
bool CGameLobby::CalculateVotes()
{
	if (m_votingEnabled)
	{
		if (!m_votingClosed)
		{
			int numVotesForLeft = 0;
			int numVotesForRight = 0;

			const unsigned int numPlayers = m_nameList.Size();
			for (unsigned int i = 0; i < numPlayers; ++ i)
			{
				SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[i];
				const ELobbyNetworkedVoteStatus votingStatus = GetVotingStateForPlayer(pPlayer);
				if (votingStatus == eLNVS_VotedLeft)
				{
					++ numVotesForLeft;
				}
				else if (votingStatus == eLNVS_VotedRight)
				{
					++ numVotesForRight;
				}
			}

			if ((numVotesForLeft > m_highestLeadingVotesSoFar) && (numVotesForLeft > numVotesForRight))
			{
				m_highestLeadingVotesSoFar = numVotesForLeft;
				m_leftHadHighestLeadingVotes = true;
			}
			else if ((numVotesForRight > m_highestLeadingVotesSoFar) && (numVotesForRight > numVotesForLeft))
			{
				m_highestLeadingVotesSoFar = numVotesForRight;
				m_leftHadHighestLeadingVotes = false;
			}

			if ((m_leftVoteChoice.m_numVotes != numVotesForLeft) || (m_rightVoteChoice.m_numVotes != numVotesForRight))
			{
				m_leftVoteChoice.m_numVotes = numVotesForLeft;
				m_rightVoteChoice.m_numVotes = numVotesForRight;
				return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------
void CGameLobby::CheckForVotingChanges(bool bUpdateFlash)
{
	if (CalculateVotes() && bUpdateFlash)
	{
		if (m_state == eLS_Lobby)
		{
			UpdateVotingInfoFlashInfo();
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::ResetLocalVotingData()
{
	SetLocalVoteStatus(eLVS_awaitingCandidates);		// Reset our vote so that our initial user data update will not have a vote in it
	m_leftVoteChoice.Reset();
	m_rightVoteChoice.Reset();
	m_highestLeadingVotesSoFar = 0;
	m_leftHadHighestLeadingVotes = false;

	UpdateVotingInfoFlashInfo();
}

//-------------------------------------------------------------------------
void CGameLobby::ClearBadServers()
{
	CryLog("[GameLobby] %p ClearBadServers", this);

	m_badServers.clear();
}

//-------------------------------------------------------------------------
uint16 CGameLobby::GetSkillRanking( int channelId )
{
	uint16 skillRanking = 1500;		// Default

	if (gEnv->pNetwork && gEnv->pNetwork->GetLobby())
	{
		if (ICryMatchMaking* pMatchMaking = gEnv->pNetwork->GetLobby()->GetMatchMaking())
		{
			const CrySessionHandle sessionHandle = m_currentSession;
			const SCryMatchMakingConnectionUID conId = pMatchMaking->GetConnectionUIDFromGameSessionHandleAndChannelID(sessionHandle, channelId);

			int playerIndex = m_nameList.Find(conId);
			if (playerIndex != SSessionNames::k_unableToFind)
			{
				SSessionNames::SSessionName &player = m_nameList.m_sessionNames[playerIndex];
				skillRanking = player.GetSkillRank();
			}
		}
	}

	return skillRanking;
}

//-------------------------------------------------------------------------
CGameLobby::EActiveStatus CGameLobby::GetActiveStatus(const ELobbyState currentState) const
{
	EActiveStatus activeStatus = eAS_Lobby;
	if (currentState == eLS_Game)
	{
		activeStatus = eAS_StartingGame;
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			activeStatus = eAS_Game;
			if (pGameRules->IsTimeLimited())
			{
				const float remainingGameTime = pGameRules->GetRemainingGameTime();
				if (remainingGameTime < s_pGameLobbyCVars->gl_timeTillEndOfGameForNoMatchMaking)
				{
					activeStatus = eAS_EndGame;
				}
			}
		}
	}
	else if (IsGameStarting())
	{
		if (m_startTimer < s_pGameLobbyCVars->gl_timeBeforeStartOfGameForNoMatchMaking)
		{
			activeStatus = eAS_StartingGame;
		}
	}

	return activeStatus;
}

//-------------------------------------------------------------------------
int32 CGameLobby::CalculateAverageSkill()
{
	// Note: This won't be an exact average since we're returning an int, however the value
	// is going to be used as an int in the session details and we're not too worried about
	// being absolutely correct on it (it's used as an approximate measure for matchmaking)

	int32 totalSkill = 0;

	const unsigned int numPlayers = m_nameList.Size();
	if (numPlayers > 1)
	{
		for (unsigned int i = 0; i < numPlayers; ++ i)
		{
			uint16 skillRank = m_nameList.m_sessionNames[i].GetSkillRank();
			totalSkill += (int32) skillRank;
		}
		totalSkill /= numPlayers;
	}
	else
	{
		totalSkill = CPlayerProgression::GetInstance()->GetData(EPP_SkillRank);
	}

	return totalSkill;
}

//-------------------------------------------------------------------------
void CGameLobby::CheckForSkillChange()
{
	if (m_server && (m_state != eLS_Leaving))
	{
		uint32 newAverage = CalculateAverageSkill();
		if (newAverage != m_lastUpdatedAverageSkill)
		{
			m_timeTillUpdateSession = s_pGameLobbyCVars->gl_skillChangeUpdateDelayTime;
		}
	}
}

//-------------------------------------------------------------------------
int32 CGameLobby::GetCurrentLanguageId()
{
	int32 languageId = 0;
	ILocalizationManager* pLocalizationManager = gEnv->pSystem->GetLocalizationManager();
	if (pLocalizationManager)
	{
		CryHashStringId hash(	pLocalizationManager->GetLanguage() );
		languageId = (int32) hash.id;
	}
	return languageId;
}

//-------------------------------------------------------------------------
void CGameLobby::CreateSessionFromSettings(const char *pGameRules, const char *pLevelName)
{
	CryLog("CreateSessionFromSettings");

	// do this first, otherwise we could overwrite any options we set below
	if (m_state != eLS_None)
	{
		CryLog("  has been called but lobby state is %d, calling LeaveSession", m_state);
		LeaveSession(true, false);
	}

	CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
	if (pPlaylistManager)
	{
		int defaultPlaylistId = pPlaylistManager->GetDefaultVariant();
		pPlaylistManager->ChooseVariant(defaultPlaylistId);
	}

	SetMatchmakingGame(false);
	ChangeGameRules(pGameRules);
	SetCurrentLevelName(pLevelName);

	m_taskQueue.AddTask(CLobbyTaskQueue::eST_Create, false);

	// Show create session dialog whether we can start the create now or not
	NOTIFY_UILOBBY_MP(ShowLoadingDialog("CreateSession"));

	//PrecachePaks(pLevelName);
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateRulesAndMapFromVoting()
{
	if (m_leftWinsVotes)
	{
		UpdateRulesAndLevel(m_leftVoteChoice.m_gameRules.c_str(), m_leftVoteChoice.m_levelName.c_str());
	}
	else
	{
		UpdateRulesAndLevel(m_rightVoteChoice.m_gameRules.c_str(), m_rightVoteChoice.m_levelName.c_str());
	}
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateRulesAndLevel(const char *pGameRules, const char *pLevelName)
{
	ChangeGameRules(pGameRules);
	SetCurrentLevelName(pLevelName);

	PrecachePaks(pLevelName);
}

//-------------------------------------------------------------------------
void CGameLobby::PrecachePaks(const char *pLevelName)
{
	if(s_pGameLobbyCVars->gl_precachePaks == 0)
	{
		// If caching disabled return
		return;
	}

	if(!m_bAllowPakPrecaching)
	{
#ifndef _RELEASE
		if(s_pGameLobbyCVars->gl_precacheLogging)
			CryLog("[PRECACHE] Trying to pre-cache level[%s]. Cannot pre-cache level now.", pLevelName );
#endif //_RELEASE
		return;
	}

	// Empty string means we don't change anything with the cached files.
	if(!pLevelName || pLevelName[0]==0)
	{
#ifndef _RELEASE
		if(s_pGameLobbyCVars->gl_precacheLogging)
			CryLog("[PRECACHE] Empty string. Current cached level[%s]", m_cachedLevelName.c_str());
#endif //_RELEASE
		return;
	}

	if(m_cachedLevelName.length()!=0)
	{
		if(m_cachedLevelName.compare(pLevelName)==0)
		{
			//level is already precached
#ifndef _RELEASE
			if(s_pGameLobbyCVars->gl_precacheLogging)
				CryLog("[PRECACHE] Level[%s] already precached.", pLevelName);
#endif //_RELEASE
			return;
		}
	}

	UnloadPrecachedPaks("New Level");

#ifndef _RELEASE
	if(s_pGameLobbyCVars->gl_precacheLogging)
		CryLog("[PRECACHE] Precaching new level[%s]", pLevelName);
#endif //_RELEASE

	m_cachedLevelName = pLevelName;

	IResourceManager *pResourceManager = gEnv->pSystem->GetIResourceManager();

	//
	// _levelcache paks
	//

	ICVar *pStreamCGFVar = gEnv->pConsole->GetCVar("e_StreamCgf");
	string pathBase = PathUtil::GetGameFolder() + "/_levelcache/";
	string fullPath;
	pathBase += pLevelName;
	pathBase += "/";

	if(!gEnv->pCryPak->IsFolder(pathBase.c_str()))
	{
		// Cached file does not exist
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"Level cache enabled, but the %s folder does not exist", pathBase.c_str());
		return;
	}

	if(pStreamCGFVar && pStreamCGFVar->GetIVal() != 0)
	{
		fullPath = pathBase + "cgf.pak";
		PrecachePak(fullPath,*pResourceManager,true);

		fullPath = pathBase + "cga.pak";
		PrecachePak(fullPath,*pResourceManager,true);

		fullPath = pathBase + "cgf_cache.pak";
		PrecachePak(fullPath,*pResourceManager,false);
	}

	fullPath = pathBase + "dds_cache.pak";
	PrecachePak(fullPath,*pResourceManager,false);

	fullPath = pathBase + "dds0.pak";
	PrecachePak(fullPath,*pResourceManager,true);

	fullPath = pathBase + "xml.pak";
	PrecachePak(fullPath,*pResourceManager,true);

	//
	// levels paks
	//
	pathBase = PathUtil::GetGameFolder() + "/levels/";
	pathBase += pLevelName;
	pathBase += "/";

	fullPath = pathBase + "level.pak";
	PrecachePak(fullPath,*pResourceManager,true);

	fullPath = pathBase + "levelshadercache.pak";
	PrecachePak(fullPath,*pResourceManager,true);

	//
	// Engine pak
	// Don't add to list or log this because we should never close it.
	fullPath = "engine/engine.pak";
	pResourceManager->LoadPakToMemAsync(fullPath.c_str(), true);
}

void CGameLobby::PrecachePak( const string& fullPath, IResourceManager& resourceManager, const bool levelLoadOnly )
{
	resourceManager.LoadPakToMemAsync(fullPath.c_str(), levelLoadOnly);
	m_precachePaks.push_back(fullPath);
#ifndef _RELEASE
	if(s_pGameLobbyCVars->gl_precacheLogging)
		CryLog("[PRECACHE] PrecachePak [%s]", fullPath.c_str());
#endif //_RELEASE
}

void CGameLobby::UnloadPrecachedPaks(const char* reason)
{
#ifndef _RELEASE
	if(s_pGameLobbyCVars->gl_precacheLogging)
		CryLog("[PRECACHE] Reason[%s] Unloading pre-cached level[%s]",reason , m_cachedLevelName.c_str());
#endif //_RELEASE

	gEnv->pSystem->GetIResourceManager()->UnloadAllAsyncPaks();

	const uint32 nPrecachePaks = m_precachePaks.size();
	for(uint32 i=0; i<nPrecachePaks; i++)
	{
		gEnv->pCryPak->ClosePack(m_precachePaks[i].c_str());
	}

	ClearLoggedPrecachedPaks("Unloaded Paks");
}

void CGameLobby::ClearLoggedPrecachedPaks( const char* reason )
{
#ifndef _RELEASE
	if(s_pGameLobbyCVars->gl_precacheLogging)
		CryLog("[PRECACHE] Reason[%s] Clearing list of logged pre-cached files.", reason);
#endif //_RELEASE

	m_cachedLevelName.clear();
	m_precachePaks.clear();
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateVoteChoices()
{
	if (ILevelRotation* pLevelRotation=g_pGame->GetPlaylistManager()->GetLevelRotation())
	{
		const int curNext = pLevelRotation->GetNext();

		m_leftVoteChoice.m_levelName = pLevelRotation->GetNextLevel();
		m_leftVoteChoice.m_gameRules = pLevelRotation->GetNextGameRules();

		if (!pLevelRotation->Advance())
		{
			pLevelRotation->First();
		}

		m_rightVoteChoice.m_levelName = pLevelRotation->GetNextLevel();
		m_rightVoteChoice.m_gameRules = pLevelRotation->GetNextGameRules();

		while (pLevelRotation->GetNext() != curNext)
		{
			if (!pLevelRotation->Advance())
			{
				pLevelRotation->First();
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::MoveUsers(CGameLobby *pFromLobby)
{
	CryLog("[GameLobby] MoveUsers pFromLobby %p pToLobby %p", pFromLobby, this);
	CRY_ASSERT_MESSAGE(pFromLobby->IsServer(), "Only the server should be moving users from one lobby to another");
	CRY_ASSERT_MESSAGE(pFromLobby != this, "Lobby we are trying to move users into is the one we are already in");
	CRY_ASSERT_MESSAGE(m_gameLobbyMgr->IsPrimarySession(pFromLobby), "Trying to move users but we're not the primary session");

	const SSessionNames &fromSession = pFromLobby->GetSessionNames();
	SSessionNames *toSession = &m_nameList;
	unsigned int userCount = fromSession.Size();

	// temporarily copy my details as well for the time being
	for(unsigned int i = 1; i < userCount; ++i)
	{
		const SSessionNames::SSessionName *pSessionName = &fromSession.m_sessionNames[i];
		if (pSessionName->m_name[0] != 0)
		{
			if (toSession->FindByUserId(pSessionName->m_userId) == SSessionNames::k_unableToFind)
			{
				toSession->Insert(pSessionName->m_userId, CryMatchMakingInvalidConnectionUID, pSessionName->m_name, pSessionName->m_userData, pSessionName->m_isDedicated); // we pass invalid connection id so as not to confuse the new lobby
			}
		}

		CryLog("[GameLobby] Moving user %s", pSessionName->m_name);
	}

	CRY_ASSERT_MESSAGE(toSession->Size() <= MAX_PLAYER_LIMIT, string().Format("Too many players added to session names. Count %d Max %d", toSession->Size(), MAX_PLAYER_LIMIT).c_str());
}

//-------------------------------------------------------------------------
void CGameLobby::OnStartPlaylistCommandIssued()
{
	CryLog("CGameLobby::OnStartPlaylistCommandIssued()");
	SetMatchmakingGame(true);
#if 0 // LEVEL ROTATION DISABLED FOR NOW
	if (m_state != eLS_None)
	{
		if (m_server)
		{
			CryLog("  currently server, switching playlist");

			if (g_pGame->GetIGameFramework()->StartedGameContext())
			{
				// Already in a started session, need to end it so we can change playlist
				gEnv->pConsole->ExecuteString("unload", false, false);
				SetState(eLS_EndSession);
				m_bServerUnloadRequired = false;
			}

			uint32 playListSeed = (uint32)(gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetValue());
			m_playListSeed = playListSeed;
			CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
			ILevelRotation*  pLevelRotation = pPlaylistManager ? pPlaylistManager->GetLevelRotation() : NULL;
			if (pLevelRotation != NULL && pLevelRotation->IsRandom())
			{
				pLevelRotation->Shuffle(playListSeed);
			}

			if (m_votingEnabled)
			{
				SvResetVotingForNextElection();
			}
			SendPacket(eGUPD_SyncPlaylistContents);
			SendPacket(eGUPD_SyncPlaylistRotation);

			if (pLevelRotation)
			{
				const int  len = pLevelRotation->GetLength();
				if (len > 0)
				{
					SetCurrentLevelName(pLevelRotation->GetNextLevel());

					// game modes can have aliases, so we get the correct name here
					IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
					const char *pGameRulesName = pGameRulesSystem->GetGameRulesName(pLevelRotation->GetNextGameRules());

					if (stricmp(m_currentGameRules.c_str(), pGameRulesName))
					{
						GameRulesChanged(pGameRulesName);
					}

					pPlaylistManager->SetModeOptions();
				}
			}

			m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
		}
		else
		{
			CryLog("  not server, leaving and starting a new game");
			StartFindGame();
		}
	}
	else
	{
		CryLog("  not in a game currently, starting FindGame");
		StartFindGame();
	}
#endif
}

//-------------------------------------------------------------------------
eHostMigrationState CGameLobby::GetMatchMakingHostMigrationState()
{
	eHostMigrationState hostMigrationState = eHMS_Unknown;

	if (m_currentSession != CrySessionInvalidHandle)
	{
		ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{
			ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
			if (pMatchmaking)
			{
				hostMigrationState = pMatchmaking->GetSessionHostMigrationState(m_currentSession);
			}
		}
	}

	return hostMigrationState;
}

//-------------------------------------------------------------------------
void CGameLobby::TerminateHostMigration()
{
	CryLog("CGameLobby::TerminateHostMigration()");

	if (m_currentSession != CrySessionInvalidHandle)
	{
		ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{
			ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
			if (pMatchmaking)
			{
				pMatchmaking->TerminateHostMigration(m_currentSession);
			}
		}
	}
}

const char* CGameLobby::GetSessionName()
{
 	return m_currentSessionName.c_str();
}

void CGameLobby::GetSessionIDAsString(char* pOutString, int inBufferSize)
{
	if(m_currentSessionId!=CrySessionInvalidID)
	{
		m_currentSessionId->AsCStr(pOutString, inBufferSize);
	}
}

SDetailedServerInfo* CGameLobby::GetDetailedServerInfo()
{
	return &m_detailedServerInfo;
}

void CGameLobby::CancelDetailedServerInfoRequest()
{
	CryLog("CancelDetailedServerInfoRequest");

	if(m_taskQueue.HasTaskInProgress() && m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SessionRequestDetailedInfo)
	{
		CryLog("invalidating in progress detailed session info request");
		m_detailedServerInfo.m_taskID = CryLobbyInvalidTaskID;
	}

	// cancel any versions of this task we have in the queue
	m_taskQueue.CancelTask(CLobbyTaskQueue::eST_SessionRequestDetailedInfo);
}

bool CGameLobby::AllowCustomiseEquipment()
{
#if 0 // old frontend
	CFlashFrontEnd *pFFE = g_pGame->GetFlashMenu();
	EFlashFrontEndScreens screen = pFFE ? pFFE->GetCurrentMenuScreen() : eFFES_unknown;

	EActiveStatus activeStatus = GetActiveStatus(m_state);

	bool allow = true;

	if(screen == eFFES_game_lobby)
	{
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		const SPlaylist* myPlaylist = pPlaylistManager ? pPlaylistManager->GetCurrentPlaylist() : NULL;

		if(myPlaylist)
		{
			// assault playlist
			if(!strcmp(myPlaylist->rotExtInfo.uniqueName, "ASSAULT"))
			{
				allow = false;
			}
		}

		if(allow)
		{
			if (m_votingEnabled)
			{
				// this, as far as I'm aware, is really only a problem on pc at the moment where people could setup
				// their own custom playlists, which might include assault and another gamemode, in that instance
				// we'd like to keep the  customise equipment option available if unlocked
				if(!strcmp(m_leftVoteChoice.m_gameRules.c_str(), "Assault") && !strcmp(m_rightVoteChoice.m_gameRules.c_str(), "Assault"))
				{
					allow = false;
				}
			}
			else
			{
				const char *currentGameRules = GetCurrentGameModeName();
				if(!strcmp(currentGameRules, "Assault"))
				{
					allow = false;
				}
			}
		}
	}

	return allow;
#else
	return false;
#endif
}

void CGameLobby::RefreshCustomiseEquipment()
{
	CryLog("CGameLobby::RefreshCustomiseEquipment");

	// if it's not a matchmaking session, then we have all the details we need,
	// we need to refresh the screen so as to update the customise loadout button
	// as it may need disabling, matchmaing sessions will refresh the screen when they
	// receive the playlist, not we also do not want to do this if we're not the game lobby screen
#if 0 // old frontend
	CFlashFrontEnd *pFFE = g_pGame->GetFlashMenu();
	EFlashFrontEndScreens screen = pFFE ? pFFE->GetCurrentMenuScreen() : eFFES_unknown;

	if(screen == eFFES_game_lobby)
	{
		pFFE->RefreshCurrentScreen();
	}
#endif
}

//-------------------------------------------------------------------------
void CGameLobby::QueueSessionUpdate()
{
	if (m_server && (m_currentSession != CrySessionInvalidHandle))
	{
		m_taskQueue.AddTask(CLobbyTaskQueue::eST_Update, true);
	}
}

//-------------------------------------------------------------------------
void CGameLobby::UpdatePreviousGameScores()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		IGameRulesPlayerStatsModule *pPlayerStatsModule = pGameRules->GetPlayerStatsModule();
		if (pPlayerStatsModule)
		{
			SPlayerScores playerScores[MAX_PLAYER_LIMIT];
			int numPreviousScores = 0;

			const float fGameStartTime = pGameRules->GetGameStartTime();
			const float fGameLength = (pGameRules->GetCurrentGameTime() * 1000.f);		// Spawn time is in milliseconds, length is in seconds
			const int numStats = pPlayerStatsModule->GetNumPlayerStats();
			for (int i=0; i<numStats; ++i)
			{
				const SGameRulesPlayerStat* pPlayerStats = pPlayerStatsModule->GetNthPlayerStats(i);
				float fFracTimeInGame = 0.f;
				CPlayer *pPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pPlayerStats->playerId));
				if (pPlayer)
				{
					const float fTimeSpawned = pPlayer->GetTimeFirstSpawned();
					const float fTimeInGame = (fGameLength + fGameStartTime) - fTimeSpawned;
					if (fGameLength > 0.f)
					{
						fFracTimeInGame = (fTimeInGame / fGameLength);
						fFracTimeInGame = CLAMP(fFracTimeInGame, 0.f, 1.f);
					}

					SPlayerScores *pScore = &playerScores[numPreviousScores++];
					pScore->m_playerId = GetConnectionUIDFromChannelID(pPlayer->GetChannelId());
					pScore->m_score = pPlayerStats->points;
					pScore->m_fracTimeInGame = fFracTimeInGame;
				}
			}

			m_teamBalancing.UpdatePlayerScores(playerScores, numPreviousScores);
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::RequestLeaveFromMenu()
{
	SetState(eLS_Leaving);

	m_taskQueue.AddTask(CLobbyTaskQueue::eST_Unload, false);

	m_isMidGameLeaving = true;

	CSquadManager *pSquadManager = g_pGame->GetSquadManager();
	if (pSquadManager)
	{
		pSquadManager->LeftGameSessionInProgress();
	}
}

//-------------------------------------------------------------------------
void CGameLobby::DoUnload()
{
	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_Unload);

	SetState(eLS_None);

	IGameFramework *pFramework = g_pGame->GetIGameFramework();
	if (pFramework->StartedGameContext() || pFramework->StartingGameContext())
	{
		gEnv->pGameFramework->ExecuteCommandNextFrame("disconnect");
	}

#if 0 // old frontend
	TFlashFrontEndPtr pFlashFrontEnd = g_pGame->GetFlashFrontEnd();
	if (pFlashFrontEnd)
	{
		CMPMenuHub *pMPMenuHub = pFlashFrontEnd->GetMPMenu();
		if (pMPMenuHub)
		{
			CUIScoreboard *pScoreboard = pMPMenuHub->GetScoreboardUI();
			if (pScoreboard)
			{
				pScoreboard->ClearLastMatchResults();
			}
		}
	}
#endif

	m_isMidGameLeaving = false;
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoSetupDedicatedServer(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId, bool &bTaskStartedOut)
{
	CryLog("CGameLobby::DoSetupDedicatedServer() pLobby=%p", this);

	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_SetupDedicatedServer);

	ECryLobbyError error = eCLE_Success;

	if (m_gameLobbyMgr->IsPrimarySession(this))
	{
		const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + (5 * CryLobbyPacketUINT32Size);
		CCryLobbyPacket packet;

		if (packet.CreateWriteBuffer(MaxBufferSize))
		{
			uint32 gameRulesHash = GameLobbyData::ConvertGameRulesToHash(m_currentGameRules.c_str());
			uint32 mapHash = GameLobbyData::ConvertMapToHash(m_currentLevelName.c_str());

			float timeLimit = g_pGameCVars->g_timelimit;
			int scoreLimit = g_pGameCVars->g_scoreLimit;
			int numSlots = m_sessionData.m_numPublicSlots + m_sessionData.m_numPrivateSlots;

			packet.StartWrite(eGUPD_SetupDedicatedServer, true);
			packet.WriteUINT32(gameRulesHash);
			packet.WriteUINT32(mapHash);
			packet.WriteUINT32((uint32)timeLimit);
			packet.WriteUINT32(scoreLimit);
			packet.WriteUINT32(numSlots);

			error = pMatchMaking->SessionSetupDedicatedServer(m_currentSession, &packet, &taskId, MatchmakingSetupDedicatedServerCallback, this);
		}
		else
		{
			// technically, if we get here then we've failed to allocate memory for the packet buffer
			error = eCLE_InternalError;
		}

		bTaskStartedOut = true;
	}

	return error;
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingSetupDedicatedServerCallback(CryLobbyTaskID taskID, ECryLobbyError error, uint32 ip, uint16 port, void* pArg)
{
	CryLog("CGameLobby::MatchmakingSetUpDedicatedServerCallback pLobby %p error %d", pArg, error);

	CGameLobby *pLobby = static_cast<CGameLobby*>(pArg);

	if(pLobby->NetworkCallbackReceived(taskID, error))
	{
		cry_sprintf(pLobby->m_joinCommand, sizeof(pLobby->m_joinCommand), "connect <session>%08X,%d.%d.%d.%d:%d", pLobby->m_currentSession, ((uint8*)&ip)[0], ((uint8*)&ip)[1], ((uint8*)&ip)[2], ((uint8*)&ip)[3], port);

		pLobby->m_dedicatedserverip = ip;
		pLobby->m_dedicatedserverport = port;

		pLobby->m_bAllocatedServer = true;

		ICryMatchMaking *pMatchMaking = gEnv->pLobby->GetMatchMaking();
		pLobby->m_allocatedServerUID = pMatchMaking->GetHostConnectionUID(pLobby->m_currentSession);
		pLobby->m_allocatedServerUID.m_uid = 0;

		// must be done after server uid is setup
		pLobby->SendPacket(eGUPD_TeamBalancingSetup, pLobby->m_allocatedServerUID); // not overly sure if it's ok to do this here
	}
	else if(error != eCLE_TimeOut && error != eCLE_NoServerAvailable)
	{
		CRY_ASSERT(pLobby->m_state == eLS_JoinSession);

		// return to a sensible state
		pLobby->SetState(eLS_Lobby);

		// we've failed to get a server, starting the session at this point will not end well
		pLobby->CancelLobbyTask(CLobbyTaskQueue::eST_SessionStart);
	}
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingDedicatedServerSetup(UCryLobbyEventData eventData, void *userParam)
{
	CryLog("CGameLobby::MatchmakingDedicatedServerSetup pLobby %p", userParam);

	CRY_ASSERT(gEnv->IsDedicated());

	SCryLobbyDedicatedServerSetupData *pSetupData = eventData.pDedicatedServerSetupData;
	CCryLobbyPacket *pPacket = pSetupData->pPacket;

	if(pPacket)
	{
		CGameLobby *pLobby = static_cast<CGameLobby*>(userParam);

		uint32 packetType = pPacket->StartRead();

		CRY_ASSERT(packetType == eGUPD_SetupDedicatedServer);

		const char *pGameRules = GameLobbyData::GetGameRulesFromHash(pPacket->ReadUINT32());
		const char *pMap = GameLobbyData::GetMapFromHash(pPacket->ReadUINT32());
		const float timeLimit = (float)pPacket->ReadUINT32();
		const int scoreLimit = pPacket->ReadUINT32();
		const int numSlots = pPacket->ReadUINT32();

		CRY_ASSERT(strcmp(pGameRules, GameLobbyData::g_sUnknown));
		CRY_ASSERT(strcmp(pMap, GameLobbyData::g_sUnknown));
		CRY_ASSERT(pLobby->m_currentSession == CrySessionInvalidHandle);

		pLobby->GameRulesChanged(pGameRules);

		ICVar *pCVar = gEnv->pConsole->GetCVar("g_timelimit");
		if (pCVar)
		{
			pCVar->Set(timeLimit);
		}

		pCVar = gEnv->pConsole->GetCVar("g_scoreLimit");
		if (pCVar)
		{
			pCVar->Set(scoreLimit);
		}

		pLobby->m_currentSession = pSetupData->session;

		pLobby->m_teamBalancing.Reset();
		pLobby->m_teamBalancing.SetLobbyPlayerCounts(numSlots);

		g_pGame->GetIGameFramework()->ExecuteCommandNextFrame(string().Format("map %s s nb", pMap).c_str());
	}
}

//-------------------------------------------------------------------------
ECryLobbyError CGameLobby::DoReleaseDedicatedServer(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId, bool &bTaskStartedOut)
{
	CryLog("CGameLobby::DoReleaseDedicatedServer() pLobby=%p", this);

	CRY_ASSERT(m_taskQueue.GetCurrentTask() == CLobbyTaskQueue::eST_ReleaseDedicatedServer);

	ECryLobbyError error = eCLE_Success;

	if (m_gameLobbyMgr->IsPrimarySession(this))
	{
		error = pMatchMaking->SessionReleaseDedicatedServer(m_currentSession, &taskId, MatchmakingReleaseDedicatedServerCallback, this);
		bTaskStartedOut = true;
	}

	return error;
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingReleaseDedicatedServerCallback(CryLobbyTaskID taskId, ECryLobbyError error, void *pArg)
{
	CryLog("CGameLobby::MatchmakingReleaseDedicatedServerCallback pLobby %p error %d", pArg, error);

	CGameLobby *pLobby = static_cast<CGameLobby*>(pArg);

	pLobby->NetworkCallbackReceived(taskId, error);

	if(error != eCLE_TimeOut)	// timeout means we'll try again, otherwise we're done
	{
		pLobby->m_bAllocatedServer = false;
		pLobby->m_allocatedServerUID = CryMatchMakingInvalidConnectionUID;
	}
}

//-------------------------------------------------------------------------
void CGameLobby::MatchmakingDedicatedServerRelease(UCryLobbyEventData eventData, void *userParam)
{
	CryLog("CGameLobby::MatchmakingDedicatedServerRelease pLobby %p", userParam);

	CGameLobby *pLobby = static_cast<CGameLobby*>(userParam);
	SCryLobbyDedicatedServerReleaseData *pReleaseData = eventData.pDedicatedServerReleaseData;

	CRY_ASSERT(gEnv->IsDedicated());

	pLobby->m_teamBalancing.Reset();
	pLobby->m_currentSession = CrySessionInvalidHandle;

	pLobby->m_taskQueue.AddTask(CLobbyTaskQueue::eST_Unload, false);
}

//-------------------------------------------------------------------------
bool CGameLobby::IsSessionMigratable()
{
	bool result = false;
#if GAME_LOBBY_ALLOW_MIGRATION
	if(!gEnv->IsDedicated() && IsCurrentlyInSession())
	{
		result = true;
	}
#endif
	return result;
}

//-------------------------------------------------------------------------
bool CGameLobby::ShouldMigrateNub()
{
	CRY_ASSERT(GAME_LOBBY_ALLOW_MIGRATION);	// if we get this far, then migration is already happening, confirm we're good here

	// ideally, this information should come from the session data, but we need a title id to be able to update
	// and query this
	bool bShouldMigrateNub = (s_pGameLobbyCVars->gl_getServerFromDedicatedServerArbitrator == 0);

	if(bShouldMigrateNub)
	{
		bShouldMigrateNub = (m_state == eLS_Game);
	}

	return bShouldMigrateNub;
}

//-------------------------------------------------------------------------
void CGameLobby::SetQuickMatchRanked( bool bIsRanked )
{
	CryLog("CGameLobby::SetQuickMatchRanked() bIsRanked=%s", bIsRanked ? "true" : "false");
	m_quickMatchRanked = bIsRanked;
}

//-------------------------------------------------------------------------
void CGameLobby::SetQuickMatchGameRules( const char *pGameRules )
{
	CryLog("CGameLobby::SetQuickMatchGameRules() pGameRules=%s", pGameRules);
	m_quickMatchGameRules = pGameRules;
}

//-------------------------------------------------------------------------
SSessionNames::SSessionName* CGameLobby::GetSessionNameByChannelId(int channelId)
{
	SSessionNames::SSessionName *pResult = NULL;

	CrySessionHandle mySession = m_currentSession;

	if (mySession != CrySessionInvalidHandle)
	{
		if (gEnv->pNetwork)
		{
			ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
			if (pLobby)
			{
				ICryMatchMaking *pMatchMaking = pLobby->GetMatchMaking();
				if (pMatchMaking)
				{
					SCryMatchMakingConnectionUID temp = pMatchMaking->GetConnectionUIDFromGameSessionHandleAndChannelID(mySession, channelId);
					int playerIndex = m_nameList.Find(temp);
					if (playerIndex != SSessionNames::k_unableToFind)
					{
						pResult = &m_nameList.m_sessionNames[playerIndex];
					}
				}
			}
		}
	}

	return pResult;
}

bool CGameLobby::GetSpectatorStatusFromChannelId( int channelId )
{
	CrySessionHandle mySession = m_currentSession;

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby && mySession != CrySessionInvalidHandle)
	{
		if(ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking())
		{
			const SCryMatchMakingConnectionUID conID = pMatchmaking->GetConnectionUIDFromGameSessionHandleAndChannelID(mySession, channelId);
			const SSessionNames::SSessionName *pSessionName = m_nameList.GetSessionName(conID, false);
			if (pSessionName)
			{
				//return pSessionName->GetSpectatorStatus();
			}
		}
	}

	return false;
}

void CGameLobby::SetLocalSpectatorStatus( bool spectator )
{
	g_pGameCVars->g_spectatorOnly = spectator;
	m_taskQueue.AddTask(CLobbyTaskQueue::eST_SetLocalUserData, true);
}

#if defined(DEDICATED_SERVER)
void CGameLobby::UpdatePingKicker(float frameTime)
{
	int pingLimit = s_pGameLobbyCVars->g_pingLimit;
	if (pingLimit)
	{
		m_timeSinceLastPingCheck += frameTime;
		if (m_timeSinceLastPingCheck > 1.f)
		{
			ICryMatchMaking *pMatchMaking = gEnv->pNetwork->GetLobby()->GetMatchMaking();

			if (pMatchMaking)
			{
				CTimeValue now = gEnv->pTimer->GetFrameStartTime();

				CGameRules *pGameRules = g_pGame->GetGameRules();

				int numPlayerPings = m_playerPingInfo.size();
				for (int i = 0; i < numPlayerPings; ++ i)
				{
					SPlayerPingInfo &info = m_playerPingInfo[i];

					bool bCanCheck = true;
					if (pGameRules)
					{
						if (pGameRules->GetActorByChannelId(info.m_channelId) == NULL)
						{
							// Don't check ping while the client is loading
							bCanCheck = false;
						}
					}

					if (bCanCheck)
					{
						SCryMatchMakingConnectionUID conUID = GetConnectionUIDFromChannelID(info.m_channelId);
						CryPing ping = CRYLOBBY_INVALID_PING;

						ECryLobbyError result = pMatchMaking->GetSessionPlayerPing( conUID, &ping );
						if (result == eCLE_Success)
						{
							CryFixedStringT<CRYLOBBY_USER_NAME_LENGTH> playerName;
							GetPlayerNameFromChannelId(info.m_channelId, playerName);
							if (ping > pingLimit)
							{
								if (info.m_bPingExceedsLimit)
								{
									if (now.GetDifferenceInSeconds(info.m_timePingExceededLimit) > s_pGameLobbyCVars->g_pingLimitTimer)
									{
										CryLogAlways("Kicking player '%s' as their ping is too high (ping=%u)", playerName.c_str(), ping);

										CryUserID userId = GetUserIDFromChannelID(info.m_channelId);
										if (userId != CryUserInvalidID)
										{
											pMatchMaking->Kick(&userId, eDC_KickedHighPing);
										}
									}
								}
								else
								{
									CryLogAlways("Player '%s' exceeding ping limit (ping=%u)", playerName.c_str(), ping);

									info.m_bPingExceedsLimit = true;
									info.m_timePingExceededLimit = now;
								}
							}
							else
							{
								if (info.m_bPingExceedsLimit)
								{
									CryLogAlways("Player '%s' no longer exceeding ping limit (ping=%u)", playerName.c_str(), ping);

									info.m_bPingExceedsLimit = false;
								}
							}
						}
					}
				}
			}

			m_timeSinceLastPingCheck = 0.f;
		}
	}
}
#endif

//-------------------------------------------------------------------------
bool CGameLobby::IsRankedGame() const
{
	CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
	return !IsPrivateGame() && !IsPasswordedGame() && (!pPlaylistManager || !pPlaylistManager->IsUsingCustomVariant());
}

//-------------------------------------------------------------------------
void CGameLobby::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
		{
			m_bCanAbortLoading = false;
		}
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_ERROR:
		{
			m_bAllowPakPrecaching = true;
			ClearLoggedPrecachedPaks("Loading Error");
		}
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			m_bAllowPakPrecaching = false;
			ClearLoggedPrecachedPaks("Loading Finished");
		}
		break;
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			m_bAllowPakPrecaching = true;
		}
		break;
	}
}

//-------------------------------------------------------------------------
bool CGameLobby::IsAutoStartingGame()  const
{
	return m_bIsAutoStartingGame;
}

//-------------------------------------------------------------------------
void CGameLobby::UpdateTeams()
{
	if (m_server && UseLobbyTeamBalancing())
	{
		bool bChanged = false;
		int numPlayers = m_nameList.Size();
		for (int i = 0; i < numPlayers; ++ i)
		{
			SSessionNames::SSessionName *pPlayer = &m_nameList.m_sessionNames[i];
			uint8 teamId = m_teamBalancing.GetTeamId(pPlayer->m_conId);
			if (teamId != pPlayer->m_teamId)
			{
				pPlayer->m_teamId = teamId;
				bChanged = true;
			}
		}
		if (bChanged)
		{
			SendPacket(eGUPD_SetTeamAndRank);
			m_nameList.m_dirty = true;
		}
	}
}

//-------------------------------------------------------------------------
void CGameLobby::OnPlayerSpawned(int channelId)
{
	SCryMatchMakingConnectionUID uid = GetConnectionUIDFromChannelID(channelId);
	if (uid != CryMatchMakingInvalidConnectionUID)
	{
		m_teamBalancing.OnPlayerSpawned(uid);
	}
}

//-------------------------------------------------------------------------
void CGameLobby::OnPlayerSwitchedTeam( int channelId, int teamId )
{
	SCryMatchMakingConnectionUID uid = GetConnectionUIDFromChannelID(channelId);
	if (uid != CryMatchMakingInvalidConnectionUID)
	{
		m_teamBalancing.OnPlayerSwitchedTeam(uid, (uint8) teamId);
	}
}

//-------------------------------------------------------------------------
bool CGameLobby::AllowForceBalanceTeams() const
{
	// non asymmetric games should always be allowed to force balance if required
	// asymmetric games should only be balanced if not in the second round (2nd round currently is m_replayMapWithTeamsSwitched == false)
	return !m_isAsymmetricGame || m_replayMapWithTeamsSwitched;
}

//-------------------------------------------------------------------------
void CGameLobby::ForceBalanceTeams()
{
	//CryLog("CGameLobby::ForceBalanceTeams isAllowed %d", AllowForceBalanceTeams());

	if(AllowForceBalanceTeams())
	{
		m_teamBalancing.ForceBalanceTeams();
		UpdateTeams();
	}
}

//-------------------------------------------------------------------------
void CGameLobby::AbortLoading()
{
	if (m_bCanAbortLoading)
	{
#if 0 // old frontend
		CFlashFrontEnd *pFFE = g_pGame->GetFlashMenu();
		if (pFFE)
		{
			pFFE->OnLoadingAborted();
		}
#endif
		m_bCanAbortLoading = false;
	}
}

bool CGameLobby::UsePlayerTeamVisualization() const
{
	if(!CGameRulesModulesManager::GetInstance())
		return false;

	 return m_hasValidGameRules && CGameRulesModulesManager::GetInstance()->UsesPlayerTeamVisualization(m_currentGameRules);
}

//-------------------------------------------------------------------------
// Be friendly with uber files
#undef GAME_LOBBY_DO_ENSURE_BEST_HOST
#undef GAME_LOBBY_DO_LOBBY_MERGING
#undef GAME_LOBBY_ALLOW_MIGRATION
