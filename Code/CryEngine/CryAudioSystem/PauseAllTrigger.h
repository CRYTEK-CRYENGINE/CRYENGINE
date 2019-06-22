// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
class CPauseAllTrigger final : public Control
{
public:

	CPauseAllTrigger(CPauseAllTrigger const&) = delete;
	CPauseAllTrigger(CPauseAllTrigger&&) = delete;
	CPauseAllTrigger& operator=(CPauseAllTrigger const&) = delete;
	CPauseAllTrigger& operator=(CPauseAllTrigger&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CPauseAllTrigger()
		: Control(g_pauseAllTriggerId, GlobalContextId, g_szPauseAllTriggerName)
	{}
#else
	CPauseAllTrigger()
		: Control(g_pauseAllTriggerId, GlobalContextId)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CPauseAllTrigger();

	void Execute() const;
	void AddConnections(TriggerConnections const& connections);
	void Clear();

private:

	TriggerConnections m_connections;
};
} // namespace CryAudio
