// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef BUCKETALLOCATORIMPL_H
#define BUCKETALLOCATORIMPL_H

#include "BucketAllocator.h"
#include <CryCore/BitFiddling.h>

#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)

	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		#include <sys/mman.h> //mmap, munmap
	#endif

	#define PROFILE_BUCKET_CLEANUP 0

//#define BUCKET_ALLOCATOR_MAP_DOWN
	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT) || CRY_PLATFORM_ANDROID
		#define BUCKET_ALLOCATOR_4K
	#endif

namespace
{
template<typename T>
void SortLL(T*& root)
{
	if (!root)
		return;

	int k = 1;
	T* p;

	for (k = 1;; k <<= 1)
	{
		T* nl = root;
		T** c = &root;

		int mrgr = 0;

		do
		{
			T* a = nl;
			T* b = nl;

			for (int i = k; i && b; --i, p = b, b = b->next)
				;
			p->next = NULL;

			nl = b;

			for (int i = k; i && nl; --i, p = nl, nl = nl->next)
				;
			p->next = NULL;

			while (a && b)
			{
				if (a < b)
				{
					*c = a;
					a = a->next;
				}
				else
				{
					*c = b;
					b = b->next;
				}

				c = (T**) &((*c)->next);
			}

			if (a)
			{
				*c = a;

				for (p = a; p->next; p = p->next)
					;
				c = (T**) &p->next;
			}
			else if (b)
			{
				*c = b;

				for (p = b; p->next; p = p->next)
					;
				c = (T**) &p->next;
			}

			++mrgr;
		}
		while (nl);

		if (mrgr == 1)
			break;
	}
}
}

	#if BUCKET_ALLOCATOR_DEBUG
		#pragma optimize("",off)
	#endif

template<typename TraitsT>
BucketAllocator<TraitsT>::BucketAllocator(void* baseAddress, bool allowExpandCleanups, bool cleanupOnDestruction)
	: m_disableExpandCleanups(allowExpandCleanups == false)
	, m_cleanupOnDestruction(cleanupOnDestruction)
{
	if (baseAddress)
	{
		memset(m_segmentsHot, 0, sizeof(m_segmentsHot));
		memset(m_segmentsCold, 0, sizeof(m_segmentsCold));
		m_segmentsHot[0].m_baseAddress = reinterpret_cast<UINT_PTR>(baseAddress);
		m_numSegments = 1;
	}
	else
	{
		m_numSegments = 0;
	}
}

template<typename TraitsT>
BucketAllocator<TraitsT>::~BucketAllocator()
{
	if (m_cleanupOnDestruction)
	{
		SegmentHot* pHot = m_segmentsHot;
		SegmentCold* pCold = m_segmentsCold;

		for (int segIdx = 0, segCount = m_numSegments; segIdx != segCount; ++segIdx, ++pHot, ++pCold)
		{
			for (uint32 mapCount = sizeof(pCold->m_pageMap) / sizeof(pCold->m_pageMap[0]), mapIdx = 0; mapIdx != mapCount; ++mapIdx)
			{
				while (pCold->m_pageMap[mapIdx])
				{
					uint32 mapVal = pCold->m_pageMap[mapIdx];

					// Find index of 1 bit in mapVal - aka a live page
					mapVal = mapVal & ((~mapVal) + 1);
					mapVal = IntegerLog2(mapVal);
					UINT_PTR mapAddress = pHot->m_baseAddress + PageLength * (mapIdx * 32 + mapVal);
					pCold->m_pageMap[mapIdx] &= ~(1 << mapVal);

					this->UnMap(mapAddress);
				}
			}

			this->UnreserveAddressSpace(pHot->m_baseAddress, NumPages, PageLength);
		}
	}
}

template<typename TraitsT>
typename BucketAllocator<TraitsT>::FreeBlockHeader * BucketAllocator<TraitsT>::CreatePage(FreeBlockHeader * after)
{
	using namespace BucketAllocatorDetail;

	Page* page = reinterpret_cast<Page*>(this->AllocatePageStorage());
	if (!page)
	{
		return NULL;
	}

	FreeBlockHeader* freeHdr = reinterpret_cast<FreeBlockHeader*>((&page->hdr) + 1);

	memset(&page->hdr, 0, sizeof(page->hdr));

	freeHdr->start = reinterpret_cast<UINT_PTR>(freeHdr);
	freeHdr->end = reinterpret_cast<UINT_PTR>(page + 1);
	freeHdr->prev = after;
	if (after)
	{
		freeHdr->next = after->next;
		after->next = freeHdr;
	}
	else
	{
		freeHdr->next = NULL;
		m_freeBlocks = freeHdr;
	}

	#if CAPTURE_REPLAY_LOG
	CryGetIMemReplay()->MarkBucket(-1, 8, &page->hdr, sizeof(PageHeader));
	CryGetIMemReplay()->MarkBucket(-3, 4, freeHdr, freeHdr->end - freeHdr->start);
	#endif

	return freeHdr;
}

template<typename TraitsT>
bool BucketAllocator<TraitsT >::DestroyPage(Page* page)
{
	BucketAssert(IsInAddressRange(page));

	if (DeallocatePageStorage(page))
	{
	#if CAPTURE_REPLAY_LOG
		CryGetIMemReplay()->UnMarkBucket(-1, &page->hdr);
	#endif

		return true;
	}

	return false;
}

