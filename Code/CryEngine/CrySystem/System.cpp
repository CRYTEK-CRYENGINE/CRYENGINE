// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "System.h"
#include <time.h>
//#include "ini_vars.h"
#include <CryCore/Platform/CryLibrary.h>

#if defined(_RELEASE) && CRY_PLATFORM_DURANGO //note: check if orbis needs this
//exclude some not needed functionality for release console builds
	#define EXCLUDE_UPDATE_ON_CONSOLE
#endif

#if CRY_PLATFORM_LINUX
	#include <execinfo.h> // for backtrace
#endif

#if CRY_PLATFORM_ANDROID
	#include <unwind.h> // for _Unwind_Backtrace and _Unwind_GetIP
#endif

#include <CryNetwork/INetwork.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryRenderer/IRenderer.h>
#include <CrySystem/File/ICryPak.h>
#include <CryMovie/IMovieSystem.h>
#include <ServiceNetwork.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryInput/IInput.h>
#include <CrySystem/ILog.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CrySystem/IProcess.h>
#include <CrySystem/IBudgetingSystem.h>
#include <CryGame/IGameFramework.h>
#include <CryNetwork/INotificationNetwork.h>
#include <CrySystem/ICodeCheckpointMgr.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include "TestSystemLegacy.h"             // CTestSystem
#include "VisRegTest.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <CryMono/IMonoRuntime.h>
#include <CrySchematyc/ICore.h>
#include <CrySchematyc2/IFramework.h>

#include "CryPak.h"
#include "XConsole.h"
#include "CrySizerStats.h"
#include "CrySizerImpl.h"
#include "NotificationNetwork.h"
#include <CrySystem/Profilers/ProfileLog.h>
#include <CryString/CryPath.h>

#include "XML/xml.h"
#include "XML/ReadWriteXMLSink.h"

#include "StreamEngine/StreamEngine.h"
#include "PhysRenderer.h"

#include "LocalizedStringManager.h"
#include "XML/XmlUtils.h"
#include "Serialization/ArchiveHost.h"
#include "ThreadProfiler.h"
#include <CrySystem/Profilers/IDiskProfiler.h>
#include "SystemEventDispatcher.h"
#include "HardwareMouse.h"
#include "ServerThrottle.h"
#include <CryMemory/ILocalMemoryUsage.h>
#include "ResourceManager.h"
#include "MemoryManager.h"
#include "LoadingProfiler.h"
#include <CryLiveCreate/ILiveCreateHost.h>
#include <CryLiveCreate/ILiveCreateManager.h>
#include "OverloadSceneManager/OverloadSceneManager.h"
#include <CryThreading/IThreadManager.h>
#include <CryReflection/IReflection.h>

#include <CrySystem/ZLib/IZLibCompressor.h>
#include <CrySystem/ZLib/IZlibDecompressor.h>
#include <CrySystem/ZLib/ILZ4Decompressor.h>
#include <zlib.h>
#include "RemoteConsole/RemoteConsole.h"
#include "ImeManager.h"
#include "BootProfiler.h"
#include "Watchdog.h"
#include "NullImplementation/NULLAudioSystems.h"
#include "NullImplementation/NULLRenderAuxGeom.h"

#include <CryMath/PNoise3.h>
#include <CryString/StringUtils.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include "CryWaterMark.h"

#include "ExtensionSystem/CryPluginManager.h"
#include "ProjectManager/ProjectManager.h"

#include "DebugCallStack.h"
#include "ManualFrameStep.h"

WATERMARKDATA(_m);

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	#include <CrySystem/Scaleform/IScaleformHelper.h>
#endif

#include "HMDManager.h"

#include <../CryAction/ILevelSystem.h>
#include <../CryAction/IViewSystem.h>

#include <CryCore/CrtDebugStats.h>
#include "Interprocess/StatsAgent.h"

#if CRY_PLATFORM_WINDOWS
#include <timeapi.h>
#include <algorithm>
#endif

// Define global cvars.
SSystemCVars g_cvars;

#include <CrySystem/ITextModeConsole.h>

extern int CryMemoryGetAllocatedSize();

// these heaps are used by underlying System structures
// to allocate, accordingly, small (like elements of std::set<..*>) and big (like memory for reading files) objects
// hopefully someday we'll have standard MT-safe heap
//CMTSafeHeap g_pakHeap;
CMTSafeHeap* g_pPakHeap = 0;// = &g_pakHeap;

//////////////////////////////////////////////////////////////////////////
#include "Validator.h"

#if CRY_PLATFORM_ANDROID
namespace
{
struct Callstack
{
	Callstack()
		: addrs(NULL)
		, ignore(0)
		, count(0)
	{
	}
	Callstack(void** addrs, size_t ignore, size_t count)
	{
		this->addrs = addrs;
		this->ignore = ignore;
		this->count = count;
	}
	void** addrs;
	size_t ignore;
	size_t count;
};

static _Unwind_Reason_Code trace_func(struct _Unwind_Context* context, void* arg)
{
	Callstack* cs = static_cast<Callstack*>(arg);
	if (cs->count)
	{
		void* ip = (void*) _Unwind_GetIP(context);
		if (ip)
		{
			if (cs->ignore)
			{
				cs->ignore--;
			}
			else
			{
				cs->addrs[0] = ip;
				cs->addrs++;
				cs->count--;
			}
		}
	}
	return _URC_NO_REASON;
}

static int Backtrace(void** addrs, size_t ignore, size_t size)
{
	Callstack cs(addrs, ignore, size);
	_Unwind_Backtrace(trace_func, (void*) &cs);
	return size - cs.count;
}
}
#endif

#if defined(CVARS_WHITELIST)
struct SCVarsWhitelistConfigSink : public ILoadConfigurationEntrySink
{
	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
	{
		ICVarsWhitelist* pCVarsWhitelist = gEnv->pSystem->GetCVarsWhiteList();
		bool whitelisted = (pCVarsWhitelist) ? pCVarsWhitelist->IsWhiteListed(szKey, false) : true;
		if (whitelisted)
		{
			gEnv->pConsole->LoadConfigVar(szKey, szValue);
		}
	}
} g_CVarsWhitelistConfigSink;
#endif // defined(CVARS_WHITELIST)

/////////////////////////////////////////////////////////////////////////////////
// System Implementation.
//////////////////////////////////////////////////////////////////////////
CSystem::CSystem(const SSystemInitParams& startupParams)
#if defined(SYS_ENV_AS_STRUCT)
	: m_env(gEnv)
#elif !defined(CRY_IS_MONOLITHIC_BUILD)
	: m_gameLibrary(nullptr)
