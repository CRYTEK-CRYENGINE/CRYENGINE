// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "AICollision.h"
#include "CAISystem.h"
#include "Walkability/WalkabilityCacheManager.h"
#include "DebugDrawContext.h"

static EAICollisionEntities aiCollisionEntitiesTable[] =
{
	AICE_STATIC,
	AICE_ALL,
	AICE_ALL_SOFT,
	AICE_DYNAMIC,
	AICE_STATIC_EXCEPT_TERRAIN,
	AICE_ALL_EXCEPT_TERRAIN,
	AICE_ALL_INLUDING_LIVING
};

#ifdef CRYAISYSTEM_DEBUG
unsigned g_CheckWalkabilityCalls;
#endif //CRYAISYSTEM_DEBUG

IPhysicalEntity* g_AIEntitiesInBoxPreAlloc[GetPhysicalEntitiesInBoxMaxResultCount];

// For automatic cleanup of memory allocated by physics
// We could have a static buffer also, at the cost of a little complication - physics memory is still needed if static buffer too small
void PhysicalEntityListAutoPtr::operator=(IPhysicalEntity** pList)
{
	if (m_pList)
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(m_pList);
	m_pList = pList;
}

//====================================================================
// IntersectSweptSphere
// hitPos is optional - may be faster if 0
//====================================================================
bool IntersectSweptSphere(Vec3* hitPos, float& hitDist, const Lineseg& lineseg, float radius, EAICollisionEntities aiCollisionEntities, IPhysicalEntity** pSkipEnts, int nSkipEnts, int geomFlagsAny)
{
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;

	Vec3 dir = lineseg.end - lineseg.start;

	geom_contact* pContact = 0;
	geom_contact** ppContact = hitPos ? &pContact : 0;
	int geomFlagsAll = 0;

	float d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, dir,
	                                                           aiCollisionEntities, ppContact,
	                                                           geomFlagsAll, geomFlagsAny, 0, 0, 0, pSkipEnts, nSkipEnts);

	if (d > 0.0f)
	{
		hitDist = d;
		if (pContact && hitPos)
			*hitPos = pContact->pt;
		return true;
	}
	else
	{
		return false;
	}
}

//====================================================================
// IntersectSweptSphere
//====================================================================
bool IntersectSweptSphere(Vec3* hitPos, float& hitDist, const Lineseg& lineseg, float radius, const std::vector<IPhysicalEntity*>& entities)
{
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;

	Vec3 dir = lineseg.end - lineseg.start;

	ray_hit hit;
	unsigned nEntities = entities.size();
	hitDist = std::numeric_limits<float>::max();
	for (unsigned iEntity = 0; iEntity < nEntities; ++iEntity)
	{
		IPhysicalEntity* pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithBeam(pEntity, lineseg.start, dir, radius, &hit))
		{
			if (hit.dist < hitDist)
			{
				if (hitPos)
					*hitPos = hit.pt;
				hitDist = hit.dist;
			}
		}
	}
	return hitDist < std::numeric_limits<float>::max();
}

//====================================================================
// OverlapCylinder
//====================================================================
bool OverlapCylinder(const Lineseg& lineseg, float radius, const std::vector<IPhysicalEntity*>& entities)
{
	intersection_params ip;
	ip.bStopAtFirstTri = true;
	ip.bNoAreaContacts = true;
	ip.bNoIntersection = 1;
	ip.bNoBorder = true;

	primitives::cylinder cylinderPrim;
	cylinderPrim.center = 0.5f * (lineseg.start + lineseg.end);
	cylinderPrim.axis = lineseg.end - lineseg.start;
	cylinderPrim.hh = 0.5f * cylinderPrim.axis.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
	cylinderPrim.r = radius;

	ray_hit hit;
	unsigned nEntities = entities.size();
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	const Vec3 vZero(ZERO);
	for (unsigned iEntity = 0; iEntity < nEntities; ++iEntity)
	{
		IPhysicalEntity* pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, cylinderPrim.type, &cylinderPrim, vZero, &hit, &ip))
			return true;
	}
	return false;
}

// for finding the start/end positions
const float WalkabilityFloorUpDist = 0.25f;
const float WalkabilityFloorDownDist = 2.0f;
// assume character is this fat
const float WalkabilityRadius = 0.25f;
// radius of the swept sphere down to find the floor
const float WalkabilityDownRadius = 0.06f;
// assume character is this tall
const float WalkabilityTotalHeight = 1.8f;
const float WalkabilityCritterTotalHeight = 0.3f;

const float WalkabilityTorsoOffset = 0.65f;
const float WalkabilityCritterTorsoOffset = 0.15f;
// maximum allowed floor height change from one to the next
const float WalkabilityMaxDeltaZ = 0.6f;
// height of the torso capsule above foot
const Vec3 WalkabilityTorsoBaseOffset(0.0f, 0.0f, WalkabilityMaxDeltaZ);
// Separation between sample points (horizontal)
const float WalkabilitySampleDist = 0.2f;

const Vec3 WalkabilityUp(0.0f, 0.0f, 1.0f);

//typedef std::vector<IPhysicalEntity*> PhysicalEntities;

