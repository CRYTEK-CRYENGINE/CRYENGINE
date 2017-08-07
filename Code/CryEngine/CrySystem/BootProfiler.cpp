// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if defined(ENABLE_LOADING_PROFILER)

	#include "BootProfiler.h"
	#include "ThreadInfo.h"
	#include "CryMath/Cry_Math.h"

namespace
{
	#define CACHE_LINE_SIZE_BP_INTERNAL 64

enum
{
	eMAX_THREADS_TO_PROFILE = 64,
	eNUM_RECORDS_PER_POOL   = 4680,   // so, eNUM_RECORDS_PER_POOL * sizeof(CBootProfilerRecord) == mem consumed by pool item
	// poolmem ~= 1Mb for 1 pool per thread
};

class CBootProfilerThreadsInterface
{
public:
	CBootProfilerThreadsInterface()
	{
		Reset();
	}

	void Reset()
	{
		m_threadCounter = 0;
		memset(m_threadInfo, 0, sizeof(m_threadInfo));
		for (int i = 0; i < eMAX_THREADS_TO_PROFILE; ++i)
		{
			m_threadNames[i].clear();
		}
	}

	ILINE unsigned int GetThreadIndexByID(threadID threadID)
	{
		for (int i = 0; i < eMAX_THREADS_TO_PROFILE; ++i)
		{
			if (m_threadInfo[i] == 0)
				break;
			if (m_threadInfo[i] == threadID)
				return i;
		}

		const unsigned int counter = CryInterlockedIncrement(&m_threadCounter) - 1;     //count to index
		m_threadInfo[counter] = threadID;
		m_threadNames[counter] = gEnv->pThreadManager->GetThreadName(threadID);

		return counter;
	}

	ILINE const char* GetThreadNameByIndex(unsigned int threadIndex)
	{
		assert(threadIndex < m_threadCounter);
		return m_threadNames[threadIndex].c_str();
	}

	int GetThreadCount() const
	{
		return m_threadCounter;
	}

private:
	threadID            m_threadInfo[eMAX_THREADS_TO_PROFILE];                    //threadIDs
	CryFixedStringT<32> m_threadNames[eMAX_THREADS_TO_PROFILE];
	int                 m_threadCounter;
};

CBootProfiler gProfilerInstance;
CBootProfilerThreadsInterface gThreadsInterface;
}

int CBootProfiler::CV_sys_bp_frames_worker_thread = 0;
int CBootProfiler::CV_sys_bp_frames = 0;
int CBootProfiler::CV_sys_bp_frames_sample_period = 0;
int CBootProfiler::CV_sys_bp_frames_sample_period_rnd = 0;
float CBootProfiler::CV_sys_bp_frames_threshold = 0.0f;
float CBootProfiler::CV_sys_bp_time_threshold = 0.0f;

class CProfileBlockTimes
{
public:
	LARGE_INTEGER GetStopTimeStamp() const { return m_stopTimeStamp; }

protected:
	LARGE_INTEGER m_startTimeStamp;
	LARGE_INTEGER m_stopTimeStamp;
	CProfileBlockTimes()
	{
		memset(&m_startTimeStamp, 0, sizeof(m_startTimeStamp));
		memset(&m_stopTimeStamp, 0, sizeof(m_stopTimeStamp));
	}
};

class CRecordPool;
class CBootProfilerSession;

class CBootProfilerRecord
{
public:
	const char*           m_label;
	LARGE_INTEGER         m_startTimeStamp;
	LARGE_INTEGER         m_stopTimeStamp;

	unsigned int          m_threadIndex;

	CBootProfilerRecord*  m_pParent;

	CBootProfilerRecord*  m_pFirstChild;
	CBootProfilerRecord*  m_pLastChild;
	CBootProfilerRecord*  m_pNextSibling;
	CBootProfilerSession* m_pSession;

	CryFixedStringT<256> m_args;

	ILINE CBootProfilerRecord(const char* label, LARGE_INTEGER timestamp, unsigned int threadIndex, const char* args, CBootProfilerSession* pSession) :
		m_label(label), m_startTimeStamp(timestamp), m_threadIndex(threadIndex), m_pParent(nullptr), m_pFirstChild(nullptr), m_pLastChild(nullptr), m_pNextSibling(nullptr), m_pSession(pSession)
	{
		memset(&m_stopTimeStamp, 0, sizeof(m_stopTimeStamp));
		if (args)
			m_args = args;
	}

