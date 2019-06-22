// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ParticleComponentRuntime.h"
#include "ParticleEmitter.h"
#include "ParticleSystem.h"
#include "ParticleManager.h"
#include "ParticleProfiler.h"

namespace pfx2
{

MakeDataType(EPVF_PositionPrev, Vec3);

extern TDataType<Vec3> EPVF_Acceleration, EPVF_VelocityField;
extern TDataType<Vec3> EPVF_ParentPosition;
extern TDataType<Quat> EPQF_ParentOrientation;

extern TDataType<SMaxParticleCounts> ESDT_ParticleCounts;

CParticleComponentRuntime::CParticleComponentRuntime(CParticleEmitter* pEmitter, CParticleComponent* pComponent)
	: m_pEmitter(pEmitter)
	, m_pComponent(pComponent)
	, m_bounds(AABB::RESET)
	, m_alive(true)
	, m_deltaTime(-1.0f)
	, m_isPreRunning(false)
	, m_chaos(0)
	, m_chaosV(0)
{
	Initialize();
	if (pComponent->UsesGPU())
	{
		auto GPUParams = pComponent->GPUComponentParams();
		GetMaxParticleCounts(GPUParams.maxParticles, GPUParams.maxNewBorns);
		GPUParams.maxParticles += GPUParams.maxParticles >> 3;
		GPUParams.maxNewBorns  += GPUParams.maxNewBorns  >> 3;
		m_pGpuRuntime = gEnv->pRenderer->GetGpuParticleManager()->CreateParticleContainer(
			GPUParams,
			pComponent->GetGpuFeatures());
	}
}

CParticleComponentRuntime::~CParticleComponentRuntime()
{
	if (Container().Size())
		GetComponent()->DestroyParticles(*this);
}

bool CParticleComponentRuntime::IsValidForComponent() const
{
	if (!m_pComponent->UsesGPU())
		return !m_pGpuRuntime;
	else
		return m_pGpuRuntime && m_pGpuRuntime->IsValidForParams(m_pComponent->GPUComponentParams());
}

uint CParticleComponentRuntime::GetNumParticles() const
{
	if (m_pGpuRuntime)
	{
		SParticleStats stats;
		m_pGpuRuntime->AccumStats(stats);
		return stats.particles.alive;
	}
	else
	{
		return GetContainer().RealSize();
	}
}

void CParticleComponentRuntime::AddBounds(const AABB& bounds)
{
	m_bounds.Add(bounds);
	m_pEmitter->AddBounds(bounds);
}

CParticleComponentRuntime* CParticleComponentRuntime::ParentRuntime() const
{
	return m_pComponent->GetParentComponent() ? m_pEmitter->GetRuntimeFor(m_pComponent->GetParentComponent()) : nullptr;
}

CParticleContainer& CParticleComponentRuntime::ParentContainer(EDataDomain domain)
{
	if (CParticleComponentRuntime* pParentRuntime = ParentRuntime())
		return pParentRuntime->m_containers[domain];
	return GetEmitter()->GetParentContainer();
}

const CParticleContainer& CParticleComponentRuntime::ParentContainer(EDataDomain domain) const
{
	return non_const(this)->ParentContainer(domain);
}

void CParticleComponentRuntime::Initialize()
{
	m_containers[EDD_Particle].SetUsedData(m_pComponent->GetUseData(), EDD_Particle);
	m_containers[EDD_Spawner].SetUsedData(m_pComponent->GetUseData(), EDD_Spawner);
}

void CParticleComponentRuntime::Clear()
{
	Container().Clear();
	RemoveAllSpawners();
	m_deltaTime = -1.0f;
}

void CParticleComponentRuntime::RunParticles(uint count, float deltaTime)
{
	m_deltaTime = deltaTime;
	m_isPreRunning = true;
	
	Container().BeginSpawn();
	SSpawnEntry entry = {};
	entry.m_count = count;
	entry.m_ageBegin = deltaTime;
	AddParticles({&entry, 1});
	InitParticles();
	Container().EndSpawn();

	CalculateBounds();
	m_pEmitter->UpdatePhysEnv();
	m_deltaTime = deltaTime;
	UpdateParticles();
	CalculateBounds();
	m_isPreRunning = false;

	m_deltaTime = -1.0f;
}

void CParticleComponentRuntime::UpdateAll()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	m_alive = false;

