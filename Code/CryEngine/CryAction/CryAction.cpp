// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 20:7:2004   11:07 : Created by Marco Koegler
   - 3:8:2004		16:00 : Taken-ver by Marcio Martins
   - 2005              : Changed by everyone

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"

#define PRODUCT_VERSION_MAX_STRING_LENGTH (256)

//#define CRYACTION_DEBUG_MEM   // debug memory usage

#if CRY_PLATFORM_WINDOWS
	#include <CryCore/Platform/CryWindows.h>
	#include <ShellApi.h>
#endif

#include <CryCore/Platform/CryLibrary.h>
#include <CryCore/Platform/platform_impl.inl>

#include "AIDebugRenderer.h"
#include "GameRulesSystem.h"
#include "ScriptBind_ActorSystem.h"
#include "ScriptBind_ItemSystem.h"
#include "ScriptBind_Inventory.h"
#include "ScriptBind_ActionMapManager.h"
#include "Network/ScriptBind_Network.h"
#include "ScriptBind_Action.h"
#include "ScriptBind_VehicleSystem.h"
#include "FlashUI/ScriptBind_UIAction.h"

#include "Network/GameClientChannel.h"
#include "Network/GameServerChannel.h"
#include "Network/ScriptRMI.h"
#include "Network/GameQueryListener.h"
#include "Network/GameContext.h"
#include "Network/GameServerNub.h"
#include "Network/NetworkCVars.h"
#include "Network/GameStatsConfig.h"
#include "Network/NetworkStallTicker.h"

#include "AI/AIProxyManager.h"
#include "AI/BehaviorTreeNodes_Action.h"
#include <CryAISystem/ICommunicationManager.h>
#include <CryAISystem/IFactionMap.h>
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/INavigationSystem.h>
#include <CrySandbox/IEditorGame.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CrySystem/IStreamEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <CryGame/IGameStartup.h>

#include <CrySystem/IProjectManager.h>

#include <CryFlowGraph/IFlowSystem.h>
#include <CryFlowGraph/IFlowGraphModuleManager.h>

#include "Serialization/GameSerialize.h"

#include "Mannequin/AnimationDatabaseManager.h"

#include "DevMode.h"
#include "ActionGame.h"

#include "AIHandler.h"
#include "AIProxy.h"

#include "CryActionCVars.h"

// game object extensions
#include "Inventory.h"

#include "IVehicleSystem.h"

#include "EffectSystem/EffectSystem.h"
#include "VehicleSystem/ScriptBind_Vehicle.h"
#include "VehicleSystem/ScriptBind_VehicleSeat.h"
#include "VehicleSystem/Vehicle.h"
#include "AnimationGraph/AnimatedCharacter.h"
#include "AnimationGraph/AnimationGraphCVars.h"
#include "MaterialEffects/MaterialEffects.h"
#include "MaterialEffects/MaterialEffectsCVars.h"
#include "MaterialEffects/ScriptBind_MaterialEffects.h"
#include "GameObjects/GameObjectSystem.h"
#include "ViewSystem/ViewSystem.h"
#include "GameplayRecorder/GameplayRecorder.h"
#include "Analyst.h"
#include "BreakableGlassSystem.h"

#include "ForceFeedbackSystem/ForceFeedbackSystem.h"

#include "Network/GameClientNub.h"

#include "DialogSystem/DialogSystem.h"
#include "DialogSystem/ScriptBind_DialogSystem.h"
#include "SubtitleManager.h"

#include "LevelSystem.h"
#include "ActorSystem.h"
#include "ItemSystem.h"
#include "VehicleSystem.h"
#include "SharedParams/SharedParamsManager.h"
#include "ActionMapManager.h"
#include "ColorGradientManager.h"

#include "Statistics/GameStatistics.h"
#include "UIDraw/UIDraw.h"
#include "GameRulesSystem.h"
#include "ActionGame.h"
#include "IGameObject.h"
#include "CallbackTimer.h"
#include "PersistantDebug.h"
#include <CrySystem/ITextModeConsole.h>
#include "TimeOfDayScheduler.h"
#include "CooperativeAnimationManager/CooperativeAnimationManager.h"
#include "Network/CVarListProcessor.h"
#include "Network/BreakReplicator.h"
#include "CheckPoint/CheckPointSystem.h"
#include "GameSession/GameSessionHandler.h"

#include "AnimationGraph/DebugHistory.h"

#include "PlayerProfiles/PlayerProfileManager.h"
#include "PlayerProfiles/PlayerProfileImplFS.h"
#include "PlayerProfiles/PlayerProfileImplConsole.h"
#if CRY_PLATFORM_DURANGO
	#include "PlayerProfiles/PlayerProfileImplDurango.h"
#endif
#if CRY_PLATFORM_ORBIS
	#include "PlayerProfiles/PlayerProfileImplOrbis.h"
#endif

#include "RemoteControl/RConServerListener.h"
#include "RemoteControl/RConClientListener.h"

#include "SimpleHttpServer/SimpleHttpServerListener.h"
#include "SimpleHttpServer/SimpleHttpServerWebsocketEchoListener.h"

#include "SignalTimers/SignalTimers.h"
#include "RangeSignalingSystem/RangeSignaling.h"

#include "LivePreview/RealtimeRemoteUpdate.h"

#include "CustomActions/CustomActionManager.h"
#include "CustomEvents/CustomEventsManager.h"

#ifdef GetUserName
	#undef GetUserName
#endif

#include "TestSystem/TimeDemoRecorder.h"
#include <CryNetwork/INetworkService.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CryInput/IHardwareMouse.h>

#include "Serialization/XmlSerializeHelper.h"
#include "Serialization/XMLCPBin/BinarySerializeHelper.h"
#include "Serialization/XMLCPBin/Writer/XMLCPB_ZLibCompressor.h"
#include "Serialization/XMLCPBin/XMLCPB_Utils.h"

#include "CryActionPhysicQueues.h"

#include "Mannequin/MannequinInterface.h"

#include "GameVolumes/GameVolumesManager.h"

#include "RuntimeAreas.h"

#include <CrySystem/Scaleform/IFlashUI.h>

#include "LipSync/LipSync_TransitionQueue.h"
#include "LipSync/LipSync_FacialInstance.h"

#include <CryFlowGraph/IFlowBaseNode.h>

#if defined(_LIB) && !defined(DISABLE_LEGACY_GAME_DLL)
extern "C" IGameStartup* CreateGameStartup();
#endif //_LIB

#define DEFAULT_BAN_TIMEOUT     (30.0f)

#define PROFILE_CONSOLE_NO_SAVE (0)     // For console demo's which don't save their player profile

#if PROFILE_CONSOLE_NO_SAVE
	#include "PlayerProfiles/PlayerProfileImplNoSave.h"
#endif
#include "Network/NetMsgDispatcher.h"
#include "EntityContainers/EntityContainerMgr.h"
#include "FlowSystem/Nodes/FlowEntityCustomNodes.h"

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>

CCryAction* CCryAction::m_pThis = 0;

static const int s_saveGameFrameDelay = 3; // enough to render enough frames to display the save warning icon before the save generation

static const float s_loadSaveDelay = 0.5f;  // Delay between load/save operations.

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListener_Action : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			if (!gEnv->IsEditor())
			{
				CCryAction::GetCryAction()->GetMannequinInterface().UnloadAll();
			}
			break;
		case ESYSTEM_EVENT_3D_POST_RENDERING_END:
			{
				if (IMaterialEffects* pMaterialEffects = CCryAction::GetCryAction()->GetIMaterialEffects())
				{
					pMaterialEffects->Reset(true);
				}
				break;
			}
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
			{
				CCryAction::GetCryAction()->FastShutdown();
			}
			break;
		}
	}
};
static CSystemEventListener_Action g_system_event_listener_action;

void CCryAction::DumpMemInfo(const char* format, ...)
{
	CryModuleMemoryInfo memInfo;
	CryGetMemoryInfoForModule(&memInfo);

	va_list args;
	va_start(args, format);
	gEnv->pLog->LogV(ILog::eAlways, format, args);
	va_end(args);

	gEnv->pLog->LogWithType(ILog::eAlways, "Alloc=%" PRIu64 "d kb  String=%" PRIu64 " kb  STL-alloc=%" PRIu64 " kb  STL-wasted=%" PRIu64 " kb", (memInfo.allocated - memInfo.freed) >> 10, memInfo.CryString_allocated >> 10, memInfo.STL_allocated >> 10, memInfo.STL_wasted >> 10);
}

// no dot use iterators in first part because of calls of some listners may modify array of listeners (add new)
#define CALL_FRAMEWORK_LISTENERS(func)                                  \
  {                                                                     \
    for (size_t n = 0; n < m_pGFListeners->size(); n++)                 \
    {                                                                   \
      const SGameFrameworkListener& le = m_pGFListeners->operator[](n); \
      if (m_validListeners[n])                                          \
        le.pListener->func;                                             \
    }                                                                   \
    TGameFrameworkListeners::iterator it = m_pGFListeners->begin();     \
    std::vector<bool>::iterator vit = m_validListeners.begin();         \
    while (it != m_pGFListeners->end())                                 \
    {                                                                   \
      if (!*vit)                                                        \
      {                                                                 \
        it = m_pGFListeners->erase(it);                                 \
        vit = m_validListeners.erase(vit);                              \
      }                                                                 \
      else                                                              \
        ++it, ++vit;                                                    \
    }                                                                   \
  }

//------------------------------------------------------------------------
CCryAction::CCryAction(SSystemInitParams& initParams)
	: m_paused(false),
	m_forcedpause(false),
	m_pSystem(0),
	m_pNetwork(0),
	m_p3DEngine(0),
	m_pScriptSystem(0),
	m_pEntitySystem(0),
	m_pTimer(0),
	m_pLog(0),
	m_pGameToEditor(nullptr),
	m_pGame(0),
	m_pLevelSystem(0),
	m_pActorSystem(0),
	m_pItemSystem(0),
	m_pVehicleSystem(0),
	m_pSharedParamsManager(0),
	m_pActionMapManager(0),
	m_pViewSystem(0),
	m_pGameplayRecorder(0),
	m_pGameplayAnalyst(0),
	m_pGameRulesSystem(0),
	m_pGameObjectSystem(0),
	m_pScriptRMI(0),
	m_pUIDraw(0),
	m_pAnimationGraphCvars(0),
	m_pMannequin(0),
	m_pMaterialEffects(0),
	m_pBreakableGlassSystem(0),
	m_pForceFeedBackSystem(0),
	m_pPlayerProfileManager(0),
	m_pDialogSystem(0),
	m_pSubtitleManager(0),
	m_pEffectSystem(0),
	m_pGameSerialize(0),
	m_pCallbackTimer(0),
	m_pLanQueryListener(0),
	m_pDevMode(0),
	m_pTimeDemoRecorder(0),
	m_pGameQueryListener(0),
	m_pRuntimeAreaManager(NULL),
	m_pScriptA(0),
	m_pScriptIS(0),
	m_pScriptAS(0),
	m_pScriptNet(0),
	m_pScriptAMM(0),
	m_pScriptVS(0),
	m_pScriptBindVehicle(0),
	m_pScriptBindVehicleSeat(0),
	m_pScriptInventory(0),
	m_pScriptBindDS(0),
	m_pScriptBindMFX(0),
	m_pScriptBindUIAction(0),
	m_pPersistantDebug(0),
	m_pColorGradientManager(nullptr),
#ifdef USE_NETWORK_STALL_TICKER_THREAD
	m_pNetworkStallTickerThread(nullptr),
	m_networkStallTickerReferences(0),
#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD
	m_pMaterialEffectsCVars(0),
	m_pEnableLoadingScreen(0),
	m_pShowLanBrowserCVAR(0),
	m_pDebugSignalTimers(0),
	m_pAsyncLevelLoad(0),
	m_pDebugRangeSignaling(0),
	m_bShowLanBrowser(false),
	m_isEditing(false),
	m_bScheduleLevelEnd(false),
	m_delayedSaveGameMethod(eSGM_NoSave),
	m_delayedSaveCountDown(0),
	m_pLocalAllocs(0),
	m_bAllowSave(true),
	m_bAllowLoad(true),
	m_pGFListeners(0),
	m_pBreakEventListener(NULL),
	m_nextFrameCommand(0),
	m_lastSaveLoad(0.0f),
	m_lastFrameTimeUI(0.0f),
	m_pbSvEnabled(false),
	m_pbClEnabled(false),
	m_pGameStatistics(0),
	m_pAIDebugRenderer(0),
	m_pAINetworkDebugRenderer(0),
	m_pCooperativeAnimationManager(NULL),
	m_pGameSessionHandler(0),
	m_pAIProxyManager(0),
	m_pCustomActionManager(0),
	m_pCustomEventManager(0),
	m_pPhysicsQueues(0),
	m_PreUpdateTicks(0),
	m_pGameVolumesManager(NULL),
	m_pNetMsgDispatcher(nullptr),
	m_pEntityContainerMgr(nullptr),
	m_pEntityAttachmentExNodeRegistry(nullptr)
{
	CRY_ASSERT(!m_pThis);
	m_pThis = this;

	m_editorLevelName[0] = 0;
	m_editorLevelFolder[0] = 0;
	cry_strcpy(m_gameGUID, "{00000000-0000-0000-0000-000000000000}");

	Initialize(initParams);
}

CCryAction::~CCryAction()
{
}

//------------------------------------------------------------------------
void CCryAction::DumpMapsCmd(IConsoleCmdArgs* args)
{
	int nlevels = GetCryAction()->GetILevelSystem()->GetLevelCount();
	if (!nlevels)
		CryLogAlways("$3No levels found!");
	else
		CryLogAlways("$3Found %d levels:", nlevels);

	for (int i = 0; i < nlevels; i++)
	{
		ILevelInfo* level = GetCryAction()->GetILevelSystem()->GetLevelInfo(i);
		const uint32 scanTag = level->GetScanTag();
		const uint32 levelTag = level->GetLevelTag();

		CryLogAlways("  %s [$9%s$o] Scan:%.4s Level:%.4s", level->GetName(), level->GetPath(), (char*)&scanTag, (char*)&levelTag);
	}
}
//------------------------------------------------------------------------

void CCryAction::ReloadReadabilityXML(IConsoleCmdArgs*)
{
	CAIFaceManager::LoadStatic();
}

//------------------------------------------------------------------------
void CCryAction::UnloadCmd(IConsoleCmdArgs* args)
{
	if (gEnv->IsEditor())
	{
		GameWarning("Won't unload level in editor");
		return;
	}

	CCryAction* pAction = CCryAction::GetCryAction();
	pAction->StartNetworkStallTicker(false);
	// Free context
	pAction->EndGameContext();
	pAction->StopNetworkStallTicker();
}

//------------------------------------------------------------------------
void CCryAction::StaticSetPbSvEnabled(IConsoleCmdArgs* pArgs)
{
	if (!m_pThis)
		return;

	if (pArgs->GetArgCount() != 2)
		GameWarning("usage: net_pb_sv_enable <true|false>");
	else
	{
		string cond = pArgs->GetArg(1);
		if (cond == "true")
			m_pThis->m_pbSvEnabled = true;
		else if (cond == "false")
			m_pThis->m_pbSvEnabled = false;
	}

	GameWarning("PunkBuster server will be %s for the next MP session", m_pThis->m_pbSvEnabled ? "enabled" : "disabled");
}

//------------------------------------------------------------------------
void CCryAction::StaticSetPbClEnabled(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() != 2)
		GameWarning("usage: net_pb_cl_enable <true|false>");
	else
	{
		string cond = pArgs->GetArg(1);
		if (cond == "true")
			m_pThis->m_pbClEnabled = true;
		else if (cond == "false")
			m_pThis->m_pbClEnabled = false;
	}

	GameWarning("PunkBuster client will be %s for the next MP session", m_pThis->m_pbClEnabled ? "enabled" : "disabled");
}

uint16 ChooseListenPort()
{
	if (gEnv->bMultiplayer)
	{
		if (gEnv->pLobby)
		{
			if (gEnv->pLobby->GetLobbyService() != nullptr)
			{
				return gEnv->pLobby->GetLobbyParameters().m_listenPort;
			}
		}
	}
	return gEnv->pConsole->GetCVar("sv_port")->GetIVal();
}

//------------------------------------------------------------------------
void CCryAction::MapCmd(IConsoleCmdArgs* args)
{
	SLICE_SCOPE_DEFINE();

	LOADING_TIME_PROFILE_SECTION;
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "MapCmd");

	uint32 flags = eGSF_NonBlockingConnect;

	// not available in the editor
	if (gEnv->IsEditor())
	{
		GameWarning("Won't load level in editor");
		return;
	}

	if (GetCryAction()->StartingGameContext())
	{
		GameWarning("Can't process map command while game context is starting!");
		return;
	}

	class CParamCheck
	{
	public:
		void          AddParam(const string& param) { m_commands.insert(param); }
		const string* GetFullParam(const string& shortParam)
		{
			m_temp = m_commands;

			for (string::const_iterator pChar = shortParam.begin(); pChar != shortParam.end(); ++pChar)
			{
				typedef std::set<string>::iterator I;
				I next;
				for (I iter = m_temp.begin(); iter != m_temp.end(); )
				{
					next = iter;
					++next;
					if ((*iter)[pChar - shortParam.begin()] != *pChar)
						m_temp.erase(iter);
					iter = next;
				}
			}

			const char* warning = 0;
			const string* ret = 0;
			switch (m_temp.size())
			{
			case 0:
				warning = "Unknown command %s";
				break;
			case 1:
				ret = &*m_temp.begin();
				break;
			default:
				warning = "Ambiguous command %s";
				break;
			}
			if (warning)
				GameWarning(warning, shortParam.c_str());

			return ret;
		}

	private:
		std::set<string> m_commands;
		std::set<string> m_temp;
	};

	IConsole* pConsole = gEnv->pConsole;

	CryStackStringT<char, 256> currentMapName;
	{
		string mapname;

		// check if a map name was provided
		if (args->GetArgCount() > 1)
		{
			// set sv_map
			mapname = args->GetArg(1);
			mapname.replace("\\", "/");

			if (mapname.find("/") == string::npos)
			{
				const char* gamerules = pConsole->GetCVar("sv_gamerules")->GetString();

				int i = 0;
				const char* loc = 0;
				string tmp;
				while (loc = CCryAction::GetCryAction()->m_pGameRulesSystem->GetGameRulesLevelLocation(gamerules, i++))
				{
					tmp = loc;
					tmp.append(mapname);

					if (CCryAction::GetCryAction()->m_pLevelSystem->GetLevelInfo(tmp.c_str()))
					{
						mapname = tmp;
						break;
					}
				}
			}

			pConsole->GetCVar("sv_map")->Set(mapname);

			currentMapName = mapname;
		}
	}

	const char* tempGameRules = pConsole->GetCVar("sv_gamerules")->GetString();

	if (const char* correctGameRules = CCryAction::GetCryAction()->m_pGameRulesSystem->GetGameRulesName(tempGameRules))
		tempGameRules = correctGameRules;

	const char* tempLevel = pConsole->GetCVar("sv_map")->GetString();
	string tempDemoFile;

	ILevelInfo* pLevelInfo = CCryAction::GetCryAction()->m_pLevelSystem->GetLevelInfo(tempLevel);
	if (!pLevelInfo)
	{
		GameWarning("Couldn't find map '%s'", tempLevel);
		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_ERROR, 0, 0);
		return;
	}
	else
	{
#if defined(IS_COMMERCIAL)
		string levelpak = pLevelInfo->GetPath() + string("/level.pak");
		if (!gEnv->pCryPak->OpenPack(levelpak, (unsigned)0))
		{
			GameWarning("Can't open PAK files for this level");
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_ERROR, 0, 0);
			return;
		}

		bool bEncrypted = false;
		bool bHasError = false;

		// force decryption for commercial freeSDK in launcher, editor can open non-encrypted files
		CHECK_ENCRYTPED_COMMERCIAL_LEVEL_PAK(pLevelInfo->GetPath(), bEncrypted, bHasError);

		if ((!bEncrypted || bHasError) && !gEnv->IsEditor())
		{
			gEnv->pCryPak->ClosePack(levelpak, (unsigned)0);
			char dName[256] = { 0, };
			GameWarning("This version of CRYENGINE is licensed to %s and cannot open unencrypted levels", gEnv->pSystem->GetDeveloperName(dName));
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_ERROR, 0, 0);
			return;
		}
		gEnv->pCryPak->ClosePack(levelpak, (unsigned)0);
