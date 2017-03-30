// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryPluginManager.h"

#include "System.h"

#include <CryExtension/ICryFactory.h>
#include <CryExtension/ICryFactoryRegistry.h>
#include <CryExtension/CryCreateClassInstance.h>

#include <CryMono/IMonoRuntime.h>

#include <CryCore/Platform/CryLibrary.h>
CCryPluginManager* CCryPluginManager::s_pThis = 0;

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

	CryClassID                     m_pluginClassId;
	string                         m_pluginAssetDirectory;

	ICryPluginManager::EPluginType m_pluginType;

	std::shared_ptr<ICryPlugin>    m_pPlugin;

	SNativePluginModule            m_module;
};

void CCryPluginManager::ReloadPluginCmd(IConsoleCmdArgs* pArgs)
{
	s_pThis->UnloadAllPlugins();
	s_pThis->LoadExtensionFile("cryplugin.csv");
	s_pThis->OnSystemEvent(ESYSTEM_EVENT_GAME_POST_INIT_DONE, 0, 0);
}

CCryPluginManager::CCryPluginManager(const SSystemInitParams& initParams)
	: m_systemInitParams(initParams)
{
	s_pThis = this;

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

void CCryPluginManager::Initialize()
{
#ifndef _RELEASE
	REGISTER_COMMAND("sys_plugin_reload", ReloadPluginCmd, 0, "Reload a plugin - not implemented yet");
#endif

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

	// Move on to loading from disk
	// Start with loading the default engine plug-ins
	LoadPluginFromDisk(EPluginType::Native, "CryDefaultEntities");
	//Schematyc + Schematyc Standard Enviroment
	LoadPluginFromDisk(EPluginType::Native, "CrySchematycCore");
	LoadPluginFromDisk(EPluginType::Native, "CrySchematycSTDEnv");
	//optional plugin, but we load it for now by default

	LoadPluginFromDisk(EPluginType::Native, "CrySensorSystem");

	LoadExtensionFile("cryplugin.csv");
}

//--- UTF8 parse helper routines

static const char* Parser_NextChar(const char* pStart, const char* pEnd)
{
	CRY_ASSERT(pStart != nullptr && pEnd != nullptr);

	if (pStart < pEnd)
	{
		CRY_ASSERT(0 <= *pStart && *pStart <= SCHAR_MAX);
		pStart++;
	}

	CRY_ASSERT(pStart <= pEnd);
	return pStart;
}

static const char* Parser_StrChr(const char* pStart, const char* pEnd, int c)
{
	CRY_ASSERT(pStart != nullptr && pEnd != nullptr);
	CRY_ASSERT(pStart <= pEnd);
	CRY_ASSERT(0 <= c && c <= SCHAR_MAX);
	const char* it = (const char*)memchr(pStart, c, pEnd - pStart);
	return (it != nullptr) ? it : pEnd;
}

static bool Parser_StrEquals(const char* pStart, const char* pEnd, const char* szKey)
{
	size_t klen = strlen(szKey);
	return (klen == pEnd - pStart) && memcmp(pStart, szKey, klen) == 0;
}

//---

bool CCryPluginManager::LoadExtensionFile(const char* szFilename)
{
	CryLogAlways("Loading extension file %s", szFilename);

	CRY_ASSERT(szFilename != nullptr);
	bool bResult = true;

	ICryPak* pCryPak = gEnv->pCryPak;

	//! TODO recursive plugin dependency? plugin depends on plugin?
	FILE* pFile = nullptr;

#ifndef _RELEASE
	// This will not work in release mode (Incorrect flag passed to FOpen for release since release only reads from paks)
	pFile = pCryPak->FOpen(szFilename, "rb", ICryPak::FLAGS_PATH_REAL);
	if (pFile == nullptr)
		CryLogAlways("Warning: Cannot open extensions file (From DISK) '%s'", szFilename);
	if (pFile == nullptr) // For the pak check below
#endif
	pFile = pCryPak->FOpen(szFilename, "rb");
	if (pFile == nullptr)
	{
		CryLogAlways("Warning: Cannot open extensions file (From ASSETS) '%s'", szFilename);
		return bResult;
	}

	size_t iFileSize = pCryPak->FGetSize(pFile);
	char* pBuffer = new char[iFileSize];
	pCryPak->FReadRawAll(pBuffer, iFileSize, pFile);
	pCryPak->FClose(pFile);

	const char* pTokenStart = pBuffer;
	const char* pBufferEnd = pBuffer + iFileSize;
	while (pTokenStart != pBufferEnd)
	{
		const char* pNewline = Parser_StrChr(pTokenStart, pBufferEnd, '\n');
		const char* pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		ICryPluginManager::EPluginType pluginType = ICryPluginManager::EPluginType::Native;
		if (Parser_StrEquals(pTokenStart, pSemicolon, "C#"))
			pluginType = ICryPluginManager::EPluginType::Managed;

		// Parsing of plugin name
		// Not actually used anymore, but no need to break existing setup seeing as we're going to move away from csv soon anyway - Filip
		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		string pluginClassName;
		pluginClassName.assign(pTokenStart, pSemicolon - pTokenStart);
		pluginClassName.Trim();

		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		string pluginBinaryPath;
		pluginBinaryPath.assign(pTokenStart, pSemicolon - pTokenStart);
		pluginBinaryPath.Trim();

		pTokenStart = Parser_NextChar(pSemicolon, pNewline);
		pSemicolon = Parser_StrChr(pTokenStart, pNewline, ';');

		string pluginAssetDirectory;
		pluginAssetDirectory.assign(pTokenStart, pSemicolon - pTokenStart);
		pluginAssetDirectory.Trim();
	#pragma message("TODO: Use plugin asset directory")

		pTokenStart = Parser_NextChar(pNewline, pBufferEnd);
		if (!LoadPluginFromDisk(pluginType, pluginBinaryPath))
		{
			bResult = false;
		}
	}

	delete[](pBuffer);
	return bResult;
}

bool CCryPluginManager::LoadPluginFromDisk(EPluginType type, const char* path)
{
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
	for (SPluginContainer& it : m_pluginContainer)
	{
		if (!it.Shutdown())
		{
			bError = true;
		}

		// notification to listeners, that plugin got un-initialized
		NotifyEventListeners(it.m_pluginClassId, IPluginEventListener::EPluginEvent::Unloaded);
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
