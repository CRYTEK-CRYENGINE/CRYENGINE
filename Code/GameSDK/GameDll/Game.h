// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 3:8:2004   11:23 : Created by Márcio Martins

*************************************************************************/

#pragma once

#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <IGameObjectSystem.h>
#include <IGameObject.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <IActorSystem.h>
#include <CryCore/StlUtils.h>
#include <CryPhysics/RayCastQueue.h>
#include <CryPhysics/IntersectionTestQueue.h>

#include "TelemetryCollector.h"

#include "Network/GameNetworkDefines.h"
#include "Network/Lobby/MatchMakingTelemetry.h"

#include <CryCore/Platform/CryWindows.h>

#define GAME_NAME     "GAMESDK"
#define GAME_LONGNAME "CRYENGINE GAME SDK"

#if 1 || !defined(_RELEASE)
	#define USE_DEDICATED_INPUT 1
#else
	#define USE_DEDICATED_INPUT 0
#endif

#if !defined(_RELEASE)
	#define ENABLE_VISUAL_DEBUG_PROTOTYPE 0
#else
	#define ENABLE_VISUAL_DEBUG_PROTOTYPE 0
#endif

#if !defined(_RELEASE)
	#define ENABLE_CONSOLE_GAME_DEBUG_COMMANDS 1
#else
	#define ENABLE_CONSOLE_GAME_DEBUG_COMMANDS 0
#endif

#define USE_REGION_FILTER 1
#define MAX_LOCAL_USERS   4

struct ISystem;
struct IConsole;

class CDLCManager;

class CScriptBind_Actor;
class CScriptBind_Item;
class CScriptBind_Weapon;
class CScriptBind_GameRules;
class CScriptBind_Game;
class CScriptBind_GameAI;
class CScriptBind_HUD;
class CScriptBind_HitDeathReactions;
class CScriptBind_Boids;
class CScriptBind_InteractiveObject;
class CScriptBind_MatchMaking;
class CScriptBind_Turret;
class CScriptBind_ProtectedBinds;
class CScriptBind_LightningArc;
class CWeaponSystem;
class CGameTokenSignalCreator;

class CProfileOptions;
class CTacticalManager;
class CWarningsManager;
#if defined(ENABLE_PROFILING_CODE)
class CTelemetryBuffer;
#endif
class CGameMechanismManager;
class CPlaylistManager;
class CGameInputActionHandlers;
class CGameCache;
class CPlayerVisTable;

struct IActionMap;
struct IActionFilter;
class CGameActions;
class CGameRules;
class CBulletTime;

struct SCVars;
struct SItemStrings;
class CRevertibleConfigLoader;
class CGameSharedParametersStorage;
class CPersistantStats;
class CGameAudio;
class CLaptopUtil;
class CScreenEffects;
class CLedgeManager;
class CWaterPuddleManager;
class CRecordingSystem;
class CHUDMissionObjectiveSystem; // TODO : Remove me?
class CGameBrowser;
class CGameLobby;
class CGameLobbyManager;
class CGameStateRecorder;
#if IMPLEMENT_PC_BLADES
class CGameServerLists;
#endif //IMPLEMENT_PC_BLADES
class CSquadManager;
class CCryLobbySessionHandler;
class CUIManager;
class CStatsRecordingMgr;
class CPatchPakManager;
class CDataPatchDownloader;
class CGameLocalizationManager;
class CAutoAimManager;
class CHitDeathReactionsSystem;
class CBodyDamageManager;
#if USE_LAGOMETER
class CLagOMeter;
#endif
class ITelemetryCollector;
class CEquipmentLoadout;
class CPlayerProgression;
class CGameAISystem;
class CVisualDebugPrototype;
class CMovementTransitionsSystem;
class CDownloadMgr;
class CHudInterferenceGameEffect;
class CSceneBlurGameEffect;
class CLightningGameEffect;
class CParameterGameEffect;
class CPlaylistActivityTracker;

class CInteractiveObjectRegistry;

class CBurnEffectManager;
class CGameAchievements;
class CWorldBuilder;

#if !defined(_RELEASE)
class CGameRealtimeRemoteUpdateListener;
#endif

class CGameConnectionTracker;

class CStatsEntityIdRegistry;

class CMovingPlatformMgr;
class CGamePhysicsSettings;

namespace Graphics
{
class CColorGradientManager;
//	class CScreenFader;
//	class CPostEffectBlender;
}

enum AsyncState
{
	AsyncFailed = 0,
	AsyncReady,
	AsyncInProgress,
	AsyncComplete,
};

// when you add stuff here, also update in CGame::RegisterGameObjectEvents
enum ECryGameEvent
{
	eCGE_OnShoot = eGFE_Last,
	eCGE_ActorRevive,
	eCGE_VehicleDestroyed,
	eCGE_VehicleTransitionEnter,
	eCGE_VehicleTransitionExit,
	eCGE_HUD_PDAMessage,
	eCGE_HUD_TextMessage,
	eCGE_TextArea,
	eCGE_HUD_Break,
	eCGE_HUD_Reboot,
	eCGE_InitiateAutoDestruction,
	eCGE_Event_Collapsing,
	eCGE_Event_Collapsed,
	eCGE_MultiplayerChatMessage,
	eCGE_ResetMovementController,
	eCGE_AnimateHands,
	eCGE_EnablePhysicalCollider,
	eCGE_DisablePhysicalCollider,
	eCGE_Turret_LockedTarget,
	eCGE_Turret_LostTarget,
	eCGE_SetTeam,
	eCGE_Launch,
	eCGE_ReactionEnd,
	eCGE_CoverTransitionEnter,
	eCGE_CoverTransitionExit,
	eCGE_AllowStartTransitionEnter,
	eCGE_AllowStartTransitionExit,
	eCGE_AllowStopTransitionEnter,
	eCGE_AllowStopTransitionExit,
	eCGE_AllowDirectionChangeTransitionEnter,
	eCGE_AllowDirectionChangeTransitionExit,
	eCGE_Ragdollize,
	eCGE_ItemTakenFromCorpse,

