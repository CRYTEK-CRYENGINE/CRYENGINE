// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImplCVars.h"
#include <CrySystem/IConsole.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
void CCVars::RegisterVariables()
{
}

//////////////////////////////////////////////////////////////////////////
void CCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
	}
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
