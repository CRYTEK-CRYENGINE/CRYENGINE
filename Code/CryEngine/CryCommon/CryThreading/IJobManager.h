// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IJobManager.h
//  Version:     v1.00
//  Created:     1/8/2011 by Christopher Bolte
//  Description: JobManager interface
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CryCore/Containers/CryArray.h>
#include <CryCore/smartptr.h>
#include <CryThreading/CryThread.h>

#if defined(ENABLE_PROFILING_CODE) && !CRY_PLATFORM_MOBILE
	//! Enable to obtain stats of job usage each frame. (Statoscope)
	#if ENABLE_STATOSCOPE
		#define JOBMANAGER_SUPPORT_STATOSCOPE
	#endif

	#if !defined(DEDICATED_SERVER)
		//! Collect per-job informations about dispatch, start, stop and sync times. (sys_job_system_profiler)
		#define JOBMANAGER_SUPPORT_PROFILING
	#endif
#endif

struct ILog;

//! Implementation of mutex/condition variable.
//! Used in the job manager for yield waiting in corner cases like waiting for a job to finish/jobqueue full and so on.
class SJobFinishedConditionVariable
{
public:
	SJobFinishedConditionVariable() :
		m_nRefCounter(0),
		m_pOwner(NULL)
	{
		m_nFinished = 1;
	}

	void Acquire()
	{
		//wait for completion of the condition
		m_Notify.Lock();
		while (m_nFinished == 0)
			m_CondNotify.Wait(m_Notify);
		m_Notify.Unlock();
	}

	void Release()
	{
		m_Notify.Lock();
		m_nFinished = 1;
		m_Notify.Unlock();
		m_CondNotify.Notify();
	}

	void SetRunning()
	{
		m_nFinished = 0;
	}

	bool IsRunning()
	{
		return m_nFinished == 0;
	}

	void SetStopped()
	{
		m_nFinished = 1;
	}

	void SetOwner(volatile const void* pOwner)
	{
		if (m_pOwner != NULL)
			__debugbreak();
		m_pOwner = pOwner;
	}

	void ClearOwner(volatile const void* pOwner)
	{
		if (m_pOwner != pOwner)
			__debugbreak();

		m_pOwner = NULL;
	}

	bool AddRef(volatile const void* pOwner)
	{
		if (m_pOwner != pOwner)
			return false;

		++m_nRefCounter;
		return true;
	}
	uint32 DecRef(volatile const void* pOwner)
	{
		if (m_pOwner != pOwner)
			__debugbreak();

		return --m_nRefCounter;
	}

	bool HasOwner() const { return m_pOwner != NULL; }

private:
	CryMutex             m_Notify;
	CryConditionVariable m_CondNotify;
	volatile uint32      m_nFinished;
	volatile uint32      m_nRefCounter;
	volatile const void* m_pOwner;
};

namespace JobManager {
class CJobBase;
}
namespace JobManager {
class CJobDelegator;
}
namespace JobManager {
namespace detail {
struct SJobQueueSlotState;
}
}

//! Wrapper for Vector4 uint.
struct CRY_ALIGN(16) SVEC4_UINT
{
#if !CRY_PLATFORM_SSE2
	uint32 v[4];
#else
	__m128 v;
#endif
};

// Global values.

//! Front end values.
namespace JobManager
{
//! Priority settings used for jobs.
enum TPriorityLevel
{
	eHighPriority     = 0,
	eRegularPriority  = 1,
	eLowPriority      = 2,
	eStreamPriority   = 3,
	eNumPriorityLevel = 4
};

//! Stores worker utilization. One instance per Worker.
//! One instance per cache line to avoid thread synchronization issues.
struct CRY_ALIGN(128) SWorkerStats
{
	unsigned int nExecutionPeriod;    //!< Accumulated job execution period in micro seconds.
	unsigned int nNumJobsExecuted;    //!< Number of jobs executed.
};

//! Type to reprensent a semaphore handle of the jobmanager.
typedef uint16 TSemaphoreHandle;

//! Magic value to reprensent an invalid job handle.
enum  : uint32
{
	INVALID_JOB_HANDLE = ((unsigned int)-1)
};

//! BackEnd Type.
enum EBackEndType
{
	eBET_Thread,
	eBET_Blocking
};

namespace Fiber
{
//! The alignment of the fibertask stack (currently set to 128 kb).
enum {FIBERTASK_ALIGNMENT = 128U << 10 };

//! The number of switches the fibertask records (default: 32).
enum {FIBERTASK_RECORD_SWITCHES = 32U };

} // namespace JobManager::Fiber

} // namespace JobManager

//! Structures used by the JobManager.
namespace JobManager
{
struct SJobStringHandle
{
	const char*  cpString;       //!< Points into repository, must be first element.
	unsigned int strLen;         //!< String length.
	int          jobId;          //!< Index (also acts as id) of job.
	uint32       nJobInvokerIdx; //!< Index of the jobInvoker (used to job switching in the prod/con queue).

	inline bool operator==(const SJobStringHandle& crOther) const;
	inline bool operator<(const SJobStringHandle& crOther) const;

};

//! Handle retrieved by name for job invocation.
typedef SJobStringHandle* TJobHandle;

bool SJobStringHandle::operator==(const SJobStringHandle& crOther) const
{
	return strcmp(cpString, crOther.cpString) == 0;
}

bool SJobStringHandle::operator<(const SJobStringHandle& crOther) const
{
	return strcmp(cpString, crOther.cpString) < 0;
}

//! Struct to collect profiling informations about job invocations and sync times.
struct CRY_ALIGN(16) SJobProfilingData
{
	CTimeValue startTime;
	CTimeValue endTime;

	TJobHandle jobHandle;
	threadID nThreadId;
	uint32 nWorkerThread;
	bool isWaiting;
};

struct SJobProfilingDataContainer
{
	enum {nCapturedFrames = 4};
	enum {nCapturedEntriesPerFrame = 4096 };
	uint32       nFrameIdx;
	uint32       nProfilingDataCounter[nCapturedFrames];

	ILINE uint32 GetFillFrameIdx()       { return nFrameIdx % nCapturedFrames; }
	ILINE uint32 GetPrevFrameIdx()       { return (nFrameIdx - 1) % nCapturedFrames; }
	ILINE uint32 GetRenderFrameIdx()     { return (nFrameIdx - 2) % nCapturedFrames; }
	ILINE uint32 GetPrevRenderFrameIdx() { return (nFrameIdx - 3) % nCapturedFrames; }

