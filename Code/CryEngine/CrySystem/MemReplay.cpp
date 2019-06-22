// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MemReplay.h"
#include "MemoryManager.h"
#include "System.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/ConsoleRegistration.h>
#include <CryCore/Platform/platform.h>
#include <CryCore/BitFiddling.h>
#include <CryCore/AlignmentTools.h>
#include <CryNetwork/CrySocks.h>

#include <stdio.h>
#include <algorithm>  // std::min()
#if CAPTURE_REPLAY_LOG && MEMREPLAY_USES_DETOURS
#	include <detours.h>
#endif

// FIXME DbgHelp broken on Durango currently
#if CRY_PLATFORM_WINDOWS //|| CRY_PLATFORM_DURANGO
	#include "DebugCallStack.h"
	#include <Psapi.h>
	#include "DbgHelp.h"

namespace
{
struct CVDebugInfo
{
	DWORD cvSig;
	GUID  pdbSig;
	DWORD age;
	char  pdbName[256];
};
}
#endif

#if CRY_PLATFORM_DURANGO
	#include <processthreadsapi.h>
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <sys/mman.h>
	#if !defined(_GNU_SOURCE)
		#define _GNU_SOURCE
	#endif
	#include <link.h>
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <sys/mman.h>
#endif

#if CRY_PLATFORM_ORBIS
	#include <net.h>
	#include <CryNetwork/CrySocks.h>
#endif

#if CAPTURE_REPLAY_LOG || ENABLE_STATOSCOPE

static int s_memReplayRecordCallstacks = 1;
static int s_memReplaySaveToProjectDirectory = 1;

#pragma pack(push)
#pragma pack(1)

struct MemReplayFrameStartEvent
{
	static const int EventId = MemReplayEventIds::RE_FrameStart;

	uint32           frameId;

	MemReplayFrameStartEvent(uint32 frameId)
		: frameId(frameId)
	{}
} __PACKED;

struct MemReplayLabelEvent
{
	static const int EventId = MemReplayEventIds::RE_Label;

	char             label[1];

	MemReplayLabelEvent(const char* label)
	{
		// Assume there is room beyond this instance.
		strcpy(this->label, label); // we're intentionally writing beyond the end of this array, so don't use cry_strcpy()
	}
} __PACKED;

struct MemReplayAddFixedContextEvent
{
	static const int EventId = MemReplayEventIds::RE_AddFixedContext;

	uint64 threadId;
	uint32 contextType;
	uint32 fixedContextId;
	char   name[1];

	MemReplayAddFixedContextEvent(uint64 threadId, const char* name, EMemStatContextType type, IMemReplay::FixedContextID ctxId)
	{
		this->threadId = threadId;
		this->contextType = static_cast<uint32>(type);
		this->fixedContextId = ctxId;
		// We're going to assume that there actually is enough space to store the name directly in the struct.
		strcpy(this->name, name); // we're intentionally writing beyond the end of this array, so don't use cry_strcpy()
	}
} __PACKED;

struct MemReplayPushFixedContextEvent
{
	static const int EventId = MemReplayEventIds::RE_PushFixedContext;

	uint64 threadId;
	IMemReplay::FixedContextID contextId;

	MemReplayPushFixedContextEvent(uint64 threadId, IMemReplay::FixedContextID contextId)
	{
		this->threadId = threadId;
		this->contextId = contextId;
	}
} __PACKED;

struct MemReplayPushContextEvent
{
	static const int EventId = MemReplayEventIds::RE_PushContext3;

	uint64           threadId;
	uint32           contextType;

	// This field must be the last in the structure, and enough memory should be allocated
	// for the structure to hold the required name.
	char name[1];

	MemReplayPushContextEvent(uint64 threadId, const char* name, EMemStatContextType type)
	{
		// We're going to assume that there actually is enough space to store the name directly in the struct.

		this->threadId = threadId;
		this->contextType = static_cast<uint32>(type);
		strcpy(this->name, name); // we're intentionally writing beyond the end of this array, so don't use cry_strcpy()
	}
} __PACKED;

struct MemReplayPopContextEvent
{
	static const int EventId = MemReplayEventIds::RE_PopContext;

	uint64 threadId;

	explicit MemReplayPopContextEvent(uint64 threadId)
	{
		this->threadId = threadId;
	}
} __PACKED;

struct MemReplayModuleRefEvent
{
	static const int EventId = MemReplayEventIds::RE_ModuleRef;

	char             name[256];
	char             path[256];
	char             sig[512];
	UINT_PTR         address;
	UINT_PTR         size;

	MemReplayModuleRefEvent(const char* name, const char* path, const UINT_PTR address, UINT_PTR size, const char* sig)
	{
		cry_strcpy(this->name, name);
		cry_strcpy(this->path, path);
		cry_strcpy(this->sig, sig);
		this->address = address;
		this->size = size;
	}
} __PACKED;

struct MemReplayModuleUnRefEvent
{
	static const int EventId = MemReplayEventIds::RE_ModuleUnRef;

	UINT_PTR         address;

	MemReplayModuleUnRefEvent(UINT_PTR address)
		: address(address) {}
} __PACKED;

struct MemReplayModuleRefShortEvent
{
	static const int EventId = MemReplayEventIds::RE_ModuleShortRef;

	char             name[256];

	MemReplayModuleRefShortEvent(const char* name)
	{
		cry_strcpy(this->name, name);
	}
} __PACKED;

struct MemReplayAllocEvent
{
	static const int EventId = MemReplayEventIds::RE_Alloc6;

	uint64           threadId;
	UINT_PTR         id;
	uint32           alignment;
	uint32           sizeRequested;
	uint32           sizeConsumed;
	int32            sizeGlobal; //  Inferred from changes in global memory status

	uint16           moduleId;
	uint16           allocClass;
	uint16           allocSubClass;
	uint16           callstackLength;
	UINT_PTR         callstack[1]; // Must be last.

	MemReplayAllocEvent(uint64 threadId, uint16 moduleId, uint16 allocClass, uint16 allocSubClass, UINT_PTR id, uint32 alignment, uint32 sizeReq, uint32 sizeCon, int32 sizeGlobal)
		: threadId(threadId)
		, id(id)
		, alignment(alignment)
		, sizeRequested(sizeReq)
		, sizeConsumed(sizeCon)
		, sizeGlobal(sizeGlobal)
		, moduleId(moduleId)
		, allocClass(allocClass)
		, allocSubClass(allocSubClass)
		, callstackLength(0)
	{
	}
} __PACKED;

struct MemReplayFreeEvent
{
	static const int EventId = MemReplayEventIds::RE_Free6;

	uint64           threadId;
	UINT_PTR         id;
	int32            sizeGlobal; //  Inferred from changes in global memory status

	uint16           moduleId;
	uint16           allocClass;
	uint16           allocSubClass;

	uint16           callstackLength;
	UINT_PTR         callstack[1]; // Must be last.

	MemReplayFreeEvent(uint64 threadId, uint16 moduleId, uint16 allocClass, uint16 allocSubClass, UINT_PTR id, int32 sizeGlobal)
		: threadId(threadId)
		, id(id)
		, sizeGlobal(sizeGlobal)
		, moduleId(moduleId)
		, allocClass(allocClass)
		, allocSubClass(allocSubClass)
		, callstackLength(0)
	{
	}
} __PACKED;

struct MemReplayInfoEvent
{
	static const int EventId = MemReplayEventIds::RE_Info3;

	uint32           executableSize;
	uint32           initialGlobalSize;
	uint32           bucketsFree;

	MemReplayInfoEvent(uint32 executableSize, uint32 initialGlobalSize, uint32 bucketsFree)
		: executableSize(executableSize)
		, initialGlobalSize(initialGlobalSize)
		, bucketsFree(bucketsFree)
	{
	}
} __PACKED;

struct MemReplayAddressProfileEvent
{
	static const int EventId = MemReplayEventIds::RE_AddressProfile2;

	UINT_PTR         rsxStart;
	uint32           rsxLength;

	MemReplayAddressProfileEvent(UINT_PTR rsxStart, uint32 rsxLength)
		: rsxStart(rsxStart)
		, rsxLength(rsxLength)
	{
	}
} __PACKED;

struct MemReplayAllocUsageEvent
{
	static const int EventId = MemReplayEventIds::RE_AllocUsage2;

	uint32           allocClass;
	UINT_PTR         id;
	uint32           used;

	MemReplayAllocUsageEvent(uint16 allocClass, UINT_PTR id, uint32 used)
		: allocClass(allocClass)
		, id(id)
		, used(used)
	{
	}
} __PACKED;

struct MemReplayScreenshotEvent
{
	static const int EventId = MemReplayEventIds::RE_Screenshot;

	uint8            bmp[1];

	MemReplayScreenshotEvent()
	{
	}
} __PACKED;

struct MemReplaySizerPushEvent
{
	static const int EventId = MemReplayEventIds::RE_SizerPush;

	char             name[1];

	MemReplaySizerPushEvent(const char* name)
	{
		strcpy(this->name, name);
	}
} __PACKED;

struct MemReplaySizerPopEvent
{
	static const int EventId = MemReplayEventIds::RE_SizerPop;
} __PACKED;

struct MemReplaySizerAddRangeEvent
{
	static const int EventId = MemReplayEventIds::RE_SizerAddRange;

	UINT_PTR         address;
	uint32           size;
	int32            count;

	MemReplaySizerAddRangeEvent(const UINT_PTR address, uint32 size, int32 count)
		: address(address)
		, size(size)
		, count(count)
	{}

} __PACKED;

struct MemReplayBucketMarkEvent
{
	static const int EventId = MemReplayEventIds::RE_BucketMark2;

	UINT_PTR         address;
	uint32           length;
	int32            index;
	uint32           alignment;

	MemReplayBucketMarkEvent(UINT_PTR address, uint32 length, int32 index, uint32 alignment)
		: address(address)
		, length(length)
		, index(index)
		, alignment(alignment)
	{}

} __PACKED;

struct MemReplayBucketUnMarkEvent
{
	static const int EventId = MemReplayEventIds::RE_BucketUnMark2;

	UINT_PTR         address;
	int32            index;

	MemReplayBucketUnMarkEvent(UINT_PTR address, int32 index)
		: address(address)
		, index(index)
	{}
} __PACKED;

struct MemReplayAddAllocReferenceEvent
{
	static const int EventId = MemReplayEventIds::RE_AddAllocReference;

	UINT_PTR         address;
	UINT_PTR         referenceId;
	uint32           callstackLength;
	UINT_PTR         callstack[1];

	MemReplayAddAllocReferenceEvent(UINT_PTR address, UINT_PTR referenceId)
		: address(address)
		, referenceId(referenceId)
		, callstackLength(0)
	{
	}
} __PACKED;

struct MemReplayRemoveAllocReferenceEvent
{
	static const int EventId = MemReplayEventIds::RE_RemoveAllocReference;

	UINT_PTR         referenceId;

	MemReplayRemoveAllocReferenceEvent(UINT_PTR referenceId)
		: referenceId(referenceId)
	{
	}
} __PACKED;

struct MemReplayPoolMarkEvent
{
	static const int EventId = MemReplayEventIds::RE_PoolMark2;

	UINT_PTR         address;
	uint32           length;
	int32            index;
	uint32           alignment;
	char             name[1];

	MemReplayPoolMarkEvent(UINT_PTR address, uint32 length, int32 index, uint32 alignment, const char* name)
		: address(address)
		, length(length)
		, index(index)
		, alignment(alignment)
	{
		strcpy(this->name, name);
	}

} __PACKED;

