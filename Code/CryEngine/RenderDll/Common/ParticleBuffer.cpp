// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleBuffer.h"
#include <CryRenderer/RenderElements/CREParticle.h>
#include "DriverD3D.h"

#if CRY_PLATFORM_DESKTOP || CRY_PLATFORM_CONSOLE
	#define PFX_32BIT_IB
#else
	#define PFX_16BIT_IB
#endif

namespace
{

	bool IsStream(uint flags)
	{
		return (flags & CDeviceObjectFactory::BIND_VERTEX_BUFFER) || (flags & CDeviceObjectFactory::BIND_INDEX_BUFFER);
	}

}

CParticleSubBuffer::CParticleSubBuffer()
	: m_handle(0)
	, m_buffer()
	, m_pLockedData(nullptr)
	, m_flags(0)
{
}

CParticleSubBuffer::~CParticleSubBuffer()
{
	Release();
}

void CParticleSubBuffer::Create(uint elemsCount, uint stride, DXGI_FORMAT format, uint32 flags)
{
	m_flags = flags;
	if (IsStream(flags))
	{
		const bool useMemoryPool = false;
		const uint size = elemsCount * stride;
		SStreamInfo streamInfo;
		m_handle = gRenDev->m_DevBufMan.Create(
			(flags & CDeviceObjectFactory::BIND_VERTEX_BUFFER) ? BBT_VERTEX_BUFFER : BBT_INDEX_BUFFER,
			BU_DYNAMIC, size, useMemoryPool);
		streamInfo.hStream = m_handle;
		streamInfo.nStride = (flags & CDeviceObjectFactory::BIND_VERTEX_BUFFER) ? stride : format;
		streamInfo.nSlot = 0;
		if (flags & CDeviceObjectFactory::BIND_VERTEX_BUFFER)
			m_stream = GetDeviceObjectFactory().CreateVertexStreamSet(1, &streamInfo);
		else
			m_stream = GetDeviceObjectFactory().CreateIndexStreamSet(&streamInfo);
	}
	else
	{
		m_buffer.Create(elemsCount, stride, format, flags, nullptr);
	}
}

void CParticleSubBuffer::Release()
{
	Unlock(0);
	m_buffer.Release();
	if (m_handle)
		gRenDev->m_DevBufMan.Destroy(m_handle);
	m_handle = 0;
	m_flags = 0;
	m_stream = nullptr;
}

byte* CParticleSubBuffer::Lock()
{
	if (m_pLockedData)
		return m_pLockedData;

	if (IsStream(m_flags))
	{
		m_pLockedData = reinterpret_cast<byte*>(gRenDev->m_DevBufMan.BeginWrite(m_handle));
	}
	else if (m_buffer.GetDevBuffer())
	{
		m_pLockedData = reinterpret_cast<byte*>(m_buffer.Lock());
	}

	return m_pLockedData;
}

void CParticleSubBuffer::Unlock(uint32 size)
{
	if (m_pLockedData)
	{
		if (IsStream(m_flags))
		{
			gRenDev->m_DevBufMan.EndReadWrite(m_handle);
		}
		else if (m_buffer.GetDevBuffer())
		{
			m_buffer.Unlock(size);
		}

		m_pLockedData = nullptr;
	}
}

CParticleBufferSet::CParticleBufferSet()
	: m_valid(false)
	, m_maxSpriteCount(0)
{
	for (auto& fence : m_fences)
	{
		fence = 0;
	}
	Release();
}

CParticleBufferSet::~CParticleBufferSet()
{
	Release();
}

void CParticleBufferSet::Create(uint poolSize, uint maxPoolSize)
{
	for (auto& fence : m_fences)
		GetDeviceObjectFactory().CreateFence(fence);

	m_subBufferVertices.Create(poolSize, maxPoolSize);
	m_subBufferIndices.Create(poolSize, maxPoolSize);
	m_subBufferPositions.Create(poolSize, maxPoolSize);
	m_subBufferAxes.Create(poolSize, maxPoolSize);
	m_subBufferColors.Create(poolSize, maxPoolSize);

	CreateSpriteBuffer(poolSize);
	m_valid = true;
}

void CParticleBufferSet::CreateSpriteBuffer(uint poolSize)
{
#if defined(PFX_16BIT_IB)
	typedef uint16 idxFormat;
	m_maxSpriteCount = 1 << 14; // max sprite count for 16 bit ibs is 2^16/4
	const auto format = DXGI_FORMAT_R16_UINT;
#elif defined(PFX_32BIT_IB)
	typedef uint32 idxFormat;
	m_maxSpriteCount = poolSize;
	const auto format = DXGI_FORMAT_R32_UINT;
#endif

	std::vector<idxFormat> spriteIndices;
	spriteIndices.resize(m_maxSpriteCount * 6);
	for (uint particleId = 0; particleId < m_maxSpriteCount; ++particleId)
	{
		const uint baseIndexIdx = particleId * 6;
		const uint baseVertIdx = particleId * 4;
		const uint quadTessIdx[] = { 0, 1, 2, 2, 1, 3 };
		for (uint i = 0; i < 6; ++i)
			spriteIndices[baseIndexIdx + i] = idxFormat(baseVertIdx + quadTessIdx[i]);
	}

	m_spriteIndexBufferHandle = gRenDev->m_DevBufMan.Create(
		BBT_INDEX_BUFFER, BU_STATIC,
		spriteIndices.size() * sizeof(idxFormat));
	gRenDev->m_DevBufMan.UpdateBuffer(
		m_spriteIndexBufferHandle, spriteIndices.data(),
		spriteIndices.size() * sizeof(idxFormat));
	SStreamInfo streamInfo;
	streamInfo.hStream = m_spriteIndexBufferHandle;
	streamInfo.nStride = RenderIndexType::Index32;
	streamInfo.nSlot = 0;
	m_spriteIndexBufferStream = GetDeviceObjectFactory().CreateIndexStreamSet(&streamInfo);
}

