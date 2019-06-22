// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryPluginManager.h"

#include "System.h"
#include "ProjectManager/ProjectManager.h"

#include <CryExtension/ICryFactory.h>
#include <CryExtension/ICryFactoryRegistry.h>
#include <CryExtension/CryCreateClassInstance.h>

#include <CryMono/IMonoRuntime.h>

#include <CryCore/Platform/CryLibrary.h>
#include "CrySystem/Testing/CryTest.h"
#include "CrySystem/Testing/CryTestCommands.h"

#if CRY_PLATFORM_WINDOWS
#include <ShlObj.h> // For SHGetKnownFolderPath()
#endif // CRY_PLATFORM_WINDOWS

// Descriptor for the binary file of a plugin.
// This is separate since a plugin does not necessarily have to come from a binary, for example if static linking is used.
struct SNativePluginModule
{
	SNativePluginModule()
		: m_pFactory(nullptr)
	{
	}

	SNativePluginModule(const char* szPath, ICryFactory* pFactory = nullptr)
		: m_engineModulePath(szPath)
		, m_pFactory(pFactory)
	{
	}

	SNativePluginModule(SNativePluginModule& other)
		: m_engineModulePath(other.m_engineModulePath)
		, m_pFactory(other.m_pFactory)
	{
		other.MarkUnloaded();
	}

	SNativePluginModule(SNativePluginModule&& other)
		: m_engineModulePath(std::move(other.m_engineModulePath))
		, m_pFactory(other.m_pFactory)
	{
		other.MarkUnloaded();
	}

	~SNativePluginModule()
	{
		Shutdown();
	}

	SNativePluginModule& operator=(const SNativePluginModule&) = default;
	SNativePluginModule& operator=(SNativePluginModule&&) = default;
	
	bool Shutdown()
	{
		bool bSuccess = false;
		if (IsLoaded())
		{
			bSuccess = static_cast<CSystem*>(gEnv->pSystem)->UnloadDynamicLibrary(m_engineModulePath);

			// Prevent Shutdown twice
			MarkUnloaded();
		}

		return bSuccess;
	}

	void MarkUnloaded()
	{
		m_engineModulePath.clear();
	}

	bool IsLoaded() const
	{
		return !m_engineModulePath.empty();
	}

	string m_engineModulePath;
	ICryFactory* m_pFactory;
};

struct SPluginContainer
{
	// Constructor for native plug-ins
	SPluginContainer(const std::shared_ptr<Cry::IEnginePlugin>& plugin, SNativePluginModule&& module)
		: m_pPlugin(plugin)
		, m_module(std::move(module))
		, m_pluginClassId(plugin->GetFactory()->GetClassID())
	{
	}

	// Constructor for managed (Mono) plug-ins, or statically linked ones
	SPluginContainer(const std::shared_ptr<Cry::IEnginePlugin>& plugin)
		: m_pPlugin(plugin) 
	{
		if (ICryFactory* pFactory = plugin->GetFactory())
		{
			m_pluginClassId = pFactory->GetClassID();
		}
	}

	bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		if (m_pPlugin)
		{
			return m_pPlugin->Initialize(env, initParams);
		}

		return false;
	}

	bool Shutdown()
	{
		m_pPlugin->UnregisterFlowNodes();

		m_pPlugin.reset();

		return m_module.Shutdown();
	}

	Cry::IEnginePlugin* GetPluginPtr() const
	{
		return m_pPlugin.get();
	}

	friend bool operator==(const SPluginContainer& left, const SPluginContainer& right)
	{
		return (left.GetPluginPtr() == right.GetPluginPtr());
	}

	CryClassID                           m_pluginClassId;
	std::shared_ptr<Cry::IEnginePlugin>  m_pPlugin;
	SNativePluginModule                  m_module;
};

CCryPluginManager::CCryPluginManager(const SSystemInitParams& initParams)
	: m_systemInitParams(initParams)
	, m_bLoadedProjectPlugins(false)
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CCryPluginManager");
}

CCryPluginManager::~CCryPluginManager()
{
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

	UnloadAllPlugins();

	CRY_ASSERT(m_pluginContainer.empty());

	if (IConsole* pConsole = gEnv->pConsole)
	{
		pConsole->RemoveCommand("sys_reload_plugin");
		pConsole->UnregisterVariable("sys_debug_plugin", true);
	}
}

