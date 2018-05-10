// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GpuParticleFeatureMotion.h"

namespace gpu_pfx2
{

void CFeatureMotion::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	if (m_integrator == EI_Linear)
		pRuntime->SetUpdateFlags(eFeatureUpdateFlags_Motion_LinearIntegral);
	else
		pRuntime->SetUpdateFlags(eFeatureUpdateFlags_Motion_DragFast);

	for (const auto& pEff : m_pEffectors)
		if (pEff)
			pEff->Initialize();

	m_parameters.CopyToDevice();
	pRuntime->SetUpdateConstantBuffer(eConstantBufferSlot_Motion, m_parameters.GetDeviceConstantBuffer());

	for (const auto& pEff : m_pEffectors)
		if (pEff)
			pEff->Update(context);

	const bool hasDrag = m_parameters->drag != 0.0f;
	const bool hasGravity = m_parameters->gravity != 0.0f || !m_parameters->uniformAcceleration.IsZero();
	bool hasEffectors = false;
	for (const auto& pEff : m_pEffectors)
		if (pEff)
			hasEffectors = true;

	if (hasDrag || hasGravity || hasEffectors)
		m_integrator = EI_DragFast;
	else
		m_integrator = EI_Linear;
}

void CFeatureMotion::Initialize()
{
	if (!m_parameters.IsDeviceBufferAllocated())
	{
		m_parameters.CreateDeviceBuffer();
	}
}

#define OPERATION_FOR_LOCAL_EFFECTOR_TYPES \
  OP(TBrownianEffector, 0)                 \
  OP(TSimplexEffector, 1)                  \
  OP(TCurlEffector, 2)                     \
  OP(TGravityEffector, 3)                  \
  OP(TVortexEffector, 4)

void CFeatureMotion::InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p)
{
	if (type == EParameterType::MotionPhysics)
	{
		const auto& params = p.GetParameters<SFeatureParametersMotionPhysics>();
		m_parameters = params;

		// reset the effectors, they need to be created with additional calls to this function
		for (auto& pEff : m_pEffectors)
			pEff.reset();
	}

#define OP(Effector, i)                                                        \
  if (type == Effector::parameterType)                                         \
  {                                                                            \
    const auto& params = p.GetParameters<Effector::TConstantBufferStruct>();   \
    m_pEffectors[(i)] = std::unique_ptr<CLocalEffector>(new Effector(params)); \
  }
	OPERATION_FOR_LOCAL_EFFECTOR_TYPES
#undef OP
}

}
