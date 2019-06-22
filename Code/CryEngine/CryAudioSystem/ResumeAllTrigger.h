// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
class CResumeAllTrigger final : public Control
{
public:

	CResumeAllTrigger(CResumeAllTrigger const&) = delete;
	CResumeAllTrigger(CResumeAllTrigger&&) = delete;
	CResumeAllTrigger& operator=(CResumeAllTrigger const&) = delete;
	CResumeAllTrigger& operator=(CResumeAllTrigger&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CResumeAllTrigger()
		: Control(g_resumeAllTriggerId, GlobalContextId, g_szResumeAllTriggerName)
	{}
#else
	CResumeAllTrigger()
		: Control(g_resumeAllTriggerId, GlobalContextId)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CResumeAllTrigger();

	void Execute() const;
	void AddConnections(TriggerConnections const& connections);
	void Clear();

private:

	TriggerConnections m_connections;
};
} // namespace CryAudio
