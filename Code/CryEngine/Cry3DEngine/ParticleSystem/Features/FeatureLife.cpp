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

	virtual EFeatureType GetFeatureType() override { return EFT_Life; }
	static uint DefaultForType() { return EFT_Life; }

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
		pComponent->PreInitParticles.add(this);

		float maxLifetime = m_lifeTime.GetValueRange().end;
		if (m_killOnParentDeath)
		{
			pComponent->PostUpdateParticles.add(this);
			if (CParticleComponent* pParent = pComponent->GetParentComponent())
				SetMin(maxLifetime, pParent->ComponentParams().m_maxParticleLife);
		}

		pComponent->ComponentParams().m_maxParticleLife = maxLifetime;
		pComponent->UpdateGPUParams.add(this);
	}

	virtual void PreInitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		// Init Lifetime and InvLifetime
		CParticleContainer& container = runtime.GetContainer();
		IOFStream lifeTimes = container.GetIOFStream(EPDT_LifeTime);
		IOFStream invLifeTimes = container.GetIOFStream(EPDT_InvLifeTime);
		IOFStream normAges = container.GetIOFStream(EPDT_NormalAge);

		if (m_lifeTime.GetBaseValue() == 0.0f)
		{
			lifeTimes.Fill(runtime.SpawnedRange(), 0.0f);
			invLifeTimes.Fill(runtime.SpawnedRange(), 0.0f);
			normAges.Fill(runtime.SpawnedRange(), 0.0f);
		}
		else
		{
			if (!m_lifeTime.IsEnabled())
			{
				// Infinite lifetime. Store huge lifetime instead, so absolute age can be computed when needed
				static const float maxLifeTime = float(1 << 30);
				lifeTimes.Fill(runtime.SpawnedRange(), maxLifeTime);
				invLifeTimes.Fill(runtime.SpawnedRange(), 1.0f / maxLifeTime);
			}
			else
			{
				m_lifeTime.Init(runtime, EPDT_LifeTime);
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
					invLifeTimes.Fill(runtime.SpawnedRange(), 1.0f / m_lifeTime.GetBaseValue());
				}
			}

			// Convert ages from absolute to normalized
			for (auto particleGroupId : runtime.SpawnedRangeV())
			{
				normAges[particleGroupId] *= invLifeTimes[particleGroupId];
			}
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
	CParamMod<EDD_Particle, UInfFloat> m_lifeTime          = 1;
	bool                               m_killOnParentDeath = false;
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
		if (auto pFeature = pComponent->FindDuplicateFeature(this))
		{
			pFeature->m_killOnParentDeath = true;
		}
		return nullptr;
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureKillOnParentDeath, "Kill", "OnParentDeath");

}

