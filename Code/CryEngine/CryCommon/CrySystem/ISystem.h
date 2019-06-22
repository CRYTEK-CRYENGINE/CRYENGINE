// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySchematyc/Utils/EnumFlags.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ICryUnknown.h>
#include "IValidator.h"
#include "ILog.h"
#include <memory>
#include <CryCore/CryEnumMacro.h>

#ifdef CRYSYSTEM_EXPORTS
	#define CRYSYSTEM_API DLL_EXPORT
#else
	#define CRYSYSTEM_API DLL_IMPORT
#endif

class CCamera;
class CPNoise3;
class CRndGen;
class ICmdLine;
class ICrySizer;
class IDiskProfiler;
class IXMLBinarySerializer;
class XmlNodeRef;

struct CryGUID;
struct I3DEngine;
struct IAISystem;
struct IAVI_Reader;
struct IBudgetingSystem;
struct ICharacterManager;
struct ICodeCheckpointMgr;
struct IConsole;
struct ICryFactoryRegistry;
struct ICryFont;
struct ICryLobby;
struct ICryPak;
struct ICryPerfHUD;
struct ICrySchematycCore;
struct IDialogSystem;
struct IEntity;
struct IEntitySystem;
struct IFileChangeMonitor;
struct IFlash;
struct IFlashLoadMovieHandler;
struct IFlashPlayer;
struct IFlashPlayerBootStrapper;
struct IFlashUI;
struct IFlowSystem;
struct IGameFramework;
struct IGameStartup;
struct IHardwareMouse;
struct IHmdDevice;
struct IImeManager;
struct IInput;
struct IKeyboard;
struct ILocalizationManager;
struct ILocalMemoryUsage;
struct ILZ4Decompressor;
struct IManualFrameStepController;
struct IMaterialEffects;
struct IMemoryManager;
struct IMonoEngineModule;
struct IMouse;
struct IMovieSystem;
struct INameTable;
struct INetContext;
struct INetwork;
struct INotificationNetwork;
struct IOpticsManager;
struct IOutputPrintSink;
struct IOverloadSceneManager;
struct IParticleManager;
struct IPhysicalWorld;
struct IPhysicsDebugRenderer;
struct IPhysRenderer;
struct IPlatformOS;
struct IProcess;
struct IReadWriteXMLSink;
struct IRemoteCommandManager;
struct IRemoteConsole;
struct IRenderAuxGeom;
struct IRenderer;
struct IResourceManager;
struct IScaleformHelper;
struct IScriptSystem;
struct IServiceNetwork;
struct IStatoscope;
struct IStreamEngine;
struct ISystem;
struct ITextModeConsole;
struct IThreadManager;
struct ITimer;
struct IUserAnalyticsSystem;
struct IValidator;
struct IWindowMessageHandler;
struct IXmlUtils;
struct IZLibCompressor;
struct IZLibDecompressor;
struct SDisplayContextKey;
struct SGraphicsPipelineKey;
struct SFileVersion;
struct ICryProfilingSystem;
struct ILegacyProfiler;

namespace CryAudio
{
struct IAudioSystem;
}

namespace Telemetry
{
struct ITelemetrySystem;
}

namespace Cry {

namespace Reflection {
struct IModule;
}   // ~Reflection namespace

namespace Script {
struct ICoreEnvironment;
struct ICoreRegistry;
}   // ~Script namespace

namespace UDR {
struct IUDRSystem;
}

struct IPluginManager;
struct IProjectManager;

} // ~Cry namespace

namespace DRS {
struct IDynamicResponseSystem;
}

namespace CryTest {
struct ITestSystem;
}

namespace UIFramework
{
struct IUIFramework;
}

namespace Serialization {
struct IArchiveHost;
}

namespace LiveCreate
{
struct IManager;
struct IHost;
}

namespace JobManager {
struct IJobManager;
}

namespace Schematyc2
{
struct IFramework;
}

namespace minigui
{
struct IMiniGUI;
}

typedef CryGUID CryInterfaceID;
typedef void*   CRY_HWND;

#define PROC_MENU     1
#define PROC_3DENGINE 2

// Maybe they should be moved into the game.dll .
//! IDs for script userdata typing.
//! ##@{
#define USER_DATA_SOUND       1
#define USER_DATA_TEXTURE     2
#define USER_DATA_OBJECT      3
#define USER_DATA_LIGHT       4
#define USER_DATA_BONEHANDLER 5
#define USER_DATA_POINTER     6
//! ##@}.

enum ESystemUpdateFlags : uint8
{
	ESYSUPDATE_IGNORE_AI         = BIT8(0),
	ESYSUPDATE_IGNORE_PHYSICS    = BIT8(2),
	//! Special update mode for editor.
	ESYSUPDATE_EDITOR            = BIT8(3),
	ESYSUPDATE_MULTIPLAYER       = BIT8(4),
	ESYSUPDATE_EDITOR_AI_PHYSICS = BIT8(5),
	ESYSUPDATE_EDITOR_ONLY       = BIT8(6)
};

//! Configuration specification, depends on user selected machine specification.
enum ESystemConfigSpec
{
	CONFIG_CUSTOM        = 0, //!< Should always be first.
	CONFIG_LOW_SPEC      = 1,
	CONFIG_MEDIUM_SPEC   = 2,
	CONFIG_HIGH_SPEC     = 3,
	CONFIG_VERYHIGH_SPEC = 4,

	CONFIG_DURANGO       = 5, //! Xbox One (Potato)
	CONFIG_DURANGO_X     = 6, //! Xbox One X
	CONFIG_ORBIS         = 7,

	//! Specialized detail config setting.
	CONFIG_DETAIL_SPEC = 8,

	END_CONFIG_SPEC_ENUM, //!< Must be the last value, used for error checking.
};

enum ESubsystem
{
	ESubsys_3DEngine = 0,
	ESubsys_AI       = 1,
	ESubsys_Physics  = 2,
	ESubsys_Renderer = 3,
	ESubsys_Script   = 4
};

//! \cond INTERNAL
//! Collates cycles taken per update.
struct sUpdateTimes
{
	uint32 PhysYields;
	uint64 SysUpdateTime;
	uint64 PhysStepTime;
	uint64 RenderTime;
	uint64 physWaitTime;  //!< Extended yimes info.
	uint64 streamingWaitTime;
	uint64 animationWaitTime;
};
//! \endcond

enum ESystemGlobalState
{
	ESYSTEM_GLOBAL_STATE_INIT,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_PREPARE,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_MATERIALS,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_OBJECTS,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_CHARACTERS,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_STATIC_WORLD,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_ENTITIES,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_PRECACHE,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_TEXTURES,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_ENDING,
	ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_COMPLETE,
	ESYSTEM_GLOBAL_STATE_RUNNING,
};

//! System wide events.
enum ESystemEvent
{
	//! Changes to main window focus.
	//! wparam is not 0 is focused, 0 if not focused.
	ESYSTEM_EVENT_CHANGE_FOCUS = 10,

	//! Moves of the main window.
	//! wparam=x, lparam=y.
	ESYSTEM_EVENT_MOVE = 11,

	//! Resizes of the main window.
	//! wparam=width, lparam=height.
	ESYSTEM_EVENT_RESIZE = 12,

	//! Activation of the main window.
	//! wparam=1/0, 1=active 0=inactive.
	ESYSTEM_EVENT_ACTIVATE = 13,

	//! Main window position changed.
	ESYSTEM_EVENT_POS_CHANGED = 14,

	//! Main window style changed.
	ESYSTEM_EVENT_STYLE_CHANGED = 15,

	//! Sent before the loading movie is begun.
	ESYSTEM_EVENT_LEVEL_LOAD_START_PRELOADINGSCREEN,

	//! Sent before the loading last save.
	ESYSTEM_EVENT_LEVEL_LOAD_RESUME_GAME,

	//! Sent before starting level, before game rules initialization and before ESYSTEM_EVENT_LEVEL_LOAD_START event.
	//! Used mostly for level loading profiling
	ESYSTEM_EVENT_LEVEL_LOAD_PREPARE,

	//! Sent to start the active loading screen rendering.
	ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN,

	//! Sent when loading screen is active.
	ESYSTEM_EVENT_LEVEL_LOAD_LOADINGSCREEN_ACTIVE,

	//! Sent before starting loading a new level.
	//! Used for a more efficient resource management.
	ESYSTEM_EVENT_LEVEL_LOAD_START,

