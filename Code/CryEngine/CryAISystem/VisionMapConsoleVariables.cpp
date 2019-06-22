// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VisionMapConsoleVariables.h"

void SAIConsoleVarsVisionMap::Init()
{
	DefineConstIntCVarName("ai_DebugDrawVisionMap", DebugDrawVisionMap, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles the debug drawing of the AI VisionMap.");

	DefineConstIntCVarName("ai_DebugDrawVisionMapStats", DebugDrawVisionMapStats, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the debug drawing of the AI VisionMap to show stats information.");

	DefineConstIntCVarName("ai_DebugDrawVisionMapVisibilityChecks", DebugDrawVisionMapVisibilityChecks, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the debug drawing of the AI VisionMap to show the status of the visibility checks.");

	DefineConstIntCVarName("ai_DebugDrawVisionMapObservers", DebugDrawVisionMapObservers, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the debug drawing of the AI VisionMap to show the location/stats of the observers.\n");

	DefineConstIntCVarName("ai_DebugDrawVisionMapObserversFOV", DebugDrawVisionMapObserversFOV, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the debug drawing of the AI VisionMap to draw representations of the observers FOV.\n");

	DefineConstIntCVarName("ai_DebugDrawVisionMapObservables", DebugDrawVisionMapObservables, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the debug drawing of the AI VisionMap to show the location/stats of the observables.");

	DefineConstIntCVarName("ai_VisionMapNumberOfPVSUpdatesPerFrame", VisionMapNumberOfPVSUpdatesPerFrame, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "");

	DefineConstIntCVarName("ai_VisionMapNumberOfVisibilityUpdatesPerFrame", VisionMapNumberOfVisibilityUpdatesPerFrame, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "");
}