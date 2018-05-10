// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   implementation of job manager
   DMA memory mappings can be issued in any order
 */
#include "StdAfx.h"
#include "JobManager.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryThreading/IJobManager.h>

#include "FallbackBackend/FallBackBackend.h"
#include "PCBackEnd/ThreadBackEnd.h"
#include "BlockingBackend/BlockingBackEnd.h"

#include "../System.h"
#include "../CPUDetect.h"

#include <3rdParty/concqueue/concqueue.hpp>

namespace JobManager {
namespace Detail {

/////////////////////////////////////////////////////////////////////////////
// functions to convert between an index and a semaphore handle by salting
// the index with a special bit (bigger than the max index), this is requiered
// to have a index of 0, but have all semaphore handles != 0
// a TSemaphoreHandle needs to be 16 bytes, it shared one word (4 byte) with the number of jobs running in a syncvar
enum { nSemaphoreSaltBit = 0x8000 };    // 2^15 (highest bit)

static TSemaphoreHandle IndexToSemaphoreHandle(uint32 nIndex)           { return nIndex | nSemaphoreSaltBit; }
static uint32           SemaphoreHandleToIndex(TSemaphoreHandle handle) { return handle & ~nSemaphoreSaltBit; }

} // namespace Detail
} // namespace JobManager

///////////////////////////////////////////////////////////////////////////////
JobManager::CWorkerBackEndProfiler::CWorkerBackEndProfiler()
	: m_nCurBufIndex(0)
{
	m_WorkerStatsInfo.m_pWorkerStats = 0;
}

