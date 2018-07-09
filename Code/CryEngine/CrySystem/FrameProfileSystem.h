// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   frameprofilesystem.h
//  Version:     v1.00
//  Created:     24/6/2003 by Timur,Sergey,Wouter.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler.h>
#include <CryInput/IInput.h>

#ifdef USE_FRAME_PROFILER

extern ColorF profile_colors[];

//////////////////////////////////////////////////////////////////////////
//! the system which does the gathering of stats
// unfortunately the IInput system does not work on win32 in the editor mode
// so this has to be ugly. can be removed as soon as input is handled in
// a unified way [mm]
class CFrameProfileSystem : public IFrameProfileSystem, public IInputEventListener
{
public:
	int   m_nCurSample;

	char* m_pPrefix;
	bool  m_bEnabled;
	//! True when collection must be paused.
	bool  m_bCollectionPaused;

	//! If set profiling data will be collected.
	bool                        m_bCollect;
	//! If set profiling data will be displayed.
	bool                        m_bDisplay;
	//! True if network profiling is enabled.
	bool                        m_bMemoryProfiling;
	//! Put memory info also in the log.
	bool                        m_bLogMemoryInfo;

	IRenderer*                  m_pRenderer;

	static CFrameProfileSystem* s_pFrameProfileSystem;
	static threadID             s_nFilterThreadId;

	// Cvars.
	int   profile_graph;
	float profile_graphScale;
	int   profile_pagefaults;
	int   profile_network;
	int   profile_additionalsub;
	float profile_peak;
	float profile_peak_display;
	float profile_min_display_ms;
	float profile_row, profile_col;
	int   profile_meminfo;
	int   profile_sampler_max_samples;
	int   profile_callstack;
	int   profile_log;

	//struct SPeakRecord
	//{
	//	CFrameProfiler *pProfiler;
	//	float peakValue;
	//	float averageValue;
	//	float variance;
	//	int pageFaults; // Number of page faults at this frame.
	//	int count;  // Number of times called for peak.
	//	float when; // when it added.
	//};
	struct SProfilerDisplayInfo
	{
		float           x, y; // Position where this profiler rendered.
		int             averageCount;
		int             level; // child level.
		CFrameProfiler* pProfiler;
	};
	struct SSubSystemInfo
	{
		const char* name = "";
		float       selfTime = 0;
		float       waitTime = 0;
		float       budgetTime = 0;
		float       maxTime = 0;
		int         totalAnalized = 0;
		int         totalOverBudget = 0;
	};

	EDisplayQuantity m_displayQuantity;

	//! When profiling frame started.
	int64 m_frameStartTime;
	//! Total time of profiling.
	int64 m_totalProfileTime;
	//! Frame time from the last frame.
	int64 m_frameTime;
	//! Ticks per profiling call, for adjustment.
	int64 m_callOverheadTime;
	int64 m_nCallOverheadTotal;
	int64 m_nCallOverheadRemainder;
	int32 m_nCallOverheadCalls;

	//! Smoothed version of frame time, lost time, and overhead.
	float                     m_frameSecAvg;
	float                     m_frameLostSecAvg;
	float                     m_frameOverheadSecAvg;

