// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/ICmdLine.h>

#include <CryGame/IGameFramework.h>
#include <CrySystem/File/ICryPak.h>
#include "ISaveGame.h"

struct IFlowSystem;
struct IGameTokenSystem;
struct IEffectSystem;
struct IBreakableGlassSystem;
struct IForceFeedbackSystem;
struct IGameVolumes;

class CAIDebugRenderer;
class CAINetworkDebugRenderer;
class CGameRulesSystem;
class CScriptBind_Action;
class CScriptBind_ActorSystem;
class CScriptBind_ItemSystem;
class CScriptBind_ActionMapManager;
class CScriptBind_Network;
class CScriptBind_VehicleSystem;
class CScriptBind_Vehicle;
class CScriptBind_VehicleSeat;
class CScriptBind_Inventory;
class CScriptBind_DialogSystem;
class CScriptBind_MaterialEffects;
class CScriptBind_UIAction;

class CFlowSystem;
class CDevMode;
class CTimeDemoRecorder;
class CGameQueryListener;
class CScriptRMI;
class CGameSerialize;
class CMaterialEffects;
class CMaterialEffectsCVars;
class CGameObjectSystem;
class CActionMapManager;
class CActionGame;
class CActorSystem;
class CallbackTimer;
class CGameClientNub;
class CGameContext;
class CGameServerNub;
class CItemSystem;
class CLevelSystem;
class CUIDraw;
class CVehicleSystem;
class CViewSystem;
class CGameplayRecorder;
class CPersistantDebug;
class CPlayerProfileManager;
class CDialogSystem;
class CSubtitleManager;
class CGameplayAnalyst;
class CTimeOfDayScheduler;
class CNetworkCVars;
class CCryActionCVars;
class CGameStatsConfig;
class CSignalTimer;
class CRangeSignaling;
class CAIProxy;
class CommunicationVoiceLibrary;
class CCustomActionManager;
class CCustomEventManager;
class CAIProxyManager;
class CForceFeedBackSystem;
class CCryActionPhysicQueues;
class CNetworkStallTickerThread;
class CSharedParamsManager;
struct ICooperativeAnimationManager;
struct IGameSessionHandler;
class CRuntimeAreaManager;
class CColorGradientManager;

struct CAnimationGraphCVars;
struct IRealtimeRemoteUpdate;
struct ISerializeHelper;
struct ITimeDemoRecorder;

class CNetMessageDistpatcher;
class CEntityContainerMgr;
class CEntityAttachmentExNodeRegistry;

