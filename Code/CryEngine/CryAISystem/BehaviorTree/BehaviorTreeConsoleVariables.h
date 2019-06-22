// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsBehaviorTree
{
	void Init();

	DeclareConstIntCVar(DrawModularBehaviorTreeStatistics, 0);
	DeclareConstIntCVar(LogModularBehaviorTreeExecutionStacks, 0);

	int ModularBehaviorTree;
	int ModularBehaviorTreeDebugTree;
	int ModularBehaviorTreeDebugVariables;
	int ModularBehaviorTreeDebugTimestamps;
	int ModularBehaviorTreeDebugEvents;
	int ModularBehaviorTreeDebugLog;
	int ModularBehaviorTreeDebugBlackboard;
};