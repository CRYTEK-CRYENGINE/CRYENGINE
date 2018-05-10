// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuParticleFeatureCollision.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

void CFeatureCollision::Initialize()
{
	if (!m_parameters.IsDeviceBufferAllocated())
	{
		m_parameters.CreateDeviceBuffer();
	}
}

void CFeatureCollision::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*) context.pRuntime;

	m_parameters.CopyToDevice();

	CTexture* zTarget = CRendererResources::s_ptexLinearDepth;
	pRuntime->SetUpdateTexture(eFeatureUpdateSrvSlot_depthBuffer, zTarget);
	pRuntime->SetUpdateConstantBuffer(eConstantBufferSlot_Collisions, m_parameters.GetDeviceConstantBuffer());
	pRuntime->SetUpdateFlags(EFeatureUpdateFlags_Collision_ScreenSpace);
}

void CFeatureCollision::InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p)
{
	m_parameters = p.GetParameters<SFeatureParametersCollision>();
}

}
