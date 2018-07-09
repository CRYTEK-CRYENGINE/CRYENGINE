// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   declaration of job structures
 */

#pragma once

#include <CryThreading/IJobManager.h>

//forward declarations for friend usage
namespace JobManager
{
namespace detail
{
// return results for AddJob
enum EAddJobRes
{
	eAJR_Success,                       // success of adding job
	eAJR_NeedFallbackJobInfoBlock,      // Job was added, but a fallback list was used
};

//triple buffer frame stats
enum
{
	eJOB_FRAME_STATS               = 3,
	eJOB_FRAME_STATS_MAX_SUPP_JOBS = 64
};

// configuration for job queue sizes:
// we have two types of backends, threads and blocking threads
// each jobqueue has three priority levels, the first value is high priority, then regular, followed by low priority
static const unsigned int cMaxWorkQueueJobs_ThreadBackEnd_HighPriority = 2048;
static const unsigned int cMaxWorkQueueJobs_ThreadBackEnd_RegularPriority = 2048;
static const unsigned int cMaxWorkQueueJobs_ThreadBackEnd_LowPriority = 2048;
static const unsigned int cMaxWorkQueueJobs_ThreadBackEnd_StreamPriority = 2048;

static const unsigned int cMaxWorkQueueJobs_BlockingBackEnd_HighPriority = 512;
static const unsigned int cMaxWorkQueueJobs_BlockingBackEnd_RegularPriority = 512;
static const unsigned int cMaxWorkQueueJobs_BlockingBackEnd_LowPriority = 512;
static const unsigned int cMaxWorkQueueJobs_BlockingBackEnd_StreamPriority = 512;

// struct to manage the state of a job slot
// used to indicate that a info block has been finished writing
struct SJobQueueSlotState
{
public:
	bool IsReady() const { return m_state == READY; }

	void SetReady()      { m_state = READY; }
	void SetNotReady()   { m_state = NOT_READY; }

private:
	enum StateT
	{
		NOT_READY = 0,
		READY     = 1
	};
	volatile StateT m_state;
};

}   // namespace detail

// queue node where jobs are pushed into and pulled from
// individual aligned because it is using MFC_LLAR atomics for mutual exclusive access(operates on a 128 byte address base)
// dont waste memory by putting the job queue between the 2 aligned buffers
template<int nMaxWorkQueueJobsHighPriority, int nMaxWorkQueueJobsRegularPriority, int nMaxWorkQueueJobsLowPriority, int nMaxWorkQueueJobsStreamPriority>
struct SJobQueue
{
	// store queue sizes to allow to query those
	enum
	{
		eMaxWorkQueueJobsHighPriority    = nMaxWorkQueueJobsHighPriority,
		eMaxWorkQueueJobsRegularPriority = nMaxWorkQueueJobsRegularPriority,
		eMaxWorkQueueJobsLowPriority     = nMaxWorkQueueJobsLowPriority,
		eMaxWorkQueueJobsStreamPriority  = nMaxWorkQueueJobsStreamPriority,
		eMaxWorkQueueJobsSize            = nMaxWorkQueueJobsHighPriority + nMaxWorkQueueJobsRegularPriority + nMaxWorkQueueJobsLowPriority + nMaxWorkQueueJobsStreamPriority
	};

	CRY_ALIGN(128) JobManager::SJobQueuePos push;                     // position in which jobs are pushed by the PPU
	CRY_ALIGN(128) JobManager::SJobQueuePos pull;                     // position from which jobs are pulled

	JobManager::SInfoBlock*                 jobInfoBlocks[eNumPriorityLevel];      // aligned array of SInfoBlocks per priority level
	JobManager::detail::SJobQueueSlotState* jobInfoBlockStates[eNumPriorityLevel]; // aligned array of SInfoBlocks states per priority level

	// initialize the jobqueue, should only be called once
	void Init();

	//gets job slot for next job (to get storage index for SJobdata), waits until a job slots becomes available again since data get overwritten
	JobManager::detail::EAddJobRes GetJobSlot(uint32& rJobSlot, uint32 nPriorityLevel, bool bWaitForFreeJobSlot);

