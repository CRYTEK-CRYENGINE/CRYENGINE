// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryMemoryManager.h
//  Version:     v1.00
//  Created:     27/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: Defines functions for CryEngine custom memory manager.
//               See also CryMemoryManager_impl.h, it must be included only once per module.
//               CryMemoryManager_impl.h is included by platform_impl.inl
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CryMemoryManager_h__
#define __CryMemoryManager_h__
#pragma once

#if !defined(_RELEASE)
//! Enable this to check for heap corruption on windows systems by walking the crt.
	#define MEMORY_ENABLE_DEBUG_HEAP 0
#endif

#if CRY_PLATFORM_ORBIS
	#define CRYMM_THROW _THROW0()
#else
	#define CRYMM_THROW throw()
#endif

#if !defined(RELEASE)
	#define CRYMM_THROW_BAD_ALLOC throw(std::bad_alloc)
#else
	#define CRYMM_THROW_BAD_ALLOC throw()
#endif

// Scope based heap checker macro
#if CRY_PLATFORM_WINDOWS && MEMORY_ENABLE_DEBUG_HEAP
	#include <crtdbg.h>
	#define MEMORY_CHECK_HEAP() do { if (!_CrtCheckMemory()) __debugbreak(); } while (0);
struct _HeapChecker
{
	_HeapChecker() { MEMORY_CHECK_HEAP(); }
	~_HeapChecker() { MEMORY_CHECK_HEAP(); }
};
	#define MEMORY_SCOPE_CHECK_HEAP_NAME_EVAL(x, y) x ## y
	#define MEMORY_SCOPE_CHECK_HEAP_NAME MEMORY_SCOPE_CHECK_HEAP_NAME_EVAL(__heap_checker__, __LINE__)
	#define MEMORY_SCOPE_CHECK_HEAP()               _HeapChecker MMRM_SCOPE_CHECK_HEAP_NAME
#endif

#if !defined(MEMORY_CHECK_HEAP)
	#define MEMORY_CHECK_HEAP() void(NULL)
#endif
#if !defined(MEMORY_SCOPE_CHECK_HEAP)
	#define MEMORY_SCOPE_CHECK_HEAP() void(NULL)
#endif

//////////////////////////////////////////////////////////////////////////
// Define this if you want to use slow debug memory manager in any config.
//////////////////////////////////////////////////////////////////////////
//#define DEBUG_MEMORY_MANAGER
//////////////////////////////////////////////////////////////////////////

// That mean we use node_allocator for all small allocations

#include <CryCore/Platform/platform.h>

#include <stdarg.h>
#include <type_traits>
#include <new>

#define _CRY_DEFAULT_MALLOC_ALIGNMENT 4

#if !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	#include <malloc.h>
#endif

#ifdef CRYSYSTEM_EXPORTS
	#define CRYMEMORYMANAGER_API DLL_EXPORT
#else
	#define CRYMEMORYMANAGER_API DLL_IMPORT
#endif

#ifdef __cplusplus
//////////////////////////////////////////////////////////////////////////
	#ifdef DEBUG_MEMORY_MANAGER
		#ifdef _DEBUG
			#define _DEBUG_MODE
		#endif
	#endif

	#if defined(_DEBUG) && CRY_PLATFORM_WINAPI
		#include <crtdbg.h>
	#endif

//! Checks if the heap is valid in debug; in release, this function shouldn't be called.
//! \return Non-0 if it's valid and 0 if not valid.
ILINE int IsHeapValid()
{
	#if (defined(_DEBUG) && !defined(RELEASE_RUNTIME) && CRY_PLATFORM_WINAPI) || (defined(DEBUG_MEMORY_MANAGER))
	return _CrtCheckMemory();
	#else
	return true;
	#endif
}

	#ifdef DEBUG_MEMORY_MANAGER
// Restore debug mode define
		#ifndef _DEBUG_MODE
			#undef _DEBUG
		#endif
	#endif
//////////////////////////////////////////////////////////////////////////