#endif
{
	m_pSystemEventDispatcher = new CSystemEventDispatcher(); // Must be first.
	m_pSystemEventDispatcher->RegisterListener(this, "CSystem");

	//////////////////////////////////////////////////////////////////////////
	// Clear environment.
	//////////////////////////////////////////////////////////////////////////
	memset(&m_env, 0, sizeof(m_env));

	//////////////////////////////////////////////////////////////////////////
	// Reset handles.
	memset(&m_dll, 0, sizeof(m_dll));
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Initialize global environment interface pointers.
	m_env.pSystem = this;
	m_env.pTimer = &m_Time;
	m_env.pNameTable = &m_nameTable;
	m_env.pFrameProfileSystem = &m_FrameProfileSystem;
	m_env.bServer = false;
	m_env.bMultiplayer = false;
	m_env.bHostMigrating = false;
	m_env.bDeepProfiling = 0;
	m_env.bBootProfilerEnabledFrames = false;
	m_env.callbackStartSection = 0;
	m_env.callbackEndSection = 0;

	m_env.bUnattendedMode = false;
	m_env.bTesting = false;

#if CRY_PLATFORM_DURANGO
	m_env.ePLM_State = EPLM_UNDEFINED;
#endif

	m_env.SetFMVIsPlaying(false);
	m_env.SetCutsceneIsPlaying(false);

	m_env.szDebugStatus[0] = '\0';

#if CRY_PLATFORM_DESKTOP
	m_env.SetIsClient(false);
#endif
#if !defined(SYS_ENV_AS_STRUCT)
	gEnv = &m_env;
#endif
	//////////////////////////////////////////////////////////////////////////

	m_randomGenerator.SetState(m_Time.GetAsyncTime().GetMicroSecondsAsInt64());

	m_pStreamEngine = nullptr;
	m_PhysThread = nullptr;

	m_pIFont = nullptr;
	m_pTestSystem = nullptr;
	m_pVisRegTest = nullptr;
	m_rWidth = nullptr;
	m_rHeight = nullptr;
	m_rColorBits = nullptr;
	m_rDepthBits = nullptr;
	m_cvSSInfo = nullptr;
	m_rStencilBits = nullptr;
	m_rFullscreen = nullptr;
	m_rDriver = nullptr;
	m_pPhysicsLibrary = nullptr;
	m_sysNoUpdate = nullptr;
	m_pMemoryManager = nullptr;
	m_pProcess = nullptr;
	m_pMtState = nullptr;

	m_pValidator = nullptr;
	m_pCmdLine = nullptr;
	m_pDefaultValidator = nullptr;
	m_pIBudgetingSystem = nullptr;
	m_pIZLibCompressor = nullptr;
	m_pIZLibDecompressor = nullptr;
	m_pILZ4Decompressor = nullptr;
	m_pNULLRenderAuxGeom = nullptr;
	m_pLocalizationManager = nullptr;
	m_sys_physics_enable_MT = nullptr;
	m_sys_min_step = nullptr;
	m_sys_max_step = nullptr;

	m_pNotificationNetwork = nullptr;

	m_cvAIUpdate = nullptr;

	m_pUserCallback = nullptr;
#if defined(CVARS_WHITELIST)
	m_pCVarsWhitelist = nullptr;
	m_pCVarsWhitelistConfigSink = &g_CVarsWhitelistConfigSink;
#endif // defined(CVARS_WHITELIST)
	m_sys_memory_debug = nullptr;
	m_sysWarnings = nullptr;
	m_sysKeyboard = nullptr;
	m_sys_profile_watchdog_timeout = nullptr;
	m_sys_job_system_filter = nullptr;
	m_sys_job_system_enable = nullptr;
	m_sys_job_system_profiler = nullptr;
	m_sys_job_system_max_worker = nullptr;
	m_sys_spec = nullptr;
	m_sys_firstlaunch = nullptr;
	m_sys_enable_budgetmonitoring = nullptr;
	m_sys_preload = nullptr;
	m_sys_use_Mono = nullptr;
	m_sys_dll_ai = nullptr;
	m_sys_dll_response_system = nullptr;
	m_sys_user_folder = nullptr;

#if !defined(_RELEASE)
	m_sys_resource_cache_folder = nullptr;
#endif

	m_sys_initpreloadpacks = nullptr;
	m_sys_menupreloadpacks = nullptr;

	//	m_sys_filecache = nullptr;
	m_gpu_particle_physics = nullptr;
	m_pCpu = nullptr;

	m_bQuit = false;
	m_bShaderCacheGenMode = false;
	m_bRelaunch = false;
	m_iLoadingMode = 0;
	m_bEditor = false;
	m_bPreviewMode = false;
	m_bIgnoreUpdates = false;
	m_bNoCrashDialog = false;

#ifndef _RELEASE
	m_checkpointLoadCount = 0;
	m_loadOrigin = eLLO_Unknown;
	m_hasJustResumed = false;
	m_expectingMapCommand = false;
#endif

	m_nStrangeRatio = 1000;
	// no mem stats at the moment
	m_pMemStats = nullptr;
	m_pSizer = nullptr;
	m_pCVarQuit = nullptr;

	m_pDownloadManager = nullptr;
	m_bForceNonDevMode = false;
	m_bWasInDevMode = false;
	m_bInDevMode = false;
	m_bGameFolderWritable = false;

	m_nServerConfigSpec = CONFIG_VERYHIGH_SPEC;
	m_nMaxConfigSpec = CONFIG_ORBIS;

	//m_hPhysicsThread = INVALID_HANDLE_VALUE;
	//m_hPhysicsActive = INVALID_HANDLE_VALUE;
	//m_bStopPhysics = 0;
	//m_bPhysicsActive = 0;

	m_pProgressListener = nullptr;

	m_bPaused = false;
	m_bNoUpdate = false;
	m_nUpdateCounter = 0;
	m_iApplicationInstance = -1;

	m_pPhysRenderer = nullptr;

	m_root = PathUtil::AddSlash(PathUtil::GetEnginePath());

	m_pXMLUtils = new CXmlUtils(this);
	m_pArchiveHost = Serialization::CreateArchiveHost();

	m_pTestSystem = stl::make_unique<CTestSystemLegacy>(this);

	m_pMemoryManager = CryGetIMemoryManager();
	m_pResourceManager = new CResourceManager;
	m_pTextModeConsole = nullptr;
	m_pThreadProfiler = nullptr;
	m_pDiskProfiler = nullptr;
	m_ttMemStatSS = 0;

#if defined(ENABLE_LOADING_PROFILER)
	if (!startupParams.bShaderCacheGen)
	{
		CBootProfiler::GetInstance().Init(this);
	}
#endif

	InitThreadSystem();

	LOADING_TIME_PROFILE_SECTION_NAMED("CSystem Boot");

	m_pMiniGUI = nullptr;
	m_pPerfHUD = nullptr;

	m_pHmdManager = nullptr;
	m_sys_vr_support = nullptr;

	g_pPakHeap = new CMTSafeHeap;

	m_bUIFrameworkMode = false;

	m_PlatformOSCreateFlags = 0;

	// create job manager
	m_env.pJobManager = GetJobManagerInterface();

	m_UpdateTimesIdx = 0U;
	m_bNeedDoWorkDuringOcclusionChecks = false;

	m_PlatformOSCreateFlags = 0;

	m_bHasRenderedErrorMessage = false;
	
	m_pImeManager = nullptr;
	RegisterWindowMessageHandler(this);

	m_env.pConsole = new CXConsole;
	if (startupParams.pPrintSync)
		m_env.pConsole->AddOutputPrintSink(startupParams.pPrintSync);

	m_pPluginManager = new CCryPluginManager(startupParams);

	m_pUserAnalyticsSystem = new CUserAnalyticsSystem();
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
CSystem::~CSystem()
{
	ShutDown();

	SAFE_DELETE(m_pImeManager);
	UnregisterWindowMessageHandler(this);

	FreeLib(m_dll.hNetwork);
	FreeLib(m_dll.hAI);
	FreeLib(m_dll.hInput);
	FreeLib(m_dll.hScript);
	FreeLib(m_dll.hPhysics);
	FreeLib(m_dll.hEntitySystem);
	FreeLib(m_dll.hRenderer);
	FreeLib(m_dll.hFlash);
	FreeLib(m_dll.hFont);
	FreeLib(m_dll.hMovie);
	FreeLib(m_dll.hIndoor);
	FreeLib(m_dll.h3DEngine);
	FreeLib(m_dll.hAnimation);
	FreeLib(m_dll.hGame);
	FreeLib(m_dll.hSound);
	SAFE_DELETE(m_pVisRegTest);
	SAFE_DELETE(m_pThreadProfiler);
#if defined(USE_DISK_PROFILER)
	SAFE_DELETE(m_pDiskProfiler);
#endif
	SAFE_DELETE(m_pXMLUtils);
	SAFE_DELETE(m_pArchiveHost);
	SAFE_DELETE(m_pResourceManager);
	SAFE_DELETE(m_pSystemEventDispatcher);
	//	SAFE_DELETE(m_pMemoryManager);
	SAFE_DELETE(m_pNULLRenderAuxGeom);

	gEnv->pThreadManager->UnRegisterThirdPartyThread("Main");
	ShutDownThreadSystem();

	SAFE_DELETE(g_pPakHeap);

	m_pTestSystem.reset();

	m_env.pSystem = nullptr;
#if !defined(SYS_ENV_AS_STRUCT)
	gEnv = 0;
#endif

	// The FrameProfileSystem should clean up as late as possible as some modules create profilers during shutdown!
	m_FrameProfileSystem.Done();

#if CRY_PLATFORM_WINDOWS
	((DebugCallStack*)IDebugCallStack::instance())->uninstallErrorHandler();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::FreeLib(WIN_HMODULE hLibModule)
{
	if (hLibModule)
	{
		CryFreeLibrary(hLibModule);
	}
}

//////////////////////////////////////////////////////////////////////////
IStreamEngine* CSystem::GetStreamEngine()
{
	return m_pStreamEngine;
}

//////////////////////////////////////////////////////////////////////////
IRemoteConsole* CSystem::GetIRemoteConsole()
{
	return CRemoteConsole::GetInst();
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetForceNonDevMode(const bool bValue)
{
	m_bForceNonDevMode = bValue;
	if (bValue)
		SetDevMode(false);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::GetForceNonDevMode() const
{
	return m_bForceNonDevMode;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetDevMode(bool bEnable)
{
	if (bEnable)
		m_bWasInDevMode = true;
	m_bInDevMode = bEnable;
}

void LvlRes_export(IConsoleCmdArgs* pParams);

///////////////////////////////////////////////////
void CSystem::ShutDown()
{
	CryLogAlways("System Shutdown");

	SAFE_DELETE(m_pManualFrameStepController);

	m_FrameProfileSystem.Enable(false, false);

#if defined(ENABLE_LOADING_PROFILER)
	CLoadingProfilerSystem::ShutDown();
#endif

	if (m_pSystemEventDispatcher)
	{
		m_pSystemEventDispatcher->RemoveListener(this);
	}

	if (m_pUserCallback)
		m_pUserCallback->OnShutdown();

	GetIRemoteConsole()->Stop();

	SAFE_DELETE(m_pTextModeConsole);

	//////////////////////////////////////////////////////////////////////////
	// Interprocess Communication
	//////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_STATS_AGENT)
	CStatsAgent::ClosePipe();
#endif

	KillPhysicsThread();

	if (m_sys_firstlaunch)
		m_sys_firstlaunch->Set("0");

	if (m_bEditor)
	{
		// restore the old saved cvars
		if (m_env.pConsole->GetCVar("r_Width"))
			m_env.pConsole->GetCVar("r_Width")->Set(m_iWidth);
		if (m_env.pConsole->GetCVar("r_Height"))
			m_env.pConsole->GetCVar("r_Height")->Set(m_iHeight);
		if (m_env.pConsole->GetCVar("r_ColorBits"))
			m_env.pConsole->GetCVar("r_ColorBits")->Set(m_iColorBits);
	}

	if (m_bEditor && !m_bRelaunch)
	{
		SaveConfiguration();
	}

	//if (!m_bEditor && !bRelaunch)
#if !CRY_PLATFORM_DURANGO && !CRY_PLATFORM_ORBIS
	if (!m_bEditor)
	{
		if (m_pCVarQuit && m_pCVarQuit->GetIVal())
		{
			SaveConfiguration();

			// Dispatch the fast-shutdown event so other systems can do any last minute processing.
			if (m_pSystemEventDispatcher != NULL)
			{
				m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_FAST_SHUTDOWN, 0, 0);
			}

			if (m_env.pNetwork != NULL)
			{
				m_env.pNetwork->FastShutdown();
			}

			SAFE_RELEASE(m_env.pRenderer);
			FreeLib(m_dll.hRenderer);

			// Shut down audio as late as possible but before the console gets released!
			SAFE_RELEASE(m_env.pAudioSystem);

			// Log must be last thing released.
			SAFE_RELEASE(m_env.pProfileLogSystem);
			m_env.pLog->FlushAndClose();
			SAFE_RELEASE(m_env.pLog);   // creates log backup

	#if CRY_PLATFORM_WINDOWS
			((DebugCallStack*)IDebugCallStack::instance())->uninstallErrorHandler();
	#endif

	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
			return; //safe clean return
	#else
			// Commit files changes to the disk.
			_flushall();
			_exit(EXIT_SUCCESS);
	#endif
		}
	}
#endif

	// Dispatch the full-shutdown event in case this is not a fast-shutdown.
	if (m_pSystemEventDispatcher != NULL)
	{
		m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_FULL_SHUTDOWN, 0, 0);
	}

	//////////////////////////////////////////////////////////////////////////
	// Release Game.
	//////////////////////////////////////////////////////////////////////////
	if (m_env.pEntitySystem)
		m_env.pEntitySystem->Unload();

	if (m_env.pPhysicalWorld)
	{
		m_env.pPhysicalWorld->SetPhysicsStreamer(0);
		m_env.pPhysicalWorld->SetPhysicsEventClient(0);
	}

	UnloadSchematycModule();

	if (gEnv->pGameFramework != nullptr)
	{
		gEnv->pGameFramework->ShutDown();
	}

	UnloadEngineModule("CryAction");
	UnloadEngineModule("CryFlowGraph");
	SAFE_DELETE(m_pPluginManager);

	m_pPlatformOS.reset();

	if (gEnv->pMonoRuntime != nullptr)
	{
		gEnv->pMonoRuntime->Shutdown();
	}

	SAFE_DELETE(m_pUserAnalyticsSystem);
	if (m_sys_dll_response_system != nullptr)
	{
		UnloadEngineModule(m_sys_dll_response_system->GetString());
	}

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	if (m_env.pRenderer)
		m_env.pRenderer->FlushRTCommands(true, true, true);

	if (!gEnv->IsDedicated() && gEnv->pScaleformHelper)
	{
		gEnv->pScaleformHelper->Destroy();
		gEnv->pScaleformHelper = nullptr;
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Clear 3D Engine resources.
	if (m_env.p3DEngine)
		m_env.p3DEngine->UnloadLevel();
	//////////////////////////////////////////////////////////////////////////

	// Shutdown resource manager.
	m_pResourceManager->Shutdown();

	// The LiveCreate module must be deleted here since at shutdown, it's using other modules, please keep this location
	SAFE_DELETE(m_env.pLiveCreateHost);
	SAFE_DELETE(m_env.pLiveCreateManager);
	SAFE_RELEASE(m_env.pHardwareMouse);
	UnloadEngineModule("CryMovie");
	SAFE_DELETE(m_env.pServiceNetwork);
	UnloadEngineModule("CryAISystem");
	UnloadEngineModule("CryFont");
	UnloadEngineModule("CryNetwork");
	//	SAFE_RELEASE(m_env.pCharacterManager);
	UnloadEngineModule("CryAnimation");
	UnloadEngineModule("Cry3DEngine"); // depends on EntitySystem
	UnloadEngineModule("CryEntitySystem");

	SAFE_DELETE(m_pPhysRenderer); // Must be destroyed before unloading CryPhysics as it holds memory that was allocated by that module
	UnloadEngineModule("CryPhysics");

	UnloadEngineModule("CryMonoBridge");

	if (m_env.pConsole)
		((CXConsole*)m_env.pConsole)->FreeRenderResources();
	SAFE_RELEASE(m_pIZLibCompressor);
	SAFE_RELEASE(m_pIZLibDecompressor);
	SAFE_RELEASE(m_pILZ4Decompressor);
	SAFE_RELEASE(m_pIBudgetingSystem);

	SAFE_RELEASE(m_env.pRenderer);

	if (ICVar* pDriverCVar = m_env.pConsole->GetCVar("r_driver"))
	{
		const char* szRenderDriver = pDriverCVar->GetString();
		CloseRenderLibrary(szRenderDriver);
	}

	SAFE_RELEASE(m_env.pCodeCheckpointMgr);

	if (m_env.pLog)
		m_env.pLog->UnregisterConsoleVariables();

	GetIRemoteConsole()->UnregisterConsoleVariables();

	// Release console variables.

	SAFE_RELEASE(m_pCVarQuit);
	SAFE_RELEASE(m_rWidth);
	SAFE_RELEASE(m_rHeight);
	SAFE_RELEASE(m_rColorBits);
	SAFE_RELEASE(m_rDepthBits);
	SAFE_RELEASE(m_cvSSInfo);
	SAFE_RELEASE(m_rStencilBits);
	SAFE_RELEASE(m_rFullscreen);
	SAFE_RELEASE(m_rDriver);
	SAFE_RELEASE(m_pPhysicsLibrary);

	SAFE_RELEASE(m_sysWarnings);
	SAFE_RELEASE(m_sysKeyboard);
	SAFE_RELEASE(m_sys_profile_watchdog_timeout);
	SAFE_RELEASE(m_sys_job_system_filter);
	SAFE_RELEASE(m_sys_job_system_enable);
	SAFE_RELEASE(m_sys_job_system_profiler);
	SAFE_RELEASE(m_sys_job_system_max_worker);
	SAFE_RELEASE(m_sys_spec);
	SAFE_RELEASE(m_sys_firstlaunch);
	SAFE_RELEASE(m_sys_enable_budgetmonitoring);
	SAFE_RELEASE(m_sys_physics_enable_MT);
	SAFE_RELEASE(m_sys_min_step);
	SAFE_RELEASE(m_sys_max_step);

	//Purposely leaking the object as we do not want to block the MainThread waiting for the Watchdog thread to join
	if (m_pWatchdog != nullptr)
		m_pWatchdog->SignalStopWork();

	if (m_env.pInput)
	{
		m_env.pInput->ShutDown();
		m_env.pInput = NULL;
	}
	UnloadEngineModule("CryInput");

	SAFE_RELEASE(m_pNotificationNetwork);
	UnloadEngineModule("CryScriptSystem");

	SAFE_DELETE(m_pMemStats);
	SAFE_DELETE(m_pSizer);

	SAFE_DELETE(m_env.pOverloadSceneManager);

	// [VR] specific
	// CHmdManager shuts down the HDM devices on destruction.
	// It must happen AFTER m_env.pRenderer: the renderer may consult info
	// about the current HMD Device.
	// It should happen AFTER m_env.pInput: as CryInput may use elements controlled
	// by CHmdManager.
	SAFE_DELETE(m_pHmdManager);

#ifdef DOWNLOAD_MANAGER
	SAFE_RELEASE(m_pDownloadManager);
#endif //DOWNLOAD_MANAGER

	SAFE_DELETE(m_pLocalizationManager);

	//DebugStats(false, false);//true);
	//CryLogAlways("");
	//CryLogAlways("release mode memory manager stats:");
	//DumpMMStats(true);

	SAFE_DELETE(m_pCpu);

	SAFE_DELETE(m_pCmdLine);

	// Shut down audio as late as possible but before the streaming system and console get released!
	SAFE_RELEASE(m_env.pAudioSystem);
	UnloadEngineModule("CryAudioSystem");

	SAFE_DELETE(m_pProjectManager);

	// Shut down the CryPak system after audio!
	SAFE_DELETE(m_env.pCryPak);

	// Shut down the streaming system and console as late as possible and after audio!
	SAFE_DELETE(m_pStreamEngine);
	SAFE_RELEASE(m_env.pConsole);

	// Log must be last thing released.
	SAFE_RELEASE(m_env.pProfileLogSystem);
	m_env.pLog->FlushAndClose();
	SAFE_RELEASE(m_env.pLog); // creates log backup

	// DefaultValidator is used by the logging system, make sure to delete this member after logging system!
	SAFE_DELETE(m_pDefaultValidator);

#if defined(MAP_LOADING_SLICING)
	delete gEnv->pSystemScheduler;
#endif // defined(MAP_LOADING_SLICING)

	UnloadEngineModule("CryReflection");

#if CAPTURE_REPLAY_LOG
	CryGetIMemReplay()->Stop();
#endif

	// Fix to improve wait() time within third party APIs using sleep()
#if CRY_PLATFORM_WINDOWS
	TIMECAPS tc;
	UINT wTimerRes;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	{
		CryFatalError("Error while changing the system timer resolution!");
	}
	wTimerRes = std::min(std::max(tc.wPeriodMin, 1u), tc.wPeriodMax);
	timeEndPeriod(wTimerRes);
#endif // CRY_PLATFORM_WINDOWS
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void CSystem::Quit()
{
	CryLog("CSystem::Quit invoked from thread %" PRI_THREADID " (main is %" PRI_THREADID ")", GetCurrentThreadId(), gEnv->mMainThreadId);
	m_bQuit = true;

	if (m_pUserCallback)
		m_pUserCallback->OnQuit();
	if (gEnv->pCryPak && gEnv->pCryPak->GetLvlResStatus())
		LvlRes_export(0);       // executable was started with -LvlRes so it should export lvlres file on quit
	// Fast Quit.

	// clean up properly the console
	if (m_pTextModeConsole)
		m_pTextModeConsole->OnShutdown();

	if (m_env.pRenderer) 
	{
		ICVar* pCVarGamma = m_env.pConsole->GetCVar("r_Gamma");
		if (pCVarGamma)
			pCVarGamma->Set(1.0f); // prevent mysterious gamma snap back on quit (CE-15284)
		m_env.pRenderer->RestoreGamma();
	}

	if (m_pCVarQuit && m_pCVarQuit->GetIVal() != 0)
	{
		// Dispatch the fast-shutdown event so other systems can do any last minute processing.
		if (m_pSystemEventDispatcher != NULL)
		{
			m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_FAST_SHUTDOWN, 0, 0);
		}

		if (m_env.pNetwork)
			m_env.pNetwork->FastShutdown();

		// HACK! to save cvars on quit.
		SaveConfiguration();

		if (gEnv->pFlashUI)
			gEnv->pFlashUI->Shutdown();

		if (m_env.pRenderer)
		{
			m_env.pRenderer->StopRenderIntroMovies(false);
			m_env.pRenderer->StopLoadtimeFlashPlayback();
		}

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
		if (m_env.pRenderer)
			m_env.pRenderer->FlushRTCommands(true, true, true);

		if (gEnv->pScaleformHelper)
		{
			gEnv->pScaleformHelper->Destroy();
			gEnv->pScaleformHelper = nullptr;
		}
#endif

		if (m_env.pRenderer)
			m_env.pRenderer->ShutDownFast();

		CryLogAlways("System:Quit");

		// Shut down audio as late as possible but before the streaming system and console get released!
		SAFE_RELEASE(m_env.pAudioSystem);

		// Shut down the streaming system as late as possible and after audio!
		if (m_pStreamEngine)
			m_pStreamEngine->Shutdown();

		// Commit files changes to the disk.
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		fflush(NULL);
#else
		_flushall();
#endif
		//////////////////////////////////////////////////////////////////////////
		// Support relaunching for windows media center edition.
		//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
		if (m_pCmdLine && strstr(m_pCmdLine->GetCommandLine(), "ReLaunchMediaCenter") != 0)
		{
			ReLaunchMediaCenter();
		}
#endif

#if CAPTURE_REPLAY_LOG
		CryGetIMemReplay()->Stop();
#endif

		GetIRemoteConsole()->Stop();

		//////////////////////////////////////////////////////////////////////////
		// [marco] in test mode, kill the process and quit without performing full C libs cleanup
		// (for faster closing of application)
		CRY_ASSERT(m_pCVarQuit->GetIVal());
#if CRY_PLATFORM_ORBIS
		_Exit(0);
#elif CRY_PLATFORM_WINDOWS
		TerminateProcess(GetCurrentProcess(), 0);
#else
		_exit(0);
#endif
	}
#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_DURANGO && !CRY_PLATFORM_ORBIS
	PostQuitMessage(0);
#endif
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::IsQuitting() const
{
	return m_bQuit;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetIProcess(IProcess* process)
{
	m_pProcess = process;
	//if (m_pProcess)
	//m_pProcess->SetPMessage("");
}

//////////////////////////////////////////////////////////////////////////
// Physics thread task
//////////////////////////////////////////////////////////////////////////

class IPhysicsThreadTask : public IThread
{
public:
	virtual ~IPhysicsThreadTask()
	{
	}
	// Start accepting work on thread
	virtual void ThreadEntry() = 0;

	// Signals the thread that it should not accept anymore work and exit
	virtual void SignalStopWork() = 0;
};

class CPhysicsThreadTask : public IPhysicsThreadTask
{
public:

	CPhysicsThreadTask()
	{
		m_bStopRequested = 0;
		m_bIsActive = 0;
		m_stepRequested = 0;
		m_bProcessing = 0;
		m_doZeroStep = 0;
		m_lastStepTimeTaken = 0U;
		m_lastWaitTimeTaken = 0U;
	}

	virtual ~CPhysicsThreadTask()
	{
	}

	//////////////////////////////////////////////////////////////////////////
	// IThread implementation.
	//////////////////////////////////////////////////////////////////////////
	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		m_bStopRequested = 0;
		m_bIsActive = 1;

		float step, timeTaken, kSlowdown = 1.0f;
		int nSlowFrames = 0;
		int64 timeStart;
#ifdef ENABLE_LW_PROFILERS
		LARGE_INTEGER stepStart, stepEnd;
#endif
		LARGE_INTEGER waitStart, waitEnd;
		uint64 yieldBegin = 0U;

		while (true)
		{
			{
				CRY_PROFILE_REGION_WAITING(PROFILE_PHYSICS, "Wait - Physics Update");

				QueryPerformanceCounter(&waitStart);
				m_FrameEvent.Wait(); // Wait until new frame
				QueryPerformanceCounter(&waitEnd);
			}

			{
				CRY_PROFILE_REGION(PROFILE_PHYSICS, "Physics Update");

				m_lastWaitTimeTaken = waitEnd.QuadPart - waitStart.QuadPart;

				if (m_bStopRequested)
				{
					return;
				}
				bool stepped = false;
#ifdef ENABLE_LW_PROFILERS
				QueryPerformanceCounter(&stepStart);
#endif
				IGameFramework* pIGameFramework = gEnv->pGameFramework;
				while ((step = m_stepRequested) > 0 || m_doZeroStep)
				{
					stepped = true;
					m_stepRequested = 0;
					m_bProcessing = 1;
					m_doZeroStep = 0;

					PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();
					pVars->bMultithreaded = 1;
					gEnv->pPhysicalWorld->TracePendingRays();
					if (kSlowdown != 1.0f)
					{
						step = max(1, FtoI(step * kSlowdown * 50 - 0.5f)) * 0.02f;
						pVars->timeScalePlayers = 1.0f / max(kSlowdown, 0.2f);
					}
					else
						pVars->timeScalePlayers = 1.0f;
					step = min(step, pVars->maxWorldStep);
					timeStart = CryGetTicks();
					pIGameFramework->PrePhysicsTimeStep(step);
					gEnv->pPhysicalWorld->TimeStep(step);
					timeTaken = gEnv->pTimer->TicksToSeconds(CryGetTicks() - timeStart);
					if (timeTaken > step * 0.9f)
					{
						if (++nSlowFrames > 5)
							kSlowdown = step * 0.9f / timeTaken;
					}
					else
						kSlowdown = 1.0f, nSlowFrames = 0;
					gEnv->pPhysicalWorld->TracePendingRays(2);
					m_bProcessing = 0;
					//int timeSleep = (int)((m_timeTarget-gEnv->pTimer->GetAsyncTime()).GetMilliSeconds()*0.9f);
					//Sleep(max(0,timeSleep));
				}
				if (!stepped) CrySleep(0);
				m_FrameDone.Set();
#ifdef ENABLE_LW_PROFILERS
				QueryPerformanceCounter(&stepEnd);
				m_lastStepTimeTaken = stepEnd.QuadPart - stepStart.QuadPart;
#endif
			}
		}
	}

	// Signals the thread that it should not accept anymore work and exit
	virtual void SignalStopWork()
	{
		Pause();
		m_bStopRequested = 1;
		m_FrameEvent.Set();
		m_bIsActive = 0;
	}

	int Pause()
	{
		if (m_bIsActive)
		{
			PhysicsVars* vars = gEnv->pPhysicalWorld->GetPhysVars();
			vars->lastTimeStep = 0;
			m_bIsActive = 0;
			m_stepRequested = min((float)m_stepRequested, 2.f * vars->maxWorldStep);
			while (m_bProcessing);
			return 1;
		}
		gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep = 0;
		return 0;
	}
	int Resume()
	{
		if (!m_bIsActive)
		{
			m_bIsActive = 1;
			return 1;
		}
		return 0;
	}
	int IsActive() { return m_bIsActive; }
	int RequestStep(float dt)
	{
		if (m_bIsActive && dt > FLT_EPSILON)
		{
			m_stepRequested += dt;
			m_stepRequested = min((float)m_stepRequested, 10.f * gEnv->pPhysicalWorld->GetPhysVars()->maxWorldStep);
			if (dt <= 0.0f)
				m_doZeroStep = 1;
			m_FrameEvent.Set();
		}

		return m_bProcessing;
	}
	float  GetRequestedStep() { return m_stepRequested; }

	uint64 LastStepTaken() const
	{
		return m_lastStepTimeTaken;
	}

	uint64 LastWaitTime() const
	{
		return m_lastWaitTimeTaken;
	}

	void EnsureStepDone()
	{
		CRY_PROFILE_REGION_WAITING(PROFILE_SYSTEM, "SysUpdate:PhysicsEnsureDone");
		CRYPROFILE_SCOPE_PROFILE_MARKER("SysUpdate:PhysicsEnsureDone");

		if (m_bIsActive)
		{
			while (m_stepRequested > 0.0f || m_bProcessing)
			{
				m_FrameDone.Wait();
			}
		}
	}

protected:

	volatile int    m_bStopRequested;
	volatile int    m_bIsActive;
	volatile float  m_stepRequested;
	volatile int    m_bProcessing;
	volatile int    m_doZeroStep;
	volatile uint64 m_lastStepTimeTaken;
	volatile uint64 m_lastWaitTimeTaken;

	CryEvent        m_FrameEvent;
	CryEvent        m_FrameDone;
};

void CSystem::CreatePhysicsThread()
{
	if (!m_PhysThread)
	{
		m_PhysThread = new CPhysicsThreadTask;
		if (!gEnv->pThreadManager->SpawnThread(m_PhysThread, "Physics"))
		{
			CryFatalError("Error spawning \"Physics\" thread.");
		}
	}

	if (g_cvars.sys_limit_phys_thread_count)
	{
		PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();
		pVars->numThreads = max(1, min(pVars->numThreads, (int)m_pCpu->GetLogicalCPUCount() - 1));
	}
}

void CSystem::KillPhysicsThread()
{
	if (m_PhysThread)
	{
		m_PhysThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(m_PhysThread, eJM_Join);
		delete m_PhysThread;
		m_PhysThread = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
int CSystem::SetThreadState(ESubsystem subsys, bool bActive)
{
	switch (subsys)
	{
	case ESubsys_Physics:
		{
			if (m_PhysThread)
			{
				return bActive ? ((CPhysicsThreadTask*)m_PhysThread)->Resume() : ((CPhysicsThreadTask*)m_PhysThread)->Pause();
			}
		}
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SleepIfInactive()
{
	LOADING_TIME_PROFILE_SECTION;
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	// Disable throttling, when various Profilers are in use

	#if defined(CRY_PROFILE_MARKERS_USE_GPA)
	return;
	#endif

	#if ALLOW_BROFILER
	if (::Profiler::IsActive())
	{
		return;
	}
	#endif

	if (gEnv->pConsole->GetCVar("e_StatoscopeEnabled")->GetIVal())
	{
		return;
	}
#endif

	// ProcessSleep()
	if (m_env.IsDedicated() || m_bEditor || gEnv->bMultiplayer)
		return;

#if CRY_PLATFORM_WINDOWS
	if (GetIRenderer())
	{
		WIN_HWND hRendWnd = GetIRenderer()->GetHWND();
		if (!hRendWnd)
			return;

		// Loop here waiting for window to be activated.
		for (int nLoops = 0; nLoops < 5; nLoops++)
		{
			WIN_HWND hActiveWnd = ::GetActiveWindow();
			if (hActiveWnd == hRendWnd)
				break;

			if (m_hWnd)
			{
				PumpWindowMessage(true, m_hWnd);
			}
			if (gEnv->pGameFramework)
			{
				// During the time demo, do not sleep even in inactive window.
				if (gEnv->pGameFramework->IsInTimeDemo())
					break;
			}
			CrySleep(5);
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SleepIfNeeded()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM)

	static ICVar * pSysMaxFPS = NULL;
	static ICVar* pVSync = NULL;

	if (pSysMaxFPS == NULL && gEnv && gEnv->pConsole)
		pSysMaxFPS = gEnv->pConsole->GetCVar("sys_MaxFPS");
	if (pVSync == NULL && gEnv && gEnv->pConsole)
		pVSync = gEnv->pConsole->GetCVar("r_Vsync");

	int32 maxFPS = 0;

	if (m_env.IsDedicated())
	{
		const float maxRate = m_svDedicatedMaxRate->GetFVal();
		maxFPS = int32(maxRate);
	}
	else
	{
		if (pSysMaxFPS && pVSync)
		{
			uint32 vSync = pVSync->GetIVal();
			if (vSync == 0)
			{
				maxFPS = pSysMaxFPS->GetIVal();
				if (maxFPS == 0)
				{
					const bool bInLoading = (ESYSTEM_GLOBAL_STATE_RUNNING != m_systemGlobalState);
					if (bInLoading || IsPaused())
					{
						maxFPS = 60;
					}
				}
			}
		}
	}

	if (maxFPS > 0)
	{
		const int64 safeMarginMS = 5; // microseconds
		const int64 thresholdMs = (1000 * 1000) / (maxFPS);

		ITimer* pTimer = gEnv->pTimer;
		static int64 sTimeLast = pTimer->GetAsyncTime().GetMicroSecondsAsInt64();
		int64 currentTime = pTimer->GetAsyncTime().GetMicroSecondsAsInt64();
		for (;; )
		{
			const int64 frameTime = currentTime - sTimeLast;
			if (frameTime >= thresholdMs)
				break;
			if (thresholdMs - frameTime > 10 * 1000)
				CrySleep(1);
			else
				CrySleep(0);

			currentTime = pTimer->GetAsyncTime().GetMicroSecondsAsInt64();
		}

		m_lastTickTime = pTimer->GetAsyncTime();
		sTimeLast = m_lastTickTime.GetMicroSecondsAsInt64() + safeMarginMS;
	}
}

//////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
HWND g_hBreakWnd;
WNDPROC g_prevWndProc;
LRESULT CALLBACK BreakWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_HOTKEY)
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		CryDebugBreak();
	#else
		__asm int 3;
	#endif
	return CallWindowProc(g_prevWndProc, hWnd, msg, wParam, lParam);
}

struct SBreakHotKeyThread : public IThread
{
	SBreakHotKeyThread()
		: m_bRun(true)
	{
	}

	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		g_hBreakWnd = CreateWindowExW(0, L"Message", L"", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, 0, GetModuleHandleW(0), 0);
		g_prevWndProc = (WNDPROC)SetWindowLongPtrW(g_hBreakWnd, GWLP_WNDPROC, (LONG_PTR)BreakWndProc);
		RegisterHotKey(g_hBreakWnd, 0, 0, VK_PAUSE);
		MSG msg;
		while (GetMessage(&msg, g_hBreakWnd, 0, 0) && m_bRun)
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		UnregisterHotKey(g_hBreakWnd, 0);
	}

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork()
	{
		m_bRun = false;
	}

protected:
	volatile bool m_bRun;
};

SBreakHotKeyThread* g_pBreakHotkeyThread;
#endif

class BreakListener : public IInputEventListener
{
	bool OnInputEvent(const SInputEvent& ie)
	{
		if (ie.deviceType == eIDT_Keyboard && ie.keyId == eKI_Pause && ie.state & (eIS_Pressed | eIS_Down))
			CryDebugBreak();
		return true;
	}
} g_BreakListener;

volatile int g_lockInput = 0;

struct SBreakListenerTask : public IThread
{
	SBreakListenerTask()
	{
		m_bStop = 0;
		m_nBreakIdle = 0;
	}
	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		do
		{
			CrySleep(200);
			if (++m_nBreakIdle > 1)
			{
				WriteLock lock(g_lockInput);
				gEnv->pInput->Update(true);
				m_nBreakIdle = 0;
			}
		}
		while (!m_bStop);
	}
	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork()
	{
		m_bStop = 1;
	}
	volatile int m_bStop;
	int          m_nBreakIdle;
};
SBreakListenerTask g_BreakListenerTask;
bool g_breakListenerOn = false;

extern DWORD g_idDebugThreads[];
extern int g_nDebugThreads;
int prev_sys_float_exceptions = -1;

//////////////////////////////////////////////////////////////////////
void CSystem::PrePhysicsUpdate()
{
	if (m_env.pGameFramework)
	{
		m_env.pGameFramework->PrePhysicsUpdate();
	}

	if (m_pPluginManager)
	{
		m_pPluginManager->UpdateBeforePhysics();
	}

	//////////////////////////////////////////////////////////////////////
	//update entity system
	if (m_env.pEntitySystem && g_cvars.sys_entitysystem)
	{
		if (gEnv->pSchematyc != nullptr)
		{
			gEnv->pSchematyc->PrePhysicsUpdate();
		}

		if (gEnv->pSchematyc2 != nullptr)
		{
			gEnv->pSchematyc2->PrePhysicsUpdate();
		}

		m_env.pEntitySystem->PrePhysicsUpdate();
	}
}

void CSystem::RunMainLoop()
{
	if (m_bShaderCacheGenMode)
	{
		return;
	}

#if CRY_PLATFORM_WINDOWS
	if (!(gEnv && m_env.pSystem) || (!m_env.IsEditor() && !m_env.IsDedicated()))
	{
		if (m_env.pHardwareMouse != nullptr)
		{
			m_env.pHardwareMouse->DecrementCounter();
		}
		else
		{
			::ShowCursor(FALSE);
		}
	}
#else
	if (gEnv && m_env.pHardwareMouse)
		m_env.pHardwareMouse->DecrementCounter();
#endif

	for (;;)
	{
#if CRY_PLATFORM_DURANGO
		Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
#endif
		if (!DoFrame({}))
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////
bool CSystem::DoFrame(const SDisplayContextKey& displayContextKey, CEnumFlags<ESystemUpdateFlags> updateFlags)
{
	// The frame profile system already creates an "overhead" profile label
	// in StartFrame(). Hence we have to set the FRAMESTART before.
	CRY_PROFILE_FRAMESTART("Main");

	if (m_pManualFrameStepController != nullptr && m_pManualFrameStepController->Update() == EManualFrameStepResult::Block)
	{
		// Skip frame update
		return true;
	}

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	m_env.GetJobManager()->SetFrameStartTime(m_env.pTimer->GetAsyncTime());
#endif

	if (!updateFlags.Check(ESYSUPDATE_EDITOR))
	{
		m_env.pFrameProfileSystem->StartFrame();
	}

	CRY_PROFILE_REGION(PROFILE_SYSTEM, __FUNC__);
	CRYPROFILE_SCOPE_PROFILE_MARKER(__FUNC__);

	if (m_env.pGameFramework != nullptr)
	{
		m_env.pGameFramework->PreSystemUpdate();
	}
	
	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		m_pPluginManager->UpdateBeforeSystem();
	}

	if (ITextModeConsole* pTextModeConsole = GetITextModeConsole())
	{
		pTextModeConsole->BeginDraw();
	}

	// Tell the network to go to sleep
	if (m_env.pNetwork)
	{
		m_env.pNetwork->SyncWithGame(eNGS_SleepNetwork);
	}

	if (!m_env.IsEditing())  // Editor calls its own rendering update
		RenderBegin(displayContextKey);

	bool continueRunning = true;

	// The Editor is responsible for updating the system manually, so we should skip in that case.
	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		int pauseMode;

		if (m_env.pRenderer != nullptr && m_env.pRenderer->IsPost3DRendererEnabled())
		{
			pauseMode = 0;
			updateFlags |= ESYSUPDATE_IGNORE_AI;
		}
		else if(m_env.pGameFramework != nullptr)
		{
			pauseMode = (m_env.pGameFramework->IsGamePaused() || !m_env.pGameFramework->IsGameStarted()) ? 1 : 0;
		}
		else
		{
			pauseMode = 0;
		}

		if (!Update(updateFlags, pauseMode))
		{
			continueRunning = false;
		}
	}

	if (m_env.pGameFramework != nullptr)
	{
		if (!m_env.pGameFramework->PostSystemUpdate(m_hasWindowFocus, updateFlags))
		{
			continueRunning = false;
		}
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		m_pPluginManager->UpdateAfterSystem();
	}

	// Synchronize all animations so ensure that their computation have finished
	// Has to be done before view update, in case camera depends on a joint
	if (m_env.pCharacterManager && !IsLoading())
	{
		m_env.pCharacterManager->SyncAllAnimations();
	}

	if (m_env.pGameFramework != nullptr && !updateFlags.Check(ESYSUPDATE_EDITOR_ONLY) && !updateFlags.Check(ESYSUPDATE_EDITOR_AI_PHYSICS))
	{
		m_env.pGameFramework->PreFinalizeCamera(updateFlags);
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		m_pPluginManager->UpdateBeforeFinalizeCamera();
	}

	ICVar* pCameraFreeze = gEnv->pConsole->GetCVar("e_CameraFreeze");
	const bool isCameraFrozen = pCameraFreeze && pCameraFreeze->GetIVal() != 0;

	const CCamera& rCameraToSet = isCameraFrozen ? m_env.p3DEngine->GetRenderingCamera() : m_ViewCamera;
	m_env.p3DEngine->PrepareOcclusion(rCameraToSet);

	if (m_env.pGameFramework != nullptr)
	{
		m_env.pGameFramework->PreRender();
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		m_pPluginManager->UpdateBeforeRender();
	}

	Render();

	if (m_env.pGameFramework != nullptr)
	{
		m_env.pGameFramework->PostRender(updateFlags);
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		m_pPluginManager->UpdateAfterRender();
	}

	if (updateFlags & ESYSUPDATE_EDITOR_AI_PHYSICS)
	{
		return continueRunning;
	}

#if !defined(_RELEASE) && !CRY_PLATFORM_DURANGO
	RenderPhysicsHelpers();
#endif

	RenderEnd();

	if (m_env.pGameFramework != nullptr)
	{
		m_env.pGameFramework->PostRenderSubmit();
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		m_pPluginManager->UpdateAfterRenderSubmit();
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		if (m_env.pStatoscope)
		{
			m_env.pStatoscope->Tick();
		}

		if (ITextModeConsole* pTextModeConsole = GetITextModeConsole())
		{
			pTextModeConsole->EndDraw();
		}

		m_env.p3DEngine->SyncProcessStreamingUpdate();

		if (NeedDoWorkDuringOcclusionChecks())
		{
			DoWorkDuringOcclusionChecks();
		}

		m_env.pFrameProfileSystem->EndFrame();
	}

	SleepIfNeeded();

	return continueRunning;
}

//////////////////////////////////////////////////////////////////////
bool CSystem::Update(CEnumFlags<ESystemUpdateFlags> updateFlags, int nPauseMode)
{
	CRY_PROFILE_REGION(PROFILE_SYSTEM, "System: Update");
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM)
	CRYPROFILE_SCOPE_PROFILE_MARKER("CSystem::Update()");

#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	// do the dedicated sleep earlier than the frame profiler to avoid having it counted
	if (gEnv->IsDedicated())
	{
	#if defined(MAP_LOADING_SLICING)
		gEnv->pSystemScheduler->SchedulingSleepIfNeeded();
	#endif // defined(MAP_LOADING_SLICING)
	}
#endif //EXCLUDE_UPDATE_ON_CONSOLE
#if CAPTURE_REPLAY_LOG
	if (CryGetIMemoryManager() && CryGetIMemReplay())
	{
		CryGetIMemReplay()->AddFrameStart();
		if ((--m_ttMemStatSS) <= 0)
		{
			CryGetIMemReplay()->AddScreenshot();
			m_ttMemStatSS = 30;
		}
	}
#endif //CAPTURE_REPLAY_LOG

	gEnv->pOverloadSceneManager->Update();

	m_pPlatformOS->Tick(m_Time.GetRealFrameTime());

#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	if (g_cvars.sys_keyboard_break && !g_breakListenerOn)
	{
	#if CRY_PLATFORM_WINDOWS
		if (m_bEditor && !g_pBreakHotkeyThread)
		{
			g_pBreakHotkeyThread = new SBreakHotKeyThread();
			if (!gEnv->pThreadManager->SpawnThread(g_pBreakHotkeyThread, "WINAPI_BreakHotkeyListener"))
			{
				CryFatalError("Error spawning \"WINAPI_BreakHotkeyListener\" thread.");
			}
		}
	#endif
		if (!gEnv->pThreadManager->SpawnThread(&g_BreakListenerTask, "BreakListener"))
		{
			CryFatalError("Error spawning \"BreakListener\" thread.");
		}
		gEnv->pInput->AddEventListener(&g_BreakListener);
		g_breakListenerOn = true;
	}
	else if (!g_cvars.sys_keyboard_break && g_breakListenerOn)
	{
	#if CRY_PLATFORM_WINDOWS
		if (g_pBreakHotkeyThread)
			g_pBreakHotkeyThread->SignalStopWork();
	#endif
		gEnv->pInput->RemoveEventListener(&g_BreakListener);
		g_BreakListenerTask.SignalStopWork();
		g_breakListenerOn = false;
	}
#endif //EXCLUDE_UPDATE_ON_CONSOLE
#if CRY_PLATFORM_WINDOWS
	// enable/disable SSE fp exceptions (#nan and /0)
	// need to do it each frame since sometimes they are being reset
	_mm_setcsr(_mm_getcsr() & ~0x280 | (g_cvars.sys_float_exceptions > 0 ? 0 : 0x280));
#endif

	m_nUpdateCounter++;
#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	if (!m_sDelayedScreeenshot.empty())
	{
		gEnv->pRenderer->ScreenShot(m_sDelayedScreeenshot.c_str());
		m_sDelayedScreeenshot.clear();
	}

	// Check if game needs to be sleeping when not active.
	SleepIfInactive();

	if (m_pUserCallback)
		m_pUserCallback->OnUpdate();

	//////////////////////////////////////////////////////////////////////////
	// Enable/Disable floating exceptions.
	//////////////////////////////////////////////////////////////////////////
	prev_sys_float_exceptions += 1 + g_cvars.sys_float_exceptions & prev_sys_float_exceptions >> 31;
	if (prev_sys_float_exceptions != g_cvars.sys_float_exceptions)
	{
		prev_sys_float_exceptions = g_cvars.sys_float_exceptions;
		m_env.pThreadManager->EnableFloatExceptions((EFPE_Severity) g_cvars.sys_float_exceptions);                  // Set FP Exceptions for this thread
		m_env.pThreadManager->EnableFloatExceptionsForEachOtherThread((EFPE_Severity)g_cvars.sys_float_exceptions); // Set FP Exceptions for all other threads
	}
#endif //EXCLUDE_UPDATE_ON_CONSOLE
	//////////////////////////////////////////////////////////////////////////

	CTimeValue updateStart = gEnv->pTimer->GetAsyncTime();

	if (m_env.pLog)
		m_env.pLog->Update();

#if !defined(RELEASE) || defined(RELEASE_LOGGING)
	GetIRemoteConsole()->Update();
#endif

	if (gEnv->pLocalMemoryUsage != NULL)
	{
		gEnv->pLocalMemoryUsage->OnUpdate();
	}

	if (!gEnv->IsEditor() && gEnv->pRenderer)
	{
		CCamera rCamera = GetViewCamera();

		// if aspect ratio changes or is different from default we need to update camera
		const float fNewAspectRatio = gEnv->pRenderer->GetPixelAspectRatio();
		const int   nNewWidth       = gEnv->pRenderer->GetOverlayWidth();
		const int   nNewHeight      = gEnv->pRenderer->GetOverlayHeight();

		if ((fNewAspectRatio != rCamera.GetPixelAspectRatio()) ||
		    (nNewWidth       != rCamera.GetViewSurfaceX()) ||
		    (nNewHeight      != rCamera.GetViewSurfaceZ()))
		{
			rCamera.SetFrustum(
				nNewWidth,
				nNewHeight,
				rCamera.GetFov(),
				rCamera.GetNearPlane(),
				rCamera.GetFarPlane(),
				fNewAspectRatio);


			SetViewCamera(rCamera);
		}
	}

#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	if (m_pTestSystem)
		m_pTestSystem->Update();
#endif //EXCLUDE_UPDATE_ON_CONSOLE
	if (nPauseMode != 0)
		m_bPaused = true;
	else
		m_bPaused = false;

#if CRY_PLATFORM_WINDOWS
	if (m_bInDevMode && g_cvars.sys_vtune != 0)
	{
		static bool bVtunePaused = true;

		bool bPaused = false;

		if (GetISystem()->GetIInput())
		{
			bPaused = !(GetKeyState(VK_SCROLL) & 1);
		}
		{
			if (bVtunePaused && !bPaused)
			{
				GetIProfilingSystem()->VTuneResume();
			}
			if (!bVtunePaused && bPaused)
			{
				GetIProfilingSystem()->VTunePause();
			}
			bVtunePaused = bPaused;
		}
	}
#endif

	if (m_pStreamEngine)
	{
		m_pStreamEngine->Update();
	}
#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	if (m_bIgnoreUpdates)
		return true;
#endif //EXCLUDE_UPDATE_ON_CONSOLE

	const bool bNotLoading = !IsLoading();

	if (m_env.pCharacterManager)
	{
		if (bNotLoading)
		{
			m_env.pCharacterManager->Update(nPauseMode != 0);
		}
		else
		{
			m_env.pCharacterManager->DummyUpdate();
		}
	}

	//static bool sbPause = false;
	//bool bPause = false;
	bool bNoUpdate = false;
#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	//check what is the current process
	IProcess* pProcess = GetIProcess();
	if (!pProcess)
		return (true); //should never happen

	if (m_sysNoUpdate && m_sysNoUpdate->GetIVal())
	{
		bNoUpdate = true;
		updateFlags = { ESYSUPDATE_IGNORE_AI, ESYSUPDATE_IGNORE_PHYSICS };
	}

	//if ((pProcess->GetFlags() & PROC_MENU) || (m_sysNoUpdate && m_sysNoUpdate->GetIVal()))
	//		bPause = true;
	m_bNoUpdate = bNoUpdate;
#endif //EXCLUDE_UPDATE_ON_CONSOLE
	//check if we are quitting from the game
	if (IsQuitting())
		return (false);

#if CRY_PLATFORM_WINDOWS
	// process window messages
	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:PeekMessageW");

		if (m_hWnd && ::IsWindow((HWND)m_hWnd))
		{
			PumpWindowMessage(true, m_hWnd);
		}
	}
#endif

	//////////////////////////////////////////////////////////////////////
	//update time subsystem
	m_Time.UpdateOnFrameStart();

	// Don't do a thing if we're not in a level
	if (m_env.p3DEngine && bNotLoading)
		m_env.p3DEngine->OnFrameStart();

	//////////////////////////////////////////////////////////////////////
	// update rate limiter for dedicated server
	if (m_pServerThrottle.get())
		m_pServerThrottle->Update();

	//////////////////////////////////////////////////////////////////////
	// initial network update
	if (m_env.pNetwork)
	{
		m_env.pNetwork->SyncWithGame(eNGS_FrameStart);
	}

	//////////////////////////////////////////////////////////////////////////
	// Update script system.
	if (m_env.pScriptSystem && bNotLoading)
	{
		m_env.pScriptSystem->Update();
	}

	if (m_env.pInput)
	{
		bool updateInput =
		  !(updateFlags & ESYSUPDATE_EDITOR) ||
		  (updateFlags & ESYSUPDATE_EDITOR_AI_PHYSICS);
		if (updateInput)
		{
			//////////////////////////////////////////////////////////////////////
			//update input system
#if !CRY_PLATFORM_WINDOWS
			m_env.pInput->Update(true);
#else
			bool bFocus = (::GetForegroundWindow() == m_hWnd) || m_bEditor;
			{
				WriteLock lock(g_lockInput);
				m_env.pInput->Update(bFocus);
				g_BreakListenerTask.m_nBreakIdle = 0;
			}
#endif
		}
	}

	if (m_pHmdManager && m_sys_vr_support->GetIVal())
	{
		m_pHmdManager->UpdateTracking(eVRComponent_All);
	}

	if (m_env.pPhysicalWorld && m_env.pPhysicalWorld->GetPhysVars()->bForceSyncPhysics)
	{
		if (m_PhysThread)
			static_cast<CPhysicsThreadTask*>(m_PhysThread)->EnsureStepDone();
	}

	//////////////////////////////////////////////////////////////////////////
	//update the dynamic response system.
	if (m_env.pDynamicResponseSystem)
	{
		m_env.pDynamicResponseSystem->Update();
	}

	//////////////////////////////////////////////////////////////////////////
	//update the mono runtime
	if (m_env.pMonoRuntime)
	{
		m_env.pMonoRuntime->Update(updateFlags.UnderlyingValue(), nPauseMode);
	}

	//////////////////////////////////////////////////////////////////////
	//update console system
	if (m_env.pConsole)
	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:Console");

		if (!(updateFlags & ESYSUPDATE_EDITOR))
			m_env.pConsole->Update();
	}
#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	//////////////////////////////////////////////////////////////////////
	//update notification network system
	if (m_pNotificationNetwork)
	{
		m_pNotificationNetwork->Update();
	}
#endif //EXCLUDE_UPDATE_ON_CONSOLE

	// When in Editor and outside of Game Mode we will need to update the listeners here.
	// But when in Editor and in Game Mode the ViewSystem will update the listeners.
	if (!m_env.IsEditorGameMode())
	{
		if (updateFlags.Check(ESYSUPDATE_EDITOR) && !bNoUpdate && nPauseMode != 1)
		{
			gEnv->pGameFramework->GetIViewSystem()->UpdateAudioListeners();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Update Resource Manager.
	//////////////////////////////////////////////////////////////////////////
	m_pResourceManager->Update();

	//////////////////////////////////////////////////////////////////////
	// update physic system
	//static float time_zero = 0;
	if (!m_bUIFrameworkMode && bNotLoading)
	{
		if (m_sys_physics_enable_MT->GetIVal() > 0 && !gEnv->IsDedicated())
			CreatePhysicsThread();
		else
			KillPhysicsThread();

		static int g_iPausedPhys = 0;
		PhysicsVars* pVars = m_env.pPhysicalWorld->GetPhysVars();
		pVars->threadLag = 0;

		CPhysicsThreadTask* pPhysicsThreadTask = ((CPhysicsThreadTask*)m_PhysThread);
		if (!pPhysicsThreadTask)
		{
			CRY_PROFILE_REGION(PROFILE_SYSTEM, "SystemUpdate: AllAIAndPhysics");
			CRYPROFILE_SCOPE_PROFILE_MARKER("SystemUpdate: AllAIAndPhysics");

			//////////////////////////////////////////////////////////////////////
			// update entity system (a little bit) before physics
			if (nPauseMode != 1 && !bNoUpdate)
			{
				PrePhysicsUpdate();
			}

			// intermingle physics/AI updates so that if we get a big timestep (frame rate glitch etc) the
			// AI gets to steer entities before they travel over cliffs etc.
			float maxTimeStep = 0.0f;
			if (m_env.pAISystem)
				maxTimeStep = m_env.pAISystem->GetUpdateInterval();
			else
				maxTimeStep = 0.25f;
			int maxSteps = 1;
			float fCurTime = m_Time.GetCurrTime();
			float fPrevTime = m_env.pPhysicalWorld->GetPhysicsTime();
			float timeToDo = m_Time.GetFrameTime();//fCurTime - fPrevTime;
			if (m_env.bMultiplayer)
				timeToDo = m_Time.GetRealFrameTime();
			m_env.pPhysicalWorld->TracePendingRays();

			while (timeToDo > 0.0001f && maxSteps-- > 0)
			{
				float thisStep = min(maxTimeStep, timeToDo);
				timeToDo -= thisStep;

				if ((nPauseMode != 1) && !(updateFlags & ESYSUPDATE_IGNORE_PHYSICS) && g_cvars.sys_physics && !bNoUpdate)
				{
					CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:Physics");

					int iPrevTime = m_env.pPhysicalWorld->GetiPhysicsTime();
					//float fPrevTime=m_env.pPhysicalWorld->GetPhysicsTime();
					pVars->bMultithreaded = 0;
					pVars->timeScalePlayers = 1.0f;
					if (!(updateFlags & ESYSUPDATE_MULTIPLAYER))
						m_env.pPhysicalWorld->TimeStep(thisStep);
					else
					{
						//@TODO: fixed step in game.
						/*
						   if (m_env.pGame->UseFixedStep())
						   {
						   m_env.pPhysicalWorld->TimeStep(fCurTime-fPrevTime, 0);
						   int iCurTime = m_env.pPhysicalWorld->GetiPhysicsTime();

						   m_env.pPhysicalWorld->SetiPhysicsTime(m_env.pGame->SnapTime(iPrevTime));
						   int i, iStep=m_env.pGame->GetiFixedStep();
						   float fFixedStep = m_env.pGame->GetFixedStep();
						   for(i=min(20*iStep,m_env.pGame->SnapTime(iCurTime)-m_pGame->SnapTime(iPrevTime)); i>0; i-=iStep)
						   {
						    m_env.pGame->ExecuteScheduledEvents();
						    m_env.pPhysicalWorld->TimeStep(fFixedStep, ent_rigid|ent_skip_flagged);
						   }

						   m_env.pPhysicalWorld->SetiPhysicsTime(iPrevTime);
						   m_env.pPhysicalWorld->TimeStep(fCurTime-fPrevTime, ent_rigid|ent_flagged_only);

						   m_env.pPhysicalWorld->SetiPhysicsTime(iPrevTime);
						   m_env.pPhysicalWorld->TimeStep(fCurTime-fPrevTime, ent_living|ent_independent|ent_deleted);
						   }
						   else
						 */
						m_env.pPhysicalWorld->TimeStep(thisStep);

					}
					g_iPausedPhys = 0;
				}
				else if (!(g_iPausedPhys++ & 31))
					m_env.pPhysicalWorld->TimeStep(0); // make sure objects get all notifications; flush deleted ents
				gEnv->pPhysicalWorld->TracePendingRays(2);

				if (bNotLoading)
				{
					CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:PumpLoggedEvents");
					CRYPROFILE_SCOPE_PROFILE_MARKER("PumpLoggedEvents");
					m_env.pPhysicalWorld->PumpLoggedEvents();
				}

				// now AI
				if ((nPauseMode == 0) && !(updateFlags & ESYSUPDATE_IGNORE_AI) && g_cvars.sys_ai && !bNoUpdate)
				{
					CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:AI");
					//////////////////////////////////////////////////////////////////////
					//update AI system - match physics
					if (m_env.pAISystem && !m_cvAIUpdate->GetIVal() && g_cvars.sys_ai)
						m_env.pAISystem->Update(gEnv->pTimer->GetFrameStartTime(), gEnv->pTimer->GetFrameTime());
				}
			}

			// Make sure we don't lag too far behind
			if ((nPauseMode != 1) && !(updateFlags & ESYSUPDATE_IGNORE_PHYSICS))
			{
				if (fabsf(m_env.pPhysicalWorld->GetPhysicsTime() - fCurTime) > 0.01f)
				{
					//GetILog()->LogToConsole("Adjusting physical world clock by %.5f", fCurTime-m_env.pPhysicalWorld->GetPhysicsTime());
					m_env.pPhysicalWorld->SetPhysicsTime(fCurTime);
				}
			}
		}
		else
		{
			if (bNotLoading)
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:PumpLoggedEvents");
				CRYPROFILE_SCOPE_PROFILE_MARKER("PumpLoggedEvents");
				m_env.pPhysicalWorld->PumpLoggedEvents();
			}

			//////////////////////////////////////////////////////////////////////
			// update entity system (a little bit) before physics
			if (nPauseMode != 1 && !bNoUpdate)
			{
				PrePhysicsUpdate();
			}

			if ((nPauseMode != 1) && !(updateFlags & ESYSUPDATE_IGNORE_PHYSICS))
			{
				pPhysicsThreadTask->Resume();
				float lag = pPhysicsThreadTask->GetRequestedStep();

				if (pPhysicsThreadTask->RequestStep(m_Time.GetFrameTime()))
				{
					pVars->threadLag = lag + m_Time.GetFrameTime();
					//GetILog()->Log("Physics thread lags behind; accum time %.3f", pVars->threadLag);
				}

			}
			else
			{
				pPhysicsThreadTask->Pause();
				m_env.pPhysicalWorld->TracePendingRays();
				m_env.pPhysicalWorld->TracePendingRays(2);
				m_env.pPhysicalWorld->TimeStep(0);
			}
			if ((nPauseMode == 0) && !(updateFlags & ESYSUPDATE_IGNORE_AI) && g_cvars.sys_ai && !bNoUpdate)
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "SysUpdate:AI");
				//////////////////////////////////////////////////////////////////////
				//update AI system
				if (m_env.pAISystem && !m_cvAIUpdate->GetIVal())
					m_env.pAISystem->Update(gEnv->pTimer->GetFrameStartTime(), gEnv->pTimer->GetFrameTime());
			}
		}
		pe_params_waterman pwm;
		pwm.posViewer = GetViewCamera().GetPosition();
		m_env.pPhysicalWorld->SetWaterManagerParams(&pwm);
	}

	// Use UI timer for CryMovie, because it should not be affected by pausing game time
	const float fMovieFrameTime = m_Time.GetFrameTime(ITimer::ETIMER_UI);

	// Run movie system pre-update
	if (!bNoUpdate)
	{
		UpdateMovieSystem(updateFlags.UnderlyingValue(), fMovieFrameTime, true);
	}

#ifndef EXCLUDE_UPDATE_ON_CONSOLE
	if (nPauseMode != 1)
#endif //EXCLUDE_UPDATE_ON_CONSOLE
	{
		//////////////////////////////////////////////////////////////////////
		//update entity system
		if (m_env.pEntitySystem && !bNoUpdate && g_cvars.sys_entitysystem)
		{
			m_env.pEntitySystem->Update();
		}
	}

	// Run movie system post-update
	if (!bNoUpdate)
	{
		UpdateMovieSystem(updateFlags.UnderlyingValue(), fMovieFrameTime, false);
	}

	//////////////////////////////////////////////////////////////////////
	//update process (3D engine)
	if (!(updateFlags & ESYSUPDATE_EDITOR) && !bNoUpdate)
	{
		if (ITimeOfDay* pTOD = m_env.p3DEngine->GetTimeOfDay())
			pTOD->Tick();

		if (m_env.p3DEngine)
			m_env.p3DEngine->Tick();  // clear per frame temp data

		if (m_pProcess && (m_pProcess->GetFlags() & PROC_3DENGINE))
		{
			if ((nPauseMode != 1))
				if (!IsEquivalent(m_ViewCamera.GetPosition(), Vec3(0, 0, 0), VEC_EPSILON))
				{
					if (m_env.p3DEngine)
					{
						//					m_env.p3DEngine->SetCamera(m_ViewCamera);
						m_pProcess->Update();
#ifndef EXCLUDE_UPDATE_ON_CONSOLE
						//////////////////////////////////////////////////////////////////////////
						// Strange, !do not remove... ask Timur for the meaning of this.
						//////////////////////////////////////////////////////////////////////////
						if (m_nStrangeRatio > 32767)
						{
							gEnv->pScriptSystem->SetGCFrequency(-1); // lets get nasty.
						}
						//////////////////////////////////////////////////////////////////////////
						// Strange, !do not remove... ask Timur for the meaning of this.
						//////////////////////////////////////////////////////////////////////////
						if (m_nStrangeRatio > 1000)
						{
							if (m_pProcess && (m_pProcess->GetFlags() & PROC_3DENGINE))
								m_nStrangeRatio += cry_random(1, 11);
						}
#endif      //EXCLUDE_UPDATE_ON_CONSOLE
						//////////////////////////////////////////////////////////////////////////
					}
				}
		}
		else
		{
			if (m_pProcess)
				m_pProcess->Update();
		}
	}

	//////////////////////////////////////////////////////////////////////
	//update sound system part 2
	if (!g_cvars.sys_deferAudioUpdateOptim && !bNoUpdate)
	{
		UpdateAudioSystems();
	}
	else
	{
		m_bNeedDoWorkDuringOcclusionChecks = true;
	}

	//////////////////////////////////////////////////////////////////////
	// final network update
	if (m_env.pNetwork)
	{
		m_env.pNetwork->SyncWithGame(eNGS_FrameEnd);
		m_env.pNetwork->SyncWithGame(eNGS_DisplayDebugInfo);
		m_env.pNetwork->SyncWithGame(eNGS_WakeNetwork);   // This will wake the network thread up
	}

#ifdef DOWNLOAD_MANAGER
	if (m_pDownloadManager && !bNoUpdate)
	{
		m_pDownloadManager->Update();
	}
#endif //DOWNLOAD_MANAGER
#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_ORBIS
	if (m_sys_SimulateTask->GetIVal() > 0)
	{
		// have a chance to win longest Pi calculation content
		int64 delay = m_sys_SimulateTask->GetIVal();
		int64 start = CryGetTicks();
		double a = 1.0, b = 1.0 / sqrt(2.0), t = 1.0 / 4.0, p = 1.0, an, bn, tn, pn, Pi = 0.0;
		while (CryGetTicks() - start < delay)
		{
			// do something
			an = (a + b) / 2.0;
			bn = sqrt(a * b);
			tn = t - p * (a - an) * (a - an);
			pn = 2 * p;

			a = an;
			b = bn;
			t = tn;
			p = pn;

			Pi = (a + b) * (a + b) / 4 / t;
		}
		//CryLog("Task calculate PI = %f ", Pi); // Thats funny , but it works :-)
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	//update stats agent
#ifdef ENABLE_STATS_AGENT
	CStatsAgent::Update();
#endif // #ifdef ENABLE_STATS_AGENT

	m_pSystemEventDispatcher->Update();

	if (gEnv->pSchematyc != nullptr)
	{
		gEnv->pSchematyc->Update();
	}

	if (gEnv->pSchematyc2 != nullptr)
	{
		gEnv->pSchematyc2->Update();
	}

	if (m_env.pHardwareMouse != nullptr)
	{
		m_env.pHardwareMouse->Update();
	}

	//Now update frame statistics
	CTimeValue cur_time = gEnv->pTimer->GetAsyncTime();

	CTimeValue a_second(g_cvars.sys_update_profile_time);
	std::vector<std::pair<CTimeValue, float>>::iterator it = m_updateTimes.begin();
	for (std::vector<std::pair<CTimeValue, float>>::iterator eit = m_updateTimes.end(); it != eit; ++it)
		if ((cur_time - it->first) < a_second)
			break;

	if (it != m_updateTimes.begin())
		m_updateTimes.erase(m_updateTimes.begin(), it);

	float updateTime = (cur_time - updateStart).GetMilliSeconds();
	m_updateTimes.push_back(std::make_pair(cur_time, updateTime));

	UpdateUpdateTimes();

	return !m_bQuit;

}

IManualFrameStepController* CSystem::GetManualFrameStepController() const
{
	return m_pManualFrameStepController;
}

bool CSystem::UpdateLoadtime()
{
	m_pPlatformOS->Tick(m_Time.GetRealFrameTime());

	/*
	   // uncomment this code if input processing is required
	   // during level loading
	   if (m_env.pInput)
	   {
	    //////////////////////////////////////////////////////////////////////
	    //update input system
	   #if !CRY_PLATFORM_WINDOWS
	    m_env.pInput->Update(true);
	   #else
	    bool bFocus = (GetFocus()==m_hWnd) || m_bEditor;
	    {
	      WriteLock lock(g_lockInput);
	      m_env.pInput->Update(bFocus);
	      g_BreakListenerTask.m_nBreakIdle = 0;
	    }
	   #endif
	   }
	 */

	return !m_bQuit;
}

void CSystem::DoWorkDuringOcclusionChecks()
{
	if (g_cvars.sys_deferAudioUpdateOptim && !m_bNoUpdate)
	{
		UpdateAudioSystems();
		m_bNeedDoWorkDuringOcclusionChecks = false;
	}
}

void CSystem::UpdateAudioSystems()
{
	if (m_env.pAudioSystem != nullptr && !IsLoading()) //do not update pAudioSystem during async level load
	{
		CRY_PROFILE_SECTION(PROFILE_SYSTEM, "UpdateAudioSystems");
		CRYPROFILE_SCOPE_PROFILE_MARKER("UpdateAudioSystems");

		m_env.pAudioSystem->ExternalUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetUpdateStats(SSystemUpdateStats& stats)
{
	if (m_updateTimes.empty())
	{
		stats = SSystemUpdateStats();
	}
	else
	{
		stats.avgUpdateTime = 0;
		stats.maxUpdateTime = -FLT_MAX;
		stats.minUpdateTime = +FLT_MAX;
		for (std::vector<std::pair<CTimeValue, float>>::const_iterator it = m_updateTimes.begin(), eit = m_updateTimes.end(); it != eit; ++it)
		{
			const float t = it->second;
			stats.avgUpdateTime += t;
			stats.maxUpdateTime = max(stats.maxUpdateTime, t);
			stats.minUpdateTime = min(stats.minUpdateTime, t);
		}
		stats.avgUpdateTime /= m_updateTimes.size();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UpdateMovieSystem(const int updateFlags, const float fFrameTime, const bool bPreUpdate)
{
	if (m_env.pMovieSystem && !(updateFlags & ESYSUPDATE_EDITOR) && g_cvars.sys_trackview)
	{
		float fMovieFrameTime = fFrameTime;

		if (fMovieFrameTime > g_cvars.sys_maxTimeStepForMovieSystem)
			fMovieFrameTime = g_cvars.sys_maxTimeStepForMovieSystem;

		if (bPreUpdate)
		{
			m_env.pMovieSystem->PreUpdate(fMovieFrameTime);
		}
		else
		{
			m_env.pMovieSystem->PostUpdate(fMovieFrameTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// XML stuff
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSystem::CreateXmlNode(const char* sNodeName, bool bReuseStrings)
{
	return new CXmlNode(sNodeName, bReuseStrings);
}

//////////////////////////////////////////////////////////////////////////
IXmlUtils* CSystem::GetXmlUtils()
{
	return m_pXMLUtils;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetViewCamera(CCamera& Camera)
{
	m_ViewCamera = Camera;
	m_ViewCamera.CalculateRenderMatrices();
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSystem::LoadXmlFromFile(const char* sFilename, bool bReuseStrings)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(sFilename);

	return m_pXMLUtils->LoadXmlFromFile(sFilename, bReuseStrings);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSystem::LoadXmlFromBuffer(const char* buffer, size_t size, bool bReuseStrings)
{
	LOADING_TIME_PROFILE_SECTION
	return m_pXMLUtils->LoadXmlFromBuffer(buffer, size, bReuseStrings);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::CheckLogVerbosity(int verbosity)
{
	if (verbosity <= m_env.pLog->GetVerbosityLevel())
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Warning(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WarningV(module, severity, flags, file, format, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::WarningOnce(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, ...)
{
	char szBuffer[MAX_WARNING_LENGTH];
	va_list args;
	va_start(args, format);
	cry_vsprintf(szBuffer, format, args);
	va_end(args);

	CryAutoLock<CryMutex> lock(m_mapWarningOnceMutex);
	uint32 crc = CCrc32::ComputeLowercase(szBuffer);
	if (m_mapWarningOnceAlreadyPrinted.find(crc) == m_mapWarningOnceAlreadyPrinted.end())
	{
		m_mapWarningOnceAlreadyPrinted[crc] = true;

		Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, 0, szBuffer);
	}
}

inline const char* ValidatorModuleToString(EValidatorModule module)
{
	switch (module)
	{
	case VALIDATOR_MODULE_RENDERER:
		return "Renderer";
	case VALIDATOR_MODULE_3DENGINE:
		return "3DEngine";
	case VALIDATOR_MODULE_ASSETS:
		return "Assets";
	case VALIDATOR_MODULE_AI:
		return "AI";
	case VALIDATOR_MODULE_ANIMATION:
		return "Animation";
	case VALIDATOR_MODULE_ENTITYSYSTEM:
		return "EntitySystem";
	case VALIDATOR_MODULE_SCRIPTSYSTEM:
		return "Script";
	case VALIDATOR_MODULE_SYSTEM:
		return "System";
	case VALIDATOR_MODULE_AUDIO:
		return "Audio";
	case VALIDATOR_MODULE_GAME:
		return "Game";
	case VALIDATOR_MODULE_MOVIE:
		return "Movie";
	case VALIDATOR_MODULE_EDITOR:
		return "Editor";
	case VALIDATOR_MODULE_NETWORK:
		return "Network";
	case VALIDATOR_MODULE_PHYSICS:
		return "Physics";
	case VALIDATOR_MODULE_FLOWGRAPH:
		return "FlowGraph";
	case VALIDATOR_MODULE_ONLINE:
		return "Online";
	case VALIDATOR_MODULE_DRS:
		return "DynamicResponseSystem";
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
void CSystem::WarningV(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, va_list args)
{
	// Fran: No logging in a testing environment
	if (m_env.pLog == 0)
	{
		return;
	}

	const char* sModuleFilter = m_env.pLog->GetModuleFilter();
	if (sModuleFilter && *sModuleFilter != 0)
	{
		const char* sModule = ValidatorModuleToString(module);
		if (strlen(sModule) > 1 || CryStringUtils::stristr(sModule, sModuleFilter) == 0)
		{
			// Filter out warnings from other modules.
			return;
		}
	}

	bool bDbgBreak = false;
	if (severity == VALIDATOR_ERROR_DBGBRK)
	{
		bDbgBreak = true;
		severity = VALIDATOR_ERROR; // change it to a standard VALIDATOR_ERROR for simplicity in the rest of the system
	}

	IMiniLog::ELogType ltype = ILog::eComment;
	switch (severity)
	{
	case VALIDATOR_ERROR:
		ltype = ILog::eError;
		break;
	case VALIDATOR_WARNING:
		ltype = ILog::eWarning;
		break;
	case VALIDATOR_COMMENT:
		ltype = ILog::eComment;
		break;
	case VALIDATOR_ASSERT:
		ltype = ILog::eAssert;
		break;
	default:
		break;
	}
	char szBuffer[MAX_WARNING_LENGTH];
	cry_vsprintf(szBuffer, format, args);

	if (file && *file)
	{
		CryFixedStringT<MAX_WARNING_LENGTH> fmt = szBuffer;
		fmt += " [File=";
		fmt += file;
		fmt += "]";

		m_env.pLog->LogWithType(ltype, flags | VALIDATOR_FLAG_SKIP_VALIDATOR, "%s", fmt.c_str());
	}
	else
	{
		m_env.pLog->LogWithType(ltype, flags | VALIDATOR_FLAG_SKIP_VALIDATOR, "%s", szBuffer);
	}

	//if(file)
	//m_env.pLog->LogWithType( ltype, "  ... caused by file '%s'",file);

	if (m_pValidator && (flags & VALIDATOR_FLAG_SKIP_VALIDATOR) == 0)
	{
		SValidatorRecord record;
		record.file = file;
		record.text = szBuffer;
		record.module = module;
		record.severity = severity;
		record.flags = flags;
		record.assetScope = m_env.pLog->GetAssetScopeString();
		m_pValidator->Report(record);
	}

#if !defined(_RELEASE)
	if (bDbgBreak && g_cvars.sys_error_debugbreak)
		__debugbreak();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Deltree(const char* szFolder, bool bRecurse)
{
	__finddata64_t fd;
	string filespec = szFolder;
	filespec += "*.*";

	intptr_t hfil = 0;
	if ((hfil = _findfirst64(filespec.c_str(), &fd)) == -1)
	{
		return;
	}

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			string name = fd.name;

			if ((name != ".") && (name != ".."))
			{
				if (bRecurse)
				{
					name = szFolder;
					name += fd.name;
					name += "/";

					Deltree(name.c_str(), bRecurse);
				}
			}
		}
		else
		{
			string name = szFolder;

			name += fd.name;

			DeleteFile(name.c_str());
		}

	}
	while (!_findnext64(hfil, &fd));

	_findclose(hfil);

	RemoveDirectory(szFolder);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetLocalizedPath(char const* const szLanguage, string& szLocalizedPath)
{
	szLocalizedPath = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + szLanguage + "_xml.pak";
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetLocalizedAudioPath(char const* const szLanguage, string& szLocalizedPath)
{
	szLocalizedPath = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + szLanguage + ".pak";
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CloseLanguagePak(char const* const szLanguage)
{
	string szLocalizedPath;
	GetLocalizedPath(szLanguage, szLocalizedPath);
	m_env.pCryPak->ClosePacks(szLocalizedPath);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CloseLanguageAudioPak(char const* const szLanguage)
{
	string szLocalizedPath;
	GetLocalizedAudioPath(szLanguage, szLocalizedPath);
	m_env.pCryPak->ClosePacks(szLocalizedPath);
}

#if CRY_PLATFORM_DURANGO
//////////////////////////////////////////////////////////////////////////
void CSystem::OnPLMEvent(EPLM_Event event)
{

	switch (event)
	{
	case EPLMEV_ON_RESUMING:
		{
			CryLogAlways("CSystem::OnPLMEvent --- OnResuming");
			if (m_pSystemEventDispatcher)
				m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_PLM_ON_RESUMING, 0, 0);
			if (gEnv->pRenderer)
				gEnv->pRenderer->ResumeDevice();
			gEnv->ePLM_State = EPLM_RUNNING;
			break;
		}

	case EPLMEV_ON_SUSPENDING:
		{
			CryLogAlways("CSystem::OnPLMEvent --- OnSuspending");
			if (m_pSystemEventDispatcher)
				m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_PLM_ON_SUSPENDING, 0, 0);
			if (gEnv->pRenderer)
				gEnv->pRenderer->SuspendDevice();
			gEnv->ePLM_State = EPLM_SUSPENDED;
			break;
		}

	case EPLMEV_ON_CONSTRAINED:
		{
			CryLogAlways("CSystem::OnPLMEvent --- OnConstrained");
			if (m_pSystemEventDispatcher)
				m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_PLM_ON_CONSTRAINED, 0, 0);
			gEnv->ePLM_State = EPLM_CONSTRAINED;
			break;
		}

	case EPLMEV_ON_FULL:
		{
			CryLogAlways("CSystem::OnPLMEvent --- OnFull");
			if (m_pSystemEventDispatcher)
				m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_PLM_ON_FULL, 0, 0);
			gEnv->ePLM_State = EPLM_RUNNING;
			break;
		}

	default:
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Unhandled PLM Event!");
			break;
		}
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
void CSystem::UnloadSchematycModule()
{
	UnloadEngineModule("CrySchematyc");
	UnloadEngineModule("CrySchematyc2");

	gEnv->pSchematyc2 = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Strange()
{
	m_nStrangeRatio += cry_random(1, 101);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Relaunch(bool bRelaunch)
{
	if (m_sys_firstlaunch)
		m_sys_firstlaunch->Set("0");

	m_bRelaunch = bRelaunch;
	SaveConfiguration();
}

//////////////////////////////////////////////////////////////////////////
ICrySizer* CSystem::CreateSizer()
{
	return new CrySizerImpl;
}

//////////////////////////////////////////////////////////////////////////
uint32 CSystem::GetUsedMemory()
{
	return CryMemoryGetAllocatedSize();
}

//////////////////////////////////////////////////////////////////////////
ILocalizationManager* CSystem::GetLocalizationManager()
{
	return m_pLocalizationManager;
}

//////////////////////////////////////////////////////////////////////////
IResourceManager* CSystem::GetIResourceManager()
{
	return m_pResourceManager;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::debug_GetCallStackRaw(void** callstack, uint32& callstackLength)
{
	uint32 callstackCapacity = callstackLength;
	uint32 nNumStackFramesToSkip = 1;

	memset(callstack, 0, sizeof(void*) * callstackLength);

#if !CRY_PLATFORM_ANDROID
	callstackLength = 0;
#endif

#if CRY_PLATFORM_ORBIS
	uint csLength = 0;
	void** stack = NULL;
	__asm__ __volatile__ ("mov %%rbp, %0" : "=r" (stack)); // WARNING: This could be brittle
	while (csLength < callstackCapacity && stack)
	{
		callstack[csLength] = stack[1];
		stack = (void**)stack[0];
		csLength++;
	}
	callstackLength = csLength;
#elif CRY_PLATFORM_WINAPI
	if (callstackCapacity > 0x40)
		callstackCapacity = 0x40;
	callstackLength = RtlCaptureStackBackTrace(nNumStackFramesToSkip, callstackCapacity, callstack, NULL);
#endif
	/*
	   static int aaa = 0;
	   aaa++;

	   if ((aaa & 0xF)  == 0)
	   {
	   CryLogAlways( "RtlCaptureStackBackTrace = (%d)",callstackLength );
	   for (int i=0; i<callstackLength; i++)
	   {
	   CryLogAlways( "   [%d] = (%X)",i,callstack[i] );
	   }
	   }

	   callstackLength = IDebugCallStack::instance()->CollectCallStackFrames( callstack,callstackCapacity );
	   if ((aaa & 0xF)  == 0)
	   {
	   CryLogAlways( "StackWalk64 = (%d)",callstackLength );
	   for (int i=0; i<callstackLength; i++)
	   {
	   CryLogAlways( "   [%d] = (%X)",i,callstack[i] );
	   }
	   }
	 */

	if (callstackLength > 0)
	{
		std::reverse(callstack, callstack + callstackLength);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ApplicationTest(const char* szParam)
{
	assert(szParam);
	if (m_pTestSystem)
		m_pTestSystem->ApplicationTest(szParam);
}

void CSystem::ExecuteCommandLine()
{
	LOADING_TIME_PROFILE_SECTION;
	// should only be called once
	{
		static bool bCalledAlready = false;
		assert(!bCalledAlready);
		bCalledAlready = true;
	}

	// auto detect system spec (overrides profile settings)
	if (m_pCmdLine->FindArg(eCLAT_Pre, "autodetect"))
		AutoDetectSpec(false);

	// execute command line arguments e.g. +g_gametype ASSAULT +map "testy"

	ICmdLine* pCmdLine = GetICmdLine();
	assert(pCmdLine);

	const int iCnt = pCmdLine->GetArgCount();

	for (int i = 0; i < iCnt; ++i)
	{
		const ICmdLineArg* pCmd = pCmdLine->GetArg(i);

		if (pCmd->GetType() == eCLAT_Post)
		{
			string sLine = pCmd->GetName();

#if defined(CVARS_WHITELIST)
			if (!GetCVarsWhiteList() || GetCVarsWhiteList()->IsWhiteListed(sLine, false))
#endif
			{
				if (pCmd->GetValue())
					sLine += string(" ") + pCmd->GetValue();

				GetILog()->Log("Executing command from command line: \n%s\n", sLine.c_str()); // - the actual command might be executed much later (e.g. level load pause)
				GetIConsole()->ExecuteString(sLine.c_str(), false, !m_bShaderCacheGenMode);
			}
#if defined(DEDICATED_SERVER)
	#if defined(CVARS_WHITELIST)
			else
			{
				GetILog()->LogError("Failed to execute command: '%s' as it is not whitelisted\n", sLine.c_str());
			}
	#endif
#endif
		}
	}

	//gEnv->pConsole->ExecuteString("sys_RestoreSpec test*"); // to get useful debugging information about current spec settings to the log file
}

void CSystem::DumpMemoryCoverage()
{
	m_MemoryFragmentationProfiler.DumpMemoryCoverage();
}

ITextModeConsole* CSystem::GetITextModeConsole()
{
	if (m_env.IsDedicated())
		return m_pTextModeConsole;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
ESystemConfigSpec CSystem::GetConfigSpec(bool bClient)
{
	if (bClient)
	{
		if (m_sys_spec)
			return (ESystemConfigSpec)m_sys_spec->GetIVal();
		return CONFIG_VERYHIGH_SPEC; // highest spec.
	}
	else
		return m_nServerConfigSpec;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetConfigSpec(ESystemConfigSpec spec, bool bClient)
{
	if (bClient)
	{
		if (m_sys_spec)
			m_sys_spec->Set((int)spec);
	}
	else
	{
		m_nServerConfigSpec = spec;
	}
}

//////////////////////////////////////////////////////////////////////////
ESystemConfigSpec CSystem::GetMaxConfigSpec() const
{
	return m_nMaxConfigSpec;
}

//////////////////////////////////////////////////////////////////////////
IProjectManager* CSystem::GetIProjectManager()
{
	return m_pProjectManager;
}

//////////////////////////////////////////////////////////////////////////
CPNoise3* CSystem::GetNoiseGen()
{
	static CPNoise3 m_pNoiseGen;
	return &m_pNoiseGen;
}

//////////////////////////////////////////////////////////////////////////
void CProfilingSystem::VTuneResume()
{
	CryLogAlways("Profiler Resume");
	CryProfile::ProfilerResume();
}

//////////////////////////////////////////////////////////////////////////
void CProfilingSystem::VTunePause()
{
	CryProfile::ProfilerPause();
	CryLogAlways("Profiler Pause");
}

//////////////////////////////////////////////////////////////////////////
sUpdateTimes& CSystem::GetCurrentUpdateTimeStats()
{
	return m_UpdateTimes[m_UpdateTimesIdx];
}

//////////////////////////////////////////////////////////////////////////
const sUpdateTimes* CSystem::GetUpdateTimeStats(uint32& index, uint32& num)
{
	index = m_UpdateTimesIdx;
	num = NUM_UPDATE_TIMES;
	return m_UpdateTimes;
}

void CSystem::FillRandomMT(uint32* pOutWords, uint32 numWords)
{
	AUTO_LOCK(m_mtLock);
	if (!m_pMtState)
	{
		struct TicksTime
		{
			int64  ticks;
			time_t tm;
		};

		TicksTime tt = { CryGetTicks(), time(nullptr) };
		m_pMtState = new CMTRand_int32(reinterpret_cast<uint32*>(&tt), sizeof(tt) / sizeof(uint32));
	}

	for (uint32 i = 0; i < numWords; ++i)
		pOutWords[i] = m_pMtState->GenerateUint32();
}

void CSystem::UpdateUpdateTimes()
{
	sUpdateTimes& sample = m_UpdateTimes[m_UpdateTimesIdx];
	if (m_PhysThread)
	{
		static uint64 lastPhysTime = 0U;
		static uint64 lastMainTime = 0U;
		static uint64 lastYields = 0U;
		static uint64 lastPhysWait = 0U;
		uint64 physTime = 0, mainTime = 0;
		uint32 yields = 0;
		physTime = ((CPhysicsThreadTask*)m_PhysThread)->LastStepTaken();
		mainTime = CryGetTicks() - lastMainTime;
		lastMainTime = mainTime;
		lastPhysWait = ((CPhysicsThreadTask*)m_PhysThread)->LastWaitTime();
		sample.PhysStepTime = physTime;
		sample.SysUpdateTime = mainTime;
		sample.PhysYields = yields;
		sample.physWaitTime = lastPhysWait;
	}
	++m_UpdateTimesIdx;
	if (m_UpdateTimesIdx >= NUM_UPDATE_TIMES) m_UpdateTimesIdx = 0;
}

IPhysicsDebugRenderer* CSystem::GetIPhysicsDebugRenderer()
{
	return m_pPhysRenderer;
}

IPhysRenderer* CSystem::GetIPhysRenderer()
{
	return m_pPhysRenderer;
}

#ifndef _RELEASE
void CSystem::GetCheckpointData(ICheckpointData& data)
{
	data.m_totalLoads = m_checkpointLoadCount;
	data.m_loadOrigin = m_loadOrigin;
}

void CSystem::IncreaseCheckpointLoadCount()
{
	if (!m_hasJustResumed)
		++m_checkpointLoadCount;

	m_hasJustResumed = false;
}

void CSystem::SetLoadOrigin(LevelLoadOrigin origin)
{
	switch (origin)
	{
	case eLLO_NewLevel: // Intentional fall through
	case eLLO_Level2Level:
		m_expectingMapCommand = true;
		break;

	case eLLO_Resumed:
		m_hasJustResumed = true;
		break;

	case eLLO_MapCmd:
		if (m_expectingMapCommand)
		{
			// We knew a map command was coming, so don't process this.
			m_expectingMapCommand = false;
			return;
		}
		break;
	}

	m_loadOrigin = origin;
	m_checkpointLoadCount = 0;
}
#endif

//////////////////////////////////////////////////////////////////////
void CSystem::OnLanguageCVarChanged(ICVar* const pLanguage)
{
	if (pLanguage != nullptr && pLanguage->GetType() == CVAR_STRING)
	{
		CSystem* const pSystem = static_cast<CSystem*>(gEnv->pSystem);

		if (pSystem != nullptr)
		{
			CLocalizedStringsManager* const pLocalizationManager = static_cast<CLocalizedStringsManager* const>(pSystem->GetLocalizationManager());

			if (pLocalizationManager != nullptr)
			{
				char const* const szCurrentLanguage = pLocalizationManager->GetLanguage();

				if (szCurrentLanguage != nullptr && szCurrentLanguage[0] != '\0')
				{
					pSystem->CloseLanguagePak(szCurrentLanguage);
				}

				CLocalizedStringsManager::TLocalizationTagVec tags;
				pLocalizationManager->GetLoadedTags(tags);
				pLocalizationManager->FreeLocalizationData();

				char const* const szNewLanguage = pLanguage->GetString();
				pSystem->OpenLanguagePak(szNewLanguage);
				pLocalizationManager->SetLanguage(szNewLanguage);

				for (auto& tag : tags)
				{
					pLocalizationManager->LoadLocalizationDataByTag(tag.c_str());
				}

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
				if (gEnv->pScaleformHelper)
				{
					gEnv->pScaleformHelper->SetTranslatorDirty(true);
				}
#endif
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
void CSystem::OnLanguageAudioCVarChanged(ICVar* const pLanguageAudio)
{
	if (pLanguageAudio != nullptr && pLanguageAudio->GetType() == CVAR_STRING)
	{
		CSystem* const pSystem = static_cast<CSystem*>(gEnv->pSystem);

		if (pSystem != nullptr)
		{
			char const* const szNewLanguage = pLanguageAudio->GetString();

			if (!pSystem->m_currentLanguageAudio.empty())
			{
				pSystem->CloseLanguageAudioPak(pSystem->m_currentLanguageAudio.c_str());
			}

			pSystem->OpenLanguageAudioPak(szNewLanguage);
			pSystem->m_currentLanguageAudio = szNewLanguage;

			if (gEnv->pAudioSystem != nullptr)
			{
				gEnv->pAudioSystem->OnLanguageChanged();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnLocalizationFolderCVarChanged(ICVar* const pLocalizationFolder)
{
	if (pLocalizationFolder && pLocalizationFolder->GetType() == CVAR_STRING)
	{
		CSystem* const pSystem = static_cast<CSystem* const>(gEnv->pSystem);

		if (pSystem != NULL && gEnv->pCryPak != NULL)
		{
			CLocalizedStringsManager* const pLocalizationManager = static_cast<CLocalizedStringsManager* const>(pSystem->GetLocalizationManager());

			if (pLocalizationManager)
			{
				// Get what is currently loaded
				CLocalizedStringsManager::TLocalizationTagVec tags;
				pLocalizationManager->GetLoadedTags(tags);

				// Release the old localization data.
				for (auto& tag : tags)
				{
					pLocalizationManager->ReleaseLocalizationDataByTag(tag.c_str());
				}

				// Close the paks situated in the previous localization folder.
				pSystem->CloseLanguagePak(pLocalizationManager->GetLanguage());
				pSystem->CloseLanguageAudioPak(pSystem->m_currentLanguageAudio.c_str());

				// Set the new localization folder.
				gEnv->pCryPak->SetLocalizationFolder(pLocalizationFolder->GetString());

				// Now open the paks situated in the new localization folder.
				pSystem->OpenLanguagePak(pLocalizationManager->GetLanguage());
				pSystem->OpenLanguageAudioPak(pSystem->m_currentLanguageAudio.c_str());

				// And load the new data.
				for (auto& tag : tags)
				{
					pLocalizationManager->LoadLocalizationDataByTag(tag.c_str());
				}
			}
		}
	}
}

void CSystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END);
		}
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			gEnv->pCryPak->DisableRuntimeFileAccess(false);
		}
		break;
	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		{
			if (!gEnv->IsEditing())
			{
				gEnv->pCryPak->DisableRuntimeFileAccess(true);
			}
		}
		break;
	}
}

ESystemGlobalState CSystem::GetSystemGlobalState(void)
{
	return m_systemGlobalState;
}

const char* CSystem::GetSystemGlobalStateName(const ESystemGlobalState systemGlobalState)
{
	static const char* const s_systemGlobalStateNames[] = {
		"INIT",                    // ESYSTEM_GLOBAL_STATE_INIT,
		"LEVEL_LOAD_PREPARE",      // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_PREPARE,
		"LEVEL_LOAD_START",        // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START,
		"LEVEL_LOAD_MATERIALS",    // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_MATERIALS,
		"LEVEL_LOAD_OBJECTS",      // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_OBJECTS,
		"LEVEL_LOAD_CHARACTERS",   // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_CHARACTERS,
		"LEVEL_LOAD_STATIC_WORLD", // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_STATIC_WORLD,
		"LEVEL_LOAD_ENTITIES",     // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_ENTITIES,
		"LEVEL_LOAD_PRECACHE",     // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_PRECACHE,
		"LEVEL_LOAD_TEXTURES",     // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_TEXTURES,
		"LEVEL_LOAD_END",          // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END,
		"LEVEL_LOAD_ENDING",       // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_ENDING,
		"LEVEL_LOAD_COMPLETE",     // ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_COMPLETE
		"RUNNING",                 // ESYSTEM_GLOBAL_STATE_RUNNING,
	};
	const size_t numElements = CRY_ARRAY_COUNT(s_systemGlobalStateNames);
	const size_t index = (size_t)systemGlobalState;
	if (index >= numElements)
	{
		return "INVALID INDEX";
	}
	return s_systemGlobalStateNames[index];
}

void CSystem::SetSystemGlobalState(const ESystemGlobalState systemGlobalState)
{
	static CTimeValue s_startTime = CTimeValue();
	if (systemGlobalState != m_systemGlobalState)
	{
		if (gEnv && gEnv->pTimer)
		{
			const CTimeValue endTime = gEnv->pTimer->GetAsyncTime();
			const float numSeconds = endTime.GetDifferenceInSeconds(s_startTime);
			CryLog("SetGlobalState %d->%d '%s'->'%s' %3.1f seconds",
			       m_systemGlobalState, systemGlobalState,
			       CSystem::GetSystemGlobalStateName(m_systemGlobalState), CSystem::GetSystemGlobalStateName(systemGlobalState),
			       numSeconds);
			s_startTime = gEnv->pTimer->GetAsyncTime();
		}
	}
	m_systemGlobalState = systemGlobalState;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RegisterWindowMessageHandler(IWindowMessageHandler* pHandler)
{
	assert(pHandler && !stl::find(m_windowMessageHandlers, pHandler) && "This IWindowMessageHandler is already registered");
	m_windowMessageHandlers.push_back(pHandler);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UnregisterWindowMessageHandler(IWindowMessageHandler* pHandler)
{
	bool bRemoved = stl::find_and_erase(m_windowMessageHandlers, pHandler);
	assert(pHandler && bRemoved && "This IWindowMessageHandler was not registered");
}

//////////////////////////////////////////////////////////////////////////
int CSystem::PumpWindowMessage(bool bAll, WIN_HWND opaqueHWnd)
{
#if CRY_PLATFORM_WINDOWS
	int count = 0;
	const HWND hWnd = (HWND)opaqueHWnd;
	const bool bUnicode = hWnd != NULL ?
	                      IsWindowUnicode(hWnd) != FALSE :
	                      !(gEnv && gEnv->IsEditor());
	#if defined(UNICODE) || defined(_UNICODE)
	// Once we compile as Unicode app on Windows, we should detect non-Unicode windows
	assert(bUnicode && "The window is not Unicode, this is most likely a bug");
	#endif

	// Pick the correct function for handling messages
	typedef BOOL (WINAPI *    PeekMessageFunc)(MSG*, HWND, UINT, UINT, UINT);
	typedef LRESULT (WINAPI * DispatchMessageFunc)(const MSG*);
	const PeekMessageFunc pfnPeekMessage = bUnicode ? PeekMessageW : PeekMessageA;
	const DispatchMessageFunc pfnDispatchMessage = bUnicode ? DispatchMessageW : DispatchMessageA;

	do
	{
		// Get a new message
		MSG msg;
		BOOL bHasMessage = pfnPeekMessage(&msg, hWnd, 0, 0, PM_REMOVE);
		if (bHasMessage == FALSE) break;
		++count;

		// Special case for WM_QUIT
		if (msg.message == WM_QUIT)
		{
			return -1;
		}

		// Pre-process the message for IME
		if (msg.hwnd == m_hWnd)
		{
			for (std::vector<IWindowMessageHandler*>::const_iterator it = m_windowMessageHandlers.begin(); it != m_windowMessageHandlers.end(); ++it)
			{
				IWindowMessageHandler* pHandler = *it;
				pHandler->PreprocessMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
			}
		}

		// Dispatch the message
		TranslateMessage(&msg);
		pfnDispatchMessage(&msg);
	}
	while (bAll);

	return count;
#else
	// No window message support on this platform
	return 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::IsImeSupported() const
{
	assert(m_pImeManager != NULL);
	return m_pImeManager->IsImeSupported();
}

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS

enum class EMouseWheelOrigin
{
	ScreenSpace,
	WindowSpace,
	WindowSpaceClamped
};

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

bool CSystem::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	static bool sbInSizingModalLoop;
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	EHARDWAREMOUSEEVENT event = (EHARDWAREMOUSEEVENT)-1;
	*pResult = 0;
	switch (uMsg)
	{
	// System event translation
	case WM_CLOSE:
		Quit();
		return false;
	case WM_MOVE:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_MOVE, x, y);
		return false;
	case WM_SIZE:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RESIZE, x, y);
		return false;
	case WM_WINDOWPOSCHANGED:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_POS_CHANGED, 1, 0);
		return false;
	case WM_STYLECHANGED:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_STYLE_CHANGED, 1, 0);
		return false;
	case WM_ACTIVATE:
		// Pass HIWORD(wParam) as well to indicate whether this window is minimized or not
		// HIWORD(wParam) != 0 is minimized, HIWORD(wParam) == 0 is not minimized
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_ACTIVATE, LOWORD(wParam) != WA_INACTIVE, HIWORD(wParam));
		return true;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		m_hasWindowFocus = uMsg == WM_SETFOCUS;
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, m_hasWindowFocus, 0);
		return false;
	case WM_INPUTLANGCHANGE:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LANGUAGE_CHANGE, wParam, lParam);
		return false;
	case WM_DISPLAYCHANGE:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_DISPLAY_CHANGED, wParam, lParam);
		return false;
	case WM_DEVICECHANGE:
		GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_DEVICE_CHANGED, wParam, lParam);
		return false;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_SCREENSAVE)
		{
			// Check if screen saver is allowed
			IConsole* const pConsole = gEnv->pConsole;
			const ICVar* const pVar = pConsole ? pConsole->GetCVar("sys_screensaver_allowed") : 0;
			return pVar && pVar->GetIVal() == 0;
		}
		return false;

	// Mouse activation
	case WM_MOUSEACTIVATE:
		*pResult = MA_ACTIVATEANDEAT;
		return true;

	// Hardware mouse counters
	case WM_ENTERSIZEMOVE:
		sbInSizingModalLoop = true;
	// Fall through intended
	case WM_ENTERMENULOOP:
		{
			IHardwareMouse* const pMouse = GetIHardwareMouse();
			if (pMouse)
			{
				pMouse->IncrementCounter();
			}
		}
		return true;
	case WM_CAPTURECHANGED:
	// If WM_CAPTURECHANGED is received after WM_ENTERSIZEMOVE (ie, moving/resizing begins).
	// but no matching WM_EXITSIZEMOVE is received (this can happen if the window is not actually moved).
	// we still need to decrement the hardware mouse counter that was incremented when WM_ENTERSIZEMOVE was seen.
	// So in this case, we effectively treat WM_CAPTURECHANGED as if it was the WM_EXITSIZEMOVE message.
	// This behavior has only been reproduced the window is deactivated during the modal loop (ie, breakpoint triggered and focus moves to VS).
	case WM_EXITSIZEMOVE:
		if (!sbInSizingModalLoop)
		{
			return false;
		}
		sbInSizingModalLoop = false;
	// Fall through intended
	case WM_EXITMENULOOP:
		{
			IHardwareMouse* const pMouse = GetIHardwareMouse();
			if (pMouse)
			{
				pMouse->DecrementCounter();
			}
		}
		return (uMsg != WM_CAPTURECHANGED);

	// Events that should be forwarded to the hardware mouse
	case WM_MOUSEMOVE:
		event = HARDWAREMOUSEEVENT_MOVE;
		break;
	case WM_LBUTTONDOWN:
		event = HARDWAREMOUSEEVENT_LBUTTONDOWN;
		break;
	case WM_LBUTTONUP:
		event = HARDWAREMOUSEEVENT_LBUTTONUP;
		break;
	case WM_LBUTTONDBLCLK:
		event = HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK;
		break;
	case WM_RBUTTONDOWN:
		event = HARDWAREMOUSEEVENT_RBUTTONDOWN;
		break;
	case WM_RBUTTONUP:
		event = HARDWAREMOUSEEVENT_RBUTTONUP;
		break;
	case WM_RBUTTONDBLCLK:
		event = HARDWAREMOUSEEVENT_RBUTTONDOUBLECLICK;
		break;
	case WM_MBUTTONDOWN:
		event = HARDWAREMOUSEEVENT_MBUTTONDOWN;
		break;
	case WM_MBUTTONUP:
		event = HARDWAREMOUSEEVENT_MBUTTONUP;
		break;
	case WM_MBUTTONDBLCLK:
		event = HARDWAREMOUSEEVENT_MBUTTONDOUBLECLICK;
		break;
	case WM_MOUSEWHEEL:
		{
			event = HARDWAREMOUSEEVENT_WHEEL;
			ICVar* cv = gEnv->pConsole->GetCVar("i_mouse_scroll_coordinate_origin");
			if (cv)
			{
				switch ((EMouseWheelOrigin)cv->GetIVal())
				{
				case EMouseWheelOrigin::ScreenSpace:
					// Windows default - do nothing
					break;
				case EMouseWheelOrigin::WindowSpace:
					{
						POINT p{ x, y };
						ScreenToClient(hWnd, &p);
						x = p.x;
						y = p.y;
						break;
					}
				case EMouseWheelOrigin::WindowSpaceClamped:
					{
						POINT p{ x, y };
						ScreenToClient(hWnd, &p);
						RECT r;
						GetClientRect(hWnd, &r);
						x = crymath::clamp<int>(p.x, 0, r.right - r.left);
						y = crymath::clamp<int>(p.y, 0, r.bottom - r.top);
						break;
					}
				default:
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "i_mouse_scroll_coordinate_origin out of range");
					break;
				}
			}
			break;
		}

	// Any other event doesn't interest us
	default:
		return false;
	}

	// This is an event that should be forwarded to the hardware mouse.
	// Note: This code has been moved from the GameDLL into here.
	// However, maybe it should be re-factored to be driven by CryInput mouse events (or implemented there).
	assert(event != -1 && "Logic problem in hardware mouse event handler");
	IHardwareMouse* pMouse = GetIHardwareMouse();
	if (pMouse)
	{
		int wheel = uMsg == WM_MOUSEWHEEL ? GET_WHEEL_DELTA_WPARAM(wParam) : 0;
		pMouse->Event(x, y, event, wheel);
	}
	return true;
}

#endif

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
static LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CSystem* pSystem = 0;
	if (gEnv)
	{
		pSystem = static_cast<CSystem*>(gEnv->pSystem);
	}
	if (pSystem && !pSystem->m_bQuit)
	{
		LRESULT result;
		bool bAny = false;
		for (std::vector<IWindowMessageHandler*>::const_iterator it = pSystem->m_windowMessageHandlers.begin(); it != pSystem->m_windowMessageHandlers.end(); ++it)
		{
			IWindowMessageHandler* pHandler = *it;
			LRESULT maybeResult = 0xDEADDEAD;
			if (pHandler->HandleMessage(hWnd, uMsg, wParam, lParam, &maybeResult))
			{
				assert(maybeResult != 0xDEADDEAD && "Message handler indicated a resulting value, but no value was written");
				if (bAny)
				{
					assert(result == maybeResult && "Two window message handlers tried to return different result values");
				}
				else
				{
					bAny = true;
					result = maybeResult;
				}
			}
		}
		if (bAny)
		{
			// One of the registered handlers returned something
			return result;
		}
	}

	// Handle with the default procedure
	#if defined(UNICODE) || defined(_UNICODE)
	assert(IsWindowUnicode(hWnd) && "Window should be Unicode when compiling with UNICODE");
	#else
	if (!IsWindowUnicode(hWnd))
	{
		return DefWindowProcA(hWnd, uMsg, wParam, lParam);
	}
	#endif
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
#endif

//////////////////////////////////////////////////////////////////////////
void* CSystem::GetRootWindowMessageHandler()
{
#if CRY_PLATFORM_WINDOWS
	return &WndProc;
#else
	assert(false && "This platform does not support window message handlers");
	return NULL;
#endif
}

#undef EXCLUDE_UPDATE_ON_CONSOLE
