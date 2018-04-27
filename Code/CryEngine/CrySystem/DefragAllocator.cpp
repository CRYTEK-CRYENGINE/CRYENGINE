// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DefragAllocator.h"

#ifdef CDBA_MORE_DEBUG
	#pragma optimize("",off)
#endif

CDefragAllocatorWalker::CDefragAllocatorWalker(CDefragAllocator& alloc)
{
	m_pAlloc = &alloc;
	m_pAlloc->m_lock.Lock();

	m_nChunkIdx = CDefragAllocator::AddrStartSentinal;
}

CDefragAllocatorWalker::~CDefragAllocatorWalker()
{
	m_pAlloc->m_lock.Unlock();
}

const SDefragAllocChunk* CDefragAllocatorWalker::Next()
{
	m_nChunkIdx = m_pAlloc->m_chunks[m_nChunkIdx].addrNextIdx;
	if (m_nChunkIdx == CDefragAllocator::AddrEndSentinal)
		return NULL;
	return &m_pAlloc->m_chunks[m_nChunkIdx];
}

CDefragAllocator::CDefragAllocator()
	: m_isThreadSafe(false)
	, m_chunksAreFixed(false)
	, m_capacity(0)
	, m_available(0)
	, m_numAllocs(0)
	, m_minAlignment(1)
	, m_logMinAlignment(0)
	, m_nCancelledMoves(0)
{
}

CDefragAllocator::~CDefragAllocator()
{
}

void CDefragAllocator::Release(bool bDiscard)
{
	if (!bDiscard)
	{
		if (m_policy.pDefragPolicy)
			Defrag_CompletePendingMoves();

		for (PendingMoveVec::iterator it = m_pendingMoves.begin(), itEnd = m_pendingMoves.end(); it != itEnd; ++it)
			CDBA_ASSERT(it->dstChunkIdx == InvalidChunkIdx);
	}

	delete this;
}

void CDefragAllocator::Init(UINT_PTR capacity, UINT_PTR minAlignment, const Policy& policy)
{
	CDBA_ASSERT((capacity & (minAlignment - 1)) == 0);
	CDBA_ASSERT((minAlignment & (minAlignment - 1)) == 0);
	CDBA_ASSERT(!policy.pDefragPolicy || policy.maxAllocs > 0);
	CDBA_ASSERT((capacity / minAlignment) < (1 << SDefragAllocChunkAttr::SizeWidth));

	uint32 logMinAlignment = (uint32)IntegerLog2(minAlignment);

	capacity >>= logMinAlignment;

	CDBA_ASSERT(capacity <= (uint32) - 1);

	m_isThreadSafe = policy.pDefragPolicy != NULL;

	m_capacity = (uint32)capacity;
	m_available = (uint32)capacity;
	m_numAllocs = 0;

	m_minAlignment = (uint32)minAlignment;
	m_logMinAlignment = logMinAlignment;

	m_chunks.clear();
	m_unusedChunks.clear();

	m_policy = policy;
	if (policy.pDefragPolicy)
		m_pendingMoves.resize(MaxPendingMoves);

	m_chunks.reserve(NumBuckets + 3);

	// Create sentinal chunk for by-address list
	{
		SDefragAllocChunk addrStartSentinal = { 0 };
		addrStartSentinal.addrNextIdx = 1;
		addrStartSentinal.addrPrevIdx = 1;
		addrStartSentinal.attr.SetBusy(true);
		addrStartSentinal.ptr = 0;
		m_chunks.push_back(addrStartSentinal);

		SDefragAllocChunk addrEndSentinal = { 0 };
		addrEndSentinal.addrNextIdx = 0;
		addrEndSentinal.addrPrevIdx = 0;
		addrEndSentinal.attr.SetBusy(true);
		addrEndSentinal.ptr = m_capacity;
		m_chunks.push_back(addrEndSentinal);
	}

	SDefragAllocSegment seg;
	seg.address = 0;
	seg.capacity = (uint32)capacity;
	seg.headSentinalChunkIdx = AddrStartSentinal;

	// Create sentinal chunks for the bucket lists
	for (size_t bucketIdx = 0; bucketIdx < NumBuckets; ++bucketIdx)
	{
		Index chunkIdx = bucketIdx + 2;

		SDefragAllocChunk chunk = { 0 };
		chunk.attr.SetBusy(true);
		chunk.freeNextIdx = chunkIdx;
		chunk.freePrevIdx = chunkIdx;

		m_freeBuckets[bucketIdx] = chunkIdx;
		m_chunks.push_back(chunk);
	}

	Index totalChunkIdx = AllocateChunk();
	SDefragAllocChunk& totalChunk = m_chunks[totalChunkIdx];
	totalChunk.attr.SetSize((unsigned int)capacity);
	totalChunk.attr.SetPinCount(0);
	totalChunk.attr.SetMoving(0);
	LinkAddrChunk(totalChunkIdx, 0);
	LinkFreeChunk(totalChunkIdx);

	if (policy.maxAllocs > 0)
	{
		size_t curChunksSize = m_chunks.size();
		m_chunks.resize(policy.maxAllocs);
		for (Index i = (Index)curChunksSize, c = (Index)m_chunks.size(); i < c; ++i)
			m_unusedChunks.push_back(i);
	}

	m_chunksAreFixed = policy.pDefragPolicy != NULL;

	m_segments.reserve(policy.maxSegments);
	m_segments.push_back(seg);

#ifdef CDBA_MORE_DEBUG
	m_nLastCheckedChunk = 0;
#endif
}

#ifndef _RELEASE
void CDefragAllocator::DumpState(const char* filename)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);

	FILE* fp = fopen(filename, "wb");
	if (fp)
	{
		uint32 magic = 0xdef7a6e7;
		fwrite(&magic, sizeof(magic), 1, fp);

		fwrite(&m_capacity, sizeof(m_capacity), 1, fp);
		fwrite(&m_available, sizeof(m_available), 1, fp);
		fwrite(&m_numAllocs, sizeof(m_numAllocs), 1, fp);
		fwrite(&m_minAlignment, sizeof(m_minAlignment), 1, fp);
		fwrite(&m_logMinAlignment, sizeof(m_logMinAlignment), 1, fp);
		fwrite(&m_freeBuckets[0], sizeof(m_freeBuckets), 1, fp);

		uint32 numChunks = (uint32)m_chunks.size();
		fwrite(&numChunks, sizeof(numChunks), 1, fp);
		fwrite(&m_chunks[0], sizeof(m_chunks[0]), numChunks, fp);
		uint32 numUnused = (uint32)m_unusedChunks.size();
		fwrite(&numUnused, sizeof(numUnused), 1, fp);
		fwrite(&m_unusedChunks[0], sizeof(m_unusedChunks[0]), numUnused, fp);
		uint32 numPending = (uint32)m_pendingMoves.size();
		fwrite(&numPending, sizeof(numPending), 1, fp);
		fwrite(&m_pendingMoves[0], sizeof(m_pendingMoves[0]), numPending, fp);
		uint32 numSegs = (uint32)m_segments.size();
		fwrite(&numSegs, sizeof(numSegs), 1, fp);
		fwrite(&m_segments[0], sizeof(m_segments[0]), numSegs, fp);

		fclose(fp);
	}
}

void CDefragAllocator::RestoreState(const char* filename)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);

	FILE* fp = fopen(filename, "rb");
	if (fp)
	{
		uint32 magic;
		fread(&magic, sizeof(magic), 1, fp);

		bool bSwap = magic == 0xe7a6f7de;

		fread(&m_capacity, sizeof(m_capacity), 1, fp);
		SwapEndian(m_capacity, bSwap);

		fread(&m_available, sizeof(m_available), 1, fp);
		SwapEndian(m_available, bSwap);

		fread(&m_numAllocs, sizeof(m_numAllocs), 1, fp);
		SwapEndian(m_numAllocs, bSwap);

		fread(&m_minAlignment, sizeof(m_minAlignment), 1, fp);
		SwapEndian(m_minAlignment, bSwap);

		fread(&m_logMinAlignment, sizeof(m_logMinAlignment), 1, fp);
		SwapEndian(m_logMinAlignment, bSwap);

		fread(&m_freeBuckets[0], sizeof(m_freeBuckets), 1, fp);
		SwapEndian(&m_freeBuckets[0], NumBuckets, bSwap);

		uint32 numChunks;
		fread(&numChunks, sizeof(numChunks), 1, fp);
		SwapEndian(numChunks, bSwap);

		m_chunks.resize(numChunks);
		fread(&m_chunks[0], sizeof(m_chunks[0]), numChunks, fp);
		if (bSwap)
		{
			for (size_t ci = 0, cc = m_chunks.size(); ci != cc; ++ci)
				m_chunks[ci].SwapEndian();
		}

		uint32 numUnused;
		fread(&numUnused, sizeof(numUnused), 1, fp);
		SwapEndian(numUnused, bSwap);

		m_unusedChunks.resize(numUnused);
		fread(&m_unusedChunks[0], sizeof(m_unusedChunks[0]), numUnused, fp);
		SwapEndian(&m_unusedChunks[0], m_unusedChunks.size(), bSwap);

		uint32 numPending;
		fread(&numPending, sizeof(numPending), 1, fp);
		SwapEndian(numPending, bSwap);

		m_pendingMoves.resize(numPending);
		fread(&m_pendingMoves[0], sizeof(m_pendingMoves[0]), numPending, fp);
		if (bSwap)
		{
			for (size_t pi = 0, pc = m_pendingMoves.size(); pi != pc; ++pi)
				m_pendingMoves[pi].SwapEndian();
		}

		uint32 numSegs;
		fread(&numSegs, sizeof(numSegs), 1, fp);
		SwapEndian(numSegs, bSwap);

		m_segments.resize(numSegs);
		fread(&m_segments[0], sizeof(m_segments[0]), numSegs, fp);
		if (bSwap)
		{
			for (size_t si = 0, sc = m_segments.size(); si != sc; ++si)
				m_segments[si].SwapEndian();
		}

		fclose(fp);

		RebuildFreeLists();

	#ifdef CDBA_MORE_DEBUG
		ValidateAddressChain();
		ValidateFreeLists();
	#endif
	}
}
#endif