void CCryPluginManager::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_REGISTER_FLOWNODES:
		{
			for (SPluginContainer& it : m_pluginContainer)
			{
				it.GetPluginPtr()->RegisterFlowNodes();
			}
		}
		break;
	}
}

void CCryPluginManager::LoadProjectPlugins()
{
	ParsePluginRegistry();

	// Find out how many Cry::IEnginePlugin implementations are available
	size_t numFactories;
	gEnv->pSystem->GetCryFactoryRegistry()->IterateFactories(cryiidof<Cry::IEnginePlugin>(), nullptr, numFactories);

	std::vector<ICryFactory*> factories;
	factories.resize(numFactories);

	// Start by looking for any Cry::IEnginePlugin implementation in the current module, in case of static linking
	gEnv->pSystem->GetCryFactoryRegistry()->IterateFactories(cryiidof<Cry::IEnginePlugin>(), factories.data(), numFactories);

	auto removePluginHeader = [](string factoryName)
	{
		// Remove "Plugin_" from the start of the name if it exists (from CRYGENERATE_SINGLETONCLASS_GUID)
		const string pluginNameHeader = "Plugin_";
		if (factoryName.compareNoCase(0, pluginNameHeader.length(), pluginNameHeader) == 0)
		{
			factoryName = factoryName.substr(pluginNameHeader.size());
		}
		return factoryName;
	};

	m_bLoadedProjectPlugins = true;

	// Load plug-ins specified in the .cryproject file from disk
	Cry::CProjectManager* pProjectManager = static_cast<Cry::CProjectManager*>(gEnv->pSystem->GetIProjectManager());
	const std::vector<Cry::SPluginDefinition>& pluginDefinitions = pProjectManager->GetPluginDefinitions();

	for (const Cry::SPluginDefinition& pluginDefinition : pluginDefinitions)
	{
		if (!pluginDefinition.platforms.empty() && !stl::find(pluginDefinition.platforms, EPlatform::Current))
		{
			continue;
		}

		auto it = std::find_if(factories.begin(), factories.end(), [&](ICryFactory* factory)
		{
			const string factoryName = removePluginHeader(factory->GetName());
			return (factoryName.compareNoCase(pluginDefinition.path) == 0);
		});
		if (it != factories.end())
		{
			ICryUnknownPtr pUnknown = (*it)->CreateClassInstance();
			std::shared_ptr<Cry::IEnginePlugin> pPlugin = cryinterface_cast<Cry::IEnginePlugin>(pUnknown);
			m_pluginContainer.emplace_back(pPlugin);
			OnPluginLoaded();
			continue;
		}
#if CrySharedLibrarySupported
		if (!pluginDefinition.guid.IsNull())
			LoadPluginByGUID(pluginDefinition.guid);
		else
			LoadPluginBinary(pluginDefinition.type, pluginDefinition.path);
#endif // CrySharedLibrarySupported
	}
#if CrySharedLibrarySupported
#if !defined(CRY_IS_MONOLITHIC_BUILD) && !defined(CRY_PLATFORM_CONSOLE)
	// Always load the CryUserAnalytics plugin
	Cry::SPluginDefinition userAnalyticsPlugin{ EType::Native, "CryUserAnalytics" };
	if (std::find(std::begin(pluginDefinitions), std::end(pluginDefinitions), userAnalyticsPlugin) == std::end(pluginDefinitions))
	{
		LoadPluginBinary(userAnalyticsPlugin.type, userAnalyticsPlugin.path, false);
	}
#endif
#endif // CrySharedLibrarySupported
}

