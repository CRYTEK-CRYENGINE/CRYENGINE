// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/ZLib/IZLibCompressor.h>
#include "CryExtension/CryCreateClassInstance.h"
#include <CryString/StringUtils.h>
#include <CrySerialization/SmartPtr.h>

namespace pfx2
{

CRYREGISTER_SINGLETON_CLASS(CParticleSystem)

CParticleSystem::CParticleSystem()
	: m_threadData(gEnv->pJobManager->GetNumWorkerThreads() + 2) // 1 for main thread, 1 for each job thread, 1 for sum stats
{
	if (gEnv && gEnv->pRenderer)
		gEnv->pRenderer->RegisterSyncWithMainListener(this);
}

CParticleSystem::~CParticleSystem()
{
	if (gEnv && gEnv->pRenderer)
		gEnv->pRenderer->RemoveSyncWithMainListener(this);
}

PParticleEffect CParticleSystem::CreateEffect()
{
	return new CParticleEffect();
}

void CParticleSystem::RenameEffect(PParticleEffect pEffect, cstr name)
{
	CRY_PFX2_ASSERT(pEffect.get());
	CParticleEffect* pCEffect = static_cast<CParticleEffect*>(+pEffect);

	if (pCEffect->GetName())
		m_effects[pCEffect->GetName()] = nullptr;
	pCEffect->SetName(name);
	m_effects[pCEffect->GetName()] = pCEffect;
}

PParticleEffect CParticleSystem::FindEffect(cstr name, bool bAllowLoad)
{
	auto it = m_effects.find(name);
	if (it != m_effects.end())
		return it->second;
	if (bAllowLoad)
		return LoadEffect(name);
	return nullptr;
}

PParticleEmitter CParticleSystem::CreateEmitter(PParticleEffect pEffect)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "pfx2::ParticleEmitter");
	PARTICLE_LIGHT_PROFILER();

	CRY_PFX2_ASSERT(pEffect.get());
	CParticleEffect* pCEffect = static_cast<CParticleEffect*>(+pEffect);
	pCEffect->Update();

	m_newEmitters.emplace_back(new CParticleEmitter(pCEffect, m_nextEmitterId++));
	return m_newEmitters.back();
}

TParticleAttributesPtr CParticleSystem::CreateParticleAttributes() const
{
	return TParticleAttributesPtr(new CAttributeInstance());
}

void CParticleSystem::OnFrameStart()
{
}

void CParticleSystem::TrimEmitters(bool finished_only)
{
	stl::find_and_erase_all_if(m_emitters, [=](const _smart_ptr<CParticleEmitter>& emitter) -> bool
	{
		if (!emitter->IsIndependent())
			return false;
		if (finished_only)
		{
			if (emitter->IsAlive())
				return false;
		}
		return true;
	});
}

void CParticleSystem::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	if (!GetCVars()->e_Particles)
		return;

	m_numFrames++;

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

	auto gpuMan = gEnv->pRenderer->GetGpuParticleManager();
	gpuMan->BeginFrame();

	// Accumulate thread stats from last frame
	auto& sumData = GetSumData();
	sumData.statsCPU = {};
	sumData.statsGPU = {};
	sumData.statsSync = {};
	for (auto& data : m_threadData)
	{
		CRY_PFX2_ASSERT(data.memHeap.GetTotalMemory().nUsed == 0);  // some emitter leaked memory on mem stack
		if (&data != &sumData)
		{
			sumData.statsCPU += data.statsCPU;
			data.statsCPU = {};
			sumData.statsGPU += data.statsGPU;
			data.statsGPU = {};
			sumData.statsSync += data.statsSync;
			data.statsSync = {};
		}
	}

	m_profiler.Display();

	TrimEmitters(!m_bResetEmitters);
	m_bResetEmitters = false;
	m_emitters.append(m_newEmitters);
	m_newEmitters.clear();

	// Detect changed sys spec
	const ESystemConfigSpec sysSpec = gEnv->pSystem->GetConfigSpec();
	const bool sys_spec_changed = m_lastSysSpec != END_CONFIG_SPEC_ENUM && m_lastSysSpec != sysSpec;
	m_lastSysSpec = sysSpec;

	// Init stats for current frame
	auto& mainData = GetMainData();
	mainData.statsCPU.emitters.alloc = m_emitters.size();

	for (auto& pEmitter : m_emitters)
	{
		if (sys_spec_changed)
			pEmitter->ResetRenderObjects();
		pEmitter->CheckUpdated();

		mainData.statsCPU.components.alloc += pEmitter->GetRuntimes().size();
		if (pEmitter->IsAlive())
		{
			mainData.statsCPU.emitters.alive++;
			pEmitter->Update();
			m_jobManager.AddUpdateEmitter(pEmitter);
		}
		else
		{
			pEmitter->Unregister();
		}
	}

	m_jobManager.ScheduleUpdates();
}