	eCGE_Last
};

//! Difficulty levels
enum EDifficulty
{
	eDifficulty_Default = 0,

	eDifficulty_Easy,
	eDifficulty_Normal,
	eDifficulty_Hard,
	eDifficulty_Delta,
	eDifficulty_PostHuman,

	eDifficulty_COUNT,
};

//! Controller layout types
enum EControllerLayout
{
	eControllerLayout_Button = 0,
	eControllerLayout_Stick,
};

static const int GLOBAL_SERVER_IP_KEY          = 1000;
static const int GLOBAL_SERVER_PUBLIC_PORT_KEY = 1001;
static const int GLOBAL_SERVER_NAME_KEY        = 1002;

//invites are session based
#define CryInviteID      CrySessionID
#define CryInvalidInvite CrySessionInvalidID

class CCountdownTimer
{
public:
	typedef void (* FN_COMPLETION_CALLBACK)();

private:
	float                  m_timeLeft;
	FN_COMPLETION_CALLBACK m_fnCompleted;

public:
	CCountdownTimer(FN_COMPLETION_CALLBACK fnComplete);
	void  Start(float countdownDurationSeconds);
	void  Advance(float dt);
	bool  IsActive();
	void  Abort();
	float ToSeconds();
};

class CGame :
	public IGame, public IGameFrameworkListener, public IPlatformOS::IPlatformListener, public IInputEventListener, public ISystemEventListener
{
public:
	typedef bool (*                   BlockingConditionFunction)();

	typedef RayCastQueue<41>          GlobalRayCaster;
	typedef IntersectionTestQueue<43> GlobalIntersectionTester;

	enum EHostMigrationState
	{
		eHMS_NotMigrating,
		eHMS_WaitingForPlayers,
		eHMS_Resuming,
	};

	enum ERichPresenceState
	{
		eRPS_none,
		eRPS_idle,
		eRPS_frontend,
		eRPS_lobby,
		eRPS_inGame
	};

	enum ERichPresenceType
	{
		eRPT_String = 0,
		eRPT_Param1,
		eRPT_Param2,
		eRPT_Max,
	};

	enum EInviteAcceptedState
	{
		eIAS_None = 0,
		eIAS_Init,                    // initialisation
		eIAS_StartAcceptInvite,       // begin the process
		eIAS_InitProfile,             // progress to profile loading screen, user might not have created profile yet
		eIAS_WaitForInitProfile,      // wait for profile creation to be finished
		eIAS_WaitForLoadToFinish,     // waiting for loading to finish
		eIAS_DisconnectGame,          // disconnect user from game
		eIAS_DisconnectLobby,         // disconnect user from lobby session
		eIAS_WaitForSessionDelete,    // waiting for game session to be deleted
		eIAS_ConfirmInvite,
		eIAS_WaitForInviteConfirmation,
		eIAS_InitSinglePlayer,
		eIAS_WaitForInitSinglePlayer,
		eIAS_WaitForSplashScreen,     // return user to splash screen
		eIAS_WaitForValidUser,        // need user to select controller
		eIAS_InitMultiplayer,         // init the multiplayer gamemode
		eIAS_WaitForInitMultiplayer,  // wait for multiplayer game mode to be initialised
		eIAS_InitOnline,              // init online functionality
		eIAS_WaitForInitOnline,       // wait for online mode to be initialised
		eIAS_WaitForSquadManagerEnabled,
		eIAS_Accept,                  // accept the invite
		eIAS_Error,                   // we recieved an invite that had an error attached to it
	};

	enum ESaveIconMode
	{
		eSIM_Off,
		eSIM_SaveStart,
		eSIM_Saving,
		eSIM_Finished
	};

	struct IRenderSceneListener
	{
		virtual ~IRenderSceneListener(){}
		virtual void OnRenderScene(const SRenderingPassInfo& passInfo) = 0;
	};

	typedef std::vector<IRenderSceneListener*> TRenderSceneListeners;

public:
	CGame();
	virtual ~CGame();

	// IGame
	virtual bool            Init();
	virtual bool            CompleteInit();
	virtual void            Shutdown();
	virtual int             Update(bool haveFocus, unsigned int updateFlags);
	virtual void            EditorResetGame(bool bStart);
	virtual bool            IsReloading()       { return m_bReload; }
	virtual IGameFramework* GetIGameFramework() { return gEnv->pGameFramework; }

	virtual const char*     GetLongName();
	virtual const char*     GetName();
	virtual EPlatform       GetPlatform() const;

	virtual void            UploadSessionTelemetry(void);
	virtual void            ClearSessionTelemetry(void);
	virtual void            GetMemoryStatistics(ICrySizer* s);

	//level names were renamed without changing the file/directory
	virtual const char*     GetMappedLevelName(const char* levelName) const;

	void                    LoadMappedLevelNames(const char* xmlPath);

	void                    LoadPatchLocalizationData();

	//
	virtual IGameStateRecorder*    CreateGameStateRecorder(IGameplayListener* pL);

	virtual THUDWarningId          AddGameWarning(const char* stringId, const char* paramMessage, IGameWarningsListener* pListener = NULL);
	virtual void                   RemoveGameWarning(const char* stringId);
	virtual void                   RenderGameWarnings();

	virtual void                   OnRenderScene(const SRenderingPassInfo& passInfo);

	virtual bool                   GameEndLevel(const char* stringId);
	void                           SetUserProfileChanged(bool yesNo) { m_userProfileChanged = yesNo; }

	virtual IGame::ExportFilesInfo ExportLevelData(const char* levelName, const char* missionName) const;
	virtual void                   LoadExportedLevelData(const char* levelName, const char* missionName);

	// ~IGame

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime);
	virtual void OnSaveGame(ISaveGame* pSaveGame);
	virtual void OnLoadGame(ILoadGame* pLoadGame);
	virtual void OnLevelEnd(const char* nextLevel);
	;
	virtual void OnActionEvent(const SActionEvent& event);
	virtual void OnSavegameFileLoadedInMemory(const char* pLevelName);
	virtual void OnForceLoadingWithFlash() {}
	// ~IGameFrameworkListener

	void PreSerialize();
	void FullSerialize(TSerialize serializer);
	void PostSerialize();

	void OnEditorGameInitComplete();
	void OnBeforeEditorLevelLoad(); //Called when loading a new level in the editor

	int  GetDifficultyForTelemetry(int difficulty = -1) const;
	int  GetCheckpointIDForTelemetry(const char* checkpointName) const;

	void OnExitGameSession();

	// IInputEventListener
	bool OnInputEvent(const SInputEvent& inputEvent);
	bool OnInputEventUI(const SUnicodeEvent& inputEvent);
	// ~IInputEventListener

	void         SetInputEventListenerOverride(IInputEventListener* pListenerOverride) { m_pInputEventListenerOverride = pListenerOverride; }

	void         AddRenderSceneListener(IRenderSceneListener* pListener);
	void         RemoveRenderSceneListener(IRenderSceneListener* pListener);

	void         SetExclusiveControllerFromPreviousInput();
	unsigned int GetExclusiveControllerDeviceIndex() const; // Gets current UserIndex
	void         SetPreviousExclusiveControllerDeviceIndex(unsigned int idx) { m_previousInputControllerDeviceIndex = idx; }
	void         RemoveExclusiveController();
	bool         HasExclusiveControllerIndex() const                         { return m_hasExclusiveController; }
	bool         IsExclusiveControllerConnected() const                      { return m_bExclusiveControllerConnected; }
	bool         IsPausedForControllerDisconnect() const                     { return m_bPausedForControllerDisconnect; }
	bool         SetControllerLayouts(const char* szButtonLayoutName, const char* szStickLayoutName, bool bUpdateProfileData = true);
	const char*  GetControllerLayout(const EControllerLayout layoutType) const;

	// IPlatformOS::IPlatformListener
	virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& event);
	// ~IPlatformOS::IPlatformListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	virtual void* GetGameInterface()                                { return NULL; }
	// Init editor related things
	virtual void  InitEditor(IGameToEditorInterface* pGameToEditor) {}

	static void   SetRichPresenceCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

	bool          SetRichPresence(ERichPresenceState state);
	void          InitRichPresence();
	void          AddRichPresence(const char* path);
	CrySessionID  GetPendingRichPresenceSessionID() { return m_pendingRichPresenceSessionID; }

	void          LoginUser(unsigned int user);

	enum ELogoutReason
	{
		eLR_SetExclusiveController,
		eLR_RemoveExclusiveController,
		eLR_StorageDeviceChange,
	};

	void ReloadPlayerParamFiles();
	void LogoutCurrentUser(ELogoutReason reason);
	void ResetSignInOrOutEventOccured()  { m_bSignInOrOutEventOccured = false; }
	bool SignInOrOutEventOccured() const { return m_bSignInOrOutEventOccured; }

	void InitGameType(bool multiplayer, bool fromInit = false);
	void InitPatchableModules(bool inIsMultiplayer);
	bool IsGameTypeFullyInitialised() const { return !m_needsInitPatchables; }

	//! Handle difficulty setting changes
	void                            SetDifficultyLevel(EDifficulty difficulty);

	void                            GameChannelDestroyed(bool isServer);
	void                            DestroyHUD() {};
	void                            OnDeathReloadComplete();

	bool                            IsGameActive() const;

	CScriptBind_HitDeathReactions*  GetHitDeathReactionsScriptBind() { return m_pScriptBindHitDeathReactions; }
	CScriptBind_Actor*              GetActorScriptBind()             { return m_pScriptBindActor; }
	CScriptBind_Item*               GetItemScriptBind()              { return m_pScriptBindItem; }
	CScriptBind_Weapon*             GetWeaponScriptBind()            { return m_pScriptBindWeapon; }
	CScriptBind_GameRules*          GetGameRulesScriptBind()         { return m_pScriptBindGameRules; }
	CScriptBind_HUD*                GetHUDScriptBind()               { return m_pScriptBindHUD; }
	CScriptBind_InteractiveObject*  GetInteractiveObjectScriptBind() { return m_pScriptBindInteractiveObject; }
	CScriptBind_MatchMaking*        GetMatchMakingScriptBind()       { return m_pScriptBindMatchMaking; }
	CScriptBind_Turret*             GetTurretScriptBind()            { return m_pScriptBindTurret; }
	CScriptBind_LightningArc*       GetLightningArcScriptBind()      { return m_pScriptBindLightningArc; }
	CWeaponSystem*                  GetWeaponSystem()                { return m_pWeaponSystem; };
	ILINE CGamePhysicsSettings*     GetGamePhysicsSettings()         { return m_pGamePhysicsSettings; }
	CGameSharedParametersStorage*   GetGameSharedParametersStorage() { return m_pGameParametersStorage; };
	CRecordingSystem*               GetRecordingSystem()             { return m_pRecordingSystem; };
	CStatsRecordingMgr*             GetStatsRecorder()               { return m_statsRecorder; }
	CPatchPakManager*               GetPatchPakManager()             { return m_patchPakManager; }
	CMatchmakingTelemetry*          GetMatchMakingTelemetry() const  { return m_pMatchMakingTelemetry; }
	CDataPatchDownloader*           GetDataPatchDownloader()         { return m_pDataPatchDownloader; }
	CGameLocalizationManager*       GetGameLocalizationManager()     { return m_pGameLocalizationManager; }
