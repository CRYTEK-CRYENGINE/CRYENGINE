// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoRuntime.h"

#include "Wrappers/RootMonoDomain.h"
#include "Wrappers/AppDomain.h"
#include "Wrappers/MonoLibrary.h"
#include "Wrappers/MonoClass.h"
#include "Wrappers/MonoObject.h"
#include "Wrappers/MonoMethod.h"

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

// Must be included only once in DLL module.
#include <CryCore/Platform/platform_impl.inl>

#if defined(CRY_PLATFORM_WINDOWS) && !defined(_LIB)
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
#endif // WIN32

CRYREGISTER_SINGLETON_CLASS(CMonoRuntime)

#if CRY_PLATFORM_WINDOWS
	#pragma comment(lib, "mono-2.0.lib")
#endif

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
	: m_pLibCommon(nullptr)
	, m_pLibCore(nullptr)
	, m_pRootDomain(nullptr)
	, m_pPluginDomain(nullptr)
	, m_listeners(5)
{
	REGISTER_COMMAND("mono_reload", OnReloadRequested, VF_NULL, "Used to reload all mono plug-ins");
}

CMonoRuntime::~CMonoRuntime()
{
	if (gEnv && gEnv->pSystem)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}
}

bool CMonoRuntime::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	CryLog("[Mono] Initialize Mono Runtime . . . ");

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CMonoRuntime");
	gEnv->pMonoRuntime = this;

#ifndef _RELEASE
	char szSoftDebuggerOption[256];
	int softDebuggerPort = 17615;

	const ICmdLineArg* pDebugPortArg = gEnv->pSystem->GetICmdLine()->FindArg(ECmdLineArgType::eCLAT_Pre, "monoDebuggerPort");
	if (pDebugPortArg != nullptr)
	{
		softDebuggerPort = pDebugPortArg->GetIValue();
	}

	sprintf_s(szSoftDebuggerOption, "--debugger-agent=transport=dt_socket,address=127.0.0.1:%d,embedding=1,server=y,suspend=n", softDebuggerPort);

	MonoInternals::mono_debug_init(MonoInternals::MONO_DEBUG_FORMAT_MONO);
	char* options[] = {
		"--soft-breakpoints",
		szSoftDebuggerOption
	};
	MonoInternals::mono_jit_parse_options(sizeof(options) / sizeof(char*), options);

	gEnv->pLog->LogAlways("[Mono] Debugger server active on port: %d", softDebuggerPort);
#endif

	// Find the Mono configuration directory
	char engineRoot[_MAX_PATH];
	CryFindEngineRootFolder(_MAX_PATH, engineRoot);

	const char* szMonoDirectoryParent = "bin\\common";

	char sMonoLib[_MAX_PATH];
	sprintf_s(sMonoLib, "%s\\%s\\Mono\\lib", engineRoot, szMonoDirectoryParent);
	char sMonoEtc[_MAX_PATH];
	sprintf_s(sMonoEtc, "%s\\%s\\Mono\\etc", engineRoot, szMonoDirectoryParent);

	if (!gEnv->pCryPak->IsFileExist(sMonoLib) || !gEnv->pCryPak->IsFileExist(sMonoEtc))
	{
		CryLogAlways("Failed to initialize Mono runtime, Mono directory was not found or incomplete in %s directory", szMonoDirectoryParent);
		delete this;

		return false;
	}

	MonoInternals::mono_set_dirs(sMonoLib, sMonoEtc);

	MonoInternals::mono_trace_set_level_string(s_monoLogLevels[0]);

	MonoInternals::mono_trace_set_log_handler(MonoLogCallback, this);
	MonoInternals::mono_trace_set_print_handler(MonoPrintCallback);
	MonoInternals::mono_trace_set_printerr_handler(MonoPrintErrorCallback);

	m_pRootDomain = std::make_shared<CRootMonoDomain>();
	m_pRootDomain->Initialize();

	m_domainLookupMap.emplace(std::make_pair(m_pRootDomain->GetMonoDomain(), m_pRootDomain));

	RegisterInternalInterfaces();

	gEnv->pConsole->RegisterListener(this, "MonoRuntime::ManagedConsoleCommandListener");

	CryLog("[Mono] Initialization done.");
	return true;
}

