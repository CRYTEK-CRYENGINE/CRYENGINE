// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmartObjectsConsoleVariables.h"

void SAIConsoleVarsLegacySmartObjects::Init()
{
	DefineConstIntCVarName("ai_DrawSmartObjects", DrawSmartObjects, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws smart object debug information.\n"
		"Usage: ai_DrawSmartObjects [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the smart objects.");

	REGISTER_CVAR2("ai_BannedNavSoTime", &BannedNavSoTime, 15.0f, VF_NULL,
		"Time indicating how long invalid navsos should be banned.");
}