#if USE_LAGOMETER
	CLagOMeter*                     GetLagOMeter()                   { return m_pLagOMeter; }
#endif
	ITelemetryCollector*            GetITelemetryCollector()         { return m_telemetryCollector; }
	CPlaylistActivityTracker*       GetPlaylistActivityTracker()     { return m_pPlaylistActivityTracker; }

	ILINE CGameInputActionHandlers& GetGameInputActionHandlers()     { CRY_ASSERT(m_pGameActionHandlers); return *m_pGameActionHandlers; }

	CLedgeManager*                  GetLedgeManager() const          { return m_pLedgeManager; };
	CWaterPuddleManager*            GetWaterPuddleManager() const    { return m_pWaterPuddleManager; }

	CGameActions&                   Actions() const                  { return *m_pGameActions; };

	CGameRules*                     GetGameRules() const;
	CHUDMissionObjectiveSystem*     GetMOSystem() const;

#ifdef USE_LAPTOPUTIL
	CLaptopUtil* GetLaptopUtil() const;
#endif

	virtual CGameAudio*               GetGameAudio() const { return m_pGameAudio; }

	bool                              IsLevelLoaded() const;

	CProfileOptions*                  GetProfileOptions() const;
	CPlayerVisTable*                  GetPlayerVisTable()                  { return m_pPlayerVisTable; }
	CPersistantStats*                 GetPersistantStats() const           { return m_pPersistantStats; }

	ILINE CHudInterferenceGameEffect* GetHudInterferenceGameEffect()       { return m_pHudInterferenceGameEffect; }
	ILINE CSceneBlurGameEffect*       GetSceneBlurGameEffect()             { return m_pSceneBlurGameEffect; }
	ILINE CLightningGameEffect*       GetLightningGameEffect()             { return m_pLightningGameEffect; }
	ILINE CParameterGameEffect*       GetParameterGameEffect()             { return m_pParameterGameEffect; }
	ILINE CScreenEffects*             GetScreenEffects()    const          { return m_pScreenEffects; }
	ILINE CAutoAimManager&            GetAutoAimManager()  const           { return *m_pAutoAimManager; }
	ILINE CHitDeathReactionsSystem&   GetHitDeathReactionsSystem() const   { CRY_ASSERT(m_pHitDeathReactionsSystem); return *m_pHitDeathReactionsSystem; }
	CInteractiveObjectRegistry&       GetInteractiveObjectsRegistry() const;
	ILINE CMovementTransitionsSystem& GetMovementTransitionsSystem() const { CRY_ASSERT(m_pMovementTransitionsSystem); return *m_pMovementTransitionsSystem; }

	CBodyDamageManager*               GetBodyDamageManager()               { return m_pBodyDamageManager; }
	const CBodyDamageManager*         GetBodyDamageManager() const         { return m_pBodyDamageManager; }

	CBurnEffectManager*               GetBurnEffectManager() const         { return m_pBurnEffectManager; }
	CPlaylistManager*                 GetPlaylistManager()                 { return m_pPlaylistManager; }
	ILINE CGameAchievements*          GetGameAchievements() const          { return m_pGameAchievements; }

	CStatsEntityIdRegistry*           GetStatsEntityIdRegistry()           { return m_pStatsEntityIdRegistry; }

	ILINE CMovingPlatformMgr*         GetMovingPlatformManager()           { return m_pMovingPlatformMgr; }

	const string&             GetLastSaveGame(string& levelName);
	const string&             GetLastSaveGame() { string tmp; return GetLastSaveGame(tmp); }

	ILINE SCVars*             GetCVars()        { return m_pCVars; }
	static void               DumpMemInfo(const char* format, ...) PRINTF_PARAMS(1, 2);

	CEquipmentLoadout*        GetEquipmentLoadout() const { return m_pEquipmentLoadout; }
	CGameAISystem*            GetGameAISystem()           { return m_pGameAISystem; }
	GlobalRayCaster&          GetRayCaster()              { assert(m_pRayCaster); return *m_pRayCaster; }
	GlobalIntersectionTester& GetIntersectionTester()     { assert(m_pIntersectionTester); return *m_pIntersectionTester; }
	CGameLobby*               GetGameLobby();
	CGameLobbyManager*        GetGameLobbyManager()       { return m_pGameLobbyManager; }

