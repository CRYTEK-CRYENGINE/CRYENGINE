// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     18/12/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "FeatureCollision.h"

CRY_PFX2_DBG

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(ESecondGenMode,
                           All,
                           Random
                           )


typedef THeapArray<CParticleComponentRuntime::SInstance> TInstanceArray;

class CFeatureSecondGenBase : public CParticleFeature
{
public:
	CFeatureSecondGenBase()
		: m_probability(1.0f)
		, m_mode(ESecondGenMode::All) {}

	void ResolveDependency(CParticleComponent* pComponent) override
	{
		CRY_PFX2_ASSERT(pComponent);

		CParticleEffect* pEffect = pComponent->GetEffect();

		m_componentIds.clear();
		for (auto& componentName : m_componentNames)
		{
			TComponentId componentId = ResolveIdForName(pComponent, componentName);
			const bool isvalid = componentId != gInvalidId;
			if (isvalid)
			{
				const bool isUnique = std::find(m_componentIds.begin(), m_componentIds.end(), componentId) == m_componentIds.end();
				if (isUnique)
				{
					CParticleComponent* pSubComp = pEffect->GetCComponent(componentId);
					if (pSubComp->SetSecondGeneration(pComponent, IsDelayed()))
						m_componentIds.push_back(componentId);
				}
			}
		}
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

		const uint numEntries = m_componentIds.size();
		for (uint i = 0; i < numEntries; ++i)
		{
			const TComponentId componentId = m_componentIds[i];
			ICommonParticleComponentRuntime* pChildComponentRuntime =
			  context.m_runtime.GetEmitter()->GetRuntimes()[componentId].pRuntime;
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

	TComponentId ResolveIdForName(CParticleComponent* pComponent, const string& componentName)
	{
		TComponentId compId = pComponent->GetComponentId();
		CParticleEffect* pEffect = pComponent->GetEffect();
		TComponentId subComponentId = gInvalidId;

		if (componentName == pComponent->GetName())
		{}  // TODO - user error - user set this component as sub component for itself.
		for (TComponentId i = 0; i < pEffect->GetNumComponents(); ++i)
		{
			CParticleComponent* pSubComp = pEffect->GetCComponent(i);
			if (pSubComp->GetName() == componentName)
			{
				subComponentId = i;
				break;
			}
		}
		if (subComponentId == gInvalidId)
		{}      // TODO - user error - sub component name was not found
		return subComponentId;
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

	std::vector<string>       m_componentNames;
	std::vector<TComponentId> m_componentIds;
	SUnitFloat                m_probability;
	ESecondGenMode            m_mode;
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
			pComponent->AddToUpdateList(EUL_InitUpdate, this);
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		TInstanceArray triggers(*context.m_pMemHeap);
		triggers.reserve(container.GetNumSpawnedParticles());

		IFStream normAges = container.GetIFStream(EPDT_NormalAge);

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const float delay = (1.0f + normAges.Load(particleId)) * context.m_deltaTime;
			triggers.emplace_back(particleId, delay);
		}
		CRY_PFX2_FOR_END;

		TriggerParticles(context, triggers);
	}
};

CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(CParticleFeature, CFeatureSecondGenOnSpawn, "SecondGen", "OnSpawn", colorSecondGen);

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnDeath : public CFeatureSecondGenBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CFeatureSecondGenBase::AddToComponent(pComponent, pParams);
		if (GetNumConnectors() != 0)
			pComponent->AddToUpdateList(EUL_KillUpdate, this);
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

CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(CParticleFeature, CFeatureSecondGenOnDeath, "SecondGen", "OnDeath", colorSecondGen);

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnCollide : public CFeatureSecondGenBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CFeatureSecondGenBase::AddToComponent(pComponent, pParams);
		if (GetNumConnectors() != 0)
			pComponent->AddToUpdateList(EUL_Update, this);
	}

	virtual void Update(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		if (!container.HasData(EPDT_ContactPoint))
			return;

		const TIStream<SContactPoint> contactPoints = container.GetTIStream<SContactPoint>(EPDT_ContactPoint);
		TInstanceArray triggers(*context.m_pMemHeap);
		triggers.reserve(container.GetLastParticleId());

		CRY_PFX2_FOR_ACTIVE_PARTICLES(context)
		{
			const SContactPoint contact = contactPoints.Load(particleId);
			if (contact.m_state.collided)
				triggers.emplace_back(particleId, contact.m_time);
		}
		CRY_PFX2_FOR_END;

		TriggerParticles(context, triggers);
	}

	virtual bool IsDelayed() const override { return true; }
};

CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(CParticleFeature, CFeatureSecondGenOnCollide, "SecondGen", "OnCollide", colorSecondGen);

}
