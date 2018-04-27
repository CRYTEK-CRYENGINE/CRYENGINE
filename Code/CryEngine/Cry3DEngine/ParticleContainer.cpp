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

#include "StdAfx.h"
#include "ParticleContainer.h"
#include "ParticleEmitter.h"
#include "Particle.h"

#include <CryThreading/IJobManager_JobDelegator.h>

#define fMAX_STATIC_BB_RADIUS        4096.f   // Static bounding box above this size forces dynamic BB
#define fMAX_RELATIVE_TAIL_DEVIATION 0.1f

#define nMAX_ITERATIONS              8
#define fMAX_FRAME_LIMIT_TIME        0.25f    // Max frame time at which to enforce iteration limit
#define fMAX_LINEAR_STEP_TIME        4.f      // Max time to test ahead

template<class T>
inline void move_raw_array_elem(T* pDest, T* pSrc)
{
	T val = *pSrc;
	if (pSrc < pDest)
		memmove(pSrc, pSrc + 1, (char*)pDest - (char*)(pSrc + 1));
	else
		memmove(pDest + 1, pDest, (char*)pSrc - (char*)pDest);
	*pDest = val;
}

//////////////////////////////////////////////////////////////////////////
CParticleContainer::CParticleContainer(CParticleContainer* pParent, CParticleEmitter* pMain, CParticleEffect const* pEffect)
	: m_pEffect(&non_const(*pEffect))
	, m_nEmitterSequence(0)
	, m_pMainEmitter(pMain)
	, m_fAgeLastUpdate(0.f)
	, m_fAgeStaticBoundsStable(0.f)
	, m_fContainerLife(0.f)
{
	ZeroStruct(m_pBeforeWaterRO);
	ZeroStruct(m_pAfterWaterRO);
	ZeroStruct(m_pRecursiveRO);

	assert(pEffect);
	assert(pEffect->IsActive() || gEnv->IsEditing());
	m_pParams = &m_pEffect->GetParams();
	assert(m_pParams->nEnvFlags & EFF_LOADED);

	if (m_pParams->eSpawnIndirection)
		m_pParentContainer = pParent;
	else if (pParent && pParent->IsIndirect())
		m_pParentContainer = pParent->GetParent();
	else
		m_pParentContainer = 0;

	// To do: Invalidate static bounds on updating areas.
	// To do: Use dynamic bounds if in non-uniform areas ??
	m_bbWorld.Reset();
	m_bbWorldStat.Reset();
	m_bbWorldDyn.Reset();

	OnEffectChange();

	m_nChildFlags = 0;
	m_bUnused = false;
	m_fMaxParticleFullLife = m_pEffect->GetMaxParticleFullLife();

	m_nNeedJobUpdate = 0;

	if (!m_pParentContainer)
	{
		// Not an indirect container, so it will need only a single SubEmitter - allocate it here
		AddEmitter(pMain);
	}

	// Do not use coverage buffer culling for 'draw near' or 'on top' particles
	if (pMain && (GetParams().bDrawNear || GetParams().bDrawOnTop))
		pMain->SetRndFlags(ERF_RENDER_ALWAYS, true);
}

CParticleContainer::~CParticleContainer()
{
	ResetRenderObjects();
}

uint32 CParticleContainer::GetEnvironmentFlags() const
{
	return m_nEnvFlags & CParticleManager::Instance()->GetAllowedEnvironmentFlags();
}

void CParticleContainer::OnEffectChange()
{
	m_nEnvFlags = m_pParams->nEnvFlags;

	// Update existing particle history arrays if needed.
	int nPrevSteps = m_nHistorySteps;
	m_nHistorySteps = m_pParams->GetTailSteps();

	// Do not use coverage buffer culling for 'draw near' or 'on top' particles
	if (m_pParams->bDrawNear || m_pParams->bDrawOnTop)
		GetMain().SetRndFlags(ERF_RENDER_ALWAYS, true);

#if CRY_PLATFORM_DESKTOP
	if (gEnv->IsEditor() && m_Particles.size() > 0)
	{
		if (m_nHistorySteps > nPrevSteps || NeedsCollisionInfo())
		{
			for (auto& p : m_Particles)
			{
				// Alloc and connect emitter structures.
				p.UpdateAllocations(nPrevSteps);
			}
		}
	}
#endif

	if (m_pParentContainer)
	{
		// Inherit dynamic bounds requirement.
		m_nEnvFlags |= m_pParentContainer->GetEnvironmentFlags() & EFF_DYNAMIC_BOUNDS;
		for (CParticleContainer* pParent = m_pParentContainer; pParent; pParent = pParent->GetParent())
			pParent->m_nChildFlags |= m_nEnvFlags;

		// Pre-compute extents of parent attached object.
		if (GetParams().eAttachType)
		{
			if (IStatObj* pStatObj = m_pParentContainer->GetParams().pStatObj)
				pStatObj->GetExtent(GetParams().eAttachForm);
		}
	}

	// Prevent invalid particle references to old geometry when editing Geometry param.
	if (gEnv->IsEditing() && GetParams().pStatObj)
		m_ExternalStatObjs.push_back(GetParams().pStatObj);
}

CParticleSubEmitter* CParticleContainer::AddEmitter(CParticleSource* pSource)
{
	if (void* pMem = m_Emitters.push_back_new())
	{
		return new(pMem) CParticleSubEmitter(pSource, this);
	}
	return NULL;
}