	//! Sent before loading entities from disk
	ESYSTEM_EVENT_LEVEL_LOAD_ENTITIES,

	//! Sent after loading a level finished.
	//! Used for a more efficient resource management.
	ESYSTEM_EVENT_LEVEL_LOAD_END,

	//! Sent after trying to load a level failed.
	//! Used for resetting the front end.
	ESYSTEM_EVENT_LEVEL_LOAD_ERROR,

	//! Sent in case the level was requested to load, but it's not ready.
	//! Used in streaming install scenario for notifying the front end.
	ESYSTEM_EVENT_LEVEL_NOT_READY,

	//! Sent after precaching of the streaming system has been done.
	ESYSTEM_EVENT_LEVEL_PRECACHE_START,

	//! Sent before object/texture precache stream requests are submitted.
	ESYSTEM_EVENT_LEVEL_PRECACHE_FIRST_FRAME,

	//! Sent when level loading is completely finished with no more onscreen movie or info rendering, and when actual gameplay can start.
	ESYSTEM_EVENT_LEVEL_GAMEPLAY_START,

	//! Level is unloading.
	ESYSTEM_EVENT_LEVEL_UNLOAD,

	//! Sent after level have been unloaded. For cleanup code.
	ESYSTEM_EVENT_LEVEL_POST_UNLOAD,

	//! Called when the game framework has been initialized.
	ESYSTEM_EVENT_GAME_POST_INIT,

	//! Called when the game framework has been initialized, not loading should happen in this event.
	ESYSTEM_EVENT_GAME_POST_INIT_DONE,

	//! Called when the Sandbox has finished initialization
	ESYSTEM_EVENT_SANDBOX_POST_INIT_DONE,

	//! Sent when the system is doing a full shutdown.
	ESYSTEM_EVENT_FULL_SHUTDOWN,

	//! Sent when the system is doing a fast shutdown.
	ESYSTEM_EVENT_FAST_SHUTDOWN,

	//! When keyboard layout changed.
	ESYSTEM_EVENT_LANGUAGE_CHANGE,

	//! Toggle fullscreen.
	//! wparam is 1 means we switched to fullscreen, 0 if for windowed.
	ESYSTEM_EVENT_TOGGLE_FULLSCREEN,
	ESYSTEM_EVENT_SHARE_SHADER_COMBINATIONS,

	//! Start 3D post rendering.
	ESYSTEM_EVENT_3D_POST_RENDERING_START,

	//! End 3D post rendering.
	ESYSTEM_EVENT_3D_POST_RENDERING_END,

	//! Sent after precaching of the streaming system has been done.
	ESYSTEM_EVENT_LEVEL_PRECACHE_END,

	//! Sent when game mode switch begins.
	ESYSTEM_EVENT_GAME_MODE_SWITCH_START,

	//! Sent when game mode switch ends.
	ESYSTEM_EVENT_GAME_MODE_SWITCH_END,

	//! Video notifications.
	//! wparam=[0/1/2/3] : [stop/play/pause/resume].
	ESYSTEM_EVENT_VIDEO,

	//! Sent if the game is paused.
	ESYSTEM_EVENT_GAME_PAUSED,

	//! Sent if the game is resumed.
	ESYSTEM_EVENT_GAME_RESUMED,

	//! Sent when time of day is set.
	ESYSTEM_EVENT_TIME_OF_DAY_SET,

	//! Sent once the Editor finished initialization.
	ESYSTEM_EVENT_EDITOR_ON_INIT,

	//! Sent when frontend is initialized.
	ESYSTEM_EVENT_FRONTEND_INITIALISED,

	//! Sent once the Editor switches between in-game and editing mode.
	ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED,

	//! Sent once the Editor switches simulation mode (AI/Physics).
	ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED,

	//! Sent when environment settings are reloaded
	ESYSTEM_EVENT_ENVIRONMENT_SETTINGS_CHANGED,

	//! Sent when frontend is reloaded.
	ESYSTEM_EVENT_FRONTEND_RELOADED,

#if CRY_PLATFORM_DURANGO
	// Description: PLM (Process Life Management) events.
	ESYSTEM_EVENT_PLM_ON_RESUMING,
	ESYSTEM_EVENT_PLM_ON_SUSPENDING,
	ESYSTEM_EVENT_PLM_ON_CONSTRAINED,
	ESYSTEM_EVENT_PLM_ON_FULL,  //!< Back from constrained to full resources.
	ESYSTEM_EVENT_PLM_ON_TERMINATED,

	ESYSTEM_EVENT_PLM_ON_SUSPENDING_COMPLETED, //!< Safe to call Deferral's Complete.

	//! Durango only so far. They are triggered by voice/gesture standard commands.
	ESYSTEM_EVENT_GLOBAL_SYSCMD_PAUSE,
	ESYSTEM_EVENT_GLOBAL_SYSCMD_SHOW_MENU,
	ESYSTEM_EVENT_GLOBAL_SYSCMD_PLAY,
	ESYSTEM_EVENT_GLOBAL_SYSCMD_BACK,
	ESYSTEM_EVENT_GLOBAL_SYSCMD_CHANGE_VIEW,

	//! ESYSTEM_EVENT_DURANGO_CHANGE_VISIBILITY is called when the app totally disappears or reappears.
	//! ESYSTEM_EVENT_CHANGE_FOCUS is called in that situation, but also when the app is constantly visible and only the focus changes.
	//! wparam = 0 -> visibility lost,  wparam = 1 -> visibility recovered.
	ESYSTEM_EVENT_DURANGO_CHANGE_VISIBILITY,

	//! Matchmaking system events.
	//! wParam on state change is match search state.
	//! lParam unused.
	ESYSTEM_EVENT_MATCHMAKING_SEARCH_STATE_CHANGE,

	//! WParam and lParam unused.
	ESYSTEM_EVENT_MATCHMAKING_GAME_SESSION_READY,

	//! Triggers when current user has been changed or started being signed out.
	ESYSTEM_EVENT_USER_SIGNOUT_STARTED,
	ESYSTEM_EVENT_USER_SIGNOUT_COMPLETED,

	//! Triggers when the user has completed the sign in process.
	ESYSTEM_EVENT_USER_SIGNIN_COMPLETED,

	ESYSTEM_EVENT_USER_ADDED,
	ESYSTEM_EVENT_USER_REMOVED,
	ESYSTEM_EVENT_USER_CHANGED,

	//! Durango only so far. Triggers when the controller pairing changes, e.g. change user in account picker or handing over the controller to another user.
	ESYSTEM_EVENT_CONTROLLER_PAIRING_CHANGED,
	ESYSTEM_EVENT_CONTROLLER_REMOVED,
	ESYSTEM_EVENT_CONTROLLER_ADDED,
#endif

	//! Currently durango only.
	//! Triggers when streaming install had failed to open newly received pak files
	//! + may be triggered on platform error as well: like scratched disks or network problems
	//! when installing from store
	ESYSTEM_EVENT_STREAMING_INSTALL_ERROR,

	//! Sent when the online services are initialized.
	ESYSTEM_EVENT_ONLINE_SERVICES_INITIALISED,

	//! Sent when a new audio implementation is loaded.
	ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED,

	//! Sent to inform the audio system to mute.
	ESYSTEM_EVENT_AUDIO_MUTE,

	//! Sent to inform the audio system to unmute.
	ESYSTEM_EVENT_AUDIO_UNMUTE,

	//! Sent to inform the audio system to pause.
	ESYSTEM_EVENT_AUDIO_PAUSE,

	//! Sent to inform the audio system to resume.
	ESYSTEM_EVENT_AUDIO_RESUME,

	//! Sent to inform the audio system to refresh.
	ESYSTEM_EVENT_AUDIO_REFRESH,

	//! Sent when the audio language has changed.
	ESYSTEM_EVENT_AUDIO_LANGUAGE_CHANGED,

	//! Purpose of this event is to enable different modules to communicate with each other without knowing about each other.
	ESYSTEM_EVENT_URI,

	ESYSTEM_EVENT_BEAM_PLAYER_TO_CAMERA_POS,

	//! Sent if the CrySystem module initialized successfully.
	ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE,

	//! Sent before initializing the renderer.
	ESYSTEM_EVENT_PRE_RENDERER_INIT,

	//! Sent if the window containing the running game loses focus, but application itself still has focus
	//! This is needed because some sub-systems still want to work even without focus on main application
	//! while others would prefer to suspend their operation
	ESYSTEM_EVENT_GAMEWINDOW_ACTIVATE,

