// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Entity Group Manager. Used for managing groups of entities
// and applying certain actions to all of them at once.
// - 10/02/2016 Created by Dario Sancho

#pragma once

#include <unordered_map>
#include <CryCore/Containers/CryListenerSet.h>
#include "EntityContainer.h"
#include "IEntityContainerListener.h"

class CEntityContainerMgr
{
public:

	CEntityContainerMgr();
	~CEntityContainerMgr();

	// Container Management
	bool     CreateContainer(EntityId containerId, const char* szContainerName = nullptr);             // szContainerName is stored as const char * , so do not pass temporary strings
	void     DestroyContainer(EntityId containerId);
	void     ClearContainer(EntityId containerId);                                                     // Removes all entities from the selected container. Notifications are sent
	size_t   MergeContainers(EntityId srcContainerId, EntityId dstContainerId, bool clearSrc = true);          // Copies all elements from srcContainerId into dstContainerId and by default clears the src container. Returns the size of the destination container after the merge
	size_t   AddEntity(EntityId containerId, EntityId entityId);                                       // Adds an entity to the selected container. Returns the new size of the container. Notifications are sent
	size_t   RemoveEntity(EntityId containerId, EntityId entityId);                                    // Removes an entity from the selected container. Returns the new size of the container. Notifications are sent
	size_t   AddContainerOfEntitiesToContainer(EntityId tgtContainerId, EntityId srcContainerId);      // Adds all the entities in srcContainer into tgtContainer. Returns the new size of tgtContainer. Notifications are sent
	size_t   RemoveContainerOfEntitiesFromContainer(EntityId tgtContainerId, EntityId srcContainerId); // Remove all the entities in srcContainer from tgtContainer. Returns the new size of tgtContainer. Notifications are sent
	void     RemoveEntityFromAllContainers(EntityId entityId);                                         // Removes an entity from all the containers it belongs to. Notifications are sent
	EntityId GetEntitysContainerId(EntityId entityId) const;                                           // Returns the ContainerId that the selected entity belongs to or 0 if not in a container
	void     GetEntitysContainerIds(EntityId entityId, CEntityContainer::TEntitiesInContainer& resultIdArray) const; // Fills resultIdArray with the ContainerIds of all the containers that the selected entity belongs to

	size_t   GetNumberOfContainers() const { return m_containers.size(); }
	size_t   GetContainerSize(EntityId containerId) const;        // Returns the number of entities in the selected container
	bool     IsContainer(EntityId entityId) const;                // Returns whether the entity passed in is a container

	// Group Action
	bool RunModule(EntityId containerId, const char* moduleName); // Executes a Flowgraph module for each entity in the selected container
	void SetHideEntities(EntityId containerId, bool hide);        // Hides/Unhides all entities in the selected container
	void RemoveEntitiesFromTheGame(EntityId containerId);         // All entities in the given container will be removed from the game (if spawned) or hidden (if level entities)

	// Group Query
	EntityId GetRandomEntity(EntityId containerId) const;         // Returns a random entity from the selected container
	// Fill up oArrayOfEntities with the EnitityIds of the container members. Returns true if container exists false otherwise
	bool     GetEntitiesInContainer(EntityId containerId, CEntityContainer::TEntitiesInContainer& oArrayOfEntities, bool addOriginalEntityIfNotContainer = false) const;

	// Notifications (set bStaticName = false if szName points to a temporary buffer)
	void RegisterListener(IEntityContainerListener* pListener, const char* szName, bool bStaticName = true) { m_listeners.Add(pListener, szName, bStaticName); }
	void UnregisterListener(IEntityContainerListener* pListener)                                            { m_listeners.Remove(pListener); }

	// Update (so far only to show debug info) : TODO do this some other way
	void Update();

private:

	void DebugRender(EntityId containerId);
	void DebugRender(EntityId containerId, const Vec3& srcPos);

	// Removes all entities from all containers. Notifications are sent.
	void Reset();
	// Takes care of cleaning up after exiting the editor mode (TODO?)

	// Returns the pointer to the container identified by the specified EntityId or nullptr if this container doesn't exist
	CEntityContainer*       GetContainer(EntityId containerId);
	const CEntityContainer* GetContainerConst(EntityId containerId) const;


	typedef std::unordered_map<EntityId, CEntityContainer> TContainerMap;
	TContainerMap m_containers;
	CEntityContainer::TEntityContainerListeners m_listeners;
};
