// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleContainer.h
//  Version:     v1.00
//  Created:     11/03/2010 by Corey (split out from other files).
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particlecontainer_h__
#define __particlecontainer_h__
#pragma once

#include "ParticleEffect.h"
#include "ParticleEnviron.h"
#include "ParticleUtils.h"
#include "ParticleList.h"
#include "Particle.h"
#include <CryRenderer/RenderElements/CREParticle.h>

class CParticleEmitter;
class CParticleSubEmitter;
struct SParticleVertexContext;

typedef ParticleList<CParticle>::Node TParticleElem;

//////////////////////////////////////////////////////////////////////////
struct CRY_ALIGN(16) SParticleUpdateContext
{
	float fUpdateTime;
	uint32 nEnvFlags;
	Vec3 vEmitBox;
	Vec3 vEmitScale;
	AABB* pbbDynamicBounds;
	bool bHasTarget;

	// Iteration support.
	float fMaxLinearDeviation;
	float fMaxLinearStepTime;
	float fMinStepTime;

	// Sorting support.
	Vec3 vMainCamPos;
	float fCenterCamDist;
	int nSortQuality;
	bool b3DRotation;

	// Updated per emitter.
	float fDensityAdjust;

	struct SSortElem
	{
		const CParticle* pPart;
		float            fDist;

		bool operator<(const SSortElem& r) const
		{ return fDist < r.fDist; }
	};
	Array<SSortElem> aParticleSort;

	struct SSpaceLoop
	{
		Vec3 vCenter, vSize;
		Vec3 vScaledAxes[3];
	};
	SSpaceLoop SpaceLoop;
};

//////////////////////////////////////////////////////////////////////////
// Particle rendering helper params.
struct CRY_ALIGN(16) SPartRenderParams
{
	float m_fCamDistance;
	float m_fMainBoundsScale;
	float m_fMaxAngularDensity;
	uint32 m_nRenFlags;
	TrinaryFlags<uint64> m_nRenObjFlags;
	uint16 m_nFogVolumeContribIdx;
	float m_fHDRDynamicMultiplier;
	uint16 m_nDeferredLightVolumeId;
};

//////////////////////////////////////////////////////////////////////////
// Contains, updates, and renders a list of particles, of a single effect

class CParticleContainer : public IParticleVertexCreator, public Cry3DEngineBase
{
public:

	CParticleContainer(CParticleContainer* pParent, CParticleEmitter* pMain, CParticleEffect const* pEffect);
	~CParticleContainer();

	// Associated structures.
	ILINE const CParticleEffect*        GetEffect() const { return m_pEffect; }
	ILINE CParticleEmitter&             GetMain()         { return *m_pMainEmitter; }
	ILINE const CParticleEmitter&       GetMain() const   { return *m_pMainEmitter; }

	ILINE ResourceParticleParams const& GetParams() const
	{
		return *m_pParams;
	}

	float      GetAge() const;

	ILINE bool IsIndirect() const
	{
		return m_pParentContainer != 0;
	}
	ILINE CParticleSubEmitter* GetDirectEmitter()
	{
		if (!m_pParentContainer && !m_Emitters.empty())
		{
			assert(m_Emitters.size() == 1);
			return &m_Emitters.front();
		}
		return NULL;
	}

	// If indirect container.
	ILINE CParticleContainer* GetParent() const
	{
		return m_pParentContainer;
	}

	void                 Reset();

	CParticleSubEmitter* AddEmitter(CParticleSource* pSource);
	CParticle*           AddParticle(SParticleUpdateContext& context, const CParticle& part);
	void                 UpdateParticles();
	void                 SyncUpdateParticles();
	void                 UpdateEmitters(SParticleUpdateContext* pEmitContext = NULL);

	void                 EmitParticle(const EmitParticleData* pData);

	uint16               GetNextEmitterSequence()
	{
		return m_nEmitterSequence++;
	}
	void  UpdateEffects();
	float InvalidateStaticBounds();
	void  Render(SRendParams const& RenParams, SPartRenderParams const& PRParams, const SRenderingPassInfo& passInfo);
	void  RenderGeometry(const SRendParams& RenParams, const SRenderingPassInfo& passInfo);
	void  RenderDecals(const SRenderingPassInfo& passInfo);
	void  RenderLights(const SRendParams& RenParams, const SRenderingPassInfo& passInfo);
	void  ResetRenderObjects();

	// Bounds functions.
	void   UpdateState();
	void   SetDynamicBounds();

	uint32 NeedsDynamicBounds() const
	{
#ifndef _RELEASE
		if (GetCVars()->e_ParticlesDebug & AlphaBit('d'))
			// Force all dynamic bounds computation and culling.
			return EFF_DYNAMIC_BOUNDS;
#endif
		return (m_nEnvFlags | m_nChildFlags) & EFF_DYNAMIC_BOUNDS;
	}
	bool StaticBoundsStable() const
	{
		return GetAge() >= m_fAgeStaticBoundsStable;
	}
	AABB const& GetBounds() const
	{
		return m_bbWorld;
	}
	AABB const& GetStaticBounds() const
	{
		return m_bbWorldStat;
	}
	AABB const& GetDynamicBounds() const
	{
		return m_bbWorldDyn;
	}

