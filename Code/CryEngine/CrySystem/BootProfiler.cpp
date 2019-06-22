// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if defined(ENABLE_LOADING_PROFILER)

#include "BootProfiler.h"
#include "ThreadInfo.h"
#include "CryMath/Cry_Math.h"

#include <CryGame/IGameFramework.h>
#include <CryRenderer/IRenderer.h>
#include <CryMath/Random.h>
#include <CrySystem/ConsoleRegistration.h>

// debugging of bootprofiler
#if 0
#define BOOTPROF_ASSERT(...) assert(__VA_ARGS__)
#else
#define BOOTPROF_ASSERT(...)
#endif

namespace
{
	#define CACHE_LINE_SIZE_BP_INTERNAL 64

enum
{
	eMAX_THREADS_TO_PROFILE = 64,
	eNUM_RECORDS_PER_POOL   = 6553,      // so, eNUM_RECORDS_PER_POOL * sizeof(CBootProfilerRecord) + sizeof(CMemoryPoolBucket) + alignmentPadding
	// poolmem ~= 512 Kb for 1 pool bucket per thread

	eSTRINGS_MEM_PER_POOL  = 64 * 1024   // actual available memory is a bit less due to header and padding
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
			if (m_threadInfo[i] == threadID)
				return i;
		}

		const unsigned int counter = CryInterlockedIncrement(&m_threadCounter) - 1;     //count to index
		BOOTPROF_ASSERT(counter < eMAX_THREADS_TO_PROFILE);
		m_threadInfo[counter] = threadID;
		
		return counter;
	}

	ILINE const char* GetThreadNameByIndex(unsigned int threadIndex)
	{
		BOOTPROF_ASSERT(threadIndex < m_threadCounter);
		
		if(m_threadNames[threadIndex].empty())
		{
			m_threadNames[threadIndex] = gEnv->pThreadManager->GetThreadName(m_threadInfo[threadIndex]);
			if (m_threadNames[threadIndex].empty())
			{
				m_threadNames[threadIndex].Format("tid_%" PRIu64, static_cast<uint64>(m_threadInfo[threadIndex]));
			}
		}

		return m_threadNames[threadIndex].c_str();
	}
	
	ILINE threadID GetThreadIdByIndex(unsigned int threadIndex)
	{
		BOOTPROF_ASSERT(threadIndex < m_threadCounter);
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

int CBootProfiler::CV_sys_bp_enabled = 0;
int CBootProfiler::CV_sys_bp_level_load = 0;
int CBootProfiler::CV_sys_bp_level_load_include_first_frame = 0;
int CBootProfiler::CV_sys_bp_frames_worker_thread = 0;
int CBootProfiler::CV_sys_bp_frames_sample_period = 0;
int CBootProfiler::CV_sys_bp_frames_sample_period_rnd = 0;
float CBootProfiler::CV_sys_bp_frames_threshold = 0.0f;
float CBootProfiler::CV_sys_bp_time_threshold = 0.0f;
EBootProfilerFormat CBootProfiler::CV_sys_bp_output_formats = EBootProfilerFormat::XML;
ICVar* CBootProfiler::CV_sys_bp_frames_required_label = nullptr;

class CRecordPool;
class CBootProfilerSession;

class CBootProfilerRecord
{
public:
	const char* m_label;
	const char* m_args;

	int64 m_startTime;
	int64 m_endTime;
	float m_durationInMs;

	CBootProfilerRecord*  m_pParent;
	CBootProfilerRecord*  m_pFirstChild;
	CBootProfilerRecord*  m_pLastChild;
	CBootProfilerRecord*  m_pNextSibling;

	ILINE CBootProfilerRecord(CBootProfilerRecord*  pParent, const char* label, const char* args, const int64 startTime)
		: m_label(label)
		, m_args(args)
		, m_startTime(startTime)
		, m_endTime(startTime)
		, m_durationInMs(-1)
		, m_pParent(pParent)
		, m_pFirstChild(nullptr)
		, m_pLastChild(nullptr)
		, m_pNextSibling(nullptr)
	{
		BOOTPROF_ASSERT(m_pParent || strcmp(label, "root") == 0);
		if (m_pParent)
			m_pParent->AddChild(this);
	}

	void AddChild(CBootProfilerRecord* pRecord)
	{
		BOOTPROF_ASSERT(pRecord->m_pParent == this);
		// we have to queue children in order or ProfVis gets confused
		// (could alternatively sort on XML export)
		if (m_pLastChild)
		{
			m_pLastChild->m_pNextSibling = pRecord;
			m_pLastChild = pRecord;
		}
		else
		{
			m_pFirstChild = pRecord;
			m_pLastChild = pRecord;
		}
	}
};

//////////////////////////////////////////////////////////////////////////

class CMemoryPoolBucket
{
public:
	enum : size_t { eAlignment = 16 };
	static constexpr size_t GetHeaderSize()
	{
		return (sizeof(CMemoryPoolBucket) + (eAlignment - 1)) & ~(eAlignment - 1);
	}
	static constexpr size_t GetHeaderSizeWithAllocPadding()
	{
		// CryModuleMemalign internally pads allocation with alignment size
		return GetHeaderSize() + eAlignment;
	}

	static CMemoryPoolBucket* Create(size_t maxBucketSize)
	{
		// allocate both CMemoryPoolBucket and memory for pool data in single allocation
		const size_t headerSize = GetHeaderSize();
		const size_t fullSize = headerSize + maxBucketSize;

		void* p = CryModuleMemalign(fullSize, eAlignment);
		void* pBase = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p) + headerSize);
		return new(p) CMemoryPoolBucket(pBase);
	}

	static void Destroy(CMemoryPoolBucket* pBucket)
	{
		CryModuleMemalignFree(pBucket);
	}

	void* Allocate(size_t size, size_t maxBucketSize)
	{
		if (m_allocatedSize + size <= maxBucketSize)
		{
			void* pNewRecord = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_pBaseAddr) + m_allocatedSize);
			m_allocatedSize += size;
			return pNewRecord;
		}
		else
		{
			return nullptr;
		}
	}

	void SetNextBucket(CMemoryPoolBucket* pPool) { m_pNext = pPool; }
	CMemoryPoolBucket* GetNextBucket() const { return m_pNext; }