///////////////////////////////////////////////////////////////////////////////
JobManager::CWorkerBackEndProfiler::~CWorkerBackEndProfiler()
{
	if (m_WorkerStatsInfo.m_pWorkerStats)
		CryModuleMemalignFree(m_WorkerStatsInfo.m_pWorkerStats);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::Init(uint16 numWorkers)
{
	// Init Job Stats
	ZeroMemory(m_JobStatsInfo.m_pJobStats, sizeof(m_JobStatsInfo.m_pJobStats));

	// Init Worker Stats
	for (uint32 i = 0; i < JobManager::detail::eJOB_FRAME_STATS; ++i)
	{
		m_WorkerStatsInfo.m_nStartTime[i] = 0;
		m_WorkerStatsInfo.m_nEndTime[i] = 0;
	}
	m_WorkerStatsInfo.m_nNumWorkers = numWorkers;

	if (m_WorkerStatsInfo.m_pWorkerStats)
		CryModuleMemalignFree(m_WorkerStatsInfo.m_pWorkerStats);

	int nWorkerStatsBufSize = sizeof(JobManager::SWorkerStats) * numWorkers * JobManager::detail::eJOB_FRAME_STATS;
	m_WorkerStatsInfo.m_pWorkerStats = (JobManager::SWorkerStats*)CryModuleMemalign(nWorkerStatsBufSize, 128);
	ZeroMemory(m_WorkerStatsInfo.m_pWorkerStats, nWorkerStatsBufSize);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::Update()
{
	Update(IWorkerBackEndProfiler::GetTimeSample());
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::Update(const uint32 curTimeSample)
{
	// End current state's time period
	m_WorkerStatsInfo.m_nEndTime[m_nCurBufIndex] = curTimeSample;

	// Get next buffer index
	uint8 nNextIndex = (m_nCurBufIndex + 1);
	nNextIndex = (nNextIndex > (JobManager::detail::eJOB_FRAME_STATS - 1)) ? 0 : nNextIndex;

	// Reset next buffer slot and start its time period
	ResetWorkerStats(nNextIndex, curTimeSample);
	ResetJobStats(nNextIndex);

	// Advance buffer index
	m_nCurBufIndex = nNextIndex;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStats(JobManager::CWorkerFrameStats& rStats) const
{
	uint8 nTailIndex = (m_nCurBufIndex + 1);
	nTailIndex = (nTailIndex > (JobManager::detail::eJOB_FRAME_STATS - 1)) ? 0 : nTailIndex;

	// Get worker stats from tail
	GetWorkerStats(nTailIndex, rStats);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStats(TJobFrameStatsContainer& rJobStats, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const
{
	uint8 nTailIndex = (m_nCurBufIndex + 1);
	nTailIndex = (nTailIndex > (JobManager::detail::eJOB_FRAME_STATS - 1)) ? 0 : nTailIndex;

	// Get job info from tail
	GetJobStats(nTailIndex, rJobStats, jobSortOrder);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStats(JobManager::CWorkerFrameStats& rStats, TJobFrameStatsContainer& rJobStats, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const
{
	uint8 nTailIndex = (m_nCurBufIndex + 1);
	nTailIndex = (nTailIndex > (JobManager::detail::eJOB_FRAME_STATS - 1)) ? 0 : nTailIndex;

	// Get worker stats from tail
	GetWorkerStats(nTailIndex, rStats);

	// Get job info from tail
	GetJobStats(nTailIndex, rJobStats, jobSortOrder);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStatsSummary(SWorkerFrameStatsSummary& rStats) const
{
	uint8 nTailIndex = (m_nCurBufIndex + 1);
	nTailIndex = (nTailIndex > (JobManager::detail::eJOB_FRAME_STATS - 1)) ? 0 : nTailIndex;

	// Calculate percentage range multiplier for this frame
	// Take absolute time delta to handle microsecond time sample counter overfilling
	int32 nSamplePeriod = abs((int)m_WorkerStatsInfo.m_nEndTime[nTailIndex] - (int)m_WorkerStatsInfo.m_nStartTime[nTailIndex]);
	const float nMultiplier = (1.0f / (float)nSamplePeriod) * 100.0f;

	// Accumulate stats
	uint32 totalExecutionTime = 0;
	uint32 totalNumJobsExecuted = 0;
	const JobManager::SWorkerStats* pWorkerStatsOffset = &m_WorkerStatsInfo.m_pWorkerStats[nTailIndex * m_WorkerStatsInfo.m_nNumWorkers];

	for (uint8 i = 0; i < m_WorkerStatsInfo.m_nNumWorkers; ++i)
	{
		const JobManager::SWorkerStats& workerStats = pWorkerStatsOffset[i];
		totalExecutionTime += workerStats.nExecutionPeriod;
		totalNumJobsExecuted += workerStats.nNumJobsExecuted;
	}

	rStats.nSamplePeriod = nSamplePeriod;
	rStats.nNumActiveWorkers = (uint8)m_WorkerStatsInfo.m_nNumWorkers;
	rStats.nAvgUtilPerc = ((float)totalExecutionTime / (float)m_WorkerStatsInfo.m_nNumWorkers) * nMultiplier;
	rStats.nTotalExecutionPeriod = totalExecutionTime;
	rStats.nTotalNumJobsExecuted = totalNumJobsExecuted;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStatsSummary(SJobFrameStatsSummary& rStats) const
{
	uint8 nTailIndex = (m_nCurBufIndex + 1);
	nTailIndex = (nTailIndex > (JobManager::detail::eJOB_FRAME_STATS - 1)) ? 0 : nTailIndex;

	const JobManager::SJobFrameStats* pJobStatsToCopyFrom = &m_JobStatsInfo.m_pJobStats[nTailIndex * JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS];

	// Accumulate job stats
	uint16 totalIndividualJobCount = 0;
	uint32 totalJobsExecutionTime = 0;
	uint32 totalJobCount = 0;
	for (uint16 i = 0; i < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS; ++i)
	{
		const JobManager::SJobFrameStats& rJobStats = pJobStatsToCopyFrom[i];
		if (rJobStats.count > 0)
		{
			totalJobsExecutionTime += rJobStats.usec;
			totalJobCount += rJobStats.count;
			totalIndividualJobCount++;
		}
	}

	rStats.nTotalExecutionTime = totalJobsExecutionTime;
	rStats.nNumJobsExecuted = totalJobCount;
	rStats.nNumIndividualJobsExecuted = totalIndividualJobCount;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::RegisterJob(const uint32 jobId, const char* jobName)
{
	CRY_ASSERT_MESSAGE(jobId < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS,
	                   string().Format("JobManager::CWorkerBackEndProfiler::RegisterJob: Limit for current max supported jobs reached. Current limit: %u. Increase JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS limit."
	                                   , JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS));

	for (uint8 i = 0; i < JobManager::detail::eJOB_FRAME_STATS; ++i)
		m_JobStatsInfo.m_pJobStats[(i* JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS) +jobId].cpName = jobName;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::RecordJob(const uint16 profileIndex, const uint8 workerId, const uint32 jobId, const uint32 runTimeMicroSec)
{
	CRY_ASSERT_MESSAGE(workerId < m_WorkerStatsInfo.m_nNumWorkers, string().Format("JobManager::CWorkerBackEndProfiler::RecordJob: workerId is out of scope. workerId:%u , scope:%u"
	                                                                               , workerId, m_WorkerStatsInfo.m_nNumWorkers));

	JobManager::SJobFrameStats& jobStats = m_JobStatsInfo.m_pJobStats[(profileIndex* JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS) +jobId];
	JobManager::SWorkerStats& workerStats = m_WorkerStatsInfo.m_pWorkerStats[profileIndex * m_WorkerStatsInfo.m_nNumWorkers + workerId];

	// Update job stats
	uint32 nCount = ~0;
	do
	{
		nCount = *const_cast<volatile uint32*>(&jobStats.count);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&jobStats.count), nCount + 1, nCount) != nCount);

	uint32 nUsec = ~0;
	do
	{
		nUsec = *const_cast<volatile uint32*>(&jobStats.usec);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&jobStats.usec), nUsec + runTimeMicroSec, nUsec) != nUsec);

	// Update worker stats
	uint32 threadExcutionTime = ~0;
	do
	{
		threadExcutionTime = *const_cast<volatile uint32*>(&workerStats.nExecutionPeriod);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&workerStats.nExecutionPeriod), threadExcutionTime + runTimeMicroSec, threadExcutionTime) != threadExcutionTime);

	uint32 numJobsExecuted = ~0;
	do
	{
		numJobsExecuted = *const_cast<volatile uint32*>(&workerStats.nNumJobsExecuted);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&workerStats.nNumJobsExecuted), numJobsExecuted + 1, numJobsExecuted) != numJobsExecuted);
}

///////////////////////////////////////////////////////////////////////////////
uint16 JobManager::CWorkerBackEndProfiler::GetProfileIndex() const
{
	return m_nCurBufIndex;
}

///////////////////////////////////////////////////////////////////////////////
uint32 JobManager::CWorkerBackEndProfiler::GetNumWorkers() const
{
	return m_WorkerStatsInfo.m_nNumWorkers;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetWorkerStats(const uint8 nBufferIndex, JobManager::CWorkerFrameStats& rWorkerStats) const
{
	assert(rWorkerStats.numWorkers <= m_WorkerStatsInfo.m_nNumWorkers);

	// Calculate percentage range multiplier for this frame
	// Take absolute time delta to handle microsecond time sample counter overfilling
	uint32 nSamplePeriode = max(m_WorkerStatsInfo.m_nEndTime[nBufferIndex] - m_WorkerStatsInfo.m_nStartTime[nBufferIndex], (uint32)1);
	const float nMultiplier = nSamplePeriode ? (1.0f / fabsf(static_cast<float>(nSamplePeriode))) * 100.0f : 0.f;
	JobManager::SWorkerStats* pWorkerStatsOffset = &m_WorkerStatsInfo.m_pWorkerStats[nBufferIndex * m_WorkerStatsInfo.m_nNumWorkers];

	rWorkerStats.numWorkers = (uint8)m_WorkerStatsInfo.m_nNumWorkers;
	rWorkerStats.nSamplePeriod = nSamplePeriode;

	for (uint8 i = 0; i < m_WorkerStatsInfo.m_nNumWorkers; ++i)
	{
		// Get previous frame's stats
		JobManager::SWorkerStats& workerStats = pWorkerStatsOffset[i];
		if (workerStats.nExecutionPeriod > 0)
		{
			rWorkerStats.workerStats[i].nUtilPerc = (float)workerStats.nExecutionPeriod * nMultiplier;
			rWorkerStats.workerStats[i].nExecutionPeriod = workerStats.nExecutionPeriod;
			rWorkerStats.workerStats[i].nNumJobsExecuted = workerStats.nNumJobsExecuted;
		}
		else
		{
			ZeroMemory(&rWorkerStats.workerStats[i], sizeof(CWorkerFrameStats::SWorkerStats));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetJobStats(const uint8 nBufferIndex, TJobFrameStatsContainer& rJobStatsContainer, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const
{
	const JobManager::SJobFrameStats* pJobStatsToCopyFrom = &m_JobStatsInfo.m_pJobStats[nBufferIndex * JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS];

	// Clear and ensure size
	rJobStatsContainer.clear();
	rJobStatsContainer.reserve(JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS);

	// Copy job stats
	uint16 curCount = 0;
	for (uint16 i = 0; i < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS; ++i)
	{
		const JobManager::SJobFrameStats& rJobStats = pJobStatsToCopyFrom[i];
		if (rJobStats.count > 0)
		{
			rJobStatsContainer.push_back(rJobStats);
		}
	}

	// Sort job stats
	switch (jobSortOrder)
	{
	case JobManager::IWorkerBackEndProfiler::eJobSortOrder_TimeHighToLow:
		std::sort(rJobStatsContainer.begin(), rJobStatsContainer.end(), SJobFrameStats::sort_time_high_to_low);
		break;
	case JobManager::IWorkerBackEndProfiler::eJobSortOrder_TimeLowToHigh:
		std::sort(rJobStatsContainer.begin(), rJobStatsContainer.end(), SJobFrameStats::sort_time_low_to_high);
		break;
	case JobManager::IWorkerBackEndProfiler::eJobSortOrder_Lexical:
		std::sort(rJobStatsContainer.begin(), rJobStatsContainer.end(), SJobFrameStats::sort_lexical);
		break;
	case JobManager::IWorkerBackEndProfiler::eJobSortOrder_NoSort:
		break;
	default:
		CRY_ASSERT_MESSAGE(false, "Unsupported type");
	}
	;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::ResetWorkerStats(const uint8 nBufferIndex, const uint32 curTimeSample)
{
	ZeroMemory(&m_WorkerStatsInfo.m_pWorkerStats[nBufferIndex * m_WorkerStatsInfo.m_nNumWorkers], sizeof(JobManager::SWorkerStats) * m_WorkerStatsInfo.m_nNumWorkers);
	m_WorkerStatsInfo.m_nStartTime[nBufferIndex] = curTimeSample;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::ResetJobStats(const uint8 nBufferIndex)
{
	JobManager::SJobFrameStats* pJobStatsToReset = &m_JobStatsInfo.m_pJobStats[nBufferIndex * JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS];

	// Reset job stats
	uint16 curCount = 0;
	for (uint16 i = 0; i < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS; ++i)
	{
		JobManager::SJobFrameStats& rJobStats = pJobStatsToReset[i];
		rJobStats.Reset();
	}
}

///////////////////////////////////////////////////////////////////////////////
JobManager::CJobManager* JobManager::CJobManager::Instance()
{
	static JobManager::CJobManager _singleton;
	return &_singleton;
}

extern "C"
{
	JobManager::IJobManager* GetJobManagerInterface()
	{
		return JobManager::CJobManager::Instance();
	}
}

JobManager::CJobManager::CJobManager()
	: m_Initialized(false),
	m_pFallBackBackEnd(NULL),
	m_pThreadBackEnd(NULL),
	m_pBlockingBackEnd(NULL),
	m_nJobIdCounter(0),
	m_nJobSystemEnabled(1),
	m_bJobSystemProfilerPaused(0),
	m_bJobSystemProfilerEnabled(false),
	m_nJobsRunCounter(0),
	m_nFallbackJobsRunCounter(0),
	m_bSuspendWorkerForMP(false)
{
	// create backends
	m_pThreadBackEnd = new ThreadBackEnd::CThreadBackEnd();

#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	m_nRegularWorkerThreads = 6;
#else
	CCpuFeatures* pCPU = new CCpuFeatures;
	pCPU->Detect();
	int numWorkers = (int)pCPU->GetLogicalCPUCount() - 1;
	if (numWorkers < 0)
		numWorkers = 0;
	//m_nRegularWorkerThreads = pCPU->GetLogicalCPUCount();
	m_nRegularWorkerThreads = numWorkers;
	delete pCPU;
#endif

	m_pRegularWorkerFallbacks = new JobManager::SInfoBlock*[m_nRegularWorkerThreads];
	memset(m_pRegularWorkerFallbacks, 0, sizeof(JobManager::SInfoBlock*) * m_nRegularWorkerThreads);

	m_pBlockingBackEnd = CryAlignedNew<BlockingBackEnd::CBlockingBackEnd>(m_pRegularWorkerFallbacks, m_nRegularWorkerThreads);
	m_pFallBackBackEnd = new FallBackBackEnd::CFallBackBackEnd();

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	m_profilingData.nFrameIdx = 0;
#endif

	memset(m_arrJobInvokers, 0, sizeof(m_arrJobInvokers));
	m_nJobInvokerIdx = 0;

	// init fallback backend early to be able to handle jobs before jobmanager is initialized
	if (m_pFallBackBackEnd)  m_pFallBackBackEnd->Init(-1 /*not used for fallback*/);
}

const bool JobManager::CJobManager::WaitForJob(JobManager::SJobState& rJobState) const
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	SJobProfilingData* pJobProfilingData = gEnv->GetJobManager()->GetProfilingData(rJobState.nProfilerIndex);
	pJobProfilingData->nWaitBegin = gEnv->pTimer->GetAsyncTime();
	pJobProfilingData->nThreadId = CryGetCurrentThreadId();
#endif

	rJobState.syncVar.Wait();

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	pJobProfilingData->nWaitEnd = gEnv->pTimer->GetAsyncTime();
	pJobProfilingData = NULL;
#endif

	return true;
}

ColorB JobManager::CJobManager::GenerateColorBasedOnName(const char* name)
{
	ColorB color;

	uint32 hash = CCrc32::Compute(name);
	color.r = static_cast<uint8>(hash);
	color.g = static_cast<uint8>(hash >> 8);
	color.b = static_cast<uint8>(hash >> 16);

	return color;
}

const JobManager::TJobHandle JobManager::CJobManager::GetJobHandle(const char* cpJobName, const uint32 cStrLen, JobManager::Invoker pInvoker)
{
	static JobManager::SJobStringHandle cFailedLookup = { "", 0 };
	JobManager::SJobStringHandle cLookup = { cpJobName, cStrLen };
	bool bJobIdSet = false;

	// mis-use the JobQueue lock, this shouldn't create contention,
	// since this functions is only called once per job
	AUTO_LOCK(m_JobManagerLock);

	// don't insert in list when we only look up the job for debugging settings
	if (pInvoker == NULL)
	{
		std::set<JobManager::SJobStringHandle>::iterator it = m_registeredJobs.find(cLookup);
		return it == m_registeredJobs.end() ? &cFailedLookup : (JobManager::TJobHandle)&(*(it));
	}

	std::pair<std::set<JobManager::SJobStringHandle>::iterator, bool> it = m_registeredJobs.insert(cLookup);
	JobManager::TJobHandle ret = (JobManager::TJobHandle)&(*(it.first));

	bJobIdSet = !it.second;

	// generate color for each entry
	if (it.second)
	{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
		m_JobColors[cLookup] = GenerateColorBasedOnName(cpJobName);
#endif
		m_arrJobInvokers[m_nJobInvokerIdx] = pInvoker;
		ret->nJobInvokerIdx = m_nJobInvokerIdx;
		m_nJobInvokerIdx += 1;
		assert(m_nJobInvokerIdx < CRY_ARRAY_COUNT(m_arrJobInvokers));
	}

	if (!bJobIdSet)
	{
		ret->jobId = m_nJobIdCounter;
		m_nJobIdCounter++;
	}

	return ret;
}

const char* JobManager::CJobManager::GetJobName(JobManager::Invoker invoker)
{

	uint32 idx = ~0;
	// find the index for this invoker function
	for (size_t i = 0; i < m_nJobInvokerIdx; ++i)
	{
		if (m_arrJobInvokers[i] == invoker)
		{
			idx = i;
			break;
		}
	}
	if (idx == ~0)
		return "JobNotFound";

	// now search for thix idx in all registered jobs
	for (std::set<JobManager::SJobStringHandle>::iterator it = m_registeredJobs.begin(), end = m_registeredJobs.end(); it != end; ++it)
	{
		if (it->nJobInvokerIdx == idx)
			return it->cpString;
	}

	return "JobNotFound";
}

void JobManager::CJobManager::AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle)
{
	char job_info[128];
	cry_sprintf(job_info, "AddJob_%s", cJobHandle->cpString);
	CryProfile::detail::SetProfilingEvent(0, job_info);

	JobManager::SInfoBlock infoBlock;

	// Test if the job should be invoked
	bool bUseJobSystem = m_nJobSystemEnabled ? CJobManager::InvokeAsJob(cJobHandle) : false;

	// get producer/consumer queue settings
	JobManager::SProdConsQueueBase* cpQueue = crJob.GetQueue();
	const bool cNoQueue = (cpQueue == NULL);

	const uint32 cOrigParamSize = crJob.GetParamDataSize();
	const uint8 cParamSize = cOrigParamSize >> 4;

	//reset info block
	unsigned int flagSet = cNoQueue ? 0 : (unsigned int)JobManager::SInfoBlock::scHasQueue;

	infoBlock.pQueue = cpQueue;
	infoBlock.nflags = (unsigned char)(flagSet);
	infoBlock.paramSize = cParamSize;
	infoBlock.jobInvoker = crJob.GetGenericDelegator();
	infoBlock.jobLambdaInvoker = crJob.GetLambda();
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	infoBlock.profilerIndex = crJob.GetProfilingDataIndex();
#endif

	if (cNoQueue && crJob.GetJobState())
	{
		infoBlock.SetJobState(crJob.GetJobState());
		crJob.SetRunning();
	}

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	SJobProfilingData* pJobProfilingData = gEnv->GetJobManager()->GetProfilingData(infoBlock.profilerIndex);
	pJobProfilingData->jobHandle = cJobHandle;
#endif

	// == dispatch to the right BackEnd == //
	IF (crJob.IsBlocking() == false && (bUseJobSystem == false || m_Initialized == false), 0)
		return static_cast<FallBackBackEnd::CFallBackBackEnd*>(m_pFallBackBackEnd)->FallBackBackEnd::CFallBackBackEnd::AddJob(crJob, cJobHandle, infoBlock);

	IF (m_pBlockingBackEnd && crJob.IsBlocking(), 0)
		return static_cast<BlockingBackEnd::CBlockingBackEnd*>(m_pBlockingBackEnd)->BlockingBackEnd::CBlockingBackEnd::AddJob(crJob, cJobHandle, infoBlock);

	// default case is the threadbackend
	if (m_pThreadBackEnd)
		return static_cast<ThreadBackEnd::CThreadBackEnd*>(m_pThreadBackEnd)->ThreadBackEnd::CThreadBackEnd::AddJob(crJob, cJobHandle, infoBlock);

	// last resort - fallback backend
	return static_cast<FallBackBackEnd::CFallBackBackEnd*>(m_pFallBackBackEnd)->FallBackBackEnd::CFallBackBackEnd::AddJob(crJob, cJobHandle, infoBlock);
}

void JobManager::CJobManager::AddLambdaJob(const char* jobName, const std::function<void()>& callback, TPriorityLevel priority, SJobState* pJobState)
{
	CJobLambda job(jobName, callback);
	job.SetPriorityLevel(priority);
	if (pJobState)
		job.RegisterJobState(pJobState);
	job.Run();
}

void JobManager::CJobManager::ShutDown()
{
	if (m_pFallBackBackEnd) m_pFallBackBackEnd->ShutDown();
	if (m_pThreadBackEnd) m_pThreadBackEnd->ShutDown();
	if (m_pBlockingBackEnd) m_pBlockingBackEnd->ShutDown();
}

void JobManager::CJobManager::Init(uint32 nSysMaxWorker)
{
	// only init once
	if (m_Initialized)
		return;

	m_Initialized = true;

	// initialize the backends for this platform
	if (m_pThreadBackEnd)
	{
		if (!m_pThreadBackEnd->Init(nSysMaxWorker))
		{
			delete m_pThreadBackEnd;
			m_pThreadBackEnd = NULL;
		}
	}
	if (m_pBlockingBackEnd)    m_pBlockingBackEnd->Init(1);
}

bool JobManager::CJobManager::InvokeAsJob(const JobManager::TJobHandle cJobHandle) const
{
	const char* cpJobName = cJobHandle->cpString;
	return this->CJobManager::InvokeAsJob(cpJobName);
}

bool JobManager::CJobManager::InvokeAsJob(const char* cpJobName) const
{
#if defined(_RELEASE)
	return true; // no support for fallback interface in release
#endif

	// try to find the jobname in the job filter list
	IF (m_pJobFilter, 0)
	{
		if (const char* p = strstr(m_pJobFilter, cpJobName))
			if (p == m_pJobFilter || p[-1] == ',')
			{
				p += strlen(cpJobName);
				if (*p == 0 || *p == ',')
					return false;
			}
	}

	return m_nJobSystemEnabled != 0;
}

uint32 JobManager::CJobManager::GetWorkerThreadId() const
{
	return JobManager::detail::GetWorkerThreadId();
}

JobManager::SJobProfilingData* JobManager::CJobManager::GetProfilingData(uint16 nProfilerIndex)
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	uint32 nFrameIdx = (nProfilerIndex & 0xC000) >> 14; // frame index is encoded in the top two bits
	uint32 nProfilingDataEntryIndex = (nProfilerIndex & ~0xC000);

	IF (nProfilingDataEntryIndex >= SJobProfilingDataContainer::nCapturedEntriesPerFrame, 0)
		return &m_profilingData.m_DymmyProfilingData;

	JobManager::SJobProfilingData* pProfilingData = &m_profilingData.arrJobProfilingData[nFrameIdx][nProfilingDataEntryIndex];
	return pProfilingData;
#else
	return NULL;
#endif
}

uint16 JobManager::CJobManager::ReserveProfilingData()
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	uint32 nFrameIdx = m_profilingData.GetFillFrameIdx();
	uint32 nProfilingDataEntryIndex = CryInterlockedIncrement((volatile int*)&m_profilingData.nProfilingDataCounter[nFrameIdx]);

	// encore nFrameIdx in top two bits
	if (nProfilingDataEntryIndex >= SJobProfilingDataContainer::nCapturedEntriesPerFrame)
	{
		//printf("Out of Profiling entries\n");
		nProfilingDataEntryIndex = 16383;
	}
	assert(nFrameIdx <= 4);
	assert(nProfilingDataEntryIndex <= 16383);

	uint16 nProfilerIndex = (nFrameIdx << 14) | nProfilingDataEntryIndex;

	return nProfilerIndex;
#else
	return ~0;
#endif
}

// struct to contain profling entries
struct SOrderedProfilingData
{
	SOrderedProfilingData(const ColorB& rColor, int beg, int end) :
		color(rColor), nBegin(beg), nEnd(end){}

	ColorB color;   // color of the entry
	int    nBegin;  // begin offset from graph
	int    nEnd;    // end offset from graph
};

void MyDraw2dLabel(float x, float y, float font_size, const float* pfColor, bool bCenter, const char* label_text, ...) PRINTF_PARAMS(6, 7);
void MyDraw2dLabel(float x, float y, float font_size, const float* pfColor, bool bCenter, const char* label_text, ...)
{
	va_list args;
	va_start(args, label_text);
	IRenderAuxText::DrawText(Vec3(x, y, 1.0f), font_size, pfColor, eDrawText_2D | eDrawText_FixedSize | eDrawText_IgnoreOverscan | eDrawText_Monospace, label_text, args);
	va_end(args);
}

// free function to make the profiling rendering code more compact/readable
namespace DrawUtils
{
void AddToGraph(ColorB* pGraph, const SOrderedProfilingData& rProfilingData)
{
	for (int i = rProfilingData.nBegin; i < rProfilingData.nEnd; ++i)
	{
		pGraph[i] = rProfilingData.color;
	}
}

void Draw2DBox(float fX, float fY, float fHeigth, float fWidth, const ColorB& rColor, float fScreenHeigth, float fScreenWidth, IRenderAuxGeom* pAuxRenderer)
{

	float fPosition[4][2] = {
		{ fX,          fY           },
		{ fX,          fY + fHeigth },
		{ fX + fWidth, fY + fHeigth },
		{ fX + fWidth, fY           }
	};

	// compute normalized position from absolute points
	Vec3 vPosition[4] = {
		Vec3(fPosition[0][0], fPosition[0][1], 0.0f),
		Vec3(fPosition[1][0], fPosition[1][1], 0.0f),
		Vec3(fPosition[2][0], fPosition[2][1], 0.0f),
		Vec3(fPosition[3][0], fPosition[3][1], 0.0f)
	};

	vtx_idx const anTriangleIndices[6] = {
		0, 1, 2,
		0, 2, 3
	};

	pAuxRenderer->DrawTriangles(vPosition, 4, anTriangleIndices, 6, rColor);
}

void Draw2DBoxOutLine(float fX, float fY, float fHeigth, float fWidth, const ColorB& rColor, float fScreenHeigth, float fScreenWidth, IRenderAuxGeom* pAuxRenderer)
{
	float fPosition[4][2] = {
		{ fX - 1.0f,          fY - 1.0f           },
		{ fX - 1.0f,          fY + fHeigth + 1.0f },
		{ fX + fWidth + 1.0f, fY + fHeigth + 1.0f },
		{ fX + fWidth + 1.0f, fY - 1.0f           }
	};

	// compute normalized position from absolute points
	Vec3 vPosition[4] = {
		Vec3(fPosition[0][0], fPosition[0][1], 0.0f),
		Vec3(fPosition[1][0], fPosition[1][1], 0.0f),
		Vec3(fPosition[2][0], fPosition[2][1], 0.0f),
		Vec3(fPosition[3][0], fPosition[3][1], 0.0f)
	};

	pAuxRenderer->DrawLine(vPosition[0], rColor, vPosition[1], rColor);
	pAuxRenderer->DrawLine(vPosition[1], rColor, vPosition[2], rColor);
	pAuxRenderer->DrawLine(vPosition[2], rColor, vPosition[3], rColor);
	pAuxRenderer->DrawLine(vPosition[3], rColor, vPosition[0], rColor);
}

void DrawGraph(ColorB* pGraph, int nGraphSize, float fBaseX, float fbaseY, float fHeight, float fScreenHeigth, float fScreenWidth, IRenderAuxGeom* pAuxRenderer)
{
	for (int i = 0; i < nGraphSize; )
	{
		// skip empty fields
		if (pGraph[i].r == 0 && pGraph[i].g == 0 && pGraph[i].b == 0)
		{
			++i;
			continue;
		}

		float fBegin = (float)i;
		ColorB color = pGraph[i];

		while (true)
		{
			if ((i + 1 >= nGraphSize) ||      // reached end of graph
			    (pGraph[i + 1] != pGraph[i])) // start of different region
			{
				// draw box for this graph part
				float fX = fBaseX + fBegin;
				float fY = fbaseY;
				float fEnd = (float)i;
				float fWidth = fEnd - fBegin;   // compute width
				DrawUtils::Draw2DBox(fX, fY, fHeight, fWidth, pGraph[i], fScreenHeigth, fScreenWidth, pAuxRenderer);
				++i;
				break;
			}

			++i;   // still the same graph, go to next entry
		}
	}
}

void WriteShortLabel(float fTextSideOffset, float fTopOffset, float fTextSize, float* fTextColor, const char* tmpBuffer, int nCapChars)
{
	char textBuffer[512] = { 0 };
	char* pDst = textBuffer;
	char* pEnd = textBuffer + nCapChars;   // keep space for tailing '\0'
	const char* pSrc = tmpBuffer;
	while (*pSrc != '\0' && pDst < pEnd)
	{
		*pDst = *pSrc;
		++pDst;
		++pSrc;
	}
	MyDraw2dLabel(fTextSideOffset, fTopOffset, fTextSize, fTextColor, false, "%s", textBuffer);
}

} // namespace DrawUtils

struct SJobProflingRenderData
{
	const char* pName;      // name of the job
	ColorB      color;      // color to use to represent this job

	CTimeValue  runTime;
	CTimeValue  waitTime;

	CTimeValue  cacheTime;
	CTimeValue  codePagingTime;
	CTimeValue  fnResolveTime;
	CTimeValue  memtransferSyncTime;

	uint32      invocations;

	bool operator<(const SJobProflingRenderData& rOther) const
	{
		return (INT_PTR)pName < (INT_PTR)rOther.pName;
	}

	bool operator<(const char* pOther) const
	{
		return (INT_PTR)pName < (INT_PTR)pOther;
	}
};

struct SJobProflingRenderDataCmp
{
	bool operator()(const SJobProflingRenderData& rA, const char* pB) const                   { return (INT_PTR)rA.pName < (INT_PTR)pB; }
	bool operator()(const SJobProflingRenderData& rA, const SJobProflingRenderData& rB) const { return (INT_PTR)rA.pName < (INT_PTR)rB.pName; }
	bool operator()(const char* pA, const SJobProflingRenderData& rB) const                   { return (INT_PTR)pA < (INT_PTR)rB.pName; }
};

struct SJobProfilingLexicalSort
{
	bool operator()(const SJobProflingRenderData& rA, const SJobProflingRenderData& rB) const
	{
		return strcmp(rA.pName, rB.pName) < 0;
	}

};

struct SWorkerProfilingRenderData
{
	CTimeValue runTime;
};

struct SRegionTime
{
	JobManager::CJobManager::SMarker::TMarkerString pName;
	ColorB     color;
	CTimeValue executionTime;
	bool       bIsMainThread;

	bool operator==(const SRegionTime& rOther) const
	{
		return strcmp(pName.c_str(), rOther.pName.c_str()) == 0;
	}
	bool operator<(const SRegionTime& rOther) const
	{
		return strcmp(pName.c_str(), rOther.pName.c_str()) < 0;
	}
};

struct SRegionLexicalSorter
{
	bool operator()(const SRegionTime& rA, const SRegionTime& rB) const
	{
		// sort highest times first
		return strcmp(rA.pName.c_str(), rB.pName.c_str()) < 0;
	}
};

struct SThreadProflingRenderData
{
	CTimeValue executionTime;
	CTimeValue waitTime;
};

void JobManager::CJobManager::Update(int nJobSystemProfiler)
{
#if 0 // integrate into profiler after fixing it's memory issues
	float fColorGreen[4] = { 0, 1, 0, 1 };
	float fColorRed[4] = { 1, 0, 0, 1 };
	uint32 nJobsRunCounter = m_nJobsRunCounter;
	uint32 nFallbackJobsRunCounter = m_nFallbackJobsRunCounter;
	IRenderAuxText::Draw2dLabel(1, 5.0f, 1.3f, nFallbackJobsRunCounter ? fColorRed : fColorGreen, false, "Jobs Submitted %d, FallbackJobs %d", nJobsRunCounter, nFallbackJobsRunCounter);
#endif
	m_nJobsRunCounter = 0;
	m_nFallbackJobsRunCounter = 0;

	// Listen for keyboard input if enabled
	if (m_bJobSystemProfilerEnabled != nJobSystemProfiler && gEnv->pInput)
	{
		if (nJobSystemProfiler)
			gEnv->pInput->AddEventListener(this);
		else
			gEnv->pInput->RemoveEventListener(this);

		m_bJobSystemProfilerEnabled = nJobSystemProfiler;
	}

	// profiler disabled
	if (nJobSystemProfiler == 0)
		return;

	AUTO_LOCK(m_JobManagerLock);

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	int nFrameId = m_profilingData.GetRenderFrameIdx();

	CTimeValue frameStartTime = m_FrameStartTime[nFrameId];
	CTimeValue frameEndTime = m_FrameStartTime[m_profilingData.GetPrevFrameIdx()];

	// skip first frames still we have enough data
	if (frameStartTime.GetValue() == 0)
		return;

	// compute how long the displayed frame took
	CTimeValue diffTime = frameEndTime - frameStartTime;

	// get used thread ids
	threadID nMainThreadId, nRenderThreadId;
	gEnv->pRenderer->GetThreadIDs(nMainThreadId, nRenderThreadId);

	// now compute the relative screen size, and how many pixels are represented by a time value
	int nScreenHeight = gEnv->pRenderer->GetOverlayHeight();
	int nScreenWidth  = gEnv->pRenderer->GetOverlayWidth();

	float fScreenHeight = (float)nScreenHeight;
	float fScreenWidth = (float)nScreenWidth;

	float fTextSize = 1.1f;
	float fTextSizePixel = 8.0f * fTextSize;
	float fTextCharWidth = 6.0f * fTextSize;

	float fTopOffset = fScreenHeight * 0.01f;                       // keep 0.1% screenspace at top
	float fTextSideOffset = fScreenWidth * 0.01f;                   // start text rendering after 0.1% of screen width
	float fGraphSideOffset = fTextSideOffset + 15 * fTextCharWidth; // leave enough space for 15 characters before drawing the graphs

	float fInfoBoxSize = fTextCharWidth * 35;
	float fGraphHeight = fTextSizePixel;
	float fGraphWidth = (fScreenWidth - fInfoBoxSize) * 0.70f; // 70%

	float pixelPerTime = (float)fGraphWidth / diffTime.GetValue();

	const int nNumWorker = m_pThreadBackEnd->GetNumWorkerThreads();
	const int nNumJobs = m_registeredJobs.size();
	const int nGraphSize = (int)fGraphWidth;
	int nNumRegions = m_nMainThreadMarkerIndex[nFrameId] + m_nRenderThreadMarkerIndex[nFrameId];

	ColorB boxBorderColor(128, 128, 128, 0);
	float fTextColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	// allocate structure on the stack to prevent too many costly memory allocations
	//first structures to represent the graph data, we just use (0,0,0) as not set
	// and set each field with the needed color, then we render the resulting boxes
	// in one pass
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * arrMainThreadRegion = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
	memset(arrMainThreadRegion, 0, nGraphSize * sizeof(ColorB));
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * arrRenderThreadRegion = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
	memset(arrRenderThreadRegion, 0, nGraphSize * sizeof(ColorB));
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * arrMainThreadWaitRegion = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
	memset(arrMainThreadWaitRegion, 0, nGraphSize * sizeof(ColorB));
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * arrRenderThreadWaitRegion = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
	memset(arrRenderThreadWaitRegion, 0, nGraphSize * sizeof(ColorB));

	PREFAST_SUPPRESS_WARNING(6255)
	SRegionTime * arrRegionProfilingData = (SRegionTime*)alloca(nNumRegions * sizeof(SRegionTime));
	memset(arrRegionProfilingData, 0, nNumRegions * sizeof(SRegionTime));

	// accumulate region time for overview
	int nRegionCounter = 0;
	for (uint32 i = 0; i < m_nMainThreadMarkerIndex[nFrameId]; ++i)
	{
		if (m_arrMainThreadMarker[nFrameId][i].type == SMarker::POP_MARKER)
			continue;

		arrRegionProfilingData[nRegionCounter].pName = m_arrMainThreadMarker[nFrameId][i].marker;
		arrRegionProfilingData[nRegionCounter].color = ColorB();
		arrRegionProfilingData[nRegionCounter].bIsMainThread = m_arrMainThreadMarker[nFrameId][i].bIsMainThread;
		++nRegionCounter;
	}
	for (uint32 i = 0; i < m_nRenderThreadMarkerIndex[nFrameId]; ++i)
	{
		if (m_arrRenderThreadMarker[nFrameId][i].type == SMarker::POP_MARKER)
			continue;

		arrRegionProfilingData[nRegionCounter].pName = m_arrRenderThreadMarker[nFrameId][i].marker;
		arrRegionProfilingData[nRegionCounter].color = ColorB();
		arrRegionProfilingData[nRegionCounter].bIsMainThread = m_arrRenderThreadMarker[nFrameId][i].bIsMainThread;
		++nRegionCounter;
	}

	// remove duplicates of region entries
	std::sort(arrRegionProfilingData, arrRegionProfilingData + nRegionCounter, SRegionLexicalSorter());
	SRegionTime* pEnd = std::unique(arrRegionProfilingData, arrRegionProfilingData + nRegionCounter);
	nNumRegions = (int)(pEnd - arrRegionProfilingData);

	// get region colors
	for (int i = 0; i < nNumRegions; ++i)
	{
		SRegionTime& rRegionData = arrRegionProfilingData[i];
		rRegionData.color = GetRegionColor(rRegionData.pName);
	}

	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * *arrWorkerThreadsRegions = (ColorB**)alloca(nNumWorker * sizeof(ColorB * *));

	for (int i = 0; i < nNumWorker; ++i)
	{
		PREFAST_SUPPRESS_WARNING(6263) PREFAST_SUPPRESS_WARNING(6255)
		arrWorkerThreadsRegions[i] = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
		memset(arrWorkerThreadsRegions[i], 0, nGraphSize * sizeof(ColorB));
	}

	// allocate per worker data
	PREFAST_SUPPRESS_WARNING(6255)
	SWorkerProfilingRenderData * arrWorkerProfilingRenderData = (SWorkerProfilingRenderData*)alloca(nNumWorker * sizeof(SWorkerProfilingRenderData));
	memset(arrWorkerProfilingRenderData, 0, nNumWorker * sizeof(SWorkerProfilingRenderData));

	// allocate per job informations
	PREFAST_SUPPRESS_WARNING(6255)
	SJobProflingRenderData * arrJobProfilingRenderData = (SJobProflingRenderData*)alloca(nNumJobs * sizeof(SJobProflingRenderData));
	memset(arrJobProfilingRenderData, 0, nNumJobs * sizeof(SJobProflingRenderData));

	// init job data
	int nJobIndex = 0;
	for (std::set<JobManager::SJobStringHandle>::const_iterator it = m_registeredJobs.begin(); it != m_registeredJobs.end(); ++it)
	{
		arrJobProfilingRenderData[nJobIndex].pName = it->cpString;
		arrJobProfilingRenderData[nJobIndex].color = m_JobColors[*it];
		++nJobIndex;
	}

	std::sort(arrJobProfilingRenderData, arrJobProfilingRenderData + nNumJobs);

	CTimeValue waitTimeMainThread;
	CTimeValue waitTimeRenderThread;

	// ==== collect per job/thread times == //
	for (int j = 0; j < 2; ++j)
	{
		// select the right frame for the pass
		int nIdx = (j == 0 ? m_profilingData.GetPrevRenderFrameIdx() : m_profilingData.GetRenderFrameIdx());
		for (uint32 i = 0; i < m_profilingData.nProfilingDataCounter[nIdx] && i < SJobProfilingDataContainer::nCapturedEntriesPerFrame; ++i)
		{
			SJobProfilingData profilingData = m_profilingData.arrJobProfilingData[nIdx][i];

			// skip invalid entries
			IF (profilingData.jobHandle == NULL, 0)
				continue;

			// skip jobs which did never run
			IF (profilingData.nEndTime.GetValue() == 0 || profilingData.nStartTime.GetValue() == 0, 0)
				continue;

			// get the job profiling rendering data structure
			SJobProflingRenderData* pJobProfilingRenderingData =
			  std::lower_bound(arrJobProfilingRenderData, arrJobProfilingRenderData + nNumJobs, profilingData.jobHandle->cpString, SJobProflingRenderDataCmp());

			// did part of the job run during this frame
			if ((profilingData.nEndTime <= frameEndTime && profilingData.nStartTime >= frameStartTime) ||                                            // in this frame
			    (profilingData.nEndTime >= frameStartTime && profilingData.nEndTime <= frameEndTime && profilingData.nStartTime < frameStartTime) || // from the last frame into this
			    (profilingData.nStartTime >= frameStartTime && profilingData.nStartTime <= frameEndTime && profilingData.nEndTime > frameEndTime))   // goes into the next frame
			{
				// clamp frame start/end into this frame
				profilingData.nEndTime = (profilingData.nEndTime > frameEndTime ? frameEndTime : profilingData.nEndTime);
				profilingData.nStartTime = (profilingData.nStartTime < frameStartTime ? frameStartTime : profilingData.nStartTime);

				// compute integer offset in the graph for start and stop position
				CTimeValue startOffset = profilingData.nStartTime - frameStartTime;
				CTimeValue endOffset = profilingData.nEndTime - frameStartTime;

				int nGraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
				int nGraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);

				// get the correct worker id
				uint32 nWorkerIdx = profilingData.nWorkerThread;

				// accumulate the time spend in dispatch(time the job waited to run)
				pJobProfilingRenderingData->runTime += (profilingData.nEndTime - profilingData.nStartTime);

				// count job invocations
				pJobProfilingRenderingData->invocations += 1;

				// accumulate time per worker thread
				arrWorkerProfilingRenderData[nWorkerIdx].runTime += (profilingData.nEndTime - profilingData.nStartTime);
				if (nGraphOffsetEnd < nGraphSize)
					DrawUtils::AddToGraph(arrWorkerThreadsRegions[nWorkerIdx], SOrderedProfilingData(pJobProfilingRenderingData->color, nGraphOffsetStart, nGraphOffsetEnd));
			}

			// did this job have wait time in this frame
			if (((profilingData.nWaitEnd.GetValue() - profilingData.nWaitBegin.GetValue()) > 1) &&
			    ((profilingData.nWaitEnd <= frameEndTime && profilingData.nWaitBegin >= frameStartTime) ||                                            // in this frame
			     (profilingData.nWaitEnd >= frameStartTime && profilingData.nWaitEnd <= frameEndTime && profilingData.nWaitBegin < frameStartTime) || // from the last frame into this
			     (profilingData.nWaitBegin >= frameStartTime && profilingData.nWaitBegin <= frameEndTime && profilingData.nWaitEnd > frameEndTime)))  // goes into the next frame
			{
				// clamp frame start/end into this frame
				profilingData.nWaitEnd = (profilingData.nWaitEnd > frameEndTime ? frameEndTime : profilingData.nWaitEnd);
				profilingData.nWaitBegin = (profilingData.nWaitBegin < frameStartTime ? frameStartTime : profilingData.nWaitBegin);

				// accumulate wait time
				pJobProfilingRenderingData->waitTime += (profilingData.nWaitEnd - profilingData.nWaitBegin);

				// compute graph offsets
				CTimeValue startOffset = profilingData.nWaitBegin - frameStartTime;
				CTimeValue endOffset = profilingData.nWaitEnd - frameStartTime;

				int nGraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
				int nGraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);

				// add to the right thread(we care only for main and renderthread)
				if (profilingData.nThreadId == nMainThreadId)
				{
					if (nGraphOffsetEnd < nGraphSize)
						DrawUtils::AddToGraph(arrMainThreadWaitRegion, SOrderedProfilingData(pJobProfilingRenderingData->color, nGraphOffsetStart, nGraphOffsetEnd));
					waitTimeMainThread += (profilingData.nWaitEnd - profilingData.nWaitBegin);
				}
				else if (profilingData.nThreadId == nRenderThreadId)
				{
					if (nGraphOffsetEnd < nGraphSize)
						DrawUtils::AddToGraph(arrRenderThreadWaitRegion, SOrderedProfilingData(pJobProfilingRenderingData->color, nGraphOffsetStart, nGraphOffsetEnd));
					waitTimeRenderThread += (profilingData.nWaitEnd - profilingData.nWaitBegin);
				}
			}
		}
	}

	// ==== collect mainthread regions ==== //
	if (m_nMainThreadMarkerIndex[nFrameId])
	{
		uint32 nStackPos = 0;
		PREFAST_SUPPRESS_WARNING(6255)
		SMarker * pStack = (SMarker*)alloca(m_nMainThreadMarkerIndex[nFrameId] * sizeof(SMarker));
		for (uint32 i = 0; i < m_nMainThreadMarkerIndex[nFrameId]; ++i)
			new(&pStack[i])SMarker();

		for (uint32 nInputPos = 0; nInputPos < m_nMainThreadMarkerIndex[nFrameId]; ++nInputPos)
		{
			SMarker& rCurrentMarker = m_arrMainThreadMarker[nFrameId][nInputPos];
			if (rCurrentMarker.type == SMarker::POP_MARKER && nStackPos > 0) // end of marker, pop it from the stack
			{
				CTimeValue startOffset = pStack[nStackPos - 1].time - frameStartTime;
				CTimeValue endOffset = rCurrentMarker.time - frameStartTime;

				int GraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
				int GraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);
				if (GraphOffsetEnd < nGraphSize)
					DrawUtils::AddToGraph(arrMainThreadRegion, SOrderedProfilingData(GetRegionColor(pStack[nStackPos - 1].marker), GraphOffsetStart, GraphOffsetEnd));

				// accumulate global time
				SRegionTime cmp = { pStack[nStackPos - 1].marker, ColorB(), CTimeValue(), false };
				SRegionTime* pRegionProfilingData = std::lower_bound(arrRegionProfilingData, arrRegionProfilingData + nNumRegions, cmp);
				pRegionProfilingData->executionTime += (rCurrentMarker.time - pStack[nStackPos - 1].time);

				// pop last elemnt from stack, and update parent time
				nStackPos -= 1;
				if (nStackPos > 0)
					pStack[nStackPos - 1].time = rCurrentMarker.time;
			}
			if (rCurrentMarker.type == SMarker::PUSH_MARKER)
			{
				if (nStackPos > 0) // only draw last segment if there was one
				{
					CTimeValue startOffset = pStack[nStackPos - 1].time - frameStartTime;
					CTimeValue endOffset = rCurrentMarker.time - frameStartTime;

					int GraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
					int GraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);
					if (GraphOffsetEnd < nGraphSize)
						DrawUtils::AddToGraph(arrMainThreadRegion, SOrderedProfilingData(GetRegionColor(pStack[nStackPos - 1].marker), GraphOffsetStart, GraphOffsetEnd));

					// accumulate global time
					SRegionTime cmp = { pStack[nStackPos - 1].marker, ColorB(), CTimeValue(), false };
					SRegionTime* pRegionProfilingData = std::lower_bound(arrRegionProfilingData, arrRegionProfilingData + nNumRegions, cmp);
					pRegionProfilingData->executionTime += (rCurrentMarker.time - pStack[nStackPos - 1].time);
				}

				// push marker to stack
				pStack[nStackPos++] = rCurrentMarker;
			}
		}
	}
	// ==== collect renderthread regions ==== //
	if (m_nRenderThreadMarkerIndex[nFrameId])
	{
		uint32 nStackPos = 0;
		PREFAST_SUPPRESS_WARNING(6255)
		SMarker * pStack = (SMarker*)alloca(m_nRenderThreadMarkerIndex[nFrameId] * sizeof(SMarker));
		for (uint32 i = 0; i < m_nRenderThreadMarkerIndex[nFrameId]; ++i)
			new(&pStack[i])SMarker();

		for (uint32 nInputPos = 0; nInputPos < m_nRenderThreadMarkerIndex[nFrameId]; ++nInputPos)
		{
			SMarker& rCurrentMarker = m_arrRenderThreadMarker[nFrameId][nInputPos];
			if (rCurrentMarker.type == SMarker::POP_MARKER && nStackPos > 0) // end of marker, pop it from the stack
			{
				CTimeValue startOffset = pStack[nStackPos - 1].time - frameStartTime;
				CTimeValue endOffset = rCurrentMarker.time - frameStartTime;

				int GraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
				int GraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);
				if (GraphOffsetEnd < nGraphSize)
					DrawUtils::AddToGraph(arrRenderThreadRegion, SOrderedProfilingData(GetRegionColor(pStack[nStackPos - 1].marker), GraphOffsetStart, GraphOffsetEnd));

				// accumulate global time
				SRegionTime cmp = { pStack[nStackPos - 1].marker, ColorB(), CTimeValue(), false };
				SRegionTime* pRegionProfilingData = std::lower_bound(arrRegionProfilingData, arrRegionProfilingData + nNumRegions, cmp);
				pRegionProfilingData->executionTime += (rCurrentMarker.time - pStack[nStackPos - 1].time);

				// pop last elemnt from stack, and update parent time
				nStackPos -= 1;
				if (nStackPos > 0)
					pStack[nStackPos - 1].time = rCurrentMarker.time;
			}
			if (rCurrentMarker.type == SMarker::PUSH_MARKER)
			{
				if (nStackPos > 0) // only draw last segment if there was one
				{
					CTimeValue startOffset = pStack[nStackPos - 1].time - frameStartTime;
					CTimeValue endOffset = rCurrentMarker.time - frameStartTime;

					int GraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
					int GraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);
					if (GraphOffsetEnd < nGraphSize)
						DrawUtils::AddToGraph(arrRenderThreadRegion, SOrderedProfilingData(GetRegionColor(pStack[nStackPos - 1].marker), GraphOffsetStart, GraphOffsetEnd));

					// accumulate global time
					SRegionTime cmp = { pStack[nStackPos - 1].marker, ColorB(), CTimeValue(), false };
					SRegionTime* pRegionProfilingData = std::lower_bound(arrRegionProfilingData, arrRegionProfilingData + nNumRegions, cmp);
					pRegionProfilingData->executionTime += (rCurrentMarker.time - pStack[nStackPos - 1].time);
				}

				// push marker to stack
				pStack[nStackPos++] = rCurrentMarker;
			}
		}
	}

	// ==== begin rendering of profiling data ==== //
	// render profiling data
	IRenderAuxGeom* pAuxGeomRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags const oOldFlags = pAuxGeomRenderer->GetRenderFlags();

	SAuxGeomRenderFlags oFlags(e_Def2DPublicRenderflags);
	oFlags.SetDepthTestFlag(e_DepthTestOff);
	oFlags.SetDepthWriteFlag(e_DepthWriteOff);
	oFlags.SetCullMode(e_CullModeNone);
	oFlags.SetAlphaBlendMode(e_AlphaNone);
	pAuxGeomRenderer->SetRenderFlags(oFlags);

	float fGraphTopOffset = fTopOffset;
	// == main thread == //
	// draw main thread box and label
	MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "MainThread");
	DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	DrawUtils::DrawGraph(arrMainThreadRegion, nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight / 2.0f, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	DrawUtils::DrawGraph(arrMainThreadWaitRegion, nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset + fGraphHeight / 2.0f, fGraphHeight / 2.0f, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	fGraphTopOffset += fGraphHeight + 2;

	// == render thread == //
	MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "RenderThread");
	DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	DrawUtils::DrawGraph(arrRenderThreadRegion, nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight / 2.0f, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	DrawUtils::DrawGraph(arrRenderThreadWaitRegion, nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset + fGraphHeight / 2.0f, fGraphHeight / 2.0f, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

	// == worker threads == //
	fGraphTopOffset += 2.0f * fGraphHeight; // add a little bit more spacing between mainthreads and worker
	for (int i = 0; i < nNumWorker; ++i)
	{
		// draw worker box and label
		char workerThreadName[128];
		cry_sprintf(workerThreadName, "Worker %d", i);
		MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "%s", workerThreadName);
		DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
		DrawUtils::DrawGraph(arrWorkerThreadsRegions[i], nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

		fGraphTopOffset += fGraphHeight + 2;
	}
	fGraphTopOffset += 2.0f * fGraphHeight; // add a little bit after the worker threads

	// are we only interrested in the graph and not in the values?
	if (nJobSystemProfiler == 2)
	{
		// Restore Aux Render setup
		pAuxGeomRenderer->SetRenderFlags(oOldFlags);
		return;
	}

	// == draw info boxes == //
	char tmpBuffer[512] = { 0 };
	float fInfoBoxTextOffset = fTopOffset;

	// draw worker data
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, "Workers", 27);

	fInfoBoxTextOffset += fTextSizePixel;
	CTimeValue accumulatedWorkerTime;
	for (int i = 0; i < nNumWorker; ++i)
	{
		float runTimePercent = 100.0f / (frameEndTime - frameStartTime).GetValue() * arrWorkerProfilingRenderData[i].runTime.GetValue();
		cry_sprintf(tmpBuffer, "  Worker %d: %05.2f ms %04.1f p", i, arrWorkerProfilingRenderData[i].runTime.GetMilliSeconds(), runTimePercent);
		DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 27);
		fInfoBoxTextOffset += fTextSizePixel;

		// accumulate times for all worker
		accumulatedWorkerTime += arrWorkerProfilingRenderData[i].runTime;
	}

	// draw accumulated worker time and percentage
	cry_sprintf(tmpBuffer, "-------------------------------");
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 27);
	fInfoBoxTextOffset += fTextSizePixel;
	float accRunTimePercentage = 100.0f / ((frameEndTime - frameStartTime).GetValue() * nNumWorker) * accumulatedWorkerTime.GetValue();
	cry_sprintf(tmpBuffer, "Sum: %05.2f ms %04.1f p", accumulatedWorkerTime.GetMilliSeconds(), accRunTimePercentage);
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 27);
	fInfoBoxTextOffset += fTextSizePixel;

	// draw accumulated wait times of main and renderthread
	fInfoBoxTextOffset += 2.0f * fTextSizePixel;
	cry_sprintf(tmpBuffer, "MainThread Wait %05.2f ms", waitTimeMainThread.GetMilliSeconds());
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 27);
	fInfoBoxTextOffset += fTextSizePixel;
	cry_sprintf(tmpBuffer, "RenderThread Wait %05.2f ms", waitTimeRenderThread.GetMilliSeconds());
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 27);
	fInfoBoxTextOffset += fTextSizePixel;
	cry_sprintf(tmpBuffer, "MainThread %05.2f ms", (frameEndTime - frameStartTime).GetMilliSeconds());
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 27);
	fInfoBoxTextOffset += fTextSizePixel;
	fInfoBoxTextOffset += fTextSizePixel;

	// sort regions by time
	std::sort(arrRegionProfilingData, arrRegionProfilingData + nNumRegions, SRegionLexicalSorter());

	fTopOffset = fInfoBoxTextOffset += fTextSizePixel;
	cry_sprintf(tmpBuffer, " Name                Time(MS)");
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 80);
	fInfoBoxTextOffset += 1.4f * fTextSizePixel;
	for (int i = 0; i < nNumRegions; ++i)
	{
		if (!arrRegionProfilingData[i].bIsMainThread) // for now don't write names for RT regions
			continue;

		// do we need to restart a new colum
		if (fInfoBoxTextOffset + (fTextSize * fTextSizePixel) > (fScreenHeight * 0.99f))
		{
			fInfoBoxTextOffset = fTopOffset;
			fTextSideOffset += fTextSizePixel * 25; // keep a little space between the bars
			cry_sprintf(tmpBuffer, " Name                Time(MS)");
			DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 80);
			fInfoBoxTextOffset += 1.4f * fTextSizePixel;
		}

		cry_sprintf(tmpBuffer, " %-21.21s %05.2f ", arrRegionProfilingData[i].pName.c_str(), arrRegionProfilingData[i].executionTime.GetMilliSeconds());
		DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 28);
		DrawUtils::Draw2DBox(fTextSideOffset, fInfoBoxTextOffset + 2.0f, fTextSizePixel * 1.25f, 30 * fTextCharWidth, arrRegionProfilingData[i].color, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

		fInfoBoxTextOffset += fTextSizePixel * 1.5f;
	}

	// == render per job informations == //
	float fJobInfoBoxTextOffset = fTopOffset;
	float fJobInfoBoxSideOffset = max(fTextSideOffset += fTextSizePixel * 25, fScreenWidth * 0.5f);
	float fJobInfoBoxTextWidth = fTextCharWidth * 80;

	// sort jobs by their name
	std::sort(arrJobProfilingRenderData, arrJobProfilingRenderData + nNumJobs, SJobProfilingLexicalSort());

	cry_sprintf(tmpBuffer, " JobName                  (Num Invocations) TimeExecuted(MS) TimeWait(MS) AvG(MS)");
	DrawUtils::WriteShortLabel(fJobInfoBoxSideOffset, fJobInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 80);
	fJobInfoBoxTextOffset += 1.4f * fTextSizePixel;

	for (int i = 0; i < nNumJobs; ++i)
	{
		// do we need to restart a new colum
		if (fJobInfoBoxTextOffset + (fTextSize * fTextSizePixel) > (fScreenHeight * 0.99f))
		{
			fJobInfoBoxTextOffset = fTopOffset;
			fJobInfoBoxSideOffset += fTextCharWidth * 85; // keep a little space between the bars
			cry_sprintf(tmpBuffer, " JobName                  (Num Invocations) TimeExecuted(MS) TimeWait(MS) AvG(MS)");
			DrawUtils::WriteShortLabel(fJobInfoBoxSideOffset, fJobInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 80);
			fJobInfoBoxTextOffset += 1.4f * fTextSizePixel;
		}

		SJobProflingRenderData& rJobProfilingData = arrJobProfilingRenderData[i];
		cry_sprintf(tmpBuffer, " %-35.35s (%3d):      %5.2f      %5.2f         %5.2f", rJobProfilingData.pName, rJobProfilingData.invocations,
		            rJobProfilingData.runTime.GetMilliSeconds(), rJobProfilingData.waitTime.GetMilliSeconds(),
		            rJobProfilingData.invocations ? rJobProfilingData.runTime.GetMilliSeconds() / rJobProfilingData.invocations : 0.0f);

		DrawUtils::WriteShortLabel(fJobInfoBoxSideOffset, fJobInfoBoxTextOffset - 1, fTextSize, fTextColor, tmpBuffer, 80);
		DrawUtils::Draw2DBox(fJobInfoBoxSideOffset, fJobInfoBoxTextOffset + 2.0f, fTextSizePixel * 1.25f, fJobInfoBoxTextWidth, rJobProfilingData.color, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
		fJobInfoBoxTextOffset += fTextSizePixel * 1.5f;

	}

	// Restore Aux Render setup
	pAuxGeomRenderer->SetRenderFlags(oOldFlags);

#endif
}

