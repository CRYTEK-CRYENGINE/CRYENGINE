// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_AudioImplementation
#include <CryCore/Platform/platform.h>
#include <CryCore/StlUtils.h>
#include <CryCore/Project/ProjectDefines.h>

#include <SoundAllocator.h>

#if !defined(_RELEASE)
// Define this to enable logging via CAudioLogger.
// We disable logging for Release builds
	#define ENABLE_AUDIO_LOGGING
#endif // _RELEASE

#include <AudioLogger.h>
#include <fmod_studio.hpp>

extern CSoundAllocator g_audioImplMemoryPool;
extern CAudioLogger g_audioImplLogger;

#define AUDIO_ALLOCATOR_MEMORY_POOL g_audioImplMemoryPool
#include <STLSoundAllocator.h>

#if !defined(_RELEASE)
	#define INCLUDE_FMOD_IMPL_PRODUCTION_CODE
#endif // _RELEASE

#if CRY_PLATFORM_DURANGO
	#define PROVIDE_FMOD_IMPL_SECONDARY_POOL
#endif // CRY_PLATFORM_DURANGO

// Memory Allocation
#if defined(PROVIDE_FMOD_IMPL_SECONDARY_POOL)
	#include <CryMemory/CryPool/PoolAlloc.h>

typedef NCryPoolAlloc::CThreadSafe<NCryPoolAlloc::CBestFit<NCryPoolAlloc::CReferenced<NCryPoolAlloc::CMemoryDynamic, 4*1024, true>, NCryPoolAlloc::CListItemReference>> tMemoryPoolReferenced;

extern tMemoryPoolReferenced g_audioImplMemoryPoolSecondary;

//////////////////////////////////////////////////////////////////////////
inline void* Secondary_Allocate(size_t const nSize)
{
	// Secondary Memory is Referenced. To not loose the handle, more memory is allocated
	// and at the beginning the handle is saved.

	/* Allocate in Referenced Secondary Pool */
	uint32 const allocHandle = g_audioImplMemoryPoolSecondary.Allocate<uint32>(nSize, AUDIO_MEMORY_ALIGNMENT);
	CRY_ASSERT(allocHandle > 0);
	void* pAlloc = NULL;

	if (allocHandle > 0)
	{
		pAlloc = g_audioImplMemoryPoolSecondary.Resolve<void*>(allocHandle);
	}

	return pAlloc;
}

//////////////////////////////////////////////////////////////////////////
inline bool Secondary_Free(void* pFree)
{
	// Secondary Memory is Referenced. To not loose the handle, more memory is allocated
	// and at the beginning the handle is saved.

	// retrieve handle
	bool bFreed = (pFree == NULL);//true by default when passing NULL
	uint32 const allocHandle = g_audioImplMemoryPoolSecondary.AddressToHandle(pFree);

	if (allocHandle > 0)
	{
		bFreed = g_audioImplMemoryPoolSecondary.Free(allocHandle);
	}

	return bFreed;
}
#endif // PROVIDE_FMOD_IMPL_SECONDARY_POOL

// Windows or XboxOne
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
#endif

// Windows32
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
#endif

// Windows64
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
#endif

// XboxOne
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_DURANGO
#endif

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_ORBIS
#endif

// Mac
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_MAC
#endif

// Android
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_ANDROID
#endif

// IOS
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_IOS
#endif

// Linux
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_LINUX
#endif
