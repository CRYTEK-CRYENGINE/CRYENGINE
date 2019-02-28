// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: J Scott Peter
//---------------------------------------------------------------------------

// Include guard needed to selectively disable this file for conflicting 3rd-party includes
#ifndef _CRY_ARRAY_H_
#define _CRY_ARRAY_H_

#pragma once

#include <utility>
#include <type_traits>
#include <CryMemory/IGeneralMemoryHeap.h> // <> required for Interfuscator

//---------------------------------------------------------------------------
//! Stack array helper
#define ALIGNED_STACK_ARRAY(T, name, size, alignment)         \
  PREFAST_SUPPRESS_WARNING(6255)                              \
  T * name = (T*) alloca((size) * sizeof(T) + alignment - 1); \
  name = Align(name, alignment);

#define STACK_ARRAY(T, name, size)               \
  ALIGNED_STACK_ARRAY(T, name, size, alignof(T)) \

#if defined(_DEBUG)
#	define debug_assert(cond) assert(cond)
#else
#	define debug_assert(cond)
#endif

//---------------------------------------------------------------------------
//! Specify semantics for moving objects.
//! If raw_movable() is true, objects will be moved with memmove().
//! If false, with the templated move_init() function.
template<class T>
bool raw_movable(T const&)
{
#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
	return std::is_trivially_move_constructible<T>();
#else
	return false;
#endif
}

//! Generic move function: transfer an existing source object to uninitialised dest address.
//! Addresses must not overlap (requirement on caller).
//! May be specialised for specific types, to provide a more optimal move.
//! For types that can be trivially moved (memcpy), do not specialise move_init, rather specialise raw_movable to return true.
template<class T>
void move_init(T& dest, T& source)
{
	debug_assert(&dest != &source);
	new(&dest) T(std::move(source));
	source.~T();
}

//! Special enums used for custom initialization
enum EDefaultInit { eDefaultInit };
enum EInit { eNoInit, eZeroInit };

/*---------------------------------------------------------------------------
   Public classes:

    Array<T, [I, STORAGE]>
    StaticArray<T, nSIZE, [I]>
    DynArray<T, [I, STORAGE]>
		FixedDynArray<T, [I]>
		StaticDynArray<T, nSIZE, [I]>
		SmallDynArray<T, [I, ALLOC]>
    FastDynArray<T, [I, ALLOC]>
		LocalDynArray<T, nSIZE, [I, ALLOC]>

   Support classes are placed in namespaces NArray and NAlloc to reduce global name usage.
   ---------------------------------------------------------------------------*/

namespace NArray
{
/*---------------------------------------------------------------------------
// STORAGE prototype for Array<T,I,STORAGE>.
struct Storage
{
	template<class T, class I>
	struct Store
	{
		[const] T* begin() [const];
		[const] T* end() [const];
		I size() const;
	};
};
---------------------------------------------------------------------------*/

//---------------------------------------------------------------------------
//! Storage: Default STORAGE Array<T,I,STORAGE>.
//! Simply contains a pointer and count to an existing array,
//! performs no allocation or deallocation.

struct Storage
{
	template<class T, class I = int>
	struct Store
	{
		static const I npos = std::numeric_limits<I>::max();

		// Construction.
		Store()
			: m_aElems(0), m_nCount(0)
		{}
		Store(T* elems, I count)
			: m_aElems(elems), m_nCount(count)
		{
			debug_assert(IsAligned(m_aElems, alignof(T)));
		}
		Store(T* start, T* finish)
			: m_aElems(start), m_nCount(check_cast<I>(finish - start))
		{
			debug_assert(IsAligned(m_aElems, alignof(T)));
		}
		template<size_t N>
		Store(T (&ar)[N])
			: m_aElems(ar), m_nCount(check_cast<I>(N))
		{
			debug_assert(IsAligned(m_aElems, alignof(T)));
		}

		void set(T* elems, I count)
		{
			m_aElems = elems;
			m_nCount = count;
			debug_assert(IsAligned(m_aElems, alignof(T)));
		}

		//! Basic storage.
		CONST_VAR_FUNCTION(T * begin(), { return m_aElems; })
		CONST_VAR_FUNCTION(T * end(), { return m_aElems + m_nCount; })
		ILINE I size() const { return m_nCount; }

		//! Modifiers, alter range in place.
		void erase_front(I count = 1)
		{
			debug_assert(count >= 0 && count <= m_nCount);
			m_nCount -= count;
			m_aElems += count;
		}

		void erase_back(I count = 1)
		{
			debug_assert(count >= 0 && count <= m_nCount);
			m_nCount -= count;
		}

		void resize(I count)
		{
			debug_assert(count >= 0 && count <= m_nCount);
			m_nCount = count;
		}

