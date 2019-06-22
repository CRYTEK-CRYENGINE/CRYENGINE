// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryProfilingSystem.h"
#include "BootProfiler.h"
#include <CrySystem/ConsoleRegistration.h>
#include <CryThreading/IThreadManager.h>

#if CRY_PLATFORM_WINDOWS
#	include <psapi.h>
#endif

// CryMemoryManager.cpp
extern int64 CryMemoryGetThreadAllocatedSize();

// debugging of profiler
#if 0
#define CRY_PROFILER_ASSERT(...) if(!(__VA_ARGS__)) __debugbreak(); //assert(__VA_ARGS__)
#else
#define CRY_PROFILER_ASSERT(...)
#endif

// measuring the overhead also increases the overhead (by ~0.5ms main thread time)
#define CRY_PROFILER_MEASURE_OVERHEAD 1

#define PROF_CVAR_THREAD "profile_exclusiveThreadId"
threadID CCryProfilingSystem::s_exclusiveThread = 0;
#define PROF_CVAR_VERBOSITY "profile_verbosity"
int CCryProfilingSystem::s_verbosity = 10;
#define PROF_CVAR_SUBSYSTEM "profile_subsystemMask"
int64 CCryProfilingSystem::s_subsystemFilterMask = 0;
float CCryProfilingSystem::profile_peak_tolerance = 0;
int CCryProfilingSystem::profile_value = TIME;

#define CRY_PROFILER_MAX_DEPTH 64
// tracks nesting level of profiling sections
static thread_local int tls_profilingStackDepth = 0;
static thread_local SProfilingSection* tls_currentSections[CRY_PROFILER_MAX_DEPTH];

//! RAII struct for measuring profiling overhead
struct CCryProfilingSystem::AutoTimer
{
	AutoTimer(CCryProfilingSystem* _pProfSystem) :
		timeStamp(CryGetTicks())
#if CRY_PROFILER_MEASURE_OVERHEAD
		, overheadAccumulator(&_pProfSystem->m_profilingOverhead.m_accumulator)
#endif
	{}

#if CRY_PROFILER_MEASURE_OVERHEAD
	~AutoTimer()
	{
		CryInterlockedAdd(overheadAccumulator, CryGetTicks() - timeStamp);
	}
#endif

	const int64 timeStamp;

#if CRY_PROFILER_MEASURE_OVERHEAD
private:
	int64* overheadAccumulator;
#endif
};

static int64 GetFrequency_ProfilerInternal()
{
	LARGE_INTEGER freq;
	if (QueryPerformanceFrequency(&freq))
		return freq.QuadPart;

	CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Couldn't determine performance counter frequency! Profiling times will likely be wrong.");
	return 1000;
}

CCryProfilingSystem::CCryProfilingSystem()
	: m_enabled(true)
	, m_trackingPaused(false)
	, m_willPause(false)
	, m_lastTotalPageFaults(0)
	, m_MsPerTick(1000.0f / GetFrequency_ProfilerInternal())
	, m_pBootProfiler(nullptr)
{
	assert(s_pInstance == nullptr);
	s_pInstance = this;
}

CCryProfilingSystem::~CCryProfilingSystem()
{
	Stop();
	s_pInstance = nullptr;
}

void CCryProfilingSystem::ProfilingSystemFilterCvarChangedCallback(ICVar* pCvar)
{
	if (s_pInstance == nullptr)
		return;

	if (strcmp(PROF_CVAR_SUBSYSTEM, pCvar->GetName()) == 0)
		s_pInstance->s_subsystemFilterMask = pCvar->GetI64Val();
	else if (strcmp(PROF_CVAR_VERBOSITY, pCvar->GetName()) == 0)
		s_pInstance->s_verbosity = pCvar->GetIVal();
	else if (strcmp(PROF_CVAR_THREAD, pCvar->GetName()) == 0)
		s_pInstance->s_exclusiveThread = (threadID)pCvar->GetI64Val();
	else
	{
		assert(!"Unrecognized CVar!");
	}

	s_pInstance->ReapplyTrackerFilter();
}

