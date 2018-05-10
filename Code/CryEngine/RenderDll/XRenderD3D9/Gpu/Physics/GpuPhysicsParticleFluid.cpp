// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuPhysicsParticleFluid.h"

#include <CryPhysics/physinterface.h>

namespace gpu_physics
{
void SetupAdjacency(int* adjescency, int resX, int resY)
{
	// Build Adjacency Lookup
	int cell = 0;
	for (int y = -1; y <= 1; y++)
		for (int z = -1; z <= 1; z++)
			for (int x = -1; x <= 1; x++)
				adjescency[cell++] = (z * resY + y) * resX + x;
}

CParticleFluidSimulation::CParticleFluidSimulation(const int maxBodies)
	: m_maxBodies(maxBodies)
{
	assert(maxBodies % kThreadsInBlock == 0);
	m_points.resize(maxBodies);
	memset(&m_params.GetHostData(), 0x00, sizeof(SParticleFluidParametersInternal));

	m_params->numberOfIterations = 2;
	m_params->deltaTime = 0.008f;
	m_params->stiffness = 60.0f;
	m_params->gravityConstant = -1.f;
	m_params->h = 0.5f;
	m_params->r0 = 32.0f;
	m_params->mass = 1.0f;
	m_params->maxVelocity = 3.0f;
	m_params->maxForce = 10.f;
	m_params->gridSizeX = 16;
	m_params->gridSizeY = 16;
	m_params->gridSizeZ = 16;
	m_params->particleInfluence = 0.1f;
	m_params->numberOfBodies = 0;
	m_params->numberBodiesInject = 0;
	m_params->worldOffsetX = 0;
	m_params->worldOffsetY = 0;
	m_params->worldOffsetZ = 0;
	m_params->atmosphericDrag = 25.f;
	m_params->cohesion = 2.0f;
	m_params->baroclinity = 0.25f;

	CShader* pShader = gcpRendD3D.m_cEF.mfForName("GpuPhysicsParticleFluid", EF_SYSTEM, 0, 0);

	m_passCalcLambda.SetTechnique(pShader, CCryNameTSCRC("CalcLambda"), 0);
	m_passPredictDensity.SetTechnique(pShader, CCryNameTSCRC("PredictDensity"), 0);
	m_passCorrectDensityError.SetTechnique(pShader, CCryNameTSCRC("CorrectDensityError"), 0);
	m_passCorrectDivergenceError.SetTechnique(pShader, CCryNameTSCRC("CorrectDivergenceError"), 0);
	m_passPositionUpdate.SetTechnique(pShader, CCryNameTSCRC("PositionUpdate"), 0);
	m_passBodiesInject.SetTechnique(pShader, CCryNameTSCRC("BodiesInject"), 0);
	m_passClearGrid.SetTechnique(pShader, CCryNameTSCRC("ClearGrid"), 0);
	m_passAssignAndCount.SetTechnique(pShader, CCryNameTSCRC("AssignAndCount"), 0);
	m_passPrefixSumBlocks.SetTechnique(pShader, CCryNameTSCRC("PrefixSumBlocks"), 0);
	m_passBuildGridIndices.SetTechnique(pShader, CCryNameTSCRC("BuildGridIndices"), 0);
	m_passRearrangeParticles.SetTechnique(pShader, CCryNameTSCRC("RearrangeParticles"), 0);
	m_passEvolveExternalParticles.SetTechnique(pShader, CCryNameTSCRC("EvolveExternalParticles"), 0);
	m_passCollisionsScreenSpace.SetTechnique(pShader, CCryNameTSCRC("CollisionScreenSpace"), 0);

	CreateResources();
}

CParticleFluidSimulation::~CParticleFluidSimulation()
{
}

void CParticleFluidSimulation::InternalSetParameters(EParameterType type, const SParameterBase* p)
{
	if (type == EParameterType::ParticleFluid)
	{
		const SParticleFluidParameters* params = static_cast<const SParticleFluidParameters*>(p);

		m_params->deltaTime = params->deltaTime;
		m_params->numberOfIterations = params->numberOfIterations;
		m_params->stiffness = params->stiffness;
		m_params->gravityConstant = params->gravityConstant;
		m_params->h = params->h;
		m_params->r0 = params->r0;
		m_params->mass = params->mass;
		m_params->maxVelocity = params->maxVelocity;
		m_params->maxForce = params->maxForce;
		m_params->atmosphericDrag = params->atmosphericDrag;
		m_params->cohesion = params->cohesion;
		m_params->baroclinity = params->baroclinity;
		m_params->worldOffsetX = params->worldOffsetX;
		m_params->worldOffsetY = params->worldOffsetY;
		m_params->worldOffsetZ = params->worldOffsetZ;
		m_params->particleInfluence = params->particleInfluence;
	}
}

void CParticleFluidSimulation::CreateResources()
{
	m_params.CreateDeviceBuffer();

	const int totalGridSize = m_params->gridSizeX * m_params->gridSizeY * m_params->gridSizeZ;
	m_pData = std::unique_ptr<SSimulationData>(
	  new SSimulationData(m_maxBodies, totalGridSize));

	int adjacency[27];
	SetupAdjacency(adjacency, m_params->gridSizeX, m_params->gridSizeY);

	m_pData->adjacencyList.UpdateBufferContent(adjacency, 27);
}

void CParticleFluidSimulation::UpdateDynamicBuffers()
{
	if (m_bodiesInject.size() > 0)
	{
		m_params->numberBodiesInject = m_bodiesInject.size();
		m_pData->bodiesInject.UpdateBufferContent(&m_bodiesInject[0], m_params->numberBodiesInject);
	}
}

void CParticleFluidSimulation::RetrieveCounter()
{
	PROFILE_LABEL_SCOPE("RETRIEVE COUNTER");
	const int counter = m_pData->bodies.RetrieveCounter();
	m_params->numberOfBodies = counter;
}

void CParticleFluidSimulation::UpdateSimulationBuffers(
  CDeviceCommandListRef RESTRICT_REFERENCE commandList,
  const int cellBlock)
{
	PROFILE_LABEL_SCOPE("INJECT BODIES");

	m_params.CopyToDevice();

	m_passBodiesInject.SetBuffer(0, &m_pData->bodiesInject.GetBuffer());
	m_passBodiesInject.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
	m_passBodiesInject.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
	m_passBodiesInject.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
	m_passBodiesInject.SetOutputUAV(3, &m_pData->grid.GetBuffer());
	m_passBodiesInject.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());

