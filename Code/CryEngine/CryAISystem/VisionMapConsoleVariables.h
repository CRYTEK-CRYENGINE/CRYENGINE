// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsVisionMap
{
	void Init();

	DeclareConstIntCVar(VisionMapNumberOfPVSUpdatesPerFrame, 1);
	DeclareConstIntCVar(VisionMapNumberOfVisibilityUpdatesPerFrame, 1);

	DeclareConstIntCVar(DebugDrawVisionMap, 0);
	DeclareConstIntCVar(DebugDrawVisionMapStats, 1);
	DeclareConstIntCVar(DebugDrawVisionMapVisibilityChecks, 1);
	DeclareConstIntCVar(DebugDrawVisionMapObservers, 1);
	DeclareConstIntCVar(DebugDrawVisionMapObserversFOV, 0);
	DeclareConstIntCVar(DebugDrawVisionMapObservables, 1);
};
