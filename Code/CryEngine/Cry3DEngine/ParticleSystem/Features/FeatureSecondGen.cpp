// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     18/12/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem/ParticleSystem.h"
#include "FeatureCollision.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(ESecondGenMode,
                           All,
                           Random
                           )


typedef THeapArray<SInstance> TInstanceArray;

class CFeatureSecondGenBase : public CParticleFeature
{
public:
	CFeatureSecondGenBase()
		: m_probability(1.0f)
		, m_mode(ESecondGenMode::All) {}

	bool ResolveDependency(CParticleComponent* pComponent) override
	{
		CRY_PFX2_ASSERT(pComponent);

		CParticleEffect* pEffect = pComponent->GetEffect();

		m_components.clear();
		for (auto& componentName : m_componentNames)
		{
			if (auto pSubComp = pEffect->FindComponentByName(componentName))
			{
				if (!stl::find(m_components, pSubComp))
				{
					pSubComp->SetParentComponent(pComponent, IsDelayed());
					m_components.push_back(pSubComp);
				}
			}
		}
		return true;
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_mode, "Mode", "Mode");
		ar(m_probability, "Probability", "Probability");
		ar(m_componentNames, "Components", "!Components");
		if (ar.isInput())
			VersionFix(ar);
	}

	uint GetNumConnectors() const override
	{
		return m_componentNames.size();
	}

	const char* GetConnectorName(uint connectorId) const override
	{
		if (connectorId >= m_componentNames.size())
			return nullptr;
		return m_componentNames[connectorId];
	}

	void ConnectTo(const char* pOtherName) override
	{
		auto it = FindComponentName(pOtherName);
		if (it == m_componentNames.end())
			m_componentNames.push_back(pOtherName);
	}

	void DisconnectFrom(const char* pOtherName) override
	{
		auto it = FindComponentName(pOtherName);
		if (it != m_componentNames.end())
			m_componentNames.erase(it);
	}

	virtual bool IsDelayed() const { return false; }

protected:

	void TriggerParticles(const SUpdateContext& context, const TInstanceArray& triggers)
	{
		CParticleContainer& container = context.m_container;
		TInstanceArray newInstances(*context.m_pMemHeap);
		newInstances.reserve(triggers.size());

		const uint numEntries = m_components.size();
		for (uint i = 0; i < numEntries; ++i)
		{
			CParticleComponentRuntime* pChildComponentRuntime = context.m_runtime.GetEmitter()->GetRuntimeFor(m_components[i]);
			if (!pChildComponentRuntime)
				continue;
			SChaosKey chaosKey = context.m_spawnRng;

			for (const auto& trigger : triggers)
			{
				if (chaosKey.RandUNorm() <= m_probability)
				{
					if (m_mode == ESecondGenMode::All || chaosKey.Rand(numEntries) == i)
						newInstances.emplace_back(container.GetRealId(trigger.m_parentId), trigger.m_startDelay);
				}
			}
			pChildComponentRuntime->AddSubInstances(newInstances);
			newInstances.clear();
		}
	}

private:

	std::vector<string>::iterator FindComponentName(const char* pOther)
	{
		auto it = std::find_if(m_componentNames.begin(), m_componentNames.end(),
		                       [pOther](const string& componentName)
			{
				return strcmp(componentName.c_str(), pOther) == 0;
		  });
		return it;
	}

	void VersionFix(Serialization::IArchive& ar)
	{
		switch (GetVersion(ar))
		{
		case 3:
			{
				string subComponentName;
				ar(subComponentName, "subComponent", "Trigger Component");
				ConnectTo(subComponentName);
			}
			break;
		}
	}

	std::vector<string> m_componentNames;
	TComponents         m_components;
	SUnitFloat          m_probability;
	ESecondGenMode      m_mode;
};

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnSpawn : public CFeatureSecondGenBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CFeatureSecondGenBase::AddToComponent(pComponent, pParams);
		if (GetNumConnectors() != 0)
			pComponent->InitParticles.add(this);
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		TInstanceArray triggers(*context.m_pMemHeap);
		triggers.reserve(container.GetNumSpawnedParticles());

		IFStream normAges = container.GetIFStream(EPDT_NormalAge);

		for (auto particleId : context.GetSpawnedRange())
		{
			const float delay = (1.0f + normAges.Load(particleId)) * context.m_deltaTime;
			triggers.emplace_back(particleId, delay);
		}

		TriggerParticles(context, triggers);
	}
};

CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(CParticleFeature, CFeatureSecondGenOnSpawn, "SecondGen", "OnSpawn", colorChild);

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnDeath : public CFeatureSecondGenBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CFeatureSecondGenBase::AddToComponent(pComponent, pParams);
		if (GetNumConnectors() != 0)
			pComponent->KillParticles.add(this);
	}

	void KillParticles(const SUpdateContext& context, TConstArray<TParticleId> particleIds) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		TInstanceArray triggers(*context.m_pMemHeap);
		triggers.reserve(particleIds.size());
		
		IFStream normAges = container.GetIFStream(EPDT_NormalAge);
		IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);

		for (auto parentId : particleIds)
		{
			const float overAge = (normAges.Load(parentId) - 1.0f) * lifeTimes.Load(parentId);
			const float delay = context.m_deltaTime - overAge;
			triggers.emplace_back(parentId, delay);
		}

		TriggerParticles(context, triggers);
	}

	virtual bool IsDelayed() const override { return true; }
};

CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(CParticleFeature, CFeatureSecondGenOnDeath, "SecondGen", "OnDeath", colorChild);

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnCollide : public CFeatureSecondGenBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CFeatureSecondGenBase::AddToComponent(pComponent, pParams);
		if (GetNumConnectors() != 0)
			pComponent->UpdateParticles.add(this);
	}

	virtual void UpdateParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		if (!container.HasData(EPDT_ContactPoint))
			return;

		const TIStream<SContactPoint> contactPoints = container.GetTIStream<SContactPoint>(EPDT_ContactPoint);
		TInstanceArray triggers(*context.m_pMemHeap);
		triggers.reserve(container.GetLastParticleId());

		for (auto particleId : context.GetUpdateRange())
		{
			const SContactPoint contact = contactPoints.Load(particleId);
			if (contact.m_state.collided)
				triggers.emplace_back(particleId, contact.m_time);
		}

		TriggerParticles(context, triggers);
	}

	virtual bool IsDelayed() const override { return true; }
};

CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(CParticleFeature, CFeatureSecondGenOnCollide, "SecondGen", "OnCollide", colorChild);

}