	uint32 emitterSeed = m_pEmitter->GetCurrentSeed();
	uint32 componentId = m_pComponent->GetComponentId() << 16;
	uint32 key = emitterSeed + componentId + Container().Size();
	m_chaos = SChaosKey(key);
	m_chaosV = SChaosKeyV(key);

	if (GetGpuRuntime())
		return UpdateGPURuntime();

	AddRemoveParticles();
	if (HasParticles())
	{
		SetAlive();
		UpdateParticles();
		CalculateBounds();
	}
	else
	{
		m_bounds.Reset();
	}
	AccumStats();
}

void CParticleComponentRuntime::AddRemoveParticles()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	CTimeProfiler profile(GetPSystem()->GetProfiler(), *this, EPS_NewBornTime);

	UpdateSpawners();
		
	Container().BeginSpawn();
	RemoveParticles();
	AgeUpdate();
	if (Container(EDD_Spawner).Size())
	{
		SetAlive();
		GetComponent()->SpawnParticles(*this);
		InitParticles();
	}
	Container().EndSpawn();
}

void CParticleComponentRuntime::UpdateParticles()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	CTimeProfiler profile(GetPSystem()->GetProfiler(), *this, EPS_UpdateTime);

	Container().FillData(EPVF_Acceleration, Vec3(0), FullRange());
	Container().FillData(EPVF_VelocityField, Vec3(0), FullRange());
	Container().CopyData(EPVF_PositionPrev, EPVF_Position, FullRange());

	GetComponent()->PreUpdateParticles(*this);
	GetComponent()->UpdateParticles(*this);
	GetComponent()->PostUpdateParticles(*this);
}

void CParticleComponentRuntime::ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	CTimeProfiler profile(GetPSystem()->GetProfiler(), *this, EPS_ComputeVerticesTime);

	GetEmitter()->SyncUpdateParticles();

	if (camInfo.pCamera->IsAABBVisible_E(m_bounds))
		GetComponent()->ComputeVertices(*this, camInfo, pRE, uRenderFlags, fMaxPixels);
}

void CParticleComponentRuntime::RemoveAllSpawners()
{
	Container(EDD_Spawner).Clear();
	DebugStabilityCheck();
}

void CParticleComponentRuntime::Reparent(TConstArray<TParticleId> swapIds)
{
	Container(EDD_Particle).Reparent(swapIds, EPDT_ParentId);
	Container(EDD_Spawner).Reparent(swapIds, ESDT_ParentId, true);
	DebugStabilityCheck();
}

extern TDataType<Vec3> ESDT_EmitOffset;

void CParticleComponentRuntime::GetEmitLocations(TVarArray<QuatTS> locations, uint firstInstance) const
{
	auto const& parentContainer = ParentContainer();
	auto parentIds = IStream(ESDT_ParentId);
	auto parentPositions = parentContainer.GetIVec3Stream(EPVF_Position, GetEmitter()->GetLocation().t);
	auto parentRotations = parentContainer.GetIQuatStream(EPQF_Orientation, GetEmitter()->GetLocation().q);

	SDynamicData<Vec3> offsets(*this, ESDT_EmitOffset, EDD_Spawner, SUpdateRange(firstInstance, firstInstance + locations.size()));

	for (uint idx = 0; idx < locations.size(); ++idx)
	{
		QuatTS& loc = locations[idx];
		const TParticleId parentId = parentIds[idx];
		loc.t = parentPositions.SafeLoad(parentId);
		loc.q = parentRotations.SafeLoad(parentId);
		loc.s = 1.0f;
		loc.t = loc * offsets[idx];
	}
}

void CParticleComponentRuntime::EmitParticle()
{
	SSpawnEntry spawn = {1, ParentContainer().Size()};
	AddParticles({&spawn, 1});
	Container().EndSpawn();
}

