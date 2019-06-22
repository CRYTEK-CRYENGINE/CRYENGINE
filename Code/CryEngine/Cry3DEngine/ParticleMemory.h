// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/PoolAllocator.h>
#include <CryThreading/MultiThread_Containers.h>

struct ParticleAllocator
{
	typedef stl::HeapAllocator<stl::PSyncMultiThread> THeap;
	typedef typename stl::SharedSizePoolAllocator<THeap> TPool;
	typedef typename THeap::Lock TLock;
	typedef CryMT::vector<TPool*> TPoolsList;
	
	ILINE static THeap& Heap()
	{
		static THeap s_Allocator(stl::FHeap().PageSize(0x10000).FreeWhenEmpty(true));
		return s_Allocator;
	}

	template<class T>
	ILINE static TPool& TypeAllocator()
	{
		static CParticlePool s_Pool(Heap(), sizeof(T), alignof(T));
		return s_Pool;
	}

	template<class T>
	ILINE static void* Allocate(T*& p)
	{
		return p = (T*)TypeAllocator<T>().Allocate();
	}

	template<class T>
	ILINE static void Deallocate(T* p)
	{
		return TypeAllocator<T>().Deallocate(p);
	}

	template<class T>
	ILINE static void Deallocate(const TLock& lock, T* p)
	{
		return TypeAllocator<T>().Deallocate(lock, p);
	}

	ILINE static TLock Lock()
	{
		return TLock(Heap());
	}

	template<class T>
	ILINE static void* Allocate(T*&p, size_t count)
	{
		return p = (T*)Heap().Allocate(Lock(), sizeof(T) * count, alignof(T));
	}

	template<class T>
	ILINE static bool Deallocate(T* p, size_t count)
	{
		return !p || Heap().Deallocate(Lock(), p, sizeof(T) * count, alignof(T));
	}

	static bool FreeMemory()
	{
		THeap::FreeMemLock lock(Heap());
		typename TPoolsList::AutoLock listLock(s_pools.get_lock());

		for (auto i = 0; i < s_pools.size(); ++i)
		{
			TPool* pPool = s_pools[i];
			if (pPool->GetTotalMemory(lock).nUsed)
				return false;
		}

		for (auto i = 0; i < s_pools.size(); ++i)
		{
			TPool* pPool = s_pools[i];
			pPool->Reset(lock);
		}
		
		Heap().Clear(lock);
		return true;
	}

	static stl::SPoolMemoryUsage GetTotalMemory()
	{
		auto lock(Lock());
		typename TPoolsList::AutoLock listLock(s_pools.get_lock());
		stl::SMemoryUsage mem;

		for (auto i = 0; i < s_pools.size(); ++i)
		{
			TPool* pPool = s_pools[i];
			mem += pPool->GetTotalMemory(lock);
		}
		return stl::SPoolMemoryUsage(Heap().GetTotalMemory(lock).nAlloc, mem.nAlloc, mem.nUsed);
	}

private:
	static TPoolsList s_pools;

	class CParticlePool : public TPool
	{
	public:
		CParticlePool(THeap& heap, size_t nSize, size_t nAlign = 0)
			: TPool(heap, nSize, nAlign)
		{
			s_pools.push_back(this);
		}
		~CParticlePool()
		{
			CRY_VERIFY(s_pools.try_remove(this));
		}
	};
};
