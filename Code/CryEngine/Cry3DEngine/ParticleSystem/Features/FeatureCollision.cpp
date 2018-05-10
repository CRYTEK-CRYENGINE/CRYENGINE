// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCollision.h"
#include "ParticleSystem/ParticleComponentRuntime.h"

namespace pfx2
{

MakeDataType(EPDT_ContactPoint, SContactPoint);
MakeDataType(EPDT_CollideSpeed, float);

extern TDataType<Vec3>   EPVF_PositionPrev;

//////////////////////////////////////////////////////////////////////////
// CFeatureCollision

CFeatureCollision::CFeatureCollision()
	: m_collisionsLimitMode(ECollisionLimitMode::Unlimited), m_rotateToNormal(false)
	, m_terrain(true), m_staticObjects(true), m_dynamicObjects(false)
{
}

void CFeatureCollision::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->InitParticles.add(this);
	pComponent->PostUpdateParticles.add(this);
	pComponent->AddParticleData(EPVF_PositionPrev);
	pComponent->AddParticleData(EPDT_ContactPoint);
	if (m_rotateToNormal)
		pComponent->AddParticleData(EPQF_Orientation);
}

void CFeatureCollision::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	ar(m_terrain, "Terrain", "Terrain");
	ar(m_staticObjects, "StaticObjects", "Static Objects");
	ar(m_dynamicObjects, "DynamicObjects", "Dynamic Objects");
	ar(m_elasticity, "Elasticity", "Elasticity");
	ar(m_friction, "Friction", "Friction");
	ar(m_collisionsLimitMode, "CollisionsLimitMode", "Collision Limit");
	if (m_collisionsLimitMode != ECollisionLimitMode::Unlimited)
		ar(m_maxCollisions, "MaxCollisions", "Maximum Collisions");
	ar(m_rotateToNormal, "RotateToNormal", "Rotate to Normal");
}

int CFeatureCollision::GetRayTraceFilter() const
{
	int filter = ent_no_ondemand_activation;
	if (m_terrain)
		filter |= ent_terrain;
	if (m_staticObjects)
		filter |= ent_static;
	if (m_dynamicObjects)
		filter |= ent_rigid | ent_sleeping_rigid | ent_living | ent_independent;
	return filter;
}

struct SCollisionLimitIgnore
{
	SCollisionLimitIgnore(CParticleContainer& container)
		: m_contactPoints(container.IOStream(EPDT_ContactPoint)) {}
	void CollisionLimit(TParticleId particleId)
	{
		SContactPoint contact = m_contactPoints.Load(particleId);
		contact.m_state.ignore = 1;
		m_contactPoints.Store(particleId, contact);
	}
	TIOStream<SContactPoint> m_contactPoints;
};

struct SCollisionLimitStop
{
	SCollisionLimitStop(CParticleContainer& container)
		: m_contactPoints(container.IOStream(EPDT_ContactPoint))
		, m_positions(container.GetIOVec3Stream(EPVF_Position))
		, m_velocities(container.GetIOVec3Stream(EPVF_Velocity)) {}
	void CollisionLimit(TParticleId particleId)
	{
		SContactPoint contact = m_contactPoints.Load(particleId);
		contact.m_state.ignore = 1;
		m_positions.Store(particleId, contact.m_point);
		m_velocities.Store(particleId, Vec3(ZERO));
		m_contactPoints.Store(particleId, contact);
	}
	TIOStream<SContactPoint> m_contactPoints;
	IOVec3Stream             m_positions;
	IOVec3Stream             m_velocities;
};

struct SCollisionLimitKill
{
	SCollisionLimitKill(CParticleContainer& container)
		: m_ages(container.GetIOFStream(EPDT_NormalAge)) {}
	void CollisionLimit(TParticleId particleId)
	{
		m_ages.Store(particleId, 1.0f);
	}
	IOFStream m_ages;
};

void CFeatureCollision::InitParticles(const SUpdateContext& context)
{
	CParticleContainer& container = context.m_container;
	container.FillData(EPDT_ContactPoint, SContactPoint(), container.GetSpawnedRange());
}

