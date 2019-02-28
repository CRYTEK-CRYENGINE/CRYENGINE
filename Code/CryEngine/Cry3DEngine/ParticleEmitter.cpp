// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleEmitter.cpp
//  Created:     18/7/2003 by Timur.
//  Modified:    17/3/2005 by Scott Peter
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleEmitter.h"
#include "ParticleContainer.h"
#include "ParticleSubEmitter.h"
#include <CryAnimation/ICryAnimation.h>
#include "Particle.h"
#include "FogVolumeRenderNode.h"
#include <CryMemory/STLGlobalAllocator.h>
#include <CryString/StringUtils.h>

#include <CryThreading/IJobManager_JobDelegator.h>

static const float fUNSEEN_EMITTER_RESET_TIME = 5.f;    // Max time to wait before freeing unseen emitter resources
static const float fVELOCITY_SMOOTHING_TIME = 0.125f;   // Interval to smooth computed entity velocity

/*
   Scheme for Emitter updating & bounding volume computation.

   Whenever possible, we update emitter particles only on viewing.
   However, the bounding volume for all particles must be precomputed,
   and passed to the renderer,	for visibility. Thus, whenever possible,
   we precompute a Static Bounding Box, loosely estimating the possible
   travel of all particles, without actually updating them.

   Currently, Dynamic BBs are computed for emitters that perform physics,
   or are affected by non-uniform physical forces.
 */

///////////////////////////////////////////////////////////////////////////////////////////////
void UpdateParticlesEmitter(CParticleEmitter* pEmitter)
{
	pEmitter->UpdateAllParticlesJob();
}

DECLARE_JOB("Particles:Update", TUpdateParticlesJob, UpdateParticlesEmitter);

JobManager::SJobState* CParticleBatchDataManager::AddUpdateJob(CParticleEmitter* pEmitter)
{
	JobManager::SJobState* pJobState = GetJobState();

	TUpdateParticlesJob job(pEmitter);
	job.RegisterJobState(pJobState);
	job.Run();
	return pJobState;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void CParticleEmitter::AddUpdateParticlesJob()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (!GetCVars()->e_ParticlesThread)
	{
		UpdateParticlesEmitter(this);
		return;
	}

	// Queue background job for particle updates.
	assert(!m_pUpdateParticlesJobState);

	m_pUpdateParticlesJobState = CParticleManager::Instance()->AddUpdateJob(this);
}

void CParticleEmitter::UpdateAllParticlesJob()
{
	for (auto& c : m_Containers)
	{
		if (c.NeedJobUpdate())
			c.UpdateParticles();
		else
			c.UpdateEmitters();
	}
}

void CParticleEmitter::SyncUpdateParticlesJob()
{
	if (m_pUpdateParticlesJobState)
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
		PARTICLE_LIGHT_SYNC_PROFILER();
		gEnv->GetJobManager()->WaitForJob(*m_pUpdateParticlesJobState);
		m_pUpdateParticlesJobState = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CParticleEmitter::GetMaterial(Vec3*) const
{
	if (m_pMaterial)
		return m_pMaterial;
	if (!m_Containers.empty())
		return m_Containers.front().GetParams().pMaterial;
	return NULL;
}

float CParticleEmitter::GetMaxViewDist()
{
	// Max particles/radian, modified by emitter settings.
	return CParticleManager::Instance()->GetMaxAngularDensity(gEnv->pSystem->GetViewCamera())
	       * m_fMaxParticleSize
	       * GetParticleScale()
	       * m_fViewDistRatio;
}

bool CParticleEmitter::IsAlive() const
{
	if (m_fAge <= m_fDeathAge)
		return true;

	if (GetSpawnParams().fPulsePeriod > 0.f)
		// Between pulses.
		return true;

	if (m_fAge < 0.f)
		// Emitter paused.
		return true;

	// Natural death.
	return false;
}

void CParticleEmitter::UpdateState()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	uint32 nEnvFlags = GetEnvFlags();
	Vec3 vPos = GetLocation().t;

	bool bUpdateBounds = (nEnvFlags & REN_ANY) && (
	  m_bbWorld.IsReset() ||
	  (nEnvFlags & EFF_DYNAMIC_BOUNDS) ||
#ifndef _RELEASE
	  (GetCVars()->e_ParticlesDebug & AlphaBit('d')) ||
#endif
	  m_fAge <= m_fBoundsStableAge
	  );

	if (m_bbWorld.IsReset() && nEnvFlags & ENV_PHYS_AREA)
	{
		// For initial bounds computation, query the physical environment at the origin.
		m_PhysEnviron.Update(CParticleManager::Instance()->GetPhysEnviron(), AABB(vPos),
			Get3DEngine()->GetVisAreaFromPos(vPos) != NULL, nEnvFlags, true, this);
	}

	bool bUpdateState = (GetRndFlags()&ERF_HIDDEN)==0 && (bUpdateBounds || m_fAge >= m_fStateChangeAge);
	if (bUpdateState)
	{
		m_fStateChangeAge = fHUGE;

		// Update states and bounds.
		if (!m_Containers.empty())
			UpdateContainers();
		else
			AllocEmitters();

		if (bUpdateBounds && !m_SpawnParams.bNowhere)
		{
			// Re-query environment to include full bounds, and water volumes for clipping.
			m_VisEnviron.Update(vPos, m_bbWorld);
			m_PhysEnviron.OnPhysAreaChange();
		}
	}

	CParticleManager::Instance()->GetPhysEnviron().Update(m_PhysEnviron, m_bbWorld, 
		m_VisEnviron.OriginIndoors(), nEnvFlags | ENV_WATER, true, this);
}

