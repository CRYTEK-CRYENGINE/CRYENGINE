// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ISystem.h>

#if CRY_PLATFORM_WINDOWS
#	include <Psapi.h>
#endif

#if !defined(_RELEASE)
#	define CRYMM_SUPPORT_DEADLIST
#endif

class CCryMemoryManager : public IMemoryManager
{
public:
	static int s_sys_MemoryDeadListSize;

public:
	CCryMemoryManager();
	virtual ~CCryMemoryManager();

	// Singleton
	static CCryMemoryManager* GetInstance();
	static void               RegisterCVars();

	//////////////////////////////////////////////////////////////////////////
	virtual bool                     GetProcessMemInfo(SProcessMemInfo& minfo);
	virtual void                     FakeAllocation(long size);

	virtual HeapHandle               TraceDefineHeap(const char* heapName, size_t size, const void* pBase);
	virtual void                     TraceHeapAlloc(HeapHandle heap, void* mem, size_t size, size_t blockSize, const char* sUsage, const char* sNameHint = 0);
	virtual void                     TraceHeapFree(HeapHandle heap, void* mem, size_t blockSize);
	virtual void                     TraceHeapSetColor(uint32 color);
	virtual uint32                   TraceHeapGetColor();
	virtual void                     TraceHeapSetLabel(const char* sLabel);

	virtual struct IMemReplay*       GetIMemReplay();
	virtual ICustomMemoryHeap* const CreateCustomMemoryHeapInstance(IMemoryManager::EAllocPolicy const eAllocPolicy);
	virtual IGeneralMemoryHeap*      CreateGeneralExpandingMemoryHeap(size_t upperLimit, size_t reserveSize, const char* sUsage);
	virtual IGeneralMemoryHeap*      CreateGeneralMemoryHeap(void* base, size_t sz, const char* sUsage);

	virtual IMemoryAddressRange*     ReserveAddressRange(size_t capacity, const char* sName);
	virtual IPageMappingHeap*        CreatePageMappingHeap(size_t addressSpace, const char* sName);

	virtual IDefragAllocator*        CreateDefragAllocator();

	virtual void*                    AllocPages(size_t size);
	virtual void                     FreePages(void* p, size_t size);

private:
#if CRY_PLATFORM_WINDOWS
	typedef decltype(&GetProcessMemoryInfo) pfGetProcessMemoryInfo;
	pfGetProcessMemoryInfo m_pGetProcessMemoryInfo;
	HMODULE m_hPSAPI;
#endif
};