#if IMPLEMENT_PC_BLADES
	CGameServerLists*        GetGameServerLists()           { return m_pGameServerLists; }
#endif //IMPLEMENT_PC_BLADES
	CGameBrowser*            GetGameBrowser()               { return m_pGameBrowser; }
	CSquadManager*           GetSquadManager()              { return m_pSquadManager; }
	void                     ClearGameSessionHandler();
	CDownloadMgr*            GetDownloadMgr()               { return m_pDownloadMgr; }
	CDLCManager*             GetDLCManager()                { return m_pDLCManager; }
	CWorldBuilder*           GetWorldBuilder()              { return m_pWorldBuilder; }

	CGameCache&              GetGameCache()                 { CRY_ASSERT_MESSAGE(m_pGameCache, "Can't obtain GameCache object until CGame::Init() is called!"); return *m_pGameCache; }
	const CGameCache&        GetGameCache() const           { CRY_ASSERT_MESSAGE(m_pGameCache, "Can't obtain GameCache object until CGame::Init() is called!"); return *m_pGameCache; }

	ILINE CUIManager*        GetUI(void)                    { return m_pUIManager; }
	ILINE CTacticalManager*  GetTacticalManager(void) const { return m_pTacticalManager; }
	CWarningsManager*        GetWarnings(void) const;

	static void              ExpandTimeSeconds(int secs, int& days, int& hours, int& minutes, int& seconds);

	int                      GetCurrentXboxLivePartySize() { return m_currentXboxLivePartySize; }

	float                    GetTimeSinceHostMigrationStateChanged() const;
	float                    GetRemainingHostMigrationTimeoutTime() const;
	float                    GetHostMigrationTimeTillResume() const;

	EHostMigrationState      GetHostMigrationState() const      { return m_hostMigrationState; }
	ILINE bool               IsGameSessionHostMigrating() const { return m_hostMigrationState != eHMS_NotMigrating; }

	void                     SetHostMigrationState(EHostMigrationState newState);
	void                     SetHostMigrationStateAndTime(EHostMigrationState newState, float timeOfChange);

	void                     AbortHostMigration();

	bool                     GetUserProfileChanged() const { return m_userProfileChanged; }
	bool                     GameDataInstalled() const     { return m_gameDataInstalled; }
	bool                     BootChecksComplete() const    { return m_postLocalisationBootChecksDone; }
	void                     SetUserRegion(int region)     { m_cachedUserRegion = region; }
	int                      GetUserRegion(void) const     { return m_cachedUserRegion; }

	uint32                   GetRandomNumber();
	float                    GetRandomFloat();

	void                     SetInviteAcceptedState(EInviteAcceptedState state);
	EInviteAcceptedState     GetInviteAcceptedState() { return m_inviteAcceptedState; }
	void                     SetInviteData(ECryLobbyService service, uint32 user, CryInviteID id, ECryLobbyError error, ECryLobbyInviteType inviteType);
	void                     InvalidateInviteData();
	void                     UpdateInviteAcceptedState();
	void                     SetInviteUserFromPreviousControllerIndex();
	const int                GetInviteUser() const      { return m_inviteAcceptedData.m_user; }
	const bool               IsInviteInProgress() const { return m_inviteAcceptedState != eIAS_None; }

	bool                     LoadLastSave();
	int                      GetCachedGsmValue()          { return m_iCachedGsmValue; }
	float                    GetCachedGsmRangeValue()     { return m_fCachedGsmRangeValue; }
	float                    GetCachedGsmRangeStepValue() { return m_fCachedGsmRangeStepValue; }

	void                     SetCrashDebugMessage(const char* const msg);
	void                     AppendCrashDebugMessage(const char* const msg);

	CRevertibleConfigLoader& GetGameModeCVars();

	void                     OnDedicatedConfigEntry(const char* szKey, const char* szValue);