CParticle* CParticleContainer::AddParticle(SParticleUpdateContext& context, const CParticle& part)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const ResourceParticleParams& params = GetParams();

	if (params.Connection)
	{
		// Segregate by emitter, and always in front of previous particles.
		// To do: faster search for multi-emitter containers.
		int nSequence = part.GetEmitter()->GetSequence();
		for (auto& p : m_Particles)
		{
			if (p.GetEmitterSequence() == nSequence)
				return m_Particles.insert(&p, part);
		}
	}
	else if (context.nEnvFlags & REN_SORT)
	{
		float fNewDist = part.GetMinDist(context.vMainCamPos);

		if (!context.aParticleSort.empty())
		{
			// More accurate sort, using sort data in SParticleUpdateContext.
			size_t nNewPos = min((size_t)context.aParticleSort.size() - 1, m_Particles.size());
			SParticleUpdateContext::SSortElem* pNew = &context.aParticleSort[nNewPos];
			pNew->pPart = NULL;
			pNew->fDist = -fNewDist;

			SParticleUpdateContext::SSortElem* pFound;
			if (context.nSortQuality == 1)
			{
				// Find nearest (progressive distance) particle. Order: log(2) n
				pFound = std::lower_bound(context.aParticleSort.begin(), pNew, *pNew);
			}
			else
			{
				// Find min distance error. Order: n
				float fMinError = 0.f;
				float fCumError = 0.f;
				pFound = context.aParticleSort.begin();
				for (size_t i = 0; i < nNewPos; i++)
				{
					fCumError += context.aParticleSort[i].fDist + fNewDist;
					if (fCumError < fMinError)
					{
						fMinError = fCumError;
						pFound = &context.aParticleSort[i + 1];
					}
				}
			}

#ifdef SORT_DEBUG
			// Compute sort error. Slow!
			{
				static float fAvgError = 0.f;
				static float fCount = 0.f;
				float fError = 0.f;
				float fDir = 1.f;
				for (ParticleList<CParticle>::const_iterator pPart(m_Particles); pPart; ++pPart)
				{
					if (pPart == pFound->pPart)
						fDir = -1.f;
					float fDist = pPart->GetMinDist(context.vMainCamPos);
					fError += max(fDir * (fNewDist - fDist), 0.f);
				}

				fAvgError = (fAvgError * fCount + fError) / (fCount + 1.f);
				fCount += 1.f;
			}
#endif

			// Move next particle in particle list, and corresponding data in local arrays.
			CParticle* pPart = m_Particles.insert(pFound->pPart, part);
			pNew->pPart = pPart;
			move_raw_array_elem(pFound, pNew);
			return pPart;
		}
		else
		{
			// Approximate sort; push to front or back of list, based on position relative to bb center.
			if (fNewDist < context.fCenterCamDist)
				return m_Particles.push_back(part);
		}
	}

	// Push newest to front.
	return m_Particles.push_front(part);
}

void CParticleContainer::EmitParticle(const EmitParticleData* pData)
{
	// Queue particle emission.
	EmitParticleData* pNew = m_DeferredEmitParticles.push_back();
	if (pNew && pData)
	{
		*pNew = *pData;

		if (pData->pPhysEnt || pData->bHasLocation || pData->bHasVel)
		{
			// Static bounds no longer reliable.
			SetDynamicBounds();
		}
		if (pData->pStatObj)
		{
			// Add reference to StatObj for lifetime of container.
			assert(GetEnvironmentFlags() & REN_GEOMETRY);
			m_ExternalStatObjs.push_back(pData->pStatObj);
		}
	}
}

void CParticleContainer::ComputeStaticBounds(AABB& bb, bool bWithSize, float fMaxLife)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const ResourceParticleParams& params = GetParams();

	QuatTS loc = GetMain().GetLocation();
	AABB bbSpawn(0.f);

	FStaticBounds opts;

	if (m_pParentContainer)
	{
		// Expand by parent spawn volume.
		bool bParentSize = params.eAttachType != GeomType_None;
		float fMaxParentLife = params.GetMaxSpawnDelay();
		if (params.bContinuous || params.bMoveRelativeEmitter)
		{
			if (params.fEmitterLifeTime)
				fMaxParentLife += params.fEmitterLifeTime.GetMaxValue();
			else
				fMaxParentLife = fHUGE;
		}
		m_pParentContainer->ComputeStaticBounds(bbSpawn, bParentSize, fMaxParentLife);
		loc.t = bbSpawn.GetCenter();
		opts.fAngMax = m_pParentContainer->GetParams().GetMaxRotationAngle();
	}
	else if (GetMain().GetEmitGeom())
	{
		// Expand by attached geom.
		GetMain().GetEmitGeom().GetAABB(bbSpawn, GetMain().GetSpawnParams().eAttachType, loc);
		loc.t = bbSpawn.GetCenter();
	}

	loc.s = GetMaxParticleScale();
	opts.vSpawnSize = bbSpawn.GetSize() * 0.5f;
	opts.fSpeedScale = GetMain().GetSpawnParams().fSpeedScale;
	opts.bWithSize = bWithSize;
	opts.fMaxLife = fMaxLife;

	SPhysForces forces;
	GetMain().GetPhysEnviron().GetForces(forces, loc.t, m_nEnvFlags);
	params.GetStaticBounds(bb, loc, forces, opts);

	// Adjust for internal/external target.
	ParticleTarget target;
	if (GetTarget(target, GetDirectEmitter()))
	{
		AABB bbTarg(target.vTarget, target.fRadius);
		bbTarg.Expand(bb.GetSize() * 0.5f);
		bb.Add(bbTarg);
	}
}

