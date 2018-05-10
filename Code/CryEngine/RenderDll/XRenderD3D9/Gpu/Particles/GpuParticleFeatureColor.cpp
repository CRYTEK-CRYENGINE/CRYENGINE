// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GpuParticleFeatureColor.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

void CFeatureColor::Initialize()
{
	if (!m_colorTable.IsDeviceBufferAllocated())
		m_colorTable.CreateDeviceBuffer();
}

void CFeatureColor::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	params.color = m_colorTable[0];
}

void CFeatureColor::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*) context.pRuntime;

	m_colorTable.UploadHostData();

	pRuntime->SetUpdateBuffer(eFeatureUpdateSrvSlot_colorTable, &m_colorTable.GetBuffer());
	pRuntime->SetUpdateFlags(eFeatureUpdateFlags_Color);
}

void CFeatureColor::InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p)
{
	const SFeatureParametersColorTable parameters = p.GetParameters<SFeatureParametersColorTable>();

	for (int i = 0; i < parameters.numSamples; ++i)
		m_colorTable[i] = parameters.samples[i];
}

}