void CParticleComponentRuntime::UpdateSpawners()
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = m_containers[EDD_Spawner];

	container.BeginSpawn();
	GetComponent()->UpdateSpawners(*this);
	if (container.HasNewBorns())
		GetComponent()->InitSpawners(*this);
	container.EndSpawn();

	DebugStabilityCheck();
}

void CParticleComponentRuntime::AddSpawners(TVarArray<SSpawnerDesc> spawners, bool cull)
{
	if (cull)
		GetComponent()->CullSpawners(*this, spawners);
	if (spawners.empty())
		return;

	CParticleContainer& container = m_containers[EDD_Spawner];
	container.AddElements(spawners.size());

	auto parentIds = IOStream(ESDT_ParentId);
	auto ages = IOStream(ESDT_Age);
	float dT = DeltaTime();

	uint id = container.LastSpawned() - spawners.size();
	for (auto const& spawner : spawners)
	{
		parentIds[id] = spawner.m_parentId;
		ages[id] = dT - spawner.m_startDelay;
		id++;
	}
}

void CParticleComponentRuntime::AddParticles(TConstArray<SSpawnEntry> spawnEntries)
{
	CRY_PFX2_PROFILE_DETAIL;

	if (m_pGpuRuntime)
	{
		m_GPUSpawnEntries.append(spawnEntries);
		return;
	}

	uint newCount = 0;
	for (const auto& spawnEntry : spawnEntries)
		newCount += spawnEntry.m_count;
	if (newCount == 0)
		return;

	Container().AddElements(newCount);

	auto parentIds = Container().IOStream(EPDT_ParentId);
	auto normAges = Container().IOStream(EPDT_NormalAge);
	auto fractions = Container().IOStream(EPDT_SpawnFraction);

	SUpdateRange range = Container().SpawnedRange();
	for (const auto& spawnEntry : spawnEntries)
	{
		range.m_end = CRY_PFX2_GROUP_ALIGN(range.m_begin + spawnEntry.m_count);

		if (parentIds.IsValid())
		{
			for (auto id : range)
				parentIds[id] = spawnEntry.m_parentId;
		}
		if (normAges.IsValid())
		{
			float age = spawnEntry.m_ageBegin;
			for (auto id : range)
			{
				normAges[id] = max(age, 0.0f);
				age += spawnEntry.m_ageIncrement;
			}
		}
		if (fractions.IsValid())
		{
			float fraction = spawnEntry.m_fractionBegin;
			for (auto id : range)
			{
				fractions[id] = min(fraction, 1.0f);
				fraction += spawnEntry.m_fractionIncrement;
			}
		}

		range.m_begin += spawnEntry.m_count;
	}
}

uint CParticleComponentRuntime::DomainSize(EDataDomain domain) const
{
	if (domain & (EDD_Particle | EDD_Spawner))
		return m_containers[domain].Size();
	else
		return 1;
}

void CParticleComponentRuntime::RemoveParticles()
{
	CRY_PFX2_PROFILE_DETAIL;

	const uint32 numParticles = Container().Size();
	IOFStream normAges = Container().GetIOFStream(EPDT_NormalAge);

	TParticleIdArray removeIds(MemHeap());
	removeIds.reserve(numParticles);

	GetComponent()->KillParticles(*this);

	for (auto particleId : FullRange())
	{
		const float normalAge = normAges.Load(particleId);
		if (IsExpired(normalAge))
			removeIds.push_back(particleId);
	}

	if (!removeIds.empty())
	{
		TParticleIdArray swapIds(MemHeap());
		const bool hasChildren = !m_pComponent->GetChildComponents().empty();
		if (hasChildren)
		{
			swapIds.resize(numParticles);
			Container().MakeSwapIds(removeIds, swapIds);
			for (const auto& pChild : m_pComponent->GetChildComponents())
			{
				if (auto pSubRuntime = m_pEmitter->GetRuntimeFor(pChild))
					pSubRuntime->Reparent(swapIds);
			}
		}

		Container().RemoveElements(removeIds);
	}
}

