// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     22/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GpuParticleFeatureBase.h"

namespace gpu_pfx2
{
struct CFeatureLocationOffset : public CFeatureWithParameterStruct<SFeatureParametersLocationOffset>
{
	static const EGpuFeatureType type = eGpuFeatureType_LocationOffset;

	virtual void InitParticles(const SUpdateContext& context) override;
};

struct CFeatureLocationBox : public CFeatureWithParameterStruct<SFeatureParametersLocationBox>
{
	static const EGpuFeatureType type = eGpuFeatureType_LocationBox;

	virtual void InitParticles(const SUpdateContext& context) override;
};

struct CFeatureLocationSphere : public CFeatureWithParameterStruct<SFeatureParametersLocationSphere>
{
	static const EGpuFeatureType type = eGpuFeatureType_LocationSphere;

	virtual void InitParticles(const SUpdateContext& context) override;
};

struct CFeatureLocationCircle : public CFeatureWithParameterStruct<SFeatureParametersLocationCircle>
{
	static const EGpuFeatureType type = eGpuFeatureType_LocationCircle;

	virtual void InitParticles(const SUpdateContext& context) override;
};

struct CFeatureLocationNoise : public CFeatureWithParameterStruct<SFeatureParametersLocationNoise>
{
	static const EGpuFeatureType type = eGpuFeatureType_LocationNoise;

	virtual void InitParticles(const SUpdateContext& context) override;
};

}
