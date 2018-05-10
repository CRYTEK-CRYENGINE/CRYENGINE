// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//---------------------------------------------------------------------------
// STL-compatible interface for the pool allocator (see PoolAllocator.h).
//
// This class is suitable for use as an allocator for STL lists. Note it will
// not work with vectors, since it allocates fixed-size blocks, while vectors
// allocate elements in variable-sized contiguous chunks.
//
// To create a list of type UserDataType using this allocator, use the
// following syntax:
//
// std::list<UserDataType, STLPoolAllocator<UserDataType>> myList;
//---------------------------------------------------------------------------

#include "PoolAllocator.h"

#include <CryCore/AlignmentTools.h>
#include <CryCore/Assert/CryAssert.h>
#include <CryCore/StlUtils.h>

#include <climits>
#include <cstddef>
#include <type_traits>

namespace stl
{
template<size_t S, class L, size_t A, bool FreeWhenEmpty, typename T>
struct STLPoolAllocatorStatic
{
	//! Non-freeing stl pool allocators should just go on the global heap.
	//! They should go on the default heap only if they've been explicitly set to cleanup.
	typedef SizePoolAllocator<
	    HeapAllocator<
	      L,
	      typename std::conditional<FreeWhenEmpty, HeapSysAllocator, GlobalHeapSysAllocator>::type>
	    > AllocatorType;

	static AllocatorType& GetAllocator()
	{
		//! \note We cannot simply use a static AllocatorType class member
		//! because AllocatorType's constructor is not/cannot be made
		//! constexpr (due to the non-trivial FHeap struct) thus causing
		//! the 'Static Initialization Order Fiasco' on startup if it is
		//! made a global static. We also want to avoid the thread-safe
		//! initializer and at-exit destructor associated with the standard
		//! 'Meyer's singleton' way of implementing it so we use the more
		//! verbose static bool+uninitialized storage implementation.
		static bool constructed = false;
		static typename SAlignedStorage<sizeof(AllocatorType), alignof(AllocatorType)>::type storage;
		IF_LIKELY (constructed)
		{
			return reinterpret_cast<AllocatorType&>(storage);
		}
		constructed = true;
		return *(new(&storage)AllocatorType(S, max(A, alignof(T)), FHeap().FreeWhenEmpty(FreeWhenEmpty)));
	}
};

template<class T, class L, size_t A, bool FreeWhenEmpty>
struct STLPoolAllocatorKungFu : public STLPoolAllocatorStatic<sizeof(T), L, A, FreeWhenEmpty, T>
{
};

template<class T, class L = PSyncMultiThread, size_t A = 0, bool FreeWhenEmpty = false>
class STLPoolAllocator : public SAllocatorConstruct
{
	static_assert(!std::is_const<T>::value, "bad type");

public:
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;

	template<class U> struct rebind
	{
		typedef STLPoolAllocator<U, L, A, FreeWhenEmpty> other;
	};

	STLPoolAllocator() noexcept
	{
	}

	STLPoolAllocator(const STLPoolAllocator&) noexcept
	{
	}

	template<class U, class M, size_t B, bool FreeWhenEmptyA>
	STLPoolAllocator(const STLPoolAllocator<U, M, B, FreeWhenEmptyA>&) noexcept
	{
	}

	pointer address(reference x) const noexcept
	{
		return &x;
	}

	const_pointer address(const_reference x) const noexcept
	{
		return &x;
	}

	pointer allocate(size_type n = 1, const void* hint = 0) noexcept
	{
		CRY_ASSERT(n == 1);
		return static_cast<T*>(STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::GetAllocator().Allocate());
	}

	void deallocate(pointer p, size_type n = 1) noexcept
	{
		CRY_ASSERT(n == 1);
		STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::GetAllocator().Deallocate(p);
	}

	size_type max_size() const noexcept
	{
		return INT_MAX;
	}

	template<class U>
	void destroy(U* p) noexcept
	{
		p->~U();
	}

	pointer new_pointer() noexcept
	{
		return new(allocate())T();
	}

	pointer new_pointer(const T& val) noexcept
	{
		return new(allocate())T(val);
	}

	void delete_pointer(pointer p) noexcept
	{
		p->~T();
		deallocate(p);
	}

	bool        operator==(const STLPoolAllocator&) const noexcept { return true; }
	bool        operator!=(const STLPoolAllocator&) const noexcept { return false; }

	static void GetMemoryUsage(ICrySizer* pSizer)
	{
		pSizer->AddObject(STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::allocator);
	}
};

template<class T, size_t A = 0, bool FreeWhenEmpty = false>
class STLPoolAllocatorNoMT : public STLPoolAllocator<T, PSyncNone, A, FreeWhenEmpty>
{
};

template<> class STLPoolAllocator<void>
{
public:
	typedef void*                               pointer;
	typedef const void*                         const_pointer;
	typedef void                                value_type;
	template<class U, class L>
	struct rebind { typedef STLPoolAllocator<U> other; };
};

} // namespace stl