void JobManager::CJobManager::SetFrameStartTime(const CTimeValue& rFrameStartTime)
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	if (!m_bJobSystemProfilerPaused)
		++m_profilingData.nFrameIdx;

	int idx = m_profilingData.GetFillFrameIdx();
	m_FrameStartTime[idx] = rFrameStartTime;
	// reset profiling counter
	m_profilingData.nProfilingDataCounter[idx] = 0;

	// clear marker
	m_nMainThreadMarkerIndex[idx] = 0;
	m_nRenderThreadMarkerIndex[idx] = 0;
#endif

}

void JobManager::CJobManager::PushProfilingMarker(const char* pName)
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	// get used thread ids
	static threadID nMainThreadId = ~0;
	static threadID nRenderThreadId = ~0;
	static bool bInitialized = false;
	IF (!bInitialized, 0)
	{
		if (!gEnv->pRenderer)
			return;
		gEnv->pRenderer->GetThreadIDs(nMainThreadId, nRenderThreadId);
		bInitialized = true;
	}

	threadID nThreadID = CryGetCurrentThreadId();
	uint32 nFrameIdx = m_profilingData.GetFillFrameIdx();
	if (nThreadID == nMainThreadId && m_nMainThreadMarkerIndex[nFrameIdx] < nMarkerEntries)
		m_arrMainThreadMarker[nFrameIdx][m_nMainThreadMarkerIndex[nFrameIdx]++] = SMarker(SMarker::PUSH_MARKER, pName, gEnv->pTimer->GetAsyncTime(), true);
	if (nThreadID == nRenderThreadId && m_nRenderThreadMarkerIndex[nFrameIdx] < nMarkerEntries)
		m_arrRenderThreadMarker[nFrameIdx][m_nRenderThreadMarkerIndex[nFrameIdx]++] = SMarker(SMarker::PUSH_MARKER, pName, gEnv->pTimer->GetAsyncTime(), false);
