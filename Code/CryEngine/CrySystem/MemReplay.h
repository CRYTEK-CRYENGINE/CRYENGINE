// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MemReplay_h__
#define __MemReplay_h__

#include <CrySystem/Profilers/CryMemReplay.h>

#define REPLAY_RECORD_FREECS        1
#define REPLAY_RECORD_USAGE_CHANGES 0
#define REPLAY_RECORD_THREADED      1
#define REPLAY_RECORD_CONTAINERS    0

#if CAPTURE_REPLAY_LOG || ENABLE_STATOSCOPE

	#include <CryNetwork/CrySocks.h>
	#include <CryThreading/IThreadManager.h>

	#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		#define REPLAY_SOCKET_WRITER
	#endif

class IReplayWriter
{
public:
	virtual ~IReplayWriter() {}

	virtual bool        Open() = 0;
	virtual void        Close() = 0;

	virtual const char* GetFilename() const = 0;
	virtual uint64      GetWrittenLength() const = 0;

	virtual int         Write(const void* data, size_t sz, size_t n) = 0;
	virtual void        Flush() = 0;
};

class ReplayDiskWriter : public IReplayWriter
{
public:
	explicit ReplayDiskWriter(const char* pSuffix);

	bool        Open();
	void        Close();

	const char* GetFilename() const      { return m_filename; }
	uint64      GetWrittenLength() const { return m_written; }

	int         Write(const void* data, size_t sz, size_t n);
	void        Flush();

private:
	char   m_filename[MAX_PATH];

	FILE*  m_fp;
	uint64 m_written;
};

class ReplaySocketWriter : public IReplayWriter
{
public:
	explicit ReplaySocketWriter(const char* address);

	bool        Open();
	void        Close();

	const char* GetFilename() const      { return "socket"; }
	uint64      GetWrittenLength() const { return m_written; }

	int         Write(const void* data, size_t sz, size_t n);
	void        Flush();

private:
	char      m_address[128];
	uint16    m_port;

	CRYSOCKET m_sock;
	uint64    m_written;
};

#endif

namespace MemReplayEventIds
{
	enum Ids
	{
		RE_Alloc,
		RE_Free,
		RE_Callstack,
		RE_FrameStart,
		RE_Label,
		RE_ModuleRef,
		RE_AllocVerbose,
		RE_FreeVerbose,
		RE_Info,
		RE_PushContext,
		RE_PopContext,
		RE_Alloc3,
		RE_Free3,
		RE_PushContext2,
		RE_ModuleShortRef,
		RE_AddressProfile,
		RE_PushContext3,
		RE_Free4,
		RE_AllocUsage,
		RE_Info2,
		RE_Screenshot,
		RE_SizerPush,
		RE_SizerPop,
		RE_SizerAddRange,
		RE_AddressProfile2,
		RE_Alloc64,
		RE_Free64,
		RE_BucketMark,
		RE_BucketMark2,
		RE_BucketUnMark,
		RE_BucketUnMark2,
		RE_PoolMark,
		RE_PoolUnMark,
		RE_TextureAllocContext,
		RE_PageFault,
		RE_AddAllocReference,
		RE_RemoveAllocReference,
		RE_PoolMark2,
		RE_TextureAllocContext2,
		RE_BucketCleanupEnabled,
		RE_Info3,
		RE_Alloc4,
		RE_Realloc,
		RE_RegisterContainer,
		RE_UnregisterContainer,
		RE_BindToContainer,
		RE_UnbindFromContainer,
		RE_RegisterFixedAddressRange,
		RE_SwapContainers,
		RE_MapPage,
		RE_UnMapPage,
		RE_ModuleUnRef,
		RE_Alloc5,
		RE_Free5,
		RE_Realloc2,
		RE_AllocUsage2,
		RE_Alloc6,
		RE_Free6,
		RE_Realloc3,
		RE_UnregisterAddressRange,
		RE_MapPage2,
		RE_DefineContextType,
		RE_Assertion,
		RE_AddFixedContext,
		RE_PushFixedContext,
	};
}

#if CAPTURE_REPLAY_LOG

	#include <CrySystem/File/ICryPak.h>
	#include <CryMemory/HeapAllocator.h>
	#include  <zlib.h>

