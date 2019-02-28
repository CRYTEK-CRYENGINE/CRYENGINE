// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleEnviron.h"
#include "VisAreas.h"


//////////////////////////////////////////////////////////////////////////
float GetMaxZ(Plane const& plane, AABB const& bb)
{
	if (plane.n.z == 1.f)
		return -plane.d;
	if (plane.n.z == 0.f)
		return bb.max.z;

	float z = plane.d
		+ max(plane.n.x * bb.min.x, plane.n.x * bb.max.x)
		+ max(plane.n.y * bb.min.y, plane.n.y * bb.max.y);
	return -z / plane.n.z;
}

//////////////////////////////////////////////////////////////////////////

void SPhysForces::Add(SPhysForces const& other, uint32 nEnvFlags)
{
	if (nEnvFlags & ENV_GRAVITY)
		vAccel = other.vAccel;
	if (nEnvFlags & ENV_WIND)
		vWind += other.vWind;
	if (nEnvFlags & ENV_WATER)
		if (other.plWater.d < plWater.d)
			plWater = other.plWater;
}

//////////////////////////////////////////////////////////////////////////
static uint32 GetAreaFlags(IPhysicalEntity* pEntity, pe_params_area& parea, pe_params_buoyancy& pbuoy, Plane* pWaterPlane = 0)
{
	uint32 flags = 0;

	if (pEntity->GetParams(&parea))
	{
		if (!is_unused(parea.gravity))
			flags |= ENV_GRAVITY;
	}

	if (pEntity->GetParams(&pbuoy))
	{
		if (pbuoy.iMedium == 1)
			flags |= ENV_WIND;
		else if (pbuoy.iMedium == 0)
		{
			flags |= ENV_WATER;
			if (pWaterPlane && !is_unused(pbuoy.waterPlane.n))
				pWaterPlane->SetPlane(pbuoy.waterPlane.n, pbuoy.waterPlane.origin);
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////
// SPhysEnviron implementation

void SPhysEnviron::Clear()
{
	m_UniformForces = SPhysForces(ZERO);
	m_tUnderWater = false;
	m_nNonUniformFlags = 0;
	m_nNonCachedFlags = 0;
	m_NonUniformAreas.resize(0);
}

static SWorldPhysEnviron* s_pWorldPhysEnviron;

static int StaticOnAreaChange(const EventPhys* pEvent)
{
	if (s_pWorldPhysEnviron)
	{
		if (s_pWorldPhysEnviron->OnAreaChange(static_cast<const EventPhysAreaChange&>(*pEvent)))
			s_pWorldPhysEnviron->OnPhysAreaChange();
	}
	return 0;
}

SWorldPhysEnviron::SWorldPhysEnviron()
{
	s_pWorldPhysEnviron = this;
	if (gEnv->pPhysicalWorld)
		gEnv->pPhysicalWorld->AddEventClient(EventPhysAreaChange::id, &StaticOnAreaChange, 0);
}
SWorldPhysEnviron::~SWorldPhysEnviron()
{
	if (gEnv->pPhysicalWorld)
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysAreaChange::id, &StaticOnAreaChange, 0);
	s_pWorldPhysEnviron = 0;
}

bool SWorldPhysEnviron::OnAreaChange(const EventPhysAreaChange& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	// Require re-querying of world physics areas
	if (!event.pEntity || event.pEntity->GetType() != PE_AREA)
		return false;

	SAreaSpec acNew;
	pe_params_area parea;
	pe_params_buoyancy pbuoy;
	acNew.m_nFlags = GetAreaFlags(event.pEntity, parea, pbuoy);
	if (!acNew.m_nFlags)
		return false;

	acNew.m_bounds = AABB(event.boxAffected[0], event.boxAffected[1]);

	AUTO_LOCK(m_Lock);
	m_SumAreaChanged |= acNew;

	// Merge with existing bb if close enough and same medium
	static const float fMERGE_THRESHOLD = 2.f;

	float fNewVolume = acNew.m_bounds.GetVolume();
	for (auto& ac : m_AreasChanged)
	{
		if (ac.m_nFlags == acNew.m_nFlags)
		{
			AABB bbUnion = acNew.m_bounds;
			bbUnion.Add(ac.m_bounds);
			if (bbUnion.GetVolume() <= (fNewVolume + ac.m_bounds.GetVolume()) * fMERGE_THRESHOLD)
			{
				ac.m_bounds = bbUnion;
				return true;
			}
		}
	}
	m_AreasChanged.push_back(acNew);
	return true;
}

void SWorldPhysEnviron::Update()
{
	if (IsCurrent() && !IsChanged())
		return;

	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	// Recycle shared SArea objects
	SmartPtrArray<SArea> aPrevAreas;
	std::swap(aPrevAreas, m_NonUniformAreas);

	Clear();

	// Mark areas as queried.
	m_nNonUniformFlags |= EFF_LOADED;

	Vec3 vWorldSize(GetTerrain() ? float(GetTerrain()->GetTerrainSize()) : 0.f);

	// Atomic iteration.
	for (IPhysicalEntity* pArea = 0; pArea = gEnv->pPhysicalWorld->GetNextArea(pArea); )
	{
		// Initial check for areas of interest.
		pe_params_area parea;
		pe_params_buoyancy pbuoy;
		Plane plWater = m_UniformForces.plWater;
		uint32 nAreaFlags = GetAreaFlags(pArea, parea, pbuoy, &plWater);

		// Skip if unneeded gravity or wind.
		// All emitters require m_tUnderwater intersection test.
		if (!nAreaFlags)
			continue;

		// Query area for intersection and forces, uniform only.
		pe_status_area sarea;
		sarea.ctr.zero();
		sarea.size = vWorldSize;
		sarea.bUniformOnly = true;
		int nStatus = pArea->GetStatus(&sarea);
		if (!nStatus)
			continue;

		if (nAreaFlags & ENV_WATER)
		{
			m_tUnderWater = ETrinary();
		}

		pe_status_pos spos;
		Matrix33 matArea;
		spos.pMtx3x3 = &matArea;
		if (!pArea->GetStatus(&spos))
			continue;

		// Get correct force directions.
		if (parea.bUniform == 2)
		{
			if (nAreaFlags & ENV_GRAVITY)
				parea.gravity = spos.q * parea.gravity;
			if (nAreaFlags & ENV_WIND)
				pbuoy.waterFlow = spos.q * pbuoy.waterFlow;
		}

		SPhysForces area_forces;
		area_forces.vAccel = nAreaFlags & ENV_GRAVITY ? parea.gravity : Vec3(0.f);
		area_forces.vWind = nAreaFlags & ENV_WIND ? pbuoy.waterFlow : Vec3(0.f);
		area_forces.plWater = plWater;

		if (nStatus < 0)
		{
			// Add to NonUniformAreas list, re-using previous SAreas
			_smart_ptr<SArea> pAreaRec;
			int i = 0;
			for (auto& area : aPrevAreas)
			{
				if (area.m_pArea == pArea)
				{
					pAreaRec = &area;
					aPrevAreas.erase(i);
					break;
				}
				i++;
			}

			if (!pAreaRec)
			{
				pAreaRec = new SArea;
				pAreaRec->m_pArea = pArea;
			}

			SArea& area = *pAreaRec;
			m_NonUniformAreas.push_back(pAreaRec);

			area.m_nFlags = nAreaFlags;
			area.m_pLock = sarea.pLockUpdate;
			area.m_bounds.min = spos.BBox[0] + spos.pos;
			area.m_bounds.max = spos.BBox[1] + spos.pos;
			area.m_Forces = area_forces;

			if (nAreaFlags & ENV_WATER)
			{
				// Set water area bounds from actual plane.
				const Plane& wp = area.m_Forces.plWater;
				area.m_bounds.max.z = ::GetMaxZ(wp, area.m_bounds);
				area.m_bounds.min.z = -fHUGE;
			}

			pe_params_foreign_data pfd;
			if (pArea->GetParams(&pfd))
			{
				if (pfd.iForeignFlags & PFF_OUTDOOR_AREA)
					area.m_bOutdoorOnly = true;
			}

			area.m_bCacheForce = false;

			if (nAreaFlags & ENV_FORCES)
			{
				// Test whether we can cache force information for quick evaluation.
				int geom_type;
				if (parea.pGeom && ((geom_type = parea.pGeom->GetType()) == GEOM_BOX || geom_type == GEOM_SPHERE))
				{
					area.m_bRadial = !parea.bUniform;
					area.m_nGeomShape = geom_type;
					area.m_fFalloffScale = div_min(1.f, 1.f - parea.falloff0, fHUGE);

					// Construct world-to-unit-sphere transform.
					area.m_vCenter = spos.pos;
					area.m_matToLocal = (matArea * Matrix33::CreateScale(area.m_bounds.GetSize() * 0.5f)).GetInverted();
					area.m_bCacheForce = true;
				}
				if (!area.m_bCacheForce)
					m_nNonCachedFlags |= nAreaFlags & ENV_FORCES;
			}
			m_nNonUniformFlags |= nAreaFlags;
		}
		else
		{
			// Uniform area.
			m_UniformForces.Add(area_forces, nAreaFlags);
		}
	}
}

void SWorldPhysEnviron::FinishUpdate()
{
	ClearAreasChanged();
}

void SPhysEnviron::Update(SPhysEnviron const& envSource, AABB const& box, bool bIndoors, uint32 nFlags, bool bNonUniformAreas, const void* pObjectSkip)
{
	assert(envSource.IsCurrent());

	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	Clear();

	// Mark areas as queried.
	m_nNonUniformFlags |= EFF_LOADED;

	m_UniformForces.vAccel = envSource.m_UniformForces.vAccel;
	if (!bIndoors)
	{
		m_UniformForces.vWind = envSource.m_UniformForces.vWind;
		m_UniformForces.plWater = envSource.m_UniformForces.plWater;

		// Set status of box with respect to water plane.
		float fDistMin, fDistMax;
		Distance::AABB_Plane(&fDistMin, &fDistMax, box, m_UniformForces.plWater);
		m_tUnderWater = fDistMin > 0.f ? ETrinary(false)
		                : fDistMax < 0.f ? ETrinary(true)
		                : ETrinary();
	}

	if (bNonUniformAreas && envSource.m_nNonUniformFlags & nFlags && !box.IsReset())
	{
		pe_status_area sarea;
		sarea.ctr = box.GetCenter();
		sarea.size = box.GetSize() * 0.5f;
		sarea.vel.zero();
		sarea.bUniformOnly = true;

		for (auto& area : envSource.m_NonUniformAreas)
		{
			if (area.m_nFlags & nFlags)
			{
				// Query area for intersection and forces, uniform only.
				if (!bIndoors || !area.m_bOutdoorOnly)
				{
					// Test bb intersection.
					if (area.m_bounds.IsIntersectBox(box))
					{
						if (pObjectSkip)
						{
							pe_params_foreign_data fd;
							if (area.m_pArea->GetParams(&fd))
							{
								if (fd.pForeignData == pObjectSkip)
									continue;
							}
						}

						float fWaterDistMin, fWaterDistMax;
						if (area.m_nFlags & ENV_WATER)
						{
							// Pre-check water plane intersection
							Distance::AABB_Plane(&fWaterDistMin, &fWaterDistMax, box, area.m_Forces.plWater);
							if (fWaterDistMin >= 0.f)
								continue;
						}

						int nStatus = area.m_pArea->GetStatus(&sarea);
						if (!nStatus)
							continue;

						// Update underwater status.
						if ((area.m_nFlags & ENV_WATER) && m_tUnderWater * false)
						{
							if (fWaterDistMax < 0.f)
							{
								// Underwater.
								m_tUnderWater = nStatus < 0 ? ETrinary() : ETrinary(true);
							}
							else
							{
								// Partially underwater.
								nStatus = -1;
								m_tUnderWater = ETrinary();
							}
						}

						if (nStatus < 0)
						{
							// Store in local nonuniform area list.
							m_NonUniformAreas.push_back(&area);
							m_nNonUniformFlags |= area.m_nFlags;
							if (!area.m_bCacheForce)
								m_nNonCachedFlags |= area.m_nFlags & ENV_FORCES;
						}
						else
						{
							// Uniform within this volume.
							m_UniformForces.Add(area.m_Forces, area.m_nFlags);
						}
					}
				}
			}
		}
	}
}

void SPhysEnviron::GetForces(SPhysForces& forces, AABB const& bb, uint32 nFlags, bool bAverage) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	forces = m_UniformForces;
	ForNonumiformAreas(bb, nFlags, [&](const SArea& area)
	{
		if (area.m_nFlags & nFlags)
			area.GetForces(forces, bb, nFlags, bAverage); 
	});
}

bool SPhysEnviron::PhysicsCollision(ray_hit& hit, Vec3 const& vStart, Vec3 const& vEnd, float fRadius, uint32 nEnvFlags, IPhysicalEntity* pTestEntity)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	bool bHit = false;
	Vec3 vMove = vEnd - vStart;
	float fMoveSqr = vMove.GetLengthSquared();
	if (fMoveSqr == 0.f)
		return false;
	float fInvMove = isqrt_tpl(fMoveSqr);

	// Extend movement by particle radius.
	float fRadiusRel = fRadius * fInvMove;
	vMove *= 1.f + fRadiusRel;

	float fMoveNorm = 0.f;
	ZeroStruct(hit);
	hit.dist = 1.f;

	// Collide terrain first (if set as separately colliding).
	if ((nEnvFlags & ~ENV_COLLIDE_PHYSICS & ENV_TERRAIN) && !pTestEntity && GetTerrain())
	{
		nEnvFlags &= ~ENV_TERRAIN;
		CHeightMap::SRayTrace rt;
		if (GetTerrain()->RayTrace(vStart, vStart + vMove, &rt))
		{
			if ((fMoveNorm = rt.vNorm * vMove) < 0.f)
			{
				bHit = true;
				hit.dist = rt.fInterp;
				hit.pt = rt.vHit;
				hit.n = rt.vNorm;
				if (rt.pMaterial)
					hit.surface_idx = rt.pMaterial->GetSurfaceTypeId();
				else
					hit.surface_idx = 0;
				hit.bTerrain = true;
			}
		}
	}

	if (pTestEntity)
	{
		// Test specified entity only.
		bHit = gEnv->pPhysicalWorld->RayTraceEntity(pTestEntity, vStart, vMove, &hit)
		       && hit.n * vMove < 0.f;
	}
	else if (hit.dist > 0.f)
	{
		int ent_collide = 0;
		if (nEnvFlags & ENV_TERRAIN)
			ent_collide |= ent_terrain;
		if (nEnvFlags & ENV_STATIC_ENT)
			ent_collide |= ent_static;
		if (nEnvFlags & ENV_DYNAMIC_ENT)
			ent_collide |= ent_rigid | ent_sleeping_rigid | ent_living | ent_independent;

		IF (ent_collide, false)
		{
			// rwi_ flags copied from similar code in CParticleEntity.
			ray_hit hit_loc;
			if (gEnv->pPhysicalWorld->RayWorldIntersection(
			      vStart, vMove * hit.dist,
			      ent_collide | ent_no_ondemand_activation,
			      sf_max_pierceable | (geom_colltype_ray | geom_colltype13) << rwi_colltype_bit | rwi_colltype_any | rwi_ignore_noncolliding,
			      &hit_loc, 1) > 0
			    )
			{
				if ((fMoveNorm = hit_loc.n * vMove) < 0.f)
				{
					bHit = true;
					hit = hit_loc;
					hit.dist *= fInvMove;
				}
			}
		}
	}

	if (bHit)
	{
		hit.pt += hit.n * fRadius;
		hit.dist -= div_min(fRadius, -fMoveNorm, 1.f);
		hit.dist = clamp_tpl(hit.dist, 0.f, 1.f);
	}
	return bHit;
}

