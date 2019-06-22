// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CoverSystemConsoleVariables.h"

void SAIConsoleVarsLegacyCoverSystem::Init()
{
	DefineConstIntCVarName("ai_CoverSystem", CoverSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables the cover system.\n"
		"Usage: ai_CoverSystem [0/1]\n"
		"Default is 1 (on)\n"
		"0 - off \n"
		"1 - use offline cover surfaces\n");

	DefineConstIntCVarName("ai_DebugDrawCover", DebugDrawCover, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays cover debug information.\n"
		"Usage: ai_DebugDrawCover [0/1/2]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - currently being used\n"
		"2 - all in 50m range (slow)\n");
	DefineConstIntCVarName("ai_DebugDrawCoverOccupancy", DebugDrawCoverOccupancy, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Renders red balls at the location of occupied cover.\n"
		"Usage: ai_DebugDrawCoverOccupancy [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - render red balls at the location of occupied cover\n");

	DefineConstIntCVarName("ai_DebugDrawCoverPlanes", DebugDrawCoverPlanes, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays cover planes.\n"
		"Usage: ai_DebugDrawCoverPlanes [0/1]\n"
		"Default is 0 (off)\n");
	DefineConstIntCVarName("ai_DebugDrawCoverLocations", DebugDrawCoverLocations, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays cover locations.\n"
		"Usage: ai_DebugDrawCoverLocations [0/1]\n"
		"Default is 0 (off)\n");
	DefineConstIntCVarName("ai_DebugDrawCoverSampler", DebugDrawCoverSampler, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays cover sampler debug rendering.\n"
		"Usage: ai_DebugDrawCoverSampler [0/1/2/3]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - display floor sampling\n"
		"2 - display surface sampling\n"
		"3 - display both\n");
	DefineConstIntCVarName("ai_DebugDrawDynamicCoverSampler", DebugDrawDynamicCoverSampler, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays dynamic cover sampler debug rendering.\n"
		"Usage: ai_DebugDrawDynamicCoverSampler [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - on\n");

	REGISTER_CVAR2("ai_CoverPredictTarget", &CoverPredictTarget, 1.0f, VF_NULL,
		"Enables simple cover system target location prediction.\n"
		"Usage: ai_CoverPredictTarget x\n"
		"Default x is 0.0 (off)\n"
		"x - how many seconds to look ahead\n");
	REGISTER_CVAR2("ai_CoverSpacing", &CoverSpacing, 0.5f, VF_NULL,
		"Minimum spacing between agents when choosing cover.\n"
		"Usage: ai_CoverPredictTarget <x>\n"
		"x - Spacing width in meters\n");
}
