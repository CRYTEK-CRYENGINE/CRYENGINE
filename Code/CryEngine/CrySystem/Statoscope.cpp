// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Statoscope.cpp
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Statoscope.h"
#include "FrameProfileSystem.h"
#include <CryGame/IGameFramework.h>
#include <CryRenderer/IRenderer.h>
#include <CryAnimation/ICryAnimation.h>
#include "SimpleStringPool.h"
#include "System.h"
#include "ThreadProfiler.h"
#include <CryThreading/IThreadManager.h>
#include <CrySystem/Scaleform/IScaleformHelper.h>
#include <CryParticleSystem/IParticlesPfx2.h>

#include "StatoscopeStreamingIntervalGroup.h"
#include "StatoscopeTextureStreamingIntervalGroup.h"

#if ENABLE_STATOSCOPE

namespace
{
class CCompareFrameProfilersSelfTime
{
public:
	bool operator()(const std::pair<CFrameProfiler*, int64>& p1, const std::pair<CFrameProfiler*, int64>& p2)
	{
		return p1.second > p2.second;
	}
};
}

static string GetFrameProfilerPath(CFrameProfiler* pProfiler)
{
	if (pProfiler)
	{
		const char* sThreadName = gEnv->pThreadManager->GetThreadName(pProfiler->m_threadId);
		char sThreadNameBuf[11]; // 0x 12345678 \0 => 2+8+1=11
		if (!sThreadName || !sThreadName[0])
		{
			cry_sprintf(sThreadNameBuf, "%" PRI_THREADID, (pProfiler->m_threadId));
		}
		if (strstr(sThreadName, "JobSystem_Worker_") == sThreadName)
		{
			sThreadName = "JobSystem_Merged";
		}

		char sNameBuffer[256];
		cry_strcpy(sNameBuffer, pProfiler->m_name);

	#if CRY_FUNC_HAS_SIGNATURE // __FUNCTION__ only contains classname on MSVC, for other function we have __PRETTY_FUNCTION__, so we need to strip return / argument types
		{
			char* pEnd = (char*)strchr(sNameBuffer, '(');
			if (pEnd)
			{
				*pEnd = 0;
				while (*(pEnd) != ' ' && *(pEnd) != '*' && pEnd != (sNameBuffer - 1))
				{
					--pEnd;
				}
				memmove(sNameBuffer, pEnd + 1, &sNameBuffer[sizeof(sNameBuffer)] - (pEnd + 1));
			}
		}
	#endif

		string path = sThreadName ? sThreadName : sThreadNameBuf;
		path += "/";
		path += gEnv->pFrameProfileSystem->GetModuleName(pProfiler);
		path += "/";
		path += sNameBuffer;
		path += "/";

		return path;
	}
	else
	{
		return string("SmallFunctions/SmallFunction/");
	}
}

CStatoscopeDataClass::CStatoscopeDataClass(const char* format)
	: m_format(format)
	, m_numDataElements(0)
{
	uint32 numOpening = 0;
	uint32 numClosing = 0;
	uint32 numDollars = 0;
	const char* pNameStart = NULL;

	string formatString(format);
	int pathStart = formatString.find_first_of('\'');
	int pathEnd = formatString.find_first_of('\'', pathStart + 1);
	m_path = formatString.substr(pathStart + 1, pathEnd - 2);

	for (const char* c = format; c && *c != '\0'; ++c)
	{
		if (*c == '(')
		{
			++numOpening;
			pNameStart = c + 1;
		}
		else if (*c == ')')
		{
			++numClosing;

			if (pNameStart)
			{
				ProcessNewBinDataElem(pNameStart, c);
				pNameStart = NULL;
			}
		}
		else if (*c == '$')
		{
			++numDollars;
		}
	}
	if (numClosing == numOpening)
	{
		m_numDataElements = numOpening + numDollars;
	}
	else
	{
		m_numDataElements = 0;
		CryFatalError("Mismatched opening/closing braces in Statoscope format description.");
	}
}

void CStatoscopeDataClass::ProcessNewBinDataElem(const char* pStart, const char* pEnd)
{
	if (pStart)
	{
		BinDataElement newElem;

		//determine data type
		string s(pStart);
		int spaceIndex = s.find_first_of(' ');

		if (spaceIndex != -1)
		{
			string typeString = s.substr(0, spaceIndex);
			bool bBroken = false;

			if (typeString.compareNoCase("float") == 0)
			{
				newElem.type = StatoscopeDataWriter::Float;
			}
			else if (typeString.compareNoCase("int") == 0)
			{
				newElem.type = StatoscopeDataWriter::Int;
			}
			else if (typeString.compareNoCase("int64") == 0)
			{
				newElem.type = StatoscopeDataWriter::Int64;
			}
			else if (typeString.compareNoCase("string") == 0)
			{
				newElem.type = StatoscopeDataWriter::String;
			}
			else
			{
				bBroken = true;
				CryLogAlways("Broken!");
			}

			if (!bBroken)
			{
				int bracketIndex = s.find_first_of(')');
				newElem.name = s.substr(spaceIndex + 1, bracketIndex - spaceIndex - 1);

				m_binElements.push_back(newElem);
			}
		}
	}
}

void CStatoscopeDataGroup::WriteHeader(CDataWriter* pDataWriter)
{
	pDataWriter->WriteDataStr(m_dataClass.GetPath());
	int nDataElems = (int)m_dataClass.GetNumBinElements();

	pDataWriter->WriteData(nDataElems);

	for (int i = 0; i < nDataElems; i++)
	{
		const CStatoscopeDataClass::BinDataElement& elem = m_dataClass.GetBinElement(i);
		pDataWriter->WriteData(elem.type);
		pDataWriter->WriteDataStr(elem.name.c_str());
	}
}

CStatoscopeIntervalGroup::CStatoscopeIntervalGroup(uint32 id, const char* name, const char* format)
	: m_id(id)
	, m_name(name)
	, m_dataClass(format)
	, m_instLength(0)
	, m_pWriter(NULL)
{
}

void CStatoscopeIntervalGroup::Enable(CStatoscopeEventWriter* pWriter)
{
	m_pWriter = pWriter;
	Enable_Impl();
}

void CStatoscopeIntervalGroup::Disable()
{
	Disable_Impl();
	m_pWriter = NULL;
}

size_t CStatoscopeIntervalGroup::GetDescEventLength() const
{
	size_t numElements = m_dataClass.GetNumElements();
	size_t length = numElements * sizeof(uint8);

	for (size_t i = 0; i < m_dataClass.GetNumBinElements(); ++i)
	{
		const CStatoscopeDataClass::BinDataElement& elem = m_dataClass.GetBinElement(i);
		length += elem.name.length() + 1;
	}

	return length;
}

void CStatoscopeIntervalGroup::WriteDescEvent(void* p) const
{
	size_t numElements = m_dataClass.GetNumElements();

	char* pc = (char*)p;
	for (size_t i = 0; i < m_dataClass.GetNumBinElements(); ++i)
	{
		const CStatoscopeDataClass::BinDataElement& elem = m_dataClass.GetBinElement(i);
		*pc++ = (char)elem.type;
		strcpy(pc, elem.name.c_str());
		pc += elem.name.length() + 1;
	}
}

void CStatoscopeFrameRecordWriter::AddValue(float f)
{
	m_pDataWriter->WriteData(f);
	++m_nWrittenElements;
}

void CStatoscopeFrameRecordWriter::AddValue(const char* s)
{
	m_pDataWriter->WriteDataStr(s);
	++m_nWrittenElements;
}

void CStatoscopeFrameRecordWriter::AddValue(int i)
{
	m_pDataWriter->WriteData(i);
	++m_nWrittenElements;
}

struct SFrameLengthDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('f', "frame lengths", "['/' (float frameLengthInMS) (float lostProfilerTimeInMS) ]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		IFrameProfileSystem* pFrameProfileSystem = gEnv->pSystem->GetIProfileSystem();

		fr.AddValue(gEnv->pTimer->GetRealFrameTime() * 1000.0f);
		fr.AddValue(pFrameProfileSystem ? pFrameProfileSystem->GetLostFrameTimeMS() : -1.f);
	}
};

struct SMemoryDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('m', "memory", "['/Memory/' (float mainMemUsageInMB) (int vidMemUsageInMB)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		IMemoryManager::SProcessMemInfo processMemInfo;
		GetISystem()->GetIMemoryManager()->GetProcessMemInfo(processMemInfo);
		fr.AddValue(processMemInfo.PagefileUsage / (1024.f * 1024.f));

		size_t vidMem, lastVidMem;
		gEnv->pRenderer->GetVideoMemoryUsageStats(vidMem, lastVidMem);
		fr.AddValue((int)vidMem);
	}
};

struct SStreamingDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('s', "streaming", "['/Streaming/' (float cgfStreamingMemUsedInMB) (float cgfStreamingMemRequiredInMB) (int cgfStreamingPoolSize) (float tempMemInKB)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		I3DEngine::SObjectsStreamingStatus objectsStreamingStatus;
		gEnv->p3DEngine->GetObjectsStreamingStatus(objectsStreamingStatus);
		fr.AddValue(objectsStreamingStatus.nAllocatedBytes / (1024.f * 1024.f));
		fr.AddValue(objectsStreamingStatus.nMemRequired / (1024.f * 1024.f));
		fr.AddValue(objectsStreamingStatus.nMeshPoolSize);
		fr.AddValue(gEnv->pSystem->GetStreamEngine()->GetStreamingStatistics().nTempMemory / 1024.f);
	}
};

struct SStreamingAudioDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('a', "streaming audio", "['/StreamingAudio/' (float bandwidthActualKBsecond) (float bandwidthRequestedKBsecond)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		I3DEngine::SStremaingBandwidthData subsystemStreamingData;
		memset(&subsystemStreamingData, 0, sizeof(I3DEngine::SStremaingBandwidthData));
		gEnv->p3DEngine->GetStreamingSubsystemData(eStreamTaskTypeSound, subsystemStreamingData);
		fr.AddValue(subsystemStreamingData.fBandwidthActual);
		fr.AddValue(subsystemStreamingData.fBandwidthRequested);
	}
};

struct SStreamingObjectsDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('o', "streaming objects", "['/StreamingObjects/' (float bandwidthActualKBsecond) (float bandwidthRequestedKBsecond)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		I3DEngine::SStremaingBandwidthData subsystemStreamingData;
		memset(&subsystemStreamingData, 0, sizeof(I3DEngine::SStremaingBandwidthData));
		gEnv->p3DEngine->GetStreamingSubsystemData(eStreamTaskTypeGeometry, subsystemStreamingData);
		fr.AddValue(subsystemStreamingData.fBandwidthActual);
		fr.AddValue(subsystemStreamingData.fBandwidthRequested);
	}
};

struct SThreadsDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('t', "threading", "['/Threading/' (float MTLoadInMS) (float MTWaitingForRTInMS) "
		                                      "(float RTLoadInMS) (float RTWaitingForMTInMS) (float RTWaitingForGPUInMS) "
		                                      "(float RTFrameLengthInMS) (float RTSceneDrawningLengthInMS) (float NetThreadTimeInMS)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		IRenderer::SRenderTimes renderTimes;
		gEnv->pRenderer->GetRenderTimes(renderTimes);

		SNetworkPerformance netPerformance;
		if (gEnv->pNetwork)
			gEnv->pNetwork->GetPerformanceStatistics(&netPerformance);
		else
			netPerformance.m_threadTime = 0.0f;

		float RTWaitingForMTInMS = renderTimes.fWaitForMain * 1000.f;
		float MTWaitingForRTInMS = renderTimes.fWaitForRender * 1000.f;
		float RTWaitingForGPUInMS = renderTimes.fWaitForGPU * 1000.f;
		float RTLoadInMS = renderTimes.fTimeProcessedRT * 1000.f;

		float MTLoadInMS = (gEnv->pTimer->GetRealFrameTime() * 1000.0f) - MTWaitingForRTInMS;

		//Load represents pure RT work, so compensate for GPU sync
		RTLoadInMS = RTLoadInMS - RTWaitingForGPUInMS;

		fr.AddValue(MTLoadInMS);
		fr.AddValue(MTWaitingForRTInMS);
		fr.AddValue(RTLoadInMS);
		fr.AddValue(RTWaitingForMTInMS);
		fr.AddValue(RTWaitingForGPUInMS);
		fr.AddValue(renderTimes.fTimeProcessedRT * 1000);
		fr.AddValue(renderTimes.fTimeProcessedRTScene * 1000);
		fr.AddValue(netPerformance.m_threadTime * 1000.f);
	}
};

struct SSystemThreadsDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('T', "system threading", "['/SystemThreading/' "
		                                             "(float MTLoadInMS) (float RTLoadInMS) (float otherLoadInMS) "
		                                             "(float sysIdle0InMS) (float sysIdle1InMS) (float sysIdleTotalInMS) "
		                                             "(float totalLoadInMS) (float timeFrameInMS)]");
	}

	virtual void Enable()
	{
		IStatoscopeDataGroup::Enable();

		SSystemThreadsDG::StartThreadProf();
	}

	virtual void Disable()
	{
		IStatoscopeDataGroup::Disable();

		SSystemThreadsDG::StopThreadProf();
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
	#if defined(THREAD_SAMPLER)
		CSystem* pSystem = static_cast<CSystem*>(gEnv->pSystem);
		CThreadProfiler* pThreadProf = pSystem->GetThreadProfiler();
		IThreadSampler* pThreadSampler = pThreadProf ? pThreadProf->GetThreadSampler() : NULL;

		if (pThreadSampler)
		{
			pThreadSampler->Tick();

			fr.AddValue(pThreadSampler->GetExecutionTime(TT_MAIN));
			fr.AddValue(pThreadSampler->GetExecutionTime(TT_RENDER));
			fr.AddValue(pThreadSampler->GetExecutionTime(TT_OTHER));
			fr.AddValue(pThreadSampler->GetExecutionTime(TT_SYSTEM_IDLE_0));
			fr.AddValue(pThreadSampler->GetExecutionTime(TT_SYSTEM_IDLE_1));
			fr.AddValue(pThreadSampler->GetExecutionTime(TT_SYSTEM_IDLE_0) + pThreadSampler->GetExecutionTime(TT_SYSTEM_IDLE_1));
			fr.AddValue(pThreadSampler->GetExecutionTime(TT_TOTAL) / pThreadSampler->GetNumHWThreads());
			fr.AddValue(pThreadSampler->GetExecutionTimeFrame());
		}
		else
	#endif // defined(THREAD_SAMPLER)
		{
			for (uint32 i = 0; i < 8; i++)
			{
				fr.AddValue(0.0f);
			}
		}
	}

	static void StartThreadProf()
	{
		CSystem* pSystem = static_cast<CSystem*>(gEnv->pSystem);
		CThreadProfiler* pThreadProf = pSystem->GetThreadProfiler();
		pThreadProf->Start();
	}

	static void StopThreadProf()
	{
		CSystem* pSystem = static_cast<CSystem*>(gEnv->pSystem);
		CThreadProfiler* pThreadProf = pSystem->GetThreadProfiler();
		pThreadProf->Stop();
	}
};

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)

struct SWorkerInfoSummarizedDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('X', "Worker Information Summarized", "['/WorkerInformation/Summary/$/' "
		                                                          "(float SamplePeriodInMS) (int ActiveWorkers) (float AvgUtilPerc) (float TotalExecutionPeriodeInMS) (int TotalNumberOfJobsExecuted)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		// Helpers
		struct SBackendPair
		{
			const JobManager::IBackend* pBackend;
			const char*                 pPrefix;
		};

		const SBackendPair pBackends[] =
		{
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Thread),   "NoneBlocking" },
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Blocking), "Blocking"     },
		};

		// Write worker summary for each backend
		for (int i = 0; i < CRY_ARRAY_COUNT(pBackends); i++)
		{
			const SBackendPair& rBackendPair = pBackends[i];
			bool addedValuesToStream = false;

			if (rBackendPair.pBackend)
			{
				JobManager::SWorkerFrameStatsSummary frameStatsSummary;
				JobManager::IWorkerBackEndProfiler* const __restrict pWorkerProfiler = rBackendPair.pBackend->GetBackEndWorkerProfiler();

				if (pWorkerProfiler)
				{
					pWorkerProfiler->GetFrameStatsSummary(frameStatsSummary);

					fr.AddValue(rBackendPair.pPrefix);                                    // Prefix
					fr.AddValue(frameStatsSummary.nSamplePeriod * 0.001f);                // SamplePeriode
					fr.AddValue((int)frameStatsSummary.nNumActiveWorkers);                // ActiveWorkers
					fr.AddValue(frameStatsSummary.nAvgUtilPerc);                          // AvgUtilPerc
					fr.AddValue((float)frameStatsSummary.nTotalExecutionPeriod * 0.001f); // TotalExecutionPeriode
					fr.AddValue((int)frameStatsSummary.nTotalNumJobsExecuted);            // TotalNumberOfJobsExecuted
					addedValuesToStream = true;
				}
			}

			if (!addedValuesToStream)
			{
				fr.AddValue(rBackendPair.pPrefix);
				fr.AddValue(0.f);
				fr.AddValue(0);
				fr.AddValue(0.f);
				fr.AddValue(0.f);
				fr.AddValue(0);
			}
		}
	}

	virtual uint32 PrepareToWrite()
	{
		return 2; // None-Blocking & Blocking Worker Summary Data Set
	}

};

struct SJobsInfoSummarizedDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('Z', "Job Information Summarized", "['/JobInformation/Summary/$/' "
		                                                       "(float TotalExecutionTimeInMS) (int TotalNumberOfJobsExecuted) (int TotalNumberOfIndividualJobsExecuted)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		// Helpers
		struct SBackendPair
		{
			const JobManager::IBackend* pBackend;
			const char*                 pPrefix;
		};

		const SBackendPair pBackendPairs[] =
		{
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Thread),   "NoneBlocking" },
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Blocking), "Blocking"     },
		};

		// Write jobs summary for each backend
		for (int i = 0; i < CRY_ARRAY_COUNT(pBackendPairs); i++)
		{
			const SBackendPair& rBackendPair = pBackendPairs[i];
			bool addedValuesToStream = false;

			if (rBackendPair.pBackend)
			{
				JobManager::SJobFrameStatsSummary jobFrameStatsSummary;
				JobManager::IWorkerBackEndProfiler* const __restrict pWorkerProfiler = rBackendPair.pBackend->GetBackEndWorkerProfiler();

				if (pWorkerProfiler)
				{
					pWorkerProfiler->GetFrameStatsSummary(jobFrameStatsSummary);

					fr.AddValue(rBackendPair.pPrefix);                                      // Prefix
					fr.AddValue((float)jobFrameStatsSummary.nTotalExecutionTime * 0.001f);  // TotalExecutionTime
					fr.AddValue((int)jobFrameStatsSummary.nNumJobsExecuted);                // TotalNumberOfJobsExecuted
					fr.AddValue((int)jobFrameStatsSummary.nNumIndividualJobsExecuted);      // TotalNumberOfIndividualJobsExecuted
					addedValuesToStream = true;
				}
			}

			if (!addedValuesToStream)
			{
				fr.AddValue(rBackendPair.pPrefix);
				fr.AddValue(0.f);
				fr.AddValue(0);
				fr.AddValue(0);
			}
		}
	}

	virtual uint32 PrepareToWrite()
	{
		return 2; // None-Blocking & Blocking Job Summary Data Set
	}
};

class CWorkerInfoIndividualDG : public IStatoscopeDataGroup
{

public:
	CWorkerInfoIndividualDG()
	{
		m_pThreadFrameStats = new JobManager::CWorkerFrameStats(0);
		m_pBlockingFrameStats = new JobManager::CWorkerFrameStats(0);
	}

	~CWorkerInfoIndividualDG()
	{
		SAFE_DELETE(m_pBlockingFrameStats);
		SAFE_DELETE(m_pThreadFrameStats);
	}

	virtual SDescription GetDescription() const
	{
		return SDescription('W', "Worker Information Individual", "['/WorkerInformation/Individual/$/$/' "
		                                                          "(float SamplePeriodInMS) (float ExecutionTimeInMS) (float IdleTimeInMS) (float UtilPerc) (int NumJobs)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		// Helpers
		struct SFrameStatsPair
		{
			const JobManager::CWorkerFrameStats* pFrameStats;
			const char*                          pPrefix;
		};

		const SFrameStatsPair pFrameStatsPairs[] =
		{
			{ m_pThreadFrameStats,   "NoneBlocking" },
			{ m_pBlockingFrameStats, "Blocking"     },
		};

		// Write individual worker information for each backend
		for (int i = 0; i < CRY_ARRAY_COUNT(pFrameStatsPairs); i++)
		{
			const SFrameStatsPair& rFrameStatsPair = pFrameStatsPairs[i];
			const float cSamplePeriod = (float)rFrameStatsPair.pFrameStats->nSamplePeriod;

			for (int j = 0; j < rFrameStatsPair.pFrameStats->numWorkers; ++j)
			{
				const JobManager::CWorkerFrameStats::SWorkerStats& pWorkerStats = rFrameStatsPair.pFrameStats->workerStats[j];
				const float nExecutionPeriod = (float)pWorkerStats.nExecutionPeriod * 0.001f;
				const float nIdleTime = (rFrameStatsPair.pFrameStats->nSamplePeriod > pWorkerStats.nExecutionPeriod) ? (rFrameStatsPair.pFrameStats->nSamplePeriod - pWorkerStats.nExecutionPeriod) * 0.001f : 0.f;

				char cWorkerName[16];
				cry_sprintf(cWorkerName, "Worker%i", j);
				fr.AddValue(rFrameStatsPair.pPrefix);             // Category
				fr.AddValue(cWorkerName);                         // Worker Name
				fr.AddValue(cSamplePeriod * 0.001f);              // SamplePeriodInMS
				fr.AddValue(nExecutionPeriod);                    // ExecutionTimeInMS
				fr.AddValue(nIdleTime);                           // IdleTimeInMS
				fr.AddValue(pWorkerStats.nUtilPerc);              // UtilPerc [0.f ... 100.f]
				fr.AddValue((int)pWorkerStats.nNumJobsExecuted);  // NumJobs
			}
		}
	}

	virtual uint32 PrepareToWrite()
	{
		// Helpers
		struct SBackendSTatePair
		{
			const JobManager::IBackend*     pBackend;
			JobManager::CWorkerFrameStats** pFrameStats;
		};

		SBackendSTatePair rBackendStatePairs[] =
		{
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Thread),   &m_pThreadFrameStats   },
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Blocking), &m_pBlockingFrameStats },
		};

		// Get individual worker information for each backend
		uint32 cNumTotalWorkers = 0;
		for (int i = 0; i < CRY_ARRAY_COUNT(rBackendStatePairs); i++)
		{
			SBackendSTatePair& rBackendStatePair = rBackendStatePairs[i];
			bool gotData = false;

			// Get job profile stats for backend
			if (rBackendStatePair.pBackend)
			{
				const uint32 cNumWorkers = rBackendStatePair.pBackend->GetBackEndWorkerProfiler()->GetNumWorkers();
				JobManager::IWorkerBackEndProfiler* const __restrict pWorkerProfiler = rBackendStatePair.pBackend->GetBackEndWorkerProfiler();

				// Get stats
				if (pWorkerProfiler)
				{
					// Validate size
					if ((*rBackendStatePair.pFrameStats)->numWorkers != cNumWorkers)
					{
						SAFE_DELETE(*rBackendStatePair.pFrameStats);
						*rBackendStatePair.pFrameStats = new JobManager::CWorkerFrameStats(cNumWorkers);
					}

					// Get frame stats
					pWorkerProfiler->GetFrameStats(**rBackendStatePair.pFrameStats);
					cNumTotalWorkers += cNumWorkers;
					gotData = true;
				}
			}

			// Set frame stats to 0 workers
			if (!gotData)
			{
				if ((*rBackendStatePair.pFrameStats)->numWorkers != 0)
				{
					SAFE_DELETE(*rBackendStatePair.pFrameStats);
					*rBackendStatePair.pFrameStats = new JobManager::CWorkerFrameStats(0);
				}
			}
		}

		// Return number of data sets to be written
		return cNumTotalWorkers;
	}

private:
	JobManager::CWorkerFrameStats* m_pThreadFrameStats;
	JobManager::CWorkerFrameStats* m_pBlockingFrameStats;
};

class CJobsInfoIndividualDG : public IStatoscopeDataGroup
{
public:
	virtual SDescription GetDescription() const
	{
		return SDescription('Y', "Job Information Individual", "['/JobInformation/Individual/$/$/' "
		                                                       "(float ExecutionTimeInMS) (int NumberOfExecutions)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		// Helpers
		struct SFrameStatsPair
		{
			const JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer* pFrameStats;
			const char* pPrefix;
		};

		const SFrameStatsPair pFrameStatsPairs[] =
		{
			{ &m_ThreadJobFrameStats,   "NoneBlocking" },
			{ &m_BlockingJobFrameStats, "Blocking"     },
		};

		// Write individual job information for each backend
		for (int i = 0; i < CRY_ARRAY_COUNT(pFrameStatsPairs); i++)
		{
			const SFrameStatsPair& pFrameStatsPair = pFrameStatsPairs[i];

			// Write job information
			for (int j = 0, nSize = pFrameStatsPair.pFrameStats->size(); j < nSize; ++j)
			{
				char cJobName[128];
				const JobManager::SJobFrameStats& rJobFrameStats = (*pFrameStatsPair.pFrameStats)[j];

				cry_sprintf(cJobName, "Job_%s", rJobFrameStats.cpName);
				fr.AddValue(pFrameStatsPair.pPrefix);             // Category
				fr.AddValue(cJobName);                            // Job Name
				fr.AddValue((float)rJobFrameStats.usec * 0.001f); // ExecutionTimeInMS
				fr.AddValue((int)rJobFrameStats.count);           // NumberOfExecutions
			}
		}
	}

	virtual uint32 PrepareToWrite()
	{
		// Helpers
		struct SBackendStatePair
		{
			const JobManager::IBackend* pBackend;
			JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer* pFrameStats;
		};

		SBackendStatePair pBackendStatePair[] =
		{
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Thread),   &m_ThreadJobFrameStats   },
			{ gEnv->GetJobManager()->GetBackEnd(JobManager::eBET_Blocking), &m_BlockingJobFrameStats },
		};

		// Get individual job information for each backend
		uint32 cNumTotalJobs = 0;
		for (int i = 0; i < CRY_ARRAY_COUNT(pBackendStatePair); i++)
		{
			SBackendStatePair& rBackendStatePair = pBackendStatePair[i];
			bool gotData = false;

			// Get job profile stats
			if (rBackendStatePair.pBackend)
			{
				JobManager::IWorkerBackEndProfiler* const __restrict pWorkerProfiler = rBackendStatePair.pBackend->GetBackEndWorkerProfiler();

				if (pWorkerProfiler)
				{
					pWorkerProfiler->GetFrameStats(*rBackendStatePair.pFrameStats, JobManager::IWorkerBackEndProfiler::eJobSortOrder_NoSort);
					cNumTotalJobs += rBackendStatePair.pFrameStats->size();
					gotData = true;
				}

				// Clear on no data
				if (!gotData)
					rBackendStatePair.pFrameStats->clear();
			}
		}

		// Return number of data sets to be written
		return cNumTotalJobs;
	}

private:
	JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer m_ThreadJobFrameStats;
	JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer m_BlockingJobFrameStats;
};
	#endif

struct SCPUTimesDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('j', "CPU Times", "['/CPUTimes/' (float physTime) "
		                                      "(float particleTime) (float particleSyncTime)"
		                                      "(float animTime) (int animNumCharacters) "
		                                      "(float aiTime) "
		                                      "(float flashTime)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		double frequency = 1.0 / static_cast<double>(freq.QuadPart);
	#define TICKS_TO_MS(t) ((float)((t) * 1000.0 * frequency))

		uint32 gUpdateTimeIdx = 0, gUpdateTimesNum = 0;
		const sUpdateTimes* gUpdateTimes = gEnv->pSystem->GetUpdateTimeStats(gUpdateTimeIdx, gUpdateTimesNum);
		float curPhysTime = TICKS_TO_MS(gUpdateTimes[gUpdateTimeIdx].PhysStepTime);
		fr.AddValue(curPhysTime);

		IParticleManager* pPartMan = gEnv->p3DEngine->GetParticleManager();
		if (pPartMan != NULL)
		{
			float fTimeMS = TICKS_TO_MS(pPartMan->GetLightProfileCounts().NumFrameTicks());
			float fTimeSyncMS = TICKS_TO_MS(pPartMan->GetLightProfileCounts().NumFrameSyncTicks());

			fr.AddValue(fTimeMS);
			fr.AddValue(fTimeSyncMS);
		}
		else
		{
			fr.AddValue(0.0f);
			fr.AddValue(0.0f);
			fr.AddValue(0.0f);
		}

		ICharacterManager* pCharManager = gEnv->pCharacterManager;
		if (pCharManager != NULL)
		{
			int nNumCharacters = (int)pCharManager->NumCharacters();
			float fTimeMS = TICKS_TO_MS(pCharManager->NumFrameTicks());

			fr.AddValue(fTimeMS);
			fr.AddValue(nNumCharacters);
		}
		else
		{
			fr.AddValue(0.0f);
			fr.AddValue(0);
		}

		IAISystem* pAISystem = gEnv->pAISystem;
		if (pAISystem != NULL)
		{
			float fTimeMS = TICKS_TO_MS(pAISystem->NumFrameTicks());
			fr.AddValue(fTimeMS);
		}
		else
		{
			fr.AddValue(0.0f);
		}

		const float flashCost = gEnv->pScaleformHelper ? gEnv->pScaleformHelper->GetFlashProfileResults() : -1.0f;
		if (flashCost >= 0.0f)
		{
			float flashCostInMS = flashCost * 1000.0f;
			fr.AddValue(flashCostInMS);
		}
		else
		{
			fr.AddValue(0.0f);
		}
	#undef TICKS_TO_MS
	}
};