private:
	CMemoryPoolBucket(void* pBase)
		: m_pBaseAddr(pBase)
		, m_allocatedSize(0)
		, m_pNext(nullptr)
	{}

private:
	void*                m_pBaseAddr;
	size_t               m_allocatedSize;
	CMemoryPoolBucket*   m_pNext;
};

template <size_t BucketSize>
class CMemoryPool
{
public:
	// NOTE pavloi 2018.02.10: can't have ctor or dtor, as CMemoryPool lives in union of SThreadEntry

	void DestroyBuckets()
	{
		for (CMemoryPoolBucket* pBucket = m_pLastBucket; pBucket != nullptr; )
		{
			CMemoryPoolBucket* pNext = pBucket->GetNextBucket();
			CMemoryPoolBucket::Destroy(pBucket);
			pBucket = pNext;
		}
	}

	void* Allocate(size_t size)
	{
		void* pRec = AllocateMem(size);
		if (pRec == nullptr)
		{
			Grow();
			pRec = AllocateMem(size);
		}
		return pRec;
	}

private:
	void* AllocateMem(size_t size)
	{
		return m_pLastBucket
			? m_pLastBucket->Allocate(size, BucketSize)
			: nullptr;
	}

	void Grow()
	{
		CMemoryPoolBucket* pNewBucket = CMemoryPoolBucket::Create(BucketSize);
		pNewBucket->SetNextBucket(m_pLastBucket);
		m_pLastBucket = pNewBucket;
	}

private:
	CMemoryPoolBucket* m_pLastBucket;
};

class CRecordPool : private CMemoryPool<eNUM_RECORDS_PER_POOL * sizeof(CBootProfilerRecord)>
{
public:
	void Destroy() { DestroyBuckets(); }

	CBootProfilerRecord* AllocateRecord()
	{
		return reinterpret_cast<CBootProfilerRecord*>(Allocate(sizeof(CBootProfilerRecord)));
	}
};

class CStringPool : private CMemoryPool<eSTRINGS_MEM_PER_POOL - CMemoryPoolBucket::GetHeaderSizeWithAllocPadding()>
{
public:
	void Destroy() { DestroyBuckets(); }