void CParticleEmitter::Activate(bool bActive)
{
	float fPrevAge = m_fAge;
	if (bActive)
		Start(min(-m_fAge, 0.f));
	else
		Stop();
	UpdateTimes(m_fAge - fPrevAge);
}

void CParticleEmitter::Restart()
{
	float fPrevAge = m_fAge;
	Start();
	UpdateTimes(m_fAge - fPrevAge);
}

void CParticleEmitter::UpdateTimes(float fAgeAdjust)
{
	if (m_Containers.empty() && m_pTopEffect)
	{
		// Set death age to effect maximum.
		m_fDeathAge = m_pTopEffect->Get(FMaxEffectLife().bAllChildren(1).fEmitterMaxLife(m_fStopAge).bParticleLife(1));
		m_fStateChangeAge = m_fAge;
	}
	else
	{
		m_fDeathAge = 0.f;
		m_fStateChangeAge = fHUGE;

		// Update subemitter timings, and compute exact death age.
		for (auto& c : m_Containers)
		{
			c.UpdateContainerLife(fAgeAdjust);
			m_fDeathAge = max(m_fDeathAge, c.GetContainerLife());
			m_fStateChangeAge = min(m_fStateChangeAge, c.GetContainerLife());
		}
	}

	m_fAgeLastRendered = min(m_fAgeLastRendered, m_fAge);
	UpdateResetAge();
}

void CParticleEmitter::Kill()
{
	if (m_fDeathAge >= 0.f)
	{
		CParticleSource::Kill();
		m_fDeathAge = -fHUGE;
	}
}

void CParticleEmitter::UpdateResetAge()
{
	m_fResetAge = m_fAge + fUNSEEN_EMITTER_RESET_TIME;
}

void CParticleEmitter::UpdateEmitCountScale()
{
	m_fEmitCountScale = m_SpawnParams.fCountScale * GetCVars()->e_ParticlesLod;
	if (m_SpawnParams.bCountPerUnit)
	{
		// Count multiplied by geometry extent.
		m_fEmitCountScale *= GetEmitGeom().GetExtent(m_SpawnParams.eAttachType, m_SpawnParams.eAttachForm);
		m_fEmitCountScale *= ScaleExtent(m_SpawnParams.eAttachForm, GetLocation().s);
	}
}

