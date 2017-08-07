// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuParticleComponentRuntime.h"
#include "GpuParticleManager.h"
#include "Gpu/GpuMergeSort.h"
#include "CompiledRenderObject.h"

#include "Gpu/Physics/GpuPhysicsParticleFluid.h"

#include <CryCore/BitFiddling.h>

namespace gpu_pfx2
{
const int CParticleComponentRuntime::kThreadsInBlock = 1024;

static pfx2::EUpdateList s_updateListmapping[] = { pfx2::EUL_Spawn, pfx2::EUL_InitUpdate, pfx2::EUL_Update, pfx2::EUL_InitSubInstance };

void CParticleComponentRuntime::AddRemoveNewBornsParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (!m_container.HasDefaultParticleData())
		return;

	// this means that the pipeline has already run, we get the kill list
	// and the amount of dead particles from the last frame, kill the particles
	// and swap them out by living ones at the end
	if (m_parameters->numKilled)
		SwapToEnd(context, commandList);

	{
		// this is the only place where there needs to be a lock during the render
		// thread processing, because here, the sub instance data is actually passed to the
		// render thread
		CryAutoLock<CryCriticalSection> lock(m_cs);
		m_subInstancesRenderThread = m_subInstances;

		m_parentDataSizeRenderThread = m_parentData.size();
		if (m_parentDataSizeRenderThread)
		{
			m_parentDataRenderThread.UpdateBufferContentAligned(&m_parentData[0], m_parentDataSizeRenderThread);
		}
	}

	AddRemoveParticles(context);
	if (m_parameters->numNewBorns)
		UpdateNewBorns(context, commandList);
}

void CParticleComponentRuntime::UpdateParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (!m_parameters->numParticles)
		return;

	m_parameters->deltaTime = context.deltaTime;
	m_parameters->currentTime += context.deltaTime;
	m_parameters->physAccel = m_envParams.physAccel;
	m_parameters->physWind = m_envParams.physWind;
	m_parameters->viewProjection = gcpRendD3D->m_CameraProjMatrix;
	m_parameters->sortMode = m_params.sortMode;

	const CCamera& cam = gEnv->p3DEngine->GetRenderingCamera();
	float zn = cam.GetNearPlane();
	float zf = cam.GetFarPlane();
	m_parameters->farToNearDistance = zf - zn;
	m_parameters->cameraPosition = cam.GetPosition();
	m_parameters.CopyToDevice();

	if (m_params.sortMode != ESortMode::None)
		Sort(context, commandList);

	UpdateFeatures(context, commandList);
	FillKillList(context, commandList);
}

void CParticleComponentRuntime::CalculateBounds(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (CRenderer::CV_r_GpuParticlesConstantRadiusBoundingBoxes > 0)
	{
		m_bounds = AABB(m_parameters->emitterPosition, (float)CRenderer::CV_r_GpuParticlesConstantRadiusBoundingBoxes);
		return;
	}

	if (!m_parameters->numParticles)
	{
		m_bounds = AABB::RESET;
		return;
	}

	m_passCalcBounds.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());
	m_passCalcBounds.SetOutputUAV(3, context.pReadbackBuffer);

	m_passCalcBounds.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());
	m_passCalcBounds.SetDispatchSize(kCalcBoundsBlocks, 1, 1);

	m_passCalcBounds.PrepareResourcesForUse(commandList);
	m_passCalcBounds.Execute(commandList);
}

CGpuBuffer& CParticleComponentRuntime::PrepareForUse()
{
	return m_container.GetDefaultParticleDataBuffer();
}

void CParticleComponentRuntime::SetBoundsFromManager(const SReadbackData* pData)
{
	if (m_parameters->numParticles && m_parameters->managerSlot >= 0)
	{
		m_bounds.min.x = (float)pData[m_parameters->managerSlot].min.x / kBoundsScale;
		m_bounds.min.y = (float)pData[m_parameters->managerSlot].min.y / kBoundsScale;
		m_bounds.min.z = (float)pData[m_parameters->managerSlot].min.z / kBoundsScale;
		m_bounds.max.x = (float)pData[m_parameters->managerSlot].max.x / kBoundsScale;
		m_bounds.max.y = (float)pData[m_parameters->managerSlot].max.y / kBoundsScale;
		m_bounds.max.z = (float)pData[m_parameters->managerSlot].max.z / kBoundsScale;
	}
	else
	{
		m_bounds = AABB::RESET;
	}
}

