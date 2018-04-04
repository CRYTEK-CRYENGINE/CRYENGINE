// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "WalkabilityCacheManager.h"
#include "ObjectContainer.h"
#include "DebugDrawContext.h"

WalkabilityCacheManager::WalkabilityCacheManager()
	: m_currentFrameID(0)
	, m_walkabilityRequestCount(0)
	, m_walkabilityCacheHitCount(0)
	, m_floorRequestCount(0)
	, m_floorCacheHitCount(0)
	, m_preservedFloorCache(0)
{
}

WalkabilityCacheManager::~WalkabilityCacheManager()
{
	Reset();
}

void WalkabilityCacheManager::Reset()
{
	while (!m_caches.empty())
		EnableActor(m_caches.begin()->first, false);

	m_alloc.FreeMemory();

	m_walkabilityRequestCount = 0;
	m_walkabilityCacheHitCount = 0;
	m_floorRequestCount = 0;
	m_floorCacheHitCount = 0;
	m_preservedFloorCache = 0;
}

void WalkabilityCacheManager::PreUpdate()
{
	m_currentFrameID = gEnv->nMainFrameID;

	m_walkabilityRequestCount = 0;
	m_walkabilityCacheHitCount = 0;
	m_floorRequestCount = 0;
	m_floorCacheHitCount = 0;
	m_preservedFloorCache = 0;
}

void WalkabilityCacheManager::PostUpdate()
{
}

void WalkabilityCacheManager::Draw()
{
	ActorWalkabilityCaches::iterator it = m_caches.begin();
	ActorWalkabilityCaches::iterator end = m_caches.end();

	size_t memoryUsage = 0;

	for (; it != end; ++it)
	{
		ActorWalkabilityCache& actorCache = it->second;
		actorCache.cache->Draw();

		memoryUsage += actorCache.cache->GetMemoryUsage();
	}

	memoryUsage += m_alloc.GetTotalMemory().nAlloc;

	const float startY = 380.0f;
	float x = 1024.0f - 10.0f - 175.0f;
	float y = startY;

	CDebugDrawContext dc;
	const float FontSize = 1.2f;
	const float LineHeight = 11.25f * FontSize;

	dc->Draw2dLabel(x, y, FontSize * 1.25f, Col_BlueViolet, false, "WalkabilityCheck Stats");
	y += LineHeight * 1.25f;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Actors: %" PRISIZE_T, m_caches.size());
	y += LineHeight;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Memory: %.2fK", memoryUsage / 1024.0f);
	y += LineHeight;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Requested: %" PRISIZE_T, m_walkabilityRequestCount);
	y += LineHeight;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Hit Cache: %" PRISIZE_T " (%.1f%%)",
	                m_walkabilityCacheHitCount,
	                m_walkabilityRequestCount ? (m_walkabilityCacheHitCount / (float)m_walkabilityRequestCount) * 100.0f : 0.0f);
	y += LineHeight;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Preserved Floor Caches: %" PRISIZE_T, m_preservedFloorCache);
	y += LineHeight;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Floor Checks: %" PRISIZE_T, m_floorRequestCount);
	y += LineHeight;
	dc->Draw2dLabel(x, y, FontSize, Col_BlueViolet, false, "Floor Hit Cache: %" PRISIZE_T " (%.1f%%)",
	                m_floorCacheHitCount,
	                m_floorRequestCount ? (m_floorCacheHitCount / (float)m_floorRequestCount) * 100.0f : 0.0f);
	y += LineHeight;
}

void WalkabilityCacheManager::EnableActor(tAIObjectID actorID, bool enabled)
{
	if (!enabled)
	{
		ActorWalkabilityCaches::iterator it = m_caches.find(actorID);
		if (it != m_caches.end())
		{
			ActorWalkabilityCache& actorCache = it->second;
			if (actorCache.cache)
			{
				actorCache.cache->~WalkabilityCache();
				m_alloc.Deallocate(actorCache.cache);
			}

			m_caches.erase(it);
		}
	}
}

void WalkabilityCacheManager::PrepareActor(tAIObjectID actorID, const AABB& aabb)
{
	ActorWalkabilityCaches::iterator it = m_caches.find(actorID);
	if (it == m_caches.end())
	{
		std::pair<ActorWalkabilityCaches::iterator, bool> iresult =
		  m_caches.insert(ActorWalkabilityCaches::value_type(actorID, ActorWalkabilityCache()));

		it = iresult.first;
	}

	if (it != m_caches.end())
	{
		ActorWalkabilityCache& actorCache = it->second;
		if (!actorCache.cache)
			actorCache.cache = new(m_alloc.Allocate())WalkabilityCache(actorID);

		if (m_currentFrameID != actorCache.frameID)
		{
			IAIObject* actorObject = gAIEnv.pObjectContainer->GetAIObject(it->first);
			assert(actorObject);
			if (actorObject)
			{
				if (!actorCache.cache->Cache(aabb))
					++m_preservedFloorCache;
			}

			actorCache.frameID = m_currentFrameID;
		}
	}
}

