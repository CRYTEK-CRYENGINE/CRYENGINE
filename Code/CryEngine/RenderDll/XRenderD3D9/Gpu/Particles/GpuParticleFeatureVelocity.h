// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryRenderer/IGpuParticles.h>
#include "GpuParticleFeatureBase.h"

namespace gpu_pfx2
{

struct CFeatureVelocityCone : public CFeatureWithParameterStruct<SFeatureParametersVelocityCone>
{
	static const EGpuFeatureType type = eGpuFeatureType_VelocityCone;

	virtual void InitParticles(const SUpdateContext& context);
};

struct CFeatureVelocityDirectional : public CFeatureWithParameterStruct<SFeatureParametersVelocityDirectional>
{
	static const EGpuFeatureType type = eGpuFeatureType_VelocityDirectional;

	virtual void InitParticles(const SUpdateContext& context);
};

struct CFeatureVelocityOmniDirectional : public CFeatureWithParameterStruct<SFeatureParametersVelocityOmniDirectional>
{
	static const EGpuFeatureType type = eGpuFeatureType_VelocityOmniDirectional;

	virtual void InitParticles(const SUpdateContext& context);
};

struct CFeatureVelocityInherit : public CFeatureWithParameterStruct<SFeatureParametersScale>
{
	static const EGpuFeatureType type = eGpuFeatureType_VelocityInherit;

	virtual void InitParticles(const SUpdateContext& context);
};

}
