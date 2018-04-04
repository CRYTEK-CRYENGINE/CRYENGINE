// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     23/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "ParticleManager.h"
#include "FogVolumeRenderNode.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"
#include "ParticleDataTypes.h"
#include "ParticleFeature.h"

CRY_PFX2_DBG

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CParticleEmitter

CParticleEmitter::CParticleEmitter(uint emitterId)
	: m_pEffect(0)
	, m_registered(false)
	, m_bounds(AABB::RESET)
	, m_resetBoundsCache(0.0f)
	, m_viewDistRatio(1.0f)
	, m_active(false)
	, m_location(IDENTITY)
	, m_editVersion(-1)
	, m_entityOwner(nullptr)
	, m_entitySlot(-1)
	, m_emitterGeometrySlot(-1)
	, m_time(0.0f)
	, m_deltaTime(0.0f)
	, m_primeTime(0.0f)
	, m_lastTimeRendered(0.0f)
	, m_initialSeed(0)
	, m_emitterId(emitterId)
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
}

CParticleEmitter::~CParticleEmitter()
{
	SetCEffect(0);
	gEnv->p3DEngine->FreeRenderNodeState(this);
	ResetRenderObjects();
}

EERType CParticleEmitter::GetRenderNodeType()
{
	return eERType_ParticleEmitter;
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
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	if (!passInfo.RenderParticles() || passInfo.IsRecursivePass())
		return;
	if (passInfo.IsShadowPass())
		return;

	m_lastTimeRendered = m_time;

	CLightVolumesMgr& lightVolumeManager = m_p3DEngine->GetLightVolumeManager();
	SRenderContext renderContext(rParam, passInfo);
	renderContext.m_lightVolumeId = lightVolumeManager.RegisterVolume(GetPos(), GetBBox().GetRadius() * 0.5f, rParam.nClipVolumeStencilRef, passInfo);
	renderContext.m_distance = GetPos().GetDistance(passInfo.GetCamera().GetPosition());
	ColorF fogVolumeContrib;
	CFogVolumeRenderNode::TraceFogVolumes(GetPos(), fogVolumeContrib, passInfo);
	renderContext.m_fogVolumeId = GetRenderer()->PushFogVolumeContribution(fogVolumeContrib, passInfo);
	
	for (auto& ref : m_componentRuntimes)
	{
		if (passInfo.GetCamera().IsAABBVisible_E(ref.pRuntime->GetBounds()))
		{
			ref.pComponent->Render(this, ref.pRuntime, renderContext);
			m_emitterStats.m_runtimesRendered++;
		}
	}
	m_emitterStats.m_rendered = true;
}

void CParticleEmitter::Update()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);

	m_deltaTime = gEnv->pTimer->GetFrameTime() * GetTimeScale();
	m_deltaTime = max(m_deltaTime, m_primeTime);
	m_primeTime = 0.0f;
	m_time += m_deltaTime;
	++m_currentSeed;

	m_pEffect->Compile();
	if (m_active && m_editVersion != m_pEffect->GetEditVersion())
	{
		m_attributeInstance.Reset(m_pEffect->GetAttributeTable(), EAttributeScope::PerEmitter);
		UpdateRuntimeRefs();
	}

	if (m_entityOwner)
		UpdateFromEntity();

	for (auto pRuntime : m_cpuComponentRuntimes)
		pRuntime->MainPreUpdate();

	UpdateBoundingBox(m_deltaTime);
}