void CParticleComponentRuntime::SetCounterFromManager(const uint32* pCounter)
{
	if (m_parameters->managerSlot >= 0)
		m_parameters->numKilled = pCounter[m_parameters->managerSlot];
	else
		m_parameters->numKilled = 0;
}

void CParticleComponentRuntime::SpawnParticles(int instanceId, uint32 count)
{
	if (count == 0)
		return;

	SSpawnEntry entry;
	entry.m_count = count;
	entry.m_instanceId = instanceId;
	m_spawnEntries.push_back(entry);
}

void CParticleComponentRuntime::SetUpdateBuffer(EFeatureUpdateSrvSlot slot, CGpuBuffer* pGpuBuffer)
{
	m_updateSrvSlots[slot] = pGpuBuffer;
}

void CParticleComponentRuntime::SetUpdateTexture(EFeatureUpdateSrvSlot slot, CTexture* pTexture)
{
	m_updateTextureSlots[slot] = pTexture;
}

void CParticleComponentRuntime::SetUpdateFlags(uint64 flags)
{
	m_updateShaderFlags |= flags;
}

void CParticleComponentRuntime::SetUpdateConstantBuffer(EConstantBufferSlot slot, CConstantBufferPtr pConstantBuffer)
{
	m_updateConstantBuffers[slot] = pConstantBuffer;
}

void CParticleComponentRuntime::AddRemoveParticles(const SUpdateContext& context)
{
	// spawn
	if (m_active)
	{
		for (CFeature* feature : m_gpuUpdateLists[eGpuUpdateList_Spawn])
		{
			feature->SpawnParticles(context);
		}
	}
	// now we have a spawn list in m_spawnEntries
	m_parameters->numNewBorns = 0;
	for (int i = 0; i < m_spawnEntries.size(); ++i)
	{
		m_parameters->numNewBorns += m_spawnEntries[i].m_count;
	}

	stl::aligned_vector<uint, CRY_PLATFORM_ALIGNMENT> newBornData;
	newBornData.resize(m_parameters->numNewBorns);
	int index = 0;
	for (int i = 0; i < m_spawnEntries.size(); ++i)
	{
		for (int c = 0; c < m_spawnEntries[i].m_count; ++c)
		{
			newBornData[index] = std::min(m_spawnEntries[i].m_instanceId, m_parentDataSizeRenderThread - 1);
			index++;
		}
	}

	// cap because of size of newborn array
	m_parameters->numNewBorns =
	  min(m_params.maxNewBorns, m_parameters->numNewBorns);
	// cap global
	m_parameters->numNewBorns =
	  min(m_params.maxParticles - m_parameters->numParticles, m_parameters->numNewBorns);

	m_parameters->numParticles += m_parameters->numNewBorns;

	if (m_parameters->numNewBorns)
		m_newBornIndices.UpdateBufferContentAligned(&newBornData[0], m_parameters->numNewBorns);

	m_parameters.CopyToDevice();

	m_spawnEntries.resize(0);
}

