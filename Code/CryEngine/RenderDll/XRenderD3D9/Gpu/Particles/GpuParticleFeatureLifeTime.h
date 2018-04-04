// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     12/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryRenderer/IGpuParticles.h>
#include "GpuParticleFeatureBase.h"

namespace gpu_pfx2
{

struct CFeatureLifeTime : public CFeatureWithParameterStruct<SFeatureParametersLifeTime>
{
	static const EGpuFeatureType type = eGpuFeatureType_LifeTime;

	virtual void InitParticles(const SUpdateContext& context) override;
};

}