bool CheckWalkability(const Vec3& origin, const Vec3& target, float radius, const ListPositions& boundary,
                      Vec3* finalFloor, bool* flatFloor, const AABB* boundaryAABB)
{
	if (Overlap::Lineseg_Polygon2D(Lineseg(origin, target), boundary, boundaryAABB))
		return false;

	return CheckWalkability(origin, target, radius, finalFloor, flatFloor);
}

//#include "Puppet.h"

bool OverlapTorsoSegment(const Vec3& startOBB, const Vec3& endOBB, float radius, StaticPhysEntityArray& overlapTorsoEntities)
{
	if (overlapTorsoEntities.empty())
		return false;

	// Marcio: HAX for Crysis2 ticks
	bool isCritter = radius < 0.2f;
	const Vec3 TorsoUp(0.0f, 0.0f, max(radius * 0.65f, isCritter ? WalkabilityCritterTorsoOffset : WalkabilityTorsoOffset));
	const float TorsoTotalHeight = isCritter ? WalkabilityCritterTotalHeight : WalkabilityTotalHeight;

	const float height = (TorsoTotalHeight - TorsoUp.z);

	intersection_params ip;
	ip.bStopAtFirstTri = true;
	ip.bNoAreaContacts = true;
	ip.bNoIntersection = 1;
	ip.bNoBorder = true;
	ip.bThreadSafe = true;

	ray_hit hit;

	IPhysicalWorld::SPWIParams params;
	params.pSkipEnts = &overlapTorsoEntities[0];
	params.nSkipEnts = -(int)overlapTorsoEntities.size();
	params.sweepDir = Vec3(0.f, 0.f, 0.f);
	params.pip = &ip;

	// short cut if start equals end
	if (endOBB.IsEquivalent(startOBB))
	{
		primitives::cylinder cylinder;
		cylinder.axis = WalkabilityUp;
		cylinder.center = Vec3(startOBB.x, startOBB.y, startOBB.z + height * 0.5f);
		cylinder.center += TorsoUp;
		cylinder.hh = height * 0.5f;
		cylinder.r = radius;

		params.itype = primitives::cylinder::type;
		params.pprim = &cylinder;

		bool result = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(params) > 0.0f;

		if (gAIEnv.CVars.DebugCheckWalkability)
		{
			CDebugDrawContext()->DrawCylinder(cylinder.center, cylinder.axis,
			                                  cylinder.r, cylinder.hh * 2.0f, result ? Col_Red : Col_Green);
		}

		return result;
	}

	Vec3 forward = (endOBB - startOBB);
	const float length = forward.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
	Vec3 right = forward.Cross(Vec3Constants<float>::fVec3_OneZ).GetNormalizedSafe(Vec3Constants<float>::fVec3_OneX);
	Vec3 up = right.Cross(forward);

	primitives::box physBox;
	physBox.center = 0.5f * (startOBB + endOBB);
	physBox.center += TorsoUp;
	physBox.center.z += 0.5f * height;
	physBox.size.Set(radius, (0.5f * length), 0.5f * height);
	physBox.bOriented = 1;
	physBox.Basis.SetFromVectors(right, forward, up);
	physBox.Basis.Transpose();

	// test obb for plane path
	params.itype = primitives::box::type;
	params.pprim = &physBox;

	bool result = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(params) > 0.0f;

	if (gAIEnv.CVars.DebugCheckWalkability)
	{
		OBB obb(OBB::CreateOBB(physBox.Basis.GetTransposed(), physBox.size, ZERO));

		CDebugDrawContext()->DrawOBB(obb, Matrix34::CreateTranslationMat(physBox.center), true, result ? Col_Red : Col_Green,
		                             eBBD_Faceted);
	}

	if (!result)
	{
		primitives::cylinder cylinderStart;
		cylinderStart.axis = WalkabilityUp;
		cylinderStart.center = Vec3(startOBB.x, startOBB.y, startOBB.z + height * 0.5f);
		cylinderStart.center += TorsoUp;
		cylinderStart.hh = height * 0.5f;
		cylinderStart.r = radius;

		params.itype = primitives::cylinder::type;
		params.pprim = &cylinderStart;
		result |= gEnv->pPhysicalWorld->PrimitiveWorldIntersection(params) > 0.0f;

		if (gAIEnv.CVars.DebugCheckWalkability)
		{
			CDebugDrawContext()->DrawCylinder(cylinderStart.center, cylinderStart.axis,
			                                  cylinderStart.r, cylinderStart.hh * 2.0f, result ? Col_Red : Col_Green);
		}
	}

	if (!result)
	{
		primitives::cylinder cylinderEnd;
		cylinderEnd.axis = WalkabilityUp;
		cylinderEnd.center = Vec3(endOBB.x, endOBB.y, endOBB.z + height * 0.5f);
		cylinderEnd.center += TorsoUp;
		cylinderEnd.hh = height * 0.5f;
		cylinderEnd.r = radius;

		params.pprim = &cylinderEnd;
		result |= gEnv->pPhysicalWorld->PrimitiveWorldIntersection(params) > 0.0f;

		if (gAIEnv.CVars.DebugCheckWalkability)
		{
			CDebugDrawContext()->DrawCylinder(cylinderEnd.center, cylinderEnd.axis,
			                                  cylinderEnd.r, cylinderEnd.hh * 2.0f, result ? Col_Red : Col_Green);
		}
	}

	return result;
}