#endif //__cplusplus

struct ICustomMemoryHeap;
class IGeneralMemoryHeap;
class IPageMappingHeap;
class IDefragAllocator;
class IMemoryAddressRange;

//! Interfaces that allow access to the CryEngine memory manager.
struct IMemoryManager
{
	typedef unsigned char HeapHandle;
	enum { BAD_HEAP_HANDLE = 0xFF };

	struct SProcessMemInfo
	{
		uint64 PageFaultCount;
		uint64 PeakWorkingSetSize;
		uint64 WorkingSetSize;
		uint64 QuotaPeakPagedPoolUsage;
		uint64 QuotaPagedPoolUsage;
		uint64 QuotaPeakNonPagedPoolUsage;
		uint64 QuotaNonPagedPoolUsage;
		uint64 PagefileUsage;
		uint64 PeakPagefileUsage;

		uint64 TotalPhysicalMemory;
		int64  FreePhysicalMemory;

		uint64 TotalVideoMemory;
		int64  FreeVideoMemory;
	};

	enum EAllocPolicy
	{
		eapDefaultAllocator,
		eapPageMapped,
		eapCustomAlignment,
#if CRY_PLATFORM_DURANGO
		eapAPU,
#endif
	};

	virtual ~IMemoryManager(){}

	virtual bool GetProcessMemInfo(SProcessMemInfo& minfo) = 0;

	//! Used to add memory block size allocated directly from the crt or OS to the memory manager statistics.
	virtual void FakeAllocation(long size) = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Heap Tracing API.
	virtual HeapHandle TraceDefineHeap(const char* heapName, size_t size, const void* pBase) = 0;
	virtual void       TraceHeapAlloc(HeapHandle heap, void* mem, size_t size, size_t blockSize, const char* sUsage, const char* sNameHint = 0) = 0;
	virtual void       TraceHeapFree(HeapHandle heap, void* mem, size_t blockSize) = 0;
	virtual void       TraceHeapSetColor(uint32 color) = 0;
	virtual uint32     TraceHeapGetColor() = 0;
	virtual void       TraceHeapSetLabel(const char* sLabel) = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Retrieve access to the MemReplay implementation class.
	virtual struct IMemReplay* GetIMemReplay() = 0;

	//! Create an instance of ICustomMemoryHeap.
	virtual ICustomMemoryHeap* const CreateCustomMemoryHeapInstance(EAllocPolicy const eAllocPolicy) = 0;

	virtual IGeneralMemoryHeap*      CreateGeneralExpandingMemoryHeap(size_t upperLimit, size_t reserveSize, const char* sUsage) = 0;
	virtual IGeneralMemoryHeap*      CreateGeneralMemoryHeap(void* base, size_t sz, const char* sUsage) = 0;

	virtual IMemoryAddressRange*     ReserveAddressRange(size_t capacity, const char* sName) = 0;
	virtual IPageMappingHeap*        CreatePageMappingHeap(size_t addressSpace, const char* sName) = 0;

	virtual IDefragAllocator*        CreateDefragAllocator() = 0;

	virtual void*                    AllocPages(size_t size) = 0;
	virtual void                     FreePages(void* p, size_t size) = 0;
};

//! Global function implemented in CryMemoryManager_impl.h.
IMemoryManager* CryGetIMemoryManager();

class STraceHeapAllocatorAutoColor
{
public:
	explicit STraceHeapAllocatorAutoColor(uint32 color) { m_color = CryGetIMemoryManager()->TraceHeapGetColor(); CryGetIMemoryManager()->TraceHeapSetColor(color); }
	~STraceHeapAllocatorAutoColor() { CryGetIMemoryManager()->TraceHeapSetColor(m_color); };
protected:
	uint32 m_color;
	STraceHeapAllocatorAutoColor() {};
};

#define TRACEHEAP_AUTOCOLOR(color) STraceHeapAllocatorAutoColor _auto_color_(color);