// Collision constants
static bool  bTraceTerrain     = true;
static bool  bAdjustPos        = false;
static float kMinSubdivideTime = 0.05f;  // Potentially subdivide frame if above this time
static float kMaxPathDeviation = 1.0f;   // Subdivide frame path curves above this amount
static float kMinBounceTime    = 0.01f;  // Minimum time to continue checking collisions
static float kMinBounceDist    = 0.001f; // Distance to slide particle above surface
static float kExpandBack       = 0.01f,  // Fraction of test distance to expand backwards or forwards to prevent missed collisions
	         kExpandFront      = 0.0f;

static const int kCollisionsFlags = sf_max_pierceable | (geom_colltype_ray | geom_colltype13) << rwi_colltype_bit | rwi_colltype_any | rwi_ignore_noncolliding | rwi_ignore_back_faces;

// Quadratic path structure, and utilities
template<typename T, typename F>
struct QuadPathT
{
	F	timeD;
	T	pos0, pos1, vel0, vel1, acc;

	QuadPathT() {}

	void FromPos01Vel1(const T& p0, const T& p1, const T& v1, F t1)
	{
		pos0 = p0;
		pos1 = p1;
		vel1 = v1;
		timeD = t1;
		const F invDT = F(1) / t1;
		T velAvg = (pos1 - pos0) * invDT;
		vel0 = velAvg * F(2) - vel1;
		acc = (vel1 - vel0) * invDT;
	}

	ILINE T Pos(F t) const
	{
		return pos0 + (vel0 + acc * (t * F(0.5))) * t;
	}
	ILINE T PosD(F t) const
	{
		return (vel0 + acc * (t * F(0.5))) * t;
	}
	ILINE T PosD(F t0, F t1) const
	{
		return vel0 * (t1 - t0) + acc * ((sqr(t1) - sqr(t0)) * 0.5f);
	}
	ILINE T Vel(F t) const
	{
		return vel0 + acc * t;
	}
	void SetEnds()
	{
		vel1 = Vel(timeD);
		pos1 = Pos(timeD);
	}

	F InflectionTime(const Vec3_tpl<F>& dir) const
	{
		F vPar = vel0 | dir,
		  aPar = acc | dir;
		if (inrange(-vPar, aPar * kMinBounceTime, aPar * (timeD - kMinBounceTime)))
		{
			F tFlec = -vPar / aPar;
			F dist = vPar * tFlec + aPar * sqr(tFlec) * F(0.5);
			if (abs(dist) > kMinBounceDist)
				return tFlec;
		}
		return timeD;
	}
};


bool RayWorldIntersection(SContactPoint& contact, const Vec3& startIn, const Vec3& rayIn, int objectFilter, bool checkSliding = true)
{
	CRY_PFX2_PROFILE_DETAIL

	Vec3 start = startIn;
	Vec3 ray = rayIn;

	if (checkSliding && contact.m_state.sliding)
	{
		// Test along contact plane
		Vec3 adjust = contact.m_normal * (kMinBounceDist * 2.0f);
		bool hitFront = RayWorldIntersection(contact, start, ray, objectFilter, false);

		// Test backward from hit or end to find negative hit
		Vec3 end = hitFront ? contact.m_point : start + ray;
		ray = start - end;
		end -= adjust;
		float rayLen2 = ray.len2();
		if (rayLen2 > sqr(kMinBounceDist))
		{
			Vec3 rayAdjust = ray * (rsqrt_fast(rayLen2) * kMinBounceDist);
			end += rayAdjust;
			ray -= (rayAdjust * 2.0f);

			SContactPoint contactBack;
			if (RayWorldIntersection(contactBack, end, ray, objectFilter, false) && contactBack.m_time > 0.0f && contactBack.m_time < 1.0f)
			{
				contact = contactBack;
				contact.m_point += adjust;
				contact.m_time = -contact.m_time;
				contact.m_state.collided = 0;
				return true;
			}
		}
		return hitFront;
	}

	if (ray.IsZero())
		return false;

	start -= ray * kExpandBack;
	ray *= (1.0f + kExpandBack + kExpandFront);

	// Use more reliable Terrain::Raytrace, controllable by debug cvar
	if ((objectFilter & ent_terrain) && !(C3DEngine::GetCVars()->e_ParticlesDebug & AlphaBit('t')))
	{
		objectFilter &= ~ent_terrain;
		CHeightMap::SRayTrace rt;
		if (Cry3DEngineBase::GetTerrain()->RayTrace(start, start + ray, &rt, false))
		{
			contact.m_point = rt.vHit;
			contact.m_normal = rt.vNorm;
			contact.m_time = rt.fInterp;
			contact.m_state.collided = 1;
			ray *= rt.fInterp;
		}
	}

	if (objectFilter)
	{
		ray_hit rayHit;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(start, ray, objectFilter, kCollisionsFlags, &rayHit, 1))
		{
			CRY_PFX2_ASSERT((rayHit.n | ray) <= 0.0f);
			contact.m_point = rayHit.pt;
			contact.m_normal = rayHit.n;
			contact.m_time = rayHit.dist * rsqrt(rayIn.len2());
			contact.m_state.collided = 1;
		}
	}

	return contact.m_state.collided;
}