//====================================================================
// CheckWalkability
//====================================================================
#pragma warning (push)
#pragma warning (disable: 6262)
bool CheckWalkability(const Vec3& origin, const Vec3& target, float radius, Vec3* finalFloor, bool* flatFloor)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	bool floorHeightChanged = false;

	Vec3 direction2D(Vec2(target - origin));
	float distanceSq = direction2D.GetLengthSquared2D();
	if (distanceSq < sqr(0.125f))
	{
		if (finalFloor)
			*finalFloor = origin;

		if (flatFloor)
			*flatFloor = true;

		return true;
	}

	// Marcio: HAX for Crysis2 ticks
	bool isCritter = radius < 0.2f;
	const Vec3 TorsoUp(0.0f, 0.0f, max(radius * 0.65f, isCritter ? WalkabilityCritterTorsoOffset : WalkabilityTorsoOffset));
	const float TorsoTotalHeight = isCritter ? WalkabilityCritterTotalHeight : WalkabilityTotalHeight;
	const float NextFloorMaxZDiff = isCritter ? 0.25f : 0.75f;
	const float NextFloorZOffset = isCritter ? 0.25f : max(radius, 0.5f);

	float distance = direction2D.NormalizeSafe() + radius;

	const float OptimalSampleOffset = max(0.225f, radius * 0.85f);
	const float OptimalSegmentLength = gAIEnv.CVars.CheckWalkabilityOptimalSectionLength;
	const size_t OptimalSegmentSampleCount = std::max<size_t>(1, (size_t)(OptimalSegmentLength / OptimalSampleOffset));

	size_t sampleCount = (size_t)(distance / OptimalSampleOffset);
	const float sampleOffsetAmount = (sampleCount > 1) ? (distance / (float)sampleCount) : distance;
	const Vec3 sampleOffset = direction2D * sampleOffsetAmount;

	const float segmentLength = OptimalSegmentSampleCount * sampleOffsetAmount;
	const size_t segmentCount = 1 + (size_t)(distance / segmentLength);
	const Vec3 segmentOffset = direction2D * segmentLength;
	const size_t segmentSampleCount = OptimalSegmentSampleCount;

	float minZ = min(origin.z, target.z) - WalkabilityFloorDownDist;
	float maxZ = max(origin.z, target.z) + TorsoTotalHeight;
	float Zdiff = fabs_tpl(maxZ - minZ);

	Vec3 segmentStart = origin;
	Vec3 segmentStartFloor;
	Vec3 newFloor;

	Vec3 startOBB(ZERO);
	Vec3 endOBB(ZERO);

	StaticAABBArray segmentAABBs;
	StaticPhysEntityArray segmentEntities;
	StaticPhysEntityArray overlapTorsoEntities;

	for (size_t i = 0; i < segmentCount; ++i)
	{
		segmentAABBs.clear();
		segmentEntities.clear();

		Vec3 segmentEnd = segmentStart + segmentOffset;

		// find all entities for this segment
		AABB enclosingAABB(AABB::RESET);

		enclosingAABB.Add(Vec3(segmentStart.x, segmentStart.y, minZ), radius);
		enclosingAABB.Add(Vec3(segmentEnd.x, segmentEnd.y, maxZ), radius);

		size_t entityCount = GetPhysicalEntitiesInBox(enclosingAABB.min, enclosingAABB.max, segmentEntities, AICE_ALL);

		if (!entityCount) // no floor
			return false;

		segmentAABBs.resize(entityCount);

		pe_status_pos status;

		for (size_t j = 0; j < entityCount; ++j)
		{
			IPhysicalEntity* entity = segmentEntities[j];

			if (entity->GetStatus(&status))
			{
				const Vec3 aabbMin(status.BBox[0]);
				const Vec3 aabbMax(status.BBox[1]);

				// terrain will have a zeroed bbox
				if (!aabbMin.IsZero() || !aabbMax.IsZero())
					segmentAABBs[j] = AABB(aabbMin + status.pos, aabbMax + status.pos);
				else
					segmentAABBs[j] = enclosingAABB;
			}
			else
				segmentAABBs[j] = AABB(AABB::RESET);
		}

		// first segment - get the floor
		if (i == 0)
		{
			if (!FindFloor(segmentStart, segmentEntities, segmentAABBs, segmentStartFloor))
				return false;

			newFloor = segmentStartFloor;
		}

		Vec3 locationFloor = segmentStartFloor;
		Vec3 checkLocation;

		for (size_t j = 0; (sampleCount > 1) && (j < segmentSampleCount); ++j, --sampleCount)
		{
			checkLocation = locationFloor + sampleOffset;
			checkLocation.z += NextFloorZOffset;

			if (!FindFloor(checkLocation, segmentEntities, segmentAABBs, newFloor))
				return false;

			assert(newFloor.IsValid());

			float deltaZ = locationFloor.z - newFloor.z;
			if (fabs_tpl(deltaZ) > NextFloorMaxZDiff)
				return false;

			locationFloor = newFloor;

			// check if we need to do the overlap torso check
			if (!startOBB.IsZero())
			{
				// find entities for this segment

				Vec3 base = locationFloor + TorsoUp;

				AABB cylinderAABB(Vec3(base.x - radius, base.y - radius, base.z),
				                  Vec3(base.x + radius, base.y + radius, base.z + TorsoTotalHeight - TorsoUp.z));

				for (size_t k = 0; k < (size_t)segmentAABBs.size(); ++k)
				{
					if (segmentAABBs[k].IsIntersectBox(cylinderAABB))
						overlapTorsoEntities.push_back(segmentEntities[k]);
				}

				// remove duplicate entries
				std::sort(overlapTorsoEntities.begin(), overlapTorsoEntities.end());
				overlapTorsoEntities.erase(std::unique(overlapTorsoEntities.begin(), overlapTorsoEntities.end()),
				                           overlapTorsoEntities.end());

				// check for planar floor, TODO add code for ramps and a like
				if (fabs_tpl(startOBB.z - locationFloor.z) > 0.005f)
				{
					floorHeightChanged = true;
					if (OverlapTorsoSegment(startOBB, endOBB, radius, overlapTorsoEntities))
						return false;

					overlapTorsoEntities.clear();
					startOBB = locationFloor;
				}
			}
			else
				startOBB = locationFloor;

			endOBB = locationFloor;
		}

		segmentStart = newFloor;
		segmentStartFloor = newFloor;
	}

	if (fabs_tpl(target.z - newFloor.z) > WalkabilityFloorDownDist + WalkabilityFloorUpDist)
		return false;

	// check the last segment also
	if (OverlapTorsoSegment(startOBB, endOBB, radius, overlapTorsoEntities))
		return false;

	if (finalFloor)
		*finalFloor = newFloor;

	if (flatFloor)
		*flatFloor = !floorHeightChanged;

	return true;
}
#pragma warning (pop)

