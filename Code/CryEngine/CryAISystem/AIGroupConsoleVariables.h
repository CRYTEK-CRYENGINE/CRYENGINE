// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyGroupSystem
{
	void Init();

	DeclareConstIntCVar(GroupSystem, 1);
	DeclareConstIntCVar(DebugDrawGroups, 0);
	DeclareConstIntCVar(DrawGroupTactic, 0);
	DeclareConstIntCVar(DebugDrawReinforcements, -1);

	const char* DrawAgentStatsGroupFilter;
};