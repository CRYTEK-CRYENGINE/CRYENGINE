// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     02/04/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleComponentRuntime.h"
#include "ParticleSystem.h"
#include "ParticleProfiler.h"

namespace pfx2
{

extern EParticleDataType EPVF_Acceleration, EPVF_VelocityField;


CParticleComponentRuntime::CParticleComponentRuntime(CParticleEmitter* pEmitter, CParticleComponent* pComponent)
	: m_pEmitter(pEmitter)
	, m_pComponent(pComponent)
	, m_bounds(AABB::RESET)
{
	Initialize();
	if (pComponent->UsesGPU())
	{
		m_pGpuRuntime = gEnv->pRenderer->GetGpuParticleManager()->CreateParticleContainer(
			pComponent->GPUComponentParams(),
			pComponent->GetGpuFeatures());
	}
}

bool CParticleComponentRuntime::IsValidForComponent() const
{
	if (!m_pComponent->UsesGPU())
		return !m_pGpuRuntime;
	else
		return m_pGpuRuntime && m_pGpuRuntime->IsValidForParams(m_pComponent->GPUComponentParams());
}

CParticleContainer& CParticleComponentRuntime::GetParentContainer()
{
	if (CParticleComponent* pParent = m_pComponent->GetParentComponent())
		return GetEmitter()->GetRuntimeFor(pParent)->GetContainer();
	else
		return GetEmitter()->GetParentContainer();
}

const CParticleContainer& CParticleComponentRuntime::GetParentContainer() const
{
	if (CParticleComponent* pParent = m_pComponent->GetParentComponent())
		return GetEmitter()->GetRuntimeFor(pParent)->GetContainer();
	else
		return GetEmitter()->GetParentContainer();
}

void CParticleComponentRuntime::Initialize()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	m_container.ResetUsedData();
	for (EParticleDataType type(0); type < EParticleDataType::size(); type = type + type.info().dimension)
		if (m_pComponent->UseParticleData(type))
			m_container.AddParticleData(type);
	m_container.Trim();
}

void CParticleComponentRuntime::UpdateAll()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (GetGpuRuntime())
		return UpdateGPURuntime(SUpdateContext(this));

	AddRemoveParticles(SUpdateContext(this));
	UpdateParticles(SUpdateContext(this));
	CalculateBounds();
}

void CParticleComponentRuntime::AddRemoveParticles(const SUpdateContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (GetGpuRuntime())
		return UpdateGPURuntime(context);

	CTimeProfiler profile(GetPSystem()->GetProfiler(), this, EPS_NewBornTime);

	m_container.RemoveNewBornFlags();
	RemoveParticles(context);

	AddParticles(context);
	UpdateNewBorns(context);
	m_container.ResetSpawnedParticles();

	m_particleStats.updated += m_container.GetLastParticleId();
}

void CParticleComponentRuntime::UpdateParticles(const SUpdateContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	CTimeProfiler profile(GetPSystem()->GetProfiler(), this, EPS_UpdateTime);

	m_container.FillData(EPVF_Acceleration, 0.0f, context.m_updateRange);
	m_container.FillData(EPVF_VelocityField, 0.0f, context.m_updateRange);

	for (EParticleDataType type(0); type < EParticleDataType::size(); type = type + type.info().step())
	{
		if (type.info().hasInit)
			m_container.CopyData(type, InitType(type), context.m_updateRange);
	}

	GetComponent()->PreUpdateParticles(context);
	GetComponent()->UpdateParticles(context);
	GetComponent()->PostUpdateParticles(context);

	UpdateLocalSpace(context.m_updateRange);
	AgeUpdate(context);
}

void CParticleComponentRuntime::ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)
{
	if (GetComponent()->IsVisible())
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
		CTimeProfiler profile(GetPSystem()->GetProfiler(), this, EPS_ComputeVerticesTime);

		GetComponent()->ComputeVertices(this, camInfo, pRE, uRenderFlags, fMaxPixels);
	}
}

