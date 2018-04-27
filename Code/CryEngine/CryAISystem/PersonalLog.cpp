// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PersonalLog.h"

void PersonalLog::AddMessage(const EntityId entityId, const char* message)
{
	if (m_messages.size() + 1 > 20)
		m_messages.pop_front();

	m_messages.push_back(message);

	if (gAIEnv.CVars.OutputPersonalLogToConsole)
	{
		const char* name = "(null)";

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId))
			name = entity->GetName();

		gEnv->pLog->Log("Personal Log [%s] %s", name, message);
	}

#ifdef CRYAISYSTEM_DEBUG
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId))
	{
		if (IAIObject* ai = entity->GetAI())
		{
			IAIRecordable::RecorderEventData recorderEventData(message);
			ai->RecordEvent(IAIRecordable::E_PERSONALLOG, &recorderEventData);
		}
	}
#endif
}
