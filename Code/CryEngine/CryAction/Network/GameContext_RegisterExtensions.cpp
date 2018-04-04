// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "GameContext.h"
#include "VoiceListener.h"

void CGameContext::RegisterExtensions(IGameFramework* pFW)
{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	REGISTER_FACTORY(pFW, "VoiceListener", CVoiceListener, false);
#endif
}
