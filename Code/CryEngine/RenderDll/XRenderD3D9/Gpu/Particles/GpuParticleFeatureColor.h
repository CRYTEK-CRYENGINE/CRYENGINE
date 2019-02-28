// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GpuParticleFeatureBase.h"
#include "../GpuComputeBackend.h"

namespace gpu_pfx2
{

struct CFeatureColor : public CFeature
{
	static const EGpuFeatureType type = eGpuFeatureType_Color;

	CFeatureColor() : m_colorTable(kNumModifierSamples) {}
	virtual void Initialize() override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) override;
	virtual void InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p) override;
private:
	gpu::CStructuredResource<Vec3, gpu::BufferFlagsDynamic> m_colorTable;
};

}
