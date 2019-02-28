// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 3:8:2004   11:26 : Created by Márcio Martins
   - 17:8:2005        : Modified - NickH: Factory registration moved to GameFactory.cpp

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"
 
#include "UI/UIManager.h"
#include "UI/ProfileOptions.h"
#include "UI/WarningsManager.h"
#include "UI/Menu3dModels/FrontEndModelCache.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/UICVars.h"
#include "UI/HUD/HUDMissionObjectiveSystem.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/Utils/ScreenLayoutManager.h"
#include "UI/UIInput.h"
#include "ScreenResolution.h"

#include "GameRules.h"
#include "Audio/GameAudio.h"
#include "ScreenEffects.h"
#include "WeaponSystem.h"
#include "Effects/GameEffectsSystem.h"
#include "GameInputActionHandlers.h"
#include "GameCache.h"
#include "ItemScheduler.h"
#include "Utility/CryWatch.h"

#include <CrySystem/ICryPluginManager.h>
#include <CrySystem/File/ICryPak.h>
#include <CryString/CryPath.h>
#include <IActionMapManager.h>
#include <IViewSystem.h>
#include <ILevelSystem.h>
#include <IItemSystem.h>
#include <IVehicleSystem.h>
#include <CryMovie/IMovieSystem.h>
#include <IPlayerProfiles.h>
#include <CryLobby/ICryLobbyUI.h>
#include <CrySystem/ILocalizationManager.h>
#include <CrySystem/File/IResourceManager.h>
#include <CryAction/ICustomActions.h>
#include <CryFlowGraph/IFlowBaseNode.h>

#include "ScriptBind_Actor.h"
#include "ScriptBind_Item.h"
#include "ScriptBind_Weapon.h"
#include "ScriptBind_GameRules.h"
#include "ScriptBind_Game.h"
#include "ScriptBind_HitDeathReactions.h"
#include "Boids/ScriptBind_Boids.h"
#include "AI/ScriptBind_GameAI.h"
#include "UI/HUD/ScriptBind_HUD.h"
#include "Environment/ScriptBind_InteractiveObject.h"
#include "Network/Lobby/ScriptBind_MatchMaking.h"
#include "Turret/Turret/ScriptBind_Turret.h"
#include "ScriptBind_ProtectedBinds.h"
#include "Environment/ScriptBind_LightningArc.h"
#include "DLCManager.h"
#include "CornerSmoother.h"
#include <CryInput/IHardwareMouse.h>

#ifdef USE_LAPTOPUTIL
	#include "LaptopUtil.h"
#endif

#include "GameFactory.h"

#include "Player.h"

#include "GameParameters.h"
#include "WeaponSharedParams.h"
#include "ItemResourceCache.h"
#include "ActorManager.h"

#include "ISaveGame.h"
#include "ILoadGame.h"
#include <CryString/CryPath.h>
#include "GameStateRecorder.h"
#include "GodMode.h"
#include "SmokeManager.h"
#include "RecordingSystem.h"
#include "StatsRecordingMgr.h"
#include "PatchPakManager.h"
#include "DataPatchDownloader.h"
#include "LagOMeter.h"
#include "TelemetryCollector.h"
#include "TacticalManager.h"

#include "Environment/LedgeManager.h"
#include "Environment/WaterPuddle.h"

#include "VehicleClient.h"
#include "AI/TacticalPointLanguageExtender.h"
#include "AI/GameAISystem.h"
#include "AI/AICorpse.h"

#include "Network/Lobby/GameUserPackets.h"
#include "Network/Lobby/GameAchievements.h"
#include "Network/Lobby/GameBrowser.h"
#include "Network/Lobby/GameLobbyManager.h"
#include "Network/Lobby/CryLobbySessionHandler.h"
#include "Network/Lobby/GameServerLists.h"
#include "Network/Squad/SquadManager.h"
#include "Network/Lobby/PlaylistActivityTracker.h"

#include "GameCodeCoverage/GameCodeCoverageManager.h"
#include "GameCodeCoverage/GameCodeCoverageGUI.h"

#include "CodeCheckpointDebugMgr.h"

#include "AutoAimManager.h"
#include "HitDeathReactionsSystem.h"
#include "MovementTransitionsSystem.h"
#include "CheckpointGame.h"
#include "BodyManager.h"
#include "StealthKill.h"
#include "VehicleMovementBase.h"

#include "PlayerProgression.h"
#include "PersistantStats.h"
#include "Testing/FeatureTester.h"
#include "GamePhysicsSettings.h"

#include "EquipmentLoadout.h"
#include "UI/Menu3dModels/MenuRender3DModelMgr.h"

#include "MikeBullet.h"
#include "PlaylistManager.h"
#include "DownloadMgr.h"
#include "Effects/GameEffects/HudInterferenceGameEffect.h"
#include "Effects/GameEffects/SceneBlurGameEffect.h"
#include "Effects/GameEffects/LightningGameEffect.h"
#include "Effects/GameEffects/ParameterGameEffect.h"

#include "Stereo3D/StereoFramework.h"

#include "GameMechanismManager/GameMechanismManager.h"
#include <CryCore/Platform/IPlatformOS.h>

#if !defined(_RELEASE)
	#include "Editor/GameRealtimeRemoteUpdate.h"
#endif

#include "GodMode.h"
#include "PlayerVisTable.h"
#include "Network/GameNetworkUtils.h"

#include "SlideController.h"

#include <CrySystem/Profilers/IStatoscope.h>

#include "GameLocalizationManager.h"

#include "RevertibleConfigLoader.h"

#include "StatsEntityIdRegistry.h"

#include "MovingPlatforms/MovingPlatformMgr.h"

#include <CryGame/IGameVolumes.h>

#include <CryLiveCreate/ILiveCreateHost.h>
#include <CryCore/SFunctor.h>

#include "WorldBuilder.h"

#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include "DynamicResponseSystem/ConditionDistanceToEntity.h"
#include "DynamicResponseSystem/GameTokenToDrsTranslator.h"
#include "DynamicResponseSystem/ActionExecuteAudioTrigger.h"
#include <CrySerialization/ClassFactory.h>

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler.h>
#include <CrySystem/CryUnitTest.h>

#include <IPerceptionManager.h>

//#define GAME_DEBUG_MEM  // debug memory usage
#undef  GAME_DEBUG_MEM

#define GAME_ITEMS_DATA_FOLDER            "scripts/entities/items/XML"

#define CRYENGINE_SDK_GUID                "{D860B77B-D6AB-4FB5-BBE3-84EE6B98D6DA}"

#define PRODUCT_VERSION_MAX_STRING_LENGTH (256)

#if CRY_PLATFORM_DURANGO
	#include "XboxOneLive/XboxLiveGameEvents.h"
#endif

#ifndef _LIB
	#include <CryCore/Common_TypeInfo.h>
//STRUCT_INFO_T_INSTANTIATE(Vec3_tpl, <float>)
STRUCT_INFO_T_INSTANTIATE(Quat_tpl, <float> )
	#if CRY_PLATFORM_ANDROID
STRUCT_INFO_T_INSTANTIATE(QuatT_tpl, <float> )
	#endif
#endif

#if CRY_PLATFORM_WINDOWS
	#define   DISABLE_FORCE_FEEDBACK_WHEN_USING_MOUSE_AND_KEYBOARD 1
#else
	#define   DISABLE_FORCE_FEEDBACK_WHEN_USING_MOUSE_AND_KEYBOARD 0
#endif

CGame* g_pGame = 0;
SCVars* g_pGameCVars = 0;
CGameActions* g_pGameActions = 0;
CTacticalPointLanguageExtender g_tacticalPointLanguageExtender;

static CRevertibleConfigLoader s_gameModeCVars(96, 5120); // 5k - needs to hold enough room for patched cvars as well as multiplayer.cfg
static CRevertibleConfigLoader s_levelCVars(20, 1024);

static void OnChangedStereoRenderDevice(ICVar* pStereoRenderDevice);

#ifndef NO_LIVECREATE
void LiveCreateUpdate()
{
	if (!gEnv)
	{
		return;
	}

	if (gEnv->IsEditor())
	{
		return;
	}

	if (gEnv->pLiveCreateHost)
	{
		gEnv->pLiveCreateHost->ExecuteCommands();
	}
}
#endif

CCountdownTimer::CCountdownTimer(FN_COMPLETION_CALLBACK fnComplete) :
	m_fnCompleted(fnComplete),
	m_timeLeft(0.0f)
{
}

void CCountdownTimer::Start(float countdownDurationSeconds)
{
	m_timeLeft = countdownDurationSeconds;
}

void CCountdownTimer::Advance(float dt)
{
	if (m_timeLeft > 0.0f)
	{
		m_timeLeft -= dt;

		if (m_timeLeft <= 0.0f)
		{
			if (m_fnCompleted != NULL)
			{
				m_fnCompleted();
			}
		}
	}
}

bool CCountdownTimer::IsActive()
{
	return m_timeLeft > 0.0f;
}

void CCountdownTimer::Abort()
{
	m_timeLeft = 0.0f;
}

float CCountdownTimer::ToSeconds()
{
	return m_timeLeft;
}

#if ENABLE_VISUAL_DEBUG_PROTOTYPE

class CVisualDebugPrototype
{
	std::vector<ITexture*> m_Textures;

	struct DrawCallData
	{
		uint32 lastDrawCallCount;
		float  redValue;
		float  lastAlpha;
		bool   up;

		DrawCallData() : lastDrawCallCount(0), redValue(0.0f), lastAlpha(0.0f), up(false)
		{}
	};

	int          m_Enabled;
	DrawCallData m_DrawCallData;

public:

	CVisualDebugPrototype()
	{
		REGISTER_CVAR2("g_VisualDebugEnable", &m_Enabled, 1, VF_CHEAT, "Set AI features to behave in earlier milestones - please use sparingly");

		ITexture* pTex = gEnv->pRenderer->EF_LoadTexture("Textures/GVDS/TooMany.tif", FT_DONT_STREAM);
		if (pTex)
		{
			m_Textures.push_back(pTex);
		}

		pTex = gEnv->pRenderer->EF_LoadTexture("Textures/GVDS/TooMany2.tif", FT_DONT_STREAM);
		if (pTex)
		{
			m_Textures.push_back(pTex);
		}

	}

	~CVisualDebugPrototype()
	{
		for (std::vector<ITexture*>::iterator it = m_Textures.begin(); m_Textures.end() != it; ++it)
		{
			if (*it)
			{
				(*(it))->Release();
			}
		}

		m_Textures.clear();
	}

	void Update(float deltaT)
	{
		if (m_Enabled != 4674)
		{
			if (gEnv->IsEditor())
			{
				CheckDrawCalls(deltaT);
			}
		}
	}

	void CheckDrawCalls(float deltaT)
	{
		if (m_Textures.size() < 2 || !m_Textures[0] || !m_Textures[1])
		{
			return;
		}

		int nDrawCalls = gEnv->pRenderer->GetCurrentNumberOfDrawCalls();

		if (3000 < nDrawCalls)
		{
			//gEnv->pRenderer->SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
			//gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);

			float alpha = max(0.0f, min(1.0f, (nDrawCalls - 3000) / 2000.0f));
			float reddy = max(0.0f, min(1.0f, (nDrawCalls - 4000) / 3000.0f));

			m_DrawCallData.lastAlpha = (m_DrawCallData.lastAlpha + alpha) * 0.5f;
			//m_DrawCallData.lastAlpha = m_DrawCallData.lastAlpha * 0.9f;

			if (reddy > m_DrawCallData.redValue && m_DrawCallData.up)
			{
				m_DrawCallData.redValue += (reddy - m_DrawCallData.redValue) * deltaT;

				if (reddy < m_DrawCallData.redValue)
				{
					m_DrawCallData.up = false;
				}
				else if (m_DrawCallData.redValue > 0.3f)
				{
					m_DrawCallData.redValue = 0.3f;
					m_DrawCallData.up = false;
				}
			}
			else if (!m_DrawCallData.up)
			{
				m_DrawCallData.redValue -= 0.1f * deltaT;

				if (m_DrawCallData.redValue < 0.0f)
				{
					m_DrawCallData.redValue = 0.0f;
					m_DrawCallData.up = true;
				}
			}
			else
			{
				m_DrawCallData.up = false;
			}

			float w = static_cast<float>(gEnv->pRenderer->GetWidth());
			float h = static_cast<float>(gEnv->pRenderer->GetHeight());

			float scalef = .4f;
			//x = w * 0.25f - m_Textures[0]->GetWidth() * scalef * 0.5f;
			//y = h * 0.5f - m_Textures[0]->GetHeight() * scalef * 0.5f;

			IRenderAuxImage::Draw2dImage(0.f, 200.f, m_Textures[0]->GetWidth() * scalef, m_Textures[0]->GetHeight() * scalef, m_Textures[0]->GetTextureID(), 0.0f, 0.0f, 1.0f, 1.0f, 180.0f, 1.0f, 1.0f - m_DrawCallData.redValue, 1.0f - m_DrawCallData.redValue, m_DrawCallData.lastAlpha, 0.9f);
			IRenderAuxImage::Draw2dImage(0.f, 200.f, m_Textures[1]->GetWidth() * scalef, m_Textures[1]->GetHeight() * scalef, m_Textures[1]->GetTextureID(), 0.0f, 0.0f, 1.0f, 1.0f, 180.0f, 1.0f, 1.0f, 1.0f, m_DrawCallData.redValue, 1.0f);
		}
		else
		{
			m_DrawCallData.redValue = 0.0f;
		}
	}
};

#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE

CGame::CGame()
	: m_pConsole(0),
	m_pWeaponSystem(0),
	m_pGamePhysicsSettings(0),
	m_pScriptBindActor(0),
	m_pScriptBindGame(0),
	m_pScriptBindHitDeathReactions(0),
	m_pScriptBindBoids(0),
	m_pScriptBindTurret(0),
	m_pPlayerProfileManager(0),
	m_pGameAudio(0),
	m_pScreenEffects(0),
	m_pPersistantStats(0),
#ifdef USE_LAPTOPUTIL
	m_pLaptopUtil(0),
#endif
	m_pLedgeManager(0),
	m_pWaterPuddleManager(0),
	m_pRecordingSystem(0),
	m_pEquipmentLoadout(0),
	m_pPlayerProgression(0),
	m_statsRecorder(0),
	m_patchPakManager(NULL),
	m_pMatchMakingTelemetry(NULL),
	m_pDataPatchDownloader(0),
	m_pGameLocalizationManager(0),
	m_pGameStateRecorder(0),
#if USE_LAGOMETER
	m_pLagOMeter(0),
#endif
	m_telemetryCollector(0),
	m_pPlaylistActivityTracker(0),
#if USE_TELEMETRY_BUFFERS
	m_performanceBuffer(0),
	m_bandwidthBuffer(0),
	m_memoryTrackingBuffer(0),
	m_soundTrackingBuffer(0),
	m_secondTimePerformance((int64)0),
	m_secondTimeMemory((int64)0),
	m_secondTimeBandwidth((int64)0),
	m_secondTimeSound((int64)0),
#endif //#if USE_TELEMETRY_BUFFERS
	m_pGameBrowser(0),
	m_pGameLobbyManager(0),
#if IMPLEMENT_PC_BLADES
	m_pGameServerLists(NULL),
#endif // IMPLEMENT_PC_BLADES
	m_pSquadManager(0),
	m_pLobbySessionHandler(0),
	m_pUIManager(NULL),
	m_pTacticalManager(NULL),
	m_pAutoAimManager(NULL),
	m_pHitDeathReactionsSystem(NULL),
	m_pBodyDamageManager(NULL),
	m_pPlaylistManager(NULL),
	m_pMovementTransitionsSystem(NULL),
	m_pScriptBindInteractiveObject(NULL),
	m_previousInputControllerDeviceIndex(0),
	m_currentXboxLivePartySize(0),
	m_hasExclusiveController(false),
	m_bExclusiveControllerConnected(false),
	m_bPausedForControllerDisconnect(false),
	m_bPausedForSystemMenu(false),
	m_bDeferredSystemMenuPause(false),
	m_previousPausedGameState(false),
	m_wasGamePausedBeforePLMForcedPause(false),
	m_hostMigrationState(eHMS_NotMigrating),
	m_hostMigrationTimeStateChanged(0.f),
	m_pDownloadMgr(NULL),
	m_pDLCManager(NULL),
	m_pInputEventListenerOverride(NULL),
	m_randomGenerator(gEnv->bNoRandomSeed ? 0 : (uint32)gEnv->pTimer->GetAsyncTime().GetValue()),
	m_pGameAISystem(NULL),
	m_pRayCaster(NULL),
	m_pGameAchievements(NULL),
	m_pIntersectionTester(NULL),
	m_pGameActionHandlers(NULL),
	m_pGameCache(NULL),
	m_gameTypeMultiplayer(false),
	m_gameTypeInitialized(false),
	m_needsInitPatchables(false),
	m_settingRichPresence(false),
	m_bSignInOrOutEventOccured(false),
	m_iCachedGsmValue(0),
	m_fCachedGsmRangeValue(0.f),
	m_fCachedGsmRangeStepValue(0.f),
	m_pHudInterferenceGameEffect(NULL),
	m_pSceneBlurGameEffect(NULL),
	m_pLightningGameEffect(0),
	m_pParameterGameEffect(NULL),
	m_saveIconMode(eSIM_Off),
	m_saveIconTimer(0.0f),
	m_bUserHasPhysicalStorage(false),
	m_bCheckPointSave(false),
	m_pStatsEntityIdRegistry(NULL),
	m_pGameTokenSignalCreator(NULL),
#if CRY_PLATFORM_DURANGO
	m_userChangedDoSignOutAndIn(false),
#endif
	m_pMovingPlatformMgr(NULL),
	m_stereoOutputFunctorId(0)
{
	static_assert(eCGE_Last <= 64, "Unexpected enum value!");

	m_pCVars = new SCVars();
	g_pGameCVars = m_pCVars;
	m_pGameActions = new CGameActions();
	g_pGameActions = m_pGameActions;
	m_pTacticalManager = new CTacticalManager();
	m_pBurnEffectManager = new CBurnEffectManager();
	g_pGame = this;
	m_bReload = false;
	m_inDevMode = false;
	m_userProfileChanged = true;
	m_bLastSaveDirty = true;
	m_editorDisplayHelpers = true;
	m_RenderingToHMD = false;

	ICVar* pStereoOutput(gEnv->pConsole->GetCVar("r_StereoOutput"));
	if (pStereoOutput)
	{
		SetRenderingToHMD(pStereoOutput->GetIVal() == 7);// 7 means HMD.
		SFunctor oFunctor;
		oFunctor.Set(OnChangedStereoRenderDevice, pStereoOutput);
		m_stereoOutputFunctorId = pStereoOutput->AddOnChangeFunctor(oFunctor);
	}
	else
	{
		SetRenderingToHMD(false);
	}

	m_gameMechanismManager = new CGameMechanismManager();

#if ENABLE_GAME_CODE_COVERAGE
	new CGameCodeCoverageManager("Scripts/gameCodeCoverage.xml");
	new CGameCodeCoverageGUI();
#endif

#if !defined(_RELEASE)
	m_pRemoteUpdateListener = &CGameRealtimeRemoteUpdateListener::GetGameRealtimeRemoteUpdateListener();
#endif

#if ENABLE_VISUAL_DEBUG_PROTOTYPE
	m_VisualDebugSys = new CVisualDebugPrototype();
#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE

	SetInviteAcceptedState(eIAS_None);
	InvalidateInviteData();

	CAnimationProxyDualCharacterBase::Load1P3PPairFile();

	m_bLoggedInFromInvite = false;
	m_updateRichPresenceTimer = 0.f;

	m_pendingRichPresenceSessionID = CrySessionInvalidID;
	m_currentRichPresenceSessionID = CrySessionInvalidID;

	m_bRefreshRichPresence = false;

	m_gameDataInstalled = true;
	m_postLocalisationBootChecksDone = true;

	m_cachedUserRegion = -1;
}

CGame::~CGame()
{
	// Remove the functor we added earlier
	if (ICVar* pStereoOutput = gEnv->pConsole->GetCVar("r_StereoOutput"))
	{
		pStereoOutput->RemoveOnChangeFunctor(m_stereoOutputFunctorId);
	}

	gEnv->pGameFramework->EndGameContext();
	gEnv->pGameFramework->UnregisterListener(this);

	if (IPlatformOS* pPlatformOS = GetISystem()->GetPlatformOS())
	{
		pPlatformOS->RemoveListener(this);
	}

	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	ReleaseScriptBinds();

	if (gEnv->pDynamicResponseSystem)
	{
		DRS::IResponseActor* pDrsActor = gEnv->pDynamicResponseSystem->GetResponseActor("GameTokenSignalSender");
		if (pDrsActor)
		{
			gEnv->pDynamicResponseSystem->ReleaseResponseActor(pDrsActor);
		}
	}

	CAnimationProxyDualCharacterBase::ReleaseBuffers();

	SAFE_DELETE_GAME_EFFECT(m_pHudInterferenceGameEffect);
	SAFE_DELETE_GAME_EFFECT(m_pSceneBlurGameEffect);
	SAFE_DELETE_GAME_EFFECT(m_pLightningGameEffect);
	SAFE_DELETE_GAME_EFFECT(m_pParameterGameEffect);

#ifdef USE_LAPTOPUTIL
	SAFE_DELETE(m_pLaptopUtil);
#endif
	SAFE_DELETE(m_pGameAudio);
	SAFE_DELETE(m_pScreenEffects);
	SAFE_DELETE(m_pPersistantStats);
	SAFE_DELETE(m_pEquipmentLoadout);
	SAFE_DELETE(m_pPlayerProgression);
	SAFE_DELETE(m_pGameAISystem);
	SAFE_DELETE(m_pPlayerVisTable);
	SAFE_DELETE(m_pRayCaster);
	SAFE_DELETE(m_pIntersectionTester);
	SAFE_DELETE(m_pGameActionHandlers);
	SAFE_DELETE(m_pGameCache);
	SAFE_RELEASE(m_pWeaponSystem);
	SAFE_DELETE(m_pGamePhysicsSettings);
	SAFE_DELETE(m_pItemStrings);
	SAFE_DELETE(m_pGameParametersStorage);
	SAFE_DELETE(m_pUIManager);
	SAFE_DELETE(m_pLedgeManager);
	SAFE_DELETE(m_pWaterPuddleManager);
	SAFE_DELETE(m_pRecordingSystem);
	SAFE_DELETE(m_statsRecorder);
	SAFE_DELETE(m_patchPakManager);
	SAFE_DELETE(m_pDataPatchDownloader);
	SAFE_DELETE(m_pGameLocalizationManager);
	SAFE_RELEASE(m_pGameStateRecorder);
	SAFE_DELETE(m_pGameTokenSignalCreator);
#if USE_LAGOMETER
	SAFE_DELETE(m_pLagOMeter);
#endif
	SAFE_DELETE(m_telemetryCollector);
	SAFE_DELETE(m_pPlaylistActivityTracker);
#if USE_TELEMETRY_BUFFERS
	SAFE_DELETE(m_performanceBuffer);
	SAFE_DELETE(m_bandwidthBuffer);
	SAFE_DELETE(m_memoryTrackingBuffer);
	SAFE_DELETE(m_soundTrackingBuffer);
#endif
	SAFE_DELETE(m_pGameBrowser);
	SAFE_DELETE(m_pGameLobbyManager);
#if IMPLEMENT_PC_BLADES
	SAFE_DELETE(m_pGameServerLists);
#endif //IMPLEMENT_PC_BLADES
	SAFE_DELETE(m_pSquadManager);
	SAFE_DELETE(m_pTacticalManager);
	SAFE_DELETE(m_pBurnEffectManager);
	SAFE_DELETE(m_pAutoAimManager);
	SAFE_DELETE(m_pHitDeathReactionsSystem);
	SAFE_DELETE(m_pBodyDamageManager);
	SAFE_DELETE(m_pPlaylistManager);
	SAFE_DELETE(m_pDLCManager);
	SAFE_DELETE(m_pMovementTransitionsSystem);
	SAFE_DELETE(m_pStatsEntityIdRegistry);
	SAFE_DELETE(m_pMovingPlatformMgr);
	SAFE_DELETE(m_pMatchMakingTelemetry);
	SAFE_DELETE(m_pWorldBuilder);

	if (m_pLobbySessionHandler != NULL)
	{
		// Clear the pointer in CryAction this will also call delete on m_pLobbySessionHandler and set it to NULL
		ClearGameSessionHandler();
	}

	if (gEnv->pNetwork && gEnv->pNetwork->GetLobby())
	{
		ICryLobby* pLobby = gEnv->pNetwork->GetLobby();

		pLobby->UnregisterEventInterest(eCLSE_PartyMembers, CGame::PartyMembersCallback, this);
		pLobby->UnregisterEventInterest(eCLSE_UserProfileChanged, CGame::UserProfileChangedCallback, this);
		pLobby->UnregisterEventInterest(eCLSE_InviteAccepted, CGame::InviteAcceptedCallback, this);
		pLobby->UnregisterEventInterest(eCLSE_OnlineState, CGame::OnlineStateCallback, this);
		pLobby->UnregisterEventInterest(eCLSE_EthernetState, CGame::EthernetStateCallback, this);
		pLobby->UnregisterEventInterest(eCLSE_InviteAccepted, CGame::InviteAcceptedCallback, this);

		if (gEnv && gEnv->pNetwork)
		{
			gEnv->pNetwork->SetMultithreadingMode(INetwork::NETWORK_MT_OFF);    // Kill the network thread then its safe to kill the lobby
		}

		if (pLobby->GetLobbyService(eCLS_LAN))
		{
			pLobby->Terminate(eCLS_LAN, eCLSO_All, NULL, NULL);
		}

		if (pLobby->GetLobbyService(eCLS_Online))
		{
			pLobby->Terminate(eCLS_Online, eCLSO_All, NULL, NULL);
		}
	}

	GAME_FX_SYSTEM.Destroy();

	ScreenResolution::ReleaseScreenResolutions();

	if (gEnv && gEnv->pInput)
	{
		gEnv->pInput->RemoveEventListener(this);
		gEnv->pInput->SetExclusiveListener(NULL);
	}

	// Delete this after everything else...
	// some of the things deleted above could well be game mechanisms, so we need
	// the game mechanism manager to still exist when they're deleted [TF]
	SAFE_DELETE(m_gameMechanismManager);
	SAFE_DELETE(m_pGameAchievements);

#if ENABLE_VISUAL_DEBUG_PROTOTYPE
	SAFE_DELETE(m_VisualDebugSys);
#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE

	SAFE_DELETE(m_pCVars); // Do this last to avoid cached CVars being used and causing crashes

	g_pGame = 0;
	g_pGameCVars = 0;
	g_pGameActions = 0;
}

#define EngineStartProfiler(x)
#define InitTerminationCheck(x)

static inline void InlineInitializationProcessing(const char* sDescription)
{
	EngineStartProfiler(sDescription);
	InitTerminationCheck(sDescription);
	gEnv->pLog->UpdateLoadingScreen(0);
}

