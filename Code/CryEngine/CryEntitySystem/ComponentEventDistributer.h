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

#pragma once

#ifndef __ComponentEventDistributer_h__
	#define __ComponentEventDistributer_h__

	#include <CryEntitySystem/IEntitySystem.h>
	#include <CryEntitySystem/IEntityComponent.h>
	#include <CryCore/CryFlags.h>

class CComponentEventDistributer : public IComponentEventDistributer
{
public:
	enum EEventUpdatePolicy
	{
		EEventUpdatePolicy_UseDistributer = BIT(0),
		EEventUpdatePolicy_SortByPriority = BIT(1),
		EEventUpdatePolicy_AlwaysSort     = BIT(2)
	};

	struct SEventPtr
	{
		EntityId      m_entityID;
		IEntityComponentPtr m_pComponent;

		bool operator==(const SEventPtr& rhs) const { return((m_entityID == rhs.m_entityID) && (m_pComponent == rhs.m_pComponent)); }

		SEventPtr()
			: m_entityID(0)
		{}
		SEventPtr(EntityId entityID, IEntityComponentPtr pComponent)
			: m_pComponent(pComponent)
			, m_entityID(entityID)
		{}
	};

	typedef std::vector<SEventPtr> TEventPtrs;
	struct SEventPtrs
	{
		TEventPtrs m_eventPtrs;
		int        m_lastSortedEvent;
		bool       m_bDirty : 1;

		SEventPtrs() : m_bDirty(true), m_lastSortedEvent(-1) {}
		SEventPtrs(const SEventPtrs& eventPtrs)
			: m_eventPtrs(eventPtrs.m_eventPtrs)
			, m_lastSortedEvent(eventPtrs.m_lastSortedEvent)
			, m_bDirty(eventPtrs.m_bDirty)
		{}
		bool operator==(const SEventPtrs& rhs) const
		{
			return((m_bDirty == rhs.m_bDirty) && (m_eventPtrs == rhs.m_eventPtrs));
		}
	};
	typedef std::map<int, SEventPtrs> TComponentContainer;

	explicit CComponentEventDistributer(const int flags);
	TComponentContainer& GetEventContainer() { return m_componentDistributer; }
	void                 SetFlags(int flags);
	ILINE bool           IsEnabled() const   { return m_flags.AreAllFlagsActive(EEventUpdatePolicy_UseDistributer); }

	void                 EnableEventForEntity(const EntityId id, const int eventID, const bool enable);
	void                 RegisterComponent(const EntityId entityID, IEntityComponentPtr pComponent, bool bEnable);
	void                 SendEvent(const SEntityEvent& event);
	void                 RemapEntityID(EntityId oldID, EntityId newID);
	void                 OnEntityDeleted(IEntity* piEntity);

	void                 Reset();

protected:

	virtual void RegisterEvent(const EntityId entityID, IEntityComponentPtr pComponent, const int eventID, const int flags);

private:

	typedef std::set<int> TRegisteredEvents;
	struct SRegisteredComponentEvents
	{
		SRegisteredComponentEvents(IEntityComponentPtr pComponent) : m_pComponent(pComponent) {}

		IEntityComponentPtr     m_pComponent;
		TRegisteredEvents m_registeredEvents;
	};

	typedef std::map<EntityId, EntityId>                        TMappedEntityIDContainer;
	typedef std::multimap<EntityId, SRegisteredComponentEvents> TComponentRegistrationContainer;
	TComponentRegistrationContainer m_componentRegistration;
	TComponentContainer             m_componentDistributer;
	TMappedEntityIDContainer        m_mappedEntityIDs;
	TEventPtrs                      m_eventPtrTemp;
	CCryFlags<uint>                 m_flags;

	void     ErasePtr(TEventPtrs& eventPtrs, IEntityComponentPtr pComponent);
	void     EnableEventForComponent(EntityId entityID, IEntityComponentPtr pComponent, const int eventID, bool bEnable);
	EntityId GetAndEraseMappedEntityID(const EntityId entityID);
};

#endif //__ComponentEventDistributer_h__
