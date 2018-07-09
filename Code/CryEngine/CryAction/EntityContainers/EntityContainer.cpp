// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Description: A Group to gather entities and applying actions to all of them at once
// - 11/09/2015 Created by Dario Sancho

#include "StdAfx.h"
#include "EntityContainer.h"


int CEntityContainer::CVars::ms_es_debugContainers = 0;


CEntityContainer::CEntityContainer(EntityId containerId, const TEntityContainerListeners* pListeners, const char* szContainerName /* = nullptr */)
	: m_containerId(containerId)
	, m_pListeners(pListeners)
	, m_szName(szContainerName)
{
}


bool CEntityContainer::IsInContainer(EntityId entityId) const
{
	if (entityId > 0 && IsEmpty() == false)
	{
		auto const it = std::find_if(m_entities.begin(), m_entities.end(), [entityId](TSentityInfoParam a) { return a.id == entityId; });
		return it != m_entities.end();
	}
	return false;
}


void CEntityContainer::Clear()
{
	while (m_entities.empty() == false)
	{
		TSentityInfoParam entityInfo = m_entities.back();
		RemoveEntity(entityInfo.id);
	}

	CRY_ASSERT(m_entities.empty());
	ENTITY_CONTAINER_LOG("EntityContainer [%d] Cleared", m_containerId);
}


size_t CEntityContainer::AddEntity(TSentityInfoParam entityInfo)
{
	if (!IsInContainer(entityInfo.id))
	{
		m_entities.push_back(entityInfo);

		NotifyListeners(m_containerId, entityInfo.id, IEntityContainerListener::eET_EntityAdded, GetSize());

		ENTITY_CONTAINER_LOG("EntityContainer [%d] - Added Entity [%d] - Total members: [%d]", m_containerId, entityInfo.id, m_entities.size());
	}
	return GetSize();
}


size_t CEntityContainer::RemoveEntity(EntityId entityId)
{
	auto it = std::find_if(m_entities.begin(), m_entities.end(), [entityId](TSentityInfoParam a) { return a.id == entityId; });
	if (it != m_entities.end())
	{
		m_entities.erase(it);

		NotifyListeners(m_containerId, entityId, IEntityContainerListener::eET_EntityRemoved, GetSize());
		ENTITY_CONTAINER_LOG("EntityContainer [%d] - Removed Entity [%d] - Total members: [%d]", m_containerId, entityId, GetSize());
		if (m_entities.empty())
		{
			NotifyListeners(m_containerId, entityId, IEntityContainerListener::eET_ContainerEmpty, GetSize());
			ENTITY_CONTAINER_LOG("EntityContainer [%d] is empty", m_containerId);
		}
	}
	return GetSize();
}


void CEntityContainer::NotifyListeners(EntityId containerId, EntityId entityId, IEntityContainerListener::eEventType eventType, size_t containerSize)
{
	if (m_pListeners)
	{
		TEntityContainerListeners& listeners = const_cast<TEntityContainerListeners&>(*m_pListeners);
		for (TEntityContainerListeners::Notifier notifier(listeners); notifier.IsValid(); notifier.Next())
		{
			IEntityContainerListener::SChangeInfo info;
			info.entityId = entityId;
			info.containerId = containerId;
			info.eventType = eventType;
			info.containerSize = containerSize;
			notifier->OnGroupChange(info);
		}
	}
}

// ------------------------------------------------------------------------
size_t CEntityContainer::AddEntitiesFromContainer(const CEntityContainer& srcContainer)
{
	for (TSentityInfoParam srcEntityInfo : srcContainer.GetEntities())
	{
		AddEntity(srcEntityInfo);
	}

	return GetSize();
}

// ------------------------------------------------------------------------
/* static */ void CEntityContainer::Log(const char* szMessage)
{
	CryLog(szMessage);
}