//====================================================================
// CheckWalkability
//====================================================================
#pragma warning (push)
#pragma warning (disable: 6262)
bool CheckWalkability(const Vec3& origin, const Vec3& target, float radius,
                      const StaticPhysEntityArray& entities, const StaticAABBArray& aabbs, Vec3* finalFloor, bool* flatFloor)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	bool floorHeightChanged = false;

	Vec3 direction2D(Vec2(target - origin));
	float distanceSq = direction2D.GetLengthSquared2D();
	if (distanceSq < sqr(0.125f))
	{
		if (finalFloor)
			*finalFloor = origin;

		if (flatFloor)
			*flatFloor = true;

		return true;
	}

	// Marcio: HAX for Crysis2 ticks
	bool isCritter = radius < 0.2f;
	const Vec3 TorsoUp(0.0f, 0.0f, max(radius * 0.65f, isCritter ? WalkabilityCritterTorsoOffset : WalkabilityTorsoOffset));
	const float TorsoTotalHeight = isCritter ? WalkabilityCritterTotalHeight : WalkabilityTotalHeight;
	const float NextFloorMaxZDiff = isCritter ? 0.25f : 0.75f;
	const float NextFloorZOffset = isCritter ? 0.25f : max(radius, 0.5f);

	float distance = direction2D.NormalizeSafe() + radius;

	const float OptimalSampleOffset = max(0.225f, radius * 0.85f);
	const float OptimalSegmentLength = gAIEnv.CVars.CheckWalkabilityOptimalSectionLength;
	const size_t OptimalSegmentSampleCount = std::max<size_t>(1, (size_t)(OptimalSegmentLength / OptimalSampleOffset));

	size_t sampleCount = (size_t)(distance / OptimalSampleOffset);
	const float sampleOffsetAmount = (sampleCount > 1) ? (distance / (float)sampleCount) : distance;
	const Vec3 sampleOffset = direction2D * sampleOffsetAmount;

	const float segmentLength = OptimalSegmentSampleCount * sampleOffsetAmount;
	const size_t segmentCount = 1 + (size_t)(distance / segmentLength);
	const Vec3 segmentOffset = direction2D * segmentLength;
	const size_t segmentSampleCount = OptimalSegmentSampleCount;

	float minZ = min(origin.z, target.z) - WalkabilityFloorDownDist;
	float maxZ = max(origin.z, target.z) + TorsoTotalHeight;
	float Zdiff = fabs_tpl(maxZ - minZ);

	Vec3 segmentStart = origin;
	Vec3 segmentStartFloor;
	Vec3 newFloor;

	Vec3 startOBB(ZERO);
	Vec3 endOBB(ZERO);

	StaticAABBArray segmentAABBs;
	StaticPhysEntityArray segmentEntities;
	StaticPhysEntityArray overlapTorsoEntities;

	for (size_t i = 0; i < segmentCount; ++i)
	{
		segmentAABBs.clear();
		segmentEntities.clear();

		Vec3 segmentEnd = segmentStart + segmentOffset;

		// find all entities for this segment
		AABB enclosingAABB(AABB::RESET);

		enclosingAABB.Add(Vec3(segmentStart.x, segmentStart.y, minZ), radius);
		enclosingAABB.Add(Vec3(segmentEnd.x, segmentEnd.y, maxZ), radius);

		for (size_t j = 0; j < (size_t)aabbs.size(); ++j)
		{
			if (enclosingAABB.IsIntersectBox(aabbs[j]))
			{
				segmentAABBs.push_back(aabbs[j]);
				segmentEntities.push_back(entities[j]);
			}
		}

		if (segmentEntities.empty()) // no floor
			return false;

		// first segment - get the floor
		if (i == 0)
		{
			if (!FindFloor(segmentStart, segmentEntities, segmentAABBs, segmentStartFloor))
				return false;

			newFloor = segmentStartFloor;
		}

		Vec3 locationFloor = segmentStartFloor;
		Vec3 checkLocation;

		for (size_t j = 0; (sampleCount > 1) && (j < segmentSampleCount); ++j, --sampleCount)
		{
			checkLocation = locationFloor + sampleOffset;
			checkLocation.z += NextFloorZOffset;

			if (!FindFloor(checkLocation, segmentEntities, segmentAABBs, newFloor))
				return false;

			assert(newFloor.IsValid());

			float deltaZ = locationFloor.z - newFloor.z;
			if (fabs_tpl(deltaZ) > NextFloorMaxZDiff)
				return false;

			locationFloor = newFloor;

			// check if we need to do the overlap torso check
			if (!startOBB.IsZero())
			{
				// find entities for this segment
				Vec3 base = locationFloor + TorsoUp;

				AABB cylinderAABB(Vec3(base.x - radius, base.y - radius, base.z),
				                  Vec3(base.x + radius, base.y + radius, base.z + TorsoTotalHeight - TorsoUp.z));

				for (size_t k = 0; k < (size_t)segmentAABBs.size(); ++k)
				{
					if (segmentAABBs[k].IsIntersectBox(cylinderAABB))
						overlapTorsoEntities.push_back(segmentEntities[k]);
				}

				// remove duplicate entries
				std::sort(overlapTorsoEntities.begin(), overlapTorsoEntities.end());
				overlapTorsoEntities.erase(std::unique(overlapTorsoEntities.begin(), overlapTorsoEntities.end()),
				                           overlapTorsoEntities.end());

				// check for planar floor, TODO add code for ramps and a like
				if (fabs_tpl(startOBB.z - locationFloor.z) > 0.005f)
				{
					floorHeightChanged = true;
					if (OverlapTorsoSegment(startOBB, endOBB, radius, overlapTorsoEntities))
						return false;

					overlapTorsoEntities.clear();
					startOBB = locationFloor;
				}
			}
			else
				startOBB = locationFloor;

			endOBB = locationFloor;
		}

		segmentStart = newFloor;
		segmentStartFloor = newFloor;
	}

	if (fabs_tpl(target.z - newFloor.z) > WalkabilityFloorDownDist + WalkabilityFloorUpDist)
		return false;

	// check the last segment also
	if (OverlapTorsoSegment(startOBB, endOBB, radius, overlapTorsoEntities))
		return false;

	if (finalFloor)
		*finalFloor = newFloor;

	if (flatFloor)
		*flatFloor = !floorHeightChanged;

	return true;
}
#pragma warning (pop)

