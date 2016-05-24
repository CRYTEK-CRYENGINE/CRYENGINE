// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

CParticleEmitter::CParticleEmitter()
	: m_pEffect(0)
	, m_registered(false)
	, m_bounds(0.0f)
	, m_viewDistRatio(1.0f)
	, m_active(false)
	, m_location(IDENTITY)
	, m_editVersion(-1)
	, m_entityId(0)
	, m_entitySlot(-1)
	, m_emitterGeometrySlot(-1)
{
	m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;
	m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
}

CParticleEmitter::~CParticleEmitter()
{
	SetCEffect(0);
	gEnv->p3DEngine->FreeRenderNodeState(this);
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

	CLightVolumesMgr& lightVolumeManager = m_p3DEngine->GetLightVolumeManager();
	SRenderContext renderContext(rParam, passInfo);
	renderContext.m_lightVolumeId = lightVolumeManager.RegisterVolume(GetPos(), GetBBox().GetRadius() * 0.5f, rParam.nClipVolumeStencilRef, passInfo);
	renderContext.m_distance = GetPos().GetDistance(passInfo.GetCamera().GetPosition());
	ColorF fogVolumeContrib;
	CFogVolumeRenderNode::TraceFogVolumes(GetPos(), fogVolumeContrib, passInfo);
	renderContext.m_fogVolumeId = GetRenderer()->PushFogVolumeContribution(fogVolumeContrib, passInfo);

	for (auto& pComponentRuntime : m_componentRuntimes)
	{
		pComponentRuntime.pComponent->Render(pComponentRuntime.pRuntime, renderContext, this);
	}
}

void CParticleEmitter::Update()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);

	m_pEffect->Compile();
	if (m_editVersion != m_pEffect->GetEditVersion())
	{
		m_attributeInstance.Reset(&m_pEffect->GetAttributeTable(), EAttributeScope::PerEmitter);
		UpdateRuntimeRefs();
	}
	m_editVersion = m_pEffect->GetEditVersion();

	if (m_entityId != 0)
		UpdateFromEntity();
	for (auto& pComponentRuntime : m_componentRuntimes)
		pComponentRuntime.pRuntime->MainPreUpdate();

	AABB newBounds = AABB(m_location.t, 0.25f);
	for (auto& ref : m_componentRuntimes)
		newBounds.Add(ref.pRuntime->GetBounds());

	m_visEnviron.Invalidate();

	if (m_registered && !IsEquivalent(m_bounds, newBounds))
		Unregister();

	m_bounds = newBounds;
	if (!m_registered)
		Register();

	if (!m_bounds.IsReset())
	{
		m_visEnviron.Update(GetPos(), m_bounds);
		m_physEnviron.GetPhysAreas(
		  CParticleManager::Instance()->GetPhysEnviron(), m_bounds,
		  m_visEnviron.OriginIndoors(), ENV_GRAVITY | ENV_WIND | ENV_WATER, true, 0);
	}
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
	float maxParticleSize = 0.0f;
	size_t compCount = m_pEffect->GetNumComponents();
	for (size_t i = 0; i < compCount; ++i)
	{
		const auto& params = m_pEffect->GetCComponent(i)->GetComponentParams();
		maxParticleSize = max(maxParticleSize, params.m_maxParticleSize * params.m_visibility.m_viewDistanceMultiple);
	}
	float maxAngularDensity = GetPSystem()->GetMaxAngularDensity();
	float maxDistance = maxAngularDensity * maxParticleSize * m_viewDistRatio;
	return maxDistance;
}

void CParticleEmitter::Precache()
{
}

void CParticleEmitter::GetMemoryUsage(ICrySizer* pSizer) const
{
}

const AABB CParticleEmitter::GetBBox() const
{
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

		UpdateRuntimeRefs();
		AddInstance();
	}
	else
	{
		StopInstances();
	}

	m_active = activate;
}

void CParticleEmitter::Kill()
{
}

bool CParticleEmitter::IsActive() const
{
	return m_active;
}

