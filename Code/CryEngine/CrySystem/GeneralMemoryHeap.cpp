// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "GeneralMemoryHeap.h"
#include "PageMappingHeap.h"

CGeneralMemoryHeap::CGeneralMemoryHeap()
	: m_nRefCount(0)
	, m_numAllocs(0)
	, m_isResizable(false)
	, m_mspace(0)
	, m_pHeap(NULL)
	, m_pBlock(NULL)
	, m_blockSize(0)
{
}

CGeneralMemoryHeap::CGeneralMemoryHeap(UINT_PTR base, size_t upperLimit, size_t reserveSize, const char* sUsage)
	: m_nRefCount(0)
	, m_numAllocs(0)
	, m_isResizable(true)
	, m_pHeap(NULL)
	, m_pBlock(NULL)
	, m_blockSize(0)
{
	if (base)
		m_pHeap = new CPageMappingHeap((char*)base, upperLimit / CMemoryAddressRange::GetSystemPageSize(), CMemoryAddressRange::GetSystemPageSize(), sUsage);
	else
		m_pHeap = new CPageMappingHeap(upperLimit, sUsage);

	m_mspace = dlcreate_mspace(max(0, (int)reserveSize - dlmspace_create_overhead()), 0, this, DLMMap, DLMUnMap);

#ifdef CRY_TRACE_HEAP
	UINT_PTR addressSpaceBase = reinterpret_cast<UINT_PTR>(m_mspace) & ~(m_pHeap->GetGranularity() - 1);
	m_nTraceHeapHandle = CryGetIMemoryManager()->TraceDefineHeap(sUsage, upperLimit, reinterpret_cast<void*>(addressSpaceBase));
#endif // CRY_TRACE_HEAP
}

CGeneralMemoryHeap::CGeneralMemoryHeap(void* base, size_t size, const char* sUsage)
	: m_nRefCount(0)
	, m_numAllocs(0)
	, m_isResizable(false)
	, m_pHeap(NULL)
	, m_pBlock(NULL)
	, m_blockSize(0)
{
	m_pBlock = base;
	m_blockSize = size;

	m_mspace = dlcreate_mspace_with_base(base, size, 1);

#ifdef CRY_TRACE_HEAP
	UINT_PTR addressSpaceBase = reinterpret_cast<UINT_PTR>(m_mspace);
	m_nTraceHeapHandle = CryGetIMemoryManager()->TraceDefineHeap(sUsage, size, reinterpret_cast<void*>(addressSpaceBase));
#endif // CRY_TRACE_HEAP
}

CGeneralMemoryHeap::~CGeneralMemoryHeap()
{
	if (m_mspace)
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);
		dldestroy_mspace(m_mspace);
	}

	delete m_pHeap;
}

bool CGeneralMemoryHeap::Cleanup()
{
	bool bCleanedFully = false;

	if (m_isResizable)
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);
		if (m_numAllocs == 0)
		{
			// There aren't any allocations remaining in dlmalloc, so reset it to fully
			// relinquish any memory it's holding onto.
			dldestroy_mspace(m_mspace);
			m_mspace = dlcreate_mspace(0, 0, this, DLMMap, DLMUnMap);

			bCleanedFully = true;
		}
		else
		{
			dlmspace_trim(m_mspace, 0);
		}
	}

	return bCleanedFully;
}

int CGeneralMemoryHeap::AddRef()
{
	return CryInterlockedIncrement(&m_nRefCount);
}

int CGeneralMemoryHeap::Release()
{
	int nRef = CryInterlockedDecrement(&m_nRefCount);

#if !defined(_RELEASE)
	//	IF (nRef < 0, 0)
	//	  __debugbreak();
#endif
	if (nRef == 0)
		delete this;

	return nRef;
}

bool CGeneralMemoryHeap::IsInAddressRange(void* ptr) const
{
	if (m_isResizable)
	{
		return m_pHeap->IsInAddressRange(ptr);
	}
	else
	{
		UINT_PTR base = reinterpret_cast<UINT_PTR>(m_pBlock);
		UINT_PTR end = base + m_blockSize;
		UINT_PTR ptri = reinterpret_cast<UINT_PTR>(ptr);
		return base <= ptri && ptri < end;
	}
}

