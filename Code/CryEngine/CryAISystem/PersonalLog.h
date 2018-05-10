// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <queue>

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define AI_COMPILE_WITH_PERSONAL_LOG
#endif

// An actor keeps a personal log where messages are stored.
// This text can be rendered to the screen and will be recorded
// in the AI Recorded for later inspection.
class PersonalLog
{
public:
	typedef std::deque<string> Messages;

	void            AddMessage(const EntityId entityId, const char* message);
	const Messages& GetMessages() const { return m_messages; }
	void            Clear()             { m_messages.clear(); }

private:
	Messages m_messages;
};