bool CGame::Init(/*IGameFramework* pFramework*/)
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());

	InlineInitializationProcessing("CGame::Init");
#ifdef GAME_DEBUG_MEM
	DumpMemInfo("CGame::Init start");
#endif

	m_pConsole = gEnv->pConsole;

	//This should be early so we can load the strings
	m_pGameLocalizationManager = new CGameLocalizationManager();

	RegisterConsoleVars();
	RegisterConsoleCommands();
	RegisterGameObjectEvents();

	if (!gEnv->IsDedicated())
	{
		//iterates the available screen resolutions and stores them, do that before creating the profile options
		ScreenResolution::InitialiseScreenResolutions();

		m_pUIManager = new CUIManager();
		m_pUIManager->Init();
	}

	InitPlatformOS();

	{
		IProceduralClipFactory& proceduralClipFactory = gEnv->pGameFramework->GetMannequinInterface().GetProceduralClipFactory();
		mannequin::RegisterProceduralClipsForModule(proceduralClipFactory);
	}

	// Initialize static item strings
	m_pItemStrings = new SItemStrings();

	m_pGameParametersStorage = new CGameSharedParametersStorage();
	m_pGamePhysicsSettings = new CGamePhysicsSettings();

	GetISystem()->GetPlatformOS()->AddListener(this, "CGame");
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CGame");

	InitScriptBinds();

	InlineInitializationProcessing("CGame::Init InitScriptBinds");

	//load user levelnames for ingame text and savegames
	LoadMappedLevelNames("Scripts/GameRules/LevelNames.xml");

	InitRichPresence();

	// Register all the games factory classes e.g. maps "Player" to CPlayer
	InitGameFactory(gEnv->pGameFramework);

	InlineInitializationProcessing("CGame::Init InitGameFactory");

	IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();
	pItemSystem->Scan(GAME_ITEMS_DATA_FOLDER);

	m_pWeaponSystem = new CWeaponSystem(this, GetISystem());
	m_pWeaponSystem->Scan(GAME_ITEMS_DATA_FOLDER);

	string actorParamsFolder = "scripts/entities/actor/parameters";
	gEnv->pGameFramework->GetIActorSystem()->Scan(actorParamsFolder);

	m_pAutoAimManager = new CAutoAimManager();

	InlineInitializationProcessing("CGame::Init HitDeathReactionsSystem");

	m_pHitDeathReactionsSystem = new CHitDeathReactionsSystem;
	CRY_ASSERT(m_pHitDeathReactionsSystem);

	m_pMovementTransitionsSystem = new CMovementTransitionsSystem;
	CRY_ASSERT(m_pMovementTransitionsSystem);

	gEnv->pConsole->CreateKeyBind("f12", "r_getscreenshot 2");

	//Ivo: initialites the Crysis conversion file.
	//this is a conversion solution for the Crysis game DLL. Other projects don't need it.
	// No need anymore
	//gEnv->pCharacterManager->LoadCharacterConversionFile("Objects/CrysisCharacterConversion.ccc");

	// set game GUID
	gEnv->pGameFramework->SetGameGUID(CRYENGINE_SDK_GUID);

	// TEMP
	// Load the action map beforehand (see above)
	// afterwards load the user's profile whose action maps get merged with default's action map
	m_pPlayerProfileManager = gEnv->pGameFramework->GetIPlayerProfileManager();

	if (CProfileOptions* profileOptions = GetProfileOptions())
		profileOptions->Init();

	InlineInitializationProcessing("CGame::Init PlayerProfileManager");

#if !defined(_RELEASE)
	if (!g_pGameCVars->g_skipStartupSignIn)
#endif
	{
		gEnv->pSystem->GetPlatformOS()->UserDoSignIn(0); // sign in the default user
	}

	LoadActionMaps("libs/config/defaultProfile.xml");
	InlineInitializationProcessing("CGame::Init LoadActionMaps");

#if CRY_PLATFORM_DURANGO
	XboxLiveGameEvents::CreateGUID(m_playerSessionId);
#endif

	InlineInitializationProcessing("CGame::Init ProfileOptions");
	if (!gEnv->IsEditor())
	{
		CryFixedStringT<128> configFileName;

		if (gEnv->IsDedicated())
		{
			const char* const szDedicatedBaseConfigName = "dedicated.cfg";
			const string configFileName = PathUtil::Make(gEnv->pSystem->GetRootFolder(), szDedicatedBaseConfigName);

			CryLog("[dedicated] loading dedicated config %s", configFileName.c_str());

			SDedicatedConfigSink sink;
			gEnv->pSystem->LoadConfiguration(configFileName.c_str(), &sink, eLoadConfigInit);
		}

		CryLogAlways("[Game Version]: "
#if defined(_RELEASE)
		             "RELEASE "
#elif defined(_PROFILE)
		             "PROFILE "
#else
		             "DEBUG "
#endif

#if defined(PURE_CLIENT)
		             "PURE CLIENT"
#elif (DEDICATED_SERVER)
		             "DEDICATED SERVER"
#else
		             "DEVELOPMENT BUILD"
#endif
		             );

#if !defined(DEDICATED_SERVER) || defined(PURE_CLIENT)
		if (gEnv->IsDedicated())
		{
	#if defined(_RELEASE)
			CryFatalError("[Game]: Running wrong version of DedicatedServer.exe - aborting");
	#endif // #if defined(_RELEASE)
		}
#endif

		ECryLobbyServiceFeatures features = ECryLobbyServiceFeatures(eCLSO_All & ~(eCLSO_Voice));
		bool isDedicated = gEnv->IsDedicated();
		ECryLobbyError error;

		if (isDedicated)
		{
			features = ECryLobbyServiceFeatures(features & ~(eCLSO_LobbyUI | eCLSO_Friends | eCLSO_Reward));
			m_pPlayerProfileManager->SetExclusiveControllerDeviceIndex(0);
			if (gEnv->pInput)
			{
				gEnv->pInput->ForceFeedbackSetDeviceIndex(0);
			}
		}

		gEnv->pNetwork->SetMultithreadingMode(INetwork::NETWORK_MT_PRIORITY_NORMAL);

#if CRY_PLATFORM_WINDOWS
		if (gEnv->IsDedicated())
		{
			ICmdLine* pCommandLine = gEnv->pSystem->GetICmdLine();
			if (pCommandLine->FindArg(eCLAT_Post, "connect") || pCommandLine->FindArg(eCLAT_Post, "client"))
			{
				gEnv->bServer = false;
				gEnv->SetIsClient(true);
			}
		}
#endif

		ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{
			pLobby->SetUserPacketEnd(eGUPD_End);

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
			if (!(g_pGameCVars && (g_pGameCVars->g_useOnlineServiceForDedicated) && gEnv->IsDedicated()))
			{
				error = pLobby->Initialise(eCLS_LAN, features, CGameBrowser::ConfigurationCallback, CGameBrowser::InitialiseCallback, this);
				CRY_ASSERT_MESSAGE(error == eCLE_Success, "Failed to initialize LAN lobby service");
			}
#endif // #if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)

			if (!gEnv->IsDedicated())
			{
				error = pLobby->Initialise(eCLS_Online, features, CGameBrowser::ConfigurationCallback, CGameBrowser::InitialiseCallback, this);
			}
			else
			{
				CryLog("Online lobby currently not supported for dedicated sever. Not initialized");
			}

			//CRY_ASSERT_MESSAGE( error == eCLE_Success, "Failed to initialize online lobby service" );
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_COMMENT, "Online Lobby not supported in the default SDK.");
			m_pSquadManager = new CSquadManager();    // MUST be done before game browser is constructed

			m_pGameBrowser = new CGameBrowser();

			//Set the matchmaking version based on the build version if g_matchmakingversion is a default value
			if (!gEnv->IsEditor() && g_pGameCVars->g_MatchmakingVersion <= 1)
			{
				const int internalBuildVersion = gEnv->pSystem->GetBuildVersion().v[0];
				CryLog("BuildVersion %d", internalBuildVersion);
				if (internalBuildVersion != 1)
				{
					g_pGameCVars->g_MatchmakingVersion = internalBuildVersion;
				}
			}

			CGameBrowser::InitLobbyServiceType();

			m_pGameLobbyManager = new CGameLobbyManager();
		}
	}

	m_pGameAchievements = new CGameAchievements;  //Should be after GameLobbyManager
	CRY_ASSERT(m_pGameAchievements);

#ifdef USE_LAPTOPUTIL
	// CLaptopUtil must be created before CFlashMenuObject as this one relies on it
	if (!m_pLaptopUtil)
		m_pLaptopUtil = new CLaptopUtil;
#endif

	if (!(gEnv->IsDedicated()) && !(gEnv->bMultiplayer))
	{
		const int iCVarDifficulty = g_pGameCVars->g_difficultyLevel;
		EDifficulty difficulty = (iCVarDifficulty >= eDifficulty_Default && iCVarDifficulty < eDifficulty_COUNT ? (EDifficulty)iCVarDifficulty : eDifficulty_Default);
		SetDifficultyLevel(difficulty);
	}

	if (!m_pGameAudio)
	{
		m_pGameAudio = new CGameAudio();
	}

	InlineInitializationProcessing("CGame::Init GameAudio");

	m_pPlayerProgression = new CPlayerProgression();  //Needs to be before persistant stats

	if (!m_pPersistantStats)
	{
		m_pPersistantStats = new CPersistantStats();  //Needs to be before Skill Assessments
	}

#if !defined(_RELEASE)
	// START FIXME
	CRY_FIXME(24, 02, 2010, "Temporarily added early cvar sets so that game init can use them to pick the correct sp/mp game. Needs to be removed when a cleaner MP/SP split is there.");
	// Abusing the pre cmd line args but necessary so that the current game type cvars are read to use for game init
	if (const ICmdLineArg* pCmdArg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "g_multiplayerDefault"))
	{
		g_pGameCVars->g_multiplayerDefault = pCmdArg->GetIValue();
	}
	// END FIXME
#endif // !defined(_RELEASE)

	GAME_FX_SYSTEM.Initialise();

	if (!gEnv->IsEditor())
	{
		if (g_pGameCVars->g_EPD == 3)
		{
			g_pGameCVars->g_multiplayerModeOnly = 1;
		}

		if (g_pGameCVars->g_multiplayerModeOnly)
		{
			g_pGameCVars->g_multiplayerDefault = 1;
		}
	}

	// DLC Manager needs to be loaded after the warnings manager because it may display warnings
	m_pDLCManager = new CDLCManager();

	ICVar* pMaxPlayers = gEnv->pConsole->GetCVar("sv_maxplayers");
	if (pMaxPlayers)
	{
		pMaxPlayers->SetOnChangeCallback(VerifyMaxPlayers); // this needs to be set 1st, if MAX_PLAYER_LIMIT is greater than 32 we'll clamp it otherwise
		pMaxPlayers->Set(MAX_PLAYER_LIMIT);
	}

	if (!gEnv->IsEditor())
	{
		m_pDownloadMgr = new CDownloadMgr();
		m_patchPakManager = new CPatchPakManager;

		m_pDownloadMgr->Init("Scripts/Network/TCPServices.xml", "Scripts/Network/DownloadMgr.xml"); // after constructing CPatchPakManager so the downloadmgr can add itself as a listener to the patchpakmanager
	}

	if (!gEnv->IsDedicated())
	{
		// This needs to be scheduled regardless of if we're showing the language selection screen or not, for autotest multiplayer (which we don't know about at this point)
		LoadPatchLocalizationData();

		//No localization is going to be shown. Perform post-loc checking immediately
		gEnv->pSystem->GetPlatformOS()->PostLocalizationBootChecks();
		InitGameType(g_pGameCVars->g_multiplayerDefault != 0, true);

		// we need this to setup the xmb buttons
		ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
		ICryLobbyService* pLobbyService = pLobby ? pLobby->GetLobbyService(eCLS_Online) : NULL;
		ICryLobbyUI* pLobbyUI = pLobbyService ? pLobbyService->GetLobbyUI() : NULL;
		if (pLobbyUI)
		{
			pLobbyUI->PostLocalizationChecks();
		}
	}
	else
	{
		InitGameType(true, true);
	}

	InlineInitializationProcessing("CGame::Init InitGameType");

	if (!m_pScreenEffects)
	{
		m_pScreenEffects = new CScreenEffects();
	}

	// Create hud interference game effect
	if (m_pHudInterferenceGameEffect == NULL)
	{
		m_pHudInterferenceGameEffect = GAME_FX_SYSTEM.CreateEffect<CHudInterferenceGameEffect>();
		if (m_pHudInterferenceGameEffect)
		{
			m_pHudInterferenceGameEffect->Initialise();
		}
	}

	// Create scene blur game effect
	if (m_pSceneBlurGameEffect == NULL)
	{
		m_pSceneBlurGameEffect = GAME_FX_SYSTEM.CreateEffect<CSceneBlurGameEffect>();
		if (m_pSceneBlurGameEffect)
		{
			m_pSceneBlurGameEffect->Initialise();
		}
	}

	// Create lightning game effect
	if (m_pLightningGameEffect == NULL)
	{
		m_pLightningGameEffect = GAME_FX_SYSTEM.CreateEffect<CLightningGameEffect>();
		if (m_pLightningGameEffect)
			m_pLightningGameEffect->Initialise();
	}

	// Create parameter game effect
	if (m_pParameterGameEffect == NULL)
	{
		m_pParameterGameEffect = GAME_FX_SYSTEM.CreateEffect<CParameterGameEffect>();
		if (m_pParameterGameEffect)
		{
			m_pParameterGameEffect->Initialise();
		}
	}

	if (!m_pLedgeManager)
	{
		m_pLedgeManager = new CLedgeManager();
	}

	if (!m_pWaterPuddleManager)
	{
		m_pWaterPuddleManager = new CWaterPuddleManager();
	}

	InlineInitializationProcessing("CGame::Init LedgeManager");

	gEnv->pGameFramework->RegisterListener(this, "Game", FRAMEWORKLISTENERPRIORITY_GAME);

	CVehicleClient* pVehicleClient = new CVehicleClient();
	pVehicleClient->Init();
	g_pGame->GetIGameFramework()->GetIVehicleSystem()->RegisterVehicleClient(pVehicleClient);

	InlineInitializationProcessing("CGame::Init RegisterVehicleClient");

#ifdef GAME_DEBUG_MEM
	DumpMemInfo("CGame::Init end");
#endif

	if (gEnv->IsEditor())
	{
		g_tacticalPointLanguageExtender.Initialize();
	}
	else
	{
		if (!gEnv->IsDedicated())
		{
#if IMPLEMENT_PC_BLADES
			m_pGameServerLists = new CGameServerLists();
#endif //IMPLEMENT_PC_BLADES

		}
	}

	// Initialise game handler for checkpoints
	CCheckpointGame::GetInstance()->Init();

	ICryLobby* pLobby = gEnv->pNetwork ? gEnv->pNetwork->GetLobby() : NULL;
	if (pLobby)
	{
		pLobby->RegisterEventInterest(eCLSE_PartyMembers, CGame::PartyMembersCallback, this);
		pLobby->RegisterEventInterest(eCLSE_UserProfileChanged, CGame::UserProfileChangedCallback, this);
		pLobby->RegisterEventInterest(eCLSE_OnlineState, CGame::OnlineStateCallback, this);
		pLobby->RegisterEventInterest(eCLSE_EthernetState, CGame::EthernetStateCallback, this);
		pLobby->RegisterEventInterest(eCLSE_InviteAccepted, CGame::InviteAcceptedCallback, this);

		// online only stuff
		ICryLobbyService* pOnlineLobbyService = pLobby->GetLobbyService(eCLS_Online);
		if (pOnlineLobbyService)
		{
			if (m_pPlayerProfileManager)
			{
				CryLog("[online] RegisterOnlineAttributes");
				INDENT_LOG_DURING_SCOPE();
				m_pPlayerProfileManager->RegisterOnlineAttributes();
			}
		}
	}

#if ENABLE_FEATURE_TESTER
	new CFeatureTester();
#endif

#ifdef CODECHECKPOINT_DEBUG_ENABLED
	CCodeCheckpointDebugMgr::RetrieveCodeCheckpointDebugMgr();
#endif

	if (gEnv->pInput)
	{
		gEnv->pInput->SetExclusiveListener(this);
	}

	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if (!gEnv->bMultiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		m_pGameAISystem = new CGameAISystem();

		InlineInitializationProcessing("CGame::Init GameAISystem");
	}

	//if (gEnv->IsEditor())
	{
		m_pRayCaster = new GlobalRayCaster;
		m_pRayCaster->SetQuota(g_pGameCVars->g_gameRayCastQuota);

		m_pIntersectionTester = new GlobalIntersectionTester;
		m_pIntersectionTester->SetQuota(g_pGameCVars->g_gameIntersectionTestQuota);
	}

	m_pPlayerVisTable = new CPlayerVisTable();

	m_pGameCache = new CGameCache;
	m_pGameCache->Init();

	CBullet::StaticInit();

	InlineInitializationProcessing("CGame::Init IntersectionTester");

	m_pWorldBuilder = new CWorldBuilder();

	if (gEnv->pDynamicResponseSystem)
	{
		// register the custom DRS actions and conditions
		REGISTER_DRS_CUSTOM_ACTION(CActionExecuteAudioTrigger);
		REGISTER_DRS_CUSTOM_ACTION(CActionSetAudioSwitch);
		REGISTER_DRS_CUSTOM_ACTION(CActionSetAudioParameter);
		REGISTER_DRS_CUSTOM_CONDITION(CConditionDistanceToEntity);

		// create a special DrsActor that sends out our automatic signals every time a gametoken changes its value
		DRS::IResponseActor* pDrsActor = gEnv->pDynamicResponseSystem->CreateResponseActor("GameTokenSignalSender");
		m_pGameTokenSignalCreator = new CGameTokenSignalCreator(pDrsActor);
	}
	return true;
}

bool CGame::CompleteInit()
{
	if (!gEnv->IsEditor())
	{
		//This will load the parameters in shared storage
		m_pWeaponSystem->Reload();
		CFrontEndModelCache::UpdateNeed3dFrontEndAssets();
	}
	//else look at OnEditorGameInitComplete()

#ifdef INCLUDE_GAME_AI_RECORDER
	assert(gEnv->bMultiplayer || m_pGameAISystem);
	if (m_pGameAISystem)
	{
		m_pGameAISystem->GetGameAIRecorder().Init();
	}
#endif //INCLUDE_GAME_AI_RECORDER

#ifdef GAME_DEBUG_MEM
	DumpMemInfo("CGame::CompleteInit");
#endif

	return true;
}

void CGame::OnEditorGameInitComplete()
{
	//This will load the parameters in shared storage
	m_pWeaponSystem->Reload();
}

void CGame::RegisterPhysicsCallbacks()
{
	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;
	if (!pPhysicalWorld)
		return;

	pPhysicalWorld->AddEventClient(EventPhysCreateEntityPart::id, OnCreatePhysicalEntityLogged, 1, 0.1f);
}

void CGame::UnregisterPhysicsCallbacks()
{
	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;
	if (!pPhysicalWorld)
		return;

	pPhysicalWorld->RemoveEventClient(EventPhysCreateEntityPart::id, OnCreatePhysicalEntityLogged, 1);
}

CRevertibleConfigLoader& CGame::GetGameModeCVars()
{
	return s_gameModeCVars;
}

#define MODE_SWITCH_PAK_FILENAME  "modes/gamemodeswitch.pak"
#define MODE_SWITCH_LIST_FILENAME "gamemodeswitch"

// Loads and Inits either singleplayer or multiplayer
// Called when selecting from the selection frontend
void CGame::InitGameType(bool multiplayer, bool fromInit /*= false*/)
{
	CryLog("CGame::InitGameType() multiplayer=%s, fromInit=%s", multiplayer ? "true" : "false", fromInit ? "true" : "false");
#if defined(DEDICATED_SERVER)
	if (!multiplayer)
	{
		CryFatalError("CGame::InitGameType(bool multiplayer, bool fromInit): multiplayer == false not supported for dedicated server");
		multiplayer = true;
	}
#endif

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CGame::InitGameType");

	SAFE_DELETE(m_pDataPatchDownloader);

	bool bWasMultiplayer = gEnv->bMultiplayer;
	const char* const hostName = gEnv->pSystem->GetPlatformOS()->GetHostName();
	CryLog("hostName = '%s'", hostName);

	if (g_pGameCVars->g_multiplayerModeOnly && multiplayer == false)
	{
		GameWarning("[InitGameType]: Cannot switch from multiplayer as g_multiplayerModeOnly is set.");
		return;
	}

	if (fromInit == false)
	{
		//Load the mode switch pak, this can considerably reduce the time spent switching especially from disc
		if (gEnv->pSystem->GetIResourceManager()->LoadModeSwitchPak(MODE_SWITCH_PAK_FILENAME, multiplayer) == false)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not load %s during game mode switch. This file can significantly reduce mode switching times.\n", MODE_SWITCH_PAK_FILENAME);
		}
	}

	gEnv->pGameFramework->InitGameType(multiplayer, fromInit);

#if USE_TELEMETRY_BUFFERS
	SAFE_DELETE(m_performanceBuffer);
	SAFE_DELETE(m_bandwidthBuffer);
	SAFE_DELETE(m_memoryTrackingBuffer);
	SAFE_DELETE(m_soundTrackingBuffer);
#endif //#if USE_TELEMETRY_BUFFERS
	SAFE_DELETE(m_statsRecorder);
	SAFE_DELETE(m_pRecordingSystem);
	SAFE_DELETE(m_pEquipmentLoadout);
#if USE_LAGOMETER
	SAFE_DELETE(m_pLagOMeter);
#endif
	SAFE_DELETE(m_telemetryCollector);
	SAFE_DELETE(m_pPlaylistActivityTracker);
	SAFE_DELETE(m_pStatsEntityIdRegistry);
	SAFE_DELETE(m_pMatchMakingTelemetry);

	if (m_pLobbySessionHandler != NULL)
	{
		// Clear the pointer in CryAction this will also call delete on m_pLobbySessionHandler and set it to NULL
		ClearGameSessionHandler();
	}
	if (m_pPlaylistManager)
	{
		SAFE_DELETE(m_pPlaylistManager);
	}

#if 0
	if (multiplayer)
	{
		CMPConfigSink cvarSink;
		gEnv->pSystem->LoadConfiguration("mp_release.cfg", &cvarSink);
	}
#endif // #if 0

	bool gameTypeChanged = (m_gameTypeMultiplayer != multiplayer) && !fromInit;

	const char* const GAMETYPE_CONFIG_NAMES[2] = { "multiplayer", "singleplayer" };
	const int gameTypeID = multiplayer ? 0 : 1;
	const char* const gameTypeConfigName = GAMETYPE_CONFIG_NAMES[gameTypeID];

	s_gameModeCVars.RevertCVarChanges();

	// patch before loading multiplayer.cfg to allow it to be patchable
	if (multiplayer == true)
	{
#if 1
		if (!gEnv->IsEditor())
		{
			if (m_patchPakManager)
			{
				// start & finish downloading and caching any paks in flight or timeout
				m_patchPakManager->BlockingUpdateTillDone();
			}
		}
#endif
	}

	// Load config file
	s_gameModeCVars.LoadConfiguration(gameTypeConfigName);

	// For consoles load console & then platform specific mp config file
	// For PC just load platform specific mp config file
	if (multiplayer == true)
	{
		gEnv->pNetwork->SetMultithreadingMode(INetwork::NETWORK_MT_PRIORITY_HIGH);

		//Force enable statoscope
#if 0 //defined(_RELEASE) && defined(ENABLE_PROFILING_CODE)
		if (gEnv->pStatoscope)
		{
			gEnv->pStatoscope->SetupFPSCaptureCVars();
		}
#endif

		if (!gEnv->IsDedicated() && gEnv->pNetwork->GetLobby())
		{
			// Late initialise voice service if playing multiplayer (and not a dedicated server)
			gEnv->pNetwork->GetLobby()->Initialise(eCLS_Online, eCLSO_Voice, CGameBrowser::ConfigurationCallback, NULL, NULL);
		}

		const char* mpConfigName = NULL;

		EPlatform platform = GetPlatform();
		switch (platform)
		{
			case EPlatform::Windows:
			case EPlatform::Linux:
			case EPlatform::MacOS:
			{
				mpConfigName = "multiplayer_pc";
				break;
			}
		default:
			{
				mpConfigName = NULL;
				break;
			}
		}
		if (mpConfigName)
		{
			// Load mp platform specific config file
			s_gameModeCVars.LoadConfiguration(mpConfigName);
		}
	}
#if !defined(DEDICATED_SERVER)
	else
	{
		gEnv->pNetwork->SetMultithreadingMode(INetwork::NETWORK_MT_PRIORITY_NORMAL);
		// Early terminate voice service if playing single player (attempt to save memory)
		if (gEnv->pNetwork->GetLobby())
		{
			gEnv->pNetwork->GetLobby()->Terminate(eCLS_Online, eCLSO_Voice, NULL, NULL);
		}
	}
#endif

	// Switch CryNetwork to the correct threading mode
	gEnv->bMultiplayer = multiplayer;

	m_pGameLocalizationManager->SetGameType(); //should be after gEnv->bMultiplayer is set

	REINST("needs verification!");
	/*ICVar* pFilename = gEnv->pConsole->GetCVar("s_AudioPreloadsFile");
	   if(pFilename)
	   {
	   gEnv->pSoundSystem->ClearAudioDataCache();
	   pFilename->Set(multiplayer ? "AudioPreloads_mp" : "AudioPreloads");
	   gEnv->pSoundSystem->CacheAudioData(NULL, 1);
	   }*/

	if (gameTypeChanged)
	{
		//Reload item/ammo parameters for game mode
		m_pWeaponSystem->Reload();
	}

	m_pPersistantStats->SetMultiplayer(multiplayer);

	if (m_pSquadManager)
	{
		m_pSquadManager->SetMultiplayer(multiplayer);
	}

	if (m_pGameLobbyManager)
	{
		m_pGameLobbyManager->SetMultiplayer(multiplayer);
		m_pGameLobbyManager->LeaveGameSession(CGameLobbyManager::eLSR_SwitchGameType);
	}

	ICVar* pAllowDisconnect = gEnv->pConsole->GetCVar("g_allowDisconnectIfUpdateFails");
	ICVar* pGsmLodsNum = gEnv->pConsole->GetCVar("e_GsmLodsNum");
	assert(pGsmLodsNum);
	ICVar* pGsmRange = gEnv->pConsole->GetCVar("e_GsmRange");
	assert(pGsmRange);
	ICVar* pGsmRangeStep = gEnv->pConsole->GetCVar("e_GsmRangeStep");
	assert(pGsmRangeStep);

#if CRY_PLATFORM_WINDOWS && !defined(DEDICATED_SERVER)
	gEnv->pHardwareMouse->IncrementCounter();
