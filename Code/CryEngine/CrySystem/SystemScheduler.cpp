// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SystemScheduler.cpp
//  Version:     v1.00
//  Created:     18:11:2010 by Sergey Rustamov.
//  Description: Implementation of the CSystemScheduler class
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#include <CryCore/Project/ProjectDefines.h>

#if defined(MAP_LOADING_SLICING)

	#include "SystemScheduler.h"

	#include <CryCore/Containers/MiniQueue.h>
	#include "ClientHandler.h"
	#include "ServerHandler.h"
	#include <CrySystem/Profilers/IStatoscope.h>

extern "C" void SliceAndSleep(const char* pFunc, int line)
{
	GetISystemScheduler()->SliceAndSleep(pFunc, line);
}

void CreateSystemScheduler(CSystem* pSystem)
{
	gEnv->pSystemScheduler = new CSystemScheduler(pSystem);
}

CSystemScheduler::CSystemScheduler(CSystem* pSystem)
	: m_pSystem(pSystem)
	, m_lastSliceCheckTime(0)
	, m_sliceLoadingRef(0)
{
	int defaultSchedulingMode = 0;
	#if defined(DEDICATED_SERVER)
	defaultSchedulingMode = 2;
	#endif

	m_svSchedulingMode = REGISTER_INT("sv_scheduling", defaultSchedulingMode, 0, "Scheduling mode\n"
	                                                                             " 0: Normal mode\n"
	                                                                             " 1: Client\n"
	                                                                             " 2: Server\n");

	m_svSchedulingBucket = REGISTER_INT("sv_schedulingBucket", 0, 0, "Scheduling bucket\n");

	m_svSchedulingAffinity = REGISTER_INT("sv_SchedulingAffinity", 0, 0, "Scheduling affinity\n");

	m_svSchedulingClientTimeout = REGISTER_TIME("sv_schedulingClientTimeout", CTimeValue(1), 0, "Client wait server\n");
	m_svSchedulingServerTimeout = REGISTER_TIME("sv_schedulingServerTimeout", CTimeValue("0.1"), 0, "Server wait server\n");

	#if defined(MAP_LOADING_SLICING)
	m_svSliceLoadEnable = REGISTER_INT("sv_sliceLoadEnable", 1, 0, "Enable/disable slice loading logic\n");
	;
	m_svSliceLoadBudget = REGISTER_TIME("sv_sliceLoadBudget", CTimeValue().SetMilliSeconds(10), 0, "Slice budget\n");
	;
	m_svSliceLoadLogging = REGISTER_INT("sv_sliceLoadLogging", 0, 0, "Enable/disable slice loading logging\n");
	#endif

	m_pLastSliceName = "INACTIVE";
	m_lastSliceLine = 0;
}

CSystemScheduler::~CSystemScheduler(void)
{
}

void CSystemScheduler::SliceLoadingBegin()
{
	m_lastSliceCheckTime = GetGTimer()->GetAsyncTime();
	m_sliceLoadingRef++;
	m_pLastSliceName = "START";
	m_lastSliceLine = 0;
}

void CSystemScheduler::SliceLoadingEnd()
{
	m_sliceLoadingRef--;
	m_pLastSliceName = "INACTIVE";
	m_lastSliceLine = 0;
}

void CSystemScheduler::SliceAndSleep(const char* sliceName, int line)
{
	#if defined(MAP_LOADING_SLICING)
	if (!gEnv->IsDedicated())
		return;

	if (!m_sliceLoadingRef)
		return;

	if (!m_svSliceLoadEnable->GetIVal())
		return;

	SchedulingModeUpdate();

	CTimeValue currTime = GetGTimer()->GetAsyncTime();

	CTimeValue sliceBudget = CLAMP(m_svSliceLoadBudget->GetTime(), 0, CTimeValue(1) / m_pSystem->GetDedicatedMaxRate()->GetMPVal());
	bool doSleep = true;
	if ((currTime - m_pSystem->GetLastTickTime()) < sliceBudget)
	{
		m_lastSliceCheckTime = currTime;
		doSleep = false;
	}

	if (doSleep)
	{
		if (m_svSliceLoadLogging->GetIVal())
		{
			CTimeValue diff = (currTime - m_lastSliceCheckTime);
			if (diff > sliceBudget)
				CryLogAlways("[SliceAndSleep]: Interval between slice [%s:%i] and [%s:%i] was [%f] out of budget [%f]", m_pLastSliceName, m_lastSliceLine, sliceName, line, (float)diff.GetMilliSeconds(), (float)sliceBudget.GetMilliSeconds());
		}

		gEnv->pFrameProfileSystem->OnSliceAndSleep();
		gEnv->pStatoscope->Tick();

		m_pSystem->SleepIfNeeded();
	}

	m_pLastSliceName = sliceName;
	m_lastSliceLine = line;
	#endif
}

void CSystemScheduler::SchedulingSleepIfNeeded()
{
	if (!gEnv->IsDedicated())
		return;

	SchedulingModeUpdate();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Scheduling mode routines
//
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Scheduling mode update logic
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void CSystemScheduler::SchedulingModeUpdate()
{
	static std::unique_ptr<ClientHandler> m_client;
	static std::unique_ptr<ServerHandler> m_server;

	if (int scheduling = m_svSchedulingMode->GetIVal())
	{
		if (scheduling == 1) //client
		{
			if (!m_client.get())
			{
				m_server.reset();
				m_client.reset(new ClientHandler(m_svSchedulingBucket->GetString(), m_svSchedulingAffinity->GetIVal(), m_svSchedulingClientTimeout->GetTime()));
			}
			if (m_client->Sync())
				return;
		}
		else if (scheduling == 2) //server
		{
			if (!m_server.get())
			{
				m_client.reset();
				m_server.reset(new ServerHandler(m_svSchedulingBucket->GetString(), m_svSchedulingAffinity->GetIVal(), m_svSchedulingServerTimeout->GetTime()));
			}
			if (m_server->Sync())
				return;
		}
	}
	else
	{
		m_client.reset();
		m_server.reset();
	}
}

#endif // defined(MAP_LOADING_SLICING)