	CryCriticalSection        m_profilersLock;
	static CryCriticalSection m_staticProfilersLock;

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)

	ILINE void ValThreadFrameStatsCapacity(int8 numWorkers)
	{
		if (m_ThreadFrameStats->numWorkers == numWorkers)
			return;

		SAFE_DELETE(m_ThreadFrameStats);
		m_ThreadFrameStats = new JobManager::CWorkerFrameStats(numWorkers);
	}

	ILINE void ValBlockingFrameStatsCapacity(int8 numWorkers)
	{
		if (m_BlockingFrameStats->numWorkers == numWorkers)
			return;

		SAFE_DELETE(m_BlockingFrameStats);
		m_BlockingFrameStats = new JobManager::CWorkerFrameStats(numWorkers);
	}

	//Job frame stats
	JobManager::CWorkerFrameStats*                              m_ThreadFrameStats;
	JobManager::CWorkerFrameStats*                              m_BlockingFrameStats;

	JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer m_ThreadJobFrameStats;
	JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer m_BlockingJobFrameStats;

	JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer* GetActiveFrameStats(JobManager::EBackEndType backeEndType)
	{
		switch (backeEndType)
		{
		case JobManager::eBET_Thread:
			return &m_ThreadJobFrameStats;
		case JobManager::eBET_Blocking:
			return &m_BlockingJobFrameStats;
		default:
			CRY_ASSERT_MESSAGE(0, "Unsupported BackEndType encountered.");
		}
		;

		return 0;
	}

	const uint32 GetActiveFrameStatCount(JobManager::EBackEndType backeEndType)
	{
		switch (backeEndType)
		{
		case JobManager::eBET_Thread:
			return m_ThreadJobFrameStats.size();
		case JobManager::eBET_Blocking:
			return m_BlockingJobFrameStats.size();
		default:
			CRY_ASSERT_MESSAGE(0, "Unsupported BackEndType encountered.");
		}
		;

		return 0;
	}

	#endif

	//! Maintain separate profiler stacks for each thread.
	//! Disallow any post-init allocation, to avoid profiler/allocator conflicts
	typedef threadID TThreadId;
	struct SProfilerThreads
	{
		SProfilerThreads(TThreadId nMainThreadId)
		{
			// First thread stack is for main thread.
			m_aThreadStacks[0].threadId = nMainThreadId;
			m_nThreadStacks = 1;
			m_pReservedProfilers = 0;
			m_lock = 0;
		}

		void            Reset();

		ILINE TThreadId GetMainThreadId() const
		{
			return m_aThreadStacks[0].threadId;
		}
		ILINE CFrameProfilerSection const* GetMainSection() const
		{
			return m_aThreadStacks[0].pProfilerSection;
		}

		static ILINE void Push(CFrameProfilerSection*& parent, CFrameProfilerSection* child)
		{
			assert(child);
			child->m_pParent = parent;
			parent = child;
		}

		ILINE void PushSection(CFrameProfilerSection* pSection, TThreadId threadId)
		{
			assert(threadId != TThreadId(0) && pSection);

			for (int i = 0, end = m_nThreadStacks; i < end; ++i)
			{
				if (m_aThreadStacks[i].threadId == threadId)
				{
					Push(m_aThreadStacks[i].pProfilerSection, pSection);
					return;
				}
			}

			// add new thread stack entry
			int newIndex = CryInterlockedIncrement((volatile int*)&m_nThreadStacks) - 1;

			if (newIndex == nMAX_THREADS)
				CryFatalError("Profiled thread count of %d exceeded!", nMAX_THREADS);

			m_aThreadStacks[newIndex].threadId = threadId;
			Push(m_aThreadStacks[newIndex].pProfilerSection, pSection);
			return;
		}

		ILINE void PopSection(CFrameProfilerSection* pSection, TThreadId threadId)
		{
			// Thread-safe without locking.
			for (int i = 0; i < m_nThreadStacks; ++i)
				if (m_aThreadStacks[i].threadId == threadId)
				{
					assert(m_aThreadStacks[i].pProfilerSection == pSection || m_aThreadStacks[i].pProfilerSection == 0);
					m_aThreadStacks[i].pProfilerSection = pSection->m_pParent;
					return;
				}
			assert(0);
		}

		ILINE CFrameProfiler* GetThreadProfiler(CFrameProfiler* pMainProfiler, TThreadId nThreadId)
		{
			// Check main threads, or existing linked threads.
			if (nThreadId == GetMainThreadId())
				return pMainProfiler;
			for (CFrameProfiler* pProfiler = pMainProfiler->m_pNextThread; pProfiler; pProfiler = pProfiler->m_pNextThread)
				if (pProfiler->m_threadId == nThreadId)
					return pProfiler;
			return NewThreadProfiler(pMainProfiler, nThreadId);
		}

		ILINE void OnEnterSliceAndSleep(TThreadId nThreadId)
		{
			for (CFrameProfilerSection* pSection = m_aThreadStacks[m_sliceAndSleepThreadIdx].pProfilerSection; pSection; pSection = pSection->m_pParent)
			{
				if (pSection->m_pFrameProfiler)
				{
					CFrameProfileSystem::AccumulateProfilerSection(pSection);
				}
			}
		}

		ILINE void OnLeaveSliceAndSleep(TThreadId nThreadId)
		{
			int64 now = CryGetTicks();

			for (CFrameProfilerSection* pSection = m_aThreadStacks[m_sliceAndSleepThreadIdx].pProfilerSection; pSection; pSection = pSection->m_pParent)
			{
				if (pSection->m_pFrameProfiler)
				{
					pSection->m_startTime = now;
					pSection->m_excludeTime = 0;
				}
			}
		}

		CFrameProfiler* NewThreadProfiler(CFrameProfiler* pMainProfiler, TThreadId nThreadId);

	protected:

		struct SThreadStack
		{
			TThreadId              threadId;
			CFrameProfilerSection* pProfilerSection;

			SThreadStack(TThreadId id = 0)
				: threadId(id), pProfilerSection(0)
			{}
		};
		static const int nMAX_THREADS = 128;
		static const int m_sliceAndSleepThreadIdx = 0;          // SLICE_AND_SLEEP and Statoscope tick are on main thread
		int              m_nThreadStacks;
		SThreadStack     m_aThreadStacks[nMAX_THREADS];
		CFrameProfiler*  m_pReservedProfilers;
		volatile int     m_lock;
	};
	SProfilerThreads        m_ProfilerThreads;
	CCustomProfilerSection* m_pCurrentCustomSection;

	typedef std::vector<CFrameProfiler*> Profilers;
	//! Array of all registered profilers.
	Profilers  m_profilers;
	//! Network profilers, they are not in regular list.
	Profilers  m_netTrafficProfilers;
	//! Currently active profilers array.
	Profilers* m_pProfilers;

	//! List of several latest peaks.
	std::vector<SPeakRecord>          m_peaks;
	std::vector<SPeakRecord>          m_absolutepeaks;
	std::vector<SProfilerDisplayInfo> m_displayedProfilers;
	bool                              m_bDisplayedProfilersValid;
	EProfiledSubsystem                m_subsystemFilter;
	bool                              m_bSubsystemFilterEnabled;
	int                               m_maxProfileCount;

	//////////////////////////////////////////////////////////////////////////
	//! Smooth frame time in milliseconds.
	CFrameProfilerSamplesHistory<float, 32> m_frameTimeHistory;
	CFrameProfilerSamplesHistory<float, 32> m_frameTimeLostHistory;

	//////////////////////////////////////////////////////////////////////////
	// Graphs.
	//////////////////////////////////////////////////////////////////////////
	std::vector<unsigned char> m_timeGraphPageFault;
	std::vector<unsigned char> m_timeGraphFrameTime;
	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)

	std::vector<std::vector<unsigned char>> m_timeGraphWorkers;
	int m_nWorkerGraphCurPos;

	#endif
	int             m_timeGraphCurrentPos;
	CFrameProfiler* m_pGraphProfiler;

	//////////////////////////////////////////////////////////////////////////
	// Histograms.
	//////////////////////////////////////////////////////////////////////////
	bool  m_bEnableHistograms;
	int   m_histogramsCurrPos;
	int   m_histogramsMaxPos;
	int   m_histogramsHeight;

	//////////////////////////////////////////////////////////////////////////
	// Selection/Render.
	//////////////////////////////////////////////////////////////////////////
	int    m_selectedRow;
	float  ROW_SIZE, COL_SIZE;
	float  m_baseY;
	float  m_offset;
	int    m_textModeBaseExtra;

	int    m_nPagesFaultsLastFrame;
	int    m_nPagesFaultsPerSec;
	int64  m_nLastPageFaultCount;

	string m_filterThreadName;

	//////////////////////////////////////////////////////////////////////////
	// Subsystems.
	//////////////////////////////////////////////////////////////////////////
	SSubSystemInfo               m_subsystems[PROFILE_LAST_SUBSYSTEM];

	CFrameProfilerOfflineHistory m_frameTimeOfflineHistory;

	//////////////////////////////////////////////////////////////////////////
	// Peak callbacks.
	//////////////////////////////////////////////////////////////////////////
	std::vector<IFrameProfilePeakCallback*> m_peakCallbacks;

	class CSampler*                         m_pSampler;