	// Copies string into pool. For empty strings - returns nullptr
	char* PutString(const char* szStr)
	{
		if (szStr)
		{
			size_t len = ::strlen(szStr);
			if (len > 0)
			{
				char* szPoolStr = reinterpret_cast<char*>(Allocate(len + 1));
				if (szPoolStr)
				{
					::memcpy(szPoolStr, szStr, len + 1);
				}
				return szPoolStr;
			}
		}
		return nullptr;
	}
};

class CBootProfilerSession
{
public:
	CBootProfilerSession(const char* szName);
	~CBootProfilerSession();

	void                 Start();
	void                 Stop();

	void StartBlock(const char* name, const char* args, int64 startTime, uint threadIndex);
	void StopBlock(int64 endTime, int64 duration, uint threadIndex);

	int64 GetStopTime() const { return m_stopTime; }
	float TicksToMs(int64 ticks) const { return ticks * m_ticksToMs; }
	float GetDurationInMs() const { return TicksToMs(m_stopTime - m_startTime); }

	const char* GetName() const
	{
		return m_name.c_str();
	}

	void SetName(const char* name)
	{
		m_name = name;
	}

	void SetRequiredLabel(bool set)
	{
		m_hasRequiredLabel = set;
	}

	bool HasRequiredLabel() const
	{
		return m_hasRequiredLabel;
	}

	struct CRY_ALIGN(CACHE_LINE_SIZE_BP_INTERNAL) SThreadEntry
	{
		union
		{
			struct
			{
				CBootProfilerRecord* m_pRootRecord;
				CBootProfilerRecord* m_pCurrentRecord;
				CRecordPool          m_recordsPool;
				CStringPool          m_stringPool;
				unsigned int         m_totalBlocks;
				unsigned int         m_blocksStarted;
			};
			volatile char m_padding[CACHE_LINE_SIZE_BP_INTERNAL];
		};
	};
	static_assert(sizeof(SThreadEntry) == CACHE_LINE_SIZE_BP_INTERNAL, "SThreadEntry is too big for one cache line, update padding size");

	const SThreadEntry* GetThreadEntries() const { return m_threadEntry; }

private:
	SThreadEntry        m_threadEntry[eMAX_THREADS_TO_PROFILE];
	CryFixedStringT<32> m_name;
	int64 m_startTime;
	int64 m_stopTime;
	float m_ticksToMs;
	bool  m_hasRequiredLabel;

};

//////////////////////////////////////////////////////////////////////////

CBootProfilerSession::CBootProfilerSession(const char* szName)
	: m_name(szName)
	, m_hasRequiredLabel(false)
{
	LARGE_INTEGER freq;
	if (QueryPerformanceFrequency(&freq))
		m_ticksToMs = 1000.0f / freq.QuadPart;
	else
		m_ticksToMs = 1; // fallback

	memset(m_threadEntry, 0, sizeof(m_threadEntry));
}

CBootProfilerSession::~CBootProfilerSession()
{
	for (unsigned int i = 0; i < eMAX_THREADS_TO_PROFILE; ++i)
	{
		SThreadEntry& entry = m_threadEntry[i];
		entry.m_recordsPool.Destroy();
		entry.m_stringPool.Destroy();
	}
}

void CBootProfilerSession::Start()
{
	m_startTime = CryGetTicks();
}

void CBootProfilerSession::Stop()
{
	m_stopTime = CryGetTicks();
	for(uint i = 0; i < gThreadsInterface.GetThreadCount(); ++i)
	{
		// terminate still running records
		for (CBootProfilerRecord* pRecord = m_threadEntry[i].m_pCurrentRecord; pRecord != nullptr; pRecord = pRecord->m_pParent)
		{
			pRecord->m_endTime = GetStopTime();
			pRecord->m_durationInMs = TicksToMs(pRecord->m_endTime - pRecord->m_startTime);
		}
	}
}

