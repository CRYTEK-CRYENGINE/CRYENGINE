// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RadixSort.h"

class CEntityComponentTriggerBounds;

//////////////////////////////////////////////////////////////////////////
struct SProximityElement
{
	//////////////////////////////////////////////////////////////////////////
	EntityId                        id;
	AABB                            aabb;
	uint32                          bActivated : 1;
	std::vector<SProximityElement*> inside;

	SProximityElement()
	{
		id = 0;
		bActivated = 0;
	}
	~SProximityElement()
	{
	}
	bool AddInside(SProximityElement* elem)
	{
		// Sorted add.
		return stl::binary_insert_unique(inside, elem);
	}
	bool RemoveInside(SProximityElement* elem)
	{
		// sorted remove.
		return stl::binary_erase(inside, elem);
	}
	bool IsInside(SProximityElement* elem)
	{
		return std::binary_search(inside.begin(), inside.end(), elem);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete for pool allocator.
	//////////////////////////////////////////////////////////////////////////
	ILINE void* operator new(size_t nSize);
	ILINE void  operator delete(void* ptr);

};

//////////////////////////////////////////////////////////////////////////
struct SProximityEvent
{
	bool               bEnter; // Enter/Leave.
	EntityId           entity;
	SProximityElement* pTrigger;
	void               GetMemoryUsage(ICrySizer* pSizer) const {}
};

//////////////////////////////////////////////////////////////////////////
class CProximityTriggerSystem
{
public:
	CProximityTriggerSystem();
	~CProximityTriggerSystem();

	SProximityElement* CreateTrigger();
	void               RemoveTrigger(SProximityElement* pTrigger);
	void               MoveTrigger(SProximityElement* pTrigger, const AABB& aabb, bool invalidateCachedAABB = false);

	SProximityElement* CreateEntity(EntityId id);
	void               MoveEntity(SProximityElement* pEntity, const Vec3& pos);
	void               RemoveEntity(SProximityElement* pEntity, bool instantEvent = false);

	void               Update();
	void               Reset();
	void               BeginReset();

	void               GetMemoryUsage(ICrySizer* pSizer) const;

private:
	void CheckIfLeavingTrigger(SProximityElement* pEntity);
	void ProcessOverlap(SProximityElement* pEntity, SProximityElement* pTrigger);
	void RemoveFromTriggers(SProximityElement* pEntity, bool instantEvent = false);
	void PurgeRemovedTriggers();
	void SortTriggers();
	void SendEvents();
	void SendEvent(EEntityEvent eventId, EntityId triggerId, EntityId entityId);

private:
	typedef std::vector<SProximityElement*> Elements;

	Elements                        m_triggers;
	Elements                        m_triggersToRemove;
	Elements                        m_entitiesToRemove;
	bool                            m_bTriggerMoved;
	bool                            m_bResetting;

	std::vector<SProximityElement*> m_entities;
	std::vector<AABB>               m_triggersAABB;

	std::vector<SProximityEvent>    m_events;

	RadixSort                       m_triggerSorter;
	RadixSort                       m_entitySorter;

	std::vector<float>              m_minPosList0;
	std::vector<float>              m_minPosList1;
	const uint32*                   m_Sorted0;
	const uint32*                   m_Sorted1;

public:
	typedef stl::PoolAllocatorNoMT<sizeof(SProximityElement)> ProximityElement_PoolAlloc;
	static ProximityElement_PoolAlloc* g_pProximityElement_PoolAlloc;
};

//////////////////////////////////////////////////////////////////////////
inline void* SProximityElement::operator new(size_t nSize)
{
	void* ptr = CProximityTriggerSystem::g_pProximityElement_PoolAlloc->Allocate();
	if (ptr)
		memset(ptr, 0, nSize); // Clear objects memory.
	return ptr;
}
//////////////////////////////////////////////////////////////////////////
inline void SProximityElement::operator delete(void* ptr)
{
	if (ptr)
		CProximityTriggerSystem::g_pProximityElement_PoolAlloc->Deallocate(ptr);
}
