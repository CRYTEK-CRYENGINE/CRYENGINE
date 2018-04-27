// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HackBullet.h"



void CHackBullet::HandleEvent(const SGameObjectEvent& event)
{
	if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision *pCollision = (EventPhysCollision *)event.ptr;

		IEntity* pTarget = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1]:0;
		if (pTarget == GetEntity())
			return;

		if (pTarget)
		{
			SEntityEvent event;
			const char* eventName = "Hacked";
			event.event = ENTITY_EVENT_ACTIVE_FLOW_NODE_OUTPUT;
			event.nParam[0] = (INT_PTR)eventName;
			pTarget->SendEvent(event);
		}

		Destroy();
	}
}