bool CheckWalkabilitySimple(/*Vec3 from, Vec3 to,*/ SWalkPosition fromPos, SWalkPosition toPos, float radius, EAICollisionEntities aiCollisionEntities)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	const Vec3 from(fromPos.m_pos);
	const Vec3 to(toPos.m_pos);

	Vec3 fromFloor;
	Vec3 toFloor;

	if (fromPos.m_isFloor)
		fromFloor = from;
	else if (!gAIEnv.pWalkabilityCacheManager->FindFloor(0, from, fromFloor))
		return false;

	if (toPos.m_isFloor)
		toFloor = to;
	else if (!gAIEnv.pWalkabilityCacheManager->FindFloor(0, to, toFloor))
		return false;

	// Marcio: HAX for Crysis2 ticks
	bool isCritter = radius < 0.2f;
	const Vec3 TorsoUp(0.0f, 0.0f, max(radius * 0.65f, isCritter ? WalkabilityCritterTorsoOffset : WalkabilityTorsoOffset));
	const float TorsoTotalHeight = isCritter ? WalkabilityCritterTotalHeight : WalkabilityTotalHeight;

	const float checkRadius = radius - 0.0001f;
	const Vec3 horWalkDelta = toFloor - fromFloor;
	const float horWalkDistSq = horWalkDelta.len2();

	if (horWalkDistSq > 0.001f)
	{
		const float horWalkDist = sqrt_tpl(horWalkDistSq);
		const Vec3 horWalkDir = horWalkDelta / horWalkDist;
		const float boxHeight = TorsoTotalHeight - TorsoUp.z;

		primitives::box boxPrim;
		boxPrim.center = 0.5f * (fromFloor + toFloor);
		boxPrim.center.z += TorsoUp.z + 0.5f * boxHeight;
		boxPrim.size.Set(checkRadius, 0.5f * horWalkDist, boxHeight * 0.5f);
		boxPrim.bOriented = 1;

		const Vec3 right = horWalkDir.Cross(Vec3Constants<float>::fVec3_OneZ).GetNormalizedSafe(Vec3Constants<float>::fVec3_OneY);
		const Vec3 up = right.Cross(horWalkDir);

		boxPrim.Basis.SetFromVectors(right, horWalkDir, up);
		boxPrim.Basis.Transpose();

		//[Alexey] Because of return false between CreatePrimitive and Release we had leaks here!!!
		//_smart_ptr<IGeometry> box = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::box::type,&boxPrim);
		//box->Release();
		// NOTE Sep 7, 2007: <pvl> made static after SetData() interface became
		// available, to avoid per-frame allocations
		//static IGeometry * box = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::box::type,&boxPrim);
		// Changed to get it from the CAISytem so it can be released at levelUnload.
		IGeometry*& walkabilityGeometryBox = GetAISystem()->m_walkabilityGeometryBox;
		if (!walkabilityGeometryBox)
			walkabilityGeometryBox = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::box::type, &boxPrim);
		walkabilityGeometryBox->SetData(&boxPrim);

		pe_status_nparts snp;
		pe_status_pos sp;
		geom_world_data gwd;
		intersection_params ip;
		ip.bStopAtFirstTri = true;
		ip.bNoAreaContacts = true;
		ip.bNoBorder = true;
		ip.bNoIntersection = 1;
		ip.bThreadSafe = true;

		bool result = (gEnv->pPhysicalWorld->PrimitiveWorldIntersection(primitives::box::type, &boxPrim, Vec3(ZERO), aiCollisionEntities,
		                                                                0, 0, geom_colltype0 | geom_colltype_player, &ip) <= 0.0f);

