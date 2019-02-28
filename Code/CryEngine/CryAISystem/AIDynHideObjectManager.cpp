// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIDynHideObjectManager.cpp
   $Id$
   Description: Provides to query hide points around entities which
   are flagged as AI hideable. The manage also caches the objects.

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Mikko Mononen
   - 2 Mar 2009	: Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include "CAISystem.h"
#include <CryCore/StlUtils.h>
#include "AIDynHideObjectManager.h"
#include "DebugDrawContext.h"
#include "AIHash.h"
#include <float.h>

// When the cache is full, items older than this will be purged to free space.
static const float CACHED_ITEM_TIMEOUT = 5.0f;
// Maximum number of new items that will be setup per query.
static const int MAX_NEW_ITEMS_PER_QUERY = 10;
// The cache size.
static const unsigned int HIDEOBJECT_CACHE_SIZE = 250;

//===================================================================
// CAIDynHideObjectManager
//===================================================================
CAIDynHideObjectManager::CAIDynHideObjectManager()
{
	m_cache.resize(HIDEOBJECT_CACHE_SIZE);
	Reset();
}

//===================================================================
// Reset
//===================================================================
void CAIDynHideObjectManager::Reset()
{
	m_cacheFreeList.resize(HIDEOBJECT_CACHE_SIZE);
	for (int i = 0; i < HIDEOBJECT_CACHE_SIZE; ++i)
		m_cacheFreeList[i] = i;
	m_cachedObjects.clear();
}

//===================================================================
// FreeDynamicHideObjectCacheItem
//===================================================================
void CAIDynHideObjectManager::FreeCacheItem(int i)
{
	// Remove from cache association
	DynamicOHideObjectMap::iterator it = m_cachedObjects.find(m_cache[i].id);
	if (it != m_cachedObjects.end())
		m_cachedObjects.erase(it);
	// Add to free list
	m_cacheFreeList.push_back(i);
}

//===================================================================
// GetNewDynamicHideObjectCacheItem
//===================================================================
int CAIDynHideObjectManager::GetNewCacheItem()
{
	if (m_cacheFreeList.empty())
	{
		// Free the old items.
		CTimeValue curTime = GetAISystem()->GetFrameStartTime();
		float maxTime = 0;
		int maxIdx = 0; // If all the times are the same (or even 0!), delete the first item.
		for (int i = 0; i < HIDEOBJECT_CACHE_SIZE; ++i)
		{
			float dt = (curTime - m_cache[i].timeStamp).GetSeconds();
			if (dt > CACHED_ITEM_TIMEOUT)
				FreeCacheItem(i);
			else
			{
				if (dt > maxTime)
				{
					maxTime = dt;
					maxIdx = i;
				}
			}
		}
		// Check if we were able to release some items.
		if (m_cacheFreeList.empty())
			FreeCacheItem(maxIdx);
	}
	int idx = m_cacheFreeList.back();
	m_cacheFreeList.pop_back();

	return idx;
}

