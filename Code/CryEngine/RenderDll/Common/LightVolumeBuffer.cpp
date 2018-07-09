// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LightVolumeBuffer.h"
#include "DriverD3D.h"

namespace
{

const uint maxNumVolumes = 256;
const uint maxNumLightInfos = 2048;

}

CLightVolumeBuffer::CLightVolumeBuffer()
	: m_lightInfosBuffer()
	, m_lightRangesBuffer()
	, m_numVolumes(0)
{
}

void CLightVolumeBuffer::Create()
{
	// NOTE: Buffers have a 256-byte alignment requirement, which is enforced when allocating the buffer
	m_lightInfosBuffer.Create(
	  maxNumLightInfos, sizeof(SLightVolumeInfo),
	  DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED,
	  nullptr);
	m_lightRangesBuffer.Create(
	  maxNumVolumes, sizeof(SLightVolumeRange),
	  DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED,
	  nullptr);
}

void CLightVolumeBuffer::Release()
{
	m_lightInfosBuffer.Release();
	m_lightRangesBuffer.Release();
}

void CLightVolumeBuffer::UpdateContent()
{
	PROFILE_FRAME(DLightsInfo_UpdateSRV);

	CD3D9Renderer* pRenderer = gcpRendD3D;

	struct SLightVolume* pLightVols;
	uint32 numVols;
	gEnv->p3DEngine->GetLightVolumes(gRenDev->GetRenderThreadID(), pLightVols, numVols);

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVectorCleared(SLightVolumeInfo, maxNumLightInfos, gpuStageInfos, CDeviceBufferManager::AlignBufferSizeForStreaming);
	m_lightVolumeRanges.reserve(maxNumLightInfos);
	m_lightVolumeRanges.clear();

	SLightVolumeRange currentRange;
	currentRange.begin = currentRange.end = 0;

	const uint actualNumRanges = min(numVols, maxNumVolumes);
	for (uint rangeId = 0; rangeId < actualNumRanges; ++rangeId)
	{
		const SLightVolume& cpuLightVolume = pLightVols[rangeId];
		const uint remainingSpace = maxNumLightInfos - currentRange.begin;
		const uint volumeSize = cpuLightVolume.pData.size();
		const uint actualVolumeSize = min(volumeSize, remainingSpace);
		for (uint infoId = 0; infoId < actualVolumeSize; ++infoId)
		{
			SLightVolumeInfo info;
			const SLightVolume::SLightData& cpuLightData = cpuLightVolume.pData[infoId];
			SLightVolumeInfo& gpuLightInfo = gpuStageInfos[currentRange.end++];
			gpuLightInfo.wPosition = cpuLightData.position;
			gpuLightInfo.radius = cpuLightData.radius;
			gpuLightInfo.cColor = cpuLightData.color;
			gpuLightInfo.bulbRadius = cpuLightData.buldRadius;
			gpuLightInfo.wProjectorDirection = cpuLightData.projectorDirection;
			gpuLightInfo.projectorCosAngle = cpuLightData.projectorCosAngle;
		}
		m_lightVolumeRanges.push_back(currentRange);
		currentRange.begin = currentRange.end;
	}

	// Minimize transfer size
	const size_t gpuStageInfosUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(SLightVolumeInfo) * currentRange.end);
	const size_t gpuStageRangesUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(SLightVolumeRange) * maxNumVolumes); // TODO: Update smaller range: actualNumRanges

	m_lightInfosBuffer.UpdateBufferContent(gpuStageInfos, gpuStageInfosUploadSize);
	m_lightRangesBuffer.UpdateBufferContent(m_lightVolumeRanges.data(), gpuStageRangesUploadSize);

	m_numVolumes = actualNumRanges;
}
