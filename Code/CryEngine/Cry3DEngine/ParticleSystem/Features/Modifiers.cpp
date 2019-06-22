// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParamMod.h"
#include "Domain.h"
#include "ParticleSystem/ParticleSystem.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include <CryMath/PNoise3.h>

namespace pfx2
{

using IFModifier = ITypeModifier<float>;

//////////////////////////////////////////////////////////////////////////
// CModCurve

class CModCurve : public TModFunction<CModCurve, float>
{
public:
	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		CDomain::Serialize(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : string("");
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "Curve", "Curve");
	}

	template<typename TTimeKernel>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TTimeKernel& timeKernel) const
	{
		if (!m_spline.HasKeys())
			return;

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = AdjustDomain(timeKernel.Sample(particleGroupId));
			const floatv value = m_spline.Interpolate(time);
			const floatv outvalue = Mul(inValue, value);
			stream.Store(particleGroupId, outvalue);
		}
	}

	virtual Range GetMinMax() const
	{
		return m_spline.GetValueRange();
	}

private:
	CParticleSpline m_spline;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModCurve, "Curve", "Curve");

//////////////////////////////////////////////////////////////////////////
// CModDoubleCurve

class CModDoubleCurve : public TModFunction<CModDoubleCurve, float>
{
public:
	virtual void AddToParam(CParticleComponent* pComponent)
	{
		if (m_spline.HasKeys())
		{
			CDomain::AddToParam(pComponent);
			pComponent->AddParticleData(EPDT_Random);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		CDomain::Serialize(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : string("");
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "DoubleCurve", "Double Curve");
	}

	template<typename TimeKernel>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& container = runtime.GetContainer();
		const IFStream unormRands = container.GetIFStream(EPDT_Random);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv unormRand = unormRands.Load(particleGroupId);
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = AdjustDomain(timeKernel.Sample(particleGroupId));
			const floatv value = m_spline.Interpolate(time, unormRand);
			const floatv outvalue = Mul(inValue, value);
			stream.Store(particleGroupId, outvalue);
		}
	}

	virtual Range GetMinMax() const
	{
		return m_spline.GetValueRange();
	}

private:
	CParticleDoubleSpline m_spline;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModDoubleCurve, "DoubleCurve", "Double Curve");

//////////////////////////////////////////////////////////////////////////
// CModRandom

class CModRandom : public IFModifier
{
public:
	CModRandom(float amount = 0.0f)
		: m_amount(amount)
	{}

	virtual EDataDomain GetDomain() const
	{
		return EDD_None;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		ar(m_amount, "Amount", "Amount");
	}

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		auto randRange = SChaosKeyV::Range(1.0f - m_amount, 1.0f);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv value = runtime.ChaosV()(randRange);
			const floatv outvalue = Mul(inValue, value);
			stream.Store(particleGroupId, outvalue);
		}
	}

	virtual Range GetMinMax() const
	{
		return Range(1.0f - m_amount, 1.0f);
	}

private:

	TValue<THardLimits<float, 0, 2>> m_amount;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModRandom, "Random", "Random");

//////////////////////////////////////////////////////////////////////////
// CModNoise

SERIALIZATION_DECLARE_ENUM(EParamNoiseMode,
                           Smooth,
                           Fractal,
                           Pulse,
                           _Random
                           )

class CModNoise : public TModFunction<CModNoise, float>
{
public:
	CModNoise()
		: m_amount(0.0f)
		, m_pulseWidth(0.5f)
		, m_mode(EParamNoiseMode::Smooth) {}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		CDomain::Serialize(ar);
		ar(m_mode, "Mode", "Mode");
		ar(m_amount, "Amount", "Amount");
		if (m_mode == EParamNoiseMode::Pulse)
			ar(m_pulseWidth, "PulseWidth", "Pulse Width");

		if (ar.isInput())
			VersionFix(ar);
	}

	virtual IFModifier* VersionFixReplace() const
	{
		if (m_mode == EParamNoiseMode::_Random)
			return new CModRandom(m_amount);
		return nullptr;
	}

	template<typename TimeKernel>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		switch (m_mode)
		{
		case EParamNoiseMode::Smooth:
			DoModify(range, stream, timeKernel,
			       [](floatv time){ return Smooth(time); });
			break;
		case EParamNoiseMode::Fractal:
			DoModify(range, stream, timeKernel,
			       [](floatv time){ return Fractal(time); });
			break;
		case EParamNoiseMode::Pulse:
			DoModify(range, stream, timeKernel,
			       [this](floatv time){ return Pulse(time); });
			break;
		}
	}

	virtual Range GetMinMax() const
	{
		return Range(1.0f - m_amount, 1.0f);
	}

