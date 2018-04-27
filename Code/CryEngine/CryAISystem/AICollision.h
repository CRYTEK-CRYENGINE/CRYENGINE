// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef WORLD_COLLISION_H
#define WORLD_COLLISION_H

#if _MSC_VER > 1000
	#pragma once
#endif

//TODO: Use actual agent's parameters instead of this constants if possible

// for finding the start/end positions
const float WalkabilityFloorUpDist = 0.25f;
const float WalkabilityFloorDownDist = 2.0f;

// assume character is this fat
const float WalkabilityRadius = 0.25f;

// radius of the swept sphere down to find the floor
const float WalkabilityDownRadius = 0.06f;

// assume character is this tall
const float WalkabilityTotalHeight = 1.8f;

// maximum allowed floor height change from one to the next
const float WalkabilityMaxDeltaZ = 0.6f;
// height of the torso capsule above foot
const Vec3 WalkabilityTorsoBaseOffset(0.0f, 0.0f, WalkabilityMaxDeltaZ);

// Simple auto_ptr to allow thread-safe and efficient use of GetEntitiesInBox while not encouraging memory leaks
class PhysicalEntityListAutoPtr
{
public:
	PhysicalEntityListAutoPtr()
	{ m_pList = 0; }

	~PhysicalEntityListAutoPtr()
	{ *this = 0; }

	// Perform cleanup of memory allocated by physics
	void              operator=(IPhysicalEntity** pList);

	IPhysicalEntity*  operator[](size_t i) { return m_pList[i]; }
	IPhysicalEntity** GetPtr()             { return m_pList; }

private:
	// Leave undefined
	PhysicalEntityListAutoPtr(const PhysicalEntityListAutoPtr&);
	void operator=(const PhysicalEntityListAutoPtr&);

	IPhysicalEntity** m_pList;
};

// Easy to use timers - useful for quick performance comparisons
struct AccurateStopTimer
{
	enum State
	{
		Running,
		Stopped,
	};

	AccurateStopTimer()
		: start_((int64)0ll)
		, total_((int64)0ll)
		, state_(Stopped)
	{
	}

	ILINE void Reset()
	{
		total_.SetValue(0);
		start_.SetValue(0);
	}

	ILINE void Start()
	{
		assert(state_ != Running);
		state_ = Running;
		start_ = gEnv->pTimer->GetAsyncTime();
	}

	ILINE void Stop()
	{
		total_ += gEnv->pTimer->GetAsyncTime() - start_;
		assert(state_ != Stopped);
		state_ = Stopped;
	}

	ILINE State GetState() const
	{
		return state_;
	}

	ILINE CTimeValue GetTime() const
	{
		if (state_ == Running)
			return gEnv->pTimer->GetAsyncTime() - start_;
		else
			return total_;
	}

private:
	CTimeValue start_;
	CTimeValue total_;
	State      state_;
};

struct ScopedAutoTimer
{
	explicit ScopedAutoTimer(AccurateStopTimer& timer, bool reset = false)
		: timer_(timer)
	{
		if (reset)
			timer_.Reset();
		timer_.Start();
	}

	~ScopedAutoTimer()
	{
		timer_.Stop();
	}

private:
	AccurateStopTimer& timer_;
};

#if 0
/// Generates 2D convex hull from ptsIn using Graham's scan.
void ConvexHull2DGraham(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn);
#endif

/// Generates 2D convex hull from ptsIn using Andrew's algorithm.
void ConvexHull2DAndrew(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn);

/// Generates 2D convex hull from ptsIn
inline void ConvexHull2D(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	// [Mikko] Note: The convex hull calculation is bound by the sorting.
	// The sort in Andrew's seems to be about 3-4x faster than Graham's--using Andrew's for now.
	ConvexHull2DAndrew(ptsOut, ptsIn);
}

/// Return the closest intersection point on surface of the swept sphere
/// and distance along the linesegment. hitPos can be zero - may be faster
bool IntersectSweptSphere(Vec3* hitPos, float& hitDist, const Lineseg& lineseg,
                          float radius, EAICollisionEntities aiCollisionEntities, IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, int geomFlagsAny = geom_colltype0);


