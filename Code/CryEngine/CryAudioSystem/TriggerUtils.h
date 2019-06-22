// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"

namespace CryAudio
{
void SendFinishedTriggerInstanceRequest(
	ControlId const triggerId,
	EntityId const entityId,
	ERequestFlags const flags = ERequestFlags::None,
	void* const pOwnerOverride = nullptr,
	void* const pUserData = nullptr,
	void* const pUserDataOwner = nullptr);

void ExecuteDefaultTriggerConnections(Control const* const pControl, TriggerConnections const& connections);

void ConstructGlobalTriggerInstance(
	ControlId const triggerId,
	uint16 const numPlayingConnectionInstances,
	uint16 const numPendingConnectionInstances,
	ERequestFlags const flags,
	void* const pOwner,
	void* const pUserData,
	void* const pUserDataOwner);
} // namespace CryAudio
