// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryGame/IGameFrameworkExtension.h>
#include <CryMath/Cry_Color.h>
#include <CrySystem/TimeValue.h>
#include <CryLobby/CommonICryMatchMaking.h>
#include <CryNetwork/INetwork.h>

struct pe_explosion;
struct IPhysicalEntity;
struct EventPhysRemoveEntityParts;
struct ICombatLog;
struct IAIActorProxy;
struct ICooperativeAnimationManager;
struct IGameSessionHandler;
struct IRealtimeRemoteUpdate;
struct IForceFeedbackSystem;
struct ICommunicationVoiceLibrary;
struct ICustomActionManager;
struct ICustomEventManager;
struct ISerializeHelper;
struct IGameVolumes;
struct IGame;
struct IGameServerNub;
struct IGameClientNub;
struct INetworkedClientListener;

//! Define to control the logging of breakability code.
#define BREAK_LOGGING 0

#if BREAK_LOGGING
	#define BreakLogAlways CryLogAlways
#else
	#define BreakLogAlways(...) ((void)0)
#endif

//! Generic factory creation.
//! This macro is used to register new game object extension classes.
#define REGISTER_FACTORY(host, name, impl, isAI)              \
  (host)->RegisterFactory((name), (impl*)0, (isAI), (impl*)0) \


#define DECLARE_GAMEOBJECT_FACTORY(impl)                                           \
  public:                                                                          \
    virtual void RegisterFactory(const char* name, impl * (*)(), bool isAI) = 0;   \
    template<class T> void RegisterFactory(const char* name, impl*, bool isAI, T*) \
    {                                                                              \
      struct Factory                                                               \
      {                                                                            \
        static impl* Create()                                                      \
        {                                                                          \
          return new T();                                                          \
        }                                                                          \
      };                                                                           \
      RegisterFactory(name, Factory::Create, isAI);                                \
    }

//! Game object extensions need more information than the generic interface can provide.
struct IGameObjectExtension;


struct IGameObjectExtensionCreatorBase
{
	// <interfuscator:shuffle>
	virtual ~IGameObjectExtensionCreatorBase(){}
	virtual IGameObjectExtension* Create(IEntity *pEntity) = 0;
	virtual void                    GetGameObjectExtensionRMIData(void** ppRMI, size_t* nCount) = 0;
	// </interfuscator:shuffle>

	void GetMemoryUsage(ICrySizer* pSizer) const { /*LATER*/ }
};