const size_t GetPhysicalEntitiesInBoxMaxResultCount = 2048;
// multiply the number by two to get it more safe, since it looks like physics can allocate its own list in case more entities are in such a box
typedef StaticDynArray<AABB, GetPhysicalEntitiesInBoxMaxResultCount>             StaticAABBArray;
typedef StaticDynArray<IPhysicalEntity*, GetPhysicalEntitiesInBoxMaxResultCount> StaticPhysEntityArray;

bool FindFloor(const Vec3& position, Vec3& floor);

//====================================================================
// GetEntitiesFromAABB
//====================================================================
inline unsigned GetEntitiesFromAABB(PhysicalEntityListAutoPtr& entities, const AABB& aabb, EAICollisionEntities aiCollisionEntities)
{
	IPhysicalEntity** pEntities = NULL;
	unsigned nEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(aabb.min, aabb.max, pEntities, aiCollisionEntities | ent_allocate_list);

	// (MATT) At this time, geb() will return pointer to internal memory even if no entities were found  {2009/11/23}
	if (!nEntities)
		pEntities = NULL;
	entities = pEntities;
	return nEntities;
}

ILINE size_t GetPhysicalEntitiesInBox(Vec3 boxMin, Vec3 boxMax, IPhysicalEntity**& entityList, int entityTypes)
{
	extern IPhysicalEntity* g_AIEntitiesInBoxPreAlloc[GetPhysicalEntitiesInBoxMaxResultCount];

	entityList = g_AIEntitiesInBoxPreAlloc;

	size_t entityCount = (size_t)gEnv->pPhysicalWorld->GetEntitiesInBox(boxMin, boxMax, entityList,
	                                                                    entityTypes | ent_allocate_list, GetPhysicalEntitiesInBoxMaxResultCount);

	if (entityCount > GetPhysicalEntitiesInBoxMaxResultCount)
	{
		assert(entityList != &g_AIEntitiesInBoxPreAlloc[0]);
		memcpy(g_AIEntitiesInBoxPreAlloc, entityList, sizeof(IPhysicalEntity*) * GetPhysicalEntitiesInBoxMaxResultCount);

		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(entityList);
		entityList = g_AIEntitiesInBoxPreAlloc;

		return GetPhysicalEntitiesInBoxMaxResultCount;
	}

	return entityCount;
}

ILINE size_t GetPhysicalEntitiesInBox(Vec3 boxMin, Vec3 boxMax, StaticPhysEntityArray& entityList, int entityTypes)
{
	entityList.resize(GetPhysicalEntitiesInBoxMaxResultCount);
	IPhysicalEntity** entityListPtr = &entityList[0];

	size_t entityCount = (size_t)gEnv->pPhysicalWorld->GetEntitiesInBox(boxMin, boxMax, entityListPtr,
	                                                                    entityTypes | ent_allocate_list, GetPhysicalEntitiesInBoxMaxResultCount);

	if (entityCount > GetPhysicalEntitiesInBoxMaxResultCount)
	{
		assert(entityListPtr != &entityList[0]);
		memcpy(&entityList[0], entityListPtr, sizeof(IPhysicalEntity*) * GetPhysicalEntitiesInBoxMaxResultCount);

		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(entityListPtr);

		return GetPhysicalEntitiesInBoxMaxResultCount;
	}

	entityList.resize(entityCount);

	return entityCount;
}

//====================================================================
// OverlapSphere
//====================================================================
inline bool OverlapSphere(const Vec3& pos, float radius, EAICollisionEntities aiCollisionEntities)
{
	primitives::sphere spherePrim;
	spherePrim.center = pos;
	spherePrim.r = radius;

	intersection_params params;
	params.bNoBorder = true;
	params.bNoAreaContacts = true;
	params.bNoIntersection = true;
	params.bStopAtFirstTri = true;
	params.bThreadSafe = true;

	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	float d = pPhysics->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, Vec3Constants<float>::fVec3_Zero,
	                                               aiCollisionEntities, 0, 0, geom_colltype0, &params);

	return (d != 0.0f);
}

