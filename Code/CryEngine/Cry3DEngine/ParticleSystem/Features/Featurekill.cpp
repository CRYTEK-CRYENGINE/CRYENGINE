// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include "FeatureCommon.h"

CRY_PFX2_DBG

volatile bool gFeatureKill = false;

namespace pfx2
{

class CFeatureKillOnParentDeath : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_Update, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		AddNoPropertiesLabel(ar);
	}

	virtual void Update(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		KillOnParentDeath(context);
	}

private:
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureKillOnParentDeath, "Kill", "OnParentDeath", defaultIcon, defaultColor);

}