#endif

		// If the level doesn't support the current game rules then use the level's default game rules
		if (!pLevelInfo->SupportsGameType(tempGameRules))
		{
			if ((gEnv->bMultiplayer == true) || (stricmp(tempGameRules, "SinglePlayer") != 0))
			{
				const char* sDefaultGameRules = pLevelInfo->GetDefaultGameRules();
				if (sDefaultGameRules)
				{
					tempGameRules = sDefaultGameRules;
				}
			}
		}
	}

	CryLogAlways("============================ Loading level %s ============================", currentMapName.c_str());
	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_PREPARE, 0, 0);

	SGameContextParams ctx;
	ctx.gameRules = tempGameRules;
	ctx.levelName = tempLevel;

	// check if we want to run a dedicated server
	bool dedicated = false;
	bool server = false;
	bool forceNewContext = false;

	//if running dedicated network server - default nonblocking mode
	bool blocking = true;
	if (GetCryAction()->StartedGameContext())
	{
		blocking = !::gEnv->IsDedicated();
	}
	//
	ICVar* pImmersive = gEnv->pConsole->GetCVar("g_immersive");
	if (pImmersive && pImmersive->GetIVal() != 0)
	{
		flags |= eGSF_ImmersiveMultiplayer;
	}
	//
	if (args->GetArgCount() > 2)
	{
		CParamCheck paramCheck;
		paramCheck.AddParam("dedicated");
		paramCheck.AddParam("record");
		paramCheck.AddParam("server");
		paramCheck.AddParam("nonblocking"); //If this is set, map load and player connection become non-blocking operations.
		paramCheck.AddParam("nb");          //This flag previously made initial server connection non-blocking. This is now always enabled.
		paramCheck.AddParam("x");
		//
		for (int i = 2; i < args->GetArgCount(); i++)
		{
			string param(args->GetArg(i));
			const string* pArg = paramCheck.GetFullParam(param);
			if (!pArg)
				return;
			const char* arg = pArg->c_str();

			// if 'd' or 'dedicated' is specified as a second argument we are server only
			if (!strcmp(arg, "dedicated") || !strcmp(arg, "d"))
			{
				dedicated = true;
				blocking = false;
			}
			else if (!strcmp(arg, "record"))
			{
				int j = i + 1;
				if (j >= args->GetArgCount())
					continue;
				tempDemoFile = args->GetArg(j);
				i = j;

				ctx.demoRecorderFilename = tempDemoFile.c_str();

				flags |= eGSF_DemoRecorder;
				server = true; // otherwise we will not be able to create more than one GameChannel when starting DemoRecorder
			}
			else if (!strcmp(arg, "server"))
			{
				server = true;
			}
			else if (!strcmp(arg, "nonblocking"))
			{
				blocking = false;
			}
			else if (!strcmp(arg, "nb"))
			{
				//This is always on now - check is preserved for backwards compatibility
			}
			else if (!strcmp(arg, "x"))
			{
				flags |= eGSF_ImmersiveMultiplayer;
			}
			else
			{
				GameWarning("Added parameter %s to paramCheck, but no action performed", arg);
				gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_ERROR, 0, 0);
				return;
			}
		}
	}

#ifndef _RELEASE
	gEnv->pSystem->SetLoadOrigin(ISystem::eLLO_MapCmd);
#endif

	if (blocking)
	{
		flags |= eGSF_BlockingClientConnect | /*eGSF_LocalOnly | eGSF_NoQueries |*/ eGSF_BlockingMapLoad;
		forceNewContext = true;
	}

	const ICVar* pAsyncLoad = gEnv->pConsole->GetCVar("g_asynclevelload");
	const bool bAsyncLoad = pAsyncLoad && pAsyncLoad->GetIVal() > 0;
	if (bAsyncLoad)
	{
		flags |= eGSF_NonBlockingConnect;
	}

	if (::gEnv->IsDedicated())
		dedicated = true;

	if (dedicated || stricmp(ctx.gameRules, "SinglePlayer") != 0)
	{
		//		tempLevel = "Multiplayer/" + tempLevel;
		//		ctx.levelName = tempLevel.c_str();
		server = true;
	}

	bool startedContext = false;
	// if we already have a game context, then we just change it
	if (GetCryAction()->StartedGameContext())
	{
		if (forceNewContext)
			GetCryAction()->EndGameContext();
		else
		{
			GetCryAction()->ChangeGameContext(&ctx);
			startedContext = true;
		}
	}

	if (CCryAction::GetCryAction()->GetIGameSessionHandler() &&
	    !CCryAction::GetCryAction()->GetIGameSessionHandler()->ShouldCallMapCommand(tempLevel, tempGameRules))
	{
		return;
	}

	if (ctx.levelName)
	{
		CCryAction::GetCryAction()->GetILevelSystem()->PrepareNextLevel(ctx.levelName);
	}

	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_mapCmdIssued, 0, currentMapName));

	if (!startedContext)
	{
		CRY_ASSERT(!GetCryAction()->StartedGameContext());

		SGameStartParams params;
		params.flags = flags | eGSF_Server;

		if (!dedicated)
		{
			params.flags |= eGSF_Client;
			params.hostname = "localhost";
		}
		if (server)
		{
			ICVar* max_players = gEnv->pConsole->GetCVar("sv_maxplayers");
			params.maxPlayers = max_players ? max_players->GetIVal() : 16;
			ICVar* loading = gEnv->pConsole->GetCVar("g_enableloadingscreen");
			if (loading)
				loading->Set(0);
			//gEnv->pConsole->GetCVar("g_enableitems")->Set(0);
		}
		else
		{
			params.flags |= eGSF_LocalOnly;
			params.maxPlayers = 1;
		}

		params.pContextParams = &ctx;
		params.port = ChooseListenPort();
		params.session = CCryAction::GetCryAction()->GetIGameSessionHandler()->GetGameSessionHandle();

		if (!CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules())
		{
			params.flags |= (eGSF_NoSpawnPlayer | eGSF_NoGameRules);
		}

		CCryAction::GetCryAction()->StartGameContext(&params);
	}
}

//------------------------------------------------------------------------
void CCryAction::PlayCmd(IConsoleCmdArgs* args)
{
	IConsole* pConsole = gEnv->pConsole;

	if (GetCryAction()->StartedGameContext())
	{
		GameWarning("Must stop the game before commencing playback");
		return;
	}
	if (args->GetArgCount() < 2)
	{
		GameWarning("Usage: \\play demofile");
		return;
	}

	SGameStartParams params;
	SGameContextParams context;

	params.pContextParams = &context;
	context.demoPlaybackFilename = args->GetArg(1);
	params.maxPlayers = 1;
	params.flags = eGSF_Client | eGSF_Server | eGSF_DemoPlayback | eGSF_NoGameRules | eGSF_NoLevelLoading;
	params.port = ChooseListenPort();
	GetCryAction()->StartGameContext(&params);
}

//------------------------------------------------------------------------
void CCryAction::ConnectCmd(IConsoleCmdArgs* args)
{
	if (!gEnv->bServer)
	{
		if (INetChannel* pCh = GetCryAction()->GetClientChannel())
			pCh->Disconnect(eDC_UserRequested, "User left the game");
	}
	GetCryAction()->EndGameContext();

	IConsole* pConsole = gEnv->pConsole;

	// check if a server address was provided
	if (args->GetArgCount() > 1)
	{
		// set cl_serveraddr
		pConsole->GetCVar("cl_serveraddr")->Set(args->GetArg(1));
	}

	// check if a port was provided
	if (args->GetArgCount() > 2)
	{
		// set cl_serverport
		pConsole->GetCVar("cl_serverport")->Set(args->GetArg(2));
	}

	string tempHost = pConsole->GetCVar("cl_serveraddr")->GetString();

	// If <session> isn't specified in the command, try to join the first searchable
	// hosted session at the address specified in cl_serveraddr
	if (tempHost.find("<session>") == tempHost.npos)
	{
		ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;
		if (pMatchMaking)
		{
			CrySessionID session = pMatchMaking->GetSessionIDFromConsole();
			if (session != CrySessionInvalidID)
			{
				GetCryAction()->GetIGameSessionHandler()->JoinSessionFromConsole(session);
				return;
			}
		}
	}

	SGameStartParams params;
	params.flags = eGSF_Client /*| eGSF_BlockingClientConnect*/;
	params.flags |= eGSF_ImmersiveMultiplayer;
	params.hostname = tempHost.c_str();
	params.pContextParams = NULL;
	params.port = (gEnv->pLobby && gEnv->bMultiplayer) ? gEnv->pLobby->GetLobbyParameters().m_connectPort : pConsole->GetCVar("cl_serverport")->GetIVal();

	if (!CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules())
	{
		params.flags |= eGSF_NoGameRules;
	}

	GetCryAction()->StartGameContext(&params);
}

//------------------------------------------------------------------------
void CCryAction::DisconnectCmd(IConsoleCmdArgs* args)
{
	if (gEnv->IsEditor())
		return;

	CCryAction* pCryAction = GetCryAction();

	pCryAction->GetIGameSessionHandler()->OnUserQuit();

	if (!gEnv->bServer)
	{
		if (INetChannel* pCh = pCryAction->GetClientChannel())
			pCh->Disconnect(eDC_UserRequested, "User left the game");
	}

	pCryAction->StartNetworkStallTicker(false);
	pCryAction->EndGameContext();
	pCryAction->StopNetworkStallTicker();

	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_disconnectCommandFinished, 0, "Disconnect Command Done"));
}

//------------------------------------------------------------------------
void CCryAction::DisconnectChannelCmd(IConsoleCmdArgs* args)
{
	CCryAction* pCryAction = GetCryAction();

	if (!gEnv->bServer)
	{
		if (INetChannel* pCh = pCryAction->GetClientChannel())
			pCh->Disconnect(eDC_UserRequested, "User left the game");
	}
}

//------------------------------------------------------------------------
void CCryAction::VersionCmd(IConsoleCmdArgs* args)
{
	CryLogAlways("-----------------------------------------");
	char myVersion[PRODUCT_VERSION_MAX_STRING_LENGTH];
	gEnv->pSystem->GetProductVersion().ToString(myVersion);
	CryLogAlways("Product version: %s", myVersion);
	gEnv->pSystem->GetFileVersion().ToString(myVersion);
	CryLogAlways("File version: %s", myVersion);
	CryLogAlways("-----------------------------------------");
}

//------------------------------------------------------------------------
void CCryAction::StatusCmd(IConsoleCmdArgs* pArgs)
{
	ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;

	if ((pMatchMaking != NULL) && (pArgs->GetArgCount() > 1))
	{
		uint32 mode = atoi(pArgs->GetArg(1));
		pMatchMaking->StatusCmd((eStatusCmdMode)mode);
	}
	else
	{
		LegacyStatusCmd(pArgs);
	}
}

void CCryAction::LegacyStatusCmd(IConsoleCmdArgs* args)
{
	ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;

	CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
	if (!pServerNub)
	{
		if (pMatchMaking != 0)
		{
			pMatchMaking->StatusCmd(eSCM_LegacyRConCompatabilityNoNub);
		}
		else
		{
			CryLogAlways("-----------------------------------------");
			CryLogAlways("Client Status:");
			CryLogAlways("name: %s", "");
			CryLogAlways("ip: %s", gEnv->pNetwork->GetHostName());
			char myVersion[PRODUCT_VERSION_MAX_STRING_LENGTH];
			gEnv->pSystem->GetProductVersion().ToString(myVersion);
			CryLogAlways("version: %s", myVersion);
			CGameClientNub* pClientNub = CCryAction::GetCryAction()->GetGameClientNub();
			if (pClientNub)
			{
				CGameClientChannel* pClientChannel = pClientNub->GetGameClientChannel();
				EntityId entId = pClientChannel->GetPlayerId();
				IActor* pActor = CCryAction::GetCryAction()->m_pActorSystem->GetActor(entId);
				const char* name = "";
				if (pActor)
				{
					name = pActor->GetEntity()->GetName();
				}
				CryLogAlways("name: %s  entID:%d", name, entId);
			}
		}
		return;
	}

	CryLogAlways("-----------------------------------------");
	CryLogAlways("Server Status:");
	CryLogAlways("name: %s", "");
	CryLogAlways("ip: %s", gEnv->pNetwork->GetHostName());
	char myVersion[PRODUCT_VERSION_MAX_STRING_LENGTH];
	gEnv->pSystem->GetProductVersion().ToString(myVersion);
	CryLogAlways("version: %s", myVersion);

	CGameContext* pGameContext = CCryAction::GetCryAction()->m_pGame->GetGameContext();
	if (!pGameContext)
		return;

	CryLogAlways("level: %s", pGameContext->GetLevelName().c_str());
	CryLogAlways("gamerules: %s", pGameContext->GetRequestedGameRules().c_str());
	CryLogAlways("players: %d/%d", pServerNub->GetPlayerCount(), pServerNub->GetMaxPlayers());

	if (IGameRules* pRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules())
		pRules->ShowStatus();

	if (pServerNub->GetPlayerCount() < 1)
		return;

	CryLogAlways("\n-----------------------------------------");
	CryLogAlways("Connection Status:");

	if (pMatchMaking != NULL)
	{
		pMatchMaking->StatusCmd(eSCM_LegacyRConCompatabilityConnectionsOnly);
	}
	else
	{
		TServerChannelMap* pChannelMap = pServerNub->GetServerChannelMap();
		for (TServerChannelMap::iterator iter = pChannelMap->begin(); iter != pChannelMap->end(); ++iter)
		{
			const char* name = "";
			IActor* pActor = CCryAction::GetCryAction()->m_pActorSystem->GetActorByChannelId(iter->first);
			EntityId entId = 0;

			if (pActor)
			{
				name = pActor->GetEntity()->GetName();
				entId = pActor->GetEntity()->GetId();
			}

			INetChannel* pNetChannel = iter->second->GetNetChannel();
			const char* ip = pNetChannel->GetName();
			int ping = (int)(pNetChannel->GetPing(true) * 1000);
			int state = pNetChannel->GetChannelConnectionState();
			int profileId = pNetChannel->GetProfileId();

			CryLogAlways("name: %s  entID:%u id: %u  ip: %s  ping: %d  state: %d profile: %d", name, entId, iter->first, ip, ping, state, profileId);
		}
	}
}

//------------------------------------------------------------------------
void CCryAction::GenStringsSaveGameCmd(IConsoleCmdArgs* pArgs)
{
	CGameSerialize* cgs = GetCryAction()->m_pGameSerialize;
	if (cgs)
	{
		cgs->SaveGame(GetCryAction(), "string-extractor", "SaveGameStrings.cpp", eSGR_Command);
	}
}

//------------------------------------------------------------------------
void CCryAction::SaveGameCmd(IConsoleCmdArgs* args)
{
	if (GetCryAction()->GetLevelName() != nullptr)
	{
		string sSavePath(PathUtil::GetGameFolder());
		if (args->GetArgCount() > 1)
		{
			sSavePath.append("/");
			sSavePath.append(args->GetArg(1));
			CryFixedStringT<64> extension(CRY_SAVEGAME_FILE_EXT);
			extension.Trim('.');
			sSavePath = PathUtil::ReplaceExtension(sSavePath, extension.c_str());
			GetCryAction()->SaveGame(sSavePath, false, false, eSGR_QuickSave, true);
		}
		else
		{
			auto saveGameName = GetCryAction()->CreateSaveGameName();

			GetCryAction()->SaveGame(saveGameName.c_str(), true, false);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Cannot create save game if no level is loaded.");
	}
}

//------------------------------------------------------------------------
void CCryAction::LoadGameCmd(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 1)
	{
		GetCryAction()->NotifyForceFlashLoadingListeners();

		const string path = PathUtil::ReplaceExtension(args->GetArg(1), CRY_SAVEGAME_FILE_EXT);
		const bool quick = args->GetArgCount() > 2;

		GetCryAction()->LoadGame(path.c_str(), quick);
	}
	else
	{
		gEnv->pConsole->ExecuteString("loadLastSave");
	}
}

//------------------------------------------------------------------------
void CCryAction::KickPlayerCmd(IConsoleCmdArgs* pArgs)
{
	ICryLobby* pLobby = gEnv->pNetwork->GetLobby();

	if (pLobby != NULL)
	{
		uint32 cx = ~0;
		uint64 id = 0;
		const char* pName = NULL;

		if (pArgs->GetArgCount() > 1)
		{
			if (_stricmp(pArgs->GetArg(1), "cx") == 0)
			{
				if (pArgs->GetArgCount() == 3)
				{
					cx = atoi(pArgs->GetArg(2));
				}
			}
			else
			{
				if (_stricmp(pArgs->GetArg(1), "id") == 0)
				{
					if (pArgs->GetArgCount() == 3)
					{
						sscanf_s(pArgs->GetArg(2), "%" PRIu64, &id);
					}
				}
				else
				{
					pName = pArgs->GetArg(1);
				}
			}

			ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;
			if (pMatchMaking != NULL)
			{
				pMatchMaking->KickCmd(cx, id, pName, eDC_Kicked);
			}
		}
		else
		{
			CryLogAlways("Usage: kick cx <connection id> | id <profile id> | <name>");
			CryLogAlways("e.g.: kick cx 0, kick id 12345678, kick ElCubo - see 'status'");
		}

	}
	else
	{
		LegacyKickPlayerCmd(pArgs);
	}
}

void CCryAction::LegacyKickPlayerCmd(IConsoleCmdArgs* pArgs)
{
	if (!gEnv->bServer)
	{
		CryLog("This only usable on server");
		return;
	}
	if (pArgs->GetArgCount() > 1)
	{
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
		if (pEntity)
		{
			IActor* pActor = GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId());
			if (pActor && pActor->IsPlayer())
			{
				if (pActor != GetCryAction()->GetClientActor())
					GetCryAction()->GetServerNetNub()->DisconnectPlayer(eDC_Kicked, pActor->GetEntityId(), "Kicked from server");
				else
					CryLog("Cannot kick local player");
			}
			else
				CryLog("%s not a player.", pArgs->GetArg(1));
		}
		else
			CryLog("Player not found");
	}
	else
		CryLog("Usage: kick player_name");
}

void CCryAction::KickPlayerByIdCmd(IConsoleCmdArgs* pArgs)
{
	ICryLobby* pLobby = gEnv->pNetwork->GetLobby();

	if (pLobby != NULL)
	{
		if (pArgs->GetArgCount() > 1)
		{
			uint32 id;
			sscanf_s(pArgs->GetArg(1), "%u", &id);

			ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;
			if (pMatchMaking != NULL)
			{
				pMatchMaking->KickCmd(id, 0, NULL, eDC_Kicked);
			}
		}
		else
		{
			CryLog("Usage: kickid <connection id>");
		}
	}
	else
	{
		LegacyKickPlayerByIdCmd(pArgs);
	}
}

void CCryAction::LegacyKickPlayerByIdCmd(IConsoleCmdArgs* pArgs)
{
	if (!gEnv->bServer)
	{
		CryLog("This only usable on server");
		return;
	}

	if (pArgs->GetArgCount() > 1)
	{
		int id = atoi(pArgs->GetArg(1));

		CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
		if (!pServerNub)
			return;

		TServerChannelMap* pChannelMap = pServerNub->GetServerChannelMap();
		TServerChannelMap::iterator it = pChannelMap->find(id);
		if (it != pChannelMap->end() && (!GetCryAction()->GetClientActor() || GetCryAction()->GetClientActor() != GetCryAction()->m_pActorSystem->GetActorByChannelId(id)))
		{
			it->second->GetNetChannel()->Disconnect(eDC_Kicked, "Kicked from server");
		}
		else
		{
			CryLog("Player with id %d not found", id);
		}
	}
	else
		CryLog("Usage: kickid player_id");
}

void CCryAction::BanPlayerCmd(IConsoleCmdArgs* pArgs)
{

#if CRY_PLATFORM_WINDOWS

	ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;

	if (pMatchMaking != NULL)
	{
		uint64 id = 0;
		const char* pArg;
		char* pEnd;
		float timeout = 0.0f;
		bool usageError = false;

		if (ICVar* pBanTimeout = gEnv->pConsole->GetCVar("ban_timeout"))
		{
			timeout = pBanTimeout->GetFVal();
		}

		switch (pArgs->GetArgCount())
		{
		case 3:

			pArg = pArgs->GetArg(2);
			timeout = static_cast<float>(strtod(pArg, &pEnd));

			if ((pEnd == pArg) || *pEnd)
			{
				usageError = true;
				break;
			}

		// FALL THROUGH

		case 2:

			pArg = pArgs->GetArg(1);
			id = _strtoui64(pArg, &pEnd, 10);

			if ((pEnd == pArg) || *pEnd)
			{
				pMatchMaking->BanCmd(pArg, timeout);
				pMatchMaking->KickCmd(~0, 0, pArg, eDC_Banned);
			}
			else
			{
				pMatchMaking->BanCmd(id, timeout);
				pMatchMaking->KickCmd(~0, id, pArg, eDC_Banned);  //CryLobbyInvalidConnectionID is not accessible outside CryNetwork :(
			}

			break;

		default:

			usageError = true;
			break;
		}

		if (usageError)
		{
			CryLogAlways("Usage: ban <profile id> <minutes>");
			CryLogAlways("       ban <user name> <minutes>");
			CryLogAlways("e.g.: ban 12345678 30");
		}
	}
	else
	{
		LegacyBanPlayerCmd(pArgs);
	}

#endif // #if CRY_PLATFORM_WINDOWS

}

void CCryAction::LegacyBanPlayerCmd(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 1)
	{
		int id = atoi(args->GetArg(1));
		CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
		if (!pServerNub)
			return;
		TServerChannelMap* pChannelMap = pServerNub->GetServerChannelMap();
		for (TServerChannelMap::iterator it = pChannelMap->begin(); it != pChannelMap->end(); ++it)
		{
			if (it->second->GetNetChannel()->GetProfileId() == id)
			{
				pServerNub->BanPlayer(it->first, "Banned from server");
				break;
			}
		}
		CryLog("Player with profileid %d not found", id);
	}
	else
	{
		CryLog("Usage: ban profileid");
	}
}

void CCryAction::BanStatusCmd(IConsoleCmdArgs* pArgs)
{
	ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;

	if (pMatchMaking != NULL)
	{
		pMatchMaking->BanStatus();
	}
	else
	{
		LegacyBanStatusCmd(pArgs);
	}
}

void CCryAction::LegacyBanStatusCmd(IConsoleCmdArgs* args)
{
	if (CCryAction::GetCryAction()->GetGameServerNub())
		CCryAction::GetCryAction()->GetGameServerNub()->BannedStatus();
}

void CCryAction::UnbanPlayerCmd(IConsoleCmdArgs* pArgs)
{

#if CRY_PLATFORM_WINDOWS

	ICryMatchMakingConsoleCommands* pMatchMaking = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingConsoleCommands() : NULL;

	if (pMatchMaking != NULL)
	{
		uint64 id = 0;
		const char* pArg;
		char* pEnd;
		bool usageError = false;

		switch (pArgs->GetArgCount())
		{
		case 2:

			pArg = pArgs->GetArg(1);
			id = _strtoui64(pArg, &pEnd, 10);

			if ((pEnd == pArg) || *pEnd)
			{
				pMatchMaking->UnbanCmd(pArg);
			}
			else
			{
				pMatchMaking->UnbanCmd(id);
			}

			break;

		default:

			usageError = true;
			break;
		}

		if (usageError)
		{
			CryLogAlways("Usage: ban_remove <profile id>");
			CryLogAlways("       ban_remove <user name>");
			CryLogAlways("e.g.: ban_remove 12345678");
		}
	}
	else
	{
		LegacyUnbanPlayerCmd(pArgs);
	}

#endif // #if CRY_PLATFORM_WINDOWS
}

