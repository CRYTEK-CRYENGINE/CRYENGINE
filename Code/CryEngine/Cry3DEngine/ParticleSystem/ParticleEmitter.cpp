// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "ParticleSystem.h"

#include "3dEngine.h"
#include "FogVolumeRenderNode.h"
#include "LightVolumeManager.h"
#include "Material.h"
#include "ObjMan.h"

#include <CryRenderer/IRenderAuxGeom.h>

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CParticleEmitter


// Common data for top-level emitters
struct SEmitterData : SUseData
{
	SEmitterData()
	{
		AddData(EPVF_Position);
		AddData(EPQF_Orientation);
		AddData(EPVF_Velocity);
		AddData(EPVF_AngularVelocity);
		AddData(EPDT_NormalAge);
	}
};

CParticleEmitter::CParticleEmitter(CParticleEffect* pEffect, uint emitterId)
	: m_pEffect(pEffect)
	, m_pEffectOriginal(pEffect)
	, m_registered(false)
	, m_reRegister(false)
	, m_realBounds(AABB::RESET)
	, m_nextBounds(AABB::RESET)
	, m_bounds(AABB::RESET)
	, m_viewDistRatio(1.0f)
	, m_active(false)
	, m_alive(true)
	, m_location(IDENTITY)
	, m_emitterEditVersion(-1)
	, m_effectEditVersion(-1)
	, m_entityOwner(nullptr)
	, m_entitySlot(-1)
	, m_emitterGeometrySlot(-1)
	, m_time(0.0f)
	, m_timeCreated(0.0f)
	, m_timeUpdated(0.0f)
	, m_timeStable(0.0f)
	, m_initialSeed(0)
	, m_emitterId(emitterId)
	, m_unrendered(0)
{
	m_currentSeed = m_initialSeed;
	m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;
	m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;

	auto smoothstep = [](float x) { return x*x*(3 - 2 * x); };
	auto contrast = [](float x) { return x * 0.75f + 0.25f; };
	float x = smoothstep(cry_random(0.0f, 1.0f));
	float r = contrast(smoothstep(max(0.0f, -x * 2 + 1)));
	float g = contrast(smoothstep(1 - abs((x - 0.5f) * 2)));
	float b = contrast(smoothstep(max(0.0f, x * 2 - 1)));
	m_profilerColor = ColorF(r, g, b);

	if (m_pEffect)
		m_attributeInstance.Reset(m_pEffect->GetAttributeTable());

	static PUseData s_emitterData = std::make_shared<SEmitterData>();
	m_parentContainer.SetUsedData(s_emitterData, EDD_Particle);
	m_parentContainer.AddElement();

	m_debug = DebugVar();
}

CParticleEmitter::~CParticleEmitter()
{
	Unregister();
	gEnv->p3DEngine->FreeRenderNodeState(this);
}

const char* CParticleEmitter::GetEntityClassName() const
{
	return "ParticleEmitter";
}

const char* CParticleEmitter::GetName() const
{
	return "";
}

void CParticleEmitter::SetMatrix(const Matrix34& mat)
{
	if (mat.IsValid())
		SetLocation(QuatTS(mat));
}

Vec3 CParticleEmitter::GetPos(bool bWorldOnly) const
{
	return m_location.t;
}