	//! Called when the display resolution has changed.
	ESYSTEM_EVENT_DISPLAY_CHANGED,

	//! Called when the hardware configuration has changed.
	ESYSTEM_EVENT_DEVICE_CHANGED,

	// Sent when flow nodes should be registered
	ESYSTEM_EVENT_REGISTER_FLOWNODES,

	//! Sent if the CryAction module initialized successfully. (Remark: Sent after ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE and after (potential) game init was called)
	ESYSTEM_EVENT_GAME_FRAMEWORK_INIT_DONE,

	//! Sent if the CryAction module is about to shutdown
	ESYSTEM_EVENT_GAME_FRAMEWORK_ABOUT_TO_SHUTDOWN,


	//! Event IDs from this value upwards can be used to define custom events.
	//! Should always be the last value!
	ESYSTEM_EVENT_USER,
};

//! User defined callback, which can be passed to ISystem.
struct ISystemUserCallback
{
	// <interfuscator:shuffle>
	virtual ~ISystemUserCallback(){}

	//! This method is called at the earliest point the ISystem pointer can be used the log might not be yet there.
	virtual void OnSystemConnect(ISystem* pSystem) {}

	//! If working in Editor environment notify user that engine want to Save current document.
	//! This happens if critical error have occurred and engine gives a user way to save data and not lose it due to crash.
#if CRY_PLATFORM_WINDOWS
	virtual bool OnSaveDocument() = 0;
#endif

	//! Notifies user that system wants to switch out of current process.
	//! Example: Called when pressing ESC in game mode to go to Menu.
	virtual void OnProcessSwitch() = 0;

	//! Notifies user, usually editor, about initialization progress in system.
	virtual void OnInitProgress(const char* sProgressMsg) = 0;

	//! Initialization callback.
	//! This is called early in CSystem::Init(), before any of the other callback methods is called.
	virtual void OnInit(ISystem*) {}

	//! Shutdown callback.
	virtual void OnShutdown() {}

	//! Quit callback.
	virtual void OnQuit() {}

	//! Notify user of an update iteration. Called in the update loop.
	virtual void OnUpdate() {}

	//! Show message by provider.
	virtual EQuestionResult ShowMessage(const char* text, const char* caption, EMessageBox uType) { return eQR_None; }

	//! Collects the memory information in the user program/application.
	virtual void GetMemoryUsage(ICrySizer* pSizer) = 0;
	// </interfuscator:shuffle>
};

//! Interface used for getting notified when a system event occurs.
struct ISystemEventListener
{
	// <interfuscator:shuffle>
	virtual ~ISystemEventListener(){}
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) = 0;
	// </interfuscator:shuffle>
};

//! Structure used for getting notified when a system event occurs.
struct ISystemEventDispatcher
{
	// <interfuscator:shuffle>
	virtual ~ISystemEventDispatcher(){}
	virtual bool RegisterListener(ISystemEventListener* pListener, const char* szName) = 0;
	virtual bool RemoveListener(ISystemEventListener* pListener) = 0;

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam, bool force_queue = false) = 0;
	virtual void Update() = 0;

	//virtual void OnLocaleChange() = 0;
	// </interfuscator:shuffle>
};

struct IErrorObserver
{
	// <interfuscator:shuffle>
	virtual ~IErrorObserver() {}
	virtual void OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber) = 0;
	virtual void OnFatalError(const char* message) = 0;
	// </interfuscator:shuffle>
};

enum ESystemProtectedFunctions
{
	eProtectedFunc_Save = 0,
	eProtectedFunc_Load = 1,
	eProtectedFuncsLast = 10,
};

struct SCvarsDefault
{
	SCvarsDefault()
	{
		sz_r_DriverDef = NULL;
	}

	const char* sz_r_DriverDef;
};

#if CRY_PLATFORM_DURANGO
struct SControllerPairingChanged
{
	struct SUserIdInfo
	{
		uint32 currentUserId;
		uint32 previousUserId;
	};

	SControllerPairingChanged(uint64 _raw) : raw(_raw) {}
	SControllerPairingChanged(uint32 currentUserId_ = 0, uint32 previousUserId_ = 0)
	{
		userInfo.currentUserId = currentUserId_;
		userInfo.previousUserId = previousUserId_;
	}

	union
	{
		SUserIdInfo userInfo;
		uint64      raw;
	};
};
#endif

//! Structure passed to Init method of ISystem interface.
struct SSystemInitParams
{
	void*                hWnd;
	ILog*                pLog;           //!< You can specify your own ILog to be used by System.
	ILogCallback*        pLogCallback;   //!< You can specify your own ILogCallback to be added on log creation (used by Editor).
	ISystemUserCallback* pUserCallback;
	const char*          sLogFileName;          //!< File name to use for log.
	IValidator*          pValidator;            //!< You can specify different validator object to use by System.
	IOutputPrintSink*    pPrintSync;            //!< Print Sync which can be used to catch all output from engine.
	char                 szSystemCmdLine[2048]; //!< Command line.
	char                 szUserPath[256];       //!< User alias path relative to My Documents folder.
	char                 szBinariesDir[256];

#if !defined(RELEASE)
	bool bEditor = false;                       //!< When running in Editor mode.
#endif

	bool bPreview;                            //!< When running in Preview mode (Minimal initialization).
	bool bDedicatedServer;                    //!< When running a dedicated server.
	bool bExecuteCommandLine;                 //!< can be switched of to suppress the feature or do it later during the initialization.
	bool bUIFramework;
	bool bSkipFont;                            //!< Don't load CryFont.dll.
	bool bSkipRenderer;                        //!< Don't load Renderer.
	bool bSkipNetwork;                         //!< Don't create Network.
	bool bSkipLiveCreate;                      //!< Don't create LiveCreate.
	bool bSkipWebsocketServer;                 //!< Don't create the WebSocket server.
	bool bMinimal;                             //!< Don't load banks.
	bool bSkipInput;                           //!< do not load CryInput.
	bool bTesting;                             //!< When running CryUnitTest.
	bool bNoRandom;                            //!< use fixed generator init/seed.
	bool bShaderCacheGen;                      //!< When running in shadercache gen mode.
	bool bUnattendedMode;                      //!< When running as part of a build on build-machines: Prevent popping up of any dialog.

#if CRY_PLATFORM_DURANGO
	const EPLM_Event* pLastPLMEvent;
	Windows::ApplicationModel::Activation::IActivatedEventArgs^ pFirstActivatedEventArgs;
#endif

	ISystem* pSystem;                     //!< Pointer to existing ISystem interface, it will be reused if not NULL.
	//! Char szLocalIP[256];              //! local IP address (needed if we have several servers on one machine).
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	void (*pCheckFunc)(void*);             //!< Authentication function (must be set).
#else
	void* pCheckFunc;                     //!< Authentication function (must be set).
#endif

	typedef void* (* ProtectedFunction)(void* param1, void* param2);
	ProtectedFunction pProtectedFunctions[eProtectedFuncsLast];  //!< Protected functions.

	SCvarsDefault*    pCvarsDefault;      //!< To override the default value of some cvar.

	//! Initialization defaults.
	SSystemInitParams()
	{
		hWnd = nullptr;
		pLog = nullptr;
		pLogCallback = nullptr;
		pUserCallback = nullptr;
		sLogFileName = nullptr;
		pValidator = nullptr;
		pPrintSync = nullptr;
		memset(szSystemCmdLine, 0, sizeof(szSystemCmdLine));
		memset(szUserPath, 0, sizeof(szUserPath));
		memset(szBinariesDir, 0, sizeof(szBinariesDir));
		bPreview = false;
		bDedicatedServer = false;
		bExecuteCommandLine = true;
		bUIFramework = false;
		bSkipFont = false;
		bSkipRenderer = false;
		bSkipNetwork = false;
#if CRY_PLATFORM_WINDOWS
		// create websocket server by default. bear in mind that USE_HTTP_WEBSOCKETS is not defined in release.
		bSkipWebsocketServer = false;
#else
		// CTCPStreamSocket does not support XBOX ONE and PS4 at all, and some of its functionality only seems to support Win32 and 64
		bSkipWebsocketServer = true;
#endif
		bMinimal = false;
		bSkipInput = false;
		bSkipLiveCreate = false;
		bTesting = false;
		bNoRandom = false;
		bShaderCacheGen = false;
		bUnattendedMode = false;

		pSystem = nullptr;
		pCheckFunc = nullptr;

#if CRY_PLATFORM_DURANGO
		pLastPLMEvent = nullptr;
#endif

		memset(pProtectedFunctions, 0, sizeof(pProtectedFunctions));
		pCvarsDefault = nullptr;
	}
};

