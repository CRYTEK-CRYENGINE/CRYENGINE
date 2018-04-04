// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     06/04/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/ZLib/IZLibCompressor.h>
#include "CryExtension/CryCreateClassInstance.h"
#include <CryString/StringUtils.h>
#include "ParticleSystem.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"

CRY_PFX2_DBG

namespace pfx2
{

CRYREGISTER_SINGLETON_CLASS(CParticleSystem)

std::vector<SParticleFeatureParams> &GetFeatureParams()
{
	static std::vector<SParticleFeatureParams> featureParams;
	return featureParams;
}

CParticleSystem::CParticleSystem()
	: m_memHeap(gEnv->pJobManager->GetNumWorkerThreads() + 1)
	, m_nextEmitterId(0)
	, m_lastCameraPose(IDENTITY)
	, m_lastSysSpec(END_CONFIG_SPEC_ENUM)
{
}

PParticleEffect CParticleSystem::CreateEffect()
{
	return new pfx2::CParticleEffect();
}

void CParticleSystem::RenameEffect(PParticleEffect pEffect, cstr name)
{
	CRY_PFX2_ASSERT(pEffect.get());
	CParticleEffect* pCEffect = CastEffect(pEffect);

	if (pCEffect->GetName())
		m_effects[pCEffect->GetName()] = nullptr;
	pCEffect->SetName(name);
	m_effects[pCEffect->GetName()] = pCEffect;
}

PParticleEffect CParticleSystem::FindEffect(cstr name)
{
	auto it = m_effects.find(name);
	if (it != m_effects.end())
		return it->second;
	return LoadEffect(name);
}

PParticleEmitter CParticleSystem::CreateEmitter(PParticleEffect pEffect)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "pfx2::ParticleEmitter");
	PARTICLE_LIGHT_PROFILER();

	CRY_PFX2_ASSERT(pEffect.get());
	CParticleEffect* pCEffect = CastEffect(pEffect);
	pCEffect->Compile();

	_smart_ptr<CParticleEmitter> pEmitter = new CParticleEmitter(m_nextEmitterId++);
	pEmitter->SetCEffect(pCEffect);
	m_newEmitters.push_back(pEmitter);
	return pEmitter;
}

uint CParticleSystem::GetNumFeatureParams() const
{
	return uint(GetFeatureParams().size());
}

SParticleFeatureParams& CParticleSystem::GetFeatureParam(uint featureIdx) const
{
	return GetFeatureParams()[featureIdx];
}

TParticleAttributesPtr CParticleSystem::CreateParticleAttributes() const
{
	return TParticleAttributesPtr(new CAttributeInstance());
}

void CParticleSystem::OnFrameStart()
{
}

void CParticleSystem::TrimEmitters()
{
	for (auto& pEmitter : m_emitters)
	{
		bool independent = pEmitter->IsIndependent();
		bool hasParticles = pEmitter->HasParticles();
		bool isAlive = pEmitter->IsAlive();
		bool expired = isAlive && independent && !hasParticles;
		if (!expired)
			continue;
		pEmitter = 0;
	}

	m_emitters.erase(
	  std::remove(m_emitters.begin(), m_emitters.end(), _smart_ptr<CParticleEmitter>(0)),
	  m_emitters.end());
}

void CParticleSystem::InvalidateCachedRenderObjects()
{
	// Render objects and PSOs need to be updated when sys_spec changes.
	static ICVar* pSysSpecCVar = gEnv->pConsole->GetCVar("sys_spec");
	if (pSysSpecCVar)
	{
		const int32 sysSpec = pSysSpecCVar->GetIVal();

		CRY_ASSERT(sysSpec != END_CONFIG_SPEC_ENUM);
		const bool bInvalidate = ((m_lastSysSpec != END_CONFIG_SPEC_ENUM) && (m_lastSysSpec != sysSpec));
		m_lastSysSpec = sysSpec;

		if (bInvalidate)
		{
			for (auto& pEmitter : m_emitters)
			{
				pEmitter->Restart();
			}
		}
	}
}

void CParticleSystem::Update()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
	const QuatT currentCameraPose = QuatT(camera.GetMatrix());
	if (m_cameraMotion.q.GetLength() == 0.0f)
	{
		m_cameraMotion = IDENTITY;
	}
	else
	{
		m_cameraMotion.t = currentCameraPose.t - m_lastCameraPose.t;
		m_cameraMotion.q = currentCameraPose.q * m_lastCameraPose.q.GetInverted();
	}

	if (GetCVars()->e_Particles)
	{
		auto gpuMan = gEnv->pRenderer->GetGpuParticleManager();
		gpuMan->BeginFrame();

		for (auto& it : m_memHeap)
			CRY_PFX2_ASSERT(it.GetTotalMemory().nUsed == 0);  // some emitter leaked memory on mem stack
		m_profiler.Display();

		TrimEmitters();
		m_emitters.insert(m_emitters.end(), m_newEmitters.begin(), m_newEmitters.end());
		m_newEmitters.clear();

		InvalidateCachedRenderObjects();

		m_stats = SParticleStats();
		m_stats.m_emittersAlive = m_emitters.size();
		for (auto& pEmitter : m_emitters)
			pEmitter->AccumStats(m_stats);

		for (auto& pEmitter : m_emitters)
		{
			pEmitter->Update();
			m_jobManager.AddEmitter(pEmitter);
		}

		for (auto& pEmitter : m_emitters)
			UpdateGpuRuntimesForEmitter(pEmitter);

		m_jobManager.KernelUpdateAll();
	}
}

