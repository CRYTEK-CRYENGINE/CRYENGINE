// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IGameRulesSystem.h"
#include "ActionGame.h"
#include <CryGame/IGameFramework.h>
#include "Network/GameClientNub.h"
#include "Network/GameServerNub.h"
#include "Network/GameClientChannel.h"
#include "Network/GameServerChannel.h"
#include "Network/GameContext.h"
#include "Network/ServerTimer.h"
#include "Network/DeformingBreak.h" // for debug function
#include "CryAction.h"
#include "CryActionCVars.h"
#include "IActorSystem.h"
#include "MaterialEffects/MaterialEffectsCVars.h"
#include <CryParticleSystem/ParticleParams.h>
#include "DelayedPlaneBreak.h"
#include <CryPhysics/IPhysics.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CryAction/IMaterialEffects.h>
#include <CryEntitySystem/IBreakableGlassSystem.h>
#include "IPlayerProfiles.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include "IMovementController.h"
#include "IGameSessionHandler.h"
#include "DialogSystem/DialogSystem.h"
#include <Cry3DEngine/ITimeOfDay.h>
#include "TimeOfDayScheduler.h"
#include "PersistantDebug.h"
#include "Network/GameStats.h"
#include "Animation/PoseModifier/IKTorsoAim.h"
#include <CryGame/IGameTokens.h>
#include <CryMovie/IMovieSystem.h>
#include "IForceFeedbackSystem.h"

#include "CryAction.h"
#include "Network/BreakReplicator.h"
#include "Network/ObjectSelector.h"

#include <CryLobby/CommonICryLobby.h>
#include <CryLobby/CommonICryMatchMaking.h>
#include <CrySystem/Scaleform/IFlashUI.h>

CActionGame* CActionGame::s_this = 0;
int CActionGame::g_procedural_breaking = 0;
int CActionGame::g_joint_breaking = 1;
float CActionGame::g_tree_cut_reuse_dist = 0;
int CActionGame::g_no_secondary_breaking = 0;
int CActionGame::g_no_breaking_by_objects = 0;
int CActionGame::g_breakage_mem_limit = 0;
int CActionGame::g_breakage_debug = 0;
int CActionGame::s_waterMaterialId = -1;
float g_breakageFadeTime = 0.f;
float g_breakageFadeDelay = 0.f;
float g_breakageMinAxisInertia = 0.01f;
int g_breakageNoDebrisCollisions = 0;
float g_glassForceTimeout = 0.f;
float g_glassForceTimeoutSpread = 0.f;
int g_glassNoDecals = 0;
int g_glassAutoShatter = 0;
int g_glassAutoShatterOnExplosions = 0;
int g_glassMaxPanesToBreakPerFrame = 0;
float CActionGame::g_glassAutoShatterMinArea = 0.f;
int g_glassSystemEnable = 1;
/*
   ============================================================================================================================================
   Tree breakage throttling
    If the counter goes above g_breakageTreeMax then tree breakage will stop
    Every time a tree is broken the counter is raised by g_breakageTreeInc
    Every frame the counter is decreased by g_breakageTreeInc

    if max = 5, inc = 1, dec = 5, then can only break 5 trees per frame, and the count is reset next frame
    if max = 5, inc = 1, dec = 1, then can only break 5 trees within 5 frames, and the count is reset after an average of 5 frames
    if max = 5, inc = 5, dec = 1, then can only break 1 tree within 1 frame, and the count is reset after 5 frames

    If there is a full glass break, then g_breakageTreeIncGlass increases the counter. This throttles tree breakage
    when too much glass is being broken
   ============================================================================================================================================
 */
int g_breakageTreeMax = 0;
int g_breakageTreeInc = 0;
int g_breakageTreeDec = 0;
int g_breakageTreeIncGlass = 0;
int g_waterHitOnly = 0;
#if BREAKAGE_THROTTLE_LOGGING
	#define ThrottleLogAlways CryLogAlways
#else
	#define ThrottleLogAlways(...) ((void)0)
#endif

#ifndef _RELEASE
float CActionGame::g_hostMigrationServerDelay = 0.f;
#endif

#define MAX_ADDRESS_SIZE (256)

void CActionGame::RegisterCVars()
{
	REGISTER_CVAR2("g_procedural_breaking", &g_procedural_breaking, 1, VF_CHEAT, "Toggles procedural mesh breaking (except explosion-breaking)");
	REGISTER_CVAR2("g_joint_breaking", &g_joint_breaking, 1, 0, "Toggles jointed objects breaking");
	REGISTER_CVAR2("g_tree_cut_reuse_dist", &g_tree_cut_reuse_dist, 0, 0,
	               "Maximum distance from a previously made cut that allows reusing");
	REGISTER_CVAR2("g_no_secondary_breaking", &g_no_secondary_breaking, 0, 0,
	               "Prevents secondary procedural breaks (to keep down memory usage)");
	REGISTER_CVAR2("g_no_breaking_by_objects", &g_no_breaking_by_objects, 0, 0,
	               "Prevents procedural breaking caused by rigid bodies");
	REGISTER_CVAR2("g_breakage_mem_limit", &g_breakage_mem_limit, 0, 0,
	               "Sets a budget for procedurally breakable objects (in KBs)");
	REGISTER_CVAR2("g_breakage_debug", &g_breakage_debug, 0, 0,
	               "Turns on debug rendering for broken objects counted against g_breakage_mem_limit");

#ifndef _RELEASE
	REGISTER_CVAR2("g_hostMigrationServerDelay", &g_hostMigrationServerDelay, 0.f, 0, "Delay in host migration before promoting to server (seconds)");
#endif

	REGISTER_CVAR2("g_breakageFadeDelay", &g_breakageFadeDelay, 6.f, 0, "");
	REGISTER_CVAR2("g_breakageFadeTime", &g_breakageFadeTime, 6.f, 0, "");
	REGISTER_CVAR2("g_breakageMinAxisInertia", &g_breakageMinAxisInertia, 0.01f, 0, "Set this to 1.0 to force broken trees to have spherical inertia");
	REGISTER_CVAR2("g_breakageNoDebrisCollisions", &g_breakageNoDebrisCollisions, 0, 0, "Turns off all collisions for debris, apart from coltype_solid");

	REGISTER_CVAR2("g_glassForceTimeout", &g_glassForceTimeout, 0.f, 0, "Make all glass break after a given time, overrides art settings");
	REGISTER_CVAR2("g_glassForceTimeoutSpread", &g_glassForceTimeoutSpread, 0.f, 0, "Add a random amount to forced glass shattering");
	REGISTER_CVAR2("g_glassNoDecals", &g_glassNoDecals, 0, 0, "Turns off glass decals");
	REGISTER_CVAR2("g_glassAutoShatter", &g_glassAutoShatter, 0, 0, "Always smash the whole pane, and spawn fracture effect");
	REGISTER_CVAR2("g_glassAutoShatterOnExplosions", &g_glassAutoShatterOnExplosions, 0, 0, "Just smash the whole pane, and spawn fracture effect for explosions");
	REGISTER_CVAR2("g_glassAutoShatterMinArea", &g_glassAutoShatterMinArea, 0, 0, "If the area of glass is below this, then autoshatter");
	REGISTER_CVAR2("g_glassMaxPanesToBreakPerFrame", &g_glassMaxPanesToBreakPerFrame, 0, 0, "Max glass breaks, before auto-shattering is forced");

	REGISTER_CVAR2("g_glassSystemEnable", &g_glassSystemEnable, 1, 0, "Enables the new dynamic breaking system for glass");

	REGISTER_CVAR2("g_breakageTreeMax", &g_breakageTreeMax, 0, 0, "Please see comments in ActionGame.cpp");
	REGISTER_CVAR2("g_breakageTreeInc", &g_breakageTreeInc, 0, 0, "Please see comments in ActionGame.cpp");
	REGISTER_CVAR2("g_breakageTreeDec", &g_breakageTreeDec, 0, 0, "Please see comments in ActionGame.cpp");
	REGISTER_CVAR2("g_breakageTreeIncGlass", &g_breakageTreeIncGlass, 0, 0, "Please see comments in ActionGame.cpp");

	REGISTER_CVAR2("g_waterHitOnly", &g_waterHitOnly, 0, 0, "Bullet hit FX appears on water and not what's underneath");

	CIKTorsoAim::InitCVars();
}

// small helper class to make local connections have a fast packet rate - during critical operations
class CAdjustLocalConnectionPacketRate
{
public:
	CAdjustLocalConnectionPacketRate(float rate, float inactivityTimeout)
	{
		m_old = -1.f;
		m_oldInactivityTimeout = -1.f;
		m_oldInactivityTimeoutDev = -1.f;

		if (ICVar* pVar = gEnv->pConsole->GetCVar("g_localPacketRate"))
		{
			m_old = pVar->GetFVal();
			pVar->Set(rate);
		}

		if (ICVar* pVar = gEnv->pConsole->GetCVar("net_inactivitytimeout"))
		{
			m_oldInactivityTimeout = pVar->GetFVal();
			pVar->Set(inactivityTimeout);
		}

		if (ICVar* pVar = gEnv->pConsole->GetCVar("net_inactivitytimeoutDevmode"))
		{
			m_oldInactivityTimeoutDev = pVar->GetFVal();
			pVar->Set(inactivityTimeout);
		}
	}

	~CAdjustLocalConnectionPacketRate()
	{
		if (m_old > 0)
		{
			if (ICVar* pVar = gEnv->pConsole->GetCVar("g_localPacketRate"))
			{
				pVar->Set(m_old);
			}
		}

		if (m_oldInactivityTimeout > 0)
		{
			if (ICVar* pVar = gEnv->pConsole->GetCVar("net_inactivitytimeout"))
			{
				pVar->Set(m_oldInactivityTimeout);
			}
		}

		if (m_oldInactivityTimeoutDev > 0)
		{
			if (ICVar* pVar = gEnv->pConsole->GetCVar("net_inactivitytimeoutDevmode"))
			{
				pVar->Set(m_oldInactivityTimeoutDev);
			}
		}
	}

private:
	float m_old;
	float m_oldInactivityTimeout;
	float m_oldInactivityTimeoutDev;
};

CActionGame::CActionGame(CScriptRMI* pScriptRMI)
	: m_pEntitySystem(gEnv->pEntitySystem)
	, m_pNetwork(gEnv->pNetwork)
	, m_pClientNub(0)
	, m_pServerNub(0)
	, m_pGameClientNub(0)
	, m_pGameServerNub(0)
	, m_pGameContext(0)
	, m_pGameTokenSystem(0)
	, m_pPhysicalWorld(0)
	, m_pGameStats(0)
	, m_pEntHits0(0)
	, m_pCHSlotPool(0)
	, m_lastDynPoolSize(0)
#ifndef _RELEASE
	, m_timeToPromoteToServer(0.f)
#endif
	, m_initState(eIS_Uninited)
	, m_pendingPlaneBreaks(10)
{
	CRY_ASSERT(!s_this);
	s_this = this;

	m_pNetwork->AddHostMigrationEventListener(this, "CActionGame", ELPT_PostEngine);

	m_pGameContext = new CGameContext(CCryAction::GetCryAction(), pScriptRMI, this);
	m_pGameTokenSystem = CCryAction::GetCryAction()->GetIGameTokenSystem();
	m_inDeleteEntityCallback = 0;
}

CActionGame::~CActionGame()
{
#ifndef _RELEASE
	CryLog("Destroying CActionGame instance %p (level=\"%s\")", this, GetLevelName().c_str());
	INDENT_LOG_DURING_SCOPE();
#endif

	m_pNetwork->RemoveHostMigrationEventListener(this);

	if (m_pGameStats)
		m_pGameStats->EndSession();

	{
		IGameSessionHandler* pGameSessionHandler = CCryAction::GetCryAction()->GetIGameSessionHandler();
		pGameSessionHandler->OnGameShutdown();
	}

	if (m_pNetwork)
	{
		if (!gEnv->IsEditor())
		{
			m_pNetwork->SyncWithGame(eNGS_FrameStart);
			m_pNetwork->SyncWithGame(eNGS_FrameEnd);
			m_pNetwork->SyncWithGame(eNGS_WakeNetwork);
		}
		m_pNetwork->SyncWithGame(eNGS_Shutdown);
	}

	if (m_pServerNub)
	{
		m_pServerNub->DeleteNub();
		m_pServerNub = 0;
	}

	if (m_pClientNub)
	{
		m_pClientNub->DeleteNub();
		m_pClientNub = NULL;
	}

	if (gEnv->pNetContext != nullptr)
	{
		gEnv->pNetContext->DeleteContext();
		gEnv->pNetContext = nullptr;
	}

	if (m_pGameContext && m_pGameContext->GetFramework()->GetIGameRulesSystem() &&
	    m_pGameContext->GetFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
		m_pGameContext->GetFramework()->GetIGameRulesSystem()->GetCurrentGameRules()->RemoveHitListener(this);

	if (m_pEntitySystem)
		m_pEntitySystem->ResetAreas(); // this is called again in UnloadLevel(). but we need it here to avoid unwanted events generated when the player entity is deleted.
	SAFE_DELETE(m_pGameContext);
	SAFE_DELETE(m_pGameClientNub);
	SAFE_DELETE(m_pGameServerNub);

	if (m_pNetwork)
	{
		m_pNetwork->SyncWithGame(eNGS_Shutdown_Clear);
	}

	// Pause and wait for the physics
	gEnv->pSystem->SetThreadState(ESubsys_Physics, false);
	EnablePhysicsEvents(false);

	CCryAction* pCryAction = CCryAction::GetCryAction();
	if (pCryAction)
		pCryAction->GetIGameRulesSystem()->DestroyGameRules();

	// ~CGameRules uses game stats so destroy game stats after the game rules
	ReleaseGameStats();

	if (!gEnv->IsDedicated())
	{
		if (ICVar* pDefaultGameRulesCVar = gEnv->pConsole->GetCVar("sv_gamerulesdefault"))
		{
			//gEnv->bMultiplayer = false;
			const char* szDefaultGameRules = pDefaultGameRulesCVar->GetString();
			gEnv->pConsole->GetCVar("sv_gamerules")->Set(szDefaultGameRules);
		}
		if (ICVar* pInputDeviceVar = gEnv->pConsole->GetCVar("sv_requireinputdevice"))
		{
			pInputDeviceVar->Set("dontcare");
		}
#ifdef __WITH_PB__
		gEnv->pConsole->ExecuteString("net_pb_sv_enable false");
#endif
	}

#ifdef __WITH_PB__
	if (gEnv->pNetwork)
		gEnv->pNetwork->CleanupPunkBuster();
#endif

	UnloadLevel();

	gEnv->bServer = false;
	{
		const IGameSessionHandler* pGameSessionHandler = CCryAction::GetCryAction()->GetIGameSessionHandler();
		gEnv->bMultiplayer = pGameSessionHandler ? pGameSessionHandler->IsMultiplayer() : false;
	}
#if CRY_PLATFORM_DESKTOP
	if (!gEnv->IsDedicated()) // Dedi client should remain client
	{
		gEnv->SetIsClient(false);
	}
#endif
	s_this = 0;
}

//////////////////////////////////////////////////////////////////////////
void CActionGame::UnloadLevel()
{
	UnloadPhysicsData();
	CCryAction::GetCryAction()->GetILevelSystem()->UnLoadLevel();
}

//////////////////////////////////////////////////////////////////////////
void CActionGame::UnloadPhysicsData()
{
	// clean up broken object refs
	FixBrokenObjects(false);
	ClearBreakHistory();

	delete[] m_pCHSlotPool;
	m_pCHSlotPool = m_pFreeCHSlot0 = 0;

	SEntityHits* phits, * phitsPool;
	int i;
	for (i = 0, phits = m_pEntHits0, phitsPool = 0; phits; phits = phits->pnext, i++)
	{
		if (phits->pHits != &phits->hit0) delete[] phits->pHits;
		if (phits->pnHits != &phits->nhits0) delete[] phits->pnHits;
		if ((i & 31) == 0)
		{
			if (phitsPool) delete[] phitsPool; phitsPool = phits;
		}
	}
	delete[] phitsPool;
	m_vegStatus.clear();
}

ILINE bool CActionGame::AllowProceduralBreaking(uint8 proceduralBreakType)
{
	// bit 1 : test state
	// bit 2 : and (true), or (false)
	// bit 3 : current state
	// bit 4 : skip flag
	static const uint16 truth = 0xf0b2;
#define UPDATE_VAL(skip, isand, test) val = ((truth >> (((skip) << 3) | ((val) << 2) | ((isand) << 1) | (test))) & 1)

	uint8 val = 0;
	// single player defaults to on, multiplayer to
	UPDATE_VAL(0, 0, (m_proceduralBreakFlags & ePBF_DefaultAllow) != 0);
	UPDATE_VAL(0, 0, proceduralBreakType == ePBT_Glass);
	UPDATE_VAL((m_proceduralBreakFlags & ePBF_ObeyCVar) == 0, 1, g_procedural_breaking != 0);
	return val == 1;
}

void CActionGame::BackupGameStartParams(const SGameStartParams* pGameStartParams)
{
	// This lot has to be deep copied now.  It usually lives on the stack in a calling function
	// but some of it is used by the BlockingConnect process.
	m_startGameParams = *pGameStartParams;
	m_hostname = m_startGameParams.hostname;
	m_connectionString = m_startGameParams.connectionString;
	m_startGameParams.hostname = m_hostname.c_str();
	m_startGameParams.connectionString = m_connectionString.c_str();

	if (pGameStartParams->pContextParams)
	{
		m_levelName = pGameStartParams->pContextParams->levelName;
		m_gameRules = pGameStartParams->pContextParams->gameRules;
		m_demoRecorderFilename = pGameStartParams->pContextParams->demoRecorderFilename;
		m_demoPlaybackFilename = pGameStartParams->pContextParams->demoPlaybackFilename;

		m_gameContextParams.levelName = m_levelName.c_str();
		m_gameContextParams.gameRules = m_gameRules.c_str();
		m_gameContextParams.demoRecorderFilename = m_demoRecorderFilename.c_str();
		m_gameContextParams.demoPlaybackFilename = m_demoPlaybackFilename.c_str();

		m_startGameParams.pContextParams = &m_gameContextParams;
	}
}

bool CActionGame::Init(const SGameStartParams* pGameStartParams)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "ActionGame::Init");

	if (!pGameStartParams)
	{
		m_initState = eIS_InitError;
		return false;
	}

	BackupGameStartParams(pGameStartParams);

	m_initState = eIS_Initing;

	m_fadeEntities.resize(0);

	memset(&m_throttling, 0, sizeof(m_throttling));

	// initialize client server infrastructure

	CAdjustLocalConnectionPacketRate adjustLocalPacketRate(50.0f, 30.0f);

	uint32 ctxFlags = 0;
	if ((pGameStartParams->flags & eGSF_Server) == 0 || (pGameStartParams->flags & eGSF_LocalOnly) == 0)
		ctxFlags |= INetwork::eNCCF_Multiplayer;
	if (m_pNetwork)
	{
		gEnv->pNetContext = m_pNetwork->CreateNetContext(m_pGameContext, ctxFlags);
		m_pGameContext->Init(gEnv->pNetContext);
	}

	if (gEnv->pAISystem)
	{
		ICVar* pAIEnabled = NULL;
		if (gEnv->bMultiplayer)
		{
			if (pGameStartParams->flags & eGSF_Server)
			{
				pAIEnabled = gEnv->pConsole->GetCVar("sv_AISystem");
			}
			else if (pGameStartParams->flags & eGSF_Client)
			{
				pAIEnabled = gEnv->pConsole->GetCVar("cl_AISystem");
			}
		}

		bool aiSystemEnable = (pAIEnabled ? pAIEnabled->GetIVal() != 0 : true);

		gEnv->pAISystem->Enable(aiSystemEnable);
	}

	CRY_ASSERT(m_pGameServerNub == NULL);
	CRY_ASSERT(m_pServerNub == NULL);

	bool ok = true;
	bool clientRequiresBlockingConnection = false;

	string connectionString = pGameStartParams->connectionString;
	if (!connectionString)
		connectionString = "";

	unsigned flags = pGameStartParams->flags;
	if (pGameStartParams->flags & eGSF_Server)
	{
		ICVar* pReqInput = gEnv->pConsole->GetCVar("sv_requireinputdevice");
		if (pReqInput)
		{
			const char* what = pReqInput->GetString();
			if (0 == strcmpi(what, "none"))
			{
				flags &= ~(eGSF_RequireKeyboardMouse | eGSF_RequireController);
			}
			else if (0 == strcmpi(what, "keyboard"))
			{
				flags |= eGSF_RequireKeyboardMouse;
				flags &= ~eGSF_RequireController;
			}
			else if (0 == strcmpi(what, "gamepad"))
			{
				flags |= eGSF_RequireController;
				flags &= ~eGSF_RequireKeyboardMouse;
			}
			else if (0 == strcmpi(what, "dontcare"))
				;
			else
			{
				GameWarning("Invalid sv_requireinputdevice %s", what);
				m_initState = eIS_InitError;
				return false;
			}
		}
		//Set the gamerules & map cvar to keep things in sync
		if (pGameStartParams->pContextParams)
		{
			gEnv->pConsole->GetCVar("sv_gamerules")->Set(pGameStartParams->pContextParams->gameRules);
			gEnv->pConsole->GetCVar("sv_map")->Set(pGameStartParams->pContextParams->levelName);
		}
	}
	else
	{
		ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby && gEnv->bMultiplayer)
		{
			const char* pLevelName = pLobby->GetCryEngineLevelNameHint();
			if (strlen(pLevelName) != 0)
			{
				m_pGameContext->SetLevelName(pLevelName);
			}
			const char* pRulesName = pLobby->GetCryEngineRulesNameHint();
			if (strlen(pRulesName) != 0)
			{
				m_pGameContext->SetGameRules(pRulesName);
			}
		}
	}

	// we need to fake demo playback as not LocalOnly, otherwise things not breakable in demo recording will become breakable
	if (flags & eGSF_DemoPlayback)
		flags &= ~eGSF_LocalOnly;

	m_pGameContext->SetContextInfo(flags, pGameStartParams->port, connectionString.c_str());

	// although demo playback doesn't allow more than one client player (the local spectator), it should still be multiplayer to be consistent
	// with the recording session (otherwise, things not physicalized in demo recording (multiplayer) will be physicalized in demo playback)
	bool isMultiplayer = !m_pGameContext->HasContextFlag(eGSF_LocalOnly) || (flags & eGSF_DemoPlayback);

	const char* configName = isMultiplayer ? "multiplayer" : "singleplayer";

	gEnv->pSystem->LoadConfiguration(configName);

	gEnv->bMultiplayer = isMultiplayer;
	gEnv->bServer = m_pGameContext->HasContextFlag(eGSF_Server);

#if CRY_PLATFORM_DESKTOP
	gEnv->SetIsClient(m_pGameContext->HasContextFlag(eGSF_Client));
#endif

	if (gEnv->pNetwork)
	{
		if (gEnv->bMultiplayer)
		{
			gEnv->pNetwork->SetMultithreadingMode(INetwork::NETWORK_MT_PRIORITY_HIGH);
		}
		else
		{
			gEnv->pNetwork->SetMultithreadingMode(INetwork::NETWORK_MT_PRIORITY_NORMAL);
		}

		SNetGameInfo gameInfo;
		gameInfo.maxPlayers = pGameStartParams->maxPlayers;
		gEnv->pNetwork->SetNetGameInfo(gameInfo);

		InitImmersiveness();
	}

	// perform some basic initialization/resetting
	if (!gEnv->IsEditor())
	{
		if (!gEnv->pSystem->IsSerializingFile()) //GameSerialize will reset and reserve in the right order
			gEnv->pEntitySystem->Reset();
	}

	m_pPhysicalWorld = gEnv->pPhysicalWorld;
	m_pFreeCHSlot0 = m_pCHSlotPool = new SEntityCollHist[32];
	int i;
	for (i = 0; i < 31; i++)
		m_pCHSlotPool[i].pnext = m_pCHSlotPool + i + 1;
	m_pCHSlotPool[i].pnext = m_pCHSlotPool;

	m_pEntHits0 = new SEntityHits[32];
	for (i = 0; i < 32; i++)
	{
		m_pEntHits0[i].pHits = &m_pEntHits0[i].hit0;
		m_pEntHits0[i].pnHits = &m_pEntHits0[i].nhits0;
		m_pEntHits0[i].nHits = 0;
		m_pEntHits0[i].nHitsAlloc = 1;
		m_pEntHits0[i].pnext = m_pEntHits0 + i + 1;
		m_pEntHits0[i].timeUsed = m_pEntHits0[i].lifeTime = 0;
	}
	m_pEntHits0[i - 1].pnext = 0;
	m_totBreakageSize = 0;

	CCryAction::GetCryAction()->AllowSave(true);
	CCryAction::GetCryAction()->AllowLoad(true);

	EnablePhysicsEvents(true);
	m_bLoading = false;

	m_nEffectCounter = 0;
	m_proceduralBreakFlags = ePBF_DefaultAllow | ePBT_Glass | ePBF_ObeyCVar;

	bool hasPbSvStarted = false;

	// TODO : Server announce here
	if (m_pGameContext->HasContextFlag(eGSF_Server))
	{
		ServerInit(pGameStartParams, &ok, &hasPbSvStarted);
	}

	CreateGameStats();//Initialize stats tracking object

	if (ok && (m_pGameContext->HasContextFlag(eGSF_Client) || m_pGameContext->HasContextFlag(eGSF_DemoRecorder)))
	{
		ClientInit(pGameStartParams, &ok, &hasPbSvStarted, &clientRequiresBlockingConnection);
	}

	m_lastDynPoolSize = 0;

	PostInit(pGameStartParams, &ok, &clientRequiresBlockingConnection);

	return ok;
}