struct MemReplayPoolUnMarkEvent
{
	static const int EventId = MemReplayEventIds::RE_PoolUnMark;

	UINT_PTR         address;
	int32            index;

	MemReplayPoolUnMarkEvent(UINT_PTR address, int32 index)
		: address(address)
		, index(index)
	{}
} __PACKED;

struct MemReplayTextureAllocContextEvent
{
	static const int EventId = MemReplayEventIds::RE_TextureAllocContext2;

	UINT_PTR         address;
	uint32           mip;
	uint32           width;
	uint32           height;
	uint32           flags;
	char             name[1];

	MemReplayTextureAllocContextEvent(UINT_PTR ptr, uint32 mip, uint32 width, uint32 height, uint32 flags, const char* name)
		: address(ptr)
		, mip(mip)
		, width(width)
		, height(height)
		, flags(flags)
	{
		strcpy(this->name, name);
	}
} __PACKED;

struct MemReplayBucketCleanupEnabledEvent
{
	static const int EventId = MemReplayEventIds::RE_BucketCleanupEnabled;

	UINT_PTR         allocatorBaseAddress;
	uint32           cleanupsEnabled;

	MemReplayBucketCleanupEnabledEvent(UINT_PTR allocatorBaseAddress, bool cleanupsEnabled)
		: allocatorBaseAddress(allocatorBaseAddress)
		, cleanupsEnabled(cleanupsEnabled ? 1 : 0)
	{
	}
} __PACKED;

struct MemReplayReallocEvent
{
	static const int EventId = MemReplayEventIds::RE_Realloc3;

	uint64           threadId;
	UINT_PTR         oldId;
	UINT_PTR         newId;
	uint32           alignment;
	uint32           newSizeRequested;
	uint32           newSizeConsumed;
	int32            sizeGlobal; //  Inferred from changes in global memory status

	uint16           moduleId;
	uint16           allocClass;
	uint16           allocSubClass;

	uint16           callstackLength;
	UINT_PTR         callstack[1]; // Must be last.

	MemReplayReallocEvent(uint64 threadId, uint16 moduleId, uint16 allocClass, uint16 allocSubClass, UINT_PTR oldId, UINT_PTR newId, uint32 alignment, uint32 newSizeReq, uint32 newSizeCon, int32 sizeGlobal)
		: threadId(threadId)
		, oldId(oldId)
		, newId(newId)
		, alignment(alignment)
		, newSizeRequested(newSizeReq)
		, newSizeConsumed(newSizeCon)
		, sizeGlobal(sizeGlobal)
		, moduleId(moduleId)
		, allocClass(allocClass)
		, allocSubClass(allocSubClass)
		, callstackLength(0)
	{
	}
} __PACKED;

struct MemReplayRegisterContainerEvent
{
	static const int EventId = MemReplayEventIds::RE_RegisterContainer;

	UINT_PTR         key;
	uint32           type;

	uint32           callstackLength;
	UINT_PTR         callstack[1]; // Must be last

	MemReplayRegisterContainerEvent(UINT_PTR key, uint32 type)
		: key(key)
		, type(type)
		, callstackLength(0)
	{
	}

} __PACKED;

struct MemReplayUnregisterContainerEvent
{
	static const int EventId = MemReplayEventIds::RE_UnregisterContainer;

	UINT_PTR         key;

	explicit MemReplayUnregisterContainerEvent(UINT_PTR key)
		: key(key)
	{
	}

} __PACKED;

struct MemReplayBindToContainerEvent
{
	static const int EventId = MemReplayEventIds::RE_BindToContainer;

	UINT_PTR         key;
	UINT_PTR         ptr;

	MemReplayBindToContainerEvent(UINT_PTR key, UINT_PTR ptr)
		: key(key)
		, ptr(ptr)
	{
	}

} __PACKED;

struct MemReplayUnbindFromContainerEvent
{
	static const int EventId = MemReplayEventIds::RE_UnbindFromContainer;

	UINT_PTR         key;
	UINT_PTR         ptr;

	MemReplayUnbindFromContainerEvent(UINT_PTR key, UINT_PTR ptr)
		: key(key)
		, ptr(ptr)
	{
	}

} __PACKED;

struct MemReplayRegisterFixedAddressRangeEvent
{
	static const int EventId = MemReplayEventIds::RE_RegisterFixedAddressRange;

	UINT_PTR         address;
	uint32           length;

	char             name[1];

	MemReplayRegisterFixedAddressRangeEvent(UINT_PTR address, uint32 length, const char* name)
		: address(address)
		, length(length)
	{
		strcpy(this->name, name);
	}
} __PACKED;

struct MemReplaySwapContainersEvent
{
	static const int EventId = MemReplayEventIds::RE_SwapContainers;

	UINT_PTR         keyA;
	UINT_PTR         keyB;

	MemReplaySwapContainersEvent(UINT_PTR keyA, UINT_PTR keyB)
		: keyA(keyA)
		, keyB(keyB)
	{
	}
} __PACKED;

struct MemReplayMapPageEvent
{
	static const int EventId = MemReplayEventIds::RE_MapPage2;

	UINT_PTR         address;
	uint32           length;
	uint32           callstackLength;
	UINT_PTR         callstack[1];

	MemReplayMapPageEvent(UINT_PTR address, uint32 length)
		: address(address)
		, length(length)
		, callstackLength(0)
	{
	}
} __PACKED;

struct MemReplayUnMapPageEvent
{
	static const int EventId = MemReplayEventIds::RE_UnMapPage;

	UINT_PTR         address;
	uint32           length;

	MemReplayUnMapPageEvent(UINT_PTR address, uint32 length)
		: address(address)
		, length(length)
	{
	}
} __PACKED;

#pragma pack(pop)

ReplayDiskWriter::ReplayDiskWriter(const char* pSuffix)
	: m_fp(NULL)
	, m_written(0)
{
	if (pSuffix == NULL || strcmp(pSuffix, "") == 0)
	{
		time_t curTime;
		time(&curTime);
		struct tm* lt = localtime(&curTime);

		strftime(m_filename, CRY_ARRAY_COUNT(m_filename), "memlog-%Y%m%d-%H%M%S.zmrl", lt);
	}
	else
	{
		cry_sprintf(m_filename, "memlog-%s.zmrl", pSuffix);
	}
}

bool ReplayDiskWriter::Open()
{
	#ifdef USE_FILE_HANDLE_CACHE
		#undef fopen
	#endif
	char fn[MAX_PATH] = "";
	#if CRY_PLATFORM_ORBIS
	cry_strcpy(fn, "/data/");
	#endif
	cry_strcat(fn, m_filename);
	m_fp = ::fopen(fn, "wb");
	return m_fp != NULL;

	#if defined(USE_FILE_HANDLE_CACHE)
		#define fopen WrappedFopen
	#endif
}

void ReplayDiskWriter::Close()
{
	#ifdef USE_FILE_HANDLE_CACHE
		#undef fclose
	#endif

	if (m_fp)
	{
		::fclose(m_fp);
		m_fp = NULL;
	}

	if (s_memReplaySaveToProjectDirectory && gEnv->pCryPak)
	{
		const CryPathString saveDirectory = CryPathString(gEnv->pCryPak->GetGameFolder()) + "/MemReplays/";
		gEnv->pCryPak->MakeDir(saveDirectory);
		const CryPathString saveFile = saveDirectory + m_filename;

		if (rename(m_filename, saveFile.c_str()) != 0)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, 
				"Failed to move the MemReplay recording from '%s' to '%s'.", m_filename, saveFile.c_str());
		}
	}

	#if defined(USE_FILE_HANDLE_CACHE)
		#define fclose WrappedFclose
	#endif
}

int ReplayDiskWriter::Write(const void* data, size_t sz, size_t n)
{
	#ifdef USE_FILE_HANDLE_CACHE
		#undef fwrite
	#endif
	int res = ::fwrite(data, sz, n, m_fp);
	#if defined(USE_FILE_HANDLE_CACHE)
		#define fwrite WrappedFWrite
	#endif

	m_written += res * sz;

	return res;
}

void ReplayDiskWriter::Flush()
{
	#ifdef USE_FILE_HANDLE_CACHE
		#undef fflush
	#endif

	::fflush(m_fp);

	#if defined(USE_FILE_HANDLE_CACHE)
		#define fflush WrappedFflush
	#endif
}

ReplaySocketWriter::ReplaySocketWriter(const char* address)
	: m_port(29494)
	, m_sock(CRY_INVALID_SOCKET)
	, m_written(0)
{
	#if defined(REPLAY_SOCKET_WRITER)
	const char* portStart = strchr(address, ':');
	if (portStart)
		m_port = atoi(portStart + 1);
	else
		portStart = address + strlen(address);

	cry_strcpy(m_address, address, (size_t)(portStart - address));
	#else
	ZeroArray(m_address);
	#endif //#if defined(REPLAY_SOCKET_WRITER)
}

bool ReplaySocketWriter::Open()
{
	#if defined(REPLAY_SOCKET_WRITER)
	CRYINADDR_T addr32 = 0;

		#if CRY_PLATFORM_WINAPI
	WSADATA wsaData;
	PREFAST_SUPPRESS_WARNING(6031);       // we don't need wsaData
	WSAStartup(MAKEWORD(2, 2), &wsaData); // ensure CrySock::socket has been initialized
		#endif

	addr32 = CrySock::GetAddrForHostOrIP(m_address, 0);

	if (!addr32)
		return false;

	m_sock = CrySock::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sock == CRY_INVALID_SOCKET)
		return false;

	CRYSOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = addr32;

	int connectErr = CRY_SOCKET_ERROR;
	for (uint16 port = m_port; (connectErr == CRY_SOCKET_ERROR) && port < (29494 + 32); ++port)
	{
		addr.sin_port = htons(port);
		connectErr = CrySock::connect(m_sock, (CRYSOCKADDR*) &addr, sizeof(addr));
	}

	if (connectErr == CRY_SOCKET_ERROR)
	{
		CrySock::shutdown(m_sock, SD_BOTH);
		CrySock::closesocket(m_sock);

		m_sock = CRY_INVALID_SOCKET;
		return false;
	}
	#endif //#if defined(REPLAY_SOCKET_WRITER)
	return true;
}

void ReplaySocketWriter::Close()
{
	#if defined(REPLAY_SOCKET_WRITER)
	if (m_sock != CRY_INVALID_SOCKET)
	{
		CrySock::shutdown(m_sock, SD_BOTH);
		CrySock::closesocket(m_sock);
		m_sock = CRY_INVALID_SOCKET;
	}

		#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	WSACleanup();
		#endif

	#endif //#if defined(REPLAY_SOCKET_WRITER)
}

int ReplaySocketWriter::Write(const void* data, size_t sz, size_t n)
{
	#if defined(REPLAY_SOCKET_WRITER)
	if (m_sock != CRY_INVALID_SOCKET)
	{
		const char* bdata = reinterpret_cast<const char*>(data);
		size_t toSend = sz * n;
		while (toSend > 0)
		{
			int sent = send(m_sock, bdata, toSend, 0);

			if (sent == CRY_SOCKET_ERROR)
			{
				__debugbreak();
				break;
			}

			toSend -= sent;
			bdata += sent;
		}

		m_written += sz * n;
	}

	#endif //#if defined(REPLAY_SOCKET_WRITER)

	return n;
}

void ReplaySocketWriter::Flush()
{
}

#endif

#if CAPTURE_REPLAY_LOG
namespace
{
	typedef CryCriticalSection TLogMutex;
	typedef CryAutoLock<TLogMutex> TLogAutoLock;

	TLogMutex s_logmutex;
	TLogMutex& GetLogMutex()
	{
		return s_logmutex;
	}