#endif
}

void JobManager::CJobManager::PopProfilingMarker()
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	// get used thread ids
	static threadID nMainThreadId = ~0;
	static threadID nRenderThreadId = ~0;
	static bool bInitialized = false;
	IF (!bInitialized, 0)
	{
		if (!gEnv->pRenderer)
			return;
		gEnv->pRenderer->GetThreadIDs(nMainThreadId, nRenderThreadId);
		bInitialized = true;
	}

	threadID nThreadID = CryGetCurrentThreadId();
	uint32 nFrameIdx = m_profilingData.GetFillFrameIdx();
	if (nThreadID == nMainThreadId && m_nMainThreadMarkerIndex[nFrameIdx] < nMarkerEntries)
		m_arrMainThreadMarker[nFrameIdx][m_nMainThreadMarkerIndex[nFrameIdx]++] = SMarker(SMarker::POP_MARKER, gEnv->pTimer->GetAsyncTime(), true);
	if (nThreadID == nRenderThreadId && m_nRenderThreadMarkerIndex[nFrameIdx] < nMarkerEntries)
		m_arrRenderThreadMarker[nFrameIdx][m_nRenderThreadMarkerIndex[nFrameIdx]++] = SMarker(SMarker::POP_MARKER, gEnv->pTimer->GetAsyncTime(), false);