#endif

	if (multiplayer)
	{
		if (!gEnv->IsEditor())
		{
			// Multiplayer Init
			CryLog("CGame: Multiplayer game type initialized");
			g_pGameCVars->g_multiplayerDefault = 1;

			//The Squadmanager, gamebrowser and gamelobbymanager upon which this is dependent are initialised early, so that Rich Presence and certain PSN related functions can run on init.
			m_pLobbySessionHandler = new CCryLobbySessionHandler();

			m_pDataPatchDownloader = new CDataPatchDownloader();
			m_pPlaylistManager = new CPlaylistManager;
			if (m_pPlaylistManager)
			{
				m_pPlaylistManager->Init("Scripts/Playlists");
			}
			//also setup any DLC Playlists
			if (m_pDLCManager)
			{
				m_pDLCManager->AddPlaylists();
			}

#if USE_LAGOMETER
			m_pLagOMeter = new CLagOMeter();
#endif

			CRY_ASSERT(!m_telemetryCollector);
			MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_None, "CTelemetryCollector");
			m_telemetryCollector = new CTelemetryCollector;

			m_pPlaylistActivityTracker = new CPlaylistActivityTracker;
			m_pPlaylistActivityTracker->SetState(CPlaylistActivityTracker::eATS_GetActivity);

			if (g_pGameCVars->g_telemetry_gameplay_enabled)
			{
				MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_None, "CStatsRecordingMgr");
				m_statsRecorder = new CStatsRecordingMgr;
			}

#if USE_TELEMETRY_BUFFERS
			{
				MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_None, "Perf Telemetry Buffers");
				if (g_pGameCVars->g_telemetrySampleRatePerformance > 0.0f)
				{
					m_performanceBuffer = new CTelemetryBuffer(60 * 1024, m_telemetryCollector, sizeof(SPerformanceTelemetry));
				}

				if (g_pGameCVars->g_telemetrySampleRateBandwidth > 0.0f)
				{
					m_bandwidthBuffer = new CTelemetryBuffer(60 * 1024, m_telemetryCollector, sizeof(SBandwidthTelemetry));
				}

				if (g_pGameCVars->g_telemetrySampleRateMemory > 0.0f)
				{
					m_memoryTrackingBuffer = new CTelemetryBuffer(30 * 1024, m_telemetryCollector, sizeof(SMemoryTelemetry));
				}
			}
#endif //#if USE_TELEMETRY_BUFFERS

			m_pStatsEntityIdRegistry = new CStatsEntityIdRegistry();

			if (m_pUIManager)
				m_pUIManager->InitGameType(true, fromInit);

			// get the user's online attributes
			if (m_pPlayerProfileManager)
			{
				m_pPlayerProfileManager->EnableOnlineAttributes(true);
			}

			// Cache shadow cvars
			m_iCachedGsmValue = pGsmLodsNum->GetIVal();
			m_fCachedGsmRangeValue = pGsmRange->GetFVal();
			m_fCachedGsmRangeStepValue = pGsmRangeStep->GetFVal();

			if (pAllowDisconnect)
			{
				pAllowDisconnect->Set(0);
			}
		}

		bool runMPInitNow = gEnv->IsDedicated();

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
		if (g_pGameCVars->net_initLobbyServiceToLan)
		{
			runMPInitNow = true;
		}
#endif

		m_needsInitPatchables = true;

		// Michiel: have to initialize this on game-side too for now, the game doesnt have its asynchtasks anymore
		runMPInitNow = true;

		if (runMPInitNow)
		{
#if PLATFORM_SUPPORTS_RSS_PLAYLISTS
			CPlaylistManager* pPlaylistManager = g_pGame->GetPlaylistManager();
			if (pPlaylistManager)
			{
				pPlaylistManager->RequestPlaylistFeedFromRSS();
				pPlaylistManager->RequestVariantsFeedFromRSS();
			}
#endif
			if (gEnv->IsDedicated())
			{
				// If it's a dedicated server wait for the data patch & playlists to download before continuing
				// normally this would be done asynchronously in MenuDataMPLoaderTasks.cpp but that isn't
				// enabled in dedicated builds
				if (m_pDownloadMgr)
				{
#if PLATFORM_SUPPORTS_RSS_PLAYLISTS
					const char* resources[] = { "datapatch", "playlists", "variants" };
#else
					const char* resources[] = { "datapatch" };
#endif
					const int numResources = sizeof(resources) / sizeof(char*);
					m_pDownloadMgr->WaitForDownloadsToFinish(resources, numResources, g_pGameCVars->g_mpLoaderScreenMaxTimeSoftLimit);
				}
			}
			if (m_pDataPatchDownloader)
			{
				CDownloadableResourcePtr pRes = m_pDataPatchDownloader->GetDownloadableResource();
				if (pRes && pRes->GetState() == CDownloadableResource::k_dataAvailable)
				{
					m_pDataPatchDownloader->SetPatchingEnabled(true);
				}
				else
				{
					m_pDataPatchDownloader->CancelDownload();
				}
			}
			InitPatchableModules(true);
		}

		GetIGameFramework()->PauseGame(false, true);
	}
#if !defined(DEDICATED_SERVER)
	else
	{
		if (m_patchPakManager)
		{
			// patch paks are currently for MP only - unload any loaded patch paks as we head into SP
			m_patchPakManager->UnloadPatchPakFiles();
		}

		// Singleplayer Init
		CryLog("CGame: Singleplayer game type initialised");
		g_pGameCVars->g_multiplayerDefault = 0;

		if (m_pUIManager)
			m_pUIManager->InitGameType(false, fromInit);

		if (!gEnv->IsEditor())
		{
			if (g_pGameCVars->g_telemetryEnabledSP != 0)      // (cvar values: 2 is record locally but not submit, 1 is record and submit)
			{
				m_statsRecorder = new CStatsRecordingMgr;       // if this it to be enabled on ship for SP it will need to ensure it is using a fixed memory buffer (like mp) and we will need to check that hand over of the storage buffer from mp/sp works in the case where the storage buffer has not been deleted due to a send still being in progress
			}

			if (g_pGameCVars->g_telemetryEnabledSP == 1 || g_pGameCVars->g_telemetrySampleRateSound > 0.0f || g_pGameCVars->g_telemetrySampleRatePerformance > 0.0f)
			{
				CRY_ASSERT(!m_telemetryCollector);
				m_telemetryCollector = new CTelemetryCollector;

	#if USE_TELEMETRY_BUFFERS
				if (g_pGameCVars->g_telemetrySampleRateSound > 0.0f)
				{
					m_soundTrackingBuffer = new CTelemetryBuffer(30 * 1024, m_telemetryCollector, sizeof(SSoundTelemetry));
				}

				if (g_pGameCVars->g_telemetrySampleRatePerformance > 0.0f)
				{
					m_performanceBuffer = new CTelemetryBuffer(60 * 1024, m_telemetryCollector, sizeof(SPerformanceTelemetry));
				}

				if (g_pGameCVars->g_telemetrySampleRateMemory > 0.0f)
				{
					m_memoryTrackingBuffer = new CTelemetryBuffer(30 * 1024, m_telemetryCollector, sizeof(SMemoryTelemetry));
				}
	#endif //#if USE_TELEMETRY_BUFFERS
			}

			CRY_ASSERT(m_pMatchMakingTelemetry == NULL);

			//LAN mode won't produce useful results for matchmaking telemetry
			if (gEnv->pNetwork->GetLobby() && gEnv->pNetwork->GetLobby()->GetLobbyServiceType() == eCLS_Online)
			{
				m_pMatchMakingTelemetry = new CMatchmakingTelemetry();
			}

			// online attributes not enabled for singleplayer
			if (m_pPlayerProfileManager)
			{
				m_pPlayerProfileManager->EnableOnlineAttributes(false);
			}

			if (bWasMultiplayer)
			{
				// Restore shadow cvars (Multiplayer game modified these to improve performance)
				pGsmLodsNum->Set(m_iCachedGsmValue);
				pGsmRange->Set(m_fCachedGsmRangeValue);
				pGsmRangeStep->Set(m_fCachedGsmRangeStepValue);
			}

			if (pAllowDisconnect)
			{
				pAllowDisconnect->Set(1);
			}
		}

		// in MP, certain modules are initialised after the game data patch is downlaoaded
		// in SP, they are initialized immediately here
		m_needsInitPatchables = true;
		InitPatchableModules(false);
	}
#endif

	m_pHitDeathReactionsSystem->OnToggleGameMode();

	m_gameTypeMultiplayer = multiplayer;
	m_gameTypeInitialized = true;

#if !defined(DEDICATED_SERVER)
	CFrontEndModelCache::UpdateNeed3dFrontEndAssets();

#endif

	if (gameTypeChanged)
	{
		GetIGameFramework()->GetIMaterialEffects()->LoadFXLibraries(); //Reloading FX libraries will re-parse for GAME="SP/MP"
	}

#if CRY_PLATFORM_WINDOWS && !defined(DEDICATED_SERVER)
	gEnv->pHardwareMouse->DecrementCounter();
#endif

	if (fromInit == false)
	{
		//Unload the mode switch pak, we don't need it hanging around. Pass in the resourcelist name in case we are saving it for the build system.
		if (multiplayer == true)
		{
			gEnv->pSystem->GetIResourceManager()->UnloadModeSwitchPak(MODE_SWITCH_PAK_FILENAME, MODE_SWITCH_LIST_FILENAME "_mp", multiplayer);
		}
#if !defined(DEDICATED_SERVER)
		else
		{
			gEnv->pSystem->GetIResourceManager()->UnloadModeSwitchPak(MODE_SWITCH_PAK_FILENAME, MODE_SWITCH_LIST_FILENAME "_sp", multiplayer);
		}
#endif
	}

	SAFE_DELETE(m_pMovingPlatformMgr);
	if (multiplayer)
		m_pMovingPlatformMgr = new CMovingPlatformMgr();

	// Set some game-mode specific rendering parameters
	if (gEnv->pRenderer)
	{
		const float hudSilFillStr = multiplayer ? 0.25f : 0.15f;
		gEnv->pRenderer->EF_SetPostEffectParam("HudSilhouettes_FillStr", hudSilFillStr, true);
	}
}

// called after any game data patch is downloaded to finish initializing the parts of MP that are patchable
void CGame::InitPatchableModules(
  bool inIsMultiplayer)
{
	if (m_needsInitPatchables)
	{
		//load the 'run once' script
		if (inIsMultiplayer && gEnv->pScriptSystem->ExecuteFile("Scripts/DataPatcher/RunOnce.lua", true, true))
		{
			SmartScriptTable pRunOnceScript;
			if (gEnv->pScriptSystem->GetGlobalValue("RunOnce", pRunOnceScript))
			{
				//enable a lot of extra script binds
				m_pScriptBindProtected->Enable();

				//run the 'run once' scripts that let us fix/adjust anything we want
				HSCRIPTFUNCTION scriptFunction;
				if (pRunOnceScript->GetValue("Execute", scriptFunction))
				{
					Script::Call(gEnv->pScriptSystem, scriptFunction, pRunOnceScript);
					gEnv->pScriptSystem->ReleaseFunc(scriptFunction);
				}

				//remove the added script binds
				m_pScriptBindProtected->Disable();

#if !defined(_RELEASE)
				if (m_pCVars->g_debugTestProtectedScripts)
				{
					//test that they are inaccessible
					if (pRunOnceScript->GetValue("Test", scriptFunction))
					{
						Script::Call(gEnv->pScriptSystem, scriptFunction, pRunOnceScript);
						gEnv->pScriptSystem->ReleaseFunc(scriptFunction);
					}
				}
#endif  //!defined(_RELEASE)
			}
		}

		if (inIsMultiplayer)
		{
			m_pPlayerProgression->ResetUnlocks();

			CRY_ASSERT_MESSAGE(m_pEquipmentLoadout == NULL, "double initialisation of equipment loadout - logic flow error");
			m_pEquipmentLoadout = new CEquipmentLoadout();

			if (CDataPatchDownloader* pDP = GetDataPatchDownloader())
			{
				pDP->ApplyCVarPatch();
			}
		}

		m_pPlayerProgression->PostInit(); //Needs to be after Skill Assessments are unlocked

		ReloadPlayerParamFiles();
	}

	m_needsInitPatchables = false;
}

void CGame::ReloadPlayerParamFiles()
{
	// to make the player params xml file patchable, reload it here. in SP also reload it, to ensure values don't leak from a patched MP session -> SP
#define PLAYER_PARAM_FILE               "Scripts/Entities/Actor/Parameters/Player_Params.xml"
#define PLAYER_ENTITY_CLASS_PARAMS_FILE "Scripts/Entities/Actor/Parameters/Player_EntityClassParams.xml"

	IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
	XmlNodeRef playerParams = GetISystem()->LoadXmlFromFile(PLAYER_PARAM_FILE);
	if (playerParams)
	{
		pActorSystem->ScanXML(playerParams, PLAYER_PARAM_FILE);
	}

	XmlNodeRef entityClassParams = GetISystem()->LoadXmlFromFile(PLAYER_ENTITY_CLASS_PARAMS_FILE);
	if (entityClassParams)
	{
		pActorSystem->ScanXML(entityClassParams, PLAYER_ENTITY_CLASS_PARAMS_FILE);
	}
}

void CGame::SetDifficultyLevel(EDifficulty difficulty)
{
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	assert(!gEnv->bMultiplayer);

	static const char* szDifficultyConfigs[eDifficulty_COUNT] =
	{
		"difficulty/normal.cfg",  // Default '0' for normal

		"difficulty/easy.cfg",
		"difficulty/normal.cfg",
		"difficulty/hard.cfg",
		"difficulty/delta.cfg",
		"difficulty/posthuman.cfg",
	};

	if (!gEnv->bMultiplayer)
	{
		if (difficulty < eDifficulty_Default || difficulty >= eDifficulty_COUNT)
			difficulty = eDifficulty_Default;

		const char* szConfig = szDifficultyConfigs[difficulty];

		CDifficultyConfigSink cvarSink("CGame::SetDifficultyLevel");
		CryLog("CGame: Loading singleplayer difficulty config \'%s\'", szConfig);
		gEnv->pSystem->LoadConfiguration(szConfig, &cvarSink, eLoadConfigGame);

		g_pGameCVars->g_difficultyLevel = difficulty;

		if (CProfileOptions* pOptions = g_pGame->GetProfileOptions())
		{
			int currProfileDif = pOptions->GetOptionValueAsInt("SP/Difficulty");
			if (difficulty != currProfileDif)
			{
				pOptions->SetOptionValue("SP/Difficulty", (int)difficulty);
				pOptions->SaveProfile(); // IAN: I don't think we should save here
			}
		}

		// Difficulty changed, so now record the easiest ever selected
		// Don't want player to get achievement for say hard difficulty if even played a little at easy
		EDifficulty difficultyExcludingDefault = difficulty;
		if (difficultyExcludingDefault == eDifficulty_Default)
		{
			difficultyExcludingDefault = eDifficulty_Normal; // Need to just be Easy,normal,hard,supersolider to determine lowest
		}

		if (g_pGameCVars->g_difficultyLevelLowestPlayed == -1) // Only way to reset, set -1 when changing difficulty in menu
		{
			g_pGameCVars->g_difficultyLevelLowestPlayed = difficultyExcludingDefault;
		}
		else
		{
			if (difficultyExcludingDefault < g_pGameCVars->g_difficultyLevelLowestPlayed)
			{
				g_pGameCVars->g_difficultyLevelLowestPlayed = difficultyExcludingDefault;
			}
		}
	}
}

#if ENABLE_VISUAL_DEBUG_PROTOTYPE
void CGame::UpdateVisualDebug(float deltaT)
{
	if (m_VisualDebugSys)
	{
		m_VisualDebugSys->Update(deltaT);
	}
}
#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE

void CGame::SetInviteAcceptedState(EInviteAcceptedState state)
{
	CryLog("[Invite] SetInviteAcceptedState %d to %d", m_inviteAcceptedState, state);

	m_inviteAcceptedState = state;
}

void CGame::SetInviteData(ECryLobbyService service, uint32 user, CryInviteID id, ECryLobbyError error, ECryLobbyInviteType inviteType)
{

	m_inviteAcceptedData.m_service = service;
	m_inviteAcceptedData.m_user = user;
	m_inviteAcceptedData.m_id = id;
	m_inviteAcceptedData.m_error = error;
	m_inviteAcceptedData.m_bannedFromSession = false;
	m_inviteAcceptedData.m_type = inviteType;
	m_inviteAcceptedData.m_failedToAcceptInviteAsNotSignedIn = false;

	if (m_pSquadManager && error == eCLE_Success && (inviteType == eCLIT_InviteToSquad))
	{
		m_pSquadManager->SetInvitePending(true);
	}
}

void CGame::InvalidateInviteData()
{
	CryLog("[Invite] InvalidateInviteData");

	SetInviteData(eCLS_Online, 0, CryInvalidInvite, eCLE_Success, eCLIT_InviteToSquad);

	if (m_pSquadManager)
	{
		m_pSquadManager->SetInvitePending(false);
	}

	CWarningsManager* pWarningsManager = GetWarnings();
	if (pWarningsManager)
	{
		pWarningsManager->RemoveGameWarning("InviteSelectController");
		pWarningsManager->RemoveGameWarning("ConfirmInvite");
	}

	m_bLoggedInFromInvite = false;
}

void CGame::SetInviteUserFromPreviousControllerIndex()
{
	CryLog("[Invite] SetInviteUserFromPreviousControllerIndex %d", m_previousInputControllerDeviceIndex);

	m_inviteAcceptedData.m_user = m_previousInputControllerDeviceIndex;
}

void CGame::UpdateInviteAcceptedState()
{
	if (GetInviteAcceptedState() != eIAS_None && m_inviteAcceptedData.m_error == eCLE_Success && m_inviteAcceptedData.m_id == CryInvalidInvite)
	{
		CryLog("[Invite] Join invite in progress, but session id is invalid, bailing...");

		SetInviteAcceptedState(eIAS_None);
		InvalidateInviteData(); // for safety
	}

	CWarningsManager* pWarningsManager = GetWarnings();
	switch (GetInviteAcceptedState())
	{
	case eIAS_None:
		{
			break;
		}

	case eIAS_Init:
		{
			m_inviteAcceptedData.m_failedToAcceptInviteAsNotSignedIn = false;
			SetInviteAcceptedState(eIAS_StartAcceptInvite);
			break;
		}

	case eIAS_ConfirmInvite:
		{
			// this isn't used anymore, but just in case it comes back, leaving here for now
			if (pWarningsManager && !g_pGame->GetUI()->IsLoading())  // don't show dialog while in the middle of loading, wait until ingame
			{
				pWarningsManager->RemoveGameWarning("ConfirmInvite");
				pWarningsManager->AddGameWarning("ConfirmInvite");
				SetInviteAcceptedState(eIAS_WaitForInviteConfirmation);
			}
			break;
		}

	case eIAS_WaitForInviteConfirmation:
		{
			// warning return will push this onto the next state
			break;
		}

	case eIAS_StartAcceptInvite:
		{
			const bool bChangeUser = false;

			if (pWarningsManager)
			{
				// invites are destructive acts that have to happen
				// (according to TCRs), we need to clear any active
				// warnings here as they are no longer relevant
				pWarningsManager->CancelWarnings();
			}

			// If we're in singleplayer or we need to change user, do existing behaviour
			if ((gEnv->bMultiplayer == false) || bChangeUser)
			{
				if (m_pUIManager && m_pUIManager->IsLoading())
				{
					CryLog("[Invite] Waiting for loading to finish");

					SetInviteAcceptedState(eIAS_WaitForLoadToFinish);
				}
				else if (GetIGameFramework()->StartedGameContext())
				{
					CryLog("[Invite] Accepting invite from in-game");

					SetInviteAcceptedState(eIAS_DisconnectGame);
				}
				else if (m_pGameLobbyManager->HaveActiveLobby())
				{
					CryLog("[Invite] Accepting invite from in-lobby");

					SetInviteAcceptedState(eIAS_DisconnectLobby);
				}
				else
				{
					CryLog("[Invite] Accepting invite from the menus");

					SetInviteAcceptedState(eIAS_InitSinglePlayer);
				}
			}
			else
			{
				if (m_inviteAcceptedData.m_error == eCLE_Success)
				{
					if (IsGameTypeFullyInitialised())
					{
						// theres a period during mp initialisation where
						// the squad manager is not enabled yet, we need to cope with that here
						SetInviteAcceptedState(eIAS_WaitForSquadManagerEnabled);
					}
					else
					{
						SetInviteAcceptedState(eIAS_WaitForInitOnline);
					}
				}
				else
				{
					SetInviteAcceptedState(eIAS_Error);
				}
			}

			break;
		}

	case eIAS_WaitForLoadToFinish:
		{
			if (m_pUIManager && !m_pUIManager->IsLoading())
			{
				SetInviteAcceptedState(eIAS_DisconnectGame);  // finished loading, kill the game off
			}
			break;
		}

	case eIAS_DisconnectGame:
		{
			if ((m_inviteAcceptedData.m_error == eCLE_Success) || (m_inviteAcceptedData.m_error == eCLE_UserNotSignedIn))
			{
				SetInviteAcceptedState(eIAS_WaitForSessionDelete);
			}
			else
			{
				CryLog("[invite] trying to disconnect game for invite, but invite was retrieved with error %d", m_inviteAcceptedData.m_error);

				// single player doesn't want disconnecting if the invite was retreived with error, if mp
				// and signed out, then should of already been returned to sp main menu anyways
				SetInviteAcceptedState(eIAS_Error);
			}

			break;
		}

	case eIAS_DisconnectLobby:
		{
			m_pGameLobbyManager->LeaveGameSession(CGameLobbyManager::eLSR_AcceptingInvite);
			SetInviteAcceptedState(eIAS_WaitForSessionDelete);
			break;
		}

	case eIAS_WaitForSessionDelete:
		{
			if (!m_pGameLobbyManager->HaveActiveLobby())
			{
				if (m_pUIManager && m_pUIManager->IsInMenu())
				{
					SetInviteAcceptedState(eIAS_InitSinglePlayer);
				}
			}
			break;
		}

	case eIAS_InitSinglePlayer:
		{
			if (m_inviteAcceptedData.m_error == eCLE_UserNotSignedIn)
			{
				CryLog("CGame::UpdateInviteAcceptedData() has found a user isn't signed in. We need to continue and switch to multiplayer, which after signing in should let us try accepting the invite again");
				m_inviteAcceptedData.m_failedToAcceptInviteAsNotSignedIn = true;
				m_inviteAcceptedData.m_error = eCLE_Success;  // we need to progress to switch to MP and sign in
			}

			if (m_inviteAcceptedData.m_error == eCLE_Success)
			{
				EInviteAcceptedState nextState = eIAS_WaitForInitSinglePlayer;

				if (gEnv->bMultiplayer)
				{
					if (HasExclusiveControllerIndex()) // demo will likely need this guard :(
					{
						CryLog("[invite] initialise single player as we are accepting an invite as a different user");
					}
				}
#if !defined(DEDICATED_SERVER)
				else if (HasExclusiveControllerIndex())
				{
					if (GetExclusiveControllerDeviceIndex() == m_inviteAcceptedData.m_user)
					{
						CryLog("[invite] accepting an invite with the current user in sp, switching to mp");

						nextState = eIAS_InitMultiplayer;
					}
					else
					{
						CryLog("[invite] in single player with a different user, heading back to splashscreen");

					}
				}
#endif

				SetInviteAcceptedState(nextState);
			}
			else
			{
				CryLog("[invite] trying to init singleplayer from invite, but invite was retrieved with error %d", m_inviteAcceptedData.m_error);
				SetInviteAcceptedState(eIAS_Error);
			}
			break;
		}

	case eIAS_WaitForInitSinglePlayer:
		{
			break;
		}

	case eIAS_WaitForSplashScreen:
		{
			if (m_gameDataInstalled)
			{
				if (m_inviteAcceptedData.m_error == eCLE_Success)
				{
					SetInviteAcceptedState(eIAS_WaitForValidUser);
				}
				else
				{
					CryLog("[invite] trying to init singleplayer from invit, but invite was retrieved with error %d", m_inviteAcceptedData.m_error);

					SetInviteAcceptedState(eIAS_Error);
				}
			}
			break;
		}

	case eIAS_WaitForValidUser:
		{
			if (m_inviteAcceptedData.m_user != INVALID_CONTROLLER_INDEX)
			{
				SetPreviousExclusiveControllerDeviceIndex(m_inviteAcceptedData.m_user); // set the controller
				SetInviteAcceptedState(eIAS_WaitForInitProfile);                        // wait
			}
			break;
		}

	case eIAS_WaitForInitProfile:
		{

			if (m_bLoggedInFromInvite) // not convinced this is needed anymore, we now just wait for main and don't progress until file writing is done
			{
				// wait until we reach the single player main menu
				{
					SetInviteAcceptedState(eIAS_InitMultiplayer);
				}
			}
			break;
		}

	case eIAS_InitMultiplayer:
		{
			if (!g_pGameCVars->g_multiplayerModeOnly)
			{
			}

			SetInviteAcceptedState(eIAS_WaitForInitMultiplayer);
			CGameLobby::SetLobbyService(eCLS_Online);
			break;
		}

	case eIAS_WaitForInitMultiplayer:
		{
			// need to wait for the multiplayer menu screen to actually load
			{
				if (gEnv->bMultiplayer) // :(
				{
					SetInviteAcceptedState(eIAS_InitOnline);
					if (m_inviteAcceptedData.m_type == eCLIT_InviteToSquad)
					{
						m_pSquadManager->SetInvitePending(true);
					}
				}
			}
			break;
		}

	case eIAS_InitOnline:
		{
			SetInviteAcceptedState(eIAS_WaitForInitOnline);
			break;
		}

	case eIAS_WaitForInitOnline:
		{
			// MP Loader sets accept invite now
			break;
		}

	case eIAS_WaitForSquadManagerEnabled:
		{
			CRY_ASSERT(gEnv->bMultiplayer);

			if (m_pSquadManager->IsEnabled())
			{
				// if we're loading, then need to wait, it does not end well otherwise
				SetInviteAcceptedState(eIAS_Accept);
			}
			break;
		}

	case eIAS_Accept:
		{
			bool failedAsNotSignedIn = m_inviteAcceptedData.m_failedToAcceptInviteAsNotSignedIn;

			if (failedAsNotSignedIn)
			{
				CryLog("CGame::UpdateInviteAcceptedState() state accept. yet we failed to accept as not signed in. We need to now get our invite from the lobbyUI if we can.");

				SetInviteAcceptedState(eIAS_None);
				InvalidateInviteData();
			}
			else
			{
				m_pSquadManager->InviteAccepted(m_inviteAcceptedData.m_id);

				SetInviteAcceptedState(eIAS_None);
				InvalidateInviteData();
			}

			break;
		}

	case eIAS_Error:
		{
			if (pWarningsManager)
			{
				pWarningsManager->RemoveGameWarning("InviteNotSignedIn");
				pWarningsManager->RemoveGameWarning("InviteInvalidRequest");

				if (m_inviteAcceptedData.m_bannedFromSession)
				{
					pWarningsManager->AddGameWarning("BannedFromSquad");
				}
				else
				{
					switch (m_inviteAcceptedData.m_error)
					{
					case eCLE_UserNotSignedIn:    // user accepted the invite while in a signed out state
						pWarningsManager->AddGameWarning("InviteNotSignedIn");
						break;

					case eCLE_InvalidInviteFriendData:
					case eCLE_InvalidJoinFriendData:
						pWarningsManager->AddGameWarning("InviteInvalidRequest");
						break;

					default:
						CryLog("[invite] unexpected error %d passed into invite data", m_inviteAcceptedData.m_error);
						break;
					}
				}

				InvalidateInviteData();
			}
			break;
		}

	default:
		{
			CryLog("[Invite] unknown invite accepted state");
			break;
		}
	}
}