struct SVertexCostDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('v', "Vertex data", "['/VertexData/' (int StaticPolyCountZ) (int SkinnedPolyCountZ) (int VegetationPolyCountZ)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		IRenderer* pRenderer = gEnv->pRenderer;

		int32 nPolyCountZ = pRenderer->GetPolygonCountByType(EFSLIST_GENERAL, EVCT_STATIC, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_SHADOW_GEN, EVCT_STATIC, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_TRANSP_AW, EVCT_STATIC, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_TRANSP_BW, EVCT_STATIC, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_DECAL, EVCT_STATIC, 1);
		fr.AddValue(nPolyCountZ);

		nPolyCountZ = pRenderer->GetPolygonCountByType(EFSLIST_GENERAL, EVCT_SKINNED, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_SHADOW_GEN, EVCT_SKINNED, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_TRANSP_AW, EVCT_SKINNED, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_TRANSP_BW, EVCT_SKINNED, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_DECAL, EVCT_SKINNED, 1);
		fr.AddValue(nPolyCountZ);

		nPolyCountZ = pRenderer->GetPolygonCountByType(EFSLIST_GENERAL, EVCT_VEGETATION, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_SHADOW_GEN, EVCT_VEGETATION, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_TRANSP_AW, EVCT_VEGETATION, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_TRANSP_BW, EVCT_VEGETATION, 1);
		nPolyCountZ += pRenderer->GetPolygonCountByType(EFSLIST_DECAL, EVCT_VEGETATION, 1);
		fr.AddValue(nPolyCountZ);
	}
};

struct SParticlesDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('p', "particles", "['/Particles/' (float numParticlesRendered) (float numParticlesActive) (float numParticlesAllocated) "
		                                      "(float particleScreenFractionRendered) (float particleScreenFractionProcessed) "
		                                      "(float numEmittersRendered) (float numEmittersActive) (float numEmittersAllocated) "
		                                      "(float numParticlesReiterated) (float numParticlesRejected) "
		                                      "(float numParticlesCollideTest) (float numParticlesCollideHit) (float numParticlesClipped)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		SParticleCounts stats;
		gEnv->pParticleManager->GetCounts(stats);

		float fScreenPix = (float)(gEnv->pRenderer->GetWidth() * gEnv->pRenderer->GetHeight());
		stats.pixels.updated  /= fScreenPix;
		stats.pixels.rendered /= fScreenPix;
		for (auto stat: stats)
			fr.AddValue(int(stat));
	}
};

struct SWavicleDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription(
			'P', "Wavicle", "['/Wavicle/'"
			"(int emittersAlive)(int emittersUpdated)(int emittersRendererd)"
			"(int componentsAlive)(int componentsUpdated)(int componentsRendered)"
			"(int particlesAllocated)(int particlesAlive)(int particlesUpdated)(int particlesRendered)(int particlesClipped)"
			"]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		using namespace pfx2;

		SParticleStats stats;
		GetIParticleSystem()->GetStats(stats);
		
		for (auto stat: stats)
			fr.AddValue(int(stat));
	}
};

struct SLocationDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('l', "location", "['/' (float posx) (float posy) (float posz) (float rotx) (float roty) (float rotz)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		Matrix33 m = Matrix33(GetISystem()->GetViewCamera().GetMatrix());
		Vec3 pos = GetISystem()->GetViewCamera().GetPosition();
		Ang3 rot = RAD2DEG(Ang3::GetAnglesXYZ(m));

		fr.AddValue(pos.x);
		fr.AddValue(pos.y);
		fr.AddValue(pos.z);

		fr.AddValue(rot.x);
		fr.AddValue(rot.y);
		fr.AddValue(rot.z);
	}
};

struct SPerCGFGPUProfilersDG : public IStatoscopeDataGroup
{
	CSimpleStringPool m_cattedCGFNames;

	SPerCGFGPUProfilersDG()
	// Frame IDs are currently problematic and will be ommitted
	//: SDataGroup('c', "per-cgf gpu profilers", "['/DrawCalls/$' (int frameID) (int totalDrawCallCount) (int numInstances)]", 4)
		: m_cattedCGFNames(true)
	{}

	virtual SDescription GetDescription() const
	{
		// Frame IDs are currently problematic and will be ommitted
		//: SDataGroup('c', "per-cgf gpu profilers", "['/DrawCalls/$' (int frameID) (int totalDrawCallCount) (int numInstances)]", 4)
		return SDescription('c', "per-cgf gpu profilers", "['/DrawCalls/$' (int totalDrawCallCount) (int numInstances)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
	#if defined(_RELEASE)
		CryFatalError("Per-CGF GPU profilers not enabled in release");
	#else
		IRenderer* pRenderer = gEnv->pRenderer;

		pRenderer->CollectDrawCallsInfo(true);

		IRenderer::RNDrawcallsMapMesh& drawCallsInfo = gEnv->pRenderer->GetDrawCallsInfoPerMesh();

		auto pEnd = drawCallsInfo.end();
		auto pItor = drawCallsInfo.begin();

		string sPathName;
		sPathName.reserve(64);

		//Per RenderNode Stats
		for (; pItor != pEnd; ++pItor)
		{
			IRenderer::SDrawCallCountInfo& drawInfo = pItor->second;

			const char* pRenderMeshName = drawInfo.meshName;
			const char* pNameShort = strrchr(pRenderMeshName, '/');

			if (pNameShort)
			{
				pRenderMeshName = pNameShort + 1;
			}

			if (drawInfo.nShadows > 0)
			{
				sPathName = "Shadow/";
				sPathName += drawInfo.typeName;
				sPathName += "/";
				sPathName += pRenderMeshName;
				sPathName += "/";
				fr.AddValue(m_cattedCGFNames.Append(sPathName.c_str(), sPathName.length()));
				//fr.AddValue( batchStat->nFrameID );
				fr.AddValue(drawInfo.nShadows);
				fr.AddValue(1);
			}
			if (drawInfo.nGeneral > 0)
			{
				sPathName = "Opaque/";
				sPathName += drawInfo.typeName;
				sPathName += "/";
				sPathName += pRenderMeshName;
				sPathName += "/";
				fr.AddValue(m_cattedCGFNames.Append(sPathName.c_str(), sPathName.length()));
				//fr.AddValue( batchStat->nFrameID );
				fr.AddValue(drawInfo.nGeneral);
				fr.AddValue(1);
			}
			if (drawInfo.nTransparent > 0)
			{
				sPathName = "Transparent/";
				sPathName += drawInfo.typeName;
				sPathName += "/";
				sPathName += pRenderMeshName;
				sPathName += "/";
				fr.AddValue(m_cattedCGFNames.Append(sPathName.c_str(), sPathName.length()));
				//fr.AddValue( batchStat->nFrameID );
				fr.AddValue(drawInfo.nTransparent);
				fr.AddValue(1);
			}
			if (drawInfo.nZpass > 0)
			{
				sPathName = "ZPass/";
				sPathName += drawInfo.typeName;
				sPathName += "/";
				sPathName += pRenderMeshName;
				sPathName += "/";
				fr.AddValue(m_cattedCGFNames.Append(sPathName.c_str(), sPathName.length()));
				//fr.AddValue( batchStat->nFrameID );
				fr.AddValue(drawInfo.nZpass);
				fr.AddValue(1);
			}
			if (drawInfo.nMisc > 0)
			{
				sPathName = "Misc/";
				sPathName += drawInfo.typeName;
				sPathName += "/";
				sPathName += pRenderMeshName;
				sPathName += "/";
				fr.AddValue(m_cattedCGFNames.Append(sPathName.c_str(), sPathName.length()));
				//fr.AddValue( batchStat->nFrameID );
				fr.AddValue(drawInfo.nMisc);
				fr.AddValue(1);
			}
		}

		//Flash
		{
			unsigned int numDPs = 0;
			unsigned int numTris = 0;
			if (gEnv->pScaleformHelper)
			{
				gEnv->pScaleformHelper->GetFlashRenderStats(numDPs, numTris);
			}

			if (numDPs)
			{
				fr.AddValue(m_cattedCGFNames.Append("Flash/Scaleform/All/", strlen("Flash/Scaleform/All/")));
				//fr.AddValue( batchStat->nFrameID );
				fr.AddValue((int)numDPs);
				fr.AddValue(1);
			}
		}
	#endif
	}

	virtual uint32 PrepareToWrite()
	{
		uint32 drawProfilerCount = 0;
	#if !defined(_RELEASE)
		IRenderer* pRenderer = gEnv->pRenderer;
		pRenderer->CollectDrawCallsInfo(true);
		IRenderer::RNDrawcallsMapMesh& drawCallsInfo = gEnv->pRenderer->GetDrawCallsInfoPerMesh();
		auto pEnd = drawCallsInfo.end();
		auto pItor = drawCallsInfo.begin();

		//Per RenderNode Stats
		for (; pItor != pEnd; ++pItor)
		{
			IRenderer::SDrawCallCountInfo& pInfo = pItor->second;

			if (pInfo.nShadows > 0)
				drawProfilerCount++;
			if (pInfo.nGeneral > 0)
				drawProfilerCount++;
			if (pInfo.nTransparent > 0)
				drawProfilerCount++;
			if (pInfo.nZpass > 0)
				drawProfilerCount++;
			if (pInfo.nMisc > 0)
				drawProfilerCount++;
		}

		//Flash!
		{
			unsigned int numDPs = 0;
			unsigned int numTris = 0;
			if (gEnv->pScaleformHelper)
			{
				gEnv->pScaleformHelper->GetFlashRenderStats(numDPs, numTris);
			}

			if (numDPs > 0)
				drawProfilerCount++;
		}
	#endif
		return drawProfilerCount;
	}
};

struct SParticleProfilersDG : public IStatoscopeDataGroup
{
	void AddParticleInfo(const SParticleInfo& pi)
	{
		if (IsEnabled())
			m_particleInfos.push_back(pi);
	}

	virtual SDescription GetDescription() const
	{
		return SDescription('y', "ParticlesColliding", "['/ParticlesColliding/$/' (int count)]");
	}

	virtual void Disable()
	{
		IStatoscopeDataGroup::Disable();

		m_particleInfos.clear();
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		for (uint32 i = 0; i < m_particleInfos.size(); i++)
		{
			SParticleInfo& particleInfo = m_particleInfos[i];

			const char* pEffectName = particleInfo.name.c_str();

			fr.AddValue(pEffectName);
			fr.AddValue(particleInfo.numParticles);
		}

		m_particleInfos.clear();
	}

	virtual uint32 PrepareToWrite()
	{
		return m_particleInfos.size();
	}

private:
	std::vector<SParticleInfo> m_particleInfos;
};

struct SPhysEntityProfilersDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('w', "PhysEntities", "['/PhysEntities/$/' (float time) (int nCalls) (float x) (float y) (float z)]");
	}

	virtual void Disable()
	{
		IStatoscopeDataGroup::Disable();

		m_physInfos.clear();
	}

	virtual uint32 PrepareToWrite()
	{
		return m_physInfos.size();
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		//poke CVar in physics
		gEnv->pPhysicalWorld->GetPhysVars()->bProfileEntities = 2;

		for (uint32 i = 0; i < m_physInfos.size(); i++)
		{
			SPhysInfo& physInfo = m_physInfos[i];

			const char* pEntityName = physInfo.name.c_str();

			fr.AddValue(pEntityName);
			fr.AddValue(physInfo.time);
			fr.AddValue(physInfo.nCalls);
			fr.AddValue(physInfo.pos.x);
			fr.AddValue(physInfo.pos.y);
			fr.AddValue(physInfo.pos.z);
		}

		m_physInfos.clear();
	}

	std::vector<SPhysInfo> m_physInfos;
};

struct SFrameProfilersDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('r', "frame profilers", "['/Threads/$' (int count) (float selfTimeInMS) (float peak)]");
	}

	virtual void Enable()
	{
		IStatoscopeDataGroup::Enable();
		ICVar* pCV_profile = gEnv->pConsole->GetCVar("profile");
		if (pCV_profile)
			pCV_profile->Set(-1);
	}

	virtual void Disable()
	{
		IStatoscopeDataGroup::Disable();
		ICVar* pCV_profile = gEnv->pConsole->GetCVar("profile");
		if (pCV_profile)
			pCV_profile->Set(0);
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		for (uint32 i = 0; i < m_frameProfilerRecords.size(); i++)
		{
			SPerfStatFrameProfilerRecord& fpr = m_frameProfilerRecords[i];
			string fpPath = GetFrameProfilerPath(fpr.m_pProfiler);
			fr.AddValue(fpPath.c_str());
			fr.AddValue(fpr.m_count);
			fr.AddValue(fpr.m_selfTime);
			fr.AddValue(fpr.m_peak);
		}

		m_frameProfilerRecords.clear();
	}

	virtual uint32 PrepareToWrite()
	{
		return m_frameProfilerRecords.size();
	}

	std::vector<SPerfStatFrameProfilerRecord> m_frameProfilerRecords; // the most recent frame's profiler data
};

struct SPerfCountersDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('q', "performance counters", "['/PerfCounters/' (int lhsCount) (int iCacheMissCount)]");
	}

	virtual void Enable()
	{
		IStatoscopeDataGroup::Enable();
	}

	virtual void Disable()
	{
		IStatoscopeDataGroup::Disable();
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		fr.AddValue(0);
		fr.AddValue(0);
	}
};

struct SUserMarkerDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('u', "user markers", "['/UserMarkers/$' (string name)]");
	}

	virtual void Disable()
	{
		IStatoscopeDataGroup::Disable();

		m_userMarkers.clear();
		m_tmpUserMarkers.clear();
	}

	virtual uint32 PrepareToWrite()
	{
		m_tmpUserMarkers.clear();
		m_userMarkers.swap(m_tmpUserMarkers);
		return m_tmpUserMarkers.size();
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		for (uint32 i = 0; i < m_tmpUserMarkers.size(); i++)
		{
			fr.AddValue(m_tmpUserMarkers[i].m_path.c_str());
			fr.AddValue(m_tmpUserMarkers[i].m_name.c_str());
		}
	}

	std::vector<SUserMarker>   m_tmpUserMarkers;
	CryMT::vector<SUserMarker> m_userMarkers;
};

struct SCallstacksDG : public IStatoscopeDataGroup
{
	CSimpleStringPool m_callstackAddressStrings;

	SCallstacksDG()
		: m_callstackAddressStrings(true)
	{}

