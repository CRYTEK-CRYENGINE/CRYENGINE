// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERVERPROFILER_H__
#define __SERVERPROFILER_H__

#pragma once

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT

	#include "Network.h"

class CServerProfiler
{
public:
	static void Init();
	static void Cleanup();

	static bool ShouldSaveAndCrash() { return m_saveAndCrash; }

private:
	CServerProfiler();
	~CServerProfiler();

	static CServerProfiler* m_pThis;
	FILE*                   m_fout;
	NetTimerId              m_timer;

	FILETIME                m_lastKernel, m_lastUser, m_lastTime;
	uint64                  m_lastIn, m_lastOut;

	static bool             m_saveAndCrash;

	static void             TimerCallback(NetTimerId, void*, CTimeValue);
	void Tick();
};

#else // CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT

class CServerProfiler
{
public:
	static void Init()               {}
	static void Cleanup()            {}
	static bool ShouldSaveAndCrash() { return false; }
};

#endif // CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT

#endif