void CParticleComponentRuntime::AddSubInstances(TVarArray<SInstance> instances)
{
	CRY_PFX2_PROFILE_DETAIL;

	if (instances.empty())
		return;

	const SComponentParams& params = GetComponentParams();

	uint firstInstance = m_subInstances.size();
	uint lastInstance = firstInstance + instances.size();
	m_subInstances.append(instances);
	m_subInstanceData.resize(params.m_instanceDataStride * m_subInstances.size());
	
	SUpdateContext context(this);
	SUpdateRange instanceRange(firstInstance, lastInstance);
	GetComponent()->InitSubInstances(context, instanceRange);

	DebugStabilityCheck();
}

void CParticleComponentRuntime::RemoveAllSubInstances()
{
	m_subInstances.clear();
	m_subInstanceData.clear();

	IOPidStream parentIds = m_container.GetIOPidStream(EPDT_ParentId);
	parentIds.Fill(m_container.GetFullRange(), gInvalidId);
	DebugStabilityCheck();
}

void CParticleComponentRuntime::ReparentParticles(TConstArray<TParticleId> swapIds)
{
	CRY_PFX2_PROFILE_DETAIL;

	const SComponentParams& params = GetComponentParams();
	IOPidStream parentIds = m_container.GetIOPidStream(EPDT_ParentId);

	for (TParticleId particleId = 0; particleId != m_container.GetLastParticleId(); ++particleId)
	{
		const TParticleId parentId = parentIds.Load(particleId);
		if (parentId != gInvalidId)
		{
			CRY_PFX2_ASSERT(parentId < swapIds.size());   // this particle was pointing to the wrong parentId already
			parentIds.Store(particleId, swapIds[parentId]);
		}
	}

	size_t dataStride = params.m_instanceDataStride;
	byte* pBytes = m_subInstanceData.data();
	uint toCopy = 0;
	for (uint i = 0; pBytes && i < m_subInstances.size(); ++i)
	{
		m_subInstances[i].m_parentId = swapIds[m_subInstances[i].m_parentId];
		if (m_subInstances[i].m_parentId == gInvalidId)
			continue;
		m_subInstances[toCopy] = m_subInstances[i];
		memcpy(pBytes + dataStride * toCopy, pBytes + dataStride * i, dataStride);
		++toCopy;
	}
	m_subInstances.erase(m_subInstances.begin() + toCopy, m_subInstances.end());
	m_subInstanceData.resize(params.m_instanceDataStride * m_subInstances.size());

	DebugStabilityCheck();
}

void CParticleComponentRuntime::AddSpawnEntry(const SSpawnEntry& entry)
{
	if (entry.m_count > 0)
		m_spawnEntries.push_back(entry);
}

SChaosKey CParticleComponentRuntime::MakeSeed(TParticleId particleId) const
{
	uint32 emitterSeed = m_pEmitter->GetCurrentSeed();
	uint32 componentId = m_pComponent->GetComponentId() << 16;
	return pfx2::SChaosKey(emitterSeed + componentId + particleId);
}

SChaosKey CParticleComponentRuntime::MakeParentSeed(TParticleId particleId) const
{
	uint32 emitterSeed = m_pEmitter->GetCurrentSeed();
	uint32 componentId = m_pComponent->GetParentComponent() ?  m_pComponent->GetParentComponent()->GetComponentId() << 16 : gInvalidId;
	return pfx2::SChaosKey(emitterSeed + componentId + particleId);
}

void CParticleComponentRuntime::AddParticles(const SUpdateContext& context)
{
	if (GetNumInstances())
		GetComponent()->SpawnParticles(context);
	m_container.AddParticles(m_spawnEntries);
	m_spawnEntries.clear();
}

