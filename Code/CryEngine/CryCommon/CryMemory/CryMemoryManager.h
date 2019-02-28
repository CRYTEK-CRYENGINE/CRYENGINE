// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
#include <algorithm>
#include <cstddef>
#include "../CryCore/Assert/CryAssert.h"

#if !defined(_RELEASE)
//! Enable this to check for heap corruption on windows systems by walking the crt.
	#define MEMORY_ENABLE_DEBUG_HEAP 0
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
// Type to allow users of the allocation-defines to specialize on these
// types of memory allocation.
// Additionally it prevents the pointer from being manipulated or accessed
// in a unintended fashion: ++,--,any manipulation of the pointer value etc.
// Members are public, and intended hacking is still possible.

template<class T>
struct SHeapAllocation
{
	size_t size;
	T*     address;
	operator T*() const { return address; }
	T* operator->() const { return address; }
};

// Workaround for: "template<class T> using stackmemory_t = memory_t<T>;"
template<class T>
struct SStackAllocation
{
	size_t size;
	T*     address;
	operator T*() const { return address; }
	T* operator->() const { return address; }

	operator SHeapAllocation<T> &() const { return *reinterpret_cast<SHeapAllocation<T>*>(this); }
};

//////////////////////////////////////////////////////////////////////////
// Some compilers/platforms do not guarantee any alignment for alloca, although standard says it should be sufficient for built-in types.
// Experimental results show that it's possible to get entirely unaligned results (ie, LSB of address set).
#define ALLOCA_ALIGN 1U

// Fire an assert when the allocation is large enough to risk a stack overflow
#define ALLOCA_LIMIT (128U * 1024)

// Needs to be a define, because _alloca() frees it's memory when going out of scope.
#define CryStackAllocVector(Type, Count, Alignment)                                               \
  (Type*)(((uintptr_t)alloca(((Count) * sizeof(Type) + (Alignment - 1)) & ~(Alignment - 1))))

#define CryStackAllocAlignedOffs(AlignmentFunc)                                                   \
  (AlignmentFunc(1) > ALLOCA_ALIGN ? AlignmentFunc(1) - ALLOCA_ALIGN : 0)
#define CryStackAllocAlignedPtr(Type, Size, Offset, AlignmentFunc)                                \
  (Type*)AlignmentFunc((uintptr_t)alloca((Size) + (Offset)))