	if (m_params->numberBodiesInject > 0)
	{
		m_passBodiesInject.SetDispatchSize((int)ceil((float)m_params->numberBodiesInject / cellBlock), 1, 1);
		m_passBodiesInject.PrepareResourcesForUse(commandList);
		m_passBodiesInject.Execute(commandList);

		m_params->numberOfBodies += m_params->numberBodiesInject;
		m_params->numberBodiesInject = 0;
		m_params.CopyToDevice();
	}
}

void CParticleFluidSimulation::SortSimulationBodies(
  CDeviceCommandListRef RESTRICT_REFERENCE commandList,
  const int gridCells,
  const int cellBlock,
  const int blocks)
{
	PROFILE_LABEL_SCOPE("SORT");

	{
		m_passClearGrid.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
		m_passClearGrid.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
		m_passClearGrid.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
		m_passClearGrid.SetOutputUAV(3, &m_pData->grid.GetBuffer());
		m_passClearGrid.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
		m_passClearGrid.SetDispatchSize(gridCells / cellBlock, 1, 1);
		m_passClearGrid.PrepareResourcesForUse(commandList);
		m_passClearGrid.Execute(commandList);
	}
	{
		m_passAssignAndCount.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
		m_passAssignAndCount.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
		m_passAssignAndCount.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
		m_passAssignAndCount.SetOutputUAV(3, &m_pData->grid.GetBuffer());
		m_passAssignAndCount.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
		m_passAssignAndCount.SetDispatchSize(blocks, 1, 1);
		m_passAssignAndCount.PrepareResourcesForUse(commandList);
		m_passAssignAndCount.Execute(commandList);
	}
	{
		m_passPrefixSumBlocks.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
		m_passPrefixSumBlocks.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
		m_passPrefixSumBlocks.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
		m_passPrefixSumBlocks.SetOutputUAV(3, &m_pData->grid.GetBuffer());
		m_passPrefixSumBlocks.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
		m_passPrefixSumBlocks.SetDispatchSize(gridCells / (2 * 512), 1, 1);
		m_passPrefixSumBlocks.PrepareResourcesForUse(commandList);
		m_passPrefixSumBlocks.Execute(commandList);
	}
	{
		m_passBuildGridIndices.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
		m_passBuildGridIndices.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
		m_passBuildGridIndices.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
		m_passBuildGridIndices.SetOutputUAV(3, &m_pData->grid.GetBuffer());
		m_passBuildGridIndices.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
		m_passBuildGridIndices.SetDispatchSize(gridCells / cellBlock, 1, 1);
		m_passBuildGridIndices.PrepareResourcesForUse(commandList);
		m_passBuildGridIndices.Execute(commandList);
	}
	{
		m_passRearrangeParticles.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
		m_passRearrangeParticles.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
		m_passRearrangeParticles.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
		m_passRearrangeParticles.SetOutputUAV(3, &m_pData->grid.GetBuffer());
		m_passRearrangeParticles.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
		m_passRearrangeParticles.SetDispatchSize(blocks, 1, 1);
		m_passRearrangeParticles.PrepareResourcesForUse(commandList);
		m_passRearrangeParticles.Execute(commandList);
	}
}

