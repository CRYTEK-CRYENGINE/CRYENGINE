// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 3:9:2004   11:25 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameContext.h"
#include "GameServerChannel.h"
#include "GameClientChannel.h"
#include "IActorSystem.h"
#include "IGameSessionHandler.h"
#include "CryAction.h"
#include "GameRulesSystem.h"
#include <CryScriptSystem/ScriptHelpers.h>
#include "GameObjects/GameObject.h"
#include "ScriptRMI.h"
#include <CryPhysics/IPhysics.h>
#include "PhysicsSync.h"
#include "GameClientNub.h"
#include "GameServerNub.h"
#include "ActionGame.h"
#include "ILevelSystem.h"
#include "IPlayerProfiles.h"
#include "VoiceController.h"
#include "BreakReplicator.h"
#include "CVarListProcessor.h"
#include "NetworkCVars.h"
#include "CryActionCVars.h"
#include <CryNetwork/INetwork.h>
#include <CryCore/Platform/IPlatformOS.h>

// context establishment tasks
#include <CryNetwork/NetHelpers.h>
#include "CET_ClassRegistry.h"
#include "CET_CVars.h"
#include "CET_EntitySystem.h"
#include "CET_GameRules.h"
#include "CET_LevelLoading.h"
#include "CET_NetConfig.h"
#include "CET_ClientConnections.h"

#include "NetDebug.h"

#include "NetMsgDispatcher.h"

#define VERBOSE_TRACE_CONTEXT_SPAWNS 0

CGameContext* CGameContext::s_pGameContext = NULL;

#ifdef USE_NETWORK_STALL_TICKER_THREAD
CryLockT<CRYLOCK_RECURSIVE> SScopedTickerLock::s_tickerMutex;
#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD

static const char* hexchars = "0123456789abcdef";

static ILINE string ToHexStr(const char* x, int len)
{
	string out;
	for (int i = 0; i < len; i++)
	{
		uint8 c = x[i];
		out += hexchars[c >> 4];
		out += hexchars[c & 0xf];
	}
	return out;
}

static ILINE bool FromHexStr(string& x)
{
	string out;
	uint8 cur = 0;
	for (int i = 0; i < x.length(); i++)
	{
		int j;
		for (j = 0; hexchars[j]; j++)
			if (hexchars[j] == x[i])
				break;
		if (!hexchars[j])
			return false;
		cur = (cur << 4) | j;
		if (i & 1)
			out += cur;
	}
	x.swap(out);
	return true;
}

CGameContext::CGameContext(CCryAction* pFramework, CScriptRMI* pScriptRMI, CActionGame* pGame) :
	m_pFramework(pFramework),
	m_pPhysicsSync(0),
	m_pNetContext(0),
	m_pEntitySystem(0),
	m_pGame(pGame),
	m_isInLevelLoad(false),
	m_pScriptRMI(pScriptRMI),
	m_bStarted(false),
	m_bIsLoadingSaveGame(false),
	m_bHasSpawnPoint(true),
	m_bAllowSendClientConnect(false),
	m_loadFlags(eEF_LoadNewLevel),
	m_resourceLocks(0),
	m_removeObjectUnlocks(0),
	m_broadcastActionEventInGame(-1)
#if ENABLE_NETDEBUG
	, m_pNetDebug(0)
#endif
{
	CRY_ASSERT(s_pGameContext == NULL);
	s_pGameContext = this;
	gEnv->pConsole->AddConsoleVarSink(this);

	m_pScriptRMI->SetContext(this);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	m_pVoiceController = NULL;
#endif

	// default (permissive) flags until we are initialized correctly
	// ensures cvars can be changed just at the editor start
	m_flags = eGSF_LocalOnly | eGSF_Server | eGSF_Client;

	//	Recently the loading has changed on C2 MP to get clients to load in parallel with the server loading. This unfortunately means that
	//	the message that turns on immersive multiplayer arrives _after_ rigid bodies are created. So they don't have the flag set, and
	//	they don't work. We have one day left to fix as much stuff as possible, so this is being done instead of a nice fix involving
	//	changing which network message things are sent in and such.
	m_flags |= eGSF_ImmersiveMultiplayer;

	m_pPhysicalWorld = gEnv->pPhysicalWorld;
	m_pGame->AddGlobalPhysicsCallback(eEPE_OnCollisionLogged, OnCollision, 0);

	gEnv->pNetwork->AddHostMigrationEventListener(this, "CGameContext", ELPT_PostEngine);

	if (!CCryActionCVars::Get().g_syncClassRegistry)
	{
		IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
		IEntityClass* pClass;
		pClassRegistry->IteratorMoveFirst();
		while (pClass = pClassRegistry->IteratorNext())
		{
			m_classRegistry.RegisterClassName(pClass->GetName(), ~uint16(0));
		}
	}
}

CGameContext::~CGameContext()
{
	gEnv->pNetwork->RemoveHostMigrationEventListener(this);

	gEnv->pConsole->RemoveConsoleVarSink(this);

	IScriptSystem* pSS = gEnv->pScriptSystem;
	for (DelegateCallbacks::iterator iter = m_delegateCallbacks.begin(); iter != m_delegateCallbacks.end(); ++iter)
	{
		pSS->ReleaseFunc(iter->second);
	}

	if (m_pEntitySystem)
		m_pEntitySystem->RemoveSink(this);

	m_pGame->RemoveGlobalPhysicsCallback(eEPE_OnCollisionLogged, OnCollision, 0);

	m_pScriptRMI->SetContext(NULL);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	delete m_pVoiceController;
#endif

#if ENABLE_NETDEBUG
	SAFE_DELETE(m_pNetDebug);
#endif

	s_pGameContext = NULL;
}

