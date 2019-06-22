// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryProfilingSystemSharedImpl.h"
#include "CryCore/RingBuffer.h"
#include <CrySystem/Profilers/ILegacyProfiler.h>
#include <CryThreading/CryThread.h>

class CBootProfiler;

//! the system which does the gathering of stats
class CCryProfilingSystem final : public CCryProfilingSystemImpl, public ILegacyProfiler
{
public:
	enum EProfiledValue
	{
		TIME,
		MEMORY,
	};

	CCryProfilingSystem();
	~CCryProfilingSystem();

	//////////////////////////////////////////////////////////
	// ICryProfilingSystem
	void Stop() final;
	bool IsStopped() const final;

	void PauseRecording(bool pause) final;
	//! Are we collecting profiling data?
	bool IsPaused() const final;
	
	void StartThread() final;
	void EndThread() final;

	void AddFrameListener(ICryProfilerFrameListener*) final;
	void RemoveFrameListener(ICryProfilerFrameListener*) final;

	void DescriptionCreated(SProfilingSectionDescription*) final;
	void DescriptionDestroyed(SProfilingSectionDescription*) final;

	bool StartSection(SProfilingSection*) final;
	void EndSection(SProfilingSection*) final;

	void StartFrame() final;
	void EndFrame() final;

	void RecordMarker(SProfilingMarker*) final;

	using CCryProfilingSystemImpl::GetModuleName;
	//////////////////////////////////////////////////////////

	// ILegacyProfiler
	const TrackerList* GetActiveTrackers() const final;
	const PeakList* GetPeakRecords() const final;

	void AcquireReadAccess() final;
	void ReleaseReadAccess() final;

	const CSamplerStats<TProfilingCount>& PageFaultsPerFrame() const final;
	const char* GetModuleName(const SProfilingSectionTracker*) const final;
	// ~ILegacyProfiler

	// CCryProfilingSystemImpl
	void RegisterCVars() final;

	void SetBootProfiler(CBootProfiler*);
	
private:
	struct AutoTimer;
	typedef CryAutoReadLock<CryRWLock> AutoReadLock;
	typedef CryAutoWriteLock<CryRWLock> AutoWriteLock;

	SProfilingSectionTracker* AddTracker(const SProfilingSectionDescription* pDesc, threadID tid);

	void CheckForPeak(int64, SProfilingSectionTracker* pTracker);
	void CommitFrame(SProfilingSectionTracker* pTracker, float blendFactor);

	bool IsExcludedByFilter(const SProfilingSectionTracker* pTracker) const;
	bool IsExcludedByFilter(const SProfilingMarker* pMarker) const;
	void ReapplyTrackerFilter();

	bool m_enabled;
	bool m_trackingPaused;
	bool m_willPause;

	CBootProfiler* m_pBootProfiler;
	std::vector<ICryProfilerFrameListener*> m_FrameListeners;

	uint32 m_lastTotalPageFaults;

	//! trackers can be filtered by thread and subsystem
	TrackerList m_activeTrackers;
	const float m_MsPerTick;
	PeakList m_peaks;
	
	// trackers currently filtered out
	TrackerList m_excludedTrackers;

	// putting these into separate cache lines to avoid false sharing
	CRY_ALIGN(64) CryRWLock m_trackerLock;
	CRY_ALIGN(64) char __padding;
	
	friend class CSystem;
	//////////////////////////////////////////////////////////
	// CVars and callbacks

	static CCryProfilingSystem* s_pInstance;
	// callbacks to register in gEnv
	static bool StartSectionStatic(SProfilingSection*);
	static void EndSectionStatic(SProfilingSection*);
	static void RecordMarkerStatic(SProfilingMarker*);

	static void ProfilingSystemFilterCvarChangedCallback(struct ICVar* pCvar);
	static void FilterSubsystemCommand(struct IConsoleCmdArgs* args);
	static void FilterThreadCommand(struct IConsoleCmdArgs* args);
	static void StopPofilingCommand(struct IConsoleCmdArgs* args);
	
	//! trackers with verbosity above this level are excluded
	static int s_verbosity;
	//! if non-zero only profile this thread
	static threadID s_exclusiveThread;
	//! the bit (1 << ProfiledSubsystem) is set, if the subsystem should be excluded
	static int64 s_subsystemFilterMask;
	static_assert(EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM < 64, "Subsystems no longer fit in bitmask!");

	static float profile_peak_tolerance;
	static int profile_value;
};