void CCryAction::LegacyUnbanPlayerCmd(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 1)
	{
		int id = atoi(args->GetArg(1));
		CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
		if (!pServerNub)
			return;
		pServerNub->UnbanPlayer(id);
	}
	else
	{
		CryLog("Usage: ban_remove profileid");
	}
}

void CCryAction::OpenURLCmd(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 1)
		GetCryAction()->ShowPageInBrowser(args->GetArg(1));
}

void CCryAction::DumpAnalysisStatsCmd(IConsoleCmdArgs* args)
{
	if (CCryAction::GetCryAction()->m_pGameplayAnalyst)
		CCryAction::GetCryAction()->m_pGameplayAnalyst->DumpToTXT();
}

#if !defined(_RELEASE)
void CCryAction::ConnectRepeatedlyCmd(IConsoleCmdArgs* args)
{
	ConnectCmd(args);

	float timeSeconds = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	IConsole* pConsole = gEnv->pConsole;

	int numAttempts = pConsole->GetCVar("connect_repeatedly_num_attempts")->GetIVal();
	float timeBetweenAttempts = pConsole->GetCVar("connect_repeatedly_time_between_attempts")->GetFVal();

	GetCryAction()->m_connectRepeatedly.m_enabled = true;
	GetCryAction()->m_connectRepeatedly.m_numAttemptsLeft = numAttempts;
	GetCryAction()->m_connectRepeatedly.m_timeForNextAttempt = timeSeconds + timeBetweenAttempts;
	CryLogAlways("CCryAction::ConnectRepeatedlyCmd() numAttempts=%d; timeBetweenAttempts=%f", numAttempts, timeBetweenAttempts);
}
#endif

//------------------------------------------------------------------------
ISimpleHttpServer* CCryAction::s_http_server = NULL;

#define HTTP_DEFAULT_PORT 80

//------------------------------------------------------------------------
void CCryAction::http_startserver(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 3)
	{
		GameWarning("usage: http_startserver [port:<port>] [pass:<pass>] (no spaces or tabs allowed for individule argument)");
		return;
	}

	if (s_http_server)
	{
		GameWarning("HTTP server already started");
		return;
	}

	uint16 port = HTTP_DEFAULT_PORT;
	string http_password;
	int nargs = args->GetArgCount();
	for (int i = 1; i < nargs; ++i)
	{
		string arg(args->GetArg(i));
		string head = arg.substr(0, 5), body = arg.substr(5);
		if (head == "port:")
		{
			int pt = atoi(body);
			if (pt <= 0 || pt > 65535)
				GameWarning("Invalid port specified, default port will be used");
			else
				port = (uint16)pt;
		}
		else if (head == "pass:")
		{
			http_password = body;
		}
		else
		{
			GameWarning("usage: http_startserver [port:<port>] [pass:<pass>] (no spaces or tabs allowed for individule argument)");
			return;
		}
	}

	s_http_server = gEnv->pNetwork->GetSimpleHttpServerSingleton();
	s_http_server->Start(port, http_password, &CSimpleHttpServerListener::GetSingleton(s_http_server));

#if defined(HTTP_WEBSOCKETS)
	s_http_server->AddWebsocketProtocol("WsEcho", &CSimpleHttpServerWebsocketEchoListener::GetSingleton());
#endif
}

//------------------------------------------------------------------------
void CCryAction::http_stopserver(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() != 1)
		GameWarning("usage: http_stopserver (no parameters) - continue anyway ...");

	if (s_http_server == NULL)
	{
		GameWarning("HTTP: server not started");
		return;
	}

	s_http_server->Stop();
	s_http_server = NULL;

	gEnv->pLog->LogToConsole("HTTP: server stopped");
}

//------------------------------------------------------------------------
IRemoteControlServer* CCryAction::s_rcon_server = NULL;
IRemoteControlClient* CCryAction::s_rcon_client = NULL;

CRConClientListener* CCryAction::s_rcon_client_listener = NULL;

//string CCryAction::s_rcon_password;

#define RCON_DEFAULT_PORT 9999

//------------------------------------------------------------------------
//void CCryAction::rcon_password(IConsoleCmdArgs* args)
//{
//	if (args->GetArgCount() > 2)
//	{
//		GameWarning("usage: rcon_password [password] (password cannot contain spaces or tabs");
//		return;
//	}
//
//	if (args->GetArgCount() == 1)
//	{
//		if (s_rcon_password.empty())
//			gEnv->pLog->LogToConsole("RCON system password has not been set");
//		else
//			gEnv->pLog->LogToConsole("RCON system password has been set (not displayed for security reasons)");
//		return;
//	}
//
//	if (args->GetArgCount() == 2)
//	{
//		s_rcon_password = args->GetArg(1);
//	}
//}

//------------------------------------------------------------------------
void CCryAction::rcon_startserver(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 3)
	{
		GameWarning("usage: rcon_startserver [port:<port>] [pass:<pass>] (no spaces or tabs allowed for individule argument)");
		return;
	}

	if (s_rcon_server)
	{
		GameWarning("RCON server already started");
		return;
	}

	uint16 port = RCON_DEFAULT_PORT;
	string rcon_password;

	ICVar* prcon_password = gEnv->pConsole->GetCVar("rcon_password");
	rcon_password = prcon_password->GetString();
	if (rcon_password.empty())
	{
		// if rcon_password not set, use http_password
		ICVar* pcvar = gEnv->pConsole->GetCVar("http_password");
		rcon_password = pcvar->GetString();
	}

	int nargs = args->GetArgCount();
	for (int i = 1; i < nargs; ++i)
	{
		string arg(args->GetArg(i));
		string head = arg.substr(0, 5), body = arg.substr(5);
		if (head == "port:")
		{
			int pt = atoi(body);
			if (pt <= 0 || pt > 65535)
				GameWarning("Invalid port specified, default port will be used");
			else
				port = (uint16)pt;
		}
		else if (head == "pass:")
		{
			rcon_password = body;
			prcon_password->Set(rcon_password.c_str());
		}
		else
		{
			GameWarning("usage: rcon_startserver [port:<port>] [pass:<pass>] (no spaces or tabs allowed for individule argument)");
			return;
		}
	}

	s_rcon_server = gEnv->pNetwork->GetRemoteControlSystemSingleton()->GetServerSingleton();
	s_rcon_server->Start(port, rcon_password, &CRConServerListener::GetSingleton(s_rcon_server));
}

//------------------------------------------------------------------------
void CCryAction::rcon_stopserver(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() != 1)
		GameWarning("usage: rcon_stopserver (no parameters) - continue anyway ...");

	if (s_rcon_server == NULL)
	{
		GameWarning("RCON: server not started");
		return;
	}

	s_rcon_server->Stop();
	s_rcon_server = NULL;
	//s_rcon_password = "";

	gEnv->pLog->LogToConsole("RCON: server stopped");
}

//------------------------------------------------------------------------
void CCryAction::rcon_connect(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 4)
	{
		GameWarning("usage: rcon_connect [addr:<addr>] [port:<port>] [pass:<pass>]");
		return;
	}

	if (s_rcon_client != NULL)
	{
		GameWarning("RCON client already started");
		return;
	}

	uint16 port = RCON_DEFAULT_PORT;
	string addr = "127.0.0.1";
	string password;
	int nargs = args->GetArgCount();
	for (int i = 1; i < nargs; ++i)
	{
		string arg(args->GetArg(i));
		string head = arg.substr(0, 5), body = arg.substr(5);
		if (head == "port:")
		{
			int pt = atoi(body);
			if (pt <= 0 || pt > 65535)
				GameWarning("Invalid port specified, default port will be used");
			else
				port = (uint16)pt;
		}
		else if (head == "pass:")
		{
			password = body;
		}
		else if (head == "addr:")
		{
			addr = body;
		}
		else
		{
			GameWarning("usage: rcon_connect [addr:<addr>] [port:<port>] [pass:<pass>]");
			return;
		}
	}

	s_rcon_client = gEnv->pNetwork->GetRemoteControlSystemSingleton()->GetClientSingleton();
	s_rcon_client_listener = &CRConClientListener::GetSingleton(s_rcon_client);
	s_rcon_client->Connect(addr, port, password, s_rcon_client_listener);
}

//------------------------------------------------------------------------
void CCryAction::rcon_disconnect(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() != 1)
		GameWarning("usage: rcon_disconnect (no parameters) - continue anyway ...");

	if (s_rcon_client == NULL)
	{
		GameWarning("RCON client not started");
		return;
	}

	s_rcon_client->Disconnect();
	s_rcon_client = NULL;
	//s_rcon_password = "";

	gEnv->pLog->LogToConsole("RCON: client disconnected");
}

//------------------------------------------------------------------------
void CCryAction::rcon_command(IConsoleCmdArgs* args)
{
	if (s_rcon_client == NULL || !s_rcon_client_listener->IsSessionAuthorized())
	{
		GameWarning("RCON: cannot issue commands unless the session is authorized");
		return;
	}

	if (args->GetArgCount() < 2)
	{
		GameWarning("usage: rcon_command [console_command arg1 arg2 ...]");
		return;
	}

	string command;
	int nargs = args->GetArgCount();
	for (int i = 1; i < nargs; ++i)
	{
		command += args->GetArg(i);
		command += " "; // a space in between
	}

	uint32 cmdid = s_rcon_client->SendCommand(command);
	if (0 == cmdid)
		GameWarning("RCON: failed sending RCON command %s to server", command.c_str());
	else
		gEnv->pLog->LogToConsole("RCON: command [%08x]%s is sent to server for execution", cmdid, command.c_str());
}

#define EngineStartProfiler(x)
#define InitTerminationCheck(x)

static inline void InlineInitializationProcessing(const char* sDescription)
{
	EngineStartProfiler(sDescription);
	InitTerminationCheck(sDescription);
	gEnv->pLog->UpdateLoadingScreen(0);
}

//------------------------------------------------------------------------
bool CCryAction::Initialize(SSystemInitParams& startupParams)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CryAction Init");

	gEnv->pGameFramework = this;
	m_pSystem = startupParams.pSystem;

	m_pGFListeners = new TGameFrameworkListeners();

	// These vectors must have enough space allocated up-front so as to guarantee no further allocs
	// If they do exceed this capacity, the level heap mechanism should result in a crash
	m_pGFListeners->reserve(20);
	m_validListeners.reserve(m_pGFListeners->capacity());

	ModuleInitISystem(m_pSystem, "CryAction");  // Needed by GetISystem();

	// Flow nodes are registered only when compiled as dynamic library
	CryRegisterFlowNodes();

	LOADING_TIME_PROFILE_SECTION_NAMED("CCryAction::Init() after system");

	InlineInitializationProcessing("CCryAction::Init CrySystem and CryAction init");

	m_pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_action, "CCryAction");

	// init gEnv->pFlashUI

	if (gEnv->pRenderer)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CCryAction::Init() pFlashUI");

		IFlashUIPtr pFlashUI = GetIFlashUIPtr();
		m_pSystem->SetIFlashUI(pFlashUI ? pFlashUI.get() : NULL);
	}

	//#if CRY_PLATFORM_MAC || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	//   gEnv = m_pSystem->GetGlobalEnvironment();
	//#endif

	// fill in interface pointers
	m_pNetwork = gEnv->pNetwork;
	m_p3DEngine = gEnv->p3DEngine;
	m_pScriptSystem = gEnv->pScriptSystem;
	m_pEntitySystem = gEnv->pEntitySystem;
	m_pTimer = gEnv->pTimer;
	m_pLog = gEnv->pLog;

#if defined(USE_CD_KEYS)
	#if CRY_PLATFORM_WINDOWS
	HKEY key;
	DWORD type;
	// Open the appropriate registry key
	LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "", 0,
	                           KEY_READ | KEY_WOW64_32KEY, &key);
	if (ERROR_SUCCESS == result)
	{
		std::vector<char> cdkey(32);

		DWORD size = cdkey.size();

		result = RegQueryValueEx(key, NULL, NULL, &type, (LPBYTE)&cdkey[0], &size);

		if (ERROR_SUCCESS == result && type == REG_SZ)
		{
			m_pNetwork->SetCDKey(&cdkey[0]);
		}
		else
			result = ERROR_BAD_FORMAT;
	}
	if (ERROR_SUCCESS != result)
		GameWarning("Failed to read CDKey from registry");
	#endif
#endif

#ifdef CRYACTION_DEBUG_MEM
	DumpMemInfo("CryAction::Init Start");
#endif

	InitCVars();

	InitGameVolumesManager();

	InlineInitializationProcessing("CCryAction::Init InitCVars");
	if (m_pSystem->IsDevMode())
		m_pDevMode = new CDevMode();

	m_pTimeDemoRecorder = new CTimeDemoRecorder();

	CScriptRMI::RegisterCVars();
	CGameObject::CreateCVars();
	m_pScriptRMI = new CScriptRMI();

	// initialize subsystems
	m_pEffectSystem = new CEffectSystem;
	m_pEffectSystem->Init();
	m_pUIDraw = new CUIDraw;
	m_pLevelSystem = new CLevelSystem(m_pSystem);

	InlineInitializationProcessing("CCryAction::Init CLevelSystem");

	m_pActorSystem = new CActorSystem(m_pSystem, m_pEntitySystem);
	m_pItemSystem = new CItemSystem(this, m_pSystem);
	m_pActionMapManager = new CActionMapManager(gEnv->pInput);

	InlineInitializationProcessing("CCryAction::Init CActionMapManager");

	m_pCooperativeAnimationManager = new CCooperativeAnimationManager;

	m_pViewSystem = new CViewSystem(m_pSystem);
	m_pGameplayRecorder = new CGameplayRecorder(this);
	m_pGameRulesSystem = new CGameRulesSystem(m_pSystem, this);
	m_pVehicleSystem = new CVehicleSystem(m_pSystem, m_pEntitySystem);

	m_pSharedParamsManager = new CSharedParamsManager;

	m_pNetworkCVars = new CNetworkCVars();
	m_pCryActionCVars = new CCryActionCVars();

	if (m_pCryActionCVars->g_gameplayAnalyst)
		m_pGameplayAnalyst = new CGameplayAnalyst();

	InlineInitializationProcessing("CCryAction::Init CGameplayAnalyst");
	m_pGameObjectSystem = new CGameObjectSystem();
	if (!m_pGameObjectSystem->Init())
		return false;
	else
	{
		// init game object events of CryAction
		m_pGameObjectSystem->RegisterEvent(eGFE_PauseGame, "PauseGame");
		m_pGameObjectSystem->RegisterEvent(eGFE_ResumeGame, "ResumeGame");
		m_pGameObjectSystem->RegisterEvent(eGFE_OnCollision, "OnCollision");
		m_pGameObjectSystem->RegisterEvent(eGFE_OnPostStep, "OnPostStep");
		m_pGameObjectSystem->RegisterEvent(eGFE_OnStateChange, "OnStateChange");
		m_pGameObjectSystem->RegisterEvent(eGFE_ResetAnimationGraphs, "ResetAnimationGraphs");
		m_pGameObjectSystem->RegisterEvent(eGFE_OnBreakable2d, "OnBreakable2d");
		m_pGameObjectSystem->RegisterEvent(eGFE_OnBecomeVisible, "OnBecomeVisible");
		m_pGameObjectSystem->RegisterEvent(eGFE_PreShatter, "PreShatter");
		m_pGameObjectSystem->RegisterEvent(eGFE_DisablePhysics, "DisablePhysics");
		m_pGameObjectSystem->RegisterEvent(eGFE_EnablePhysics, "EnablePhysics");
		m_pGameObjectSystem->RegisterEvent(eGFE_ScriptEvent, "ScriptEvent");
		m_pGameObjectSystem->RegisterEvent(eGFE_QueueRagdollCreation, "QueueRagdollCreation");
		m_pGameObjectSystem->RegisterEvent(eGFE_RagdollPhysicalized, "RagdollPhysicalized");
		m_pGameObjectSystem->RegisterEvent(eGFE_StoodOnChange, "StoodOnChange");
		m_pGameObjectSystem->RegisterEvent(eGFE_EnableBlendRagdoll, "EnableBlendToRagdoll");
		m_pGameObjectSystem->RegisterEvent(eGFE_DisableBlendRagdoll, "DisableBlendToRagdoll");
	}

	m_pAnimationGraphCvars = new CAnimationGraphCVars();
	m_pMannequin = new CMannequinInterface();
	if (gEnv->IsEditor())
		m_pCallbackTimer = new CallbackTimer();
	m_pPersistantDebug = new CPersistantDebug();
	m_pPersistantDebug->Init();
#if !CRY_PLATFORM_DESKTOP
	#if PROFILE_CONSOLE_NO_SAVE
	// Used for demos
	m_pPlayerProfileManager = new CPlayerProfileManager(new CPlayerProfileImplNoSave());
	#else
		#if CRY_PLATFORM_DURANGO
	m_pPlayerProfileManager = new CPlayerProfileManager(new CPlayerProfileImplDurango());
		#elif CRY_PLATFORM_ORBIS
	m_pPlayerProfileManager = new CPlayerProfileManager(new CPlayerProfileImplOrbis());
		#else
	m_pPlayerProfileManager = new CPlayerProfileManager(new CPlayerProfileImplConsole());
		#endif
	#endif
#else
	m_pPlayerProfileManager = new CPlayerProfileManager(new CPlayerProfileImplFSDir());
#endif
	m_pDialogSystem = new CDialogSystem();
	m_pDialogSystem->Init();

	m_pTimeOfDayScheduler = new CTimeOfDayScheduler();
	m_pSubtitleManager = new CSubtitleManager();

	m_pCustomActionManager = new CCustomActionManager();
	m_pCustomEventManager = new CCustomEventManager();

	CRangeSignaling::Create();
	CSignalTimer::Create();

	IMovieSystem* movieSys = gEnv->pMovieSystem;
	if (movieSys != NULL)
		movieSys->SetUser(m_pViewSystem);

	if (m_pVehicleSystem)
	{
		m_pVehicleSystem->Init();
	}

	REGISTER_FACTORY((IGameFramework*)this, "Inventory", CInventory, false);

	if (m_pLevelSystem && m_pItemSystem)
	{
		m_pLevelSystem->AddListener(m_pItemSystem);
	}

	InitScriptBinds();

	///Disabled as we now use the communication manager exclusively for readabilities
	//CAIHandler::s_ReadabilityManager.Reload();
	CAIFaceManager::LoadStatic();

	// m_pGameRulesSystem = new CGameRulesSystem(m_pSystem, this);

	m_pLocalAllocs = new SLocalAllocs();

#if 0
	BeginLanQuery();
#endif

	// Player profile stuff
	if (m_pPlayerProfileManager)
	{
		bool ok = m_pPlayerProfileManager->Initialize();
		if (!ok)
			GameWarning("[PlayerProfiles] CCryAction::Init: Cannot initialize PlayerProfileManager");
	}

#ifdef CRYACTION_DEBUG_MEM
	DumpMemInfo("CryAction::Init End");
#endif

	m_nextFrameCommand = new string();

	m_pGameStatsConfig = new CGameStatsConfig();
	m_pGameStatsConfig->ReadConfig();

	//	InlineInitializationProcessing("CCryAction::Init m_pCharacterPartsManager");

	m_pGameStatistics = new CGameStatistics();

	if (gEnv->pAISystem)
	{
		m_pAIDebugRenderer = new CAIDebugRenderer(gEnv->pRenderer);
		gEnv->pAISystem->SetAIDebugRenderer(m_pAIDebugRenderer);

		m_pAINetworkDebugRenderer = new CAINetworkDebugRenderer(gEnv->pRenderer);
		gEnv->pAISystem->SetAINetworkDebugRenderer(m_pAINetworkDebugRenderer);

		RegisterActionBehaviorTreeNodes();
	}

	if (gEnv->IsEditor())
		CreatePhysicsQueues();

	XMLCPB::CDebugUtils::Create();

	if (!gEnv->IsDedicated())
	{
		XMLCPB::InitializeCompressorThread();
	}

	m_pNetMsgDispatcher = new CNetMessageDistpatcher();
	m_pEntityContainerMgr = new CEntityContainerMgr();
	m_pEntityAttachmentExNodeRegistry = new CEntityAttachmentExNodeRegistry();
	
	if (gEnv->pRenderer)
	{
		m_pColorGradientManager = new CColorGradientManager();
	}

	InitGame(startupParams);

	if (m_pVehicleSystem)
		m_pVehicleSystem->RegisterVehicles(this);
	if (m_pGameObjectSystem)
		m_pGameObjectSystem->RegisterFactories(this);
	CGameContext::RegisterExtensions(this);

	if (startupParams.bExecuteCommandLine)
		GetISystem()->ExecuteCommandLine();

	// game got initialized, time to finalize framework initialization
	if (CompleteInit())
	{
		InlineInitializationProcessing("CCryAction::Init End");

		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_FRAMEWORK_INIT_DONE, 0, 0);

		gEnv->pConsole->ExecuteString("exec autoexec.cfg");

		return true;
	}

	return false;
}