void CGameContext::Init(INetContext* pNetContext)
{
	CRY_ASSERT(!m_pEntitySystem);
	m_pEntitySystem = gEnv->pEntitySystem;
	CRY_ASSERT(m_pEntitySystem);

	m_pEntitySystem->AddSink(this, IEntitySystem::AllSinkEvents);
	m_pNetContext = pNetContext;

#if ENABLE_NETDEBUG
	m_pNetDebug = new CNetDebug();
	assert(m_pNetDebug);
#endif

	m_pNetContext->DeclareAspect("GameClientDynamic", eEA_GameClientDynamic, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerDynamic", eEA_GameServerDynamic, 0);
	m_pNetContext->DeclareAspect("GameClientStatic", eEA_GameClientStatic, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerStatic", eEA_GameServerStatic, eAF_ServerManagedProfile);
	m_pNetContext->DeclareAspect("Physics", eEA_Physics, eAF_Delegatable | eAF_ServerManagedProfile | eAF_TimestampState);
	m_pNetContext->DeclareAspect("Script", eEA_Script, 0);

	m_pNetContext->DeclareAspect("GameClientA", eEA_GameClientA, eAF_Delegatable);
	m_pNetContext->DeclareAspect("PlayerUpdate", eEA_Aspect31, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerA", eEA_GameServerA, 0);
	m_pNetContext->DeclareAspect("GameClientB", eEA_GameClientB, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerB", eEA_GameServerB, 0);
	m_pNetContext->DeclareAspect("GameClientC", eEA_GameClientC, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerC", eEA_GameServerC, 0);
	m_pNetContext->DeclareAspect("GameClientD", eEA_GameClientD, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientE", eEA_GameClientE, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientF", eEA_GameClientF, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientG", eEA_GameClientG, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientH", eEA_GameClientH, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientI", eEA_GameClientI, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientJ", eEA_GameClientJ, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerD", eEA_GameServerD, 0);
	m_pNetContext->DeclareAspect("GameClientK", eEA_GameClientK, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientL", eEA_GameClientL, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientM", eEA_GameClientM, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientN", eEA_GameClientN, eAF_Delegatable | eAF_ServerControllerOnly);
	m_pNetContext->DeclareAspect("GameClientO", eEA_GameClientO, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameClientP", eEA_GameClientP, eAF_Delegatable);
	m_pNetContext->DeclareAspect("GameServerE", eEA_GameServerE, 0);

	//m_pNetContext->RegisterServerControlledFile("Game/motd.txt");
	//m_pNetContext->RegisterServerControlledFile("Game/Libs/ActionGraphs/yawn.xml");
}

void CGameContext::SetContextInfo(unsigned flags, uint16 port, const char* connectionString)
{
	m_flags = flags;
	m_port = port;
	m_connectionString = connectionString;

	if (!(flags & eGSF_LocalOnly))
		m_pBreakReplicator.reset(new CBreakReplicator(this));

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (!gEnv->IsDedicated())
	{
		m_pVoiceController = new CVoiceController(this);
		if (!m_pVoiceController->Init())
		{
			delete m_pVoiceController;
			m_pVoiceController = NULL;
		}
		else
			m_pVoiceController->Enable(m_pFramework->IsVoiceRecordingEnabled());
	}
#endif
}

//
// IGameContext
//

void CGameContext::AddLoadLevelTasks(IContextEstablisher* pEst, bool isServer, int flags, bool** ppLoadingStarted, int establishedToken, bool bChannelIsMigrating)
{
	bool usingLobbyHints = false;

	ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby && gEnv->bMultiplayer)
	{
		const char* pLevelName = pLobby->GetCryEngineLevelNameHint();
		const char* pRulesName = pLobby->GetCryEngineRulesNameHint();
		if (strlen(pLevelName) != 0 && strlen(pRulesName) != 0)
		{
			usingLobbyHints = true;
		}
	}

	if (!bChannelIsMigrating)
	{
		if (gEnv->IsClient() && (flags & eEF_LoadNewLevel) == 0)
		{
			AddActionEvent(pEst, eCVS_Begin, eAE_resetBegin);
			AddActionEvent(pEst, eCVS_Begin, SActionEvent(eAE_resetProgress, 0));
		}

		const bool loadingNewLevel = (flags & eEF_LoadNewLevel) != 0;
		if (flags & eEF_LevelLoaded)
		{
			AddRandomSystemReset(pEst, eCVS_Begin, loadingNewLevel);
		}
		if (flags & eEF_LoadNewLevel)
		{
			AddPrepareLevelLoad(pEst, usingLobbyHints ? eCVS_Initial : eCVS_Begin);
		}
		if (flags & eEF_LevelLoaded)
		{
			bool resetSkipPlayers = false && ((flags & eEF_LoadNewLevel) == 0) && isServer;
			bool resetSkipGamerules = ((flags & eEF_LoadNewLevel) == 0);
			AddEntitySystemReset(pEst, eCVS_Begin, resetSkipPlayers, resetSkipGamerules);
		}
		if (flags & eEF_LoadNewLevel)
			AddClearPlayerIds(pEst, eCVS_Begin);
		AddSetValue(pEst, usingLobbyHints ? eCVS_Initial : eCVS_EstablishContext, &m_isInLevelLoad, true, "BeginLevelLoad");
		AddInitImmersiveness(pEst, eCVS_EstablishContext);
		if (flags & eEF_LoadNewLevel)
		{
			//		We used to add a task create the game rules here using AddGameRulesCreation.
			//		That has moved into the LoadLevel task as of 25/5/10 - peter.
			AddLoadLevel(pEst, usingLobbyHints ? eCVS_Initial : eCVS_EstablishContext, ppLoadingStarted);
		}
		else
		{
			AddFakeSpawn(pEst, eCVS_EstablishContext, eFS_GameRules | eFS_Opt_Rebind);
			AddLoadLevelEntities(pEst, eCVS_EstablishContext);
			/*
			   if (isServer)
			   {
			    AddSetValue( pEst, eCVS_EstablishContext, &m_isInLevelLoad, false, "PauseLevelLoad" );
			    AddFakeSpawn( pEst, eCVS_EstablishContext, eFS_Players | eFS_Opt_Rebind );
			    AddSetValue( pEst, eCVS_EstablishContext, &m_isInLevelLoad, true, "RestartLevelLoad" );
			   }
			 */
		}

		// Only reset areas right after the level loading task!
		if (!gEnv->IsEditor() && gEnv->pSystem->IsSerializingFile() != 2)
		{
			AddResetAreas(pEst, usingLobbyHints ? eCVS_Initial : eCVS_EstablishContext);
		}

		AddSetValue(pEst, eCVS_EstablishContext, &m_isInLevelLoad, false, "EndLevelLoad");
		AddEstablishedContext(pEst, eCVS_EstablishContext, establishedToken);
	}
}

void CGameContext::AddLoadingCompleteTasks(IContextEstablisher* pEst, int flags, bool* pLoadingStarted, bool bChannelIsMigrating)
{
	if (HasContextFlag(eGSF_Server) && !bChannelIsMigrating)
	{
		AddPauseGame(pEst, eCVS_Begin, true, false);
	}

	if (!bChannelIsMigrating)
	{
		if (flags & eEF_LoadNewLevel)
		{
			SEntityEvent startLevelEvent(ENTITY_EVENT_START_LEVEL);
			AddEntitySystemEvent(pEst, eCVS_InGame, startLevelEvent);
		}

		if (!gEnv->IsEditor() && gEnv->pSystem->IsSerializingFile() != 2) /*  && (flags&eEF_LoadNewLevel)==0) */
		{
			AddResetAreas(pEst, eCVS_InGame);
		}

		if ((flags & eEF_LoadNewLevel) == 0)
		{
			AddGameRulesReset(pEst, eCVS_InGame);
		}

		if (!gEnv->IsEditor() && (flags & eEF_LoadNewLevel))
		{
			AddLoadingComplete(pEst, eCVS_InGame, pLoadingStarted);
		}

		if (HasContextFlag(eGSF_Server))
		{
			AddPauseGame(pEst, eCVS_InGame, false, false);
		}
	}

	AddWaitForPrecachingToFinish(pEst, eCVS_InGame, &m_bStarted);

	if (!bChannelIsMigrating)
	{
		if (HasContextFlag(eGSF_Client))
		{
			if (CGameClientNub* pGCN = CCryAction::GetCryAction()->GetGameClientNub())
			{
				if (CGameClientChannel* pGCC = pGCN->GetGameClientChannel())
				{
					pGCC->AddUpdateLevelLoaded(pEst);
				}
			}
		}
	}
}

class CCET_WaitForPendingConnections : public CCET_Base
{
public:
	const char* GetName() { return ""; }

	enum EWaitState
	{
		EW_INITIAL_TICK,
		EW_WAIT_FOR_PENDING,
		EW_WAIT_UNTIL_CONNECTED
	};

#define DEFAULT_WAIT_LENGTH    (10.f)
#define NEW_CLIENT_WAIT_LENGTH (5.f)

	CCET_WaitForPendingConnections(CActionGame* cag, CCryAction* cca) : m_state(EW_INITIAL_TICK), m_cag(cag), m_cca(cca), m_start(0.f), m_prevNumConnections(0), m_waitLen(DEFAULT_WAIT_LENGTH) {}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		if (m_cag->GetServerNetNub())
		{
			bool havePendingConnections = m_cag->GetServerNetNub()->HasPendingConnections();

			if (m_state == EW_INITIAL_TICK)
			{
				m_start = gEnv->pTimer->GetAsyncTime();
				m_state = EW_WAIT_FOR_PENDING;
			}

			float diff = (gEnv->pTimer->GetAsyncTime() - m_start).GetSeconds();

			switch (m_state)
			{
			case EW_WAIT_FOR_PENDING:
				CryLog("CEst -- Waiting until we have pending connections!");
				if (havePendingConnections)
				{
					CryLog("CEst -- We now have pending connections!");
					m_state = EW_WAIT_UNTIL_CONNECTED;
				}
				break;

			case EW_WAIT_UNTIL_CONNECTED:
				{
					IGameSessionHandler* gsh = m_cca->GetIGameSessionHandler();
					int numExpected = 1;

					if (gsh)
					{
						numExpected = gsh->GetNumberOfExpectedClients();
					}

					int numChannels = m_cag->GetServerNetNub()->GetNumChannels();

					if (numChannels > m_prevNumConnections)
					{
						m_prevNumConnections = numChannels;
						// wait for this client to tick through states
						m_waitLen = max(m_waitLen, (diff + NEW_CLIENT_WAIT_LENGTH));
					}

					//CryLog("CEst -- num channels %d, expecting %d!", numChannels, numExpected);

					if (numChannels >= numExpected && !havePendingConnections)
					{
						CryLog("CEst -- We have NO pending connections remaining!");
						//m_start = gEnv->pTimer->GetAsyncTime();
						return eCETR_Ok;
					}
					break;
				}
			}

			if (diff < m_waitLen)
				return eCETR_Wait;
			else
				CryLog("CEst -- Waited more than %f seconds, continuing...", m_waitLen);
		}

		CryLog("CEst -- No pending connections, we have a go!");
		return eCETR_Ok;
	}

private:
	EWaitState   m_state;
	CActionGame* m_cag;
	CCryAction*  m_cca;
	CTimeValue   m_start;
	int          m_prevNumConnections;
	float        m_waitLen;
};

void AddWaitForPendingConnections(IContextEstablisher* pEst, EContextViewState state, CActionGame* cag, CCryAction* cca)
{
	if (gEnv->bMultiplayer)
	{
		//pEst->AddTask(state, new CCET_WaitForPendingConnections(cag, cca));
	}
}

bool CGameContext::InitGlobalEstablishmentTasks(IContextEstablisher* pEst, int establishedToken)
{
	AddSetValue(pEst, eCVS_Begin, &m_bStarted, false, "GameNotStarted");
	AddWaitForPendingConnections(pEst, eCVS_Begin, m_pGame, m_pFramework);
	if (gEnv->IsEditor())
	{
		AddEstablishedContext(pEst, eCVS_EstablishContext, establishedToken);
		AddFakeSpawn(pEst, eCVS_EstablishContext, eFS_All);
	}
	else if (HasContextFlag(eGSF_Server))
	{
		bool* pLoadingStarted = 0;
		AddLoadLevelTasks(pEst, true, m_loadFlags, &pLoadingStarted, establishedToken, false);
		// here is the special handling for playing back a demo session in dedicated mode - both eGSF_Server and eGSF_Client are specified,
		// but we need to perform LoadingCompleteTasks while skipping all the channel establishment tasks
		if (gEnv->IsDedicated() || !HasContextFlag(eGSF_Client))
			AddLoadingCompleteTasks(pEst, m_loadFlags, pLoadingStarted, false);
		if (HasContextFlag(eGSF_Server))
		{
			AddEntitySystemGameplayStart(pEst, eCVS_InGame);
		}
	}
	return true;
}

bool CGameContext::InitChannelEstablishmentTasks(IContextEstablisher* pEst, INetChannel* pChannel, int establishedSerial)
{
	// local channels on a dedicated server are dummy connection for dedicated server demo recording
	// skip all the channel establishment tasks (esp. player spawning and setup - OnClientConnect/EnteredGame)
	if (gEnv->IsDedicated() && pChannel->IsLocal())
		return true;

	// for DemoPlayback, we want the local player spawned as a spectator for the demo session, so
	// the normal channel establishment tasks are needed

	bool isLocal = pChannel->IsLocal();
	bool isServer = ((CGameChannel*)pChannel->GetGameChannel())->IsServer();
	bool isClient = !isServer;
	bool isLevelLoaded = false;
	bool bIsChannelMigrating = pChannel->IsMigratingChannel();

	CGameServerChannel* pServerChannel = 0;
	if (isServer)
	{
		pServerChannel = (CGameServerChannel*)pChannel->GetGameChannel();
		isLevelLoaded = pServerChannel->CheckLevelLoaded();
	}
	else
	{
		if (CGameClientNub* pGCN = CCryAction::GetCryAction()->GetGameClientNub())
			if (CGameClientChannel* pGCC = pGCN->GetGameClientChannel())
				isLevelLoaded = pGCC->CheckLevelLoaded();
	}

	bool isReset = stl::member_find_and_erase(m_resetChannels, pChannel);

	if (isReset && !isLevelLoaded)
		isReset = false;

	int flags = (isLevelLoaded ? eEF_LevelLoaded : 0) | (isReset ? 0 : eEF_LoadNewLevel);

	if (CCryActionCVars::Get().g_syncClassRegistry)
	{
		if (isServer && !isReset && (0 == (flags & eEF_LevelLoaded)))
		{
			AddRegisterAllClasses(pEst, eCVS_Begin, &m_classRegistry);
		}
	}

	// please keep the following bare minimum channel establishment tasks first, since DemoRecorderChannel will make
	// an early out right after these
	if (isServer && !isLocal)
	{
		if (!isReset)
		{
			std::vector<SSendableHandle>* pWaitFor = 0;
			if (0 == (flags & eEF_LevelLoaded))
			{
				// Don't send all the cvar's again - makes it ages to join a network game
				// Might want to do this on a PC build - need to discuss it
#if ENABLE_CVARS_FLUSHING
				AddCVarSync(pEst, eCVS_Begin, static_cast<CGameServerChannel*>(pChannel->GetGameChannel()));
#endif
				if (CCryActionCVars::Get().g_syncClassRegistry)
				{
					AddSendClassRegistration(pEst, eCVS_Begin, &m_classRegistry, &pWaitFor);
				}
				else
				{
					AddSendClassHashRegistration(pEst, eCVS_Begin, &m_classRegistry, &pWaitFor);
				}
			}
			if (!bIsChannelMigrating)
			{
				AddSendGameType(pEst, eCVS_Begin, &m_classRegistry, pWaitFor);
			}
		}
		else
		{
			//AddSendResetMap( pEst, eCVS_Begin );
		}
	}

	pChannel->AddWaitForFileSyncComplete(pEst, eCVS_Begin);

	// do NOT add normal channel establishment tasks here, otherwise it breaks DemoRecorder

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
	if (isClient)
	{
		AddClientTimeSync(pEst, eCVS_Begin);
	}
#endif

	// should skip all other channel establishment tasks for DemoRecorderChannel (local channel ID is 0)
	if (pChannel->GetLocalChannelID() == 0)
		return true;

	// add normal channel establishment tasks here

	if (isServer)
	{
		if (gEnv->IsEditor())
			AddWaitValue(pEst, eCVS_PostSpawnEntities, &m_bAllowSendClientConnect, true, "WaitForAllowSendClientConnect", 20.0f);
		AddOnClientConnect(pEst, eCVS_PostSpawnEntities, isReset);
		AddOnClientEnteredGame(pEst, eCVS_InGame, isReset);
		if (pServerChannel->IsOnHold())
			AddClearOnHold(pEst, eCVS_InGame);

		AddDelegateAuthorityToClientActor(pEst, eCVS_InGame);
	}

	bool* pLoadingStarted = 0;
	if (isClient && !HasContextFlag(eGSF_Server))
	{
		AddLoadLevelTasks(pEst, false, flags, &pLoadingStarted, establishedSerial, bIsChannelMigrating);
	}

	if (isClient)
		AddLoadingCompleteTasks(pEst, flags, pLoadingStarted, bIsChannelMigrating);

	if (isClient && !HasContextFlag(eGSF_Server))
	{
		AddEntitySystemGameplayStart(pEst, eCVS_InGame);
	}

	if (isClient && !gEnv->IsEditor() && !gEnv->bMultiplayer && !(m_flags & eGSF_DemoPlayback))
		AddInitialSaveGame(pEst, eCVS_InGame);

	if (isClient)
	{
		if (isReset)
		{
			for (int i = eCVS_EstablishContext; i < eCVS_PostSpawnEntities; i++)
				AddActionEvent(pEst, eCVS_Begin, SActionEvent(eAE_resetProgress, 100 * (i - eCVS_EstablishContext) / (eCVS_PostSpawnEntities - eCVS_EstablishContext)));
			AddActionEvent(pEst, eCVS_InGame, eAE_resetEnd);
		}
		else
		{
			AddSetValue(pEst, eCVS_InGame, &m_broadcastActionEventInGame, 4, "BroadcastInGame");
		}
	}

	// AlexL: Unpause the Game when Editor is started, but no Level isLevelLoaded
	//        this ensures that CTimer's ETIMER_GAME is updated and systems relying
	//        on it work correctly (CharEditor)
	if (gEnv->IsEditor() && HasContextFlag(eGSF_Server))
		AddPauseGame(pEst, eCVS_Begin, false, false);

	if (pServerChannel)
		pServerChannel->AddUpdateLevelLoaded(pEst);

	return true;
}

void CGameContext::ResetMap(bool isServer)
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->Reset();

	if (!isServer && HasContextFlag(eGSF_Server))
		return;

	m_loadFlags = EstablishmentFlags_ResetMap;
	m_resetChannels.clear();

	if (CGameClientNub* pGCN = CCryAction::GetCryAction()->GetGameClientNub())
		if (CGameClientChannel* pGCC = pGCN->GetGameClientChannel())
			m_resetChannels.insert(pGCC->GetNetChannel());

	if (CGameServerNub* pGSN = CCryAction::GetCryAction()->GetGameServerNub())
		if (TServerChannelMap* m = pGSN->GetServerChannelMap())
			for (TServerChannelMap::iterator iter = m->begin(); iter != m->end(); ++iter)
			{
				if (iter->second->GetNetChannel())//channel can be on hold thus not having net channel currently
				{
					CGameClientChannel::SendResetMapWith(SNoParams(), iter->second->GetNetChannel());
					m_resetChannels.insert(iter->second->GetNetChannel());
				}
			}

	if (isServer)
		m_pNetContext->ChangeContext();
}

/*
   void CGameContext::EndContext()
   {
   // TODO: temporary
   IActionMap* pActionMap = 0;
   IPlayerProfileManager* pPPMgr = m_pFramework->GetIPlayerProfileManager();
   if (pPPMgr)
   {
    int userCount = pPPMgr->GetUserCount();
    if (userCount > 0)
    {
      IPlayerProfileManager::SUserInfo info;
      pPPMgr->GetUserInfo(0, info);
      IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
      if (pProfile)
      {
        pActionMap = pProfile->GetActionMap("default");
        if (pActionMap == 0)
          GameWarning("[PlayerProfiles] CGameContext::EndContext: User '%s' has no actionmap 'default'!", info.userId);
      }
      else
        GameWarning("[PlayerProfiles] CGameContext::EndContext: User '%s' has no active profile!", info.userId);
    }
    else
    {
      ;
      // GameWarning("[PlayerProfiles] CGameContext::EndContext: No users logged in");
    }
   }

   if (pActionMap == 0)
   {
    // use action map without any profile stuff
    IActionMapManager * pActionMapMan = m_pFramework->GetIActionMapManager();
    if (pActionMapMan)
    {
      pActionMap = pActionMapMan->GetActionMap("default");
    }
   }

   if (pActionMap)
    pActionMap->SetActionListener(0);
   // ~TODO: temporary

   if (!HasContextFlag(eGSF_NoGameRules))
   {
    m_pEntitySystem->Reset();
    for (int i=0; i<64; i++)
      m_pEntitySystem->ReserveEntityId(LOCAL_PLAYER_ENTITY_ID+i);
    if (CGameClientNub * pClientNub = m_pFramework->GetGameClientNub())
      if (CGameClientChannel * pClientChannel = pClientNub->GetGameClientChannel())
        pClientChannel->ClearPlayer();
   }

   m_classRegistry.Reset();
   m_controlledObjects.Reset( m_pFramework->IsServer() );
   }
 */

bool CGameContext::HasSpawnPoint()
{
	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

	pIt->MoveFirst();
	while (!pIt->IsEnd())
	{
		if (IEntity* pEntity = pIt->Next())
			if (0 == strcmp(pEntity->GetClass()->GetName(), "SpawnPoint"))
				return true;
	}
	return false;
}

void CGameContext::CallOnSpawnComplete(IEntity* pEntity)
{
	IScriptTable* pScriptTable(pEntity->GetScriptTable());
	if (!pScriptTable)
		return;

	if (gEnv->bServer)
	{
		SmartScriptTable server;
		if (pScriptTable->GetValue("Server", server))
		{
			if ((server->GetValueType("OnSpawnComplete") == svtFunction) && pScriptTable->GetScriptSystem()->BeginCall(server, "OnSpawnComplete"))
			{
				pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
				pScriptTable->GetScriptSystem()->EndCall();
			}
		}
	}

	if (gEnv->IsClient())
	{
		SmartScriptTable client;
		if (pScriptTable->GetValue("Client", client))
		{
			if ((client->GetValueType("OnSpawnComplete") == svtFunction) && pScriptTable->GetScriptSystem()->BeginCall(client, "OnSpawnComplete"))
			{
				pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
				pScriptTable->GetScriptSystem()->EndCall();
			}
		}
	}
}

ESynchObjectResult CGameContext::SynchObject(EntityId entityId, NetworkAspectType nAspect, uint8 profile, TSerialize serialize, bool verboseLogging)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
	{
		if (!gEnv->IsEditor())
			GameWarning("Trying to synchronize non-existent entity %d", entityId);
		return eSOR_Failed;
	}

	if (gEnv->bServer && serialize.IsReading() && (m_pNetContext->DelegatableAspects() & nAspect) && !(m_pNetContext->ServerControllerOnlyAspects() & nAspect))
	{
		NET_PROFILE_COUNT_READ_BITS(true);
	}
	NET_PROFILE_SCOPE(pEntity->GetClass()->GetName(), serialize.IsReading());
	NET_PROFILE_SCOPE(pEntity->GetName(), serialize.IsReading());

	if (nAspect == eEA_Script)
	{
		IEntityScriptComponent* pScriptProxy = static_cast<IEntityScriptComponent*>(pEntity->GetProxy(ENTITY_PROXY_SCRIPT));
		if (pScriptProxy)
		{
			NET_PROFILE_SCOPE("ScriptProxy", serialize.IsReading());
			pScriptProxy->GameSerialize(serialize);
		}

		NET_PROFILE_SCOPE("ScriptRMI", serialize.IsReading());
		if (!m_pScriptRMI->SerializeScript(serialize, pEntity))
		{
			if (verboseLogging)
				GameWarning("CGameContext::SynchObject: failed to serialize script aspect");
			NET_PROFILE_COUNT_READ_BITS(false);
			return eSOR_Failed;
		}
		return eSOR_Ok;
	}

	int pflags = 0;
	if (nAspect == eEA_Physics && m_pPhysicsSync && serialize.IsReading())
	{
		if (m_pPhysicsSync->IgnoreSnapshot())
		{
			NET_PROFILE_COUNT_READ_BITS(false);
			return eSOR_Skip;
		}
		else if (m_pPhysicsSync->NeedToCatchup())
		{
			pflags |= ssf_compensate_time_diff;
		}
	}

	bool ok = pEntity->GetNetEntity()->NetSerializeEntity(serialize, (EEntityAspects)nAspect, profile, 0);
	if (!ok)
	{
		if (verboseLogging)
			GameWarning("CGameContext::SynchObject: game fails to serialize aspect %d on profile %d", BitIndex(nAspect), int(profile));
		NET_PROFILE_COUNT_READ_BITS(false);
		return eSOR_Failed;
	}

	if (nAspect == eEA_Physics && m_pPhysicsSync && serialize.IsReading() && serialize.ShouldCommitValues())
	{
		m_pPhysicsSync->UpdatedEntity(entityId);
	}

	NET_PROFILE_COUNT_READ_BITS(false);
	return eSOR_Ok;
}

class CSpawnMsg : public INetSendableHook, private SBasicSpawnParams
{
public:
	CSpawnMsg(const SBasicSpawnParams& params, ISerializableInfoPtr pObj) :
		INetSendableHook(),
		SBasicSpawnParams(params),
		m_pObj(pObj) {}

	EMessageSendResult Send(INetSender* pSender)
	{
		pSender->BeginMessage(CGameClientChannel::DefaultSpawn);
#if RESERVE_UNBOUND_ENTITIES
		pSender->ser.Value("partialNetID", m_partialNetID, 'eid');
#endif
		SBasicSpawnParams::SerializeWith(pSender->ser);
		if (m_pObj)
			m_pObj->SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate)
	{
	}

	size_t GetSize()
	{
		return sizeof(*this);
	}

private:
	ISerializableInfoPtr m_pObj;
};

INetSendableHookPtr CGameContext::CreateObjectSpawner(EntityId entityId, INetChannel* pChannel)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
	{
		return NULL;
	}

	uint16 channelId = ~uint16(0);
	if (pChannel)
	{
		IGameChannel* pIGameChannel = pChannel->GetGameChannel();
		CGameServerChannel* pGameServerChannel = (CGameServerChannel*) pIGameChannel;
		channelId = pGameServerChannel->GetChannelId();
	}

	SBasicSpawnParams params;
	params.name = pEntity->GetName();
	if (pEntity->GetArchetype())
	{
		ClassIdFromName(params.classId, pEntity->GetArchetype()->GetName());
	}
	else
	{
		auto pEntityClass = pEntity->GetClass();
		ClassIdFromName(params.classId, pEntityClass->GetName());

		if (pEntityClass == gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass()
			&& pEntity->GetComponentsCount())
		{
			// For entities of the default class, we serialize the first component GUID only,
			// assuming the first component will take care of adding other components remotely.
			DynArray<IEntityComponent *> comps;
			pEntity->GetComponents(comps);
			params.baseComponent = comps[0]->GetClassDesc().GetGUID();
		}
	}
	params.pos = pEntity->GetPos();
	params.scale = pEntity->GetScale();
	params.rotation = pEntity->GetRotation();
	params.nChannelId = pEntity->GetNetEntity()->GetChannelId();
	// Make sure that the remotely spawned entity uses the same flags, except the local player flag!
	params.flags = pEntity->GetFlags() & ~ENTITY_FLAG_LOCAL_PLAYER;

	params.bClientActor = pChannel ?
	                      ((CGameChannel*)pChannel->GetGameChannel())->GetPlayerId() == pEntity->GetId() : false;

	if (params.bClientActor)
	{
		pChannel->DeclareWitness(entityId);
	}

	return new CSpawnMsg(params, pEntity->GetSerializableNetworkSpawnInfo());
}

void CGameContext::ObjectInitClient(EntityId entityId, INetChannel* pChannel)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
	{
		return;
	}

	uint16 channelId = ~uint16(0);
	if (pChannel)
	{
		IGameChannel* pIGameChannel = pChannel->GetGameChannel();
		CGameServerChannel* pGameServerChannel = (CGameServerChannel*) pIGameChannel;
		channelId = pGameServerChannel->GetChannelId();
	}

	IEntityComponent* pProxy = pEntity->GetProxy(ENTITY_PROXY_USER);

	CGameObject* pGameObject = reinterpret_cast<CGameObject*>(pProxy);
	if (pGameObject)
	{
		pGameObject->InitClient(channelId);
	}
	m_pScriptRMI->OnInitClient(channelId, pEntity);
}

bool CGameContext::SendPostSpawnObject(EntityId id, INetChannel* pINetChannel)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(id);
	if (!pEntity)
		return false;

	uint16 channelId = ~uint16(0);
	if (pINetChannel)
	{
		IGameChannel* pIGameChannel = pINetChannel->GetGameChannel();
		CGameServerChannel* pGameServerChannel = (CGameServerChannel*) pIGameChannel;
		channelId = pGameServerChannel->GetChannelId();
	}

	IEntityComponent* pProxy = pEntity->GetProxy(ENTITY_PROXY_USER);
	CGameObject* pGameObject = reinterpret_cast<CGameObject*>(pProxy);
	if (pGameObject)
		pGameObject->PostInitClient(channelId);
	m_pScriptRMI->OnPostInitClient(channelId, pEntity);

	return true;
}

void CGameContext::ControlObject(EntityId id, bool bHaveControl)
{
	SDelegateCallbackIndex idx = { id, bHaveControl };
	DelegateCallbacks::iterator iter = m_delegateCallbacks.lower_bound(idx);
	IScriptSystem* pSS = gEnv->pScriptSystem;
	while (iter != m_delegateCallbacks.end() && iter->first == idx)
	{
		DelegateCallbacks::iterator next = iter;
		++next;

		Script::Call(pSS, iter->second);
		pSS->ReleaseFunc(iter->second);
		m_delegateCallbacks.erase(iter);

		iter = next;
	}
}

void CGameContext::PassDemoPlaybackMappedOriginalServerPlayer(EntityId id)
{
	CCryAction::GetCryAction()->GetIActorSystem()->SetDemoPlaybackMappedOriginalServerPlayer(id);
}

void CGameContext::AddControlObjectCallback(EntityId id, bool willHaveControl, HSCRIPTFUNCTION func)
{
	SDelegateCallbackIndex idx = { id, willHaveControl };
	m_delegateCallbacks.insert(std::make_pair(idx, func));
}

void CGameContext::UnboundObject(EntityId entityId)
{
	m_pEntitySystem->RemoveEntity(entityId, false);
}

INetAtSyncItem* CGameContext::HandleRMI(bool bClient, EntityId objID, uint8 funcID, TSerialize ser, INetChannel* pChannel)
{
	return m_pScriptRMI->HandleRMI(bClient, objID, funcID, ser, pChannel);
}

//
// IEntitySystemSink
//

bool CGameContext::OnBeforeSpawn(SEntitySpawnParams& params)
{
	if (m_isInLevelLoad && !gEnv->IsEditor() && gEnv->bMultiplayer)
	{
#ifndef _RELEASE
		static ICVar* pLoadAllLayersForResList = 0;
		if (!pLoadAllLayersForResList)
		{
			pLoadAllLayersForResList = gEnv->pConsole->GetCVar("sv_LoadAllLayersForResList");
		}

		if (pLoadAllLayersForResList && pLoadAllLayersForResList->GetIVal() != 0)
		{
			//Bypass the layer filtering. This ensures that an autoresourcelist for generating pak files will contain all the possible assets.
			return true;
		}
#endif // #ifndef _RELEASE

		static const char prefix[] = "GameType_";
		static const size_t prefixLen = sizeof(prefix) - 1;

		// if a layer prefixed with GameType_ exists,
		// then discard it if it's not the current game type
		if (params.sLayerName && !strnicmp(params.sLayerName, prefix, prefixLen))
		{
			if (IEntity* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
			{
				const char* currentType = pGameRules->GetClass()->GetName();
				const char* gameType = params.sLayerName + prefixLen;
				if (stricmp(gameType, currentType))
					return false;
			}
		}
	}

	return true;
}

void CGameContext::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
	if (HasContextFlag(eGSF_ImmersiveMultiplayer) && m_pBreakReplicator.get())
		m_pBreakReplicator->OnSpawn(pEntity, params);

	if (IGameRules* pGameRules = GetGameRules())
		pGameRules->OnEntitySpawn(pEntity);

	if (params.nFlags & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
		return;
	if (pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
		return;

	//	if (aspects & eEA_Physics)
	//	{
	//		aspects &= ~eEA_Volatile;
	//	}

#if VERBOSE_TRACE_CONTEXT_SPAWNS
	CryLogAlways("CGameContext: Spawn Entity");
	CryLogAlways("  name       : %s", params.sName);
	CryLogAlways("  id         : 0x%.4x", params.id);
	CryLogAlways("  flags      : 0x%.8x", params.nFlags);
	CryLogAlways("  pos        : %.1f %.1f %.1f", params.vPosition.x, params.vPosition.y, params.vPosition.z);
	CryLogAlways("  cls name   : %s", params.pClass->GetName());
	CryLogAlways("  cls flags  : 0x%.8x", params.pClass->GetFlags());
	CryLogAlways("  channel    : %d", channelId);
	CryLogAlways("  net aspects: 0x%.8x", aspects);
#endif

	if (pEntity->GetProxy(ENTITY_PROXY_SCRIPT))
	{
		m_pScriptRMI->SetupEntity(params.id, pEntity, true, true);
	}

	bool calledBindToNetwork = false;
	if (m_isInLevelLoad && gEnv->bMultiplayer)
	{
		if (pEntity->GetScriptTable() && !pEntity->GetProxy(ENTITY_PROXY_USER))
		{
			//CryLog("Forcibly binding %s to network", params.sName);
			calledBindToNetwork = true;
			pEntity->GetNetEntity()->BindToNetwork();
		}
	}

	if (!calledBindToNetwork)
	{
		pEntity->GetNetEntity()->BindToNetwork(eBTNM_NowInitialized);
	}

	CallOnSpawnComplete(pEntity);
}

void CGameContext::CompleteUnbind(EntityId id)
{
	CScopedRemoveObjectUnlock removeObjectUnlock(this);
	gEnv->pEntitySystem->RemoveEntity(id);
}

bool CGameContext::OnRemove(IEntity* pEntity)
{
	EntityId id = pEntity->GetId();
	bool bound = m_pNetContext->IsBound(id);

	if (IGameRules* pGameRules = GetGameRules())
		pGameRules->OnEntityRemoved(pEntity);

	if (gEnv->IsEditor() || CCryAction::GetCryAction()->IsGameSessionMigrating())
	{
		if (bound)
		{
			m_pNetContext->UnbindObject(id);
		}

		return true;
	}

	if (0 == m_removeObjectUnlocks)
	{
		if (!HasContextFlag(eGSF_Server) && bound)
		{
			GameWarning("Attempt to remove entity %s %.8x - disallowing", pEntity->GetName(), id);
			return false;
		}
		else if (gEnv->bMultiplayer)
		{
			m_pNetContext->SafelyUnbind(id);
			return false;
		}
	}

	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnRemove(pEntity);

	bool ok = true;

	if (bound)
	{
		ok = m_pNetContext->UnbindObject(id);
	}

	if (pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY | ENTITY_FLAG_UNREMOVABLE))
	{
		return true;
	}

	if (ok)
	{
		m_pScriptRMI->RemoveEntity(id);

		if (CGameClientNub* pClientNub = m_pFramework->GetGameClientNub())
			if (CGameClientChannel* pClientChannel = pClientNub->GetGameClientChannel())
				if (pClientChannel->GetPlayerId() == id)
					pClientChannel->ClearPlayer();
	}
	return ok;
}

void CGameContext::OnReused(IEntity* pEntity, SEntitySpawnParams& params)
{
	if (IGameRules* pGameRules = GetGameRules())
		pGameRules->OnEntityReused(pEntity, params, params.prevId);

	m_pNetContext->UnbindObject(params.prevId);

	if (0 == m_removeObjectUnlocks && gEnv->bMultiplayer && !CCryAction::GetCryAction()->IsGameSessionMigrating())
	{
		m_pNetContext->SafelyUnbind(pEntity->GetId());
	}

	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnReuse(pEntity, params);

	m_pScriptRMI->RemoveEntity(params.prevId);

	if (pEntity->GetProxy(ENTITY_PROXY_SCRIPT))
	{
		m_pScriptRMI->SetupEntity(params.id, pEntity, true, true);
	}

	if (CGameClientNub* pClientNub = m_pFramework->GetGameClientNub())
		if (CGameClientChannel* pClientChannel = pClientNub->GetGameClientChannel())
			if (pClientChannel->GetPlayerId() == params.prevId)
				pClientChannel->ClearPlayer();

	if (CGameObject* pGO = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER))
		pGO->BindToNetwork(eBTNM_NowInitialized);
}

//
// physics synchronization
//

void CGameContext::OnCollision(const EventPhys* pEvent, void*)
{
	const EventPhysCollision* pCEvent = static_cast<const EventPhysCollision*>(pEvent);
	//IGameObject *pSrc = pCollision->iForeignData[0]==PHYS_FOREIGN_ID_ENTITY ? s_this->GetEntityGameObject((IEntity*)pCollision->pForeignData[0]):0;
	//IGameObject *pTrg = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? s_this->GetEntityGameObject((IEntity*)pCollision->pForeignData[1]):0;
	IEntity* pEntitySrc = GetEntity(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
	IEntity* pEntityTrg = GetEntity(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

	if (!pEntitySrc || !pEntityTrg)
		return;

	s_pGameContext->m_pNetContext->PulseObject(pEntitySrc->GetId(), 'bump');
	s_pGameContext->m_pNetContext->PulseObject(pEntityTrg->GetId(), 'bump');
}

//
// internal functions
//

bool CGameContext::RegisterClassName(const string& name, uint16 id)
{
	return m_classRegistry.RegisterClassName(name, id);
}

uint32 CGameContext::GetClassesHash()
{
	return m_classRegistry.GetHash();
}

void CGameContext::DumpClasses()
{
	m_classRegistry.DumpClasses();
}

bool CGameContext::ClassIdFromName(uint16& id, const string& name) const
{
	return m_classRegistry.ClassIdFromName(id, name);
}

bool CGameContext::ClassNameFromId(string& name, uint16 id) const
{
	return m_classRegistry.ClassNameFromId(name, id);
}

bool CGameContext::ChangeContext(bool isServer, const SGameContextParams* params)
{
	if (CCryAction::GetCryAction()->GetILevelSystem()->IsLevelLoaded())
		m_loadFlags = EstablishmentFlags_LoadNextLevel;
	else
		m_loadFlags = EstablishmentFlags_InitialLoad;

	m_resetChannels.clear();

	if (HasContextFlag(eGSF_DemoRecorder) && HasContextFlag(eGSF_DemoPlayback))
	{
		GameWarning("Cannot both playback a demo file and record a demo at the same time.");
		return false;
	}

	if (!HasContextFlag(eGSF_NoLevelLoading))
	{
		if (!params->levelName)
		{
			GameWarning("No level specified: not changing context");
			return false;
		}

		ILevelInfo* pLevelInfo = m_pFramework->GetILevelSystem()->GetLevelInfo(params->levelName);
		if (!pLevelInfo)
		{
			GameWarning("Level %s not found", params->levelName);

			m_pFramework->GetILevelSystem()->OnLevelNotFound(params->levelName);

			return false;
		}

#ifndef _DEBUG
		string gameMode = params->gameRules;
		if ((pLevelInfo->GetGameTypeCount() > 0) && (gameMode != "SinglePlayer") && !pLevelInfo->SupportsGameType(gameMode) && !gEnv->IsEditor() && !gEnv->pSystem->IsDevMode())
		{
			GameWarning("Level %s does not support %s gamerules.", params->levelName, gameMode.c_str());

			return false;
		}
#endif
	}

	if (!HasContextFlag(eGSF_NoGameRules))
	{
		if (!params->gameRules)
		{
			GameWarning("No rules specified: not changing context");
			return false;
		}

		if (!m_pFramework->GetIGameRulesSystem()->HaveGameRules(params->gameRules))
		{
			GameWarning("Game rules %s not found; not changing context", params->gameRules);
			return false;
		}
	}

	if (params->levelName)
		m_levelName = params->levelName;
	else
		m_levelName.clear();
	if (params->gameRules)
		m_gameRules = params->gameRules;
	else
		m_gameRules.clear();

	//if (HasContextFlag(eGSF_Server) && HasContextFlag(eGSF_Client))
	while (HasContextFlag(eGSF_Server))
	{
		if (HasContextFlag(eGSF_Client) && HasContextFlag(eGSF_DemoPlayback))
		{
			if (params->demoPlaybackFilename == NULL)
				break;
			INetChannel* pClientChannel = m_pFramework->GetGameClientNub()->GetGameClientChannel()->GetNetChannel();
			INetChannel* pServerChannel = m_pFramework->GetGameServerNub()->GetChannel(1)->GetNetChannel();
			m_pNetContext->ActivateDemoPlayback(params->demoPlaybackFilename, pClientChannel, pServerChannel);
			break;
		}
		if (HasContextFlag(eGSF_DemoRecorder))
		{
			if (params->demoRecorderFilename == NULL)
				break;
			m_pNetContext->ActivateDemoRecorder(params->demoRecorderFilename);
			CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_demoRecorderCreated, 0));
			break;
		}

		break;
	}

	m_pNetContext->EnableBackgroundPassthrough(HasContextFlag(eGSF_Server) && gEnv->bMultiplayer);

	// start this thing off
	if (isServer)
	{
		if (!m_pNetContext->ChangeContext())
			return false;
	}

	return true;
}

bool CGameContext::Update()
{
	bool ret = false;

	float white[] = { 1, 1, 1, 1 };
	if (!m_bHasSpawnPoint)
	{
		IRenderAuxText::Draw2dLabel(10, 10, 4, white, false, "NO SPAWN POINT");
	}

	// TODO: This block should be moved into GameSDK code, since sv_pacifist is a GameSDK-only CVar
	{
		static ICVar* pPacifist = 0;
		static bool bTried = false;
		if (!pPacifist && !bTried)
		{
			pPacifist = gEnv->pConsole->GetCVar("sv_pacifist");
			bTried = true;
		}
		if (gEnv->IsClient() && !gEnv->bServer && pPacifist && pPacifist->GetIVal() == 1)
		{
			IRenderAuxText::Draw2dLabel(10, 10, 4, white, false, "PACIFIST MODE");
		}
	}

	/*
	   else if (m_bScheduledLevelLoad)
	   {
	    m_bScheduledLevelLoad = false;
	    // if we're not the editor then we need to load a level
	    bool oldLevelLoad = m_isInLevelLoad;
	    m_isInLevelLoad = true;
	    m_pFramework->GetIActionMapManager()->Enable(false); // diable action processing during loading
	    if (!m_pFramework->GetILevelSystem()->LoadLevel(m_levelName.c_str()))
	    {
	      m_pFramework->EndGameContext();
	      return false;
	    }
	    m_bHasSpawnPoint = HasSpawnPoint();
	    m_pFramework->GetIActionMapManager()->Enable(true);
	    m_isInLevelLoad = oldLevelLoad;
	    m_pNetContext->EstablishedContext();

	    if (!HasContextFlag(eGSF_Client))
	    {
	      StartGame();
	      m_bStarted = true;
	    }
	   }
	 */

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (m_pVoiceController)
		m_pVoiceController->Enable(m_pFramework->IsVoiceRecordingEnabled());
#endif

	if (0 == (m_broadcastActionEventInGame -= (m_broadcastActionEventInGame != -1)))
		CCryAction::GetCryAction()->OnActionEvent(eAE_inGame);

#if ENABLE_NETDEBUG
	if (m_pNetDebug)
		m_pNetDebug->Update();
#endif

	return ret;
}

bool CGameContext::OnBeforeVarChange(ICVar* pVar, const char* sNewValue)
{
	int flags = pVar->GetFlags();
	bool netSynced = ((flags & VF_NET_SYNCED) != 0);
	bool isServer = HasContextFlag(eGSF_Server);
	bool allowChange = (!netSynced) || isServer;

#if LOG_CVAR_USAGE
	if (!allowChange)
	{
		CryLog("[CVARS]: [CHANGED] CGameContext::OnBeforeVarChange(): variable [%s] is VF_NET_SYNCED with a value of [%s]; CLIENT changing attempting to set to [%s] - IGNORING",
		       pVar->GetName(),
		       pVar->GetString(),
		       sNewValue);
	}
#endif // LOG_CVAR_USAGE

	return allowChange;
}

void CGameContext::OnAfterVarChange(ICVar* pVar)
{
}

// IHostMigrationEventListener
IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	// Register all client actor data in the game at this point
	// (N.B. Even if we're not going to become the new server, we know
	// that we can accurately store all local player statistics here
	// (such as health and ammo counts) that aren't normally transmitted
	// to other clients). This info could be sent as part of the
	// migrating player connection string, or a discrete message.
	IGameRules* pGameRules = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules();
	if (pGameRules)
	{
		pGameRules->ClearAllMigratingPlayers();
		IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
		IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
		IActor* pActor = pActorIterator->Next();

		while (pActor != NULL)
		{
			if (pActor->IsPlayer())
			{
				pGameRules->StoreMigratingPlayer(pActor);
			}

			pActor = pActorIterator->Next();
		}
	}
	else
	{
		CryLogAlways("[Host Migration]: no game rules found - aborting");
		gEnv->pNetwork->TerminateHostMigration(hostMigrationInfo.m_session);
	}

	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	CryLogAlways("[Host Migration]: CGameContext::OnDemoteToClient() started");

	if (HasContextFlag(eGSF_Server) == true)
	{
		m_flags &= ~eGSF_Server;

		CCryAction::GetCryAction()->GetIGameSessionHandler()->EndSession();
	}

	CryLogAlways("[Host Migration]: CGameContext::OnDemoteToClient() finished");
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	CryLogAlways("[Host Migration]: CGameContext::OnPromoteToServer() started");
	m_flags |= eGSF_Server;

	ICVar* pCVar = gEnv->pConsole->GetCVar("sv_requireinputdevice");
	if (pCVar)
	{
		const char* requiredInputDevice = pCVar->GetString();
		if (0 == strcmpi(requiredInputDevice, "none"))
		{
			m_flags &= ~(eGSF_RequireKeyboardMouse | eGSF_RequireController);
		}
		else if (0 == strcmpi(requiredInputDevice, "keyboard"))
		{
			m_flags |= eGSF_RequireKeyboardMouse;
			m_flags &= ~eGSF_RequireController;
		}
		else if (0 == strcmpi(requiredInputDevice, "gamepad"))
		{
			m_flags |= eGSF_RequireController;
			m_flags &= ~eGSF_RequireKeyboardMouse;
		}
		else if (0 == strcmpi(requiredInputDevice, "dontcare"))
		{
		}
	}

	CryLogAlways("[Host Migration]: CGameContext::OnPromoteToServer() finished");
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameContext::OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}
// ~IHostMigrationEventListener