	static volatile threadID s_ignoreThreadId = (threadID)-1;
	static uint32 g_memReplayFrameCount = 0;
	const uint k_maxCallStackDepth = 256;

	static volatile UINT_PTR s_replayLastGlobal = 0;
	static volatile int s_replayStartingFree = 0;

	static volatile threadID s_recordThreadId = threadID(-1);

	inline bool ThreadIsNotFlushing()
	{
		return s_recordThreadId != CryGetCurrentThreadId();
	}

	// No write thread: While writing out data we need to avoid recording new entries, as we are under lock
	// With write Thread: Just ignore allocations from the recording thread
	struct RecordingThreadScope
	{
		RecordingThreadScope() { assert(s_recordThreadId == threadID(-1)); s_recordThreadId = CryGetCurrentThreadId(); }
		~RecordingThreadScope() { assert(s_recordThreadId != threadID(-1)); s_recordThreadId = threadID(-1); }
	};

	static bool g_memReplayPaused = false;
}

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO

	void* ReplayAllocatorBase::ReserveAddressSpace(size_t sz)
	{
		return VirtualAlloc(NULL, sz, MEM_RESERVE, PAGE_READWRITE);
	}

	void ReplayAllocatorBase::UnreserveAddressSpace(void* base, void* committedEnd)
	{
		VirtualFree(base, 0, MEM_RELEASE);
	}

	void* ReplayAllocatorBase::MapAddressSpace(void* addr, size_t sz)
	{
		return VirtualAlloc(addr, sz, MEM_COMMIT, PAGE_READWRITE);
	}

#elif CRY_PLATFORM_ORBIS

void* ReplayAllocatorBase::ReserveAddressSpace(size_t sz)
{
	return VirtualAllocator::AllocateVirtualAddressSpace(sz);
}

void ReplayAllocatorBase::UnreserveAddressSpace(void* base, void* committedEnd)
{
	VirtualAllocator::FreeVirtualAddressSpace(base);
}

void* ReplayAllocatorBase::MapAddressSpace(void* addr, size_t sz)
{
	for (size_t i = 0; i < sz; i += PAGE_SIZE)
	{
		if (!VirtualAllocator::MapPage((char*)addr + i))
		{
			for (size_t k = 0; k < i; k += PAGE_SIZE)
			{
				VirtualAllocator::UnmapPage((char*)addr + k);
			}
			return NULL;
		}
	}
	return addr;
}

#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	void* ReplayAllocatorBase::ReserveAddressSpace(size_t sz)
	{
		return mmap(NULL, sz, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	}

	void ReplayAllocatorBase::UnreserveAddressSpace(void* base, void* committedEnd)
	{
		int res = munmap(base, reinterpret_cast<char*>(committedEnd) - reinterpret_cast<char*>(base));
		CRY_VERIFY(res == 0);
	}

	void* ReplayAllocatorBase::MapAddressSpace(void* addr, size_t sz)
	{
		int res = mprotect(addr, sz, PROT_READ | PROT_WRITE);
		CRY_VERIFY(res == 0);
		return addr;
	}
#elif CRY_PLATFORM_ORBIS

	void* ReplayAllocatorBase::ReserveAddressSpace(size_t sz)
	{
		return VirtualAllocator::AllocateVirtualAddressSpace(sz);
	}

	void ReplayAllocatorBase::UnreserveAddressSpace(void* base, void* committedEnd)
	{
		VirtualAllocator::FreeVirtualAddressSpace(base);
	}

	void* ReplayAllocatorBase::MapAddressSpace(void* addr, size_t sz)
	{
		for (size_t i = 0; i < sz; i += PAGE_SIZE)
		{
			if (!VirtualAllocator::MapPage((char*)addr + i))
			{
				for (size_t k = 0; k < i; k += PAGE_SIZE)
				{
					VirtualAllocator::UnmapPage((char*)addr + k);
				}
				return NULL;
			}
		}
		return addr;
	}

#endif

namespace
{
ILINE void** CastCallstack(UINT_PTR* pCallstack)
{
	return alias_cast<void**>(pCallstack);
}

void RetrieveMemReplaySuffix(char* pSuffixBuffer, const char* lwrCommandLine, int bufferLength)
{
	cry_strcpy(pSuffixBuffer, bufferLength, "");
	const char* pSuffix = strstr(lwrCommandLine, "-memreplaysuffix");
	if (pSuffix != NULL)
	{
		pSuffix += strlen("-memreplaysuffix");
		while (1)
		{
			if (*pSuffix == '\0' || *pSuffix == '+' || *pSuffix == '-')
			{
				pSuffix = NULL;
				break;
			}
			if (*pSuffix != ' ')
				break;
			++pSuffix;
		}
		if (pSuffix != NULL)
		{
			const char* pEnd = pSuffix;
			while (*pEnd != '\0' && *pEnd != ' ')
				++pEnd;
			cry_strcpy(pSuffixBuffer, bufferLength, pSuffix, (size_t)(pEnd - pSuffix));
		}
	}
}
}

ReplayAllocator::ReplayAllocator()
{
	m_heap = ReserveAddressSpace(MaxSize);
	if (!m_heap)
		__debugbreak();

	m_commitEnd = m_heap;
	m_allocEnd = m_heap;
}

ReplayAllocator::~ReplayAllocator()
{
	Free();
}

void ReplayAllocator::Reserve(size_t sz)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	INT_PTR commitEnd = reinterpret_cast<INT_PTR>(m_commitEnd);
	INT_PTR newCommitEnd = std::min<INT_PTR>(commitEnd + sz, commitEnd + MaxSize);

	if ((newCommitEnd - commitEnd) > 0)
	{
		LPVOID res = MapAddressSpace(m_commitEnd, newCommitEnd - commitEnd);
		if (!res)
			__debugbreak();
	}

	m_commitEnd = reinterpret_cast<void*>(newCommitEnd);
}

void* ReplayAllocator::Allocate(size_t sz)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	sz = (sz + 7) & ~7;

	uint8* alloc = reinterpret_cast<uint8*>(m_allocEnd);
	uint8* newEnd = reinterpret_cast<uint8*>(m_allocEnd) + sz;

	if (newEnd > reinterpret_cast<uint8*>(m_commitEnd))
	{
		uint8* alignedEnd = reinterpret_cast<uint8*>((reinterpret_cast<INT_PTR>(newEnd) + (PageSize - 1)) & ~(PageSize - 1));

		if (alignedEnd > reinterpret_cast<uint8*>(m_allocEnd) + MaxSize)
			__debugbreak();

		LPVOID res = MapAddressSpace(m_commitEnd, alignedEnd - reinterpret_cast<uint8*>(m_commitEnd));
		if (!res)
			__debugbreak();

		m_commitEnd = alignedEnd;
	}

	m_allocEnd = newEnd;

	return alloc;
}

void ReplayAllocator::Free()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	UnreserveAddressSpace(m_heap, m_commitEnd);
}

ReplayCompressor::ReplayCompressor(ReplayAllocator& allocator, IReplayWriter* writer)
	: m_allocator(&allocator)
	, m_writer(writer)
{
	m_zSize = 0;

	memset(&m_compressStream, 0, sizeof(m_compressStream));
	m_compressStream.zalloc = &ReplayCompressor::zAlloc;
	m_compressStream.zfree = &ReplayCompressor::zFree;
	m_compressStream.opaque = this;
	deflateInit(&m_compressStream, 2);//Z_DEFAULT_COMPRESSION);

	uint64 streamLen = 0;
	m_writer->Write(&streamLen, sizeof(streamLen), 1);

	m_compressTarget = (uint8*) m_allocator->Allocate(CompressTargetSize);
}

ReplayCompressor::~ReplayCompressor()
{
	deflateEnd(&m_compressStream);
	m_compressTarget = NULL;
	m_writer = NULL;
}

void ReplayCompressor::Flush()
{
	m_writer->Flush();
}

void ReplayCompressor::Write(const uint8* data, size_t len)
{
	if (CompressTargetSize < len)
		__debugbreak();

	m_compressStream.next_in = const_cast<uint8*>(data);
	m_compressStream.avail_in = len;
	m_compressStream.next_out = m_compressTarget;
	m_compressStream.avail_out = CompressTargetSize;
	m_compressStream.total_out = 0;

	do
	{
		int err = deflate(&m_compressStream, Z_SYNC_FLUSH);

		if ((err == Z_OK && m_compressStream.avail_out == 0) || (err == Z_BUF_ERROR && m_compressStream.avail_out == 0))
		{
			int total_out = m_compressStream.total_out;
			m_writer->Write(m_compressTarget, total_out * sizeof(uint8), 1);

			m_compressStream.next_out = m_compressTarget;
			m_compressStream.avail_out = CompressTargetSize;
			m_compressStream.total_out = 0;

			continue;
		}
		else if (m_compressStream.avail_in == 0)
		{
			int total_out = m_compressStream.total_out;
			m_writer->Write(m_compressTarget, total_out * sizeof(uint8), 1);
		}
		else
		{
			// Shrug?
		}

		break;
	}
	while (true);

	Flush();
}

voidpf ReplayCompressor::zAlloc(voidpf opaque, uInt items, uInt size)
{
	ReplayCompressor* str = reinterpret_cast<ReplayCompressor*>(opaque);
	return str->m_allocator->Allocate(items * size);
}

void ReplayCompressor::zFree(voidpf opaque, voidpf address)
{
	// Doesn't seem to be called, except when shutting down and then we'll just free it all anyway
}

#if REPLAY_RECORD_THREADED

ReplayRecordThread::ReplayRecordThread(ReplayCompressor* compressor)
	: m_compressor(compressor)
{
	m_nextCommand = CMD_Idle;
	m_nextData = NULL;
	m_nextLength = 0;
}

void ReplayRecordThread::Flush()
{
	m_mtx.Lock();

	while (m_nextCommand != CMD_Idle)
	{
		m_mtx.Unlock();
		CrySleep(1);
		m_mtx.Lock();
	}

	m_nextCommand = CMD_Flush;
	m_mtx.Unlock();
	m_cond.NotifySingle();
}

void ReplayRecordThread::Write(const uint8* data, size_t len)
{
	m_mtx.Lock();

	while (m_nextCommand != CMD_Idle)
	{
		m_mtx.Unlock();
		CrySleep(1);
		m_mtx.Lock();
	}

	m_nextData = data;
	m_nextLength = len;
	m_nextCommand = CMD_Write;
	m_mtx.Unlock();
	m_cond.NotifySingle();
}

void ReplayRecordThread::ThreadEntry()
{
	RecordingThreadScope recordScope;

	while (true)
	{
		m_mtx.Lock();

		while (m_nextCommand == CMD_Idle)
			m_cond.Wait(m_mtx);

		switch (m_nextCommand)
		{
		case CMD_Stop:
			m_mtx.Unlock();
			m_nextCommand = CMD_Idle;
			return;

		case CMD_Write:
			{
				size_t length = m_nextLength;
				const uint8* data = m_nextData;

				m_nextLength = 0;
				m_nextData = NULL;

				m_compressor->Write(data, length);
			}
			break;

		case CMD_Flush:
			m_compressor->Flush();
			break;
		}

		m_nextCommand = CMD_Idle;

		m_mtx.Unlock();
	}
}

void ReplayRecordThread::SignalStopWork()
{
	m_mtx.Lock();

	while (m_nextCommand != CMD_Idle)
	{
		m_mtx.Unlock();
		CrySleep(1);
		m_mtx.Lock();
	}

	m_nextCommand = CMD_Stop;
	m_cond.NotifySingle();

	while (m_nextCommand != CMD_Idle)
	{
		m_mtx.Unlock();
		CrySleep(1);
		m_mtx.Lock();
	}

	m_mtx.Unlock();
}

