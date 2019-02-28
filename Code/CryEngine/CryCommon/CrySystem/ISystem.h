// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifdef CRYSYSTEM_EXPORTS
	#define CRYSYSTEM_API DLL_EXPORT
#else
	#define CRYSYSTEM_API DLL_IMPORT
#endif

#include <CryCore/Platform/platform.h> // Needed for LARGE_INTEGER (for consoles).
#include <CryCore/Assert/CryAssert.h>

#include <CrySystem/XML/IXml.h> // <> required for Interfuscator
#include "IValidator.h"         // <> required for Interfuscator
#include <CrySystem/ILog.h>     // <> required for Interfuscator
#include <CryString/StringUtils.h>
#include "CryVersion.h"
#include <CryCore/smartptr.h>
#include <CryMemory/IMemory.h>          // <> required for Interfuscator
#include <CrySystem/ISystemScheduler.h> // <> required for Interfuscator

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_Shared.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <CryMath/LCGRandom.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ICryUnknown.h>
#include <CrySystem/IManualFrameStepController.h>

struct ILog;
struct IProfileLogSystem;
struct IEntitySystem;
struct IEntity;
struct ICryPak;
struct IKeyboard;
struct IMouse;
struct IRemoteConsole;
struct IInput;
struct IRenderer;
struct SDisplayContextKey;
struct IRenderAuxGeom;
struct IConsole;
namespace Telemetry {
struct ITelemetrySystem;
}
struct IProcess;
struct I3DEngine;
struct ITimer;
struct IGameFramework;
struct IGameStartup;
struct IScriptSystem;
struct IAISystem;
struct IFlash;
struct INetwork;
struct INetContext;
struct ICryLobby;
struct ICryFont;
struct IMovieSystem;
struct IPhysicalWorld;
struct IMemoryManager;
namespace CryAudio
{
struct IAudioSystem;
}
struct ISystem;
namespace Cry
{
namespace Reflection
{
struct IReflection;
}
}
struct IFrameProfileSystem;
struct IStatoscope;
class IDiskProfiler;
struct FrameProfiler;
struct IStreamEngine;
struct ICharacterManager;
struct SFileVersion;
struct INameTable;
struct IBudgetingSystem;
struct IFlowSystem;
struct IDialogSystem;
namespace DRS {
struct IDynamicResponseSystem;
}
struct IMaterialEffects;
struct IParticleManager;
class IOpticsManager;
struct IHardwareMouse;
struct IFlashPlayer;
struct IFlashPlayerBootStrapper;
struct IFlashLoadMovieHandler;
struct IHmdDevice;
class ICrySizer;
struct ITestSystem;
class IXMLBinarySerializer;
struct IReadWriteXMLSink;
struct IResourceManager;
struct ITextModeConsole;
struct IAVI_Reader;
class CPNoise3;
struct IFileChangeMonitor;
struct ILocalizationManager;
struct ICryFactoryRegistry;
struct ICodeCheckpointMgr;
struct IZLibCompressor;
struct IZLibDecompressor;
struct ILZ4Decompressor;
struct IOutputPrintSink;
struct IPhysicsDebugRenderer;
struct IPhysRenderer;
struct IOverloadSceneManager;
struct IFlashUI;
struct IThreadManager;
struct IServiceNetwork;
struct IUserAnalyticsSystem;
struct IRemoteCommandManager;
struct IWindowMessageHandler;
struct SFunctor;
struct IScaleformHelper;
struct IProjectManager;
class IImeManager;
struct SDisplayContextKey;

class CBootProfilerRecord;

namespace Cry
{
	struct IPluginManager;
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

struct IMonoEngineModule;

struct ILocalMemoryUsage;

typedef void* WIN_HWND;

class CCamera;
struct CLoadingTimeProfiler;

class ICmdLine;
class CFrameProfilerSection;

struct INotificationNetwork;
struct IPlatformOS;
struct ICryPerfHUD;

namespace JobManager {
struct IJobManager;
}

struct ICrySchematycCore;
namespace Schematyc2
{
	struct IFramework;
}

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
	ESYSUPDATE_IGNORE_AI         = BIT(0),
	ESYSUPDATE_IGNORE_PHYSICS    = BIT(2),
	//! Special update mode for editor.
	ESYSUPDATE_EDITOR            = BIT(3),
	ESYSUPDATE_MULTIPLAYER       = BIT(4),
	ESYSUPDATE_EDITOR_AI_PHYSICS = BIT(5),
	ESYSUPDATE_EDITOR_ONLY       = BIT(6)
};

//! Configuration specification, depends on user selected machine specification.
enum ESystemConfigSpec
{
	CONFIG_CUSTOM        = 0, //!< Should always be first.
	CONFIG_LOW_SPEC      = 1,
	CONFIG_MEDIUM_SPEC   = 2,
	CONFIG_HIGH_SPEC     = 3,
	CONFIG_VERYHIGH_SPEC = 4,

	CONFIG_DURANGO       = 5,
	CONFIG_ORBIS         = 6,

	//! Specialized detail config setting.
	CONFIG_DETAIL_SPEC = 7,

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

	//! Called when the sanbox has finished initialization
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

	//! Sent when frontend is initialised.
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
	//! Triggers when streaming install had failed to open newly recevied pak files
	//! + may be triggerd on platform error as well: like scratched disks or network problems
	//! when installing from store
	ESYSTEM_EVENT_STREAMING_INSTALL_ERROR,

	//! Sent when the online services are initialized.
	ESYSTEM_EVENT_ONLINE_SERVICES_INITIALISED,

	//! Sent when a new audio implementation is loaded.
	ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED,

