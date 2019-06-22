// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CollisionAvoidanceConsoleVariables.h"

void SAIConsoleVarsCollisionAvoidance::Init()
{
	// is not cheat protected because it changes during game, depending on your settings
	DefineConstIntCVarName("ai_EnableORCA", EnableORCA, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable obstacle avoidance system.");

	DefineConstIntCVarName("ai_CollisionAvoidanceUpdateVelocities", CollisionAvoidanceUpdateVelocities, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable/disable agents updating their velocities after processing collision avoidance.");
	
	DefineConstIntCVarName("ai_CollisionAvoidanceEnableRadiusIncrement", CollisionAvoidanceEnableRadiusIncrement, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable/disable agents adding an increment to their collision avoidance radius when moving.");
	
	DefineConstIntCVarName("ai_CollisionAvoidanceClampVelocitiesWithNavigationMesh", CollisionAvoidanceClampVelocitiesWithNavigationMesh, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable/Disable the clamping of the speed resulting from ORCA with the navigation mesh");
	
	DefineConstIntCVarName("ai_DebugDrawCollisionAvoidance", DebugDrawCollisionAvoidance, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable debugging obstacle avoidance system.");

	REGISTER_CVAR2("ai_CollisionAvoidanceAgentExtraFat", &CollisionAvoidanceAgentExtraFat, 0.025f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Extra radius to use in Collision Avoidance calculations as a buffer.");

	REGISTER_CVAR2("ai_CollisionAvoidanceRadiusIncrementIncreaseRate", &CollisionAvoidanceRadiusIncrementIncreaseRate, 0.25f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Increase rate of the collision avoidance radius increment.");

	REGISTER_CVAR2("ai_CollisionAvoidanceRadiusIncrementDecreaseRate", &CollisionAvoidanceRadiusIncrementDecreaseRate, 2.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Decrease rate of the collision avoidance radius increment.");

	REGISTER_CVAR2("ai_CollisionAvoidanceRange", &CollisionAvoidanceRange, 10.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Range for collision avoidance.");

	REGISTER_CVAR2("ai_CollisionAvoidanceTargetCutoffRange", &CollisionAvoidanceTargetCutoffRange, 3.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Distance from it's current target for an agent to stop avoiding obstacles. Other actors will still avoid the agent.");

	REGISTER_CVAR2("ai_CollisionAvoidancePathEndCutoffRange", &CollisionAvoidancePathEndCutoffRange, 0.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Distance from it's current path end for an agent to stop avoiding obstacles. Other actors will still avoid the agent.");

	REGISTER_CVAR2("ai_CollisionAvoidanceSmartObjectCutoffRange", &CollisionAvoidanceSmartObjectCutoffRange, 1.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Distance from it's next smart object for an agent to stop avoiding obstacles. Other actors will still avoid the agent.");

	REGISTER_CVAR2("ai_CollisionAvoidanceTimestep", &CollisionAvoidanceTimeStep, 0.1f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"TimeStep used to calculate an agent's collision free velocity.");
	
	REGISTER_CVAR2("ai_CollisionAvoidanceMinSpeed", &CollisionAvoidanceMinSpeed, 0.2f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Minimum speed allowed to be used by ORCA.");
	
	REGISTER_CVAR2("ai_CollisionAvoidanceAgentTimeHorizon", &CollisionAvoidanceAgentTimeHorizon, 2.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Time horizon used to calculate an agent's collision free velocity against other agents.");
	
	REGISTER_CVAR2("ai_CollisionAvoidanceObstacleTimeHorizon", &CollisionAvoidanceObstacleTimeHorizon, 1.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Time horizon used to calculate an agent's collision free velocity against static obstacles.");
	
	REGISTER_CVAR2("ai_DebugCollisionAvoidanceForceSpeed", &DebugCollisionAvoidanceForceSpeed, 0.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Force agents velocity to it's current direction times the specified value.");
	
	REGISTER_CVAR2("ai_DebugDrawCollisionAvoidanceAgentName", &DebugDrawCollisionAvoidanceAgentName, "", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Name of the agent to draw collision avoidance data for.");
}