//! \cond INTERNAL
//! \note Can be used for LoadConfiguration().
struct ILoadConfigurationEntrySink
{
	// <interfuscator:shuffle>
	virtual ~ILoadConfigurationEntrySink(){}
	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup) = 0;
	virtual void OnLoadConfigurationEntry_End() {}
	// </interfuscator:shuffle>
};
//! \endcond

enum ELoadConfigurationType
{
	eLoadConfigDefault,
	eLoadConfigInit,
	eLoadConfigSystemSpec,
	eLoadConfigGame,
	eLoadConfigLevel
};

enum class ELoadConfigurationFlags : uint32
{
	None                          = 0,
	SuppressConfigNotFoundWarning = BIT(0)
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ELoadConfigurationFlags);

struct SPlatformInfo
{
	const char*  szProcessorType = nullptr;
	unsigned int numCoresAvailableToProcess = 0;
	unsigned int numLogicalProcessors = 0;

#if CRY_PLATFORM_WINDOWS
	enum EWinVersion
	{
		WinUndetected,
		Win2000,
		WinXP,
		WinSrv2003,
		WinVista,
		Win7,
		Win8,
		Win8Point1,
		Win10
	};

	struct SWinInfo
	{
		char        path[_MAX_PATH] = { '\0' };
		EWinVersion ver = EWinVersion::WinUndetected;
		uint32_t    build = 0;
		bool        is64Bit = false;
		bool        vistaKB940105Required = false;
	};

	SWinInfo winInfo;
#endif
};

/// Cpu Features
#define CPUF_FPUEMULATION 1
#define CPUF_FP16         2
#define CPUF_MMX          4
#define CPUF_3DNOW        8
#define CPUF_SSE          0x10
#define CPUF_SSE2         0x20
#define CPUF_SSE3         0x40
#define CPUF_SSE4         0x80
#define CPUF_AVX          0x100
#define CPUF_AVX2         0x200
#define CPUF_FMA          0x400

//! \cond INTERNAL
//! Holds info about system update stats over period of time (cvar-tweakable)
struct SSystemUpdateStats
{
	SSystemUpdateStats() : avgUpdateTime(0.0f), minUpdateTime(0.0f), maxUpdateTime(0.0f), avgUpdateRate(0.0f) {}
	float avgUpdateTime;
	float minUpdateTime;
	float maxUpdateTime;
	float avgUpdateRate;
};

//! Union to handle communication between the AsycDIP jobs and the general job system.
//! To allow usage of CAS all informations are encoded in 32 bit.
union UAsyncDipState
{
	struct
	{
		uint32 nQueueGuard  : 1;
		uint32 nWorker_Idle : 4;
		uint32 nNumJobs     : 27;
	};
	uint32 nValue;
};
//! \endcond

//!	Global environment. Contains pointers to all global often needed interfaces.
//!	This is a faster way to get interface pointer then calling ISystem interface to retrieve one.
//! \note Some pointers can be NULL, use with care.
//! \see ISystem
struct SSystemGlobalEnvironment
{
	IDialogSystem*                 pDialogSystem = nullptr;
	I3DEngine*                     p3DEngine = nullptr;
	INetwork*                      pNetwork = nullptr;
	INetContext*                   pNetContext = nullptr;
	ICryLobby*                     pLobby = nullptr;
	IScriptSystem*                 pScriptSystem = nullptr;
	IPhysicalWorld*                pPhysicalWorld = nullptr;
	IFlowSystem*                   pFlowSystem = nullptr;
	IInput*                        pInput = nullptr;
	IStatoscope*                   pStatoscope = nullptr;
	ICryPak*                       pCryPak = nullptr;
	IFileChangeMonitor*            pFileChangeMonitor = nullptr;
	IParticleManager*              pParticleManager = nullptr;
	IOpticsManager*                pOpticsManager = nullptr;
	ITimer*                        pTimer = nullptr;
	ICryFont*                      pCryFont = nullptr;
	IGameFramework*                pGameFramework = nullptr;
	ILocalMemoryUsage*             pLocalMemoryUsage = nullptr;
	IEntitySystem*                 pEntitySystem = nullptr;
	IConsole*                      pConsole = nullptr;
	CryAudio::IAudioSystem*        pAudioSystem = nullptr;
	ISystem*                       pSystem = nullptr;
	ICharacterManager*             pCharacterManager = nullptr;
	IAISystem*                     pAISystem = nullptr;
	ILog*                          pLog = nullptr;
	ICodeCheckpointMgr*            pCodeCheckpointMgr = nullptr;
	IMovieSystem*                  pMovieSystem = nullptr;
	INameTable*                    pNameTable = nullptr;
	IRenderer*                     pRenderer = nullptr;
	IRenderAuxGeom*                pAuxGeomRenderer = nullptr;
	IHardwareMouse*                pHardwareMouse = nullptr;
	IMaterialEffects*              pMaterialEffects = nullptr;
	JobManager::IJobManager*       pJobManager = nullptr;
	IOverloadSceneManager*         pOverloadSceneManager = nullptr;
	IFlashUI*                      pFlashUI = nullptr;
	UIFramework::IUIFramework*     pUIFramework = nullptr;
	IServiceNetwork*               pServiceNetwork = nullptr;
	IRemoteCommandManager*         pRemoteCommandManager = nullptr;
	DRS::IDynamicResponseSystem*   pDynamicResponseSystem = nullptr;
	IThreadManager*                pThreadManager = nullptr;
	IScaleformHelper*              pScaleformHelper = nullptr;
	ICrySchematycCore*             pSchematyc = nullptr;
	Schematyc2::IFramework*        pSchematyc2 = nullptr;
	Cry::Reflection::IModule*      pReflection = nullptr;
	Cry::Script::ICoreEnvironment* pScriptCoreEnv = nullptr;
	Cry::Script::ICoreRegistry*    pScriptCoreRegistry = nullptr;
	Cry::UDR::IUDRSystem*          pUDR = nullptr;

#if CRY_PLATFORM_DURANGO
	void*      pWindow = nullptr;
	EPLM_State ePLM_State = EPLM_UNDEFINED;
#endif

#if defined(MAP_LOADING_SLICING)
	ISystemScheduler*     pSystemScheduler = nullptr;
#endif
	LiveCreate::IManager* pLiveCreateManager = nullptr;
	LiveCreate::IHost*    pLiveCreateHost = nullptr;

	IMonoEngineModule*    pMonoRuntime = nullptr;

	threadID              mMainThreadId = THREADID_NULL;      //!< The main thread ID is used in multiple systems so should be stored globally.

	uint32                nMainFrameID = 0;

	const char*           szCmdLine = "";       //!< Startup command line.

	//! Generic debug string which can be easily updated by any system and output by the debug handler
	enum { MAX_DEBUG_STRING_LENGTH = 128 };
	char szDebugStatus[MAX_DEBUG_STRING_LENGTH] = {'\0'};

	//! Used to tell if this is a server/multiplayer instance
	bool bServer = false;
	bool bMultiplayer = false;
	bool bHostMigrating = false;

#if defined(CRY_PLATFORM_ORBIS) && (!defined(RELEASE) || defined(PERFORMANCE_BUILD))
	//! Specifies if we are on a PS4 development kit
	bool bPS4DevKit = false;
#endif

	//////////////////////////////////////////////////////////////////////////
	//! Profiling callback functions.
	typedef bool(*TProfilerSectionStartCallback)(struct SProfilingSection*);
	typedef void(*TProfilerSectionEndCallback)  (struct SProfilingSection*);
	typedef void(*TProfilerMarkerCallback)      (struct SProfilingMarker*);
	TProfilerSectionStartCallback startProfilingSection = nullptr;
	TProfilerSectionEndCallback   endProfilingSection = nullptr;
	TProfilerMarkerCallback       recordProfilingMarker = nullptr;
	//////////////////////////////////////////////////////////////////////////

	//! Whether we are running unattended, disallows message boxes and other blocking events that require human intervention
	bool bUnattendedMode = false;
	//! Whether we are unit testing
	bool bTesting = false;

	bool bNoRandomSeed = false;

#if defined(USE_CRY_ASSERT)
	Cry::Assert::Detail::SSettings assertSettings;
#endif