	//! Purpose of this event is to enable different modules to communicate with each other without knowing about each other.
	ESYSTEM_EVENT_URI,

	ESYSTEM_EVENT_USER = 0x1000,

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
	ESYSTEM_EVENT_GAME_FRAMEWORK_ABOUT_TO_SHUTDOWN
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
	virtual void OnSystemEventAnyThread(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) {}
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

#if defined(CVARS_WHITELIST)
struct ICVarsWhitelist
{
	// <interfuscator:shuffle>
	virtual ~ICVarsWhitelist() {};
	virtual bool IsWhiteListed(const string& command, bool silent) = 0;
	// </interfuscator:shuffle>
};
#endif // defined(CVARS_WHITELIST)

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
#if defined(CVARS_WHITELIST)
	ICVarsWhitelist*     pCVarsWhitelist;       //!< CVars whitelist callback.
#endif                                        // defined(CVARS_WHITELIST)
	const char*          sLogFileName;          //!< File name to use for log.
	IValidator*          pValidator;            //!< You can specify different validator object to use by System.
	IOutputPrintSink*    pPrintSync;            //!< Print Sync which can be used to catch all output from engine.
	char                 szSystemCmdLine[2048]; //!< Command line.
	char                 szUserPath[256];       //!< User alias path relative to My Documents folder.
	char                 szBinariesDir[256];

	bool                 bEditor;             //!< When running in Editor mode.
	bool                 bPreview;            //!< When running in Preview mode (Minimal initialization).
	bool                 bDedicatedServer;    //!< When running a dedicated server.
	bool                 bExecuteCommandLine; //!< can be switched of to suppress the feature or do it later during the initialization.
	bool                 bUIFramework;
	bool                 bSkipFont;            //!< Don't load CryFont.dll.
	bool                 bSkipRenderer;        //!< Don't load Renderer.
	bool                 bSkipNetwork;         //!< Don't create Network.
	bool                 bSkipLiveCreate;      //!< Don't create LiveCreate.
	bool                 bSkipWebsocketServer; //!< Don't create the WebSocket server.
	bool                 bMinimal;             //!< Don't load banks.
	bool                 bSkipInput;           //!< do not load CryInput.
	bool                 bTesting;             //!< When running CryUnitTest.
	bool                 bNoRandom;            //!< use fixed generator init/seed.
	bool                 bShaderCacheGen;      //!< When running in shadercache gen mode.
	bool                 bUnattendedMode;      //!< When running as part of a build on build-machines: Prevent popping up of any dialog.

#if CRY_PLATFORM_DURANGO
	const EPLM_Event* pLastPLMEvent;
#endif

	ISystem*        pSystem;              //!< Pointer to existing ISystem interface, it will be reused if not NULL.
	//! Char szLocalIP[256];              //! local IP address (needed if we have several servers on one machine).
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	void (* pCheckFunc)(void*);            //!< Authentication function (must be set).
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
#if defined(CVARS_WHITELIST)
		pCVarsWhitelist = nullptr;
#endif // defined(CVARS_WHITELIST)
		sLogFileName = nullptr;
		pValidator = nullptr;
		pPrintSync = nullptr;
		memset(szSystemCmdLine, 0, sizeof(szSystemCmdLine));
		memset(szUserPath, 0, sizeof(szUserPath));
		memset(szBinariesDir, 0, sizeof(szBinariesDir));
		bEditor = false;
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

//! Typedef for frame profile callback function.
typedef void (* FrameProfilerSectionCallback)(class CFrameProfilerSection* pSection);

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

struct SPlatformInfo
{
	unsigned int numCoresAvailableToProcess;
	unsigned int numLogicalProcessors;

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

	EWinVersion winVer;
	bool        win64Bit;
	bool        vistaKB940105Required;
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
//! Holds info about system update stats over perior of time (cvar-tweakable)
struct SSystemUpdateStats
{
	SSystemUpdateStats() : avgUpdateTime(0.0f), minUpdateTime(0.0f), maxUpdateTime(0.0f){}
	float avgUpdateTime;
	float minUpdateTime;
	float maxUpdateTime;
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
	IDialogSystem*               pDialogSystem;
	I3DEngine*                   p3DEngine;
	INetwork*                    pNetwork;
	INetContext*                 pNetContext;
	ICryLobby*                   pLobby;
	IScriptSystem*               pScriptSystem;
	IPhysicalWorld*              pPhysicalWorld;
	IFlowSystem*                 pFlowSystem;
	IInput*                      pInput;
	IStatoscope*                 pStatoscope;
	ICryPak*                     pCryPak;
	IFileChangeMonitor*          pFileChangeMonitor;
	IProfileLogSystem*           pProfileLogSystem;
	IParticleManager*            pParticleManager;
	IOpticsManager*              pOpticsManager;
	IFrameProfileSystem*         pFrameProfileSystem;
	ITimer*                      pTimer;
	ICryFont*                    pCryFont;
	IGameFramework*              pGameFramework;
	ILocalMemoryUsage*           pLocalMemoryUsage;
	IEntitySystem*               pEntitySystem;
	IConsole*                    pConsole;
	CryAudio::IAudioSystem*      pAudioSystem;
	ISystem*                     pSystem;
	ICharacterManager*           pCharacterManager;
	IAISystem*                   pAISystem;
	ILog*                        pLog;
	ICodeCheckpointMgr*          pCodeCheckpointMgr;
	IMovieSystem*                pMovieSystem;
	INameTable*                  pNameTable;
	IRenderer*                   pRenderer;
	IRenderAuxGeom*              pAuxGeomRenderer;
	IHardwareMouse*              pHardwareMouse;
	IMaterialEffects*            pMaterialEffects;
	JobManager::IJobManager*     pJobManager;
	IOverloadSceneManager*       pOverloadSceneManager;
	IFlashUI*                    pFlashUI;
	UIFramework::IUIFramework*   pUIFramework;
	IServiceNetwork*             pServiceNetwork;
	IRemoteCommandManager*       pRemoteCommandManager;
	DRS::IDynamicResponseSystem* pDynamicResponseSystem;
	IThreadManager*              pThreadManager;
	IScaleformHelper*            pScaleformHelper; // nullptr when Scaleform support is not enabled
	ICrySchematycCore*           pSchematyc;
	Schematyc2::IFramework*      pSchematyc2;
	Cry::Reflection::IReflection* pReflection;

#if CRY_PLATFORM_DURANGO
	void*      pWindow;
	EPLM_State ePLM_State;
#endif

#if defined(MAP_LOADING_SLICING)
	ISystemScheduler*     pSystemScheduler;
#endif
	LiveCreate::IManager* pLiveCreateManager;
	LiveCreate::IHost*    pLiveCreateHost;

