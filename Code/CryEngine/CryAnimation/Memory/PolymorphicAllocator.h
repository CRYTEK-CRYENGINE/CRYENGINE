// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>
#include "IAllocator.h"

namespace Memory
{

/**
 * A wrapper class managing lifetime of IAllocator instances.
 * Provides additional move, swap and copy semantics compatible with the standard C++ Allocator concept and may be used in a similar fashion.
 * TODO: Extend into a proper C++ allocator.
 */
class PolymorphicAllocator
{
public:

	/**
	 * Constructs a new PolymorphicAllocator object.
	 * @param allocator An IAllocator instance to be wrapped by this object.
	 */
	PolymorphicAllocator(std::unique_ptr<IAllocator> allocator);

	/**
	 * Allocates a memory buffer with the requested size.
	 * Each call to this method should be coupled with corresponding call to Free().
	 * @param length Size of the buffer to allocate, expressed in bytes. If a value 0 is passed, allocation fails and a nullptr is returned.
	 * @return Pointer to a buffer that is at least 'length' bytes long or nullptr if allocation could not be performed.
	 */
	void* Allocate(uint32 length);

	/**
	 * Releases a memory buffer previously allocated with a call to Allocate().
	 * @param pAddress Pointer to the buffer which should be released. If a null pointer is passed, no action occurs.
	 */
	void Free(void* pAddress);

	/**
	 * Returns the least guaranteed address alignment of all memory buffers returned by the Allocate() method. Always a power of two.
	 */
	size_t GetGuaranteedAlignment() const;

	/**
	 * Copy constructor.
	 */
	PolymorphicAllocator(const PolymorphicAllocator& other);

	/**
	 * Copy assignment operator.
	 */
	PolymorphicAllocator& operator=(const PolymorphicAllocator& other);

	/**
	 * Swaps two PolymorphicAllocator instances.
	 */
	void Swap(PolymorphicAllocator& other);

private:

	std::unique_ptr<IAllocator> m_pAllocator;
};

} // namespace Memory

//////////////////////////////////////////////////////////////////////////

namespace Memory
{

inline PolymorphicAllocator::PolymorphicAllocator(std::unique_ptr<IAllocator> allocator)
	: m_pAllocator(std::move(allocator))
{}

} // namespace Memory

namespace std
{

inline void swap(Memory::PolymorphicAllocator& lhs, Memory::PolymorphicAllocator& rhs)
{
	lhs.Swap(rhs);
}

} // namespace std
