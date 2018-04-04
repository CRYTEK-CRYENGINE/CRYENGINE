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

class CParticleComponentRuntime;

class CFeatureSpawnBase : public CFeatureWithParameterStruct<SFeatureParametersSpawn>
{
public:
	virtual void InitSubInstance(IParticleComponentRuntime* pRuntime, SSpawnData* pInstances, size_t count) override;

protected:
	bool CanSpawnParticles(CParticleComponentRuntime* pRuntime) const;
};

class CFeatureSpawnCount : public CFeatureSpawnBase
{
public:
	static const EGpuFeatureType type = eGpuFeatureType_SpawnCount;
	virtual void SpawnParticles(const gpu_pfx2::SUpdateContext& context) override;
};

class CFeatureSpawnRate : public CFeatureSpawnBase
{
public:
	static const EGpuFeatureType type = eGpuFeatureType_SpawnRate;
	virtual void InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p) override;
	virtual void SpawnParticles(const gpu_pfx2::SUpdateContext& context) override;

private:
	ESpawnRateMode m_mode;
};

}