	IMonoEngineModule*    pMonoRuntime;

	threadID              mMainThreadId;      //!< The main thread ID is used in multiple systems so should be stored globally.

	uint32                nMainFrameID;

	const char*           szCmdLine = "";       //!< Startup command line.

	//! Generic debug string which can be easily updated by any system and output by the debug handler
	enum { MAX_DEBUG_STRING_LENGTH = 128 };
	char szDebugStatus[MAX_DEBUG_STRING_LENGTH];

	//! Used to tell if this is a server/multiplayer instance
	bool bServer;
	bool bMultiplayer;
	bool bHostMigrating;

#if defined(CRY_PLATFORM_ORBIS) && (!defined(_RELEASE) || defined(PERFORMANCE_BUILD))
	//! Specifies if we are on a PS4 development kit
	bool bPS4DevKit;
#endif

	//////////////////////////////////////////////////////////////////////////
	// Used by frame profiler.
	int                          bDeepProfiling;
	bool                         bBootProfilerEnabledFrames;
	FrameProfilerSectionCallback callbackStartSection;
	FrameProfilerSectionCallback callbackEndSection;
	//////////////////////////////////////////////////////////////////////////

#if defined(USE_CRY_ASSERT)
	bool ignoreAllAsserts = false;
	bool stoppedOnAssert = false;
	ECryAssertLevel cryAssertLevel = ECryAssertLevel::Enabled;
#endif

	//! Whether we are running unattended, disallows message boxes and other blocking events that require human intervention
	bool bUnattendedMode;
	//! Whether we are unit testing
	bool bTesting;

	bool          bNoRandomSeed;

	SPlatformInfo pi;

	// Protected functions.
	SSystemInitParams::ProtectedFunction pProtectedFunctions[eProtectedFuncsLast];

	//////////////////////////////////////////////////////////////////////////
	//! Flag to able to print out of memory condition
	bool             bIsOutOfMemory;
	bool             bIsOutOfVideoMemory;