void CParticleSystem::SyncMainWithRender()
{
	if (ThreadMode() >= 1)
		m_jobManager.SynchronizeUpdates();
		
	const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
	m_lastCameraPose = QuatT(camera.GetMatrix());
}

void CParticleSystem::FinishRenderTasks(const SRenderingPassInfo& passInfo)
{
	m_jobManager.DeferredRender();
	DebugParticleSystem(m_emitters);

	const bool debugBBox = (GetCVars()->e_ParticlesDebug & AlphaBit('b')) != 0;
	if (debugBBox)
	{
		for (auto& pEmitter : m_emitters)
			pEmitter->DebugRender(passInfo);
	}
}

void DisplayStatsHeader(Vec2& displayLocation, float lineHeight, cstr name)
{
	const char* titleLabel = "%-12s: Rendered /  Updated /    Alive /    Alloc";
	gEnv->p3DEngine->DrawTextRightAligned(
		displayLocation.x, displayLocation.y,
		titleLabel,
		name);
	displayLocation.y += lineHeight;
}

void DisplayElementStats(Vec2& displayLocation, float lineHeight, cstr name, const TElementCounts<float>& stats, int prec = 0)
{
	if (!stats.IsZero())
	{
		const char* emittersLabel = " %-11s: %8.*f / %8.*f / %8.*f / %8.*f";
		gEnv->p3DEngine->DrawTextRightAligned(
			displayLocation.x, displayLocation.y,
			emittersLabel,
			name, prec, stats.rendered, prec, stats.updated, prec, stats.alive, prec, stats.alloc, prec);
		displayLocation.y += lineHeight;
	}
}

template<typename TStats>
void DisplayParticleStats(Vec2& displayLocation, float lineHeight, cstr name, TStats& stats)
{
	DisplayStatsHeader(displayLocation, lineHeight, name);
	DisplayElementStats(displayLocation, lineHeight, "Emitters", stats.emitters);
	DisplayElementStats(displayLocation, lineHeight, "Components", stats.components);
	DisplayElementStats(displayLocation, lineHeight, "Particles", reinterpret_cast<const TElementCounts<float>&>(stats.particles));

	if (!stats.pixels.IsZero())
	{
		auto screens = stats.pixels;
		const float pixelsToScreens = 1.0f / (gEnv->pRenderer->GetWidth() * gEnv->pRenderer->GetHeight());
		screens.alloc = C3DEngine::GetCVars()->e_ParticlesMaxScreenFill;
		screens.alive = 1;
		screens.rendered *= pixelsToScreens;
		screens.updated *= pixelsToScreens;
		DisplayElementStats(displayLocation, lineHeight, "Screens", screens, 3);
	}
}

