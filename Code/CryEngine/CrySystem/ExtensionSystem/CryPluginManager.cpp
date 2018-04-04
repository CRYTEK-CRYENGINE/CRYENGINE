// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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
	SNativePluginModule() {}

	SNativePluginModule(const char* path)
		: m_engineModulePath(path)
		, m_pFactory(nullptr)
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "LoadPlugin");
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "%s", path);

		WIN_HMODULE hModule = static_cast<CSystem*>(gEnv->pSystem)->LoadDynamicLibrary(path, false);
		if (hModule != nullptr)
		{
			// Find the first ICryPlugin instance inside the module
			PtrFunc_GetHeadToRegFactories getHeadToRegFactories = (PtrFunc_GetHeadToRegFactories)CryGetProcAddress(hModule, "GetHeadToRegFactories");
			SRegFactoryNode* pFactoryNode = getHeadToRegFactories();

			while (pFactoryNode != nullptr)
			{
				if (pFactoryNode->m_pFactory->ClassSupports(cryiidof<ICryPlugin>()))
				{
					m_pFactory = pFactoryNode->m_pFactory;
					break;
				}

				pFactoryNode = pFactoryNode->m_pNext;
			}

			if (m_pFactory == nullptr)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Plugin load failed - valid ICryPlugin implementation was not found in plugin %s!", path);

				MarkUnloaded();
				return;
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Plugin load failed, could not find dynamic library %s!", path);

			MarkUnloaded();
		}
	}

	SNativePluginModule(SNativePluginModule& other)
	{
		m_engineModulePath = other.m_engineModulePath;
		
		other.MarkUnloaded();
	}

	SNativePluginModule(SNativePluginModule&& other)
	{
		m_engineModulePath = other.m_engineModulePath;
		
		other.MarkUnloaded();
	}

	SNativePluginModule& operator=(const SNativePluginModule&& other)
	{
		m_engineModulePath = other.m_engineModulePath;
		
		return *this;
	}

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

	ICryFactory* GetFactory() const { return m_pFactory; }

protected:
	string m_engineModulePath;
	ICryFactory* m_pFactory;
};

struct SPluginContainer
{
	// Constructor for native plug-ins
	SPluginContainer(const std::shared_ptr<ICryPlugin>& plugin, SNativePluginModule&& module)
		: m_pPlugin(plugin)
		, m_module(module)
		, m_pluginClassId(plugin->GetFactory()->GetClassID())
	{
	}

	// Constructor for managed (Mono) plug-ins, or statically linked ones
	SPluginContainer(const std::shared_ptr<ICryPlugin>& plugin)
		: m_pPlugin(plugin) 
	{
		if (ICryFactory* pFactory = plugin->GetFactory())
		{
			m_pluginClassId = pFactory->GetClassID();
		}
	}

	bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		bool bSuccess = false;

		if (m_pPlugin)
		{
			m_pPlugin->SetUpdateFlags(IPluginUpdateListener::EUpdateType_NoUpdate);
			bSuccess = m_pPlugin->Initialize(env, initParams);
		}