void CParticleComponentRuntime::RemoveParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	const uint32 numParticles = m_container.GetNumParticles();
	TIStream<uint8> states = m_container.GetTIStream<uint8>(EPDT_State);

	if (GetComponent()->KillParticles.size())
	{
		TParticleIdArray particleIds(*context.m_pMemHeap);
		particleIds.reserve(numParticles);

		for (auto particleId : context.GetUpdateRange())
		{
			const uint8 state = states.Load(particleId);
			if (state == ES_Expired)
				particleIds.push_back(particleId);
		}
		if (!particleIds.empty())
		{
			GetComponent()->KillParticles(context, particleIds);
		}
	}

	TParticleIdArray removeIds(*context.m_pMemHeap);
	removeIds.reserve(numParticles);

	for (auto particleId : context.GetUpdateRange())
	{
		const uint8 state = states.Load(particleId);
		if (state == ES_Dead)
			removeIds.push_back(particleId);
	}

	if (!removeIds.empty())
	{
		TParticleIdArray swapIds(*context.m_pMemHeap);
		const bool hasChildren = !m_pComponent->GetChildComponents().empty();
		if (hasChildren)
			swapIds.resize(numParticles);

		m_container.RemoveParticles(removeIds, swapIds);

		if (hasChildren)
		{
			for (const auto& pChild : m_pComponent->GetChildComponents())
			{
				if (auto pSubRuntime = m_pEmitter->GetRuntimeFor(pChild))
					pSubRuntime->ReparentParticles(swapIds);
			}
		}
	}
}

void CParticleComponentRuntime::UpdateNewBorns(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	if (!m_container.HasSpawnedParticles())
		return;

	const floatv deltaTime = ToFloatv(context.m_deltaTime);
	CParticleContainer& parentContainer = GetParentContainer();

	// interpolate position and normAge over time and velocity
	const IPidStream parentIds = m_container.GetIPidStream(EPDT_ParentId);
	const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	const IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);
	const IFStream parentNormAges = parentContainer.GetIFStream(EPDT_NormalAge);
	const IFStream parentLifeTimes = parentContainer.GetIFStream(EPDT_LifeTime);

	IOVec3Stream positions = m_container.GetIOVec3Stream(EPVF_Position);
	IOFStream normAges = m_container.GetIOFStream(EPDT_NormalAge);

	const bool checkParentLife = IsChild();

	for (auto particleGroupId : context.GetSpawnedGroupRange())
	{
		floatv backTime = normAges.Load(particleGroupId) * deltaTime;
		const uint32v parentGroupId = parentIds.Load(particleGroupId);
		if (checkParentLife)
		{
			const floatv parentNormAge = parentNormAges.SafeLoad(parentGroupId);
			const floatv parentLifeTime = parentLifeTimes.SafeLoad(parentGroupId);
			const floatv parentOverAge = max(parentNormAge * parentLifeTime - parentLifeTime, convert<floatv>());
			backTime = min(backTime + parentOverAge, convert<floatv>());
		}
		const Vec3v wParentPos = parentPositions.SafeLoad(parentGroupId);
		const Vec3v wParentVel = parentVelocities.SafeLoad(parentGroupId);
		const Vec3v wPosition = MAdd(wParentVel, backTime, wParentPos);
		positions.Store(particleGroupId, wPosition);
	}

	// update orientation if any
	if (m_container.HasData(EPQF_Orientation))
	{
		const Quat defaultQuat = GetEmitter()->GetLocation().q;
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		IOQuatStream quats = m_container.GetIOQuatStream(EPQF_Orientation);
		for (auto particleGroupId : context.GetSpawnedGroupRange())
		{
			const TParticleIdv parentGroupId = parentIds.Load(particleGroupId);
			const Quatv wParentQuat = parentQuats.SafeLoad(parentGroupId);
			quats.Store(particleGroupId, wParentQuat);
		}
	}

	// neutral velocity
	m_container.FillData(EPVF_Velocity, 0.0f, m_container.GetSpawnedRange());

	// initialize unormrand
	if (m_container.HasData(EPDT_Random))
	{
		IOFStream unormRands = m_container.GetIOFStream(EPDT_Random);
		for (auto particleGroupId : context.GetSpawnedGroupRange())
		{
			const floatv unormRand = context.m_spawnRngv.RandUNorm();
			unormRands.Store(particleGroupId, unormRand);
		}
	}

	// feature init particles
	m_container.FillData(EPDT_State, uint8(ES_NewBorn), m_container.GetSpawnedRange());
	GetComponent()->InitParticles(context);

	// modify with spawn params
	const SpawnParams& spawnParams = GetEmitter()->GetSpawnParams();
	if (spawnParams.fSizeScale != 1.0f && m_container.HasData(EPDT_Size))
	{
		const floatv scalev = ToFloatv(spawnParams.fSizeScale);
		IOFStream sizes = m_container.GetIOFStream(EPDT_Size);
		for (auto particleGroupId : context.GetSpawnedGroupRange())
		{
			const floatv size0 = sizes.Load(particleGroupId);
			const floatv size1 = size0 * scalev;
			sizes.Store(particleGroupId, size1);
		}
		m_container.CopyData(InitType(EPDT_Size), EPDT_Size, context.GetSpawnedRange());
	}
	if (spawnParams.fSpeedScale != 1.0f && m_container.HasData(EPVF_Velocity))
	{
		const floatv scalev = ToFloatv(spawnParams.fSpeedScale);
		IOVec3Stream velocities = m_container.GetIOVec3Stream(EPVF_Velocity);
		for (auto particleGroupId : context.GetSpawnedGroupRange())
		{
			const Vec3v velocity0 = velocities.Load(particleGroupId);
			const Vec3v velocity1 = velocity0 * scalev;
			velocities.Store(particleGroupId, velocity1);
		}
	}

	UpdateLocalSpace(m_container.GetSpawnedRange());

	// feature post init particles
	GetComponent()->PostInitParticles(context);
}

