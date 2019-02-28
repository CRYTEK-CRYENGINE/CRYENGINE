// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoRuntime.h"

#include "Wrappers/RootMonoDomain.h"
#include "Wrappers/AppDomain.h"
#include "Wrappers/MonoLibrary.h"
#include "Wrappers/MonoClass.h"
#include "Wrappers/MonoObject.h"
#include "Wrappers/MonoMethod.h"
#include "Wrappers/MonoException.h"

#include "ManagedPlugin.h"

#include "NativeToManagedInterfaces/Entity.h"
#include "NativeToManagedInterfaces/Console.h"
#include "NativeToManagedInterfaces/Audio.h"

#include <CrySystem/ILog.h>
#include <CrySystem/IProjectManager.h>
#include <CryAISystem/IAISystem.h>
#include <CrySystem/IConsole.h>

#include "NativeToManagedInterfaces/IMonoNativeToManagedInterface.h"
#include <CryInput/IHardwareMouse.h>
#include <CrySystem/ICmdLine.h>

#if CRY_PLATFORM_WINDOWS
#include <Shlwapi.h>
#endif

// Must be included only once in DLL module.
#include <CryCore/Platform/platform_impl.inl>

#if defined(CRY_PLATFORM_WINDOWS) && !defined(_LIB)
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
#endif // WIN32

CRYREGISTER_SINGLETON_CLASS(CMonoRuntime)

static const char* s_monoLogLevels[] =
{
	NULL,
	"error",
	"critical",
	"warning",
	"message",
	"info",
	"debug"
};

static IMiniLog::ELogType s_monoToEngineLevels[] =
{
	IMiniLog::eAlways,
	IMiniLog::eErrorAlways,
	IMiniLog::eError,
	IMiniLog::eWarning,
	IMiniLog::eMessage,
	IMiniLog::eMessage,
	IMiniLog::eComment
};

void OnReloadRequested(IConsoleCmdArgs *pArgs)
{
	GetMonoRuntime()->ReloadPluginDomain();
}

CMonoRuntime::CMonoRuntime()
	: m_pRootDomain(nullptr)
	, m_pPluginDomain(nullptr)
	, m_listeners(5)
	, m_compileListeners(1)
{
}

CMonoRuntime::~CMonoRuntime()
{
	if (gEnv)
	{
		gEnv->pMonoRuntime = nullptr;
		
		if (gEnv->pConsole)
		{
			gEnv->pConsole->UnregisterListener(this);
		}

		if (gEnv->pSystem)
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}
	}
}

bool CMonoRuntime::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CMonoRuntime");
	gEnv->pMonoRuntime = this;

	return true;
}

bool CMonoRuntime::InitializeRuntime()
{
	CryLog("Initializing .NET/Mono...");

#ifndef _RELEASE
	char szSoftDebuggerOption[256];
	int softDebuggerPort = 17615;
	string szSuspend = "n";

	const ICmdLineArg* pDebugPortArg = gEnv->pSystem->GetICmdLine()->FindArg(ECmdLineArgType::eCLAT_Pre, "monoDebuggerPort");
	if (pDebugPortArg != nullptr)
	{
		softDebuggerPort = pDebugPortArg->GetIValue();
	}

	const ICmdLineArg* pSuspendArg = gEnv->pSystem->GetICmdLine()->FindArg(ECmdLineArgType::eCLAT_Pre, "monoSuspend");
	if (pSuspendArg != nullptr)
	{
		if (pSuspendArg->GetIValue() == 1)
		{
			szSuspend = "y";
		}
	}
	sprintf_s(szSoftDebuggerOption, "--debugger-agent=transport=dt_socket,address=127.0.0.1:%d,embedding=1,server=y,suspend=%s", softDebuggerPort, szSuspend);

	MonoInternals::mono_debug_init(MonoInternals::MONO_DEBUG_FORMAT_MONO);
	char* options[] = {
		"--soft-breakpoints",
		szSoftDebuggerOption
	};
	MonoInternals::mono_jit_parse_options(sizeof(options) / sizeof(char*), options);

	gEnv->pLog->LogAlways("[Mono] Debugger server active on port: %d and suspended is set to %s", softDebuggerPort, szSuspend);
#endif

	// Find the Mono configuration directory
	char engineRoot[_MAX_PATH];
	CryFindEngineRootFolder(_MAX_PATH, engineRoot);

	const char* szMonoDirectoryParent = "bin\\common";

	char sMonoLib[_MAX_PATH];
	sprintf_s(sMonoLib, "%s\\%s\\Mono\\lib", engineRoot, szMonoDirectoryParent);
	char sMonoEtc[_MAX_PATH];
	sprintf_s(sMonoEtc, "%s\\%s\\Mono\\etc", engineRoot, szMonoDirectoryParent);

	if (!gEnv->pCryPak->IsFileExist(sMonoLib, ICryPak::eFileLocation_OnDisk) || !gEnv->pCryPak->IsFileExist(sMonoEtc, ICryPak::eFileLocation_OnDisk))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Failed to initialize Mono runtime, Mono directory was not found or incomplete in %s directory", szMonoDirectoryParent);
		return false;
	}

	MonoInternals::mono_set_dirs(sMonoLib, sMonoEtc);

	MonoInternals::mono_trace_set_level_string(s_monoLogLevels[0]);

	MonoInternals::mono_trace_set_log_handler(MonoLogCallback, this);
	MonoInternals::mono_trace_set_print_handler(MonoPrintCallback);
	MonoInternals::mono_trace_set_printerr_handler(MonoPrintErrorCallback);

	m_pRootDomain = std::make_shared<CRootMonoDomain>();
	m_domains.emplace_back(m_pRootDomain);

	m_pRootDomain->Initialize();

	RegisterInternalInterfaces();

	gEnv->pConsole->RegisterListener(this, "MonoRuntime::ManagedConsoleCommandListener");

	REGISTER_COMMAND("mono_reload", OnReloadRequested, VF_NULL, "Used to reload all mono plug-ins");

	CryLog(".NET/Mono Initialization done.");
	return true;
}