#if CrySharedLibrarySupported
bool CCryPluginManager::LoadPluginBinary(EType type, const char* szBinaryPath, bool notifyUserOnFailure)
{
	CRY_ASSERT_MESSAGE(m_bLoadedProjectPlugins, "Plug-ins must not be loaded before LoadProjectPlugins!");

	const string binaryName = PathUtil::GetFileName(szBinaryPath);
	CryLogAlways("Loading plug-in %s", binaryName.c_str());

	switch (type)
	{
	case EType::Native:
		{
			MEMSTAT_CONTEXT(EMemStatContextType::Other, "LoadPlugin");
			MEMSTAT_CONTEXT(EMemStatContextType::Other, binaryName.c_str());

			WIN_HMODULE hModule = static_cast<CSystem*>(gEnv->pSystem)->LoadDynamicLibrary(szBinaryPath, false);
			if (hModule == nullptr)
			{
				const string errorMessage = string().Format("Plugin load failed, could not find dynamic library %s!", binaryName.c_str());
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			// Create RAII struct to ensure that module is unloaded on failure
			SNativePluginModule nativeModule(szBinaryPath);

			// Find the first Cry::IEnginePlugin instance inside the module
			PtrFunc_GetHeadToRegFactories getHeadToRegFactories = (PtrFunc_GetHeadToRegFactories)CryGetProcAddress(hModule, "GetHeadToRegFactories");
			SRegFactoryNode* pFactoryNode = getHeadToRegFactories();

			while (pFactoryNode != nullptr)
			{
				if (pFactoryNode->m_pFactory->ClassSupports(cryiidof<Cry::IEnginePlugin>()))
				{
					nativeModule.m_pFactory = pFactoryNode->m_pFactory;
					break;
				}

				pFactoryNode = pFactoryNode->m_pNext;
			}

			if (nativeModule.m_pFactory == nullptr)
			{
				const string errorMessage = string().Format("Plugin load failed - valid ICryPlugin implementation was not found in plugin %s!", binaryName.c_str());
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			ICryUnknownPtr pUnk = nativeModule.m_pFactory->CreateClassInstance();
			std::shared_ptr<Cry::IEnginePlugin> pPlugin = cryinterface_cast<Cry::IEnginePlugin>(pUnk);
			if (pPlugin == nullptr)
			{
				const string errorMessage = string().Format("Plugin load failed - failed to create plug-in class instance in %s!", binaryName.c_str());
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			m_pluginContainer.emplace_back(pPlugin, std::move(nativeModule));

			break;
		}

	case EType::Managed:
		{
			if (gEnv->pMonoRuntime == nullptr)
			{
				const string errorMessage = string().Format("Plugin load failed - Tried to load Mono plugin %s without having loaded the CryMono module!", binaryName.c_str());
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			std::shared_ptr<Cry::IEnginePlugin> pPlugin = gEnv->pMonoRuntime->LoadBinary(szBinaryPath);
			if (pPlugin == nullptr)
			{
				const string errorMessage = string().Format("Plugin load failed - Plugin load failed - Could not load Mono binary %s!", binaryName.c_str());
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			m_pluginContainer.emplace_back(pPlugin);
			break;
		}
	}

	return OnPluginLoaded(notifyUserOnFailure);
}

bool CCryPluginManager::LoadPluginFromFile(const char* szPluginFile, bool notifyUserOnFailure)
{
	struct SPluginFile
	{
		struct SInfo
		{
			int api_version;
			int release_version;
			EType type;
			CryGUID guid;
			string display_name;
			
			void Serialize(Serialization::IArchive& ar)
			{
				if (ar.isInput())
				{
					string typeString;
					ar(typeString, "type");

					if (typeString == "native")
						type = EType::Native;
					else if (typeString == "managed")
						type = EType::Managed;
				}
				else if (ar.isOutput())
				{
					string typeString;
					if (type == EType::Native)
						typeString = "native";
					else if (type == EType::Managed)
						typeString = "managed";

					ar(typeString, "type");
				}

				ar(api_version, "api_version");
				ar(release_version, "release_version");
				ar(guid, "guid");
				ar(display_name, "display_name");
			}
		};

		struct SContent
		{
			std::vector<string> binaries;

			void Serialize(Serialization::IArchive& ar)
			{
				ar(binaries, "binaries");
			}
		};

		struct SRequire
		{
			struct SDependency
			{
				CryGUID guid;
				string name;

				void Serialize(Serialization::IArchive& ar)
				{
					ar(guid, "guid");
					ar(name, "name");
				}
			};

			std::vector<SDependency> plugins;

			void Serialize(Serialization::IArchive& ar)
			{
				ar(plugins, "plugins");
			}
		};

		int version;
		SInfo info;
		SContent content;
		SRequire require;

		void Serialize(Serialization::IArchive& ar)
		{
			ar(version, "version");
			ar(info, "info");
			ar(content, "content");
			ar(require, "require");
		}
	};

#if CRY_PLATFORM_WINDOWS
	SPluginFile plugin;
	if (!gEnv->pSystem->GetArchiveHost()->LoadJsonFile(Serialization::SStruct(plugin), szPluginFile, true))
	{
		const string errorMessage = string().Format("Plugin load failed - could not parse plugin file %s", szPluginFile);
		CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
		if (notifyUserOnFailure)
		{
			CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
		}
		return false;
	}

	// Load all dependencies
	for (const auto& it : plugin.require.plugins)
	{
		if (!LoadPluginByGUID(it.guid, notifyUserOnFailure))
			return false;
	}

	// Load all binaries specified in the .cryplugin file
	constexpr const char* szBinFolder = "bin\\win_x64";

	const string pluginRoot = PathUtil::GetPathWithoutFilename(szPluginFile);
	const string binaryDirectory = PathUtil::Make(pluginRoot, szBinFolder);
	for (const auto& it : plugin.content.binaries)
		LoadPluginBinary(plugin.info.type, PathUtil::Make(binaryDirectory, it));

	return true;
#else
	CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Plugins are currently not supported on this platform");
	return false;
#endif
}

bool CCryPluginManager::LoadPluginByGUID(CryGUID guid, bool notifyUserOnFailure)
{
	const auto it = m_registedPlugins.find(guid);
	if (it != std::end(m_registedPlugins))
		return LoadPluginFromFile(it->second.uri.c_str(), notifyUserOnFailure);

	const string errorMessage = string().Format("Plugin load failed - could not find %s in the plugin registry", guid.ToString().c_str());
	CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
	if (notifyUserOnFailure)
	{
		CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
	}

	return false;
}
#endif

bool CCryPluginManager::OnPluginLoaded(bool notifyUserOnFailure)
{
	SPluginContainer& containedPlugin = m_pluginContainer.back();

	if (!containedPlugin.Initialize(*gEnv, m_systemInitParams))
	{
		const string errorMessage = string().Format("Plugin load failed - Initialization failure, check log for possible errors");
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, errorMessage.c_str());
		if (notifyUserOnFailure)
		{
			CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
		}

		if (containedPlugin.m_pPlugin != nullptr)
		{
			OnPluginUnloaded(containedPlugin.m_pPlugin.get());
		}

		containedPlugin.Shutdown();
		m_pluginContainer.pop_back();

		return false;
	}

	// Notification to listeners, that plugin got initialized
	NotifyEventListeners(containedPlugin.m_pluginClassId, IEventListener::EEvent::Initialized);
	return true;
}

void CCryPluginManager::OnPluginUnloaded(Cry::IEnginePlugin* pPlugin)
{
	using TUpdateStep = std::underlying_type<Cry::IEnginePlugin::EUpdateStep>::type;

	for (TUpdateStep i = 0; i < static_cast<TUpdateStep>(Cry::IEnginePlugin::EUpdateStep::Count); ++i)
	{
		std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(static_cast<Cry::IEnginePlugin::EUpdateStep>(1 << i));
		stl::find_and_erase(updatedPlugins, pPlugin);
	}
}

bool CCryPluginManager::UnloadAllPlugins()
{
	bool bError = false;
	for (auto it = m_pluginContainer.rbegin(); it != m_pluginContainer.rend(); ++it)
	{
		if (it->m_pPlugin != nullptr)
		{
			OnPluginUnloaded(it->m_pPlugin.get());
		}

		if (!it->Shutdown())
		{
			bError = true;
		}

		// notification to listeners, that plugin got un-initialized
		NotifyEventListeners(it->m_pluginClassId, IEventListener::EEvent::Unloaded);
	}

	m_pluginContainer.clear();

	return !bError;
}

void CCryPluginManager::NotifyEventListeners(const CryClassID& classID, IEventListener::EEvent event)
{
	for (const TPluginListenerPair& pluginPair : m_pluginListenerMap)
	{
		if (std::find(pluginPair.second.cbegin(), pluginPair.second.cend(), classID) != pluginPair.second.cend())
		{
			pluginPair.first->OnPluginEvent(classID, event);
		}
	}
}

bool CCryPluginManager::ParsePluginRegistry()
{
	struct SRegisteredPluginWrapper
	{
		void Serialize(Serialization::IArchive& ar)
		{
			struct SEngine
			{
				void Serialize(Serialization::IArchive& ar)
				{
					if (ar.isInput())
					{
						std::map<string, TRegisteredPluginList::mapped_type> temp;
						ar(temp, "plugins");

						for (const auto& it : temp)
							plugins[CryGUID::FromString(it.first)] = it.second;
					}
					else if (ar.isOutput())
					{
						std::map<string, TRegisteredPluginList::mapped_type> temp;
						for (const auto& it : plugins)
							temp[it.first.ToString()] = it.second;

						ar(temp, "plugins");
					}

					ar(uri, "uri");
				}

				TRegisteredPluginList plugins;
				string uri;
			};

			std::map<string, SEngine> engines;
			ar(engines, "engines");

			const char* szEngineID = gEnv->pSystem->GetIProjectManager()->GetCurrentEngineID();
			if (strcmp(szEngineID, ".") != 0)
			{
				const auto it = engines.find(szEngineID);
				if (it != std::end(engines))
					plugins = it->second.plugins;
			}
			else
			{
				string currentEngineRoot = PathUtil::GetEnginePath();
				PathUtil::UnifyFilePath(currentEngineRoot);
				if (currentEngineRoot[currentEngineRoot.size() - 1] != '/')
					currentEngineRoot.append("/");

				for (const auto& it : engines)
				{
					string engineRoot = PathUtil::GetPathWithoutFilename(it.second.uri);
					PathUtil::UnifyFilePath(engineRoot);
					if (currentEngineRoot == engineRoot)
					{
						plugins = it.second.plugins;
						break;
					}
				}
			}
		}

		TRegisteredPluginList& plugins;
	};

	m_registedPlugins.clear();

#if CRY_PLATFORM_WINDOWS
	PWSTR programData;
	if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData) != S_OK)
		return false;

	const string pluginRegistryPath = PathUtil::Make(CryStringUtils::WStrToUTF8(programData), "Crytek\\CRYENGINE\\cryengine.json");

	CoTaskMemFree(programData);

	// Yasli can't serialize std::map from the root of a JSON document, so we need to create a temporary wrapper here
	constexpr const char* szJsonPrefix = "{\"engines\":";
	constexpr const char* szJsonPostfix = "}";
	const size_t prefixLength = strlen(szJsonPrefix);
	const size_t postfixLength = strlen(szJsonPostfix);

	const size_t fileSize = gEnv->pCryPak->FGetSize(pluginRegistryPath.c_str(), true);
	std::vector<char> buff(fileSize + prefixLength + postfixLength);
	strcpy(&buff[0], szJsonPrefix);

	FILE* pFile = gEnv->pCryPak->FOpenRaw(pluginRegistryPath.c_str(), "r");
	if (pFile == nullptr)
		return false;
	const size_t bytesRead = gEnv->pCryPak->FReadRaw(&buff[prefixLength], sizeof(char), fileSize, pFile);
	gEnv->pCryPak->FClose(pFile);
	strcpy(&buff[prefixLength + bytesRead], szJsonPostfix);


	if (!gEnv->pSystem->GetArchiveHost()->LoadJsonBuffer(Serialization::SStruct(SRegisteredPluginWrapper{ m_registedPlugins }), buff.data(), fileSize))
		return false;
#else
	CRY_ASSERT_MESSAGE(false, "Reading the plugin registry is not supported on this platform!");
	return false;
#endif // CRY_PLATFORM_WINDOWS

	return true;
}

void CCryPluginManager::UpdateBeforeSystem()
{
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::BeforeSystem);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->UpdateBeforeSystem();
	}
}

