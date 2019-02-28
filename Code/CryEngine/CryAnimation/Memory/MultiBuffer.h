// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <array>
#include <CryMemory/CrySizer.h>
#include <CryCore/Typelist.h>

#include "PolymorphicAllocator.h"

namespace Memory
{

/**
 * A simple container class for storing multiple heterogeneous data fields within a continuous memory range.
 */
template<typename TTypeList>
class MultiBuffer
{
	// TODO: Check TTypeList list for trivial construction and trivial copying.
public:

	/**
	 * The total number of fields stored within this buffer.
	 */
	static const size_t FIELDS_COUNT = NTypelist::Length<TTypeList>::value;

	/*
	 * A meta-function returning the static types of fields stored within this buffer.
	 * @param index Index of the field to query.
	 */
	template<size_t index>
	struct get_field_type
	{
		typedef typename NTypelist::TypeAt<TTypeList, index>::type type;
	};

	~MultiBuffer();

	/**
	 * Constructs an empty buffer instance. All new instances are constructed in a locked state by default.
	 * @param allocator A PolymorphicAllocator object to be used by this buffer.
	 */
	MultiBuffer(PolymorphicAllocator allocator);

	/**
	 * Unlocks this buffer instance.
	 * The unlocked state is required for performing resize() calls. It is illegal to call the get() method while in the unlocked state.
	 * Calling this method invalidates all of the existing data fields, if any.
	 */
	void unlock();

	/**
	 * Locks this buffer instance.
	 * The unlocked state is required for performing get() calls. It is illegal to call the resize() method while in the unlocked state.
	 * Calling this method invalidates all of the existing data fields, if any.
	 * Calling this method may cause the buffer object to allocate memory from the associated PolymorphicAllocator object.
	 */
	void lock();

	/**
	 * Resizes a single field of this buffer. This method may only be called if the buffer is in the unlocked state.
	 * @param index Index of the field to resize.
	 * @param size New size of the field, expressed in number of underlying type objects.
	 * @param padding Additional padding of the field, expressed in bytes. Defaults to the underlying type's size.
	 * TODO: Change the index argument into a dynamic one.
	 */
	template<size_t index>
	void resize(const size_t size);
	template<size_t index>
	void resize(const size_t size, const size_t padding);

	/**
	 * Retrieves a field with the specified index. This method may only be called if the buffer is in the locked state.
	 * @param index Index of the field to access.
	 * @return Pointer to the field buffer.
	 */
	template<size_t index>
	auto get()->typename get_field_type<index>::type *;

	/**
	 * Retrieves a field with the specified index. This method may only be called if the buffer is in the locked state.
	 * @param index Index of the field to access.
	 * @return Pointer to the field buffer.
	 */
	template<size_t index>
	auto get() const->typename get_field_type<index>::type const*;

	/**
	 * Returns the size of the field, expressed in number of underlying type objects. This method may only be called if the buffer is in the locked state.
	 * This method does NOT peform an allocation check. Operator bool() still need to be called before accessing
	 * any of the fields after changing the memory layout to make sure the memory has been properly allocated.
	 * @param index Index of the field.
	 * @return Size of the field. It is guaranteed to be _at_least_ the size requested with a prior call to resize (might be greater!).
	 * TODO: Change the index argument into a dynamic one.
	 */
	template<size_t index>
	size_t size() const;

	/**
	 * Checks if this buffer instance is empty. This method may only be called if the buffer is in the locked state.
	 * This method does NOT peform an allocation check. Operator bool() still need to be called before accessing
	 * any of the fields after changing the memory layout to make sure the memory has been properly allocated.
	 * @return Returns true if sizes of _all_ fields are equal to 0.
	 */
	bool empty() const;

	/**
	 * Tests if all fields of this buffer object are properly allocated.
	 * Client should always perform this check after changing memory layout of the buffer (e.g. resizing fields,
	 * copying data from another object) before accessing any data fields with the get() method.
	 */
	operator bool() const;

	/**
	 * Copy constructor. All new instances are constructed in a locked state by default.
	 * @param other Another buffer instance to copy data fields from. Shall be in a locked state.
	 * @param allocator An optional parameter specifying a PolymorphicAllocator object to used by this buffer. If not provided, the allocator of the source object will be used.
	 */
	explicit MultiBuffer(const MultiBuffer& other);
	explicit MultiBuffer(const MultiBuffer& other, PolymorphicAllocator allocator);