#if ENABLE_VISUAL_DEBUG_PROTOTYPE
	void UpdateVisualDebug(float deltaT);
#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE

	virtual void LoadActionMaps(const char* filename = "libs/config/defaultProfile.xml");

	void         QueueDeferredKill(const EntityId entityId);

	void         OnEditorDisplayRenderUpdated(bool displayHelpers) { m_editorDisplayHelpers = displayHelpers; }
	bool         DisplayEditorHelpersEnabled() const               { return m_editorDisplayHelpers; }

	void         SetRenderingToHMD(bool bRenderingToHMD)           { m_RenderingToHMD = bRenderingToHMD; }
	bool         IsRenderingToHMD() const                          { return m_RenderingToHMD; }

	float        GetFOV() const;
	float        GetPowerSprintTargetFov() const;

#if CRY_PLATFORM_DURANGO
	const GUID* GetPlayerSessionId() const { return &m_playerSessionId; }
#endif

	void RegisterPhysicsCallbacks();
	void UnregisterPhysicsCallbacks();

protected:

	//! Difficulty config loading helper
	class CDifficultyConfigSink : public ILoadConfigurationEntrySink
	{
	public:
		CDifficultyConfigSink(const char* who) : m_szWho(who) {}

		void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
		{
			ICVar* pCVar = gEnv->pConsole->GetCVar(szKey);
			if (pCVar)
			{
				pCVar->ForceSet(szValue);
			}
			else
			{
				GameWarning("%s : Can only set existing CVars during loading (no commands!) (%s = %s)",
				            m_szWho ? m_szWho : "CDifficultyConfigSink", szKey, szValue);
			}
		}

	private:
		const char* m_szWho;
	};
	// matchmaking block & version config loading helper
	class CMPConfigSink : public ILoadConfigurationEntrySink
	{
	public:
		CMPConfigSink() {}

		void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup);
	};

	struct SDedicatedConfigSink : public ILoadConfigurationEntrySink
	{
		virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup);
	};

	//! Platform information as defined in defaultProfile.xml
	struct SPlatformInfo
	{
		EPlatform platformId;
		BYTE      devices;      // Devices to use when registering actions

		SPlatformInfo(EPlatform _platformId = EPlatform::Current) : platformId(_platformId), devices(eAID_KeyboardMouse | eAID_XboxPad | eAID_PS4Pad) {}
	};
	SPlatformInfo m_platformInfo;

	void         InitPlatformOS();

	void         InitScriptBinds();
	void         ReleaseScriptBinds();

	void         UpdateSaveIcon();

	virtual void CheckReloadLevel();

	// These funcs live in GameCVars.cpp
	virtual void RegisterConsoleVars();
	virtual void RegisterConsoleCommands();
	virtual void UnregisterConsoleCommands();

	virtual void RegisterGameObjectEvents();

	// marcok: this is bad and evil ... should be removed soon
	static void CmdRestartGame(IConsoleCmdArgs* pArgs);
	static void CmdDumpAmmoPoolStats(IConsoleCmdArgs* pArgs);

	static void CmdLastInv(IConsoleCmdArgs* pArgs);
	static void CmdName(IConsoleCmdArgs* pArgs);
	static void CmdTeam(IConsoleCmdArgs* pArgs);
	static void CmdLoadLastSave(IConsoleCmdArgs* pArgs);
	static void CmdSpectator(IConsoleCmdArgs* pArgs);
	static void CmdJoinGame(IConsoleCmdArgs* pArgs);
	static void CmdKill(IConsoleCmdArgs* pArgs);
	static void CmdTakeDamage(IConsoleCmdArgs* pArgs);
	static void CmdRevive(IConsoleCmdArgs* pArgs);
	static void CmdVehicleKill(IConsoleCmdArgs* pArgs);
	static void CmdRestart(IConsoleCmdArgs* pArgs);
	static void CmdSay(IConsoleCmdArgs* pArgs);
	static void CmdEcho(IConsoleCmdArgs* pArgs);
	static void CmdReloadItems(IConsoleCmdArgs* pArgs);
	static void CmdLoadActionmap(IConsoleCmdArgs* pArgs);
	static void CmdReloadGameRules(IConsoleCmdArgs* pArgs);
	static void CmdNextLevel(IConsoleCmdArgs* pArgs);
	static void CmdReloadHitDeathReactions(IConsoleCmdArgs* pArgs);
	static void CmdDumpHitDeathReactionsAssetUsage(IConsoleCmdArgs* pArgs);
	static void CmdReloadSpectacularKill(IConsoleCmdArgs* pArgs);
	static void CmdReloadImpulseHandler(IConsoleCmdArgs* pArgs);
	static void CmdReloadPickAndThrowProxies(IConsoleCmdArgs* pArgs);
	static void CmdReloadMovementTransitions(IConsoleCmdArgs* pArgs);
	static void CmdReloadSkillSystem(IConsoleCmdArgs* pArgs);
	static void CmdStartKickVoting(IConsoleCmdArgs* pArgs);
	static void CmdVote(IConsoleCmdArgs* pArgs);

	static void CmdListAllRandomLoadingMessages(IConsoleCmdArgs* pArgs);

	static void CmdFreeCamEnable(IConsoleCmdArgs* pArgs);
	static void CmdFreeCamDisable(IConsoleCmdArgs* pArgs);
	static void CmdFreeCamLockCamera(IConsoleCmdArgs* pArgs);
	static void CmdFreeCamUnlockCamera(IConsoleCmdArgs* pArgs);
	static void CmdFlyCamSetPoint(IConsoleCmdArgs* pArgs);
	static void CmdFlyCamPlay(IConsoleCmdArgs* pArgs);

