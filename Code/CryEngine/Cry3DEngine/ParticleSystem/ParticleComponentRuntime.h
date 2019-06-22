// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "ParticleCommon.h"
#include "ParticleComponent.h"
#include "ParticleContainer.h"
#include <CryRenderer/IGpuParticles.h>

namespace pfx2
{

struct SSpawnerDesc
{
	SSpawnerDesc(TParticleId id = 0, float delay = 0.0f)
		: m_parentId(id), m_startDelay(delay) {}

	TParticleId m_parentId;
	float m_startDelay;
};

struct SMaxParticleCounts
{
	uint32 burst = 0;
	uint32 perFrame = 0;
	float  rate = 0;
};

class CParticleComponentRuntime : public _i_reference_target_t, public IParticleVertexCreator
{
public:
	CParticleComponentRuntime(CParticleEmitter* pEmitter, CParticleComponent* pComponent);
	~CParticleComponentRuntime();

	bool                          IsCPURuntime() const   { return !m_pGpuRuntime; }
	gpu_pfx2::IParticleComponentRuntime* GetGpuRuntime() const { return m_pGpuRuntime; }
	CParticleComponent*           GetComponent() const   { return m_pComponent; }
	bool                          IsValidForComponent() const;
	const AABB&                   GetBounds() const      { return m_pGpuRuntime ? m_pGpuRuntime->GetBounds() : m_bounds; }
	uint                          GetNumParticles() const;
	void                          AddBounds(const AABB& bounds);
	bool                          IsChild() const        { return m_pComponent->GetParentComponent() != nullptr; }
	void                          Reparent(TConstArray<TParticleId> swapIds);
	void                          AddSpawners(TVarArray<SSpawnerDesc> descs, bool cull = true);
	void                          RemoveAllSpawners();
	void                          RunParticles(uint count, float deltaTime);

	void                          ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;

	void                      Initialize();
	void                      Clear();
	CParticleEffect*          GetEffect() const          { return m_pComponent->GetEffect(); }
	CParticleEmitter*         GetEmitter() const         { return m_pEmitter; }

	CParticleComponentRuntime* ParentRuntime() const;
	
	CParticleContainer&       ParentContainer(EDataDomain domain = EDD_Particle);
	const CParticleContainer& ParentContainer(EDataDomain domain = EDD_Particle) const;
	CParticleContainer&       Container(EDataDomain domain = EDD_Particle)       { return m_containers[domain]; }
	const CParticleContainer& Container(EDataDomain domain = EDD_Particle) const { return m_containers[domain]; }

	template<typename T> TIStream<T>  IStream(TDataType<T> type, const T& defaultVal = T()) const    { return Container(type.info().domain).IStream(type, defaultVal); }
	template<typename T> TIOStream<T> IOStream(TDataType<T> type)                                    { return Container(type.info().domain).IOStream(type); }

	template<typename T> void FillData(TDataType<T> type, const T& data, SUpdateRange range) { Container(type.info().domain).FillData(type, data, range); }

	void                      UpdateAll();
	void                      AddParticles(TConstArray<SSpawnEntry> spawnEntries);

	bool                      IsAlive() const         { return m_alive; }
	void                      SetAlive()              { m_alive = true; }
	uint                      DomainSize(EDataDomain domain) const;

	void                      GetMaxParticleCounts(int& total, int& perFrame, float minFPS = 4.0f, float maxFPS = 120.0f) const;
	void                      GetEmitLocations(TVarArray<QuatTS> locations, uint firstInstance) const;
	void                      EmitParticle();

	bool                      HasParticles() const;
	void                      AccumStats();

	SChaosKey&                Chaos() const           { return m_chaos; }
	SChaosKeyV&               ChaosV() const          { return m_chaosV; }

	SUpdateRange              FullRange(EDataDomain domain = EDD_Particle) const     { return Container(domain).FullRange(); }
	SGroupRange               FullRangeV(EDataDomain domain = EDD_Particle) const    { return SGroupRange(Container(domain).FullRange()); }
	SUpdateRange              SpawnedRange(EDataDomain domain = EDD_Particle) const  { return Container(domain).SpawnedRange(); }
	SGroupRange               SpawnedRangeV(EDataDomain domain = EDD_Particle) const { return SGroupRange(Container(domain).SpawnedRange()); }

	const SComponentParams&   ComponentParams() const { return m_pComponent->GetComponentParams(); }
	STimingParams             GetMaxTimings() const;

	static TParticleHeap&     MemHeap();
	float                     DeltaTime() const;
	bool                      IsPreRunning() const              { return m_isPreRunning; }

	void                      ClearRenderObjects(uint threadId) { m_renderObjects[threadId].clear(); }
	void                      ResetRenderObjects(uint threadId) { m_renderObjects[threadId].reset(); }
	CRenderObject*            GetRenderObject(uint threadId, ERenderObjectFlags extraFlags);
	CRenderObject*            CreateRenderObject(uint64 renderFlags) const;

	// Legacy names
	CParticleContainer&       GetParentContainer()       { return ParentContainer(); }
	const CParticleContainer& GetParentContainer() const { return ParentContainer(); }
	CParticleContainer&       GetContainer()             { return Container(); }
	const CParticleContainer& GetContainer() const       { return Container(); }

private:
	void AddRemoveParticles();
	void UpdateSpawners();
	void RemoveParticles();
	void InitParticles();
	void AgeUpdate();
	void UpdateParticles();
	void CalculateBounds();
	void DebugStabilityCheck();
	void UpdateGPURuntime();

	struct PRenderObject
	{
		~PRenderObject()
		{
			if (m_ptr)
			{
				if (m_ptr->m_pRE)
					m_ptr->m_pRE->Release();
				if (gEnv->pRenderer)
					gEnv->pRenderer->EF_FreeObject(m_ptr);
			}
		}
		PRenderObject() {}
		PRenderObject(PRenderObject&& m)
			: m_ptr(m.m_ptr)
		{
			m.m_ptr = nullptr;
		}
		PRenderObject(const PRenderObject&) = delete;
		void operator=(const PRenderObject&) = delete;

		PRenderObject(CRenderObject* ptr)
			: m_ptr(ptr) {}
		void operator=(CRenderObject* ptr)
		{
			this->~PRenderObject();
			m_ptr = ptr;
		}

		operator CRenderObject*() const { return m_ptr; }
		CRenderObject* operator->() const { return m_ptr; }

	private:
		CRenderObject* m_ptr = nullptr;
	};

	_smart_ptr<CParticleComponent>       m_pComponent;
	CParticleEmitter*                    m_pEmitter;
	ElementTypeArray<CParticleContainer> m_containers;
	TReuseArray<PRenderObject>           m_renderObjects[RT_COMMAND_BUF_COUNT];
	AABB                                 m_bounds;
	bool                                 m_alive;
	bool                                 m_isPreRunning;
	float                                m_deltaTime;
	SChaosKey mutable                    m_chaos;
	SChaosKeyV mutable                   m_chaosV;

	_smart_ptr<gpu_pfx2::IParticleComponentRuntime> m_pGpuRuntime;
	TSmallArray<SSpawnEntry>             m_GPUSpawnEntries;
};

template<typename T>
struct SDynamicData : THeapArray<T>
{
	SDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, EDataDomain domain, SUpdateRange range)
		: THeapArray<T>(runtime.MemHeap(), range.size())
	{
		memset(this->data(), 0, this->size_mem());
		runtime.GetComponent()->GetDynamicData(runtime, type, this->data(), domain, range);
	}
};



}