public:
	//////////////////////////////////////////////////////////////////////////
	// Methods.
	//////////////////////////////////////////////////////////////////////////
	CFrameProfileSystem();
	~CFrameProfileSystem();
	void Init();
	void Done();

	void SetProfiling(bool on, bool display, char* prefix, ISystem* pSystem);

	//////////////////////////////////////////////////////////////////////////
	// IFrameProfileSystem interface implementation.
	//////////////////////////////////////////////////////////////////////////
	//! Reset all profiling data.
	void Reset();
	//! Add new frame profiler.
	//! Profile System will not delete those pointers, client must take care of memory management issues.
	void         AddFrameProfiler(CFrameProfiler* pProfiler);
	//! Remove existing frame profiler.
	virtual void RemoveFrameProfiler(CFrameProfiler* pProfiler);
	//! Must be called at the start of the frame.
	void         StartFrame();
	//! Must be called at the end of the frame.
	void         EndFrame();
	//! Must be called when something quacks like the end of the frame.
	void         OnSliceAndSleep();
	//! Get number of registered frame profilers.
	int          GetProfilerCount() const { return (int)m_profilers.size(); };
	//! Return the fraction used to blend current with average values.
	float        GetSmoothFactor() const;

	virtual int  GetPeaksCount() const
	{
		return (int)m_absolutepeaks.size();
	}

	virtual const SPeakRecord* GetPeak(int index) const
	{
		if (index >= 0 && index < (int)m_absolutepeaks.size())
			return &m_absolutepeaks[index];
		return 0;
	}

	virtual float GetLostFrameTimeMS() const { return 0.0f; }

	//! Get frame profiler at specified index.
	//! @param index must be 0 <= index < GetProfileCount()
	CFrameProfiler*  GetProfiler(int index) const;
	inline TThreadId GetMainThreadId() const
	{
		return m_ProfilerThreads.GetMainThreadId();
	}

	//////////////////////////////////////////////////////////////////////////
	// Sampling related.
	//////////////////////////////////////////////////////////////////////////
	void StartSampling();

	//////////////////////////////////////////////////////////////////////////
	// Adds a value to profiler.
	virtual void StartCustomSection(CCustomProfilerSection* pSection);
	virtual void EndCustomSection(CCustomProfilerSection* pSection);

	//////////////////////////////////////////////////////////////////////////
	// Peak callbacks.
	//////////////////////////////////////////////////////////////////////////
	virtual void AddPeaksListener(IFrameProfilePeakCallback* pPeakCallback);
	virtual void RemovePeaksListener(IFrameProfilePeakCallback* pPeakCallback);

	//////////////////////////////////////////////////////////////////////////
	//! Starts profiling a new section.
	static void StartProfilerSection(CFrameProfilerSection* pSection);
	//! Ends profiling a section.
	static void EndProfilerSection(CFrameProfilerSection* pSection);
	static void AccumulateProfilerSection(CFrameProfilerSection* pSection);

	static void StartMemoryProfilerSection(CFrameProfilerSection* pSection);
	static void EndMemoryProfilerSection(CFrameProfilerSection* pSection);

	//! Gets the bottom active section.
	virtual CFrameProfilerSection const* GetCurrentProfilerSection();

	//! Enable/Disable profile samples gathering.
	void         Enable(bool bCollect, bool bDisplay);
	void         SetSubsystemFilter(bool bFilterSubsystem, EProfiledSubsystem subsystem);
	bool         IsSubSystemFiltered(EProfiledSubsystem subsystem) const { return m_bSubsystemFilterEnabled && m_subsystemFilter != subsystem; }
	bool         IsSubSystemFiltered(CFrameProfiler* pProfiler) const    { return IsSubSystemFiltered((EProfiledSubsystem)pProfiler->m_subsystem); }
	void         EnableHistograms(bool bEnableHistograms);
	bool         IsEnabled() const   { return m_bEnabled; }
	virtual bool IsVisible() const   { return m_bDisplay; }
	bool         IsProfiling() const { return m_bCollect; }
	void         SetDisplayQuantity(EDisplayQuantity quantity);
	void         AddPeak(SPeakRecord& peak);

	void         SetSubsystemFilter(const char* sFilterName);
	void         SetSubsystemFilterThread(const char* sFilterThreadName);

	void         UpdateOfflineHistory(CFrameProfiler* pProfiler);

	//////////////////////////////////////////////////////////////////////////
	// Rendering.
	//////////////////////////////////////////////////////////////////////////
	void            Render();
	void            RenderMemoryInfo();
	void            RenderProfiler(CFrameProfiler* pProfiler, int level, float col, float row, bool bExtended, bool bSelected);
	void            RenderProfilerHeader(float col, float row, bool bExtended);
	void            RenderProfilers(float col, float row, bool bExtended);
	float           RenderPeaks();
	void            RenderSubSystems(float col, float row);
	void            RenderHistograms();
	void            CalcDisplayedProfilers();
	void            DrawGraph();
	void            DrawLabel(float raw, float column, float* fColor, float glow, const char* szText, float fScale = 1.0f);
	void            DrawRect(float x1, float y1, float x2, float y2, float* fColor);
	CFrameProfiler* GetSelectedProfiler();
	// Recursively add frame profiler and childs to displayed list.
	void            AddDisplayedProfiler(CFrameProfiler* pProfiler, int level);

	//////////////////////////////////////////////////////////////////////////
	float               TranslateToDisplayValue(int64 val);
	const char*         GetFullName(CFrameProfiler* pProfiler);
	virtual const char* GetModuleName(CFrameProfiler* pProfiler);
	const char*         GetModuleName(int num) const;
	const int           GetModuleCount(void) const;
	const float         GetOverBudgetRatio(int modulenumber) const;

	//////////////////////////////////////////////////////////////////////////
	// Performance stats logging
	//////////////////////////////////////////////////////////////////////////