void CGame::UpdateSaveIcon()
{
	if (!gEnv->pRenderer)
		return;

	static const float MAX_ICON_DISPLAY_TIME = 10.0f; // TODO: Failsafe. If required, fine tune this to the game.
	float currentTime = gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetSeconds();
	bool bTooLong = currentTime - m_saveIconTimer >= MAX_ICON_DISPLAY_TIME;
	bool bSetIcon = false;
	bool bEnableIcon = false;

	if (m_saveIconMode != eSIM_Off)
	{
		if (m_saveIconMode == eSIM_SaveStart)
		{
			bSetIcon = true;
			bEnableIcon = true;
			bTooLong = false;
			m_saveIconMode = eSIM_Saving;
			m_saveIconTimer = currentTime;
		}

		if (bTooLong || m_saveIconMode == eSIM_Finished)
		{
			if (bTooLong || currentTime - m_saveIconTimer >= IPlatformOS::MINIMUM_SAVE_ICON_DISPLAY_TIME) // must be visible for this long regardless of how long the write takes
			{
				bSetIcon = true;
				bEnableIcon = false;
				bTooLong = false;
				m_saveIconMode = eSIM_Off;
				m_saveIconTimer = currentTime;
			}
		}
	}
	else
	{
		// In some instances the event isn't caught due to the flash object not existing. Send the message every MAX_ICON_DISPLAY_TIME secs to ensure they go away.
		if (bTooLong)
		{
			bSetIcon = true;
			bEnableIcon = false;
			m_saveIconMode = eSIM_Off;
			m_saveIconTimer = currentTime;
		}
	}

	if (bSetIcon)
	{
		if (bEnableIcon && m_bCheckPointSave)
		{
			m_bCheckPointSave = false;
		}

		if (m_bUserHasPhysicalStorage) // Only show icon if there is physical storage
		{
			// Always set the icon state, since at certain points the show/hide may not work depending on if the flash object exists
			SHUDEvent hudEvent;
			hudEvent.eventType = eHUDEvent_OnFileIO;
			hudEvent.AddData(bEnableIcon);
			CHUDEventDispatcher::CallEvent(hudEvent);
		}
	}
}

//warning 6262 needs an investigation
int CGame::Update(bool haveFocus, unsigned int updateFlags) PREFAST_SUPPRESS_WARNING(6262)
{
	CryProfile::ProfilerFrameStart(gEnv->nMainFrameID);

#if defined(USER_timf)
	if (m_needMultiplayerFrontEndAssets)
	{
		assert(gEnv->bMultiplayer);
		assert(m_allowedToLoadMultiplayerFrontEndAssets);
		CryWatch("%s: $3Multiplayer front-end assets are currently loaded", gEnv->bMultiplayer ? "MULTIPLAYER" : "SINGLE PLAYER");
	}
	else if (m_allowedToLoadMultiplayerFrontEndAssets)
	{
		CryWatch("%s: $6Loading of multiplayer front-end assets is allowed but assets are not currently loaded", gEnv->bMultiplayer ? "MULTIPLAYER" : "SINGLE PLAYER");
	}
	else
	{
		CryWatch("%s: $7Loading of front-end assets is not currently allowed", gEnv->bMultiplayer ? "MULTIPLAYER" : "SINGLE PLAYER");
	}
#endif

#if !defined(DEDICATED_SERVER)
	if (!gEnv->bMultiplayer && m_bDeferredSystemMenuPause)
	{
		m_bDeferredSystemMenuPause = false;
		if (m_bPausedForSystemMenu)
		{
			gEnv->pGameFramework->PauseGame(true, false);
		}
		else if (!m_previousPausedGameState)
		{
			gEnv->pGameFramework->PauseGame(false, false);
		}
	}
#endif

	if (gEnv->bMultiplayer)
	{
		static float s_startTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
		const float nowTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
		const float elapsedTime = nowTime - s_startTime;
		const char* const elapsedTimeMsg = GetTimeString(elapsedTime, true);
		SetCrashDebugMessage(elapsedTimeMsg);
		CGameLobby* pGameLobby = g_pGame->GetGameLobby();
		if (pGameLobby)
		{
			pGameLobby->UpdateDebugString();
		}
	}

	if (m_pDLCManager)
	{
		m_pDLCManager->Update();
	}

#if USE_LAGOMETER
	if (m_pLagOMeter)
	{
		m_pLagOMeter->Update();
	}
#endif

	//bool bRun = gEnv->pGameFramework->PreUpdate(true, updateFlags);

	float frameTime = gEnv->pTimer->GetFrameTime();

	if (m_pMovingPlatformMgr)
		m_pMovingPlatformMgr->Update(frameTime);

	{
		if (m_pRayCaster)
		{
			CRY_PROFILE_REGION(PROFILE_GAME, "GlobalRayCaster");

			m_pRayCaster->SetQuota(g_pGameCVars->g_gameRayCastQuota);
			m_pRayCaster->Update(frameTime);
		}

		if (m_pIntersectionTester)
		{
			CRY_PROFILE_REGION(PROFILE_GAME, "GlobalIntersectionTester");

			m_pIntersectionTester->SetQuota(g_pGameCVars->g_gameIntersectionTestQuota);
			m_pIntersectionTester->Update(frameTime);
		}

#ifdef ENABLE_PROFILING_CODE    // will be off in release build, but on in debug, profile and perf builds
		if (g_pGameCVars->g_telemetryDisplaySessionId)
		{
			ColorF color = Col_White;
			const float x0 = 60.0f;
			const float y0 = 30.0f;
			const float scale = 1.50f;

			stack_string text;
			CTelemetryCollector* const tc = static_cast<CTelemetryCollector*>(GetITelemetryCollector());
			if (tc)
			{
				if (tc->AreTransfersInProgress() && int(gEnv->pTimer->GetAsyncCurTime() * 2.0f) & 1)
				{
					text.Format("SessionID: %s ** UPLOADING DATA ** ", tc->GetSessionId().c_str());
				}
				else
				{
					text.Format("SessionID: %s", tc->GetSessionId().c_str());
				}
			}
			else
			{
				text.Format("SessionID: NULL");
			}

			IRenderAuxText::Draw2dLabel(x0, y0, scale, &color.r, false, "%s", text.c_str());
		}
#endif

		if (g_pGameCVars->g_DebugDrawPhysicsAccess)
		{
			stack_string text;

			if (m_pRayCaster)
			{
				GlobalRayCaster::ContentionStats rstats = m_pRayCaster->GetContentionStats();
				text.Format(
				  "RayCaster\n"
				  "---------\n"
				  "Quota: %d\n"
				  "Queue Size: %d / %d\n"
				  "Immediate Count: %d / %d\n"
				  "Immediate Average: %.1f\n"
				  "Deferred Count: %d / %d\n"
				  "Deferred Average: %.1f",
				  rstats.quota,
				  rstats.queueSize,
				  rstats.peakQueueSize,
				  rstats.immediateCount,
				  rstats.peakImmediateCount,
				  rstats.immediateAverage,
				  rstats.deferredCount,
				  rstats.peakDeferredCount,
				  rstats.deferredAverage);

				bool warning = (rstats.immediateCount + rstats.deferredCount) > rstats.quota;
				warning = warning || (rstats.immediateAverage + rstats.deferredAverage) > rstats.quota;
				warning = warning || rstats.queueSize > (3 * rstats.quota);

				ColorF color = warning ? Col_Red : Col_DarkTurquoise;
				IRenderAuxText::Draw2dLabel(400.f, 40.f, 1.25f, &color.r, false, "%s", text.c_str());
			}

			if (m_pIntersectionTester)
			{
				GlobalIntersectionTester::ContentionStats istats = m_pIntersectionTester->GetContentionStats();
				text.Format(
				  "IntersectionTester\n"
				  "------------------\n"
				  "Quota: %d\n"
				  "Queue Size: %d / %d\n"
				  "Immediate Count: %d / %d\n"
				  "Immediate Average: %.1f\n"
				  "Deferred Count: %d / %d\n"
				  "Deferred Average: %.1f",
				  istats.quota,
				  istats.queueSize,
				  istats.peakQueueSize,
				  istats.immediateCount,
				  istats.peakImmediateCount,
				  istats.immediateAverage,
				  istats.deferredCount,
				  istats.peakDeferredCount,
				  istats.deferredAverage);

				bool warning = (istats.immediateCount + istats.deferredCount) > istats.quota;
				warning = warning || (istats.immediateAverage + istats.deferredAverage) > istats.quota;
				warning = warning || istats.queueSize > (3 * istats.quota);

				ColorF color = warning ? Col_Red : Col_DarkTurquoise;
				IRenderAuxText::Draw2dLabel(600.0, 745.0f, 1.25f, &color.r, false, "%s", text.c_str());
			}
		}
	}

	UpdateInviteAcceptedState();

	float rpTime = m_updateRichPresenceTimer > 0.f ? m_updateRichPresenceTimer - frameTime : m_updateRichPresenceTimer;
	m_updateRichPresenceTimer = rpTime;

	if (m_desiredRichPresenceState != eRPS_none)
	{
		if (rpTime <= 0.f)
		{
			// at present the lobby ui is capable of running only
			// one task at a time, so it is possible for SetRichPresence
			// to fail. To counter this, we store the desired state and try again
			// each frame until we succeed
			if (SetRichPresence(m_desiredRichPresenceState))
			{
				m_desiredRichPresenceState = eRPS_none;
			}
		}
	}

	if (GetUserProfileChanged())
	{
#if !defined(DEDICATED_SERVER)
		if (!gEnv->bMultiplayer)
		{
			m_lastSaveGame.clear();
			SetUserProfileChanged(false);
		}
		else
#endif
		{
			IGameFramework* pFramework = GetIGameFramework();
			if (pFramework->StartedGameContext()) // use this to determine if we are still in a game
			{
				IActor* pActor = pFramework->GetClientActor();
				if (pActor) // the entity name can't be changed if we don't have it yet
				{
					IPlatformOS::TUserName tUserName;
					uint32 userIndex = GetExclusiveControllerDeviceIndex();

					gEnv->pSystem->GetPlatformOS()->UserGetOnlineName(userIndex, tUserName);

					CGameRules* pGameRules = GetGameRules();
					if (pGameRules)
					{
						pGameRules->RenamePlayer(pActor, tUserName.c_str());
					}

					SetUserProfileChanged(false);
				}
			}
			else
			{
				SetUserProfileChanged(false);
			}
		}
	}

	if (gEnv->pGameFramework->IsGamePaused() == false)
	{
		m_pWeaponSystem->Update(frameTime);

		m_pScreenEffects->Update(frameTime);

		m_pPlayerVisTable->Update(frameTime);

#if !defined(DEDICATED_SERVER)
		if (!gEnv->bMultiplayer)
		{
			if (gEnv->pGameFramework->StartedGameContext())
				m_pGameAISystem->Update(frameTime);
		}
#endif

		m_pBurnEffectManager->Update(frameTime);

		CItemAnimationDBAManager& itemDbaManager = m_pGameParametersStorage->GetItemResourceCache().Get1pDBAManager();
		CItemPrefetchCHRManager& itemPfManager = m_pGameParametersStorage->GetItemResourceCache().GetPrefetchCHRManager();

		float fCurrTime = gEnv->pTimer->GetCurrTime();
		itemDbaManager.Update(fCurrTime);
		itemDbaManager.Debug();

		itemPfManager.Update(fCurrTime);

		m_pGameCache->Debug();
	}
	else
	{
		if (m_hostMigrationState == eHMS_WaitingForPlayers)
		{
			if (gEnv->bServer)
			{
				if (GetRemainingHostMigrationTimeoutTime() <= 0.f)
				{
					CryLog("CGame: HostMigration timeout reached");
					SetHostMigrationState(eHMS_Resuming);
				}
			}
		}
		else if (m_hostMigrationState == eHMS_Resuming)
		{
			const float curTime = gEnv->pTimer->GetAsyncCurTime();
			const float timePassed = curTime - m_hostMigrationTimeStateChanged;
			const float timeRemaining = g_pGameCVars->g_hostMigrationResumeTime - timePassed;
			if (timeRemaining > 0.f)
			{
				SHUDEvent resumingEvent(eHUDEvent_OnUpdateGameResumeMessage);
				int time = std::max(int(floor(timeRemaining + 0.5f)), 0);
				resumingEvent.AddData(time);
				CHUDEventDispatcher::CallEvent(resumingEvent);
			}
			else
			{
				SetHostMigrationState(eHMS_NotMigrating);
			}
		}
	}

	// Some effects need to be updated when game is paused, so use UI frame time to update Game Effects System
	float frameTimeNoPause = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);
	GAME_FX_SYSTEM.Update(frameTimeNoPause);

	UpdateSaveIcon();

	m_gameMechanismManager->Update(frameTime);

	if (m_pRecordingSystem)
	{
		m_pRecordingSystem->Update(frameTime);
	}

	m_pPersistantStats->Update(frameTime);

	if (m_pPlaylistActivityTracker)
	{
		m_pPlaylistActivityTracker->Update(frameTime);
	}

#if !defined(_RELEASE)
	m_pRemoteUpdateListener->Update();
#endif

	if (gEnv->IsClient() && !gEnv->bServer && g_pGameCVars->sv_pacifist)
	{
		IRenderAuxText::Draw2dLabel(10, 10, 4, Col_White, false, "PACIFIST MODE ENABLED (Actors don't get damage)");
	}

	//gEnv->pGameFramework->PostUpdate(true, updateFlags);

	//--- Moved this from the earlier is not paused block to it stop querying joint positions mid-update.
	//--- Ideally this should not be a problem & then this can move back where it was.
	if (gEnv->pGameFramework->IsGamePaused() == false)
	{
		m_pAutoAimManager->Update(frameTime);
	}

	if (m_pGameBrowser)
	{
		m_pGameBrowser->Update(frameTime);
	}

	if (m_pRecordingSystem)
	{
		m_pRecordingSystem->PostUpdate();
	}

	{
		//Beni - For some reason, radial blur FX parameters have to be updated here (not during update)
		m_pScreenEffects->PostUpdate(frameTime);
	}

	if (m_inDevMode != gEnv->pSystem->IsDevMode())
	{
		m_inDevMode = gEnv->pSystem->IsDevMode();
	}

	IActionMapManager* pAMM = gEnv->pGameFramework->GetIActionMapManager();
	CRY_ASSERT(pAMM);
	IActionMap* pAM = pAMM->GetActionMap("debug");
	if (pAM && pAM->Enabled() != m_inDevMode)
	{
		pAM->Enable(m_inDevMode);
	}

	CheckReloadLevel();

#ifndef _RELEASE
	if (gEnv->bMultiplayer)
	{
		CPlayerProgression::GetInstance()->UpdateDebug();
	}
#endif //#ifndef _RELEASE

#if NET_PROFILE_ENABLE
	if (gEnv->bMultiplayer && !gEnv->IsEditor() && (gEnv->pConsole->GetCVar("net_profile_logging")->GetIVal() != 0) && m_telemetryCollector)
	{
		static ICVar* pName = gEnv->pConsole->GetCVar("net_profile_logname");
		static CTimeValue s_timeValue = gEnv->pTimer->GetAsyncCurTime();
		CTimeValue timeValue = gEnv->pTimer->GetAsyncCurTime();

		if (timeValue - s_timeValue >= CTimeValue(10.f))
		{
			CryFixedStringT<1024> filename;
			const char* fullFilename = pName->GetString();
			const char* forwardSlash = strrchr(fullFilename, '/');
			const char* backSlash = strrchr(fullFilename, '\\');
			const char* lastSlash = (forwardSlash > backSlash) ? forwardSlash : backSlash;
			lastSlash = (lastSlash) ? lastSlash : fullFilename;
			filename.Format("./%s", lastSlash);
			m_telemetryCollector->SubmitFile(filename.c_str(), filename.c_str());
			s_timeValue = timeValue;
		}
	}
#endif // #if NET_PROFILE_ENABLE

#if USE_TELEMETRY_BUFFERS
	if (!gEnv->IsEditor() && (m_performanceBuffer || m_bandwidthBuffer || m_memoryTrackingBuffer || m_soundTrackingBuffer))
	{
		CGameRules* cgr = GetGameRules();
		if (cgr)
		{
			const CTimeValue timeValue = gEnv->pTimer->GetAsyncTime();

			CTimeValue deltaTime;
			const float serverTimeInSeconds = cgr->GetServerTime() / 1000.0f;
			const bool updatePerfTelemetry = g_pGameCVars->g_telemetry_onlyInGame ? (cgr->IsGameInProgress() && serverTimeInSeconds >= 0.1f) : true;
			const bool updateBandwidthTelemetry = updatePerfTelemetry;

			// performance
			if (m_performanceBuffer && updatePerfTelemetry)
			{
				static float s_gpuTime = 0.f;
				static int s_gpuLimited = 0;
				static int framerateFrameCount = 0;
				static int s_drawcallCount = 0;
				static int s_drawcallOverbudget = 0;
				static float s_renderThreadTime = 0.0f;
				static float s_waitForRenderTime = 0.0f;
				static float s_waitForGPUTime = 0.0f;
				IF_UNLIKELY (m_secondTimePerformance.GetValue() == 0)
				{
					// Reset counters to zero if this is the first update
					s_gpuTime = 0.f;
					s_gpuLimited = 0;
					framerateFrameCount = 0;
					s_drawcallCount = 0;
					s_drawcallOverbudget = 0;
					s_renderThreadTime = 0.0f;
					s_waitForRenderTime = 0.0f;
					s_waitForGPUTime = 0.0f;
					m_secondTimePerformance = timeValue;
				}
				else
				{
					framerateFrameCount++;

					//Render stats must be updated each frame
					IRenderer* pRenderer = gEnv->pRenderer;
					if (pRenderer)
					{
						IRenderer::SRenderTimes renderTimes;

						pRenderer->GetRenderTimes(renderTimes);

						const float fRenderThreadTime = renderTimes.fTimeProcessedRT;
						const float fWaitForRender = renderTimes.fWaitForRender;

						if (fRenderThreadTime > 0.0f)
						{
							s_renderThreadTime += fRenderThreadTime;
						}
						if (fWaitForRender > 0.0f)
						{
							s_waitForRenderTime += fWaitForRender;
						}

						const float gpuFrameTime = pRenderer->GetGPUFrameTime();

						if (gpuFrameTime > 0.f)
						{
							s_gpuTime += gpuFrameTime;
						}
						s_waitForGPUTime += renderTimes.fWaitForGPU;

						//wait for GPU is never zero, using small epsilon to determine if GPU Limited
						const float GPU_EPSILON = 0.001f;
						s_gpuLimited += (renderTimes.fWaitForGPU > GPU_EPSILON) ? 1 : 0;

						int nGeneralDrawcalls = 0;
						int nShadowDrawcalls = 0;
						pRenderer->GetCurrentNumberOfDrawCalls(nGeneralDrawcalls, nShadowDrawcalls);
						const int totalDrawcalls = nGeneralDrawcalls + nShadowDrawcalls;
						s_drawcallCount += totalDrawcalls;
						if (totalDrawcalls > g_pGameCVars->g_telemetry_drawcall_budget)
						{
							s_drawcallOverbudget++;
						}
					}
				}

				const float sampleTimePerformance = g_pGameCVars->g_telemetrySampleRatePerformance;
				deltaTime = timeValue - m_secondTimePerformance;
				if (deltaTime >= sampleTimePerformance)
				{
					SPerformanceTelemetry spt;
					spt.m_timeInSeconds = serverTimeInSeconds;
					spt.m_numTicks = framerateFrameCount;
					spt.m_gpuTime = s_gpuTime;
					spt.m_gpuLimited = s_gpuLimited;
					spt.m_drawcalls = s_drawcallCount;
					spt.m_drawcallOverbudget = s_drawcallOverbudget;
					spt.m_deltaTime = deltaTime.GetSeconds();
					spt.m_renderThreadTime = s_renderThreadTime;
					spt.m_mainThreadTime = spt.m_deltaTime - s_waitForRenderTime;
					spt.m_waitForGPUTime = s_waitForGPUTime;

					m_performanceBuffer->AddData(&spt);

					m_secondTimePerformance = timeValue;
					framerateFrameCount = 0;
					s_gpuTime = 0.f;
					s_gpuLimited = 0;
					s_drawcallCount = 0;
					s_drawcallOverbudget = 0;
					s_renderThreadTime = 0.0f;
					s_waitForRenderTime = 0.0f;
					s_waitForGPUTime = 0.0f;
				}
			}

			// bandwidth
			if (m_bandwidthBuffer && updateBandwidthTelemetry)
			{
				IF_UNLIKELY (m_secondTimeBandwidth.GetValue() == 0)
				{
					m_secondTimeBandwidth = timeValue;
				}

				const float sampleTimeBandwidth = g_pGameCVars->g_telemetrySampleRateBandwidth;
				deltaTime = timeValue - m_secondTimeBandwidth;
				if (deltaTime >= sampleTimeBandwidth)
				{
					INetwork* pNetwork = gEnv->pNetwork;
					SBandwidthStats stats;
					pNetwork->GetBandwidthStatistics(&stats);

					SBandwidthTelemetry sbt;
					sbt.m_timeInSeconds = serverTimeInSeconds;
					sbt.m_bandwidthReceived = stats.m_total.m_totalBandwidthRecvd;
					sbt.m_bandwidthSent = stats.m_total.m_totalBandwidthSent;
					sbt.m_packetsSent = stats.m_total.m_totalPacketsSent;
					sbt.m_deltaTime = deltaTime.GetSeconds();
					m_bandwidthBuffer->AddData(&sbt);

					m_secondTimeBandwidth = timeValue;
				}
			}

			// memory
			if (m_memoryTrackingBuffer)
			{
				IF_UNLIKELY (m_secondTimeMemory.GetValue() == 0)
				{
					m_secondTimeMemory = timeValue;
				}

				const float sampleTimeMemory = g_pGameCVars->g_telemetrySampleRateMemory;
				deltaTime = timeValue - m_secondTimeMemory;
				if (deltaTime >= sampleTimeMemory)
				{
					IMemoryManager::SProcessMemInfo processMemInfo;
					GetISystem()->GetIMemoryManager()->GetProcessMemInfo(processMemInfo);
					const float cpuMemUsedInMB = (float)(processMemInfo.PagefileUsage) / (1024.0f * 1024.0f);
					const float gpuMemUsedInMB = 0.0f;

					SMemoryTelemetry memTelem;
					memTelem.m_timeInSeconds = serverTimeInSeconds;
					memTelem.m_cpuMemUsedInMB = cpuMemUsedInMB;
					memTelem.m_gpuMemUsedInMB = gpuMemUsedInMB;
					m_memoryTrackingBuffer->AddData(&memTelem);

					m_secondTimeMemory = timeValue;
				}
			}

			// sound
			REINST("needs verification!");
			/*if(m_soundTrackingBuffer)
			   {
			   IF_UNLIKELY(m_secondTimeSound.GetValue() == 0)
			   {
			    m_secondTimeSound = timeValue;
			   }

			   deltaTime = timeValue - m_secondTimeSound;
			   if(deltaTime >= g_pGameCVars->g_telemetrySampleRateSound)
			   {
			    ISoundSystem_Extended* pExt = gEnv->pSoundSystem->GetInterfaceExtended();
			    if(pExt)
			    {
			      SSoundTelemetry soundTelem;
			      pExt->GetMemoryInfo(&soundTelem.m_soundInfo);
			      m_soundTrackingBuffer->AddData(&soundTelem);
			    }

			    m_secondTimeSound = timeValue;
			   }
			   }*/
		}
	}
#endif //#if USE_TELEMETRY_BUFFERS

	if (m_telemetryCollector)
	{
		m_telemetryCollector->Update();
	}
	if (m_statsRecorder)
	{
		m_statsRecorder->Update(frameTime);
	}
	if (m_patchPakManager)
	{
		m_patchPakManager->Update(frameTime);
	}

	if (gEnv->pRenderer)
	{
		Stereo3D::Update(frameTime);
	}

	CommitDeferredKills();

#ifndef NO_LIVECREATE
	LiveCreateUpdate();
#endif

	CryProfile::ProfilerFrameEnd(gEnv->nMainFrameID);

	return 1;
}

void CGame::EditorResetGame(bool bStart)
{
	CRY_ASSERT(gEnv->IsEditor());

	if (bStart)
	{
		IActionMapManager* pAM = gEnv->pGameFramework->GetIActionMapManager();
		if (pAM)
		{
			pAM->InitActionMaps(pAM->GetLoadFromXMLPath());
			pAM->EnableActionMap("player", true); // enable default movement action map
			pAM->EnableFilter(0, false);          // disable all filters

			const bool bMultiplayer = gEnv->bMultiplayer;
			pAM->EnableActionMap("multiplayer", bMultiplayer);
			pAM->EnableActionMap("singleplayer", !bMultiplayer);
		}

		// load & reset hud and related data.
		if (m_pUIManager)
		{
			m_pUIManager->ActivateDefaultState();
			SHUDEvent event;
			event.eventType = eHUDEvent_OnHUDReload;
			event.AddData(SHUDEventData(false));
			event.AddData(SHUDEventData(false));   // Dynamically loaded
			CHUDEventDispatcher::CallEvent(event);
			m_pUIManager->GetLayoutManager()->UpdateHUDCanvasSize();
		}
	}
	else
	{
		gEnv->pConsole->ShowConsole(false);

		IActionMapManager* pAM = gEnv->pGameFramework->GetIActionMapManager();
		if (pAM)
		{
			pAM->EnableActionMap(0, false); // disable all action maps
			pAM->EnableFilter(0, false);    // disable all filters
		}
		GetMOSystem()->Reset();

		if (m_pTacticalManager)
		{
			m_pTacticalManager->ResetAllTacticalPoints();
			m_pTacticalManager->ResetClassScanningData();
		}

		m_pBurnEffectManager->Reset();

		m_pAutoAimManager->OnEditorReset();

		if (CGameRules* gameRules = GetGameRules())
		{
			if (CCorpseManager* corpseManager = gameRules->GetCorpseManager())
			{
				corpseManager->ClearCorpses();
			}
		}

		if (CAICorpseManager* aiCorpseManager = CAICorpseManager::GetInstance())
		{
			aiCorpseManager->RemoveAllCorpses("CGame::EditorResetGame");
		}
	}

	if (m_pMovingPlatformMgr)
		m_pMovingPlatformMgr->Reset();
}

