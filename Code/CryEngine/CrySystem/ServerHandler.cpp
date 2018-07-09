// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/Project/ProjectDefines.h>
#if defined(MAP_LOADING_SLICING)

	#include "ServerHandler.h"

ServerHandler::ServerHandler(const char* bucket, int affinity, int serverTimeout)
	: HandlerBase(bucket, affinity)
{
	m_serverTimeout = serverTimeout;
	DoScan();
}

void ServerHandler::DoScan()
{
	std::set<int> gotIndices;
	for (int i = 0; i < m_srvLocks.size(); ++i)
	{
		gotIndices.insert(m_srvLocks[i]->number);
	}
	for (int i = 0; i < MAX_CLIENTS_NUM; ++i)
	{
		if (gotIndices.find(i) == gotIndices.end())
		{
			std::unique_ptr<SSyncLock> lock(new SSyncLock(m_clientLockName, i, false));
			if (lock->IsValid())
			{
				std::unique_ptr<SSyncLock> srv(new SSyncLock(m_serverLockName, i, true));
				if (srv->IsValid())
				{
					m_srvLocks.push_back(std::move(srv));
					m_clientLocks.push_back(std::move(lock));
					CryLogAlways("Client %d bound", i);
				}
				else
				{
					CryLogAlways("Failed to bind client %d", i);
				}
			}
		}
	}
	if (!m_clientLocks.empty())
		SetAffinity();
	m_lastScan = gEnv->pTimer->GetAsyncTime();
}

bool ServerHandler::Sync()
{
	if ((gEnv->pTimer->GetAsyncTime() - m_lastScan).GetSeconds() > 1.0f)
	{
		DoScan();
	}
	for (int i = 0; i < m_srvLocks.size(); )
	{
		m_srvLocks[i]->Signal();
		if (!m_clientLocks[i]->Wait(m_serverTimeout))//actually if not waited, let's kill it!
		{
			CryLogAlways("Dropped client %d", m_clientLocks[i]->number);
			m_clientLocks[i]->Own(m_clientLockName);
			m_clientLocks.erase(m_clientLocks.begin() + i);
			m_srvLocks.erase(m_srvLocks.begin() + i);
			continue;
		}
		++i;
	}
	return false;//!m_clientLocks.empty();
}

#endif // defined(MAP_LOADING_SLICING)