bool CDefragAllocator::AppendSegment(UINT_PTR capacity)
{
	CDBA_ASSERT((capacity & (m_minAlignment - 1)) == 0);

	bool bSuccessful = false;

	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);

	capacity >>= m_logMinAlignment;

	// Create a sentinal chunk for the head of the new segment.
	Index headSentinalIdx = AllocateChunk();
	if (headSentinalIdx != InvalidChunkIdx)
	{
		// And a free block.
		Index freeBlockIdx = AllocateChunk();
		if (freeBlockIdx != InvalidChunkIdx)
		{
			SDefragAllocChunk& headSentinalChunk = m_chunks[headSentinalIdx];
			SDefragAllocChunk& freeChunk = m_chunks[freeBlockIdx];
			SDefragAllocChunk& addrEndSentinal = m_chunks[AddrEndSentinal];

			uint32 segIdx = (uint32)m_segments.size();

			uint32 allocCapacity = m_capacity;

			headSentinalChunk.ptr = allocCapacity;
#ifndef _RELEASE
			headSentinalChunk.source = "Segment Sentinal";
#endif
			headSentinalChunk.attr.SetBusy(true);
			headSentinalChunk.attr.SetMoving(false);
			headSentinalChunk.attr.SetPinCount(1);
			headSentinalChunk.attr.SetSize(0);

			freeChunk.ptr = allocCapacity;
			freeChunk.attr.SetBusy(false);
			freeChunk.attr.SetMoving(false);
			freeChunk.attr.SetPinCount(0);
			freeChunk.attr.SetSize((unsigned int)capacity);

			LinkAddrChunk(headSentinalIdx, addrEndSentinal.addrPrevIdx);
			LinkAddrChunk(freeBlockIdx, headSentinalIdx);
			LinkFreeChunk(freeBlockIdx);
			addrEndSentinal.ptr = allocCapacity + (uint32)capacity;

			SDefragAllocSegment seg;
			seg.address = allocCapacity;
			seg.capacity = (uint32)capacity;
			seg.headSentinalChunkIdx = headSentinalIdx;
			m_segments.push_back(seg);

			m_capacity = allocCapacity + (uint32)capacity;
			m_available += (uint32)capacity;
			bSuccessful = true;

#ifdef CDBA_MORE_DEBUG
			ValidateAddressChain();
#endif
		}
		else
		{
			ReleaseChunk(headSentinalIdx);
		}
	}

	return bSuccessful;
}

void CDefragAllocator::UnAppendSegment()
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);

	// Can't remove the last segment!
	CDBA_ASSERT(m_segments.size() > 1);

	bool bHasMoves = Defrag_CompletePendingMoves();

	// All outstanding moves should be completed.
	CDBA_ASSERT(!bHasMoves);

	// Move all live chunks from this segment into another.
	SyncMoveSegment((uint32)m_segments.size() - 1);

	SDefragAllocSegment& segment = m_segments.back();
	SDefragAllocChunk& headSentinalChunk = m_chunks[segment.headSentinalChunkIdx];

	// Assert that the segment is empty, and release all chunks within it.
	uint32 endAddress = m_capacity;
	Index chunkIdx = headSentinalChunk.addrNextIdx;

	UnlinkAddrChunk(segment.headSentinalChunkIdx);
	ReleaseChunk(segment.headSentinalChunkIdx);

	SDefragAllocChunk* pChunk = &m_chunks[chunkIdx];

	int numValidSegs = (int)m_segments.size() - 1;

	while (pChunk->ptr != endAddress)
	{
		CDBA_ASSERT(!pChunk->attr.IsBusy());

		Index nextChunkIdx = pChunk->addrNextIdx;

		UnlinkFreeChunk(chunkIdx);
		UnlinkAddrChunk(chunkIdx);
		ReleaseChunk(chunkIdx);

		chunkIdx = nextChunkIdx;
		pChunk = &m_chunks[chunkIdx];
	}

	// chunkIdx/pChunk should be the end of memory sentinal.
	CDBA_ASSERT(chunkIdx == AddrEndSentinal && pChunk->attr.IsBusy() && !pChunk->attr.GetSize());

	pChunk->ptr -= segment.capacity;

	m_capacity -= segment.capacity;
	m_available -= segment.capacity;
	m_segments.pop_back();
}

IDefragAllocator::Hdl CDefragAllocator::Allocate(size_t sz, const char* source, void* pContext)
{
	CDBA_ASSERT((IntegerLog2(NextPower2(Align(sz, m_minAlignment))) - m_logMinAlignment) < (1u << SDefragAllocChunkAttr::SizeWidth));

	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	return Allocate_Locked(sz, m_minAlignment, source, pContext);
}

IDefragAllocator::Hdl CDefragAllocator::AllocateAligned(size_t sz, size_t alignment, const char* source, void* pContext)
{
	alignment = max((size_t)m_minAlignment, alignment);
	sz = Align(sz, m_minAlignment);

	// Just to check the alignment can be stored
	CDBA_ASSERT((IntegerLog2(alignment) - m_logMinAlignment) < (1u << SDefragAllocChunk::AlignBitCount));
	CDBA_ASSERT((IntegerLog2(NextPower2(Align(sz, alignment))) - m_logMinAlignment) < (1u << SDefragAllocChunkAttr::SizeWidth));

	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	return Allocate_Locked(sz, alignment, source, pContext);
}

IDefragAllocator::AllocatePinnedResult CDefragAllocator::AllocatePinned(size_t sz, const char* source, void* pContext /* = NULL */)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	AllocatePinnedResult apr = { 0 };
	apr.hdl = Allocate_Locked(sz, m_minAlignment, source, pContext);
	if (apr.hdl != InvalidHdl)
	{
		apr.offs = Pin(apr.hdl);
		apr.usableSize = CDefragAllocator::UsableSize(apr.hdl);
	}

	return apr;
}

IDefragAllocator::AllocatePinnedResult CDefragAllocator::AllocateAlignedPinned(size_t sz, size_t alignment, const char* source, void* pContext)
{
	alignment = max((size_t)m_minAlignment, alignment);
	sz = Align(sz, m_minAlignment);

	// Just to check the alignment and size can be stored
	CDBA_ASSERT((IntegerLog2(alignment) - m_logMinAlignment) < (1u << SDefragAllocChunk::AlignBitCount));
	CDBA_ASSERT((IntegerLog2(NextPower2(sz)) - m_logMinAlignment) < (1u << SDefragAllocChunkAttr::SizeWidth));

	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	AllocatePinnedResult apr = {0};
	apr.hdl = Allocate_Locked(sz, alignment, source, pContext);
	if (apr.hdl != InvalidHdl)
	{
		apr.offs = Pin(apr.hdl);
		apr.usableSize = CDefragAllocator::UsableSize(apr.hdl);
	}

	return apr;
}

bool CDefragAllocator::Free(Hdl hdl)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);

	if (hdl != InvalidHdl)
	{
		// Assume that the allocator now has exclusive ownership of hdl - that is,
		// that the caller won't be modifying it externally.

		Index hdlIdx = ChunkIdxFromHdl(hdl);
		CDBA_ASSERT(hdlIdx < m_chunks.size());

		SDefragAllocChunk* chunk = &m_chunks[hdlIdx];
		CDBA_ASSERT(chunk->attr.IsBusy());

		// Moving state can only be acquired by defrag and released by CancelMove, both own lock as well as Free.
		if (chunk->attr.IsMoving())
		{
			CancelMove_Locked(hdlIdx, false);
			CDBA_ASSERT(!chunk->attr.IsMoving());
		}

		MarkAsFree(*chunk);
		MergeFreeBlock(hdlIdx);

		return true;
	}

	return false;
}