void CParticleComponentRuntime::UpdateNewBorns(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	m_initializationShaderFlags = 0;

	m_particleInitializationParameters->velocityScale = 0.0f;

	for (CFeature* feature : m_gpuUpdateLists[eGpuUpdateList_InitUpdate])
	{
		feature->InitParticles(context);
	}

	UpdatePasses();

	for (int i = 0; i < eFeatureInitializationSrvSlot_COUNT; ++i)
	{
		if (m_initializationSrvSlots[i])
			m_passFeatureInitialization.SetBuffer(i, m_initializationSrvSlots[i]);
	}
	m_passFeatureInitialization.SetBuffer(eFeatureInitializationSrvSlot_newBornIndices, &m_newBornIndices.GetBuffer());
	m_passFeatureInitialization.SetBuffer(eFeatureInitializationSrvSlot_parentData, &m_parentDataRenderThread.GetBuffer());

	m_passFeatureInitialization.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());
	m_passFeatureInitialization.SetOutputUAV(1, context.pReadbackBuffer);

	m_passFeatureInitialization.SetSampler(0, EDefaultSamplerStates::BilinearClamp);
	m_passFeatureInitialization.SetSampler(1, EDefaultSamplerStates::PointClamp);

	m_particleInitializationParameters.CopyToDevice();
	m_parameters.CopyToDevice();

	m_passFeatureInitialization.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());
	m_passFeatureInitialization.SetInlineConstantBuffer(eConstantBufferSlot_Initialization, m_particleInitializationParameters.GetDeviceConstantBuffer());

	const int blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(
	  m_parameters->numNewBorns,
	  kThreadsInBlock);
	if (blocks == 0)
		CryFatalError("");
	m_passFeatureInitialization.SetDispatchSize(blocks, 1, 1);

	m_passFeatureInitialization.PrepareResourcesForUse(commandList);
	m_passFeatureInitialization.Execute(commandList);
}

void CParticleComponentRuntime::FillKillList(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	m_passFillKillList.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());
	m_passFillKillList.SetOutputUAV(1, &m_killList.GetBuffer());
	m_passFillKillList.SetOutputUAV(2, context.pCounterBuffer);
	m_passFillKillList.SetOutputUAV(3, context.pReadbackBuffer);
	m_parameters.CopyToDevice();
	m_passFillKillList.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());

	const int blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(
	  GetNumParticles(),
	  kThreadsInBlock);

	m_passFillKillList.SetDispatchSize(blocks, 1, 1);

	m_passFillKillList.PrepareResourcesForUse(commandList);
	m_passFillKillList.Execute(commandList);
}

void CParticleComponentRuntime::UpdateFeatures(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	m_updateShaderFlags = 0;

	for (CFeature* feature : m_gpuUpdateLists[eGpuUpdateList_Update])
	{
		feature->Update(context, commandList);
	}

	UpdatePasses();

	for (int i = 0; i < eFeatureUpdateSrvSlot_COUNT; ++i)
	{
		if (m_updateSrvSlots[i])
			m_passFeatureUpdate.SetBuffer(i, m_updateSrvSlots[i]);
		else if (m_updateTextureSlots[i])
			m_passFeatureUpdate.SetTexture(i, m_updateTextureSlots[i]);
	}

	m_passFeatureUpdate.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());

	const int blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(
	  GetNumParticles(),
	  kThreadsInBlock);

	m_passFeatureUpdate.SetSampler(0, EDefaultSamplerStates::BilinearClamp);
	m_passFeatureUpdate.SetSampler(1, EDefaultSamplerStates::PointClamp);

	for (int i = 0; i < eConstantBufferSlot_COUNT; ++i)
	{
		if (m_updateConstantBuffers[i])
			m_passFeatureUpdate.SetInlineConstantBuffer(i, m_updateConstantBuffers[i]);
	}

	m_parameters.CopyToDevice();
	m_passFeatureUpdate.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());
	m_passFeatureUpdate.SetDispatchSize(blocks, 1, 1);
	m_passFeatureUpdate.PrepareResourcesForUse(commandList);
	m_passFeatureUpdate.Execute(commandList);
}

void CParticleComponentRuntime::Sort(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	m_parameters.CopyToDevice();

	const int processSlots = NextPower2(GetNumParticles());
	const int blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(processSlots, kThreadsInBlock);

	{
		m_passPrepareSort.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());
		m_passPrepareSort.SetOutputUAV(1, &m_pMergeSort->GetBuffer());
		m_passPrepareSort.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());

		m_passPrepareSort.SetDispatchSize(blocks, 1, 1);
		m_passPrepareSort.PrepareResourcesForUse(commandList);
		m_passPrepareSort.Execute(commandList);
	}

	m_pMergeSort->Sort(processSlots, commandList);

	{
		m_passReorderParticles.SetBuffer(1, &m_pMergeSort->GetBuffer());
		m_passReorderParticles.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());
		m_passReorderParticles.SetOutputUAV(1, &m_container.GetDefaultParticleDataBackBuffer());
		m_passReorderParticles.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());

		m_passReorderParticles.SetDispatchSize(blocks, 1, 1);
		m_passReorderParticles.PrepareResourcesForUse(commandList);
		m_passReorderParticles.Execute(commandList);
	}

	m_container.Swap();
}