void SPhysEnviron::SArea::GetForcesPhys(SPhysForces& forces, AABB const& bb) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	pe_status_area sarea;
	sarea.ctr = bb.GetCenter();
	sarea.size = bb.GetSize() * 0.5f;
	if (m_pArea->GetStatus(&sarea))
	{
		if (!is_unused(sarea.gravity))
			forces.vAccel = sarea.gravity;
		if (sarea.pb.iMedium == 1 && !is_unused(sarea.pb.waterFlow))
			forces.vWind += sarea.pb.waterFlow;
	}
}

void SPhysEnviron::SArea::GetForces(SPhysForces& forces, AABB const& bb, uint32 nFlags, bool bAverage) const
{
	nFlags &= m_nFlags;

	Vec3 vPos = bb.GetCenter();
	if (nFlags & ENV_FORCES)
	{
		if (!m_bCacheForce)
		{
			GetForcesPhys(forces, bb);
		}
		else
		{
			CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

#ifdef DEBUG_FORCES
			// Compare locally computed results to physics system.
			// Occasionally fails because of threaded physics area updates.
			SPhysForces forcesPhys = forces;
			GetForcesPhys(forcesPhys, bb);
#endif

			float fStrength = 1.0f;
			float fVolume = bb.GetVolume();

			if (fVolume)
			{
				AABB bbClip = m_bounds;
				bbClip.ClipToBox(bb);
				vPos = bbClip.GetCenter();
				if (bAverage)
					// Scale force by bounds overlap ratio
					fStrength = bbClip.GetVolume() / fVolume;
			}

			Vec3 vPosRel = vPos - m_vCenter;
			Vec3 vDist = m_matToLocal * vPosRel;
			float fDist = m_nGeomShape == GEOM_BOX ? max(max(abs(vDist.x), abs(vDist.y)), abs(vDist.z))
			              : vDist.GetLengthFast();
			if (fDist <= 1.f)
			{
				fStrength *= min((1.f - fDist) * m_fFalloffScale, 1.f);

				if (m_bRadial && fDist > 0.f)
				{
					// Normalize vOffset for force scaling.
					fStrength *= isqrt_fast_tpl(vPosRel.GetLengthSquared());
					if (m_nFlags & ENV_GRAVITY)
						forces.vAccel = vPosRel * (m_Forces.vAccel.z * fStrength);
					if (m_nFlags & ENV_WIND)
						forces.vWind += vPosRel * (m_Forces.vWind.z * fStrength);
				}
				else
				{
					if (m_nFlags & ENV_GRAVITY)
						forces.vAccel = m_Forces.vAccel * fStrength;
					if (m_nFlags & ENV_WIND)
						forces.vWind += m_Forces.vWind * fStrength;
				}
			}

#ifdef DEBUG_FORCES
			assert(forcesPhys.vAccel.IsEquivalent(forces.vAccel));
			assert(forcesPhys.vWind.IsEquivalent(forces.vWind));
#endif
		}
	}

	if (nFlags & ENV_WATER)
	{
		GetWaterPlane(forces.plWater, vPos, forces.plWater.DistFromPlane(vPos));
	}
}

