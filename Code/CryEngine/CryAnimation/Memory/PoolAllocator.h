// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>

#include "IAllocator.h"
#include "PolymorphicAllocator.h"

namespace Memory {
class CPool;
}

namespace Memory
{

/**
 * An IAllocator wrapper for the CPool class.
 */
class PoolAllocator : public IAllocator
{
public:

	/**
	 * Constructs a new PoolAllocator object wrapped with the PolymorphicAllocator class.
	 * @param pool Reference to a Memory::CPool instance to wrap within this allocator.
	 */
	static PolymorphicAllocator Create(CPool& pool);

	/*
	 * Constructs a new PoolAllocator object.
	 * @param pool Reference to a Memory::CPool instance to wrap within this allocator.
	 */
	PoolAllocator(CPool& pool);

private:

	// IAllocator
	virtual void*                       Allocate(uint32 length) override;
	virtual void                        Free(void* pAddress) override;
	virtual size_t                      GetGuaranteedAlignment() const override;
	virtual std::unique_ptr<IAllocator> Clone() const override;

	CPool* m_pPool;
};

} // namespace Memory

//////////////////////////////////////////////////////////////////////////

namespace Memory
{

inline PolymorphicAllocator PoolAllocator::Create(CPool& pool)
{
	return PolymorphicAllocator(std::unique_ptr<PoolAllocator>(new PoolAllocator(pool)));
}

inline PoolAllocator::PoolAllocator(CPool& pool)
	: m_pPool(&pool)
{}

} // namespace Memory