void CParticleContainer::UpdateState()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	UpdateContainerLife();

	if (!NeedsDynamicBounds())
	{
		if (m_bbWorldStat.IsReset())
		{
			ComputeStaticBounds(m_bbWorldStat);
			if (m_bbWorldStat.GetRadiusSqr() >= sqr(fMAX_STATIC_BB_RADIUS))
				SetDynamicBounds();
		}
	}

	if (NeedsDynamicBounds() || !m_DeferredEmitParticles.empty())
	{
		UpdateParticles();
		m_DeferredEmitParticles.clear();
	}

	if (NeedsDynamicBounds())
	{
		SyncUpdateParticles();
		m_bbWorld = m_bbWorldDyn;
	}
	else
	{
		m_bbWorld = m_bbWorldStat;
		if (!StaticBoundsStable())
			m_bbWorld.Add(m_bbWorldDyn);
	}
}

// To do: Add flags for movement/gravity/wind.
float CParticleContainer::InvalidateStaticBounds()
{
	if (!NeedsDynamicBounds())
	{
		m_bbWorldStat.Reset();
		if (m_pParams->NeedsExtendedBounds() && !m_Particles.empty())
			return m_fAgeStaticBoundsStable = GetAge() + m_pParams->GetMaxParticleLife();
	}
	return 0.f;
}

bool CParticleContainer::GetTarget(ParticleTarget& target, const CParticleSubEmitter* pSubEmitter) const
{
	ParticleParams::STargetAttraction::ETargeting eTargeting = GetParams().TargetAttraction.eTarget;
	if (eTargeting == eTargeting.Ignore)
		return false;

	target = GetMain().GetTarget();
	if (!target.bPriority && eTargeting == eTargeting.OwnEmitter && pSubEmitter)
	{
		// Local target override.
		target.bTarget = true;
		target.vTarget = pSubEmitter->GetSource().GetLocation().t;
		target.vVelocity = pSubEmitter->GetSource().GetVelocity().vLin;
		target.fRadius = 0.f;
	}
	return target.bTarget;
}