void CParticleFluidSimulation::EvolveParticles(CDeviceCommandListRef RESTRICT_REFERENCE commandList, CGpuBuffer& defaultParticleBuffer, int numParticles)
{
	const int blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(numParticles, kThreadsInBlock);
	m_passEvolveExternalParticles.SetBuffer(0, &m_pData->adjacencyList.GetBuffer());
	m_passEvolveExternalParticles.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
	m_passEvolveExternalParticles.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
	m_passEvolveExternalParticles.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
	m_passEvolveExternalParticles.SetOutputUAV(3, &m_pData->grid.GetBuffer());
	m_passEvolveExternalParticles.SetOutputUAV(4, &defaultParticleBuffer);
	m_passEvolveExternalParticles.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
	m_passEvolveExternalParticles.SetDispatchSize(blocks, 1, 1);
	m_passEvolveExternalParticles.PrepareResourcesForUse(commandList);
	m_passEvolveExternalParticles.Execute(commandList);
}

void CParticleFluidSimulation::FluidCollisions(CDeviceCommandListRef RESTRICT_REFERENCE commandList, CConstantBufferPtr parameterBuffer, int constantBufferSlot)
{
	const uint blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(m_params->numberOfBodies, kThreadsInBlock);
	m_passCollisionsScreenSpace.SetBuffer(0, &m_pData->adjacencyList.GetBuffer());
	m_passCollisionsScreenSpace.SetTexture(1, CRendererResources::s_ptexLinearDepth);
	m_passCollisionsScreenSpace.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
	m_passCollisionsScreenSpace.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
	m_passCollisionsScreenSpace.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
	m_passCollisionsScreenSpace.SetOutputUAV(3, &m_pData->grid.GetBuffer());
	m_passCollisionsScreenSpace.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
	m_passCollisionsScreenSpace.SetInlineConstantBuffer(constantBufferSlot, parameterBuffer);
	m_passCollisionsScreenSpace.SetSampler(0, EDefaultSamplerStates::BilinearClamp);
	m_passCollisionsScreenSpace.SetSampler(1, EDefaultSamplerStates::PointClamp);
	m_passCollisionsScreenSpace.SetDispatchSize(blocks, 1, 1);
	m_passCollisionsScreenSpace.PrepareResourcesForUse(commandList);
	m_passCollisionsScreenSpace.Execute(commandList);
}

