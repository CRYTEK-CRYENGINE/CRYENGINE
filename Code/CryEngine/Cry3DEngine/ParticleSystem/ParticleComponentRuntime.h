// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     02/04/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLECOMPONENTRUNTIME_H
#define PARTICLECOMPONENTRUNTIME_H

#pragma once

#include "ParticleCommon.h"
#include "ParticleComponent.h"

namespace pfx2
{

class CParticleComponentRuntime : public IParticleComponentRuntime, public IParticleVertexCreator
{
public:
	typedef std::vector<SInstance, stl::aligned_alloc<SInstance, CRY_PFX2_PARTICLES_ALIGNMENT>> TInstances;

public:
	CParticleComponentRuntime(CParticleEmitter* pEmitter, CParticleComponent* pComponent);

	// ICommonParticleComponentRuntime
	// PFX2_TODO : Figure out a way to do static dispatches
	virtual CParticleComponent*        GetComponent() const override { return m_pComponent; }
	virtual const AABB&                GetBounds() const override    { return m_bounds; }
	virtual CParticleComponentRuntime* GetCpuRuntime() override      { return this; }
	virtual bool                       IsChild() const override      { return m_pComponent->GetParentComponent() != nullptr; }
	virtual bool                       IsActive() const override     { return m_active; }
	virtual void                       SetActive(bool active) override;
	virtual void                       ReparentParticles(TConstArray<TParticleId> swapIds) override;
	virtual bool                       IsValidForParams(const gpu_pfx2::SComponentParams& parameters) override { return true; }
	virtual void                       AddSubInstances(TConstArray<SInstance> instances) override;
	virtual void                       RemoveAllSubInstances() override;
	virtual void                       ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;
	// ~ICommonParticleComponentRuntime

	void                      Initialize();
	void                      Reset();
	CParticleEffect*          GetEffect() const          { return m_pComponent->GetEffect(); }
	CParticleEmitter*         GetEmitter() const         { return m_pEmitter; }
	const SComponentParams&   GetComponentParams() const { return m_pComponent->GetComponentParams(); }

	CParticleContainer&       GetParentContainer();
	const CParticleContainer& GetParentContainer() const;
	CParticleContainer&       GetContainer()            { return m_container; }
	const CParticleContainer& GetContainer() const      { return m_container; }

	void                      MainPreUpdate();
	void                      UpdateAll();
	void                      AddRemoveNewBornsParticles(const SUpdateContext& context);
	void                      UpdateParticles(const SUpdateContext& context);
	void                      OrphanAllParticles();
	void                      CalculateBounds();

	size_t                    GetNumInstances() const       { return m_subInstances.size(); }
	const SInstance&          GetInstance(size_t idx) const { return m_subInstances[idx]; }
	SInstance&                GetInstance(size_t idx)       { return m_subInstances[idx]; }
	TParticleId               GetParentId(size_t idx) const { return GetInstance(idx).m_parentId; }
	template<typename T> T*   GetSubInstanceData(size_t instanceId, TInstanceDataOffset offset);
	void                      SpawnParticles(CParticleContainer::SSpawnEntry const& entry);
	void                      GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents);
	void                      AccumStatsNonVirtual(SParticleStats& counts);
	const AABB&               GetBoundsNonVirtual() const { return m_bounds; }

private:
	void AddRemoveParticles(const SUpdateContext& context);
	void UpdateNewBorns(const SUpdateContext& context);
	void UpdateFeatures(const SUpdateContext& context);

	void AgeUpdate(const SUpdateContext& context);
	void UpdateLocalSpace(SUpdateRange range);

	void AlignInstances();

	void DebugStabilityCheck();

	CParticleComponent*                          m_pComponent;
	CParticleEmitter*                            m_pEmitter;
	CParticleContainer                           m_container;
	TInstances                                   m_subInstances;
	TDynArray<byte>                              m_subInstanceData;
	TDynArray<CParticleContainer::SSpawnEntry>   m_spawnEntries;
	AABB                                         m_bounds;
	bool                                         m_active;
};

}

#include "ParticleComponentImpl.h"

#endif // PARTICLECOMPONENTRUNTIME_H
