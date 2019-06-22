// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyCoverSystem
{
	void Init();

	DeclareConstIntCVar(CoverSystem, 1);
	DeclareConstIntCVar(DebugDrawCover, 0);
	DeclareConstIntCVar(DebugDrawCoverOccupancy, 0);
	DeclareConstIntCVar(DebugDrawCoverPlanes, 0);
	DeclareConstIntCVar(DebugDrawCoverLocations, 0);
	DeclareConstIntCVar(DebugDrawCoverSampler, 0);
	DeclareConstIntCVar(DebugDrawDynamicCoverSampler, 0);

	float CoverPredictTarget;
	float CoverSpacing;
};