	virtual SDescription GetDescription() const
	{
		return SDescription('k', "callstacks", "['/Callstacks/$' (string callstack)]");
	}

	virtual uint32 PrepareToWrite()
	{
		m_tmpCallstacks.clear();
		m_callstacks.swap(m_tmpCallstacks);
		return m_tmpCallstacks.size();
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		for (uint32 i = 0; i < m_tmpCallstacks.size(); i++)
		{
			const SCallstack& callstack = m_tmpCallstacks[i];
			uint32 numAddresses = callstack.m_addresses.size();
			string callstackString;
			callstackString.reserve((numAddresses * 11) + 1); // see ptrStr

			for (uint32 j = 0; j < numAddresses; j++)
			{
				char ptrStr[20]; // 0x + 0123456789ABCDEF + " " + '\0'
				cry_sprintf(ptrStr, "0x%p ", callstack.m_addresses[j]);
				callstackString += ptrStr;
			}

			fr.AddValue(callstack.m_tag.c_str());
			fr.AddValue(m_callstackAddressStrings.Append(callstackString.c_str(), callstackString.length()));
		}
	}

	CryMT::vector<SCallstack> m_callstacks;
	std::vector<SCallstack>   m_tmpCallstacks;
};

struct SNetworkDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('n', "network", "['/Network/' (int TotalBandwidthSent) (int TotalBandwidthRecvd) (int TotalPacketsSent) (int LobbyBandwidthSent) (int LobbyPacketsSent) (int SeqBandwidthSent) (int SeqPacketsSent) (int FragmentedBandwidthSent) (int FragmentedPacketsSent) (int OtherBandwidthSent) (int OtherPacketsSent)]");
	}

	//this PREFAST_SUPPRESS_WARNING needs better investigation
	virtual void Write(IStatoscopeFrameRecord& fr) PREFAST_SUPPRESS_WARNING(6262)
	{
		if (gEnv->pNetwork)
		{
			SBandwidthStats bandwidthStats;
			gEnv->pNetwork->GetBandwidthStatistics(&bandwidthStats);

			memcpy(&bandwidthStats.m_prev, &m_prev, sizeof(SBandwidthStatsSubset));
			SBandwidthStatsSubset delta = bandwidthStats.TickDelta();
			memcpy(&m_prev, &bandwidthStats.m_total, sizeof(SBandwidthStatsSubset));

			fr.AddValue((int)delta.m_totalBandwidthSent);
			fr.AddValue((int)delta.m_totalBandwidthRecvd);
			fr.AddValue(delta.m_totalPacketsSent);
			fr.AddValue((int)delta.m_lobbyBandwidthSent);
			fr.AddValue(delta.m_lobbyPacketsSent);
			fr.AddValue((int)delta.m_seqBandwidthSent);
			fr.AddValue(delta.m_seqPacketsSent);
			fr.AddValue((int)delta.m_fragmentBandwidthSent);
			fr.AddValue(delta.m_fragmentPacketsSent);
			fr.AddValue((int)(delta.m_totalBandwidthSent - delta.m_lobbyBandwidthSent - delta.m_seqBandwidthSent - delta.m_fragmentBandwidthSent));
			fr.AddValue(delta.m_totalPacketsSent - delta.m_lobbyPacketsSent - delta.m_seqPacketsSent - delta.m_fragmentPacketsSent);
		}
		else
		{
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
			fr.AddValue(0);
		}
	}

	SBandwidthStatsSubset m_prev;
};

struct SChannelDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
	#if ENABLE_URGENT_RMIS
		return SDescription('z', "channel", "['/Channel/$/' (float BandwidthSent) (float BandwidthRecvd) (float PacketRate) (int MaxPacketSize) (int IdealPacketSize) (int SparePacketSize) (int UsedPacketSize) (int SentMessages) (int UnsentMessages) (int UrgentRMIs) (int Ping)]");
	#else
		return SDescription('z', "channel", "['/Channel/$/' (float BandwidthSent) (float BandwidthRecvd) (float PacketRate) (int MaxPacketSize) (int IdealPacketSize) (int SparePacketSize) (int UsedPacketSize) (int SentMessages) (int UnsentMessages) (int Ping)]");
	#endif // ENABLE_URGENT_RMIS
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		if (gEnv->pNetwork)
		{
			for (uint32 index = 0; index < m_count; ++index)
			{
				fr.AddValue(m_bandwidthStats.m_channel[index].m_name);
				fr.AddValue(m_bandwidthStats.m_channel[index].m_bandwidthOutbound);
				fr.AddValue(m_bandwidthStats.m_channel[index].m_bandwidthInbound);
				fr.AddValue(m_bandwidthStats.m_channel[index].m_currentPacketRate);
				fr.AddValue((int)m_bandwidthStats.m_channel[index].m_maxPacketSize);
				fr.AddValue((int)m_bandwidthStats.m_channel[index].m_idealPacketSize);
				fr.AddValue((int)m_bandwidthStats.m_channel[index].m_sparePacketSize);
				fr.AddValue((int)m_bandwidthStats.m_channel[index].m_messageQueue.m_usedPacketSize);
				fr.AddValue(m_bandwidthStats.m_channel[index].m_messageQueue.m_sentMessages);
				fr.AddValue(m_bandwidthStats.m_channel[index].m_messageQueue.m_unsentMessages);
	#if ENABLE_URGENT_RMIS
				fr.AddValue(m_bandwidthStats.m_channel[index].m_messageQueue.m_urgentRMIs);
	#endif // ENABLE_URGENT_RMIS
				fr.AddValue((int)m_bandwidthStats.m_channel[index].m_ping);
			}
		}
	}

	virtual uint32 PrepareToWrite()
	{
		m_count = 0;
		if (gEnv->pNetwork)
		{
			gEnv->pNetwork->GetBandwidthStatistics(&m_bandwidthStats);

			for (uint32 index = 0; index < STATS_MAX_NUMBER_OF_CHANNELS; ++index)
			{
				if (m_bandwidthStats.m_channel[index].m_inUse)
				{
					++m_count;
				}
			}
		}

		return m_count;
	}

protected:
	SBandwidthStats m_bandwidthStats;
	uint32          m_count;
};

struct SNetworkProfileDG : public IStatoscopeDataGroup
{
	virtual SDescription GetDescription() const
	{
		return SDescription('d', "network profile", "['/NetworkProfile/$' (int totalBits) (int seqBits) (int rmiBits) (int calls)]");
	}

	virtual void Write(IStatoscopeFrameRecord& fr)
	{
		for (uint32 i = 0; i < m_statsCache.size(); i++)
		{
			SProfileInfoStat& nps = m_statsCache[i];
			fr.AddValue(nps.m_name.c_str());
			fr.AddValue((int)nps.m_totalBits); //-- as bits, not Kbits, so we can graph against the bandwidth stats on a comparable scale.
			fr.AddValue((int)(nps.m_rmi ? 0 : nps.m_totalBits));
			fr.AddValue((int)(nps.m_rmi ? nps.m_totalBits : 0));
			fr.AddValue((int)nps.m_calls);
		}
	}

	virtual uint32 PrepareToWrite()
	{
		m_statsCache.clear();

		if (gEnv->pNetwork)
		{
			SNetworkProfilingStats profileStats;
			gEnv->pNetwork->GetProfilingStatistics(&profileStats);

			for (int32 i = 0; i < profileStats.m_ProfileInfoStats.size(); i++)
			{
				SProfileInfoStat& nps = profileStats.m_ProfileInfoStats[i];
				if (nps.m_totalBits)
				{
					m_statsCache.push_back(nps);
				}
			}
		}

		return m_statsCache.size();
	}

	std::vector<SProfileInfoStat> m_statsCache;
};

class CStreamingObjectIntervalGroup : public CStatoscopeIntervalGroup, public IStreamedObjectListener
{
public:
	enum
	{
		Stage_Unloaded,
		Stage_Requested,
		Stage_LoadedUnused,
		Stage_LoadedUsed,
	};

public:
	CStreamingObjectIntervalGroup()
		: CStatoscopeIntervalGroup('o', "streaming objects",
		                           "['/Objects/' "
		                           "(string filename) "
		                           "(int stage) "
		                           "]")
	{
	}

	void Enable_Impl()
	{
		gEnv->p3DEngine->SetStreamableListener(this);
	}

	void Disable_Impl()
	{
		gEnv->p3DEngine->SetStreamableListener(NULL);
	}

	void WriteChangeStageBlockEvent(CStatoscopeEventWriter* pWriter, const void* pHandle, int stage)
	{
		size_t payloadLen = GetValueLength(stage);
		StatoscopeDataWriter::EventModifyInterval* pEv = pWriter->BeginBlockEvent<StatoscopeDataWriter::EventModifyInterval>(payloadLen);
		pEv->id = reinterpret_cast<UINT_PTR>(pHandle);
		pEv->classId = GetId();
		pEv->field = StatoscopeDataWriter::EventModifyInterval::FieldSplitIntervalMask | 1;

		char* pPayload = (char*)(pEv + 1);
		WriteValue(pPayload, stage);

		pWriter->EndBlockEvent();
	}

	void WriteChangeStageEvent(const void* pHandle, int stage)
	{
		CStatoscopeEventWriter* pWriter = GetWriter();

		if (pWriter)
		{
			pWriter->BeginBlock();
			WriteChangeStageBlockEvent(pWriter, pHandle, stage);
			pWriter->EndBlock();
		}
	}

public: // IStreamedObjectListener Members
	virtual void OnCreatedStreamedObject(const char* filename, void* pHandle)
	{
		CStatoscopeEventWriter* pWriter = GetWriter();

		if (pWriter)
		{
			size_t payloadLen =
			  GetValueLength(filename) +
			  GetValueLength(0)
			;

			StatoscopeDataWriter::EventBeginInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventBeginInterval>(payloadLen);
			pEv->id = reinterpret_cast<UINT_PTR>(pHandle);
			pEv->classId = GetId();

			char* pPayload = (char*)(pEv + 1);
			WriteValue(pPayload, filename);
			WriteValue(pPayload, Stage_Unloaded);

			pWriter->EndEvent();
		}
	}

	virtual void OnRequestedStreamedObject(void* pHandle)
	{
		WriteChangeStageEvent(pHandle, Stage_Requested);
	}

	virtual void OnReceivedStreamedObject(void* pHandle)
	{
		WriteChangeStageEvent(pHandle, Stage_LoadedUnused);
	}

	virtual void OnUnloadedStreamedObject(void* pHandle)
	{
		WriteChangeStageEvent(pHandle, Stage_Unloaded);
	}

	void OnBegunUsingStreamedObjects(void** pHandles, size_t numHandles)
	{
		CStatoscopeEventWriter* pWriter = GetWriter();

		if (pWriter)
		{
			pWriter->BeginBlock();

			for (size_t i = 0; i < numHandles; ++i)
				WriteChangeStageBlockEvent(pWriter, pHandles[i], Stage_LoadedUsed);

			pWriter->EndBlock();
		}
	}

	void OnEndedUsingStreamedObjects(void** pHandles, size_t numHandles)
	{
		CStatoscopeEventWriter* pWriter = GetWriter();

		if (pWriter)
		{
			pWriter->BeginBlock();

			for (size_t i = 0; i < numHandles; ++i)
				WriteChangeStageBlockEvent(pWriter, pHandles[i], Stage_LoadedUnused);

			pWriter->EndBlock();
		}
	}

	virtual void OnDestroyedStreamedObject(void* pHandle)
	{
		CStatoscopeEventWriter* pWriter = GetWriter();

		if (pWriter)
		{
			StatoscopeDataWriter::EventEndInterval* pEv = pWriter->BeginEvent<StatoscopeDataWriter::EventEndInterval>();
			pEv->id = reinterpret_cast<UINT_PTR>(pHandle);
			pWriter->EndEvent();
		}
	}
};

CStatoscopeEventWriter::CStatoscopeEventWriter()
	: m_eventNextSequence(0)
	, m_lastTimestampUs(0)
{
}

void CStatoscopeEventWriter::Flush(CDataWriter* pWriter)
{
	{
		using std::swap;
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_eventStreamLock);
		swap(m_eventStream, m_eventStreamTmp);
	}

	uint32 size = (uint32)m_eventStreamTmp.size();
	pWriter->WriteData(size);
	if (size)
		pWriter->WriteData(&m_eventStreamTmp[0], size);

	#ifndef _RELEASE
	if (!m_eventStreamTmp.empty())
	{
		char* pBuff = (char*)&m_eventStreamTmp[0];
		char* pBuffEnd = pBuff + size;
		StatoscopeDataWriter::EventHeader* pHdr = (StatoscopeDataWriter::EventHeader*)pBuff;
		uint64 firstTimestampUs = pHdr->timeStampUs;

		pBuff += pHdr->eventLengthInWords * 4;
		while (pBuff < pBuffEnd)
		{
			pHdr = (StatoscopeDataWriter::EventHeader*)pBuff;

			if (pHdr->timeStampUs < firstTimestampUs)
				__debugbreak();
			firstTimestampUs = pHdr->timeStampUs;

			pBuff += pHdr->eventLengthInWords * 4;
		}
	}
	#endif

	m_eventStreamTmp.clear();
}

void CStatoscopeEventWriter::Reset()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_eventStreamLock);
	stl::free_container(m_eventStream);
	stl::free_container(m_eventStreamTmp);
	m_eventNextSequence = 0;
}