void CActionGame::ServerInit(const SGameStartParams* pGameStartParams, bool* io_ok, bool* io_hasPbSvStarted)
{
	bool& ok = *io_ok;
	bool& hasPbSvStarted = *io_hasPbSvStarted;

	CRY_ASSERT(m_pGameContext->GetServerPort() != 0);

#ifdef  __WITH_PB__
	if (CCryAction::GetCryAction()->IsPbSvEnabled() && gEnv->bMultiplayer)
	{
		gEnv->pNetwork->StartupPunkBuster(true);
		hasPbSvStarted = true;
	}
#endif

	m_pGameServerNub = new CGameServerNub();
	m_pGameServerNub->SetGameContext(m_pGameContext);
	m_pGameServerNub->SetMaxPlayers(pGameStartParams->maxPlayers);

	char address[256];
	if (pGameStartParams->flags & eGSF_LocalOnly)
	{
		cry_sprintf(address, "%s:%u", LOCAL_CONNECTION_STRING, pGameStartParams->port);
	}
	else
	{
		ICVar* pCVar = gEnv->pConsole->GetCVar("sv_bind");
		if (pCVar && pCVar->GetString())
		{
			cry_sprintf(address, "%s:%u", pCVar->GetString(), pGameStartParams->port);
		}
		else
		{
			cry_sprintf(address, "0.0.0.0:%u", pGameStartParams->port);
		}
	}

	IGameQuery* pGameQuery = m_pGameContext;
	if (m_pGameContext->HasContextFlag(eGSF_NoQueries))
		pGameQuery = NULL;
	if (m_pNetwork)
		m_pServerNub = m_pNetwork->CreateNub(address, m_pGameServerNub, 0, pGameQuery);

	if (!m_pServerNub)
	{
		ok = false;
	}
}

void CActionGame::ClientInit(const SGameStartParams* pGameStartParams, bool* io_ok, bool* io_hasPbSvStarted, bool* io_requireBlockingConnection)
{
	bool& ok = *io_ok;
	bool& hasPbSvStarted = *io_hasPbSvStarted;
	bool& clientRequiresBlockingConnection = *io_requireBlockingConnection;

#ifdef __WITH_PB__
	if (hasPbSvStarted || CCryAction::GetCryAction()->IsPbClEnabled() && gEnv->bMultiplayer)
		gEnv->pNetwork->StartupPunkBuster(false);
#endif

	m_pGameClientNub = new CGameClientNub(CCryAction::GetCryAction());
	m_pGameClientNub->SetGameContext(m_pGameContext);

	const char* hostname;
	const char* clientname;
	if (m_pGameContext->HasContextFlag(eGSF_Server))
	{
		clientname = hostname = LOCAL_CONNECTION_STRING;
	}
	else
	{
		hostname = pGameStartParams->hostname;
		clientname = "0.0.0.0";
	}
	CRY_ASSERT(hostname);

	string addressFormatter;
	if (strchr(hostname, ':') == 0)
	{
		if (pGameStartParams->session != CrySessionInvalidHandle)
		{
			addressFormatter.Format("<session>%08X,%s:%u", pGameStartParams->session, hostname, m_pGameContext->GetServerPort());
		}
		else
		{
			addressFormatter.Format("%s:%u", hostname, m_pGameContext->GetServerPort());
		}

	}
	else
	{
		if (pGameStartParams->session != CrySessionInvalidHandle)
		{
			addressFormatter.Format("<session>%08X,%s", pGameStartParams->session, hostname);
		}
		else
		{
			addressFormatter = hostname;
		}
	}
	string whereFormatter;
	whereFormatter.Format("%s:0", clientname);

	ok = false;

	m_pClientNub = m_pNetwork->CreateNub(whereFormatter.c_str(), m_pGameClientNub, 0, 0);

	if (!m_pClientNub)
		return;

	if (!m_pClientNub->ConnectTo(addressFormatter.c_str(), m_pGameContext->GetConnectionString(NULL, false)))
		return;

	clientRequiresBlockingConnection |= m_pGameContext->HasContextFlag(eGSF_BlockingClientConnect);

	ok = true;
}

void CActionGame::PostInit(const SGameStartParams* pGameStartParams, bool* io_ok, bool* io_requireBlockingConnection)
{
	const bool bIsEditor = gEnv->IsEditor();
	const bool bIsDedicated = gEnv->IsDedicated();
	const bool bNonBlocking = (pGameStartParams->flags & eGSF_NonBlockingConnect) != 0;

	if (!bIsEditor && !bIsDedicated && bNonBlocking)
	{
		m_initState = eIS_WaitForConnection;
	}
	else
	{
		bool& ok = *io_ok;
		bool& clientRequiresBlockingConnection = *io_requireBlockingConnection;

		if (ok && m_pGameContext->HasContextFlag(eGSF_Server) && m_pGameContext->HasContextFlag(eGSF_Client))
			clientRequiresBlockingConnection = true;

		if (ok && clientRequiresBlockingConnection)
		{
			ok &= BlockingConnect(&CActionGame::ConditionHaveConnection, true, "have connection");
		}

		if (ok && m_pGameContext->HasContextFlag(eGSF_Server) && !ChangeGameContext(pGameStartParams->pContextParams))
		{
			ok = false;
		}

		if (ok && m_pGameStats)
			m_pGameStats->StartSession();

		if (ok && m_pGameContext->HasContextFlag(eGSF_BlockingClientConnect) && !m_pGameContext->HasContextFlag(eGSF_NoSpawnPlayer) && m_pGameContext->HasContextFlag(eGSF_Client))
		{
			ok &= BlockingConnect(&CActionGame::ConditionHavePlayer, true, "have player");
		}

		if (ok && m_pGameContext->HasContextFlag(eGSF_BlockingMapLoad))
		{
			ok &= BlockingConnect(&CActionGame::ConditionInGame, false, "in game");
		}

		m_initState = eIS_InitDone;
	}
}

void CActionGame::LogModeInformation(const bool isMultiplayer, const char* hostname) const
{
	assert(gEnv->pSystem);

	if (gEnv->IsEditor())
	{
		CryLogAlways("Starting in Editor mode");
	}
	else if (gEnv->IsDedicated())
	{
		CryLogAlways("Starting in Dedicated server mode");
	}
	else if (!isMultiplayer)
	{
		CryLogAlways("Starting in Singleplayer mode");
	}
	else if (IsServer())
	{
		CryLogAlways("Starting in Server mode");
	}
	else if (m_pGameContext->HasContextFlag(eGSF_Client))
	{
		CryLogAlways("Starting in Client mode");
		string address = hostname;
		size_t position = address.find(":");
		if (position != string::npos)
		{
			address = address.substr(0, position);
		}
		CryLogAlways("ServerName: %s", address.c_str());
	}
}

void CActionGame::UpdateImmersiveness()
{
	bool procMP = !m_pGameContext->HasContextFlag(eGSF_LocalOnly);
	bool immMP = m_pGameContext->HasContextFlag(eGSF_ImmersiveMultiplayer);
	m_proceduralBreakFlags =
	  (int)/*(!procMP) **/ ePBF_ObeyCVar +
	  (int)procMP * ePBF_AllowGlass +
	  (int)(immMP || !procMP) * ePBF_DefaultAllow;
	if (!m_pGameContext->HasContextFlag(eGSF_Server))
		m_proceduralBreakFlags &= ~(ePBF_DefaultAllow | ePBF_AllowGlass);
	bool isServer = m_pGameContext->HasContextFlag(eGSF_Server);

	PhysicsVars* physVars = gEnv->pPhysicalWorld->GetPhysVars();
	physVars->breakImpulseScale = 1.0f * (isServer && (immMP || !procMP && g_joint_breaking));

	if (g_breakageNoDebrisCollisions)
	{
		// Make debris collide with only static geometry
		physVars->massLimitDebris = 1000.f;
		physVars->flagsColliderDebris = geom_colltype_debris;
		physVars->flagsANDDebris = geom_colltype_solid;
	}
	else
	{
		physVars->massLimitDebris = 1e10f;
		MARK_UNUSED physVars->flagsColliderDebris;
		physVars->flagsANDDebris = ~0;
	}

	// If set to 1.0f, this forces broken trees to have spherical inertia, which makes them harder to spin about their vertical axis
	physVars->breakageMinAxisInertia = g_breakageMinAxisInertia;

	if (gEnv->bMultiplayer && gEnv->bServer)
	{
		if (immMP)
		{
			static ICVar* pLength = gEnv->pConsole->GetCVar("sv_timeofdaylength");
			static ICVar* pStart = gEnv->pConsole->GetCVar("sv_timeofdaystart");
			static ICVar* pTOD = gEnv->pConsole->GetCVar("sv_timeofdayenable");

			ITimeOfDay::SAdvancedInfo advancedInfo;
			gEnv->p3DEngine->GetTimeOfDay()->GetAdvancedInfo(advancedInfo);

			advancedInfo.fAnimSpeed = 0.0f;
			if (pTOD && pTOD->GetIVal())
			{
				advancedInfo.fStartTime = pStart ? pStart->GetFVal() : 0.0f;
				advancedInfo.fEndTime = 24.0f;
				if (pLength)
				{
					float lengthInHours = pLength->GetFVal();
					if (lengthInHours > 0.01f)
					{
						lengthInHours = CLAMP(pLength->GetFVal(), 0.2f, 24.0f);
						advancedInfo.fAnimSpeed = 1.0f / lengthInHours / 150.0f;
					}
					advancedInfo.fEndTime = advancedInfo.fStartTime + lengthInHours;
				}
			}
			gEnv->p3DEngine->GetTimeOfDay()->SetAdvancedInfo(advancedInfo);
		}
	}

	if (immMP && !m_pGameContext->HasContextFlag(eGSF_Server))
		gEnv->p3DEngine->GetTimeOfDay()->SetTimer(CServerTimer::Get());
	else
		gEnv->p3DEngine->GetTimeOfDay()->SetTimer(gEnv->pTimer);
}

void CActionGame::InitImmersiveness()
{
	bool immMP = m_pGameContext->HasContextFlag(eGSF_ImmersiveMultiplayer);
	UpdateImmersiveness();

	if (gEnv->bMultiplayer)
	{
		gEnv->pSystem->SetConfigSpec(immMP ? CONFIG_VERYHIGH_SPEC : CONFIG_LOW_SPEC, false);
		if (immMP)
		{
			if (gEnv->pConsole->GetCVar("sv_timeofdayenable")->GetIVal())
			{
				static ICVar* pStart = gEnv->pConsole->GetCVar("sv_timeofdaystart");
				static ICVar* pTOD = gEnv->pConsole->GetCVar("e_TimeOfDay");
				if (pStart && pTOD)
					pTOD->Set(pStart->GetFVal());
			}
		}
	}
}

bool CActionGame::BlockingSpawnPlayer()
{
	CAdjustLocalConnectionPacketRate adjuster(50.0f, 30.0f);

	assert(gEnv->IsEditor());

	if (!m_pGameContext)
		return false;
	if (!m_pGameContext->HasContextFlag(eGSF_BlockingClientConnect) || !m_pGameContext->HasContextFlag(eGSF_NoSpawnPlayer))
		return false;
	if (!m_pGameServerNub)
		return false;
	TServerChannelMap* pChannelMap = m_pGameServerNub->GetServerChannelMap();
	if (!pChannelMap)
		return false;
	if (pChannelMap->size() != 1)
		return false;
	CGameServerChannel* pChannel = pChannelMap->begin()->second;

	m_pGameContext->AllowCallOnClientConnect();

	return BlockingConnect(&CActionGame::ConditionInGame, true, "in game");
}

bool CActionGame::ConditionHaveConnection(CGameClientChannel* pChannel)
{
	return pChannel->GetNetChannel()->IsConnectionEstablished();
}

bool CActionGame::ConditionHavePlayer(CGameClientChannel* pChannel)
{
	return pChannel->GetPlayerId() != 0;
}

bool CActionGame::ConditionInGame(CGameClientChannel* pChannel)
{
	return CCryAction::GetCryAction()->IsGameStarted() && !CCryAction::GetCryAction()->IsGamePaused();
}

CActionGame::eInitTaskState CActionGame::NonBlockingConnect(BlockingConditionFunction condition, bool requireClientChannel, const char* conditionText)
{
	bool done = false;

	CGameClientChannel* pChannel = NULL;
	if (requireClientChannel)
	{
		if (!m_pGameClientNub)
		{
			GameWarning("NonBlockingConnect: Client nub doesn't exist while waiting for condition '%s'", conditionText);
			return eITS_Error;
		}
		pChannel = m_pGameClientNub->GetGameClientChannel();
		if (!pChannel && IsStale()) // '||' => '&&' (see notes below)
		{
			GameWarning("NonBlockingConnect: Disconnected while waiting for condition '%s'", conditionText);
			return eITS_Error;
		}
	}
	// NOTE: because now we have pre-channel hand-shaking (key exchange), it is legal that
	// a GameChannel will be created a while later than a GameNub is created - Lin
	if (!requireClientChannel || pChannel)
		done = (this->*condition)(pChannel);

	return (done ? eITS_Done : eITS_InProgress);
}

bool CActionGame::BlockingConnect(BlockingConditionFunction condition, bool requireClientChannel, const char* conditionText)
{
	LOADING_TIME_PROFILE_SECTION
	  MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "BlockingConnect");

	bool ok = false;

	ITimer* pTimer = gEnv->pTimer;
	CTimeValue startTime = pTimer->GetAsyncTime();

	while (!ok)
	{
		m_pNetwork->SyncWithGame(eNGS_FrameStart);
		m_pNetwork->SyncWithGame(eNGS_FrameEnd);
		m_pNetwork->SyncWithGame(eNGS_WakeNetwork);
		gEnv->pTimer->UpdateOnFrameStart();
		CGameClientChannel* pChannel = NULL;
		if (requireClientChannel)
		{
			if (!m_pGameClientNub)
			{
				GameWarning("BlockingConnect: Client nub doesn't exist while waiting for condition '%s' (after %.2f seconds)", conditionText, (pTimer->GetAsyncTime() - startTime).GetSeconds());
				break;
			}
			pChannel = m_pGameClientNub->GetGameClientChannel();
			if (!pChannel && IsStale()) // '||' => '&&' (see notes below)
			{
				GameWarning("BlockingConnect: Disconnected while waiting for condition '%s' (after %.2f seconds)", conditionText, (pTimer->GetAsyncTime() - startTime).GetSeconds());
				break;
			}
		}
		// NOTE: because now we have pre-channel hand-shaking (key exchange), it is legal that
		// a GameChannel will be created a while later than a GameNub is created - Lin
		if (!requireClientChannel || pChannel)
			ok |= (this->*condition)(pChannel);

		if (!gEnv || !gEnv->pSystem || gEnv->pSystem->IsQuitting())
			break;                                                      // FIX DT- 20377 PC : SP : DESIGN : CRASH : Pure function call engine error followed by title crash when user hits X button or uses Alt F4 during loading screen
	}

	if (ok && gEnv && !gEnv->IsEditor())
	{
		float numSecondsTaken = (pTimer->GetAsyncTime() - startTime).GetSeconds();

		if (numSecondsTaken > 2.0f)
		{
			GameWarning("BlockingConnect: It's taken %.2f seconds to achieve condition '%s' - either you're on slow connection, or you're doing something intensive", numSecondsTaken, conditionText);
		}
	}

	if (ok == false)
	{
		float numSecondsTaken = (pTimer->GetAsyncTime() - startTime).GetSeconds();
		CryLog("BlockingConnect: Failed to achieve condition '%s' (tried for %.2f seconds)", conditionText, numSecondsTaken);
	}

	return ok;
}

bool CActionGame::ChangeGameContext(const SGameContextParams* pGameContextParams)
{
	if (!IsServer())
	{
		GameWarning("Can't ChangeGameContext() on client");
		CRY_ASSERT(!"Can't ChangeGameContext() on client");
		return false;
	}

	CRY_ASSERT(pGameContextParams);

	return m_pGameContext->ChangeContext(true, pGameContextParams);
}

IActor* CActionGame::GetClientActor()
{
	if (!m_pGameClientNub)
		return NULL;

	CGameClientChannel* pGameClientChannel = m_pGameClientNub->GetGameClientChannel();
	if (!pGameClientChannel)
		return NULL;
	EntityId playerId = pGameClientChannel->GetPlayerId();
	if (!playerId)
		return NULL;
	if (m_pGameContext->GetNetContext()->IsDemoPlayback())
		return gEnv->pGameFramework->GetIActorSystem()->GetCurrentDemoSpectator();

	return CCryAction::GetCryAction()->GetIActorSystem()->GetActor(playerId);
}

bool CActionGame::Update()
{
	if (m_initState == eIS_InitDone)
	{
		const float deltaTime = gEnv->pTimer->GetFrameTime();
		_smart_ptr<CActionGame> pThis(this);

		IGameRulesSystem* pgrs;
		IGameRules* pgr;
		if (m_pGameContext && (pgrs = m_pGameContext->GetFramework()->GetIGameRulesSystem()) && (pgr = pgrs->GetCurrentGameRules()))
			pgr->AddHitListener(this);
		UpdateImmersiveness();

		CServerTimer::Get()->UpdateOnFrameStart();

#ifdef _GAMETOKENSDEBUGINFO
		if (m_pGameTokenSystem)
			m_pGameTokenSystem->DebugDraw();
#endif

		if (g_breakage_debug)
			DrawBrokenMeshes();
		UpdateBrokenMeshes(deltaTime);

		if (gEnv->bMultiplayer)
		{
			m_throttling.m_brokenTreeCounter -= g_breakageTreeDec;
			if (m_throttling.m_brokenTreeCounter < 0)
				m_throttling.m_brokenTreeCounter = 0;
			if (m_throttling.m_numGlassEvents > 0)
				m_throttling.m_numGlassEvents--;
			UpdateFadeEntities(deltaTime);
		}

		if (m_pGameStats && IsServer())
			m_pGameStats->Update();

		if (s_waterMaterialId == -1)
		{
			s_waterMaterialId = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceTypeByName("mat_water")->GetId();
		}
		// if the game context returns true, then it wants to abort it's mission
		// that probably means that loading the level failed... and in any case
		// it also probably means that we've been deleted.
		// only call IsStale if the game context indicates it's not yet finished
		// (returns false) - is stale will check if we're a client based system
		// and still have an active client
		return (m_pGameContext && m_pGameContext->Update()) || IsStale();
	}
	else
	{
		switch (m_initState)
		{
		case eIS_WaitForConnection:
			{
				eInitTaskState done = NonBlockingConnect(&CActionGame::ConditionHaveConnection, true, "have connection");

				if (done == eITS_Error)
				{
					m_initState = eIS_InitError;
					break;
				}
				else if (done == eITS_Done)
				{
					if (m_pGameContext->HasContextFlag(eGSF_Server) && !ChangeGameContext(m_startGameParams.pContextParams))
					{
						m_initState = eIS_InitError;
						break;
					}

					if (m_pGameStats)
					{
						m_pGameStats->StartSession();
					}

					m_initState = eIS_WaitForPlayer;
				}
				break;
			}

		case eIS_WaitForPlayer:
			{
				const bool bNoSpawnPlayer = m_pGameContext->HasContextFlag(eGSF_NoSpawnPlayer);
				const bool bClient = m_pGameContext->HasContextFlag(eGSF_Client);

				if (!bNoSpawnPlayer && bClient)
				{
					eInitTaskState done = NonBlockingConnect(&CActionGame::ConditionHavePlayer, true, "have player");

					if (done == eITS_Error)
					{
						m_initState = eIS_InitError;
						break;
					}
					else if (done == eITS_Done)
					{
						m_initState = eIS_WaitForInGame;
					}
				}
				else
				{
					m_initState = eIS_WaitForInGame;
				}
			}
			break;

		case eIS_WaitForInGame:
			{
				eInitTaskState done = NonBlockingConnect(&CActionGame::ConditionInGame, false, "in game");

				if (done == eITS_Error)
				{
					m_initState = eIS_InitError;
					break;
				}
				else if (done == eITS_Done)
				{
					m_initState = eIS_InitDone;
				}
			}
			break;

		default:
		case eIS_InitError:
			// returning true causes this ActionGame object to be deleted
			return true;
		}
	}

	return false;
}

void CActionGame::OnBreakageSpawnedEntity(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity)
{
	// For now this is MP only
	assert(gEnv->bMultiplayer);

	// The breakable manager spawned a new entity - add it to the fade list
	if (pSrcPhysEntity != pPhysEntity)
	{
		DynArray<SEntityFadeState>::iterator it = m_fadeEntities.push_back();
		it->entId = pEntity->GetId();
		it->time = 0.f;
		it->bCollisions = 1;
	}
}

void CActionGame::UpdateFadeEntities(float dt)
{
	if (const int N = m_fadeEntities.size())
	{
		int n = 0;
		SEntityFadeState* p = &m_fadeEntities[0];
		const float inv = 1.f / (g_breakageFadeTime + 0.01f);
		for (int i = 0; i < N; i++)
		{
			SEntityFadeState* state = &m_fadeEntities[i];
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(state->entId))
			{
				{
					const float newTime = state->time + dt;
					const float t = newTime - g_breakageFadeDelay;
					IPhysicalEntity* pent = pEntity->GetPhysics();
					if (t >= g_breakageFadeTime)
					{
						FreeBrokenMeshesForEntity(pent);
						continue;
					}
					if (t > 0.f)
					{
						float opacity = 1.f - t * inv;
						pEntity->SetOpacity(opacity);
						if (pent && state->bCollisions)
						{
							// Turn off some collisions
							// NB: by leaving pe_params_part.ipart undefined, all the geom flags will changed
							pe_params_part pp;
							pp.flagsAND = ~(geom_colltype_ray | geom_colltype_vehicle | geom_colltype_player);
							pent->SetParams(&pp);
							state->bCollisions = 0;
						}
					}
					state->time = newTime;
					*p = *state;
					++n;
					++p;
				}
			}
		}
		if (n != N)
		{
			m_fadeEntities.resize(n);
		}
	}
}

IGameObject* CActionGame::GetEntityGameObject(IEntity* pEntity)
{
	return static_cast<CGameObject*>(pEntity ? pEntity->GetProxy(ENTITY_PROXY_USER) : 0);
}

IGameObject* CActionGame::GetPhysicalEntityGameObject(IPhysicalEntity* pPhysEntity)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPhysEntity);
	if (pEntity)
		return static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
	return 0;
}

bool CActionGame::IsStale()
{
	if (m_pGameClientNub && m_pClientNub)
		if (!m_pClientNub->IsConnecting() && !m_pGameClientNub->GetGameClientChannel() && !CCryAction::GetCryAction()->IsGameSessionMigrating())
			return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////
// Host Migration listener
IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	CryLogAlways("[Host Migration]: CActionGame::OnInitiate() started");

	if (gEnv->pInput)
	{
		//disable rumble
		gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eIDT_Gamepad, eFF_Rumble_Basic, SFFTriggerOutputData::Initial::ZeroIt, 0.0f, 0.0f, 0.0f));
	}
	IForceFeedbackSystem* pForceFeedbackSystem = gEnv->pGameFramework->GetIForceFeedbackSystem();
	if (pForceFeedbackSystem)
	{
		pForceFeedbackSystem->StopAllEffects();
	}

	// Save any state information that needs preserving across the migration
	IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();

	if (s_this->m_pGameClientNub == NULL)
	{
		return IHostMigrationEventListener::Listener_Terminate;
	}

	// Store migrating player name (used for the migrating connection handshake string)
	CGameClientChannel* pGameClientChannel = s_this->m_pGameClientNub->GetGameClientChannel();
	if (pGameClientChannel != NULL)
	{
		hostMigrationInfo.m_playerID = pGameClientChannel->GetPlayerId();
		IActor* pActor = pActorSystem->GetActor(hostMigrationInfo.m_playerID);

		if (pActor)
		{
			hostMigrationInfo.SetMigratedPlayerName(pActor->GetEntity()->GetName());
		}
		else
		{
			CryLogAlways("[Host Migration]: no actor found - aborting");
			return IHostMigrationEventListener::Listener_Terminate;
		}
	}

	CryLogAlways("[Host Migration]: CActionGame::OnInitiate() finished");
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	bool done = false;

	switch (state)
	{
	case eDS_Disconnect:
		CryLogAlways("[Host Migration]: CActionGame::OnDisconnectClient() started");
		// Do the disconnect
		if (m_pGameClientNub)
		{
			m_pGameClientNub->Disconnect(eDC_UserRequested, "Host migrating");
		}
		if (m_pClientNub != NULL)
		{
			m_pClientNub->DeleteNub();
			m_pClientNub = NULL;
		}
		else
		{
			CryLog("[Host Migration]: CActionGame::OnDisconnectClient() - m_pClientNub is NULL");
		}

		// TODO: Don't think SyncWithGame(eNGS_Shutdown) is necessary
		//s_this->m_pNetwork->SyncWithGame(eNGS_Shutdown);

		state = eDS_Disconnecting;
		break;

	case eDS_Disconnecting:
		CryLogAlways("[Host Migration]: CActionGame::OnDisconnectClient() waiting");
		// Wait for the disconnect to complete
		if (m_pGameClientNub)
		{
			if (m_pGameClientNub->GetGameClientChannel() == NULL)
			{
				state = eDS_Disconnected;
			}
		}
		else
		{
			state = eDS_Disconnected;
		}
		break;

	case eDS_Disconnected: // Intentional fall-through
		if (CCryAction::GetCryAction()->GetIMaterialEffects())
		{
			// This is a speculative fix for a crash that happened where a delayed decal was created that referenced a deleted CStatObj on a host migration.
			// Curiously it was a bit of glass (probably broken) and its ref count was -1 (!). So this won't fix the cause, but should help stop that particular crash.
			CCryAction::GetCryAction()->GetIMaterialEffects()->ClearDelayedEffects();
		}

		CryLogAlways("[Host Migration]: CActionGame::OnDisconnectClient() finished");
	default:
		done = true;
		break;
	}

	if (done)
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	return IHostMigrationEventListener::Listener_Wait;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	CryLogAlways("[Host Migration]: CActionGame::OnDemoteToClient() started");

	if (s_this->m_pServerNub)
	{
		s_this->m_pServerNub->DeleteNub();
		s_this->m_pServerNub = NULL;
	}

	gEnv->bServer = false;

	CryLogAlways("[Host Migration]: CActionGame::OnDemoteToClient() finished");
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	CryLogAlways("[Host Migration]: CActionGame::OnPromoteToServer() started");

	// Create a server on this machine
	gEnv->bServer = true;

	// Pause to allow the feature tester to run host migration tests on a single pc, must be after bServer is set so that other OnPromoteToServer listeners work
#ifndef _RELEASE
	if (g_hostMigrationServerDelay > 0.f)
	{
		const CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
		const float currentTimeInSeconds = currentTime.GetSeconds();

		if (m_timeToPromoteToServer == 0.f)
		{
			m_timeToPromoteToServer = currentTimeInSeconds + g_hostMigrationServerDelay;
			return IHostMigrationEventListener::Listener_Wait;
		}

		if (currentTimeInSeconds < m_timeToPromoteToServer)
		{
			return IHostMigrationEventListener::Listener_Wait;
		}
	}
