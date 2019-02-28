// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <PRT/SHAllocator.h>

#if defined(USE_MEM_ALLOCATOR)

static void* SHMalloc(size_t Size)
{
	return malloc(Size);
}

static void SHFreeSize(void* pPtr, size_t Size)
{
	return free(pPtr);
}

void LoadAllocatorModule(FNC_SHMalloc& pfnMalloc, FNC_SHFreeSize& pfnFree)
{
	pfnMalloc = &SHMalloc;
	pfnFree = &SHFreeSize;
}

CSHAllocator<unsigned char> gsByteAllocator;

#endif