template<typename TraitsT>
bool BucketAllocator<TraitsT >::Refill(uint8 bucket)
{
	using namespace BucketAllocatorDetail;

	typename SyncingPolicy::RefillLock lock(*this);

	// Validate that something else hasn't refilled this bucket whilst waiting for the lock

	for (size_t flIdx = bucket * NumGenerations, flIdxEnd = flIdx + NumGenerations; flIdx != flIdxEnd; ++flIdx)
	{
		if (!SyncingPolicy::IsFreeListEmpty(m_freeLists[flIdx]))
			return true;
	}

	size_t itemSize = TraitsT::GetSizeForBucket(bucket);

	bool useForward;

	#ifdef BUCKET_ALLOCATOR_PACK_SMALL_SIZES
	UINT_PTR alignmentMask = 0;
	#else
	UINT_PTR alignmentMask = MEMORY_ALLOCATION_ALIGNMENT - 1;
	#endif

	if ((SmallBlockLength % itemSize) == 0)
	{
		useForward = false;
	}
	else if ((itemSize % 16) == 0)
	{
		useForward = true;

		// For allocs whose size is a multiple of 16, ensure they are aligned to 16 byte boundary, in case any aligned
		// XMVEC types are members
		alignmentMask = 15;
	}
	else if ((itemSize % 8) == 0)
	{
		useForward = true;
	}
	else
	{
		useForward = false;
	}

	size_t numItems;
	FreeBlockHeader* fbh = FindFreeBlock(useForward, alignmentMask, itemSize, numItems);
	if (!numItems)
	{
		// Failed to find a matching free block - to avoid allocating more from the system, try and garbage collect some space.
		if (!m_disableExpandCleanups)
		{
			CleanupInternal(false);
			fbh = FindFreeBlock(useForward, alignmentMask, itemSize, numItems);
		}

		if (!numItems)
		{
			// Cleanup failed to yield any small blocks that can be used for this refill, so grab another page from the OS.
			if (!CreatePage(fbh))
			{
				return false;
			}

			fbh = FindFreeBlock(useForward, alignmentMask, itemSize, numItems);
		}
	}

	BucketAssert(IsInAddressRange((void*)fbh->start));
	BucketAssert(IsInAddressRange((void*)(fbh->end - 1)));

	UINT_PTR segments[4];

	segments[0] = fbh->start;
	segments[3] = fbh->end;

	if (useForward)
	{
		segments[1] = fbh->start;
		segments[2] = ((fbh->start + alignmentMask) & ~alignmentMask) + itemSize * numItems;
	}
	else
	{
		segments[1] = (fbh->end & SmallBlockAlignMask) - numItems * itemSize;
		segments[2] = fbh->end & SmallBlockAlignMask;
	}

	BucketAssert(fbh && IsInAddressRange((void*) fbh->start));

	UINT_PTR baseAddress = (segments[1] + alignmentMask) & ~alignmentMask;
	UINT_PTR endAddress = segments[2];
	UINT_PTR blockBase = baseAddress & PageAlignMask;

	size_t smallBlockIdx = (baseAddress - blockBase) / SmallBlockLength;
	size_t smallBlockEnd = ((endAddress - itemSize - blockBase) + SmallBlockOffsetMask) / SmallBlockLength;
	size_t numSmallBlocks = smallBlockEnd - smallBlockIdx;

	BucketAssert(useForward || !(endAddress & SmallBlockOffsetMask));
	BucketAssert(!useForward || !(baseAddress & 7));
	BucketAssert(numSmallBlocks > 0);
	BucketAssert(smallBlockIdx < SmallBlocksPerPage);
	BucketAssert(smallBlockEnd <= SmallBlocksPerPage);
	BucketAssert(baseAddress >= fbh->start);
	BucketAssert(endAddress <= fbh->end);
	BucketAssert(IsInAddressRange((void*)blockBase));
	BucketAssert(IsInAddressRange((void*)baseAddress));
	BucketAssert(IsInAddressRange((void*)(endAddress - 1)));

	#if CAPTURE_REPLAY_LOG
	CryGetIMemReplay()->UnMarkBucket(-3, fbh);
	#endif

	if (fbh->next)
		fbh->next->prev = fbh->prev;
	if (fbh->prev)
		fbh->prev->next = fbh->next;
	else
		m_freeBlocks = fbh->next;

	InsertFreeBlock(InsertFreeBlock(fbh->prev, segments[0], segments[1]), segments[2], segments[3]);

	// Can't touch fbh beyond this point.

	Page* page = reinterpret_cast<Page*>(blockBase);

	for (UINT_PTR sbId = smallBlockIdx, sbBase = baseAddress; sbId != smallBlockEnd; ++sbId)
	{
		UINT_PTR sbBaseRoundedUp = (sbBase & SmallBlockAlignMask) + SmallBlockLength;

		page->hdr.SetBucketId(sbId, bucket, endAddress > sbBaseRoundedUp);
		page->hdr.SetBaseOffset(sbId, sbBase & SmallBlockOffsetMask);

		BucketAssert(page->hdr.GetBaseOffset(sbId) == (sbBase & SmallBlockOffsetMask));

		// naive implementation
		for (; sbBase < sbBaseRoundedUp; sbBase += itemSize)
			;
	}

	#if CAPTURE_REPLAY_LOG
	{
		int alignment = (itemSize <= 32) ? 4 : 8;

		UINT_PTR sbBase = baseAddress;
		UINT_PTR sbEnd = baseAddress;

		do
		{
			for (; (sbEnd & SmallBlockAlignMask) == (sbBase & SmallBlockAlignMask); sbEnd += itemSize)
				;
			sbEnd = min(sbEnd, endAddress);

			CryGetIMemReplay()->MarkBucket(itemSize, alignment, (void*) sbBase, sbEnd - sbBase);
			sbBase = sbEnd;
		}
		while (sbBase != endAddress);
	}
	#endif

	typename SyncingPolicy::FreeListHeader& freeList = m_freeLists[bucket * NumGenerations + NumGenerations - 1];

	for (size_t item = 0; item != (numItems - 1); ++item)
	{
		AllocHeader* cur = reinterpret_cast<AllocHeader*>(baseAddress + itemSize * item);
		AllocHeader* next = reinterpret_cast<AllocHeader*>(baseAddress + itemSize * (item + 1));
		cur->next = next;

	#ifdef BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
		cur->magic = FreeListMagic;
	#endif
	}

	AllocHeader* firstItem = reinterpret_cast<AllocHeader*>(baseAddress);
	AllocHeader* lastItem = reinterpret_cast<AllocHeader*>(baseAddress + itemSize * (numItems - 1));
	lastItem->next = NULL;

	#ifdef BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
	lastItem->magic = FreeListMagic;
	#endif

	this->PushListOnto(freeList, firstItem, lastItem, numItems);

	return true;
}

