// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BlockingBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "BlockingBackEnd.h"
#include "../JobManager.h"
#include "../../System.h"
#include "../../CPUDetect.h"

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEnd::CBlockingBackEnd(JobManager::SInfoBlock** pRegularWorkerFallbacks, uint32 nRegularWorkerThreads) :
	m_Semaphore(SJobQueue_BlockingBackEnd::eMaxWorkQueueJobsSize),
	m_pRegularWorkerFallbacks(pRegularWorkerFallbacks),
	m_nRegularWorkerThreads(nRegularWorkerThreads),
	m_pWorkerThreads(NULL),
	m_nNumWorker(0)
{
	m_JobQueue.Init();

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	m_pBackEndWorkerProfiler = 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEnd::~CBlockingBackEnd()
{
	ShutDown();
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::BlockingBackEnd::CBlockingBackEnd::Init(uint32 nSysMaxWorker)
{
	m_pWorkerThreads = new CBlockingBackEndWorkerThread*[nSysMaxWorker];

	for (uint32 i = 0; i < nSysMaxWorker; ++i)
	{
		m_pWorkerThreads[i] = new CBlockingBackEndWorkerThread(this, m_Semaphore, m_JobQueue, m_pRegularWorkerFallbacks, m_nRegularWorkerThreads, i);

		if (!gEnv->pThreadManager->SpawnThread(m_pWorkerThreads[i], "JobSystem_Worker_%u (Blocking)", i))
		{
			CryFatalError("Error spawning \"JobSystem_Worker_%u (Blocking)\" thread.", i);
		}
	}

	m_nNumWorker = nSysMaxWorker;

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	m_pBackEndWorkerProfiler = new JobManager::CWorkerBackEndProfiler;
	m_pBackEndWorkerProfiler->Init(m_nNumWorker);
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::BlockingBackEnd::CBlockingBackEnd::ShutDown()
{
	for (uint32 i = 0; i < m_nNumWorker; ++i)
	{
		if (m_pWorkerThreads[i] == NULL)
			continue;

		m_pWorkerThreads[i]->SignalStopWork();
		m_Semaphore.Release();
	}

	for (uint32 i = 0; i < m_nNumWorker; ++i)
	{
		if (m_pWorkerThreads[i] == NULL)
			continue;

		if (gEnv->pThreadManager->JoinThread(m_pWorkerThreads[i], eJM_TryJoin))
		{
			delete m_pWorkerThreads[i];
			m_pWorkerThreads[i] = NULL;
		}
	}

	m_pWorkerThreads = NULL;
	m_nNumWorker = 0;

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	SAFE_DELETE(m_pBackEndWorkerProfiler);
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEnd::AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock)
{
	uint32 nJobPriority = crJob.GetPriorityLevel();

	/////////////////////////////////////////////////////////////////////////////
	// Acquire Infoblock to use
	uint32 jobSlot;

	m_JobQueue.GetJobSlot(jobSlot, nJobPriority);

#if !defined(_RELEASE)
	CJobManager::Instance()->IncreaseRunJobs();
#endif
	// copy info block into job queue
	JobManager::SInfoBlock& RESTRICT_REFERENCE rJobInfoBlock = m_JobQueue.jobInfoBlocks[nJobPriority][jobSlot];

	// since we will use the whole InfoBlock, and it is aligned to 128 bytes, clear the cacheline, this is faster than a cachemiss on write
	//STATIC_CHECK( sizeof(JobManager::SInfoBlock) == 512, ERROR_SIZE_OF_SINFOBLOCK_NOT_EQUALS_512 );

	// first cache line needs to be persistent
	ResetLine128(&rJobInfoBlock, 128);
	ResetLine128(&rJobInfoBlock, 256);
	ResetLine128(&rJobInfoBlock, 384);

	/////////////////////////////////////////////////////////////////////////////
	// Initialize the InfoBlock
	rInfoBlock.AssignMembersTo(&rJobInfoBlock);

	JobManager::CJobManager::CopyJobParameter(crJob.GetParamDataSize(), rJobInfoBlock.GetParamAddress(), crJob.GetJobParamData());

	assert(rInfoBlock.jobInvoker);

	const uint32 cJobId = cJobHandle->jobId;
	rJobInfoBlock.jobId = (unsigned char)cJobId;

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	assert(cJobId < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS);
	m_pBackEndWorkerProfiler->RegisterJob(cJobId, CJobManager::Instance()->GetJobName(rInfoBlock.jobInvoker));
	rJobInfoBlock.frameProfIndex = (unsigned char)m_pBackEndWorkerProfiler->GetProfileIndex();
#endif

	/////////////////////////////////////////////////////////////////////////////
	// initialization finished, make all visible for worker threads
	// the producing thread won't need the info block anymore, flush it from the cache
	FlushLine128(&rJobInfoBlock, 0);
	FlushLine128(&rJobInfoBlock, 128);
	FlushLine128(&rJobInfoBlock, 256);
	FlushLine128(&rJobInfoBlock, 384);

	//CryLogAlways("Add Job to Slot 0x%x, priority 0x%x", jobSlot, nJobPriority );
	MemoryBarrier();
	m_JobQueue.jobInfoBlockStates[nJobPriority][jobSlot].SetReady();

	// Release semaphore count to signal the workers that work is available
	m_Semaphore.Release();
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::SignalStopWork()
{
	m_bStop = true;
}

bool JobManager::BlockingBackEnd::IsBlockingWorkerId(uint32 workerId)
{
	return (workerId & JobManager::s_blockingWorkerFlag) != 0;
}

uint32 JobManager::BlockingBackEnd::GetIndexFromWorkerId(uint32 workerId)
{
	CRY_ASSERT(IsBlockingWorkerId(workerId));
	return workerId & ~JobManager::s_blockingWorkerFlag;
}

//////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::ThreadEntry()
{
	// set up thread id
	JobManager::detail::SetWorkerThreadId(m_nId | JobManager::s_blockingWorkerFlag);
	do
	{
		SInfoBlock infoBlock;
		///////////////////////////////////////////////////////////////////////////
		// wait for new work
		{
			//CRY_PROFILE_SECTION_WAITING(PROFILE_SYSTEM, "Wait - JobWorkerThread");
			m_rSemaphore.Acquire();
		}

		IF (m_bStop == true, 0)
			break;

		bool bFoundBlockingFallbackJob = false;

		// handle fallbacks added by other worker threads
		for (uint32 i = 0; i < m_nRegularWorkerThreads; ++i)
		{
			if (m_pRegularWorkerFallbacks[i])
			{
				JobManager::SInfoBlock* pRegularWorkerFallback = NULL;
				do
				{
					pRegularWorkerFallback = const_cast<JobManager::SInfoBlock*>(*(const_cast<volatile JobManager::SInfoBlock**>(&m_pRegularWorkerFallbacks[i])));
				}
				while (CryInterlockedCompareExchangePointer(alias_cast<void* volatile*>(&m_pRegularWorkerFallbacks[i]), pRegularWorkerFallback->pNext, alias_cast<void*>(pRegularWorkerFallback)) != pRegularWorkerFallback);

				// in case of a fallback job, just get it from the global per thread list
				pRegularWorkerFallback->AssignMembersTo(&infoBlock);
				JobManager::CJobManager::CopyJobParameter(infoBlock.paramSize << 4, infoBlock.GetParamAddress(), pRegularWorkerFallback->GetParamAddress());

				// free temp info block again
				delete pRegularWorkerFallback;

				bFoundBlockingFallbackJob = true;
				break;
			}
		}

		// in case we didn't find a fallback, try the regular queue
		if (bFoundBlockingFallbackJob == false)
		{
			///////////////////////////////////////////////////////////////////////////
			// multiple steps to get a job of the queue
			// 1. get our job slot index
			uint64 currentPushIndex = ~0;
			uint64 currentPullIndex = ~0;
			uint64 newPullIndex = ~0;
			uint32 nPriorityLevel = ~0;
			do
			{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID // emulate a 64bit atomic read on PC platfom
				currentPullIndex = CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&m_rJobQueue.pull.index), 0, 0);
				currentPushIndex = CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&m_rJobQueue.push.index), 0, 0);
#else
				currentPullIndex = *const_cast<volatile uint64*>(&m_rJobQueue.pull.index);
				currentPushIndex = *const_cast<volatile uint64*>(&m_rJobQueue.push.index);
#endif
				// spin if the updated push ptr didn't reach us yet
				if (currentPushIndex == currentPullIndex)
					continue;

				// compute priority level from difference between push/pull
				if (!JobManager::SJobQueuePos::IncreasePullIndex(currentPullIndex, currentPushIndex, newPullIndex, nPriorityLevel,
				                                                 m_rJobQueue.GetMaxWorkerQueueJobs(eHighPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eRegularPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eLowPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eStreamPriority)))
					continue;

				// stop spinning when we succesfull got the index
				if (CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&m_rJobQueue.pull.index), newPullIndex, currentPullIndex) == currentPullIndex)
					break;

			}
			while (true);

			// compute our jobslot index from the only increasing publish index
			uint32 nExtractedCurIndex = static_cast<uint32>(JobManager::SJobQueuePos::ExtractIndex(currentPullIndex, nPriorityLevel));
			uint32 nNumWorkerQUeueJobs = m_rJobQueue.GetMaxWorkerQueueJobs(nPriorityLevel);
			uint32 nJobSlot = nExtractedCurIndex & (nNumWorkerQUeueJobs - 1);

			//CryLogAlways("Got Job From Slot 0x%x nPriorityLevel 0x%x", nJobSlot, nPriorityLevel );
			// 2. Wait still the produces has finished writing all data to the SInfoBlock
			JobManager::detail::SJobQueueSlotState* pJobInfoBlockState = &m_rJobQueue.jobInfoBlockStates[nPriorityLevel][nJobSlot];
			int iter = 0;
			while (!pJobInfoBlockState->IsReady())
			{
				CrySleep(iter++ > 10 ? 1 : 0);
			}
			;

			// 3. Get a local copy of the info block as asson as it is ready to be used
			JobManager::SInfoBlock* pCurrentJobSlot = &m_rJobQueue.jobInfoBlocks[nPriorityLevel][nJobSlot];
			pCurrentJobSlot->AssignMembersTo(&infoBlock);
			JobManager::CJobManager::CopyJobParameter(infoBlock.paramSize << 4, infoBlock.GetParamAddress(), pCurrentJobSlot->GetParamAddress());

			// 4. Remark the job state as suspended
			MemoryBarrier();
			pJobInfoBlockState->SetNotReady();

			// 5. Mark the jobslot as free again
			MemoryBarrier();
			pCurrentJobSlot->Release((1 << JobManager::SJobQueuePos::eBitsPerPriorityLevel) / m_rJobQueue.GetMaxWorkerQueueJobs(nPriorityLevel));
		}

		///////////////////////////////////////////////////////////////////////////
		// now we have a valid SInfoBlock to start work on it
		// Now we are safe to use the info block
		assert(infoBlock.jobInvoker);
		assert(infoBlock.GetParamAddress());