namespace MemReplayPlatformIds
{
enum Ids
{
	PI_Unknown = 0,
	// PI_360,
	// PI_PS3,
	PI_PC = 3,
	PI_Durango,
	PI_Orbis
};
};

#pragma pack(push)
#pragma pack(1)
struct MemReplayLogHeader
{
	MemReplayLogHeader(uint32 tag, uint32 platform, uint32 pointerWidth)
		: tag(tag)
		, platform(platform)
		, pointerWidth(pointerWidth)
	{
	}

	uint32 tag;
	uint32 platform;
	uint32 pointerWidth;
} __PACKED;

struct MemReplayEventHeader
{
	MemReplayEventHeader(int id, size_t size, uint8 sequenceCheck)
		: sequenceCheck(sequenceCheck)
		, eventId(static_cast<uint8>(id))
		, eventLength(static_cast<uint16>(size))
	{
	}

	uint8  sequenceCheck;
	uint8  eventId;
	uint16 eventLength;
} __PACKED;
#pragma pack(pop)

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE

class ReplayAllocatorBase
{
public:
	enum
	{
		PageSize = 4096
	};

public:
	void* ReserveAddressSpace(size_t sz);
	void  UnreserveAddressSpace(void* base, void* committedEnd);

	void* MapAddressSpace(void* addr, size_t sz);
};

	#elif CRY_PLATFORM_ORBIS

class ReplayAllocatorBase
{
public:
	enum
	{
		PageSize = 65536
	};

public:
	void* ReserveAddressSpace(size_t sz);
	void  UnreserveAddressSpace(void* base, void* committedEnd);

	void* MapAddressSpace(void* addr, size_t sz);
};

	#endif

class ReplayAllocator : private ReplayAllocatorBase
{
public:
	ReplayAllocator();
	~ReplayAllocator();

	void   Reserve(size_t sz);

	void*  Allocate(size_t sz);
	void   Free();

	size_t GetCommittedSize() const { return static_cast<size_t>(reinterpret_cast<INT_PTR>(m_commitEnd) - reinterpret_cast<INT_PTR>(m_heap)); }

private:
	ReplayAllocator(const ReplayAllocator&);
	ReplayAllocator& operator=(const ReplayAllocator&);

private:
	enum
	{
		MaxSize = 6 * 1024 * 1024
	};

private:
	CryCriticalSectionNonRecursive m_lock;

	LPVOID                         m_heap;
	LPVOID                         m_commitEnd;
	LPVOID                         m_allocEnd;
};

class ReplayCompressor
{
public:
	ReplayCompressor(ReplayAllocator& allocator, IReplayWriter* writer);
	~ReplayCompressor();

	void Flush();
	void Write(const uint8* data, size_t len);

private:
	enum
	{
		CompressTargetSize = 64 * 1024
	};

private:
	static voidpf zAlloc(voidpf opaque, uInt items, uInt size);
	static void   zFree(voidpf opaque, voidpf address);

private:
	ReplayCompressor(const ReplayCompressor&);
	ReplayCompressor& operator=(const ReplayCompressor&);

private:
	ReplayAllocator* m_allocator;
	IReplayWriter*   m_writer;

	uint8*           m_compressTarget;
	z_stream         m_compressStream;
	int              m_zSize;
};

	#if REPLAY_RECORD_THREADED

class ReplayRecordThread : public IThread
{
public:
	ReplayRecordThread(ReplayCompressor* compressor);

	void Flush();
	void Write(const uint8* data, size_t len);

public:
	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();

private:
	enum Command
	{
		CMD_Idle = 0,
		CMD_Write,
		CMD_Flush,
		CMD_Stop
	};

private:
	CryMutex              m_mtx;
	CryConditionVariable  m_cond;

	ReplayCompressor*     m_compressor;
	volatile Command      m_nextCommand;
	const uint8* volatile m_nextData;
	long volatile         m_nextLength;
};
	#endif

class ReplayLogStream
{
public:
	ReplayLogStream();
	~ReplayLogStream();

	bool              Open(const char* openString);
	void              Close();

	bool              IsOpen() const { return m_isOpen != 0; }
	const char* const GetFilename()  { assert(m_writer); return m_writer->GetFilename(); }

