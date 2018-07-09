// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleFixedSizeElementPool.cpp
//  Version:     v1.00
//  Created:     15/03/2010 by Corey.
//  Compilers:   Visual Studio.NET
//  Description: A fragmentation-free allocator from a fixed-size pool and which
//							 only allocates elements of the same size.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleFixedSizeElementPool.h"
#include "ParticleUtils.h"

#ifndef _RELEASE //Debugging code for specific issue CE-10725
volatile int g_allocCounter = 0;
#endif

///////////////////////////////////////////////////////////////////////////////
ParticleObjectPool::ParticleObjectPool() :
	m_pPoolMemory(NULL),
	m_nPoolCapacity(0)
{
	CryInitializeSListHead(m_freeList4KB);
	CryInitializeSListHead(m_freeList128Bytes);
	CryInitializeSListHead(m_freeList256Bytes);
	CryInitializeSListHead(m_freeList512Bytes);

#if defined(TRACK_PARTICLE_POOL_USAGE)
	m_nUsedMemory = 0;
	m_nMaxUsedMemory = 0;
	m_nMemory128Bytes = 0;
	m_nMemory256Bytes = 0;
	m_nMemory512Bytes = 0;
	m_nMemory128BytesUsed = 0;
	m_nMemory256BytesUsed = 0;
	m_nMemory512Bytesused = 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
ParticleObjectPool::~ParticleObjectPool()
{
	CryInterlockedFlushSList(m_freeList4KB);
	CryInterlockedFlushSList(m_freeList128Bytes);
	CryInterlockedFlushSList(m_freeList256Bytes);
	CryInterlockedFlushSList(m_freeList512Bytes);

	CryModuleMemalignFree(m_pPoolMemory);
}

///////////////////////////////////////////////////////////////////////////////
void ParticleObjectPool::Init(uint32 nBytesToAllocate)
{
	m_nPoolCapacity = Align(nBytesToAllocate, 4 * 1024);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Particles Pool");

	// allocate memory block to use as pool
	if (m_pPoolMemory)
	{
		CryModuleMemalignFree(m_pPoolMemory);
		m_pPoolMemory = NULL;
	}
	if (m_nPoolCapacity)
	{
		m_pPoolMemory = CryModuleMemalign(m_nPoolCapacity, 128);
	}

	// fill 4KB freelist
	Init4KBFreeList();
}

///////////////////////////////////////////////////////////////////////////////
void* ParticleObjectPool::Allocate(uint32 nSize)
{
	void* pRet = NULL;

	if (nSize <= 128)
		pRet = Allocate_128Byte();
	else if (nSize <= 256)
		pRet = Allocate_256Byte();
	else if (nSize <= 512)
		pRet = Allocate_512Byte();
	else // fallback for the case that a a particle histy is bigger than 512 bytes
		pRet = malloc(nSize);

	return pRet;
}

///////////////////////////////////////////////////////////////////////////////
void ParticleObjectPool::Deallocate(void* a_memoryToDeallocate, uint32 nSize)
{
	if (nSize <= 128)
		Deallocate_128Byte(a_memoryToDeallocate);
	else if (nSize <= 256)
		Deallocate_256Byte(a_memoryToDeallocate);
	else if (nSize <= 512)
		Deallocate_512Byte(a_memoryToDeallocate);
	else
		free(a_memoryToDeallocate);
}

///////////////////////////////////////////////////////////////////////////////
void ParticleObjectPool::Init4KBFreeList()
{
	CryInterlockedFlushSList(m_freeList4KB);
	CryInterlockedFlushSList(m_freeList128Bytes);
	CryInterlockedFlushSList(m_freeList256Bytes);
	CryInterlockedFlushSList(m_freeList512Bytes);

	char* pElementMemory = (char*)m_pPoolMemory;
	for (int i = (m_nPoolCapacity / (4 * 1024)) - 1; i >= 0; --i)
	{
		// Build up the linked list of free nodes to begin with - do it in
		// reverse order so that consecutive allocations will be given chunks
		// in a cache-friendly order
		SLockFreeSingleLinkedListEntry* pNewNode = (SLockFreeSingleLinkedListEntry*)(pElementMemory + (i * (4 * 1024)));
		CryInterlockedPushEntrySList(m_freeList4KB, *pNewNode);
	}
}

///////////////////////////////////////////////////////////////////////////////
void* ParticleObjectPool::Allocate_128Byte()
{
	const uint32 nAllocationSize = 128;
	do
	{
		void* pListEntry = CryInterlockedPopEntrySList(m_freeList128Bytes);
		if (pListEntry)
		{
#if defined(TRACK_PARTICLE_POOL_USAGE)
#ifndef _RELEASE //Debugging code for specific issue CE-10725			
			CryInterlockedAdd(&g_allocCounter, 1);
#endif
			CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory128BytesUsed), nAllocationSize);
			const int nUsedMemory = CryInterlockedAdd(alias_cast<volatile int*>(&m_nUsedMemory), nAllocationSize);
			int nMaxUsed = ~0;
			do
			{
				nMaxUsed = *alias_cast<volatile int*>(&m_nMaxUsedMemory);
				if (nUsedMemory <= nMaxUsed)
					break;

			}
			while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nMaxUsedMemory), nUsedMemory, nMaxUsed) == nMaxUsed);
