// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Interface to the 'old' CryEngine-internal profiling system

#include <CryCore/Platform/platform.h>
#include "ICryProfilingSystem.h"
#include "SamplerStats.h"
#include <CryCore/RingBuffer.h>

//! Static instances of this class are created at the places we want to profile to track every invocation of the code.
struct SProfilingSectionTracker
{
	SProfilingSectionTracker(const SProfilingSectionDescription* pDesc, threadID tid) 
		: pDescription(pDesc), threadId(tid)
		, pNextThreadTracker(nullptr), peakSelfValue(0)
	{}

	//! Description of the tracked event.
	const SProfilingSectionDescription* pDescription;
	//! Thread Id of this instance.
	threadID threadId;
	//! trackers for different threads are stored as linked list
	SProfilingSectionTracker* pNextThreadTracker;

	//! Profiling may be filtered by subsystem or thread. Filtering status is cached here.
	bool isActive;
	
	// the following data is reset after each frame
	//! How many times this profiler counter was executed.
	CSamplerStats<TProfilingCount> count;
	//! Total time spent in this counter including time of child profilers.
	CSamplerStats<TProfilingValue> totalValue;
	//! Time spent in this counter excluding time of child profilers (but includes recursive calls to same counter).
	CSamplerStats<TProfilingValue> selfValue;
	//! longest selfValue of an associated event
	float peakSelfValue;
};

struct SPeakRecord
{
	SProfilingSectionTracker* pTracker;
	float peakValue;
	float averageValue;
	float variance;
	int   pageFaults;  //!< Number of page faults at this frame.
	int   count;       //!< Number of times called for peak.
	uint  frame;       //!< When it happened.
	float timeSeconds; //!< When it happened.
	BYTE  waiting;     //!< If it needs to go in separate waiting peak list
};

struct ICryProfilerFrameListener;

//! Profiling interface used by engine-internal runtime profilers.
//! This interface is used by e.g. Statoscope and TimeDemoRecorder.
//! Avoid using it in new code.
struct ILegacyProfiler
{
	virtual void AddFrameListener(ICryProfilerFrameListener*) = 0;
	virtual void RemoveFrameListener(ICryProfilerFrameListener*) = 0;

	// access to trackers and peaks should be guarded by these calls, if outside of an implementation of ICryProfiler
	// otherwise multi-threading correctness is not guaranteed
	virtual void AcquireReadAccess() = 0;
	virtual void ReleaseReadAccess() = 0;

	typedef std::vector<SProfilingSectionTracker*> TrackerList;
	virtual const TrackerList* GetActiveTrackers() const = 0;

	static const uint MAX_TRACKED_PEAKS = 128;
	typedef CRingBuffer<SPeakRecord, MAX_TRACKED_PEAKS, uint32> PeakList;
	virtual const PeakList* GetPeakRecords() const = 0;

	virtual const char* GetModuleName(const SProfilingSectionTracker*) const = 0;

	virtual const CSamplerStats<TProfilingCount>& PageFaultsPerFrame() const = 0;

protected:
	~ILegacyProfiler() = default;
};

struct SCountUpdateTraits
{
	static float ToFloat(TProfilingCount val) { return float(val); }
};

struct STickUpdateTraits
{
	static float ToFloat(TProfilingValue val) { return gEnv->pTimer->TicksToSeconds(val) * 1000.0f; }
};

struct SMemoryUpdateTraits
{
	static float ToFloat(TProfilingValue val) { return float(val) * (1.0f / 1024); }
};

// (experimental) if set, there will be only one tracker for all jobs together, instead of one per job
// the SProfilingSectionTracker::threadId is then different from CryGetCurrentThreadId() for job worker threads
#define CRY_PROFILER_COMBINE_JOB_TRACKERS 0
// threadID used for the combined job tracker
#define CRY_PROFILER_JOB_THREAD_ID (~threadID(0))

struct SProfilingSectionEnd
{
	TProfilingValue totalValue;
	TProfilingValue selfValue;
};

struct ICryProfilerFrameListener
{
	typedef int64 TTime;

	virtual void OnFrameEnd(TTime, ILegacyProfiler*) {}

protected:
	~ICryProfilerFrameListener() = default;
};