void CParticleComponentRuntime::UpdatePasses()
{
	if (m_initializationShaderFlags != m_previousInitializationShaderFlags)
	{
		m_pInitShader.reset();
		CShader* pShader = gcpRendD3D.m_cEF.mfForName("GpuParticles", 0, 0, m_initializationShaderFlags);
		m_pInitShader.Assign_NoAddRef(pShader);
		m_passFeatureInitialization.SetTechnique(m_pInitShader, CCryNameTSCRC("FeatureInitialization"), 0);
		m_previousInitializationShaderFlags = m_initializationShaderFlags;
	}
	if (m_updateShaderFlags != m_previousUpdateShaderShaderFlags)
	{
		m_pUpdateShader.reset();
		CShader* pShader = gcpRendD3D.m_cEF.mfForName("GpuParticles", 0, 0, m_updateShaderFlags);
		m_pUpdateShader.Assign_NoAddRef(pShader);
		m_passFeatureUpdate.SetTechnique(m_pUpdateShader, CCryNameTSCRC("FeatureUpdate"), 0);
		m_previousUpdateShaderShaderFlags = m_updateShaderFlags;
	}
}

bool CParticleComponentRuntime::IsValidForParams(const SComponentParams& parameters)
{
	if (
	  parameters.usesGpuImplementation &&
	  parameters.maxNewBorns == m_params.maxNewBorns &&
	  parameters.maxParticles == m_params.maxParticles &&
	  parameters.sortMode == m_params.sortMode &&
	  parameters.facingMode == m_params.facingMode &&
	  parameters.version == m_params.version
	)
		return true;
	return false;
}

const AABB& CParticleComponentRuntime::GetBounds() const
{
	return m_bounds;
}

void CParticleComponentRuntime::SetInitializationSRV(EFeatureInitializationSrvSlot slot, CGpuBuffer* pGpuBuffer)
{
	m_initializationSrvSlots[slot] = pGpuBuffer;
}

void CParticleComponentRuntime::SetInitializationFlags(uint64 flags)
{
	m_initializationShaderFlags |= flags;
}

CParticleComponentRuntime::CParticleComponentRuntime(IParticleEmitter* pEmitter, pfx2::IParticleComponent* pComponent, const SComponentParams& params)
	: m_bounds(AABB::RESET)
	, m_active(true)
	, m_state(IParticleComponentRuntime::EState::Uninitialized)
	, m_blockSums(params.maxParticles)
	, m_killList(params.maxParticles)
	, m_newBornIndices(params.maxNewBorns)
	, m_parentDataRenderThread(params.maxNewBorns)
	, m_parentDataSizeRenderThread(0)
	, m_pComponent(pComponent)
	, m_params(params)
	, m_container(params.maxParticles)
	, m_updateShaderFlags(0)
	, m_previousUpdateShaderShaderFlags(std::numeric_limits<uint64>::max())
	, m_initializationShaderFlags(0)
	, m_previousInitializationShaderFlags(std::numeric_limits<uint64>::max())
	, m_pEmitter(pEmitter)
{
	assert(m_pEmitter);
	for (int i = 0; i < eGpuUpdateList_COUNT; ++i)
	{
		m_gpuUpdateLists[i].resize(0);
		auto list = pComponent->GetGpuUpdateList(s_updateListmapping[i]);
		for (auto feature: list)
			m_gpuUpdateLists[i].push_back(static_cast<CFeature*>(feature));
	}

	memset(m_updateSrvSlots, 0, sizeof(m_updateSrvSlots));
	memset(m_updateTextureSlots, 0, sizeof(m_updateTextureSlots));
	memset(m_updateConstantBuffers, 0, sizeof(m_updateConstantBuffers));
	memset(m_initializationSrvSlots, 0, sizeof(m_initializationSrvSlots));
}

