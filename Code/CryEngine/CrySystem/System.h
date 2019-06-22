// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ISystem.h>
#include <CryCore/Platform/CryLibrary.h>

#include "PakVars.h"
#include "Timer.h"
#include <CrySystem/IWindowMessageHandler.h>
#include <CrySystem/CryVersion.h>
#include <CryMath/Cry_Camera.h>
#include <CryString/CryName.h>
#include <CryMath/LCGRandom.h>
#include <CryRenderer/IRenderer.h>
#include <CryThreading/CryThread.h>
#include <bitset>

class CCmdLine;
class CCpuFeatures;
class CCryPluginManager;
class CDownloadManager;
class CJSONUtils;
class CLocalizedStringsManager;
class CManualFrameStepController;
class CMTRand_int32;
class CNULLRenderAuxGeom;
class CResourceManager;
class CrySizerImpl;
class CrySizerStats;
class CServerThrottle;
class CStreamEngine;
class CThreadManager;
class CUserAnalyticsSystem;
class CXmlUtils;
class ICrySizer;
class IDiskProfiler;
class IPhysicsThreadTask;

struct IConsoleCmdArgs;
struct ICryFactoryRegistryImpl;
struct ICryPerfHUD;
struct ICVar;
struct IFFont;
struct IResourceCollector;
struct IZLibCompressor;
struct SThreadMetaData;

namespace minigui
{
struct IMiniGUI;
}

namespace LiveCreate
{
struct IManager;
struct IHost;
}

namespace Cry
{
class CProjectManager;
}

#if CRY_PLATFORM_ANDROID
	#define USE_ANDROIDCONSOLE
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC
	#define USE_UNIXCONSOLE
#elif CRY_PLATFORM_IOS
	#define USE_IOSCONSOLE
#elif CRY_PLATFORM_WINDOWS
	#define USE_WINDOWSCONSOLE
#endif

#if defined(USE_UNIXCONSOLE) || defined(USE_ANDROIDCONSOLE) || defined(USE_WINDOWSCONSOLE) || defined(USE_IOSCONSOLE)
	#define USE_DEDICATED_SERVER_CONSOLE
#endif

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_DURANGO && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	#define DOWNLOAD_MANAGER
#endif

#ifdef DOWNLOAD_MANAGER
	#include "DownloadManager.h"
#endif //DOWNLOAD_MANAGER

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <CryCore/Platform/CryLibrary.h>
#endif

#define NUM_UPDATE_TIMES (128U)

typedef void* WIN_HMODULE;

#if !defined(CRY_ASYNC_MEMCPY_DELEGATE_TO_CRYSYSTEM)
CRY_ASYNC_MEMCPY_API void cryAsyncMemcpy(void* dst, const void* src, size_t size, int nFlags, volatile int* sync);
#else
CRY_ASYNC_MEMCPY_API void _cryAsyncMemcpy(void* dst, const void* src, size_t size, int nFlags, volatile int* sync);
#endif

//forward declarations
class CScriptSink;
class CLUADbg;
struct SDefaultValidator;
class CPhysRenderer;
class CVisRegTest;
class CImeManager;

#define PHSYICS_OBJECT_ENTITY    0

#define MAX_STREAMING_POOL_INDEX 6
#define MAX_THREAD_POOL_INDEX    6

struct SSystemCVars
{
	int    sys_streaming_requests_grouping_time_period;
	int    sys_streaming_sleep;
	int    sys_streaming_memory_budget;
	int    sys_streaming_max_finalize_per_frame;
	float  sys_streaming_max_bandwidth;
	int    sys_streaming_debug;
	int    sys_streaming_resetstats;
	int    sys_streaming_debug_filter;
	float  sys_streaming_debug_filter_min_time;
	int    sys_streaming_use_optical_drive_thread;
	ICVar* sys_streaming_debug_filter_file_name;
	ICVar* sys_localization_folder = nullptr;
	ICVar* sys_localization_pak_suffix;
	ICVar* sys_build_folder;
	int    sys_streaming_in_blocks;

	int    sys_float_exceptions;
	int    sys_no_crash_dialog;
	int    sys_dump_aux_threads;
	int    sys_keyboard_break;
	int    sys_WER;
	int    sys_dump_type;
	int    sys_ai;
	int    sys_physics;
	int    sys_entitysystem;
	int    sys_trackview;
	int    sys_livecreate;
	float  sys_update_profile_time;
	int    sys_limit_phys_thread_count;
	int    sys_usePlatformSavingAPI;
#ifndef _RELEASE
	int    sys_usePlatformSavingAPIEncryption;
#endif
	int    sys_MaxFPS;
	float  sys_maxTimeStepForMovieSystem;
	int    sys_force_installtohdd_mode;

#ifdef USE_HTTP_WEBSOCKETS
	int sys_simple_http_base_port;
#endif

	int     sys_error_debugbreak;

	int     sys_enable_crash_handler;

	int     sys_intromoviesduringinit;
	ICVar*  sys_splashscreen;

	int     sys_filesystemCaseSensitivity;

	int     sys_reflection_natvis;

	PakVars pakVars;

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	int sys_display_threads;
#endif

#if CRY_PLATFORM_WINDOWS
	int sys_highrestimer;
#endif

	int sys_vr_support;
};
extern SSystemCVars g_cvars;

class CSystem;

struct SmallModuleInfo
{
	string              name;
	CryModuleMemoryInfo memInfo;
};