void CMonoRuntime::Shutdown()
{
	// Root domain HAS to be deleted last, its destructor shuts down the entire runtime!
	m_domains.clear();
	m_pRootDomain.reset();

	m_nodeCreators.clear();
}

std::shared_ptr<Cry::IEnginePlugin> CMonoRuntime::LoadBinary(const char* szBinaryPath)
{
	// Mono runtime is only initialized at demand when there are C# plug-ins available
	if (!m_initialized)
	{
		m_initialized = true;
		InitializeRuntime();
	}

	string binaryPath;

#if CRY_PLATFORM_DURANGO
	if (true)
#elif CRY_PLATFORM_WINAPI
	if (PathIsRelative(szBinaryPath))
#elif CRY_PLATFORM_POSIX
	if (szBinaryPath[0] != '/')
#endif
	{
		// First search in the project directory
		binaryPath = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), szBinaryPath);
		if (!gEnv->pCryPak->IsFileExist(binaryPath.c_str()))
		{
			// File did not exist in the project directory, try the engine binary directory
			char szEngineDirectoryBuffer[_MAX_PATH];
			CryGetExecutableFolder(CRY_ARRAY_COUNT(szEngineDirectoryBuffer), szEngineDirectoryBuffer);

			binaryPath = PathUtil::Make(szEngineDirectoryBuffer, szBinaryPath);
			if (!gEnv->pCryPak->IsFileExist(binaryPath.c_str()))
			{
				binaryPath = szBinaryPath;
			}
		}
	}
	else
	{
		binaryPath = szBinaryPath;
	}

	std::shared_ptr<CManagedPlugin> pPlugin = std::make_shared<CManagedPlugin>(binaryPath);
	m_plugins.emplace_back(pPlugin);
	return pPlugin;
}

void CMonoRuntime::Update(int updateFlags, int nPauseMode)
{
	for (MonoListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnUpdate(updateFlags, nPauseMode);
	}
}

void CMonoRuntime::MonoLogCallback(const char* szLogDomain, const char* szLogLevel, const char* szMessage, MonoInternals::mono_bool is_fatal, void* pUserData)
{
	IMiniLog::ELogType engineLvl = IMiniLog::eComment;
	if (szLogLevel)
		for (int i = 1; i < CRY_ARRAY_COUNT(s_monoLogLevels); i++)
			if (_stricmp(s_monoLogLevels[i], szLogLevel) == 0)
			{
				engineLvl = s_monoToEngineLevels[i];
				break;
			}

	gEnv->pLog->LogWithType(engineLvl, "[Mono] [%s] [%s] %s", szLogDomain, szLogLevel, szMessage);

#if CRY_PLATFORM_WINDOWS && !defined(RELEASE)
	if (IsDebuggerPresent())
	{
		__debugbreak();
	}
#endif
}

