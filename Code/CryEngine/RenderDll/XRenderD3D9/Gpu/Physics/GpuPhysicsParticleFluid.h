// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef GPU_FLUID_SIM_H
#define GPU_FLUID_SIM_H

#include <CryRenderer/IGpuPhysics.h>
#include "Gpu/GpuComputeBackend.h"
#include "GraphicsPipeline/Common/ComputeRenderPass.h"

namespace gpu_physics
{
const int kThreadsInBlock = 1024u;

struct SGridCell
{
	uint count;
	uint sum;
	uint blockSum;
};

struct SParticleFluidParametersInternal : SParticleFluidParameters
{
	int numberOfBodies;
	int numberBodiesInject;
	int pad2[2];
};

struct SSimulationData
{
	SSimulationData(const int numBodies, const int totalGridSize)
		: bodies(numBodies), bodiesTemp(numBodies), bodiesOffsets(numBodies),
		grid(totalGridSize), bodiesInject(numBodies), adjacencyList(27)
	{
		bodies.CreateDeviceBuffer();
		bodiesTemp.CreateDeviceBuffer();
		bodiesOffsets.CreateDeviceBuffer();
		grid.CreateDeviceBuffer();
		bodiesInject.CreateDeviceBuffer();
		adjacencyList.CreateDeviceBuffer();
	};

	gpu::CStructuredResource<SFluidBody, gpu::BufferFlagsReadWriteAppend>   bodies;
	gpu::CStructuredResource<SFluidBody, gpu::BufferFlagsReadWriteReadback> bodiesTemp;
	gpu::CStructuredResource<uint, gpu::BufferFlagsReadWrite>               bodiesOffsets;
	gpu::CStructuredResource<SGridCell, gpu::BufferFlagsReadWrite>          grid;
	gpu::CStructuredResource<SFluidBody, gpu::BufferFlagsDynamic>           bodiesInject;
	gpu::CStructuredResource<int, gpu::BufferFlagsDynamic>                  adjacencyList;
};

// fwd
class CParticleEffector;

class CParticleFluidSimulation : public ISimulationInstance
{
public:
	enum { simulationType = eSimulationType_ParticleFluid };

	CParticleFluidSimulation(const int maxBodies);
	~CParticleFluidSimulation();

	// most of the simulation runs in the render thread
	void RenderThreadUpdate(CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	void CreateResources();
	void EvolveParticles(CDeviceCommandListRef RESTRICT_REFERENCE commandList, CGpuBuffer& defaultParticleBuffer, int numParticles);
	void FluidCollisions(CDeviceCommandListRef RESTRICT_REFERENCE commandList, CConstantBufferPtr parameterBuffer, int constantBufferSlot);
protected:
	void InternalInjectBodies(const EBodyType type, const SBodyBase* b, const int numBodies);
	void InternalSetParameters(const EParameterType type, const SParameterBase* p);
	void SetUAVsAndConstants();

	void RetrieveCounter();
	void UpdateDynamicBuffers();
	void UpdateSimulationBuffers(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const int cellBlock);
	void SortSimulationBodies(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const int gridCells, const int cellBlock, const int blocks);

	void IntegrateBodies(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const int blocks);

	void DebugDraw();

	const int m_maxBodies;

	gpu::CTypedConstantBuffer<SParticleFluidParametersInternal> m_params;

	// resources
	std::unique_ptr<SSimulationData> m_pData;
	std::vector<SFluidBody>          m_bodiesInject;

	CComputeRenderPass               m_passCalcLambda;
	CComputeRenderPass               m_passPredictDensity;
	CComputeRenderPass               m_passCorrectDensityError;
	CComputeRenderPass               m_passCorrectDivergenceError;
	CComputeRenderPass               m_passPositionUpdate;
	CComputeRenderPass               m_passBodiesInject;
	CComputeRenderPass               m_passClearGrid;
	CComputeRenderPass               m_passAssignAndCount;
	CComputeRenderPass               m_passPrefixSumBlocks;
	CComputeRenderPass               m_passBuildGridIndices;
	CComputeRenderPass               m_passRearrangeParticles;
	CComputeRenderPass               m_passEvolveExternalParticles;
	CComputeRenderPass               m_passCollisionsScreenSpace;

	// only for debug
	std::vector<Vec3> m_points;
};
}

#endif