	SPlatformInfo pi;

	// Protected functions.
	SSystemInitParams::ProtectedFunction pProtectedFunctions[eProtectedFuncsLast] = {};

	//////////////////////////////////////////////////////////////////////////
	//! Flag to able to print out of memory condition
	bool             bIsOutOfMemory = false;
	bool             bIsOutOfVideoMemory = false;

	ILINE const bool IsClient() const
	{
#if defined(DEDICATED_SERVER)
		return false;
#elif CRY_PLATFORM_DESKTOP
		return bClient;
#else
		return true;
#endif
	}

	ILINE const bool IsDedicated() const
	{
#if !CRY_PLATFORM_DESKTOP
		return false;
#elif defined(DEDICATED_SERVER)
		return true;
#else
		return bDedicated;
#endif
	}

#if CRY_PLATFORM_DESKTOP
	#if !defined(RELEASE)
	ILINE void SetIsEditor(bool isEditor)
	{
		bEditor = isEditor;
	}

	ILINE void SetIsEditorGameMode(bool isEditorGameMode)
	{
		bEditorGameMode = isEditorGameMode;
	}

	ILINE void SetIsEditorSimulationMode(bool isEditorSimulationMode)
	{
		bEditorSimulationMode = isEditorSimulationMode;
	}
	#endif

	ILINE void SetIsDedicated(bool isDedicated)
	{
	#if !defined(DEDICATED_SERVER)
		bDedicated = isDedicated;
	#endif
	}

	ILINE void SetIsClient(bool isClient)
	{
	#if !defined(DEDICATED_SERVER)
		bClient = isClient;
	#endif
	}
#endif

	//! This way the compiler can strip out code for consoles.
	ILINE const bool IsEditor() const
	{
#if CRY_PLATFORM_DESKTOP && !defined(RELEASE)
		return bEditor;
#else
		return false;
#endif
	}

	ILINE const bool IsEditorGameMode() const
	{
#if CRY_PLATFORM_DESKTOP && !defined(RELEASE)
		return bEditorGameMode;
#else
		return false;
#endif
	}

	ILINE const bool IsEditorSimulationMode() const
	{
#if CRY_PLATFORM_DESKTOP && !defined(RELEASE)
		return bEditorSimulationMode;
#else
		return false;
#endif
	}

	ILINE const bool IsGameOrSimulation() const
	{
#if CRY_PLATFORM_DESKTOP && !defined(RELEASE)
		return !bEditor || bEditorGameMode || bEditorSimulationMode;
#else
		return true;
#endif
	}

	ILINE const bool IsEditing() const
	{
#if CRY_PLATFORM_DESKTOP && !defined(RELEASE)
		return bEditor && !bEditorGameMode;
#else
		return false;
#endif
	}

	ILINE const bool IsFMVPlaying() const
	{
		return m_isFMVPlaying;
	}

	ILINE void SetFMVIsPlaying(const bool isPlaying)
	{
		m_isFMVPlaying = isPlaying;
	}

	ILINE const bool IsCutscenePlaying() const
	{
		return m_isCutscenePlaying;
	}

	ILINE void SetCutsceneIsPlaying(const bool isPlaying)
	{
		m_isCutscenePlaying = isPlaying;
	}

#if defined(SYS_ENV_AS_STRUCT)
	//! Remove pointer indirection.
	ILINE SSystemGlobalEnvironment* operator->()
	{
		return this;
	}

	ILINE SSystemGlobalEnvironment& operator*()
	{
		return *this;
	}

	ILINE const bool operator!() const
	{
		return false;
	}

	ILINE operator bool() const
	{
		return true;
	}
#endif

	//! Getter function for jobmanager.
	ILINE JobManager::IJobManager* GetJobManager()
	{
		return pJobManager;
	}

#if CRY_PLATFORM_DESKTOP
	bool bDedicatedArbitrator = false;

private:
	#if !defined(RELEASE)
	bool bEditor = false;               //!< Engine is running under editor.
	bool bEditorGameMode = false;       //!< Engine is in editor game mode.
	bool bEditorSimulationMode = false; //!< Engine is in editor Physics/AI simulation mode.
	#endif

	#if !defined(DEDICATED_SERVER)
	bool bDedicated = false; //!< Engine is in dedicated.
	bool bClient = false;
	#endif
#endif

	bool m_isFMVPlaying = false;
	bool m_isCutscenePlaying = false;

public:
	SSystemGlobalEnvironment()
	{
		mAsyncDipState.nValue = 0;
	}

	CRY_ALIGN(64) UAsyncDipState mAsyncDipState;
};

// The ISystem interface that follows has a member function called 'GetUserName'. If we don't #undef'ine the same-named Win32 symbol here ISystem wouldn't even compile.
// There might be a better place for this.
#ifdef GetUserName
	#undef GetUserName
#endif

//! Main Engine Interface.
//! Initialize and dispatch all engine's subsystems.
struct ISystem
{
	struct ILoadingProgressListener
	{
		// <interfuscator:shuffle>
		virtual ~ILoadingProgressListener(){}
		virtual void OnLoadingProgress(int steps) = 0;
		// </interfuscator:shuffle>
	};

#if !defined(RELEASE)
	enum LevelLoadOrigin
	{
		eLLO_Unknown,
		eLLO_NewLevel,
		eLLO_Level2Level,
		eLLO_Resumed,
		eLLO_MapCmd,
	};

	struct ICheckpointData
	{
		int             m_totalLoads;
		LevelLoadOrigin m_loadOrigin;
	};
#endif

	// <interfuscator:shuffle>
	virtual ~ISystem(){}

	//! Will return NULL if no whitelisting.
	virtual ILoadConfigurationEntrySink* GetCVarsWhiteListConfigSink() const = 0;

	//! Returns pointer to the global environment structure.
	virtual SSystemGlobalEnvironment* GetGlobalEnvironment() = 0;

	//! Returns the user-defined callback, notifies of system initialization flow.
	virtual ISystemUserCallback* GetUserCallback() const = 0;

	//! Returns the root folder of the engine installation.
	virtual const char* GetRootFolder() const = 0;

	//! Starts a new frame, updates engine systems, game logic and finally renders.
	//! \return Returns true if the engine should continue running, false to quit.
	virtual bool DoFrame(const SDisplayContextKey& displayContextKey, const SGraphicsPipelineKey& graphicsPipelineKey, CEnumFlags<ESystemUpdateFlags> updateFlags = CEnumFlags<ESystemUpdateFlags>()) = 0;

	virtual void RenderBegin(const SDisplayContextKey& displayContextKey, const SGraphicsPipelineKey& graphicsPipelineKey) = 0;
	virtual void RenderEnd(bool bRenderStats = true) = 0;

	//! Updates the engine's systems without creating a rendered frame
	virtual bool Update(CEnumFlags<ESystemUpdateFlags> updateFlags, int nPauseMode = 0) = 0;

	virtual void RenderPhysicsHelpers() = 0;

	//! Get the manual frame step controller, allows for completely blocking per-frame update
	virtual IManualFrameStepController* GetManualFrameStepController() const = 0;

	//! Updates only require components during loading.
	virtual bool UpdateLoadtime() = 0;

	//! Update screen and call some important tick functions during loading.
	virtual void SynchronousLoadingTick(const char* pFunc, int line) = 0;

	//! Renders the statistics.
	//! This is called from RenderEnd, but if the Host application (Editor) doesn't employ the Render cycle in ISystem,
	//! it may call this method to render the essential statistics.
	virtual void RenderStatistics() = 0;
	virtual void RenderPhysicsStatistics(IPhysicalWorld* pWorld) = 0;

	//! Returns the current used memory.
	virtual uint32 GetUsedMemory() = 0;

	//! Retrieve the name of the user currently logged in to the computer.
	virtual const char* GetUserName() = 0;

	//! Gets current supported CPU features flags. (CPUF_SSE, CPUF_SSE2, CPUF_3DNOW, CPUF_MMX)
	virtual uint32 GetCPUFlags() = 0;

	//! Gets number of CPUs
	virtual int GetLogicalCPUCount() = 0;

	//! Dumps the memory usage statistics to the logging default MB. (can be KB)
	virtual void DumpMemoryUsageStatistics(bool bUseKB = false) = 0;

	//! Quits the application.
	virtual void Quit() = 0;

	//! Tells the system if it is relaunching or not.
	virtual void Relaunch(bool bRelaunch) = 0;