#define DECLARE_GAMEOBJECTEXTENSION_FACTORY(name)                                       \
  struct I ## name ## Creator : public IGameObjectExtensionCreatorBase                  \
  {                                                                                     \
  };                                                                                    \
  template<class T>                                                                     \
  struct C ## name ## Creator : public I ## name ## Creator                             \
  {                                                                                     \
    IGameObjectExtension* Create(IEntity *pEntity) override                             \
    {                                                                                   \
      return pEntity->GetOrCreateComponentClass<T>();                                   \
    }                                                                                   \
    void GetGameObjectExtensionRMIData(void** ppRMI, size_t * nCount) override          \
    {                                                                                   \
      T::GetGameObjectExtensionRMIData(ppRMI, nCount);                                  \
    }                                                                                   \
  };                                                                                    \
  virtual void RegisterFactory(const char* name, I ## name ## Creator*, bool isAI) = 0; \
  template<class T> void RegisterFactory(const char* name, I ## name*, bool isAI, T*)   \
  {                                                                                     \
    static C ## name ## Creator<T> creator;                                             \
    RegisterFactory(name, &creator, isAI);                                              \
  }

struct ISystem;
struct IUIDraw;
struct ILanQueryListener;
struct IActor;
struct IActorSystem;
struct IItem;
struct IGameRules;
struct IWeapon;
struct IItemSystem;
struct ILevelSystem;
struct IActionMapManager;
struct IGameChannel;
struct IViewSystem;
struct IVehicle;
struct IVehicleSystem;
struct IGameRulesSystem;
struct IFlowSystem;
struct IGameTokenSystem;
struct IEffectSystem;
struct IGameObject;
struct IGameObjectExtension;
struct IGameObjectSystem;
struct IGameplayRecorder;
struct IAnimationStateNodeFactory;
struct ISaveGame;
struct ILoadGame;
struct IGameObject;
struct IMaterialEffects;
struct INetChannel;
struct IPlayerProfileManager;
struct IAnimationGraphState;
struct INetNub;
struct ISaveGame;
struct ILoadGame;
struct IDebugHistoryManager;
struct IDebrisMgr;
struct ISubtitleManager;
struct IDialogSystem;
struct IGameStatistics;
struct ICheckpointSystem;
struct IGameToEditorInterface;
struct IMannequin;
struct IScriptTable;
struct ITimeDemoRecorder;

struct SEntitySchedulingProfiles
{
	uint32 normal;
	uint32 owned;
};

class ISharedParamsManager;

struct INeuralNet;
typedef _smart_ptr<INeuralNet> INeuralNetPtr;

enum EGameStartFlags
{
	eGSF_NoLevelLoading        = 0x0001,
	eGSF_Server                = 0x0002,
	eGSF_Client                = 0x0004,
	eGSF_NoDelayedStart        = 0x0008,
	eGSF_BlockingClientConnect = 0x0010,
	eGSF_NoGameRules           = 0x0020,
	eGSF_LocalOnly             = 0x0040,
	eGSF_NoQueries             = 0x0080,
	eGSF_NoSpawnPlayer         = 0x0100,
	eGSF_BlockingMapLoad       = 0x0200,

	eGSF_DemoRecorder          = 0x0400,
	eGSF_DemoPlayback          = 0x0800,

	eGSF_ImmersiveMultiplayer  = 0x1000,
	eGSF_RequireController     = 0x2000,
	eGSF_RequireKeyboardMouse  = 0x4000,

	eGSF_HostMigrated          = 0x8000,
	eGSF_NonBlockingConnect    = 0x10000
};

enum ESaveGameReason
{
	eSGR_LevelStart,
	eSGR_FlowGraph,
	eSGR_Command,
	eSGR_QuickSave
};

enum ELoadGameResult
{
	eLGR_Ok,
	eLGR_Failed,
	eLGR_FailedAndDestroyedState,
	eLGR_CantQuick_NeedFullLoad
};

static const EntityId LOCAL_PLAYER_ENTITY_ID = 0x7777u; //!< 30583 between static and dynamic EntityIDs.

struct SGameContextParams
{
	const char* levelName;
	const char* gameRules;
	const char* demoRecorderFilename;
	const char* demoPlaybackFilename;

	SGameContextParams()
	{
		levelName = 0;
		gameRules = 0;
		demoRecorderFilename = 0;
		demoPlaybackFilename = 0;
	}
};

struct SGameStartParams
{
	//! IP address/hostname of server to connect to - needed if bClient==true.
	const char* hostname;

	//! Optional connection string for client.
	const char* connectionString;

	//! Context parameters - needed if bServer==true.
	const SGameContextParams* pContextParams;

	//! A combination of EGameStartFlags - needed if bServer==true.
	uint32 flags;

	//! Maximum players to allow to connect.
	int32 maxPlayers;

	//! Session handle if connecting via CryLobby.
	CrySessionHandle session;

	//! UDP port to connect to.
	uint16 port;

	SGameStartParams()
	{
		flags = 0;
		port = 0;
		hostname = 0;
		connectionString = 0;
		pContextParams = NULL;
		maxPlayers = 32;
		session = CrySessionInvalidHandle;
	}
};

struct SEntityTagParams
{
	EntityId entity;
	string   text;
	float    size;          //!< Font size.
	float    visibleTime;   //!< Seconds before starting fade, >= 0.
	float    fadeTime;      //!< Seconds to fade over, >= 0.
	float    viewDistance;  //!< Maximum distance of entity from camera to show tag.
	string   staticId;      //!< When nonempty string, display first for entity, and only most recent one (for continuous info like health display).
	int      column;        //!< For multicolumn tag display (0 or 1 defaults to standard 1 column display).
	ColorF   color;
	string   tagContext;

	SEntityTagParams()  { Init(); }
	SEntityTagParams(EntityId entity, const char* text)
	{
		Init();
		this->entity = entity;
		this->text = text ? text : "";
	}
	SEntityTagParams(EntityId entity, const char* text, float size, const ColorF& color, float duration)
	{
		Init();
		this->entity = entity;
		this->text = text ? text : "";
		this->size = size;
		this->color = color;
		this->fadeTime = duration;
	}

private:
	void Init()
	{
		entity = 0;
		text = "";
		size = 1.5f;
		visibleTime = 2.f;
		fadeTime = 1.f;
		viewDistance = 1000.f;
		staticId = "";
		column = 1;
		color = ColorF(1.f, 1.f, 1.f, 1.f);
		tagContext = "";
	}
};

typedef uint32 THUDWarningId;
struct IGameWarningsListener
{
	// <interfuscator:shuffle>
	virtual ~IGameWarningsListener(){}
	virtual bool OnWarningReturn(THUDWarningId id, const char* returnValue) { return true; }
	virtual void OnWarningRemoved(THUDWarningId id)                         {}
	// </interfuscator:shuffle>
};

//! \cond INTERNAL
//! SRenderNodeCloneLookup is used to associate original IRenderNodes (used in the game) with cloned IRenderNodes, to allow breaks to be played back.
struct SRenderNodeCloneLookup
{
	SRenderNodeCloneLookup()
	{
		pOriginalNodes = NULL;
		pClonedNodes = NULL;
		iNumPairs = 0;
	}

	void UpdateStoragePointers(std::vector<IRenderNode*>& originalNodes, std::vector<IRenderNode*>& clonedNodes)
	{
		pOriginalNodes = originalNodes.empty() ? NULL : &(originalNodes[0]);
		pClonedNodes = clonedNodes.empty() ? NULL : &(clonedNodes[0]);
	}

	void AddNodePair(IRenderNode* originalNode, IRenderNode* clonedNode)
	{
		pOriginalNodes[iNumPairs] = originalNode;
		pClonedNodes[iNumPairs] = clonedNode;
		iNumPairs++;
	}

	void Reset()
	{
		iNumPairs = 0;
		pOriginalNodes = NULL;
		pClonedNodes = NULL;
	}

	IRenderNode** pOriginalNodes;
	IRenderNode** pClonedNodes;
	int           iNumPairs;
};

//! Provides an interface to game so game will be able to display numeric stats in user-friendly way.
struct IGameStatsConfig
{
	// <interfuscator:shuffle>
	virtual ~IGameStatsConfig(){}
	virtual int         GetStatsVersion() = 0;
	virtual int         GetCategoryMod(const char* cat) = 0;
	virtual const char* GetValueNameByCode(const char* cat, int id) = 0;
	// </interfuscator:shuffle>
};
//! \endcond

struct IBreakReplicator
{
	// <interfuscator:shuffle>
	virtual ~IBreakReplicator(){}
	virtual const EventPhysRemoveEntityParts* GetRemovePartEvents(int& iNumEvents) = 0;
	// </interfuscator:shuffle>
};

//! Persistent debug exposes functionality for drawing debug geometry over a specific period of time, without having to continuously re-render manually each frame.
//! This can be extremely useful to debug gameplay logic.
struct IPersistantDebug
{
	// <interfuscator:shuffle>
	virtual ~IPersistantDebug(){}
	//! Starts a persistent debug drawing group
	//! It is mandatory to call this function before invoking any of the Add* functions!
	//! \param szName The name of the group
	//! \param clear Whether or not to clear any persistent drawing done to the specified group before
	virtual void Begin(const char* szName, bool clear) = 0;
	//! Adds a persistent sphere at the specified location
	//! \param pos The world coordinates to draw this object at
	//! \param radius Radius of the sphere
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddSphere(const Vec3& pos, float radius, ColorF clr, float timeout) = 0;
	//! Adds a persistent direction indicator at the specified location
	//! \param pos The world coordinates to draw this object at
	//! \param radius Radius of the directional indicator
	//! \param dir Directional vector we want to visualize
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddDirection(const Vec3& pos, float radius, const Vec3& dir, ColorF clr, float timeout) = 0;
	//! Adds a persistent line at the specified coordinates
	//! \param pos1 Origin of the line, in world coordinates
	//! \param pos2 End point of the line, in world coordinates
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddLine(const Vec3& pos1, const Vec3& pos2, ColorF clr, float timeout) = 0;
	//! Adds a planar disc to the specified coordinates
	//! \param pos The world coordinates to draw this object at
	//! \param innerRadius The inner radius of the disc
	//! \param outerRadius The outer radius of the disc
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddPlanarDisc(const Vec3& pos, float innerRadius, float outerRadius, ColorF clr, float timeout) = 0;
	//! \param pos The world coordinates to draw this object at
	//! \param dir The direction in which the cone will point
	//! \param baseRadius Radius of the cone at its base
	//! \param height Height of the cone
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, ColorF clr, float timeout) = 0;
	//! Adds a cylinder at the specified coordinates
	//! \param pos The world coordinates to draw this object at
	//! \param dir Direction in which the cylinder will point
	//! \param radius Radius of the cylinder
	//! \param height Height of the cylinder
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, ColorF clr, float timeout) = 0;
	//! Adds 2D text on screen
	//! \param szText Text message to draw
	//! \param size Size of the text
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void Add2DText(const char* szText, float size, ColorF clr, float timeout) = 0;
	//! Adds 2D text to the specified screen coordinates
	//! \param x X axis coordinate in screen space
	//! \param y Y axis coordinate in screen space
	//! \param size Size of the text
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	//! \param fmt... printf style text message to be drawn on screen
	virtual void AddText(float x, float y, float size, ColorF clr, float timeout, const char* fmt, ...) = 0;
	//! Adds 3D text to the specified world coordinates
	//! \param pos The world coordinates to draw this object at
	//! \param size Size of the text
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	//! \param fmt... printf style text message to be drawn on screen
	virtual void AddText3D(const Vec3& pos, float size, ColorF clr, float timeout, const char* fmt, ...) = 0;
	//! Adds a 2D line on screen
	//! \param x1 X axis coordinate in screen space where the line starts
	//! \param y1 Y axis coordinate in screen space where the line starts
	//! \param x2 X axis coordinate in screen space where the line ends
	//! \param y2 Y axis coordinate in screen space where the line ends
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void Add2DLine(float x1, float y1, float x2, float y2, ColorF clr, float timeout) = 0;
	//! Adds a visualized quaternion to the specified coordinates
	//! \param pos The world coordinates to draw this object at
	//! \param q The quaternion to visualize
	//! \param r Radius of the helper
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddQuat(const Vec3& pos, const Quat& q, float r, ColorF clr, float timeout) = 0;
	//! Adds an axis-aligned bounding box
	//! \param min Starting coordinates of the bounding box
	//! \param min End coordinates of the bounding box
	//! \param clr Color of the debug geometry
	//! \param timeout Timeout in seconds after which the item will stop rendering
	virtual void AddAABB(const Vec3& min, const Vec3& max, ColorF clr, float timeout) = 0;
	//! Adds a tag above the specified entity, using the specified parameters
	//! \param params Parameters describing the tag
	//! \param The context in which we'll add the tag
	virtual void AddEntityTag(const SEntityTagParams& params, const char* tagContext = "") = 0;
	//! Clears all entity tags for the specified entity
	virtual void ClearEntityTags(EntityId entityId) = 0;
	//! Clears a specific tag for the specified entity
	virtual void ClearStaticTag(EntityId entityId, const char* staticId) = 0;
	//! Clears all entity tags by context
	virtual void ClearTagContext(const char* tagContext) = 0;
	//! Clears all entity tags by context, for a specific entity instance
	virtual void ClearTagContext(const char* tagContext, EntityId entityId) = 0;
	virtual void Update(float frameTime) = 0;
	virtual void PostUpdate(float frameTime) = 0;
	virtual void Reset() = 0;
	// </interfuscator:shuffle>
};

