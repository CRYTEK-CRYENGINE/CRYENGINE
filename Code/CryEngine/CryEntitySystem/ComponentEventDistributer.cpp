// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   EntityEventDistributer.h
//  Version:     v1.00
//  Created:     14/06/2012 by Steve North
//  Compilers:   Visual Studio.NET 2010
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "ComponentEventDistributer.h"

//#define DEBUG_EVENTEVENTS

namespace
{
struct FEventSend
{
	explicit FEventSend(const SEntityEvent& event)
		: m_event(event)
	{}

	void operator()(const CComponentEventDistributer::SEventPtr& eventPtr) const
	{
		CRY_ASSERT_MESSAGE(gEnv->pEntitySystem->GetEntity(eventPtr.m_entityID), "Entity doesn't exist, ");

		eventPtr.m_pComponent->ProcessEvent(const_cast<SEntityEvent&>(m_event));
	}

private:
	SEntityEvent m_event;
};

struct FEntityEventSort
{
	explicit FEntityEventSort(const int eventID) : m_eventID(eventID) {}
	bool operator()(const CComponentEventDistributer::SEventPtr& lhs, const CComponentEventDistributer::SEventPtr& rhs) const
	{
		if (lhs.m_pComponent->GetEventPriority(m_eventID) < rhs.m_pComponent->GetEventPriority(m_eventID))
		{
			return false;
		}

		if (lhs.m_pComponent->GetEventPriority(m_eventID) > rhs.m_pComponent->GetEventPriority(m_eventID))
		{
			return true;
		}

		if (lhs.m_entityID < rhs.m_entityID)
		{
			return false;
		}

		if (lhs.m_entityID > rhs.m_entityID)
		{
			return true;
		}

		return false;
	}
private:
	int m_eventID;
};

struct FEntityEventSortByEntityFirst
{
	explicit FEntityEventSortByEntityFirst(const int eventID) : m_eventID(eventID) {}
	bool operator()(const CComponentEventDistributer::SEventPtr& lhs, const CComponentEventDistributer::SEventPtr& rhs) const
	{
		if (lhs.m_entityID < rhs.m_entityID)
		{
			return false;
		}

		if (lhs.m_entityID > rhs.m_entityID)
		{
			return true;
		}

		if (lhs.m_pComponent->GetEventPriority(m_eventID) < rhs.m_pComponent->GetEventPriority(m_eventID))
		{
			return false;
		}

		if (lhs.m_pComponent->GetEventPriority(m_eventID) > rhs.m_pComponent->GetEventPriority(m_eventID))
		{
			return true;
		}

		return false;
	}
private:
	int m_eventID;
};

}

CComponentEventDistributer::CComponentEventDistributer(const int flags)
{
	m_eventPtrTemp.reserve(256);

	SetFlags(flags);
}

void CComponentEventDistributer::RegisterEvent(const EntityId entityID, IEntityComponentPtr pComponent, const int eventID, const int flags)
{
	TComponentRegistrationContainer::iterator it = m_componentRegistration.find(entityID);
	const TComponentRegistrationContainer::const_iterator iEnd = m_componentRegistration.end();
	const bool bEnable = ((flags& IEntityComponent::EComponentFlags_Enable) != 0);
	if (bEnable && !pComponent->GetFlags().AreAllFlagsActive(IEntityComponent::EComponentFlags_IsRegistered))
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
		pEntity->RegisterComponent(pComponent, flags);
	}
	for (; (it != iEnd) && (it->first == entityID); ++it)
	{
		if (it->second.m_pComponent == pComponent)
		{
			if (bEnable)
			{
				it->second.m_registeredEvents.insert(eventID);
			}
			else
			{
				it->second.m_registeredEvents.erase(eventID);
			}
			break;
		}
	}

	EnableEventForComponent(entityID, pComponent, eventID, bEnable);
}

