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

void CWatchdogThread::SetTimeout(const CTimeValue& timeOut)
{
	CRY_ASSERT(timeOut > 0);
	m_timeOut = timeOut;
}

CWatchdogThread::CWatchdogThread(const CTimeValue& timeOut)
	: m_timeOut(timeOut)
{
	CRY_ASSERT(timeOut > 0);
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
	CRY_ASSERT(m_timeOut > 0);
	CryLowLatencySleep(m_timeOut);
}