//! Structure filled by call to CryModuleGetMemoryInfo().
struct CryModuleMemoryInfo
{
	uint64 requested;

	uint64 allocated;           //!< Total amount of memory allocated.
	uint64 freed;               //!< Total amount of memory freed.
	int    num_allocations;     //!< Total number of memory allocations.
	uint64 CryString_allocated; //!< Allocated in CryString.
	uint64 STL_allocated;       //!< Allocated in STL.
	uint64 STL_wasted;          //!< Amount of memory wasted in pools in stl (not useful allocations).
};

struct CryReplayInfo
{
	uint64      uncompressedLength;
	uint64      writtenLength;
	uint32      trackingSize;
	const char* filename;
};

//////////////////////////////////////////////////////////////////////////
// Extern declarations of globals inside CrySystem.
//////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

CRYMEMORYMANAGER_API void*  CryMalloc(size_t size, size_t& allocated, size_t alignment);
CRYMEMORYMANAGER_API void*  CryRealloc(void* memblock, size_t size, size_t& allocated, size_t& oldsize, size_t alignment);
CRYMEMORYMANAGER_API size_t CryFree(void* p, size_t alignment);
CRYMEMORYMANAGER_API size_t CryGetMemSize(void* p, size_t size);
CRYMEMORYMANAGER_API int    CryStats(char* buf);
CRYMEMORYMANAGER_API void   CryFlushAll();
CRYMEMORYMANAGER_API void   CryCleanup();
CRYMEMORYMANAGER_API int    CryGetUsedHeapSize();
CRYMEMORYMANAGER_API int    CryGetWastedHeapSize();
CRYMEMORYMANAGER_API void*  CrySystemCrtMalloc(size_t size);
CRYMEMORYMANAGER_API void*  CrySystemCrtRealloc(void* p, size_t size);
CRYMEMORYMANAGER_API size_t CrySystemCrtFree(void* p);
CRYMEMORYMANAGER_API size_t CrySystemCrtSize(void* p);
CRYMEMORYMANAGER_API size_t CrySystemCrtGetUsedSpace();
CRYMEMORYMANAGER_API void   CryGetIMemoryManagerInterface(void** pIMemoryManager);

// This function is local in every module
/*CRYMEMORYMANAGER_API*/
void CryGetMemoryInfoForModule(CryModuleMemoryInfo* pInfo);

#ifdef __cplusplus
}
#endif //__cplusplus

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Cry Memory Manager accessible in all build modes.
//////////////////////////////////////////////////////////////////////////
#if !defined(USING_CRY_MEMORY_MANAGER)
	#define USING_CRY_MEMORY_MANAGER
#endif

#ifdef _LIB
	#define CRY_MEM_USAGE_API
#else
	#define CRY_MEM_USAGE_API extern "C" DLL_EXPORT
#endif

#include <CrySystem/Profilers/CryMemReplay.h>

#if CAPTURE_REPLAY_LOG
	#define CRYMM_INLINE inline
#else
	#define CRYMM_INLINE ILINE
#endif

#if defined(NOT_USE_CRY_MEMORY_MANAGER)
CRYMM_INLINE void* CryModuleMalloc(size_t size)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	void* ptr = malloc(size);
	MEMREPLAY_SCOPE_ALLOC(ptr, size, 0);
	return ptr;
}

CRYMM_INLINE void* CryModuleRealloc(void* memblock, size_t size)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	void* ret = realloc(memblock, size);
	MEMREPLAY_SCOPE_REALLOC(memblock, ret, size, 0);
	return ret;
}

CRYMM_INLINE void CryModuleFree(void* memblock)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	free(memblock);
	MEMREPLAY_SCOPE_FREE(memblock);
}

CRYMM_INLINE void CryModuleMemalignFree(void* memblock)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	free(memblock);
	MEMREPLAY_SCOPE_FREE(memblock);
}