struct SCryEngineStatsModuleInfo
{
	string              name;
	CryModuleMemoryInfo memInfo;
	uint32              moduleStaticSize;
	uint32              usedInModule;
	uint32              SizeOfCode;
	uint32              SizeOfInitializedData;
	uint32              SizeOfUninitializedData;
};

struct SCryEngineStatsGlobalMemInfo
{
	int    totalUsedInModules;
	int    totalCodeAndStatic;
	int    countedMemoryModules;
	uint64 totalAllocatedInModules;
	int    totalNumAllocsInModules;
	std::vector<SCryEngineStatsModuleInfo> modules;
};

IThreadManager* CreateThreadManager();

/*
   ===========================================
   The System interface Class
   ===========================================
 */
class CXConsole;

class CSystem final : public ISystem, public ILoadConfigurationEntrySink, public ISystemEventListener, public IWindowMessageHandler
{
public:
	CSystem(const SSystemInitParams& startupParams);
	~CSystem();
	bool        IsUIFrameworkMode() override { return m_bUIFrameworkMode; }

	static void OnLanguageCVarChanged(ICVar* const pLanguage);
	static void OnLanguageAudioCVarChanged(ICVar* const pLanguageAudio);
	static void OnLocalizationFolderCVarChanged(ICVar* const pLocalizationFolder);

	// interface ILoadConfigurationEntrySink ----------------------------------

	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup) override;

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

	///////////////////////////////////////////////////////////////////////////
	//! @name ISystem implementation
	//@{
	virtual SSystemGlobalEnvironment* GetGlobalEnvironment() override { return &m_env; }

	const char*                       GetRootFolder() const override  { return m_root.c_str(); }

	virtual bool                      DoFrame(const SDisplayContextKey& displayContextKey = SDisplayContextKey{},
	                                          const SGraphicsPipelineKey& graphicsPipelineKey = SGraphicsPipelineKey::BaseGraphicsPipelineKey,
	                                          CEnumFlags<ESystemUpdateFlags> updateFlags = CEnumFlags<ESystemUpdateFlags>()) override;

	virtual IManualFrameStepController* GetManualFrameStepController() const override;

	virtual bool                        UpdateLoadtime() override;

	//! Begin rendering frame.
	virtual void RenderBegin(const SDisplayContextKey& displayContextKey, const SGraphicsPipelineKey& graphicsPipelineKey) override;
	//! Render subsystems.
	void         Render(const SGraphicsPipelineKey& graphicsPipelineKey);
	//! End rendering frame and swap back buffer.
	virtual void RenderEnd(bool bRenderStats = true) override;

	virtual bool Update(CEnumFlags<ESystemUpdateFlags> updateFlags = CEnumFlags<ESystemUpdateFlags>(), int nPauseMode = 0) override;

	virtual void RenderPhysicsHelpers() override;

	//! Update screen during loading.
	void UpdateLoadingScreen();

	//! Update screen and call some important tick functions during loading.
	void SynchronousLoadingTick(const char* pFunc, int line) override;

	//! Renders the statistics; this is called from RenderEnd, but if the
	//! Host application (Editor) doesn't employ the Render cycle in ISystem,
	//! it may call this method to render the essential statistics
	void         RenderStatistics() override;
	void         RenderPhysicsStatistics(IPhysicalWorld* pWorld) override;

	uint32       GetUsedMemory() override;

	virtual void DumpMemoryUsageStatistics(bool bUseKB) override;
	void         CollectMemInfo(SCryEngineStatsGlobalMemInfo&);

#ifndef _RELEASE
	virtual void GetCheckpointData(ICheckpointData& data) override;
	virtual void IncreaseCheckpointLoadCount() override;
	virtual void SetLoadOrigin(LevelLoadOrigin origin) override;