void CParticleEmitter::UpdateBoundingBox(const float frameTime)
{
	AABB bounds = AABB(m_location.t, 1.0f / 1024.0f);
	for (auto pRuntime : m_cpuComponentRuntimes)
		bounds.Add(pRuntime->GetBoundsNonVirtual());
	for (auto pRuntime : m_gpuComponentRuntimes)
		bounds.Add(pRuntime->GetBounds());

	const float sideLen = (bounds.max - bounds.min).GetLength();
	const float round = exp2(ceil(log2(sideLen))) / 64.0f;
	const float invRound = 1.0f / round;

	AABB outterBox;
	outterBox.min.x = floor(bounds.min.x * invRound) * round;
	outterBox.min.y = floor(bounds.min.y * invRound) * round;
	outterBox.min.z = floor(bounds.min.z * invRound) * round;
	outterBox.max.x = ceil(bounds.max.x * invRound) * round;
	outterBox.max.y = ceil(bounds.max.y * invRound) * round;
	outterBox.max.z = ceil(bounds.max.z * invRound) * round;

	bool reRegister = !m_registered;
	m_resetBoundsCache -= frameTime;
	if (m_resetBoundsCache < 0.0f)
	{
		m_bounds = outterBox;
		m_resetBoundsCache = 2.0f;
	}
	else if (!m_bounds.ContainsBox(outterBox))
	{
		reRegister = true;
		m_bounds.Add(outterBox);
	}

	if (reRegister)
	{
		Unregister();
		Register();
		m_visEnviron.Update(GetPos(), m_bounds);
		m_physEnviron.GetPhysAreas(
			CParticleManager::Instance()->GetPhysEnviron(), m_bounds,
			m_visEnviron.OriginIndoors(), ENV_GRAVITY | ENV_WIND | ENV_WATER, true, 0);
	}
}

void CParticleEmitter::DebugRender() const
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	if (m_bounds.IsReset())
		return;

	const bool visible = (m_lastTimeRendered == m_time);
	const ColorB cachedColor = visible ? ColorB(255, 255, 255) : ColorB(255, 0, 0);
	const ColorB boundsColor = visible ? ColorB(255, 128, 0) : ColorB(255, 0, 0);
	pRenderAux->DrawAABB(m_bounds, false, cachedColor, eBBD_Faceted);
	if (visible)
	{
		for (auto& ref : m_componentRuntimes)
			pRenderAux->DrawAABB(ref.pRuntime->GetBounds(), false, boundsColor, eBBD_Faceted);
	}

	ColorF labelColor = ColorF(1.0f, 1.0f, 1.0f);
	stack_string label = stack_string().Format(
		"\"%s\"",
		GetEffect()->GetName());
	IRenderAuxText::DrawLabelEx(m_bounds.GetCenter(), 1.5f, (float*)&labelColor, true, true, label);
}

void CParticleEmitter::PostUpdate()
{
	m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Store(0, Vec3(ZERO));
	m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Store(0, Vec3(ZERO));
}

IPhysicalEntity* CParticleEmitter::GetPhysics() const
{
	return 0;
}

void CParticleEmitter::SetPhysics(IPhysicalEntity*)
{
}

void CParticleEmitter::SetMaterial(IMaterial* pMat)
{
}

IMaterial* CParticleEmitter::GetMaterial(Vec3* pHitPos) const
{
	return 0;
}

IMaterial* CParticleEmitter::GetMaterialOverride()
{
	return 0;
}

float CParticleEmitter::GetMaxViewDist()
{
	IRenderer* pRenderer = GetRenderer();
	const float angularDensity =
		(pRenderer ? GetPSystem()->GetMaxAngularDensity(pRenderer->GetCamera()) : 1080.0f)
		* m_viewDistRatio;

	float maxViewDist = 0.0f;
	size_t compCount = m_pEffect->GetNumComponents();
	for (size_t i = 0; i < compCount; ++i)
	{
		auto comp = m_pEffect->GetCComponent(i);
		if (comp->IsEnabled())
		{
			const auto& params = comp->GetComponentParams();
			const float sizeDist = params.m_maxParticleSize * angularDensity * params.m_visibility.m_viewDistanceMultiple;
			const float dist = min(sizeDist, +params.m_visibility.m_maxCameraDistance);
			maxViewDist = max(maxViewDist, dist);
		}
	}
	return maxViewDist;
}

void CParticleEmitter::Precache()
{
}

void CParticleEmitter::GetMemoryUsage(ICrySizer* pSizer) const
{
}

const AABB CParticleEmitter::GetBBox() const
{
	if (m_bounds.IsReset())
		return AABB(m_location.t, 0.05f);
	else
		return m_bounds;
}

void CParticleEmitter::FillBBox(AABB& aabb)
{
	aabb = GetBBox();
}

void CParticleEmitter::SetBBox(const AABB& WSBBox)
{
}

void CParticleEmitter::OffsetPosition(const Vec3& delta)
{
}

bool CParticleEmitter::IsAllocatedOutsideOf3DEngineDLL()
{
	return false;
}

