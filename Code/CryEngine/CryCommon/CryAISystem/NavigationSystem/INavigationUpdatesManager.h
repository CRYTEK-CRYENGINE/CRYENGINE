// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct INavigationUpdatesManager
{
	virtual ~INavigationUpdatesManager() {}

	virtual void Update() = 0;

	virtual void EntityChanged(int physicalEntityId, const AABB& aabb) = 0;
	virtual void WorldChanged(const AABB& aabb) = 0;

	virtual void RequestGlobalUpdate() = 0;
	virtual void RequestGlobalUpdateForAgentType(NavigationAgentTypeID agentTypeID) = 0;

	//! Allow regeneration requests to be executed. 
	//! If updateChangedVolumes parameter is false, pending NavMesh changes, that weren't updated while navigation generation was disabled, won't be applied.
	virtual void EnableRegenerationRequestsExecution(bool updateChangedVolumes) = 0;

	//! Block regeneration requests from being executed.
	//! note: they will be buffered, but not implicitly executed when they are allowed again
	virtual void DisableRegenerationRequestsAndBuffer() = 0;
	virtual bool AreRegenerationRequestsDisabled() const = 0;

	//! Returns true if there are any buffered regeneration requests
	virtual bool HasBufferedRegenerationRequests() const = 0;

	//! Moves buffered regeneration requests to the active requests queue
	virtual void ApplyBufferedRegenerationRequests() = 0;

	//! Clear the buffered regeneration requests that were received when execution was disabled
	virtual void ClearBufferedRegenerationRequests() = 0;

	virtual bool WasRegenerationRequestedThisCycle() const = 0;
	virtual void ClearRegenerationRequestedThisCycleFlag() = 0;

	virtual void EnableUpdatesAfterStabilization() = 0;
	virtual void DisableUpdatesAfterStabilization() = 0;
};