//===================================================================
// GetHidePositionsWithinRange
//===================================================================
void CAIDynHideObjectManager::GetHidePositionsWithinRange(std::vector<SDynamicObjectHideSpot>& hideSpots,
                                                          const Vec3& pos, float radius,
                                                          IAISystem::tNavCapMask navCapMask, float passRadius,
                                                          unsigned int lastNavNodeIndex)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	hideSpots.resize(0);

	float radiusSq = square(radius);

	// Get hideable entities from
	SEntityProximityQuery query;
	query.box.min = pos - Vec3(radius, radius, radius);
	query.box.max = pos + Vec3(radius, radius, radius);
	query.nEntityFlags = (uint32)ENTITY_FLAG_AI_HIDEABLE; // Filter by entity flag.
	gEnv->pEntitySystem->QueryProximity(query);

	CTimeValue curTime = GetAISystem()->GetFrameStartTime();

	// Keep track of the new items added per query and limit it.
	int newItemsPerQuery = 0;

	ray_hit hit;

	for (int i = 0; i < query.nCount; ++i)
	{
		IEntity* pEntity = query.pEntities[i];
		if (!pEntity) continue;

		EntityId id = pEntity->GetId();

		const Vec3 entPos = pEntity->GetWorldPos();
		const Quat& entQuat = pEntity->GetRotation();
		if (Distance::Point_PointSq(entPos, pos) > radiusSq)
			continue;

		IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
		if (!pPhysEntity)
			continue;

		pe_status_dynamics status;
		if (pPhysEntity->GetStatus(&status) == 0)
			continue;
		pe_params_bbox params;
		if (pPhysEntity->GetParams(&params) == 0)
			continue;

		// Skip moving entities.
		if (status.v.GetLengthSquared() > square(0.1f) || status.w.GetLengthSquared() > square(0.1f))
			continue;

		unsigned positionHash = GetPositionHashFromEntity(pEntity);

		SCachedDynamicObject* pCached = 0;
		int cachedIdx = -1;
		DynamicOHideObjectMap::iterator it = m_cachedObjects.find(id);
		if (it != m_cachedObjects.end())
		{
			cachedIdx = it->second;
			pCached = &m_cache[cachedIdx];
			if (positionHash == pCached->positionHash)
			{
				// The object has not moved, reuse the last arrangement.
				for (unsigned j = 0, nj = pCached->spots.size(); j < nj; ++j)
					hideSpots.push_back(pCached->spots[j]);
				continue;
			}
		}
		else
		{
			// Limit the amount of checks per query. This will sometimes result some valid
			// locations not to be returned, but will improve the performance.
			newItemsPerQuery++;
			if (newItemsPerQuery > MAX_NEW_ITEMS_PER_QUERY)
				break;
			// Create new object in cache.
			cachedIdx = GetNewCacheItem();
			if (cachedIdx < 0) continue;
			pCached = &m_cache[cachedIdx];
			m_cachedObjects[id] = cachedIdx;
		}

		AIAssert(cachedIdx != -1 && pCached != 0);

		// Init cached item.
		pCached->id = id;
		pCached->positionHash = positionHash;
		pCached->timeStamp = curTime;
		pCached->spots.clear();

		// Note: If the following checks fail, it's ok. In that case the cache also caches entities which
		// canned be used.

		AABB bounds;
		pEntity->GetLocalBounds(bounds);
		static float extraRadius = 0.8f;
		bounds.max += Vec3(extraRadius, extraRadius, extraRadius);
		bounds.min -= Vec3(extraRadius, extraRadius, extraRadius);

		Vec3 extents = bounds.max - bounds.min;
		float maxExtent = max(max(extents.x, extents.y), extents.z);
		static float minUsefulExtent = 0.7f + 2.0f * extraRadius;
		if (maxExtent < minUsefulExtent)
			continue;

		// check the altitude - raycast rather than terrain so it works indoors
		Vec3 midBBox = 0.5f * (params.BBox[0] + params.BBox[1]);
		Vec3 targetPos(midBBox.x, midBBox.y, midBBox.z - 4.0f);
		Vec3 delta = targetPos - midBBox;
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(midBBox, delta, AICE_STATIC, //AICE_ALL,
		                                                rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
			continue;

		targetPos = hit.pt;

		float midAlt = targetPos.z;
		const float minH = midAlt + 0.0f;
		const float maxH = midAlt + 1.5f;
		if (params.BBox[0].z > maxH || params.BBox[1].z < minH)
			continue;

		Vec3 halfExtents = 0.5f * extents;
		Vec3 localMid = 0.5f * (bounds.max + bounds.min);
		Vec3 worldMid = entPos + entQuat * localMid;

		// if extent is bigger than this use a pair of spots
		static float sizeForHideSpotPair = extraRadius + 3.0f;
		// if using a pair then how much to move each back from the edge
		static float hideSpotPairEdgeOffset = extraRadius + 0.5f;
		Vec3 pairDistsFromMid = halfExtents - Vec3(hideSpotPairEdgeOffset, hideSpotPairEdgeOffset, hideSpotPairEdgeOffset);

		const unsigned maxHideSpots = 24;
		SDynamicObjectHideSpot pts[maxHideSpots];

		unsigned nHideSpots = 0;
		if (min(extents.y, extents.z) > minUsefulExtent)
		{
			if (extents.y > sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(halfExtents.x, pairDistsFromMid.y, 0.0f), entQuat * Vec3(-1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(halfExtents.x, -pairDistsFromMid.y, 0.0f), entQuat * Vec3(-1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-halfExtents.x, pairDistsFromMid.y, 0.0f), entQuat * Vec3(1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-halfExtents.x, -pairDistsFromMid.y, 0.0f), entQuat * Vec3(1, 0, 0), id);
			}
			if (extents.z > sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(halfExtents.x, 0.0f, pairDistsFromMid.z), entQuat * Vec3(-1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(halfExtents.x, 0.0f, -pairDistsFromMid.z), entQuat * Vec3(-1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-halfExtents.x, 0.0f, pairDistsFromMid.z), entQuat * Vec3(1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-halfExtents.x, 0.0f, -pairDistsFromMid.z), entQuat * Vec3(1, 0, 0), id);
			}
			if (extents.y <= sizeForHideSpotPair && extents.z <= sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(halfExtents.x, 0, 0), entQuat * Vec3(-1, 0, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-halfExtents.x, 0, 0), entQuat * Vec3(1, 0, 0), id);
			}
		}
		if (min(extents.z, extents.x) > minUsefulExtent)
		{
			if (extents.z > sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, halfExtents.y, pairDistsFromMid.z), entQuat * Vec3(0, -1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, halfExtents.y, -pairDistsFromMid.z), entQuat * Vec3(0, -1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, -halfExtents.y, pairDistsFromMid.z), entQuat * Vec3(0, 1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, -halfExtents.y, -pairDistsFromMid.z), entQuat * Vec3(0, 1, 0), id);
			}
			if (extents.x > sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(pairDistsFromMid.x, halfExtents.y, 0), entQuat * Vec3(0, -1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-pairDistsFromMid.x, halfExtents.y, 0), entQuat * Vec3(0, -1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(pairDistsFromMid.x, -halfExtents.y, 0), entQuat * Vec3(0, 1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-pairDistsFromMid.x, -halfExtents.y, 0), entQuat * Vec3(0, 1, 0), id);
			}
			if (extents.z <= sizeForHideSpotPair && extents.x <= sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, halfExtents.y, 0), entQuat * Vec3(0, -1, 0), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, -halfExtents.y, 0), entQuat * Vec3(0, 1, 0), id);
			}
		}
		if (min(extents.x, extents.y) > minUsefulExtent)
		{
			if (extents.x > sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(pairDistsFromMid.x, 0, halfExtents.z), entQuat * Vec3(0, 0, -1), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-pairDistsFromMid.x, 0, halfExtents.z), entQuat * Vec3(0, 0, -1), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(pairDistsFromMid.x, 0, -halfExtents.z), entQuat * Vec3(0, 0, 1), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(-pairDistsFromMid.x, 0, -halfExtents.z), entQuat * Vec3(0, 0, 1), id);
			}
			if (extents.y > sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, pairDistsFromMid.y, halfExtents.z), entQuat * Vec3(0, 0, -1), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, -pairDistsFromMid.y, halfExtents.z), entQuat * Vec3(0, 0, -1), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, pairDistsFromMid.y, -halfExtents.z), entQuat * Vec3(0, 0, 1), id);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, -pairDistsFromMid.y, -halfExtents.z), entQuat * Vec3(0, 0, 1), id);
			}
			if (extents.x <= sizeForHideSpotPair && extents.y <= sizeForHideSpotPair)
			{
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, 0, halfExtents.z), entQuat * Vec3(0, 0, -1), id);
				PREFAST_SUPPRESS_WARNING(6386);
				pts[nHideSpots++] = SDynamicObjectHideSpot(worldMid + entQuat * Vec3(0, 0, -halfExtents.z), entQuat * Vec3(0, 0, 1), id);
			}
		}

		if (!nHideSpots)
			continue;

		const float maxDirZ = 0.6f;
		for (unsigned j = 0; j < nHideSpots; ++j)
		{
			SDynamicObjectHideSpot& pt = pts[j];
			if (pt.dir.z > maxDirZ || pt.dir.z < -maxDirZ)
				continue;
			float distSq = Distance::Point_PointSq(pt.pos, pos);
			if (distSq > radiusSq)
				continue;
			// raycast to check (a) the HS doesn't go through geometry other than the obstructor
			Vec3 offset = pt.pos - worldMid;
			float offsetLen = offset.NormalizeSafe();
			if (offsetLen < 0.001f)
				continue;
			offset.SetLength(offsetLen + 0.6f);
			if (gEnv->pPhysicalWorld->RayWorldIntersection(worldMid, offset, AICE_ALL,
			                                               rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
			{
				continue;
			}

			pt.nodeIndex = 0;

			pCached->spots.push_back(pt);

			hideSpots.push_back(pt);
		}
	}
}

