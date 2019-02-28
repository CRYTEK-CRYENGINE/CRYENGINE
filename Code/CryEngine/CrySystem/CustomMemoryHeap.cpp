// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "CustomMemoryHeap.h"

#if CRY_PLATFORM_DURANGO
	#include <apu.h>
#endif // CRY_PLATFORM_DURANGO

//////////////////////////////////////////////////////////////////////////
CCustomMemoryHeapBlock::CCustomMemoryHeapBlock(CCustomMemoryHeap* pHeap, void* pData, size_t const allocateSize, char const* const szUsage)
	: m_pHeap(pHeap)
	, m_pData(pData)
	, m_size(allocateSize)
#if !defined(_RELEASE)
	, m_usage(szUsage)
#endif
{
}

//////////////////////////////////////////////////////////////////////////
CCustomMemoryHeapBlock::~CCustomMemoryHeapBlock()
{
	m_pHeap->DeallocateBlock(this);
}

//////////////////////////////////////////////////////////////////////////
void* CCustomMemoryHeapBlock::GetData()
{
	return m_pData;
}

//////////////////////////////////////////////////////////////////////////
void CCustomMemoryHeapBlock::CopyMemoryRegion(void* pOutputBuffer, size_t offset, size_t size)
{
	CRY_ASSERT(offset + size <= m_size);
	memcpy(pOutputBuffer, (uint8*)m_pData + offset, size);
}

//////////////////////////////////////////////////////////////////////////
ICustomMemoryBlock* CCustomMemoryHeap::AllocateBlock(size_t const allocateSize, char const* const szUsage, size_t const alignment /* = 16 */)
{
	void* pCreatedData;

	switch (m_allocPolicy)
	{
	case IMemoryManager::eapDefaultAllocator:
		pCreatedData = malloc(allocateSize);
		break;
	case IMemoryManager::eapPageMapped:
		pCreatedData = CryGetIMemoryManager()->AllocPages(allocateSize);
		break;
	case IMemoryManager::eapCustomAlignment:
		CRY_ASSERT_MESSAGE(alignment != 0, "CCustomMemoryHeap: trying to allocate memory via eapCustomAlignment with an alignment of zero!");
		pCreatedData = CryModuleMemalign(allocateSize, alignment);
		break;
#if CRY_PLATFORM_DURANGO
	case IMemoryManager::eapAPU:
		if (ApuAlloc(&pCreatedData, nullptr, Align(allocateSize, alignment), static_cast<UINT32>(alignment)) != S_OK)
		{
			CryFatalError("CCustomMemoryHeap: failed to allocate APU memory! (%" PRISIZE_T " bytes) (%" PRISIZE_T " alignment)", allocateSize, alignment);
		}
		break;
#endif // CRY_PLATFORM_DURANGO
	default:
		CryFatalError("CCustomMemoryHeap: unknown allocation policy during AllocateBlock!");
		break;
	}

	CryInterlockedAdd(&m_allocatedSize, allocateSize);
	return new CCustomMemoryHeapBlock(this, pCreatedData, allocateSize, szUsage);
}

//////////////////////////////////////////////////////////////////////////
void CCustomMemoryHeap::DeallocateBlock(CCustomMemoryHeapBlock* pBlock)
{
	switch (m_allocPolicy)
	{
	case IMemoryManager::eapDefaultAllocator:
		free(pBlock->GetData());
		break;
	case IMemoryManager::eapPageMapped:
		CryGetIMemoryManager()->FreePages(pBlock->GetData(), pBlock->GetSize());
		break;
	case IMemoryManager::eapCustomAlignment:
		CryModuleMemalignFree(pBlock->GetData());
		break;
#if CRY_PLATFORM_DURANGO
	case IMemoryManager::eapAPU:
		ApuFree(pBlock->GetData());
		break;
#endif // CRY_PLATFORM_DURANGO
	default:
		CryFatalError("CCustomMemoryHeap: unknown allocation policy during DeallocateBlock!");
		break;
	}

	const int allocateSize = pBlock->GetSize();
	CryInterlockedAdd(&m_allocatedSize, -allocateSize);
}

//////////////////////////////////////////////////////////////////////////
void CCustomMemoryHeap::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->AddObject(this, m_allocatedSize);
}

//////////////////////////////////////////////////////////////////////////
size_t CCustomMemoryHeap::GetAllocated()
{
	return static_cast<size_t>(m_allocatedSize);
}

//////////////////////////////////////////////////////////////////////////
CCustomMemoryHeap::CCustomMemoryHeap(IMemoryManager::EAllocPolicy const allocPolicy)
	: m_allocatedSize(0)
	, m_allocPolicy(allocPolicy)
{
}