	ILINE const bool IsClient() const
	{
#if CRY_PLATFORM_DESKTOP
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

	ILINE void SetIsDedicated(bool isDedicated)
	{
	#if defined(DEDICATED_SERVER)
		bDedicated = true;
	#else
		bDedicated = isDedicated;
	#endif
	}

	ILINE void SetIsClient(bool isClient)
	{
		bClient = isClient;
	}
#endif

	//! This way the compiler can strip out code for consoles.
	ILINE const bool IsEditor() const
	{
#if CRY_PLATFORM_DESKTOP
		return bEditor;
#else
		return false;
#endif
	}

	ILINE const bool IsEditorGameMode() const
	{
#if CRY_PLATFORM_DESKTOP
		return bEditorGameMode;
#else
		return false;
#endif
	}

	ILINE const bool IsEditorSimulationMode() const
	{
#if CRY_PLATFORM_DESKTOP
		return bEditorSimulationMode;
#else
		return false;
#endif
	}

	ILINE const bool IsGameOrSimulation() const
	{
#if CRY_PLATFORM_DESKTOP
		return !bEditor || bEditorGameMode || bEditorSimulationMode;
#else
		return true;
#endif
	}

	ILINE const bool IsEditing() const
	{
#if CRY_PLATFORM_DESKTOP
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
	bool bDedicatedArbitrator;

private:
	bool bClient;
	bool bEditor;          //!< Engine is running under editor.
	bool bEditorGameMode;  //!< Engine is in editor game mode.
	bool bDedicated;       //!< Engine is in dedicated.
	bool bEditorSimulationMode; //!< Engine is in editor Physics/AI simulation mode.
#endif

	bool m_isFMVPlaying;
	bool m_isCutscenePlaying;

public:
	SSystemGlobalEnvironment()
	{
		mAsyncDipState.nValue = 0;
	};

	CRY_ALIGN(64) UAsyncDipState mAsyncDipState;
};

// The ISystem interface that follows has a member function called 'GetUserName'. If we don't #undef'ine the same-named Win32 symbol here ISystem wouldn't even compile.
// There might be a better place for this.
#ifdef GetUserName
	#undef GetUserName
#endif

struct IProfilingSystem
{
	// <interfuscator:shuffle>
	virtual ~IProfilingSystem(){}
	//////////////////////////////////////////////////////////////////////////
	// VTune Profiling interface.

	//! Resumes vtune data collection.
	virtual void VTuneResume() = 0;

	//! Pauses vtune data collection.
	virtual void VTunePause() = 0;
	//////////////////////////////////////////////////////////////////////////
	// </interfuscator:shuffle>
};

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

#ifndef _RELEASE
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
	virtual bool DoFrame(const SDisplayContextKey& displayContextKey, CEnumFlags<ESystemUpdateFlags> updateFlags = CEnumFlags<ESystemUpdateFlags>()) = 0;

	virtual void RenderBegin(const SDisplayContextKey& displayContextKey) = 0;
	virtual void RenderEnd(bool bRenderStats = true) = 0;

	//! Updates the engine's systems without creating a rendered frame
	virtual bool Update(CEnumFlags<ESystemUpdateFlags> updateFlags, int nPauseMode = 0) = 0;

	virtual void DoWorkDuringOcclusionChecks() = 0;
	virtual bool NeedDoWorkDuringOcclusionChecks() = 0;

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
	virtual IPlatformOS*            GetPlatformOS() = 0;
	virtual INotificationNetwork*   GetINotificationNetwork() = 0;
	virtual IHardwareMouse*         GetIHardwareMouse() = 0;
	virtual IDialogSystem*          GetIDialogSystem() = 0;
	virtual IFlowSystem*            GetIFlowSystem() = 0;
	virtual IBudgetingSystem*       GetIBudgetingSystem() = 0;
	virtual INameTable*             GetINameTable() = 0;
	virtual IDiskProfiler*          GetIDiskProfiler() = 0;
	virtual IFrameProfileSystem*    GetIProfileSystem() = 0;
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
	virtual IProjectManager*        GetIProjectManager() = 0;

	//! \return Can be NULL, because it only exists when running through the editor, not in pure game mode.
	virtual IResourceManager*                  GetIResourceManager() = 0;

	virtual IProfilingSystem*                  GetIProfilingSystem() = 0;
	virtual ISystemEventDispatcher*            GetISystemEventDispatcher() = 0;
	virtual IFileChangeMonitor*                GetIFileChangeMonitor() = 0;

	virtual WIN_HWND                           GetHWND() = 0;

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
	virtual void                         SetViewCamera(CCamera& Camera) = 0;
	//! Gets the camera that will be used for main rendering next frame
	//! Note that the camera might be overridden by user code, and is only considered final after Cry::IEnginePlugin::UpdateBeforeFinalizeCamera has been executed.
	virtual const CCamera&               GetViewCamera() const = 0;

	//! When ignore update sets to true, system will ignore and updates and render calls.
	virtual void IgnoreUpdates(bool bIgnore) = 0;

	//! Sets the active process.
	//! \param process Pointer to a class that implement the IProcess interface.
	virtual void SetIProcess(IProcess* process) = 0;

	//! Gets the active process.
	//! \return Pointer to the current active process.
	virtual IProcess* GetIProcess() = 0;

	//! Frame profiler functions.
	virtual void SetFrameProfiler(bool on, bool display, char* prefix) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Loading time/memory profiling
	//! Starts function loading stats profiling.
	virtual struct SLoadingTimeContainer* StartLoadingSectionProfiling(CLoadingTimeProfiler* pProfiler, const char* szFuncName) = 0;

	//! Ends function loading stats profiling.
	virtual void EndLoadingSectionProfiling(CLoadingTimeProfiler* pProfiler) = 0;

	//! Starts function profiling with bootprofiler (session must be started).
	virtual CBootProfilerRecord* StartBootSectionProfiler(const char* name, const char* args,EProfileDescription type) = 0;

	//! Ends function profiling with bootprofiler.
	virtual void StopBootSectionProfiler(CBootProfilerRecord* record) = 0;

	// Summary:
	//	 Starts bootprofiler session.
	virtual void StartBootProfilerSession(const char* szName) = 0;

	// Summary:
	//	 Stops bootprofiler session.
	virtual void StopBootProfilerSession(const char* szName) = 0;

	//! game dll should call this on frame start
	virtual void OnFrameStart(const char* szName) = 0;

	//! game dll should call this on frame end
	virtual void OnFrameEnd() = 0;

	//! Prints loading stats into log.
	virtual void OutputLoadingTimeStats() = 0;

	//! Starts function loading stats profiling.
	virtual const char* GetLoadingProfilerCallstack() = 0;

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
	//! \param pCallback 0 means normal LoadConfigVar behaviour is used.
	virtual void LoadConfiguration(const char* sFilename, ILoadConfigurationEntrySink* pSink = 0, ELoadConfigurationType configType = eLoadConfigDefault) = 0;

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

	//! Returns if the assert window from CryAssert is visible.
	//! OBS1: needed by the editor, as in some cases it can freeze if during an assert engine it will handle
	//! some events such as mouse movement in a CryPhysics assert.
	//! OBS2: it will always return false, if asserts are disabled or ignored.
	virtual bool IsAssertDialogVisible() const = 0;

	//! Checks if asserts are enabled for the specified module (see eCryModule)
	virtual bool AreAssertsEnabledForModule(uint32 moduleId) = 0;
	//! Disables assertions for the specified module (see eCryModule)
	virtual void DisableAssertionsForModule(uint32 moduleId) = 0;
	//! Sets the AssertVisisble internal variable.
	//! Typically it should only be called by CryAssert.
	virtual void SetAssertVisible(bool bAssertVisble) = 0;
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

	//! Can be called through console
	//! Example: #System.ApplicationTest("testcase0")
	//! \param szParam 0 generates error.
	virtual void ApplicationTest(const char* szParam) = 0;

	//! \return 0 if not activated, activate through #System.ApplicationTest.
	virtual ITestSystem* GetITestSystem() = 0;

	//! Execute command line arguments. Should be after init game.
	//! Example: +g_gametype ASSAULT +map "testy"
	virtual void ExecuteCommandLine() = 0;

	//! GetSystemUpdate stats (all systems update without except console).
	//! Very useful on dedicated server as we throttle it to fixed frequency.
	//! \return zeroes if no updates happened yet.
	virtual void GetUpdateStats(SSystemUpdateStats& stats) = 0;

	//! Useful to investigate memory fragmentation.
	//! Every time you call this from the console: #System.DumpMemoryCoverage()
	//! it adds a line to "MemoryCoverage.bmp" (generated the first time, there is a max line count).
	virtual void               DumpMemoryCoverage() = 0;
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

#if defined(CVARS_WHITELIST)
	virtual ICVarsWhitelist* GetCVarsWhiteList() const = 0;
#endif // defined(CVARS_WHITELIST)

#ifndef _RELEASE
	virtual void GetCheckpointData(ICheckpointData& data) = 0;
	virtual void IncreaseCheckpointLoadCount() = 0;
	virtual void SetLoadOrigin(LevelLoadOrigin origin) = 0;
#endif

#if CRY_PLATFORM_DURANGO
	virtual void OnPLMEvent(EPLM_Event event) = 0;
#endif

#if !defined(_RELEASE)
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
	virtual int PumpWindowMessage(bool bAll, WIN_HWND hWnd = 0) = 0;

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

#ifdef EXCLUDE_CVARHELP
	#define CVARHELP(_comment) 0
#else
	#define CVARHELP(_comment) _comment
#endif

//! Provide macros for fixing cvars for release mode on consoles to enums to allow for code stripping.
//! Do not enable for PC, apply VF_CHEAT there if required.
#if CRY_PLATFORM_DESKTOP
	#define CONST_CVAR_FLAGS (VF_NULL)
#else
	#define CONST_CVAR_FLAGS (VF_CHEAT)
#endif

#include <CrySystem/IConsole.h>

#if (defined(_LAUNCHER) && defined(CRY_IS_MONOLITHIC_BUILD)) || !defined(_LIB)
extern std::vector<const char*> g_moduleCommands;
extern std::vector<const char*> g_moduleCVars;
#define MODULE_REGISTER_COMMAND(name) g_moduleCommands.push_back(name)
#define MODULE_REGISTER_CVAR(name) g_moduleCVars.push_back(name)
#else
#define MODULE_REGISTER_COMMAND(name) static_cast<void>(0)
#define MODULE_REGISTER_CVAR(name) static_cast<void>(0)
#endif

struct ConsoleRegistrationHelper
{
	static CRY_FORCE_INLINE void AddCommand(const char* szCommand, ConsoleCommandFunc func, int flags = 0, const char* szHelp = nullptr, bool bIsManagedExternally = false)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		MODULE_REGISTER_COMMAND(szCommand);
		gEnv->pConsole->AddCommand(szCommand, func, flags, szHelp, bIsManagedExternally);
	}
	static CRY_FORCE_INLINE void AddCommand(const char* szName, const char* szScriptFunc, int flags = 0, const char* szHelp = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		MODULE_REGISTER_COMMAND(szName);
		gEnv->pConsole->AddCommand(szName, szScriptFunc, flags, szHelp);
	}

	static CRY_FORCE_INLINE ICVar* RegisterString(const char* szName, const char* szValue, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterString(szName, szValue, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}
	static CRY_FORCE_INLINE ICVar* RegisterInt(const char* szName, int value, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterInt(szName, value, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}
	static CRY_FORCE_INLINE ICVar* RegisterInt64(const char* szName, int64 value, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterInt64(szName, value, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}
	static CRY_FORCE_INLINE ICVar* RegisterFloat(const char* szName, float value, int flags, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->RegisterFloat(szName, value, flags, szHelp, pChangeFunc);
		}
		else
		{
			return nullptr;
		}
	}

	template<class T, class U>
	static CRY_FORCE_INLINE ICVar* Register(const char* szName, T* pSrc, U defaultValue, int flags = 0, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr, bool bAllowModify = true)
	{
		return RegisterImpl(get_enum_tag<T>(), szName, pSrc, defaultValue, flags, szHelp, pChangeFunc, bAllowModify);
	}

	static CRY_FORCE_INLINE ICVar* Register(ICVar* pVar)
	{
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(pVar->GetName());
			return gEnv->pConsole->Register(pVar);
		}
		else
		{
			return nullptr;
		}
	}

private:
	struct enum_tag {};
	struct non_enum_tag {};

	template<class T>
	struct get_enum_tag : std::conditional<std::is_enum<T>::value, enum_tag, non_enum_tag>::type {};

	template<class T, class U>
	static CRY_FORCE_INLINE ICVar* RegisterImpl(enum_tag, const char* szName, T* pSrc, U defaultValue, int flags = 0, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr, bool bAllowModify = true)
	{
		using ET = typename std::underlying_type<T>::type;
		static_assert(std::is_same<ET, int>::value, "Invalid template type!");
		return RegisterImpl(non_enum_tag(), szName, reinterpret_cast<ET*>(pSrc), static_cast<ET>(defaultValue), flags, szHelp, pChangeFunc, bAllowModify);
	}


	template<class T, class U>
	static CRY_FORCE_INLINE ICVar* RegisterImpl(non_enum_tag, const char* szName, T* pSrc, U defaultValue, int flags = 0, const char* szHelp = "", ConsoleVarFunc pChangeFunc = nullptr, bool bAllowModify = true)
	{
		static_assert(std::is_same<T, int>::value || std::is_same<T, float>::value || std::is_same<T, const char*>::value, "Invalid template type!");
		static_assert(std::is_convertible<U, T>::value, "Invalid default value type!");
		CRY_ASSERT(gEnv && gEnv->pConsole);
		if (gEnv && gEnv->pConsole)
		{
			MODULE_REGISTER_CVAR(szName);
			return gEnv->pConsole->Register(szName, pSrc, static_cast<T>(defaultValue), flags, szHelp, pChangeFunc, bAllowModify);
		}
		else
		{
			return nullptr;
		}
	}
};

#undef MODULE_REGISTER_COMMAND
#undef MODULE_REGISTER_CVAR

#if defined(_RELEASE) && !CRY_PLATFORM_DESKTOP
	#ifndef LOG_CONST_CVAR_ACCESS
		#error LOG_CONST_CVAR_ACCESS should be defined in ProjectDefines.h
	#endif

	#include "IConsole.h"
namespace Detail
{
template<typename T>
struct SQueryTypeEnum;
template<>
struct SQueryTypeEnum<int>
{
	static const int type = CVAR_INT;
	static int ParseString(const char* s) { return atoi(s); }
};
template<>
struct SQueryTypeEnum<float>
{
	static const int type = CVAR_FLOAT;
	static float ParseString(const char* s) { return (float)atof(s); }
};

template<typename T>
struct SDummyCVar : ICVar
{
	const T      value;
	#if LOG_CONST_CVAR_ACCESS
	mutable bool bWasRead;
	mutable bool bWasChanged;
	SDummyCVar(T value) : value(value), bWasChanged(false), bWasRead(false) {}
	#else
	SDummyCVar(T value) : value(value) {}
	#endif

	void WarnUse() const
	{
	#if LOG_CONST_CVAR_ACCESS
		if (!bWasRead)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[CVAR] Read from const CVar '%s' via name look-up, this is non-optimal", GetName());
			bWasRead = true;
		}
	#endif
	}

	void InvalidAccess() const
	{
	#if LOG_CONST_CVAR_ACCESS
		if (!bWasChanged)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVAR] Write to const CVar '%s' with wrong value '%f' was ignored. This indicates a bug in code or a config file", GetName(), GetFVal());
			bWasChanged = true;
		}
	#endif
	}