void CDefragAllocator::ChangeContext(Hdl hdl, void* pNewContext)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	CDBA_ASSERT(hdl != InvalidHdl);

	Index hdlIdx = ChunkIdxFromHdl(hdl);
	SDefragAllocChunk& chunk = m_chunks[hdlIdx];
	CDBA_ASSERT(chunk.attr.IsBusy());
	chunk.pContext = pNewContext;
}

void* CDefragAllocator::GetContext(Hdl hdl)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	CDBA_ASSERT(hdl != InvalidHdl);

	Index hdlIdx = ChunkIdxFromHdl(hdl);
	SDefragAllocChunk& chunk = m_chunks[hdlIdx];
	CDBA_ASSERT(chunk.attr.IsBusy());
	return chunk.pContext;
}

IDefragAllocatorStats CDefragAllocator::GetStats()
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);

	IDefragAllocatorStats stats = { 0 };

	stats.nCapacity = static_cast<size_t>(m_capacity) << m_logMinAlignment;
	stats.nInUseSize = static_cast<size_t>(m_capacity - m_available) << m_logMinAlignment;
	stats.nInUseBlocks = m_numAllocs;

	uint32 numFreeBlocks = 0;
	uint32 numPinnedBlocks = 0;
	uint32 numMovingBlocks = 0;
	uint32 maxFreeBlockSize = 0;
	uint32 minFreeBlockSize = (uint32) - 1;

	SDefragAllocChunk* pChunks = &m_chunks[0];
	for (Index idx = pChunks[AddrStartSentinal].addrNextIdx; idx != AddrEndSentinal; idx = pChunks[idx].addrNextIdx)
	{
		SDefragAllocChunk& c = pChunks[idx];
		SDefragAllocChunkAttr ca = c.attr;

		if (ca.IsBusy())
		{
			if (ca.IsPinned())
				++numPinnedBlocks;
			if (ca.IsMoving())
				++numMovingBlocks;
		}
		else
		{
			maxFreeBlockSize = max(maxFreeBlockSize, ca.GetSize());
			minFreeBlockSize = min(minFreeBlockSize, ca.GetSize());
			++numFreeBlocks;
		}
	}

	stats.nFreeBlocks = numFreeBlocks;
	stats.nPinnedBlocks = numPinnedBlocks - numMovingBlocks - (int)(m_segments.size() - 1); // Each moving block pins a block for the destination
	stats.nMovingBlocks = numMovingBlocks;
	stats.nLargestFreeBlockSize = maxFreeBlockSize << m_logMinAlignment;
	stats.nSmallestFreeBlockSize = minFreeBlockSize << m_logMinAlignment;
	stats.nMeanFreeBlockSize = (numFreeBlocks > 0)
	                           ? ((m_available << m_logMinAlignment) / numFreeBlocks)
	                           : 0;
	stats.nCancelledMoveCount = (uint32)m_nCancelledMoves;

	return stats;
}

size_t CDefragAllocator::DefragmentTick(size_t maxMoves, size_t maxAmount, bool bForce)
{
	if (m_isThreadSafe)
	{
		if (bForce)
			m_lock.Lock();
		else if (!m_lock.TryLock())
			return 0;
	}

	Defrag_CompletePendingMoves();

#ifdef CDBA_MORE_DEBUG
	Tick_Validation_Locked();
#endif

	maxMoves = min(maxMoves, (size_t)MaxPendingMoves);
	maxMoves = min(maxMoves, m_unusedChunks.size());    // Assume that each move requires a split
	maxAmount >>= m_logMinAlignment;

	PendingMove* moves[MaxPendingMoves];
	size_t numMoves = 0;
	size_t curAmount = 0;

#ifdef CDBA_MORE_DEBUG
	Defrag_ValidateFreeBlockIteration();
#endif

	if (maxMoves > 0)
	{
		numMoves = Defrag_FindMovesBwd(moves + numMoves, maxMoves - numMoves, curAmount, maxAmount);
		if (numMoves < maxMoves)
			numMoves += Defrag_FindMovesFwd(moves + numMoves, maxMoves - numMoves, curAmount, maxAmount);

#ifdef CDBA_DEBUG
		for (size_t moveIdx = 0; moveIdx < numMoves; ++moveIdx)
		{
			PendingMove& pm = *moves[moveIdx];

			SDefragAllocChunk& srcChunk = m_chunks[pm.srcChunkIdx];
			SDefragAllocChunk& dstChunk = m_chunks[pm.dstChunkIdx];
			SDefragAllocChunkAttr srcChunkAttr = srcChunk.attr;
			SDefragAllocChunkAttr dstChunkAttr = dstChunk.attr;

			CDBA_ASSERT(srcChunkAttr.IsMoving());
			CDBA_ASSERT(dstChunkAttr.IsPinned());
			CDBA_ASSERT(dstChunkAttr.IsBusy());
			CDBA_ASSERT(dstChunkAttr.GetSize() == srcChunkAttr.GetSize());
		}
#endif
	}

	if (m_isThreadSafe)
		m_lock.Unlock();

	return curAmount;
}

CDefragAllocator::Index CDefragAllocator::AllocateChunk()
{
	Index result = InvalidChunkIdx;

	if (!m_unusedChunks.empty())
	{
		result = m_unusedChunks.back();
		m_unusedChunks.pop_back();
	}
	else if (!m_chunksAreFixed)
	{
		m_chunks.push_back(SDefragAllocChunk());
		result = (Index) (m_chunks.size() - 1);
		CDBA_ASSERT(result == (m_chunks.size() - 1));
	}

	IF_LIKELY (result != InvalidChunkIdx)
	{
		SDefragAllocChunk& chunk = m_chunks[result];
		chunk.attr.SetBusy(false);
		chunk.attr.SetPinCount(0);
		chunk.attr.SetMoving(false);
		chunk.pContext = NULL;
#ifndef _RELEASE
		chunk.source = "";
#endif
#ifdef CDBA_MORE_DEBUG
		chunk.hash = 0;
#endif
	}

	return result;
}

void CDefragAllocator::ReleaseChunk(Index idx)
{
	m_unusedChunks.push_back(idx);
}

void CDefragAllocator::LinkFreeChunk(Index idx)
{
	SDefragAllocChunk* pChunks = &m_chunks[0];
	SDefragAllocChunk& chunk = pChunks[idx];

	CDBA_ASSERT(!chunk.attr.IsBusy());

	int bucket = BucketForSize(chunk.attr.GetSize());

	Index rootIdx = m_freeBuckets[bucket];
	Index beforeIdx;

	// Search for the position in the list to insert the new block, maintaining address order
	for (beforeIdx = pChunks[rootIdx].freeNextIdx; beforeIdx != rootIdx; beforeIdx = pChunks[beforeIdx].freeNextIdx)
	{
		if (pChunks[beforeIdx].ptr > chunk.ptr)
			break;
	}

	Index afterIdx = pChunks[beforeIdx].freePrevIdx;
	SDefragAllocChunk& after = pChunks[afterIdx];

	chunk.freeNextIdx = after.freeNextIdx;
	chunk.freePrevIdx = afterIdx;
	pChunks[after.freeNextIdx].freePrevIdx = idx;
	after.freeNextIdx = idx;
}

IDefragAllocator::Hdl CDefragAllocator::Allocate_Locked(size_t sz, size_t alignment, const char* source, void* pContext)
{
#if CRY_PLATFORM_64BIT
	CDBA_ASSERT(sz <= (BIT64(NumBuckets)));
#else
	CDBA_ASSERT(sz <= (BIT(NumBuckets)));
#endif

	sz = Align(sz, alignment) >> m_logMinAlignment;
	alignment >>= m_logMinAlignment;

	if (sz > m_available)
		return InvalidHdl;

	Index bestChunkIdx = InvalidChunkIdx;
	switch (m_policy.blockSearchKind)
	{
	case eBSK_BestFit:
		bestChunkIdx = BestFit_FindFreeBlockFor(sz, alignment, 0, ~(uint32)0, true);
		break;
	case eBSK_FirstFit:
		bestChunkIdx = FirstFit_FindFreeBlockFor(sz, alignment, 0, ~(uint32)0, true);
		break;
	default:
		__debugbreak();
		break;
	}

	if (bestChunkIdx != InvalidChunkIdx)
	{
		SplitResult sr = SplitFreeBlock(bestChunkIdx, sz, alignment, true);

		IF_LIKELY (sr.bSuccessful)
		{
			SDefragAllocChunk& bestChunk = m_chunks[bestChunkIdx];
			CDBA_ASSERT(bestChunk.attr.GetSize() == sz);

			MarkAsInUse(bestChunk);
			bestChunk.pContext = pContext;
#ifndef _RELEASE
			bestChunk.source = source;
#endif
			bestChunk.attr.SetPinCount(0);
			bestChunk.attr.SetMoving(false);

			return ChunkHdlFromIdx(bestChunkIdx);
		}
	}

	return InvalidHdl;
}