static XmlNodeRef Child(XmlNodeRef& from, const char* name)
{
	if (XmlNodeRef found = from->findChild(name))
		return found;
	else
	{
		XmlNodeRef newNode = from->createNode(name);
		from->addChild(newNode);
		return newNode;
	}
}

XmlNodeRef CGameContext::GetGameState()
{
	CGameServerNub* pGameServerNub = m_pFramework->GetGameServerNub();

	XmlNodeRef root = GetISystem()->CreateXmlNode("root");

	// add server/network properties
	XmlNodeRef serverProperties = Child(root, "server");

	// add game properties
	XmlNodeRef gameProperties = Child(root, "game");
	gameProperties->setAttr("levelName", m_levelName.c_str());
	gameProperties->setAttr("curPlayers", pGameServerNub->GetPlayerCount());
	gameProperties->setAttr("gameRules", m_gameRules.c_str());
	char buffer[32];
	GetISystem()->GetProductVersion().ToShortString(buffer);
	gameProperties->setAttr("version", string(buffer).c_str());
	gameProperties->setAttr("maxPlayers", pGameServerNub->GetMaxPlayers());

	return root;
}

IGameRules* CGameContext::GetGameRules()
{
	IEntity* pGameRules = m_pFramework->GetIGameRulesSystem()->GetCurrentGameRulesEntity();
	if (pGameRules)
	{
		return 0;
	}

	return 0;
}

