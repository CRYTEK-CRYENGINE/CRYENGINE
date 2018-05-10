// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

void CParticleComponentRuntime::AddRemoveParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (!m_container.HasDefaultParticleData())
		return;

	// this means that the pipeline has already run, we get the kill list
	// and the amount of dead particles from the last frame, kill the particles
	// and swap them out by living ones at the end
	if (m_parameters->numKilled)
		SwapToEnd(context, commandList);

	AddParticles(context);
	if (m_parameters->numNewBorns)
		UpdateNewBorns(context, commandList);
}

void CParticleComponentRuntime::UpdateParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (!m_parameters->numParticles)
		return;

	m_parameters->deltaTime = context.deltaTime;
	m_parameters->currentTime += context.deltaTime;
	m_parameters->viewProjection = context.pRenderView->GetViewInfo(CCamera::eEye_Left).cameraProjMatrix;
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
	if (!m_parameters->numParticles)
	{
		m_bounds = AABB::RESET;
		return;
	}

	if (CRenderer::CV_r_GpuParticlesConstantRadiusBoundingBoxes > 0)
	{
		m_bounds = AABB(m_parameters->emitterPosition, (float)CRenderer::CV_r_GpuParticlesConstantRadiusBoundingBoxes);
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

void CParticleComponentRuntime::UpdateData(const SUpdateParams& params, TConstArray<SSpawnEntry> entries, TConstArray<SParentData> parentData)
{
	auto& updateData = m_updateData[gRenDev->m_nFillThreadID];

	static_cast<SUpdateParams&>(updateData) = params;
	updateData.parentData = parentData;
	updateData.spawnEntries = entries;
}

void CParticleComponentRuntime::AddParticles(SUpdateContext& context)
{
	const auto& updateData = m_updateData[gRenDev->m_nProcessThreadID];

	// copy parameters from CPU features
	m_particleInitializationParameters = updateData;
	m_initializationShaderFlags        = updateData.initFlags;

	m_parameters->lifeTime             = updateData.lifeTime;
	m_parameters->emitterPosition      = updateData.emitterPosition;
	m_parameters->emitterOrientation   = updateData.emitterOrientation;
	m_parameters->physAccel            = updateData.physAccel;
	m_parameters->physWind             = updateData.physWind;
	
	// count and cap the number of particles to spawn
	m_parameters->numNewBorns = 0;
	for (const auto& entry : updateData.spawnEntries)
		m_parameters->numNewBorns += entry.m_count;

	// cap the count to max newborns
	CRY_ASSERT(m_parameters->numNewBorns <= m_params.maxNewBorns);
	CRY_ASSERT(m_parameters->numParticles + m_parameters->numNewBorns <= m_params.maxParticles);
	int maxNewBorns = min(m_parameters->numNewBorns, m_params.maxParticles - m_parameters->numParticles);
	m_parameters->numNewBorns = min(m_parameters->numNewBorns, maxNewBorns);
	m_parameters->numParticles += m_parameters->numNewBorns;

	m_parameters.CopyToDevice();

	// copy parentData and newbornData
	if (updateData.parentData.size())
		m_parentDataRenderThread.UpdateBufferContentAligned(updateData.parentData.begin(), updateData.parentData.size());

	if (m_parameters->numNewBorns)
	{
		TDynArrayAligned<TParticleId> newBornIndices;
		newBornIndices.reserve(m_parameters->numNewBorns);
		for (const auto& entry : updateData.spawnEntries)
		{
			TParticleId id = min(entry.m_parentId, updateData.parentData.size() - 1);
			TParticleId count = min(entry.m_count, m_parameters->numNewBorns - newBornIndices.size());
			newBornIndices.append(count, id);
		}

		m_newBornIndices.UpdateBufferContentAligned(newBornIndices.begin(), newBornIndices.size());
	}
}

void CParticleComponentRuntime::UpdateNewBorns(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	for (CFeature* feature : m_features)
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

	for (CFeature* feature : m_features)
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

CParticleComponentRuntime::CParticleComponentRuntime(const SComponentParams& params, TConstArray<IParticleFeature*> features)
	: m_bounds(AABB::RESET)
	, m_initialized(false)
	, m_blockSums(params.maxParticles)
	, m_killList(params.maxParticles)
	, m_newBornIndices(params.maxNewBorns)
	, m_parentDataRenderThread(params.maxNewBorns)
	, m_params(params)
	, m_container(params.maxParticles)
	, m_updateShaderFlags(0)
	, m_previousUpdateShaderShaderFlags(std::numeric_limits<uint64>::max())
	, m_initializationShaderFlags(0)
	, m_previousInitializationShaderFlags(std::numeric_limits<uint64>::max())
{
	ZeroStruct(m_updateSrvSlots);
	ZeroStruct(m_updateTextureSlots);
	ZeroStruct(m_updateConstantBuffers);
	ZeroStruct(m_initializationSrvSlots);

	m_features.reserve(features.size());
	for (auto pFeature : features)
		m_features.push_back(static_cast<CFeature*>(pFeature));
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
	// set up compute passes that dont have varying material flags
	m_passCalcBounds.SetTechnique(CShaderMan::s_ShaderGpuParticles, CCryNameTSCRC("CalculateBounds"), 0);
	m_passSwapToEnd.SetTechnique(CShaderMan::s_ShaderGpuParticles, CCryNameTSCRC("SwapToEnd"), 0);
	m_passFillKillList.SetTechnique(CShaderMan::s_ShaderGpuParticles, CCryNameTSCRC("FillKillList"), 0);
	m_passPrepareSort.SetTechnique(CShaderMan::s_ShaderGpuParticles, CCryNameTSCRC("PrepareSort"), 0);
	m_passReorderParticles.SetTechnique(CShaderMan::s_ShaderGpuParticles, CCryNameTSCRC("ReorderParticles"), 0);
}

void CParticleComponentRuntime::Initialize()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (m_initialized)
		return;

	ZeroStruct(m_updateSrvSlots);
	ZeroStruct(m_updateTextureSlots);
	ZeroStruct(m_updateConstantBuffers);
	ZeroStruct(m_initializationSrvSlots);

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

	m_initialized = true;
}

void CParticleComponentRuntime::AccumStats(SParticleStats& stats)
{
	stats.components.alloc++;
	stats.components.alive++;
	stats.components.updated++;
	stats.components.rendered++;
	stats.particles.alloc += m_params.maxParticles;
	stats.particles.alive += m_parameters->numParticles;
	stats.particles.updated += m_parameters->numParticles;
	stats.particles.rendered += m_parameters->numParticles;
}
}