#if defined(JOBMANAGER_SUPPORT_PROFILING)
		SJobProfilingData* pJobProfilingData = gEnv->GetJobManager()->GetProfilingData(infoBlock.profilerIndex);
		pJobProfilingData->startTime = gEnv->pTimer->GetAsyncTime();
		pJobProfilingData->isWaiting = false;
		pJobProfilingData->nWorkerThread = GetWorkerThreadId();
#endif

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
		const uint64 nStartTime = JobManager::IWorkerBackEndProfiler::GetTimeSample();
#endif
		// call delegator function to invoke job entry
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
		CRY_PROFILE_SECTION(PROFILE_SYSTEM, "Job");
#endif
		{
			MEMSTAT_CONTEXT_FMT(EMemStatContextType::Other, "Job: %s", CJobManager::Instance()->GetJobName(infoBlock.jobInvoker));
			(*infoBlock.jobInvoker)(infoBlock.GetParamAddress());
		}
#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
		JobManager::IWorkerBackEndProfiler* workerProfiler = m_pBlockingBackend->GetBackEndWorkerProfiler();
		const uint64 nEndTime = JobManager::IWorkerBackEndProfiler::GetTimeSample();
		workerProfiler->RecordJob(infoBlock.frameProfIndex, m_nId, static_cast<const uint32>(infoBlock.jobId), static_cast<const uint32>(nEndTime - nStartTime));