void CCryProfilingSystem::FilterSubsystemCommand(IConsoleCmdArgs* args)
{
	if (s_pInstance == nullptr)
		return;

	if (args->GetArgCount() < 3)
	{
		CryLog("Too few arguments. Usage: %s [0,1,2] System1 System2 ..", args->GetArg(0));
		return;
	}

	int mode;
	if (strcmp("0", args->GetArg(1)) == 0)
		mode = 0;
	else if (strcmp("1", args->GetArg(1)) == 0)
		mode = 1;
	else if (strcmp("2", args->GetArg(1)) == 0)
		mode = 2;
	else
	{
		CryLog("First argument has to be 0, 1 or 2.");
		return;
	}

	int64 changeMask = 0;
	for (int arg = 2; arg < args->GetArgCount(); ++arg)
	{
		bool found = false;
		for (int subsystemId = 0; subsystemId < EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM; ++subsystemId)
		{
			if (stricmp(args->GetArg(arg), s_pInstance->GetModuleName((EProfiledSubsystem)subsystemId)) == 0)
			{
				changeMask |= int64(1) << subsystemId;
				found = true;
				break;
			}
		}
		if(!found)
			CryLog("Unknown Subsystem name '%s'.", args->GetArg(arg));
	}

	if(changeMask)
	{
		ICVar* pCvar = gEnv->pConsole->GetCVar(PROF_CVAR_SUBSYSTEM);
		switch (mode)
		{
		case 0: pCvar->Set(pCvar->GetI64Val() & ~changeMask); break;
		case 1: pCvar->Set(pCvar->GetI64Val() | changeMask); break;
		case 2: pCvar->Set(~changeMask); break;
		}
		CryLog("Subsystem mask is now set to 0x%" PRIx64 ".", pCvar->GetI64Val());
	}
}

void CCryProfilingSystem::FilterThreadCommand(struct IConsoleCmdArgs* args)
{
	if (s_pInstance == nullptr)
		return;

	if (args->GetArgCount() != 2)
	{
		CryLog("Incorrect number of arguments. Usage: %s ThreadName", args->GetArg(0));
		return;
	}

	 threadID tid = gEnv->pThreadManager->GetThreadId(args->GetArg(1));
	 if (tid)
	 {
		 ICVar* pCvar = gEnv->pConsole->GetCVar(PROF_CVAR_THREAD);
		 pCvar->Set((int64)tid);
	 }
	 else
		CryLog("Thread not found.");
}

void CCryProfilingSystem::StopPofilingCommand(struct IConsoleCmdArgs* args)
{
	if (s_pInstance)
		s_pInstance->Stop();
}

void CCryProfilingSystem::RegisterCVars()
{
	REGISTER_INT64_CB(PROF_CVAR_SUBSYSTEM, 0, VF_BITFIELD,
		"Set the bitmask for filtering out subsystems from profiling. Set a bit to 1 to exclude to corresponding system from the profile."
		, ProfilingSystemFilterCvarChangedCallback);
	
	REGISTER_INT64_CB(PROF_CVAR_THREAD, 0, VF_NULL,
		"Allows filtering the profiling based on thread id. Only the thread with the given id will be included in the profile."
		, ProfilingSystemFilterCvarChangedCallback);
	REGISTER_COMMAND("profile_exclusiveThread", FilterThreadCommand, VF_NULL,
		"Allows filtering the profiling based on thread name. Only the thread with the given name will be included in the profile.");
	
	ICVar* pVerbosityVar = REGISTER_INT_CB(PROF_CVAR_VERBOSITY, s_verbosity, VF_NULL,
		"Sets the depth up to which profiling information will be gathered.\n"
		"Higher values lead to more detailed profiling information, but also slightly increase performance costs."
		, ProfilingSystemFilterCvarChangedCallback);
	pVerbosityVar->SetMaxValue(CRY_PROFILER_MAX_DEPTH);

	string filterSubsystemHelp = "Allows to turn on/off the profiling filter for subsystems by name.\n"
		"First parameter indicates if you want to enable or disable filtering.\n"
		"Use 1 to enable filtering, i.e. exclude the subsystem from profiling; otherwise use 0.\n"
		"Use 2 to profile a system/systems exclusively (i.e. disable all others).\n"
		"After that you can give the names of the subsystems you want to set the filter for.\n"
		"E.g. 'profile_filterSubsystem 1 AI Renderer' will exclude the AI and rendering systems from profiling.\n"
		"Available subsystem names are: ";
	for (int subsystemId = 0; subsystemId < EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM; ++subsystemId)
	{
		filterSubsystemHelp.append(GetModuleName((EProfiledSubsystem) subsystemId));
		if (subsystemId != EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM - 1)
			filterSubsystemHelp.append(", ");
	}

	REGISTER_COMMAND("profile_filterSubsystem", FilterSubsystemCommand, VF_NULL, filterSubsystemHelp.c_str());
	REGISTER_COMMAND("profile_stop", StopPofilingCommand, VF_NULL, "Terminates the profiling session. (Cannot be restarted. Use the 'Scroll Lock' key for un/pausing.)");

	REGISTER_CVAR(profile_peak_tolerance, 5.0f, VF_NULL, "Profiler Peaks Tolerance in Milliseconds."
		" If the self time of a section in one frame exceeds its average by more than the tolerance a peak is logged.");

	{
		ICVar* pvar = REGISTER_CVAR(profile_value, TIME, VF_NULL, 
			"Allows to set the value that should be profiled. Set to 0 for time and to 1 for allocated memory.\n"
			"Only use time profiling with the 'profile X' commands. Otherwise, e.g. Bootprofiler and Statoscope would give unexpected results.");
		pvar->SetMinValue(TIME);
		pvar->SetMaxValue(MEMORY);
	}
}