CParticleContainer* CParticleEmitter::AddContainer(CParticleContainer* pParentContainer, const CParticleEffect* pEffect)
{
	void* pMem = m_Containers.push_back_new();
	if (pMem)
		return new(pMem) CParticleContainer(pParentContainer, this, pEffect);
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// CParticleEmitter implementation.
////////////////////////////////////////////////////////////////////////
CParticleEmitter::CParticleEmitter(const IParticleEffect* pEffect, const QuatTS& loc, const SpawnParams* pSpawnParams)
	: m_fMaxParticleSize(0.f)
	, m_fEmitCountScale(1.f)
	, m_fViewDistRatio(1.f)
	, m_timeLastUpdate(GetParticleTimer()->GetFrameStartTime())
	, m_fDeathAge(0.f)
	, m_fStateChangeAge(0.f)
	, m_fAgeLastRendered(0.f)
	, m_fBoundsStableAge(0.f)
	, m_fResetAge(fUNSEEN_EMITTER_RESET_TIME)
	, m_pUpdateParticlesJobState(NULL)
{
	m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP;

	m_nEntitySlot = -1;
	m_nEnvFlags = 0;
	m_bbWorld.Reset();

	m_Loc = loc;

	if (pSpawnParams)
		m_SpawnParams = *pSpawnParams;

	GetInstCount(GetRenderNodeType())++;

	SetEffect(pEffect);

	// Set active
	float fStartAge = 0.f;
	if (m_SpawnParams.bPrime)
		fStartAge = m_pTopEffect->GetEquilibriumAge(true);
	Start(-fStartAge);

	UpdateTimes();
}

//////////////////////////////////////////////////////////////////////////
CParticleEmitter::~CParticleEmitter()
{
	m_p3DEngine->FreeRenderNodeState(this); // Also does unregister entity.
	GetInstCount(GetRenderNodeType())--;
}

//////////////////////////////////////////////////////////////////////////
void CParticleEmitter::Register(bool b, bool bImmediate)
{
	if (!b)
	{
		if (m_nEmitterFlags & ePEF_Registered)
		{
			bImmediate ? m_p3DEngine->UnRegisterEntityDirect(this) : m_p3DEngine->UnRegisterEntityAsJob(this);
			m_nEmitterFlags &= ~ePEF_Registered;
		}
	}
	else
	{
		// Register top-level render node if applicable.
		if (m_SpawnParams.bNowhere)
		{
			bImmediate ? m_p3DEngine->UnRegisterEntityDirect(this) : m_p3DEngine->UnRegisterEntityAsJob(this);
		}
		else if (!(m_nEmitterFlags & ePEF_Registered))
		{
			if (!m_bbWorld.IsReset())
			{
				// Register node.
				// Register node.
				// Normally, use emitter's designated position for visibility.
				// However, if all bounds are computed dynamically, they might not contain the origin, so use bounds for visibility.
				SetRndFlags(ERF_REGISTER_BY_POSITION, m_bbWorld.IsContainPoint(GetPos()));
				SetRndFlags(ERF_REGISTER_BY_BBOX, m_SpawnParams.bRegisterByBBox);
				SetRndFlags(ERF_RENDER_ALWAYS, m_SpawnParams.bIgnoreVisAreas);
				m_p3DEngine->RegisterEntity(this);
				m_nEmitterFlags |= ePEF_Registered;
			}
			else if (m_nEmitterFlags & ePEF_Registered)
			{
				bImmediate ? m_p3DEngine->UnRegisterEntityDirect(this) : m_p3DEngine->UnRegisterEntityAsJob(this);
				m_nEmitterFlags &= ~ePEF_Registered;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CParticleEmitter::GetDebugString(char type) const
{
	string s = GetName();
	if (type == 's')
	{
		// Serialization debugging.
		if (m_pOwnerEntity)
			s += string().Format(" entity=%s slot=%d", m_pOwnerEntity->GetName(), m_nEntitySlot);
		if (IsIndependent())
			s += " Indep";
		if (IsActive())
			s += " Active";
		else if (IsAlive())
			s += " Dormant";
		else
			s += " Dead";
	}
	else
	{
		SParticleCounts counts;
		GetCounts(counts);
		s += string().Format(" E=%.0f P=%.0f R=%0.f", counts.components.updated, counts.particles.updated, counts.particles.rendered);
	}

	s += string().Format(" age=%.3f", GetAge());
	return s;
}

//////////////////////////////////////////////////////////////////////////
void CParticleEmitter::AddEffect(CParticleContainer* pParentContainer, const CParticleEffect* pEffect, bool bUpdate)
{
	CParticleContainer* pContainer = pParentContainer;

	// Add if playable in current config.
	if (pEffect->IsActive())
	{
		CParticleContainer* pNext = 0;
		if (bUpdate)
		{
			// Look for existing container
			for (auto& c : m_Containers)
			{
				if (!c.IsUsed())
				{
					if (c.GetEffect() == pEffect && 
						(c.GetParent() ? pEffect->GetIndirectParent() && c.GetParent()->IsUsed() : !pEffect->GetIndirectParent()))
					{
						pContainer = &c;
						c.SetUsed(true);
						c.OnEffectChange();
						c.InvalidateStaticBounds();
						break;
					}
					if (!pNext)
						pNext = &c;
				}
			}
		}
		if (pContainer == pParentContainer)
		{
			// Add new container
			if (void* pMem = pNext ? m_Containers.insert_new(pNext) : m_Containers.push_back_new())
				pContainer = new(pMem) CParticleContainer(pParentContainer, this, pEffect);
			else
				return;
		}
	}

	// Recurse effect tree.
	if (pEffect)
	{
		for (int i = 0, n = pEffect->GetChildCount(); i < n; i++)
		{
			if (CParticleEffect const* pChild = static_cast<const CParticleEffect*>(pEffect->GetChild(i)))
			{
				AddEffect(pContainer, pChild, bUpdate);
			}
		}
	}
}

void CParticleEmitter::RefreshEffect()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (!m_pTopEffect)
		return;

	m_pTopEffect->LoadResources(true);

	if (m_Containers.empty())
	{
		// Create new containers
		AddEffect(0, m_pTopEffect, false);
	}
	else
	{
		// Make sure the particle update isn't running anymore when we update an effect
		SyncUpdateParticlesJob();

		// Update existing containers
		for (auto& c : m_Containers)
			c.SetUsed(false);

		AddEffect(0, m_pTopEffect, true);

		// Remove unused containers, in reverse order, children before parents
		for (auto& c : m_Containers.reversed())
		{
			if (!c.IsUsed())
				m_Containers.erase(&c);
		}
	}

	m_nEnvFlags = 0;
	m_nRenObjFlags = 0;
	m_fMaxParticleSize = 0.f;

	for (auto& c : m_Containers)
	{
		c.ResetRenderObjects();
		m_nEnvFlags |= c.GetEnvironmentFlags();
		m_nRenObjFlags |= c.GetParams().nRenObjFlags.On;
		m_fMaxParticleSize = max(m_fMaxParticleSize, c.GetEffect()->GetMaxParticleSize() * c.GetParams().fViewDistanceAdjust);
		if (c.GetParams().fMinPixels)
			m_fMaxParticleSize = fHUGE;
	}

	m_bbWorld.Reset();
	m_VisEnviron.Invalidate();
	m_PhysEnviron.Clear();

	// Force state update.
	m_fStateChangeAge = -fHUGE;
	m_fDeathAge = fHUGE;
	m_nEmitterFlags |= ePEF_NeedsEntityUpdate;

	m_Vel = ZERO;

	Register(false, true);
}

void CParticleEmitter::SetEffect(IParticleEffect const* pEffect)
{
	if (m_pTopEffect != pEffect)
	{
		m_pTopEffect = &non_const(*static_cast<CParticleEffect const*>(pEffect));
		Reset();
		RefreshEffect();
	}
}

void CParticleEmitter::CreateIndirectEmitters(CParticleSource* pSource, CParticleContainer* pCont)
{
	for (auto& child : m_Containers)
	{
		if (child.GetParent() == pCont)
		{
			// Found an indirect container for this emitter.
			if (!child.AddEmitter(pSource))
				break;
		}
	}
}

void CParticleEmitter::SetLocation(const QuatTS& loc)
{
	if (!IsEquivalent(GetLocation(), loc, 0.0045f, 1e-5f))
	{
		InvalidateStaticBounds();
		m_VisEnviron.Invalidate();
	}

	if (!(m_nEmitterFlags & ePEF_HasPhysics))
	{
		// Infer velocity from movement, smoothing over time.
		if (float fStep = GetParticleTimer()->GetFrameTime())
		{
			Velocity3 vv;
			vv.FromDelta(QuatT(GetLocation().q, GetLocation().t), QuatT(loc.q, loc.t), max(fStep, fVELOCITY_SMOOTHING_TIME));
			m_Vel += vv;
		}
	}

	m_Loc = loc;
}

void CParticleEmitter::ResetUnseen()
{
	if (!(GetEnvFlags() & (EFF_DYNAMIC_BOUNDS | EFF_ANY)))
	{
		// Can remove all emitter structures.
		m_Containers.clear();
	}
	else
	{
		// Some containers require update every frame; reset the others.
		for (auto& c : m_Containers.reversed())
			if (!c.NeedsDynamicBounds())
				c.Reset();
	}
}

void CParticleEmitter::AllocEmitters()
{
	// (Re)alloc cleared emitters as needed.
	if (m_Containers.empty())
	{
		SyncUpdateParticlesJob();

		AddEffect(0, m_pTopEffect, false);
		UpdateContainers();
	}
}

void CParticleEmitter::UpdateContainers()
{
	AABB bbPrev = m_bbWorld;
	m_bbWorld.Reset();
	m_fDeathAge = 0.f;
	for (auto& c : m_Containers)
	{
		c.UpdateState();
		float fContainerLife = c.GetContainerLife();
		m_fDeathAge = max(m_fDeathAge, fContainerLife);
		if (fContainerLife > m_fAge)
			m_fStateChangeAge = min(m_fStateChangeAge, fContainerLife);

		if (c.GetEnvironmentFlags() & REN_ANY)
			m_bbWorld.Add(c.GetBounds());

		if (IsIndependent() && fContainerLife >= fHUGE * 0.5f)
			Warning("Immortal independent effect spawned: %s", c.GetEffect()->GetFullName().c_str());
	}
	if (!IsEquivalent(bbPrev, m_bbWorld))
		Register(false);
}

void CParticleEmitter::SetEmitGeom(GeomRef const& geom)
{
	if (geom != GetEmitGeom())
	{
		static_cast<GeomRef&>(*this) = geom;
		InvalidateStaticBounds();
	}

	if (m_SpawnParams.eAttachType)
	{
		m_nEmitterFlags |= ePEF_HasAttachment;
		GetEmitGeom().GetExtent(m_SpawnParams.eAttachType, m_SpawnParams.eAttachForm);
	}
	else
		m_nEmitterFlags &= ~ePEF_HasAttachment;
}

void CParticleEmitter::SetSpawnParams(SpawnParams const& spawnParams)
{
	if (ZeroIsHuge(spawnParams.fPulsePeriod) < 0.1f)
		Warning("Particle emitter (effect %s) PulsePeriod %g too low to be useful",
		        GetName(), spawnParams.fPulsePeriod);
	if (spawnParams.eAttachType != m_SpawnParams.eAttachType ||
	    spawnParams.fSizeScale != m_SpawnParams.fSizeScale ||
	    spawnParams.fSpeedScale != m_SpawnParams.fSpeedScale)
	{
		InvalidateStaticBounds();
	}
	if (spawnParams.bPrime > m_SpawnParams.bPrime)
	{
		float fStartAge = m_pTopEffect->GetEquilibriumAge(true);
		if (fStartAge > m_fAge)
		{
			Start(-fStartAge);
			UpdateTimes();
		}
	}

	m_SpawnParams = spawnParams;
}

bool GetPhysicalVelocity(Velocity3& Vel, IEntity* pEnt, const Vec3& vPos)
{
	if (pEnt)
	{
		if (IPhysicalEntity* pPhysEnt = pEnt->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pPhysEnt->GetStatus(&dyn))
			{
				Vel.vLin = dyn.v;
				Vel.vRot = dyn.w;
				Vel.vLin = Vel.VelocityAt(vPos - dyn.centerOfMass);
				return true;
			}
		}
		if (pEnt = pEnt->GetParent())
			return GetPhysicalVelocity(Vel, pEnt, vPos);
	}
	return false;
}

void CParticleEmitter::UpdateFromEntity()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	m_nEmitterFlags &= ~(ePEF_HasPhysics | ePEF_HasTarget | ePEF_HasAttachment);

	// Get emitter entity.
	IEntity* pEntity = m_pOwnerEntity;

	// Set external target.
	if (!m_Target.bPriority)
	{
		ParticleTarget target;
		if (pEntity)
		{
			for (IEntityLink* pLink = pEntity->GetEntityLinks(); pLink; pLink = pLink->next)
			{
				if (!stricmp(pLink->name, "Target") || !strnicmp(pLink->name, "Target-", 7))
				{
					if (IEntity* pTarget = gEnv->pEntitySystem->GetEntity(pLink->entityId))
					{
						target.bTarget = true;
						target.vTarget = pTarget->GetPos();

						Velocity3 Vel(ZERO);
						GetPhysicalVelocity(Vel, pTarget, GetLocation().t);
						target.vVelocity = Vel.vLin;

						AABB bb;
						pTarget->GetLocalBounds(bb);
						target.fRadius = max(bb.min.len(), bb.max.len());
						m_nEmitterFlags |= ePEF_HasTarget;
						break;
					}
				}
			}
		}

		if (target.bTarget != m_Target.bTarget || target.vTarget != m_Target.vTarget)
			InvalidateStaticBounds();
		m_Target = target;
	}

	bool bShadows = (GetEnvFlags() & REN_CAST_SHADOWS) != 0;

	// Get entity of attached parent.
	if (pEntity)
	{
		if (!(pEntity->GetFlags() & ENTITY_FLAG_CASTSHADOW))
			bShadows = false;

		if (pEntity->GetPhysics())
			m_nEmitterFlags |= ePEF_HasPhysics;

		if (pEntity->GetParent())
			pEntity = pEntity->GetParent();

		if (pEntity->GetPhysics())
			m_nEmitterFlags |= ePEF_HasPhysics;

		if (m_SpawnParams.eAttachType != GeomType_None)
		{
			// If entity attached, find attached physics and geometry on parent.
			GeomRef geom;
			int iSlot = geom.Set(pEntity);
			if (iSlot >= 0)
			{
				SetMatrix(pEntity->GetSlotWorldTM(iSlot));
			}

			SetEmitGeom(geom);
		}
	}

	if (bShadows)
		SetRndFlags(ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS, true);
	else
		SetRndFlags(ERF_CASTSHADOWMAPS, false);
}

void CParticleEmitter::GetLocalBounds(AABB& bbox)
{
	if (!m_bbWorld.IsReset())
	{
		bbox.min = m_bbWorld.min - GetLocation().t;
		bbox.max = m_bbWorld.max - GetLocation().t;
	}
	else
	{
		bbox.min = bbox.max = Vec3(0);
	}
}

void CParticleEmitter::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	m_fAge += (GetParticleTimer()->GetFrameStartTime() - m_timeLastUpdate).GetSeconds() * m_SpawnParams.fTimeScale;
	m_timeLastUpdate = GetParticleTimer()->GetFrameStartTime();

	if (m_SpawnParams.fPulsePeriod > 0.f && m_fAge < m_fStopAge)
	{
		// Apply external pulsing (restart).
		if (m_fAge >= m_SpawnParams.fPulsePeriod)
		{
			float fPulseAge = m_fAge - fmodf(m_fAge, m_SpawnParams.fPulsePeriod);
			m_fAge -= fPulseAge;
			m_fStopAge -= fPulseAge;
			UpdateTimes(-fPulseAge);
		}
	}
	else if (m_fAge > m_fDeathAge)
		return;

	if (m_nEmitterFlags & (ePEF_NeedsEntityUpdate | ePEF_HasTarget | ePEF_HasAttachment))
	{
		m_nEmitterFlags &= ~ePEF_NeedsEntityUpdate;
		UpdateFromEntity();
	}

	if (m_fAge > m_fResetAge)
	{
		ResetUnseen();
		m_fResetAge = fHUGE;
	}

	// Update velocity
	Velocity3 Vel;
	if ((m_nEmitterFlags & ePEF_HasPhysics) && GetPhysicalVelocity(Vel, m_pOwnerEntity, GetLocation().t))
	{
		// Get velocity from physics.
		m_Vel = Vel;
	}
	else
	{
		// Decay velocity, using approx exponential decay, so we reach zero soon.
		float fDecay = max(1.f - GetParticleTimer()->GetFrameTime() / fVELOCITY_SMOOTHING_TIME, 0.f);
		m_Vel *= fDecay;
	}

	if ((m_nEnvFlags & REN_BIND_CAMERA) && gEnv->pSystem)
	{
		SetMatrix(gEnv->pSystem->GetViewCamera().GetMatrix());
	}

	UpdateEmitCountScale();

	// Update emitter state and bounds.
	UpdateState();
}

void CParticleEmitter::UpdateEffects()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (!(GetRndFlags() & ERF_HIDDEN))
	{
		for (auto& c : m_Containers)
		{
			if (c.GetEnvironmentFlags() & EFF_ANY)
				c.UpdateEffects();
		}
	}
}