void CMonoRuntime::MonoPrintCallback(const char* szMessage, MonoInternals::mono_bool is_stdout)
{
	gEnv->pLog->Log("[Mono] %s", szMessage);
}

void CMonoRuntime::MonoPrintErrorCallback(const char* szMessage, MonoInternals::mono_bool is_stdout)
{
	gEnv->pLog->LogError("[Mono] %s", szMessage);

#if CRY_PLATFORM_WINDOWS && !defined(RELEASE)
	if (IsDebuggerPresent())
	{
		__debugbreak();
	}
#endif
}

CRootMonoDomain* CMonoRuntime::GetRootDomain()
{
	return m_pRootDomain.get();
}

CMonoDomain* CMonoRuntime::GetActiveDomain()
{
	MonoInternals::MonoDomain* pActiveMonoDomain = MonoInternals::mono_domain_get();
	
	if (CMonoDomain* pDomain = FindDomainByHandle(pActiveMonoDomain))
	{
		return pDomain;
	}

	CRY_ASSERT_MESSAGE(false, "Kept here for safety if code reaches it, but should never be called");
	m_domains.emplace_back(std::make_shared<CAppDomain>(pActiveMonoDomain));
	static_cast<CAppDomain*>(m_domains.back().get())->Initialize();
	return m_domains.back().get();
}

CAppDomain* CMonoRuntime::CreateDomain(char* name, bool bActivate)
{
	m_domains.emplace_back(std::make_shared<CAppDomain>(name, bActivate));
	static_cast<CAppDomain*>(m_domains.back().get())->Initialize();
	return static_cast<CAppDomain*>(m_domains.back().get());
}

CMonoLibrary* CMonoRuntime::GetCryCommonLibrary() const
{
	return m_pPluginDomain->GetCryCommonLibrary();
}

CMonoLibrary* CMonoRuntime::GetCryCoreLibrary() const
{
	return m_pPluginDomain->GetCryCoreLibrary();
}

void CMonoRuntime::RegisterManagedNodeCreator(const char* szClassName, IManagedNodeCreator* pCreator)
{
	BehaviorTree::IBehaviorTreeManager& manager = *gEnv->pAISystem->GetIBehaviorTreeManager();
	m_nodeCreators.emplace_back(std::make_shared<CManagedNodeCreatorProxy>(szClassName, pCreator));
	manager.GetNodeFactory().RegisterNodeCreator(m_nodeCreators.back().get());
}

void CMonoRuntime::RegisterNativeToManagedInterface(IMonoNativeToManagedInterface& nativeToManagedInterface)
{
	string functionNamePrefix;
	functionNamePrefix.Format("%s.%s::", nativeToManagedInterface.GetNamespace(), nativeToManagedInterface.GetClassName());

	auto registerInternalCall = [functionNamePrefix](const void* pMethod, const char* szMethodName)
	{
		string methodName = functionNamePrefix + szMethodName;

		MonoInternals::mono_add_internal_call(methodName, pMethod);
	};

	nativeToManagedInterface.RegisterFunctions(registerInternalCall);
}

CMonoDomain* CMonoRuntime::FindDomainByHandle(MonoInternals::MonoDomain* pMonoDomain)
{
	for (const std::shared_ptr<CMonoDomain>& pDomain : m_domains)
	{
		if (pDomain->GetMonoDomain() == pMonoDomain)
		{
			return pDomain.get();
		}
	}

	m_domains.emplace_back(std::make_shared<CAppDomain>(pMonoDomain));
	static_cast<CAppDomain*>(m_domains.back().get())->Initialize();
	return m_domains.back().get();
}

CAppDomain* CMonoRuntime::LaunchPluginDomain()
{
	if (m_pPluginDomain != nullptr)
	{
		return m_pPluginDomain;
	}

	// Create the plug-in domain and activate it
	m_pPluginDomain = static_cast<CAppDomain*>(CreateDomain("CryEngine.Plugins", true));
	return m_pPluginDomain;
}

void CMonoRuntime::ReloadPluginDomain()
{
	if (m_initialized)
	{
		m_pPluginDomain->Reload();
	}
	else
	{
		m_initialized = true;
		InitializeRuntime();
		InitializePluginDomain();
	}
}

void CMonoRuntime::RegisterInternalInterfaces()
{
	CManagedEntityInterface entityInterface;
	RegisterNativeToManagedInterface(entityInterface);

	CConsoleCommandInterface consoleCommandInterface;
	RegisterNativeToManagedInterface(consoleCommandInterface);

	CAudioInterface audioInterface;
	RegisterNativeToManagedInterface(audioInterface);
}

