// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ServerProfiler.h"

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT

extern uint64 g_bytesIn, g_bytesOut;
extern int g_nChannels;

bool CServerProfiler::m_saveAndCrash = false;

static float ftdiff(const FILETIME& b, const FILETIME& a)
{
	uint64 aa = *reinterpret_cast<const uint64*>(&a);
	uint64 bb = *reinterpret_cast<const uint64*>(&b);
	return (bb - aa) * 1e-7f;
}

CServerProfiler* CServerProfiler::m_pThis;

static const float TICK = 5.0f;

void CServerProfiler::Init()
{
	NET_ASSERT(m_pThis == 0);
	m_pThis = new CServerProfiler;
	if (!m_pThis->m_fout)
		Cleanup();
}

void CServerProfiler::Cleanup()
{
	NET_ASSERT(m_pThis);
	SAFE_DELETE(m_pThis);
}

CServerProfiler::CServerProfiler()
{
	SCOPED_GLOBAL_LOCK;

	m_timer = 0;

	m_timer = TIMER.ADDTIMER(g_time + TICK, TimerCallback, this, "CServerProfiler::CServerProfiler() m_timer");
	string filename = PathUtil::Make(gEnv->pSystem->GetRootFolder(), "server_profile.txt");
	if (m_fout = fxopen(filename.c_str(), "wt"))
		fprintf(m_fout, "cpu\tmemory\tcommitted\tbwin\tbwout\tnchan\n");

	GetSystemTimeAsFileTime(&m_lastTime);
	FILETIME notNeeded;
	GetProcessTimes(GetCurrentProcess(), &notNeeded, &notNeeded, &m_lastKernel, &m_lastUser);
	m_lastIn = g_bytesIn;
	m_lastOut = g_bytesOut;
}

CServerProfiler::~CServerProfiler()
{
	SCOPED_GLOBAL_LOCK;
	TIMER.CancelTimer(m_timer);
	fclose(m_fout);
}

void CServerProfiler::Tick()
{
	FILETIME kernel, user, cur;
	FILETIME notNeeded;
	GetSystemTimeAsFileTime(&cur);
	GetProcessTimes(GetCurrentProcess(), &notNeeded, &notNeeded, &kernel, &user);

	float sKernel = ftdiff(kernel, m_lastKernel);
	float sUser = ftdiff(user, m_lastUser);
	float sCur = ftdiff(cur, m_lastTime);

	float cpu = 100 * (sKernel + sUser) / sCur;

	IMemoryManager::SProcessMemInfo memcnt;
	GetISystem()->GetIMemoryManager()->GetProcessMemInfo(memcnt);

	float megs = memcnt.WorkingSetSize / 1024.0f / 1024.0f;
	float commitMegs = memcnt.PagefileUsage / 1024.0f / 1024.0f;

	float in = (g_bytesIn - m_lastIn) / sCur / 1024.0f;
	float out = (g_bytesOut - m_lastOut) / sCur / 1024.0f;

	fprintf(m_fout, "%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\n", cpu, megs, commitMegs, in, out, g_nChannels);
	fflush(m_fout);

	if (CNetCVars::Get().MaxMemoryUsage && commitMegs > CNetCVars::Get().MaxMemoryUsage)
	{
		m_saveAndCrash = true;
	}

	m_lastTime = cur;
	m_lastKernel = kernel;
	m_lastUser = user;
	m_lastIn = g_bytesIn;
	m_lastOut = g_bytesOut;
}

void CServerProfiler::TimerCallback(NetTimerId, void*, CTimeValue)
{
	m_pThis->Tick();
	m_pThis->m_timer = TIMER.ADDTIMER(g_time + TICK, TimerCallback, m_pThis, "CServerProfiler::TimerCallback() m_timer");
}

#endif
