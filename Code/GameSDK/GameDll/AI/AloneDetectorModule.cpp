// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ActorsPresenceAwarenessModule.cpp
//  Version:     v1.00
//  Created:     30/01/2012 by Francesco Roccucci.
//  Description: This is used for detecting the proximity of a specific 
//		 	set of AI agents to drive the selection of the behavior in
//				the tree without specific checks in the behavior itself
// -------------------------------------------------------------------------
//  History:
//	30/01/2012 12:00 - Added by Francesco Roccucci
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h" 
#include "AloneDetectorModule.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIObjectManager.h>
#include "Agent.h"


void AloneDetectorContainer::SetupDetector(const AloneDetectorSetup& setup)
{
	m_setup = setup;
} 

void AloneDetectorContainer::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if(m_entityClassNames.empty())
		return;

	Agent me(GetEntityID());
	const Vec3 entityPos = me.GetPos();	

	AutoAIObjectIter it(gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObjectInRange(OBJFILTER_GROUP,me.GetGroupID(),entityPos,m_setup.range,false));

	AloneDetectorSetup::State newState = AloneDetectorSetup::Alone;

	for(;it->GetObject();it->Next())
	{
		IAIObject* pAIObject = it->GetObject();

		Agent agent(pAIObject->GetEntityID());

		if( !IsActorValid(agent) )
			continue;

		string sEntityClassName = GetActorClassName(agent);
		EntitiesList::const_iterator entityIt = std::find(m_entityClassNames.begin(),m_entityClassNames.end(), sEntityClassName);
		if(entityIt != m_entityClassNames.end())
		{
			newState = AloneDetectorSetup::EntitiesInRange;
			break;
		}
	}

	if(m_setup.state != newState)
	{
		m_setup.state = newState;
		SendCorrectSignal();
	}
}

void AloneDetectorContainer::SendCorrectSignal()
{
	switch(m_setup.state)
	{
	case AloneDetectorSetup::Alone :
		SendSignal(m_setup.aloneSignal.c_str());	
		break;
	case AloneDetectorSetup::EntitiesInRange :
		SendSignal(m_setup.notAloneSignal.c_str());
		break;
	default:
		assert(0);
		break;
	}
}

void AloneDetectorContainer::AddEntityClass(const char* entityClassName)
{
	if(std::find(m_entityClassNames.begin(),m_entityClassNames.end(),entityClassName) == m_entityClassNames.end())
		m_entityClassNames.push_back(entityClassName);
}

void AloneDetectorContainer::RemoveEntityClass(const char* entityClassName)
{
	EntitiesList::iterator it = std::find(m_entityClassNames.begin(),m_entityClassNames.end(),entityClassName);
	if(it != m_entityClassNames.end())
		m_entityClassNames.erase(it);
}

void AloneDetectorContainer::ResetDetector()
{
	m_setup.state = AloneDetectorSetup::Unkown;
	m_entityClassNames.clear();
}

bool AloneDetectorContainer::IsActorValid(const Agent& agent) const
{
	const bool isValid = agent.IsValid() && !agent.IsHidden() && agent.IsEnabled() && !agent.IsDead();
	return isValid;
}

const char* AloneDetectorContainer::GetActorClassName(const Agent& agent) const
{
	return (agent.GetEntity() && agent.GetEntity()->GetClass()) ? agent.GetEntity()->GetClass()->GetName() : "";
}

bool AloneDetectorContainer::IsAlone() const
{
	return m_setup.state == AloneDetectorSetup::EntitiesInRange;
}