#endif

		IF (infoBlock.GetJobState(), 1)
		{
			SJobState* pJobState = infoBlock.GetJobState();
			pJobState->SetStopped();
		}

#if defined(JOBMANAGER_SUPPORT_PROFILING)
		pJobProfilingData->endTime = gEnv->pTimer->GetAsyncTime();
#endif
	}
	while (m_bStop == false);
}
///////////////////////////////////////////////////////////////////////////////
ILINE void IncrQueuePullPointer_Blocking(INT_PTR& rCurPullAddr, const INT_PTR cIncr, const INT_PTR cQueueStart, const INT_PTR cQueueEnd)
{
	const INT_PTR cNextPull = rCurPullAddr + cIncr;
	rCurPullAddr = (cNextPull >= cQueueEnd) ? cQueueStart : cNextPull;
}

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::CBlockingBackEndWorkerThread(CBlockingBackEnd* pBlockingBackend, CryFastSemaphore& rSemaphore, JobManager::SJobQueue_BlockingBackEnd& rJobQueue, JobManager::SInfoBlock** pRegularWorkerFallbacks, uint32 nRegularWorkerThreads, uint32 nID) :
	m_rSemaphore(rSemaphore),
	m_rJobQueue(rJobQueue),
	m_bStop(false),
	m_pBlockingBackend(pBlockingBackend),
	m_nId(nID),
	m_pRegularWorkerFallbacks(pRegularWorkerFallbacks),
	m_nRegularWorkerThreads(nRegularWorkerThreads)
{
}

///////////////////////////////////////////////////////////////////////////////
JobManager::BlockingBackEnd::CBlockingBackEndWorkerThread::~CBlockingBackEndWorkerThread()
{

}

///////////////////////////////////////////////////////////////////////////////
void JobManager::BlockingBackEnd::CBlockingBackEnd::AddBlockingFallbackJob(JobManager::SInfoBlock* pInfoBlock, uint32 nWorkerThreadID)
{
	volatile JobManager::SInfoBlock* pCurrentWorkerFallback = NULL;
	do
	{
		pCurrentWorkerFallback = *(const_cast<volatile JobManager::SInfoBlock**>(&m_pRegularWorkerFallbacks[nWorkerThreadID]));
		pInfoBlock->pNext = const_cast<JobManager::SInfoBlock*>(pCurrentWorkerFallback);
	}
	while (CryInterlockedCompareExchangePointer(alias_cast<void* volatile*>(&m_pRegularWorkerFallbacks[nWorkerThreadID]), pInfoBlock, alias_cast<void*>(pCurrentWorkerFallback)) != pCurrentWorkerFallback);
}
