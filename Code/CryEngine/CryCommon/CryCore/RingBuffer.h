// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "AlignmentTools.h"

// A fixed-size type-safe ring buffer (ie, fixed-size double-ended queue).
// Note: It's possible to add support for iterators, indexing if needed.
template<typename T, size_t N, typename I = uint32>
class CRingBuffer
{
	static_assert(std::is_integral<I>::value && std::is_unsigned<I>::value, "I is not unsigned integral type");
	static_assert(N != 0 && N <= I(-1), "N is not a valid value (or I is too small)");
	enum : I
	{
		kPowerOf2 = (N & (N - 1)) == 0,
		kMaxSize = static_cast<I>(N),
	};

public:
	typedef T        value_type;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef I        size_type;

	// Constructs an empty ring buffer.
	CRingBuffer()
		: m_begin(0)
		, m_count(0)
	{}

	// Destroy a ring buffer.
	~CRingBuffer()
	{
		clear();
	}

	// Retrieve the size of the collection.
	size_type size() const
	{
		return m_count;
	}

	// Retrieve the maximum size of the collection.
	size_type max_size() const
	{
		return kMaxSize;
	}

	// Test if the collection is empty.
	bool empty() const
	{
		return m_count == 0;
	}

	// Test if the collection is full.
	bool full() const
	{
		return m_count == kMaxSize;
	}

	// Get the front-most item of the collection.
	// If the collection is empty, the behavior is undefined.
	reference front()
	{
		CRY_ASSERT_MESSAGE(m_count != 0, "Container is empty");
		return *ptr(m_begin);
	}

	// Get the front-most item of the collection.
	// If the collection is empty, the behavior is undefined.
	const_reference front() const
	{
		CRY_ASSERT_MESSAGE(m_count != 0, "Container is empty");
		return *ptr(m_begin);
	}

	// Get the back-most item of the collection.
	// If the collection is empty, the behavior is undefined.
	reference back()
	{
		CRY_ASSERT_MESSAGE(m_count != 0, "Container is empty");
		return *ptr(wrap(m_begin + m_count - 1));
	}

	// Get the back-most item of the collection.
	// If the collection is empty, the behavior is undefined.
	const_reference back() const
	{
		CRY_ASSERT_MESSAGE(m_count != 0, "Container is empty");
		return *ptr(wrap(m_begin + m_count - 1));
	}

	// Adds an item to the front of the collection.
	// In case the collection is full, the function returns false and the collection remains unmodified.
	template<typename X>
	bool push_front(X&& value)
	{
		static_assert(std::is_constructible<T, X&&>::value, "T cannot be constructed from the given type");
		if (full())
		{
			return false;
		}
		const I index = decrement(m_begin);
		::new(static_cast<void*>(ptr(index)))T(std::forward<X>(value));
		m_begin = index;
		++m_count;
		return true;
	}

	// Adds an item to the front of the collection.
	// In case the collection is full, the function overwrites the last item in the collection.
	template<typename X>
	void push_front_overwrite(X&& value)
	{
		static_assert(std::is_constructible<T, X&&>::value, "T cannot be constructed from the given type");
		const I index = decrement(m_begin);
		if (full())
		{
			ptr(index)->~T();
			--m_count;
		}
		::new(static_cast<void*>(ptr(index)))T(std::forward<X>(value));
		m_begin = index;
		++m_count;
	}

	// Removes an item from the front of the collection.
	// If the collection is empty, the behavior is undefined.
	void pop_front()
	{
		CRY_ASSERT_MESSAGE(m_count != 0, "Container is empty");
		ptr(m_begin)->~T();
		m_begin = increment(m_begin);
		--m_count;
	}

	// Attempts to remove an item from the front of the collection, and assigns it to 'value'.
	// Returns true if an item was removed, false if the collection was empty (and 'value' remains unmodified).
	bool try_pop_front(T& value)
	{
		if (m_count != 0)
		{
			T* const pItem = ptr(m_begin);
			value = std::move(*pItem);
			pItem->~T();
			m_begin = increment(m_begin);
			--m_count;
			return true;
		}
		return false;
	}

	// Adds an item to the back of the collection.
	// In case the collection is full, the function returns false and the collection remains unmodified.
	template<typename X>
	bool push_back(X&& value)
	{
		static_assert(std::is_constructible<T, X&&>::value, "T cannot be constructed from the given type");
		if (full())
		{
			return false;
		}
		const I index = wrap(m_begin + m_count);
		::new(static_cast<void*>(ptr(index)))T(std::forward<X>(value));
		++m_count;
		return true;
	}

	// Adds an item to the back of the collection.
	// In case the collection is full, the function overwrites the first item in the collection.
	template<typename X>
	void push_back_overwrite(X&& value)
	{
		static_assert(std::is_constructible<T, X&&>::value, "T cannot be constructed from the given type");
		const I index = wrap(m_begin + m_count);
		if (full())
		{
			ptr(index)->~T();
			m_begin = increment(index);
			--m_count;
		}
		::new(static_cast<void*>(ptr(index)))T(std::forward<X>(value));
		++m_count;
	}

	// Removes an item from the back of the collection.
	// If the collection is empty, the behavior is undefined.
	void pop_back()
	{
		CRY_ASSERT_MESSAGE(m_count != 0, "Container is empty");
		const I index = wrap(m_begin + m_count - 1);
		ptr(index)->~T();
		--m_count;
	}

	// Attempts to remove an item from the back of the collection, and assigns it to 'value'.
	// Returns true if an item was removed, false if the collection was empty (and 'value' remains unmodified).
	bool try_pop_back(T& value)
	{
		if (m_count != 0)
		{
			const I index = wrap(m_begin + m_count - 1);
			T* const pItem = ptr(index);
			value = std::move(*pItem);
			pItem->~T();
			--m_count;
			return true;
		}
		return false;
	}

	// Destroy all items in a ring buffer.
	void clear()
	{
		size_type index = m_begin;
		for (size_type i = 0; i < m_count; ++i, index = increment(index))
		{
			ptr(index)->~T();
		}
		m_begin = 0;
		m_count = 0;
	}

private:
	// Decrements a given index, wrapping it around N.
	static size_type decrement(size_type index)
	{
		return kPowerOf2 ? ((index - 1) & (kMaxSize - 1)) : index ? index - 1 : kMaxSize - 1;
	}

	// Increments a given index, wrapping it around N.
	static size_type increment(size_type index)
	{
		++index;
		return kPowerOf2 ? index & (kMaxSize - 1) : index == kMaxSize ? 0 : index;
	}

	// Wraps an index, which has a maximum value of 2N-1.
	static size_type wrap(size_type index)
	{
		return kPowerOf2 ? index & (kMaxSize - 1) : index >= kMaxSize ? index - kMaxSize : index;
	}

	// Obtain pointer to raw storage at given index.
	pointer ptr(size_type index)
	{
		return reinterpret_cast<pointer>(&m_storage) + index;
	}

	// Obtain pointer to raw storage at given index.
	const_pointer ptr(size_type index) const
	{
		return reinterpret_cast<const_pointer>(&m_storage) + index;
	}

	// No copy/assign supported.
	CRingBuffer(const CRingBuffer&);
	void operator=(const CRingBuffer&);

	size_type m_begin, m_count;
	typename SAlignedStorage<sizeof(T)* N, std::alignment_of<T>::value>::type m_storage;
};
