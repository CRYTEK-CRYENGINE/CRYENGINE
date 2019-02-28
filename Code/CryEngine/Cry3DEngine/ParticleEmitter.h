// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleEmitter.h
//  Version:     v1.00
//  Created:     18/7/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particleemitter_h__
#define __particleemitter_h__
#pragma once

#include "ParticleEffect.h"
#include "ParticleEnviron.h"
#include "ParticleContainer.h"
#include "ParticleSubEmitter.h"
#include "ParticleManager.h"

#include <CryEntitySystem/IEntity.h>

#undef PlaySound

class CParticle;

//////////////////////////////////////////////////////////////////////////
// A top-level emitter system, interfacing to 3D engine
class CParticleEmitter : public IParticleEmitter, public CParticleSource
{
public:

	CParticleEmitter(const IParticleEffect* pEffect, const QuatTS& loc, const SpawnParams* pSpawnParams = NULL);
	~CParticleEmitter();

	//////////////////////////////////////////////////////////////////////////
	// IRenderNode implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void        ReleaseNode(bool bImmediate) { CRY_ASSERT((m_dwRndFlags & ERF_PENDING_DELETE) == 0); Register(false, bImmediate); Kill(); }
	virtual EERType     GetRenderNodeType()          { return eERType_ParticleEmitter; }

	virtual char const* GetName() const              { return m_pTopEffect ? m_pTopEffect->GetName() : ""; }
	virtual char const* GetEntityClassName() const   { return "ParticleEmitter"; }
	virtual string      GetDebugString(char type = 0) const;

	virtual Vec3        GetPos(bool bWorldOnly = true) const { return GetLocation().t; }

	virtual const AABB  GetBBox() const                      { return m_bbWorld; }
	virtual void        SetBBox(const AABB& WSBBox)          { m_bbWorld = WSBBox; }
	virtual void        FillBBox(AABB& aabb)                 { aabb = GetBBox(); }

	virtual void        GetLocalBounds(AABB& bbox);
	virtual void        SetViewDistRatio(int nViewDistRatio)
	{
		// Override to cache normalized value.
		IRenderNode::SetViewDistRatio(nViewDistRatio);
		m_fViewDistRatio = GetViewDistRatioNormilized();
	}
	ILINE float              GetViewDistRatioFloat() const { return m_fViewDistRatio; }
	virtual float            GetMaxViewDist();
	virtual void             UpdateStreamingPriority(const SUpdateStreamingPriorityContext& context);

	virtual void             SetMatrix(Matrix34 const& mat)    { if (mat.IsValid()) SetLocation(QuatTS(mat)); }