void CParticleContainer::ComputeUpdateContext(SParticleUpdateContext& context, float fUpdateTime)
{
	const ResourceParticleParams& params = GetParams();

	context.fUpdateTime = fUpdateTime;
	context.fMaxLinearStepTime = fMAX_LINEAR_STEP_TIME;

	context.nEnvFlags = GetEnvironmentFlags();

	// Sorting support.
	context.nSortQuality = max((int)params.nSortQuality, GetCVars()->e_ParticlesSortQuality);
	context.vMainCamPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
	const AABB& bb = NeedsDynamicBounds() ? m_bbWorldDyn : m_bbWorldStat;
	context.fCenterCamDist = (context.vMainCamPos - bb.GetCenter()).GetLength() - params.fSize.GetMaxValue();

	// Compute rotations in 3D for geometry particles, or those set to 3D orientation.
	context.b3DRotation = params.eFacing == params.eFacing.Free || params.eFacing == params.eFacing.Decal
	                      || params.pStatObj;

	// Emission bounds parameters.
	context.vEmitBox = params.vRandomOffset;
	context.vEmitScale.zero();

	if (params.fOffsetRoundness)
	{
		float fRound = params.fOffsetRoundness * max(max(context.vEmitBox.x, context.vEmitBox.y), context.vEmitBox.z);
		Vec3 vRound(min(context.vEmitBox.x, fRound), min(context.vEmitBox.y, fRound), min(context.vEmitBox.z, fRound));
		context.vEmitBox -= vRound;
		context.vEmitScale(vRound.x ? 1 / vRound.x : 0, vRound.y ? 1 / vRound.y : 0, vRound.z ? 1 / vRound.z : 0);
	}

	if (params.bSpaceLoop)
	{
		// Use emission bounding volume.
		AABB bbSpaceLocal = params.GetEmitOffsetBounds();
		if (GetMain().GetEnvFlags() & REN_BIND_CAMERA)
		{
			// Use CameraMaxDistance as alternate space loop size.
			bbSpaceLocal.Add(Vec3(0.f), params.fCameraMaxDistance);

			// Limit bounds to camera frustum.
			bbSpaceLocal.min.y = max(bbSpaceLocal.min.y, +params.fCameraMinDistance);

			// Scale cam dist limits to handle zoom.
			CCamera const& cam = gEnv->p3DEngine->GetRenderingCamera();
			float fHSinCos[2];
			sincos_tpl(cam.GetHorizontalFov() * 0.5f, &fHSinCos[0], &fHSinCos[1]);
			float fVSinCos[2];
			sincos_tpl(cam.GetFov() * 0.5f, &fVSinCos[0], &fVSinCos[1]);
			float fZoom = 1.f / cam.GetFov();

			float fMin = fHSinCos[1] * fVSinCos[1] * (params.fCameraMinDistance * fZoom);

			bbSpaceLocal.max.CheckMin(Vec3(fHSinCos[0], 1.f, fVSinCos[0]) * (bbSpaceLocal.max.y * fZoom));
			bbSpaceLocal.min.CheckMax(Vec3(-bbSpaceLocal.max.x, fMin, -bbSpaceLocal.max.z));
		}

		// Compute params for quick space-loop bounds checking.
		QuatTS const& loc = GetMain().GetLocation();
		context.SpaceLoop.vCenter = loc * bbSpaceLocal.GetCenter();
		context.SpaceLoop.vSize = bbSpaceLocal.GetSize() * (0.5f * loc.s);
		Matrix33 mat(loc.q);
		for (int a = 0; a < 3; a++)
		{
			context.SpaceLoop.vScaledAxes[a] = mat.GetColumn(a);
			context.SpaceLoop.vSize[a] = max(context.SpaceLoop.vSize[a], 1e-6f);
			context.SpaceLoop.vScaledAxes[a] /= context.SpaceLoop.vSize[a];
		}
	}

	ParticleParams::STargetAttraction::ETargeting eTarget = params.TargetAttraction.eTarget;
	context.bHasTarget = eTarget == eTarget.OwnEmitter
	                     || (eTarget == eTarget.External && GetMain().GetTarget().bTarget);

	context.fMinStepTime = min(fUpdateTime, fMAX_FRAME_LIMIT_TIME) / float(nMAX_ITERATIONS);

	// Compute time and distance limits in certain situations.
	context.fMaxLinearDeviation = fHUGE;
	if (GetHistorySteps() > 0)
	{
		context.fMaxLinearDeviation = fMAX_RELATIVE_TAIL_DEVIATION * params.fSize.GetMaxValue();
		context.fMinStepTime = min(context.fMinStepTime, params.fTailLength.GetMaxValue() * 0.5f / GetHistorySteps());
	}
	if (context.nEnvFlags & ENV_COLLIDE_ANY)
	{
		context.fMaxLinearDeviation = min(context.fMaxLinearDeviation, fMAX_COLLIDE_DEVIATION);
	}

	// If random turbulence enabled, limit linear time based on deviation limit.
	// dp(t) = Turb/2 t^(3/2)
	// t = (2 Dev/Turb)^(2/3)
	if (params.fTurbulence3DSpeed)
	{
		context.fMaxLinearStepTime = min(context.fMaxLinearStepTime, powf(2.f * context.fMaxLinearDeviation / params.fTurbulence3DSpeed, 0.666f));
	}

	// If vortex turbulence enabled, find max rotation angle for allowed deviation.
	float fVortexRadius = params.fTurbulenceSize.GetMaxValue() * GetMaxParticleScale();
	if (fVortexRadius > context.fMaxLinearDeviation)
	{
		// dev(r, a) = r (1 - sqrt( (cos(a)+1) / 2 ))
		// cos(a) = 2 (1 - dev/r)^2 - 1
		float fCos = sqr(1.f - context.fMaxLinearDeviation / fVortexRadius) * 2.f - 1.f;
		float fAngle = RAD2DEG(acos(clamp_tpl(fCos, -1.f, 1.f)));
		context.fMaxLinearStepTime = div_min(fAngle, fabs(params.fTurbulenceSpeed.GetMaxValue()), context.fMaxLinearStepTime);
	}

	if (NeedsDynamicBounds() || !StaticBoundsStable()
	    || (GetCVars()->e_ParticlesDebug & AlphaBits('bx')) || GetMain().IsEditSelected())
	{
		// Accumulate dynamic bounds.
		context.pbbDynamicBounds = &m_bbWorldDyn;
		if (NeedsDynamicBounds() || !StaticBoundsStable())
			m_bbWorldDyn.Reset();
	}
	else
		context.pbbDynamicBounds = NULL;

	context.fDensityAdjust = 1.f;
}

//////////////////////////////////////////////////////////////////////////
void CParticleContainer::UpdateParticles()
{
	if (m_nNeedJobUpdate > 0)
		m_nNeedJobUpdate--;

	float fAge = GetAge();
	float fUpdateTime = fAge - m_fAgeLastUpdate;
	if (fUpdateTime > 0.f || !m_DeferredEmitParticles.empty())
	{
		FUNCTION_PROFILER_CONTAINER(this);

		if (m_pParentContainer)
		{
			m_pParentContainer->UpdateParticles();
		}

		SParticleUpdateContext context;
		ComputeUpdateContext(context, fUpdateTime);

		GetMain().GetPhysEnviron().LockAreas(context.nEnvFlags, 1);

		// Update all particle positions etc, delete dead particles.
		UpdateParticleStates(context);

		GetMain().GetPhysEnviron().LockAreas(context.nEnvFlags, -1);

		m_fAgeLastUpdate = fAge;
	}
}

void CParticleContainer::SyncUpdateParticles()
{
	GetMain().SyncUpdateParticlesJob();
	UpdateParticles();
}