	//! Returns true if the application is in the shutdown phase.
	virtual bool IsQuitting() const = 0;

	//! Returns true if the application was initialized to generate the shader cache.
	virtual bool IsShaderCacheGenMode() const = 0;

	//! Tells the system in which way we are using the serialization system.
	virtual void SerializingFile(int mode) = 0;
	virtual int  IsSerializingFile() const = 0;

	virtual bool IsRelaunch() const = 0;

	//! Displays an error message to display info for certain time
	//! \param acMessage Message to show.
	//! \param fTime Amount of seconds to show onscreen.
	virtual void DisplayErrorMessage(const char* acMessage, float fTime, const float* pfColor = 0, bool bHardError = true) = 0;

	//! Displays error message.
	//! Logs it to console and file and error message box then terminates execution.
	virtual void FatalError(const char* sFormat, ...) PRINTF_PARAMS(2, 3) = 0;

	//! Reports a bug using the crash handler.
	//! Logs an error to the console and launches the crash handler, then continues execution.
	virtual void ReportBug(const char* sFormat, ...) PRINTF_PARAMS(2, 3) = 0;

	//! Report warning to current Validator object.
	//! Doesn't terminate the execution.
	//! ##@{
	virtual void WarningV(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, va_list args) = 0;
	virtual void Warning(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, ...) = 0;
	virtual void WarningOnce(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, ...) = 0;
	//! ##@}.

	//! Compare specified verbosity level to the one currently set.
	virtual bool CheckLogVerbosity(int verbosity) = 0;

	virtual bool IsUIFrameworkMode() { return false; }

	//! Fills the output array by random numbers using CMTRand_int32 generator
	virtual void     FillRandomMT(uint32* pOutWords, uint32 numWords) = 0;

	virtual CRndGen& GetRandomGenerator() = 0;

	// Return the related subsystem interface.

	virtual IZLibCompressor*        GetIZLibCompressor() = 0;
	virtual IZLibDecompressor*      GetIZLibDecompressor() = 0;
	virtual ILZ4Decompressor*       GetLZ4Decompressor() = 0;
	virtual ICryPerfHUD*            GetPerfHUD() = 0;
	virtual minigui::IMiniGUI*      GetMiniGUI() = 0;
	virtual IPlatformOS*            GetPlatformOS() = 0;
	virtual INotificationNetwork*   GetINotificationNetwork() = 0;
	virtual IHardwareMouse*         GetIHardwareMouse() = 0;
	virtual IDialogSystem*          GetIDialogSystem() = 0;
	virtual IFlowSystem*            GetIFlowSystem() = 0;
	virtual IBudgetingSystem*       GetIBudgetingSystem() = 0;
	virtual INameTable*             GetINameTable() = 0;
	virtual IDiskProfiler*          GetIDiskProfiler() = 0;
	virtual ICryProfilingSystem*    GetProfilingSystem() = 0;
	virtual ILegacyProfiler*        GetLegacyProfilerInterface() = 0;
	virtual IValidator*             GetIValidator() = 0;
	virtual IPhysicsDebugRenderer*  GetIPhysicsDebugRenderer() = 0;
	virtual IPhysRenderer*          GetIPhysRenderer() = 0;
	virtual ICharacterManager*      GetIAnimationSystem() = 0;
	virtual IStreamEngine*          GetStreamEngine() = 0;
	virtual ICmdLine*               GetICmdLine() = 0;
	virtual ILog*                   GetILog() = 0;
	virtual ICryPak*                GetIPak() = 0;
	virtual ICryFont*               GetICryFont() = 0;
	virtual IEntitySystem*          GetIEntitySystem() = 0;
	virtual IMemoryManager*         GetIMemoryManager() = 0;
	virtual IAISystem*              GetAISystem() = 0;
	virtual IMovieSystem*           GetIMovieSystem() = 0;
	virtual IPhysicalWorld*         GetIPhysicalWorld() = 0;
	virtual CryAudio::IAudioSystem* GetIAudioSystem() = 0;
	virtual I3DEngine*              GetI3DEngine() = 0;
	virtual IScriptSystem*          GetIScriptSystem() = 0;
	virtual IConsole*               GetIConsole() = 0;
	virtual IRemoteConsole*         GetIRemoteConsole() = 0;
	virtual IUserAnalyticsSystem*   GetIUserAnalyticsSystem() = 0;
	virtual Cry::IPluginManager*    GetIPluginManager() = 0;
	virtual Cry::IProjectManager*   GetIProjectManager() = 0;
	virtual Cry::UDR::IUDRSystem*   GetIUDR() = 0;

	//! \return Can be NULL, because it only exists when running through the editor, not in pure game mode.
	virtual IResourceManager*                  GetIResourceManager() = 0;

	virtual ISystemEventDispatcher*            GetISystemEventDispatcher() = 0;
	virtual IFileChangeMonitor*                GetIFileChangeMonitor() = 0;

	virtual CRY_HWND                           GetHWND() = 0;
	virtual CRY_HWND                           GetActiveHWND() = 0;

	virtual INetwork*                          GetINetwork() = 0;
	virtual IRenderer*                         GetIRenderer() = 0;
	virtual IInput*                            GetIInput() = 0;
	virtual ITimer*                            GetITimer() = 0;

	virtual IThreadManager*                    GetIThreadManager() = 0;
	virtual IMonoEngineModule*                 GetIMonoEngineModule() = 0;

	virtual void                               SetLoadingProgressListener(ILoadingProgressListener* pListener) = 0;
	virtual ISystem::ILoadingProgressListener* GetLoadingProgressListener() const = 0;

	virtual void                               SetIFlowSystem(IFlowSystem* pFlowSystem) = 0;
	virtual void                               SetIDialogSystem(IDialogSystem* pDialogSystem) = 0;
	virtual void                               SetIMaterialEffects(IMaterialEffects* pMaterialEffects) = 0;
	virtual void                               SetIParticleManager(IParticleManager* pParticleManager) = 0;
	virtual void                               SetIOpticsManager(IOpticsManager* pOpticsManager) = 0;
	virtual void                               SetIFileChangeMonitor(IFileChangeMonitor* pFileChangeMonitor) = 0;
	virtual void                               SetIFlashUI(IFlashUI* pFlashUI) = 0;
	virtual void                               SetIUIFramework(UIFramework::IUIFramework* pUIFramework) = 0;

	//! Changes current user sub path, the path is always relative to the user documents folder.
	//! Example: "My Games\Crysis"
	virtual void ChangeUserPath(const char* sUserPath) = 0;

	//virtual	const char			*GetGamePath()=0;

	virtual void DebugStats(bool checkpoint, bool leaks) = 0;
	virtual void DumpWinHeaps() = 0;
	virtual int  DumpMMStats(bool log) = 0;

	//! \param bValue Set to true when running on a cheat protected server or a client that is connected to it (not used in singleplayer).
	virtual void SetForceNonDevMode(const bool bValue) = 0;

	//! \return true when running on a cheat protected server or a client that is connected to it (not used in singleplayer).
	virtual bool GetForceNonDevMode() const = 0;
	virtual bool WasInDevMode() const = 0;
	virtual bool IsDevMode() const = 0;
	virtual bool IsMODValid(const char* szMODName) const = 0;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IXmlNode interface.
	//!	 Creates new xml node.
	//! \par Example
	//! \include CrySystem/Examples/XmlWriting.cpp
	virtual XmlNodeRef CreateXmlNode(const char* sNodeName = "", bool bReuseStrings = false) = 0;

	//! Loads xml from memory buffer, returns 0 if load failed.
	//! \par Example
	//! \include CrySystem/Examples/XmlParsing.cpp
	virtual XmlNodeRef LoadXmlFromBuffer(const char* buffer, size_t size, bool bReuseStrings = false) = 0;

	//! Loads xml file, returns 0 if load failed.
	//! \par Example
	//! \include CrySystem/Examples/XmlParsing.cpp
	virtual XmlNodeRef LoadXmlFromFile(const char* sFilename, bool bReuseStrings = false) = 0;

	//! Retrieves access to XML utilities interface.
	virtual IXmlUtils* GetXmlUtils() = 0;

	//! Interface to access different implementations of Serialization::IArchive in a centralized way.
	virtual Serialization::IArchiveHost* GetArchiveHost() const = 0;