void CParticleSystem::SyncronizeUpdateKernels()
{
	FUNCTION_PROFILER_3DENGINE;

	m_jobManager.SynchronizeUpdate();
	for (auto& pEmitter : m_emitters)
		pEmitter->PostUpdate();
	const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
	m_lastCameraPose = QuatT(camera.GetMatrix());
}

void CParticleSystem::DeferredRender()
{
	m_jobManager.DeferredRender();
	DebugParticleSystem(m_emitters);

	CVars* pCVars = static_cast<C3DEngine*>(gEnv->p3DEngine)->GetCVars();
	const bool debugBBox = (pCVars->e_ParticlesDebug & AlphaBit('b')) != 0;
	if (debugBBox)
	{
		for (auto& pEmitter : m_emitters)
			pEmitter->DebugRender();
	}
}

float CParticleSystem::DisplayDebugStats(Vec2 displayLocation, float lineHeight)
{
	const char* titleBar =       "Wavicle Stats - Renderered/  Alive/ Updated";
	const char* particlesLabel =     "Particles -     %6d/ %6d/  %6d";
	const char* emittersLabel  =      "Emitters -     %6d/ %6d/  %6d";
	const char* runtimesLabel  =    "Components -     %6d/ %6d/  %6d";

	Get3DEngine()->DrawTextRightAligned(
		displayLocation.x, displayLocation.y,
		titleBar);
	displayLocation.y += lineHeight;

	Get3DEngine()->DrawTextRightAligned(
		displayLocation.x, displayLocation.y,
		particlesLabel,
		m_stats.m_particlesRendered, m_stats.m_particlesAlive, m_stats.m_particlesUpdated);
	displayLocation.y += lineHeight;

	Get3DEngine()->DrawTextRightAligned(
		displayLocation.x, displayLocation.y,
		emittersLabel,
		m_stats.m_emittersRendererd, m_stats.m_emittersAlive, m_stats.m_emittersUpdated);
	displayLocation.y += lineHeight;

	Get3DEngine()->DrawTextRightAligned(
		displayLocation.x, displayLocation.y,
		runtimesLabel,
		m_stats.m_runtimesRendered, m_stats.m_runtimesAlive, m_stats.m_runtimesUpdated);
	displayLocation.y += lineHeight;

	return displayLocation.y;
}

void CParticleSystem::ClearRenderResources()
{
#if !defined(_RELEASE)
	for (const auto& pEmitter : m_emitters)
	{
		CRY_PFX2_ASSERT(pEmitter->Unique()); // All external references need to be released before this point to prevent leaks
	}
#endif

	m_emitters.clear();

	for (auto it = m_effects.begin(); it != m_effects.end(); )
	{
		if (!it->second || it->second->Unique())
			it = m_effects.erase(it);
		else
			++it;
	}
}

void CParticleSystem::Reset()
{
}

void CParticleSystem::Serialize(TSerialize ser)
{
}

PParticleEffect CParticleSystem::LoadEffect(cstr effectName)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "pfx2::LoadEffect");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_ParticleLibrary, 0, "Particle effect (%s)", effectName);

	if (gEnv->pCryPak->IsFileExist(effectName))
	{
		PParticleEffect pEffect = CreateEffect();
		if (Serialization::LoadJsonFile(*CastEffect(pEffect), effectName))
		{
			RenameEffect(pEffect, effectName);
			return pEffect;
		}
	}

	m_effects[effectName] = _smart_ptr<CParticleEffect>();
	return PParticleEffect();
}

void CParticleSystem::UpdateGpuRuntimesForEmitter(CParticleEmitter* pEmitter)
{
	FUNCTION_PROFILER_3DENGINE

	const auto& runtimeRefs = pEmitter->GetRuntimes();

	for (uint i = 0; i < runtimeRefs.size(); ++i)
	{
		auto pGpuRuntime = runtimeRefs[i].pRuntime->GetGpuRuntime();
		if (!pGpuRuntime)
			continue;
		const bool isActive = pGpuRuntime->IsActive();
		if (isActive)
		{
			gpu_pfx2::SEnvironmentParameters params;
			params.physAccel = pEmitter->GetPhysicsEnv().m_UniformForces.vAccel;
			params.physWind = pEmitter->GetPhysicsEnv().m_UniformForces.vWind;
			pGpuRuntime->SetEnvironmentParameters(params);
			pGpuRuntime->UpdateEmitterData();
		}
	}
}

void CParticleSystem::GetStats(SParticleStats& stats)
{
	stats = m_stats;
}

void CParticleSystem::GetMemoryUsage(ICrySizer* pSizer) const
{
}

ILINE CParticleEffect* CParticleSystem::CastEffect(const PParticleEffect& pEffect) const
{
	return static_cast<CParticleEffect*>(pEffect.get());
}

ILINE CParticleEmitter* CParticleSystem::CastEmitter(const PParticleEmitter& pEmitter) const
{
	return static_cast<CParticleEmitter*>(pEmitter.get());
}

uint GetVersion(Serialization::IArchive& ar)
{
	SSerializationContext* pContext = ar.context<SSerializationContext>();
	if (!pContext)
		return gCurrentVersion;
	return pContext->m_documentVersion;
}

}