void CParticleComponentRuntime::CalculateBounds()
{
	CRY_PFX2_PROFILE_DETAIL;
	CTimeProfiler profile(GetPSystem()->GetProfiler(), this, EPS_UpdateTime);

	IVec3Stream positions = m_container.GetIVec3Stream(EPVF_Position);
	IFStream sizes = m_container.GetIFStream(EPDT_Size);
	const floatv fMin = ToFloatv(std::numeric_limits<float>::max());
	const floatv fMax = ToFloatv(-std::numeric_limits<float>::max());

	Vec3v bbMin = Vec3v(fMin, fMin, fMin);
	Vec3v bbMax = Vec3v(fMax, fMax, fMax);

	// PFX2_TODO : clean up this mess
#ifdef CRY_PFX2_USE_SSE
	// vector part
	const TParticleId lastParticleId = m_container.GetLastParticleId();
	const TParticleGroupId lastParticleGroupId = CRY_PFX2_PARTICLESGROUP_LOWER(lastParticleId);
	for (auto particleGroupId : SGroupRange(0, lastParticleGroupId))
	{
		const floatv size = sizes.Load(particleGroupId);
		const Vec3v position = positions.Load(particleGroupId);
		bbMin = min(bbMin, Sub(position, size));
		bbMax = max(bbMax, Add(position, size));
	}
	m_bounds.min = HMin(bbMin);
	m_bounds.max = HMax(bbMax);

	// linear part
	for (auto particleId : SUpdateRange(+lastParticleGroupId, lastParticleId))
	{
		const float size = sizes.Load(particleId);
		const Vec3 sizev = Vec3(size, size, size);
		const Vec3 position = positions.Load(particleId);
		m_bounds.min = min(m_bounds.min, position - sizev);
		m_bounds.max = max(m_bounds.max, position + sizev);
	}
#else
	for (auto particleId : m_container.GetFullRange())
	{
		const float size = sizes.Load(particleId);
		const Vec3 sizev = Vec3(size, size, size);
		const Vec3 position = positions.Load(particleId);
		m_bounds.min = min(m_bounds.min, position - sizev);
		m_bounds.max = max(m_bounds.max, position + sizev);
	}
#endif

	if (m_bounds.GetRadius() > 1000.f)
	{
		m_bounds = m_bounds;
	}

	// augment bounds from features
	GetComponent()->ComputeBounds(this, m_bounds);
}