void CParticleBufferSet::Release()
{
	for (auto& fence : m_fences)
	{
		if (fence != 0)
		{
			GetDeviceObjectFactory().ReleaseFence(fence);
			fence = 0;
		}
	}

	m_subBufferVertices.Release();
	m_subBufferIndices.Release();
	m_subBufferPositions.Release();
	m_subBufferAxes.Release();
	m_subBufferColors.Release();

	if (m_spriteIndexBufferHandle)
		gRenDev->m_DevBufMan.Destroy(m_spriteIndexBufferHandle);
	m_spriteIndexBufferHandle = 0;

	m_maxSpriteCount = 0;
	m_valid = false;
}

void CParticleBufferSet::Lock(int frameId)
{
	assert(m_valid);
	assert(gRenDev->m_pRT->IsRenderThread());


	const uint lockId = frameId % CREParticle::numBuffers;

	m_subBufferVertices.Lock(lockId);
	m_subBufferIndices.Lock(lockId);

	// Those buffers share an offset
	const auto offset = m_subBufferPositions.m_offset[lockId];
	m_subBufferPositions.Lock(lockId, offset);
	m_subBufferAxes.Lock(lockId, offset);
	m_subBufferColors.Lock(lockId, offset);
}

void CParticleBufferSet::Unlock(int frameId)
{
	assert(m_valid);
	assert(gRenDev->m_pRT->IsRenderThread());

	const uint unlockId = frameId % CREParticle::numBuffers;

	m_subBufferVertices.Unlock(unlockId);
	m_subBufferIndices.Unlock(unlockId);

	// Those buffers share an offset
	const auto end = m_subBufferPositions.m_offset[unlockId];
	m_subBufferPositions.Unlock(unlockId, end);
	m_subBufferAxes.Unlock(unlockId, end);
	m_subBufferColors.Unlock(unlockId, end);
}

void CParticleBufferSet::SetFence(int frameId)
{
#if BUFFER_ENABLE_DIRECT_ACCESS == 1
	assert(m_valid);
	assert(gRenDev->m_pRT->IsRenderThread());

	const uint bindId = frameId % CREParticle::numBuffers;
	GetDeviceObjectFactory().IssueFence(m_fences[bindId]);
#endif
}

void CParticleBufferSet::WaitForFence(int frameId)
{
#if BUFFER_ENABLE_DIRECT_ACCESS == 1
	assert(m_valid);

	const uint cvId = frameId % CREParticle::numBuffers;
	GetDeviceObjectFactory().SyncFence(m_fences[cvId], true, false);
#endif
}

uint CParticleBufferSet::GetAllocId(int frameId) const
{
	const uint bindId = frameId % CREParticle::numBuffers;
	return bindId;
}

CParticleBufferSet::SAlloc CParticleBufferSet::AllocVertices(uint allocId, uint numElems)
{
	CRY_ASSERT(m_subBufferVertices.m_pMemoryBase[allocId]);

	const auto allocation = AllocBuffer(m_subBufferVertices, allocId, numElems);
	return SAlloc{ reinterpret_cast<byte*>(m_subBufferVertices.m_pMemoryBase[allocId]), allocation.first, allocation.second };
}

CParticleBufferSet::SAlloc CParticleBufferSet::AllocIndices(uint allocId, uint numElems)
{
	CRY_ASSERT(m_subBufferIndices.m_pMemoryBase[allocId]);

	const auto allocation = AllocBuffer(m_subBufferIndices, allocId, numElems);
	return SAlloc{ reinterpret_cast<byte*>(m_subBufferIndices.m_pMemoryBase[allocId]), allocation.first, allocation.second };
}

CParticleBufferSet::SAllocStreams CParticleBufferSet::AllocStreams(uint allocId, uint numElems)
{
	CRY_ASSERT(m_subBufferPositions.m_pMemoryBase[allocId] &&
		m_subBufferAxes.m_pMemoryBase[allocId] &&
		m_subBufferColors.m_pMemoryBase[allocId]);

	const auto allocation = AllocBuffer(m_subBufferPositions, allocId, numElems);

	SAllocStreams allocOut;
	allocOut.m_pPositions = m_subBufferPositions.m_pMemoryBase[allocId] + allocation.first;
	allocOut.m_pAxes = m_subBufferAxes.m_pMemoryBase[allocId] + allocation.first;
	allocOut.m_pColorSTs = m_subBufferColors.m_pMemoryBase[allocId] + allocation.first;
	allocOut.m_firstElem = allocation.first;
	allocOut.m_numElemns = allocation.second;

	return allocOut;
}

bool CParticleBufferSet::IsValid(int frameId) const
{
	const uint bindId = frameId % CREParticle::numBuffers;
	return
		m_valid &&
		m_subBufferVertices.m_pMemoryBase[bindId] &&
		m_subBufferIndices.m_pMemoryBase[bindId] &&
		m_subBufferPositions.m_pMemoryBase[bindId] &&
		m_subBufferAxes.m_pMemoryBase[bindId] &&
		m_subBufferColors.m_pMemoryBase[bindId];
}