void CParticleEmitter::EmitParticle(const EmitParticleData* pData)
{
	AllocEmitters();
	if (!m_Containers.empty())
	{
		m_Containers.front().EmitParticle(pData);
		UpdateResetAge();
		m_fStateChangeAge = m_fAge;
	}
}

void CParticleEmitter::SetEntity(IEntity* pEntity, int nSlot)
{
	SetOwnerEntity(pEntity);

	m_nEntitySlot = nSlot;
	m_nEmitterFlags |= ePEF_NeedsEntityUpdate;
	m_Vel = ZERO;

	for (auto& c : m_Containers)
		if (CParticleSubEmitter* e = c.GetDirectEmitter())
			e->ResetLoc();
}

void CParticleEmitter::InvalidateCachedEntityData()
{
	m_nEmitterFlags |= ePEF_NeedsEntityUpdate;
	m_Vel = ZERO;
}

void CParticleEmitter::Render(SRendParams const& RenParams, const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	IF (!passInfo.RenderParticles(), 0)
		return;

	IF (passInfo.IsRecursivePass() && (m_nEnvFlags & REN_BIND_CAMERA), 0)
		// Render only in main camera.
		return;

	if (!IsActive())
		return;

	AllocEmitters();

	// Check combined emitter flags.
	uint32 nRenFlags = REN_SPRITE | REN_GEOMETRY;
	uint32 nRenRequiredFlags = 0;

	if (passInfo.IsShadowPass())
	{
		nRenRequiredFlags |= REN_CAST_SHADOWS;
	}
	else if (!passInfo.IsAuxWindow())
	{
		nRenFlags |= REN_DECAL;
		if (!passInfo.IsRecursivePass())
			nRenFlags |= REN_LIGHTS;
	}

	uint32 nEnvFlags = m_nEnvFlags & CParticleManager::Instance()->GetAllowedEnvironmentFlags();
	nRenFlags &= nEnvFlags;

	if (!nRenFlags)
		return;

	uint32 nRenRequiredCheckFlags = nRenRequiredFlags & ~nRenFlags;
	if ((nRenRequiredFlags & nEnvFlags) != nRenRequiredFlags)
		return;

	if (!passInfo.IsShadowPass())
	{
		m_fAgeLastRendered = GetAge();
		UpdateResetAge();
	}

	SPartRenderParams PartParams;
	ZeroStruct(PartParams);

	ColorF fogVolumeContrib;
	CFogVolumeRenderNode::TraceFogVolumes(GetPos(), fogVolumeContrib, passInfo);
	PartParams.m_nFogVolumeContribIdx = passInfo.GetIRenderView()->PushFogVolumeContribution(fogVolumeContrib, passInfo);

	// Compute camera distance with top effect's SortBoundsScale.
	PartParams.m_fMainBoundsScale = m_pTopEffect->IsEnabled() ? +m_pTopEffect->GetParams().fSortBoundsScale : 1.f;
	if (PartParams.m_fMainBoundsScale == 1.f && RenParams.fDistance > 0.f)
		// Use 3DEngine-computed BB distance.
		PartParams.m_fCamDistance = RenParams.fDistance;
	else
		// Compute with custom bounds scale and adjustment for camera inside BB.
		PartParams.m_fCamDistance = GetNearestDistance(passInfo.GetCamera().GetPosition(), PartParams.m_fMainBoundsScale);

	// Compute max allowed res.
	PartParams.m_fMaxAngularDensity = CParticleManager::Instance()->GetMaxAngularDensity(passInfo.GetCamera()) * GetParticleScale() * m_fViewDistRatio;
	bool bHDRModeEnabled = false;
	GetRenderer()->EF_Query(EFQ_HDRModeEnabled, bHDRModeEnabled);
	PartParams.m_fHDRDynamicMultiplier = bHDRModeEnabled ? HDRDynamicMultiplier : 1.f;

	PartParams.m_nRenObjFlags = CParticleManager::Instance()->GetRenderFlags();

	if (!passInfo.IsAuxWindow())
	{
		CLightVolumesMgr& lightVolumeManager = m_p3DEngine->GetLightVolumeManager();
		PartParams.m_nDeferredLightVolumeId = lightVolumeManager.RegisterVolume(GetPos(), m_bbWorld.GetRadius() * 0.5f, RenParams.nClipVolumeStencilRef, passInfo);
	}
	else
	{
		// Emitter in preview window, etc
		PartParams.m_nRenObjFlags.SetState(-1, FOB_INSHADOW | FOB_SOFT_PARTICLE | FOB_MOTION_BLUR);
	}

	if (!PartParams.m_nDeferredLightVolumeId)
		PartParams.m_nRenObjFlags.SetState(-1, FOB_LIGHTVOLUME);
	if (RenParams.m_pVisArea && !RenParams.m_pVisArea->IsAffectedByOutLights())
		PartParams.m_nRenObjFlags.SetState(-1, FOB_IN_DOORS | FOB_INSHADOW);
	if (RenParams.dwFObjFlags & FOB_NEAREST)
		PartParams.m_nRenObjFlags.SetState(-1, FOB_SOFT_PARTICLE);

	if (passInfo.IsAuxWindow())
	{
		// Stand-alone rendering.
		CParticleManager::Instance()->PrepareForRender(passInfo);
	}

	// Render relevant containers.
	PartParams.m_nRenFlags = nRenFlags;
	int nThreadJobs = 0;
	for (auto& c : m_Containers)
	{
		uint32 nContainerFlags = c.GetEnvironmentFlags();
		if (nContainerFlags & nRenFlags)
		{
			if ((nContainerFlags & nRenRequiredCheckFlags) == nRenRequiredFlags)
			{
				if (GetAge() <= c.GetContainerLife())
				{
					c.Render(RenParams, PartParams, passInfo);
					nThreadJobs += c.NeedJobUpdate();
				}
			}
		}
	}

	if (passInfo.IsAuxWindow())
	{
		// Stand-alone rendering. Submit render object.
		CParticleManager::Instance()->FinishParticleRenderTasks(passInfo);
		RenderDebugInfo();
	}
	else if (nThreadJobs && !m_pUpdateParticlesJobState)
	{
		// Schedule new emitter update, for containers first rendered this frame.
		AddUpdateParticlesJob();
	}
}