//====================================================================
// OverlapCapsule
//====================================================================
inline bool OverlapCapsule(const Lineseg& lineseg, float radius, EAICollisionEntities aiCollisionEntities)
{
	primitives::capsule capsulePrim;
	capsulePrim.center = 0.5f * (lineseg.start + lineseg.end);
	capsulePrim.axis = lineseg.end - lineseg.start;
	capsulePrim.hh = 0.5f * capsulePrim.axis.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
	capsulePrim.r = radius;

	intersection_params ip;
	ip.bNoBorder = true;
	ip.bNoAreaContacts = true;
	ip.bNoIntersection = true;
	ip.bStopAtFirstTri = true;
	ip.bThreadSafe = true;

	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	float d = pPhysics->PrimitiveWorldIntersection(capsulePrim.type, &capsulePrim, Vec3Constants<float>::fVec3_Zero,
	                                               aiCollisionEntities, 0, 0, geom_colltype0, &ip);

	return (d != 0.0f);
}

//====================================================================
// OverlapCylinder
//====================================================================
inline bool OverlapCylinder(const Lineseg& lineseg, float radius, EAICollisionEntities aiCollisionEntities, IPhysicalEntity* pSkipEntity = NULL, int customFilter = 0, int* pEntityID = 0, int maxIDs = 0)
{
	IPhysicalEntity* pSkipEnts = pSkipEntity;

	primitives::cylinder cylinderPrim;
	cylinderPrim.center = 0.5f * (lineseg.start + lineseg.end);
	cylinderPrim.axis = lineseg.end - lineseg.start;
	cylinderPrim.hh = 0.5f * cylinderPrim.axis.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
	cylinderPrim.r = radius;

	geom_contact* contacts = 0;
	intersection_params params;
	params.bNoBorder = true;
	params.bNoAreaContacts = true;
	params.bNoIntersection = true;
	params.bStopAtFirstTri = true;

	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	WriteLockCond lockContacts;
	float d = pPhysics->PrimitiveWorldIntersection(cylinderPrim.type, &cylinderPrim, Vec3Constants<float>::fVec3_Zero,
	                                               aiCollisionEntities, &contacts, 0, customFilter ? customFilter : geom_colltype0, &params, 0, 0, &pSkipEnts, pSkipEntity ? 1 : 0, &lockContacts);

	if (contacts && pEntityID)
	{
		maxIDs = min(maxIDs, static_cast<int>(d));
		for (int loop = 0; loop < maxIDs; ++loop)
		{
			pEntityID[loop] = contacts[loop].iPrim[0];
		}
	}

	if (d != 0.0f)
	{
		return true;
	}

	return false;
}

//====================================================================
// IntersectSegment
//====================================================================
inline bool OverlapSegment(const Lineseg& lineseg, EAICollisionEntities aiCollisionEntities)
{
	Vec3 dir = lineseg.end - lineseg.start;
	ray_hit hit;
	if (gAIEnv.pRayCaster->Cast(RayCastRequest(lineseg.start, dir, aiCollisionEntities,
	                                           rwi_ignore_noncolliding | rwi_stop_at_pierceable)))
		return true;
	else
		return false;
}

//====================================================================
// IntersectSegment
//====================================================================
inline bool IntersectSegment(Vec3& hitPos, const Lineseg& lineseg, EAICollisionEntities aiCollisionEntities, int filter = rwi_ignore_noncolliding | rwi_stop_at_pierceable)
{
	ray_hit hit;
	const RayCastResult& result = gAIEnv.pRayCaster->Cast(RayCastRequest(lineseg.start, lineseg.end - lineseg.start,
	                                                                     aiCollisionEntities, filter));
	if (!result || (result[0].dist < 0.0f))
		return false;

	hitPos = result[0].pt;
	return true;
}

//====================================================================
// GetFloorPos
/// returns false if no valid floor found. Checks from upDist above pos to
/// downDist below pos.
//====================================================================
inline bool GetFloorPos(Vec3& floorPos, const Vec3& pos, float upDist, float downDist, float radius, EAICollisionEntities aiCollisionEntities)
{
	// (MATT) This function doesn't have the behaviour you might expect. It only searches up by radius amount, and checks down by
	// upDist + downDist + rad. Might or might not be safe to fix, but this code is being superseded. {2009/07/01}
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	Vec3 delta = Vec3(0, 0, -downDist - upDist);
	Vec3 capStart = pos + Vec3(0, 0, upDist);
	const Vec3 vRad(0, 0, radius);

	if (IntersectSegment(floorPos, Lineseg(pos + vRad, pos - Vec3(0.0f, 0.0f, downDist)), aiCollisionEntities, rwi_stop_at_pierceable | (geom_colltype_player << rwi_colltype_bit)))
		return true;

	if (delta.z > 0.0f)
		return false;

	float hitDist;

	float numSteps = 1.f + downDist * 0.5f;
	float fStepNumInv = 1.f / numSteps;
	Lineseg seg;
	for (float fStep = 0.f; fStep < numSteps; fStep += 1.f)
	{
		float frac1 = fStep * fStepNumInv;
		float frac2 = (fStep + 1.f) * fStepNumInv;
		seg.start = capStart + frac1 * delta;
		seg.end = capStart + frac2 * delta;
		if (IntersectSweptSphere(0, hitDist, seg, radius, aiCollisionEntities, 0, 0, rwi_stop_at_pierceable | (geom_colltype_player << rwi_colltype_bit)))
		{
			floorPos.Set(pos.x, pos.y, seg.start.z - (radius + hitDist));
			return true;
		}
	}
	return false;
}

