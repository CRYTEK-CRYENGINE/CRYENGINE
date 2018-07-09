// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERVICEMANAGER_H__
#define __SERVICEMANAGER_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include <CryNetwork/INetworkService.h>
#include "NetTimer.h"

class CServiceManager
{
public:
	CServiceManager();
	~CServiceManager();

	INetworkServicePtr GetService(const string& name);

	void               CreateExtension(bool server, IContextViewExtensionAdder* adder);

	void               GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CServiceManager");

		pSizer->Add(*this);
		pSizer->AddContainer(m_services);
		for (TServices::const_iterator it = m_services.begin(); it != m_services.end(); ++it)
		{
			pSizer->AddString(it->first);
			it->second->GetMemoryStatistics(pSizer);
		}
	}

private:
	NetTimerId m_timer;
	typedef std::map<string, INetworkServicePtr> TServices;
	TServices  m_services;

	INetworkServicePtr CreateService(const string& name);
	static void        TimerCallback(NetTimerId id, void* pUser, CTimeValue tm);
};

#endif
