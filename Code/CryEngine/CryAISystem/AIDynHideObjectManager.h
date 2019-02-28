// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIDynHideObjectManager.h
   $Id$
   Description: Provides to query hide points around entities which
   are flagged as AI hideable. The manage also caches the objects.

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Mikko Mononen
   - 2 Mar 2009	: Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/

#ifndef _AIDYNHIDEOBJECTMANAGER_H_
#define _AIDYNHIDEOBJECTMANAGER_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryCore/StlUtils.h>

struct SDynamicObjectHideSpot
{
	Vec3         pos, dir;
	EntityId     entityId;
	unsigned int nodeIndex;

	SDynamicObjectHideSpot(const Vec3& pos = ZERO, const Vec3& dir = ZERO, EntityId id = 0, unsigned int nodeIndex = 0) :
		pos(pos), dir(dir), entityId(id), nodeIndex(nodeIndex) {}
};

class CAIDynHideObjectManager
{
public:
	CAIDynHideObjectManager();

	void Reset();

	void GetHidePositionsWithinRange(std::vector<SDynamicObjectHideSpot>& hideSpots, const Vec3& pos, float radius,
	                                 IAISystem::tNavCapMask navCapMask, float passRadius, unsigned int lastNavNodeIndex = 0);

	bool ValidateHideSpotLocation(const Vec3& pos, const SAIBodyInfo& bi, EntityId objectEntId);

	void DebugDraw();

private:

	void         ResetCache();
	void         FreeCacheItem(int i);
	int          GetNewCacheItem();
	unsigned int GetPositionHashFromEntity(IEntity* pEntity);
	void         InvalidateHideSpotLocation(const Vec3& pos, EntityId objectEntId);

	struct SCachedDynamicObject
	{
		EntityId                            id;
		unsigned int                        positionHash;
		std::vector<SDynamicObjectHideSpot> spots;
		CTimeValue                          timeStamp;
	};

	typedef VectorMap<EntityId, unsigned int> DynamicOHideObjectMap;

	DynamicOHideObjectMap             m_cachedObjects;
	std::vector<SCachedDynamicObject> m_cache;
	std::vector<int>                  m_cacheFreeList;
};

#endif  // #ifndef _AIDYNHIDEOBJECTMANAGER_H_
