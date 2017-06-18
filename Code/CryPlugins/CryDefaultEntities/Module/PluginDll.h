// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>

class CPlugin_CryDefaultEntities
	: public ICryPlugin
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ICryPlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS(CPlugin_CryDefaultEntities, "Plugin_CryDefaultEntities", 0x2C51634796014B70, 0xBB74CE14DD711EE6)

	PLUGIN_FLOWNODE_REGISTER
	PLUGIN_FLOWNODE_UNREGISTER

	virtual ~CPlugin_CryDefaultEntities() {}

public:
	CPlugin_CryDefaultEntities();

	//! Retrieve name of plugin.
	virtual const char* GetName() const override { return "CryDefaultEntities"; }

	//! Retrieve category for the plugin.
	virtual const char* GetCategory() const override { return "Default"; }

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}
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