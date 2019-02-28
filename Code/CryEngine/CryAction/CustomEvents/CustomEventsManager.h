// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CUSTOMEVENTMANAGER_H_
#define _CUSTOMEVENTMANAGER_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAction/ICustomEvents.h>
#include <CryCore/Containers/CryListenerSet.h>

///////////////////////////////////////////////////
// Manager implementation to associate custom events
///////////////////////////////////////////////////
class CCustomEventManager : public ICustomEventManager
{
public:
	CCustomEventManager();
	virtual ~CCustomEventManager();

public:
	// ICustomEventManager
	virtual bool           RegisterEventListener(ICustomEventListener* pListener, const TCustomEventId eventId) override;
	virtual bool           UnregisterEventListener(ICustomEventListener* pListener, const TCustomEventId eventId) override;
	virtual bool           UnregisterEvent(TCustomEventId eventId) override;
	virtual void           Clear() override;
	virtual void           FireEvent(const TCustomEventId eventId, const TFlowInputData& customData) override;
	virtual TCustomEventId GetNextCustomEventId() override;
	// ~ICustomEventManager

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CustomEventManager");

		pSizer->AddObject(this, sizeof(*this));
	}

private:
	typedef CListenerSet<ICustomEventListener*> TCustomEventListeners;

	struct SCustomEventData
	{
		SCustomEventData()
			: m_listeners(0)
		{}

		TCustomEventListeners m_listeners;
	};

	// Mapping of custom event id to listeners
	typedef std::map<TCustomEventId, SCustomEventData> TCustomEventsMap;
	TCustomEventsMap m_customEventsData;
	TCustomEventId   m_curHighestEventId;
};

#endif