//! This is the order in which GOEs receive events.
enum EEntityEventPriority
{
	EEntityEventPriority_GameObject = 0,
	EEntityEventPriority_PrepareAnimatedCharacterForUpdate,
	EEntityEventPriority_Actor,         //!< Actor must always be higher than AnimatedCharacter.
	EEntityEventPriority_Vehicle,       //!< Vehicles can potentially create move request too!
	EEntityEventPriority_AnimatedCharacter,
	EEntityEventPriority_StartAnimProc
};

//! When you add stuff here, you must also update in CCryAction::Init.
enum EGameFrameworkEvent
{
	eGFE_PauseGame,
	eGFE_ResumeGame,
	eGFE_OnCollision,
	eGFE_OnPostStep,
	eGFE_OnStateChange,
	eGFE_ResetAnimationGraphs,
	eGFE_OnBreakable2d,
	eGFE_OnBecomeVisible,
	eGFE_PreShatter,
	eGFE_DisablePhysics,
	eGFE_EnablePhysics,
	eGFE_ScriptEvent,
	eGFE_StoodOnChange,
	eGFE_QueueRagdollCreation,  //!< Queue the ragdoll for creation so the engine can do it at the best time.
	eGFE_QueueBlendFromRagdoll, //!< Queue the blend from ragdoll event (i.e. standup).
	eGFE_RagdollPhysicalized,   //!< Dispatched when the queued ragdoll is physicalized.
	eGFE_RagdollUnPhysicalized, //!< Dispatched when the queued ragdoll is unphysicalized (i.e. Stoodup).
	eGFE_EnableBlendRagdoll,    //!< Enable blend with ragdoll mode (will blend with the currently active animation).
	eGFE_DisableBlendRagdoll,   //!< Disable blend with ragdoll (will blend out the ragdoll with the currently active animation).