template<typename TraitsT>
typename BucketAllocator<TraitsT>::FreeBlockHeader * BucketAllocator<TraitsT>::FindFreeBlock(bool useForward, UINT_PTR alignmentMask, size_t itemSize, size_t & numItems)
{
	FreeBlockHeader* fbh = m_freeBlocks;
	if (useForward)
	{
		FreeBlockHeader* prev = NULL;
		UINT_PTR minSize = static_cast<UINT_PTR>(itemSize) + alignmentMask + 1U;
		for (; fbh && (fbh->end - fbh->start) < minSize; prev = fbh, fbh = fbh->next)
			;

		if (!fbh)
		{
			numItems = 0;
			return prev;
		}

		UINT_PTR fbhStart = (fbh->start + alignmentMask) & ~alignmentMask;
		numItems = GetRefillItemCountForBucket_Spill(fbhStart, itemSize);

		size_t remainingInBlock = fbh->end - fbhStart;
		if (remainingInBlock < itemSize * numItems)
			numItems = remainingInBlock / itemSize;
	}
	else
	{
		numItems = GetRefillItemCountForBucket_LCM(itemSize);

		FreeBlockHeader* prev = NULL;
		for (; fbh && ((fbh->end & SmallBlockAlignMask) - fbh->start) < (int)itemSize; prev = fbh, fbh = fbh->next)
			;

		if (!fbh)
		{
			numItems = 0;
			return prev;
		}

		size_t remainingInBlock = (fbh->end & SmallBlockAlignMask) - fbh->start;
		if (remainingInBlock < itemSize * numItems)
			numItems = remainingInBlock / itemSize;
	}

	return fbh;
}