	protected:
		T* m_aElems;
		I  m_nCount;
	};
};

//---------------------------------------------------------------------------
//! StaticStorage: STORAGE scheme with a statically sized member array.
template<int nSIZE>
struct StaticStorage
{
	template<class T, class I = int>
	struct Store
	{
		//! Basic storage.
		CONST_VAR_FUNCTION(T * begin(), { return m_aElems; })
		CONST_VAR_FUNCTION(T * end(), { return m_aElems + nSIZE; })
		ILINE static I size() { return (I)nSIZE; }

	protected:
		T m_aElems[nSIZE];
	};
};
} // NArray

//---------------------------------------------------------------------------
//! Array<T,I,S>: Non-growing array.
//! S serves as base class, and implements storage scheme: begin(), end(), size().
template<class T, class I = int, class STORE = NArray::Storage>
struct Array : STORE::template Store<T, I>
{
	typedef typename STORE::template Store<T, I> S;

	// Tedious redundancy.
	using S::size;
	using S::begin;
	using S::end;

	// STL-compatible typedefs.
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;

	typedef T*       iterator;
	typedef const T* const_iterator;

	typedef I        size_type;
	typedef typename std::make_signed<I>::type
	  difference_type;

	typedef Array<T, I>       array;
	typedef Array<const T, I> const_array;

	static const I npos = std::numeric_limits<I>::max();

	// Forward storage constructors.
	using S::S;

	// Accessors.
	ILINE bool empty() const    { return size() == 0; }
	ILINE I    size_mem() const { return size() * sizeof(T); }

	CONST_VAR_FUNCTION(T * data(), { return begin(); })
	CONST_VAR_FUNCTION(T * rbegin(), { return end() - 1; })
	CONST_VAR_FUNCTION(T * rend(), { return begin() - 1; })

	CONST_VAR_FUNCTION(T & front(),
	{
		debug_assert(!empty());
		return *begin();
	})
	CONST_VAR_FUNCTION(T & back(),
	{
		debug_assert(!empty());
		return *rbegin();
	})

	CONST_VAR_FUNCTION(T & at(I i),
	{
		debug_assert(i >= 0 && i < size());
		return begin()[i];
	})

	CONST_VAR_FUNCTION(T & operator[](I i),
	{
		debug_assert(i >= 0 && i < size());
		return begin()[i];
	})

	//! Conversion to canonical array type.
	ILINE operator const_array() const { return const_array(begin(), size()); }
	ILINE operator array()  { return array(begin(), size()); }

	//! Additional conversion via operator() to full or sub array.
	ILINE const_array operator()(I pos = 0, I count = npos) const
	{
		debug_assert(pos >= 0 && pos <= size());
		count = std::min(count, size() - pos);
		return const_array(begin() + pos, count);
	}
	ILINE array operator()(I pos = 0, I count = npos)
	{
		debug_assert(pos >= 0 && pos <= size());
		count = std::min(count, size() - pos);
		return array(begin() + pos, count);
	}

	// Basic element assignment functions.

	//! Copy values to existing elements.
	void copy(I pos, EDefaultInit)
	{
		at(pos) = T();
	}
	void copy(I pos, EInit e)
	{
		copy(pos, 1, e);
	}
	void copy(I pos, const T& val)
	{
		at(pos) = val;
	}
	void copy(I pos, T&& val)
	{
		at(pos) = std::move(val);
	}

	void copy(I pos, I count, EDefaultInit)
	{
		for (auto& e : (*this)(pos, count))
			e = T();
	}
	void copy(I pos, I count, EInit e)
	{
		destroy(pos, count);
		if (e == eZeroInit)
			memset(&at(pos), 0, count * sizeof(T));
	}
	void copy(I pos, I count, const T& val)
	{
		for (auto& e : (*this)(pos, count))
			e = val;
	}
	void copy(I pos, I count, T&& val)
	{
		if (count)
		{
			at(pos) = std::move(val);
			copy(pos + 1, count - 1, at(pos));
		}
	}
	void copy(I pos, const_iterator p, I count)
	{
		for (auto& e : (*this)(pos, count))
			e = *p++;
	}
	void copy(I pos, const_iterator p, const_iterator e)
	{
		copy(pos, p, check_cast<I>(e - p));
	}
	void copy(I pos, const_array array)
	{
		copy(pos, array.begin(), array.size());
	}

	void fill(const T& val)
	{
		copy(0, size(), val);
	}