private:
	template<typename TimeKernel, typename ModeFn>
	ILINE void DoModify(const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel, ModeFn modeFn) const
	{
		const floatv one = ToFloatv(1.0f);
		const floatv amount = ToFloatv(m_amount);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = AdjustDomain(timeKernel.Sample(particleGroupId));
			const floatv value = modeFn(time);
			const floatv outvalue = Mul(inValue, Sub(one, Mul(value, amount)));
			stream.Store(particleGroupId, outvalue);
		}
	}

	// PFX2_TODO : properly vectorize this code and make it not dependent on SSE4.1
	ILINE static floatv UPNoise(floatv x)
	{
		CPNoise3& noiseGen = *gEnv->pSystem->GetNoiseGen();
#ifndef CRY_PFX2_USE_SSE
		return noiseGen.Noise1D(x) * 2.5f;
#else
		const float x0 = get_element<0>(x);
		const float x1 = get_element<1>(x);
		const float x2 = get_element<2>(x);
		const float x3 = get_element<3>(x);
		const float v0 = noiseGen.Noise1D(x0);
		const float v1 = noiseGen.Noise1D(x1);
		const float v2 = noiseGen.Noise1D(x2);
		const float v3 = noiseGen.Noise1D(x3);
		return _mm_mul_ps(_mm_set_ps(v3, v2, v1, v0), _mm_set1_ps(2.5f));
#endif
	}

	ILINE static floatv Smooth(floatv time)
	{
		const floatv half = ToFloatv(0.5f);
		return MAdd(UPNoise(time), half, half);
	}

	ILINE static floatv Fractal(floatv time)
	{
		const floatv two = ToFloatv(2.0f);
		const floatv four = ToFloatv(4.0f);
		const floatv half = ToFloatv(0.5f);
		const floatv quarter = ToFloatv(0.25f);
		floatv fractal = UPNoise(time);
		fractal = MAdd(UPNoise(Mul(time, two)), half, fractal);
		fractal = MAdd(UPNoise(Mul(time, four)), quarter, fractal);
		return MAdd(fractal, half, half);
	}

	ILINE floatv Pulse(floatv time) const
	{
		const floatv one = ToFloatv(1.0f);
		const floatv pulseWidth = ToFloatv(m_pulseWidth);
		return if_else_zero(UPNoise(time) < pulseWidth, one);
	}

	void VersionFix(Serialization::IArchive& ar)
	{
		if (GetVersion(ar) <= 8)
		{
			float rate;
			if (ar(rate, "Rate", "Rate"))
				m_domainScale *= rate;
		}
	}

	SFloat          m_amount;
	UUnitFloat      m_pulseWidth;
	EParamNoiseMode m_mode;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModNoise, "Noise", "Noise");

//////////////////////////////////////////////////////////////////////////
// CModWave

SERIALIZATION_DECLARE_ENUM(EWaveType,
                           Sin,
                           Saw,
                           Pulse
                           )

class CModWave : public TModFunction<CModWave, float>
{
public:
	CModWave()
		: m_waveType(EWaveType::Sin)
		, m_amplitude(1.0f)
		, m_bias(0.5f)
		, m_inverse(false) {}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		CDomain::Serialize(ar);
		ar(m_waveType, "Wave", "Wave");
		ar(m_amplitude, "Amplitude", "Amplitude");
		ar(m_bias, "Bias", "Bias");
		ar(m_inverse, "Inverse", "Inverse");
		if (ar.isInput())
			VersionFix(ar);
	}

	void VersionFix(Serialization::IArchive& ar)
	{
		if (GetVersion(ar) <= 8)
		{
			float rate, phase;
			if (ar(rate, "Rate"))
				m_domainScale *= rate;
			if (ar(phase, "Phase"))
				m_domainBias -= phase * m_domainScale;
			m_scaleV = ToFloatv(m_domainScale);
			m_biasV  = ToFloatv(m_domainBias);
		}
	}

	template<typename TimeKernel>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		switch (m_waveType)
		{
		case EWaveType::Sin:
			DoModify(range, stream, timeKernel,
			       [](floatv time) { return Sin(time); });
			break;
		case EWaveType::Saw:
			DoModify(range, stream, timeKernel,
			       [](floatv time) { return Saw(time); });
			break;
		case EWaveType::Pulse:
			DoModify(range, stream, timeKernel,
			       [](floatv time) { return Pulse(time); });
			break;
		}
	}

	virtual Range GetMinMax() const
	{
		return Range(m_bias - m_amplitude * 0.5f, m_bias + m_amplitude * 0.5f);
	}

