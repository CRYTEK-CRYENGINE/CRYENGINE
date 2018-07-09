// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: CTPEndpoint.cpp
//  Description: a thread that sends keep-alive notifications to registered sockets in the event that the global lock is held for a very long time
//
//	History:
//	-August 24,2007:Created by Craig Tiller
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include <CryNetwork/NetAddress.h>
#include <CryNetwork/CrySocks.h>

extern volatile uint32 g_watchdogTimerGlobalLockCount;
extern volatile uint32 g_watchdogTimerLockedCounter;

class CAutoUpdateWatchdogCounters
{
public:
	ILINE CAutoUpdateWatchdogCounters()
	{
		++g_watchdogTimerGlobalLockCount;
		++g_watchdogTimerLockedCounter;
	}
	ILINE ~CAutoUpdateWatchdogCounters()
	{
		--g_watchdogTimerLockedCounter;
	}
};

namespace detail
{
class CTimerThread;
}

class CWatchdogTimer
{
public:
	CWatchdogTimer();
	~CWatchdogTimer();

	void RegisterTarget(CRYSOCKET sock, const TNetAddress& addr);
	void UnregisterTarget(CRYSOCKET sock, const TNetAddress& addr);

	bool HasStalled() const;
	void ClearStalls();

private:
	std::auto_ptr<detail::CTimerThread> m_pThread;
};
