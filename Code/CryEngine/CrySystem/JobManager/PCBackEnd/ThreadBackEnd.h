// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ThreadBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////

#ifndef THREAD_BACKEND_H_
#define THREAD_BACKEND_H_

#include <CryThreading/IJobManager.h>
#include "../JobStructs.h"

#include <CryThreading/IThreadManager.h>

#if CRY_PLATFORM_DURANGO
	#include <CryCore/Platform/CryWindows.h>
	#define JOB_SPIN_DURING_IDLE
#endif

namespace JobManager
{
class CJobManager;
class CWorkerBackEndProfiler;
}

namespace JobManager {
class CJobManager;

namespace ThreadBackEnd {
namespace detail {
// stack size for backend worker threads
enum { eStackSize = 256 * 1024 };

class CWaitForJobObject
{
public:
	CWaitForJobObject(uint32 nMaxCount) :
#if defined(JOB_SPIN_DURING_IDLE)
		m_nCounter(0)
#else
		m_Semaphore(nMaxCount)
#endif
	{}

	void SignalNewJob()
	{
#if defined(JOB_SPIN_DURING_IDLE)
		int nCount = ~0;
		do
		{
			nCount = *const_cast<volatile int*>(&m_nCounter);
		}
		while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount + 1, nCount) != nCount);
#else
		m_Semaphore.Release();
#endif
	}

	bool TryGetJob()
	{
#if CRY_PLATFORM_DURANGO
		int nCount = *const_cast<volatile int*>(&m_nCounter);
		if (nCount > 0)
		{
			if (CryInterlockedCompareExchange(alias_cast<volatile long*>(&m_nCounter), nCount - 1, nCount) == nCount)
				return true;
		}
		return false;
#else
		return false;
#endif
	}
	void WaitForNewJob(uint32 nWorkerID)
	{
#if defined(JOB_SPIN_DURING_IDLE)
		int nCount = ~0;

	#if CRY_PLATFORM_DURANGO
		UAsyncDipState nCurrentState;
		UAsyncDipState nNewState;

		do      // mark as idle
		{
			nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
			nNewState = nCurrentState;
			nNewState.nWorker_Idle |= 1 << nWorkerID;
			if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
				break;
		}
		while (true);
retry:
	#endif  // CRY_PLATFORM_DURANGO

		do
		{

	#if CRY_PLATFORM_DURANGO
			nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
			if (nCurrentState.nQueueGuard == 0 && nCurrentState.nNumJobs > 0)
			{
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

				nNewState = nCurrentState;
				nNewState.nQueueGuard = 1;
				if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
				{
ExecuteAsyncDip:
					gEnv->pRenderer->ExecuteAsyncDIP();

					// clear guard variable
					do
					{
						nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
						nNewState = nCurrentState;
						nNewState.nQueueGuard = 0;

						// if jobs were added in the meantime, continue executing those
						if (nCurrentState.nNumJobs > 0)
							goto ExecuteAsyncDip;

						if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
							break;
					}
					while (true);
				}     // else another thread must have gotten the guard var, go back to IDLE priority spinning
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
			}
	#endif    // CRY_PLATFORM_DURANGO

			nCount = *const_cast<volatile int*>(&m_nCounter);
			if (nCount > 0)
			{
				if (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount - 1, nCount) == nCount)
					break;
			}

			YieldProcessor();
			YieldProcessor();
			YieldProcessor();
			YieldProcessor();
			YieldProcessor();
			YieldProcessor();
			YieldProcessor();
			YieldProcessor();

			SwitchToThread();

		}
		while (true);

	#if CRY_PLATFORM_DURANGO
		do      // mark as busy
		{
			nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
			nNewState = nCurrentState;
			nNewState.nWorker_Idle &= ~(1 << nWorkerID);

			// new job was submitted while we were leaving the loop
			// try to get the guard again
			if (nCurrentState.nQueueGuard == 0 && nCurrentState.nNumJobs > 0)
			{
				do      // increment job counter again, to allow another thread to take this job
				{
					nCount = *const_cast<volatile int*>(&m_nCounter);
					if (CryInterlockedCompareExchange(alias_cast<volatile long*>(&m_nCounter), nCount + 1, nCount) == nCount)
						break;
				}
				while (true);

				goto retry;
			}

			if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
				break;

		}
		while (true);
	#endif  // CRY_PLATFORM_DURANGO
#else
		m_Semaphore.Acquire();
#endif
	}
private:
#if defined(JOB_SPIN_DURING_IDLE)
	volatile int m_nCounter;
#else
	CryFastSemaphore m_Semaphore;
#endif
};

} // namespace detail
  // forward declarations
class CThreadBackEnd;

// class to represent a worker thread for the PC backend
class CThreadBackEndWorkerThread : public IThread
{
public:
	CThreadBackEndWorkerThread(CThreadBackEnd* pThreadBackend, detail::CWaitForJobObject& rSemaphore, JobManager::SJobQueue_ThreadBackEnd& rJobQueue, uint32 nId);
	~CThreadBackEndWorkerThread();

	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();
private:
	void DoWorkProducerConsumerQueue(SInfoBlock& rInfoBlock);

	uint32                               m_nId;                   // id of the worker thread
	volatile bool                        m_bStop;
	detail::CWaitForJobObject&           m_rSemaphore;
	JobManager::SJobQueue_ThreadBackEnd& m_rJobQueue;
	CThreadBackEnd*                      m_pThreadBackend;
};

// the implementation of the PC backend
// has n-worker threads which use atomic operations to pull from the job queue
// and uses a semaphore to signal the workers if there is work required
class CThreadBackEnd : public IBackend
{
public:
	CThreadBackEnd();
	virtual ~CThreadBackEnd();

	bool           Init(uint32 nSysMaxWorker);
	bool           ShutDown();
	void           Update() {}

	virtual void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock);

	virtual uint32 GetNumWorkerThreads() const { return m_nNumWorkerThreads; }

	// returns the index to use for the frame profiler
	uint32 GetCurrentFrameBufferIndex() const;

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	JobManager::IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const { return m_pBackEndWorkerProfiler; }
#endif

private:
	friend class JobManager::CJobManager;

	JobManager::SJobQueue_ThreadBackEnd      m_JobQueue;              // job queue node where jobs are pushed into and from
	detail::CWaitForJobObject                m_Semaphore;             // semaphore to count available jobs, to allow the workers to go sleeping instead of spinning when no work is required
	std::vector<CThreadBackEndWorkerThread*> m_arrWorkerThreads;      // array of worker threads
	uint8 m_nNumWorkerThreads;                                        // number of worker threads

	// members required for profiling jobs in the frame profiler
#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	JobManager::IWorkerBackEndProfiler* m_pBackEndWorkerProfiler;
#endif
};

} // namespace ThreadBackEnd
} // namespace JobManager

#endif // THREAD_BACKEND_H_