void* CGeneralMemoryHeap::Calloc(size_t nmemb, size_t size, const char* sUsage)
{
#if CAPTURE_REPLAY_LOG
	bool ms = m_isResizable ? CryGetIMemReplay()->EnterScope(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc, eCryModule) : 0;
#endif

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);
	void* ptr = dlmspace_calloc(m_mspace, nmemb, size);

#ifdef CRY_TRACE_HEAP
	if (sUsage && m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
	{
		CryGetIMemoryManager()->TraceHeapAlloc(m_nTraceHeapHandle, ptr, size, size, sUsage);
	}
#endif // CRY_TRACE_HEAP

	if (ptr)
	{
		++m_numAllocs;
	}

#if CAPTURE_REPLAY_LOG
	if (ms)
		CryGetIMemReplay()->ExitScope_Alloc((UINT_PTR)ptr, (UINT_PTR)(nmemb * size));
#endif

	return ptr;
}

void* CGeneralMemoryHeap::Malloc(size_t sz, const char* sUsage)
{
#if CAPTURE_REPLAY_LOG
	bool ms = m_isResizable ? CryGetIMemReplay()->EnterScope(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc, eCryModule) : 0;
#endif

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);
	void* ptr = dlmspace_malloc(m_mspace, sz);

#ifdef CRY_TRACE_HEAP
	if (sUsage && m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
	{
		CryGetIMemoryManager()->TraceHeapAlloc(m_nTraceHeapHandle, ptr, sz, sz, sUsage);
	}
#endif // CRY_TRACE_HEAP

	if (ptr)
	{
		++m_numAllocs;
	}

#if CAPTURE_REPLAY_LOG
	if (ms)
		CryGetIMemReplay()->ExitScope_Alloc((UINT_PTR)ptr, (UINT_PTR)sz);
#endif

	return ptr;
}

size_t CGeneralMemoryHeap::Free(void* ptr)
{
	UINT_PTR ptri = reinterpret_cast<UINT_PTR>(ptr);

	if (CGeneralMemoryHeap::IsInAddressRange(ptr))
	{
#if CAPTURE_REPLAY_LOG
		bool ms = m_isResizable ? CryGetIMemReplay()->EnterScope(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc, eCryModule) : 0;
#endif

		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);
		size_t sz = dlmspace_usable_size(ptr);
		dlmspace_free(m_mspace, ptr);

#ifdef CRY_TRACE_HEAP
		if (m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
		{
			CryGetIMemoryManager()->TraceHeapFree(m_nTraceHeapHandle, ptr, sz);
		}
#endif // CRY_TRACE_HEAP

		--m_numAllocs;

#if CAPTURE_REPLAY_LOG
		if (ms)
			CryGetIMemReplay()->ExitScope_Free((UINT_PTR)ptr);
#endif

		return sz;
	}

	return 0;
}

void* CGeneralMemoryHeap::Realloc(void* ptr, size_t sz, const char* sUsage)
{
	if (!ptr)
	{
		return Malloc(sz, sUsage);
	}

	if (!sz)
	{
		Free(ptr);
		return NULL;
	}

#if CAPTURE_REPLAY_LOG
	bool ms = m_isResizable ? CryGetIMemReplay()->EnterScope(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc, eCryModule) : 0;
#endif

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);