void CParticleEmitter::SetViewDistRatio(int nViewDistRatio)
{
	IRenderNode::SetViewDistRatio(nViewDistRatio);
	m_viewDistRatio = GetViewDistRatioNormilized();
}

void CParticleEmitter::ReleaseNode(bool bImmediate)
{
	Activate(false);
	Unregister();
}

const IParticleEffect* CParticleEmitter::GetEffect() const
{
	return m_pEffect;
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
	m_lastTimeRendered = m_time;
	m_currentSeed = m_initialSeed;
}

void CParticleEmitter::Activate(bool activate)
{
	if (!m_pEffect || activate == m_active)
		return;

	if (activate)
	{
		m_parentContainer.AddParticleData(EPVF_Position);
		m_parentContainer.AddParticleData(EPQF_Orientation);
		m_parentContainer.AddParticleData(EPVF_Velocity);
		m_parentContainer.AddParticleData(EPVF_AngularVelocity);
		m_parentContainer.AddParticleData(EPDT_NormalAge);

		InitSeed();

		UpdateRuntimeRefs();

		if (m_spawnParams.bPrime)
		{
			if (!(GetCVars()->e_ParticlesDebug & AlphaBit('p')))
				m_primeTime = m_pEffect->GetEquilibriumTime();
		}
	}
	else
	{
		for (auto ref : m_componentRuntimes)
		{
			if (ref.pRuntime->GetGpuRuntime() || !ref.pRuntime->IsSecondGen())
				ref.pRuntime->RemoveAllSubInstances();
		}
	}

	m_active = activate;
}

void CParticleEmitter::Restart()
{
	Activate(false);
	Activate(true);
}

void CParticleEmitter::Kill()
{
	Activate(false);
	for (auto pRuntime : m_cpuComponentRuntimes)
		pRuntime->Reset();
}

bool CParticleEmitter::IsActive() const
{
	return m_active;
}

void CParticleEmitter::SetLocation(const QuatTS& loc)
{
	const Vec3 prevPos = m_location.t;
	const Quat prevQuat = m_location.q;
	const Vec3 newPos = loc.t;
	const Quat newQuat = loc.q;
	m_parentContainer.GetIOVec3Stream(EPVF_Position).Store(0, newPos);
	m_parentContainer.GetIOQuatStream(EPQF_Orientation).Store(0, newQuat);

	if (m_registered)
	{
		const float deltaTime = gEnv->pTimer->GetFrameTime();
		const float invDeltaTime = abs(deltaTime) > FLT_EPSILON ? rcp_fast(deltaTime) : 0.0f;
		const Vec3 velocity0 = m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Load(0);
		const Vec3 angularVelocity0 = m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Load(0);

		const Vec3 velocity1 =
		  velocity0 +
		  (newPos - prevPos) * invDeltaTime;
		const Vec3 angularVelocity1 =
		  angularVelocity0 +
		  2.0f * Quat::log(newQuat * prevQuat.GetInverted()) * invDeltaTime;

		m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Store(0, velocity1);
		m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Store(0, angularVelocity1);
	}

	m_location = loc;
}

void CParticleEmitter::EmitParticle(const EmitParticleData* pData)
{
	// #PFX2_TODO : handle EmitParticleData (create new instances)
	CParticleContainer::SSpawnEntry spawn = {1, m_parentContainer.GetLastParticleId()};
	for (auto pRuntime: m_cpuComponentRuntimes)
	{
		if (pRuntime->IsActive() && !pRuntime->IsSecondGen())
		{
			pRuntime->SpawnParticles(spawn);
		}
	}
}

void CParticleEmitter::SetEntity(IEntity* pEntity, int nSlot)
{
	m_entityOwner = pEntity;
	m_entitySlot = nSlot;
}

//////////////////////////////////////////////////////////////////////////
void CParticleEmitter::InvalidateCachedEntityData()
{
	UpdateFromEntity();
}

void CParticleEmitter::SetTarget(const ParticleTarget& target)
{
	if ((int)target.bPriority >= (int)m_target.bPriority)
		m_target = target;
}