void CParticleSystem::DisplayStats(Vec2& location, float lineHeight)
{
	float blendTime = GetTimer()->GetCurrTime();
	int blendMode = 0;
	float blendCur = GetTimer()->GetProfileFrameBlending(&blendTime, &blendMode);

	static TParticleStats<float> statsCPUAvg, statsGPUAvg;
	TParticleStats<float> statsCPUCur, statsGPUCur; 
	statsCPUCur.Set(GetSumData().statsCPU);
	statsCPUAvg = Lerp(statsCPUAvg, statsCPUCur, blendCur);
	statsGPUCur.Set(GetSumData().statsGPU);
	statsGPUAvg = Lerp(statsGPUAvg, statsGPUCur, blendCur);

	if (!statsCPUAvg.emitters.IsZero())
		DisplayParticleStats(location, lineHeight, "Wavicle CPU", statsCPUAvg);

	static TElementCounts<float> statsSyncAvg;
	TElementCounts<float> statsSyncCur;
	statsSyncCur.Set(GetSumData().statsSync);
	statsSyncAvg = Lerp(statsSyncAvg, statsSyncCur, blendCur);
	DisplayElementStats(location, lineHeight, "Comp Sync", statsSyncAvg);

	if (!statsGPUAvg.components.IsZero())
		DisplayParticleStats(location, lineHeight, "Wavicle GPU", statsGPUAvg);

	if (m_pPartManager)
	{
		static SParticleCounts countsAvg;
		SParticleCounts counts;
		m_pPartManager->GetCounts(counts);
		countsAvg = Lerp(countsAvg, counts, blendCur);

		if (!countsAvg.emitters.IsZero())
			DisplayParticleStats(location, lineHeight, "Particles V1", countsAvg);
			
		if (GetCVars()->e_ParticlesDebug & AlphaBit('r'))
		{
			gEnv->p3DEngine->DrawTextRightAligned(location.x, location.y += lineHeight,
			                     "Reiter %4.0f, Reject %4.0f, Clip %4.1f, Coll %4.1f / %4.1f",
			                     countsAvg.particles.reiterate, countsAvg.particles.reject, countsAvg.particles.clip,
			                     countsAvg.particles.collideHit, countsAvg.particles.collideTest);
		}
		if (GetCVars()->e_ParticlesDebug & AlphaBits('bx'))
		{
			if (countsAvg.volume.stat + countsAvg.volume.dyn > 0.0f)
			{
				float fDiv = 1.f / (countsAvg.volume.dyn + FLT_MIN);
				gEnv->p3DEngine->DrawTextRightAligned(location.x, location.y += lineHeight,
					"Particle BB vol: Stat %.3g, Stat/Dyn %.2f, Err/Dyn %.3g",
					countsAvg.volume.stat, countsAvg.volume.stat * fDiv, countsAvg.volume.error * fDiv);
			}
		}
	}
}

void CParticleSystem::ClearRenderResources()
{
#if !defined(_RELEASE)
	// All external references need to be released before this point to prevent leaks
	for (const auto& pEmitter : m_emitters)
	{
		CRY_ASSERT_MESSAGE(pEmitter->Unique(), "Emitter %s not unique", pEmitter->GetCEffect()->GetShortName().c_str());
	}
	for (const auto& pEmitter : m_newEmitters)
	{
		CRY_ASSERT_MESSAGE(pEmitter->Unique(), "Emitter %s not unique", pEmitter->GetCEffect()->GetShortName().c_str());
	}
#endif

	m_emitters.clear();
	m_newEmitters.clear();
	m_effects.clear();
	m_numFrames = 0;
	m_numClears++;
}

void CParticleSystem::CheckFileAccess(cstr filename) const
{
	if (IsRuntime() && filename)
		Warning("Particle asset runtime access: %s", filename);
}

float CParticleSystem::GetMaxAngularDensity(const CCamera& camera)
{
	return camera.GetAngularResolution() / max(GetCVars()->e_ParticlesMinDrawPixels, 0.125f) * 2.0f;
}

IMaterial* CParticleSystem::GetFlareMaterial()
{
	if (!m_pFlareMaterial)
	{
		const char* flareMaterialName = "%ENGINE%/EngineAssets/Materials/lens_optics";
		m_pFlareMaterial = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(flareMaterialName);
	}
	return m_pFlareMaterial;
}

void CParticleSystem::Reset()
{
	m_bResetEmitters = true;
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
		CheckFileAccess(effectName);
		PParticleEffect pEffect = CreateEffect();
		RenameEffect(pEffect, effectName);
		if (Serialization::LoadJsonFile(*static_cast<CParticleEffect*>(+pEffect), effectName))
			return pEffect;
	}

	m_effects[effectName] = _smart_ptr<CParticleEffect>();
	return PParticleEffect();
}

const SParticleFeatureParams* CParticleSystem::FindFeatureParam(cstr groupName, cstr featureName)
{
	for (const auto& feature : GetFeatureParams())
		if (!strcmp(feature.m_groupName, groupName) && !strcmp(feature.m_featureName, featureName))
			return &feature;
	return nullptr;
}

const SParticleFeatureParams* CParticleSystem::GetDefaultFeatureParam(EFeatureType type)
{
	for (const auto& feature : GetFeatureParams())
		if (feature.m_defaultForType == type)
			return &feature;
	return nullptr;
}

bool CParticleSystem::SerializeFeatures(IArchive& ar, TParticleFeatures& features, cstr name, cstr label) const
{
	auto size = features.size();
	bool ok = ar(features, name, label);
	if (ar.isInput() && size + features.size() > 0)
		features.m_editVersion++;
	return ok;
}

void CParticleSystem::GetStats(SParticleStats& stats)
{
	stats = GetSumData().statsCPU + GetSumData().statsGPU;
}

void CParticleSystem::GetMemoryUsage(ICrySizer* pSizer) const
{
}

}
