// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "WalkabilityCache.h"
#include "WalkabilityCacheManager.h"
#include "DebugDrawContext.h"

ILINE float cbrt_halley_step(float a, float R)
{
	const float a3 = a * a * a;
	const float b = a * (a3 + R + R) / (a3 + a3 + R);

	return b;
}

float cbrt_fast(float cube)
{
	union intfloat
	{
		size_t i;
		float  f;
	};

	intfloat x;
	x.f = cube;
	x.i = x.i / 3 + 709921077;

	float a = x.f;

	a = cbrt_halley_step(a, cube);
	return cbrt_halley_step(a, cube);
};

WalkabilityCache::WalkabilityCache(tAIObjectID actorID)
	: m_center(ZERO)
	, m_aabb(AABB::RESET)
	, m_entititesHash(0)
	, m_actorID(actorID)
{
	m_floorCache.Reset();
}

void WalkabilityCache::Reset(bool resetFloorCache)
{
	m_aabb.Reset();

	if (resetFloorCache)
		m_floorCache.Reset();

	m_entities.clear();
	m_aabbs.clear();
	m_entititesHash = 0;
}

bool WalkabilityCache::Cache(const AABB& aabb)
{
	const float ExtraBorder = 2.0f;

	// we always calculate a bit extra border and avoid clearing the floor cache if nothing changed inside this border
	if (!m_aabb.ContainsBox(aabb) || ((cbrt_fast(m_aabb.GetVolume()) / cbrt_fast(aabb.GetVolume())) > 2.0f))
	{
		m_aabb = aabb;
		m_aabb.min.z = aabb.min.z - (WalkabilityFloorDownDist - WalkabilityFloorUpDist) - 0.2f;
		m_aabb.max.z = aabb.max.z + WalkabilityTotalHeight + 0.2f;

		m_aabb.Expand(Vec3(ExtraBorder, ExtraBorder, ExtraBorder * 0.25f));
	}

	const size_t capacity = m_entities.capacity();

	m_entities.resize(capacity);
	IPhysicalEntity** entityListPtr = &m_entities.front();

	size_t entityCount = (size_t)gEnv->pPhysicalWorld->GetEntitiesInBox(m_aabb.min, m_aabb.max, entityListPtr,
	                                                                    AICE_ALL | ent_allocate_list, capacity);

	if (entityCount <= capacity)
		m_entities.resize(entityCount);
	else
	{
		assert(entityListPtr != &m_entities[0]);
		memcpy(&m_entities[0], entityListPtr, sizeof(IPhysicalEntity*) * capacity);
		entityCount = capacity;

		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(entityListPtr);
	}

	m_aabbs.resize(entityCount);

	size_t entitiesHash = 0;
	pe_status_pos status;

	const float HashVec3Precision = 0.05f;
	const float InvHashVec3Precision = 1.0f / HashVec3Precision;
	const float HashQuatPrecision = 0.01f;
	const float InvHashQuatPrecision = 1.0f / HashQuatPrecision;

	for (size_t i = 0; i < entityCount; ++i)
	{
		const IPhysicalEntity* entity = m_entities[i];

		if (entity->GetStatus(&status))
		{
			entitiesHash += HashFromUInt((size_t)(UINT_PTR)entity);
			entitiesHash += HashFromVec3(status.pos, HashVec3Precision, InvHashVec3Precision);
			entitiesHash += HashFromQuat(status.q, HashQuatPrecision, InvHashQuatPrecision);

			const Vec3 aabbMin(status.BBox[0]);
			const Vec3 aabbMax(status.BBox[1]);

			// terrain will have a zeroed bbox
			if (!aabbMin.IsZero() && !aabbMax.IsZero())
				m_aabbs[i] = AABB(aabbMin + status.pos, aabbMax + status.pos);
			else
				m_aabbs[i] = m_aabb;
		}
		else
			m_aabbs[i] = AABB(AABB::RESET);
	}

	if (entityCount && !entitiesHash)
		entitiesHash = 0x1337d00d;

	if (entitiesHash != m_entititesHash)
	{
		m_floorCache.Reset();
		m_entititesHash = entitiesHash;

		return true;
	}

	return false;
}

void WalkabilityCache::Draw()
{
	CDebugDrawContext dc;

	dc->DrawAABB(m_aabb, IDENTITY, false, Col_BlueViolet, eBBD_Faceted);
	m_floorCache.Draw(Col_DarkGreen, Col_Red);
}

