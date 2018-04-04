// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     13/11/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryRenderer/IGpuParticles.h>

namespace gpu_pfx2
{

// this is the renderer-internal interface for GPU features
struct CFeature : public IParticleFeatureGpuInterface
{
public:
	// called from render thread to initialize constant buffers and SRVs
	virtual void Initialize()                                                                               {}
	virtual void SpawnParticles(const gpu_pfx2::SUpdateContext& context)                                    {}
	virtual void InitParticles(const gpu_pfx2::SUpdateContext& context)                                     {}
	virtual void InitSubInstance(IParticleComponentRuntime* pRuntime, SSpawnData* pInstances, size_t count) {}
	virtual void KillParticles(const gpu_pfx2::SUpdateContext& context,
	                           uint32* pParticles, size_t count) {}
	virtual void Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) {}
};

// handy base class for features
template<class ParameterStruct>
struct CFeatureWithParameterStruct : public CFeature
{
	typedef ParameterStruct TFeatureParameterType;
	virtual void InternalSetParameters(
	  const EParameterType type,
	  const SFeatureParametersBase& p) override
	{
		m_parameters = p.GetParameters<ParameterStruct>();
	}
protected:
	const ParameterStruct& GetParameters() const { return m_parameters; }
private:
	ParameterStruct        m_parameters;
};

}