void CParticleComponentRuntime::UpdateEmitterData()
{
	assert(m_pEmitter);
	CryAutoLock<CryCriticalSection> lock(m_cs);
	m_parameters->emitterPosition = m_pEmitter->GetLocation().t;
	m_parameters->emitterOrientation = m_pEmitter->GetLocation().q;
	m_parentData.resize(m_subInstances.size());
	if (m_subInstances.size())
	{
		m_pComponent->GetParentData(m_pEmitter, m_subInstances, &m_parentData[0]);
	}
}

void CParticleComponentRuntime::AddSubInstances(TConstArray<SInstance> instances)
{
	CryAutoLock<CryCriticalSection> lock(m_cs);
	uint count = min(m_params.maxNewBorns - m_subInstances.size(), instances.size());
	for (int i = 0; i < count; ++i)
	{
		m_subInstances.push_back(instances[i].m_parentId);
		m_spawnData.push_back(SSpawnData());
	}

	for (CFeature* feature : m_gpuUpdateLists[eGpuUpdateList_InitSubInstance])
	{
		feature->InitSubInstance(this, &m_spawnData[m_spawnData.size() - count], count);
	}
}

void CParticleComponentRuntime::RemoveSubInstance(size_t instanceId)
{
	CryAutoLock<CryCriticalSection> lock(m_cs);
	size_t toCopy = m_subInstances.size() - 1;
	m_subInstances[instanceId] = m_subInstances[toCopy];
	m_spawnData[instanceId] = m_spawnData[toCopy];
	m_subInstances.pop_back();
	m_spawnData.pop_back();
}

void CParticleComponentRuntime::RemoveAllSubInstances()
{
	CryAutoLock<CryCriticalSection> lock(m_cs);
	m_subInstances.clear();
	m_spawnData.clear();
}

void CParticleComponentRuntime::ReparentParticles(TConstArray<TParticleId> swapIds)
{
	CryAutoLock<CryCriticalSection> lock(m_cs);
	size_t toCopy = 0;
	for (size_t i = 0; i < m_subInstances.size(); ++i)
	{
		m_subInstances[i] = swapIds[m_subInstances[i]];
		if (m_subInstances[i] == -1)
			continue;
		m_subInstances[toCopy] = m_subInstances[i];
		++toCopy;
	}
	m_subInstances.erase(m_subInstances.begin() + toCopy, m_subInstances.end());
}

void CParticleComponentRuntime::SwapToEnd(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CRY_ASSERT(m_parameters->numKilled <= m_parameters->numParticles);

	m_passSwapToEnd.SetOutputUAV(0, &m_container.GetDefaultParticleDataBuffer());
	m_passSwapToEnd.SetOutputUAV(1, &m_killList.GetBuffer());
	m_passSwapToEnd.SetOutputUAV(2, context.pScratchBuffer);

	const int blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(
	  m_parameters->numKilled,
	  kThreadsInBlock);

	m_parameters.CopyToDevice();
	m_passSwapToEnd.SetInlineConstantBuffer(eConstantBufferSlot_Base, m_parameters.GetDeviceConstantBuffer());

	m_passSwapToEnd.SetDispatchSize(blocks, 1, 1);
	m_passSwapToEnd.PrepareResourcesForUse(commandList);
	m_passSwapToEnd.Execute(commandList);

	m_parameters->numParticles -= m_parameters->numKilled;
}

gpu_physics::CParticleFluidSimulation* CParticleComponentRuntime::CreateFluidSimulation()
{
	// [PFX2_TODO_GPU] : The maximum fluid particle number needs to be adjustable
	m_pFluidSimulation = std::unique_ptr<gpu_physics::CParticleFluidSimulation>(new gpu_physics::CParticleFluidSimulation(32 * 1024));
	return m_pFluidSimulation.get();
}