		return bSuccess;
	}

	bool Shutdown()
	{
		m_pPlugin->UnregisterFlowNodes();

		m_pPlugin.reset();

		return m_module.Shutdown();
	}

	ICryPlugin* GetPluginPtr() const
	{
		return m_pPlugin ? m_pPlugin.get() : nullptr;
	}

	friend bool operator==(const SPluginContainer& left, const SPluginContainer& right)
	{
		return (left.GetPluginPtr() == right.GetPluginPtr());
	}

	CryClassID                     m_pluginClassId;
	string                         m_pluginAssetDirectory;

	ICryPluginManager::EPluginType m_pluginType;

	std::shared_ptr<ICryPlugin>    m_pPlugin;

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
	// Find out how many ICryPlugin implementations are available
	size_t numFactories;
	gEnv->pSystem->GetCryFactoryRegistry()->IterateFactories(cryiidof<ICryPlugin>(), nullptr, numFactories);

	std::vector<ICryFactory*> factories;
	factories.resize(numFactories);

	// Start by looking for any ICryPlugin implementation in the current module, in case of static linking
	gEnv->pSystem->GetCryFactoryRegistry()->IterateFactories(cryiidof<ICryPlugin>(), factories.data(), numFactories);
	for (size_t i = 0; i < numFactories; ++i)
	{
		// Create an instance of the plug-in
		ICryUnknownPtr pUnknown = factories[i]->CreateClassInstance();
		std::shared_ptr<ICryPlugin> pPlugin = cryinterface_cast<ICryPlugin>(pUnknown);

		m_pluginContainer.emplace_back(pPlugin);

		OnPluginLoaded();
	}

	m_bLoadedProjectPlugins = true;

	// Load plug-ins specified in the .cryproject file from disk
	CProjectManager* pProjectManager = static_cast<CProjectManager*>(gEnv->pSystem->GetIProjectManager());
	const std::vector<SPluginDefinition>& pluginDefinitions = pProjectManager->GetPluginDefinitions();

	for (const SPluginDefinition& pluginDefinition : pluginDefinitions)
	{
#if defined(CRY_IS_MONOLITHIC_BUILD)
		// Don't attempt to load plug-ins that were statically linked in
		string pluginName = PathUtil::GetFileName(pluginDefinition.path);
		auto defaultPluginsArray = GetDefaultPlugins();
		bool bValid = true;

		for (const char* szPluginName : defaultPluginsArray)
		{
			if (!strcmp(szPluginName, pluginName.c_str()))
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

#if !defined(CRY_IS_MONOLITHIC_BUILD)
	// Always load the CryUserAnalytics plugin
	SPluginDefinition userAnalyticsPlugin(EPluginType::Native, "CryUserAnalytics");
	if (std::find(std::begin(pluginDefinitions), std::end(pluginDefinitions), userAnalyticsPlugin) == std::end(pluginDefinitions))
	{
		LoadPluginFromDisk(userAnalyticsPlugin.type, userAnalyticsPlugin.path);
	}
#endif
}

bool CCryPluginManager::LoadPluginFromDisk(EPluginType type, const char* path)
{
	CRY_ASSERT_MESSAGE(m_bLoadedProjectPlugins, "Plug-ins must not be loaded before LoadProjectPlugins!");

	CryLogAlways("Loading plug-in %s", path);

	std::shared_ptr<ICryPlugin> pPlugin;

	switch (type)
	{
	case EPluginType::Native:
		{
			// Load the module, note that this calls ISystem::InitializeEngineModule
			// Automatically unloads in destructor
			SNativePluginModule module(path);
			ICryFactory* pFactory = module.GetFactory();
			if (pFactory == nullptr)
			{
				return false;
			}

			ICryUnknownPtr pUnk = pFactory->CreateClassInstance();
			pPlugin = cryinterface_cast<ICryPlugin>(pUnk);
			if (pPlugin == nullptr)
			{
				return false;
			}

			m_pluginContainer.emplace_back(pPlugin, std::move(module));
			module.MarkUnloaded();

			break;
		}

	case EPluginType::Managed:
		{
			if (gEnv->pMonoRuntime == nullptr)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Tried to load Mono plugin %s without having loaded the CryMono module!", path);

				return false;
			}

			pPlugin = gEnv->pMonoRuntime->LoadBinary(path);
			if (!pPlugin)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Plugin load failed - Could not load Mono binary %s", path);

				return false;
			}

			m_pluginContainer.emplace_back(pPlugin);
			break;
		}
	}

	return OnPluginLoaded();
}

bool CCryPluginManager::OnPluginLoaded()
{
	SPluginContainer& containedPlugin = m_pluginContainer.back();

	if (!containedPlugin.Initialize(*gEnv, m_systemInitParams))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Plugin load failed - Failed to initialize!");

		containedPlugin.Shutdown();
		m_pluginContainer.pop_back();

		return false;
	}

	// Notification to listeners, that plugin got initialized
	NotifyEventListeners(containedPlugin.m_pluginClassId, IPluginEventListener::EPluginEvent::Initialized);
	return true;
}

bool CCryPluginManager::UnloadAllPlugins()
{
	bool bError = false;
	for(auto it = m_pluginContainer.rbegin(), end = m_pluginContainer.rend(); it != end; ++it)
	{
		if (!it->Shutdown())
		{
			bError = true;
		}

		// notification to listeners, that plugin got un-initialized
		NotifyEventListeners(it->m_pluginClassId, IPluginEventListener::EPluginEvent::Unloaded);
	}

	m_pluginContainer.clear();

	return !bError;
}

void CCryPluginManager::NotifyEventListeners(const CryClassID& classID, IPluginEventListener::EPluginEvent event)
{
	for (const auto& it : m_pluginListenerMap)
	{
		const std::vector<CryClassID>& classIDs = it.second;
		if (std::find(classIDs.cbegin(), classIDs.cend(), classID) != classIDs.cend())
		{
			it.first->OnPluginEvent(classID, event);
		}
	}
}

void CCryPluginManager::Update(IPluginUpdateListener::EPluginUpdateType updateFlags)
{
	for (const SPluginContainer& it : m_pluginContainer)
	{
		ICryPlugin* pPlugin = it.GetPluginPtr();
		CRY_ASSERT(pPlugin != nullptr);
		if ((pPlugin->GetUpdateFlags() & updateFlags) != 0)
		{
			pPlugin->OnPluginUpdate(updateFlags);
		}
	}
}

std::shared_ptr<ICryPlugin> CCryPluginManager::QueryPluginById(const CryClassID& classID) const
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


std::shared_ptr<ICryPlugin> CCryPluginManager::AcquirePluginById(const CryClassID& classID)
{
	std::shared_ptr<ICryPlugin> pPlugin;
	CryCreateClassInstance(classID, pPlugin);

	if (pPlugin == nullptr)
	{
		return nullptr;
	}

	m_pluginContainer.emplace_back(pPlugin);

	if (OnPluginLoaded())
	{
		return pPlugin;
	}

	return nullptr;
}