class CCryAction :
	public IGameFramework
{

public:
	CCryAction(SSystemInitParams& initParams);
	~CCryAction();

	// IGameFramework
	virtual void                          ShutDown();

	virtual void                          PreSystemUpdate();
	virtual bool                          PostSystemUpdate(bool hasFocus, CEnumFlags<ESystemUpdateFlags> updateFlags = CEnumFlags<ESystemUpdateFlags>());
	virtual void                          PreFinalizeCamera(CEnumFlags<ESystemUpdateFlags> updateFlags);
	virtual void                          PreRender();
	virtual void                          PostRender(CEnumFlags<ESystemUpdateFlags> updateFlags);
	virtual void                          PostRenderSubmit();

	void                                  ClearTimers();
	virtual TimerID                       AddTimer(CTimeValue interval, bool repeat, TimerCallback callback, void* userdata);
	virtual void*                         RemoveTimer(TimerID timerID);

	virtual uint32                        GetPreUpdateTicks();

	virtual void                          RegisterFactory(const char* name, IActorCreator* pCreator, bool isAI);
	virtual void                          RegisterFactory(const char* name, IItemCreator* pCreator, bool isAI);
	virtual void                          RegisterFactory(const char* name, IVehicleCreator* pCreator, bool isAI);
	virtual void                          RegisterFactory(const char* name, IGameObjectExtensionCreator* pCreator, bool isAI);
	virtual void                          RegisterFactory(const char* name, ISaveGame*(*func)(), bool);
	virtual void                          RegisterFactory(const char* name, ILoadGame*(*func)(), bool);

	virtual void                          InitGameType(bool multiplayer, bool fromInit);
	virtual bool                          CompleteInit();
	virtual void                          PrePhysicsUpdate() /*override*/;
	virtual void                          Reset(bool clients);
	virtual void                          GetMemoryUsage(ICrySizer* pSizer) const;

	virtual void                          PauseGame(bool pause, bool force, unsigned int nFadeOutInMS = 0);
	virtual bool                          IsGamePaused();
	virtual bool                          IsGameStarted();
	virtual bool                          IsInLevelLoad();
	virtual bool                          IsLoadingSaveGame();
	virtual const char*                   GetLevelName();
	virtual void                          GetAbsLevelPath(char* pPathBuffer, uint32 pathBufferSize);
	virtual bool                          IsInTimeDemo();        // Check if time demo is in progress (either playing or recording);
	virtual bool                          IsTimeDemoRecording(); // Check if time demo is recording;

	virtual ISystem*                      GetISystem()           { return m_pSystem; }
	virtual ILanQueryListener*            GetILanQueryListener() { return m_pLanQueryListener; }
	virtual IUIDraw*                      GetIUIDraw();
	virtual IMannequin&                   GetMannequinInterface();
	virtual ILevelSystem*                 GetILevelSystem();
	virtual IActorSystem*                 GetIActorSystem();
	virtual IItemSystem*                  GetIItemSystem();
	virtual IBreakReplicator*             GetIBreakReplicator();
	virtual IVehicleSystem*               GetIVehicleSystem();
	virtual IActionMapManager*            GetIActionMapManager();
	virtual IViewSystem*                  GetIViewSystem();
	virtual IGameplayRecorder*            GetIGameplayRecorder();
	virtual IGameRulesSystem*             GetIGameRulesSystem();
	virtual IGameObjectSystem*            GetIGameObjectSystem();
	virtual IFlowSystem*                  GetIFlowSystem();
	virtual IGameTokenSystem*             GetIGameTokenSystem();
	virtual IEffectSystem*                GetIEffectSystem();
	virtual IMaterialEffects*             GetIMaterialEffects();
	virtual IBreakableGlassSystem*        GetIBreakableGlassSystem();
	virtual IPlayerProfileManager*        GetIPlayerProfileManager();
	virtual ISubtitleManager*             GetISubtitleManager();
	virtual IDialogSystem*                GetIDialogSystem();
	virtual ICooperativeAnimationManager* GetICooperativeAnimationManager();
	virtual ICheckpointSystem*            GetICheckpointSystem();
	virtual IForceFeedbackSystem*         GetIForceFeedbackSystem() const;
	virtual ICustomActionManager*         GetICustomActionManager() const;
	virtual ICustomEventManager*          GetICustomEventManager() const;
	virtual IRealtimeRemoteUpdate*        GetIRealTimeRemoteUpdate();
	virtual ITimeDemoRecorder*            GetITimeDemoRecorder() const;

	virtual bool                          StartGameContext(const SGameStartParams* pGameStartParams);
	virtual bool                          ChangeGameContext(const SGameContextParams* pGameContextParams);
	virtual void                          EndGameContext();
	virtual bool                          StartedGameContext() const;
	virtual bool                          StartingGameContext() const;
	virtual bool                          BlockingSpawnPlayer();

	virtual void                          ReleaseGameStats();

	virtual void                          ResetBrokenGameObjects();
	virtual void                          CloneBrokenObjectsAndRevertToStateAtTime(int32 iFirstBreakEventIndex, uint16* pBreakEventIndices, int32& iNumBreakEvents, IRenderNode** outClonedNodes, int32& iNumClonedNodes, SRenderNodeCloneLookup& renderNodeLookup);
	virtual void                          ApplySingleProceduralBreakFromEventIndex(uint16 uBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup);
	virtual void                          UnhideBrokenObjectsByIndex(uint16* ObjectIndicies, int32 iNumObjectIndices);

	void                                  Serialize(TSerialize ser); // defined in ActionGame.cpp
	virtual void                          FlushBreakableObjects();   // defined in ActionGame.cpp
	void                                  ClearBreakHistory();

	IGameToEditorInterface*               GetIGameToEditor() { return m_pGameToEditor; }
	virtual void                          InitEditor(IGameToEditorInterface* pGameToEditor);
	virtual void                          SetEditorLevel(const char* levelName, const char* levelFolder);
	virtual void                          GetEditorLevel(char** levelName, char** levelFolder);

	virtual void                          BeginLanQuery();
	virtual void                          EndCurrentQuery();

	virtual IActor*                       GetClientActor() const;
	virtual EntityId                      GetClientActorId() const;
	virtual IEntity*                      GetClientEntity() const;
	virtual EntityId                      GetClientEntityId() const;
	virtual INetChannel*                  GetClientChannel() const;
	virtual CTimeValue                    GetServerTime();
	virtual uint16                        GetGameChannelId(INetChannel* pNetChannel);
	virtual INetChannel*                  GetNetChannel(uint16 channelId);
	virtual void                          SetServerChannelPlayerId(uint16 channelId, EntityId id);
	virtual const SEntitySchedulingProfiles* GetEntitySchedulerProfiles(IEntity* pEnt);
	virtual bool                          IsChannelOnHold(uint16 channelId);
	virtual IGameObject*                  GetGameObject(EntityId id);
	virtual bool                          GetNetworkSafeClassId(uint16& id, const char* className);
	virtual bool                          GetNetworkSafeClassName(char* className, size_t classNameSizeInBytes, uint16 id);
	virtual IGameObjectExtension*         QueryGameObjectExtension(EntityId id, const char* name);

	virtual INetContext*                  GetNetContext();

	virtual bool                          SaveGame(const char* path, bool bQuick = false, bool bForceImmediate = false, ESaveGameReason reason = eSGR_QuickSave, bool ignoreDelay = false, const char* checkpointName = NULL);
	virtual ELoadGameResult               LoadGame(const char* path, bool quick = false, bool ignoreDelay = false);
	virtual TSaveGameName                 CreateSaveGameName();

	virtual void                          ScheduleEndLevel(const char* nextLevel);
	virtual void                          ScheduleEndLevelNow(const char* nextLevel);

	virtual void                          OnEditorSetGameMode(int iMode);
	virtual bool                          IsEditing() { return m_isEditing; }

	virtual void                          OnBreakageSpawnedEntity(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity);

	bool                                  IsImmersiveMPEnabled();

	virtual void                          AllowSave(bool bAllow = true)
	{
		m_bAllowSave = bAllow;
	}

	virtual void AllowLoad(bool bAllow = true)
	{
		m_bAllowLoad = bAllow;
	}

	virtual bool                  CanSave();
	virtual bool                  CanLoad();

	virtual ISerializeHelper*     GetSerializeHelper() const;

	virtual bool                  CanCheat();

	virtual INetNub*              GetServerNetNub();
	virtual IGameServerNub*       GetIGameServerNub();
	virtual INetNub*              GetClientNetNub();
	virtual IGameClientNub*       GetIGameClientNub();

	void                          SetGameGUID(const char* gameGUID);
	const char*                   GetGameGUID()             { return m_gameGUID; }

	virtual bool                  IsVoiceRecordingEnabled() { return m_VoiceRecordingEnabled != 0; }

	virtual bool                  IsGameSession(CrySessionHandle sessionHandle);
	virtual bool                  ShouldMigrateNub(CrySessionHandle sessionHandle);

	virtual ISharedParamsManager* GetISharedParamsManager();

	virtual IGame*                GetIGame();
	virtual void* GetGameModuleHandle() const { return m_externalGameLibrary.dllHandle; }

	virtual float                 GetLoadSaveDelay() const { return m_lastSaveLoad; }

	virtual IGameVolumes*         GetIGameVolumesManager() const;

	virtual void                  PreloadAnimatedCharacter(IScriptTable* pEntityScript);
	virtual void                  PrePhysicsTimeStep(float deltaTime);

	virtual void                  RegisterExtension(ICryUnknownPtr pExtension);
	virtual void                  ReleaseExtensions();

	virtual void AddNetworkedClientListener(INetworkedClientListener& listener) { stl::push_back_unique(m_networkClientListeners, &listener); }
	virtual void RemoveNetworkedClientListener(INetworkedClientListener& listener) { stl::find_and_erase(m_networkClientListeners, &listener); }

	void DefineProtocolRMI(IProtocolBuilder* pBuilder);
	virtual void DoInvokeRMI(_smart_ptr<IRMIMessageBody> pBody, unsigned where, int channel, const bool isGameObjectRmi);

protected:
	virtual ICryUnknownPtr        QueryExtensionInterfaceById(const CryInterfaceID& interfaceID) const;
	// ~IGameFramework

public:

	static CCryAction*          GetCryAction() { return m_pThis; }

	virtual CGameServerNub*     GetGameServerNub();
	CGameClientNub*             GetGameClientNub();
	CGameContext*               GetGameContext();
	CScriptBind_Vehicle*        GetVehicleScriptBind()     { return m_pScriptBindVehicle; }
	CScriptBind_VehicleSeat*    GetVehicleSeatScriptBind() { return m_pScriptBindVehicleSeat; }
	CScriptBind_Inventory*      GetInventoryScriptBind()   { return m_pScriptInventory; }
	CPersistantDebug*           GetPersistantDebug()       { return m_pPersistantDebug; }
	CSignalTimer*               GetSignalTimer();
	CRangeSignaling*            GetRangeSignaling();
	virtual IPersistantDebug*   GetIPersistantDebug();
	virtual IGameStatsConfig*   GetIGameStatsConfig();
	CColorGradientManager*      GetColorGradientManager() const { return m_pColorGradientManager; }

	virtual void                AddBreakEventListener(IBreakEventListener* pListener);
	virtual void                RemoveBreakEventListener(IBreakEventListener* pListener);

	void                        OnBreakEvent(uint16 uBreakEventIndex);
	void                        OnPartRemoveEvent(int32 iPartRemoveEventIndex);

	virtual void                RegisterListener(IGameFrameworkListener* pGameFrameworkListener, const char* name, EFRAMEWORKLISTENERPRIORITY eFrameworkListenerPriority);
	virtual void                UnregisterListener(IGameFrameworkListener* pGameFrameworkListener);

	CDialogSystem*              GetDialogSystem()             { return m_pDialogSystem; }
	CTimeOfDayScheduler*        GetTimeOfDayScheduler() const { return m_pTimeOfDayScheduler; }

	CGameStatsConfig*           GetGameStatsConfig();
	IGameStatistics*            GetIGameStatistics();

	IGameSessionHandler*        GetIGameSessionHandler();
	void                        SetGameSessionHandler(IGameSessionHandler* pSessionHandler);

	CNetMessageDistpatcher*     GetNetMessageDispatcher()      { return m_pNetMsgDispatcher; }
	CEntityContainerMgr&         GetEntityContainerMgr()       { return *m_pEntityContainerMgr; }
	CEntityAttachmentExNodeRegistry& GetEntityAttachmentExNodeRegistry() { return *m_pEntityAttachmentExNodeRegistry; }

	//	INetQueryListener* GetLanQueryListener() {return m_pLanQueryListener;}
	bool                          LoadingScreenEnabled() const;

	int                           NetworkExposeClass(IFunctionHandler* pFH);

	void                          NotifyGameFrameworkListeners(ISaveGame* pSaveGame);
	void                          NotifyGameFrameworkListeners(ILoadGame* pLoadGame);
	void                          NotifySavegameFileLoadedToListeners(const char* pLevelName);
	void                          NotifyForceFlashLoadingListeners();
	virtual void                  EnableVoiceRecording(const bool enable);
	virtual void                  MutePlayerById(EntityId mutePlayer);
	virtual IDebugHistoryManager* CreateDebugHistoryManager();
	virtual void                  ExecuteCommandNextFrame(const char* cmd);
	virtual const char*           GetNextFrameCommand() const;
	virtual void                  ClearNextFrameCommand();
	virtual void                  PrefetchLevelAssets(const bool bEnforceAll);

	virtual void                  ShowPageInBrowser(const char* URL);
	virtual bool                  StartProcess(const char* cmd_line);
	virtual bool                  SaveServerConfig(const char* path);

	void                          OnActionEvent(const SActionEvent& ev);

	bool                          IsPbSvEnabled() const { return m_pbSvEnabled; }
	bool                          IsPbClEnabled() const { return m_pbClEnabled; }

	void                          DumpMemInfo(const char* format, ...) PRINTF_PARAMS(2, 3);

	const char*                   GetStartLevelSaveGameName();

	virtual IAIActorProxy*        GetAIActorProxy(EntityId entityid) const;
	CAIProxyManager*              GetAIProxyManager()       { return m_pAIProxyManager; }
	const CAIProxyManager*        GetAIProxyManager() const { return m_pAIProxyManager; }

	void                          CreatePhysicsQueues();
	void                          ClearPhysicsQueues();
	CCryActionPhysicQueues& GetPhysicQueues();
	bool                    IsGameSessionMigrating();

	void                    StartNetworkStallTicker(bool includeMinimalUpdate);
	void                    StopNetworkStallTicker();
	void                    GoToSegment(int x, int y);

	const std::vector<INetworkedClientListener*>& GetNetworkClientListeners() const { return m_networkClientListeners; }
	void FastShutdown();

private:
	bool Initialize(SSystemInitParams& initParams);

	void InitScriptBinds();
	void ReleaseScriptBinds();

	bool InitGame(SSystemInitParams& startupParams);
	bool ShutdownGame();

	void InitForceFeedbackSystem();
	void InitGameVolumesManager();

	void InitCVars();
	void ReleaseCVars();

	void InitCommands();

	// console commands provided by CryAction
	static void DumpMapsCmd(IConsoleCmdArgs* args);
	static void MapCmd(IConsoleCmdArgs* args);
	static void ReloadReadabilityXML(IConsoleCmdArgs* args);
	static void UnloadCmd(IConsoleCmdArgs* args);
	static void PlayCmd(IConsoleCmdArgs* args);
	static void ConnectCmd(IConsoleCmdArgs* args);
	static void DisconnectCmd(IConsoleCmdArgs* args);
	static void DisconnectChannelCmd(IConsoleCmdArgs* args);
	static void StatusCmd(IConsoleCmdArgs* args);
	static void LegacyStatusCmd(IConsoleCmdArgs* args);
	static void VersionCmd(IConsoleCmdArgs* args);
	static void SaveTagCmd(IConsoleCmdArgs* args);
	static void LoadTagCmd(IConsoleCmdArgs* args);
	static void SaveGameCmd(IConsoleCmdArgs* args);
	static void GenStringsSaveGameCmd(IConsoleCmdArgs* args);
	static void LoadGameCmd(IConsoleCmdArgs* args);
	static void KickPlayerCmd(IConsoleCmdArgs* args);
	static void LegacyKickPlayerCmd(IConsoleCmdArgs* args);
	static void KickPlayerByIdCmd(IConsoleCmdArgs* args);
	static void LegacyKickPlayerByIdCmd(IConsoleCmdArgs* args);
	static void BanPlayerCmd(IConsoleCmdArgs* args);
	static void LegacyBanPlayerCmd(IConsoleCmdArgs* args);
	static void BanStatusCmd(IConsoleCmdArgs* args);
	static void LegacyBanStatusCmd(IConsoleCmdArgs* args);
	static void UnbanPlayerCmd(IConsoleCmdArgs* args);
	static void LegacyUnbanPlayerCmd(IConsoleCmdArgs* args);
	static void OpenURLCmd(IConsoleCmdArgs* args);
	static void TestResetCmd(IConsoleCmdArgs* args);

	static void DumpAnalysisStatsCmd(IConsoleCmdArgs* args);

#if !defined(_RELEASE)
	static void ConnectRepeatedlyCmd(IConsoleCmdArgs* args);
#endif

	static void TestTimeout(IConsoleCmdArgs* args);
	static void TestNSServerBrowser(IConsoleCmdArgs* args);
	static void TestNSServerReport(IConsoleCmdArgs* args);
	static void TestNSChat(IConsoleCmdArgs* args);
	static void TestNSStats(IConsoleCmdArgs* args);
	static void TestNSNat(IConsoleCmdArgs* args);
	static void TestPlayerBoundsCmd(IConsoleCmdArgs* args);
	static void DelegateCmd(IConsoleCmdArgs* args);
	static void DumpStatsCmd(IConsoleCmdArgs* args);

	// console commands for the remote control system
	//static void rcon_password(IConsoleCmdArgs* args);
	static void rcon_startserver(IConsoleCmdArgs* args);
	static void rcon_stopserver(IConsoleCmdArgs* args);
	static void rcon_connect(IConsoleCmdArgs* args);
	static void rcon_disconnect(IConsoleCmdArgs* args);
	static void rcon_command(IConsoleCmdArgs* args);

	static struct IRemoteControlServer* s_rcon_server;
	static struct IRemoteControlClient* s_rcon_client;

	static class CRConClientListener*   s_rcon_client_listener;

	//static string s_rcon_password;

	// console commands for the simple http server
	static void http_startserver(IConsoleCmdArgs* args);
	static void http_stopserver(IConsoleCmdArgs* args);

	static struct ISimpleHttpServer* s_http_server;

	// change the game query (better than setting it explicitly)
	void SetGameQueryListener(CGameQueryListener*);

	void CheckEndLevelSchedule();

#if !defined(_RELEASE)
	void CheckConnectRepeatedly();
#endif

	static void MutePlayer(IConsoleCmdArgs* pArgs);

	static void VerifyMaxPlayers(ICVar* pVar);
	static void ResetComments(ICVar* pVar);

	static void StaticSetPbSvEnabled(IConsoleCmdArgs* pArgs);
	static void StaticSetPbClEnabled(IConsoleCmdArgs* pArgs);

	// NOTE: anything owned by this class should be a pointer or a simple
	// type - nothing that will need its constructor called when CryAction's
	// constructor is called (we don't have access to malloc() at that stage)

	bool                          m_paused;
	bool                          m_forcedpause;

	static CCryAction*            m_pThis;

	ISystem*                      m_pSystem;
	INetwork*                     m_pNetwork;
	I3DEngine*                    m_p3DEngine;
	IScriptSystem*                m_pScriptSystem;
	IEntitySystem*                m_pEntitySystem;
	ITimer*                       m_pTimer;
	ILog*                         m_pLog;
	IGameToEditorInterface*       m_pGameToEditor;

	_smart_ptr<CActionGame>       m_pGame;

	char                          m_editorLevelName[512]; // to avoid having to call string constructor, or allocating memory.
	char                          m_editorLevelFolder[512];
	char                          m_gameGUID[128];

	CLevelSystem*                 m_pLevelSystem;
	CActorSystem*                 m_pActorSystem;
	CItemSystem*                  m_pItemSystem;
	CVehicleSystem*               m_pVehicleSystem;
	CSharedParamsManager*         m_pSharedParamsManager;
	CActionMapManager*            m_pActionMapManager;
	CViewSystem*                  m_pViewSystem;
	CGameplayRecorder*            m_pGameplayRecorder;
	CGameRulesSystem*             m_pGameRulesSystem;

	CGameObjectSystem*            m_pGameObjectSystem;
	CUIDraw*                      m_pUIDraw;
	CScriptRMI*                   m_pScriptRMI;
	CAnimationGraphCVars*         m_pAnimationGraphCvars;
	IMannequin*                   m_pMannequin;
	CMaterialEffects*             m_pMaterialEffects;
	IBreakableGlassSystem*        m_pBreakableGlassSystem;
	CPlayerProfileManager*        m_pPlayerProfileManager;
	CDialogSystem*                m_pDialogSystem;
	CSubtitleManager*             m_pSubtitleManager;

	IEffectSystem*                m_pEffectSystem;
	CGameSerialize*               m_pGameSerialize;
	CallbackTimer*                m_pCallbackTimer;
	CGameplayAnalyst*             m_pGameplayAnalyst;
	CForceFeedBackSystem*         m_pForceFeedBackSystem;
	//	INetQueryListener *m_pLanQueryListener;
	ILanQueryListener*            m_pLanQueryListener;
	CCustomActionManager*         m_pCustomActionManager;
	CCustomEventManager*          m_pCustomEventManager;

	CGameStatsConfig*             m_pGameStatsConfig;

	IGameStatistics*              m_pGameStatistics;

	ICooperativeAnimationManager* m_pCooperativeAnimationManager;
	IGameSessionHandler*          m_pGameSessionHandler;

	CAIProxyManager*              m_pAIProxyManager;

	IGameVolumes*                 m_pGameVolumesManager;

	// developer mode
	CDevMode* m_pDevMode;

	// TimeDemo recorder.
	CTimeDemoRecorder* m_pTimeDemoRecorder;

	// game queries
	CGameQueryListener* m_pGameQueryListener;

	// Currently handles the automatic creation of vegetation areas
	CRuntimeAreaManager* m_pRuntimeAreaManager;

	// script binds
	CScriptBind_Action*           m_pScriptA;
	CScriptBind_ItemSystem*       m_pScriptIS;
	CScriptBind_ActorSystem*      m_pScriptAS;
	CScriptBind_Network*          m_pScriptNet;
	CScriptBind_ActionMapManager* m_pScriptAMM;
	CScriptBind_VehicleSystem*    m_pScriptVS;
	CScriptBind_Vehicle*          m_pScriptBindVehicle;
	CScriptBind_VehicleSeat*      m_pScriptBindVehicleSeat;
	CScriptBind_Inventory*        m_pScriptInventory;
	CScriptBind_DialogSystem*     m_pScriptBindDS;
	CScriptBind_MaterialEffects*  m_pScriptBindMFX;
	CScriptBind_UIAction*         m_pScriptBindUIAction;
	CTimeOfDayScheduler*          m_pTimeOfDayScheduler;
	CPersistantDebug*             m_pPersistantDebug;

	CAIDebugRenderer*             m_pAIDebugRenderer;
	CAINetworkDebugRenderer*      m_pAINetworkDebugRenderer;

	CNetworkCVars*                m_pNetworkCVars;
	CCryActionCVars*              m_pCryActionCVars;

	CColorGradientManager*        m_pColorGradientManager;

	//-- Network Stall ticker thread
#ifdef USE_NETWORK_STALL_TICKER_THREAD
	CNetworkStallTickerThread* m_pNetworkStallTickerThread;
	uint32                     m_networkStallTickerReferences;
#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD

	// Console Variables with some CryAction as owner
	CMaterialEffectsCVars*  m_pMaterialEffectsCVars;

	CCryActionPhysicQueues* m_pPhysicsQueues;

	typedef std::vector<ICryUnknownPtr> TFrameworkExtensions;
	TFrameworkExtensions m_frameworkExtensions;

	// console variables
	ICVar* m_pEnableLoadingScreen;
	ICVar* m_pCheats;
	ICVar* m_pShowLanBrowserCVAR;
	ICVar* m_pDebugSignalTimers;
	ICVar* m_pDebugRangeSignaling;
	ICVar* m_pAsyncLevelLoad;

	bool   m_bShowLanBrowser;
	//
	bool   m_isEditing;
	bool   m_bScheduleLevelEnd;

	enum ESaveGameMethod
	{
		eSGM_NoSave = 0,
		eSGM_QuickSave,
		eSGM_Save
	};

	ESaveGameMethod m_delayedSaveGameMethod;     // 0 -> no save, 1=quick save, 2=save, not quick
	ESaveGameReason m_delayedSaveGameReason;
	int             m_delayedSaveCountDown;

	struct SExternalGameLibrary
	{
		string        dllName;
		HMODULE       dllHandle;
		IGameStartup* pGameStartup;
		IGame*        pGame;

		SExternalGameLibrary() : dllName(""), dllHandle(0), pGameStartup(nullptr), pGame(nullptr) {}
		bool IsValid() const { return (pGameStartup != nullptr && pGame != nullptr); }
		void Reset()         { dllName = ""; dllHandle = 0; pGameStartup = nullptr; pGame = nullptr; }
	};

	struct SLocalAllocs
	{
		string m_delayedSaveGameName;
		string m_checkPointName;
		string m_nextLevelToLoad;
	};
	SLocalAllocs* m_pLocalAllocs;

	struct SGameFrameworkListener
	{
		IGameFrameworkListener*    pListener;
		CryStackStringT<char, 64>  name;
		EFRAMEWORKLISTENERPRIORITY eFrameworkListenerPriority;
		SGameFrameworkListener() : pListener(0), eFrameworkListenerPriority(FRAMEWORKLISTENERPRIORITY_DEFAULT) {}
		void                       GetMemoryUsage(ICrySizer* pSizer) const {}
	};

	typedef std::vector<SGameFrameworkListener> TGameFrameworkListeners;
	TGameFrameworkListeners* m_pGFListeners;
	IBreakEventListener*     m_pBreakEventListener;
	std::vector<bool>        m_validListeners;

	int                      m_VoiceRecordingEnabled;

	bool                     m_bAllowSave;
	bool                     m_bAllowLoad;
	string*                  m_nextFrameCommand;
	string*                  m_connectServer;

#if !defined(_RELEASE)
	struct SConnectRepeatedly
	{
		bool  m_enabled;
		int   m_numAttemptsLeft;
		float m_timeForNextAttempt;

		SConnectRepeatedly() : m_enabled(false), m_numAttemptsLeft(0), m_timeForNextAttempt(0.0f) {}
	} m_connectRepeatedly;
#endif

	float  m_lastSaveLoad;
	float  m_lastFrameTimeUI;

	bool   m_pbSvEnabled;
	bool   m_pbClEnabled;
	uint32 m_PreUpdateTicks;


	SExternalGameLibrary                   m_externalGameLibrary;

	CNetMessageDistpatcher*                m_pNetMsgDispatcher;
	CEntityContainerMgr*                   m_pEntityContainerMgr;
	CEntityAttachmentExNodeRegistry*       m_pEntityAttachmentExNodeRegistry;

	CTimeValue                             m_levelStartTime;

	std::vector<INetworkedClientListener*> m_networkClientListeners;
};
