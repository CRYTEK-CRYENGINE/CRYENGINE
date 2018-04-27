// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ProximityTriggerSystem.h"
#include "TriggerProxy.h"

#define AXIS_0        0
#define AXIS_1        1
#define AXIS_2        2

#define ENTITY_RADIUS (0.5f)

#define GET_ENTITY_NAME(eid) g_pIEntitySystem->GetEntityFromID(eid)->GetName()

namespace
{
struct InOut
{
	float pos;
	// the following fields can take the value -1, 0, or 1, and are used to increment/decrement/keep-the-same
	// the inA and inB arrays in the routine below.
	int a;
	int b;
	int test;

	bool operator<(const InOut& rhs) const
	{
		return pos < rhs.pos;
	}
};
}
// find the remainder from a-b
static const int MAX_AABB_DIFFERENCES = 4 * 4 * 4;
static void AABBDifference(const AABB& a, const AABB& b, AABB* differences, int* ndifferences)
{
	*ndifferences = 0;

	static const float SMALL_SIZE = 5;

	if (b.GetVolume() < SMALL_SIZE * SMALL_SIZE * SMALL_SIZE || !a.IsIntersectBox(b))
		differences[(*ndifferences)++] = b;
	else
	{
		InOut inout[3][4];
		for (int i = 0; i < 3; i++)
		{
#define FILL_INOUT(n, p, aiv, aov, biv, bov) inout[i][n].pos = p[i]; inout[i][n].a = aiv - aov; inout[i][n].b = biv - bov
			FILL_INOUT(0, a.min, 1, 0, 0, 0);
			FILL_INOUT(1, a.max, 0, 1, 0, 0);
			FILL_INOUT(2, b.min, 0, 0, 1, 0);
			FILL_INOUT(3, b.max, 0, 0, 0, 1);
#undef FILL_INOUT
			std::sort(inout[i], inout[i] + 4);
			for (int j = 0; j < 3; j++)
				inout[i][j].test = (inout[i][j].pos != inout[i][j + 1].pos);
			inout[i][3].test = 0;
		}
		// tracks whether we're inside or outside the AABB's a and b (1 if we're inside)
		int inA[3] = { 0, 0, 0 };
		int inB[3] = { 0, 0, 0 };
		for (int ix = 0; ix < 4; ix++)
		{
			inA[0] += inout[0][ix].a;
			inB[0] += inout[0][ix].b;
			if (inout[0][ix].test)
			{
				for (int iy = 0; iy < 4; iy++)
				{
					inA[1] += inout[1][iy].a;
					inB[1] += inout[1][iy].b;
					if (inout[1][iy].test)
					{
						for (int iz = 0; iz < 4; iz++)
						{
							inA[2] += inout[2][iz].a;
							inB[2] += inout[2][iz].b;
							if (inout[2][iz].test)
							{
								if (inB[0] && inB[1] && inB[2])
								{
									if (!(inA[0] && inA[1] && inA[2]))
									{
										differences[*ndifferences] = AABB(
										  Vec3(inout[0][ix].pos, inout[1][iy].pos, inout[2][iz].pos),
										  Vec3(inout[0][ix + 1].pos, inout[1][iy + 1].pos, inout[2][iz + 1].pos));
										if (differences[*ndifferences].GetVolume())
											(*ndifferences)++;
										else
											assert(false);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CProximityTriggerSystem::ProximityElement_PoolAlloc* CProximityTriggerSystem::g_pProximityElement_PoolAlloc = 0;

//////////////////////////////////////////////////////////////////////////
CProximityTriggerSystem::CProximityTriggerSystem()
	: m_bTriggerMoved(false)
	, m_bResetting(false)
	, m_Sorted0(nullptr)
	, m_Sorted1(nullptr)
{
	assert(!g_pProximityElement_PoolAlloc);
	g_pProximityElement_PoolAlloc = new ProximityElement_PoolAlloc;
}

//////////////////////////////////////////////////////////////////////////
CProximityTriggerSystem::~CProximityTriggerSystem()
{
	delete g_pProximityElement_PoolAlloc;
}

//////////////////////////////////////////////////////////////////////////
SProximityElement* CProximityTriggerSystem::CreateTrigger()
{
	// Should use pool allocator here.
	SProximityElement* pTrigger = new SProximityElement;
	m_triggers.push_back(pTrigger);
	m_bTriggerMoved = true;
	return pTrigger;
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::RemoveTrigger(SProximityElement* pTrigger)
{
	m_triggersToRemove.push_back(pTrigger);
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::MoveTrigger(SProximityElement* pTrigger, const AABB& aabb, bool invalidateCachedAABB)
{
	pTrigger->aabb = aabb;
	pTrigger->bActivated = true;
	m_bTriggerMoved = true;

	if (invalidateCachedAABB)
	{
		// notify each entity which is inside
		uint32 num = (uint32)pTrigger->inside.size();
		for (uint32 i = 0; i < num; ++i)
		{
			pTrigger->inside[i]->RemoveInside(pTrigger);
		}

		pTrigger->inside.clear();
		// marcok: find the trigger (not ideal, but I don't have an index I can access here)
		num = (uint32)m_triggers.size();
		for (uint32 i = 0; i < num; ++i)
		{
			if (m_triggers[i] == pTrigger && m_triggersAABB.size() > i)
			{
				m_triggersAABB[i] = AABB(ZERO);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::SortTriggers()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (m_bTriggerMoved)
	{
		m_bTriggerMoved = false;
		m_minPosList1.resize(m_triggers.size());
		m_triggersAABB.resize(m_triggers.size(), AABB(ZERO));
		if (!m_triggers.empty())
		{
			uint32 num = (uint32)m_triggers.size();
			for (uint32 i = 0; i < num; ++i)
			{
				SProximityElement* pTrigger = m_triggers[i];
				if (pTrigger->bActivated)
				{
					pTrigger->bActivated = false;
					// Check if any entity inside this trigger is leaving.
					for (int j = 0; j < (int)pTrigger->inside.size(); )
					{
						SProximityElement* pEntity = pTrigger->inside[j];
						if (!pTrigger->aabb.IsIntersectBox(pEntity->aabb))
						{
							// Entity Leaving this trigger.
							pTrigger->inside.erase(pTrigger->inside.begin() + j);
							if (pEntity->RemoveInside(pTrigger))
							{
								// Add leave event.
								SProximityEvent event;
								event.bEnter = false;
								event.entity = pEntity->id;
								event.pTrigger = pTrigger;
								m_events.push_back(event);
							}
							else
								assert(0); // Should never happen.
						}
						else
							j++;
					}
					// check if anything new needs to be included
					AABB differences[MAX_AABB_DIFFERENCES];
					int ndifferences;
					AABBDifference(m_triggersAABB[i], m_triggers[i]->aabb, differences, &ndifferences);
					for (int j = 0; j < ndifferences; j++)
					{
						SEntityProximityQuery q;
						q.box = differences[j];
						g_pIEntitySystem->QueryProximity(q);
						for (int k = 0; k < q.nCount; k++)
						{
							if (CEntity* pEnt = static_cast<CEntity*>(q.pEntities[k]))
							{
								if (SProximityElement* pProxElem = pEnt->GetProximityElement())
								{
									Vec3 pos = pEnt->GetWorldPos();
									MoveEntity(pProxElem, pos);
								}
							}
						}
					}
					/*
					   #define LOG_AABB(idx,xx) CryLogAlways("diff[%d]: (%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f)", idx, xx.min.x, xx.min.y, xx.min.z, xx.max.x, xx.max.y, xx.max.z)
					          if (ndifferences)
					          {
					            LOG_AABB(-200, m_triggersAABB[i]);m_pTriggerSorter
					            LOG_AABB(-100, m_triggers[i]->aabb);
					            for (int j=0; j<ndifferences; j++)
					            {
					              LOG_AABB(j, differences[j]);
					            }
					          }
					   #undef LOG_AABB
					 */
				}
				m_triggersAABB[i] = m_triggers[i]->aabb;
				m_minPosList1[i] = m_triggers[i]->aabb.min[AXIS_0];
			}
			m_Sorted1 = m_triggerSorter.Sort(&m_minPosList1[0], m_minPosList1.size()).GetRanks();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
inline bool IsIntersectInAxis(const AABB& a, const AABB& b, uint32 axis)
{
	if (a.max[axis] < b.min[axis] || b.max[axis] < a.min[axis])
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::ProcessOverlap(SProximityElement* pEntity, SProximityElement* pTrigger)
{
	if (pEntity->AddInside(pTrigger))
	{
		pTrigger->AddInside(pEntity);

		// Add enter event.
		SProximityEvent event;
		event.bEnter = true;
		event.entity = pEntity->id;
		event.pTrigger = pTrigger;
		m_events.push_back(event);
	}

	//CryLogAlways( "[Trigger] Overlap %s and %s",GET_ENTITY_NAME(pEntity->id),GET_ENTITY_NAME(pTrigger->id) );
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (m_bTriggerMoved)
		SortTriggers();

	if (!m_entities.empty())
	{
		uint32 num = (uint32)m_entities.size();
		m_minPosList0.resize(num);
		for (uint32 i = 0; i < num; ++i)
		{
			m_entities[i]->bActivated = false;
			CheckIfLeavingTrigger(m_entities[i]);
			m_minPosList0[i] = m_entities[i]->aabb.min[AXIS_0];
		}
		m_Sorted0 = m_entitySorter.Sort(&m_minPosList0[0], m_minPosList0.size()).GetRanks();
		if (!m_minPosList0.empty() && !m_minPosList1.empty())
		{
			// Prune triggers.
			uint32 Index0, Index1;

			float* MinPosList0 = &m_minPosList0[0];
			float* MinPosList1 = &m_minPosList1[0];

			AABB* array1 = &m_triggersAABB[0];

			const uint32* Sorted0 = &m_Sorted0[0];
			const uint32* Sorted1 = &m_Sorted1[0];
			const uint32* const LastSorted0 = &Sorted0[m_minPosList0.size()];
			const uint32* const LastSorted1 = &Sorted1[m_minPosList1.size()];
			const uint32* RunningAddress0 = Sorted0;
			const uint32* RunningAddress1 = Sorted1;

			while (RunningAddress1 < LastSorted1 && Sorted0 < LastSorted0)
			{
				Index0 = *Sorted0++;

				while (RunningAddress1 < LastSorted1 && MinPosList1[*RunningAddress1] < MinPosList0[Index0])
					RunningAddress1++;

				const uint32* RunningAddress2_1 = RunningAddress1;

				while (RunningAddress2_1 < LastSorted1 && MinPosList1[Index1 = *RunningAddress2_1++] <= m_entities[Index0]->aabb.max[AXIS_0])
				{
					if (IsIntersectInAxis(m_entities[Index0]->aabb, array1[Index1], AXIS_1))
					{
						if (IsIntersectInAxis(m_entities[Index0]->aabb, array1[Index1], AXIS_2))
						{
							ProcessOverlap(m_entities[Index0], m_triggers[Index1]);
							//pairs.AddPair(Index0, Index1);
						}
					}
				}
			}

			////
			while (RunningAddress0 < LastSorted0 && Sorted1 < LastSorted1)
			{
				Index0 = *Sorted1++;

				while (RunningAddress0 < LastSorted0 && MinPosList0[*RunningAddress0] <= MinPosList1[Index0])
					RunningAddress0++;

				const uint32* RunningAddress2_0 = RunningAddress0;

				while (RunningAddress2_0 < LastSorted0 && MinPosList0[Index1 = *RunningAddress2_0++] <= array1[Index0].max[AXIS_0])
				{
					if (IsIntersectInAxis(m_entities[Index1]->aabb, array1[Index0], AXIS_1))
					{
						if (IsIntersectInAxis(m_entities[Index1]->aabb, array1[Index0], AXIS_2))
						{
							ProcessOverlap(m_entities[Index1], m_triggers[Index0]);
							//pairs.AddPair(Index1, Index0);
						}
					}
				}
			}
		}
		m_entities.resize(0);
	}

	if (!m_triggersToRemove.empty())
		PurgeRemovedTriggers();

	if (!m_events.empty())
		SendEvents();

}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::SendEvent(EEntityEvent eventId, EntityId triggerId, EntityId entityId)
{
	CEntity* pTriggerEntity = g_pIEntitySystem->GetEntityFromID(triggerId);
	if (pTriggerEntity)
	{
		SEntityEvent event;
		//if (g_pIEntitySystem->GetEntityFromID( entityId ))
		//{
		// if (eventId = ENTITY_EVENT_ENTERAREA)
		//CryLogAlways( "[Trigger] Enter %s into %s",GET_ENTITY_NAME( entityId ),pTriggerEntity->GetName() );
		//	else
		// if (eventId = ENTITY_EVENT_LEAVEAREA)
		//CryLogAlways( "[Trigger] Leave %s from %s",GET_ENTITY_NAME( entityId ),pTriggerEntity->GetName() );
		//	else
		//		assert(false)
		//}
		event.event = eventId;
		event.nParam[0] = entityId;
		event.nParam[1] = 0;
		event.nParam[2] = triggerId;
		pTriggerEntity->SendEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::SendEvents()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		EEntityEvent eventId = m_events[i].bEnter ? ENTITY_EVENT_ENTERAREA : ENTITY_EVENT_LEAVEAREA;
		SendEvent(eventId, m_events[i].pTrigger->id, m_events[i].entity);
	}
	m_events.resize(0);
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::PurgeRemovedTriggers()
{
	for (int i = 0; i < (int)m_triggersToRemove.size(); i++)
	{
		SProximityElement* pTrigger = m_triggersToRemove[i];
		PREFAST_ASSUME(pTrigger);
		for (int j = 0; j < (int)pTrigger->inside.size(); j++)
		{
			SProximityElement* pEntity = pTrigger->inside[j];
			pEntity->RemoveInside(pTrigger);
		}

		stl::find_and_erase(m_triggers, pTrigger);
		delete pTrigger;
		m_bTriggerMoved = true;
	}
	m_triggersToRemove.resize(0);
}

//////////////////////////////////////////////////////////////////////////
SProximityElement* CProximityTriggerSystem::CreateEntity(EntityId id)
{
	if (m_bResetting)
		return 0;

	SProximityElement* pEntity = new SProximityElement;
	pEntity->id = id;
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::RemoveEntity(SProximityElement* pEntity, bool instantEvent)
{
	if (m_bResetting)
	{
		m_entitiesToRemove.push_back(pEntity);
		return;
	}

	// Remove it from all triggers.
	RemoveFromTriggers(pEntity, instantEvent);
	if (!m_entities.empty())
	{
		stl::find_and_erase(m_entities, pEntity);
	}
	delete pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::MoveEntity(SProximityElement* pEntity, const Vec3& pos)
{
	if (m_bResetting)
		return;

	// Entity 1 meter
	pEntity->aabb.min = pos - Vec3(ENTITY_RADIUS, ENTITY_RADIUS, 0);
	pEntity->aabb.max = pos + Vec3(ENTITY_RADIUS, ENTITY_RADIUS, ENTITY_RADIUS);
	if (!pEntity->bActivated)
	{
		pEntity->bActivated = true;
		m_entities.push_back(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::RemoveFromTriggers(SProximityElement* pEntity, bool instantEvent)
{
	int num = (int)pEntity->inside.size();
	for (int i = 0; i < num; ++i)
	{
		SProximityElement* pTrigger = pEntity->inside[i];
		if (pTrigger && pTrigger->RemoveInside(pEntity))
		{
			if (instantEvent)
			{
				SendEvent(ENTITY_EVENT_LEAVEAREA, pTrigger->id, pEntity->id);
			}
			else
			{
				// Add leave event.
				SProximityEvent event;
				event.bEnter = false;
				event.entity = pEntity->id;
				event.pTrigger = pTrigger;
				m_events.push_back(event);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::CheckIfLeavingTrigger(SProximityElement* pEntity)
{
	// Check if we are still inside stored triggers.
	for (int i = 0; i < (int)pEntity->inside.size(); )
	{
		SProximityElement* pTrigger = pEntity->inside[i];
		if (!pTrigger->aabb.IsIntersectBox(pEntity->aabb))
		{
			// Leaving this trigger.
			pEntity->inside.erase(pEntity->inside.begin() + i);
			if (pTrigger->RemoveInside(pEntity))
			{
				// Add leave event.
				SProximityEvent event;
				event.bEnter = false;
				event.entity = pEntity->id;
				event.pTrigger = pTrigger;
				m_events.push_back(event);
			}
			else
				assert(0); // Should not happen that we wasn't inside trigger.
		}
		else
			i++;
	}
}

//////////////////////////////////////////////////////////////////////////
void CProximityTriggerSystem::Reset()
{
	// by this stage nothing else should care about being notified about anything, so just delete the entities and triggers
	for (uint32 i = 0; i < (uint32)m_entitiesToRemove.size(); i++)
	{
		delete m_entitiesToRemove[i];
	}

	for (uint32 i = 0; i < (uint32)m_triggersToRemove.size(); i++)
	{
		delete m_triggersToRemove[i];
	}

	m_entities.clear();
	m_triggersAABB.clear();
	m_events.clear();
	m_minPosList0.clear();
	m_minPosList1.clear();
	m_triggers.clear();
	m_triggersToRemove.clear();
	m_entitiesToRemove.clear();

	// use swap trick to deallocate memory in vector
	std::vector<SProximityElement*>(m_entities).swap(m_entities);
	std::vector<AABB>(m_triggersAABB).swap(m_triggersAABB);
	std::vector<SProximityEvent>(m_events).swap(m_events);
	std::vector<float>(m_minPosList0).swap(m_minPosList0);
	std::vector<float>(m_minPosList1).swap(m_minPosList1);
	Elements(m_triggers).swap(m_triggers);
	Elements(m_triggersToRemove).swap(m_triggersToRemove);
	Elements(m_entitiesToRemove).swap(m_entitiesToRemove);
	stl::reconstruct(m_entitySorter);
	stl::reconstruct(m_triggerSorter);

	g_pProximityElement_PoolAlloc->FreeMemory();

	m_bResetting = false;
}

void CProximityTriggerSystem::BeginReset()
{
	// When we begin resetting we ignore all calls to the trigger system until call to the Reset.
	m_bResetting = true;
}

void CProximityTriggerSystem::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(g_pProximityElement_PoolAlloc);
	pSizer->AddContainer(m_triggers);
	pSizer->AddContainer(m_triggersToRemove);
	pSizer->AddContainer(m_entities);
	pSizer->AddContainer(m_triggersAABB);
	pSizer->AddContainer(m_events);
	pSizer->AddContainer(m_minPosList0);
	pSizer->AddContainer(m_minPosList1);
}
