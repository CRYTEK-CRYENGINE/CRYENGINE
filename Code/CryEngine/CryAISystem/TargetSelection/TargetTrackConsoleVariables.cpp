// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TargetTrackConsoleVariables.h"

void SAIConsoleVarsLegacyTargetTracking::Init()
{
	DefineConstIntCVarName("ai_TargetTracking", TargetTracking, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable/disable target tracking. 0=disable, any other value = Enable");

	DefineConstIntCVarName("ai_TargetTracks_GlobalTargetLimit", TargetTracks_GlobalTargetLimit, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Global override to control the number of agents that can actively target another agent (unless there is no other choice)\n"
		"A value of 0 means no global limit is applied. If the global target limit is less than the agent's target limit, the global limit is used.");

	DefineConstIntCVarName("ai_DebugTargetTracksTarget", TargetTracks_TargetDebugDraw, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws lines to illustrate where each agent's target is\n"
		"Usage: ai_DebugTargetTracking 0/1/2\n"
		"0 = Off. 1 = Show best target. 2 = Show all possible targets.");

	DefineConstIntCVarName("ai_DebugTargetTracksConfig", TargetTracks_ConfigDebugDraw, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws the information contained in the loaded target track configurations to the screen");

	REGISTER_CVAR2("ai_DebugTargetTracksAgent", &TargetTracks_AgentDebugDraw, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws the target tracks for the given agent\n"
		"Usage: ai_DebugTargetTracksAgent AIName\n"
		"Default is 'none'. AIName is the name of the AI agent to debug");

	REGISTER_CVAR2("ai_DebugTargetTracksConfig_Filter", &TargetTracks_ConfigDebugFilter, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Filter what configurations are drawn when debugging target tracks\n"
		"Usage: ai_DebugTargetTracks_Filter Filter\n"
		"Default is 'none'. Filter is a substring that must be in the configuration name");
}