const AABB& WalkabilityCache::GetAABB() const
{
	return m_aabb;
}

bool WalkabilityCache::FullyContaints(const AABB& aabb) const
{
	return (m_aabb.ContainsBox(aabb));
}

size_t WalkabilityCache::GetOverlapping(const AABB& aabb, Entities& entities) const
{
	size_t entityCount = m_entities.size();
	size_t count = 0;

	for (size_t i = 0; i < entityCount; ++i)
	{
		const AABB entityAABB = m_aabbs[i];
		if (entityAABB.IsIntersectBox(aabb))
		{
			entities.push_back(m_entities[i]);
			++count;
		}
	}

	return count;
}

size_t WalkabilityCache::GetOverlapping(const AABB& aabb, Entities& entities, AABBs& aabbs) const
{
	size_t entityCount = m_entities.size();
	size_t count = 0;

	for (size_t i = 0; i < entityCount; ++i)
	{
		const AABB entityAABB = m_aabbs[i];
		if (entityAABB.IsIntersectBox(aabb))
		{
			entities.push_back(m_entities[i]);
			aabbs.push_back(entityAABB);
			++count;
		}
	}

	return count;
}

bool WalkabilityCache::IsFloorCached(const Vec3& position, Vec3& floor) const
{
	float height;

	if (m_floorCache.GetHeight(position, height))
	{
		floor = Vec3(position.x, position.y, height);

		return true;
	}

	return false;
}

bool WalkabilityCache::FindFloor(const Vec3& position, Vec3& floor)
{
	return FindFloor(position, floor, &m_entities.front(), &m_aabbs.front(), m_entities.size(), m_aabb);
}

