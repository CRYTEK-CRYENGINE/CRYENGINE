// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyTargetTracking
{
	void Init();

	DeclareConstIntCVar(TargetTracking, 1);
	DeclareConstIntCVar(TargetTracks_GlobalTargetLimit, 0);
	DeclareConstIntCVar(TargetTracks_TargetDebugDraw, 0);
	DeclareConstIntCVar(TargetTracks_ConfigDebugDraw, 0);

	const char* TargetTracks_AgentDebugDraw;
	const char* TargetTracks_ConfigDebugFilter;
};