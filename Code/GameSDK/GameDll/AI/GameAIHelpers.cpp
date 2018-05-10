// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameAIHelpers.h"
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>

CGameAIInstanceBase::CGameAIInstanceBase(EntityId entityID)
{
	Init(entityID);
}

void CGameAIInstanceBase::Init(EntityId entityID)
{
	m_entityID = entityID;

#ifndef RELEASE
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID))
	{
		m_debugEntityName = entity->GetName();
	}
#endif
}

void CGameAIInstanceBase::SendSignal(const char* signal, IAISignalExtraData* data)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(GetEntityID());
	assert(entity);
	if (entity)
	{
		if (IAIObject* aiObject = entity->GetAI())
			if (IAIActor* aiActor = aiObject->CastToIAIActor())
				aiActor->SetSignal(1, signal, NULL, data, 0);
	}
}


// ===========================================================================
//	Send a signal to the AI actor.
//
//	In:		The signal name (NULL is invalid!)
//	In:		Optional signal context data that gets passed to Lua behavior 
//			scripts	(NULL will ignore).
//	In:		Option signal ID (or mode really). Should be a AISIGNAL_xxx
//			value.
//
void CGameAIInstanceBase::SendSignal(
	const char* signal, IAISignalExtraData* data, int nSignalID)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(GetEntityID());
	assert(entity != NULL);
	if (entity != NULL)
	{
		IAIObject* aiObject = entity->GetAI();
		if (aiObject != NULL)
		{
			IAIActor* aiActor = aiObject->CastToIAIActor();
			if (aiActor != NULL)
			{
				aiActor->SetSignal(nSignalID, signal, NULL, data, 0);
			}
		}
	}
}
