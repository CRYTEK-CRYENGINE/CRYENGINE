// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyPuppet
{
	void Init();

	DeclareConstIntCVar(ForceStance, -1);
	DeclareConstIntCVar(ForceAllowStrafing, -1);
	DeclareConstIntCVar(DebugDrawCrowdControl, 0);

	const char* ForceAGAction;
	const char* ForceAGSignal;
	const char* ForcePosture;
	const char* ForceLookAimTarget;
};