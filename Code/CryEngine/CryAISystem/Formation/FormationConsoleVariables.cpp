// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FormationConsoleVariables.h"

void SAIConsoleVarsLegacyFormation::Init()
{
	DefineConstIntCVarName("ai_FormationSystem", FormationSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enables/Dsiable the Formation System.\n");

	DefineConstIntCVarName("ai_DrawFormations", DrawFormations, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws all the currently active formations of the AI agents.\n"
		"Usage: ai_DrawFormations [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the AI formations.");
}