void CParticleEmitter::Render(const struct SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	DBG_LOCK_TO_THREAD(this);

	if (!passInfo.RenderParticles() || passInfo.IsRecursivePass())
		return;

	CParticleJobManager& jobManager = GetPSystem()->GetJobManager();
	SRenderContext renderContext(rParam, passInfo);
	renderContext.m_distance = GetPos().GetDistance(passInfo.GetCamera().GetPosition());

	if (passInfo.IsShadowPass())
	{
		if (!(m_environFlags & ENV_CAST_SHADOWS))
			return;
	}
	else
	{
		if (ThreadMode() >= 4 && !WasRenderedLastFrame())
		{
			// Not currently scheduled for high priority update
			jobManager.ScheduleUpdateEmitter(this, m_runtimesDeferred.size() ? JobManager::eHighPriority : JobManager::eRegularPriority);
		}

		// TODO: make it threadsafe and add it to e_ExecuteRenderAsJobMask
		CLightVolumesMgr& lightVolumeManager = m_p3DEngine->GetLightVolumeManager();
		renderContext.m_lightVolumeId = lightVolumeManager.RegisterVolume(GetPos(), GetBBox().GetRadius() * 0.5f, rParam.nClipVolumeStencilRef, passInfo);
		ColorF fogVolumeContrib;
		CFogVolumeRenderNode::TraceFogVolumes(GetPos(), fogVolumeContrib, passInfo);
		renderContext.m_fogVolumeId = passInfo.GetIRenderView()->PushFogVolumeContribution(fogVolumeContrib, passInfo);
	}

	if (m_runtimesDeferred.size())
	{
		jobManager.AddDeferredRender(this, renderContext);
	}
	
	for (auto& pRuntime : m_runtimes)
	{
		if (pRuntime->GetComponent()->IsVisible())
		{
			if (!passInfo.IsShadowPass() || pRuntime->ComponentParams().m_environFlags & ENV_CAST_SHADOWS)
			{
				if (m_unrendered)
					pRuntime->ResetRenderObjects(passInfo.ThreadID());
				pRuntime->GetComponent()->Render(*pRuntime, renderContext);
			}
		}
	}
	
	m_unrendered = 0;
	GetPSystem()->GetThreadData().statsCPU.emitters.rendered ++;
}

void CParticleEmitter::CheckUpdated()
{
	if (!m_active)
		return;

	CRY_PFX2_PROFILE_DETAIL;

	if (gEnv->IsEditing() && m_effectEditVersion != m_pEffectOriginal->GetEditVersion() + m_emitterEditVersion)
	{
		m_attributeInstance.Reset(m_pEffectOriginal->GetAttributeTable());
		UpdateRuntimes();
	}
	if (m_runtimesFor.empty())
	{
		UpdateRuntimes();
	}
}

bool CParticleEmitter::UpdateState()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (m_time > m_timeDeath)
		m_alive = false;

	if (ThreadMode() >= 3 && IsStable())
		// Update only for last frame, even if update skipped for longer
		m_timeUpdated = m_time;

	const float frameTime = gEnv->pTimer->GetFrameTime() * GetTimeScale();
	m_time += frameTime;
	++m_currentSeed;
	m_unrendered++;

	if (!m_alive)
	{
		if (!m_runtimesFor.empty())
			Clear();
		return false;
	}

	if (m_attributeInstance.WasChanged())
		SetChanged();

	if (m_runtimesFor.empty())
	{
		assert(!m_active);
		if (!m_active)
			return false;
		UpdateRuntimes();
	}

	UpdateFromEntity();

	if (m_reRegister)
	{
		m_reRegister = false;
		m_bounds = m_nextBounds;
		if (!HasBounds())
			Unregister();
		else
			Register();
	}

	if (HasBounds())
		UpdatePhysEnv();

	return true;
}

namespace Bounds
{
	float Expansion       = 1.125f;
	float ShrinkThreshold = 0.125f;
}

void CParticleEmitter::UpdateBounds()
{
	CRY_PFX2_PROFILE_DETAIL;

	const bool allowShrink = ThreadMode() < 3 || !IsStable();
	if (!m_registered || (allowShrink && m_realBounds.GetVolume() <= m_bounds.GetVolume() * Bounds::ShrinkThreshold))
	{
		m_reRegister = true;
		m_nextBounds = m_realBounds;
	}
	else if (!m_bounds.ContainsBox(m_realBounds))
	{
		m_reRegister = true;
		m_nextBounds.Add(m_realBounds);
	}

	if (m_reRegister)
	{
		if (!m_nextBounds.IsReset())
		{
			// Expand bounds to avoid frequent re-registering
			Vec3 center = m_nextBounds.GetCenter();
			Vec3 extent = m_nextBounds.GetSize() * (Bounds::Expansion * 0.5f);
			m_nextBounds = AABB(center - extent, center + extent);
		}
	}
	m_realBounds.Reset();
}

void CParticleEmitter::AddBounds(const AABB& bb)
{
	m_realBounds.Add(bb);
}

