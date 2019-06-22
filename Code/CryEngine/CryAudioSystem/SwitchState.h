// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CSwitchState final : public CPoolObject<CSwitchState, stl::PSyncNone>
{
public:

	CSwitchState() = delete;
	CSwitchState(CSwitchState const&) = delete;
	CSwitchState(CSwitchState&&) = delete;
	CSwitchState& operator=(CSwitchState const&) = delete;
	CSwitchState& operator=(CSwitchState&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CSwitchState(
		ControlId const switchId,
		SwitchStateId const switchStateId,
		SwitchStateConnections const& connections,
		char const* const szName)
		: m_switchId(switchId)
		, m_switchStateId(switchStateId)
		, m_connections(connections)
		, m_name(szName)
	{}
#else
	explicit CSwitchState(
		ControlId const switchId,
		SwitchStateId const switchStateId,
		SwitchStateConnections const& connections)
		: m_switchId(switchId)
		, m_switchStateId(switchStateId)
		, m_connections(connections)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CSwitchState();

	void          Set() const;
	void          SetGlobally() const;
	SwitchStateId GetId() const { return m_switchStateId; }

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	void        Set(CObject const& object) const;
	char const* GetName() const { return m_name.c_str(); }
#else
	void        Set(Impl::IObject* const pIObject) const;
#endif // CRY_AUDIO_USE_DEBUG_CODE

private:

	ControlId const              m_switchId;
	SwitchStateId const          m_switchStateId;
	SwitchStateConnections const m_connections;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};
} // namespace CryAudio
