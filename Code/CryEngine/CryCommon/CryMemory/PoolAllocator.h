// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Smith
// Modified 2008-06, Scott Peter
//	Refactored to utilise storage management of new HeapAllocator class
//---------------------------------------------------------------------------

#ifndef __POOLALLOCATOR_H__
#define __POOLALLOCATOR_H__

//---------------------------------------------------------------------------
// Memory allocation class. Allocates, frees, and reuses fixed-size blocks of
// memory, a scheme sometimes known as Simple Segregated Memory.
//
// Allocation is amortized constant time. The normal case is very fast -
// basically just a couple of dereferences. If many blocks are allocated,
// the system may occasionally need to allocate a further bucket of blocks
// for itself. Deallocation is strictly fast constant time.
//
// Each PoolAllocator allocates blocks of a single size and alignment, specified
// by template arguments. There is no per-block space overhead, except for
// alignment. The free list mechanism uses the memory of the block itself
// when it is deallocated.
//
// In this implementation memory claimed by the system is never deallocated,
// until the entire allocator is deallocated. This is to ensure fast
// allocation/deallocation - reference counting the bucket quickly would
// require a pointer to the bucket be stored, whereas now no memory is used
// while the block is allocated.
//
// This allocator is suitable for use with STL lists - see STLPoolAllocator
// for an STL-compatible interface.
//
// The class can optionally support multi-threading, using the second
// template parameter. By default it is multithread-safe.
// See Synchronization.h.
//
// The class is implemented using a HeapAllocator.
//---------------------------------------------------------------------------

#include "HeapAllocator.h"

namespace stl
{
//! Fixed-size pool allocator, using a shared heap.
template<typename THeap>
class SharedSizePoolAllocator
{
	template<typename T> friend struct PoolCommonAllocator;
protected:

	using_type(THeap, Lock);

	struct ObjectNode
	{
		ObjectNode* pNext;
	};

	static size_t AllocSize(size_t nSize)
	{
		return max<size_t>(nSize, sizeof(ObjectNode));
	}
	static size_t AllocAlign(size_t nSize, size_t nAlign)
	{
		if (nAlign == 0)
		{
			for (nAlign = 1; nAlign < 16; nAlign <<= 1)
			{
				if (nSize & nAlign)
					break;
			}
		}
		return nAlign;
	}

public:

	SharedSizePoolAllocator(THeap& heap, size_t nSize, size_t nAlign = 0)
		: _pHeap(&heap),
		_nAllocSize(AllocSize(nSize)),
		_nAllocAlign(AllocAlign(nSize, nAlign)),
		_pFreeList(0)
	{
	}

	~SharedSizePoolAllocator()
	{
		// All allocated objects should be freed by now.
		Lock lock(*_pHeap);
		Validate(lock);
		for (ObjectNode* pFree = _pFreeList; pFree; )
		{
			ObjectNode* pNext = pFree->pNext;
			_pHeap->Deallocate(lock, pFree, _nAllocSize, _nAllocAlign);
			pFree = pNext;
		}
	}

	//! Raw allocation.
	void* Allocate()
	{
		Lock lock(*_pHeap);
		if (_pFreeList)
		{
			ObjectNode* pFree = _pFreeList;
			_pFreeList = _pFreeList->pNext;
			Validate(lock);
			_Counts.nUsed++;
			return pFree;
		}

		// No free pointer, allocate a new one.
		void* pNewMemory = _pHeap->Allocate(lock, _nAllocSize, _nAllocAlign);
		if (pNewMemory)
		{
			_Counts.nUsed++;
			_Counts.nAlloc++;
			Validate(lock);
		}
		return pNewMemory;
	}

	void Deallocate(void* pObject)
	{
		Deallocate(Lock(*_pHeap), pObject);
	}

	SMemoryUsage GetCounts() const
	{
		Lock lock(*_pHeap);
		return _Counts;
	}
	SMemoryUsage GetTotalMemory(const Lock&) const
	{
		return SMemoryUsage(_Counts.nAlloc * _nAllocSize, _Counts.nUsed * _nAllocSize);
	}

protected:

	void Deallocate(const Lock& lock, void* pObject)
	{
		if (pObject)
		{
#ifdef _DEBUG
			// This can be slow
			assert(_pHeap->CheckPtr(lock, pObject, _nAllocSize));
#endif

			ObjectNode* pNode = static_cast<ObjectNode*>(pObject);

			// Add the object to the front of the free list.
			pNode->pNext = _pFreeList;
			_pFreeList = pNode;
			_Counts.nUsed--;
			Validate(lock);
		}
	}