	bool              EnableAsynchMode();

	template<typename T>
	ILINE T* BeginAllocateRawEvent(size_t evAdditionalReserveLength)
	{
		return reinterpret_cast<T*>(BeginAllocateRaw(sizeof(T) + evAdditionalReserveLength));
	}

	template<typename T>
	ILINE void EndAllocateRawEvent(size_t evAdditionalLength)
	{
		EndAllocateRaw(T::EventId, sizeof(T) + evAdditionalLength);
	}

	template<typename T>
	ILINE T* AllocateRawEvent(size_t evAdditionalLength)
	{
		return reinterpret_cast<T*>(AllocateRaw(T::EventId, sizeof(T) + evAdditionalLength));
	}

	void WriteRawEvent(uint8 id, const void* event, uint eventSize)
	{
		void* ev = AllocateRaw(id, eventSize);
		memcpy(ev, event, eventSize);
	}

	template<typename T>
	ILINE void WriteEvent(const T* ev, size_t evAdditionalLength)
	{
		WriteRawEvent(T::EventId, ev, sizeof(T) + evAdditionalLength);
	}

	template<typename T>
	ILINE void WriteEvent(const T& ev)
	{
		WriteRawEvent(T::EventId, &ev, sizeof(T));
	}

	void   Flush();
	void   FullFlush();

	size_t GetSize() const;
	uint64 GetWrittenLength() const;
	uint64 GetUncompressedLength() const;

private:
	ReplayLogStream(const ReplayLogStream&);
	ReplayLogStream& operator=(const ReplayLogStream&);

private:
	void* BeginAllocateRaw(size_t evReserveLength)
	{
		uint32 size = sizeof(MemReplayEventHeader) + evReserveLength;
		size = (size + 3) & ~3;
		evReserveLength = size - sizeof(MemReplayEventHeader);

		if (((&m_buffer[m_bufferSize]) - m_bufferEnd) < (int)size)
			Flush();

		return reinterpret_cast<MemReplayEventHeader*>(m_bufferEnd) + 1;
	}

	void EndAllocateRaw(uint8 id, size_t evLength)
	{
		uint32 size = sizeof(MemReplayEventHeader) + evLength;
		size = (size + 3) & ~3;
		evLength = size - sizeof(MemReplayEventHeader);

		MemReplayEventHeader* header = reinterpret_cast<MemReplayEventHeader*>(m_bufferEnd);
		new(header) MemReplayEventHeader(id, evLength, m_sequenceCheck);

		++m_sequenceCheck;
		m_bufferEnd += size;
	}

	void* AllocateRaw(uint8 id, uint eventSize)
	{
		void* result = BeginAllocateRaw(eventSize);
		EndAllocateRaw(id, eventSize);
		return result;
	}

private:
	ReplayAllocator   m_allocator;

	volatile int      m_isOpen;

	uint8*            m_buffer;
	size_t            m_bufferSize;
	uint8*            m_bufferEnd;

	uint64            m_uncompressedLen;
	uint8             m_sequenceCheck;

	uint8*            m_bufferA;
	ReplayCompressor* m_compressor;

	#if REPLAY_RECORD_THREADED
	uint8*              m_bufferB;
	ReplayRecordThread* m_recordThread;
	#endif

	IReplayWriter* m_writer;
};

class MemReplayCrySizer : public ICrySizer
{
public:
	MemReplayCrySizer(ReplayLogStream& stream, const char* name);
	~MemReplayCrySizer();

	virtual void                Release() { delete this; }

	virtual size_t              GetTotalSize();
	virtual size_t              GetObjectCount();
	virtual void                Reset();
	virtual void                End() {}

	virtual bool                AddObject(const void* pIdentifier, size_t nSizeBytes, int nCount = 1);

	virtual IResourceCollector* GetResourceCollector();
	virtual void                SetResourceCollector(IResourceCollector*);

	virtual void                Push(const char* szComponentName);
	virtual void                PushSubcomponent(const char* szSubcomponentName);
	virtual void                Pop();

private:
	ReplayLogStream*      m_stream;

	size_t                m_totalSize;
	size_t                m_totalCount;
	std::set<const void*> m_addedObjects;
};