void CParticleComponentRuntime::AgeUpdate(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	IFStream invLifeTimes = m_container.GetIFStream(EPDT_InvLifeTime);
	IOFStream normAges = m_container.GetIOFStream(EPDT_NormalAge);
	const floatv frameTime = ToFloatv(context.m_deltaTime);

	for (auto particleGroupId : context.GetUpdateGroupRange())
	{
		const floatv invLifeTime = invLifeTimes.Load(particleGroupId);
		const floatv normAge0 = normAges.Load(particleGroupId);
		const floatv normalAge1 = __fsel(normAge0,
			normAge0 + frameTime * invLifeTime,
			-normAge0 * frameTime * invLifeTime
		);
		normAges.Store(particleGroupId, normalAge1);
	}

	TIOStream<uint8> states = m_container.GetTIOStream<uint8>(EPDT_State);
	for (auto particleId : context.GetUpdateRange())
	{
		const float normalAge = normAges.Load(particleId);
		uint8 state = states.Load(particleId);
		if (normalAge >= 1.0f && state == ES_Alive)
			state = ES_Expired;
		else if (state == ES_Expired)
			state = ES_Dead;
		states.Store(particleId, state);
	}
}

void CParticleComponentRuntime::UpdateLocalSpace(SUpdateRange range)
{
	CRY_PFX2_PROFILE_DETAIL;

	const bool hasLocalPositions = m_container.HasData(EPVF_LocalPosition);
	const bool hasLocalVelocities = m_container.HasData(EPVF_LocalVelocity);
	const bool hasLocalOrientations = m_container.HasData(EPQF_LocalOrientation) && m_container.HasData(EPQF_Orientation);

	if (!hasLocalPositions && !hasLocalVelocities && !hasLocalOrientations)
		return;

	const CParticleContainer& parentContainer = GetParentContainer();
	const Quat defaultQuat = GetEmitter()->GetLocation().q;
	const IPidStream parentIds = m_container.GetIPidStream(EPDT_ParentId);
	const auto parentStates = parentContainer.GetTIStream<uint8>(EPDT_State);
	const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	const IQuatStream parentOrientations = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
	const IVec3Stream worldPositions = m_container.GetIVec3Stream(EPVF_Position);
	const IVec3Stream worldVelocities = m_container.GetIVec3Stream(EPVF_Velocity);
	const IQuatStream worldOrientations = m_container.GetIQuatStream(EPQF_Orientation);
	IOVec3Stream localPositions = m_container.GetIOVec3Stream(EPVF_LocalPosition);
	IOVec3Stream localVelocities = m_container.GetIOVec3Stream(EPVF_LocalVelocity);
	IOQuatStream localOrientations = m_container.GetIOQuatStream(EPQF_LocalOrientation);

	for (auto particleId : range)
	{
		const TParticleId parentId = parentIds.Load(particleId);
		const uint8 parentState = (parentId != gInvalidId) ? parentStates.Load(parentId) : ES_Expired;
		if (parentState == ES_Expired)
			continue;

		const Vec3 wParentPos = parentPositions.Load(parentId);
		const Quat parentOrientation = parentOrientations.SafeLoad(parentId);
		const QuatT worldToParent = QuatT(wParentPos, parentOrientation).GetInverted();

		if (hasLocalPositions)
		{
			const Vec3 wPosition = worldPositions.Load(particleId);
			const Vec3 oPosition = worldToParent * wPosition;
			localPositions.Store(particleId, oPosition);
		}

		if (hasLocalVelocities)
		{
			const Vec3 wVelocity = worldVelocities.Load(particleId);
			const Vec3 oVelocity = worldToParent.q * wVelocity;
			localVelocities.Store(particleId, oVelocity);
		}

		if (hasLocalOrientations)
		{
			const Quat wOrientation = worldOrientations.Load(particleId);
			const Quat oOrientation = worldToParent.q * wOrientation;
			localOrientations.Store(particleId, oOrientation);
		}
	}
}