float CParticleEmitter::GetNearestDistance(const Vec3& vPos, float fBoundsScale) const
{
	if (fBoundsScale == 0.f)
		return (vPos - m_Loc.t).GetLength();

	AABB bb;
	float fAbsScale = abs(fBoundsScale);
	if (fAbsScale == 1.f)
	{
		bb = m_bbWorld;
	}
	else
	{
		// Scale toward origin
		bb.min = Lerp(m_Loc.t, m_bbWorld.min, fAbsScale);
		bb.max = Lerp(m_Loc.t, m_bbWorld.max, fAbsScale);
	}
	bb.min -= vPos;
	bb.max -= vPos;

	if (fBoundsScale < 0.f)
	{
		// Find furthest point.
		Vec3 vFar;
		vFar.x = max(abs(bb.min.x), abs(bb.max.x));
		vFar.y = max(abs(bb.min.y), abs(bb.max.y));
		vFar.z = max(abs(bb.min.z), abs(bb.max.z));
		return vFar.GetLength();
	}

	// Find nearest point.
	Vec3 vNear(ZERO);
	vNear.CheckMin(bb.max);
	vNear.CheckMax(bb.min);
	float fDistanceSq = vNear.GetLengthSquared();

	if (fDistanceSq > 0.f)
	{
		return sqrt_tpl(fDistanceSq);
	}
	else
	{
		// When point inside bounds, return negative distance, for stable sorting.
		vNear = -bb.min;
		vNear.CheckMin(bb.max);
		float fMinDist = min(vNear.x, min(vNear.y, vNear.z));
		return -fMinDist;
	}
}