void CCryProfilingSystem::Stop()
{
	AutoTimer timer(this);
	AutoWriteLock autoLock(m_trackerLock);
	if (m_enabled)
	{
		m_enabled = false;
		stl::free_container(m_activeTrackers);
		stl::free_container(m_excludedTrackers);
		stl::free_container(m_FrameListeners);
		m_pBootProfiler = nullptr;
	}
}

bool CCryProfilingSystem::IsStopped() const
{
	return !m_enabled;
}

void CCryProfilingSystem::PauseRecording(bool pause)
{
	// actual pausing is triggered at frame end
	m_willPause = pause;
}

bool CCryProfilingSystem::IsPaused() const
{
	return m_trackingPaused;
}

void CCryProfilingSystem::StartThread() {}

void CCryProfilingSystem::EndThread() {}

void CCryProfilingSystem::AddFrameListener(ICryProfilerFrameListener* pListener)
{
	AutoWriteLock autoLock(m_trackerLock);
	if (m_enabled)
	{
		m_FrameListeners.push_back(pListener);
	}
}

void CCryProfilingSystem::SetBootProfiler(CBootProfiler* pProf)
{
	m_pBootProfiler = pProf;
}

void CCryProfilingSystem::RemoveFrameListener(ICryProfilerFrameListener* pListener)
{
	AutoTimer timer(this);
	AutoWriteLock autoLock(m_trackerLock);
	stl::find_and_erase(m_FrameListeners, pListener);
}

void CCryProfilingSystem::DescriptionCreated(SProfilingSectionDescription* pDesc)
{
	pDesc->color_argb = GenerateColorBasedOnName(pDesc->szEventname);
}

void CCryProfilingSystem::DescriptionDestroyed(SProfilingSectionDescription* pDesc)
{
	AutoTimer timer(this);
	AutoWriteLock lock(m_trackerLock);

	SProfilingSectionTracker* pTracker = (SProfilingSectionTracker*) pDesc->customData;
	while (pTracker)
	{
		bool erased = false;
		if (pTracker->isActive)
			erased = stl::find_and_erase(m_activeTrackers, pTracker);
		else
			erased = stl::find_and_erase(m_excludedTrackers, pTracker);
		CRY_PROFILER_ASSERT(erased);

		SProfilingSectionTracker* tmp = pTracker;
		pTracker = pTracker->pNextThreadTracker;
		delete tmp;
	}
	pDesc->customData = 0;
}

SProfilingSectionTracker* CCryProfilingSystem::AddTracker(const SProfilingSectionDescription* pDesc, threadID tid)
{
	AutoWriteLock autoLock(m_trackerLock);

	// look up to ensure there is not already a tracker for the current thread
	SProfilingSectionTracker* pTracker = (SProfilingSectionTracker*)pDesc->customData;
	while (pTracker && pTracker->threadId != tid)
		pTracker = pTracker->pNextThreadTracker;

	if (pTracker)
		return pTracker;
	
	// didn't find any, so create one
	pTracker = new SProfilingSectionTracker(pDesc, tid);
	
	pTracker->isActive = !IsExcludedByFilter(pTracker);
	if (pTracker->isActive)
		m_activeTrackers.push_back(pTracker);
	else
		m_excludedTrackers.push_back(pTracker);

	// link new tracker into list
	pTracker->pNextThreadTracker = (SProfilingSectionTracker*) pDesc->customData;
	pDesc->customData = (uintptr_t) pTracker;

	return pTracker;
}