	void AddChild(CBootProfilerRecord* pRecord)
	{
		if (!m_pFirstChild)
			m_pFirstChild = pRecord;

		if (m_pLastChild)
		{
			m_pLastChild->m_pNextSibling = pRecord;
		}

		m_pLastChild = pRecord;
		pRecord->m_pParent = this;
	}

	void StopBlock();

	void Print(FILE* file, char* buf, size_t buf_size, size_t depth, LARGE_INTEGER stopTime, LARGE_INTEGER frequency, const char* threadName, const float timeThreshold)
	{
		if (m_stopTimeStamp.QuadPart == 0)
			m_stopTimeStamp = stopTime;

		const float time = (float)(m_stopTimeStamp.QuadPart - m_startTimeStamp.QuadPart) * 1000.f / (float)frequency.QuadPart;

		if (timeThreshold > 0.0f && time < timeThreshold)
		{
			return;
		}

		stack_string tabs;
		tabs.insert(0, depth++, '\t');

		{
			stack_string label = m_label;
			label.replace("&", "&amp;");
			label.replace("<", "&lt;");
			label.replace(">", "&gt;");
			label.replace("\"", "&quot;");
			label.replace("'", "&apos;");

			if (m_args.size() > 0)
			{
				m_args.replace("&", "&amp;");
				m_args.replace("<", "&lt;");
				m_args.replace(">", "&gt;");
				m_args.replace("\"", "&quot;");
				m_args.replace("'", "&apos;");
			}

			cry_sprintf(buf, buf_size, "%s<block name=\"%s\" totalTimeMS=\"%f\" startTime=\"%" PRIu64 "\" stopTime=\"%" PRIu64 "\" args=\"%s\"> \n",
			            tabs.c_str(), label.c_str(), time, m_startTimeStamp.QuadPart, m_stopTimeStamp.QuadPart, m_args.c_str());
			fprintf(file, "%s", buf);
		}

		for (CBootProfilerRecord* pRecord = m_pFirstChild; pRecord; pRecord = pRecord->m_pNextSibling)
		{
			pRecord->Print(file, buf, buf_size, depth, m_stopTimeStamp, frequency, threadName, timeThreshold);
		}

		cry_sprintf(buf, buf_size, "%s</block>\n", tabs.c_str());
		fprintf(file, "%s", buf);
	}
};

//////////////////////////////////////////////////////////////////////////

class CRecordPool
{
public:
	CRecordPool() : m_baseAddr(nullptr), m_allocCounter(0), m_next(nullptr)
	{
		m_baseAddr = (CBootProfilerRecord*)CryModuleMemalign(eNUM_RECORDS_PER_POOL * sizeof(CBootProfilerRecord), 16);
	}

	~CRecordPool()
	{
		CryModuleMemalignFree(m_baseAddr);
		delete m_next;
	}

	ILINE CBootProfilerRecord* allocateRecord()
	{
		if (m_allocCounter < eNUM_RECORDS_PER_POOL)
		{
			CBootProfilerRecord* newRecord = m_baseAddr + m_allocCounter;
			++m_allocCounter;
			return newRecord;
		}
		else
		{
			return nullptr;
		}
	}

	ILINE void setNextPool(CRecordPool* pool) { m_next = pool; }

private:
	CBootProfilerRecord* m_baseAddr;
	uint32               m_allocCounter;

	CRecordPool*         m_next;
};

class CBootProfilerSession : public CProfileBlockTimes
{
	friend class CBootProfilerRecord;

public:
	CBootProfilerSession(const char* szName);
	~CBootProfilerSession();

	void                 Start();
	void                 Stop();

	CBootProfilerRecord* StartBlock(const char* name, const char* args, const unsigned int threadIndex);

	float GetTotalTime() const
	{
		const float time = (float)(m_stopTimeStamp.QuadPart - m_startTimeStamp.QuadPart) * 1000.f / (float)m_frequency.QuadPart;
		return time;
	}

	bool CanBeDeleted() const 
	{
		for (int i = 0; i < eMAX_THREADS_TO_PROFILE; ++i)
		{
			if (m_threadEntry[i].m_blocksStarted != 0)
			{
				return false;
			}
		}

		return true;
	}

	void CollectResults(const float functionMinTimeThreshold);

	const char* GetName() const
	{
		return m_name.c_str();
	}

	void SetName(const char* name)
	{
		m_name = name;
	}

