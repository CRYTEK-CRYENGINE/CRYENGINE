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

struct IEntityRegistrator
{
	IEntityRegistrator()
	{
		if (g_pFirst == nullptr)
		{
			g_pFirst = this;
			g_pLast = this;
		}
		else
		{
			g_pLast->m_pNext = this;
			g_pLast = g_pLast->m_pNext;
		}
	}

	virtual void Register() = 0;
	virtual void Unregister() = 0;

public:
	IEntityRegistrator *m_pNext;

	static IEntityRegistrator *g_pFirst;
	static IEntityRegistrator *g_pLast;
};