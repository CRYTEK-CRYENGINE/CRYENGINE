// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
	
	ILINE threadID GetThreadIdByIndex(unsigned int threadIndex)
	{
		assert(threadIndex < m_threadCounter);
		return m_threadInfo[threadIndex];
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

int CBootProfiler::CV_sys_bp_enabled = 1;
int CBootProfiler::CV_sys_bp_level_load = 1;
int CBootProfiler::CV_sys_bp_frames_worker_thread = 0;
int CBootProfiler::CV_sys_bp_frames = 0;
int CBootProfiler::CV_sys_bp_frames_sample_period = 0;
int CBootProfiler::CV_sys_bp_frames_sample_period_rnd = 0;
float CBootProfiler::CV_sys_bp_frames_threshold = 0.0f;
float CBootProfiler::CV_sys_bp_time_threshold = 0.0f;
EBootProfilerFormat CBootProfiler::CV_sys_bp_output_formats = EBootProfilerFormat::XML;

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

	unsigned int          m_threadIndex;
	EProfileDescription   m_type;
	string                m_args;

	const char*           m_label;
	LARGE_INTEGER         m_startTimeStamp;
	LARGE_INTEGER         m_stopTimeStamp;

	CBootProfilerRecord*  m_pParent;

	CBootProfilerRecord*  m_pFirstChild;
	CBootProfilerRecord*  m_pLastChild;
	CBootProfilerRecord*  m_pNextSibling;
	CBootProfilerSession* m_pSession;


	ILINE CBootProfilerRecord(const char* label, LARGE_INTEGER timestamp, unsigned int threadIndex, const char* args, CBootProfilerSession* pSession,EProfileDescription type)
		:	m_label(label)
		, m_startTimeStamp(timestamp)
		, m_threadIndex(threadIndex)
		, m_pParent(nullptr)
		, m_pFirstChild(nullptr)
		, m_pLastChild(nullptr)
		, m_pNextSibling(nullptr)
		, m_pSession(pSession)
		, m_type(type)
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

	CBootProfilerRecord* StartBlock(const char* name, const char* args, const unsigned int threadIndex, EProfileDescription type);

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
	for (unsigned int i = 0; i < eMAX_THREADS_TO_PROFILE; ++i)
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

CBootProfilerRecord* CBootProfilerSession::StartBlock(const char* name, const char* args, const unsigned int threadIndex, EProfileDescription type)
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
		entry.m_pRootRecord = entry.m_pCurrentRecord = new(rec) CBootProfilerRecord("root", m_startTimeStamp, threadIndex, args, this,type);
	}
	
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

	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);

	CBootProfilerRecord* pNewRecord = new(rec) CBootProfilerRecord(name, time, threadIndex, args, this,type);
	entry.m_pCurrentRecord->AddChild(pNewRecord);
	entry.m_pCurrentRecord = pNewRecord;

	uint32 profilerType = type & EProfileDescription::TYPE_MASK;
	if (profilerType == EProfileDescription::MARKER || profilerType == EProfileDescription::PUSH_MARKER || profilerType == EProfileDescription::POP_MARKER)
	{
		// When pushing markers immediately stop block
		pNewRecord->StopBlock();
	}

	return pNewRecord;
}

struct BootProfilerSessionSerializerToJSON
{
	CBootProfilerSession* pSession = nullptr;
	float funcMinTimeThreshold = 0;

	BootProfilerSessionSerializerToJSON() {}
	BootProfilerSessionSerializerToJSON(CBootProfilerSession* pS,float threshold ) : pSession(pS),funcMinTimeThreshold(threshold) {}

	struct SSerializeFixedStringArg
	{
		string str;
		const char *label;
		void Serialize(Serialization::IArchive& ar)
		{
			ar(str,label);
		}
	};
	struct SSerializeIntArg
	{
		uint32 arg;
		const char *label;
		void Serialize(Serialization::IArchive& ar)
		{
			ar(arg, label);
		}
	};
	struct SSerializeLambda
	{
		std::function<void(Serialization::IArchive& ar)> lambda;
		void Serialize(Serialization::IArchive& ar)
		{
			lambda(ar);
		}
	};