void CGame::Shutdown()
{
#ifdef INCLUDE_GAME_AI_RECORDER
	assert(gEnv->bMultiplayer || m_pGameAISystem);
	if (m_pGameAISystem)
	{
		m_pGameAISystem->GetGameAIRecorder().Shutdown();
	}
#endif //INCLUDE_GAME_AI_RECORDER

	if (gEnv->IsEditor())
	{
		g_tacticalPointLanguageExtender.Deinitialize();
	}

	if (m_pPlayerProfileManager)
	{
		m_pPlayerProfileManager->LogoutUser(m_pPlayerProfileManager->GetCurrentUser());
	}

	IVehicleSystem* pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
	IVehicleClient* pVehicleClient = pVehicleSystem->GetVehicleClient();
	pVehicleSystem->RegisterVehicleClient(NULL);

	//  manually update player vehicle client to NULL to avoid access violation in CPlayerInput::OnAction() when ALT+F4
	if (CPlayer* pPlayer = static_cast<CPlayer*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(gEnv->pGameFramework->GetClientActorId())))
	{
		pPlayer->RegisterVehicleClient(NULL);
	}

	SAFE_DELETE(pVehicleClient);

	if (m_pPlaylistManager)
	{
		SAFE_DELETE(m_pPlaylistManager);
	}

	CBullet::StaticShutdown();

	CFrontEndModelCache::Allow3dFrontEndAssets(false, true);

	gEnv->pGameFramework->ReleaseExtensions();

	this->~CGame();
}

const char* CGame::GetLongName()
{
	return GAME_LONGNAME;
}

const char* CGame::GetName()
{
	return GAME_NAME;
}

EPlatform CGame::GetPlatform() const
{
	return EPlatform::Current;
}

void CGame::InitPlatformOS()
{
	static const char s_encryptionMagic[] = { 'C', 'R', 'Y', '3', 'S', 'D', 'K' };     //2 for release, 1 for demo

	static const uint64 s_saveEncryptionKey[] = {
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
		0x000000000000000000, 0x000000000000000000, 0x000000000000000000, 0x000000000000000000,
	};

	assert(GetISystem()->GetPlatformOS());
	GetISystem()->GetPlatformOS()->InitEncryptionKey(s_encryptionMagic, sizeof(s_encryptionMagic), (uint8*)s_saveEncryptionKey, sizeof(s_saveEncryptionKey));
}

void CGame::OnPostUpdate(float fDeltaTime)
{
#if ENABLE_VISUAL_DEBUG_PROTOTYPE
	UpdateVisualDebug(fDeltaTime);
#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE
}

void CGame::OnSaveGame(ISaveGame* pSaveGame)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetIGameFramework()->GetClientActor());
	GetGameRules()->PlayerPosForRespawn(pPlayer, true);

	//save difficulty
	pSaveGame->AddMetadata("sp_difficulty", g_pGameCVars->g_difficultyLevel);
	pSaveGame->AddMetadata("sp_difficultylowestplayed", g_pGameCVars->g_difficultyLevelLowestPlayed);
	const char* levelName = g_pGame->GetIGameFramework()->GetLevelName();
	if (levelName && levelName[0])
	{
		pSaveGame->AddMetadata("sp_levelname", levelName);
	}

	//write file to profile
	if (m_pPlayerProfileManager)
	{
		const char* saveGameFolder = m_pPlayerProfileManager->GetSharedSaveGameFolder();
		const bool bSaveGameFolderShared = saveGameFolder && *saveGameFolder;
		const char* user = m_pPlayerProfileManager->GetCurrentUser();
		if (IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(user))
		{
			string filename(pSaveGame->GetFileName());
			CryFixedStringT<128> profilename(pProfile->GetName());
			profilename += '_';
			filename = filename.substr(filename.rfind('/') + 1);
			// strip profileName_ prefix
			if (bSaveGameFolderShared)
			{
				if (strnicmp(filename.c_str(), profilename.c_str(), profilename.length()) == 0)
					filename = filename.substr(profilename.length());
			}
			pProfile->SetAttribute("Singleplayer.LastSavedGame", filename);
		}

		m_bLastSaveDirty = true;
	}

	pSaveGame->AddMetadata("v_altitudeLimit", g_pGameCVars->pAltitudeLimitCVar->GetString());

	m_bLastSaveDirty = true;
}

void CGame::OnLoadGame(ILoadGame* pLoadGame)
{
	CHUDMissionObjectiveSystem* pMOSystem = GetMOSystem();
	if (pMOSystem)
	{
		pMOSystem->DeactivateObjectives(false);
	}

	// Ian: reload game side profile persistent stats to prevent exploits.
	IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
	if (pProfile)
		m_pPlayerProfileManager->ReloadProfile(pProfile, ePR_Game);

	bool isLastSaveGame = true;

#if CRY_PLATFORM_WINDOWS
	// Check to see if we're loading the last save game.
	string saveGame = pLoadGame->GetFileName(), lastSaveGame = GetLastSaveGame();
	size_t pos = saveGame.find_last_of("\\/");
	if (pos != string::npos)
	{
		++pos;
		saveGame = saveGame.substr(pos, saveGame.length() - pos);
	}
	saveGame.MakeLower();
	lastSaveGame.MakeLower();
	isLastSaveGame = (saveGame == lastSaveGame);
#endif

	// dificulty level from the savegame now always overwrites the value in the user profile.
	{
		int difficulty = g_pGameCVars->g_difficultyLevel;
		pLoadGame->GetMetadata("sp_difficulty", difficulty);
		if (!isLastSaveGame) // to avoid exploits
			pLoadGame->GetMetadata("sp_difficultylowestplayed", g_pGameCVars->g_difficultyLevelLowestPlayed);
		SetDifficultyLevel((EDifficulty)difficulty);
		if (pProfile)
		{
			pProfile->SetAttribute("Singleplayer.LastSelectedDifficulty", difficulty);
			pProfile->SetAttribute("Option.g_difficultyLevel", difficulty);
			IPlayerProfileManager::EProfileOperationResult result;
			m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result, ePR_Options);
		}
	}

	// altitude limit
	const char* v_altitudeLimit = pLoadGame->GetMetadata("v_altitudeLimit");
	if (v_altitudeLimit && *v_altitudeLimit)
	{
		g_pGameCVars->pAltitudeLimitCVar->ForceSet(v_altitudeLimit);
	}
	else
	{
		char buf[64];
		cry_sprintf(buf, "%g", g_pGameCVars->v_altitudeLimitDefault());
		g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf);
	}
}

void CGame::OnSavegameFileLoadedInMemory(const char* pLevelName)
{
}

// All input is filtered through this function, if return true then other listeners will not recieve the input
bool CGame::OnInputEvent(const SInputEvent& inputEvent)
{
	bool isGamePadController = (inputEvent.deviceType == eIDT_Gamepad);

	if (isGamePadController)
	{
		// Store device index of controller providing input
		bool isConnectionChangeEvent = ((inputEvent.keyId == eKI_SYS_ConnectDevice) || (inputEvent.keyId == eKI_SYS_DisconnectDevice) ||
		                                (inputEvent.keyId == eKI_XI_Connect) || (inputEvent.keyId == eKI_XI_Disconnect));
		if (isConnectionChangeEvent == false) // Only want to set device index when real input comes through, not device changes
		{
			m_previousInputControllerDeviceIndex = inputEvent.deviceIndex;
		}

		unsigned int myDeviceIndex = GetExclusiveControllerDeviceIndex();

		// If there is an exclusive controller, then ignore other controllers
		if (m_hasExclusiveController && (inputEvent.deviceIndex != myDeviceIndex))
		{
			return true; // Return true so that other listeners won't recieve this event
		}

#if DISABLE_FORCE_FEEDBACK_WHEN_USING_MOUSE_AND_KEYBOARD
		// On controller event re-enable force feedback for controller
		const unsigned int forceFeedbackDeviceIndex = (gEnv->IsEditor()) ? inputEvent.deviceIndex : myDeviceIndex;
		gEnv->pInput->ForceFeedbackSetDeviceIndex(forceFeedbackDeviceIndex);
#endif

		if (m_hasExclusiveController)
		{
			// Add any game specific handling of controllers connecting/disconnecting here:-
			switch (inputEvent.keyId)
			{
			case eKI_SYS_ConnectDevice:
				{
					// Controller connected
					m_bExclusiveControllerConnected = true;
					break;
				}
			case eKI_SYS_DisconnectDevice:
				{
					// Controller disconnected
					m_bExclusiveControllerConnected = false;
					break;
				}
			}
		}
	}
#if DISABLE_FORCE_FEEDBACK_WHEN_USING_MOUSE_AND_KEYBOARD
	else if (inputEvent.deviceType == eIDT_Keyboard || inputEvent.deviceType == eIDT_Mouse)
	{
		// On keyboard/mouse event, disable Force Feedback for controller
		gEnv->pInput->ForceFeedbackSetDeviceIndex(EFF_INVALID_DEVICE_INDEX);
	}
#endif

	bool result = false;
	if (m_pInputEventListenerOverride)
	{
		result = m_pInputEventListenerOverride->OnInputEvent(inputEvent);
	}

	return result; // Return false for the other listeners to get this event
}

bool CGame::OnInputEventUI(const SUnicodeEvent& inputEvent)
{
	bool result = false;
	if (m_pInputEventListenerOverride)
	{
		result = m_pInputEventListenerOverride->OnInputEventUI(inputEvent);
	}

	return result; // Return false for the other listeners to get this event
}

void CGame::AddRenderSceneListener(IRenderSceneListener* pListener)
{
	stl::push_back_unique(m_renderSceneListeners, pListener);
}

void CGame::RemoveRenderSceneListener(IRenderSceneListener* pListener)
{
	stl::find_and_erase(m_renderSceneListeners, pListener);
}

void CGame::SetExclusiveControllerFromPreviousInput()
{
	CRY_ASSERT_MESSAGE(m_pPlayerProfileManager, "No player profile manager, controller index will not be set, this will cause problems");

	if (m_hasExclusiveController)
	{
		CryLog("CGame::SetExclusiveControllerFromPreviousInput() already have exclusive controller, ignoring");
		return;
	}

	const bool bChangeUser = m_pPlayerProfileManager ? (m_pPlayerProfileManager->GetExclusiveControllerDeviceIndex() != m_previousInputControllerDeviceIndex) : false;

	if (bChangeUser)
	{
		LogoutCurrentUser(eLR_SetExclusiveController);
	}

	m_hasExclusiveController = true;
	m_bExclusiveControllerConnected = true;

	if (bChangeUser)
	{
		LoginUser(m_previousInputControllerDeviceIndex);

		m_pPlayerProfileManager->SetExclusiveControllerDeviceIndex(m_previousInputControllerDeviceIndex);
		gEnv->pInput->ForceFeedbackSetDeviceIndex(m_previousInputControllerDeviceIndex);

		GetISystem()->GetPlatformOS()->UserSelectStorageDevice(m_previousInputControllerDeviceIndex);
	}
}

unsigned int CGame::GetExclusiveControllerDeviceIndex() const
{
	return m_pPlayerProfileManager ? m_pPlayerProfileManager->GetExclusiveControllerDeviceIndex() : 0;
}