//===================================================================
// ValidateHideSpotLocation
//===================================================================
void CAIDynHideObjectManager::InvalidateHideSpotLocation(const Vec3& pos, EntityId objectEntId)
{
	DynamicOHideObjectMap::iterator it = m_cachedObjects.find(objectEntId);
	if (it == m_cachedObjects.end())
		return;
	SCachedDynamicObject* pCached = &m_cache[it->second];

	// Check if the position is close enough to one of the cached hide spots and remove that.
	for (unsigned i = 0, ni = pCached->spots.size(); i < ni; ++i)
	{
		SDynamicObjectHideSpot& spot = pCached->spots[i];
		if (Distance::Point_PointSq(spot.pos, pos) < sqr(0.1f))
		{
			pCached->spots[i] = pCached->spots.back();
			pCached->spots.pop_back();
			return;
		}
	}
}

//===================================================================
// ValidateHideSpotLocation
//===================================================================
bool CAIDynHideObjectManager::ValidateHideSpotLocation(const Vec3& pos, const SAIBodyInfo& bi, EntityId objectEntId)
{
	Vec3 stanceSize = bi.stanceSize.GetSize();
	Vec3 colliderSize = bi.colliderSize.GetSize();

	const float padding = 0.2f;

	float capsuleRad = max(colliderSize.x, colliderSize.y) * 0.5f + padding;
	float capsuleMin = stanceSize.z - colliderSize.z + capsuleRad;
	float capsuleMax = colliderSize.z - capsuleRad;
	if (capsuleMax < (capsuleMin + 0.001f))
		capsuleMax = capsuleMin + 0.001f;

	Vec3 groundPos = pos;
	groundPos.z += capsuleMin;
	// Raycast to find ground pos.
	ray_hit hit;
	if (!gEnv->pPhysicalWorld->RayWorldIntersection(groundPos, Vec3(0, 0, -(capsuleMin + 1)), AICE_ALL,
	                                                rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1))
	{
		InvalidateHideSpotLocation(pos, objectEntId);
		return false;
	}
	groundPos = hit.pt;

	// Check if it possible to stand at the object position.
	if (OverlapCapsule(Lineseg(Vec3(groundPos.x, groundPos.y, groundPos.z + capsuleMin),
	                           Vec3(groundPos.x, groundPos.y, groundPos.z + capsuleMax)), capsuleRad, AICE_ALL))
	{
		InvalidateHideSpotLocation(pos, objectEntId);
		return false;
	}
	return true;
}

