// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GMemorySTLAlloc_H_
#define _GMemorySTLAlloc_H_

#pragma once
#include <CryCore/StlUtils.h>

#ifdef INCLUDE_SCALEFORM_SDK

	#pragma warning(push)
	#pragma warning(disable : 6326)// Potential comparison of a constant with another constant
	#pragma warning(disable : 6011)// Dereferencing NULL pointer
	#include <GMemory.h>
	#include <GRefCount.h>
	#pragma warning(pop)

template<typename T>
class GMemorySTLAlloc : public stl::SAllocatorConstruct
{
public:
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;

	template<typename U>
	struct rebind
	{
		typedef GMemorySTLAlloc<U> other;
	};

	template<typename U> friend class GMemorySTLAlloc;

public:
	GMemorySTLAlloc(GMemoryHeap* pHeap = GMemory::GetGlobalHeap(), unsigned int SID = GStat_Default_Mem) throw()
		: m_pHeap(pHeap)
		, m_SID(SID)
	{
		assert(m_pHeap.GetPtr() != 0);
	}

	GMemorySTLAlloc(const GMemorySTLAlloc& rhs) throw()
		: m_pHeap(rhs.m_pHeap)
		, m_SID(rhs.m_SID)
	{
		assert(m_pHeap.GetPtr() != 0);
	}

	template<typename U>
	GMemorySTLAlloc(const GMemorySTLAlloc<U>& rhs) throw()
		: m_pHeap(rhs.m_pHeap)
		, m_SID(rhs.m_SID)
	{
		assert(m_pHeap.GetPtr() != 0);
	}

	~GMemorySTLAlloc() throw()
	{
	}

	pointer address(reference r) const
	{
		return &r;
	}

	const_pointer address(const_reference c) const
	{
		return &c;
	}

	pointer allocate(size_type n, const void* pHint = 0)
	{
		T* p = (T*) GHEAP_ALLOC(m_pHeap, n * sizeof(T), m_SID);
		return p;
	}

	void deallocate(void* p, size_type n)
	{
		if (p)
			GHEAP_FREE(m_pHeap, p);
	}

	size_type max_size() const
	{
		return ((size_t) -1) / sizeof(T);
	}

	void destroy(pointer p)
	{
		p->~T();
	}

private:
	GMemorySTLAlloc& operator=(const GMemorySTLAlloc&);

private:
	GPtr<GMemoryHeap> m_pHeap;
	unsigned int      m_SID;
};

template<typename T1, typename T2>
bool operator==(const GMemorySTLAlloc<T1>& lhs, const GMemorySTLAlloc<T2>& rhs) throw()
{
	return lhs.m_pHeap == rhs.m_pHeap;
}

template<typename T1, typename T2>
bool operator!=(const GMemorySTLAlloc<T1>& lhs, const GMemorySTLAlloc<T2>& rhs) throw()
{
	return lhs.m_pHeap != rhs.m_pHeap;
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK

#endif // #ifndef _GMemorySTLAlloc_H_