void CGame::RemoveExclusiveController()
{
	CryLog("CGame::RemoveExclusiveController");

	bool hasExclusiveController = m_hasExclusiveController;
	m_hasExclusiveController = false;
	m_bExclusiveControllerConnected = false;

	m_pPlayerProfileManager->SetExclusiveControllerDeviceIndex(INVALID_CONTROLLER_INDEX);
	gEnv->pInput->ForceFeedbackSetDeviceIndex(EFF_INVALID_DEVICE_INDEX);

	// Disable save icon, it may have been playing and can get stuck on
	{
		m_saveIconMode = eSIM_Off;
		m_saveIconTimer = 0.0f;

		SHUDEvent hudEvent;
		hudEvent.eventType = eHUDEvent_OnFileIO;
		hudEvent.AddData(false);
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

bool CGame::SetControllerLayouts(const char* szButtonLayoutName, const char* szStickLayoutName, bool bUpdateProfileData)
{
	if (szButtonLayoutName == NULL || strcmp(szButtonLayoutName, "") == 0)
		return false;

	if (szStickLayoutName == NULL || strcmp(szStickLayoutName, "") == 0)
		return false;

	IActionMapManager* pActionMapManager = GetIGameFramework()->GetIActionMapManager();
	CRY_ASSERT(pActionMapManager != NULL);

	CryFixedStringT<32> layoutKeyName;
	bool bResult;
	layoutKeyName.Format("buttonlayout_%s", szButtonLayoutName);

	bResult = pActionMapManager->LoadControllerLayoutFile(layoutKeyName);
	if (!bResult)
	{
		GameWarning("CGame::SetControllerLayouts: Failed to load controller layout: %s", layoutKeyName.c_str());
		return false;
	}

	layoutKeyName.Format("sticklayout_%s", szStickLayoutName);

	bResult = pActionMapManager->LoadControllerLayoutFile(layoutKeyName);
	if (!bResult)
	{
		GameWarning("CGame::SetControllerLayout: Failed to load controller layout file: %s", layoutKeyName.c_str());
		return false;
	}

	if (bUpdateProfileData && GetProfileOptions())
	{
		GetProfileOptions()->SetOptionValue("ControllerButtonLayout", szButtonLayoutName);
		GetProfileOptions()->SetOptionValue("ControllerStickLayout", szStickLayoutName);
	}

	return true;
}

const char* CGame::GetControllerLayout(const EControllerLayout layoutType) const
{
	CProfileOptions* profileOptions = GetProfileOptions();

	if (!profileOptions)
		return NULL;

	if (layoutType == eControllerLayout_Button)
	{
		return profileOptions->GetOptionValue("ControllerButtonLayout");
	}
	else // Must be stick
	{
		return profileOptions->GetOptionValue("ControllerStickLayout");
	}
}

void CGame::OnActionEvent(const SActionEvent& event)
{
	switch (event.m_event)
	{
	case eAE_connectFailed:
		CCCPOINT(Net_ConnectFailed);
		break;
	case eAE_channelDestroyed:
		CCCPOINT(Net_ChannelDestroyed);
		m_pBurnEffectManager->Reset();
		GameChannelDestroyed(event.m_value == 1);
		break;
	case eAE_serverIp:
		CCCPOINT(Net_GetServerIp);
		break;
	case eAE_serverName:
		CCCPOINT(Net_GetServerName);
		break;
	case eAE_earlyPreUpdate:
		break;
	case eAE_disconnected:
		break;
	case eAE_unloadLevel:
		{
			MEMSTAT_LABEL_SCOPED("CGame::OnActionEvent(eAE_unloadLevel)");
			m_pGameCache->Reset();

			m_pGameParametersStorage->GetItemResourceCache().FlushCaches();
			m_pGameParametersStorage->ReleaseLevelResources();
			m_pPlayerVisTable->Reset();

			m_pBurnEffectManager->Reset();
			if (m_pBodyDamageManager)
				m_pBodyDamageManager->FlushLevelResourcesCache();

			g_tacticalPointLanguageExtender.Reset();
			m_pScreenEffects->Reset();
			CStealthKill::CleanUp();
			CSpectacularKill::CleanUp();
			m_pMovementTransitionsSystem->Flush();
			CSmokeManager::GetSmokeManager()->ReleaseObstructionObjects();

			if (m_pGameAISystem)
			{
				m_pGameAISystem->Reset(false); // Going to lie about the unload here, and reset it for unload later...
			}
			m_pGameAudio->Reset();
			if (GetMOSystem())
			{
				GetMOSystem()->Reset();
			}
			if (!gEnv->IsEditor())
			{
				g_tacticalPointLanguageExtender.Deinitialize();
			}
			if (m_renderSceneListeners.empty())
				stl::free_container(m_renderSceneListeners);
			if (m_pDownloadMgr)
				m_pDownloadMgr->Reset();
			if (m_pRecordingSystem)
				m_pRecordingSystem->Reset();

			m_pLedgeManager->Reset();

			m_pLightningGameEffect->ClearSparks();

			m_pWaterPuddleManager->Reset();

			if (m_pMovingPlatformMgr)
				m_pMovingPlatformMgr->Reset();
		}
		break;
	case eAE_postUnloadLevel:
		{
			MEMSTAT_LABEL_SCOPED("CGame::OnActionEvent(eAE_postUnloadLevel)");
			if (m_pGameAISystem)
			{
				m_pGameAISystem->Reset(true);
			}
			CSchedulerActionPoolBase::ResetAll();
			CWeapon::StaticReset();
			if (m_statsRecorder)
			{
				m_statsRecorder->FlushData();
			}
		}
		break;
	case eAE_loadLevel:
		{
			MEMSTAT_LABEL_SCOPED("CGame::OnActionEvent(eAE_loadLevel)");
			if (m_pGameAISystem)
			{
				m_pGameAISystem->Reset(false);
				if (!gEnv->IsEditor())
				{
					g_tacticalPointLanguageExtender.Initialize();
				}
			}

			//Because of the code just right bellow (case eAE_inGame) we need to make sure to pre-cache the corresponding
			//resources for items tracked on persistant stats during level load
			for (int i = 0; i < s_numUnlockableAttachments; ++i)
			{
				const int hasAttachment = m_pPersistantStats->GetStat(s_unlockableAttachmentNames[i], EMPS_AttachmentUnlocked);

#if !defined(RELEASE)
				bool bSavingResourceList = gEnv->pSystem->IsSavingResourceList();
#else
				bool bSavingResourceList = false;
#endif

				if (hasAttachment || bSavingResourceList)
				{
					CItemSharedParams* pItemParams = m_pGameParametersStorage->GetItemSharedParameters(s_unlockableAttachmentNames[i], false);
					IEntityClass* pItemClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(s_unlockableAttachmentNames[i]);
					if (pItemParams && pItemClass)
					{
						pItemParams->CacheResources(m_pGameParametersStorage->GetItemResourceCache(), pItemClass);
					}
				}
			}
		}
		break;
	case eAE_mapCmdIssued:
		if (!m_gameTypeInitialized || (g_pGameCVars->autotest_enabled && !!g_pGameCVars->g_multiplayerDefault != m_gameTypeMultiplayer))
		{
			InitGameType(g_pGameCVars->g_multiplayerDefault != 0, !m_gameTypeInitialized);
		}
		if (gEnv->bMultiplayer)
		{
			CGameLobby* pGameLobby = g_pGame->GetGameLobby();
			pGameLobby->OnMapCommandIssued();
		}
		break;
	case eAE_inGame:
		AddPersistentAccessories();
		break;
	case eAE_preSaveGame:
		AddPersistentAccessories();
		break;
	case eAE_disconnectCommandFinished:
#if CRY_PLATFORM_DURANGO
		if (g_pGame->GetGameLobby() && !g_pGame->GetGameLobby()->IsCurrentlyInSession())
		{
			EnsureSigninState();
		}
#endif
		break;
	}
}

void CGame::GameChannelDestroyed(bool isServer)
{
	if (!isServer)
	{
		if (!gEnv->pSystem->IsSerializingFile())
		{
			char buf[64];
			cry_sprintf(buf, "%g", g_pGameCVars->v_altitudeLimitDefault());
			g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf);
		}
	}
}

CGameRules* CGame::GetGameRules() const
{
	return static_cast<CGameRules*>(gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules());
}

bool CGame::IsLevelLoaded() const
{
	CGameRules* pGameRules = GetGameRules();
	return pGameRules ? pGameRules->IsLevelLoaded() : false;
}

#ifdef USE_LAPTOPUTIL
CLaptopUtil* CGame::GetLaptopUtil() const
{
	return m_pLaptopUtil;
}
#endif

CProfileOptions* CGame::GetProfileOptions() const
{
	return m_pUIManager ? m_pUIManager->GetOptions() : NULL;
}

CWarningsManager* CGame::GetWarnings() const
{
	return m_pUIManager ? m_pUIManager->GetWarningManager() : NULL;
}

CHUDMissionObjectiveSystem* CGame::GetMOSystem() const
{
	return m_pUIManager ? m_pUIManager->GetMOSystem() : NULL;
}

void CGame::LoadActionMaps(const char* filename)
{
	IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

	if (pActionMapManager)
	{
		pActionMapManager->RegisterActionMapEventListener(m_pGameActions);
		if (!pActionMapManager->InitActionMaps(filename))
		{
			CryFatalError("CGame::LoadActionMaps() Invalid action maps setup");
		}
	}

	const char* disableGamemodeActionMapName = "player_mp";
	const char* gamemodeActionMapName = "player_sp";

	if (gEnv->bMultiplayer)
	{
		disableGamemodeActionMapName = "player_sp";
		gamemodeActionMapName = "player_mp";
	}

	IActionMap* pDefaultActionMap = nullptr;
	IActionMap* pDebugActionMap = nullptr;
	IActionMap* pPlayerActionMap = nullptr;

	IPlayerProfileManager* pPPMgr = gEnv->pGameFramework->GetIPlayerProfileManager();
	if (pPPMgr)
	{
		int userCount = pPPMgr->GetUserCount();

		IPlayerProfile* pProfile = NULL;
		const char* userId = "UNKNOWN";
		if (userCount == 0)
		{
			if (gEnv->pSystem->IsDevMode())
			{
#ifndef _RELEASE
				//In devmode and not release get the default user if no users are signed in e.g. autotesting, map on the command line
				pProfile = pPPMgr->GetDefaultProfile();
				if (pProfile)
				{
					userId = pProfile->GetUserId();
				}
#endif            // #ifndef _RELEASE
			}
			else
			{
				CryFatalError("[PlayerProfiles] CGameContext::StartGame: No users logged in");
				return;
			}
		}

		if (userCount > 0)
		{
			IPlayerProfileManager::SUserInfo info;
			pPPMgr->GetUserInfo(0, info);
			pProfile = pPPMgr->GetCurrentProfile(info.userId);
			userId = info.userId;
		}
		if (pProfile)
		{
			pDefaultActionMap = pProfile->GetActionMap("default");
			pDebugActionMap = pProfile->GetActionMap("debug");
			pPlayerActionMap = pProfile->GetActionMap("player");

			if (pDefaultActionMap == 0 && pPlayerActionMap == 0)
			{
				CryFatalError("[PlayerProfiles] CGameContext::StartGame: User '%s' has no actionmap 'default'!", userId);
				return;
			}
		}
		else
		{
			CryFatalError("[PlayerProfiles] CGameContext::StartGame: User '%s' has no active profile!", userId);
			return;
		}
	}
	else
	{
		CryFatalError("[PlayerProfiles] CGameContext::StartGame: No player profile manager!");
		return;
	}

	if (pDefaultActionMap == nullptr)
	{
		// use action map without any profile stuff
		pDefaultActionMap = pActionMapManager->GetActionMap("default");
		CRY_ASSERT_MESSAGE(pDefaultActionMap, "'default' action map not found!");
	}

	if (pDebugActionMap == nullptr)
	{
		// use action map without any profile stuff
		pDebugActionMap = pActionMapManager->GetActionMap("debug");
	}

	if (pPlayerActionMap == nullptr)
	{
		pPlayerActionMap = pActionMapManager->GetActionMap("player");
	}

	if (!pDefaultActionMap)
	{
		CryFatalError("Failed to load default actionmap!");
		return;
	}

	pActionMapManager->SetDefaultActionEntity(LOCAL_PLAYER_ENTITY_ID);
	pActionMapManager->EnableActionMap(disableGamemodeActionMapName, false);
	pActionMapManager->EnableActionMap(gamemodeActionMapName, true);
	auto* pPlayerGamemodeActionMap = pActionMapManager->GetActionMap(gamemodeActionMapName);

	if (pPlayerGamemodeActionMap)
	{
		pPlayerGamemodeActionMap->SetActionListener(LOCAL_PLAYER_ENTITY_ID);
	}

	pActionMapManager->Enable(true);
}

void CGame::InitScriptBinds()
{
	m_pScriptBindActor = new CScriptBind_Actor(gEnv->pGameFramework->GetISystem());
	m_pScriptBindItem = new CScriptBind_Item(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindWeapon = new CScriptBind_Weapon(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindHUD = new CScriptBind_HUD(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindGameRules = new CScriptBind_GameRules(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindGame = new CScriptBind_Game(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindHitDeathReactions = new CScriptBind_HitDeathReactions(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindInteractiveObject = new CScriptBind_InteractiveObject(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	m_pScriptBindBoids = new CScriptBind_Boids(gEnv->pGameFramework->GetISystem());
	m_pScriptBindTurret = new CScriptBind_Turret(gEnv->pGameFramework->GetISystem());
	m_pScriptBindProtected = new CScriptBind_ProtectedBinds(gEnv->pGameFramework->GetISystem());
	m_pScriptBindLightningArc = new CScriptBind_LightningArc(gEnv->pGameFramework->GetISystem());

	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if (!gEnv->bMultiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		m_pScriptBindGameAI = new CScriptBind_GameAI(gEnv->pGameFramework->GetISystem(), gEnv->pGameFramework);
	}
	else
	{
		m_pScriptBindGameAI = NULL;
	}
}

void CGame::ReleaseScriptBinds()
{
	SAFE_DELETE(m_pScriptBindActor);
	SAFE_DELETE(m_pScriptBindItem);
	SAFE_DELETE(m_pScriptBindWeapon);
	SAFE_DELETE(m_pScriptBindHUD);
	SAFE_DELETE(m_pScriptBindGameRules);
	SAFE_DELETE(m_pScriptBindGame);
	SAFE_DELETE(m_pScriptBindInteractiveObject);
	SAFE_DELETE(m_pScriptBindHitDeathReactions);
	SAFE_DELETE(m_pScriptBindBoids);
	SAFE_DELETE(m_pScriptBindTurret);
	SAFE_DELETE(m_pScriptBindProtected);
	SAFE_DELETE(m_pScriptBindGameAI);
}

void CGame::CheckReloadLevel()
{
	if (!m_bReload)
		return;

	m_bReload = false;

	if (gEnv->IsEditor() || gEnv->bMultiplayer)
	{
		return;
	}

	CryFixedStringT<256> command;
	command.Format("map %s nb", gEnv->pGameFramework->GetLevelName());
	gEnv->pConsole->ExecuteString(command.c_str());
}

void CGame::RegisterGameObjectEvents()
{
	IGameObjectSystem* pGOS = gEnv->pGameFramework->GetIGameObjectSystem();

	pGOS->RegisterEvent(eCGE_OnShoot, "OnShoot");
	pGOS->RegisterEvent(eCGE_ActorRevive, "ActorRevive");
	pGOS->RegisterEvent(eCGE_VehicleDestroyed, "VehicleDestroyed");
	pGOS->RegisterEvent(eCGE_VehicleTransitionEnter, "VehicleTransitionEnter");
	pGOS->RegisterEvent(eCGE_VehicleTransitionExit, "VehicleTransitionExit");
	pGOS->RegisterEvent(eCGE_HUD_PDAMessage, "HUD_PDAMessage");
	pGOS->RegisterEvent(eCGE_HUD_TextMessage, "HUD_TextMessage");
	pGOS->RegisterEvent(eCGE_TextArea, "TextArea");
	pGOS->RegisterEvent(eCGE_HUD_Break, "HUD_Break");
	pGOS->RegisterEvent(eCGE_HUD_Reboot, "HUD_Reboot");
	pGOS->RegisterEvent(eCGE_InitiateAutoDestruction, "InitiateAutoDestruction");
	pGOS->RegisterEvent(eCGE_Event_Collapsing, "Event_Collapsing");
	pGOS->RegisterEvent(eCGE_Event_Collapsed, "Event_Collapsed");
	pGOS->RegisterEvent(eCGE_MultiplayerChatMessage, "MultiplayerChatMessage");
	pGOS->RegisterEvent(eCGE_ResetMovementController, "ResetMovementController");
	pGOS->RegisterEvent(eCGE_AnimateHands, "AnimateHands");
	pGOS->RegisterEvent(eCGE_EnablePhysicalCollider, "EnablePhysicalCollider");
	pGOS->RegisterEvent(eCGE_DisablePhysicalCollider, "DisablePhysicalCollider");
	pGOS->RegisterEvent(eCGE_SetTeam, "SetTeam");
	pGOS->RegisterEvent(eCGE_Launch, "Launch");
	// [*DavidR | 1/Sep/2009] CHECK: Can we put this on some HitDeathReaction
	// initialization code?
	pGOS->RegisterEvent(eCGE_ReactionEnd, "ReactionEnd");
	pGOS->RegisterEvent(eCGE_CoverTransitionEnter, "CoverTransitionEnter");
	pGOS->RegisterEvent(eCGE_CoverTransitionExit, "CoverTransitionExit");
	pGOS->RegisterEvent(eCGE_AllowStartTransitionEnter, "AllowStartTransitionEnter");
	pGOS->RegisterEvent(eCGE_AllowStartTransitionExit, "AllowStartTransitionExit");
	pGOS->RegisterEvent(eCGE_AllowStopTransitionEnter, "AllowStopTransitionEnter");
	pGOS->RegisterEvent(eCGE_AllowStopTransitionExit, "AllowStopTransitionExit");
	pGOS->RegisterEvent(eCGE_AllowDirectionChangeTransitionEnter, "AllowDirectionChangeTransitionEnter");
	pGOS->RegisterEvent(eCGE_AllowDirectionChangeTransitionExit, "AllowDirectionChangeTransitionExit");
	pGOS->RegisterEvent(eCGE_Ragdollize, "Ragdollize");
	pGOS->RegisterEvent(eCGE_ItemTakenFromCorpse, "ItemTakeFromCorpse");
}

void CGame::GetMemoryStatistics(ICrySizer* s)
{
	s->AddObject(m_pGameAudio);
	s->AddObject(m_pUIManager);
	s->AddObject(m_telemetryCollector);

	m_pWeaponSystem->GetMemoryStatistics(s);
	m_pScreenEffects->GetMemoryStatistics(s);

	s->Add(*m_pScriptBindActor);
	s->Add(*m_pScriptBindItem);
	s->Add(*m_pScriptBindWeapon);
	s->Add(*m_pScriptBindGameRules);
	s->Add(*m_pScriptBindGame);
	s->Add(*m_pScriptBindHUD);
	s->Add(*m_pScriptBindInteractiveObject);
	s->Add(*m_pScriptBindHitDeathReactions);
	s->Add(*m_pScriptBindBoids);
	s->Add(*m_pScriptBindTurret);
	s->Add(*m_pGameActions);

	m_pGameParametersStorage->GetMemoryStatistics(s);

	if (m_pPlayerProfileManager)
		m_pPlayerProfileManager->GetMemoryStatistics(s);

	if (m_pHitDeathReactionsSystem)
		m_pHitDeathReactionsSystem->GetMemoryUsage(s);

	if (m_pBodyDamageManager)
		m_pBodyDamageManager->GetMemoryUsage(s);

	if (m_pMovementTransitionsSystem)
		m_pMovementTransitionsSystem->GetMemoryUsage(s);

	m_pGameCache->GetMemoryUsage(s);
}

void CGame::DumpMemInfo(const char* format, ...)
{
	CryModuleMemoryInfo memInfo;
	CryGetMemoryInfoForModule(&memInfo);

	va_list args;
	va_start(args, format);
	gEnv->pLog->LogV(ILog::eAlways, format, args);
	va_end(args);

	gEnv->pLog->LogWithType(ILog::eAlways, "Alloc=%" PRIu64 " kb  String=%" PRIu64 " kb  STL-alloc=%" PRIu64 " kb  STL-wasted=%" PRIu64 " kb", (memInfo.allocated - memInfo.freed) >> 10, memInfo.CryString_allocated >> 10, memInfo.STL_allocated >> 10, memInfo.STL_wasted >> 10);
}

const string& CGame::GetLastSaveGame(string& levelName)
{
	if (m_pPlayerProfileManager && (m_bLastSaveDirty || m_lastSaveGame.empty()))
	{
		IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
		if (pProfile)
		{
			ISaveGameEnumeratorPtr pSGE = pProfile->CreateSaveGameEnumerator();
			ISaveGameEnumerator::SGameDescription desc;
			time_t curLatestTime = (time_t) 0;
			const char* lastSaveGame = "";
			const int nSaveGames = pSGE->GetCount();

			for (int i = 0; i < nSaveGames; ++i)
			{
				if (pSGE->GetDescription(i, desc))
				{
					if (desc.metaData.loadTime >= curLatestTime)
					{
						lastSaveGame = desc.name;
						curLatestTime = desc.metaData.loadTime;
						levelName = desc.metaData.levelName;
					}
				}
			}
			m_lastSaveGame = lastSaveGame;
		}

		m_bLastSaveDirty = false;
	}
	return m_lastSaveGame;
}

/*static */ void CGame::ExpandTimeSeconds(int secs, int& days, int& hours, int& minutes, int& seconds)
{
	days = secs / 86400;
	secs -= days * 86400;
	hours = secs / 3600;
	secs -= hours * 3600;
	minutes = secs / 60;
	seconds = secs - minutes * 60;
}

const char* CGame::GetMappedLevelName(const char* levelName) const
{
	TLevelMapMap::const_iterator iter = m_mapNames.find(CONST_TEMP_STRING(levelName));
	return (iter == m_mapNames.end()) ? levelName : iter->second.c_str();
}

void CGame::LoadMappedLevelNames(const char* xmlPath)
{
	//load user levelnames for ingame text and savegames
	XmlNodeRef lnames = GetISystem()->LoadXmlFromFile(xmlPath);
	if (lnames)
	{
		int num = lnames->getNumAttributes();
		const char* nameA, * nameB;
		for (int n = 0; n < num; ++n)
		{
			lnames->getAttributeByIndex(n, &nameA, &nameB);
			m_mapNames[string(nameA)] = string(nameB);
		}
	}
}

IGameStateRecorder* CGame::CreateGameStateRecorder(IGameplayListener* pL)
{
	if (!m_pGameStateRecorder)
	{
		m_pGameStateRecorder = new CGameStateRecorder();
		CRY_ASSERT(m_pGameStateRecorder);
		m_pGameStateRecorder->RegisterListener(pL);
	}
	return m_pGameStateRecorder;
}

CInteractiveObjectRegistry& CGame::GetInteractiveObjectsRegistry() const
{
	return m_pScriptBindInteractiveObject->GetObjectDataRegistry();
}

void CGame::ClearSessionTelemetry(void)
{
#if USE_TELEMETRY_BUFFERS
	m_secondTimePerformance = CTimeValue();
	m_secondTimeMemory = CTimeValue();
	m_secondTimeBandwidth = CTimeValue();
	m_secondTimeSound = CTimeValue();
	if (m_performanceBuffer)
	{
		m_performanceBuffer->Reset();
	}
	if (m_bandwidthBuffer)
	{
		m_bandwidthBuffer->Reset();
	}
	if (m_memoryTrackingBuffer)
	{
		m_memoryTrackingBuffer->Reset();
	}
	if (m_soundTrackingBuffer)
	{
		m_soundTrackingBuffer->Reset();
	}
#endif //#if USE_TELEMETRY_BUFFERS
}

//---------------------------------------
class BufferUtil
{
public:
	BufferUtil(int size)
		: m_pos(0)
		, m_size(size)
		, m_bufferOverflow(false)
	{
		m_pBuffer = new char[size];
	}

	~BufferUtil()
	{
		delete[] m_pBuffer;
	}

	template<class T>
	void Write(T& data)
	{
		SwapEndian(data, eBigEndian);   //swap to Big Endian

		if (m_pos + (int)sizeof(T) <= m_size)
		{
			memcpy(m_pBuffer + m_pos, &data, sizeof(T));
			m_pos += sizeof(T);
		}
		else
		{
			m_bufferOverflow = true;
			CRY_ASSERT_MESSAGE(false, "Buffer size is not large enough");
		}

		SwapEndian(data, eBigEndian); //swap back again
	}

	void WriteString(const char* string, int length)
	{
		// Write the length of the string followed by the string itself
		Write(length);
		if (m_pos + length <= m_size)
		{
			memcpy(m_pBuffer + m_pos, string, length);
			m_pos += length;
		}
		else
		{
			m_bufferOverflow = true;
			CRY_ASSERT_MESSAGE(false, "Buffer size is not large enough");
		}
	}

	template<class T, size_t S>
	void WriteString(CryStackStringT<T, S>& string)
	{
		WriteString(string.c_str(), string.length());
	}

	const char* GetBuffer()   { return m_pBuffer; }
	int         GetUsedSize() { return m_pos; }
	bool        Overflow()    { return m_bufferOverflow; }

private:
	char* m_pBuffer;
	int   m_pos;
	int   m_size;
	bool  m_bufferOverflow;
};

//---------------------------------------
void CGame::UploadSessionTelemetry(void)
{
	if (m_telemetryCollector)
	{
		stack_string levelName;

		if (GetGameLobby() && gEnv->bMultiplayer)
		{
			levelName = GetGameLobby()->GetCurrentLevelName();
		}
		else if (gEnv->pGameFramework)
		{
			levelName = gEnv->pGameFramework->GetLevelName();
		}

		if (levelName.empty())
		{
			if (ILevelInfo* pLevelInfo = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel())
			{
				levelName = pLevelInfo->GetName();
			}
		}

		// strip the path off the beginning of the map for consistency with existing playtime.xml
		int pathOffset = levelName.rfind('/');
		if (pathOffset != -1)
		{
			levelName = levelName.Right(levelName.length() - pathOffset - 1);
		}

		if (levelName.empty())
		{
			levelName = "unknown";
		}

		stack_string gameMode = "unknown";
		stack_string playerNames = "unknown";
		stack_string buildType = "unknown";
		int time = 0;
		CGameRules* pGameRules = GetGameRules();
		if (pGameRules)
		{
			gameMode = pGameRules->GetEntity()->GetClass()->GetName();
			gameMode.MakeLower();
			time = int(pGameRules->GetCurrentGameTime() * 1000);
			const CGameRules::TCryUserIdSet& users = pGameRules->GetParticipatingUsers();
			if (gEnv->bServer)
			{
				BufferUtil buffer(1024);
				int VERSION = 2;

				buffer.Write(VERSION);
				// Version 1 data
				buffer.WriteString(levelName);
				buffer.WriteString(gameMode);
				buffer.Write(time);

				int numUsers = users.size();
				buffer.Write(numUsers);
				CGameRules::TCryUserIdSet::const_iterator itUser;
				for (itUser = users.begin(); itUser != users.end(); ++itUser)
				{
					CRY_ASSERT_MESSAGE(itUser->IsValid(), "CryUserId is not valid");
					CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> str = itUser->get()->GetGUIDAsString();
					buffer.WriteString(str);
				}
				// Version 2 additions
#if defined(PERFORMANCE_BUILD)
				buildType = "PERFORMANCE";
#elif defined(_RELEASE)
				buildType = "RELEASE";
#else
				buildType = "PROFILE";
#endif
				buffer.WriteString(buildType);

				if (!buffer.Overflow())
				{
					m_telemetryCollector->SubmitFromMemory("session_summary.bin", buffer.GetBuffer(), buffer.GetUsedSize(), CTelemetryCollector::k_tf_none);
				}
			}
		}

#if USE_TELEMETRY_BUFFERS
		if (pGameRules)
		{
			CGameRules::TPlayers players;
			pGameRules->GetPlayersClient(players);
			playerNames.clear();
			bool bFirst = true;
			for (CGameRules::TPlayers::const_iterator itPlayer = players.begin(); itPlayer != players.end(); ++itPlayer)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*itPlayer);
				if (pEntity)
				{
					if (!bFirst)
					{
						playerNames.append(",");
					}
					playerNames.append(pEntity->GetName());
					bFirst = false;
				}
			}
		}

		string str;
		str.Format("<sheet><row map=\"%s\" gamemode=\"%s\" time=\"%d\" playerNames=\"%s\" /></sheet>",
		           levelName.c_str(), gameMode.c_str(), time, playerNames.c_str());

		m_telemetryCollector->SubmitFromMemory("playtime.xml", str.c_str(), str.length(), CTelemetryCollector::k_tf_none);

		CryFixedStringT<255> localFileName;

		if (m_performanceBuffer)
		{
			m_performanceBuffer->SubmitToServer("frametimes.log");
			localFileName.Format("%%USER%%/MiscTelemetry/%s_frametimes.log", m_telemetryCollector->GetSessionId().c_str());
			m_performanceBuffer->DumpToFile(localFileName.c_str());
			m_performanceBuffer->Reset();
		}
		if (m_bandwidthBuffer)
		{
			m_bandwidthBuffer->SubmitToServer("bandwidth.log");
			localFileName.Format("%%USER%%/MiscTelemetry/%s_bandwidth.log", m_telemetryCollector->GetSessionId().c_str());
			m_bandwidthBuffer->DumpToFile(localFileName.c_str());
			m_bandwidthBuffer->Reset();
		}
		if (m_memoryTrackingBuffer)
		{
			m_memoryTrackingBuffer->SubmitToServer("memory.log");
			localFileName.Format("%%USER%%/MiscTelemetry/%s_memory.log", m_telemetryCollector->GetSessionId().c_str());
			m_memoryTrackingBuffer->DumpToFile(localFileName.c_str());
			m_memoryTrackingBuffer->Reset();
		}
		if (m_soundTrackingBuffer)
		{
			m_soundTrackingBuffer->SubmitToServer("sound.log");
			localFileName.Format("%%USER%%/MiscTelemetry/%s_sound.log", m_telemetryCollector->GetSessionId().c_str());
			m_soundTrackingBuffer->DumpToFile(localFileName.c_str());
			m_soundTrackingBuffer->Reset();
		}

		//Make summarystats.xml - one row for all the summary stats we output
		string row;
		str.clear();

		row.Format("<sheet>");
		str += row;
		row.Format("<row level=\"%s\"", levelName.c_str());
		str += row;

		if (gEnv->pRenderer)
		{
			//shader misses: global
			static int s_lastNumGlobalMisses = 0;

			SShaderCacheStatistics stats;
			gEnv->pRenderer->EF_Query(EFQ_GetShaderCacheInfo, stats);

			int globalMisses = stats.m_nGlobalShaderCacheMisses - s_lastNumGlobalMisses;

			row.Format(" numGlobalMisses=\"%d\"", globalMisses);
			str += row;

			s_lastNumGlobalMisses = stats.m_nGlobalShaderCacheMisses;
		}

		//maximum number of bound network objects
		INetwork* pNetwork = gEnv->pNetwork;
		SNetworkProfilingStats profileStats;
		pNetwork->GetProfilingStatistics(&profileStats);
		const int maxNumNetBoundObjects = profileStats.m_maxBoundObjects;
		row.Format(" maxNumNetBoundObjects=\"%d\"", maxNumNetBoundObjects);
		str += row;

		row.Format(" />");
		str += row;
		row.Format("</sheet>");
		str += row;

		m_telemetryCollector->SubmitFromMemory("summarystats.xml", str.c_str(), str.length(), CTelemetryCollector::k_tf_none);
#endif // #if USE_TELEMETRY_BUFFERS
	}
}

void CGame::OnLevelEnd(const char* nextLevel)
{
	m_pBurnEffectManager->Reset();
}

//------------------------------------------------------------------------
float CGame::GetTimeSinceHostMigrationStateChanged() const
{
	const float curTime = gEnv->pTimer->GetAsyncCurTime();
	const float timePassed = curTime - m_hostMigrationTimeStateChanged;
	return timePassed;
}

//------------------------------------------------------------------------
float CGame::GetRemainingHostMigrationTimeoutTime() const
{
	const float timePassed = GetTimeSinceHostMigrationStateChanged();
	const float timeRemaining = m_hostMigrationNetTimeoutLength - timePassed;
	return std::max(timeRemaining, 0.f);
}

//------------------------------------------------------------------------
float CGame::GetHostMigrationTimeTillResume() const
{
	float timeRemaining = 0.f;
	if (m_hostMigrationState == eHMS_WaitingForPlayers)
	{
		timeRemaining = GetRemainingHostMigrationTimeoutTime() + g_pGameCVars->g_hostMigrationResumeTime;
	}
	else if (m_hostMigrationState == eHMS_Resuming)
	{
		const float curTime = gEnv->pTimer->GetAsyncCurTime();
		const float timePassed = curTime - m_hostMigrationTimeStateChanged;
		timeRemaining = std::max(g_pGameCVars->g_hostMigrationResumeTime - timePassed, 0.f);
	}
	return timeRemaining;
}

//------------------------------------------------------------------------
void CGame::SetHostMigrationState(EHostMigrationState newState)
{
	float timeOfChange = gEnv->pTimer->GetAsyncCurTime();
	SetHostMigrationStateAndTime(newState, timeOfChange);
}

//------------------------------------------------------------------------
void CGame::SetHostMigrationStateAndTime(EHostMigrationState newState, float timeOfChange)
{
	CryLog("CGame::SetHostMigrationState() state changing to '%i' (from '%i')", int(newState), int(m_hostMigrationState));

	if ((m_hostMigrationState == eHMS_NotMigrating) && (newState != eHMS_NotMigrating))
	{
		gEnv->pGameFramework->PauseGame(true, false);
		g_pGameActions->FilterHostMigration()->Enable(true);

		ICVar* pTimeoutCVar = gEnv->pConsole->GetCVar("net_migrate_timeout");
		m_hostMigrationNetTimeoutLength = pTimeoutCVar->GetFVal();
		pTimeoutCVar->SetOnChangeCallback(OnHostMigrationNetTimeoutChanged);
	}

	m_hostMigrationState = newState;
	m_hostMigrationTimeStateChanged = timeOfChange;

	if (newState == eHMS_WaitingForPlayers)
	{
		SHUDEvent showHostMigration;
		showHostMigration.eventType = eHUDEvent_ShowHostMigrationScreen;
		CHUDEventDispatcher::CallEvent(showHostMigration);
	}
	else if (newState == eHMS_Resuming)
	{
		SHUDEvent hideHostMigration(eHUDEvent_HideHostMigrationScreen);
		CHUDEventDispatcher::CallEvent(hideHostMigration);
	}
	else if (newState == eHMS_NotMigrating)
	{
		AbortHostMigration();
	}

	// Notify the gamerules
	CGameRules* pGameRules = GetGameRules();
	pGameRules->OnHostMigrationStateChanged();
}

//------------------------------------------------------------------------
void CGame::AbortHostMigration()
{
	gEnv->pGameFramework->PauseGame(false, false);
	m_hostMigrationState = eHMS_NotMigrating;
	m_hostMigrationTimeStateChanged = 0.f;
	ICVar* pTimeoutCVar = gEnv->pConsole->GetCVar("net_migrate_timeout");
	pTimeoutCVar->SetOnChangeCallback(NULL);
	g_pGameActions->FilterHostMigration()->Enable(false);
}

//------------------------------------------------------------------------
void CGame::OnHostMigrationNetTimeoutChanged(ICVar* pVar)
{
	g_pGame->m_hostMigrationNetTimeoutLength = pVar->GetFVal();
}

void CGame::LogoutCurrentUser(ELogoutReason reason)
{
#if CRY_PLATFORM_DURANGO
	// Everything falls to pieces due to startup flow requirements in FlashFrontEnd if we don't allow the language select screen in autotests
	//ICVar* pAutoTest = gEnv->pConsole->GetCVar("autotest_enabled");
	//if(pAutoTest && pAutoTest->GetIVal())
	//	return;

	string profileName = m_pPlayerProfileManager->GetCurrentUser();

	IPlatformOS::TUserName userName;

	// For 360, we need to reactivate the profile in case the storage has changed,
	// or the user wants to play with a different save game on another storage device.
	// Ideally we should be able to call this for all platforms,
	// however it causes PC to fail since it assumes always signed in status.
	if (!GetISystem()->GetPlatformOS()->UserIsSignedIn(IPlatformOS::Unknown_User)) // there is an active user
	{
		m_pPlayerProfileManager->LogoutUser(profileName);
	}

	/*
	   if (reason == eLR_SetExclusiveController)
	   {
	   // Sign out this user from IPlatformOS to relinquish save game memory etc.
	   unsigned int maxUsers = GetISystem()->GetPlatformOS()->UserGetMaximumSignedInUsers();
	   for(unsigned int user = 0; user < maxUsers; ++user)
	    if(GetISystem()->GetPlatformOS()->UserIsSignedIn(user))
	      if(GetISystem()->GetPlatformOS()->UserGetName(user, userName))
	      {
	        if(strcmp(userName.c_str(), profileName.c_str()) == 0)
	        {
	          GetISystem()->GetPlatformOS()->UserSignOut(user);
	        }
	      }

	   // Ensure default user is signed out if the controller index isn't actually signed in
	   if(GetISystem()->GetPlatformOS()->UserIsSignedIn(IPlatformOS::Unknown_User))
	    GetISystem()->GetPlatformOS()->UserSignOut(IPlatformOS::Unknown_User);
	   }
	 */

	GetProfileOptions()->Init();
#endif // CRY_PLATFORM_DURANGO
}

void CGame::LoginUser(unsigned int user)
{
	bool bIsFirstTime = false;
	const bool bResetProfile = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "ResetProfile") != 0;
	if (m_pPlayerProfileManager)
	{
		IPlatformOS::TUserName tUserName;
		gEnv->pSystem->GetPlatformOS()->UserGetName(user, tUserName);
		const char* userName = tUserName.c_str();

		bool signedIn = false;
		bool ok = m_pPlayerProfileManager->LoginUser(tUserName, bIsFirstTime);
		if (ok)
		{
			bool handled = false;

			// activate the always present profile "default"
			int profileCount = m_pPlayerProfileManager->GetProfileCount(userName);
			if (profileCount > 0)
			{
				if (gEnv->IsDedicated())
				{
					for (int i = 0; i < profileCount; ++i)
					{
						IPlayerProfileManager::SProfileDescription profDesc;
						ok = m_pPlayerProfileManager->GetProfileInfo(userName, i, profDesc);
						if (ok)
						{
							const IPlayerProfile* preview = m_pPlayerProfileManager->PreviewProfile(userName, profDesc.name);
							int iActive = 0;
							if (preview)
							{
								preview->GetAttribute("Activated", iActive);
							}
							if (iActive > 0)
							{
								m_pPlayerProfileManager->ActivateProfile(userName, profDesc.name);
								CryLogAlways("[GameProfiles]: Successfully activated profile '%s' for user '%s'", profDesc.name, userName);
								gEnv->pGameFramework->GetILevelSystem()->LoadRotation();
								handled = true;
								break;
							}
						}
					}
					m_pPlayerProfileManager->PreviewProfile(userName, NULL);
				}

				if (!handled)
				{
					IPlayerProfileManager::SProfileDescription desc;
					ok = m_pPlayerProfileManager->GetProfileInfo(userName, 0, desc);
					if (ok)
					{
						time_t lastLoginTime;
						time(&lastLoginTime);
						m_pPlayerProfileManager->SetProfileLastLoginTime(userName, 0, lastLoginTime);
						IPlayerProfile* pProfile = m_pPlayerProfileManager->ActivateProfile(userName, desc.name);

						if (pProfile == 0)
						{
							GameWarning("[GameProfiles]: Cannot activate profile '%s' for user '%s'. Trying to re-create.", desc.name, userName);
							IPlayerProfileManager::EProfileOperationResult profileResult;
							m_pPlayerProfileManager->CreateProfile(userName, desc.name, true, profileResult); // override if present!
							pProfile = m_pPlayerProfileManager->ActivateProfile(userName, desc.name);
							if (pProfile == 0)
							{
								GameWarning("[GameProfiles]: Cannot activate profile '%s' for user '%s'.", desc.name, userName);
							}
							else
							{
								GameWarning("[GameProfiles]: Successfully re-created profile '%s' for user '%s'.", desc.name, userName);
								signedIn = true;
							}
						}

						if (pProfile)
						{
							if (bResetProfile)
							{
								bIsFirstTime = true;
								pProfile->Reset();
								gEnv->pCryPak->RemoveFile("%USER%/game.cfg");
								CryLogAlways("[GameProfiles]: Successfully reset and activated profile '%s' for user '%s'", desc.name, userName);
							}

							// NOTE: temporary workaround, mark user device for force feedback, should get refactored at some point
							if (gEnv->pInput)
							{
								const IInputDevice* pDevice = gEnv->pInput->GetDevice(user, eIDT_Gamepad);
								if (pDevice)
								{
									m_pPlayerProfileManager->SetExclusiveControllerDeviceIndex(user);
									gEnv->pInput->ForceFeedbackSetDeviceIndex(user);
								}
							}

							CryLogAlways("[GameProfiles]: Successfully activated profile '%s' for user '%s'", desc.name, userName);
							signedIn = true;

							gEnv->pGameFramework->GetILevelSystem()->LoadRotation();
						}
					}
					else
					{
						GameWarning("[GameProfiles]: Cannot get profile info for user '%s'", userName);
					}
				}
			}
			else
			{
				GameWarning("[GameProfiles]: User '%s' has no profiles", userName);
			}

			if (signedIn)
			{
				IPlayerProfileManager* pPlayerProfileManager = gEnv->pGameFramework->GetIPlayerProfileManager();
				if (pPlayerProfileManager)
				{
					const char* currentUser = pPlayerProfileManager->GetCurrentUser();
					IPlayerProfile* pProfile = pPlayerProfileManager->GetCurrentProfile(currentUser);
					if (currentUser && pProfile)
					{
						const char* curUserName = pPlayerProfileManager->GetCurrentProfile(currentUser)->GetUserId();
						const char* profileName = pPlayerProfileManager->GetCurrentProfile(currentUser)->GetName();
						CryLogAlways("username %s signedIn %d userIndex %d", curUserName, signedIn, GetExclusiveControllerDeviceIndex());

						CryLogAlways("ActivateProfile profileName %s", profileName);
						pPlayerProfileManager->ActivateProfile(curUserName, profileName);
					}

					if (bIsFirstTime)
					{
						pProfile->LoadGamerProfileDefaults();

						if (m_pUIManager)
						{
							m_pUIManager->GetOptions()->InitializeFromCVar();
							m_pUIManager->GetOptions()->SaveProfile();
							m_pPlayerProfileManager->ReloadProfile(pProfile, ePR_Options);
						}
					}

					ICVar* pLanguageCVar = gEnv->pConsole->GetCVar("g_language");
					if (pLanguageCVar)
					{
						const char* currentLanguage = pLanguageCVar->GetString();
						//when starting the very first time, set subtitle option for czech and chineset to true, because they don't have localized audio
						if (m_pUIManager && currentLanguage != NULL && currentLanguage[0] != '\0' && (stricmp(currentLanguage, "chineset") == 0 || stricmp(currentLanguage, "czech") == 0))
						{
							int shouldCheck = m_pUIManager->GetOptions()->GetOptionValueAsInt("NonLocalizedAudioSubtitleCheck");
							if (shouldCheck == 0)
							{
								m_pUIManager->GetOptions()->SetOptionValue("NonLocalizedAudioSubtitleCheck", 1);
								m_pUIManager->GetOptions()->SetOptionValue("Subtitles", 1);
								m_pUIManager->GetOptions()->SaveProfile();
							}
						}
					}

					m_bLoggedInFromInvite = (m_inviteAcceptedState == eIAS_WaitForInitProfile) ? true : false;
				}
			}

			//Cache the user region
			int userRegion = -1;
			if (IPlayerProfileManager* pPlayerProfileManager = gEnv->pGameFramework->GetIPlayerProfileManager())
			{
				const char* currentUserName = pPlayerProfileManager->GetCurrentUser();

				IPlatformOS::SUserProfileVariant preference;
				IPlatformOS::TUserName curUserName = currentUserName;
				unsigned int curUser;
				if (gEnv->pSystem->GetPlatformOS()->UserIsSignedIn(curUserName, curUser) && curUser != IPlatformOS::Unknown_User)
				{
					if (gEnv->pSystem->GetPlatformOS()->GetUserProfilePreference(curUser, IPlatformOS::EUPP_REGION, preference))
					{
						userRegion = preference.GetInt();
					}
				}
			}
			SetUserRegion(userRegion);

			// Update controller layouts from profile
			const char* szControllerLayoutButton = GetControllerLayout(eControllerLayout_Button);
			const char* szControllerLayoutStick = GetControllerLayout(eControllerLayout_Stick);
			SetControllerLayouts(szControllerLayoutButton, szControllerLayoutStick, false);
		}
		else
			GameWarning("[GameProfiles]: Cannot login user '%s'", userName);
	}
	else
		GameWarning("[GameProfiles]: PlayerProfileManager not available. Running without.");
}

void CGame::OnPlatformEvent(const IPlatformOS::SPlatformEvent& event)
{
	switch (event.m_eEventType)
	{
	case IPlatformOS::SPlatformEvent::eET_StorageMounted:
		{
			m_bUserHasPhysicalStorage = event.m_uParams.m_storageMounted.m_bPhysicalMedia;
			if (!event.m_uParams.m_storageMounted.m_bOnlyUpdateMediaState)
			{
				LoginUser(event.m_user);
			}
			break;
		}

	case IPlatformOS::SPlatformEvent::eET_SignIn:
		{
			{
				bool bIsExclusiveController = m_hasExclusiveController && GetExclusiveControllerDeviceIndex() == event.m_user;

				if (event.m_uParams.m_signIn.m_signedInState != IPlatformOS::SPlatformEvent::eSIS_NotSignedIn)
				{
				}
				else
				{
					// Handle sign-out
					if (event.m_uParams.m_signIn.m_previousSignedInState != IPlatformOS::SPlatformEvent::eSIS_NotSignedIn)
					{
						if (bIsExclusiveController && GetWarnings())
							GetWarnings()->CancelWarnings();

						assert(m_hasExclusiveController || event.m_user == IPlatformOS::Unknown_User);
						if (bIsExclusiveController || event.m_user == IPlatformOS::Unknown_User)
						{
							CryLog("CGame::OnPlatformEvent signed out, isExclusive=%s, isUnknown=%s, bMultiplayer=%s", bIsExclusiveController ? "true" : "false", (event.m_user == IPlatformOS::Unknown_User) ? "true" : "false", gEnv->bMultiplayer ? "true" : "false");
#if !defined(DEDICATED_SERVER)
							if (bIsExclusiveController)
							{
								CErrorHandling* pErrorHandling = CErrorHandling::GetInstance();
								if (pErrorHandling)
								{
									pErrorHandling->OnFatalError(CErrorHandling::eFE_LocalSignedOut);
								}
								RemoveExclusiveController();
								m_bSignInOrOutEventOccured = true;
							}
#else
							// need to leave our game session too
							CGameLobbyManager* pLobbyManager = GetGameLobbyManager();
							if (pLobbyManager)
							{
								pLobbyManager->LeaveGameSession(CGameLobbyManager::eLSR_SignedOut);
							}
#endif
						}
					}
				}
			}

			// clear invite data if we have any
			if (event.m_uParams.m_signIn.m_signedInState == IPlatformOS::SPlatformEvent::eSIS_NotSignedIn)
			{
				// don't want to clear this if signing in
				if (m_inviteAcceptedData.m_id != CryInvalidInvite)
				{
					// only clear the invite if the user signed out is the same
					// as the user accepting the invite
					bool isJoiningInvite = true;
					if (isJoiningInvite)
					{
						InvalidateInviteData();
					}
				}
			}
			break;
		}
	case IPlatformOS::SPlatformEvent::eET_StorageRemoved:
		CryLog("CGame::OnPlatformEvent() eET_StorageRemoved deviceRemovedIsPrimary=%d", event.m_uParams.m_storageRemoved.m_bDeviceRemovedIsPrimary);
		if (event.m_uParams.m_storageRemoved.m_bDeviceRemovedIsPrimary && gEnv->bMultiplayer)
		{
			CryLog("CGame::OnPlatformEvent() eET_StorageRemoved with the primary device being removed whilst in multiplayer.. we need to bail.");

			CErrorHandling* pErrorHandling = CErrorHandling::GetInstance();
			if (pErrorHandling)
			{
				pErrorHandling->OnFatalError(CErrorHandling::eFE_StorageRemoved);
			}
		}

		break;

	case IPlatformOS::SPlatformEvent::eET_FileError:
		{
			if (event.m_uParams.m_fileError.m_bRetry)
			{
				if (event.m_uParams.m_fileError.m_errorType & IPlatformOS::eFOC_ReadMask)
				{
					const string& file = GetLastSaveGame();
					if (!file.empty())
					{
						if (!GetIGameFramework()->LoadGame(file.c_str(), true))
							GetIGameFramework()->LoadGame(file.c_str(), false);
					}
				}
				else if (event.m_uParams.m_fileError.m_errorType & IPlatformOS::eFOC_WriteMask)
				{
					auto saveGameName = GetIGameFramework()->CreateSaveGameName();

					GetIGameFramework()->SaveGame(saveGameName.c_str());
				}
			}
			break;
		}

	case IPlatformOS::SPlatformEvent::eET_FileWrite:
		{
			if (event.m_uParams.m_fileWrite.m_type == IPlatformOS::SPlatformEvent::eFWT_CheckpointLevelStart
			    || event.m_uParams.m_fileWrite.m_type == IPlatformOS::SPlatformEvent::eFWT_Checkpoint)
			{
				m_bCheckPointSave = true;
			}

			// Don't display the icon immediately at level start since it happens during level load / precache and we'll end up breaking TCRs.
			// Instead wait for the SaveStart message to display the icon.
			if (event.m_uParams.m_fileWrite.m_type != IPlatformOS::SPlatformEvent::eFWT_CheckpointLevelStart)
			{
				if (event.m_uParams.m_fileWrite.m_type != IPlatformOS::SPlatformEvent::eFWT_SaveEnd)
				{
					if (m_saveIconMode != eSIM_Saving)
					{
						m_saveIconMode = eSIM_SaveStart;
						if (event.m_uParams.m_fileWrite.m_type == IPlatformOS::SPlatformEvent::eFWT_CreatingSave)
							m_bUserHasPhysicalStorage = true;
					}
				}
				else
				{
					m_saveIconMode = (event.m_uParams.m_fileWrite.m_type != IPlatformOS::SPlatformEvent::eFWT_SaveEnd) ? eSIM_SaveStart : eSIM_Finished;
				}
				if (CryGetCurrentThreadId() == gEnv->mMainThreadId)
				{
					UpdateSaveIcon();
				}
			}
			break;
		}
	case IPlatformOS::SPlatformEvent::eET_SystemMenu:
		{
			const IPlatformOS::SPlatformEvent::UEventParams::SSystemMenu& systemMenuEventData = event.m_uParams.m_systemMenu;
			if (systemMenuEventData.m_bOpened)
			{
#if !defined(DEDICATED_SERVER)
				if (!gEnv->bMultiplayer)
				{
					if (gEnv->pGameFramework->StartedGameContext())
					{
						//check if we're opening the system menu while it's still closing and the eventual unpause hasn't happen yet
						if (!m_bDeferredSystemMenuPause)
						{
							m_previousPausedGameState = gEnv->pGameFramework->IsGamePaused();
						}
						m_bPausedForSystemMenu = true;
						m_bDeferredSystemMenuPause = true;
					}
				}
#endif
				gEnv->pGameFramework->GetIActionMapManager()->Enable(false, true);
			}
			else if (systemMenuEventData.m_bClosed)
			{
#if !defined(DEDICATED_SERVER)
				if (!gEnv->bMultiplayer)
				{
					if (gEnv->pGameFramework->IsGamePaused())
					{
						const bool bInIngameMenu = IsGameActive() && m_pUIManager && m_pUIManager->IsInMenu();
						if (m_bPausedForSystemMenu && !m_bPausedForControllerDisconnect && !bInIngameMenu) // Only unpause if all states are clear
						{
							m_bPausedForSystemMenu = false;
							m_bDeferredSystemMenuPause = true;
						}
					}
				}
#endif
				gEnv->pGameFramework->GetIActionMapManager()->Enable(true);
			}
		}
		break;
	}
}

void CGame::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
		{
			CryLog("Preparing to load level!");
			INDENT_LOG_DURING_SCOPE();
			CFrontEndModelCache::Allow3dFrontEndAssets(false, true);
		}
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		{
			MEMSTAT_LABEL_SCOPED("CGame::OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START)");
			CryLog("CGame::OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START) while in %s mode", m_gameTypeMultiplayer ? "multiplayer" : "single player");
			INDENT_LOG_DURING_SCOPE();

			m_pPersistantStats->RegisterLevelTimeListeners();
			if (!m_pRayCaster)
			{
				m_pRayCaster = new GlobalRayCaster;
				m_pRayCaster->SetQuota(g_pGameCVars->g_gameRayCastQuota);
			}
			if (!m_pIntersectionTester)
			{
				m_pIntersectionTester = new GlobalIntersectionTester;
				m_pIntersectionTester->SetQuota(g_pGameCVars->g_gameIntersectionTestQuota);
			}
			if (!m_pBodyDamageManager)
			{
				m_pBodyDamageManager = new CBodyDamageManager();
			}
			if (!m_pGameActionHandlers)
			{
				m_pGameActionHandlers = new CGameInputActionHandlers;
			}

			m_pTacticalManager->Init();

			if (gEnv->bMultiplayer && !m_pRecordingSystem)
			{
				MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_None, "RecordingSystem");
				m_pRecordingSystem = new CRecordingSystem();
			}

			if (m_pScriptBindInteractiveObject)
			{
				m_pScriptBindInteractiveObject->GetObjectDataRegistry().Init();
			}

			if (gEnv->pScriptSystem)
			{
				// Load explosion shapes.
				gEnv->pScriptSystem->ExecuteFile("scripts/physics.lua", true, true);
			}
		}
		break;
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			MEMSTAT_LABEL_SCOPED("CGame::OnSystemEvent(ESYSTEM_EVENT_LEVEL_UNLOAD)");
			m_pHitDeathReactionsSystem->Reset();
			m_pPlayerVisTable->Reset();

			SAFE_DELETE(m_pRayCaster);
			SAFE_DELETE(m_pIntersectionTester);
			SAFE_DELETE(m_pBodyDamageManager);
			SAFE_DELETE(m_pGameActionHandlers);
			m_pWeaponSystem->FreePools();
			m_pTacticalManager->Reset();

			CActorManager::GetActorManager()->Reset(false);
			//	m_pPersistantStats->ResetCachedStats();
			CHUDEventDispatcher::FreeEventListeners();
			m_pPersistantStats->UnRegisterLevelTimeListeners();

			if (m_pScriptBindInteractiveObject)
			{
				m_pScriptBindInteractiveObject->GetObjectDataRegistry().Shutdown();
			}

			if (m_pMovingPlatformMgr)
				m_pMovingPlatformMgr->Reset();

			s_levelCVars.RevertCVarChanges();

			CornerSmoothing::OnLevelUnload();

			if (m_pGameCache)
			{
				m_pGameCache->Reset();
			}
		}
		break;

	case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
		{
			if (m_pParameterGameEffect)
			{
				m_pParameterGameEffect->Reset();
			}
			SAFE_DELETE(m_pRecordingSystem);
			CFrontEndModelCache::Allow3dFrontEndAssets(true, false);
		}
		break;

	case ESYSTEM_EVENT_TIME_OF_DAY_SET:
		{
			CGameRules* const pGameRules = GetGameRules();
			if (pGameRules)
			{
				pGameRules->OnTimeOfDaySet();
			}
			break;
		}
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		{
			if (wparam != 0)
			{
				// Update audio environments on the player if dropping into game!
				IEntity* const pLocalPlayerEntity = gEnv->pEntitySystem->GetEntity(g_pGame->GetIGameFramework()->GetClientActorId());

				if (pLocalPlayerEntity != NULL)
				{
					auto pIEntityAudioComponent = pLocalPlayerEntity->GetComponent<IEntityAudioComponent>();

					if (pIEntityAudioComponent != NULL)
					{
						pIEntityAudioComponent->SetCurrentEnvironments(CryAudio::InvalidAuxObjectId);
					}
				}
			}

			break;
		}
#if CRY_PLATFORM_DURANGO
	case ESYSTEM_EVENT_CONTROLLER_REMOVED:
		{
			if (HasExclusiveControllerIndex())
			{
				int exclusiveDevIdx = GetExclusiveControllerDeviceIndex();
				if (exclusiveDevIdx == wparam)
				{
					CUIInput* pUIEvt = UIEvents::Get<CUIInput>();
					if (pUIEvt)
						pUIEvt->ExclusiveControllerDisconnected();
				}
			}
		}
		break;

	case ESYSTEM_EVENT_USER_CHANGED:
		{

			SUserXUID newXuid((wchar_t*)wparam);
			SUserXUID currentXuid;
			gEnv->pSystem->GetPlatformOS()->UserGetXUID(0, currentXuid);

			const int newUserId = (uint32)lparam;
			const int currentUserId = gEnv->pSystem->GetPlatformOS()->UserGetId(0);

			const bool bSameUser = (newXuid == currentXuid);

			if (newUserId == 0)
			{
				// no user signed in
			}
			else if (!bSameUser)
			{
				m_userChangedDoSignOutAndIn = true;
				GetIGameFramework()->ExecuteCommandNextFrame("disconnect");
			}
		}
		break;

	case ESYSTEM_EVENT_DURANGO_CHANGE_VISIBILITY:
	case ESYSTEM_EVENT_CHANGE_FOCUS:
		{
			bool isVisible = wparam != 0;
			bool pause = !isVisible;

			if (pause)
			{
				m_wasGamePausedBeforePLMForcedPause = gEnv->pGameFramework->IsGamePaused();
				if (!gEnv->pGameFramework->IsGamePaused())
					gEnv->pGameFramework->PauseGame(true, false);
			}
			else
			{
				if (!m_wasGamePausedBeforePLMForcedPause)
					gEnv->pGameFramework->PauseGame(false, false);
				m_wasGamePausedBeforePLMForcedPause = false;
			}
		}
		break;

	case ESYSTEM_EVENT_PLM_ON_CONSTRAINED:
		{
			m_wasGamePausedBeforePLMForcedPause = gEnv->pGameFramework->IsGamePaused();
			gEnv->pGameFramework->PauseGame(true, false);
		}
		break;

	case ESYSTEM_EVENT_PLM_ON_RESUMING:
	case ESYSTEM_EVENT_PLM_ON_FULL:
		if (!m_wasGamePausedBeforePLMForcedPause)
		{
			gEnv->pGameFramework->PauseGame(false, false);
		}
		m_wasGamePausedBeforePLMForcedPause = false;
		break;
#endif // CRY_PLATFORM_DURANGO
	}
}

/* static */
void CGame::SetRichPresenceCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg)
{
	CryLog("[RichPresence] SetRichPresenceCallback - Rich presence %s with error code %d", error == eCLE_Success ? "succeeded" : "failed", error);

	bool setCurrentRichPresence = true;

	g_pGame->m_settingRichPresence = false;

	// so far, not supported on PC yet
	if (error != eCLE_Success)
	{
		if (error == eCLE_SystemIsBusy)
		{
			// if no new state pending, then try again with the previous state
			if (g_pGame->m_desiredRichPresenceState == eRPS_none)
			{
				CryLog("  failed to set rich presence and no new state desired, retrying pending %d", g_pGame->m_pendingRichPresenceState);
				g_pGame->m_desiredRichPresenceState = g_pGame->m_pendingRichPresenceState;
			}

			g_pGame->m_updateRichPresenceTimer = g_pGameCVars->g_updateRichPresenceInterval;
			setCurrentRichPresence = false;
		}
		else
		{
			CryLog("  cannot handle rich presence error, setting as complete for now");
		}
	}
	else
	{
		CryLog("  successfully set rich presence");
	}

	if (setCurrentRichPresence)
	{
		CryLog("  setting current rich presence");

		g_pGame->m_currentRichPresenceState = g_pGame->m_pendingRichPresenceState;
		g_pGame->m_currentRichPresenceSessionID = g_pGame->m_pendingRichPresenceSessionID;

		if (g_pGame->m_bRefreshRichPresence)
		{
			g_pGame->m_desiredRichPresenceState = (g_pGame->m_desiredRichPresenceState == eRPS_none) ? g_pGame->m_currentRichPresenceState : g_pGame->m_desiredRichPresenceState;
		}
	}

	g_pGame->m_pendingRichPresenceState = eRPS_none;
	g_pGame->m_pendingRichPresenceSessionID = CrySessionInvalidID;
	g_pGame->m_bRefreshRichPresence = false;
}