CStatoscope::CStatoscope()
{
	m_lastDumpTime = 0.f;
	m_screenshotLastCaptureTime = 0.f;
	m_activeDataGroupMask = 0;
	m_activeIvDataGroupMask = 0;
	m_groupMaskInitialized = false;
	m_lastScreenWidth = 0;
	m_lastScreenHeight = 0;

	m_pScreenShotBuffer = NULL;
	m_ScreenShotState = eSSCS_Idle;

	m_bLevelStarted = false;

	RegisterBuiltInDataGroups();
	RegisterBuiltInIvDataGroups();

	m_pStatoscopeEnabledCVar = REGISTER_INT("e_StatoscopeEnabled", 0, VF_NULL, "Controls whether all statoscope is enabled.");
	m_pStatoscopeDumpAllCVar = REGISTER_INT("e_StatoscopeDumpAll", 0, VF_NULL, "Controls whether all functions are dumped in a profile log.");
	m_pStatoscopeDataGroupsCVar = REGISTER_INT64("e_StatoscopeDataGroups", AlphaBits64("fgmrtuO"), VF_BITFIELD, GetDataGroupsCVarHelpString(m_allDataGroups));
	m_pStatoscopeIvDataGroupsCVar = REGISTER_INT64("e_StatoscopeIvDataGroups", m_activeIvDataGroupMask, VF_BITFIELD, GetDataGroupsCVarHelpString(m_intervalGroups));
	m_pStatoscopeLogDestinationCVar = REGISTER_INT_CB("e_StatoscopeLogDestination", eLD_Socket, VF_NULL, "Where the Statoscope log gets written to:\n  0 - file\n  1 - socket\n  2 - telemetry server (default)", OnLogDestinationCVarChange);  // see ELogDestination
	m_pStatoscopeScreenshotCapturePeriodCVar = REGISTER_FLOAT("e_StatoscopeScreenshotCapturePeriod", -1.0f, VF_NULL, "How many seconds between Statoscope screenshot captures (-1 to disable).");
	m_pStatoscopeFilenameUseBuildInfoCVar = REGISTER_INT("e_StatoscopeFilenameUseBuildInfo", 1, VF_NULL, "Set to include the platform and build number in the log filename.");
	m_pStatoscopeFilenameUseMapCVar = REGISTER_INT("e_StatoscopeFilenameUseMap", 0, VF_NULL, "Set to include the map name in the log filename.");
	m_pStatoscopeFilenameUseTagCvar = REGISTER_STRING_CB("e_StatoscopeFilenameUseTag", "", VF_NULL, "Set to include tag in the log file name.", OnTagCVarChange);
	m_pStatoscopeFilenameUseTimeCVar = REGISTER_INT("e_StatoscopeFilenameUseTime", 0, VF_NULL, "Set to include the time and date in the log filename.");
	m_pStatoscopeFilenameUseDatagroupsCVar = REGISTER_INT("e_StatoscopeFilenameUseDatagroups", 0, VF_NULL, "Set to include the datagroup and date in the log filename.");
	m_pStatoscopeMinFuncLengthMsCVar = REGISTER_FLOAT("e_StatoscopeMinFuncLengthMs", 0.01f, VF_NULL, "Min func duration (ms) to be logged by statoscope.");
	m_pStatoscopeMaxNumFuncsPerFrameCVar = REGISTER_INT("e_StatoscopeMaxNumFuncsPerFrame", 150, VF_NULL, "Max number of funcs to log per frame.");
	m_pStatoscopeCreateLogFilePerLevelCVar = REGISTER_INT("e_StatoscopeCreateLogFilePerLevel", 0, VF_NULL, "Create a new perflog file per level.");
	m_pStatoscopeWriteTimeout = REGISTER_FLOAT("e_StatoscopeWriteTimeout", 1.0f, VF_NULL, "The number of seconds the data writer will stall before it gives up trying to write data (currently only applies to the telemetry data writer).");
	m_pStatoscopeConnectTimeout = REGISTER_FLOAT("e_StatoscopeConnectTimeout", 5.0f, VF_NULL, "The number of seconds the data writer will stall while trying connect to the telemetry server.");
	m_pStatoscopeAllowFPSOverrideCVar = REGISTER_INT("e_StatoscopeAllowFpsOverride", 1, VF_NULL, "Allow overriding of cvars in release for fps captures (MP only).");
	m_pGameRulesCVar = NULL;

	m_logNum = 1;

	REGISTER_COMMAND("e_StatoscopeAddUserMarker", &ConsoleAddUserMarker, 0, "Add a user marker to the perf stat logging for this frame");

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CStatoscope");

	CryCreateDirectory("%USER%/statoscope");

	m_pServer = new CStatoscopeServer(this);

	m_pDataWriter = NULL;
}

CStatoscope::~CStatoscope()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	gEnv->pRenderer->UnRegisterCaptureFrame(this);
	delete[] m_pScreenShotBuffer;
	m_pScreenShotBuffer = NULL;

	SAFE_DELETE(m_pDataWriter);
	SAFE_DELETE(m_pServer);
}

static char* Base64Encode(const uint8* buffer, int len)
{
	CRY_PROFILE_REGION(PROFILE_SYSTEM, "CStatoscope::Base64Encode");

	static const char base64Dict[64] = {
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
	};
	int b64Len = ((len + 2) / 3) * 4;
	char* b64Buf = new char[b64Len + 1];
	int byteCount = 0;
	int cycle = 0;

	for (int i = 0; i < b64Len; i++)
	{
		uint8 val = 0;

		switch (cycle)
		{
		case 0:
			val = buffer[byteCount] >> 2;
			break;
		case 1:
			val = (buffer[byteCount++] & 0x3) << 4;
			val |= buffer[byteCount] >> 4;
			break;
		case 2:
			val = (buffer[byteCount++] & 0xf) << 2;
			val |= buffer[byteCount] >> 6;
			break;
		case 3:
			val = buffer[byteCount++] & 0x3f;
			break;
		}

		cycle = (cycle + 1) & 3;

		b64Buf[i] = base64Dict[val];
	}

	b64Buf[b64Len] = '\0';

	return b64Buf;
}

bool CStatoscope::RegisterDataGroup(IStatoscopeDataGroup* pDG)
{
	IStatoscopeDataGroup::SDescription desc = pDG->GetDescription();

	for (std::vector<CStatoscopeDataGroup*>::iterator it = m_allDataGroups.begin(), itEnd = m_allDataGroups.end(); it != itEnd; ++it)
	{
		CStatoscopeDataGroup* pGroup = *it;

		if (pGroup->GetId() == desc.key)
			return false;
	}

	CStatoscopeDataGroup* pNewGroup = new CStatoscopeDataGroup(desc, pDG);
	m_allDataGroups.push_back(pNewGroup);

	return true;
}

void CStatoscope::UnregisterDataGroup(IStatoscopeDataGroup* pDG)
{
	for (std::vector<CStatoscopeDataGroup*>::iterator it = m_activeDataGroups.begin(), itEnd = m_activeDataGroups.end(); it != itEnd; ++it)
	{
		CStatoscopeDataGroup* pGroup = *it;
		IStatoscopeDataGroup* pCallback = pGroup->GetCallback();

		if (pCallback == pDG)
		{
			pDG->Disable();
			m_activeDataGroups.erase(it);
			break;
		}
	}

	for (std::vector<CStatoscopeDataGroup*>::iterator it = m_allDataGroups.begin(), itEnd = m_allDataGroups.end(); it != itEnd; ++it)
	{
		CStatoscopeDataGroup* pGroup = *it;
		IStatoscopeDataGroup* pCallback = pGroup->GetCallback();

		if (pCallback == pDG)
		{
			delete *it;
			m_allDataGroups.erase(it);
			break;
		}
	}
}

void CStatoscope::Tick()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	if (m_pStatoscopeEnabledCVar->GetIVal() != 0)
	{
		CreateDataWriter();

		if (m_pDataWriter && m_pDataWriter->Open())
		{
			SetIsRunning(true);

			uint64 currentActiveDataGroupMask = (uint64)m_pStatoscopeDataGroupsCVar->GetI64Val();
			uint64 differentDGs = currentActiveDataGroupMask ^ m_activeDataGroupMask;

			if (differentDGs)
			{
				uint64 enabledDGs = differentDGs & currentActiveDataGroupMask;
				uint64 disabledDGs = differentDGs & ~currentActiveDataGroupMask;
				SetDataGroups(enabledDGs, disabledDGs);

				m_activeDataGroupMask = currentActiveDataGroupMask;
				m_groupMaskInitialized = true;
			}

			AddFrameRecord(differentDGs != 0);
		}
	}
	else if (IsRunning())
	{
		CryLogAlways("Flushing Statoscope log\n");
		if (m_pDataWriter)
		{
			m_pDataWriter->Close();
		}

		for (IntervalGroupVec::const_iterator it = m_intervalGroups.begin(), itEnd = m_intervalGroups.end(); it != itEnd; ++it)
			(*it)->Disable();

		for (std::vector<CStatoscopeDataGroup*>::iterator it = m_activeDataGroups.begin(), itEnd = m_activeDataGroups.end(); it != itEnd; ++it)
			(*it)->GetCallback()->Disable();

		m_activeDataGroups.clear();
		m_activeDataGroupMask = 0;

		m_eventWriter.Reset();

		SetIsRunning(false);
	}
}

void CStatoscope::SetCurrentProfilerRecords(const std::vector<CFrameProfiler*>* profilers)
{
	if (m_pFrameProfilers)
	{
		// we want to avoid reallocation of m_perfStatDumpProfilers
		// even if numProfilers is quite large (in the thousands), it'll only be tens of KB
		uint32 numProfilers = profilers->size();
		m_perfStatDumpProfilers.clear();
		m_perfStatDumpProfilers.reserve(std::max((size_t)numProfilers, m_perfStatDumpProfilers.size()));		

		float minFuncTime = m_pStatoscopeMinFuncLengthMsCVar->GetFVal();

		int64 smallFuncs = 0;
		uint32 smallFuncsCount = 0;

		for (uint32 i = 0; i < numProfilers; i++)
		{
			CFrameProfiler* pProfiler = (*profilers)[i];

			// ignore really quick functions or ones what weren't called
			if (1000.f * gEnv->pTimer->TicksToSeconds(pProfiler->m_selfTime) > minFuncTime)
			{
				m_perfStatDumpProfilers.push_back(std::make_pair(pProfiler, pProfiler->m_selfTime));
			}
			else
			{
				smallFuncs += pProfiler->m_selfTime;
				smallFuncsCount++;
			}
		}

		std::sort(m_perfStatDumpProfilers.begin(), m_perfStatDumpProfilers.end(), CCompareFrameProfilersSelfTime());

		bool bDumpAll = false;

		if (m_pStatoscopeDumpAllCVar->GetIVal())
		{
			bDumpAll = true;
		}

		if (!bDumpAll)
		{
			uint32 maxNumFuncs = (uint32)m_pStatoscopeMaxNumFuncsPerFrameCVar->GetIVal();
			// limit the number being recorded
			m_perfStatDumpProfilers.resize(std::min(m_perfStatDumpProfilers.size(), (size_t)maxNumFuncs));
		}

		uint32 numDumpProfilers = m_perfStatDumpProfilers.size();
		std::vector<SPerfStatFrameProfilerRecord>& records = m_pFrameProfilers->m_frameProfilerRecords;

		records.reserve(numDumpProfilers);
		records.resize(0);

		for (uint32 i = 0; i < numDumpProfilers; i++)
		{
			CFrameProfiler* pProfiler = m_perfStatDumpProfilers[i].first;
			int64 selfTime = m_perfStatDumpProfilers[i].second;
			SPerfStatFrameProfilerRecord profilerRecord;

			profilerRecord.m_pProfiler = pProfiler;
			profilerRecord.m_count = pProfiler->m_count;
			profilerRecord.m_selfTime = 1000.f * gEnv->pTimer->TicksToSeconds(selfTime);
			profilerRecord.m_variance = pProfiler->m_variance;
			profilerRecord.m_peak = 1000.f * gEnv->pTimer->TicksToSeconds(pProfiler->m_peak);

			records.push_back(profilerRecord);
		}

		if (bDumpAll)
		{
			SPerfStatFrameProfilerRecord profilerRecord;

			profilerRecord.m_pProfiler = NULL;
			profilerRecord.m_count = smallFuncsCount;
			profilerRecord.m_selfTime = 1000.f * gEnv->pTimer->TicksToSeconds(smallFuncs);
			profilerRecord.m_variance = 0;

			records.push_back(profilerRecord);
		}
	}
}

void CStatoscope::AddParticleEffect(const char* pEffectName, int count)
{
	if (m_pParticleProfilers)
	{
		SParticleInfo p = { pEffectName, count };
		m_pParticleProfilers->AddParticleInfo(p);
	}
}

void CStatoscope::AddPhysEntity(const phys_profile_info* pInfo)
{
	if (m_pPhysEntityProfilers && m_pPhysEntityProfilers->IsEnabled())
	{
		float dt = 1000.f * gEnv->pTimer->TicksToSeconds(pInfo->nTicksLast * 1024);

		if (dt > 0.f)
		{
			static const char* peTypes[] =
			{
				"None",         //PE_NONE=0,
				"Static",       //PE_STATIC=1,
				"Rigid",        //PE_RIGID=2,
				"Wheeled",      //PE_WHEELEDVEHICLE=3,
				"Living",       //PE_LIVING=4,
				"Particle",     //PE_PARTICLE=5,
				"Articulated",  //PE_ARTICULATED=6,
				"Rope",         //PE_ROPE=7,
				"Soft",         //PE_SOFT=8,
				"Area"          //PE_AREA=9
			};

			pe_type type = pInfo->pEntity->GetType();

			int numElems = sizeof(peTypes) / sizeof(char*);

			if (type >= numElems)
			{
				CryFatalError("peTypes enum has changed, please update statoscope CStatoscope::AddPhysEntity");
			}

			const char* pEntName = pInfo->pName;

			IRenderNode* pRenderNode = (IRenderNode*)pInfo->pEntity->GetForeignData(PHYS_FOREIGN_ID_STATIC);

			if (pRenderNode)
			{
				pEntName = pRenderNode->GetName();
			}

			//extra '/'s will break the log output
			const char* pLastSlash = strrchr(pEntName, '/');
			pEntName = pLastSlash ? pLastSlash + 1 : pEntName;

			string name;
			assert(type < 10);
			PREFAST_ASSUME(type < 10);
			name.Format("%s/%s", peTypes[type], pEntName);

			pe_status_pos status_pos;
			status_pos.pos = Vec3(0.f, 0.f, 0.f);

			pInfo->pEntity->GetStatus(&status_pos);

			SPhysInfo p = { name.c_str(), dt, pInfo->nCalls, status_pos.pos };
			m_pPhysEntityProfilers->m_physInfos.push_back(p);
		}
	}
}

void CStatoscope::CreateTelemetryStream(const char* postHeader, const char* hostname, int port)
{
	SAFE_DELETE(m_pDataWriter);
	if (postHeader)
	{
		m_pDataWriter = new CTelemetryDataWriter(postHeader, hostname, port, m_pStatoscopeWriteTimeout->GetFVal(), m_pStatoscopeConnectTimeout->GetFVal());
	}
}

void CStatoscope::CloseTelemetryStream()
{
	SAFE_DELETE(m_pDataWriter);
}

void CStatoscope::PrepareScreenShot()
{
	const int widthDelta  = m_lastScreenWidth  - gEnv->pRenderer->GetWidth();
	const int heightDelta = m_lastScreenHeight - gEnv->pRenderer->GetHeight();

	m_lastScreenWidth  = gEnv->pRenderer->GetWidth();
	m_lastScreenHeight = gEnv->pRenderer->GetHeight();

	CRY_ASSERT(gEnv->pRenderer->GetWidth () == gEnv->pRenderer->GetOverlayWidth ());
	CRY_ASSERT(gEnv->pRenderer->GetHeight() == gEnv->pRenderer->GetOverlayHeight());

	const int shrunkenWidthNotAligned = OnGetFrameWidth();
	const int shrunkenWidth = shrunkenWidthNotAligned - (shrunkenWidthNotAligned % 4);
	const int shrunkenHeight = OnGetFrameHeight();

	if (!m_pScreenShotBuffer || widthDelta != 0 || heightDelta != 0)
	{
		SAFE_DELETE_ARRAY(m_pScreenShotBuffer);

		const size_t SCREENSHOT_BIT_DEPTH = 4;
		const size_t bufferSize = shrunkenWidth * shrunkenHeight * SCREENSHOT_BIT_DEPTH;
		m_pScreenShotBuffer = new uint8[bufferSize];
		memset(m_pScreenShotBuffer, 0, bufferSize * sizeof(uint8));
		gEnv->pRenderer->RegisterCaptureFrame(this);
	}

}