#endif

	IConsole* pConsole = gEnv->pConsole;
	ICVar* pCVar = pConsole->GetCVar("sv_maxplayers");
	int maxPlayers = pCVar->GetIVal();

	// Set the server name
	IPlayerProfileManager* pPlayerProfileManager = gEnv->pGameFramework->GetIPlayerProfileManager();
	CryFixedStringT<128> serverName(s_this->m_pNetwork->GetHostName());
	serverName.append(" ");
	serverName.append(pPlayerProfileManager->GetCurrentProfile(pPlayerProfileManager->GetCurrentUser())->GetName());
	gEnv->pConsole->GetCVar("sv_servername")->Set(serverName.c_str());

	// Create a new game server nub
	s_this->m_pGameServerNub = new CGameServerNub();
	s_this->m_pGameServerNub->SetGameContext(s_this->m_pGameContext);
	s_this->m_pGameServerNub->SetMaxPlayers(maxPlayers);

	char address[MAX_ADDRESS_SIZE];
	pCVar = gEnv->pConsole->GetCVar("sv_bind");
	if (pCVar && pCVar->GetString())
	{
		cry_sprintf(address, "%s:%u", pCVar->GetString(), s_this->m_pGameContext->GetServerPort());
	}
	else
	{
		cry_sprintf(address, "0.0.0.0:%u", s_this->m_pGameContext->GetServerPort());
	}

	IGameQuery* pGameQuery = s_this->m_pGameContext;
	if (s_this->m_pGameContext->HasContextFlag(eGSF_NoQueries))
	{
		pGameQuery = NULL;
	}

	if (s_this->m_pNetwork)
	{
		s_this->m_pServerNub = s_this->m_pNetwork->CreateNub(address, s_this->m_pGameServerNub, 0, pGameQuery);
	}

	if (s_this->m_pServerNub == NULL)
	{
		// Failed
		CryLogAlways("Host migration error: unable to create server");
		return IHostMigrationEventListener::Listener_Terminate;
	}

	CryLogAlways("[Host Migration]: CActionGame::OnPromoteToServer() finished");
#ifndef _RELEASE
	m_timeToPromoteToServer = 0.f;
#endif
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	bool done = false;

	switch (state)
	{
	case eRS_Reconnect:
		{
			CryLogAlways("[Host Migration]: CActionGame::OnReconnectClient() started");

			// Initiate client reconnection to the migrated server
			s_this->m_pGameClientNub->SetGameContext(s_this->m_pGameContext);

			// Create a new client nub
			CryFixedStringT<64> address;
			address.Format("%s:0", (hostMigrationInfo.IsNewHost() ? LOCAL_CONNECTION_STRING : "0.0.0.0"));
			s_this->m_pClientNub = s_this->m_pNetwork->CreateNub(address.c_str(), s_this->m_pGameClientNub, NULL, NULL);

			bool succeeded = false;
			if (s_this->m_pClientNub)
			{
				// Attempt to connect our client nub to the newly created server
				address.Format("<session>%08X,%s", hostMigrationInfo.m_session, hostMigrationInfo.m_newServer.c_str());

				CryLogAlways("[Host Migration]: Will use %s as connection address", address.c_str());

				// Migrating players take the name originally assigned to them by the server (e.g. andy(2))
				if (s_this->m_pClientNub->ConnectTo(address.c_str(), s_this->m_pGameContext->GetConnectionString(&hostMigrationInfo.m_migratedPlayerName, false)))
				{
					succeeded = true;
					state = eRS_Reconnecting;
				}
			}

			if (!succeeded)
			{
				state = eRS_Terminated;
			}
		}
		break;

	case eRS_Reconnecting:
		CryLogAlways("[Host Migration]: CActionGame::OnReconnectClient() waiting");

		// Wait for the reconnect to complete
		if (s_this->m_pGameClientNub->GetGameClientChannel() != NULL)
		{
			s_this->m_pGameClientNub->GetGameClientChannel()->SetPlayerIdOnMigration(hostMigrationInfo.m_playerID);
			state = eRS_Reconnected;
		}
		break;

	case eRS_Terminated:
		// Failed
		CryLogAlways("Host migration error: unable to reconnect client");
		return IHostMigrationEventListener::Listener_Terminate;

	case eRS_Reconnected: // Intentional fall-through
		CryLogAlways("[Host Migration]: CActionGame::OnReconnectClient() finished");
	default:
		if (hostMigrationInfo.IsNewHost())
		{
			// Store the server channel so that CNetContext can take ownership of game entities
			hostMigrationInfo.m_pServerChannel = s_this->m_pGameServerNub->GetLocalChannel();
		}
		done = true;
		break;
	}

	if (done)
	{
		return IHostMigrationEventListener::Listener_Done;
	}
	return IHostMigrationEventListener::Listener_Wait;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	CryLogAlways("[Host Migration]: CActionGame::OnFinalise() started");

	if (!gEnv->bServer && s_this->m_pGameServerNub)
	{
		// If this is a demoted server, deferred deletion of the game
		// server nub happens here instead of OnDemoteToClient() to
		// allow channels the chance to disconnect (different thread)
		SAFE_DELETE(s_this->m_pGameServerNub);
	}

	CryLogAlways("[Host Migration]: CActionGame::OnFinalise() finished");
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CActionGame::OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}
/////////////////////////////////////////////////////////////////////////////

void CActionGame::AddGlobalPhysicsCallback(int event, void (* proc)(const EventPhys*, void*), void* userdata)
{
	int idx = (event & (0xff << 8)) != 0;
	if (event & eEPE_OnCollisionLogged || event & eEPE_OnCollisionImmediate)
		m_globalPhysicsCallbacks.collision[idx].insert(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnPostStepLogged || event & eEPE_OnPostStepImmediate)
		m_globalPhysicsCallbacks.postStep[idx].insert(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnStateChangeLogged || event & eEPE_OnStateChangeImmediate)
		m_globalPhysicsCallbacks.stateChange[idx].insert(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnCreateEntityPartLogged || event & eEPE_OnCreateEntityPartImmediate)
		m_globalPhysicsCallbacks.createEntityPart[idx].insert(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnUpdateMeshLogged || event & eEPE_OnUpdateMeshImmediate)
		m_globalPhysicsCallbacks.updateMesh[idx].insert(TGlobalPhysicsCallbackSet::value_type(proc, userdata));
}

void CActionGame::RemoveGlobalPhysicsCallback(int event, void (* proc)(const EventPhys*, void*), void* userdata)
{
	int idx = (event & (0xff << 8)) != 0;
	if (event & eEPE_OnCollisionLogged || event & eEPE_OnCollisionImmediate)
		m_globalPhysicsCallbacks.collision[idx].erase(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnPostStepLogged || event & eEPE_OnPostStepImmediate)
		m_globalPhysicsCallbacks.postStep[idx].erase(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnStateChangeLogged || event & eEPE_OnStateChangeImmediate)
		m_globalPhysicsCallbacks.stateChange[idx].erase(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnCreateEntityPartLogged || event & eEPE_OnCreateEntityPartImmediate)
		m_globalPhysicsCallbacks.createEntityPart[idx].erase(TGlobalPhysicsCallbackSet::value_type(proc, userdata));

	if (event & eEPE_OnUpdateMeshLogged || event & eEPE_OnUpdateMeshImmediate)
		m_globalPhysicsCallbacks.updateMesh[idx].erase(TGlobalPhysicsCallbackSet::value_type(proc, userdata));
}

void CActionGame::EnablePhysicsEvents(bool enable)
{
	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;

	if (!pPhysicalWorld)
		return;

	if (enable)
	{

		pPhysicalWorld->AddEventClient(EventPhysBBoxOverlap::id, OnBBoxOverlap, 1);
		pPhysicalWorld->AddEventClient(EventPhysCollision::id, OnCollisionLogged, 1);
		pPhysicalWorld->AddEventClient(EventPhysPostStep::id, OnPostStepLogged, 1);
		pPhysicalWorld->AddEventClient(EventPhysStateChange::id, OnStateChangeLogged, 1);
		pPhysicalWorld->AddEventClient(EventPhysCreateEntityPart::id, OnCreatePhysicalEntityLogged, 1, 0.1f);
		pPhysicalWorld->AddEventClient(EventPhysUpdateMesh::id, OnUpdateMeshLogged, 1, 0.1f);
		pPhysicalWorld->AddEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysicalEntityPartsLogged, 1, 2.0f);
		pPhysicalWorld->AddEventClient(EventPhysCollision::id, OnCollisionImmediate, 0);
		pPhysicalWorld->AddEventClient(EventPhysPostStep::id, OnPostStepImmediate, 0);
		pPhysicalWorld->AddEventClient(EventPhysStateChange::id, OnStateChangeImmediate, 0);
		pPhysicalWorld->AddEventClient(EventPhysCreateEntityPart::id, OnCreatePhysicalEntityImmediate, 0, 10.0f);
		pPhysicalWorld->AddEventClient(EventPhysUpdateMesh::id, OnUpdateMeshImmediate, 0, 10.0f);
		pPhysicalWorld->AddEventClient(EventPhysEntityDeleted::id, OnPhysEntityDeleted, 0);
	}
	else
	{
		pPhysicalWorld->RemoveEventClient(EventPhysBBoxOverlap::id, OnBBoxOverlap, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, OnCollisionLogged, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, OnPostStepLogged, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysStateChange::id, OnStateChangeLogged, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysCreateEntityPart::id, OnCreatePhysicalEntityLogged, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysUpdateMesh::id, OnUpdateMeshLogged, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysicalEntityPartsLogged, 1);
		pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, OnCollisionImmediate, 0);
		pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, OnPostStepImmediate, 0);
		pPhysicalWorld->RemoveEventClient(EventPhysStateChange::id, OnStateChangeImmediate, 0);
		pPhysicalWorld->RemoveEventClient(EventPhysCreateEntityPart::id, OnCreatePhysicalEntityImmediate, 0);
		pPhysicalWorld->RemoveEventClient(EventPhysUpdateMesh::id, OnUpdateMeshImmediate, 0);
		pPhysicalWorld->RemoveEventClient(EventPhysEntityDeleted::id, OnPhysEntityDeleted, 0);
	}
}

int CActionGame::OnBBoxOverlap(const EventPhys* pEvent)
{
	EventPhysBBoxOverlap* pOverlap = (EventPhysBBoxOverlap*)pEvent;
	IEntity* pEnt = 0;
	pEnt = GetEntity(pOverlap->iForeignData[0], pOverlap->pForeignData[0]);

	if (pOverlap->iForeignData[1] == PHYS_FOREIGN_ID_STATIC)
	{
		pe_status_rope sr;
		IRenderNode* rn = (IRenderNode*)pOverlap->pEntity[1]->GetForeignData(PHYS_FOREIGN_ID_STATIC);

		if (rn != NULL && eERType_Vegetation == rn->GetRenderNodeType())
		{
			bool hit_veg = false;
			int idx = 0;
			IPhysicalEntity* phys = rn->GetBranchPhys(idx);
			while (phys != 0)
			{
				phys->GetStatus(&sr);

				if (sr.nCollDyn > 0)
				{
					hit_veg = true;
					break;
				}
				//CryLogAlways("colldyn: %d collstat: %d", sr.nCollDyn, sr.nCollStat);
				idx++;
				phys = rn->GetBranchPhys(idx);
			}
			if (hit_veg && pEnt)
			{
				bool play_sound = false;
				if (s_this->m_vegStatus.find(pEnt->GetId()) == s_this->m_vegStatus.end())
				{
					play_sound = true;
					s_this->m_vegStatus[pEnt->GetId()] = pEnt->GetWorldPos();
				}
				else
				{
					float distSquared = (pEnt->GetWorldPos() - s_this->m_vegStatus[pEnt->GetId()]).GetLengthSquared();
					if (distSquared > 2.0f * 2.0f)
					{
						play_sound = true;
						s_this->m_vegStatus[pEnt->GetId()] = pEnt->GetWorldPos();
					}
				}
				if (play_sound)
				{
					IMaterialEffects* pMaterialEffects = CCryAction::GetCryAction()->GetIMaterialEffects();
					TMFXEffectId effectId = pMaterialEffects ? pMaterialEffects->GetEffectIdByName("vegetation", PathUtil::GetFileName(rn->GetName())) : InvalidEffectId;
					if (effectId != InvalidEffectId)
					{
						SMFXRunTimeEffectParams params;
						params.pos = rn->GetBBox().GetCenter();
						//params.soundSemantic = eSoundSemantic_Physics_General;

						pe_status_dynamics dyn;

						if (pOverlap->pEntity[0])
						{
							pOverlap->pEntity[0]->GetStatus(&dyn);
							const float speed = min(1.0f, dyn.v.GetLengthSquared() / (10.0f * 10.0f));
							params.AddAudioRtpc("speed", speed);
						}
						pMaterialEffects->ExecuteEffect(effectId, params);
					}
				}
			}
		}
	}
	return 1;
}

int CActionGame::OnCollisionLogged(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.collision[0].begin();
	     it != s_this->m_globalPhysicsCallbacks.collision[0].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysCollision* pCollision = static_cast<const EventPhysCollision*>(pEvent);
	IGameRules::SGameCollision gameCollision;
	memset(&gameCollision, 0, sizeof(IGameRules::SGameCollision));
	gameCollision.pCollision = pCollision;
	if (pCollision->iForeignData[0] == PHYS_FOREIGN_ID_ENTITY)
	{
		//gameCollision.pSrcEntity = gEnv->pEntitySystem->GetEntityFromPhysics(gameCollision.pCollision->pEntity[0]);
		gameCollision.pSrcEntity = (IEntity*)pCollision->pForeignData[0];
		gameCollision.pSrc = GetEntityGameObject(gameCollision.pSrcEntity);
	}
	if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY)
	{
		//gameCollision.pTrgEntity = gEnv->pEntitySystem->GetEntityFromPhysics(gameCollision.pCollision->pEntity[1]);
		gameCollision.pTrgEntity = (IEntity*)pCollision->pForeignData[1];
		gameCollision.pTrg = GetEntityGameObject(gameCollision.pTrgEntity);
	}

	SGameObjectEvent event(eGFE_OnCollision, eGOEF_ToExtensions | eGOEF_ToGameObject | eGOEF_LoggedPhysicsEvent);
	event.ptr = (void*)pCollision;
	if (gameCollision.pSrc && gameCollision.pSrc->WantsPhysicsEvent(eEPE_OnCollisionLogged))
		gameCollision.pSrc->SendEvent(event);
	if (gameCollision.pTrg && gameCollision.pTrg->WantsPhysicsEvent(eEPE_OnCollisionLogged))
		gameCollision.pTrg->SendEvent(event);

	if (gameCollision.pSrc)
	{
		IRenderNode* pNode = NULL;
		if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY)
		{
			IEntity* pTarget = (IEntity*)pCollision->pForeignData[1];
			if (pTarget)
			{
				pNode = pTarget->GetRenderNode();
			}
		}
		else if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_STATIC)
			pNode = (IRenderNode*)pCollision->pForeignData[1];
		/*else
		   if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_FOLIAGE)
		   {
		   CStatObjFoliage *pFolliage = (CStatObjFoliage*)pCollision->pForeignData[1];
		   if (pFolliage)
		    pNode = pFolliage->m_pVegInst;
		   }*/
		if (pNode)
			gEnv->p3DEngine->SelectEntity(pNode);
	}

	IGameRules* pGameRules = s_this->m_pGameContext->GetFramework()->GetIGameRulesSystem()->GetCurrentGameRules();
	if (pGameRules)
	{
		if (!pGameRules->OnCollision(gameCollision))
			return 0;

		pGameRules->OnCollision_NotifyAI(pEvent);
	}

	OnCollisionLogged_Breakable(pEvent);
	OnCollisionLogged_MaterialFX(pEvent);

	return 1;
}

bool CActionGame::ProcessHitpoints(const Vec3& pt, IPhysicalEntity* pent, int partid, ISurfaceType* pMat, int iDamage)
{
	if (m_bLoading)
		return true;
	int i, imin, id;
	Vec3 ptloc;
	SEntityHits* phits;
	pe_status_pos sp;
	std::map<int, SEntityHits*>::iterator iter;
	float curtime = gEnv->pTimer->GetCurrTime();
	sp.partid = partid;
	if (!pent->GetStatus(&sp))
		return false;
	ptloc = (pt - sp.pos) * sp.q;

	id = m_pPhysicalWorld->GetPhysicalEntityId(pent) * 256 + partid;
	if ((iter = m_mapEntHits.find(id)) != m_mapEntHits.end())
		phits = iter->second;
	else
	{
		for (phits = m_pEntHits0; phits->timeUsed + phits->lifeTime > curtime && phits->pnext; phits = phits->pnext)
			;
		if (phits->timeUsed + phits->lifeTime > curtime)
		{
			phits->pnext = new SEntityHits[32];
			for (i = 0; i < 32; i++)
			{
				phits->pnext[i].pHits = &phits->pnext[i].hit0;
				phits->pnext[i].pnHits = &phits->pnext[i].nhits0;
				phits->pnext[i].nHits = 0;
				phits->pnext[i].nHitsAlloc = 1;
				phits->pnext[i].pnext = phits->pnext + i + 1;
				phits->pnext[i].timeUsed = phits->pnext[i].lifeTime = 0;
			}
			phits->pnext[i - 1].pnext = 0;
			phits = phits->pnext;
		}
		phits->nHits = 0;
		phits->hitRadius = 100.0f;
		phits->hitpoints = 1;
		phits->maxdmg = 100;
		phits->nMaxHits = 64;
		phits->lifeTime = 10.0f;
		const ISurfaceType::SPhysicalParams& physParams = pMat->GetPhyscalParams();
		phits->hitRadius = physParams.hit_radius;
		phits->hitpoints = (int)physParams.hit_points;
		phits->maxdmg = (int)physParams.hit_maxdmg;
		phits->lifeTime = physParams.hit_lifetime;
		m_mapEntHits.insert(std::pair<int, SEntityHits*>(id, phits));
	}
	phits->timeUsed = curtime;

	for (i = 1, imin = 0; i < phits->nHits; i++)
		if ((phits->pHits[i] - ptloc).len2() < (phits->pHits[imin] - ptloc).len2())
			imin = i;
	if (phits->nHits == 0 || (phits->pHits[imin] - ptloc).len2() > sqr(phits->hitRadius) && phits->nHits < phits->nMaxHits)
	{
		if (phits->nHitsAlloc == phits->nHits)
		{
			Vec3* pts = phits->pHits;
			memcpy(phits->pHits = new Vec3[phits->nHitsAlloc = phits->nHits + 1], pts, phits->nHits * sizeof(Vec3));
			if (pts != &phits->hit0) delete[] pts;
			int* ns = phits->pnHits;
			memcpy(phits->pnHits = new int[phits->nHitsAlloc], ns, phits->nHits * sizeof(int));
			if (ns != &phits->nhits0) delete[] ns;
		}
		phits->pHits[imin = phits->nHits] = ptloc;
		phits->pnHits[phits->nHits++] = min(phits->maxdmg, iDamage);
	}
	else
	{
		iDamage = min(phits->maxdmg, iDamage);
		phits->pHits[imin] = (phits->pHits[imin] * (float)phits->pnHits[imin] + ptloc * (float)iDamage) / (float)(phits->pnHits[imin] + iDamage);
		phits->pnHits[imin] += iDamage;
	}

	if (phits->pnHits[imin] >= phits->hitpoints)
	{
		memmove(phits->pHits + imin, phits->pHits + imin + 1, (phits->nHits - imin - 1) * sizeof(phits->pHits[0]));
		memmove(phits->pnHits + imin, phits->pnHits + imin + 1, (phits->nHits - imin - 1) * sizeof(phits->pnHits[0]));
		--phits->nHits;
		phits->hitpoints = FtoI(pMat->GetPhyscalParams().hit_points_secondary);
		return true;
	}
	return false;
}

SBreakEvent& CActionGame::RegisterBreakEvent(const EventPhysCollision* pColl, float energy)
{
	if (m_bLoading)
		return m_breakEvents[m_iCurBreakEvent];

	BreakLogAlways("   CActionGame::RegisterBreakEvent() - generating break event for store");
	SBreakEvent be;
	memset(&be, 0, sizeof(be));
	IEntity* pEntity;
	pe_status_pos sp;

	switch (be.itype = pColl->iForeignData[1])
	{
	case PHYS_FOREIGN_ID_ENTITY:
		be.idEnt = (pEntity = (IEntity*)pColl->pForeignData[1])->GetId();
		pColl->pEntity[1]->GetStatus(&sp);
		be.pos = sp.pos;
		be.rot = sp.q;
		be.scale = pEntity->GetScale();
		break;
	case PHYS_FOREIGN_ID_STATIC:
		IRenderNode* pVeg = (IRenderNode*)pColl->pForeignData[1];
		be.eventPos = pColl->pt;
		be.hash = CObjectSelector::CalculateHash(pVeg, true);
		break;
	}

	be.pt = pColl->pt;
	be.n = pColl->n;
	be.vloc[0] = pColl->vloc[0];
	be.vloc[1] = pColl->vloc[1];
	be.mass[0] = pColl->mass[0];
	be.mass[1] = pColl->mass[1];
	be.idmat[0] = pColl->idmat[0];
	be.idmat[1] = pColl->idmat[1];
	be.partid[0] = pColl->partid[0];
	be.partid[1] = pColl->partid[1];
	be.iPrim[0] = pColl->iPrim[0];
	be.iPrim[1] = pColl->iPrim[1];
	be.penetration = pColl->penetration;
	be.energy = energy;
	be.radius = pColl->radius;
	be.time = gEnv->pTimer->GetCurrTime();
	be.iBrokenObjectIndex = -1;

	return StoreBreakEvent(be);
}

/////////////////////////////////
// Store break event for later playback. This should only occur with break events that
//		have been received over the network or generated locally
SBreakEvent& CActionGame::StoreBreakEvent(const SBreakEvent& breakEvent)
{
	BreakLogAlways("   CActionGame::StoreBreakEvent() - logging to m_breakEvents");
	m_breakEvents.push_back(breakEvent);
	m_breakEvents.back().iState = eBES_Processed;
	CCryAction::GetCryAction()->OnBreakEvent(static_cast<uint16>(m_breakEvents.size() - 1));

	return m_breakEvents.back();
}