#endif

	void                         Relaunch(bool bRelaunch) override;
	bool                         IsRelaunch() const override        { return m_bRelaunch; }
	void                         SerializingFile(int mode) override { m_iLoadingMode = mode; }
	int                          IsSerializingFile() const override { return m_iLoadingMode; }
	void                         Quit() override;
	bool                         IsQuitting() const override;
	bool                         IsShaderCacheGenMode() const override { return m_bShaderCacheGenMode; }
	virtual const char*          GetUserName() override;
	virtual int                  GetApplicationInstance() override;
	virtual sUpdateTimes&        GetCurrentUpdateTimeStats() override;
	virtual const sUpdateTimes*  GetUpdateTimeStats(uint32&, uint32&) override;
	virtual void                 FillRandomMT(uint32* pOutWords, uint32 numWords) override;

	virtual CRndGen&             GetRandomGenerator() override   { return m_randomGenerator; }

	INetwork*                    GetINetwork() override          { return m_env.pNetwork; }
	IRenderer*                   GetIRenderer() override         { return m_env.pRenderer; }
	IInput*                      GetIInput() override            { return m_env.pInput; }
	ITimer*                      GetITimer() override            { return m_env.pTimer; }
	ICryPak*                     GetIPak() override              { return m_env.pCryPak; }
	IConsole*                    GetIConsole() override          { return m_env.pConsole; }
	IRemoteConsole*              GetIRemoteConsole() override;
	IScriptSystem*               GetIScriptSystem() override     { return m_env.pScriptSystem; }
	I3DEngine*                   GetI3DEngine() override         { return m_env.p3DEngine; }
	ICharacterManager*           GetIAnimationSystem() override  { return m_env.pCharacterManager; }
	CryAudio::IAudioSystem*      GetIAudioSystem() override      { return m_env.pAudioSystem; }
	IPhysicalWorld*              GetIPhysicalWorld() override    { return m_env.pPhysicalWorld; }
	IMovieSystem*                GetIMovieSystem() override      { return m_env.pMovieSystem; }
	IAISystem*                   GetAISystem() override          { return m_env.pAISystem; }
	IMemoryManager*              GetIMemoryManager() override    { return m_pMemoryManager; }
	IEntitySystem*               GetIEntitySystem() override     { return m_env.pEntitySystem; }
	LiveCreate::IHost*           GetLiveCreateHost()             { return m_env.pLiveCreateHost; }
	LiveCreate::IManager*        GetLiveCreateManager()          { return m_env.pLiveCreateManager; }
	IThreadManager*              GetIThreadManager() override    { return m_env.pThreadManager; }
	IMonoEngineModule*           GetIMonoEngineModule() override { return m_env.pMonoRuntime; }
	ICryFont*                    GetICryFont() override          { return m_env.pCryFont; }
	ILog*                        GetILog() override              { return m_env.pLog; }
	ICmdLine*                    GetICmdLine() override;
	IStreamEngine*               GetStreamEngine() override;
	IValidator*                  GetIValidator() override { return m_pValidator; }
	IPhysicsDebugRenderer*       GetIPhysicsDebugRenderer() override;
	IPhysRenderer*               GetIPhysRenderer() override;
	ICryProfilingSystem*         GetProfilingSystem() override;
	ILegacyProfiler*             GetLegacyProfilerInterface() override;
	virtual IDiskProfiler*       GetIDiskProfiler() override          { return m_pDiskProfiler; }
	INameTable*                  GetINameTable() override             { return m_env.pNameTable; }
	IBudgetingSystem*            GetIBudgetingSystem() override       { return(m_pIBudgetingSystem); }
	IFlowSystem*                 GetIFlowSystem() override            { return m_env.pFlowSystem; }
	IDialogSystem*               GetIDialogSystem() override          { return m_env.pDialogSystem; }
	DRS::IDynamicResponseSystem* GetIDynamicResponseSystem()          { return m_env.pDynamicResponseSystem; }
	IHardwareMouse*              GetIHardwareMouse() override         { return m_env.pHardwareMouse; }
	ISystemEventDispatcher*      GetISystemEventDispatcher() override { return m_pSystemEventDispatcher; }
#ifdef CRY_TESTING
	CryTest::ITestSystem*        GetITestSystem() override            { return m_pTestSystem.get(); }
#endif
	IUserAnalyticsSystem*         GetIUserAnalyticsSystem() override;
	Cry::IPluginManager*          GetIPluginManager() override;
	Cry::IProjectManager*         GetIProjectManager() override;
	virtual Cry::UDR::IUDRSystem* GetIUDR() override                 { return m_env.pUDR; }

	IResourceManager*            GetIResourceManager() override;
	ITextModeConsole*            GetITextModeConsole() override;
	IFileChangeMonitor*          GetIFileChangeMonitor() override   { return m_env.pFileChangeMonitor; }
	INotificationNetwork*        GetINotificationNetwork() override { return m_pNotificationNetwork; }
	IPlatformOS*                 GetPlatformOS() override           { return m_pPlatformOS.get(); }
	ICryPerfHUD*                 GetPerfHUD() override              { return m_pPerfHUD; }
	minigui::IMiniGUI*           GetMiniGUI() override              { return m_pMiniGUI; }
	IZLibCompressor*             GetIZLibCompressor() override      { return m_pIZLibCompressor; }
	IZLibDecompressor*           GetIZLibDecompressor() override    { return m_pIZLibDecompressor; }
	ILZ4Decompressor*            GetLZ4Decompressor() override      { return m_pILZ4Decompressor; }
	CRY_HWND                     GetHWND() override                 { return m_hWnd; }
	CRY_HWND                     GetActiveHWND() override           { return m_hWndActive; }
	//////////////////////////////////////////////////////////////////////////
	// retrieves the perlin noise singleton instance
	CPNoise3*      GetNoiseGen() override;
	virtual uint64 GetUpdateCounter() override { return m_nUpdateCounter; }

	virtual void   SetLoadingProgressListener(ILoadingProgressListener* pLoadingProgressListener) override
	{
		m_pProgressListener = pLoadingProgressListener;
	}

	virtual ILoadingProgressListener* GetLoadingProgressListener() const override
	{
		return m_pProgressListener;
	}

	void         SetIFlowSystem(IFlowSystem* pFlowSystem) override                              { m_env.pFlowSystem = pFlowSystem; }
	void         SetIDialogSystem(IDialogSystem* pDialogSystem) override                        { m_env.pDialogSystem = pDialogSystem; }
	void         SetIDynamicResponseSystem(DRS::IDynamicResponseSystem* pDynamicResponseSystem) { m_env.pDynamicResponseSystem = pDynamicResponseSystem; }
	void         SetIMaterialEffects(IMaterialEffects* pMaterialEffects) override               { m_env.pMaterialEffects = pMaterialEffects; }
	void         SetIParticleManager(IParticleManager* pParticleManager) override               { m_env.pParticleManager = pParticleManager; }
	void         SetIOpticsManager(IOpticsManager* pOpticsManager) override                     { m_env.pOpticsManager = pOpticsManager; }
	void         SetIFileChangeMonitor(IFileChangeMonitor* pFileChangeMonitor) override         { m_env.pFileChangeMonitor = pFileChangeMonitor; }
	void         SetIFlashUI(IFlashUI* pFlashUI) override                                       { m_env.pFlashUI = pFlashUI; }
	void         SetIUIFramework(UIFramework::IUIFramework* pUIFramework) override              { m_env.pUIFramework = pUIFramework; }
	void         ChangeUserPath(const char* sUserPath) override;
	void         DetectGameFolderAccessRights();

	virtual void ExecuteCommandLine() override;

	virtual void GetUpdateStats(SSystemUpdateStats& stats) override;

	//////////////////////////////////////////////////////////////////////////
	virtual XmlNodeRef CreateXmlNode(const char* sNodeName = "", bool bReuseStrings = false) override;
	virtual XmlNodeRef LoadXmlFromFile(const char* sFilename, bool bReuseStrings = false) override;
	virtual XmlNodeRef LoadXmlFromBuffer(const char* buffer, size_t size, bool bReuseStrings = false) override;
	virtual IXmlUtils* GetXmlUtils() override;
	//////////////////////////////////////////////////////////////////////////

	virtual Serialization::IArchiveHost* GetArchiveHost() const override { return m_pArchiveHost; }

	void                                 SetViewCamera(CCamera& Camera) override;
	const CCamera& GetViewCamera() const override { return m_ViewCamera; }

	virtual uint32 GetCPUFlags() override;
	virtual int    GetLogicalCPUCount() override;

	void           IgnoreUpdates(bool bIgnore) override { m_bIgnoreUpdates = bIgnore; }
	void           SetGCFrequency(const float fRate);

	void           SetIProcess(IProcess* process) override;
	IProcess*      GetIProcess() override { return m_pProcess; }
	//@}

	void StartBootProfilerSession(const char* szName) override;
	void EndBootProfilerSession() override;

	void         SleepIfNeeded();

	virtual void DisplayErrorMessage(const char* acMessage, float fTime, const float* pfColor = 0, bool bHardError = true) override;

	virtual void FatalError(const char* format, ...) override PRINTF_PARAMS(2, 3);
	virtual void ReportBug(const char* format, ...) override PRINTF_PARAMS(2, 3);
	// Validator Warning.
	void         WarningV(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, va_list args) override;
	void         Warning(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, ...) override;
	void         WarningOnce(EValidatorModule module, EValidatorSeverity severity, int flags, const char* file, const char* format, ...) override;
	bool         CheckLogVerbosity(int verbosity) override;

	virtual void DebugStats(bool checkpoint, bool leaks) override;
	void         DumpWinHeaps() override;

	// tries to log the call stack . for DEBUG purposes only
	void        LogCallStack();

	virtual int DumpMMStats(bool log) override;