	virtual void             SetMaterial(IMaterial* pMaterial) { m_pMaterial = pMaterial; }
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = NULL) const;
	virtual IMaterial*       GetMaterialOverride()             { return m_pMaterial; }

	virtual IPhysicalEntity* GetPhysics() const                { return 0; }
	virtual void             SetPhysics(IPhysicalEntity*)      {}

	virtual void             Render(SRendParams const& rParam, const SRenderingPassInfo& passInfo);

	virtual void             Hide(bool bHide);

	virtual void             GetMemoryUsage(ICrySizer* pSizer) const;

	//////////////////////////////////////////////////////////////////////////
	// IParticleEmitter implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual int                    GetVersion() const { return 1; }
	virtual void                   SetEffect(IParticleEffect const* pEffect);
	virtual const IParticleEffect* GetEffect() const
	{ return m_pTopEffect.get(); };

	virtual QuatTS        GetLocation() const
	{ return CParticleSource::GetLocation(); }
	virtual void          SetLocation(const QuatTS& loc);

	ParticleTarget const& GetTarget() const
	{ return m_Target; }
	virtual void          SetTarget(ParticleTarget const& target)
	{
		if ((int)target.bPriority >= (int)m_Target.bPriority)
			m_Target = target;
	}
	virtual void                 SetEmitGeom(const GeomRef& geom);
	virtual void                 SetSpawnParams(const SpawnParams& spawnParams);
	virtual void                 GetSpawnParams(SpawnParams& sp) const { sp = m_SpawnParams; }
	const SpawnParams&           GetSpawnParams() const                { return m_SpawnParams; }

	virtual bool                 IsAlive() const;
	virtual void                 Activate(bool bActive);
	virtual void                 Kill();
	virtual void                 Restart();
	virtual void                 Update();
	virtual void                 EmitParticle(const EmitParticleData* pData = NULL);

	virtual void                 SetEntity(IEntity* pEntity, int nSlot);
	virtual void                 InvalidateCachedEntityData() final;
	virtual void                 OffsetPosition(const Vec3& delta);
	virtual EntityId             GetAttachedEntityId();
	virtual int                  GetAttachedEntitySlot()
	{ return m_nEntitySlot; }
	virtual IParticleAttributes& GetAttributes();

	virtual void   SetOwnerEntity(IEntity* pEntity) final { m_pOwnerEntity = pEntity; }
	virtual IEntity* GetOwnerEntity() const final         { return m_pOwnerEntity; }

	//////////////////////////////////////////////////////////////////////////
	// Other methods.
	//////////////////////////////////////////////////////////////////////////

	bool IsActive() const
	// Has particles
	{ return m_fAge <= m_fDeathAge; }

	void        UpdateEmitCountScale();
	float       GetNearestDistance(const Vec3& vPos, float fBoundsScale) const;

	void        SerializeState(TSerialize ser);
	void        Register(bool b, bool bImmediate = false);
	ILINE float GetEmitCountScale() const
	{ return m_fEmitCountScale; }

	void                RefreshEffect();

	void                UpdateEffects();

	void                UpdateState();
	void                UpdateResetAge();

	void                CreateIndirectEmitters(CParticleSource* pSource, CParticleContainer* pCont);

	SPhysEnviron const& GetPhysEnviron() const
	{
		return m_PhysEnviron;
	}
	SVisEnviron const& GetVisEnviron() const
	{
		return m_VisEnviron;
	}
	void OnVisAreaDeleted(IVisArea* pVisArea)
	{
		m_VisEnviron.OnVisAreaDeleted(pVisArea);
	}

	void GetDynamicBounds(AABB& bb) const
	{
		bb.Reset();
		for (const auto& c : m_Containers)
			bb.Add(c.GetDynamicBounds());
	}
	ILINE uint32 GetEnvFlags() const
	{
		return m_nEnvFlags & CParticleManager::Instance()->GetAllowedEnvironmentFlags();
	}
	void AddEnvFlags(uint32 nFlags)
	{
		m_nEnvFlags |= nFlags;
	}

	float GetParticleScale() const
	{
		// Somewhat obscure. But top-level emitters spawned from entities,
		// and not attached to other objects, should apply the entity scale to their particles.
		if (!GetEmitGeom())
			return m_SpawnParams.fSizeScale * GetLocation().s;
		else
			return m_SpawnParams.fSizeScale;
	}

	void InvalidateStaticBounds()
	{
		m_bbWorld.Reset();
		for (auto& c : m_Containers)
		{
			float fStableTime = c.InvalidateStaticBounds();
			m_fBoundsStableAge = max(m_fBoundsStableAge, fStableTime);
		}
	}
	void     RenderDebugInfo();
	void     UpdateFromEntity();
	bool     IsIndependent() const
	{
		return  Unique();
	}
	bool NeedSerialize() const
	{
		return IsIndependent() && !m_pTopEffect->IsTemporary();
	}
	float TimeNotRendered() const
	{
		return GetAge() - m_fAgeLastRendered;
	}

	void GetCounts(SParticleCounts& counts, bool bClear = false) const
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		counts.emitters.alloc += 1.f;
		if (IsActive())
		{
			counts.emitters.alive += 1.f;
			counts.emitters.updated += 1.f;
		}
		if (TimeNotRendered() == 0.f)
			counts.emitters.rendered += 1.f;

		for (const auto& c : m_Containers)
		{
			c.GetCounts(counts);
			if (bClear)
				non_const(c).ClearCounts();
		}
	}
	void GetAndClearCounts(SParticleCounts& counts)
	{
		GetCounts(counts, true);
	}
	
	ParticleList<CParticleContainer> const& GetContainers() const
	{
		return m_Containers;
	}

	bool IsEditSelected() const
	{
#if CRY_PLATFORM_DESKTOP
		if (gEnv->IsEditing())
		{
			if (m_pOwnerEntity)
			{
				if (IEntityRender* pIEntityRender = m_pOwnerEntity->GetRenderInterface())
				{
					if (IRenderNode* pRenderNode = pIEntityRender->GetRenderNode())
						return (pRenderNode->GetRndFlags() & ERF_SELECTED) != 0;
				}
			}
		}
#endif
		return false;
	}

	void AddUpdateParticlesJob();
	void SyncUpdateParticlesJob();
	void UpdateAllParticlesJob();

	void Reset()
	{
		SetUpdateParticlesJobState(nullptr);
		Register(false);

		// Free unneeded memory.
		m_Containers.clear();

		// Release and remove external geom refs.
		GeomRef::operator=(GeomRef());
	}

	void SetUpdateParticlesJobState(JobManager::SJobState* pJobState)
	{
		m_pUpdateParticlesJobState = pJobState;
	}

