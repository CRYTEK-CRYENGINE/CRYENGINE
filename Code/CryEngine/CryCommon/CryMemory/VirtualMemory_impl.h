// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "VirtualMemory.h"

#if CRY_PLATFORM_WINAPI
#	include <CryCore/Platform/CryWindows.h>
#	include <sysinfoapi.h>
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
#	include <sys/mman.h>
#endif

#if CAPTURE_REPLAY_LOG
#	define VA_REGISTER_RANGE(PBASE, SIZE, NAME) CryGetIMemReplay()->RegisterFixedAddressRange(PBASE, SIZE, NAME)
#	define VA_REGISTER_PAGE(PBASE, SIZE)        CryGetIMemReplay()->MapPage(PBASE, SIZE)
#	define VA_UNREGISTER_PAGE(PBASE, SIZE)      CryGetIMemReplay()->UnMapPage(PBASE, SIZE)
#else
#	define VA_REGISTER_RANGE(PBASE, SIZE, NAME)
#	define VA_REGISTER_PAGE(PBASE, SIZE)
#	define VA_UNREGISTER_PAGE(PBASE, SIZE)
#endif

CVirtualMemory::CVirtualMemory()
{
	CRY_ASSERT(ValidateSystemPageSize());
}

#if CRY_PLATFORM_WINAPI

const size_t CVirtualMemory::s_systemPageSize = 4 * 1024;

bool CVirtualMemory::ValidateSystemPageSize()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return s_systemPageSize == si.dwPageSize;
}

void* CVirtualMemory::ReserveAddressRange(size_t reserveSize, const char* usage, size_t alignment)
{
	reserveSize = Align(reserveSize, s_systemPageSize);
	UINT_PTR addr;

	do
	{
		addr = reinterpret_cast<UINT_PTR>(VirtualAlloc(NULL, reserveSize, MEM_RESERVE, PAGE_READWRITE));
		if (!addr)
			break;

		UINT_PTR alignedAddr = alignment == 0 ? addr : Align(addr, alignment);

		if ((alignedAddr - addr) > 0)
		{
			VirtualFree(reinterpret_cast<LPVOID>(addr), 0, MEM_RELEASE);
			addr = reinterpret_cast<UINT_PTR>(VirtualAlloc(reinterpret_cast<LPVOID>(alignedAddr), reserveSize, MEM_RESERVE, PAGE_READWRITE));
		}
	} while (!addr);

	VA_REGISTER_RANGE((void*)addr, reserveSize, usage);
	return reinterpret_cast<void*>(addr);
}

void CVirtualMemory::UnreserveAddressRange(void* pBase, size_t reservedSize, size_t alignment)
{
	VirtualFree(pBase, 0, MEM_RELEASE);
}

void CVirtualMemory::MapPages(void* pBase, size_t size)
{
	VirtualAlloc(pBase, size, MEM_COMMIT, PAGE_READWRITE);
	VA_REGISTER_PAGE(pBase, size);
}

void CVirtualMemory::UnmapPages(void* pBase, size_t size)
{
	VirtualFree(pBase, size, MEM_DECOMMIT);
	VA_UNREGISTER_PAGE(pBase, size);
}

#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE

const size_t CVirtualMemory::s_systemPageSize = 4 * 1024;

#ifdef RELEASE
#	define VA_ASSERT_POSIX_SUCCESS(EXPR) EXPR
#else
#	define VA_ASSERT_POSIX_SUCCESS(EXPR) \
		{ \
			int returnVal = EXPR; \
			CRY_ASSERT(returnVal == 0); \
		}
#endif

bool CVirtualMemory::ValidateSystemPageSize()
{
	return s_systemPageSize == sysconf(_SC_PAGESIZE);
}

#define VA_MAP_FAILED_ADDR (reinterpret_cast<UINT_PTR>(MAP_FAILED))

void* CVirtualMemory::ReserveAddressRange(size_t reserveSize, const char* usage, size_t alignment)
{
	reserveSize = Align(reserveSize, s_systemPageSize);

	if (alignment <= s_systemPageSize)
	{
		void* ptr = mmap(NULL, reserveSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if(ptr == MAP_FAILED)
            return nullptr;
        VA_REGISTER_RANGE(ptr, reserveSize, usage);
		return ptr;
	}

    // mmap on does not necessarily allocate on the requested aligned address,
	// so we have to fix it manually.
	
	size_t paddedReserveSize = reserveSize + alignment;
	UINT_PTR addr = reinterpret_cast<UINT_PTR>(mmap(NULL, paddedReserveSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
    if (addr == VA_MAP_FAILED_ADDR)
		return nullptr;
		
	UINT_PTR upAlignedAddr = (addr + alignment) & ~(alignment - 1);
    size_t diff = upAlignedAddr - addr;

	// Give read and write access to that region in order to store the
	// Real address allocated by mmap.
	mprotect(reinterpret_cast<void*>(addr), diff, PROT_READ | PROT_WRITE);

	// Store the address
	size_t* pAdjustment = ((size_t*)upAlignedAddr) - 1;
	*pAdjustment = diff;
	// Revert to read only.
	mprotect(reinterpret_cast<void*>(addr), diff, PROT_READ);

	VA_REGISTER_RANGE((void*)upAlignedAddr, reserveSize, usage);
	return (void*)upAlignedAddr;
}

void CVirtualMemory::UnreserveAddressRange(void* pBase, size_t reservedSize, size_t alignment)
{
	reservedSize = Align(reservedSize, s_systemPageSize);
	if (alignment <= s_systemPageSize)
    {
		VA_ASSERT_POSIX_SUCCESS( munmap(pBase, reservedSize) );
    }
	else
	{
        size_t paddedReserveSize = reservedSize + alignment;
		size_t* pAdjustment = ((size_t*)pBase) - 1;
		UINT_PTR baseAddr = (UINT_PTR)pBase;
		baseAddr -= *pAdjustment;
		VA_ASSERT_POSIX_SUCCESS( munmap((void*)baseAddr, paddedReserveSize) );
	}
}

void CVirtualMemory::MapPages(void* pBase, size_t size)
{
	UINT_PTR baseAddr = (UINT_PTR)pBase & ~(s_systemPageSize - 1);
	UINT_PTR endAddr = Align((UINT_PTR)pBase + size, s_systemPageSize);
	VA_ASSERT_POSIX_SUCCESS( mprotect((void*)baseAddr, endAddr - baseAddr, PROT_READ | PROT_WRITE) );
	VA_REGISTER_PAGE(pBase, size);
}

void CVirtualMemory::UnmapPages(void* pBase, size_t size)
{
	UINT_PTR baseAddr = (UINT_PTR)pBase & ~(s_systemPageSize - 1);
	UINT_PTR endAddr = Align((UINT_PTR)pBase + size, s_systemPageSize);
	VA_ASSERT_POSIX_SUCCESS( mprotect((void*)baseAddr, endAddr - baseAddr, PROT_NONE) );
    VA_UNREGISTER_PAGE(pBase, size);
}

#endif
