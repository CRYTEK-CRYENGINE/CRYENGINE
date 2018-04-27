// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleCommon.h"
#include "ParticleComponent.h"
#include <CryRenderer/IGpuParticles.h>

namespace pfx2
{

struct SInstance
{
	SInstance(TParticleId id = 0, float delay = 0.0f)
		: m_parentId(id), m_startDelay(delay) {}

	TParticleId m_parentId;
	float m_startDelay;
};

class CParticleComponentRuntime : public _i_reference_target_t, public IParticleVertexCreator
{
public:
	CParticleComponentRuntime(CParticleEmitter* pEmitter, CParticleComponent* pComponent);
	~CParticleComponentRuntime();

	CParticleComponentRuntime*    GetCpuRuntime()      { return !m_pGpuRuntime ? this : nullptr; }
	gpu_pfx2::IParticleComponentRuntime* GetGpuRuntime() { return m_pGpuRuntime; }
	CParticleComponent*           GetComponent() const { return m_pComponent; }
	bool                          IsValidForComponent() const;
	const AABB&                   GetBounds() const    { return m_pGpuRuntime ? m_pGpuRuntime->GetBounds() : m_bounds; }
	bool                          IsChild() const      { return m_pComponent->GetParentComponent() != nullptr; }
	void                          ReparentParticles(TConstArray<TParticleId> swapIds);
	void                          AddSubInstances(TVarArray<SInstance> instances);
	void                          RemoveAllSubInstances();
	void                          ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;

	void                      Initialize();
	CParticleEffect*          GetEffect() const          { return m_pComponent->GetEffect(); }
	CParticleEmitter*         GetEmitter() const         { return m_pEmitter; }
	const SComponentParams&   GetComponentParams() const { return m_pComponent->GetComponentParams(); }

	CParticleContainer&       GetParentContainer();
	const CParticleContainer& GetParentContainer() const;
	CParticleContainer&       GetContainer()            { return m_container; }
	const CParticleContainer& GetContainer() const      { return m_container; }

	void                      UpdateAll();
	void                      AddRemoveParticles(const SUpdateContext& context);
	void                      UpdateParticles(const SUpdateContext& context);
	void                      CalculateBounds();

	bool                      IsAlive() const               { return m_alive; }
	void                      SetAlive()                    { m_alive = true; }
	uint                      GetNumInstances() const       { return m_subInstances.size(); }
	const SInstance&          GetInstance(uint idx) const   { return m_subInstances[idx]; }
	SInstance&                GetInstance(uint idx)         { return m_subInstances[idx]; }
	TParticleId               GetParentId(uint idx) const   { return GetInstance(idx).m_parentId; }
	template<typename T> T&   GetInstanceData(uint idx, TDataOffset<T> offset)
	{
		const auto stride = GetComponentParams().m_instanceDataStride;
		CRY_PFX2_ASSERT(offset + sizeof(T) <= stride);
		CRY_PFX2_ASSERT(idx < m_subInstances.size());
		byte* addr = m_subInstanceData.data() + stride * idx + offset;
		return *reinterpret_cast<T*>(addr);
	}
	void                      GetEmitLocations(TVarArray<QuatTS> locations) const;
	void                      EmitParticle();
	SChaosKey                 MakeSeed(TParticleId id = 0) const;
	SChaosKey                 MakeParentSeed(TParticleId id = 0) const;

	bool                      HasParticles() const;
	void                      AccumStats();

private:
	void AddParticles(const SUpdateContext& context);
	void RemoveParticles(const SUpdateContext& context);
	void UpdateNewBorns(const SUpdateContext& context);
	void UpdateGPURuntime(const SUpdateContext& context);
	void AgeUpdate(const SUpdateContext& context);
	void DebugStabilityCheck();

	_smart_ptr<CParticleComponent>                  m_pComponent;
	CParticleEmitter*                               m_pEmitter;
	CParticleContainer                              m_container;
	TDynArray<SInstance>                            m_subInstances;
	TDynArray<byte>                                 m_subInstanceData;
	AABB                                            m_bounds;
	bool                                            m_alive;

	_smart_ptr<gpu_pfx2::IParticleComponentRuntime> m_pGpuRuntime;
};

}

