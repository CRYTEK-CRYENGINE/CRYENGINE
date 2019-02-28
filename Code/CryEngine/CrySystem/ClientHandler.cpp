// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/Project/ProjectDefines.h>
#if defined(MAP_LOADING_SLICING)

	#include "ClientHandler.h"

ClientHandler::ClientHandler(const char* bucket, int affinity, int clientTimeout)
	: HandlerBase(bucket, affinity)
{
	m_clientTimeout = clientTimeout;
	Reset();
}

void ClientHandler::Reset()
{
	m_srvLock.reset(0);
	for (int i = 0; i < MAX_CLIENTS_NUM; i++)
	{
		std::unique_ptr<SSyncLock> srv(new SSyncLock(m_serverLockName, i, false));
		//fist get the client lock up!
		//m_clientLock.reset(new SSyncLock(CLIENT_LOCK_NAME, 0, MAX_CLIENTS_NUM));
		if (!srv->IsValid())
		{
			//try to create client lock
			m_clientLock.reset(new SSyncLock(m_clientLockName, i, true));
			if (m_clientLock->IsValid())
				break;
			else
				m_clientLock.reset(0);
		}
	}
}

bool ClientHandler::ServerIsValid()
{
	if (!m_srvLock.get())
	{
		if (m_clientLock.get() && m_clientLock->IsValid())
		{
			m_srvLock.reset(new SSyncLock(m_serverLockName, m_clientLock->number, false));
			if (m_srvLock->IsValid())
			{
				SetAffinity();
				//got synched
				return true;
			}
			m_srvLock.reset(0);
		}
		return false;
	}
	return m_srvLock->IsValid();
}

bool ClientHandler::Sync()
{
	if (ServerIsValid())
	{
		m_clientLock->Signal();//signal that we're done and
		if (m_srvLock->Wait(m_clientTimeout))//wait for server
		{
			//bla bla, track waiting
			return true;
		}
		else
		{
			Reset();
		}
	}
	return false;
}

#endif // defined(MAP_LOADING_SLICING)
