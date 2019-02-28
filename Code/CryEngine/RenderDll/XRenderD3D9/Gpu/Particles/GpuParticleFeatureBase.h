// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

class CParticleComponentRuntime;

// context passed to functions during update phase
struct SUpdateContext
{
	CParticleComponentRuntime* pRuntime;
	CRenderView*               pRenderView;
	CGpuBuffer*                pReadbackBuffer;
	CGpuBuffer*                pCounterBuffer;
	CGpuBuffer*                pScratchBuffer;
	float                      deltaTime;
};

// this is the renderer-internal interface for GPU features
struct CFeature : public IParticleFeature
{
public:
	// called from render thread to initialize constant buffers and SRVs
	virtual void Initialize() {}
	virtual void InitParticles(const SUpdateContext& context) {}
	virtual void Update(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList) {}
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
		m_parameters = p.template GetParameters<ParameterStruct>();
	};
protected:
	const ParameterStruct& GetParameters() const { return m_parameters; }
private:
	ParameterStruct        m_parameters;
};

}