	struct CRY_ALIGN(CACHE_LINE_SIZE_BP_INTERNAL) SThreadEntry
	{
		union
		{
			struct
			{
				CBootProfilerRecord* m_pRootRecord;
				CBootProfilerRecord* m_pCurrentRecord;
				CRecordPool*         m_pRecordsPool;
				unsigned int         m_totalBlocks;
				unsigned int         m_blocksStarted;
			};
			volatile char m_padding[CACHE_LINE_SIZE_BP_INTERNAL];
		};
	};

	LARGE_INTEGER GetFrequency() const { return m_frequency; }
	const SThreadEntry* GetThreadEntries() const { return m_threadEntry; }

private:

	SThreadEntry        m_threadEntry[eMAX_THREADS_TO_PROFILE];

	CryFixedStringT<32> m_name;

	LARGE_INTEGER       m_frequency;
};

//////////////////////////////////////////////////////////////////////////

CBootProfilerSession::CBootProfilerSession(const char* szName) : m_name(szName)
{
	memset(m_threadEntry, 0, sizeof(m_threadEntry));

	QueryPerformanceFrequency(&m_frequency);
}

CBootProfilerSession::~CBootProfilerSession()
{
	const unsigned int nThreadCount = gThreadsInterface.GetThreadCount();
	for (unsigned int i = 0; i < nThreadCount; ++i)
	{
		SThreadEntry& entry = m_threadEntry[i];
		delete entry.m_pRecordsPool;
	}
}

void CBootProfilerSession::Start()
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	m_startTimeStamp = time;
}

void CBootProfilerSession::Stop()
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	m_stopTimeStamp = time;
}

CBootProfilerRecord* CBootProfilerSession::StartBlock(const char* name, const char* args, const unsigned int threadIndex)
{
	assert(threadIndex < eMAX_THREADS_TO_PROFILE);

	SThreadEntry& entry = m_threadEntry[threadIndex];
	++entry.m_totalBlocks;
	++entry.m_blocksStarted;

	if (!entry.m_pRootRecord)
	{
		CRecordPool* pPool = new CRecordPool;
		entry.m_pRecordsPool = pPool;

		CBootProfilerRecord* rec = pPool->allocateRecord();
		entry.m_pRootRecord = entry.m_pCurrentRecord = new(rec) CBootProfilerRecord("root", m_startTimeStamp, threadIndex, args, this);
	}

	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);

	assert(entry.m_pRootRecord);

	CRecordPool* pPool = entry.m_pRecordsPool;
	assert(pPool);
	CBootProfilerRecord* rec = pPool->allocateRecord();
	if (!rec)
	{
		//pool is full, create a new one
		pPool = new CRecordPool;
		pPool->setNextPool(entry.m_pRecordsPool);
		entry.m_pRecordsPool = pPool;
		rec = pPool->allocateRecord();
	}

	CBootProfilerRecord* pNewRecord = new(rec) CBootProfilerRecord(name, time, threadIndex, args, this);
	entry.m_pCurrentRecord->AddChild(pNewRecord);
	entry.m_pCurrentRecord = pNewRecord;

	return pNewRecord;
}

