// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TacticalPointSystemConsoleVariables.h"

void SAIConsoleVarsLegacyTacticalPointSystem::Init()
{
	DefineConstIntCVarName("ai_TacticalPointSystem", TacticalPointSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the Tactical Point System.\n");

	REGISTER_CVAR2("ai_TacticalPointUpdateTime", &TacticalPointUpdateTime, 0.0005f, VF_NULL,
		"Maximum allowed update time in main AI thread for Tactical Point System\n"
		"Usage: ai_TacticalPointUpdateTime <number>\n"
		"Default is 0.0003");
}