bool CCryAction::InitGame(SSystemInitParams& startupParams)
{
	LOADING_TIME_PROFILE_SECTION;
	if (ICVar* pCVarGameDir = gEnv->pConsole->GetCVar("sys_dll_game"))
	{
		const char* gameDLLName = pCVarGameDir->GetString();
		if (strlen(gameDLLName) == 0)
			return false;

		HMODULE hGameDll = 0;

#if !defined(_LIB)
		hGameDll = CryLoadLibrary(gameDLLName);

		if (!hGameDll)
		{
			// workaround to make the legacy game work with the new project system where the dll is in a separate folder
			char executableFolder[MAX_PATH];
			char engineRootFolder[MAX_PATH];
			CryGetExecutableFolder(MAX_PATH, executableFolder);
			CryFindEngineRootFolder(MAX_PATH, engineRootFolder);

			string newGameDLLPath = string(executableFolder).erase(0, strlen(engineRootFolder));

			newGameDLLPath += gameDLLName;

			hGameDll = CryLoadLibrary(newGameDLLPath.c_str());

			if (!hGameDll)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Failed to load the Game DLL! %s", gameDLLName);
				return false;
			}
		}

		IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(hGameDll, "CreateGameStartup");
		if (!CreateGameStartup)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Failed to find the GameStartup Interface in %s!", gameDLLName);
			CryFreeLibrary(hGameDll);
			return false;
		}
#endif

#if !defined(_LIB) || !defined(DISABLE_LEGACY_GAME_DLL)
		// create the game startup interface
		IGameStartup* pGameStartup = CreateGameStartup();
		if (!pGameStartup)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Failed to find the GameStartup Interface in %s!", gameDLLName);
			CryFreeLibrary(hGameDll);
			return false;
		}

		if (m_externalGameLibrary.pGame = pGameStartup->Init(startupParams))
		{
			m_externalGameLibrary.dllName = gameDLLName;
			m_externalGameLibrary.dllHandle = hGameDll;
			m_externalGameLibrary.pGameStartup = pGameStartup;
		}
#endif
	}

	return m_externalGameLibrary.IsValid();
}

//------------------------------------------------------------------------
void CCryAction::InitForceFeedbackSystem()
{
	LOADING_TIME_PROFILE_SECTION;
	SAFE_DELETE(m_pForceFeedBackSystem);
	m_pForceFeedBackSystem = new CForceFeedBackSystem();
	m_pForceFeedBackSystem->Initialize();
}

//------------------------------------------------------------------------

void CCryAction::InitGameVolumesManager()
{
	LOADING_TIME_PROFILE_SECTION;

	if (m_pGameVolumesManager == NULL)
	{
		m_pGameVolumesManager = new CGameVolumesManager();
	}
}

//------------------------------------------------------------------------
void CCryAction::InitGameType(bool multiplayer, bool fromInit)
{
	gEnv->pSystem->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_RUNNING);
	SAFE_DELETE(m_pGameSerialize);

	InitForceFeedbackSystem();

#if !defined(DEDICATED_SERVER)
	if (!multiplayer)
	{
		m_pGameSerialize = new CGameSerialize();

		if (m_pGameSerialize)
			m_pGameSerialize->RegisterFactories(this);
	}

	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if (!multiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		if (!m_pAIProxyManager)
		{
			m_pAIProxyManager = new CAIProxyManager;
			m_pAIProxyManager->Init();
		}
	}
	else
#endif
	{
		if (m_pAIProxyManager)
		{
			m_pAIProxyManager->Shutdown();
			SAFE_DELETE(m_pAIProxyManager);
		}
	}
}

static std::vector<const char*> gs_lipSyncExtensionNamesForExposureToEditor;

//------------------------------------------------------------------------
bool CCryAction::CompleteInit()
{
	LOADING_TIME_PROFILE_SECTION;
#ifdef CRYACTION_DEBUG_MEM
	DumpMemInfo("CryAction::CompleteInit Start");
#endif

	InlineInitializationProcessing("CCryAction::CompleteInit");

	REGISTER_FACTORY((IGameFramework*)this, "AnimatedCharacter", CAnimatedCharacter, false);
	REGISTER_FACTORY((IGameFramework*)this, "LipSync_TransitionQueue", CLipSync_TransitionQueue, false);
	REGISTER_FACTORY((IGameFramework*)this, "LipSync_FacialInstance", CLipSync_FacialInstance, false);
	gs_lipSyncExtensionNamesForExposureToEditor.clear();
	gs_lipSyncExtensionNamesForExposureToEditor.push_back("LipSync_TransitionQueue");
	gs_lipSyncExtensionNamesForExposureToEditor.push_back("LipSync_FacialInstance");

	EndGameContext();

	// init IFlashUI extension
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->Init();

	m_pSystem->SetIDialogSystem(m_pDialogSystem);

	InlineInitializationProcessing("CCryAction::CompleteInit SetDialogSystem");

	if (m_pGameplayAnalyst)
		m_pGameplayRecorder->RegisterListener(m_pGameplayAnalyst);

	CRangeSignaling::ref().Init();
	CSignalTimer::ref().Init();
	// ---------------------------

	m_pMaterialEffects = new CMaterialEffects();
	m_pScriptBindMFX = new CScriptBind_MaterialEffects(m_pSystem, m_pMaterialEffects);
	m_pSystem->SetIMaterialEffects(m_pMaterialEffects);

	InlineInitializationProcessing("CCryAction::CompleteInit MaterialEffects");

	m_pBreakableGlassSystem = new CBreakableGlassSystem();

	InitForceFeedbackSystem();

	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if (!gEnv->bMultiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		m_pAIProxyManager = new CAIProxyManager;
		m_pAIProxyManager->Init();
	}

	// in pure game mode we load the equipment packs from disk
	// in editor mode, this is done in GameEngine.cpp
	if ((m_pItemSystem) && (gEnv->IsEditor() == false))
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CCryAction::CompleteInit(): EquipmentPacks");
		m_pItemSystem->GetIEquipmentManager()->DeleteAllEquipmentPacks();
		m_pItemSystem->GetIEquipmentManager()->LoadEquipmentPacksFromPath("Libs/EquipmentPacks");
	}

	InlineInitializationProcessing("CCryAction::CompleteInit EquipmentPacks");

	gEnv->p3DEngine->GetMaterialManager()->GetDefaultLayersMaterial();

	if (auto* pGame = CCryAction::GetCryAction()->GetIGame())
	{
		pGame->CompleteInit();
	}

	InlineInitializationProcessing("CCryAction::CompleteInit LoadFlowGraphLibs");

	// load flowgraphs (done after Game has initialized, because it might add additional flownodes)
	if (m_pMaterialEffects)
		m_pMaterialEffects->LoadFlowGraphLibs();

	if (!m_pScriptRMI->Init())
		return false;

	if (gEnv->pFlashUI)
		gEnv->pFlashUI->PostInit();
	InlineInitializationProcessing("CCryAction::CompleteInit FlashUI");

	// after everything is initialized, run our main script
	m_pScriptSystem->ExecuteFile("scripts/main.lua");
	m_pScriptSystem->BeginCall("OnInit");
	m_pScriptSystem->EndCall();

	InlineInitializationProcessing("CCryAction::CompleteInit RunMainScript");

#ifdef CRYACTION_DEBUG_MEM
	DumpMemInfo("CryAction::CompleteInit End");
#endif

	CBreakReplicator::RegisterClasses();

	if (gEnv->pRenderer)
	{
		gEnv->pRenderer->StopRenderIntroMovies(true);
	}

	m_pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_POST_INIT, 0, 0);
	m_pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_POST_INIT_DONE, 0, 0);

	if (gEnv->pMaterialEffects)
	{
		gEnv->pMaterialEffects->CompleteInit();
	}

	if (gEnv->pConsole->GetCVar("g_enableMergedMeshRuntimeAreas")->GetIVal() > 0)
	{
		m_pRuntimeAreaManager = new CRuntimeAreaManager();
	}

#if defined(CRY_UNIT_TESTING)
	if (gEnv->bTesting)
	{
		//in local unit tests we pass in -unit_test_open_failed to notify the user, in automated tests we don't pass in.
		CryUnitTest::EReporterType reporterType = m_pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "unit_test_open_failed") ? 
			CryUnitTest::EReporterType::ExcelWithNotification : CryUnitTest::EReporterType::Excel;
		ITestSystem* pTestSystem = m_pSystem->GetITestSystem();
		CRY_ASSERT(pTestSystem != nullptr);
		pTestSystem->GetIUnitTestManager()->RunAllTests(reporterType);
		pTestSystem->QuitInNSeconds(1.f);
	}
#endif

	InitCommands();

	InlineInitializationProcessing("CCryAction::CompleteInit End");
	return true;
}

//------------------------------------------------------------------------
void CCryAction::InitScriptBinds()
{
	LOADING_TIME_PROFILE_SECTION;

	m_pScriptNet = new CScriptBind_Network(m_pSystem, this);
	m_pScriptA = new CScriptBind_Action(this);
	m_pScriptIS = new CScriptBind_ItemSystem(m_pSystem, m_pItemSystem, this);
	m_pScriptAS = new CScriptBind_ActorSystem(m_pSystem, this);
	m_pScriptAMM = new CScriptBind_ActionMapManager(m_pSystem, m_pActionMapManager);

	m_pScriptVS = new CScriptBind_VehicleSystem(m_pSystem, m_pVehicleSystem);
	m_pScriptBindVehicle = new CScriptBind_Vehicle(m_pSystem, this);
	m_pScriptBindVehicleSeat = new CScriptBind_VehicleSeat(m_pSystem, this);

	m_pScriptInventory = new CScriptBind_Inventory(m_pSystem, this);
	m_pScriptBindDS = new CScriptBind_DialogSystem(m_pSystem, m_pDialogSystem);
	m_pScriptBindUIAction = new CScriptBind_UIAction();
}

//------------------------------------------------------------------------
void CCryAction::ReleaseScriptBinds()
{
	// before we release script binds call out main "OnShutdown"
	if (m_pScriptSystem)
	{
		m_pScriptSystem->BeginCall("OnShutdown");
		m_pScriptSystem->EndCall();
	}

	SAFE_RELEASE(m_pScriptA);
	SAFE_RELEASE(m_pScriptIS);
	SAFE_RELEASE(m_pScriptAS);
	SAFE_RELEASE(m_pScriptAMM);
	SAFE_RELEASE(m_pScriptVS);
	SAFE_RELEASE(m_pScriptNet);
	SAFE_RELEASE(m_pScriptBindVehicle);
	SAFE_RELEASE(m_pScriptBindVehicleSeat);
	SAFE_RELEASE(m_pScriptInventory);
	SAFE_RELEASE(m_pScriptBindDS);
	SAFE_RELEASE(m_pScriptBindMFX);
	SAFE_RELEASE(m_pScriptBindUIAction);
}

//------------------------------------------------------------------------
bool CCryAction::ShutdownGame()
{
	// unload game dll if present
	if (m_externalGameLibrary.IsValid())
	{
		IFlowGraphModuleManager* pFlowGraphModuleManager = gEnv->pFlowSystem->GetIModuleManager();
		if (pFlowGraphModuleManager)
		{
			pFlowGraphModuleManager->ClearModules();
		}

		IMaterialEffects* pMaterialEffects = GetIMaterialEffects();
		if (pMaterialEffects)
		{
			pMaterialEffects->Reset(true);
		}

		IFlashUI* pFlashUI = gEnv->pFlashUI;
		if (pFlashUI)
		{
			pFlashUI->ClearUIActions();
		}

		m_externalGameLibrary.pGameStartup->Shutdown();
		if (m_externalGameLibrary.dllHandle)
		{
			CryFreeLibrary(m_externalGameLibrary.dllHandle);
		}
		m_externalGameLibrary.Reset();
	}

	return true;
}

//------------------------------------------------------------------------
void CCryAction::ShutDown()
{
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_FRAMEWORK_ABOUT_TO_SHUTDOWN, 0, 0);

	ShutdownGame();

#ifndef _LIB
	// Flow nodes are registered/unregistered only when compiled as dynamic library
	CryUnregisterFlowNodes();
#endif

	XMLCPB::ShutdownCompressorThread();

	SAFE_DELETE(m_pAIDebugRenderer);
	SAFE_DELETE(m_pAINetworkDebugRenderer);

	if (s_rcon_server)
		s_rcon_server->Stop();
	if (s_rcon_client)
		s_rcon_client->Disconnect();
	s_rcon_server = NULL;
	s_rcon_client = NULL;

	if (s_http_server)
		s_http_server->Stop();
	s_http_server = NULL;

	if (m_pGameplayAnalyst)
		m_pGameplayRecorder->UnregisterListener(m_pGameplayAnalyst);

	if (gEnv)
	{
		EndGameContext();
	}

	if (m_pEntitySystem)
	{
		m_pEntitySystem->Unload();
	}

	if (gEnv)
	{
		IMovieSystem* movieSys = gEnv->pMovieSystem;
		if (movieSys != NULL)
			movieSys->SetUser(NULL);
	}

	// profile manager needs to shut down (logout users, ...)
	// while most systems are still up
	if (m_pPlayerProfileManager)
		m_pPlayerProfileManager->Shutdown();

	if (m_pDialogSystem)
		m_pDialogSystem->Shutdown();

	SAFE_RELEASE(m_pActionMapManager);
	SAFE_RELEASE(m_pItemSystem);
	SAFE_RELEASE(m_pLevelSystem);
	SAFE_RELEASE(m_pViewSystem);
	SAFE_RELEASE(m_pGameplayRecorder);
	SAFE_RELEASE(m_pGameplayAnalyst);
	SAFE_RELEASE(m_pGameRulesSystem);
	SAFE_RELEASE(m_pSharedParamsManager);
	SAFE_RELEASE(m_pVehicleSystem);
	SAFE_DELETE(m_pMaterialEffects);
	SAFE_DELETE(m_pBreakableGlassSystem);
	SAFE_RELEASE(m_pActorSystem);
	SAFE_DELETE(m_pForceFeedBackSystem);
	SAFE_DELETE(m_pSubtitleManager);
	SAFE_DELETE(m_pUIDraw);
	SAFE_DELETE(m_pScriptRMI);
	SAFE_DELETE(m_pEffectSystem);
	SAFE_DELETE(m_pAnimationGraphCvars);
	SAFE_DELETE(m_pGameObjectSystem);
	SAFE_DELETE(m_pMannequin);
	SAFE_DELETE(m_pTimeDemoRecorder);
	SAFE_DELETE(m_pGameSerialize);
	SAFE_DELETE(m_pPersistantDebug);
	SAFE_DELETE(m_pPlayerProfileManager);
	SAFE_DELETE(m_pDialogSystem); // maybe delete before
	SAFE_DELETE(m_pTimeOfDayScheduler);
	SAFE_DELETE(m_pLocalAllocs);
	SAFE_DELETE(m_pCooperativeAnimationManager);
	SAFE_DELETE(m_pGameSessionHandler);
	SAFE_DELETE(m_pCustomEventManager)
	SAFE_DELETE(m_pCustomActionManager)

	SAFE_DELETE(m_pGameVolumesManager);

	SAFE_DELETE(m_pRuntimeAreaManager);

	ReleaseScriptBinds();
	ReleaseCVars();

	SAFE_DELETE(m_pColorGradientManager);

	SAFE_DELETE(m_pDevMode);
	SAFE_DELETE(m_pCallbackTimer);

	CSignalTimer::Shutdown();
	CRangeSignaling::Shutdown();

	if (m_pAIProxyManager)
	{
		m_pAIProxyManager->Shutdown();
		SAFE_DELETE(m_pAIProxyManager);
	}

	SAFE_DELETE(m_pNetMsgDispatcher);
	SAFE_DELETE(m_pEntityContainerMgr);
	SAFE_DELETE(m_pEntityAttachmentExNodeRegistry);

	ReleaseExtensions();

	// Shutdown pFlashUI after shutdown since FlowSystem will hold UIFlowNodes until deletion
	// this will allow clean dtor for all UIFlowNodes
	if (gEnv && gEnv->pFlashUI)
		gEnv->pFlashUI->Shutdown();

	SAFE_DELETE(m_pGFListeners);

	XMLCPB::CDebugUtils::Destroy();

	if (m_pSystem)
	{
		m_pSystem->GetISystemEventDispatcher()->RemoveListener(&g_system_event_listener_action);
	}

	SAFE_DELETE(m_nextFrameCommand);
	SAFE_DELETE(m_pPhysicsQueues);

	m_pThis = nullptr;
}

//------------------------------------------------------------------------
void CCryAction::FastShutdown()
{
	IForceFeedbackSystem* pForceFeedbackSystem = GetIForceFeedbackSystem();
	if (pForceFeedbackSystem)
	{
		CryLogAlways("ActionGame - Shutting down all force feedback");
		pForceFeedbackSystem->StopAllEffects();
	}

	//Ensure that the load ticker is not currently running. It can perform tasks on the network systems while they're being shut down
	StopNetworkStallTicker();

	ShutdownGame();
}

//------------------------------------------------------------------------
void CCryAction::PrePhysicsUpdate()
{
	if (auto* pGame = GetIGame())
	{
		pGame->PrePhysicsUpdate();  // required if PrePhysicsUpdate() is overriden in game
	}
}

void CCryAction::PreSystemUpdate()
{
	CRY_PROFILE_REGION(PROFILE_GAME, "CCryAction::PreRenderUpdate");
	CRYPROFILE_SCOPE_PROFILE_MARKER("CCryAction::PreRenderUpdate");

	if (!m_nextFrameCommand->empty())
	{
		gEnv->pConsole->ExecuteString(*m_nextFrameCommand);
		m_nextFrameCommand->resize(0);
	}

	CheckEndLevelSchedule();

#if !defined(_RELEASE)
	CheckConnectRepeatedly();   // handle repeated connect mode - mainly for autotests to not get broken by timeouts on initial connect
#endif

	bool bGameIsPaused = !IsGameStarted() || IsGamePaused(); // slightly different from m_paused (check's gEnv->pTimer as well)
	if (m_pTimeDemoRecorder && !IsGamePaused())
		m_pTimeDemoRecorder->PreUpdate();

	// update the callback system
	if (!bGameIsPaused)
	{
		if (m_pCallbackTimer)
			m_pCallbackTimer->Update();
	}
}

uint32 CCryAction::GetPreUpdateTicks()
{
	uint32 ticks = m_PreUpdateTicks;
	m_PreUpdateTicks = 0;
	return ticks;
}

bool CCryAction::PostSystemUpdate(bool haveFocus, CEnumFlags<ESystemUpdateFlags> updateFlags)
{
	CRY_PROFILE_REGION(PROFILE_GAME, "CCryAction::PostSystemUpdate");
	CRYPROFILE_SCOPE_PROFILE_MARKER("CCryAction::PostSystemUpdate");

	float frameTime = gEnv->pTimer->GetFrameTime();

	LARGE_INTEGER updateStart, updateEnd;
	updateStart.QuadPart = 0;
	updateEnd.QuadPart = 0;

	bool isGamePaused = !IsGameStarted() || IsGamePaused(); // slightly different from m_paused (check's gEnv->pTimer as well)
	bool isGameRunning = IsGameStarted();

	// when we are updated by the editor, we should not update the system
	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		const bool wasGamePaused = isGamePaused;

#ifdef ENABLE_LW_PROFILERS
		CRY_PROFILE_SECTION(PROFILE_ACTION, "ActionPreUpdate");
		QueryPerformanceCounter(&updateStart);
#endif
		// notify listeners
		OnActionEvent(SActionEvent(eAE_earlyPreUpdate));

		// during m_pSystem->Update call the Game might have been paused or un-paused
		isGameRunning = IsGameStarted() && m_pGame && m_pGame->IsInited();
		isGamePaused = !isGameRunning || IsGamePaused();

		if (!isGamePaused && !wasGamePaused) // don't update gameplayrecorder if paused
			if (m_pGameplayRecorder)
				m_pGameplayRecorder->Update(frameTime);

		{
			CDebugHistoryManager::RenderAll();
			CCryAction::GetCryAction()->GetTimeOfDayScheduler()->Update();
		}

		if (!isGamePaused && isGameRunning)
		{
			if (gEnv->pFlowSystem)
			{
				gEnv->pFlowSystem->Update();
			}
		}

		if (gEnv->pFlashUI)
		{
			gEnv->pFlashUI->UpdateFG();
		}

		gEnv->pMovieSystem->ControlCapture();

		// if the Game had been paused and is now unpaused, don't update viewsystem until next frame
		// if the Game had been unpaused and is now paused, don't update viewsystem until next frame
		if (m_pViewSystem)
		{
			const bool useDeferredViewSystemUpdate = m_pViewSystem->UseDeferredViewSystemUpdate();
			if (!useDeferredViewSystemUpdate)
			{
				if (!isGamePaused && !wasGamePaused) // don't update view if paused
					m_pViewSystem->Update(min(frameTime, 0.1f));
			}
		}
	}

	// These things need to be updated in game mode and ai/physics mode
	m_pPersistantDebug->Update(gEnv->pTimer->GetFrameTime());

	m_pActionMapManager->Update();

	m_pForceFeedBackSystem->Update(frameTime);

	if (m_pPhysicsQueues)
	{
		m_pPhysicsQueues->Update(frameTime);
	}

	if (!isGamePaused)
	{
		if (m_pItemSystem)
			m_pItemSystem->Update();

		if (m_pMaterialEffects)
			m_pMaterialEffects->Update(frameTime);

		if (m_pBreakableGlassSystem)
			m_pBreakableGlassSystem->Update(frameTime);

		if (m_pDialogSystem)
			m_pDialogSystem->Update(frameTime);

		if (m_pVehicleSystem)
			m_pVehicleSystem->Update(frameTime);

		if (m_pCooperativeAnimationManager)
			m_pCooperativeAnimationManager->Update(frameTime);
	}

	if (gEnv->pRenderer)
	{
		m_pColorGradientManager->UpdateForThisFrame(gEnv->pTimer->GetFrameTime());
	}

	CRConServerListener::GetSingleton().Update();
	CSimpleHttpServerListener::GetSingleton().Update();

#if !defined(_RELEASE)
	CRealtimeRemoteUpdateListener::GetRealtimeRemoteUpdateListener().Update();
#endif //_RELEASE

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
#ifdef ENABLE_LW_PROFILERS
		QueryPerformanceCounter(&updateEnd);
		m_PreUpdateTicks += (uint32)(updateEnd.QuadPart - updateStart.QuadPart);