void CMonoRuntime::Shutdown()
{
	gEnv->pConsole->UnregisterListener(this);
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (m_pLibCore != nullptr)
	{
		// Get the equivalent of gEnv
		std::shared_ptr<CMonoClass> pEngineClass = m_pLibCore->GetTemporaryClass("CryEngine", "Engine");
		CRY_ASSERT(pEngineClass != nullptr);

		// Call the static Shutdown function
		pEngineClass->FindMethod("OnEngineShutdown")->Invoke();
	}

	// Root domain HAS to be deleted last, its destructor shuts down the entire runtime!
	m_domainLookupMap.clear();
	m_pRootDomain.reset();

	m_nodeCreators.clear();

	gEnv->pMonoRuntime = nullptr;
}

std::shared_ptr<ICryPlugin> CMonoRuntime::LoadBinary(const char* szBinaryPath)
{
	std::shared_ptr<CManagedPlugin> pPlugin = std::make_shared<CManagedPlugin>(szBinaryPath);
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

	gEnv->pLog->LogWithType(engineLvl, "[Mono][%s][%s] %s", szLogDomain, szLogLevel, szMessage);

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

	auto pair = m_domainLookupMap.emplace(std::make_pair(pActiveMonoDomain, std::make_shared<CAppDomain>(pActiveMonoDomain)));
	return pair.first->second.get();
}

CAppDomain* CMonoRuntime::CreateDomain(char* name, bool bActivate)
{
	std::shared_ptr<CAppDomain> pDomain = std::make_shared<CAppDomain>(name, bActivate);
	auto pair = m_domainLookupMap.emplace(std::make_pair(pDomain->GetMonoDomain(), pDomain));

	return pDomain.get();
}

CMonoLibrary* CMonoRuntime::GetCryCommonLibrary() const
{
	return m_pLibCommon; 
}

CMonoLibrary* CMonoRuntime::GetCryCoreLibrary() const
{
	return m_pLibCore; 
}

void CMonoRuntime::RegisterManagedNodeCreator(const char* szClassName, IManagedNodeCreator* pCreator)
{
	BehaviorTree::IBehaviorTreeManager& manager = *gEnv->pAISystem->GetIBehaviorTreeManager();
	m_nodeCreators.emplace_back(std::make_shared<CManagedNodeCreatorProxy>(szClassName, pCreator));
	manager.GetNodeFactory().RegisterNodeCreator(m_nodeCreators.back().get());
}

void CMonoRuntime::RegisterNativeToManagedInterface(IMonoNativeToManagedInterface& interface)
{
	string functionNamePrefix;
	functionNamePrefix.Format("%s.%s::", interface.GetNamespace(), interface.GetClassName());

	auto registerInternalCall = [functionNamePrefix](const void* pMethod, const char* szMethodName)
	{
		string methodName = functionNamePrefix + szMethodName;

		MonoInternals::mono_add_internal_call(methodName, pMethod);
	};

	interface.RegisterFunctions(registerInternalCall);
}

CMonoDomain* CMonoRuntime::FindDomainByHandle(MonoInternals::MonoDomain* pDomain)
{
	auto domainIt = m_domainLookupMap.find(pDomain);
	if (domainIt == m_domainLookupMap.end())
	{
		domainIt = m_domainLookupMap.emplace(std::make_pair(pDomain, std::make_shared<CAppDomain>(pDomain))).first;
	}

	return domainIt->second.get();
}

