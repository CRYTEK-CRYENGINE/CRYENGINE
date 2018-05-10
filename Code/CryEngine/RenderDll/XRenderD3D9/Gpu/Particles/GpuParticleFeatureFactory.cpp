// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuParticleFeatureFactory.h"

#include "GpuParticleFeatureColor.h"
#include "GpuParticleFeatureCollision.h"
#include "GpuParticleFeatureField.h"
#include "GpuParticleFeatureMotion.h"
#include "GpuParticleFeatureFluidDynamics.h"

namespace gpu_pfx2
{

// this dummy is used when a feature is supported on GPU but does not
// need any special treatment (e.g. all the appearance features
struct CFeatureDummy : public CFeature
{
	static const EGpuFeatureType type = eGpuFeatureType_Dummy;
};

CGpuInterfaceFactory::CGpuInterfaceFactory()
{
	memset(m_functions, 0, sizeof(m_functions));
#define X(name) RegisterClass<CFeature ## name>();
	LIST_OF_FEATURE_TYPES
#undef X
}

}
