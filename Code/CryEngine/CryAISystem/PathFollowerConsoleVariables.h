// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsPathFollower
{
	void Init();

	DeclareConstIntCVar(UseSmartPathFollower, 1);
	DeclareConstIntCVar(SmartPathFollower_useAdvancedPathShortcutting, 1);
	DeclareConstIntCVar(SmartPathFollower_useAdvancedPathShortcutting_debug, 0);
	DeclareConstIntCVar(DrawPathFollower, 0);

	float SmartPathFollower_LookAheadDistance;
	float SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk;
	float SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint;
	float SmartPathFollower_decelerationHuman;
	float SmartPathFollower_decelerationVehicle;
};