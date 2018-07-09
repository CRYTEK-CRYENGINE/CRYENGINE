// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Statoscope.h
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Statoscope_h__
#define __Statoscope_h__
#pragma once

#include <CrySystem/Profilers/IStatoscope.h>
#include <CryThreading/IThreadManager.h>

#if ENABLE_STATOSCOPE

const uint32 STATOSCOPE_BINARY_VERSION = 2;

	#include <CryString/CryName.h>
	#include <CryNetwork/CrySocks.h>

struct SPerfStatFrameProfilerRecord
{
	CFrameProfiler* m_pProfiler;
	int             m_count;
	float           m_selfTime;
	float           m_variance;
	float           m_peak;
};

struct SPerfTexturePoolAllocationRecord
{
	int    m_size;
	string m_path;
};

struct SParticleInfo
{
	string name;
	int    numParticles;
};

struct SPhysInfo
{
	string name;
	float  time;
	int    nCalls;
	Vec3   pos;
};

struct SUserMarker
{
	SUserMarker(const string path, const string name) : m_path(path), m_name(name) {}
	string m_path;
	string m_name;
};

struct SCallstack
{
	string             m_tag;
	std::vector<void*> m_addresses;

	SCallstack() {}

	SCallstack(void** addresses, uint32 numAddresses, const char* tag)
		: m_addresses(addresses, addresses + numAddresses), m_tag(tag)
	{}

	void swap(SCallstack& other)
	{
		m_tag.swap(other.m_tag);
		m_addresses.swap(other.m_addresses);
	}
};

class CDataWriter;

class CStatoscopeFrameRecordWriter : public IStatoscopeFrameRecord
{
public:
	explicit CStatoscopeFrameRecordWriter(CDataWriter* pDataWriter)
		: m_pDataWriter(pDataWriter)
		, m_nWrittenElements(0)
	{
	}

	virtual void AddValue(float f);
	virtual void AddValue(const char* s);
	virtual void AddValue(int i);

	inline void  ResetWrittenElementCount()
	{
		m_nWrittenElements = 0;
	}

	inline int GetWrittenElementCount() const
	{
		return m_nWrittenElements;
	}

private:
	CDataWriter* m_pDataWriter;
	int          m_nWrittenElements;
};

namespace StatoscopeDataWriter
{
//The following enums must match in the statoscope tool

enum EFrameElementType
{
	None = 0,
	Float,
	Int,
	String,
	B64Texture,
	Int64
};

enum EEndian
{
	EE_LittleEndian = 0,
	EE_BigEndian
};

enum EEventId
{
	EI_DefineClass = 0,
	EI_BeginInterval,
	EI_EndInterval,
	EI_ModifyInterval,
	EI_ModifyIntervalBit
};

	#pragma pack(push)
	#pragma pack(1)

struct EventHeader
{
	uint8  eventId;
	uint8  sequence;
	uint16 eventLengthInWords;
	uint64 timeStampUs;
};

struct EventDefineClass
{
	enum { EventId = EI_DefineClass };

	uint32 classId;
	uint32 numElements;

	// numElements {uint8 of EFrameElementType, null terminated name}
};

struct EventBeginInterval
{
	enum { EventId = EI_BeginInterval };

	uint64 id;
	uint32 classId;

	// numElements properties
};

struct EventEndInterval
{
	enum { EventId = EI_EndInterval };

	uint64 id;
};

struct EventModifyInterval
{
	enum { EventId = EI_ModifyInterval };

	static const uint32 FieldIdMask = 0x7fffffff;
	static const uint32 FieldSplitIntervalMask = 0x80000000;

	uint64              id;
	uint32              classId;
	uint32              field;

	// numElements properties
};

struct EventModifyIntervalBit
{
	enum { EventId = EI_ModifyIntervalBit };

	static const uint32 FieldIdMask = 0x7fffffff;
	static const uint32 FieldSplitIntervalMask = 0x80000000;