uint8* CStatoscope::ProcessScreenShot()
{
	//Reserved bytes in buffer indicate size and scale
	enum { SCREENSHOT_SCALED_WIDTH, SCREENSHOT_SCALED_HEIGHT, SCREENSHOT_MULTIPLIER };
	uint8* pScreenshotBuf = NULL;

	if (m_ScreenShotState == eSSCS_DataReceived)
	{
		const int SCREENSHOT_BIT_DEPTH = 4;
		const int SCREENSHOT_TARGET_BIT_DEPTH = 3;
		const int shrunkenWidthNotAligned = OnGetFrameWidth();
		const int shrunkenWidth = shrunkenWidthNotAligned - shrunkenWidthNotAligned % 4;
		const int shrunkenHeight = OnGetFrameHeight();

		const size_t bufferSize = 3 + (shrunkenWidth * shrunkenHeight * SCREENSHOT_TARGET_BIT_DEPTH);
		pScreenshotBuf = new uint8[bufferSize];

		if (pScreenshotBuf)
		{
			pScreenshotBuf[SCREENSHOT_MULTIPLIER] = (uint8)((max(shrunkenWidth, shrunkenHeight) + UCHAR_MAX) / UCHAR_MAX);            //Scaling factor
			pScreenshotBuf[SCREENSHOT_SCALED_WIDTH] = (uint8)(shrunkenWidth / pScreenshotBuf[SCREENSHOT_MULTIPLIER]);
			pScreenshotBuf[SCREENSHOT_SCALED_HEIGHT] = (uint8)(shrunkenHeight / pScreenshotBuf[SCREENSHOT_MULTIPLIER]);
			int iSrcPixel = 0;
			int iDstPixel = 3;

			while (iSrcPixel < shrunkenWidth * shrunkenHeight * SCREENSHOT_BIT_DEPTH)
			{
				pScreenshotBuf[iDstPixel + 0] = m_pScreenShotBuffer[iSrcPixel++];
				pScreenshotBuf[iDstPixel + 1] = m_pScreenShotBuffer[iSrcPixel++];
				pScreenshotBuf[iDstPixel + 2] = m_pScreenShotBuffer[iSrcPixel++];

				iSrcPixel++;
				iDstPixel += SCREENSHOT_TARGET_BIT_DEPTH;
			}

			m_ScreenShotState = eSSCS_Idle;
		}
	}

	return pScreenshotBuf;
}

void CStatoscope::AddFrameRecord(bool bOutputHeader)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	float currentTime = gEnv->pTimer->GetAsyncTime().GetSeconds();

	CStatoscopeFrameRecordWriter fr(m_pDataWriter);

	uint8* pScreenshot = NULL;

	//Screen shot in progress, attempt to process
	if (m_ScreenShotState == eSSCS_DataReceived)
	{
		pScreenshot = ProcessScreenShot();
	}

	//auto screen shot logic
	float screenshotCapturePeriod = m_pStatoscopeScreenshotCapturePeriodCVar->GetFVal();

	if ((m_ScreenShotState == eSSCS_Idle) && (screenshotCapturePeriod >= 0.0f))
	{
		if (currentTime >= m_screenshotLastCaptureTime + screenshotCapturePeriod)
		{
			//Tell the Render thread to dump the mini screenshot to main memory
			//Then wait for the callback to tell us the data is ready
			RequestScreenShot();
			m_screenshotLastCaptureTime = currentTime;
		}
	}

	if (m_pDataWriter->m_bShouldOutputLogTopHeader)
	{
	#if defined(NEED_ENDIAN_SWAP)
		m_pDataWriter->WriteData((char)StatoscopeDataWriter::EE_BigEndian);
	#else
		m_pDataWriter->WriteData((char)StatoscopeDataWriter::EE_LittleEndian);
	#endif

		m_pDataWriter->WriteData(STATOSCOPE_BINARY_VERSION);

		//Using string pool?
		m_pDataWriter->WriteData(m_pDataWriter->IsUsingStringPool());

		for (IntervalGroupVec::const_iterator it = m_intervalGroups.begin(), itEnd = m_intervalGroups.end(); it != itEnd; ++it)
			(*it)->Disable();

		m_eventWriter.Reset();
		WriteIntervalClassEvents();

		uint64 ivGroups = m_pStatoscopeIvDataGroupsCVar->GetI64Val();

		for (IntervalGroupVec::const_iterator it = m_intervalGroups.begin(), itEnd = m_intervalGroups.end(); it != itEnd; ++it)
		{
			if (AlphaBit64((*it)->GetId()) & ivGroups)
				(*it)->Enable(&m_eventWriter);
		}

		bOutputHeader = true;
		m_pDataWriter->m_bShouldOutputLogTopHeader = false;
	}

	if (bOutputHeader)
	{
		m_pDataWriter->WriteData(true);

		//Module info
		if (m_activeDataGroupMask & AlphaBit64('k') && !m_pDataWriter->m_bHaveOutputModuleInformation)
		{
			m_pDataWriter->WriteData(true);
			OutputLoadedModuleInformation(m_pDataWriter);
		}
		else
		{
			m_pDataWriter->WriteData(false);
		}

		m_pDataWriter->WriteData((int)m_activeDataGroups.size());

		for (uint32 i = 0; i < m_activeDataGroups.size(); i++)
		{
			CStatoscopeDataGroup* dataGroup = m_activeDataGroups[i];
			dataGroup->WriteHeader(m_pDataWriter);
		}
	}
	else
	{
		m_pDataWriter->WriteData(false);
	}

	//
	// 1. Frame time
	//
	m_pDataWriter->WriteData(currentTime);

	//
	// 2. Screen shot
	//
	if (pScreenshot)
	{
		m_pDataWriter->WriteData(StatoscopeDataWriter::B64Texture);
		int screenShotSize = 3 + ((pScreenshot[0] * pScreenshot[2]) * (pScreenshot[1] * pScreenshot[2]) * 3); // width,height,scale + (width*scale * height*scale * 3bpp)
		m_pDataWriter->WriteData(screenShotSize);
		m_pDataWriter->WriteData(pScreenshot, screenShotSize);
		SAFE_DELETE_ARRAY(pScreenshot);
	}
	else
	{
		m_pDataWriter->WriteData(StatoscopeDataWriter::None);
	}

	//
	// 3. Data groups
	//
	for (uint32 i = 0; i < m_activeDataGroups.size(); i++)
	{
		CStatoscopeDataGroup& dataGroup = *m_activeDataGroups[i];
		IStatoscopeDataGroup* pCallback = dataGroup.GetCallback();

		//output how many data sets to expect
		int nDataSets = pCallback->PrepareToWrite();
		m_pDataWriter->WriteData(nDataSets);

		fr.ResetWrittenElementCount();
		pCallback->Write(fr);

		int nElementsWritten = fr.GetWrittenElementCount();
		int nExpectedElems = dataGroup.GetNumElements() * nDataSets;

		if (nExpectedElems != nElementsWritten)
			CryFatalError("Statoscope data group: %s is broken. Check data group declaration", dataGroup.GetName());
	}

	//
	// 4. Events
	//
	m_eventWriter.Flush(m_pDataWriter);

	// 5. Magic Number, indicate end of frame record
	m_pDataWriter->WriteData(0xdeadbeef);
}

void CStatoscope::Flush()
{
	if (m_pDataWriter)
		m_pDataWriter->Flush();
}

bool CStatoscope::RequiresParticleStats(bool& bEffectStats)
{
	bool bGlobalStats = (m_activeDataGroupMask & AlphaBit64('p')) != 0;
	bool bCollisionStats = (m_activeDataGroupMask & AlphaBit64('y')) != 0;

	bool bRequiresParticleStats = false;
	bEffectStats = false;

	if (bCollisionStats)
	{
		bEffectStats = true;
		bRequiresParticleStats = true;
	}
	else if (bGlobalStats)
	{
		bRequiresParticleStats = true;
	}

	return bRequiresParticleStats;
}

void CStatoscope::SetLogFilename()
{
	m_logFilename = "%USER%/statoscope/perf";

	if (m_pStatoscopeFilenameUseBuildInfoCVar->GetIVal() > 0)
	{
	#if CRY_PLATFORM_DURANGO
		m_logFilename += "_XBOXONE";
	#elif CRY_PLATFORM_ORBIS
		m_logFilename += "_PS4";
	#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		m_logFilename += "_WIN64";
	#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
		m_logFilename += "_WIN32";
	#elif CRY_PLATFORM_MAC
		m_logFilename += "_MAC";
	#elif CRY_PLATFORM_ANDROID
		m_logFilename += "_ANDROID";
	#elif CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT
		m_logFilename += "_LINUX64";
	#elif CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT
		m_logFilename += "_LINUX32";
	#elif CRY_PLATFORM_IOS && CRY_PLATFORM_64BIT
		m_logFilename += "_IOS64";
	#elif CRY_PLATFORM_IOS && CRY_PLATFORM_32BIT
		m_logFilename += "_IOS32";
	#else
		m_logFilename += "_UNKNOWNPLAT";
	#endif

		const SFileVersion& ver = gEnv->pSystem->GetFileVersion();
		char versionString[64];
		cry_sprintf(versionString, "_%d_%d_%d_%d", ver.v[3], ver.v[2], ver.v[1], ver.v[0]);

		m_logFilename += versionString;
	}

	if (strcmp(m_pStatoscopeFilenameUseTagCvar->GetString(), "") != 0)
	{
		m_logFilename += "_";
		m_logFilename += m_pStatoscopeFilenameUseTagCvar->GetString();
	}

	if (m_pStatoscopeFilenameUseMapCVar->GetIVal() > 0)
	{
		const char* mapName = NULL;

		//If we don't have a last map loaded, try to look up the current one now
		if (m_currentMap.empty())
		{
			if (gEnv->pGameFramework)
			{
				mapName = gEnv->pGameFramework->GetLevelName();
			}
		}
		//If we tracked the last map loaded then use it here
		else
		{
			mapName = m_currentMap.c_str();
		}

		if (mapName)
		{
			const char* nameNoDir = strrchr(mapName, '/');

			// strip directory for now
			if (nameNoDir)
			{
				mapName = nameNoDir + 1;
			}

			string mapNameTrucated(mapName);

			m_logFilename += "_";
			int mapNameLenLimit = 32;
			if (m_pStatoscopeFilenameUseBuildInfoCVar->GetIVal() > 0)
			{
				mapNameLenLimit = 12;
			}
			m_logFilename += mapNameTrucated.Left(mapNameLenLimit);
		}
	}

	if (m_pStatoscopeFilenameUseDatagroupsCVar->GetIVal() > 0)
	{
		string datagroups = "_";
		uint64 currentActiveDataGroupMask = (uint64)m_pStatoscopeDataGroupsCVar->GetI64Val();
		for (uint32 i = 0, ic = m_allDataGroups.size(); i < ic; i++)
		{
			CStatoscopeDataGroup& dataGroup = *m_allDataGroups[i];

			if (AlphaBit64(dataGroup.GetId()) & currentActiveDataGroupMask)
			{
				datagroups += dataGroup.GetId();
			}
		}

		m_logFilename += datagroups;
	}

	if (m_pStatoscopeFilenameUseTimeCVar->GetIVal() > 0)
	{
		time_t curTime;
		time(&curTime);
		const struct tm* lt = localtime(&curTime);

		char name[MAX_PATH];
		strftime(name, CRY_ARRAY_COUNT(name), "_%Y%m%d_%H%M%S", lt);

		m_logFilename += name;
	}

	//ensure unique log name
	if (m_pStatoscopeCreateLogFilePerLevelCVar->GetIVal())
	{
		char logNumBuf[10];
		cry_sprintf(logNumBuf, "_%u", m_logNum++);
		m_logFilename += logNumBuf;
	}

	if (m_pStatoscopeLogDestinationCVar->GetIVal() == eLD_File)
		SAFE_DELETE(m_pDataWriter);

	m_logFilename += ".bin";
}

void CStatoscope::SetDataGroups(uint64 enabledDGs, uint64 disabledDGs)
{
	for (uint32 i = 0, ic = m_allDataGroups.size(); i < ic; i++)
	{
		CStatoscopeDataGroup& dataGroup = *m_allDataGroups[i];

		if (AlphaBit64(dataGroup.GetId()) & disabledDGs)
		{
			dataGroup.GetCallback()->Disable();
			m_activeDataGroups.erase(std::find(m_activeDataGroups.begin(), m_activeDataGroups.end(), &dataGroup));
		}

		if (AlphaBit64(dataGroup.GetId()) & enabledDGs)
		{
			m_activeDataGroups.push_back(&dataGroup);
			dataGroup.GetCallback()->Enable();
		}
	}
}

void CStatoscope::OutputLoadedModuleInformation(CDataWriter* pDataWriter)
{
	pDataWriter->WriteData(0);
	pDataWriter->m_bHaveOutputModuleInformation = true;
}

void CStatoscope::StoreCallstack(const char* tag, void** callstackAddresses, uint32 callstackLength)
{
	if (m_pCallstacks && m_pCallstacks->IsEnabled())
	{
		CryMT::vector<SCallstack>::AutoLock lock(m_pCallstacks->m_callstacks.get_lock());
		SCallstack callstack(callstackAddresses, callstackLength, tag);
		m_pCallstacks->m_callstacks.push_back(SCallstack());
		m_pCallstacks->m_callstacks.back().swap(callstack);
	}
}

void CStatoscope::AddUserMarker(const char* path, const char* name)
{
	if (!IsRunning())
		return;

	if (m_pUserMarkers && m_pUserMarkers->IsEnabled())
	{
		m_pUserMarkers->m_userMarkers.push_back(SUserMarker(path, name));
	}
}

void CStatoscope::AddUserMarkerFmt(const char* path, const char* fmt, ...)
{
	if (!IsRunning())
		return;

	if (m_pUserMarkers && m_pUserMarkers->IsEnabled())
	{
		char msg[1024];
		va_list args;
		va_start(args, fmt);
		cry_vsprintf(msg, fmt, args);
		va_end(args);

		m_pUserMarkers->m_userMarkers.push_back(SUserMarker(path, msg));
	}
}

//LogCallstack("USER MARKER");
void CStatoscope::LogCallstack(const char* tag)
{
	if (!IsRunning())
	{
		return;
	}

	uint32 callstackLength = 128;
	void* callstackAddresses[128];

	CSystem::debug_GetCallStackRaw(callstackAddresses, callstackLength);
	StoreCallstack(tag, callstackAddresses, callstackLength);
}