class CReplayModules
{
public:
	struct ModuleLoadDesc
	{
		UINT_PTR address;
		UINT_PTR size;
		char     name[256];
		char     path[256];
		char     sig[512];
	};

	struct ModuleUnloadDesc
	{
		UINT_PTR address;
	};

	typedef void (* FModuleLoad)(void*, const ModuleLoadDesc& desc);
	typedef void (* FModuleUnload)(void*, const ModuleUnloadDesc& desc);

public:
	CReplayModules()
		: m_numKnownModules(0)
	{
	}

	void RefreshModules(FModuleLoad onLoad, FModuleUnload onUnload, void* pUser);

private:
	struct Module
	{
		UINT_PTR baseAddress;
		UINT_PTR size;
		UINT     timestamp;

		Module(){}
		Module(UINT_PTR ba, UINT_PTR s, UINT t)
			: baseAddress(ba), size(s), timestamp(t) {}

		friend bool operator<(Module const& a, Module const& b)
		{
			if (a.baseAddress != b.baseAddress) return a.baseAddress < b.baseAddress;
			if (a.size != b.size) return a.size < b.size;
			return a.timestamp < b.timestamp;
		}
	};

	#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	struct EnumModuleState
	{
		CReplayModules* pSelf;
		HANDLE          hProcess;
		FModuleLoad     onLoad;
		void*           pOnLoadUser;
	};
	#endif

private:
	#if CRY_PLATFORM_DURANGO
	static BOOL EnumModules_Durango(PCSTR moduleName, DWORD64 moduleBase, ULONG moduleSize, PVOID userContext);
	#endif

	#if CRY_PLATFORM_ORBIS
	void RefreshModules_Orbis(FModuleLoad onLoad, void* pUser);
	#endif

	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	static int EnumModules_Linux(struct dl_phdr_info* info, size_t size, void* userContext);
	#endif

private:
	Module m_knownModules[128];
	size_t m_numKnownModules;
};

//////////////////////////////////////////////////////////////////////////
class CMemReplay final : public IMemReplay
{
private:
	enum class ECreationStatus
	{
		Uninitialized = 0,
		Initialized = 1,
	};

	typedef CryCriticalSection TLogMutex;
	typedef CryAutoLock<TLogMutex> TLogAutoLock;

public:
	CMemReplay();
	virtual ~CMemReplay() {};

	static void Init();
	static void Shutdown();

	static CMemReplay* GetInstance();
	static void RegisterCVars();

	//////////////////////////////////////////////////////////////////////////
	// IMemReplay interface implementation
	//////////////////////////////////////////////////////////////////////////
	void DumpStats() final;
	void DumpSymbols() final;

	void StartOnCommandLine(const char* cmdLine) final;
	void Start(bool bPaused, const char* openString) final;
	void Stop() final;
	void Flush() final;
	bool EnableAsynchMode() final;

	void GetInfo(CryReplayInfo& infoOut);

	// Call to begin a new allocation scope.
	bool EnterScope(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId);

	// Records an event against the currently active scope and exits it.
	void ExitScope_Alloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id, UINT_PTR sz, UINT_PTR alignment = 0) final;
	void ExitScope_Realloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR originalId, UINT_PTR newId, UINT_PTR sz, UINT_PTR alignment = 0) final;
	void ExitScope_Free(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id) final;
	void ExitScope() final;

	void AllocUsage(EMemReplayAllocClass allocClass, UINT_PTR id, UINT_PTR used) final;

	void AddAllocReference(void* ptr, void* ref) final;
	void RemoveAllocReference(void* ref) final;

	void AddLabel(const char* label) final;
	void AddLabelFmt(const char* labelFmt, ...) final;
	void AddFrameStart() final;
	void AddScreenshot() final;

	FixedContextID AddFixedContext(EMemStatContextType type, const char* str) final;
	void PushFixedContext(FixedContextID id) final;

	void PushContext(EMemStatContextType type, const char* str) final;
	void PushContextV(EMemStatContextType type, const char* format, va_list args) final;
	void PopContext() final;

	void MapPage(void* base, size_t size) final;
	void UnMapPage(void* base, size_t size) final;

	void RegisterFixedAddressRange(void* base, size_t size, const char* name) final;
	void MarkBucket(int bucket, size_t alignment, void* base, size_t length) final;
	void UnMarkBucket(int bucket, void* base) final;
	void BucketEnableCleanups(void* allocatorBase, bool enabled) final;

	void MarkPool(int pool, size_t alignment, void* base, size_t length, const char* name) final;
	void UnMarkPool(int pool, void* base) final;
	void AddTexturePoolContext(void* ptr, int mip, int width, int height, const char* name, uint32 flags) final;

	void AddSizerTree(const char* name) final;

	void RegisterContainer(const void* key, int type) final;
	void UnregisterContainer(const void* key) final;
	void BindToContainer(const void* key, const void* alloc) final;
	void UnbindFromContainer(const void* key, const void* alloc) final;
	void SwapContainers(const void* keyA, const void* keyB) final;
	//////////////////////////////////////////////////////////////////////////

