// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MemoryAddressRange.h"
#include <CryCore/Platform/CryWindows.h>

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <sys/mman.h>
#endif

CMemoryAddressRange::CMemoryAddressRange(char* pBaseAddress, size_t nPageSize, size_t nPageCount, const char* sName)
	: m_pBaseAddress(pBaseAddress)
	, m_nPageSize(nPageSize)
	, m_nPageCount(nPageCount)
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	, m_allocatedSpace(0)
#endif
{
	CryGetIMemReplay()->RegisterFixedAddressRange(pBaseAddress, nPageSize * nPageCount, sName);
}

void CMemoryAddressRange::Release()
{
	delete this;
}

char* CMemoryAddressRange::GetBaseAddress() const
{
	return m_pBaseAddress;
}

size_t CMemoryAddressRange::GetPageCount() const
{
	return m_nPageCount;
}

size_t CMemoryAddressRange::GetPageSize() const
{
	return m_nPageSize;
}

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO

void* CMemoryAddressRange::ReserveSpace(size_t capacity)
{
	return VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE);
}

size_t CMemoryAddressRange::GetSystemPageSize()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwPageSize;
}

CMemoryAddressRange::CMemoryAddressRange(size_t capacity, const char* name)
{
	m_nPageSize = GetSystemPageSize();

	size_t algnCap = Align(capacity, m_nPageSize);
	m_pBaseAddress = (char*)ReserveSpace(algnCap);
	m_nPageCount = algnCap / m_nPageSize;

	if (m_pBaseAddress && name && name[0])
	{
		CryGetIMemReplay()->RegisterFixedAddressRange(m_pBaseAddress, m_nPageSize * m_nPageCount, name);
	}
}

CMemoryAddressRange::~CMemoryAddressRange()
{
	VirtualFree(m_pBaseAddress, 0, MEM_RELEASE);
}

void* CMemoryAddressRange::MapPage(size_t pageIdx)
{
	void* pRet = VirtualAlloc(m_pBaseAddress + pageIdx * m_nPageSize, m_nPageSize, MEM_COMMIT, PAGE_READWRITE);

	#if CAPTURE_REPLAY_LOG
	if (pRet)
		CryGetIMemReplay()->MapPage(pRet, m_nPageSize);
	#endif

	return pRet;
}

void CMemoryAddressRange::UnmapPage(size_t pageIdx)
{
	char* pBase = m_pBaseAddress + pageIdx * m_nPageSize;

	CryGetIMemReplay()->UnMapPage(pBase, m_nPageSize);

	// Disable warning about only decommitting pages, and not releasing them
	#pragma warning( push )
	#pragma warning( disable : 6250 )
	VirtualFree(pBase, m_nPageSize, MEM_DECOMMIT);
	#pragma warning( pop )
}

#elif CRY_PLATFORM_ORBIS

void* CMemoryAddressRange::ReserveSpace(size_t capacity)
{
	return VirtualAllocator::AllocateVirtualAddressSpace(capacity);
}

size_t CMemoryAddressRange::GetSystemPageSize()
{
	return PAGE_SIZE;
}

CMemoryAddressRange::CMemoryAddressRange(size_t capacity, const char* name)
{
	m_nPageSize = PAGE_SIZE;

	size_t algnCap = Align(capacity, std::max<size_t>(1024 * 1024, m_nPageSize));
	m_pBaseAddress = (char*)ReserveSpace(algnCap);
	m_nPageCount = algnCap / m_nPageSize;

	if (m_pBaseAddress && name && name[0])
	{
		CryGetIMemReplay()->RegisterFixedAddressRange(m_pBaseAddress, m_nPageSize * m_nPageCount, name);
	}
}

CMemoryAddressRange::~CMemoryAddressRange()
{
	if (m_pBaseAddress)
	{
		VirtualAllocator::FreeVirtualAddressSpace(m_pBaseAddress);
	}
}

void* CMemoryAddressRange::MapPage(size_t pageIdx)
{
	char* pBase = m_pBaseAddress + pageIdx * m_nPageSize;
	VirtualAllocator::MapPage(pBase);

	#if CAPTURE_REPLAY_LOG
	if (pBase)
		CryGetIMemReplay()->MapPage(pBase, m_nPageSize);
	#endif

	return pBase;
}

void CMemoryAddressRange::UnmapPage(size_t pageIdx)
{
	char* pBase = m_pBaseAddress + pageIdx * m_nPageSize;
	CryGetIMemReplay()->UnMapPage(pBase, m_nPageSize);

	VirtualAllocator::UnmapPage(pBase);
}

#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE

void* CMemoryAddressRange::ReserveSpace(size_t capacity)
{
	return mmap(0, capacity, PROT_NONE, MAP_ANON | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
}

size_t CMemoryAddressRange::GetSystemPageSize()
{
	return sysconf(_SC_PAGESIZE);
}

CMemoryAddressRange::CMemoryAddressRange(size_t capacity, const char* name)
{
	m_nPageSize = GetSystemPageSize();

	m_allocatedSpace = Align(capacity, m_nPageSize);
	m_pBaseAddress = (char*)ReserveSpace(m_allocatedSpace);
	assert(m_pBaseAddress != MAP_FAILED);
	m_nPageCount = m_allocatedSpace / m_nPageSize;

	if (m_pBaseAddress && name && name[0])
	{
		CryGetIMemReplay()->RegisterFixedAddressRange(m_pBaseAddress, m_nPageSize * m_nPageCount, name);
	}
}

CMemoryAddressRange::~CMemoryAddressRange()
{
	int ret = munmap(m_pBaseAddress, m_allocatedSpace);
	(void) ret;
	assert(ret == 0);
}

void* CMemoryAddressRange::MapPage(size_t pageIdx)
{
	// There is no equivalent to this function with mmap, this
	// happens automatically in the OS. We just return the
	// correct address.
	void* pRet = NULL;
	if (0 == mprotect(m_pBaseAddress + (pageIdx * m_nPageSize), m_nPageSize, PROT_READ | PROT_WRITE))
	{
		pRet = m_pBaseAddress + (pageIdx * m_nPageSize);
	}
	#if CAPTURE_REPLAY_LOG
	if (pRet)
		CryGetIMemReplay()->MapPage(pRet, m_nPageSize);
	#endif

	return pRet;
}

void CMemoryAddressRange::UnmapPage(size_t pageIdx)
{
	char* pBase = m_pBaseAddress + pageIdx * m_nPageSize;
	int ret = mprotect(pBase, m_nPageSize, PROT_NONE);
	(void) ret;
	assert(ret == 0);
	CryGetIMemReplay()->UnMapPage(pBase, m_nPageSize);
}

#endif
