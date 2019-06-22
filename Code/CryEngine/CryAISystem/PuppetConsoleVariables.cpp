// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PuppetConsoleVariables.h"

void SAIConsoleVarsLegacyPuppet::Init()
{
	DefineConstIntCVarName("ai_ForceStance", ForceStance, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all AI characters to specified stance:\n"
		"Disable = -1, Stand = 0, Crouch = 1, Prone = 2, Relaxed = 3, Stealth = 4, Cover = 5, Swim = 6, Zero-G = 7");

	DefineConstIntCVarName("ai_ForceAllowStrafing", ForceAllowStrafing, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all AI characters to use/not use strafing (-1 disables)");

	DefineConstIntCVarName("ai_DebugDrawCrowdControl", DebugDrawCrowdControl, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws crowd control debug information. 0=off, 1=on");

	REGISTER_CVAR2("ai_ForceAGAction", &ForceAGAction, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all AI characters to specified AG action input. 0 to disable.\n");
	REGISTER_CVAR2("ai_ForceAGSignal", &ForceAGSignal, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all AI characters to specified AG signal input. 0 to disable.\n");
	REGISTER_CVAR2("ai_ForcePosture", &ForcePosture, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all AI characters to specified posture. 0 to disable.\n");
	REGISTER_CVAR2("ai_ForceLookAimTarget", &ForceLookAimTarget, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all AI characters to use/not use a fixed look/aim target\n"
		"none disables\n"
		"x, y, xz or yz sets it to the appropriate direction\n"
		"otherwise it forces looking/aiming at the entity with this name (no name -> (0, 0, 0))");
}