void CStatoscope::LogCallstackFormat(const char* tagFormat, ...)
{
	if (!IsRunning())
	{
		return;
	}

	va_list args;
	va_start(args, tagFormat);
	char tag[256];
	cry_vsprintf(tag, tagFormat, args);
	va_end(args);

	uint32 callstackLength = 128;
	void* callstackAddresses[128];

	CSystem::debug_GetCallStackRaw(callstackAddresses, callstackLength);
	StoreCallstack(tag, callstackAddresses, callstackLength);
}

void CStatoscope::ConsoleAddUserMarker(IConsoleCmdArgs* pParams)
{
	if (pParams->GetArgCount() == 3)
	{
		gEnv->pStatoscope->AddUserMarker(pParams->GetArg(1), pParams->GetArg(2));
	}
	else
	{
		CryLogAlways("Invalid use of e_StatoscopeAddUserMarker. Expecting 2 arguments, not %d.\n", pParams->GetArgCount() - 1);
	}
}

void CStatoscope::OnLogDestinationCVarChange(ICVar* pVar)
{
	CStatoscope* pStatoscope = (CStatoscope*)gEnv->pStatoscope;
	SAFE_DELETE(pStatoscope->m_pDataWriter);
	pStatoscope->m_pServer->CloseConnection();
}

void CStatoscope::OnTagCVarChange(ICVar* pVar)
{
	CStatoscope* pStatoscope = (CStatoscope*)gEnv->pStatoscope;
	if (pStatoscope->m_pDataWriter)
	{
		pStatoscope->m_pDataWriter->Close();
	}
	SAFE_DELETE(pStatoscope->m_pDataWriter);
	pStatoscope->m_pServer->CloseConnection();

	pStatoscope->SetLogFilename();
}

bool CStatoscope::IsLoggingForTelemetry()
{
	return m_pStatoscopeLogDestinationCVar->GetIVal() == eLD_Telemetry;
}

void CStatoscope::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		{
			if (!m_bLevelStarted)
			{
				if (!m_pGameRulesCVar)
				{
					m_pGameRulesCVar = gEnv->pConsole->GetCVar("sv_gamerules");
				}

				const char* mapName = NULL;

				if (gEnv->pGameFramework)
				{
					mapName = gEnv->pGameFramework->GetLevelName();
				}

				if (!mapName)
				{
					mapName = "unknown_map";
				}
				else
				{
					m_currentMap = mapName;
				}

				string userMarker = "Start ";
				userMarker += mapName;

				if (m_pGameRulesCVar)
				{
					userMarker += " ";
					userMarker += m_pGameRulesCVar->GetString();
				}

				AddUserMarker("Level", userMarker.c_str());

				if (m_pStatoscopeCreateLogFilePerLevelCVar->GetIVal())
				{
					//force new log file
					SetLogFilename();
				}

				m_bLevelStarted = true;
			}
		}
		break;
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			AddUserMarker("Level", "End");
			m_bLevelStarted = false;

			if (m_pDataWriter)
				AddFrameRecord(false);
		}
		break;
	}
}

//Screenshot capturing
bool CStatoscope::OnNeedFrameData(unsigned char*& pConvertedTextureBuf)
{
	//The renderer will only perform the screen grab if we supply a pointer from this callback. Since we only want one intermittently return null most of the time
	if (m_ScreenShotState == eSSCS_AwaitingBufferRequest || m_ScreenShotState == eSSCS_AwaitingCapture)
	{
		m_ScreenShotState = eSSCS_AwaitingCapture;
		pConvertedTextureBuf = m_pScreenShotBuffer;
		return true;
	}
	return false;
}

void CStatoscope::OnFrameCaptured()
{
	//The renderer has finished copying the screenshot into the buffer. Change state so we write the shot out.
	m_ScreenShotState = eSSCS_DataReceived;
}

int CStatoscope::OnGetFrameWidth(void)
{
	return m_lastScreenWidth / SCREENSHOT_SCALING_FACTOR;
}

int CStatoscope::OnGetFrameHeight(void)
{
	return m_lastScreenHeight / SCREENSHOT_SCALING_FACTOR;
}

int CStatoscope::OnCaptureFrameBegin(int* pTexHandle)
{
	//Called at the beginning of the rendering pass, the flags returned determine if the screenshot render target gets written to.
	//For performance reasons we do this infrequently.
	int flags = 0;
	if (m_ScreenShotState == eSSCS_RequestCapture)
	{
		flags |= eCFF_CaptureThisFrame;
		m_ScreenShotState = eSSCS_AwaitingBufferRequest;  //Frame initialised. Wait for the buffer to be requested
	}

	return flags;
}

void CStatoscope::SetupFPSCaptureCVars()
{
	if (m_pStatoscopeAllowFPSOverrideCVar && m_pStatoscopeAllowFPSOverrideCVar->GetIVal() != 0)
	{
		if (m_pStatoscopeDataGroupsCVar)
		{
			m_pStatoscopeDataGroupsCVar->Set("fgmut");
		}
		if (m_pStatoscopeFilenameUseBuildInfoCVar)
		{
			m_pStatoscopeFilenameUseBuildInfoCVar->Set("0");
		}
		if (m_pStatoscopeFilenameUseMapCVar)
		{
			m_pStatoscopeFilenameUseMapCVar->Set("1");
		}
		if (m_pStatoscopeCreateLogFilePerLevelCVar)
		{
			m_pStatoscopeCreateLogFilePerLevelCVar->Set("1");
		}

		ICVar* pCVar = gEnv->pConsole->GetCVar("e_StatoscopeScreenCapWhenGPULimited");
		if (pCVar)
		{
			pCVar->Set(1);
		}
	}
}

bool CStatoscope::RequestScreenShot()
{
	if (m_ScreenShotState == eSSCS_Idle)
	{
		m_ScreenShotState = eSSCS_RequestCapture;
		PrepareScreenShot();
		return true;
	}

	return false;
}

void CStatoscope::RegisterBuiltInDataGroups()
{
	m_pParticleProfilers = new SParticleProfilersDG();
	m_pPhysEntityProfilers = new SPhysEntityProfilersDG();
	m_pFrameProfilers = new SFrameProfilersDG();
	m_pUserMarkers = new SUserMarkerDG();
	m_pCallstacks = new SCallstacksDG();

	RegisterDataGroup(new SFrameLengthDG());
	RegisterDataGroup(new SMemoryDG());
	RegisterDataGroup(new SStreamingDG());
	RegisterDataGroup(new SStreamingAudioDG());
	RegisterDataGroup(new SStreamingObjectsDG());
	RegisterDataGroup(new SThreadsDG());
	RegisterDataGroup(new SSystemThreadsDG());
	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	RegisterDataGroup(new CWorkerInfoIndividualDG());
	RegisterDataGroup(new SWorkerInfoSummarizedDG());
	RegisterDataGroup(new CJobsInfoIndividualDG());
	RegisterDataGroup(new SJobsInfoSummarizedDG());
	#endif
	RegisterDataGroup(new SCPUTimesDG());
	RegisterDataGroup(new SVertexCostDG());
	RegisterDataGroup(new SParticlesDG);
	RegisterDataGroup(new SWavicleDG);
	RegisterDataGroup(new SLocationDG());
	RegisterDataGroup(new SPerCGFGPUProfilersDG());
	RegisterDataGroup(m_pParticleProfilers);
	RegisterDataGroup(m_pPhysEntityProfilers);
	RegisterDataGroup(m_pFrameProfilers);
	RegisterDataGroup(new SPerfCountersDG());
	RegisterDataGroup(m_pUserMarkers);
	RegisterDataGroup(m_pCallstacks);
	RegisterDataGroup(new SNetworkDG());
	RegisterDataGroup(new SChannelDG());
	RegisterDataGroup(new SNetworkProfileDG());
}

void CStatoscope::RegisterBuiltInIvDataGroups()
{
	m_intervalGroups.push_back(new CStatoscopeStreamingIntervalGroup());
	m_intervalGroups.push_back(new CStreamingObjectIntervalGroup());
	m_intervalGroups.push_back(new CStatoscopeTextureStreamingIntervalGroup());
}

void CStatoscope::CreateDataWriter()
{
	if (m_pDataWriter == NULL)
	{
		if (m_pStatoscopeLogDestinationCVar->GetIVal() == eLD_File)
		{
			if (m_logFilename.empty())
				SetLogFilename();

			m_pDataWriter = new CFileDataWriter(m_logFilename, m_pStatoscopeWriteTimeout->GetFVal() * 10);
		}
		else if (m_pStatoscopeLogDestinationCVar->GetIVal() == eLD_Socket)
		{
			m_pDataWriter = new CSocketDataWriter(m_pServer, m_pStatoscopeWriteTimeout->GetFVal() * 10);
		}
		else if (m_pStatoscopeLogDestinationCVar->GetIVal() == eLD_Telemetry)
		{
			return;
		}

		assert(m_pDataWriter);
	}
}

void CStatoscope::WriteIntervalClassEvents()
{
	for (IntervalGroupVec::iterator it = m_intervalGroups.begin(), itEnd = m_intervalGroups.end(); it != itEnd; ++it)
	{
		size_t descLength = (*it)->GetDescEventLength();

		StatoscopeDataWriter::EventDefineClass* pEv = m_eventWriter.BeginEvent<StatoscopeDataWriter::EventDefineClass>(descLength);

		pEv->classId = (*it)->GetId();
		pEv->numElements = (*it)->GetNumElements();
		(*it)->WriteDescEvent(pEv + 1);

		m_eventWriter.EndEvent();
	}
}

CStatoscopeServer::CStatoscopeServer(CStatoscope* pStatoscope)
	: m_socket(CRY_INVALID_SOCKET),
	m_isConnected(false),
	m_pStatoscope(pStatoscope)
{
	//m_DataThread.SetServer(this);
}

void CStatoscopeServer::StartListening()
{
	CloseConnection();

	assert(m_socket == CRY_INVALID_SOCKET);

	m_socket = CrySock::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == CRY_INVALID_SOCKET)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CStatoscopeServer() - failed to create a valid socket");
		return;
	}

	int err = 0;
	int statoscopeBasePort = 29527;
	do
	{
		CRYSOCKADDR_IN sa;
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = INADDR_ANY;
		sa.sin_port = htons(statoscopeBasePort++);
		err = CrySock::bind(m_socket, (const CRYSOCKADDR*)&sa, sizeof(sa));
	}
	while (CrySock::TranslateSocketError(err) == CrySock::eCSE_EADDRINUSE);
	if (CheckError(err, "CStatoscopeServer() - failed to bind the socket"))
	{
		return;
	}

	if (CheckError(CrySock::listen(m_socket, 1), "CStatoscopeServer() - failed to set the socket to listen"))
	{
		return;
	}

	CRYSOCKADDR_IN saIn;
	CRYSOCKLEN_T slen = sizeof(saIn);
	int result = CrySock::getsockname(m_socket, (CRYSOCKADDR*)&saIn, &slen);
	if (CrySock::TranslateSocketError(result) == CrySock::eCSE_NO_ERROR)
	{
		CryLog("Statoscope server listening on port: %u\n", ntohs(saIn.sin_port));
	}

	SetBlockingState(false);
	m_pStatoscope->m_pDataWriter->ResetForNewLog();
}

void CStatoscopeServer::CheckForConnection()
{
	if (m_isConnected)
		return;

	if (m_socket == CRY_INVALID_SOCKET)
		StartListening();

	if (m_socket == CRY_INVALID_SOCKET)
		return;

	CRYSOCKADDR sa;
	CRYSOCKLEN_T addrLen = sizeof(sa);
	CRYSOCKET newSocket = CrySock::accept(m_socket, &sa, &addrLen);
	const CrySock::eCrySockError errCode = CrySock::TranslateInvalidSocket(newSocket);

	if (errCode != CrySock::eCSE_NO_ERROR)
	{
		// this error reflects the absence of a pending connection request
		if (errCode != CrySock::eCSE_EWOULDBLOCK)
		{
			CheckError(newSocket ? static_cast<int>(newSocket) : 0, "CStatoscopeServer::CheckForConnection() - invalid socket from accept()");
		}
		return;
	}

	if (CheckError(CrySock::closesocket(m_socket), "CStatoscopeServer::CheckForConnection() - failed to close the listening socket"))
	{
		return;
	}

	m_socket = newSocket;
	SetBlockingState(true);
	m_isConnected = true;

	CRYSOCKADDR_IN saIn;
	memcpy(&saIn, &sa, sizeof(saIn));
	uint32 addr = ntohl(saIn.sin_addr.s_addr);
	CryLog("CStatoscopeServer connected to: %u.%u.%u.%u:%u\n", (uint8)(addr >> 24), (uint8)(addr >> 16), (uint8)(addr >> 8), (uint8)(addr >> 0), ntohs(saIn.sin_port));
}

void CStatoscopeServer::CloseConnection()
{
	if (m_socket == CRY_INVALID_SOCKET)
		return;

	SetBlockingState(false);

	if (CrySock::shutdown(m_socket, SD_SEND) < 0)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "CStatoscopeServer::CloseConnection() - shutdown failed");

	int ret;

	do
	{
		char buffer[256];
		ret = CrySock::recv(m_socket, buffer, sizeof(buffer), 0);
	}
	while (ret > 0);

	if (CrySock::closesocket(m_socket) < 0)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "CStatoscopeServer::CloseConnection() - closesocket failed");

	m_socket = CRY_INVALID_SOCKET;
	m_isConnected = false;
}

void CStatoscopeServer::SetBlockingState(bool block)
{
	bool succeeded = block ? CrySock::MakeSocketBlocking(m_socket) : CrySock::MakeSocketNonBlocking(m_socket);
	if (!succeeded)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CStatoscopeServer::SetBlockingState() failed");
}

int32 CStatoscopeServer::ReceiveData(void* buffer, int bufferSize)
{
	if (m_socket == CRY_INVALID_SOCKET || !m_isConnected)
	{
		return 0;
	}

	int ret = CrySock::recv(m_socket, static_cast<char*>(buffer), bufferSize, 0);

	if (CheckError(ret, "CStatoscopeServer::ReceiveData()"))
	{
		return 0;
	}

	return ret;
}

void CStatoscopeServer::SendData(const char* buffer, int bufferSize)
{
	threadID threadID = CryGetCurrentThreadId();

	if (m_socket == CRY_INVALID_SOCKET || !m_isConnected)
	{
		return;
	}

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	//float startTime = gEnv->pTimer->GetAsyncCurTime();
	//int origBufferSize = bufferSize;
	//const char *origBuffer = buffer;

	while (bufferSize > 0)
	{
		int ret = CrySock::send(m_socket, buffer, bufferSize, 0);

		if (CheckError(ret, "CStatoscopeServer::SendData()"))
		{
			return;
		}

		buffer += ret;
		bufferSize -= ret;
	}

	//float endTime = gEnv->pTimer->GetAsyncCurTime();
	//printf("Statoscope Send Data 0x%p size: %d time: %f time taken %f\n", origBuffer, origBufferSize, endTime, endTime - startTime);
}