CDefragAllocator::SplitResult CDefragAllocator::SplitFreeBlock(Index fbId, size_t sz, size_t alignment, bool allocateInLowHalf)
{
	CDefragAllocator::SplitResult res;
	res.bSuccessful = false;
	res.nLeftSplitChunkIdx = InvalidChunkIdx;
	res.nRightSplitChunkIdx = InvalidChunkIdx;

	SDefragAllocChunk* allocChunk = &m_chunks[fbId];

	CDBA_ASSERT(!allocChunk->attr.IsBusy());
	CDBA_ASSERT(sz <= allocChunk->attr.GetSize());

	if ((allocChunk->ptr & (alignment - 1)) != 0 || allocChunk->attr.GetSize() != sz)
	{
		// Determine the sizes of the new chunks
		UINT_PTR baseAddress = allocChunk->ptr;
		UINT_PTR endAddress = allocChunk->ptr + allocChunk->attr.GetSize();
		UINT_PTR allocAddress = allocateInLowHalf
		                        ? Align(baseAddress, alignment)
		                        : ((endAddress - sz) & ~(alignment - 1));
		UINT_PTR allocEndAddress = allocAddress + sz;

		// Allocate split chunks
		Index nLeftSplitChunkIdx = InvalidChunkIdx;
		if (baseAddress != allocAddress)
		{
			nLeftSplitChunkIdx = AllocateChunk();
			if (nLeftSplitChunkIdx == InvalidChunkIdx)
				return res;
		}

		Index nRightSplitChunkIdx = InvalidChunkIdx;
		if (allocEndAddress != endAddress)
		{
			nRightSplitChunkIdx = AllocateChunk();
			if (nRightSplitChunkIdx == InvalidChunkIdx)
			{
				if (nLeftSplitChunkIdx != InvalidChunkIdx)
					ReleaseChunk(nLeftSplitChunkIdx);
				return res;
			}
		}

		// Split
		UnlinkFreeChunk(fbId);

		allocChunk = &m_chunks[fbId];

		if (nLeftSplitChunkIdx != InvalidChunkIdx)
		{
			SDefragAllocChunk& splitChunk = m_chunks[nLeftSplitChunkIdx];

			splitChunk.ptr = baseAddress;
			splitChunk.attr.SetSize((int)(allocAddress - baseAddress));
			LinkAddrChunk(nLeftSplitChunkIdx, allocChunk->addrPrevIdx);
		}

		if (nRightSplitChunkIdx != InvalidChunkIdx)
		{
			SDefragAllocChunk& splitChunk = m_chunks[nRightSplitChunkIdx];

			splitChunk.ptr = allocEndAddress;
			splitChunk.attr.SetSize((int)(endAddress - allocEndAddress));
			LinkAddrChunk(nRightSplitChunkIdx, fbId);
		}

		allocChunk->ptr = allocAddress;
		allocChunk->attr.SetSize((int)(allocEndAddress - allocAddress));
		allocChunk->logAlign = IntegerLog2(alignment);

		if (nLeftSplitChunkIdx != InvalidChunkIdx)
			LinkFreeChunk(nLeftSplitChunkIdx);
		if (nRightSplitChunkIdx != InvalidChunkIdx)
			LinkFreeChunk(nRightSplitChunkIdx);

		res.bSuccessful = true;
		res.nLeftSplitChunkIdx = nLeftSplitChunkIdx;
		res.nRightSplitChunkIdx = nRightSplitChunkIdx;
	}
	else
	{
		// Perfect fit.
		UnlinkFreeChunk(fbId);

		res.bSuccessful = true;
	}

#ifdef CDBA_MORE_DEBUG
	ValidateFreeLists();
#endif

	return res;
}

CDefragAllocator::Index CDefragAllocator::MergeFreeBlock(Index fbId)
{
	SDefragAllocChunk* pChunks = &m_chunks[0];

	SDefragAllocChunk* chunk = &pChunks[fbId];

	CDBA_ASSERT(m_available <= m_capacity);
	CDBA_ASSERT(!chunk->attr.IsBusy());

	LinkFreeChunk(fbId);

	Index prevChunkIdx = chunk->addrPrevIdx;
	SDefragAllocChunk& prevChunk = pChunks[prevChunkIdx];
	if (!prevChunk.attr.IsBusy())
	{
		// Merge with previous chunk

		UnlinkFreeChunk(fbId);
		UnlinkFreeChunk(prevChunkIdx);

		prevChunk.attr.AddSize(chunk->attr.GetSize());
		prevChunk.addrNextIdx = chunk->addrNextIdx;
		pChunks[chunk->addrNextIdx].addrPrevIdx = prevChunkIdx;

		CDBA_ASSERT(pChunks[prevChunk.addrPrevIdx].ptr + pChunks[prevChunk.addrPrevIdx].attr.GetSize() == prevChunk.ptr);
		CDBA_ASSERT(prevChunk.ptr + prevChunk.attr.GetSize() == pChunks[prevChunk.addrNextIdx].ptr);
		CDBA_ASSERT(pChunks[prevChunk.addrPrevIdx].addrNextIdx == prevChunkIdx);
		CDBA_ASSERT(pChunks[prevChunk.addrNextIdx].addrPrevIdx == prevChunkIdx);

		LinkFreeChunk(prevChunkIdx);
		ReleaseChunk(fbId);

		// Pretend the released chunk is the previous one, so the next merge can happen

		fbId = prevChunkIdx;
		chunk = &prevChunk;
	}

	Index nextChunkIdx = chunk->addrNextIdx;
	SDefragAllocChunk& nextChunk = pChunks[nextChunkIdx];
	if (!nextChunk.attr.IsBusy())
	{
		// Merge with next chunk

		UnlinkFreeChunk(fbId);
		UnlinkFreeChunk(nextChunkIdx);

		chunk->attr.AddSize(nextChunk.attr.GetSize());
		chunk->addrNextIdx = nextChunk.addrNextIdx;
		pChunks[chunk->addrNextIdx].addrPrevIdx = fbId;

		CDBA_ASSERT(pChunks[chunk->addrPrevIdx].ptr + pChunks[chunk->addrPrevIdx].attr.GetSize() == chunk->ptr);
		CDBA_ASSERT(chunk->ptr + chunk->attr.GetSize() == pChunks[chunk->addrNextIdx].ptr);
		CDBA_ASSERT(pChunks[chunk->addrPrevIdx].addrNextIdx == fbId);
		CDBA_ASSERT(pChunks[chunk->addrNextIdx].addrPrevIdx == fbId);

		LinkFreeChunk(fbId);
		ReleaseChunk(nextChunkIdx);
	}

	return fbId;
}

#ifdef CDBA_MORE_DEBUG
void CDefragAllocator::Defrag_ValidateFreeBlockIteration()
{
	Index freeLists[NumBuckets];
	size_t numFreeLists = 0;

	{
		PrepareMergePopPrev(freeLists);
		UINT_PTR addr = (UINT_PTR)-1;

		if (numFreeLists)
		{
			do
			{
				size_t list = MergePeekPrevChunk(freeLists);
				Index chk = freeLists[list];
				UINT_PTR chkAddr = m_chunks[chk].ptr;
				CDBA_ASSERT(chkAddr < addr);
				addr = chkAddr;
				MergePopPrevChunk(freeLists, list);
			}
			while (numFreeLists > 0);
		}
	}
	{
		PrepareMergePopNext(freeLists);
		UINT_PTR addr = 0;

		if (numFreeLists)
		{
			do
			{
				size_t list = MergePeekNextChunk(freeLists);
				Index chk = freeLists[list];
				UINT_PTR chkAddr = m_chunks[chk].ptr;
				CDBA_ASSERT(chkAddr > addr);
				addr = chkAddr;
				MergePopNextChunk(freeLists, list);
			}
			while (numFreeLists > 0);
		}
	}
}

void CDefragAllocator::Tick_Validation_Locked()
{
	if (m_policy.pDefragPolicy)
	{
		uint32 nFirstChunk = m_nLastCheckedChunk;
		uint32 nTest = 64;
		do
		{
			SDefragAllocChunk& chunk = m_chunks[m_nLastCheckedChunk];

			if (chunk.attr.IsBusy() && !chunk.attr.IsMoving() && chunk.attr.GetSize() > 0)
			{
				// Mark the chunk as moving before starting the crc - this will force
				// any users that may want to pin to sync on the allocator lock
				MarkAsMoving(chunk);

				if (!chunk.attr.IsPinned())
				{
					uint32 hash = m_policy.pDefragPolicy->Hash(chunk.ptr << m_logMinAlignment, chunk.attr.GetSize() << m_logMinAlignment);

					if (!chunk.attr.IsInvalid())
					{
						if (hash && chunk.hash && (hash != chunk.hash))
							__debugbreak();
					}

					chunk.hash = hash;

					// Release the "move"
					MarkAsNotMovingValid(chunk);
				}
				else
				{
					// Release the "move"
					MarkAsNotMoving(chunk);
				}

				-- nTest;
			}

			m_nLastCheckedChunk = (m_nLastCheckedChunk + 1) == m_chunks.size() ? 0 : (m_nLastCheckedChunk + 1);
		}
		while (nTest > 0 && m_nLastCheckedChunk != nFirstChunk);
	}
}
#endif

