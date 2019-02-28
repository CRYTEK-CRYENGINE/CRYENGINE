// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeGraft.h"
#include "BehaviorTreeManager.h"
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

namespace BehaviorTree
{
void GraftManager::Reset()
{
	m_activeGraftNodes.clear();
	m_graftModeRequests.clear();
	m_graftBehaviorRequests.clear();
}

void GraftManager::GraftNodeReady(EntityId entityId, IGraftNode* graftNode)
{
	std::pair<ActiveGraftNodesContainer::iterator, bool> insertResult = m_activeGraftNodes.insert(std::make_pair(entityId, graftNode));
	if (!insertResult.second)
	{
		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		gEnv->pLog->LogError("Graft Manager: More than one graft node is running for the entity '%s'", entity->GetName());
		return;
	}

	GraftModeRequestsContainer::iterator requestIt = m_graftModeRequests.find(entityId);
	if (requestIt != m_graftModeRequests.end() && requestIt->second != NULL)
	{
		requestIt->second->GraftModeReady(entityId);
	}
	else
	{
		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		gEnv->pLog->LogError("Graft Manager: A graft node is running for the entity '%s' without a corresponding request.", entity->GetName());
	}
}

void GraftManager::GraftNodeTerminated(EntityId entityId)
{
	GraftModeRequestsContainer::iterator graftModeRequestIt = m_graftModeRequests.find(entityId);
	if (graftModeRequestIt != m_graftModeRequests.end() && graftModeRequestIt->second != NULL)
	{
		graftModeRequestIt->second->GraftModeInterrupted(entityId);
		m_graftModeRequests.erase(graftModeRequestIt);
	}

	ActiveGraftNodesContainer::iterator activeGraftNodeIt = m_activeGraftNodes.find(entityId);
	if (activeGraftNodeIt != m_activeGraftNodes.end())
	{
		m_activeGraftNodes.erase(activeGraftNodeIt);
	}

	m_graftBehaviorRequests.erase(entityId);
}

void GraftManager::GraftBehaviorComplete(EntityId entityId)
{
	GraftBehaviorRequestsContainer::iterator graftBehaviorRequestIt = m_graftBehaviorRequests.find(entityId);
	if (graftBehaviorRequestIt != m_graftBehaviorRequests.end() && graftBehaviorRequestIt->second != NULL)
	{
		graftBehaviorRequestIt->second->GraftBehaviorComplete(entityId);
	}
	else
	{
		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		gEnv->pLog->LogError("Graft Manager: A graft behavior is complete for the entity '%s' without a corresponding request.", entity->GetName());
	}
}

bool GraftManager::RunGraftBehavior(EntityId entityId, const char* behaviorName, XmlNodeRef behaviorXml, IGraftBehaviorListener* listener)
{
	m_graftBehaviorRequests[entityId] = listener;

	ActiveGraftNodesContainer::iterator activeGraftNodeIt = m_activeGraftNodes.find(entityId);
	if (activeGraftNodeIt != m_activeGraftNodes.end())
	{
		return activeGraftNodeIt->second->RunBehavior(entityId, behaviorName, behaviorXml);
	}

	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
	gEnv->pLog->LogError("Graft Manager: There is no active graft node to run the behavior '%s' by the entity '%s'.", behaviorName, entity->GetName());
	return false;
}

bool GraftManager::RequestGraftMode(EntityId entityId, IGraftModeListener* listener)
{
	assert(entityId);

	GraftModeRequestsContainer::iterator graftModeRequestIt = m_graftModeRequests.find(entityId);
	const bool graftModeAlreadyRequested = graftModeRequestIt != m_graftModeRequests.end();
	if (graftModeAlreadyRequested)
	{
		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		gEnv->pLog->LogError("Graft Manager: More the one graft mode request was performed for the entity '%s'", entity->GetName());
		return false;
	}
	else
	{
		m_graftModeRequests.insert(std::make_pair(entityId, listener));
		Event graftRequestEvent("OnGraftRequested");
		gAIEnv.pBehaviorTreeManager->HandleEvent(entityId, graftRequestEvent);
		return true;
	}
}

void GraftManager::CancelGraftMode(EntityId entityId)
{
	m_graftModeRequests.erase(entityId);
	Event graftRequestEvent("OnGraftCanceled");
	gAIEnv.pBehaviorTreeManager->HandleEvent(entityId, graftRequestEvent);
}
}