void CGameContext::OnStartNetworkFrame()
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnStartFrame();
}

void CGameContext::OnEndNetworkFrame()
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnEndFrame();
}

void CGameContext::OnBrokeSomething(const SBreakEvent& be, EventPhysMono* pMono, bool isPlane)
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnBrokeSomething(be, pMono, isPlane);
}

void CGameContext::ReconfigureGame(INetChannel* pNetChannel)
{
	CryLogAlways("Game reconfiguration: %s", pNetChannel->GetName());
}

void CGameContext::AllowCallOnClientConnect()
{
	m_bAllowSendClientConnect = true;
}

CTimeValue CGameContext::GetPhysicsTime()
{
	int tick = gEnv->pPhysicalWorld->GetiPhysicsTime();
	CTimeValue tm = tick * double(gEnv->pPhysicalWorld->GetPhysVars()->timeGranularity);
	return tm;
}

void CGameContext::BeginUpdateObjects(CTimeValue physTime, INetChannel* pChannel)
{
	//UpdateTimestampedAspect calls this and pChannel is used there already; will be valid
	if (CGameChannel* pGameChannel = (CGameChannel*)pChannel->GetGameChannel())
		if (m_pPhysicsSync = pGameChannel->CGameChannel::GetPhysicsSync())
			m_pPhysicsSync->OnPacketHeader(physTime);
}