bool CStatoscopeServer::CheckError(int err, const char* tag)
{
	if (err < 0)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "%s error: %d", tag, err);
		CloseConnection();
		return true;
	}

	return false;
}

CDataWriter::CDataWriter(bool bUseStringPool, float writeTimeout)
{
	m_bUseStringPool = bUseStringPool;
	m_bTimedOut = false;
	m_writeTimeout = writeTimeout;

	m_buffer.resize(128 * 1024);
	m_formatBuffer.resize(64 * 1024);

	m_pWritePtr = &m_buffer[0];
	m_pFlushStartPtr = &m_buffer[0];

	ResetForNewLog();
}

void CDataWriter::Close()
{
	Flush();
}

void CDataWriter::Flush()
{
	m_DataThread.QueueSendData(m_pFlushStartPtr, (int)(m_pWritePtr - m_pFlushStartPtr));
	m_pFlushStartPtr = m_pWritePtr;

	m_DataThread.Flush();
}

void CDataWriter::ResetForNewLog()
{
	m_pFlushStartPtr = m_pWritePtr;
	m_DataThread.Clear();
	m_DataThread.Flush();
	m_bShouldOutputLogTopHeader = true;
	m_bHaveOutputModuleInformation = false;
	m_GlobalStringPoolHashes.clear();
	m_bTimedOut = false;
}

void CDataWriter::FlushIOThread()
{
	printf("[Statoscope]Flush Data Thread\n");
	m_DataThread.Flush();
	m_pWritePtr = &m_buffer[0];
	m_pFlushStartPtr = &m_buffer[0];
}

//align write pointer to 4byte
void CDataWriter::Pad4()
{
	int pad = ((INT_PTR)m_pWritePtr) & 3;

	if (pad)
	{
		char pBuf[4] = { 0 };
		WriteData(pBuf, pad);
	}
}

void CDataWriter::WriteData(const void* vpData, int vsize)
{
	if (m_bTimedOut)
	{
		return;
	}
	const char* pData = (const char*)vpData;
	const int bufferSize = (int)m_buffer.size() - 1;
	while (vsize > 0)
	{
		const int size = std::min(bufferSize, vsize);

		int capacity = (int)(&m_buffer[bufferSize] - m_pWritePtr);

		bool bWrapBuffer = size > capacity;

		//if we are wrapping the buffer, send the remaining jobs
		if (bWrapBuffer)
		{
			m_DataThread.QueueSendData(m_pFlushStartPtr, (int)(m_pWritePtr - m_pFlushStartPtr));

			//reset write pointer
			m_pWritePtr = &m_buffer[0];
			m_pFlushStartPtr = &m_buffer[0];
		}

		char* pWriteStart = m_pWritePtr;
		char* pWriteEnd = pWriteStart + size;

		// Stall until the read thread clears the buffer we need to use
		CTimeValue startTime = gEnv->pTimer->GetAsyncTime();
		do
		{
			const char* pReadBoundsStart, * pReadBoundsEnd;
			uint32 numBytesInQueue = m_DataThread.GetReadBounds(pReadBoundsStart, pReadBoundsEnd);

			if (numBytesInQueue == 0)
				break;

			// if these are the same, there's no room in the buffer
			if (pReadBoundsStart != pReadBoundsEnd)
			{
				if (pReadBoundsStart <= pReadBoundsEnd)
				{
					// Simple case, just the one in use section
					if (pWriteEnd <= pReadBoundsStart || pWriteStart >= pReadBoundsEnd)
						break;
				}
				else
				{
					// Two in use sections, wrapping at the boundaries
					if ((pWriteEnd <= pReadBoundsStart) && (pWriteStart >= pReadBoundsEnd))
						break;
				}
			}
			CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
			if (m_writeTimeout != 0.0f && currentTime.GetDifferenceInSeconds(startTime) > m_writeTimeout)
			{
				CryLog("CDataWriter write timeout exceeded: %f", currentTime.GetDifferenceInSeconds(startTime));
				m_bTimedOut = true;
				return;
			}

			CrySleep(1);
		}
		while (true);

		memcpy((void*)pWriteStart, pData, size);
		m_pWritePtr = pWriteEnd;

		if ((m_pWritePtr - m_pFlushStartPtr) >= FlushLength)
		{
			m_DataThread.QueueSendData(m_pFlushStartPtr, (int)(m_pWritePtr - m_pFlushStartPtr));
			m_pFlushStartPtr = m_pWritePtr;
		}

		pData += size;
		vsize -= size;
	}
}
CFileDataWriter::CFileDataWriter(const string& fileName, float writeTimeout)
	: CDataWriter(true, writeTimeout)
{
	m_fileName = fileName;
	m_pFile = NULL;
	m_bAppend = false;
	m_DataThread.SetDataWriter(this);
}

CFileDataWriter::~CFileDataWriter()
{
	Close();
}

bool CFileDataWriter::Open()
{
	if (!m_pFile)
	{
		SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

		const char* modeStr = m_bAppend ? "ab" : "wb";
		m_bAppend = true;

		m_pFile = fxopen(m_fileName.c_str(), modeStr);
	}

	return m_pFile != NULL;
}

void CFileDataWriter::Close()
{
	if (m_pFile != NULL)
	{
		CDataWriter::Close();
		fclose(m_pFile);
		m_pFile = NULL;
	}
}

void CFileDataWriter::Flush()
{
	if (m_pFile != NULL)
	{
		CDataWriter::Flush();
		fflush(m_pFile);
	}
}

void CFileDataWriter::SendData(const char* pBuffer, int nBytes)
{
	if (m_pFile)
	{
		fwrite(pBuffer, nBytes, 1, m_pFile);

		//float endTime = gEnv->pTimer->GetAsyncCurTime();
		//printf("Statoscope Write Data 0x%p size: %d time: %f time taken %f\n", pBuffer, nBytes, endTime, endTime - startTime);
	}
	else
	{
		CryFatalError("Statoscope file not open");
	}
}

CSocketDataWriter::CSocketDataWriter(CStatoscopeServer* pStatoscopeServer, float writeTimeout)
	: CDataWriter(true, writeTimeout)
{
	m_pStatoscopeServer = pStatoscopeServer;
	m_DataThread.SetDataWriter(this);
}

bool CSocketDataWriter::Open()
{
	m_pStatoscopeServer->CheckForConnection();
	return m_pStatoscopeServer->IsConnected();
}

void CSocketDataWriter::SendData(const char* pBuffer, int nBytes)
{
	m_pStatoscopeServer->SendData(pBuffer, nBytes);
}

CTelemetryDataWriter::CTelemetryDataWriter(const char* postHeader, const char* hostname, int port, float writeTimeout, float connectTimeout)
	: CDataWriter(true, writeTimeout)
	, m_socket(-1)
	, m_hasSentHeader(false)
	, m_socketErrorTriggered(false)
	, m_connectTimeout(connectTimeout)
{
	m_postHeader = postHeader;
	m_hostname = hostname;
	m_port = port;
	m_DataThread.SetDataWriter(this);
}

bool CTelemetryDataWriter::Open()
{
	if (m_socket == CRY_INVALID_SOCKET && !m_socketErrorTriggered)
	{
		CRYSOCKET s = CrySock::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == CRY_INVALID_SOCKET)
		{
			CryLog("CTelemetryDataWriter failed to create a valid socket");
			m_socketErrorTriggered = true;
			return false;
		}
		if (!CrySock::MakeSocketNonBlocking(s))
		{
			CryLog("CTelemetryDataWriter failed to make socket non-blocking");
			m_socketErrorTriggered = true;
			return false;
		}
		CRYSOCKADDR_IN sa;
		sa.sin_family = AF_INET;
		sa.sin_port = htons(m_port);
		sa.sin_addr.s_addr = CrySock::DNSLookup(m_hostname.c_str());
		if (sa.sin_addr.s_addr)
		{
			m_socket = s;
			int ret = CrySock::connect(m_socket, (const CRYSOCKADDR*)&sa, sizeof(sa));
			CrySock::eCrySockError err = CrySock::TranslateSocketError(ret);
			if (err != CrySock::eCSE_EWOULDBLOCK_CONN && err != CrySock::eCSE_EINPROGRESS)
			{
				CheckSocketError(err, "Connect to telemetry server");
			}
		}
		else
		{
			CryLog("CTelemetryDataWriter failed to resolve host name '%s'", m_hostname.c_str());
			m_socketErrorTriggered = true;
		}
	}
	return (m_socket >= 0);
}

void CTelemetryDataWriter::Close()
{
	CDataWriter::Close();
	if (m_socket >= 0)
	{
		CRYSOCKET sock = m_socket;
		SendToSocket("0\r\n\r\n", 5, "End HTTP chunked data stream");
		CheckSocketError(CrySock::TranslateSocketError(CrySock::shutdown(sock, SD_SEND)), "Shutdown socket for sending");
		while (!m_bTimedOut)
		{
			const int recvBufferSize = 1024;
			char recvBuffer[recvBufferSize];
			int result = CrySock::recv(sock, recvBuffer, recvBufferSize, 0);
			CheckSocketError(CrySock::TranslateSocketError(result), "Wait for response");
			if (result <= 0)
			{
				break;
			}
			// TODO: Check recvBuffer contains OK message
		}
		CheckSocketError(CrySock::TranslateSocketError(CrySock::closesocket(sock)), "Close socket");
		m_socket = -1;
	}
}

void CTelemetryDataWriter::SendData(const char* pBuffer, int nBytes)
{
	if (m_socket >= 0)
	{
		if (!m_hasSentHeader)
		{
			// Wait until the connection is fully established
			CRYTIMEVAL timeout;
			timeout.tv_sec = (long)(m_connectTimeout);
			timeout.tv_usec = (long)((m_connectTimeout - timeout.tv_sec) * 1e6);

			int result = CrySock::WaitForWritableSocket(m_socket, &timeout);

			if (result != 1)
			{
				if (result == 0)
				{
					CheckSocketError(CrySock::eCSE_ETIMEDOUT, "Waiting for connection to be fully established");
				}
				else
				{
					CheckSocketError(CrySock::TranslateSocketError(result), "Waiting for connection to be fully established");
				}
			}

			SendToSocket(m_postHeader.c_str(), m_postHeader.size(), "Begin HTTP chunked data stream");
			m_hasSentHeader = true;
		}
		char chunkHeaderBuffer[16];
		cry_sprintf(chunkHeaderBuffer, "%x\r\n", nBytes);
		SendToSocket(chunkHeaderBuffer, strlen(chunkHeaderBuffer), "Write HTTP chunk size");
		SendToSocket(pBuffer, nBytes, "Write HTTP chunk data");
		SendToSocket("\r\n", 2, "Terminate HTTP chunk data");
	}
}

void CTelemetryDataWriter::CheckSocketError(CrySock::eCrySockError sockErr, const char* description)
{
	if (sockErr != CrySock::eCSE_NO_ERROR)
	{
		CryLog("CTelemetryDataWriter socket error '%s' - '%d'", description, sockErr);
		if (m_socket >= 0)
		{
			CrySock::closesocket(m_socket);
			m_socket = -1;
		}
		m_socketErrorTriggered = true;
	}
}

void CTelemetryDataWriter::SendToSocket(const char* pData, size_t nSize, const char* sDescription)
{
	if (m_socket >= 0)
	{
		size_t nSent = 0;
		while (!m_bTimedOut)
		{
			int ret = CrySock::send(m_socket, pData + nSent, nSize - nSent, 0);
			CrySock::eCrySockError sockErr = CrySock::TranslateSocketError(ret);
			if (sockErr != CrySock::eCSE_NO_ERROR)
			{
				if (sockErr != CrySock::eCSE_EWOULDBLOCK)
				{
					CheckSocketError(sockErr, sDescription);
					return;
				}
			}
			else
			{
				nSent += ret;
				if (nSent >= nSize)
				{
					return;
				}
			}
			CrySleep(1);
		}
	}
}

CStatoscopeIOThread::CStatoscopeIOThread()
{
	m_pDataWriter = NULL;
	m_threadID = THREADID_NULL;
	m_numBytesInQueue = 0;
	m_bRun = true;

	if (!gEnv->pThreadManager->SpawnThread(this, "StatoscopeDataWriter"))
	{
		CryFatalError("Error spawning \"StatoscopeDataWriter\" thread.");
	}
}

CStatoscopeIOThread::~CStatoscopeIOThread()
{
	//ensure all data is sent
	Flush();

	// Stop thread task.
	m_bRun = false;
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

void CStatoscopeIOThread::Flush()
{
	CTimeValue startTime = gEnv->pTimer->GetAsyncTime();
	while (m_sendJobs.size())
	{
		CrySleep(1);
		CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
		float timeout = m_pDataWriter->GetWriteTimeout();
		if (timeout != 0.0f && currentTime.GetDifferenceInSeconds(startTime) > timeout)
		{
			// This should cause the data writer to abort attempting to write data and clear the send jobs queue
			CryLog("CDataWriter write timeout exceeded during flush: %f", currentTime.GetDifferenceInSeconds(startTime));
			m_pDataWriter->TimeOut();
		}
	}
}

void CStatoscopeIOThread::ThreadEntry()
{
	while (m_bRun)
	{
		if (m_sendJobs.size() > 0)
		{
			SendJob job;
			{
				CryMT::queue<SendJob>::AutoLock lock(m_sendJobs.get_lock());
				job = m_sendJobs.front();
			}

			m_pDataWriter->SendData(job.pBuffer, job.nBytes);

			{
				CryMT::queue<SendJob>::AutoLock lock(m_sendJobs.get_lock());
				SendJob j;
				m_sendJobs.try_pop(j);
				m_numBytesInQueue -= j.nBytes;
			}

			//PIXEndNamedEvent();
		}
		else
		{
			CrySleep(1);
		}
	}
}

void CStatoscopeIOThread::QueueSendData(const char* pBuffer, int nBytes)
{
	if (nBytes > 0)
	{
		bool bWait = false;

		//PIXSetMarker(0, "[STATOSCOPE]Queue Data\n");

		assert(pBuffer);
		assert(nBytes > 0);

		SendJob newJob = { pBuffer, nBytes };

		{
			CryMT::queue<SendJob>::AutoLock lock(m_sendJobs.get_lock());
			m_sendJobs.push(newJob);
			m_numBytesInQueue += newJob.nBytes;
		}

		//printf("Statoscope Queue Data 0x%p size %d at time: %f\n", pBuffer, nBytes, gEnv->pTimer->GetAsyncCurTime());
	}
	else if (nBytes < 0)
	{
		CryFatalError("Borked!");
	}
}

	#undef EWOULDBLOCK
	#undef GetLastError

#endif // ENABLE_STATOSCOPE