static void SaveProfileSessionToDisk(const float funcMinTimeThreshold, CBootProfilerSession* pSession)
{
	if (!(gEnv && gEnv->pCryPak))
	{
		CBootProfiler::GetInstance().QueueSessionToDelete(pSession);
		return;
	}

	static const char* szTestResults = "%USER%/TestResults";
	stack_string filePath; 
	filePath.Format("%s\\bp_%s.xml", szTestResults, pSession->GetName());
	char path[ICryPak::g_nMaxPath] = "";
	gEnv->pCryPak->AdjustFileName(filePath.c_str(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
	gEnv->pCryPak->MakeDir(szTestResults);

	FILE* pFile = ::fopen(path, "wb");
	if (!pFile)
	{
		CBootProfiler::GetInstance().QueueSessionToDelete(pSession);
		return; //TODO: use accessible path when punning from package on durango
	}

	char buf[512];
	const unsigned int buf_size = sizeof(buf);

	cry_sprintf(buf, buf_size, "<root>\n");
	fprintf(pFile, "%s", buf);

	const size_t numThreads = gThreadsInterface.GetThreadCount();
	for (size_t i = 0; i < numThreads; ++i)
	{
		const CBootProfilerSession::SThreadEntry& entry = pSession->GetThreadEntries()[i];
		CBootProfilerRecord* pRoot = entry.m_pRootRecord;
		if (pRoot)
		{
			pRoot->m_stopTimeStamp = pSession->GetStopTimeStamp();

			const char* threadName = gThreadsInterface.GetThreadNameByIndex(i);
			if (!threadName)
				threadName = "UNKNOWN";

			const float time = (float)(pRoot->m_stopTimeStamp.QuadPart - pRoot->m_startTimeStamp.QuadPart) * 1000.f / (float)pSession->GetFrequency().QuadPart;

			cry_sprintf(buf, buf_size, "\t<thread name=\"%s\" totalTimeMS=\"%f\" startTime=\"%" PRIu64 "\" stopTime=\"%" PRIu64 "\" totalBlocks=\"%u\"> \n", threadName, time,
				pRoot->m_startTimeStamp.QuadPart, pRoot->m_stopTimeStamp.QuadPart, entry.m_totalBlocks);
			fprintf(pFile, "%s", buf);

			for (CBootProfilerRecord* pRecord = pRoot->m_pFirstChild; pRecord; pRecord = pRecord->m_pNextSibling)
			{
				pRecord->Print(pFile, buf, buf_size, 2, pSession->GetStopTimeStamp(), pSession->GetFrequency(), threadName, funcMinTimeThreshold);
			}

			cry_sprintf(buf, buf_size, "\t</thread>\n");
			fprintf(pFile, "%s", buf);
		}
	}

	cry_sprintf(buf, buf_size, "</root>\n");
	fprintf(pFile, "%s", buf);

	::fclose(pFile);

	CBootProfiler::GetInstance().QueueSessionToDelete(pSession);
}

void CBootProfilerSession::CollectResults(const float functionMinTimeThreshold)
{
	if (CBootProfiler::GetInstance().CV_sys_bp_frames_worker_thread)
	{
		CBootProfiler::GetInstance().QueueSessionToSave(functionMinTimeThreshold, this);
	}
	else
	{
		SaveProfileSessionToDisk(functionMinTimeThreshold, this);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CBootProfilerRecord::StopBlock()
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	m_stopTimeStamp = time;
	assert(m_threadIndex < eMAX_THREADS_TO_PROFILE);

	CBootProfilerSession::SThreadEntry& entry = m_pSession->m_threadEntry[m_threadIndex];
	entry.m_pCurrentRecord = m_pParent;
	--entry.m_blocksStarted;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CBootProfiler& CBootProfiler::GetInstance()
{
	return gProfilerInstance;
}

CBootProfiler::CBootProfiler()
	: m_pCurrentSession(nullptr)
	, m_quitSaveThread(false)
	, m_pMainThreadFrameRecord(nullptr)
	, m_levelLoadAdditionalFrames(0)
	, m_countdownToNextSaveSesssion(0)
{
}

CBootProfiler::~CBootProfiler()
{
	StopSaveSessionsThread();

	if (gEnv && gEnv->pSystem)
	{
		ISystemEventDispatcher* pSystemEventDispatcher = gEnv->pSystem->GetISystemEventDispatcher();
		if (pSystemEventDispatcher)
		{
			pSystemEventDispatcher->RemoveListener(this);
		}
	}

	delete m_pCurrentSession;
}

// start session
void CBootProfiler::StartSession(const char* sessionName)
{
	if (m_pCurrentSession)
	{
		CryLogAlways("BootProfiler: failed to start session '%s' as another one is active '%s'", sessionName, m_pCurrentSession->GetName());
		return;
	}

	gThreadsInterface.Reset();
	CBootProfilerSession* pSession = new CBootProfilerSession(sessionName);
	pSession->Start();
	m_pCurrentSession = pSession;
}

// stop session
void CBootProfiler::StopSession()
{
	if (m_pCurrentSession)
	{
		gEnv->bBootProfilerEnabledFrames = false;

		CBootProfilerSession* session = m_pCurrentSession;
		m_pCurrentSession = nullptr;

		session->Stop();
		session->CollectResults(CV_sys_bp_time_threshold);
	}
}

CBootProfilerRecord* CBootProfiler::StartBlock(const char* name, const char* args)
{
	if (CBootProfilerSession* pSession = m_pCurrentSession)
	{
		const threadID curThread = CryGetCurrentThreadId();
		const unsigned int threadIndex = gThreadsInterface.GetThreadIndexByID(curThread);
		return pSession->StartBlock(name, args, threadIndex);
	}
	return nullptr;
}

void CBootProfiler::StopBlock(CBootProfilerRecord* record)
{
	if (record)
	{
		record->StopBlock();
	}
}

void CBootProfiler::StartFrame(const char* name)
{
	static int prev_CV_sys_bp_frames = CV_sys_bp_frames;
	if (CV_sys_bp_frames > 0)
	{
		if (prev_CV_sys_bp_frames == 0)
		{
			StartSession("frame");
			gEnv->bBootProfilerEnabledFrames = true;
		}

		m_pMainThreadFrameRecord = StartBlock(name, nullptr);

		if (CV_sys_bp_frames_threshold != 0.0f) // we can't have 2 modes enabled at the same time
			CV_sys_bp_frames_threshold = 0.0f;
	}

	prev_CV_sys_bp_frames = CV_sys_bp_frames;

	static float prev_CV_sys_bp_frames_threshold = CV_sys_bp_frames_threshold;
	if (prev_CV_sys_bp_frames_threshold == 0.0f && CV_sys_bp_frames_threshold != 0.0f)
	{
		gThreadsInterface.Reset();
		gEnv->bBootProfilerEnabledFrames = true;
	}
	prev_CV_sys_bp_frames_threshold = CV_sys_bp_frames_threshold;

	if (CV_sys_bp_frames_threshold != 0.0f)
	{
		if (m_pCurrentSession)
		{
			CryLogAlways("BootProfiler: failed to start session 'frame_threshold' as another one is active '%s'", m_pCurrentSession->GetName());
		}
		else
		{
			CBootProfilerSession* pSession = new CBootProfilerSession("frame_threshold");
			pSession->Start();
			m_pCurrentSession = pSession;
			m_pMainThreadFrameRecord = StartBlock(name, nullptr);
		}
	}
}

void CBootProfiler::StopFrame()
{
	if (!m_pCurrentSession)
		return;

	if (CV_sys_bp_frames)
	{
		m_pMainThreadFrameRecord->StopBlock();
		m_pMainThreadFrameRecord = nullptr;

		--CV_sys_bp_frames;
		if (0 == CV_sys_bp_frames)
		{
			StopSession();
		}
	}

	if (m_levelLoadAdditionalFrames)
	{
		--m_levelLoadAdditionalFrames;
		if (0 == m_levelLoadAdditionalFrames)
		{
			StopSession();
		}
	}

	// Do cleanup on following already saved sessions
	if (m_sessionsToDelete.size() >= 2)
	{
		for (size_t i = 0, count = m_sessionsToDelete.size(); i < count; ++i)
		{
			CBootProfilerSession* pSession = m_sessionsToDelete[i];
			if (pSession->CanBeDeleted())
			{
				delete pSession;
				m_sessionsToDelete[i] = nullptr;
			}
		}

		m_sessionsToDelete.try_remove_and_erase_if([](CBootProfilerSession* pSession) { return pSession == nullptr; });
	}

	static float prev_CV_sys_bp_frames_threshold = CV_sys_bp_frames_threshold;
	const bool bDisablingThresholdMode = (prev_CV_sys_bp_frames_threshold > 0.0f && CV_sys_bp_frames_threshold == 0.0f);

	if (CV_sys_bp_frames_threshold > 0.0f || bDisablingThresholdMode)
	{
		m_pMainThreadFrameRecord->StopBlock();
		m_pMainThreadFrameRecord = nullptr;

		m_pCurrentSession->Stop();

		--m_countdownToNextSaveSesssion;
		const bool bShouldCollectResults = m_countdownToNextSaveSesssion < 0;

		if (bShouldCollectResults)
		{
			const int nextOffset = cry_random(0, CV_sys_bp_frames_sample_period_rnd);
			m_countdownToNextSaveSesssion = crymath::clamp(CV_sys_bp_frames_sample_period + nextOffset, 0, std::numeric_limits<int>::max());
		}

		if ((m_pCurrentSession->GetTotalTime() >= CV_sys_bp_frames_threshold) && !bDisablingThresholdMode && bShouldCollectResults)
		{
			CBootProfilerSession* pSession = m_pCurrentSession;
			m_pCurrentSession = nullptr;

			static int frameNumber = 0;
			const int frameNum = gEnv->pRenderer ? gEnv->pRenderer->GetFrameID(false) : ++frameNumber;

			CryFixedStringT<32> sessionName;
			sessionName.Format("frame_%d_%3.1fms", frameNum, CV_sys_bp_frames_threshold);
			pSession->SetName(sessionName.c_str());
			pSession->CollectResults(CV_sys_bp_time_threshold);
		}
		else
		{
			m_sessionsToDelete.push_back(m_pCurrentSession);
			m_pCurrentSession = nullptr;
		}

		if (bDisablingThresholdMode)
		{
			gEnv->bBootProfilerEnabledFrames = false;
		}
	}

	prev_CV_sys_bp_frames_threshold = CV_sys_bp_frames_threshold;
}

void CBootProfiler::StopSaveSessionsThread()
{
	m_quitSaveThread = true;
	m_saveThreadWakeUpEvent.Set();
}

void CBootProfiler::QueueSessionToDelete(CBootProfilerSession*pSession)
{
	m_sessionsToDelete.push_back(pSession);
}

void CBootProfiler::QueueSessionToSave(float functionMinTimeThreshold, CBootProfilerSession* pSession)
{
	m_sessionsToSave.push_back(SSaveSessionInfo(functionMinTimeThreshold, pSession));
	m_saveThreadWakeUpEvent.Set();
}

void CBootProfiler::ThreadEntry()
{
	while (!m_quitSaveThread)
	{
		m_saveThreadWakeUpEvent.Wait();
		m_saveThreadWakeUpEvent.Reset();

		for (size_t i = 0; i < m_sessionsToSave.size(); ++i)
		{
			SaveProfileSessionToDisk(m_sessionsToSave[i].functionMinTimeThreshold, m_sessionsToSave[i].pSession);
		}

		m_sessionsToSave.clear();
	}
}

void CBootProfiler::Init(ISystem* pSystem)
{
	pSystem->GetISystemEventDispatcher()->RegisterListener(this,"CBootProfiler");
	StartSession("boot");
}

void CBootProfiler::RegisterCVars()
{
	REGISTER_CVAR2("sys_bp_frames_worker_thread", &CV_sys_bp_frames_worker_thread, 0, VF_DEV_ONLY | VF_REQUIRE_APP_RESTART, "If this is set to true. The system will dump the profiled session from a different thread.");
	REGISTER_CVAR2("sys_bp_frames", &CV_sys_bp_frames, 0, VF_DEV_ONLY, "Starts frame profiling for specified number of frames using BootProfiler");
	REGISTER_CVAR2("sys_bp_frames_sample_period", &CV_sys_bp_frames_sample_period, 0, VF_DEV_ONLY, "When in threshold mode, the period at which we are going to dump a frame.");
	REGISTER_CVAR2("sys_bp_frames_sample_period_rnd", &CV_sys_bp_frames_sample_period_rnd, 0, VF_DEV_ONLY, "When in threshold mode, the random offset at which we are going to dump a next frame.");
	REGISTER_CVAR2("sys_bp_frames_threshold", &CV_sys_bp_frames_threshold, 0, VF_DEV_ONLY, "Starts frame profiling but gathers the results for frames that frame time exceeded the threshold");
	REGISTER_CVAR2("sys_bp_time_threshold", &CV_sys_bp_time_threshold, 0.1f, VF_DEV_ONLY, "If greater than 0 don't write blocks that took less time (default 0.1 ms)");

	if (CV_sys_bp_frames_worker_thread)
	{
		gEnv->pThreadManager->SpawnThread(this, "BootProfiler");
	}
}

void CBootProfiler::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_SANDBOX_POST_INIT_DONE:
		{
			StopSession();
			break;
		}
	case ESYSTEM_EVENT_GAME_POST_INIT_DONE:
		{
			if (!gEnv->IsEditor())
			{
				StopSession();
			}
			break;
		}
	case ESYSTEM_EVENT_GAME_MODE_SWITCH_START:
		{
			break;
		}

	case ESYSTEM_EVENT_GAME_MODE_SWITCH_END:
		{
			break;
		}

	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		{
			break;
		}
	case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
		{
			CV_sys_bp_time_threshold = 0.1f;
			StartSession("level");
			break;
		}
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			break;
		}
	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		{
			//level loading can be stopped here, or m_levelLoadAdditionalFrames can be used to prolong dump for this amount of frames
			StopSession();
			//m_levelLoadAdditionalFrames = 20;

			CV_sys_bp_time_threshold = 0.0f; //gather all blocks when in runtime
			break;
		}
	case ESYSTEM_EVENT_FAST_SHUTDOWN:
	case ESYSTEM_EVENT_FULL_SHUTDOWN:
		{
			StopSaveSessionsThread();
			break;
		}
	}
}

#endif