#define CryStackAllocWithSize(Type, Name, AlignmentFunc)                                          \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type));                                        \
  const size_t Name ## Offset = CryStackAllocAlignedOffs(AlignmentFunc);                          \
  assert(Name ## Size + Name ## Offset <= ALLOCA_LIMIT);                                          \
  Type* Name ## Mem = CryStackAllocAlignedPtr(Type, Name ## Size, Name ## Offset, AlignmentFunc); \
  const SStackAllocation<Type> Name = { Name ## Size, Name ## Mem };

#define CryStackAllocWithSizeCleared(Type, Name, AlignmentFunc)                                   \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type));                                        \
  const size_t Name ## Offset = CryStackAllocAlignedOffs(AlignmentFunc);                          \
  assert(Name ## Size + Name ## Offset <= ALLOCA_LIMIT);                                          \
  Type* Name ## Mem = CryStackAllocAlignedPtr(Type, Name ## Size, Name ## Offset, AlignmentFunc); \
  const SStackAllocation<Type> Name = { Name ## Size, Name ## Mem };                              \
  ZeroMemory(Name ## Mem, Name ## Size);

#define CryStackAllocWithSizeVector(Type, Count, Name, AlignmentFunc)                             \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type) * (Count));                              \
  const size_t Name ## Offset = CryStackAllocAlignedOffs(AlignmentFunc);                          \
  assert(Name ## Size + Name ## Offset <= ALLOCA_LIMIT);                                          \
  Type* Name ## Mem = CryStackAllocAlignedPtr(Type, Name ## Size, Name ## Offset, AlignmentFunc); \
  const SStackAllocation<Type> Name = { Name ## Size, Name ## Mem };

#define CryStackAllocWithSizeVectorCleared(Type, Count, Name, AlignmentFunc)                      \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type) * (Count));                              \
  const size_t Name ## Offset = CryStackAllocAlignedOffs(AlignmentFunc);                          \
  assert(Name ## Size + Name ## Offset <= ALLOCA_LIMIT);                                          \
  Type* Name ## Mem = CryStackAllocAlignedPtr(Type, Name ## Size, Name ## Offset, AlignmentFunc); \
  const SStackAllocation<Type> Name = { Name ## Size, Name ## Mem };                              \
  ZeroMemory(Name ## Mem, Name ## Size);

#include <memory>

namespace CryStack
{

// A stl compliant stack allocator that uses the stack frame.
// It can only allocate a single time the full memory footprint
// of the stl container it's used for.
template<class T>
class CSingleBlockAllocator
{
public:
	typedef T                 value_type;
	typedef value_type*       pointer;
	typedef const value_type* const_pointer;
	typedef value_type&       reference;
	typedef const value_type& const_reference;
	typedef size_t            size_type;
	typedef ptrdiff_t         difference_type;
	typedef std::allocator<T> fallback_allocator_type;

	template<class value_type1> struct rebind
	{
		typedef CSingleBlockAllocator<value_type1> other;
	};

	CSingleBlockAllocator(const SStackAllocation<T>& stack_allocation)
	{
		assert(stack_allocation.address != nullptr);
		this->stack_capacity = stack_allocation.size;
		this->stack_memory = stack_allocation.address;
	}

	CSingleBlockAllocator(size_t stack_capacity, void* stack_memory)
	{
		assert(stack_memory != nullptr);
		this->stack_capacity = stack_capacity;
		this->stack_memory = reinterpret_cast<value_type*>(stack_memory);
	}

	CSingleBlockAllocator(const CSingleBlockAllocator<value_type>& other)
	{
		stack_capacity = other.stack_capacity;
		stack_memory = other.stack_memory;
	}

	CSingleBlockAllocator(const CSingleBlockAllocator<value_type>&& other)
	{
		stack_capacity = other.stack_capacity;
		stack_memory = other.stack_memory;
	}

	~CSingleBlockAllocator()
	{
	}

	// in visual studio when in debug configuration,
	// stl containers create a proxy allocator and call allocate(1) once during construction
	// this copy constructor is needed to cope with that behavior, and will use a standard
	// allocator in case this occurs
	template<class value_type1> CSingleBlockAllocator(const CSingleBlockAllocator<value_type1>& other)
	{
		stack_capacity = 0;
		stack_memory = nullptr;
	}

	pointer       address(reference x) const       { return &x; }
	const_pointer address(const_reference x) const { return &x; }

	value_type*   allocate(size_type n, const void* hint = 0)
	{
		if (stack_memory == nullptr)
		{
			// allocating something else than the main block
			return fallback_allocator_type().allocate(n, hint);
		}
		if (n != max_size())
		{
			// main block allocation is of the wrong size, this is a performance hazard, but not fatal
			assert(0 && "Only a reserve of the correct size is possible on the stack, falling back to heap memory.");
			return fallback_allocator_type().allocate(n, hint);
		}
		return stack_memory;
	}

	void deallocate(pointer p, size_type n)
	{
		if (stack_memory != p)
			return fallback_allocator_type().deallocate(p, n);
	}

	size_type max_size() const           { return stack_capacity / sizeof(value_type); }

	template<class U, class... Args>
	void construct(U* p, Args&&... args) { new (p) U(std::forward<Args>(args)...); }

	void      destroy(pointer p)         { p->~value_type(); }

	void      cleanup()                  {}

	size_t    get_heap_size()            { return 0; }

	size_t    get_wasted_in_allocation() { return 0; }

	size_t    get_wasted_in_blocks()     { return 0; }

private:
	// in case a standard allocator is used (which should only occur in debug configurations)
	// stack_memory is set to nullptr
	size_t      stack_capacity;
	value_type* stack_memory;
};

// template<template<typename, class> class C, typename T> using CSingleBlockContainer = C<T, CSingleBlockAllocator<T>>;
// template<typename T> using CSingleBlockVector = CSingleBlockContainer<std::vector, T>;
}

#define CryStackAllocatorWithSize(Type, Name, AlignmentFunc)                                      \
  CryStackAllocWithSize(Type, Name ## T, AlignmentFunc);                                          \
  CryStack::CSingleBlockAllocator<Type> Name(Name ## T);

#define CryStackAllocatorWithSizeCleared(Type, Name, AlignmentFunc)                               \
  CryStackAllocWithSizeCleared(Type, Name ## T, AlignmentFunc);                                   \
  CryStack::CSingleBlockAllocator<Type> Name(Name ## T);

#define CryStackAllocatorWithSizeVector(Type, Count, Name, AlignmentFunc)                         \
  CryStackAllocWithSizeVector(Type, Count, Name ## T, AlignmentFunc);                             \
  CryStack::CSingleBlockAllocator<Type> Name(Name ## T);

#define CryStackAllocatorWithSizeVectorCleared(Type, Count, Name, AlignmentFunc)                  \
  CryStackAllocWithSizeVectorCleared(Type, Count, Name ## T, AlignmentFunc);                      \
  CryStack::CSingleBlockAllocator<Type> Name(Name ## T);

//////////////////////////////////////////////////////////////////////////
// variation of CryStackAlloc behaving identical, except memory is taken from heap instead of stack
#define CryScopedMem(Type, Size, Name, AlignmentFunc)                                             \
  struct Name ## SMemScoped {                                                                     \
    Name ## SMemScoped(size_t S) {                                                                \
      Name = reinterpret_cast<Type*>(CryModuleMemalign(S, AlignmentFunc(1))); }                   \
    ~Name ## SMemScoped() {                                                                       \
      if (Name != nullptr) CryModuleMemalignFree(Name); }                                         \
    Type* Name;                                                                                   \
  };                                                                                              \
  Name ## SMemScoped Name ## MemScoped(Size);                                                     \

#define CryScopedAllocWithSize(Type, Name, AlignmentFunc)                                         \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type));                                        \
  CryScopedMem(Type, Name ## Size, Name, AlignmentFunc);                                          \
  Type* Name ## Mem = Name ## MemScoped.Name;                                                     \
  const SHeapAllocation<Type> Name = { Name ## Size, Name ## Mem };

#define CryScopedAllocWithSizeCleared(Type, Name, AlignmentFunc)                                  \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type));                                        \
  CryScopedMem(Type, Name ## Size, Name, AlignmentFunc);                                          \
  Type* Name ## Mem = Name ## MemScoped.Name;                                                     \
  const SHeapAllocation<Type> Name = { Name ## Size, Name ## Mem };                               \
  ZeroMemory(Name ## Mem, Name ## Size);

#define CryScopedAllocWithSizeVector(Type, Count, Name, AlignmentFunc)                            \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type) * (Count));                              \
  CryScopedMem(Type, Name ## Size, Name, AlignmentFunc);                                          \
  Type* Name ## Mem = Name ## MemScoped.Name;                                                     \
  const SHeapAllocation<Type> Name = { Name ## Size, Name ## Mem };

#define CryScopedAllocWithSizeVectorCleared(Type, Count, Name, AlignmentFunc)                     \
  const size_t Name ## Size = AlignmentFunc(sizeof(Type) * (Count));                              \
  CryScopedMem(Type, Name ## Size, Name, AlignmentFunc);                                          \
  Type* Name ## Mem = Name ## MemScoped.Name;                                                     \
  const SHeapAllocation<Type> Name = { Name ## Size, Name ## Mem };                               \
  ZeroMemory(Name ## Mem, Name ## Size);

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

// Allow launcher to export functions as e.g. render DLL is still linked dynamically
#if defined(CRYSYSTEM_EXPORTS) || (defined(_LIB) && defined(_LAUNCHER))
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
CRYMM_INLINE void* CryModuleMalloc(size_t size) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	void* ptr = malloc(size);
	MEMREPLAY_SCOPE_ALLOC(ptr, size, 0);
	return ptr;
}

CRYMM_INLINE void* CryModuleRealloc(void* memblock, size_t size) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	void* ret = realloc(memblock, size);
	MEMREPLAY_SCOPE_REALLOC(memblock, ret, size, 0);
	return ret;
}

CRYMM_INLINE void CryModuleFree(void* memblock) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	free(memblock);
	MEMREPLAY_SCOPE_FREE(memblock);
}

CRYMM_INLINE void CryModuleMemalignFree(void* memblock) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	#if defined(__GNUC__) && !CRY_PLATFORM_APPLE
	free(memblock);
	#else
	_aligned_free(memblock);
	#endif
	MEMREPLAY_SCOPE_FREE(memblock);
}

CRYMM_INLINE void* CryModuleReallocAlign(void* memblock, size_t size, size_t alignment) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
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
	void* ret = _aligned_realloc(memblock, size, alignment);
	#endif
	MEMREPLAY_SCOPE_REALLOC(memblock, ret, size, alignment);
	return ret;
}

CRYMM_INLINE void* CryModuleMemalign(size_t size, size_t alignment) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	#if defined(__GNUC__) && !CRY_PLATFORM_APPLE
	void* ret = memalign(alignment, size);
	#else
	void* ret = _aligned_malloc(size, alignment);
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
	// Allocation set 1, uses system default alignment.
	// Never mix with functions from set 2.
	void* CryModuleMalloc(size_t size) noexcept;
	void* CryModuleRealloc(void* memblock, size_t size) noexcept;
	void  CryModuleFree(void* ptr) noexcept;
	void* CryModuleCalloc(size_t a, size_t b) noexcept;

	// Allocation set 2, uses custom alignment.
	// You may pass alignment 0 if you want system default alignment.
	// Never mix with functions from set 1.
	void* CryModuleMemalign(size_t size, size_t alignment) noexcept;
	void* CryModuleReallocAlign(void* memblock, size_t size, size_t alignment) noexcept;
	void  CryModuleMemalignFree(void*) noexcept;

	// When inspecting a block from set 1, you must pass alignment 0.
	// When inspecting a block from set 2, you must pass in the original alignment value (which could be 0).
	size_t CryModuleMemSize(void* ptr, size_t alignment = 0) noexcept;
}

CRYMM_INLINE void* CryModuleCRTMalloc(size_t s) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CrtMalloc);
	void* ret = CryModuleMalloc(s);
	MEMREPLAY_SCOPE_ALLOC(ret, s, 0);
	return ret;
}

CRYMM_INLINE void* CryModuleCRTRealloc(void* p, size_t s) noexcept
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CrtMalloc);
	void* ret = CryModuleRealloc(p, s);
	MEMREPLAY_SCOPE_REALLOC(p, ret, s, 0);
	return ret;
}

CRYMM_INLINE void CryModuleCRTFree(void* p) noexcept
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
// Note: No need to override placement new, new[], delete, delete[]
PREFAST_SUPPRESS_WARNING(28251)
void* __cdecl operator new(std::size_t size);

PREFAST_SUPPRESS_WARNING(28251)
void* __cdecl operator new(std::size_t size, const std::nothrow_t& nothrow_value) noexcept;

PREFAST_SUPPRESS_WARNING(28251)
void* __cdecl operator new[](std::size_t size);

PREFAST_SUPPRESS_WARNING(28251)
void* __cdecl operator new[](std::size_t size, const std::nothrow_t& nothrow_value) noexcept;


void __cdecl operator delete (void* ptr) noexcept;
void __cdecl operator delete (void* ptr, const std::nothrow_t& nothrow_constant) noexcept;

void __cdecl operator delete[] (void* ptr) noexcept;
void __cdecl operator delete[] (void* ptr, const std::nothrow_t& nothrow_constant) noexcept;
#endif

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