protected:
	//inherited from IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& event);
	void         UpdateInputSystemStatus();
	const char*  GetProfilerThreadName(CFrameProfiler* pProfiler)  const;
};

#else

// Dummy Frame profile system interface.
struct CFrameProfileSystem : public IFrameProfileSystem
{
	//! Reset all profiling data.
	virtual void Reset() {};
	//! Add new frame profiler.
	//! Profile System will not delete those pointers, client must take care of memory managment issues.
	virtual void AddFrameProfiler(CFrameProfiler* pProfiler);
	//! Remove existing frame profiler.
	virtual void RemoveFrameProfiler(CFrameProfiler* pProfiler);
	//! Must be called at the start of the frame.
	virtual void StartFrame();
	//! Must be called at the end of the frame.
	virtual void EndFrame();
	//! Must be called when something quacks like the end of the frame.
	virtual void OnSliceAndSleep();

	//! Here the new methods needed to enable profiling to go off.

	virtual int                          GetPeaksCount(void) const  { return 0; }
	virtual const SPeakRecord*           GetPeak(int index) const   { return 0; }
	virtual int                          GetProfilerCount() const   { return 0; }

	virtual float                        GetLostFrameTimeMS() const { return 0.f; }

	virtual CFrameProfiler*              GetProfiler(int index) const;