#if defined(USE_CRY_ASSERT)
	static void CmdIgnoreAllAsserts(IConsoleCmdArgs* pArgs);
#endif

	static void CmdReloadPlayer(IConsoleCmdArgs* cmdArgs);

	static void CmdSetPlayerHealth(IConsoleCmdArgs* pArgs);
	static void CmdSwitchGameMultiplayer(IConsoleCmdArgs* pArgs);
	static void CmdSpawnActor(IConsoleCmdArgs* pArgs);
	static void CmdReportLag(IConsoleCmdArgs* pArgs);

	static void CmdReloadGameFx(IConsoleCmdArgs* pArgs);

#if ENABLE_CONSOLE_GAME_DEBUG_COMMANDS
	static void CmdGiveGameAchievement(IConsoleCmdArgs* pArgs);
#endif

#if (USE_DEDICATED_INPUT)
	static void CmdSpawnDummyPlayer(IConsoleCmdArgs* cmdArgs);
	static void CmdRemoveDummyPlayers(IConsoleCmdArgs* cmdArgs);
	static void CmdSetDummyPlayerState(IConsoleCmdArgs* cmdArgs);
	static void CmdHideAllDummyPlayers(IConsoleCmdArgs* pCmdArgs);
#endif

#if CRY_PLATFORM_DURANGO
	static void CmdInspectConnectedStorage(IConsoleCmdArgs* pCmdArgs);
	void        EnsureSigninState();
#endif

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	// TODO: Move more console commands which shouldn't feature in final release into this block...
	static void CmdSendConsoleCommand(IConsoleCmdArgs* pArgs);
	static void CmdNetSetOnlineMode(IConsoleCmdArgs* pArgs);