#endif
	}

	bool continueRunning = true;

	if (auto* pGame = CCryAction::GetCryAction()->GetIGame())
	{
		CRY_PROFILE_REGION(PROFILE_GAME, "UpdateLegacyGame");
		continueRunning = pGame->Update(haveFocus, updateFlags.UnderlyingValue()) > 0;
	}

	if (!updateFlags.Check(ESYSUPDATE_EDITOR_ONLY)  && updateFlags.Check(ESYSUPDATE_EDITOR_AI_PHYSICS) && !gEnv->bMultiplayer)
	{
		CRangeSignaling::ref().SetDebug(m_pDebugRangeSignaling->GetIVal() == 1);
		CRangeSignaling::ref().Update(frameTime);

		CSignalTimer::ref().SetDebug(m_pDebugSignalTimers->GetIVal() == 1);
		CSignalTimer::ref().Update(frameTime);
	}

	return continueRunning;
}

void CCryAction::PreFinalizeCamera(CEnumFlags<ESystemUpdateFlags> updateFlags)
{
	CRY_PROFILE_REGION(PROFILE_GAME, "CCryAction::PreFinalizeCamera");
	CRYPROFILE_SCOPE_PROFILE_MARKER("CCryAction::PreFinalizeCamera");

	if (m_pShowLanBrowserCVAR->GetIVal() == 0)
	{
		if (m_bShowLanBrowser)
		{
			m_bShowLanBrowser = false;
			EndCurrentQuery();
			m_pLanQueryListener = 0;
		}
	}
	else
	{
		if (!m_bShowLanBrowser)
		{
			m_bShowLanBrowser = true;
			BeginLanQuery();
		}
	}

	float delta = gEnv->pTimer->GetFrameTime();
	const bool bGameIsPaused = IsGamePaused(); // slightly different from m_paused (check's gEnv->pTimer as well)

	if (!bGameIsPaused)
	{
		if (m_pEffectSystem)
			m_pEffectSystem->Update(delta);
	}

	//update view system before p3DEngine->PrepareOcclusion as it might change view camera
	const bool useDeferredViewSystemUpdate = m_pViewSystem->UseDeferredViewSystemUpdate();
	if (useDeferredViewSystemUpdate)
	{
		m_pViewSystem->Update(min(delta, 0.1f));
	}
}

void CCryAction::PreRender()
{
	CRY_PROFILE_REGION(PROFILE_GAME, "CCryAction::PreRender");
	CRYPROFILE_SCOPE_PROFILE_MARKER("CCryAction::PreRender");

	CALL_FRAMEWORK_LISTENERS(OnPreRender());
}

void CCryAction::PostRender(CEnumFlags<ESystemUpdateFlags> updateFlags)
{
	CRY_PROFILE_REGION(PROFILE_GAME, "CCryAction::PostRender");
	CRYPROFILE_SCOPE_PROFILE_MARKER("CCryAction::PostRender");

	if (updateFlags & ESYSUPDATE_EDITOR_AI_PHYSICS)
	{
		float frameTime = gEnv->pTimer->GetFrameTime();

		if (m_pPersistantDebug)
			m_pPersistantDebug->PostUpdate(frameTime);

		if (m_pGameObjectSystem)
			m_pGameObjectSystem->PostUpdate(frameTime);

		return;
	}

	float delta = gEnv->pTimer->GetFrameTime();

	if (gEnv->pRenderer && m_pPersistantDebug)
		m_pPersistantDebug->PostUpdate(delta);

	CALL_FRAMEWORK_LISTENERS(OnPostUpdate(delta));

	const float now = gEnv->pTimer->GetCurrTime(ITimer::ETIMER_UI);
	float deltaUI = now - m_lastFrameTimeUI;
	m_lastFrameTimeUI = now;

	if (m_lastSaveLoad)
	{
		m_lastSaveLoad -= deltaUI;
		if (m_lastSaveLoad < 0.0f)
			m_lastSaveLoad = 0.0f;
	}

	if (gEnv->pFlashUI)
		gEnv->pFlashUI->Update(deltaUI);

	const bool bInLevelLoad = IsInLevelLoad();
	if (!bInLevelLoad)
		m_pGameObjectSystem->PostUpdate(delta);

	CRangeSignaling::ref().SetDebug(m_pDebugRangeSignaling->GetIVal() == 1);
	CRangeSignaling::ref().Update(delta);

	CSignalTimer::ref().SetDebug(m_pDebugSignalTimers->GetIVal() == 1);
	CSignalTimer::ref().Update(delta);
}

void CCryAction::PostRenderSubmit()
{
	CRY_PROFILE_REGION(PROFILE_GAME, "CCryAction::PostRenderSubmit");
	CRYPROFILE_SCOPE_PROFILE_MARKER("CCryAction::PostRenderSubmit");

	if (m_pGame)
	{
		if (m_pGame->Update())
		{
			// we need to do disconnect otherwise GameDLL does not realise that
			// the game has gone
			if (CCryActionCVars::Get().g_allowDisconnectIfUpdateFails)
			{
				gEnv->pConsole->ExecuteString("disconnect");
			}
			else
			{
				CryLog("m_pGame->Update has failed but cannot disconnect as g_allowDisconnectIfUpdateFails is not set");
			}
		}

		m_pNetMsgDispatcher->Update();
		m_pEntityContainerMgr->Update();
	}

	if (CGameServerNub* pServerNub = GetGameServerNub())
		pServerNub->Update();

	if (m_pTimeDemoRecorder && !IsGamePaused())
		m_pTimeDemoRecorder->PostUpdate();

	if (m_delayedSaveCountDown)
	{
		--m_delayedSaveCountDown;
	}
	else if (m_delayedSaveGameMethod != eSGM_NoSave && m_pLocalAllocs)
	{
		const bool quick = m_delayedSaveGameMethod == eSGM_QuickSave ? true : false;
		SaveGame(m_pLocalAllocs->m_delayedSaveGameName, quick, true, m_delayedSaveGameReason, false, m_pLocalAllocs->m_checkPointName.c_str());
		m_delayedSaveGameMethod = eSGM_NoSave;
		m_pLocalAllocs->m_delayedSaveGameName.assign("");
	}
}

void CCryAction::Reset(bool clients)
{
	CGameContext* pGC = GetGameContext();
	if (pGC && pGC->HasContextFlag(eGSF_Server))
	{
		pGC->ResetMap(true);
		if (gEnv->bServer && GetGameServerNub())
			GetGameServerNub()->ResetOnHoldChannels();
	}

	if (m_pGameplayRecorder)
		m_pGameplayRecorder->Event(0, GameplayEvent(eGE_GameReset));
}

void CCryAction::PauseGame(bool pause, bool force, unsigned int nFadeOutInMS)
{
	// we should generate some events here
	// who is up to receive them ?

	if (!force && m_paused && m_forcedpause)
	{
		return;
	}

	if (m_paused != pause || m_forcedpause != force)
	{
		gEnv->pTimer->PauseTimer(ITimer::ETIMER_GAME, pause);

		// no game input should happen during pause
		// LEAVE THIS COMMENTED CODE IN - we might still need to uncommented it if this would give any issues
		//m_pActionMapManager->Enable(!pause);

		REINST("notify the audio system!");

		if (pause && gEnv->pInput) //disable rumble
			gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eIDT_Gamepad, eFF_Rumble_Basic, SFFTriggerOutputData::Initial::ZeroIt, 0.0f, 0.0f, 0.0f));

		gEnv->p3DEngine->GetTimeOfDay()->SetPaused(pause);

		// pause EntitySystem Timers
		gEnv->pEntitySystem->PauseTimers(pause);

		m_paused = pause;
		m_forcedpause = m_paused ? force : false;

		if (gEnv->pMovieSystem)
		{
			if (pause)
				gEnv->pMovieSystem->Pause();
			else
				gEnv->pMovieSystem->Resume();
		}

		if (m_paused)
		{
			SGameObjectEvent evt(eGFE_PauseGame, eGOEF_ToAll);
			m_pGameObjectSystem->BroadcastEvent(evt);
			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_PAUSED, 0, 0);
		}
		else
		{
			SGameObjectEvent evt(eGFE_ResumeGame, eGOEF_ToAll);
			m_pGameObjectSystem->BroadcastEvent(evt);
			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_RESUMED, 0, 0);
		}
	}
}

bool CCryAction::IsGamePaused()
{
	return m_paused || !gEnv->pTimer->IsTimerEnabled();
}

bool CCryAction::IsGameStarted()
{
	CGameContext* pGameContext = m_pGame ?
	                             m_pGame->GetGameContext() :
	                             NULL;

	bool bStarted = pGameContext ?
	                pGameContext->IsGameStarted() :
	                false;

	return bStarted;
}

bool CCryAction::StartGameContext(const SGameStartParams* pGameStartParams)
{
	LOADING_TIME_PROFILE_SECTION;
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "StartGameContext");

	if (gEnv->IsEditor())
	{
		if (!m_pGame)
			CryFatalError("Must have game around always for editor");
	}
	else
	{
		// game context should already be delete here, because start game context
		// is not supposed to end the previous game context (level heap issues)
		//EndGameContext();

		m_pGame = new CActionGame(m_pScriptRMI);
	}

	// Unlock shared parameters manager. This must happen after the level heap is initialized and before level load.

	if (m_pSharedParamsManager)
	{
		m_pSharedParamsManager->Unlock();
	}

	if (!m_pGame->Init(pGameStartParams))
	{
		GameWarning("Failed initializing game");
		EndGameContext();
		return false;
	}

	return true;
}

bool CCryAction::BlockingSpawnPlayer()
{
	if (!m_pGame)
		return false;
	return m_pGame->BlockingSpawnPlayer();
}

void CCryAction::ResetBrokenGameObjects()
{
	if (m_pGame)
	{
		m_pGame->FixBrokenObjects(true);
		m_pGame->ClearBreakHistory();
	}
}

/////////////////////////////////////////
// CloneBrokenObjectsAndRevertToStateAtTime() is called by the kill cam. It takes a list of indices into
//		the break events array, which it uses to find the objects that were broken during the kill cam, and
//		therefore need to be cloned for playback. It clones the broken objects, and hides the originals. It
//		then reverts the cloned objects to their unbroken state, and re-applies any breaks that occured up
//		to the start of the kill cam.
void CCryAction::CloneBrokenObjectsAndRevertToStateAtTime(int32 iFirstBreakEventIndex, uint16* pBreakEventIndices, int32& iNumBreakEvents, IRenderNode** outClonedNodes, int32& iNumClonedNodes, SRenderNodeCloneLookup& renderNodeLookup)
{
	if (m_pGame)
	{
		//CryLogAlways(">> Cloning objects broken during killcam and reverting to unbroken state");
		m_pGame->CloneBrokenObjectsByIndex(pBreakEventIndices, iNumBreakEvents, outClonedNodes, iNumClonedNodes, renderNodeLookup);

		//CryLogAlways(">> Hiding original versions of objects broken during killcam");
		m_pGame->HideBrokenObjectsByIndex(pBreakEventIndices, iNumBreakEvents);

		//CryLogAlways(">> Applying breaks up to start of killcam to cloned objects");
		m_pGame->ApplyBreaksUntilTimeToObjectList(iFirstBreakEventIndex, renderNodeLookup);
	}
}

/////////////////////////////////////////
// ApplySingleProceduralBreakFromEventIndex() re-applies a single break to a cloned object, for use in the
//		kill cam during playback
void CCryAction::ApplySingleProceduralBreakFromEventIndex(uint16 uBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup)
{
	if (m_pGame)
	{
		m_pGame->ApplySingleProceduralBreakFromEventIndex(uBreakEventIndex, renderNodeLookup);
	}
}

/////////////////////////////////////////
// UnhideBrokenObjectsByIndex() is provided a list of indices into the Break Event array, that it uses
//		to look up and unhide broken objects assocated with the break events. This is used by the kill
//		cam when it finishes playback
void CCryAction::UnhideBrokenObjectsByIndex(uint16* pObjectIndicies, int32 iNumObjectIndices)
{
	if (m_pGame)
	{
		m_pGame->UnhideBrokenObjectsByIndex(pObjectIndicies, iNumObjectIndices);
	}
}

bool CCryAction::ChangeGameContext(const SGameContextParams* pGameContextParams)
{
	if (!m_pGame)
		return false;
	return m_pGame->ChangeGameContext(pGameContextParams);
}

void CCryAction::EndGameContext()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!gEnv) // SCA fix
		return;

#ifndef _RELEASE
	CryLog("Ending game context...");
	INDENT_LOG_DURING_SCOPE();
#endif

	// to make this function re-entrant, m_pGame pointer must be set to 0
	// BEFORE the destructor of CActionGame is invoked (Craig)
	_smart_ptr<CActionGame> pGame = m_pGame;

#if CAPTURE_REPLAY_LOG
	static int unloadCount = 0;
	CryStackStringT<char, 128> levelName;
	bool addedUnloadLabel = false;
	if (pGame)
	{
		levelName = pGame->GetLevelName().c_str();
		addedUnloadLabel = true;
		MEMSTAT_LABEL_FMT("unloadStart%d_%s", unloadCount, levelName.c_str());
	}
#endif

	m_pGame = 0;
	pGame = 0;

	if (m_pSharedParamsManager)
	{
		m_pSharedParamsManager->Reset();
	}

	if (m_pVehicleSystem)
	{
		m_pVehicleSystem->Reset();
	}

	if (m_pScriptRMI)
	{
		m_pScriptRMI->UnloadLevel();
	}

	if (gEnv && gEnv->IsEditor())
		m_pGame = new CActionGame(m_pScriptRMI);

	if (m_pActorSystem)
	{
		m_pActorSystem->Reset();
	}

	if (m_nextFrameCommand)
	{
		stl::free_container(*m_nextFrameCommand);
	}

	if (m_pLocalAllocs)
	{
		stl::free_container(m_pLocalAllocs->m_delayedSaveGameName);
		stl::free_container(m_pLocalAllocs->m_checkPointName);
		stl::free_container(m_pLocalAllocs->m_nextLevelToLoad);
	}

#if CAPTURE_REPLAY_LOG
	if (addedUnloadLabel)
	{
		if (!levelName.empty())
		{
			MEMSTAT_LABEL_FMT("unloadEnd%d_%s", unloadCount++, levelName.c_str());
		}
		else
		{
			MEMSTAT_LABEL_FMT("unloadEnd%d", unloadCount++);
		}
	}
#endif

	if (gEnv && gEnv->pSystem)
	{
		// clear all error messages to prevent stalling due to runtime file access check during chainloading
		gEnv->pSystem->ClearErrorMessages();
	}

	if (gEnv && gEnv->pCryPak)
	{
		gEnv->pCryPak->DisableRuntimeFileAccess(false);
	}

	// Reset and lock shared parameters manager. This must happen after level unload and before the level heap is released.

	if (m_pSharedParamsManager)
	{
		m_pSharedParamsManager->Reset();

		m_pSharedParamsManager->Lock();
	}

	// Clear the net message listeners for the current session
	if (m_pNetMsgDispatcher)
	{
		m_pNetMsgDispatcher->ClearListeners();
	}
}

void CCryAction::ReleaseGameStats()
{
	if (m_pGame)
	{
		m_pGame->ReleaseGameStats();
	}

	/*
	   #if STATS_MODE_CVAR
	   if(CCryActionCVars::Get().g_statisticsMode == 2)
	   #endif

	    // Fran: need to be ported to the new interface
	    GetIGameStatistics()->EndSession();
	 */
}

void CCryAction::InitEditor(IGameToEditorInterface* pGameToEditor)
{
	LOADING_TIME_PROFILE_SECTION;
	m_isEditing = true;

	m_pGameToEditor = pGameToEditor;

	uint32 commConfigCount = gEnv->pAISystem->GetCommunicationManager()->GetConfigCount();
	if (commConfigCount)
	{
		std::vector<const char*> configNames;
		configNames.resize(commConfigCount);

		for (uint i = 0; i < commConfigCount; ++i)
			configNames[i] = gEnv->pAISystem->GetCommunicationManager()->GetConfigName(i);

		pGameToEditor->SetUIEnums("CommConfig", &configNames.front(), commConfigCount);
	}

	uint32 factionCount = gEnv->pAISystem->GetFactionMap().GetFactionCount();
	if (factionCount)
	{
		std::vector<const char*> factionNames;
		factionNames.resize(factionCount + 1);
		factionNames[0] = "";

		for (uint32 i = 0; i < factionCount; ++i)
			factionNames[i + 1] = gEnv->pAISystem->GetFactionMap().GetFactionName(i);

		pGameToEditor->SetUIEnums("Faction", &factionNames.front(), factionCount + 1);
		pGameToEditor->SetUIEnums("FactionFilter", &factionNames.front(), factionCount + 1);
	}

	if (factionCount)
	{
		const uint32 reactionCount = 4;
		const char* reactions[reactionCount] = {
			"",
			"Hostile",
			"Neutral",
			"Friendly",
		};

		pGameToEditor->SetUIEnums("Reaction", reactions, reactionCount);
		pGameToEditor->SetUIEnums("ReactionFilter", reactions, reactionCount);
	}

	uint32 agentTypeCount = gEnv->pAISystem->GetNavigationSystem()->GetAgentTypeCount();
	if (agentTypeCount)
	{
		std::vector<const char*> agentTypeNames;
		agentTypeNames.resize(agentTypeCount + 1);
		agentTypeNames[0] = "";

		for (size_t i = 0; i < agentTypeCount; ++i)
			agentTypeNames[i + 1] = gEnv->pAISystem->GetNavigationSystem()->GetAgentTypeName(
			  gEnv->pAISystem->GetNavigationSystem()->GetAgentTypeID(i));

		pGameToEditor->SetUIEnums("NavigationType", &agentTypeNames.front(), agentTypeCount + 1);
	}

	uint32 lipSyncCount = gs_lipSyncExtensionNamesForExposureToEditor.size();
	if (lipSyncCount)
	{
		pGameToEditor->SetUIEnums("LipSyncType", &gs_lipSyncExtensionNamesForExposureToEditor.front(), lipSyncCount);
	}
}

//------------------------------------------------------------------------
void CCryAction::SetEditorLevel(const char* levelName, const char* levelFolder)
{
	cry_strcpy(m_editorLevelName, levelName);
	cry_strcpy(m_editorLevelFolder, levelFolder);
}

//------------------------------------------------------------------------
void CCryAction::GetEditorLevel(char** levelName, char** levelFolder)
{
	if (levelName) *levelName = &m_editorLevelName[0];
	if (levelFolder) *levelFolder = &m_editorLevelFolder[0];
}

//------------------------------------------------------------------------
const char* CCryAction::GetStartLevelSaveGameName()
{
	static string levelstart;
#if !CRY_PLATFORM_DESKTOP
	levelstart = CRY_SAVEGAME_FILENAME;
#else
	levelstart = (gEnv->pGameFramework->GetLevelName());

	if (auto* pGame = CCryAction::GetCryAction()->GetIGame())
	{
		const char* szMappedName = pGame->GetMappedLevelName(levelstart.c_str());
		if (szMappedName)
		{
			levelstart = szMappedName;
		}
	}

	levelstart.append("_");

	if (auto* pGame = CCryAction::GetCryAction()->GetIGame())
	{
		levelstart.append(pGame->GetName());
	}
#endif
	levelstart.append(CRY_SAVEGAME_FILE_EXT);
	return levelstart.c_str();
}

static void BroadcastCheckpointEvent(ESaveGameReason reason)
{
	if (reason == eSGR_FlowGraph || reason == eSGR_Command || reason == eSGR_LevelStart)
	{
		IPlatformOS::SPlatformEvent event(GetISystem()->GetPlatformOS()->GetFirstSignedInUser());
		event.m_eEventType = IPlatformOS::SPlatformEvent::eET_FileWrite;
		event.m_uParams.m_fileWrite.m_type = reason == eSGR_LevelStart ? IPlatformOS::SPlatformEvent::eFWT_CheckpointLevelStart : IPlatformOS::SPlatformEvent::eFWT_Checkpoint;
		GetISystem()->GetPlatformOS()->NotifyListeners(event);
	}
}

