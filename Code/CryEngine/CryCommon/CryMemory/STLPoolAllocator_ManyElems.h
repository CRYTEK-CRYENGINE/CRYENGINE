// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Craig Tiller
//---------------------------------------------------------------------------
#ifndef __STLPOOLALLOCATOR_MANYELEMS_H__
#define __STLPOOLALLOCATOR_MANYELEMS_H__

//---------------------------------------------------------------------------
// STL-compatible interface for the pool allocator (see PoolAllocator.h).
//
// this class acts like STLPoolAllocator, but it is also usable for vectors
// which means that it can be used as a more efficient allocator for many
// implementations of hash_map (typically this uses internally a vector and
// a list with the same allocator)
//---------------------------------------------------------------------------

#include "STLPoolAllocator.h"

namespace stl
{
template<size_t S, typename L, size_t A>
struct STLPoolAllocator_ManyElemsStatic
{
	static PoolAllocator<S, L, A>* allocator;
};

template<typename T, typename L = PSyncMultiThread, size_t LargeAllocationSizeThreshold = 54* sizeof(void*), size_t A = 0>
class STLPoolAllocator_ManyElems : public STLPoolAllocator<T, L, A>
{
	typedef STLPoolAllocator<T, L, A>                         Super;
	typedef PoolAllocator<LargeAllocationSizeThreshold, L, A> LargeAllocator;

public:
	typedef typename Super::pointer   pointer;
	typedef typename Super::size_type size_type;

	template<typename U> struct rebind
	{
		typedef STLPoolAllocator_ManyElems<U, L, LargeAllocationSizeThreshold, A> other;
	};

	STLPoolAllocator_ManyElems() throw()
	{
	}

	template<typename U, typename M, size_t C, size_t B>
	STLPoolAllocator_ManyElems(const STLPoolAllocator_ManyElems<U, M, C, B>&) throw()
	{
	}

	pointer allocate(size_type n = 1, const void* hint = 0)
	{
		if (n == 1)
			return Super::allocate(n, hint);
		else if (n * sizeof(T) <= LargeAllocationSizeThreshold)
		{
			if (!STLPoolAllocator_ManyElemsStatic<LargeAllocationSizeThreshold, L, A>::allocator)
				STLPoolAllocator_ManyElemsStatic<LargeAllocationSizeThreshold, L, A>::allocator = new LargeAllocator();
			return static_cast<T*>(STLPoolAllocator_ManyElemsStatic<LargeAllocationSizeThreshold, L, A>::allocator->Allocate());
		}
		else
		{
			return static_cast<pointer>(CryModuleMalloc(n * sizeof(T)));
		}
	}

	void deallocate(pointer p, size_type n = 1)
	{
		if (n == 1)
			Super::deallocate(p);
		else if (n * sizeof(T) <= LargeAllocationSizeThreshold)
			STLPoolAllocator_ManyElemsStatic<LargeAllocationSizeThreshold, L, A>::allocator->Deallocate(p);
		else
		{
			CryModuleFree(p);
		}
	}
};

template<size_t S, typename L, size_t A>
PoolAllocator<S, L, A>* STLPoolAllocator_ManyElemsStatic<S, L, A>::allocator;
}

#endif //__STLPOOLALLOCATOR_H__