float SPhysEnviron::SArea::GetWaterPlane(Plane& plane, Vec3 const& vPos, float fMaxDist) const
{
	float fDist = m_Forces.plWater.DistFromPlane(vPos);
	if (fDist < fMaxDist)
	{
		if (m_bounds.IsOverlapSphereBounds(vPos, fMaxDist))
		{
			CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
			pe_status_contains_point st;
			st.pt = vPos - m_Forces.plWater.n * fDist;
			if (m_pArea->GetStatus(&st))
			{
				plane = m_Forces.plWater;
				return fDist;
			}
		}
	}
	return fMaxDist;
}

float SPhysEnviron::GetNonUniformWaterPlane(Plane& plWater, Vec3 const& vPos, float fMaxDist) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (auto& area : m_NonUniformAreas)
	{
		if (area.m_nFlags & ENV_WATER)
			fMaxDist = area.GetWaterPlane(plWater, vPos, fMaxDist);
	}

	return fMaxDist;
}

ETrinary SPhysEnviron::IsNonUniformUnderWater(AABB const& bb) const
{
	SAreaSpec spec { ENV_WATER, bb };
	for (const auto& area : m_NonUniformAreas)
	{
		if (area & spec)
			return ETrinary();
	}
	return false;
}

/*
   Emitters render either entirely inside or entirely outside VisAreas (due to rendering architecture).
    Emitter origin in vis area:
      Params.VisibleInside == false? Don't render
      else entire emitter in vis area: we're good
      else (intersecting): Test each particle for dist, shrink / fade-out near boundary
    Emitter origin outside vis areas:
      Params.VisibleInside == true ? Don't render
      else entirely outside vis areas: we're good
      else (intersecting): Test each particle for dist, shrink / fade-out near boundary
 */