	uint64              id;
	uint32              classId;
	uint32              field;

	// property mask
	// property or
};

	#pragma pack(pop)
}

class CStatoscopeEventWriter;

class CStatoscopeDataClass
{
public:
	struct BinDataElement
	{
		StatoscopeDataWriter::EFrameElementType type;
		string name;
	};

public:
	CStatoscopeDataClass(const char* format);

	size_t                GetNumElements() const          { return m_numDataElements; }
	size_t                GetNumBinElements() const       { return m_binElements.size(); }
	const BinDataElement& GetBinElement(size_t idx) const { return m_binElements[idx]; }

	const char*           GetFormat() const               { return m_format; }
	const char*           GetPath() const                 { return m_path.c_str(); }

private:
	void ProcessNewBinDataElem(const char* pStart, const char* pEnd);

private:
	const char*                 m_format;

	string                      m_path;
	uint32                      m_numDataElements;
	std::vector<BinDataElement> m_binElements;
};

class CStatoscopeDataGroup
{
public:
	CStatoscopeDataGroup(const IStatoscopeDataGroup::SDescription& desc, IStatoscopeDataGroup* pCallback)
		: m_id(desc.key)
		, m_name(desc.name)
		, m_dataClass(desc.format)
		, m_pCallback(pCallback)
	{
	}

	char                  GetId() const       { return m_id; }
	const char*           GetName() const     { return m_name; }
	IStatoscopeDataGroup* GetCallback() const { return m_pCallback; }

	void                  WriteHeader(CDataWriter* pDataWriter);
	size_t                GetNumElements() const { return m_dataClass.GetNumElements(); }

private:
	const char            m_id;
	const char*           m_name;
	CStatoscopeDataClass  m_dataClass;
	IStatoscopeDataGroup* m_pCallback;
};

class CStatoscopeIntervalGroup
{
public:
	CStatoscopeIntervalGroup(uint32 id, const char* name, const char* format);
	virtual ~CStatoscopeIntervalGroup() {}

	uint32      GetId() const          { return m_id; }
	const char* GetName() const        { return m_name; }

	size_t      GetNumElements() const { return m_dataClass.GetNumElements(); }

	void        Enable(CStatoscopeEventWriter* pWriter);
	void        Disable();

	size_t      GetDescEventLength() const;
	void        WriteDescEvent(void* p) const;

protected:
	size_t GetValueLength(const char* pStr) const
	{
		assert(pStr);
		return (strlen(pStr) + 4) & ~3;
	}

	size_t GetValueLength(float f) { return sizeof(f); }
	size_t GetValueLength(int i)   { return sizeof(i); }
	size_t GetValueLength(int64 i) { return sizeof(i); }

	void   Align(char*& p)
	{
		p = reinterpret_cast<char*>((reinterpret_cast<UINT_PTR>(p) + 3) & ~3);
	}

	void WriteValue(char*& p, const char* pStr)
	{
		assert(pStr);
		strcpy(p, pStr);
		p += strlen(pStr) + 1;
		Align(p);
	}

	void WriteValue(char*& p, float f)
	{
		*alias_cast<float*>(p) = f;
		p += sizeof(float);
	}

	void WriteValue(char*& p, int64 i)
	{
		*alias_cast<int64*>(p) = i;
		p += sizeof(int64);
	}

	void WriteValue(char*& p, int i)
	{
		*alias_cast<int*>(p) = i;
		p += sizeof(int);
	}

	CStatoscopeEventWriter* GetWriter() const { return m_pWriter; }

private:
	virtual void Enable_Impl() = 0;
	virtual void Disable_Impl() = 0;

private:
	uint32                  m_id;
	const char*             m_name;
	CStatoscopeDataClass    m_dataClass;
	size_t                  m_instLength;

	CStatoscopeEventWriter* m_pWriter;
};

class CStatoscope;

class CStatoscopeServer
{
public:
	CStatoscopeServer(CStatoscope* pStatoscope);

