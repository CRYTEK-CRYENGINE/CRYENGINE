// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleFixedSizeElementPool.h
//  Version:     v1.00
//  Created:     15/03/2010 by Corey.
//  Compilers:   Visual Studio.NET
//  Description: A fragmentation-free allocator from a fixed-size pool and which
//							 only allocates elements of the same size.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particlefixedsizeelementpool_h__
#define __particlefixedsizeelementpool_h__
#pragma once

#if !defined(_RELEASE)
	#define TRACK_PARTICLE_POOL_USAGE
#endif

///////////////////////////////////////////////////////////////////////////////
// thread safe pool for faster allocation/deallocation of particle container and alike objects
// allocate/deallocate are only a single atomic operations on a freelist
class ParticleObjectPool
{
public:
	ParticleObjectPool();
	~ParticleObjectPool();

	void                  Init(uint32 nBytesToAllocate);

	void*                 Allocate(uint32 nSize);
	void                  Deallocate(void* a_memoryToDeallocate, uint32 nSize);

	stl::SPoolMemoryUsage GetTotalMemory() const;
	void                  GetMemoryUsage(ICrySizer* pSizer);

	void                  ResetUsage();
private:
	void                  Init4KBFreeList();

	void*                 Allocate_128Byte();
	void*                 Allocate_256Byte();
	void*                 Allocate_512Byte();

	void                  Deallocate_128Byte(void* pPtr);
	void                  Deallocate_256Byte(void* pPtr);
	void                  Deallocate_512Byte(void* pPtr);

	SLockFreeSingleLinkedListHeader m_freeList4KB;  // master freelist of 4KB blocks they are splitted into the sub lists (merged back during ResetUsage)
	SLockFreeSingleLinkedListHeader m_freeList128Bytes;
	SLockFreeSingleLinkedListHeader m_freeList256Bytes;
	SLockFreeSingleLinkedListHeader m_freeList512Bytes;

	void*                           m_pPoolMemory;
	uint32                          m_nPoolCapacity;

#if defined(TRACK_PARTICLE_POOL_USAGE)
	int m_nUsedMemory;
	int m_nMaxUsedMemory;
	int m_nMemory128Bytes;
	int m_nMemory256Bytes;
	int m_nMemory512Bytes;
	int m_nMemory128BytesUsed;
	int m_nMemory256BytesUsed;
	int m_nMemory512Bytesused;
#endif
};

///////////////////////////////////////////////////////////////////////////////
inline stl::SPoolMemoryUsage ParticleObjectPool::GetTotalMemory() const
{
#if defined(TRACK_PARTICLE_POOL_USAGE)
	return stl::SPoolMemoryUsage(
	  (size_t)m_nPoolCapacity,
	  (size_t)m_nMaxUsedMemory,
	  (size_t)m_nUsedMemory);
#else
	return stl::SPoolMemoryUsage(0, 0, 0);
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void ParticleObjectPool::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->AddObject(m_pPoolMemory, m_nPoolCapacity);
}

///////////////////////////////////////////////////////////////////////////////
inline void ParticleObjectPool::ResetUsage()
{
	// merge all allocations back into 4KB
	Init4KBFreeList();
#if defined(TRACK_PARTICLE_POOL_USAGE)
	m_nUsedMemory = 0;
	m_nMaxUsedMemory = 0;
#endif
}

#endif // __fixedsizeelementpool_h__
