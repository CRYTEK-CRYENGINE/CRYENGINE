// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

#include "BehaviorTree/BehaviorTreeConsoleVariables.h"
#include "CollisionAvoidance/CollisionAvoidanceConsoleVariables.h"
#include "VisionMapConsoleVariables.h"
#include "Movement/MovementConsoleVariables.h"
#include "Navigation/NavigationSystem/NavigationConsoleVariables.h"
#include "MNMPathfinderConsoleVariables.h"
#include "PathFollowerConsoleVariables.h"
#include "AILegacyConsoleVariables.h"

struct SAIConsoleVars
{
	void Init();

	static void DebugAgent(IConsoleCmdArgs* args);

	DeclareConstIntCVar(DebugDraw, -1);
	DeclareConstIntCVar(RayCasterQuota, 12);
	DeclareConstIntCVar(NetworkDebug, 0);
	DeclareConstIntCVar(IntersectionTesterQuota, 12);
	DeclareConstIntCVar(UpdateAllAlways, 0);
	DeclareConstIntCVar(DebugDrawVegetationCollisionDist, 0);
	DeclareConstIntCVar(DrawPlayerRanges, 0);
	DeclareConstIntCVar(WaterOcclusionEnable, 1);
	DeclareConstIntCVar(Recorder, 0);
	DeclareConstIntCVar(StatsDisplayMode, 0);
	DeclareConstIntCVar(DebugDrawSignalsHistory, 0);
	DeclareConstIntCVar(UpdateProxy, 1);
	DeclareConstIntCVar(LogConsoleVerbosity, 0);
	DeclareConstIntCVar(LogFileVerbosity, 0);
	DeclareConstIntCVar(EnableWarningsErrors, 0);
	DeclareConstIntCVar(IgnorePlayer, 0);

	int                              AiSystem;
	float                            OverlayMessageDuration;
	float                            AIUpdateInterval;
	float                            SteepSlopeUpValue;
	float                            SteepSlopeAcrossValue;
	float                            WaterOcclusionScale;
	float                            DebugDrawOffset;

	SAIConsoleVarsNavigation         navigation;
	SAIConsoleVarsMNMPathfinder      pathfinder;
	SAIConsoleVarsPathFollower       pathFollower;
	SAIConsoleVarsMovement           movement;
	SAIConsoleVarsCollisionAvoidance collisionAvoidance;
	SAIConsoleVarsVisionMap          visionMap;
	SAIConsoleVarsBehaviorTree       behaviorTree;

	// Legacy CVars
	DeclareConstIntCVar(LegacyDebugPathFinding, 0);
	DeclareConstIntCVar(LegacyDebugDrawBannedNavsos, 0);
	DeclareConstIntCVar(LegacyCoverExactPositioning, 0);
	DeclareConstIntCVar(LegacyIgnoreVisibilityChecks, 0);
	DeclareConstIntCVar(LegacyRecordLog, 0);
	DeclareConstIntCVar(LegacyDebugRecordAuto, 0);
	DeclareConstIntCVar(LegacyDebugRecordBuffer, 1024);
	DeclareConstIntCVar(LegacyDrawAreas, 0);
	DeclareConstIntCVar(LegacyScriptBind, 1);
	DeclareConstIntCVar(LegacyAdjustPathsAroundDynamicObstacles, 1);
	DeclareConstIntCVar(LegacyOutputPersonalLogToConsole, 0);
	DeclareConstIntCVar(LegacyPredictivePathFollowing, 1);
	DeclareConstIntCVar(LegacyForceSerializeAllObjects, 0);

	int                                     LegacyLogSignals;
	const char*                             LegacyCompatibilityMode;
	const char*                             LegacyDrawLocate;

	SAIConsoleVarsLegacyCoverSystem         legacyCoverSystem;
	SAIConsoleVarsLegacyBubblesSystem       legacyBubblesSystem;
	SAIConsoleVarsLegacySmartObjects        legacySmartObjects;
	SAIConsoleVarsLegacyTacticalPointSystem legacyTacticalPointSystem;
	SAIConsoleVarsLegacyCommunicationSystem legacyCommunicationSystem;
	SAIConsoleVarsLegacyPathObstacles       legacyPathObstacles;
	SAIConsoleVarsLegacyDebugDraw           legacyDebugDraw;
	SAIConsoleVarsLegacyGroupSystem         legacyGroupSystem;
	SAIConsoleVarsLegacyFormation           legacyFormationSystem;
	SAIConsoleVarsLegacyPerception          legacyPerception;
	SAIConsoleVarsLegacyTargetTracking      legacyTargetTracking;
	SAIConsoleVarsLegacyInterestSystem      legacyInterestSystem;
	SAIConsoleVarsLegacyFiring              legacyFiring;
	SAIConsoleVarsLegacyPuppet              legacyPuppet;
	SAIConsoleVarsLegacyPuppetROD           legacyPuppetRod;
	// ~Legacy CVars
};