	void            Release() override                                             {}
	int             GetIVal() const override                                       { WarnUse(); return static_cast<int>(value); }
	int64           GetI64Val() const override                                     { WarnUse(); return static_cast<int64>(value); }
	float           GetFVal() const override                                       { WarnUse(); return static_cast<float>(value); }
	const char*     GetString() const override                                     { return ""; }
	const char*     GetDataProbeString() const override                            { return ""; }
	void            Set(const char* s) override                                    { if (SQueryTypeEnum<T>::ParseString(s) != value) InvalidAccess(); }
	void            ForceSet(const char* s) override                               { Set(s); }
	void            Set(const float f) override                                    { if (static_cast<T>(f) != value) InvalidAccess(); }
	void            Set(const int i) override                                      { if (static_cast<T>(i) != value) InvalidAccess(); }
	void            ClearFlags(int flags) override                                 {}
	int             GetFlags() const override                                      { return VF_CONST_CVAR | VF_READONLY; }
	int             SetFlags(int flags) override                                   { return 0; }
	int             GetType() override                                             { return SQueryTypeEnum<T>::type; }
	const char*     GetHelp() override                                             { return NULL; }
	bool            IsConstCVar() const override                                   { return true; }
	void            SetOnChangeCallback(ConsoleVarFunc pChangeFunc) override       { (void)pChangeFunc; }
	uint64          AddOnChangeFunctor(const SFunctor& /*changeFunctor*/) override { return 0;  }
	uint64          GetNumberOfOnChangeFunctors() const override                   { return 0; }
	const SFunctor& GetOnChangeFunctor(uint64 nFunctorIndex) const override        { InvalidAccess();  static SFunctor oDummy; return oDummy; }
	bool            RemoveOnChangeFunctor(const uint64 nElement) override          { return true; }
	ConsoleVarFunc  GetOnChangeCallback() const override                           { InvalidAccess(); return NULL; }
	void            GetMemoryUsage(class ICrySizer* pSizer) const override         {}
	int             GetRealIVal() const override                                   { return GetIVal(); }
	void            SetDataProbeString(const char* pDataProbeString)               { InvalidAccess(); }
};
}

