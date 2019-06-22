// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryThreading/IJobManager.h>
#include "../JobStructs.h"

#include <CryThreading/IThreadManager.h>

namespace JobManager
{
class CJobManager;
class CWorkerBackEndProfiler;
}

namespace JobManager {
class CJobManager;

namespace ThreadBackEnd {
namespace detail {

#if DURANGO_ENABLE_ASYNC_DIPS == 1
	void ExecuteAsyncDipJob()
	{
		UAsyncDipState nCurrentState;
		UAsyncDipState nNewState;

		nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);

		if (nCurrentState.nQueueGuard == 0 && nCurrentState.nNumJobs > 0)
		{
			HANDLE  threadHandle = GetCurrentThread();
			int oldPrio = GetThreadPriority(threadHandle);
			SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL);

			nNewState = nCurrentState;
			nNewState.nQueueGuard = 1;

			// Aquire Guard
			if (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue)
			{
				// Execute Async DIP Job
				do
				{
					gEnv->pRenderer->ExecuteAsyncDIP();
				} while (nCurrentState.nNumJobs > 0);

				do
				{
					nCurrentState.nValue = *const_cast<volatile uint32*>(&gEnv->mAsyncDipState.nValue);
					nNewState = nCurrentState;
					nNewState.nQueueGuard = 0;
				} while (CryInterlockedCompareExchange((volatile long*)&gEnv->mAsyncDipState.nValue, nNewState.nValue, nCurrentState.nValue) == nCurrentState.nValue);
			}

			// Reset prio
			SetThreadPriority(threadHandle, oldPrio);
		}
	}
#endif  // DURANGO_ENABLE_ASYNC_DIPS	

static thread_local int tls_workerSpins = 0;
#ifdef JOB_SPIN_DURING_IDLE
	
	class CWorkStatusSyncVar
	{
	public:
		CWorkStatusSyncVar()
		{}

		void Wait()
		{
#if DURANGO_ENABLE_ASYNC_DIPS == 1
			ExecuteAsyncDipJob();
#endif
			if (tls_workerSpins++ > 10) // Worker failed more than 10 times to get work from the queue
			{			
				// Deep OS call (10k+ cycles on Win)
				// Allow switching to another thread 
				CrySleep(0);
			}
			else
			{
				// Yield ... will pause processor and give resources to another processor that is on the same core (hyperthreading) if available 
				CryMT::CryYieldThread();
			}					
		}

		void Aquire()
		{
			// Managed to successfully acquire a resource
			tls_workerSpins = 0;
		}

		void ReleaseAndSignal()
		{
		}
	};
#else

// Use lock to allow workers to sleep
// Reduce contention between threads
// Lock contention should only happen when the "count" of jobs in the queue is low and new jobs are added at the same time. (or when jobs wake up)
//
// Producer: 
//  - Increment job counter
//  - Notify condition
//
// Consumer:
//  - When a worker managed to grab a job from the queue, decrement the counter
//  - If a worker has failed to grab a job often enough he might enter Wait. Once we ensured that there is no work available it will enter the wait condition
//
// Note:
// The "m_counter" & "m_workIndicator" might be negative if an already active worker has grabbed the job from the queue prior ReleaseAndSignal() was called by the producer
// 

	class CWorkStatusSyncVar
	{
	public:
		CWorkStatusSyncVar()
			: m_counter(0)
			, m_workIndicator(0)
		{
		}

		void Wait()
		{
			if (m_workIndicator <= 0)
			{
				//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Wait - Wait Start");
				AUTO_LOCK_T(CryMutexFast, m_hasWorkLock);
				{
					//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Wait - Wait End");
					while (m_workIndicator <= 0)
					{
						m_hasWorkCnd.Wait(m_hasWorkLock);
					}
				}
			}
			else
			{
#if DURANGO_ENABLE_ASYNC_DIPS == 1
				ExecuteAsyncDipJob();
#endif
				if (tls_workerSpins++ < 10) // Worker failed more than 10 times to get work from the queue
				{
					//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Wait - Sleep");
					// Deep OS call (10k+ cycles on Win)
					// Allow switching to another thread 
					CrySleep(0);
				}
				else
				{
					// Yield ... will pause processor and give resources to another processor that is on the same core (hyperthreading) if available 
					CryMT::CryYieldThread();
				}
			}
		}