bool CGame::SetRichPresence(ERichPresenceState state)
{
	// don't set rich presence if we don't have a controller yet
	if (!m_hasExclusiveController)
	{
		CryLog("[RichPresence] not setting richpresence no player set yet");
		return true;
	}

	CrySessionID sessionID = CrySessionInvalidID;

	if ((m_currentRichPresenceState == state) && GameNetworkUtils::CompareCrySessionId(sessionID, m_currentRichPresenceSessionID))
	{
		if (state != eRPS_inGame || !gEnv->bMultiplayer)
		{
			CryLog("[RichPresence] not setting richpresence state %d multiplayer %d", state, gEnv->bMultiplayer);
			return true;
		}
	}

	// we are already setting rich presence, so wait until that task
	// has finished
	if (m_settingRichPresence)
	{
		CryLog("  current setting rich presence, setting desired state to %d", state);
		m_desiredRichPresenceState = state;
		return false;
	}

	ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
	ICryLobbyService* pLobbyService = pLobby ? pLobby->GetLobbyService(eCLS_Online) : NULL;
	ICryLobbyUI* pLobbyUI = pLobbyService ? pLobbyService->GetLobbyUI() : NULL;
	ECryLobbyError error = eCLE_Success;

	m_pendingRichPresenceSessionID = sessionID;

	if (pLobbyUI)
	{
		uint32 userIndex = GetExclusiveControllerDeviceIndex();

		CryLog("[RichPresence] SetRichPresence %d userIndex %d", state, userIndex);

		switch (state)
		{
		case eRPS_idle:
			{
				SCryLobbyUserData data;
				data.m_id = RICHPRESENCE_ID;
				data.m_type = eCLUDT_Int32;
				data.m_int32 = RICHPRESENCE_IDLE;
				error = pLobbyUI->SetRichPresence(userIndex, &data, 1, 0, CGame::SetRichPresenceCallback, this);
				break;
			}

		case eRPS_frontend:
			{
				SCryLobbyUserData data;
				data.m_id = RICHPRESENCE_ID;
				data.m_type = eCLUDT_Int32;
				data.m_int32 = RICHPRESENCE_LOBBY; // TODO michiel: add richpresence frontend to spa?
				error = pLobbyUI->SetRichPresence(userIndex, &data, 1, 0, CGame::SetRichPresenceCallback, this);
				break;
			}

		case eRPS_lobby:
			{
				SCryLobbyUserData data;
				data.m_id = RICHPRESENCE_ID;
				data.m_type = eCLUDT_Int32;
				data.m_int32 = RICHPRESENCE_LOBBY;
				error = pLobbyUI->SetRichPresence(userIndex, &data, 1, 0, CGame::SetRichPresenceCallback, this);
				break;
			}

		case eRPS_inGame:
			{
#if 0
				// Todo: MICHIEL add in-game presence to spa
				if (gEnv->bMultiplayer)
				{
					CGameRules* pGameRules = GetGameRules();
					const char* levelName = m_pFramework ? m_pFramework->GetLevelName() : NULL;
					const char* gameRulesName = pGameRules ? pGameRules->GetEntity()->GetClass()->GetName() : NULL;
					int32 gameMode = 0;
					int32 map = 0;

					if (levelName)
					{
						levelName = PathUtil::GetFileName(levelName);
						TRichPresenceMap::const_iterator iter = m_richPresence.find(levelName);
						map = (iter == m_richPresence.end()) ? 0 : iter->second;
					}

					if (gameRulesName)
					{
						TRichPresenceMap::const_iterator iter = m_richPresence.find(gameRulesName);
						gameMode = (iter == m_richPresence.end()) ? 0 : iter->second;
					}

					SCryLobbyUserData data[eRPT_Max];
					data[eRPT_String].m_id = RICHPRESENCE_ID;
					data[eRPT_String].m_type = eCLUDT_Int32;
					data[eRPT_String].m_int32 = RICHPRESENCE_GAMEPLAY;
					data[eRPT_Param1].m_id = RICHPRESENCE_GAMEMODES;
					data[eRPT_Param1].m_type = eCLUDT_Int32;
					data[eRPT_Param1].m_int32 = gameMode;
					data[eRPT_Param2].m_id = RICHPRESENCE_MAPS;
					data[eRPT_Param2].m_type = eCLUDT_Int32;
					data[eRPT_Param2].m_int32 = map;

					error = pLobbyUI->SetRichPresence(userIndex, data, 3, 0, CGame::SetRichPresenceCallback, this);
#else
				if (gEnv->bMultiplayer)
				{
#endif
				}
#if !defined(DEDICATED_SERVER)
				else
				{
					SCryLobbyUserData data;
					data.m_id = RICHPRESENCE_ID;
					data.m_type = eCLUDT_Int32;
					data.m_int32 = RICHPRESENCE_SINGLEPLAYER;

					error = pLobbyUI->SetRichPresence(userIndex, &data, 1, 0, CGame::SetRichPresenceCallback, this);
				}
#endif
				break;
			}
		default:
			CryLog("[RichPresence] SetRichPresence - unknown rich presence %d", state);
			break;
		}

		if (error != eCLE_Success)
		{
			// failed to set rich presence, possibly because of too many lobby tasks,
			// store it and try again later
			m_desiredRichPresenceState = state;
			m_pendingRichPresenceSessionID = CrySessionInvalidID;

			CryLog("[RichPresence] SetRichPresence - Rich presence %s with error code %d", error == eCLE_Success ? "succeeded" : "failed", error);
		}
		else
		{
			m_settingRichPresence = true;
			m_pendingRichPresenceState = state;
			m_desiredRichPresenceState = eRPS_none;

			CryLog("[RichPresence] SetRichPresence - Rich presence has been successfully started");
		}
	}
#if !defined(_RELEASE)
	else
	{
		error = eCLE_InternalError;
		CryLog("[RichPresence] SetRichPresence called but we have no lobby, tried to set state to %d", state);
	}
#endif

	return (error == eCLE_Success);
}

void CGame::InitRichPresence()
{
	XmlNodeRef rpXML = gEnv->pGameFramework->GetISystem()->LoadXmlFromFile("Scripts/Network/RichPresence.xml");
	if (rpXML)
	{
		int numElements = rpXML->getChildCount();
		for (int i = 0; i < numElements; ++i)
		{
			XmlNodeRef childXml = rpXML->getChild(i);
			const char* levelName = NULL;
			int id = -1;

			if (childXml->getAttr("name", &levelName) && childXml->getAttr("id", id))
			{
				m_richPresence[levelName] = id;
			}
		}
	}

	// management
	m_desiredRichPresenceState = eRPS_none;
	m_pendingRichPresenceState = eRPS_none;
	m_currentRichPresenceState = eRPS_none;
}

void CGame::AddRichPresence(const char* path)
{
	XmlNodeRef rpXML = gEnv->pGameFramework->GetISystem()->LoadXmlFromFile(path);

	if (rpXML)
	{
		int numElements = rpXML->getChildCount();
		for (int i = 0; i < numElements; ++i)
		{
			XmlNodeRef childXml = rpXML->getChild(i);
			const char* levelName = NULL;
			int id = -1;

			if (childXml->getAttr("name", &levelName) && childXml->getAttr("id", id))
			{
				m_richPresence[levelName] = id;
			}
		}
	}
}

//static---------------------------------------
void CGame::PartyMembersCallback(UCryLobbyEventData eventData, void* arg)
{
	CryLogAlways("CGame::PartyMembersCallback(UCryLobbyEventData eventData, void *arg)");

	CGame* pGame = (CGame*)arg;
	CRY_ASSERT(pGame);
	pGame->m_currentXboxLivePartySize = eventData.pPartyMembers->m_numMembers;
}

//static---------------------------------------
void CGame::UserProfileChangedCallback(UCryLobbyEventData eventData, void* arg)
{
	CryLog("[GameLobbyManager] UserProfileChangedCallback user %d", eventData.pUserProfileChanged->m_user);

	CGame* pGame = (CGame*)arg;
	assert(pGame);

	IGameFramework* pFramework = pGame ? pGame->GetIGameFramework() : NULL;
	uint32 userIndex = pGame ? pGame->GetExclusiveControllerDeviceIndex() : 0;

	if (eventData.pUserProfileChanged->m_user == userIndex)
	{
		if (pFramework && pFramework->StartedGameContext())
		{
			pGame->SetUserProfileChanged(true);
		}
	}
}

//static---------------------------------------
void CGame::InviteAcceptedCallback(UCryLobbyEventData eventData, void* arg)
{
	CryLog("[Invite] InviteAcceptedCallback");

	CGame* pGame = (CGame*)arg;

	SCryLobbyInviteAcceptedData* inviteData = eventData.pInviteAcceptedData;

	bool acceptInvite = true;

	CRY_ASSERT_MESSAGE(pGame, "No game!");

	// we should always accept the invite if we have no exclusive controller
	if (pGame->m_hasExclusiveController)
	{
		// can't possibly be in a squad if we're not multiplayer, i hope
		if (gEnv->bMultiplayer && inviteData->m_error == eCLE_Success)
		{
			ICryLobby* pLobby = gEnv->pNetwork->GetLobby();
			ICryLobbyService* pLobbyService = pLobby ? pLobby->GetLobbyService(eCLS_Online) : NULL;
			ICryMatchMaking* pMatchMaking = pLobbyService ? pLobbyService->GetMatchMaking() : NULL;

			bool alreadyInSession = false;

			// the session we are trying to join is the same as the session we are in
			if (alreadyInSession)
			{
				CWarningsManager* pWarnings = pGame->GetWarnings();
				unsigned int exclusiveControllerIndex = pGame->GetExclusiveControllerDeviceIndex();
				if (exclusiveControllerIndex == inviteData->m_user)
				{
					CryLog("[invite] we tried to accept an invite to a session we are already in");

					// the user is already in the session, tell them
					acceptInvite = false;

					pWarnings->RemoveGameWarning("InviteFailedIsHost");
					pWarnings->RemoveGameWarning("InviteFailedAlreadyInSession");
					pWarnings->AddGameWarning("InviteFailedAlreadyInSession");
				}
				else if (pGame->m_pSquadManager->InCharge())
				{
					CryLog("[invite] session is hosted on this system, yet someone on this system has tried to join it via invite");

					// someone else is trying to accept the session on the same system
					// as the host. only a problem on 360 which can have multiple users
					// signed in at a time
					acceptInvite = false;

					pWarnings->RemoveGameWarning("InviteFailedIsHost");
					pWarnings->RemoveGameWarning("InviteFailedAlreadyInSession");
					pWarnings->AddGameWarning("InviteFailedIsHost");
				}
			}
		}
	}

	if (acceptInvite)
	{
		CryInviteID id = inviteData->m_id;
		ECryLobbyInviteType inviteType = inviteData->m_id->IsFromInvite() ? eCLIT_InviteToSquad : eCLIT_JoinSessionInProgress;

		pGame->SetInviteData(inviteData->m_service, inviteData->m_user, id, inviteData->m_error, inviteType);
		pGame->SetInviteAcceptedState(eIAS_Init);
		pGame->m_bLoggedInFromInvite = true;
	}
}

//static---------------------------------------
void CGame::OnlineStateCallback(UCryLobbyEventData eventData, void* arg)
{
	CryLog("[Game] OnlineStateCallback");

	CGame* pGame = (CGame*)arg;
	CRY_ASSERT_MESSAGE(pGame, "No game!");

	SCryLobbyOnlineStateData* pOnlineStateData = eventData.pOnlineStateData;
	if (pOnlineStateData)
	{
		if (pOnlineStateData->m_curState == eOS_SignedOut)
		{
			bool isCurrentUser = true;

			if ((pGame->m_inviteAcceptedData.m_id != CryInvalidInvite) && (isCurrentUser))
			{
				if (pOnlineStateData->m_reason != eCLE_CyclingForInvite)
				{
					CryLog("[Game] User %d signed out and was accepting an invite, invalidating the invite", pOnlineStateData->m_user);

					pGame->InvalidateInviteData();
				}
			}
		}

	}
}

//static---------------------------------------
void CGame::EthernetStateCallback(UCryLobbyEventData eventData, void* arg)
{
	CryLog("[Game] EthernetStateCallback");

	CGame* pGame = (CGame*)arg;
	CRY_ASSERT_MESSAGE(pGame, "No game!");

	SCryLobbyEthernetStateData* pEthernetStateData = eventData.pEthernetStateData;

	if (pEthernetStateData)
	{
		ECableState cableState = pEthernetStateData->m_curState;
		if (cableState == eCS_Unplugged || cableState == eCS_Disconnected)
		{
			if (pGame->m_inviteAcceptedData.m_id != CryInvalidInvite)
			{
				CryLog("[Game] EthernetState has been %s, invalidating invite data", cableState == eCS_Unplugged ? "unplugged" : "disconnected");

				// cable has been pulled, invalidate the invite data
				pGame->InvalidateInviteData();
			}
		}
	}
}

THUDWarningId CGame::AddGameWarning(const char* stringId, const char* paramMessage, IGameWarningsListener* pListener /*= NULL*/)
{
	if (GetWarnings())
		return GetWarnings()->AddGameWarning(stringId, paramMessage, pListener);
	return 0;
}

void CGame::RemoveGameWarning(const char* stringId)
{
	if (GetWarnings())
		GetWarnings()->RemoveGameWarning(stringId);
}

void CGame::RenderGameWarnings()
{
}

bool CGame::GameEndLevel(const char* nextLevel)
{
	// Ensure objectives cleared when chaining levels
	g_pGame->GetMOSystem()->DeactivateObjectives();
	return false;
}

void CGame::OnRenderScene(const SRenderingPassInfo& passInfo)
{
	TRenderSceneListeners::const_iterator it = m_renderSceneListeners.begin();
	TRenderSceneListeners::const_iterator end = m_renderSceneListeners.end();
	for (; it != end; ++it)
	{
		(*it)->OnRenderScene(passInfo);
	}
}

uint32 CGame::GetRandomNumber()
{
	return m_randomGenerator.GenerateUint32();
}

float CGame::GetRandomFloat()
{
	return m_randomGenerator.GenerateFloat();
}

bool CGame::LoadLastSave()
{
#if !defined(DEDICATED_SERVER)
	if (!gEnv->bMultiplayer)
	{
		bool bLoadSave = true;
		if (gEnv->IsEditor())
		{
			ICVar* pAllowSaveLoadInEditor = gEnv->pConsole->GetCVar("g_allowSaveLoadInEditor");
			if (pAllowSaveLoadInEditor)
			{
				bLoadSave = (pAllowSaveLoadInEditor->GetIVal() != 0);
			}
			else
			{
				bLoadSave = false;
			}

			if (!bLoadSave) // Wont go through normal path which reloads hud, reload here
			{
				g_pGame->PostSerialize();
			}
		}

		bool bSuccess = false;

		if (bLoadSave)
		{
			if (g_pGameCVars->g_enableSlimCheckpoints)
			{
				bSuccess = GetIGameFramework()->GetICheckpointSystem()->LoadLastCheckpoint();
			}
			else
			{
				string levelName("");
				m_bLastSaveDirty = true; // Just to be safe (Some code paths won't refresh level name, i.e. quicksave and possibly others)
				const string& fileName = g_pGame->GetLastSaveGame(levelName);

				if (!fileName.empty())
				{
					gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_RESUME_GAME, 0, 0);
					// Attempt a quick-load
					ELoadGameResult result = g_pGame->GetIGameFramework()->LoadGame(fileName.c_str(), true);

					if (result == eLGR_CantQuick_NeedFullLoad)
					{
						// Fall-back on a full-load
						result = g_pGame->GetIGameFramework()->LoadGame(fileName.c_str(), false);
					}

					bSuccess = result == eLGR_Ok;
				}
				else
				{
					GameWarning("[LoadLastSave]: No save game found to load.");
				}
			}
		}
		else
		{
			bSuccess = CGodMode::GetInstance().RespawnPlayerIfDead();
		}

		return bSuccess;
	}
#endif

	return false;
}