void CCryPluginManager::UpdateBeforePhysics()
{
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::BeforePhysics);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->UpdateBeforePhysics();
	}
}

void CCryPluginManager::UpdateAfterSystem()
{
	const float frameTime = gEnv->pTimer->GetFrameTime();
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::MainUpdate);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->MainUpdate(frameTime);
	}
}

void CCryPluginManager::UpdateBeforeFinalizeCamera()
{
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::BeforeFinalizeCamera);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->UpdateBeforeFinalizeCamera();
	}
}

void CCryPluginManager::UpdateBeforeRender()
{
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::BeforeRender);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->UpdateBeforeRender();
	}
}

void CCryPluginManager::UpdateAfterRender()
{
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::AfterRender);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->UpdateAfterRender();
	}
}

void CCryPluginManager::UpdateAfterRenderSubmit()
{
	const std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep::AfterRenderSubmit);
	for (Cry::IEnginePlugin* pPlugin : updatedPlugins)
	{
		pPlugin->UpdateAfterRenderSubmit();
	}
}

std::shared_ptr<Cry::IEnginePlugin> CCryPluginManager::QueryPluginById(const CryClassID& classID) const
{
	for (const SPluginContainer& it : m_pluginContainer)
	{
		CRY_ASSERT(it.GetPluginPtr() != nullptr);
		if (ICryFactory* pFactory = it.GetPluginPtr()->GetFactory())
		{
			if (pFactory->GetClassID() == classID || pFactory->ClassSupports(classID))
			{
				return it.m_pPlugin;
			}
		}
	}

	return nullptr;
}