	eGFE_Last
};

//! All events game should be aware of need to be added here.
enum EActionEvent
{
	eAE_channelCreated,
	eAE_channelDestroyed,
	eAE_connectFailed,
	eAE_connected,
	eAE_disconnected,
	eAE_clientDisconnected,
	eAE_disconnectCommandFinished,
	// Map resetting.
	eAE_resetBegin,
	eAE_resetEnd,
	eAE_resetProgress,
	eAE_preSaveGame,         //!< m_value -> ESaveGameReason.
	eAE_postSaveGame,        //!< m_value -> ESaveGameReason, m_description: 0 (failed), != 0 (successful).
	eAE_inGame,

	eAE_serverName,          //!< Started server.
	eAE_serverIp,            //!< Obtained server ip.
	eAE_earlyPreUpdate,      //!< Called from CryAction's PreUpdate loop after System has been updated, but before subsystems.
	eAE_demoRecorderCreated,
	eAE_mapCmdIssued,
	eAE_unloadLevel,
	eAE_postUnloadLevel,
	eAE_loadLevel,
};

struct SActionEvent
{
	SActionEvent(EActionEvent e, int val = 0, const char* des = 0) :
		m_event(e),
		m_value(val),
		m_description(des)
	{}
	EActionEvent m_event;
	int          m_value;
	const char*  m_description;
};

//! We must take care of order in which listeners are called.
//! Priority order is from low to high.
//! As an example, menu must follow hud as it must be drawn on top of the rest.
enum EFRAMEWORKLISTENERPRIORITY
{
	//! Default priority should not be used unless you don't care about order (it will be called first).
	FRAMEWORKLISTENERPRIORITY_DEFAULT,

	//! Add your order somewhere here if you need to be called between one of them.
	FRAMEWORKLISTENERPRIORITY_GAME,
	FRAMEWORKLISTENERPRIORITY_HUD,
	FRAMEWORKLISTENERPRIORITY_MENU
};

struct IGameFrameworkListener
{
	virtual ~IGameFrameworkListener(){}
	virtual void OnPostUpdate(float fDeltaTime) = 0;
	virtual void OnSaveGame(ISaveGame* pSaveGame) = 0;
	virtual void OnLoadGame(ILoadGame* pLoadGame) = 0;
	virtual void OnLevelEnd(const char* nextLevel) = 0;
	virtual void OnActionEvent(const SActionEvent& event) = 0;
	virtual void OnPreRender() {}

	//! Called when the savegame data is in memory, but before the procesing of it starts.
	virtual void OnSavegameFileLoadedInMemory(const char* pLevelName) {}
	virtual void OnForceLoadingWithFlash()                            {}
};

struct IBreakEventListener
{
	virtual ~IBreakEventListener(){}
	virtual void OnBreakEvent(uint16 uBreakEventIndex) = 0;
	virtual void OnPartRemoveEvent(int32 iPartRemoveEventIndex) = 0;
	virtual void OnEntityDrawSlot(IEntity* pEntity, int32 slot, int32 flags) = 0;
	virtual void OnEntityChangeStatObj(IEntity* pEntity, int32 iBrokenObjectIndex, int32 slot, IStatObj* pOldStatObj, IStatObj* pNewStatObj) = 0;
	virtual void OnSetSubObjHideMask(IEntity* pEntity, int nSlot, hidemask nSubObjHideMask) = 0;
};