bool CParticleEmitter::UpdateParticles()
{
	// Multithread support
	stl::AutoLock<stl::PSyncMultiThread> lock(m_lock);
	if (m_timeUpdated >= m_time)
		return false;

	// Update all components, and accumulate bounds and stats
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	auto& stats = GetPSystem()->GetThreadData().statsCPU;
	stats.emitters.updated ++;

	m_alive = false;
	for (auto& pRuntime : m_runtimes)
	{
		pRuntime->UpdateAll();
		m_realBounds.Add(pRuntime->GetBounds());
		m_alive |= pRuntime->IsAlive();
	}

	PostUpdate();

	UpdateBounds();
	m_timeUpdated = m_time;
	return true;
}

void CParticleEmitter::SyncUpdateParticles()
{
	if (ThreadMode() >= 1)
	{
		auto& statsSync = GetPSystem()->GetThreadData().statsSync;
		statsSync.alloc += m_runtimes.size();
		if (m_runtimesDeferred.size())
			statsSync.alive += m_runtimes.size();
		statsSync.rendered += m_runtimesDeferred.size();
		if (UpdateParticles())
			statsSync.updated += m_runtimes.size();
	}
	CRY_ASSERT(m_timeUpdated >= m_time);
}

void CParticleEmitter::RenderDeferred(const SRenderContext& renderContext)
{
	SyncUpdateParticles();

	for (auto& pRuntime: m_runtimesDeferred)
	{
		pRuntime->GetComponent()->RenderDeferred(*pRuntime, renderContext);
		
		auto& stats = GetPSystem()->GetThreadData().statsCPU;
		stats.components.rendered++;
		stats.spawners.rendered += pRuntime->Container(EDD_Spawner).Size();
		stats.particles.rendered += pRuntime->Container(EDD_Particle).Size();
	}
}

void CParticleEmitter::DebugRender(const SRenderingPassInfo& passInfo) const
{
	if (!GetRenderer())
		return;
	if (!DebugMode('b'))
		return;
	if (!IsAlive() && !HasParticles())
		return;

	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	const bool visible = WasRenderedLastFrame();
	const bool stable = IsStable();

	const float maxDist = non_const(*this).GetMaxViewDist();
	const float dist = (GetPos() - passInfo.GetCamera().GetPosition()).GetLength();
	const float alpha = sqr(crymath::saturate(1.0f - dist / maxDist));
	const float ac = alpha * 0.75f + 0.25f;
	const ColorF alphaColor(ac, ac, ac, alpha);

	if (visible)
	{
		// Draw component boxes and labels
		uint emitterParticles = 0;
		for (auto& pRuntime : m_runtimes)
		{
			if (pRuntime->GetComponent()->IsVisible() && !pRuntime->GetBounds().IsReset())
			{
				uint numParticles = pRuntime->GetNumParticles();
				emitterParticles += numParticles;
				const float volumeRatio = div_min(pRuntime->GetBounds().GetVolume(), m_realBounds.GetVolume(), 1.0f);
				bool hasShadows = !!(pRuntime->ComponentParams().m_environFlags & ENV_CAST_SHADOWS);
				const ColorB componentColor = ColorF(!hasShadows, 0.5, 0) * (alphaColor * sqrt(sqrt(volumeRatio)));
				pRenderAux->DrawAABB(pRuntime->GetBounds(), false, componentColor, eBBD_Faceted);
				string label = string().Format("%s #%d", pRuntime->GetComponent()->GetName(), numParticles);
				IRenderAuxText::DrawLabelEx(pRuntime->GetBounds().GetCenter(), 1.5f, componentColor, true, true, label);
			}
		}
		string label = string().Format("%s #%d Age %.3f", m_pEffect->GetShortName().c_str(), emitterParticles, GetAge());
		IRenderAuxText::DrawLabelEx(m_location.t, 1.5f, (float*)&alphaColor, true, true, label);
	}
	if (!m_bounds.IsReset())
	{
		const ColorB emitterColor = ColorF(1, stable, visible) * alphaColor;
		pRenderAux->DrawAABB(m_bounds, false, emitterColor, eBBD_Faceted);
	}
}

void CParticleEmitter::PostUpdate()
{
	m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Store(TParticleGroupId(0), Vec3v(ZERO));
	m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Store(TParticleGroupId(0), Vec3v(ZERO));
}