//------------------------------------------------------------------------
bool CCryAction::SaveGame(const char* path, bool bQuick, bool bForceImmediate, ESaveGameReason reason, bool ignoreDelay, const char* checkPointName)
{
	CryLog("[SAVE GAME] %s to '%s'%s%s - checkpoint=\"%s\"", bQuick ? "Quick-saving" : "Saving", path, bForceImmediate ? " immediately" : "Delayed", ignoreDelay ? " ignoring delay" : "", checkPointName);
	INDENT_LOG_DURING_SCOPE();

	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Saving game");

	if (gEnv->bMultiplayer)
		return false;

	IActor* pClientActor = GetClientActor();
	if (!pClientActor)
		return false;

	if (pClientActor->IsDead())
	{
		//don't save when the player already died - savegame will be corrupt
		GameWarning("Saving failed : player is dead!");
		return false;
	}

	if (CanSave() == false)
	{
		// When in time demo but not chain loading we need to allow the level start save
		bool bIsInTimeDemoButNotChainLoading = IsInTimeDemo() && !m_pTimeDemoRecorder->IsChainLoading();
		if (!(reason == eSGR_LevelStart && bIsInTimeDemoButNotChainLoading))
		{
			ICVar* saveLoadEnabled = gEnv->pConsole->GetCVar("g_EnableLoadSave");
			bool enabled = saveLoadEnabled->GetIVal() == 1;
			if (enabled)
			{
				GameWarning("CCryAction::SaveGame: Suppressing QS (likely due to timedemo or cutscene)");
			}
			else
			{
				GameWarning("CCryAction::SaveGame: Suppressing QS (g_EnableLoadSave is disabled)");
			}
			return false;
		}
	}

	if (m_lastSaveLoad > 0.0f)
	{
		if (ignoreDelay)
			m_lastSaveLoad = 0.0f;
		else
			return false;
	}

	bool bRet = true;
	if (bForceImmediate)
	{
		// Ensure we have the save checkpoint icon going on by telling IPlatformOS that the following writes are for a checkpoint.
		if (m_delayedSaveGameMethod == eSGM_NoSave)
			BroadcastCheckpointEvent(reason);

		IPlatformOS::CScopedSaveLoad osSaveLoad(GetISystem()->GetPlatformOS()->GetFirstSignedInUser(), true);
		if (!osSaveLoad.Allowed())
			return false;

		// Best save profile or we'll lose persistent stats
		if (m_pPlayerProfileManager)
		{
			IPlayerProfileManager::EProfileOperationResult result;
			m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result, ePR_Game);
		}

#if ENABLE_STATOSCOPE
		if (gEnv->pStatoscope)
		{
			CryFixedStringT<128> s;
			switch (reason)
			{
			case eSGR_Command:
				s = "Command";
				break;
			case eSGR_FlowGraph:
				s = "FlowGraph";
				break;
			case eSGR_LevelStart:
				s = "LevelStart";
				break;
			case eSGR_QuickSave:
				s = "QuickSave";
				break;
			default:
				s = "Unknown Reason";
				break;
			}
			if (checkPointName && *checkPointName)
			{
				s += " - ";
				s += checkPointName;
			}
			gEnv->pStatoscope->AddUserMarker("SaveGame", s.c_str());
		}
#endif

		// check, if preSaveGame has been called already
		if (m_pLocalAllocs && m_pLocalAllocs->m_delayedSaveGameName.empty())
			OnActionEvent(SActionEvent(eAE_preSaveGame, (int) reason));
		CTimeValue elapsed = -gEnv->pTimer->GetAsyncTime();
		gEnv->pSystem->SerializingFile(bQuick ? 1 : 2);
		bRet = m_pGameSerialize ? m_pGameSerialize->SaveGame(this, "xml", path, reason, checkPointName) : false;
		gEnv->pSystem->SerializingFile(0);
		OnActionEvent(SActionEvent(eAE_postSaveGame, (int) reason, bRet ? (checkPointName ? checkPointName : "") : 0));
		m_lastSaveLoad = s_loadSaveDelay;
		elapsed += gEnv->pTimer->GetAsyncTime();

		if (!bRet)
			GameWarning("[CryAction] SaveGame: '%s' %s. [Duration=%.4f secs]", path, "failed", elapsed.GetSeconds());
		else
			CryLog("SaveGame: '%s' %s. [Duration=%.4f secs]", path, "done", elapsed.GetSeconds());

#if !defined(_RELEASE)
		ICVar* pCVar = gEnv->pConsole->GetCVar("g_displayCheckpointName");

		if (pCVar && pCVar->GetIVal())
		{
			IPersistantDebug* pPersistantDebug = GetIPersistantDebug();

			if (pPersistantDebug)
			{
				static const ColorF colour(1.0f, 0.0f, 0.0f, 1.0f);

				char text[128];

				cry_sprintf(text, "Saving Checkpoint '%s'\n", checkPointName ? checkPointName : "unknown");

				pPersistantDebug->Add2DText(text, 2.f, colour, 10.0f);
			}
		}
#endif //_RELEASE
	}
	else
	{
		// Delayed save. Allows us to render the saving warning icon briefly before committing
		BroadcastCheckpointEvent(reason);

		m_delayedSaveGameMethod = bQuick ? eSGM_QuickSave : eSGM_Save;
		m_delayedSaveGameReason = reason;
		m_delayedSaveCountDown = s_saveGameFrameDelay;
		if (m_pLocalAllocs)
		{
			m_pLocalAllocs->m_delayedSaveGameName = path;
			if (checkPointName)
				m_pLocalAllocs->m_checkPointName = checkPointName;
			else
				m_pLocalAllocs->m_checkPointName.clear();
		}
		OnActionEvent(SActionEvent(eAE_preSaveGame, (int) reason));
	}
	return bRet;
}

//------------------------------------------------------------------------
ELoadGameResult CCryAction::LoadGame(const char* path, bool quick, bool ignoreDelay)
{
	CryLog("[LOAD GAME] %s saved game '%s'%s", quick ? "Quick-loading" : "Loading", path, ignoreDelay ? " ignoring delay" : "");
	INDENT_LOG_DURING_SCOPE();

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Loading game");

	if (gEnv->bMultiplayer)
		return eLGR_Failed;

	if (m_lastSaveLoad > 0.0f)
	{
		if (ignoreDelay)
			m_lastSaveLoad = 0.0f;
		else
			return eLGR_Failed;
	}

	if (CanLoad() == false)
	{
		ICVar* saveLoadEnabled = gEnv->pConsole->GetCVar("g_EnableLoadSave");
		bool enabled = saveLoadEnabled->GetIVal() == 1;
		if (enabled)
		{
			GameWarning("CCryAction::SaveGame: Suppressing QL (likely manually disabled)");
		}
		else
		{
			GameWarning("CCryAction::SaveGame: Suppressing QL (g_EnableLoadSave is disabled)");
		}
		return eLGR_Failed;
	}

	IPlatformOS::CScopedSaveLoad osSaveLoad(GetISystem()->GetPlatformOS()->GetFirstSignedInUser(), false);
	if (!osSaveLoad.Allowed())
		return eLGR_Failed;

	CTimeValue elapsed = -gEnv->pTimer->GetAsyncTime();

	gEnv->pSystem->SerializingFile(quick ? 1 : 2);

	SGameStartParams params;
	params.flags = eGSF_Server | eGSF_Client;
	params.hostname = "localhost";
	params.maxPlayers = 1;
	//	params.pContextParams = &ctx; (set by ::LoadGame)
	params.port = ChooseListenPort();

	//pause entity event timers update
	gEnv->pEntitySystem->PauseTimers(true, false);

	// Restore game persistent stats from profile to avoid exploits
	if (m_pPlayerProfileManager)
	{
		const char* userName = m_pPlayerProfileManager->GetCurrentUser();
		IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(userName);
		if (pProfile)
		{
			m_pPlayerProfileManager->ReloadProfile(pProfile, ePR_Game);
		}
	}

	GameWarning("[CryAction] LoadGame: '%s'", path);

	ELoadGameResult loadResult = m_pGameSerialize ? m_pGameSerialize->LoadGame(this, "xml", path, params, quick) : eLGR_Failed;

	switch (loadResult)
	{
	case eLGR_Ok:
		gEnv->pEntitySystem->PauseTimers(false, false);
		GetISystem()->SerializingFile(0);

		// AllowSave never needs to be serialized, but reset here, because
		// if we had saved a game before it is obvious that at that point saving was not prohibited
		// otherwise we could not have saved it beforehand
		AllowSave(true);

		elapsed += gEnv->pTimer->GetAsyncTime();
		CryLog("[LOAD GAME] LoadGame: '%s' done. [Duration=%.4f secs]", path, elapsed.GetSeconds());
		m_lastSaveLoad = s_loadSaveDelay;
#ifndef _RELEASE
		GetISystem()->IncreaseCheckpointLoadCount();
#endif
		return eLGR_Ok;
	default:
		gEnv->pEntitySystem->PauseTimers(false, false);
		GameWarning("Unknown result code from CGameSerialize::LoadGame");
	// fall through
	case eLGR_FailedAndDestroyedState:
		GameWarning("[CryAction] LoadGame: '%s' failed. Ending GameContext", path);
		EndGameContext();
	// fall through
	case eLGR_Failed:
		GetISystem()->SerializingFile(0);
		// Unpause all streams in streaming engine.
		GetISystem()->GetStreamEngine()->PauseStreaming(false, -1);
		return eLGR_Failed;
	case eLGR_CantQuick_NeedFullLoad:
		GetISystem()->SerializingFile(0);
		// Unpause all streams in streaming engine.
		GetISystem()->GetStreamEngine()->PauseStreaming(false, -1);
		return eLGR_CantQuick_NeedFullLoad;
	}
}

//------------------------------------------------------------------------
IGameFramework::TSaveGameName CCryAction::CreateSaveGameName()
{
	//design wants to have different, more readable names for the savegames generated
	int id = 0;

	TSaveGameName saveGameName;
#if CRY_PLATFORM_DURANGO
	saveGameName = CRY_SAVEGAME_FILENAME;
#else
	//saves a running savegame id which is displayed with the savegame name
	if (IPlayerProfileManager* m_pPlayerProfileManager = gEnv->pGameFramework->GetIPlayerProfileManager())
	{
		const char* user = m_pPlayerProfileManager->GetCurrentUser();
		if (IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(user))
		{
			pProfile->GetAttribute("Singleplayer.SaveRunningID", id);
			pProfile->SetAttribute("Singleplayer.SaveRunningID", id + 1);
		}
	}

	saveGameName = CRY_SAVEGAME_FILENAME;
	char buffer[16];
	itoa(id, buffer, 10);
	saveGameName.clear();
	if (id < 10)
		saveGameName += "0";
	saveGameName += buffer;
	saveGameName += "_";

	const char* levelName = GetLevelName();

	if (auto* pGame = m_externalGameLibrary.pGame)
	{
		const char* mappedName = pGame->GetMappedLevelName(levelName);
		saveGameName += mappedName;
	}

	saveGameName += "_";
	saveGameName += gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName();
	saveGameName += "_";
	string timeString;

	CTimeValue time = gEnv->pTimer->GetFrameStartTime() - m_levelStartTime;
	timeString.Format("%d", int_round(time.GetSeconds()));

	saveGameName += timeString;
#endif
	saveGameName += CRY_SAVEGAME_FILE_EXT;

	return saveGameName;
}

//------------------------------------------------------------------------
void CCryAction::OnEditorSetGameMode(int iMode)
{
	LOADING_TIME_PROFILE_SECTION;

	if (m_externalGameLibrary.pGame && iMode < 2)
	{
		m_externalGameLibrary.pGame->EditorResetGame(iMode != 0 ? true : false);
	}

	if (iMode < 2)
	{
		/* AlexL: for now don't set time to 0.0
		   (because entity timers might still be active and getting confused)
		   if (iMode == 1)
		   gEnv->pTimer->SetTimer(ITimer::ETIMER_GAME, 0.0f);
		 */

		if (iMode == 0)
			m_pTimeDemoRecorder->Reset();

		if (m_pGame)
			m_pGame->OnEditorSetGameMode(iMode != 0);

		if (m_pMaterialEffects)
		{
			if (iMode && m_isEditing)
				m_pMaterialEffects->PreLoadAssets();
			m_pMaterialEffects->SetUpdateMode(iMode != 0);
		}
		m_isEditing = !iMode;
	}
	else if (m_pGame)
	{
		m_pGame->FixBrokenObjects(true);
		m_pGame->ClearBreakHistory();
	}

	const char* szResetCameraCommand = "CryAction.ResetToNormalCamera()";
	gEnv->pScriptSystem->ExecuteBuffer(szResetCameraCommand, strlen(szResetCameraCommand));

	// reset any pending camera blending
	if (m_pViewSystem)
	{
		m_pViewSystem->SetBlendParams(0, 0, 0);
		IView* pView = m_pViewSystem->GetActiveView();
		if (pView)
			pView->ResetBlending();
	}

	if (GetIForceFeedbackSystem())
		GetIForceFeedbackSystem()->StopAllEffects();

	CRangeSignaling::ref().OnEditorSetGameMode(iMode != 0);
	CSignalTimer::ref().OnEditorSetGameMode(iMode != 0);

	if (m_pCooperativeAnimationManager)
	{
		m_pCooperativeAnimationManager->Reset();
	}

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(iMode > 0 ? ESYSTEM_EVENT_GAME_MODE_SWITCH_START : ESYSTEM_EVENT_GAME_MODE_SWITCH_END, 0, 0);
}

//------------------------------------------------------------------------
IFlowSystem* CCryAction::GetIFlowSystem()
{
	return gEnv->pFlowSystem;
}

//------------------------------------------------------------------------
void CCryAction::SetGameQueryListener(CGameQueryListener* pListener)
{
	if (m_pGameQueryListener)
	{
		m_pGameQueryListener->Complete();
		m_pGameQueryListener = NULL;
	}
	CRY_ASSERT(m_pGameQueryListener == NULL);
	m_pGameQueryListener = pListener;
}

//------------------------------------------------------------------------
void CCryAction::BeginLanQuery()
{
	CGameQueryListener* pNewListener = new CGameQueryListener;
	m_pLanQueryListener = m_pNetwork->CreateLanQueryListener(pNewListener);
	pNewListener->SetNetListener(m_pLanQueryListener);
	SetGameQueryListener(pNewListener);
}

//------------------------------------------------------------------------
void CCryAction::EndCurrentQuery()
{
	SetGameQueryListener(NULL);
}

//------------------------------------------------------------------------
void CCryAction::InitCVars()
{
	IConsole* pC = ::gEnv->pConsole;
	assert(pC);
	m_pEnableLoadingScreen = REGISTER_INT("g_enableloadingscreen", 1, VF_DUMPTODISK, "Enable/disable the loading screen");
	REGISTER_INT("g_enableitems", 1, 0, "Enable/disable the item system");
	m_pShowLanBrowserCVAR = REGISTER_INT("net_lanbrowser", 0, VF_CHEAT, "enable lan games browser");
	REGISTER_INT("g_aimdebug", 0, VF_CHEAT, "Enable/disable debug drawing for aiming direction");
	REGISTER_INT("g_groundeffectsdebug", 0, 0, "Enable/disable logging for GroundEffects (2 = use non-deferred ray-casting)");
	REGISTER_FLOAT("g_breakImpulseScale", 1.0f, VF_REQUIRE_LEVEL_RELOAD, "How big do explosions need to be to break things?");
	REGISTER_INT("g_breakage_particles_limit", 200, 0, "Imposes a limit on particles generated during 2d surfaces breaking");
	REGISTER_FLOAT("c_shakeMult", 1.0f, VF_CHEAT, "");

	m_pDebugSignalTimers = REGISTER_INT("ai_DebugSignalTimers", 0, VF_CHEAT, "Enable Signal Timers Debug Screen");
	m_pDebugRangeSignaling = REGISTER_INT("ai_DebugRangeSignaling", 0, VF_CHEAT, "Enable Range Signaling Debug Screen");

	m_pAsyncLevelLoad = REGISTER_INT("g_asynclevelload", 0, VF_CONST_CVAR, "Enable asynchronous level loading");

	REGISTER_INT("cl_packetRate", 30, 0, "Packet rate on client");
	REGISTER_INT("sv_packetRate", 30, 0, "Packet rate on server");
	REGISTER_INT("cl_bandwidth", 50000, 0, "Bit rate on client");
	REGISTER_INT("sv_bandwidth", 50000, 0, "Bit rate on server");
	REGISTER_FLOAT("g_localPacketRate", 50, 0, "Packet rate locally on faked network connection");
	REGISTER_INT("sv_timeout_disconnect", 0, 0, "Timeout for fully disconnecting timeout connections");

	//REGISTER_CVAR2( "cl_voice_playback",&m_VoicePlaybackEnabled);
	// NB this should be false by default - enabled when user holds down CapsLock
	REGISTER_CVAR2("cl_voice_recording", &m_VoiceRecordingEnabled, 0, 0, "Enable client voice recording");

	int defaultServerPort = SERVER_DEFAULT_PORT;

	ICVar* pLobbyPort = gEnv->pConsole->GetCVar("net_lobby_default_port");
	if (pLobbyPort && pLobbyPort->GetIVal() > 0)
	{
		defaultServerPort = pLobbyPort->GetIVal();
	}

	REGISTER_STRING("cl_serveraddr", "localhost", VF_DUMPTODISK, "Server address");
	REGISTER_INT("cl_serverport", defaultServerPort, VF_DUMPTODISK, "Server port");
	REGISTER_STRING("cl_serverpassword", "", VF_DUMPTODISK, "Server password");
	REGISTER_STRING("sv_map", "nolevel", 0, "The map the server should load");

	REGISTER_STRING("sv_levelrotation", "levelrotation", 0, "Sequence of levels to load after each game ends");

	REGISTER_STRING("sv_requireinputdevice", "dontcare", VF_DUMPTODISK | VF_REQUIRE_LEVEL_RELOAD, "Which input devices to require at connection (dontcare, none, gamepad, keyboard)");
	const char* defaultGameRules = "SinglePlayer";
	if (gEnv->IsDedicated())
		defaultGameRules = "DeathMatch";
	ICVar* pDefaultGameRules = REGISTER_STRING("sv_gamerulesdefault", defaultGameRules, 0, "The game rules that the server default to when disconnecting");
	const char* currentGameRules = pDefaultGameRules ? pDefaultGameRules->GetString() : "";
	REGISTER_STRING("sv_gamerules", currentGameRules, 0, "The game rules that the server should use");
	REGISTER_INT("sv_port", SERVER_DEFAULT_PORT, VF_DUMPTODISK, "Server address");
	REGISTER_STRING("sv_password", "", VF_DUMPTODISK, "Server password");
	REGISTER_INT("sv_lanonly", 0, VF_DUMPTODISK, "Set for LAN games");

	REGISTER_STRING("cl_nickname", "", VF_DUMPTODISK, "Nickname for player on connect.");

	REGISTER_STRING("sv_bind", "0.0.0.0", VF_REQUIRE_LEVEL_RELOAD, "Bind the server to a specific IP address");

	REGISTER_STRING("sv_servername", "", VF_DUMPTODISK, "Server name will be displayed in server list. If empty, machine name will be used.");
	REGISTER_INT_CB("sv_maxplayers", 32, VF_DUMPTODISK, "Maximum number of players allowed to join server.", VerifyMaxPlayers);
	REGISTER_INT("sv_maxspectators", 32, VF_DUMPTODISK, "Maximum number of players allowed to be spectators during the game.");
	REGISTER_INT("ban_timeout", 30, VF_DUMPTODISK, "Ban timeout in minutes");
	REGISTER_FLOAT("sv_timeofdaylength", 1.0f, VF_DUMPTODISK, "Sets time of day changing speed.");
	REGISTER_FLOAT("sv_timeofdaystart", 12.0f, VF_DUMPTODISK, "Sets time of day start time.");
	REGISTER_INT("sv_timeofdayenable", 0, VF_DUMPTODISK, "Enables time of day simulation.");

	REGISTER_INT("g_immersive", 1, 0, "If set, multiplayer physics will be enabled");

	REGISTER_INT("sv_dumpstats", 1, 0, "Enables/disables dumping of level and player statistics, positions, etc. to files");
	REGISTER_INT("sv_dumpstatsperiod", 1000, 0, "Time period of statistics dumping in milliseconds");
	REGISTER_INT("g_EnableLoadSave", 1, 0, "Enables/disables saving and loading of savegames");

	REGISTER_STRING("http_password", "password", 0, "Password for http administration");
	REGISTER_STRING("rcon_password", "", 0, "Sets password for the RCON system");

#if !defined(_RELEASE)
	REGISTER_INT("connect_repeatedly_num_attempts", 5, 0, "the number of attempts to connect that connect_repeatedly tries");
	REGISTER_FLOAT("connect_repeatedly_time_between_attempts", 10.0f, 0, "the time between connect attempts for connect_repeatedly");
	REGISTER_INT("g_displayCheckpointName", 0, VF_NULL, "Display checkpoint name when game is saved");
#endif

	REGISTER_INT_CB("cl_comment", (gEnv->IsEditor() ? 1 : 0), VF_NULL, "Hide/Unhide comments in game-mode", ResetComments);

	m_pMaterialEffectsCVars = new CMaterialEffectsCVars();

	CActionGame::RegisterCVars();
}

//------------------------------------------------------------------------
void CCryAction::ReleaseCVars()
{
	SAFE_DELETE(m_pMaterialEffectsCVars);
	SAFE_DELETE(m_pCryActionCVars);
}

void CCryAction::InitCommands()
{
	// create built-in commands
	REGISTER_COMMAND("map", MapCmd, VF_BLOCKFRAME, "Load a map");
	// for testing purposes
	REGISTER_COMMAND("readabilityReload", ReloadReadabilityXML, 0, "Reloads readability xml files.");
	REGISTER_COMMAND("unload", UnloadCmd, 0, "Unload current map");
	REGISTER_COMMAND("dump_maps", DumpMapsCmd, 0, "Dumps currently scanned maps");
	REGISTER_COMMAND("play", PlayCmd, 0, "Play back a recorded game");
	REGISTER_COMMAND("connect", ConnectCmd, VF_RESTRICTEDMODE, "Start a client and connect to a server");
	REGISTER_COMMAND("disconnect", DisconnectCmd, 0, "Stop a game (or a client or a server)");
	REGISTER_COMMAND("disconnectchannel", DisconnectChannelCmd, 0, "Stop a game (or a client or a server)");
	REGISTER_COMMAND("status", StatusCmd, 0, "Shows connection status");
	REGISTER_COMMAND("version", VersionCmd, 0, "Shows game version number");
	REGISTER_COMMAND("save", SaveGameCmd, VF_RESTRICTEDMODE, "Save game");
	REGISTER_COMMAND("save_genstrings", GenStringsSaveGameCmd, VF_CHEAT, "");
	REGISTER_COMMAND("load", LoadGameCmd, VF_RESTRICTEDMODE, "Load game");
	REGISTER_COMMAND("test_reset", TestResetCmd, VF_CHEAT, "");
	REGISTER_COMMAND("open_url", OpenURLCmd, VF_NULL, "");

	REGISTER_COMMAND("test_playersBounds", TestPlayerBoundsCmd, VF_CHEAT, "");
	REGISTER_COMMAND("g_dump_stats", DumpStatsCmd, VF_CHEAT, "");

	REGISTER_COMMAND("kick", KickPlayerCmd, 0, "Kicks player from game");
	REGISTER_COMMAND("kickid", KickPlayerByIdCmd, 0, "Kicks player from game");

	REGISTER_COMMAND("test_delegate", DelegateCmd, VF_CHEAT, "delegate test cmd");

	REGISTER_COMMAND("ban", BanPlayerCmd, 0, "Bans player for 30 minutes from server.");
	REGISTER_COMMAND("ban_status", BanStatusCmd, 0, "Shows currently banned players.");
	REGISTER_COMMAND("ban_remove", UnbanPlayerCmd, 0, "Removes player from ban list.");

	REGISTER_COMMAND("dump_stats", DumpAnalysisStatsCmd, 0, "Dumps some player statistics");

#if !defined(_RELEASE)
	REGISTER_COMMAND("connect_repeatedly", ConnectRepeatedlyCmd, VF_RESTRICTEDMODE, "Start a client and attempt to connect repeatedly to a server");
#endif

	// RCON system
	REGISTER_COMMAND("rcon_startserver", rcon_startserver, 0, "Starts a remote control server");
	REGISTER_COMMAND("rcon_stopserver", rcon_stopserver, 0, "Stops a remote control server");
	REGISTER_COMMAND("rcon_connect", rcon_connect, 0, "Connects to a remote control server");
	REGISTER_COMMAND("rcon_disconnect", rcon_disconnect, 0, "Disconnects from a remote control server");
	REGISTER_COMMAND("rcon_command", rcon_command, 0, "Issues a console command from a RCON client to a RCON server");

	// HTTP server
	REGISTER_COMMAND("http_startserver", http_startserver, 0, "Starts an HTTP server");
	REGISTER_COMMAND("http_stopserver", http_stopserver, 0, "Stops an HTTP server");

	REGISTER_COMMAND("voice_mute", MutePlayer, 0, "Mute player's voice comms");

	REGISTER_COMMAND("net_pb_sv_enable", StaticSetPbSvEnabled, 0, "Sets PunkBuster server enabled state");
	REGISTER_COMMAND("net_pb_cl_enable", StaticSetPbClEnabled, 0, "Sets PunkBuster client enabled state");
}