void CParticleComponentRuntime::UpdateGPURuntime(const SUpdateContext& context)
{
	if (!m_pGpuRuntime)
		return;

	gpu_pfx2::SUpdateParams params;

	params.emitterPosition    = m_pEmitter->GetLocation().t;
	params.emitterOrientation = m_pEmitter->GetLocation().q;
	params.physAccel          = m_pEmitter->GetPhysicsEnv().m_UniformForces.vAccel;
	params.physWind           = m_pEmitter->GetPhysicsEnv().m_UniformForces.vWind;
	
	GetComponent()->UpdateGPUParams(context, params);

	if (GetNumInstances())
		GetComponent()->SpawnParticles(context);

	// Get data of parent particles
	const auto& parentContainer = context.m_parentContainer;
	IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);

	THeapArray<SParentData> parentData(*context.m_pMemHeap, parentContainer.GetNumParticles());

	for (auto parentId : parentContainer.GetFullRange())
	{
		parentData[parentId].position = parentPositions.Load(parentId);
		parentData[parentId].velocity = parentVelocities.Load(parentId);
	}

	m_pGpuRuntime->UpdateData(params, m_spawnEntries, parentData);

	m_spawnEntries.clear();
}

void CParticleComponentRuntime::DebugStabilityCheck()
{
#if 0 // #ifdef CRY_PFX2_DEBUG
	const CParticleContainer& parentContainer = GetParentContainer();
	const TParticleId parentCount = parentContainer.GetLastParticleId();
	IPidStream parentIds = m_container.GetIPidStream(EPDT_ParentId);

	if (!m_container.HasSpawnedParticles())
	{
		for (auto particleId : m_container.GetSpawnedRange())
		{
			TParticleId parentId = parentIds.Load(particleId);
			CRY_PFX2_ASSERT(parentIds.Load(particleId) != gInvalidId);      // recently spawn particles are not supposed to be orphan
		}
	}

	for (auto particleId : m_container.GetFullRange())
	{
		TParticleId parentId = parentIds.Load(particleId);
		CRY_PFX2_ASSERT(parentId < parentCount || parentId == gInvalidId);    // this particle is not pointing to the correct parent
	}

	for (auto instance : m_subInstances)
	{
		TParticleId parentId = instance.m_parentId;
		CRY_PFX2_ASSERT(parentId < parentCount);    // this instance is not pointing to the correct parent
	}
#endif
}

bool CParticleComponentRuntime::IsAlive() const
{
	if (HasParticles())
		return true;
	if (GetNumInstances())
	{
		if (m_pEmitter->GetTime() <= m_pComponent->GetComponentParams().m_emitterLifeTime.end)
			return true;
	}
	return false;
}

bool CParticleComponentRuntime::HasParticles() const
{
	return m_pGpuRuntime ? m_pGpuRuntime->HasParticles() : m_container.GetLastParticleId() != 0; 
}

void CParticleComponentRuntime::AccumStats(SParticleStats& statsCPU, SParticleStats& statsGPU)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (m_pGpuRuntime)
	{
		SParticleStats stats;
		m_pGpuRuntime->AccumStats(stats);
		stats.particles.rendered *= m_particleStats.rendered;
		stats.components.rendered *= m_particleStats.rendered;
		statsGPU += stats;
	}
	else
	{
		statsCPU.particles += m_particleStats;

		const uint allocParticles = uint(m_container.GetMaxParticles());
		const uint aliveParticles = uint(m_container.GetLastParticleId());

		statsCPU.particles.alive += allocParticles;

		statsCPU.components.alive += aliveParticles > 0;
		statsCPU.components.updated += m_particleStats.updated > 0;
		statsCPU.components.rendered += m_particleStats.rendered > 0;

		CParticleProfiler& profiler = GetPSystem()->GetProfiler();
		profiler.AddEntry(this, EPS_ActiveParticles, aliveParticles);
		profiler.AddEntry(this, EPS_AllocatedParticles, allocParticles);
	}

	m_particleStats = {};
}


}
