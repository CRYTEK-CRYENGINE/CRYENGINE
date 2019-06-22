// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "../ParticleEmitter.h"

namespace pfx2
{

MakeDataType(ESDT_ParentId, uint,  EDD_Spawner);

MakeDataType(ESDT_Amount,   float, EDD_SpawnerUpdate);

MakeDataType(ESDT_Age,      float, EDD_Spawner);
MakeDataType(ESDT_LifeTime, float, EDD_Spawner);
MakeDataType(ESDT_Restart,  float, EDD_Spawner);
MakeDataType(ESDT_Spawned,  float, EDD_Spawner);
MakeDataType(ESDT_Fraction, float, EDD_Spawner);

Range SpawnTimes(const CParticleComponentRuntime& runtime, float age, float lifetime)
{
	float dT = runtime.DeltaTime();
	const float ageStart = age - dT;
	const float endTime = min(dT, lifetime - ageStart);
	const float startTime = max(max(0.0f, -ageStart), endTime - runtime.ComponentParams().m_stableTime);
	return Range(startTime, endTime);
}

MakeDataType(ESDT_ParticleCounts, SMaxParticleCounts, EDD_None);

class CParticleFeatureSpawnBase : public CParticleFeature
{
public:
	virtual EFeatureType GetFeatureType() override { return EFT_Spawn; }

	// Initialize after LifeTime and Child features, for timing
	virtual int Priority() const override { return 1; }

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->UpdateSpawners.add(this);
		pComponent->InitSpawners.add(this);
		pComponent->SpawnParticles.add(this);
		pComponent->GetDynamicData.add(this);
		pComponent->OnEdit.add(this);
		
		m_amount.AddToComponent(pComponent, this, ESDT_Amount);
		m_duration.AddToComponent(pComponent, this, ESDT_LifeTime);
		m_delay.AddToComponent(pComponent, this, ESDT_Age);
		if (m_restart.IsEnabled())
			pComponent->AddParticleData(ESDT_Restart);

		pComponent->AddParticleData(ESDT_Spawned);
		if (m_duration.IsEnabled())
			pComponent->AddParticleData(ESDT_Fraction);
		pComponent->AddParticleData(ESDT_ParentId);

		// Compute equilibrium and full life times
		CParticleComponent* pParent = pComponent->GetParentComponent();
		const float parentParticleLife = pParent ? pParent->ComponentParams().m_maxParticleLife : gInfinity;
		const float preDelay = pParams->m_maxTotalLife;
		const float delay = preDelay + m_delay.GetValueRange().end;

		STimingParams timings;
		timings.m_equilibriumTime = delay;
		timings.m_maxTotalLife = min(delay + m_duration.GetValueRange().end, parentParticleLife);

		timings.m_maxTotalLife += pParams->m_maxParticleLife;
		if (valueisfinite(pParams->m_maxParticleLife))
		{
			timings.m_equilibriumTime += pParams->m_maxParticleLife;
			timings.m_stableTime += pParams->m_maxParticleLife;
		}

		if (m_restart.IsEnabled())
		{
			timings.m_stableTime = timings.m_maxTotalLife;
			timings.m_equilibriumTime = timings.m_maxTotalLife;
			timings.m_maxTotalLife = parentParticleLife;
		}
		SetMax(pParams->m_stableTime, timings.m_stableTime);
		SetMax(pParams->m_equilibriumTime, timings.m_equilibriumTime);
		SetMax(pParams->m_maxTotalLife, timings.m_maxTotalLife);

		if (m_duration.GetBaseValue() == 0.0f)
		{
			// Particles all have same age, add Spawn fraction or Id to distinguish them
			pComponent->AddParticleData(EPDT_SpawnFraction);
			if (m_restart.IsEnabled())
				pComponent->AddParticleData(EPDT_SpawnId);
		}
	}