void CParticleComponentRuntime::InitParticles()
{
	CRY_PFX2_PROFILE_DETAIL;

	if (!Container().NumSpawned())
		return;

	// Initialize SpawnIds
	if (Container().HasData(EPDT_SpawnId))
	{
		auto spawnIds = Container().IOStream(EPDT_SpawnId);
		uint32 spawnId = Container().TotalSpawned();
		for (auto id : SpawnedRange())
			spawnIds[id] = spawnId++;
	}

	// Initialize Random
	if (Container().HasData(EPDT_Random))
	{
		IOFStream unormRands = Container().GetIOFStream(EPDT_Random);
		for (auto particleGroupId : SpawnedRangeV())
		{
			const floatv unormRand = ChaosV().RandUNorm();
			unormRands.Store(particleGroupId, unormRand);
		}
	}

	// Zero velocity
	Container().FillData(EPVF_Velocity, Vec3(0), SpawnedRange());

	GetComponent()->PreInitParticles(*this);

	CParticleContainer& parentContainer = ParentContainer();

	// interpolate position and normAge over time and velocity
	const IPidStream parentIds = Container().GetIPidStream(EPDT_ParentId);
	const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	const IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);
	const IQuatStream parentOrientations = parentContainer.GetIQuatStream(EPQF_Orientation, GetEmitter()->GetLocation().q);
	const IVec3Stream parentAngularVelocities = parentContainer.GetIVec3Stream(EPVF_AngularVelocity);
	const IFStream parentNormAges = parentContainer.GetIFStream(EPDT_NormalAge);
	const IFStream parentLifeTimes = parentContainer.GetIFStream(EPDT_LifeTime);

	IOVec3Stream parentPrevPositions = Container().IOStream(EPVF_ParentPosition);
	IOQuatStream parentPrevOrientations = Container().IOStream(EPQF_ParentOrientation);

	IOVec3Stream positions = Container().GetIOVec3Stream(EPVF_Position);
	IOQuatStream orientations = Container().GetIOQuatStream(EPQF_Orientation);

	IOFStream normAges = Container().GetIOFStream(EPDT_NormalAge);
	IFStream lifeTimes = Container().GetIFStream(EPDT_LifeTime);

	const bool checkParentLife = IsChild();

	for (auto particleGroupId : SpawnedRangeV())
	{
		// Convert absolute spawned particle age to normal age / life
		floatv backTime = -normAges.Load(particleGroupId) * lifeTimes.Load(particleGroupId);

		// Set initial position and orientation from parent
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

		if (Container().HasData(EPVF_ParentPosition))
			parentPrevPositions.Store(particleGroupId, wPosition);

		if (Container().HasData(EPQF_Orientation) || Container().HasData(EPQF_ParentOrientation))
		{
			Quatv wParentQuat = parentOrientations.SafeLoad(parentGroupId);
			if (Container().HasData(EPVF_AngularVelocity))
			{
				const Vec3v parentAngularVelocity = parentAngularVelocities.SafeLoad(parentGroupId);
				wParentQuat = AddAngularVelocity(wParentQuat, parentAngularVelocity, backTime);
			}
			if (Container().HasData(EPQF_Orientation))
				orientations.Store(particleGroupId, wParentQuat);

			if (Container().HasData(EPQF_ParentOrientation))
				parentPrevOrientations.Store(particleGroupId, wParentQuat);
		}
	}

	// feature init particles
	GetComponent()->InitParticles(*this);

	// modify with spawn params
	const SpawnParams& spawnParams = GetEmitter()->GetSpawnParams();
	if (spawnParams.fSizeScale != 1.0f && Container().HasData(EPDT_Size))
	{
		const floatv scalev = ToFloatv(spawnParams.fSizeScale);
		IOFStream sizes = Container().GetIOFStream(EPDT_Size);
		for (auto particleGroupId : SpawnedRangeV())
		{
			const floatv size0 = sizes.Load(particleGroupId);
			const floatv size1 = size0 * scalev;
			sizes.Store(particleGroupId, size1);
		}
		Container().CopyData(EPDT_Size.InitType(), EPDT_Size, SpawnedRange());
	}
	if (spawnParams.fSpeedScale != 1.0f && Container().HasData(EPVF_Velocity))
	{
		const floatv scalev = ToFloatv(spawnParams.fSpeedScale);
		IOVec3Stream velocities = Container().GetIOVec3Stream(EPVF_Velocity);
		for (auto particleGroupId : SpawnedRangeV())
		{
			const Vec3v velocity0 = velocities.Load(particleGroupId);
			const Vec3v velocity1 = velocity0 * scalev;
			velocities.Store(particleGroupId, velocity1);
		}
	}

	// feature post init particles
	GetComponent()->PostInitParticles(*this);

	if (ComponentParams().m_isPreAged)
	{
		m_deltaTime = max(GetEmitter()->GetDeltaTime(), ComponentParams().m_maxParticleLife);
		GetComponent()->PastUpdateParticles(*this);
		m_deltaTime = -1.0f;
	}
}