	//! Initialize raw elements.
	void init(I pos, EDefaultInit)
	{
		new(&at(pos)) T();
	}
	void init(I pos, EInit e)
	{
		if (e == eZeroInit)
			memset(&at(pos), 0, sizeof(T));
	}
	void init(I pos, const T& val)
	{
		new(&at(pos)) T(val);
	}
	void init(I pos, T&& val)
	{
		new(&at(pos)) T(std::move(val));
	}
	void init(I pos, I count, EDefaultInit)
	{
		for (auto& e : (*this)(pos, count))
			new(&e) T();
	}
	void init(I pos, I count, EInit e)
	{
		if (e == eZeroInit)
			memset(&at(pos), 0, count * sizeof(T));
	}
	void init(I pos, I count, const T& val)
	{
		for (auto& e : (*this)(pos, count))
			new(&e) T(val);
	}
	void init(I pos, I count, T&& val)
	{
		if (count)
		{
			new(&at(pos)) T(std::move(val));
			init(pos + 1, count - 1, at(pos));
		}
	}
	void init(I pos, const_iterator p, I count)
	{
		for (auto& e : (*this)(pos, count))
			new(&e) T(*p++);
	}
	void init(I pos, const_iterator p, const_iterator e)
	{
		init(pos, p, check_cast<I>(e - p));
	}
	void init(I pos, const_array array)
	{
		init(pos, array.begin(), array.size());
	}

	//! Move from one array location to another
	void move_init(I pos, array source)
	{
		assert(pos + source.size() <= size());
		iterator s = source.begin();
		iterator d = begin() + pos;
		if (!source.empty() && s != d)
		{
			if (raw_movable(*s))
			{
				memmove(d, s, source.size_mem());
			}
			else if (s > d || s + source.size() <= d)
			{
				for (auto& e : (*this)(pos, source.size()))
					::move_init(e, *s++);
			}
			else
			{
				s = source.end();
				for (iterator it = d + source.size(); it > d; )
					::move_init(*--it, *--s);
			}
		}
	}

	void destroy()
	{
		// Destroy in reverse order, to complement construction order.
		for (iterator it = end(); it-- > begin(); )
			it->~T();
	}
	void destroy(I pos, I count = npos)
	{
		(*this)(pos, count).destroy();
	}
};

// Type-inferring Array constructors (will not be needed with C++17)

template<class T>
ILINE Array<T> ArrayT(T* elems, int count)
{
	return Array<T>(elems, count);
}

template<class T>
ILINE Array<T> ArrayT(T* start, T* finish)
{
	return Array<T>(start, finish);
}

template<class T>
ILINE Array<T> ArrayT(T& obj)
{
	return Array<T>(&obj, 1);
}

template<class T, int N>
ILINE Array<T> ArrayT(T (&start)[N])
{
	return Array<T>(start, N);
}

//---------------------------------------------------------------------------
// StaticArray<T,I,nSIZE>
// A superior alternative to static C arrays.
// Provides standard STL-like Array interface, including bounds-checking.
//		standard:		Type array[256];
//		structured:	StaticArray<Type,256> array;

template<class T, int nSIZE, class I = int>
using StaticArray = Array<T, I, NArray::StaticStorage<nSIZE>>;

/*--------------------------------------------------------------------------
// STORAGE prototype for DynArray<T,I,STORAGE>:
// Extends Storage with resizing functionality.
struct DynStorage
{
	struct Store<T, I> : Storage<T, I>::Store
	{
		I      capacity() const;
		I      max_size() const;
		size_t get_alloc_size() const;

	protected:
		// Resizing function pair
		bool reallocate(Array<T,I>& old, I new_size, bool allow_slack) -> true if reallocated
		void dealloc(Array<T,I>); // deallocate old array

		// Usage by DynArray client:
		//		array old; if (S::reallocate(old, new_size, allow_slack))
		//		{
		//			...; // move existing data
		//			S::deallocate(old);
		//		}
	};
};
--------------------------------------------------------------------------*/

// Non-allocating DynStorage implementations
namespace NArray
{

//---------------------------------------------------------------------------
//! FixedDynStorage: uses a pre-allocated range of elements
struct FixedDynStorage
{
	template<class T, class I = int>
	struct Store : Storage::Store<T, I>
	{
		typedef Storage::Store<T, I> B;
		typedef Array<T, I>          array;

		I             capacity() const { return m_nCapacity; }
		I             max_size() const { return m_nCapacity; }
		static size_t get_alloc_size() { return 0; }

		Store()
			: m_nCapacity(0) {}

		Store(T* elems, I count, I cap)
			: B(elems, count), m_nCapacity(cap) {}

		void set(void* elems, I mem_size)
		{
			m_aElems = (T*)elems;
			m_nCapacity = mem_size / sizeof(T);
			m_nCount = 0;
		}
		void set(array a)
		{
			m_aElems = a.begin();
			m_nCapacity = a.size();
			m_nCount = 0;
		}

	protected:

		// Resize functions; no allocation, just set size
		ILINE bool reallocate(array&, I new_size, bool allow_slack = true)
		{
			debug_assert(new_size >= 0 && new_size <= capacity());
			m_nCount = new_size;
			return false;
		}

		ILINE static void deallocate(array) {}

		using B::m_aElems;
		using B::m_nCount;