CDefragAllocator::Index CDefragAllocator::BestFit_FindFreeBlockForSegment(size_t sz, size_t alignment, uint32 seg)
{
	SDefragAllocSegment& segment = m_segments[seg];
	return BestFit_FindFreeBlockFor(sz, alignment, segment.address, segment.address + segment.capacity, true);
}

CDefragAllocator::Index CDefragAllocator::BestFit_FindFreeBlockFor(size_t sz, size_t alignment, UINT_PTR addressMin, UINT_PTR addressMax, bool allocateInLowHalf)
{
	Index bestChunkIdx = InvalidChunkIdx;
	UINT_PTR bestWastage = (UINT_PTR)-1;

	for (int bucket = BucketForSize(sz); (bestChunkIdx == InvalidChunkIdx) && (bucket < NumBuckets); ++bucket)
	{
		size_t rootIdx = m_freeBuckets[bucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];

		for (size_t idx = root.freeNextIdx; idx != rootIdx; idx = m_chunks[idx].freeNextIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max(1U, chunk.attr.GetSize()) >= (1U << bucket));

			if (chunk.ptr < addressMax)
			{
				UINT_PTR chunkStart = chunk.ptr;
				UINT_PTR chunkSize = chunk.attr.GetSize();
				UINT_PTR chunkEnd = chunkStart + chunkSize;
				UINT_PTR allocStart = allocateInLowHalf
				                      ? Align(chunkStart, alignment)
				                      : ((chunkEnd - sz) & ~(alignment - 1));
				UINT_PTR allocEnd = allocStart + sz;

				if (chunkStart <= allocStart && allocEnd <= chunkEnd)
				{
					if (allocStart == chunkStart && chunkSize == sz)
					{
						bestChunkIdx = idx;
						bestWastage = 0;

						break;
					}

					UINT_PTR wastage = (allocStart - chunkStart) + (chunkEnd - allocEnd);
					if (wastage < bestWastage)
					{
						bestChunkIdx = idx;
						bestWastage = wastage;
					}
				}
			}
			else
			{
				break;
			}
		}
	}

	return bestChunkIdx;
}

CDefragAllocator::Index CDefragAllocator::FirstFit_FindFreeBlockFor(size_t sz, size_t alignment, UINT_PTR addressMin, UINT_PTR addressMax, bool allocateInLowHalf)
{
	for (int bucket = BucketForSize(sz); bucket < NumBuckets; ++bucket)
	{
		size_t rootIdx = m_freeBuckets[bucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];
		for (size_t idx = root.freeNextIdx; idx != rootIdx; idx = m_chunks[idx].freeNextIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];
			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max(1U, chunk.attr.GetSize()) >= (1U << bucket));
			if (chunk.ptr < addressMax)
			{
				UINT_PTR chunkStart = chunk.ptr;
				UINT_PTR chunkSize = chunk.attr.GetSize();
				UINT_PTR chunkEnd = chunkStart + chunkSize;
				UINT_PTR allocStart = allocateInLowHalf
				                      ? Align(chunkStart, alignment)
				                      : ((chunkEnd - sz) & ~(alignment - 1));
				UINT_PTR allocEnd = allocStart + sz;
				if (chunkStart <= allocStart && allocEnd <= chunkEnd)
				{
					return idx;
				}
			}
			else
			{
				break;
			}
		}
	}
	return InvalidChunkIdx;
}
size_t CDefragAllocator::Defrag_FindMovesBwd(PendingMove** pMoves, size_t maxMoves, size_t& curAmount, size_t maxAmount)
{
	Index freeLists[NumBuckets];

	size_t numMoves = 0;
	bool bIsLast = true;

	PrepareMergePopPrev(freeLists);

	int nSegmentIdx = (int)m_segments.size() - 1;

	// Try and copy a block from the back to the front
	do
	{
		size_t freeChunkList = MergePeekPrevChunk(freeLists);
		if (freeChunkList == (size_t)-1)
			break;

		assert(freeChunkList < CRY_ARRAY_COUNT(freeLists));
		Index freeChunkIdx = freeLists[freeChunkList];
		MergePopPrevChunk(freeLists, freeChunkList);

		// Keep track of the last free block in each segment, by watching free blocks
		// crossing segment boundaries.
		if (nSegmentIdx >= 0)
		{
			if (m_chunks[freeChunkIdx].ptr < m_segments[nSegmentIdx].address)
			{
				bIsLast = true;
				--nSegmentIdx;
			}
		}

		// Don't move free block->next blocks if they're at the end of a segment - they're defragged enough.
		// TODO - could allow this (and self moves below) if defragging stalls in a bad state
		if (!bIsLast)
		{
			for (Index candidateIdx = m_chunks[freeChunkIdx].addrNextIdx; freeChunkIdx != InvalidChunkIdx && numMoves < maxMoves && curAmount < maxAmount; )
			{
				SDefragAllocChunk& freeChunk = m_chunks[freeChunkIdx];
				SDefragAllocChunk& candidateChunk = m_chunks[candidateIdx];
				SDefragAllocChunkAttr candidateChunkAttr = candidateChunk.attr;

				CDBA_ASSERT(!freeChunk.attr.IsBusy());

				if (IsMoveableCandidate(candidateChunkAttr, 0xffffffff))
				{
#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
					size_t candidateChunkAlign = BIT64(candidateChunk.logAlign);
#else
					size_t candidateChunkAlign = BIT(candidateChunk.logAlign);
#endif

					// Try and reallocate this chunk. For the time being, assume that it's going to remain in a valid moveable state.
					Index dstChunkIdx = Defrag_Bwd_FindFreeBlockFor(candidateChunkAttr.GetSize(), candidateChunkAlign, freeChunk.ptr);// + freeChunk.attr.GetSize());

					if (dstChunkIdx != InvalidChunkIdx)
					{
						PendingMove* pPM = AllocPendingMove();
						IF_UNLIKELY (!pPM)
							return numMoves;

						// Found a possible move, so try and mark the chunk as moveable - as long as it's still a valid candidate.
						if (TryMarkAsMoving(candidateChunk, 0xffffffff))
						{
							// Chunk has been marked as moveable - can now split the dest chunk and set up the move state.
							// If the chunk gets pinned in the meantime, it'll sync (through CancelMove) on the allocator lock.

							if (TryScheduleCopy(candidateChunk, m_chunks[dstChunkIdx], pPM, true))
							{
								SplitResult sr = SplitFreeBlock(dstChunkIdx, candidateChunkAttr.GetSize(), candidateChunkAlign, true);
								CDBA_ASSERT(sr.bSuccessful);

								MergePatchPrevRemove(freeLists, dstChunkIdx);

								if (sr.nRightSplitChunkIdx != InvalidChunkIdx)
									MergePatchPrevInsert(freeLists, sr.nRightSplitChunkIdx);

								pPM->srcChunkIdx = candidateIdx;
								pPM->dstChunkIdx = dstChunkIdx;

								// dstChunk is owned by the allocator, so we don't need to be atomic when changing it's state.
								SDefragAllocChunk& dstChunk = m_chunks[dstChunkIdx];
								MarkAsInUse(dstChunk);
								dstChunk.attr.SetPinCount(1);

								CDBA_ASSERT(sr.nRightSplitChunkIdx == InvalidChunkIdx || (m_chunks[sr.nRightSplitChunkIdx].ptr > dstChunk.ptr));
								CDBA_ASSERT(dstChunk.ptr < candidateChunk.ptr);

#ifdef CDBA_MORE_DEBUG
								ValidateAddressChain();
#endif

								pMoves[numMoves++] = pPM;
								curAmount += dstChunk.attr.GetSize();

								candidateIdx = m_chunks[candidateIdx].addrNextIdx;
								if (dstChunkIdx == freeChunkIdx)
									freeChunkIdx = sr.nRightSplitChunkIdx;
								continue;
							}
							else
							{
								MarkAsNotMoving(candidateChunk);
							}
						}

						// Candidate (probably) got pinned whilst finding a free block, or the copy schedule failed. Nevermind - undo what we need to.
						FreePendingMove(pPM);
						pPM = NULL;
					}
				}

				break;
			}
		}

		if (freeChunkIdx != InvalidChunkIdx)
		{
			for (Index candidateIdx = m_chunks[freeChunkIdx].addrPrevIdx; numMoves < maxMoves && curAmount < maxAmount; )
			{
				SDefragAllocChunk& freeChunk = m_chunks[freeChunkIdx];
				SDefragAllocChunk& candidateChunk = m_chunks[candidateIdx];
				SDefragAllocChunkAttr candidateChunkAttr = candidateChunk.attr;

				if (IsMoveableCandidate(candidateChunkAttr, 0xffffffff))
				{
#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
					size_t candidateChunkAlign = BIT64(candidateChunk.logAlign);
#else
					size_t candidateChunkAlign = BIT(candidateChunk.logAlign);
#endif

					// Try and reallocate this chunk. For the time being, assume that it's going to remain in a valid moveable state.
					Index dstChunkIdx = Defrag_Bwd_FindFreeBlockFor(candidateChunkAttr.GetSize(), candidateChunkAlign, candidateChunk.ptr);
					CDBA_ASSERT(dstChunkIdx != freeChunkIdx);

					if (dstChunkIdx != InvalidChunkIdx)
					{
						PendingMove* pPM = AllocPendingMove();
						IF_UNLIKELY (!pPM)
							return numMoves;

						// Found a possible move, so try and mark the chunk as moveable - as long as it's still a valid candidate.
						if (TryMarkAsMoving(candidateChunk, 0xffffffff))
						{
							// Chunk has been marked as moveable - can now split the chunk and set up the move state.
							// If the chunk gets pinned in the meantime, it'll sync (through CancelMove) on the allocator lock.

							if (TryScheduleCopy(candidateChunk, m_chunks[dstChunkIdx], pPM, true))
							{
								SplitResult sr = SplitFreeBlock(dstChunkIdx, candidateChunkAttr.GetSize(), candidateChunkAlign, true);
								CDBA_ASSERT(sr.bSuccessful);

								MergePatchPrevRemove(freeLists, dstChunkIdx);

								if (sr.nRightSplitChunkIdx != InvalidChunkIdx)
									MergePatchPrevInsert(freeLists, sr.nRightSplitChunkIdx);

								pPM->srcChunkIdx = candidateIdx;
								pPM->dstChunkIdx = dstChunkIdx;

								// dstChunk is owned by the allocator, so don't need to be atomic when changing it's state.
								SDefragAllocChunk& dstChunk = m_chunks[dstChunkIdx];
								MarkAsInUse(dstChunk);
								dstChunk.attr.SetPinCount(1);

								CDBA_ASSERT(sr.nRightSplitChunkIdx == InvalidChunkIdx || (m_chunks[sr.nRightSplitChunkIdx].ptr > dstChunk.ptr));
								CDBA_ASSERT(dstChunk.ptr < candidateChunk.ptr);

#ifdef CDBA_MORE_DEBUG
								ValidateAddressChain();
#endif

								pMoves[numMoves++] = pPM;
								curAmount += dstChunk.attr.GetSize();

								candidateIdx = m_chunks[candidateIdx].addrPrevIdx;
								continue;
							}
							else
							{
								MarkAsNotMoving(candidateChunk);
							}
						}

						// Candidate (probably) got pinned whilst finding a free block, or the copy schedule failed. Nevermind - undo what we need to.
						FreePendingMove(pPM);
						pPM = NULL;
					}
				}

				break;
			}
		}
		bIsLast = false;
	}
	while (numMoves < maxMoves);

	return numMoves;
}

