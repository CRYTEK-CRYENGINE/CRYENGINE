// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     02/04/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

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

	CParticleComponentRuntime*    GetCpuRuntime()      { return !m_pGpuRuntime ? this : nullptr; }
	gpu_pfx2::IParticleComponentRuntime* GetGpuRuntime()      { return m_pGpuRuntime; }
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

	uint                      GetNumInstances() const       { return m_subInstances.size(); }
	const SInstance&          GetInstance(uint idx) const   { return m_subInstances[idx]; }
	SInstance&                GetInstance(uint idx)         { return m_subInstances[idx]; }
	TParticleId               GetParentId(uint idx) const   { return GetInstance(idx).m_parentId; }
	template<typename T> T*   GetSubInstanceData(uint instanceId, TInstanceDataOffset offset);
	void                      AddSpawnEntry(const SSpawnEntry& entry);
	SChaosKey                 MakeSeed(TParticleId id = 0) const;
	SChaosKey                 MakeParentSeed(TParticleId id = 0) const;

	bool                      IsAlive() const;
	bool                      HasParticles() const;
	void                      AccumStats(SParticleStats& statsCPU, SParticleStats& statsGPU);

	SParticleStats::ParticleStats& GetParticleStats() { return m_particleStats; }

private:
	void AddParticles(const SUpdateContext& context);
	void RemoveParticles(const SUpdateContext& context);
	void UpdateNewBorns(const SUpdateContext& context);
	void UpdateGPURuntime(const SUpdateContext& context);
	void AgeUpdate(const SUpdateContext& context);
	void UpdateLocalSpace(SUpdateRange range);
	void DebugStabilityCheck();

	CParticleComponent*                             m_pComponent;
	CParticleEmitter*                               m_pEmitter;
	CParticleContainer                              m_container;
	TDynArray<SInstance>                            m_subInstances;
	TDynArray<byte>                                 m_subInstanceData;
	TDynArray<SSpawnEntry>                          m_spawnEntries;
	AABB                                            m_bounds;
	SParticleStats::ParticleStats                   m_particleStats;

	_smart_ptr<gpu_pfx2::IParticleComponentRuntime> m_pGpuRuntime;
};

}

#include "ParticleComponentImpl.h"

