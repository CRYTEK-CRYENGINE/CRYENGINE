// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MemoryManager.h"
#include <CryCore/Platform/platform.h>
#include "MemReplay.h"
#include "CustomMemoryHeap.h"
#include "GeneralMemoryHeap.h"
#include "PageMappingHeap.h"
#include "DefragAllocator.h"

#if CRY_PLATFORM_WINDOWS
	#include <Psapi.h>
#endif

#if CRY_PLATFORM_APPLE
	#include <mach/mach.h> // task_info
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <sys/types.h> // required by mman.h
	#include <sys/mman.h>  //mmap - virtual memory manager
#endif
extern LONG g_TotalAllocatedMemory;

#ifdef MEMMAN_STATIC
CCryMemoryManager g_memoryManager;
#endif

//////////////////////////////////////////////////////////////////////////
CCryMemoryManager* CCryMemoryManager::GetInstance()
{
#ifdef MEMMAN_STATIC
	return &g_memoryManager;
#else
	static CCryMemoryManager memman;
	return &memman;
#endif
}

#ifndef MEMMAN_STATIC
int CCryMemoryManager::s_sys_MemoryDeadListSize;

void CCryMemoryManager::RegisterCVars()
{
	REGISTER_CVAR2("sys_MemoryDeadListSize", &s_sys_MemoryDeadListSize, 0, VF_REQUIRE_APP_RESTART, "Keep upto size bytes in a \"deadlist\" of allocations to assist in capturing tramples");
}
#endif

//////////////////////////////////////////////////////////////////////////
bool CCryMemoryManager::GetProcessMemInfo(SProcessMemInfo& minfo)
{
	ZeroStruct(minfo);
#if CRY_PLATFORM_WINDOWS

	MEMORYSTATUSEX mem;
	mem.dwLength = sizeof(mem);
	GlobalMemoryStatusEx(&mem);

	minfo.TotalPhysicalMemory = mem.ullTotalPhys;
	minfo.FreePhysicalMemory = mem.ullAvailPhys;

	//////////////////////////////////////////////////////////////////////////
	typedef BOOL (WINAPI * GetProcessMemoryInfoProc)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

	PROCESS_MEMORY_COUNTERS pc;
	ZeroStruct(pc);
	pc.cb = sizeof(pc);
	static HMODULE hPSAPI = LoadLibraryA("psapi.dll");
	if (hPSAPI)
	{
		static GetProcessMemoryInfoProc pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(hPSAPI, "GetProcessMemoryInfo");
		if (pGetProcessMemoryInfo)
		{
			if (pGetProcessMemoryInfo(GetCurrentProcess(), &pc, sizeof(pc)))
			{
				minfo.PageFaultCount = pc.PageFaultCount;
				minfo.PeakWorkingSetSize = pc.PeakWorkingSetSize;
				minfo.WorkingSetSize = pc.WorkingSetSize;
				minfo.QuotaPeakPagedPoolUsage = pc.QuotaPeakPagedPoolUsage;
				minfo.QuotaPagedPoolUsage = pc.QuotaPagedPoolUsage;
				minfo.QuotaPeakNonPagedPoolUsage = pc.QuotaPeakNonPagedPoolUsage;
				minfo.QuotaNonPagedPoolUsage = pc.QuotaNonPagedPoolUsage;
				minfo.PagefileUsage = pc.PagefileUsage;
				minfo.PeakPagefileUsage = pc.PeakPagefileUsage;

				return true;
			}
		}
	}
	return false;

#elif CRY_PLATFORM_ORBIS

	size_t mainMemory, videoMemory;
	VirtualAllocator::QueryMemory(mainMemory, videoMemory); // GlobalMemoryStatus would be more accurate but also slower
	minfo.TotalPhysicalMemory = sceKernelGetDirectMemorySize();
	minfo.PeakPagefileUsage = minfo.PagefileUsage = mainMemory + videoMemory;
	minfo.FreePhysicalMemory = minfo.TotalPhysicalMemory - (mainMemory + videoMemory);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID

	MEMORYSTATUS MemoryStatus;
	GlobalMemoryStatus(&MemoryStatus);
	minfo.PagefileUsage = minfo.PeakPagefileUsage = MemoryStatus.dwTotalPhys - MemoryStatus.dwAvailPhys;

	minfo.FreePhysicalMemory = MemoryStatus.dwAvailPhys;
	minfo.TotalPhysicalMemory = MemoryStatus.dwTotalPhys;

	#if CRY_PLATFORM_ANDROID
	// On Android, mallinfo() is an EXTREMELY time consuming operation. Nearly 80% CPU time will be spent
	// on this operation once -memreplay is given. Since WorkingSetSize is only used for statistics and
	// debugging purpose, it's simply ignored.
	minfo.WorkingSetSize = 0;
	#else
	struct mallinfo meminfo = mallinfo();
	minfo.WorkingSetSize = meminfo.usmblks + meminfo.uordblks;
	#endif

#elif CRY_PLATFORM_APPLE

	MEMORYSTATUS MemoryStatus;
	GlobalMemoryStatus(&MemoryStatus);
	minfo.PagefileUsage = minfo.PeakPagefileUsage = MemoryStatus.dwTotalPhys - MemoryStatus.dwAvailPhys;

	minfo.FreePhysicalMemory = MemoryStatus.dwAvailPhys;
	minfo.TotalPhysicalMemory = MemoryStatus.dwTotalPhys;

	// Retrieve WorkingSetSize from task_info
	task_basic_info kTaskInfo;
	mach_msg_type_number_t uInfoCount(sizeof(kTaskInfo) / sizeof(natural_t));
	if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&kTaskInfo, &uInfoCount) != 0)
	{
		gEnv->pLog->LogError("task_info failed\n");
		return false;
	}
	minfo.WorkingSetSize = kTaskInfo.resident_size;
