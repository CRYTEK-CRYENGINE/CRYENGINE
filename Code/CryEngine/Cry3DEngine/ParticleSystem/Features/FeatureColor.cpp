// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureColor.h"
#include "Domain.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include <CrySerialization/SmartPtr.h>

namespace pfx2
{

template<typename F>
ILINE void VecMul(Vec3_tpl<F>& a, Vec3_tpl<F> const& b)
{
	a.x *= b.x;
	a.y *= b.y;
	a.z *= b.z;
}

MakeDataType(EPDT_Color, UCol, EDD_ParticleUpdate);

using IColorModifier = ITypeModifier<UCol, Vec3>;

void CFeatureFieldColor::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	CParamModColor::AddToComponent(pComponent, this, EPDT_Color);

	if (auto pInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Color))
	{
		const int numSamples = gpu_pfx2::kNumModifierSamples;
		Vec3 samples[numSamples];
		Sample(samples);
		gpu_pfx2::SFeatureParametersColorTable table;
		table.samples = samples;
		table.numSamples = numSamples;
		pInt->SetParameters(table);
	}
}

void CFeatureFieldColor::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	ar(static_cast<CParamModColor&>(*this), "Color", "Color");
}

void CFeatureFieldColor::InitParticles(CParticleComponentRuntime& runtime)
{
	Init(runtime, EPDT_Color);
}

void CFeatureFieldColor::UpdateParticles(CParticleComponentRuntime& runtime)
{
	Update(runtime, EPDT_Color);
}

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureFieldColor, "Field", "Color", colorField);

//////////////////////////////////////////////////////////////////////////
// CColorRandom

class CColorRandom : public IColorModifier
{
public:

	virtual EDataDomain GetDomain() const
	{
		return EDD_None;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_luminance, "Luminance", "Luminance");
		ar(m_rgb, "RGB", "RGB");
	}

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (m_luminance && m_rgb)
			DoModify<true, true>(runtime, range, stream);
		else if (m_luminance)
			DoModify<true, false>(runtime, range, stream);
		if (m_rgb)
			DoModify<false, true>(runtime, range, stream);
	}
	virtual TRange<Vec3> GetMinMax() const
	{
		float lower = (1.0f - m_luminance) * (1.0f - m_rgb);
		return TRange<Vec3>(Vec3(lower), Vec3(1));
	}

private:

	template<bool doLuminance, bool doRGB>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const
	{
		auto randRange = SChaosKeyV::Range(1.0f - m_luminance, 1.0f);
		floatv rgb = ToFloatv(m_rgb),
		       unrgb = ToFloatv(1.0f - m_rgb);

		for (auto particleGroupId : SGroupRange(range))
		{
			Vec3v color = ToVec3v(stream.Load(particleGroupId));
			if (doLuminance)
			{
				const floatv lum = runtime.ChaosV()(randRange);
				color = color * lum;
			}
			if (doRGB)
			{
				Vec3v randColor(
					runtime.ChaosV().RandUNorm(),
					runtime.ChaosV().RandUNorm(),
					runtime.ChaosV().RandUNorm());
				color = color * unrgb + randColor * rgb;
			}
			stream.Store(particleGroupId, ToUColv(color));
		}
	}

	UUnitFloat m_luminance;
	UUnitFloat m_rgb;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorRandom, "ColorRandom", "Color Random");

//////////////////////////////////////////////////////////////////////////
// CColorCurve

class CColorCurve;

template<>
void TModFunction<CColorCurve, UCol, Vec3>::Sample(TVarArray<Vec3> samples) const
{
}

class CColorCurve : public TModFunction<CColorCurve, UCol, Vec3>
{
public:
	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		CDomain::Serialize(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : string("");
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "ColorCurve", "Color Curve");
	}

	template<typename TTimeKernel>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream, const TTimeKernel& timeKernel) const
	{
		if (!m_spline.HasKeys())
			return;

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv sample = AdjustDomain(timeKernel.Sample(particleGroupId));
			Vec3v color = ToVec3v(stream.Load(particleGroupId));
			const Vec3v curve = m_spline.Interpolate(sample);
			VecMul(color, curve);
			stream.Store(particleGroupId, ToUColv(color));
		}
	}

	virtual void Sample(TVarArray<Vec3> samples) const
	{
		if (samples.empty() || m_domain != EDomain::Age || m_sourceOwner != EDomainOwner::Self)
			return;

		float scale = 1.0f / (samples.size() - 1);
		for (uint i = 0; i < samples.size(); ++i)
		{
			Vec3& color = samples[i];
			Vec3 curve = m_spline.Interpolate(i * scale);
			VecMul(color, curve);
		}
	}

	virtual TRange<Vec3> GetMinMax() const
	{
		Range ranges[3] = { m_spline.GetValueRange(0), m_spline.GetValueRange(1), m_spline.GetValueRange(2) };
		return TRange<Vec3>(
			Vec3(ranges[0].start, ranges[1].start, ranges[2].start),
			Vec3(ranges[0].end, ranges[1].end, ranges[2].end));
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
		return m_spawnOnly ? EDD_HasUpdate : EDD_None;
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

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CAttributeInstance& attributes = runtime.GetEmitter()->GetAttributeInstance();
		ColorF attribute = m_attribute.GetValueAs(attributes, ColorB(~0U));
		Vec3 attr;
		attr.x = pow(attribute.r, m_gamma) * m_scale + m_bias;
		attr.y = pow(attribute.g, m_gamma) * m_scale + m_bias;
		attr.z = pow(attribute.b, m_gamma) * m_scale + m_bias;
		const Vec3v attrv = ToVec3v(attr);

		for (auto particleGroupId : SGroupRange(range))
		{
			Vec3v color = ToVec3v(stream.Load(particleGroupId));
			VecMul(color, attrv);
			stream.Store(particleGroupId, ToUColv(color));
		}
	}

private:
	CAttributeReference m_attribute;
	SFloat              m_scale     = 1;
	SFloat              m_bias      = 0;
	UFloat              m_gamma     = 1;
	bool                m_spawnOnly = true;
};

SERIALIZATION_CLASS_NAME(IColorModifier, CColorAttribute, "Attribute", "Attribute");

//////////////////////////////////////////////////////////////////////////
// CColorInherit

using IColorFieldModifier = IFieldModifier<UCol, Vec3>;

SERIALIZATION_INHERIT_CREATORS(IColorModifier, IColorFieldModifier);

class CColorInherit : public IColorFieldModifier
{
public:
	virtual EDataDomain GetDomain() const
	{
		return m_spawnOnly ? EDD_Spawner : EDD_SpawnerUpdate;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IColorModifier::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		if (!parentContainer.HasData(EPDT_Color))
			return;
		IPidStream parentIds = runtime.GetContainer().GetIPidStream(EPDT_ParentId);
		IColorStream parentColors = parentContainer.GetIColorStream(EPDT_Color, UCol{ {~0u} });

		for (auto particleGroupId : SGroupRange(range))
		{
			const TParticleIdv parentId = parentIds.Load(particleGroupId);
			Vec3v color = ToVec3v(stream.Load(particleGroupId));
			const Vec3v parent = ToVec3v(parentColors.SafeLoad(parentId));
			VecMul(color, parent);
			stream.Store(particleGroupId, ToUColv(color));
		}
	}

private:
	bool m_spawnOnly = true;
};

SERIALIZATION_CLASS_NAME(IColorFieldModifier, CColorInherit, "Inherit", "Inherit");

}
