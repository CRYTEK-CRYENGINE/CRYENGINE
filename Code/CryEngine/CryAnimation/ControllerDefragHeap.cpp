// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ControllerDefragHeap.h"

#include "ControllerPQ.h"

CControllerDefragHeap::CControllerDefragHeap()
	: m_pAddressRange(NULL)
	, m_pAllocator(NULL)
	, m_pBaseAddress(NULL)
	, m_nPageSize(0)
	, m_nLogPageSize(0)
	, m_nBytesInFixedAllocs(0)
	, m_numAvailableCopiesInFlight(MaxInFlightCopies)
	, m_bytesInFlight(0)
	, m_tickId(0)
	, m_nLastCopyIdx(0)
	, m_numQueuedCancels(0)
{
}

CControllerDefragHeap::~CControllerDefragHeap()
{
	if (m_pAllocator)
	{
		// Release with discard because controller heap is global, and torn down with engine. This may
		// occur even though the character manager hasn't been destroyed, so allocations may still be active.
		// The engine is lost though, so it doesn't matter...
		m_pAllocator->Release(true);
		m_pAllocator = NULL;
	}
}

void CControllerDefragHeap::Init(size_t capacity)
{
	assert(!m_pAllocator);

	m_pAddressRange = CryGetIMemoryManager()->ReserveAddressRange(capacity, "Anim Defrag Heap");
	m_pBaseAddress = m_pAddressRange->GetBaseAddress();
	m_nPageSize = m_pAddressRange->GetPageSize();
	m_nLogPageSize = IntegerLog2(m_nPageSize);

	capacity = Align(capacity, m_nPageSize);

	m_pAllocator = CryGetIMemoryManager()->CreateDefragAllocator();

	IDefragAllocator::Policy pol;
	pol.pDefragPolicy = this;
	pol.maxAllocs = MaxAllocs;
	m_pAllocator->Init(capacity, MinAlignment, pol);

	m_numAllocsPerPage.resize(capacity >> m_nLogPageSize);
}

CControllerDefragHeap::Stats CControllerDefragHeap::GetStats()
{
	Stats stats;

	stats.defragStats = m_pAllocator->GetStats();
	stats.bytesInFixedAllocs = (size_t)m_nBytesInFixedAllocs;

	return stats;
}

void CControllerDefragHeap::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	UpdateInflight(m_tickId);

	if (m_pAllocator != nullptr)
	{
		m_pAllocator->DefragmentTick(
			(size_t)min((int)MaxScheduledCopiesPerUpdate, (int)m_numAvailableCopiesInFlight),
			(size_t)min((int)max(0, (int)MinFixedAllocSize - (int)m_bytesInFlight), (int)MaxScheduledBytesPerUpdate));
	}

	++m_tickId;
}

CControllerDefragHdl CControllerDefragHeap::AllocPinned(size_t sz, IControllerRelocatableChain* pContext)
{
	CControllerDefragHdl ret;

	if (sz < MinFixedAllocSize)
	{
		IDefragAllocator::AllocatePinnedResult apr = m_pAllocator->AllocatePinned(sz, "", pContext);

		if (apr.hdl != IDefragAllocator::InvalidHdl)
		{
			// Ensure that the requisite pages are mapped
			if (IncreaseRange(apr.offs, apr.usableSize))
			{
				ret = CControllerDefragHdl(apr.hdl);

				MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
				MEMREPLAY_SCOPE_ALLOC(m_pBaseAddress + apr.offs, apr.usableSize, MinAlignment);
			}
			else
			{
				m_pAllocator->Free(apr.hdl);
			}
		}
		else if (AllowGPHFallback)
		{
			MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

			ret = CControllerDefragHdl(FixedAlloc(sz, true));

			if (ret.IsValid())
			{
				MEMREPLAY_SCOPE_ALLOC(ret.AsFixed(), UsableSize(ret), MinAlignment);
			}
		}
	}
	else
	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

		ret = CControllerDefragHdl(FixedAlloc(sz, false));

		if (ret.IsValid())
		{
			MEMREPLAY_SCOPE_ALLOC(ret.AsFixed(), UsableSize(ret), MinAlignment);
		}
	}

	return ret;
}

void CControllerDefragHeap::Free(CControllerDefragHdl hdl)
{
	if (!hdl.IsFixed())
	{
		UINT_PTR offs = m_pAllocator->Pin(hdl.AsHdl());

		{
			MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			MEMREPLAY_SCOPE_FREE(m_pBaseAddress + offs);
		}

		assert(hdl.IsValid());

		// Ensure that pages are unmapped
		UINT_PTR sz = m_pAllocator->UsableSize(hdl.AsHdl());

		DecreaseRange(offs, sz);

		m_pAllocator->Free(hdl.AsHdl());
	}
	else
	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

		FixedFree(hdl.AsFixed());

		MEMREPLAY_SCOPE_FREE(hdl.AsFixed());
	}
}