#elif CRY_PLATFORM_DURANGO

	//Memory status
	TITLEMEMORYSTATUS DurangoMemoryStatus;
	DurangoMemoryStatus.dwLength = sizeof(DurangoMemoryStatus);
	if (TitleMemoryStatus(&DurangoMemoryStatus) != 0)
	{
		uint64 titleUsed = (uint64)(DurangoMemoryStatus.ullTitleUsed);
		uint64 legacyUsed = (uint64)(DurangoMemoryStatus.ullLegacyUsed);
		uint64 total = (uint64)(DurangoMemoryStatus.ullTotalMem);

		minfo.PagefileUsage = legacyUsed + titleUsed;

		static uint64 peak = minfo.PagefileUsage;
		if (peak < minfo.PagefileUsage)
			peak = minfo.PagefileUsage;
		minfo.PeakPagefileUsage = peak;

		minfo.TotalPhysicalMemory = total;
		minfo.FreePhysicalMemory = total - (legacyUsed + titleUsed);
	}
#else
	return false;
#endif

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::FakeAllocation(long size)
{
	CryInterlockedExchangeAdd((LONG volatile*)&g_TotalAllocatedMemory, (LONG)size);
}

//////////////////////////////////////////////////////////////////////////
CCryMemoryManager::HeapHandle CCryMemoryManager::TraceDefineHeap(const char* heapName, size_t size, const void* pBase)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapAlloc(HeapHandle heap, void* mem, size_t size, size_t blockSize, const char* sUsage, const char* sNameHint)
{

}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapFree(HeapHandle heap, void* mem, size_t blockSize)
{

}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapSetColor(uint32 color)
{

}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapSetLabel(const char* sLabel)
{

}

//////////////////////////////////////////////////////////////////////////
uint32 CCryMemoryManager::TraceHeapGetColor()
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IMemReplay* CCryMemoryManager::GetIMemReplay()
{
#if CAPTURE_REPLAY_LOG
	return CMemReplay::GetInstance();
#else
	static IMemReplay m;
	return &m;
#endif
}

//////////////////////////////////////////////////////////////////////////
ICustomMemoryHeap* const CCryMemoryManager::CreateCustomMemoryHeapInstance(IMemoryManager::EAllocPolicy const eAllocPolicy)
{
	return new CCustomMemoryHeap(eAllocPolicy);
}

IGeneralMemoryHeap* CCryMemoryManager::CreateGeneralExpandingMemoryHeap(size_t upperLimit, size_t reserveSize, const char* sUsage)
{
	return new CGeneralMemoryHeap(static_cast<UINT_PTR>(0), upperLimit, reserveSize, sUsage);
}

IGeneralMemoryHeap* CCryMemoryManager::CreateGeneralMemoryHeap(void* base, size_t sz, const char* sUsage)
{
	return new CGeneralMemoryHeap(base, sz, sUsage);
}

IMemoryAddressRange* CCryMemoryManager::ReserveAddressRange(size_t capacity, const char* sName)
{
	return new CMemoryAddressRange(capacity, sName);
}

IPageMappingHeap* CCryMemoryManager::CreatePageMappingHeap(size_t addressSpace, const char* sName)
{
	return new CPageMappingHeap(addressSpace, sName);
}

IDefragAllocator* CCryMemoryManager::CreateDefragAllocator()
{
	return new CDefragAllocator();
}

void* CCryMemoryManager::AllocPages(size_t size)
{
	void* ret = NULL;
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

#if CRY_PLATFORM_ORBIS

	return CryModuleMemalign(size, PAGE_SIZE);

#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#else

	ret = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#endif

	if (ret)
	{
		MEMREPLAY_SCOPE_ALLOC(ret, size, 4096);
	}

	return ret;
}

void CCryMemoryManager::FreePages(void* p, size_t size)
{
	UINT_PTR id = (UINT_PTR)p;
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

#if CRY_PLATFORM_ORBIS

	CryModuleMemalignFree(p);

#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#if defined(_DEBUG)
	int ret = munmap(p, size);
	CRY_ASSERT_MESSAGE(ret == 0, "munmap returned error.");
	#else
	munmap(p, size);
	#endif
#else

	VirtualFree(p, 0, MEM_RELEASE);

#endif

	MEMREPLAY_SCOPE_FREE(id);
}

//////////////////////////////////////////////////////////////////////////
extern "C"
{
	CRYMEMORYMANAGER_API void CryGetIMemoryManagerInterface(void** pIMemoryManager)
	{
		// Static instance of the memory manager
		*pIMemoryManager = CCryMemoryManager::GetInstance();
	}
};