CRYMM_INLINE void* CryModuleReallocAlign(void* memblock, size_t size, size_t alignment)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	#if defined(CRY_FORCE_MALLOC_NEW_ALIGN)
		#if defined(__GNUC__)
	// realloc makes no guarantees about the alignment of memory.  Rather than unconditionally
	// copying data, if the new allocation we got back from realloc isn't properly aligned:
	// 1) Create new properly aligned allocation
	// 2) Copy original data to aligned allocation
	// 3) Free and replace unaligned allocation
	void* ret = realloc(memblock, size);
	if ((size_t(ret) & (std::max<size_t>(alignment, 1) - 1)) != 0)       // MAX handles alignment == 0
	{
		// Should be able to use malloc_usage_size() to copy the correct amount from the original allocation,
		// but it's left undefined in Android NDK.  Thanks...
		//const size_t oldSize = memblock ? _msize(memblock) : 0;
		const size_t oldSize = size;
		// memalign is deprecated but not all platforms have aligned_alloc()...
		void* alignedAlloc = memalign(alignment, size);
		if (alignedAlloc && oldSize > 0)
		{
			// We copy from the unaligned re-allocation rather than original memblock to ensure we
			// don't read beyond the end of any allocated memory.  Even if oldSize == newSize when
			// malloc_usage_size(memblock) < newSize, we know that malloc_usage_size(ret) >= newSize
			memcpy(alignedAlloc, ret, size);
		}
		free(ret);
		ret = alignedAlloc;
	}
	return ret;
		#else
			#error "Not implemented"
		#endif    // __GNUC__
	#else
	void* ret = realloc(memblock, size);
	#endif
	MEMREPLAY_SCOPE_REALLOC(memblock, ret, size, alignment);
	return ret;
}

CRYMM_INLINE void* CryModuleMemalign(size_t size, size_t alignment)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	#if defined(__GNUC__) && !CRY_PLATFORM_APPLE
	void* ret = memalign(alignment, size);
	#else
	void* ret = malloc(size);
	#endif
	MEMREPLAY_SCOPE_ALLOC(ret, size, alignment);
	return ret;
}

#else //NOT_USE_CRY_MEMORY_MANAGER

/////////////////////////////////////////////////////////////////////////
// Extern declarations,used by overridden new and delete operators.
//////////////////////////////////////////////////////////////////////////
extern "C"
{
	void* CryModuleMalloc(size_t size) throw();
	void* CryModuleRealloc(void* memblock, size_t size) throw();
	void  CryModuleFree(void* ptr) throw();
	void* CryModuleMemalign(size_t size, size_t alignment);
	void* CryModuleReallocAlign(void* memblock, size_t size, size_t alignment);
	void  CryModuleMemalignFree(void*);
	void* CryModuleCalloc(size_t a, size_t b);
}

CRYMM_INLINE void* CryModuleCRTMalloc(size_t s)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CrtMalloc);
	void* ret = CryModuleMalloc(s);
	MEMREPLAY_SCOPE_ALLOC(ret, s, 0);
	return ret;
}

CRYMM_INLINE void* CryModuleCRTRealloc(void* p, size_t s)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CrtMalloc);
	void* ret = CryModuleRealloc(p, s);
	MEMREPLAY_SCOPE_REALLOC(p, ret, s, 0);
	return ret;
}

CRYMM_INLINE void CryModuleCRTFree(void* p)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CrtMalloc);
	CryModuleFree(p);
	MEMREPLAY_SCOPE_FREE(p);
}

	#define malloc  CryModuleCRTMalloc
	#define realloc CryModuleCRTRealloc
	#define free    CryModuleCRTFree
	#define calloc  CryModuleCalloc

#endif //NOT_USE_CRY_MEMORY_MANAGER

CRY_MEM_USAGE_API void CryModuleGetMemoryInfo(CryModuleMemoryInfo* pMemInfo);

#if !defined(NOT_USE_CRY_MEMORY_MANAGER)
	#if defined(_LIB) && !defined(NEW_OVERRIDEN)
void* __cdecl operator new(size_t size);

void* __cdecl operator new[](size_t size);