	#define REGISTER_DUMMY_CVAR(type, name, value)                                                        \
	  do {                                                                                                \
	    static struct DummyCVar : Detail::SDummyCVar<type>                                                \
	    {                                                                                                 \
	      DummyCVar() : Detail::SDummyCVar<type>(value) {}                                                \
	      const char* GetName() const { return name; }                                                    \
	    } DummyStaticInstance;                                                                            \
	    if (!(gEnv->pConsole != nullptr ? ConsoleRegistrationHelper::Register(&DummyStaticInstance) : 0)) \
	    {                                                                                                 \
	      __debugbreak();                                                                                 \
	      CryFatalError("Can not register dummy CVar");                                                   \
	    }                                                                                                 \
	  } while (0)

	#define CONSOLE_CONST_CVAR_MODE
	#define DeclareConstIntCVar(name, defaultValue)                          enum : int { name = (defaultValue) }
	#define DeclareStaticConstIntCVar(name, defaultValue)                    enum : int { name = (defaultValue) }
	#define DefineConstIntCVarName(strname, name, defaultValue, flags, help) { static_assert(static_cast<int>(defaultValue) == static_cast<int>(name), "Unexpected value!"); REGISTER_DUMMY_CVAR(int, strname, defaultValue); }
	#define DefineConstIntCVar(name, defaultValue, flags, help)              { static_assert(static_cast<int>(defaultValue) == static_cast<int>(name), "Unexpected value!"); REGISTER_DUMMY_CVAR(int, ( # name), defaultValue); }

//! DefineConstIntCVar2 is deprecated, any such instance can be converted to the 3 variant by removing the quotes around the first parameter.
	#define DefineConstIntCVar3(name, _var_, defaultValue, flags, help) { static_assert(static_cast<int>(defaultValue) == static_cast<int>(_var_), "Unexpected value!"); REGISTER_DUMMY_CVAR(int, name, defaultValue); }
	#define AllocateConstIntCVar(scope, name)