void CParticleEmitter::Prime()
{
	// PFX2_TODO : implement
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
		const float invDeltaTime = abs(deltaTime) > FLT_EPSILON ? __fres(deltaTime) : 0.0f;
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
	// PFX2_TODO : this is awfull, make it better
	CParticleComponentRuntime::SInstance instance;
	instance.m_parentId = m_parentContainer.GetLastParticleId();
	m_parentContainer.AddParticle();
	if (pData->bHasLocation)
	{
		QuatT location = QuatT(pData->Location);
		m_parentContainer.GetIOVec3Stream(EPVF_Position).Store(instance.m_parentId, location.t);
		m_parentContainer.GetIOQuatStream(EPQF_Orientation).Store(instance.m_parentId, location.q);
		m_parentContainer.GetIOVec3Stream(EPVF_Velocity).Store(instance.m_parentId, Vec3(ZERO));
		m_parentContainer.GetIOVec3Stream(EPVF_AngularVelocity).Store(instance.m_parentId, Vec3(ZERO));
	}

	TComponentId lastComponentId = m_pEffect->GetNumComponents();
	for (TComponentId componentId = 0; componentId < lastComponentId; ++componentId)
	{
		CParticleComponent* pComponent = m_pEffect->GetCComponent(componentId);
		const SComponentParams& params = pComponent->GetComponentParams();
		const bool isEnabled = pComponent->IsEnabled();
		const bool isvalid = params.IsValid();
		if (isEnabled && isvalid && !params.IsSecondGen())
			m_componentRuntimes.back().pRuntime->AddSubInstances(&instance, 1);
	}
}

void CParticleEmitter::SetEntity(IEntity* pEntity, int nSlot)
{
	if (pEntity)
	{
		m_entityId = pEntity->GetId();
		m_entitySlot = nSlot;
	}
	else
		m_entityId = 0;
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
		CMatInfo* pMatInfo = reinterpret_cast<CMatInfo*>(pMaterial);
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

void CParticleEmitter::UpdateRuntimeRefs()
{
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
	for (TComponentId componentId = 0; componentId < lastComponentId; ++componentId)
	{
		bool isActive = true;
		TComponentId thisComponentId = componentId;
		while (thisComponentId != gInvalidId)
		{
			const CParticleComponent* pComponent = m_pEffect->GetCComponent(thisComponentId);
			const SComponentParams& params = pComponent->GetComponentParams();
			const bool isEnabled = pComponent->IsEnabled();
			const bool isValid = params.IsValid();
			if (!isEnabled || !isValid)
			{
				isActive = false;
				break;
			}
			thisComponentId = params.m_parentId;
		}

		const CParticleComponent* component = m_pEffect->GetCComponent(componentId);
		if (!component->GetRuntimeInitializationParameters().usesGpuImplementation)
		{
			CParticleComponentRuntime* pRuntime = m_componentRuntimes[componentId].pRuntime->GetCpuRuntime();
			const SComponentParams& params = pRuntime->GetComponentParams();
			const bool wasActive = pRuntime->IsActive();
			const bool isSecondGen = params.IsSecondGen();
			pRuntime->RemoveAllSubInstances();
			pRuntime->SetActive(isActive);
			if (isActive)
				pRuntime->Initialize();
			else
				pRuntime->Reset();
			if (m_active && !isSecondGen)
			{
				CParticleComponentRuntime::SInstance instance;
				instance.m_parentId = 0;
				pRuntime->AddSubInstances(&instance, 1);
			}
		}
		else
		{
			gpu_pfx2::IParticleComponentRuntime* pRuntime =
			  m_componentRuntimes[componentId].pRuntime->GetGpuRuntime();
			pRuntime->RemoveAllSubInstances();
			pRuntime->SetActive(isActive);
			const bool isSecondGen = pRuntime->IsSecondGen();
			if (isActive && !isSecondGen)
			{
				CParticleComponentRuntime::SInstance instance;
				instance.m_parentId = 0;
				pRuntime->AddSubInstances(&instance, 1);
			}
		}
	}

	m_editVersion = m_pEffect->GetEditVersion();
}

void CParticleEmitter::AddInstance()
{
	m_parentContainer.AddParticle();

	CParticleComponentRuntime::SInstance instance;
	instance.m_parentId = 0;

	for (auto& ref : m_componentRuntimes)
	{
		if (auto pRuntime = ref.pRuntime->GetCpuRuntime())
		{
			const SComponentParams& params = pRuntime->GetComponentParams();
			const bool isSecondGen = params.IsSecondGen();
			if (!isSecondGen)
				ref.pRuntime->AddSubInstances(&instance, 1);
		}
	}
}

void CParticleEmitter::StopInstances()
{
	for (auto& ref : m_componentRuntimes)
	{
		if (auto pRuntime = ref.pRuntime->GetCpuRuntime())
		{
			const SComponentParams& params = pRuntime->GetComponentParams();
			const bool isSecondGen = params.IsSecondGen();
			if (!isSecondGen)
				pRuntime->RemoveAllSubInstances();
		}
		else if (auto pRuntime = ref.pRuntime->GetGpuRuntime())
		{
			const bool isSecondGen = pRuntime->IsSecondGen();
			if (!isSecondGen)
				pRuntime->RemoveAllSubInstances();
		}
	}
}

void CParticleEmitter::UpdateFromEntity()
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
	if (!pEntity)
		return;
	UpdateTargetFromEntity(pEntity);
}