	void UpdateSpawners(CParticleComponentRuntime& runtime) override
	{
		// Add first spawner if none ever added
		auto& container = runtime.Container(EDD_Spawner);

		if (!runtime.IsChild() && runtime.GetEmitter()->IsActive() && container.TotalSpawned() == 0)
		{
			SSpawnerDesc spawner;
			TVarArray<SSpawnerDesc> spawners(&spawner, 1);
			runtime.AddSpawners(spawners);
			return;
		}

		SUpdateRange range = container.NonSpawnedRange();
		if (range.size())
		{
			// Disable restarts for independent top spawners
			const bool isIndependent = runtime.GetEmitter()->IsIndependent() && !runtime.IsChild();
			const bool doRestart = m_restart.IsEnabled() && !isIndependent;

			TParticleIdArray removeIds(runtime.MemHeap());
			removeIds.reserve(range.size());

			THeapArray<SSpawnerDesc> spawners(runtime.MemHeap());
			if (doRestart)
				spawners.reserve(range.size());

			auto ages = runtime.IOStream(ESDT_Age);
			auto lifetimes = runtime.IStream(ESDT_LifeTime);
			auto restarts = runtime.IStream(ESDT_Restart);
			auto parentIds = runtime.IStream(ESDT_ParentId);
			const float dT = runtime.DeltaTime();

			for (auto i : range)
			{
				if (ages[i] > lifetimes[i])
				{
					removeIds.push_back(i);
 					if (doRestart)
 						spawners.push_back(SSpawnerDesc(parentIds[i], max(restarts[i] - ages[i], 0.0f)));
				}
				ages[i] += dT;
			}

			container.RemoveElements(removeIds);
			runtime.AddSpawners(spawners, false);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_amount, "Amount", "Amount");

		SerializeEnabled(ar, m_delay, "Delay", 9, 0.0f);
		SerializeEnabled(ar, m_duration, "Duration", 11, DefaultDuration());
		if (m_duration.IsEnabled())
		{
			SerializeEnabled(ar, m_restart, "Restart", 11, gInfinity);
			if (ar.isInput() && m_restart.GetBaseValue() == 0.0f)
				m_restart = gInfinity;
		}
	}

	virtual void OnEdit(CParticleComponentRuntime& runtime) override
	{
		// Re-init spawners
		runtime.RemoveAllSpawners();
	}

	virtual float DefaultDuration() const { return gInfinity; }

protected:

	static float CountScale(const CParticleComponentRuntime& runtime)
	{
		float scale = runtime.ComponentParams().m_scaleParticleCount;
		if (!runtime.IsChild())
			scale *= runtime.GetEmitter()->GetSpawnParams().fCountScale;
		return scale;
	}

	template<typename TParam>
	void SerializeEnabled(Serialization::IArchive& ar, TParam& param, cstr name, uint newVersion, float disabledValue)
	{
		if (ar.isInput() && GetVersion(ar) < newVersion)
		{
			struct SEnabledValue
			{
				SEnabledValue(TParam& value)
					: m_value(value) {}
				TParam& m_value;
			
				void Serialize(Serialization::IArchive& ar)
				{
					typedef CParamMod<EDD_Spawner, UFloat> TTimeParam;

					bool state = false;
					ar(state, "State", "^");
					if (state)
						ar(reinterpret_cast<TTimeParam&>(m_value), "Value", "^");
				}
			};
			param = disabledValue;
			ar(SEnabledValue(param), name, name);
			return;
		}
		if (ar.isInput() && GetVersion(ar) < 12)
			param = TParam::TValue::Default();
		ar(param, name, name);
	}

	// Convert amounts to spawn counts for frame
	virtual void GetSpawnCounts(CParticleComponentRuntime& runtime, TVarArray<float> counts) const = 0;

	virtual void SpawnParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		SUpdateRange range = runtime.FullRange(EDD_Spawner);

		const float dT = runtime.DeltaTime();
		const bool isIndependent = runtime.GetEmitter()->IsIndependent() && !runtime.IsChild();
		if (isIndependent)
		{
			// Skip spawning immortal independent effects
			float maxLifetime = m_delay.GetValueRange().end + m_duration.GetValueRange().end + runtime.ComponentParams().m_maxParticleLife;
			if (!valueisfinite(maxLifetime))
				range = SUpdateRange();
		}

		float countScale = CountScale(runtime);

		m_amount.Update(runtime, ESDT_Amount);

		THeapArray<float> counts(runtime.MemHeap(), range.size());
		GetSpawnCounts(runtime, counts);

		THeapArray<SSpawnEntry> spawnEntries(runtime.MemHeap());
		spawnEntries.reserve(range.size());

		auto ages = runtime.IStream(ESDT_Age);
		auto lifetimes = runtime.IStream(ESDT_LifeTime);
		auto parentIds = runtime.IStream(ESDT_ParentId);
		auto spawned = runtime.IOStream(ESDT_Spawned);
		auto fractions = runtime.IOStream(ESDT_Fraction);

		for (auto i : range)
		{
			const Range spawnTimes = SpawnTimes(runtime, ages[i], lifetimes[i]);
			const float spawnTime = spawnTimes.Length();
			const float count = counts[i] * countScale;
			if (spawnTime >= 0.0f && count > 0.0f)
			{
				SSpawnEntry entry = {};
				entry.m_count = uint32(ceil(spawned[i] + count) - ceil(spawned[i]));
				if (entry.m_count)
				{
					// Set parameters for absolute particle age and spawn fraction
					entry.m_parentId = parentIds[i];
					entry.m_ageIncrement = -spawnTime * rcp(count);
					entry.m_ageBegin = dT - spawnTimes.start;
					entry.m_ageBegin += (ceil(spawned[i]) - spawned[i]) * entry.m_ageIncrement;
					if (entry.m_count == 1)
						entry.m_ageIncrement = 0.0f;

					if (valueisfinite(lifetimes[i]))
					{
						float total = count;
						if (lifetimes[i] * spawnTime > 0.0f)
							total *= lifetimes[i] * rcp(spawnTime);
						entry.m_fractionIncrement = total > 1.0f ? rcp(total - 1.0f) : 0.0f;
						entry.m_fractionBegin = fractions[i];
						fractions[i] += entry.m_fractionIncrement * entry.m_count;
					}

					spawnEntries.push_back(entry);
				}
				spawned[i] += count;
			}
		}