CAppDomain* CMonoRuntime::LaunchPluginDomain()
{
	if (m_pPluginDomain != nullptr)
		return m_pPluginDomain;

	// Create the plug-in domain and activate it
	if (m_pPluginDomain = static_cast<CAppDomain*>(CreateDomain("CryEngine.Plugins", true)))
	{
		char executableFolder[_MAX_PATH];
		CryGetExecutableFolder(_MAX_PATH, executableFolder);

		string libraryPath = PathUtil::Make(executableFolder, "CryEngine.Common");
		m_pLibCommon = m_pPluginDomain->LoadLibrary(libraryPath);
		if (m_pLibCommon == nullptr)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Failed to load managed common library %s", libraryPath.c_str());
			m_pPluginDomain = nullptr;
			return nullptr;
		}

		libraryPath = PathUtil::Make(executableFolder, "CryEngine.Core");
		m_pLibCore = m_pPluginDomain->LoadLibrary(libraryPath);
		if (m_pLibCore == nullptr)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Failed to load managed core library %s", libraryPath.c_str());
			m_pPluginDomain = nullptr;
			return nullptr;
		}

		// Get the equivalent of gEnv
		std::shared_ptr<CMonoClass> pEngineClass = m_pLibCore->GetTemporaryClass("CryEngine", "Engine");
		CRY_ASSERT(pEngineClass != nullptr);

		// Call the static Initialize function
		pEngineClass->FindMethod("OnEngineStart")->Invoke();

		return m_pPluginDomain;
	}

	return nullptr;
}

void CMonoRuntime::ReloadPluginDomain()
{
	// Trigger removal of C++ listeners, since the memory they belong to will be invalid shortly
	std::shared_ptr<CMonoClass> pEngineClass = m_pLibCore->GetTemporaryClass("CryEngine", "Engine");
	CRY_ASSERT(pEngineClass != nullptr);

	// Call the static Shutdown function
	pEngineClass->FindMethod("OnUnloadStart")->Invoke();

	if (m_pPluginDomain->Reload())
	{
		pEngineClass->ReloadClass();

		// Notify the framework so that internal listeners etc. can be added again.
		pEngineClass->FindMethod("OnReloadDone")->Invoke();
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
	std::shared_ptr<CMonoClass> pConsoleCommandArgumentHolderClass = std::static_pointer_cast<CMonoClass>(m_pLibCore->GetTemporaryClass("CryEngine", "ConsoleCommandArgumentsHolder"));
	int numArguments = pCommandArguments->GetArgCount();

	CMonoDomain* pDomain = static_cast<CMonoDomain*>(pConsoleCommandArgumentHolderClass->GetAssembly()->GetDomain());

	void* pConstructorArgs[1] = { &numArguments };
	std::shared_ptr<CMonoObject> pConsoleCommandArgumentHolderInstance = pConsoleCommandArgumentHolderClass->CreateInstance(pConstructorArgs, 1);

	void* pSetArguments[2];
	std::shared_ptr<CMonoMethod> pSetMethod = std::static_pointer_cast<CMonoMethod>(pConsoleCommandArgumentHolderClass->FindMethod("SetArgument", 2));
	
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
	CMonoClass* classConsoleCommand = m_pLibCore->GetClass("CryEngine", "ConsoleCommand");
	classConsoleCommand->FindMethod("NotifyManagedConsoleCommand", 2)->Invoke(nullptr, methodArg2);
}

void CMonoRuntime::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
		case ESYSTEM_EVENT_GAME_POST_INIT:
		{
			// Make sure the plug-in domain exists, since plugins are always loaded in there
			CAppDomain* pPluginDomain = LaunchPluginDomain();
			CRY_ASSERT(pPluginDomain != nullptr);

			// Temporary, remove scanning of the Core assembly when we use the unified components
			static CManagedPlugin::TComponentFactoryMap coreLibraryFactoryMap;
			CManagedPlugin::s_pCurrentlyRegisteringFactory = &coreLibraryFactoryMap;

			//Scan the Core-assembly for entity components etc.
			void* pRegisterArgs[1] = { m_pLibCore->GetManagedObject() };
			std::shared_ptr<CMonoClass> pEngineClass = m_pLibCore->GetTemporaryClass("CryEngine", "Engine");
			pEngineClass->FindMethodWithDesc("ScanAssembly(Assembly)")->InvokeStatic(pRegisterArgs);

			CManagedPlugin::s_pCurrentlyRegisteringFactory = nullptr;

			CompileAssetSourceFiles();

			for (const std::weak_ptr<CManagedPlugin>& plugin : m_plugins)
			{
				if (std::shared_ptr<CManagedPlugin> pPlugin = plugin.lock())
				{
					pPlugin->Load(pPluginDomain);
				}
			}
		}
		break;
	}
}