size_t CDefragAllocator::Defrag_FindMovesFwd(PendingMove** pMoves, size_t maxMoves, size_t& curAmount, size_t maxAmount)
{
	Index freeLists[NumBuckets];

	size_t numMoves = 0;

	PrepareMergePopNext(freeLists);

	// Try and copy a block from the front to the back
	do
	{
		size_t freeChunkList = MergePeekNextChunk(freeLists);
		if (freeChunkList == (size_t)-1)
			break;

		assert(freeChunkList < CRY_ARRAY_COUNT(freeLists));

		Index freeChunkIdx = freeLists[freeChunkList];
		MergePopNextChunk(freeLists, freeChunkList);

		CDBA_ASSERT(!m_chunks[freeChunkIdx].attr.IsBusy());

		for (Index candidateIdx = m_chunks[freeChunkIdx].addrNextIdx; numMoves < maxMoves && curAmount < maxAmount; )
		{
			SDefragAllocChunk& freeChunk = m_chunks[freeChunkIdx];
			SDefragAllocChunk& candidateChunk = m_chunks[candidateIdx];
			SDefragAllocChunkAttr candidateChunkAttr = candidateChunk.attr;

			if (IsMoveableCandidate(candidateChunkAttr, 0xffffffff))
			{
#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
				size_t candidateChunkAlign = BIT64(candidateChunk.logAlign);
#else
				size_t candidateChunkAlign = BIT(candidateChunk.logAlign);
#endif

				// Try and reallocate this chunk. For the time being, assume that it's going to remain in a valid moveable state.
				Index dstChunkIdx = Defrag_Fwd_FindFreeBlockFor(candidateChunkAttr.GetSize(), candidateChunkAlign, freeChunk.ptr + freeChunk.attr.GetSize());

				CDBA_ASSERT(dstChunkIdx != freeChunkIdx);

				if (dstChunkIdx != InvalidChunkIdx)
				{
					PendingMove* pPM = AllocPendingMove();
					IF_UNLIKELY (!pPM)
						return numMoves;

					CDBA_ASSERT(!m_chunks[dstChunkIdx].attr.IsBusy());
					CDBA_ASSERT(m_chunks[dstChunkIdx].attr.GetSize() >= candidateChunkAttr.GetSize());

					// Found a possible move, so try and mark the chunk as moveable - as long as it's still a valid candidate.
					if (TryMarkAsMoving(candidateChunk, 0xffffffff))
					{
						// Chunk has been marked as moveable - can now split the dest chunk and set up the move state.
						// If the chunk gets pinned in the meantime, it'll sync (through CancelMove) on the allocator lock.

						if (TryScheduleCopy(candidateChunk, m_chunks[dstChunkIdx], pPM, false))
						{
							SplitResult sr = SplitFreeBlock(dstChunkIdx, candidateChunkAttr.GetSize(), candidateChunkAlign, false);
							CDBA_ASSERT(sr.bSuccessful);

							MergePatchNextRemove(freeLists, dstChunkIdx);

							if (sr.nLeftSplitChunkIdx != InvalidChunkIdx)
								MergePatchNextInsert(freeLists, sr.nLeftSplitChunkIdx);

							pPM->srcChunkIdx = candidateIdx;
							pPM->dstChunkIdx = dstChunkIdx;

							// dstChunk is owned by the allocator, so we don't need to be atomic when changing it's state.
							SDefragAllocChunk& dstChunk = m_chunks[dstChunkIdx];
							MarkAsInUse(dstChunk);
							dstChunk.attr.SetPinCount(1);

							CDBA_ASSERT(sr.nLeftSplitChunkIdx == InvalidChunkIdx || (m_chunks[sr.nLeftSplitChunkIdx].ptr < dstChunk.ptr));
							CDBA_ASSERT(dstChunk.ptr > candidateChunk.ptr);

#ifdef CDBA_MORE_DEBUG
							ValidateAddressChain();
#endif

							pMoves[numMoves++] = pPM;
							curAmount += dstChunk.attr.GetSize();

							candidateIdx = m_chunks[candidateIdx].addrNextIdx;
							continue;
						}
						else
						{
							MarkAsNotMoving(candidateChunk);
						}
					}

					// Candidate (probably) got pinned whilst finding a free block, or the copy schedule failed. Nevermind - undo what we need to.
					FreePendingMove(pPM);
					pPM = NULL;
				}
			}

			break;
		}
	}
	while (numMoves < maxMoves);

	return numMoves;
}

bool CDefragAllocator::Defrag_CompletePendingMoves()
{
	CDBA_ASSERT(m_policy.pDefragPolicy);

	bool bHasMoves = false;

	for (DynArray<PendingMove>::iterator it = m_pendingMoves.begin(), itEnd = m_pendingMoves.end(); it != itEnd; ++it)
	{
		if (it->notify.bCancel)
		{
			// Currently only support cancelling before a move has really begun
			// Also assume that there is no copy in flight - the policy has requested it, so it's their responsibility to
			// make sure that this is the case
			CDBA_ASSERT(!it->relocated);

			MarkAsFree(m_chunks[it->dstChunkIdx]);
			MergeFreeBlock(it->dstChunkIdx);

			MarkAsNotMoving(m_chunks[it->srcChunkIdx]);

			FreePendingMove(&*it);
		}
		else
		{
			if (it->notify.bDstIsValid)
			{
				if (!it->relocated)
				{
					CDBA_ASSERT(it->dstChunkIdx != InvalidChunkIdx);

					if (!it->cancelled)
						Relocate(it->userMoveId, it->srcChunkIdx, it->dstChunkIdx);

					it->relocated = true;
				}

				if (it->notify.bSrcIsUnneeded)
				{
					SDefragAllocChunk& dst = m_chunks[it->dstChunkIdx];
					MarkAsFree(dst);
					MergeFreeBlock(it->dstChunkIdx);

					if (!it->cancelled)
					{
						CDBA_ASSERT(it->srcChunkIdx != InvalidChunkIdx);

						SDefragAllocChunk& src = m_chunks[it->srcChunkIdx];

						// Should be the last thing to occur during move
						MarkAsNotMoving(src);
					}
					else
					{
						// Only the destination chunk is now valid - just free it.
						CDBA_ASSERT(it->srcChunkIdx == InvalidChunkIdx);
					}

					FreePendingMove(&*it);
				}
			}

			if (it->dstChunkIdx != InvalidChunkIdx)
				bHasMoves = true;
		}
	}

	return bHasMoves;
}