void CGameContext::EndUpdateObjects()
{
	if (m_pPhysicsSync)
	{
		m_pPhysicsSync->OnPacketFooter();
		m_pPhysicsSync = nullptr;
	}
}

void CGameContext::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (m_pVoiceController)
		m_pVoiceController->GetMemoryStatistics(s);
#endif
	m_classRegistry.GetMemoryStatistics(s);
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->GetMemoryStatistics(s);
	s->AddObject(m_levelName);
	s->AddObject(m_gameRules);
	s->AddObject(m_connectionString);
	//s->AddObject(m_delegateCallbacks);
}

// encode size ignore last 3 bits of size in bytes. (encode by 8bytes min)
#define TEA_GETSIZE(len) ((len) & (~7))

static ILINE bool IsDX11()
{
	ERenderType renderType = gEnv->pRenderer->GetRenderType();
	return true;//renderType == eRT_DX11;	// marcio: in this context, we assume DX11 for Crysis2, so immersiveness can work
}

static ILINE bool HasController()
{
	if (gEnv->pInput)
		return gEnv->pInput->HasInputDeviceOfType(eIDT_Gamepad);
	else
		return false;
}

static ILINE bool HasKeyboardMouse()
{
	if (gEnv->pInput)
		return gEnv->pInput->HasInputDeviceOfType(eIDT_Keyboard) && gEnv->pInput->HasInputDeviceOfType(eIDT_Mouse);
	else
		return true;
}