//===================================================================
// CheckBodyPos
/// Given a floor position (expected to be valid!) checks if a "standard"
/// human body will fit there. Returns true if it will.
//===================================================================
inline bool CheckBodyPos(const Vec3& floorPos, EAICollisionEntities aiCollisionEntities)
{
	Vec3 segStart = floorPos + WalkabilityTorsoBaseOffset + Vec3(0, 0, WalkabilityRadius);
	Vec3 segEnd = floorPos + Vec3(0, 0, WalkabilityTotalHeight - WalkabilityRadius);
	Lineseg torsoSeg(segStart, segEnd);
	return !OverlapCapsule(torsoSeg, WalkabilityRadius, aiCollisionEntities);
}

//===================================================================
// CalcPolygonArea
//===================================================================
template<typename VecContainer, typename T>
T CalcPolygonArea(const VecContainer& pts)
{
	if (pts.size() < 2)
		return T(0);

	T totalCross = T(0);
	const typename VecContainer::const_iterator itEnd = pts.end();
	typename VecContainer::const_iterator it = pts.begin();
	const typename VecContainer::const_iterator firstIt = pts.begin();
	typename VecContainer::const_iterator itNext = it;
	for (; it != itEnd; ++it)
	{
		if (++itNext == itEnd)
			itNext = pts.begin();
		totalCross += (it->x - firstIt->x) * (itNext->y - firstIt->y) - (itNext->x - firstIt->x) * (it->y - firstIt->y);
	}

	return totalCross / T(2);
}

//===================================================================
// IsWoundAnticlockwise
//===================================================================
template<typename VecContainer, typename T>
bool IsWoundAnticlockwise(const VecContainer& pts)
{
	if (pts.size() < 2)
		return true;
	return CalcPolygonArea<VecContainer, T>(pts) > T(0);
}

//===================================================================
// EnsureShapeIsWoundAnticlockwise
//===================================================================
template<typename VecContainer, typename T>
void EnsureShapeIsWoundAnticlockwise(VecContainer& pts)
{
	if (!IsWoundAnticlockwise<VecContainer, T>(pts))
		std::reverse(pts.begin(), pts.end());
}

//===================================================================
// GetFloorRectangleFromOrientedBox
// Returns a rectangle in XY plane from given
//===================================================================
struct SAIRect3
{
	Vec3 center;
	Vec3 axisu, axisv;
	Vec2 min, max;
};

void GetFloorRectangleFromOrientedBox(const Matrix34& tm, const AABB& box, SAIRect3& rect);

//===================================================================
// OverlapLinesegAABB2D
//===================================================================
inline bool OverlapLinesegAABB2D(const Vec3& p0, const Vec3& p1, const AABB& aabb)
{
	Vec3 c = (aabb.min + aabb.max) * 0.5f;
	Vec3 e = aabb.max - c;
	Vec3 m = (p0 + p1) * 0.5f;
	Vec3 d = p1 - m;
	m = m - c;

	// Try world coordinate axes as separating axes
	float adx = fabsf(d.x);
	if (fabsf(m.x) > e.x + adx) return false;
	float ady = fabsf(d.y);
	if (fabsf(m.y) > e.y + ady) return false;

	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis (see text for detail)
	const float EPSILON = 0.000001f;
	adx += EPSILON;
	ady += EPSILON;

	if (fabsf(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) return false;

	// No separating axis found; segment must be overlapping AABB
	return true;
}

void CleanupAICollision();

#endif