	#define DefineConstFloatCVar(name, flags, help) { REGISTER_DUMMY_CVAR(float, ( # name), name ## Default); }
	#define DeclareConstFloatCVar(name)
	#define DeclareStaticConstFloatCVar(name)
	#define AllocateConstFloatCVar(scope, name)

#else

	#define DeclareConstIntCVar(name, defaultValue)       int name
	#define DeclareStaticConstIntCVar(name, defaultValue) static int name
	#define DefineConstIntCVarName(strname, name, defaultValue, flags, help) ConsoleRegistrationHelper::Register(strname, &name, defaultValue, flags | CONST_CVAR_FLAGS, CVARHELP(help))
	#define DefineConstIntCVar(name, defaultValue, flags, help) ConsoleRegistrationHelper::Register(( # name), &name, defaultValue, flags | CONST_CVAR_FLAGS, CVARHELP(help), nullptr, false)

//! DefineConstIntCVar2 is deprecated, any such instance can be converted to the 3 variant by removing the quotes around the first parameter.
	#define DefineConstIntCVar3(_name, _var, _def_val, _flags, help) ConsoleRegistrationHelper::Register(_name, &(_var), (_def_val), (_flags) | CONST_CVAR_FLAGS, CVARHELP(help), nullptr, false)
	#define AllocateConstIntCVar(scope, name) int scope::name

