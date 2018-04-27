// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     29/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem/ParticleSystem.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include "ParamMod.h"

namespace pfx2
{

struct SSpawnData
{
	float m_amount;
	float m_spawned;
	float m_duration;
	float m_restart;
	float m_timer;

	float DeltaTime(float dT) const
	{
		const float startTime = max(m_timer, 0.0f);
		const float endTime = min(m_timer + dT, m_duration);
		return endTime - startTime;
	}
};

// PFX2_TODO : not enough self documented code. Refactor!

class CParticleFeatureSpawnBase : public CParticleFeature
{
public:
	virtual EFeatureType GetFeatureType() override
	{
		return EFT_Spawn;
	}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitSubInstances.add(this);
		pComponent->SpawnParticles.add(this);
		m_offsetSpawnData = pComponent->AddInstanceData<SSpawnData>();
		m_amount.AddToComponent(pComponent, this);
		m_delay.AddToComponent(pComponent, this);
		m_duration.AddToComponent(pComponent, this);
		m_restart.AddToComponent(pComponent, this);

		// Compute equilibrium and full life times
		CParticleComponent* pParent = pComponent->GetParentComponent();
		const float parentParticleLife = pParent ? pParent->ComponentParams().m_maxParticleLife : gInfinity;

		const float preDelay = pParams->m_maxTotalLIfe;
		float delay = preDelay + m_delay.GetValueRange().end;
		float stableTime = FiniteOr(pParams->m_maxParticleLife, 0.0f);
		float equilibriumTime = delay + stableTime;
		float emitterLife = min(delay + m_duration.GetValueRange().end, parentParticleLife);
		float maxLife = emitterLife + pParams->m_maxParticleLife;

		if (m_restart.IsEnabled())
		{
			stableTime = maxLife;
			equilibriumTime = maxLife;
			maxLife = parentParticleLife + pParams->m_maxParticleLife;
		}

		SetMax(pParams->m_stableTime, stableTime);
		SetMax(pParams->m_equilibriumTime, equilibriumTime);
		SetMax(pParams->m_maxTotalLIfe, maxLife);
	}

	virtual void InitSubInstances(const SUpdateContext& context, SUpdateRange instanceRange) override
	{
		StartInstances(context, instanceRange, {});
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_amount, "Amount", "Amount");

		SerializeEnabled(ar, m_delay, "Delay", 9, 0.0f);
		SerializeEnabled(ar, m_duration, "Duration", 11, DefaultDuration());
		if (m_duration.IsEnabled())
			SerializeEnabled(ar, m_restart, "Restart", 11, gInfinity);
	}

	virtual float DefaultDuration() const { return gInfinity; }