#endif

ReplayLogStream::ReplayLogStream()
	: m_isOpen(0)
	, m_buffer(nullptr)
	, m_bufferSize(0)
	, m_bufferEnd(nullptr)
	, m_uncompressedLen(0)
	, m_sequenceCheck(0)
	, m_bufferA(nullptr)
	, m_compressor(nullptr)
	#if REPLAY_RECORD_THREADED
	, m_bufferB(nullptr)
	, m_recordThread(nullptr)
	#endif
	, m_writer(nullptr)
{
}

ReplayLogStream::~ReplayLogStream()
{
	if (IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());
		Close();
	}
}

bool ReplayLogStream::Open(const char* openString)
{
	using std::swap;

	m_allocator.Reserve(512 * 1024);

	while (isspace((unsigned char) *openString))
		++openString;

	const char* sep = strchr(openString, ':');
	if (!sep)
		sep = openString + strlen(openString);

	char destination[32];
	const char* arg = "";

	if (sep != openString)
	{
		while (isspace((unsigned char) sep[-1]))
			--sep;

		cry_strcpy(destination, openString, (size_t)(sep - openString));

		arg = sep;

		if (*arg)
		{
			++arg;
			while (isspace((unsigned char) *arg))
				++arg;
		}
	}
	else
	{
		cry_strcpy(destination, "disk");
	}

	IReplayWriter* writer = NULL;

	if (strcmp(destination, "disk") == 0)
	{
		writer = new(m_allocator.Allocate(sizeof(ReplayDiskWriter)))ReplayDiskWriter(arg);
	}
	else if (strcmp(destination, "socket") == 0)
	{
		writer = new(m_allocator.Allocate(sizeof(ReplaySocketWriter)))ReplaySocketWriter(arg);
	}

	if (writer && writer->Open() == false)
	{
		fprintf(stdout, "Failed to open writer\n");
		writer->~IReplayWriter();
		writer = NULL;

		m_allocator.Free();
		return false;
	}

	swap(m_writer, writer);

	m_bufferSize = 64 * 1024;
	m_bufferA = (uint8*) m_allocator.Allocate(m_bufferSize);

	#if REPLAY_RECORD_THREADED
	m_bufferB = (uint8*) m_allocator.Allocate(m_bufferSize);
	#endif

	m_buffer = m_bufferA;
	m_compressor = new(m_allocator.Allocate(sizeof(ReplayCompressor)))ReplayCompressor(m_allocator, m_writer);

	{
		m_bufferEnd = &m_buffer[0];

		// Write stream header.
		const uint8 version = 5;
		uint32 platform = MemReplayPlatformIds::PI_Unknown;

	#if CRY_PLATFORM_ORBIS
		platform = MemReplayPlatformIds::PI_Orbis;
	#elif CRY_PLATFORM_DURANGO
		platform = MemReplayPlatformIds::PI_Durango;
	#elif CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		platform = MemReplayPlatformIds::PI_PC;
	#endif

		MemReplayLogHeader hdr(('M' << 0) | ('R' << 8) | ('L' << 16) | (version << 24), platform, sizeof(void*) * 8);
		memcpy(&m_buffer[0], &hdr, sizeof(hdr));
		m_bufferEnd += sizeof(hdr);

		CryInterlockedIncrement(&m_isOpen);
	}

	return true;
}

void ReplayLogStream::Close()
{
	using std::swap;

	// Flag it as closed here, to prevent recursion into the memory logging as it is closing.
	CryInterlockedDecrement(&m_isOpen);

	Flush();
	Flush();
	Flush();

	#if REPLAY_RECORD_THREADED
	if (m_recordThread)
	{
		m_recordThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(m_recordThread, eJM_Join);
		m_recordThread->~ReplayRecordThread();
		m_recordThread = NULL;
	}
	#endif

	if (m_compressor)
	{
		m_compressor->~ReplayCompressor();
		m_compressor = NULL;
	}

	m_writer->Close();
	m_writer = NULL;

	m_buffer = NULL;
	m_bufferEnd = 0;
	m_bufferSize = 0;

	m_allocator.Free();
}

bool ReplayLogStream::EnableAsynchMode()
{
	#if REPLAY_RECORD_THREADED
	// System to create thread is not running yet
	if (!gEnv || !gEnv->pSystem)
		return false;

	// Asynch mode already enabled
	if (m_recordThread)
		return true;

	// Write out pending data
	Flush();

	// Try create ReplayRecord thread
	m_recordThread = new(m_allocator.Allocate(sizeof(ReplayRecordThread)))ReplayRecordThread(m_compressor);
	if (!gEnv->pThreadManager->SpawnThread(m_recordThread, "ReplayRecord"))
	{
		CRY_ASSERT_MESSAGE(false, "Error spawning \"ReplayRecord\" thread.");
		m_recordThread->~ReplayRecordThread();
		m_recordThread = NULL;
		return false;
	}

	return true;
	#else
	return false;
	#endif
}

void ReplayLogStream::Flush()
{
	if (m_writer != NULL)
	{
		m_uncompressedLen += m_bufferEnd - &m_buffer[0];

#if REPLAY_RECORD_THREADED
		if (m_recordThread)
		{
			m_recordThread->Write(&m_buffer[0], m_bufferEnd - &m_buffer[0]);
			m_buffer = (m_buffer == m_bufferA) ? m_bufferB : m_bufferA;
		}
		else
#endif
		{
			RecordingThreadScope writeScope;
			m_compressor->Write(&m_buffer[0], m_bufferEnd - &m_buffer[0]);
		}
	}

	m_bufferEnd = &m_buffer[0];
}

void ReplayLogStream::FullFlush()
{
#if REPLAY_RECORD_THREADED
	if (m_recordThread)
	{
		m_recordThread->Flush();
	}
	else
#endif
	{
		RecordingThreadScope writeScope;
		m_compressor->Flush();
	}
}

size_t ReplayLogStream::GetSize() const
{
	return m_allocator.GetCommittedSize();
}

uint64 ReplayLogStream::GetWrittenLength() const
{
	return m_writer ? m_writer->GetWrittenLength() : 0ULL;
}

uint64 ReplayLogStream::GetUncompressedLength() const
{
	return m_uncompressedLen;
}

#define SIZEOF_MEMBER(cls, mbr) (sizeof(reinterpret_cast<cls*>(0)->mbr))

//////////////////////////////////////////////////////////////////////////
// CMemReplay class implementation.
//////////////////////////////////////////////////////////////////////////

static int GetCurrentSysAlloced()
{
	int trackingUsage = 0;//m_stream.GetSize();
	IMemoryManager::SProcessMemInfo mi;
	CCryMemoryManager::GetInstance()->GetProcessMemInfo(mi);
	return (uint32)(mi.PagefileUsage - trackingUsage);
}

#if !CRY_PLATFORM_ORBIS
int CMemReplay::GetCurrentExecutableSize()
{
	return GetCurrentSysAlloced();
}
#endif

//////////////////////////////////////////////////////////////////////////
static UINT_PTR GetCurrentSysFree()
{
	IMemoryManager::SProcessMemInfo mi;
	CCryMemoryManager::GetInstance()->GetProcessMemInfo(mi);
	return (UINT_PTR)-(INT_PTR)mi.PagefileUsage;
}

void CReplayModules::RefreshModules(FModuleLoad onLoad, FModuleUnload onUnload, void* pUser)
{
	MEMSTAT_LABEL_SCOPED("RefreshModules");

	#if CRY_PLATFORM_WINDOWS
	HMODULE hMods[1024];
	HANDLE hProcess;
	DWORD cbNeeded;
	unsigned int i;

	// Get a list of all the modules in this process.
	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	if (NULL != hProcess)
	{
		if (SymInitialize(hProcess, NULL, TRUE))
		{
			decltype(&EnumProcessModules) pfEnumProcessModules = nullptr;
			decltype(&GetModuleFileNameExA) pfGetModuleFileNameExA = nullptr;
			decltype(&GetModuleInformation) pfGetModuleInformation = nullptr;

			HMODULE hPsapiModule = ::LoadLibraryA("psapi.dll");
			if (hPsapiModule)
			{
				pfEnumProcessModules = (decltype(pfEnumProcessModules)) GetProcAddress(hPsapiModule, "EnumProcessModules");
				pfGetModuleFileNameExA = (decltype(pfGetModuleFileNameExA)) GetProcAddress(hPsapiModule, "GetModuleFileNameExA");
				pfGetModuleInformation = (decltype(pfGetModuleInformation)) GetProcAddress(hPsapiModule, "GetModuleInformation");
			}

			if(pfEnumProcessModules && pfGetModuleFileNameExA && pfGetModuleInformation)
			{
				if (pfEnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
				{
					for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
					{
						TCHAR szModName[MAX_PATH];
						// Get the full path to the module's file.
						if (pfGetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
						{
							char pdbPath[1024];
							cry_strcpy(pdbPath, szModName);
							char* pPdbPathExt = strrchr(pdbPath, '.');
							if (pPdbPathExt)
								strcpy(pPdbPathExt, ".pdb");
							else
								cry_strcat(pdbPath, ".pdb");

							MODULEINFO modInfo;
							pfGetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo));

							IMAGEHLP_MODULE64 ihModInfo;
							memset(&ihModInfo, 0, sizeof(ihModInfo));
							ihModInfo.SizeOfStruct = sizeof(ihModInfo);
							if (SymGetModuleInfo64(hProcess, (DWORD64)modInfo.lpBaseOfDll, &ihModInfo))
							{
								cry_strcpy(pdbPath, ihModInfo.CVData);

								if (ihModInfo.PdbSig70.Data1 == 0)
								{
									DWORD debugEntrySize;
									IMAGE_DEBUG_DIRECTORY* pDebugDirectory = (IMAGE_DEBUG_DIRECTORY*)ImageDirectoryEntryToDataEx(modInfo.lpBaseOfDll, TRUE, IMAGE_DIRECTORY_ENTRY_DEBUG, &debugEntrySize, NULL);
									if (pDebugDirectory)
									{
										size_t numDirectories = debugEntrySize / sizeof(IMAGE_DEBUG_DIRECTORY);
										for (size_t dirIdx = 0; dirIdx != numDirectories; ++dirIdx)
										{
											IMAGE_DEBUG_DIRECTORY& dir = pDebugDirectory[dirIdx];
											if (dir.Type == 2)
											{
												CVDebugInfo* pCVInfo = (CVDebugInfo*)((char*)modInfo.lpBaseOfDll + dir.AddressOfRawData);
												ihModInfo.PdbSig70 = pCVInfo->pdbSig;
												ihModInfo.PdbAge = pCVInfo->age;
												cry_strcpy(pdbPath, pCVInfo->pdbName);
												break;
											}
										}
									}
								}

								ModuleLoadDesc mld;
								cry_sprintf(mld.sig, "%p, %08X, %08X, {%08X-%04X-%04X-%02X %02X-%02X %02X %02X %02X %02X %02X}, %d\n",
									modInfo.lpBaseOfDll, modInfo.SizeOfImage, ihModInfo.TimeDateStamp,
									ihModInfo.PdbSig70.Data1, ihModInfo.PdbSig70.Data2, ihModInfo.PdbSig70.Data3,
									ihModInfo.PdbSig70.Data4[0], ihModInfo.PdbSig70.Data4[1],
									ihModInfo.PdbSig70.Data4[2], ihModInfo.PdbSig70.Data4[3],
									ihModInfo.PdbSig70.Data4[4], ihModInfo.PdbSig70.Data4[5],
									ihModInfo.PdbSig70.Data4[6], ihModInfo.PdbSig70.Data4[7],
									ihModInfo.PdbAge);

								cry_strcpy(mld.name, szModName);
								cry_strcpy(mld.path, pdbPath);
								mld.address = (UINT_PTR)modInfo.lpBaseOfDll;
								mld.size = modInfo.SizeOfImage;
								onLoad(pUser, mld);
							}
						}
					}
				}
			}

			::FreeLibrary(hPsapiModule);
			SymCleanup(hProcess);
		}
		CloseHandle(hProcess);
	}
	#elif CRY_PLATFORM_DURANGO && 0

	// Get a list of all the modules in this process.
	EnumModuleState ems;
	ems.hProcess = GetCurrentProcess();
	ems.pSelf = this;
	ems.onLoad = onLoad;
	ems.pOnLoadUser = pUser;
	if (NULL != ems.hProcess)
	{
		if (SymInitialize(ems.hProcess, NULL, TRUE))
		{
			EnumerateLoadedModulesEx(ems.hProcess, EnumModules_Durango, &ems);

			SymCleanup(ems.hProcess);
		}
	}
	#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	EnumModuleState ems;
	ems.hProcess = 0;
	ems.pSelf = this;
	ems.onLoad = onLoad;
	ems.pOnLoadUser = pUser;
	dl_iterate_phdr(EnumModules_Linux, &ems);
	#elif CRY_PLATFORM_ORBIS
	RefreshModules_Orbis(onLoad, pUser);
	#endif
}

	#if CRY_PLATFORM_DURANGO && 0