private:
	template<typename TimeKernel, typename WaveFn>
	void DoModify(const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel, WaveFn waveFn) const
	{
		const floatv mult = ToFloatv(m_amplitude * (m_inverse ? -1.0f : 1.0f) * 0.5f);
		const floatv bias = ToFloatv(m_bias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = AdjustDomain(timeKernel.Sample(particleGroupId));
			const floatv sample = waveFn(time);
			const floatv value = MAdd(sample, mult, bias);
			const floatv outvalue = Mul(inValue, value);
			stream.Store(particleGroupId, outvalue);
		}
	}

	// PFX2_TODO : improve functions and place them in a common library
#ifdef CRY_PFX2_USE_SSE
	ILINE static floatv SinApproax(floatv x)
	{
		const floatv one = ToFloatv(1.0f);
		const floatv half = ToFloatv(0.5f);
		const floatv negHalfPi = ToFloatv(-gf_PI * 0.5f);
		const floatv pi = ToFloatv(gf_PI);
		const floatv ipi = ToFloatv(1.0f / gf_PI);
		const floatv ipi2 = ToFloatv(1.0f / gf_PI2);

		const floatv x1 = MAdd(frac(Mul(x, ipi)), pi, negHalfPi);
		const floatv m = signnz(frac(Mul(x, ipi2)) - half);

		const floatv p0 = ToFloatv(-0.4964738f);
		const floatv p1 = ToFloatv(0.036957536f);
		const floatv x2 = Mul(x1, x1);
		const floatv x4 = Mul(x2, x2);
		const floatv result = Mul(MAdd(x4, p1, MAdd(x2, p0, one)), m);

		return result;
	}

	ILINE static floatv CosApproax(floatv x)
	{
		const floatv halfPi = ToFloatv(gf_PI * 0.5f);
		return SinApproax(Sub(x, halfPi));
	}
#else

	ILINE static floatv CosApproax(floatv x)
	{
		return cosf(x);
	}

#endif

	ILINE static floatv Sin(floatv time)
	{
		return -CosApproax(time * ToFloatv(gf_PI2));
	}

	ILINE static floatv Saw(floatv time)
	{
		const floatv f = frac(time);
		return f + f - ToFloatv(1.0f);
	}

	ILINE static floatv Pulse(floatv time)
	{
		const floatv half = ToFloatv(0.5f);
		return signnz(half - frac(time));
	}

	EWaveType m_waveType;
	UFloat10  m_rate;
	UFloat    m_amplitude;
	SFloat    m_phase;
	SFloat    m_bias;
	bool      m_inverse;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModWave, "Wave", "Wave");

//////////////////////////////////////////////////////////////////////////
// CModLinear

class CModLinear : public TModFunction<CModLinear, float>
{
public:
	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		CDomain::Serialize(ar);
	}

	template<typename TimeKernel>
	void DoModify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = AdjustDomain(timeKernel.Sample(particleGroupId));
			const floatv outvalue = Mul(inValue, time);
			stream.Store(particleGroupId, outvalue);
		}
	}
};

SERIALIZATION_CLASS_NAME(IFModifier, CModLinear, "Linear", "Linear");

//////////////////////////////////////////////////////////////////////////
// CModConfigSpec

struct SSpecData
{
	const char* m_pName;
	const char* m_pLabel;
	uint  m_index;
};

const SSpecData gConfigSpecs[] =
{
	{ "Low",      "Low",            CONFIG_LOW_SPEC },
	{ "Medium",   "Medium",         CONFIG_MEDIUM_SPEC },
	{ "High",     "High",           CONFIG_HIGH_SPEC },
	{ "VeryHigh", "Very High",      CONFIG_VERYHIGH_SPEC },
	{ "XBO",      "XBox One",       CONFIG_DURANGO },
	{ "XboxOneX", "XBox One X",     CONFIG_DURANGO_X },
	{ "PS4",      "Playstation 4",  CONFIG_ORBIS },
};