// Disable printf argument verification since it is generated at runtime
#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic ignored "-Wformat-security"
	#else
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wformat-security"
	#endif
#endif

void CParticleEmitter::RenderDebugInfo()
{
	if (TimeNotRendered() < 0.25f)
	{
		bool bSelected = IsEditSelected();
		if (bSelected || (GetCVars()->e_ParticlesDebug & AlphaBit('b')))
		{
			AABB const& bb = GetBBox();
			if (bb.IsReset())
				return;

			AABB bbDyn;
			GetDynamicBounds(bbDyn);

			CCamera const& cam = GetISystem()->GetViewCamera();
			ColorF color(1, 1, 1, 1);

			// Compute label position, in bb clipped to camera.
			Vec3 vPos = GetLocation().t;
			Vec3 vLabel = vPos;

			// Clip to cam frustum.
			float fBorder = 1.f;
			for (int i = 0; i < FRUSTUM_PLANES; i++)
			{
				Plane const& pl = *cam.GetFrustumPlane(i);
				float f = pl.DistFromPlane(vLabel) + fBorder;
				if (f > 0.f)
					vLabel -= pl.n * f;
			}

			Vec3 vDist = vLabel - cam.GetPosition();
			vLabel += (cam.GetViewdir() * (vDist * cam.GetViewdir()) - vDist) * 0.1f;
			vLabel.CheckMax(bbDyn.min);
			vLabel.CheckMin(bbDyn.max);

			SParticleCounts counts;
			GetCounts(counts);
			float fPixToScreen = 1.f / ((float)GetRenderer()->GetOverlayWidth() * (float)GetRenderer()->GetOverlayHeight());

			if (!bSelected)
			{
				// Randomize hue by effect.
				color = CryStringUtils::CalculateHash(GetEffect()->GetName());
				color.r = color.r * 0.5f + 0.5f;
				color.g = color.g * 0.5f + 0.5f;
				color.b = color.b * 0.5f + 0.5f;

				// Modulate alpha by screen-space bounds extents.
				int aiOut[4];
				cam.CalcScreenBounds(aiOut, &bbDyn, GetRenderer()->GetOverlayWidth(), GetRenderer()->GetOverlayHeight());
				float fCoverage = (aiOut[2] - aiOut[0]) * (aiOut[3] - aiOut[1]) * fPixToScreen;

				// And by pixel and particle counts.
				float fWeight = sqrt_fast_tpl(fCoverage * counts.particles.rendered * counts.pixels.rendered * fPixToScreen);

				color.a = clamp_tpl(fWeight, 0.2f, 1.f);
			}

			IRenderAuxGeom* pRenAux = GetRenderer()->GetIRenderAuxGeom();
			pRenAux->SetRenderFlags(SAuxGeomRenderFlags());

			stack_string sLabel = GetName();
			sLabel += stack_string().Format(" P=%.0f F=%.3f S/D=",
			                                counts.particles.rendered,
			                                counts.pixels.rendered * fPixToScreen);
			if (counts.volume.dyn > 0.f)
			{
				sLabel += stack_string().Format("%.2f", counts.volume.stat / counts.volume.dyn);
				if (counts.volume.error > 0.f)
					sLabel += stack_string().Format(" E/D=%3f", counts.volume.error / counts.volume.dyn);
			}
			else
				sLabel += "~0";

			if (counts.particles.collideTest)
				sLabel += stack_string().Format(" Col=%.0f/%.0f", counts.particles.collideHit, counts.particles.collideTest);
			if (counts.particles.clip)
				sLabel += stack_string().Format(" Clip=%.0f", counts.particles.clip);

			ColorF colLabel = color;
			if (!bb.ContainsBox(bbDyn))
			{
				// Tint red.
				colLabel.r = 1.f;
				colLabel.g *= 0.5f;
				colLabel.b *= 0.5f;
				colLabel.a = (color.a + 1.f) * 0.5f;
			}
			else if (m_nEnvFlags & EFF_DYNAMIC_BOUNDS)
			{
				// Tint yellow.
				colLabel.r = 1.f;
				colLabel.g = 1.f;
				colLabel.b *= 0.5f;
			}
			IRenderAuxText::DrawLabelEx(vLabel, 1.5f, (float*)&colLabel, true, true, sLabel);

			// Compare static and dynamic boxes.
			if (!bbDyn.IsReset())
			{
				if (bbDyn.min != bb.min || bbDyn.max != bb.max)
				{
					// Separate dynamic BB.
					// Color outlying points bright red, draw connecting lines.
					ColorF colorGood = color * 0.8f;
					ColorF colorBad = colLabel;
					colorBad.a = colorGood.a;
					Vec3 vStat[8], vDyn[8];
					ColorB clrDyn[8];
					for (int i = 0; i < 8; i++)
					{
						vStat[i] = Vec3(i & 1 ? bb.max.x : bb.min.x, i & 2 ? bb.max.y : bb.min.y, i & 4 ? bb.max.z : bb.min.z);
						vDyn[i] = Vec3(i & 1 ? bbDyn.max.x : bbDyn.min.x, i & 2 ? bbDyn.max.y : bbDyn.min.y, i & 4 ? bbDyn.max.z : bbDyn.min.z);
						clrDyn[i] = bb.IsContainPoint(vDyn[i]) ? colorGood : colorBad;
						pRenAux->DrawLine(vStat[i], color, vDyn[i], clrDyn[i]);
					}

					// Draw dyn bb.
					if (bb.ContainsBox(bbDyn))
					{
						pRenAux->DrawAABB(bbDyn, false, colorGood, eBBD_Faceted);
					}
					else
					{
						pRenAux->DrawLine(vDyn[0], clrDyn[0], vDyn[1], clrDyn[1]);
						pRenAux->DrawLine(vDyn[0], clrDyn[0], vDyn[2], clrDyn[2]);
						pRenAux->DrawLine(vDyn[0], clrDyn[0], vDyn[4], clrDyn[4]);

						pRenAux->DrawLine(vDyn[1], clrDyn[1], vDyn[3], clrDyn[3]);
						pRenAux->DrawLine(vDyn[1], clrDyn[1], vDyn[5], clrDyn[5]);

						pRenAux->DrawLine(vDyn[2], clrDyn[2], vDyn[3], clrDyn[3]);
						pRenAux->DrawLine(vDyn[2], clrDyn[2], vDyn[6], clrDyn[6]);

						pRenAux->DrawLine(vDyn[3], clrDyn[3], vDyn[7], clrDyn[7]);

						pRenAux->DrawLine(vDyn[4], clrDyn[4], vDyn[5], clrDyn[5]);
						pRenAux->DrawLine(vDyn[4], clrDyn[4], vDyn[6], clrDyn[6]);

						pRenAux->DrawLine(vDyn[5], clrDyn[5], vDyn[7], clrDyn[7]);

						pRenAux->DrawLine(vDyn[6], clrDyn[6], vDyn[7], clrDyn[7]);
					}
				}
			}

			pRenAux->DrawAABB(bb, false, color, eBBD_Faceted);
		}

		if (bSelected)
		{
			// Draw emission volume(s)
			IRenderAuxGeom* pRenAux = GetRenderer()->GetIRenderAuxGeom();

			int c = 0;
			for (const auto& cont : m_Containers)
			{
				if (!cont.IsIndirect())
				{
					c++;
					ColorB color(c & 1 ? 255 : 128, c & 2 ? 255 : 128, c & 4 ? 255 : 128, 128);

					const ParticleParams& params = cont.GetEffect()->GetParams();
					pRenAux->DrawAABB(params.GetEmitOffsetBounds(), Matrix34(GetLocation()), false, color, eBBD_Faceted);
				}
			}
		}
	}
}
#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic error "-Wformat-security"
	#else
		#pragma GCC diagnostic pop
	#endif
