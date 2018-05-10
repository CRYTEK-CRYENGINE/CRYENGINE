// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: CTPEndpoint.cpp
//  Description: a thread that sends keep-alive notifications to registered sockets in the event that the global lock is held for a very long time
//
//	History:
//	-August 24,2007:Created by Craig Tiller
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "WatchdogTimer.h"
#include "Network.h"
#include <CryThreading/IThreadManager.h>

volatile uint32 g_watchdogTimerGlobalLockCount = 0;
volatile uint32 g_watchdogTimerLockedCounter = 0;

namespace detail
{
enum EAction
{
	eA_RegisterTarget,
	eA_UnregisterTarget,
};

struct SAction
{
	EAction     action;
	CRYSOCKET   sock;
	TNetAddress addr;
};
}

#define THREAD_NAME "NetworkWatchdog"

class detail::CTimerThread : public IThread
{
public:
	CTimerThread() : m_done(false), m_stallDetected(false) {}

	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		NetFastMutex& commMtx = CNetwork::Get()->GetCommMutex();
		uint32 lastSeenGlobalLockCount = 0;

		bool possiblyStalled = false;

		while (true)
		{
			{
				CryAutoLock<TMutex> lk(m_mutex);
				m_cond.TimedWait(m_mutex, 500);
				if (m_done)
					break;
			}

			CryAutoLock<NetFastMutex> lk(commMtx);

			if (!m_actions.empty())
			{
				for (std::vector<detail::SAction>::const_iterator it = m_actions.begin(); it != m_actions.end(); ++it)
				{
					STarget tgt;
					switch (it->action)
					{
					case detail::eA_RegisterTarget:
						tgt.sock = it->sock;
						tgt.addr = it->addr;
						stl::push_back_unique(m_target, tgt);
						break;
					case detail::eA_UnregisterTarget:
						tgt.sock = it->sock;
						tgt.addr = it->addr;
						stl::find_and_erase(m_target, tgt);
						break;
					}
				}
				m_actions.resize(0);
			}

			if (g_watchdogTimerLockedCounter && lastSeenGlobalLockCount == g_watchdogTimerGlobalLockCount)
			{
				if (possiblyStalled)
				{
					m_stallDetected = true;
					SendKeepAlives();
				}
				else
					possiblyStalled = true;
			}
			else
			{
				possiblyStalled = false;
				lastSeenGlobalLockCount = g_watchdogTimerGlobalLockCount;
			}
		}
	}

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork()
	{
		CryAutoLock<TMutex> lk(m_mutex);
		m_done = true;
		m_cond.Notify();
	}

	void QueueAction(const detail::SAction& action)
	{
		SCOPED_COMM_LOCK;
		m_actions.push_back(action);
	}

	ILINE bool HasStalled() const
	{
		return m_stallDetected;
	}

	ILINE void ClearStalls()
	{
		m_stallDetected = false;
	}

private:
	typedef CryMutex             TMutex;
	typedef CryConditionVariable TCond;

	TMutex                       m_mutex;
	TCond                        m_cond;
	volatile bool                m_done;
	volatile bool                m_stallDetected;

	std::vector<detail::SAction> m_actions;

	struct STarget
	{
		CRYSOCKET   sock;
		TNetAddress addr;

		bool operator==(const STarget& rhs) const
		{
			return sock == rhs.sock && addr == rhs.addr;
		}
	};

	std::vector<STarget> m_target;

	void SendKeepAlives()
	{
		for (std::vector<STarget>::iterator it = m_target.begin(); it != m_target.end(); ++it)
		{
			char addrBuf[_SS_MAXSIZE];
			int addrSize = _SS_MAXSIZE;

			if (ConvertAddr(it->addr, (CRYSOCKADDR*)addrBuf, &addrSize))
				CrySock::sendto(it->sock, (char*)(Frame_IDToHeader + eH_BackOff), 1, 0, (CRYSOCKADDR*)addrBuf, addrSize);
		}
	}
};

CWatchdogTimer::CWatchdogTimer()
{
	m_pThread.reset(new detail::CTimerThread);

	if (!gEnv->pThreadManager->SpawnThread(m_pThread.get(), THREAD_NAME))
	{
		CryFatalError("Error spawning \"%s\" thread.", THREAD_NAME);
	}
}

CWatchdogTimer::~CWatchdogTimer()
{
	m_pThread->SignalStopWork();
	gEnv->pThreadManager->JoinThread(m_pThread.get(), eJM_Join);
	m_pThread.reset();
}

void CWatchdogTimer::RegisterTarget(CRYSOCKET sock, const TNetAddress& addr)
{
	detail::SAction action;
	action.action = detail::eA_RegisterTarget;
	action.sock = sock;
	action.addr = addr;
	m_pThread->QueueAction(action);
}

void CWatchdogTimer::UnregisterTarget(CRYSOCKET sock, const TNetAddress& addr)
{
	detail::SAction action;
	action.action = detail::eA_UnregisterTarget;
	action.sock = sock;
	action.addr = addr;
	m_pThread->QueueAction(action);
}

bool CWatchdogTimer::HasStalled() const
{
	return m_pThread->HasStalled();
}

void CWatchdogTimer::ClearStalls()
{
	m_pThread->ClearStalls();
}
#undef THREAD_NAME