size_t CControllerDefragHeap::UsableSize(CControllerDefragHdl hdl)
{
	assert(hdl.IsValid());
	if (!hdl.IsFixed())
	{
		return m_pAllocator->UsableSize(hdl.AsHdl());
	}
	else
	{
		FixedHdr* pHdr = ((FixedHdr*)hdl.AsFixed()) - 1;
		return pHdr->size;
	}
}

uint32 CControllerDefragHeap::BeginCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size, IDefragAllocatorCopyNotification* pNotification)
{
	// Try and boost the destination range
	if (IncreaseRange(dstOffset, size))
	{
		uint32 nLastId = m_nLastCopyIdx;
		uint32 nIdx = (uint32) - 1;

		for (uint32 i = 0; i < MaxInFlightCopies; ++i)
		{
			if (!m_copiesInFlight[(nLastId + i) % MaxInFlightCopies].inUse)
			{
				nIdx = (nLastId + i) % MaxInFlightCopies;
				break;
			}
		}

		if (nIdx != (uint32) - 1)
		{
			assert(nIdx >= 0 && nIdx < CRY_ARRAY_COUNT(m_copiesInFlight));
			Copy& cp = m_copiesInFlight[nIdx];
			stl::reconstruct(cp, pContext, dstOffset, srcOffset, size, pNotification);

			if (cp.size >= MinJobCopySize)
			{
				// Boost the src region so it will remain valid during the copy
				IncreaseRange(srcOffset, size);

				cp.jobState = 1;
				cryAsyncMemcpy(m_pBaseAddress + cp.dstOffset, m_pBaseAddress + cp.srcOffset, cp.size, MC_CPU_TO_CPU, &cp.jobState);
			}
			else
			{
				memcpy(m_pBaseAddress + cp.dstOffset, m_pBaseAddress + cp.srcOffset, cp.size);
				cp.pNotification->bDstIsValid = true;
			}

			--m_numAvailableCopiesInFlight;
			m_bytesInFlight += cp.size;
			m_nLastCopyIdx = nIdx;

			return nIdx + 1;
		}
	}

	return 0;
}

void CControllerDefragHeap::Relocate(uint32 userMoveId, void* pContext, UINT_PTR newOffset, UINT_PTR oldOffset, UINT_PTR size)
{
	Copy& cp = m_copiesInFlight[userMoveId - 1];
	if (cp.inUse && (cp.pContext == pContext))
	{
#ifndef _RELEASE
		if (cp.relocated)
			__debugbreak();
#endif

		char* pNewBase = m_pBaseAddress + newOffset;
		char* pOldBase = m_pBaseAddress + oldOffset;

		for (IControllerRelocatableChain* rbc = (IControllerRelocatableChain*)pContext; rbc; rbc = rbc->GetNext())
			rbc->Relocate(pNewBase, pOldBase);

		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
		MEMREPLAY_SCOPE_REALLOC(m_pBaseAddress + oldOffset, m_pBaseAddress + newOffset, size, MinAlignment);

		cp.relocateFrameId = m_tickId;
		cp.relocated = true;
		return;
	}

#ifndef _RELEASE
	__debugbreak();
#endif
}

void CControllerDefragHeap::CancelCopy(uint32 userMoveId, void* pContext, bool bSync)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_queuedCancelLock);
#ifndef _RELEASE
	if (m_numQueuedCancels == CRY_ARRAY_COUNT(m_queuedCancels))
		__debugbreak();
#endif
	assert(m_numQueuedCancels < CRY_ARRAY_COUNT(m_queuedCancels));
	m_queuedCancels[m_numQueuedCancels++] = userMoveId;
}

void CControllerDefragHeap::SyncCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size)
{
	// Not supported - but shouldn't be needed.
	__debugbreak();
}

bool CControllerDefragHeap::IncreaseRange(UINT_PTR offs, UINT_PTR sz)
{
	UINT_PTR basePageIdx = offs >> m_nLogPageSize;
	UINT_PTR lastPageIdx = (offs + sz + m_nPageSize - 1) >> m_nLogPageSize;

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_pagesLock);

	for (UINT_PTR pageIdx = basePageIdx; pageIdx != lastPageIdx; ++pageIdx)
	{
		if ((++m_numAllocsPerPage[pageIdx]) == 1)
		{
			if (!m_pAddressRange->MapPage(pageIdx))
			{
				// Failed to allocate. Unwind mapping.
				DecreaseRange_Locked(offs, (pageIdx - basePageIdx) * m_nPageSize);
				return false;
			}
		}
	}

	return true;
}