	/**
	 * Copy assignment operator. This operator may only be called if the buffer is in the locked state.
	 * @param other Another buffer instance to copy data fields from. Shall be in a locked state.
	 */
	MultiBuffer& operator=(const MultiBuffer& other);

	/**
	 * Swaps two compatible buffer instances. The swap is performed on the entire object, including all underlying buffers and allocator affiliation.
	 */
	void swap(MultiBuffer& other);

	void GetMemoryUsage(ICrySizer* pSizer) const;

private:

	static size_t GetBufferAlignmentRequirement() { return NTypelist::MaximumAlignment<TTypeList>::value; }

	void          UpdateBuffers();
	void          CopyBuffersFrom(const MultiBuffer& other);

	void*         GetBuffer();
	void*         GetField(size_t index);

	static size_t ComputeFieldAlignment(size_t index);
	static bool   IsPowerOfTwo(size_t value);

	std::array<size_t, FIELDS_COUNT> m_offsets;

	void*                            m_pBuffer;
	size_t                           m_bufferSize;

	PolymorphicAllocator             m_allocator;
};

} // namespace Memory

namespace Memory
{

template<typename TTypeList>
inline MultiBuffer<TTypeList>::~MultiBuffer()
{
	m_allocator.Free(m_pBuffer);
}

template<typename TTypeList>
inline MultiBuffer<TTypeList>::MultiBuffer(PolymorphicAllocator allocator)
	: m_offsets()
	, m_pBuffer(nullptr)
	, m_bufferSize(0)
	, m_allocator(allocator)
{}

template<typename TTypeList>
inline void MultiBuffer<TTypeList >::unlock()
{
	// In the unlocked mode, field sizes are stored within the m_offsets buffer instead of offsets.

	for (size_t i = 1; i < FIELDS_COUNT; ++i)
	{
		assert(m_offsets[i] >= m_offsets[i - 1]);
		m_offsets[i] = m_offsets[i] - m_offsets[i - 1];
	}
}

template<typename TTypeList>
inline void MultiBuffer<TTypeList >::lock()
{
	// When switching to locked mode, compute field offsets from their sizes while
	// also taking into account alignment requirements for subsequent fields.

	for (size_t i = 0; i < FIELDS_COUNT; ++i)
	{
		const size_t prevOffset = i > 0 ? m_offsets[i - 1] : 0;
		const size_t nextAlignment = ComputeFieldAlignment(i + 1);

		m_offsets[i] = Align(prevOffset + m_offsets[i], nextAlignment);
	}

	UpdateBuffers();
}
template<typename TTypeList>
template<size_t index>
inline void MultiBuffer<TTypeList >::resize(const size_t size)
{
	static_assert(index < FIELDS_COUNT, "Index is out of range!");

	const size_t padding = std::alignment_of<typename get_field_type<index>::type>::value;
	m_offsets[index] = Align(size * sizeof(typename get_field_type<index>::type), padding);
}

template<typename TTypeList>
template<size_t index>
inline void MultiBuffer<TTypeList >::resize(const size_t size, const size_t padding)
{
	static_assert(index < FIELDS_COUNT, "Index is out of range!");
	assert(IsPowerOfTwo(padding));

	m_offsets[index] = Align(size * sizeof(typename get_field_type<index>::type), padding);
}

template<typename TTypeList>
template<size_t index>
inline auto MultiBuffer<TTypeList>::get()->typename get_field_type<index>::type *
{
	static_assert(index < FIELDS_COUNT, "Index out of range!");

	assert(*this);
	assert(reinterpret_cast<uintptr_t>(GetField(index)) % ComputeFieldAlignment(index) == 0);

	return static_cast<typename get_field_type<index>::type*>(GetField(index));
}

template<typename TTypeList>
template<size_t index>
inline auto MultiBuffer<TTypeList>::get() const->typename get_field_type<index>::type const*
{
	return const_cast<MultiBuffer&>(*this).get<index>();
}

template<typename TTypeList>
template<size_t index>
inline size_t MultiBuffer<TTypeList >::size() const
{
	const size_t beginOffset = (index > 0) ? (m_offsets[index - 1]) : (0);
	const size_t endOffset = m_offsets[index];
	assert(endOffset >= beginOffset);

	return (endOffset - beginOffset) / sizeof(typename get_field_type<index>::type);
}

template<typename TTypeList>
inline bool MultiBuffer<TTypeList >::empty() const
{
	return (m_offsets[FIELDS_COUNT - 1] == 0);
}

template<typename TTypeList>
inline MultiBuffer<TTypeList >::operator bool() const
{
	if (m_bufferSize >= m_offsets[FIELDS_COUNT - 1])
	{
		return true;
	}
	else
	{
		return false;
	}
}

template<typename TTypeList>
inline MultiBuffer<TTypeList>::MultiBuffer(const MultiBuffer& other)
	: m_offsets(other.m_offsets)
	, m_pBuffer(nullptr)
	, m_bufferSize(0)
	, m_allocator(other.m_allocator)
{
	if (other)
	{
		CopyBuffersFrom(other);
	}
}

template<typename TTypeList>
inline MultiBuffer<TTypeList>& MultiBuffer<TTypeList >::operator=(const MultiBuffer& other)
{
	m_offsets = other.m_offsets;

	if (other)
	{
		CopyBuffersFrom(other);
	}
	else
	{
		m_allocator.Free(m_pBuffer);
		m_pBuffer = nullptr;
		m_bufferSize = 0;
	}

	return *this;
}

template<typename TTypeList>
inline MultiBuffer<TTypeList>::MultiBuffer(const MultiBuffer& other, PolymorphicAllocator allocator)
	: m_offsets(other.m_offsets)
	, m_pBuffer(nullptr)
	, m_bufferSize(0)
	, m_allocator(allocator)
{
	if (other)
	{
		CopyBuffersFrom(other);
	}
}

template<typename TTypeList>
inline void MultiBuffer<TTypeList >::swap(MultiBuffer<TTypeList>& other)
{
	std::swap(m_offsets, other.m_offsets);
	std::swap(m_pBuffer, other.m_pBuffer);
	std::swap(m_bufferSize, other.m_bufferSize);
	std::swap(m_allocator, other.m_allocator);
}

template<typename TTypeList>
inline void MultiBuffer<TTypeList >::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, m_bufferSize);
}