void CComponentEventDistributer::SendEvent(const SEntityEvent& event)
{
	TComponentContainer::iterator it = m_componentDistributer.find(event.event);
	if (it != m_componentDistributer.end())
	{
		SEventPtrs& eventPtrs = it->second;

		if (m_flags.AreAllFlagsActive(EEventUpdatePolicy_AlwaysSort) || (eventPtrs.m_lastSortedEvent != event.event))
		{
			eventPtrs.m_bDirty = true;
		}

		if (eventPtrs.m_bDirty)
		{
			if (m_flags.AreAllFlagsActive(EEventUpdatePolicy_SortByPriority))
			{
				std::sort(eventPtrs.m_eventPtrs.begin(), eventPtrs.m_eventPtrs.end(), FEntityEventSortByEntityFirst(event.event));
			}
			else
			{
				std::sort(eventPtrs.m_eventPtrs.begin(), eventPtrs.m_eventPtrs.end(), FEntityEventSort(event.event));
			}
			eventPtrs.m_lastSortedEvent = event.event;
			eventPtrs.m_bDirty = false;
		}

		m_eventPtrTemp = eventPtrs.m_eventPtrs;

		std::for_each(m_eventPtrTemp.begin(), m_eventPtrTemp.end(), FEventSend(event));
	}
	m_eventPtrTemp.clear();
}

void CComponentEventDistributer::EnableEventForEntity(const EntityId entityID, const int eventID, const bool enable)
{
	TComponentRegistrationContainer::iterator it = m_componentRegistration.find(entityID);
	const TComponentRegistrationContainer::const_iterator iEnd = m_componentRegistration.end();
	for (; (it != iEnd) && (it->first == entityID); ++it)
	{
		if (it->second.m_registeredEvents.count(eventID) > 0)
		{
			EnableEventForComponent(entityID, it->second.m_pComponent, eventID, enable);
		}
	}
}

void CComponentEventDistributer::RegisterComponent(const EntityId entityID, IEntityComponentPtr pComponent, bool bEnable)
{
	if (bEnable)
	{
		TComponentRegistrationContainer::iterator it = m_componentRegistration.find(entityID);
		const TComponentRegistrationContainer::const_iterator iEnd = m_componentRegistration.end();
		for (; (it != iEnd) && (it->first == entityID); ++it)
		{
			if (it->second.m_pComponent == pComponent)
			{
				return;
			}
		}
		m_componentRegistration.insert(TComponentRegistrationContainer::value_type(entityID, pComponent));
		pComponent->SetDistributer(this, entityID);
		pComponent->GetFlags().SetFlags(IEntityComponent::EComponentFlags_IsRegistered, true);
	}
	else
	{
		const EntityId entityIDEntry = GetAndEraseMappedEntityID(entityID);

		EnableEventForComponent(entityIDEntry, pComponent, EVENT_ALL, false);

		TComponentRegistrationContainer::iterator it = m_componentRegistration.find(entityIDEntry);
		const TComponentRegistrationContainer::const_iterator iEnd = m_componentRegistration.end();
		for (; (it != iEnd) && (it->first == entityID); ++it)
		{
			if (it->second.m_pComponent == pComponent)
			{
				m_componentRegistration.erase(it);
				break;
			}
		}
		pComponent->SetDistributer(NULL, 0);
		pComponent->GetFlags().SetFlags(IEntityComponent::EComponentFlags_IsRegistered, false);

	}
}

void CComponentEventDistributer::EnableEventForComponent(const EntityId entityID, IEntityComponentPtr pComponent, const int event, bool bEnable)
{
	if (event == EVENT_ALL)
	{
		for (int iEvent = 0; iEvent < ENTITY_EVENT_LAST; ++iEvent)
		{
			EnableEventForComponent(entityID, pComponent, iEvent, bEnable);
		}
	}

	if (bEnable)
	{
		TComponentContainer::iterator it = m_componentDistributer.find(event);
		if (it != m_componentDistributer.end())
		{
			it->second.m_bDirty = true;
			SEventPtr eventPtr(entityID, pComponent);
			TEventPtrs& eventPtrs = it->second.m_eventPtrs;
			stl::push_back_unique(eventPtrs, eventPtr);
		}
		else
		{
			m_componentDistributer.insert(TComponentContainer::value_type(event, SEventPtrs()));
			EnableEventForComponent(entityID, pComponent, event, true);
		}
	}
	else
	{
		TComponentContainer::iterator it = m_componentDistributer.find(event);
		if (it != m_componentDistributer.end())
		{
			TEventPtrs& eventPtrs = it->second.m_eventPtrs;
			ErasePtr(eventPtrs, pComponent);

			if (eventPtrs.empty())
			{
				m_componentDistributer.erase(it);
			}
		}
	}
}

