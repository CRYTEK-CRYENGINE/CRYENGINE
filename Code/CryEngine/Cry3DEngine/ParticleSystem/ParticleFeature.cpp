// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     01/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleFeature.h"
#include "ParticleSystem.h"

namespace pfx2
{

bool CParticleFeature::RegisterFeature(const SParticleFeatureParams& params)
{
	return CParticleSystem::RegisterFeature(params);
}

void CParticleFeature::Serialize(Serialization::IArchive& ar)
{
	ar(m_enabled);
}

void CParticleFeature::AddNoPropertiesLabel(Serialization::IArchive& ar)
{
	if (ar.isEdit() && ar.isOutput())
	{
		const string label;		
		ar(label, "", "!No Properties");
	}
}

gpu_pfx2::IParticleFeature* CParticleFeature::MakeGpuInterface(CParticleComponent* pComponent, gpu_pfx2::EGpuFeatureType feature)
{
	if (!feature || !pComponent->UsesGPU() || !gEnv->pRenderer)
		m_gpuInterface.reset();
	else if (!m_gpuInterface)
		m_gpuInterface = gEnv->pRenderer->GetGpuParticleManager()->CreateParticleFeature(feature);
	pComponent->AddGPUFeature(m_gpuInterface);
	return m_gpuInterface;
}

class CFeatureComment : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_text, "Text", "Text");
	}

private:
	string m_text;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComment, "General", "Comment", colorGeneral);

}