// Interface for the CryAction engine module
struct IGameFrameworkEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IGameFrameworkEngineModule, "CE1E93CB-2665-4F76-809D-070F11418EB9"_cry_guid);
};

//! Interface which exposes the CryAction subsystems.
struct IGameFramework
{
	DECLARE_GAMEOBJECT_FACTORY(ISaveGame);
	DECLARE_GAMEOBJECT_FACTORY(ILoadGame);
	DECLARE_GAMEOBJECTEXTENSION_FACTORY(Actor);
	DECLARE_GAMEOBJECTEXTENSION_FACTORY(Item);
	DECLARE_GAMEOBJECTEXTENSION_FACTORY(Vehicle);
	DECLARE_GAMEOBJECTEXTENSION_FACTORY(GameObjectExtension);

	typedef uint32                   TimerID;
	typedef Functor2<void*, TimerID> TimerCallback;

	//! Type to represent saved game names, keeping the string on the stack if possible.
	typedef CryStackStringT<char, 256> TSaveGameName;

	// <interfuscator:shuffle>
	virtual ~IGameFramework(){}

	//! Called when the engine is shutting down to finalize the game framework
	virtual void ShutDown() = 0;

	//! Called just before calling ISystem::RenderBegin, after the renderer has been notified to prepare for a new frame
	virtual void PreSystemUpdate() = 0;

	//! Updates the main game systems
	//! Called immediately after ISystem::Update, when core engine systems have been updated
	//! \return True if the engine should continue running, otherwise false.
	virtual bool PostSystemUpdate(bool hasFocus, CEnumFlags<ESystemUpdateFlags> updateFlags) = 0;

	//! Called when systems depending on rendering have been updated, and we are about to use the system camera
	//! This is the final chance to modify the camera before it is passed to the 3D engine for occlusion culling
	virtual void PreFinalizeCamera(CEnumFlags<ESystemUpdateFlags> updateFlags) = 0;

	//! Called just before ISystem::Render
	virtual void PreRender() = 0;

	//! Called after ISystem::Render, when the renderer should now have started rendering
	virtual void PostRender(CEnumFlags<ESystemUpdateFlags> updateFlags) = 0;

	//! Called after ISystem::RenderEnd, when the renderer has been notified that the frame is final
	virtual void PostRenderSubmit() = 0;

	//! Used to notify the framework that we're switching between single and multi player.
	virtual void InitGameType(bool multiplayer, bool fromInit) = 0;

	//! Calls Physics update before starting a game frame
	virtual void PrePhysicsUpdate() = 0;

	//! Resets the current game
	virtual void Reset(bool clients) = 0;

	//! Pauses the game
	//! \param pause true if the game is pausing, false otherwise.
	//! \param nFadeOutInMS Time SFX and Voice will be faded out over in MilliSec.
	virtual void PauseGame(bool pause, bool force, unsigned int nFadeOutInMS = 0) = 0;

	//! Returns the pause status
	//! \return true if the game is paused, false otherwise.
	virtual bool IsGamePaused() = 0;

	//! Are we completely into game mode?
	virtual bool IsGameStarted() = 0;

	//! \return Pointer to the ISystem interface.
	virtual ISystem* GetISystem() = 0;

	//! \return Pointer to the ILanQueryListener interface.
	virtual ILanQueryListener* GetILanQueryListener() = 0;

	//! \return Pointer to the IUIDraw interface.
	virtual IUIDraw*    GetIUIDraw() = 0;

	virtual IMannequin& GetMannequinInterface() = 0;

	//! Returns a pointer to the IGameObjectSystem interface.
	//! \return Pointer to IGameObjectSystem interface.
	virtual IGameObjectSystem* GetIGameObjectSystem() = 0;

	//! Returns a pointer to the ILevelSystem interface.
	//! \return Pointer to ILevelSystem interface.
	virtual ILevelSystem* GetILevelSystem() = 0;

	//! Returns a pointer to the IActorSystem interface.
	//! \return Pointer to IActorSystem interface.
	virtual IActorSystem* GetIActorSystem() = 0;

	//! Returns a pointer to the IItemSystem interface.
	//! \return Pointer to IItemSystem interface.
	virtual IItemSystem* GetIItemSystem() = 0;

	//! Returns a pointer to the IBreakReplicator interface.
	//! \return Pointer to IBreakReplicator interface.
	virtual IBreakReplicator* GetIBreakReplicator() = 0;

	//! Returns a pointer to the IActionMapManager interface.
	//! \return Pointer to IActionMapManager interface.
	virtual IActionMapManager* GetIActionMapManager() = 0;

	//! Returns a pointer to the IViewSystem interface.
	//! \return Pointer to IViewSystem interface.
	virtual IViewSystem* GetIViewSystem() = 0;

	//! Returns a pointer to the IGameplayRecorder interface.
	//! \return Pointer to IGameplayRecorder interface.
	virtual IGameplayRecorder* GetIGameplayRecorder() = 0;

	//! Returns a pointer to the IVehicleSystem interface.
	//! \return Pointer to IVehicleSystem interface.
	virtual IVehicleSystem* GetIVehicleSystem() = 0;

	//! Returns a pointer to the IGameRulesSystem interface.
	//! \return Pointer to IGameRulesSystem interface.
	virtual IGameRulesSystem* GetIGameRulesSystem() = 0;