		I m_nCapacity;
	};
};

//---------------------------------------------------------------------------
//! StaticDynStorage: Uses fixed local member storage.
template<int nSIZE>
struct StaticDynStorage
{
	template<class T, class I = int>
	struct Store
	{
		// Basic storage
		T*       begin()               { return (T*)m_aData; }
		const T* begin() const         { return (const T*)m_aData; }
		CONST_VAR_FUNCTION(T * end(),  { return begin() + m_nCount; })
		I        size() const          { return m_nCount; }

		// Dynamic storage
		static I      capacity()       { return (I)nSIZE; }
		static I      max_size()       { return (I)nSIZE; }
		static size_t get_alloc_size() { return 0; }

	protected:

		// Resize functions; no allocation, just set size
		typedef Array<T, I> array;

		ILINE bool reallocate(array&, I new_size, bool allow_slack = true)
		{
			debug_assert(new_size >= 0 && new_size <= capacity());
			m_nCount = new_size;
			return false;
		}
		ILINE static void deallocate(array) {}

		I    m_nCount = 0;

		CRY_ALIGN(alignof(T)) char m_aData[nSIZE * sizeof(T)];       // Storage for elems, deferred construction
	};
};
} // NArray

//---------------------------------------------------------------------------
// Specify allocation for dynamic arrays

namespace NAlloc
{
// Multi-purpose allocation function prototype
struct AllocArray
{
	void*  data;
	size_t size;

	AllocArray(void* data_ = 0, size_t size_ = 0)
		: data(data_), size(size_) {}

	void adjust_front(int n)
	{
		if (data)
			(char*&)data += n;
		if (size)
			size -= n;
	}
};

typedef AllocArray (* Allocator)(AllocArray a, size_t nSize, size_t nAlign, bool bSlack);

//
// Allocation utilities
//

// Helper function for types
template<class A, class T, class I>
static Array<T, I> Alloc(A& allocator, Array<T, I> cur, I nNewCount, size_t nAlign = 1, bool bSlack = false)
{
	NAlloc::AllocArray a = allocator.alloc(NAlloc::AllocArray(cur.begin(), cur.size_mem()), nNewCount * sizeof(T), nAlign, bSlack);
	return Array<T, I>((T*)a.data, check_cast<I>(a.size / sizeof(T)));
}

inline size_t realloc_size(size_t nMinSize)
{
	// Choose an efficient realloc size, when growing an existing (non-zero) block.
	// Find the next power-of-two, minus a bit of presumed system alloc overhead.
	static const size_t nMinAlloc = 32;
	static const size_t nOverhead = 16;
	static const size_t nDoubleLimit =
	  sizeof(size_t) < 8 ? 1 << 12    // 32-bit system
	  : 1 << 16;                      // >= 64-bit system

	nMinSize += nOverhead;
	size_t nAlloc = nMinAlloc;
	while (nAlloc < nMinSize)
		nAlloc <<= 1;
	if (nAlloc > nDoubleLimit)
	{
		size_t nAlign = std::max(nAlloc >> 3, nDoubleLimit);
		nAlloc = Align(nMinSize, nAlign);
	}
	return nAlloc - nOverhead;
}

struct AllocFunction
{
	Allocator  m_Function;

	AllocArray alloc(AllocArray a, size_t nSize, size_t nAlign = 1, bool bSlack = false)
	{
		return m_Function(a, nSize, nAlign, bSlack);
	}
};

//! Adds alignment to allocation
template<class A, size_t nAlignment>
struct AllocAlign : A
{
	AllocArray alloc(AllocArray a, size_t nSize, size_t nAlign = 1, bool bSlack = false)
	{
		return A::alloc(a, Align(nSize, nAlignment), std::max(nAlign, nAlignment), bSlack);
	}
};


//! Adds prefix bytes to allocation, preserving alignment.
template<class A, class Prefix, int nSizeAlign = 1>
struct AllocPrefix : A
{
	AllocArray alloc(AllocArray a, size_t nSize, size_t nAlign = 1, bool bSlack = false)
	{
		// Adjust pointer and size for prefix bytes
		nAlign = std::max(nAlign, alignof(Prefix));
		int nPrefixSize = check_cast<int>(Align(sizeof(Prefix), nAlign));

		a.adjust_front(-nPrefixSize);
		if (nSize)
		{
			nSize = Align(nSize, nSizeAlign);
			nSize += nPrefixSize;
		}

		a = A::alloc(a, nSize, nAlign, bSlack);

		a.adjust_front(nPrefixSize);
		return a;
	}
};

//! Stores and retrieves allocator function in memory, for compatibility with diverse allocators.
template<class A>
struct AllocCompatible
{
	AllocArray alloc(AllocArray a, size_t nSize, size_t nAlign = 1, bool bSlack = false)
	{
		AllocPrefix<AllocFunction, Allocator> alloc_prefix;
		if (a.data)
		{
			// Retrieve original allocation function, for realloc or dealloc
			alloc_prefix.m_Function = ((Allocator*)a.data)[-1];
		}
		else
		{
			// Allocate new with this module's base_allocator
			alloc_prefix.m_Function = &A::alloc;
		}
		a = alloc_prefix.alloc(a, nSize, nAlign, bSlack);
		if (a.data)
			// Store the pointer to the allocator
			((Allocator*)a.data)[-1] = alloc_prefix.m_Function;
		return a;
	}
};

//---------------------------------------------------------------------------
// Allocators for DynArray.

//! Standard CryModule memory allocation, using aligned versions.
struct ModuleAlloc
{
	static AllocArray alloc(AllocArray a, size_t nSize, size_t nAlign = 1, bool bSlack = false)
	{
		if (nSize)
		{
			if (nSize != a.size)
			{
				// Alloc new
				if (bSlack && a.size)
					nSize = realloc_size(nSize);
				a.data = CryModuleMemalign(nSize, nAlign);
				a.size = nSize;
			}
		}
		else if (a.data)
		{
			// Dealloc
			CryModuleMemalignFree(a.data);
			a = AllocArray();
		}
		return a;
	}
};

//! Standard allocator for DynArray stores a compatibility pointer in the memory.
typedef AllocCompatible<ModuleAlloc> StandardAlloc;

//! Allocator using specific heaps.
struct GeneralHeapAlloc : ModuleAlloc
{
	IGeneralMemoryHeap* m_pHeap;

