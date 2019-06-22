// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CSwitchState;

class CSwitch final : public Control, public CPoolObject<CSwitch, stl::PSyncNone>
{
public:

	CSwitch() = delete;
	CSwitch(CSwitch const&) = delete;
	CSwitch(CSwitch&&) = delete;
	CSwitch& operator=(CSwitch const&) = delete;
	CSwitch& operator=(CSwitch&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CSwitch(ControlId const id, ContextId const contextId, char const* const szName)
		: Control(id, contextId, szName)
	{}
#else
	explicit CSwitch(ControlId const id, ContextId const contextId)
		: Control(id, contextId)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CSwitch();

	using SwitchStates = std::map<SwitchStateId, CSwitchState const*>;

	void                AddState(SwitchStateId const id, CSwitchState const* pState) { m_states[id] = pState; }
	SwitchStates const& GetStates() const                                            { return m_states; }

private:

	SwitchStates m_states;
};
} // namespace CryAudio
