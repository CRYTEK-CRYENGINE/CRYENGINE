// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     01/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleFeature.h"

namespace pfx2
{

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

gpu_pfx2::IParticleFeatureGpuInterface* pfx2::CParticleFeature::GetGpuInterface()
{

	if (m_gpuInterfaceRef.feature == gpu_pfx2::eGpuFeatureType_None)
		return nullptr;
	if (!m_gpuInterfaceRef.gpuInterface)
		m_gpuInterfaceRef.gpuInterface = gEnv->pRenderer->GetGpuParticleManager()->CreateParticleFeatureGpuInterface(m_gpuInterfaceRef.feature);
	return m_gpuInterfaceRef.gpuInterface.get();
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

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComment, "General", "Comment", defaultIcon, defaultColor);

}
