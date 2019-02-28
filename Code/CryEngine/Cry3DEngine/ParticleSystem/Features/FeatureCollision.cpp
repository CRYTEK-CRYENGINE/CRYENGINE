// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "FeatureCollision.h"

namespace pfx2
{

MakeDataType(EPDT_ContactPoint, SContactPoint);
MakeDataType(EPDT_ContactNext, SContactPredict);
MakeDataType(EPDT_CollideSpeed, float);
MakeDataType(EPDT_CollisionCount, uint);

MakeDataType(EPVF_PositionPrev,  Vec3);

//////////////////////////////////////////////////////////////////////////
// CFeatureCollision

void CFeatureCollision::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->PostInitParticles.add(this);
	pComponent->PostUpdateParticles.add(this);
	pComponent->AddParticleData(EPVF_PositionPrev);
	pComponent->AddParticleData(EPDT_ContactPoint);
	pComponent->AddParticleData(EPDT_ContactNext);
	if (m_collisionsLimitMode != ECollisionLimitMode::Unlimited)
		pComponent->AddParticleData(EPDT_CollisionCount);
	if (m_rotateToNormal)
		pComponent->AddParticleData(EPQF_Orientation);
}

void CFeatureCollision::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	SERIALIZE_VAR(ar, m_terrain);
	SERIALIZE_VAR(ar, m_staticObjects);
	SERIALIZE_VAR(ar, m_dynamicObjects);
	SERIALIZE_VAR(ar, m_water);
	SERIALIZE_VAR(ar, m_elasticity);
	SERIALIZE_VAR(ar, m_friction);
	SERIALIZE_VAR(ar, m_collisionsLimitMode);
	if (m_collisionsLimitMode != ECollisionLimitMode::Unlimited)
		SERIALIZE_VAR(ar, m_maxCollisions);
	SERIALIZE_VAR(ar, m_rotateToNormal);
}

enum
{
	ent_terrain_raytrace = ent_triggers
};

int CFeatureCollision::GetRayTraceFilter() const
{
	const int allowedCollisions = C3DEngine::GetCVars()->e_ParticlesCollisions & 3;
	int filter = 0;
	if (m_terrain && allowedCollisions >= 1)
	{
		if (C3DEngine::GetCVars()->e_ParticlesCollisions & AlphaBit('t'))
			// Use physics RayWorldIntersection for terrain intersections
			filter |= ent_terrain;
		else
			// Use more reliable Terrain::Raytrace
			filter |= ent_terrain_raytrace;
	}
	if (m_staticObjects && allowedCollisions >= 2)
		filter |= ent_static;
	if (m_dynamicObjects && allowedCollisions >= 3)
		filter |= ent_rigid | ent_sleeping_rigid | ent_living | ent_independent;
	if (m_water && allowedCollisions >= 1)
		filter |= ent_water;
	return filter;
}