	//! Returns a pointer to the IFlowSystem interface.
	//! \return Pointer to IFlowSystem interface.
	virtual IFlowSystem* GetIFlowSystem() = 0;

	//! Returns a pointer to the IGameTokenSystem interface
	//! \return Pointer to IGameTokenSystem interface.
	virtual IGameTokenSystem* GetIGameTokenSystem() = 0;

	//! Returns a pointer to the IEffectSystem interface
	//! \return Pointer to IEffectSystem interface.
	virtual IEffectSystem* GetIEffectSystem() = 0;

	//! Returns a pointer to the IMaterialEffects interface.
	//! \return Pointer to IMaterialEffects interface.
	virtual IMaterialEffects* GetIMaterialEffects() = 0;

	//! Returns a pointer to the IDialogSystem interface
	//! \return Pointer to IDialogSystem interface.
	virtual IDialogSystem* GetIDialogSystem() = 0;

	//! Returns a pointer to the IPlayerProfileManager interface.
	//! \return Pointer to IPlayerProfileManager interface.
	virtual IPlayerProfileManager* GetIPlayerProfileManager() = 0;

	//! Returns a pointer to the ISubtitleManager interface.
	//! \return Pointer to ISubtitleManager interface.
	virtual ISubtitleManager* GetISubtitleManager() = 0;

	//! Returns a pointer to the IRealtimeUpdate Interface.
	virtual IRealtimeRemoteUpdate* GetIRealTimeRemoteUpdate() = 0;

	//! Returns a pointer to the IGameStatistics interface.
	virtual IGameStatistics* GetIGameStatistics() = 0;

	//! Pointer to ICooperativeAnimationManager interface.
	virtual ICooperativeAnimationManager* GetICooperativeAnimationManager() = 0;

	//! Pointer to ICheckpointSystem interface.
	virtual ICheckpointSystem* GetICheckpointSystem() = 0;

	//! Pointer to IForceFeedbackSystem interface.
	virtual IForceFeedbackSystem* GetIForceFeedbackSystem() const = 0;

	//! Pointer to ICustomActionManager interface.
	virtual ICustomActionManager* GetICustomActionManager() const = 0;

	//! Pointer to ICustomEventManager interface.
	virtual ICustomEventManager* GetICustomEventManager() const = 0;

	virtual IGameSessionHandler* GetIGameSessionHandler() = 0;

	//! Get pointer to Shared Parameters manager interface class.
	virtual ISharedParamsManager* GetISharedParamsManager() = 0;

	// Get game implementation, if any
	virtual IGame* GetIGame() = 0;

	// Gets the handle for the Game DLL
	virtual void* GetGameModuleHandle() const = 0;

	//! Initialises a game context.
	//! \param pGameStartParams Parameters for configuring the game.
	//! \return true if successful, false otherwise.
	virtual bool StartGameContext(const SGameStartParams* pGameStartParams) = 0;

	//! Changes a game context (levels and rules, etc); only allowed on the server.
	//! \param pGameContextParams Parameters for configuring the context.
	//! \return true if successful, false otherwise.
	virtual bool ChangeGameContext(const SGameContextParams* pGameContextParams) = 0;

	//! Finished a game context (no game running anymore).
	virtual void EndGameContext() = 0;

	//! Detect if a context is currently running.
	//! \return true if a game context is running.
	virtual bool StartedGameContext() const = 0;

	//! Detect if a context is currently starting.
	//! \return true if a game context is starting.
	virtual bool StartingGameContext() const = 0;

	//! Sets the current game session handler to another implementation.
	virtual void SetGameSessionHandler(IGameSessionHandler* pSessionHandler) = 0;

	//! For the editor: spawn a player and wait for connection
	virtual bool BlockingSpawnPlayer() = 0;

	//! Remove broken entity parts
	virtual void FlushBreakableObjects() = 0;

	//! For the game : fix the broken game objects (to restart the map)
	virtual void ResetBrokenGameObjects() = 0;

	//! For the kill cam : clone the list of objects specified in the break events indexed
	virtual void CloneBrokenObjectsAndRevertToStateAtTime(int32 iFirstBreakEventIndex, uint16* pBreakEventIndices, int32& iNumBreakEvents, IRenderNode** outClonedNodes, int32& iNumClonedNodes, SRenderNodeCloneLookup& renderNodeLookup) = 0;

	//! For the kill cam: apply a single break event from an index
	virtual void ApplySingleProceduralBreakFromEventIndex(uint16 uBreakEventIndex, const SRenderNodeCloneLookup& renderNodeLookup) = 0;

	//! For the game: unhide the broken game objects (at the end of the kill cam)
	virtual void UnhideBrokenObjectsByIndex(uint16* ObjectIndicies, int32 iNumObjectIndices) = 0;

	//! Let the GameFramework initialize with the editor
	virtual void InitEditor(IGameToEditorInterface* pGameToEditor) = 0;

	//! Inform the GameFramework of the current level loaded in the editor.
	virtual void SetEditorLevel(const char* levelName, const char* levelFolder) = 0;

	//! Retrieves the current level loaded by the editor.
	//! Parameters are pointers to receive the level infos.
	virtual void GetEditorLevel(char** levelName, char** levelFolder) = 0;

	//! Begin a query on the LAN for games
	virtual void BeginLanQuery() = 0;