	CRY_ALIGN(128) SJobProfilingData m_DymmyProfilingData;
	SJobProfilingData arrJobProfilingData[nCapturedFrames][nCapturedEntriesPerFrame];
};

//! Special combination of a volatile spinning variable combined with a semaphore.
//! Used if the running state is not yet set to finish during waiting.
struct SJobSyncVariable
{
	SJobSyncVariable();

	bool IsRunning() const volatile;

	//! Interface, should only be used by the job manager or the job state classes.
	void Wait() volatile;
	bool NeedsToWait() volatile;
	void SetRunning(uint16 count = 1) volatile;
	bool SetStopped(struct SJobState* pPostCallback = nullptr, uint16 count = 1) volatile;

private:
	friend class CJobManager;

	//! Union used to combine the semaphore and the running state in a single word.
	union SyncVar
	{
		volatile uint32 wordValue;
		struct
		{
			uint16           nRunningCounter;
			TSemaphoreHandle semaphoreHandle;
		};
	};

	SyncVar syncVar;      //!< Sync-variable which contain the running state or the used semaphore.
	char    padding[4];
};

//! Condition variable-alike object to be used for polling if a job has been finished.
//! Has reference semantic, copies of the job state refer to the same object
struct SJobState
{
	SJobState()
		: m_pImpl(new SJobStateImpl())
	{}

	ILINE bool IsRunning() const { return m_pImpl->syncVar.IsRunning(); }

	ILINE void SetRunning(uint16 count = 1)
	{
		m_pImpl->syncVar.SetRunning(count);
	}

	bool SetStopped(uint16 count = 1)
	{
		return m_pImpl->syncVar.SetStopped(this, count);
	}

	void RunPostJob();

	template<class TJob>
	ILINE void RegisterPostJob(TJob&& postJobS);

	// Non blocking trying to stop state, and run post job.
	ILINE bool TryStopping()
	{
		if (IsRunning())
		{
			return SetStopped();
		}
		return true;
	}

	ILINE const bool Wait();

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	void SetProfilerIndex(uint16 index)
	{
		m_pImpl->nProfilerIndex = index;
	}

	uint16 GetProfilerIndex() const { return m_pImpl->nProfilerIndex; }
#endif

private:

	struct SJobStateImpl : public CMultiThreadRefCount
	{
		SJobSyncVariable syncVar;
		std::unique_ptr<CJobBase> m_pFollowUpJob;
		//! When profiling, intercept the SetRunning() and SetStopped() functions for profiling informations.
#if defined(JOBMANAGER_SUPPORT_PROFILING)
		uint16 nProfilerIndex = ~0;
#endif
	};

	friend class CJobManager;

	SJobSyncVariable& GetSyncVar()
	{
		return m_pImpl->syncVar;
	}

	_smart_ptr<SJobStateImpl> m_pImpl;
};

//! Stores worker utilization stats for a frame.
class CWorkerFrameStats
{
public:
	struct SWorkerStats
	{
		float  nUtilPerc;             //!< Utilization percentage this frame [0.f..100.f].
		uint32 nExecutionPeriod;      //!< Total execution time on this worker in usec.
		uint32 nNumJobsExecuted;      //!< Number of jobs executed contributing to nExecutionPeriod.
	};

public:
	ILINE CWorkerFrameStats(uint8 workerNum)
		: numWorkers(workerNum)
	{
		workerStats = new SWorkerStats[workerNum];
		Reset();
	}

	ILINE ~CWorkerFrameStats()
	{
		delete[] workerStats;
	}

	ILINE void Reset()
	{
		for (int i = 0; i < numWorkers; ++i)
		{
			workerStats[i].nUtilPerc = 0.f;
			workerStats[i].nExecutionPeriod = 0;
			workerStats[i].nNumJobsExecuted = 0;
		}

		nSamplePeriod = 0;
	}

public:
	uint8         numWorkers;
	uint32        nSamplePeriod;
	SWorkerStats* workerStats;
};

struct SWorkerFrameStatsSummary
{
	uint32 nSamplePeriod;             //!< Worker sample period.
	uint32 nTotalExecutionPeriod;     //!< Total execution time on this worker in usec.
	uint32 nTotalNumJobsExecuted;     //!< Total number of jobs executed contributing to nExecutionPeriod.
	float  nAvgUtilPerc;              //!< Average worker utilization [0.f ... 100.f].
	uint8  nNumActiveWorkers;         //!< Active number of workers for this frame.
};

//! Per frame and job specific.
struct CRY_ALIGN(16) SJobFrameStats
{
	unsigned int usec;                //!< Last frame's job time in microseconds.
	unsigned int count;               //!< Number of calls this frame.
	const char* cpName;               //!< Job name.

	unsigned int usecLast;            //!< Last but one frames Job time in microseconds (written and accumulated by Thread).

	inline SJobFrameStats();
	inline SJobFrameStats(const char* cpJobName);

	inline void Reset();
	inline void operator=(const SJobFrameStats& crFrom);
	inline bool operator<(const SJobFrameStats& crOther) const;

	static bool sort_lexical(const JobManager::SJobFrameStats& i, const JobManager::SJobFrameStats& j)
	{
		return strcmp(i.cpName, j.cpName) < 0;
	}

	static bool sort_time_high_to_low(const JobManager::SJobFrameStats& i, const JobManager::SJobFrameStats& j)
	{
		return i.usec > j.usec;
	}

	static bool sort_time_low_to_high(const JobManager::SJobFrameStats& i, const JobManager::SJobFrameStats& j)
	{
		return i.usec < j.usec;
	}
};

struct CRY_ALIGN(16) SJobFrameStatsSummary
{
	unsigned int nTotalExecutionTime;             //!< Total execution time of all jobs in microseconds.
	unsigned int nNumJobsExecuted;                //!< Total Number of job executions this frame.
	unsigned int nNumIndividualJobsExecuted;      //!< Total Number of individual jobs.
};

SJobFrameStats::SJobFrameStats(const char* cpJobName) : usec(0), count(0), cpName(cpJobName), usecLast(0)
{}

SJobFrameStats::SJobFrameStats() : usec(0), count(0), cpName("Uninitialized"), usecLast(0)
{}

void SJobFrameStats::operator=(const SJobFrameStats& crFrom)
{
	usec = crFrom.usec;
	count = crFrom.count;
	cpName = crFrom.cpName;
	usecLast = crFrom.usecLast;
}

void SJobFrameStats::Reset()
{
	usecLast = usec;
	usec = count = 0;
}

bool SJobFrameStats::operator<(const SJobFrameStats& crOther) const
{
	//sort from large to small
	return usec > crOther.usec;
}

//! Delegator function.
//! Takes a pointer to a params structure and does the decomposition of the parameters, then calls the Job entry function.
typedef void (* Invoker)(void*);

//! Info block transferred first for each job.
//! We have to transfer the info block, parameter block, input memory needed for execution.
struct CRY_ALIGN(128) SInfoBlock
{
	//! Struct to represent the state of a SInfoBlock.
	struct SInfoBlockState
	{
		//! Union of requiered members and uin32 to allow easy atomic operations.
		union
		{
			struct
			{
				TSemaphoreHandle nSemaphoreHandle;    //!< Handle for the conditon variable to use in case some thread is waiting.
				uint16           nRoundID;            //!< Number of rounds over the job queue, to prevent a race condition with SInfoBlock reuse (we have 15 bits, thus 32k full job queue iterations are safe).
			};
			volatile LONG nValue;                     //!< value for easy Compare and Swap.
		};