#ifdef DBG_WALK
		// debug
		OBB obb;
		obb.SetOBB(boxPrim.Basis.GetTransposed(), boxPrim.size, Vec3(ZERO));
		GetAISystem()->AddDebugBox(boxPrim.center, obb, 128, 0, 0, 5.0f);
#endif
		return result;
	}

	return true;
}

//====================================================================
// FindFloor
//====================================================================
bool FindFloor(const Vec3& position, Vec3& floor)
{
	const Vec3 dir = Vec3(0.0f, 0.0f, -(WalkabilityFloorDownDist + WalkabilityFloorUpDist));
	const Vec3 start = position + Vec3(0, 0, WalkabilityFloorUpDist);

	const RayCastResult& result = gAIEnv.pRayCaster->Cast(RayCastRequest(start, dir, AICE_ALL,
	                                                                     rwi_stop_at_pierceable | rwi_colltype_any(geom_colltype_player)));

	if (!result || (result[0].dist < 0.0f))
	{
		if (gAIEnv.CVars.DebugCheckWalkability)
		{
			CDebugDrawContext dc;

			dc->DrawLine(start, Col_Red, start + dir, Col_Red);
			dc->DrawCone(start, Vec3(0.0f, 0.0f, -1.0f), 0.175f, 0.45f, Col_Red);
		}

		return false;
	}

	floor = Vec3(start.x, start.y, start.z - result[0].dist);

	if (gAIEnv.CVars.DebugCheckWalkability)
	{
		CDebugDrawContext dc;

		dc->DrawLine(start, Col_Green, start + dir, Col_Green);
		dc->DrawCone(start, Vec3(0.0f, 0.0f, -1.0f), 0.125f, 0.25f, Col_Green);
		dc->DrawCone(floor + Vec3(0.0f, 0.0f, 0.35f), Vec3(0.0f, 0.0f, -1.0f), 0.125f, 0.35f, Col_SteelBlue);
	}

	return true;
}

//====================================================================
// FindFloor
//====================================================================
bool FindFloor(const Vec3& position, const StaticPhysEntityArray& entities, const StaticAABBArray& aabbs, Vec3& floor)
{
	Vec3 dir = Vec3(0.0f, 0.0f, -(WalkabilityFloorDownDist + WalkabilityFloorUpDist));
	const Vec3 start = position + Vec3(0, 0, WalkabilityFloorUpDist);
	const Lineseg line(start, start + dir);
	IPhysicalWorld* const physicalWorld = gEnv->pPhysicalWorld;

	ray_hit hit;
	float closest = FLT_MAX;

	size_t entityCount = (size_t)aabbs.size();
	for (size_t i = 0; i < entityCount; ++i)
	{
		const AABB& aabb = aabbs[i];

		if ((aabb.min.x <= start.x) && (aabb.max.x >= start.x) &&
		    (aabb.min.y <= start.y) && (aabb.max.y >= start.y) &&
		    physicalWorld->RayTraceEntity(entities[i], start, dir, &hit, 0, geom_colltype_player))
		{
			if (hit.dist < closest)
				closest = hit.dist;
		}
	}

	if (closest < FLT_MAX)
	{
		floor = Vec3(position.x, position.y, start.z - closest);

		if (gAIEnv.CVars.DebugCheckWalkability)
		{
			CDebugDrawContext dc;

			dc->DrawLine(line.start, Col_Green, line.end, Col_Green);
			dc->DrawCone(line.start, Vec3(0.0f, 0.0f, -1.0f), 0.125f, 0.25f, Col_Green);
			dc->DrawCone(floor + Vec3(0.0f, 0.0f, 0.35f), Vec3(0.0f, 0.0f, -1.0f), 0.125f, 0.35f, Col_SteelBlue);
		}

		return true;
	}

	if (gAIEnv.CVars.DebugCheckWalkability)
	{
		CDebugDrawContext dc;

		dc->DrawLine(line.start, Col_Red, line.end, Col_Red);
		dc->DrawCone(line.start, Vec3(0.0f, 0.0f, -1.0f), 0.175f, 0.45f, Col_Red);
	}

	return false;
}