#ifdef CRY_TRACE_HEAP
	if (m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
	{
		size_t szOld = dlmspace_usable_size(ptr);
		CryGetIMemoryManager()->TraceHeapFree(m_nTraceHeapHandle, ptr, szOld);
	}
#endif // CRY_TRACE_HEAP

	void* pNewPtr = dlmspace_realloc(m_mspace, ptr, sz);

#ifdef CRY_TRACE_HEAP
	if (sUsage && m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
	{
		CryGetIMemoryManager()->TraceHeapAlloc(m_nTraceHeapHandle, pNewPtr, sz, sz, sUsage);
	}
#endif // CRY_TRACE_HEAP

#if CAPTURE_REPLAY_LOG
	if (ms)
	{
		if (pNewPtr)
			CryGetIMemReplay()->ExitScope_Realloc((UINT_PTR)ptr, (UINT_PTR)pNewPtr, (UINT_PTR)sz);
		else
			CryGetIMemReplay()->ExitScope();
	}
#endif

	return pNewPtr;
}

void* CGeneralMemoryHeap::ReallocAlign(void* ptr, size_t size, size_t alignment, const char* sUsage)
{
	if (!ptr)
	{
		return Memalign(alignment, size, sUsage);
	}

	if (!size)
	{
		Free(ptr);
		return NULL;
	}

#if CAPTURE_REPLAY_LOG
	bool ms = m_isResizable ? CryGetIMemReplay()->EnterScope(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc, eCryModule) : 0;
#endif

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);

	size_t oldSize = dlmspace_usable_size(ptr);
	void* newPtr = dlmspace_memalign(m_mspace, alignment, size);

	// Check if the new allocation failed before copying to new location
	if (newPtr == NULL)
		return NULL;

	memcpy(newPtr, ptr, std::min<size_t>(oldSize, size));
	dlmspace_free(m_mspace, ptr);

#ifdef CRY_TRACE_HEAP
	if (sUsage && m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
	{
		CryGetIMemoryManager()->TraceHeapFree(m_nTraceHeapHandle, ptr, oldSize);
		CryGetIMemoryManager()->TraceHeapAlloc(m_nTraceHeapHandle, newPtr, size, size, sUsage);
	}
#endif // CRY_TRACE_HEAP

#if CAPTURE_REPLAY_LOG
	if (ms)
		CryGetIMemReplay()->ExitScope_Realloc((UINT_PTR)ptr, (UINT_PTR)newPtr, (UINT_PTR)size, (UINT_PTR)alignment);
#endif

	return newPtr;
}

void* CGeneralMemoryHeap::Memalign(size_t boundary, size_t size, const char* sUsage)
{
#if CAPTURE_REPLAY_LOG
	bool ms = m_isResizable ? CryGetIMemReplay()->EnterScope(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc, eCryModule) : 0;
#endif

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_mspaceLock);
	void* ptr = dlmspace_memalign(m_mspace, boundary, size);

#ifdef CRY_TRACE_HEAP
	if (sUsage && m_nTraceHeapHandle != IMemoryManager::BAD_HEAP_HANDLE)
	{
		CryGetIMemoryManager()->TraceHeapAlloc(m_nTraceHeapHandle, ptr, size, Align(size, boundary), sUsage);
	}
#endif // CRY_TRACE_HEAP

	if (ptr)
	{
		++m_numAllocs;
	}

#if CAPTURE_REPLAY_LOG
	if (ms)
		CryGetIMemReplay()->ExitScope_Alloc((UINT_PTR)ptr, (UINT_PTR)size, (UINT_PTR)boundary);
#endif

	return ptr;
}

size_t CGeneralMemoryHeap::UsableSize(void* ptr) const
{
	UINT_PTR ptri = reinterpret_cast<UINT_PTR>(ptr);

	if (CGeneralMemoryHeap::IsInAddressRange(ptr))
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(const_cast<CryCriticalSectionNonRecursive&>(m_mspaceLock));
		return dlmspace_usable_size(ptr);
	}

	return 0;
}

void* CGeneralMemoryHeap::DLMMap(void* vself, size_t length)
{
	CGeneralMemoryHeap* heap = reinterpret_cast<CGeneralMemoryHeap*>(vself);
	void* ptr = heap->m_pHeap->Map(length);
	if (ptr)
		return ptr;
	else
		return dlmmap_error;
}

int CGeneralMemoryHeap::DLMUnMap(void* vself, void* mem, size_t length)
{
	CGeneralMemoryHeap* heap = reinterpret_cast<CGeneralMemoryHeap*>(vself);
	heap->m_pHeap->Unmap(mem, length);
	return 0;
}