	GeneralHeapAlloc()
		: m_pHeap(0) {}

	explicit GeneralHeapAlloc(IGeneralMemoryHeap* pHeap)
		: m_pHeap(pHeap) {}

	AllocArray alloc(AllocArray a, size_t nSize, size_t nAlign = 1, bool bSlack = false) const
	{
		if (m_pHeap)
		{
			if (!nSize)
			{
				if (a.data)
					// Dealloc
					m_pHeap->Free(a.data);
				return AllocArray();
			}
			else if (nSize != a.size)
			{
				// Alloc
				if (bSlack && a.size)
					nSize = realloc_size(nSize);
				a.data = m_pHeap->Memalign(nAlign, nSize, "");
				a.size = nSize;
			}
			return a;
		}

		return ModuleAlloc::alloc(a, nSize, nAlign, bSlack);
	}
};
} // NAlloc

//---------------------------------------------------------------------------
//! Storage schemes for allocating DynArrays
namespace NArray
{

//---------------------------------------------------------------------------
//! FastDynStorage: STORAGE scheme for DynArray<T,I,STORAGE>.
//! Simple extension to Storage: size & capacity fields are inline, 3 words storage, fast access.
template<class A = NAlloc::StandardAlloc>
struct FastDynStorage
{
	template<class T, class I = int>
	struct Store : protected A, public FixedDynStorage::Store<T, I>
	{
		typedef FixedDynStorage::Store<T, I> B;

		// Construction.
		Store() {}

		Store(const A& a)
			: A(a) {}

		static I max_size()             { return std::numeric_limits<I>::max(); }
		size_t   get_alloc_size() const { return m_nCapacity * sizeof(T); }

	protected:

		typedef Array<T, I> array;

		bool reallocate(array& old, I new_size, bool allow_slack = true)
		{
			if (allow_slack ? new_size > m_nCapacity : new_size != m_nCapacity)
			{
				old.set(m_aElems, m_nCapacity);
				if (!new_size)
				{
					this->set(0, 0);
				}
				else
				{
					auto ra = NAlloc::Alloc(static_cast<A&>(*this), old, new_size, alignof(T), allow_slack);
					m_aElems = ra.data();
					m_nCapacity = ra.size();
					m_nCount = new_size;
				}
				return m_aElems != old.begin();
			}
			else
			{
				m_nCount = new_size;
				return false;
			}
		}

		ILINE void deallocate(array a)
		{
			if (a.begin())
				NAlloc::Alloc(static_cast<A&>(*this), a, I(0), alignof(T), false);
		}

		using B::m_aElems;
		using B::m_nCount;
		using B::m_nCapacity;
	};
};

//---------------------------------------------------------------------------
//! SmallDynStorage: STORAGE scheme for DynArray<T,I,STORAGE,ALLOC>.
//! Array is just a single pointer, size and capacity information stored before the array data.
//! Slightly slower than FastDynStorage, optimal for saving space, especially when array likely to be empty.
template<class A = NAlloc::StandardAlloc>
struct SmallDynStorage
{
	template<class T, class I>
	struct Store : protected A
	{
		// Construction.
		Store()
		{
			set_null();
		}

		Store(const A& a)
			: A(a)
		{
			set_null();
		}

		// Basic storage.
		CONST_VAR_FUNCTION(T * begin(), { return m_aElems; })
		CONST_VAR_FUNCTION(T * end(),   { return m_aElems + size(); })
		ILINE I size() const            { return header()->m_nCount; }

		// Dynamic storage
		ILINE I  capacity() const       { return header()->m_nCapacity; }
		static I max_size()             { return std::numeric_limits<I>::max(); }
		size_t   get_alloc_size() const { return is_null() ? 0 : capacity() * sizeof(T) + sizeof(I); }