void SVisEnviron::Update(Vec3 const& vOrigin, AABB const& bb)
{
	if (!m_bValid)
	{
		// Determine emitter's vis area, by origin.
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		m_pVisArea = Get3DEngine()->GetVisAreaFromPos(vOrigin);
		m_pVisNodeCache = 0;

		// See whether it crosses boundaries.
		if (m_pVisArea)
			m_bCrossesVisArea = m_pVisArea->GetAABBox()->IsIntersectBox(bb);
		else
			m_bCrossesVisArea = Get3DEngine()->IntersectsVisAreas(bb, &m_pVisNodeCache);

		m_bValid = true;
	}
}

IVisArea* SVisEnviron::GetClipVisArea(IVisArea* pVisAreaCam, AABB const& bb) const
{
	if (m_bCrossesVisArea)
	{
		// Clip only against cam area.
		if (pVisAreaCam != m_pVisArea)
		{
			if (pVisAreaCam)
			{
				if (pVisAreaCam->GetAABBox()->IsIntersectBox(bb))
				{
					if (!m_pVisArea || !pVisAreaCam->FindVisArea(m_pVisArea, 3, true))
						return pVisAreaCam;
				}
			}
			else if (m_pVisArea)
				return m_pVisArea;
		}
	}
	return 0;
}
