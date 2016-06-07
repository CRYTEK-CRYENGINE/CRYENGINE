// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     29/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include "ParamMod.h"
#include "FeatureCommon.h"

CRY_PFX2_DBG

volatile bool gFeatureLife = false;

namespace pfx2
{

class CFeatureLifeTime : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLifeTime()
		: m_lifeTime(1.0f)
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_LifeTime)
	{}

	virtual EFeatureType GetFeatureType() override
	{
		return EFT_Life;
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pParams->m_baseParticleLifeTime = m_lifeTime.GetBaseValue();
		m_lifeTime.AddToComponent(pComponent, this, EPDT_LifeTime);

		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLifeTime params;
			params.lifeTime = m_lifeTime.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_lifeTime, "LifeTime", "Life time");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		m_lifeTime.InitParticles(context, EPDT_LifeTime, EPDT_LifeTime);
	}

private:
	CParamMod<SModParticleSpawnInit, UFloat10> m_lifeTime;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLifeTime, "Life", "Time", defaultIcon, defaultColor);

class CFeatureLifeImmortal : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual EFeatureType GetFeatureType() override
	{
		return EFT_Life;
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pParams->m_baseParticleLifeTime = 0.0f;
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddToUpdateList(EUL_Update, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		AddNoPropertiesLabel(ar);
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		IOFStream lifeTimes = container.GetIOFStream(EPDT_LifeTime);

		CRY_PFX2_FOR_SPAWNED_PARTICLEGROUP(context)
		lifeTimes.Store(particleGroupId, ToFloatv(0.0f));
		CRY_PFX2_FOR_END;
	}

	virtual void Update(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		KillOnParentDeath(context);
	}

private:
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLifeImmortal, "Life", "Immortal", defaultIcon, defaultColor);

}
