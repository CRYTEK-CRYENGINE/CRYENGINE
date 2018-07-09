// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IDEFRAGALLOCATOR_H
#define IDEFRAGALLOCATOR_H

struct IDefragAllocatorStats
{
	size_t nCapacity;
	size_t nInUseSize;
	uint32 nInUseBlocks;
	uint32 nFreeBlocks;
	uint32 nPinnedBlocks;
	uint32 nMovingBlocks;
	uint32 nLargestFreeBlockSize;
	uint32 nSmallestFreeBlockSize;
	uint32 nMeanFreeBlockSize;
	uint32 nCancelledMoveCount;
};

struct IDefragAllocatorCopyNotification
{
	IDefragAllocatorCopyNotification()
		: bDstIsValid(false)
		, bSrcIsUnneeded(false)
		, bCancel(false)
	{
	}

	bool bDstIsValid;
	bool bSrcIsUnneeded;

	//! Flag to indicate that the copy can't be initiated after all - currently only cancelling before a relocate
	//! is begun is supported, and the destination region must be stable.
	bool bCancel;
};

class IDefragAllocatorPolicy
{
public:
	enum : uint32
	{
		InvalidUserMoveId = 0xffffffff
	};

public:
	// <interfuscator:shuffle>
	virtual uint32 BeginCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size, IDefragAllocatorCopyNotification* pNotification) = 0;
	virtual void   Relocate(uint32 userMoveId, void* pContext, UINT_PTR newOffset, UINT_PTR oldOffset, UINT_PTR size) = 0;
	virtual void   CancelCopy(uint32 userMoveId, void* pContext, bool bSync) = 0;

	//! Perform the copy and relocate immediately - will only be called when UnAppendSegment is.
	virtual void SyncCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size) = 0;
	
	virtual uint32 Hash(UINT_PTR offset, UINT_PTR size) { return 0; }
	// </interfuscator:shuffle>

protected:
	virtual ~IDefragAllocatorPolicy() {}
};

class IDefragAllocator
{
public:
	typedef uint32 Hdl;
	enum
	{
		InvalidHdl = 0,
	};

	struct AllocatePinnedResult
	{
		Hdl      hdl;
		UINT_PTR offs;
		UINT_PTR usableSize;
	};

	enum EBlockSearchKind
	{
		eBSK_BestFit,
		eBSK_FirstFit
	};

	struct Policy
	{
		Policy()
			: pDefragPolicy(NULL)
			, maxAllocs(0)
			, maxSegments(1)
			, blockSearchKind(eBSK_BestFit)
		{
		}

		IDefragAllocatorPolicy* pDefragPolicy;
		size_t                  maxAllocs;
		size_t                  maxSegments;
		EBlockSearchKind        blockSearchKind;
	};

public:
	// <interfuscator:shuffle>
	virtual void                  Release(bool bDiscard = false) = 0;

	virtual void                  Init(UINT_PTR capacity, UINT_PTR alignment, const Policy& policy = Policy()) = 0;

	virtual bool                  AppendSegment(UINT_PTR capacity) = 0;
	virtual void                  UnAppendSegment() = 0;

	virtual Hdl                   Allocate(size_t sz, const char* source, void* pContext = NULL) = 0;
	virtual Hdl                   AllocateAligned(size_t sz, size_t alignment, const char* source, void* pContext = NULL) = 0;
	virtual AllocatePinnedResult  AllocatePinned(size_t sz, const char* source, void* pContext = NULL) = 0;
	virtual AllocatePinnedResult  AllocateAlignedPinned(size_t sz, size_t alignment, const char* source, void* pContext = NULL) = 0;
	virtual bool                  Free(Hdl hdl) = 0;

	virtual void                  ChangeContext(Hdl hdl, void* pNewContext) = 0;
	virtual void*                 GetContext(Hdl hdl) = 0;

	virtual size_t                GetAllocated() const = 0;
	virtual IDefragAllocatorStats GetStats() = 0;

	virtual size_t                DefragmentTick(size_t maxMoves, size_t maxAmount, bool bForce = false) = 0;

	virtual UINT_PTR              UsableSize(Hdl hdl) = 0;

	//! Pin the chunk until the next defrag tick, when it will be automatically unpinned.
	virtual UINT_PTR WeakPin(Hdl hdl) = 0;

	//! Pin the chunk until Unpin is called.
	virtual UINT_PTR    Pin(Hdl hdl) = 0;

	virtual void        Unpin(Hdl hdl) = 0;

	virtual const char* GetSourceOf(Hdl hdl) = 0;
	// </interfuscator:shuffle>

#ifndef _RELEASE
	virtual void DumpState(const char* filename) = 0;
	virtual void RestoreState(const char* filename) = 0;
#endif

protected:
	virtual ~IDefragAllocator() {}
};

#endif
