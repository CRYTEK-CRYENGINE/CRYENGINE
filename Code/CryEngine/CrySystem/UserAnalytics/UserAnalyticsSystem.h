// Copyright 2001 - 2016 Crytek GmbH / Crytek Group.All rights reserved.

#pragma once

#include <CrySystem/UserAnalytics/IUserAnalytics.h>

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#include <CryExtension/ICryPluginManager.h>

struct ICryUserAnalyticsPlugin;

class CUserAnalyticsSystem : public IUserAnalyticsSystem, public IPluginEventListener
{
public:
	CUserAnalyticsSystem();
	~CUserAnalyticsSystem();

	virtual void TriggerEvent(const char* szEventName, UserAnalytics::Attributes* attributes) override;

	void         Initialize();
	void         RegisterCVars();

private:
	virtual void OnPluginEvent(const CryClassID& pluginClassId, IPluginEventListener::EPluginEvent event) override;

	ICryUserAnalyticsPlugin* m_pUserAnalyticsPlugin;
	IUserAnalytics*          m_pUserAnalytics;

	static int               m_enableUserAnalyticsCollect;
	static int               m_enableUserAnalyticsLogging;
};
#else
class CUserAnalyticsSystem : public IUserAnalyticsSystem
{
public:
	CUserAnalyticsSystem() {};
	~CUserAnalyticsSystem() {};

	virtual void TriggerEvent(const char* szEventName, UserAnalytics::Attributes* attributes) override {}

	void         Initialize()                                                                          {}
	void         RegisterCVars()                                                                       {}
};
#endif
