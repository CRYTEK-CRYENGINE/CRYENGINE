// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef SHADERALLOCATOR_H
#define SHADERALLOCATOR_H

#include <CryMemory/BucketAllocator.h>
#include <CryMemory/CryMemoryAllocator.h>
#include <CryMemory/IMemory.h>
#include <CryCore/StlUtils.h>

typedef cry_crt_node_allocator ShaderBucketAllocator;

extern ShaderBucketAllocator g_shaderBucketAllocator;
extern IGeneralMemoryHeap* g_shaderGeneralHeap;

template<class T>
class STLShaderAllocator : public stl::SAllocatorConstruct
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
		typedef STLShaderAllocator<U> other;
	};

	STLShaderAllocator() throw() {}
	STLShaderAllocator(const STLShaderAllocator&) throw() {}
	template<class U> STLShaderAllocator(const STLShaderAllocator<U>&) throw() {}

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
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_STL);

		pointer ret = NULL;

		(void)hint;
		size_t sz = std::max<size_type>(n * sizeof(T), 1);
		if (sz <= ShaderBucketAllocator::MaxSize)
		{
			ret = static_cast<pointer>(g_shaderBucketAllocator.allocate(sz));
		}
		else
		{
			ret = static_cast<pointer>(g_shaderGeneralHeap->Malloc(sz, NULL));
		}

		MEMREPLAY_SCOPE_ALLOC(ret, n * sizeof(T), 0);

		return ret;
	}

	void deallocate(pointer p, size_type n = 1)
	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_STL);

		(void)n;
		if (p)
		{
			if (!g_shaderGeneralHeap->Free(p))
				g_shaderBucketAllocator.deallocate(p);
		}

		MEMREPLAY_SCOPE_FREE(p);
	}

	size_type max_size() const throw()
	{
		return INT_MAX;
	}

	void destroy(pointer p)
	{
		p->~T();
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

	bool        operator==(const STLShaderAllocator&) { return true; }
	bool        operator!=(const STLShaderAllocator&) { return false; }

	static void GetMemoryUsage(ICrySizer* pSizer)     {}
};

#endif