string CGameContext::GetConnectionString(CryFixedStringT<HOST_MIGRATION_MAX_PLAYER_NAME_SIZE>* pNameOverride, bool fake) const
{
	bool bForceImmersive = false;
	ICVar* pImmersive = gEnv->pConsole->GetCVar("g_immersive");
	if (pImmersive && pImmersive->GetIVal() != 0)
	{
		bForceImmersive = true;
	}

	string playerName;
	if (pNameOverride)
	{
		playerName = pNameOverride->c_str();
	}
	else
	{
		bool bDefaultName = false;
		if (IPlayerProfileManager* pProfileManager = CCryAction::GetCryAction()->GetIPlayerProfileManager())
		{
			IPlatformOS::TUserName userName;
			IPlatformOS* pOS = GetISystem()->GetPlatformOS();
			uint32 userIndex = pProfileManager->GetExclusiveControllerDeviceIndex();
			if (!pOS->UserGetOnlineName(userIndex, userName) || userName.empty())
			{
				if (!pOS->UserGetName(userIndex, userName) || userName.empty())
				{
					bDefaultName = true;
				}
			}

			if (!bDefaultName)
			{
				playerName = userName.c_str();
			}
			else if (CCryActionCVars::Get().useCurrentUserNameAsDefault != 0)
			{
				playerName = gEnv->pSystem->GetUserName();
			}
		}
	}

	char buf[128];
	string constr;
	constr += '!';
	gEnv->pSystem->GetProductVersion().ToShortString(buf);
	constr += buf;
	constr += ':';
	if (fake || bForceImmersive || IsDX11())
		constr += 'X';
	if (fake || HasController())
		constr += 'C';
	if (fake || HasKeyboardMouse())
		constr += 'K';
	// Indicate this is a migrating player so it can be handled slightly
	// differently when CGameServerNub::CreateChannel() deals with it.
	if (CCryAction::GetCryAction()->IsGameSessionMigrating())
		constr += 'M';
	constr += ':';
	constr += ToHexStr(playerName.data(), playerName.size());
	constr += ':';
	constr += fake ? "fake" : m_connectionString;
	cry_sprintf(buf, "%d", (int)time(NULL));
	constr = buf + constr;
	int len = constr.length();
	int r = cry_random(0, 19) + CLAMP(64 - len, 0, 64);
	for (int i = 0; i < r || (constr.length() & 7); i++)
	{
		char c = cry_random('a', 'z');
		constr = c + constr;
	}
	len = constr.length();

	string out = ToHexStr(&constr[0], len);
	string temp = out;
	CRY_ASSERT(FromHexStr(temp));
	CRY_ASSERT(temp == constr);
	return out;
}

