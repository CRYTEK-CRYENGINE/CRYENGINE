// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     02/10/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GpuParticleFeatureSpawn.h"
#include "GpuParticleComponentRuntime.h"

namespace {
const float fHUGE = 1e9f;
}

namespace gpu_pfx2
{

// #PFX2_TODO try to merge this code with CPU features, no need for duplication
//	Trouble is, there is no data share between CPU and GPU and so there is no CPU
//	instance information

bool CFeatureSpawnBase::CanSpawnParticles(CParticleComponentRuntime* pRuntime) const
{
	const auto& params = GetParameters();
	const IParticleEmitter* pEmitter = pRuntime->GetEmitter();
	const bool isEmitterIndependent = pEmitter->Unique();
	const bool isSecondGen = pRuntime->IsSecondGen();
	const bool isContinuous = !params.useDuration || params.useRestart;
	if (isContinuous && isEmitterIndependent && !isSecondGen)
		return false;

	return true;
}

void CFeatureSpawnBase::InitSubInstance(IParticleComponentRuntime* pRuntime, SSpawnData* pInstances, size_t count)
{
	const auto& params = GetParameters();
	auto pGpuRuntime = static_cast<CParticleComponentRuntime*>(pRuntime);
	for (size_t i = 0; i < count; ++i)
		pInstances[i] = SSpawnData();
}

void CFeatureSpawnCount::SpawnParticles(const gpu_pfx2::SUpdateContext& context)
{
	const auto& params = GetParameters();
	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*)(context.pRuntime);
	if (!CanSpawnParticles(pRuntime))
		return;

	const size_t numInstances = pRuntime->GetNumInstances();

	const float amount = params.amount;
	const float duration = params.useDuration ? params.duration : 0.0f;

	for (size_t i = 0; i < numInstances; ++i)
	{
		SSpawnData& spawn = pRuntime->GetSpawnData(i);

		const int spawnedBefore = int(spawn.spawned);
		const float spawning = spawn.timer >= params.delay ? 1.0f : 0.0f;
		const float rate = amount * __fres(max(FLT_EPSILON, duration)) * spawning;
		spawn.spawned = min(amount, spawn.spawned + rate * context.deltaTime);
		spawn.timer += context.deltaTime;
		const int spawnedAfter = max(spawnedBefore, int(spawn.spawned));
		pRuntime->SpawnParticles(i, spawnedAfter - spawnedBefore);
	}

	if (params.useRestart)
	{
		for (size_t i = 0; i < pRuntime->GetNumInstances(); ++i)
		{
			SSpawnData& spawn = pRuntime->GetSpawnData(i);
			if (spawn.timer > params.restart)
				spawn = SSpawnData();
		}
	}
}

void CFeatureSpawnRate::InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p)
{
	if (type == EParameterType::SpawnMode)
	{
		const SFeatureParametersSpawnMode& parameters = p.GetParameters<SFeatureParametersSpawnMode>();
		m_mode = parameters.mode;
	}
	else
		CFeatureSpawnBase::InternalSetParameters(type, p);
}

void CFeatureSpawnRate::SpawnParticles(const gpu_pfx2::SUpdateContext& context)
{
	const auto& params = GetParameters();
	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*)(context.pRuntime);
	if (!CanSpawnParticles(pRuntime))
		return;

	const size_t numInstances = pRuntime->GetNumInstances();

	const float amount = params.amount;
	const float duration = params.useDuration ? params.duration : fHUGE;

	for (size_t i = 0; i < pRuntime->GetNumInstances(); ++i)
	{
		SSpawnData& spawn = pRuntime->GetSpawnData(i);
		const int spawnedBefore = int(spawn.spawned);
		const float endTime = params.delay + duration;
		const float spawning = spawn.timer >= params.delay && spawn.timer < endTime ? 1.0f : 0.0f;
		const float rate = (m_mode == ESpawnRateMode::ParticlesPerSecond ? amount : __fres(amount)) * spawning;
		spawn.spawned += rate * context.deltaTime;
		spawn.timer += context.deltaTime;
		const int spawnedAfter = max(spawnedBefore, int(spawn.spawned));
		pRuntime->SpawnParticles(i, spawnedAfter - spawnedBefore);
	}

	if (params.useRestart)
	{
		for (size_t i = 0; i < pRuntime->GetNumInstances(); ++i)
		{
			SSpawnData& spawn = pRuntime->GetSpawnData(i);
			if (spawn.timer > params.restart)
				spawn = SSpawnData();
		}
	}
}

}
