// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
		}

		if (m_pFactory == nullptr)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Plugin load failed - valid ICryPlugin implementation was not found in plugin %s!", path);

			MarkUnloaded();
			return;
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
		, m_bFailedInit(false)
	{
	}

	// Constructor for managed (Mono) plug-ins, or statically linked ones
	SPluginContainer(const std::shared_ptr<ICryPlugin>& plugin)
		: m_pPlugin(plugin)
		, m_bFailedInit(false)
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

		m_bFailedInit = !bSuccess;

		return bSuccess;
	}

	bool Shutdown()
	{
		m_pPlugin->UnregisterFlowNodes();
		CRY_ASSERT_MESSAGE(m_pPlugin.use_count() == 2, m_pPlugin->GetName()); // Referenced here and in the factory
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

	bool						   m_bFailedInit;

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

	}

	m_bLoadedProjectPlugins = true;

	// Load plug-ins specified in the .cryproject file from disk
	CProjectManager* pProjectManager = static_cast<CProjectManager*>(gEnv->pSystem->GetIProjectManager());
	const std::vector<SPluginDefinition>& pluginDefinitions = pProjectManager->GetPluginDefinitions();

	for (const SPluginDefinition& pluginDefinition : pluginDefinitions)
	{
		LoadModule(pluginDefinition.type, pluginDefinition.path);
	}

	// Always load the CryUserAnalytics plugin
	SPluginDefinition userAnalyticsPlugin(EPluginType::Native, "CryUserAnalytics");
	if (std::find(std::begin(pluginDefinitions), std::end(pluginDefinitions), userAnalyticsPlugin) == std::end(pluginDefinitions))
	{
		LoadModule(userAnalyticsPlugin.type, userAnalyticsPlugin.path);
	}

	// Danger Robinson! Dragons Ahead!
	// Don't touch m_pluginContainers until we are done with containerPtrs!

	// Make temp list we can sort without moving modules around
	std::vector<SPluginContainer*> containerPtrs;
	for (auto & container : m_pluginContainer)
		containerPtrs.push_back(&container);

	// Sort containers by load priority
	std::sort(containerPtrs.begin(), containerPtrs.end(),
	[](const SPluginContainer* lhs, const SPluginContainer* rhs) -> bool
	{
		if (lhs->m_pPlugin == nullptr || rhs->m_pPlugin == nullptr)
			return false;
		return (lhs->m_pPlugin->m_loadPriority > rhs->m_pPlugin->m_loadPriority); 
	});

	// Initialize in order, erase failed-init plugins
	for (auto & container : containerPtrs)
		if(!container->Initialize(*gEnv, m_systemInitParams))
			container->Shutdown();
		else
			NotifyEventListeners(container->m_pluginClassId, IPluginEventListener::EPluginEvent::Initialized);
	containerPtrs.clear();

	// clean failed containers
	m_pluginContainer.erase(
		std::remove_if(m_pluginContainer.begin(), m_pluginContainer.end(),
			[](const SPluginContainer & o) { return o.m_bFailedInit; }),
		m_pluginContainer.end());
}

// Manually loaded plugins will of course be loaded as and when requested.
bool CCryPluginManager::LoadPluginFromDisk(EPluginType type, const char* path)
{
	CRY_ASSERT_MESSAGE(m_bLoadedProjectPlugins, "Plug-ins must not be loaded before LoadProjectPlugins!");

	if(LoadModule(type, path))
	{
		if (m_pluginContainer.back().Initialize(*gEnv, m_systemInitParams))
		{
			NotifyEventListeners(m_pluginContainer.back().m_pluginClassId, IPluginEventListener::EPluginEvent::Initialized);
			return true;
		}
		else
		{
			m_pluginContainer.back().Shutdown();
			m_pluginContainer.pop_back();
		}
	}

	return false;
}

bool CCryPluginManager::LoadModule(EPluginType type, const char* path)
{
	CryLogAlways("Loading plugin module %s", path);

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

	return true;
}

bool CCryPluginManager::UnloadAllPlugins()
{
	bool bError = false;

	// Unload in specified order (matching priorities in reverse)
	std::vector<SPluginContainer*> containerPtrs;
	for (auto& container : m_pluginContainer)
		containerPtrs.push_back(&container);

	// Reverse order
	std::reverse(containerPtrs.begin(), containerPtrs.end());

	// Sort containers by unload priority
	std::sort(containerPtrs.begin(), containerPtrs.end(),
		[](const SPluginContainer* lhs, const SPluginContainer* rhs) -> bool
	{
		if (lhs->m_pPlugin == nullptr || rhs->m_pPlugin == nullptr)
			return false;
		return (lhs->m_pPlugin->m_unloadPriority > rhs->m_pPlugin->m_unloadPriority);
	});

	// Shutdown and notify
	for (const auto& containerPtr : containerPtrs)
	{
		if (!containerPtr->Shutdown())
			bError = true;
		NotifyEventListeners(containerPtr->m_pluginClassId, IPluginEventListener::EPluginEvent::Unloaded);
	}

	// Cleanup
	containerPtrs.clear();
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

	if (pPlugin != nullptr)
	{
		m_pluginContainer.emplace_back(pPlugin);

		if (m_pluginContainer.back().Initialize(*gEnv, m_systemInitParams))
		{
			NotifyEventListeners(m_pluginContainer.back().m_pluginClassId, IPluginEventListener::EPluginEvent::Initialized);
			return pPlugin;
		}
		else
		{
			m_pluginContainer.back().Shutdown();
			m_pluginContainer.pop_back();
		}
	}

	return nullptr;
}
