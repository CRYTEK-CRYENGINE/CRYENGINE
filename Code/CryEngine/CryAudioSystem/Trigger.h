// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CTriggerInstance;

class CTrigger final : public Control, public CPoolObject<CTrigger, stl::PSyncNone>
{
public:

	CTrigger() = delete;
	CTrigger(CTrigger const&) = delete;
	CTrigger(CTrigger&&) = delete;
	CTrigger& operator=(CTrigger const&) = delete;
	CTrigger& operator=(CTrigger&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CTrigger(
		ControlId const id,
		ContextId const contextId,
		TriggerConnections const& connections,
		float const radius,
		char const* const szName)
		: Control(id, contextId, szName)
		, m_connections(connections)
		, m_radius(radius)
	{}
#else
	explicit CTrigger(
		ControlId const id,
		ContextId const contextId,
		TriggerConnections const& connections)
		: Control(id, contextId)
		, m_connections(connections)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CTrigger();

	void Execute(
		CObject& object,
		void* const pOwner = nullptr,
		void* const pUserData = nullptr,
		void* const pUserDataOwner = nullptr,
		ERequestFlags const flags = ERequestFlags::None,
		EntityId const entityId = INVALID_ENTITYID) const;

	void Execute(
		void* const pOwner = nullptr,
		void* const pUserData = nullptr,
		void* const pUserDataOwner = nullptr,
		ERequestFlags const flags = ERequestFlags::None) const;

	void Stop(Impl::IObject* const pIObject) const;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	void Execute(
		CObject& object,
		TriggerInstanceId const triggerInstanceId,
		CTriggerInstance* const pTriggerInstance,
		uint16 const triggerCounter) const;

	void Execute(
		TriggerInstanceId const triggerInstanceId,
		CTriggerInstance* const pTriggerInstance,
		uint16 const triggerCounter) const;

	float GetRadius() const { return m_radius; }
#endif // CRY_AUDIO_USE_DEBUG_CODE

private:

	TriggerConnections const m_connections;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	float const m_radius;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};
} // namespace CryAudio