CDefragAllocator::Index CDefragAllocator::Defrag_Bwd_FindFreeBlockFor(size_t sz, size_t alignment, UINT_PTR addressLimit)
{
	Index bestChunkIdx = InvalidChunkIdx;
	UINT_PTR maxAddress = addressLimit;

	int minBucket = BucketForSize(sz);

	// Search for an exact fit
	{
		size_t rootIdx = m_freeBuckets[minBucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];

		for (size_t idx = root.freeNextIdx; idx != rootIdx; idx = m_chunks[idx].freeNextIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max((uint32)1, (uint32)chunk.attr.GetSize()) >= (1U << minBucket));

			UINT_PTR chunkBase = chunk.ptr;
			UINT_PTR chunkEnd = chunkBase + chunk.attr.GetSize();
			UINT_PTR allocBase = Align(chunkBase, alignment);
			UINT_PTR allocEnd = allocBase + sz;

			if (chunk.ptr < maxAddress)
			{
				if (chunkBase == allocBase && chunkEnd == allocEnd)
				{
					bestChunkIdx = idx;
					maxAddress = chunk.ptr;
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	for (size_t bucket = minBucket; (bestChunkIdx == InvalidChunkIdx) && (bucket < NumBuckets); ++bucket)
	{
		size_t rootIdx = m_freeBuckets[bucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];

		for (size_t idx = root.freeNextIdx; idx != rootIdx; idx = m_chunks[idx].freeNextIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max((uint32)1, (uint32)chunk.attr.GetSize()) >= (1U << bucket));

			UINT_PTR chunkBase = chunk.ptr;
			UINT_PTR chunkEnd = chunkBase + chunk.attr.GetSize();
			UINT_PTR allocBase = Align(chunkBase, alignment);
			UINT_PTR allocEnd = allocBase + sz;

			if (allocEnd <= maxAddress)
			{
				if (chunkBase <= allocBase && allocEnd <= chunkEnd)
				{
					bestChunkIdx = idx;
					maxAddress = chunkBase;
					break;
				}
			}
			else
			{
				// Chunks are ordered by address. If the maxAddress has been exceeded, there's no point in searching the rest of the list.
				break;
			}
		}
	}

	return bestChunkIdx;
}

CDefragAllocator::Index CDefragAllocator::Defrag_Fwd_FindFreeBlockFor(size_t sz, size_t alignment, UINT_PTR addressLimit)
{
	Index bestChunkIdx = InvalidChunkIdx;
	UINT_PTR minAddress = addressLimit;

	int minBucket = BucketForSize(sz);

	// Search for an exact fit
	{
		size_t rootIdx = m_freeBuckets[minBucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];

		for (size_t idx = root.freePrevIdx; idx != rootIdx; idx = m_chunks[idx].freePrevIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max((uint32)1, (uint32)chunk.attr.GetSize()) >= (1U << minBucket));

			UINT_PTR chunkBase = chunk.ptr;
			UINT_PTR chunkEnd = chunkBase + chunk.attr.GetSize();
			UINT_PTR allocBase = (chunkEnd - sz) & ~(alignment - 1);
			UINT_PTR allocEnd = allocBase + sz;

			if (chunkBase >= minAddress)
			{
				if (allocBase == chunkBase && allocEnd == chunkEnd)
				{
					bestChunkIdx = idx;
					minAddress = chunk.ptr;
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	for (size_t bucket = minBucket; (bestChunkIdx == InvalidChunkIdx) && (bucket < NumBuckets); ++bucket)
	{
		size_t rootIdx = m_freeBuckets[bucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];

		for (size_t idx = root.freePrevIdx; idx != rootIdx; idx = m_chunks[idx].freePrevIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max((uint32)1, (uint32)chunk.attr.GetSize()) >= (1U << bucket));

			UINT_PTR chunkBase = chunk.ptr;
			UINT_PTR chunkEnd = chunkBase + chunk.attr.GetSize();
			UINT_PTR allocBase = (chunkEnd - sz) & ~(alignment - 1);
			UINT_PTR allocEnd = allocBase + sz;

			if (chunkBase > minAddress)
			{
				if (chunkBase <= allocBase && allocEnd <= chunkEnd)
				{
					bestChunkIdx = idx;
					minAddress = chunkBase;
					break;
				}
			}
			else
			{
				// Chunks are ordered by address. If the minAddress has been exceeded, there's no point in searching the rest of the list.
				break;
			}
		}
	}

	return bestChunkIdx;
}

CDefragAllocator::PendingMove* CDefragAllocator::AllocPendingMove()
{
	// Naive - just search for a free slot.
	for (PendingMoveVec::iterator it = m_pendingMoves.begin(), itEnd = m_pendingMoves.end(); it != itEnd; ++it)
	{
		if (it->dstChunkIdx == InvalidChunkIdx)
			return &*it;
	}

	return NULL;
}

void CDefragAllocator::FreePendingMove(PendingMove* pMove)
{
	stl::reconstruct(*pMove);
}

void CDefragAllocator::CancelMove(Index srcChunkIdx, bool bIsContentNeeded)
{
	CryOptionalAutoLock<CryCriticalSection> lock(m_lock, m_isThreadSafe);
	CancelMove_Locked(srcChunkIdx, bIsContentNeeded);
}

void CDefragAllocator::CancelMove_Locked(Index srcChunkIdx, bool bIsContentNeeded)
{
	SDefragAllocChunk* pChunk = &m_chunks[srcChunkIdx];

	// In theory CancelMove could get called twice for the same chunk - make sure that
	// redundant cancels are ignored
	if (pChunk->attr.IsMoving())
	{
		// Chunk is being moved. Find the job responsible and cancel it (if it's not complete).
		PendingMoveVec::iterator it = std::find_if(m_pendingMoves.begin(), m_pendingMoves.end(), PendingMoveSrcChunkPredicate(srcChunkIdx));
		CDBA_ASSERT(it != m_pendingMoves.end());
		CDBA_ASSERT(it->srcChunkIdx != InvalidChunkIdx);

		if (it->notify.bDstIsValid && it->notify.bSrcIsUnneeded)
		{
			// Chunk has actually completed, just not finalised. Discard the destination chunk.
			MarkAsFree(m_chunks[it->dstChunkIdx]);
			MergeFreeBlock(it->dstChunkIdx);
			FreePendingMove(&*it);
		}
		else
		{
			// Chunk is still in flight.

			// If the content is important, then we can either cancel the copy (if not overlapped), or
			// sync and wait for it complete. TODO.

			m_policy.pDefragPolicy->CancelCopy(it->userMoveId, pChunk->pContext, false);
			it->srcChunkIdx = InvalidChunkIdx;
			it->cancelled = true;
		}

		MarkAsNotMoving(*pChunk);
		++m_nCancelledMoves;
	}
}

void CDefragAllocator::Relocate(uint32 userMoveId, Index srcChunkIdx, Index dstChunkIdx)
{
	using std::swap;

	SDefragAllocChunk* pChunks = &m_chunks[0];
	uint32 logAlignment = m_logMinAlignment;

	SDefragAllocChunk& src = pChunks[srcChunkIdx];
	SDefragAllocChunk& dst = pChunks[dstChunkIdx];
	SDefragAllocChunkAttr srcAttr = src.attr;
	SDefragAllocChunkAttr dstAttr = dst.attr;

	CDBA_ASSERT(srcAttr.IsMoving());
	CDBA_ASSERT(dstAttr.IsPinned());
	CDBA_ASSERT(!dstAttr.IsMoving());
	CDBA_ASSERT(srcAttr.GetSize() == dstAttr.GetSize());

	UINT_PTR oldPtr = src.ptr;
	UINT_PTR newPtr = dst.ptr;

	// src and dst are linked in the addr chain only - src needs to move where dst is (so user handles remain consistent)
	// and the src block be freed.

	SDefragAllocChunk* pA = &dst;
	SDefragAllocChunk* pB = &src;

	CDBA_ASSERT(pChunks[pA->addrPrevIdx].ptr + pChunks[pA->addrPrevIdx].attr.GetSize() == pA->ptr);
	CDBA_ASSERT(pA->ptr + pA->attr.GetSize() == pChunks[pA->addrNextIdx].ptr);
	CDBA_ASSERT(pChunks[pB->addrPrevIdx].ptr + pChunks[pB->addrPrevIdx].attr.GetSize() == pB->ptr);
	CDBA_ASSERT(pB->ptr + pB->attr.GetSize() == pChunks[pB->addrNextIdx].ptr);
	CDBA_ASSERT(&pChunks[pChunks[pA->addrNextIdx].addrPrevIdx] == pA);
	CDBA_ASSERT(&pChunks[pChunks[pA->addrPrevIdx].addrNextIdx] == pA);
	CDBA_ASSERT(&pChunks[pChunks[pB->addrNextIdx].addrPrevIdx] == pB);
	CDBA_ASSERT(&pChunks[pChunks[pB->addrPrevIdx].addrNextIdx] == pB);

	Index nA = dstChunkIdx;
	Index nB = srcChunkIdx;

	if (pA->addrNextIdx == nB)
	{
		// Neighbours
		pB->addrPrevIdx = pA->addrPrevIdx;
		pA->addrNextIdx = pB->addrNextIdx;
		pA->addrPrevIdx = nB;
		pB->addrNextIdx = nA;
		pChunks[pB->addrPrevIdx].addrNextIdx = nB;
		pChunks[pA->addrNextIdx].addrPrevIdx = nA;
	}
	else if (pA->addrPrevIdx == nB)
	{
		// Neighbours
		pB->addrNextIdx = pA->addrNextIdx;
		pA->addrPrevIdx = pB->addrPrevIdx;
		pB->addrPrevIdx = nA;
		pA->addrNextIdx = nB;
		pChunks[pA->addrPrevIdx].addrNextIdx = nA;
		pChunks[pB->addrNextIdx].addrPrevIdx = nB;
	}
	else
	{
		swap(pB->addrNextIdx, pA->addrNextIdx);
		swap(pA->addrPrevIdx, pB->addrPrevIdx);
		pChunks[pA->addrPrevIdx].addrNextIdx = nA;
		pChunks[pA->addrNextIdx].addrPrevIdx = nA;
		pChunks[pB->addrNextIdx].addrPrevIdx = nB;
		pChunks[pB->addrPrevIdx].addrNextIdx = nB;
	}

	swap(src.packedPtr, dst.packedPtr);

#ifdef CDBA_MORE_DEBUG
	ValidateAddressChain();
#endif

	CDBA_ASSERT(pChunks[pA->addrPrevIdx].ptr + pChunks[pA->addrPrevIdx].attr.GetSize() == pA->ptr);
	CDBA_ASSERT(pA->ptr + pA->attr.GetSize() == pChunks[pA->addrNextIdx].ptr);
	CDBA_ASSERT(pChunks[pB->addrPrevIdx].ptr + pChunks[pB->addrPrevIdx].attr.GetSize() == pB->ptr);
	CDBA_ASSERT(pB->ptr + pB->attr.GetSize() == pChunks[pB->addrNextIdx].ptr);
	CDBA_ASSERT(&pChunks[pChunks[pA->addrNextIdx].addrPrevIdx] == pA);
	CDBA_ASSERT(&pChunks[pChunks[pA->addrPrevIdx].addrNextIdx] == pA);
	CDBA_ASSERT(&pChunks[pChunks[pB->addrNextIdx].addrPrevIdx] == pB);
	CDBA_ASSERT(&pChunks[pChunks[pB->addrPrevIdx].addrNextIdx] == pB);

	if (userMoveId != IDefragAllocatorPolicy::InvalidUserMoveId)
		m_policy.pDefragPolicy->Relocate(userMoveId, src.pContext, newPtr << logAlignment, oldPtr << logAlignment, (UINT_PTR)srcAttr.GetSize() << logAlignment);
}

void CDefragAllocator::SyncMoveSegment(uint32 seg)
{
	SDefragAllocSegment& segment = m_segments[seg];
	SDefragAllocChunk& headSentinalChunk = m_chunks[segment.headSentinalChunkIdx];

	// Assert that the segment is empty.
	uint32 endAddress = segment.address + segment.capacity;

	Index chunkIdx = headSentinalChunk.addrNextIdx;
	SDefragAllocChunk* pChunk = &m_chunks[chunkIdx];

	int numValidSegs = seg;

	while (pChunk->ptr != endAddress)
	{
		if (pChunk->attr.IsBusy())
		{
			CDBA_ASSERT(!pChunk->attr.IsMoving());
			CDBA_ASSERT(!pChunk->attr.IsPinned());
			CDBA_ASSERT(m_policy.pDefragPolicy != NULL);

#if (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
			size_t chunkAlign = BIT64(pChunk->logAlign);
#else
			size_t chunkAlign = BIT(pChunk->logAlign);
#endif

			// Try and synchronously move the allocation
			Index destinationChunkIdx = InvalidChunkIdx;
			for (int segIdx = 0; destinationChunkIdx == InvalidChunkIdx && segIdx < numValidSegs; ++segIdx)
				destinationChunkIdx = BestFit_FindFreeBlockForSegment(pChunk->attr.GetSize(), chunkAlign, segIdx);

			CDBA_ASSERT(destinationChunkIdx != InvalidChunkIdx);

			SplitResult sr = SplitFreeBlock(destinationChunkIdx, pChunk->attr.GetSize(), chunkAlign, true);
			CDBA_ASSERT(sr.bSuccessful);

			SDefragAllocChunk& destinationChunk = m_chunks[destinationChunkIdx];
			MarkAsInUse(destinationChunk);

			Pin(ChunkHdlFromIdx(destinationChunkIdx));
			MarkAsMoving(*pChunk);

			m_policy.pDefragPolicy->SyncCopy(pChunk->pContext, destinationChunk.ptr << m_logMinAlignment, pChunk->ptr << m_logMinAlignment, (UINT_PTR)pChunk->attr.GetSize() << m_logMinAlignment);
			Relocate(IDefragAllocatorPolicy::InvalidUserMoveId, chunkIdx, destinationChunkIdx);

			MarkAsNotMoving(*pChunk);
			Unpin(ChunkHdlFromIdx(destinationChunkIdx));

			MarkAsFree(destinationChunk);
			destinationChunkIdx = MergeFreeBlock(destinationChunkIdx);
			pChunk = &m_chunks[destinationChunkIdx];
		}

		chunkIdx = pChunk->addrNextIdx;
		pChunk = &m_chunks[chunkIdx];
	}
}

void CDefragAllocator::RebuildFreeLists()
{
	for (int bi = 0; bi < NumBuckets; ++bi)
	{
		Index ci = m_freeBuckets[bi];
		m_chunks[ci].freeNextIdx = ci;
		m_chunks[ci].freePrevIdx = ci;
	}

	for (Index ci = m_chunks[AddrStartSentinal].addrNextIdx; ci != AddrEndSentinal; ci = m_chunks[ci].addrNextIdx)
	{
		SDefragAllocChunk& ch = m_chunks[ci];
		if (!ch.attr.IsBusy())
		{
			LinkFreeChunk(ci);
		}
	}

#ifdef CDBA_MORE_DEBUG
	ValidateFreeLists();
#endif
}

void CDefragAllocator::ValidateAddressChain()
{
	UINT_PTR p = 0;

	Index hdr = 0;
	for (Index ci = m_chunks[hdr].addrNextIdx; ci != hdr; )
	{
		SDefragAllocChunk& c = m_chunks[ci];

		CDBA_ASSERT(p == c.ptr);
		p += c.attr.GetSize();

		ci = c.addrNextIdx;
	}

	for (int bi = 0; bi < NumBuckets; ++bi)
	{
		hdr = m_freeBuckets[bi];

		for (Index ci = m_chunks[hdr].freeNextIdx; ci != hdr; )
		{
			SDefragAllocChunk& c = m_chunks[ci];

			CDBA_ASSERT(!c.attr.IsBusy());

			ci = c.freeNextIdx;
		}
	}
}

void CDefragAllocator::ValidateFreeLists()
{
	for (size_t bucket = 0; bucket < NumBuckets; ++bucket)
	{
		size_t rootIdx = m_freeBuckets[bucket];
		SDefragAllocChunk& root = m_chunks[rootIdx];

		for (size_t idx = root.freeNextIdx; idx != rootIdx; idx = m_chunks[idx].freeNextIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max((uint32)1, (uint32)chunk.attr.GetSize()) >= (1U << bucket));
			CDBA_ASSERT(idx >= NumBuckets + 2);

			CDBA_ASSERT(m_chunks[chunk.freeNextIdx].freePrevIdx == idx);
			CDBA_ASSERT(m_chunks[chunk.freePrevIdx].freeNextIdx == idx);
		}

		for (size_t idx = root.freePrevIdx; idx != rootIdx; idx = m_chunks[idx].freePrevIdx)
		{
			SDefragAllocChunk& chunk = m_chunks[idx];

			CDBA_ASSERT(!chunk.attr.IsBusy());
			CDBA_ASSERT(max((uint32)1, (uint32)chunk.attr.GetSize()) >= (1U << bucket));
			CDBA_ASSERT(idx >= NumBuckets + 2);

			CDBA_ASSERT(m_chunks[chunk.freeNextIdx].freePrevIdx == idx);
			CDBA_ASSERT(m_chunks[chunk.freePrevIdx].freeNextIdx == idx);
		}
	}
}