bool CParticleEmitter::UpdateStreamableComponents(float fImportance, const Matrix34A& objMatrix, IRenderNode* pRenderNode, float fEntDistance, bool bFullUpdate, int nLod)
{
	FUNCTION_PROFILER_3DENGINE;

	const TComponentId numComponents = m_pEffect->GetNumComponents();
	for (TComponentId componentId = 0; componentId < numComponents; ++componentId)
	{
		CParticleComponent* pComponent = m_pEffect->GetCComponent(componentId);
		const SComponentParams& params = pComponent->GetComponentParams();

		IMaterial* pMaterial = params.m_pMaterial;
		CMatInfo*  pMatInfo  = reinterpret_cast<CMatInfo*>(pMaterial);
		if (pMatInfo)
			pMatInfo->PrecacheMaterial(fEntDistance, nullptr, bFullUpdate);

		IMeshObj* pMesh = params.m_pMesh;
		if (pMesh)
		{
			CStatObj* pStatObj = static_cast<CStatObj*>(pMesh);
			IMaterial* pMaterial = pStatObj->GetMaterial();
			m_pObjManager->PrecacheStatObj(pStatObj, nLod, objMatrix, pMaterial, fImportance, fEntDistance, bFullUpdate, false);
		}
	}

	return true;
}

void CParticleEmitter::GetSpawnParams(SpawnParams& sp) const
{
	sp = m_spawnParams;
}

void CParticleEmitter::SetEmitGeom(const GeomRef& geom)
{
	// If emitter has an OwnerEntity, it will override this GeomRef on the next frame.
	// So SetOwnerEntity(nullptr) should be called as well.
	m_emitterGeometry = geom;
}

void CParticleEmitter::SetSpawnParams(const SpawnParams& spawnParams)
{
	m_spawnParams = spawnParams;	
}

uint CParticleEmitter::GetAttachedEntityId()
{
	return m_entityOwner ? m_entityOwner->GetId() : INVALID_ENTITYID;
}

void CParticleEmitter::UpdateRuntimeRefs()
{
	// #PFX2_TODO : clean up and optimize this function. Way too messy.

	ResetRenderObjects();

	TComponentRuntimes newRuntimes;

	TComponentId lastComponentId = m_pEffect->GetNumComponents();

	for (TComponentId componentId = 0; componentId < lastComponentId; ++componentId)
	{
		CParticleComponent* pComponent = m_pEffect->GetCComponent(componentId);

		auto it = std::find_if(m_componentRuntimes.begin(), m_componentRuntimes.end(),
		                       [=](const SRuntimeRef& ref)
			{
				return ref.pComponent == pComponent;
		  });

		SRuntimeRef runtimeRef;
		const SRuntimeInitializationParameters& params = pComponent->GetRuntimeInitializationParameters();

		bool createNew = false;
		if (it == m_componentRuntimes.end())
			createNew = true;
		else
		{
			// exists, but wrong runtime type
			// (can mean wrong cpu/gpu type, or wrong maximum number of particles, etc)
			createNew = !it->pRuntime->IsValidRuntimeForInitializationParameters(params);
		}

		if (createNew)
			runtimeRef = SRuntimeRef(m_pEffect, this, pComponent, params);
		else
			runtimeRef = *it;

		newRuntimes.push_back(runtimeRef);
	}

	m_componentRuntimes = newRuntimes;
	m_cpuComponentRuntimes.clear();
	m_gpuComponentRuntimes.clear();

	for (TComponentId componentId = 0; componentId < lastComponentId; ++componentId)
	{
		ICommonParticleComponentRuntime* pRuntime = m_componentRuntimes[componentId].pRuntime;

		if (auto gpuRuntime = pRuntime->GetGpuRuntime())
			m_gpuComponentRuntimes.push_back(gpuRuntime);
		else if (auto cpuRuntime = pRuntime->GetCpuRuntime())
			m_cpuComponentRuntimes.push_back(cpuRuntime);		

		bool isActive = true;
		for (TComponentId thisComponentId = componentId; thisComponentId != gInvalidId; )
		{
			const CParticleComponent* pComponent = m_componentRuntimes[thisComponentId].pComponent;
			const SComponentParams& params = pComponent->GetComponentParams();

			if (!(params.IsValid() && pComponent->IsEnabled() && pComponent->CanMakeRuntime(this)))
			{
				isActive = false;
				break;
			}
			thisComponentId = params.m_parentId;
		}

		pRuntime->RemoveAllSubInstances();
		pRuntime->SetActive(isActive);
		CParticleComponent* pComponent = m_componentRuntimes[componentId].pComponent;
		if (isActive && !pComponent->GetComponentParams().IsSecondGen())
		{
			CParticleComponentRuntime::SInstance instance;
			pRuntime->AddSubInstances({&instance, 1});
		}
		pComponent->PrepareRenderObjects(this);
	}
	
	m_editVersion = m_pEffect->GetEditVersion();
}