	//! Sets the camera that will be used for main rendering next frame.
	//! This has to be set before Cry::IEnginePlugin::UpdateBeforeFinalizeCamera is called in order to be set in time for occlusion culling and rendering.
	virtual void SetViewCamera(CCamera& Camera) = 0;
	//! Gets the camera that will be used for main rendering next frame
	//! Note that the camera might be overridden by user code, and is only considered final after Cry::IEnginePlugin::UpdateBeforeFinalizeCamera has been executed.
	virtual const CCamera& GetViewCamera() const = 0;

	//! When ignore update sets to true, system will ignore and updates and render calls.
	virtual void IgnoreUpdates(bool bIgnore) = 0;

	//! Sets the active process.
	//! \param process Pointer to a class that implement the IProcess interface.
	virtual void SetIProcess(IProcess* process) = 0;

	//! Gets the active process.
	//! \return Pointer to the current active process.
	virtual IProcess* GetIProcess() = 0;

	//! Start recording a new session in the Bootprofiler (best use via LOADING_TIME_PROFILE_AUTO_SESSION)
	virtual void StartBootProfilerSession(const char* szName) = 0;
	//! End recording the current session in the Bootprofiler (best use via LOADING_TIME_PROFILE_AUTO_SESSION)
	virtual void EndBootProfilerSession() = 0;

	//////////////////////////////////////////////////////////////////////////
	// File version.
	//! Gets file version.
	virtual const SFileVersion& GetFileVersion() = 0;

	//! Gets product version.
	virtual const SFileVersion& GetProductVersion() = 0;

	//! Gets build version.
	virtual const SFileVersion& GetBuildVersion() = 0;

	//! Data compression.
	//! ##@{
	virtual bool CompressDataBlock(const void* input, size_t inputSize, void* output, size_t& outputSize, int level = 3) = 0;
	virtual bool DecompressDataBlock(const void* input, size_t inputSize, void* output, size_t& outputSize) = 0;
	//! ##@}.

	//////////////////////////////////////////////////////////////////////////
	// Configuration.
	//! Saves system configuration.
	virtual void SaveConfiguration() = 0;

	//! Loads system configuration
	//! \param pCallback 0 means normal LoadConfigVar behavior is used.
	//! \param bQuiet when set to true will suppress warning message if config file is not found.
	virtual void LoadConfiguration(const char* sFilename, ILoadConfigurationEntrySink* pSink = 0, ELoadConfigurationType configType = eLoadConfigDefault,
	                               ELoadConfigurationFlags flags = ELoadConfigurationFlags::None) = 0;

	//! Retrieves current configuration specification for client or server.
	//! \param bClient If true returns local client config spec, if false returns server config spec.
	virtual ESystemConfigSpec GetConfigSpec(bool bClient = true) = 0;

	virtual ESystemConfigSpec GetMaxConfigSpec() const = 0;

	//! Changes current configuration specification for client or server.
	//! \param bClient If true changes client config spec (sys_spec variable changed), if false changes only server config spec (as known on the client).
	virtual void SetConfigSpec(ESystemConfigSpec spec, bool bClient) = 0;

	//! Detects and set optimal spec.
	virtual void AutoDetectSpec(const bool detectResolution) = 0;

	//! Thread management for subsystems
	//! \return Non-0 if the state was indeed changed, 0 if already in that state.
	virtual int SetThreadState(ESubsystem subsys, bool bActive) = 0;

	//! Creates and returns a usable object implementing ICrySizer interface.
	virtual ICrySizer* CreateSizer() = 0;

	//! Query if system is now paused.
	//! Pause flag is set when calling system update with pause mode.
	virtual bool IsPaused() const = 0;

	//! Retrieves localized strings manager interface.
	virtual ILocalizationManager* GetLocalizationManager() = 0;

	//! Gets the head mounted display manager.
	virtual struct IHmdManager* GetHmdManager() = 0;

	//! Creates an instance of the AVI Reader class.
	virtual IAVI_Reader* CreateAVIReader() = 0;

	//! Release the AVI reader.
	virtual void              ReleaseAVIReader(IAVI_Reader* pAVIReader) = 0;

	virtual ITextModeConsole* GetITextModeConsole() = 0;

	//! Retrieves the perlin noise singleton instance.
	virtual CPNoise3* GetNoiseGen() = 0;

	//! Retrieves system update counter.
	virtual uint64 GetUpdateCounter() = 0;

	//! Gets access to all registered factories.
	virtual ICryFactoryRegistry* GetCryFactoryRegistry() const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Error callback handling
	//! Registers listeners to CryAssert and error messages (may not be called if asserts are disabled).
	//! Each pointer can be registered only once (stl::push_back_unique).
	//! It will return false if the pointer is already registered. Returns true, otherwise.
	virtual bool RegisterErrorObserver(IErrorObserver* errorObserver) = 0;

	//! Unregisters listeners to CryAssert and error messages.
	//! It will return false if the pointer is not registered. Otherwise, returns true.
	virtual bool UnregisterErrorObserver(IErrorObserver* errorObserver) = 0;

	//! Returns true if the system is loading a level currently
	virtual bool IsLoading() = 0;

#if defined(USE_CRY_ASSERT)
	//! Called after the processing of the assert message box(Windows or Xbox).
	//! It will be called even when asserts are disabled by the console variables.
	virtual void OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber) = 0;
#endif

	//! Get the index of the currently running Crytek application, (0 = first instance, 1 = second instance, etc).
	virtual int GetApplicationInstance() = 0;

	//! Retrieves the current stats for systems to update the respective time taken.
	virtual sUpdateTimes& GetCurrentUpdateTimeStats() = 0;

	//! Retrieves the array of update times and the number of entries.
	virtual const sUpdateTimes* GetUpdateTimeStats(uint32&, uint32&) = 0;

	//! Clear all currently logged and drawn on screen error messages.
	virtual void ClearErrorMessages() = 0;

	//! Fills array of function pointers, nCount return number of functions.
	//! \note Pass nCount to indicate maximum number of functions to get.
	//! \note For debugging use only!, queries current C++ call stack.
	virtual void debug_GetCallStack(const char** pFunctions, int& nCount) = 0;

	//! Logs current callstack.
	//! \note For debugging use only!, queries current C++ call stack.
	virtual void debug_LogCallStack(int nMaxFuncs = 32, int nFlags = 0) = 0;

#ifdef CRY_TESTING
	//! \return 0 if not activated, activate through #System.ApplicationTest.
	virtual CryTest::ITestSystem* GetITestSystem() = 0;
#endif

	//! Execute command line arguments. Should be after init game.
	//! Example: +g_gametype ASSAULT +map "testy"
	virtual void ExecuteCommandLine() = 0;

	//! GetSystemUpdate stats (all systems update without except console).
	//! Very useful on dedicated server as we throttle it to fixed frequency.
	//! \return zeros if no updates happened yet.
	virtual void               GetUpdateStats(SSystemUpdateStats& stats) = 0;

	virtual ESystemGlobalState GetSystemGlobalState(void) = 0;
	virtual void               SetSystemGlobalState(const ESystemGlobalState systemGlobalState) = 0;

	//! Add a PlatformOS create flag
	virtual void AddPlatformOSCreateFlag(const uint8 createFlag) = 0;

	//! Asynchronous memcpy.
	//! Note sync variable will be incremented (in calling thread) before job starts
	//! and decremented when job finishes. Multiple async copies can therefore be
	//! tied to the same sync variable, therefore it's advised to wait for completion with while(*sync) (yield());.
	virtual void AsyncMemcpy(void* dst, const void* src, size_t size, int nFlags, volatile int* sync) = 0;
	// </interfuscator:shuffle>

	virtual bool IsCVarWhitelisted(const char* szName, bool silent) const = 0;

#if !defined(RELEASE)
	virtual void GetCheckpointData(ICheckpointData& data) = 0;
	virtual void IncreaseCheckpointLoadCount() = 0;
	virtual void SetLoadOrigin(LevelLoadOrigin origin) = 0;
#endif

#if CRY_PLATFORM_DURANGO
	virtual void OnPLMEvent(EPLM_Event event) = 0;
#endif

#if !defined(RELEASE)
	virtual bool IsSavingResourceList() const = 0;
