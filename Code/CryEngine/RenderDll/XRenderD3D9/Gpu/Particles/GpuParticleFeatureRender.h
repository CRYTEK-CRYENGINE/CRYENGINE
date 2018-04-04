// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GpuParticleFeatureBase.h"

namespace gpu_pfx2
{

class CParticleRenderGpuSprites;

struct CFeatureRenderGpu : public CFeature
{
	static const EGpuFeatureType type = eGpuFeatureType_RenderGpu;

	virtual bool IsGpuEnabling() override { return true; }
private:
};

}
