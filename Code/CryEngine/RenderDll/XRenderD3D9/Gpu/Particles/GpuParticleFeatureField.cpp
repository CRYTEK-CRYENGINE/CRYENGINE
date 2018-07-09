// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GpuParticleFeatureField.h"

namespace gpu_pfx2
{

void CFeatureFieldOpacity::Initialize()
{
	if (!m_opacityTable.IsDeviceBufferAllocated())
	{
		m_opacityTable.CreateDeviceBuffer();
	}
}

void CFeatureFieldOpacity::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	params.opacity = m_opacityTable[0];
}

void CFeatureFieldOpacity::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CParticleComponentRuntime* runtime = (CParticleComponentRuntime*) context.pRuntime;

	m_opacityTable.UploadHostData();

	runtime->SetUpdateBuffer(eFeatureUpdateSrvSlot_opacityTable, &m_opacityTable.GetBuffer());
	runtime->SetUpdateFlags(eFeatureUpdateFlags_Opacity);
}

void CFeatureFieldOpacity::InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p)
{
	const SFeatureParametersOpacity parameters = p.GetParameters<SFeatureParametersOpacity>();

	for (int i = 0; i < parameters.numSamples; ++i)
	{
		m_opacityTable[i] = parameters.samples[i];
	}
}

void CFeatureFieldSize::Initialize()
{
	if (!m_sizeTable.IsDeviceBufferAllocated())
		m_sizeTable.CreateDeviceBuffer();
}

void CFeatureFieldSize::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	params.size = m_sizeTable[0];
}

void CFeatureFieldSize::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*) context.pRuntime;

	m_sizeTable.UploadHostData();

	pRuntime->SetUpdateBuffer(eFeatureUpdateSrvSlot_sizeTable, &m_sizeTable.GetBuffer());
	pRuntime->SetUpdateFlags(eFeatureUpdateFlags_Size);
}

void CFeatureFieldSize::InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p)
{
	const SFeatureParametersSizeTable parameters = p.GetParameters<SFeatureParametersSizeTable>();

	for (int i = 0; i < parameters.numSamples; ++i)
		m_sizeTable[i] = parameters.samples[i];
}

void CFeatureFieldPixelSize::Initialize()
{
	if (!m_internalParameters.IsDeviceBufferAllocated())
	{
		m_internalParameters.CreateDeviceBuffer();
	}
}

void CFeatureFieldPixelSize::InitParticles(const SUpdateContext& context)
{

}

void CFeatureFieldPixelSize::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
	m_internalParameters->projectionPlane = Vec4(camera.GetViewdir(), -camera.GetPosition().dot(camera.GetViewdir()));

	const float height = float(CRendererResources::s_renderHeight);
	const float fov = camera.GetFov();
	const float minPixelSizeF = GetParameters().minSize > 0.0f ? GetParameters().minSize : 0.0f;

	m_internalParameters->minPixelSize = minPixelSizeF;
	m_internalParameters->invMinPixelSize = minPixelSizeF != 0.0f ? 1.0f / minPixelSizeF : 1.0f;
	m_internalParameters->maxPixelSize = GetParameters().maxSize > 0.0f ? GetParameters().maxSize : std::numeric_limits<float>::max();
	m_internalParameters->minDrawPixels = fov / height * 0.5f;
	m_internalParameters->affectOpacity = GetParameters().affectOpacity;
	m_internalParameters.CopyToDevice();

	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*)context.pRuntime;

	pRuntime->SetUpdateConstantBuffer(eConstantBufferSlot_PixelSize, m_internalParameters.GetDeviceConstantBuffer());
	pRuntime->SetUpdateFlags(EFeatureUpdateFlags_PixelSize);
}

}
