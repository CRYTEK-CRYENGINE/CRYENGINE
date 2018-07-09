// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ProjectManager/ProjectManager.h"

#include <CrySystem/ICryPluginManager.h>
#include <CrySystem/ICryPlugin.h>
#include <array>

#include <CryCore/Platform/CryLibrary.h>

struct SPluginContainer;

class CCryPluginManager final 
	: public Cry::IPluginManager
	, public ISystemEventListener
{
public:
	using TPluginListenerPair = std::pair<IEventListener*, std::vector<CryClassID>>;

	CCryPluginManager(const SSystemInitParams& initParams);
	virtual ~CCryPluginManager();

	// Cry::IPluginManager
	virtual void RegisterEventListener(const CryClassID& pluginClassId, IEventListener* pListener) override
	{
		// we have to simply add this listener now because the plugin can be loaded at any time
		// this should change in release builds since all the necessary plugins will be loaded upfront
		m_pluginListenerMap[pListener].push_back(pluginClassId);
	}

	virtual void RemoveEventListener(const CryClassID& pluginClassId, IEventListener* pListener) override
	{
		auto it = m_pluginListenerMap.find(pListener);
		if (it != m_pluginListenerMap.end())
		{
			stl::find_and_erase(it->second, pluginClassId);
		}
	}

	virtual std::shared_ptr<Cry::IEnginePlugin> QueryPluginById(const CryClassID& classID) const override;
	virtual void OnPluginUpdateFlagsChanged(Cry::IEnginePlugin& plugin, uint8 newFlags, uint8 changedStep) override;
	// ~Cry::IPluginManager

	// Called by CrySystem during early init to initialize the manager and load plugins
	// Plugins that require later activation can do so by listening to system events such as ESYSTEM_EVENT_PRE_RENDERER_INIT
	void LoadProjectPlugins();

	void UpdateBeforeSystem() override;
	void UpdateBeforePhysics() override;
	void UpdateAfterSystem() override;
	void UpdateBeforeFinalizeCamera() override;
	void UpdateBeforeRender() override;
	void UpdateAfterRender() override;
	void UpdateAfterRenderSubmit() override;

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

	using TDefaultPluginPair = std::pair<uint8 /* version with which the plug-in was made default */, SPluginDefinition>;
	// Gets the plug-ins, along with the version that they were made default in
	// This is called in order to update the default plug-ins for projects on upgrade
	// These are also built with the engine, thus will be statically linked in for monolithic builds.
	static std::array<TDefaultPluginPair, 4> GetDefaultPlugins()
	{
		return 
		{
			{ 
				// Plug-ins made default with version 1
				{ 1, SPluginDefinition { EType::Native, "CryDefaultEntities" } },
				{ 1, SPluginDefinition { EType::Native, "CrySensorSystem" } },
				{ 1, SPluginDefinition { EType::Native, "CryPerceptionSystem" } },
				// Plug-ins made default with version 3
				{ 3, SPluginDefinition{ EType::Native, "CryGamePlatform",{ EPlatform::PS4 } } }
			}
		};
	}

protected:
#if CrySharedLibrarySupported
	bool LoadPluginFromDisk(EType type, const char* path, bool notifyUserOnFailure = true);
#endif

	bool OnPluginLoaded(bool notifyUserOnFailure = true);
	void OnPluginUnloaded(Cry::IEnginePlugin* pPlugin);

	std::vector<Cry::IEnginePlugin*>& GetUpdatedPluginsForStep(Cry::IEnginePlugin::EUpdateStep step) { return m_updatedPlugins[IntegerLog2(static_cast<uint8>(step))]; }

private:
	bool                    UnloadAllPlugins();
	void                    NotifyEventListeners(const CryClassID& classID, IEventListener::EEvent event);

	std::vector<SPluginContainer> m_pluginContainer;
	std::map<IEventListener*, std::vector<CryClassID>> m_pluginListenerMap;

	const SSystemInitParams m_systemInitParams;
	bool                    m_bLoadedProjectPlugins;

	std::array<std::vector<Cry::IEnginePlugin*>, static_cast<size_t>(Cry::IEnginePlugin::EUpdateStep::Count)> m_updatedPlugins;
};