template<typename TraitsT>
void BucketAllocator<TraitsT >::CleanupInternal(bool sortFreeLists)
{
	using namespace BucketAllocatorDetail;

	MemoryBarrier();

	#if PROFILE_BUCKET_CLEANUP
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
	#endif

	UINT_PTR segmentBases[MaxSegments];
	int numSegments = m_numSegments;
	for (int i = 0; i < numSegments; ++i)
		segmentBases[i] = m_segmentsHot[i].m_baseAddress;

	size_t pageCapacity = this->GetBucketStoragePages();

	if (pageCapacity == 0)
		return;

	const size_t cleanupReserveCapacity = 
		(sizeof(SmallBlockCleanupInfo*) * pageCapacity) + 
		(sizeof(SmallBlockCleanupInfo) * SmallBlocksPerPage * pageCapacity);

	CleanupAllocator alloc(cleanupReserveCapacity);
	if (!alloc.IsValid())
		return;

	SmallBlockCleanupInfo** pageInfos = (SmallBlockCleanupInfo**) alloc.Calloc(pageCapacity, sizeof(SmallBlockCleanupInfo*));
	if (!pageInfos)
		return;

	AllocHeader* freeLists[NumBuckets * NumGenerations];

	for (size_t flIdx = 0; flIdx != NumBuckets * NumGenerations; ++flIdx)
		freeLists[flIdx] = SyncingPolicy::PopListOff(m_freeLists[flIdx]);

	MemoryBarrier();

	// For each small block, count the number of items that are free

	uint8 bucketsTouched[NumBuckets] = { 0 };

	for (size_t bucketIdx = 0; bucketIdx != NumBuckets; ++bucketIdx)
	{
		if (!m_bucketTouched[bucketIdx])
			continue;

		bucketsTouched[bucketIdx] = 1;

		for (size_t freeList = bucketIdx * NumGenerations, freeListEnd = freeList + NumGenerations; freeList != freeListEnd; ++freeList)
		{
			for (AllocHeader* hdr = freeLists[freeList]; hdr; hdr = hdr->next)
			{
	#ifdef BUCKET_ALLOCATOR_TRAP_FREELIST_TRAMPLING
				if (hdr->next && (!this->IsInAddressRange(hdr->next) || this->GetBucketInternal(hdr->next) != bucketIdx))
				{
					// If this fires, something has trampled the free list and caused the next pointer to point
					// either outside the allocator's address range, or into another bucket. Either way, baaad.
					__debugbreak();
				}

		#ifdef BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
				if (hdr->magic != FreeListMagic)
				{
					// If this fires, something has trampled the free list items.
					__debugbreak();
				}
		#endif
	#endif
				UINT_PTR allocPtr = reinterpret_cast<UINT_PTR>(hdr);

				int segIdx = GetSegmentForAddress(hdr, segmentBases, numSegments);
				BucketAssert(segIdx != numSegments);

				UINT_PTR segBaseAddress = segmentBases[segIdx];

				size_t cleanupPageId = segIdx * NumPages + (allocPtr - segBaseAddress) / PageLength;
				size_t sbId = (allocPtr & PageOffsetMask) / SmallBlockLength;

				SmallBlockCleanupInfo*& sbInfos = pageInfos[cleanupPageId];
				if (!sbInfos)
				{
					sbInfos = (SmallBlockCleanupInfo*) alloc.Calloc(SmallBlocksPerPage, sizeof(SmallBlockCleanupInfo));
					if (!sbInfos)
					{
#ifdef BUCKET_ALLOCATOR_TRAP_CLEANUP_OOM
						// If this fires, CleanupAllocator is misconfigured and doesn't have enough space for cleanup info
						__debugbreak();
#endif
						FreeCleanupInfo(alloc, pageInfos, pageCapacity);
						return;
					}
				}

				++sbInfos[sbId].freeItemCount;
			}
		}
	}

	// Add existing free blocks to info vec so they will get coalesced with the newly freed blocks

	for (FreeBlockHeader* fbh = m_freeBlocks; fbh; fbh = fbh->next)
	{
		int segIdx = GetSegmentForAddress(reinterpret_cast<void*>(fbh->start), segmentBases, numSegments);
		BucketAssert(segIdx != numSegments);

		UINT_PTR segBaseAddress = segmentBases[segIdx];

		size_t cleanupPageId = segIdx * NumPages + (fbh->start - segBaseAddress) / PageLength;
		UINT_PTR pageBase = fbh->start & PageAlignMask;
		UINT_PTR startSbId = (fbh->start - pageBase) / SmallBlockLength;
		UINT_PTR endSbId = (fbh->end - pageBase) / SmallBlockLength;
		SmallBlockCleanupInfo*& cleanupInfos = pageInfos[cleanupPageId];

		BucketAssert(startSbId != endSbId);

		if (!cleanupInfos)
		{
			cleanupInfos = (SmallBlockCleanupInfo*) alloc.Calloc(SmallBlocksPerPage, sizeof(SmallBlockCleanupInfo));
			if (!cleanupInfos)
			{
#ifdef BUCKET_ALLOCATOR_TRAP_CLEANUP_OOM
				// If this fires, CleanupAllocator is misconfigured and doesn't have enough space for cleanup info
				__debugbreak();
#endif
				FreeCleanupInfo(alloc, pageInfos, pageCapacity);
				return;
			}
		}

		for (UINT_PTR sbId = startSbId; sbId < endSbId; ++sbId)
		{
			SmallBlockCleanupInfo& sbInfo = cleanupInfos[sbId];
			sbInfo.cleaned = 1;
		}

	#if CAPTURE_REPLAY_LOG
		CryGetIMemReplay()->UnMarkBucket(-3, fbh);
	#endif
	}

	m_freeBlocks = NULL;

	// At this point, info should be fully populated with the details of available small blocks from the free lists.

	// Do a pass over the small block cleanup infos to mark up blocks that are going to be unbound

	for (int segId = 0; segId < numSegments; ++segId)
	{
		SegmentHot& segh = m_segmentsHot[segId];
		SegmentCold& segc = m_segmentsCold[segId];

		size_t basePageId = segId * NumPages;

		for (size_t segPageId = 0; segPageId != NumPages; ++segPageId)
		{
			SmallBlockCleanupInfo* sbInfos = pageInfos[basePageId + segPageId];

			if (!sbInfos)
				continue;

			Page* page = reinterpret_cast<Page*>(segh.m_baseAddress + PageLength * segPageId);
			PageHeader* pageHdr = &page->hdr;

			for (size_t sbId = 0; sbId < SmallBlocksPerPage; ++sbId)
			{
				SmallBlockCleanupInfo& sbInfo = sbInfos[sbId];

				if (sbInfo.cleaned)
					continue;

				size_t itemSize = TraitsT::GetSizeForBucket(page->hdr.GetBucketId(sbId));
				sbInfo.SetItemSize(itemSize);

				if (sbInfo.freeItemCount == 0)
					continue;

				uint32 maximumFreeCount = pageHdr->GetItemCountForBlock(sbId);

				BucketAssert(maximumFreeCount > 0);
				BucketAssert(sbInfo.freeItemCount <= maximumFreeCount);

				if (maximumFreeCount == sbInfo.freeItemCount)
				{
					sbInfo.cleaned = 1;

	#if CAPTURE_REPLAY_LOG
					UINT_PTR sbStart = reinterpret_cast<UINT_PTR>(&page->smallBlocks[sbId]) + page->hdr.GetBaseOffset(sbId);
					CryGetIMemReplay()->UnMarkBucket(itemSize, (void*) sbStart);
	#endif
				}
			}
		}
	}

	// Walk over the free lists again, checking each item against the page info
	// vector to determine whether it's still a valid free item and removing those that aren't.

	int unboundSize = 0;

	for (size_t bucketId = 0; bucketId != NumBuckets; ++bucketId)
	{
		if (!bucketsTouched[bucketId])
			continue;

		size_t freeListBase = bucketId * NumGenerations;

		for (size_t genId = 0; genId != NumGenerations; ++genId)
		{
			size_t freeList = freeListBase + genId;

			for (AllocHeader** hdr = &freeLists[freeList]; *hdr; )
			{
				UINT_PTR hdri = reinterpret_cast<UINT_PTR>(*hdr);
				int segId = GetSegmentForAddress(*hdr, segmentBases, numSegments);
				BucketAssert(segId != numSegments);

				size_t cleanupPageId = segId * NumPages + (hdri - segmentBases[segId]) / PageLength;
				size_t sbId = (hdri & PageOffsetMask) / SmallBlockLength;

				SmallBlockCleanupInfo* cleanupInfos = pageInfos[cleanupPageId];
				SmallBlockCleanupInfo& pageInfo = cleanupInfos[sbId];

				if (pageInfo.cleaned)
				{
					// Remove this item from the free list, as the small block that it lives in has been unbound.
					*hdr = (*hdr)->next;
					unboundSize += pageInfo.GetItemSize();
				}
				else
				{
					Page* page = reinterpret_cast<Page*>(hdri & PageAlignMask);
					BucketAssert(page);

					uint8 sbStability = min((uint8)(page->hdr.GetStability(sbId) + 1), (uint8)255);
					size_t sbGen = TraitsT::GetGenerationForStability(sbStability);

					if (sbGen != genId)
					{
						AllocHeader* item = *hdr;

						// Remove this item from this free list, as it has changed generation and should be moved to another free list.
						*hdr = item->next;

						// Push it onto the right free list
						item->next = freeLists[freeListBase + sbGen];
						freeLists[freeListBase + sbGen] = item;
					}
					else
					{
						hdr = (AllocHeader**) &(*hdr)->next;
					}
				}
			}

		}
	}

	// Finally, do another pass over the small block cleanups again and build the new free block list, now
	// that the free lists won't alias it.

	FreeBlockHeader* freeBlocks = NULL;

	for (int segId = 0; segId < numSegments; ++segId)
	{
		size_t basePageId = segId * NumPages;

		for (size_t segPageId = 0; segPageId != NumPages; ++segPageId)
		{
			SmallBlockCleanupInfo* sbInfos = pageInfos[basePageId + segPageId];

			if (!sbInfos)
				continue;

			Page* page = reinterpret_cast<Page*>(segmentBases[segId] + sizeof(Page) * segPageId);

			for (size_t sbId = 0; sbId != SmallBlocksPerPage; )
			{
				if (!sbInfos[sbId].cleaned)
				{
					page->hdr.IncrementBlockStability(sbId);
					++sbId;
					continue;
				}

				size_t startSbId = sbId;

				for (; sbId != SmallBlocksPerPage && sbInfos[sbId].cleaned; ++sbId)
				{
					page->hdr.ResetBlockStability(sbId);
				}

				UINT_PTR fbhStart = 0;

				if (startSbId > 0)
				{
					size_t baseOffset = 0;

					if (page->hdr.DoesSmallBlockSpill(startSbId - 1))
					{
						size_t sbOffset = page->hdr.GetBaseOffset(startSbId - 1);
						size_t itemSize = TraitsT::GetSizeForBucket(page->hdr.GetBucketId(startSbId - 1));
						size_t itemCount = (SmallBlockLength - sbOffset) / itemSize + 1;

						baseOffset = sbOffset + itemSize * itemCount - SmallBlockLength;
					}

					// Need to make sure that the starting edge is 8 byte aligned
					baseOffset = (baseOffset + 7) & ~7;

					fbhStart = reinterpret_cast<UINT_PTR>(&page->smallBlocks[startSbId]) + baseOffset;
				}
				else
				{
					fbhStart = reinterpret_cast<UINT_PTR>(&page->hdr + 1);
				}

				UINT_PTR fbhEnd = (sbId != SmallBlocksPerPage)
				                  ? reinterpret_cast<UINT_PTR>(&page->smallBlocks[sbId]) + page->hdr.GetBaseOffset(sbId)
				                  : reinterpret_cast<UINT_PTR>(page + 1);

				if (((fbhStart & PageOffsetMask) == sizeof(PageHeader)) && ((fbhEnd & PageOffsetMask) == 0))
				{
					// Free block extends from the end of the page header to the end of the page - i.e. the page is empty
					if (DestroyPage(reinterpret_cast<Page*>(fbhStart & PageAlignMask)))
					{
						// Don't add it to the free block list. For obvious reasons.
						continue;
					}
				}

				// Add the block to the free block list.
				FreeBlockHeader* fbh = reinterpret_cast<FreeBlockHeader*>(fbhStart);
				fbh->start = fbhStart;
				fbh->end = fbhEnd;

				BucketAssert(!(fbhStart & 7));
				BucketAssert(fbhEnd > fbhStart);

				fbh->next = NULL;
				fbh->prev = freeBlocks;
				if (freeBlocks)
					freeBlocks->next = fbh;
				else
					m_freeBlocks = fbh;
				freeBlocks = fbh;
			}
		}
	}

	#if CAPTURE_REPLAY_LOG
	for (FreeBlockHeader* fbh = m_freeBlocks; fbh; fbh = fbh->next)
		CryGetIMemReplay()->MarkBucket(-3, 4, fbh, fbh->end - fbh->start);
	#endif

	#if BUCKET_ALLOCATOR_DEBUG
	for (FreeBlockHeader* fbh = m_freeBlocks; fbh; fbh = fbh->next)
	{
		BucketAssert(IsInAddressRange(fbh));

		BucketAssert(fbh->start == reinterpret_cast<UINT_PTR>(fbh));
		BucketAssert((fbh->start & SmallBlockAlignMask) != (fbh->end & SmallBlockAlignMask));
		BucketAssert((fbh->start & PageAlignMask) == ((fbh->end - 1) & PageAlignMask));

		{
			bool foundfbh = false;
			for (size_t flid = 0; flid != NumBuckets * NumGenerations && !foundfbh; ++flid)
			{
				for (AllocHeader* ah = freeLists[flid]; ah && !foundfbh; ah = ah->next)
				{
					if ((void*)ah == (void*) fbh)
						foundfbh = true;
				}
			}
			BucketAssert(!foundfbh);
		}
	}
	#endif

	FreeCleanupInfo(alloc, pageInfos, pageCapacity);

	if (sortFreeLists)
	{
		for (size_t fl = 0; fl < NumGenerations * NumBuckets; ++fl)
		{
	#if BUCKET_ALLOCATOR_DEBUG
			int length = 0;
			for (AllocHeader* ah = freeLists[fl]; ah; ah = ah->next)
				++length;
	#endif

			AllocHeader** root = &freeLists[fl];
			SortLL(*root);

	#if BUCKET_ALLOCATOR_DEBUG
			int lengthPostSort = 0;
			for (AllocHeader* ah = freeLists[fl]; ah; ah = ah->next)
				++lengthPostSort;
			BucketAssert(length == lengthPostSort);
	#endif
		}
	}

	for (size_t flIdx = 0; flIdx < NumBuckets * NumGenerations; ++flIdx)
	{
		AllocHeader* item = freeLists[flIdx];
		AllocHeader* next;

		while (item)
		{
			next = item->next;
			item->next = NULL;

			this->PushOnto(m_freeLists[flIdx], item);

			item = next;
		}
	}

	memset((void*) m_bucketTouched, 0, sizeof(m_bucketTouched));

	#if PROFILE_BUCKET_CLEANUP
	LARGE_INTEGER end, freq;
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&freq);

	{
		char msg[256];
		cry_sprintf(msg, "[of] cleanup took %fms, unbound %i\n", (end.QuadPart - start.QuadPart) / (freq.QuadPart / 1000.0), unboundSize);
		OutputDebugString(msg);
	}
	#endif
}