	static uint32                  GetMaxWorkerQueueJobs(uint32 nPriorityLevel);
	static const char*             PrioToString(uint32 nPriorityLevel);
};

///////////////////////////////////////////////////////////////////////////////
// convinient typedef for the different platform queues
typedef SJobQueue<JobManager::detail::cMaxWorkQueueJobs_ThreadBackEnd_HighPriority, JobManager::detail::cMaxWorkQueueJobs_ThreadBackEnd_RegularPriority, JobManager::detail::cMaxWorkQueueJobs_ThreadBackEnd_LowPriority, JobManager::detail::cMaxWorkQueueJobs_ThreadBackEnd_StreamPriority>         SJobQueue_ThreadBackEnd;
typedef SJobQueue<JobManager::detail::cMaxWorkQueueJobs_BlockingBackEnd_HighPriority, JobManager::detail::cMaxWorkQueueJobs_BlockingBackEnd_RegularPriority, JobManager::detail::cMaxWorkQueueJobs_BlockingBackEnd_LowPriority, JobManager::detail::cMaxWorkQueueJobs_BlockingBackEnd_StreamPriority> SJobQueue_BlockingBackEnd;
}

///////////////////////////////////////////////////////////////////////////////
template<int nMaxWorkQueueJobsHighPriority, int nMaxWorkQueueJobsRegularPriority, int nMaxWorkQueueJobsLowPriority, int nMaxWorkQueueJobsStreamPriority>
inline JobManager::detail::EAddJobRes JobManager::SJobQueue<nMaxWorkQueueJobsHighPriority, nMaxWorkQueueJobsRegularPriority, nMaxWorkQueueJobsLowPriority, nMaxWorkQueueJobsStreamPriority >::GetJobSlot(uint32& rJobSlot, uint32 nPriorityLevel, bool bWaitForFreeJobSlot)
{
	// verify assumation about queue size at compile time
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsHighPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE__HIGH_PRIORITY_IS_NOT_POWER_OF_TWO);
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsRegularPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE_REGULAR_PRIORITY_IS_NOT_POWER_OF_TWO);
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsLowPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE__LOW_PRIORITY_IS_NOT_POWER_OF_TWO);
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsStreamPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE__LOW_PRIORITY_IS_NOT_POWER_OF_TWO);

	SJobQueuePos& RESTRICT_REFERENCE curPushEntry = push;

	uint64 currentIndex;
	uint64 nextIndex;
	uint32 nRoundID;
	uint32 nExtractedIndex;
	JobManager::SInfoBlock* pPushInfoBlock = NULL;
	uint32 nMaxWorkerQueueJobs = GetMaxWorkerQueueJobs(nPriorityLevel);
	do
	{
		// fetch next to update field
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX // emulate a 64bit atomic read on PC platfom
		currentIndex = CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&curPushEntry.index), 0, 0);
#else
		currentIndex = *const_cast<volatile uint64*>(&curPushEntry.index);
#endif

		nextIndex = JobManager::SJobQueuePos::IncreasePushIndex(currentIndex, nPriorityLevel);
		nExtractedIndex = static_cast<uint32>(JobManager::SJobQueuePos::ExtractIndex(currentIndex, nPriorityLevel));
		nRoundID = nExtractedIndex / nMaxWorkerQueueJobs;

		// compute the job slot for this fetch index
		unsigned int jobSlot = nExtractedIndex & (nMaxWorkerQueueJobs - 1);
		pPushInfoBlock = &curPushEntry.jobQueue[nPriorityLevel][jobSlot];

		//do not overtake pull pointer
		bool bWait = false;
		bool bRetry = false;
		pPushInfoBlock->IsInUse(nRoundID, bWait, bRetry, (1 << JobManager::SJobQueuePos::eBitsPerPriorityLevel) / nMaxWorkerQueueJobs);

		if (bRetry) // need to refetch due long suspending time
			continue;

		if (bWait)
		{
			CRY_ASSERT_MESSAGE(false, "JobManager: Exceeded job queue size (%u) for priority level \"%s\"", nMaxWorkerQueueJobs, PrioToString(nPriorityLevel));
			if (bWaitForFreeJobSlot)
			{
				pPushInfoBlock->Wait(nRoundID, (1 << JobManager::SJobQueuePos::eBitsPerPriorityLevel) / nMaxWorkerQueueJobs);
			}
			else
			{
				return JobManager::detail::eAJR_NeedFallbackJobInfoBlock;
		}
		}

		rJobSlot = jobSlot;

		if (CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&curPushEntry.index), nextIndex, currentIndex) == currentIndex)
			break;

	}
	while (true);

	return JobManager::detail::eAJR_Success;
}

