// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#define CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE 12

class TFlowInputData;

//! Represents an event.
typedef uint32 TCustomEventId;

//! Invalid event id.
static const TCustomEventId CUSTOMEVENTID_INVALID = 0;

//! Enum to limit the ways the event can be used to avoid overpopulating flowgraph inputs and outputs.
enum EPrefabEventType
{
	ePrefabEventType_InOut = 0,
	ePrefabEventType_In,
	ePrefabEventType_Out,
};


//! Custom event listener.
struct ICustomEventListener
{
	// <interfuscator:shuffle>
	virtual ~ICustomEventListener(){}
	virtual void OnCustomEvent(const TCustomEventId eventId, const TFlowInputData& customData) = 0;
	// </interfuscator:shuffle>
};

//! Custom event manager interface.
struct ICustomEventManager
{
	// <interfuscator:shuffle>
	virtual ~ICustomEventManager(){}

	//! Registers custom event listener.
	virtual bool RegisterEventListener(ICustomEventListener* pListener, const TCustomEventId eventId) = 0;

	//! Unregisters custom event listener.
	virtual bool UnregisterEventListener(ICustomEventListener* pListener, const TCustomEventId eventId) = 0;

	//! Unregisters all listeners associated to an event.
	virtual bool UnregisterEvent(TCustomEventId eventId) = 0;

	//! Clear event data.
	virtual void Clear() = 0;

	//! Fires custom event.
	virtual void FireEvent(const TCustomEventId eventId, const TFlowInputData& customData) = 0;

	//! Gets next free event id.
	virtual TCustomEventId GetNextCustomEventId() = 0;
	// </interfuscator:shuffle>
};

//! \endcond