float CParticleEmitter::GetMaxViewDist() const
{
	IRenderer* pRenderer = GetRenderer();
	const float angularDensity =
		(pRenderer ? GetPSystem()->GetMaxAngularDensity(GetISystem()->GetViewCamera()) : 1080.0f)
		* m_viewDistRatio;

	float maxViewDist = 0.0f;
	for (auto& pRuntime : m_runtimes)
	{
		const auto& params = pRuntime->ComponentParams();
		const float sizeDist = params.m_maxParticleSize * angularDensity * params.m_visibility.m_viewDistanceMultiple;
		const float dist = min(sizeDist, +params.m_visibility.m_maxCameraDistance);
		SetMax(maxViewDist, dist);
	}
	return maxViewDist;
}

void CParticleEmitter::GetMemoryUsage(ICrySizer* pSizer) const
{
}

void CParticleEmitter::SetViewDistRatio(int nViewDistRatio)
{
	IRenderNode::SetViewDistRatio(nViewDistRatio);
	m_viewDistRatio = GetViewDistRatioNormilized();
}

void CParticleEmitter::ReleaseNode(bool bImmediate)
{
	CRY_ASSERT((m_dwRndFlags & ERF_PENDING_DELETE) == 0);

	Activate(false);
	Unregister();
}

void CParticleEmitter::InitSeed()
{
	const int forcedSeed = GetCVars()->e_ParticlesForceSeed;
	if (m_spawnParams.nSeed != -1)
	{
		m_initialSeed = uint32(m_spawnParams.nSeed);
		m_time = 0.0f;
	}
	else if (forcedSeed != 0)
	{
		m_initialSeed = forcedSeed;
		m_time = 0.0f;
	}
	else
	{
		m_initialSeed = cry_random_uint32();
		m_time = gEnv->pTimer->GetCurrTime();
	}
	m_timeCreated = m_time;
	m_currentSeed = m_initialSeed;
}

void CParticleEmitter::Activate(bool activate)
{
	if (!m_pEffect || activate == m_active)
		return;

	m_active = activate;
	if (activate)
	{
		const bool isNew = m_timeCreated == m_time;
		InitSeed();
		m_timeUpdated = m_timeStable = m_timeDeath = m_time;
		m_bounds = m_realBounds = m_nextBounds = AABB::RESET;
		m_reRegister = false;
		m_alive = true;

		m_effectEditVersion = -1; // Force creation of Runtimes next Update

		if (!isNew)
			CheckUpdated();
	}
	else
	{
		int hasParticles = 0;
		for (auto& pRuntime : m_runtimes)
		{
			if (!pRuntime->IsChild())
				pRuntime->RemoveAllSpawners();
			hasParticles += pRuntime->HasParticles();
		}
		m_timeDeath = m_time;
		if (hasParticles)
		{
			const STimingParams timings = GetMaxTimings();
			m_timeDeath += timings.m_stableTime;
		}
		m_timeStable = m_timeDeath;
	}
}

void CParticleEmitter::Restart()
{
	if (m_time != m_timeCreated)
		Activate(false);
	Activate(true);
}

void CParticleEmitter::SetChanged()
{
	// Update stability and death time
	const STimingParams timings = GetMaxTimings();
	const float frameTime = gEnv->pTimer->GetFrameTime() * GetTimeScale();
	SetMax(m_timeStable, m_time + timings.m_stableTime + frameTime);
}

void CParticleEmitter::Kill()
{
	m_active = false;
	m_alive = false;
}

bool CParticleEmitter::IsActive() const
{
	return m_active;
}