//------------------------------------------------------------------------
void CCryAction::VerifyMaxPlayers(ICVar* pVar)
{
	int nPlayers = pVar->GetIVal();
	if (nPlayers < 2 || nPlayers > 32)
		pVar->Set(CLAMP(nPlayers, 2, 32));
}

//------------------------------------------------------------------------
void CCryAction::ResetComments(ICVar* pVar)
{
	// Iterate through all comment entities and reset them. This will activate/deactivate them as required.
	SEntityEvent event(ENTITY_EVENT_RESET);
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Comment");

	if (pClass)
	{
		IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
		it->MoveFirst();
		while (IEntity* pEntity = it->Next())
		{
			if (pEntity->GetClass() == pClass)
			{
				pEntity->SendEvent(event);
			}
		}
	}
}

//------------------------------------------------------------------------
bool CCryAction::LoadingScreenEnabled() const
{
	return m_pEnableLoadingScreen ? m_pEnableLoadingScreen->GetIVal() != 0 : true;
}

int CCryAction::NetworkExposeClass(IFunctionHandler* pFH)
{
	return m_pScriptRMI->ExposeClass(pFH);
}

//------------------------------------------------------------------------
void CCryAction::RegisterFactory(const char* name, ISaveGame*(*func)(), bool)
{
	if (m_pGameSerialize)
	{
		m_pGameSerialize->RegisterSaveGameFactory(name, func);
	}
}

//------------------------------------------------------------------------
void CCryAction::RegisterFactory(const char* name, ILoadGame*(*func)(), bool)
{
	if (m_pGameSerialize)
	{
		m_pGameSerialize->RegisterLoadGameFactory(name, func);
	}
}

IActor* CCryAction::GetClientActor() const
{
	return m_pGame ? m_pGame->GetClientActor() : NULL;
}

EntityId CCryAction::GetClientActorId() const
{
	if (m_pGame)
	{
		if (IActor* pActor = m_pGame->GetClientActor())
			return pActor->GetEntityId();
	}

	return LOCAL_PLAYER_ENTITY_ID;
}

IEntity* CCryAction::GetClientEntity() const
{
	EntityId id = GetClientEntityId();
	return gEnv->pEntitySystem->GetEntity(id);
}

EntityId CCryAction::GetClientEntityId() const
{
	if (m_pGame)
	{
		CGameClientChannel* pChannel = m_pGame->GetGameClientNub() ? m_pGame->GetGameClientNub()->GetGameClientChannel() : NULL;
		if (pChannel)
			return pChannel->GetPlayerId();
	}

	return LOCAL_PLAYER_ENTITY_ID;
}

INetChannel* CCryAction::GetClientChannel() const
{
	if (m_pGame)
	{
		CGameClientChannel* pChannel = m_pGame->GetGameClientNub() ? m_pGame->GetGameClientNub()->GetGameClientChannel() : NULL;
		if (pChannel)
			return pChannel->GetNetChannel();
	}

	return NULL;
}

IGameObject* CCryAction::GetGameObject(EntityId id)
{
	if (IEntity* pEnt = gEnv->pEntitySystem->GetEntity(id))
		if (CGameObject* pGameObject = (CGameObject*) pEnt->GetProxy(ENTITY_PROXY_USER))
			return pGameObject;

	return NULL;
}

bool CCryAction::GetNetworkSafeClassId(uint16& id, const char* className)
{
	if (CGameContext* pGameContext = GetGameContext())
		return pGameContext->ClassIdFromName(id, className);
	else
		return false;
}

bool CCryAction::GetNetworkSafeClassName(char* className, size_t classNameSizeInBytes, uint16 id)
{
	if (className && classNameSizeInBytes > 0)
	{
		className[0] = 0;
		if (CGameContext* const pGameContext = GetGameContext())
		{
			string name;
			if (pGameContext->ClassNameFromId(name, id))
			{
				cry_strcpy(className, classNameSizeInBytes, name.c_str());
				return true;
			}
		}
	}

	return false;
}

IGameObjectExtension* CCryAction::QueryGameObjectExtension(EntityId id, const char* name)
{
	if (IGameObject* pObj = GetGameObject(id))
		return pObj->QueryExtension(name);
	else
		return NULL;
}

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
CTimeValue CCryAction::GetServerTime()
{
	if (gEnv->bServer)
		return gEnv->pTimer->GetAsyncTime();

	if (CGameClientNub* pGameClientNub = GetGameClientNub())
	{
		if (CGameClientChannel* pGameClientChannel = pGameClientNub->GetGameClientChannel())
		{
			const CTimeValue localTime = gEnv->pTimer->GetAsyncTime();
			const CTimeValue serverTime = localTime + pGameClientChannel->GetClock().GetServerTimeOffset();
			return serverTime;
		}
	}

	return CTimeValue(0.0f);
}
#else
CTimeValue CCryAction::GetServerTime()
{
	if (gEnv->bServer)
		return gEnv->pTimer->GetFrameStartTime();

	return GetClientChannel() ? GetClientChannel()->GetRemoteTime() : CTimeValue(0.0f);
}
#endif

uint16 CCryAction::GetGameChannelId(INetChannel* pNetChannel)
{
	if (gEnv->bServer)
	{
		CGameServerNub* pNub = GetGameServerNub();
		if (!pNub)
			return 0;

		return pNub->GetChannelId(pNetChannel);
	}

	return 0;
}

INetChannel* CCryAction::GetNetChannel(uint16 channelId)
{
	if (gEnv->bServer)
	{
		CGameServerNub* pNub = GetGameServerNub();
		if (!pNub)
			return 0;

		CGameServerChannel* pChannel = pNub->GetChannel(channelId);
		if (pChannel)
			return pChannel->GetNetChannel();
	}

	return 0;
}

void CCryAction::SetServerChannelPlayerId(uint16 channelId, EntityId id)
{
	CGameServerNub* pServerNub = GetGameServerNub();
	CGameServerChannel* pServerChannel = pServerNub ? pServerNub->GetChannel(channelId) : nullptr;
	if (pServerChannel)
	{
		pServerChannel->SetPlayerId(id);
	}
}

const SEntitySchedulingProfiles* CCryAction::GetEntitySchedulerProfiles(IEntity* pEnt)
{
	return m_pGameObjectSystem->GetEntitySchedulerProfiles(pEnt);
}

bool CCryAction::IsChannelOnHold(uint16 channelId)
{
	if (CGameServerNub* pNub = GetGameServerNub())
		if (CGameServerChannel* pServerChannel = pNub->GetChannel(channelId))
			return pServerChannel->IsOnHold();

	return false;
}

CGameContext* CCryAction::GetGameContext()
{
	return m_pGame ? m_pGame->GetGameContext() : NULL;
}

void CCryAction::RegisterFactory(const char* name, IActorCreator* pCreator, bool isAI)
{
	m_pActorSystem->RegisterActorClass(name, pCreator, isAI);
}

void CCryAction::RegisterFactory(const char* name, IItemCreator* pCreator, bool isAI)
{
	if (m_pItemSystem)
		m_pItemSystem->RegisterItemClass(name, pCreator);
}

void CCryAction::RegisterFactory(const char* name, IVehicleCreator* pCreator, bool isAI)
{
	if (m_pVehicleSystem)
		m_pVehicleSystem->RegisterVehicleClass(name, pCreator, isAI);
}

void CCryAction::RegisterFactory(const char* name, IGameObjectExtensionCreator* pCreator, bool isAI)
{
	m_pGameObjectSystem->RegisterExtension(name, pCreator, NULL);
}

IActionMapManager* CCryAction::GetIActionMapManager()
{
	return m_pActionMapManager;
}

IUIDraw* CCryAction::GetIUIDraw()
{
	return m_pUIDraw;
}

IMannequin& CCryAction::GetMannequinInterface()
{
	CRY_ASSERT(m_pMannequin != NULL);
	return *m_pMannequin;
}

ILevelSystem* CCryAction::GetILevelSystem()
{
	return m_pLevelSystem;
}

IActorSystem* CCryAction::GetIActorSystem()
{
	return m_pActorSystem;
}

IItemSystem* CCryAction::GetIItemSystem()
{
	return m_pItemSystem;
}

IBreakReplicator* CCryAction::GetIBreakReplicator()
{
	return CBreakReplicator::GetIBreakReplicator();
}

IVehicleSystem* CCryAction::GetIVehicleSystem()
{
	return m_pVehicleSystem;
}

ISharedParamsManager* CCryAction::GetISharedParamsManager()
{
	return m_pSharedParamsManager;
}

IGame* CCryAction::GetIGame()
{
	return m_externalGameLibrary.IsValid() ? m_externalGameLibrary.pGame : nullptr;
}

IViewSystem* CCryAction::GetIViewSystem()
{
	return m_pViewSystem;
}

IGameplayRecorder* CCryAction::GetIGameplayRecorder()
{
	return m_pGameplayRecorder;
}

IGameRulesSystem* CCryAction::GetIGameRulesSystem()
{
	return m_pGameRulesSystem;
}

IGameObjectSystem* CCryAction::GetIGameObjectSystem()
{
	return m_pGameObjectSystem;
}

IGameTokenSystem* CCryAction::GetIGameTokenSystem()
{
	return gEnv->pFlowSystem->GetIGameTokenSystem();
}

IEffectSystem* CCryAction::GetIEffectSystem()
{
	return m_pEffectSystem;
}

IMaterialEffects* CCryAction::GetIMaterialEffects()
{
	return m_pMaterialEffects;
}

IBreakableGlassSystem* CCryAction::GetIBreakableGlassSystem()
{
	return m_pBreakableGlassSystem;
}

IDialogSystem* CCryAction::GetIDialogSystem()
{
	return m_pDialogSystem;
}

IRealtimeRemoteUpdate* CCryAction::GetIRealTimeRemoteUpdate()
{
	return &CRealtimeRemoteUpdateListener::GetRealtimeRemoteUpdateListener();
}

ITimeDemoRecorder* CCryAction::GetITimeDemoRecorder() const
{
	return m_pTimeDemoRecorder;
}

IPlayerProfileManager* CCryAction::GetIPlayerProfileManager()
{
	return m_pPlayerProfileManager;
}

IGameVolumes* CCryAction::GetIGameVolumesManager() const
{
	return m_pGameVolumesManager;
}

void CCryAction::PreloadAnimatedCharacter(IScriptTable* pEntityScript)
{
	animatedcharacter::Preload(pEntityScript);
}

//------------------------------------------------------------------------
// NOTE: This function must be thread-safe.
void CCryAction::PrePhysicsTimeStep(float deltaTime)
{
	if (m_pVehicleSystem)
		m_pVehicleSystem->OnPrePhysicsTimeStep(deltaTime);
}

void CCryAction::RegisterExtension(ICryUnknownPtr pExtension)
{
	CRY_ASSERT(pExtension != NULL);

	m_frameworkExtensions.push_back(pExtension);
}

void CCryAction::ReleaseExtensions()
{
	// Delete framework extensions in the reverse order of creation to avoid dependencies crashing
	while (m_frameworkExtensions.size() > 0)
	{
		m_frameworkExtensions.pop_back();
	}
}

ICryUnknownPtr CCryAction::QueryExtensionInterfaceById(const CryInterfaceID& interfaceID) const
{
	ICryUnknownPtr pExtension;
	for (TFrameworkExtensions::const_iterator it = m_frameworkExtensions.begin(), endIt = m_frameworkExtensions.end(); it != endIt; ++it)
	{
		if ((*it)->GetFactory()->ClassSupports(interfaceID))
		{
			pExtension = *it;
			break;
		}
	}

	return pExtension;
}

void CCryAction::ClearTimers()
{
	m_pCallbackTimer->Clear();
}

IGameFramework::TimerID CCryAction::AddTimer(CTimeValue interval, bool repeating, TimerCallback callback, void* userdata)
{
	return m_pCallbackTimer->AddTimer(interval, repeating, callback, userdata);
}

void* CCryAction::RemoveTimer(TimerID timerID)
{
	return m_pCallbackTimer->RemoveTimer(timerID);
}

const char* CCryAction::GetLevelName()
{
	if (gEnv->IsEditor())
	{
		char* levelFolder = NULL;
		char* levelName = NULL;
		GetEditorLevel(&levelName, &levelFolder);

		return levelName;
	}
	else
	{
		if (StartedGameContext() || (m_pGame && m_pGame->IsIniting()))
			if (ILevelInfo* pLevelInfo = GetILevelSystem()->GetCurrentLevel())
				return pLevelInfo->GetName();
	}

	return NULL;
}

void CCryAction::GetAbsLevelPath(char* const pPathBuffer, const uint32 pathBufferSize)
{
	// initial (bad implementation) - need to be improved by Craig
	CRY_ASSERT(pPathBuffer);

	if (gEnv->IsEditor())
	{
		char* levelFolder = 0;

		GetEditorLevel(0, &levelFolder);

		// todo: abs path
		cry_strcpy(pPathBuffer, pathBufferSize, levelFolder);

		return;
	}

	if (StartedGameContext())
	{
		if (ILevelInfo* pLevelInfo = GetILevelSystem()->GetCurrentLevel())
		{
			// todo: abs path
			cry_sprintf(pPathBuffer, pathBufferSize, "%s/%s", PathUtil::GetGameFolder().c_str(), pLevelInfo->GetPath());
			return;
		}
	}

	// no level loaded
	cry_strcpy(pPathBuffer, pathBufferSize, "");
}

bool CCryAction::IsInLevelLoad()
{
	if (CGameContext* pGameContext = GetGameContext())
		return pGameContext->IsInLevelLoad();
	return false;
}

bool CCryAction::IsLoadingSaveGame()
{
	if (CGameContext* pGameContext = GetGameContext())
		return pGameContext->IsLoadingSaveGame();
	return false;
}

bool CCryAction::CanCheat()
{
	return !gEnv->bMultiplayer || gEnv->pSystem->IsDevMode();
}

IPersistantDebug* CCryAction::GetIPersistantDebug()
{
	return m_pPersistantDebug;
}

IGameStatsConfig* CCryAction::GetIGameStatsConfig()
{
	return m_pGameStatsConfig;
}

void CCryAction::TestResetCmd(IConsoleCmdArgs* args)
{
	GetCryAction()->Reset(true);
	//	if (CGameContext * pCtx = GetCryAction()->GetGameContext())
	//		pCtx->GetNetContext()->RequestReconfigureGame();
}

void CCryAction::TestPlayerBoundsCmd(IConsoleCmdArgs* args)
{
	TServerChannelMap* pChannelMap = GetCryAction()->GetGameServerNub()->GetServerChannelMap();
	for (TServerChannelMap::iterator iter = pChannelMap->begin(); iter != pChannelMap->end(); ++iter)
	{
		IActor* pActor = CCryAction::GetCryAction()->m_pActorSystem->GetActorByChannelId(iter->first);

		if (!pActor)
			continue;

		AABB aabb, aabb2;
		IEntity* pEntity = pActor->GetEntity();
		if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(0))
		{
			if (pCharInstance->GetISkeletonAnim())
			{
				aabb = pCharInstance->GetAABB();
				CryLog("%s ca_local_bb (%.4f %.4f %.4f) (%.4f %.4f %.4f)", pEntity->GetName(), aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
				aabb.min = pEntity->GetWorldTM() * aabb.min;
				aabb.max = pEntity->GetWorldTM() * aabb.max;
				pEntity->GetWorldBounds(aabb2);

				CryLog("%s ca_bb (%.4f %.4f %.4f) (%.4f %.4f %.4f)", pEntity->GetName(), aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
				CryLog("%s es_bb (%.4f %.4f %.4f) (%.4f %.4f %.4f)", pEntity->GetName(), aabb2.min.x, aabb2.min.y, aabb2.min.z, aabb2.max.x, aabb2.max.y, aabb2.max.z);
			}
		}
	}
}

void CCryAction::DelegateCmd(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 1)
	{
		IEntitySystem* pEntitySys = gEnv->pEntitySystem;
		IEntity* pEntity = pEntitySys->FindEntityByName(args->GetArg(1));
		if (pEntity)
		{
			INetChannel* pCh = 0;
			if (args->GetArgCount() > 2)
				pCh = CCryAction::GetCryAction()->GetNetChannel(atoi(args->GetArg(2)));
			CCryAction::GetCryAction()->GetNetContext()->DelegateAuthority(pEntity->GetId(), pCh);
		}
	}
}

void CCryAction::DumpStatsCmd(IConsoleCmdArgs* args)
{
	CActionGame* pG = CActionGame::Get();
	if (!pG)
		return;
	pG->DumpStats();
}

void CCryAction::AddBreakEventListener(IBreakEventListener* pListener)
{
	assert(m_pBreakEventListener == NULL);
	m_pBreakEventListener = pListener;
}

void CCryAction::RemoveBreakEventListener(IBreakEventListener* pListener)
{
	assert(m_pBreakEventListener == pListener);
	m_pBreakEventListener = NULL;
}

void CCryAction::OnBreakEvent(uint16 uBreakEventIndex)
{
	if (m_pBreakEventListener)
	{
		m_pBreakEventListener->OnBreakEvent(uBreakEventIndex);
	}
}

void CCryAction::OnPartRemoveEvent(int32 iPartRemoveEventIndex)
{
	if (m_pBreakEventListener)
	{
		m_pBreakEventListener->OnPartRemoveEvent(iPartRemoveEventIndex);
	}
}

void CCryAction::RegisterListener(IGameFrameworkListener* pGameFrameworkListener, const char* name, EFRAMEWORKLISTENERPRIORITY eFrameworkListenerPriority)
{
	// listeners must be unique
	for (int i = 0; i < m_pGFListeners->size(); ++i)
	{
		if ((*m_pGFListeners)[i].pListener == pGameFrameworkListener && m_validListeners[i])
		{
			//CRY_ASSERT(false);
			return;
		}
	}

	SGameFrameworkListener listener;
	listener.pListener = pGameFrameworkListener;
	listener.name = name;
	listener.eFrameworkListenerPriority = eFrameworkListenerPriority;

	TGameFrameworkListeners::iterator iter = m_pGFListeners->begin();
	std::vector<bool>::iterator vit = m_validListeners.begin();
	for (; iter != m_pGFListeners->end(); ++iter, ++vit)
	{
		if ((*iter).eFrameworkListenerPriority > eFrameworkListenerPriority)
		{
			m_pGFListeners->insert(iter, listener);
			m_validListeners.insert(vit, true);
			return;
		}
	}
	m_pGFListeners->push_back(listener);
	m_validListeners.push_back(true);
}

void CCryAction::UnregisterListener(IGameFrameworkListener* pGameFrameworkListener)
{
	if (m_pGFListeners == nullptr)
	{
		return;
	}

	for (int i = 0; i < m_pGFListeners->size(); i++)
	{
		if ((*m_pGFListeners)[i].pListener == pGameFrameworkListener)
		{
			(*m_pGFListeners)[i].pListener = NULL;
			m_validListeners[i] = false;
			return;
		}
	}
}

CGameStatsConfig* CCryAction::GetGameStatsConfig()
{
	return m_pGameStatsConfig;
}

IGameStatistics* CCryAction::GetIGameStatistics()
{
	return m_pGameStatistics;
}

void CCryAction::SetGameSessionHandler(IGameSessionHandler* pSessionHandler)
{
	IGameSessionHandler* currentHandler = m_pGameSessionHandler;
	m_pGameSessionHandler = pSessionHandler;
	if (currentHandler)
	{
		delete currentHandler;
	}
}

void CCryAction::ScheduleEndLevel(const char* nextLevel)
{
	if (GetIGame() == nullptr || !GetIGame()->GameEndLevel(nextLevel))
	{
		ScheduleEndLevelNow(nextLevel);
	}

#ifndef _RELEASE
	gEnv->pSystem->SetLoadOrigin(ISystem::eLLO_Level2Level);
#endif
}

void CCryAction::ScheduleEndLevelNow(const char* nextLevel)
{
	m_bScheduleLevelEnd = true;
	if (!m_pLocalAllocs)
		return;
	m_pLocalAllocs->m_nextLevelToLoad = nextLevel;

#ifndef _RELEASE
	gEnv->pSystem->SetLoadOrigin(ISystem::eLLO_Level2Level);
#endif
}