		runtime.AddParticles(spawnEntries);
	}

	virtual void InitSpawners(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		auto range = runtime.SpawnedRange(EDD_Spawner);
		if (range.size() == 0)
			return;

		if (m_delay.GetBaseValue() > 0.0f)
		{
			STempInitBuffer<float> delays(runtime, m_delay, range);
			auto ages = runtime.IOStream(ESDT_Age);
			for (auto i : range)
				ages[i] -= delays[i];
		}

		m_amount.Init(runtime, ESDT_Amount, range);
		m_duration.Init(runtime, ESDT_LifeTime, range);
		m_restart.Init(runtime, ESDT_Restart, range);

		runtime.FillData(ESDT_Spawned, 0.0f, range);
		runtime.FillData(ESDT_Fraction, 0.0f, range);
	}

protected:

	CParamMod<EDD_SpawnerUpdate, UFloat> m_amount   = 1;
	CParamMod<EDD_Spawner, UFloat>       m_delay    = 0;
	CParamMod<EDD_Spawner, UInfFloat>    m_duration = gInfinity;
	CParamMod<EDD_Spawner, UInfFloat>    m_restart  = gInfinity;
};

SERIALIZATION_DECLARE_ENUM(ESpawnCountMode,
	MaximumParticles, 
	TotalParticles
)

class CFeatureSpawnCount : public CParticleFeatureSpawnBase
{
public:
	CRY_PFX2_DECLARE_FEATURE
		
	CFeatureSpawnCount()
	{
		m_duration = 0.0f;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeatureSpawnBase::Serialize(ar);
		if (m_duration.IsEnabled() && m_duration.GetBaseValue() > 0.0f)
			ar(m_mode, "Mode", "Mode");
	}

	virtual float DefaultDuration() const override { return 0.0f; }

	virtual void AdjustAmounts(const CParticleComponentRuntime& runtime, TIOStream<float> amounts) const {}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto counts = ESDT_ParticleCounts.Cast(type, data, range))
		{
			SSpawnerUpdateBuffer<float> amounts(runtime, m_amount, domain, true);
			AdjustAmounts(runtime, amounts.RawStream());
			SSpawnerUpdateBuffer<float> durations(runtime, m_duration, domain);
			const float scale = CountScale(runtime);
			for (auto i : range)
			{
				const float amount = amounts[i].end * scale;
				const float spawnTime = m_mode == ESpawnCountMode::TotalParticles ? durations[i].start
					: min(runtime.ComponentParams().m_maxParticleLife, durations[i].start);
				if (spawnTime > 0.0f)
					counts[i].rate += amount / spawnTime;
				else
					counts[i].burst += int_ceil(amount);
			}
		}
	}

	void GetSpawnCounts(CParticleComponentRuntime& runtime, TVarArray<float> counts) const override
	{
		STempUpdateBuffer<float> amounts(runtime, m_amount, runtime.FullRange(EDD_Spawner), true);
		AdjustAmounts(runtime, amounts.RawStream());
		auto ages = runtime.IStream(ESDT_Age);
		auto lifetimes = runtime.IStream(ESDT_LifeTime);
		auto spawned = runtime.IStream(ESDT_Spawned);

		for (uint i = 0; i < counts.size(); ++i)
		{
			const float dT = SpawnTimes(runtime, ages[i], lifetimes[i]).Length();
			if (dT < 0.0f)
				continue;
			if (m_mode == ESpawnCountMode::TotalParticles || lifetimes[i] <= runtime.ComponentParams().m_maxParticleLife)
			{
				if (lifetimes[i] > dT)
					counts[i] = amounts[i] * dT * rcp(lifetimes[i]);
				else
					counts[i] = amounts[i] - spawned[i];
			}
			else
			{
				counts[i] = amounts[i] * dT * rcp(runtime.ComponentParams().m_maxParticleLife);
			}
		}
	}

private:
	ESpawnCountMode m_mode = ESpawnCountMode::MaximumParticles;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureSpawnCount, "Spawn", "Count", colorSpawn);

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(ESpawnRateMode,
	ParticlesPerSecond,
	_SecondPerParticle, SecondsPerParticle = _SecondPerParticle,
	ParticlesPerFrame
)

