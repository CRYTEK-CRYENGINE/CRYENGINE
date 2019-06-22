// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemInit.h"
#if defined(MAP_LOADING_SLICING)
	#include "SystemScheduler.h"
#endif // defined(MAP_LOADING_SLICING)
#include <CryCore/CryCustomTypes.h>
#include <CryCore/Platform/CryLibrary.h>
#include <CryString/StringUtils.h>
#include "NullImplementation/NullInput.h"
#include "NullImplementation/NullLiveCreate.h"
#include "NullImplementation/NullResponseSystem.h"
#include "NullImplementation/NULLRenderAuxGeom.h"
#include "MemoryManager.h"
#include "MemReplay.h"
#include "ImeManager.h"
#include <CrySystem/IEngineModule.h>
#include <CrySystem/ICryPlugin.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <CryMono/IMonoRuntime.h>
#include <CryGame/IGameStartup.h>
#include <CryFont/IFont.h>
#include <CrySystem/ConsoleRegistration.h>

#if (CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID) && !defined(DEDICATED_SERVER)
	#include <dlfcn.h>
#endif

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	#include <CrySystem/Scaleform/IScaleformHelper.h>
#endif

#if CRY_PLATFORM_WINDOWS
	#include <float.h>
	#include <timeapi.h>
	#include <algorithm>
#endif

#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAudio/IAudioSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryInput/IInput.h>
#include <CryMovie/IMovieSystem.h>
#include <CryNetwork/INetwork.h>
#include <CryPhysics/IPhysics.h>
#include <CryReflection/Framework.h>
#include <CryRenderer/IRenderer.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/ICmdLine.h>
#include <CrySystem/ILog.h>
#include <CrySystem/IProcess.h>
#include <CryUDR/InterfaceIncludes.h>

#include "CryPak.h"
#include "XConsole.h"
#include "Log.h"
#include "XML/xml.h"
#include "StreamEngine/StreamEngine.h"
#include "BudgetingSystem.h"
#include "PhysRenderer.h"
#include "LocalizedStringManager.h"
#include "SystemEventDispatcher.h"
#include "Statistics.h"
#include "Statistics/LocalMemoryUsage.h"
#include "ThreadConfigManager.h"
#include "HardwareMouse.h"
#include "Validator.h"
#include "ServerThrottle.h"
#include "SystemCFG.h"
#include "AutoDetectSpec.h"
#include "ResourceManager.h"
#include "BootProfiler.h"
#include "Profiling/ProfilingRenderer.h"
#include "Profiling/CryProfilingSystem.h"
#include "DiskProfiler.h"
#include "Watchdog.h"
#include "Statoscope.h"
#ifdef CRY_TESTING
#include "TestSystem.h"
#endif // CRY_TESTING
#include "VisRegTest.h"
#include "MTSafeAllocator.h"
#include "NotificationNetwork.h"
#include "HotUpdate.h"
#include "ExtensionSystem/CryPluginManager.h"
#include "ExtensionSystem/CryFactoryRegistryImpl.h"
#include "ExtensionSystem/TestCases/TestExtensions.h"
#include "CodeCoverage/CodeCheckpointMgr.h"
#include "ZLibCompressor.h"
#include "ZLibDecompressor.h"
#include "LZ4Decompressor.h"
#include "OverloadSceneManager/OverloadSceneManager.h"
#include "ServiceNetwork.h"
#include "RemoteCommand.h"
#include "NullImplementation/NULLAudioSystems.h"
#include "Interprocess/StatsAgent.h"
#include <CryNetwork/ISimpleHttpServer.h>

#include "ProjectManager/ProjectManager.h"

#include <CryCore/Platform/IPlatformOS.h>
#include "PerfHUD.h"
#include "MiniGUI/MiniGUI.h"

#include <CryGame/IGameFramework.h>

#include "Stroboscope/Stroboscope.h"

#include "HMDManager.h"

#include "Serialization/ArchiveHost.h"

#include "CrySchematyc/ICore.h"
#include <CrySchematyc2/IFramework.h>
#include "ManualFrameStep.h"

#if CRY_PLATFORM_IOS
	#include "IOSConsole.h"
#endif

#if CRY_PLATFORM_ANDROID
	#include "AndroidConsole.h"
#endif

#if CRY_PLATFORM_WINDOWS
	#include "DebugCallStack.h"
#elif CRY_PLATFORM_DURANGO
	#include "DurangoDebugCallstack.h"
#endif

#include "WindowsConsole.h"
#include "CmdLine.h"
#include "CPUDetect.h"
#include "UserAnalytics/UserAnalyticsSystem.h"

#if CRY_PLATFORM_WINDOWS
extern LONG WINAPI CryEngineExceptionFilterWER(struct _EXCEPTION_POINTERS* pExceptionPointers);
#endif

#ifdef USE_UNIXCONSOLE
CUNIXConsole* pUnixConsole;
#endif

#if CRY_PLATFORM_ANDROID && defined(ANDROID_OBB)
extern const char*    androidGetPackageName();
extern const char*    androidGetMainExpName();
extern const char*    androidGetPatchExpName();
extern const char*    androidGetExpFilePath();
extern const char*    androidGetAssetFileName();
extern AAssetManager* androidGetAssetManager();
#endif

//////////////////////////////////////////////////////////////////////////
#define DEFAULT_LOG_FILENAME    "Log.txt"

#define CRYENGINE_ENGINE_FOLDER "Engine"

//////////////////////////////////////////////////////////////////////////
#define CRYENGINE_DEFAULT_LOCALIZATION_LANG "english"

//////////////////////////////////////////////////////////////////////////
// Where possible, these are defaults used to initialize cvars
// System.cfg can then be used to override them
// This includes the Game DLL, although it is loaded elsewhere
#define DLL_AUDIOSYSTEM   "CryAudioSystem"
#define DLL_NETWORK       "CryNetwork"
#define DLL_ENTITYSYSTEM  "CryEntitySystem"
#define DLL_SCRIPTSYSTEM  "CryScriptSystem"
#define DLL_INPUT         "CryInput"
#define DLL_PHYSICS       "CryPhysics"
#define DLL_MOVIE         "CryMovie"
#define DLL_AI            "CryAISystem"
#define DLL_ANIMATION     "CryAnimation"
#define DLL_FONT          "CryFont"
#define DLL_3DENGINE      "Cry3DEngine"
#define DLL_RENDERER_DX11 "CryRenderD3D11"
#define DLL_RENDERER_DX12 "CryRenderD3D12"
#define DLL_RENDERER_GL   "CryRenderOpenGL"
#define DLL_RENDERER_VK   "CryRenderVulkan"
#define DLL_RENDERER_GNM  "CryRenderGNM"
#define DLL_LIVECREATE    "CryLiveCreate"
#define DLL_MONO_BRIDGE   "CryMonoBridge"
#define DLL_UDR           "CryUDR"
#define DLL_SCALEFORM     "CryScaleformHelper"

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_DURANGO || CRY_PLATFORM_APPLE
	#define DLL_MODULE_INIT_ISYSTEM "ModuleInitISystem"
	#define DLL_INITFUNC_RENDERER   "PackageRenderConstructor"
	#define DLL_INITFUNC_NETWORK    "CreateNetwork"
	#define DLL_INITFUNC_ENTITY     "CreateEntitySystem"
	#define DLL_INITFUNC_INPUT      "CreateInput"
	#define DLL_INITFUNC_SOUND      "CreateSoundSystem"
	#define DLL_INITFUNC_PHYSIC     "CreatePhysicalWorld"
	#define DLL_INITFUNC_MOVIE      "CreateMovieSystem"
	#define DLL_INITFUNC_AI         "CreateAISystem"
	#define DLL_INITFUNC_SCRIPT     "CreateScriptSystem"
	#define DLL_INITFUNC_FONT       "CreateCryFontInterface"
	#define DLL_INITFUNC_3DENGINE   "CreateCry3DEngine"
	#define DLL_INITFUNC_ANIMATION  "CreateCharManager"
	#define DLL_INITFUNC_LIVECREATE "CreateLiveCreate"
#else
	#define DLL_MODULE_INIT_ISYSTEM (LPCSTR)2
	#define DLL_INITFUNC_RENDERER   (LPCSTR)1
	#define DLL_INITFUNC_RENDERER   (LPCSTR)1
	#define DLL_INITFUNC_NETWORK    (LPCSTR)1
	#define DLL_INITFUNC_ENTITY     (LPCSTR)1
	#define DLL_INITFUNC_INPUT      (LPCSTR)1
	#define DLL_INITFUNC_SOUND      (LPCSTR)1
	#define DLL_INITFUNC_PHYSIC     (LPCSTR)1
	#define DLL_INITFUNC_MOVIE      (LPCSTR)1
	#define DLL_INITFUNC_AI         (LPCSTR)1
	#define DLL_INITFUNC_SCRIPT     (LPCSTR)1
	#define DLL_INITFUNC_FONT       (LPCSTR)1
	#define DLL_INITFUNC_3DENGINE   (LPCSTR)1
	#define DLL_INITFUNC_ANIMATION  (LPCSTR)1
	#define DLL_INITFUNC_LIVECREATE (LPCSTR)1
#endif

//////////////////////////////////////////////////////////////////////////
// Extern declarations for static libraries.
//////////////////////////////////////////////////////////////////////////
#if defined(_LIB)
extern "C"
{
	IAISystem*    CreateAISystem(ISystem* pSystem);
	IMovieSystem* CreateMovieSystem(ISystem* pSystem);
}
#endif //_LIB
//////////////////////////////////////////////////////////////////////////

extern CMTSafeHeap* g_pPakHeap;

#if CRY_PLATFORM_WINDOWS
extern HMODULE gDLLHandle;
#endif

//static int g_sysSpecChanged = false;
int sys_SchematycPlugin;

const char* g_szLvlResExt = "_LvlRes.txt";

#if defined(DEDICATED_SERVER)
struct SCVarsClientConfigSink : public ILoadConfigurationEntrySink
{
	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
	{
		gEnv->pConsole->SetClientDataProbeString(szKey, szValue);
	}
};
#endif

//////////////////////////////////////////////////////////////////////////
static inline void InlineInitializationProcessing(const char* sDescription)
{
	assert(IsHeapValid());
	if (gEnv->pLog)
		gEnv->pLog->UpdateLoadingScreen(0);
}

//////////////////////////////////////////////////////////////////////////
static void CmdCrashTest(IConsoleCmdArgs* pArgs)
{
	CRY_DISABLE_WARN_UNUSED_VARIABLES();
	assert(pArgs);

	if (pArgs->GetArgCount() == 2)
	{
		// This method intentionally crashes, a lot.
#if CRY_COMPILER_MSVC
	#pragma warning(push)
	#pragma warning(disable:4723) // potential divide by 0
#endif // CRY_COMPILER_MSVC
		int crashType = atoi(pArgs->GetArg(1));
		switch (crashType)
		{
		// NULL pointer access
		case 1:
			{
				int* p = 0;
				PREFAST_SUPPRESS_WARNING(6011) * p = 0xABCD;
			}
			break;

		// Floating Point Exception: Devision by 0
		case 2:
			{
				float a = 1.0f;
				memset(&a, 0, sizeof(a));
				float* b = &a;
				float c = 3;
				CryLog("%f", (c / *b));
			}
			break;

		// Memory allocation exception
		case 3:
			while (true)
			{
				new char[40960];
				// cppcheck-suppress memleak
			}
			break;

		// CryFatalError test
		case 4:
			CryFatalError("sys_crashtest 4");
			break;

		// Memory allocation test for small blocks
		case 5:
			while (true)
			{
				new char[128];   //testing the crash handler an exception in the cry memory allocation occurred
				// cppcheck-suppress memleak
			}

		// CryAssert
		case 6:
			{
				CRY_ASSERT_MESSAGE(false, "Testing assert for testing crashes");
			}
			break;

		// Debugbreak
		case 7:
			__debugbreak();
			break;

		// Sleep 10min
		case 8:
			CrySleep(1000 * 60 * 10);
			break;

#if defined(CRY_PLATFORM_WINAPI)
		// Invalid argument passed
		case 9:
			strcpy_s(0, 4, "abc");
			break;

		// Pure virtual function call
		case 10:
			{
				struct Base
				{
					virtual void PureVirtual() const = 0;
					Base() { DispatchDoom(); }
					void         DispatchDoom() { PureVirtual(); }
				};

				struct Derived : public Base
				{
					virtual void PureVirtual() const {}
				};

				Derived d;
			}
			break;
#endif
		default:
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "CmdCrashTest: Unsupported error number \"%i\" provided for crash test function.", crashType);
			break;
		}
#if CRY_COMPILER_MSVC
	#pragma warning(pop)
#endif // CRY_COMPILER_MSVC
	}

	CRY_RESTORE_WARN_UNUSED_VARIABLES();
}

//////////////////////////////////////////////////////////////////////////
class CCrashTestThread : public IThread
{
public:
	void SetArgs(IConsoleCmdArgs* pArgs)
	{
		for (int i = 0; i < pArgs->GetArgCount(); ++i)
		{
			args.push_back(pArgs->GetArg(i));
		}

		line = pArgs->GetCommandLine();
	}
	void ThreadEntry()
	{
		CConsoleCommandArgs commandArg(line, args);

		while (true)
		{
			CmdCrashTest(&commandArg);
			CrySleep(1); // Allow other threads to run
		}
	}

private:
	std::vector<string> args;
	string              line;
};

//////////////////////////////////////////////////////////////////////////
static void CmdCrashTestOnThread(IConsoleCmdArgs* pArgs)
{
	CCrashTestThread* pCrashTestThread = new CCrashTestThread(); // Leak object deliberately as we are trying to crash anyway
	pCrashTestThread->SetArgs(pArgs);

	if (!gEnv->pThreadManager->SpawnThread(pCrashTestThread, "SysCrashTestOnThread"))
	{
		CryFatalError("Error spawning \"SysCrashTestOnThread\" thread.");
	}
}

//////////////////////////////////////////////////////////////////////////
static void CmdDumpJobManagerJobList(IConsoleCmdArgs* pArgs)
{
	if (gEnv->pJobManager)
	{
		gEnv->pJobManager->DumpJobList();
	}
}

//////////////////////////////////////////////////////////////////////////
static void CmdDumpCvars(IConsoleCmdArgs* pArgs)
{
	struct CCVarSink : public ICVarDumpSink
	{
		CCVarSink()
		{
			m_cvars.reserve(4000);
		}

		~CCVarSink()
		{
		}

		void OnElementFound(ICVar* pCVar)
		{
			if (!pCVar)
			{
				return;
			}

			const char* name = pCVar->GetName();
			const char* val = pCVar->GetString();
			m_cvars.push_back({ name, val });
		}

		void LogToFile()
		{
			const char* file_path = "%USER%/dumped_cvars.cfg";
			FILE* pFile = fxopen(file_path, "w");
			if (!pFile)
			{
				return;
			}

			std::sort(m_cvars.begin(), m_cvars.end(), [](const std::pair<const char*, const char*>& rA, const std::pair<const char*, const char*>& rB) -> bool { return strcmp(rA.first, rB.first) < 0; });

#if !defined(_RELEASE) || defined(RELEASE_LOGGING)
			for (const auto& cvar : m_cvars)
			{
				fprintf(pFile, "%s=%s\n", cvar.first, cvar.second);
			}
#endif

			if (gEnv && gEnv->pCryPak)
			{
#if !defined(EXCLUDE_NORMAL_LOG)
				CryPathString path;
				gEnv->pCryPak->AdjustFileName(file_path, path, 0);
				CryLogAlways("\n=================\n CVars dumped to file: \"%s\" \n=================\n", path.c_str());
#endif
			}

			fclose(pFile);
		}

		std::vector<std::pair<const char*, const char*>> m_cvars;
	};

	if (gEnv && gEnv->pConsole)
	{
		CCVarSink sink;
		gEnv->pConsole->DumpCVars(&sink);
		sink.LogToFile();
	}
}

//////////////////////////////////////////////////////////////////////////
static void CmdDumpThreadConfigList(IConsoleCmdArgs* pArgs)
{
#if !defined(RELEASE)
	if (gEnv->pThreadManager)
	{
		gEnv->pThreadManager->GetThreadConfigManager()->DumpThreadConfigurationsToLog();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
#if defined(USE_CRY_ASSERT)
static void CmdSetAssertLevel(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		const int assertLevel = atoi(pArgs->GetArg(1));
		switch (Cry::Assert::ELevel(assertLevel))
		{
		case Cry::Assert::ELevel::Disabled: // "0 = Disable Asserts\n"
			Cry::Assert::SetAssertLevel(Cry::Assert::ELevel::Disabled);
			break;
		case Cry::Assert::ELevel::Enabled:  // "1 = Enable Asserts\n"
			Cry::Assert::SetAssertLevel(Cry::Assert::ELevel::Enabled);
			break;
		case Cry::Assert::ELevel::FatalErrorOnAssert: // "2 = Fatal Error on Assert\n"
			Cry::Assert::SetAssertLevel(Cry::Assert::ELevel::FatalErrorOnAssert);
			break;
		case Cry::Assert::ELevel::DebugBreakOnAssert: // "3 = Debug break on Assert\n"
			Cry::Assert::SetAssertLevel(Cry::Assert::ELevel::DebugBreakOnAssert);
			break;
		default:
			CryLogAlways("Unrecognized assert level: %i", assertLevel);
		}
	}
	else
	{
		CryLogAlways("Current assert level: %i", int(Cry::Assert::GetAssertLevel()));
	}
}

static void CmdLogAsserts(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		const int doLog = atoi(pArgs->GetArg(1));
		Cry::Assert::LogAssertsAlways(doLog != 0);
	}
	else
	{
		CryLogAlways("%s = %i", pArgs->GetArg(0), int(Cry::Assert::LogAssertsAlways()));
	}
}

static void CmdIgnoreAssertsFromModule(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() == 2)
	{
		string requestedModule = pArgs->GetArg(1);

		for (uint i = 0; i < eCryM_Num; ++i)
		{
			if (requestedModule == GetCryModuleName(i))
			{
				Cry::Assert::DisableAssertionsForModule(i);
				return;
			}
		}

		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Tried to ignore assertions for unknown module %s", pArgs->GetArg(1));
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
struct SysSpecOverrideSink : public ILoadConfigurationEntrySink
{
	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
	{
		ICVar* pCvar = gEnv->pConsole->GetCVar(szKey);

		if (pCvar)
		{
			const bool wasNotInConfig = ((pCvar->GetFlags() & VF_WASINCONFIG) == 0);
			bool applyCvar = wasNotInConfig;
			if (applyCvar == false)
			{
				// Special handling for sys_spec_full
				if (stricmp(szKey, "sys_spec_full") == 0)
				{
					// If it is set to 0 then ignore this request to set to something else
					// If it is set to 0 then the user wants to changes system spec settings in system.cfg
					// Ignore the spec if the renderer wasn't initialized.
					if (pCvar->GetIVal() != 0 && gEnv->pRenderer != nullptr)
					{
						applyCvar = true;
					}
				}
			}

			if (applyCvar)
			{
				pCvar->SetFromString(szValue);
			}
			else
			{
				CryLogAlways("NOT VF_WASINCONFIG Ignoring cvar '%s' new value '%s' old value '%s' group '%s'", szKey, szValue, pCvar->GetString(), szGroup);
			}
		}
		else
		{
			CryLogAlways("Can't find cvar '%s' value '%s' group '%s'", szKey, szValue, szGroup);
		}
	}
};

#if CRY_PLATFORM_DESKTOP
struct SysSpecOverrideSinkConsole : public ILoadConfigurationEntrySink
{
	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
	{
		// Ignore platform-specific cvars that should just be executed on the console
		if (stricmp(szGroup, "Platform") == 0)
			return;

		ICVar* pCvar = gEnv->pConsole->GetCVar(szKey);
		if (pCvar)
			pCvar->SetFromString(szValue);
	}
};
#endif

static void OnSysSpecChange(ICVar* pVar)
{
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	SysSpecOverrideSink sysSpecOverrideSink;
	ILoadConfigurationEntrySink* pSysSpecOverrideSinkConsole = NULL;

#if CRY_PLATFORM_DESKTOP
	SysSpecOverrideSinkConsole sysSpecOverrideSinkConsole;
	pSysSpecOverrideSinkConsole = &sysSpecOverrideSinkConsole;
#endif

	//	g_sysSpecChanged = true;
	static int no_recursive = false;
	if (no_recursive)
		return;
	no_recursive = true;

	// finish any outstanding rendering tasks
	if (gEnv->pRenderer)
		gEnv->pRenderer->FlushRTCommands(true, true, true);

	// Called when sys_spec (client config spec) variable changes.
	int spec = pVar->GetIVal();

	if (spec > ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec())
	{
		spec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();
		pVar->Set(spec);
	}

#if CRY_PLATFORM_ORBIS
	spec = CONFIG_ORBIS;
#elif CRY_PLATFORM_DURANGO
	spec = CONFIG_DURANGO;
#elif CRY_PLATFORM_MOBILE
	spec = CONFIG_CUSTOM;
	GetISystem()->LoadConfiguration("mobile.cfg", 0, eLoadConfigSystemSpec);
#endif

	CryLog("OnSysSpecChange(%d)", spec);

	switch (spec)
	{
	case CONFIG_LOW_SPEC:
		GetISystem()->LoadConfiguration("config/LowSpec.cfg", &sysSpecOverrideSink, eLoadConfigSystemSpec);
		break;
	case CONFIG_MEDIUM_SPEC:
		GetISystem()->LoadConfiguration("config/MedSpec.cfg", &sysSpecOverrideSink, eLoadConfigSystemSpec);
		break;
	case CONFIG_HIGH_SPEC:
		GetISystem()->LoadConfiguration("config/HighSpec.cfg", &sysSpecOverrideSink, eLoadConfigSystemSpec);
		break;
	case CONFIG_VERYHIGH_SPEC:
		GetISystem()->LoadConfiguration("config/VeryHighSpec.cfg", &sysSpecOverrideSink, eLoadConfigSystemSpec);
		break;
	case CONFIG_DURANGO:
		GetISystem()->LoadConfiguration("config/durango.cfg", pSysSpecOverrideSinkConsole, eLoadConfigSystemSpec);
		GetISystem()->LoadConfiguration("config/durango_non_x.cfg", pSysSpecOverrideSinkConsole, eLoadConfigSystemSpec);
		break;
	case CONFIG_DURANGO_X:
		GetISystem()->LoadConfiguration("config/durango.cfg", pSysSpecOverrideSinkConsole, eLoadConfigSystemSpec);
		GetISystem()->LoadConfiguration("config/durango_x.cfg", pSysSpecOverrideSinkConsole, eLoadConfigSystemSpec);
		break;
	case CONFIG_ORBIS:
		GetISystem()->LoadConfiguration("config/orbis.cfg", pSysSpecOverrideSinkConsole, eLoadConfigSystemSpec);
		break;

	default:
		// Do nothing.
		break;
	}

	// make sure editor specific settings are not changed
	if (gEnv->IsEditor() && gEnv->pCryPak->IsFileExist("%ENGINEROOT%/editor.cfg"))
	{
		GetISystem()->LoadConfiguration("%ENGINEROOT%/editor.cfg", 0, eLoadConfigInit);
	}

	bool bMultiGPUEnabled = false;
	if (gEnv->pRenderer)
		gEnv->pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPUEnabled);
	if (bMultiGPUEnabled)
		GetISystem()->LoadConfiguration("mgpu.cfg", 0, eLoadConfigSystemSpec);

	bool bChangeServerSpec = true;
	if (gEnv->bMultiplayer)
		bChangeServerSpec = false;
	if (bChangeServerSpec)
		GetISystem()->SetConfigSpec((ESystemConfigSpec)spec, false);

	if (g_cvars.sys_vr_support)
		GetISystem()->LoadConfiguration("vr.cfg", 0, eLoadConfigSystemSpec);

	if (gEnv->pRenderer)
		gEnv->pRenderer->EF_ReloadTextures();

	if (gEnv->p3DEngine)
		gEnv->p3DEngine->GetMaterialManager()->RefreshMaterialRuntime();

	if (gEnv->pRenderer)
		gEnv->pRenderer->FlushPendingUploads();

	no_recursive = false;
}