//===================================================================
// IsLeft: tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2 on the line
//            <0 for P2 right of the line
//===================================================================
inline float IsLeft(Vec3 P0, Vec3 P1, const Vec3& P2)
{
	bool swap = false;
	if (P0.x < P1.x)
		swap = true;
	else if (P0.x == P1.x && P0.y < P1.y)
		swap = true;

	if (swap)
		std::swap(P0, P1);

	float res = (P1.x - P0.x) * (P2.y - P0.y) - (P2.x - P0.x) * (P1.y - P0.y);
	const float tol = 0.0000f;
	if (res > tol || res < -tol)
		return swap ? -res : res;
	else
		return 0.0f;
}

inline bool ptEqual(const Vec3& lhs, const Vec3& rhs)
{
	const float tol = 0.01f;
	return (fabs(lhs.x - rhs.x) < tol) && (fabs(lhs.y - rhs.y) < tol);
}

#if 0
struct SPointSorter
{
	SPointSorter(const Vec3& pt) : pt(pt) {}
	bool operator()(const Vec3& lhs, const Vec3& rhs)
	{
		float isLeft = IsLeft(pt, lhs, rhs);
		if (isLeft > 0.0f)
			return true;
		else if (isLeft < 0.0f)
			return false;
		else
			return (lhs - pt).GetLengthSquared2D() < (rhs - pt).GetLengthSquared2D();
	}
	const Vec3 pt;
};

//===================================================================
// ConvexHull2D
// Implements Graham's scan
//===================================================================
void ConvexHull2DGraham(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	const unsigned nPtsIn = ptsIn.size();
	if (nPtsIn < 3)
	{
		ptsOut = ptsIn;
		return;
	}
	unsigned iBotRight = 0;
	for (unsigned iPt = 1; iPt < nPtsIn; ++iPt)
	{
		if (ptsIn[iPt].y < ptsIn[iBotRight].y)
			iBotRight = iPt;
		else if (ptsIn[iPt].y == ptsIn[iBotRight].y && ptsIn[iPt].x < ptsIn[iBotRight].x)
			iBotRight = iPt;
	}

	static std::vector<Vec3> ptsSorted; // avoid memory allocation
	ptsSorted.assign(ptsIn.begin(), ptsIn.end());

	std::swap(ptsSorted[0], ptsSorted[iBotRight]);
	{
		FRAME_PROFILER("SORT Graham", gEnv->pSystem, PROFILE_AI)
		std::sort(ptsSorted.begin() + 1, ptsSorted.end(), SPointSorter(ptsSorted[0]));
	}
	ptsSorted.erase(std::unique(ptsSorted.begin(), ptsSorted.end(), ptEqual), ptsSorted.end());

	const unsigned nPtsSorted = ptsSorted.size();
	if (nPtsSorted < 3)
	{
		ptsOut = ptsSorted;
		return;
	}

	ptsOut.resize(0);
	ptsOut.push_back(ptsSorted[0]);
	ptsOut.push_back(ptsSorted[1]);
	unsigned int i = 2;
	while (i < nPtsSorted)
	{
		if (ptsOut.size() <= 1)
		{
			AIWarning("Badness in ConvexHull2D");
			AILogComment("i = %d ptsIn = ", i);
			for (unsigned j = 0; j < ptsIn.size(); ++j)
				AILogComment("%6.3f, %6.3f, %6.3f", ptsIn[j].x, ptsIn[j].y, ptsIn[j].z);
			AILogComment("ptsSorted = ");
			for (unsigned j = 0; j < ptsSorted.size(); ++j)
				AILogComment("%6.3f, %6.3f, %6.3f", ptsSorted[j].x, ptsSorted[j].y, ptsSorted[j].z);
			ptsOut.resize(0);
			return;
		}
		const Vec3& pt1 = ptsOut[ptsOut.size() - 1];
		const Vec3& pt2 = ptsOut[ptsOut.size() - 2];
		const Vec3& p = ptsSorted[i];
		float isLeft = IsLeft(pt2, pt1, p);
		if (isLeft > 0.0f)
		{
			ptsOut.push_back(p);
			++i;
		}
		else if (isLeft < 0.0f)
		{
			ptsOut.pop_back();
		}
		else
		{
			ptsOut.pop_back();
			ptsOut.push_back(p);
			++i;
		}
	}
}
#endif