void CParticleComponentRuntime::CalculateBounds()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	CTimeProfiler profile(GetPSystem()->GetProfiler(), *this, EPS_UpdateTime);

	IVec3Stream positions = Container().GetIVec3Stream(EPVF_Position);
	IFStream sizes = Container().GetIFStream(EPDT_Size);
	const float sizeScale = ComponentParams().m_scaleParticleSize;

	SUpdateRange range = Container().FullRange();

#ifdef CRY_PFX2_USE_SSE
	const floatv fMin = ToFloatv(std::numeric_limits<float>::max());
	const floatv fMax = ToFloatv(-std::numeric_limits<float>::max());
	Vec3v bbMin = Vec3v(fMin, fMin, fMin);
	Vec3v bbMax = Vec3v(fMax, fMax, fMax);

	// vector part
	const floatv scalev = convert<floatv>(sizeScale);
	const TParticleId lastParticleId = Container().Size();
	const TParticleGroupId lastParticleGroupId { lastParticleId & ~(CRY_PFX2_GROUP_STRIDE - 1) };
	for (auto particleGroupId : SGroupRange(TParticleGroupId(0), lastParticleGroupId))
	{
		const floatv size = sizes.Load(particleGroupId) * scalev;
		const Vec3v position = positions.Load(particleGroupId);
		bbMin = min(bbMin, Sub(position, size));
		bbMax = max(bbMax, Add(position, size));
	}
	m_bounds.min = HMin(bbMin);
	m_bounds.max = HMax(bbMax);

	range = SUpdateRange(+lastParticleGroupId, lastParticleId);
#else
	m_bounds.Reset();
#endif

	// linear part
	for (auto particleId : range)
	{
		const float size = sizes.Load(particleId) * sizeScale;
		const Vec3 position = positions.Load(particleId);
		m_bounds.Add(position, size);
	}

	CRY_PFX2_ASSERT(m_bounds.GetRadius() < 1000000.f);
}

void CParticleComponentRuntime::AgeUpdate()
{
	CRY_PFX2_PROFILE_DETAIL;

	IFStream invLifeTimes = Container().GetIFStream(EPDT_InvLifeTime);
	IOFStream normAges = Container().GetIOFStream(EPDT_NormalAge);
	const floatv frameTime = ToFloatv(DeltaTime());

	for (auto particleGroupId : FullRangeV())
	{
		const floatv invLifeTime = invLifeTimes.Load(particleGroupId);
		const floatv normAge0 = normAges.Load(particleGroupId);
		const floatv normAge1 = normAge0 + frameTime * invLifeTime;
		normAges.Store(particleGroupId, normAge1);
	}
}