bool WalkabilityCacheManager::IsFloorCached(tAIObjectID actorID, const Vec3& position, Vec3& floor)
{
	++m_floorRequestCount;

	if (!m_caches.empty())
	{
		ActorWalkabilityCaches::iterator it = m_caches.find(actorID);
		ActorWalkabilityCaches::iterator end = m_caches.end();

		if (it != end)
		{
			ActorWalkabilityCache& actorCache = it->second;

			if (actorCache.frameID == m_currentFrameID)
			{
				if (actorCache.cache->IsFloorCached(position, floor))
				{
					++m_floorCacheHitCount;

					return true;
				}
			}
		}

		// check other actors' aabbs
		it = m_caches.begin();

		for (; it != end; ++it)
		{
			ActorWalkabilityCache& actorCache = it->second;

			if ((actorCache.frameID == m_currentFrameID) && (it->first != actorID))
			{
				if (actorCache.cache->IsFloorCached(position, floor))
				{
					++m_floorCacheHitCount;

					return true;
				}
			}
		}
	}

	return false;
}

bool WalkabilityCacheManager::FindFloor(tAIObjectID actorID, const Vec3& position, Vec3& floor)
{
	++m_floorRequestCount;

	if (!m_caches.empty())
	{
		ActorWalkabilityCaches::iterator it = m_caches.find(actorID);
		ActorWalkabilityCaches::iterator end = m_caches.end();

		if (it != end)
		{
			ActorWalkabilityCache& actorCache = it->second;

			if (actorCache.frameID == m_currentFrameID)
			{
				if (actorCache.cache->IsFloorCached(position, floor))
				{
					++m_floorCacheHitCount;

					return floor.z < FLT_MAX;
				}
			}
		}

		// check other actors' aabbs
		it = m_caches.begin();

		for (; it != end; ++it)
		{
			ActorWalkabilityCache& actorCache = it->second;

			if ((actorCache.frameID == m_currentFrameID) && (it->first != actorID))
			{
				if (actorCache.cache->IsFloorCached(position, floor))
				{
					++m_floorCacheHitCount;

					return floor.z < FLT_MAX;
				}
			}
		}
	}

	// TODO: Keep track of the best containing cache and perform the floor search in there, so it's stored in the cache and
	// uses the already filtered physical entities

	return ::FindFloor(position, floor);
}

bool WalkabilityCacheManager::CheckWalkability(tAIObjectID actorID, const Vec3& origin, const Vec3& target, float radius,
                                               Vec3* finalFloor, bool* flatFloor)
{
	++m_walkabilityRequestCount;

	if (!m_caches.empty())
	{
		float minZ = min(origin.z, target.z) - WalkabilityFloorDownDist;
		float maxZ = max(origin.z, target.z) + WalkabilityTotalHeight;

		AABB enclosingAABB(AABB::RESET);
		enclosingAABB.Add(Vec3(origin.x, origin.y, minZ), radius);
		enclosingAABB.Add(Vec3(target.x, target.y, maxZ), radius);

		ActorWalkabilityCaches::iterator it = m_caches.find(actorID);
		ActorWalkabilityCaches::iterator end = m_caches.end();

		if (it != end)
		{
			ActorWalkabilityCache& actorCache = it->second;

			if (actorCache.frameID == m_currentFrameID)
			{
				if (actorCache.cache->FullyContaints(enclosingAABB))
				{
					++m_walkabilityCacheHitCount;
					return actorCache.cache->CheckWalkability(origin, target, radius, finalFloor, flatFloor);
				}
			}
		}

		// check other actors' aabbs
		it = m_caches.begin();

		for (; it != end; ++it)
		{
			ActorWalkabilityCache& actorCache = it->second;

			if ((actorCache.frameID == m_currentFrameID) && (it->first != actorID))
			{
				if (actorCache.cache->FullyContaints(enclosingAABB))
				{
					++m_walkabilityCacheHitCount;
					return actorCache.cache->CheckWalkability(origin, target, radius, finalFloor, flatFloor);
				}
			}
		}
	}

	// need to add floor flatness computation
	return ::CheckWalkability(origin, target, radius, finalFloor, flatFloor);
}

bool WalkabilityCacheManager::CheckWalkability(tAIObjectID actorID, const Vec3& origin, const Vec3& target, float radius,
                                               const ListPositions& boundary, Vec3* finalFloor, bool* flatFloor,
                                               const AABB* boundaryAABB)
{
	if (Overlap::Lineseg_Polygon2D(Lineseg(origin, target), boundary, boundaryAABB))
		return false;

	return CheckWalkability(actorID, origin, target, radius, finalFloor, flatFloor);
}
