// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuParticleContainer.h"

namespace gpu_pfx2
{
void CParticleContainer::Initialize(bool isDoubleBuffered)
{
	m_defaultData.Initialize(isDoubleBuffered);
	m_counter.CreateDeviceBuffer();
}

void CParticleContainer::ReadbackCounter(uint32 readLength)
{
	m_counter.Readback(readLength);
}

int CParticleContainer::RetrieveCounter(uint32 readLength)
{
	int result = 0;
	if (const uint* counter = m_counter.Map(readLength))
	{
		result = counter[0];
		m_counter.Unmap();
	}
	return result;
}

}
