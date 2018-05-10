// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DeferredCollisionEvent.h
//  Version:     v1.00
//  Created:     12/08/2010 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "DeferredCollisionEvent.h"
#include <CryThreading/IJobManager_JobDelegator.h>
#include <CryEntitySystem/IEntitySystem.h>

void CDeferredPhysicsEventManager::DispatchDeferredEvent(IDeferredPhysicsEvent* pEvent)
{
	assert(pEvent);
	// execute immediately if we don't use deferred physics events
	if (GetCVars()->e_DeferredPhysicsEvents == 0 || gEnv->IsEditor() || gEnv->IsDedicated())
	{
		pEvent->Execute();
		return;
	}

	// Push onto the job queue
	pEvent->ExecuteAsJob();
}

void ApplyCollisionImpulse(EventPhysCollision* pCollision)
{
	if (pCollision->normImpulse && pCollision->pEntity[1] &&
	    pCollision->pEntity[0] && pCollision->pEntity[0]->GetType() == PE_PARTICLE &&
	    pCollision->pEntity[0]->GetForeignData(pCollision->pEntity[0]->GetiForeignData()))  // no foreign data mean it's likely scheduled for deletion
	{
		pe_action_impulse ai;
		ai.point = pCollision->pt;
		ai.partid = pCollision->partid[1];
		ai.impulse = (pCollision->vloc[0] - pCollision->vloc[1]) * pCollision->normImpulse;
		pCollision->pEntity[1]->Action(&ai);
	}
}

int CDeferredPhysicsEventManager::HandleEvent(const EventPhys* pEvent, IDeferredPhysicsEventManager::CreateEventFunc pCreateFunc, IDeferredPhysicsEvent::DeferredEventType type)
{
	EventPhysCollision* pCollision = (EventPhysCollision*)pEvent;
	assert(pCollision);

	if (pCollision->deferredState == EPC_DEFERRED_FINISHED)
	{
		ApplyCollisionImpulse(pCollision);
		return pCollision->deferredResult;
	}

	// == create new deferred event object, and do some housekeeping(ensuring entities not deleted, remebering event for cleanup) == //
	IDeferredPhysicsEvent* pDeferredEvent = pCreateFunc(pEvent);

	// == start executing == //
	pDeferredEvent->Start();

	// == check if we really needed to deferred this event(early outs, not deferred code paths) == //
	if (GetCVars()->e_DeferredPhysicsEvents == 0 || pDeferredEvent->HasFinished())
	{
		int nResult = pDeferredEvent->Result((EventPhys*)pEvent);
		SAFE_DELETE(pDeferredEvent);
		ApplyCollisionImpulse(pCollision);
		return nResult;
	}

	// make sure the referenced entities are not deleted when running async
	if (pCollision->iForeignData[0] == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pSrcEntity = (IEntity*)pCollision->pForeignData[0];
		if (pSrcEntity) pSrcEntity->IncKeepAliveCounter();
	}

	if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pTrgEntity = (IEntity*)pCollision->pForeignData[1];
		if (pTrgEntity) pTrgEntity->IncKeepAliveCounter();
	}

	if (pCollision->pEntity[0]) pCollision->pEntity[0]->AddRef();
	if (pCollision->pEntity[1]) pCollision->pEntity[1]->AddRef();

	// == re-queue event for the next frame, to keep the physical entity alive == //
	RegisterDeferredEvent(pDeferredEvent);

	return 0;
}

void CDeferredPhysicsEventManager::RegisterDeferredEvent(IDeferredPhysicsEvent* pDeferredEvent)
{
	assert(pDeferredEvent);
	m_activeDeferredEvents.push_back(pDeferredEvent);
}

void CDeferredPhysicsEventManager::UnRegisterDeferredEvent(IDeferredPhysicsEvent* pDeferredEvent)
{

	std::vector<IDeferredPhysicsEvent*>::iterator it = std::find(m_activeDeferredEvents.begin(), m_activeDeferredEvents.end(), pDeferredEvent);
	if (it == m_activeDeferredEvents.end())
		return;

	// == remove from active list == //
	m_activeDeferredEvents.erase(it);

	if (m_bEntitySystemReset)
		return;

	// == decrement keep alive counter on entity == //
	EventPhysCollision* pCollision = (EventPhysCollision*)pDeferredEvent->PhysicsEvent();

	if (pCollision->iForeignData[0] == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pSrcEntity = (IEntity*)pCollision->pForeignData[0];
		if (pSrcEntity)  pSrcEntity->DecKeepAliveCounter();
	}

	if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pTrgEntity = (IEntity*)pCollision->pForeignData[1];
		if (pTrgEntity)  pTrgEntity->DecKeepAliveCounter();
	}

	if (pCollision->pEntity[0]) pCollision->pEntity[0]->Release();
	if (pCollision->pEntity[1]) pCollision->pEntity[1]->Release();
}

void CDeferredPhysicsEventManager::ClearDeferredEvents()
{
	// move content of the active deferred events array to a tmp one to prevent problems with UnRegisterDeferredEvent called by destructors
	std::vector<IDeferredPhysicsEvent*> tmp = m_activeDeferredEvents;
	m_bEntitySystemReset = !gEnv->pEntitySystem || !gEnv->pEntitySystem->GetNumEntities();

	for (std::vector<IDeferredPhysicsEvent*>::iterator it = tmp.begin(); it != tmp.end(); ++it)
	{
		(*it)->Sync();
		delete *it;
	}
	stl::free_container(m_activeDeferredEvents);
	m_bEntitySystemReset = false;
}

void CDeferredPhysicsEventManager::Update()
{
	std::vector<IDeferredPhysicsEvent*> tmp = m_activeDeferredEvents;

	for (std::vector<IDeferredPhysicsEvent*>::iterator it = tmp.begin(), end = tmp.end(); it != end; ++it)
	{
		IDeferredPhysicsEvent* collisionEvent = *it;
		assert(collisionEvent);
		PREFAST_ASSUME(collisionEvent);
		EventPhysCollision* epc = (EventPhysCollision*) collisionEvent->PhysicsEvent();

		if (collisionEvent->HasFinished() == false)
		{
			continue;
		}

		epc->deferredResult = collisionEvent->Result();

		if (epc->deferredState != EPC_DEFERRED_FINISHED)
		{
			epc->deferredState = EPC_DEFERRED_FINISHED;
			gEnv->pPhysicalWorld->AddDeferredEvent(EventPhysCollision::id, epc);
		}
		else
			SAFE_DELETE(collisionEvent);
	}
}

IDeferredPhysicsEvent* CDeferredPhysicsEventManager::GetLastCollisionEventForEntity(IPhysicalEntity* pPhysEnt)
{
	EventPhysCollision* pLastPhysEvent;
	for (int i = m_activeDeferredEvents.size() - 1; i >= 0; i--)
		if ((pLastPhysEvent = (EventPhysCollision*)m_activeDeferredEvents[i]->PhysicsEvent()) && pLastPhysEvent->idval == EventPhysCollision::id && pLastPhysEvent->pEntity[0] == pPhysEnt)
			return m_activeDeferredEvents[i];
	return 0;
}
