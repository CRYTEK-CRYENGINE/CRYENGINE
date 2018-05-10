// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "Watchdog.h"
#include <CryThreading/IThreadManager.h>

void CWatchdogThread::SignalStopWork()
{
	m_bQuit = true;
}

CWatchdogThread::~CWatchdogThread()
{
	SignalStopWork();
}

void CWatchdogThread::SetTimeout(int timeOutSeconds)
{
	CRY_ASSERT(timeOutSeconds > 0);
	m_timeOutSeconds = timeOutSeconds;
}

CWatchdogThread::CWatchdogThread(int timeOutSeconds)
	: m_timeOutSeconds(timeOutSeconds)
{
	CRY_ASSERT(timeOutSeconds > 0);
	if (!gEnv->pThreadManager->SpawnThread(this, "Watch Dog"))
	{
		CRY_ASSERT_MESSAGE(false, "Error spawning \"Watch Dog\" thread.");
	}
}

uint64 CWatchdogThread::GetSystemUpdateCounter()
{
	CRY_ASSERT(GetISystem());
	return GetISystem()->GetUpdateCounter();
}

void CWatchdogThread::ThreadEntry()
{
	uint64 prevUpdateCounter = GetSystemUpdateCounter();
	Sleep();
	while (!m_bQuit)
	{
		auto curUpdateCounter = GetSystemUpdateCounter();
		if (prevUpdateCounter != curUpdateCounter)
		{
			prevUpdateCounter = curUpdateCounter;
		}
		else
		{
			CryFatalError("Freeze detected. Watchdog timeout.");
		}
		Sleep();
	}
}

void CWatchdogThread::Sleep() const
{
	CRY_ASSERT(m_timeOutSeconds > 0);
	CrySleep(1000 * m_timeOutSeconds);
}