BOOL CReplayModules::EnumModules_Durango(PCSTR moduleName, DWORD64 moduleBase, ULONG moduleSize, PVOID userContext)
{
	EnumModuleState& ems = *static_cast<EnumModuleState*>(userContext);

	char pdbPath[1024];
	cry_strcpy(pdbPath, moduleName);
	char* pPdbPathExt = strrchr(pdbPath, '.');
	if (pPdbPathExt)
		strcpy(pPdbPathExt, ".pdb");
	else
		cry_strcat(pdbPath, ".pdb");

	IMAGEHLP_MODULE64 ihModInfo;
	memset(&ihModInfo, 0, sizeof(ihModInfo));
	ihModInfo.SizeOfStruct = sizeof(ihModInfo);
	if (SymGetModuleInfo64(ems.hProcess, moduleBase, &ihModInfo))
	{
		cry_strcpy(pdbPath, ihModInfo.CVData);

		if (ihModInfo.PdbSig70.Data1 == 0)
		{
			DWORD debugEntrySize;
			IMAGE_DEBUG_DIRECTORY* pDebugDirectory = (IMAGE_DEBUG_DIRECTORY*)ImageDirectoryEntryToDataEx((PVOID)moduleBase, TRUE, IMAGE_DIRECTORY_ENTRY_DEBUG, &debugEntrySize, NULL);
			if (pDebugDirectory)
			{
				size_t numDirectories = debugEntrySize / sizeof(IMAGE_DEBUG_DIRECTORY);
				for (size_t dirIdx = 0; dirIdx != numDirectories; ++dirIdx)
				{
					IMAGE_DEBUG_DIRECTORY& dir = pDebugDirectory[dirIdx];
					if (dir.Type == 2)
					{
						CVDebugInfo* pCVInfo = (CVDebugInfo*)((char*)moduleBase + dir.AddressOfRawData);
						ihModInfo.PdbSig70 = pCVInfo->pdbSig;
						ihModInfo.PdbAge = pCVInfo->age;
						cry_strcpy(pdbPath, pCVInfo->pdbName);
						break;
					}
				}
			}
		}

		ModuleLoadDesc mld;
		cry_sprintf(mld.sig, "%p, %08X, %08X, {%08X-%04X-%04X-%02X %02X-%02X %02X %02X %02X %02X %02X}, %d\n",
					moduleBase, moduleSize, ihModInfo.TimeDateStamp,
					ihModInfo.PdbSig70.Data1, ihModInfo.PdbSig70.Data2, ihModInfo.PdbSig70.Data3,
					ihModInfo.PdbSig70.Data4[0], ihModInfo.PdbSig70.Data4[1],
					ihModInfo.PdbSig70.Data4[2], ihModInfo.PdbSig70.Data4[3],
					ihModInfo.PdbSig70.Data4[4], ihModInfo.PdbSig70.Data4[5],
					ihModInfo.PdbSig70.Data4[6], ihModInfo.PdbSig70.Data4[7],
					ihModInfo.PdbAge);

		cry_strcpy(mld.name, moduleName);
		cry_strcpy(mld.path, pdbPath);
		mld.address = moduleBase;
		mld.size = moduleSize;
		ems.onLoad(ems.pOnLoadUser, mld);
	}

	return TRUE;
}

	#endif

	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
int CReplayModules::EnumModules_Linux(struct dl_phdr_info* info, size_t size, void* userContext)
{
	EnumModuleState& ems = *reinterpret_cast<EnumModuleState*>(userContext);

	ModuleLoadDesc mld;
	mld.address = info->dlpi_addr;
	cry_strcpy(mld.name, info->dlpi_name);
	cry_strcpy(mld.path, info->dlpi_name);

	Module& module = ems.pSelf->m_knownModules[ems.pSelf->m_numKnownModules];
	ems.pSelf->m_numKnownModules++;
	module.baseAddress = info->dlpi_addr;
	module.timestamp = 0;

	uint64 memSize = 0;
	for (int j = 0; j < info->dlpi_phnum; j++)
	{
		memSize += info->dlpi_phdr[j].p_memsz;
	}
	mld.size = (UINT_PTR)memSize;
	module.size = (UINT_PTR)memSize;

	ems.onLoad(ems.pOnLoadUser, mld);

	return 0;
}
	#endif

#if MEMREPLAY_USES_DETOURS
namespace
{
	template <typename TDetouredMethodPtr>
	LONG MemReplayDetourAttach(TDetouredMethodPtr* pOriginalMethod, TDetouredMethodPtr detourMethod)
	{
		if (!pOriginalMethod || !*pOriginalMethod || !detourMethod)
			return ERROR_INVALID_HANDLE;

		LONG error = ::DetourTransactionBegin();
		if (error != NO_ERROR)
			return error;

		error = ::DetourUpdateThread(::GetCurrentThread());
		if (error != NO_ERROR)
		{
			::DetourTransactionCommit();
			return error;
		}

		error = ::DetourAttach(&(PVOID&)*pOriginalMethod, detourMethod);
		if (error != NO_ERROR)
		{
			::DetourTransactionCommit();
			return error;
		}

		error = ::DetourTransactionCommit();
		return error;
	}

	template <typename TDetouredMethodPtr>
	LONG MemReplayDetourDetach(TDetouredMethodPtr* pOriginalMethod, TDetouredMethodPtr detourMethod)
	{
		if (!pOriginalMethod || !*pOriginalMethod || !detourMethod)
			return ERROR_INVALID_HANDLE;

		LONG error = ::DetourTransactionBegin();
		if (error != NO_ERROR)
			return error;

		error = ::DetourUpdateThread(::GetCurrentThread());
		if (error != NO_ERROR)
		{
			::DetourTransactionCommit();
			return error;
		}

		error = ::DetourDetach(&(PVOID&)*pOriginalMethod, detourMethod);
		if (error != NO_ERROR)
		{
			::DetourTransactionCommit();
			return error;
		}

		error = ::DetourTransactionCommit();
		return error;
	}
}
#endif

// we cannot use a static here, due to initialization order requirements
SUninitialized<CMemReplay> g_memReplay;
CMemReplay* CMemReplay::GetInstance()
{
 	if (((CMemReplay&)g_memReplay).m_status == ECreationStatus::Initialized)
		return &(CMemReplay&)g_memReplay;
 	else
 		return nullptr;
}


static void DumpAllocs(IConsoleCmdArgs* pParams)
{
	CryGetIMemReplay()->DumpStats();
}

static void ReplayDumpSymbols(IConsoleCmdArgs* pParams)
{
	CryGetIMemReplay()->DumpSymbols();
}

static void ReplayStop(IConsoleCmdArgs* pParams)
{
	CryGetIMemReplay()->Stop();
}

static void ReplayPause(IConsoleCmdArgs* pParams)
{
	CryGetIMemReplay()->Start(true);
}

static void ReplayResume(IConsoleCmdArgs* pParams)
{
	CryGetIMemReplay()->Start(false);
}

static void MemReplayRecordCallstacksChanged(ICVar* pCvar)
{
	if (s_memReplayRecordCallstacks)
		CryGetIMemReplay()->AddLabel("Start recording Callstacks");
	else
		CryGetIMemReplay()->AddLabel("Stop recording Callstacks");
}

static void AddReplayLabel(IConsoleCmdArgs* pParams)
{
	if (pParams->GetArgCount() < 2)
		CryLog("Not enough arguments");
	else
		CryGetIMemReplay()->AddLabel(pParams->GetArg(1));
}

static void ReplayInfo(IConsoleCmdArgs* pParams)
{
	CryReplayInfo info;
	CryGetIMemReplay()->GetInfo(info);

	CryLog("Uncompressed length: %" PRIu64, info.uncompressedLength);
	CryLog("Written length: %" PRIu64, info.writtenLength);
	CryLog("Tracking overhead: %u", info.trackingSize);
	CryLog("Output filename: %s", info.filename ? info.filename : "(not open)");
}

static void AddReplaySizerTree(IConsoleCmdArgs* pParams)
{
	const char* name = "Sizers";

	if (pParams->GetArgCount() >= 2)
		name = pParams->GetArg(1);

	CryGetIMemReplay()->AddSizerTree(name);
}

void CMemReplay::RegisterCVars()
{
	REGISTER_COMMAND("memDumpAllocs", &DumpAllocs, VF_NULL, "print allocs with stack traces");
	REGISTER_COMMAND("memReplayDumpSymbols", &ReplayDumpSymbols, VF_NULL, "dump symbol info to mem replay log");
	REGISTER_COMMAND("memReplayStop", &ReplayStop, VF_NULL, "stop logging to mem replay");
	REGISTER_COMMAND("memReplayPause", &ReplayPause, VF_NULL, "Pause collection of mem replay data");
	REGISTER_COMMAND("memReplayResume", &ReplayResume, VF_NULL, "Resume collection of mem replay data (use with -memReplayPaused cmdline)");
	REGISTER_CVAR2_CB("memReplayRecordCallstacks", &s_memReplayRecordCallstacks, 1, VF_NULL,
		"Turn the logging of callstacks by memreplay on(1) or off(0).\n"
		"Saves a lot of memory on the log, but it will obviously contain less information. "
		"Can be toggled during recording sessions to only add detail to specific sections of the recording."
		, MemReplayRecordCallstacksChanged);
	REGISTER_CVAR2("memReplaySaveToProjectDirectory", &s_memReplaySaveToProjectDirectory, 1, VF_NULL, 
		"Setting this to 0 will save MemReplay recordings to the engine root directory, rather than the directory of the project.");
	REGISTER_COMMAND("memReplayLabel", &AddReplayLabel, VF_NULL, "record a label in the mem replay log");
	REGISTER_COMMAND("memReplayInfo", &ReplayInfo, VF_NULL, "output some info about the replay log");
	REGISTER_COMMAND("memReplayAddSizerTree", &AddReplaySizerTree, VF_NULL, "output in-game sizer information to the log");
}

