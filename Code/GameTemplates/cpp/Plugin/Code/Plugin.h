#pragma once

#include <CrySystem/ICryPlugin.h>

#include <CryGame/IGameFramework.h>

#include <CryEntitySystem/IEntityClass.h>

class CPlugin 
	: public ICryPlugin
	, public ISystemEventListener
{
public:
	CRYINTERFACE_SIMPLE(ICryPlugin)
	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin, "Plugin_Sample", "2711a23d-3848-4cdd-a95b-e9d88ffa23b0"_cry_guid)

	virtual ~CPlugin();
	
	//! Retrieve name of plugin.
	virtual const char* GetName() const override { return "MyPlugin"; }

	//! Retrieve category for the plugin.
	virtual const char* GetCategory() const override { return "Game"; }

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener
};