#endif

	//! Loads a dynamic library and returns the first factory with the specified interface id contained inside the module
	virtual ICryFactory* LoadModuleWithFactory(const char* szDllName, const CryInterfaceID& moduleInterfaceId) = 0;

	//! Loads a dynamic library and creates an instance of the first factory contained inside the module
	template<typename T>
	inline std::shared_ptr<T> LoadModuleAndCreateFactoryInstance(const char* szDllName, const SSystemInitParams& initParams)
	{
		if (ICryFactory* pFactory = LoadModuleWithFactory(szDllName, cryiidof<T>()))
		{
			// Create an instance of the implementation
			std::shared_ptr<T> pModule = cryinterface_cast<T, ICryUnknown>(pFactory->CreateClassInstance());

			pModule->Initialize(*GetGlobalEnvironment(), initParams);

			return pModule;
		}

		return nullptr;
	}

	//! Unloads a dynamic library as well as the corresponding instance of the module class
	virtual bool UnloadEngineModule(const char* szDllName) = 0;

	//! Gets the root window message handler function.
	//! The returned pointer is platform-specific: for Windows OS, the pointer is of type WNDPROC
	virtual void* GetRootWindowMessageHandler() = 0;

	//! Register a IWindowMessageHandler that will be informed about window messages.
	//! The delivered messages are platform-specific.
	virtual void RegisterWindowMessageHandler(IWindowMessageHandler* pHandler) = 0;

	//! Unregister an IWindowMessageHandler that was previously registered using RegisterWindowMessageHandler.
	virtual void UnregisterWindowMessageHandler(IWindowMessageHandler* pHandler) = 0;

	//! Pumps window messages, informing all registered IWindowMessageHandlers
	//! If bAll is true, all available messages will be pumped
	//! If hWnd is not NULL, only messages for the given window are processed (ignored on non-windows platforms)
	//! Returns the number of messages pumped, or -1 if the OS indicated the application should quit
	//! Note: Calling GetMessage or PeekMessage yourself will skip the pre-process handling required for IME support
	virtual int PumpWindowMessage(bool bAll, CRY_HWND hWnd = 0) = 0;

	//! Check if IME is supported on the current platform
	//! Note: This flag depends on compile-time settings, it cannot be enabled or disabled at runtime
	//! However, the support itself can typically be enabled/disabled through CVar
	virtual bool IsImeSupported() const = 0;

	//! Returns the IME manager in use.
	virtual IImeManager* GetImeManager() const = 0;
};

//! This is a very important function for the dedicated server - it lets us run >1000 players per piece of server hardware.
//! It this saves us lots of money on the dedicated server hardware.
#define SYNCHRONOUS_LOADING_TICK() do { if (gEnv && gEnv->pSystem) gEnv->pSystem->SynchronousLoadingTick(__FUNC__, __LINE__); } while (0)

// CrySystem DLL Exports.
typedef ISystem* (* PFNCREATESYSTEMINTERFACE)(SSystemInitParams& initParams, bool bManualEngineLoop);

// Global environment variable.
#if defined(SYS_ENV_AS_STRUCT)
extern SSystemGlobalEnvironment gEnv;
#else
extern SSystemGlobalEnvironment* gEnv;
#endif

//! Gets the system interface.
inline ISystem* GetISystem()
{
#if defined(SYS_ENV_AS_STRUCT)
	return gEnv->pSystem;
#else
	return gEnv != nullptr ? gEnv->pSystem : nullptr;
#endif // defined(SYS_ENV_AS_STRUCT)
}

#if defined(MAP_LOADING_SLICING)
//! Gets the system scheduler interface.
inline ISystemScheduler* GetISystemScheduler(void)
{
	#if defined(SYS_ENV_AS_STRUCT)
	return gEnv->pSystemScheduler;
	#else
	return gEnv != nullptr ? gEnv->pSystemScheduler : nullptr;
	#endif // defined(SYS_ENV_AS_STRUCT)
}
#endif // defined(MAP_LOADING_SLICING)
//! This function must be called once by each module at the beginning, to setup global pointers.
extern "C" DLL_EXPORT void ModuleInitISystem(ISystem* pSystem, const char* moduleName);
extern int g_iTraceAllocations;

//! Interface of the DLL.
extern "C"
{
	CRYSYSTEM_API ISystem* CreateSystemInterface(SSystemInitParams& initParams, bool bManualEngineLoop);
}

//! Displays error message, logs it to console and file and error message box, tThen terminates execution.
void        CryFatalError(const char*, ...) PRINTF_PARAMS(1, 2);
inline void CryFatalError(const char* format, ...)
{
	if (!gEnv || !gEnv->pSystem)
		return;

	va_list ArgList;
	char szBuffer[MAX_WARNING_LENGTH];
	va_start(ArgList, format);
	cry_vsprintf(szBuffer, format, ArgList);
	va_end(ArgList);

	gEnv->pSystem->FatalError("%s", szBuffer);
}

//! Displays warning message, logs it to console and file and display a warning message box. Doesn't terminate execution.
void CryWarning(EValidatorModule, EValidatorSeverity, const char*, ...) PRINTF_PARAMS(3, 4);
inline void CryWarning(EValidatorModule module, EValidatorSeverity severity, const char* format, ...)
{
	if (!gEnv || !gEnv->pSystem || !format)
		return;

	va_list args;
	va_start(args, format);
	GetISystem()->WarningV(module, severity, 0, 0, format, args);
	va_end(args);
}

#ifdef EXCLUDE_NORMAL_LOG       // setting this removes a lot of logging to reduced code size (useful for consoles)

	#define CryLog(...)       ((void)0)
	#define CryComment(...)   ((void)0)
	#define CryLogAlways(...) ((void)0)

#else // EXCLUDE_NORMAL_LOG

//! Simple logs of data with low verbosity.
void        CryLog(const char*, ...) PRINTF_PARAMS(1, 2);
inline void CryLog(const char* format, ...)
{
	// Fran: we need these guards for the testing framework to work
	if (gEnv && gEnv->pSystem && gEnv->pLog)
	{
		va_list args;
		va_start(args, format);
		gEnv->pLog->LogV(ILog::eMessage, format, args);
		va_end(args);
	}
}
//! Very rarely used log comment.
void        CryComment(const char*, ...) PRINTF_PARAMS(1, 2);
inline void CryComment(const char* format, ...)
{
	// Fran: we need these guards for the testing framework to work
	if (gEnv && gEnv->pSystem && gEnv->pLog)
	{
		va_list args;
		va_start(args, format);
		gEnv->pLog->LogV(ILog::eComment, format, args);
		va_end(args);
	}
}
//! Logs important data that must be printed regardless verbosity.
void        CryLogAlways(const char*, ...) PRINTF_PARAMS(1, 2);
inline void CryLogAlways(const char* format, ...)
{
	// log should not be used before system is ready
	// error before system init should be handled explicitly

	// Fran: we need these guards for the testing framework to work

	if (gEnv && gEnv->pSystem && gEnv->pLog)
	{
		//		assert(gEnv);
		//		assert(gEnv->pSystem);

		va_list args;
		va_start(args, format);
		gEnv->pLog->LogV(ILog::eAlways, format, args);
		va_end(args);
	}
}

#endif // EXCLUDE_NORMAL_LOG

/*****************************************************
   ASYNC MEMCPY FUNCTIONS
*****************************************************/

//! Complex delegation required because it is not really easy to export a external standalone symbol like a memcpy function when building with modules.
//! Dlls pay an extra indirection cost for calling this function.
#if !defined(_LIB)
	#define CRY_ASYNC_MEMCPY_DELEGATE_TO_CRYSYSTEM
#endif
#define CRY_ASYNC_MEMCPY_API extern "C"

//! Note sync variable will be incremented (in calling thread) before job starts and decremented when job finishes.
//! Multiple async copies can therefore be tied to the same sync variable, therefore wait for completion with while(*sync) (yield());.
#if defined(CRY_ASYNC_MEMCPY_DELEGATE_TO_CRYSYSTEM)
inline void cryAsyncMemcpy(
	void* dst
	, const void* src
	, size_t size
	, int nFlags
	, volatile int* sync)
{
	GetISystem()->AsyncMemcpy(dst, src, size, nFlags, sync);
}
#else
CRY_ASYNC_MEMCPY_API void cryAsyncMemcpy(
	void* dst
	, const void* src
	, size_t size
	, int nFlags
	, volatile int* sync);
#endif

inline CryGUID CryGUID::Create()
{
	CryGUID guid;
	gEnv->pSystem->FillRandomMT(reinterpret_cast<uint32*>(&guid), sizeof(guid) / sizeof(uint32));
	MEMORY_RW_REORDERING_BARRIER;
	return guid;
}

// brings CRY_PROFILE_X macros everywhere
// included at the end as we need gEnv first
#include <CrySystem/Profilers/ICryProfilingSystem.h>
