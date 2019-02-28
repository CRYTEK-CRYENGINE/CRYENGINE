// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{


ILINE void CParticleProfiler::AddEntry(const CParticleComponentRuntime& runtime, EProfileStat type, uint value)
{
#if !defined(_RELEASE)
	if (!IsEnabled())
		return;
	const uint32 threadId = JobManager::GetWorkerThreadId();
	SEntry entry { &runtime, type, value };
	m_entries[threadId + 1].push_back(entry);
#endif
}

ILINE CTimeProfiler::CTimeProfiler(CParticleProfiler& profiler, const CParticleComponentRuntime& runtime, EProfileStat stat)
	: m_profiler(profiler)
	, m_runtime(runtime)
#if !defined(_RELEASE)
	, m_stat(stat)
	, m_startTicks(CryGetTicks())
#endif
{
}

ILINE CTimeProfiler::~CTimeProfiler()
{
#if !defined(_RELEASE)
	const int64 endTicks = CryGetTicks();
	const uint time = uint(gEnv->pTimer->TicksToSeconds(endTicks - m_startTicks) * 1000000.0f);
	m_profiler.AddEntry(m_runtime, m_stat, time);
	m_profiler.AddEntry(m_runtime, EPS_TotalTiming, time);
#endif
}
}
