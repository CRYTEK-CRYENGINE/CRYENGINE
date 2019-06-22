// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "VirtualMemory.h"

#if CRY_PLATFORM_ORBIS

const size_t CVirtualMemory::s_systemPageSize = PAGE_SIZE;

bool CVirtualMemory::ValidateSystemPageSize()
{
	return s_systemPageSize == PAGE_SIZE;
}

void* CVirtualMemory::ReserveAddressRange(size_t reserveSize, const char* usage, size_t alignment)
{
	reserveSize = Align(reserveSize, s_systemPageSize);
	if (alignment <= s_systemPageSize)
	{
		void* ptr = VirtualAllocator::AllocateVirtualAddressSpace(reserveSize);
		if (ptr != nullptr)
		{
			VA_REGISTER_RANGE(ptr, reserveSize, usage);
		}
		return ptr;
	}
	else
	{
		// we can't specify a base address, so we need to pad
		size_t paddedReserveSize = reserveSize + alignment;
		UINT_PTR addr = reinterpret_cast<UINT_PTR>(VirtualAllocator::AllocateVirtualAddressSpace(paddedReserveSize));
		if (!addr)
			return nullptr;

		UINT_PTR upAlignedAddr = (addr + alignment) & ~(alignment - 1);
		size_t diff = upAlignedAddr - addr;

		// Get access to that region in order to store the padding offset.
		VirtualAllocator::MapPageBlock(reinterpret_cast<void*>(addr), diff);

		// Store the address
		size_t* pAdjustment = ((size_t*)upAlignedAddr) - 1;
		*pAdjustment = diff;

		VA_REGISTER_RANGE((void*)upAlignedAddr, reserveSize, usage);
		return (void*)upAlignedAddr;
	}
}

void CVirtualMemory::UnreserveAddressRange(void* pBase, size_t reservedSize, size_t alignment)
{
	if (alignment <= s_systemPageSize)
	{
		VirtualAllocator::FreeVirtualAddressSpace(pBase);
	}
	else
	{
		size_t* pAdjustment = ((size_t*)pBase) - 1;
		UINT_PTR baseAddr = (UINT_PTR)pBase;
		baseAddr -= *pAdjustment;
		VirtualAllocator::FreeVirtualAddressSpace((void*)baseAddr);
	}
}

void CVirtualMemory::MapPages(void* pBase, size_t size)
{
	UINT_PTR baseAddr = (UINT_PTR)pBase & ~(s_systemPageSize - 1);
	UINT_PTR endAddr = Align((UINT_PTR)pBase + size, s_systemPageSize);
	VirtualAllocator::MapPageBlock((void*)baseAddr, endAddr - baseAddr);
	VA_REGISTER_PAGE(pBase, size);
}

void CVirtualMemory::UnmapPages(void* pBase, size_t size)
{
	UINT_PTR baseAddr = (UINT_PTR)pBase & ~(s_systemPageSize - 1);
	UINT_PTR endAddr = Align((UINT_PTR)pBase + size, s_systemPageSize);
	VirtualAllocator::UnmapPageBlock((void*)baseAddr, endAddr - baseAddr);
	VA_UNREGISTER_PAGE(pBase, size);
}
#endif