	bool  IsConnected() { return m_isConnected; }
	void  StartListening();
	void  CheckForConnection();
	void  CloseConnection();
	void  SetBlockingState(bool block);

	int32 ReceiveData(void* buffer, int bufferSize);    // returns num bytes received or -1 if an error occurred

	//called from data thread
	void SendData(const char* buffer, int bufferSize);

protected:

	bool CheckError(int err, const char* tag);          // returns true if err is a CrySock error and also handles the error (prints it and closes the connection)
	void HandleError(const char* tag);

	CRYSOCKET    m_socket;
	bool         m_isConnected;
	CStatoscope* m_pStatoscope;
};

class CStatoscopeIOThread : public IThread
{
public:
	CStatoscopeIOThread();
	virtual ~CStatoscopeIOThread();

	void     QueueSendData(const char* pBuffer, int nBytes);

	threadID GetThreadID() const                     { return m_threadID; }

	void     SetDataWriter(CDataWriter* pDataWriter) { m_pDataWriter = pDataWriter; }

	void     Flush();

	void     Clear()
	{
		CryMT::queue<SendJob>::AutoLock lock(m_sendJobs.get_lock());
		m_sendJobs.clear();
		m_numBytesInQueue = 0;
	}

	uint32 GetReadBounds(const char*& pStart, const char*& pEnd)
	{
		CryMT::queue<SendJob>::AutoLock lock(m_sendJobs.get_lock());
		if (m_sendJobs.size() > 0)
		{
			const SendJob* currentJob = &m_sendJobs.front();
			pStart = currentJob->pBuffer;

			currentJob = &m_sendJobs.back();
			pEnd = currentJob->pBuffer + currentJob->nBytes;
		}
		else
		{
			pStart = NULL;
			pEnd = NULL;
		}
		return m_numBytesInQueue;
	}

protected:
	// Start accepting work on thread
	virtual void ThreadEntry();

	struct SendJob
	{
		const char* pBuffer;
		int         nBytes;
	};

	CryMT::queue<SendJob> m_sendJobs;
	uint32                m_numBytesInQueue;
	threadID              m_threadID;
	CDataWriter*          m_pDataWriter;
	volatile bool         m_bRun;
};

//Base data writer class
class CDataWriter
{
public:

	CDataWriter(bool bUseStringPool, float writeTimeout);
	virtual ~CDataWriter() {}

	virtual bool Open() = 0;
	virtual void Close();
	virtual void Flush();
	virtual void SendData(const char* pBuffer, int nBytes) = 0;

	void         ResetForNewLog();

	//write to internal buffer, common to all IO Writers
	void WriteData(const void* pData, int Size);

	template<typename T>
	void WriteData(T data)
	{
		WriteData((void*)&data, sizeof(T));
	}

	void WriteDataStr(const char* pStr)
	{
		if (m_bUseStringPool)
		{
			uint32 crc = CCrc32::Compute(pStr);
			WriteData(crc);

			if (m_GlobalStringPoolHashes.find(crc) == m_GlobalStringPoolHashes.end())
			{
				m_GlobalStringPoolHashes.insert(crc);
				WriteDataStr_Raw(pStr);
			}
		}
		else
		{
			WriteDataStr_Raw(pStr);
		}
	}

	void Pad4();

	void FlushIOThread();

	bool IsUsingStringPool()
	{
		return m_bUseStringPool;
	}

	void TimeOut()
	{
		m_bTimedOut = true;
	}

	bool HasTimedOut()
	{
		return m_bTimedOut;
	}

	float GetWriteTimeout()
	{
		return m_writeTimeout;
	}

	bool m_bShouldOutputLogTopHeader;
	bool m_bHaveOutputModuleInformation;

protected:

	void WriteDataStr_Raw(const char* pStr)
	{
		int size = strlen(pStr);
		WriteData(size);
		WriteData(pStr, size);
	}

	enum
	{
		FlushLength = 4096,
	};

protected:
	CStatoscopeIOThread                              m_DataThread;

