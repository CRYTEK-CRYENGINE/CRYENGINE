// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>

namespace Memory
{

struct IAllocator
{
	virtual ~IAllocator() {}

	/**
	 * Allocates a memory buffer with the requested size.
	 * Each call to this method should be coupled with corresponding call to Free().
	 * @param length Size of the buffer to allocate, expressed in bytes. If a value 0 is passed, allocation fails and a nullptr is returned.
	 * @return Pointer to a buffer that is at least 'length' bytes long or nullptr if allocation could not be performed.
	 */
	virtual void* Allocate(uint32 length) = 0;

	/**
	 * Releases a memory buffer previously allocated with a call to Allocate().
	 * @param pAddress Pointer to the buffer which should be released. If a null pointer is passed, no action occurs.
	 */
	virtual void Free(void* pAddress) = 0;

	/**
	 * Returns the least guaranteed address alignment of all memory buffers returned by the Allocate() method. Always a power of two.
	 */
	virtual size_t GetGuaranteedAlignment() const = 0;

	/**
	 * Creates of copy of this allocator instance (polymorphic copy construction).
	 */
	virtual std::unique_ptr<IAllocator> Clone() const = 0;
};

} // namespace Memory