	void Validate(const Lock& lock) const
	{
		_pHeap->Validate(lock);
		_Counts.Validate();
		assert(_Counts.nAlloc * _nAllocSize <= _pHeap->GetTotalMemory(lock).nUsed);
	}

	void Reset(const Lock&, bool bForce = false)
	{
		assert(bForce || _Counts.nUsed == 0);
		_Counts.Clear();
		_pFreeList = 0;
	}

protected:
	const size_t _nAllocSize, _nAllocAlign;
	SMemoryUsage _Counts;

	THeap*       _pHeap;
	ObjectNode*  _pFreeList;
};

struct SPoolMemoryUsage : SMemoryUsage
{
	size_t nPool;

	SPoolMemoryUsage(size_t _nAlloc = 0, size_t _nPool = 0, size_t _nUsed = 0)
		: SMemoryUsage(_nAlloc, _nUsed), nPool(_nPool)
	{
		Validate();
	}

	size_t nPoolFree() const
	{
		return nPool - nUsed;
	}
	size_t nNonPoolFree() const
	{
		return nAlloc - nPool;
	}
	void Validate() const
	{
		assert(nUsed <= nPool);
		assert(nPool <= nAlloc);
	}
	void Clear()
	{
		nAlloc = nUsed = nPool = 0;
	}

	void operator+=(SPoolMemoryUsage const& op)
	{
		nAlloc += op.nAlloc;
		nPool += op.nPool;
		nUsed += op.nUsed;
	}
};

//! SizePoolAllocator with owned heap.
template<typename THeap>
class SizePoolAllocator : protected THeap, public SharedSizePoolAllocator<THeap>
{
	typedef SharedSizePoolAllocator<THeap> TPool;

	using_type(THeap, Lock);
	using_type(THeap, FreeMemLock);
	using TPool::AllocSize;
	using TPool::_Counts;
	using TPool::_nAllocSize;

public:

	SizePoolAllocator(size_t nSize, size_t nAlign = 0, FHeap opts = {})
		: THeap(opts.PageSize(opts.PageSize() * AllocSize(nSize))),
		TPool(*this, nSize, nAlign)
	{
	}

	using TPool::Allocate;
	using THeap::GetMemoryUsage;

	void Deallocate(void* pObject)
	{
		FreeMemLock lock(*this);
		TPool::Deallocate(lock, pObject);
		if (THeap::FreeWhenEmpty() && _Counts.nUsed == 0)
		{
			TPool::Reset(lock);
			THeap::Clear(lock);
		}
	}

	void FreeMemoryIfEmpty()
	{
		FreeMemLock lock(*this);
		if (_Counts.nUsed == 0)
		{
			TPool::Reset(lock);
			THeap::Clear(lock);
		}
	}

	void ResetMemory()
	{
		FreeMemLock lock(*this);
		TPool::Reset(lock);
		THeap::Reset(lock);
	}

	void FreeMemory()
	{
		FreeMemLock lock(*this);
		TPool::Reset(lock);
		THeap::Clear(lock);
	}

	void FreeMemoryForce()
	{
		FreeMemLock lock(*this);
		TPool::Reset(lock, true);
		THeap::Clear(lock);
	}

	SPoolMemoryUsage GetTotalMemory()
	{
		Lock lock(*this);
		return SPoolMemoryUsage(THeap::GetTotalMemory(lock).nAlloc, _Counts.nAlloc * _nAllocSize, _Counts.nUsed * _nAllocSize);
	}
};

//! Templated size version of SizePoolAllocator
template<int S, typename L = PSyncMultiThread, int A = 0>
class PoolAllocator : public SizePoolAllocator<HeapAllocator<L>>
{
public:
	PoolAllocator(FHeap opts = {})
		: SizePoolAllocator<HeapAllocator<L>>(S, A, opts)
	{
	}
};

template<int S, int A = 0>
class PoolAllocatorNoMT : public SizePoolAllocator<HeapAllocator<PSyncNone>>
{
public:
	PoolAllocatorNoMT(FHeap opts = {})
		: SizePoolAllocator<HeapAllocator<PSyncNone>>(S, A, opts)
	{
	}
};

template<typename T, typename L = PSyncMultiThread, size_t A = 0>
class TPoolAllocator : public SizePoolAllocator<HeapAllocator<L>>
{
	typedef SizePoolAllocator<HeapAllocator<L>> TSizePool;

public:

	using TSizePool::Allocate;
	using TSizePool::Deallocate;