		void IsInUse(uint32 nRoundIdToCheckOrg, bool& rWait, bool& rRetry, uint32 nMaxValue) volatile const
		{
			uint32 nRoundIdToCheck = nRoundIdToCheckOrg & 0x7FFF;   //!< Clamp nRoundID to 15 bit.
			uint32 nLoadedRoundID = nRoundID;
			uint32 nNextLoadedRoundID = ((nLoadedRoundID + 1) & 0x7FFF);
			nNextLoadedRoundID = nNextLoadedRoundID >= nMaxValue ? 0 : nNextLoadedRoundID;

			if (nLoadedRoundID == nRoundIdToCheck)  // job slot free
			{
				rWait = false;
				rRetry = false;
			}
			else if (nNextLoadedRoundID == nRoundIdToCheck)     // job slot need to be worked on by worker
			{
				rWait = true;
				rRetry = false;
			}
			else   // producer was suspended long enough that the worker overtook it
			{
				rWait = false;
				rRetry = true;
			}
		}
	};

	//! data needed to run the job and it's functionality.
	volatile SInfoBlockState jobState;             //!< State of the SInfoBlock in job queue, should only be modified by access functions.
	Invoker jobInvoker;                            //!< Callback function to job invoker (extracts parameters and calls entry function).
	JobManager::SJobState* pJobState;              //!< External job state address
	JobManager::SInfoBlock* pNext;                 //!< Single linked list for fallback jobs in the case the queue is full, and a worker wants to push new work.

	// Per-job settings like cache size and so on.
	unsigned char frameProfIndex;                  //!< Index of SJobFrameStats*.
	unsigned char nflags;
	unsigned char paramSize;                       //!< Size in total of parameter block in 16 byte units.
	unsigned char jobId;                           //!< Corresponding job ID, needs to track jobs.

	// We could also use a union, but this solution is (hopefully) clearer.
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	unsigned short profilerIndex;                  //!< index for the job system profiler.
#endif

	//! Size of the SInfoBlock struct and how much memory we have to store parameters.
	static const unsigned int scSizeOfSJobQueueEntry = 512;
	static const unsigned int scSizeOfJobQueueEntryHeader = 64;   //!< Please adjust when adding/removing members, keep as a multiple of 16.
	static const unsigned int scAvailParamSize = scSizeOfSJobQueueEntry - scSizeOfJobQueueEntryHeader;

	//! Parameter data are enclosed within to save a cache miss.
	CRY_ALIGN(16) unsigned char paramData[scAvailParamSize];    //!< is 16 byte aligned, make sure it is kept aligned.

	ILINE void AssignMembersTo(SInfoBlock* pDest) const
	{
		pDest->jobInvoker = jobInvoker;
		pDest->pJobState = pJobState;
		pDest->pNext = pNext;

		pDest->frameProfIndex = frameProfIndex;
		pDest->nflags = nflags;
		pDest->paramSize = paramSize;
		pDest->jobId = jobId;

#if defined(JOBMANAGER_SUPPORT_PROFILING)
		pDest->profilerIndex = profilerIndex;
#endif

		memcpy(pDest->paramData, paramData, sizeof(paramData));
	}

	ILINE void SetJobState(JobManager::SJobState* _pJobState)
	{
		pJobState = _pJobState;
	}

	ILINE JobManager::SJobState* GetJobState() const
	{
		return pJobState;
	}

	ILINE unsigned char* const __restrict GetParamAddress()
	{
		return &paramData[0];
	}

	//! Functions to access jobstate of SInfoBlock.
	ILINE void IsInUse(uint32 nRoundId, bool& rWait, bool& rRetry, uint32 nMaxValue) const { return jobState.IsInUse(nRoundId, rWait, rRetry, nMaxValue); }

	void Release(uint32 nMaxValue);
	void Wait(uint32 nRoundID, uint32 nMaxValue);

};

//! State information for the fill and empty pointers of the job queue.
struct CRY_ALIGN(16) SJobQueuePos
{
	//! Base of job queue per priority level.
	JobManager::SInfoBlock* jobQueue[JobManager::eNumPriorityLevel];

	//! Base of job queue per priority level.
	JobManager::detail::SJobQueueSlotState* jobQueueStates[JobManager::eNumPriorityLevel];

	//! Index use for job slot publishing each priority level is encoded in this single value.
	volatile uint64 index;