void CActionGame::PerformPlaneBreak(const EventPhysCollision& epc, SBreakEvent* pRecordedEvent, int flags, CDelayedPlaneBreak* pDelayedTask)
{
	// TODO: improve
	bool actuallyLoading = s_this->m_bLoading;

	class CRestoreLoadingFlag
	{
	public:
		CRestoreLoadingFlag() : m_bLoading(s_this->m_bLoading) {}
		~CRestoreLoadingFlag() { s_this->m_bLoading = m_bLoading; }

	private:
		bool m_bLoading;
	};
	CRestoreLoadingFlag restoreLoadingFlag;

	if (pRecordedEvent)
		s_this->m_bLoading = true; // fake loading here

	pe_params_part pp;
	IStatObj* pStatObj = 0;
	IStatObj* pStatObjHost = 0;
	IStatObj::SSubObject* pSubObj;
	IRenderNode* pBrush;
	IMaterial* pRenderMat;
	IPhysicalEntity* pPhysEnt;
	float r = epc.radius;
	pp.pMatMapping = 0;
	pp.flagsOR = geom_can_modify;
	Matrix34A mtx;
	SProcBrokenObjRec rec;
	rec.itype = epc.pEntity[1] ? epc.pEntity[1]->GetiForeignData() : epc.iForeignData[1];
	rec.mass = -1.0f;
	rec.pBrush = NULL;
	rec.pStatObjOrg = NULL;
	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	ISurfaceType* pMat = pSurfaceTypeManager->GetSurfaceType(epc.idmat[1]);
	if (!pMat)
		return;

	ISurfaceType::SBreakable2DParams* pb2d = pMat->GetBreakable2DParams();
	float timeout = 0.0f;
	const char* killFX = 0;
	if (pb2d)
	{
		timeout = pb2d->destroy_timeout + pb2d->destroy_timeout_spread * cry_random(-1.0f, 1.0f);
		killFX = pb2d->full_fracture_fx;
		if (!pb2d->blast_radius_first && !pb2d->blast_radius)
			r = 0.0f;
		else if (!pb2d->blast_radius_first && (epc.vloc[0].len2() < sqr(100.0f) || epc.mass[0] > 1.0f))
			r = max(r, 100.0f); // make sure larger particles (grenades) shatter 'no-holes' windows
	}

	if (g_glassForceTimeout != 0.f)
	{
		timeout = g_glassForceTimeout + cry_random(0.0f, 1.0f) * g_glassForceTimeoutSpread;
	}

	//IEntity* pEntitySrc = epc.pEntity[0] ? (IEntity*)epc.pEntity[0]->GetForeignData(PHYS_FOREIGN_ID_ENTITY) : 0;
	IEntity* pEntityTrg = epc.pEntity[1] ? (IEntity*)epc.pEntity[1]->GetForeignData(PHYS_FOREIGN_ID_ENTITY) : 0;

	if (pEntityTrg && rec.itype == PHYS_FOREIGN_ID_ENTITY)
	{
		BreakLogAlways("> PHYS_FOREIGN_ID_ENTITY");
		assert(pEntityTrg);
		if ((pStatObj = pEntityTrg->GetStatObj(ENTITY_SLOT_ACTUAL)) &&
		    (pStatObj->GetFlags() & (STATIC_OBJECT_COMPOUND | STATIC_OBJECT_CLONE)) == STATIC_OBJECT_COMPOUND)
		{
			SProcBrokenObjRec rec1;
			pe_params_part ppt;
			rec1.itype = PHYS_FOREIGN_ID_ENTITY;
			rec1.islot = -1;
			rec1.idEnt = pEntityTrg->GetId();
			(rec1.pStatObjOrg = pStatObj)->AddRef();
			pe_status_nparts status_nparts;
			for (rec1.mass = 0, ppt.ipart = epc.pEntity[1]->GetStatus(&status_nparts) - 1; ppt.ipart >= 0; ppt.ipart--)
			{
				MARK_UNUSED ppt.partid;
				if (epc.pEntity[1]->GetParams(&ppt) != 0)
					rec1.mass += ppt.mass;
			}
			s_this->m_brokenObjs.push_back(rec1);
		}
		pStatObj = pEntityTrg->GetStatObj(epc.partid[1]);
		mtx = pEntityTrg->GetSlotWorldTM(epc.partid[1]);
		pRenderMat = (pEntityTrg->GetRenderInterface())->GetRenderMaterial(epc.partid[1]);
		// FIXME, workaround
		if ((pRenderMat && !_stricmp(pRenderMat->GetName(), "default") || !pRenderMat) && pStatObj && pStatObj->GetMaterial())
			pRenderMat = pStatObj->GetMaterial();
		// ~FIXME
		pPhysEnt = pEntityTrg->GetPhysics();
		rec.idEnt = pEntityTrg->GetId();
		rec.islot = epc.partid[1];
	}
	else if (rec.itype == PHYS_FOREIGN_ID_STATIC)
	{
		BreakLogAlways("> PHYS_FOREIGN_ID_STATIC");
		pStatObj = (pBrush = ((IRenderNode*)epc.pForeignData[1]))->GetEntityStatObj(0, &mtx);
		if (pStatObj && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
		{
			if (pSubObj = (pStatObjHost = pStatObj)->GetSubObject(epc.partid[1]))
			{
				pStatObj = pSubObj->pStatObj;
				if (!pSubObj->bIdentityMatrix)
					mtx = mtx * pSubObj->tm;
			}
			else
				pStatObj = 0;
		}
		pPhysEnt = ((IRenderNode*)epc.pForeignData[1])->GetPhysics();
		pRenderMat = pBrush->GetMaterial();
		rec.pBrush = pBrush;
		rec.islot = 0;
	}

	int iBrokenObjectIndex = -1;
	bool bEntityMustBreakGlass = false;
	bool bIsClientBreak = false;  // Did the client break this bit of glass

	if (pStatObj)
	{
		if (!pDelayedTask && ((!pRecordedEvent || pRecordedEvent->iState == eBES_Generated)) && !(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
		{
			if (epc.pEntity[0] && epc.pEntity[0]->GetType() == PE_LIVING)
			{
				IEntity* pEntitySrc = (IEntity*)epc.pEntity[0]->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
				IActor* pActor = pEntitySrc ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pEntitySrc->GetId()) : NULL;
				if (!pActor || !pActor->CanBreakGlass())
				{
					return;
				}
				else
				{
					bEntityMustBreakGlass = pActor->MustBreakGlass();
				}
			}
		}
		SBreakEvent& be = pRecordedEvent ? *pRecordedEvent :
		                  (pDelayedTask ? s_this->m_breakEvents[pDelayedTask->m_iBreakEvent] : s_this->RegisterBreakEvent(&epc, 0));

		// Is the break event in the event list ?
		const int breakEventSize = s_this->m_breakEvents.size();
		const SBreakEvent* breaks = &(*(s_this->m_breakEvents.begin()));
		bool bIsStoredEvent = (breakEventSize > 0) && (&be >= breaks) && (&be < (breaks + breakEventSize));

		if (!pDelayedTask)
		{
			//If this is a locally generated break event, or a recorded one that has arrived via
			//	the network, we want to do full processing on it, and record it for later playback
			if (!pRecordedEvent || pRecordedEvent->iState == eBES_Generated)
			{
				if (!(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
				{
					BreakLogAlways("> Marked as original brush, not generated");

					//================================================
					// We now allow clients to break the glass
					// The server will also only send the first break
					// Secondary breaks will be sent for client-joins
					//================================================
					if (pRecordedEvent == NULL && !s_this->IsServer() && (flags & ePPB_PlaybackEvent) == 0)
					{
						bIsClientBreak = true;
					}

					//Some breaks resulting in new StatObj do not result in the STATIC_OBJECT_GENERATED
					//	flag being set, so we want to check the existing broken objects list for a brush match.
					//	We're not checking for a statobj match, because new statobjs can be generated that will
					//	not match with the original, and we want to record the index of the original for
					//	later reversion in the kill cam
					for (int i = 0; i < s_this->m_brokenObjs.size(); i++)
					{
						//assert(s_this->m_brokenObjs[i].pBrush);
						if (s_this->m_brokenObjs[i].pBrush == rec.pBrush)
						{
							BreakLogAlways(">> Found brush match at index %d", i);
							iBrokenObjectIndex = i;
							break;
						}
					}

					//If there  is not an already-existing record of the object being broken, generate a new one
					if (iBrokenObjectIndex == -1)
					{
						(rec.pStatObjOrg = pStatObjHost ? pStatObjHost : pStatObj)->AddRef();
						s_this->m_brokenObjs.push_back(rec);
						iBrokenObjectIndex = s_this->m_brokenObjs.size() - 1;
						BreakLogAlways("  Adding new statobj break at index %d, statobj 0x%p, original statobj 0x%p, brush 0x%p, original is %shost", iBrokenObjectIndex, pStatObj, rec.pStatObjOrg, rec.pBrush, pStatObjHost ? "" : "not ");
					}
				}
				else
				{
					//The object is a generated one, and should therefore already be in the broken objects list
					//	find the existing entry in the broken objects list and use the index from that
					if (rec.itype == PHYS_FOREIGN_ID_STATIC)
					{
						BreakLogAlways("> Searching m_brokenObjs for match, StatObj marked as generated");
						for (int i = 0; i < s_this->m_brokenObjs.size(); i++)
						{
							assert(s_this->m_brokenObjs[i].pBrush);
							BreakLogAlways(">>  Index: %0d, pBrush 0x%p", i, s_this->m_brokenObjs[i].pBrush);
							if (s_this->m_brokenObjs[i].pBrush == rec.pBrush)
							{
								BreakLogAlways(">>>  Found existing pBrush break at index %d, statobj 0x%p  <<<", i, pStatObj);
								iBrokenObjectIndex = i;
								break;
							}
						}
					}

					//TODO: This code to be used when glass breakage on entities is working in killcam

					//        if(iBrokenObjectIndex == -1)
					//        {
					//          BreakLogAlways(">>>  Failed to find existing statobj break, looking for pBrush: 0x%p  <<<\n>>>  Now searching on EntityID (NOT PTR) 0x%08X", rec.pBrush, rec.idEnt);
					//          for(int i = 0; i < s_this->m_brokenObjs.size(); i++)
					//          {
					//            BreakLogAlways(">>  Index: %0d, entID 0x%08X", i, s_this->m_brokenObjs[i].idEnt );
					//            if(s_this->m_brokenObjs[i].idEnt == rec.idEnt)
					//            {
					//              BreakLogAlways(">>>  Found existing ENTITY break at index %d <<<", i);
					//              iBrokenObjectIndex = i;
					//              break;
					//            }
					//          }
					//
					//          if(iBrokenObjectIndex == -1)
					//          {
					//            BreakLogAlways(">>>  FAILED TO FIND ENTITY OR BRUSH  <<<");
					//          }
					//        }
				}
			}

			if (iBrokenObjectIndex != -1)
			{
				if (!pRecordedEvent || pRecordedEvent->iState == eBES_Generated)
				{
					//Only new events and recorded events from the network will enter this
					be.iBrokenObjectIndex = iBrokenObjectIndex;
					BreakLogAlways("Storing broken object index as: %d, original statobj 0x%p, pBrush 0x%p", iBrokenObjectIndex, rec.pStatObjOrg, rec.pBrush);
				}

				if (be.iState == eBES_Generated && !bIsStoredEvent)
				{
					// This is not in the recorded events, we still want to store it for Kill Cam
					CryLog("Should not happen!");
					be = s_this->StoreBreakEvent(be);
					bIsStoredEvent = true;
				}
			}
		} // if (!pDelayedTask)

		int bIsExplosion = iszero(epc.idmat[0] + 1);  // idmat==-1 is an explosion
		uint32 processFlags = 0;
		if (pDelayedTask)
		{
			processFlags = pDelayedTask->m_islandIn.processFlags;
		}
		else
		{
			processFlags |= (pPhysEnt ? pPhysEnt->GetType() == PE_STATIC : ePlaneBreak_Static);
			processFlags |= (ePlaneBreak_AutoSmash) * (g_glassAutoShatter | (bIsExplosion & g_glassAutoShatterOnExplosions));
			if (s_this->m_bLoading && (flags & ePPB_EnableParticleEffects) == 0)
				processFlags |= (ePlaneBreak_NoFractureEffect);
			if ((processFlags & ePlaneBreak_AutoSmash) == 0)
			{
				s_this->m_throttling.m_brokenTreeCounter += g_breakageTreeIncGlass;
				s_this->m_throttling.m_numGlassEvents++;
			}
			if (g_glassMaxPanesToBreakPerFrame && s_this->m_throttling.m_numGlassEvents > g_glassMaxPanesToBreakPerFrame)
			{
				// We are over budget, force autoshattering
				processFlags |= ePlaneBreak_AutoSmash;
				ThrottleLogAlways("THROTTLING: autosmashing glass, too many events");
			}
			if (flags & ePPB_PlaybackEvent)
			{
				processFlags |= ePlaneBreak_PlaybackEvent;
			}
		}

		IStatObj* pStatObjAux = NULL;
		IStatObj* pStatObjNew = NULL;
		CDelayedPlaneBreak dpb;

		if (epc.iPrim[1] >= 0 &&
		    bIsStoredEvent &&
		    pDelayedTask == NULL &&
		    (!actuallyLoading || (flags & ePPB_PlaybackEvent) || (processFlags & ePlaneBreak_PlaybackEvent)))
		{
			pDelayedTask = &dpb;
		}

		/*	- ProcessImpact -
		 *
		 *  pStatObjAux==NULL, then an island was given to ProcessImpact, and pStatObjNew is the new broken glass
		 *
		 *  pStatObjAux!=NULL, then an island extraction took place
		 *    pStatObj stays the same
		 *    pStatObjNew = remainder
		 *		pStatObjAux = broken glass
		 *    if there is no host, one is created with one slot
		 *		pStatObjNew is placed into the original sub slot (or first)
		 *		pStatObjAux is appended to a new slot at the end
		 */

		// Fill breakable impact struct
		IBreakableManager* pBreakableManager = gEnv->pEntitySystem->GetBreakableManager();
		SProcessImpactIn in;
		{
			in.pthit = epc.pt;
			in.hitvel = epc.vloc[0];
			in.hitnorm = epc.n;
			in.hitmass = epc.mass[0];
			in.hitradius = r;
			in.itriHit = epc.iPrim[1];
			in.pStatObj = pStatObj;
			in.pStatObjAux = pStatObjAux;
			in.mtx = mtx;
			in.pMat = pMat;
			in.pRenderMat = pRenderMat;
			in.processFlags = processFlags;
			in.eventSeed = be.seed;
			in.bLoading = s_this->m_bLoading;
			in.glassAutoShatterMinArea = CActionGame::g_glassAutoShatterMinArea;
			in.addChunkFunc = &CActionGame::AddBroken2DChunkId;
			in.bVerify = false;
		}

		SProcessImpactOut out;
		EProcessImpactResult result = eProcessImpact_BadGeometry;

		// Try to break with the new system if:
		//	a) Not auto-smashing; no difference in systems in this case
		//	b) First break; don't want to interfere with partially-broken mesh
		if (g_glassSystemEnable
		    && !(processFlags &= ePlaneBreak_AutoSmash)
		    && !(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
		{
			IBreakableGlassSystem* pGlassSys = CCryAction::GetCryAction()->GetIBreakableGlassSystem();

			if (pGlassSys && pGlassSys->BreakGlassObject(epc, bEntityMustBreakGlass))
			{
				result = eProcessImpact_Done;
			}
		}

		// Clear pointers if successful, else fall back to old system
		if (result == eProcessImpact_Done)
		{
			pStatObjNew = pStatObjAux = NULL;
		}
		else
		{
			if (pDelayedTask)
			{
				in.bDelay = true;
				in.pIslandOut = (pDelayedTask->m_status == CDelayedPlaneBreak::eStatus_DONE) ? &pDelayedTask->m_islandOut : 0;
			}
			else
			{
				in.bDelay = false;
				in.pIslandOut = 0;
			}

			result = pBreakableManager->ProcessPlaneImpact(in, out);

			be.seed = out.eventSeed;
			pStatObjNew = out.pStatObjNew;
			pStatObjAux = out.pStatObjAux;
		}

		if (result == eProcessImpact_Delayed ||
		    result == eProcessImpact_DelayedMeshOnly)
		{
			assert(pDelayedTask != 0);
			pDelayedTask->m_islandIn = out.islandIn;
			pDelayedTask->m_bMeshPrepOnly = (result == eProcessImpact_DelayedMeshOnly);
			pDelayedTask->m_status = CDelayedPlaneBreak::eStatus_STARTED;

			for (int i = s_this->m_pendingPlaneBreaks.size() - 1; i >= 0; i--)
			{
				if (s_this->m_pendingPlaneBreaks[i].m_status != CDelayedPlaneBreak::eStatus_NONE)
				{
					if ((s_this->m_pendingPlaneBreaks[i].m_islandIn.pStatObj == dpb.m_islandIn.pStatObj) &&
					    (s_this->m_pendingPlaneBreaks[i].m_islandIn.itriSeed == dpb.m_islandIn.itriSeed))
					{
						// Already doing a delayed break on the same pane of glass, ignore this request
						return;
					}
				}
				else
				{
					CDelayedPlaneBreak& dpbTrg = s_this->m_pendingPlaneBreaks[i];
					dpbTrg = dpb;
					dpbTrg.m_iBreakEvent = (int)(&be - &s_this->m_breakEvents[0]);
					dpbTrg.m_epc = epc;
					dpbTrg.m_islandIn.pStatObj->AddRef();
					dpbTrg.m_idx = i;
					dpbTrg.m_count = s_this->m_pendingPlaneBreaks.size();
					gEnv->p3DEngine->GetDeferredPhysicsEventManager()->DispatchDeferredEvent(&dpbTrg);
					return;
				}
			}
			if (gEnv->bMultiplayer)
			{
				// No more tasks, do it immediately
				in.bDelay = false;
				pBreakableManager->ProcessPlaneImpact(in, out);

				be.seed = out.eventSeed;
				pStatObjNew = out.pStatObjNew;
				pStatObjAux = out.pStatObjAux;
			}
			else
			{
				return;
			}
		}

		//CryLogAlways("CActionGame::PerformPlaneBreak() - ProcessImpact returned pStatObjNew: 0x%p\n  pStatObjAux: 0x%p", pStatObjNew, pStatObjAux);

		be.bFirstBreak = iszero(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED);  // keep note whether this is a primary or secondary break

		EventPhysMono mono;
		mono.pEntity = epc.pEntity[1];
		mono.pForeignData = epc.pForeignData[1];
		mono.iForeignData = epc.iForeignData[1];
		if (gEnv->bServer || bIsClientBreak)
			s_this->m_pGameContext->OnBrokeSomething(be, &mono, true);

		// Was the break successful?
		if (pStatObjAux == NULL && (pStatObj->GetFlags() & (STATIC_OBJECT_GENERATED | STATIC_OBJECT_CLONE)) == 0 && pStatObjNew == pStatObj)
			return;

		if (pStatObjAux || pStatObjNew && (!(pStatObjNew->GetFlags() & STATIC_OBJECT_GENERATED) ||
		                                   pStatObjNew->GetPhysGeom() && pStatObjNew->GetPhysGeom()->pMatMapping))
			MARK_UNUSED pp.pMatMapping;
		if (!pStatObjAux)
		{
			if (!pStatObjNew)
			{
				s_this->RegisterBrokenMesh(epc.pEntity[1], 0, epc.partid[1], 0, 0, timeout, killFX);
			}
			else if (!(pStatObjNew->GetFlags() & STATIC_OBJECT_COMPOUND))
			{
				phys_geometry* physGeom = pStatObjNew->GetPhysGeom();
				IGeometry* pGeom = physGeom ? physGeom->pGeom : NULL;
				s_this->RegisterBrokenMesh(epc.pEntity[1], pGeom, epc.partid[1], pStatObjNew, 0, timeout, killFX);
			}
		}
		if (rec.itype == PHYS_FOREIGN_ID_STATIC && pStatObjAux && !pStatObjHost)
		{
			pStatObjHost = gEnv->p3DEngine->CreateStatObj();
			pStatObjHost->SetSubObjectCount(1);
			pStatObjHost->SetFlags(pStatObjHost->GetFlags() | STATIC_OBJECT_CLONE | STATIC_OBJECT_COMPOUND);
			if (pStatObj)
				pStatObjHost->SetFilePath(pStatObj->GetFilePath());
			pStatObjHost->FreeIndexedMesh();
			pStatObjHost->SetMaterial(pStatObj->GetMaterial());
			goto ForceObjUpdate;
		}
		if (pStatObjNew != pStatObj)
		{
			if (rec.itype == PHYS_FOREIGN_ID_ENTITY)
			{
				assert(pEntityTrg);
				pp.partid = pEntityTrg->SetStatObj(pStatObjNew, epc.partid[1], true);
				if (pEntityTrg->GetPhysics())
					pEntityTrg->GetPhysics()->SetParams(&pp);
			}
			else if (rec.itype == PHYS_FOREIGN_ID_STATIC)
			{
ForceObjUpdate:
				if (pStatObjHost)
				{
					if (!(pStatObjHost->GetFlags() & STATIC_OBJECT_CLONE))
						pStatObjHost = pStatObjHost->Clone(false, false, false);
					pp.pPhysGeom = pStatObjNew ? pStatObjNew->GetPhysGeom() : 0;
					if (pSubObj = pStatObjHost->GetSubObject(epc.partid[1]))
					{
						if (pSubObj->pStatObj)
							pSubObj->pStatObj->Release();
						if (pSubObj->pStatObj = pStatObjNew)
							pSubObj->pStatObj->AddRef();
						pStatObjHost->Invalidate(false);
					}
					pStatObjNew = pStatObjHost;
				}
				pBrush->SetEntityStatObj(pStatObjNew);
				if (!pStatObjHost)
					pBrush->Physicalize(true);
				pp.partid = epc.partid[1];
				if (pBrush->GetPhysics())
					if (pp.pPhysGeom)
						pBrush->GetPhysics()->SetParams(&pp);
					else
						pBrush->GetPhysics()->RemoveGeometry(pp.partid);
			}

			if (pEntityTrg && pStatObjNew)
			{
				if (IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject(pEntityTrg->GetId()))
				{
					SGameObjectEvent evt(eGFE_OnBreakable2d, eGOEF_ToExtensions);
					evt.ptr = (void*)&epc;
					evt.param = pStatObjNew;
					pGameObject->SendEvent(evt);
				}
			}
		}
		if (pStatObjAux)
		{
			if (rec.itype == PHYS_FOREIGN_ID_ENTITY)
			{
				assert(pEntityTrg);
				pEntityTrg->SetStatObj(pStatObjAux, -1, true);
				SProcBrokenObjRec rec2;
				rec2.itype = PHYS_FOREIGN_ID_ENTITY;
				rec2.islot = -1;
				rec2.idEnt = pEntityTrg->GetId();
				rec2.pStatObjOrg = 0;
				s_this->m_brokenObjs.push_back(rec2);
				const_cast<EventPhysCollision&>(epc).partid[1] =
				  ((pStatObj = pEntityTrg->GetStatObj(ENTITY_SLOT_ACTUAL)) && (pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND) ?
				   pStatObj->GetSubObjectCount() : pEntityTrg->GetSlotCount()) - 1;
			}
			else if (rec.itype == PHYS_FOREIGN_ID_STATIC)
			{
				PREFAST_ASSUME(pStatObjHost);
				pp.partid = pStatObjHost->GetSubObjectCount();
				pStatObjHost->SetSubObjectCount(pp.partid + 1);
				if (pp.partid > 0)
				{
					pSubObj = pStatObjHost->GetSubObject(pp.partid - 1);
					if (pSubObj->nType != STATIC_SUB_OBJECT_MESH)
					{
						for (--pp.partid; pp.partid > 0 && (pSubObj = pStatObjHost->GetSubObject(pp.partid - 1)) && pSubObj->nType != STATIC_SUB_OBJECT_MESH; pp.partid--)
							;
						pStatObjHost->CopySubObject(pStatObjHost->GetSubObjectCount() - 1, pStatObjHost, pp.partid);
						pSubObj = pStatObjHost->GetSubObject(pp.partid);
					}
					else
						pSubObj = pStatObjHost->GetSubObject(pStatObjHost->GetSubObjectCount() - 1);
				}
				else
					pSubObj = pStatObjHost->GetSubObject(pp.partid);
				pSubObj->nType = STATIC_SUB_OBJECT_MESH;
				pSubObj->name = "broken";
				pSubObj->properties = "mass 0";
				pSubObj->bIdentityMatrix = 1;
				pSubObj->localTM.SetIdentity();
				(pSubObj->pStatObj = pStatObjAux)->AddRef();
				pStatObjHost->Invalidate(false);
				pe_geomparams gp;
				gp.mass = 0;

				IPhysicalEntity* pPhysicalEntity = pBrush->GetPhysics();
				if (pPhysicalEntity)
				{
					pPhysicalEntity->AddGeometry(pStatObjAux->GetPhysGeom(), &gp, pp.partid);
				}
				const_cast<EventPhysCollision&>(epc).partid[1] = pp.partid;
			}
			s_this->RegisterBrokenMesh(epc.pEntity[1], pStatObjAux->GetPhysGeom()->pGeom, epc.partid[1], pStatObjAux, 0, timeout, killFX);
			//if (pMat->GetPhyscalParams().hit_radius<=0.0f)
			s_this->m_mapEntHits.erase(gEnv->pPhysicalWorld->GetPhysicalEntityId(epc.pEntity[1]) * 256 + epc.partid[1]);
		}

		if (g_glassNoDecals == 0 && !bIsExplosion && pb2d && pb2d->crack_decal_scale && r > 0)
		{
			assert(pEntityTrg || rec.itype != PHYS_FOREIGN_ID_ENTITY);
			CryEngineDecalInfo dcl;
			dcl.ownerInfo.pRenderNode = rec.itype == PHYS_FOREIGN_ID_ENTITY ?
			                            (pEntityTrg->GetRenderInterface())->GetRenderNode() : pBrush;
			dcl.ownerInfo.nRenderNodeSlotId = 0;
			dcl.ownerInfo.nRenderNodeSlotSubObjectId = EntityPhysicsUtils::GetSlotIdx(epc.partid[1], 1);
			dcl.vPos = epc.pt;
			dcl.vNormal = epc.n;
			dcl.vHitDirection = epc.n; // epc.vloc[0].normalized();
			dcl.fLifeTime = 1e10f;
			dcl.fSize = r * pb2d->crack_decal_scale;
			dcl.fAngle = cry_random(0.0f, 2.0f * gf_PI);
			dcl.bSkipOverlappingTest = true;
			dcl.bForceSingleOwner = true;
			dcl.bDeferred = false;
			cry_strcpy(dcl.szMaterialName, pb2d->crack_decal_mtl);
			gEnv->p3DEngine->CreateDecal(dcl);
		}
	}
}

namespace
{
ILINE bool CheckCarParamBreakable(const EventPhysCollision* pCEvent)
{
	if (pCEvent->pEntity[0])
	{
		if (pCEvent->pEntity[0]->GetType() == PE_LIVING)
			return true;
		if (pCEvent->pEntity[0]->GetType() == PE_WHEELEDVEHICLE)
		{
			pe_status_wheel sw;
			if ((sw.partid = pCEvent->partid[0]) >= 0 && pCEvent->pEntity[0]->GetStatus(&sw))
			{
				pe_params_car pcar;
				if (pCEvent->pEntity[0]->GetParams(&pcar) && pcar.nWheels <= 4)
					return true;
			}
		}
	}
	return false;
}
}

void CActionGame::OnCollisionLogged_Breakable(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const EventPhysCollision* pCEvent = (const EventPhysCollision*) pEvent;
	IEntity* pEntitySrc = pCEvent->pEntity[0] ? (IEntity*)pCEvent->pEntity[0]->GetForeignData(PHYS_FOREIGN_ID_ENTITY) : 0;
	IEntity* pEntityTrg = pCEvent->pEntity[1] ? (IEntity*)pCEvent->pEntity[1]->GetForeignData(PHYS_FOREIGN_ID_ENTITY) : 0;

	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	ISurfaceType* pMat = pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1]), * pMat0;
	SmartScriptTable props;
	float energy, hitenergy;

	Vec3 vloc0 = pCEvent->vloc[0];
	Vec3 vloc1 = pCEvent->vloc[1];

	float mass0 = pCEvent->mass[0];

	bool backface = (pCEvent->n.Dot(vloc0) >= 0.0f);

	if (pMat)
	{
		if (Get()->AllowProceduralBreaking(ePBT_Glass) && pMat->GetBreakability() == 1 &&
		    (mass0 > 10.0f || pCEvent->pEntity[0] && pCEvent->pEntity[0]->GetType() == PE_ARTICULATED || pCEvent->radius > 0.1f ||
		     (vloc0 * pCEvent->n < 0 &&
		      (pCEvent->idmat[0] < 0 ||
		       (energy = pMat->GetBreakEnergy()) > 0 &&
		       (hitenergy = max(fabs_tpl(vloc0.x) + fabs_tpl(vloc0.y) + fabs_tpl(vloc0.z),
		                        vloc0.len2()) * mass0) >= energy &&
		       (!(pMat->GetHitpoints()) ||
		        s_this->ProcessHitpoints(pCEvent->pt, pCEvent->pEntity[1], pCEvent->partid[1], pMat, FtoI(min(1E6f, hitenergy / energy))))))))
		{
			PerformPlaneBreak(*pCEvent, NULL, 0, 0);
		}
		else if (s_this->IsServer() &&
		         (pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY || pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_STATIC) &&
		         Get()->AllowProceduralBreaking(ePBT_Normal) && pMat->GetBreakability() == 2 &&
		         pCEvent->idmat[0] != pCEvent->idmat[1] && pCEvent->idmat[0] != -2 && (energy = pMat->GetBreakEnergy()) > 0 &&
		         !CheckCarParamBreakable(pCEvent) &&
		         (hitenergy = max(fabs_tpl(vloc0.x) + fabs_tpl(vloc0.y) + fabs_tpl(vloc0.z),
		                          vloc0.len2()) * mass0) >= energy &&
		         (!(pMat->GetHitpoints()) ||
		          s_this->ProcessHitpoints(pCEvent->pt, pCEvent->pEntity[1], pCEvent->partid[1], pMat, FtoI(min(1E6f, hitenergy / energy)))))
		{
			if (!(pMat0 = pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])) || !(pMat0->GetFlags() & SURFACE_TYPE_NO_COLLIDE))
			{
				// moved to create entity
				bool bBreak = false;
				pe_status_pos sp;
				pe_params_structural_joint psj;
				primitives::box bbox;
				Vec3 ptloc;
				int iaxis;
				sp.partid = pCEvent->partid[1];
				psj.idx = 0;
				PREFAST_ASSUME(pCEvent->pEntity[1]);
				if (pCEvent->pEntity[1]->GetStatus(&sp) && (!s_this->m_bLoading || !pCEvent->pEntity[1]->GetParams(&psj)))
				{
					if (sp.scale > 10.0f)
						bBreak = false;
					else if (pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_STATIC && pCEvent->pt.z - sp.pos.z < (sp.BBox[0].z + sp.BBox[1].z) * 0.5f)
						bBreak = true; // for vegetation objects, always allow breaking when close enough to the ground
					else
					{
						// check if the hit point is far enough from the geometry boundary; don't break otherwise
						sp.pGeom->GetBBox(&bbox);
						ptloc = bbox.Basis * ((pCEvent->pt - sp.pos) * sp.q * (sp.scale == 1.0f ? 1.0f : 1.0f / sp.scale) - bbox.center);
						iaxis = idxmax3((float*)&bbox.size);
						bBreak = fabs_tpl(ptloc[iaxis]) < bbox.size[iaxis] - max(bbox.size[iaxis] * 0.15f, 0.5f);
					}
					IStatObj* pStatObj = 0;
					// TODO: Improve. On consoles disable procedural breaking by objects
					if (g_no_breaking_by_objects && pCEvent->iForeignData[0] && pCEvent->pEntity[0] && pCEvent->pEntity[0]->GetType() != PE_PARTICLE)
						bBreak = false;
					if (g_no_secondary_breaking &&
					    pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY && (pStatObj = ((IEntity*)pCEvent->pForeignData[1])->GetStatObj(pCEvent->partid[1])) &&
					    pStatObj->GetFlags() & STATIC_OBJECT_GENERATED)
						bBreak = false;
					// filter for multiplayer modes
					//if (!s_this->m_pGameContext->HasContextFlag(eGSF_LocalOnly) && !s_this->m_pGameContext->HasContextFlag(eGSF_ImmersiveMultiplayer))
					//	bBreak = false;

					// Tree breakage throttling
					if ((pCEvent->pEntity[0] && pCEvent->pEntity[0]->GetType() != PE_WHEELEDVEHICLE) &&
					    (s_this->m_throttling.m_brokenTreeCounter > g_breakageTreeMax))
					{
						ThrottleLogAlways("THROTTLING: DeformPhysicalEntity denied");
						bBreak = false;
					}

					if (bBreak)
					{
						const ISurfaceType::SPhysicalParams& params = pMat->GetPhyscalParams();
						energy = params.hole_size;

						energy *= max(1.0f, sp.scale);
						int flags = s_this->m_bLoading ? 0 : 2;
						if (pCEvent->idmat[0] < 0 && params.hole_size_explosion > 0)
							energy = params.hole_size_explosion;
						else if (pCEvent->idmat[0] < 0 && sp.pGeom->GetVolume() * cube(sp.scale) < 0.5f || mass0 >= 1500.0f)
							energy = max(energy, min(energy * 4, 1.5f));//, flags=2; // for explosions
						if (mass0 >= 1500.0f)
							flags |= (geom_colltype_vehicle | geom_colltype6) << 16;

						SBreakEvent& be = s_this->RegisterBreakEvent(pCEvent, energy);
						s_this->m_throttling.m_brokenTreeCounter += g_breakageTreeInc;

						if (sp.pGeom->GetForeignData(DATA_MESHUPDATE) || !s_this->ReuseBrokenTrees(pCEvent, energy, flags))
						{
							LogDeformPhysicalEntity("SERVER", pCEvent->pEntity[1], pCEvent->pt, -pCEvent->n, energy);
							if (s_this->m_pPhysicalWorld->DeformPhysicalEntity(pCEvent->pEntity[1], pCEvent->pt, -pCEvent->n, energy, flags))
							{
								if (g_tree_cut_reuse_dist > 0 && !gEnv->bMultiplayer)
									s_this->RemoveEntFromBreakageReuse(pCEvent->pEntity[1], 0);
								EventPhysMono mono;
								mono.pEntity = pCEvent->pEntity[1];
								mono.pForeignData = pCEvent->pForeignData[1];
								mono.iForeignData = pCEvent->iForeignData[1];
								s_this->m_pGameContext->OnBrokeSomething(be, &mono, false);
							}
						}
					}
				}
			}
		}
	}
}

void CActionGame::OnCollisionLogged_MaterialFX(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const EventPhysCollision* pCEvent = (const EventPhysCollision*) pEvent;
	IMaterialEffects* pMaterialEffects = CCryAction::GetCryAction()->GetIMaterialEffects();
	if ((pMaterialEffects == nullptr) || (pCEvent->idmat[1] == s_waterMaterialId) &&
	    (pCEvent->pEntity[1] == gEnv->pPhysicalWorld->AddGlobalArea() && gEnv->p3DEngine->GetVisAreaFromPos(pCEvent->pt)))
		return;

	Vec3 vloc0 = pCEvent->vloc[0];
	Vec3 vloc1 = pCEvent->vloc[1];

	float mass0 = pCEvent->mass[0];

	bool backface = (pCEvent->n.Dot(vloc0) >= 0.0f);

	// track contacts info for physics sounds generation
	Vec3 vrel, r;
	float velImpact, velSlide2, velRoll2;
	int iop, id, i;
	SEntityCollHist* pech = 0;
	std::map<int, SEntityCollHist*>::iterator iter;

	iop = inrange(pCEvent->mass[1], 0.0f, mass0);
	id = s_this->m_pPhysicalWorld->GetPhysicalEntityId(pCEvent->pEntity[iop]);
	if ((iter = s_this->m_mapECH.find(id)) != s_this->m_mapECH.end())
		pech = iter->second;
	else if (s_this->m_pFreeCHSlot0->pnext != s_this->m_pFreeCHSlot0)
	{
		pech = s_this->m_pFreeCHSlot0->pnext;
		s_this->m_pFreeCHSlot0->pnext = pech->pnext;
		pech->pnext = 0;
		pech->timeRolling = pech->timeNotRolling = pech->rollTimeout = pech->slideTimeout = 0;
		pech->velImpact = pech->velSlide2 = pech->velRoll2 = 0;
		pech->imatImpact[0] = pech->imatImpact[1] = pech->imatSlide[0] = pech->imatSlide[1] = pech->imatRoll[0] = pech->imatRoll[1] = 0;
		pech->mass = 0;
		s_this->m_mapECH.insert(std::pair<int, SEntityCollHist*>(id, pech));
	}

	pe_status_dynamics sd;
	if (pech && pCEvent->pEntity[iop]->GetStatus(&sd))
	{
		vrel = pCEvent->vloc[iop ^ 1] - pCEvent->vloc[iop];
		r = pCEvent->pt - sd.centerOfMass;
		if (sd.w.len2() > 0.01f)
			r -= sd.w * ((r * sd.w) / sd.w.len2());
		velImpact = fabs_tpl(vrel * pCEvent->n);
		velSlide2 = (vrel - pCEvent->n * velImpact).len2();
		velRoll2 = (sd.w ^ r).len2();
		pech->mass = pCEvent->mass[iop];

		i = isneg(pech->velImpact - velImpact);
		pech->imatImpact[0] += pCEvent->idmat[iop] - pech->imatImpact[0] & - i;
		pech->imatImpact[1] += pCEvent->idmat[iop ^ 1] - pech->imatImpact[1] & - i;
		pech->velImpact = max(pech->velImpact, velImpact);

		i = isneg(pech->velSlide2 - velSlide2);
		pech->imatSlide[0] += pCEvent->idmat[iop] - pech->imatSlide[0] & - i;
		pech->imatSlide[1] += pCEvent->idmat[iop ^ 1] - pech->imatSlide[1] & - i;
		pech->velSlide2 = max(pech->velSlide2, velSlide2);

		i = isneg(max(pech->velRoll2 - velRoll2, r.len2() * sqr(0.97f) - sqr(r * pCEvent->n)));
		pech->imatRoll[0] += pCEvent->idmat[iop] - pech->imatRoll[0] & - i;
		pech->imatSlide[1] += pCEvent->idmat[iop ^ 1] - pech->imatRoll[1] & - i;
		pech->velRoll2 += (velRoll2 - pech->velRoll2) * i;
	}
	// --- Begin Material Effects Code ---
	// Relative velocity, adjusted to be between 0 and 1 for sound effect parameters.
	const int debug = CMaterialEffectsCVars::Get().mfx_Debug & 0x1;

	float impactVelSquared = (vloc0 - vloc1).GetLengthSquared();

	// Anything faster than 15 m/s is fast enough to consider maximum speed
	float adjustedRelativeVelocity = (float)min(1.0f, impactVelSquared * (1.0f / sqr(15.0f)));

	// Relative mass, also adjusted to fit into sound effect parameters.
	// 100.0 is very heavy, the top end for the mass parameter.
	float adjustedRelativeMass = (float)min(1.0f, fabsf(mass0 - pCEvent->mass[1]) * 0.01f);

	const float particleImpactThresh = CMaterialEffectsCVars::Get().mfx_ParticleImpactThresh;
	float partImpThresh = particleImpactThresh;

	Vec3 vdir0 = vloc0.normalized();
	float testSpeed = (vloc0 * vdir0.Dot(pCEvent->n)).GetLengthSquared();

	// prevent slow objects from making too many collision events by only considering the velocity towards
	//  the surface (prevents sliding creating tons of effects)
	if (impactVelSquared < sqr(25.0f) && testSpeed < (partImpThresh * partImpThresh))
	{
		impactVelSquared = 0.0f;
	}

	// velocity vector
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pCEvent->pt, ColorB(0,0,255,255), pCEvent->pt + pCEvent->vloc[0], ColorB(0,0,255,255));
	// surface normal
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pCEvent->pt, ColorB(255,0,0,255), pCEvent->pt + (pCEvent->n * 5.0f), ColorB(255,0,0,255));
	// velocity with regard to surface normal
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pCEvent->pt, ColorB(0,255,0,255), pCEvent->pt + (pCEvent->vloc[0] * vloc0Dir.Dot(pCEvent->n)), ColorB(0,255,0,255));

	if (!backface && impactVelSquared > (partImpThresh * partImpThresh))
	{
		IEntity* pEntitySrc = GetEntity(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
		IEntity* pEntityTrg = GetEntity(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

		TMFXEffectId effectId = InvalidEffectId;
		const int defaultSurfaceIndex = pMaterialEffects->GetDefaultSurfaceIndex();

		SMFXRunTimeEffectParams params;
		params.src = pEntitySrc ? pEntitySrc->GetId() : 0;
		params.trg = pEntityTrg ? pEntityTrg->GetId() : 0;
		params.srcSurfaceId = pCEvent->idmat[0];
		params.trgSurfaceId = pCEvent->idmat[1];
		params.fDecalPlacementTestMaxSize = pCEvent->fDecalPlacementTestMaxSize;

		//params.soundSemantic = eSoundSemantic_Physics_Collision;

		if (pCEvent->iForeignData[0] == PHYS_FOREIGN_ID_STATIC)
		{
			params.srcRenderNode = (IRenderNode*)pCEvent->pForeignData[0];
		}
		if (pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_STATIC)
		{
			params.trgRenderNode = (IRenderNode*)pCEvent->pForeignData[1];
		}
		if (pEntitySrc && pCEvent->idmat[0] == pMaterialEffects->GetDefaultCanopyIndex())
		{
			SVegCollisionStatus* test = s_this->m_treeStatus[params.src];
			if (!test)
			{
				IEntityRender* rp = pEntitySrc->GetRenderInterface();
				if (rp)
				{
					IRenderNode* rn = rp->GetRenderNode();
					if (rn)
					{
						effectId = pMaterialEffects->GetEffectIdByName("vegetation", "tree_impact");
						s_this->m_treeStatus[params.src] = new SVegCollisionStatus();
					}
				}
			}
		}

		//Prevent the same FX to be played more than once in mfx_Timeout time interval
		float fTimeOut = CMaterialEffectsCVars::Get().mfx_Timeout;
		for (int k = 0; k < MAX_CACHED_EFFECTS; k++)
		{
			SMFXRunTimeEffectParams& cachedParams = s_this->m_lstCachedEffects[k];
			if (cachedParams.src == params.src && cachedParams.trg == params.trg &&
			    cachedParams.srcSurfaceId == params.srcSurfaceId && cachedParams.trgSurfaceId == params.trgSurfaceId &&
			    cachedParams.srcRenderNode == params.srcRenderNode && cachedParams.trgRenderNode == params.trgRenderNode)
			{
				if (GetISystem()->GetITimer()->GetCurrTime() - cachedParams.fLastTime <= fTimeOut)
					return; // didnt timeout yet
			}
		}

		// add it overwriting the oldest one
		s_this->m_nEffectCounter = (s_this->m_nEffectCounter + 1) & (MAX_CACHED_EFFECTS - 1);
		SMFXRunTimeEffectParams& cachedParams = s_this->m_lstCachedEffects[s_this->m_nEffectCounter];
		cachedParams.src = params.src;
		cachedParams.trg = params.trg;
		cachedParams.srcSurfaceId = params.srcSurfaceId;
		cachedParams.trgSurfaceId = params.trgSurfaceId;
		//cachedParams.soundSemantic=params.soundSemantic;
		cachedParams.srcRenderNode = params.srcRenderNode;
		cachedParams.trgRenderNode = params.trgRenderNode;
		cachedParams.fLastTime = GetISystem()->GetITimer()->GetCurrTime();

		if (effectId == InvalidEffectId)
		{
			const char* pSrcArchetype = (pEntitySrc && pEntitySrc->GetArchetype()) ? pEntitySrc->GetArchetype()->GetName() : 0;
			const char* pTrgArchetype = (pEntityTrg && pEntityTrg->GetArchetype()) ? pEntityTrg->GetArchetype()->GetName() : 0;

			if (pEntitySrc)
			{
				if (pSrcArchetype)
					effectId = pMaterialEffects->GetEffectId(pSrcArchetype, pCEvent->idmat[1]);
				if (effectId == InvalidEffectId)
					effectId = pMaterialEffects->GetEffectId(pEntitySrc->GetClass(), pCEvent->idmat[1]);
			}
			if (effectId == InvalidEffectId && pEntityTrg)
			{
				if (pTrgArchetype)
					effectId = pMaterialEffects->GetEffectId(pTrgArchetype, pCEvent->idmat[0]);
				if (effectId == InvalidEffectId)
					effectId = pMaterialEffects->GetEffectId(pEntityTrg->GetClass(), pCEvent->idmat[0]);
			}

			if (effectId == InvalidEffectId)
			{
				effectId = pMaterialEffects->GetEffectId(pCEvent->idmat[0], pCEvent->idmat[1]);
				// No effect found, our world is crumbling around us, try the default material
#if !defined(_RELEASE)
				if (effectId == InvalidEffectId && pEntitySrc)
				{
					if (pSrcArchetype)
						effectId = pMaterialEffects->GetEffectId(pSrcArchetype, defaultSurfaceIndex);
					if (effectId == InvalidEffectId)
						effectId = pMaterialEffects->GetEffectId(pEntitySrc->GetClass(), defaultSurfaceIndex);
					if (effectId == InvalidEffectId && pEntityTrg)
					{
						if (pTrgArchetype)
							effectId = pMaterialEffects->GetEffectId(pTrgArchetype, defaultSurfaceIndex);
						if (effectId == InvalidEffectId)
							effectId = pMaterialEffects->GetEffectId(pEntityTrg->GetClass(), defaultSurfaceIndex);
						if (effectId == InvalidEffectId)
						{
							effectId = pMaterialEffects->GetEffectId(defaultSurfaceIndex, defaultSurfaceIndex);
						}
					}
				}
#endif
			}

		}

		if (effectId != InvalidEffectId)
		{
			//It's a bullet if it is a particle, has small mass and flies at high speed (>100m/s)
			const bool isBullet = pCEvent->pEntity[0] ? (pCEvent->pEntity[0]->GetType() == PE_PARTICLE && vloc0.len2() > 10000.0f && mass0 < 1.0f) : false;

			params.pos = pCEvent->pt;

			if (isBullet)
			{
				IGameFramework* pGameFrameWork = gEnv->pGameFramework;
				CRY_ASSERT(pGameFrameWork);

				IActor* pCollidedActor = pGameFrameWork->GetIActorSystem()->GetActor(params.trg);
				if (pCollidedActor)
				{
					if (pCollidedActor->IsClient())
					{
						Vec3 proxyOffset(ZERO);
						Matrix34 tm = pCollidedActor->GetEntity()->GetWorldTM();
						tm.Invert();

						IMovementController* pMV = pCollidedActor->GetMovementController();
						if (pMV)
						{
							SMovementState state;
							pMV->GetMovementState(state);
							params.pos = state.eyePosition + (state.eyeDirection.normalize() * 1.0f);
							params.audioProxyEntityId = params.trg;
							params.audioProxyOffset = tm.TransformVector((state.eyePosition + (state.eyeDirection * 1.0f)) - state.pos);

							//Do not play FX in FP
							params.playflags = eMFXPF_All & ~eMFXPF_Particles;
						}
					}
				}
				else if (pEntityTrg != nullptr)
				{
					// If 'render nearest is set on the target entity, then it is in FP mode
					if ((pEntityTrg->GetSlotFlags(0) & ENTITY_SLOT_RENDER_NEAREST) != 0)
					{
						const Matrix34& worldTm = pEntityTrg->GetWorldTM();
						params.pos += worldTm.GetColumn1();
						params.playflags = eMFXPF_All & ~eMFXPF_Particles;
					}
				}

			}

			params.decalPos = pCEvent->pt;
			params.normal = pCEvent->n;
			Vec3 vdir1 = vloc1.normalized();

			params.dir[0] = vdir0;
			params.dir[1] = vdir1;
			params.src = pEntitySrc ? pEntitySrc->GetId() : 0;
			params.trg = pEntityTrg ? pEntityTrg->GetId() : 0;
			params.partID = pCEvent->partid[1];

			float massMin = 0.0f;
			float massMax = 500.0f;
			float paramMin = 0.0f;
			float paramMax = 1.0f / 3.0f;

			// tiny - bullets
			if ((mass0 <= 0.1f) && pCEvent->pEntity[0] && pCEvent->pEntity[0]->GetType() == PE_PARTICLE)
			{
				// small
				massMin = 0.0f;
				massMax = 0.1f;
				paramMin = 0.0f;
				paramMax = 1.0f;
			}
			else if (mass0 < 20.0f)
			{
				// small
				massMin = 0.0f;
				massMax = 20.0f;
				paramMin = 0.0f;
				paramMax = 1.5f / 3.0f;
			}
			else if (mass0 < 200.0f)
			{
				// medium
				massMin = 20.0f;
				massMax = 200.0f;
				paramMin = 1.0f / 3.0f;
				paramMax = 2.0f / 3.0f;
			}
			else
			{
				// ultra large
				massMin = 200.0f;
				massMax = 2000.0f;
				paramMin = 2.0f / 3.0f;
				paramMax = 1.0f;
			}

			float p = min(1.0f, (mass0 - massMin) / (massMax - massMin));
			float finalparam = paramMin + (p * (paramMax - paramMin));

			params.AddAudioRtpc("mass", finalparam);
			params.AddAudioRtpc("speed", adjustedRelativeVelocity);

			bool playHit = true;
			if (g_waterHitOnly && isBullet)
			{
				pe_params_particle particleParams;
				pCEvent->pEntity[0]->GetParams(&particleParams);
				if (particleParams.dontPlayHitEffect)
				{
					playHit = false;
				}
				else if (pCEvent->idmat[1] == s_waterMaterialId)
				{
					pe_params_particle newParams;
					newParams.dontPlayHitEffect = 1; // prevent playing next time
					pCEvent->pEntity[0]->SetParams(&newParams);
				}
			}
			if (playHit)
			{
				pMaterialEffects->ExecuteEffect(effectId, params);
			}

			if (debug != 0)
			{
				pEntitySrc = GetEntity(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
				pEntityTrg = GetEntity(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

				ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				CryLogAlways("[MFX] Running effect for:");
				if (pEntitySrc)
				{
					const char* pSrcName = pEntitySrc->GetName();
					const char* pSrcClass = pEntitySrc->GetClass()->GetName();
					const char* pSrcArchetype = pEntitySrc->GetArchetype() ? pEntitySrc->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : SrcClass=%s SrcName=%s Arch=%s", pSrcClass, pSrcName, pSrcArchetype);
				}
				if (pEntityTrg)
				{
					const char* pTrgName = pEntityTrg->GetName();
					const char* pTrgClass = pEntityTrg->GetClass()->GetName();
					const char* pTrgArchetype = pEntityTrg->GetArchetype() ? pEntityTrg->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : TrgClass=%s TrgName=%s Arch=%s", pTrgClass, pTrgName, pTrgArchetype);
				}
				CryLogAlways("      : Mat0=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])->GetName());
				CryLogAlways("      : Mat1=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1])->GetName());
				CryLogAlways("impact-speed=%f fx-threshold=%f mass=%f speed=%f", sqrtf(impactVelSquared), partImpThresh, finalparam, adjustedRelativeVelocity);
			}
		}
		else
		{
			if (debug != 0)
			{
				pEntitySrc = GetEntity(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
				pEntityTrg = GetEntity(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

				ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				CryLogAlways("[MFX] Couldn't find effect for any combination of:");
				if (pEntitySrc)
				{
					const char* pSrcName = pEntitySrc->GetName();
					const char* pSrcClass = pEntitySrc->GetClass()->GetName();
					const char* pSrcArchetype = pEntitySrc->GetArchetype() ? pEntitySrc->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : SrcClass=%s SrcName=%s Arch=%s", pSrcClass, pSrcName, pSrcArchetype);
				}
				if (pEntityTrg)
				{
					const char* pTrgName = pEntityTrg->GetName();
					const char* pTrgClass = pEntityTrg->GetClass()->GetName();
					const char* pTrgArchetype = pEntityTrg->GetArchetype() ? pEntityTrg->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : TrgClass=%s TrgName=%s Arch=%s", pTrgClass, pTrgName, pTrgArchetype);
				}
				CryLogAlways("      : Mat0=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])->GetName());
				CryLogAlways("      : Mat1=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1])->GetName());
			}
		}
	}
	else
	{
		/*
		   if (debug != 0)
		   {
		   IEntity* pEntitySrc = GetEntity( pCEvent->iForeignData[0], pCEvent->pForeignData[0] );
		   IEntity* pEntityTrg = GetEntity( pCEvent->iForeignData[1], pCEvent->pForeignData[1] );

		   ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		   CryLogAlways("discarded:");
		   if (pEntitySrc)
		    CryLogAlways("      : SrcClass: %s SrcName: %s", pEntitySrc->GetClass()->GetName(), pEntitySrc->GetName());
		   if (pEntityTrg)
		    CryLogAlways("      : TrgClass: %s TrgName: %s", pEntityTrg->GetClass()->GetName(), pEntityTrg->GetName());
		   CryLogAlways("      : Mat0: %s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])->GetName());
		   CryLogAlways("      : Mat1: %s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1])->GetName());
		   if (backface)
		    CryLogAlways("backface collision");
		   CryLogAlways("impact speed squared: %f", impactVelSquared);
		   }
		 */
	}

	// this might be a good spot to add dedicated rolling sounds
	//if (!backface && pech)
	//{
	//	if ((pech->timeRolling > 0) && false)
	//	{
	//		IEntity* pEntitySrc = GetEntity( pCEvent->iForeignData[0], pCEvent->pForeignData[0] );
	//		IEntity* pEntityTrg = GetEntity( pCEvent->iForeignData[1], pCEvent->pForeignData[1] );

	//		ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	//		CryLogAlways("[MFX] Running effect for:");
	//		if (pEntitySrc)
	//			CryLogAlways("      : SrcClass: %s SrcName: %s", pEntitySrc->GetClass()->GetName(), pEntitySrc->GetName());
	//		if (pEntityTrg)
	//			CryLogAlways("      : TrgClass: %s TrgName: %s", pEntityTrg->GetClass()->GetName(), pEntityTrg->GetName());
	//		CryLogAlways("      : Mat0: %s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])->GetName());
	//		CryLogAlways("      : Mat1: %s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1])->GetName());
	//		CryLogAlways("rolling speed: %f, rolling time: %f", sqrtf(pech->velRoll2), pech->timeRolling);
	//	}
	//	else
	//	{
	//	}
	//}

	// --- End Material Effects Code ---
}

int CActionGame::OnPostStepLogged(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.postStep[0].begin();
	     it != s_this->m_globalPhysicsCallbacks.postStep[0].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysPostStep* pPostStep = static_cast<const EventPhysPostStep*>(pEvent);
	IGameObject* pSrc = s_this->GetPhysicalEntityGameObject(pPostStep->pEntity);

	if (pSrc && pSrc->WantsPhysicsEvent(eEPE_OnPostStepLogged))
	{
		SGameObjectEvent event(eGFE_OnPostStep, eGOEF_ToExtensions | eGOEF_ToGameObject | eGOEF_LoggedPhysicsEvent);
		event.ptr = (void*)pPostStep;
		pSrc->SendEvent(event);
	}

	OnPostStepLogged_MaterialFX(pEvent);

	return 1;
}

void CActionGame::OnPostStepLogged_MaterialFX(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const EventPhysPostStep* pPSEvent = (const EventPhysPostStep*) pEvent;
	const float maxSoundDist = 30.0f;
	Vec3 pos0 = CCryAction::GetCryAction()->GetISystem()->GetViewCamera().GetPosition();

	if ((pPSEvent->pos - pos0).len2() < sqr(maxSoundDist * 1.4f))
	{
		int id = s_this->m_pPhysicalWorld->GetPhysicalEntityId(pPSEvent->pEntity);
		std::map<int, SEntityCollHist*>::iterator iter;
		float velImpactThresh = 1.5f;

		if ((iter = s_this->m_mapECH.find(id)) != s_this->m_mapECH.end())
		{
			SEntityCollHist* pech = iter->second;
			bool bRemove = false;
			if ((pPSEvent->pos - pos0).len2() > sqr(maxSoundDist * 1.2f))
				bRemove = true;
			else
			{
				if (pech->velRoll2 < 0.1f)
				{
					if ((pech->timeNotRolling += pPSEvent->dt) > 0.15f)
						pech->timeRolling = 0;
				}
				else
				{
					pech->timeRolling += pPSEvent->dt;
					pech->timeNotRolling = 0;
				}
				if (pech->timeRolling < 0.2f)
					pech->velRoll2 = 0;

				if (pech->velRoll2 > 0.1f)
				{
					pech->rollTimeout = 0.7f;
					//CryLog("roll %.2f",sqrt_tpl(pech->velRoll2));
					pech->velRoll2 = 0;
					velImpactThresh = 3.5f;
				}
				else if (pech->velSlide2 > 0.1f)
				{
					pech->slideTimeout = 0.5f;
					//CryLog("slide %.2f",sqrt_tpl(pech->velSlide2));
					pech->velSlide2 = 0;
				}
				if (pech->velImpact > velImpactThresh)
				{
					//CryLog("impact %.2f",pech->velImpact);
					pech->velImpact = 0;
				}
				if (inrange(pech->rollTimeout, 0.0f, pPSEvent->dt))
				{
					pech->velRoll2 = 0;
					//CryLog("stopped rolling");
				}
				if (inrange(pech->slideTimeout, 0.0f, pPSEvent->dt))
				{
					pech->velSlide2 = 0;
					//CryLog("stopped sliding");
				}
				pech->rollTimeout -= pPSEvent->dt;
				pech->slideTimeout -= pPSEvent->dt;
			}
			if (bRemove)
			{
				s_this->m_mapECH.erase(iter);
				pech->pnext = s_this->m_pFreeCHSlot0->pnext;
				s_this->m_pFreeCHSlot0->pnext = pech;
			}
		}
	}
}

int CActionGame::OnStateChangeLogged(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.stateChange[0].begin();
	     it != s_this->m_globalPhysicsCallbacks.stateChange[0].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysStateChange* pStateChange = static_cast<const EventPhysStateChange*>(pEvent);
	IGameObject* pSrc = s_this->GetPhysicalEntityGameObject(pStateChange->pEntity);

	if (!gEnv->bServer && pSrc && pStateChange->iSimClass[1] > 1 && pStateChange->iSimClass[0] <= 1 && gEnv->pNetContext)
	{
		//CryLogAlways("[0] = %d, [1] = %d", pStateChange->iSimClass[0], pStateChange->iSimClass[1]);
		gEnv->pNetContext->RequestRemoteUpdate(pSrc->GetEntityId(), eEA_Physics);
	}

	if (pSrc && pSrc->WantsPhysicsEvent(eEPE_OnStateChangeLogged))
	{
		SGameObjectEvent event(eGFE_OnStateChange, eGOEF_ToExtensions | eGOEF_ToGameObject | eGOEF_LoggedPhysicsEvent);
		event.ptr = (void*)pStateChange;
		pSrc->SendEvent(event);
	}

	OnStateChangeLogged_MaterialFX(pEvent);

	return 1;
}

void CActionGame::OnStateChangeLogged_MaterialFX(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const EventPhysStateChange* pSCEvent = (const EventPhysStateChange*) pEvent;
	if (pSCEvent->iSimClass[0] + pSCEvent->iSimClass[1] * 4 == 6)
	{
		int id = s_this->m_pPhysicalWorld->GetPhysicalEntityId(pSCEvent->pEntity);
		std::map<int, SEntityCollHist*>::iterator iter;
		if ((iter = s_this->m_mapECH.find(id)) != s_this->m_mapECH.end())
		{
			if (iter->second->velRoll2 > 0)
			{}// CryLog("stopped rolling");
			if (iter->second->velSlide2 > 0)
			{}// CryLog("stopped sliding");
			iter->second->pnext = s_this->m_pFreeCHSlot0->pnext;
			s_this->m_pFreeCHSlot0->pnext = iter->second;
			s_this->m_mapECH.erase(iter);
		}
	}
}

int CActionGame::OnCreatePhysicalEntityLogged(const EventPhys* pEvent)
{
	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.createEntityPart[0].begin();
	     it != s_this->m_globalPhysicsCallbacks.createEntityPart[0].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysCreateEntityPart* pCEvent = (const EventPhysCreateEntityPart*) pEvent;
	IEntity* pEntity = GetEntity(pCEvent->iForeignData, pCEvent->pForeignData);

	// need to check what's broken (tree, glass, ....)
	if (!pEntity && pCEvent->iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		IRenderNode* rn = (IRenderNode*)pCEvent->pForeignData;
		if (eERType_Vegetation == rn->GetRenderNodeType())
		{
			IMaterialEffects* pMaterialEffects = CCryAction::GetCryAction()->GetIMaterialEffects();
			TMFXEffectId effectId = pMaterialEffects ? pMaterialEffects->GetEffectIdByName("vegetation", "tree_break") : InvalidEffectId;
			if (effectId != InvalidEffectId)
			{
				SMFXRunTimeEffectParams params;
				params.pos = rn->GetPos();
				params.dir[0] = params.dir[1] = Vec3(0, 0, 1);
				//params.soundSemantic = eSoundSemantic_Physics_General;
				pMaterialEffects->ExecuteEffect(effectId, params);
			}
		}
	}
	CryComment("CPE: %s", pEntity ? pEntity->GetName() : "Vegetation/Brush Object");

	if (pCEvent->pEntity->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY && pCEvent->pEntNew->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY &&
	    pCEvent->pEntity != pCEvent->pEntNew)
	{
		SBrokenEntPart bep;
		bep.idSrcEnt = ((IEntity*)pCEvent->pEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY))->GetId();
		bep.idNewEnt = ((IEntity*)pCEvent->pEntNew->GetForeignData(PHYS_FOREIGN_ID_ENTITY))->GetId();
		s_this->m_brokenEntParts.push_back(bep);
	}

	if (g_tree_cut_reuse_dist > 0 && !gEnv->bMultiplayer && pCEvent->cutRadius > 0)
		s_this->RegisterEntsForBreakageReuse(pCEvent->pEntity, pCEvent->partidSrc, pCEvent->pEntNew, pCEvent->cutPtLoc[0].z, pCEvent->breakSize);

	s_this->RegisterBrokenMesh(pCEvent->pEntNew, pCEvent->pMeshNew);

	return 1;
}

void CActionGame::RegisterEntsForBreakageReuse(IPhysicalEntity* pPhysEnt, int partid, IPhysicalEntity* pPhysEntNew, float h, float size)
{
	IEntity* pEntity;
	IEntitySubstitutionComponent* pSubst;
	IRenderNode* pVeg;
	pe_params_part pp;
	pp.partid = partid;
	if ((pEntity = (IEntity*)pPhysEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY)) &&
	    (pSubst = (IEntitySubstitutionComponent*)pEntity->GetProxy(ENTITY_PROXY_SUBSTITUTION)) &&
	    (pVeg = pSubst->GetSubstitute()) && pVeg->GetRenderNodeType() == eERType_Vegetation &&
	    pPhysEnt->GetParams(&pp) && pp.pPhysGeom->pGeom->GetSubtractionsCount() == 1)
	{
		std::map<IPhysicalEntity*, STreeBreakInst*>::iterator iter;
		STreeBreakInst* rec;
		STreePieceThunk* thunk, * thunkPrev;
		if ((iter = m_mapBrokenTreesByPhysEnt.find(pPhysEnt)) == m_mapBrokenTreesByPhysEnt.end())
		{
			float rscale = 1.0f / pEntity->GetScale().x;
			rec = new STreeBreakInst;
			rec->pPhysEntSrc = pPhysEnt;
			rec->pStatObj = pVeg->GetEntityStatObj();
			rec->cutHeight = h * rscale;
			rec->cutSize = size * rscale;
			rec->pPhysEntNew0 = pPhysEntNew;
			rec->pNextPiece = 0;
			rec->pThis = rec;
			rec->pNext = 0;
			m_mapBrokenTreesByPhysEnt.insert(std::pair<IPhysicalEntity*, STreeBreakInst*>(pPhysEnt, rec));
			std::map<IStatObj*, STreeBreakInst*>::iterator iter2;
			if ((iter2 = m_mapBrokenTreesByCGF.find(rec->pStatObj)) == m_mapBrokenTreesByCGF.end())
				m_mapBrokenTreesByCGF.insert(std::pair<IStatObj*, STreeBreakInst*>(rec->pStatObj, rec));
			else
			{
				rec->pNext = iter2->second->pNext;
				iter2->second->pNext = rec;
			}
			thunk = (STreePieceThunk*)&rec->pPhysEntNew0;
		}
		else
		{
			rec = iter->second;
			thunk = new STreePieceThunk;
			thunk->pPhysEntNew = pPhysEntNew;
			thunk->pNext = 0;
			for (thunkPrev = (STreePieceThunk*)&rec->pPhysEntNew0; thunkPrev->pNext; thunkPrev = thunkPrev->pNext)
				;
			thunkPrev->pNext = thunk;
			thunk->pParent = rec;
		}
		m_mapBrokenTreesChunks.insert(std::pair<IPhysicalEntity*, STreePieceThunk*>(thunk->pPhysEntNew, thunk));
	}
}

int CActionGame::ReuseBrokenTrees(const EventPhysCollision* pCEvent, float size, int flags)
{
	if (g_tree_cut_reuse_dist <= 0 || gEnv->bMultiplayer)
		return 0;
	IRenderNode* pVeg;
	std::map<IStatObj*, STreeBreakInst*>::iterator iter;
	Matrix34A objMat;

	if (pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_STATIC &&
	    (pVeg = (IRenderNode*)pCEvent->pForeignData[1])->GetRenderNodeType() == eERType_Vegetation &&
	    pVeg->GetPhysics() && pVeg->GetEntityStatObj() &&
	    (iter = m_mapBrokenTreesByCGF.find(pVeg->GetEntityStatObj())) != m_mapBrokenTreesByCGF.end())
	{
		STreeBreakInst* rec;
		IEntity* pentSrc, * pentClone;
		IPhysicalEntity* pPhysEntSrc, * pPhysEntClone;
		pVeg->GetEntityStatObj(0, &objMat);
		float scale = objMat.GetColumn(0).len();
		float hHit = pCEvent->pt.z - pVeg->GetPos().z;

		for (rec = iter->second; rec && (fabs_tpl(rec->cutHeight * scale - hHit) > g_tree_cut_reuse_dist ||
		                                 fabs_tpl(rec->cutSize * scale - size) > size * scale * 0.1f); rec = rec->pNext)
			;
		if (rec && (pentSrc = (IEntity*)(pPhysEntSrc = rec->pPhysEntSrc)->GetForeignData(PHYS_FOREIGN_ID_ENTITY)))
		{
			SEntitySpawnParams esp;
			SEntityPhysicalizeParams epp;
			pe_action_remove_all_parts arap;
			pe_params_part pp, pp1;
			pe_geomparams gp;
			pe_action_impulse ai;
			Matrix34A mtx;
			SBrokenEntPart bep;
			STreePieceThunk* thunk = (STreePieceThunk*)&rec->pPhysEntNew0;
			STreeBreakInst* recClone = new STreeBreakInst;
			IStatObj* pStatObj;

			recClone->pPhysEntSrc = pVeg->GetPhysics();
			recClone->pStatObj = rec->pStatObj;
			recClone->cutHeight = rec->cutHeight;
			recClone->cutSize = rec->cutSize;
			recClone->pPhysEntNew0 = 0;
			recClone->pNextPiece = 0;
			recClone->pThis = recClone;
			recClone->pNext = 0;
			m_mapBrokenTreesByPhysEnt.insert(std::pair<IPhysicalEntity*, STreeBreakInst*>(recClone->pPhysEntSrc, recClone));
			recClone->pNext = rec->pNext;
			rec->pNext = recClone;

			epp.type = PE_STATIC;
			esp.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Breakage");
			pStatObj = pVeg->GetEntityStatObj(0, &mtx);
			esp.vPosition = mtx.GetTranslation();
			esp.vScale = Vec3(mtx.GetColumn(0).len());
			esp.qRotation = Quat(Matrix33(mtx) / esp.vScale.x);
			pp1.flagsAND = ~geom_can_modify;
			if (m_bLoading)
				gEnv->pPhysicalWorld->SetPhysicalEntityId(pPhysEntSrc, (bep.idSrcEnt = UpdateEntityIdForVegetationBreak(pVeg)) & 0xFFFF);

			do
			{
				assert(pStatObj);
				PREFAST_ASSUME(pStatObj);
				esp.id = 0;
				esp.sName = pentSrc->GetName();
				if (m_bLoading && epp.type == PE_RIGID)
					UpdateEntityIdForBrokenPart(bep.idSrcEnt);
				pentClone = gEnv->pEntitySystem->SpawnEntity(esp, true);
				if (epp.type == PE_STATIC)
				{
					pentClone->AssignPhysicalEntity(pPhysEntClone = pVeg->GetPhysics());
					(crycomponent_cast<IEntitySubstitutionComponent*>(pentClone->CreateProxy(ENTITY_PROXY_SUBSTITUTION)))->SetSubstitute(pVeg);
					pVeg->SetPhysics(0);
					gEnv->p3DEngine->DeleteEntityDecals(pVeg);
					gEnv->p3DEngine->UnRegisterEntityAsJob(pVeg);
					if (!m_bLoading)
					{
						SBrokenVegPart bvp;
						bvp.pos = pVeg->GetPos();
						bvp.volume = pStatObj->GetPhysGeom()->V;
						bvp.idNewEnt = bep.idSrcEnt = pentClone->GetId();
						m_brokenVegParts.push_back(bvp);
					}
				}
				else
				{
					STreePieceThunk* thunkClone = 0;
					pentClone->Physicalize(epp);
					pPhysEntClone = pentClone->GetPhysics();
					if (!recClone->pPhysEntNew0)
						thunkClone = (STreePieceThunk*)&(recClone->pPhysEntNew0 = pPhysEntClone);
					else
					{
						STreePieceThunk* thunkPrev = 0;
						thunkClone = new STreePieceThunk;
						thunkClone->pPhysEntNew = pPhysEntClone;
						thunkClone->pNext = 0;
						for (thunkPrev = (STreePieceThunk*)&recClone->pPhysEntNew0; thunkPrev->pNext; thunkPrev = thunkPrev->pNext)
							;
						thunkPrev->pNext = thunkClone;
						thunkClone->pParent = recClone;
					}
					m_mapBrokenTreesChunks.insert(std::pair<IPhysicalEntity*, STreePieceThunk*>(thunkClone->pPhysEntNew, thunkClone));
					if (!m_bLoading)
					{
						bep.idNewEnt = pentClone->GetId();
						m_brokenEntParts.push_back(bep);
					}
				}

				if ((pStatObj = pentSrc->GetStatObj(0)))
				{
					pStatObj->SetFlags(pStatObj->GetFlags() & ~(STATIC_OBJECT_GENERATED | STATIC_OBJECT_CLONE));
					pentClone->SetStatObj(pStatObj, 0, false);
					pPhysEntClone->Action(&arap);
					for (pp.ipart = 0; pPhysEntSrc->GetParams(&pp); pp.ipart++)
					{
						gp.pos = pp.pos;
						gp.q = pp.q;
						gp.scale = scale;
						gp.flags = pp.flagsOR & ~(flags >> 16 & 0xFFFF | geom_can_modify);
						gp.flagsCollider = pp.flagsColliderOR;
						gp.minContactDist = pp.minContactDist;
						gp.idmatBreakable = pp.idmatBreakable;
						gp.pMatMapping = pp.pMatMapping;
						gp.nMats = pp.nMats;
						gp.mass = pp.mass;
						pPhysEntClone->AddGeometry(pp.pPhysGeom, &gp, pp.partid);
						pp1.ipart = pp.ipart;
						pPhysEntSrc->SetParams(&pp1);

						if (epp.type == PE_RIGID && pp.flagsOR & geom_colltype_ray)
						{
							Vec3 org = esp.vPosition + esp.qRotation * (gp.pos + gp.q * pp.pPhysGeom->origin), n = pCEvent->pt - org;
							ai.impulse = pCEvent->n * -pp.mass;
							if (n.len2() > 0)
							{
								ai.impulse -= n * ((n * ai.impulse) / n.len2());
								Quat q = esp.qRotation * gp.q * pp.pPhysGeom->q;
								ai.angImpulse = q * (Diag33(pp.pPhysGeom->Ibody) * (!q * (n ^ pCEvent->n))) * (cube(gp.scale) * pp.density / n.len2());
							}
							pPhysEntClone->Action(&ai);
						}
					}
					pentClone->PhysicalizeFoliage(0);
					epp.type = PE_RIGID;
					if (!thunk)
						break;
					pPhysEntSrc = thunk->pPhysEntNew;
					thunk = thunk->pNext;
				}
			}
			while (pentSrc = (IEntity*)pPhysEntSrc->GetForeignData(PHYS_FOREIGN_ID_ENTITY));

			return 1;
		}
	}

	return 0;
}

int CActionGame::OnUpdateMeshLogged(const EventPhys* pEvent)
{
	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.updateMesh[0].begin();
	     it != s_this->m_globalPhysicsCallbacks.updateMesh[0].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysUpdateMesh* pepum = (const EventPhysUpdateMesh*) pEvent;

	if (pepum->iForeignData == PHYS_FOREIGN_ID_STATIC && pepum->pEntity->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pEntity = (IEntity*)pepum->pEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (!pEntity)
			return 1;

		IRenderNode* pRenderNode = (IRenderNode*)pepum->pForeignData;
		IStatObj* pStatObj = pRenderNode->GetEntityStatObj();
		if (pStatObj)
		{
			phys_geometry* pPhysGeom = pRenderNode->GetEntityStatObj()->GetPhysGeom();
			if (pPhysGeom)
			{
				SBrokenVegPart bvp;

				bvp.pos = pRenderNode->GetPos();
				bvp.volume = pPhysGeom->V;
				bvp.idNewEnt = pEntity->GetId();
				s_this->m_brokenVegParts.push_back(bvp);
			}
		}
	}

	if (g_tree_cut_reuse_dist > 0 && !gEnv->bMultiplayer && pepum->pMesh && pepum->pMesh->GetSubtractionsCount() > 0)
		s_this->RemoveEntFromBreakageReuse(pepum->pEntity, pepum->pMesh->GetSubtractionsCount() <= 1);

	IEntity* pEntity;
	IStatObj* pStatObj;
	if (pepum->iReason == EventPhysUpdateMesh::ReasonDeform &&
	    pepum->iForeignData == PHYS_FOREIGN_ID_ENTITY && (pEntity = (IEntity*)pepum->pForeignData) &&
	    (pStatObj = pEntity->GetStatObj(ENTITY_SLOT_ACTUAL)) &&
	    (pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND) &&
	    pepum->pMeshSkel->GetForeignData())
	{
		SProcBrokenObjRec rec;
		rec.itype = PHYS_FOREIGN_ID_ENTITY;
		rec.islot = -1;
		rec.idEnt = pEntity->GetId();
		if (pStatObj->GetCloneSourceObject())
			pStatObj = pStatObj->GetCloneSourceObject();
		(rec.pStatObjOrg = pStatObj)->AddRef();
		pe_params_part pp;
		for (pp.ipart = 0, rec.mass = 0; pepum->pEntity->GetParams(&pp); pp.ipart++)
			rec.mass += pp.mass;
		s_this->m_brokenObjs.push_back(rec);

		pepum->pMeshSkel->SetForeignData(0, 0);
	}

	s_this->RegisterBrokenMesh(pepum->pEntity, pepum->pMesh, pepum->partid, 0, pepum->iReason == EventPhysUpdateMesh::ReasonDeform ? pepum->pMeshSkel : 0);

	return 1;
}

struct CrySizerNaive : ICrySizer
{
	CrySizerNaive() : m_count(0), m_size(0) {}
	virtual void                Release()        {}
	virtual size_t              GetTotalSize()   { return m_size; }
	virtual size_t              GetObjectCount() { return m_count; }
	virtual IResourceCollector* GetResourceCollector()
	{
		assert(0);
		return (IResourceCollector*)0;
	}
	virtual void Push(const char*)                                     {}
	virtual void PushSubcomponent(const char*)                         {}
	virtual void Pop()                                                 {}
	virtual bool AddObject(const void* id, size_t size, int count = 1) { m_size += size * count; m_count++; return true; }
	virtual void Reset()                                               { m_size = 0; }
	virtual void End()                                                 {}
	virtual void SetResourceCollector(IResourceCollector* pColl)       {};
	size_t m_count, m_size;
};

const char* GetStatObjName(IStatObj* pStatObj)
{
	if (!pStatObj)
		return "";
	for (IStatObj* pParent = pStatObj->GetCloneSourceObject(); pParent && pParent != pStatObj; pParent = pParent->GetCloneSourceObject())
		pStatObj = pParent;
	const char* ptr0 = pStatObj->GetFilePath(), * ptr;
	for (ptr = ptr0 + strlen(ptr0); ptr > ptr0 && ptr[-1] != '/'; ptr--)
		;
	return ptr;
}

const char* GetGeomName(const SBrokenMeshSize& bms)
{
	IStatObj* pStatObj = 0;
	if (IEntity* pEntity = (IEntity*)bms.pent->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
		pStatObj = pEntity->GetStatObj(bms.partid);
	else if (IRenderNode* pBrush = (IRenderNode*)bms.pent->GetForeignData(PHYS_FOREIGN_ID_STATIC))
		pStatObj = pBrush->GetEntityStatObj();
	return GetStatObjName(pStatObj);
}

void CActionGame::FreeBrokenMeshesForEntity(IPhysicalEntity* pent)
{
	// Remove all broken meshes for this entity
	if (pent)
	{
		int id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pent);
		std::map<int, SBrokenMeshSize>::iterator iter = m_mapBrokenMeshes.lower_bound(id << 8), nextIter;
		while (iter != m_mapBrokenMeshes.end() && (iter->first >> 8) == id)
		{
			CRY_ASSERT(pent == iter->second.pent);
			++(nextIter = iter);
			if (FreeBrokenMesh(pent, iter->second))
			{
				m_totBreakageSize -= iter->second.size;
				CryLog("Removed broken mesh for entity: %p; new total size %dKb", pent, m_totBreakageSize);
				m_mapBrokenMeshes.erase(iter);
			}
			iter = nextIter;
		}
	}
}

int CActionGame::OnPhysEntityDeleted(const EventPhys* pEvent)
{
	EventPhysEntityDeleted* peped = (EventPhysEntityDeleted*)pEvent;
	if (!s_this->m_inDeleteEntityCallback && (peped->iForeignData == PHYS_FOREIGN_ID_STATIC || peped->iForeignData == PHYS_FOREIGN_ID_ENTITY))
		s_this->FreeBrokenMeshesForEntity(peped->pEntity);
	return 1;
}

static void FreeSlotsAndFoilage(IEntity* pEntity)
{
	pEntity->DephysicalizeFoliage(0);
	SEntityPhysicalizeParams epp;
	epp.type = PE_NONE;
	pEntity->Physicalize(epp);
	for (int i = pEntity->GetSlotCount() - 1; i >= 0; i--)
		pEntity->FreeSlot(i);
}

int CActionGame::FreeBrokenMesh(IPhysicalEntity* pent, SBrokenMeshSize& bm)
{
	if (!pent || pent != bm.pent)
		return 0;
	IStatObj::SSubObject* pSubObj;
	IStatObj* pStatObj;
	int ifd = pent->GetiForeignData();
	void* pfd = pent->GetForeignData(ifd);
	if (ifd != PHYS_FOREIGN_ID_ENTITY && ifd != PHYS_FOREIGN_ID_STATIC || !pfd)
		return 0;
	IRenderNode* pRndNode = (IRenderNode*)pfd;
	IEntity* pEntity = (IEntity*)pfd;
	m_inDeleteEntityCallback = 1;
	RemoveEntFromBreakageReuse(pent, 0);

	pe_status_pos sp;
	pent->GetStatus(&sp);
	Vec3 center = sp.pos + (sp.BBox[1] + sp.BBox[0]) * 0.5f, dir(0, 0, 1);

	pe_params_part pp;
	pp.partid = bm.partid;
	int res = pent->GetParams(&pp);
	if (res)
	{
		primitives::box bbox;
		pp.pPhysGeom->pGeom->GetBBox(&bbox);
		center = sp.pos + sp.q * (pp.pos + pp.q * bbox.center * pp.scale);
		dir = sp.q * pp.q * bbox.Basis.GetRow(idxmin3(bbox.size));
	}

	if (res && pp.idSkeleton >= 0)
	{
		pStatObj = ifd == PHYS_FOREIGN_ID_ENTITY ? pEntity->GetStatObj(ENTITY_SLOT_ACTUAL) : pRndNode->GetEntityStatObj();
		IStatObj* pStatObjOrg = pStatObj->GetCloneSourceObject();
		IStatObj::SSubObject* pSubObjOrg;
		pe_params_skeleton ps;
		ps.partid = bm.partid;
		ps.bReset = 1;
		pent->SetParams(&ps);
		if (pStatObjOrg && (pSubObj = pStatObj->GetSubObject(bm.partid)) && (pSubObjOrg = pStatObjOrg->GetSubObject(bm.partid)))
		{
			if (pSubObj->pStatObj) pSubObj->pStatObj->Release();
			(pSubObj->pStatObj = pSubObjOrg->pStatObj);
			if (pSubObj->pStatObj) pSubObj->pStatObj->AddRef();
			new(&pp)pe_params_part;
			pp.partid = bm.partid;
			pp.pPhysGeom = pSubObjOrg->pStatObj ? pSubObjOrg->pStatObj->GetPhysGeom() : NULL;
			pent->SetParams(&pp);
		}
	}
	else
	{
		pent->RemoveGeometry(bm.partid);
		SEntitySlotInfo esli;

		if (ifd == PHYS_FOREIGN_ID_ENTITY && pEntity->GetProxy(ENTITY_PROXY_SUBSTITUTION))
		{
			// can't delete this entity - this will restore the vegetation object through the substitution proxy
			FreeSlotsAndFoilage(pEntity);
		}
		else if ((pStatObj = ifd == PHYS_FOREIGN_ID_ENTITY ? pEntity->GetStatObj(ENTITY_SLOT_ACTUAL) : pRndNode->GetEntityStatObj()) &&
		         (pSubObj = pStatObj->GetSubObject(bm.partid)) && pSubObj->pStatObj)
		{
			pSubObj->pStatObj->Release();
			pSubObj->pStatObj = 0;
			pSubObj->bHidden = true;
		}
		else if (ifd != PHYS_FOREIGN_ID_ENTITY)
			pRndNode->SetEntityStatObj(0, 0);
		else if (pEntity->GetSlotCount() <= 1 || pEntity->GetSlotCount() == 2 && pEntity->GetSlotInfo(1, esli) && (!esli.pStatObj || !esli.pStatObj->GetPhysGeom()))
		{
			CBreakReplicator* pBreakReplicator;
			if (gEnv->bMultiplayer && (pBreakReplicator = CBreakReplicator::Get()) != NULL)
			{
				FreeSlotsAndFoilage(pEntity);
				pBreakReplicator->RemoveEntity(pEntity);
			}
			else
			{
				gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
			}
		}
		else
			pEntity->FreeSlot(bm.partid);
	}

	IParticleEffect* pFractureFx;
	if (bm.timeout > 0 && bm.fractureFX && (pFractureFx = gEnv->pParticleManager->FindEffect(bm.fractureFX)))
		pFractureFx->Spawn(false, IParticleEffect::ParticleLoc(center, dir));

	if (!m_bLoading)
		m_brokenMeshRemovals.push_back(gEnv->pPhysicalWorld->GetPhysicalEntityId(pent) << 8 | bm.partid & 255);
	m_inDeleteEntityCallback = 0;
	return 1;
}

void CActionGame::RegisterBrokenMesh(IPhysicalEntity* pPhysEnt, IGeometry* pPhysGeom, int partid, IStatObj* pStatObj, IGeometry* pSkel,
                                     float timeout, const char* fractureFX)
{
	if (pPhysEnt && (g_breakage_mem_limit > 0 || timeout > 0.0f))
	{
		CrySizerNaive sizer;
		if (pPhysGeom)
		{

			pPhysGeom->GetMemoryStatistics(&sizer);

			if (void* pPlane = pPhysGeom->GetForeignData(1))
			{
				IBreakableManager* pBreakableManager = gEnv->pEntitySystem->GetBreakableManager();
				pBreakableManager->GetPlaneMemoryStatistics(pPlane, &sizer);
			}
		}
		if (pStatObj || pPhysGeom && (pStatObj = (IStatObj*)pPhysGeom->GetForeignData()))
		{
			pStatObj->GetMemoryUsage(&sizer);
			IRenderMesh* pRndMesh = pStatObj->GetRenderMesh();
			if (!pRndMesh && pStatObj->GetCloneSourceObject())
				pRndMesh = pStatObj->GetCloneSourceObject()->GetRenderMesh();
			if (pRndMesh)
				pRndMesh->GetMemoryUsage(&sizer, IRenderMesh::MEM_USAGE_COMBINED);
		}
		if (pSkel)
			pSkel->GetMemoryStatistics(&sizer);
		sizer.m_size = (sizer.m_size + 512) >> 10;

		int id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pPhysEnt) << 8 | partid & 255;
		std::map<int, SBrokenMeshSize>::iterator iter, iterNext;
		IStatObj* pStatObjNamer = pStatObj;
		if (IRenderNode* pBrush = (IRenderNode*)pPhysEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC))
			pStatObjNamer = pBrush->GetEntityStatObj();
		if ((iter = m_mapBrokenMeshes.find(id)) != m_mapBrokenMeshes.end())
		{
			if (iter->second.size == sizer.m_size)
				return;
			m_totBreakageSize -= iter->second.size;
			CryLog("Updated breakable mesh %s: %dKb->%" PRISIZE_T "Kb, new total size %" PRISIZE_T "Kb", GetStatObjName(pStatObjNamer), iter->second.size, sizer.m_size, m_totBreakageSize + sizer.m_size);
			m_mapBrokenMeshes.erase(iter);
		}

		if (!pPhysGeom)
			return;

		m_mapBrokenMeshes.insert(std::pair<int, SBrokenMeshSize>(id, SBrokenMeshSize(pPhysEnt, sizer.m_size, partid, timeout, fractureFX)));
		m_totBreakageSize += sizer.m_size;

		if (m_bLoading && gEnv->bServer)
		{
			IPhysicalEntity* pent;
			for (id = 0; id < m_brokenMeshRemovals.size() && m_brokenMeshRemovals[id] != -1; id++)
				if ((pent = gEnv->pPhysicalWorld->GetPhysicalEntityById(m_brokenMeshRemovals[id] >> 8)) &&
				    (iter = m_mapBrokenMeshes.find(m_brokenMeshRemovals[id])) != m_mapBrokenMeshes.end())
				{
					FreeBrokenMesh(pent, iter->second);
					m_totBreakageSize -= iter->second.size;
					m_mapBrokenMeshes.erase(iter);
				}
			size_t eraseTo = min((size_t)(id + 1), m_brokenMeshRemovals.size());
			m_brokenMeshRemovals.erase(m_brokenMeshRemovals.begin(), m_brokenMeshRemovals.begin() + eraseTo);
			return;
		}

		Vec3 pos0 = gEnv->pSystem->GetViewCamera().GetPosition();
		int idRndFrame = gEnv->pRenderer->GetFrameID();
		while (m_totBreakageSize * g_breakage_mem_limit > sqr(g_breakage_mem_limit))
		{
			IPhysicalEntity* pent;
			int ifd;
			void* pfd;
			float maxDist[2] = { 0, 0 };
			EntityId maxDistId[2];
			IRenderNode* pRndNode;

			for (iter = m_mapBrokenMeshes.begin(); iter != m_mapBrokenMeshes.end(); iter = iterNext)
				if (++(iterNext = iter),
				    (pent = gEnv->pPhysicalWorld->GetPhysicalEntityById(iter->first >> 8)) && pent == iter->second.pent &&
				    ((ifd = pent->GetiForeignData()) == PHYS_FOREIGN_ID_ENTITY || ifd == PHYS_FOREIGN_ID_STATIC) && (pfd = pent->GetForeignData(ifd)))
				{
					pe_params_part pp;
					pp.partid = iter->second.partid;
					if (pent == pPhysEnt && iter->second.partid == partid || pent->GetParams(&pp) && pp.pPhysGeom->pGeom->GetForeignData(DATA_MESHUPDATE))
						continue;
					IEntityRender* pRndProxy;
					if (ifd == PHYS_FOREIGN_ID_STATIC)
						pRndNode = (IRenderNode*)pfd;
					else if (pRndProxy = ((IEntity*)pfd)->GetRenderInterface())
						pRndNode = pRndProxy->GetRenderNode();
					else
						continue;
					int lastDrawFrame = pRndNode->GetDrawFrame();
					int bVisible = isneg(idRndFrame - lastDrawFrame - 10) || pent == pPhysEnt;
					float dist = (pos0 - pRndNode->GetBBox().GetCenter()).len2();
					if (maxDist[bVisible] < dist)
						maxDist[bVisible] = dist, maxDistId[bVisible] = iter->first;
				}
				else
				{
					// apparently the entity was deleted in the meantime
					CryLog("Removing obsolete breakable mesh record of %dKb", iter->second.size);
					m_totBreakageSize -= iter->second.size;
					m_mapBrokenMeshes.erase(iter);
				}
			if (maxDist[0] + maxDist[1] == 0.0f)
				break;

			if (!(pent = gEnv->pPhysicalWorld->GetPhysicalEntityById((id = maxDistId[maxDist[0] > 0.0f ? 0 : 1]) >> 8)))
				break;
			iter = m_mapBrokenMeshes.find(id);
			FreeBrokenMesh(pent, iter->second);

			m_totBreakageSize -= iter->second.size;
			m_mapBrokenMeshes.erase(iter);
			CryLog("Breakable memory limit breached - removing a mesh; new total size %dKb", m_totBreakageSize);
		}
		m_brokenMeshRemovals.push_back(-1);
	}
}

void CActionGame::DrawBrokenMeshes()
{
	std::map<int, SBrokenMeshSize>::iterator iter;
	int sizes[16] = { 0 }, keys[16] = { 0 }, nTop = 0, i;
	pe_status_pos sp;
	float clr[] = { 1.0f, 0.3f, 0.2f, 1.0f };
	Vec3 posCam = gEnv->pSystem->GetViewCamera().GetPosition();
	for (iter = m_mapBrokenMeshes.begin(); iter != m_mapBrokenMeshes.end(); ++iter)
	{
		sp.partid = iter->second.partid;
		if (!iter->second.pent->GetStatus(&sp))
			continue;
		Vec3 pos = (sp.BBox[0] + sp.BBox[1]) * 0.5f + sp.pos;
		IRenderAuxText::DrawLabelExF(pos, 1.4f, clr, true, true, "%s (%d Kb) - %.1fm", GetGeomName(iter->second), iter->second.size, (pos - posCam).len());
		for (i = 0; i<nTop&& sizes[i]> iter->second.size; i++)
			;
		if (i < 16)
		{
			nTop -= nTop >> 4;
			memmove(sizes + i + 1, sizes + i, (nTop - i) * sizeof(sizes[0]));
			memmove(keys + i + 1, keys + i, (nTop - i) * sizeof(keys[0]));
			sizes[i] = iter->second.size;
			keys[i] = iter->first;
			nTop++;
		}
	}
	IRenderAuxText::Draw2dLabel(10.0f, 60.0f, 1.3f, clr, false, "Top procedurally broken objects (total size - %d Kb):", m_totBreakageSize);
	for (i = 0; i < nTop; i++)
		IRenderAuxText::Draw2dLabel(10.0f, 72.0f + i * 12.0f, 1.3f, clr, false, "%d Kb %s", sizes[i], GetGeomName(m_mapBrokenMeshes.find(keys[i])->second));
}

void CActionGame::AddBroken2DChunkId(int id)
{
	s_this->m_broken2dChunkIds.push_back(id);
}

void CActionGame::UpdateBrokenMeshes(float dt)
{
	CDelayedPlaneBreak* pdpb;
	for (int i = m_pendingPlaneBreaks.size() - 1; i >= 0; i--)
		if ((pdpb = &m_pendingPlaneBreaks[i])->m_status == CDelayedPlaneBreak::eStatus_DONE)
		{
			if (pdpb->m_islandIn.pStatObj->Release() > 0)
				PerformPlaneBreak(pdpb->m_epc, &m_breakEvents[pdpb->m_iBreakEvent], ePPB_EnableParticleEffects, pdpb);
			pdpb->m_status = CDelayedPlaneBreak::eStatus_NONE;
		}

	std::map<int, SBrokenMeshSize>::iterator iter, iterNext;
	if (dt)
		for (iter = m_mapBrokenMeshes.begin(); iter != m_mapBrokenMeshes.end(); iter = iterNext)
			if (++(iterNext = iter), iter->second.timeout > 0 && (iter->second.timeout -= dt) <= 0.0)
			{
				iter->second.timeout += dt;
				FreeBrokenMesh(iter->second.pent, iter->second);
				m_totBreakageSize -= iter->second.size;
				CryLog("Removing broken mesh on timeout; new total size %dKb", m_totBreakageSize);
				m_mapBrokenMeshes.erase(iter);
			}
}

void CActionGame::RemoveEntFromBreakageReuse(IPhysicalEntity* pEntity, int bRemoveOnlyIfSecondary)
{
	std::map<IPhysicalEntity*, STreePieceThunk*>::iterator iter1;
	STreeBreakInst* rec = 0, * rec0;
	if ((iter1 = m_mapBrokenTreesChunks.find(pEntity)) != m_mapBrokenTreesChunks.end())
		rec = iter1->second->pParent;
	else if (!bRemoveOnlyIfSecondary)
	{
		std::map<IPhysicalEntity*, STreeBreakInst*>::iterator iter;
		if ((iter = m_mapBrokenTreesByPhysEnt.find(pEntity)) != m_mapBrokenTreesByPhysEnt.end())
			rec = iter->second;
	}
	if (rec)
	{
		m_mapBrokenTreesByPhysEnt.erase(rec->pPhysEntSrc);
		m_mapBrokenTreesChunks.erase(rec->pPhysEntNew0);
		STreePieceThunk* thunk, * thunkNext;
		for (thunk = rec->pNextPiece; thunk; thunk = thunkNext)
		{
			m_mapBrokenTreesChunks.erase(thunk->pPhysEntNew);
			thunkNext = thunk->pNext;
			delete thunk;
		}
		std::map<IStatObj*, STreeBreakInst*>::iterator iter;
		if ((iter = m_mapBrokenTreesByCGF.find(rec->pStatObj)) != m_mapBrokenTreesByCGF.end())
		{
			if (iter->second == rec)
			{
				if (rec->pNext)
					iter->second = rec->pNext;
				else
					m_mapBrokenTreesByCGF.erase(rec->pStatObj);
			}
			else
			{
				for (rec0 = iter->second; rec0->pNext != rec; rec0 = rec0->pNext)
					;
				rec0->pNext = rec->pNext;
			}
		}
		delete rec;
	}
}

int CActionGame::OnRemovePhysicalEntityPartsLogged(const EventPhys* pEvent)
{
	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.updateMesh[1].begin();
	     it != s_this->m_globalPhysicsCallbacks.updateMesh[1].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysRemoveEntityParts* pREvent = (EventPhysRemoveEntityParts*)pEvent;
	IEntity* pEntity;
	IStatObj* pStatObj;

	if (pREvent->iForeignData == PHYS_FOREIGN_ID_ENTITY && (pEntity = (IEntity*)pREvent->pForeignData))
	{
		int idOffs = pREvent->idOffs;
		if (pREvent->idOffs >= EntityPhysicsUtils::PARTID_LINKED)
			pEntity = pEntity->UnmapAttachedChild(idOffs);
		if (pEntity && (pStatObj = pEntity->GetStatObj(ENTITY_SLOT_ACTUAL)) && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
		{
			SProcBrokenObjRec rec;
			rec.itype = PHYS_FOREIGN_ID_ENTITY;
			rec.islot = -1;
			rec.idEnt = pEntity->GetId();
			if (pStatObj->GetCloneSourceObject() && pStatObj->GetCloneSourceObject()->GetSubObjectCount() >= pStatObj->GetSubObjectCount())
				pStatObj = pStatObj->GetCloneSourceObject();
			(rec.pStatObjOrg = pStatObj)->AddRef();
			if (pREvent->massOrg < 0.0001f)
			{
				float mass, density;
				pStatObj->GetPhysicalProperties(mass, density);
				rec.mass = mass;
			}
			else
				rec.mass = pREvent->massOrg;
			s_this->m_brokenObjs.push_back(rec);
		}
	}

	return 1;
}

int CActionGame::OnCollisionImmediate(const EventPhys* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.collision[1].begin();
	     it != s_this->m_globalPhysicsCallbacks.collision[1].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysCollision* pCollision = static_cast<const EventPhysCollision*>(pEvent);
	IGameObject* pSrc = s_this->GetPhysicalEntityGameObject(pCollision->pEntity[0]);
	IGameObject* pTrg = s_this->GetPhysicalEntityGameObject(pCollision->pEntity[1]);

	SGameObjectEvent event(eGFE_OnCollision, eGOEF_ToExtensions | eGOEF_ToGameObject);
	event.ptr = (void*)pCollision;

	if (pSrc && pSrc->WantsPhysicsEvent(eEPE_OnCollisionImmediate))
		pSrc->SendEvent(event);
	if (pTrg && pTrg->WantsPhysicsEvent(eEPE_OnCollisionImmediate))
		pTrg->SendEvent(event);

	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	ISurfaceType* pMat = pSurfaceTypeManager->GetSurfaceType(pCollision->idmat[1]);
	float energy, hitenergy;

	if (/*pCollision->mass[0]>=1500.0f &&*/ s_this->AllowProceduralBreaking(ePBT_Normal))
	{
		if (pMat)
			if (pMat->GetBreakability() == 1 &&
			    (pCollision->mass[0] > 10.0f || pCollision->pEntity[0] && pCollision->pEntity[0]->GetType() == PE_ARTICULATED ||
			     (pCollision->vloc[0] * pCollision->n < 0 &&
			      (energy = pMat->GetBreakEnergy()) > 0 &&
			      (hitenergy = max(fabs_tpl(pCollision->vloc[0].x) + fabs_tpl(pCollision->vloc[0].y) + fabs_tpl(pCollision->vloc[0].z),
			                       pCollision->vloc[0].len2()) * pCollision->mass[0]) >= energy &&
			      pMat->GetHitpoints() <= FtoI(min(1E6f, hitenergy / energy)))))
				return 0; // the object will break, tell the physics to ignore the collision
			else if (pMat->GetBreakability() == 2 &&
			         pCollision->idmat[0] != pCollision->idmat[1] &&
			         (energy = pMat->GetBreakEnergy()) > 0 &&
			         pCollision->mass[0] * 2 > energy &&
			         (hitenergy = max(fabs_tpl(pCollision->vloc[0].x) + fabs_tpl(pCollision->vloc[0].y) + fabs_tpl(pCollision->vloc[0].z),
			                          pCollision->vloc[0].len2()) * pCollision->mass[0]) >= energy &&
			         pMat->GetHitpoints() <= FtoI(min(1E6f, hitenergy / energy)) &&
			         pCollision)
				return 0;
	}

	return 1;
}

int CActionGame::OnPostStepImmediate(const EventPhys* pEvent)
{
	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.postStep[1].begin();
	     it != s_this->m_globalPhysicsCallbacks.postStep[1].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysPostStep* pPostStep = static_cast<const EventPhysPostStep*>(pEvent);
	IGameObject* piGameObject = s_this->GetPhysicalEntityGameObject(pPostStep->pEntity);

	// we cannot delete the gameobject while the event is processed
	if (piGameObject)
	{
		CGameObject* pGameObject = static_cast<CGameObject*>(piGameObject);
		pGameObject->AcquireMutex();

		// Test that the physics proxy is still enabled.
		if (piGameObject->WantsPhysicsEvent(eEPE_OnPostStepImmediate))
		{
			SGameObjectEvent event(eGFE_OnPostStep, eGOEF_ToExtensions | eGOEF_ToGameObject);
			event.ptr = (void*)pEvent;

			piGameObject->SendEvent(event);
		}
		pGameObject->ReleaseMutex();
	}

	return 1;
}

int CActionGame::OnStateChangeImmediate(const EventPhys* pEvent)
{
	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.stateChange[1].begin();
	     it != s_this->m_globalPhysicsCallbacks.stateChange[1].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	const EventPhysStateChange* pStateChange = static_cast<const EventPhysStateChange*>(pEvent);
	IGameObject* pSrc = s_this->GetPhysicalEntityGameObject(pStateChange->pEntity);

	if (pSrc && pSrc->WantsPhysicsEvent(eEPE_OnStateChangeImmediate))
	{
		SGameObjectEvent event(eGFE_OnStateChange, eGOEF_ToExtensions | eGOEF_ToGameObject);
		event.ptr = (void*)pStateChange;
		pSrc->SendEvent(event);
	}

	return 1;
}

int CActionGame::OnCreatePhysicalEntityImmediate(const EventPhys* pEvent)
{
	for (TGlobalPhysicsCallbackSet::const_iterator it = s_this->m_globalPhysicsCallbacks.createEntityPart[1].begin();
	     it != s_this->m_globalPhysicsCallbacks.createEntityPart[1].end();
	     ++it)
	{
		it->first(pEvent, it->second);
	}

	EventPhysCreateEntityPart* pepcep = (EventPhysCreateEntityPart*)pEvent;
	if (pepcep->iForeignData == PHYS_FOREIGN_ID_ENTITY && pepcep->pEntity != pepcep->pEntNew)
		gEnv->pPhysicalWorld->SetPhysicalEntityId(pepcep->pEntNew, s_this->UpdateEntityIdForBrokenPart(((IEntity*)pepcep->pForeignData)->GetId()) & 0xFFFF);
	if (g_tree_cut_reuse_dist > 0 && !gEnv->bMultiplayer && pepcep->cutRadius > 0)
		s_this->RegisterEntsForBreakageReuse(pepcep->pEntity, pepcep->partidSrc, pepcep->pEntNew, pepcep->cutPtLoc[0].z, pepcep->breakSize);
	s_this->RegisterBrokenMesh(pepcep->pEntNew, pepcep->pMeshNew);

	return 1;
}

EntityId CActionGame::UpdateEntityIdForBrokenPart(EntityId idSrc)
{
	std::map<EntityId, int>::iterator iter;
	EntityId id = 0;
	if ((iter = m_entPieceIdx.find(idSrc)) == m_entPieceIdx.end())
		iter = m_entPieceIdx.insert(std::pair<EntityId, int>(idSrc, -1)).first;
	for (++iter->second; iter->second < m_brokenEntParts.size() && m_brokenEntParts[iter->second].idSrcEnt != idSrc; ++iter->second)
		;
	if (iter->second < m_brokenEntParts.size())
		m_pEntitySystem->SetNextSpawnId(id = m_brokenEntParts[iter->second].idNewEnt);
	return id;
}

int CActionGame::OnUpdateMeshImmediate(const EventPhys* pEvent)
{
	EventPhysUpdateMesh* pepum = (EventPhysUpdateMesh*)pEvent;
	if (pepum->iForeignData == PHYS_FOREIGN_ID_STATIC)
		gEnv->pPhysicalWorld->SetPhysicalEntityId(pepum->pEntity, s_this->UpdateEntityIdForVegetationBreak((IRenderNode*)pepum->pForeignData) & 0xFFFF);
	if (g_tree_cut_reuse_dist > 0 && !gEnv->bMultiplayer && pepum->pMesh->GetSubtractionsCount() > 0)
		s_this->RemoveEntFromBreakageReuse(pepum->pEntity, pepum->pMesh->GetSubtractionsCount() <= 1);
	if (pepum->iReason != EventPhysUpdateMesh::ReasonDeform)
		s_this->RegisterBrokenMesh(pepum->pEntity, pepum->pMesh, pepum->partid, 0, 0);

	return 1;
}

EntityId CActionGame::UpdateEntityIdForVegetationBreak(IRenderNode* pVeg)
{
	Vec3 pos = pVeg->GetPos();
	float V = pVeg->GetEntityStatObj()->GetPhysGeom() ? pVeg->GetEntityStatObj()->GetPhysGeom()->V : 0;
	int i;
	EntityId id = 0;
	for (i = 0; i < m_brokenVegParts.size() && ((m_brokenVegParts[i].pos - pos).len2() > sqr(0.03f) ||
	                                            fabs_tpl(m_brokenVegParts[i].volume - V) > min(V, m_brokenVegParts[i].volume) * 1e-4f); i++)
		;
	if (i < m_brokenVegParts.size())
		m_pEntitySystem->SetNextSpawnId(id = m_brokenVegParts[i].idNewEnt);
	return id;
}

//////////////////////////////////
// HideBrokenObjectsByIndex() takes a list of indices into the BreakEvents array, which
//	is used to look up objects in the broken objects array, that are then hidden and then have
//	their decals removed
void CActionGame::HideBrokenObjectsByIndex(uint16* pObjectIndicies, int32 iNumObjectIndices)
{
	for (int k = iNumObjectIndices - 1; k >= 0; k--)
	{
		int iEventIndex = pObjectIndicies[k];

		int i = m_breakEvents[iEventIndex].iBrokenObjectIndex;

		if (i >= 0)
		{
			if (m_brokenObjs[i].itype == PHYS_FOREIGN_ID_STATIC)
			{
				//CryLogAlways("Hide & remove decals from original pBrush 0x%p at index %d", m_brokenObjs[i].pBrush, i);
				m_brokenObjs[i].pBrush->Hide(true);

				//At the moment there is no code support for hiding decals for later re-display. This means that to avoid
				//	decals being left floating in mid air, we need to remove them when the killcam starts
				gEnv->p3DEngine->DeleteEntityDecals(m_brokenObjs[i].pBrush);
			}
			else if (m_brokenObjs[i].itype == PHYS_FOREIGN_ID_ENTITY)
			{
				CryLogAlways("Hide & remove decals from ENTITY 0x%08X at index %d", m_brokenObjs[i].idEnt, i);
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_brokenObjs[i].idEnt);

				if (pEnt)
				{
					pEnt->Hide(true);
				}
			}
		}
	}
}

//////////////////////////////////
// UnhideBrokenObjectsByIndex() takes a list of indices into the BreakEvents array, which
//	is used to look up objects in the broken objects array, that are then unhidden
void CActionGame::UnhideBrokenObjectsByIndex(uint16* pObjectIndicies, int32 iNumObjectIndices)
{
	for (int k = iNumObjectIndices - 1; k >= 0; k--)
	{
		int iEventIndex = pObjectIndicies[k];

		int i = m_breakEvents[iEventIndex].iBrokenObjectIndex;

		if (i >= 0)
		{
			if (m_brokenObjs[i].itype == PHYS_FOREIGN_ID_STATIC)
			{
				BreakLogAlways("UNHIDING original pBrush 0x%p at index %d", m_brokenObjs[i].pBrush, i);
				m_brokenObjs[i].pBrush->Hide(false);

				//At the moment there is no code support for hiding decals for later re-display. This means that to avoid
				//	decals being left floating in mid air, we need to remove them when the killcam starts
				//gEnv->p3DEngine->DeleteEntityDecals(m_brokenObjs[i].pBrush);
			}
			else if (m_brokenObjs[i].itype == PHYS_FOREIGN_ID_ENTITY)
			{
				BreakLogAlways("UNHIDING ENTITY 0x%08X at index %d", m_brokenObjs[i].idEnt, i);
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_brokenObjs[i].idEnt);

				if (pEnt)
				{
					pEnt->Hide(false);
				}
			}
		}
	}
}

void CActionGame::ApplyBreakToClonedObjectFromEvent(const SRenderNodeCloneLookup& renderNodeLookup, int brokenObjIndex, int i)
{
	BreakLogAlways(">> Valid BrokenObjectIndex for break, %d, looking for IRenderNode: 0x%p", brokenObjIndex, m_brokenObjs[brokenObjIndex].pBrush);
	int iNodeIndex = -1;

	const int kNumNodes = renderNodeLookup.iNumPairs;

	for (int a = 0; a < kNumNodes; a++)
	{
		if (m_brokenObjs[brokenObjIndex].pBrush == renderNodeLookup.pOriginalNodes[a])
		{
			iNodeIndex = a;
			BreakLogAlways(">>>> Found break for index %d at time: %.6f", a, m_breakEvents[i].time);
			assert(m_breakEvents[i].time > 0.0f);
			break;
		}
	}

	if (iNodeIndex >= 0)
	{
		EventPhysCollision epc;

		epc.pEntity[0] = 0;
		epc.iForeignData[0] = -1;

		m_iCurBreakEvent = i;

		IRenderNode* pVeg = renderNodeLookup.pClonedNodes[iNodeIndex];

		if (m_breakEvents[i].itype == PHYS_FOREIGN_ID_STATIC)
		{
			BreakLogAlways(">>>  Break, original: 0x%p      new: 0x%p", renderNodeLookup.pOriginalNodes[iNodeIndex], renderNodeLookup.pClonedNodes[iNodeIndex]);
			epc.pForeignData[1] = pVeg;
			epc.iForeignData[1] = PHYS_FOREIGN_ID_STATIC;
			epc.pEntity[1] = pVeg->GetPhysics();
		}
		else
		{
			BreakLogAlways(">>>  Aborting, not PHYS_FOREIGN_ID_STATIC");
			return;
		}

		if (m_breakEvents[i].idmat[1] != -1)
		{
			epc.pt = m_breakEvents[i].pt;
			epc.n = m_breakEvents[i].n;
			epc.vloc[0] = m_breakEvents[i].vloc[0];
			epc.vloc[1] = m_breakEvents[i].vloc[1];
			epc.mass[0] = m_breakEvents[i].mass[0];
			epc.mass[1] = m_breakEvents[i].mass[1];
			epc.idmat[0] = m_breakEvents[i].idmat[0];
			epc.idmat[1] = m_breakEvents[i].idmat[1];
			epc.partid[0] = m_breakEvents[i].partid[0];
			epc.partid[1] = m_breakEvents[i].partid[1];
			epc.penetration = m_breakEvents[i].penetration;
			epc.radius = m_breakEvents[i].radius;
			BreakLogAlways(">>>  Break valid, carrying it out");
			PerformPlaneBreak(epc, &m_breakEvents[i], ePPB_PlaybackEvent, 0);
		}

		//This is for a purpose, some ents require 2 updates to form broken chunks
		//m_pPhysicalWorld->UpdateDeformingEntities();
		//m_pPhysicalWorld->UpdateDeformingEntities();
	}

}

////////////////////////////////
// ApplySingleProceduralBreakFromEventIndex() is used to apply a single break. This is used during killcam
//	playback to apply breaks as they are encountered in the event stream
void CActionGame::ApplySingleProceduralBreakFromEventIndex(uint16 uBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup)
{
	int index = m_breakEvents[uBreakEventIndex].iBrokenObjectIndex;
	if (index >= 0)
	{
		ApplyBreakToClonedObjectFromEvent(renderNodeLookup, index, uBreakEventIndex);
	}
}

/////////////////////////////////////////////////
//	This code is from ::FixBrokenObjects, with some minor changes. All break events up until a point in time
//		are applied to the list of IRenderNodes supplied by the caller.

void CActionGame::ApplyBreaksUntilTimeToObjectList(int iFirstBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup)
{
	int i = 0;

	m_bLoading = true;

	BreakLogAlways("<<<< Original -> Clone pBrush lookup PROCEDURAL >>>>>");
	for (int q = 0; q < renderNodeLookup.iNumPairs; q++)
	{
		BreakLogAlways("  Idx: %3d  -  Original 0x%p - Replay 0x%p", q, renderNodeLookup.pOriginalNodes[q], renderNodeLookup.pClonedNodes[q]);
	}

	BreakLogAlways("Applying breaks to index %d", iFirstBreakEventIndex);
	while (i < iFirstBreakEventIndex)
	{
		int index = m_breakEvents[i].iBrokenObjectIndex;
		if (index >= 0)
		{
			BreakLogAlways("> Valid break index on iter %d", i);
			ApplyBreakToClonedObjectFromEvent(renderNodeLookup, index, i);
		}
		else
		{
			BreakLogAlways("> Invalid break index on iter %d", i);
		}

		i++;
	}

	m_entPieceIdx.clear();
	m_bLoading = false;
}

void CActionGame::CloneBrokenObjectsByIndex(uint16* pBreakEventIndices, int32& iNumBreakEventIndices, IRenderNode** outClonedNodes, int32& iNumClonedNodes, SRenderNodeCloneLookup& nodeLookup)
{
	//Remove duplicates
	int iNumIndices = iNumBreakEventIndices;
	//CryLogAlways("Searching for duplicate indices");
	for (int i = iNumIndices - 1; i >= 0; i--)
	{
		const int iSearchIndex = m_breakEvents[pBreakEventIndices[i]].iBrokenObjectIndex;

		for (int j = iNumIndices - 1; j >= 0; j--)
		{
			if (m_breakEvents[pBreakEventIndices[j]].iBrokenObjectIndex == iSearchIndex && i != j)
			{
				//CryLogAlways("  Duplicate index found: %d, removing one", iSearchIndex);
				pBreakEventIndices[j] = pBreakEventIndices[iNumIndices - 1];
				iNumIndices--;
				i--;
			}
		}
	}

	int iLocalNumClonedNodes = iNumClonedNodes;

	for (int k = iNumIndices - 1; k >= 0; k--)
	{
		int iEventIndex = pBreakEventIndices[k];

		int i = m_breakEvents[iEventIndex].iBrokenObjectIndex;

		if (i >= 0)
		{
			//CryLogAlways("  Event %d has a valid broken object index of %d", iEventIndex, i);
			if (m_brokenObjs[i].itype == PHYS_FOREIGN_ID_ENTITY)
			{
				//TODO:
				//	Get the kill cam working for entities that have breakable planes. Doesn't currently. Code below would work,
				//	or nearly work, if there was an output array for cloned entities, not just StatObj

				//assert(0);
				//        IEntity * pOriginalEntity = gEnv->pEntitySystem->GetEntity(m_brokenObjs[i].idEnt);
				//
				//        IRenderNode * pRenderNode = m_brokenObjs[i].pBrush;
				//
				//        if(pOriginalEntity)
				//        {
				//          //CryLogAlways("PR:   Found entity with ID 0x%08X, cloning...", originalBrokenId);
				//
				//          IBreakableManager::SCreateParams createParams;
				//
				//          //createParams.nSlotIndex = 0;
				//          createParams.nSlotIndex = ENTITY_SLOT_ACTUAL;
				//          //createParams.slotTM;
				//          const Matrix34& worldTM = pOriginalEntity->GetWorldTM();
				//          createParams.worldTM = worldTM;
				//          createParams.fScale = worldTM.GetColumn0().len();
				//          createParams.pCustomMtl = pRenderNode->GetMaterial();
				//          createParams.nMatLayers = pRenderNode->GetMaterialLayers();
				//          createParams.nEntityFlagsAdd = (ENTITY_FLAG_NEVER_NETWORK_STATIC|ENTITY_FLAG_CLIENT_ONLY);
				//          //createParams.nEntitySlotFlagsAdd;
				//          createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
				//          createParams.pSrcStaticRenderNode = pRenderNode;
				//          createParams.pName			= pRenderNode->GetName();
				//          createParams.overrideEntityClass = pOriginalEntity->GetClass();
				//
				//          IBreakableManager * pBreakableMgr = gEnv->pEntitySystem->GetBreakableManager();
				//
				//          IEntity *pClonedEntity = pBreakableMgr->CreateObjectAsEntity(m_brokenObjs[i].pStatObjOrg, NULL, createParams);
				//
				//          assert(pClonedEntity);
				//
				//          pOutClonedEntities[iNumClonedEntitiesLocal] = pClonedEntity->GetId();
				//          iNumClonedEntitiesLocal++;
				//
				//          BreakLogAlways("PR:   Entity cloned, clone ID: 0x%08X, associating with ORIGINAL RENDERNODE 0x%p and ENTITY 0x%p",
				//            pClonedEntity->GetId(),
				//            reinterpret_cast<IRenderNode*>(pRenderNode),
				//            reinterpret_cast<IRenderNode*>(pOriginalEntity));
				//
				//          //Need to add both, because we don't know if a lookup is going to be for rendernode (first event) or for
				//          //	an entity (subsequent events)
				//          nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pRenderNode), reinterpret_cast<IRenderNode*>(pClonedEntity));
				//          nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pOriginalEntity), reinterpret_cast<IRenderNode*>(pClonedEntity));
				//        }
			}
			else
			{
				SProcBrokenObjRec& BrokenObj = m_brokenObjs[i];
				const EERType iRenderNodeType = BrokenObj.pBrush->GetRenderNodeType();

				if (iRenderNodeType == eERType_Brush)
				{
					//Clone the object, remove its physics, and register it for rendering
					//CryLogAlways("    iRenderNodeType eERType_Brush - adding");

					IRenderNode* pNewNode = BrokenObj.pBrush->Clone();

					pNewNode->SetEntityStatObj(BrokenObj.pStatObjOrg);

					IPhysicalEntity* pPhysEnt = pNewNode->GetPhysics();
					if (pPhysEnt)
					{
						gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhysEnt);
						pNewNode->SetPhysics(NULL);
					}

					gEnv->p3DEngine->RegisterEntity(pNewNode);
					outClonedNodes[iLocalNumClonedNodes] = pNewNode;
					iLocalNumClonedNodes++;
					nodeLookup.AddNodePair(BrokenObj.pBrush, pNewNode);
				}
				else if (iRenderNodeType == eERType_Vegetation)
				{
					//Clone the object, remove its physics, and register it for rendering
					BreakLogAlways("    iRenderNodeType eERType_Vegetation - adding");

					IRenderNode* pNewNode = gEnv->p3DEngine->CreateRenderNode(eERType_Brush);

					Matrix34A mtx;
					BrokenObj.pBrush->GetEntityStatObj(0, &mtx);

					BrokenObj.pBrush->CopyIRenderNodeData(pNewNode);

					pNewNode->SetEntityStatObj(BrokenObj.pStatObjOrg, &mtx);

					IPhysicalEntity* pPhysEnt = pNewNode->GetPhysics();
					if (pPhysEnt)
					{
						gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhysEnt);
						pNewNode->SetPhysics(NULL);
					}

					gEnv->p3DEngine->RegisterEntity(pNewNode);
					outClonedNodes[iLocalNumClonedNodes] = pNewNode;
					iLocalNumClonedNodes++;
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(BrokenObj.idEnt);
					if (pEntity)
					{
						nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pEntity), pNewNode);
					}

					nodeLookup.AddNodePair(BrokenObj.pBrush, pNewNode);
				}
				else
				{
					assert(!"RenderNode type not supported");
				}
			}
		}
		else
		{
			//CryLogAlways("  Event %d has an INVALID BROKEN OBJECT INDEX", iEventIndex);
		}
	}

	iNumBreakEventIndices = iNumIndices;
	iNumClonedNodes = iLocalNumClonedNodes;
}