void CControllerDefragHeap::DecreaseRange(UINT_PTR offs, UINT_PTR sz)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_pagesLock);
	DecreaseRange_Locked(offs, sz);
}

void CControllerDefragHeap::DecreaseRange_Locked(UINT_PTR offs, UINT_PTR sz)
{
	UINT_PTR basePageIdx = offs >> m_nLogPageSize;
	UINT_PTR lastPageIdx = (offs + sz + m_nPageSize - 1) >> m_nLogPageSize;

	for (UINT_PTR pageIdx = basePageIdx; pageIdx != lastPageIdx; ++pageIdx)
	{
		if (!(--m_numAllocsPerPage[pageIdx]))
			m_pAddressRange->UnmapPage(pageIdx);
	}
}

void* CControllerDefragHeap::FixedAlloc(size_t sz, bool bFromGPH)
{
	PREFAST_SUPPRESS_WARNING(6240) // did not intend to use bitwise-and, its just a bool check
	if (bFromGPH && AllowGPHFallback)
	{
		FixedHdr* p = (FixedHdr*)CryModuleMalloc(sz + sizeof(FixedHdr));
		if (p)
		{
			CryInterlockedAdd(&m_nBytesInFixedAllocs, (int)sz);

			p->size = sz;
			p->bFromGPH = true;
			return p + 1;
		}
	}

	FixedHdr* p = (FixedHdr*)CryGetIMemoryManager()->AllocPages(sz + sizeof(FixedHdr));
	if (p)
	{
		CryInterlockedAdd(&m_nBytesInFixedAllocs, (int)sz);

		p->size = sz;
		p->bFromGPH = false;
		return p + 1;
	}
	else
	{
		return NULL;
	}
}

void CControllerDefragHeap::FixedFree(void* p)
{
	FixedHdr* fh = ((FixedHdr*)p) - 1;
	CryInterlockedAdd(&m_nBytesInFixedAllocs, -(int)fh->size);

	PREFAST_SUPPRESS_WARNING(6239) // did not intend to use bitwise-and, its just a bool check
	if (AllowGPHFallback && fh->bFromGPH)
		CryModuleFree(fh);
	else
		CryGetIMemoryManager()->FreePages(fh, (size_t)fh->size);
}

void CControllerDefragHeap::UpdateInflight(int frameId)
{
	if (m_numAvailableCopiesInFlight != MaxInFlightCopies)
	{
		uint32 pQueuedCancels[MaxInFlightCopies];
		int nQueuedCancels = 0;

		{
			CryAutoLock<CryCriticalSectionNonRecursive> lock(m_queuedCancelLock);
			memcpy(pQueuedCancels, m_queuedCancels, m_numQueuedCancels * sizeof(pQueuedCancels[0]));
			nQueuedCancels = m_numQueuedCancels;
			m_numQueuedCancels = 0;
		}

		std::sort(pQueuedCancels, pQueuedCancels + nQueuedCancels);
		nQueuedCancels = (int)std::distance(pQueuedCancels, std::unique(pQueuedCancels, pQueuedCancels + nQueuedCancels));

		for (int32 i = 0; i < MaxInFlightCopies; ++i)
		{
			Copy& cp = m_copiesInFlight[i];

			if (cp.inUse)
			{
				if (std::binary_search(pQueuedCancels, pQueuedCancels + nQueuedCancels, (uint32)(i + 1)))
					cp.cancelled = true;

				bool bDone = false;

				if (cp.jobState >= 0)
				{
					if (cp.jobState == 0)
					{
						// Undo the boost for the copy
						DecreaseRange(cp.srcOffset, cp.size);
						cp.pNotification->bDstIsValid = true;
						cp.jobState = -1;
					}
				}
				else if (cp.relocateFrameId)
				{
					if ((frameId - cp.relocateFrameId) >= CompletionLatencyFrames)
					{
						cp.pNotification->bSrcIsUnneeded = true;
						cp.relocateFrameId = 0;
						bDone = true;
					}
				}
				else if (cp.cancelled)
				{
					cp.pNotification->bSrcIsUnneeded = true;
					bDone = true;
				}

				if (bDone)
				{
					if (cp.cancelled && !cp.relocated)
						DecreaseRange(cp.dstOffset, cp.size);
					else
						DecreaseRange(cp.srcOffset, cp.size);

					cp.inUse = false;
					++m_numAvailableCopiesInFlight;
					m_bytesInFlight -= cp.size;
				}
			}
		}
	}
}
