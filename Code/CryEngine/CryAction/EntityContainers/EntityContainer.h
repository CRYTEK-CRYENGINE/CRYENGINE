// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Description: A container to gather entities and applying actions to all of them at once
// - 11/09/2015 Created by Dario Sancho

#pragma once

#include <CryCore/Containers/CryListenerSet.h>
#include "IEntityContainerListener.h"

#if !defined(_RELEASE)
	#define ENTITY_CONTAINER_LOG(...) if (CEntityContainer::CVars::IsEnabled(CEntityContainer::CVars::eDebugFlag_ShowLog)) { CEntityContainer::Log(stack_string().Format("[Info][EntityContainer] " __VA_ARGS__).c_str()); }
#else
	#define ENTITY_CONTAINER_LOG(...) ((void)0)
#endif

class CEntityContainer
{
	friend class CEntityContainerMgr;

public:

	// Unit of information stored per entity in a container
	// Currently just the EntityId but there is potential for other useful data to be stored here
	// Let's define the type we will use as argument in functions when passing an SEntityInfo around
	struct SEntityInfo;
	typedef SEntityInfo TSentityInfoParam;
	// if the amount of data was to grow significantly we should then use the following definition:
	//typedef const SEntityInfo & TSentityInfoParam;

	struct SEntityInfo
	{
		SEntityInfo() : id(0) {}
		SEntityInfo(EntityId id_) : id(id_) {}
		friend bool operator<(TSentityInfoParam a, TSentityInfoParam b)  { return a.id < b.id; }
		friend bool operator==(TSentityInfoParam a, TSentityInfoParam b) { return a.id == b.id; }

		EntityId id;
	};

	typedef EntityId                                ContainerId;
	typedef CListenerSet<IEntityContainerListener*> TEntityContainerListeners;
	typedef std::vector<SEntityInfo>                TEntitiesInContainer;

private:

	CEntityContainer() : m_containerId(0), m_szName(nullptr), m_pListeners(nullptr) {}
	CEntityContainer(EntityId containerId, const TEntityContainerListeners* pListeners, const char* szContainerName = nullptr);

	EntityId GetId() const   { return m_containerId; }
	size_t   GetSize() const { return m_entities.size(); }
	bool     IsEmpty() const { return m_entities.empty(); }
	bool     IsInContainer(EntityId entityId) const;

	// adding and removing entities sends notifications to the container listeners
	size_t                      AddEntity(TSentityInfoParam entityId);
	size_t                      RemoveEntity(EntityId entityId);
	size_t                      AddEntitiesFromContainer(const CEntityContainer& srcContainer);
	void                        Clear();

	const TEntitiesInContainer& GetEntities() const { return m_entities; }

	// Notify listeners when an event is triggered in the container (e.g. entity added, removed, container empty)
	void NotifyListeners(EntityId containerId, EntityId entityId, IEntityContainerListener::eEventType eventType, size_t containerSize);

private:

	// Debug / Log related
	struct CVars
	{
		enum EDebugFlags
		{
			eDebugFlag_ShowInfo        = BIT(0),
			eDebugFlag_InEditor        = BIT(1),
			eDebugFlag_ShowConnections = BIT(2),
			eDebugFlag_ShowLog         = BIT(3)
		};

		static bool IsEnabled(EDebugFlags f) { return (ms_es_debugContainers & (eDebugFlag_ShowInfo | f)) == (eDebugFlag_ShowInfo | f); }
		static int  ms_es_debugContainers;
	};

	const char* GetName() const { return m_szName; }
	static void Log(const char* szMessage);

private:

	EntityId                         m_containerId;  // container id
	TEntitiesInContainer             m_entities;     // list of entities in the container
	Vec3                             m_containerPos; // container position
	const char*                      m_szName;
	const TEntityContainerListeners* m_pListeners;   // Pointer to listeners owned by EntityContainerMgr
};