	protected:

		typedef Array<T, I> array;

		struct Header
		{
			I m_nCapacity;
			I m_nCount;

			ILINE bool is_null() const { return m_nCapacity == 0; }
		};

		CONST_VAR_FUNCTION(Header * header(),
		{
			debug_assert(m_aElems);
			return ((Header*)m_aElems) - 1;
		})

		ILINE static T* null_header()
		{
			// m_aElems is never 0, for empty array points to a static empty header.
			struct EmptyHeader
			{
				Header h;
				char CRY_ALIGN(alignof(T)) elem;
			};
			static EmptyHeader s_EmptyHeader;
			size_t st = sizeof(T), at = alignof(T), sh = sizeof(EmptyHeader), ah = alignof(EmptyHeader);
			return (T*)&s_EmptyHeader.elem;
		}
		ILINE void set_null()      { m_aElems = null_header(); }
		ILINE bool is_null() const { return header()->is_null(); }

		bool reallocate(array& old, I new_size, bool allow_slack = true)
		{
			I cap = capacity();
			if (allow_slack ? new_size > cap : new_size != cap)
			{
				old.set(is_null() ? 0 : begin(), cap);
				if (!new_size)
				{
					set_null();
				}
				else
				{
					auto ra = NAlloc::Alloc(allocator(), old, new_size, alignof(T), allow_slack);
					if (!ra.size())
						set_null();
					else
					{
						m_aElems = ra.data();
						header()->m_nCount = new_size;
						header()->m_nCapacity = ra.size();
					}
				}
				return begin() != old.begin();
			}
			else
			{
				if (!is_null())
					header()->m_nCount = new_size;
				return false;
			}
		}

		void deallocate(array a)
		{
			if (a.begin())
				NAlloc::Alloc(allocator(), a, I(0), alignof(T), false);
		}

		typedef NAlloc::AllocPrefix<A, Header, sizeof(I)> AP;

		AP& allocator()
		{
			static_assert(sizeof(AP) == sizeof(A), "Invalid type size!");
			return static_cast<AP&>(static_cast<A&>(*this));
		}

		T* m_aElems;
	};
};

//---------------------------------------------------------------------------
// LocalDynStorage: STORAGE scheme with a local member array for small allocations, dynamic allocation for larger
template<int nSIZE, class A = NAlloc::StandardAlloc>
struct LocalDynStorage
{
	template<class T, class I = int>
	struct Store : protected A, public FixedDynStorage::Store<T, I>
	{
		typedef FixedDynStorage::Store<T, I> B;

		// Construction.
		Store()
			: B((T*)m_aData, 0, nSIZE) {}

		Store(const A& a)
			: B((T*)m_aData, 0, nSIZE), A(a) {}

		Store(const Store& o)
			: B(o)
		{
			copy_local(o);
		}
		Store& operator=(const Store& o)
		{
			B::operator=(o);
			copy_local(o);
			return *this;
		}

		static I max_size()             { return std::numeric_limits<I>::max(); }
		size_t   get_alloc_size() const { return is_local() ? 0 : B::get_alloc_size(); }

	protected:

		typedef Array<T, I> array;

		bool reallocate(array& old, I new_size, bool allow_slack = true)
		{
			if (allow_slack || is_local() ? new_size > m_nCapacity : new_size != m_nCapacity)
			{
				old.set(m_aElems, m_nCapacity);
				if (!new_size)
					this->set(0, 0);
				else
				{
					array ra = NAlloc::Alloc(static_cast<A&>(*this), is_local() ? array() : old, new_size, alignof(T), allow_slack);
					m_aElems = ra.data();
					m_nCapacity = ra.size();
					m_nCount = new_size;
				}
				return m_aElems != old.begin();
			}
			else
			{
				m_nCount = new_size;
				return false;
			}
		}

		void deallocate(array a)
		{
			if (a.begin() && a.begin() != (T*)m_aData)
				NAlloc::Alloc(static_cast<A&>(*this), a, 0, alignof(T), false);
		}

		bool is_local() const { return m_aElems == (T*)m_aData; }

		void copy_local(const Store& o)
		{
			if (o.is_local())
			{
				m_aElems = (T*)m_aData;
				array(m_aElems, m_nCount).copy(0, o.m_aElems, m_nCount);
			}
		}

		using B::m_aElems;
		using B::m_nCount;
		using B::m_nCapacity;

		CRY_ALIGN(alignof(T)) char m_aData[nSIZE * sizeof(T)];       // Storage for elems, deferred construction
	};
};
} // NArray

//---------------------------------------------------------------------------
//! DynArray<T,I,S>: Extension of Array allowing dynamic allocation.
//! S specifies storage scheme, as with Array, but adds resize(), capacity(), ...
//! Behaves in almost all respects like std::vector.

