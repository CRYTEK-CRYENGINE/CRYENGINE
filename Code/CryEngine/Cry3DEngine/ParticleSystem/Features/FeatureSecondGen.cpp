// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Legacy SecondGen features, replaced with Child features

#include "StdAfx.h"
#include "ParticleSystem/ParticleSystem.h"
#include "FeatureCollision.h"

#include <CrySerialization/IArchiveHost.h>

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
// Convert legacy SecondGen features, which specify parenting on parents, to Child features, which specify parenting on children

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

	void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_mode, "Mode", "Mode");
		ar(m_probability, "Probability", "Probability");
		ar(m_componentNames, "Components", "!Components");
		if (ar.isInput())
		{
			VersionFix(ar);

			// Add Child feature of corresponding name
			if (auto pParam = GetPSystem()->FindFeatureParam("Child", GetChildFeatureName()))
			{
				m_pChildFeature = static_cast<CParticleFeature*>((pParam->m_pFactory)());
				m_pChildFeature->Serialize(ar);
			}
		}
	}

	virtual cstr GetChildFeatureName() = 0;

	CParticleFeature* ResolveDependency(CParticleComponent* pComponent) override
	{
		if (!m_pChildFeature)
			return nullptr;

		CParticleEffect* pEffect = pComponent->GetEffect();

		// Count number of real children
		uint numChildren = 0;
		for (auto& componentName : m_componentNames)
			if (pEffect->FindComponentByName(componentName))
				numChildren++;

		if (numChildren == 0)
			return nullptr;

		float componentFrac = m_mode == ESecondGenMode::All ? 1.0f : 1.0f / numChildren;
		float probability = m_probability * componentFrac;
		float selectionStart = 0.0f;

		static uint                s_childGroup = 1;
		static CParticleComponent* s_lastComponent = nullptr;
		if (probability < 1.0f && numChildren > 1)
		{
			if (pComponent != s_lastComponent)
				s_childGroup = 1;
			else
				s_childGroup++;
			s_lastComponent = pComponent;
		}

		for (auto& componentName : m_componentNames)
		{
			if (auto pChild = pEffect->FindComponentByName(componentName))
			{
				pChild->SetParent(pComponent);

				// Add Child feature of corresponding name
				pChild->AddFeature(0, m_pChildFeature);

				if (probability < 1.0f)
				{
					// Add Spawn Random feature	
					if (auto pParam = CRY_PFX2_VERIFY(GetPSystem()->FindFeatureParam("Component", "ActivateRandom")))
					{
						IParticleFeature* pFeature = pChild->AddFeature(1, *pParam);
						XmlNodeRef attrs = gEnv->pSystem->CreateXmlNode("ActivateRandom");
						AddValue(attrs, "Probability", probability);

						if (numChildren > 1)
						{
							AddValue(attrs, "Group", s_childGroup);
						}
						if (componentFrac < 1.0f)
						{
							AddValue(attrs, "SelectionStart", selectionStart);
							selectionStart += componentFrac;
						}

						CRY_PFX2_VERIFY(Serialization::LoadXmlNode(*pFeature, attrs));
					}
				}
			}
		}

		// Delete this feature
		return nullptr;
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

	CParticleFeature*   m_pChildFeature = 0;
};

//////////////////////////////////////////////////////////////////////////

class CFeatureSecondGenOnSpawn : public CFeatureSecondGenBase
{
public:
	cstr GetChildFeatureName() override { return "OnBirth"; }
};

CRY_PFX2_LEGACY_FEATURE(CFeatureSecondGenOnSpawn, "SecondGen", "OnSpawn");

class CFeatureSecondGenOnDeath : public CFeatureSecondGenBase
{
public:
	cstr GetChildFeatureName() override { return "OnDeath"; }
};

CRY_PFX2_LEGACY_FEATURE(CFeatureSecondGenOnDeath, "SecondGen", "OnDeath");

class CFeatureSecondGenOnCollide : public CFeatureSecondGenBase
{
public:
	cstr GetChildFeatureName() override { return "OnCollide"; }
};

CRY_PFX2_LEGACY_FEATURE(CFeatureSecondGenOnCollide, "SecondGen", "OnCollide");

}
