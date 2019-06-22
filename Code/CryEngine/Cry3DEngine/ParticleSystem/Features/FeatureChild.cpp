// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "FeatureCollision.h"
#include "FeatureMotion.h"
#include <Cry3DEngine/ISurfaceType.h>

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
		pComponent->UpdateSpawners.add(this);
		if (IsDelayed())
			pParams->m_maxTotalLife += pComponent->GetParentComponent()->GetComponentParams().m_maxParticleLife;
	}

	virtual bool IsDelayed() const { return false; }
};

//////////////////////////////////////////////////////////////////////////

class CFeatureChildOnBirth : public CFeatureChildBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	static uint DefaultForType() { return EFT_Child; }

	void UpdateSpawners(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		IFStream normAges = parentContainer.GetIFStream(EPDT_NormalAge);
		IFStream lifeTimes = parentContainer.GetIFStream(EPDT_LifeTime);

		THeapArray<SSpawnerDesc> spawners(runtime.MemHeap());
		spawners.reserve(parentContainer.NumSpawned());
		const float deltaTime = runtime.DeltaTime();

		for (auto particleId : parentContainer.SpawnedRange())
		{
			const float delay = deltaTime - normAges.Load(particleId) * lifeTimes.Load(particleId);
			spawners.emplace_back(particleId, delay);
		}
		runtime.AddSpawners(spawners);
	}
};

CRY_PFX2_IMPLEMENT_COMPONENT_FEATURE(CParticleFeature, CFeatureChildOnBirth, "Child", "OnBirth", colorChild);

//////////////////////////////////////////////////////////////////////////

class CFeatureChildOnDeath : public CFeatureChildBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	bool IsDelayed() const override { return true; }

	void UpdateSpawners(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		
		IFStream normAges = parentContainer.GetIFStream(EPDT_NormalAge);
		IFStream lifeTimes = parentContainer.GetIFStream(EPDT_LifeTime);

		THeapArray<SSpawnerDesc> spawners(runtime.MemHeap());
		spawners.reserve(parentContainer.GetNumParticles());
		const float deltaTime = runtime.DeltaTime();

		for (auto particleId : parentContainer.FullRange())
		{
			const float normAge = normAges.Load(particleId);
			if (IsExpired(normAge))
			{
				const float overAge = (normAge - 1.0f) * lifeTimes.Load(particleId);
				const float delay = deltaTime - overAge;
				spawners.emplace_back(particleId, delay);
			}
		}
		runtime.AddSpawners(spawners);
	}
};

CRY_PFX2_IMPLEMENT_COMPONENT_FEATURE(CParticleFeature, CFeatureChildOnDeath, "Child", "OnDeath", colorChild);

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(ESurfaceRequirement,
	Any,
	Only,
	Not
)

class CFeatureChildOnCollide : public CFeatureChildBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	void Serialize(Serialization::IArchive& ar) override
	{
		SERIALIZE_VAR(ar, m_surfaceRequirement);
		if (m_surfaceRequirement != ESurfaceRequirement::Any)
		{
			SERIALIZE_VAR(ar, m_surfaces);
			if (ar.isInput() && GetVersion(ar) <= 13)
			{
				ESurfaceType surface;
				if (ar(surface, "Surface", "Surface"))
					m_surfaces.push_back(surface);
			}
		}
	}

	bool IsDelayed() const override { return true; }

	void UpdateSpawners(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		THeapArray<SSpawnerDesc> spawners(runtime.MemHeap());
		spawners.reserve(parentContainer.GetNumParticles());
		
		const auto contactPoints = parentContainer.IStream(EPDT_ContactPoint);

		for (auto particleId : parentContainer.FullRange())
		{
			const SContactPoint& contact = contactPoints[particleId];
			if (contact.m_collided)
			{
				if (m_surfaceRequirement == ESurfaceRequirement::Only)
				{
					if (!stl::find(m_surfaces, contact.m_pSurfaceType->GetId()))
						continue;
				}
				else if (m_surfaceRequirement == ESurfaceRequirement::Not)
				{
					if (stl::find(m_surfaces, contact.m_pSurfaceType->GetId()))
						continue;
				}
				spawners.emplace_back(particleId, contact.m_time);
			}
		}
		runtime.AddSpawners(spawners);
	}

private:
	ESurfaceRequirement     m_surfaceRequirement;
	TDynArray<ESurfaceType> m_surfaces;
};

CRY_PFX2_IMPLEMENT_COMPONENT_FEATURE(CParticleFeature, CFeatureChildOnCollide, "Child", "OnCollide", colorChild);


}