#if defined(CVARS_WHITELIST)
	virtual ILoadConfigurationEntrySink* GetCVarsWhiteListConfigSink() const override { return m_pCVarsWhitelistConfigSink; }
#else
	virtual ILoadConfigurationEntrySink* GetCVarsWhiteListConfigSink() const override { return NULL; }
#endif // defined(CVARS_WHITELIST)
	virtual bool                         IsCVarWhitelisted(const char* szName, bool silent) const override;

	virtual ISystemUserCallback*         GetUserCallback() const override { return m_pUserCallback; }

	//////////////////////////////////////////////////////////////////////////
	virtual void              SaveConfiguration() override;
	virtual void              LoadConfiguration(const char* sFilename, ILoadConfigurationEntrySink* pSink = 0, ELoadConfigurationType configType = eLoadConfigDefault,
	                                            ELoadConfigurationFlags flags = ELoadConfigurationFlags::None) override;
	virtual ESystemConfigSpec GetConfigSpec(bool bClient = true) override;
	virtual void              SetConfigSpec(ESystemConfigSpec spec, bool bClient) override;
	virtual ESystemConfigSpec GetMaxConfigSpec() const override;
	//////////////////////////////////////////////////////////////////////////

	virtual int        SetThreadState(ESubsystem subsys, bool bActive) override;
	virtual ICrySizer* CreateSizer() override;
	virtual bool       IsPaused() const override { return m_bPaused; }

	//////////////////////////////////////////////////////////////////////////
	virtual IHmdManager* GetHmdManager() override { return m_pHmdManager; }

	//////////////////////////////////////////////////////////////////////////
	// Creates an instance of the AVI Reader class.
	virtual IAVI_Reader*          CreateAVIReader() override;
	// Release the AVI reader
	virtual void                  ReleaseAVIReader(IAVI_Reader* pAVIReader) override;

	virtual ILocalizationManager* GetLocalizationManager() override;
	virtual void                  debug_GetCallStack(const char** pFunctions, int& nCount) override;
	virtual void                  debug_LogCallStack(int nMaxFuncs = 32, int nFlags = 0) override;
	// Get the current callstack in raw address form (more lightweight than the above functions)
	// static as memReplay needs it before CSystem has been setup - expose a ISystem interface to this function if you need it outside CrySystem
	static void                  debug_GetCallStackRaw(void** callstack, uint32& callstackLength);

	virtual ICryFactoryRegistry* GetCryFactoryRegistry() const override;