struct SCollisionLimitIgnore
{
	SCollisionLimitIgnore(CParticleContainer& container)
		: m_contactPoints(container.IOStream(EPDT_ContactPoint)) {}
	void CollisionLimit(TParticleId particleId)
	{
		SContactPoint& contact = m_contactPoints[particleId];
		contact.m_ignore = 1;
		contact.m_collided = 0;
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
		SContactPoint& contact = m_contactPoints[particleId];
		contact.m_ignore = 1;
		contact.m_stopped = 1;
		contact.m_collided = 0;
		m_positions.Store(particleId, contact.m_point);
		m_velocities.Store(particleId, Vec3(ZERO));
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

void CFeatureCollision::PostInitParticles(CParticleComponentRuntime& runtime)
{
	CParticleContainer& container = runtime.GetContainer();
	container.FillData(EPDT_ContactPoint, SContactPoint(), container.GetSpawnedRange());
	container.FillData(EPDT_ContactNext, SContactPredict(), container.GetSpawnedRange());
	container.FillData(EPDT_CollisionCount, 0u, container.GetSpawnedRange());
	container.FillData(EPDT_CollideSpeed, 0.0f, container.GetSpawnedRange());
	container.CopyData(EPVF_PositionPrev, EPVF_Position, container.GetSpawnedRange());
}

// Collision constants
static float kMinSubdivideTime = 0.01f;  // Potentially subdivide frame if above this time
static float kMaxExtrapolation = 32.0f;  // Max factor we extrapolate a path in the future
static float kMaxPathDeviation = 0.5f;   // Subdivide path if it curves above this amount
static float kMinBounceTime    = 0.01f;  // Minimum time to continue checking collisions
static float kMinBounceDist    = 0.001f; // Distance to slide particle above surface
static float kExpandBackDist   = 0.001f, // Distance to expand backwards or forwards to prevent missed collisions
	         kExpandFrontDist  = 0.0f;
static float kImmediateTime    = 0.001f; // Collision time regarded as immediate, for iteration limiting
static uint  kImmediateLimit   = 2;
static uint  kTotalLimit       = 8;

static const int kCollisionsFlags = sf_max_pierceable | (geom_colltype_ray | geom_colltype13) << rwi_colltype_bit | rwi_colltype_any | rwi_ignore_noncolliding | rwi_ignore_back_faces;

ILINE bool Bounces(float acc, float vel)
{
	bool timeBounce = vel > acc * kMinBounceTime * -0.5f;
	bool distBounce = sqr(vel) > acc * kMinBounceDist * -2.0f;
	return timeBounce && distBounce;
}

// Quadratic path structure, and utilities
template<typename T, typename F>
struct QuadPathT
{
	F	time1, timeD;
	T	pos0, pos1, vel0, vel1, acc;

	QuadPathT() {}

	void FromPos01Vel1(const T& p0, const T& p1, const T& v1, F t1, F tMax)
	{
		time1 = t1;
		timeD = min(tMax, t1 * kMaxExtrapolation);
		pos0 = p0;
		pos1 = p1;
		vel1 = v1;
		const F invDT = F(1) / t1;
		vel0 = (pos1 - pos0) * (invDT * F(2)) - vel1;
		acc = (vel1 - vel0) * invDT;
	}

	// Interpolation
	ILINE T Pos(F t) const
	{
		return pos0 + PosD(t);
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

	float TimeFromDist(const Vec3& dir, float dist) const
	{
		float accNorm  = acc | dir;
		float vel0Norm = vel0 | dir;

		float t[2];
		for (int n = solve_quadratic(accNorm * 0.5f, vel0Norm, -dist, t); --n >= 0; )
		{
			if (t[n] >= 0.0f)
			{
				float v = vel0Norm + accNorm * t[n];
				if (v >= 0.0f)
					return t[n];
			}
		}
		return timeD;
	}

	bool AdjustCollision(SContactPoint& contact, float t0, float t1) const
	{
		contact.m_time = TimeFromDist(-contact.m_normal, (pos0 - contact.m_point) | contact.m_normal);
		if (inrange(contact.m_time, t0, t1))
		{
			contact.m_point = Pos(contact.m_time);
			return true;
		}
		return false;
	}

	void SlideAdjust(SContactPoint& contact, float friction)
	{
		// Constrain remaining path to surface
		const float accNorm = acc | contact.m_normal;
		const float velNorm = vel0 | contact.m_normal;
		if (Bounces(accNorm, velNorm))
		{
			// No longer sliding
			contact.m_sliding = contact.m_stopped = 0;
			return;
		}

		acc -= contact.m_normal * accNorm;
		const float accDrag = -accNorm * friction;
		if (contact.m_stopped)
		{
			if (acc.len2() <= sqr(accDrag))
			{
				// Still stopped
				acc = vel0 = vel1 = Vec3(0);
				pos1 = pos0;
				return;
			}
			contact.m_stopped = 0;
		}

		vel0 -= contact.m_normal * velNorm;

		// Add drag acceleration, opposite to current velocity,
		// limited by maximum movement during frame
		const float velSqr = vel0 | vel0;
		if (accDrag * timeD * velSqr > FLT_EPSILON)
		{
			const float rtime = rsqrt(velSqr) * accDrag;
			acc -= vel0 * rtime;

			// Test for stopping
			if (timeD * rtime > 1.0f)
			{
				timeD = rcp(rtime);
				if (timeD <= time1)
				{
					time1 = timeD;
					contact.m_stopped = true;
				}
			}
		}
		vel1 = contact.m_stopped ? Vec3(0) : Vel(time1);
		pos1 = Pos(time1);
	}
};

bool CFeatureCollision::RayWorldIntersection(SContactPoint& contact, const Vec3& startIn, const Vec3& rayIn) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (rayIn.IsZero())
		return false;

	Vec3 start = startIn;
	Vec3 ray = rayIn;

	Vec3 rayDir = ray.GetNormalizedFast();
	start -= rayDir * kExpandBackDist;
	ray += rayDir * (kExpandBackDist + kExpandFrontDist);

	contact.m_collided = 0;
	if (m_objectFilter & ent_terrain_raytrace)
	{
		CTerrain::SRayTrace rt;
		if (Cry3DEngineBase::GetTerrain()->RayTrace(start, start + ray, &rt, false))
		{
			contact.m_point = rt.vHit;
			contact.m_normal = rt.vNorm;
			contact.m_pSurfaceType = rt.pMaterial ? rt.pMaterial->GetSurfaceType() : nullptr;
			contact.m_time = rt.fInterp;
			contact.m_collided = 1;
			ray *= rt.fInterp;
		}
	}

	if (m_objectFilter & ~ent_terrain_raytrace)
	{
		ray_hit rayHit;
		while (gEnv->pPhysicalWorld->RayWorldIntersection(start, ray, (m_objectFilter & ~ent_terrain_raytrace | ent_no_ondemand_activation), kCollisionsFlags, &rayHit, 1))
		{
			if ((rayHit.n | ray) >= 0.0f)
			{
				ray += start - rayHit.pt;
				start = rayHit.pt;
			}
			else
			{
				contact.m_point = rayHit.pt;
				contact.m_normal = rayHit.n;
				contact.m_pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(rayHit.surface_idx);
				contact.m_time = rayHit.dist * rayIn.GetInvLength();
				contact.m_collided = 1;
				break;
			}
		}
	}

	return contact.m_collided;
}

bool CFeatureCollision::PathWorldIntersection(SContactPoint& contact, const QuadPath& path, float t0, float t1, bool splitInflection) const
{
	CRY_PFX2_PROFILE_DETAIL

	if (splitInflection && t1 - t0 > kMinBounceTime * 2.0f)
	{
		// Subdivide path at bounce or acceleration inflection point
		float tDiv = t1;
		if (contact.m_collided && !contact.m_sliding)
			tDiv = path.InflectionTime(contact.m_normal);
		if (tDiv >= t1)
			tDiv = path.InflectionTime(path.acc.GetNormalized());

		if (tDiv > t0 && tDiv < t1)
		{
			return PathWorldIntersection(contact, path, t0, tDiv, false)
				|| (tDiv < path.time1 && PathWorldIntersection(contact, path, tDiv, t1, false));
		}
	}

	if (t1 - t0 > kMinSubdivideTime)
	{
		// Subdivide based on path deviation from linear
		Vec3 posD = path.PosD(t0, t1);
		Vec3 posH = path.PosD(t0, (t0 + t1) * 0.5f);
		float sqrD = posD.len2(), sqrH = posH.len2(), dotDH = posD | posH;
		float maxDev = kMaxPathDeviation * (path.timeD > path.time1 ? 0.75f : 1.0f);
		if (sqrH * sqrD - sqr(dotDH) > sqr(kMaxPathDeviation) * sqrD)
		{
			float tDiv = (t0 + t1) * 0.5f;
			return PathWorldIntersection(contact, path, t0, tDiv, false)
				|| (tDiv < path.time1 && PathWorldIntersection(contact, path, tDiv, t1, false));
		}
	}

	Vec3 pos0 = path.Pos(t0);
	Vec3 ray = path.PosD(t0, t1);
	contact.m_collided = RayWorldIntersection(contact, pos0, ray)
		&& path.AdjustCollision(contact, t0, t1);

	if (m_doPrediction && !contact.m_collided)
	{
		contact.m_time = t1;
		contact.m_point = pos0 + ray;
		contact.m_normal = -ray.GetNormalized();
	}

	return contact.m_collided;
}

bool CFeatureCollision::TestSlide(SContactPoint& contact, Vec3& pos, Vec3& vel, const Vec3& acc) const
{
	// Test for continued sliding on surface
	float normTest = kMinBounceDist * 4.0f;
	SContactPoint contactSurface;
	if (RayWorldIntersection(contactSurface, pos, contact.m_normal * -normTest))
	{
		float accNorm = acc | contactSurface.m_normal;
		float velNorm = vel | contactSurface.m_normal;
		if (!Bounces(accNorm, velNorm))
		{
			// Still sliding
			contact.m_sliding = 1;
			contact.m_normal = contactSurface.m_normal;
			pos = contactSurface.m_point + contact.m_normal * kMinBounceDist;
			if (velNorm < 0.0f)
				vel += contact.m_normal * velNorm;
			return true;
		}
	}
	contact.m_sliding = 0;
	return false;
}

void CFeatureCollision::Collide(SContactPoint& contact, float& collideSpeed, QuadPath& path) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	path.time1 -= contact.m_time;
	path.timeD -= contact.m_time;
	path.pos0 = contact.m_point;
	path.vel0 = path.Vel(contact.m_time);

	// Collision response
	float velNorm = path.vel0 | contact.m_normal;
	if (velNorm > 0.0f)
	{
		// Left surface
		contact.m_collided = contact.m_sliding = contact.m_stopped = 0;
		return;
	}

	collideSpeed = -velNorm;

	// Sliding occurs when the bounce distance would be less than the collide buffer.
	// d = - v^2 / 2a <= dmax
	// v^2 <= -dmax*2a
	// Or when bounce time is less than limit
	// t = -2v/a
	// v <= -tmax*a/2
	float velBounce = -velNorm * m_elasticity;
	float accNorm = path.acc | contact.m_normal;
	if (Bounces(accNorm, velBounce))
	{
		// Bounce
		contact.m_sliding = 0;
	}
	else
	{
		// Slide
		contact.m_sliding = 1;
		velBounce = 0.0f;
		path.pos0 += contact.m_normal * kMinBounceDist;
	}

	Vec3 velocityDelta = contact.m_normal * (velBounce - velNorm);
	path.vel0 += velocityDelta;
	path.vel1 += velocityDelta;
	path.pos1 = path.Pos(path.time1);
}

bool CFeatureCollision::DoCollision(SContactPredict& contactNext, SContactPoint& contact, float& collideSpeed, QuadPath& path) const
{
	CRY_PFX2_PROFILE_DETAIL;

	Vec3 accOrig = path.acc;
	bool wasSliding = contact.m_sliding;
	Vec3 slideNormal = contact.m_normal;

	if (contact.m_sliding || contact.m_stopped)
	{
		bool wasStopped = contact.m_stopped;
		path.SlideAdjust(contact, m_friction);
		if (wasStopped && contact.m_stopped)
			return false;
	}

	if (m_doPrediction)
	{
		if (contactNext.m_time)
		{
			// Validate collision prediction
			Vec3 diff = path.pos1 - contactNext.m_point;
			float dist = diff | contactNext.m_normal;
			if (dist <= 0.0f)
			{
				// Crossed prediction plane
				if (!contactNext.m_collided)
					contactNext.m_time = 0;
				else
				{
					// Check if near predicted collision point
					contact = contactNext;
					path.AdjustCollision(contact, 0.0f, path.time1);
					if ((contact.m_point - contactNext.m_point).GetLengthSquared() > sqr(kMaxPathDeviation))
						contactNext.m_time = 0;
				}
			}
			else
			{
				// Check for reversed direction
				if ((path.vel1 | contactNext.m_testDir) < 0.0f)
					contactNext.m_time = 0;
				else
				{
					// Check for deviation from test path
					diff -= contactNext.m_testDir * (contactNext.m_testDir | diff);
					if (diff.GetLengthSquared() > sqr(kMaxPathDeviation))
						contactNext.m_time = 0;
				}
			}
		}

		if (!contactNext.m_time)
		{
			// New test
			static_cast<SContactPoint&>(contactNext) = contact;
			PathWorldIntersection(contactNext, path, 0.0f, path.timeD, true);
			contactNext.m_testDir = (contactNext.m_point - path.pos0).GetNormalized();
			if (contactNext.m_time > path.time1)
				contact.m_collided = false;
			else 
			{
				if (contactNext.m_collided)
					contact = contactNext;
				contactNext.m_time = 0;
			}
		}
		else if (contact.m_collided)
			contactNext.m_time = 0;
	}
	else
	{
		PathWorldIntersection(contact, path, 0.0f, path.time1, true);
	}

	path.acc = accOrig;
	if (contact.m_collided)
	{
		Collide(contact, collideSpeed, path);
		if (wasSliding && !contact.m_sliding)
		{
			// Determine whether still sliding on previous surface
			float accNorm = path.acc | slideNormal;
			float velNorm = path.vel0 | slideNormal;
			if (!Bounces(accNorm, velNorm))
			{
				contact.m_sliding = 1;
				contact.m_normal = slideNormal;
			}
		}
	}
	else if (contact.m_sliding)
	{
		if (!m_doPrediction || (path.pos1 - contact.m_point).GetLengthSquared() > sqr(kMaxPathDeviation))
			// Re-test for sliding
			TestSlide(contact, path.pos1, path.vel1, path.acc);
	}

	return contact.m_collided;
}

void CFeatureCollision::DoCollisions(CParticleComponentRuntime& runtime) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	CParticleContainer& container = runtime.GetContainer();
	const IFStream normAges = container.IStream(EPDT_NormalAge);
	const IFStream lifeTimes = container.IStream(EPDT_LifeTime);
	IVec3Stream positionsPrev = container.IStream(EPVF_PositionPrev);
	IOVec3Stream positions = container.IOStream(EPVF_Position);
	IOQuatStream orientations = container.IOStream(EPQF_Orientation);
	IOVec3Stream velocities = container.IOStream(EPVF_Velocity);
	IOFStream collideSpeeds = container.IOStream(EPDT_CollideSpeed);
	IOUintStream collisionCounts = container.IOStream(EPDT_CollisionCount);
	TIOStream<SContactPoint> contactPoints = container.IOStream(EPDT_ContactPoint);
	TIOStream<SContactPredict> contactNexts = container.IOStream(EPDT_ContactNext);