void CCryPluginManager::OnPluginUpdateFlagsChanged(Cry::IEnginePlugin& plugin, uint8 newFlags, uint8 changedStep)
{
	std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(static_cast<Cry::IEnginePlugin::EUpdateStep>(changedStep));
	if ((newFlags & changedStep) != 0)
	{
		updatedPlugins.push_back(&plugin);
	}
	else
	{
		stl::find_and_erase(updatedPlugins, &plugin);
	}
}

CRY_TEST_SUITE(CryPluginSystemTest)
{

	// Minimal plug-in implementation example, initializing and receiving per-frame Update callbacks
	class CTestPlugin final : public Cry::IEnginePlugin
	{
		// Start declaring the inheritance hierarchy for this plug-in
		// This is followed by any number of CRYINTERFACE_ADD, passing an interface implementing ICryUnknown that has declared its own GUID using CRYINTERFACE_DECLARE_GUID
		CRYINTERFACE_BEGIN()
			// Indicate that we implement Cry::IEnginePlugin, this is mandatory in order for the plug-in instance to be detected after the plug-in has been loaded
			CRYINTERFACE_ADD(Cry::IEnginePlugin)
			// End the declaration of inheritance hierarchy
			CRYINTERFACE_END()

			// Set the GUID for our plug-in, this should be unique across all used plug-ins
			// Can be generated in Visual Studio under Tools -> Create GUID
			CRYGENERATE_SINGLETONCLASS_GUID(CTestPlugin, "TestPlugin", "{213DF908-0944-44F3-A501-70E6E4FE2E86}"_cry_guid)

			// Called shortly after loading the plug-in from disk
			// This is usually where you would initialize any third-party APIs and custom code
			virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
		{
			/* Initialize plug-in here */

			// Make sure that we receive per-frame call to MainUpdate
			EnableUpdate(IEnginePlugin::EUpdateStep::MainUpdate, true);

			return true;
		}

		virtual void MainUpdate(float frameTime) override
		{
			MainUpdateCount++;
		}

	public:

		static int MainUpdateCount;
	};

	int CTestPlugin::MainUpdateCount = 0;

	// Register the factory that can create this plug-in instance
	// Note that this has to be done in a source file that is not included anywhere else.
	CRYREGISTER_SINGLETON_CLASS(CTestPlugin);

	void CheckCTestPluginUpdateStatus()
	{
		CRY_TEST_ASSERT(CTestPlugin::MainUpdateCount > 0);
	}

	//http://jira.cryengine.com/browse/CE-17006
	CRY_TEST(PluginSystemTest, timeout = 10.f)
	{
		commands =
		{
			CryTest::CCommandWait(1.f),
			CheckCTestPluginUpdateStatus
		};
	}
}