template<class T, class I = int, class STORE = NArray::SmallDynStorage<>>
struct DynArray : Array<T, I, STORE>
{
	typedef Array<T, I, STORE>                   B;
	typedef typename STORE::template Store<T, I> S;

	// Tedious redundancy for non-MS compilers
	using_type(B, iterator);
	using_type(B, const_iterator);
	using_type(B, array);
	using_type(B, const_array);

	using B::npos;
	using B::begin;
	using B::end;
	using B::size;
	using B::capacity;
	using B::at;
	using B::back;
	using B::copy;
	using B::init;
	using B::destroy;
	using B::move_init;

	//
	// Construction.
	//
	~DynArray()
	{
		clear();
	}

	using B::B;

	DynArray()
	{}

	// Copy init/assign.
	DynArray(const DynArray& a)
	{
		create(a());
	}
	DynArray& operator=(const DynArray& a)
	{
		assign(a());
		return *this;
	}

	//! Move init/assign, by copying and clearing array storage structure; does not realloc or copy elements.
	DynArray(DynArray&& a)
	{
		std::swap(base(), a.base());
	}
	DynArray& operator=(DynArray&& a)
	{
		std::swap(base(), a.base());
		return *this;
	}

	// Single-argument constructors
	DynArray(I count)
	{
		create(count, eDefaultInit);
	}
	DynArray(const_array a)
	{
		create(a);
	}

	// 2-argument constructors
	template<class Val1, class Val2>
	DynArray(Val1&& val1, Val2&& val2)
	{
		create(std::forward<Val1>(val1), std::forward<Val2>(val2));
	}

	DynArray(std::initializer_list<T> list)
	{
		reserve(list.size());
		for (const T& element : list)
			emplace_back(element);
	}

	template<class Val>
	DynArray& operator=(const Val& val)
	{
		assign(val);
		return *this;
	}

	void swap(DynArray& a)
	{
		std::swap(base(), a.base());
	}

	I available() const
	{
		return capacity() - size();
	}

	//
	// Allocation modifiers.
	//

	void reserve(I count)
	{
		if (count > capacity())
		{
			I s = size();
			realloc_move(count, false);
			realloc_move(s, true);
		}
	}

	void shrink_to_fit()
	{
		// Realloc memory to exact array size.
		if (size() < capacity())
			realloc_move(size(), false);
	}

	// Resize
	void resize(I new_size)
	{
		resize(new_size, eDefaultInit);
	}
	template<class V>
	void resize(I new_size, const V& val)
	{
		I s = size();
		if (new_size > s)
			append(new_size - s, val);
		else if (new_size < s)
			erase(new_size, s - new_size);
	}

	// Universal function to insert/erase/replace elements with initializer values
	template<class Pos1, class Pos2, class ... Vals>
	iterator replace(Pos1 pos1, Pos2 pos2, Vals&& ... vals)
	{
		I pos = index(pos1), count = index_count(pos1, pos2);
		I val_count = value_count(vals ...);
		if (val_count > count)
		{
			// Growing
			destroy(pos, count);
			realloc_replace(pos, count, val_count);
			init(pos, std::forward<Vals>(vals) ...);
		}
		else
		{
			copy(pos, std::forward<Vals>(vals) ...);
			if (val_count < count)
			{
				// Shrinking; safe for partial reassignment a = a(i, n)
				destroy(pos + val_count, count - val_count);
				realloc_replace(pos, count, val_count);
			}
		}
		return begin() + pos;
	}

	// Assign
	template<class ... Vals>
	void assign(Vals&& ... vals)
	{
		replace(0, size(), std::forward<Vals>(vals) ...);
	}

	// Insert
	template<class Pos, class ... Vals>
	iterator insert(Pos pos1, Vals&& ... vals)
	{
		// Collapsed version of replace
		I pos = index(pos1);
		I val_count = value_count(vals ...);
		if (val_count)
		{
			realloc_replace(pos, 0, val_count);
			init(pos, std::forward<Vals>(vals) ...);
		}
		return begin() + pos;
	}

	template<class Pos, class ... Vals>
	iterator emplace(Pos pos1, Vals&& ... vals)
	{
		I pos = index(pos1);
		realloc_replace(pos, 0, 1);
		return new(&at(pos)) T(std::forward<Vals>(vals) ...);
	}

	// Append
	template<class ... Vals>
	iterator append(Vals&& ... vals)
	{
		// Collapsed version of insert
		I pos = size();
		I val_count = value_count(vals ...);
		if (val_count)
		{
			realloc_move(pos + val_count);
			init(pos, std::forward<Vals>(vals) ...);
		}
		return begin() + pos;
	}

	template<class ... Vals>
	iterator emplace_back(Vals&& ... vals)
	{
		realloc_move(size() + 1);
		return new(&back()) T(std::forward<Vals>(vals) ...);
	}