	SContactPredict contactNextDummy;

	for (auto particleId : runtime.FullRange())
	{
		SContactPoint& contact = contactPoints[particleId];
		if (contact.m_ignore)
			continue;

		const float normAge = normAges.Load(particleId);
		const float tLife = lifeTimes.Load(particleId);
		float tD = DeltaTime(runtime.DeltaTime(), normAge, tLife);
		if (tD <= 0.0f)
			continue;

		Vec3 position0 = positionsPrev.Load(particleId);
		Vec3 position1 = positions.Load(particleId);
		Vec3 velocity1 = velocities.Load(particleId);

		// Create path struct
		QuadPath path;
		const float tMax = tD + max(tLife - normAge * tLife, 0.0f);
		path.FromPos01Vel1(position0, position1, velocity1, tD, tMax);

		SContactPredict& contactNext = contactNexts.IsValid() ? contactNexts[particleId] : contactNextDummy;

		contact.m_collided = 0;
		uint immediateContacts = 0;  // Escape anomalous infinite loop
		uint totalContacts = 0;
		float timeContact = 0.0f;
		float collideSpeed = 0.0f;

		// Repeat collision test & response in frame
		while (DoCollision(contactNext, contact, collideSpeed, path))
		{
			totalContacts++;
			if (contact.m_time <= kImmediateTime)
				immediateContacts++;
			else
				immediateContacts = 0;
			timeContact += contact.m_time;
			contact.m_time = timeContact;

			if (immediateContacts >= kImmediateLimit || totalContacts >= kTotalLimit)
				break;
		}

		if (totalContacts || contact.m_sliding)
		{
			positions.Store(particleId, path.pos1);
			velocities.Store(particleId, path.vel1);
			if (m_rotateToNormal)
			{
				Quat orientation = orientations.Load(particleId);
				const Quat rotate = Quat::CreateRotationV0V1(orientation.GetColumn2(), contact.m_normal);
				orientation = rotate * orientation;
				orientations.Store(particleId, orientation);
			}
		}

		contact.m_collided = totalContacts > 0;
		if (collideSpeeds.IsValid())
			collideSpeeds.Store(particleId, collideSpeed);
		if (collisionCounts.IsValid())
		{
			uint collisionCount = collisionCounts.Load(particleId);
			collisionCounts.Store(particleId, collisionCount + totalContacts);
		}
	}
}