void CParticleEmitter::SetLocation(const QuatTS& loc)
{
	CRY_PFX2_PROFILE_DETAIL;

	QuatTS prevLoc = m_location;
	m_location = loc;
	if (m_spawnParams.bPlaced && m_pEffect->IsSubstitutedPfx1())
	{
		ParticleLoc::RotateYtoZ(m_location.q);
	}

	m_parentContainer.GetIOVec3Stream(EPVF_Position).Store(0, m_location.t);
	m_parentContainer.GetIOQuatStream(EPQF_Orientation).Store(0, m_location.q);

	if (IsEquivalent(m_location, prevLoc, RAD_EPSILON, 0.001f))
		return;

	if (m_registered)
	{
		const float deltaTime = gEnv->pTimer->GetFrameTime();
		const float invDeltaTime = abs(deltaTime) > FLT_EPSILON ? rcp_fast(deltaTime) : 0.0f;
		const Vec3 velocity0 = m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Load(0);
		const Vec3 angularVelocity0 = m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Load(0);

		const Vec3 velocity1 =
		  velocity0 +
		  (m_location.t - prevLoc.t) * invDeltaTime;
		const Vec3 angularVelocity1 =
		  angularVelocity0 +
		  2.0f * Quat::log(m_location.q * prevLoc.q.GetInverted()) * invDeltaTime;

		m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Store(0, velocity1);
		m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Store(0, angularVelocity1);
		m_visEnviron.Invalidate();
	}

	SetChanged();
}

void CParticleEmitter::EmitParticle(const EmitParticleData* pData)
{
	// #PFX2_TODO : handle EmitParticleData (create new spawners)
	for (auto pRuntime: m_runtimes)
	{
		if (!pRuntime->IsChild())
		{
			pRuntime->EmitParticle();
			SetChanged();
		}
	}
}

void CParticleEmitter::SetEmitterFeatures(TParticleFeatures& features)
{
	if (features.m_editVersion != m_emitterEditVersion)
	{
		m_emitterFeatures = features;
		m_emitterEditVersion = features.m_editVersion;
	}
}

