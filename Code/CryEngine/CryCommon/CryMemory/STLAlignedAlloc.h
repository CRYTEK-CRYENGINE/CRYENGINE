// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <stddef.h>           // Required for size_t and ptrdiff_t and NULL
#include <CryCore/StlUtils.h> // SAllocatorConstruct

namespace stl
{
template<typename T, int AlignSize>
class aligned_alloc : public SAllocatorConstruct
{
public:
	// The following will be the same for virtually all allocators.
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;

	T* address(T& r) const
	{
		return &r;
	}

	const T* address(const T& s) const
	{
		return &s;
	}

	size_t max_size() const
	{
		// The following has been carefully written to be independent of
		// the definition of size_t and to avoid signed/unsigned warnings.
		return (static_cast<size_t>(0) - static_cast<size_t>(1)) / sizeof(T);
	}

	//! The following must be the same for all allocators.
	template<typename U>
	struct rebind
	{
		typedef aligned_alloc<U, AlignSize> other;
	};

	bool operator!=(const aligned_alloc& other) const
	{
		return !(*this == other);
	}

	void destroy(T* const p) const;

	//! Returns true if and only if storage allocated from *this
	//! can be deallocated from other, and vice versa.
	//! Always returns true for stateless allocators.
	bool operator==(const aligned_alloc& other) const
	{
		return true;
	}

	//! Default constructor, copy constructor, rebinding constructor, and destructor.
	//! Empty for stateless allocators.
	aligned_alloc() {}
	aligned_alloc(const aligned_alloc&) {}
	template<typename U>
	aligned_alloc(const aligned_alloc<U, AlignSize>&) {}
	~aligned_alloc() {}

	//! The following will be different for each allocator.
	T* allocate(const size_t n) const
	{
		// The return value of allocate(0) is unspecified.
		// aligned_alloc returns NULL in order to avoid depending
		// on malloc(0)'s implementation-defined behavior
		// (the implementation can define malloc(0) to return NULL,
		// in which case the bad_alloc check below would fire).
		// All allocators can return NULL in this case.
		if (n == 0)
		{
			return NULL;
		}

		// All allocators should contain an integer overflow check.
		// The Standardization Committee recommends that std::length_error
		// be thrown in the case of integer overflow.
		if (n > max_size())
		{
			//throw std::length_error( "aligned_alloc<T>::allocate() - Integer overflow." );
			assert(0);
		}

		// aligned_alloc wraps malloc().
		void* const pv = CryModuleMemalign(n * sizeof(T), AlignSize);

		// Allocators should throw std::bad_alloc in the case of memory allocation failure.
		if (pv == NULL)
		{
			//throw std::bad_alloc();
		}
		return static_cast<T*>(pv);
	}

	void deallocate(T* const p, const size_t n) const
	{
		// aligned_alloc wraps free().
		CryModuleMemalignFree(p);
	}

	//! The following will be the same for all allocators that ignore hints.
	template<typename U>
	T* allocate(const size_t n, const U* /* const hint */) const
	{
		return allocate(n);
	}

	// Allocators are not required to be assignable, so
	// all allocators should have a private unimplemented
	// assignment operator. Note that this will trigger the
	// off-by-default (enabled under /Wall) warning C4626
	// "assignment operator could not be generated because a
	// base class assignment operator is inaccessible" within
	// the STL headers, but that warning is useless.
private:
	aligned_alloc& operator=(const aligned_alloc&);
};

// A compiler bug causes it to believe that p->~T() doesn't reference p.
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4100)// unreferenced formal parameter
#endif

//! The definition of destroy() must be the same for all allocators.
template<typename T, int AlignSize>
void aligned_alloc<T, AlignSize >::destroy(T* const p) const
{
	p->~T();
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

//! Defines aligned vector type.
template<typename T, int AlignSize>
class aligned_vector : public std::vector<T, aligned_alloc<T, AlignSize>>
{
public:
	typedef aligned_alloc<T, AlignSize>  MyAlloc;
	typedef std::vector<T, MyAlloc>      MySuperClass;
	typedef aligned_vector<T, AlignSize> MySelf;
	typedef size_t                       size_type;

	aligned_vector() {}
	explicit aligned_vector(const MyAlloc& _Al) : MySuperClass(_Al) {}
	explicit aligned_vector(size_type _Count) : MySuperClass(_Count) {};
	aligned_vector(size_type _Count, const T& _Val) : MySuperClass(_Count, _Val) {}
	aligned_vector(size_type _Count, const T& _Val, const MyAlloc& _Al) : MySuperClass(_Count, _Val) {}
	aligned_vector(const MySelf& _Right) : MySuperClass(_Right) {};

	template<class _Iter>
	aligned_vector(_Iter _First, _Iter _Last) : MySuperClass(_First, _Last) {};

	template<class _Iter>
	aligned_vector(_Iter _First, _Iter _Last, const MyAlloc& _Al) : MySuperClass(_First, _Last, _Al) {};
};

template<class Vec>
inline size_t size_of_aligned_vector(const Vec& c)
{
	if (!c.empty())
	{
		// Not really correct as not taking alignment into the account
		return c.capacity() * sizeof(typename Vec::value_type);
	}
	return 0;
}
}