	//! End the current game query
	virtual void EndCurrentQuery() = 0;

	//! Returns the Actor associated with the client (or NULL)
	virtual IActor* GetClientActor() const = 0;

	//! Returns the Actor Id associated with the client (or NULL)
	virtual EntityId GetClientActorId() const = 0;

	//! Returns the Entity associated with the client (or NULL)
	virtual IEntity* GetClientEntity() const = 0;

	//! Returns the EntityId associated with the client (or NULL)
	virtual EntityId GetClientEntityId() const = 0;

	//! Returns the INetChannel associated with the client (or NULL)
	virtual INetChannel* GetClientChannel() const = 0;

	//! Returns the (synched) time of the server (so use this for timed events, such as MP round times)
	virtual CTimeValue GetServerTime() = 0;

	//! Retrieve the Game Server Channel Id associated with the specified INetChannel.
	//! \return The Game Server ChannelId associated with the specified INetChannel.
	virtual uint16 GetGameChannelId(INetChannel* pNetChannel) = 0;

	//! Check if the game server channel has lost connection but still on hold and able to recover...
	//! \return true if the specified game server channel has lost connection but it's stil able to recover...
	virtual bool IsChannelOnHold(uint16 channelId) = 0;

	//! Retrieve a pointer to the INetChannel associated with the specified Game Server Channel Id.
	//! \return Pointer to INetChannel associated with the specified Game Server Channel Id.
	virtual INetChannel* GetNetChannel(uint16 channelId) = 0;

	// HACK: CNetEntity calls this when binding a player's entity to the network.
	virtual void SetServerChannelPlayerId(uint16 channelId, EntityId id) = 0;

	// TODO: Move profiles into CNetEntity and get rid of this.
	virtual const SEntitySchedulingProfiles* GetEntitySchedulerProfiles(IEntity* pEnt) = 0;

	//! Retrieve an IGameObject from an entity id
	//! \return Pointer to IGameObject of the entity if it exists (or NULL otherwise)
	virtual IGameObject* GetGameObject(EntityId id) = 0;

	//! Retrieve a network safe entity class id, that will be the same in client and server
	//! \return true if an entity class with this name has been registered
	virtual bool GetNetworkSafeClassId(uint16& id, const char* className) = 0;

	//! Retrieve a network safe entity class name, that will be the same in client and server
	//! \return true if an entity class with this id has been registered
	virtual bool GetNetworkSafeClassName(char* className, size_t maxn, uint16 id) = 0;

	//! Retrieve an IGameObjectExtension by name from an entity
	//! \return Pointer to IGameObjectExtension of the entity if it exists (or NULL otherwise)
	virtual IGameObjectExtension* QueryGameObjectExtension(EntityId id, const char* name) = 0;

	//! Retrieve pointer to the ITimeDemoRecorder (or NULL)
	virtual ITimeDemoRecorder* GetITimeDemoRecorder() const = 0;

	//! Save the current game to disk
	virtual bool SaveGame(const char* path, bool quick = false, bool bForceImmediate = true, ESaveGameReason reason = eSGR_QuickSave, bool ignoreDelay = false, const char* checkPoint = NULL) = 0;

	//! Load a game from disk (calls StartGameContext...)
	virtual ELoadGameResult LoadGame(const char* path, bool quick = false, bool ignoreDelay = false) = 0;

	virtual TSaveGameName CreateSaveGameName() = 0;

	virtual void ScheduleEndLevel(const char* nextLevel) = 0;

	//! Schedules the level load for the next level
	virtual void ScheduleEndLevelNow(const char* nextLevel) = 0;

	//! Notification that game mode is being entered/exited
	//! iMode values: 0-leave game mode, 1-enter game mode, 3-leave AI/Physics mode, 4-enter AI/Physics mode
	virtual void OnEditorSetGameMode(int iMode) = 0;

	virtual bool IsEditing() = 0;

	virtual bool IsInLevelLoad() = 0;

	virtual bool IsLoadingSaveGame() = 0;

	virtual bool IsInTimeDemo() = 0;
	virtual bool IsTimeDemoRecording() = 0;

	virtual void AllowSave(bool bAllow = true) = 0;
	virtual void AllowLoad(bool bAllow = true) = 0;
	virtual bool CanSave() = 0;
	virtual bool CanLoad() = 0;

	//! Gets a serialization helper for read/write usage based on settings
	virtual ISerializeHelper* GetSerializeHelper() const = 0;

	//! Check if the current game can activate cheats (flymode, godmode, nextspawn)
	virtual bool CanCheat() = 0;

	//! \return Path relative to the levels folder e.g. "Multiplayer\PS\Shore".
	virtual const char* GetLevelName() = 0;

	// \return pPathBuffer[0] == 0 if no level is loaded.
	virtual void              GetAbsLevelPath(char* pPathBuffer, uint32 pathBufferSize) = 0;

	virtual IPersistantDebug* GetIPersistantDebug() = 0;

	//! Adds a listener for break events.
	virtual void AddBreakEventListener(IBreakEventListener* pListener) = 0;

	//! Removes a listener for break events.
	virtual void                  RemoveBreakEventListener(IBreakEventListener* pListener) = 0;

	virtual void                  RegisterListener(IGameFrameworkListener* pGameFrameworkListener, const char* name, EFRAMEWORKLISTENERPRIORITY eFrameworkListenerPriority) = 0;
	virtual void                  UnregisterListener(IGameFrameworkListener* pGameFrameworkListener) = 0;

