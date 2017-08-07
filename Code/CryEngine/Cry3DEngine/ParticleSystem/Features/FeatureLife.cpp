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

CRY_PFX2_DBG

namespace pfx2
{

class CFeatureLifeTime : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLifeTime()
		: CParticleFeature(gpu_pfx2::eGpuFeatureType_LifeTime)
	{}

	virtual EFeatureType GetFeatureType() override
	{
		return EFT_Life;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_lifeTime);
		if (m_lifeTime.IsEnabled())
			SERIALIZE_VAR(ar, m_killOnParentDeath);
		else if (ar.isInput())
			m_killOnParentDeath = true;
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		m_lifeTime.AddToComponent(pComponent, this, EPDT_LifeTime);
		pParams->m_maxParticleLifeTime = m_lifeTime.GetValueRange().end;

		if (m_killOnParentDeath)
			pComponent->AddToUpdateList(EUL_PostUpdate, this);

		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLifeTime params;
			params.lifeTime = m_lifeTime.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		IOFStream lifeTimes = container.GetIOFStream(EPDT_LifeTime);
		IOFStream invLifeTimes = container.GetIOFStream(EPDT_InvLifeTime);

		if (m_lifeTime.IsEnabled())
		{
			m_lifeTime.InitParticles(context, EPDT_LifeTime);
			if (m_lifeTime.HasModifiers())
			{
				const floatv minimum = ToFloatv(std::numeric_limits<float>::min());
				for (auto particleGroupId : context.GetSpawnedGroupRange())
				{
					const floatv lifetime = lifeTimes.Load(particleGroupId);
					const floatv maxedLifeTime = max(lifetime, minimum);
					lifeTimes.Store(particleGroupId, maxedLifeTime);
					const floatv invLifeTime = rcp(lifetime);
					invLifeTimes.Store(particleGroupId, invLifeTime);
				}
			}
			else
			{
				invLifeTimes.Fill(context.GetSpawnedRange(), rcp(m_lifeTime.GetBaseValue()));
			}
		}
		else
		{
			lifeTimes.Fill(context.GetSpawnedRange(), 0.0f);
			invLifeTimes.Fill(context.GetSpawnedRange(), 0.0f);
		}
	}

	virtual void PostUpdate(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		// Kill on parent death
		CParticleContainer& container = context.m_container;
		const CParticleContainer& parentContainer = context.m_parentContainer;
		const auto parentStates = parentContainer.GetTIStream<uint8>(EPDT_State);
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IOFStream ages = container.GetIOFStream(EPDT_NormalAge);

		for (auto particleId : context.GetUpdateRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const uint8 parentState = (parentId != gInvalidId) ? parentStates.Load(parentId) : ES_Expired;
			if (parentState & ESB_Dead)
				ages.Store(particleId, 1.0f);
		}
	}

protected:
	CParamMod<SModParticleSpawnInit, UInfFloat> m_lifeTime          = 1;
	bool                                        m_killOnParentDeath = false;
};

CRY_PFX2_IMPLEMENT_FEATURE_DEFAULT(CParticleFeature, CFeatureLifeTime, "Life", "Time", colorLife, EFT_Life);

//////////////////////////////////////////////////////////////////////////

// Legacy LifeImmortal and KillOnParentDeath features, for versions < 11

class CFeatureLifeImmortal : public CFeatureLifeTime
{
public:
	CFeatureLifeImmortal()
	{
		m_lifeTime = gInfinity;
	}
};

CRY_PFX2_LEGACY_FEATURE(CParticleFeature, CFeatureLifeImmortal, "LifeImmortal")

class CFeatureKillOnParentDeath : public CFeatureLifeTime
{
public:
	CFeatureKillOnParentDeath()
	{
		m_killOnParentDeath = true;
	}

	virtual bool VersionValidate(CParticleComponent* pComponent) override
	{
		// If another LifeTime feature exists, use it, and set the Kill param.
		// Otherwise, use this feature, with default LifeTime param.
		uint num = pComponent->GetNumFeatures();
		for (uint i = 0; i < num; ++i)
		{
			CFeatureKillOnParentDeath* feature = static_cast<CFeatureKillOnParentDeath*>(pComponent->GetFeature(i));
			if (feature && feature != this && feature->GetFeatureType() == EFT_Life)
			{
				feature->m_killOnParentDeath = true;
				return false;
			}
		}
		return true;
	}
};

CRY_PFX2_LEGACY_FEATURE(CParticleFeature, CFeatureKillOnParentDeath, "KillOnParentDeath");

}