void CMonoRuntime::CompileAssetSourceFiles()
{
	std::vector<string> sourceFiles;

	const char* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute();
	if (szAssetDirectory == nullptr || strlen(szAssetDirectory) == 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Failed to locate asset directory");
		return;
	}

	// Return early if there is no asset directory, otherwise this will scan the whole hard-drive for .cs files.
	if (szAssetDirectory == nullptr || szAssetDirectory[0] == '\0')
	{
		return;
	}

	FindSourceFilesInDirectoryRecursive(szAssetDirectory, sourceFiles);
	if (sourceFiles.size() == 0)
		return;

	CMonoDomain* pDomain = m_pLibCore->GetDomain();

	std::shared_ptr<CMonoClass> pCompilerClass = m_pLibCore->GetTemporaryClass("CryEngine.Compilation", "Compiler");
	std::shared_ptr<CMonoMethod> pCompilationMethod = pCompilerClass->FindMethod("CompileCSharpSourceFiles", 1);

	MonoInternals::MonoArray* pStringArray = MonoInternals::mono_array_new(pDomain->GetMonoDomain(), MonoInternals::mono_get_string_class(), sourceFiles.size());
	for (int i = 0; i < sourceFiles.size(); ++i)
	{
		mono_array_set(pStringArray, MonoInternals::MonoString*, i, mono_string_new(pDomain->GetMonoDomain(), sourceFiles[i]));
	}

	void* pParams[1] = { pStringArray };
	std::shared_ptr<CMonoObject> pResult = pCompilationMethod->InvokeStatic(pParams);
	if (MonoInternals::MonoReflectionAssembly* pReflectionAssembly = (MonoInternals::MonoReflectionAssembly*)pResult->GetManagedObject())
	{
		MonoInternals::MonoAssembly* pAssembly = mono_reflection_assembly_get_assembly(pReflectionAssembly);

		m_pAssetsPlugin = std::make_shared<CManagedPlugin>(pDomain->GetLibraryFromMonoAssembly(pAssembly));
		m_plugins.emplace_back(m_pAssetsPlugin);
	}

	/*for (int i = 0; i < sourceFiles.size(); ++i)
	{
		mono_free(mono_array_get(pStringArray, MonoInternals::MonoString*, i));
	}*/
}

void CMonoRuntime::FindSourceFilesInDirectoryRecursive(const char* szDirectory,std::vector<string>& sourceFiles)
{
	string searchPath = PathUtil::Make(szDirectory, "*.cs");

	_finddata_t fd;
	intptr_t handle = gEnv->pCryPak->FindFirst(searchPath, &fd);
	if (handle != -1)
	{
		do
		{
			sourceFiles.emplace_back(PathUtil::Make(szDirectory, fd.name));
		} while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	// Find additional directories
	searchPath = PathUtil::Make(szDirectory, "*.*");

	handle = gEnv->pCryPak->FindFirst(searchPath, &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
				{
					string sDirectory = PathUtil::Make(szDirectory, fd.name);

					FindSourceFilesInDirectoryRecursive(sDirectory, sourceFiles);
				}
			}
		} while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}
}

void CMonoRuntime::HandleException(MonoInternals::MonoException* pException)
{
#if CRY_PLATFORM_WINDOWS && !defined(RELEASE)
	if (IsDebuggerPresent())
	{
		if (MonoInternals::MonoString* pString = MonoInternals::mono_object_to_string((MonoInternals::MonoObject*)pException, nullptr))
		{
			std::shared_ptr<CMonoString> pErrorMessage = CMonoDomain::CreateString(pString);

			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Handled managed exception with debugger attached:\n%s", pErrorMessage->GetString());

			__debugbreak();
		}
	}
#endif

	gEnv->pHardwareMouse->UseSystemCursor(true);
	
	void* args[1];
	args[0] = pException;

	std::shared_ptr<CMonoClass> pExceptionHandlerClass = m_pLibCore->GetTemporaryClass("CryEngine.Debugging", "ExceptionHandler");
	CRY_ASSERT(pExceptionHandlerClass != nullptr);

	// Call the static Initialize function
	pExceptionHandlerClass->FindMethod("Display", 1)->Invoke(nullptr, args);
}