void CParticleContainer::UpdateParticleStates(SParticleUpdateContext& context)
{
	const ResourceParticleParams& params = GetParams();
	float fLifetimeCheck = params.bRemainWhileVisible ? -fHUGE : 0.f;

	// Emit new particles.
	if ((context.nEnvFlags & REN_SORT) && context.nSortQuality > 0)
	{
		// Allocate particle sort helper array. Overestimate size from current + expected emitted count.
		int nMaxParticles = GetMaxParticleCount(context);
		STACK_ARRAY(SParticleUpdateContext::SSortElem, aParticleSort, nMaxParticles);
		context.aParticleSort.set(aParticleSort, nMaxParticles);
	}

	SParticleUpdateContext::SSortElem* pElem = context.aParticleSort.begin();
	for (auto& part : m_Particles)
	{
		if (!part.IsAlive(fLifetimeCheck))
		{
			if (part.NumRefs() == 0)
			{
				m_Particles.erase(&part);
				continue;
			}
			else
			{
				part.Hide();
				m_Counts.particles.reject++;
			}
		}
		else
		{
			part.Update(context, context.fUpdateTime);
		}

		if (!context.aParticleSort.empty())
		{
			assert(pElem < context.aParticleSort.end());
			pElem->pPart = &part;
			pElem->fDist = -part.GetMinDist(context.vMainCamPos);

			if (context.nSortQuality == 1 && pElem > context.aParticleSort.begin() && pElem->fDist < pElem[-1].fDist)
			{
				// Force progressive sort order.
				pElem->fDist = pElem[-1].fDist;
			}
			pElem++;
		}
	}

	// Emit new particles.
	UpdateEmitters(&context);
}

void CParticleContainer::UpdateEmitters(SParticleUpdateContext* pUpdateContext)
{
	// Remove finished emitters; those with effects components removed in UpdateEffects, main thread.
	bool bEraseEmitters = !(m_nEnvFlags & EFF_ANY) && IsIndirect();

	for (auto& e : m_Emitters)
	{
		e.UpdateState();
		if (pUpdateContext)
		{
			// Emit particles.

			// Emit deferred particles first.
			// Array will only be present for 1st container update, which should have just one emitter.
			for (auto& emit : m_DeferredEmitParticles)
			{
				if (!emit.bHasLocation)
					emit.Location = e.GetSource().GetLocation();
				e.EmitParticle(*pUpdateContext, emit);
			}

			// Emit regular particles.
			if (e.GetSource().IsAlive(-pUpdateContext->fUpdateTime))
				e.EmitParticles(*pUpdateContext);
		}

		if (bEraseEmitters && !e.GetSource().IsAlive() && e.NumRefs() == 0)
			m_Emitters.erase(&e);
	}
}

void CParticleContainer::UpdateEffects()
{
	for (auto& e : m_Emitters)
	{
		if (e.GetSource().IsAlive())
		{
			e.UpdateState();
			if (m_nEnvFlags & EFF_FORCE)
				e.UpdateForce();
			if (m_nEnvFlags & EFF_AUDIO)
				e.UpdateAudio();
		}
		else
		{
			// Emitters with effects removed here.
			e.Deactivate();
			if (IsIndirect() && e.NumRefs() == 0)
				m_Emitters.erase(&e);
		}
	}
}

