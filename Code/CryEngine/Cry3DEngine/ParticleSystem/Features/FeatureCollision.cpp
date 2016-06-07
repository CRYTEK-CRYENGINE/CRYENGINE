// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCollision.h"

CRY_PFX2_DBG

volatile bool gFeatureCollision = false;

namespace pfx2
{

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureCollision, "Motion", "Collision", defaultIcon, defaultColor);

void CFeatureCollision::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->AddToUpdateList(EUL_Update, this);
}

void CFeatureCollision::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	AddNoPropertiesLabel(ar);
}

}