bool CCryProfilingSystem::StartSection(SProfilingSection* pSection)
{
	if (!m_enabled)
		return false;
	AutoTimer timer(this);

	if (tls_profilingStackDepth >= s_verbosity)
		return false;

#if CRY_PROFILER_COMBINE_JOB_TRACKERS
	const threadID currentTID = (gEnv->pJobManager && JobManager::IsWorkerThread()) ? CRY_PROFILER_JOB_THREAD_ID : CryGetCurrentThreadId();
#else
	const threadID currentTID = CryGetCurrentThreadId();
#endif

	SProfilingSectionTracker* pTracker = nullptr;
	{
		AutoReadLock autoLock(m_trackerLock);

		// look for tracker in the description's custom data
		pTracker = (SProfilingSectionTracker*)pSection->pDescription->customData;
		while (pTracker && pTracker->threadId != currentTID)
			pTracker = pTracker->pNextThreadTracker;

		// did not find a tracker, so create a new one
		if (pTracker == nullptr)
		{
			// AddTracker() write-locks internally
			m_trackerLock.RUnlock();
			pTracker = AddTracker(pSection->pDescription, currentTID);
			m_trackerLock.RLock();
		}
	}
	CRY_PROFILER_ASSERT(pTracker);

	// no tracking on excluded sections
	if (!pTracker->isActive)
		return false;
	if (!m_trackingPaused)
		++pTracker->count;

	// use section custom data to store tracker, so we don't have to look it up again later
	pSection->customData = (uintptr_t) pTracker;

	if(profile_value == MEMORY)
		pSection->startValue = CryMemoryGetThreadAllocatedSize();
	else
		pSection->startValue = timer.timeStamp;
	
#if defined(ENABLE_LOADING_PROFILER)
 	if (m_pBootProfiler)
		m_pBootProfiler->OnSectionStart(*pSection);
#endif

	tls_currentSections[tls_profilingStackDepth] = pSection;
	++tls_profilingStackDepth;
	return true;
}

void CCryProfilingSystem::EndSection(SProfilingSection* pSection)
{
	if (!m_enabled)
		return;
	AutoTimer timer(this);

	// We need to finish sections that were started before pausing, so we pop before checking for m_trackingPaused
	--tls_profilingStackDepth;
	CRY_PROFILER_ASSERT(tls_profilingStackDepth >= 0);
	CRY_PROFILER_ASSERT(tls_currentSections[tls_profilingStackDepth] == pSection);

	// calculate measurements
	SProfilingSectionEnd sectionEnd;
	if (profile_value == MEMORY)
	{
		sectionEnd.totalValue = CryMemoryGetThreadAllocatedSize() - pSection->startValue;
		sectionEnd.selfValue = sectionEnd.totalValue - pSection->childExcludeValue;
	}
	else
	{
		sectionEnd.totalValue = timer.timeStamp - pSection->startValue;
		CRY_PROFILER_ASSERT(sectionEnd.totalValue >= 0);

		sectionEnd.selfValue = sectionEnd.totalValue - pSection->childExcludeValue;
		CRY_PROFILER_ASSERT(sectionEnd.selfValue >= 0);
	}

#if defined(ENABLE_LOADING_PROFILER)	
	if (m_pBootProfiler)
		m_pBootProfiler->OnSectionEnd(*pSection, sectionEnd);
#endif

	if (!m_trackingPaused)
	{
		SProfilingSection* const pParentSection = tls_profilingStackDepth > 0 ? tls_currentSections[tls_profilingStackDepth - 1] : nullptr;
		SProfilingSectionTracker* const pTracker = (SProfilingSectionTracker*)pSection->customData;
#if CRY_PROFILER_COMBINE_JOB_TRACKERS
		CRY_PROFILER_ASSERT(pTracker->threadId == CryGetCurrentThreadId() || (JobManager::IsWorkerThread() && pTracker->threadId == CRY_PROFILER_JOB_THREAD_ID));
#else
		CRY_PROFILER_ASSERT(pTracker->threadId == CryGetCurrentThreadId());
#endif

		if (pParentSection != nullptr)
		{
			pParentSection->childExcludeValue += sectionEnd.totalValue;
		}

		// update tracker
		pTracker->totalValue += sectionEnd.totalValue;
		pTracker->selfValue += sectionEnd.selfValue;
		if (profile_value == MEMORY)
			pTracker->peakSelfValue = max(pTracker->peakSelfValue, SMemoryUpdateTraits::ToFloat(sectionEnd.selfValue));
		else
			pTracker->peakSelfValue = max(pTracker->peakSelfValue, STickUpdateTraits::ToFloat(sectionEnd.selfValue));
	}
}