	//! Util function to increase counter for their priority level only.
	static const uint64 eBitsInIndex = sizeof(uint64) * CHAR_BIT;
	static const uint64 eBitsPerPriorityLevel = eBitsInIndex / JobManager::eNumPriorityLevel;
	static const uint64 eMaskStreamPriority = (1ULL << eBitsPerPriorityLevel) - 1ULL;
	static const uint64 eMaskLowPriority = (((1ULL << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) - 1ULL) & ~eMaskStreamPriority;
	static const uint64 eMaskRegularPriority = ((((1ULL << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) - 1ULL) & ~(eMaskStreamPriority | eMaskLowPriority);
	static const uint64 eMaskHighPriority = (((((1ULL << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) << eBitsPerPriorityLevel) - 1ULL) & ~(eMaskRegularPriority | eMaskStreamPriority | eMaskLowPriority);

	ILINE static uint64 ExtractIndex(uint64 currentIndex, uint32 nPriorityLevel)
	{
		switch (nPriorityLevel)
		{
		case eHighPriority:
			return ((((currentIndex & eMaskHighPriority) >> eBitsPerPriorityLevel) >> eBitsPerPriorityLevel) >> eBitsPerPriorityLevel);
		case eRegularPriority:
			return (((currentIndex & eMaskRegularPriority) >> eBitsPerPriorityLevel) >> eBitsPerPriorityLevel);
		case eLowPriority:
			return ((currentIndex & eMaskLowPriority) >> eBitsPerPriorityLevel);
		case eStreamPriority:
			return (currentIndex & eMaskStreamPriority);
		default:
			return ~0;
		}
	}

	ILINE static bool IncreasePullIndex(uint64 currentPullIndex, uint64 currentPushIndex, uint64& rNewPullIndex, uint32& rPriorityLevel,
	                                    uint32 nJobQueueSizeHighPriority, uint32 nJobQueueSizeRegularPriority, uint32 nJobQueueSizeLowPriority, uint32 nJobQueueSizeStreamPriority)
	{
		uint32 nPushExtractedHighPriority = static_cast<uint32>(ExtractIndex(currentPushIndex, eHighPriority));
		uint32 nPushExtractedRegularPriority = static_cast<uint32>(ExtractIndex(currentPushIndex, eRegularPriority));
		uint32 nPushExtractedLowPriority = static_cast<uint32>(ExtractIndex(currentPushIndex, eLowPriority));
		uint32 nPushExtractedStreamPriority = static_cast<uint32>(ExtractIndex(currentPushIndex, eStreamPriority));

		uint32 nPullExtractedHighPriority = static_cast<uint32>(ExtractIndex(currentPullIndex, eHighPriority));
		uint32 nPullExtractedRegularPriority = static_cast<uint32>(ExtractIndex(currentPullIndex, eRegularPriority));
		uint32 nPullExtractedLowPriority = static_cast<uint32>(ExtractIndex(currentPullIndex, eLowPriority));
		uint32 nPullExtractedStreamPriority = static_cast<uint32>(ExtractIndex(currentPullIndex, eStreamPriority));

		uint32 nRoundPushHighPriority = nPushExtractedHighPriority / nJobQueueSizeHighPriority;
		uint32 nRoundPushRegularPriority = nPushExtractedRegularPriority / nJobQueueSizeRegularPriority;
		uint32 nRoundPushLowPriority = nPushExtractedLowPriority / nJobQueueSizeLowPriority;
		uint32 nRoundPushStreamPriority = nPushExtractedStreamPriority / nJobQueueSizeStreamPriority;

		uint32 nRoundPullHighPriority = nPullExtractedHighPriority / nJobQueueSizeHighPriority;
		uint32 nRoundPullRegularPriority = nPullExtractedRegularPriority / nJobQueueSizeRegularPriority;
		uint32 nRoundPullLowPriority = nPullExtractedLowPriority / nJobQueueSizeLowPriority;
		uint32 nRoundPullStreamPriority = nPullExtractedStreamPriority / nJobQueueSizeStreamPriority;

		uint32 nPushJobSlotHighPriority = nPushExtractedHighPriority & (nJobQueueSizeHighPriority - 1);
		uint32 nPushJobSlotRegularPriority = nPushExtractedRegularPriority & (nJobQueueSizeRegularPriority - 1);
		uint32 nPushJobSlotLowPriority = nPushExtractedLowPriority & (nJobQueueSizeLowPriority - 1);
		uint32 nPushJobSlotStreamPriority = nPushExtractedStreamPriority & (nJobQueueSizeStreamPriority - 1);

		uint32 nPullJobSlotHighPriority = nPullExtractedHighPriority & (nJobQueueSizeHighPriority - 1);
		uint32 nPullJobSlotRegularPriority = nPullExtractedRegularPriority & (nJobQueueSizeRegularPriority - 1);
		uint32 nPullJobSlotLowPriority = nPullExtractedLowPriority & (nJobQueueSizeLowPriority - 1);
		uint32 nPullJobSlotStreamPriority = nPullExtractedStreamPriority & (nJobQueueSizeStreamPriority - 1);

		//NOTE: if this shows in the profiler, find a lockless variant of it
		if (((nRoundPushHighPriority == nRoundPullHighPriority) && nPullJobSlotHighPriority < nPushJobSlotHighPriority) ||
		    ((nRoundPushHighPriority != nRoundPullHighPriority) && nPullJobSlotHighPriority >= nPushJobSlotHighPriority))
		{
			rPriorityLevel = eHighPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eHighPriority);
		}
		else if (((nRoundPushRegularPriority == nRoundPullRegularPriority) && nPullJobSlotRegularPriority < nPushJobSlotRegularPriority) ||
		         ((nRoundPushRegularPriority != nRoundPullRegularPriority) && nPullJobSlotRegularPriority >= nPushJobSlotRegularPriority))
		{
			rPriorityLevel = eRegularPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eRegularPriority);
		}
		else if (((nRoundPushLowPriority == nRoundPullLowPriority) && nPullJobSlotLowPriority < nPushJobSlotLowPriority) ||
		         ((nRoundPushLowPriority != nRoundPullLowPriority) && nPullJobSlotLowPriority >= nPushJobSlotLowPriority))
		{
			rPriorityLevel = eLowPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eLowPriority);
		}
		else if (((nRoundPushStreamPriority == nRoundPullStreamPriority) && nPullJobSlotStreamPriority < nPushJobSlotStreamPriority) ||
		         ((nRoundPushStreamPriority != nRoundPullStreamPriority) && nPullJobSlotStreamPriority >= nPushJobSlotStreamPriority))
		{
			rPriorityLevel = eStreamPriority;
			rNewPullIndex = IncreaseIndex(currentPullIndex, eStreamPriority);
		}
		else
		{
			rNewPullIndex = ~0;
			return false;
		}

		return true;
	}

	ILINE static uint64 IncreasePushIndex(uint64 currentPushIndex, uint32 nPriorityLevel)
	{
		return IncreaseIndex(currentPushIndex, nPriorityLevel);
	}

	ILINE static uint64 IncreaseIndex(uint64 currentIndex, uint32 nPriorityLevel)
	{
		uint64 nIncrease = 1ULL;
		uint64 nMask = 0;
		switch (nPriorityLevel)
		{
		case eHighPriority:
			nIncrease <<= eBitsPerPriorityLevel;
			nIncrease <<= eBitsPerPriorityLevel;
			nIncrease <<= eBitsPerPriorityLevel;
			nMask = eMaskHighPriority;
			break;
		case eRegularPriority:
			nIncrease <<= eBitsPerPriorityLevel;
			nIncrease <<= eBitsPerPriorityLevel;
			nMask = eMaskRegularPriority;
			break;
		case eLowPriority:
			nIncrease <<= eBitsPerPriorityLevel;
			nMask = eMaskLowPriority;
			break;
		case eStreamPriority:
			nMask = eMaskStreamPriority;
			break;
		}

		// increase counter while preventing overflow
		uint64 nCurrentValue = currentIndex & nMask;                        // extract all bits for this priority only
		uint64 nCurrentValueCleared = currentIndex & ~nMask;                // extract all bits of other priorities (they shouldn't change)
		nCurrentValue += nIncrease;                                         // increase value by one (already at the right bit position)
		nCurrentValue &= nMask;                                             // mask out again to handle overflow
		uint64 nNewCurrentValue = nCurrentValueCleared | nCurrentValue;     // add new value to other priorities
		return nNewCurrentValue;
	}

};

//! Struct covers DMA data setup common to all jobs and packets.
class CCommonDMABase
{
public:
	CCommonDMABase();
	void        SetJobParamData(void* pParamData);
	const void* GetJobParamData() const;
	uint16      GetProfilingDataIndex() const;

	//! In case of persistent job objects, we have to reset the profiling data
	void ForceUpdateOfProfilingDataIndex();

protected:
	void*  m_pParamData;      //!< parameter struct, not initialized to 0 for performance reason.
	uint16 nProfilerIndex;    //!< index handle for Jobmanager Profiling Data.
};

//! Delegation class for each job.
class CJobDelegator : public CCommonDMABase
{
public:

	typedef volatile CJobBase* TDepJob;

	CJobDelegator();
	void RunJob(const JobManager::TJobHandle cJobHandle);

	void RegisterJobState(JobManager::SJobState* __restrict pJobState);

	//return a copy since it will get overwritten
	void                   SetParamDataSize(const unsigned int cParamDataSize);
	const unsigned int     GetParamDataSize() const;
	void                   SetCurrentThreadId(const threadID cID);
	const threadID         GetCurrentThreadId() const;

	JobManager::SJobState* GetJobState() const;
	void                   SetRunning();

	ILINE void             SetDelegator(Invoker pGenericDelecator)
	{
		m_pGenericDelecator = pGenericDelecator;
	}

	Invoker GetGenericDelegator()
	{
		return m_pGenericDelecator;
	}

	unsigned int GetPriorityLevel() const                      { return m_nPrioritylevel; }
	bool         IsBlocking() const                            { return m_bIsBlocking; };

	void         SetPriorityLevel(unsigned int nPrioritylevel) { m_nPrioritylevel = nPrioritylevel; }
	void         SetBlocking()                                 { m_bIsBlocking = true; }

protected:
	JobManager::SJobState* m_pJobState;                     //!< Extern job state.
	unsigned int           m_nPrioritylevel;                //!< Enum represent the priority level to use.
	bool                   m_bIsBlocking;                   //!< If true, then the job could block on a mutex/sleep.
	unsigned int           m_ParamDataSize;                 //!< Sizeof parameter struct.
	threadID               m_CurThreadID;                   //!< Current thread id.
	Invoker                m_pGenericDelecator;
};

//! Base class for jobs.
class CJobBase
{
public:
	virtual ~CJobBase() = default;

	ILINE CJobBase() : m_pJobProgramData(NULL)
	{
	}

	ILINE void RegisterJobState(JobManager::SJobState* __restrict pJobState) volatile
	{
		((CJobBase*)this)->m_JobDelegator.RegisterJobState(pJobState);
	}

	ILINE void Run(JobManager::SJobState* __restrict pJobState)
	{
		RegisterJobState(pJobState);
		Run();
	}

	ILINE void Run()
	{
		m_JobDelegator.RunJob(m_pJobProgramData);
	}

	ILINE const JobManager::TJobHandle GetJobProgramData()
	{
		return m_pJobProgramData;
	}

	ILINE void SetCurrentThreadId(const threadID cID)
	{
		m_JobDelegator.SetCurrentThreadId(cID);
	}

	ILINE const threadID GetCurrentThreadId() const
	{
		return m_JobDelegator.GetCurrentThreadId();
	}

	ILINE CJobDelegator* GetJobDelegator()
	{
		return &m_JobDelegator;
	}

protected:
	CJobDelegator          m_JobDelegator;        //!< Delegation implementation, all calls to job manager are going through it.
	JobManager::TJobHandle m_pJobProgramData;     //!< Handle to program data to run.

	//! Set the job program data.
	ILINE void SetJobProgramData(const JobManager::TJobHandle pJobProgramData)
	{
		assert(pJobProgramData);
		m_pJobProgramData = pJobProgramData;
	}

};
} // namespace JobManager

// Interface of the JobManager.

//! Declaration for the InvokeOnLinkedStacked util function.
extern "C"
{
	void InvokeOnLinkedStack(void (* proc)(void*), void* arg, void* stack, size_t stackSize);
} // extern "C"

namespace JobManager
{
class IWorkerBackEndProfiler;

//! Base class for the various backends the jobmanager supports.
struct IBackend
{
	// <interfuscator:shuffle>
	virtual ~IBackend(){}

