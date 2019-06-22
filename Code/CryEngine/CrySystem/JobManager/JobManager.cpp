// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   implementation of job manager
   DMA memory mappings can be issued in any order
 */
#include "StdAfx.h"
#include "JobManager.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryThreading/IJobManager.h>

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
	// Get info from previous frame
	uint8 index = m_nCurBufIndex == 0 ? JobManager::detail::eJOB_FRAME_STATS - 1 : m_nCurBufIndex - 1;
	GetWorkerStats(index, rStats);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStats(TJobFrameStatsContainer& rJobStats, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const
{
	// Get info from previous frame
	uint8 index = m_nCurBufIndex == 0 ? JobManager::detail::eJOB_FRAME_STATS - 1 : m_nCurBufIndex - 1;
	GetJobStats(index, rJobStats, jobSortOrder);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStats(JobManager::CWorkerFrameStats& rStats, TJobFrameStatsContainer& rJobStats, IWorkerBackEndProfiler::EJobSortOrder jobSortOrder) const
{
	// Get info from previous frame
	uint8 index = m_nCurBufIndex == 0 ? JobManager::detail::eJOB_FRAME_STATS - 1 : m_nCurBufIndex - 1;
	GetWorkerStats(index, rStats);
	GetJobStats(index, rJobStats, jobSortOrder);
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CWorkerBackEndProfiler::GetFrameStatsSummary(SWorkerFrameStatsSummary& rStats) const
{
	// Get info from previous frame
	uint8 index = m_nCurBufIndex == 0 ? JobManager::detail::eJOB_FRAME_STATS - 1 : m_nCurBufIndex - 1;
	
	// Calculate percentage range multiplier for this frame
	// Take absolute time delta to handle microsecond time sample counter overfilling
	int32 nSamplePeriod = abs((int)m_WorkerStatsInfo.m_nEndTime[index] - (int)m_WorkerStatsInfo.m_nStartTime[index]);
	const float nMultiplier = (1.0f / (float)nSamplePeriod) * 100.0f;

	// Accumulate stats
	uint32 totalExecutionTime = 0;
	uint32 totalNumJobsExecuted = 0;
	const JobManager::SWorkerStats* pWorkerStatsOffset = &m_WorkerStatsInfo.m_pWorkerStats[index * m_WorkerStatsInfo.m_nNumWorkers];

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
	// Get info from previous frame
	uint8 index = m_nCurBufIndex == 0 ? JobManager::detail::eJOB_FRAME_STATS - 1 : m_nCurBufIndex - 1;
	
	const JobManager::SJobFrameStats* pJobStatsToCopyFrom = &m_JobStatsInfo.m_pJobStats[index * JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS];

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
	CryInterlockedIncrement(alias_cast<volatile int*>(&jobStats.count));
	CryInterlockedAdd(alias_cast<volatile int*>(&jobStats.usec), runTimeMicroSec);
	// Update worker stats
	CryInterlockedIncrement(alias_cast<volatile int*>(&workerStats.nNumJobsExecuted));
	CryInterlockedAdd(alias_cast<volatile int*>(&workerStats.nExecutionPeriod), runTimeMicroSec);
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

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	m_profilingData.nFrameIdx = 0;
#endif

	memset(m_arrJobInvokers, 0, sizeof(m_arrJobInvokers));
	m_nJobInvokerIdx = 0;
}

const bool JobManager::CJobManager::WaitForJob(JobManager::SJobState& rJobState) const
{
	static ICVar* isActiveWaitEnableCVar = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_job_system_worker_boost_enabled") : nullptr;
	if (!rJobState.GetSyncVar().NeedsToWait())
	{
		return true;
	}

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	SJobProfilingData* pJobProfilingData = gEnv->GetJobManager()->GetProfilingData(rJobState.GetProfilerIndex());
	pJobProfilingData->startTime = gEnv->pTimer->GetAsyncTime();
	pJobProfilingData->isWaiting = true;
	pJobProfilingData->nThreadId = CryGetCurrentThreadId();
#endif

	// Allow Main and Render Thread to process work if they hit a wait call
	const threadID curThreadId = CryGetCurrentThreadId();
	bool processJobsWhileWaiting = false;

	if (isActiveWaitEnableCVar && isActiveWaitEnableCVar->GetIVal() == 1)
	{
		if (gEnv->pRenderer)
		{
			threadID mainThreadID = 0;
			threadID renderThreadID = 0;
			gEnv->pRenderer->GetThreadIDs(mainThreadID, renderThreadID);
			processJobsWhileWaiting = (curThreadId == mainThreadID) || (curThreadId == renderThreadID);
		}
		else
		{
			processJobsWhileWaiting = (curThreadId == gEnv->mMainThreadId);
		}
	}	

	if (processJobsWhileWaiting)
	{		
		KickTempWorker();
	}

	rJobState.GetSyncVar().Wait();

	if (processJobsWhileWaiting)
	{
		StopTempWorker();
	}


#if defined(JOBMANAGER_SUPPORT_PROFILING)
	pJobProfilingData->endTime = gEnv->pTimer->GetAsyncTime();
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
	CRY_PROFILE_FUNCTION_ARG(EProfiledSubsystem::PROFILE_SYSTEM, cJobHandle->cpString)

	JobManager::SInfoBlock infoBlock;

	const uint32 cOrigParamSize = crJob.GetParamDataSize();
	const uint8 cParamSize = cOrigParamSize >> 4;

	//reset info block
	infoBlock.pJobState = nullptr;
	infoBlock.nflags = 0;
	infoBlock.paramSize = cParamSize;
	infoBlock.jobInvoker = crJob.GetGenericDelegator();
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	infoBlock.profilerIndex = crJob.GetProfilingDataIndex();
#endif

	if (crJob.GetJobState())
	{
		infoBlock.SetJobState(crJob.GetJobState());
		crJob.SetRunning();
	}

#if defined(JOBMANAGER_SUPPORT_PROFILING)
	SJobProfilingData* pJobProfilingData = gEnv->GetJobManager()->GetProfilingData(infoBlock.profilerIndex);
	pJobProfilingData->jobHandle = cJobHandle;
#endif

	// == When the job is filtered or job system is disabled, execute directly == //
	if (!InvokeAsJob(cJobHandle))
	{
		Invoker delegator = crJob.GetGenericDelegator();
		const void* pParamMem = crJob.GetJobParamData();
		{
			MEMSTAT_CONTEXT_FMT(EMemStatContextType::Other, "Job: %s", cJobHandle->cpString);
			// execute job function
			(*delegator)((void*)pParamMem);
		}

		IF(infoBlock.GetJobState(), 1)
		{
			infoBlock.GetJobState()->SetStopped();
		}
	}
	// == dispatch to the right BackEnd == //
	// thread backend is preferred
	else if (m_pThreadBackEnd && !crJob.IsBlocking())
	{
		m_pThreadBackEnd->AddJob(crJob, cJobHandle, infoBlock);
	}
	else
	{
		CRY_ASSERT(m_pBlockingBackEnd);
		m_pBlockingBackEnd->AddJob(crJob, cJobHandle, infoBlock);
	}
}

void JobManager::CJobManager::ShutDown()
{
	if (m_pThreadBackEnd) m_pThreadBackEnd->ShutDown();
	if (m_pBlockingBackEnd) m_pBlockingBackEnd->ShutDown();
	m_Initialized = false;
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
#if !defined(_RELEASE) // job filtering is only supported in non-release builds
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
#endif

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

struct SThreadProflingRenderData
{
	CTimeValue executionTime;
	CTimeValue waitTime;
};

void JobManager::CJobManager::Update(int nJobSystemProfiler)
{
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

#if defined(JOBMANAGER_SUPPORT_STATOSCOPE)
	m_pThreadBackEnd->GetBackEndWorkerProfiler()->Update();
	m_pBlockingBackEnd->GetBackEndWorkerProfiler()->Update();
#endif

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
	// round up to 5ms and convert to seconds
	diffTime = CTimeValue(5 * crymath::ceil(diffTime.GetMilliSeconds() / 5) / 1000);

	// get used thread ids
	threadID nMainThreadId, nRenderThreadId;
	gEnv->pRenderer->GetThreadIDs(nMainThreadId, nRenderThreadId);

	// now compute the relative screen size, and how many pixels are represented by a time value
	int nScreenHeight = gEnv->pRenderer->GetOverlayHeight();
	int nScreenWidth  = gEnv->pRenderer->GetOverlayWidth();

	const float fScreenHeight = (float)nScreenHeight;
	const float fScreenWidth = (float)nScreenWidth;

	const float fTextSize = 1.1f;
	const float fTextSizePixel = 8.0f * fTextSize;
	const float fTextCharWidth = 6.0f * fTextSize;

	const float fTopOffset = fScreenHeight * 0.01f;                       // keep 1% screen space at top
	const float fTextSideOffset = fScreenWidth * 0.01f;                   // start text rendering after 1% of screen width
	const float fGraphSideOffset = fTextSideOffset + 15 * fTextCharWidth; // leave enough space for 15 characters before drawing the graphs

	const float fInfoBoxSize = fTextCharWidth * 35;
	const float fGraphHeight = fTextSizePixel;
	const float fGraphWidth = (fScreenWidth - fInfoBoxSize) * 0.70f; // 70%

	const float pixelPerTime = (float)fGraphWidth / diffTime.GetValue();

	const int nNumWorker = m_pThreadBackEnd->GetNumWorkerThreads();
	const int numBlockingWorkers = m_pBlockingBackEnd->GetNumWorkerThreads();
	const int nNumJobs = m_registeredJobs.size();
	const int nGraphSize = (int)fGraphWidth;

	ColorB boxBorderColor(128, 128, 128, 0);
	float fTextColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	// allocate structure on the stack to prevent too many costly memory allocations
	// first structures to represent the graph data, we just use (0,0,0) as not set
	// and set each field with the needed color, then we render the resulting boxes
	// in one pass
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * arrMainThreadWaitRegion = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
	memset(arrMainThreadWaitRegion, 0, nGraphSize * sizeof(ColorB));
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * arrRenderThreadWaitRegion = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
	memset(arrRenderThreadWaitRegion, 0, nGraphSize * sizeof(ColorB));
	
	// allocate graphs for worker threads
	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * *arrWorkerThreadsRegions = (ColorB**)alloca(nNumWorker * sizeof(ColorB * *));
	for (int i = 0; i < nNumWorker; ++i)
	{
		PREFAST_SUPPRESS_WARNING(6263) PREFAST_SUPPRESS_WARNING(6255)
		arrWorkerThreadsRegions[i] = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
		memset(arrWorkerThreadsRegions[i], 0, nGraphSize * sizeof(ColorB));
	}

	PREFAST_SUPPRESS_WARNING(6255)
	ColorB * *arrBlockingWorkerThreadsRegions = (ColorB**)alloca(numBlockingWorkers * sizeof(ColorB * *));
	for (int i = 0; i < numBlockingWorkers; ++i)
	{
		PREFAST_SUPPRESS_WARNING(6263) PREFAST_SUPPRESS_WARNING(6255)
		arrBlockingWorkerThreadsRegions[i] = (ColorB*)alloca(nGraphSize * sizeof(ColorB));
		memset(arrBlockingWorkerThreadsRegions[i], 0, nGraphSize * sizeof(ColorB));
	}

	// allocate per worker data
	PREFAST_SUPPRESS_WARNING(6255)
	SWorkerProfilingRenderData * arrWorkerProfilingRenderData = (SWorkerProfilingRenderData*)alloca(nNumWorker * sizeof(SWorkerProfilingRenderData));
	memset(arrWorkerProfilingRenderData, 0, nNumWorker * sizeof(SWorkerProfilingRenderData));

	PREFAST_SUPPRESS_WARNING(6255)
	SWorkerProfilingRenderData * arrBlockingWorkerProfilingRenderData = (SWorkerProfilingRenderData*)alloca(numBlockingWorkers * sizeof(SWorkerProfilingRenderData));
	memset(arrBlockingWorkerProfilingRenderData, 0, numBlockingWorkers * sizeof(SWorkerProfilingRenderData));
	
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
			IF (profilingData.endTime.GetValue() == 0 || profilingData.startTime.GetValue() == 0, 0)
				continue;

			// get the job profiling rendering data structure
			SJobProflingRenderData* pJobProfilingRenderingData =
			  std::lower_bound(arrJobProfilingRenderData, arrJobProfilingRenderData + nNumJobs, profilingData.jobHandle->cpString, SJobProflingRenderDataCmp());

			// did part of the job run during this frame?
			// i.e. did not start after this frame or ended before it
			if (!(profilingData.startTime >= frameEndTime || profilingData.endTime <= frameStartTime))
			{
				// clamp frame start/end into this frame
				profilingData.endTime = (profilingData.endTime > frameEndTime ? frameEndTime : profilingData.endTime);
				profilingData.startTime = (profilingData.startTime < frameStartTime ? frameStartTime : profilingData.startTime);

				// accumulate wait time
				if(profilingData.isWaiting)
					pJobProfilingRenderingData->waitTime += (profilingData.endTime - profilingData.startTime);
				
				// compute integer offset in the graph for start and stop position
				CTimeValue startOffset = profilingData.startTime - frameStartTime;
				CTimeValue endOffset = profilingData.endTime - frameStartTime;

				int nGraphOffsetStart = (int)(startOffset.GetValue() * pixelPerTime);
				int nGraphOffsetEnd = (int)(endOffset.GetValue() * pixelPerTime);

				if(profilingData.isWaiting)
				{
					// add to the right thread(we care only for main and render thread)
					if (profilingData.nThreadId == nMainThreadId)
					{
						if (nGraphOffsetEnd < nGraphSize)
							DrawUtils::AddToGraph(arrMainThreadWaitRegion, SOrderedProfilingData(pJobProfilingRenderingData->color, nGraphOffsetStart, nGraphOffsetEnd));
						waitTimeMainThread += (profilingData.endTime - profilingData.startTime);
					}
					else if (profilingData.nThreadId == nRenderThreadId)
					{
						if (nGraphOffsetEnd < nGraphSize)
							DrawUtils::AddToGraph(arrRenderThreadWaitRegion, SOrderedProfilingData(pJobProfilingRenderingData->color, nGraphOffsetStart, nGraphOffsetEnd));
						waitTimeRenderThread += (profilingData.endTime - profilingData.startTime);
					}
				}
				else
				{
					// accumulate the time spend in dispatch(time the job waited to run)
					pJobProfilingRenderingData->runTime += (profilingData.endTime - profilingData.startTime);

					// count job invocations
					pJobProfilingRenderingData->invocations += 1;

					SWorkerProfilingRenderData& renderData = BlockingBackEnd::IsBlockingWorkerId(profilingData.nWorkerThread)
						? arrBlockingWorkerProfilingRenderData[BlockingBackEnd::GetIndexFromWorkerId(profilingData.nWorkerThread)]
						: arrWorkerProfilingRenderData[profilingData.nWorkerThread];

					ColorB* graph = BlockingBackEnd::IsBlockingWorkerId(profilingData.nWorkerThread)
						? arrBlockingWorkerThreadsRegions[BlockingBackEnd::GetIndexFromWorkerId(profilingData.nWorkerThread)]
						: arrWorkerThreadsRegions[profilingData.nWorkerThread];

					// accumulate time per worker thread
					renderData.runTime += (profilingData.endTime - profilingData.startTime);
					if (nGraphOffsetEnd < nGraphSize)
						DrawUtils::AddToGraph(graph, SOrderedProfilingData(pJobProfilingRenderingData->color, nGraphOffsetStart, nGraphOffsetEnd));
				}
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
	MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "Main (Wait)");
	DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	DrawUtils::DrawGraph(arrMainThreadWaitRegion, nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

	{ // marker for the end of the main thread
		ColorB red(255, 0, 0, 0);
		const float markerOffset = (m_mainDoneTime[nFrameId] - frameStartTime).GetValue() * pixelPerTime;
		Vec3 markerTop(fInfoBoxSize + fGraphSideOffset + markerOffset, fGraphTopOffset - 0.5f * fGraphHeight, 0);
		Vec3 markerBottom(fInfoBoxSize + fGraphSideOffset + markerOffset, fGraphTopOffset + 1.5f * fGraphHeight, 0);
		pAuxGeomRenderer->DrawLine(markerTop, red, markerBottom, red);
	}
	fGraphTopOffset += fGraphHeight + 2;

	// == render thread == //
	MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "Render (Wait)");
	DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
	DrawUtils::DrawGraph(arrRenderThreadWaitRegion, nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

	{ // marker for the end of the render thread
		ColorB red(255, 0, 0, 0);
		const float markerOffset = (m_renderDoneTime[nFrameId] - frameStartTime).GetValue() * pixelPerTime;
		Vec3 markerTop(fInfoBoxSize + fGraphSideOffset + markerOffset, fGraphTopOffset - 0.5f * fGraphHeight, 0);
		Vec3 markerBottom(fInfoBoxSize + fGraphSideOffset + markerOffset, fGraphTopOffset + 1.5f * fGraphHeight, 0);
		pAuxGeomRenderer->DrawLine(markerTop, red, markerBottom, red);
	}

	// == worker threads == //
	fGraphTopOffset += 2.0f * fGraphHeight; // add a little bit more spacing between mainthreads and worker
	for (int i = 0; i < nNumWorker - 2; ++i)
	{
		MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "Worker %d", i);
		DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
		DrawUtils::DrawGraph(arrWorkerThreadsRegions[i], nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

		fGraphTopOffset += fGraphHeight + 2;
	}

	fGraphTopOffset += fGraphHeight; 
	// helper workers -- we're assuming that it's the last two!
	for (int i = nNumWorker - 2; i < nNumWorker; ++i)
	{
		MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "Worker %d (H)", i);
		DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
		DrawUtils::DrawGraph(arrWorkerThreadsRegions[i], nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

		fGraphTopOffset += fGraphHeight + 2;
	}

	fGraphTopOffset += fGraphHeight;
	for (int i = 0; i < numBlockingWorkers; ++i)
	{
		MyDraw2dLabel(fInfoBoxSize + fTextSideOffset, fGraphTopOffset, fTextSize, fTextColor, false, "Worker(B) %d", i);
		DrawUtils::Draw2DBoxOutLine(fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fGraphWidth, boxBorderColor, fScreenHeight, fScreenWidth, pAuxGeomRenderer);
		DrawUtils::DrawGraph(arrBlockingWorkerThreadsRegions[i], nGraphSize, fInfoBoxSize + fGraphSideOffset, fGraphTopOffset, fGraphHeight, fScreenHeight, fScreenWidth, pAuxGeomRenderer);

		fGraphTopOffset += fGraphHeight + 2;
	}

	// draw vertical line every 5ms
	const int64 tickOf5Ms = CTimeValue(0.005f).GetValue();
	const int64 markerCount = ((diffTime.GetMilliSecondsAsInt64() - 1) / 5);
	for(int64 i = 1; i <= markerCount; ++i)
	{
		Vec3 markerTop(fInfoBoxSize + fGraphSideOffset + i * tickOf5Ms * pixelPerTime, fTopOffset - fGraphHeight, 0);
		Vec3 markerBottom(fInfoBoxSize + fGraphSideOffset + i * tickOf5Ms * pixelPerTime, fGraphTopOffset + fGraphHeight, 0);
		pAuxGeomRenderer->DrawLine(markerTop, boxBorderColor, markerBottom, boxBorderColor);
	}

	// are we only interested in the graph and not in the values?
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
	CTimeValue accumulatedWorkerTime;
	for (int i = 0; i < nNumWorker; ++i)
	{
		float runTimePercent = 100.0f / (frameEndTime - frameStartTime).GetValue() * arrWorkerProfilingRenderData[i].runTime.GetValue();
		cry_sprintf(tmpBuffer, "Worker %d:    %5.2f ms %5.1f %%", i, arrWorkerProfilingRenderData[i].runTime.GetMilliSeconds(), runTimePercent);
		DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
		fInfoBoxTextOffset += fTextSizePixel;

		// accumulate times for all worker
		accumulatedWorkerTime += arrWorkerProfilingRenderData[i].runTime;
	}
	for (int i = 0; i < numBlockingWorkers; ++i)
	{
		float runTimePercent = 100.0f / (frameEndTime - frameStartTime).GetValue() * arrBlockingWorkerProfilingRenderData[i].runTime.GetValue();
		cry_sprintf(tmpBuffer, "Worker(B) %d: %5.2f ms %5.1f %%", i, arrBlockingWorkerProfilingRenderData[i].runTime.GetMilliSeconds(), runTimePercent);
		DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
		fInfoBoxTextOffset += fTextSizePixel;

		// accumulate times for all worker
		accumulatedWorkerTime += arrWorkerProfilingRenderData[i].runTime;
	}

	// draw accumulated worker time and percentage
	cry_sprintf(tmpBuffer, "--------------------------------");
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
	fInfoBoxTextOffset += fTextSizePixel;
	float accRunTimePercentage = 100.0f / ((frameEndTime - frameStartTime).GetValue() * nNumWorker) * accumulatedWorkerTime.GetValue();
	cry_sprintf(tmpBuffer, "Sum:         %5.2f ms %5.1f %%", accumulatedWorkerTime.GetMilliSeconds(), accRunTimePercentage);
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
	fInfoBoxTextOffset += fTextSizePixel;

	// draw accumulated wait times of main and render thread
	fInfoBoxTextOffset += 2.0f * fTextSizePixel;
	cry_sprintf(tmpBuffer, "Frame Time  %5.2f ms", (frameEndTime - frameStartTime).GetMilliSeconds());
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
	fInfoBoxTextOffset += fTextSizePixel;
	cry_sprintf(tmpBuffer, "Main Wait   %5.2f ms", waitTimeMainThread.GetMilliSeconds());
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
	fInfoBoxTextOffset += fTextSizePixel;
	cry_sprintf(tmpBuffer, "Render Wait %5.2f ms", waitTimeRenderThread.GetMilliSeconds());
	DrawUtils::WriteShortLabel(fTextSideOffset, fInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 30);
	fInfoBoxTextOffset += fTextSizePixel;
	fInfoBoxTextOffset += fTextSizePixel;

	// == render per job information == //
	float fJobInfoBoxTextOffset = fInfoBoxTextOffset + fTextSizePixel;
	float fJobInfoBoxSideOffset = fTextSideOffset;
	float fJobInfoBoxTextWidth  = fTextCharWidth * 80;

	// sort jobs by their name
	std::sort(arrJobProfilingRenderData, arrJobProfilingRenderData + nNumJobs, SJobProfilingLexicalSort());

	cry_sprintf(tmpBuffer, " JobName                  Num Invocations TimeExecuted(MS) TimeWait(MS) AvG(MS)");
	DrawUtils::WriteShortLabel(fJobInfoBoxSideOffset, fJobInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 80);
	fJobInfoBoxTextOffset += 1.4f * fTextSizePixel;

	for (int i = 0; i < nNumJobs; ++i)
	{
		// do we need to start a new column
		if (fJobInfoBoxTextOffset + (fTextSize * fTextSizePixel) > (fScreenHeight * 0.99f))
		{
			fJobInfoBoxTextOffset = fTopOffset;
			fJobInfoBoxSideOffset += fTextCharWidth * 85; // keep a little space between the bars
			cry_sprintf(tmpBuffer, " JobName                  Num Invocations TimeExecuted(MS) TimeWait(MS) Avg(MS)");
			DrawUtils::WriteShortLabel(fJobInfoBoxSideOffset, fJobInfoBoxTextOffset, fTextSize, fTextColor, tmpBuffer, 80);
			fJobInfoBoxTextOffset += 1.4f * fTextSizePixel;
		}

		SJobProflingRenderData& rJobProfilingData = arrJobProfilingRenderData[i];
		cry_sprintf(tmpBuffer, " %-35.35s %3d      %5.2f          %5.2f     %5.2f", rJobProfilingData.pName, rJobProfilingData.invocations,
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

void JobManager::CJobManager::SetMainDoneTime(const CTimeValue& time)
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	m_mainDoneTime[m_profilingData.GetFillFrameIdx()] = time;
#endif
}

void JobManager::CJobManager::SetRenderDoneTime(const CTimeValue &time)
{
#if defined(JOBMANAGER_SUPPORT_PROFILING)
	m_renderDoneTime[m_profilingData.GetFillFrameIdx()] = time;
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
#if !defined(EXCLUDE_NORMAL_LOG)
	int i = 1;
	CryLogAlways("== JobManager registered Job List ==");
	for (std::set<JobManager::SJobStringHandle>::iterator it = m_registeredJobs.begin(); it != m_registeredJobs.end(); ++it)
	{
		CryLogAlways("%3d. %s", i++, it->cpString);
	}
#endif
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
void JobManager::CJobManager::KickTempWorker() const
{
	CRY_ASSERT(m_pThreadBackEnd);
	m_pThreadBackEnd->KickTempWorker();
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::CJobManager::StopTempWorker() const
{
	CRY_ASSERT(m_pThreadBackEnd);
	m_pThreadBackEnd->StopTempWorker();
}

JobManager::CJobManager::~CJobManager()
{
	delete m_pThreadBackEnd;
	CryAlignedDelete(m_pBlockingBackEnd);
}

JobManager::IBackend* JobManager::CJobManager::GetBackEnd(JobManager::EBackEndType backEndType)
{
	switch (backEndType)
	{
	case eBET_Thread:
		return m_pThreadBackEnd;
	case eBET_Blocking:
		return m_pBlockingBackEnd;
	default:
		CRY_ASSERT_MESSAGE(0, "Unsupported EBackEndType encountered.");
		__debugbreak();
		return nullptr;
	}

	return nullptr;
}

uint32 JobManager::CJobManager::GetNumWorkerThreads() const
{
	return m_pThreadBackEnd ? m_pThreadBackEnd->GetNumWorkerThreads() : 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
thread_local uint32 tls_workerThreadId = 0;

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
	tls_workerThreadId = mark_worker_thread_id(nWorkerThreadId);
}

///////////////////////////////////////////////////////////////////////////////
uint32 JobManager::detail::GetWorkerThreadId()
{
	return is_marked_worker_thread_id(tls_workerThreadId) ? unmark_worker_thread_id(tls_workerThreadId) : s_nonWorkerThreadId;
}