		void Aquire()
		{
			int count = CryInterlockedDecrement(&m_counter);

			// Notify that this was "potentially" the last job in the queue i.e. someone might have already added new work. 
			// Each count == 0 (on decrement) should be matched by a count == 1 (on increment) to keep m_workIndicator in sync
			// Note: count can be negative
			if (count == 0)
			{
				//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Aquire - Wait Start");
				AUTO_LOCK_T(CryMutexFast, m_hasWorkLock);
				{
					//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Aquire - Wait End");
					--m_workIndicator;
				}
			}

			// Reset spin counter as we just managed to grab work from the queue
			tls_workerSpins = 0;
		}

		void ReleaseAndSignal()
		{
			int count = CryInterlockedIncrement(&m_counter);

			// Signal if work is available 
			// Notify that this was "potentially" there might be work in the queue i.e. some worker might have already grabbed the work. 
			// Each count == 1 (on increment) should be matched by a count == 0 (on decrement) to keep m_workIndicator in sync
			// Note: count can be negative
			if (count == 1)
			{
				//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Release - Wait Start");
				AUTO_LOCK_T(CryMutexFast, m_hasWorkLock);
				{
					//CRY_PROFILE_SECTION(PROFILE_SYSTEM, "JobWorkerThread: Release - Wait End");
					m_workIndicator++;

					// Need to wake up all workers as we don't want to end up with a single worker active when "count" > 2
					// Alternative 1: We can use NotifySingle ... if "count" < numWorkers 
					// Alternative 2: Call NotifySingle every time we increment the count
					m_hasWorkCnd.Notify(); 
					
					// Notify() will wake up all jobs but that is ok if we add a lot of jobs quickly we want them to be running, ready to take work.
					// In the worst case they go back to sleep and we wasted cycles
				}
			}
		}

	protected:
		CryConditionVariable m_hasWorkCnd;
		CryMutexFast m_hasWorkLock;
		volatile int32 m_counter;
		volatile int32 m_workIndicator;
	};
#endif

} // namespace detail
  // forward declarations
class CThreadBackEnd;

// class to represent a worker thread for the PC backend
class CThreadBackEndWorkerThread : public IThread
{
	struct STempWorkerInfo
	{
		STempWorkerInfo()
			: doWork(false)
		{
		};

		bool doWork;
		CryConditionVariable doWorkCnd;
		CryMutexFast  doWorkLock;
	};

public:
	CThreadBackEndWorkerThread(CThreadBackEnd* pThreadBackend, detail::CWorkStatusSyncVar& rSemaphore, JobManager::SJobQueue_ThreadBackEnd& rJobQueue, uint32 nId, bool bIsTempWorker);
	~CThreadBackEndWorkerThread();

	// Start accepting work on thread
	virtual void ThreadEntry();

	bool KickTempWorker();
	bool StopTempWorker();
	bool IsTempWorker() { return m_pTempWorkerInfo ? true : false; }

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();
private:

	detail::CWorkStatusSyncVar&          m_rWorkSyncVar;
	JobManager::SJobQueue_ThreadBackEnd& m_rJobQueue;
	CThreadBackEnd*                      m_pThreadBackend;

	STempWorkerInfo*                   m_pTempWorkerInfo;

	uint32                               m_nId;                   // id of the worker thread
	volatile bool                        m_bStop;
};

// the implementation of the PC backend
// has n-worker threads which use atomic operations to pull from the job queue
// and uses a semaphore to signal the workers if there is work required
class CThreadBackEnd final : public IBackend
{
public:
	CThreadBackEnd();
	virtual ~CThreadBackEnd();

	bool           Init(uint32 nSysMaxWorker);
	bool           ShutDown();
	void           Update() {}

	void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock);
	bool   KickTempWorker();
	bool   StopTempWorker();
	uint32 GetNumWorkerThreads() const { return m_nNumWorkerThreads; }

	// returns the index to use for the frame profiler
	uint32 GetCurrentFrameBufferIndex() const;

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	JobManager::IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const { return m_pBackEndWorkerProfiler; }
#endif

private:
	friend class JobManager::CJobManager;

	JobManager::SJobQueue_ThreadBackEnd      m_JobQueue;              // job queue node where jobs are pushed into and from
	detail::CWorkStatusSyncVar               m_Semaphore;             // semaphore to count available jobs, to allow the workers to go sleeping instead of spinning when no work is required
	std::vector<CThreadBackEndWorkerThread*> m_arrWorkerThreads;      // array of worker threads
	uint8 m_nNumWorkerThreads;                                        // number of worker threads

	// members required for profiling jobs in the frame profiler
#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	JobManager::IWorkerBackEndProfiler* m_pBackEndWorkerProfiler;
#endif
};

} // namespace ThreadBackEnd
} // namespace JobManager