private:

	// Internal emitter flags, extend EParticleEmitterFlags
	enum EFlags
	{
		ePEF_HasPhysics        = BIT(6),
		ePEF_HasTarget         = BIT(7),
		ePEF_HasAttachment     = BIT(8),
		ePEF_NeedsEntityUpdate = BIT(9),
		ePEF_Registered        = BIT(10),
	};

	JobManager::SJobState* m_pUpdateParticlesJobState;

	// Constant values, effect-related.
	_smart_ptr<CParticleEffect> m_pTopEffect;
	_smart_ptr<IMaterial>       m_pMaterial;          // Override material for this emitter.

	// Cache values derived from the main effect.
	float       m_fMaxParticleSize;

	CTimeValue  m_timeLastUpdate;                     // Track this to automatically update age.
	SpawnParams m_SpawnParams;                        // External settings modifying emission.
	float       m_fEmitCountScale;                    // Composite particle count scale.
	float       m_fViewDistRatio;                     // Normalised value of IRenderNode version.

	ParticleList<CParticleContainer>
	               m_Containers;
	uint32         m_nEnvFlags;                       // Union of environment flags affecting emitters.
	uint64         m_nRenObjFlags;                    // Union of render feature flags.
	ParticleTarget m_Target;                          // Target set from external source.

	AABB           m_bbWorld;                         // World bbox.

	float          m_fAgeLastRendered;
	float          m_fBoundsStableAge;                // Next age at which bounds stable.
	float          m_fResetAge;                       // Age to purge unseen particles.
	float          m_fStateChangeAge;                 // Next age at which a container's state changes.
	float          m_fDeathAge;                       // Age when all containers (particles) dead.

	// Entity connection params.
	int          m_nEntitySlot;
	IEntity*     m_pOwnerEntity = 0;
	uint32       m_nEmitterFlags;

	SPhysEnviron m_PhysEnviron;                       // Common physical environment (uniform forces only) for emitter.
	SVisEnviron  m_VisEnviron;

	// Functions.
	void                ResetUnseen();
	void                AllocEmitters();
	void                UpdateContainers();
	void                UpdateTimes(float fAgeAdjust = 0.f);

	void                AddEffect(CParticleContainer* pParentContainer, CParticleEffect const* pEffect, bool bUpdate = true);
	CParticleContainer* AddContainer(CParticleContainer* pParentContainer, const CParticleEffect* pEffect);
};

#endif // __particleemitter_h__
