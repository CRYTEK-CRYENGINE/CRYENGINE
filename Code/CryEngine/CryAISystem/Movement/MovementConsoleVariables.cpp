// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementConsoleVariables.h"

void SAIConsoleVarsMovement::Init()
{
	DefineConstIntCVarName("ai_DebugMovementSystem", DebugMovementSystem, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draw debug information to the screen regarding character movement.");

	DefineConstIntCVarName("ai_DebugMovementSystemActorRequests", DebugMovementSystemActorRequests, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draw queued movement requests of actors in the world as text above their head.\n"
		"0 - Off\n"
		"1 - Draw request queue of only the currently selected agent\n"
		"2 - Draw request queue of all agents");

	REGISTER_CVAR2("ai_MovementSystemPathReplanningEnabled", &MovementSystemPathReplanningEnabled, 0, VF_NULL,
		"Enable/disable path-replanning of moving actors in the MovementSystem every time\n"
		"a navigation-mesh change at runtime affects their current path.\n"
		"Ignores designer-placed paths.\n"
		"0 - disabled (default)\n"
		"1 - enabled");
}