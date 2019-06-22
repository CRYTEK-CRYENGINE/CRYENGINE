// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyTacticalPointSystem
{
	void Init();

	DeclareConstIntCVar(TacticalPointSystem, 1);

	float TacticalPointUpdateTime;
};