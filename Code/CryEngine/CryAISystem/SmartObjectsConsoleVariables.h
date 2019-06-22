// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacySmartObjects
{
	void Init();

	DeclareConstIntCVar(DrawSmartObjects, 0);

	float BannedNavSoTime;
};