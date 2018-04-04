// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     23/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLEEMITTER_H
#define PARTICLEEMITTER_H

#pragma once

#include "ParticleEffect.h"
#include "ParticleEnviron.h"
#include <CryRenderer/IGpuParticles.h>

namespace pfx2
{

struct SEmitterStats
{
	bool m_updated = false;
	bool m_rendered = false;
	uint m_runtimesUpdated = 0;
	uint m_runtimesRendered = 0;
	uint m_particlesRendered = 0;
	uint m_particlesUpdated = 0;
	uint m_particlesClipped = 0;
};

class CParticleEmitter : public IParticleEmitter, public Cry3DEngineBase
{
private:
	struct SRuntimeRef
	{
		SRuntimeRef(CParticleEffect* effect, CParticleEmitter* emitter, CParticleComponent* component, const SRuntimeInitializationParameters& params);
		SRuntimeRef() : pRuntime(nullptr), pComponent(nullptr) {}
		_smart_ptr<ICommonParticleComponentRuntime> pRuntime;
		CParticleComponent*                         pComponent;
	};

	typedef std::vector<SRuntimeRef>                          TComponentRuntimes;
	typedef std::vector<CParticleComponentRuntime*>           TCPUComponentRuntimes;
	typedef std::vector<gpu_pfx2::IParticleComponentRuntime*> TGPUComponentRuntimes;

public:
	CParticleEmitter(uint emitterId);
	~CParticleEmitter();

	// IRenderNode
	virtual EERType          GetRenderNodeType() override;
	virtual const char*      GetEntityClassName() const override;
	virtual const char*      GetName() const override;
	virtual void             SetMatrix(const Matrix34& mat) override;
	virtual Vec3             GetPos(bool bWorldOnly = true) const override;
	virtual void             Render(const struct SRendParams& rParam, const SRenderingPassInfo& passInfo) override;
	virtual IPhysicalEntity* GetPhysics() const override;
	virtual void             SetPhysics(IPhysicalEntity*) override;
	virtual void             SetMaterial(IMaterial* pMat) override;
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = 0) const override;
	virtual IMaterial*       GetMaterialOverride() override;
	virtual float            GetMaxViewDist() override;
	virtual void             Precache() override;
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual const AABB       GetBBox() const override;
	virtual void             FillBBox(AABB& aabb) override;
	virtual void             SetBBox(const AABB& WSBBox) override;
	virtual void             OffsetPosition(const Vec3& delta) override;
	virtual bool             IsAllocatedOutsideOf3DEngineDLL() override;
	virtual void             SetViewDistRatio(int nViewDistRatio) override;
	virtual void             ReleaseNode(bool bImmediate) override;
	virtual void             SetOwnerEntity(IEntity* pEntity) override { SetEntity(pEntity, m_entitySlot); }
	virtual IEntity*         GetOwnerEntity() const override { return m_entityOwner; }
	// ~IRenderNode

	// pfx2 IParticleEmitter
	virtual const IParticleEffect* GetEffect() const override;
	virtual void                   Activate(bool activate) override;
	virtual void                   Kill() override;
	virtual bool                   IsActive() const;
	virtual void                   SetLocation(const QuatTS& loc) override;
	virtual QuatTS                 GetLocation() const override { return m_location; }
	virtual void                   EmitParticle(const EmitParticleData* pData = NULL)  override;
	virtual IParticleAttributes& GetAttributes()  override    { return m_attributeInstance; }
	virtual void                 SetEntity(IEntity* pEntity, int nSlot) override;
	virtual void                 InvalidateCachedEntityData() override;
	virtual void                 SetTarget(const ParticleTarget& target) override;
	virtual bool                 UpdateStreamableComponents(float fImportance, const Matrix34A& objMatrix, IRenderNode* pRenderNode, float fEntDistance, bool bFullUpdate, int nLod) override;
	// ~pfx2 IParticleEmitter

	// pfx1 IParticleEmitter
	virtual int          GetVersion() const override                        { return 2; }
	virtual bool         IsAlive() const override                           { return m_registered; }
	virtual bool         IsInstant() const override                         { return false; }
	virtual void         Restart() override;
	virtual void         SetEffect(const IParticleEffect* pEffect) override {}
	virtual void         SetEmitGeom(const GeomRef& geom) override;
	virtual void         SetSpawnParams(const SpawnParams& spawnParams) override;
	virtual void         GetSpawnParams(SpawnParams& sp) const override;
	using                IParticleEmitter::GetSpawnParams;
	virtual void         Update() override;
	virtual uint         GetAttachedEntityId() override;
	virtual int          GetAttachedEntitySlot() override { return m_entitySlot; }
	// ~pfx1 CPArticleEmitter