	std::vector<char, stl::STLGlobalAllocator<char>> m_buffer;  //circular buffer for data
	std::vector<char, stl::STLGlobalAllocator<char>> m_formatBuffer;
	char*            m_pWritePtr;
	char*            m_pFlushStartPtr;

	std::set<uint32> m_GlobalStringPoolHashes;

	float            m_writeTimeout;
	bool             m_bUseStringPool;
	volatile bool    m_bTimedOut;
};

class CFileDataWriter : public CDataWriter
{
public:
	CFileDataWriter(const string& fileName, float writeTimeout);
	~CFileDataWriter();

	virtual bool Open();
	virtual void Close();
	virtual void Flush();

	virtual void SendData(const char* pBuffer, int nBytes);

private:
	// don't want m_pFile being pinched
	CFileDataWriter(const CFileDataWriter&);
	CFileDataWriter& operator=(const CFileDataWriter&);

protected:
	string m_fileName;
	FILE*  m_pFile;
	bool   m_bAppend;
};

class CSocketDataWriter : public CDataWriter
{
public:
	CSocketDataWriter(CStatoscopeServer* pStatoscopeServer, float writeTimeout);
	~CSocketDataWriter() { Close(); }

	virtual bool Open();
	virtual void SendData(const char* pBuffer, int nBytes);

protected:
	CStatoscopeServer* m_pStatoscopeServer;
};

class CStatoscopeEventWriter
{
public:
	CStatoscopeEventWriter();

	void BeginBlock()
	{
		m_eventStreamLock.Lock();
		CTimeValue tv = gEnv->pTimer->GetAsyncTime();
		uint64 timeStamp = (uint64)tv.GetMicroSecondsAsInt64();
		if (timeStamp < m_lastTimestampUs)
			__debugbreak();
		m_lastTimestampUs = timeStamp;
	}

	void EndBlock()
	{
		m_eventStreamLock.Unlock();
	}

	template<typename T>
	T* BeginEvent(size_t additional = 0)
	{
		BeginBlock();
		return BeginBlockEvent<T>(additional);
	}

	void EndEvent()
	{
		EndBlockEvent();
		EndBlock();
	}

	template<typename T>
	T* BeginBlockEvent(size_t additional = 0)
	{
		using namespace StatoscopeDataWriter;

		uint8 id = (uint8)T::EventId;
		size_t req = ((sizeof(EventHeader) + sizeof(T) + additional) + 3) & ~3;
		size_t len = m_eventStream.size();
		m_eventStream.resize(len + req);
		EventHeader* pHdr = (EventHeader*)&m_eventStream[len];
		pHdr->eventId = id;
		pHdr->sequence = m_eventNextSequence++;
		pHdr->eventLengthInWords = static_cast<uint16>(req / 4);
		pHdr->timeStampUs = m_lastTimestampUs;
		T* pEv = (T*)&m_eventStream[len + sizeof(EventHeader)];
		return pEv;
	}

	void EndBlockEvent()
	{
	}

	void Flush(CDataWriter* pWriter);
	void Reset();

private:
	CryCriticalSectionNonRecursive                   m_eventStreamLock;
	std::vector<char, stl::STLGlobalAllocator<char>> m_eventStream;
	std::vector<char, stl::STLGlobalAllocator<char>> m_eventStreamTmp;
	uint8  m_eventNextSequence;
	uint64 m_lastTimestampUs;
};

class CTelemetryDataWriter : public CDataWriter
{
public:
	CTelemetryDataWriter(const char* postHeader, const char* hostname, int port, float writeTimeout, float connectTimeout);
	~CTelemetryDataWriter() { Close(); }

	virtual bool Open();
	virtual void Close();
	virtual void SendData(const char* pBuffer, int nBytes);

protected:
	void CheckSocketError(CrySock::eCrySockError sockErr, const char* description);

private:
	void SendToSocket(const char* pData, size_t nSize, const char* sDescription);

