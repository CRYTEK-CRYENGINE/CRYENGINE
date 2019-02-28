// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureColor.h"
#include "Domain.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include <CrySerialization/SmartPtr.h>

namespace pfx2
{

MakeDataType(EPDT_Color, UCol, EDD_ParticleUpdate);

void IColorModifier::Serialize(Serialization::IArchive& ar)
{
	ar(m_enabled);
}

CFeatureFieldColor::CFeatureFieldColor() : m_color(255, 255, 255) {}

void CFeatureFieldColor::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->InitParticles.add(this);
	pComponent->AddParticleData(EPDT_Color);

	for (auto& pModifier : m_modifiers)
	{
		if (pModifier && pModifier->IsEnabled())
			pModifier->AddToParam(pComponent);
	}
	if (!m_modUpdate.empty())
	{
		pComponent->AddParticleData(EPDT_Color.InitType());
		pComponent->UpdateParticles.add(this);
	}

	if (auto pInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Color))
	{
		const int numSamples = gpu_pfx2::kNumModifierSamples;
		Vec3 samples[numSamples];
		Sample(samples, numSamples);
		gpu_pfx2::SFeatureParametersColorTable table;
		table.samples = samples;
		table.numSamples = numSamples;
		pInt->SetParameters(table);
	}
}

void CFeatureFieldColor::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	EDataDomain domain = EDD_ParticleUpdate;
	Serialization::SContext _modContext(ar, static_cast<EDataDomain*>(&domain));
	struct SerStruct
	{
		SerStruct(std::vector<PColorModifier>& modifiers, ColorB& color)
			: m_modifiers(modifiers), m_color(color) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(m_color, "Color", "^");
			ar(SkipEmpty(m_modifiers), "Modifiers", "^");
		}
		std::vector<PColorModifier>& m_modifiers;
		ColorB&                      m_color;
	} serStruct(m_modifiers, m_color);
	ar(serStruct, "Color", "Color");
	if (ar.isInput())
	{
		m_modInit.clear();
		m_modUpdate.clear();
		stl::find_and_erase_all(m_modifiers, nullptr);
		for (auto& pMod : m_modifiers)
		{
			if (pMod->IsEnabled())
			{
				if (pMod->GetDomain() & EDD_HasUpdate)
					m_modUpdate.push_back(pMod);
				else
					m_modInit.push_back(pMod);
			}
		}
	}
}

void CFeatureFieldColor::InitParticles(CParticleComponentRuntime& runtime)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = runtime.GetContainer();
	IOColorStream colors = container.GetIOColorStream(EPDT_Color);

	UCol uColor;
	uColor.dcolor = m_color.pack_argb8888() | 0xff000000;
	UColv baseColor = ToUColv(uColor);
	for (auto particleGroupId : runtime.SpawnedRangeV())
	{
		colors.Store(particleGroupId, baseColor);
	}

	SUpdateRange spawnRange = runtime.SpawnedRange();
	for (auto& pModifier : m_modInit)
		pModifier->Modify(runtime, spawnRange, colors);

	container.CopyData(EPDT_Color.InitType(), EPDT_Color, container.GetSpawnedRange());
}

void CFeatureFieldColor::UpdateParticles(CParticleComponentRuntime& runtime)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = runtime.GetContainer();
	IOColorStream colors = container.GetIOColorStream(EPDT_Color);

	for (auto& pModifier : m_modUpdate)
		pModifier->Modify(runtime, runtime.FullRange(), colors);
}

void CFeatureFieldColor::AddToInitParticles(IColorModifier* pMod)
{
	if (std::find(m_modInit.begin(), m_modInit.end(), pMod) == m_modInit.end())
		m_modInit.push_back(pMod);
}

void CFeatureFieldColor::AddToUpdate(IColorModifier* pMod)
{
	if (std::find(m_modUpdate.begin(), m_modUpdate.end(), pMod) == m_modUpdate.end())
		m_modUpdate.push_back(pMod);
}

void CFeatureFieldColor::Sample(Vec3* samples, const int numSamples)
{
	Vec3 baseColor(m_color.r / 255.f, m_color.g / 255.f, m_color.b / 255.f);

	for (int i = 0; i < numSamples; ++i)
		samples[i] = baseColor;
	for (auto& pModifier : m_modUpdate)
		pModifier->Sample(samples, numSamples);
}

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureFieldColor, "Field", "Color", colorField);

//////////////////////////////////////////////////////////////////////////
// CColorRandom

class CColorRandom : public IColorModifier
{
public:

	virtual EDataDomain GetDomain() const
	{
		return EDD_PerParticle;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_luminance, "Luminance", "Luminance");
		ar(m_rgb, "RGB", "RGB");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (m_luminance && m_rgb)
			DoModify<true, true>(runtime, range, stream);
		else if (m_luminance)
			DoModify<true, false>(runtime, range, stream);
		if (m_rgb)
			DoModify<false, true>(runtime, range, stream);
	}