	virtual void                         Enable(bool bCollect, bool bDisplay);

	virtual void                         SetSubsystemFilter(bool bFilterSubsystem, EProfiledSubsystem subsystem) {}
	virtual void                         SetSubsystemFilter(const char* sFilterName)                             {}
	virtual void                         SetSubsystemFilterThread(const char* sFilterThreadName)                 {}

	virtual bool                         IsEnabled() const                                                       { return 0; }

	virtual bool                         IsProfiling() const                                                     { return 0; }

	virtual void                         SetDisplayQuantity(EDisplayQuantity quantity)                           {}

	virtual void                         StartCustomSection(CCustomProfilerSection* pSection)                    {}
	virtual void                         EndCustomSection(CCustomProfilerSection* pSection)                      {}

	virtual void                         StartSampling(int)                                                      {}

	virtual void                         AddPeaksListener(IFrameProfilePeakCallback* pPeakCallback);
	virtual void                         RemovePeaksListener(IFrameProfilePeakCallback* pPeakCallback);

	virtual const char*                  GetFullName(CFrameProfiler* pProfiler)     { return 0; }
	virtual const int                    GetModuleCount(void) const                 { return 0; }
	virtual void                         SetPeakDisplayDuration(float duration)     {}
	virtual void                         SetAdditionalSubsystems(bool bEnabled)     {}
	virtual const float                  GetOverBudgetRatio(int modulenumber) const { return 0.0f; }

	void                                 Init()                   {}
	void                                 Done()                                     {}
	void                                 Render()                                   {}

	void                                 SetHistogramScale(float fScale)            {}
	void                                 SetDrawGraph(bool bDrawGraph)              {}
	void                                 SetNetworkProfiler(bool bNet)              {}
	void                                 SetPeakTolerance(float fPeakTimeMillis)    {}
	void                                 SetSmoothingTime(float fSmoothTime)        {}
	void                                 SetPageFaultsGraph(bool bEnabled)          {}
	void                                 SetThreadSupport(int)                      {}

	virtual bool                         IsVisible() const                          { return true; }
	virtual const CFrameProfilerSection* GetCurrentProfilerSection()                { return NULL; }

	virtual const char*                  GetModuleName(CFrameProfiler* pProfiler)   { return 0; }
	const char*                          GetModuleName(int num) const               { return 0; }

protected:
	//inherited from IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& event) { return false; }
	void         UpdateInputSystemStatus()              {}
};

#endif // USE_FRAME_PROFILER