	// Synonyms for append: push_back, and +=
	iterator push_back()
	{
		return append(eDefaultInit);
	}
	template<class Val>
	iterator push_back(Val&& val)
	{
		return append(std::forward<Val>(val));
	}
	template<class Val>
	DynArray& operator+=(Val&& val)
	{
		append(std::forward<Val>(val));
		return *this;
	}
	template<class Val>
	DynArray operator+(Val&& val) const
	{
		DynArray ret;
		I val_count = value_count(val);
		ret.reserve(size() + val_count);
		ret.create(*this);
		ret.append(std::forward<Val>(val));
		return ret;
	}

	// Erase
	template<class Pos1, class Pos2>
	iterator erase(Pos1 pos1, Pos2 pos2)
	{
		// Collapsed version of _replace
		I pos = index(pos1), count = index_count(pos1, pos2);
		if (count)
		{
			destroy(pos, count);
			realloc_replace(pos, count, 0);
		}
		return begin() + pos;
	}

	template<class Pos>
	iterator erase(Pos pos)
	{
		return erase(pos, 1);
	}

	void pop_back(I count = 1)
	{
		erase(size() - count, count);
	}

	void clear()
	{
		destroy();
		array old;
		if (S::reallocate(old, 0, false))
			S::deallocate(old);
	}

protected:

	B& base() { return static_cast<B&>(*this); }

	void realloc_move(I new_size, bool allow_slack = true)
	{
		array old;
		I old_size = size();
		if (S::reallocate(old, new_size, allow_slack))
		{
			move_init(0, old(0, old_size));
			S::deallocate(old);
		}
	}

	void realloc_replace(I pos, I old_count, I new_count, bool allow_slack = true)
	{
		array old;
		if (S::reallocate(old, size() + new_count - old_count, allow_slack))
		{
			move_init(0, old(0, pos));
			move_init(pos + new_count, old(pos + old_count, size() - pos - new_count));
			S::deallocate(old);
		}
		else
		{
			move_init(pos + new_count, array(begin() + pos + old_count, size() - pos - new_count));
		}
	}

	// Create
	template<class ... Vals>
	void create(Vals&& ... vals)
	{
		assert(size() == 0);
		I val_count = value_count(vals ...);
		if (val_count)
		{
			array old;
			S::reallocate(old, val_count);
			init(0, std::forward<Vals>(vals) ...);
		}
	}

	//
	// Helper functions to allow both indices and iterators as position indicators
	//
	ILINE I index(I i) const
	{
		debug_assert(i >= 0 && i <= size());
		return i;
	}
	ILINE I index(const_iterator it) const
	{
		debug_assert(it >= begin() && it <= end());
		return I(it - begin());
	}

	ILINE I index_count(I pos, I count) const
	{
		debug_assert(pos + count <= size());
		return count;
	}
	ILINE I index_count(iterator start, iterator finish) const
	{
		debug_assert(start >= begin() && finish >= start && finish <= end());
		return I(finish - start);
	}
	ILINE I index_count(iterator start, I count) const
	{
		debug_assert(start >= begin() && start + count <= end());
		return count;
	}

	//
	// Count of initializer values
	//

	// Single-argument values
	struct single_value
	{
		template<class Val> ILINE static I count(const Val&) { return 1; }
	};
	struct array_value
	{
		template<class Val> ILINE static I count(const Val& val) { return const_array(val).size(); }
	};

	template<class Val> ILINE static I value_count(const Val& val)
	{
		typedef typename std::conditional<
			std::is_convertible<Val, const_array>::value,
			array_value,
			single_value
		>::type V;
		return V::count(val);
	}

	// Multi-argument values
	template<class Val> ILINE static I     value_count(I n, const Val&)                    { return n; }
	ILINE static I                         value_count(const_iterator p, I n)              { return n; }
	ILINE static I                         value_count(const_iterator p, const_iterator e) { return check_cast<I>(e - p); }
};

//---------------------------------------------------------------------------
// DynArray type aliases

template<class T, class I = int>
using FixedDynArray = DynArray<T, I, NArray::FixedDynStorage>;

template<class T, int nSIZE, class I = int>
using StaticDynArray = DynArray<T, I, NArray::StaticDynStorage<nSIZE>>;

template<class T, class I = int, class A = NAlloc::StandardAlloc>
using FastDynArray = DynArray<T, I, NArray::FastDynStorage<A>>;

template<class T, class I = int, class A = NAlloc::StandardAlloc>
using SmallDynArray = DynArray<T, I, NArray::SmallDynStorage<A>>;

template<class T, int nSIZE, class I = int, class A = NAlloc::StandardAlloc>
using LocalDynArray = DynArray<T, I, NArray::LocalDynStorage<nSIZE, A>>;

//! Legacy base class of DynArray, only used for read-only access.
template<class T>
using DynArrayRef = DynArray<T>;

#undef debug_assert

#include <Cry3DEngine/CryPodArray.h>

#endif // _CRY_ARRAY_H_