private:

	template<bool doLuminance, bool doRGB>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const
	{
		SChaosKeyV::Range randRange(1.0f - m_luminance, 1.0f);
		floatv rgb = ToFloatv(m_rgb),
		       unrgb = ToFloatv(1.0f - m_rgb);

		for (auto particleGroupId : SGroupRange(range))
		{
			ColorFv color = ToColorFv(stream.Load(particleGroupId));
			if (doLuminance)
			{
				const floatv lum = runtime.ChaosV().Rand(randRange);
				color = color * lum;
			}
			if (doRGB)
			{
				ColorFv randColor(
					runtime.ChaosV().RandUNorm(),
					runtime.ChaosV().RandUNorm(),
					runtime.ChaosV().RandUNorm());
				color = color * unrgb + randColor * rgb;
			}
			stream.Store(particleGroupId, ColorFvToUColv(color));
		}
	}

	UUnitFloat m_luminance;
	UUnitFloat m_rgb;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorRandom, "ColorRandom", "Color Random");

//////////////////////////////////////////////////////////////////////////
// CColorCurve

class CColorCurve : public CDomain, public IColorModifier
{
public:
	virtual EDataDomain GetDomain() const { return CDomain::GetDomain(); }

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : "";
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "ColorCurve", "Color Curve");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		if (m_spline.HasKeys())
			CDomain::Dispatch<CColorCurve>(runtime, range, stream, EDD_PerParticle);
	}

	template<typename TTimeKernel>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream, const TTimeKernel& timeKernel) const
	{
		const floatv rate = ToFloatv(m_domainScale);
		const floatv offset = ToFloatv(m_domainBias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv sample = MAdd(timeKernel.Sample(particleGroupId), rate, offset);
			const ColorFv color0 = ToColorFv(stream.Load(particleGroupId));
			const ColorFv curve = m_spline.Interpolate(sample);
			const ColorFv color1 = color0 * curve;
			stream.Store(particleGroupId, ColorFvToUColv(color1));
		}
	}

	virtual void Sample(Vec3* samples, uint samplePoints) const
	{
		for (uint i = 0; i < samplePoints; ++i)
		{
			const float point = (float) i / samplePoints;
			Vec3 color0 = samples[i];
			ColorF curve = m_spline.Interpolate(point);
			Vec3 color1(color0.x * curve.r, color0.y * curve.g, color0.z * curve.b);
			samples[i] = color1;
		}
	}
private:
	CParticleColorSpline m_spline;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorCurve, "ColorCurve", "Color Curve");

//////////////////////////////////////////////////////////////////////////
// CColorAttribute

class CColorAttribute : public IColorModifier
{
public:
	virtual EDataDomain GetDomain() const
	{
		return m_spawnOnly ? EDD_PerParticle : EDD_ParticleUpdate;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_attribute, "Name", "Attribute Name");
		ar(m_scale, "Scale", "Scale");
		ar(m_bias, "Bias", "Bias");
		ar(m_gamma, "Gamma", "Gamma");
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		const CAttributeInstance& attributes = runtime.GetEmitter()->GetAttributeInstance();
		ColorF attribute = m_attribute.GetValueAs(attributes, ColorB(~0U));
		attribute.r = pow(attribute.r, m_gamma) * m_scale + m_bias;
		attribute.g = pow(attribute.g, m_gamma) * m_scale + m_bias;
		attribute.b = pow(attribute.b, m_gamma) * m_scale + m_bias;
		const ColorFv value = ToColorFv(attribute);

		for (auto particleGroupId : SGroupRange(range))
		{
			const ColorFv color0 = ToColorFv(stream.Load(particleGroupId));
			const ColorFv color1 = color0 * value;
			stream.Store(particleGroupId, ColorFvToUColv(color1));
		}
	}

private:
	CAttributeReference m_attribute;
	SFloat              m_scale     = 1;
	SFloat              m_bias      = 0;
	UFloat              m_gamma     = 1;
	bool                m_spawnOnly = false;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorAttribute, "Attribute", "Attribute");

//////////////////////////////////////////////////////////////////////////
// CColorInherit

class CColorInherit : public IColorModifier
{
public:
	virtual EDataDomain GetDomain() const
	{
		return m_spawnOnly ? EDD_PerParticle : EDD_ParticleUpdate;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		if (!parentContainer.HasData(EPDT_Color))
			return;
		IPidStream parentIds = runtime.GetContainer().GetIPidStream(EPDT_ParentId);
		IColorStream parentColors = parentContainer.GetIColorStream(EPDT_Color, UCol{ {~0u} });

		for (auto particleGroupId : SGroupRange(range))
		{
			const TParticleIdv parentId = parentIds.Load(particleGroupId);
			const ColorFv color0 = ToColorFv(stream.Load(particleGroupId));
			const ColorFv parent = ToColorFv(parentColors.SafeLoad(parentId));
			const ColorFv color1 = color0 * parent;
			stream.Store(particleGroupId, ColorFvToUColv(color1));
		}
	}

private:
	bool m_spawnOnly = true;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorInherit, "Inherit", "Inherit");

}
