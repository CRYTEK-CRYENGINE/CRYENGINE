// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleBuffer.h"
#include <CryRenderer/RenderElements/CREParticle.h>
#include "DriverD3D.h"

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_APPLE
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
	, m_elemCount(0)
	, m_stride(0)
	, m_flags(0)
{
}

CParticleSubBuffer::~CParticleSubBuffer()
{
	Release();
}

void CParticleSubBuffer::Create(uint elemsCount, uint stride, DXGI_FORMAT format, uint32 flags)
{
	m_elemCount = elemsCount;
	m_stride = stride;
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
	m_elemCount = 0;
	m_stride = 0;
	m_flags = 0;
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

void CParticleBufferSet::Create(uint poolSize)
{
	for (auto& fence : m_fences)
	{
		GetDeviceObjectFactory().CreateFence(fence);
	}

	CreateSubBuffer(
		EBT_Vertices, poolSize, sizeof(SVF_P3F_C4B_T4B_N3F2), DXGI_FORMAT_UNKNOWN,
		CDeviceObjectFactory::BIND_VERTEX_BUFFER | CDeviceObjectFactory::USAGE_CPU_WRITE);

	CreateSubBuffer(
		EBT_Indices, poolSize * 3, sizeof(uint16), DXGI_FORMAT_R16_UINT,
		CDeviceObjectFactory::BIND_INDEX_BUFFER | CDeviceObjectFactory::USAGE_CPU_WRITE);

	CreateSubBuffer(
		EBT_PositionsSRV, poolSize, sizeof(Vec3), DXGI_FORMAT_UNKNOWN,
		CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED);
	CreateSubBuffer(
		EBT_AxesSRV, poolSize, sizeof(SParticleAxes), DXGI_FORMAT_UNKNOWN,
		CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED);
	CreateSubBuffer(
		EBT_ColorSTsSRV, poolSize, sizeof(SParticleColorST), DXGI_FORMAT_UNKNOWN,
		CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED);

	CreateSpriteBuffer(poolSize);

	const uint processId = gRenDev->GetRenderThreadID();
	const uint fillId = gRenDev->GetMainThreadID();
	m_ids[fillId] = 1;
	m_ids[processId] = 0;

	m_valid = true;
}

void CParticleBufferSet::CreateSubBuffer(CParticleBufferSet::EBufferTypes type, uint elemsCount, uint stride, DXGI_FORMAT format, uint32 flags)
{
	SubBuffer& subBuffer = m_subBuffers[type];
	for (uint i = 0; i < CREParticle::numBuffers; ++i)
	{
		subBuffer.m_buffers[i].Create(elemsCount, stride, format, flags);
		subBuffer.m_pMemoryBase[i] = nullptr;
		subBuffer.m_offset[i] = 0;
	}
	subBuffer.m_availableMemory = elemsCount * stride;
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

	for (uint i = 0; i < EBT_Total; ++i)
	{
		SubBuffer& subBuffer = m_subBuffers[i];
		subBuffer.m_availableMemory = 0;
		for (uint j = 0; j < CREParticle::numBuffers; ++j)
		{
			subBuffer.m_buffers[j].Release();
			subBuffer.m_offset[j] = 0;
			subBuffer.m_pMemoryBase[j] = nullptr;
		}
	}

	if (m_spriteIndexBufferHandle)
		gRenDev->m_DevBufMan.Destroy(m_spriteIndexBufferHandle);
	m_spriteIndexBufferHandle = 0;

	m_maxSpriteCount = 0;
	m_valid = false;
}

void CParticleBufferSet::Lock()
{
	assert(m_valid);
	assert(gRenDev->m_pRT->IsRenderThread());

	const uint processId = gRenDev->GetRenderThreadID();
	const uint fillId = gRenDev->GetMainThreadID();
	if (CRenderer::CV_r_multithreaded)
	{
		m_ids[fillId] = (m_ids[processId] + 1) % CREParticle::numBuffers;
		m_ids[processId] = (m_ids[processId] + (CREParticle::numBuffers - 1)) % CREParticle::numBuffers;
	}
	else
	{
		m_ids[processId] = (m_ids[processId] + 1) % CREParticle::numBuffers;
	}
	const uint lockId = m_ids[processId];

	for (auto& subBuffer : m_subBuffers)
		subBuffer.m_buffers[lockId].Unlock(0);

	for (auto& subBuffer : m_subBuffers)
	{
		subBuffer.m_pMemoryBase[lockId] = subBuffer.m_buffers[lockId].Lock();
		subBuffer.m_offset[lockId] = 0;
	}
}

void CParticleBufferSet::Unlock()
{
	assert(m_valid);
	assert(gRenDev->m_pRT->IsRenderThread());

	const uint processId = gRenDev->GetRenderThreadID();
	const uint bindId = m_ids[processId];

	const uint numPulledElements = m_subBuffers[EBT_PositionsSRV].m_offset[bindId] / m_subBuffers[EBT_PositionsSRV].m_buffers[bindId].GetStride();
	m_subBuffers[EBT_AxesSRV].m_offset[bindId] = numPulledElements * m_subBuffers[EBT_AxesSRV].m_buffers[bindId].GetStride();
	m_subBuffers[EBT_ColorSTsSRV].m_offset[bindId] = numPulledElements * m_subBuffers[EBT_ColorSTsSRV].m_buffers[bindId].GetStride();

	for (auto& subBuffer : m_subBuffers)
	{
		subBuffer.m_buffers[bindId].Unlock(subBuffer.m_offset[bindId]);
		subBuffer.m_pMemoryBase[bindId] = nullptr;
		subBuffer.m_offset[bindId] = 0;
	}
}

void CParticleBufferSet::SetFence()
{
#if BUFFER_ENABLE_DIRECT_ACCESS == 1
	assert(m_valid);
	assert(gRenDev->m_pRT->IsRenderThread());

	const uint processId = gRenDev->GetRenderThreadID();
	const uint bindId = m_ids[processId];

	GetDeviceObjectFactory().IssueFence(m_fences[bindId]);
#endif
}

void CParticleBufferSet::WaitForFence()
{
#if BUFFER_ENABLE_DIRECT_ACCESS == 1
	assert(m_valid);

	const uint fillId = gRenDev->GetMainThreadID();
	const uint cvId = m_ids[fillId];

	GetDeviceObjectFactory().SyncFence(m_fences[cvId], true, false);
#endif
}

uint CParticleBufferSet::GetAllocId() const
{
	const uint fillId = gRenDev->GetMainThreadID();
	return gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet().m_ids[fillId];
}

uint CParticleBufferSet::GetBindId() const
{
	const uint processId = gRenDev->GetRenderThreadID();
	const uint bindId = m_ids[processId];
	return bindId;
}

void CParticleBufferSet::Alloc(uint index, EBufferTypes type, uint numElems, SAlloc* pAllocOut)
{
	assert(pAllocOut != nullptr);
	assert(uint(type) < EBT_Total);

	SubBuffer& subBuffer = m_subBuffers[type];
	const uint stride = subBuffer.m_buffers[index].GetStride();
	LONG offset;
	do
	{
		offset = *(volatile LONG*)&subBuffer.m_offset[index];
		if (numElems * stride > (subBuffer.m_availableMemory - offset))
			numElems = (subBuffer.m_availableMemory - offset) / stride;
	} while (CryInterlockedCompareExchange((volatile LONG*)&subBuffer.m_offset[index], offset + numElems * stride, offset) != offset);

	pAllocOut->m_pBase = subBuffer.m_pMemoryBase[index];
	pAllocOut->m_firstElem = offset / stride;
	pAllocOut->m_numElemns = numElems;
}

void CParticleBufferSet::Alloc(uint index, uint numElems, SAllocStreams* pAllocOut)
{
	assert(pAllocOut != nullptr);

	SubBuffer& subBuffer = m_subBuffers[EBT_PositionsSRV];
	const uint stride = subBuffer.m_buffers[index].GetStride();
	LONG offset;
	do
	{
		offset = *(volatile LONG*)&subBuffer.m_offset[index];
		if (numElems * stride > (subBuffer.m_availableMemory - offset))
			numElems = (subBuffer.m_availableMemory - offset) / stride;
	}
	while (CryInterlockedCompareExchange((volatile LONG*)&subBuffer.m_offset[index], offset + numElems * stride, offset) != offset);
	const uint firstElement = offset / stride;

	pAllocOut->m_pPositions = alias_cast<Vec3*>(m_subBuffers[EBT_PositionsSRV].m_pMemoryBase[index]) + firstElement;
	pAllocOut->m_pAxes = alias_cast<SParticleAxes*>(m_subBuffers[EBT_AxesSRV].m_pMemoryBase[index]) + firstElement;
	pAllocOut->m_pColorSTs = alias_cast<SParticleColorST*>(m_subBuffers[EBT_ColorSTsSRV].m_pMemoryBase[index]) + firstElement;
	pAllocOut->m_firstElem = firstElement;
	pAllocOut->m_numElemns = numElems;
}

const CGpuBuffer& CParticleBufferSet::GetPositionStream() const
{
	return GetGpuBuffer(EBT_PositionsSRV);
}

const CGpuBuffer& CParticleBufferSet::GetAxesStream() const
{
	return GetGpuBuffer(EBT_AxesSRV);
}

const CGpuBuffer& CParticleBufferSet::GetColorSTsStream() const
{
	return GetGpuBuffer(EBT_ColorSTsSRV);
}

const CDeviceInputStream* CParticleBufferSet::GetVertexStream() const
{
	return GetStreamBuffer(EBT_Vertices);
}

const CDeviceInputStream* CParticleBufferSet::GetIndexStream() const
{
	return GetStreamBuffer(EBT_Indices);
}

ILINE const CGpuBuffer& CParticleBufferSet::GetGpuBuffer(uint index) const
{
	const uint processId = gRenDev->GetRenderThreadID();
	const uint bindId = m_ids[processId];
	CRY_ASSERT_MESSAGE(
		m_subBuffers[index].m_pMemoryBase[bindId] == nullptr,
		"Cannot use buffer that wasn't unlocked.");
	return m_subBuffers[index].m_buffers[bindId].GetBuffer();
}

ILINE const CDeviceInputStream* CParticleBufferSet::GetStreamBuffer(uint index) const
{
	const uint processId = gRenDev->GetRenderThreadID();
	const uint bindId = m_ids[processId];
	CRY_ASSERT_MESSAGE(
		m_subBuffers[index].m_pMemoryBase[bindId] == nullptr,
		"Cannot use stream that wasn't unlocked.");
	return m_subBuffers[index].m_buffers[bindId].GetStream();
}

bool CParticleBufferSet::IsValid() const
{
	const uint fillId = gRenDev->GetMainThreadID();
	const uint cvId = m_ids[fillId];
	if (!m_valid)
		return false;
	for (int i = 0; i < EBT_Total; ++i)
	{
		if (m_subBuffers[i].m_pMemoryBase[cvId] == nullptr)
			return false;
	}
	return true;
}