void CMonoRuntime::OnManagedConsoleCommandEvent(const char* commandName, IConsoleCmdArgs* consoleCommandArguments)
{
	gEnv->pLog->Log("[Mono] %s", "OnManagedConsoleCommandEvent invoked");
	InvokeManagedConsoleCommandNotification(commandName, consoleCommandArguments);
}

void CMonoRuntime::InvokeManagedConsoleCommandNotification(const char* szCommandName, IConsoleCmdArgs* pCommandArguments)
{
	std::shared_ptr<CMonoClass> pConsoleCommandArgumentHolderClass = std::static_pointer_cast<CMonoClass>(m_pPluginDomain->GetCryCoreLibrary()->GetTemporaryClass("CryEngine", "ConsoleCommandArgumentsHolder"));
	int numArguments = pCommandArguments->GetArgCount();

	CMonoDomain* pDomain = static_cast<CMonoDomain*>(pConsoleCommandArgumentHolderClass->GetAssembly()->GetDomain());

	void* pConstructorArgs[1] = { &numArguments };
	std::shared_ptr<CMonoObject> pConsoleCommandArgumentHolderInstance = pConsoleCommandArgumentHolderClass->CreateInstance(pConstructorArgs, 1);

	void* pSetArguments[2];
	std::shared_ptr<CMonoMethod> pSetMethod = pConsoleCommandArgumentHolderClass->FindMethod("SetArgument", 2).lock();
	
	for (int i = 0; i < numArguments; ++i)
	{
		std::shared_ptr<CMonoString> pArgumentString = std::static_pointer_cast<CMonoString>(pDomain->CreateString(pCommandArguments->GetArg(i)));

		pSetArguments[0] = &i;
		pSetArguments[1] = pArgumentString->GetManagedObject();
		pSetMethod->Invoke(pConsoleCommandArgumentHolderInstance.get(), pSetArguments);
	}

	std::shared_ptr<CMonoString> pCommandName = m_pRootDomain->CreateString(szCommandName);

	void* methodArg2[2];
	methodArg2[0] = pCommandName->GetManagedObject();
	methodArg2[1] = pConsoleCommandArgumentHolderInstance->GetManagedObject();
	CMonoClass* classConsoleCommand = m_pPluginDomain->GetCryCoreLibrary()->GetClass("CryEngine", "ConsoleCommand");
	if (std::shared_ptr<CMonoMethod> pMethod = classConsoleCommand->FindMethod("NotifyManagedConsoleCommand", 2).lock())
	{
		pMethod->Invoke(nullptr, methodArg2);
	}
}

void CMonoRuntime::InitializePluginDomain()
{
	// Make sure the plug-in domain exists, since plugins are always loaded in there
	CAppDomain* pPluginDomain = LaunchPluginDomain();
	CRY_ASSERT(pPluginDomain != nullptr);

	if (pPluginDomain != nullptr)
	{
		CManagedPlugin::s_pCrossPluginRegisteredFactories->clear();
		CManagedPlugin::s_pCurrentlyRegisteringFactories = CManagedPlugin::s_pCrossPluginRegisteredFactories;

		//Scan the Core-assembly for entity components etc.
		std::shared_ptr<CMonoClass> pEngineClass = m_pPluginDomain->GetCryCoreLibrary()->GetTemporaryClass("CryEngine", "Engine");
		if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethodWithDesc("ScanEngineAssembly").lock())
		{
			pMethod->InvokeStatic(nullptr);
		}

		CManagedPlugin::s_pCurrentlyRegisteringFactories = nullptr;

		if (gEnv->IsEditor())
		{
			// Compile C# source files in the assets directory
			// This is placed at the back of m_plugins to make sure that the compiled library is always the last one to be scanned.
			const char* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute();
			if (szAssetDirectory != nullptr && szAssetDirectory[0] != '\0')
			{
				CMonoLibrary* pCompiledLibrary = pPluginDomain->CompileFromSource(szAssetDirectory);
				m_pAssetsPlugin = std::make_shared<CManagedPlugin>(pCompiledLibrary);
				m_plugins.emplace_back(m_pAssetsPlugin);
			}
		}
		else
		{
			if (CMonoLibrary* pCompiledLibrary = pPluginDomain->GetCompiledLibrary())
			{
				m_pAssetsPlugin = std::make_shared<CManagedPlugin>(pCompiledLibrary);
				m_plugins.emplace_back(m_pAssetsPlugin);
			}
		}

		for(auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
		{
			const std::weak_ptr<IManagedPlugin>& plugin = *it;

			if (std::shared_ptr<IManagedPlugin> pPlugin = plugin.lock())
			{
				const size_t loadOrder = std::distance(m_plugins.begin(), it);
				pPlugin->SetLoadIndex(static_cast<int>(loadOrder));
				pPlugin->Load(pPluginDomain);
			}
		}
	}
}

