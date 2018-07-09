// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     21/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GpuParticleFeatureBase.h"

namespace gpu_pfx2
{

struct CFeatureMotionFluidDynamics : public CFeatureWithParameterStruct<SFeatureParametersMotionFluidDynamics>
{
	static const EGpuFeatureType type = eGpuFeatureType_MotionFluidDynamics;

	virtual void Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) override;
	virtual void Initialize() override;
private:
};
}