//===================================================================
// GetPositionHashFromEntity
//===================================================================
unsigned int CAIDynHideObjectManager::GetPositionHashFromEntity(IEntity* pEntity)
{
	return HashFromVec3(pEntity->GetWorldPos(), 0.05f, 1.0f / 0.05f) + HashFromQuat(pEntity->GetWorldRotation(), 0.01f, 1.0f / 0.01f);
}

//===================================================================
// DebugDraw
//===================================================================
void CAIDynHideObjectManager::DebugDraw()
{
	CDebugDrawContext dc;

	CTimeValue curTime = GetAISystem()->GetFrameStartTime();
	// Draw cache
	Vec3 padding(0.1f, 0.1f, 0.1f);
	unsigned int cacheSize = m_cachedObjects.size();
	unsigned int cnt = 0;
	for (DynamicOHideObjectMap::iterator it = m_cachedObjects.begin(), end = m_cachedObjects.end(); it != end; ++it, ++cnt)
	{
		int cachedIdx = it->second;
		SCachedDynamicObject* pCached = &m_cache[cachedIdx];
		float dt = (curTime - pCached->timeStamp).GetSeconds();
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pCached->id);
		if (!pEntity) continue;

		unsigned positionHash = GetPositionHashFromEntity(pEntity);

		bool moving = positionHash != pCached->positionHash;

		AABB bbox;
		pEntity->GetLocalBounds(bbox);
		bbox.min -= padding;
		bbox.max += padding;
		dc->DrawAABB(bbox, pEntity->GetWorldTM(), false,
		             ColorB(255, 255, 255, moving ? 128 : 255), eBBD_Faceted);

		dc->Draw3dLabel(pEntity->GetPos(), 1.2f, "Cached %d/%d\n%.1fs\n%s", cnt + 1, cacheSize, dt, moving ? "MOVING" : "");

		if (!moving)
		{
			for (unsigned i = 0, ni = pCached->spots.size(); i < ni; ++i)
			{
				const SDynamicObjectHideSpot& spot = pCached->spots[i];
				dc->DrawSphere(spot.pos, 0.1f, ColorB(255, 255, 255));
				dc->DrawLine(spot.pos, ColorB(255, 255, 255), spot.pos + spot.dir * 0.5f, ColorB(255, 255, 255, 0), 3.0f);
			}
		}
	}
}