void CActionGame::FixBrokenObjects(bool bRestoreBroken)
{
	//
	if (IBreakableManager* pBreakManager = gEnv->pEntitySystem->GetBreakableManager())
	{
		pBreakManager->ResetBrokenObjects();
	}

	// Even if cvar off, allow to reset in case it *was* enabled at some point
	if (IBreakableGlassSystem* pGlassSystem = CCryAction::GetCryAction()->GetIBreakableGlassSystem())
	{
		pGlassSystem->ResetAll();
	}

	//
	for (int i = m_brokenObjs.size() - 1; i >= 0; i--)
	{
		if (bRestoreBroken)
		{
			if (m_brokenObjs[i].itype == PHYS_FOREIGN_ID_ENTITY)
			{
				IEntity* pEnt = m_pEntitySystem->GetEntity(m_brokenObjs[i].idEnt);
				if (pEnt)
				{
					bool bHidden = pEnt->IsHidden();
					pEnt->Hide(false);
					if (m_brokenObjs[i].islot >= 0)
					{
						IStatObj* pStatObj = pEnt->GetStatObj(m_brokenObjs[i].islot);
						pEnt->SetStatObj(m_brokenObjs[i].pStatObjOrg, m_brokenObjs[i].islot, true);
					}
					else if (m_brokenObjs[i].pStatObjOrg)
					{
						for (int j = pEnt->GetSlotCount() - 1; j >= 0; j--)
							pEnt->FreeSlot(j);
						pEnt->SetStatObj(m_brokenObjs[i].pStatObjOrg, ENTITY_SLOT_ACTUAL, true, m_brokenObjs[i].mass);
					}
					else
						for (int j = pEnt->GetSlotCount() - 1; j > 0; j--)
							pEnt->FreeSlot(j);
					pEnt->Hide(bHidden);
				}
			}
			else
			{
				m_brokenObjs[i].pBrush->SetEntityStatObj(m_brokenObjs[i].pStatObjOrg);
				//m_brokenObjs[i].pBrush->GetEntityStatObj()->Refresh(FRO_GEOMETRY);
			}
		}
		if (m_brokenObjs[i].pStatObjOrg)
		{
			m_brokenObjs[i].pStatObjOrg->Release();
		}
	}
	m_brokenObjs.clear();
	m_mapEntHits.clear();
	for (SEntityHits* phits = m_pEntHits0; phits; phits = phits->pnext)
	{
		phits->lifeTime = 0;
		phits->timeUsed = 0;
		phits->nHits = 0;
	}
}