public:
	bool Initialize(SSystemInitParams& initParams);
	void RunMainLoop();

	// this enumeration describes the purpose for which the statistics is gathered.
	// if it's gathered to be dumped, then some different rules may be applied
	enum MemStatsPurposeEnum {nMSP_ForDisplay, nMSP_ForDump, nMSP_ForCrashLog, nMSP_ForBudget};
	// collects the whole memory statistics into the given sizer object
	void                 CollectMemStats(ICrySizer* pSizer, MemStatsPurposeEnum nPurpose = nMSP_ForDisplay, std::vector<SmallModuleInfo>* pStats = 0);
	void                 GetExeSizes(ICrySizer* pSizer, MemStatsPurposeEnum nPurpose = nMSP_ForDisplay);
	//! refreshes the m_pMemStats if necessary; creates it if it's not created
	void                 TickMemStats(MemStatsPurposeEnum nPurpose = nMSP_ForDisplay, IResourceCollector* pResourceCollector = 0);

	void                 SetVersionInfo(const char* const szVersion);

	virtual ICryFactory* LoadModuleWithFactory(const char* dllName, const CryInterfaceID& moduleInterfaceId) override;
	virtual bool         UnloadEngineModule(const char* dllName) override;

#if CRY_PLATFORM_WINDOWS
	friend LRESULT WINAPI WndProc(CRY_HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
	virtual void*         GetRootWindowMessageHandler() override;
	virtual void          RegisterWindowMessageHandler(IWindowMessageHandler* pHandler) override;
	virtual void          UnregisterWindowMessageHandler(IWindowMessageHandler* pHandler) override;
	virtual int           PumpWindowMessage(bool bAll, CRY_HWND hWnd) override;
	virtual bool          IsImeSupported() const override;
	virtual IImeManager*  GetImeManager() const override { return m_pImeManager; }

	// IWindowMessageHandler
#if CRY_PLATFORM_WINDOWS
	virtual bool HandleMessage(CRY_HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
#endif
	// ~IWindowMessageHandler

	WIN_HMODULE LoadDynamicLibrary(const char* dllName, bool bQuitIfNotFound = true, bool bLogLoadingInfo = false);
	bool        UnloadDynamicLibrary(const char* dllName);
	void        GetLoadedDynamicLibraries(std::vector<string>& moduleNames) const;

private:

	bool InitializeEngineModule(const SSystemInitParams& startupParams, const char* dllName, const CryInterfaceID& moduleInterfaceId, bool bQuitIfNotFound);

	// Release all resources.
	void ShutDown();

	//! @name Initialization routines
	//@{

	bool InitNetwork(const SSystemInitParams& startupParams);
	bool InitInput(const SSystemInitParams& startupParams);

	bool InitRenderer(SSystemInitParams& startupParams);
	bool InitPhysics(const SSystemInitParams& startupParams);
	bool InitPhysicsRenderer(const SSystemInitParams& startupParams);

	bool InitFont(const SSystemInitParams& startupParams);
	bool InitFlash();
	bool InitAISystem(const SSystemInitParams& startupParams);
	bool InitScriptSystem(const SSystemInitParams& startupParams);
	bool InitFileSystem(const SSystemInitParams& startupParams);
	void InitLog(const SSystemInitParams& startupParams);
	void LoadPatchPaks();
	bool InitFileSystem_LoadEngineFolders();
	void InitResourceCacheFolder();
	bool InitStreamEngine();
	bool Init3DEngine(const SSystemInitParams& startupParams);
	bool InitAnimationSystem(const SSystemInitParams& startupParams);
	bool InitMovieSystem(const SSystemInitParams& startupParams);
	bool InitReflectionSystem(const SSystemInitParams& startupParams);
	bool InitSchematyc(const SSystemInitParams& startupParams);
	bool InitEntitySystem(const SSystemInitParams& startupParams);
	bool InitDynamicResponseSystem(const SSystemInitParams& startupParams);
	bool InitLiveCreate(const SSystemInitParams& startupParams);
	bool InitMonoBridge(const SSystemInitParams& startupParams);
	bool InitUDR(const SSystemInitParams& startupParams);
	void InitGameFramework(SSystemInitParams& startupParams);
	bool OpenRenderLibrary(const SSystemInitParams& startupParams, int type);
	bool OpenRenderLibrary(const SSystemInitParams& startupParams, const char* t_rend);
	bool CloseRenderLibrary(const char* t_rend);

	//@}

	//! @name Unload routines
	//@{
	void UnloadSchematycModule();
	//@}

	bool ParseSystemConfig(string& sFileName);

	//////////////////////////////////////////////////////////////////////////
	// Threading functions.
	//////////////////////////////////////////////////////////////////////////
	void InitThreadSystem();
	void ShutDownThreadSystem();

	//////////////////////////////////////////////////////////////////////////
	// Helper functions.
	//////////////////////////////////////////////////////////////////////////
	void        CreateRendererVars(const SSystemInitParams& startupParams);
	void        CreateSystemVars();
	void        CreateAudioVars();
	void        RenderStats();
	void        RenderOverscanBorders();
	void        RenderJobStats();
	void        RenderMemStats();
	void        RenderThreadInfo();
	void        RenderMemoryInfo();
	void        FreeLib(WIN_HMODULE hLibModule);
	void        QueryVersionInfo();
	void        LogVersion();
	void        SetDevMode(bool bEnable);
	void        InitScriptDebugger();

	void        CreatePhysicsThread();
	void        KillPhysicsThread();

	static void SystemVersionChanged(ICVar* pCVar);

	bool        ReLaunchMediaCenter();
	void        LogSystemInfo();
	void        UpdateAudioSystems();
	void        PrePhysicsUpdate();

	// recursive
	// Arguments:
	//   sPath - e.g. "Game/Config/CVarGroups"
	void AddCVarGroupDirectory(const string& sPath);

#if CRY_PLATFORM_WINDOWS
	bool GetWinGameFolder(char* szMyDocumentsPath, int maxPathSize);
#endif
public:
	// interface ISystem -------------------------------------------
	virtual void SetForceNonDevMode(const bool bValue) override;
	virtual bool GetForceNonDevMode() const override;
	virtual bool WasInDevMode() const override { return m_bWasInDevMode; }
	virtual bool IsDevMode() const override    { return m_bInDevMode && !GetForceNonDevMode(); }
	virtual bool IsMODValid(const char* szMODName) const override
	{
		if (!szMODName || strstr(szMODName, ".") || strstr(szMODName, "\\") || stricmp(szMODName, PathUtil::GetGameFolder().c_str()) == 0)
			return (false);
		return (true);
	}
	virtual void AutoDetectSpec(const bool detectResolution) override;

	virtual void AsyncMemcpy(void* dst, const void* src, size_t size, int nFlags, volatile int* sync) override
	{
#if !defined(CRY_ASYNC_MEMCPY_DELEGATE_TO_CRYSYSTEM)
		cryAsyncMemcpy(dst, src, size, nFlags, sync);
#else
		_cryAsyncMemcpy(dst, src, size, nFlags, sync);
#endif
	}

#if CRY_PLATFORM_DURANGO
	virtual void OnPLMEvent(EPLM_Event event);
#endif

	// -------------------------------------------------------------

	//! attaches the given variable to the given container;
	//! recreates the variable if necessary
	ICVar*            attachVariable(const char* szVarName, int* pContainer, const char* szComment, int dwFlags = 0);

	CCpuFeatures*     GetCPUFeatures()                { return m_pCpu; }

	string&           GetDelayedScreeenshot()         { return m_sDelayedScreeenshot; }

	CVisRegTest*&     GetVisRegTestPtrRef()           { return m_pVisRegTest; }

	const CTimeValue& GetLastTickTime(void) const     { return m_lastTickTime; }
	const ICVar*      GetDedicatedMaxRate(void) const { return m_svDedicatedMaxRate; }

private: // ------------------------------------------------------

	// System environment.
#if defined(SYS_ENV_AS_STRUCT)
	//since gEnv is a global var, this should just be a reference for code consistency
	SSystemGlobalEnvironment & m_env;
#else
	SSystemGlobalEnvironment m_env;
#endif

	CTimer             m_Time;                  //!<
	CCamera            m_ViewCamera;            //!<
	volatile bool      m_bQuit;                 //!< if is true the system is quitting. Volatile as it may be polled from multiple threads.
	bool               m_bShaderCacheGenMode;   //!< true if the application runs in shader cache generation mode
	bool               m_bRelaunch;             //!< relaunching the app or not (true beforerelaunch)
	int                m_iLoadingMode;          //!< Game is loading w/o changing context (0 not, 1 quickloading, 2 full loading)
	bool               m_bNoCrashDialog;
	bool               m_bPreviewMode;          //!< If running in Preview mode.
	bool               m_bUIFrameworkMode;
	bool               m_bIgnoreUpdates;        //!< When set to true will ignore Update and Render calls,
	IValidator*        m_pValidator;            //!< Pointer to validator interface.
	bool               m_bForceNonDevMode;      //!< true when running on a cheat protected server or a client that is connected to it (not used in singlplayer)
	bool               m_bWasInDevMode;         //!< Set to true if was in dev mode.
	bool               m_bInDevMode;            //!< Set to true if was in dev mode.
	bool               m_bGameFolderWritable;   //!< True when verified that current game folder have write access.
	SDefaultValidator* m_pDefaultValidator;     //!<
	string             m_sDelayedScreeenshot;   //!< to delay a screenshot call for a frame
	CCpuFeatures*      m_pCpu;                  //!< CPU features
	int                m_ttMemStatSS;           //!< Time to memstat screenshot
	string             m_szCmdLine;

	int                m_iTraceAllocations;

#ifndef _RELEASE
	int             m_checkpointLoadCount;      // Total times game has loaded from a checkpoint
	LevelLoadOrigin m_loadOrigin;               // Where the load was initiated from
	bool            m_hasJustResumed;           // Has resume game just been called
	bool            m_expectingMapCommand;
#endif

	IHmdManager* m_pHmdManager;

	//! DLLs handles.
	struct SDllHandles
	{
		WIN_HMODULE hRenderer;
		WIN_HMODULE hInput;
		WIN_HMODULE hFlash;
		WIN_HMODULE hSound;
		WIN_HMODULE hEntitySystem;
		WIN_HMODULE hNetwork;
		WIN_HMODULE hAI;
		WIN_HMODULE hMovie;
		WIN_HMODULE hPhysics;
		WIN_HMODULE hFont;
		WIN_HMODULE hScript;
		WIN_HMODULE h3DEngine;
		WIN_HMODULE hAnimation;
		WIN_HMODULE hIndoor;
		WIN_HMODULE hGame;
		WIN_HMODULE hLiveCreate;
		WIN_HMODULE hInterface;
	};
	SDllHandles m_dll;

	std::unordered_map<string, WIN_HMODULE, stl::hash_strcmp<string>> m_moduleDLLHandles;

	//! THe streaming engine
	CStreamEngine* m_pStreamEngine;

	//! current active process
	IProcess*       m_pProcess;

	IMemoryManager* m_pMemoryManager;

	CPhysRenderer*  m_pPhysRenderer;
	CCamera         m_PhysRendererCamera;
	ICVar*          m_p_draw_helpers_str;
	int             m_iJumpToPhysProfileEnt;

	CTimeValue      m_lastTickTime;

	//! system event dispatcher
	ISystemEventDispatcher* m_pSystemEventDispatcher;

	//! The default font
	IFFont* m_pIFont;

	//! System to monitor given budget.
	IBudgetingSystem* m_pIBudgetingSystem;

	//! System to access zlib compressor
	IZLibCompressor* m_pIZLibCompressor;

	//! System to access zlib decompressor
	IZLibDecompressor* m_pIZLibDecompressor;

	//! System to access lz4 hc decompressor
	ILZ4Decompressor*   m_pILZ4Decompressor;

	CNULLRenderAuxGeom* m_pNULLRenderAuxGeom;

	// XML Utils interface.
	CXmlUtils*                   m_pXMLUtils;

	Serialization::IArchiveHost* m_pArchiveHost;

	//! global root folder
	string m_root;
	int    m_iApplicationInstance;

	//! to hold the values stored in system.cfg
	//! because editor uses it's own values,
	//! and then saves them to file, overwriting the user's resolution.
	int m_iHeight = 0;
	int m_iWidth = 0;
	int m_iColorBits = 0;

	// System console variables.
	//////////////////////////////////////////////////////////////////////////

	// DLL names
	ICVar* m_sys_dll_ai;
	ICVar* m_sys_dll_response_system;
	ICVar* m_sys_user_folder;

#if !defined(_RELEASE)
	ICVar* m_sys_resource_cache_folder;
#endif

	ICVar* m_sys_initpreloadpacks;
	ICVar* m_sys_menupreloadpacks;

	ICVar* m_cvAIUpdate;
	ICVar* m_rIntialWindowSizeRatio;
	ICVar* m_rWidth;
	ICVar* m_rHeight;
	ICVar* m_rColorBits;
	ICVar* m_rDepthBits;
	ICVar* m_rStencilBits;
	ICVar* m_rFullscreen;
	ICVar* m_rFullsceenNativeRes;
	ICVar* m_rWindowState;
	ICVar* m_rDriver;
	ICVar* m_pPhysicsLibrary;
	ICVar* m_rDisplayInfo;
	ICVar* m_rDisplayInfoTargetPolygons;
	ICVar* m_rDisplayInfoTargetDrawCalls;
	ICVar* m_rDisplayInfoTargetFPS;
	ICVar* m_rOverscanBordersDrawDebugView;
	ICVar* m_sysNoUpdate;
	ICVar* m_cvEntitySuppressionLevel;
	ICVar* m_pCVarQuit;
	ICVar* m_cvMemStats;
	ICVar* m_cvMemStatsThreshold;
	ICVar* m_cvMemStatsMaxDepth;
	int profile_meminfo;
	bool m_logMemoryInfo;
	ICVar* m_sysKeyboard;
	ICVar* m_sysWarnings;                   //!< might be 0, "sys_warnings" - Treat warning as errors.
	ICVar* m_cvSSInfo;                      //!< might be 0, "sys_SSInfo" 0/1 - get file sourcesafe info
	ICVar* m_svDedicatedMaxRate;
	ICVar* m_svAISystem;
	ICVar* m_clAISystem;
	ICVar* m_sys_profile_watchdog_timeout;
	ICVar* m_sys_job_system_filter;
	ICVar* m_sys_job_system_enable;
	ICVar* m_sys_job_system_profiler;
	ICVar* m_sys_job_system_max_worker;
	ICVar* m_sys_job_system_worker_boost_enabled;
	ICVar* m_sys_spec;
	ICVar* m_sys_firstlaunch;

	ICVar* m_sys_physics_enable_MT;
	ICVar* m_sys_audio_disable;

	ICVar* m_sys_SimulateTask;

	ICVar* m_sys_min_step;
	ICVar* m_sys_max_step;
	ICVar* m_sys_enable_budgetmonitoring;
	ICVar* m_sys_memory_debug;
	ICVar* m_sys_use_Mono;

	//	ICVar *m_sys_filecache;
	ICVar* m_gpu_particle_physics;

	ICVar* m_sys_vr_support;  // depends on the m_pHmdManager

	string m_sSavedRDriver;                 //!< to restore the driver when quitting the dedicated server

	//////////////////////////////////////////////////////////////////////////
	//! User define callback for system events.
	ISystemUserCallback* m_pUserCallback;

#if defined(CVARS_WHITELIST)
	//////////////////////////////////////////////////////////////////////////
	//! User define callback for whitelisting cvars
	ILoadConfigurationEntrySink* m_pCVarsWhitelistConfigSink;
#endif // defined(CVARS_WHITELIST)

	CRY_HWND m_hWnd = nullptr;
	CRY_HWND m_hWndActive = nullptr;
	bool     m_throttleFPS = false;

	// this is the memory statistics that is retained in memory between frames
	// in which it's not gathered
	CrySizerStats*               m_pMemStats;
	CrySizerImpl*                m_pSizer;

	IDiskProfiler*               m_pDiskProfiler;

	std::unique_ptr<IPlatformOS> m_pPlatformOS;
	ICryPerfHUD*                 m_pPerfHUD;
	minigui::IMiniGUI*           m_pMiniGUI;

	//int m_nCurrentLogVerbosity;

	SFileVersion              m_fileVersion;
	SFileVersion              m_productVersion;
	SFileVersion              m_buildVersion;

	CLocalizedStringsManager* m_pLocalizationManager;

	// Name table.
	CNameTable                       m_nameTable;

	IPhysicsThreadTask*              m_PhysThread;

	ESystemConfigSpec                m_nServerConfigSpec;
	ESystemConfigSpec                m_nMaxConfigSpec;

	std::unique_ptr<CServerThrottle> m_pServerThrottle;

	class CCryProfilingSystemImpl*   m_pProfilingSystem;
	class CCryProfilingSystem*       m_pLegacyProfiler;
	class CProfilingRenderer*        m_pProfileRenderer;
	sUpdateTimes                     m_UpdateTimes[NUM_UPDATE_TIMES];
	uint32                           m_UpdateTimesIdx;

	// Pause mode.
	bool   m_bPaused;
	uint8  m_PlatformOSCreateFlags;
	bool   m_bNoUpdate;

	uint64 m_nUpdateCounter;

	// MT random generator
	CryCriticalSection m_mtLock;
	CMTRand_int32*     m_pMtState;

	// The random generator used by the engine and all its modules
	CRndGen m_randomGenerator;

public:
	//! Pointer to the download manager
	CDownloadManager* m_pDownloadManager;
	
	//////////////////////////////////////////////////////////////////////////
	// File version.
	//////////////////////////////////////////////////////////////////////////
	virtual const SFileVersion& GetFileVersion() override;
	virtual const SFileVersion& GetProductVersion() override;
	virtual const SFileVersion& GetBuildVersion() override;

	bool                        CompressDataBlock(const void* input, size_t inputSize, void* output, size_t& outputSize, int level) override;
	bool                        DecompressDataBlock(const void* input, size_t inputSize, void* output, size_t& outputSize) override;

	void                        OpenBasicPaks(bool bLoadGamePaks);
	void                        OpenLanguagePak(char const* const szLanguage);
	void                        OpenLanguageAudioPak(char const* const szLanguage);
	void                        GetLocalizedPath(char const* const szLanguage, string& szLocalizedPath);
	void                        GetLocalizedAudioPath(char const* const szLanguage, string& szLocalizedPath);
	void                        CloseLanguagePak(char const* const szLanguage);
	void                        CloseLanguageAudioPak(char const* const szLanguage);

	void                        Deltree(const char* szFolder, bool bRecurse);
	void                        UpdateMovieSystem(const int updateFlags, const float fFrameTime, const bool bPreUpdate);

	//////////////////////////////////////////////////////////////////////////
	// CryAssert and error related.
	virtual bool RegisterErrorObserver(IErrorObserver* errorObserver) override;
	bool         UnregisterErrorObserver(IErrorObserver* errorObserver) override;

	void         OnFatalError(const char* message);

#if defined(USE_CRY_ASSERT)
	virtual void OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber) override;
#endif

	virtual void ClearErrorMessages() override
	{
		m_ErrorMessages.clear();
	}

	virtual void AddPlatformOSCreateFlag(const uint8 createFlag) override { m_PlatformOSCreateFlags |= createFlag; }

	virtual bool IsLoading() override
	{
		return (m_systemGlobalState < ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END);
	}

	virtual ESystemGlobalState GetSystemGlobalState(void) override;
	virtual void               SetSystemGlobalState(const ESystemGlobalState systemGlobalState) override;

#if !defined(_RELEASE)
	virtual bool IsSavingResourceList() const override { return (g_cvars.pakVars.nSaveLevelResourceList != 0); }
#endif

private:
	std::vector<IErrorObserver*> m_errorObservers;
	ESystemGlobalState           m_systemGlobalState = ESYSTEM_GLOBAL_STATE_INIT;
	static const char* GetSystemGlobalStateName(const ESystemGlobalState systemGlobalState);

public:
	void InitLocalization();
	void UpdateUpdateTimes();

protected: // -------------------------------------------------------------
	ILoadingProgressListener*             m_pProgressListener;
	CCmdLine*                             m_pCmdLine;
#ifdef CRY_TESTING
	std::unique_ptr<CryTest::ITestSystem> m_pTestSystem;
#endif
	CVisRegTest*                          m_pVisRegTest;
	CResourceManager*                     m_pResourceManager;
	ITextModeConsole*                     m_pTextModeConsole;
	INotificationNetwork*                 m_pNotificationNetwork;
	CCryPluginManager*                    m_pPluginManager;
	CUserAnalyticsSystem*                 m_pUserAnalyticsSystem;
	Cry::CProjectManager*                 m_pProjectManager;
	CManualFrameStepController*           m_pManualFrameStepController = nullptr;

	bool   m_hasWindowFocus = true;

	string m_binariesDir;
	string m_currentLanguageAudio;

	std::vector<std::pair<CTimeValue, float>> m_updateTimes;

#if !defined(CRY_IS_MONOLITHIC_BUILD)
	CCryLibrary m_gameLibrary;
#endif

	struct SErrorMessage
	{
		string m_Message;
		float  m_fTimeToShow;
		float  m_Color[4];
		bool   m_HardFailure;
	};
	typedef std::list<SErrorMessage> TErrorMessages;
	TErrorMessages                   m_ErrorMessages;
	bool                             m_bHasRenderedErrorMessage;

	std::unordered_map<uint32, bool> m_mapWarningOnceAlreadyPrinted;
	CryMutex                         m_mapWarningOnceMutex;

	friend struct SDefaultValidator;
	friend struct SCryEngineFoldersLoader;
	//	friend void ScreenshotCmd( IConsoleCmdArgs *pParams );

	std::vector<IWindowMessageHandler*> m_windowMessageHandlers;
	IImeManager*                        m_pImeManager;

	class CWatchdogThread*              m_pWatchdog = nullptr;
	static void WatchDogTimeOutChanged(ICVar* cvar);
};

/*extern static */ bool QueryModuleMemoryInfo(SCryEngineStatsModuleInfo& moduleInfo, int index);