void CParticleContainer::Render(SRendParams const& RenParams, SPartRenderParams const& PRParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_CONTAINER(this);

	const ResourceParticleParams* pParams = m_pParams;

	//It doesn't look like the nEnvFlags prefetch will be much use due its use almost immediately afterwards, but we're often cache
	//	missing on pParams->bEnabled, so that will give time for the cache line to be fetched
	PrefetchLine(&pParams->nEnvFlags, 0);
	PrefetchLine(&m_bbWorld, 0);
	PrefetchLine(pParams, 128);

	// Individual container distance culling.
	const float fDistSq = m_bbWorld.GetDistanceSqr(passInfo.GetCamera().GetPosition());
	float fMaxDist = GetEffect()->GetMaxParticleSize() * pParams->fViewDistanceAdjust * PRParams.m_fMaxAngularDensity;

	if (fDistSq > sqr(fMaxDist) && !pParams->fMinPixels)
		return;

	IF (pParams->fCameraMaxDistance, 0)
	{
		float fFOV = passInfo.GetCamera().GetFov();
		if (fDistSq * sqr(fFOV) > sqr(pParams->fCameraMaxDistance))
			return;
	}

	// Water and indoor culling tests.
	if (!(pParams->tVisibleIndoors * GetMain().GetVisEnviron().OriginIndoors()))
		return;
	if (!(pParams->tVisibleUnderwater * GetMain().GetPhysEnviron().m_tUnderWater))
		return;

	uint32 nRenderFlags = GetEnvironmentFlags() & PRParams.m_nRenFlags;

	if (!passInfo.IsAuxWindow())
	{
		if (!m_nNeedJobUpdate)
			// Was not scheduled for threaded update, so sync existing emitter update before creating new job.
			GetMain().SyncUpdateParticlesJob();

		// Flag this and all parents for update this frame; Set count to 2, to automatically prime updates at start of next frame too.
		if (GetCVars()->e_ParticlesThread)
		{
			for (CParticleContainer* pCont = this; pCont; pCont = pCont->GetParent())
				pCont->SetNeedJobUpdate(2);
		}
		else
		{
			UpdateParticles();
		}
	}
	else
	{
		SyncUpdateParticles();
	}

	if (nRenderFlags & REN_LIGHTS)
	{
		SyncUpdateParticles();
		RenderLights(RenParams, passInfo);
	}

	if (nRenderFlags & REN_GEOMETRY)
	{
		SyncUpdateParticles();
		RenderGeometry(RenParams, passInfo);
	}
	else if (nRenderFlags & REN_DECAL)
	{
		SyncUpdateParticles();
		RenderDecals(passInfo);
	}
	else if (nRenderFlags & REN_SPRITE)
	{
		// Copy pre-computed render and state flags.
		uint64 nObjFlags = pParams->nRenObjFlags & PRParams.m_nRenObjFlags;

		IF (pParams->eFacing == pParams->eFacing.Water, 0)
		{
			// Water-aligned particles are always rendered before water, to avoid intersection artifacts
			if (passInfo.IsRecursivePass())
				return;
			IF (Get3DEngine()->GetOceanRenderFlags() & OCR_NO_DRAW, 0)
				return;
		}
		else
		{
			// Other particles determine whether to render before or after water, or both
			bool bCameraUnderWater = passInfo.IsCameraUnderWater() ^ passInfo.IsRecursivePass();
			if (!passInfo.IsRecursivePass() && pParams->tVisibleUnderwater == ETrinary() && GetMain().GetPhysEnviron().m_tUnderWater == ETrinary())
			{
				// Must render in 2 passes.
				if (!(nObjFlags & FOB_AFTER_WATER))
				{
					SPartRenderParams PRParamsAW = PRParams;
					PRParamsAW.m_nRenObjFlags |= FOB_AFTER_WATER;
					Render(RenParams, PRParamsAW, passInfo);
				}
			}
			else if (pParams->tVisibleUnderwater * bCameraUnderWater
			         && GetMain().GetPhysEnviron().m_tUnderWater * bCameraUnderWater)
			{
				nObjFlags |= FOB_AFTER_WATER;
			}
		}

		const uint threadId = passInfo.ThreadID();
		CRenderObject** pCachedRO = nullptr;
		if (passInfo.IsRecursivePass())
			pCachedRO = m_pRecursiveRO;
		else if (nObjFlags & FOB_AFTER_WATER)
			pCachedRO = m_pAfterWaterRO;
		else
			pCachedRO = m_pBeforeWaterRO;
		CRenderObject* pRenderObject = pCachedRO[threadId];
		if (!pRenderObject)
		{
			pRenderObject = CreateRenderObject(nObjFlags, passInfo);
			pCachedRO[threadId] = pRenderObject;
		}

		SAddParticlesToSceneJob& job = CParticleManager::Instance()->GetParticlesToSceneJob(passInfo);
		job.pVertexCreator = this;
		job.pRenderObject = pRenderObject;
		SRenderObjData* pOD = job.pRenderObject->GetObjData();

		if (nObjFlags & FOB_OCTAGONAL)
		{
			// Size threshold for octagonal rendering.
			static const float fOCTAGONAL_PIX_THRESHOLD = 20;
			if (sqr(pParams->fSize.GetMaxValue() * pParams->fFillRateCost * passInfo.GetCamera().GetViewSurfaceZ())
			    < sqr(fOCTAGONAL_PIX_THRESHOLD * passInfo.GetCamera().GetFov()) * fDistSq)
				nObjFlags &= ~FOB_OCTAGONAL;
		}
		
		job.pRenderObject->m_ObjFlags = (nObjFlags & ~0xFF) | RenParams.dwFObjFlags;
		job.pRenderObject->SetInstanceDataDirty();

		pOD->m_FogVolumeContribIdx = PRParams.m_nFogVolumeContribIdx;

		pOD->m_LightVolumeId = PRParams.m_nDeferredLightVolumeId;

		if (const auto pTempData = GetMain().m_pTempData.load())
			*((Vec4f*)&pOD->m_fTempVars[0]) = Vec4f(pTempData->userData.vEnvironmentProbeMults);
		else
			*((Vec4f*)&pOD->m_fTempVars[0]) = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

		// Set sort distance based on params and bounding box.
		if (pParams->fSortBoundsScale == PRParams.m_fMainBoundsScale)
			job.pRenderObject->m_fDistance = PRParams.m_fCamDistance;
		else
			job.pRenderObject->m_fDistance = GetMain().GetNearestDistance(passInfo.GetCamera().GetPosition(), pParams->fSortBoundsScale);
		job.pRenderObject->m_fDistance += pParams->fSortOffset;

		//
		// Set remaining SAddParticlesToSceneJob data.
		//

		job.pShaderItem = &pParams->pMaterial->GetShaderItem();
		if (job.pShaderItem->m_pShader && (job.pShaderItem->m_pShader->GetFlags() & EF_REFRACTIVE))
			SetScreenBounds(passInfo.GetCamera(), pOD->m_screenBounds);
		if (pParams->fTexAspect == 0.f)
			non_const(*m_pParams).UpdateTextureAspect();

		job.nCustomTexId = RenParams.nTextureID;

		int passId = passInfo.IsShadowPass() ? 1 : 0;
		int passMask = BIT(passId);
		pRenderObject->m_passReadyMask |= passMask;

		passInfo.GetIRenderView()->AddPermanentObject(
			pRenderObject,
			passInfo);
	}
}