void CParticleEmitter::ResetRenderObjects()
{
	if (!m_pEffect)
		return;

	const uint numROs = m_pEffect->GetNumRenderObjectIds();
	for (uint threadId = 0; threadId < RT_COMMAND_BUF_COUNT; ++threadId)
		m_pRenderObjects[threadId].resize(numROs, nullptr);

	const TComponentId lastComponentId = m_pEffect->GetNumComponents();
	for (TComponentId componentId = 0; componentId < lastComponentId; ++componentId)
	{
		CParticleComponent* pComponent = m_pEffect->GetCComponent(componentId);
		pComponent->ResetRenderObjects(this);
	}
}

void CParticleEmitter::AddInstance()
{
	CParticleComponentRuntime::SInstance instance(m_parentContainer.GetLastParticleId());

	m_parentContainer.AddParticle();

	for (auto ref : m_componentRuntimes)
	{
		if (ref.pRuntime->IsActive() && !ref.pRuntime->IsSecondGen())
			ref.pRuntime->AddSubInstances({&instance, 1});
	}
}

void CParticleEmitter::UpdateFromEntity()
{
	if (m_entityOwner)
		UpdateTargetFromEntity(m_entityOwner);
}

IEntity* CParticleEmitter::GetEmitGeometryEntity() const
{
	IEntity* pEntity = m_entityOwner;
	if (pEntity)
	{
		// Override m_emitterGeometry with geometry of owning or attached entity if it exists
		if (IEntity* pParent = pEntity->GetParent())
			pEntity = pEntity->GetParent();
	}
	return pEntity;
}

void CParticleEmitter::UpdateEmitGeomFromEntity()
{
	IEntity* pEntity = GetEmitGeometryEntity();
	if (pEntity)
		m_emitterGeometrySlot = m_emitterGeometry.Set(pEntity);
}