protected:

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
					typedef CParamMod<SModInstanceTimer, UFloat> TTimeParam;

					bool state = false;
					ar(state, "State", "^");
					if (state)
						ar(reinterpret_cast<TTimeParam&>(m_value), "Value", "^");
				}
			};
			param = disabledValue;
			ar(SEnabledValue(param), name, name);
		}
		else
			ar(param, name, name);
	}

	// Convert amounts to spawn counts for frame
	virtual void GetSpawnCounts(const SUpdateContext& context, TVarArray<float> amounts) const = 0;
	
	virtual void SpawnParticles(const SUpdateContext& context, TDynArray<SSpawnEntry>& spawnEntries) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleComponentRuntime& runtime = context.m_runtime;
		const uint numInstances = runtime.GetNumInstances();
		if (numInstances == 0)
			return;

		const CParticleEmitter* pEmitter = context.m_runtime.GetEmitter();
		const bool isIndependent = runtime.GetEmitter()->IsIndependent() && !runtime.IsChild();
		if (isIndependent)
		{
			// Skip spawning immortal independent effects
			float maxLifetime = m_delay.GetValueRange().end + m_duration.GetValueRange().end + context.m_params.m_maxParticleLife;
			if (!std::isfinite(maxLifetime))
				return;
		}
		else if (m_restart.IsEnabled())
		{
			// Skip restarts on independent effects
			THeapArray<uint> indicesArray(*context.m_pMemHeap);
			indicesArray.reserve(numInstances);

			for (uint i = 0; i < numInstances; ++i)
			{
				SSpawnData& spawnData = runtime.GetInstanceData(i, m_offsetSpawnData);
				if (std::isfinite(spawnData.m_restart))
					runtime.SetAlive();
				spawnData.m_restart -= context.m_deltaTime;
				if (spawnData.m_restart <= 0.0f)
					indicesArray.push_back(i);
			}

			StartInstances(context, SUpdateRange(), indicesArray);
		}

		float countScale = context.m_params.m_scaleParticleCount;
		if (!runtime.IsChild())
			countScale *= runtime.GetEmitter()->GetSpawnParams().fCountScale;
		const float dT = context.m_deltaTime;
		SUpdateRange range(0, numInstances);

		TFloatArray amounts(*context.m_pMemHeap, numInstances);
		IOFStream amountStream(amounts.data());
		for (uint i = 0; i < numInstances; ++i)
		{
			SSpawnData& spawnData = runtime.GetInstanceData(i, m_offsetSpawnData);
			amounts[i] = spawnData.m_amount;
		}

		// Pad end of array, for vectorized modifiers
		std::fill(amounts.end(), amounts.begin() + amounts.capacity(), 0.0f);
		m_amount.ModifyUpdate(context, amountStream, range);

		GetSpawnCounts(context, amounts);

		for (uint i = 0; i < numInstances; ++i)
		{
			SSpawnData& spawnData = runtime.GetInstanceData(i, m_offsetSpawnData);

			const float startTime = max(spawnData.m_timer, 0.0f);
			const float endTime = min(spawnData.m_timer + dT, spawnData.m_duration);
			const float spawnTime = endTime - startTime;
			const float spawned = amounts[i] * countScale;
			
			if (spawnData.m_timer <= spawnData.m_duration)
				runtime.SetAlive();

			if (spawnTime >= 0.0f && spawned > 0.0f)
			{
				SSpawnEntry entry = {};
				entry.m_count = uint32(ceil(spawnData.m_spawned + spawned) - ceil(spawnData.m_spawned));
				if (entry.m_count)
				{
					// Set parameters for absolute particle age and spawn fraction
					entry.m_parentId = runtime.GetParentId(i);
					entry.m_ageIncrement = -spawnTime * rcp(spawned);
					entry.m_ageBegin = dT + min(spawnData.m_timer, 0.0f);
					entry.m_ageBegin += (ceil(spawnData.m_spawned) - spawnData.m_spawned) * entry.m_ageIncrement;

					if (std::isfinite(spawnData.m_duration))
					{
						entry.m_fractionIncrement = rcp((float)entry.m_count);
						if (spawnData.m_duration > 0.0f)
						{
							const float invDuration = rcp(spawnData.m_duration);
							entry.m_fractionBegin = startTime * invDuration;
							const float fractionEnd = endTime * invDuration;
							entry.m_fractionIncrement *= (fractionEnd - entry.m_fractionBegin);
						}
					}

					spawnEntries.push_back(entry);
				}
				spawnData.m_spawned += spawned;
			}

			spawnData.m_timer += dT;
		}
	}

	void StartInstances(const SUpdateContext& context, SUpdateRange instanceRange, TConstArray<uint> instanceIndices)
	{
		CRY_PFX2_PROFILE_DETAIL;

		const uint numStarts = instanceRange.size() + instanceIndices.size();
		if (numStarts == 0)
			return;

		CParticleComponentRuntime& runtime = context.m_runtime;
		SUpdateRange startRange(0, numStarts);

		STempInitBuffer<float> amounts(context, m_amount, startRange);
		STempInitBuffer<float> delays(context, m_delay, startRange);
		STempInitBuffer<float> durations(context, m_duration, startRange);
		STempInitBuffer<float> restarts(context, m_restart, startRange);

		for (uint i = 0; i < numStarts; ++i)
		{
			const uint idx = i < instanceRange.size() ?
				instanceRange.m_begin + i
				: instanceIndices[i - instanceRange.size()];
			SSpawnData& spawnData = runtime.GetInstanceData(idx, m_offsetSpawnData);
			const float delay = delays[i] + runtime.GetInstance(idx).m_startDelay;

			spawnData.m_timer    = -delay;
			spawnData.m_spawned  = 0.0f;
			spawnData.m_amount   = amounts[i];
			spawnData.m_duration = durations[i];
			spawnData.m_restart  = max(restarts[i], delay + spawnData.m_duration);
		}
	}