static bool HasScriptFiles(const string& path)
{
	_finddata_t fd;
	intptr_t handle = gEnv->pCryPak->FindFirst(path + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			// Skip back folders.
			if (fd.name[0] == '.')
				continue;

			string filename = path;
			filename += "/";
			filename += fd.name;

			if (fd.attrib & _A_SUBDIR)
			{
				if (HasScriptFiles(filename))
				{
					return true;
				}
			}
			else if (!stricmp(PathUtil::GetExt(fd.name), "cs"))
			{
				return true;
			}
		} while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);
		gEnv->pCryPak->FindClose(handle);
	}

	return false;
}

void CMonoRuntime::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
		case ESYSTEM_EVENT_GAME_POST_INIT:
		{
			// Only initialize run-time if there were C# source files present in asset directory
			// Otherwise, C# is only initialized when a plug-in is loaded from disk
			if (HasScriptFiles(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute()) && !m_initialized)
			{
				m_initialized = true;
				InitializeRuntime();
			}
			
			if(m_initialized)
			{
				// Now compile C# from disk
				InitializePluginDomain();
			}
		}
		break;
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
		{
			Shutdown();
		}
		break;
	}
}

void CMonoRuntime::HandleException(MonoInternals::MonoException* pException)
{
#if CRY_PLATFORM_WINDOWS && !defined(RELEASE)
	if (IsDebuggerPresent())
	{
		CMonoException exception = CMonoException(pException);
		string exceptionMessage = exception.GetExceptionString();
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Handled managed exception with debugger attached:\n%s", exceptionMessage);

		__debugbreak();
	}
#endif

	gEnv->pHardwareMouse->UseSystemCursor(true);
	
	void* args[1];
	args[0] = pException;

	std::shared_ptr<CMonoClass> pExceptionHandlerClass = m_pPluginDomain->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Debugging", "ExceptionHandler");
	CRY_ASSERT(pExceptionHandlerClass != nullptr);

	// Call the static Initialize function
	if (std::shared_ptr<CMonoMethod> pMethod = pExceptionHandlerClass->FindMethod("Display", 1).lock())
	{
		pMethod->Invoke(nullptr, args);
	}
}

void CMonoRuntime::OnCoreLibrariesDeserialized()
{
	// Clear the previous components, and get all engine components again.
	CManagedPlugin::s_pCrossPluginRegisteredFactories->clear();
	CManagedPlugin::s_pCurrentlyRegisteringFactories = CManagedPlugin::s_pCrossPluginRegisteredFactories;

	//Scan the Core-assembly for entity components etc.
	std::shared_ptr<CMonoClass> pEngineClass = m_pPluginDomain->GetCryCoreLibrary()->GetTemporaryClass("CryEngine", "Engine");
	if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethodWithDesc("ScanEngineAssembly").lock())
	{
		pMethod->InvokeStatic(nullptr);
	}

	CManagedPlugin::s_pCurrentlyRegisteringFactories = nullptr;

	for (const std::weak_ptr<IManagedPlugin>& pWeakPlugin : m_plugins)
	{
		if (std::shared_ptr<IManagedPlugin> pPlugin = pWeakPlugin.lock())
		{
			pPlugin->OnCoreLibrariesDeserialized();
		}
	}
}

void CMonoRuntime::OnPluginLibrariesDeserialized()
{
	for (const std::weak_ptr<IManagedPlugin>& pWeakPlugin : m_plugins)
	{
		if (std::shared_ptr<IManagedPlugin> pPlugin = pWeakPlugin.lock())
		{
			pPlugin->OnPluginLibrariesDeserialized();
		}
	}
}

void CMonoRuntime::NotifyCompileFinished(const char* szCompileMessage)
{
	// Compiling should only happen in the editor, never in the GameLauncher.
	CRY_ASSERT(gEnv->IsEditor());

	m_latestCompileMessage = szCompileMessage;

	for (MonoCompileListeners::Notifier notifier(m_compileListeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnCompileFinished(szCompileMessage);
	}
}