bool PathWorldIntersection(SContactPoint& contact, const QuadPath& path, float t0, float t1, int objectFilter)
{
	CRY_PFX2_PROFILE_DETAIL

	if (t0 == 0.0f && t1 == path.timeD && t1 > kMinBounceTime * 2.0f)
	{
		// Subdivide path at acceleration inflection point
		float tDiv = t1;
		if (contact.m_totalCollisions && !contact.m_state.sliding)
			tDiv = path.InflectionTime(contact.m_normal);
		if (tDiv == t1)
			tDiv = path.InflectionTime(path.acc.GetNormalized());

		if (tDiv > t0 && tDiv < t1)
		{
			return PathWorldIntersection(contact, path, t0, tDiv, objectFilter)
				|| PathWorldIntersection(contact, path, tDiv, t1, objectFilter);
		}
	}

	float tD = t1 - t0;
	if (tD > kMinSubdivideTime)
	{
		// Check max path deviation, and subdivide if necessary
		// dev^2 = (v1^2 - (v1|d)^2 / d^2) (dt/4)^2 > devMax^2
		// vd := v1 dt/4
		// - (vd|d)^2 > (devMax^2 - vd^2) * d^2
		Vec3 posD = path.PosD(t0, t1);
		Vec3 rayVel = path.Vel(t1) * (tD * 0.25f);
		if (sqr(rayVel | posD) < (rayVel.len2() - sqr(kMaxPathDeviation)) * posD.len2())
		{
			float tDiv = (t0 + t1) * 0.5f;
			return PathWorldIntersection(contact, path, t0, tDiv, objectFilter)
				|| PathWorldIntersection(contact, path, tDiv, t1, objectFilter);
		}
	}

	return RayWorldIntersection(contact, path.Pos(t0), path.PosD(t0, t1), objectFilter);
}

bool SetContactTime(SContactPoint& contact, const QuadPath& path, float& accNorm, float& velNorm)
{
	accNorm = path.acc | contact.m_normal;
	float vel0Norm = path.vel0 | contact.m_normal;
	float delNorm  = (path.pos0 - contact.m_point) | contact.m_normal;

	float times[2];
	int n = solve_quadratic(accNorm * 0.5f, vel0Norm, delNorm, times);
	while (n-- > 0)
	{
		velNorm = vel0Norm + accNorm * times[n];
		if (velNorm * contact.m_time <= 0.f && times[n] > -path.timeD && times[n] < 2.0f * path.timeD)
		{
			contact.m_time = clamp(times[n], 0.0f, path.timeD);
			return true;
		}
	}

	return false;
}

ILINE bool Bounces(float acc, float vel)
{
	bool timeBounce = vel > acc * kMinBounceTime * -0.5f;
	bool distBounce = sqr(vel) > acc * kMinBounceDist * -2.0f;
	return timeBounce && distBounce;
}

