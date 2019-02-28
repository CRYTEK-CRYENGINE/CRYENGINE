// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ParamMod.h"
#include "Domain.h"
#include "ParticleSystem/ParticleSystem.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include <CryMath/PNoise3.h>

namespace pfx2
{

class CFTimeSource : public CDomain, public IModifier
{
public:
	virtual EDataDomain GetDomain() const
	{
		return CDomain::GetDomain();
	}
};

//////////////////////////////////////////////////////////////////////////
// CModCurve

class CModCurve : public CFTimeSource
{
public:
	virtual void AddToParam(CParticleComponent* pComponent)
	{
		CDomain::AddToParam(pComponent);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : "";
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "Curve", "Curve");
	}

	virtual void Sample(float* samples, uint numSamples) const
	{
		for (uint i = 0; i < numSamples; ++i)
		{
			const float point = (float)i / numSamples;
			float dataIn = samples[i];
			float spline = m_spline.Interpolate(point);
			float dataOut = dataIn * spline;
			samples[i] = dataOut;
		}
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		if (m_spline.HasKeys())
			CDomain::Dispatch<CModCurve>(runtime, range, stream, domain);
	}

	template<typename TTimeKernel>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TTimeKernel& timeKernel) const
	{
		const floatv rate = ToFloatv(m_domainScale);
		const floatv offset = ToFloatv(m_domainBias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = MAdd(timeKernel.Sample(particleGroupId), rate, offset);
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

SERIALIZATION_CLASS_NAME(IModifier, CModCurve, "Curve", "Curve");

//////////////////////////////////////////////////////////////////////////
// CModDoubleCurve

class CModDoubleCurve : public CFTimeSource
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
		IModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
		string desc = ar.isEdit() ? GetSourceDescription() : "";
		Serialization::SContext _splineContext(ar, desc.data());
		ar(m_spline, "DoubleCurve", "Double Curve");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		CDomain::Dispatch<CModDoubleCurve>(runtime, range, stream, domain);
	}

	template<typename TimeKernel>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& container = runtime.GetContainer();
		const IFStream unormRands = container.GetIFStream(EPDT_Random);
		const floatv rate = ToFloatv(m_domainScale);
		const floatv offset = ToFloatv(m_domainBias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv unormRand = unormRands.Load(particleGroupId);
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = MAdd(timeKernel.Sample(particleGroupId), rate, offset);
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

SERIALIZATION_CLASS_NAME(IModifier, CModDoubleCurve, "DoubleCurve", "Double Curve");

//////////////////////////////////////////////////////////////////////////
// CModRandom

class CModRandom : public IModifier
{
public:
	CModRandom(float amount = 0.0f)
		: m_amount(amount)
	{}

	virtual EDataDomain GetDomain() const
	{
		return EDD_PerParticle;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		ar(m_amount, "Amount", "Amount");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		SChaosKeyV::Range randRange(1.0f - m_amount, 1.0f);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv value = runtime.ChaosV().Rand(randRange);
			const floatv outvalue = Mul(inValue, value);
			stream.Store(particleGroupId, outvalue);
		}
	}

	virtual Range GetMinMax() const
	{
		return Range(1.0f - m_amount, 1.0f);
	}

private:

	TValue<float, THardLimits<0, 2>> m_amount;
};

SERIALIZATION_CLASS_NAME(IModifier, CModRandom, "Random", "Random");

//////////////////////////////////////////////////////////////////////////
// CModNoise

SERIALIZATION_DECLARE_ENUM(EParamNoiseMode,
                           Smooth,
                           Fractal,
                           Pulse,
                           _Random
                           )

class CModNoise : public CFTimeSource
{
public:
	CModNoise()
		: m_amount(0.0f)
		, m_pulseWidth(0.5f)
		, m_mode(EParamNoiseMode::Smooth) {}

	virtual void AddToParam(CParticleComponent* pComponent)
	{
		CDomain::AddToParam(pComponent);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
		ar(m_mode, "Mode", "Mode");
		ar(m_amount, "Amount", "Amount");
		if (m_mode == EParamNoiseMode::Pulse)
			ar(m_pulseWidth, "PulseWidth", "Pulse Width");

		if (ar.isInput())
			VersionFix(ar);
	}

	virtual IModifier* VersionFixReplace() const
	{
		if (m_mode == EParamNoiseMode::_Random)
			return new CModRandom(m_amount);
		return nullptr;
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		CDomain::Dispatch<CModNoise>(runtime, range, stream, domain);
	}

	template<typename TimeKernel>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		switch (m_mode)
		{
		case EParamNoiseMode::Smooth:
			Modify(runtime, range, stream, timeKernel,
			       [](floatv time){ return Smooth(time); });
			break;
		case EParamNoiseMode::Fractal:
			Modify(runtime, range, stream, timeKernel,
			       [](floatv time){ return Fractal(time); });
			break;
		case EParamNoiseMode::Pulse:
			Modify(runtime, range, stream, timeKernel,
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
	ILINE void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel, ModeFn modeFn) const
	{
		const floatv one = ToFloatv(1.0f);
		const floatv amount = ToFloatv(m_amount);
		const floatv rate = ToFloatv(m_domainScale);
		const floatv offset = ToFloatv(m_domainBias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = MAdd(timeKernel.Sample(particleGroupId), rate, offset);
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

SERIALIZATION_CLASS_NAME(IModifier, CModNoise, "Noise", "Noise");

//////////////////////////////////////////////////////////////////////////
// CModWave

SERIALIZATION_DECLARE_ENUM(EWaveType,
                           Sin,
                           Saw,
                           Pulse
                           )

class CModWave : public CFTimeSource
{
public:
	CModWave()
		: m_waveType(EWaveType::Sin)
		, m_amplitude(1.0f)
		, m_bias(0.5f)
		, m_inverse(false) {}

	virtual void AddToParam(CParticleComponent* pComponent)
	{
		CDomain::AddToParam(pComponent);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
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
		}
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		CDomain::Dispatch<CModWave>(runtime, range, stream, domain);
	}

	template<typename TimeKernel>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		switch (m_waveType)
		{
		case EWaveType::Sin:
			Modify(runtime, range, stream, timeKernel,
			       [](floatv time) { return Sin(time); });
			break;
		case EWaveType::Saw:
			Modify(runtime, range, stream, timeKernel,
			       [](floatv time) { return Saw(time); });
			break;
		case EWaveType::Pulse:
			Modify(runtime, range, stream, timeKernel,
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
	void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel, WaveFn waveFn) const
	{
		const floatv mult = ToFloatv(m_amplitude * (m_inverse ? -1.0f : 1.0f) * 0.5f);
		const floatv bias = ToFloatv(m_bias);
		const floatv rate = ToFloatv(m_domainScale);
		const floatv offset = ToFloatv(m_domainBias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = MAdd(timeKernel.Sample(particleGroupId), rate, offset);
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

SERIALIZATION_CLASS_NAME(IModifier, CModWave, "Wave", "Wave");

//////////////////////////////////////////////////////////////////////////
// CModLinear

class CModLinear : public CFTimeSource
{
public:
	virtual void AddToParam(CParticleComponent* pComponent)
	{
		CDomain::AddToParam(pComponent);
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		CDomain::SerializeInplace(ar);
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;
		CDomain::Dispatch<CModLinear>(runtime, range, stream, domain);
	}

	template<typename TimeKernel>
	void DoModify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, const TimeKernel& timeKernel) const
	{
		const floatv rate = ToFloatv(m_domainScale);
		const floatv offset = ToFloatv(m_domainBias);

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv time = MAdd(timeKernel.Sample(particleGroupId), rate, offset);
			const floatv outvalue = Mul(inValue, time);
			stream.Store(particleGroupId, outvalue);
		}
	}

	virtual Range GetMinMax() const
	{
		// PFX2_TODO: Wrong! Depends on TimeSource range
		return Range(Adjust(0.0f)) | Adjust(1.0f);
	}
};

SERIALIZATION_CLASS_NAME(IModifier, CModLinear, "Linear", "Linear");

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
	{ "PS4",      "Playstation 4",  CONFIG_ORBIS },
};

const uint gNumConfigSpecs = sizeof(gConfigSpecs) / sizeof(gConfigSpecs[0]);

class CModConfigSpec : public IModifier
{
public:
	CModConfigSpec()
		: m_range(1.0f, 1.0f)
		, m_spawnOnly(true)
	{
		for (uint i = 0; i < gNumConfigSpecs; ++i)
			m_specMultipliers[i] = 1.0f;
	}

	virtual EDataDomain GetDomain() const override
	{
		return m_spawnOnly ? EDD_PerParticle : EDD_ParticleUpdate;
	}

	virtual Range GetMinMax() const override
	{
		return m_range;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		IModifier::Serialize(ar);


		for (uint i = 0; i < gNumConfigSpecs; ++i)
			ar(m_specMultipliers[i], gConfigSpecs[i].m_pName, gConfigSpecs[i].m_pLabel);
		m_range = Range(m_specMultipliers[0], m_specMultipliers[0]);
		for (uint i = 1; i < gNumConfigSpecs; ++i)
			m_range = Range(min(m_range.start, m_specMultipliers[1]), max(m_range.end, m_specMultipliers[0]));

		const EDataDomain domain = *ar.context<EDataDomain>();
		if (domain & EDD_PerInstance)
			m_spawnOnly = false;
		else if (!(domain & EDD_HasUpdate))
			m_spawnOnly = true;
		else
			ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const uint particleSpec = runtime.GetEmitter()->GetParticleSpec();
		floatv multiplier = ToFloatv(1.0f);

		for (uint i = 0; i < gNumConfigSpecs; ++i)
		{
			if (gConfigSpecs[i].m_index == particleSpec)
			{
				multiplier = ToFloatv(m_specMultipliers[i]);
				break;
			}
		}

		for (auto particleGroupId : SGroupRange(range))
		{
			const floatv inValue = stream.Load(particleGroupId);
			const floatv outvalue = inValue * multiplier;
			stream.Store(particleGroupId, outvalue);
		}
	}

private:
	float m_specMultipliers[gNumConfigSpecs];
	Range m_range;
	bool  m_spawnOnly;
};

SERIALIZATION_CLASS_NAME(IModifier, CModConfigSpec, "ConfigSpec", "Config Spec");

//////////////////////////////////////////////////////////////////////////
// CModAttribute

class CModAttribute : public IModifier
{
public:
	virtual EDataDomain GetDomain() const override
	{
		return m_spawnOnly ? EDD_PerParticle : EDD_ParticleUpdate;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		IModifier::Serialize(ar);
		ar(m_attribute, "Name", "Attribute Name");
		ar(m_scale, "Scale", "Scale");
		ar(m_bias, "Bias", "Bias");
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
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

	virtual Range GetMinMax() const override
	{
		// PFX2_TODO: Wrong! Depends on attribute value
		return Range(0.0f, 1.0f);
	}

private:
	CAttributeReference m_attribute;
	SFloat              m_scale     = 1;
	SFloat              m_bias      = 0;
	bool                m_spawnOnly = false;
};

SERIALIZATION_CLASS_NAME(IModifier, CModAttribute, "Attribute", "Attribute");

//////////////////////////////////////////////////////////////////////////
// Copy factory creators from base class to derived class
template<typename BaseType, typename Type>
struct ClassFactoryInheritor
{
	using BaseFactory = Serialization::ClassFactory<BaseType>;
	using TypeFactory = Serialization::ClassFactory<Type>;

	ClassFactoryInheritor()
	{
		auto creators = BaseFactory::the().creatorChain();
		TypeFactory::the().registerChain(reinterpret_cast<typename TypeFactory::CreatorBase*>(creators));
	}
};

#define SERIALIZATION_INHERIT_CREATORS(BaseType, Type) \
	static ClassFactoryInheritor<BaseType, Type> ClassFactoryInherit_ ## Type;

//////////////////////////////////////////////////////////////////////////
SERIALIZATION_INHERIT_CREATORS(IModifier, IFieldModifier);

//////////////////////////////////////////////////////////////////////////
// CModInherit

class CModInherit : public IFieldModifier
{
public:
	CModInherit()
		: m_spawnOnly(true) {}

	virtual EDataDomain GetDomain() const
	{
		return m_spawnOnly ? EDD_PerParticle : EDD_ParticleUpdate;
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		IModifier::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		if (!parentContainer.HasData(streamType))
			return;
		IPidStream parentIds = runtime.GetContainer().GetIPidStream(EPDT_ParentId);
		IFStream parentStream = parentContainer.GetIFStream(streamType);

		for (auto particleGroupId : SGroupRange(range))
		{
			const TParticleIdv parentId = parentIds.Load(particleGroupId);
			const floatv input = stream.Load(particleGroupId);
			const floatv parent = parentStream.SafeLoad(parentId);
			const floatv output = Mul(input, parent);
			stream.Store(particleGroupId, output);
		}
	}

	virtual Range GetMinMax() const
	{
		// PFX2_TODO: Wrong! Depends on inherited value
		return Range(0.0f, 1.0f);
	}
private:
	bool m_spawnOnly;
};

SERIALIZATION_CLASS_NAME(IFieldModifier, CModInherit, "Inherit", "Inherit");


}