	void                      InitSeed();
	void                      DebugRender() const;
	void                      PostUpdate();
	CParticleContainer&       GetParentContainer()         { return m_parentContainer; }
	const CParticleContainer& GetParentContainer() const   { return m_parentContainer; }
	virtual void              GetParentData(const int parentComponentId, const uint* parentParticleIds, const int numParentParticleIds, SInitialData* data) const override;
	const TComponentRuntimes& GetRuntimes() const          { return m_componentRuntimes; }
	void                      SetCEffect(CParticleEffect* pEffect);
	const CParticleEffect*    GetCEffect() const           { return m_pEffect; }
	CParticleEffect*          GetCEffect()                 { return m_pEffect; }
	void                      Register();
	void                      Unregister();
	void                      UpdateEmitGeomFromEntity();
	const SVisEnviron&        GetVisEnv() const            { return m_visEnviron; }
	const SPhysEnviron&       GetPhysicsEnv() const        { return m_physEnviron; }
	const SpawnParams&        GetSpawnParams() const       { return m_spawnParams; }
	const GeomRef&            GetEmitterGeometry() const   { return m_emitterGeometry; }
	QuatTS                    GetEmitterGeometryLocation() const;
	const CAttributeInstance& GetAttributeInstance() const { return m_attributeInstance; }
	const ParticleTarget&     GetTarget() const            { return m_target; }
	float                     GetViewDistRatio() const     { return m_viewDistRatio; }
	float                     GetTimeScale() const         { return Cry3DEngineBase::GetCVars()->e_ParticlesDebug & AlphaBit('z') ? 0.0f : m_spawnParams.fTimeScale; }
	CRenderObject*            GetRenderObject(uint threadId, uint renderObjectIdx);
	void                      SetRenderObject(CRenderObject* pRenderObject, uint threadId, uint renderObjectIdx);
	float                     GetDeltaTime() const         { return m_deltaTime; }
	float                     GetTime() const              { return m_time; }
	uint32                    GetInitialSeed() const       { return m_initialSeed; }
	uint32                    GetCurrentSeed() const       { return m_currentSeed; }
	uint                      GetEmitterId() const         { return m_emitterId; }
	ColorF                    GetProfilerColor() const     { return m_profilerColor; }
	uint                      GetParticleSpec() const;

	bool                      IsIndependent() const { return Unique(); }
	bool                      HasParticles() const;
	bool                      WasRenderedLastFrame() const { return (m_lastTimeRendered >= m_time) && ((GetRndFlags() & ERF_HIDDEN) == 0); }

	void                      AccumStats(SParticleStats& stats);
	void                      AddUpdatedParticles(uint updatedParticles);
	void                      AddDrawCallCounts(uint numRendererdParticles, uint numClippedParticles);

private:
	void     UpdateBoundingBox(const float frameTime);
	void     UpdateRuntimeRefs();
	void     AddInstance();
	void     UpdateFromEntity();
	void     UpdateTargetFromEntity(IEntity* pEntity);
	IEntity* GetEmitGeometryEntity() const;
	void     ResetRenderObjects();

private:
	std::vector<CRenderObject*> m_pRenderObjects[RT_COMMAND_BUF_COUNT];
	SVisEnviron                 m_visEnviron;
	SPhysEnviron                m_physEnviron;
	SpawnParams                 m_spawnParams;
	CAttributeInstance          m_attributeInstance;
	AABB                        m_bounds;
	float                       m_resetBoundsCache;
	CParticleContainer          m_parentContainer;
	TComponentRuntimes          m_componentRuntimes;
	TCPUComponentRuntimes       m_cpuComponentRuntimes;
	TGPUComponentRuntimes       m_gpuComponentRuntimes;
	CParticleEffect*            m_pEffect;
	SEmitterStats               m_emitterStats;
	CryMutex                    m_statsMutex;
	QuatTS                      m_location;
	IEntity*                    m_entityOwner;
	int                         m_entitySlot;
	ParticleTarget              m_target;
	GeomRef                     m_emitterGeometry;
	int                         m_emitterGeometrySlot;
	ColorF                      m_profilerColor;
	float                       m_viewDistRatio;
	float                       m_time;
	float                       m_deltaTime;
	float                       m_primeTime;
	float                       m_lastTimeRendered;
	int                         m_editVersion;
	uint                        m_initialSeed;
	uint                        m_currentSeed;
	uint                        m_emitterId;
	bool                        m_registered;
	bool                        m_active;
};

}

#endif // PARTICLEEMITTER_H
