// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

#include "UserAnalyticsSystem.h"

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS

	#include <CrySystem/UserAnalytics/IUserAnalytics.h>
	#include <CrySystem/UserAnalytics/ICryUserAnalyticsPlugin.h>
	#include "ExtensionSystem/CryPluginManager.h"

int CUserAnalyticsSystem::m_enableUserAnalyticsCollect = 0;
int CUserAnalyticsSystem::m_enableUserAnalyticsLogging = 0;

///////////////////////////////////////////////////////////////////////////
CUserAnalyticsSystem::CUserAnalyticsSystem()
	: m_pUserAnalyticsPlugin(nullptr)
	, m_pUserAnalytics(nullptr)
{
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalyticsSystem::RegisterCVars()
{
	REGISTER_CVAR2("sys_UserAnalyticsCollect", &m_enableUserAnalyticsCollect, 1, VF_NULL,
	               "Collect User Analytics Events and push them to CRYENGINE server\n"
	               " 0 - disabled\n"
	               " 1 - enabled, recommended");

	REGISTER_CVAR2("sys_UserAnalyticsLogging", &m_enableUserAnalyticsLogging, 0, VF_NULL,
	               "Enable User Analytics Logging\n"
	               " 0 - disabled\n"
	               " 1 - enabled");
}

///////////////////////////////////////////////////////////////////////////
CUserAnalyticsSystem::~CUserAnalyticsSystem()
{
	// Plugin Manager might be not available anymore
	ICryPluginManager* pPluginManager = GetISystem()->GetIPluginManager();
	if (pPluginManager != nullptr)
	{
		pPluginManager->RemoveEventListener(cryiidof<ICryUserAnalyticsPlugin>(), this);
	}

	if (IConsole* const pConsole = gEnv->pConsole)
	{
		pConsole->UnregisterVariable("sys_UserAnalyticsCollect");
		pConsole->UnregisterVariable("sys_UserAnalyticsLogging");
	}
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalyticsSystem::Initialize()
{
	ICryPluginManager* pPluginManager = GetISystem()->GetIPluginManager();
	if (pPluginManager != nullptr)
	{
		m_pUserAnalyticsPlugin = pPluginManager->QueryPlugin<ICryUserAnalyticsPlugin>();

		if (m_pUserAnalyticsPlugin != nullptr)
		{
			pPluginManager->RegisterEventListener(cryiidof<ICryUserAnalyticsPlugin>(), this);

			m_pUserAnalytics = m_pUserAnalyticsPlugin->GetIUserAnalytics();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalyticsSystem::OnPluginEvent(const CryClassID& pluginClassId, IPluginEventListener::EPluginEvent event)
{
	if (event == IPluginEventListener::EPluginEvent::Unloaded)
	{
		m_pUserAnalyticsPlugin = nullptr;
		m_pUserAnalytics = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////
void CUserAnalyticsSystem::TriggerEvent(const char* szEventName, UserAnalytics::Attributes* attributes)
{
	if (m_enableUserAnalyticsCollect == 0)
	{
		return;
	}

	if (m_pUserAnalytics)
	{
		if (m_enableUserAnalyticsLogging > 0)
		{
			CryLogAlways("[User Analytics] Queuing Event: %s", szEventName);
		}

		m_pUserAnalytics->TriggerEvent(szEventName, attributes);
	}
}
#endif