void CCryAction::CheckEndLevelSchedule()
{
	if (!m_bScheduleLevelEnd)
		return;
	m_bScheduleLevelEnd = false;
	if (m_pLocalAllocs == 0)
	{
		assert(false);
		return;
	}

	const bool bHaveNextLevel = (m_pLocalAllocs->m_nextLevelToLoad.empty() == false);

	IEntity* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity();
	if (pGameRules != 0)
	{
		IScriptSystem* pSS = gEnv->pScriptSystem;
		SmartScriptTable table = pGameRules->GetScriptTable();
		SmartScriptTable params(pSS);
		if (bHaveNextLevel)
			params->SetValue("nextlevel", m_pLocalAllocs->m_nextLevelToLoad.c_str());
		Script::CallMethod(table, "EndLevel", params);
	}

	CALL_FRAMEWORK_LISTENERS(OnLevelEnd(m_pLocalAllocs->m_nextLevelToLoad.c_str()));

	if (bHaveNextLevel)
	{
		CryFixedStringT<256> cmd;
		cmd.Format("map %s nb", m_pLocalAllocs->m_nextLevelToLoad.c_str());
		if (gEnv->IsEditor())
		{
			GameWarning("CCryAction: Suppressing loading of next level '%s' in Editor!", m_pLocalAllocs->m_nextLevelToLoad.c_str());
			m_pLocalAllocs->m_nextLevelToLoad.assign("");
		}
		else
		{
			GameWarning("CCryAction: Loading next level '%s'.", m_pLocalAllocs->m_nextLevelToLoad.c_str());
			m_pLocalAllocs->m_nextLevelToLoad.assign("");
			gEnv->pConsole->ExecuteString(cmd.c_str());

#ifndef _RELEASE
			gEnv->pSystem->SetLoadOrigin(ISystem::eLLO_Level2Level);
#endif
		}
	}
	else
	{
		GameWarning("CCryAction:LevelEnd");
	}
}

#if !defined(_RELEASE)
void CCryAction::CheckConnectRepeatedly()
{
	if (!gEnv->bServer && m_connectRepeatedly.m_enabled)
	{
		float timeSeconds = gEnv->pTimer->GetFrameStartTime().GetSeconds();

		if (timeSeconds > m_connectRepeatedly.m_timeForNextAttempt)
		{
			IConsole* pConsole = gEnv->pConsole;
			CryLogAlways("CCryAction::CheckConnectRepeatedly() currentTime (%f) is greater than timeForNextAttempt (%f). Seeing if another connect attempt is required", timeSeconds, m_connectRepeatedly.m_timeForNextAttempt);

			INetChannel* pCh = GetCryAction()->GetClientChannel();
			if (pCh)
			{
				CryLogAlways("CCryAction::CheckConnectRepeatedly() we have a client channel and hence are connected.");
				m_connectRepeatedly.m_numAttemptsLeft--;

				if (m_connectRepeatedly.m_numAttemptsLeft > 0)
				{
					CryLogAlways("CCryAction::CheckConnectRepeatedly() we still have %d attempts left. Waiting to see if we stay connected", m_connectRepeatedly.m_numAttemptsLeft);
					m_connectRepeatedly.m_timeForNextAttempt = timeSeconds + pConsole->GetCVar("connect_repeatedly_time_between_attempts")->GetFVal();
				}
				else
				{
					CryLogAlways("CCryAction::CheckConnectRepeatedly() we've used up all our attempts. Turning off connect repeated mode");
					m_connectRepeatedly.m_enabled = false;
				}
			}
			else
			{
				CryLogAlways("CCryAction::CheckConnectRepeatedly() we don't have a client channel, hence are NOT connected.");

				m_connectRepeatedly.m_numAttemptsLeft--;

				if (m_connectRepeatedly.m_numAttemptsLeft > 0)
				{
					CryLogAlways("CCryAction::CheckConnectRepeatedly() we still have %d attempts left. Attempting a connect attempt", m_connectRepeatedly.m_numAttemptsLeft);
					m_connectRepeatedly.m_timeForNextAttempt = timeSeconds + pConsole->GetCVar("connect_repeatedly_time_between_attempts")->GetFVal();
					gEnv->pConsole->ExecuteString("connect"); // any server params should have been set on the initial connect and will be used again here
				}
				else
				{
					CryLogAlways("CCryAction::CheckConnectRepeatedly() we've used up all our attempts. Turning off connect repeated mode. We have failed to connect");
					m_connectRepeatedly.m_enabled = false;
				}
			}
		}
	}
}
#endif

void CCryAction::ExecuteCommandNextFrame(const char* cmd)
{
	CRY_ASSERT(m_nextFrameCommand && m_nextFrameCommand->empty());
	(*m_nextFrameCommand) = cmd;
}

const char* CCryAction::GetNextFrameCommand() const
{
	return *m_nextFrameCommand;
}

void CCryAction::ClearNextFrameCommand()
{
	if (!m_nextFrameCommand->empty())
	{
		m_nextFrameCommand->resize(0);
	}
}

void CCryAction::PrefetchLevelAssets(const bool bEnforceAll)
{
	if (m_pItemSystem)
		m_pItemSystem->PrecacheLevel();
}

void CCryAction::ShowPageInBrowser(const char* URL)
{
#if CRY_PLATFORM_WINDOWS
	ShellExecute(0, 0, URL, 0, 0, SW_SHOWNORMAL);
#endif
}

bool CCryAction::StartProcess(const char* cmd_line)
{
#if CRY_PLATFORM_WINDOWS
	//save all stuff
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	return 0 != CreateProcess(NULL, const_cast<char*>(cmd_line), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#else
	return false;
#endif
}

bool CCryAction::SaveServerConfig(const char* path)
{
	class CConfigWriter : public ICVarListProcessorCallback
	{
	public:
		CConfigWriter(const char* path)
		{
			m_file = gEnv->pCryPak->FOpen(path, "wb");
		}
		~CConfigWriter()
		{
			gEnv->pCryPak->FClose(m_file);
		}
		void OnCVar(ICVar* pV)
		{
			string szValue = pV->GetString();

			// replace \ with \\ in the string
			size_t pos = 1;
			for (;; )
			{
				pos = szValue.find_first_of("\\", pos);

				if (pos == string::npos)
				{
					break;
				}

				szValue.replace(pos, 1, "\\\\", 2);
				pos += 2;
			}

			// replace " with \"
			pos = 1;
			for (;; )
			{
				pos = szValue.find_first_of("\"", pos);

				if (pos == string::npos)
				{
					break;
				}

				szValue.replace(pos, 1, "\\\"", 2);
				pos += 2;
			}

			string szLine = pV->GetName();

			if (pV->GetType() == CVAR_STRING)
				szLine += " = \"" + szValue + "\"\r\n";
			else
				szLine += " = " + szValue + "\r\n";

			gEnv->pCryPak->FWrite(szLine.c_str(), szLine.length(), m_file);
		}
		bool IsOk() const
		{
			return m_file != 0;
		}
		FILE* m_file;
	};
	CCVarListProcessor p(PathUtil::GetGameFolder() + "/Scripts/Network/server_cvars.txt");
	CConfigWriter cw(path);
	if (cw.IsOk())
	{
		p.Process(&cw);
		return true;
	}
	return false;
}

void CCryAction::OnActionEvent(const SActionEvent& ev)
{
	switch (ev.m_event)
	{
	case eAE_loadLevel:
		{
			if (!m_pCallbackTimer)
				m_pCallbackTimer = new CallbackTimer();
			if (m_pTimeDemoRecorder)
				m_pTimeDemoRecorder->Reset();
		}
		break;

	case eAE_unloadLevel:
		{
			if (gEnv->pRenderer)
			{
				m_pColorGradientManager->Reset();
			}
		}
		break;

	case eAE_inGame:
		m_levelStartTime = gEnv->pTimer->GetFrameStartTime();
		break;

	default:
		break;
	}

	CALL_FRAMEWORK_LISTENERS(OnActionEvent(ev));

	switch (ev.m_event)
	{
	case eAE_postUnloadLevel:
		if (!gEnv->IsEditor())
			SAFE_DELETE(m_pCallbackTimer);
		break;
	default:
		break;
	}
}

INetNub* CCryAction::GetServerNetNub()
{
	return m_pGame ? m_pGame->GetServerNetNub() : 0;
}

IGameServerNub* CCryAction::GetIGameServerNub()
{
	return GetGameServerNub();
}

CGameServerNub* CCryAction::GetGameServerNub()
{
	return m_pGame ? m_pGame->GetGameServerNub() : NULL;
}

IGameClientNub* CCryAction::GetIGameClientNub()
{
	return GetGameClientNub();
}

CGameClientNub* CCryAction::GetGameClientNub()
{
	return m_pGame ? m_pGame->GetGameClientNub() : NULL;
}

INetNub* CCryAction::GetClientNetNub()
{
	return m_pGame ? m_pGame->GetClientNetNub() : 0;
}

void CCryAction::NotifyGameFrameworkListeners(ISaveGame* pSaveGame)
{
	CALL_FRAMEWORK_LISTENERS(OnSaveGame(pSaveGame));
}

void CCryAction::NotifyGameFrameworkListeners(ILoadGame* pLoadGame)
{
	CALL_FRAMEWORK_LISTENERS(OnLoadGame(pLoadGame));
}

void CCryAction::NotifySavegameFileLoadedToListeners(const char* pLevelName)
{
	CALL_FRAMEWORK_LISTENERS(OnSavegameFileLoadedInMemory(pLevelName));
}

void CCryAction::NotifyForceFlashLoadingListeners()
{
	CALL_FRAMEWORK_LISTENERS(OnForceLoadingWithFlash());
}

void CCryAction::SetGameGUID(const char* gameGUID)
{
	cry_strcpy(m_gameGUID, gameGUID);
}

INetContext* CCryAction::GetNetContext()
{
	//return GetGameContext()->GetNetContext();

	// Julien: This was crashing sometimes when exiting game!
	// I've replaced with a safe pointer access and an assert so that anyone who
	// knows why we were accessing this unsafe pointer->func() can fix it the correct way

	CGameContext* pGameContext = GetGameContext();
	//CRY_ASSERT(pGameContext); - GameContext can be NULL when the game is exiting
	if (!pGameContext)
		return NULL;
	return pGameContext->GetNetContext();
}

void CCryAction::EnableVoiceRecording(const bool enable)
{
	m_VoiceRecordingEnabled = enable ? 1 : 0;
}

IDebugHistoryManager* CCryAction::CreateDebugHistoryManager()
{
	return new CDebugHistoryManager();
}

void CCryAction::GetMemoryUsage(ICrySizer* s) const
{
#ifndef _LIB // Only when compiling as dynamic library
	{
		//SIZER_COMPONENT_NAME(pSizer,"Strings");
		//pSizer->AddObject( (this+1),string::_usedMemory(0) );
	}
	{
		SIZER_COMPONENT_NAME(s, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		s->AddObject((this + 2), (size_t)meminfo.STL_wasted);
	}
#endif

	//s->Add(*this);
#define CHILD_STATISTICS(x) if (x) \
  (x)->GetMemoryStatistics(s)
	s->AddObject(m_pGame);
	s->AddObject(m_pLevelSystem);
	s->AddObject(m_pCustomActionManager);
	s->AddObject(m_pCustomEventManager);
	CHILD_STATISTICS(m_pActorSystem);
	s->AddObject(m_pItemSystem);
	CHILD_STATISTICS(m_pVehicleSystem);
	CHILD_STATISTICS(m_pSharedParamsManager);
	CHILD_STATISTICS(m_pActionMapManager);
	s->AddObject(m_pViewSystem);
	CHILD_STATISTICS(m_pGameRulesSystem);
	CHILD_STATISTICS(m_pUIDraw);
	s->AddObject(m_pGameObjectSystem);
	CHILD_STATISTICS(m_pScriptRMI);
	if (m_pAnimationGraphCvars)
		s->Add(*m_pAnimationGraphCvars);
	s->AddObject(m_pMaterialEffects);
	s->AddObject(m_pBreakableGlassSystem);
	CHILD_STATISTICS(m_pPlayerProfileManager);
	CHILD_STATISTICS(m_pDialogSystem);
	CHILD_STATISTICS(m_pEffectSystem);
	CHILD_STATISTICS(m_pGameSerialize);
	CHILD_STATISTICS(m_pCallbackTimer);
	CHILD_STATISTICS(m_pLanQueryListener);
	CHILD_STATISTICS(m_pDevMode);
	CHILD_STATISTICS(m_pTimeDemoRecorder);
	CHILD_STATISTICS(m_pGameQueryListener);
	CHILD_STATISTICS(m_pGameplayRecorder);
	CHILD_STATISTICS(m_pGameplayAnalyst);
	CHILD_STATISTICS(m_pTimeOfDayScheduler);
	CHILD_STATISTICS(m_pGameStatistics);
	CHILD_STATISTICS(gEnv->pFlashUI);
	s->Add(*m_pScriptA);
	s->Add(*m_pScriptIS);
	s->Add(*m_pScriptAS);
	s->Add(*m_pScriptNet);
	s->Add(*m_pScriptAMM);
	s->Add(*m_pScriptVS);
	s->Add(*m_pScriptBindVehicle);
	s->Add(*m_pScriptBindVehicleSeat);
	s->Add(*m_pScriptInventory);
	s->Add(*m_pScriptBindDS);
	s->Add(*m_pScriptBindMFX);
	s->Add(*m_pScriptBindUIAction);
	s->Add(*m_pMaterialEffectsCVars);
	s->AddObject(*m_pGFListeners);
	s->Add(*m_nextFrameCommand);
}

ISubtitleManager* CCryAction::GetISubtitleManager()
{
	return m_pSubtitleManager;
}

void CCryAction::MutePlayer(IConsoleCmdArgs* pArgs)
{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (pArgs->GetArgCount() != 2)
		return;

	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
	if (pEntity)
	{
		GetCryAction()->MutePlayerById(pEntity->GetId());
	}
#endif
}

void CCryAction::MutePlayerById(EntityId playerId)
{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	IActor* pRequestingActor = gEnv->pGameFramework->GetClientActor();
	if (pRequestingActor && pRequestingActor->IsPlayer())
	{
		IActor* pActor = GetCryAction()->GetIActorSystem()->GetActor(playerId);
		if (pActor && pActor->IsPlayer())
		{
			if (pActor->GetEntityId() != pRequestingActor->GetEntityId())
			{
				IVoiceContext* pVoiceContext = GetCryAction()->GetGameContext()->GetNetContext()->GetVoiceContext();
				bool muted = pVoiceContext->IsMuted(pRequestingActor->GetEntityId(), pActor->GetEntityId());
				pVoiceContext->Mute(pRequestingActor->GetEntityId(), pActor->GetEntityId(), !muted);

				if (!gEnv->bServer)
				{
					SMutePlayerParams muteParams;
					muteParams.requestor = pRequestingActor->GetEntityId();
					muteParams.id = pActor->GetEntityId();
					muteParams.mute = !muted;

					if (CGameClientChannel* pGCC = CCryAction::GetCryAction()->GetGameClientNub()->GetGameClientChannel())
						CGameServerChannel::SendMutePlayerWith(muteParams, pGCC->GetNetChannel());
				}
			}
		}
	}
#endif
}

bool CCryAction::IsImmersiveMPEnabled()
{
	if (CGameContext* pGameContext = GetGameContext())
		return pGameContext->HasContextFlag(eGSF_ImmersiveMultiplayer);
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
bool CCryAction::IsInTimeDemo()
{
	if (m_pTimeDemoRecorder && m_pTimeDemoRecorder->IsTimeDemoActive())
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CCryAction::IsTimeDemoRecording()
{
	if (m_pTimeDemoRecorder && m_pTimeDemoRecorder->IsRecording())
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CCryAction::CanSave()
{
	ICVar* saveLoadEnabled = gEnv->pConsole->GetCVar("g_EnableLoadSave");
	bool enabled = saveLoadEnabled->GetIVal() == 1;

	const bool bViewSystemAllows = m_pViewSystem ? m_pViewSystem->IsPlayingCutScene() == false : true;
	return enabled && bViewSystemAllows && m_bAllowSave && !IsInTimeDemo();
}

//////////////////////////////////////////////////////////////////////////
bool CCryAction::CanLoad()
{
	ICVar* saveLoadEnabled = gEnv->pConsole->GetCVar("g_EnableLoadSave");
	bool enabled = saveLoadEnabled->GetIVal() == 1;

	return enabled && m_bAllowLoad;
}

//////////////////////////////////////////////////////////////////////////
ISerializeHelper* CCryAction::GetSerializeHelper() const
{
	const bool useXMLCPBin = (!CPlayerProfileManager::sUseRichSaveGames && CCryActionCVars::Get().g_useXMLCPBinForSaveLoad == 1);
	if (useXMLCPBin)
		return new CBinarySerializeHelper();

	return new CXmlSerializeHelper();
}

CSignalTimer* CCryAction::GetSignalTimer()
{
	return (&(CSignalTimer::ref()));
}

ICooperativeAnimationManager* CCryAction::GetICooperativeAnimationManager()
{
	return m_pCooperativeAnimationManager;
}

ICheckpointSystem* CCryAction::GetICheckpointSystem()
{
	return(CCheckpointSystem::GetInstance());
}

IForceFeedbackSystem* CCryAction::GetIForceFeedbackSystem() const
{
	return m_pForceFeedBackSystem;
}

ICustomActionManager* CCryAction::GetICustomActionManager() const
{
	return m_pCustomActionManager;
}

ICustomEventManager* CCryAction::GetICustomEventManager() const
{
	return m_pCustomEventManager;
}

IGameSessionHandler* CCryAction::GetIGameSessionHandler()
{
	// If no session handler is set, create a default one
	if (m_pGameSessionHandler == 0)
	{
		m_pGameSessionHandler = new CGameSessionHandler();
	}
	return m_pGameSessionHandler;
}

CRangeSignaling* CCryAction::GetRangeSignaling()
{
	return (&(CRangeSignaling::ref()));
}

IAIActorProxy* CCryAction::GetAIActorProxy(EntityId id) const
{
	assert(m_pAIProxyManager);
	return m_pAIProxyManager->GetAIActorProxy(id);
}

void CCryAction::OnBreakageSpawnedEntity(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity)
{
	CBreakReplicator* pBreakReplicator = CBreakReplicator::Get();
	if (pBreakReplicator)
	{
		pBreakReplicator->OnSpawn(pEntity, pPhysEntity, pSrcPhysEntity);
	}
	if (gEnv->bMultiplayer)
	{
		m_pGame->OnBreakageSpawnedEntity(pEntity, pPhysEntity, pSrcPhysEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CCryAction::IsGameSession(CrySessionHandle sessionHandle)
{
	CrySessionHandle gameSessionHandle = GetIGameSessionHandler()->GetGameSessionHandle();
	return (gameSessionHandle == sessionHandle);
}

//////////////////////////////////////////////////////////////////////////
bool CCryAction::ShouldMigrateNub(CrySessionHandle sessionHandle)
{
	return IsGameSession(sessionHandle) && GetIGameSessionHandler()->ShouldMigrateNub();
}

bool CCryAction::StartedGameContext(void) const
{
	return (m_pGame != 0) && (m_pGame->IsInited());
}

bool CCryAction::StartingGameContext(void) const
{
	return (m_pGame != 0) && (m_pGame->IsIniting());
}

void CCryAction::CreatePhysicsQueues()
{
	if (!m_pPhysicsQueues)
		m_pPhysicsQueues = new CCryActionPhysicQueues();
}

void CCryAction::ClearPhysicsQueues()
{
	SAFE_DELETE(m_pPhysicsQueues);
}

CCryActionPhysicQueues& CCryAction::GetPhysicQueues()
{
	CRY_ASSERT(m_pPhysicsQueues);

	return *m_pPhysicsQueues;
}

bool CCryAction::IsGameSessionMigrating()
{
	const IGameSessionHandler* pSessionHandler = GetIGameSessionHandler();
	return pSessionHandler->IsGameSessionMigrating();
}

void CCryAction::StartNetworkStallTicker(bool includeMinimalUpdate)
{
	if (includeMinimalUpdate)
	{
		gEnv->pNetwork->SyncWithGame(eNGS_AllowMinimalUpdate);
	}

#ifdef USE_NETWORK_STALL_TICKER_THREAD
	if (gEnv->bMultiplayer)
	{
		if (!m_pNetworkStallTickerThread)
		{
			CRY_ASSERT(m_networkStallTickerReferences == 0);

			// Spawn thread to tick needed tasks
			m_pNetworkStallTickerThread = new CNetworkStallTickerThread();

			if (!gEnv->pThreadManager->SpawnThread(m_pNetworkStallTickerThread, "NetworkStallTicker"))
			{
				CryFatalError("Error spawning \"NetworkStallTicker\" thread.");
			}
		}

		m_networkStallTickerReferences++;
	}
#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD
}

void CCryAction::StopNetworkStallTicker()
{
#ifdef USE_NETWORK_STALL_TICKER_THREAD
	if (gEnv->bMultiplayer)
	{
		if (m_networkStallTickerReferences > 0)
		{
			m_networkStallTickerReferences--;
		}

		if (m_networkStallTickerReferences == 0)
		{
			if (m_pNetworkStallTickerThread)
			{
				m_pNetworkStallTickerThread->SignalStopWork();
				gEnv->pThreadManager->JoinThread(m_pNetworkStallTickerThread, eJM_Join);
				delete m_pNetworkStallTickerThread;
				m_pNetworkStallTickerThread = NULL;
			}
		}
	}
#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD

	if (gEnv && gEnv->pNetwork)
	{
		gEnv->pNetwork->SyncWithGame(eNGS_DenyMinimalUpdate);
	}
}

void CCryAction::GoToSegment(int x, int y)
{
}

// TypeInfo implementations for CryAction
#ifndef _LIB
	#include <CryCore/Common_TypeInfo.h>
	#if !CRY_PLATFORM_ORBIS
		#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float> )
		#endif
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float> )
	#endif
#endif
#include <CryCore/TypeInfo_impl.h>
#include "PlayerProfiles/RichSaveGameTypes_info.h"