void CCryProfilingSystem::StartFrame()
{
	if (!m_enabled)
		return;
#if defined(ENABLE_LOADING_PROFILER)
	AutoTimer timer(this);

	if (m_pBootProfiler)
		m_pBootProfiler->OnFrameStart();
#endif
}

void CCryProfilingSystem::EndFrame()
{
	if (IsStopped())
		return;
	AutoTimer timer(this);

	const float blendFactor = gEnv->pTimer->GetProfileFrameBlending();

#if CRY_PLATFORM_WINDOWS
	typedef BOOL(WINAPI * TGetProcessMemoryInfo)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
	static TGetProcessMemoryInfo pfGetProcessMemoryInfo = nullptr;
	static bool s_missingPsapiDll = false;

	if (!pfGetProcessMemoryInfo)
	{
		if (!s_missingPsapiDll)
		{
			HMODULE hPsapiModule = ::LoadLibraryA("psapi.dll");
			if (hPsapiModule)
			{
				pfGetProcessMemoryInfo = (TGetProcessMemoryInfo)(::GetProcAddress(hPsapiModule, "GetProcessMemoryInfo"));
			}
			else
				s_missingPsapiDll = true;
		}
	}

	if (pfGetProcessMemoryInfo)
	{
		PROCESS_MEMORY_COUNTERS pc;
		pfGetProcessMemoryInfo(GetCurrentProcess(), &pc, sizeof(pc));
		m_pageFaultsPerFrame = pc.PageFaultCount - m_lastTotalPageFaults;
		m_pageFaultsPerFrame.CommitSample<SCountUpdateTraits>(blendFactor);
		m_lastTotalPageFaults = pc.PageFaultCount;
	}
#endif

	m_profilingOverhead.CommitSample<STickUpdateTraits>(blendFactor);
	if (!m_trackingPaused)
	{
		{ AutoWriteLock autoLock(m_trackerLock);
			for (SProfilingSectionTracker* pTracker : m_activeTrackers)
				CommitFrame(pTracker, blendFactor);

			for (SProfilingSectionTracker* pTracker : m_activeTrackers)
				CheckForPeak(timer.timeStamp, pTracker);
		}

		{ AutoReadLock autoLock(m_trackerLock);
			for (ICryProfilerFrameListener* pProfiler : m_FrameListeners)
				pProfiler->OnFrameEnd(timer.timeStamp, this);

			for (SProfilingSectionTracker* pTracker : m_activeTrackers)
				pTracker->peakSelfValue = 0;
		}
	}

#if defined(ENABLE_LOADING_PROFILER)
	if (m_pBootProfiler)
		m_pBootProfiler->OnFrameEnd();
#endif

	bool neededForDisplayinfo =  false;
	if (gEnv->pConsole)
	{
		ICVar* pRDisplayinfo = gEnv->pConsole->GetCVar("r_displayinfo");
		if (pRDisplayinfo)
			neededForDisplayinfo = pRDisplayinfo->GetIVal() == 2 || pRDisplayinfo->GetIVal() >= 5;
	}
	// pause requested or no one needs the results
	m_trackingPaused = m_willPause || (m_FrameListeners.empty() && !neededForDisplayinfo);
}

void CCryProfilingSystem::RecordMarker(SProfilingMarker* pMarker)
{
	if (!m_enabled || m_pBootProfiler == nullptr)
		return;
	AutoTimer timer(this);

	if (IsExcludedByFilter(pMarker))
		return;

	if(pMarker->pDescription->color_argb == 0)
	{
		pMarker->pDescription->color_argb = GenerateColorBasedOnName(pMarker->pDescription->szMarkername);
	}

#if defined(ENABLE_LOADING_PROFILER)
	m_pBootProfiler->OnMarker(timer.timeStamp, *pMarker);
#endif
}

const CCryProfilingSystem::TrackerList* CCryProfilingSystem::GetActiveTrackers() const
{
	return &m_activeTrackers;
}

const CCryProfilingSystem::PeakList* CCryProfilingSystem::GetPeakRecords() const
{
	return &m_peaks;
}

