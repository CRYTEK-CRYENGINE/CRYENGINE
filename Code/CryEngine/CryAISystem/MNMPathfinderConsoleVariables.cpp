// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MNMPathfinderConsoleVariables.h"

void SAIConsoleVarsMNMPathfinder::Init()
{
	DefineConstIntCVarName("ai_MNMPathfinderPositionInTrianglePredictionType", MNMPathfinderPositionInTrianglePredictionType, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Defines which type of prediction for the point inside each triangle used by the pathfinder heuristic to search for the "
		"path with minimal cost.\n"
		"0 - Triangle center.\n"
		"1 - Advanced prediction.\n");

	DefineConstIntCVarName("ai_BeautifyPath", BeautifyPath, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles AI optimisation of the generated path.\n"
		"Usage: ai_BeautifyPath [0/1]\n"
		"Default is 1 (on). Optimisation is on by default. Set to 0 to\n"
		"disable path optimisation (AI uses non-optimised path).");

	DefineConstIntCVarName("ai_PathStringPullingIterations", PathStringPullingIterations, 5, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Defines the number of iteration used for the string pulling operation to simplify the path");

	DefineConstIntCVarName("ai_MNMPathfinderMT", MNMPathfinderMT, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enable/Disable Multi Threading for the pathfinder.");

	DefineConstIntCVarName("ai_MNMPathfinderConcurrentRequests", MNMPathfinderConcurrentRequests, 4, VF_CHEAT | VF_CHEAT_NOCHECK, "Defines the amount of concurrent pathfinder requests that can be served at the same time.");

	REGISTER_CVAR2("ai_MNMPathFinderQuota", &MNMPathFinderQuota, 0.001f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Set path finding frame time quota in seconds (Set to 0 for no limit)");

	REGISTER_CVAR2("ai_PathfindTimeLimit", &AllowedTimeForPathfinding, 0.08f, VF_NULL,
		"Specifies how many seconds an individual AI can hold the pathfinder blocked\n"
		"Usage: ai_PathfindTimeLimit 0.15\n"
		"Default is 0.08. A lower value will result in more path requests that end in NOPATH -\n"
		"although the path may actually exist.");

	REGISTER_CVAR2("ai_MNMPathFinderDebug", &MNMPathFinderDebug, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable debug draw statistics on pathfinder load. Note that construction and beautify times are only calculated once this CVAR is enabled.");

	DefineConstIntCVarName("ai_PathfinderDangerCostForAttentionTarget", PathfinderDangerCostForAttentionTarget, 5, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Cost used in the heuristic calculation for the danger created by the attention target position.");
	DefineConstIntCVarName("ai_PathfinderDangerCostForExplosives", PathfinderDangerCostForExplosives, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Cost used in the heuristic calculation for the danger created by the position of explosive objects.");
	DefineConstIntCVarName("ai_PathfinderAvoidanceCostForGroupMates", PathfinderAvoidanceCostForGroupMates, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Cost used in the heuristic calculation for the avoidance of the group mates's positions.");

	REGISTER_CVAR2("ai_PathfinderExplosiveDangerRadius", &PathfinderExplosiveDangerRadius, 5.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Range used to evaluate the explosive threats in the path calculation. Outside this range a location is considered safe.");

	REGISTER_CVAR2("ai_PathfinderExplosiveDangerMaxThreatDistance", &PathfinderExplosiveDangerMaxThreatDistance, 50.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Range used to decide if evaluate an explosive danger as an actual threat.");

	REGISTER_CVAR2("ai_PathfinderGroupMatesAvoidanceRadius", &PathfinderGroupMatesAvoidanceRadius, 4.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Range used to evaluate the group mates avoidance in the path calculation.");
}