#ifndef _RELEASE //Debugging code for specific issue CE-10725			
			CryInterlockedAdd(&g_allocCounter, -1);
#endif
#endif
			return pListEntry;
		}

		// allocation failed, refill list from a 4KB block if we have one
		void* pMemoryBlock = CryInterlockedPopEntrySList(m_freeList4KB);

		// stop if we ran out of pool memory
		IF (pMemoryBlock == NULL, 0)
			return NULL;

#if defined(TRACK_PARTICLE_POOL_USAGE)
		CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory128Bytes), 4 * 1024);
#endif

		char* pElementMemory = (char*)pMemoryBlock;
		for (int i = ((4 * 1024) / (nAllocationSize)) - 1; i >= 0; --i)
		{
			// Build up the linked list of free nodes to begin with - do it in
			// reverse order so that consecutive allocations will be given chunks
			// in a cache-friendly order
			SLockFreeSingleLinkedListEntry* pNewNode = (SLockFreeSingleLinkedListEntry*)(pElementMemory + (i * (nAllocationSize)));
			CryInterlockedPushEntrySList(m_freeList128Bytes, *pNewNode);
		}

	}
	while (true);
}

///////////////////////////////////////////////////////////////////////////////
void* ParticleObjectPool::Allocate_256Byte()
{
	const uint32 nAllocationSize = 256;
	do
	{
		void* pListEntry = CryInterlockedPopEntrySList(m_freeList256Bytes);
		if (pListEntry)
		{
#if defined(TRACK_PARTICLE_POOL_USAGE)			
#ifndef _RELEASE //Debugging code for specific issue CE-10725			
			CryInterlockedAdd(&g_allocCounter, 1);
#endif
			CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory256BytesUsed), nAllocationSize);
			const int nUsedMemory = CryInterlockedAdd(alias_cast<volatile int*>(&m_nUsedMemory), nAllocationSize);
			int nMaxUsed = ~0;
			do
			{
				nMaxUsed = *alias_cast<volatile int*>(&m_nMaxUsedMemory);
				if (nUsedMemory <= nMaxUsed)
					break;

			}
			while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nMaxUsedMemory), nUsedMemory, nMaxUsed) == nMaxUsed);
#ifndef _RELEASE //Debugging code for specific issue CE-10725			
			CryInterlockedAdd(&g_allocCounter, -1);
#endif
#endif
			return pListEntry;
		}

		// allocation failed, refill list from a 4KB block if we have one
		void* pMemoryBlock = CryInterlockedPopEntrySList(m_freeList4KB);
		// stop if we ran out of pool memory
		IF (pMemoryBlock == NULL, 0)
			return NULL;

#if defined(TRACK_PARTICLE_POOL_USAGE)
		CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory256Bytes), 4 * 1024);
#endif

		char* pElementMemory = (char*)pMemoryBlock;
		for (int i = ((4 * 1024) / (nAllocationSize)) - 1; i >= 0; --i)
		{
			// Build up the linked list of free nodes to begin with - do it in
			// reverse order so that consecutive allocations will be given chunks
			// in a cache-friendly order
			SLockFreeSingleLinkedListEntry* pNewNode = (SLockFreeSingleLinkedListEntry*)(pElementMemory + (i * (nAllocationSize)));
			CryInterlockedPushEntrySList(m_freeList256Bytes, *pNewNode);
		}

	}
	while (true);
}