	virtual bool   Init(uint32 nSysMaxWorker) = 0;
	virtual bool   ShutDown() = 0;
	virtual void   Update() = 0;

	virtual void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock) = 0;
	virtual uint32 GetNumWorkerThreads() const = 0;

	virtual bool KickTempWorker() = 0;
	virtual bool StopTempWorker() = 0;
	// </interfuscator:shuffle>

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	virtual IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const = 0;
#endif
};

//! Singleton managing the job queues.
struct IJobManager
{
	// <interfuscator:shuffle>
	// === front end interface ===
	virtual ~IJobManager(){}

	virtual void Init(uint32 nSysMaxWorker) = 0;

	//! Add a job.
	virtual void AddJob(JobManager::CJobDelegator& RESTRICT_REFERENCE crJob, const JobManager::TJobHandle cJobHandle) = 0;

	//! Add a job as a lambda callback.
	template<typename Callback>
	void AddLambdaJob(const char* jobName, Callback&& lambdaCallback, TPriorityLevel priority = JobManager::eRegularPriority, SJobState* pJobState = nullptr);

	//! Wait for a job, preempt the calling thread if the job is not done yet.
	virtual const bool WaitForJob(JobManager::SJobState& rJobState) const = 0;

	//! Obtain job handle from name.
	virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, const unsigned int cStrLen, JobManager::Invoker pInvoker) = 0;

	//! Obtain job handle from name.
	virtual const JobManager::TJobHandle GetJobHandle(const char* cpJobName, JobManager::Invoker pInvoker) = 0;

	//! Shut down the job manager.
	virtual void                           ShutDown() = 0;

	virtual JobManager::IBackend*          GetBackEnd(JobManager::EBackEndType backEndType) = 0;

	virtual bool                           InvokeAsJob(const char* cJobHandle) const = 0;
	virtual bool                           InvokeAsJob(const JobManager::TJobHandle cJobHandle) const = 0;