SParsedConnectionInfo CGameContext::ParseConnectionInfo(const string& sconst)
{
	SParsedConnectionInfo info;
	info.allowConnect = false;
	info.isMigrating = false;
	info.cause = eDC_GameError;

	string badFormat = "Illegal connection packet";
	string temp = sconst;
	if (!FromHexStr(temp))
	{
		info.errmsg = badFormat;
		return info;
	}

	int slen = temp.length();
	if (temp.length() > 511)
	{
		info.errmsg = badFormat;
		return info;
	}

	char s[512];
	memset(s, 0, 512);
	memcpy(s, temp.data(), TEA_GETSIZE(slen));

	char buf[128];
	gEnv->pSystem->GetProductVersion().ToShortString(buf);

	int i = 0;
	string versionMismatch = "Version mismatch";
	while (i < slen && s[i] != '!')
		++i;
	if (!s[i])
		return info;
	++i;
	int sbuf = i;
	for (; buf[i - sbuf]; i++)
	{
		if (slen <= i)
		{
			info.errmsg = badFormat;
			return info;
		}
		if (s[i] != buf[i - sbuf])
		{
			info.errmsg = versionMismatch;
			info.cause = eDC_VersionMismatch;
			return info;
		}
	}
	if (slen <= i)
	{
		info.errmsg = badFormat;
		return info;
	}
	if (s[i++] != ':')
	{
		info.errmsg = badFormat;
		return info;
	}

	bool isDX11 = false;
	bool hasController = false;
	bool hasKeyboardMouse = false;
	bool gotName = false;
	for (; i < slen; i++)
	{
		switch (s[i])
		{
		case 'X':
			isDX11 = true;
			break;
		case 'C':
			hasController = true;
			break;
		case 'K':
			hasKeyboardMouse = true;
			break;
		case 'M':
			info.isMigrating = true;
			break;
		case ':':
			if (!gotName)
			{
				gotName = true;
				int k = i + 1;
				while (k < slen && s[k] != ':')
					++k;
				info.playerName = string(&s[i + 1], k - i - 1);
				i = k - 1;

				if (!FromHexStr(info.playerName))
				{
					info.errmsg = badFormat;
					return info;
				}
				break;
			}
			else
			{
				if (HasContextFlag(eGSF_RequireController) && !hasController)
				{
					info.errmsg = "Client has no game controller";
					info.cause = eDC_NoController;
					return info;
				}
				if (HasContextFlag(eGSF_RequireKeyboardMouse) && !hasKeyboardMouse)
				{
					info.errmsg = "Client has no keyboard/mouse";
					info.cause = eDC_GameError;
					return info;
				}
				if (HasContextFlag(eGSF_ImmersiveMultiplayer) && !isDX11)
				{
					info.errmsg = "Client is not DX11 capable";
					info.cause = eDC_NotDX11Capable;
					return info;
				}
				info.allowConnect = true;
				info.gameConnectionString = s + i + 1;
				return info;
			}
		default:
			info.errmsg = badFormat;
			return info;
		}
	}
	info.errmsg = badFormat;
	return info;
}