protected:

	CParamMod<SModInstanceCounter, UFloat>  m_amount   = 1;
	CParamMod<SModInstanceTimer, UFloat>    m_delay    = 0;
	CParamMod<SModInstanceTimer, UInfFloat> m_duration = gInfinity;
	CParamMod<SModInstanceTimer, PInfFloat> m_restart  = gInfinity;

	TDataOffset<SSpawnData>                 m_offsetSpawnData;
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
		if (m_duration.GetBaseValue())
			ar(m_mode, "Mode", "Mode");
	}

	virtual float DefaultDuration() const override { return 0.0f; }

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CParticleFeatureSpawnBase::AddToComponent(pComponent, pParams);

		const float amount = m_amount.GetValueRange().end;
		const float spawnTime = m_mode == ESpawnCountMode::TotalParticles ? m_duration.GetValueRange().start
			: min(pParams->m_maxParticleLife, m_duration.GetValueRange().start);
		if (spawnTime > 0.0f)
			pParams->m_maxParticleRate += amount / spawnTime;
		else
			pParams->m_maxParticlesBurst += int_ceil(amount);
	}

	void GetSpawnCounts(const SUpdateContext& context, TVarArray<float> amounts) const override
	{
		for (uint i = 0; i < amounts.size(); ++i)
		{
			SSpawnData& spawnData = context.m_runtime.GetInstanceData(i, m_offsetSpawnData);
			const float dt = spawnData.DeltaTime(context.m_deltaTime);
			if (dt < 0.0f)
				continue;
			if (m_mode == ESpawnCountMode::TotalParticles || spawnData.m_duration <= context.m_params.m_maxParticleLife)
			{
				if (spawnData.m_duration > dt)
					amounts[i] *= dt * rcp(spawnData.m_duration);
				else
					amounts[i] -= spawnData.m_spawned;
			}
			else
			{
				amounts[i] *= dt * rcp(context.m_params.m_maxParticleLife);
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

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CParticleFeatureSpawnBase::AddToComponent(pComponent, pParams);

		const auto amount = m_amount.GetValueRange();
		if (m_mode == ESpawnRateMode::ParticlesPerFrame)
			pParams->m_maxParticlesPerFrame += int_ceil(amount.end);
		else
			pParams->m_maxParticleRate += (m_mode == ESpawnRateMode::ParticlesPerSecond ? amount.end : rcp(amount.start));
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeatureSpawnBase::Serialize(ar);
		ar(m_mode, "Mode", "Mode");
	}

	void GetSpawnCounts(const SUpdateContext& context, TVarArray<float> amounts) const override
	{
		for (uint i = 0; i < amounts.size(); ++i)
		{
			SSpawnData& spawnData = context.m_runtime.GetInstanceData(i, m_offsetSpawnData);
			const float dt = spawnData.DeltaTime(context.m_deltaTime);
			if (dt < 0.0f)
				continue;
			amounts[i] =
				m_mode == ESpawnRateMode::ParticlesPerFrame ? amounts[i]
				: dt * (m_mode == ESpawnRateMode::ParticlesPerSecond ? amounts[i] : rcp(amounts[i]));
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
		m_offsetEmitPos = pComponent->AddInstanceData<Vec3>();
	}
	virtual void InitSubInstances(const SUpdateContext& context, SUpdateRange instanceRange) override
	{
		CParticleFeatureSpawnBase::InitSubInstances(context, instanceRange);

		THeapArray<QuatTS> locations(*context.m_pMemHeap, instanceRange.size());
		context.m_runtime.GetEmitLocations(locations);

		for (auto instanceId : instanceRange)
		{
			Vec3& emitPos = context.m_runtime.GetInstanceData(instanceId, m_offsetEmitPos);
			emitPos = locations[instanceId].t;
		}
	}

	void GetSpawnCounts(const SUpdateContext& context, TVarArray<float> amounts) const override
	{
		THeapArray<QuatTS> locations(*context.m_pMemHeap, amounts.size());
		context.m_runtime.GetEmitLocations(locations);

		for (uint i = 0; i < amounts.size(); ++i)
		{
			Vec3& emitPos = context.m_runtime.GetInstanceData(i, m_offsetEmitPos);
			const Vec3 emitPos0 = emitPos;
			const Vec3 emitPos1 = locations[i].t;
			emitPos = emitPos1;

			const float distance = (emitPos1 - emitPos0).GetLengthFast();
			amounts[i] = distance * (m_mode == ESpawnDistanceMode::ParticlesPerMeter ? amounts[i] : rcp(amounts[i]));
		}
	}

private:
	ESpawnDistanceMode m_mode = ESpawnDistanceMode::ParticlesPerMeter;
	TDataOffset<Vec3>  m_offsetEmitPos;

};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureSpawnDistance, "Spawn", "Distance", colorSpawn);

class CFeatureSpawnDensity : public CFeatureSpawnCount
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeatureSpawnBase::Serialize(ar);
	}

	void GetSpawnCounts(const SUpdateContext& context, TVarArray<float> amounts) const override
	{
		TFloatArray extents(*context.m_pMemHeap, amounts.size());
		extents.fill(0.0f);
		context.m_runtime.GetComponent()->GetSpatialExtents(context, amounts, extents);
		amounts.copy(0, extents);

		CFeatureSpawnCount::GetSpawnCounts(context, amounts);
	}
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureSpawnDensity, "Spawn", "Density", colorSpawn);

}