void CComponentEventDistributer::OnEntityDeleted(IEntity* piEntity)
{
	EntityId id = piEntity->GetId();

	// disable all events for entity.
	EnableEventForEntity(id, EVENT_ALL, false);

	// erase all registerations for entity ID.
	for (TComponentRegistrationContainer::iterator it = m_componentRegistration.find(id); it != m_componentRegistration.end(); )
	{
		it->second.m_pComponent->SetDistributer(NULL, 0);
		piEntity->RegisterComponent(it->second.m_pComponent, false);

		// 'it' is invalid now, so attempt to find it again, we're popping all entries with this loop, so it's valid.
		it = m_componentRegistration.find(id);
	}
}

void CComponentEventDistributer::ErasePtr(TEventPtrs& eventPtrs, IEntityComponentPtr pComponent)
{
	TEventPtrs::iterator iEventPtr = eventPtrs.begin();
	const TEventPtrs::const_iterator iEnd = eventPtrs.end();
	for (; iEventPtr != iEnd; ++iEventPtr)
	{
		if (iEventPtr->m_pComponent == pComponent)
		{
			eventPtrs.erase(iEventPtr);
			break;
		}
	}
}

void CComponentEventDistributer::SetFlags(int flags)
{
	m_flags.ClearAllFlags();
	m_flags.SetFlags(flags, true);

	TComponentContainer::iterator it = m_componentDistributer.begin();
	const TComponentContainer::const_iterator iEnd = m_componentDistributer.end();
	for (; it != iEnd; )
	{
		it->second.m_bDirty = true;
	}
}

void CComponentEventDistributer::RemapEntityID(EntityId oldID, EntityId newID)
{
	CRY_ASSERT_MESSAGE(m_mappedEntityIDs.size() <= 16, "More remapped entities than are in the EntityPool, did the EntityPool increase in size?");

	// Ensure all events are unregistered when unmapping.
	EnableEventForEntity(oldID, EVENT_ALL, false);

	TComponentRegistrationContainer::iterator iOld = m_componentRegistration.find(oldID);
	const TComponentRegistrationContainer::const_iterator iEnd = m_componentRegistration.end();
	for (; (iOld != iEnd) && (iOld->first == oldID); ++iOld)
	{
		m_componentRegistration.insert(TComponentRegistrationContainer::value_type(newID, iOld->second));
	}
	m_componentRegistration.erase(oldID);

	bool bShouldInsertID = true;
	TMappedEntityIDContainer::iterator iMapped = m_mappedEntityIDs.begin();
	const TMappedEntityIDContainer::const_iterator iEndMapped = m_mappedEntityIDs.end();
	for (; iMapped != iEndMapped; ++iMapped)
	{
		if (iMapped->second == oldID)
		{
			if (iMapped->first == newID)
			{
				// just mapping back to the original - erase!
				m_mappedEntityIDs.erase(iMapped);

				bShouldInsertID = false;

				break;
			}
			else
			{
				// It's possible for a remap to happen before the ID was unmapped, so need to handle this.
				// remapped the remap, just update the remapped ID!
				// NOTE: iMapped->first is still embedded inside the EventProxy
				iMapped->second = newID;

				bShouldInsertID = false;

				break;
			}
		}
	}
	if (bShouldInsertID)
	{
		CRY_ASSERT(m_mappedEntityIDs.count(oldID) == 0);

		m_mappedEntityIDs.insert(TMappedEntityIDContainer::value_type(oldID, newID));
	}
}

EntityId CComponentEventDistributer::GetAndEraseMappedEntityID(const EntityId entityID)
{
	TMappedEntityIDContainer::iterator iMapped = m_mappedEntityIDs.begin();
	const TMappedEntityIDContainer::const_iterator iEnd = m_mappedEntityIDs.end();

	for (; iMapped != iEnd; ++iMapped)
	{
		if ((iMapped->first == entityID) || (iMapped->second == entityID))
		{
			const EntityId mappedID = iMapped->second;

			m_mappedEntityIDs.erase(iMapped);

			return mappedID;
		}
	}

	return entityID;
}

void CComponentEventDistributer::Reset()
{
	stl::free_container(m_mappedEntityIDs);
	stl::free_container(m_componentRegistration);
	stl::free_container(m_componentDistributer);
	stl::free_container(m_eventPtrTemp);
}
