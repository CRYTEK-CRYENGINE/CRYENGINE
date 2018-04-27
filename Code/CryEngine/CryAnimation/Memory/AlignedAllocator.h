// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>
#include <CryMemory/STLAlignedAlloc.h>

#include "IAllocator.h"
#include "PolymorphicAllocator.h"

namespace Memory
{

/**
 * A simple IAllocator implementation that allocates aligned memory on the heap.
 */
template<size_t IAlignment>
class AlignedAllocator : public IAllocator
{

public:

	/**
	 * Constructs a new AlignedAllocator object wrapped with the PolymorphicAllocator class.
	 */
	static PolymorphicAllocator Create();

private:

	// IAllocator
	virtual void*                       Allocate(uint32 length) override;
	virtual void                        Free(void* pAddress) override;
	virtual size_t                      GetGuaranteedAlignment() const override;
	virtual std::unique_ptr<IAllocator> Clone() const override;

};

} // namespace Memory

//////////////////////////////////////////////////////////////////////////

namespace Memory
{

template<size_t IAlignment>
inline PolymorphicAllocator AlignedAllocator<IAlignment >::Create()
{
	return PolymorphicAllocator(std::unique_ptr<AlignedAllocator<IAlignment>>(new AlignedAllocator<IAlignment> ));
}

template<size_t IAlignment>
inline void* AlignedAllocator<IAlignment >::Allocate(uint32 length)
{
	return (length > 0) ? (stl::aligned_alloc<char, IAlignment>().allocate(length)) : (nullptr);
}

template<size_t IAlignment>
inline void AlignedAllocator<IAlignment >::Free(void* pAddress)
{
	return stl::aligned_alloc<char, IAlignment>().deallocate(static_cast<char*>(pAddress), 0);
}

template<size_t IAlignment>
inline std::unique_ptr<IAllocator> AlignedAllocator<IAlignment >::Clone() const
{
	return std::unique_ptr<IAllocator>(new AlignedAllocator((*this)));
}

template<size_t IAlignment>
inline size_t AlignedAllocator<IAlignment >::GetGuaranteedAlignment() const
{
	return IAlignment;
}

} // namespace Memory
