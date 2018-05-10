// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParticleSystem/ParticleComponentRuntime.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentEnableIf

class CFeatureComponentEnableIf : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual bool CanMakeRuntime(CParticleEmitter* pEmitter) const override
	{
		return m_attribute.GetValueAs(pEmitter->GetAttributeInstance(), true);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_attribute, "Attribute", "Attribute");
	}

private:
	CAttributeReference m_attribute;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentEnableIf, "Component", "EnableIf", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentActivateIf

class CFeatureComponentActivateIf : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->MainPreUpdate.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_attribute, "Attribute", "Attribute");
	}

	virtual void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		if (!m_attribute.GetValueAs(pComponentRuntime->GetEmitter()->GetAttributeInstance(), true))
			pComponentRuntime->RemoveAllSubInstances();
	}

private:
	CAttributeReference m_attribute;
};

CRY_PFX2_LEGACY_FEATURE(CFeatureComponentActivateIf, "Component", "SpawnIf");
CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentActivateIf, "Component", "ActivateIf", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentActivateRandom

class CFeatureComponentActivateRandom : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->CullSubInstances.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_probability);
		if (m_probability < 1.0f)
		{
			SERIALIZE_VAR(ar, m_siblingExclusive);
			if (m_siblingExclusive)
				SERIALIZE_VAR(ar, m_selectionRange);
		}
	}

	virtual void CullSubInstances(const SUpdateContext& context, TVarArray<SInstance>& instances) override
	{
		if (m_probability < 1.0f && instances.size() > 0)
		{
			auto seed = context.m_runtime.GetEmitter()->GetCurrentSeed();
			SChaosKey childKey(seed ^ instances[0].m_parentId);
			uint i = 0;
			for (const auto& instance : instances)
			{
				if (m_probability < 1.0f)
				{
					if (m_siblingExclusive)
					{
						SChaosKey parentKey(seed ^ instance.m_parentId);
						float select = parentKey.RandUNorm();
						if (select < m_selectionRange || select > m_selectionRange + m_probability)
							continue;
					}
					else if (childKey.RandUNorm() > m_probability)
						continue;
					instances[i++] = instance;
				}
			}
			instances.resize(i);
		}
	}

private:
	bool       m_siblingExclusive = false;
	UUnitFloat m_selectionRange   = 0.0f;
	UUnitFloat m_probability      = 1.0f;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentActivateRandom, "Component", "ActivateRandom", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentEnableByConfig

SERIALIZATION_DECLARE_ENUM(EEnableIfConfig,
	Low      = CONFIG_LOW_SPEC,
	Medium   = CONFIG_MEDIUM_SPEC,
	High     = CONFIG_HIGH_SPEC,
	VeryHigh = CONFIG_VERYHIGH_SPEC
	)

class CFeatureComponentEnableByConfig : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	CFeatureComponentEnableByConfig()
		: m_minimumConfig(EEnableIfConfig::Low)
		, m_maximumConfig(EEnableIfConfig::VeryHigh)
		, m_PC(true), m_XBoxOne(true), m_PS4(true) {}

	virtual bool CanMakeRuntime(CParticleEmitter* pEmitter) const override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const uint particleSpec = pEmitter->GetParticleSpec();
		const bool isPc = particleSpec <= CONFIG_VERYHIGH_SPEC;

		if (isPc)
		{
			return m_PC &&
				(particleSpec >= uint(m_minimumConfig)) &&
				(particleSpec <= uint(m_maximumConfig));
		}
		else if (particleSpec == CONFIG_DURANGO && m_XBoxOne)
			return true;
		else if (particleSpec == CONFIG_ORBIS && m_PS4)
			return true;

		return false;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_PC, "PC", "PC");
		if (m_PC)
		{
			ar(m_minimumConfig, "Minimum", "Minimum");
			ar(m_maximumConfig, "Maximum", "Maximum");
		}
		ar(m_XBoxOne, "XBoxOne", "XBox One");
		ar(m_PS4, "PS4", "Playstation 4");
	}

private:
	EEnableIfConfig m_minimumConfig;
	EEnableIfConfig m_maximumConfig;
	bool m_PC, m_XBoxOne, m_PS4;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentEnableByConfig, "Component", "EnableByConfig", colorComponent);

}