void CParticleFluidSimulation::IntegrateBodies(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const int blocks)
{
	PROFILE_LABEL_SCOPE("INTEGRATE");

	Vec3 minV(m_params->worldOffsetX, m_params->worldOffsetY, m_params->worldOffsetZ);
	Vec3 maxV(m_params->gridSizeX * m_params->h, m_params->gridSizeY * m_params->h, m_params->gridSizeZ * m_params->h);
	maxV += minV;

	if (CRenderer::CV_r_GpuPhysicsFluidDynamicsDebug)
	{
		gcpRendD3D.GetIRenderAuxGeom()->SetRenderFlags(e_Def3DPublicRenderflags);
		gcpRendD3D.GetIRenderAuxGeom()->DrawAABB(AABB(minV, maxV), false, RGBA8(0xff, 0xff, 0xff, 0xff), eBBD_Faceted);
	}

	// Position Based Update
#if 0
	for (int i = 0; i < m_params->numberOfIterations; ++i)
	{
		{
			backend.BeginPass("CalcLambda");
			backend.Dispatch(blocks, 1, 1);
			backend.EndPass();
		}
		{
			backend.BeginPass("DeltaPos");
			backend.Dispatch(blocks, 1, 1);
			backend.EndPass();
		}
	}
#else
	for (int i = 0; i < m_params->numberOfIterations; ++i)
	{
		{
			PROFILE_LABEL_SCOPE("Predict Density");
			{
				m_passPredictDensity.SetBuffer(0, &m_pData->adjacencyList.GetBuffer());
				m_passPredictDensity.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
				m_passPredictDensity.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
				m_passPredictDensity.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
				m_passPredictDensity.SetOutputUAV(3, &m_pData->grid.GetBuffer());
				m_passPredictDensity.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
				m_passPredictDensity.SetDispatchSize(blocks, 1, 1);
				m_passPredictDensity.PrepareResourcesForUse(commandList);
				m_passPredictDensity.Execute(commandList);
			}
		}
		{
			PROFILE_LABEL_SCOPE("CorrectDensityError");
			{
				m_passCorrectDensityError.SetBuffer(0, &m_pData->adjacencyList.GetBuffer());
				m_passCorrectDensityError.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
				m_passCorrectDensityError.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
				m_passCorrectDensityError.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
				m_passCorrectDensityError.SetOutputUAV(3, &m_pData->grid.GetBuffer());
				m_passCorrectDensityError.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
				m_passCorrectDensityError.SetDispatchSize(blocks, 1, 1);
				m_passCorrectDensityError.PrepareResourcesForUse(commandList);
				m_passCorrectDensityError.Execute(commandList);
			}
		}
		{
			PROFILE_LABEL_SCOPE("CorrectDivergenceError");
			{
				m_passCorrectDivergenceError.SetBuffer(0, &m_pData->adjacencyList.GetBuffer());
				m_passCorrectDivergenceError.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
				m_passCorrectDivergenceError.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
				m_passCorrectDivergenceError.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
				m_passCorrectDivergenceError.SetOutputUAV(3, &m_pData->grid.GetBuffer());
				m_passCorrectDivergenceError.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
				m_passCorrectDivergenceError.SetDispatchSize(blocks, 1, 1);
				m_passCorrectDivergenceError.PrepareResourcesForUse(commandList);
				m_passCorrectDivergenceError.Execute(commandList);
			}
		}
	}
#endif
	{
		PROFILE_LABEL_SCOPE("POSITION UPDATE");

		m_passPositionUpdate.SetOutputUAV(0, &m_pData->bodies.GetBuffer());
		m_passPositionUpdate.SetOutputUAV(1, &m_pData->bodiesTemp.GetBuffer());
		m_passPositionUpdate.SetOutputUAV(2, &m_pData->bodiesOffsets.GetBuffer());
		m_passPositionUpdate.SetOutputUAV(3, &m_pData->grid.GetBuffer());
		m_passPositionUpdate.SetInlineConstantBuffer(3, m_params.GetDeviceConstantBuffer());
		m_passPositionUpdate.SetDispatchSize(blocks, 1, 1);
		m_passPositionUpdate.PrepareResourcesForUse(commandList);
		m_passPositionUpdate.Execute(commandList);
	}
}

void CParticleFluidSimulation::RenderThreadUpdate(CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	const uint gridCells = m_params->gridSizeX * m_params->gridSizeY * m_params->gridSizeZ;
	const uint blocks = gpu::GetNumberOfBlocksForArbitaryNumberOfThreads(m_params->numberOfBodies, kThreadsInBlock);

	PROFILE_LABEL_SCOPE("FLUIDSIM");

	// there were already bodies injected
	if (m_params->numberOfBodies > 0)
		RetrieveCounter();

	UpdateDynamicBuffers();
	UpdateSimulationBuffers(commandList, kThreadsInBlock);
	SortSimulationBodies(commandList, gridCells, kThreadsInBlock, blocks);
	IntegrateBodies(commandList, blocks);

	{
		PROFILE_LABEL_SCOPE("READBACK COUNTER");
		m_pData->bodies.ReadbackCounter();
	}

	m_bodiesInject.resize(0);
	DebugDraw();
}

void CParticleFluidSimulation::InternalInjectBodies(const EBodyType type, const SBodyBase* b, const int numBodies)
{
	if (type == EBodyType::Fluid)
	{
		m_bodiesInject.resize(numBodies);
		memcpy(&m_bodiesInject[0], b, sizeof(SFluidBody) * numBodies);
	}
}

void CParticleFluidSimulation::DebugDraw()
{
	if (CRenderer::CV_r_GpuPhysicsFluidDynamicsDebug)
	{
		m_pData->bodiesTemp.Readback(m_params->numberOfBodies);
		const SFluidBody* parts = m_pData->bodiesTemp.Map(m_params->numberOfBodies);

		if (!parts)
			return;

		gcpRendD3D.GetIRenderAuxGeom()->SetRenderFlags(e_Def3DPublicRenderflags);
		m_points.resize(m_params->numberOfBodies);

		for (int i = 0; i < m_params->numberOfBodies; ++i)
			m_points[i] = parts[i].x;

		m_pData->bodiesTemp.Unmap();

		if (m_params->numberOfBodies)
		{
			gcpRendD3D.GetIRenderAuxGeom()->DrawPoints(
			  &m_points[0],
			  m_params->numberOfBodies,
			  RGBA8(0xff, 0xff, 0xff, 0xff), 128);
		}
	}
}
}