void CParticleEmitter::SetEntity(IEntity* pEntity, int nSlot)
{
	m_entityOwner = pEntity;
	m_entitySlot = nSlot;

	if (pEntity && nSlot >= 0)
	{
		uint32 flags0 = pEntity->GetSlotFlags(nSlot);
		uint32 flags1 = flags0;
		SetFlags(flags1, ENTITY_SLOT_CAST_SHADOW, !!(m_environFlags & ENV_CAST_SHADOWS));
		if (flags1 != flags0)
			pEntity->SetSlotFlags(nSlot, flags1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleEmitter::InvalidateCachedEntityData()
{
	UpdateFromEntity();
}

void CParticleEmitter::SetTarget(const ParticleTarget& target)
{
	if ((int)target.bPriority >= (int)m_target.bPriority)
	{
		m_target = target;
		SetChanged();
	}
}

void CParticleEmitter::UpdateStreamingPriority(const SUpdateStreamingPriorityContext& context)
{
	FUNCTION_PROFILER_3DENGINE;

	for (auto& pRuntime : m_runtimes)
	{
		const auto* pComponent = pRuntime->GetComponent();
		const SComponentParams& params = pComponent->GetComponentParams();
		const float normalizedDist = context.distance 
			* context.pPassInfo->GetZoomFactor()
			* rcp_safe(params.m_maxParticleSize);
		const bool bHighPriority = !!(params.m_renderObjectFlags & FOB_NEAREST);

		if (CMatInfo* pMatInfo  = static_cast<CMatInfo*>(params.m_pMaterial.get()))
		{
			const float adjustedDist = normalizedDist
				* min(params.m_shaderData.m_tileSize[0], params.m_shaderData.m_tileSize[1]);
			pMatInfo->PrecacheMaterial(adjustedDist, nullptr, context.bFullUpdate, bHighPriority);
		}

		if (CStatObj* pStatObj = static_cast<CStatObj*>(params.m_pMesh.get()))
			m_pObjManager->PrecacheStatObj(pStatObj, context.lod,
				pStatObj->GetMaterial(), context.importance, normalizedDist, context.bFullUpdate, bHighPriority);
	}
}

void CParticleEmitter::UpdatePhysEnv()
{
	CRY_PFX2_PROFILE_DETAIL;
	CParticleManager::Instance()->GetPhysEnviron().Update(m_physEnviron, 
		m_bounds, m_visEnviron.OriginIndoors(), m_environFlags | ENV_WATER, true, 0);
}

void CParticleEmitter::GetSpawnParams(SpawnParams& spawnParams) const
{
	spawnParams = m_spawnParams;
}

void CParticleEmitter::SetEmitGeom(const GeomRef& geom)
{
	// If emitter has an OwnerEntity, it will override this GeomRef on the next frame.
	// So SetOwnerEntity(nullptr) should be called as well.
	m_emitterGeometry = geom;
	SetChanged();
}

void CParticleEmitter::SetSpawnParams(const SpawnParams& spawnParams)
{
	if (spawnParams.bIgnoreVisAreas != m_spawnParams.bIgnoreVisAreas || spawnParams.bRegisterByBBox != m_spawnParams.bRegisterByBBox)
		Unregister();
	bool update = spawnParams.eSpec != m_spawnParams.eSpec;
	m_spawnParams = spawnParams;
	m_reRegister = true;
	SetChanged();
	if (update)
		m_runtimesFor.clear();
}

uint CParticleEmitter::GetAttachedEntityId()
{
	return m_entityOwner ? m_entityOwner->GetId() : INVALID_ENTITYID;
}

void CParticleEmitter::UpdateRuntimes()
{
	CRY_PFX2_PROFILE_DETAIL;

	m_pEffectOriginal->Compile();

	if (!m_emitterFeatures.empty())
	{
		// clone and modify effect
		m_pEffect = new CParticleEffect(*m_pEffectOriginal);
		for (auto& pComponent : m_pEffect->GetComponents())
		{
			pComponent = new CParticleComponent(*pComponent);
			pComponent->SetEffect(m_pEffect);
			pComponent->ClearChildren();
			if (auto pParentComponent = pComponent->GetParentComponent())
			{
				auto pNewParent = m_pEffect->GetComponent(pParentComponent->GetComponentId());
				pComponent->SetParent(pNewParent);
			}
			else
			{
				for (auto& feature : m_emitterFeatures)
					pComponent->AddFeature(static_cast<CParticleFeature*>(feature.get()));
			}
		}
		m_pEffect->SetChanged();
		m_pEffect->Compile();
	}
	else
	{
		m_pEffect = m_pEffectOriginal;
	}

	m_runtimesFor.clear();
	m_runtimesPreUpdate.clear();
	m_runtimesDeferred.clear();
	m_environFlags = 0;

	TRuntimes newRuntimes;

	for (const auto& pComponent: m_pEffect->GetComponents())
	{
		if (!pComponent->CanMakeRuntime(this))
		{
			m_runtimesFor.push_back(nullptr);
			continue;
		}

		CParticleComponentRuntime* pRuntime = stl::find_value_if(m_runtimes,
			[&](CParticleComponentRuntime* pRuntime)
			{
				return pRuntime->GetComponent() == pComponent;
			}
		);

		if (!pRuntime || !pRuntime->IsValidForComponent())
		{
			pRuntime = new CParticleComponentRuntime(this, pComponent);
		}
		else
		{
			pRuntime->Initialize();
			if (pRuntime->HasParticles())
				pComponent->OnEdit(*pRuntime);
		}

		newRuntimes.push_back(pRuntime);
		m_runtimesFor.push_back(pRuntime);
		if (pComponent->MainPreUpdate.size())
			m_runtimesPreUpdate.push_back(pRuntime);
		if (pComponent->RenderDeferred.size())
			m_runtimesDeferred.push_back(pRuntime);
		m_environFlags |= pComponent->ComponentParams().m_environFlags;
	}

	m_runtimes = newRuntimes;

	if (!!(GetRndFlags() & ERF_CASTSHADOWMAPS) != !!(m_environFlags & ENV_CAST_SHADOWS))
		m_reRegister = true;

	m_effectEditVersion = m_pEffectOriginal->GetEditVersion() + m_emitterEditVersion;
	m_alive = true;

	const STimingParams timings = GetMaxTimings();
	m_timeStable = m_timeCreated + timings.m_equilibriumTime;
	m_timeDeath = m_timeCreated + timings.m_maxTotalLife;

	if (m_spawnParams.bPrime && !DebugMode('p'))
	{
		if (m_timeUpdated == m_timeCreated)
			m_time = m_timeStable;
	}
}

ILINE void CParticleEmitter::UpdateFromEntity()
{
	if (m_environFlags & ENV_TARGET)
		UpdateTarget(m_target, m_entityOwner, GetLocation().t);
}

IEntity* CParticleEmitter::GetEmitGeometryEntity() const
{
	CRY_PFX2_PROFILE_DETAIL;
	IEntity* pEntity = m_entityOwner;
	if (pEntity)
	{
		// Override m_emitterGeometry with geometry of owning or attached entity if it exists
		if (IEntity* pParent = pEntity->GetParent())
			pEntity = pParent;
	}
	return pEntity;
}

void CParticleEmitter::UpdateEmitGeomFromEntity()
{
	CRY_PFX2_PROFILE_DETAIL;
	IEntity* pEntity = GetEmitGeometryEntity();
	m_emitterGeometrySlot = m_emitterGeometry.Set(pEntity);
}

QuatTS CParticleEmitter::GetEmitterGeometryLocation() const
{
	CRY_PFX2_PROFILE_DETAIL;
	if (IEntity* pEntity = GetEmitGeometryEntity())
	{
		SEntitySlotInfo slotInfo;
		if (pEntity->GetSlotInfo(m_emitterGeometrySlot, slotInfo))
		{
			if (slotInfo.pWorldTM)
				return QuatTS(*slotInfo.pWorldTM);
		}

	}
	return m_location;
}

void CParticleEmitter::Register()
{
	CRY_PFX2_PROFILE_DETAIL;

	if (m_registered)
		gEnv->p3DEngine->UnRegisterEntityDirect(this);
	if (!HasBounds())
	{
		m_registered = false;
		return;
	}

	bool posContained = GetBBox().IsContainPoint(GetPos());
	SetRndFlags(ERF_REGISTER_BY_POSITION, posContained);
	SetRndFlags(ERF_REGISTER_BY_BBOX, m_spawnParams.bRegisterByBBox);
	SetRndFlags(ERF_RENDER_ALWAYS, m_spawnParams.bIgnoreVisAreas);
	SetRndFlags(ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS, !!(m_environFlags & ENV_CAST_SHADOWS));
	SetRndFlags(ERF_FOB_ALLOW_TERRAIN_LAYER_BLEND, !m_spawnParams.bIgnoreTerrainLayerBlend);
	SetRndFlags(ERF_FOB_ALLOW_DECAL_BLEND, !m_spawnParams.bIgnoreDecalBlend);

	gEnv->p3DEngine->RegisterEntity(this);
	m_registered = true;

	m_visEnviron.Invalidate();
	m_visEnviron.Update(GetPos(), m_bounds);
	m_physEnviron.OnPhysAreaChange();
}

void CParticleEmitter::Unregister()
{
	if (!m_registered)
		return;
	CRY_PFX2_PROFILE_DETAIL;

	gEnv->p3DEngine->UnRegisterEntityDirect(this);
	m_registered = false;
	m_bounds.Reset();
	m_realBounds.Reset();
}

void CParticleEmitter::Clear()
{
	CRY_PFX2_PROFILE_DETAIL;
	m_alive = m_active = false;
	Unregister();
	m_runtimes.clear();
	m_runtimesFor.clear();
}

bool CParticleEmitter::HasParticles() const
{
	for (auto& pRuntime : m_runtimes)
	{
		if (pRuntime->HasParticles())
			return true;
	}

	return false;
}

uint CParticleEmitter::GetParticleSpec() const
{
	if (m_spawnParams.eSpec != EParticleSpec::Default)
		return uint(m_spawnParams.eSpec);

	return GetPSystem()->GetParticleSpec();
}

pfx2::STimingParams CParticleEmitter::GetMaxTimings() const
{
	CRY_PFX2_PROFILE_DETAIL;

	STimingParams timingsMax = {};
	for (const auto& pRuntime : m_runtimes)
	{
		if (!pRuntime->GetComponent()->GetParentComponent())
		{
			STimingParams timings = pRuntime->GetMaxTimings();
			SetMax(timingsMax.m_maxParticleLife, timings.m_maxParticleLife);
			SetMax(timingsMax.m_stableTime, timings.m_stableTime);
			SetMax(timingsMax.m_equilibriumTime, timings.m_equilibriumTime);
			SetMax(timingsMax.m_maxTotalLife, timings.m_maxTotalLife);
		}
	}

	return timingsMax;
}

}
