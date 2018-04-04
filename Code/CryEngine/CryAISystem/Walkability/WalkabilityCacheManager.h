// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __WalkabilityCacheManager_h__
#define __WalkabilityCacheManager_h__

#include <CryMemory/PoolAllocator.h>
#include "WalkabilityCache.h"

class WalkabilityCacheManager
{
public:
	WalkabilityCacheManager();
	~WalkabilityCacheManager();

	void Reset();
	void PreUpdate();
	void PostUpdate();
	void Draw();

	void EnableActor(tAIObjectID actorID, bool enabled);
	void PrepareActor(tAIObjectID actorID, const AABB& aabb);

	bool IsFloorCached(tAIObjectID actorID, const Vec3& position, Vec3& floor);
	bool FindFloor(tAIObjectID actorID, const Vec3& position, Vec3& floor);
	bool CheckWalkability(tAIObjectID actorID, const Vec3& origin, const Vec3& target, float radius, Vec3* finalFloor = 0,
	                      bool* flatFloor = 0);
	bool CheckWalkability(tAIObjectID actorID, const Vec3& origin, const Vec3& target, float radius,
	                      const ListPositions& boundary, Vec3* finalFloor = 0, bool* flatFloor = 0, const AABB* boundaryAABB = 0);
private:
	struct ActorWalkabilityCache
	{
		ActorWalkabilityCache()
			: cache(0)
			, frameID(0)
		{
		}

		WalkabilityCache* cache;
		size_t            frameID;
	};

	size_t m_currentFrameID;

	typedef VectorMap<tAIObjectID, ActorWalkabilityCache> ActorWalkabilityCaches;
	ActorWalkabilityCaches                           m_caches;

	stl::PoolAllocatorNoMT<sizeof(WalkabilityCache)> m_alloc;

	size_t m_walkabilityRequestCount;
	size_t m_walkabilityCacheHitCount;
	size_t m_floorRequestCount;
	size_t m_floorCacheHitCount;
	size_t m_preservedFloorCache;
};

#endif