CRenderObject* CParticleContainer::CreateRenderObject(uint64 nObjFlags, const SRenderingPassInfo& passInfo)
{
	const ResourceParticleParams* pParams = m_pParams;
	CRenderObject* pRenderObject = gEnv->pRenderer->EF_GetObject();
	SRenderObjData* pOD = pRenderObject->GetObjData();

	pRenderObject->m_pRE = gEnv->pRenderer->EF_CreateRE(eDATA_Particle);
	pRenderObject->SetMatrix(Matrix34::CreateIdentity(), passInfo);
	pRenderObject->m_RState = uint8(nObjFlags);
	pRenderObject->m_pCurrMaterial = pParams->pMaterial;
	pOD->m_pParticleShaderData = &GetEffect()->GetParams().ShaderData;

	IF(!!pParams->fHeatScale, 0)
	{
		pOD->m_nVisionScale = MAX_HEATSCALE;
		uint32 nHeatAmount = pParams->fHeatScale.GetStore();
		pOD->m_nVisionParams = (nHeatAmount << 24) | (nHeatAmount << 16) | (nHeatAmount << 8) | (0);
	}

	pRenderObject->m_ParticleObjFlags = (pParams->bHalfRes ? CREParticle::ePOF_HALF_RES : 0)
		| (pParams->bVolumeFog ? CREParticle::ePOF_VOLUME_FOG : 0);

	return pRenderObject;
}

void CParticleContainer::ResetRenderObjects()
{
	for (uint threadId = 0; threadId < RT_COMMAND_BUF_COUNT; ++threadId)
	{
		if (m_pAfterWaterRO[threadId])
		{
			if (m_pAfterWaterRO[threadId]->m_pRE)
				m_pAfterWaterRO[threadId]->m_pRE->Release();
			gEnv->pRenderer->EF_FreeObject(m_pAfterWaterRO[threadId]);
		}
		if (m_pBeforeWaterRO[threadId])
		{
			if (m_pBeforeWaterRO[threadId]->m_pRE)
				m_pBeforeWaterRO[threadId]->m_pRE->Release();
			gEnv->pRenderer->EF_FreeObject(m_pBeforeWaterRO[threadId]);
		}
		if (m_pRecursiveRO[threadId])
		{
			if (m_pRecursiveRO[threadId]->m_pRE)
				m_pRecursiveRO[threadId]->m_pRE->Release();
			gEnv->pRenderer->EF_FreeObject(m_pRecursiveRO[threadId]);
		}
		m_pAfterWaterRO[threadId] = nullptr;
		m_pBeforeWaterRO[threadId] = nullptr;
		m_pRecursiveRO[threadId] = nullptr;
	}
}

void CParticleContainer::SetScreenBounds(const CCamera& cam, uint8 aScreenBounds[4])
{
	const int32 align16 = (16 - 1);
	const int32 shift16 = 4;
	int iOut[4];
	int nWidth = cam.GetViewSurfaceX();
	int nHeight = cam.GetViewSurfaceZ();
	AABB posAABB = AABB(cam.GetPosition(), 1.00f);
	if (!posAABB.IsIntersectBox(m_bbWorld))
	{
		cam.CalcScreenBounds(&iOut[0], &m_bbWorld, nWidth, nHeight);
		if (((iOut[2] - iOut[0]) == nWidth) && ((iOut[3] - iOut[1]) == nHeight))
		{
			iOut[2] += 16;  // Split fullscreen particles and fullscreen geometry. Better to use some sort of ID/Flag, but this will do the job for now
		}
		aScreenBounds[0] = min(iOut[0] >> shift16, (int32)255);
		aScreenBounds[1] = min(iOut[1] >> shift16, (int32)255);
		aScreenBounds[2] = min((iOut[2] + align16) >> shift16, (int32)255);
		aScreenBounds[3] = min((iOut[3] + align16) >> shift16, (int32)255);
	}
	else
	{
		aScreenBounds[0] = 0;
		aScreenBounds[1] = 0;
		aScreenBounds[2] = min((nWidth >> shift16) + 1, (int32)255);
		aScreenBounds[3] = min((nHeight >> shift16) + 1, (int32)255);
	}
}

bool CParticleContainer::NeedsUpdate() const
{
	return GetMain().GetAge() > m_fAgeLastUpdate;
}

void CParticleContainer::SetDynamicBounds()
{
	m_nEnvFlags |= EFF_DYNAMIC_BOUNDS;
	for (CParticleContainer* pParent = m_pParentContainer; pParent; pParent = pParent->GetParent())
		pParent->m_nChildFlags |= EFF_DYNAMIC_BOUNDS;
	GetMain().AddEnvFlags(EFF_DYNAMIC_BOUNDS);
}

float CParticleContainer::GetMaxParticleScale() const
{
	return (m_pParentContainer && GetParams().bMoveRelativeEmitter.ScaleWithSize() ?
	        m_pParentContainer->GetEffect()->GetMaxParticleSize() : 1.f)
	       * GetMain().GetParticleScale();
}

float CParticleContainer::GetEmitterLife() const
{
	float fEmitterLife = GetMain().GetStopAge();
	if (!m_pParentContainer)
	{
		if (!m_pParams->fPulsePeriod)
		{
			if (!m_Emitters.empty())
				fEmitterLife = min(fEmitterLife, m_Emitters.front().GetStopAge());
			else
				fEmitterLife = 0.f;
		}
	}
	else
	{
		fEmitterLife = min(fEmitterLife, m_pParentContainer->GetContainerLife());
		if (!m_pParams->bContinuous)
			fEmitterLife = min(fEmitterLife, m_pParentContainer->GetEmitterLife() + m_pParams->GetMaxSpawnDelay());
	}
	return fEmitterLife;
}