template<typename TTypeList>
inline void MultiBuffer<TTypeList >::UpdateBuffers()
{
	const size_t dataSize = m_offsets[FIELDS_COUNT - 1];
	const size_t alignmentOffset = (dataSize > 0) ? (GetBufferAlignmentRequirement() - std::min(GetBufferAlignmentRequirement(), m_allocator.GetGuaranteedAlignment())) : (0);
	const size_t allocationSize = dataSize + alignmentOffset;
	if (m_bufferSize < allocationSize)
	{
		m_allocator.Free(m_pBuffer);

		m_pBuffer = m_allocator.Allocate(allocationSize);
		m_bufferSize = m_pBuffer ? allocationSize : 0;
	}
}

template<typename TTypeList>
inline void MultiBuffer<TTypeList >::CopyBuffersFrom(const MultiBuffer& other)
{
	assert(other);
	assert(m_offsets[FIELDS_COUNT - 1] == other.m_offsets[FIELDS_COUNT - 1]);

	UpdateBuffers();
	if (*this)
	{
		const size_t dataSize = m_offsets[FIELDS_COUNT - 1];
		if (dataSize > 0)
		{
			assert(dataSize <= m_bufferSize);
			assert(dataSize <= other.m_bufferSize);
			memcpy(m_pBuffer, other.m_pBuffer, dataSize);
		}
	}
}

template<typename TTypeList>
inline void* MultiBuffer<TTypeList >::GetBuffer()
{
	return Align(m_pBuffer, GetBufferAlignmentRequirement());
}

template<typename TTypeList>
inline void* MultiBuffer<TTypeList >::GetField(size_t index)
{
	assert(GetBuffer());

	const size_t offset = (index > 0) ? (m_offsets[index - 1]) : (0);
	return static_cast<char*>(GetBuffer()) + offset;
}

template<typename TTypeList>
inline size_t MultiBuffer<TTypeList >::ComputeFieldAlignment(size_t index)
{
	if (index < FIELDS_COUNT)
	{
		return NTypelist::RuntimeTraitEvaluator<TTypeList, std::alignment_of>()(index);
	}
	else
	{
		return 1;
	}
}

template<typename TTypeList>
inline bool MultiBuffer<TTypeList >::IsPowerOfTwo(size_t value)
{
	return ((value & (value - (size_t)1)) == 0);
}

} // namespace Memory