inline float IsLeftAndrew(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
	return (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
}

inline bool PointSorterAndrew(const Vec3& lhs, const Vec3& rhs)
{
	if (lhs.x < rhs.x) return true;
	if (lhs.x > rhs.x) return false;
	return lhs.y < rhs.y;
}

//===================================================================
// ConvexHull2D
// Implements Andrew's algorithm
//
// Copyright 2001, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
//===================================================================
static std::vector<Vec3> ConvexHull2DAndrewTemp;

void ConvexHull2DAndrew(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	const int n = (int)ptsIn.size();
	if (n < 3)
	{
		ptsOut = ptsIn;
		return;
	}

	std::vector<Vec3>& P = ConvexHull2DAndrewTemp;
	P = ptsIn;

	{
		FRAME_PROFILER("SORT Andrew", gEnv->pSystem, PROFILE_AI)
		std::sort(P.begin(), P.end(), PointSorterAndrew);
	}

	// the output array ptsOut[] will be used as the stack
	int i;

	ptsOut.clear();
	ptsOut.reserve(P.size());

	// Get the indices of points with min x-coord and min|max y-coord
	int minmin = 0, minmax;
	float xmin = P[0].x;
	for (i = 1; i < n; i++)
		if (P[i].x != xmin)
			break;

	minmax = i - 1;
	if (minmax == n - 1)
	{
		// degenerate case: all x-coords == xmin
		ptsOut.push_back(P[minmin]);
		if (P[minmax].y != P[minmin].y) // a nontrivial segment
			ptsOut.push_back(P[minmax]);
		ptsOut.push_back(P[minmin]);           // add polygon endpoint
		return;
	}

	// Get the indices of points with max x-coord and min|max y-coord
	int maxmin, maxmax = n - 1;
	float xmax = P[n - 1].x;
	for (i = n - 2; i >= 0; i--)
		if (P[i].x != xmax) break;
	maxmin = i + 1;

	// Compute the lower hull on the stack H
	ptsOut.push_back(P[minmin]);      // push minmin point onto stack
	i = minmax;
	while (++i <= maxmin)
	{
		// the lower line joins P[minmin] with P[maxmin]
		if (IsLeftAndrew(P[minmin], P[maxmin], P[i]) >= 0 && i < maxmin)
			continue;          // ignore P[i] above or on the lower line

		while ((int)ptsOut.size() > 1) // there are at least 2 points on the stack
		{
			// test if P[i] is left of the line at the stack top
			if (IsLeftAndrew(ptsOut[ptsOut.size() - 2], ptsOut.back(), P[i]) > 0)
				break;         // P[i] is a new hull vertex
			else
				ptsOut.pop_back(); // pop top point off stack
		}
		ptsOut.push_back(P[i]);       // push P[i] onto stack
	}

	// Next, compute the upper hull on the stack H above the bottom hull
	if (maxmax != maxmin)             // if distinct xmax points
		ptsOut.push_back(P[maxmax]);    // push maxmax point onto stack
	int bot = (int)ptsOut.size() - 1; // the bottom point of the upper hull stack
	i = maxmin;
	while (--i >= minmax)
	{
		// the upper line joins P[maxmax] with P[minmax]
		if (IsLeftAndrew(P[maxmax], P[minmax], P[i]) >= 0 && i > minmax)
			continue;          // ignore P[i] below or on the upper line

		while ((int)ptsOut.size() > bot + 1)    // at least 2 points on the upper stack
		{
			// test if P[i] is left of the line at the stack top
			if (IsLeftAndrew(ptsOut[ptsOut.size() - 2], ptsOut.back(), P[i]) > 0)
				break;         // P[i] is a new hull vertex
			else
				ptsOut.pop_back();         // pop top po2int off stack
		}
		ptsOut.push_back(P[i]);       // push P[i] onto stack
	}
	if (minmax != minmin)
		ptsOut.push_back(P[minmin]);  // push joining endpoint onto stack
	if (!ptsOut.empty() && ptEqual(ptsOut.front(), ptsOut.back()))
		ptsOut.pop_back();
}

//===================================================================
// GetFloorRectangleFromOrientedBox
//===================================================================
void GetFloorRectangleFromOrientedBox(const Matrix34& tm, const AABB& box, SAIRect3& rect)
{
	Matrix33 tmRot(tm);
	tmRot.Transpose();

	Vec3 corners[8];
	SetAABBCornerPoints(box, corners);

	rect.center = tm.GetTranslation();

	rect.axisu = tm.GetColumn1();
	rect.axisu.z = 0;
	rect.axisu.Normalize();
	rect.axisv.Set(rect.axisu.y, -rect.axisu.x, 0);

	Vec3 tu = tmRot.TransformVector(rect.axisu);
	Vec3 tv = tmRot.TransformVector(rect.axisv);

	rect.min.x = FLT_MAX;
	rect.min.y = FLT_MAX;
	rect.max.x = -FLT_MAX;
	rect.max.y = -FLT_MAX;

	for (unsigned i = 0; i < 8; ++i)
	{
		float du = tu.Dot(corners[i]);
		float dv = tv.Dot(corners[i]);
		rect.min.x = min(rect.min.x, du);
		rect.max.x = max(rect.max.x, du);
		rect.min.y = min(rect.min.y, dv);
		rect.max.y = max(rect.max.y, dv);
	}
}

void CleanupAICollision()
{
	stl::free_container(ConvexHull2DAndrewTemp);
}
