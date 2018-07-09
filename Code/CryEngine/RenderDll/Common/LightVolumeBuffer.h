// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Include_HLSL_CPP_Shared.h"
#include "DevBuffer.h" // CGpuBuffer

struct SLightVolume;

class CLightVolumeBuffer
{
public:
	CLightVolumeBuffer();

	void Create();
	void Release();
	bool HasVolumes() const    { return m_numVolumes != 0; }
	uint GetNumVolumes() const { return m_numVolumes; }

	void UpdateContent();

	CGpuBuffer& GetLightInfosBuffer() { return m_lightInfosBuffer; }
	CGpuBuffer& GetLightRangesBuffer() { return m_lightRangesBuffer; }
	const CGpuBuffer& GetLightInfosBuffer() const { return m_lightInfosBuffer; }
	const CGpuBuffer& GetLightRangesBuffer() const { return m_lightRangesBuffer; }
	bool              HasLights(uint lightVolumeIdx) const { return m_lightVolumeRanges[lightVolumeIdx].begin != m_lightVolumeRanges[lightVolumeIdx].end; }

private:
	CGpuBuffer                     m_lightInfosBuffer;
	CGpuBuffer                     m_lightRangesBuffer;
	std::vector<SLightVolumeRange> m_lightVolumeRanges;
	uint                           m_numVolumes;
};
