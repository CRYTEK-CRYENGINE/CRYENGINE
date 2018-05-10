// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/Project/ProjectDefines.h>
#if defined(MAP_LOADING_SLICING)

	#include "HandlerBase.h"
	#include <CryCore/Platform/CryWindows.h>

const char* SERVER_LOCK_NAME = "SynchronizeGameServer";
const char* CLIENT_LOCK_NAME = "SynchronizeGameClient";

HandlerBase::HandlerBase(const char* bucket, int affinity)
{
	m_serverLockName.Format("%s_%s", SERVER_LOCK_NAME, bucket);
	m_clientLockName.Format("%s_%s", CLIENT_LOCK_NAME, bucket);
	if (affinity != 0)
	{
		m_affinity = uint32(1) << (affinity - 1);
	}
	else
	{
		m_affinity = -1;
	}
	m_prevAffinity = 0;
}

HandlerBase::~HandlerBase()
{
	if (m_prevAffinity)
	{
		if (SyncSetAffinity(m_prevAffinity))
			CryLogAlways("Restored affinity to %d", m_prevAffinity);
		else
			CryLogAlways("Failed to restore affinity to %d", m_prevAffinity);
	}
}

void HandlerBase::SetAffinity()
{
	if (m_prevAffinity) //already set
		return;
	if (uint32 p = SyncSetAffinity(m_affinity))
	{
		CryLogAlways("Changed affinity to %d", m_affinity);
		m_prevAffinity = p;
	}
	else
		CryLogAlways("Failed to change affinity to %d", m_affinity);
}

	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID

uint32 HandlerBase::SyncSetAffinity(uint32 cpuMask)//put -1
{
#if  CRY_PLATFORM_ANDROID
	return 0;
#else
	if (cpuMask != 0)
	{
		cpu_set_t cpuSet;
		uint32 affinity = 0;
		if (!sched_getaffinity(getpid(), sizeof cpuSet, &cpuSet))
		{
			for (int cpu = 0; cpu < sizeof(cpuMask) * 8; ++cpu)
			{
				if (CPU_ISSET(cpu, &cpuSet))
					affinity |= 1 << cpu;
			}
		}
		if (affinity)
		{
			CPU_ZERO(&cpuSet);
			for (int cpu = 0; cpu < sizeof(cpuMask) * 8; ++cpu)
			{
				if (cpuMask & (1 << cpu))
				{
					CPU_SET(cpu, &cpuSet);
				}
			}

			if (!sched_setaffinity(getpid(), sizeof(cpuSet), &cpuSet))
				return affinity;
		}
	}
	return 0;
#endif
}

	#elif CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO

uint32 HandlerBase::SyncSetAffinity(uint32 cpuMask)//put -1
{
	uint32 p = (uint32)SetThreadAffinityMask(GetCurrentThread(), cpuMask);
	if (p == 0)
		CryLogAlways("Error updating affinity mask to %d", cpuMask);
	return p;
}

	#else

uint32 HandlerBase::SyncSetAffinity(uint32 cpuMask)//put -1
{
	CryLogAlways("Updating thread affinity not supported on this platform");
	return 0;
}

	#endif

#endif // defined(MAP_LOADING_SLICING)