#endif

	static void OnHostMigrationNetTimeoutChanged(ICVar* pVar);
	static void VerifyMaxPlayers(ICVar* pVar);

	static void PartyMembersCallback(UCryLobbyEventData eventData, void* userParam);
	static void UserProfileChangedCallback(UCryLobbyEventData eventData, void* arg);
	static void InviteAcceptedCallback(UCryLobbyEventData eventData, void* arg);
	static void OnlineStateCallback(UCryLobbyEventData eventData, void* arg);
	static void EthernetStateCallback(UCryLobbyEventData eventData, void* arg);
	static void GetTelemetryTimers(int& careerTime, int& gameTime, int& sessionTime, void* pArg);

	void        CommitDeferredKills();

	void        AddPersistentAccessories();

	CGameCache*               m_pGameCache;
	CGameInputActionHandlers* m_pGameActionHandlers;
	CRndGen                   m_randomGenerator;

	CGameTokenSignalCreator*  m_pGameTokenSignalCreator;

	//IGameFramework			*m_pFramework;
	IConsole*             m_pConsole;

	CWeaponSystem*        m_pWeaponSystem;
	CGamePhysicsSettings* m_pGamePhysicsSettings;

	bool                  m_bReload;
	bool                  m_gameTypeMultiplayer;
	bool                  m_gameTypeInitialized;
	bool                  m_userProfileChanged;
	bool                  m_bLastSaveDirty;
	bool                  m_needsInitPatchables;
	bool                  m_editorDisplayHelpers;
	bool                  m_RenderingToHMD;

	// script binds
	CScriptBind_Actor*             m_pScriptBindActor;
	CScriptBind_Item*              m_pScriptBindItem;
	CScriptBind_Weapon*            m_pScriptBindWeapon;
	CScriptBind_GameRules*         m_pScriptBindGameRules;
	CScriptBind_Game*              m_pScriptBindGame;
	CScriptBind_GameAI*            m_pScriptBindGameAI;
	CScriptBind_HUD*               m_pScriptBindHUD;
	CScriptBind_InteractiveObject* m_pScriptBindInteractiveObject;
	CScriptBind_HitDeathReactions* m_pScriptBindHitDeathReactions;
	CScriptBind_Boids*             m_pScriptBindBoids;
	CScriptBind_MatchMaking*       m_pScriptBindMatchMaking;
	CScriptBind_Turret*            m_pScriptBindTurret;
	CScriptBind_ProtectedBinds*    m_pScriptBindProtected;
	CScriptBind_LightningArc*      m_pScriptBindLightningArc;

	//vis table
	CPlayerVisTable*          m_pPlayerVisTable;

	CRecordingSystem*         m_pRecordingSystem;
	CStatsRecordingMgr*       m_statsRecorder;
	CPatchPakManager*         m_patchPakManager;
	CMatchmakingTelemetry*    m_pMatchMakingTelemetry;
	CDataPatchDownloader*     m_pDataPatchDownloader;
	CGameLocalizationManager* m_pGameLocalizationManager;
	CGameStateRecorder*       m_pGameStateRecorder;
#if USE_LAGOMETER
	CLagOMeter*               m_pLagOMeter;
#endif
	ITelemetryCollector*      m_telemetryCollector;
	CPlaylistActivityTracker* m_pPlaylistActivityTracker;
#if USE_TELEMETRY_BUFFERS
	CTelemetryBuffer*         m_performanceBuffer;
	CTelemetryBuffer*         m_bandwidthBuffer;
	CTelemetryBuffer*         m_memoryTrackingBuffer;
	CTelemetryBuffer*         m_soundTrackingBuffer;
	CTimeValue                m_secondTimePerformance;
	CTimeValue                m_secondTimeMemory;
	CTimeValue                m_secondTimeBandwidth;
	CTimeValue                m_secondTimeSound;
#endif //#if USE_TELEMETRY_BUFFERS

	CGameActions*                 m_pGameActions;
	IPlayerProfileManager*        m_pPlayerProfileManager;

	CPersistantStats*             m_pPersistantStats;
	bool                          m_inDevMode;

	bool                          m_hasExclusiveController;
	bool                          m_bExclusiveControllerConnected;
	bool                          m_bPausedForControllerDisconnect;
	bool                          m_bPausedForSystemMenu;
	bool                          m_bDeferredSystemMenuPause;
	bool                          m_previousPausedGameState;
	bool                          m_wasGamePausedBeforePLMForcedPause;
	unsigned int                  m_previousInputControllerDeviceIndex;
	int                           m_currentXboxLivePartySize;

	SCVars*                       m_pCVars;
	SItemStrings*                 m_pItemStrings;
	CGameSharedParametersStorage* m_pGameParametersStorage;
	string                        m_lastSaveGame;

#ifdef USE_LAPTOPUTIL
	CLaptopUtil*                m_pLaptopUtil;
#endif
	CScreenEffects*             m_pScreenEffects;
	CDownloadMgr*               m_pDownloadMgr;
	CDLCManager*                m_pDLCManager;
	CHudInterferenceGameEffect* m_pHudInterferenceGameEffect;
	CSceneBlurGameEffect*       m_pSceneBlurGameEffect;
	CLightningGameEffect*       m_pLightningGameEffect;
	CParameterGameEffect*       m_pParameterGameEffect;

	CWorldBuilder*              m_pWorldBuilder;

	IInputEventListener*        m_pInputEventListenerOverride;

	typedef std::map<string, string, stl::less_stricmp<string>> TLevelMapMap;
	TLevelMapMap m_mapNames;

public:
	typedef std::map<string, string, stl::less_stricmp<string>> TStringStringMap;
	TStringStringMap* GetVariantOptions() { return &m_variantOptions; }