bool WalkabilityCache::FindFloor(const Vec3& position, Vec3& floor, IPhysicalEntity** entities, AABB* aabbs,
                                 size_t entityCount, const AABB& enclosingAABB)
{
	if (!entityCount)
		return false;

	if (gAIEnv.pWalkabilityCacheManager->IsFloorCached(m_actorID, position, floor))
		return floor.z < FLT_MAX;

	Vec3 dir = Vec3(0.0f, 0.0f, -(WalkabilityFloorDownDist + WalkabilityFloorUpDist));
	const Vec3 start = m_floorCache.GetCellCenter(position) + Vec3(0, 0, WalkabilityFloorUpDist);
	const Lineseg line(start, start + dir);

	AABB cell = m_floorCache.GetAABB(position);
	cell.min.z -= (WalkabilityFloorDownDist - WalkabilityFloorUpDist);
	cell.max.z += WalkabilityFloorUpDist;

	assert(enclosingAABB.ContainsBox(cell));

	ray_hit hit;
	float height = FLT_MAX;
	float closest = FLT_MAX;
	IPhysicalWorld* const physicalWorld = gEnv->pPhysicalWorld;

	for (size_t i = 0; i < entityCount; ++i)
	{
		const AABB entityAABB = aabbs[i];

		if ((entityAABB.min.x < start.x) && (entityAABB.max.x > start.x) &&
		    (entityAABB.min.y < start.y) && (entityAABB.max.y > start.y) &&
		    physicalWorld->RayTraceEntity(entities[i], start, dir, &hit, 0, geom_colltype_player))
		{
			if (hit.dist < closest)
			{
				closest = hit.dist;
				height = start.z - closest;
			}
		}
	}

	m_floorCache.SetHeight(position, height);

	if (height < FLT_MAX)
	{
		floor = Vec3(position.x, position.y, height);

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

bool WalkabilityCache::CheckWalkability(const Vec3& origin, const Vec3& target, float radius, Vec3* finalFloor, bool* flatFloor) PREFAST_SUPPRESS_WARNING(6262)
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

	float minZ = min(origin.z, target.z) - WalkabilityFloorDownDist - 0.2f;
	float maxZ = max(origin.z, target.z) + TorsoTotalHeight + 0.2f;
	float Zdiff = fabs_tpl(maxZ - minZ);

	Vec3 segmentStart = origin;
	Vec3 segmentStartFloor;
	Vec3 newFloor;

	Vec3 startOBB(ZERO);
	Vec3 endOBB(ZERO);

	Entities segmentEntities;
	AABBs segmentAABBs;
	Entities overlapTorsoEntities;

	for (size_t i = 0; i < segmentCount; ++i)
	{
		segmentAABBs.clear();
		segmentEntities.clear();

		Vec3 segmentEnd = segmentStart + segmentOffset;

		// find all entities for this segment
		AABB enclosingAABB(AABB::RESET);

		enclosingAABB.Add(Vec3(segmentStart.x, segmentStart.y, minZ), radius);
		enclosingAABB.Add(Vec3(segmentEnd.x, segmentEnd.y, maxZ), radius);

		if (!GetOverlapping(enclosingAABB, segmentEntities, segmentAABBs))
			return false; // no floor

		// first segment - get the floor
		if (i == 0)
		{
			if (!FindFloor(segmentStart, segmentStartFloor, &segmentEntities.front(), &segmentAABBs.front(),
			               segmentEntities.size(), enclosingAABB))
				return false;

			newFloor = segmentStartFloor;

			assert(newFloor.IsValid());
			assert(newFloor.z < FLT_MAX);
		}

		Vec3 locationFloor = segmentStartFloor;
		Vec3 checkLocation;

		for (size_t j = 0; (sampleCount > 1) && (j < segmentSampleCount); ++j, --sampleCount)
		{
			checkLocation = locationFloor + sampleOffset;
			checkLocation.z += NextFloorZOffset;

			if (!FindFloor(checkLocation, newFloor, &segmentEntities.front(), &segmentAABBs.front(),
			               segmentEntities.size(), enclosingAABB))
				return false;

			assert(newFloor.IsValid());
			assert(newFloor.z < FLT_MAX);

			float deltaZ = locationFloor.z - newFloor.z;
			if (fabs_tpl(deltaZ) > NextFloorMaxZDiff)
				return false;

			locationFloor = newFloor;

			// check if we need to do the overlap torso check
			if (!startOBB.IsZero())
			{
				// find entities for this segment
				Vec3 base = locationFloor + TorsoUp;

				AABB cylinderAABB(Vec3(base.x - radius, base.y - radius, base.z), Vec3(base.x + radius, base.y + radius, base.z + TorsoTotalHeight - TorsoUp.z));

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
					if (OverlapTorsoSegment(startOBB, endOBB, radius, &overlapTorsoEntities.front(), overlapTorsoEntities.size()))
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
	if (!overlapTorsoEntities.empty())
	{
		if (OverlapTorsoSegment(startOBB, endOBB, radius, &overlapTorsoEntities.front(), overlapTorsoEntities.size()))
			return false;
	}

	if (finalFloor)
		*finalFloor = newFloor;

	if (flatFloor)
		*flatFloor = !floorHeightChanged;

	return true;
}

bool WalkabilityCache::OverlapTorsoSegment(const Vec3& startOBB, const Vec3& endOBB, float radius,
                                           IPhysicalEntity** entities, size_t entityCount)
{
	if (!entityCount)
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
	params.pSkipEnts = entities;
	params.nSkipEnts = -(int)entityCount;
	params.sweepDir = Vec3(0.0f, 0.0f, 0.0f);
	params.pip = &ip;

	// short cut if start equals end
	if (endOBB.IsEquivalent(startOBB))
	{
		primitives::cylinder cylinder;
		cylinder.axis = Vec3(0.0f, 0.0f, 1.0f);
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

		/*
		   if (const bool drawOverlappingAABB = true)
		   {
		   for (size_t i = 0; i < overlapTorsoEntities.size(); ++i)
		   {
		    IPhysicalEntity* entity = overlapTorsoEntities[i];
		    Matrix34 worldTM;

		    pe_status_pos ppos;
		    ppos.pMtx3x4 = &worldTM;

		    if (entity->GetStatus(&ppos))
		    {
		      CDebugDrawContext dc;

		      dc->DrawAABB(AABB(ppos.BBox[0], ppos.BBox[1]), worldTM, false, Col_Red, eBBD_Faceted);
		    }
		   }
		   }
		 */
	}

	if (!result)
	{
		primitives::cylinder cylinderStart;
		cylinderStart.axis = Vec3(0.0f, 0.0f, 1.0f);
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
		cylinderEnd.axis = Vec3(0.0f, 0.0f, 1.0f);
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

size_t WalkabilityCache::GetMemoryUsage() const
{
	return sizeof(m_floorCache) + m_floorCache.GetMemoryUsage();
}