IEntity* CParticleEmitter::GetEmitGeometryEntity() const
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
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
	if (IEntity* pEntity = GetEmitGeometryEntity())
	{
		m_emitterGeometrySlot = m_emitterGeometry.Set(pEntity, m_entitySlot);
	}
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

void CParticleEmitter::UpdateTargetFromEntity(IEntity* pEntity)
{
	if (m_target.bPriority)
		return;

	ParticleTarget target;
	for (IEntityLink* pLink = pEntity->GetEntityLinks(); pLink; pLink = pLink->next)
	{
		if (stricmp(pLink->name, "Target") != 0 && strnicmp(pLink->name, "Target-", 7) != 0)
			continue;

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
	m_pEffect = pEffect;
	if (m_pEffect)
		m_attributeInstance.Reset(&m_pEffect->GetAttributeTable(), EAttributeScope::PerEmitter);
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
	// PFX2_TODO - UnRegisterEntityDirect should only be needed to update bbox on 3dengine
	// gEnv->p3DEngine->UnRegisterEntityAsJob(this);
	gEnv->p3DEngine->UnRegisterEntityDirect(this);
	m_registered = false;
}

bool CParticleEmitter::HasParticles() const
{
	CRY_PFX2_ASSERT(m_pEffect != 0);
	for (auto& ref : m_componentRuntimes)
	{
		if (auto pRuntime = ref.pRuntime->GetCpuRuntime())
		{
			if (pRuntime->GetContainer().GetLastParticleId() != 0)
				return true;
		}
		else if (auto pRuntime = ref.pRuntime->GetGpuRuntime())
		{
			if (pRuntime->HasParticles())
				return true;
		}
	}
	return false;
}

void CParticleEmitter::AccumCounts(SParticleCounts& counts)
{
	counts.EmittersRendered += m_emitterCounts.EmittersRendered;
	counts.ParticlesRendered += m_emitterCounts.ParticlesRendered;
	counts.ParticlesClip += m_emitterCounts.ParticlesClip;
	for (auto& ref : m_componentRuntimes)
	{
		ref.pRuntime->AccumCounts(counts);
	}
	m_emitterCounts = SContainerCounts();
}

void CParticleEmitter::AddDrawCallCounts(int numRendererdParticles, int numClippedParticles)
{
	m_countsMutex.Lock();
	m_emitterCounts.EmittersRendered += 1.0f;
	m_emitterCounts.ParticlesRendered += float(numRendererdParticles);
	m_emitterCounts.ParticlesClip += float(numClippedParticles);
	m_countsMutex.Unlock();
}

CParticleEmitter::SRuntimeRef::SRuntimeRef(CParticleEffect* effect, CParticleEmitter* emitter, CParticleComponent* component, const SRuntimeInitializationParameters& params)
{
	if (params.usesGpuImplementation)
		pRuntime = gEnv->pRenderer->GetGpuParticleManager()->CreateParticleComponentRuntime(component, params);
	else
		pRuntime = new CParticleComponentRuntime(effect, emitter, component);

	pComponent = component;
}

}