	TPoolAllocator(FHeap opts = {})
		: TSizePool(sizeof(T), max<size_t>(alignof(T), A), opts)
	{}

	T* New()
	{
		return ::new(Allocate())T();
	}

	template<class I>
	T* New(const I& init)
	{
		return ::new(Allocate())T(init);
	}

	void Delete(T* ptr)
	{
		if (ptr)
		{
			ptr->~T();
			Deallocate(ptr);
		}
	}
};

typedef PSyncNone        PoolAllocatorSynchronizationSinglethreaded;    //!< Legacy verbose typedef.
typedef PSyncMultiThread PoolAllocatorSynchronizationMultithreaded;     //!< Legacy verbose typedef.

//! Allocator maintaining multiple type-specific pools, sharing a common heap source.
template<typename THeap>
struct PoolCommonAllocator : protected THeap
{
	typedef SharedSizePoolAllocator<THeap> TPool;

	using_type(THeap, Lock);
	using_type(THeap, FreeMemLock);

	struct TPoolNode : SharedSizePoolAllocator<THeap>
	{
		TPoolNode* pNext;

		TPoolNode(THeap& heap, TPoolNode*& pList, size_t nSize, size_t nAlign)
			: SharedSizePoolAllocator<THeap>(heap, nSize, nAlign)
		{
			pNext = pList;
			pList = this;
		}
	};

public:

	PoolCommonAllocator()
		: _pPoolList(0)
	{
	}
	~PoolCommonAllocator()
	{
		TPoolNode* pPool = _pPoolList;
		while (pPool)
		{
			TPoolNode* pNextPool = pPool->pNext;
			delete pPool;
			pPool = pNextPool;
		}
	}

	TPool* CreatePool(size_t nSize, size_t nAlign = 0)
	{
		return new TPoolNode(*this, _pPoolList, nSize, nAlign);
	}

	SPoolMemoryUsage GetTotalMemory()
	{
		Lock lock(*this);
		SMemoryUsage mem;
		for (TPoolNode* pPool = _pPoolList; pPool; pPool = pPool->pNext)
			mem += pPool->GetTotalMemory(lock);
		return SPoolMemoryUsage(THeap::GetTotalMemory(lock).nAlloc, mem.nAlloc, mem.nUsed);
	}

	bool FreeMemory(bool bDeallocate = true)
	{
		FreeMemLock lock(*this);
		for (TPoolNode* pPool = _pPoolList; pPool; pPool = pPool->pNext)
			if (pPool->GetTotalMemory(lock).nUsed)
				return false;

		for (TPoolNode* pPool = _pPoolList; pPool; pPool = pPool->pNext)
			pPool->Reset(lock);

		if (bDeallocate)
			THeap::Clear(lock);
		else
			THeap::Reset(lock);
		return true;
	}

protected:
	TPoolNode* _pPoolList;
};

//! The additional TInstancer type provides a way of instantiating multiple instances of this class, without static variables.
template<typename THeap, typename TInstancer = int>
struct StaticPoolCommonAllocator
{
	ILINE static PoolCommonAllocator<THeap>& StaticAllocator()
	{
		static PoolCommonAllocator<THeap> s_Allocator;
		return s_Allocator;
	}

	typedef SharedSizePoolAllocator<THeap> TPool;

	template<class T>
	ILINE static TPool& TypeAllocator()
	{
		static TPool* sp_Pool = CreatePoolOnGlobalHeap(sizeof(T), alignof(T));
		return *sp_Pool;
	}

	template<class T>
	ILINE static void* Allocate(T*& p)
	{ return p = (T*)TypeAllocator<T>().Allocate(); }

	template<class T>
	ILINE static void Deallocate(T* p)
	{ return TypeAllocator<T>().Deallocate(p); }

	template<class T>
	static T* New()
	{ return new(TypeAllocator<T>().Allocate())T(); }

	template<class T, class I>
	static T* New(const I& init)
	{ return new(TypeAllocator<T>().Allocate())T(init); }

	template<class T>
	static void Delete(T* ptr)
	{
		if (ptr)
		{
			ptr->~T();
			TypeAllocator<T>().Deallocate(ptr);
		}
	}

	static SPoolMemoryUsage GetTotalMemory()
	{ return StaticAllocator().GetTotalMemory(); }

private:

	ILINE static TPool* CreatePoolOnGlobalHeap(size_t nSize, size_t nAlign = 0)
	{
		return StaticAllocator().CreatePool(nSize, nAlign);
	}
};
};

#endif //__POOLALLOCATOR_H__
