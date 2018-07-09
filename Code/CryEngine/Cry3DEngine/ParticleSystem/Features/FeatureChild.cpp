// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "FeatureCollision.h"

namespace pfx2
{


//////////////////////////////////////////////////////////////////////////
class CFeatureChildBase : public CParticleFeature
{
public:
	virtual EFeatureType GetFeatureType() override
	{
		return EFT_Child;
	}

	void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		if (!pComponent->GetParentComponent())
			return;
		pComponent->AddSubInstances.add(this);
		if (IsDelayed())
			pParams->m_maxTotalLIfe += pComponent->GetParentComponent()->GetComponentParams().m_maxParticleLife;
	}

	virtual bool IsDelayed() const { return false; }
};

//////////////////////////////////////////////////////////////////////////

class CFeatureChildOnBirth : public CFeatureChildBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	static uint DefaultForType() { return EFT_Child; }

	void AddSubInstances(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		IFStream normAges = parentContainer.GetIFStream(EPDT_NormalAge);
		IFStream lifeTimes = parentContainer.GetIFStream(EPDT_LifeTime);

		THeapArray<SInstance> triggers(runtime.MemHeap());
		triggers.reserve(parentContainer.GetNumParticles());
		for (auto particleId : parentContainer.GetSpawnedRange())
		{
			const float delay = runtime.DeltaTime() - normAges.Load(particleId) * lifeTimes.Load(particleId);
			triggers.emplace_back(particleId, delay);
		}
		runtime.AddSubInstances(triggers);
	}
};

CRY_PFX2_IMPLEMENT_COMPONENT_FEATURE(CParticleFeature, CFeatureChildOnBirth, "Child", "OnBirth", colorChild);

//////////////////////////////////////////////////////////////////////////

class CFeatureChildOnDeath : public CFeatureChildBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	bool IsDelayed() const override { return true; }

	void AddSubInstances(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		
		IFStream normAges = parentContainer.GetIFStream(EPDT_NormalAge);
		IFStream lifeTimes = parentContainer.GetIFStream(EPDT_LifeTime);

		THeapArray<SInstance> triggers(runtime.MemHeap());
		triggers.reserve(parentContainer.GetNumParticles());
		for (auto particleId : parentContainer.GetFullRange())
		{
			const float normAge = normAges.Load(particleId);
			if (IsExpired(normAge))
			{
				const float overAge = (normAge - 1.0f) * lifeTimes.Load(particleId);
				const float delay = runtime.DeltaTime() - overAge;
				triggers.emplace_back(particleId, delay);
			}
		}
		runtime.AddSubInstances(triggers);
	}
};

CRY_PFX2_IMPLEMENT_COMPONENT_FEATURE(CParticleFeature, CFeatureChildOnDeath, "Child", "OnDeath", colorChild);

//////////////////////////////////////////////////////////////////////////

class CFeatureChildOnCollide : public CFeatureChildBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	bool IsDelayed() const override { return true; }

	void AddSubInstances(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		THeapArray<SInstance> triggers(runtime.MemHeap());
		triggers.reserve(parentContainer.GetNumParticles());
		
		const auto contactPoints = parentContainer.IStream(EPDT_ContactPoint);

		for (auto particleId : parentContainer.GetFullRange())
		{
			const SContactPoint contact = contactPoints.Load(particleId);
			if (contact.m_state.collided)
				triggers.emplace_back(particleId, contact.m_time);
		}
		runtime.AddSubInstances(triggers);
	}
};

CRY_PFX2_IMPLEMENT_COMPONENT_FEATURE(CParticleFeature, CFeatureChildOnCollide, "Child", "OnCollide", colorChild);


}