void CMemReplay::Init()
{
#if MEMREPLAY_USES_DETOURS
	// Prefer ntdll alloc functions over kernel32 functions (the deeper the better)
	const HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");
	s_originalRtlAllocateHeap = (RtlAllocateHeap_Ptr)::GetProcAddress(ntdll, "RtlAllocateHeap");
	s_originalRtlReAllocateHeap = (RtlReAllocateHeap_Ptr)::GetProcAddress(ntdll, "RtlReAllocateHeap");
	s_originalRtlFreeHeap = (RtlFreeHeap_Ptr)::GetProcAddress(ntdll, "RtlFreeHeap");
	s_originalNtAllocateVirtualMemory = (NtAllocateVirtualMemory_Ptr)::GetProcAddress(ntdll, "NtAllocateVirtualMemory");
	s_originalNtFreeVirtualMemory = (NtFreeVirtualMemory_Ptr)::GetProcAddress(ntdll, "NtFreeVirtualMemory");

	MemReplayDetourAttach(&s_originalRtlAllocateHeap, RtlAllocateHeap_Detour);
	MemReplayDetourAttach(&s_originalRtlReAllocateHeap, RtlReAllocateHeap_Detour); // Not strictly necessary to hook, it seems to call Allocate and Free functions
	MemReplayDetourAttach(&s_originalRtlFreeHeap, RtlFreeHeap_Detour);
	MemReplayDetourAttach(&s_originalNtAllocateVirtualMemory, NtAllocateVirtualMemory_Detour);
	MemReplayDetourAttach(&s_originalNtFreeVirtualMemory, NtFreeVirtualMemory_Detour);

#endif
	g_memReplay.DefaultConstruct();
	((CMemReplay&)g_memReplay).m_status = ECreationStatus::Initialized;
}

void CMemReplay::Shutdown()
{
#if MEMREPLAY_USES_DETOURS
	((CMemReplay&)g_memReplay).m_status = ECreationStatus::Uninitialized;

	MemReplayDetourDetach(&s_originalRtlAllocateHeap, RtlAllocateHeap_Detour);
	MemReplayDetourDetach(&s_originalRtlReAllocateHeap, RtlReAllocateHeap_Detour);
	MemReplayDetourDetach(&s_originalRtlFreeHeap, RtlFreeHeap_Detour);
	MemReplayDetourDetach(&s_originalNtAllocateVirtualMemory, NtAllocateVirtualMemory_Detour);
	MemReplayDetourDetach(&s_originalNtFreeVirtualMemory, NtFreeVirtualMemory_Detour);
#endif
}

CMemReplay::CMemReplay()
	: m_allocReference(0)
	, m_scopeDepth(0)
	, m_threadScopePairsInUse(0)
{}

#if MEMREPLAY_USES_DETOURS

#define MEMREPLAY_SCOPE_INTERNAL(cls, subCls)                 CMemReplayScope _mrCls((cls), (subCls), eCryModule)
#define MEMREPLAY_SCOPE_ALLOC_INTERNAL(id, sz, align)         _mrCls.Alloc((UINT_PTR)(id), (sz), (align))
#define MEMREPLAY_SCOPE_REALLOC_INTERNAL(oid, nid, sz, align) _mrCls.Realloc((UINT_PTR)(oid), (UINT_PTR)(nid), (sz), (align))
#define MEMREPLAY_SCOPE_FREE_INTERNAL(id)                     _mrCls.Free((UINT_PTR)(id))

CMemReplay::RtlAllocateHeap_Ptr         CMemReplay::s_originalRtlAllocateHeap = nullptr;
CMemReplay::RtlReAllocateHeap_Ptr       CMemReplay::s_originalRtlReAllocateHeap = nullptr;
CMemReplay::RtlFreeHeap_Ptr             CMemReplay::s_originalRtlFreeHeap = nullptr;
CMemReplay::NtAllocateVirtualMemory_Ptr CMemReplay::s_originalNtAllocateVirtualMemory = nullptr;
CMemReplay::NtFreeVirtualMemory_Ptr     CMemReplay::s_originalNtFreeVirtualMemory = nullptr;

PVOID NTAPI CMemReplay::RtlAllocateHeap_Detour(PVOID HeapHandle, ULONG Flags, SIZE_T Size)
{
	MEMREPLAY_SCOPE_INTERNAL(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::HeapAlloc);
	LPVOID ptr = s_originalRtlAllocateHeap(HeapHandle, Flags, Size);
	MEMREPLAY_SCOPE_ALLOC_INTERNAL(ptr, Size, 0);
	return ptr;
}

PVOID NTAPI CMemReplay::RtlReAllocateHeap_Detour(PVOID HeapHandle, ULONG Flags, PVOID BaseAddress, ULONG Size)
{
	MEMREPLAY_SCOPE_INTERNAL(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::HeapAlloc);
	LPVOID ptr = s_originalRtlReAllocateHeap(HeapHandle, Flags, BaseAddress, Size);
	MEMREPLAY_SCOPE_REALLOC_INTERNAL(BaseAddress, ptr, Size, 0);
	return ptr;
}

BOOLEAN NTAPI CMemReplay::RtlFreeHeap_Detour(PVOID HeapHandle, ULONG Flags, PVOID BaseAddress)
{
	MEMREPLAY_SCOPE_INTERNAL(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::HeapAlloc);
	BOOL freed = s_originalRtlFreeHeap(HeapHandle, Flags, BaseAddress);
	if (freed)
	{
		MEMREPLAY_SCOPE_FREE_INTERNAL(BaseAddress);
	}
	return freed;
}


size_t GetSizeInAddressRange(void* pBase, size_t length, DWORD commitStatus)
{
	size_t commitedSize = 0;
	void* const pRegionEnd = (char*)pBase + length;
	while (pBase < pRegionEnd)
	{
		MEMORY_BASIC_INFORMATION memInfo;
		if (VirtualQuery(pBase, &memInfo, length) != sizeof(MEMORY_BASIC_INFORMATION))
			break;
		if (memInfo.State == commitStatus)
		{
			commitedSize += memInfo.RegionSize;
		}
		pBase = (char*)memInfo.BaseAddress + memInfo.RegionSize;
	}
	return commitedSize;
}

CMemReplay::NTSTATUS NTAPI CMemReplay::NtAllocateVirtualMemory_Detour(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect)
{
	if (AllocationType & MEM_COMMIT)
	{
		MEMREPLAY_SCOPE_INTERNAL(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::VirtualAlloc);
		size_t alreadyCommitted = BaseAddress != 0 ? GetSizeInAddressRange(BaseAddress, *RegionSize, MEM_COMMIT) : 0;
		NTSTATUS status = s_originalNtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
		if (status == 0L)
			MEMREPLAY_SCOPE_ALLOC_INTERNAL(*BaseAddress, *RegionSize - alreadyCommitted, 0);
		return status;
	}
	else
	{
		return s_originalNtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
	}
}

CMemReplay::NTSTATUS NTAPI CMemReplay::NtFreeVirtualMemory_Detour(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType)
{
	MEMREPLAY_SCOPE_INTERNAL(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::VirtualAlloc);
	if (FreeType & MEM_RELEASE)
	{
		NTSTATUS status = s_originalNtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, FreeType);
		if (status == 0L)
			MEMREPLAY_SCOPE_FREE_INTERNAL(*BaseAddress);
		return status;
	}
	else if (FreeType & MEM_DECOMMIT)
	{
		NTSTATUS status = s_originalNtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, FreeType);
		if (status == 0L)
			MEMREPLAY_SCOPE_FREE_INTERNAL(*BaseAddress);
		return status;
	}
	return s_originalNtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, FreeType);
}

#endif