void CFeatureCollision::PostUpdateParticles(CParticleComponentRuntime& runtime)
{
	CRY_PFX2_PROFILE_DETAIL;

	if (C3DEngine::GetCVars()->e_ParticlesCollisions == 0)
		return;

	m_doPrediction = !(C3DEngine::GetCVars()->e_ParticlesCollisions & AlphaBit('p'));
	m_objectFilter = GetRayTraceFilter();

	DoCollisions(runtime);

	switch (m_collisionsLimitMode)
	{
	case ECollisionLimitMode::Ignore:
		UpdateCollisionLimit<SCollisionLimitIgnore>(runtime);
		break;
	case ECollisionLimitMode::Stop:
		UpdateCollisionLimit<SCollisionLimitStop>(runtime);
		break;
	case ECollisionLimitMode::Kill:
		UpdateCollisionLimit<SCollisionLimitKill>(runtime);
		break;
	}

	CParticleContainer& container = runtime.GetContainer();
	container.CopyData(EPVF_PositionPrev, EPVF_Position, container.GetFullRange());
}

template<typename TCollisionLimit>
void CFeatureCollision::UpdateCollisionLimit(CParticleComponentRuntime& runtime) const
{
	CParticleContainer& container = runtime.GetContainer();
	TCollisionLimit limiter(container);
	const auto collisionCounts = container.IStream(EPDT_CollisionCount);

	for (auto particleId : runtime.FullRange())
	{
		if (collisionCounts.Load(particleId) >= m_maxCollisions)
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
