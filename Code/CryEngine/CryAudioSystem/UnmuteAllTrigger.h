// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
class CUnmuteAllTrigger final : public Control
{
public:

	CUnmuteAllTrigger(CUnmuteAllTrigger const&) = delete;
	CUnmuteAllTrigger(CUnmuteAllTrigger&&) = delete;
	CUnmuteAllTrigger& operator=(CUnmuteAllTrigger const&) = delete;
	CUnmuteAllTrigger& operator=(CUnmuteAllTrigger&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CUnmuteAllTrigger()
		: Control(g_unmuteAllTriggerId, GlobalContextId, g_szUnmuteAllTriggerName)
	{}
#else
	CUnmuteAllTrigger()
		: Control(g_unmuteAllTriggerId, GlobalContextId)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CUnmuteAllTrigger();

	void Execute() const;
	void AddConnections(TriggerConnections const& connections);
	void Clear();

private:

	TriggerConnections m_connections;
};
} // namespace CryAudio
