// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ThreadBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////

#ifndef BLOCKING_BACKEND_H_
#define BLOCKING_BACKEND_H_

#include <CryThreading/IJobManager.h>
#include "../JobStructs.h"

#include <CryThreading/IThreadManager.h>

namespace JobManager
{
class CJobManager;
class CWorkerBackEndProfiler;
}

namespace JobManager {
namespace BlockingBackEnd {
namespace detail {
// stack size for each worker thread of the blocking backend
enum {eStackSize = 32 * 1024 };

}   // namespace detail

// forward declarations
class CBlockingBackEnd;

// class to represent a worker thread for the PC backend
class CBlockingBackEndWorkerThread : public IThread
{
public:
	CBlockingBackEndWorkerThread(CBlockingBackEnd* pBlockingBackend, CryFastSemaphore& rSemaphore, JobManager::SJobQueue_BlockingBackEnd& rJobQueue, JobManager::SInfoBlock** pRegularWorkerFallbacks, uint32 nRegularWorkerThreads, uint32 nID);
	~CBlockingBackEndWorkerThread();

	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals to the worker thread that is should not accept anymore work and exit
	void SignalStopWork();

private:
	void DoWork();
	void DoWorkProducerConsumerQueue(SInfoBlock& rInfoBlock);

	uint32                                 m_nId;                   // id of the worker thread
	volatile bool                          m_bStop;
	CryFastSemaphore&                      m_rSemaphore;
	JobManager::SJobQueue_BlockingBackEnd& m_rJobQueue;
	CBlockingBackEnd*                      m_pBlockingBackend;

	// members used for special blocking backend fallback handling
	JobManager::SInfoBlock** m_pRegularWorkerFallbacks;
	uint32                   m_nRegularWorkerThreads;
};

// the implementation of the PC backend
// has n-worker threads which use atomic operations to pull from the job queue
// and uses a semaphore to signal the workers if there is work requiered
class CBlockingBackEnd : public IBackend
{
public:
	CBlockingBackEnd(JobManager::SInfoBlock** pRegularWorkerFallbacks, uint32 nRegularWorkerThreads);
	virtual ~CBlockingBackEnd();

	bool           Init(uint32 nSysMaxWorker);
	bool           ShutDown();
	void           Update() {}

	virtual void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock);

	virtual uint32 GetNumWorkerThreads() const { return m_nNumWorker; }

	void           AddBlockingFallbackJob(JobManager::SInfoBlock* pInfoBlock, uint32 nWorkerThreadID);

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	JobManager::IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const { return m_pBackEndWorkerProfiler; }
#endif

private:
	friend class JobManager::CJobManager;

	JobManager::SJobQueue_BlockingBackEnd m_JobQueue;                   // job queue node where jobs are pushed into and from
	CryFastSemaphore                      m_Semaphore;                  // semaphore to count available jobs, to allow the workers to go sleeping instead of spinning when no work is requiered
	CBlockingBackEndWorkerThread**        m_pWorkerThreads;             // worker threads for blocking backend
	uint8 m_nNumWorker;                                                 // number of allocated worker threads

	// members used for special blocking backend fallback handling
	JobManager::SInfoBlock** m_pRegularWorkerFallbacks;
	uint32                   m_nRegularWorkerThreads;

	// members required for profiling jobs in the frame profiler
#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	JobManager::IWorkerBackEndProfiler* m_pBackEndWorkerProfiler;
#endif
};

} // namespace BlockingBackEnd
} // namespace JobManager

#endif // BLOCKING_BACKEND_H_