void CBootProfilerSession::StartBlock(const char* name, const char* args, int64 startTime, uint threadIndex)
{
	BOOTPROF_ASSERT(threadIndex < eMAX_THREADS_TO_PROFILE);
	SThreadEntry& entry = m_threadEntry[threadIndex];

	if (!entry.m_pRootRecord)
	{
		CBootProfilerRecord* rec = entry.m_recordsPool.AllocateRecord();
		if (!rec)
		{
			CryFatalError("Unable to allocate memory for new CBootProfilerRecord");
		}

		entry.m_pRootRecord = entry.m_pCurrentRecord = new(rec) CBootProfilerRecord(nullptr, "root", nullptr, m_startTime);
	}	
	BOOTPROF_ASSERT(entry.m_pRootRecord);
	BOOTPROF_ASSERT(entry.m_pCurrentRecord);

	CBootProfilerRecord* rec = entry.m_recordsPool.AllocateRecord();
	if (!rec)
	{
		CryFatalError("Unable to allocate memory for new CBootProfilerRecord");
	}
	
	++entry.m_totalBlocks;
	++entry.m_blocksStarted;
	const char* szArgsInPool = entry.m_stringPool.PutString(args);

	CBootProfilerRecord* pNewRecord = new(rec) CBootProfilerRecord(entry.m_pCurrentRecord, name, szArgsInPool, startTime);
	entry.m_pCurrentRecord = pNewRecord;
}

void CBootProfilerSession::StopBlock(int64 endTime, int64 duration, uint threadIndex)
{
	BOOTPROF_ASSERT(threadIndex < eMAX_THREADS_TO_PROFILE);
	SThreadEntry& entry = m_threadEntry[threadIndex];
	
	// if we are already at the root record that means we are ending sections that began before the current section began
	// TODO we just ignore those for now; could insert them into the record structure instead
	if (entry.m_pCurrentRecord == entry.m_pRootRecord)
		return;

	entry.m_pCurrentRecord->m_endTime = endTime;
	entry.m_pCurrentRecord->m_durationInMs = TicksToMs(duration);
	entry.m_pCurrentRecord = entry.m_pCurrentRecord->m_pParent;
	--entry.m_blocksStarted;
}

struct BootProfilerSessionSerializerToJSON
{
	CBootProfilerSession* pSession = nullptr;
	float funcMinTimeThreshold = 0;

	BootProfilerSessionSerializerToJSON(CBootProfilerSession* pS, float threshold ) : pSession(pS),funcMinTimeThreshold(threshold) {}

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

	enum class EventType { RECORD, THREAD_NAME, THREAD_SORT };

	struct BootProfilerEventSerializerToJSON
	{
		CBootProfilerSession* pSession = nullptr;
		CBootProfilerRecord* pRecord = nullptr;
		threadID threadId = 0;
		EventType type = EventType::RECORD;

		BootProfilerEventSerializerToJSON() {}
		BootProfilerEventSerializerToJSON(CBootProfilerSession* pS, CBootProfilerRecord* pR, threadID tid, EventType et)
			: pSession(pS), pRecord(pR), threadId(tid), type(et) {}

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
				ar(string("thread_name"), "name");
				ar(string("M"), "ph");

				stack_string threadName = GetISystem()->GetIThreadManager()->GetThreadName(threadId);
				ar(SSerializeFixedStringArg{threadName, "name"}, "args");
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

			{
				float timeStartInMus = pSession->TicksToMs(pRecord->m_startTime) * 1000;
				float durationInMus = pRecord->m_durationInMs * 1000;
				const bool isMarker = (pRecord->m_durationInMs == -1);

				string label = pRecord->m_label;
				label.replace("\"", "&quot;");
				label.replace("'", "&apos;");
				ar(label, "name");

				// Events Documentation: https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview#heading=h.lenwiilchoxp
				if (isMarker)
				{
					static string eventType = "R"; // Mark Event
					static string category = "MARKER";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStartInMus, "ts");
				}
				else
				{
					static string eventType = "X"; // Complete Event
					static string category = "PERF";
					ar(category, "cat");
					ar(eventType, "ph");
					ar(timeStartInMus, "ts");
					ar(durationInMus, "dur");
				}