#endif

void CParticleEmitter::SerializeState(TSerialize ser)
{
	// Time values.
	ser.Value("Age", m_fAge);
	ser.Value("StopAge", m_fStopAge);
	if (ser.IsReading())
	{
		UpdateTimes();
		m_timeLastUpdate = GetParticleTimer()->GetFrameStartTime();
	}

	// Spawn params.
	ser.Value("SizeScale", m_SpawnParams.fSizeScale);
	ser.Value("SpeedScale", m_SpawnParams.fSpeedScale);
	ser.Value("TimeScale", m_SpawnParams.fTimeScale);
	ser.Value("CountScale", m_SpawnParams.fCountScale);
	ser.Value("CountPerUnit", m_SpawnParams.bCountPerUnit);
	ser.Value("PulsePeriod", m_SpawnParams.fPulsePeriod);
}

void CParticleEmitter::Hide(bool bHide)
{
	IParticleEmitter::Hide(bHide);
	if (bHide)
	{
		for (auto& c : m_Containers)
		{
			c.OnHidden();
		}
	}
	else
	{
		if (m_nEnvFlags & EFF_ANY)
			AllocEmitters();
		for (auto& c : m_Containers)
		{
			c.OnUnhidden();
		}
	}
}

void CParticleEmitter::GetMemoryUsage(ICrySizer* pSizer) const
{
	m_Containers.GetMemoryUsage(pSizer);
	m_PhysEnviron.GetMemoryUsage(pSizer);
}

