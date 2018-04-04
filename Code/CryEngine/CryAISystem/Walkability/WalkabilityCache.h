// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __WalkabilityCache_h__
#define __WalkabilityCache_h__

#include "FloorHeightCache.h"

class WalkabilityCache
{
public:
	typedef StaticDynArray<IPhysicalEntity*, 768> Entities;
	typedef StaticDynArray<AABB, 768>             AABBs;

	WalkabilityCache(tAIObjectID actorID);

	void        Reset(bool resetFloorCache = true);
	bool        Cache(const AABB& aabb);
	void        Draw();

	const AABB& GetAABB() const;
	bool        FullyContaints(const AABB& aabb) const;

	size_t      GetOverlapping(const AABB& aabb, Entities& entities) const;
	size_t      GetOverlapping(const AABB& aabb, Entities& entities, AABBs& aabbs) const;

	bool        IsFloorCached(const Vec3& position, Vec3& floor) const;

	bool        FindFloor(const Vec3& position, Vec3& floor);
	bool        FindFloor(const Vec3& position, Vec3& floor, IPhysicalEntity** entities, AABB* aabbs, size_t entityCount,
	                      const AABB& enclosingAABB);
	bool        CheckWalkability(const Vec3& origin, const Vec3& target, float radius, Vec3* finalFloor = 0, bool* flatFloor = 0);
	bool        OverlapTorsoSegment(const Vec3& start, const Vec3& end, float radius, IPhysicalEntity** entities, size_t entityCount);

	size_t      GetMemoryUsage() const;

private:
	Vec3             m_center;
	AABB             m_aabb;

	size_t           m_entititesHash;
	tAIObjectID      m_actorID;

	Entities         m_entities;
	AABBs            m_aabbs;

	FloorHeightCache m_floorCache;
};

#endif
