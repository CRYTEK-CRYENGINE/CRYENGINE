// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeConsoleVariables.h"

void SAIConsoleVarsBehaviorTree::Init()
{
	DefineConstIntCVarName("ai_DrawModularBehaviorTreeStatistics", DrawModularBehaviorTreeStatistics, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draw modular behavior statistics to the screen.");

	DefineConstIntCVarName("ai_ModularBehaviorTreeDebugExecutionStacks", LogModularBehaviorTreeExecutionStacks, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-2] Enable/Disable logging of the execution stacks of modular behavior trees to individual files in the MBT_Logs directory.\n"
		"0 - Off\n"
		"1 - Log execution stacks of only the currently selected agent\n"
		"2 - Log execution stacks of all currently active agents");

	REGISTER_CVAR2("ai_ModularBehaviorTree", &ModularBehaviorTree, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the usage of the modular behavior tree system.");

	REGISTER_CVAR2("ai_ModularBehaviorTreeDebugTree", &ModularBehaviorTreeDebugTree, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug text of the behavior tree execution.");

	REGISTER_CVAR2("ai_ModularBehaviorTreeDebugVariables", &ModularBehaviorTreeDebugVariables, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug text of the behavior tree's variables.");

	REGISTER_CVAR2("ai_ModularBehaviorTreeDebugTimestamps", &ModularBehaviorTreeDebugTimestamps, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug text of the modular behavior tree's timestamps.");

	REGISTER_CVAR2("ai_ModularBehaviorTreeDebugEvents", &ModularBehaviorTreeDebugEvents, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug text of the behavior tree's events.");

	REGISTER_CVAR2("ai_ModularBehaviorTreeDebugLog", &ModularBehaviorTreeDebugLog, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug text of the behavior tree's log.");

	REGISTER_CVAR2("ai_ModularBehaviorTreeDebugBlackboard", &ModularBehaviorTreeDebugBlackboard, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug text of the behavior tree's blackboards.");
}