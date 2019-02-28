// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WorldMonitor.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "DebugDrawContext.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

WorldMonitor::WorldMonitor()
	: m_callback(0)
	, m_enabled(gEnv->IsEditor())
{
}

WorldMonitor::WorldMonitor(const Callback& callback)
	: m_callback(callback)
	, m_enabled(gEnv->IsEditor())
{
}

void WorldMonitor::Start()
{
	AILogComment("Navigation System WorldMonitor::Start");

	if (IsEnabled())
	{
		if (m_callback)
		{
			gEnv->pPhysicalWorld->AddEventClient(EventPhysStateChange::id, StateChangeHandler, 1, 1.0f);
			gEnv->pPhysicalWorld->AddEventClient(EventPhysEntityDeleted::id, EntityRemovedHandler, 1, 1.0f);
			gEnv->pPhysicalWorld->AddEventClient(EventPhysEntityDeleted::id, EntityRemovedHandlerAsync, 0, 1.0f);
		}
	}
}

void WorldMonitor::Stop()
{
	AILogComment("Navigation System WorldMonitor::Stop");

	if (IsEnabled())
	{
		if (m_callback)
		{
			gEnv->pPhysicalWorld->RemoveEventClient(EventPhysStateChange::id, StateChangeHandler, 1);
			gEnv->pPhysicalWorld->RemoveEventClient(EventPhysEntityDeleted::id, EntityRemovedHandler, 1);
			gEnv->pPhysicalWorld->RemoveEventClient(EventPhysEntityDeleted::id, EntityRemovedHandlerAsync, 0);
		}
	}
}

bool WorldMonitor::IsEnabled() const
{
	return m_enabled;
}

int WorldMonitor::StateChangeHandler(const EventPhys* pPhysEvent)
{
	WorldMonitor* pthis = gAIEnv.pNavigationSystem->GetWorldMonitor();

	assert(pthis != NULL);
	assert(pthis->IsEnabled());

	{
		const EventPhysStateChange* event = (EventPhysStateChange*)pPhysEvent;

		bool consider = false;

		if (event->iSimClass[1] == SC_STATIC)
			consider = true;
		else
		{
			IPhysicalEntity* physEnt = event->pEntity;

			if ((event->iSimClass[1] == SC_SLEEPING_RIGID) ||
			    (event->iSimClass[1] == SC_ACTIVE_RIGID) ||
			    (physEnt->GetType() == PE_RIGID))
			{
				consider = NavigationSystemUtils::IsDynamicObjectPartOfTheMNMGenerationProcess(physEnt);
			}
		}

		if (consider)
		{
			const AABB aabbOld(event->BBoxOld[0], event->BBoxOld[1]);
			const AABB aabbNew(event->BBoxNew[0], event->BBoxNew[1]);

			if (((aabbOld.min - aabbNew.min).len2() + (aabbOld.max - aabbNew.max).len2()) > 0.0f)
			{
				int entityId = gEnv->pPhysicalWorld->GetPhysicalEntityId(event->pEntity);
				
				pthis->m_callback(entityId, aabbOld);
				pthis->m_callback(entityId, aabbNew);

				if (gAIEnv.CVars.DebugDrawNavigationWorldMonitor)
				{
					CDebugDrawContext dc;

					dc->DrawAABB(aabbOld, IDENTITY, false, Col_DarkGray, eBBD_Faceted);
					dc->DrawAABB(aabbNew, IDENTITY, false, Col_White, eBBD_Faceted);
				}
			}
		}
	}

	return 1;
}

int WorldMonitor::EntityRemovedHandler(const EventPhys* pPhysEvent)
{
	WorldMonitor* pthis = gAIEnv.pNavigationSystem->GetWorldMonitor();

	assert(pthis != NULL);
	assert(pthis->IsEnabled());

	EntityAABBChange change;
	if (ShallEventPhysEntityDeletedBeHandled(pPhysEvent, change))
	{
		const EventPhysEntityDeleted* event = static_cast<const EventPhysEntityDeleted*>(pPhysEvent);
		pthis->m_callback(change.entityId, change.aabb);

		if (gAIEnv.CVars.DebugDrawNavigationWorldMonitor)
		{
			CDebugDrawContext dc;
			dc->DrawAABB(change.aabb, IDENTITY, false, Col_White, eBBD_Faceted);
		}
	}

	return 1;
}

int WorldMonitor::EntityRemovedHandlerAsync(const EventPhys* pPhysEvent)
{
	WorldMonitor* pthis = gAIEnv.pNavigationSystem->GetWorldMonitor();

	assert(pthis != NULL);
	assert(pthis->IsEnabled());
	
	EntityAABBChange change;
	if (ShallEventPhysEntityDeletedBeHandled(pPhysEvent, change))
	{
		const EventPhysEntityDeleted* event = static_cast<const EventPhysEntityDeleted*>(pPhysEvent);
		pthis->m_queuedAABBChanges.push_back(change);
	}

	return 1;
}

bool WorldMonitor::ShallEventPhysEntityDeletedBeHandled(const EventPhys* pPhysEvent, EntityAABBChange& outChange)
{
	assert(pPhysEvent->idval == EventPhysEntityDeleted::id);

	const EventPhysEntityDeleted* event = static_cast<const EventPhysEntityDeleted*>(pPhysEvent);
	if (event->isFromPOD)
		return false;

	bool consider = false;

	IPhysicalEntity* physEnt = event->pEntity;
	pe_type type = physEnt->GetType();

	if (type == PE_STATIC)
	{
		consider = true;
	}
	else if (type == PE_RIGID)
	{
		consider = NavigationSystemUtils::IsDynamicObjectPartOfTheMNMGenerationProcess(physEnt);
	}

	if (consider)
	{
		pe_status_pos sp;

		if (physEnt->GetStatus(&sp))
		{
			// Careful: the AABB we just received is relative to the entity's position. This is specific to the EventPhysEntityDeleted we're currently handling.
			outChange.aabb.min = sp.BBox[0] + sp.pos;
			outChange.aabb.max = sp.BBox[1] + sp.pos;
			outChange.entityId = gEnv->pPhysicalWorld->GetPhysicalEntityId(physEnt);
			return true;
		}
	}

	return false;
}

void WorldMonitor::FlushPendingAABBChanges()
{
	std::vector<EntityAABBChange> changesInTheWorld;
	m_queuedAABBChanges.swap(changesInTheWorld);

	if (!changesInTheWorld.empty())
	{
		for (const EntityAABBChange& aabbChange : changesInTheWorld)
		{
			m_callback(aabbChange.entityId, aabbChange.aabb);
		}

		if (gAIEnv.CVars.DebugDrawNavigationWorldMonitor)
		{
			CDebugDrawContext dc;
			for (const EntityAABBChange& aabbChange : changesInTheWorld)
			{
				dc->DrawAABB(aabbChange.aabb, IDENTITY, false, Col_White, eBBD_Faceted);
			}
		}
	}
}