// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "ParticleDebug.h"
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
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "pfx2::ParticleEmitter");
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	CRY_PFX2_ASSERT(pEffect.get());
	CParticleEffect* pCEffect = static_cast<CParticleEffect*>(+pEffect);
	pCEffect->Compile();

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
	CRY_PFX2_PROFILE_DETAIL;

	auto doTrim = [=](const _smart_ptr<CParticleEmitter>& emitter) -> bool
	{
		if (!emitter->IsIndependent())
			return false;
		if (finished_only)
		{
			if (emitter->IsAlive())
				return false;
		}
		return true;
	};

	stl::find_and_erase_all_if(m_emitters, doTrim);
	stl::find_and_erase_all_if(m_emittersPreUpdate, doTrim);
}

void CParticleSystem::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();
	MEMSTAT_CONTEXT(EMemStatContextType::ParticleLibrary, "CParticleSystem::Update");

	if (!GetCVars()->e_Particles)
		return;

	m_numFrames++;

	if (auto gpuMan = gEnv->pRenderer->GetGpuParticleManager())
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
	for (auto& pEmitter : m_newEmitters)
	{
		pEmitter->CheckUpdated();
		if (pEmitter->GetRuntimesPreUpdate().size())
			m_emittersPreUpdate.push_back(pEmitter);
	}
	m_newEmitters.clear();

	// Init stats for current frame
	auto& mainData = GetMainData();
	mainData.statsCPU.emitters.alloc = m_emitters.size();

	// Detect changed sys spec
	const ESystemConfigSpec sysSpec = gEnv->pSystem->GetConfigSpec();
	if (m_lastSysSpec != END_CONFIG_SPEC_ENUM && m_lastSysSpec != sysSpec)
	{
		if (GetCVars()->e_ParticlesPrecacheAssets)
		{
			for (auto& elem : m_effects)
			{
				if (auto& pEffect = elem.second)
				{
					pEffect->LoadResources();
				}
			}
		}
		for (auto& pEmitter : m_emitters)
		{
			pEmitter->UpdateRuntimes();
		}
	}
	m_lastSysSpec = sysSpec;

	// Check for edited effects
	if (gEnv->IsEditing())
	{
		m_emittersPreUpdate.clear();
		for (auto& pEmitter : m_emitters)
		{
			pEmitter->CheckUpdated();
			if (pEmitter->GetRuntimesPreUpdate().size())
				m_emittersPreUpdate.push_back(pEmitter);
		}
	}

	// Execute PreUpdates, to update forces, etc
	for (auto& pEmitter : m_emittersPreUpdate)
	{
		if (!pEmitter->IsAlive())
			continue;
		for (auto& pRuntime: pEmitter->GetRuntimesPreUpdate())
		{
			pRuntime->GetComponent()->MainPreUpdate(*pRuntime);
		}
	}

	// Update remaining emitter state
	for (auto& pEmitter : m_emitters)
	{
		mainData.statsCPU.components.alloc += pEmitter->GetRuntimes().size();
		if (pEmitter->UpdateState())
		{
			mainData.statsCPU.emitters.alive++;
			m_jobManager.AddUpdateEmitter(pEmitter);
		}
	}

	m_jobManager.ScheduleUpdates();
}

void CParticleSystem::SyncMainWithRender()
{
	if (ThreadMode() != 0)
		m_jobManager.SynchronizeUpdates();
		
	const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
	m_lastCameraPose = QuatT(camera.GetMatrix());
}