QuatTS CParticleEmitter::GetEmitterGeometryLocation() const
{
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

CRenderObject* CParticleEmitter::GetRenderObject(uint threadId, uint renderObjectIdx)
{
	CRY_PFX2_ASSERT(threadId < RT_COMMAND_BUF_COUNT);
	if (m_pRenderObjects[threadId].empty())
		return nullptr;
	return m_pRenderObjects[threadId][renderObjectIdx];
}

void CParticleEmitter::SetRenderObject(CRenderObject* pRenderObject, uint threadId, uint renderObjectIdx)
{
	CRY_PFX2_ASSERT(threadId < RT_COMMAND_BUF_COUNT);
	m_pRenderObjects[threadId][renderObjectIdx] = pRenderObject;
}

void CParticleEmitter::UpdateTargetFromEntity(IEntity* pEntity)
{
	if (m_target.bPriority)
		return;

	ParticleTarget target;
	for (IEntityLink* pLink = pEntity->GetEntityLinks(); pLink; pLink = pLink->next)
	{
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(pLink->entityId);
		if (pTarget)
		{
			target.bTarget = true;
			target.vTarget = pTarget->GetPos();

			target.vVelocity = Vec3(ZERO);  // PFX2_TODO : grab velocity from entity's physical object

			AABB boundingBox;
			pTarget->GetLocalBounds(boundingBox);
			target.fRadius = boundingBox.GetRadius();

			break;
		}
	}
	m_target = target;
}

void CParticleEmitter::GetParentData(const int parentComponentId, const uint* parentParticleIds, const int numParentParticleIds, SInitialData* data) const
{
	const CParticleContainer& container = (parentComponentId < 0)
	                                      ? m_parentContainer
	                                      : GetRuntimes()[parentComponentId].pRuntime->GetCpuRuntime()->GetContainer();

	IVec3Stream parentPositions = container.GetIVec3Stream(EPVF_Position);
	IVec3Stream parentVelocities = container.GetIVec3Stream(EPVF_Velocity);

	for (int i = 0; i < numParentParticleIds; ++i)
	{
		data[i].position = parentPositions.Load(parentParticleIds[i]);
		data[i].velocity = parentVelocities.Load(parentParticleIds[i]);
	}
}

void CParticleEmitter::SetCEffect(CParticleEffect* pEffect)
{
	Unregister();
	Activate(false);
	if (!pEffect)
		ResetRenderObjects();
	m_pEffect = pEffect;
	if (m_pEffect)
		m_attributeInstance.Reset(m_pEffect->GetAttributeTable(), EAttributeScope::PerEmitter);
	else
		m_attributeInstance.Reset();
}

void CParticleEmitter::Register()
{
	if (m_registered)
		return;
	bool posContained = GetBBox().IsContainPoint(GetPos());
	SetRndFlags(ERF_REGISTER_BY_POSITION, posContained);
	SetRndFlags(ERF_CASTSHADOWMAPS, false);
	gEnv->p3DEngine->RegisterEntity(this);
	m_registered = true;
}

void CParticleEmitter::Unregister()
{
	if (!m_registered)
		return;
	gEnv->p3DEngine->UnRegisterEntityDirect(this);
	m_registered = false;
}

bool CParticleEmitter::HasParticles() const
{
	CRY_PFX2_ASSERT(m_pEffect != 0);

	for (auto pRuntime : m_cpuComponentRuntimes)
	{
		if (pRuntime->GetContainer().GetLastParticleId() != 0)
			return true;
	}
	for (auto pRuntime : m_gpuComponentRuntimes)
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

	CVars* pCVars = static_cast<C3DEngine*>(gEnv->p3DEngine)->GetCVars();
	if (pCVars->e_ParticlesQuality != 0)
		return pCVars->e_ParticlesQuality;

	const ESystemConfigSpec configSpec = gEnv->pSystem->GetConfigSpec();
	return uint(configSpec);
}

void CParticleEmitter::AccumStats(SParticleStats& stats)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);
		
	const uint numRuntimes = m_cpuComponentRuntimes.size();
	stats.m_runtimesAlive += numRuntimes;
	for (auto pRuntime : m_cpuComponentRuntimes)
		pRuntime->AccumStatsNonVirtual(stats);
	stats.m_emittersUpdated += m_emitterStats.m_updated ? 1 : 0;
	stats.m_emittersRendererd += m_emitterStats.m_rendered ? 1 : 0;
	stats.m_runtimesUpdated += m_emitterStats.m_runtimesUpdated;
	stats.m_runtimesRendered += m_emitterStats.m_runtimesRendered;
	stats.m_particlesRendered += m_emitterStats.m_particlesRendered;
	stats.m_particlesUpdated += m_emitterStats.m_particlesUpdated;
	stats.m_particlesClipped += m_emitterStats.m_particlesClipped;

	m_emitterStats = SEmitterStats();
}

void CParticleEmitter::AddUpdatedParticles(uint updatedParticles)
{
	m_statsMutex.Lock();
	m_emitterStats.m_updated = true;
	m_emitterStats.m_runtimesUpdated++;
	m_emitterStats.m_particlesUpdated += updatedParticles;
	m_statsMutex.Unlock();
}

void CParticleEmitter::AddDrawCallCounts(uint numRendererdParticles, uint numClippedParticles)
{
	m_statsMutex.Lock();
	m_emitterStats.m_particlesRendered += numRendererdParticles;
	m_emitterStats.m_particlesClipped += numClippedParticles;
	m_statsMutex.Unlock();
}

CParticleEmitter::SRuntimeRef::SRuntimeRef(CParticleEffect* effect, CParticleEmitter* emitter, CParticleComponent* component, const SRuntimeInitializationParameters& params)
{
	if (params.usesGpuImplementation)
		pRuntime = gEnv->pRenderer->GetGpuParticleManager()->CreateParticleComponentRuntime(emitter, component, params);
	else
		pRuntime = new CParticleComponentRuntime(effect, emitter, component);

	pComponent = component;
}

}
