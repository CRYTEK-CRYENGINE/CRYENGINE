// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyDebugDraw
{
	void Init();

	DeclareConstIntCVar(DebugDrawCoolMisses, 0);
	DeclareConstIntCVar(DebugDrawFireCommand, 0);
	DeclareConstIntCVar(DrawGoals, 0);
	DeclareConstIntCVar(DrawStats, 0);
	DeclareConstIntCVar(DrawAttentionTargetsPosition, 0);
	DeclareConstIntCVar(DrawTrajectory, 0);
	DeclareConstIntCVar(DrawRadar, 0);
	DeclareConstIntCVar(DrawRadarDist, 20);
	DeclareConstIntCVar(DrawProbableTarget, 0);
	DeclareConstIntCVar(DebugDrawDamageParts, 0);
	DeclareConstIntCVar(DebugDrawStanceSize, 0);
	DeclareConstIntCVar(DebugDrawUpdate, 0);
	DeclareConstIntCVar(DebugDrawEnabledActors, 0);
	DeclareConstIntCVar(DebugDrawEnabledPlayers, 0);
	DeclareConstIntCVar(DebugDrawLightLevel, 0);
	DeclareConstIntCVar(DebugDrawPhysicsAccess, 0);
	DeclareConstIntCVar(DebugTargetSilhouette, 0);
	DeclareConstIntCVar(DebugDrawDamageControl, 0);
	DeclareConstIntCVar(DebugDrawExpensiveAccessoryQuota, 0);
	DeclareConstIntCVar(DrawFakeDamageInd, 0);
	DeclareConstIntCVar(DebugDrawAdaptiveUrgency, 0);
	DeclareConstIntCVar(DebugDrawPlayerActions, 0);
	DeclareConstIntCVar(DrawType, -1);

	const char* DrawPath;
	const char* DrawPathAdjustment;
	const char* DrawRefPoints;
	const char* StatsTarget;
	const char* DrawShooting;
	const char* DrawAgentStats;
	const char* FilterAgentName;
	float       DebugDrawArrowLabelsVisibilityDistance;
	float       AgentStatsDist;
	int         DrawSelectedTargets;
};