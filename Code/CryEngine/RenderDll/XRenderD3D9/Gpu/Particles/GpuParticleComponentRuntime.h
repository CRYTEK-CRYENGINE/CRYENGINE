// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GpuParticleContainer.h"

#include "Gpu/Physics/GpuPhysicsParticleFluid.h"
#include "Gpu/GpuMergeSort.h"
#include "GpuParticleFeatureBase.h"
#include "GraphicsPipeline/Common/ComputeRenderPass.h"

namespace gpu_pfx2
{

template<typename T> using TConstArray = Array<const T, uint>;


static const float kBoundsScale = 100.f;
static const uint32 kCalcBoundsBlocks = 16u;

struct SReadbackData
{
	Vec3i min;
	Vec3i max;
};

struct SParticleParameters
{
	Matrix44  viewProjection;
	Quat      emitterOrientation;
	Vec3      emitterPosition;
	f32       deltaTime;
	Vec3      physAccel;
	f32       currentTime;
	Vec3      physWind;
	float     farToNearDistance;
	Vec3      cameraPosition;
	float     lifeTime;
	int32     numParticles;
	int32     numNewBorns;
	int32     numKilled;
	ESortMode sortMode;
	int32     managerSlot;
};

struct SSpawnData
{
	SSpawnData() : spawned(0.f), timer(0.f) {}
	float spawned;
	float timer;
};

enum EFeatureUpdateFlags
{
	eFeatureUpdateFlags_Motion                = 0x04,
	eFeatureUpdateFlags_Color                 = 0x08,
	eFeatureUpdateFlags_Size                  = 0x10,
	eFeatureUpdateFlags_Opacity               = 0x20,
	eFeatureUpdateFlags_Motion_LinearIntegral = 0x40,
	eFeatureUpdateFlags_Motion_DragFast       = 0x80,
	eFeatureUpdateFlags_Motion_Brownian       = 0x100,
	eFeatureUpdateFlags_Motion_Simplex        = 0x200,
	eFeatureUpdateFlags_Motion_Curl           = 0x400,
	eFeatureUpdateFlags_Motion_Gravity        = 0x800,
	eFeatureUpdateFlags_Motion_Vortex         = 0x1000,
	EFeatureUpdateFlags_Collision_ScreenSpace = 0x2000,
	EFeatureUpdateFlags_PixelSize             = 0x4000
};

enum EFeatureUpdateSrvSlot
{
	eFeatureUpdateSrvSlot_sizeTable,
	eFeatureUpdateSrvSlot_colorTable,
	eFeatureUpdateSrvSlot_opacityTable,
	eFeatureUpdateSrvSlot_depthBuffer,

	eFeatureUpdateSrvSlot_COUNT
};

enum EFeatureInitializationFlags
{
	eFeatureInitializationFlags_LocationOffset          = 0x04,
	eFeatureInitializationFlags_LocationBox             = 0x08,
	eFeatureInitializationFlags_LocationSphere          = 0x10,
	eFeatureInitializationFlags_LocationCircle          = 0x20,
	eFeatureInitializationFlags_LocationNoise           = 0x200,
	eFeatureInitializationFlags_VelocityCone            = 0x40,
	eFeatureInitializationFlags_VelocityDirectional     = 0x80,
	eFeatureInitializationFlags_VelocityOmniDirectional = 0x100,
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

struct SParticleInitializationParameters
{
	Vec3  offset;
	float velocity;
	Vec3  box;
	float velocityScale;
	Vec3  scale;
	float radius;
	Vec3  color;
	float size;
	Vec3  direction;
	float opacity;
	float angle;
	float directionScale;
	float omniVelocity;
	float amplitude;
	float noiseSize;
	float rate;
	int octaves;
};

class CParticleComponentRuntime;

// context passed to functions during update phase
struct SUpdateContext
{
	CParticleComponentRuntime* pRuntime;
	CGpuBuffer*                pReadbackBuffer;
	CGpuBuffer*                pCounterBuffer;
	CGpuBuffer*                pScratchBuffer;
	float                      deltaTime;
};

class CParticleComponentRuntime : public IParticleComponentRuntime
{
private:
	struct SSpawnEntry
	{
		uint32 m_count;
		int32  m_instanceId;
	};

public:
	static const int kThreadsInBlock;

	enum EGpuUpdateList
	{
		eGpuUpdateList_Spawn,
		eGpuUpdateList_InitUpdate,
		eGpuUpdateList_Update,
		eGpuUpdateList_InitSubInstance,
		eGpuUpdateList_COUNT
	};

	std::vector<_smart_ptr<gpu_pfx2::CFeature>> m_gpuUpdateLists[eGpuUpdateList_COUNT];

	typedef pfx2::TParticleId TParticleId;
	typedef DynArray<TParticleId, uint> TInstances;

	CParticleComponentRuntime(
	  IParticleEmitter* pEmitter,
	  pfx2::IParticleComponent* pComponent,
	  const SComponentParams& params);

	virtual ~CParticleComponentRuntime()
	{
	}
	virtual void        UpdateEmitterData() override;
	virtual EState      GetState() const override       { return m_state; };
	virtual bool        IsActive() const override       { return m_active; }
	virtual void        SetActive(bool active) override { m_active = active; }
	virtual const pfx2::IParticleComponent* 
		                GetComponent() const override   { return m_pComponent; }
	virtual bool        IsChild() const override        { return m_pComponent->GetParent() != nullptr; }

	virtual bool        IsValidForParams(const SComponentParams& parameters) override;
	virtual void        SetEnvironmentParameters(const SEnvironmentParameters& params) override { m_envParams = params; }

