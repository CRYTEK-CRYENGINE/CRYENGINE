// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:Cry_Math.h
//	Description: Misc mathematical functions
//
//	History:
//	-Jan 31,2001: Created by Marco Corbetta
//	-Jan 04,2003: SSE and 3DNow optimizations by Andrey Honich
//
//////////////////////////////////////////////////////////////////////

//
#ifndef CRY_MEMORYACCESS_H
#define CRY_MEMORYACCESS_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryCore/Platform/platform.h>

#if CRY_PLATFORM_DURANGO // no prefetching on durango, as it costs more than it gains
	#define PrefetchLine(ptr, off) ((ptr),(off))
	#define ResetLine128(ptr, off) (void)(0)
	#define FlushLine128(ptr, off) (void)(0)
#else
	#define PrefetchLine(ptr, off) cryPrefetchT0SSE((void*)((uint8*)(ptr) + off))
	#define ResetLine128(ptr, off) (void)(0)
	#define FlushLine128(ptr, off) (void)(0)
#endif

//========================================================================================

// cryMemcpy flags
#define MC_CPU_TO_GPU 0x10
#define MC_GPU_TO_CPU 0x20
#define MC_CPU_TO_CPU 0x40

#if CRY_PLATFORM_SSE2
	#if CRY_PLATFORM_X64
		#include <xmmintrin.h>
	#endif

	#define _MM_PREFETCH(MemPtr, Hint)              _mm_prefetch((MemPtr), (Hint));
	#define _MM_PREFETCH_LOOP(nCount, MemPtr, Hint) { for (int p = 0; p < nCount; p += 64) { _mm_prefetch((const char*)(MemPtr) + p, Hint); } \
	  }
#else
	#define _MM_PREFETCH(MemPtr, Hint)
	#define _MM_PREFETCH_LOOP(nCount, MemPtr, Hint)
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
//! Define this for Mac and Linux since it is used with the pthread sources.
	#define mymemcpy16 memcpy
#endif

//==========================================================================================
// 3DNow! optimizations

#pragma warning(push)
#pragma warning(disable:4731) // frame pointer register 'ebp' modified by inline assembly code

ILINE void cryPrefetchT0SSE(const void* src)
{
	_MM_PREFETCH((char*)src, _MM_HINT_T0);
}

//=================================================================================

// Very optimized memcpy() routine for AMD Athlon and Duron family.
// This code uses any of FOUR different basic copy methods, depending
// on the transfer size.
// NOTE:  Since this code uses MOVNTQ (also known as "Non-Temporal MOV" or
// "Streaming Store"), and also uses the software prefetch instructions,
// be sure you're running on Athlon/Duron or other recent CPU before calling!

#define TINY_BLOCK_COPY 64            //!< Upper limit for movsd type copy.
// The smallest copy uses the X86 "movsd" instruction, in an optimized
// form which is an "unrolled loop".

#define IN_CACHE_COPY 64 * 1024       //!< Upper limit for movq/movq copy w/SW prefetch.
// Next is a copy that uses the MMX registers to copy 8 bytes at a time,
// also using the "unrolled loop" optimization.   This code uses
// the software prefetch instruction to get the data into the cache.

#define UNCACHED_COPY 197 * 1024      //!<U pper limit for movq/movntq w/SW prefetch.
// For larger blocks, which will spill beyond the cache, it's faster to
// use the Streaming Store instruction MOVNTQ.   This write instruction
// bypasses the cache and writes straight to main memory.  This code also
// uses the software prefetch instruction to pre-read the data.
// USE 64 * 1024 FOR THIS VALUE IF YOU'RE ALWAYS FILLING A "CLEAN CACHE".

#define BLOCK_PREFETCH_COPY infinity  //!< No limit for movq/movntq w/block prefetch.
#define CACHEBLOCK          80h       //!< Number of 64-byte blocks (cache lines) for block prefetch.
// For the largest size blocks, a special technique called Block Prefetch
// can be used to accelerate the read operations.   Block Prefetch reads
// one address per cache line, for a series of cache lines, in a short loop.
// This is faster than using software prefetch.  The technique is great for
// getting maximum read bandwidth, especially in DDR memory systems.

const int PREFNTA_BLOCK = 0x4000;

ILINE void cryMemcpy(void* Dst, const void* Src, int n)
{
	char* dst = (char*)Dst;
	char* src = (char*)Src;
	while (n > PREFNTA_BLOCK)
	{

		_MM_PREFETCH_LOOP(PREFNTA_BLOCK, src, _MM_HINT_NTA);

		memcpy(dst, src, PREFNTA_BLOCK);
		src += PREFNTA_BLOCK;
		dst += PREFNTA_BLOCK;
		n -= PREFNTA_BLOCK;
	}
	_MM_PREFETCH_LOOP(n, src, _MM_HINT_NTA);
	memcpy(dst, src, n);
}

ILINE void cryMemcpy(void* Dst, const void* Src, int n, int nFlags)
{
	char* dst = (char*)Dst;
	char* src = (char*)Src;
	while (n > PREFNTA_BLOCK)
	{
		_MM_PREFETCH_LOOP(PREFNTA_BLOCK, src, _MM_HINT_NTA);
		memcpy(dst, src, PREFNTA_BLOCK);
		src += PREFNTA_BLOCK;
		dst += PREFNTA_BLOCK;
		n -= PREFNTA_BLOCK;
	}
	_MM_PREFETCH_LOOP(n, src, _MM_HINT_NTA);
	memcpy(dst, src, n);
}

#pragma warning(pop)

#if CRY_PLATFORM_DURANGO
//! Empty implementations for durango, as prefetching is not good for performance there.
ILINE void CryPrefetch(const void* const cpSrc) {}
#else
//! Implement something usual to bring one memory location into L1 data cache.
ILINE void CryPrefetch(const void* const cpSrc)
{
	cryPrefetchT0SSE(cpSrc);
}
#endif

#define CryPrefetchInl CryPrefetch

#endif //math