///////////////////////////////////////////////////////////////////////////////
template<int nMaxWorkQueueJobsHighPriority, int nMaxWorkQueueJobsRegularPriority, int nMaxWorkQueueJobsLowPriority, int nMaxWorkQueueJobsStreamPriority>
inline void JobManager::SJobQueue<nMaxWorkQueueJobsHighPriority, nMaxWorkQueueJobsRegularPriority, nMaxWorkQueueJobsLowPriority, nMaxWorkQueueJobsStreamPriority >::Init()
{
	// verify assumation about queue size at compile time
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsHighPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE__HIGH_PRIORITY_IS_NOT_POWER_OF_TWO);
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsRegularPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE_REGULAR_PRIORITY_IS_NOT_POWER_OF_TWO);
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsLowPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE__LOW_PRIORITY_IS_NOT_POWER_OF_TWO);
	STATIC_CHECK(IsPowerOfTwoCompileTime<eMaxWorkQueueJobsStreamPriority>::IsPowerOfTwo, ERROR_MAX_JOB_QUEUE_SIZE__LOW_PRIORITY_IS_NOT_POWER_OF_TWO);

	// init job queues
	jobInfoBlocks[eHighPriority] = static_cast<JobManager::SInfoBlock*>(CryModuleMemalign(eMaxWorkQueueJobsHighPriority * sizeof(JobManager::SInfoBlock), 128));
	jobInfoBlockStates[eHighPriority] = static_cast<JobManager::detail::SJobQueueSlotState*>(CryModuleMemalign(eMaxWorkQueueJobsHighPriority * sizeof(JobManager::detail::SJobQueueSlotState), 128));
	memset(jobInfoBlocks[eHighPriority], 0, eMaxWorkQueueJobsHighPriority * sizeof(JobManager::SInfoBlock));
	memset(jobInfoBlockStates[eHighPriority], 0, eMaxWorkQueueJobsHighPriority * sizeof(JobManager::detail::SJobQueueSlotState));

	jobInfoBlocks[eRegularPriority] = static_cast<JobManager::SInfoBlock*>(CryModuleMemalign(eMaxWorkQueueJobsRegularPriority * sizeof(JobManager::SInfoBlock), 128));
	jobInfoBlockStates[eRegularPriority] = static_cast<JobManager::detail::SJobQueueSlotState*>(CryModuleMemalign(eMaxWorkQueueJobsRegularPriority * sizeof(JobManager::detail::SJobQueueSlotState), 128));
	memset(jobInfoBlocks[eRegularPriority], 0, eMaxWorkQueueJobsRegularPriority * sizeof(JobManager::SInfoBlock));
	memset(jobInfoBlockStates[eRegularPriority], 0, eMaxWorkQueueJobsRegularPriority * sizeof(JobManager::detail::SJobQueueSlotState));

	jobInfoBlocks[eLowPriority] = static_cast<JobManager::SInfoBlock*>(CryModuleMemalign(eMaxWorkQueueJobsLowPriority * sizeof(JobManager::SInfoBlock), 128));
	jobInfoBlockStates[eLowPriority] = static_cast<JobManager::detail::SJobQueueSlotState*>(CryModuleMemalign(eMaxWorkQueueJobsLowPriority * sizeof(JobManager::detail::SJobQueueSlotState), 128));
	memset(jobInfoBlocks[eLowPriority], 0, eMaxWorkQueueJobsLowPriority * sizeof(JobManager::SInfoBlock));
	memset(jobInfoBlockStates[eLowPriority], 0, eMaxWorkQueueJobsLowPriority * sizeof(JobManager::detail::SJobQueueSlotState));

	jobInfoBlocks[eStreamPriority] = static_cast<JobManager::SInfoBlock*>(CryModuleMemalign(eMaxWorkQueueJobsStreamPriority * sizeof(JobManager::SInfoBlock), 128));
	jobInfoBlockStates[eStreamPriority] = static_cast<JobManager::detail::SJobQueueSlotState*>(CryModuleMemalign(eMaxWorkQueueJobsStreamPriority * sizeof(JobManager::detail::SJobQueueSlotState), 128));
	memset(jobInfoBlocks[eStreamPriority], 0, eMaxWorkQueueJobsStreamPriority * sizeof(JobManager::SInfoBlock));
	memset(jobInfoBlockStates[eStreamPriority], 0, eMaxWorkQueueJobsStreamPriority * sizeof(JobManager::detail::SJobQueueSlotState));

	// init queue pos objects
	push.jobQueue[eHighPriority] = jobInfoBlocks[eHighPriority];
	push.jobQueue[eRegularPriority] = jobInfoBlocks[eRegularPriority];
	push.jobQueue[eLowPriority] = jobInfoBlocks[eLowPriority];
	push.jobQueue[eStreamPriority] = jobInfoBlocks[eStreamPriority];
	push.jobQueueStates[eHighPriority] = jobInfoBlockStates[eHighPriority];
	push.jobQueueStates[eRegularPriority] = jobInfoBlockStates[eRegularPriority];
	push.jobQueueStates[eLowPriority] = jobInfoBlockStates[eLowPriority];
	push.jobQueueStates[eStreamPriority] = jobInfoBlockStates[eStreamPriority];
	push.index = 0;

	pull.jobQueue[eHighPriority] = jobInfoBlocks[eHighPriority];
	pull.jobQueue[eRegularPriority] = jobInfoBlocks[eRegularPriority];
	pull.jobQueue[eLowPriority] = jobInfoBlocks[eLowPriority];
	pull.jobQueue[eStreamPriority] = jobInfoBlocks[eStreamPriority];
	pull.jobQueueStates[eHighPriority] = jobInfoBlockStates[eHighPriority];
	pull.jobQueueStates[eRegularPriority] = jobInfoBlockStates[eRegularPriority];
	pull.jobQueueStates[eLowPriority] = jobInfoBlockStates[eLowPriority];
	pull.jobQueueStates[eStreamPriority] = jobInfoBlockStates[eStreamPriority];
	pull.index = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<int nMaxWorkQueueJobsHighPriority, int nMaxWorkQueueJobsRegularPriority, int nMaxWorkQueueJobsLowPriority, int nMaxWorkQueueJobsStreamPriority>