const uint gNumConfigSpecs = sizeof(gConfigSpecs) / sizeof(gConfigSpecs[0]);

class CModConfigSpec : public IFModifier
{
public:
	CModConfigSpec()
		: m_range(1.0f, 1.0f)
	{
		for (uint i = 0; i < gNumConfigSpecs; ++i)
			m_specMultipliers[i] = 1.0f;
	}

	virtual EDataDomain GetDomain() const
	{
		return EDD_None;
	}

	virtual Range GetMinMax() const
	{
		return m_range;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);

		for (uint i = 0; i < gNumConfigSpecs; ++i)
			ar(m_specMultipliers[i], gConfigSpecs[i].m_pName, gConfigSpecs[i].m_pLabel);
		m_range = Range(m_specMultipliers[0], m_specMultipliers[0]);
		for (uint i = 1; i < gNumConfigSpecs; ++i)
			m_range = Range(min(m_range.start, m_specMultipliers[1]), max(m_range.end, m_specMultipliers[0]));
	}

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const uint particleSpec = runtime.GetEmitter()->GetParticleSpec();
		float multiplier = 1.0f;

		for (uint i = 0; i < gNumConfigSpecs; ++i)
		{
			if (gConfigSpecs[i].m_index == particleSpec)
			{
				multiplier = m_specMultipliers[i];
				break;
			}
		}

		if (multiplier != 1.0f)
		{
			floatv multiplierV = ToFloatv(multiplier);
			for (auto particleGroupId : SGroupRange(range))
				stream[particleGroupId] *= multiplierV;
		}
	}

private:
	float m_specMultipliers[gNumConfigSpecs];
	Range m_range;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModConfigSpec, "ConfigSpec", "Config Spec");

//////////////////////////////////////////////////////////////////////////
// CModAttribute

class CModAttribute : public IFModifier
{
public:
	virtual EDataDomain GetDomain() const
	{
		return m_spawnOnly ? EDD_None : EDD_HasUpdate;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		ar(m_attribute, "Name", "Attribute Name");
		ar(m_scale, "Scale", "Scale");
		ar(m_bias, "Bias", "Bias");
		const EDataDomain domain = *ar.context<EDataDomain>();
		if (!(domain & EDD_HasUpdate))
			m_spawnOnly = true;
		else
			SERIALIZE_VAR(ar, m_spawnOnly);
	}

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CAttributeInstance& attributes = runtime.GetEmitter()->GetAttributeInstance();
		const float attribute = m_attribute.GetValueAs(attributes, 1.0f);
		const floatv value = ToFloatv(attribute * m_scale + m_bias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv outvalue = inValue * value;
			stream.Store(particleGroupId, outvalue);
		}
	}

private:
	CAttributeReference m_attribute;
	SFloat              m_scale     = 1;
	SFloat              m_bias      = 0;
	bool                m_spawnOnly = false;
};

SERIALIZATION_CLASS_NAME(IFModifier, CModAttribute, "Attribute", "Attribute");

//////////////////////////////////////////////////////////////////////////
using IFFieldModifier = IFieldModifier<float>;

SERIALIZATION_INHERIT_CREATORS(IFModifier, IFFieldModifier);

//////////////////////////////////////////////////////////////////////////
// CModInherit

class CModInherit : public IFFieldModifier
{
public:
	virtual EDataDomain GetDomain() const
	{
		return m_spawnOnly ? EDD_Spawner : EDD_SpawnerUpdate;
	}

	virtual void SetDataType(EParticleDataType dataType)
	{
		m_dataType = dataType;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IFModifier::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		if (!parentContainer.HasData(m_dataType))
			return;
		IPidStream parentIds = runtime.GetContainer().GetIPidStream(EPDT_ParentId);
		IFStream parentStream = parentContainer.GetIFStream(TDataType<float>(m_dataType));

		for (auto particleGroupId : SGroupRange(range))
		{
			const TParticleIdv parentId = parentIds.Load(particleGroupId);
			const floatv input = stream.Load(particleGroupId);
			const floatv parent = parentStream.SafeLoad(parentId);
			const floatv output = Mul(input, parent);
			stream.Store(particleGroupId, output);
		}
	}

private:
	bool              m_spawnOnly = true;
	EParticleDataType m_dataType;
};

SERIALIZATION_CLASS_NAME(IFFieldModifier, CModInherit, "Inherit", "Inherit");


}
