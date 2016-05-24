// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __STLPOOLALLOCATOR_H__
#define __STLPOOLALLOCATOR_H__

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
// std::list<UserDataType, STLPoolAllocator<UserDataType> > myList;
//---------------------------------------------------------------------------

#include "PoolAllocator.h"
#include <CryCore/MetaUtils.h>
#include <CryCore/StlUtils.h>
#include <stddef.h>
#include <climits>

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
	      typename metautils::select<FreeWhenEmpty, HeapSysAllocator, GlobalHeapSysAllocator>::type>
	    > AllocatorType;

	static AllocatorType* GetOrCreateAllocator()
	{
		if (allocator)
			return allocator;

		allocator = new AllocatorType(S, A, FHeap().FreeWhenEmpty(FreeWhenEmpty));
		return allocator;
	}

	static AllocatorType* allocator;
};

template<class T, class L, size_t A, bool FreeWhenEmpty>
struct STLPoolAllocatorKungFu : public STLPoolAllocatorStatic<sizeof(T), L, A, FreeWhenEmpty, T>
{
};

template<class T, class L = PSyncMultiThread, size_t A = 0, bool FreeWhenEmpty = false>
class STLPoolAllocator : public SAllocatorConstruct
{
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

	STLPoolAllocator() throw()
	{
	}

	STLPoolAllocator(const STLPoolAllocator&) throw()
	{
	}

	template<class U, class M, size_t B, bool FreeWhenEmptyA> STLPoolAllocator(const STLPoolAllocator<U, M, B, FreeWhenEmptyA>&) throw()
	{
	}

	~STLPoolAllocator() throw()
	{
	}

	pointer address(reference x) const
	{
		return &x;
	}

	const_pointer address(const_reference x) const
	{
		return &x;
	}

	pointer allocate(size_type n = 1, const void* hint = 0)
	{
		assert(n == 1);
		typename STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::AllocatorType * allocator = STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::GetOrCreateAllocator();
		return static_cast<T*>(allocator->Allocate());
	}

	void deallocate(pointer p, size_type n = 1)
	{
		assert(n == 1);
		typename STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::AllocatorType * allocator = STLPoolAllocatorKungFu<T, L, A, FreeWhenEmpty>::allocator;
		allocator->Deallocate(p);
	}

	size_type max_size() const throw()
	{
		return INT_MAX;
	}

	template<class U>
	void destroy(U* p)
	{
		p->~U();
	}

	pointer new_pointer()
	{
		return new(allocate())T();
	}

	pointer new_pointer(const T& val)
	{
		return new(allocate())T(val);
	}

	void delete_pointer(pointer p)
	{
		p->~T();
		deallocate(p);
	}

	bool        operator==(const STLPoolAllocator&) const { return true; }
	bool        operator!=(const STLPoolAllocator&) const { return false; }

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

template<size_t S, typename L, size_t A, bool FreeWhenEmpty, typename T>
typename STLPoolAllocatorStatic<S, L, A, FreeWhenEmpty, T>::AllocatorType * STLPoolAllocatorStatic<S, L, A, FreeWhenEmpty, T>::allocator;
}

#endif //__STLPOOLALLOCATOR_H__
