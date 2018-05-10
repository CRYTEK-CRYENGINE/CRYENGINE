// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CustomEvents/CustomEventsManager.h"

///////////////////////////////////////////////////
// Manager implementation to associate custom events
///////////////////////////////////////////////////

//------------------------------------------------------------------------------------------------------------------------
CCustomEventManager::CCustomEventManager()
	: m_curHighestEventId(CUSTOMEVENTID_INVALID)
{
}

//------------------------------------------------------------------------------------------------------------------------
CCustomEventManager::~CCustomEventManager()
{
	Clear();
}

//------------------------------------------------------------------------------------------------------------------------
bool CCustomEventManager::RegisterEventListener(ICustomEventListener* pListener, const TCustomEventId eventId)
{
	SCustomEventData& eventData = m_customEventsData[eventId];   // Creates new entry if doesn't exist
	TCustomEventListeners& listeners = eventData.m_listeners;
	return listeners.Add(pListener);
}

//------------------------------------------------------------------------------------------------------------------------
bool CCustomEventManager::UnregisterEventListener(ICustomEventListener* pListener, const TCustomEventId eventId)
{
	TCustomEventsMap::iterator eventsDataIter = m_customEventsData.find(eventId);
	if (eventsDataIter != m_customEventsData.end())
	{
		SCustomEventData& eventData = eventsDataIter->second;
		TCustomEventListeners& listeners = eventData.m_listeners;

		if (listeners.Contains(pListener))
		{
			listeners.Remove(pListener);
			return true;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CCustomEventManager::UnregisterEventListener: Listener isn't registered for event id: %u", eventId);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CCustomEventManager::UnregisterEventListener: No event data exists for event id: %u", eventId);
	}

	return false;
}

//------------------------------------------------------------------------------------------------------------------------
bool CCustomEventManager::UnregisterEvent(TCustomEventId eventId)
{
	TCustomEventsMap::iterator eventsDataIter = m_customEventsData.find(eventId);
	if (eventsDataIter != m_customEventsData.end())
	{
		SCustomEventData& eventData = eventsDataIter->second;
		TCustomEventListeners& listeners = eventData.m_listeners;
		listeners.Clear(true);
		m_customEventsData.erase(eventsDataIter);
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------------------------------------------------
void CCustomEventManager::Clear()
{
	m_customEventsData.clear();
	m_curHighestEventId = CUSTOMEVENTID_INVALID;
}

//------------------------------------------------------------------------------------------------------------------------
void CCustomEventManager::FireEvent(const TCustomEventId eventId, const TFlowInputData& customData)
{
	TCustomEventsMap::iterator eventsDataIter = m_customEventsData.find(eventId);
	if (eventsDataIter != m_customEventsData.end())
	{
		SCustomEventData& eventData = eventsDataIter->second;
		TCustomEventListeners& listeners = eventData.m_listeners;
		listeners.ForEachListener([&](ICustomEventListener* pListener){ pListener->OnCustomEvent(eventId, customData); });
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CCustomEventManager::FireEvent: No event data exists for event id: %u", eventId);
	}
}

//------------------------------------------------------------------------------------------------------------------------
TCustomEventId CCustomEventManager::GetNextCustomEventId()
{
	const TCustomEventId curNextId = m_curHighestEventId + 1;
	m_curHighestEventId = curNextId;
	return curNextId;
}
