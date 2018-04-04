// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryExtension/ICryPluginManager.h>
#include <array>

struct SPluginContainer;

class CCryPluginManager final 
	: public ICryPluginManager
	, public ISystemEventListener
{
public:
	CCryPluginManager(const SSystemInitParams& initParams);
	virtual ~CCryPluginManager();

	virtual void RegisterEventListener(const CryClassID& pluginClassId, IPluginEventListener* pListener) override
	{
		// we have to simply add this listener now because the plugin can be loaded at any time
		// this should change in release builds since all the necessary plugins will be loaded upfront
		m_pluginListenerMap[pListener].push_back(pluginClassId);
	}

	virtual void RemoveEventListener(const CryClassID& pluginClassId, IPluginEventListener* pListener) override
	{
		auto it = m_pluginListenerMap.find(pListener);
		if (it != m_pluginListenerMap.end())
		{
			stl::find_and_erase(it->second, pluginClassId);
		}
	}

	// Called by CrySystem during early init to initialize the manager and load plugins
	// Plugins that require later activation can do so by listening to system events such as ESYSTEM_EVENT_PRE_RENDERER_INIT
	void LoadProjectPlugins();

	void Update(IPluginUpdateListener::EPluginUpdateType updateFlags);

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

	// Gets the default plug-ins that are always loaded into fresh projects
	// These are also built with the engine, thus will be statically linked in for monolithic builds.
	static constexpr std::array<const char*, 3> GetDefaultPlugins() { return{ { "CryDefaultEntities", "CrySensorSystem", "CryPerceptionSystem" } }; }

protected:
	virtual bool                        LoadPluginFromDisk(EPluginType type, const char* path) override;

	virtual std::shared_ptr<ICryPlugin> QueryPluginById(const CryClassID& classID) const override;
	virtual std::shared_ptr<ICryPlugin> AcquirePluginById(const CryClassID& classID) override;

	bool OnPluginLoaded();

private:
	bool                    UnloadAllPlugins();
	void                    NotifyEventListeners(const CryClassID& classID, IPluginEventListener::EPluginEvent event);

	std::vector<SPluginContainer> m_pluginContainer;
	std::map<IPluginEventListener*, std::vector<CryClassID>> m_pluginListenerMap;

	const SSystemInitParams m_systemInitParams;
	bool                    m_bLoadedProjectPlugins;
};