private:
	static void RecordModuleLoad(void* pSelf, const CReplayModules::ModuleLoadDesc& mld);
	static void RecordModuleUnload(void* pSelf, const CReplayModules::ModuleUnloadDesc& mld);
    uint16 WriteCallstack(UINT_PTR* callstack);

private:
	CMemReplay(const CMemReplay&);
	CMemReplay& operator=(const CMemReplay&);

private:
	void RecordAlloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR p, UINT_PTR alignment, UINT_PTR sizeRequested, UINT_PTR sizeConsumed, INT_PTR sizeGlobal);
	void RecordRealloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR op, UINT_PTR p, UINT_PTR alignment, UINT_PTR sizeRequested, UINT_PTR sizeConsumed, INT_PTR sizeGlobal);
	void RecordFree(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR p, INT_PTR sizeGlobal);
	void RecordModules();

	int  GetCurrentExecutableSize();

private:
	int* GetScopeDepthPtr(threadID tid);

#if MEMREPLAY_USES_DETOURS
	// ntdll
	typedef __success(return >= 0) LONG NTSTATUS;

	static PVOID NTAPI RtlAllocateHeap_Detour(PVOID HeapHandle, ULONG Flags, SIZE_T Size);
	static PVOID NTAPI RtlReAllocateHeap_Detour(PVOID HeapHandle, ULONG Flags, PVOID MemoryPointer, ULONG Size);
	static BOOLEAN NTAPI RtlFreeHeap_Detour(PVOID HeapHandle, ULONG Flags, PVOID BaseAddress);
	static NTSTATUS NTAPI NtAllocateVirtualMemory_Detour(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
	static NTSTATUS NTAPI NtFreeVirtualMemory_Detour(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType);

	typedef decltype(&RtlAllocateHeap_Detour) RtlAllocateHeap_Ptr;
	typedef decltype(&RtlReAllocateHeap_Detour) RtlReAllocateHeap_Ptr;
	typedef decltype(&RtlFreeHeap_Detour) RtlFreeHeap_Ptr;
	typedef decltype(&NtAllocateVirtualMemory_Detour) NtAllocateVirtualMemory_Ptr;
	typedef decltype(&NtFreeVirtualMemory_Detour) NtFreeVirtualMemory_Ptr;

	static RtlAllocateHeap_Ptr         s_originalRtlAllocateHeap;
	static RtlReAllocateHeap_Ptr       s_originalRtlReAllocateHeap;
	static RtlFreeHeap_Ptr             s_originalRtlFreeHeap;
	static NtAllocateVirtualMemory_Ptr s_originalNtAllocateVirtualMemory;
	static NtFreeVirtualMemory_Ptr     s_originalNtFreeVirtualMemory;
#endif

	volatile uint32             m_allocReference = 0;
	ReplayLogStream             m_stream;
	CReplayModules              m_modules;

	int                         m_scopeDepth;
	
	CryReadModifyLock           m_threadScopesLock;
	int                         m_threadScopePairsInUse;
	std::pair<threadID, int>    m_threadScopeDepthPairs[128];

	ECreationStatus             m_status = ECreationStatus::Uninitialized;
};
//////////////////////////////////////////////////////////////////////////

#endif

#endif