//////////////////////////////////////////////////////////////////////////
void CMemReplay::DumpStats()
{
	CMemReplay::DumpSymbols();
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::DumpSymbols()
{
	if (m_stream.IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());
		if (m_stream.IsOpen())
		{
			RecordModules();

			m_stream.FullFlush();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::StartOnCommandLine(const char* cmdLine)
{
	const char* mrCmd = 0;

	CryStackStringT<char, 2048> lwrCmdLine = cmdLine;
	lwrCmdLine.MakeLower();

	if ((mrCmd = strstr(lwrCmdLine.c_str(), "-memreplay")) != nullptr)
	{
		bool bPaused = strstr(lwrCmdLine.c_str(), "-memreplaypaused") != nullptr;

		//Retrieve file suffix if present
		const int bufferLength = 64;
		char openCmd[bufferLength] = "disk:";

		// Try and detect a new style open string, and fall back to the old suffix format if it fails.
		if (
		  (strncmp(mrCmd, "-memreplay disk", 15) == 0) ||
		  (strncmp(mrCmd, "-memreplay socket", 17) == 0))
		{
			const char* arg = mrCmd + strlen("-memreplay ");
			const char* argEnd = arg;
			while (*argEnd && !isspace((unsigned char) *argEnd))
				++argEnd;
			cry_strcpy(openCmd, arg, (size_t)(argEnd - arg));
		}
		else
		{
			RetrieveMemReplaySuffix(openCmd + 5, lwrCmdLine.c_str(), bufferLength - 5);
		}

		Start(bPaused, openCmd);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::Start(bool bPaused, const char* openString)
{
	if (!openString)
	{
		openString = "disk";
	}

	TLogAutoLock lock(GetLogMutex());
	if(m_stream.IsOpen())
	{
		const char* label = nullptr;
		if (bPaused && !g_memReplayPaused)
			label = "Pausing MemReplay";
		else if (!bPaused && g_memReplayPaused)
			label = "Resuming MemReplay";

		if(label)
			new(m_stream.AllocateRawEvent<MemReplayLabelEvent>(strlen(label)))MemReplayLabelEvent(label);
	}

	g_memReplayPaused = bPaused;

	if (!m_stream.IsOpen())
	{
		if (m_stream.Open(openString))
		{
			s_replayLastGlobal = GetCurrentSysFree();
			int initialGlobal = GetCurrentSysAlloced();
			int exeSize = GetCurrentExecutableSize();

	#if CRY_PLATFORM_ORBIS
			// Also record used space in the system allocator
			exeSize += CrySystemCrtGetUsedSpace();
	#endif

			m_stream.WriteEvent(MemReplayAddressProfileEvent(0xffffffff, 0));
			m_stream.WriteEvent(MemReplayInfoEvent(exeSize, initialGlobal, s_replayStartingFree));
			m_stream.WriteEvent(MemReplayFrameStartEvent(g_memReplayFrameCount++));

	#if CRY_PLATFORM_DURANGO
			m_stream.WriteEvent(MemReplayModuleRefShortEvent("minidump"));
	#endif

	#if CRY_PLATFORM_ORBIS
			m_stream.WriteEvent(MemReplayModuleRefShortEvent("orbis"));
	#endif
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::Stop()
{
	if (m_stream.IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());
		g_memReplayPaused = false; // unpause or modules might not be recorded

		if (m_stream.IsOpen())
		{
			RecordModules();

			m_stream.Close();
		}
	}
}

int* CMemReplay::GetScopeDepthPtr(threadID tid)
{
	AUTO_READLOCK(m_threadScopesLock);
	for(int i = 0; i < m_threadScopePairsInUse; ++i)
	{
		if (m_threadScopeDepthPairs[i].first == tid)
			return &m_threadScopeDepthPairs[i].second;
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CMemReplay::EnterScope(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId)
{
	
	if (m_stream.IsOpen())
	{
		int* pScopeDepth = GetScopeDepthPtr(CryGetCurrentThreadId());
		if (pScopeDepth == nullptr)
		{
			AUTO_MODIFYLOCK(m_threadScopesLock);
			CRY_ASSERT(m_threadScopePairsInUse < CRY_ARRAY_COUNT(m_threadScopeDepthPairs));
			m_threadScopeDepthPairs[m_threadScopePairsInUse].first = CryGetCurrentThreadId();
			m_threadScopeDepthPairs[m_threadScopePairsInUse++].second = 1;
		}
		else
		{
			++*pScopeDepth;
		}
		return true;
	}

	return false;
}

void CMemReplay::ExitScope_Alloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id, UINT_PTR sz, UINT_PTR alignment)
{
	if (--*GetScopeDepthPtr(CryGetCurrentThreadId()) == 0)
	{
		if (id && m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
		{
			UINT_PTR global = GetCurrentSysFree();
			INT_PTR changeGlobal = (INT_PTR)(s_replayLastGlobal - global);
			s_replayLastGlobal = global;

			TLogAutoLock lock(GetLogMutex());
			if (m_stream.IsOpen())
				RecordAlloc(cls, subCls, moduleId, id, alignment, sz, sz, changeGlobal);
		}
	}
}

void CMemReplay::ExitScope_Realloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR originalId, UINT_PTR newId, UINT_PTR sz, UINT_PTR alignment)
{
	if (!originalId)
	{
		ExitScope_Alloc(cls, subCls, moduleId, newId, sz, alignment);
		return;
	}

	if (!newId)
	{
		ExitScope_Free(cls, subCls, moduleId, originalId);
		return;
	}

	if (--*GetScopeDepthPtr(CryGetCurrentThreadId()) == 0)
	{
		if (m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
		{
			UINT_PTR global = GetCurrentSysFree();
			INT_PTR changeGlobal = (INT_PTR)(s_replayLastGlobal - global);
			s_replayLastGlobal = global;

			TLogAutoLock lock(GetLogMutex());
			RecordRealloc(cls, subCls, moduleId, originalId, newId, alignment, sz, sz, changeGlobal);
		}
	}
}

void CMemReplay::ExitScope_Free(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id)
{
	if (--*GetScopeDepthPtr(CryGetCurrentThreadId()) == 0)
	{
		if (id && m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
		{
			if (m_stream.IsOpen())
			{
				UINT_PTR global = GetCurrentSysFree();
				INT_PTR changeGlobal = (INT_PTR)(s_replayLastGlobal - global);
				s_replayLastGlobal = global;

				TLogAutoLock lock(GetLogMutex());
				PREFAST_SUPPRESS_WARNING(6326)
				RecordFree(cls, subCls, moduleId, id, changeGlobal);
			}
		}
	}
}

void CMemReplay::ExitScope()
{
	--*GetScopeDepthPtr(CryGetCurrentThreadId());
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::AddLabel(const char* label)
{
	if (m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			new(m_stream.AllocateRawEvent<MemReplayLabelEvent>(strlen(label)))MemReplayLabelEvent(label);
		}
	}
}

void CMemReplay::AddLabelFmt(const char* labelFmt, ...)
{
	if (m_stream.IsOpen())
	{
		va_list args;
		va_start(args, labelFmt);

		char msg[256];
		cry_vsprintf(msg, labelFmt, args);

		va_end(args);

		AddLabel(msg);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::AddFrameStart()
{
	if (m_stream.IsOpen())
	{
		static bool bSymbolsDumped = false;
		if (!bSymbolsDumped)
		{
			DumpSymbols();
			bSymbolsDumped = true;
		}

		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			m_stream.WriteEvent(MemReplayFrameStartEvent(g_memReplayFrameCount++));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::GetInfo(CryReplayInfo& infoOut)
{
	memset(&infoOut, 0, sizeof(CryReplayInfo));

	if (m_stream.IsOpen())
	{
		infoOut.uncompressedLength = m_stream.GetUncompressedLength();
		infoOut.writtenLength = m_stream.GetWrittenLength();
		infoOut.trackingSize = m_stream.GetSize();
		infoOut.filename = m_stream.GetFilename();
	}
	else
	{
		ZeroStruct(infoOut);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::AllocUsage(EMemReplayAllocClass allocClass, UINT_PTR id, UINT_PTR used)
{
	#if REPLAY_RECORD_USAGE_CHANGES
	if (!ptr)
		return;

	if (m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());
		if (m_stream.IsOpen())
			m_stream.WriteEvent(MemReplayAllocUsageEvent((uint16)allocClass, id, used));
	}
	#endif
}

void CMemReplay::AddAllocReference(void* ptr, void* ref)
{
	if (ptr && m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			MemReplayAddAllocReferenceEvent* ev = m_stream.BeginAllocateRawEvent<MemReplayAddAllocReferenceEvent>(
			  k_maxCallStackDepth * SIZEOF_MEMBER(MemReplayAddAllocReferenceEvent, callstack[0])
			  - SIZEOF_MEMBER(MemReplayAddAllocReferenceEvent, callstack));

			new(ev) MemReplayAddAllocReferenceEvent(reinterpret_cast<UINT_PTR>(ptr), reinterpret_cast<UINT_PTR>(ref));

			ev->callstackLength = WriteCallstack(ev->callstack);
			m_stream.EndAllocateRawEvent<MemReplayAddAllocReferenceEvent>(ev->callstackLength * sizeof(ev->callstack[0]) - sizeof(ev->callstack));
		}
	}
}

void CMemReplay::RemoveAllocReference(void* ref)
{
	if (ref && m_stream.IsOpen() && ThreadIsNotFlushing() && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			m_stream.WriteEvent(MemReplayRemoveAllocReferenceEvent(reinterpret_cast<UINT_PTR>(ref)));
	}
}

IMemReplay::FixedContextID CMemReplay::AddFixedContext(EMemStatContextType type, const char* str)
{
	const threadID threadId = CryGetCurrentThreadId();
	if (m_stream.IsOpen() && threadId != s_recordThreadId && !g_memReplayPaused)
	{
		CryAutoLock<CryCriticalSection> lock(GetLogMutex());
		static IMemReplay::FixedContextID s_fixedContextCounter = 0;
		if (m_stream.IsOpen())
		{
			FixedContextID id = s_fixedContextCounter++;
			new(m_stream.AllocateRawEvent<MemReplayAddFixedContextEvent>(strlen(str)))
				MemReplayAddFixedContextEvent(threadId, str, type, id);
			return id;
		}
	}
	return CMemStatStaticContext::s_invalidContextId;
}

void CMemReplay::PushFixedContext(FixedContextID id)
{
	const threadID threadId = CryGetCurrentThreadId();
	if (m_stream.IsOpen() && threadId != s_recordThreadId && !g_memReplayPaused)
	{
		CryAutoLock<CryCriticalSection> lock(GetLogMutex());
		if (m_stream.IsOpen())
		{
			m_stream.WriteEvent(MemReplayPushFixedContextEvent(threadId, id));
		}
	}
}

void CMemReplay::PushContext(EMemStatContextType type, const char* str)
{
	if (m_stream.IsOpen() && ThreadIsNotFlushing())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			new(m_stream.AllocateRawEvent<MemReplayPushContextEvent>(strlen(str)))
			MemReplayPushContextEvent(CryGetCurrentThreadId(), str, type);
		}
	}
}

void CMemReplay::PushContextV(EMemStatContextType type, const char* format, va_list args)
{
	if (m_stream.IsOpen() && ThreadIsNotFlushing())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			MemReplayPushContextEvent* ev = m_stream.BeginAllocateRawEvent<MemReplayPushContextEvent>(511);
			new(ev) MemReplayPushContextEvent(CryGetCurrentThreadId(), "", type);

			cry_vsprintf(ev->name, 512, format, args);

			m_stream.EndAllocateRawEvent<MemReplayPushContextEvent>(strlen(ev->name));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMemReplay::PopContext()
{
	if (m_stream.IsOpen() && ThreadIsNotFlushing())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			m_stream.WriteEvent(MemReplayPopContextEvent(CryGetCurrentThreadId()));
		}
	}
}

void CMemReplay::Flush()
{
	if (m_stream.IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			m_stream.Flush();
	}
}

bool CMemReplay::EnableAsynchMode()
{
	if (m_stream.IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			return m_stream.EnableAsynchMode();
	}

	return false;
}

void CMemReplay::MapPage(void* base, size_t size)
{
	if (m_stream.IsOpen() && base && size && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			MemReplayMapPageEvent* ev = m_stream.BeginAllocateRawEvent<MemReplayMapPageEvent>(
			  k_maxCallStackDepth * sizeof(void*) - SIZEOF_MEMBER(MemReplayMapPageEvent, callstack));
			new(ev) MemReplayMapPageEvent(reinterpret_cast<UINT_PTR>(base), size);

			ev->callstackLength = WriteCallstack(ev->callstack);
			m_stream.EndAllocateRawEvent<MemReplayMapPageEvent>(sizeof(ev->callstack[0]) * ev->callstackLength - sizeof(ev->callstack));
		}
	}
}

void CMemReplay::UnMapPage(void* base, size_t size)
{
	if (m_stream.IsOpen() && base && size && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			m_stream.WriteEvent(MemReplayUnMapPageEvent(reinterpret_cast<UINT_PTR>(base), size));
		}
	}
}

void CMemReplay::RegisterFixedAddressRange(void* base, size_t size, const char* name)
{
	if (m_stream.IsOpen() && base && size && name && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			new(m_stream.AllocateRawEvent<MemReplayRegisterFixedAddressRangeEvent>(strlen(name)))
			MemReplayRegisterFixedAddressRangeEvent(reinterpret_cast<UINT_PTR>(base), size, name);
		}
	}
}

void CMemReplay::MarkBucket(int bucket, size_t alignment, void* base, size_t length)
{
	if (m_stream.IsOpen() && base && length && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			m_stream.WriteEvent(MemReplayBucketMarkEvent(reinterpret_cast<UINT_PTR>(base), length, bucket, alignment));
	}
}

void CMemReplay::UnMarkBucket(int bucket, void* base)
{
	if (m_stream.IsOpen() && base && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			m_stream.WriteEvent(MemReplayBucketUnMarkEvent(reinterpret_cast<UINT_PTR>(base), bucket));
	}
}

void CMemReplay::BucketEnableCleanups(void* allocatorBase, bool enabled)
{
	if (m_stream.IsOpen() && allocatorBase && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			m_stream.WriteEvent(MemReplayBucketCleanupEnabledEvent(reinterpret_cast<UINT_PTR>(allocatorBase), enabled));
	}
}

void CMemReplay::MarkPool(int pool, size_t alignment, void* base, size_t length, const char* name)
{
	if (m_stream.IsOpen() && base && length && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			new(m_stream.AllocateRawEvent<MemReplayPoolMarkEvent>(strlen(name)))
			MemReplayPoolMarkEvent(reinterpret_cast<UINT_PTR>(base), length, pool, alignment, name);
		}
	}
}

void CMemReplay::UnMarkPool(int pool, void* base)
{
	if (m_stream.IsOpen() && base && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
			m_stream.WriteEvent(MemReplayPoolUnMarkEvent(reinterpret_cast<UINT_PTR>(base), pool));
	}
}

void CMemReplay::AddTexturePoolContext(void* ptr, int mip, int width, int height, const char* name, uint32 flags)
{
	if (m_stream.IsOpen() && ptr && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			new(m_stream.AllocateRawEvent<MemReplayTextureAllocContextEvent>(strlen(name)))
			MemReplayTextureAllocContextEvent(reinterpret_cast<UINT_PTR>(ptr), mip, width, height, flags, name);
		}
	}
}

void CMemReplay::AddSizerTree(const char* name)
{
	MemReplayCrySizer sizer(m_stream, name);
	static_cast<CSystem*>(gEnv->pSystem)->CollectMemStats(&sizer);
}

void CMemReplay::AddScreenshot()
{
	#if 0
	if (m_stream.IsOpen() && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			PREFAST_SUPPRESS_WARNING(6255)
			MemReplayScreenshotEvent * ev = new(alloca(sizeof(MemReplayScreenshotEvent) + 65536))MemReplayScreenshotEvent();
			gEnv->pRenderer->ScreenShotBuf(8, ev->bmp);
			m_stream.WriteEvent(ev, ev->bmp[0] * ev->bmp[1] * 3 + 3);
		}
	}
	#endif
}

void CMemReplay::RegisterContainer(const void* key, int type)
{
	#if REPLAY_RECORD_CONTAINERS
	if (key)
	{
		if (m_stream.IsOpen() && !g_memReplayPaused)
		{
			TLogAutoLock lock(GetLogMutex());

			if (m_stream.IsOpen())
			{
				MemReplayRegisterContainerEvent* ev = m_stream.BeginAllocateRawEvent<MemReplayRegisterContainerEvent>(
				  k_maxCallStackDepth * SIZEOF_MEMBER(MemReplayRegisterContainerEvent, callstack[0]) - SIZEOF_MEMBER(MemReplayRegisterContainerEvent, callstack));

				new(ev) MemReplayRegisterContainerEvent(reinterpret_cast<UINT_PTR>(key), type);

				WriteCallstack(ev->callstack, ev->callstackLength);
				m_stream.EndAllocateRawEvent<MemReplayRegisterContainerEvent>(length * sizeof(ev->callstack[0]) - sizeof(ev->callstack));
			}
		}
	}
	#endif
}

void CMemReplay::UnregisterContainer(const void* key)
{
	#if REPLAY_RECORD_CONTAINERS
	if (key)
	{
		if (m_stream.IsOpen() && !g_memReplayPaused)
		{
			TLogAutoLock lock(GetLogMutex());

			if (m_stream.IsOpen())
			{
				m_stream.WriteEvent(MemReplayUnregisterContainerEvent(reinterpret_cast<UINT_PTR>(key)));
			}
		}
	}
	#endif
}

void CMemReplay::BindToContainer(const void* key, const void* alloc)
{
	#if REPLAY_RECORD_CONTAINERS
	if (key && alloc)
	{
		if (m_stream.IsOpen() && !g_memReplayPaused)
		{
			TLogAutoLock lock(GetLogMutex());

			if (m_stream.IsOpen())
			{
				m_stream.WriteEvent(MemReplayBindToContainerEvent(reinterpret_cast<UINT_PTR>(key), reinterpret_cast<UINT_PTR>(alloc)));
			}
		}
	}
	#endif
}

void CMemReplay::UnbindFromContainer(const void* key, const void* alloc)
{
	#if REPLAY_RECORD_CONTAINERS
	if (key && alloc)
	{
		if (m_stream.IsOpen() && !g_memReplayPaused)
		{
			TLogAutoLock lock(GetLogMutex());

			if (m_stream.IsOpen())
			{
				m_stream.WriteEvent(MemReplayUnbindFromContainerEvent(reinterpret_cast<UINT_PTR>(key), reinterpret_cast<UINT_PTR>(alloc)));
			}
		}
	}
	#endif
}

void CMemReplay::SwapContainers(const void* keyA, const void* keyB)
{
	#if REPLAY_RECORD_CONTAINERS
	if (keyA && keyB && (keyA != keyB) && m_stream.IsOpen() && !g_memReplayPaused)
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream.IsOpen())
		{
			m_stream.WriteEvent(MemReplaySwapContainersEvent(reinterpret_cast<UINT_PTR>(keyA), reinterpret_cast<UINT_PTR>(keyB)));
		}
	}
	#endif
}

void CMemReplay::RecordModuleLoad(void* pSelf, const CReplayModules::ModuleLoadDesc& mld)
{
	CMemReplay* pMR = reinterpret_cast<CMemReplay*>(pSelf);

	const char* baseName = strrchr(mld.name, '\\');
	if (!baseName)
		baseName = mld.name;

	pMR->m_stream.WriteEvent(MemReplayModuleRefEvent(mld.name, mld.path, mld.address, mld.size, mld.sig));

	pMR->PushContext(EMemStatContextType::Other, baseName);

	pMR->RecordAlloc(
	  EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::CryMalloc, eCryModule,
	  (UINT_PTR)mld.address,
	  4096,
	  mld.size,
	  mld.size,
	  0);

	pMR->PopContext();
}

void CMemReplay::RecordModuleUnload(void* pSelf, const CReplayModules::ModuleUnloadDesc& mld)
{
	CMemReplay* pMR = reinterpret_cast<CMemReplay*>(pSelf);

	PREFAST_SUPPRESS_WARNING(6326)
	pMR->RecordFree(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::CryMalloc, eCryModule, mld.address, 0);
	pMR->m_stream.WriteEvent(MemReplayModuleUnRefEvent(mld.address));
}

void CMemReplay::RecordAlloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR p, UINT_PTR alignment, UINT_PTR sizeRequested, UINT_PTR sizeConsumed, INT_PTR sizeGlobal)
{
	MemReplayAllocEvent* ev = m_stream.BeginAllocateRawEvent<MemReplayAllocEvent>(
	  k_maxCallStackDepth * sizeof(void*) - SIZEOF_MEMBER(MemReplayAllocEvent, callstack));

	new(ev) MemReplayAllocEvent(
	  CryGetCurrentThreadId(),
	  static_cast<uint16>(moduleId),
	  static_cast<uint16>(cls),
	  static_cast<uint16>(subCls),
	  p,
	  static_cast<uint32>(alignment),
	  static_cast<uint32>(sizeRequested),
	  static_cast<uint32>(sizeConsumed),
	  static_cast<int32>(sizeGlobal));

	ev->callstackLength = WriteCallstack(ev->callstack);
	m_stream.EndAllocateRawEvent<MemReplayAllocEvent>(sizeof(ev->callstack[0]) * ev->callstackLength - sizeof(ev->callstack));
}

void CMemReplay::RecordRealloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR originalId, UINT_PTR newId, UINT_PTR alignment, UINT_PTR sizeRequested, UINT_PTR sizeConsumed, INT_PTR sizeGlobal)
{
	MemReplayReallocEvent* ev = new(m_stream.BeginAllocateRawEvent<MemReplayReallocEvent>(
									  k_maxCallStackDepth * SIZEOF_MEMBER(MemReplayReallocEvent, callstack[0]) - SIZEOF_MEMBER(MemReplayReallocEvent, callstack)))
								MemReplayReallocEvent(
	  CryGetCurrentThreadId(),
	  static_cast<uint16>(moduleId),
	  static_cast<uint16>(cls),
	  static_cast<uint16>(subCls),
	  originalId,
	  newId,
	  static_cast<uint32>(alignment),
	  static_cast<uint32>(sizeRequested),
	  static_cast<uint32>(sizeConsumed),
	  static_cast<int32>(sizeGlobal));

	ev->callstackLength = WriteCallstack(ev->callstack);
	m_stream.EndAllocateRawEvent<MemReplayReallocEvent>(ev->callstackLength * sizeof(ev->callstack[0]) - sizeof(ev->callstack));
}

void CMemReplay::RecordFree(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id, INT_PTR sizeGlobal)
{
	MemReplayFreeEvent* ev =
	  new(m_stream.BeginAllocateRawEvent<MemReplayFreeEvent>(SIZEOF_MEMBER(MemReplayFreeEvent, callstack[0]) * k_maxCallStackDepth - SIZEOF_MEMBER(MemReplayFreeEvent, callstack)))
	  MemReplayFreeEvent(
		CryGetCurrentThreadId(),
		static_cast<uint16>(moduleId),
		static_cast<uint16>(cls),
		static_cast<uint16>(subCls),
		id,
		static_cast<int32>(sizeGlobal));

	if (REPLAY_RECORD_FREECS)
		ev->callstackLength = WriteCallstack(ev->callstack);
	m_stream.EndAllocateRawEvent<MemReplayFreeEvent>(ev->callstackLength * sizeof(ev->callstack[0]) - sizeof(ev->callstack));
}

void CMemReplay::RecordModules()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "DLL Image Size (incl. Data Segment)");
	m_modules.RefreshModules(RecordModuleLoad, RecordModuleUnload, this);
}