private:
	static int OnCreatePhysicalEntityLogged(const EventPhys* pEvent);

	TStringStringMap m_variantOptions;

	typedef std::map<CryFixedStringT<128>, int> TRichPresenceMap;
	TRichPresenceMap      m_richPresence;

	CGameAudio*           m_pGameAudio;

	TRenderSceneListeners m_renderSceneListeners;

	// Manager the ledges in the level (markup) that the player can grab onto
	CLedgeManager*       m_pLedgeManager;
	CWaterPuddleManager* m_pWaterPuddleManager;

	// Game side browser - searching for games
	CGameBrowser*            m_pGameBrowser;
	// Game side Lobby handler
	CGameLobbyManager*       m_pGameLobbyManager;
	// Game side session handler implementation
	CCryLobbySessionHandler* m_pLobbySessionHandler;
	//squad session handler
	CSquadManager*           m_pSquadManager;

#if IMPLEMENT_PC_BLADES
	CGameServerLists* m_pGameServerLists;
#endif // ~IMPLEMENT_PC_BLADES

	CEquipmentLoadout*        m_pEquipmentLoadout;
	CPlayerProgression*       m_pPlayerProgression;
	CGameAISystem*            m_pGameAISystem;

	GlobalRayCaster*          m_pRayCaster;
	GlobalIntersectionTester* m_pIntersectionTester;

	// Game side HUD, only valid when client,
	// only functions after player joins.
	CUIManager*                 m_pUIManager;
	CTacticalManager*           m_pTacticalManager;
	CAutoAimManager*            m_pAutoAimManager;
	CHitDeathReactionsSystem*   m_pHitDeathReactionsSystem;
	CBodyDamageManager*         m_pBodyDamageManager;
	CMovementTransitionsSystem* m_pMovementTransitionsSystem;
	CBurnEffectManager*         m_pBurnEffectManager;
	CGameMechanismManager*      m_gameMechanismManager;
	CGameAchievements*          m_pGameAchievements;

	CPlaylistManager*           m_pPlaylistManager;
	CStatsEntityIdRegistry*     m_pStatsEntityIdRegistry;

	CMovingPlatformMgr*         m_pMovingPlatformMgr;

#if !defined(_RELEASE)
	// Live Create support for things that must/should be updated in the game
	// on the Main Thread.
	CGameRealtimeRemoteUpdateListener* m_pRemoteUpdateListener;
#endif

	float               m_hostMigrationTimeStateChanged; // Time when the host migration started (from timer->GetAsyncCurTime())
	float               m_hostMigrationNetTimeoutLength;
	EHostMigrationState m_hostMigrationState;

	ERichPresenceState  m_desiredRichPresenceState;
	ERichPresenceState  m_pendingRichPresenceState;
	ERichPresenceState  m_currentRichPresenceState;

	CrySessionID        m_pendingRichPresenceSessionID;
	CrySessionID        m_currentRichPresenceSessionID;

#if CRY_PLATFORM_DURANGO
	GUID m_playerSessionId;
#endif

	float m_updateRichPresenceTimer;

	bool  m_settingRichPresence;
	bool  m_bRefreshRichPresence;
	bool  m_bSignInOrOutEventOccured;

	struct SInviteAcceptedData
	{
		ECryLobbyService    m_service;
		uint32              m_user;
		CryInviteID         m_id;
		ECryLobbyError      m_error;
		ECryLobbyInviteType m_type;
		bool                m_bannedFromSession;
		bool                m_failedToAcceptInviteAsNotSignedIn;
	}                    m_inviteAcceptedData;

	EInviteAcceptedState m_inviteAcceptedState;
	bool                 m_bLoggedInFromInvite;
	bool                 m_gameDataInstalled;
	bool                 m_postLocalisationBootChecksDone;

#if ENABLE_VISUAL_DEBUG_PROTOTYPE
	CVisualDebugPrototype* m_VisualDebugSys;
#endif // ENABLE_VISUAL_DEBUG_PROTOTYPE

	int        m_iCachedGsmValue;
	float      m_fCachedGsmRangeValue;
	float      m_fCachedGsmRangeStepValue;

	// Save icon
	ESaveIconMode m_saveIconMode;
	float         m_saveIconTimer;

	int           m_cachedUserRegion;
	bool          m_bUserHasPhysicalStorage;
	bool          m_bCheckPointSave;

	// Instead of killing off an actor from any point in the frame, we defer all
	// the kills and kill them all off from a single point in the frame.
	typedef std::vector<EntityId> DeferredKills;
	DeferredKills m_deferredKills;
#if CRY_PLATFORM_DURANGO
	bool          m_userChangedDoSignOutAndIn;
#endif

	uint64                   m_stereoOutputFunctorId;
};

extern CGame* g_pGame;

#define SAFE_HARDWARE_MOUSE_FUNC(func) \
  if (gEnv->pHardwareMouse)            \
    gEnv->pHardwareMouse->func

#ifdef USE_LAPTOPUTIL
	#define SAFE_LAPTOPUTIL_FUNC(func) \
	  { if (g_pGame && g_pGame->GetLaptopUtil()) g_pGame->GetLaptopUtil()->func; }

	#define SAFE_LAPTOPUTIL_FUNC_RET(func) \
	  ((g_pGame && g_pGame->GetLaptopUtil()) ? g_pGame->GetLaptopUtil()->func : NULL)
#endif

#define NOTIFY_UILOBBY_MP(fct) {                    \
  CUILobbyMP* pUIEvt = UIEvents::Get<CUILobbyMP>(); \
  if (pUIEvt) pUIEvt->fct; \
 }                        
