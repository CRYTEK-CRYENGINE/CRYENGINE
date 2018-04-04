// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "GpuParticleFeatureLifeTime.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

void CFeatureLifeTime::InitParticles(const SUpdateContext& context)
{
	context.pRuntime->SetLifeTime(GetParameters().lifeTime);
}

}