ILINE uint32 JobManager::SJobQueue<nMaxWorkQueueJobsHighPriority, nMaxWorkQueueJobsRegularPriority, nMaxWorkQueueJobsLowPriority, nMaxWorkQueueJobsStreamPriority >::GetMaxWorkerQueueJobs(uint32 nPriorityLevel)
{
	switch (nPriorityLevel)
	{
	case eHighPriority:
		return eMaxWorkQueueJobsHighPriority;
	case eRegularPriority:
		return eMaxWorkQueueJobsRegularPriority;
	case eLowPriority:
		return eMaxWorkQueueJobsLowPriority;
	case eStreamPriority:
		return eMaxWorkQueueJobsStreamPriority;
	default:
		return ~0;
	}
}


template<int nMaxWorkQueueJobsHighPriority, int nMaxWorkQueueJobsRegularPriority, int nMaxWorkQueueJobsLowPriority, int nMaxWorkQueueJobsStreamPriority>
ILINE const char* JobManager::SJobQueue<nMaxWorkQueueJobsHighPriority, nMaxWorkQueueJobsRegularPriority, nMaxWorkQueueJobsLowPriority, nMaxWorkQueueJobsStreamPriority>::PrioToString(uint32 nPriorityLevel)
{
	switch (nPriorityLevel)
	{
	case eHighPriority:
		return "High";
	case eRegularPriority:
		return "Regular";
	case eLowPriority:
		return "Low";
	case eStreamPriority:
		return "Stream";
	default:
		return "Unknown";
	}
}
