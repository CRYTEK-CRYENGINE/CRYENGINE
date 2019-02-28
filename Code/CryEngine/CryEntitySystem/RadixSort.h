// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/**
 *	Contains source code from the article "Radix Sort Revisited".
 *	\file		IceRevisitedRadix.h
 *	\author		Pierre Terdiman
 *	\date		April, 4, 2000
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(NEED_ENDIAN_SWAP)
// The radix sort code below does not work (yet) on big-endian machines.
	#define RADIX_USE_STDSORT 1
#else
	#undef RADIX_USE_STDSORT
#endif

#if !defined RADIX_USE_STDSORT

//! Allocate histograms & offsets locally
	#define RADIX_LOCAL_RAM

enum RadixHint
{
	RADIX_SIGNED,     //!< Input values are signed
	RADIX_UNSIGNED,   //!< Input values are unsigned

	RADIX_FORCE_DWORD = 0x7fffffff
};

class RadixSort
{
public:
	// Constructor/Destructor
	RadixSort();
	~RadixSort();
	// Sorting methods
	RadixSort& Sort(const uint32* input, uint32 nb, RadixHint hint = RADIX_SIGNED);
	RadixSort& Sort(const float* input, uint32 nb);

	//! Access to results. mRanks is a list of indices in sorted order, i.e. in the order you may further process your data
	ILINE const uint32* GetRanks()      const { return mRanks;    }

	//! mIndices2 gets trashed on calling the sort routine, but otherwise you can recycle it the way you want.
	ILINE uint32* GetRecyclable()   const { return mRanks2;   }

	// Stats
	uint32       GetUsedRam()    const;
	//! Returns the total number of calls to the radix sorter.
	ILINE uint32 GetNbTotalCalls() const { return mTotalCalls; }
	//! Returns the number of eraly exits due to temporal coherence.
	ILINE uint32 GetNbHits()     const   { return mNbHits;   }

	void         GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:
	#ifndef RADIX_LOCAL_RAM
	uint32 * mHistogram;            //!< Counters for each byte
	uint32* mOffset;                //!< Offsets (nearly a cumulative distribution function)
	#endif
	uint32 mCurrentSize;            //!< Current size of the indices list
	uint32  mAllocatedSize;         //!< Current size of the indices list in memory
	uint32* mRanks;                 //!< Two lists, swapped each pass
	uint32* mRanks2;
	// Stats
	uint32  mTotalCalls;            //!< Total number of calls to the sort routine
	uint32  mNbHits;                //!< Number of early exits due to coherence
	// Internal methods
	void CheckResize(uint32 nb);
	bool Resize(uint32 nb);
};

#else // !RADIX_USE_STDSORT

enum RadixHint { RADIX_SIGNED, RADIX_UNSIGNED };

class RadixSort
{
	uint32*             mRanks[2];
	uint32              mAllocatedSize[2];
	uint32              mAllocatedSize2;
	static const uint32 cStaticSize = 1024;
	uint32              sRanks[2][cStaticSize];
	uint32              mRanksIndex;

	uint32* Swap(uint32 nb);

public:
	RadixSort();
	~RadixSort();

	RadixSort&    Sort(const uint32* input, uint32 nb, RadixHint hint = RADIX_SIGNED);
	RadixSort&    Sort(const float* input, uint32 nb);

	const uint32* GetRanks() const { return mRanks[mRanksIndex]; }

	void          GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

#endif // else !RADIX_USE_STDSORT