template<typename TraitsT>
void BucketAllocator<TraitsT >::FreeCleanupInfo(CleanupAllocator& alloc, SmallBlockCleanupInfo** infos, size_t infoCapacity)
{
	for (size_t pageId = 0; pageId != infoCapacity; ++pageId)
	{
		if (infos[pageId])
			alloc.Free(infos[pageId]);
	}
	alloc.Free(infos);
}

template<typename TraitsT>
void BucketAllocator<TraitsT >::cleanup()
{
	typename SyncingPolicy::RefillLock lock(*this);
	CleanupInternal(true);
}

template<typename TraitsT>
void* BucketAllocator<TraitsT >::AllocatePageStorage()
{
	SegmentHot* pSegH = m_segmentsHot;
	SegmentCold* pSegC = m_segmentsCold;

	int segIdx = 0, segCount = m_numSegments;
	for (; segIdx != segCount; ++segIdx)
	{
		if (pSegC[segIdx].m_committed < PageLength * NumPages)
			break;
	}

	if (segIdx == segCount)
	{
		if (segCount != MaxSegments)
		{
			UINT_PTR baseAddress = this->ReserveAddressSpace(NumPages, PageLength);

			if (baseAddress)
			{
				memset(&pSegH[segCount], 0, sizeof(pSegH[segCount]));
				memset(&pSegC[segCount], 0, sizeof(pSegC[segCount]));
				pSegH[segCount].m_baseAddress = baseAddress;

				++segCount;
				m_numSegments = segCount;

	#if CAPTURE_REPLAY_LOG
				static int id;
				char name[64];
				cry_sprintf(name, "Bucket Segment %i", id);
				++id;
				CryGetIMemReplay()->RegisterFixedAddressRange((void*)baseAddress, NumPages * PageLength, name);
	#endif
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
	}

	SegmentHot& segh = pSegH[segIdx];
	SegmentCold& segc = pSegC[segIdx];

	void* result = NULL;

	// Search from the last page mapped onwards, wrapping if necessary

	uint32 pageMidIdx = segc.m_lastPageMapped % NumPages;

	for (uint32 pageIdx = (pageMidIdx + 1) != NumPages ? (pageMidIdx + 1) : 0, numChecked = 0;
	     numChecked < NumPages;
	     pageIdx = (pageIdx + 1) != NumPages ? (pageIdx + 1) : 0, ++numChecked)
	{
		uint32 pageCellIdx = pageIdx / 32;
		uint32 pageBitMask = 1U << (pageIdx % 32);

		BucketAssert(pageCellIdx < sizeof(segc.m_pageMap) / sizeof(segc.m_pageMap[0]));
		uint32* pPageMap = segc.m_pageMap;
		if ((pPageMap[pageCellIdx] & pageBitMask) == 0)
		{
			UINT_PTR mapAddress = segh.m_baseAddress + PageLength * pageIdx;
			result = reinterpret_cast<void*>(this->Map(mapAddress, PageLength));
			if (!result)
				return NULL;

			pPageMap[pageCellIdx] |= pageBitMask;
			segc.m_lastPageMapped = pageIdx;
			break;
		}
	}

	if (!result)
	{
		return NULL;
	}

	segc.m_committed += PageLength;

	#if CAPTURE_REPLAY_LOG
	CryGetIMemReplay()->MapPage(result, PageLength);
	#endif

	return result;
}

template<typename TraitsT>
bool BucketAllocator<TraitsT >::DeallocatePageStorage(void* ptr)
{
	SegmentHot* pSegH = m_segmentsHot;
	SegmentCold* pSegC = m_segmentsCold;

	int segIdx = 0, segCount = m_numSegments;
	for (; segIdx != segCount; ++segIdx)
	{
		UINT_PTR ba = pSegH[segIdx].m_baseAddress;
		if (ba <= reinterpret_cast<UINT_PTR>(ptr) && reinterpret_cast<UINT_PTR>(ptr) < (ba + NumPages * PageLength))
			break;
	}

	if (segIdx != segCount)
	{
		SegmentHot& segh = pSegH[segIdx];
		SegmentCold& segc = pSegC[segIdx];

		this->UnMap(reinterpret_cast<UINT_PTR>(ptr));

		UINT_PTR pageId = (reinterpret_cast<UINT_PTR>(ptr) - segh.m_baseAddress) / PageLength;
		segc.m_pageMap[pageId / 32] &= ~(1 << (pageId % 32));

		segc.m_committed -= PageLength;

	#if CAPTURE_REPLAY_LOG
		CryGetIMemReplay()->UnMapPage(ptr, PageLength);
	#endif
	}

	return true;
}

template<typename TraitsT>
size_t BucketAllocator<TraitsT >::GetBucketStorageSize()
{
	SegmentCold* pSegC = m_segmentsCold;

	size_t sz = 0;
	for (int i = 0, c = m_numSegments; i != c; ++i)
		sz += pSegC[i].m_committed;
	return sz;
}

template<typename TraitsT>
size_t BucketAllocator<TraitsT >::GetBucketStoragePages()
{
	return (size_t)m_numSegments * NumPages;
}

template<typename TraitsT>
size_t BucketAllocator<TraitsT >::GetBucketConsumedSize()
{
	#ifdef BUCKET_ALLOCATOR_TRACK_CONSUMED
	return m_consumed;
	#else
	return 0;
	#endif
}

	#if CRY_PLATFORM_WINAPI

inline UINT_PTR BucketAllocatorDetail::SystemAllocator::ReserveAddressSpace(size_t numPages, size_t pageLen)
{
	BucketAssert(pageLen == 64 * 1024);
		#ifdef BUCKET_ALLOCATOR_4K
	UINT_PTR addr;

	do
	{
		addr = reinterpret_cast<UINT_PTR>(VirtualAlloc(NULL, (numPages + 1) * pageLen, MEM_RESERVE, PAGE_READWRITE));
		if (!addr)
			break;

		UINT_PTR alignedAddr = (addr + pageLen - 1) & ~(pageLen - 1);

		if ((alignedAddr - addr) > 0)
		{
			VirtualFree(reinterpret_cast<LPVOID>(addr), 0, MEM_RELEASE);
			addr = reinterpret_cast<UINT_PTR>(VirtualAlloc(reinterpret_cast<LPVOID>(alignedAddr), numPages * pageLen, MEM_RESERVE, PAGE_READWRITE));
		}
	}
	while (!addr);

	return addr;
		#else
	return reinterpret_cast<UINT_PTR>(VirtualAlloc(NULL, numPages * pageLen, MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE));
		#endif
}

inline void BucketAllocatorDetail::SystemAllocator::UnreserveAddressSpace(UINT_PTR base, size_t numPages, size_t pageLen)
{
	VirtualFree(reinterpret_cast<LPVOID>(base), 0, MEM_RELEASE);
}

inline UINT_PTR BucketAllocatorDetail::SystemAllocator::Map(UINT_PTR base, size_t len)
{
	BucketAssert(len == 64 * 1024);
		#ifdef BUCKET_ALLOCATOR_4K
	return (UINT_PTR) VirtualAlloc(reinterpret_cast<LPVOID>(base), len, MEM_COMMIT, PAGE_READWRITE);
		#else
	return (UINT_PTR) VirtualAlloc(reinterpret_cast<LPVOID>(base), len, MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
		#endif
}

inline void BucketAllocatorDetail::SystemAllocator::UnMap(UINT_PTR addr)
{
	// Disable warning about only decommitting pages, and not releasing them
		#pragma warning( push )
		#pragma warning( disable : 6250 )
	VirtualFree(reinterpret_cast<LPVOID>(addr), 64 * 1024, MEM_DECOMMIT);
		#pragma warning( pop )
}

inline BucketAllocatorDetail::SystemAllocator::CleanupAllocator::CleanupAllocator(size_t reserveCapacity)
	: m_reserveCapacity(reserveCapacity)
{
	m_base = VirtualAlloc(NULL, reserveCapacity, MEM_RESERVE, PAGE_READWRITE);
	m_end = m_base;
}

inline BucketAllocatorDetail::SystemAllocator::CleanupAllocator::~CleanupAllocator()
{
	if (m_base != NULL)
		VirtualFree(m_base, 0, MEM_RELEASE);
}

inline bool BucketAllocatorDetail::SystemAllocator::CleanupAllocator::IsValid() const
{
	return m_base != NULL;
}

inline void* BucketAllocatorDetail::SystemAllocator::CleanupAllocator::Calloc(size_t num, size_t sz)
{
	void* result = NULL;
	size_t size = num * sz;

	UINT_PTR base = reinterpret_cast<UINT_PTR>(m_base);
	UINT_PTR end = reinterpret_cast<UINT_PTR>(m_end);
	if (end + size <= (base + m_reserveCapacity))
	{
		UINT_PTR endAligned = (end + 4095) & ~4095;
		UINT_PTR sizeNeeded = ((end + size - endAligned) + 4095) & ~4095;

		if (sizeNeeded)
		{
			if (!VirtualAlloc(reinterpret_cast<void*>(endAligned), sizeNeeded, MEM_COMMIT, PAGE_READWRITE))
				return NULL;
		}

		result = m_end;
		m_end = reinterpret_cast<void*>(end + size);
	}

	return result;
}

inline void BucketAllocatorDetail::SystemAllocator::CleanupAllocator::Free(void* ptr)
{
	// Will be freed automatically when the allocator is destroyed
}

	#elif CRY_PLATFORM_ORBIS

		#define BUCKET_ALLOCATOR_PAGE_SIZE (64 * 1024)

inline UINT_PTR BucketAllocatorDetail::SystemAllocator::ReserveAddressSpace(size_t numPages, size_t pageLen)
{
	BucketAssert(pagelen == BUCKET_ALLOCATOR_PAGE_SIZE);
	return (UINT_PTR)VirtualAllocator::AllocateVirtualAddressSpace(numPages * pageLen);
}

inline void BucketAllocatorDetail::SystemAllocator::UnreserveAddressSpace(UINT_PTR base, size_t numPages, size_t pageLen)
{
	(void) numPages;
	(void) pageLen;
	VirtualAllocator::FreeVirtualAddressSpace((void*)base);
}

inline UINT_PTR BucketAllocatorDetail::SystemAllocator::Map(UINT_PTR base, size_t len)
{
	BucketAssert(len == BUCKET_ALLOCATOR_PAGE_SIZE);
	return (UINT_PTR)VirtualAllocator::MapPage((void*)base, BUCKET_ALLOCATOR_PAGE_SIZE);
}

inline void BucketAllocatorDetail::SystemAllocator::UnMap(UINT_PTR addr)
{
	VirtualAllocator::UnmapPage((void*)addr, BUCKET_ALLOCATOR_PAGE_SIZE);
}

inline BucketAllocatorDetail::SystemAllocator::CleanupAllocator::CleanupAllocator(size_t reserveCapacity)
	: m_reserveCapacity(reserveCapacity)
{
	m_base = VirtualAllocator::AllocateVirtualAddressSpace(reserveCapacity);
	if (!m_base)
		__debugbreak();
	m_end = m_base;
}

inline BucketAllocatorDetail::SystemAllocator::CleanupAllocator::~CleanupAllocator()
{
	VirtualAllocator::FreeVirtualAddressSpace(m_base);
}

inline void* BucketAllocatorDetail::SystemAllocator::CleanupAllocator::Calloc(size_t num, size_t sz)
{
	void* result = NULL;
	size_t size = num * sz;

	UINT_PTR base = reinterpret_cast<UINT_PTR>(m_base);
	UINT_PTR end = reinterpret_cast<UINT_PTR>(m_end);
	if (end + size <= (base + m_reserveCapacity))
	{
		UINT_PTR endAligned = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		UINT_PTR sizeNeeded = ((end + size - endAligned) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

		if (sizeNeeded)
		{
			for (UINT_PTR i = 0; i < sizeNeeded; i += PAGE_SIZE)
			{
				if (!VirtualAllocator::MapPage((void*)(endAligned + i)))
					return NULL;
			}
		}

		result = m_end;
		m_end = reinterpret_cast<void*>(end + size);
	}

	return result;
}

inline void BucketAllocatorDetail::SystemAllocator::CleanupAllocator::Free(void* ptr)
{
	// Will be freed automatically when the allocator is destroyed
}

inline bool BucketAllocatorDetail::SystemAllocator::CleanupAllocator::IsValid() const
{
	return m_base != NULL;
}

	#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
inline UINT_PTR BucketAllocatorDetail::SystemAllocator::ReserveAddressSpace(size_t numPages, size_t pageLen)
{
	BucketAssert(pageLen == 64 * 1024);
	//#ifdef BUCKET_ALLOCATOR_4K
	UINT_PTR addr;

	do
	{
		addr = reinterpret_cast<UINT_PTR>(mmap(NULL, (numPages + 1) * pageLen, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
		if (!addr)
			break;

		#if CRY_PLATFORM_APPLE
		// mmap on Mac OS X does not allocate on the requested allgined address,
		// so we have to fix it manually.

		UINT_PTR alignedAddr = (addr + pageLen) & ~(pageLen - 1);
		size_t diff = alignedAddr - addr;

		// Give read and write access to that region in order to store the
		// Real address allocated by mmap.
		mprotect(reinterpret_cast<LPVOID>(addr), diff, PROT_READ | PROT_WRITE);

		// Store the address
		size_t* pAdjustment = ((size_t*)alignedAddr) - 1;
		*pAdjustment = diff;
		// Revert to read only.
		mprotect(reinterpret_cast<LPVOID>(addr), diff, PROT_READ);
		return alignedAddr;
		#else
		UINT_PTR alignedAddr = (addr + pageLen - 1) & ~(pageLen - 1);
		if ((alignedAddr - addr) > 0)
		{
			int ret = munmap(reinterpret_cast<LPVOID>(addr), (numPages + 1) * pageLen);
			(void) ret;
			assert(ret == 0);
			addr = reinterpret_cast<UINT_PTR>(mmap(reinterpret_cast<LPVOID>(alignedAddr), numPages * pageLen, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
		}
		#endif
	}
	while (!addr);
	return addr;
	//#else
	//	return reinterpret_cast<UINT_PTR>(mmap(NULL, numPages * pageLen, PROT_NONE, MAP_ANONYMOUS|MAP_NORESERVE|MAP_PRIVATE, -1, 0));
	//#endif
}

inline void BucketAllocatorDetail::SystemAllocator::UnreserveAddressSpace(UINT_PTR base, size_t numPages, size_t pageLen)
{
	// TODO: -1?
		#if CRY_PLATFORM_APPLE
	size_t* pAdjustment = ((size_t*) base) - 1;
	base -= *pAdjustment;
	int ret = munmap(reinterpret_cast<LPVOID>(base), (numPages + 1) * pageLen);
		#else
	int ret = munmap(reinterpret_cast<LPVOID>(base), numPages * pageLen);
		#endif
	(void) ret;
	assert(ret == 0);
}

inline UINT_PTR BucketAllocatorDetail::SystemAllocator::Map(UINT_PTR base, size_t len)
{
	BucketAssert(len == 64 * 1024);
	/*if (MAP_FAILED == mmap(reinterpret_cast<LPVOID>(base),len, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0))
	   return 0;

	   return base;*/
	if (0 == mprotect(reinterpret_cast<LPVOID>(base), len, PROT_READ | PROT_WRITE))
	{
		return base;
	}
	else
	{
		return 0;
	}
}

inline void BucketAllocatorDetail::SystemAllocator::UnMap(UINT_PTR addr)
{
	//int ret = munmap( reinterpret_cast<LPVOID>(addr),64 * 1024 ); // TODO: -1?
	int ret = mprotect(reinterpret_cast<LPVOID>(addr), 64 * 1024, PROT_NONE);
	(void) ret;
	assert(ret == 0);
}

inline BucketAllocatorDetail::SystemAllocator::CleanupAllocator::CleanupAllocator(size_t reserveCapacity)
	: m_reserveCapacity(reserveCapacity)
{
	m_base = reinterpret_cast<void*>(mmap(NULL, reserveCapacity, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
	assert(m_base != MAP_FAILED);
	m_end = m_base;
}

inline BucketAllocatorDetail::SystemAllocator::CleanupAllocator::~CleanupAllocator()
{
	if (m_base != NULL)
	{
		int ret = munmap(m_base, m_reserveCapacity);  // TODO: -1?
		(void) ret;
		assert(ret == 0);
	}
}

inline bool BucketAllocatorDetail::SystemAllocator::CleanupAllocator::IsValid() const
{
	return m_base != NULL;
}

inline void* BucketAllocatorDetail::SystemAllocator::CleanupAllocator::Calloc(size_t num, size_t sz)
{
	void* result = NULL;
	size_t size = num * sz;

	UINT_PTR base = reinterpret_cast<UINT_PTR>(m_base);
	UINT_PTR end = reinterpret_cast<UINT_PTR>(m_end);
	if (end + size <= (base + m_reserveCapacity))
	{
		UINT_PTR endAligned = (end + 4095) & ~4095;
		UINT_PTR sizeNeeded = ((end + size - endAligned) + 4095) & ~4095;

		if (sizeNeeded)
		{
			if (MAP_FAILED == mmap(reinterpret_cast<void*>(endAligned), sizeNeeded, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
				return NULL;
		}

		result = m_end;
		m_end = reinterpret_cast<void*>(end + size);
	}

	return result;
}

inline void BucketAllocatorDetail::SystemAllocator::CleanupAllocator::Free(void* ptr)
{
	// Will be freed automatically when the allocator is destroyed
}
	#endif

#endif

#endif