void __cdecl  operator delete(void* p) CRYMM_THROW;

void __cdecl  operator delete[](void* p) CRYMM_THROW;

		#if CRY_PLATFORM_ORBIS
void* operator new(_CSTD size_t size, const std::nothrow_t& nothrow) CRYMM_THROW;

void  operator delete(void* p, const std::nothrow_t&) CRYMM_THROW;

void  operator delete[](void* p, const std::nothrow_t&) CRYMM_THROW;
		#endif
	#endif // defined(_LIB) && !defined(NEW_OVERRIDEN)
#endif   // !defined(NOT_USE_CRY_MEMORY_MANAGER)

//! Needed for our allocator to avoid deadlock in cleanup.
void*  CryCrtMalloc(size_t size);
size_t CryCrtFree(void* p);

//! Wrapper for _msize on PC.
size_t CryCrtSize(void* p);

#if !defined(NOT_USE_CRY_MEMORY_MANAGER)
	#include "CryMemoryAllocator.h"
#endif

//! These utility functions should be used for allocating objects with specific alignment requirements on the heap.
//! \note On MSVC before 2013, only zero to three argument are supported, because C++11 support is not complete.
#if !defined(_MSC_VER) || _MSC_VER >= 1800

template<typename T, typename ... Args>
inline T* CryAlignedNew(Args&& ... args)
{
	void* pAlignedMemory = CryModuleMemalign(sizeof(T), std::alignment_of<T>::value);
	return new(pAlignedMemory) T(std::forward<Args>(args) ...);
}

#else

template<typename T>
inline T* CryAlignedNew()
{
	void* pAlignedMemory = CryModuleMemalign(sizeof(T), std::alignment_of<T>::value);
	return new(pAlignedMemory) T();
}

template<typename T, typename A1>
inline T* CryAlignedNew(A1&& a1)
{
	void* pAlignedMemory = CryModuleMemalign(sizeof(T), std::alignment_of<T>::value);
	return new(pAlignedMemory) T(std::forward<A1>(a1));
}

template<typename T, typename A1, typename A2>
inline T* CryAlignedNew(A1&& a1, A2&& a2)
{
	void* pAlignedMemory = CryModuleMemalign(sizeof(T), std::alignment_of<T>::value);
	return new(pAlignedMemory) T(std::forward<A1>(a1), std::forward<A2>(a2));
}

template<typename T, typename A1, typename A2, typename A3>
inline T* CryAlignedNew(A1&& a1, A2&& a2, A3&& a3)
{
	void* pAlignedMemory = CryModuleMemalign(sizeof(T), std::alignment_of<T>::value);
	return new(pAlignedMemory) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
}

#endif

//! This utility function should be used for allocating arrays of objects with specific alignment requirements on the heap.
//! \note The caller must remember the number of items in the array, since CryAlignedDeleteArray needs this information.
template<typename T>
inline T* CryAlignedNewArray(size_t count)
{
	T* const pAlignedMemory = reinterpret_cast<T*>(CryModuleMemalign(sizeof(T) * count, std::alignment_of<T>::value));
	T* pCurrentItem = pAlignedMemory;
	for (size_t i = 0; i < count; ++i, ++pCurrentItem)
	{
		new(static_cast<void*>(pCurrentItem))T();
	}
	return pAlignedMemory;
}

//! Utility function that frees an object previously allocated with CryAlignedNew.
template<typename T>
inline void CryAlignedDelete(T* pObject)
{
	if (pObject)
	{
		pObject->~T();
		CryModuleMemalignFree(pObject);
	}
}

//! Utility function that frees an array of objects previously allocated with CryAlignedNewArray.
//! The same count used to allocate the array must be passed to this function.
template<typename T>
inline void CryAlignedDeleteArray(T* pObject, size_t count)
{
	if (pObject)
	{
		for (size_t i = 0; i < count; ++i)
		{
			(pObject + i)->~T();
		}
		CryModuleMemalignFree(pObject);
	}
}

#endif // __CryMemoryManager_h__
