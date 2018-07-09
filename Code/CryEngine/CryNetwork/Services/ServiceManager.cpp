// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ServiceManager.h"
#include "Network.h"
#include "Config.h"

#include "NetworkACL/NetProfileTokens.h"

static const float POLL_TIME = 10.0f;

CServiceManager::CServiceManager()
{
	SCOPED_GLOBAL_LOCK;
	m_timer = TIMER.ADDTIMER(g_time + POLL_TIME, TimerCallback, this, "CServiceManager::CServiceManager() m_timer");
}

CServiceManager::~CServiceManager()
{
	SCOPED_GLOBAL_LOCK;
	TIMER.CancelTimer(m_timer);
	for (TServices::iterator it = m_services.begin(), eit = m_services.end(); it != eit; ++it)
	{
		it->second->Close();
	}
}

INetworkServicePtr CServiceManager::GetService(const string& name)
{
	TServices::iterator iter = m_services.find(name);
	if (iter == m_services.end() || iter->second->GetState() == eNSS_Closed)
	{
		INetworkServicePtr pService = CreateService(name);
		if (pService)
			m_services[name] = pService;
		return pService;
	}
	return iter->second;
}

INetworkServicePtr CServiceManager::CreateService(const string& name)
{
	if (name == "NetProfileTokens")
		return new CNetProfileTokens();
	return NULL;
}

void CServiceManager::CreateExtension(bool server, IContextViewExtensionAdder* adder)
{
	for (TServices::iterator iter = m_services.begin(); iter != m_services.end(); ++iter)
	{
		iter->second->CreateContextViewExtensions(server, adder);
	}
}

void CServiceManager::TimerCallback(NetTimerId id, void* pUser, CTimeValue tm)
{
	CServiceManager* pThis = static_cast<CServiceManager*>(pUser);

	for (TServices::iterator iter = pThis->m_services.begin(); iter != pThis->m_services.end(); )
	{
		TServices::iterator next = iter;
		++next;
		if (iter->second->GetState() == eNSS_Closed)
			pThis->m_services.erase(iter);
		iter = next;
	}

	SCOPED_GLOBAL_LOCK;
	pThis->m_timer = TIMER.ADDTIMER(g_time + POLL_TIME, TimerCallback, pThis, "CServiceManager::TimerCallback() m_timer");
}