#endif
}

ColorB JobManager::CJobManager::GetRegionColor(SMarker::TMarkerString marker)
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	if (m_RegionColors.find(marker) == m_RegionColors.end())
	{
		m_RegionColors[marker] = GenerateColorBasedOnName(marker.c_str());
	}
	return m_RegionColors[marker];
#else
	return ColorB();
#endif
}

///////////////////////////////////////////////////////////////////////////////
JobManager::TSemaphoreHandle JobManager::CJobManager::AllocateSemaphore(volatile const void* pOwner)
{
	// static checks
	STATIC_CHECK(sizeof(JobManager::TSemaphoreHandle) == 2, ERROR_SIZE_OF_SEMAPHORE_HANDLE_IS_NOT_2);
	STATIC_CHECK(static_cast<int>(nSemaphorePoolSize) < JobManager::Detail::nSemaphoreSaltBit, ERROR_SEMAPHORE_POOL_IS_BIGGER_THAN_SALT_HANDLE);
	STATIC_CHECK(IsPowerOfTwoCompileTime<nSemaphorePoolSize>::IsPowerOfTwo, ERROR_SEMAPHORE_SIZE_IS_NOT_POWER_OF_TWO);

	AUTO_LOCK(m_JobManagerLock);
	int nSpinCount = 0;
	for (;; ) // normally we should never spin here, if we do, increase the semaphore pool size
	{
		if (nSpinCount > 10)
			__debugbreak(); // breaking here means that there is a logic flaw which causes not finished syncvars to be returned to the pool

		uint32 nIndex = (++m_nCurrentSemaphoreIndex) % nSemaphorePoolSize;
		SJobFinishedConditionVariable* pSemaphore = &m_JobSemaphorePool[nIndex];
		if (pSemaphore->HasOwner())
		{
			nSpinCount++;
			continue;
		}

		// set owner and increase ref counter
		pSemaphore->SetRunning();
		pSemaphore->SetOwner(pOwner);
		pSemaphore->AddRef(pOwner);
		return JobManager::Detail::IndexToSemaphoreHandle(nIndex);
	}
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CJobManager::DeallocateSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner)
{
	AUTO_LOCK(m_JobManagerLock);
	uint32 nIndex = JobManager::Detail::SemaphoreHandleToIndex(nSemaphoreHandle);
	assert(nIndex < nSemaphorePoolSize);

	SJobFinishedConditionVariable* pSemaphore = &m_JobSemaphorePool[nIndex];

	if (pSemaphore->DecRef(pOwner) == 0)
	{
		if (pSemaphore->IsRunning())
			__debugbreak();
		pSemaphore->ClearOwner(pOwner);
	}
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::CJobManager::AddRefSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner)
{
	AUTO_LOCK(m_JobManagerLock);
	uint32 nIndex = JobManager::Detail::SemaphoreHandleToIndex(nSemaphoreHandle);
	assert(nIndex < nSemaphorePoolSize);

	SJobFinishedConditionVariable* pSemaphore = &m_JobSemaphorePool[nIndex];

	return pSemaphore->AddRef(pOwner);
}