class CFeatureSpawnRate : public CParticleFeatureSpawnBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	static uint DefaultForType() { return EFT_Spawn; }

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto counts = ESDT_ParticleCounts.Cast(type, data, range))
		{
			SSpawnerUpdateBuffer<float> amounts(runtime, m_amount, domain);
			const float scale = CountScale(runtime);
			for (auto i : range)
			{
				const auto amount = amounts[i] * scale;
				if (m_mode == ESpawnRateMode::ParticlesPerFrame)
					counts[i].perFrame += int_ceil(amount.end);
				else
					counts[i].rate += (m_mode == ESpawnRateMode::ParticlesPerSecond ? amount.end : rcp(amount.start));
			}
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeatureSpawnBase::Serialize(ar);
		ar(m_mode, "Mode", "Mode");
	}

	void GetSpawnCounts(CParticleComponentRuntime& runtime, TVarArray<float> counts) const override
	{
		auto amounts = runtime.IStream(ESDT_Amount);
		auto ages = runtime.IStream(ESDT_Age);
		auto lifetimes = runtime.IStream(ESDT_LifeTime);

		for (uint i = 0; i < counts.size(); ++i)
		{
			const float dT = SpawnTimes(runtime, ages[i], lifetimes[i]).Length();
			if (dT < 0.0f)
				continue;
			counts[i] =
				m_mode == ESpawnRateMode::ParticlesPerFrame ? amounts[i]
				: dT * (m_mode == ESpawnRateMode::ParticlesPerSecond ? amounts[i] : rcp(amounts[i]));
		}
	}

private:
	ESpawnRateMode m_mode = ESpawnRateMode::ParticlesPerSecond;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureSpawnRate, "Spawn", "Rate", colorSpawn);

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(ESpawnDistanceMode,
    ParticlesPerMeter,
    MetersPerParticle
)

MakeDataType(ESVT_EmitPos, Vec3, EDD_Spawner);

class CFeatureSpawnDistance : public CParticleFeatureSpawnBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeatureSpawnBase::Serialize(ar);
		ar(m_mode, "Mode", "Mode");
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CParticleFeatureSpawnBase::AddToComponent(pComponent, pParams);
		pComponent->AddParticleData(ESVT_EmitPos);
	}

	virtual void InitSpawners(CParticleComponentRuntime& runtime) override
	{
		CParticleFeatureSpawnBase::InitSpawners(runtime);

		auto range = runtime.SpawnedRange(EDD_Spawner);
		auto emitPositions = runtime.IOStream(ESVT_EmitPos);

		THeapArray<QuatTS> locations(runtime.MemHeap(), range.size());
		runtime.GetEmitLocations(locations, range.m_begin);

		for (auto i : range)
		{
			emitPositions.Store(i, locations[i - range.m_begin].t);
		}
	}

	void GetSpawnCounts(CParticleComponentRuntime& runtime, TVarArray<float> counts) const override
	{
		auto amounts = runtime.IStream(ESDT_Amount);
		auto emitPositions = runtime.IOStream(ESVT_EmitPos);

		THeapArray<QuatTS> locations(runtime.MemHeap(), counts.size());
		runtime.GetEmitLocations(locations, 0);

		for (uint i = 0; i < counts.size(); ++i)
		{
			const Vec3 emitPos = emitPositions.Load(i);
			const float distance = (locations[i].t - emitPos).GetLengthFast();
			counts[i] = distance * (m_mode == ESpawnDistanceMode::ParticlesPerMeter ? amounts[i] : rcp(amounts[i]));
			emitPositions.Store(i, locations[i].t);
		}
	}

private:
	ESpawnDistanceMode m_mode = ESpawnDistanceMode::ParticlesPerMeter;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureSpawnDistance, "Spawn", "Distance", colorSpawn);

extern TDataType<Vec4> ESDT_SpatialExtents;

class CFeatureSpawnDensity : public CFeatureSpawnCount
{
public:
	CRY_PFX2_DECLARE_FEATURE

	static float ApplyExtents(float amount, Vec4 const& extents)
	{
		return extents[0] + amount * extents[1] + sqr(amount) * extents[2] + cube(amount) * extents[3];
	}

	virtual void AdjustAmounts(const CParticleComponentRuntime& runtime, TIOStream<float> amounts) const override
	{
		auto range = runtime.FullRange(EDD_Spawner);
		SDynamicData<Vec4> extents(runtime, ESDT_SpatialExtents, EDD_Spawner, range);
		for (auto i : range)
		{
			amounts[i] = ApplyExtents(amounts[i], extents[i]);
		}
	}
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureSpawnDensity, "Spawn", "Density", colorSpawn);

}