void CParticleComponentRuntime::FluidCollisions(CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (m_parameters->numParticles)
	{
	   m_pFluidSimulation->FluidCollisions(commandList, m_parameters.GetDeviceConstantBuffer(), eConstantBufferSlot_Base);
	}
}

void CParticleComponentRuntime::EvolveParticles(CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (m_parameters->numParticles)
	{
	   m_pFluidSimulation->EvolveParticles(commandList, m_container.GetDefaultParticleDataBuffer(), m_parameters->numParticles);
	}
}

void CParticleComponentRuntime::InitializePasses()
{
	m_pShader.reset();
	CShader* pShader = gcpRendD3D.m_cEF.mfForName("GpuParticles", 0, 0, 0);
	m_pShader.Assign_NoAddRef(pShader);

	// set up compute passes that dont have varying material flags
	m_passCalcBounds.SetTechnique(m_pShader, CCryNameTSCRC("CalculateBounds"), 0);
	m_passSwapToEnd.SetTechnique(m_pShader, CCryNameTSCRC("SwapToEnd"), 0);
	m_passFillKillList.SetTechnique(m_pShader, CCryNameTSCRC("FillKillList"), 0);
	m_passPrepareSort.SetTechnique(m_pShader, CCryNameTSCRC("PrepareSort"), 0);
	m_passReorderParticles.SetTechnique(m_pShader, CCryNameTSCRC("ReorderParticles"), 0);
}

void CParticleComponentRuntime::Initialize()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	memset(m_updateSrvSlots, 0, sizeof(m_updateSrvSlots));
	memset(m_updateTextureSlots, 0, sizeof(m_updateTextureSlots));
	memset(m_updateConstantBuffers, 0, sizeof(m_updateConstantBuffers));
	memset(m_initializationSrvSlots, 0, sizeof(m_initializationSrvSlots));

	m_particleInitializationParameters->velocityScale = 0.0;
	m_particleInitializationParameters->color = Vec3(1.0f, 1.0f, 1.0f);
	m_particleInitializationParameters->size = 1.0f;
	m_particleInitializationParameters->opacity = 1.0f;
	m_particleInitializationParameters.CreateDeviceBuffer();

	m_parameters.CreateDeviceBuffer();

	m_parameters->currentTime = 0.0f;
	m_parameters->deltaTime = 0.016f;
	m_parameters->numNewBorns = 0;
	m_parameters->numParticles = 0;
	m_parameters->managerSlot = -1;

	m_parameters.CopyToDevice();

	bool sorting = m_params.sortMode != ESortMode::None;
	m_container.Initialize(sorting);
	if (sorting)
		m_pMergeSort = std::unique_ptr<gpu::CMergeSort>(new gpu::CMergeSort(m_params.maxParticles));

	// Alloc GPU resources
	m_blockSums.CreateDeviceBuffer();
	m_killList.CreateDeviceBuffer();
	m_parentDataRenderThread.CreateDeviceBuffer();
	m_newBornIndices.CreateDeviceBuffer();

	m_pFluidSimulation.reset();

	InitializePasses();

	m_state = EState::Ready;
}

void CParticleComponentRuntime::Reset()
{
	m_container.Clear();
	m_subInstances.clear();
	m_subInstances.shrink_to_fit();
	m_spawnData.clear();
	m_spawnData.shrink_to_fit();

	m_parameters->numParticles = 0;
	m_parameters->numNewBorns = 0;

	m_bounds = AABB::RESET;

	m_state = EState::Uninitialized;
}

void CParticleComponentRuntime::PrepareRelease()
{
	for (int i = 0; i < eGpuUpdateList_COUNT; ++i)
	{
		m_gpuUpdateLists[i].resize(0);
	}
}

void CParticleComponentRuntime::AccumStats(pfx2::SParticleStats& stats)
{
	stats.components.alive++;
	stats.components.updated++;
	stats.components.rendered++;
	stats.particles.rendered += m_parameters->numParticles;
	stats.particles.updated += m_parameters->numParticles;
	stats.particles.alive += m_parameters->numParticles;
}
}