uint16 CMemReplay::WriteCallstack(UINT_PTR* callstack)
{
	uint32 length;
	if (s_memReplayRecordCallstacks)
	{
		static_assert(k_maxCallStackDepth < uint16(~0), "Maximum stack depth is too large!");
		length = k_maxCallStackDepth;
		CSystem::debug_GetCallStackRaw(CastCallstack(callstack), length);
	}
	else
	{
		length = 1;
		*callstack = 0;
	}
	return length;
}

MemReplayCrySizer::MemReplayCrySizer(ReplayLogStream& stream, const char* name)
	: m_stream(&stream)
	, m_totalSize(0)
	, m_totalCount(0)
{
	Push(name);
}

MemReplayCrySizer::~MemReplayCrySizer()
{
	Pop();

	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::Unknown);

		if (m_stream->IsOpen())
		{
			TLogAutoLock lock(GetLogMutex());

			if (m_stream->IsOpen())
			{
				// Clear the added objects set within the g_replayProcessed lock,
				// as memReplay won't have seen the allocations made by it.
				m_addedObjects.clear();
			}
		}
	}
}

size_t MemReplayCrySizer::GetTotalSize()
{
	return m_totalSize;
}

size_t MemReplayCrySizer::GetObjectCount()
{
	return m_totalCount;
}

void MemReplayCrySizer::Reset()
{
	m_totalSize = 0;
	m_totalCount = 0;
}

bool MemReplayCrySizer::AddObject(const void* pIdentifier, size_t nSizeBytes, int nCount)
{
	bool ret = false;

	if (m_stream->IsOpen())
	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::UserPointer, EMemReplayUserPointerClass::Unknown);
		TLogAutoLock lock(GetLogMutex());

		if (m_stream->IsOpen())
		{
			std::set<const void*>::iterator it = m_addedObjects.find(pIdentifier);
			if (it == m_addedObjects.end())
			{
				m_totalSize += nSizeBytes;
				m_totalCount += nCount;
				m_addedObjects.insert(pIdentifier);
				m_stream->WriteEvent(MemReplaySizerAddRangeEvent((UINT_PTR)pIdentifier, nSizeBytes, nCount));
				ret = true;
			}
		}
	}

	return ret;
}

static NullResCollector s_nullCollectorMemreplay;

IResourceCollector* MemReplayCrySizer::GetResourceCollector()
{
	return &s_nullCollectorMemreplay;
}

void MemReplayCrySizer::SetResourceCollector(IResourceCollector*)
{
}

void MemReplayCrySizer::Push(const char* szComponentName)
{
	if (m_stream->IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream->IsOpen())
		{
			new(m_stream->AllocateRawEvent<MemReplaySizerPushEvent>(strlen(szComponentName)))MemReplaySizerPushEvent(szComponentName);
		}
	}
}

void MemReplayCrySizer::PushSubcomponent(const char* szSubcomponentName)
{
	Push(szSubcomponentName);
}

void MemReplayCrySizer::Pop()
{
	if (m_stream->IsOpen())
	{
		TLogAutoLock lock(GetLogMutex());

		if (m_stream->IsOpen())
		{
			m_stream->WriteEvent(MemReplaySizerPopEvent());
		}
	}
}

#endif //CAPTURE_REPLAY_LOG