void CGameContext::DefineContextProtocols(IProtocolBuilder* pBuilder, bool server)
{
	if (m_pBreakReplicator.get())
	{
		m_pBreakReplicator->m_bDefineProtocolMode_server = server;
		m_pBreakReplicator->DefineProtocol(pBuilder);
	}

	CCryAction* cca = CCryAction::GetCryAction();
	if (cca->GetNetMessageDispatcher())
		cca->GetNetMessageDispatcher()->DefineProtocol(pBuilder);

	if (IManualFrameStepController* pFrameStepController = gEnv->pSystem->GetManualFrameStepController())
	{
		pFrameStepController->DefineProtocols(pBuilder);
	}
}

void CGameContext::PlaybackBreakage(int breakId, INetBreakagePlaybackPtr pBreakage)
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->PlaybackBreakage(breakId, pBreakage);
}

void* CGameContext::ReceiveSimpleBreakage(TSerialize ser)
{
	if (m_pBreakReplicator.get())
		return m_pBreakReplicator->ReceiveSimpleBreakage(ser);
	return NULL;
}

void CGameContext::PlaybackSimpleBreakage(void* userData, INetBreakageSimplePlaybackPtr pBreakage)
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->PlaybackSimpleBreakage(userData, pBreakage);
}

bool CGameContext::SetImmersive(bool immersive)
{
	bool bForceImmersive = false;
	ICVar* pImmersive = gEnv->pConsole->GetCVar("g_immersive");
	if (pImmersive && pImmersive->GetIVal() != 0)
	{
		bForceImmersive = true;
	}

	if (!bForceImmersive && (immersive && !IsDX11()))
		return false;
	if (immersive)
		m_flags |= eGSF_ImmersiveMultiplayer;
	else
		m_flags &= ~eGSF_ImmersiveMultiplayer;
	return true;
}