//////////////////////////////////////////////////////////////////////////
WIN_HMODULE CSystem::LoadDynamicLibrary(const char* szModulePath, bool bQuitIfNotFound, bool bLogLoadingInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	stack_string modulePath = szModulePath;
	modulePath = CrySharedLibraryPrefix + PathUtil::ReplaceExtension(modulePath, CrySharedLibraryExtension);

	MEMSTAT_CONTEXT(EMemStatContextType::Other, "LoadDLL");
	MEMSTAT_CONTEXT(EMemStatContextType::Other, modulePath.c_str());

	stack_string msg;
	msg = "Loading Module ";
	msg += modulePath;
	msg += "...";

	if (m_pUserCallback)
	{
		m_pUserCallback->OnInitProgress(msg.c_str());
	}

	if (bLogLoadingInfo)
	{
		CryLog(msg);
	}

	WIN_HMODULE handle = nullptr;
#if CRY_PLATFORM_WINDOWS
	if (m_binariesDir.empty())
	{
		handle = CryLoadLibrary(modulePath);
		if (!handle && bLogLoadingInfo)
		{
			DWORD dwErrorCode = GetLastError();
			CryLogAlways("DLL Failed to load, error code: %X", dwErrorCode);
		}
	}
	else
	{
		char currentDirectory[1024];
		GetCurrentDirectory(sizeof(currentDirectory), currentDirectory);
		SetCurrentDirectory(m_binariesDir.c_str());
		handle = CryLoadLibrary(modulePath);
		SetCurrentDirectory(currentDirectory);
	}
#elif !defined(CRY_PLATFORM_ORBIS) // TODO: Orbis support
	handle = CryLoadLibrary(modulePath);
#endif

	if (handle == nullptr)
	{
		if (bQuitIfNotFound)
		{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
			CryFatalError("Error loading dynamic library: %s, error :  %s\n", modulePath.c_str(), dlerror());
#else
			CryFatalError("Error loading dynamic library: %s, error code %d", modulePath.c_str(), GetLastError());
#endif

			Quit();
		}

		return nullptr;
	}

	m_moduleDLLHandles.emplace(modulePath, handle);

	//////////////////////////////////////////////////////////////////////////
	// After loading DLL initialize it by calling ModuleInitISystem
	//////////////////////////////////////////////////////////////////////////
	string moduleName = PathUtil::GetFileName(modulePath);

	typedef void*(* PtrFunc_ModuleInitISystem)(ISystem* pSystem, const char* moduleName);
	PtrFunc_ModuleInitISystem pfnModuleInitISystem = (PtrFunc_ModuleInitISystem) CryGetProcAddress(handle, DLL_MODULE_INIT_ISYSTEM);
	if (pfnModuleInitISystem)
	{
		pfnModuleInitISystem(this, moduleName.c_str());
	}

	return handle;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::UnloadDynamicLibrary(const char* szDllName)
{
	stack_string modulePath = szDllName;
	modulePath = CrySharedLibraryPrefix + PathUtil::ReplaceExtension(modulePath, CrySharedLibraryExtension);

	auto moduleIt = m_moduleDLLHandles.find(modulePath);
	if (moduleIt != m_moduleDLLHandles.end())
	{
		string msg = string().Format("Unloading %s...", modulePath.c_str());

		CryLog("%s", msg.c_str());

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		WIN_HMODULE hModule = moduleIt->second;
#endif

		// CVars should be unregistered earlier than owning objects/modules are destroyed.
		auto CleanupModuleCVars = (void (*)())CryGetProcAddress(hModule, "CleanupModuleCVars");
		if (CleanupModuleCVars)
		{
			CleanupModuleCVars();
		}

		auto GetHeadToRegFactories = (PtrFunc_GetHeadToRegFactories)CryGetProcAddress(hModule, "GetHeadToRegFactories");
		SRegFactoryNode* pFactoryNode = GetHeadToRegFactories();

		if (pFactoryNode)
		{
			ICryFactoryRegistryImpl* const pReg = static_cast<ICryFactoryRegistryImpl*>(GetCryFactoryRegistry());

			pReg->UnregisterFactories(pFactoryNode);
		}

		CryFreeLibrary(hModule);
		m_moduleDLLHandles.erase(moduleIt);

		return true;
	}

	return false;
}

void CSystem::GetLoadedDynamicLibraries(std::vector<string>& moduleNames) const
{
	moduleNames.reserve(m_moduleDLLHandles.size() + 1);

	for (const std::pair<string, WIN_HMODULE>& modulePair : m_moduleDLLHandles)
	{
		moduleNames.emplace_back(modulePair.first);
	}

#ifdef CRY_PLATFORM_WINDOWS
	// Push back the executing module.
	char filename[MAX_PATH];
	::GetModuleFileName(nullptr, filename, MAX_PATH);

	moduleNames.push_back(filename);
#endif
}

ICryFactory* CSystem::LoadModuleWithFactory(const char* dllName, const CryInterfaceID& moduleInterfaceId)
{
	// Start by looking in the current context, in case of static linking
	ICryFactory* pFactory = nullptr;
	size_t numFactories = 1;

	GetCryFactoryRegistry()->IterateFactories(moduleInterfaceId, &pFactory, numFactories);
	if (numFactories == 1 && pFactory != nullptr)
	{
		return pFactory;
	}

	// Attempt to load the DLL
	WIN_HMODULE hModule = LoadDynamicLibrary(dllName, false);

	if (hModule == nullptr)
	{
		return nullptr;
	}

	if (auto getHeadToRegFactories = (PtrFunc_GetHeadToRegFactories)CryGetProcAddress(hModule, "GetHeadToRegFactories"))
	{
		SRegFactoryNode* pFactoryNode = getHeadToRegFactories();

		while (pFactoryNode != nullptr)
		{
			if (pFactoryNode->m_pFactory->ClassSupports(moduleInterfaceId))
			{
				return pFactoryNode->m_pFactory;
			}

			pFactoryNode = pFactoryNode->m_pNext;
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitializeEngineModule(const SSystemInitParams& startupParams, const char* dllName, const CryInterfaceID& moduleInterfaceId, bool bQuitIfNotFound)
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Engine Module");
	MEMSTAT_CONTEXT(EMemStatContextType::Other, dllName);
	IMemoryManager::SProcessMemInfo memStart, memEnd;
	if (GetIMemoryManager())
		GetIMemoryManager()->GetProcessMemInfo(memStart);
	else
		ZeroStruct(memStart);

	bool loadedLibrary = false;

	std::shared_ptr<Cry::IDefaultModule> pModule;
	if (!CryCreateClassInstanceForInterface(moduleInterfaceId, pModule))
	{
		if (LoadDynamicLibrary(dllName, bQuitIfNotFound, true) == 0)
			return false;

		loadedLibrary = true;
		CryCreateClassInstanceForInterface(moduleInterfaceId, pModule);
	}

	if (pModule == nullptr)
	{
		return false;
	}

	MEMSTAT_CONTEXT_FMT(EMemStatContextType::Other, "Initialize module: %s", pModule->GetFactory()->GetName());
	if (!pModule->Initialize(m_env, startupParams))
	{
		if (loadedLibrary)
		{
			pModule.reset();
			UnloadDynamicLibrary(dllName);
		}

		return false;
	}

	if (GetIMemoryManager())
	{
		GetIMemoryManager()->GetProcessMemInfo(memEnd);

#if !defined(EXCLUDE_NORMAL_LOG)
		uint64 memUsed = memEnd.WorkingSetSize - memStart.WorkingSetSize;
		CryLog("Initializing %s done, MemUsage=%uKb", dllName, uint32(memUsed / 1024));
#endif
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::UnloadEngineModule(const char* szDllName)
{
	return UnloadDynamicLibrary(szDllName);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::OpenRenderLibrary(const SSystemInitParams& startupParams, const char* t_rend)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (gEnv->IsDedicated())
		return true;

	if (stricmp(t_rend, STR_DX11_RENDERER) == 0)
		return OpenRenderLibrary(startupParams, R_DX11_RENDERER);
	else if (stricmp(t_rend, STR_DX12_RENDERER) == 0)
		return OpenRenderLibrary(startupParams, R_DX12_RENDERER);
	else if (stricmp(t_rend, STR_GL_RENDERER) == 0)
		return OpenRenderLibrary(startupParams, R_GL_RENDERER);
	else if (stricmp(t_rend, STR_VK_RENDERER) == 0)
		return OpenRenderLibrary(startupParams, R_VK_RENDERER);
	else if (stricmp(t_rend, STR_GNM_RENDERER) == 0)
		return OpenRenderLibrary(startupParams, R_GNM_RENDERER);

	CryFatalError("Unknown renderer type: %s", t_rend);
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::CloseRenderLibrary(const char* t_rend)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (gEnv->IsDedicated())
		return true;

	if (stricmp(t_rend, STR_DX11_RENDERER) == 0)
		return UnloadEngineModule(DLL_RENDERER_DX11);
	else if (stricmp(t_rend, STR_DX12_RENDERER) == 0)
		return UnloadEngineModule(DLL_RENDERER_DX12);
	else if (stricmp(t_rend, STR_GL_RENDERER) == 0)
		return UnloadEngineModule(DLL_RENDERER_GL);
	else if (stricmp(t_rend, STR_VK_RENDERER) == 0)
		return UnloadEngineModule(DLL_RENDERER_VK);
	else if (stricmp(t_rend, STR_GNM_RENDERER) == 0)
		return UnloadEngineModule(DLL_RENDERER_GNM);

	CryFatalError("Unknown renderer type: %s", t_rend);
	return false;
}

//////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

#if CRY_PLATFORM_WINDOWS
static wstring GetErrorStringUnsupportedGPU(const char* gpuName, unsigned int gpuVendorId, unsigned int gpuDeviceId)
{

	static const wchar_t s_EN[] = L"Unsupported Graphics Card detected.\n A GPU with support for D3D FeatureLevel 11.0 is required.";
	static const wchar_t s_FR[] = { 0 };
	static const wchar_t s_RU[] = { 0 };
	static const wchar_t s_ES[] = { 0 };
	static const wchar_t s_DE[] = { 0 };
	static const wchar_t s_IT[] = { 0 };

	const size_t fullLangID = (size_t) GetKeyboardLayout(0);
	const size_t primLangID = fullLangID & 0x3FF;
	const wchar_t* pFmt = s_EN;

	/*switch (primLangID)
	   {
	   case 0x07: // German
	   pFmt = s_DE;
	   break;
	   case 0x0a: // Spanish
	   pFmt = s_ES;
	   break;
	   case 0x0c: // French
	   pFmt = s_FR;
	   break;
	   case 0x10: // Italian
	   pFmt = s_IT;
	   break;
	   case 0x19: // Russian
	   pFmt = s_RU;
	   break;
	   case 0x09: // English
	   default:
	   break;
	   }*/
	return wstring(pFmt);
}
#endif

bool CSystem::OpenRenderLibrary(const SSystemInitParams& startupParams, int type)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Render Library");

	if (gEnv->IsDedicated())
		return true;

#if !defined(DEDICATED_SERVER)
	#if CRY_PLATFORM_WINDOWS
	if (!gEnv->IsDedicated())
	{
		unsigned int gpuVendorId = 0, gpuDeviceId = 0, totVidMem = 0;
		char gpuName[256];
		Win32SysInspect::DXFeatureLevel featureLevel = Win32SysInspect::DXFL_Undefined;
		Win32SysInspect::GetGPUInfo(gpuName, sizeof(gpuName), gpuVendorId, gpuDeviceId, totVidMem, featureLevel);

		if (featureLevel < Win32SysInspect::DXFL_11_0)
		{
			const char logMsgFmt[] ("Unsupported GPU configuration!\n- %s (vendor = 0x%.4x, device = 0x%.4x)\n- Dedicated video memory: %d MB\n- Feature level: %s\n");
			CryLogAlways(logMsgFmt, gpuName, gpuVendorId, gpuDeviceId, totVidMem >> 20, GetFeatureLevelAsString(featureLevel));

		#if !defined(_RELEASE)
			if (m_env.pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "anygpu") == NULL)  // Useful for shader cache generation
		#endif
			{
				CryMessageBox(GetErrorStringUnsupportedGPU(gpuName, gpuVendorId, gpuDeviceId).c_str(), L"CRYENGINE", eMB_Error);
				return false;
			}
		}
	}
	#endif
#endif   // !defined(DEDICATED_SERVER)

	const char* libname = "";
	if (type == R_DX11_RENDERER)
		libname = DLL_RENDERER_DX11;
	else if (type == R_DX12_RENDERER)
		libname = DLL_RENDERER_DX12;
	else if (type == R_GL_RENDERER)
		libname = DLL_RENDERER_GL;
	else if (type == R_VK_RENDERER)
		libname = DLL_RENDERER_VK;
	else if (type == R_GNM_RENDERER)
		libname = DLL_RENDERER_GNM;
	else
	{
		CryFatalError("No renderer specified!");
		return false;
	}

	MEMSTAT_CONTEXT_FMT(EMemStatContextType::Other, "Init %s", libname);
	if (!InitializeEngineModule(startupParams, libname, cryiidof<IRendererEngineModule>(), true))
	{
		return false;
	}

	if (!m_env.pRenderer)
	{
		CryFatalError("Error creating Render System!");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitNetwork(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Network");

	if (!InitializeEngineModule(startupParams, DLL_NETWORK, cryiidof<INetworkEngineModule>(), true))
		return false;

	if (m_env.pNetwork == NULL)
	{
		CryFatalError("Error creating Network System!");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitReflectionSystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (!InitializeEngineModule(startupParams, "CryReflection", cryiidof<Cry::Reflection::IModule>(), true))
		return false;

	if (!m_env.pReflection)
	{
		CryFatalError("Error initializing Reflection!");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitSchematyc(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (sys_SchematycPlugin == 0 || sys_SchematycPlugin == 1)
	{
		if (!InitializeEngineModule(startupParams, "CrySchematyc2", cryiidof<Schematyc2::IFramework>(), true))
			return false;

		if (m_env.pSchematyc2 == nullptr)
		{
			CryFatalError("Error initializing Schematyc!");
			return false;
		}
	}

	if (sys_SchematycPlugin == 0 || sys_SchematycPlugin == 2)
	{
		if (!InitializeEngineModule(startupParams, "CrySchematyc", cryiidof<ICrySchematycCore>(), true))
			return false;

		if (m_env.pSchematyc == nullptr)
		{
			CryFatalError("Error initializing experimental Schematyc!");
			return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitEntitySystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Entity System");

	if (!InitializeEngineModule(startupParams, DLL_ENTITYSYSTEM, cryiidof<IEntitySystemEngineModule>(), true))
		return false;

	if (!m_env.pEntitySystem)
	{
		CryFatalError("Error creating Entity System!");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitDynamicResponseSystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Dynamic Responese System");

	const char* sDLLName = m_sys_dll_response_system->GetString();

	if (!sDLLName || sDLLName[0] == '\0')
	{
		return false;
	}
	else
	{
		InitializeEngineModule(startupParams, sDLLName, cryiidof<DRS::IDynamicResponseSystemEngineModule>(), false);
	}

	if (!m_env.pDynamicResponseSystem)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Error creating Dynamic Response System from dll: '%s'!", sDLLName);
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitLiveCreate(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Live Create");
	bool bSkip = startupParams.bSkipLiveCreate;

#ifdef NO_LIVECREATE
	bSkip = true;
#endif

	// 0 means use Null implementation
	if (g_cvars.sys_livecreate == 0)
	{
		bSkip = true;
	}

	if (!bSkip)
	{
		// load initialize the module, this will create and setup the LiveCreate interfaces in the m_env.
		if (!InitializeEngineModule(startupParams, DLL_LIVECREATE, cryiidof<ILiveCreateEngineModule>(), false))
			return false;

		// initialize the new host interface
		if (NULL != m_env.pLiveCreateHost)
		{
			// start listening
			m_env.pLiveCreateHost->Initialize();
		}
	}

	// fallback: when host interface was not created use NULL implementation
	if (NULL == m_env.pLiveCreateHost)
	{
		CryLog("LiveCreateHost not created, using NULL implementation.");
		m_env.pLiveCreateHost = new LiveCreate::CNullHost();
	}

	// fallback: when host interface was not created use NULL implementation
	if (NULL == m_env.pLiveCreateHost)
	{
		CryLog("LiveCreateManager not created, using NULL implementation.");
		m_env.pLiveCreateManager = new LiveCreate::CNullManagerEx();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitMonoBridge(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init C#");

	if (!InitializeEngineModule(startupParams, DLL_MONO_BRIDGE, cryiidof<IMonoEngineModule>(), false))
	{
		gEnv->pLog->LogWarning("MonoRuntime not created.");
		m_env.pMonoRuntime = nullptr;
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitUDR(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (!InitializeEngineModule(startupParams, DLL_UDR, cryiidof<Cry::UDR::IUDREngineModule>(), false))
	{
		gEnv->pLog->LogWarning("UDR Module could not be created.");
		return false;
	}

	const bool initializedSuccessfully = m_env.pUDR->Initialize();
	if (!initializedSuccessfully)
	{
		CRY_ASSERT_MESSAGE(initializedSuccessfully, "UDR System could not be initialized.");
		gEnv->pLog->LogWarning("UDR System could not be initialized.");
		return false;
		
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitGameFramework(SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Game Framework");

	if (!InitializeEngineModule(startupParams, "CryAction", cryiidof<IGameFrameworkEngineModule>(), false))
	{
		gEnv->pLog->LogWarning("Game Framework not created, this is currently unsupported!");
		m_env.pGameFramework = nullptr;
		return;
	}

#if !defined(CRY_IS_MONOLITHIC_BUILD)
	m_gameLibrary.Free();
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitInput(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Input");

	if (startupParams.bSkipInput)
	{
		m_env.pInput = new CNullInput();
		return true;
	}

	if (!InitializeEngineModule(startupParams, DLL_INPUT, cryiidof<IInputEngineModule>(), true))
	{
#if CRY_PLATFORM_WINDOWS
		CryMessageBox("CryInput.dll could not be loaded. This is likely due to not having XInput support installed.\nPlease install the most recent version of the DirectX runtime.", "ERROR: CryInput.dll could not be loaded!", eMB_Error);
#endif
		return false;
	}

	if (!m_env.pInput)
	{
		CryFatalError("Error creating Input System!");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// attaches the given variable to the given container;
// recreates the variable if necessary
ICVar* CSystem::attachVariable(const char* szVarName, int* pContainer, const char* szComment, int dwFlags)
{
	IConsole* pConsole = GetIConsole();

	ICVar* pOldVar = pConsole->GetCVar(szVarName);
	int nDefault;
	if (pOldVar)
	{
		nDefault = pOldVar->GetIVal();
		pConsole->UnregisterVariable(szVarName, true);
	}

	// NOTE: maybe we should preserve the actual value of the variable across the registration,
	// because of the strange architecture of IConsole that converts int->float->int

	REGISTER_CVAR2(szVarName, pContainer, *pContainer, dwFlags, szComment);

	ICVar* pVar = pConsole->GetCVar(szVarName);

#ifdef _DEBUG
	// test if the variable really has this container
	assert(*pContainer == pVar->GetIVal());
	++*pContainer;
	assert(*pContainer == pVar->GetIVal());
	--*pContainer;
#endif

	if (pOldVar)
	{
		// carry on the default value from the old variable anyway
		pVar->Set(nDefault);
	}
	return pVar;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitRenderer(SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Renderer");

	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress("Initializing Renderer...");

	if (m_env.IsEditor())
	{
		m_env.pConsole->GetCVar("r_Width");

		// save current screen width/height/bpp, so they can be restored on shutdown
		m_iWidth = m_env.pConsole->GetCVar("r_Width")->GetIVal();
		m_iHeight = m_env.pConsole->GetCVar("r_Height")->GetIVal();
		m_iColorBits = m_env.pConsole->GetCVar("r_ColorBits")->GetIVal();
	}

	if (!OpenRenderLibrary(startupParams, m_rDriver->GetString()))
		return false;

	if (m_env.pRenderer)
	{
#ifndef RELEASE
		const CRY_HWND hwnd = (startupParams.bEditor) ? (CRY_HWND)1 : m_hWnd;
#else
		const CRY_HWND hwnd = m_hWnd;
#endif

		int width = m_rWidth->GetIVal();
		int height = m_rHeight->GetIVal();
		if (gEnv->IsEditor())
		{
			// In Editor base default Display Context is not really used, so it is allocated with the minimal resolution.
			width = 32;
			height = 32;
		}

		m_hWnd = m_env.pRenderer->Init(
			0, 0, width, height,
			m_rColorBits->GetIVal(), m_rDepthBits->GetIVal(), m_rStencilBits->GetIVal(),
			hwnd, false, startupParams.bShaderCacheGen);

		startupParams.hWnd = m_hWnd;

		m_env.pAuxGeomRenderer = m_env.pRenderer->GetIRenderAuxGeom();
		InitPhysicsRenderer(startupParams);

		if (m_env.pHardwareMouse)
			m_env.pHardwareMouse->OnPostInitRenderer();

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE || CRY_PLATFORM_ORBIS
		return true;
#else
		return (startupParams.bUnattendedMode || startupParams.bShaderCacheGen || m_hWnd != 0);
#endif
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
char* PhysHelpersToStr(int iHelpers, char* strHelpers)
{
	char* ptr = strHelpers;
	if (iHelpers & 128) *ptr++ = 't';
	if (iHelpers & 256) *ptr++ = 's';
	if (iHelpers & 512) *ptr++ = 'r';
	if (iHelpers & 1024) *ptr++ = 'R';
	if (iHelpers & 2048) *ptr++ = 'l';
	if (iHelpers & 4096) *ptr++ = 'i';
	if (iHelpers & 8192) *ptr++ = 'e';
	if (iHelpers & 16384) *ptr++ = 'g';
	if (iHelpers & 32768) *ptr++ = 'w';
	if (iHelpers & 32) *ptr++ = 'a';
	if (iHelpers & 64) *ptr++ = 'y';
	*ptr++ = iHelpers ? '_' : '0';
	if (iHelpers & 1) *ptr++ = 'c';
	if (iHelpers & 2) *ptr++ = 'g';
	if (iHelpers & 4) *ptr++ = 'b';
	if (iHelpers & 8) *ptr++ = 'l';
	if (iHelpers & 16) *ptr++ = 'j';
	if (iHelpers >> 16)
		if (!(iHelpers & 1 << 27))
			ptr += sprintf(ptr, "t(%d)", iHelpers >> 16);
		else
			for (int i = 0; i < 16; i++)
				if (i != 11 && iHelpers & 1 << (16 + i))
					ptr += sprintf(ptr, "f(%d)", i);
	*ptr++ = 0;
	return strHelpers;
}

int StrToPhysHelpers(const char* strHelpers)
{
	const char* ptr;
	int iHelpers = 0, level = 0;
	if (*strHelpers == '1')
		return 7970;
	if (*strHelpers == '2')
		return 7970 | 1 << 31 | 1 << 27;
	for (ptr = strHelpers; *ptr && *ptr != '_'; ptr++)
		switch (*ptr)
		{
		case 't':
			iHelpers |= 128;
			break;
		case 's':
			iHelpers |= 256;
			break;
		case 'r':
			iHelpers |= 512;
			break;
		case 'R':
			iHelpers |= 1024;
			break;
		case 'l':
			iHelpers |= 2048;
			break;
		case 'i':
			iHelpers |= 4096;
			break;
		case 'e':
			iHelpers |= 8192;
			break;
		case 'g':
			iHelpers |= 16384;
			break;
		case 'w':
			iHelpers |= 32768;
			break;
		case 'a':
			iHelpers |= 32;
			break;
		case 'y':
			iHelpers |= 64;
			break;
		}
	if (*ptr == '_') ptr++;
	for (; *ptr; ptr++)
		switch (*ptr)
		{
		case 'c':
			iHelpers |= 1;
			break;
		case 'g':
			iHelpers |= 2;
			break;
		case 'b':
			iHelpers |= 4;
			break;
		case 'l':
			iHelpers |= 8;
			break;
		case 'j':
			iHelpers |= 16;
			break;
		case 'f':
			if (*++ptr && *++ptr)
				for (level = 0; *(ptr + 1) && *ptr != ')'; ptr++)
					level = level * 10 + *ptr - '0';
			iHelpers |= 1 << (16 + level) | 1 << 27;
			break;
		case 't':
			if (*++ptr && *++ptr)
				for (level = 0; *(ptr + 1) && *ptr != ')'; ptr++)
					level = level * 10 + *ptr - '0';
			iHelpers |= level << 16 | 2;
		}
	return iHelpers;
}

void OnDrawHelpersStrChange(ICVar* pVar)
{
	gEnv->pPhysicalWorld->GetPhysVars()->iDrawHelpers = StrToPhysHelpers(pVar->GetString());
}

bool CSystem::InitPhysics(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Physics, "Init Physics");

#if defined(_LIB) && CRY_PLATFORM_DURANGO
	m_env.pPhysicalWorld = CreatePhysicalWorld(this);
#else
	// Check m_pPhysicsLibrary - if not specified, load CryPhysics, if specified, load that one
	const char* physDLL = m_pPhysicsLibrary ? m_pPhysicsLibrary->GetString() : DLL_PHYSICS;
	if (!InitializeEngineModule(startupParams, physDLL, cryiidof<IPhysicsEngineModule>(), true))
	{
		CryFatalError("Error loading physics dll: %s", physDLL);
		return false;
	}
#endif

	if (!m_env.pPhysicalWorld)
	{
		CryFatalError("Error creating Physics System!");
		return false;
	}
	//m_env.pPhysicalWorld->Init();	// don't need a second Init, the world is created initialized
	if (!m_env.IsDedicated())
		m_env.pPhysicalWorld->GetPhysVars()->bMultithreaded = 1;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitPhysicsRenderer(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Physics, "Init Physics Renderer");
	//////////////////////////////////////////////////////////////////////////
	// Physics Renderer (for debug helpers)
	//////////////////////////////////////////////////////////////////////////
	if (!m_bUIFrameworkMode && !startupParams.bShaderCacheGen)
	{
		m_pPhysRenderer = new CPhysRenderer;
		m_pPhysRenderer->Init(); // needs to be created after physics and renderer
		m_p_draw_helpers_str = REGISTER_STRING_CB("p_draw_helpers", "0", VF_CHEAT,
		                                          "Same as p_draw_helpers_num, but encoded in letters\n"
		                                          "Usage [Entity_Types]_[Helper_Types] - [t|s|r|R|l|i|g|a|y|e]_[g|c|b|l|t(#)]\n"
		                                          "Entity Types:\n"
		                                          "t - show terrain\n"
		                                          "s - show static entities\n"
		                                          "r - show sleeping rigid bodies\n"
		                                          "R - show active rigid bodies\n"
		                                          "l - show living entities\n"
		                                          "i - show independent entities\n"
		                                          "g - show triggers\n"
		                                          "a - show areas\n"
		                                          "y - show rays in RayWorldIntersection\n"
		                                          "e - show explosion occlusion maps\n"
		                                          "Helper Types\n"
		                                          "g - show geometry\n"
		                                          "c - show contact points\n"
		                                          "b - show bounding boxes\n"
		                                          "l - show tetrahedra lattices for breakable objects\n"
		                                          "j - show structural joints (will force translucency on the main geometry)\n"
		                                          "t(#) - show bounding volume trees up to the level #\n"
		                                          "f(#) - only show geometries with this bit flag set (multiple f\'s stack)\n"
		                                          "Example: p_draw_helpers larRis_g - show geometry for static, sleeping, active, independent entities and areas",
		                                          OnDrawHelpersStrChange);

		REGISTER_CVAR2("p_cull_distance", &m_pPhysRenderer->m_cullDist, m_pPhysRenderer->m_cullDist, 0,
		               "Culling distance for physics helpers rendering");
		REGISTER_CVAR2("p_wireframe_distance", &m_pPhysRenderer->m_wireframeDist, m_pPhysRenderer->m_wireframeDist, 0,
		               "Maximum distance at which wireframe is drawn on physics helpers");
		REGISTER_CVAR2("p_meridian_distance", &m_pPhysRenderer->m_meridianDist, m_pPhysRenderer->m_meridianDist, 0,
		               "Maximum distance at which meridians/parallels are drawn on physics helpers for primitives with round parts");
		REGISTER_CVAR2("p_ray_fadeout", &m_pPhysRenderer->m_timeRayFadein, m_pPhysRenderer->m_timeRayFadein, 0,
		               "Fade-out time for ray physics helpers");
		REGISTER_CVAR2("p_ray_peak_time", &m_pPhysRenderer->m_rayPeakTime, m_pPhysRenderer->m_rayPeakTime, 0,
		               "Rays that take longer then this (in ms) will use different color");
		REGISTER_CVAR2("p_proxy_highlight_threshold", &m_pPhysRenderer->m_maxTris, m_pPhysRenderer->m_maxTris, 0,
		               "Physics proxies with triangle counts large than this will be highlighted");
		REGISTER_CVAR2("p_proxy_highlight_range", &m_pPhysRenderer->m_maxTrisRange, m_pPhysRenderer->m_maxTrisRange, 0,
		               "Physics proxies with triangle counts >= p_proxy_highlight_threshold+p_proxy_highlight_range will get the maximum highlight");
		REGISTER_CVAR2("p_jump_to_profile_ent", &(m_iJumpToPhysProfileEnt = 0), 0, 0,
		               "Move the local player next to the corresponding entity in the p_profile_entities list");
		GetIConsole()->CreateKeyBind("alt_1", "p_jump_to_profile_ent 1");
		GetIConsole()->CreateKeyBind("alt_2", "p_jump_to_profile_ent 2");
		GetIConsole()->CreateKeyBind("alt_3", "p_jump_to_profile_ent 3");
		GetIConsole()->CreateKeyBind("alt_4", "p_jump_to_profile_ent 4");
		GetIConsole()->CreateKeyBind("alt_5", "p_jump_to_profile_ent 5");
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitMovieSystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Movie System");

	if (!InitializeEngineModule(startupParams, DLL_MOVIE, cryiidof<IMovieEngineModule>(), true))
		return false;

	if (!m_env.pMovieSystem)
	{
		CryFatalError("Error creating Movie System!");
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitAISystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init AISystem ");

	const char* sDLLName = m_sys_dll_ai->GetString();
	if (!InitializeEngineModule(startupParams, sDLLName, cryiidof<IAIEngineModule>(), false))
		return false;

	if (!m_env.pAISystem)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Cannot create AI System!");

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitScriptSystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::LUA, "Init Script System");

	if (!InitializeEngineModule(startupParams, DLL_SCRIPTSYSTEM, cryiidof<IScriptSystemEngineModule>(), true))
		return false;

	if (m_env.pScriptSystem == NULL)
	{
		CryFatalError("Error creating Script System!");
		return (false);
	}

	m_env.pScriptSystem->PostInit();

	// Load script surface types.
	if (m_env.pScriptSystem)
		m_env.pScriptSystem->LoadScriptedSurfaceTypes("Scripts/Materials", false);

	return (true);
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFileSystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::LUA, "Init File System");

	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress("Initializing File System...");

	bool bLvlRes = false;               // true: all assets since executable start are recorded, false otherwise

#if !defined(_RELEASE)
	const ICmdLineArg* pArg = m_pCmdLine->FindArg(eCLAT_Pre, "LvlRes");      // -LvlRes command line option

	if (pArg)
		bLvlRes = true;
#endif // !defined(_RELEASE)

	CCryPak* pCryPak;
	pCryPak = new CCryPak(m_env.pLog, &g_cvars.pakVars, bLvlRes);
	pCryPak->SetGameFolderWritable(m_bGameFolderWritable);
	m_env.pCryPak = pCryPak;

	// Check if root folder is overridden by command-line
	const ICmdLineArg* root = m_pCmdLine->FindArg(eCLAT_Pre, "root");
	if (root)
	{
		string temp = PathUtil::ToUnixPath(PathUtil::AddSlash(root->GetValue()));
		if (pCryPak->MakeDir(temp.c_str()))
			m_root = temp;
	}

	if (m_env.IsEditor() || bLvlRes)
		m_env.pCryPak->RecordFileOpen(ICryPak::RFOM_EngineStartup);

	{
		char szEngineRootDir[_MAX_PATH];
		CryFindEngineRootFolder(CRY_ARRAY_COUNT(szEngineRootDir), szEngineRootDir);
		string engineRootDir = PathUtil::RemoveSlash(szEngineRootDir);
		m_env.pCryPak->SetAlias("%ENGINEROOT%", engineRootDir.c_str(), true);

		const CryPathString engineDir = PathUtil::Make(CryPathString(engineRootDir.c_str()), CryPathString(CRYENGINE_ENGINE_FOLDER));
		m_env.pCryPak->SetAlias("%ENGINE%", engineDir.c_str(), true);

#ifndef RELEASE
		if (m_env.IsEditor())
		{
			const CryPathString editorDir = PathUtil::Make(CryPathString(engineRootDir.c_str()), CryPathString("Editor"));
			m_env.pCryPak->SetAlias("%EDITOR%", editorDir.c_str(), true);

			m_env.pCryPak->SetAlias("%{PLUGIN-GUID}%", editorDir.c_str(), true);
		}
#endif
	}

	// Now set up the log
	InitLog(startupParams);

	LogVersion();

	((CCryPak*)m_env.pCryPak)->SetLog(m_env.pLog);

	ILoadConfigurationEntrySink* pCVarsWhiteListConfigSink = GetCVarsWhiteListConfigSink();
#if CRY_PLATFORM_ANDROID && !defined(ANDROID_OBB)
	string path = string(CryGetProjectStoragePath()) + "/system.cfg";
#else
	string path = "%ENGINEROOT%/system.cfg";
#endif

#if defined(USE_RUNTIME_CVAR_OVERRIDES)
	// We load the runtime CVar overrides before system.cfg
	if (m_env.pConsole != nullptr)
	{
		const bool result = static_cast<CXConsole*>(m_env.pConsole)->ParseCVarOverridesFile(path);
		if (!result)
		{
			CryMessageBox("Error: Failed to parse the runtime CVar overrides file, look for assert failure.", "ERROR", EMessageBox::eMB_Error);
		}
	}
#endif

	// Load value of sys_game_folder from system.cfg into the sys_project console variable
	LoadConfiguration(path.c_str(), pCVarsWhiteListConfigSink, eLoadConfigInit, ELoadConfigurationFlags::SuppressConfigNotFoundWarning);

	const char* szConfigPakPath = "%ENGINEROOT%/config.pak";
	m_env.pCryPak->OpenPack(szConfigPakPath);

	// Initialize console before the project system
	// This ensures that "exec" and other early commands can be executed immediately on parsing
	if (m_env.pConsole != nullptr)
	{
		static_cast<CXConsole*>(m_env.pConsole)->PreProjectSystemInit();
	}

	if (!m_pProjectManager->ParseProjectFile())
	{
		m_env.pCryPak->ClosePack(szConfigPakPath);
		return false;
	}

	m_env.pCryPak->ClosePack(szConfigPakPath);

	// Legacy support for setting decryption key from IGameStartup interface
	// Should be removed when legacy game dll's are gone
	ICVar* pLegacyGameDllCVar = m_env.pConsole->GetCVar("sys_dll_game");
	if (pLegacyGameDllCVar != nullptr)
	{
		HMODULE hGameDll;

#if !defined(CRY_IS_MONOLITHIC_BUILD)
		m_gameLibrary.Set(pLegacyGameDllCVar->GetString());
		hGameDll = m_gameLibrary.m_hModule;
#else
		hGameDll = CryGetCurrentModule();
#endif
		if (hGameDll != nullptr)
		{
			if (IGameStartup::TEntryFunction CreateTempGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(hGameDll, "CreateGameStartup"))
			{
				IGameStartup* pGameStartup = CreateTempGameStartup();

				uint32 keyLen;
				const uint8* pKeyData = pGameStartup->GetRSAKey(&keyLen);
				if (pKeyData && keyLen > 0)
				{
					(static_cast<CCryPak*>(m_env.pCryPak))->SetDecryptionKey(pKeyData, keyLen);
				}
			}
		}
	}

	bool bRes = m_env.pCryPak->Init("");

	if (bRes)
	{
#if !defined(_RELEASE)
		const ICmdLineArg* pakalias = m_pCmdLine->FindArg(eCLAT_Pre, "pakalias");
#else
		const ICmdLineArg* pakalias = NULL;
#endif // !defined(_RELEASE)
		if (pakalias && strlen(pakalias->GetValue()) > 0)
			m_env.pCryPak->ParseAliases(pakalias->GetValue());
	}

	// Create Engine folder mod mapping only for Engine assets
	pCryPak->AddMod("%ENGINEROOT%/" CRYENGINE_ENGINE_FOLDER);

#if CRY_PLATFORM_ANDROID
	pCryPak->AddMod(CryGetProjectStoragePath());
	#if defined(ANDROID_OBB)
	pCryPak->SetAssetManager(androidGetAssetManager());
	#endif
#elif CRY_PLATFORM_LINUX
	//apparently Linux needs the parent dir as a module for letting CryPak find the file system.cfg
	pCryPak->AddMod("./");
#endif

	return (bRes);
}

void CSystem::InitLog(const SSystemInitParams& startupParams)
{
	MEMSTAT_CONTEXT(EMemStatContextType::LUA, "Init Log");
	if (startupParams.pLog == nullptr)
	{
		m_env.pLog = new CLog(this);
		if (startupParams.pLogCallback)
		{
			m_env.pLog->AddCallback(startupParams.pLogCallback);
		}

		string sLogFileName = startupParams.sLogFileName != nullptr ? startupParams.sLogFileName : DEFAULT_LOG_FILENAME;
		if (!sLogFileName.empty())
		{
			const int instance = GetApplicationInstance();
			if (instance > 0)
			{
				string logFileExtension;
				string logFileNamePrefix = sLogFileName;
				const size_t extensionIndex = sLogFileName.find_last_of('.');
				if (extensionIndex != string::npos)
				{
					logFileExtension = sLogFileName.substr(extensionIndex, sLogFileName.length() - extensionIndex);
					logFileNamePrefix = sLogFileName.substr(0, extensionIndex);
				}
				sLogFileName.Format("%s(%d)%s", logFileNamePrefix.c_str(), instance, logFileExtension.c_str());
			}
		}

		const ICmdLineArg* logfile = m_pCmdLine->FindArg(eCLAT_Pre, "logfile");
		if (logfile && strlen(logfile->GetValue()) > 0)
		{
			sLogFileName = logfile->GetValue();
		}

		if (sLogFileName.size() > 0)
		{
			if (m_env.pLog->SetFileName(sLogFileName.c_str()))
			{
#ifdef CRY_USE_CRASHRPT
				CCrashRpt::ReInstallCrashRptHandler(0);
#endif
			}
		}
	}
	else
	{
		m_env.pLog = startupParams.pLog;
	}
}

void CSystem::LoadPatchPaks()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	const char* pPatchPakMountPath = "";
	uint32 nFlags = 0;
	nFlags |= ICryPak::FLAGS_NEVER_IN_PAK;
	nFlags |= ICryPak::FLAGS_PATH_REAL;
	nFlags |= ICryArchive::FLAGS_OVERRIDE_PAK;
#endif // #if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO

	// For patching open the special patch pak with FLAGS_OVERRIDE_PAK so it beats all other paks

#if CRY_PLATFORM_WINDOWS
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch1.pak", nFlags);
#endif // #if CRY_PLATFORM_WINDOWS

#if CRY_PLATFORM_DURANGO
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch1_launch.pak", nFlags);
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch2_launch.pak", nFlags);
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch3_launch.pak", nFlags);
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch1.pak", nFlags);
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch2.pak", nFlags);
	m_env.pCryPak->OpenPack(pPatchPakMountPath, "patch/patch3.pak", nFlags);
#endif // #if CRY_PLATFORM_DURANGO
}
/////////////////////////////////////////////////////////////////////////////////

bool CSystem::InitFileSystem_LoadEngineFolders()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Engine Folders");
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (g_cvars.pakVars.nPriority == ePakPriorityPakOnly)
	{
		OpenBasicPaks(false);  //we need to open then engine.pak, since we only allow data from pak files
	}

#if CRY_PLATFORM_ANDROID && defined(ANDROID_OBB)
	{
		uint32 nFlags = ICryPak::FLAGS_NEVER_IN_PAK;
		const char* szMainExpName = androidGetMainExpName();
		m_env.pCryPak->AddMod(androidGetExpFilePath());

		const char* szAssetName = androidGetAssetFileName();
		if (szAssetName && szAssetName[0])
		{
			bool r = m_env.pCryPak->OpenPack("", szAssetName, nFlags | ICryPak::FLAGS_NO_FULL_PATH);
			if (r)
			{
				CryLog("Asset from APK package loaded");
			}
			else
			{
				CryLog("Asset not found in APK package.");
			}
		}

		/// Open main expansion file if it exists.
		if (szMainExpName && szMainExpName[0])
		{
			bool r = m_env.pCryPak->OpenPack("", szMainExpName, nFlags);
			if (r)
			{
				CryLog("Main expansion file %s loaded", szMainExpName);
			}
			else
			{
				CryLog("Main expansion file %s load failed", szMainExpName);
			}
		}

		/// Open patch expansion file if it exists.
		const char* szPatchExpName = androidGetPatchExpName();
		if (szPatchExpName && szPatchExpName[0])
		{
			bool r = m_env.pCryPak->OpenPack("", szPatchExpName, nFlags);
			if (r)
			{
				CryLog("Patch expansion file %s loaded", szPatchExpName);
			}
		}
	}
#endif

	// We set now the correct "game" folder to use in Pak File
	ICVar* pGameFolderCVar = gEnv->pConsole->GetCVar("sys_game_folder");
	CRY_ASSERT(pGameFolderCVar != nullptr);

	m_env.pCryPak->SetGameFolder(pGameFolderCVar->GetString());

	if (g_cvars.sys_build_folder->GetString() != nullptr && g_cvars.sys_build_folder->GetString()[0] != '\0')
	{
		m_env.pCryPak->AddMod(PathUtil::AddSlash(g_cvars.sys_build_folder->GetString()) + m_pProjectManager->GetCurrentAssetDirectoryRelative());
	}

	m_pProjectManager->MigrateFromLegacyWorkflowIfNecessary();

	// Load engine folders.
	ChangeUserPath(m_sys_user_folder->GetString());

#if !defined(_RELEASE)
	if (const ICmdLineArg* pModArg = GetICmdLine()->FindArg(eCLAT_Pre, "MOD"))
	{
		if (IsMODValid(pModArg->GetValue()))
		{
			string modPath;
			modPath.append("Mods\\");
			modPath.append(pModArg->GetValue());
			modPath.append("\\");
			modPath.append(m_env.pCryPak->GetGameFolder());

			m_env.pCryPak->AddMod(modPath.c_str());
		}
	}
#endif // !defined(_RELEASE)

	InitResourceCacheFolder();

	// simply open all paks if fast load pak can't be found
	if (!g_cvars.sys_intromoviesduringinit || !m_pResourceManager->LoadFastLoadPaks(true))
	{
		OpenBasicPaks(true);
	}

	// Load cvar groups first from game folder then from engine folder.
	{
		string gameFolder = (!PathUtil::GetGameFolder().empty()) ? (PathUtil::GetGameFolder() + "/") : string("");
		AddCVarGroupDirectory(gameFolder + "Config/CVarGroups");
	}
	AddCVarGroupDirectory("%ENGINE%/Config/CVarGroups");

#if defined(USE_PATCH_PAK)
	LoadPatchPaks();
#endif
	return (true);
}

/////////////////////////////////////////////////////////////////////////////////
void CSystem::InitResourceCacheFolder()
{
	// Resource Cache folder is not enabled in the release configuration
#if !defined(_RELEASE)
	const char* szResourceCacheFolder = m_sys_resource_cache_folder->GetString();

	if (0 == strlen(szResourceCacheFolder))
		return;

	CryPathString cacheFolder(szResourceCacheFolder);
	//////////////////////////////////////////////////////////////////////////
	// Open Paks from Engine folder
	//////////////////////////////////////////////////////////////////////////
	// After game paks to have same search order as with files on disk
	{
		CryPathString cacheFolderParentFolder;
		auto slashPos = cacheFolder.rfind('/');
		if (slashPos != string::npos)
		{
			cacheFolderParentFolder = cacheFolder.substr(0, slashPos);
		}
		if (!cacheFolderParentFolder.empty())
		{
			const char* szBindRoot = m_env.pCryPak->GetAlias("%ENGINE%", false);
			CryPathString paksFolder = cacheFolderParentFolder + "/Engine/*.pak";
			// Will open engine specific paks in the parent of the resource ccache folder /engine folder.
			m_env.pCryPak->OpenPacks(szBindRoot, paksFolder.c_str());
		}
	}

	// Resource cache folder is used to store locally compiled resources (or precompiled asset cache folder).
	m_env.pCryPak->AddMod(szResourceCacheFolder, ICryPak::EModAccessPriority::AfterSource);

#endif // !defined(_RELEASE)
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitStreamEngine()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Stream Engine");

	if (m_pUserCallback)
		m_pUserCallback->OnInitProgress("Initializing Stream Engine...");

	m_pStreamEngine = new CStreamEngine();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFont(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Font");

	if (!InitializeEngineModule(startupParams, DLL_FONT, cryiidof<IFontEngineModule>(), true))
		return false;

	if (!m_env.pCryFont)
	{
		CryFatalError("Error creating Font System!");
		return false;
	}

	if (gEnv->IsDedicated())
		return true;

	// Load the default font
	m_pIFont = m_env.pCryFont->NewFont("default");
	if (!m_pIFont)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Error creating the default fonts");
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* szFontPath = "%engine%/Fonts/default.xml";

	if (!m_pIFont->Load(szFontPath))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Error loading the default font from '%s'. You're probably running the executable from the wrong working folder.", szFontPath);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::Init3DEngine(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init 3D Engine");

	if (!InitializeEngineModule(startupParams, DLL_3DENGINE, cryiidof<I3DEngineModule>(), true))
		return false;

	if (!m_env.p3DEngine)
	{
		CryFatalError("Error creating 3D Engine!");
		return false;
	}

	if (!m_env.p3DEngine->Init())
	{
		CryFatalError("Error initializing 3D Engine!");
		return false;
	}
	m_pProcess = m_env.p3DEngine;
	m_pProcess->SetFlags(PROC_3DENGINE);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitAnimationSystem(const SSystemInitParams& startupParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Animation System");

	if (!InitializeEngineModule(startupParams, DLL_ANIMATION, cryiidof<IAnimationEngineModule>(), true))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitLocalization()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Open Localization Pak");

	// Set the localization folder
	ICVar* pCVar = (m_env.pConsole != nullptr) ? m_env.pConsole->GetCVar("sys_localization_folder") : nullptr;

	if (pCVar != nullptr)
	{
		static_cast<CCryPak* const>(m_env.pCryPak)->SetLocalizationFolder(g_cvars.sys_localization_folder->GetString());
	}

	stack_string szLanguage = CRYENGINE_DEFAULT_LOCALIZATION_LANG;
	pCVar = (m_env.pConsole != nullptr) ? m_env.pConsole->GetCVar("g_language") : nullptr;

	if (pCVar != nullptr)
	{
		if (strlen(pCVar->GetString()) == 0)
		{
			// Set "g_language" to default language if it has not been set.
			pCVar->Set(szLanguage.c_str());
		}
		else
		{
			szLanguage = pCVar->GetString();
		}
	}

	OpenLanguagePak(szLanguage.c_str());

	stack_string szLanguageAudio = szLanguage;
	pCVar = (m_env.pConsole != nullptr) ? m_env.pConsole->GetCVar("g_languageAudio") : nullptr;

	if (pCVar != nullptr)
	{
		if (strlen(pCVar->GetString()) == 0)
		{
			// Use "g_language" value in case "g_languageAudio" has not been set.
			pCVar->Set(szLanguageAudio.c_str());
		}
		else
		{
			szLanguageAudio = pCVar->GetString();
		}
	}

	OpenLanguageAudioPak(szLanguageAudio.c_str());

	// Construct the localization manager after the paks have been opened.
	if (m_pLocalizationManager == nullptr)
	{
		m_pLocalizationManager = new CLocalizedStringsManager(this);
	}

	GetLocalizationManager()->SetLanguage(szLanguage.c_str());
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenBasicPaks(bool bLoadGamePaks)
{
	static bool s_bEnginePakLoaded = false;
	static bool s_bGamePaksLoaded = false;

	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Open Pak Files");

	string buildFolder = PathUtil::AddSlash(g_cvars.sys_build_folder->GetString());

	// open pak files
	if (bLoadGamePaks && !s_bGamePaksLoaded)
	{
		string paksFolder = buildFolder + string(PathUtil::GetGameFolder()) + "/*.pak";
		m_env.pCryPak->OpenPacks(PathUtil::GetGameFolder(), paksFolder.c_str());
		InlineInitializationProcessing("CSystem::OpenBasicPaks OpenPacks( paksFolder.c_str() )");
		s_bGamePaksLoaded = true;
	}

	if (!s_bEnginePakLoaded)
	{
		//////////////////////////////////////////////////////////////////////////
		// Open Paks from Engine folder
		//////////////////////////////////////////////////////////////////////////
		// After game paks to have same search order as with files on disk
		{
			const char* szBindRoot = m_env.pCryPak->GetAlias("%ENGINE%", false);
			string paksFolder = PathUtil::Make(buildFolder.empty() ? string("%ENGINEROOT%") : buildFolder, "Engine");

			const unsigned int numOpenPacksBeforeEngine = m_env.pCryPak->GetPakInfo()->numOpenPaks;
			m_env.pCryPak->OpenPacks(szBindRoot, PathUtil::Make(paksFolder, "*.pak"));

			if (g_cvars.pakVars.nPriority == ePakPriorityPakOnly && numOpenPacksBeforeEngine == m_env.pCryPak->GetPakInfo()->numOpenPaks)
			{
				CryFatalError("Engine initialization failed: Engine assets are required to be in pak files and cannot be read from the directory structure");
			}
		}

		InlineInitializationProcessing("CSystem::OpenBasicPaks OpenPacks( Engine... )");

		//////////////////////////////////////////////////////////////////////////
		// Open paks in MOD subfolders.
		//////////////////////////////////////////////////////////////////////////
#if !defined(_RELEASE)
		if (const ICmdLineArg* pModArg = GetICmdLine()->FindArg(eCLAT_Pre, "MOD"))
		{
			if (IsMODValid(pModArg->GetValue()))
			{
				string modFolder = "Mods\\";
				modFolder += pModArg->GetValue();
				modFolder += "\\";
				modFolder += PathUtil::GetGameFolder();

				string paksModFolder = modFolder;
				paksModFolder += "\\*.pak";
				GetIPak()->OpenPacks(PathUtil::GetGameFolder(), paksModFolder.c_str(), ICryPak::FLAGS_PATH_REAL | ICryArchive::FLAGS_OVERRIDE_PAK);
			}
		}
#endif // !defined(_RELEASE)

		// Load paks required for game init to mem
		gEnv->pCryPak->LoadPakToMemory("engine.pak", ICryPak::eInMemoryPakLocale_GPU);
		s_bEnginePakLoaded = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenLanguagePak(char const* const szLanguage)
{
	// Load xml pak with full filenames to perform wildcard searches.
	string localizedPath;
	GetLocalizedPath(szLanguage, localizedPath);

	string const bindingRoot(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR);
	string pakFilePath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + localizedPath);

	if (g_cvars.sys_build_folder->GetString() != nullptr && g_cvars.sys_build_folder->GetString()[0] != '\0')
	{
		pakFilePath = PathUtil::AddSlash(g_cvars.sys_build_folder->GetString()) + string(PathUtil::GetGameFolder()) + CRY_NATIVE_PATH_SEPSTR + localizedPath;
	}

	if (!m_env.pCryPak->OpenPack(bindingRoot.c_str(), pakFilePath))
	{
		// make sure the localized language is found - not really necessary, for TC
		CryLogAlways("Localized language content(%s) not available or modified from the original installation.", szLanguage);
	}

	//Debugging code for profiling memory usage of pak system
	/*ICryPak::PakInfo* pPakInfo = m_env.pCryPak->GetPakInfo();
	   size_t openPakSize = 0;
	   for( uint32 pak = 0; pak < pPakInfo->numOpenPaks; pak++ )
	   {
	   openPakSize += pPakInfo->arrPaks[pak].nUsedMem;
	   }
	   m_env.pCryPak->FreePakInfo(pPakInfo);

	   CryLogAlways("Total pak size after loading localization is %d", openPakSize);*/
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenLanguageAudioPak(char const* const szLanguage)
{
	// load localized pak with crc32 filenames on consoles to save memory.
	string localizedPath;
	GetLocalizedAudioPath(szLanguage, localizedPath);

	string const bindingRoot(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR);
	string pakFilePath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + localizedPath);

	if (g_cvars.sys_build_folder->GetString() != nullptr && g_cvars.sys_build_folder->GetString()[0] != '\0')
	{
		pakFilePath = PathUtil::AddSlash(g_cvars.sys_build_folder->GetString()) + string(PathUtil::GetGameFolder()) + CRY_NATIVE_PATH_SEPSTR + localizedPath;
	}

	if (!m_env.pCryPak->OpenPack(bindingRoot.c_str(), pakFilePath))
	{
		// make sure the localized language is found - not really necessary, for TC
		CryLogAlways("Localized language content(%s) not available or modified from the original installation.", szLanguage);
	}

	//Debugging code for profiling memory usage of pak system
	/*ICryPak::PakInfo* pPakInfo = m_env.pCryPak->GetPakInfo();
	   size_t openPakSize = 0;
	   for( uint32 pak = 0; pak < pPakInfo->numOpenPaks; pak++ )
	   {
	   openPakSize += pPakInfo->arrPaks[pak].nUsedMem;
	   }
	   m_env.pCryPak->FreePakInfo(pPakInfo);

	   CryLogAlways("Total pak size after loading localization is %d", openPakSize);*/
}

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
static wstring GetErrorStringUnsupportedCPU()
{
	static const wchar_t s_EN[] = L"Unsupported CPU detected. CPU needs to support SSE, SSE2 and SSE3.";
	static const wchar_t s_FR[] = { 0 };
	static const wchar_t s_RU[] = { 0 };
	static const wchar_t s_ES[] = { 0 };
	static const wchar_t s_DE[] = { 0 };
	static const wchar_t s_IT[] = { 0 };

	const size_t fullLangID = (size_t) GetKeyboardLayout(0);
	const size_t primLangID = fullLangID & 0x3FF;
	const wchar_t* pFmt = s_EN;

	/*switch (primLangID)
	   {
	   case 0x07: // German
	   pFmt = s_DE;
	   break;
	   case 0x0a: // Spanish
	   pFmt = s_ES;
	   break;
	   case 0x0c: // French
	   pFmt = s_FR;
	   break;
	   case 0x10: // Italian
	   pFmt = s_IT;
	   break;
	   case 0x19: // Russian
	   pFmt = s_RU;
	   break;
	   case 0x09: // English
	   default:
	   break;
	   }*/
	return wstring(pFmt);
}
#endif

static bool CheckCPURequirements(CCpuFeatures* pCpu, CSystem* pSystem)
{
#if !defined(DEDICATED_SERVER)
	#if CRY_PLATFORM_WINDOWS
	if (!gEnv->IsEditor() && !gEnv->IsDedicated())
	{
		if (!(pCpu->GetFeatures() & CPUF_SSE3))
		{
			CryLogAlways("Unsupported CPU! Need SSE, SSE2 and SSE3 instructions to be available.");

			CryLogAlways("Asking user if they wish to continue...");
			if (CryMessageBox(GetErrorStringUnsupportedCPU().c_str(), L"CRYENGINE", eMB_YesCancel) == eQR_Cancel)
			{
				CryLogAlways("User chose to cancel.");
				return false;
			}

			CryLogAlways("User chose to continue despite unsupported CPU!");
		}
	}
	#endif
#endif // !defined(DEDICATED_SERVER)
	return true;
}

// System initialization
/////////////////////////////////////////////////////////////////////////////////
// INIT
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::Initialize(SSystemInitParams& startupParams)
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CSystem: Init");

	// Fix to improve wait() time within third-party APIs
#if CRY_PLATFORM_WINDOWS
	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	{
		CryFatalError("Error while changing the system timer resolution!");
	}
	timeBeginPeriod(tc.wPeriodMin);
#endif // CRY_PLATFORM_WINDOWS

	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_INIT);
	gEnv->mMainThreadId = GetCurrentThreadId();     //Set this ASAP on startup

	InlineInitializationProcessing("CSystem::Init start");
	m_szCmdLine = startupParams.szSystemCmdLine;

	m_pCmdLine = new CCmdLine(startupParams.szSystemCmdLine);

	if (m_pCmdLine->FindArg(eCLAT_Pre, "norandom"))
	{
		startupParams.bNoRandom = true;
	}

	// set unit test flag at start so multiple systems could handle initialization differently when needed
	if (m_pCmdLine->FindArg(eCLAT_Pre, "run_crytest"))
	{
		startupParams.bTesting = true;
	}

	// Skipping renderer is useful for automation modes when visual output is unnecessary
	if (m_pCmdLine->FindArg(eCLAT_Pre, "skiprenderer"))
	{
		startupParams.bSkipRenderer = true;
	}

	// Skipping input is useful for automation modes
	if (m_pCmdLine->FindArg(eCLAT_Pre, "skipinput"))
	{
		startupParams.bSkipInput = true;
	}

	m_env.szCmdLine = m_szCmdLine.c_str();
	m_env.bTesting = startupParams.bTesting;
	m_env.bUnattendedMode = startupParams.bTesting || startupParams.bUnattendedMode;
	if (m_pCmdLine->FindArg(eCLAT_Pre, "noprompt"))
	{
		m_env.bUnattendedMode = true;
	}

	m_env.bNoRandomSeed = startupParams.bNoRandom;
	m_bShaderCacheGenMode = startupParams.bShaderCacheGen;
	assert(IsHeapValid());

#ifdef EXTENSION_SYSTEM_INCLUDE_TESTCASES
	TestExtensions(&CCryFactoryRegistryImpl::Access());
#endif

	//_controlfp(0, _EM_INVALID|_EM_ZERODIVIDE | _PC_64 );

	// Get file version information.
	QueryVersionInfo();
	DetectGameFolderAccessRights();

	m_hWnd = (CRY_HWND)startupParams.hWnd;

	m_binariesDir = startupParams.szBinariesDir;

#if CRY_PLATFORM_DESKTOP && !defined(_RELEASE)
	m_env.SetIsEditor(startupParams.bEditor);
	m_env.SetIsEditorGameMode(false);
	m_env.SetIsEditorSimulationMode(false);
#endif

	m_bPreviewMode = startupParams.bPreview;
	m_bUIFrameworkMode = startupParams.bUIFramework;
	m_pUserCallback = startupParams.pUserCallback;

#if !defined(_RELEASE)
	if (!startupParams.bDedicatedServer)
	{
		const ICmdLineArg* dedicated = m_pCmdLine->FindArg(eCLAT_Pre, "dedicated");
		if (dedicated)
		{
			startupParams.bDedicatedServer = true;
		}
	}
#endif // !defined(_RELEASE)

#if defined(DEDICATED_SERVER)
	startupParams.bDedicatedServer = true;
#endif // #if defined(DEDICATED_SERVER)

#if CRY_PLATFORM_DESKTOP
	const ICmdLineArg* pDedicatedArbitrator = m_pCmdLine->FindArg(eCLAT_Pre, "dedicatedarbitrator");
	if (pDedicatedArbitrator)
	{
		startupParams.bDedicatedServer = true;
		gEnv->bDedicatedArbitrator = true;
	}
#endif

#if CRY_PLATFORM_DESKTOP
	if (startupParams.bDedicatedServer)
	{
		m_env.SetIsDedicated(true);
	}
#endif

	m_currentLanguageAudio = "";
#if defined(DEDICATED_SERVER)
	m_bNoCrashDialog = true;
#else
	m_bNoCrashDialog = false;
#endif

	memcpy(gEnv->pProtectedFunctions, startupParams.pProtectedFunctions, sizeof(startupParams.pProtectedFunctions));

	m_env.bIsOutOfMemory = false;

	{
		bool devModeEnable = true;

#if defined(_RELEASE)
		// disable devmode by default in release builds outside the editor
		devModeEnable = m_env.IsEditor();
#endif

		// disable devmode in launcher if someone really wants to (even in non release builds)
		if (!m_env.IsEditor() && m_pCmdLine->FindArg(eCLAT_Pre, "nodevmode"))
		{
			devModeEnable = false;
		}

		SetDevMode(devModeEnable);
	}

#if !defined(DEDICATED_SERVER)
	const ICmdLineArg* crashdialog = m_pCmdLine->FindArg(eCLAT_Post, "sys_no_crash_dialog");
	if (crashdialog)
	{
		m_bNoCrashDialog = true;
	}
#endif

	if (startupParams.bUnattendedMode)
	{
		m_bNoCrashDialog = true;
	}

	if (!startupParams.pValidator)
	{
		m_pDefaultValidator = new SDefaultValidator(this);
		m_pValidator = m_pDefaultValidator;
	}
	else
	{
		m_pValidator = startupParams.pValidator;
	}

	if (!startupParams.bSkipRenderer)
	{
		m_pHmdManager = new CHmdManager();
	}

#if CRY_PLATFORM_DESKTOP
	#if !defined(_RELEASE)
	bool isDaemonMode = (m_pCmdLine->FindArg(eCLAT_Pre, "daemon") != 0);
	#else
	bool isDaemonMode = false;
	#endif // !defined(_RELEASE)

	#if defined(USE_DEDICATED_SERVER_CONSOLE)

		#if !defined(_RELEASE)
	bool isSimpleConsole = (m_pCmdLine->FindArg(eCLAT_Pre, "simple_console") != 0);

	if (!(isDaemonMode || isSimpleConsole))
		#endif // !defined(_RELEASE)
	{
		string headerName;
		#if defined(USE_UNIXCONSOLE)
		CUNIXConsole* pConsole = pUnixConsole = new CUNIXConsole();
		headerName = "Unix ";
		#elif defined(USE_IOSCONSOLE)
		CIOSConsole* pConsole = new CIOSConsole();
		headerName = "iOS ";
		#elif defined(USE_WINDOWSCONSOLE)
		CWindowsConsole* pConsole = new CWindowsConsole();
		#elif defined(USE_ANDROIDCONSOLE)
		CAndroidConsole* pConsole = new CAndroidConsole();
		headerName = "Android "
		#else
		CNULLConsole * pConsole = new CNULLConsole(false);
		#endif
		m_pTextModeConsole = static_cast<ITextModeConsole*>(pConsole);

		if (m_pUserCallback == NULL)
		{
			auto getProductVersion = [this]
			{
				char version[64];
				GetProductVersion().ToString(version);
				return string(version);
			};

			if (m_env.IsDedicated())
			{
				m_pUserCallback = pConsole;
				pConsole->SetRequireDedicatedServer(true);
				headerName.append("Dedicated Server");
				if (gEnv->bDedicatedArbitrator)
				{
					headerName.append(" Arbitrator");
				}
				headerName.append(" - Version ");
				headerName.append(getProductVersion());
				pConsole->SetHeader(headerName.c_str());
			}
#if !defined(RELEASE) || defined(ENABLE_DEVELOPER_CONSOLE_IN_RELEASE)
			else if (m_pCmdLine->FindArg(eCLAT_Pre, "console"))
			{
				m_pUserCallback = pConsole;
				pConsole->SetRequireDedicatedServer(false);
				headerName.append("Client - Version ");
				headerName.append(getProductVersion());
				pConsole->SetHeader(headerName.c_str());
			}
#endif
		}
	}
		#if !defined(_RELEASE)
	else
		#endif
	#endif

	#if !(defined(USE_DEDICATED_SERVER_CONSOLE) && defined(_RELEASE))
	{
		CNULLConsole* pConsole = new CNULLConsole(isDaemonMode);
		m_pTextModeConsole = pConsole;

		if (m_pUserCallback == NULL && m_env.IsDedicated())
			m_pUserCallback = pConsole;
	}
	#endif

#endif // CRY_PLATFORM_DESKTOP

	//////////////////////////////////////////////////////////////////////////
	// LOAD GAME PROJECT CONFIGURATION
	//////////////////////////////////////////////////////////////////////////
	InlineInitializationProcessing("CSystem::Init Load project configuration");

	// Load project directory early, since it relies on overriding current working folder
	m_pProjectManager = new Cry::CProjectManager();

	//////////////////////////////////////////////////////////////////////////
	// Create PlatformOS
	//////////////////////////////////////////////////////////////////////////
	// moved before 'InitFileSystem' for streaming install initialization:
	//	10/06/2013 by Andriy
	m_pPlatformOS.reset(IPlatformOS::Create(m_PlatformOSCreateFlags));
	InlineInitializationProcessing("CSystem::Init PlatformOS");

	{
		//////////////////////////////////////////////////////////////////////////
		// File system, must be very early
		//////////////////////////////////////////////////////////////////////////
		InitFileSystem(startupParams);

		//////////////////////////////////////////////////////////////////////////
		InlineInitializationProcessing("CSystem::Init InitFileSystem");

		//here we should be good to ask Crypak to do something

		//#define GEN_PAK_CDR_CRC
#ifdef GEN_PAK_CDR_CRC

		const char* filename = m_pCmdLine->GetArg(1)->GetName();
		gEnv->pCryPak->OpenPack(filename);

		int crc = gEnv->pCryPak->ComputeCachedPakCDR_CRC(filename, false);

		exit(crc);

#endif

		// CPU features detection.
		m_pCpu = new CCpuFeatures;
		m_pCpu->Detect();
		m_env.pi.numCoresAvailableToProcess = m_pCpu->GetCPUCount();
		m_env.pi.numLogicalProcessors = m_pCpu->GetLogicalCPUCount();
		m_env.pi.szProcessorType = m_pCpu->m_Cpu[0].mCpuType;

		// Check hard minimum CPU requirements
		if (!CheckCPURequirements(m_pCpu, this))
			return false;

		m_env.pLog->RegisterConsoleVariables();

		GetIRemoteConsole()->RegisterConsoleVariables();

#if defined(ENABLE_PROFILING_CODE)
		CStroboscope::GetInst()->RegisterCommands();
#endif

		// Register system console variables.
		CreateSystemVars();

		if (*startupParams.szUserPath)
		{
			m_sys_user_folder->Set(startupParams.szUserPath);
		}

		// Set this as soon as the system cvars got initialized.
		static_cast<CCryPak* const>(m_env.pCryPak)->SetLocalizationFolder(g_cvars.sys_localization_folder->GetString());

		//////////////////////////////////////////////////////////////////////////
		//Load engine files
		//////////////////////////////////////////////////////////////////////////
		InlineInitializationProcessing("CSystem::Init Load Engine Folders");

		InitFileSystem_LoadEngineFolders();
		//////////////////////////////////////////////////////////////////////////

		//notify test system to init logs (since file system is setup).
#ifdef CRY_TESTING
		if (m_pTestSystem)
		{
			m_pTestSystem->InitLog();
		}

		if (m_env.pConsole != nullptr)
		{
			CryTest::CTestSystem::InitCommands();
		}
#endif // CRY_TESTING

		// Initialise after pLog and CPU feature initialization
		// AND after console creation (Editor only)
		// May need access to engine folder .pak files
		gEnv->pThreadManager->GetThreadConfigManager()->LoadConfig("%engine%/config/engine_core.thread_config");

		if (m_env.IsEditor())
			gEnv->pThreadManager->GetThreadConfigManager()->LoadConfig("%engine%/config/engine_sandbox.thread_config");

		// Setup main thread
		void* pThreadHandle = 0; // Let system figure out thread handle
		gEnv->pThreadManager->RegisterThirdPartyThread(pThreadHandle, "Main");

		// Start watchdog after thread manager initialization
		if (int val = m_sys_profile_watchdog_timeout->GetIVal())
		{
			m_pWatchdog = new CWatchdogThread(val);
		}

		CryGetIMemReplay()->EnableAsynchMode();

		if (!InitReflectionSystem(startupParams))
		{
			return false;
		}

		Cry::Reflection::CTypeRegistrationChain::Execute(g_cvars.sys_reflection_natvis != 0);
		// Init UDR

		if (!startupParams.bPreview && !startupParams.bShaderCacheGen)
		{
			CryLogAlways("UDR initialization");
			if (!InitUDR(startupParams))
			{
				return false;
			}
		}
		m_pResourceManager->Init();

#ifdef CODECHECKPOINT_ENABLED
		// Setup code checkpoint manager if checkpoints are enabled
		m_env.pCodeCheckpointMgr = new CCodeCheckpointMgr;
#else
		m_env.pCodeCheckpointMgr = NULL;
#endif

		//////////////////////////////////////////////////////////////////////////
		// CREATE NOTIFICATION NETWORK
		//////////////////////////////////////////////////////////////////////////
		InlineInitializationProcessing("CSystem::Init NotificationNetwork");

		m_pNotificationNetwork = NULL;
#ifndef _RELEASE
	#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)

		if (!startupParams.bMinimal && !gEnv->IsDedicated())
		{
			if (m_pNotificationNetwork = CNotificationNetwork::Create())
			{
				m_pNotificationNetwork->ListenerBind("HotUpdate", &CHotUpdateNotification::Instance());
			}
		}

	#endif
#endif // _RELEASE

		if (m_pUserCallback)
			m_pUserCallback->OnInit(this);

		if (m_pProfilingSystem)
			m_pProfilingSystem->RegisterCVars();

#ifdef ENABLE_LOADING_PROFILER
		CBootProfiler::GetInstance().RegisterCVars();
#endif
		if (m_pProfileRenderer)
			m_pProfileRenderer->RegisterCVars();

		// Register Audio-related system CVars
		CreateAudioVars();

		InlineInitializationProcessing("CSystem::Init Create console");

		LogSystemInfo();

		//////////////////////////////////////////////////////////////////////////
		//Load config files
		//////////////////////////////////////////////////////////////////////////
		InlineInitializationProcessing("CSystem::Init Load Engine Config Files");

		int curSpecVal = 0;
		ICVar* pSysSpecCVar = gEnv->pConsole->GetCVar("sys_spec");
		if (gEnv->pSystem->IsDevMode())
		{
			if (pSysSpecCVar && pSysSpecCVar->GetFlags() & VF_WASINCONFIG)
			{
				curSpecVal = pSysSpecCVar->GetIVal();
				pSysSpecCVar->SetFlags(pSysSpecCVar->GetFlags() | VF_SYSSPEC_OVERWRITE);
			}
		}

		if (m_pCmdLine->FindArg(eCLAT_Pre, "ResetProfile") == 0)
		{
			LoadConfiguration("%USER%/game.cfg", 0, eLoadConfigGame, ELoadConfigurationFlags::SuppressConfigNotFoundWarning);
		}

		//if sys spec variable was specified, is not 0 and we are in devmode, restore the value from before loading game.cfg
		//this enables setting of a specific sys_spec outaide menu and game.cfg
		if (gEnv->pSystem->IsDevMode())
		{
			if (pSysSpecCVar && curSpecVal && curSpecVal != pSysSpecCVar->GetIVal())
			{
				pSysSpecCVar->Set(curSpecVal);
			}
		}

		if (!startupParams.bSkipRenderer)
		{
			CreateRendererVars(startupParams);
		}

		{
			ILoadConfigurationEntrySink* pCVarsWhiteListConfigSink = GetCVarsWhiteListConfigSink();

			// We have to load this file again since first time we did it without devmode
			LoadConfiguration("%ENGINEROOT%/system.cfg", pCVarsWhiteListConfigSink, eLoadConfigInit, ELoadConfigurationFlags::SuppressConfigNotFoundWarning);
			LoadConfiguration("user.cfg", pCVarsWhiteListConfigSink, eLoadConfigInit, ELoadConfigurationFlags::SuppressConfigNotFoundWarning);

#if defined(ENABLE_STATS_AGENT)
			if (m_pCmdLine->FindArg(eCLAT_Pre, "useamblecfg"))
			{
				LoadConfiguration("amble.cfg", pCVarsWhiteListConfigSink, eLoadConfigInit);
			}
#endif
		}

#if CRY_PLATFORM_DURANGO
		// OnSysSpecChange Triggers loading of durango.cfg
		OnSysSpecChange(m_sys_spec);
#elif CRY_PLATFORM_ORBIS
		LoadConfiguration("orbis.cfg", 0, eLoadConfigSystemSpec);
#elif CRY_PLATFORM_MOBILE
		LoadConfiguration("mobile.cfg", 0, eLoadConfigInit);
#elif CRY_PLATFORM_LINUX
		LoadConfiguration("linux.cfg", 0, eLoadConfigInit);
#endif
#if defined(PERFORMANCE_BUILD)
		LoadConfiguration("performance.cfg", 0, eLoadConfigInit);
#endif

		if (g_cvars.sys_vr_support)
			GetISystem()->LoadConfiguration("vr.cfg", 0, eLoadConfigInit);

#ifdef USE_CRY_ASSERT
		if (Cry::Assert::GetAssertLevel() > Cry::Assert::ELevel::Enabled)
		{
			gEnv->bUnattendedMode = true; // skip assert UI when sys_assert is 2 or 3
		}
#endif

#if defined(CRY_PLATFORM_DESKTOP) && defined(USE_DEDICATED_SERVER_CONSOLE)
		m_pTextModeConsole->SetTitle(m_pProjectManager->GetCurrentProjectName());
#endif

		//////////////////////////////////////////////////////////////////////////
		// CREATE JOBMANAGER
		//////////////////////////////////////////////////////////////////////////
		m_env.pJobManager->Init(m_sys_job_system_max_worker ? m_sys_job_system_max_worker->GetIVal() : 0);

		//////////////////////////////////////////////////////////////////////////
		// Stream Engine
		//////////////////////////////////////////////////////////////////////////
		CryLogAlways("Stream Engine Initialization");
		InitStreamEngine();
		InlineInitializationProcessing("CSystem::Init StreamEngine");

		//	if (!g_sysSpecChanged)
		//		OnSysSpecChange( m_sys_spec );

		{
			if (m_pCmdLine->FindArg(eCLAT_Pre, STR_DX11_RENDERER))
				m_env.pConsole->LoadConfigVar("r_Driver", STR_DX11_RENDERER);
			else if (m_pCmdLine->FindArg(eCLAT_Pre, STR_DX12_RENDERER))
				m_env.pConsole->LoadConfigVar("r_Driver", STR_DX12_RENDERER);
			else if (m_pCmdLine->FindArg(eCLAT_Pre, STR_GL_RENDERER))
				m_env.pConsole->LoadConfigVar("r_Driver", STR_GL_RENDERER);
			else if (m_pCmdLine->FindArg(eCLAT_Pre, STR_VK_RENDERER))
				m_env.pConsole->LoadConfigVar("r_Driver", STR_VK_RENDERER);
			else if (m_pCmdLine->FindArg(eCLAT_Pre, STR_GNM_RENDERER))
				m_env.pConsole->LoadConfigVar("r_Driver", STR_GNM_RENDERER);
		}

		CryLogAlways("BuildTime: " __DATE__ " " __TIME__);

		InlineInitializationProcessing("CSystem::Init LoadConfigurations");

#if ENABLE_STATOSCOPE
		if (!m_env.pStatoscope)
		{
			m_env.pStatoscope = new CStatoscope();
		}
#elif defined(CRY_UBSAN)
		if (!m_env.pStatoscope)
		{
			// UBSAN: We have code that relies on IStatoscope members not accessing any this-relative memory.
			// However, UBSAN will check for this, so we actually instantiate with a dummy IStatoscope instance.
			m_env.pStatoscope = new IStatoscope();
		}
#else
		m_env.pStatoscope = NULL;
#endif

		m_env.pOverloadSceneManager = new COverloadSceneManager;

		if (m_env.IsDedicated() && m_rDriver)
		{
			m_sSavedRDriver = m_rDriver->GetString();
			m_rDriver->Set("NULL");
		}

#if CRY_PLATFORM_WINDOWS
		if (!startupParams.bSkipRenderer)
		{
			if (stricmp(m_rDriver->GetString(), STR_AUTO_RENDERER) == 0)
			{
				m_rDriver->Set(STR_DX11_RENDERER);
			}
		}
#endif

#if CRY_PLATFORM_WINDOWS
		if (g_cvars.sys_WER) SetUnhandledExceptionFilter(CryEngineExceptionFilterWER);
#endif

		//////////////////////////////////////////////////////////////////////////
		// Interprocess Communication
		//////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_STATS_AGENT)
		const ICmdLineArg* pPipeArg = m_pCmdLine->FindArg(eCLAT_Pre, "lt_pipename");
		if (pPipeArg != nullptr)
			CStatsAgent::CreatePipe(pPipeArg);
#endif

		//////////////////////////////////////////////////////////////////////////
		// PHYSICS
		//////////////////////////////////////////////////////////////////////////
		//if (!params.bPreview)
		if (!m_bUIFrameworkMode && !startupParams.bShaderCacheGen)
		{
			CryLogAlways("Physics initialization");
			if (!InitPhysics(startupParams))
				return false;
		}

		InlineInitializationProcessing("CSystem::Init InitPhysics");

		//////////////////////////////////////////////////////////////////////////
		// Localization
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bMinimal)
		{
			InitLocalization();
		}
		InlineInitializationProcessing("CSystem::Init InitLocalizations");

		//////////////////////////////////////////////////////////////////////////
		// AUDIO
		//////////////////////////////////////////////////////////////////////////
		bool bAudioInitSuccess = false;
		if (!startupParams.bPreview && !m_env.IsDedicated() && !m_bUIFrameworkMode && !startupParams.bShaderCacheGen &&
		    (m_sys_audio_disable->GetIVal() == 0))
		{
			CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "AudioSystem initialization");
			CryLogAlways("<Audio>: AudioSystem initialization");
			INDENT_LOG_DURING_SCOPE();

			bAudioInitSuccess = InitializeEngineModule(startupParams, DLL_AUDIOSYSTEM, cryiidof<CryAudio::ISystemModule>(), false);
		}

		if (!bAudioInitSuccess)
		{
			m_env.pAudioSystem = new CryAudio::Null::CSystem();

			if (m_env.pAudioSystem != nullptr)
			{
				CryLogAlways("<Audio>: Running with NULL AudioSystem.");
			}
			else
			{
				CryFatalError("<Audio>: Could not create an instance of CNULLAudioSystem!");
			}
		}

		// Note: IME manager needs to be created before Scaleform is initialized
		m_pImeManager = new CImeManager();

#if defined(USE_MONO) && USE_MONO == 1
		// Initialize CryMono / C# integration
		// Note that this has to occur before plug-ins are loaded as this is a prerequisite for C# plug-ins!
		{
			const ICVar* pCVar = m_env.pConsole->GetCVar("sys_use_mono");
			if (pCVar && pCVar->GetIVal())
			{
				CryLogAlways("C# Backend initialization");
				INDENT_LOG_DURING_SCOPE();

				if (m_pUserCallback)
				{
					m_pUserCallback->OnInitProgress("Initializing C#...");
				}

				InitMonoBridge(startupParams);
			}
		}
#endif

		InlineInitializationProcessing("CSystem::Init LoadProjectPlugins");
		m_pPluginManager->LoadProjectPlugins();

		InlineInitializationProcessing("CSystem::Init InitRenderer");

		m_pSystemEventDispatcher->OnSystemEvent(ESYSTEM_EVENT_PRE_RENDERER_INIT, 0, 0);

		// [VR]
		if (m_pHmdManager)
		{
			m_pHmdManager->SetupAction(IHmdManager::eHmdSetupAction_Init);
		}

		// Hardware mouse
		//////////////////////////////////////////////////////////////////////////
		// - Dedicated server is in console mode by default (Hardware Mouse is always shown when console is)
		// - Mouse is always visible by default in Editor (we never start directly in Game Mode)
		// - Mouse has to be enabled manually by the Game (this is typically done in the main menu)
		// - Hardware mouse is initialize prior Renderer to catch window focus events
#ifdef DEDICATED_SERVER
		m_env.pHardwareMouse = NULL;
#else
		m_env.pHardwareMouse = new CHardwareMouse(true);
#endif

		//////////////////////////////////////////////////////////////////////////
		// RENDERER
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bSkipRenderer && !m_env.IsDedicated())
		{
			assert(IsHeapValid());
			CryLogAlways("Renderer initialization");

			if (!InitRenderer(startupParams))
			{
				return false;
			}
			assert(IsHeapValid());
			if (m_env.pRenderer)
			{
				bool bMultiGPUEnabled = false;
				m_env.pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPUEnabled);
				if (bMultiGPUEnabled)
					LoadConfiguration("mgpu.cfg");

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
				if (!m_bShaderCacheGenMode)
				{
					if (!InitializeEngineModule(startupParams, DLL_SCALEFORM, cryiidof<IScaleformHelperEngineModule>(), false))
					{
						m_env.pScaleformHelper = nullptr;
						CryLog("Attempt to load Scaleform helper library from '%s' failed, this feature will not be available", DLL_SCALEFORM);
					}
					else if (!m_env.pScaleformHelper->Init())
					{
						m_env.pScaleformHelper->Destroy();
						m_env.pScaleformHelper = nullptr;
						CryLog("Unable to initialize Scaleform helper library, this feature will not be available");
					}
					else
					{
						m_env.pScaleformHelper->SetAmpEnabled(false);
					}
				}
#endif
				else
				{
					m_env.pScaleformHelper = nullptr;
				}
			}
		}
		else
		{
#if CRY_PLATFORM_DESKTOP
			if (m_env.IsDedicated() && !isDaemonMode)
			{
				m_pNULLRenderAuxGeom = CNULLRenderAuxGeom::Create();
				m_env.pAuxGeomRenderer = m_pNULLRenderAuxGeom;
				InitPhysicsRenderer(startupParams);
			}
#endif // CRY_PLATFORM_DESKTOP
		}

		//////////////////////////////////////////////////////////////////////////
		m_pUserAnalyticsSystem->Initialize();

		InlineInitializationProcessing("CSystem::Init CSharedFlashPlayerResources::Init");

		const bool bStartScreensAllowed = !startupParams.bShaderCacheGen
#ifndef RELEASE
		                                  && !startupParams.bEditor
#endif
		                                  && !m_env.IsDedicated()
		                                  && m_env.pRenderer;

		if (g_cvars.sys_intromoviesduringinit && bStartScreensAllowed)
		{
			m_env.pRenderer->InitSystemResources(FRR_SYSTEM_RESOURCES);
			m_env.pRenderer->StartRenderIntroMovies();
		}
		else if (g_cvars.sys_splashscreen != nullptr && bStartScreensAllowed && g_cvars.sys_splashscreen->GetString()[0] != '\0')
		{
			CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "Rendering Splash Screen");
			ITexture* pTex = m_env.pRenderer->EF_LoadTexture(g_cvars.sys_splashscreen->GetString(), FT_DONT_STREAM | FT_NOMIPS);
			if (pTex)
			{
				const int splashWidth = pTex->GetWidth();
				const int splashHeight = pTex->GetHeight();

				const int screenWidth = m_env.pRenderer->GetOverlayWidth();
				const int screenHeight = m_env.pRenderer->GetOverlayHeight();

				if (splashWidth > 0 && splashHeight > 0 && screenWidth > 0 && screenHeight > 0)
				{
					const float scaleX = (float)screenWidth / (float)splashWidth;
					const float scaleY = (float)screenHeight / (float)splashHeight;

					const float scale = (scaleY * splashWidth > screenWidth) ? scaleX : scaleY;

					const float w = splashWidth * scale;
					const float h = splashHeight * scale;
					const float x = (screenWidth - w) * 0.5f;
					const float y = (screenHeight - h) * 0.5f;

					// make sure it's rendered in full screen mode when triple buffering is enabled as well
					for (size_t n = 0; n < 3; n++)
					{
						m_env.pRenderer->BeginFrame({}, SGraphicsPipelineKey::BaseGraphicsPipelineKey);
						IRenderAuxImage::Draw2dImage(x, y, w, h, pTex->GetTextureID(), 0.0f, 1.0f, 1.0f, 0.0f);
						m_env.pRenderer->EndFrame();
					}
				}
				else
				{
					gEnv->pLog->LogWarning("Invalid splash screen texture");
				}

				pTex->Release();
			}
		}

		InlineInitializationProcessing("CSystem::Init m_env.pRenderer->StartRenderIntroMovies");
		//////////////////////////////////////////////////////////////////////////
		// Open basic pak files after intro movie playback started
		//////////////////////////////////////////////////////////////////////////
		OpenBasicPaks(true);

		//////////////////////////////////////////////////////////////////////////
		// FONT
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bSkipFont)
		{
			CryLogAlways("Font initialization");
			if (!InitFont(startupParams))
				return false;

			if (m_env.pCryFont)
				m_env.pCryFont->SetRendererProperties(m_env.pRenderer);
		}

		InlineInitializationProcessing("CSystem::Init InitFonts");

		//////////////////////////////////////////////////////////////////////////
		// POST RENDERER
		//////////////////////////////////////////////////////////////////////////
		if (m_env.pRenderer)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init Post Renderer");
			if (m_pUserCallback != nullptr)
			{
				m_pUserCallback->OnInitProgress("Initializing Renderer...");
			}

			m_env.pRenderer->PostInit();

			if (!startupParams.bShaderCacheGen)
			{
				// try to do a flush to keep the renderer busy during loading
				m_env.pRenderer->TryFlush();
			}
		}
		InlineInitializationProcessing("CSystem::Init Renderer::PostInit");

		//////////////////////////////////////////////////////////////////////////
		// NETWORK
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bPreview && !m_bUIFrameworkMode && !startupParams.bShaderCacheGen)
		{
			INDENT_LOG_DURING_SCOPE();
			InitNetwork(startupParams);

			if (gEnv->IsDedicated())
				m_pServerThrottle.reset(new CServerThrottle(this, m_pCpu->GetCPUCount()));
		}
		InlineInitializationProcessing("CSystem::Init InitNetwork");

		//////////////////////////////////////////////////////////////////////////
		// MOVIE
		//////////////////////////////////////////////////////////////////////////
		if (!m_bUIFrameworkMode && !startupParams.bShaderCacheGen)
		{
			INDENT_LOG_DURING_SCOPE();
			if (!InitMovieSystem(startupParams))
				return false;
		}

		InlineInitializationProcessing("CSystem::Init InitMovie");

		//////////////////////////////////////////////////////////////////////////
		// SERVICE NETWORK
		//////////////////////////////////////////////////////////////////////////
		m_env.pServiceNetwork = new CServiceNetwork();

		//////////////////////////////////////////////////////////////////////////
		// REMOTE COMMAND SYTSTEM
		//////////////////////////////////////////////////////////////////////////
		m_env.pRemoteCommandManager = new CRemoteCommandManager();

		//////////////////////////////////////////////////////////////////////////
		// DISK PROFILER
		//////////////////////////////////////////////////////////////////////////
#if defined(USE_DISK_PROFILER)
		m_pDiskProfiler = new CDiskProfiler(this);
#else
		m_pDiskProfiler = 0;
#endif

		// TIME
		//////////////////////////////////////////////////////////////////////////
		CryLogAlways("Time initialization");
		if (!m_Time.Init())
			return (false);
		m_Time.ResetTimer();

		//////////////////////////////////////////////////////////////////////////
		// INPUT
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bPreview && !gEnv->IsDedicated() && !startupParams.bShaderCacheGen)
		{
			CryLogAlways("Input initialization");
			INDENT_LOG_DURING_SCOPE();
			if (!InitInput(startupParams))
				return false;

			if (m_env.pHardwareMouse)
				m_env.pHardwareMouse->OnPostInitInput();
		}

		if(gEnv->pInput)
		{
			if(m_pProfilingSystem)
				gEnv->pInput->AddEventListener(m_pProfilingSystem);
			if (m_pProfileRenderer)
				gEnv->pInput->AddEventListener(m_pProfileRenderer);
		}

		InlineInitializationProcessing("CSystem::Init InitInput");

		//////////////////////////////////////////////////////////////////////////
		// Create MiniGUI
		//////////////////////////////////////////////////////////////////////////
		minigui::IMiniGUIPtr pMiniGUI;
		if (CryCreateClassInstanceForInterface(cryiidof<minigui::IMiniGUI>(), pMiniGUI))
		{
			m_pMiniGUI = pMiniGUI.get();
			m_pMiniGUI->Init();
		}

		InlineInitializationProcessing("CSystem::Init InitMiniGUI");

		if (m_env.pConsole != nullptr)
		{
			static_cast<CXConsole*>(m_env.pConsole)->PostRendererInit();
		}

		//////////////////////////////////////////////////////////////////////////
		// Init Animation system
		//////////////////////////////////////////////////////////////////////////
		{
			CryLogAlways("Initializing Animation System");
			INDENT_LOG_DURING_SCOPE();
			if (!m_bUIFrameworkMode && !startupParams.bShaderCacheGen)
				if (!InitAnimationSystem(startupParams))
					return false;
		}

		//////////////////////////////////////////////////////////////////////////
		// Init 3d engine
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bShaderCacheGen)
		{
			CryLogAlways("Init 3D Engine");
			INDENT_LOG_DURING_SCOPE();

			if (!Init3DEngine(startupParams))
				return false;

			// try flush to keep renderer busy
			if (m_env.pRenderer)
				m_env.pRenderer->TryFlush();
		}

		InlineInitializationProcessing("CSystem::Init Init3DEngine");
		if (m_env.pCharacterManager)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::Other, "Post Init Character Manager");
			m_env.pCharacterManager->PostInit();
		}

#ifdef DOWNLOAD_MANAGER
		m_pDownloadManager = new CDownloadManager;
		m_pDownloadManager->Create(this);
#endif //DOWNLOAD_MANAGER

		//////////////////////////////////////////////////////////////////////////
		// SCRIPT SYSTEM
		//////////////////////////////////////////////////////////////////////////
		// We need script materials for now

		if (!startupParams.bShaderCacheGen)
		{
			CryLogAlways("Script System Initialization");
			INDENT_LOG_DURING_SCOPE();

			if (!InitScriptSystem(startupParams))
				return false;
		}

		InlineInitializationProcessing("CSystem::Init InitScripts");

		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// ENTITY SYSTEM
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bPreview && !startupParams.bShaderCacheGen)
		{
			// Start with initializing Schematyc before the entity system
			{
				CryLogAlways("Schematyc initialization");
				INDENT_LOG_DURING_SCOPE();

				if (!InitSchematyc(startupParams))
					return false;
			}

			CryLogAlways("Entity system initialization");
			INDENT_LOG_DURING_SCOPE();

			if (!InitEntitySystem(startupParams))
				return false;
		}

		InlineInitializationProcessing("CSystem::Init InitEntitySystem");

		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// LIVECREATE SYSTEM
		//////////////////////////////////////////////////////////////////////////
		{
			InlineInitializationProcessing("CSystem::Init InitLiveCreate");

			CryLogAlways("LiveCreate initialization");
			INDENT_LOG_DURING_SCOPE();

			if (m_pUserCallback)
				m_pUserCallback->OnInitProgress("Initializing LiveCreate...");
			// we dont have to return if fail, no problem if there is no LiveCreate

			InitLiveCreate(startupParams);
		}

		InlineInitializationProcessing("CSystem::Init InitInterface");
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// DYNAMIC RESPONSE SYSTEM
		if (!startupParams.bShaderCacheGen)
		{
			CryLogAlways("Dynamic Response System initialization");
			INDENT_LOG_DURING_SCOPE();

			if (m_pUserCallback)
				m_pUserCallback->OnInitProgress("Initializing Dynamic Response System...");

			if (m_env.IsDedicated() || !InitDynamicResponseSystem(startupParams))
			{
				CryLogAlways("No Dynamic Response System was loaded from a module, will use the NULL implementation.");
				m_env.pDynamicResponseSystem = new NullDRS::CSystem();
				if (m_env.pDynamicResponseSystem == NULL)
				{
					CryFatalError("<DRS>: Could not create an instance of NULLDynamicResponse::CSystem!");
				}
			}
		}

		InlineInitializationProcessing("CSystem::Init Dynamic Response System");

		//////////////////////////////////////////////////////////////////////////
		// BUDGETING SYSTEM
		m_pIBudgetingSystem = new CBudgetingSystem();

		InlineInitializationProcessing("CSystem::Init BudgetingSystem");

		//////////////////////////////////////////////////////////////////////////
		// Zlib compressor
		m_pIZLibCompressor = new CZLibCompressor();

		InlineInitializationProcessing("CSystem::Init ZLibCompressor");

		//////////////////////////////////////////////////////////////////////////
		// Zlib decompressor
		m_pIZLibDecompressor = new CZLibDecompressor();

		InlineInitializationProcessing("CSystem::Init ZLibDecompressor");

		//////////////////////////////////////////////////////////////////////////
		// Zlib decompressor
		m_pILZ4Decompressor = new CLZ4Decompressor();

		InlineInitializationProcessing("CSystem::Init LZ4Decompressor");

		//////////////////////////////////////////////////////////////////////////
		// Load FlowGraph
		if (!startupParams.bShaderCacheGen)
		{
			InitializeEngineModule(startupParams, "CryFlowGraph", cryiidof<IFlowSystemEngineModule>(), true);
		}

		//////////////////////////////////////////////////////////////////////////
		// AI
		//////////////////////////////////////////////////////////////////////////
		if (!startupParams.bPreview && !m_bUIFrameworkMode && !startupParams.bShaderCacheGen)
		{
			if (gEnv->IsDedicated() && m_svAISystem && !m_svAISystem->GetIVal())
				;
			else
			{
				CryLogAlways("AI initialization");
				INDENT_LOG_DURING_SCOPE();

				if (!InitAISystem(startupParams))
					return false;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// AI SYSTEM INITIALIZATION
		//////////////////////////////////////////////////////////////////////////
		// AI System needs to be initialized after entity system
		if (!startupParams.bPreview && !m_bUIFrameworkMode && m_env.pAISystem)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::Other, "Initialize AI System");

			if (m_pUserCallback)
				m_pUserCallback->OnInitProgress("Initializing AI System...");
			CryLogAlways("Initializing AI System");
			INDENT_LOG_DURING_SCOPE();
			m_env.pAISystem->Init();
		}

		if (m_pUserCallback)
			m_pUserCallback->OnInitProgress("Initializing additional systems...");
		CryLogAlways("Initializing additional systems");

		InlineInitializationProcessing("CSystem::Init AIInit");

		if (!startupParams.bShaderCacheGen)
		{
			InitGameFramework(startupParams);
		}
		else
		{
			// Command line is otherwise executed in InitGameFramework
			// Call it manually here to ensure that command line can still be used
			ExecuteCommandLine();
		}

		//////////////////////////////////////////////////////////////////////////
		// Create PerfHUD
		//////////////////////////////////////////////////////////////////////////

#if defined(USE_PERFHUD)
		MEMSTAT_CONTEXT(EMemStatContextType::Other, "Init PerfHUD");
		//Create late in Init so that associated CVars have already been created
		ICryPerfHUDPtr pPerfHUD;
		if (CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD))
		{
			m_pPerfHUD = pPerfHUD.get();
			m_pPerfHUD->Init();
		}
#endif

		//////////////////////////////////////////////////////////////////////////
		// Initialize task threads.
		//////////////////////////////////////////////////////////////////////////
		if (!m_env.IsDedicated())
		{
			assert(IsHeapValid());
			RegisterEngineStatistics();
		}

		InlineInitializationProcessing("CSystem::Init InitTaskThreads");

		//////////////////////////////////////////////////////////////////////////
		// Input Post Initialise - enables input threads to be created after thread init
		//////////////////////////////////////////////////////////////////////////
		if (m_env.pInput)
		{
			m_env.pInput->PostInit();
		}

		//////////////////////////////////////////////////////////////////////////
		// Init websockets webserver listening on non-standard port.
		//////////////////////////////////////////////////////////////////////////

#ifdef USE_HTTP_WEBSOCKETS
		if (startupParams.bSkipWebsocketServer == false && gEnv->pNetwork)
		{
			if (ISimpleHttpServer* http = gEnv->pNetwork->GetSimpleHttpServerSingleton())
			{
				// NOTE: Old port 880 will not work on Orbis. Needs to be between 1024 and 32767
				// Changed default to 1880.
				http->Start(g_cvars.sys_simple_http_base_port + GetApplicationInstance(), "", NULL);
			}
		}
#endif

		// final tryflush to be sure that all framework init request have been processed
		if (!startupParams.bShaderCacheGen && m_env.pRenderer)
			m_env.pRenderer->TryFlush();

#if !defined(RELEASE)
		m_env.pLocalMemoryUsage = new CLocalMemoryUsage();
#else
		m_env.pLocalMemoryUsage = NULL;
#endif

		if (g_cvars.sys_float_exceptions > 0)
		{
			if (g_cvars.sys_float_exceptions == 3 && gEnv->IsEditor()) // Turn off float exceptions in editor if sys_float_exceptions = 3
			{
				g_cvars.sys_float_exceptions = 0;
			}
			if (g_cvars.sys_float_exceptions > 0)
			{
				CryLogAlways("enabled float exceptions(sys_float_exceptions %d) this makes the performance slower.", g_cvars.sys_float_exceptions);
			}
		}
		m_env.pThreadManager->EnableFloatExceptions((EFPE_Severity)g_cvars.sys_float_exceptions);
	}

	InlineInitializationProcessing("CSystem::Init End");

#if defined(DEDICATED_SERVER)
	SCVarsClientConfigSink CVarsClientConfigSink;
	LoadConfiguration("client.cfg", &CVarsClientConfigSink, eLoadConfigInit);
#endif

#if CRY_PLATFORM_DURANGO
	if (startupParams.pLastPLMEvent)
	{
		switch (*startupParams.pLastPLMEvent)
		{
		case EPLMEV_ON_APP_ACTIVATED:
		case EPLMEV_ON_FULL:
		case EPLMEV_ON_RESUMING:
			m_env.ePLM_State = EPLM_RUNNING;
			break;

		case EPLMEV_ON_CONSTRAINED:
			m_env.ePLM_State = EPLM_CONSTRAINED;
			break;
		case EPLMEV_ON_SUSPENDING:
			m_env.ePLM_State = EPLM_SUSPENDED;
			break;
		}
	}
	CRY_ASSERT(m_env.ePLM_State != EPLM_UNDEFINED);
	if (m_env.ePLM_State == EPLM_UNDEFINED)
		m_env.ePLM_State = EPLM_RUNNING;
#endif

	// [VR]
	if (m_pHmdManager)
	{
		m_pHmdManager->SetupAction(IHmdManager::eHmdSetupAction_PostInit);
	}

	// All CVars should be registered by this point, we must now flush the cvar groups
	OnSysSpecChange(m_sys_spec);

#if CRY_PLATFORM_WINDOWS
	// This code works around a potential perf limit inside the engine due to excessive use of sleeps, etc. Increasing the system wide
	// timer resolution will cause the OS scheduler to respond more instantly. However, it's not recommended to enable this cvar by
	// default as it can increase CPU usage inside the kernel depending on the OS version, meaning adverse perf impact on lower end machines
	// with not enough cores, increased power consumption, etc.
	if (g_cvars.sys_highrestimer)
	{
		HMODULE hModNtDll = GetModuleHandleA("ntdll");
		if (hModNtDll)
		{
			typedef LONG (WINAPI * FP_NtQueryTimerResolution)(PULONG, PULONG, PULONG);
			FP_NtQueryTimerResolution pNtQueryTimerResolution = (FP_NtQueryTimerResolution) GetProcAddress(hModNtDll, "NtQueryTimerResolution");

			typedef LONG (WINAPI * FP_NtSetTimerResolution)(ULONG, BOOLEAN, PULONG);
			FP_NtSetTimerResolution pNtSetTimerResolution = (FP_NtSetTimerResolution) GetProcAddress(hModNtDll, "NtSetTimerResolution");

			if (pNtQueryTimerResolution && pNtSetTimerResolution)
			{
				// Resolution is defined in 100 ns units, the smaller the value the higher the precision.
				ULONG minRes = -1, maxRes = -1, curRes = -1;
				if (0 == pNtQueryTimerResolution(&minRes, &maxRes, &curRes))
				{
					ULONG originalRes = curRes;
					// If system timer isn't already configured to run in higher precision, we want 1 ms resolution or
					// (if not available on a particular machine) the highest possible resolution.
					ULONG newRes = maxRes < 10000 ? 10000 : maxRes;
					if (newRes < curRes)
					{
						// System will adjust to next higher supported resolution.
						if (0 == pNtSetTimerResolution(newRes, TRUE, &newRes))
						{
							curRes = newRes;
						}
					}
					gEnv->pLog->LogAlways("System timer configured to %.2f ms (was %.2f ms before)", curRes * 1e-4, originalRes * 1e-4);
				}
			}
		}
	}
#endif

	m_pManualFrameStepController = new CManualFrameStepController();

	if (startupParams.bShaderCacheGen)
	{
		GetIConsole()->ExecuteString("r_PrecacheShaderList");
	}

	return (true);
}

static void LoadConfigurationCmd(IConsoleCmdArgs* pParams)
{
	assert(pParams);

	if (pParams->GetArgCount() != 2)
	{
		gEnv->pLog->LogError("LoadConfiguration failed, one parameter needed");
		return;
	}

	ILoadConfigurationEntrySink* pCVarsWhiteListConfigSink = GetISystem()->GetCVarsWhiteListConfigSink();
	GetISystem()->LoadConfiguration(string("%engine%/Config/") + pParams->GetArg(1), pCVarsWhiteListConfigSink, eLoadConfigGame);
}

// --------------------------------------------------------------------------------------------------------------------------

static void _LvlRes_export_IResourceList(FILE* hFile, const ICryPak::ERecordFileOpenList& eList)
{
	IResourceList* pResList = gEnv->pCryPak->GetResourceList(eList);

	for (const char* filename = pResList->GetFirst(); filename; filename = pResList->GetNext())
	{
		CryPathString absPath;
		gEnv->pCryPak->AdjustFileName(filename, absPath, 0);
		gEnv->pCryPak->FPrintf(hFile, "%s\n", absPath.c_str());
	}
}

void LvlRes_export(IConsoleCmdArgs* pParams)
{
	// * this assumes the level was already loaded in the editor (resources have been recorded)
	// * it could be easily changed to run the launcher, start recording, load a level and quit (useful to autoexport many levels)

	const char* szLevelName = gEnv->pGameFramework->GetLevelName();
	char szAbsLevelPathBuf[512];
	gEnv->pGameFramework->GetAbsLevelPath(szAbsLevelPathBuf, sizeof(szAbsLevelPathBuf));

	if (!szAbsLevelPathBuf[0] || !szLevelName)
	{
		gEnv->pLog->LogError("Error: LvlRes_export no level loaded?");
		return;
	}

	string sPureLevelName = PathUtil::GetFile(szLevelName);   // level name without path

	// record all assets that might be loaded after level loading
	if (gEnv->pGameFramework)
		gEnv->pGameFramework->PrefetchLevelAssets(true);

	enum {nMaxPath = 0x800};
	char szAbsPathBuf[nMaxPath];

	cry_sprintf(szAbsPathBuf, "%s/%s%s", szAbsLevelPathBuf, sPureLevelName.c_str(), g_szLvlResExt);

	// Write resource list to file.
	FILE* hFile = gEnv->pCryPak->FOpen(szAbsPathBuf, "wt");

	if (!hFile)
	{
		gEnv->pLog->LogError("Error: LvlRes_export file open failed");
		return;
	}

	gEnv->pCryPak->FPrintf(hFile, "; this file can be safely deleted - it's only purpose is to produce a striped build (without unused assets)\n\n");

	char rootpath[_MAX_PATH];
	CryGetCurrentDirectory(sizeof(rootpath), rootpath);

	gEnv->pCryPak->FPrintf(hFile, "; EngineStartup\n");
	_LvlRes_export_IResourceList(hFile, ICryPak::RFOM_EngineStartup);
	gEnv->pCryPak->FPrintf(hFile, "; Level '%s'\n", szAbsLevelPathBuf);
	_LvlRes_export_IResourceList(hFile, ICryPak::RFOM_Level);

	gEnv->pCryPak->FClose(hFile);
}

// create all directories needed to represent the path, \ and / are handled
// currently no error checking
// (should be moved to PathUtil)
// Arguments:
//   szPath - e.g. "c:\temp/foldername1/foldername2"
static void CreateDirectoryPath(const char* szPath)
{
	const char* p = szPath;

	string sFolder;

	for (;;)
	{
		if (*p == '/' || *p == '\\' || *p == 0)
		{
			CryCreateDirectory(sFolder.c_str());

			if (*p == 0)
				break;

			sFolder += '\\';
			++p;
		}
		else
			sFolder += *p++;
	}
}

static string ConcatPath(const char* szPart1, const char* szPart2)
{
	if (szPart1[0] == 0)
		return szPart2;

	string ret;

	ret.reserve(strlen(szPart1) + 1 + strlen(szPart2));

	ret = szPart1;
	ret += "/";
	ret += szPart2;

	return ret;
}

class CLvlRes_base
{
public:

	// destructor
	virtual ~CLvlRes_base()
	{
	}

	void RegisterAllLevelPaks(const string& sPath)
	{
		_finddata_t fd;

		string sPathPattern = ConcatPath(sPath, "*.*");

		intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd);

		if (handle < 0)
		{
			gEnv->pLog->LogError("ERROR: CLvlRes_base failed '%s'", sPathPattern.c_str());
			return;
		}

		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
					RegisterAllLevelPaks(ConcatPath(sPath, fd.name));
			}
			else if (HasRightExtension(fd.name))     // open only the level paks if there is a LvlRes.txt, opening all would be too slow
			{
				OnPakEntry(sPath, fd.name);
			}

		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	void Process(const string& sPath)
	{
		_finddata_t fd;

		string sPathPattern = ConcatPath(sPath, "*.*");

		intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd);

		if (handle < 0)
		{
			gEnv->pLog->LogError("ERROR: LvlRes_finalstep failed '%s'", sPathPattern.c_str());
			return;
		}

		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
					Process(ConcatPath(sPath, fd.name));
			}
			else if (HasRightExtension(fd.name))
			{
				string sFilePath = ConcatPath(sPath, fd.name);

				gEnv->pLog->Log("CLvlRes_base processing '%s' ...", sFilePath.c_str());

				FILE* hFile = gEnv->pCryPak->FOpen(sFilePath.c_str(), "rb");

				if (hFile)
				{
					std::vector<char> vBuffer;

					size_t len = gEnv->pCryPak->FGetSize(hFile);
					vBuffer.resize(len + 1);

					if (len)
					{
						if (gEnv->pCryPak->FReadRaw(&vBuffer[0], len, 1, hFile) == 1)
						{
							vBuffer[len] = 0;                             // end terminator

							char* p = &vBuffer[0];

							while (*p)
							{
								while (*p != 0 && *p <= ' ')                   // jump over whitespace
									++p;

								char* pLineStart = p;

								while (*p != 0 && *p != 10 && *p != 13)          // goto end of line
									++p;

								char* pLineEnd = p;

								while (*p != 0 && (*p == 10 || *p == 13))        // goto next line with data
									++p;

								if (*pLineStart != ';')                      // if it's not a commented line
								{
									*pLineEnd = 0;
									OnFileEntry(pLineStart);        // add line
								}
							}
						}
						else gEnv->pLog->LogError("Error: LvlRes_finalstep file open '%s' failed", sFilePath.c_str());
					}

					gEnv->pCryPak->FClose(hFile);
				}
				else gEnv->pLog->LogError("Error: LvlRes_finalstep file open '%s' failed", sFilePath.c_str());
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	bool IsFileKnown(const char* szFilePath)
	{
		string sFilePath = szFilePath;

		return m_UniqueFileList.find(sFilePath) != m_UniqueFileList.end();
	}

protected: // -------------------------------------------------------------------------

	static bool HasRightExtension(const char* szFileName)
	{
		const char* szLvlResExt = szFileName;

		size_t lenName = strlen(szLvlResExt);
		static size_t lenLvlExt = strlen(g_szLvlResExt);

		if (lenName >= lenLvlExt)
			szLvlResExt += lenName - lenLvlExt;     // "test_LvlRes.txt" -> "_LvlRes.txt"

		return stricmp(szLvlResExt, g_szLvlResExt) == 0;
	}

	// Arguments
	//   sFilePath - e.g. "game/object/vehices/car01.dds"
	void OnFileEntry(const char* szFilePath)
	{
		string sFilePath = szFilePath;
		if (m_UniqueFileList.find(sFilePath) == m_UniqueFileList.end())    // to to file processing only once per file
		{
			m_UniqueFileList.insert(sFilePath);

			ProcessFile(sFilePath);

			gEnv->pLog->UpdateLoadingScreen(0);
		}
	}

	virtual void ProcessFile(const string& sFilePath) = 0;

	virtual void OnPakEntry(const string& sPath, const char* szPak) {}

	// -----------------------------------------------------------------

	std::set<string> m_UniqueFileList;              // to removed duplicate files
};

class CLvlRes_finalstep : public CLvlRes_base
{
public:

	// constructor
	CLvlRes_finalstep(const char* szPath) : m_sPath(szPath)
	{
		assert(szPath);
	}

	// destructor
	virtual ~CLvlRes_finalstep()
	{
		// free registered paks
		std::set<string>::iterator it, end = m_RegisteredPakFiles.end();

		for (it = m_RegisteredPakFiles.begin(); it != end; ++it)
		{
			string sName = *it;

			gEnv->pCryPak->ClosePack(sName.c_str());
		}
	}

	// register a pak file so all files within do not become file entries but the pak file becomes
	void RegisterPak(const string& sPath, const char* szFile)
	{
		string sPak = ConcatPath(sPath, szFile);

		gEnv->pCryPak->ClosePack(sPak.c_str());     // so we don't get error for paks that were already opened

		if (!gEnv->pCryPak->OpenPack(sPak.c_str()))
		{
			CryLog("RegisterPak '%s' failed - file not present?", sPak.c_str());
			return;
		}

		CryPathString absPath;
		gEnv->pCryPak->AdjustFileName(sPak, absPath, 0);

		CryLog("RegisterPak '%s'", absPath.c_str());

		m_RegisteredPakFiles.insert(string(absPath.c_str()));

		OnFileEntry(sPak);    // include pak as file entry
	}

	// finds a specific file
	static bool FindFile(const char* szFilePath, const char* szFile, _finddata_t& fd)
	{
		intptr_t handle = gEnv->pCryPak->FindFirst(szFilePath, &fd);

		if (handle < 0)
			return false;

		do
		{
			if (stricmp(fd.name, szFile) == 0)
			{
				gEnv->pCryPak->FindClose(handle);
				return true;
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd));

		gEnv->pCryPak->FindClose(handle);
		return false;
	}

	// slow but safe (to correct path and file name upper/lower case to the existing files)
	// some code might rely on the case (e.g. CVarGroup creation) so it's better to correct the case
	static void CorrectCaseInPlace(char* szFilePath)
	{
		// required for FindFirst, TODO: investigate as this seems wrong behavior
		{
			// jump over "Game"
			while (*szFilePath != '/' && *szFilePath != '\\' && *szFilePath != 0)
				++szFilePath;
			// jump over "/"
			if (*szFilePath != 0)
				++szFilePath;
		}

		char* szFile = szFilePath, * p = szFilePath;

		for (;;)
		{
			if (*p == '/' || *p == '\\' || *p == 0)
			{
				char cOldChar = *p;
				*p = 0;                         // create zero termination
				_finddata_t fd;

				bool bOk = FindFile(szFilePath, szFile, fd);

				if (bOk)
					assert(strlen(szFile) == strlen(fd.name));

				*p = cOldChar;                  // get back the old separator

				if (!bOk)
					return;

				memcpy((void*)szFile, fd.name, strlen(fd.name) + 1);   // set

				if (*p == 0)
					break;

				++p;
				szFile = p;
			}
			else ++p;
		}
	}

	virtual void ProcessFile(const string& _sFilePath)
	{
		string sFilePath = _sFilePath;

		CorrectCaseInPlace((char*)&sFilePath[0]);

		gEnv->pLog->LogWithType(ILog::eAlways, "LvlRes: %s", sFilePath.c_str());

		CCryFile file;
		std::vector<char> data;

		if (!file.Open(sFilePath.c_str(), "rb"))
		{
			OutputDebugString(">>>>> failed to open '");
			OutputDebugString(sFilePath.c_str());
			OutputDebugString("'\n");
			//			gEnv->pLog->LogError("ERROR: failed to open '%s'",sFilePath.c_str());			// pak not opened ?
			//			assert(0);
			return;
		}

		if (IsInRegisteredPak(file.GetHandle()))
			return;         // then don't process as we include the pak

		// Save this file in target folder.
		string trgFilename = PathUtil::Make(m_sPath, sFilePath);
		int fsize = file.GetLength();

		if (fsize > (int)data.size())
			data.resize(fsize + 16);

		// Read data.
		file.ReadRaw(&data[0], fsize);

		// Save this data to target file.
		string trgFileDir = PathUtil::ToDosPath(PathUtil::RemoveSlash(PathUtil::GetPathWithoutFilename(trgFilename)));

		CreateDirectoryPath(trgFileDir);      // ensure path exists

		// Create target file
		FILE* trgFile = fopen(trgFilename, "wb");

		if (trgFile)
		{
			fwrite(&data[0], fsize, 1, trgFile);
			fclose(trgFile);
		}
		else
		{
			gEnv->pLog->LogError("ERROR: failed to write '%s' (write protected/disk full/rights)", trgFilename.c_str());
			assert(0);
		}
	}

	bool IsInRegisteredPak(FILE* hFile)
	{
		const char* szPak = gEnv->pCryPak->GetFileArchivePath(hFile);

		if (!szPak)
			return false;     // outside pak

		bool bInsideRegisteredPak = m_RegisteredPakFiles.find(szPak) != m_RegisteredPakFiles.end();

		return bInsideRegisteredPak;
	}

	virtual void OnPakEntry(const string& sPath, const char* szPak)
	{
		RegisterPak(sPath, "level.pak");
		RegisterPak(sPath, "levelmm.pak");
	}

	// -------------------------------------------------------------------------------

	string           m_sPath;                       // directory path to store the assets e.g. "c:\temp\Out"
	std::set<string> m_RegisteredPakFiles;          // abs path to pak files we registered e.g. "c:\MasterCD\game\GameData.pak", to avoid processing files inside these pak files - the ones we anyway want to include
};

class CLvlRes_findunused : public CLvlRes_base
{
public:

	virtual void ProcessFile(const string& sFilePath)
	{
	}
};

static void LvlRes_finalstep(IConsoleCmdArgs* pParams)
{
	assert(pParams);

	uint32 dwCnt = pParams->GetArgCount();

	if (dwCnt != 2)
	{
		gEnv->pLog->LogWithType(ILog::eError, "ERROR: sys_LvlRes_finalstep requires destination path as parameter");
		return;
	}

	const char* szPath = pParams->GetArg(1);
	assert(szPath);

	gEnv->pLog->LogWithType(ILog::eInputResponse, "sys_LvlRes_finalstep %s ...", szPath);

	// open console
	gEnv->pConsole->ShowConsole(true);

	CLvlRes_finalstep sink(szPath);

	sink.RegisterPak(PathUtil::GetGameFolder(), "GameData.pak");
	sink.RegisterPak(PathUtil::GetGameFolder(), "Shaders.pak");

	sink.RegisterAllLevelPaks(PathUtil::GetGameFolder() + "/levels");
	sink.Process(PathUtil::GetGameFolder() + "/levels");
}

static void _LvlRes_findunused_recursive(CLvlRes_findunused& sink, const string& sPath,
                                         uint32& dwUnused, uint32& dwAll)
{
	_finddata_t fd;

	string sPathPattern = ConcatPath(sPath, "*.*");

	// ignore some directories
	if (stricmp(sPath.c_str(), "Shaders") == 0
	    || stricmp(sPath.c_str(), "ScreenShots") == 0
	    || stricmp(sPath.c_str(), "Scripts") == 0
	    || stricmp(sPath.c_str(), "Config") == 0
	    || stricmp(sPath.c_str(), "LowSpec") == 0)
		return;

	//	gEnv->pLog->Log("_LvlRes_findunused_recursive '%s'",sPath.c_str());

	intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd);

	if (handle < 0)
	{
		gEnv->pLog->LogError("ERROR: _LvlRes_findunused_recursive failed '%s'", sPathPattern.c_str());
		return;
	}

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
				_LvlRes_findunused_recursive(sink, ConcatPath(sPath, fd.name), dwUnused, dwAll);
		}
		else
		{
			/*
			   // ignore some extensions
			   if(stricmp(PathUtil::GetExt(fd.name),"cry")==0)
			   continue;

			   // ignore some files
			   if(stricmp(fd.name,"TerrainTexture.pak")==0)
			   continue;
			 */

			string sFilePath = CryStringUtils::toLower(ConcatPath(sPath, fd.name));
			
			CryPathString absPath;
			gEnv->pCryPak->AdjustFileName(sFilePath.c_str(), absPath, 0);

			if (!sink.IsFileKnown(absPath))
			{
				gEnv->pLog->LogWithType(IMiniLog::eAlways, "%d, %s", (uint32)fd.size, absPath.c_str());
				++dwUnused;
			}
			++dwAll;
		}
	}
	while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

	gEnv->pCryPak->FindClose(handle);
}

static void LvlRes_findunused(IConsoleCmdArgs* pParams)
{
	assert(pParams);

	gEnv->pLog->LogWithType(ILog::eInputResponse, "sys_LvlRes_findunused ...");

	// open console
	gEnv->pConsole->ShowConsole(true);

	CLvlRes_findunused sink;

	sink.RegisterAllLevelPaks(PathUtil::GetGameFolder() + "/levels");
	sink.Process(PathUtil::GetGameFolder() + "/levels");

	gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
	gEnv->pLog->LogWithType(ILog::eInputResponse, "Assets not used by the existing LvlRes data:");
	gEnv->pLog->LogWithType(ILog::eInputResponse, " ");

	char rootpath[_MAX_PATH];
	CryGetCurrentDirectory(sizeof(rootpath), rootpath);

	gEnv->pLog->LogWithType(ILog::eInputResponse, "Folder: %s", rootpath);

	uint32 dwUnused = 0, dwAll = 0;

	string unused;
	_LvlRes_findunused_recursive(sink, unused, dwUnused, dwAll);

	gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
	gEnv->pLog->LogWithType(ILog::eInputResponse, "Unused assets: %d/%d", dwUnused, dwAll);
	gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
}

// --------------------------------------------------------------------------------------------------------------------------

static void RecordClipCmd(IConsoleCmdArgs* pArgs)
{
	// "Usage: RecordClipCmd <exec/config> <time before> <time after> <local backup (backup/no_backup)> <annotation text>"
	// Params:
	// 0 - RecordClipCmd
	// 1 - record the clip (exec) or configure its parameters (config), 2 - time to record before the command, 3 - time to record after the command,
	// 4 - back up to HDD (backup/no_backup), 5 to end - clip description
	if (IPlatformOS::IClipCaptureOS* pClipCapture = gEnv->pSystem->GetPlatformOS()->GetClipCapture())
	{
#define DEFAULT_CLIP_DESC L"Alt F11 Clip"

		const size_t MAX_WSTR_BUFFER = 64;

		// make sure the description string has always a reasonable size and the default string is withing size range
		static_assert(CRY_ARRAY_COUNT(DEFAULT_CLIP_DESC) <= MAX_WSTR_BUFFER, "Invalid string length!");
		static_assert(MAX_WSTR_BUFFER >= 8 && MAX_WSTR_BUFFER <= 64, "Invalid buffer size!");

		// parameters to keep
		static CryFixedWStringT<MAX_WSTR_BUFFER> sParamFixedWStrDescription(DEFAULT_CLIP_DESC);
		static float sParamTimeAfter = 0.f, sParamTimeBefore = 10.f;
		static bool sParamDoBackUp = true;

		const uint32 argCount = pArgs->GetArgCount();

		bool bShouldRecordClip = true;

		// check argument count validity
		if (argCount > 2 && argCount < 6)
		{
			gEnv->pLog->LogWithType(ILog::eError, "RecordClipCmd requires either no parameters, the first parameter or all of them (5)'.\nYou entered: [%u]", argCount - 1);
			return;
		}

		// command type (exec/config)
		if (argCount > 1)
		{
			bShouldRecordClip = stack_string(pArgs->GetArg(1)).MakeLower() == "exec";
			if (bShouldRecordClip == false && stack_string(pArgs->GetArg(1)).MakeLower() != "config")
			{
				gEnv->pLog->LogWithType(ILog::eError, "RecordClipCmd requires that if used with parameters the first one should be either 'exec' or 'config'.\nYou entered: [%s]", pArgs->GetArg(1));
				return;
			}
		}

		// configuration parameters
		if (argCount > 2)
		{
			// time line
			sParamTimeBefore = float(atoi(pArgs->GetArg(2)));
			sParamTimeAfter = float(atoi(pArgs->GetArg(3)));

			// backup
			const bool b = stack_string(pArgs->GetArg(4)).MakeLower() == "backup";
			if (b == false && stack_string(pArgs->GetArg(4)).MakeLower() != "no_backup")
			{
				gEnv->pLog->LogWithType(ILog::eError, "RecordClipCmd requires the backup parameter to be either 'backup' or 'no_backup'.\nYou entered: [%s]", pArgs->GetArg(4));
				return;
			}
			sParamDoBackUp = b;
		}

		// clip description
		stack_string strClipDescription;
		for (uint32 i = 5; i < argCount; ++i)
		{
			if (i > 5)
				strClipDescription += " ";

			strClipDescription += pArgs->GetArg(i);
		}

		if (strClipDescription.empty() == false)
		{
			stack_string subStr = strClipDescription.substr(0, MAX_WSTR_BUFFER - 1);
			sParamFixedWStrDescription = CryStringUtils::UTF8ToWStr(subStr.c_str()).c_str();
		}

		// record clip or display configuration
		if (bShouldRecordClip)
		{
			IPlatformOS::IClipCaptureOS::SSpan span(sParamTimeBefore, sParamTimeAfter);
			IPlatformOS::IClipCaptureOS::SClipTextInfo clipTextInfo("RecordClipCmd", sParamFixedWStrDescription.c_str());
			pClipCapture->RecordClip(clipTextInfo, span, NULL, sParamDoBackUp);
		}
		else
		{
			gEnv->pLog->LogWithType(ILog::eMessage, "RecordClipCmd params: before[%.2f] after[%.2f] backUp[%s] desc[%ls]", sParamTimeBefore, sParamTimeAfter, sParamDoBackUp ? "True" : "False", sParamFixedWStrDescription.c_str());
		}
#undef DEFAULT_CLIP_DESC
	}
}

static void ScreenshotCmd(IConsoleCmdArgs* pParams)
{
	assert(pParams);

	uint32 dwCnt = pParams->GetArgCount();

	if (dwCnt <= 1)
	{
		if (!gEnv->IsEditing())
		{
			// open console one line only

			//line should lie within title safe area, so calculate overscan border
			Vec2 overscanBorders = Vec2(0.0f, 0.0f);
			gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorders);
			float yDelta = /*((float)gEnv->pRenderer->GetHeight())*/ 600.0f * overscanBorders.y;

			//set console height depending on top/bottom overscan border
			gEnv->pConsole->ShowConsole(true, (int)(16 + yDelta));
			gEnv->pConsole->SetInputLine("Screenshot ");
		}
		else
		{
			gEnv->pLog->LogWithType(ILog::eInputResponse, "Screenshot <annotation> missing - no screenshot was done");
		}
	}
	else
	{
		static int iScreenshotNumber = -1;

		const char* szPrefix = "Screenshot";
		uint32 dwPrefixSize = strlen(szPrefix);

		
		CryPathString path;
		gEnv->pCryPak->AdjustFileName("%USER%/ScreenShots", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);

		if (iScreenshotNumber == -1)   // first time - find max number to start
		{
			ICryPak* pCryPak = gEnv->pCryPak;
			_finddata_t fd;

			intptr_t handle = pCryPak->FindFirst((path + "/*.*"), &fd);   // mastercd folder
			if (handle != -1)
			{
				int res = 0;
				do
				{
					int iCurScreenshotNumber;

					if (strnicmp(fd.name, szPrefix, dwPrefixSize) == 0)
					{
						int iCnt = sscanf(fd.name + 10, "%d", &iCurScreenshotNumber);

						if (iCnt)
							iScreenshotNumber = max(iCurScreenshotNumber, iScreenshotNumber);
					}

					res = pCryPak->FindNext(handle, &fd);
				}
				while (res >= 0);
				pCryPak->FindClose(handle);
			}
		}

		++iScreenshotNumber;

		CryPathString sScreenshotName;
		sScreenshotName.Format("%s%.4d ", szPrefix, iScreenshotNumber);

		for (uint32 dwI = 1; dwI < dwCnt; ++dwI)
		{
			if (dwI > 1)
				sScreenshotName += '_';

			sScreenshotName += pParams->GetArg(dwI);
		}
		sScreenshotName.replace('\\', '_');
		sScreenshotName.replace('/',  '_');
		sScreenshotName.replace(':',  '_');

		const char* pExtension = PathUtil::GetExt(sScreenshotName);

		if (stricmp("jpg", pExtension) != 0 && stricmp("tga", pExtension) != 0)
		{
			sScreenshotName += ".jpg";
		}

		gEnv->pConsole->ShowConsole(false);

		CSystem* pCSystem = (CSystem*)(gEnv->pSystem);
		pCSystem->GetDelayedScreeenshot() = (path + '/' + sScreenshotName).c_str();// to delay a screenshot call for a frame
	}
}

static void SysRestoreSpecCmd(IConsoleCmdArgs* pParams)
{
	assert(pParams);

	if (pParams->GetArgCount() == 2)
	{
		const char* szArg = pParams->GetArg(1);

		ICVar* pCVar = gEnv->pConsole->GetCVar("sys_spec_Full");

		if (!pCVar)
		{
			gEnv->pLog->LogWithType(ILog::eInputResponse, "sys_RestoreSpec: no action");   // e.g. running Editor in shder compile mode
			return;
		}

		ICVar::EConsoleLogMode mode = ICVar::eCLM_Off;

		if (stricmp(szArg, "test") == 0)
			mode = ICVar::eCLM_ConsoleAndFile;
		else if (stricmp(szArg, "test*") == 0)
			mode = ICVar::eCLM_FileOnly;
		else if (stricmp(szArg, "info") == 0)
			mode = ICVar::eCLM_FullInfo;

		if (mode != ICVar::eCLM_Off)
		{
			bool bFileOrConsole = (mode == ICVar::eCLM_FileOnly || mode == ICVar::eCLM_FullInfo);

			if (bFileOrConsole)
				gEnv->pLog->LogToFile(" ");
			else
				CryLog(" ");

			int iSysSpec = pCVar->GetRealIVal();

			if (iSysSpec == -1)
			{
				iSysSpec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();

				if (bFileOrConsole)
					gEnv->pLog->LogToFile("   sys_spec = Custom (assuming %d)", iSysSpec);
				else
					gEnv->pLog->LogWithType(ILog::eInputResponse, "   $3sys_spec = $6Custom (assuming %d)", iSysSpec);
			}
			else
			{
				if (bFileOrConsole)
					gEnv->pLog->LogToFile("   sys_spec = %d", iSysSpec);
				else
					gEnv->pLog->LogWithType(ILog::eInputResponse, "   $3sys_spec = $6%d", iSysSpec);
			}

			pCVar->DebugLog(iSysSpec, mode);

			if (bFileOrConsole)
				gEnv->pLog->LogToFile(" ");
			else
				gEnv->pLog->LogWithType(ILog::eInputResponse, " ");

			return;
		}
		else if (strcmp(szArg, "apply") == 0)
		{
			const char* szPrefix = "sys_spec_";

			ESystemConfigSpec originalSpec = CONFIG_CUSTOM;
			if (gEnv->IsEditor())
			{
				originalSpec = gEnv->pSystem->GetConfigSpec(true);
			}

			std::vector<const char*> cmds;

			cmds.resize(gEnv->pConsole->GetSortedVars(0, 0, szPrefix));
			gEnv->pConsole->GetSortedVars(&cmds[0], cmds.size(), szPrefix);

			gEnv->pLog->LogWithType(IMiniLog::eInputResponse, " ");

			std::vector<const char*>::const_iterator it, end = cmds.end();

			for (it = cmds.begin(); it != end; ++it)
			{
				const char* szName = *it;

				if (stricmp(szName, "sys_spec_Full") == 0)
					continue;

				pCVar = gEnv->pConsole->GetCVar(szName);
				assert(pCVar);

				if (!pCVar)
					continue;

				bool bNeeded = pCVar->GetIVal() != pCVar->GetRealIVal();

				gEnv->pLog->LogWithType(IMiniLog::eInputResponse, " $3%s = $6%d ... %s",
				                        szName, pCVar->GetIVal(),
				                        bNeeded ? "$4restored" : "valid");

				if (bNeeded)
					pCVar->Set(pCVar->GetIVal());
			}

			gEnv->pLog->LogWithType(IMiniLog::eInputResponse, " ");

			if (gEnv->IsEditor())
			{
				gEnv->pSystem->SetConfigSpec(originalSpec, true);
			}
			return;
		}
	}

	gEnv->pLog->LogWithType(ILog::eInputResponse, "ERROR: sys_RestoreSpec invalid arguments");
}

void ChangeLogAllocations(ICVar* pVal)
{
	g_iTraceAllocations = pVal->GetIVal();
#if CRY_PLATFORM_WINDOWS
	((DebugCallStack*)IDebugCallStack::instance())->SetMemLogFile(g_iTraceAllocations == 2, "memallocfile.txt");
#endif
}

static void VisRegTest(IConsoleCmdArgs* pParams)
{
	CSystem* pCSystem = static_cast<CSystem*>(gEnv->pSystem);
	CVisRegTest*& visRegTest = pCSystem->GetVisRegTestPtrRef();
	if (!visRegTest)
		visRegTest = new CVisRegTest();

	visRegTest->Init(pParams);
}

void CSystem::WatchDogTimeOutChanged(ICVar* pCVar)
{
	CSystem* pCSystem = static_cast<CSystem*>(gEnv->pSystem);
	int val = pCVar->GetIVal();
	if (val > 0)
	{
		if (pCSystem->m_pWatchdog == nullptr)
		{
			pCSystem->m_pWatchdog = new CWatchdogThread(val);
		}
		else
		{
			pCSystem->m_pWatchdog->SetTimeout(val);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CreateSystemVars()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Create System CVars");

	assert(gEnv);
	assert(gEnv->pConsole);

	// Register DLL names as cvars before we load them
	//
	EVarFlags dllFlags = (EVarFlags)0;
	m_sys_dll_ai = REGISTER_STRING("sys_dll_ai", DLL_AI, dllFlags, "Specifies the DLL to load for the AI system");

	m_sys_dll_response_system = REGISTER_STRING("sys_dll_response_system", "CryDynamicResponseSystem", dllFlags, "Specifies the DLL to load for the dynamic response system");

	g_cvars.sys_build_folder = REGISTER_STRING("sys_build_folder", "", 0, "Optionally specifies external full path to the build folder to read pak files from. Can be a full path to an external folder or a relative path to a folder inside of the local build.");

	m_sys_initpreloadpacks = REGISTER_STRING("sys_initpreloadpacks", "", 0, "Specifies the paks for an engine initialization");
	m_sys_menupreloadpacks = REGISTER_STRING("sys_menupreloadpacks", 0, 0, "Specifies the paks for a main menu loading");

	m_sys_user_folder = REGISTER_STRING("sys_user_folder", "", 0, "Specifies the name of the user folder inside the 'Users/<username>/Saved Games/' folder, otherwise if left blank the User folder will be stored inside the root.");

#ifndef _RELEASE
	m_sys_resource_cache_folder = REGISTER_STRING("sys_resource_cache_folder", "Editor\\ResourceCache", 0, "Folder for resource compiled locally. Managed by Sandbox.");
#endif

	REGISTER_INT("cvDoVerboseWindowTitle", 0, VF_NULL, "");

	//TODO this cvar should be replaced by fixed shutdown logic considering the particularities of each platform
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX
	if (m_env.IsEditor())
	{
		// in editor we must exit on quit.
		m_pCVarQuit = REGISTER_INT("ExitOnQuit", 1, VF_NULL, "");
	}
	else
	{
		m_pCVarQuit = REGISTER_INT("ExitOnQuit", 0, VF_NULL, "");
		REGISTER_COMMAND("quit", "System.Quit()", VF_RESTRICTEDMODE, "Quit/Shutdown the engine");
	}
#else
	m_pCVarQuit = REGISTER_INT("ExitOnQuit", 1, VF_NULL, "");
#endif

#ifndef _RELEASE
	REGISTER_STRING_CB("sys_version", "", VF_CHEAT, "Override system file/product version", SystemVersionChanged);
#endif // #ifndef _RELEASE

	m_cvAIUpdate = REGISTER_INT("ai_NoUpdate", 0, VF_CHEAT, "Disables AI system update when 1");

	REGISTER_INT("r_AllowLiveMoCap", 0, VF_CHEAT, "Offers the LiveCreate MoCap Editor on Editor Startup when 1");

	m_iTraceAllocations = g_iTraceAllocations;
	REGISTER_CVAR2_CB("sys_logallocations", &m_iTraceAllocations, m_iTraceAllocations, VF_DUMPTODISK, "Save allocation call stack", ChangeLogAllocations);

	m_cvMemStats = REGISTER_INT("MemStats", 0, 0,
	                            "0/x=refresh rate in milliseconds\n"
	                            "Use 1000 to switch on and 0 to switch off\n"
	                            "Usage: MemStats [0..]");
	m_cvMemStatsThreshold = REGISTER_INT("MemStatsThreshold", 32000, VF_NULL, "");
	m_cvMemStatsMaxDepth = REGISTER_INT("MemStatsMaxDepth", 4, VF_NULL, "");

	if (m_env.IsEditor())
	{
		// In Editor our Pak priority is always 0
		g_cvars.pakVars.nPriority = ePakPriorityFileFirst;
	}

	attachVariable("sys_PakPriority", &g_cvars.pakVars.nPriority, "If set to 1, tells CryPak to try to open the file in pak first, then go to file system", VF_READONLY | VF_CHEAT);
	attachVariable("sys_PakReadSlice", &g_cvars.pakVars.nReadSlice, "If non-0, means number of kilobytes to use to read files in portions. Should only be used on Win9x kernels");
	attachVariable("sys_PakLogMissingFiles", &g_cvars.pakVars.nLogMissingFiles,
	               "If non-0, missing file names go to mastercd/MissingFilesX.log.\n"
	               "1) only resulting report\n"
	               "2) run-time report is ON, one entry per file\n"
	               "3) full run-time report");

	attachVariable("sys_PakInMemorySizeLimit", &g_cvars.pakVars.nInMemoryPerPakSizeLimit, "Individual pak size limit for being loaded into memory (MB)");
	attachVariable("sys_PakTotalInMemorySizeLimit", &g_cvars.pakVars.nTotalInMemoryPakSizeLimit, "Total limit (in MB) for all in memory paks");
	attachVariable("sys_PakLoadCache", &g_cvars.pakVars.nLoadCache, "Load in memory paks from _LoadCache folder");
	attachVariable("sys_PakLoadModePaks", &g_cvars.pakVars.nLoadModePaks, "Load mode switching paks from modes folder");
	attachVariable("sys_PakStreamCache", &g_cvars.pakVars.nStreamCache, "Load in memory paks for faster streaming (cgf_cache.pak,dds_cache.pak)");
	attachVariable("sys_PakSaveTotalResourceList", &g_cvars.pakVars.nSaveTotalResourceList, "Save resource list");
	attachVariable("sys_PakSaveLevelResourceList", &g_cvars.pakVars.nSaveLevelResourceList, "Save resource list when loading level");
	attachVariable("sys_PakSaveFastLoadResourceList", &g_cvars.pakVars.nSaveFastloadResourceList, "Save resource list during initial loading");
	attachVariable("sys_PakSaveMenuCommonResourceList", &g_cvars.pakVars.nSaveMenuCommonResourceList, "Save resource list during front end menu flow");
	attachVariable("sys_PakMessageInvalidFileAccess", &g_cvars.pakVars.nMessageInvalidFileAccess, "Message Box synchronous file access when in game");
	attachVariable("sys_PakLogInvalidFileAccess", &g_cvars.pakVars.nLogInvalidFileAccess, "Log synchronous file access when in game");
#ifndef _RELEASE
	attachVariable("sys_PakLogAllFileAccess", &g_cvars.pakVars.nLogAllFileAccess, "Log all file access allowing you to easily see whether a file has been loaded directly, or which pak file.");
#endif
	attachVariable("sys_PakValidateFileHash", &g_cvars.pakVars.nValidateFileHashes, "Validate file hashes in pak files for collisions");
	attachVariable("sys_LoadFrontendShaderCache", &g_cvars.pakVars.nLoadFrontendShaderCache, "Load frontend shader cache (on/off)");
	attachVariable("sys_UncachedStreamReads", &g_cvars.pakVars.nUncachedStreamReads, "Enable stream reads via an uncached file handle");
	attachVariable("sys_PakDisableNonLevelRelatedPaks", &g_cvars.pakVars.nDisableNonLevelRelatedPaks, "Disables all paks that are not required by specific level; This is used with per level splitted assets.");

	REGISTER_CVAR2("sys_intromoviesduringinit", &g_cvars.sys_intromoviesduringinit, 0, VF_NULL, "Render the intro movies during game initialization");

#ifndef CRY_PLATFORM_ORBIS
	g_cvars.sys_splashscreen = REGISTER_STRING("sys_splashscreen", "", 0, "Specifies the path to the splashscreen texture to render at startup");
#else
	g_cvars.sys_splashscreen = nullptr;
#endif

	REGISTER_CVAR2("sys_filesystemCaseSensitivity", &g_cvars.sys_filesystemCaseSensitivity, 0, VF_NULL, "0 = Ignore letter casing mismatches, 1 = Show warning on mismatch, 2 = Show error on mismatch");

	m_sysNoUpdate = REGISTER_INT("sys_noupdate", 0, VF_CHEAT,
	                             "Toggles updating of system with sys_script_debugger.\n"
	                             "Usage: sys_noupdate [0/1]\n"
	                             "Default is 0 (system updates during debug).");

	m_sysWarnings = REGISTER_INT("sys_warnings", 0, 0,
	                             "Toggles printing system warnings.\n"
	                             "Usage: sys_warnings [0/1]\n"
	                             "Default is 0 (off).");

#if defined(_RELEASE) && !CRY_PLATFORM_DESKTOP && !defined(ENABLE_LW_PROFILERS)
	enum {e_sysKeyboardDefault = 0};
#else
	enum {e_sysKeyboardDefault = 1};
#endif
	m_sysKeyboard = REGISTER_INT("sys_keyboard", e_sysKeyboardDefault, 0,
	                             "Enables keyboard.\n"
	                             "Usage: sys_keyboard [0/1]\n"
	                             "Default is 1 (on).");

	m_svDedicatedMaxRate = REGISTER_FLOAT("sv_DedicatedMaxRate", 30.0f, 0,
	                                      "Sets the maximum update rate when running as a dedicated server.\n"
	                                      "Usage: sv_DedicatedMaxRate [5..500]\n"
	                                      "Default is 30.");

	REGISTER_FLOAT("sv_DedicatedCPUPercent", 0.0f, 0,
	               "Sets the target CPU usage when running as a dedicated server, or disable this feature if it's zero.\n"
	               "Usage: sv_DedicatedCPUPercent [0..100]\n"
	               "Default is 0 (disabled).");
	REGISTER_FLOAT("sv_DedicatedCPUVariance", 10.0f, 0,
	               "Sets how much the CPU can vary from sv_DedicateCPU (up or down) without adjusting the framerate.\n"
	               "Usage: sv_DedicatedCPUVariance [5..50]\n"
	               "Default is 10.");

	m_svAISystem = REGISTER_INT("sv_AISystem", 1, VF_REQUIRE_APP_RESTART, "Load and use the AI system on the server");

	m_clAISystem = REGISTER_INT("cl_AISystem", 0, VF_REQUIRE_APP_RESTART, "Load and use the AI system on the client");

	m_cvSSInfo = REGISTER_INT("sys_SSInfo", 0, 0,
	                          "Show SourceSafe information (Name,Comment,Date) for file errors."
	                          "Usage: sys_SSInfo [0/1]\n"
	                          "Default is 0 (off)");

	m_cvEntitySuppressionLevel = REGISTER_INT("e_EntitySuppressionLevel", 0, 0,
	                                          "Defines the level at which entities are spawned.\n"
	                                          "Entities marked with lower level will not be spawned - 0 means no level.\n"
	                                          "Usage: e_EntitySuppressionLevel [0-infinity]\n"
	                                          "Default is 0 (off)");

	m_sys_profile_watchdog_timeout = REGISTER_INT_CB("watchdog", 0, VF_NULL,
	                                                 "Set time out in seconds (positive) to start watching over game freezes", WatchDogTimeOutChanged);
	m_sys_job_system_filter = REGISTER_STRING("sys_job_system_filter", "", 0,
	                                          "Filters a Job.\n"
	                                          "Usage: sys_job_system_filter name1,name2,..\n"
	                                          "Where 'name' refers to the exact name of the job, 0 disables it\n"
	                                          "More than one job can be specified by using a comma separated list");
	m_sys_job_system_enable = REGISTER_INT("sys_job_system_enable", 1, 0,
	                                       "Enable the JobSystem.\n"
	                                       "Usage: sys_job_system_enable 0/1\n"
	                                       "0: The Jobsystem is disabled, each job is executed in its invoking thread.\n"
	                                       "1: The JobSystem is enabled, each job is invoked in one of the worker threads.");
	m_sys_job_system_profiler = REGISTER_INT("sys_job_system_profiler", 0, 0,
	                                         "Enable the job system profiler.\n"
	                                         "Usage: sys_job_system_profiler <value>\n"
	                                         "0: Disable the profiler\n"
	                                         "1: Show the full profiler\n"
	                                         "2: Show only the execution graph\n");
#if CRY_PLATFORM_CONSOLE || CRY_PLATFORM_MOBILE
	const uint32 nJobSystemDefaultCoreNumber = 4;
#else
	const uint32 nJobSystemDefaultCoreNumber = 8;
#endif
	m_sys_job_system_max_worker = REGISTER_INT("sys_job_system_max_worker", nJobSystemDefaultCoreNumber, 0,
	                                           "Sets the number of threads to use for the job system"
	                                           "Defaults to 4 on consoles and 8 threads an PC"
	                                           "Set to 0 to create as many threads as cores are available");

	m_sys_job_system_worker_boost_enabled = REGISTER_INT("sys_job_system_worker_boost_enabled", 1, 0,
	                                                     "Kicks off anadditional worker thread when the Main/Render-Thread have to wait on a job state");

	REGISTER_COMMAND("sys_job_system_dump_job_list", CmdDumpJobManagerJobList, VF_CHEAT, "Show a list of all registered job in the console");
	REGISTER_COMMAND("sys_dump_cvars", CmdDumpCvars, VF_CHEAT, "Dump all cvars to file");

	REGISTER_CVAR2("MemInfo", &profile_meminfo, 0, 0, "Display memory information by modules\n1=on, 0=off");

	m_sys_spec = REGISTER_INT_CB("sys_spec", CONFIG_CUSTOM, VF_ALWAYSONCHANGE,    // starts with CONFIG_CUSTOM so callback is called when setting initial value
	                             "Tells the system cfg spec. (0=custom, 1=low, 2=med, 3=high, 4=very high, 5=XBoxOne, 6=PS4)",
	                             OnSysSpecChange);

	m_sys_SimulateTask = REGISTER_INT("sys_SimulateTask", 0, 0,
	                                  "Simulate a task in System:Update which takes X ms");

	m_sys_firstlaunch = REGISTER_INT("sys_firstlaunch", 0, 0,
	                                 "Indicates that the game was run for the first time.");
	//if physics thread is excluded all locks inside are mapped to NO_LOCK
	//var must be not visible to accidentally get enabled
#if defined(EXCLUDE_PHYSICS_THREAD)
	m_sys_physics_enable_MT = REGISTER_INT("sys_physics_enable_MT", 0, 0,
	                                       "Specifies if the physics thread should run in parallel to the main thread");
#else
	m_sys_physics_enable_MT = REGISTER_INT("sys_physics_enable_MT", 1, 0,
	                                       "Specifies if the physics thread should run in parallel to the main thread");
#endif

	m_sys_min_step = REGISTER_FLOAT("sys_min_step", 0.01f, 0,
	                                "Specifies the minimum physics step in a separate thread");
	m_sys_max_step = REGISTER_FLOAT("sys_max_step", 0.05f, 0,
	                                "Specifies the maximum physics step in a separate thread");

	m_sys_enable_budgetmonitoring = REGISTER_INT("sys_enable_budgetmonitoring", 0, 0,
	                                             "Enables budget monitoring. Use #System.SetBudget( sysMemLimitInMB, videoMemLimitInMB,\n"
	                                             "frameTimeLimitInMS, soundChannelsPlaying ) or sys_budget_sysmem, sys_budget_videomem\n"
	                                             "or sys_budget_fps to set budget limits.");

	// used in define MEMORY_DEBUG_POINT()
	m_sys_memory_debug = REGISTER_INT("sys_memory_debug", 0, VF_CHEAT,
	                                  "Enables to activate low memory situation is specific places in the code (argument defines which place), 0=off");

	REGISTER_CVAR2("sys_streaming_memory_budget", &g_cvars.sys_streaming_memory_budget, 10 * 1024, VF_NULL, "Temp memory streaming system can use in KB");
	REGISTER_CVAR2("sys_streaming_max_finalize_per_frame", &g_cvars.sys_streaming_max_finalize_per_frame, 0, VF_NULL,
	               "Maximum stream finalizing calls per frame to reduce the CPU impact on main thread (0 to disable)");
	REGISTER_CVAR2("sys_streaming_max_bandwidth", &g_cvars.sys_streaming_max_bandwidth, 0, VF_NULL, "Enables capping of max streaming bandwidth in MB/s");
	REGISTER_CVAR2("sys_streaming_debug", &g_cvars.sys_streaming_debug, 0, VF_NULL, "Enable streaming debug information\n"
	                                                                                "0=off\n"
	                                                                                "1=Streaming Stats\n"
	                                                                                "2=File IO\n"
	                                                                                "3=Request Order\n"
	                                                                                "4=Write to Log\n"
	                                                                                "5=Stats per extension\n"
	               );
	REGISTER_CVAR2("sys_streaming_requests_grouping_time_period", &g_cvars.sys_streaming_requests_grouping_time_period, 2, VF_NULL, // Vlad: 2 works better than 4 visually, should be be re-tested when streaming pak's activated
	               "Streaming requests are grouped by request time and then sorted by disk offset");
	REGISTER_CVAR2("sys_streaming_debug_filter", &g_cvars.sys_streaming_debug_filter, 0, VF_NULL, "Set streaming debug information filter.\n"
	                                                                                              "0=all\n"
	                                                                                              "1=Texture\n"
	                                                                                              "2=Geometry\n"
	                                                                                              "3=Terrain\n"
	                                                                                              "4=Animation\n"
	                                                                                              "7=Sound\n"
	                                                                                              "8=Shader\n"
	               );
	g_cvars.sys_streaming_debug_filter_file_name = REGISTER_STRING("sys_streaming_debug_filter_file_name", "", VF_CHEAT,
	                                                               "Set streaming debug information filter");
	REGISTER_CVAR2("sys_streaming_debug_filter_min_time", &g_cvars.sys_streaming_debug_filter_min_time, 0.f, VF_NULL, "Show only slow items.");
	REGISTER_CVAR2("sys_streaming_resetstats", &g_cvars.sys_streaming_resetstats, 0, VF_NULL,
	               "Reset all the streaming stats");

	REGISTER_CVAR2("sys_streaming_use_optical_drive_thread", &g_cvars.sys_streaming_use_optical_drive_thread, 0, VF_NULL,
	               "Allow usage of an extra optical drive thread for faster streaming from 2 medias");

	g_cvars.sys_localization_folder = REGISTER_STRING_CB("sys_localization_folder", "localization", VF_NULL,
	                                                     "Sets the folder where to look for localized data.\n"
	                                                     "This cvar allows for backwards compatibility so localized data under the game folder can still be found.\n"
	                                                     "Usage: sys_localization_folder <folder name>\n"
	                                                     "Default: Localization\n",
	                                                     CSystem::OnLocalizationFolderCVarChanged);

	g_cvars.sys_localization_pak_suffix = REGISTER_STRING("sys_localization_pak_suffix", "_xml", VF_CHEAT,
	                                                      "Suffix added to the language name to form the filename of the localization pak.\n"
	                                                      "Default is _xml");

	REGISTER_CVAR2("sys_streaming_in_blocks", &g_cvars.sys_streaming_in_blocks, 1, VF_NULL,
	               "Streaming of large files happens in blocks");
#if defined(USE_FPE)
	REGISTER_CVAR2("sys_float_exceptions", &g_cvars.sys_float_exceptions, 0, 0,
	               "Floating Point Exceptions:\n"
	               "  0 = Disabled\n"
	               "  1 = Basic [ZERODIVIDE, INVALID] \n"
	               "  2 = Full  [ZERODIVIDE, INVALID, OVERFLOW, UNDERFLOW] \n\n"
	               "Explanation:\n"
	               "  ZERODIVIDE: An exact infinite result is produced by an operation on finite operands. E.g. x/0, log(0)\n"
	               "  INVALID: An operand is invalid for the operation about to be performed. E.g. 0/0, NaN/Nan, 0xNan, sqrt(-1)\n"
	               "  OVERFLOW: The result would be larger than the largest finite number representable in the destination format. E.g (float)DBL_MAX, FLT_MAX + 1.0e32, expf(88.8)\n"
	               "  UNDERFLOW: The result would be smaller than the smallest normal number representable in the destination format. E.g (float)DBL_MIN, nextafterf(FLT_MIN, -), expf(-87.4)\n");
#else
	g_cvars.sys_float_exceptions = 0;
#endif
#undef CVAR_FPE_DEFAULT_VALUE

	REGISTER_CVAR2("sys_update_profile_time", &g_cvars.sys_update_profile_time, 1.0f, 0, "Time to keep updates timings history for.");
	REGISTER_CVAR2("sys_no_crash_dialog", &g_cvars.sys_no_crash_dialog, m_bNoCrashDialog, VF_NULL, "");
#if !defined(DEDICATED_SERVER) && defined(_RELEASE)
	REGISTER_CVAR2("sys_WER", &g_cvars.sys_WER, 1, 0, "Enables Windows Error Reporting");
#else
	REGISTER_CVAR2("sys_WER", &g_cvars.sys_WER, 0, 0, "Enables Windows Error Reporting");
#endif

#ifdef USE_HTTP_WEBSOCKETS
	REGISTER_CVAR2("sys_simple_http_base_port", &g_cvars.sys_simple_http_base_port, 1880, VF_REQUIRE_APP_RESTART,
	               "sets the base port for the simple http server to run on, defaults to 1880");
#endif

	REGISTER_CVAR2("sys_dump_aux_threads", &g_cvars.sys_dump_aux_threads, 1, VF_NULL, "Dumps callstacks of other threads in case of a crash");
	REGISTER_CVAR2("sys_keyboard_break", &g_cvars.sys_keyboard_break, 0, VF_NULL, "Enables keyboard break handler");

	REGISTER_CVAR2("sys_limit_phys_thread_count", &g_cvars.sys_limit_phys_thread_count, 1, VF_NULL, "Limits p_num_threads to physical CPU count - 1");

#if CRY_PLATFORM_WINDOWS && defined(_RELEASE)
	const int DEFAULT_SYS_MAX_FPS = 0;
#else
	const int DEFAULT_SYS_MAX_FPS = -1;
#endif
	REGISTER_CVAR2("sys_MaxFPS", &g_cvars.sys_MaxFPS, DEFAULT_SYS_MAX_FPS, VF_NULL, "Limits the frame rate to specified number n (if n>0 and if vsync is disabled).\n"
	                                                                                " 0 = on PC if vsync is off auto throttles fps while in menu or game is paused (default)\n"
	                                                                                "-1 = off");

	REGISTER_CVAR2("sys_maxTimeStepForMovieSystem", &g_cvars.sys_maxTimeStepForMovieSystem, 0.1f, VF_NULL, "Caps the time step for the movie system so that a cut-scene won't be jumped in the case of an extreme stall.");

	REGISTER_CVAR2("sys_force_installtohdd_mode", &g_cvars.sys_force_installtohdd_mode, 0, VF_NULL, "Forces install to HDD mode even when doing DVD emulation");

	m_sys_use_Mono = REGISTER_INT("sys_use_mono", 1, 0,
	                              "Use Mono Framework\n"
	                              "0 = off\n"
	                              "1 = on");

#define CRASH_CMD_HELP                      \
  " 0=off\n"                                \
  " 1=null pointer exception\n"             \
  " 2=floating point exception\n"           \
  " 3=memory allocation exception\n"        \
  " 4=cry fatal error is called\n"          \
  " 5=memory allocation for small blocks\n" \
  " 6=assert\n"                             \
  " 7=debugbreak\n"                         \
  " 8=10min sleep\n"

#if CRY_PLATFORM_WINAPI
	#define CRASH_CMD_HELP_EXTRA     \
	  " 9=invalid argument passed\n" \
	  "10=pure virtual function call"
#else
	#define CRASH_CMD_HELP_EXTRA
#endif

	REGISTER_COMMAND("sys_crashtest", CmdCrashTest, VF_CHEAT, "Make the game crash\n" CRASH_CMD_HELP CRASH_CMD_HELP_EXTRA);
	REGISTER_COMMAND("sys_crashtest_thread", CmdCrashTestOnThread, VF_CHEAT, "Make the game crash on a thread\n" CRASH_CMD_HELP CRASH_CMD_HELP_EXTRA);
#undef CRASH_CMD_HELP_EXTRA

	REGISTER_FLOAT("sys_scale3DMouseTranslation", 0.2f, 0, "Scales translation speed of supported 3DMouse devices.");
	REGISTER_FLOAT("sys_Scale3DMouseYPR", 0.05f, 0, "Scales rotation speed of supported 3DMouse devices.");

	REGISTER_INT("capture_frames", 0, 0, "Enables capturing of frames. 0=off, 1=on");
	REGISTER_STRING("capture_folder", "CaptureOutput", 0, "Specifies sub folder to write captured frames.");
	REGISTER_STRING("capture_file_format", "jpg", 0, "Specifies file format of captured files (jpg, tga).");
	REGISTER_INT("capture_frame_once", 0, 0, "Makes capture single frame only");
	REGISTER_STRING("capture_file_name", "", 0, "If set, specifies the path and name to use for the captured frame");
	REGISTER_STRING("capture_file_prefix", "", 0, "If set, specifies the prefix to use for the captured frame instead of the default 'Frame'.");

	m_gpu_particle_physics = REGISTER_INT("gpu_particle_physics", 0, VF_REQUIRE_APP_RESTART, "Enable GPU physics if available (0=off / 1=enabled).");
	assert(m_gpu_particle_physics);

	REGISTER_COMMAND("LoadConfig", &LoadConfigurationCmd, 0,
	                 "Load .cfg file from disk (from the {Game}/Config directory)\n"
	                 "e.g. LoadConfig lowspec.cfg\n"
	                 "Usage: LoadConfig <filename>");

#if defined(USE_SCHEMATYC) && defined(USE_SCHEMATYC_EXPERIMENTAL)
	static const int default_sys_SchematycPlugin = 0;
#elif defined(USE_SCHEMATYC_EXPERIMENTAL)
	static const int default_sys_SchematycPlugin = 2;
#else // default = USE_SCHEMATYC
	static const int default_sys_SchematycPlugin = 1;
#endif

	REGISTER_CVAR(sys_SchematycPlugin, default_sys_SchematycPlugin, VF_REQUIRE_APP_RESTART,
	              "Set whether default Schematyc and/or experimental plugin is loaded\n"
	              "0 = Both plugins\n"
	              "1 = Loads default Schematyc plugin only\n"
	              "2 = Loads experimental Schematyc plugin only");

	assert(m_env.pConsole);
	m_env.pConsole->CreateKeyBind("alt_f12", "Screenshot");
	m_env.pConsole->CreateKeyBind("alt_f11", "RecordClip");

	// video clip recording functionality in system as console command
	REGISTER_COMMAND("RecordClip", &RecordClipCmd, VF_BLOCKFRAME,
	                 "Records a video clip of the game\n"
	                 "Usage: RecordClipCmd <exec/config> <time before> <time after> <local backup (backup/no_backup)> <annotation text>\n"
	                 "e.g. RecordClipCmd config 10 5 backup My Test Video\n"
	                 "     Configures the recording parameters\n"
	                 "e.g. RecordClipCmd config\n"
	                 "     Shows the current parameters\n"
	                 "e.g. RecordClipCmd\n"
	                 "     Records a video clip using the stored parameters\n"
	                 "e.g. RecordClipCmd exec 3 6 no_backup Other Test Video\n"
	                 "     Records a video clip using the given recording parameters and updates the configuration\n");

	// screenshot functionality in system as console
	REGISTER_COMMAND("Screenshot", &ScreenshotCmd, VF_BLOCKFRAME,
	                 "Create a screenshot with annotation\n"
	                 "e.g. Screenshot beach scene with shark\n"
	                 "Supported filetypes are .jpg and .tga; default is .jpg\n"
	                 "Usage: Screenshot <annotation text>");

	REGISTER_COMMAND("sys_LvlRes_finalstep", &LvlRes_finalstep, 0, "to combine all recorded level resources and create final stripped build (pass directory name as parameter)");
	REGISTER_COMMAND("sys_LvlRes_findunused", &LvlRes_findunused, 0, "find unused level resources");
	/*
	   // experimental feature? - needs to be created very early
	   m_sys_filecache = REGISTER_INT("sys_FileCache",0,0,
	    "To speed up loading from non HD media\n"
	    "0=off / 1=enabled");
	 */
	REGISTER_CVAR2("sys_AI", &g_cvars.sys_ai, 1, 0, "Enables AI Update");
	REGISTER_CVAR2("sys_physics", &g_cvars.sys_physics, 1, 0, "Enables Physics Update");
	REGISTER_CVAR2("sys_entities", &g_cvars.sys_entitysystem, 1, 0, "Enables Entities Update");
	REGISTER_CVAR2("sys_trackview", &g_cvars.sys_trackview, 1, 0, "Enables TrackView Update");
	REGISTER_CVAR2("sys_livecreate", &g_cvars.sys_livecreate, 1, VF_REQUIRE_APP_RESTART,
	               "Enable/disable LiveCreate. Values: 0-disabled, use Null implementation, 1-fully enabled, 2-disabled (but initialized)");

	//Defines selected language.
	REGISTER_STRING_CB("g_language", "", VF_NULL, "Defines which language pak is loaded", CSystem::OnLanguageCVarChanged);
	REGISTER_STRING_CB("g_languageAudio", "", VF_NULL, "Will automatically match g_language setting unless specified otherwise", CSystem::OnLanguageAudioCVarChanged);

	REGISTER_COMMAND("sys_RestoreSpec", &SysRestoreSpecCmd, 0,
	                 "Restore or test the cvar settings of game specific spec settings,\n"
	                 "'test*' and 'info' log to the log file only\n"
	                 "Usage: sys_RestoreSpec [test|test*|apply|info]");

	REGISTER_STRING("sys_root", m_root.c_str(), VF_READONLY, "");

	REGISTER_COMMAND("VisRegTest", &VisRegTest, 0, "Run visual regression test.\n"
	                                               "Usage: VisRegTest [<name>=test] [<config>=visregtest.xml] [quit=false]");

	CCryMemoryManager::RegisterCVars();
#if CAPTURE_REPLAY_LOG
	CMemReplay::RegisterCVars();
#endif

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	REGISTER_CVAR2("sys_display_threads", &g_cvars.sys_display_threads, 0, 0, "Displays Thread info");
#endif

	REGISTER_COMMAND("sys_threads_dump_thread_config_list", CmdDumpThreadConfigList, VF_CHEAT, "Show a list of thread \"startup\" configurations loaded for this platform.");

#if CRY_PLATFORM_WINDOWS
	static const int default_sys_usePlatformSavingAPI = 0;
	static const int default_sys_usePlatformSavingAPIDefault = 0;
#else
	static const int default_sys_usePlatformSavingAPI = 1;

	#ifndef _RELEASE
	static const int default_sys_usePlatformSavingAPIDefault = 1;
	#endif
#endif

	REGISTER_CVAR2("sys_usePlatformSavingAPI", &g_cvars.sys_usePlatformSavingAPI, default_sys_usePlatformSavingAPI, VF_REQUIRE_APP_RESTART, "Use the platform APIs for saving and loading (complies with TRCs, but allocates lots of memory)");
#ifndef _RELEASE
	REGISTER_CVAR2("sys_usePlatformSavingAPIEncryption", &g_cvars.sys_usePlatformSavingAPIEncryption, default_sys_usePlatformSavingAPIDefault, VF_CHEAT, "Use encryption cipher when using the platform APIs for saving and loading");
#endif

#if defined(USE_CRY_ASSERT)
	REGISTER_COMMAND("sys_asserts", CmdSetAssertLevel, VF_NULL,
	               "0 = Disable Asserts\n"
	               "1 = Enable Asserts\n"
	               "2 = Fatal Error on Assert\n"
	               "3 = Debug break on Assert\n"
	               );

	REGISTER_COMMAND("sys_ignore_asserts_from_module", CmdIgnoreAssertsFromModule, VF_NULL, "Disables asserts from the specified module");
	REGISTER_COMMAND("sys_log_asserts", CmdLogAsserts, VF_NULL, "If set to 0, only the first occurence of an assert will be logged. Default is 1.");
#endif

	REGISTER_CVAR2("sys_error_debugbreak", &g_cvars.sys_error_debugbreak, 0, VF_CHEAT, "__debugbreak() if a VALIDATOR_ERROR_DBGBREAK message is hit");

	REGISTER_CVAR2("sys_enable_crash_handler", &g_cvars.sys_enable_crash_handler, 0, VF_CHEAT, "Enable/Disable crash handler (PC only)");

	REGISTER_INT("sys_debugger_adjustments", 0, VF_CHEAT, "Adjust behavior to help with debugger breakpoints");

	// [VR]
	if (m_pHmdManager)
	{
		m_pHmdManager->SetupAction(IHmdManager::eHmdSetupAction_CreateCvars);
		m_sys_vr_support = REGISTER_CVAR2_CB("sys_vr_support", &g_cvars.sys_vr_support, 0, VF_NULL, "Specifies if virtual reality (VR) devices should be enabled.", CHmdManager::OnVirtualRealityDeviceChanged);
	}

	REGISTER_STRING("dlc_directory", "", 0, "Holds the path to the directory where DLC should be installed to and read from");

	static const char* p_physics_library_default = "CryPhysics";
	m_pPhysicsLibrary = REGISTER_STRING("p_physics_library", p_physics_library_default, VF_DUMPTODISK,
	                                    "Sets the physics library to be used. Default is 'CryPhysics'"
	                                    "Specify in system.cfg like this: p_physics_library = \"CryPhysics\"");

	REGISTER_INT("sys_system_timer_resolution", 1, VF_NULL, "(Windows only) Value of the system timer resolution in milliseconds (ms)");

#if defined(MAP_LOADING_SLICING)
	CreateSystemScheduler(this);
#endif // defined(MAP_LOADING_SLICING)

#if CRY_PLATFORM_WINDOWS
	REGISTER_INT("sys_screensaver_allowed", 0, VF_NULL, "Specifies if screen saver is allowed to start up while the game is running.");
#endif

#if CRY_PLATFORM_WINDOWS
	REGISTER_CVAR2("sys_highrestimer", &g_cvars.sys_highrestimer, 0, VF_REQUIRE_APP_RESTART, "Enables high resolution system timer.");
#endif

#if CRY_PLATFORM_WINDOWS
	REGISTER_CVAR2("sys_reflection_natvis", &g_cvars.sys_reflection_natvis, 0, VF_NULL, "Enables reflection .natvis file generation on startup.");
#endif

	g_cvars.sys_intromoviesduringinit = 0;
#if CRY_PLATFORM_WINDOWS
	((DebugCallStack*)IDebugCallStack::instance())->RegisterCVars();
#elif CRY_PLATFORM_DURANGO
	((DurangoDebugCallStack*)IDebugCallStack::instance())->RegisterCVars();
#endif

	m_pUserAnalyticsSystem->RegisterCVars();

	Serialization::RegisterArchiveHostCVars();
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CreateAudioVars()
{
	assert(gEnv);
	assert(gEnv->pConsole);

	m_sys_audio_disable = REGISTER_INT("sys_audio_disable", 0, VF_REQUIRE_APP_RESTART,
	                                   "Specifies whether to use the NULLAudioSystem in place of the regular AudioSystem\n"
	                                   "Usage: sys_audio_disable [0/1]\n"
	                                   "0: use regular AudioSystem.\n"
	                                   "1: use NullAudioSystem (disable all audio functionality).\n"
	                                   "Default: 0 (enable audio functionality)");
}

/////////////////////////////////////////////////////////////////////
void CSystem::AddCVarGroupDirectory(const string& sPath)
{
	CryLog("creating CVarGroups from directory '%s' ...", sPath.c_str());
	INDENT_LOG_DURING_SCOPE();

	_finddata_t fd;

	intptr_t handle = gEnv->pCryPak->FindFirst(ConcatPath(sPath, "*.cfg"), &fd);

	if (handle < 0)
		return;

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
				AddCVarGroupDirectory(ConcatPath(sPath, fd.name));
		}
		else
		{
			string sFilePath = ConcatPath(sPath, fd.name);
			string sCVarName = PathUtil::GetFileName(fd.name);
			if (m_env.pConsole != 0)
			{
				((CXConsole*)m_env.pConsole)->RegisterCVarGroup(sCVarName, sFilePath);
			}
		}
	}
	while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

	gEnv->pCryPak->FindClose(handle);
}

bool CSystem::RegisterErrorObserver(IErrorObserver* errorObserver)
{
	return stl::push_back_unique(m_errorObservers, errorObserver);
}

bool CSystem::UnregisterErrorObserver(IErrorObserver* errorObserver)
{
	return stl::find_and_erase(m_errorObservers, errorObserver);
}

void CSystem::OnFatalError(const char* message)
{
	std::vector<IErrorObserver*>::const_iterator end = m_errorObservers.end();
	for (std::vector<IErrorObserver*>::const_iterator it = m_errorObservers.begin(); it != end; ++it)
	{
		(*it)->OnFatalError(message);
	}
}

#if defined(USE_CRY_ASSERT)
void CSystem::OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber)
{
	if (Cry::Assert::IsAssertLevel(Cry::Assert::ELevel::Disabled))
	{
		return;
	}

	std::vector<IErrorObserver*>::const_iterator end = m_errorObservers.end();
	for (std::vector<IErrorObserver*>::const_iterator it = m_errorObservers.begin(); it != end; ++it)
	{
		(*it)->OnAssert(condition, message, fileName, fileLineNumber);
	}

	if (!Cry::Assert::IgnoreAllAsserts() && Cry::Assert::IsAssertLevel(Cry::Assert::ELevel::FatalErrorOnAssert))
	{
		CryFatalError("<assert> %s\r\n%s\r\n%s (%u)\r\n", condition, message, fileName, fileLineNumber);
	}
}

#endif