bool CFeatureCollision::DoCollision(SContactPoint& contact, QuadPath& path, int objectFilter, bool doSliding) const
{
	CRY_PFX2_PROFILE_DETAIL

	if (doSliding && contact.m_state.sliding)
	{
		Vec3 accOrig = path.acc;
		const float accNorm = path.acc | contact.m_normal;
		if (accNorm > 0.0f)
		{
			// No longer sliding
			contact.m_state.sliding = 0;
		}
		else
		{
			path.acc -= contact.m_normal * accNorm;

			// Add sliding acceleration, opposite to current velocity,
			// limited by maximum movement during frame
			const float velSqr = path.vel0.len2();
			if (velSqr * path.timeD * m_friction > FLT_EPSILON)
			{
				const float rvel = rsqrt(velSqr);
				const Vec3 velDir = path.vel0 * rvel;
				float accDrag = -accNorm * m_friction;
				if (sqr(accDrag) >= path.acc.len2()) // Friction allows object to stop
				{
					const float accMax = rcp(rvel * path.timeD);
					if (accDrag >= accMax) // Will stop this frame
						accDrag = accMax;
				}
				path.acc -= velDir * accDrag;
			}

			path.SetEnds();

			SContactPoint contactPrev = contact;
			bool hit = DoCollision(contact, path, objectFilter, false);
			path.acc = accOrig;

			if (hit)
			{
				if (contact.m_state.collided && !contact.m_state.sliding)
				{
					// Determine whether still sliding on previous surface
					float accNorm = path.acc | contactPrev.m_normal;
					float velNorm = path.vel0 | contactPrev.m_normal;
					if (!Bounces(accNorm, velNorm))
					{
						contact.m_state.sliding = 1;
						contact.m_normal = contactPrev.m_normal;
						path.pos0 += contact.m_normal * kMinBounceDist;
					}
				}
			}

			if (!contact.m_state.sliding)
				path.SetEnds();

			return true;
		}
	}

	contact.m_state.collided = 0;
	float accNorm, velNorm;
	if (!PathWorldIntersection(contact, path, 0.0f, path.timeD, objectFilter) || 
		!SetContactTime(contact, path, accNorm, velNorm))
	{
		path.timeD = 0.0f;
		return false;
	}

	if (bAdjustPos)
		// Adjust contact point to actual path point
		contact.m_point = path.Pos(contact.m_time);

	path.timeD -= contact.m_time;
	path.pos0 = contact.m_point;
	path.vel0 = path.Vel(contact.m_time);

	// Collision response
	if (velNorm > 0.0f)
	{
		// Left sliding surface
		contact.m_state.collided = contact.m_state.sliding = 0;
		return true;
	}

	contact.m_speedIn = -velNorm;
	contact.m_totalCollisions += contact.m_state.collided;

	// Sliding occurs when the bounce distance would be less than the collide buffer.
	// d = - v^2 / 2a <= dmax
	// v^2 <= -dmax*2a
	// Or when bounce time is less than limit
	// t = -2v/a
	// v <= -tmax*a/2
	float velBounce = -velNorm * m_elasticity;
	if (Bounces(accNorm, velBounce))
	{
		// Bounce
		contact.m_state.sliding = 0;
	}
	else
	{
		// Slide
		contact.m_state.sliding = 1;
		velBounce = 0.0f;
		path.pos0 += contact.m_normal * kMinBounceDist;
	}

	Vec3 velocityDelta = contact.m_normal * (velBounce - velNorm);
	path.vel0 += velocityDelta;
	path.vel1 += velocityDelta;
	path.pos1 = path.Pos(path.timeD);

	return true;
}


