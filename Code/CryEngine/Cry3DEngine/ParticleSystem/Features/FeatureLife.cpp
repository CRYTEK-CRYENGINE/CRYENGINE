// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"

namespace pfx2
{

class CFeatureLifeTime : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLifeTime(float lifetime = 1, bool killOnParentDeath = false)
		: m_lifeTime(lifetime), m_killOnParentDeath(killOnParentDeath) {}

	static uint DefaultForType() { return EFT_Life; }

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

	virtual CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		float maxLifetime = m_lifeTime.GetValueRange().end;
		if (m_killOnParentDeath)
		{
			if (CParticleComponent* pParent = pComponent->GetParentComponent())
				SetMin(maxLifetime, pParent->ComponentParams().m_maxParticleLife);
		}
		pComponent->ComponentParams().m_maxParticleLife = maxLifetime;
		return this;
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		m_lifeTime.AddToComponent(pComponent, this, EPDT_LifeTime);
		pComponent->PreInitParticles.add(this);
		pParams->m_maxParticleLife = m_lifeTime.GetValueRange().end;

		if (m_killOnParentDeath)
			pComponent->PostUpdateParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
	}

	virtual void PreInitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		IOFStream lifeTimes = container.GetIOFStream(EPDT_LifeTime);
		IOFStream invLifeTimes = container.GetIOFStream(EPDT_InvLifeTime);

		if (m_lifeTime.IsEnabled() && m_lifeTime.GetBaseValue())
		{
			m_lifeTime.InitParticles(runtime, EPDT_LifeTime);
			if (m_lifeTime.HasModifiers())
			{
				for (auto particleGroupId : runtime.SpawnedRangeV())
				{
					const floatv lifetime = lifeTimes.Load(particleGroupId);
					const floatv invLifeTime = rcp(max(lifetime, ToFloatv(FLT_EPSILON)));
					invLifeTimes.Store(particleGroupId, invLifeTime);
				}
			}
			else
			{
				invLifeTimes.Fill(runtime.SpawnedRange(), rcp(m_lifeTime.GetBaseValue()));
			}
		}
		else
		{
			lifeTimes.Fill(runtime.SpawnedRange(), 0.0f);
			invLifeTimes.Fill(runtime.SpawnedRange(), 0.0f);
		}
	}

	virtual void PostUpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		// Kill on parent death
		CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const auto parentAges = parentContainer.GetIFStream(EPDT_NormalAge);

		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IOFStream ages = container.GetIOFStream(EPDT_NormalAge);

		for (auto particleId : runtime.FullRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			if (parentId == gInvalidId || IsExpired(parentAges.Load(parentId)))
				ages.Store(particleId, 1.0f);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.lifeTime = m_lifeTime.GetValueRange(runtime)(0.5f);
	}

protected:
	CParamMod<EDD_PerParticle, UInfFloat> m_lifeTime          = 1;
	bool                                  m_killOnParentDeath = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLifeTime, "Life", "Time", colorLife);

//////////////////////////////////////////////////////////////////////////

// Legacy LifeImmortal and KillOnParentDeath features, for versions < 11

class CFeatureLifeImmortal : public CFeatureLifeTime
{
public:
	virtual CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		return (new CFeatureLifeTime(gInfinity, true))->ResolveDependency(pComponent);
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureLifeImmortal, "Life", "Immortal")

class CFeatureKillOnParentDeath : public CFeatureLifeTime
{
public:
	virtual CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
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
				return nullptr;
			}
		}
		return nullptr;
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureKillOnParentDeath, "Kill", "OnParentDeath");

}