	uint32 GetEnvironmentFlags() const;

	size_t GetNumParticles() const
	{
		return m_Particles.size();
	}

	uint32 GetChildFlags() const
	{
		return m_nChildFlags;
	}

	float GetContainerLife() const
	{
		return m_fContainerLife;
	}
	void  UpdateContainerLife(float fAgeAdjust = 0.f);

	bool  GetTarget(ParticleTarget& target, const CParticleSubEmitter* pSubEmitter) const;

	float GetTimeToUpdate() const
	{
		return GetAge() - m_fAgeLastUpdate;
	}

	// Temp functions to update edited effects.
	inline bool IsUsed() const
	{
		return !m_bUnused;
	}

	void SetUsed(bool b)
	{
		m_bUnused = !b;
	}

	int GetHistorySteps() const
	{
		return m_nHistorySteps;
	}
	uint32 NeedsCollisionInfo() const
	{
		return GetEnvironmentFlags() & ENV_COLLIDE_INFO;
	}
	float GetMaxParticleFullLife() const
	{
		return m_fMaxParticleFullLife;
	}

	void OnEffectChange();

	void ComputeUpdateContext(SParticleUpdateContext& context, float fUpdateTime);

	// Stat/profile functions.
	SContainerCounts& GetCounts()
	{
		return m_Counts;
	}

	void GetCounts(SParticleCounts& counts) const;
	void ClearCounts()
	{
		ZeroStruct(m_Counts);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const;

	//////////////////////////////////////////////////////////////////////////
	// IParticleVertexCreator methods

	virtual void ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uVertexFlags, float fMaxPixels);

	// Other methods.
	bool NeedsUpdate() const;

	// functions for particle generate and culling of vertices directly into Video Memory
	int  CullParticles(SParticleVertexContext& Context, int& nVertices, int& nIndices, uint8 auParticleAlpha[]);
	void WriteVerticesToVMEM(SParticleVertexContext& Context, SRenderVertices& RenderVerts, const uint8 auParticleAlpha[]);

	void SetNeedJobUpdate(uint8 n)
	{
		m_nNeedJobUpdate = n;
	}
	uint8 NeedJobUpdate() const
	{
		return m_nNeedJobUpdate;
	}
	bool WasRenderedPrevFrame() const
	{
		return m_nNeedJobUpdate > 0;
	}

	int GetEmitterCount() const
	{
		return m_Emitters.size();
	}

	void OffsetPosition(const Vec3& delta);
	void OnHidden();
	void OnUnhidden();

private:
	const ResourceParticleParams*      m_pParams;     // Pointer to particle params (effect or code).
	_smart_ptr<CParticleEffect>        m_pEffect;     // Particle effect used for this emitter.
	uint32                             m_nEnvFlags;
	uint32                             m_nChildFlags; // Summary of rendering/environment info for child containers.

	ParticleList<CParticleSubEmitter>  m_Emitters;                    // All emitters into this container.
	ParticleList<EmitParticleData>     m_DeferredEmitParticles;       // Data for client EmitParticles calls, deferred till next update.
	ParticleList<_smart_ptr<IStatObj>> m_ExternalStatObjs;            // Any StatObjs from EmitParticle; released on destruction.

	uint16                             m_nHistorySteps;
	uint16                             m_nEmitterSequence;

	ParticleList<CParticle>            m_Particles;

	CRenderObject*                     m_pBeforeWaterRO[RT_COMMAND_BUF_COUNT];
	CRenderObject*                     m_pAfterWaterRO[RT_COMMAND_BUF_COUNT];
	CRenderObject*                     m_pRecursiveRO[RT_COMMAND_BUF_COUNT];

	// Last time when emitter updated, and static bounds validated.
	float m_fAgeLastUpdate;
	float m_fAgeStaticBoundsStable;
	float m_fContainerLife;

	// Final bounding volume for rendering. Also separate static & dynamic volumes for sub-computation.
	AABB  m_bbWorld, m_bbWorldStat, m_bbWorldDyn;

	bool  m_bUnused;                                            // Temp var used during param editing.
	uint8 m_nNeedJobUpdate;                                     // Mark container as needing sprite rendering this frame.
	                                                            // Can be set to >1 to prime for threaded update next frame.

	// Associated structures.
	CParticleContainer* m_pParentContainer;                     // Parent container, if indirect.
	CParticleEmitter*   m_pMainEmitter;                         // Emitter owning this container.
	float               m_fMaxParticleFullLife;                 // Cached value indicating max update time necessary.

	SContainerCounts    m_Counts;                               // Counts for stats.

	void  ComputeStaticBounds(AABB& bb, bool bWithSize = true, float fMaxLife = fHUGE);

	float GetEmitterLife() const;
	float GetMaxParticleScale() const;
	int   GetMaxParticleCount(const SParticleUpdateContext& context) const;
	void  UpdateParticleStates(SParticleUpdateContext& context);
	void  SetScreenBounds(const CCamera& cam, TRect_tpl<uint16> &bounds);

	CRenderObject* CreateRenderObject(uint64 nObjFlags, const SRenderingPassInfo& passInfo);
};

#endif // __particlecontainer_h__