void CParticleComponentRuntime::GetMaxParticleCounts(int& total, int& perFrame, float minFPS, float maxFPS) const
{
	SMaxParticleCounts counts;
	m_pComponent->GetDynamicData(*this, ESDT_ParticleCounts, &counts, EDD_None, SUpdateRange(0,1));

	total = counts.burst;
	const float rate = counts.rate + counts.perFrame * maxFPS;
	const float extendedLife = ComponentParams().m_maxParticleLife + rcp(minFPS); // Particles stay 1 frame after death
	if (rate > 0.0f && valueisfinite(extendedLife))
		total += int_ceil(rate * extendedLife);
	perFrame = int(counts.burst + counts.perFrame) + int_ceil(counts.rate / minFPS);

	if (auto parent = ParentRuntime())
	{
		int totalParent, perFrameParent;
		parent->GetMaxParticleCounts(totalParent, perFrameParent, minFPS, maxFPS);
		total *= totalParent;
		perFrame *= totalParent;
	}
}

void CParticleComponentRuntime::UpdateGPURuntime()
{
	if (!m_pGpuRuntime)
		return;

	UpdateSpawners();
	if (Container(EDD_Spawner).Size())
	{
		SetAlive();
		GetComponent()->SpawnParticles(*this);
	}

	// Accum stats
	SParticleStats stats;
	m_pGpuRuntime->AccumStats(stats);
	stats.particles.rendered = stats.components.rendered = 0;
	GetPSystem()->GetThreadData().statsGPU += stats;

	if (stats.particles.alive)
		SetAlive();

	gpu_pfx2::SUpdateParams params;

	params.deltaTime = DeltaTime();
	params.emitterPosition = m_pEmitter->GetLocation().t;
	params.emitterOrientation = m_pEmitter->GetLocation().q;
	params.physAccel = m_pEmitter->GetPhysicsEnv().m_UniformForces.vAccel;
	params.physWind = m_pEmitter->GetPhysicsEnv().m_UniformForces.vWind;

	GetComponent()->UpdateGPUParams(*this, params);

	// Get data of parent particles
	const auto& parentContainer = ParentContainer();
	IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);

	THeapArray<SParentData> parentData(MemHeap(), parentContainer.GetNumParticles());

	for (auto parentId : parentContainer.FullRange())
	{
		parentData[parentId].position = parentPositions.Load(parentId);
		parentData[parentId].velocity = parentVelocities.Load(parentId);
	}

	m_pGpuRuntime->UpdateData(params, m_GPUSpawnEntries, parentData);
	m_GPUSpawnEntries.resize(0);
}

void CParticleComponentRuntime::DebugStabilityCheck()
{
#ifdef CRY_PFX2_DEBUG
	const CParticleContainer& parentContainer = ParentContainer();
	const TParticleId parentCount = parentContainer.RealSize();
	IPidStream parentIds = IStream(EPDT_ParentId);

	if (Container().HasNewBorns())
	{
		for (auto particleId : SpawnedRange())
		{
			TParticleId parentId = parentIds.Load(particleId);
			CRY_PFX2_ASSERT(parentId != gInvalidId);  // newborn particles cannot be orphans
		}
	}

	for (auto particleId : FullRange())
	{
		TParticleId parentId = parentIds.Load(particleId);
		CRY_PFX2_ASSERT(parentId < parentCount || parentId == gInvalidId);   // this particle is not pointing to the correct parent
	}

	parentIds = IStream(ESDT_ParentId);
	for (auto id : FullRange(EDD_Spawner))
	{
		TParticleId parentId = parentIds.Load(id);
		CRY_PFX2_ASSERT(parentId < parentCount || parentId == gInvalidId);   // this spawner is not pointing to the correct parent
	}
#endif
}

bool CParticleComponentRuntime::HasParticles() const
{
	return m_pGpuRuntime ? m_pGpuRuntime->HasParticles() : Container().RealSize() != 0; 
}

