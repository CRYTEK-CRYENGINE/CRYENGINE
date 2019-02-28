// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IGpuParticles.h>
#include "Gpu/GpuComputeBackend.h"
#include <memory.h>

namespace gpu_pfx2
{

// this structure is mirrored on GPU
struct SDefaultParticleData
{
	Vec3   Position;
	Vec3   Velocity;
	uint32 Color;
	uint32 AuxData;
};

class CParticleContainer
{
public:
	CParticleContainer(int maxParticles) : m_defaultData(maxParticles), m_counter(4) {}
	void        Initialize(bool isDoubleBuffered);
	size_t      GetMaxParticles() const  { return m_defaultData.Get().GetSize(); }
	bool        HasDefaultParticleData() { return m_defaultData.Get().IsDeviceBufferAllocated(); }

	void        Clear()                  { m_defaultData.Reset(); }
	void        Swap()                   { m_defaultData.Swap(); }

	void ReadbackCounter(uint32 readLength);
	int RetrieveCounter(uint32 readLength);

	CGpuBuffer& GetDefaultParticleDataBuffer()     { return m_defaultData.Get().GetBuffer(); };
	CGpuBuffer& GetDefaultParticleDataBackBuffer() { return m_defaultData.GetBackBuffer().GetBuffer(); }

private:
	// this will only be double buffered when needed (i.e. when the particles get sorted)
	gpu::CDoubleBuffered<gpu::CStructuredResource<SDefaultParticleData, gpu::BufferFlagsReadWrite>> m_defaultData;
	gpu::CStructuredResource<uint, gpu::BufferFlagsReadWriteReadback>                               m_counter;
};
}