///////////////////////////////////////////////////////////////////////////////
void* ParticleObjectPool::Allocate_512Byte()
{
	const uint32 nAllocationSize = 512;
	do
	{
		void* pListEntry = CryInterlockedPopEntrySList(m_freeList512Bytes);
		if (pListEntry)
		{
#if defined(TRACK_PARTICLE_POOL_USAGE)
#ifndef _RELEASE //Debugging code for specific issue CE-10725			
			CryInterlockedAdd(&g_allocCounter, 1);
#endif
			CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory512Bytesused), nAllocationSize);
			const int nUsedMemory = CryInterlockedAdd(alias_cast<volatile int*>(&m_nUsedMemory), nAllocationSize);
			int nMaxUsed = ~0;
			do
			{
				nMaxUsed = *alias_cast<volatile int*>(&m_nMaxUsedMemory);
				if (nUsedMemory <= nMaxUsed)
					break;

			}
			while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nMaxUsedMemory), nUsedMemory, nMaxUsed) == nMaxUsed);
#ifndef _RELEASE //Debugging code for specific issue CE-10725			
			CryInterlockedAdd(&g_allocCounter, -1);
#endif
#endif
			return pListEntry;
		}

		// allocation failed, refill list from a 4KB block if we have one
		void* pMemoryBlock = CryInterlockedPopEntrySList(m_freeList4KB);
		// stop if we ran out of pool memory
		IF (pMemoryBlock == NULL, 0)
			return NULL;

#if defined(TRACK_PARTICLE_POOL_USAGE)
		CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory512Bytes), 4 * 1024);
#endif

		char* pElementMemory = (char*)pMemoryBlock;
		for (int i = ((4 * 1024) / (nAllocationSize)) - 1; i >= 0; --i)
		{
			// Build up the linked list of free nodes to begin with - do it in
			// reverse order so that consecutive allocations will be given chunks
			// in a cache-friendly order
			SLockFreeSingleLinkedListEntry* pNewNode = (SLockFreeSingleLinkedListEntry*)(pElementMemory + (i * (nAllocationSize)));
			CryInterlockedPushEntrySList(m_freeList512Bytes, *pNewNode);
		}

	}
	while (true);
}

///////////////////////////////////////////////////////////////////////////////
void ParticleObjectPool::Deallocate_128Byte(void* pPtr)
{
	IF (!pPtr, 0)
		return;

	CryInterlockedPushEntrySList(m_freeList128Bytes, *alias_cast<SLockFreeSingleLinkedListEntry*>(pPtr));
#if defined(TRACK_PARTICLE_POOL_USAGE)
	CryInterlockedAdd(alias_cast<volatile int*>(&m_nUsedMemory), -128);
	CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory128BytesUsed), -128);
#endif
}

///////////////////////////////////////////////////////////////////////////////
void ParticleObjectPool::Deallocate_256Byte(void* pPtr)
{
	IF (!pPtr, 0)
		return;

	CryInterlockedPushEntrySList(m_freeList256Bytes, *alias_cast<SLockFreeSingleLinkedListEntry*>(pPtr));
#if defined(TRACK_PARTICLE_POOL_USAGE)
	CryInterlockedAdd(alias_cast<volatile int*>(&m_nUsedMemory), -256);
	CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory256BytesUsed), -256);
#endif
}

///////////////////////////////////////////////////////////////////////////////
void ParticleObjectPool::Deallocate_512Byte(void* pPtr)
{
	IF (!pPtr, 0)
		return;

	CryInterlockedPushEntrySList(m_freeList512Bytes, *alias_cast<SLockFreeSingleLinkedListEntry*>(pPtr));
#if defined(TRACK_PARTICLE_POOL_USAGE)
	CryInterlockedAdd(alias_cast<volatile int*>(&m_nUsedMemory), -512);
	CryInterlockedAdd(alias_cast<volatile int*>(&m_nMemory512Bytesused), -512);
#endif
}
