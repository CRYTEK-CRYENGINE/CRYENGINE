// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyBubblesSystem
{
	void Init();

	DeclareConstIntCVar(BubblesSystemAlertnessFilter, 7);
	DeclareConstIntCVar(BubblesSystemUseDepthTest, 0);
	DeclareConstIntCVar(BubblesSystemAllowPrototypeDialogBubbles, 0);

	int         EnableBubblesSystem;
	float       BubblesSystemFontSize;
	float       BubblesSystemDecayTime;
	const char* BubblesSystemNameFilter;

	static void AIBubblesNameFilterCallback(ICVar* pCvar);
};