	CryStringLocal m_postHeader;
	CryStringLocal m_hostname;
	float          m_connectTimeout;
	int            m_port;
	CRYSOCKET      m_socket;
	bool           m_hasSentHeader;
	bool           m_socketErrorTriggered;
};

struct SParticleProfilersDG;
struct SPhysEntityProfilersDG;
struct SFrameProfilersDG;
struct STexturePoolBucketsDG;
struct SUserMarkerDG;
struct SCallstacksDG;

// Statoscope implementation, access IStatoscope through gEnv->pStatoscope
class CStatoscope : public IStatoscope, public ISystemEventListener, ICaptureFrameListener
{
public:
	CStatoscope();
	~CStatoscope();

	virtual bool        RegisterDataGroup(IStatoscopeDataGroup* pDG);
	virtual void        UnregisterDataGroup(IStatoscopeDataGroup* pDG);

	virtual void        Tick();
	virtual void        AddUserMarker(const char* path, const char* name);
	virtual void        AddUserMarkerFmt(const char* path, const char* fmt, ...);
	virtual void        LogCallstack(const char* tag);
	virtual void        LogCallstackFormat(const char* tagFormat, ...);
	virtual void        SetCurrentProfilerRecords(const std::vector<CFrameProfiler*>* profilers);
	virtual void        Flush();
	virtual bool        IsLoggingForTelemetry();
	virtual bool        RequiresParticleStats(bool& bEffectStats);
	virtual void        AddParticleEffect(const char* pEffectName, int count);
	virtual void        AddPhysEntity(const phys_profile_info* pInfo);
	virtual const char* GetLogFileName() { return m_logFilename.c_str(); }
	virtual void        CreateTelemetryStream(const char* postHeader, const char* hostname, int port);
	virtual void        CloseTelemetryStream();

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);

	//protected:
	enum EScreenShotCaptureState
	{
		eSSCS_Idle = 0,
		eSSCS_RequestCapture,
		eSSCS_AwaitingBufferRequest,
		eSSCS_AwaitingCapture,
		eSSCS_DataReceived
	};

	enum { SCREENSHOT_SCALING_FACTOR = 8 };

	enum ELogDestination
	{
		// these values must match the help string for e_StatoscopeLogDestination
		eLD_File      = 0,
		eLD_Socket    = 1,
		eLD_Telemetry = 2
	};

	void AddFrameRecord(bool bOutputHeader);
	void SetLogFilename();
	void SetDataGroups(uint64 enableDGs, uint64 disableDGs);
	void OutputLoadedModuleInformation(CDataWriter* pDataWriter);
	void StoreCallstack(const char* tag, void** callstack, uint32 callstackLength);

	template<typename T, typename A>
	static const char* GetDataGroupsCVarHelpString(const std::vector<T*, A>& dgs)
	{
		const char* helpStr = "Which data groups are recorded each frame: flags+ enables, flags- disables\n"
		                      " 0 = none\n"
		                      " 1 = all\n";

		uint32 sizeNeeded = strlen(helpStr) + 1;  // +1 for the null terminator

		for (uint32 i = 0; i < dgs.size(); i++)
			sizeNeeded += 7 + strlen(dgs[i]->GetName());

		static string helpString;
		helpString.reserve(sizeNeeded);
		helpString.append(helpStr);

		string tmp;
		for (uint32 i = 0; i < dgs.size(); i++)
		{
			const T* dg = dgs[i];
			tmp.Format(" %c+ = %s\n", dg->GetId(), dg->GetName());
			helpString += tmp;
		}

		return helpString.c_str();
	}

	static void ConsoleAddUserMarker(IConsoleCmdArgs* pParams);
	static void OnLogDestinationCVarChange(ICVar* pVar);
	static void OnTagCVarChange(ICVar* pVar);

	//Screenshot capturing
	virtual bool OnNeedFrameData(unsigned char*& pConvertedTextureBuf);
	virtual void OnFrameCaptured(void);
	virtual int  OnGetFrameWidth(void);
	virtual int  OnGetFrameHeight(void);
	virtual int  OnCaptureFrameBegin(int* pTexHandle);

	//Setup statoscope for fps captures
	virtual void SetupFPSCaptureCVars();

	//Request statoscope screen shot, may not succeed if screem shot is already in progress
	virtual bool RequestScreenShot();

	void         PrepareScreenShot();
	uint8*       ProcessScreenShot();

	std::vector<CStatoscopeDataGroup*>             m_activeDataGroups;
	std::vector<CStatoscopeDataGroup*>             m_allDataGroups;

	ICVar*                                         m_pStatoscopeEnabledCVar;
	ICVar*                                         m_pStatoscopeDumpAllCVar;
	ICVar*                                         m_pStatoscopeDataGroupsCVar;
	ICVar*                                         m_pStatoscopeIvDataGroupsCVar;
	ICVar*                                         m_pStatoscopeLogDestinationCVar;
	ICVar*                                         m_pStatoscopeScreenshotCapturePeriodCVar;
	ICVar*                                         m_pStatoscopeFilenameUseBuildInfoCVar;
	ICVar*                                         m_pStatoscopeFilenameUseMapCVar;
	ICVar*                                         m_pStatoscopeFilenameUseTagCvar;
	ICVar*                                         m_pStatoscopeFilenameUseTimeCVar;
	ICVar*                                         m_pStatoscopeFilenameUseDatagroupsCVar;
	ICVar*                                         m_pStatoscopeMinFuncLengthMsCVar;
	ICVar*                                         m_pStatoscopeMaxNumFuncsPerFrameCVar;
	ICVar*                                         m_pStatoscopeCreateLogFilePerLevelCVar;
	ICVar*                                         m_pStatoscopeWriteTimeout;
	ICVar*                                         m_pStatoscopeConnectTimeout;
	ICVar*                                         m_pGameRulesCVar;
	ICVar*                                         m_pStatoscopeAllowFPSOverrideCVar;

	string                                         m_currentMap;

	float                                          m_lastDumpTime;
	float                                          m_screenshotLastCaptureTime;
	int                                            m_lastScreenWidth;
	int                                            m_lastScreenHeight;
	bool                                           m_groupMaskInitialized;
	bool                                           m_bLevelStarted;
	uint64                                         m_activeDataGroupMask;
	uint64                                         m_activeIvDataGroupMask;
	uint32                                         m_logNum;

	std::vector<std::pair<CFrameProfiler*, int64>> m_perfStatDumpProfilers;   // only used locally in SetCurrentProfilerRecords() but kept to avoid reallocation

	string                                         m_logFilename;

	uint8*                                         m_pScreenShotBuffer; //Buffer for the render thread to asynchronously push screenshots to
	volatile EScreenShotCaptureState               m_ScreenShotState;

	CStatoscopeServer*                             m_pServer;

	CDataWriter*                                   m_pDataWriter;

private:
	typedef std::vector<CStatoscopeIntervalGroup*, stl::STLGlobalAllocator<CStatoscopeIntervalGroup*>> IntervalGroupVec;

private:
	CStatoscope(const CStatoscope&);
	CStatoscope& operator=(const CStatoscope&);

private:
	void RegisterBuiltInDataGroups();
	void RegisterBuiltInIvDataGroups();

	void CreateDataWriter();
	void WriteIntervalClassEvents();

private:
	IntervalGroupVec       m_intervalGroups;
	CStatoscopeEventWriter m_eventWriter;

	// Built in data groups
	SParticleProfilersDG*   m_pParticleProfilers;
	SPhysEntityProfilersDG* m_pPhysEntityProfilers;
	SFrameProfilersDG*      m_pFrameProfilers;
	SUserMarkerDG*          m_pUserMarkers;
	SCallstacksDG*          m_pCallstacks;
};

#endif // ENABLE_STATOSCOPE

#endif  // __Statoscope_h__