void CCryProfilingSystem::CheckForPeak(int64 time, SProfilingSectionTracker* pTracker)
{
	const float average = pTracker->selfValue.Average();
	const float peakValue = pTracker->selfValue.Latest();

	if ((peakValue - average) > profile_peak_tolerance)
	{
		SPeakRecord peak;
		peak.pTracker = pTracker;
		peak.peakValue = peakValue;
		peak.averageValue = average;
		peak.variance = pTracker->selfValue.Variance();
		peak.count = int(pTracker->count.Latest());

		peak.pageFaults = m_pageFaultsPerFrame.CurrentRaw();
		peak.frame = gEnv->nMainFrameID;
		peak.timeSeconds = time * m_MsPerTick * 0.001f;
		peak.waiting = pTracker->pDescription->isWaiting;
		
		m_peaks.push_front_overwrite(peak);
	}
}

void CCryProfilingSystem::CommitFrame(SProfilingSectionTracker* pTracker, float blendFactor)
{
	pTracker->count.CommitSample<SCountUpdateTraits>(blendFactor);
	if(profile_value == TIME)
	{
		pTracker->selfValue.CommitSample<STickUpdateTraits>(blendFactor);
		pTracker->totalValue.CommitSample<STickUpdateTraits>(blendFactor);
	}
	else
	{
		pTracker->selfValue.CommitSample<SMemoryUpdateTraits>(blendFactor);
		pTracker->totalValue.CommitSample<SMemoryUpdateTraits>(blendFactor);
	}
}

bool CCryProfilingSystem::IsExcludedByFilter(const SProfilingSectionTracker* pTracker) const
{
	if ((int64(1) << (uint8) pTracker->pDescription->subsystem) & s_subsystemFilterMask)
		return true;

	if (s_exclusiveThread != 0 && pTracker->threadId != s_exclusiveThread)
		return true;

	return false;
}

bool CCryProfilingSystem::IsExcludedByFilter(const SProfilingMarker* pMarker) const
{
	if ((int64(1) << (uint8)pMarker->pDescription->subsystem) & s_subsystemFilterMask)
		return true;

	if (s_exclusiveThread != 0 && pMarker->threadId != s_exclusiveThread)
		return true;

	return false;
}

void CCryProfilingSystem::ReapplyTrackerFilter()
{
	AutoTimer timer(this);

	TrackerList activeTrackers;
	TrackerList excludedTrackers;

	{ AutoReadLock autoLock(m_trackerLock);

		activeTrackers.reserve(m_activeTrackers.size() + m_excludedTrackers.size());
		excludedTrackers.reserve(m_activeTrackers.size() + m_excludedTrackers.size());

		for (SProfilingSectionTracker* pTracker : m_activeTrackers)
		{
			pTracker->isActive = !IsExcludedByFilter(pTracker);
			if (pTracker->isActive)
				activeTrackers.push_back(pTracker);
			else
				excludedTrackers.push_back(pTracker);
		}

		const float blendFactor = gEnv->pTimer->GetProfileFrameBlending();
		for (SProfilingSectionTracker* pTracker : m_excludedTrackers)
		{
			pTracker->isActive = !IsExcludedByFilter(pTracker);
			if (pTracker->isActive)
			{
				CommitFrame(pTracker, blendFactor);
				activeTrackers.push_back(pTracker);
			}
			else
				excludedTrackers.push_back(pTracker);
		}
	}

	{ AutoWriteLock autoLock(m_trackerLock);
		
		m_activeTrackers = activeTrackers;
		m_excludedTrackers = excludedTrackers;
	}
}

void CCryProfilingSystem::AcquireReadAccess()
{
	m_trackerLock.RLock();
}

void CCryProfilingSystem::ReleaseReadAccess()
{
	m_trackerLock.RUnlock();
}

CCryProfilingSystem* CCryProfilingSystem::s_pInstance = nullptr;

bool  CCryProfilingSystem::StartSectionStatic(SProfilingSection* pSection)
{
	return s_pInstance->StartSection(pSection);
}

void CCryProfilingSystem::EndSectionStatic(SProfilingSection* pSection)
{
	s_pInstance->EndSection(pSection);
}

void CCryProfilingSystem::RecordMarkerStatic(SProfilingMarker* pMarker)
{
	s_pInstance->RecordMarker(pMarker);
}

const char* CCryProfilingSystem::GetModuleName(const SProfilingSectionTracker* pTracker) const
{
	return GetModuleName(pTracker->pDescription->subsystem);
}

const CSamplerStats<TProfilingCount>& CCryProfilingSystem::PageFaultsPerFrame() const
{
	return m_pageFaultsPerFrame;
}