void CParticleComponentRuntime::AccumStats()
{
	const uint allocParticles = Container().Capacity();
	const uint aliveParticles = Container().Size();

	auto& stats = GetPSystem()->GetThreadData().statsCPU;
	stats.components.alive += IsAlive();
	stats.components.updated ++;
	stats.spawners.alloc += Container(EDD_Spawner).Capacity();
	stats.spawners.alive += Container(EDD_Spawner).Size();
	stats.spawners.updated += Container(EDD_Spawner).Size();
	stats.particles.alloc += allocParticles;
	stats.particles.alive += aliveParticles;
	stats.particles.updated += aliveParticles;

	CParticleProfiler& profiler = GetPSystem()->GetProfiler();
	profiler.AddEntry(*this, EPS_ActiveParticles, aliveParticles);
	profiler.AddEntry(*this, EPS_AllocatedParticles, allocParticles);
}

pfx2::STimingParams CParticleComponentRuntime::GetMaxTimings() const
{
	if (m_pComponent->GetChildComponents().empty())
		return ComponentParams();

	STimingParams timings = ComponentParams();

	// Adjust lifetimes to include child lifetimes
	float maxChildEq = 0.0f, maxChildLife = 0.0f;
	for (const auto& pChild : m_pComponent->GetChildComponents())
	{
		if (auto pSubRuntime = m_pEmitter->GetRuntimeFor(pChild))
		{
			STimingParams timingsChild = pSubRuntime->GetMaxTimings();
			SetMax(maxChildEq, timingsChild.m_equilibriumTime);
			SetMax(maxChildLife, timingsChild.m_maxTotalLife);
		}
	}

	const float moreEq = maxChildEq - FiniteOr(timings.m_maxParticleLife, 0.0f);
	if (moreEq > 0.0f)
	{
		timings.m_stableTime += moreEq;
		timings.m_equilibriumTime += moreEq;
	}
	const float moreLife = maxChildLife - FiniteOr(timings.m_maxParticleLife, 0.0f);
	if (moreLife > 0.0f)
	{
		timings.m_maxTotalLife += moreLife;
	}

	return timings;
}

pfx2::TParticleHeap& CParticleComponentRuntime::MemHeap()
{
	return GetPSystem()->GetThreadData().memHeap;
}


CRenderObject* CParticleComponentRuntime::CreateRenderObject(uint64 renderFlags) const
{
	CRY_PFX2_PROFILE_DETAIL;
	const SComponentParams& params = ComponentParams();
	CRenderObject* pRenderObject = gEnv->pRenderer->EF_GetObject();
	auto particleMaterial = params.m_pMaterial;

	pRenderObject->SetIdentityMatrix();
	pRenderObject->m_fAlpha = 1.0f;
	pRenderObject->m_pCurrMaterial = particleMaterial;
	pRenderObject->m_pRenderNode = m_pEmitter;
	pRenderObject->m_ObjFlags = ERenderObjectFlags(renderFlags & ~0xFF);
	pRenderObject->m_RState = uint8(renderFlags);
	pRenderObject->m_fSort = 0;
	pRenderObject->m_pRE = gEnv->pRenderer->EF_CreateRE(eDATA_Particle);

	SRenderObjData* pObjData = pRenderObject->GetObjData();
	pObjData->m_pParticleShaderData = &params.m_shaderData;

	return pRenderObject;
}

CRenderObject* CParticleComponentRuntime::GetRenderObject(uint threadId, ERenderObjectFlags extraFlags)
{
	// Determine needed render flags from component params, render context, and cvars
	auto allowedRenderFlags = CParticleManager::Instance()->GetRenderFlags();
	const SComponentParams& params = ComponentParams();
	uint64 curRenderFlags = allowedRenderFlags & (params.m_renderObjectFlags | params.m_renderStateFlags | extraFlags);

	for (;;)
	{
		PRenderObject& pRO = m_renderObjects[threadId].append();
		if (pRO)
		{
			auto objRenderFlags = (pRO->m_ObjFlags & ~0xFF) | pRO->m_RState;
			if (objRenderFlags == curRenderFlags)
				return pRO;
		}
		else
		{
			pRO = CreateRenderObject(curRenderFlags);
			return pRO;
		}
	}
}

float CParticleComponentRuntime::DeltaTime() const
{
	return m_deltaTime >= 0.0f ? m_deltaTime : m_pEmitter->GetDeltaTime();
}

}