///////////////////////////////////////////////////////////////////////////////
SJobFinishedConditionVariable* JobManager::CJobManager::GetSemaphore(JobManager::TSemaphoreHandle nSemaphoreHandle, volatile const void* pOwner)
{
	uint32 nIndex = JobManager::Detail::SemaphoreHandleToIndex(nSemaphoreHandle);
	assert(nIndex < nSemaphorePoolSize);

	return &m_JobSemaphorePool[nIndex];
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CJobManager::DumpJobList()
{
	int i = 1;
	CryLogAlways("== JobManager registered Job List ==");
	for (std::set<JobManager::SJobStringHandle>::iterator it = m_registeredJobs.begin(); it != m_registeredJobs.end(); ++it)
	{
		CryLogAlways("%3d. %s", i++, it->cpString);
	}
}

//////////////////////////////////////////////////////////////////////////
bool JobManager::CJobManager::OnInputEvent(const SInputEvent& event)
{
	bool ret = false;

	// Only process keyboard input
	if (eIDT_Keyboard == event.deviceType)
	{
		// Only if key was pressed
		if (eIS_Pressed == event.state)
		{
			switch (event.keyId)
			{
			case eKI_ScrollLock: // Pause/Continue profiler data collection with Scroll Lock key
				m_bJobSystemProfilerPaused = !m_bJobSystemProfilerPaused;
				break;
			}
		}
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CJobManager::AddBlockingFallbackJob(JobManager::SInfoBlock* pInfoBlock, uint32 nWorkerThreadID)
{
	assert(m_pFallBackBackEnd);
	static_cast<BlockingBackEnd::CBlockingBackEnd*>(m_pBlockingBackEnd)->AddBlockingFallbackJob(pInfoBlock, nWorkerThreadID);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TLS_DEFINE(uint32, gWorkerThreadId);

///////////////////////////////////////////////////////////////////////////////
namespace JobManager {
namespace detail {

enum { eWorkerThreadMarker = 0x80000000 };
uint32 mark_worker_thread_id(uint32 nWorkerThreadID)      { return nWorkerThreadID | eWorkerThreadMarker; }
uint32 unmark_worker_thread_id(uint32 nWorkerThreadID)    { return nWorkerThreadID & ~eWorkerThreadMarker; }
bool   is_marked_worker_thread_id(uint32 nWorkerThreadID) { return (nWorkerThreadID & eWorkerThreadMarker) != 0; }
} // namespace detail
} // namespace JobManager

///////////////////////////////////////////////////////////////////////////////
void JobManager::detail::SetWorkerThreadId(uint32 nWorkerThreadId)
{
	TLS_SET(gWorkerThreadId, (size_t)mark_worker_thread_id(nWorkerThreadId));
}

///////////////////////////////////////////////////////////////////////////////
uint32 JobManager::detail::GetWorkerThreadId()
{
	uint32 nID = (uint32)TLS_GET(uintptr_t, gWorkerThreadId);
	return is_marked_worker_thread_id(nID) ? unmark_worker_thread_id(nID) : ~0;
}

static BoundMPMC<JobManager::SInfoBlock*> gFallbackInfoBlocks(JobManager::detail::GetFallbackJobListSize());

///////////////////////////////////////////////////////////////////////////////
void JobManager::detail::PushToFallbackJobList(JobManager::SInfoBlock* pInfoBlock)
{
	bool ret = gFallbackInfoBlocks.enqueue(pInfoBlock);
	CRY_ASSERT_MESSAGE(ret, "JobSystem: Fallback info block limit reached");
}

JobManager::SInfoBlock* JobManager::detail::PopFromFallbackJobList()
{
	JobManager::SInfoBlock* pInfoBlock = nullptr;
	return gFallbackInfoBlocks.dequeue(pInfoBlock) ? pInfoBlock : nullptr;
}