void CParticleSystem::FinishRenderTasks(const SRenderingPassInfo& passInfo)
{
	m_jobManager.DeferredRender();
	DebugParticleSystem(m_emitters);

	if (DebugMode('b'))
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
	DisplayElementStats(displayLocation, lineHeight, "Spawners", stats.spawners);
	DisplayElementStats(displayLocation, lineHeight, "Particles", reinterpret_cast<const TElementCounts<float>&>(stats.particles));

	if (!stats.pixels.IsZero())
	{
		auto screens = stats.pixels;
		const float pixelsToScreens = 1.0f / (gEnv->pRenderer->GetWidth() * gEnv->pRenderer->GetHeight());
		screens.alloc = GetCVars()->e_ParticlesMaxScreenFill;
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
			
		if (DebugMode('r'))
		{
			gEnv->p3DEngine->DrawTextRightAligned(location.x, location.y += lineHeight,
			                     "Reiter %4.0f, Reject %4.0f, Clip %4.1f, Coll %4.1f / %4.1f",
			                     countsAvg.particles.reiterate, countsAvg.particles.reject, countsAvg.particles.clip,
			                     countsAvg.particles.collideHit, countsAvg.particles.collideTest);
		}
		if (DebugMode('bx'))
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
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
#if !defined(_RELEASE)
	// All external references need to be released before this point to prevent leaks
	for (const auto& pEmitter : m_emitters)
	{
		CRY_ASSERT_MESSAGE(pEmitter->Unique(), "Emitter %s not unique", pEmitter->GetCEffect()->GetShortName().c_str());
	}
#endif

	m_emitters.clear();
	m_newEmitters.clear();
	m_emittersPreUpdate.clear();

	for (auto it = m_effects.begin(); it != m_effects.end(); )
	{
		if (auto& pEffect = it->second)
		{
			if (m_numLevelLoads == 0)
			{
				// Preserve all effects loaded at startup
				pEffect->AddRef();
			}
			else if (pEffect->Unique())
			{
				// Remove unreferenced effects loaded last level
				it = m_effects.erase(it);
				continue;
			}
			pEffect->UnloadResources();
		}
		++it;
	}

	m_materials.clear();

	m_numFrames = 0;
	m_numLevelLoads ++;

	for (auto& data : m_threadData)
		data.memHeap.FreeEmptyPages();
}

bool CParticleSystem::IsRuntime() const
{
	return !gEnv->IsEditing() && m_numFrames > 1;
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

IMaterial* CParticleSystem::GetTextureMaterial(cstr textureName, bool gpu, gpu_pfx2::EFacingMode facing)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (!gEnv->pRenderer)
		return nullptr;

	enum EGpuParticlesVertexShaderFlags
	{
		FacingVelocity = 0x2000
	};

	string materialName = string(textureName);
	cstr shaderName = "Particles";
	uint32 shaderMask = 0;
	if (gpu)
	{
		materialName += ":GPU";
		shaderName = "Particles.ParticlesGpu";
		if (facing == gpu_pfx2::EFacingMode::Velocity)
		{
			materialName += ":FacingVelocity";
			shaderMask |= EGpuParticlesVertexShaderFlags::FacingVelocity;
		}
	}

	IMaterial* pMaterial = m_materials[materialName];
	if (pMaterial)
		return pMaterial;

	pMaterial = gEnv->p3DEngine->GetMaterialManager()->CreateMaterial(materialName);
	static uint32 preload = !!GetCVars()->e_ParticlesPrecacheAssets;
	static uint32 textureLoadFlags = FT_DONT_STREAM * preload;
	_smart_ptr<ITexture> pTexture;
	if (GetPSystem()->IsRuntime())
		pTexture = gEnv->pRenderer->EF_GetTextureByName(textureName, textureLoadFlags);
	if (!pTexture.get())
	{
		GetPSystem()->CheckFileAccess(textureName);
		pTexture.Assign_NoAddRef(gEnv->pRenderer->EF_LoadTexture(textureName, textureLoadFlags));
	}
	if (pTexture->GetTextureID() <= 0)
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect texture %s not found", textureName);

	SInputShaderResourcesPtr pResources = gEnv->pRenderer->EF_CreateInputShaderResource();
	pResources->m_Textures[EFTT_DIFFUSE].m_Name = textureName;
	SShaderItem shaderItem = gEnv->pRenderer->EF_LoadShaderItem(shaderName, false, EF_PRECACHESHADER * preload, pResources, shaderMask);
	assert(shaderItem.m_pShader);
	pMaterial->AssignShaderItem(shaderItem);

	Vec3 white = Vec3(1.0f, 1.0f, 1.0f);
	float defaultOpacity = 1.0f;
	pMaterial->SetGetMaterialParamVec3("diffuse", white, false);
	pMaterial->SetGetMaterialParamFloat("opacity", defaultOpacity, false);

	m_materials[materialName] = pMaterial;

	return pMaterial;
}

uint CParticleSystem::GetParticleSpec() const
{
	int quality = GetCVars()->e_ParticlesQuality;
	if (quality != 0)
		return quality;

	const ESystemConfigSpec configSpec = gEnv->pSystem->GetConfigSpec();
	return uint(configSpec);
}

void CParticleSystem::Reset()
{
	m_bResetEmitters = true;
	m_numFrames = 0;
	for (auto& elem : m_effects)
	{
		if (auto& pEffect = elem.second)
		{
			pEffect->LoadResources();
		}
	}
}

void CParticleSystem::Serialize(TSerialize ser)
{
}

PParticleEffect CParticleSystem::LoadEffect(cstr effectName)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "pfx2::LoadEffect");
	MEMSTAT_CONTEXT(EMemStatContextType::ParticleLibrary, effectName);

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
