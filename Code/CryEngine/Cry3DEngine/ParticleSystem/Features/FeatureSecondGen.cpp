// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Legacy SecondGen features, replaced with Child features

#include "StdAfx.h"
#include "ParticleSystem/ParticleSystem.h"
#include "FeatureCollision.h"

namespace pfx2
{

template<typename T>
ILINE T Verify(T in, const char* message)
{
	CRY_ASSERT_MESSAGE(in, message);
	return in;
}

#define CRY_PFX2_VERIFY(expr) Verify(expr, #expr)

//////////////////////////////////////////////////////////////////////////

template<typename T>
void AddValue(XmlNodeRef node, cstr name, const T& value)
{
	node->newChild(name)->setAttr("value", value);
}

SERIALIZATION_DECLARE_ENUM(ESecondGenMode,
                           All,
                           Random
                           )

class CFeatureSecondGenBase : public CParticleFeature
{
public:
	CFeatureSecondGenBase()
		: m_probability(1.0f)
		, m_mode(ESecondGenMode::All) {}

	CParticleFeature* MakeChildFeatures(CParticleComponent* pComponent, cstr featureName) const
	{
		CParticleEffect* pEffect = pComponent->GetEffect();

		float componentFrac = m_mode == ESecondGenMode::All ? 1.0f : 1.0f / max<int>(m_componentNames.size(), 1);
		float selectionStart = 0.0f;

		for (auto& componentName : m_componentNames)
		{
			if (auto pChild = pEffect->FindComponentByName(componentName))
			{
				pChild->SetParent(pComponent);

				// Add Child feature of corresponding name
				if (auto pParam = CRY_PFX2_VERIFY(GetPSystem()->FindFeatureParam("Child", featureName)))
				{
					pChild->AddFeature(0, *pParam);

					if (m_probability < 1.0f || componentFrac < 1.0f)
					{
						// Add Spawn Random feature	
						if (auto pParam = CRY_PFX2_VERIFY(GetPSystem()->FindFeatureParam("Component", "ActivateRandom")))
						{
							IParticleFeature* pFeature = pChild->AddFeature(1, *pParam);
							XmlNodeRef attrs = gEnv->pSystem->CreateXmlNode("ActivateRandom");
							AddValue(attrs, "Probability", m_probability * componentFrac);
							if (componentFrac < 1.0f)
							{
								AddValue(attrs, "SiblingExclusive", true);
								AddValue(attrs, "SelectionRange", selectionStart);
								selectionStart += componentFrac;
							}

							CRY_PFX2_VERIFY(Serialization::LoadXmlNode(*pFeature, attrs));
						}
					}
				}
			}
		}

		return nullptr;
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

	const SParticleFeatureParams& GetFeatureParams() const override
	{
		static SParticleFeatureParams s_params; return s_params;
	}

private:

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
	SUnitFloat          m_probability;
	ESecondGenMode      m_mode;
};

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnSpawn : public CFeatureSecondGenBase
{
public:
	CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		return MakeChildFeatures(pComponent, "OnBirth");
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureSecondGenOnSpawn, "SecondGen", "OnSpawn");

class CFeatureSecondGenOnDeath : public CFeatureSecondGenBase
{
public:
	CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		return MakeChildFeatures(pComponent, "OnDeath");
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureSecondGenOnDeath, "SecondGen", "OnDeath");

class CFeatureSecondGenOnCollide : public CFeatureSecondGenBase
{
public:
	CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		return MakeChildFeatures(pComponent, "OnCollide");
	}
};

CRY_PFX2_LEGACY_FEATURE(CFeatureSecondGenOnCollide, "SecondGen", "OnCollide");

}
