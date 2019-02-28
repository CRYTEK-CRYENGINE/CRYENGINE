// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GpuParticleContainer.h"

#include "Gpu/Physics/GpuPhysicsParticleFluid.h"
#include "Gpu/GpuMergeSort.h"
#include "GpuParticleFeatureBase.h"
#include "GraphicsPipeline/Common/ComputeRenderPass.h"

namespace gpu_pfx2
{

static const float kBoundsScale = 100.f;
static const uint32 kCalcBoundsBlocks = 16u;

struct SReadbackData
{
	Vec3i min;
	Vec3i max;
};

enum EFeatureUpdateSrvSlot
{
	eFeatureUpdateSrvSlot_sizeTable,
	eFeatureUpdateSrvSlot_colorTable,
	eFeatureUpdateSrvSlot_opacityTable,
	eFeatureUpdateSrvSlot_depthBuffer,

	eFeatureUpdateSrvSlot_COUNT
};

enum EFeatureInitializationSrvSlot
{
	eFeatureInitializationSrvSlot_newBornIndices,
	eFeatureInitializationSrvSlot_parentData,

	eFeatureInitializationSrvSlot_COUNT
};

enum EConstantBufferSlot
{
	eConstantBufferSlot_Base = 4,
	eConstantBufferSlot_Initialization,
	eConstantBufferSlot_Motion,
	eConstantBufferSlot_Brownian,
	eConstantBufferSlot_Simplex,
	eConstantBufferSlot_Curl,
	eConstantBufferSlot_Gravity,
	eConstantBufferSlot_Vortex,
	eConstantBufferSlot_PixelSize,
	eConstantBufferSlot_Collisions,

	eConstantBufferSlot_COUNT
};

template<class T>
using TDynArrayAligned = DynArray<T, uint, NArray::FastDynStorage<NAlloc::AllocAlign<NAlloc::ModuleAlloc, CRY_PLATFORM_ALIGNMENT>>>;

class CParticleComponentRuntime : public IParticleComponentRuntime
{
public:
	static const int kThreadsInBlock;

	CParticleComponentRuntime(const SComponentParams& params, TConstArray<IParticleFeature*> features);

	// IParticleComponentRuntime interface
	virtual bool        IsValidForParams(const SComponentParams& parameters) override;

	virtual const AABB& GetBounds() const override;
	virtual void        AccumStats(SParticleStats& stats) override;
	virtual bool        HasParticles() override { return m_parameters->numParticles != 0; }

	virtual void        UpdateData(const SUpdateParams& params, TConstArray<SSpawnEntry> entries, TConstArray<SParentData> parentData) override;
	// ~IParticleComponentRuntime

	void                Initialize();

	void                SetManagerSlot(int i) { m_parameters->managerSlot = i; }

	void                SetLifeTime(float lifeTime) { m_parameters->lifeTime = lifeTime; }

	int                 GetNumParticles()           { return m_parameters->numParticles; };

	// this is from the render thread
	void AddRemoveParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void UpdateParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void CalculateBounds(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void SetBoundsFromManager(const SReadbackData* pData);
	void SetCounterFromManager(const uint32* pCounter);
	CGpuBuffer& PrepareForUse();

	CParticleContainer*                GetContainer() { return &m_container; }

	void                               SetUpdateTexture(EFeatureUpdateSrvSlot slot, CTexture* pTexture);
	void                               SetUpdateBuffer(EFeatureUpdateSrvSlot slot, CGpuBuffer* pSRV);
	void                               SetUpdateFlags(uint64 flags);
	void                               SetUpdateConstantBuffer(EConstantBufferSlot slot, CConstantBufferPtr pConstantBuffer);

	SParticleInitializationParameters& GetParticleInitializationParameters() { return m_particleInitializationParameters.GetHostData(); }
	void                               SetInitializationSRV(EFeatureInitializationSrvSlot slot, CGpuBuffer* pSRV);

	// currently called from the render element mfDraw, but may go somewhere else
	void SwapToEnd(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	Vec3                                   GetPos() const { return m_parameters->emitterPosition; };

	gpu_physics::CParticleFluidSimulation* CreateFluidSimulation();
	gpu_physics::CParticleFluidSimulation* GetFluidSimulation() { return m_pFluidSimulation.get(); }
	void FluidCollisions(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void EvolveParticles(CDeviceCommandListRef RESTRICT_REFERENCE commandList);

private:
	void AddParticles(SUpdateContext& context);
	void UpdateNewBorns(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void FillKillList(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void UpdateFeatures(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void Sort(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	// update passes with varying material flags
	void InitializePasses();
	// update passes with varying material flags
	void UpdatePasses();

	CParticleContainer                                  m_container;

	gpu::CStructuredResource<int, gpu::BufferFlagsReadWrite> m_blockSums;
	gpu::CStructuredResource<int, gpu::BufferFlagsReadWrite> m_killList;
	gpu::CStructuredResource<uint, gpu::BufferFlagsDynamic>  m_newBornIndices;

	// Features handled by GPU system
	DynArray<_smart_ptr<CFeature>, uint> m_features;

	// Data set by CPU system, double buffered
	struct SUpdateData: SUpdateParams
	{
		TDynArrayAligned<SParentData> parentData;
		TDynArrayAligned<SSpawnEntry> spawnEntries;
	};
	SUpdateData m_updateData[RT_COMMAND_BUF_COUNT];
	
	gpu::CStructuredResource<SParentData, gpu::BufferFlagsDynamic>    m_parentDataRenderThread;
	gpu::CTypedConstantBuffer<SParticleInitializationParameters> m_particleInitializationParameters;
	gpu::CTypedConstantBuffer<SParticleParameters>               m_parameters;

	SComponentParams          m_params;

	AABB                      m_bounds;

	// these pointers must be declared before the ComputeRenderPasses, for proper destruction order.

	std::unique_ptr<gpu_physics::CParticleFluidSimulation> m_pFluidSimulation;
	std::unique_ptr<gpu::CMergeSort>                       m_pMergeSort;

	_smart_ptr<CShader>      m_pInitShader;
	_smart_ptr<CShader>      m_pUpdateShader;

	CGpuBuffer*              m_updateSrvSlots[eFeatureUpdateSrvSlot_COUNT];
	CTexture*                m_updateTextureSlots[eFeatureUpdateSrvSlot_COUNT];
	CConstantBufferPtr       m_updateConstantBuffers[eConstantBufferSlot_COUNT];
	uint64                   m_updateShaderFlags;

	CGpuBuffer*              m_initializationSrvSlots[eFeatureInitializationSrvSlot_COUNT];

	// Compute Passes
	CComputeRenderPass       m_passCalcBounds;
	CComputeRenderPass       m_passFeatureInitialization;
	CComputeRenderPass       m_passFillKillList;
	CComputeRenderPass       m_passFeatureUpdate;
	CComputeRenderPass       m_passPrepareSort;
	CComputeRenderPass       m_passReorderParticles;
	CComputeRenderPass       m_passSwapToEnd;

	uint64                   m_initializationShaderFlags;
	uint64                   m_previousInitializationShaderFlags;
	uint64                   m_previousUpdateShaderShaderFlags;
	bool                     m_initialized;

	CryCriticalSection       m_cs;
};
}