void CActionGame::OnEditorSetGameMode(bool bGameMode)
{
	LOADING_TIME_PROFILE_SECTION;
	FixBrokenObjects(true);
	ClearBreakHistory();

	// changed mode
	if (m_pEntitySystem)
	{
		IEntityItPtr pIt = m_pEntitySystem->GetEntityIterator();
		while (IEntity* pEntity = pIt->Next())
			CallOnEditorSetGameMode(pEntity, bGameMode);
	}

	REINST("notify the audio system?");

	if (!bGameMode)
	{
		s_this->m_vegStatus.clear();

		std::map<EntityId, SVegCollisionStatus*>::iterator it2 = s_this->m_treeStatus.begin();
		while (it2 != s_this->m_treeStatus.end())
		{
			SVegCollisionStatus* cur = (SVegCollisionStatus*)it2->second;
			if (cur)
			{
				delete cur;
			}
			++it2;
		}
		s_this->m_treeStatus.clear();
	}

	CCryAction* const pCryAction = CCryAction::GetCryAction();
	pCryAction->AllowSave(true);
	pCryAction->AllowLoad(true);

	// reset time of day scheduler before flowsystem
	// as nodes could register in initialize
	pCryAction->GetTimeOfDayScheduler()->Reset();
	gEnv->pFlowSystem->Reset(false);
	if (gEnv->pFlashUI) gEnv->pFlashUI->Reload();
	CDialogSystem* pDS = pCryAction->GetDialogSystem();
	if (pDS)
	{
		pDS->Reset(false);
		if (bGameMode && CDialogSystem::sAutoReloadScripts != 0)
		{
			pDS->ReloadScripts();
		}
	}

	pCryAction->GetPersistantDebug()->Reset();
}

