// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: playback of generic breaking events
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#include <CryEntitySystem/IBreakableManager.h>

#if !NET_USE_SIMPLE_BREAKAGE

	#include "GenericPlaybackListener.h"

CGenericPlaybackListener::CGenericPlaybackListener() : m_spawnIdx(0), m_pBreakage(0)
{
}

void CGenericPlaybackListener::Perform(const SSimulateRemoveEntityParts& param, INetBreakagePlaybackPtr pBreakage)
{
	CRY_ASSERT(!m_pBreakage);
	m_pBreakage = pBreakage;
	CRY_ASSERT(m_spawnIdx == 0);

	EventPhysRemoveEntityParts event;
	event.pEntity = param.ent.Find();
	if (!event.pEntity)
		return;
	event.iForeignData = event.pEntity->GetiForeignData();
	event.pForeignData = event.pEntity->GetForeignData(event.iForeignData);
	gEnv->pEntitySystem->GetBreakableManager()->FakePhysicsEvent(&event);
}

bool CGenericPlaybackListener::AcceptUpdateMesh(const EventPhysUpdateMesh* pEvent)
{
	return false;
}

bool CGenericPlaybackListener::AcceptCreateEntityPart(const EventPhysCreateEntityPart* pEvent)
{
	return false;
}

bool CGenericPlaybackListener::AcceptRemoveEntityParts(const EventPhysRemoveEntityParts* pEvent)
{
	return false;
}

bool CGenericPlaybackListener::AcceptJointBroken(const EventPhysJointBroken* pEvent)
{
	return false;
}

void CGenericPlaybackListener::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
	CRY_ASSERT(m_pBreakage != NULL);
	m_pBreakage->SpawnedEntity(m_spawnIdx++, pEntity->GetId());
}

void CGenericPlaybackListener::OnRemove(IEntity* pEntity)
{
}

void CGenericPlaybackListener::EndEvent(INetContext* pCtx)
{
	CRY_ASSERT(m_pBreakage != NULL);
	m_pBreakage = 0;
	m_spawnIdx = 0;
}

void CGenericPlaybackListener::OnStartFrame()
{
}

bool CGenericPlaybackListener::OnEndFrame()
{
	CRY_ASSERT(false);
	return false;
}

void CGenericPlaybackListener::OnTimeout()
{
}

/*
   void CGenericPlaybackListener::Perform( const SSimulateCreateEntityPart& param, INetBreakagePlayback * pBreakage )
   {
   CRY_ASSERT(!m_pBreakage);
   m_pBreakage = pBreakage;
   CRY_ASSERT(m_spawnIdx == 0);

   EventPhysCreateEntityPart event;
   event.bInvalid = param.bInvalid;
   event.breakAngImpulse = param.breakAngImpulse;
   event.breakImpulse = param.breakImpulse;
   for (int i=0; i<2; i++)
   {
    event.cutDirLoc[i] = param.cutDirLoc[i];
    event.cutPtLoc[i] = param.cutPtLoc[i];
   }
   event.cutRadius = param.cutRadius;
   event.pEntNew = param.entNew.Find();
   event.pEntity = param.entSrc.Find();
   if (!event.pEntity)
    return;
   event.iForeignData = event.pEntity->GetiForeignData();
   event.pForeignData = event.pEntity->GetForeignData();
   event.nTotParts = param.nTotParts;
   event.partidNew = param.partidNew;
   event.partidSrc = param.partidSrc;
   gEnv->pEntitySystem->GetBreakableManager()->FakePhysicsEvent(&event);
   }
 */

#endif // !NET_USE_SIMPLE_BREAKAGE
