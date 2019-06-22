// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PathFollowerConsoleVariables.h"

void SAIConsoleVarsPathFollower::Init()
{
	DefineConstIntCVarName("ai_UseSmartPathFollower", UseSmartPathFollower, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables Smart PathFollower (default: 1).");

	DefineConstIntCVarName("ai_SmartPathFollower_useAdvancedPathShortcutting", SmartPathFollower_useAdvancedPathShortcutting, 1, VF_NULL, "Enables a more failsafe way of preventing the AI to shortcut through obstacles (0 = disable, any other value = enable)");

	DefineConstIntCVarName("ai_SmartPathFollower_useAdvancedPathShortcutting_debug", SmartPathFollower_useAdvancedPathShortcutting_debug, 0, VF_NULL, "Show debug lines for when CVar ai_SmartPathFollower_useAdvancedPathShortcutting_debug is enabled");

	DefineConstIntCVarName("ai_DrawPathFollower", DrawPathFollower, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables PathFollower debug drawing displaying agent paths and safe follow target.");

	REGISTER_CVAR2("ai_UseSmartPathFollower_LookAheadDistance", &SmartPathFollower_LookAheadDistance, 10.0f, VF_NULL, "LookAheadDistance of SmartPathFollower");

	REGISTER_CVAR2("ai_SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk", &SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk, 0.5f, VF_NULL,
		"Defines the time frame the AI is allowed to look ahead while moving strictly along a path to decide whether to cut towards the next point. (Walk only)\n");

	REGISTER_CVAR2("ai_SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint", &SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint, 0.25f, VF_NULL,
		"Defines the time frame the AI is allowed to look ahead while moving strictly along a path to decide whether to cut towards the next point. (Run and Sprint only)\n");

	REGISTER_CVAR2("ai_SmartPathFollower_decelerationHuman", &SmartPathFollower_decelerationHuman, 7.75f, VF_NULL, "Deceleration multiplier for non-vehicles");

	REGISTER_CVAR2("ai_SmartPathFollower_decelerationVehicle", &SmartPathFollower_decelerationVehicle, 1.0f, VF_NULL, "Deceleration multiplier for vehicles");
}
