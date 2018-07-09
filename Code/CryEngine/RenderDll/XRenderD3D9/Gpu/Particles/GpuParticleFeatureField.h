// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GpuParticleFeatureBase.h"
#include "GpuParticleComponentRuntime.h"
#include "../GpuComputeBackend.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

struct CFeatureFieldOpacity : public CFeature
{
	static const EGpuFeatureType type = eGpuFeatureType_FieldOpacity;

	CFeatureFieldOpacity() : m_opacityTable(kNumModifierSamples) {}
	virtual void Initialize() override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) override;
	virtual void InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p) override;

private:
	gpu::CStructuredResource<float, gpu::BufferFlagsDynamic> m_opacityTable;
};

struct CFeatureFieldSize : public CFeature
{
	static const EGpuFeatureType type = eGpuFeatureType_FieldSize;

	CFeatureFieldSize() : m_sizeTable(kNumModifierSamples) {}
	virtual void Initialize() override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) override;
	virtual void InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p) override;

private:
	gpu::CStructuredResource<float, gpu::BufferFlagsDynamic> m_sizeTable;
};

struct SFeatureInternalParametersPixelSize
{
	Vec4  projectionPlane;
	float minPixelSize;
	float invMinPixelSize;
	float maxPixelSize;
	float minDrawPixels;
	int32 affectOpacity;
};

struct CFeatureFieldPixelSize : public CFeatureWithParameterStruct<SFeatureParametersPixelSize>
{
	static const EGpuFeatureType type = eGpuFeatureType_FieldPixelSize;

	CFeatureFieldPixelSize()  {}
	virtual void Initialize() override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) override;
private:
	gpu::CTypedConstantBuffer<SFeatureInternalParametersPixelSize> m_internalParameters;
};
}