void CParticleEmitter::UpdateStreamingPriority(const SUpdateStreamingPriorityContext& context)
{
	FUNCTION_PROFILER_3DENGINE;

	for (const auto& c : m_Containers)
	{
		ResourceParticleParams const& params = c.GetParams();
		const float normalizedDist = context.distance * crymath::rcp_safe(params.fSize.GetMaxValue());
		if (CMatInfo* pMatInfo  = static_cast<CMatInfo*>(params.pMaterial.get()))
		{
			const float adjustedDist = normalizedDist
				* min(params.ShaderData.m_tileSize[0], params.ShaderData.m_tileSize[1]);
			pMatInfo->PrecacheMaterial(adjustedDist, nullptr, context.bFullUpdate, params.bDrawNear);
		}

		if (CStatObj* pStatObj = static_cast<CStatObj*>(params.pStatObj.get()))
		{
			m_pObjManager->PrecacheStatObj(pStatObj, context.lod, Matrix34A(m_Loc), pStatObj->GetMaterial(),
				context.importance, normalizedDist, context.bFullUpdate, params.bDrawNear);
		}
	}
}

EntityId CParticleEmitter::GetAttachedEntityId()
{
	if (GetOwnerEntity())
		return GetOwnerEntity()->GetId();
	return INVALID_ENTITYID;
}

IParticleAttributes& CParticleEmitter::GetAttributes()
{
	return m_pTopEffect->GetAttributes();
}

void CParticleEmitter::OffsetPosition(const Vec3& delta)
{
	SMoveState::OffsetPosition(delta);
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_Target.vTarget += delta;
	m_bbWorld.Move(delta);

	for (auto& c : m_Containers)
		c.OffsetPosition(delta);
}