CGameLobby* CGame::GetGameLobby()
{
	return m_pGameLobbyManager ? m_pGameLobbyManager->GetGameLobby() : NULL;
}

void CGame::ClearGameSessionHandler()
{
	GetIGameFramework()->SetGameSessionHandler(NULL);
	m_pLobbySessionHandler = NULL;
}

void CGame::OnBeforeEditorLevelLoad()
{
	m_pGameParametersStorage->ReleaseLevelResources();
	if (m_pBodyDamageManager)
		m_pBodyDamageManager->FlushLevelResourcesCache();

	if (m_pTacticalManager)
	{
		m_pTacticalManager->ClearAllTacticalPoints();
	}

	m_pLedgeManager->Reset();

	CSmokeManager::GetSmokeManager()->ReleaseObstructionObjects();
}

void CGame::OnExitGameSession()
{
}

void CGame::PreSerialize()
{
	//This is called while loading a saved game
	//Reset some game systems that might cause problems during loading
	m_pWeaponSystem->GetTracerManager().Reset();
	gEnv->pGameFramework->GetIItemSystem()->Reset();
	m_pGameParametersStorage->GetItemResourceCache().Get1pDBAManager().Reset();

	g_tacticalPointLanguageExtender.Reset();
}

// this function is now called after all entities have been fullserialized'd.  (it was part of gamerules serialization before).
// This means that almost all modules could now do the whole serialization here, instead of doing part here and part in postserialize().
// however, there is no point in changing that for now.
void CGame::FullSerialize(TSerialize serializer)
{
	serializer.BeginGroup("IGame");
	if (GetMOSystem())
	{
		GetMOSystem()->Serialize(serializer);
	}

	if (m_pLedgeManager)
	{
		m_pLedgeManager->Serialize(serializer);
	}

	if (m_pGameAISystem)
	{
		m_pGameAISystem->Serialize(serializer);
		g_tacticalPointLanguageExtender.FullSerialize(serializer);
	}

	if (m_statsRecorder)
		m_statsRecorder->Serialize(serializer);

	if (m_pTacticalManager)
	{
		m_pTacticalManager->Serialize(serializer);
	}

	if (gEnv->pGameFramework->GetICustomActionManager())
		gEnv->pGameFramework->GetICustomActionManager()->Serialize(serializer);

	serializer.EndGroup();
}

void CGame::PostSerialize()
{
	//reset HUD
	if (m_pUIManager)
		m_pUIManager->ActivateDefaultState();

	// Need to init player after dead or load saved
	SHUDEvent hudEvent_initLocalPlayer(eHUDEvent_OnInitLocalPlayer);
	IActor* pActor = gEnv->pGameFramework->GetClientActor();
	if (pActor)
	{
		hudEvent_initLocalPlayer.AddData(static_cast<int>(pActor->GetEntityId()));
	}
	else
	{
		hudEvent_initLocalPlayer.AddData(static_cast<int>(0));
	}

	CHUDEventDispatcher::CallEvent(hudEvent_initLocalPlayer);

	// Ensure radar back to saved state
	if (m_pTacticalManager)
	{
		m_pTacticalManager->PostSerialize();
	}

	if (m_pGameAISystem)
	{
		m_pGameAISystem->PostSerialize();
	}

	if (m_pUIManager)
	{
		m_pUIManager->PostSerialize();
	}

	// Want to set last save as dirty since the timestamp was updated, so thats the latest save now which should be resumed from
	m_bLastSaveDirty = true;
}

void CGame::OnDeathReloadComplete()
{
#if !defined(DEDICATED_SERVER)
	if (!gEnv->bMultiplayer)
	{
		if (!CGodMode::GetInstance().IsGodModeActive())
		{
			//load the last save-game if available
			gEnv->pConsole->ExecuteString("loadLastSave", false, true);
		}
	}
#endif
}

bool CGame::IsGameActive() const
{
	assert(g_pGame);
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	assert(pGameFramework);
	return (pGameFramework->StartingGameContext() || pGameFramework->StartedGameContext()) && (IsGameSessionHostMigrating() || pGameFramework->GetClientChannel());
}

void CGame::SetCrashDebugMessage(const char* const msg)
{
	cry_strcpy(gEnv->szDebugStatus, msg);
}

void CGame::AppendCrashDebugMessage(const char* const msg)
{
	cry_strcat(gEnv->szDebugStatus, msg);
}

void CGame::OnDedicatedConfigEntry(const char* szKey, const char* szValue)
{
	CryLog("CGame::OnDedicatedConfigEntry() option=%s, value=%s, alreadyFound=%s", szKey, szValue, m_variantOptions.find(szKey) == m_variantOptions.end() ? "false" : "true");
	if (m_variantOptions.find(szKey) == m_variantOptions.end())
	{
		m_variantOptions[szKey] = szValue;
	}

#if defined(CVARS_WHITELIST)
	ICVarsWhitelist* pCVarsWhitelist = gEnv->pSystem->GetCVarsWhiteList();
	bool execute = (pCVarsWhitelist) ? pCVarsWhitelist->IsWhiteListed(szKey, false) : true;
	if (execute)
#endif // defined(CVARS_WHITELIST)
	{
		gEnv->pConsole->LoadConfigVar(szKey, szValue);
	}
}

void CGame::CMPConfigSink::OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
{
	if ((stricmp("g_matchmakingblock", szKey) != 0) && (stricmp("g_matchmakingversion", szKey) != 0))
	{
		CryLog("nigel.cfg::'%s' only supports g_matchmakingblock & g_matchmakingversion", szKey);
		return;
	}
	ICVar* pCVar = gEnv->pConsole->GetCVar(szKey);
	if (pCVar)
	{
		pCVar->ForceSet(szValue);
	}
}

void CGame::LoadPatchLocalizationData()
{
	ILocalizationManager* pLocMan = GetISystem()->GetLocalizationManager();

	string sLocaFolderName = "Libs/Localization/";

	ICVar* pCVar = gEnv->pConsole->GetCVar("g_language");
	const char* g_languageIs = pCVar->GetString();

	//here we just load the patch localization file

	string sFileName;
	sFileName.Format("localization_%s.xml", g_languageIs);
	CryLog("CGame::LoadPatchLocalizationData() is loading localization file=%s", sFileName.c_str());

	string locaFile = sLocaFolderName + sFileName.c_str();

	if (pLocMan->InitLocalizationData(locaFile.c_str()))
	{
		if (!gEnv->IsEditor())
		{
			// load only the init xml files
			pLocMan->LoadLocalizationDataByTag("patch");
		}
	}
}

void CGame::VerifyMaxPlayers(ICVar* pVar)
{
	int nPlayers = pVar->GetIVal();
	if (nPlayers < 2 || nPlayers > MAX_PLAYER_LIMIT)
		pVar->Set(CLAMP(nPlayers, 2, MAX_PLAYER_LIMIT));
}

void CGame::SDedicatedConfigSink::OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
{
	g_pGame->OnDedicatedConfigEntry(szKey, szValue);
}

void CGame::QueueDeferredKill(const EntityId entityID)
{
	stl::push_back_unique(m_deferredKills, entityID);
}

void CGame::CommitDeferredKills()
{
	if (m_deferredKills.empty())
		return;

	DeferredKills::const_iterator it = m_deferredKills.begin();
	DeferredKills::const_iterator end = m_deferredKills.end();

	for (; it != end; ++it)
	{
		const EntityId entityID = *it;

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID))
		{
			// Kill the agent by entityId a lot of damage.
			const HitInfo hitinfo(entityID, entityID, entityID,
			                      10000.0f, 0.0f, 0, -1, CGameRules::EHitType::Punish,
			                      ZERO, ZERO, ZERO);

			GetGameRules()->ClientHit(hitinfo);
		}
	}

	m_deferredKills.clear();
}

void CGame::AddPersistentAccessories()
{
#if !defined(DEDICATED_SERVER)
	if (!gEnv->bMultiplayer)
	{
		// Note: If this code changes, update the code right above which does the pre-cache during level load (case eAE_loadLevel)
		// Give the player his attachments
		if (IActor* pClientActor = GetIGameFramework()->GetClientActor())
		{
			IInventory* pInventory = pClientActor->GetInventory();
			if (pInventory)
			{
				pInventory->IgnoreNextClear();

				IItemSystem* pItemSystem = GetIGameFramework()->GetIItemSystem();

				for (int i = 0; i < s_numUnlockableAttachments; ++i)
				{
					int hasAttachment = m_pPersistantStats->GetStat(s_unlockableAttachmentNames[i], EMPS_AttachmentUnlocked);
					if (hasAttachment)
					{
						pItemSystem->GiveItem(pClientActor, s_unlockableAttachmentNames[i], false, false, true);
					}
				}
			}
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/// Export & load from level pak file
/// IMPORTANT: KEEP THE ORDER OF EXPORT-LOAD!

namespace
{
const char* GenerateExportingBaseFileName(const char* levelName, const char* missionName)
{
	static char baseFileName[512];   //This must be persistent memory

	cry_sprintf(baseFileName, "%s/datafile_%s", levelName, missionName);

	return baseFileName;
}
}

IGame::ExportFilesInfo CGame::ExportLevelData(const char* levelName, const char* missionName) const
{
	const char* baseFileName = GenerateExportingBaseFileName(levelName, missionName);
	char fileName[512];

	uint32 exportedFileCount = 0; // Note: Increase the counter for every system which exports a file, regardless of success or not

	// Shape-volume data
	IGameVolumesEdit* pGameVolumesEdit = gEnv->pGameFramework->GetIGameVolumesManager()->GetEditorInterface();
	if (pGameVolumesEdit != NULL)
	{
		IGame::ExportFilesInfo::GetNameForFile(baseFileName, exportedFileCount, fileName, sizeof(fileName));
		pGameVolumesEdit->Export(fileName);
	}
	exportedFileCount++;

	// Ledge manager data
	CLedgeManagerEdit* pLedgeManagerEdit = m_pLedgeManager ? m_pLedgeManager->GetEditorManager() : NULL;
	if (pLedgeManagerEdit != NULL)
	{
		IGame::ExportFilesInfo::GetNameForFile(baseFileName, exportedFileCount, fileName, sizeof(fileName));
		pLedgeManagerEdit->Export(fileName);
	}
	exportedFileCount++;

	return IGame::ExportFilesInfo(baseFileName, exportedFileCount);
}

void CGame::LoadExportedLevelData(const char* levelName, const char* missionName)
{
	const char* baseFileName = GenerateExportingBaseFileName(levelName, missionName);

	char fileName[512];
	uint32 loadedFileCount = 0; // Note: Increase the counter for every system which loads a file, regardless of success or not

	// Shape-volume data
	IGameVolumes* pGameVolumes = gEnv->pGameFramework->GetIGameVolumesManager();
	if (pGameVolumes != NULL)
	{
		IGame::ExportFilesInfo::GetNameForFile(baseFileName, loadedFileCount, fileName, sizeof(fileName));
		pGameVolumes->Load(fileName);
	}
	loadedFileCount++;

	// Ledge manager data
	if (m_pLedgeManager != NULL)
	{
		IGame::ExportFilesInfo::GetNameForFile(baseFileName, loadedFileCount, fileName, sizeof(fileName));
		m_pLedgeManager->Load(fileName);
	}
	loadedFileCount++;
}

void CGame::GetTelemetryTimers(int& careerTime, int& gameTime, int& sessionTime, void* pArg)
{
	CPersistantStats* pStats = g_pGame->GetPersistantStats();
	if (pStats)
	{
		careerTime = pStats->GetStat(EIPS_TimePlayed);
		//game time follows same rules as career time, just telemetry component starts counting from 0 at start of each session
		gameTime = careerTime;
	}

	sessionTime = (int)gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetSeconds();
}

int CGame::GetDifficultyForTelemetry(int difficulty /*=-1*/) const
{
	//ICrysis3PartnerTelemetry::EGameDifficulty telemDiff = ICrysis3PartnerTelemetry::eGD_None;

	//if( gEnv->bMultiplayer == false )
	//{
	//	if( difficulty==-1 )
	//	{
	//		difficulty = g_pGameCVars->g_difficultyLevel;
	//	}
	//	switch( difficulty )
	//	{
	//	case eDifficulty_Easy:			telemDiff = ICrysis3PartnerTelemetry::eGD_Easy;
	//		break;
	//	case eDifficulty_Default:		//intentional fall through
	//	case eDifficulty_Normal:		telemDiff = ICrysis3PartnerTelemetry::eGD_Normal;
	//		break;
	//	case eDifficulty_Hard:			telemDiff = ICrysis3PartnerTelemetry::eGD_Hard;
	//		break;
	//	case eDifficulty_Delta:			telemDiff = ICrysis3PartnerTelemetry::eGD_VeryHard;
	//		break;
	//	case eDifficulty_PostHuman:	telemDiff = ICrysis3PartnerTelemetry::eGD_Expert;
	//		break;
	//	}
	//}

	//return telemDiff;
	return -1;
}

float CGame::GetFOV() const
{
	float fFov(g_pGameCVars->cl_fov);
	if (gEnv->bMultiplayer)
	{
		CGameRules* pGameRules = GetGameRules();
		if (pGameRules && !pGameRules->IsIntroSequenceCurrentlyPlaying())
		{
			return fFov * g_pGameCVars->cl_mp_fov_scalar;
		}
	}

	return fFov;
}

float CGame::GetPowerSprintTargetFov() const
{
	if (gEnv->bMultiplayer)
	{
		return g_pGameCVars->pl_movement.power_sprint_targetFov * g_pGameCVars->cl_mp_fov_scalar;
	}
	else
		return g_pGameCVars->pl_movement.power_sprint_targetFov;
}

int CGame::OnCreatePhysicalEntityLogged(const EventPhys* pEvent)
{
	const EventPhysCreateEntityPart* pCEvent = static_cast<const EventPhysCreateEntityPart*>(pEvent);
	
	if (pCEvent->iForeignData != PHYS_FOREIGN_ID_STATIC)
		return 1;

	IRenderNode* rn = static_cast<IRenderNode*>(pCEvent->pForeignData);
	if (eERType_Vegetation == rn->GetRenderNodeType())
	{
		// notify PerceptionManager
		IPerceptionManager* perceptionManager = IPerceptionManager::GetInstance();
		if (perceptionManager)
		{
			IEntity* pNewEnt = static_cast<IEntity*>(pCEvent->pEntNew->GetForeignData(PHYS_FOREIGN_ID_ENTITY));
			if (pNewEnt)
			{
				// Classify the collision type.
				SAICollisionObjClassification type = AICOL_LARGE;
				if (pCEvent->breakSize < 0.5f)
					type = AICOL_SMALL;
				else if (pCEvent->breakSize < 2.0f)
					type = AICOL_MEDIUM;
				else
					type = AICOL_LARGE;

				// Magic formula to calculate the reaction and sound radii.
				const float impulse = pCEvent->breakImpulse.GetLength();
				const float reactionRadius = pCEvent->breakSize * 2.0f;
				const float soundRadius = 10.0f + sqrtf(clamp_tpl(impulse / 800.0f, 0.0f, 1.0f)) * 60.0f;

				SAIStimulus stim(AISTIM_COLLISION, type, pNewEnt ? pNewEnt->GetId() : 0, 0, rn->GetPos(), ZERO, reactionRadius);
				SAIStimulus stimSound(AISTIM_SOUND, type == AICOL_SMALL ? AISOUND_COLLISION : AISOUND_COLLISION_LOUD,
					pNewEnt ? pNewEnt->GetId() : 0, 0, rn->GetPos(), ZERO, soundRadius, AISTIMPROC_FILTER_LINK_WITH_PREVIOUS);
				
				perceptionManager->RegisterStimulus(stim);
				perceptionManager->RegisterStimulus(stimSound);
			}
		}
	}
	return 1;
}

static void OnChangedStereoRenderDevice(ICVar* pStereoRenderDevice)
{
	if (!pStereoRenderDevice)
	{
		return;
	}
	g_pGame->SetRenderingToHMD(pStereoRenderDevice->GetIVal() == 7);
}

static const char* g_checkpointOrder[] =
{
	//	Jailbreak
	"CP1_Dock_A",
	"CP2_Dock_B",
	"CP3_Platform_Exit",
	"CP4_Bridge",
	"CP5_Dome",
	"CP6_Reactor",
	"CP7_Tower",
	"AB0_Minefield",
	//	Fields
	"Fields_VVV_CP1",
	"Fields_AB1_ReachedSniper",
	"Fields_AB1_TowerDead",
	"Fields_AB1_Clearing",
	"Fields_AB1_TunnelStart",
	"Fields_AB1_AB2_Tunnel",
	"Fields_AB1_AB2_PreStalker",
	"Fields_Shed",
	"Fields_EndShed",
	"Fields_Hill",
	//	Canyon
	"canyon_top_save_1",
	"canyon_lowervvv_save_2",
	"canyon_dam_top_save_3",
	"canyon_dam_1_save_4",
	"canyon_dam_2_save_5",
	"canyon_flood_save_6",
	"canyon_relay_save_7",
	"canyon_tower_save_8",
	"canyon_miniboss_save_9",
	"canyon_controlroom_save_10",
	//	Swamp
	"Swamp1",
	"Swamp2",
	"Swamp3",
	"Swamp4",
	"Swamp5",
	"Swamp6",
	"Swamp6b",
	"Swamp7",
	"Swamp8",
	"Swamp9",
	"Swamp10",
	"Swamp11",
	//	River
	"CP1_Hivemind",
	"CP2_Supercharge",
	"CP3_Stalkers",
	"CP4_CellIntel",
	"CP5_Buggy",
	"CP6_Bridge",
	"CP7_BridgeInterior",
	"CP8_RiverBed",
	//	River2
	//	"CP8_RiverBed",
	"CP9_Pinger",
	"CP10_SO_ICV",
	"CP11_Gate",
	"CP12_Approach",
	"CP13_Defences",
	"CP14_DefencesSO",
	"CP15_Pipe_Entrance",
	//	"CP16_Rear_Entrance",
	"CP17_CELLHQDefences",
	"CP18_CellHQ",
	//	Islands
	"islands_checkpoint_100",
	"islands_checkpoint_110",
	"islands_checkpoint_120",
	"islands_checkpoint_130",
	"islands_checkpoint_200",
	"islands_checkpoint_240",
	"islands_checkpoint_220",
	"islands_checkpoint_290",
	"islands_checkpoint_230",
	"islands_checkpoint_232",
	"islands_checkpoint_210",
	"islands_checkpoint_212",
	"islands_checkpoint_250",
	"islands_checkpoint_260",
	"islands_checkpoint_291",
	"islands_checkpoint_310",
	"islands_checkpoint_320",
	"islands_checkpoint_330",
	"islands_checkpoint_350",
	"360",
	"islands_checkpoint_201",
	"islands_checkpoint_202",
	"islands_checkpoint_203",
	//	Cave
	"savegame05",
	"savegame10",
	"savegame11",
	"savegame12",
	"savegame13",
	"savegame14",
	"savegame20",
	"savegame21",
	"savegame22",
	"savegame23",
	"savegame24A",
	"savegame24B",
	"savegame25",
	"savegame26",
	"savegame30",
	"savegame31"
};

static const size_t g_checkpointOrderSize = (CRY_ARRAY_COUNT(g_checkpointOrder));

int CGame::GetCheckpointIDForTelemetry(const char* checkpointName) const
{
	for (int i = 0; i < g_checkpointOrderSize; ++i)
	{
		if (stricmp(g_checkpointOrder[i], checkpointName) == 0)
			return i;
	}
	assert(!"invalid checkpoint name in CGame::GetCheckpointIDForTelemetry()");
	return 0;
}

#if CRY_PLATFORM_DURANGO
//////////////////////////////////////////////////////////////////////////
void CGame::EnsureSigninState()
{
	if (m_userChangedDoSignOutAndIn == true)
	{
		m_userChangedDoSignOutAndIn = false;
		LogoutCurrentUser(eLR_SetExclusiveController);
		gEnv->pSystem->GetPlatformOS()->UserDoSignIn(1);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