	#define DefineConstFloatCVar(name, flags, help) ConsoleRegistrationHelper::Register(( # name), &name, name ## Default, flags | CONST_CVAR_FLAGS, CVARHELP(help), nullptr, false)
	#define DeclareConstFloatCVar(name)         float name
	#define DeclareStaticConstFloatCVar(name)   static float name
	#define AllocateConstFloatCVar(scope, name) float scope::name
#endif

//! The following macros allow the help text to be easily stripped out.

//! Preferred way to register a CVar
#define REGISTER_CVAR(_var, _def_val, _flags, _comment) ConsoleRegistrationHelper::Register(( # _var), &(_var), (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register a CVar with a callback
#define REGISTER_CVAR_CB(_var, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::Register(( # _var), &(_var), (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register a string CVar
#define REGISTER_STRING(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterString(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register a string CVar with a callback
#define REGISTER_STRING_CB(_name, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::RegisterString(_name, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register an int CVar
#define REGISTER_INT(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterInt(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register an int CVar with a callback
#define REGISTER_INT_CB(_name, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::RegisterInt(_name, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register an int64 CVar
#define REGISTER_INT64(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterInt64(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Preferred way to register a float CVar
#define REGISTER_FLOAT(_name, _def_val, _flags, _comment) ConsoleRegistrationHelper::RegisterFloat(_name, (_def_val), (_flags), CVARHELP(_comment))

//! Offers more flexibility but more code is required
#define REGISTER_CVAR2(_name, _var, _def_val, _flags, _comment) ConsoleRegistrationHelper::Register(_name, _var, (_def_val), (_flags), CVARHELP(_comment))

//! Offers more flexibility but more code is required
#define REGISTER_CVAR2_CB(_name, _var, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::Register(_name, _var, (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Offers more flexibility but more code is required, explicit address taking of destination variable
#define REGISTER_CVAR3(_name, _var, _def_val, _flags, _comment) ConsoleRegistrationHelper::Register(_name, &(_var), (_def_val), (_flags), CVARHELP(_comment))

//! Offers more flexibility but more code is required, explicit address taking of destination variable
#define REGISTER_CVAR3_CB(_name, _var, _def_val, _flags, _comment, _onchangefunction) ConsoleRegistrationHelper::Register(_name, &(_var), (_def_val), (_flags), CVARHELP(_comment), _onchangefunction)

//! Preferred way to register a console command
#define REGISTER_COMMAND(_name, _func, _flags, _comment) ConsoleRegistrationHelper::AddCommand(_name, _func, (_flags), CVARHELP(_comment))

//! Preferred way to register an externally managed console command
#define REGISTER_MANAGED_COMMAND(_name, _func, _flags, _comment) ConsoleRegistrationHelper::AddCommand(_name, _func, (_flags), CVARHELP(_comment), true)

////////////////////////////////////////////////////////////////////////////////
//! Development only cvars.
//! (1) Registered as real cvars *in non release builds only*.
//! (2) Can still be manipulated in release by the mapped variable, so not the same as const cvars.
//! (3) Any 'OnChanged' callback will need guarding against in release builds since the cvar won't exist.
//! (4) Any code that tries to get ICVar* will need guarding against in release builds since the cvar won't exist.
//! ILLEGAL_DEV_FLAGS is a mask of all those flags which make no sense in a _DEV_ONLY or _DEDI_ONLY cvar since the.
//! Cvar potentially won't exist in a release build.
#define ILLEGAL_DEV_FLAGS (VF_NET_SYNCED | VF_CHEAT | VF_CHEAT_ALWAYS_CHECK | VF_CHEAT_NOCHECK | VF_READONLY | VF_CONST_CVAR)

#if defined(_RELEASE)
	#define REGISTER_CVAR_DEV_ONLY(_var, _def_val, _flags, _comment)                               NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val
	#define REGISTER_CVAR_CB_DEV_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val /* _onchangefunction consumed; callback not available */
	#define REGISTER_STRING_DEV_ONLY(_name, _def_val, _flags, _comment)                            NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_STRING_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_INT_DEV_ONLY(_name, _def_val, _flags, _comment)                               NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_INT_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_INT64_DEV_ONLY(_name, _def_val, _flags, _comment)                             NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_FLOAT_DEV_ONLY(_name, _def_val, _flags, _comment)                             NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")                  /* consumed; pure cvar not available */
	#define REGISTER_CVAR2_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); *(_var) = _def_val
	#define REGISTER_CVAR2_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); *(_var) = _def_val
	#define REGISTER_CVAR3_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val
	#define REGISTER_CVAR3_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) NULL; static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!"); _var = _def_val
	#define REGISTER_COMMAND_DEV_ONLY(_name, _func, _flags, _comment)                              /* consumed; command not available */
#else
	#define REGISTER_CVAR_DEV_ONLY(_var, _def_val, _flags, _comment)                               REGISTER_CVAR(_var, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR_CB_DEV_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         REGISTER_CVAR_CB(_var, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_DEV_ONLY(_name, _def_val, _flags, _comment)                            REGISTER_STRING(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      REGISTER_STRING_CB(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_DEV_ONLY(_name, _def_val, _flags, _comment)                               REGISTER_INT(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_CB_DEV_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         REGISTER_INT_CB(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT64_DEV_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_INT64(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_FLOAT_DEV_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_FLOAT(_name, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR2(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR2_CB(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_DEV_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR3(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_CB_DEV_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR3_CB(_name, _var, _def_val, ((_flags) | VF_DEV_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_COMMAND_DEV_ONLY(_name, _func, _flags, _comment)                              REGISTER_COMMAND(_name, _func, ((_flags) | VF_DEV_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
#endif // defined(_RELEASE)
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//! Dedicated server only cvars
//! (1) Registered as real cvars in all non release builds
//! (2) Registered as real cvars in release on dedi servers only, otherwise treated as DEV_ONLY type cvars (see above)
#if defined(_RELEASE) && defined(DEDICATED_SERVER)
	#define REGISTER_CVAR_DEDI_ONLY(_var, _def_val, _flags, _comment)                               REGISTER_CVAR(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR_CB_DEDI_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         REGISTER_CVAR_CB(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_DEDI_ONLY(_name, _def_val, _flags, _comment)                            REGISTER_STRING(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_STRING_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      REGISTER_STRING_CB(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_DEDI_ONLY(_name, _def_val, _flags, _comment)                               REGISTER_INT(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         REGISTER_INT_CB(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_INT64_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_INT64(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_FLOAT_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_FLOAT(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR2(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR2_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR2_CB(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR3(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_CVAR3_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR3_CB(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
	#define REGISTER_COMMAND_DEDI_ONLY(_name, _func, _flags, _comment)                              REGISTER_COMMAND(_name, _func, ((_flags) | VF_DEDI_ONLY), _comment); static_assert((_flags & ILLEGAL_DEV_FLAGS) == 0, "Flags must not contain any development flags!")
#else
	#define REGISTER_CVAR_DEDI_ONLY(_var, _def_val, _flags, _comment)                               REGISTER_CVAR_DEV_ONLY(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR_CB_DEDI_ONLY(_var, _def_val, _flags, _comment, _onchangefunction)         REGISTER_CVAR_CB_DEV_ONLY(_var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_STRING_DEDI_ONLY(_name, _def_val, _flags, _comment)                            REGISTER_STRING_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_STRING_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)      REGISTER_STRING_CB_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_INT_DEDI_ONLY(_name, _def_val, _flags, _comment)                               REGISTER_INT_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_INT_CB_DEDI_ONLY(_name, _def_val, _flags, _comment, _onchangefunction)         REGISTER_INT_CB_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_INT64_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_INT64_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_FLOAT_DEDI_ONLY(_name, _def_val, _flags, _comment)                             REGISTER_FLOAT_DEV_ONLY(_name, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR2_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR2_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR2_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR2_CB_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_CVAR3_DEDI_ONLY(_name, _var, _def_val, _flags, _comment)                       REGISTER_CVAR3_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment)
	#define REGISTER_CVAR3_CB_DEDI_ONLY(_name, _var, _def_val, _flags, _comment, _onchangefunction) REGISTER_CVAR3_CB_DEV_ONLY(_name, _var, _def_val, ((_flags) | VF_DEDI_ONLY), _comment, _onchangefunction)
	#define REGISTER_COMMAND_DEDI_ONLY(_name, _func, _flags, _comment)                              REGISTER_COMMAND_DEV_ONLY(_name, _func, ((_flags) | VF_DEDI_ONLY), _comment)
#endif // defined(_RELEASE)
//////////////////////////////////////////////////////////////////////////////

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

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler.h>

inline CryGUID CryGUID::Create()
{
	CryGUID guid;
	gEnv->pSystem->FillRandomMT(reinterpret_cast<uint32*>(&guid), sizeof(guid) / sizeof(uint32));
	MEMORY_RW_REORDERING_BARRIER;
	return guid;
}