	virtual INetNub*              GetServerNetNub() = 0;
	virtual IGameServerNub*       GetIGameServerNub() = 0;
	virtual INetNub*              GetClientNetNub() = 0;
	virtual IGameClientNub*       GetIGameClientNub() = 0;

	virtual void                  SetGameGUID(const char* gameGUID) = 0;
	virtual const char*           GetGameGUID() = 0;
	virtual INetContext*          GetNetContext() = 0;

	virtual void                  GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual void                  EnableVoiceRecording(const bool enable) = 0;

	virtual void                  MutePlayerById(EntityId mutePlayer) = 0;

	virtual IDebugHistoryManager* CreateDebugHistoryManager() = 0;

	virtual void                  DumpMemInfo(const char* format, ...) PRINTF_PARAMS(2, 3) = 0;

	//! Check whether the client actor is using voice communication.
	virtual bool IsVoiceRecordingEnabled() = 0;

	virtual bool IsImmersiveMPEnabled() = 0;

	//! Executes console command on next frame's beginning
	virtual void        ExecuteCommandNextFrame(const char*) = 0;

	virtual const char* GetNextFrameCommand() const = 0;

	virtual void        ClearNextFrameCommand() = 0;

	//! Opens a page in default browser.
	virtual void ShowPageInBrowser(const char* URL) = 0;

	//! Opens a page in default browser.
	virtual bool StartProcess(const char* cmd_line) = 0;

	//! Saves dedicated server console variables in server config file.
	virtual bool SaveServerConfig(const char* path) = 0;

	//! To avoid stalls during gameplay and to get a list of all assets needed for the level (bEnforceAll=true).
	//! \param bEnforceAll true to ensure all possible assets become registered (list should not be too conservative - to support level stripification).
	virtual void PrefetchLevelAssets(const bool bEnforceAll) = 0;

	virtual void ReleaseGameStats() = 0;

	//! Inform that an IEntity was spawned from breakage.
	virtual void OnBreakageSpawnedEntity(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity) = 0;

	//! Returns true if the supplied game session is a game session.
	virtual bool IsGameSession(CrySessionHandle sessionHandle) = 0;

	//! Returns true if the nub should be migrated for a given session.
	virtual bool ShouldMigrateNub(CrySessionHandle sessionHandle) = 0;

	//! Adds a timer that will trigger a callback function passed by parameter.
	//! Allows to pass some user data pointer that will be one of the parameters for the callback function.
	//! The signature for the callback function is: void (void*, int).
	//! It allows member functions by using CE functors.
	//! \return Handle of the timer created
	virtual IGameFramework::TimerID AddTimer(CTimeValue interval, bool repeat, TimerCallback callback, void* userdata = 0) = 0;

	//! Remove an existing timer by using its handle, returns user data.
	virtual void* RemoveTimer(TimerID timerID) = 0;

	//! Return ticks last preupdate took.
	virtual uint32 GetPreUpdateTicks() = 0;

	//! Get the time left when we are allowed to load a new game.
	//! When this returns 0, we are allowed to load a new game.
	virtual float GetLoadSaveDelay() const = 0;

	//! Allows the network code to keep ticking in the event of a stall on the main thread.
	virtual void StartNetworkStallTicker(bool includeMinimalUpdate) = 0;
	virtual void StopNetworkStallTicker() = 0;

	//! Retrieves manager which handles game objects tied to editor shapes and volumes.
	virtual IGameVolumes* GetIGameVolumesManager() const = 0;

	virtual void          PreloadAnimatedCharacter(IScriptTable* pEntityScript) = 0;

	//! Gets called from the physics thread just before doing a time step.
	//! \param deltaTime - the time interval that will be simulated.
	virtual void PrePhysicsTimeStep(float deltaTime) = 0;

	//! Register an extension to the game framework and makes it accessible through it
	//! \param pExtension Extension to be added to the game framework.
	virtual void RegisterExtension(ICryUnknownPtr pExtension) = 0;

	virtual void ReleaseExtensions() = 0;

	//! Retrieves an extension interface if registered with the framework
	template<typename ExtensionInterface>
	ExtensionInterface* QueryExtension() const
	{
		const CryInterfaceID interfaceId = cryiidof<ExtensionInterface>();
		return cryinterface_cast<ExtensionInterface>(QueryExtensionInterfaceById(interfaceId)).get();
	}

	virtual void AddNetworkedClientListener(INetworkedClientListener& listener) = 0;
	virtual void RemoveNetworkedClientListener(INetworkedClientListener& listener) = 0;

	virtual void DoInvokeRMI(_smart_ptr<IRMIMessageBody> pBody, unsigned where, int channel, const bool isGameObjectRmi) = 0;

protected:
	//! Retrieves an extension interface by interface id.
	//! Internal, client uses 'QueryExtension<ExtensionInterface>()
	//! \param interfaceID Interface id.
	virtual ICryUnknownPtr QueryExtensionInterfaceById(const CryInterfaceID& interfaceID) const = 0;

	// </interfuscator:shuffle>
};

ILINE bool IsDemoPlayback()
{
	ISystem* pSystem = GetISystem();
	INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
	return pNetContext ? pNetContext->IsDemoPlayback() : false;
}