	enum class EventType { RECORD,THREAD_NAME,THREAD_SORT };
	struct BootProfilerEventSerializerToJSON
	{
		CBootProfilerSession* pSession = nullptr;
		CBootProfilerRecord* pRecord = nullptr;
		threadID threadId = 0;
		EventType type = EventType::RECORD;

		BootProfilerEventSerializerToJSON() {}
		BootProfilerEventSerializerToJSON(CBootProfilerSession* pS,CBootProfilerRecord* pR,threadID tid,EventType et)
			: pSession(pS),pRecord(pR),threadId(tid),type(et) {}

		// Serializes into the Chrome trace compatible JSON format
		void Serialize(Serialization::IArchive& ar)
		{
			static uint32 processId = 0;
#ifdef WIN32
			processId = ::GetCurrentProcessId();
#endif
			ar(processId, "pid");
			ar(threadId,  "tid");

			if (type == EventType::THREAD_NAME)
			{
				ar(string("thread_name"),"name");
				ar(string("M"),"ph");

				string threadName = GetISystem()->GetIThreadManager()->GetThreadName(threadId);
				ar(SSerializeFixedStringArg{threadName,"name"},"args");
				return;
			}
			else if (type == EventType::THREAD_SORT)
			{
				static int counter = 0;
				ar(string("thread_sort_index"), "name");
				ar(string("M"), "ph");
				ar(SSerializeIntArg{(uint32)counter++,"sort_index"},"args");
				return;
			}

			string label = pRecord->m_label;
			label.replace("\"", "&quot;");
			label.replace("'", "&apos;");

			if (pRecord->m_args.size() > 0)
			{
				pRecord->m_args.replace("\"", "&quot;");
				pRecord->m_args.replace("'", "&apos;");
			}

			{
				double time = (double)(pRecord->m_stopTimeStamp.QuadPart - pRecord->m_startTimeStamp.QuadPart) * 1000000.0 / (double)pSession->GetFrequency().QuadPart;
				double timeStart = (double)(pRecord->m_startTimeStamp.QuadPart) * 1000000.0 / (double)pSession->GetFrequency().QuadPart; // microseconds

				//static int timeStart = 0;
				//timeStart++;
				//int time = 10;

				static string category = "PERF";

				ar(label, "name");

				uint32 profilerType = pRecord->m_type & EProfileDescription::TYPE_MASK; //FUNCTIONENTRY,REGION,SECTION
				bool bWaiting = (pRecord->m_type & EProfileDescription::WAITING) != 0;
				
				// Events Documentation: https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview#heading=h.lenwiilchoxp
				if (profilerType == EProfileDescription::MARKER)
				{
					// Instant Event
					static string eventType = "i";
					static string category = "MARKER";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStart, "ts");
				}
				else if (profilerType == EProfileDescription::PUSH_MARKER)
				{
					// Complete Event
					static string eventType = "B";
					static string category = "MARKER";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStart, "ts");
				}
				else if (profilerType == EProfileDescription::POP_MARKER)
				{
					// Complete Event
					static string eventType = "E";
					static string category = "MARKER";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStart, "ts");
				}
				else if (profilerType == EProfileDescription::SECTION)
				{
					// Complete Event
					static string eventType = "X";
					static string category = "SECTION";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStart, "ts");
					ar(time, "dur");
				}
				else if (profilerType == EProfileDescription::REGION)
				{
					// Complete Event
					static string eventType = "X";
					static string category = "REGION";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStart, "ts");
					ar(time, "dur");
				}
				else
				{
					// Complete Event
					static string eventType = "X";
					static string category = "PERF";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStart, "ts");
					ar(time, "dur");
				}
				
				if (!pRecord->m_args.empty())
				{
					ar(SSerializeFixedStringArg{pRecord->m_args,"arg"},"args");
				}
			}
		}
	};

	void CollectProfilers(CBootProfilerRecord* pRecord,threadID threadId,std::vector<BootProfilerEventSerializerToJSON>& profilers)
	{
		// Serializes into the Chrome trace compatible JSON format
		if (pRecord->m_stopTimeStamp.QuadPart == 0)
			pRecord->m_stopTimeStamp = pSession->GetStopTimeStamp();

		const double time = (double)(pRecord->m_stopTimeStamp.QuadPart - pRecord->m_startTimeStamp.QuadPart) * 1000.f / (double)pSession->GetFrequency().QuadPart;

		if (funcMinTimeThreshold == 0 || time > funcMinTimeThreshold)
		{
			profilers.push_back(BootProfilerEventSerializerToJSON(pSession,pRecord,threadId,EventType::RECORD));
		}

		for (CBootProfilerRecord* pNewRecord = pRecord->m_pFirstChild; pNewRecord; pNewRecord = pNewRecord->m_pNextSibling)
		{
			CollectProfilers(pNewRecord,threadId,profilers);
		}
	}

	void Serialize(Serialization::IArchive& ar)
	{
		int pid = 0;
#ifdef WIN32
		pid = ::GetCurrentProcessId();
#endif

		std::vector<BootProfilerEventSerializerToJSON> threadProfilers;

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
				
				threadID tid = gThreadsInterface.GetThreadIdByIndex(i);

				threadProfilers.push_back(BootProfilerEventSerializerToJSON(pSession,nullptr,tid,EventType::THREAD_NAME));
				threadProfilers.push_back(BootProfilerEventSerializerToJSON(pSession,nullptr,tid,EventType::THREAD_SORT));
				for (CBootProfilerRecord* pRecord = pRoot->m_pFirstChild; pRecord; pRecord = pRecord->m_pNextSibling)
				{
					CollectProfilers(pRecord,tid,threadProfilers);
				}
			}
		}

		ar(threadProfilers,"traceEvents");
		ar(string("ms"),"displayTimeUnit");
		//{"name": "thread_name", "ph" : "M", "pid" : 2343, "tid" : 2347,
			//"args" : {
			//"name" : "RendererThread"
		//}
	}
};