	virtual void                           SetJobFilter(const char* pFilter) = 0;
	virtual void                           SetJobSystemEnabled(int nEnable) = 0;

	virtual uint32                         GetWorkerThreadId() const = 0;

	virtual JobManager::SJobProfilingData* GetProfilingData(uint16 nProfilerIndex) = 0;
	virtual uint16                         ReserveProfilingData() = 0;

	virtual void                           Update(int nJobSystemProfiler) = 0;

	virtual uint32                         GetNumWorkerThreads() const = 0;

	//! Get a free semaphore handle from the Job Manager pool.
	virtual JobManager::TSemaphoreHandle AllocateSemaphore(volatile const void* pOwner) = 0;

	//! Return a semaphore handle to the Job Manager pool.
	virtual void DeallocateSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) = 0;

	//! Increase the refcounter of a semaphore, but only if it is > 0, else returns false.
	virtual bool AddRefSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) = 0;

	//! Get the actual semaphore object for aquire/release calls.
	virtual SJobFinishedConditionVariable* GetSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner) = 0;

	virtual void                           DumpJobList() = 0;

	virtual void                           SetFrameStartTime(const CTimeValue& rFrameStartTime) = 0;
	virtual void                           SetMainDoneTime(const CTimeValue &) = 0;
	virtual void                           SetRenderDoneTime(const CTimeValue &) = 0;
};

static constexpr uint32 s_nonWorkerThreadId = -1;
static constexpr uint32 s_blockingWorkerFlag = 0x40000000;

//! Utility function to get the worker thread id in a job, returns 0xFFFFFFFF otherwise.
ILINE uint32 GetWorkerThreadId()
{
	uint32 nWorkerThreadID = gEnv->GetJobManager()->GetWorkerThreadId();
	return nWorkerThreadID == s_nonWorkerThreadId ? s_nonWorkerThreadId : (nWorkerThreadID & ~s_blockingWorkerFlag);
}

//! Utility function to find out if a call comes from the mainthread or from a worker thread.
ILINE bool IsWorkerThread()
{
	return gEnv->GetJobManager()->GetWorkerThreadId() != s_nonWorkerThreadId;
}

//! Utility function to find out if a call comes from the mainthread or from a blocking worker thread.
ILINE bool IsBlockingWorkerThread()
{
	return IsWorkerThread() && ((gEnv->GetJobManager()->GetWorkerThreadId() & s_blockingWorkerFlag) != 0);
}

//! Utility function to check if a specific job should really run as job.
ILINE bool InvokeAsJob(const char* pJobName)
{
#if defined(_RELEASE)
	return true;      // In release mode, always assume that the job should be executed.
#else
	return gEnv->pJobManager->InvokeAsJob(pJobName);
#endif
}

//! Utility functions namespace for the producer consumer queue.
namespace ProdConsQueueBase
{
inline INT_PTR MarkPullPtrThatWeAreWaitingOnASemaphore(INT_PTR nOrgValue) { assert((nOrgValue & 1) == 0); return nOrgValue | 1; }
inline bool    IsPullPtrMarked(INT_PTR nOrgValue)                         { return (nOrgValue & 1) == 1; }
}

//! Interface to track BackEnd worker utilisation and job execution timings.
class IWorkerBackEndProfiler
{
public:
	enum EJobSortOrder
	{
		eJobSortOrder_NoSort,
		eJobSortOrder_TimeHighToLow,
		eJobSortOrder_TimeLowToHigh,
		eJobSortOrder_Lexical,
	};

public:
	typedef DynArray<JobManager::SJobFrameStats> TJobFrameStatsContainer;

public:
	virtual ~IWorkerBackEndProfiler(){; }

	virtual void Init(const uint16 numWorkers) = 0;

	//! Update the profiler at the beginning of the sample period.
	virtual void Update() = 0;
	virtual void Update(const uint32 curTimeSample) = 0;

	//! Register a job with the profiler.
	virtual void RegisterJob(const uint32 jobId, const char* jobName) = 0;

	//! Record execution information for a registered job.
	virtual void RecordJob(const uint16 profileIndex, const uint8 workerId, const uint32 jobId, const uint32 runTimeMicroSec) = 0;

	//! Get worker frame stats.
	virtual void GetFrameStats(JobManager::CWorkerFrameStats& rStats) const = 0;
	virtual void GetFrameStats(TJobFrameStatsContainer& rJobStats, EJobSortOrder jobSortOrder) const = 0;
	virtual void GetFrameStats(JobManager::CWorkerFrameStats& rStats, TJobFrameStatsContainer& rJobStats, EJobSortOrder jobSortOrder) const = 0;

	//! Get worker frame stats summary.
	virtual void GetFrameStatsSummary(SWorkerFrameStatsSummary& rStats) const = 0;
	virtual void GetFrameStatsSummary(SJobFrameStatsSummary& rStats) const = 0;

	//! Returns the index of the active multi-buffered profile data.
	virtual uint16 GetProfileIndex() const = 0;

	//! Get the number of workers tracked.
	virtual uint32 GetNumWorkers() const = 0;

public:
	//! Returns a microsecond sample.
	static uint32 GetTimeSample()
	{
		return static_cast<uint32>(gEnv->pTimer->GetAsyncTime().GetMicroSecondsAsInt64());
	}
};

}// namespace JobManager

extern "C"
{
	//! Interface to create the JobManager.
	//! Needed here for calls from Producer Consumer queue.
	DLL_EXPORT JobManager::IJobManager* GetJobManagerInterface();
}