				if (pRecord->m_args != nullptr)
				{
					string args = pRecord->m_args;
					args.replace("\"", "&quot;");
					args.replace("'", "&apos;");
					ar(SSerializeFixedStringArg{args, "arg"}, "args");
				}
			}
		}
	};

	void CollectProfilers(CBootProfilerRecord* pRecord, threadID threadId, std::vector<BootProfilerEventSerializerToJSON>& profilers)
	{
		if (funcMinTimeThreshold <= 0 || pRecord->m_durationInMs >= funcMinTimeThreshold // long enough block
			|| pRecord->m_durationInMs == -1) // marker
		{
			profilers.push_back(BootProfilerEventSerializerToJSON(pSession, pRecord, threadId, EventType::RECORD));
		}

		for (CBootProfilerRecord* pNewRecord = pRecord->m_pFirstChild; pNewRecord; pNewRecord = pNewRecord->m_pNextSibling)
		{
			CollectProfilers(pNewRecord,threadId,profilers);
		}
	}

	void Serialize(Serialization::IArchive& ar)
	{
		std::vector<BootProfilerEventSerializerToJSON> threadProfilers;

		const size_t numThreads = gThreadsInterface.GetThreadCount();
		for (size_t i = 0; i < numThreads; ++i)
		{
			const CBootProfilerSession::SThreadEntry& entry = pSession->GetThreadEntries()[i];

			CBootProfilerRecord* pRoot = entry.m_pRootRecord;
			if (pRoot)
			{
				pRoot->m_durationInMs = pSession->GetDurationInMs();

				const char* threadName = gThreadsInterface.GetThreadNameByIndex(i);
				if (!threadName)
					threadName = "UNKNOWN";
				
				threadID tid = gThreadsInterface.GetThreadIdByIndex(i);

				threadProfilers.push_back(BootProfilerEventSerializerToJSON(pSession, nullptr, tid, EventType::THREAD_NAME));
				threadProfilers.push_back(BootProfilerEventSerializerToJSON(pSession, nullptr, tid, EventType::THREAD_SORT));
				for (CBootProfilerRecord* pRecord = pRoot->m_pFirstChild; pRecord; pRecord = pRecord->m_pNextSibling)
				{
					CollectProfilers(pRecord,tid,threadProfilers);
				}
			}
		}

		ar(threadProfilers,"traceEvents");
		ar(string("ms"),"displayTimeUnit");
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

class BootProfilerSessionSerializerToXML
{
public:

	BootProfilerSessionSerializerToXML(CBootProfilerSession* pSession, float funcMinTimeThreshold) 
		: m_pSession(pSession), m_funcMinTimeThreshold(funcMinTimeThreshold) {}

	void SaveToFile(FILE* pFile)
	{
		fprintf(pFile, "%s", "<root>\n");

		const size_t numThreads = gThreadsInterface.GetThreadCount();
		for (size_t i = 0; i < numThreads; ++i)
		{
			const CBootProfilerSession::SThreadEntry& entry = m_pSession->GetThreadEntries()[i];
			CBootProfilerRecord* pRoot = entry.m_pRootRecord;
			if (pRoot)
			{
				const char* threadName = gThreadsInterface.GetThreadNameByIndex(i);
				if (!threadName)
					threadName = "UNKNOWN";

				fprintf(pFile, "\t<thread name=\"%s\" totalTimeMS=\"%f\" startTime=\"%" PRIu64 "\" stopTime=\"%" PRIu64 "\" totalBlocks=\"%u\"> \n"
					, threadName, pRoot->m_durationInMs, pRoot->m_startTime, pRoot->m_endTime, entry.m_totalBlocks);

				m_indentation = "\t";
				for (CBootProfilerRecord* pRecord = pRoot->m_pFirstChild; pRecord; pRecord = pRecord->m_pNextSibling)
				{
					Print(pFile, pRecord);
				}

				fprintf(pFile, "%s", "\t</thread>\n");
			}
		}

		fprintf(pFile, "%s", "</root>\n");
	}

private:
	void Print(FILE* pFile, CBootProfilerRecord* pRecord)
	{
		if ((m_funcMinTimeThreshold > 0.0f && pRecord->m_durationInMs < m_funcMinTimeThreshold) // block too short
			|| pRecord->m_durationInMs == -1) // marker (not supported by ProfVis)
		{
			return;
		}

		m_indentation.push_back('\t');
		{
			stack_string label = pRecord->m_label;
			XmlEscape(label);

			stack_string args;
			if (pRecord->m_args)
			{
				args = pRecord->m_args;
				XmlEscape(args);
			}

			fprintf(pFile, "%s<block name=\"%s\" totalTimeMS=\"%f\" startTime=\"%" PRIi64 "\" stopTime=\"%" PRIi64 "\" args=\"%s\"> \n",
				m_indentation.c_str(), label.c_str(), pRecord->m_durationInMs, pRecord->m_startTime, pRecord->m_endTime, args.c_str());
		}

		for (CBootProfilerRecord* pChild = pRecord->m_pFirstChild; pChild; pChild = pChild->m_pNextSibling)
		{
			Print(pFile, pChild);
		}

		fprintf(pFile, "%s</block>\n", m_indentation.c_str());
		if (!m_indentation.empty())
			m_indentation.resize(m_indentation.length() - 1);
	}

	void XmlEscape(stack_string& str)
	{
		str.replace("&", "&amp;");
		str.replace("<", "&lt;");
		str.replace(">", "&gt;");
		str.replace("\"", "&quot;");
		str.replace("'", "&apos;");
	}

	CBootProfilerSession* m_pSession;
	const float m_funcMinTimeThreshold;
	stack_string m_indentation;
};

static void SaveProfileSessionToXML(CBootProfilerSession* pSession, const float funcMinTimeThreshold)
{
	static const char* szTestResults = "%USER%/TestResults";
	stack_string filePath;
	filePath.Format("%s\\bp_%s.xml", szTestResults, pSession->GetName());
	CryPathString path;
	gEnv->pCryPak->AdjustFileName(filePath.c_str(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
	gEnv->pCryPak->MakeDir(szTestResults);

	//TODO: use accessible path when running from package on durango
	FILE* pFile = ::fopen(path, "wb");
	if (!pFile)
	{
		return;
	}

	BootProfilerSessionSerializerToXML serializer(pSession, funcMinTimeThreshold);
	serializer.SaveToFile(pFile);
	::fclose(pFile);
}

void CBootProfiler::SaveProfileSessionToDisk(const float funcMinTimeThreshold, CBootProfilerSession* pSession)
{
	if (!(gEnv && gEnv->pCryPak))
	{
		delete pSession;
		return;
	}

	EBootProfilerFormat outputFormat = CBootProfiler::CV_sys_bp_output_formats;
	if (outputFormat == EBootProfilerFormat::XML)
	{
		SaveProfileSessionToXML(pSession, funcMinTimeThreshold);
	}
	else if (outputFormat == EBootProfilerFormat::ChromeTraceJSON)
	{
		SaveProfileSessionToChromeTraceJson(funcMinTimeThreshold, pSession);
	}

	delete pSession;
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
	, m_numFramesToLog(0)
	, m_levelLoadAdditionalFrames(0)
	, m_countdownToNextSaveSesssion(0)
{}

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
}

// stop session
void CBootProfiler::StopSession()
{
	if (m_pCurrentSession)
	{
		CBootProfilerSession* session = m_pCurrentSession;
		m_pCurrentSession = nullptr;

		session->Stop();
		QueueSessionToSave(session);
	}
}

void CBootProfiler::StartFrameProfilingCmd(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		const int argNumFramesToLog = atoi(pArgs->GetArg(1));
		if (argNumFramesToLog > 0)
		{
			CBootProfiler::GetInstance().m_numFramesToLog = argNumFramesToLog;
		}
	}
}

void CBootProfiler::StopSaveSessionsThread()
{
	if (!m_quitSaveThread)
	{
		m_quitSaveThread = true;
		m_saveThreadWakeUpEvent.Set();

		if (gEnv && gEnv->pThreadManager)
		{
			gEnv->pThreadManager->JoinThread(this, eJM_Join);
		}
	}
}

void CBootProfiler::QueueSessionToSave(CBootProfilerSession* pSession)
{
	if (CBootProfiler::CV_sys_bp_frames_worker_thread)
	{
		m_sessionsToSave.push_back(SSaveSessionInfo(CV_sys_bp_time_threshold, pSession));
		m_saveThreadWakeUpEvent.Set();
	}
	else
	{
		SaveProfileSessionToDisk(CV_sys_bp_time_threshold, pSession);
	}
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

void CBootProfiler::Init(ISystem* pSystem, const char* cmdLine)
{
	pSystem->GetISystemEventDispatcher()->RegisterListener(this,"CBootProfiler");
	CryStackStringT<char, 2048> lwrCmdLine = cmdLine;
	lwrCmdLine.MakeLower();
	const char* bpCmd = nullptr;
	if ((bpCmd = strstr(lwrCmdLine.c_str(), "-bootprofiler")) != 0)
	{
		CV_sys_bp_enabled = 1;
	}
	StartSession("boot");

	m_initialized = true;
}

void CBootProfiler::RegisterCVars()
{
	if (!m_initialized)
		return;

	REGISTER_CVAR2("sys_bp_enabled", &CV_sys_bp_enabled, 0, VF_DEV_ONLY, "If this is set to false, new boot profiler sessions will not be started.");
	REGISTER_CVAR2("sys_bp_level_load", &CV_sys_bp_level_load, 0, VF_DEV_ONLY, "If this is set to true, a boot profiler session will be started to profile the level loading. Ignored if sys_bp_enabled is false.");
	REGISTER_CVAR2("sys_bp_level_load_include_first_frame", &CV_sys_bp_level_load_include_first_frame, 0, VF_DEV_ONLY, "If this is set to true, the level profiler will include the first frame - in order to catch calls to Misc:Start from Flowgraph and other late initialization events that result in stalls.");
	REGISTER_CVAR2("sys_bp_frames_worker_thread", &CV_sys_bp_frames_worker_thread, 0, VF_DEV_ONLY | VF_REQUIRE_APP_RESTART, "If this is set to true. The system will dump the profiled session from a different thread.");
	REGISTER_COMMAND("sys_bp_frames", &StartFrameProfilingCmd, VF_DEV_ONLY, "Starts frame profiling for specified number of frames using BootProfiler");
	REGISTER_CVAR2("sys_bp_frames_sample_period", &CV_sys_bp_frames_sample_period, 0, VF_DEV_ONLY, "When in threshold mode, the period (in frames) at which we are going to dump a frame.");
	REGISTER_CVAR2("sys_bp_frames_sample_period_rnd", &CV_sys_bp_frames_sample_period_rnd, 0, VF_DEV_ONLY, "When in threshold mode, the random offset (in frames) at which we are going to dump a next frame.");
	REGISTER_CVAR2("sys_bp_frames_threshold", &CV_sys_bp_frames_threshold, 0, VF_DEV_ONLY, "Starts frame profiling but gathers the results for frames whose frame time exceeded the threshold. [milliseconds]");
	REGISTER_CVAR2("sys_bp_time_threshold", &CV_sys_bp_time_threshold, 0.1f, VF_DEV_ONLY, "If greater than 0 don't write blocks that took less time (default 0.1 ms)");
	REGISTER_CVAR2("sys_bp_format", &CV_sys_bp_output_formats, EBootProfilerFormat::XML, VF_DEV_ONLY, "Determines the output format for the boot profiler.\n0 = XML\n1 = Chrome Trace JSON");
	CV_sys_bp_frames_required_label = REGISTER_STRING("sys_bp_frames_required_label", "", VF_DEV_ONLY, "Specify a record label that must be present in a record session in order to be saved on disk");

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
	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
		{
			if (m_pCurrentSession)
			{
				if(CV_sys_bp_level_load_include_first_frame)
				{
					// Abort the profiler at the end of the next frame
					// This will result in StopSession being called from CBootProfiler::StopFrame(), at the very end of the system frame
					m_levelLoadAdditionalFrames = 2;
				}
				else
				{
					StopSession();
				}
			}
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

void CBootProfiler::OnSectionStart(const SProfilingSection& section)
{
	if (m_pCurrentSession)
	{
		uint threadIdx = gThreadsInterface.GetThreadIndexByID(CryGetCurrentThreadId());
		m_pCurrentSession->StartBlock(section.pDescription->szEventname, section.szDynamicName, section.startValue, threadIdx);
		
		if (CV_sys_bp_frames_required_label)
		{
			const char* szReqLabel = CV_sys_bp_frames_required_label->GetString();
			if (szReqLabel[0] && strcmp(szReqLabel, section.pDescription->szEventname) == 0)
			{
				m_pCurrentSession->SetRequiredLabel(true);
			}
		}
	}
}

void CBootProfiler::OnSectionEnd(const SProfilingSection& section, const SProfilingSectionEnd& sectionEnd)
{
	if (m_pCurrentSession)
	{
		uint threadIdx = gThreadsInterface.GetThreadIndexByID(CryGetCurrentThreadId());
		// we may end sections that were started before the current (boot)profiling session began
		// for these we will fall back to the root record
		BOOTPROF_ASSERT(m_pCurrentSession->GetThreadEntries()[threadIdx].m_pCurrentRecord == m_pCurrentSession->GetThreadEntries()[threadIdx].m_pRootRecord
			|| m_pCurrentSession->GetThreadEntries()[threadIdx].m_pCurrentRecord->m_label == section.pTracker->pDescription->szEventname);

		m_pCurrentSession->StopBlock(section.startValue + sectionEnd.totalValue, sectionEnd.totalValue, threadIdx);
	}
}

void CBootProfiler::OnMarker(int64 timeStamp, const SProfilingMarker& marker)
{
	if (m_pCurrentSession)
	{
		m_pCurrentSession->StartBlock(marker.pDescription->szMarkername, nullptr, timeStamp, gThreadsInterface.GetThreadIndexByID(marker.threadId));
		// markers are indicated by a negative duration
		m_pCurrentSession->StopBlock(0, -1, gThreadsInterface.GetThreadIndexByID(marker.threadId));
	}
}

void CBootProfiler::OnFrameStart()
{
	if (m_numFramesToLog == 0 && CV_sys_bp_frames_threshold == 0.0f)
	{
		return;
	}

	if (!m_pCurrentSession)
	{
		StartSession("frames");
	}
}

void CBootProfiler::OnFrameEnd()
{
	if (!m_pCurrentSession)
		return;

	if (m_levelLoadAdditionalFrames)
	{
		if (gEnv->IsEditor() || gEnv->pGameFramework->IsGameStarted())
		{
			if (--m_levelLoadAdditionalFrames == 0)
			{
				StopSession();
			}
		}

		// Do not monitor current frames while we are still profiling level load
		return;
	}

	if (m_numFramesToLog > 0)
	{
		// m_numFramesToLog can be set mid-frame. The session won't be started until the beginning of the next frame.
		if (m_pCurrentSession)
		{
			--m_numFramesToLog;
		}

		if (m_numFramesToLog == 0)
		{
			StopSession();
		}
	}

	static float prev_CV_sys_bp_frames_threshold = CV_sys_bp_frames_threshold;
	const bool bDisablingThresholdMode = (prev_CV_sys_bp_frames_threshold > 0.0f && CV_sys_bp_frames_threshold == 0.0f);

	// there is a sessions that is not profiling frames and threshold profiling is/was active
	if ((m_pCurrentSession && m_numFramesToLog <= 0) && (CV_sys_bp_frames_threshold > 0.0f || bDisablingThresholdMode))
	{
		m_pCurrentSession->Stop();

		--m_countdownToNextSaveSesssion;
		const bool bShouldCollectResults = m_countdownToNextSaveSesssion < 0;
		
		const char* szReqLabel = CV_sys_bp_frames_required_label->GetString();
		const bool hasRequiredLabel = !szReqLabel[0] || m_pCurrentSession->HasRequiredLabel();

		if (bShouldCollectResults)
		{
			const int nextOffset = cry_random(0, CV_sys_bp_frames_sample_period_rnd);
			m_countdownToNextSaveSesssion = crymath::clamp(CV_sys_bp_frames_sample_period + nextOffset, 0, std::numeric_limits<int>::max());
		}

		if ((m_pCurrentSession->GetDurationInMs() >= CV_sys_bp_frames_threshold) && !bDisablingThresholdMode && bShouldCollectResults && hasRequiredLabel)
		{
			CBootProfilerSession* pSession = m_pCurrentSession;
			m_pCurrentSession = nullptr;

			static int frameNumber = 0;
			const int frameNum = gEnv->pRenderer ? gEnv->pRenderer->GetFrameID(false) : ++frameNumber;

			CryFixedStringT<32> sessionName;
			sessionName.Format("frame_%d_%3.1fms", frameNum, CV_sys_bp_frames_threshold);
			pSession->SetName(sessionName.c_str());
			QueueSessionToSave(pSession);
		}
		else
		{
			SAFE_DELETE(m_pCurrentSession);
		}
	}

	prev_CV_sys_bp_frames_threshold = CV_sys_bp_frames_threshold;
}

#endif