void CActionGame::CallOnEditorSetGameMode(IEntity* pEntity, bool bGameMode)
{
	IScriptTable* pScriptTable(pEntity->GetScriptTable());
	if (!pScriptTable)
		return;

	if ((pScriptTable->GetValueType("OnEditorSetGameMode") == svtFunction) && pScriptTable->GetScriptSystem()->BeginCall(pScriptTable, "OnEditorSetGameMode"))
	{
		pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
		pScriptTable->GetScriptSystem()->PushFuncParam(bGameMode);
		pScriptTable->GetScriptSystem()->EndCall();
	}
}

void CActionGame::ClearBreakHistory()
{
	std::map<EntityId, SVegCollisionStatus*>::iterator it = m_treeBreakStatus.begin();
	while (it != m_treeBreakStatus.end())
	{
		SVegCollisionStatus* cur = (SVegCollisionStatus*)it->second;
		if (cur)
			delete cur;
		++it;
	}
	m_treeBreakStatus.clear();
	it = m_treeStatus.begin();
	while (it != m_treeStatus.end())
	{
		SVegCollisionStatus* cur = (SVegCollisionStatus*)it->second;
		if (cur)
			delete cur;
		++it;
	}
	m_treeStatus.clear();

	stl::free_container(m_breakEvents);
	stl::free_container(m_brokenEntParts);
	stl::free_container(m_brokenVegParts);
	stl::free_container(m_broken2dChunkIds);
	stl::free_container(m_brokenMeshRemovals);
	m_mapBrokenMeshes.clear();
	m_totBreakageSize = 0;
	ClearTreeBreakageReuseLog();
}

