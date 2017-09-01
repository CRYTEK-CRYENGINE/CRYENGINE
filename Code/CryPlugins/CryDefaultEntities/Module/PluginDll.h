// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IDefaultComponentsPlugin.h"

class CPlugin_CryDefaultEntities final : public IPlugin_CryDefaultEntities
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IPlugin_CryDefaultEntities)
	CRYINTERFACE_ADD(ICryPlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin_CryDefaultEntities, "Plugin_CryDefaultEntities", "{CB9E7C85-3289-41B6-983A-6A076ABA6351}"_cry_guid)

	PLUGIN_FLOWNODE_REGISTER
	PLUGIN_FLOWNODE_UNREGISTER

	virtual ~CPlugin_CryDefaultEntities();

	void RegisterComponents(Schematyc::IEnvRegistrar& registrar);

public:
	// ICryPlugin
	virtual const char* GetName() const override { return "CryDefaultEntities"; }
	virtual const char* GetCategory() const override { return "Default"; }
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}
	// ~ICryPlugin

	virtual ICameraManager* GetICameraManager() override
	{
		return m_pCameraManager.get();
	}

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

private:
	std::unique_ptr<ICameraManager> m_pCameraManager;
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

public:
	IEntityRegistrator *m_pNext;

	static IEntityRegistrator *g_pFirst;
	static IEntityRegistrator *g_pLast;
};