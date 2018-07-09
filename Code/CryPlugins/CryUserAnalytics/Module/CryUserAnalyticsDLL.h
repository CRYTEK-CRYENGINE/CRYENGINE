// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/UserAnalytics/ICryUserAnalyticsPlugin.h>

class CUserAnalytics;

class CPlugin_CryUserAnalytics : public ICryUserAnalyticsPlugin
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ICryUserAnalyticsPlugin)
	CRYINTERFACE_ADD(Cry::IEnginePlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin_CryUserAnalytics, "Plugin_CryUserAnalytics", "2284d2bf-677c-4e72-8ace-10f924bdd068"_cry_guid)

	CPlugin_CryUserAnalytics();
	virtual ~CPlugin_CryUserAnalytics();

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

public:
	virtual IUserAnalytics* GetIUserAnalytics() const override;

private:
	CUserAnalytics* m_pUserAnalytics;
};
