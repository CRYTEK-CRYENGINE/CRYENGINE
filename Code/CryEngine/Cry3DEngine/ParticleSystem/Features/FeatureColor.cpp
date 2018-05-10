// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     29/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FeatureColor.h"
#include "Domain.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include <CrySerialization/SmartPtr.h>

namespace pfx2
{

MakeDataType(EPDT_Color, UCol, EDataFlags::BHasInit);

void IColorModifier::Serialize(Serialization::IArchive& ar)
{
	ar(m_enabled);
}

CFeatureFieldColor::CFeatureFieldColor() : m_color(255, 255, 255) {}

void CFeatureFieldColor::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	m_modInit.clear();
	m_modUpdate.clear();

	pComponent->InitParticles.add(this);
	pComponent->AddParticleData(EPDT_Color);

	for (auto& pModifier : m_modifiers)
	{
		if (pModifier && pModifier->IsEnabled())
			pModifier->AddToParam(pComponent, this);
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
	SModParticleField modContext;
	Serialization::SContext _modContext(ar, static_cast<IParamModContext*>(&modContext));
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
}

void CFeatureFieldColor::InitParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = context.m_container;
	IOColorStream colors = container.GetIOColorStream(EPDT_Color);

	UCol uColor;
	uColor.dcolor = m_color.pack_argb8888() | 0xff000000;
	UColv baseColor = ToUColv(uColor);
	for (auto particleGroupId : context.GetSpawnedGroupRange())
	{
		colors.Store(particleGroupId, baseColor);
	}

	SUpdateRange spawnRange = context.m_container.GetSpawnedRange();
	for (auto& pModifier : m_modInit)
		pModifier->Modify(context, spawnRange, colors);

	container.CopyData(EPDT_Color.InitType(), EPDT_Color, container.GetSpawnedRange());
}

void CFeatureFieldColor::UpdateParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = context.m_container;
	IOColorStream colors = container.GetIOColorStream(EPDT_Color);

	for (auto& pModifier : m_modUpdate)
		pModifier->Modify(context, context.m_updateRange, colors);
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

	virtual void AddToParam(CParticleComponent* pComponent, CFeatureFieldColor* pParam)
	{
		pParam->AddToInitParticles(this);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_luminance, "Luminance", "Luminance");
		ar(m_rgb, "RGB", "RGB");
	}

	virtual void Modify(const SUpdateContext& context, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (m_luminance && m_rgb)
			DoModify<true, true>(context, range, stream);
		else if (m_luminance)
			DoModify<true, false>(context, range, stream);
		if (m_rgb)
			DoModify<false, true>(context, range, stream);
	}

private:

	template<bool doLuminance, bool doRGB>
	void DoModify(const SUpdateContext& context, const SUpdateRange& range, IOColorStream stream) const
	{
		SChaosKeyV::Range randRange(1.0f - m_luminance, 1.0f);
		floatv rgb = ToFloatv(m_rgb),
		       unrgb = ToFloatv(1.0f - m_rgb);

		for (auto particleGroupId : SGroupRange(range))
		{
			ColorFv color = ToColorFv(stream.Load(particleGroupId));
			if (doLuminance)
			{
				const floatv lum = context.m_spawnRngv.Rand(randRange);
				color = color * lum;
			}
			if (doRGB)
			{
				ColorFv randColor(
					context.m_spawnRngv.RandUNorm(),
					context.m_spawnRngv.RandUNorm(),
					context.m_spawnRngv.RandUNorm());
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
	virtual void AddToParam(CParticleComponent* pComponent, CFeatureFieldColor* pParam)
	{
		if (m_spline.HasKeys())
			CDomain::AddToParam(pComponent, pParam, this);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : "";
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "ColorCurve", "Color Curve");
	}

	virtual void Modify(const SUpdateContext& context, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		CDomain::Dispatch<CColorCurve>(context, range, stream, EMD_PerParticle);
	}

	template<typename TTimeKernel>
	void DoModify(const SUpdateContext& context, const SUpdateRange& range, IOColorStream stream, const TTimeKernel& timeKernel) const
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

	virtual void Sample(Vec3* samples, int samplePoints) const
	{
		for (int i = 0; i < samplePoints; ++i)
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
	virtual void AddToParam(CParticleComponent* pComponent, CFeatureFieldColor* pParam)
	{
		if (m_spawnOnly)
			pParam->AddToInitParticles(this);
		else
			pParam->AddToUpdate(this);
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

	virtual void Modify(const SUpdateContext& context, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		const CAttributeInstance& attributes = context.m_runtime.GetEmitter()->GetAttributeInstance();
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
	CColorInherit()
		: m_spawnOnly(true) {}

	virtual void AddToParam(CParticleComponent* pComponent, CFeatureFieldColor* pParam)
	{
		if (m_spawnOnly)
			pParam->AddToInitParticles(this);
		else
			pParam->AddToUpdate(this);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(const SUpdateContext& context, const SUpdateRange& range, IOColorStream stream) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		CParticleContainer& parentContainer = context.m_parentContainer;
		if (!parentContainer.HasData(EPDT_Color))
			return;
		IPidStream parentIds = context.m_container.GetIPidStream(EPDT_ParentId);
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
	bool m_spawnOnly;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorInherit, "Inherit", "Inherit");

}
