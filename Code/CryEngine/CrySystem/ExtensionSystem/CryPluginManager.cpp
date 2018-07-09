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

// Descriptor for the binary file of a plugin.
// This is separate since a plugin does not necessarily have to come from a binary, for example if static linking is used.
struct SNativePluginModule
{
	SNativePluginModule() : m_pFactory(nullptr) {}
	SNativePluginModule(const char* szPath, ICryFactory* pFactory = nullptr)
		: m_engineModulePath(szPath)
		, m_pFactory(pFactory) {}
	SNativePluginModule(SNativePluginModule& other)
		: m_engineModulePath(other.m_engineModulePath)
		, m_pFactory(other.m_pFactory)
	{
		other.MarkUnloaded();
	}
	SNativePluginModule& operator=(const SNativePluginModule&) = default;
	SNativePluginModule(SNativePluginModule&& other)
		: m_engineModulePath(std::move(other.m_engineModulePath))
		, m_pFactory(other.m_pFactory)
	{
		other.MarkUnloaded();
	}
	SNativePluginModule& operator=(SNativePluginModule&&) = default;
	~SNativePluginModule()
	{
		Shutdown();
	}

	bool Shutdown()
	{
		bool bSuccess = false;
		if (m_engineModulePath.size() > 0)
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

	bool IsLoaded()
	{
		return m_engineModulePath.size() > 0;
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
		return m_pPlugin ? m_pPlugin.get() : nullptr;
	}

	friend bool operator==(const SPluginContainer& left, const SPluginContainer& right)
	{
		return (left.GetPluginPtr() == right.GetPluginPtr());
	}

	CryClassID                     m_pluginClassId;
	string                         m_pluginAssetDirectory;

	Cry::IPluginManager::EType     m_pluginType;

	std::shared_ptr<Cry::IEnginePlugin>  m_pPlugin;

	SNativePluginModule            m_module;
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

	if (gEnv->pConsole)
	{
		gEnv->pConsole->RemoveCommand("sys_reload_plugin");
		gEnv->pConsole->UnregisterVariable("sys_debug_plugin", true);
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
	// Find out how many Cry::IEnginePlugin implementations are available
	size_t numFactories;
	gEnv->pSystem->GetCryFactoryRegistry()->IterateFactories(cryiidof<Cry::IEnginePlugin>(), nullptr, numFactories);

	std::vector<ICryFactory*> factories;
	factories.resize(numFactories);

	// Start by looking for any Cry::IEnginePlugin implementation in the current module, in case of static linking
	gEnv->pSystem->GetCryFactoryRegistry()->IterateFactories(cryiidof<Cry::IEnginePlugin>(), factories.data(), numFactories);
	for (size_t i = 0; i < numFactories; ++i)
	{
		// Create an instance of the plug-in
		ICryUnknownPtr pUnknown = factories[i]->CreateClassInstance();
		std::shared_ptr<Cry::IEnginePlugin> pPlugin = cryinterface_cast<Cry::IEnginePlugin>(pUnknown);

		m_pluginContainer.emplace_back(pPlugin);

		OnPluginLoaded();
	}

	m_bLoadedProjectPlugins = true;

#if CrySharedLibrarySupported
	// Load plug-ins specified in the .cryproject file from disk
	CProjectManager* pProjectManager = static_cast<CProjectManager*>(gEnv->pSystem->GetIProjectManager());
	const std::vector<SPluginDefinition>& pluginDefinitions = pProjectManager->GetPluginDefinitions();

	for (const SPluginDefinition& pluginDefinition : pluginDefinitions)
	{
		if (!pluginDefinition.platforms.empty() && !stl::find(pluginDefinition.platforms, EPlatform::Current))
		{
			continue;
		}

#if defined(CRY_IS_MONOLITHIC_BUILD)
		// Don't attempt to load plug-ins that were statically linked in
		string pluginName = PathUtil::GetFileName(pluginDefinition.path);
		bool bValid = true;

		for (const std::pair<uint8, SPluginDefinition>& defaultPlugin : CCryPluginManager::GetDefaultPlugins())
		{
			if (!strcmp(defaultPlugin.second.path, pluginName.c_str()))
			{
				bValid = false;
				break;
			}
		}

		if (!bValid)
		{
			continue;
		}
#endif

		LoadPluginFromDisk(pluginDefinition.type, pluginDefinition.path);
	}

#if !defined(CRY_IS_MONOLITHIC_BUILD) && !defined(CRY_PLATFORM_CONSOLE)
	// Always load the CryUserAnalytics plugin
	SPluginDefinition userAnalyticsPlugin{ EType::Native, "CryUserAnalytics" };
	if (std::find(std::begin(pluginDefinitions), std::end(pluginDefinitions), userAnalyticsPlugin) == std::end(pluginDefinitions))
	{
		LoadPluginFromDisk(userAnalyticsPlugin.type, userAnalyticsPlugin.path, false);
	}
#endif
#endif // CrySharedLibrarySupported
}

#if CrySharedLibrarySupported
bool CCryPluginManager::LoadPluginFromDisk(EType type, const char* szPath, bool notifyUserOnFailure)
{
	CRY_ASSERT_MESSAGE(m_bLoadedProjectPlugins, "Plug-ins must not be loaded before LoadProjectPlugins!");

	CryLogAlways("Loading plug-in %s", szPath);

	switch (type)
	{
	case EType::Native:
		{
			MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "LoadPlugin");
			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "%s", szPath);

			WIN_HMODULE hModule = static_cast<CSystem*>(gEnv->pSystem)->LoadDynamicLibrary(szPath, false);
			if (hModule == nullptr)
			{
				string errorMessage = string().Format("Plugin load failed, could not find dynamic library %s!", szPath);
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			// Create RAII struct to ensure that module is unloaded on failure
			SNativePluginModule nativeModule(szPath);

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
				string errorMessage = string().Format("Plugin load failed - valid ICryPlugin implementation was not found in plugin %s!", szPath);
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
				string errorMessage = string().Format("Plugin load failed - failed to create plug-in class instance in %s!", szPath);
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
				string errorMessage = string().Format("Plugin load failed - Tried to load Mono plugin %s without having loaded the CryMono module!", szPath);
				CryWarning(VALIDATOR_MODULE_SYSTEM, notifyUserOnFailure ? VALIDATOR_ERROR : VALIDATOR_COMMENT, errorMessage.c_str());
				if (notifyUserOnFailure)
				{
					CryMessageBox(errorMessage.c_str(), "Plug-in load failed!", eMB_Error);
				}

				return false;
			}

			std::shared_ptr<Cry::IEnginePlugin> pPlugin = gEnv->pMonoRuntime->LoadBinary(szPath);
			if (pPlugin == nullptr)
			{
				string errorMessage = string().Format("Plugin load failed - Plugin load failed - Could not load Mono binary %s!", szPath);
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
#endif

bool CCryPluginManager::OnPluginLoaded(bool notifyUserOnFailure)
{
	SPluginContainer& containedPlugin = m_pluginContainer.back();

	if (!containedPlugin.Initialize(*gEnv, m_systemInitParams))
	{
		string errorMessage = string().Format("Plugin load failed - Initialization failure, check log for possible errors");
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
	for (uint8 i = 0; i < static_cast<uint8>(Cry::IEnginePlugin::EUpdateStep::Count); ++i)
	{
		std::vector<Cry::IEnginePlugin*>& updatedPlugins = GetUpdatedPluginsForStep(static_cast<Cry::IEnginePlugin::EUpdateStep>(1 << i));

		stl::find_and_erase(updatedPlugins, pPlugin);
	}
}

bool CCryPluginManager::UnloadAllPlugins()
{
	bool bError = false;
	for(auto it = m_pluginContainer.rbegin(), end = m_pluginContainer.rend(); it != end; ++it)
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
	float frameTime = gEnv->pTimer->GetFrameTime();

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
		ICryFactory* pFactory = it.GetPluginPtr()->GetFactory();
		if (pFactory)
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