// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsCollisionAvoidance
{
	void Init();

	DeclareConstIntCVar(EnableORCA, 1);
	DeclareConstIntCVar(CollisionAvoidanceUpdateVelocities, 1);
	DeclareConstIntCVar(CollisionAvoidanceEnableRadiusIncrement, 1);
	DeclareConstIntCVar(CollisionAvoidanceClampVelocitiesWithNavigationMesh, 0);
	DeclareConstIntCVar(DebugDrawCollisionAvoidance, 0);

	float       CollisionAvoidanceAgentExtraFat;
	float       CollisionAvoidanceRadiusIncrementIncreaseRate;
	float       CollisionAvoidanceRadiusIncrementDecreaseRate;
	float       CollisionAvoidanceRange;
	float       CollisionAvoidanceTargetCutoffRange;
	float       CollisionAvoidancePathEndCutoffRange;
	float       CollisionAvoidanceSmartObjectCutoffRange;
	float       CollisionAvoidanceTimeStep;
	float       CollisionAvoidanceMinSpeed;
	float       CollisionAvoidanceAgentTimeHorizon;
	float       CollisionAvoidanceObstacleTimeHorizon;
	float       DebugCollisionAvoidanceForceSpeed;
	const char* DebugDrawCollisionAvoidanceAgentName;
};