//! CCommonDMABase Function implementations.
inline JobManager::CCommonDMABase::CCommonDMABase()
{
	ForceUpdateOfProfilingDataIndex();
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CCommonDMABase::ForceUpdateOfProfilingDataIndex()
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	nProfilerIndex = gEnv->GetJobManager()->ReserveProfilingData();
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void JobManager::CCommonDMABase::SetJobParamData(void* pParamData)
{
	m_pParamData = pParamData;
}

///////////////////////////////////////////////////////////////////////////////
inline const void* JobManager::CCommonDMABase::GetJobParamData() const
{
	return m_pParamData;
}

///////////////////////////////////////////////////////////////////////////////
inline uint16 JobManager::CCommonDMABase::GetProfilingDataIndex() const
{
	return nProfilerIndex;
}

//! Job Delegator Function implementations.
ILINE JobManager::CJobDelegator::CJobDelegator() : m_ParamDataSize(0), m_CurThreadID(THREADID_NULL)
{
	m_pJobState = NULL;
	m_nPrioritylevel = JobManager::eRegularPriority;
	m_bIsBlocking = false;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::RunJob(const JobManager::TJobHandle cJobHandle)
{
	gEnv->GetJobManager()->AddJob(*static_cast<CJobDelegator*>(this), cJobHandle);
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::RegisterJobState(JobManager::SJobState* __restrict pJobState)
{
	assert(pJobState);
	m_pJobState = pJobState;
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	pJobState->SetProfilerIndex(this->GetProfilingDataIndex());
#endif
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::SetParamDataSize(const unsigned int cParamDataSize)
{
	m_ParamDataSize = cParamDataSize;
}

///////////////////////////////////////////////////////////////////////////////
ILINE const unsigned int JobManager::CJobDelegator::GetParamDataSize() const
{
	return m_ParamDataSize;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::SetCurrentThreadId(const threadID cID)
{
	m_CurThreadID = cID;
}

///////////////////////////////////////////////////////////////////////////////
ILINE const threadID JobManager::CJobDelegator::GetCurrentThreadId() const
{
	return m_CurThreadID;
}

///////////////////////////////////////////////////////////////////////////////
ILINE JobManager::SJobState* JobManager::CJobDelegator::GetJobState() const
{
	return m_pJobState;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void JobManager::CJobDelegator::SetRunning()
{
	m_pJobState->SetRunning();
}

//! Implementation of SJObSyncVariable functions.
inline JobManager::SJobSyncVariable::SJobSyncVariable()
{
	syncVar.wordValue = 0;
	padding[0] = padding[1] = padding[2] = padding[3] = 0;
}

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::IsRunning() const volatile
{
	return syncVar.nRunningCounter != 0;
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobSyncVariable::Wait() volatile
{
	if (!NeedsToWait())
		return;

	IJobManager* pJobManager = gEnv->GetJobManager();

	SyncVar currentValue;
	SyncVar newValue;
	SyncVar resValue;
	TSemaphoreHandle semaphoreHandle = gEnv->GetJobManager()->AllocateSemaphore(this);

retry:
	// volatile read
	currentValue.wordValue = syncVar.wordValue;

	if (currentValue.wordValue != 0)
	{
		// there is already a semaphore, wait for this one
		if (currentValue.semaphoreHandle)
		{
			// prevent the following race condition: Thread A Gets the semaphore and waits, then Thread B fails the earlier C-A-S
			// thus will wait for the same semaphore, but Thread A already release the semaphore and now another Thread (could even be A again)
			// is using the same semaphore to wait for the job (executed by thread B) which waits for the samephore -> Deadlock
			// this works since the jobmanager semphore function internally are using locks
			// so if the following line succeeds, we got the lock before the release and have increased the use counter
			// if not, it means that thread A release the semaphore, which also means thread B doesn't have to wait anymore

			if (pJobManager->AddRefSemaphore(currentValue.semaphoreHandle, this))
			{
				pJobManager->GetSemaphore(currentValue.semaphoreHandle, this)->Acquire();
				pJobManager->DeallocateSemaphore(currentValue.semaphoreHandle, this);
			}
		}
		else // no semaphore found
		{
			newValue = currentValue;
			newValue.semaphoreHandle = semaphoreHandle;
			resValue.wordValue = CryInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue);

			// four case are now possible:
			//a) job has finished -> we only need to free our semaphore
			//b) we succeeded setting our semaphore, thus wait for it
			//c) another waiter has set it's sempahore wait for the other one
			//d) another thread has increased/decreased the num runner variable, we need to do the run again

			if (resValue.wordValue != 0) // case a), do nothing
			{
				if (resValue.wordValue == currentValue.wordValue) // case b)
				{
					pJobManager->GetSemaphore(semaphoreHandle, this)->Acquire();
				}
				else // C-A-S not succeeded, check how to proceed
				{
					if (resValue.semaphoreHandle) // case c)
					{
						// prevent the following race condition: Thread A Gets the semaphore and waits, then Thread B fails the earlier C-A-S
						// thus will wait for the same semaphore, but Thread A already release the semaphore and now another Thread (could even be A again)
						// is using the same semaphore to wait for the job (executed by thread B) which waits for the samephore -> Deadlock
						// this works since the jobmanager semphore function internally are using locks
						// so if the following line succeeds, we got the lock before the release and have increased the use counter
						// if not, it means that thread A release the semaphore, which also means thread B doesn't have to wait anymore

						if (pJobManager->AddRefSemaphore(resValue.semaphoreHandle, this))
						{
							pJobManager->GetSemaphore(resValue.semaphoreHandle, this)->Acquire();
							pJobManager->DeallocateSemaphore(resValue.semaphoreHandle, this);
						}
					}
					else // case d
					{
						goto retry;
					}
				}
			}
		}
	}

	// mark an old semaphore as stopped, in case we didn't use use	(if we did use it, this is a nop)
	pJobManager->GetSemaphore(semaphoreHandle, this)->SetStopped();
	pJobManager->DeallocateSemaphore(semaphoreHandle, this);
}

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::NeedsToWait() volatile
{
	return syncVar.wordValue != 0;
}
/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobSyncVariable::SetRunning(uint16 count) volatile
{
	SyncVar currentValue;
	SyncVar newValue;

	do
	{
		// volatile read
		currentValue.wordValue = syncVar.wordValue;

		newValue = currentValue;
		newValue.nRunningCounter += count;

		if (newValue.nRunningCounter == 0)
		{
			CRY_ASSERT_MESSAGE(0, "JobManager: Atomic counter overflow");
		}
	}
	while (CryInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue) != currentValue.wordValue);
}

/////////////////////////////////////////////////////////////////////////////////
inline bool JobManager::SJobSyncVariable::SetStopped(SJobState* pPostCallback, uint16 count) volatile
{
	SyncVar currentValue;
	SyncVar newValue;
	SyncVar resValue;

	// first use atomic operations to decrease the running counter
	do
	{
		// volatile read
		currentValue.wordValue = syncVar.wordValue;

		if (currentValue.nRunningCounter == 0)
		{
			CRY_ASSERT_MESSAGE(0, "JobManager: Atomic counter underflow");
			newValue.nRunningCounter = 1; // Force for potential stability problem, should not happen.
		}

		newValue = currentValue;
		newValue.nRunningCounter -= count;

		resValue.wordValue = CryInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue);
	}
	while (resValue.wordValue != currentValue.wordValue);

	// we now successfully decreased our counter, check if we need to signal a semaphore
	if (newValue.nRunningCounter > 0) // return if we still have other jobs on this syncvar
		return false;

	// Post job needs to be added before releasing semaphore of the current job, to allow chain of post jobs to be waited on.
	if (pPostCallback)
		pPostCallback->RunPostJob();

	//! Do we need to release a semaphore?
	if (currentValue.semaphoreHandle)
	{
		// try to set the running counter to 0 atomically
		do
		{
			// volatile read
			currentValue.wordValue = syncVar.wordValue;
			newValue = currentValue;
			newValue.semaphoreHandle = 0;

			// another thread increased the running counter again, don't call semaphore release in this case
			if (currentValue.nRunningCounter)
				return false;

		}
		while (CryInterlockedCompareExchange((volatile LONG*)&syncVar.wordValue, newValue.wordValue, currentValue.wordValue) != currentValue.wordValue);
		// set the running successful to 0, now we can release the semaphore
		gEnv->GetJobManager()->GetSemaphore(resValue.semaphoreHandle, this)->Release();
	}

	return true;
}

// SInfoBlock State functions.

inline void JobManager::SInfoBlock::Release(uint32 nMaxValue)
{
	SInfoBlockState currentInfoBlockState;
	SInfoBlockState newInfoBlockState;
	SInfoBlockState resultInfoBlockState;

	// use atomic operations to set the running flag
	do
	{
		// volatile read
		currentInfoBlockState.nValue = *(const_cast<volatile LONG*>(&jobState.nValue));

		newInfoBlockState.nSemaphoreHandle = 0;
		newInfoBlockState.nRoundID = (currentInfoBlockState.nRoundID + 1) & 0x7FFF;
		newInfoBlockState.nRoundID = newInfoBlockState.nRoundID >= nMaxValue ? 0 : newInfoBlockState.nRoundID;

		resultInfoBlockState.nValue = CryInterlockedCompareExchange((volatile LONG*)&jobState.nValue, newInfoBlockState.nValue, currentInfoBlockState.nValue);
	}
	while (resultInfoBlockState.nValue != currentInfoBlockState.nValue);

	// Release semaphore
	// Since this is a copy of the state when we succeeded the CAS it is ok to it after original jobState was returned to the free list.
	if (currentInfoBlockState.nSemaphoreHandle)
		gEnv->GetJobManager()->GetSemaphore(currentInfoBlockState.nSemaphoreHandle, this)->Release();
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SInfoBlock::Wait(uint32 nRoundID, uint32 nMaxValue)
{
	bool bWait = false;
	bool bRetry = false;
	jobState.IsInUse(nRoundID, bWait, bRetry, nMaxValue);
	if (!bWait)
		return;

	JobManager::IJobManager* pJobManager = gEnv->GetJobManager();

	// get a semaphore to wait on
	TSemaphoreHandle semaphoreHandle = pJobManager->AllocateSemaphore(this);

	// try to set semaphore (info block could have finished in the meantime, or another thread has set the semaphore
	SInfoBlockState currentInfoBlockState;
	SInfoBlockState newInfoBlockState;
	SInfoBlockState resultInfoBlockState;

	currentInfoBlockState.nValue = *(const_cast<volatile LONG*>(&jobState.nValue));

	currentInfoBlockState.IsInUse(nRoundID, bWait, bRetry, nMaxValue);
	if (bWait)
	{
		// is a semaphore already set
		if (currentInfoBlockState.nSemaphoreHandle != 0)
		{
			if (pJobManager->AddRefSemaphore(currentInfoBlockState.nSemaphoreHandle, this))
			{
				pJobManager->GetSemaphore(currentInfoBlockState.nSemaphoreHandle, this)->Acquire();
				pJobManager->DeallocateSemaphore(currentInfoBlockState.nSemaphoreHandle, this);
			}
		}
		else
		{
			// try to set the semaphore
			newInfoBlockState.nRoundID = currentInfoBlockState.nRoundID;
			newInfoBlockState.nSemaphoreHandle = semaphoreHandle;

			resultInfoBlockState.nValue = CryInterlockedCompareExchange((volatile LONG*)&jobState.nValue, newInfoBlockState.nValue, currentInfoBlockState.nValue);

			// three case are now possible:
			//a) job has finished -> we only need to free our semaphore
			//b) we succeeded setting our semaphore, thus wait for it
			//c) another waiter has set it's sempahore wait for the other one
			resultInfoBlockState.IsInUse(nRoundID, bWait, bRetry, nMaxValue);
			if (bWait == true)  // case a) do nothing
			{
				if (resultInfoBlockState.nValue == currentInfoBlockState.nValue) // case b)
				{
					pJobManager->GetSemaphore(semaphoreHandle, this)->Acquire();
				}
				else // case c)
				{
					if (pJobManager->AddRefSemaphore(resultInfoBlockState.nSemaphoreHandle, this))
					{
						pJobManager->GetSemaphore(resultInfoBlockState.nSemaphoreHandle, this)->Acquire();
						pJobManager->DeallocateSemaphore(resultInfoBlockState.nSemaphoreHandle, this);
					}
				}
			}
		}
	}

	pJobManager->GetSemaphore(semaphoreHandle, this)->SetStopped();
	pJobManager->DeallocateSemaphore(semaphoreHandle, this);
}

/////////////////////////////////////////////////////////////////////////////////
inline void JobManager::SJobState::RunPostJob()
{
	// Start post job if set.
	if (m_pImpl->m_pFollowUpJob)
	{
		//job is cleared after the scope to prevent infinite chaining
		std::unique_ptr<CJobBase> job = std::move(m_pImpl->m_pFollowUpJob);
		job->Run();
	}
}

/////////////////////////////////////////////////////////////////////////////////
inline const bool JobManager::SJobState::Wait()
{
	return gEnv->pJobManager->WaitForJob(*this);
}

#include "IJobManager_JobDelegator.h"

/////////////////////////////////////////////////////////////////////////////////
template<typename Callback>
inline void JobManager::IJobManager::AddLambdaJob(const char* jobName, Callback&& lambdaCallback, TPriorityLevel priority, SJobState* pJobState)
{
	Detail::CGenericJob<Detail::SJobLambdaFunction<>> job{ jobName, std::forward<Callback>(lambdaCallback) };
	job.SetPriorityLevel(priority);
	job.RegisterJobState(pJobState);
	job.Run();
}

/////////////////////////////////////////////////////////////////////////////////
template<class TJob>
ILINE void JobManager::SJobState::RegisterPostJob(TJob&& postJob)
{
	m_pImpl->m_pFollowUpJob.reset(new TJob(std::forward<TJob>(postJob)));
}

//! Global helper function to wait for a job.
//! Wait for a job, preempt the calling thread if the job is not done yet.
inline const bool CryWaitForJob(JobManager::SJobState& rJobState)
{
	return gEnv->pJobManager->WaitForJob(rJobState);
}

// Shorter helper type for job states
typedef JobManager::SJobState       CryJobState;