void CActionGame::ClearTreeBreakageReuseLog()
{
	for (std::map<IPhysicalEntity*, STreePieceThunk*>::iterator iter = m_mapBrokenTreesChunks.begin(); iter != m_mapBrokenTreesChunks.end(); ++iter)
		if (iter->second != (STreePieceThunk*)&iter->second->pParent->pPhysEntNew0)
			delete iter->second;
	for (std::map<IPhysicalEntity*, STreeBreakInst*>::iterator iter = m_mapBrokenTreesByPhysEnt.begin(); iter != m_mapBrokenTreesByPhysEnt.end(); ++iter)
		delete iter->second;
	m_mapBrokenTreesByPhysEnt.clear();
	m_mapBrokenTreesByCGF.clear();
	m_mapBrokenTreesChunks.clear();
}

void CActionGame::FlushBreakableObjects()
{
	int i;
	IEntity* pent;
	for (i = 0; i < m_brokenVegParts.size(); i++)
		if ((pent = m_pEntitySystem->GetEntity(m_brokenVegParts[i].idNewEnt)) && !(pent->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS))
			m_pEntitySystem->RemoveEntity(m_brokenVegParts[i].idNewEnt, true);
	for (i = 0; i < m_brokenEntParts.size(); i++)
		if ((pent = m_pEntitySystem->GetEntity(m_brokenEntParts[i].idNewEnt)) && !(pent->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS))
			m_pEntitySystem->RemoveEntity(m_brokenEntParts[i].idNewEnt, true);
	for (i = 0; i < m_broken2dChunkIds.size(); i++)
		if ((pent = m_pEntitySystem->GetEntity(m_broken2dChunkIds[i])) && !(pent->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS))
			m_pEntitySystem->RemoveEntity(m_broken2dChunkIds[i], true);
	// maybe clear vectors as well
}

void CActionGame::Serialize(TSerialize ser)
{
	SerializeBreakableObjects(ser);

	if (ser.IsReading())
	{
		for (int i = 0; i < MAX_CACHED_EFFECTS; ++i)
			m_lstCachedEffects[i].fLastTime = 0.0f; //reset timeout
	}
}

void CActionGame::SerializeBreakableObjects(TSerialize ser)
{
	SSerializeScopedBeginGroup breakableObjectsGroup(ser, "BreakableObjects");

	int i;
	if (ser.IsReading())
	{
#if 0 // don't remove them now. removing would increment salt of an reserved entity salthandle
		for (i = 0; i < m_brokenVegParts.size(); i++)
			m_pEntitySystem->RemoveEntity(m_brokenVegParts[i].idNewEnt, true);
		for (i = 0; i < m_brokenEntParts.size(); i++)
			m_pEntitySystem->RemoveEntity(m_brokenEntParts[i].idNewEnt, true);
		for (i = 0; i < m_broken2dChunkIds.size(); i++)
			m_pEntitySystem->RemoveEntity(m_broken2dChunkIds[i], true);
#endif
	}/*	else
	    {
	    IEntity *pent;
	    for(i=0;i<m_brokenVegParts.size();i++) if (pent=m_pEntitySystem->GetEntity(m_brokenVegParts[i].idNewEnt))
	    pent->ClearFlags(ENTITY_FLAG_SPAWNED|ENTITY_FLAG_SLOTS_CHANGED);
	    for(i=0;i<m_brokenEntParts.size();i++) if (pent=m_pEntitySystem->GetEntity(m_brokenEntParts[i].idNewEnt))
	    pent->ClearFlags(ENTITY_FLAG_SPAWNED|ENTITY_FLAG_SLOTS_CHANGED);
	    for(i=0;i<m_broken2dChunkIds.size();i++) if (pent=m_pEntitySystem->GetEntity(m_broken2dChunkIds[i]))
	    pent->ClearFlags(ENTITY_FLAG_SPAWNED|ENTITY_FLAG_SLOTS_CHANGED);
	    }*/

	ser.Value("BreakEvents", m_breakEvents);
	ser.Value("BrokenEntParts", m_brokenEntParts);
	ser.Value("BrokenVegParts", m_brokenVegParts);
	ser.Value("Broken2dChunkIds", m_broken2dChunkIds);
	ser.Value("BrokenMeshRemovals", m_brokenMeshRemovals);
	ser.Value("BreakageMemLimit", g_breakage_mem_limit);

	if (ser.IsReading())
	{
		m_mapBrokenMeshes.clear();
		m_totBreakageSize = 0;

		EventPhysCollision epc;
		IEntity* pEnt;
		pe_params_structural_joint psj;
		m_bLoading = true;
		m_pPhysicalWorld->GetPhysVars()->bLogStructureChanges = 0;
		m_entPieceIdx.clear();
		epc.pEntity[0] = 0;
		epc.iForeignData[0] = -1;
		ClearTreeBreakageReuseLog();

		m_pPhysicalWorld->UpdateDeformingEntities(-1);
		FixBrokenObjects(true);

		for (i = 0; i < m_breakEvents.size(); i++)
		{
			m_iCurBreakEvent = i;
			if ((epc.iForeignData[1] = m_breakEvents[i].itype) == PHYS_FOREIGN_ID_ENTITY)
			{
				if (!(pEnt = m_pEntitySystem->GetEntity(m_breakEvents[i].idEnt)))
				{
					m_pPhysicalWorld->UpdateDeformingEntities();
					if (!(pEnt = m_pEntitySystem->GetEntity(m_breakEvents[i].idEnt)))
						continue;
				}
				epc.pForeignData[1] = pEnt;
				epc.pEntity[1] = pEnt->GetPhysics();
				pEnt->SetPosRotScale(m_breakEvents[i].pos, m_breakEvents[i].rot, m_breakEvents[i].scale);
				if (!epc.pEntity[1] || pEnt->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS)
					continue;
			}
			else if (m_breakEvents[i].itype == PHYS_FOREIGN_ID_STATIC)
			{
				IPhysicalEntity* pent = CObjectSelector::FindPhysicalEntity(m_breakEvents[i].eventPos, m_breakEvents[i].hash, CObjectSelector_Eps);
				if (pent == NULL)
					continue;
				epc.pForeignData[1] = pent->GetForeignData(PHYS_FOREIGN_ID_STATIC);
				epc.pEntity[1] = pent;
				psj.idx = 0;
				MARK_UNUSED psj.id;
				if (!epc.pEntity[1] || (m_breakEvents[i].idmat[0] == -1 && epc.pEntity[1]->GetParams(&psj)))
					continue;
			}
			else
				continue;

			if (m_breakEvents[i].idmat[0] != -1)
			{
				epc.pt = m_breakEvents[i].pt;
				epc.n = m_breakEvents[i].n;
				epc.vloc[0] = m_breakEvents[i].vloc[0];
				epc.vloc[1] = m_breakEvents[i].vloc[1];
				epc.mass[0] = m_breakEvents[i].mass[0];
				epc.mass[1] = m_breakEvents[i].mass[1];
				epc.idmat[0] = m_breakEvents[i].idmat[0];
				epc.idmat[1] = m_breakEvents[i].idmat[1];
				epc.partid[0] = m_breakEvents[i].partid[0];
				epc.partid[1] = m_breakEvents[i].partid[1];
				epc.iPrim[0] = m_breakEvents[i].iPrim[0];
				epc.iPrim[1] = m_breakEvents[i].iPrim[1];
				epc.penetration = m_breakEvents[i].penetration;
				epc.radius = m_breakEvents[i].radius;
				OnCollisionLogged_Breakable(&epc);
			}
			else
				m_pPhysicalWorld->DeformPhysicalEntity(epc.pEntity[1], m_breakEvents[i].pt, m_breakEvents[i].n, m_breakEvents[i].penetration, 1);
			//This is for a purpose, some ents require 2 updates to form broken chunks
			m_pPhysicalWorld->UpdateDeformingEntities();
			m_pPhysicalWorld->UpdateDeformingEntities();
		}
		m_pPhysicalWorld->GetPhysVars()->bLogStructureChanges = 1;
		m_entPieceIdx.clear();
		m_bLoading = false;
	}
}

void CCryAction::Serialize(TSerialize ser)
{
	if (m_pGame)
		m_pGame->Serialize(ser);
}

void CCryAction::FlushBreakableObjects()
{
	if (m_pGame)
		m_pGame->FlushBreakableObjects();
}

void CCryAction::ClearBreakHistory()
{
	if (m_pGame)
		m_pGame->ClearBreakHistory();
}

void CActionGame::OnExplosion(const ExplosionInfo& ei)
{
	// This code is purely for serialisation, which is not needed in MP
	if (gEnv->bMultiplayer)
		return;

	int i, j, nEnts;
	IPhysicalEntity** pents;
	pe_status_nparts snp;
	pe_status_pos sp;
	pe_params_part pp;
	SBreakEvent be;
	memset(&be, 0, sizeof(be));
	IEntity* pEntity;

	nEnts = m_pPhysicalWorld->GetEntitiesInBox(ei.pos - Vec3(ei.physRadius), ei.pos + Vec3(ei.physRadius), pents, ent_static | ent_rigid | ent_sleeping_rigid);
	for (i = 0; i < nEnts; i++)
	{
		for (j = pents[i]->GetStatus(&snp) - 1; j >= 0; j--)
		{
			pp.ipart = j;
			MARK_UNUSED pp.partid;
			if (pents[i]->GetParams(&pp) && pp.idmatBreakable >= 0 && !(pp.flagsOR & geom_manually_breakable))
				break;
		}
		pents[i]->GetStatus(&sp);
		if (j >= 0)
		{
			switch (be.itype = pents[i]->GetiForeignData())
			{
			case PHYS_FOREIGN_ID_ENTITY:
				be.idEnt = (pEntity = (IEntity*)pents[i]->GetForeignData(be.itype))->GetId();
				be.pos = sp.pos;
				be.rot = sp.q;
				be.scale = pEntity->GetScale();
				break;
			case PHYS_FOREIGN_ID_STATIC:
				IRenderNode* pVeg = (IRenderNode*)pents[i]->GetForeignData(be.itype);
				be.eventPos = pVeg->GetBBox().GetCenter();
				be.hash = CObjectSelector::CalculateHash(pVeg, true);
				break;
			}

			be.pt = ei.pos;
			be.n = ei.dir;
			be.idmat[0] = -1;
			be.penetration = ei.hole_size;
			be.time = gEnv->pTimer->GetCurrTime();
			be.iBrokenObjectIndex = -1;
			m_breakEvents.push_back(be);
		}
	}
}

void CActionGame::DumpStats()
{
	if (m_pGameStats)
		m_pGameStats->Dump();
}

void CActionGame::GetMemoryUsage(ICrySizer* s) const
{
	{
		SIZER_SUBCOMPONENT_NAME(s, "Network");
		s->AddObject(m_pGameClientNub);
		s->AddObject(m_pGameServerNub);
		s->AddObject(m_pGameContext);
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "ActionGame");
		s->Add(*this);
		s->AddObject(m_pGameStats);
		s->AddObject(m_brokenObjs);
		s->AddObject(m_breakEvents);
		s->AddObject(m_brokenEntParts);
		s->AddObject(m_brokenVegParts);
		s->AddObject(m_broken2dChunkIds);
		s->AddObject(m_entPieceIdx);
		s->AddObject(m_mapECH);
		s->AddObject(m_mapEntHits);
		s->AddObject(m_vegStatus);
		s->AddObject(m_treeStatus);
		s->AddObject(m_treeBreakStatus);

	}
}

void CActionGame::CreateGameStats()
{
	m_pGameStats = new CGameStats(CCryAction::GetCryAction());
}

void CActionGame::ReleaseGameStats()
{
	delete m_pGameStats;
	m_pGameStats = 0;
}