	virtual const AABB& GetBounds() const override;
	virtual void        AccumStats(pfx2::SParticleStats& stats) override;
	virtual bool        HasParticles() override { return m_parameters->numParticles != 0; }

	virtual void        AddSubInstances(TConstArray<SInstance> instances) override;
	virtual void        RemoveAllSubInstances() override;
	virtual void        ReparentParticles(TConstArray<TParticleId> swapIds) override;

	IParticleEmitter*   GetEmitter() const { return m_pEmitter; }
	void                Initialize();
	void                Reset();

	void                SetManagerSlot(int i) { m_parameters->managerSlot = i; }

	void                PrepareRelease();

	void                SetLifeTime(float lifeTime) { m_parameters->lifeTime = lifeTime; }

	int                 GetNumParticles()           { return m_parameters->numParticles; };

	// this is from the render thread
	void AddRemoveNewBornsParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void UpdateParticles(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void CalculateBounds(SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void SetBoundsFromManager(const SReadbackData* pData);
	void SetCounterFromManager(const uint32* pCounter);
	CGpuBuffer& PrepareForUse();

	void SpawnParticles(int instanceID, uint32 count);

	CParticleContainer*                GetContainer() { return &m_container; }

	void                               SetUpdateTexture(EFeatureUpdateSrvSlot slot, CTexture* pTexture);
	void                               SetUpdateBuffer(EFeatureUpdateSrvSlot slot, CGpuBuffer* pSRV);
	void                               SetUpdateFlags(uint64 flags);
	void                               SetUpdateConstantBuffer(EConstantBufferSlot slot, CConstantBufferPtr pConstantBuffer);

	SParticleInitializationParameters& GetParticleInitializationParameters() { return m_particleInitializationParameters.GetHostData(); }
	void                               SetInitializationSRV(EFeatureInitializationSrvSlot slot, CGpuBuffer* pSRV);
	void                               SetInitializationFlags(uint64 flags);

	void                               RemoveSubInstance(size_t instanceId);
	size_t                             GetNumInstances() const       { return m_subInstancesRenderThread.size(); }
	const uint&                        GetInstance(size_t idx) const { return m_subInstancesRenderThread[idx]; }

	// currently called from the render element mfDraw, but may go somewhere else
	void SwapToEnd(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	Vec3                                   GetPos() const { return m_parameters->emitterPosition; };

	gpu_physics::CParticleFluidSimulation* CreateFluidSimulation();
	gpu_physics::CParticleFluidSimulation* GetFluidSimulation() { return m_pFluidSimulation.get(); }
	void FluidCollisions(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void EvolveParticles(CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	SSpawnData& GetSpawnData(size_t idx) { return m_spawnData[idx]; }

private:
	void AddRemoveParticles(const SUpdateContext& context);
	void UpdateNewBorns(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void FillKillList(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void UpdateFeatures(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void Sort(const SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList);

	// update passes with varying material flags
	void InitializePasses();
	// update passes with varying material flags
	void UpdatePasses();

	_smart_ptr<pfx2::IParticleComponent>                m_pComponent;
	IParticleEmitter*                                   m_pEmitter;
	gpu::CTypedConstantBuffer<SParticleParameters>      m_parameters;

	CParticleContainer                                  m_container;

	gpu::CTypedResource<int, gpu::BufferFlagsReadWrite> m_blockSums;
	gpu::CTypedResource<int, gpu::BufferFlagsReadWrite> m_killList;
	gpu::CTypedResource<uint, gpu::BufferFlagsDynamic>  m_newBornIndices;

	// the subinstances are double buffered, so the render thread can work
	// with a different set while the main thread (or any other thread, there is a lock)
	// manipulates them. They are synchronized in AddRemoveNewBornsParticles
	TInstances m_subInstances;
	TInstances m_subInstancesRenderThread;

	// m_parentData corresponds to the subInstances in m_subInstances, while the buffer
	// in m_parentDataRenderThread corresponds to subInstances in m_subInstancesRenderThread
	stl::aligned_vector<SInitialData, CRY_PLATFORM_ALIGNMENT>  m_parentData;
	gpu::CTypedResource<SInitialData, gpu::BufferFlagsDynamic> m_parentDataRenderThread;
	int                                                        m_parentDataSizeRenderThread;

	SComponentParams          m_params;

	std::vector<SSpawnData>   m_spawnData;
	std::vector<SSpawnEntry>  m_spawnEntries;
	AABB                      m_bounds;
	bool                      m_active;

	// Compute Passes

	// these pointers must be declared before the ComputeRenderPasses, for proper destruction order.
	gpu::CTypedConstantBuffer<SParticleInitializationParameters> m_particleInitializationParameters;

	std::unique_ptr<gpu_physics::CParticleFluidSimulation> m_pFluidSimulation;
	std::unique_ptr<gpu::CMergeSort>                       m_pMergeSort;

	_smart_ptr<CShader>      m_pShader;
	_smart_ptr<CShader>      m_pInitShader;
	_smart_ptr<CShader>      m_pUpdateShader;

	CGpuBuffer*              m_updateSrvSlots[eFeatureUpdateSrvSlot_COUNT];
	CTexture*                m_updateTextureSlots[eFeatureUpdateSrvSlot_COUNT];
	CConstantBufferPtr       m_updateConstantBuffers[eConstantBufferSlot_COUNT];
	uint64                   m_updateShaderFlags;

	CGpuBuffer*              m_initializationSrvSlots[eFeatureInitializationSrvSlot_COUNT];

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
	EState                   m_state;

	CryCriticalSection       m_cs;

	SEnvironmentParameters   m_envParams;
};
}