void CFeatureCollision::DoCollisions(const SUpdateContext& context) const
{
	// #PFX2_TODO : raytrace caching not implemented yet
	const int objectFilter = GetRayTraceFilter();

	CParticleContainer& container = context.m_container;
	const IFStream normAges = container.GetIFStream(EPDT_NormalAge);
	const IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);
	IVec3Stream positionsPrev = container.GetIVec3Stream(EPVF_PositionPrev);
	IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
	IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
	IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
	IOFStream collideSpeeds = container.GetIOFStream(EPDT_CollideSpeed);
	TIOStream<SContactPoint> contactPoints = container.IOStream(EPDT_ContactPoint);

	for (auto particleId : context.GetUpdateRange())
	{
		SContactPoint contact = contactPoints.Load(particleId);
		if (contact.m_state.ignore)
			continue;

		float dT = DeltaTime(context.m_deltaTime, particleId, normAges, lifeTimes);
		if (dT == 0.0f)
			continue;

		Vec3 position0 = positionsPrev.Load(particleId);
		Vec3 position1 = positions.Load(particleId);
		Vec3 velocity1 = velocities.Load(particleId);

		QuadPath path;
		path.FromPos01Vel1(position0, position1, velocity1, dT);

		uint prevCollisions = contact.m_totalCollisions;
		uint immediateContacts = 0;  // Escape anomalous infinite loop
		bool contacted = false;
		float timeSum = 0.0f;
		while (path.timeD > 0.0f && immediateContacts < 2 && DoCollision(contact, path, objectFilter))
		{
			if (!contact.m_time)
				immediateContacts++;
			else
				immediateContacts = 0;
			timeSum += contact.m_time;
			contact.m_time = timeSum;
			contacted = true;
		}

		if (contacted)
		{
			contact.m_state.collided = contact.m_totalCollisions > prevCollisions;
			positions.Store(particleId, path.pos1);
			velocities.Store(particleId, path.vel1);
			if (collideSpeeds.IsValid())
				collideSpeeds.Store(particleId, contact.m_speedIn);
			if (m_rotateToNormal)
			{
				Quat orientation = orientations.Load(particleId);
				const Quat rotate = Quat::CreateRotationV0V1(orientation.GetColumn2(), contact.m_normal);
				orientation = rotate * orientation;
				orientations.Store(particleId, orientation);
			}
		}
		else
		{
			contact.m_state = {};
		}

		contactPoints.Store(particleId, contact);
	}
}

void CFeatureCollision::PostUpdateParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	DoCollisions(context);

	switch (m_collisionsLimitMode)
	{
	case ECollisionLimitMode::Ignore:
		UpdateCollisionLimit<SCollisionLimitIgnore>(context);
		break;
	case ECollisionLimitMode::Stop:
		UpdateCollisionLimit<SCollisionLimitStop>(context);
		break;
	case ECollisionLimitMode::Kill:
		UpdateCollisionLimit<SCollisionLimitKill>(context);
		break;
	}
}

template<typename TCollisionLimit>
void CFeatureCollision::UpdateCollisionLimit(const SUpdateContext& context) const
{
	CParticleContainer& container = context.m_container;
	TCollisionLimit limiter(container);
	const auto contactPoints = container.IStream(EPDT_ContactPoint);

	for (auto particleId : context.GetUpdateRange())
	{
		const SContactPoint contact = contactPoints.Load(particleId);
		if (contact.m_totalCollisions >= m_maxCollisions)
			limiter.CollisionLimit(particleId);
	}
}

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureCollision, "Motion", "Collisions", colorMotion);

//////////////////////////////////////////////////////////////////////////
// CFeatureGPUCollision

void CFeatureGPUCollision::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->UpdateParticles.add(this);

	if (auto pInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Collision))
	{
		gpu_pfx2::SFeatureParametersCollision params;
		params.offset = m_offset;
		params.radius = m_radius;
		params.restitution = m_restitution;
		pInt->SetParameters(params);
	}
}

void CFeatureGPUCollision::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	ar(m_offset, "Offset", "Offset");
	ar(m_radius, "Radius", "Radius");
	ar(m_restitution, "Restitution", "Restitution");
}

CRY_PFX2_LEGACY_FEATURE(CFeatureGPUCollision, "Motion", "Collision");
CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureGPUCollision, "GPU Particles", "Collision", colorGPU);

}
