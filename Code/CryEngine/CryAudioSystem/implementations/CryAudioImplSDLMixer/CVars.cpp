// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CVars.h"
#include <CrySystem/ConsoleRegistration.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
void CCVars::RegisterVariables()
{
	REGISTER_CVAR2("s_SDLMixerEventPoolSize", &m_eventPoolSize, m_eventPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated events.\n"
	               "Usage: s_SDLMixerEventPoolSize [0/...]\n"
	               "Default: 256\n");
}

//////////////////////////////////////////////////////////////////////////
void CCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_SDLMixerEventPoolSize");
	}
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