static void SaveProfileSessionToChromeTraceJson(const float funcMinTimeThreshold, CBootProfilerSession* pSession)
{
	static const char* szTestResults = "%USER%/TestResults";
	stack_string filePath;
	filePath.Format("%s\\chrome_trace_%s.json", szTestResults, pSession->GetName());
	gEnv->pCryPak->MakeDir(szTestResults);

	GetISystem()->GetArchiveHost()->SaveJsonFile(filePath,Serialization::SStruct(BootProfilerSessionSerializerToJSON(pSession,funcMinTimeThreshold)));
}

static void SaveProfileSessionToXML(CBootProfilerSession* pSession, const float funcMinTimeThreshold)
{
	static const char* szTestResults = "%USER%/TestResults";
	stack_string filePath;
	filePath.Format("%s\\bp_%s.xml", szTestResults, pSession->GetName());
	char path[ICryPak::g_nMaxPath] = "";
	gEnv->pCryPak->AdjustFileName(filePath.c_str(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
	gEnv->pCryPak->MakeDir(szTestResults);

	FILE* pFile = ::fopen(path, "wb");
	if (!pFile)
	{
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
}

static void SaveProfileSessionToDisk(const float funcMinTimeThreshold, CBootProfilerSession* pSession, EBootProfilerFormat outputFormat)
{
	if (!(gEnv && gEnv->pCryPak))
	{
		CBootProfiler::GetInstance().QueueSessionToDelete(pSession);
		return;
	}

	if (outputFormat == EBootProfilerFormat::XML)
	{
		SaveProfileSessionToXML(pSession, funcMinTimeThreshold);
	}
	else if (outputFormat == EBootProfilerFormat::ChromeTraceJSON)
	{
		SaveProfileSessionToChromeTraceJson(funcMinTimeThreshold, pSession);
	}

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
		SaveProfileSessionToDisk(functionMinTimeThreshold, this, CBootProfiler::GetInstance().CV_sys_bp_output_formats);
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
	if (!CV_sys_bp_enabled)
	{
		return;
	}

	if (m_pCurrentSession)
	{
		CryLogAlways("BootProfiler: failed to start session '%s' as another one is active '%s'", sessionName, m_pCurrentSession->GetName());
		return;
	}

	gThreadsInterface.Reset();
	CBootProfilerSession* pSession = new CBootProfilerSession(sessionName);
	pSession->Start();
	m_pCurrentSession = pSession;
	gEnv->bBootProfilerEnabledFrames = true;
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

CBootProfilerRecord* CBootProfiler::StartBlock(const char* name, const char* args, EProfileDescription type)
{
	if (CBootProfilerSession* pSession = m_pCurrentSession)
	{
		const threadID curThread = CryGetCurrentThreadId();
		const unsigned int threadIndex = gThreadsInterface.GetThreadIndexByID(curThread);
		return pSession->StartBlock(name, args, threadIndex, type);
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
		}

		m_pMainThreadFrameRecord = StartBlock(name, nullptr,EProfileDescription::REGION);

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
			m_pMainThreadFrameRecord = StartBlock(name, nullptr,EProfileDescription::REGION);
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
	if (!m_quitSaveThread)
	{
		m_quitSaveThread = true;
		m_saveThreadWakeUpEvent.Set();

		if (gEnv)
		{
			gEnv->pThreadManager->JoinThread(this, eJM_Join);
		}
	}
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
			SaveProfileSessionToDisk(m_sessionsToSave[i].functionMinTimeThreshold, m_sessionsToSave[i].pSession, CV_sys_bp_output_formats);
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
	REGISTER_CVAR2("sys_bp_enabled", &CV_sys_bp_enabled, 1, VF_DEV_ONLY, "If this is set to false, new boot profiler sessions will not be started.");
	REGISTER_CVAR2("sys_bp_level_load", &CV_sys_bp_level_load, 1, VF_DEV_ONLY, "If this is set to true, a boot profiler session will be started to profile the level loading. Ignored if sys_bp_enabled is false.");
	REGISTER_CVAR2("sys_bp_frames_worker_thread", &CV_sys_bp_frames_worker_thread, 0, VF_DEV_ONLY | VF_REQUIRE_APP_RESTART, "If this is set to true. The system will dump the profiled session from a different thread.");
	REGISTER_CVAR2("sys_bp_frames", &CV_sys_bp_frames, 0, VF_DEV_ONLY, "Starts frame profiling for specified number of frames using BootProfiler");
	REGISTER_CVAR2("sys_bp_frames_sample_period", &CV_sys_bp_frames_sample_period, 0, VF_DEV_ONLY, "When in threshold mode, the period at which we are going to dump a frame.");
	REGISTER_CVAR2("sys_bp_frames_sample_period_rnd", &CV_sys_bp_frames_sample_period_rnd, 0, VF_DEV_ONLY, "When in threshold mode, the random offset at which we are going to dump a next frame.");
	REGISTER_CVAR2("sys_bp_frames_threshold", &CV_sys_bp_frames_threshold, 0, VF_DEV_ONLY, "Starts frame profiling but gathers the results for frames that frame time exceeded the threshold");
	REGISTER_CVAR2("sys_bp_time_threshold", &CV_sys_bp_time_threshold, 0.1f, VF_DEV_ONLY, "If greater than 0 don't write blocks that took less time (default 0.1 ms)");
	REGISTER_CVAR2("sys_bp_format", &CV_sys_bp_output_formats, EBootProfilerFormat::XML, VF_DEV_ONLY, "Determines the output format for the boot profiler.\n0 = XML\n1 = Chrome Trace JSON");

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
			if (CV_sys_bp_level_load)
			{
				StartSession("level");
			}
			break;
		}
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			break;
		}
	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		{
			if (m_pCurrentSession)
			{
				//level loading can be stopped here, or m_levelLoadAdditionalFrames can be used to prolong dump for this amount of frames
				StopSession();
				//m_levelLoadAdditionalFrames = 20;
			}

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
