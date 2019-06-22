// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AIGroupConsoleVariables.h"

void SAIConsoleVarsLegacyGroupSystem::Init()
{
	DefineConstIntCVarName("ai_GroupSystem", GroupSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the Group System.\n");

	DefineConstIntCVarName("ai_DebugDrawGroups", DebugDrawGroups, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles the AI Groups debugging view.\n"
		"Usage: ai_DebugDrawGroups [0/1]\n"
		"Default is 0 (off). ai_DebugDrawGroups displays groups' leaders, members and their desired positions.");

	DefineConstIntCVarName("ai_DrawGroupTactic", DrawGroupTactic, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"draw group tactic: 0 = disabled, 1 = draw simple, 2 = draw complex.");

	DefineConstIntCVarName("ai_DebugDrawReinforcements", DebugDrawReinforcements, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables debug draw for reinforcement logic for specified group.\n"
		"Usage: ai_DebugDrawReinforcements <groupid>, or -1 to disable.");

	REGISTER_CVAR2("ai_DrawAgentStatsGroupFilter", &DrawAgentStatsGroupFilter, "", VF_NULL,
		"Filters Debug Draw Agent Stats displays to the specified groupIDs separated by spaces\n"
		"usage: ai_DrawAgentStatsGroupFilter 1011 1012");
}