int CParticleContainer::GetMaxParticleCount(const SParticleUpdateContext& context) const
{
	const ResourceParticleParams& params = GetParams();

	// Start count is existing number of particles plus deferred
	int nCount = m_Particles.size() + m_DeferredEmitParticles.size();

	// Quickly (over)estimate max emissions for this frame
	float fEmitCount = params.fCount.GetMaxValue();
	if (!GetParent())
		fEmitCount *= GetMain().GetEmitCountScale();
	if (params.eGeometryPieces == params.eGeometryPieces.AllPieces && params.pStatObj != 0)
	{
		// Emit 1 particle per piece.
		int nPieces = GetSubGeometryCount(params.pStatObj);
		fEmitCount *= max(nPieces, 1);
	}

	float fEmitTime = min(context.fUpdateTime, GetMaxParticleFullLife());

	if (params.bContinuous)
	{
		// Compute emission rate
		float fLifeTime = params.GetMaxParticleLife();
		if (params.fEmitterLifeTime)
			fLifeTime = min(fLifeTime, params.fEmitterLifeTime(VMIN));

		// Determine time window to update.
		if (fEmitTime < fLifeTime)
			fEmitCount *= fEmitTime / fLifeTime;
		if (params.fMaintainDensity)
			fEmitCount *= fMAX_DENSITY_ADJUST;
	}

	// Account for pulsing
	int nEmitCount = int_ceil(fEmitCount) * m_Emitters.size();
	if (params.fPulsePeriod)
		nEmitCount *= int_ceil(fEmitTime / max(params.fPulsePeriod(VMIN), 0.1f));
	nCount += nEmitCount;

	return nCount;
}

void CParticleContainer::UpdateContainerLife(float fAgeAdjust)
{
	m_fAgeLastUpdate += fAgeAdjust;
	m_fAgeStaticBoundsStable += fAgeAdjust;

	if (CParticleSubEmitter* pDirectEmitter = GetDirectEmitter())
		pDirectEmitter->UpdateState(fAgeAdjust);
	if (m_pParams->bRemainWhileVisible && GetMain().TimeNotRendered() < 0.5f)
		m_fContainerLife = GetAge() + 0.5f;
	else
	{
		m_fContainerLife = GetEmitterLife() + m_pParams->fParticleLifeTime.GetMaxValue();
		if (!m_pParams->fParticleLifeTime && m_pParentContainer)
			// Zero particle life implies particles live as long as parent emitter lives.
			m_fContainerLife += m_pParentContainer->m_fContainerLife;
	}
}

float CParticleContainer::GetAge() const
{
	return GetMain().GetAge();
}

void CParticleContainer::Reset()
{
	// Free all particle and sub-emitter memory.
	m_Particles.clear();
	if (IsIndirect())
		m_Emitters.clear();
	m_fAgeLastUpdate = 0.f;
	m_bbWorldDyn.Reset();
}

// Stat functions.
void CParticleContainer::GetCounts(SParticleCounts& counts) const
{
	counts.components.alloc += 1.f;
	counts.components.alive += GetContainerLife() <= GetAge();
	counts.particles.alloc += m_Particles.size();
	counts.particles.alive += m_Particles.size();

	if (GetTimeToUpdate() == 0.f)
	{
		// Was updated this frame.
		reinterpret_cast<SContainerCounts&>(counts) += m_Counts;
		counts.components.updated += 1.f;
		counts.particles.updated += m_Particles.size();
		counts.subemitters.updated += m_Emitters.size();

		if ((m_nEnvFlags & REN_ANY) && !m_bbWorldDyn.IsReset())
		{
			counts.volume.dyn += m_bbWorldDyn.GetVolume();
			counts.volume.stat += m_bbWorld.GetVolume();
			if (!m_bbWorld.ContainsBox(m_bbWorldDyn))
			{
				AABB bbDynClip = m_bbWorldDyn;
				bbDynClip.ClipToBox(m_bbWorld);
				float fErrorVol = m_bbWorldDyn.GetVolume() - bbDynClip.GetVolume();
				counts.volume.error += fErrorVol;
			}
		}
	}
}

void CParticleContainer::GetMemoryUsage(ICrySizer* pSizer) const
{
	m_Particles.GetMemoryUsagePlain(pSizer);
	pSizer->AddObject(&m_Particles, CParticle::GetAllocationSize(this) * m_Particles.size());
	m_Emitters.GetMemoryUsage(pSizer);
	m_DeferredEmitParticles.GetMemoryUsagePlain(pSizer);
	m_ExternalStatObjs.GetMemoryUsagePlain(pSizer);
}

void CParticleContainer::OffsetPosition(const Vec3& delta)
{
	m_bbWorld.Move(delta);
	m_bbWorldStat.Move(delta);
	m_bbWorldDyn.Move(delta);

	for (auto& e : m_Emitters)
		e.OffsetPosition(delta);

	for (auto& p : m_Particles)
		p.OffsetPosition(delta);
}

void CParticleContainer::OnHidden()
{
	for (auto& e : m_Emitters)
	{
		e.Deactivate();
	}
}

void CParticleContainer::OnUnhidden()
{
	if (m_nEnvFlags & EFF_AUDIO)
	{
		for (auto& e : m_Emitters)
		{
			e.UpdateAudio();
		}
	}
}
