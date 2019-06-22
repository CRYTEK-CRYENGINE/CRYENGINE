// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AILegacyConsoleVariables.h"

void SAIConsoleVarsLegacyPathObstacles::Init()
{
	REGISTER_CVAR2("ai_MinActorDynamicObstacleAvoidanceRadius", &MinActorDynamicObstacleAvoidanceRadius, 0.6f, VF_NULL,
		"Minimum value in meters to be added to the obstacle's own size for actors\n"
		"(pathRadius property can override it if bigger)");
	REGISTER_CVAR2("ai_ExtraVehicleAvoidanceRadiusBig", &ExtraVehicleAvoidanceRadiusBig, 4.0f, VF_NULL,
		"Value in meters to be added to a big obstacle's own size while computing obstacle\n"
		"size for purposes of vehicle steering. See also ai_ObstacleSizeThreshold.");
	REGISTER_CVAR2("ai_ExtraVehicleAvoidanceRadiusSmall", &ExtraVehicleAvoidanceRadiusSmall, 0.5f, VF_NULL,
		"Value in meters to be added to a big obstacle's own size while computing obstacle\n"
		"size for purposes of vehicle steering. See also ai_ObstacleSizeThreshold.");
	REGISTER_CVAR2("ai_ObstacleSizeThreshold", &ObstacleSizeThreshold, 1.2f, VF_NULL,
		"Obstacle size in meters that differentiates small obstacles from big ones so that vehicles can ignore the small ones");
}

void SAIConsoleVarsLegacyPerception::Init()
{
	DefineConstIntCVarName("ai_DrawTargets", DrawTargets, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Distance to display the perception events of all enabled puppets.\n"
		"Displays target type and priority");

	DefineConstIntCVarName("ai_PlayerAffectedByLight", PlayerAffectedByLight, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Sets if player is affected by light from observable perception checks");

	DefineConstIntCVarName("ai_DrawPerceptionIndicators", DrawPerceptionIndicators, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws indicators showing enemy current perception level of player");

	DefineConstIntCVarName("ai_DrawPerceptionDebugging", DrawPerceptionDebugging, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws indicators showing enemy view intersection with perception modifiers");

	DefineConstIntCVarName("ai_DrawPerceptionModifiers", DrawPerceptionModifiers, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws perception modifier areas in game mode");

	DefineConstIntCVarName("ai_DebugGlobalPerceptionScale", DebugGlobalPerceptionScale, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws global perception scale multipliers on screen");

	DefineConstIntCVarName("ai_IgnoreVisualStimulus", IgnoreVisualStimulus, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Have the Perception Handler ignore all visual stimulus always");

	DefineConstIntCVarName("ai_IgnoreSoundStimulus", IgnoreSoundStimulus, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Have the Perception Handler ignore all sound stimulus always");

	DefineConstIntCVarName("ai_IgnoreBulletRainStimulus", IgnoreBulletRainStimulus, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Have the Perception Handler ignore all bullet rain stimulus always");

	REGISTER_CVAR2("ai_DrawPerceptionHandlerModifiers", &DrawPerceptionHandlerModifiers, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws perception handler modifiers on a specific AI\n"
		"Usage: ai_DrawPerceptionHandlerModifiers AIName\n"
		"Default is 'none'. AIName is the name of the AI");

	REGISTER_CVAR2("ai_SightRangeSuperDarkIllumMod", &SightRangeSuperDarkIllumMod, 0.25f, VF_NULL,
		"Multiplier for sightrange when the target is in super dark light condition.");

	REGISTER_CVAR2("ai_SightRangeDarkIllumMod", &SightRangeDarkIllumMod, 0.5f, VF_NULL,
		"Multiplier for sightrange when the target is in dark light condition.");

	REGISTER_CVAR2("ai_SightRangeMediumIllumMod", &SightRangeMediumIllumMod, 0.8f, VF_NULL,
		"Multiplier for sightrange when the target is in medium light condition.");

	REGISTER_CVAR2("ai_DrawAgentFOV", &DrawAgentFOV, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles the vision cone of the AI agent.\n"
		"Usage: ai_DrawagentFOV [0..1]\n"
		"Default is 0 (off), value 1 will draw the cone all the way to\n"
		"the sight range, value 0.1 will draw the cone to distance of 10%\n"
		"of the sight range, etc. ai_DebugDraw must be enabled before\n"
		"this tool can be used.");
}

void SAIConsoleVarsLegacyInterestSystem::Init()
{
	DefineConstIntCVarName("ai_InterestSystem", InterestSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable interest system");

	DefineConstIntCVarName("ai_DebugInterestSystem", DebugInterest, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Display debugging information on interest system");
}

void SAIConsoleVarsLegacyFiring::Init()
{
	DefineConstIntCVarName("ai_AmbientFireEnable", AmbientFireEnable, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enable ambient fire system.");

	DefineConstIntCVarName("ai_DebugDrawAmbientFire", DebugDrawAmbientFire, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays fire quota on puppets.");

	DefineConstIntCVarName("ai_DrawFireEffectEnabled", DrawFireEffectEnabled, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enabled AI to sweep fire when starting to shoot after a break.");

	DefineConstIntCVarName("ai_AllowedToHitPlayer", AllowedToHitPlayer, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"If turned off, all agents will miss the player all the time.");

	DefineConstIntCVarName("ai_AllowedToHit", AllowedToHit, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"If turned off, all agents will miss all the time.");

	DefineConstIntCVarName("ai_EnableCoolMisses", EnableCoolMisses, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"If turned on, when agents miss the player, they will pick cool objects to shoot at.");

	DefineConstIntCVarName("ai_LobThrowUseRandomForInaccuracySimulation", LobThrowSimulateRandomInaccuracy, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Uses random variation for simulating inaccuracy in the lob throw.");

	REGISTER_CVAR2("ai_AmbientFireQuota", &AmbientFireQuota, 2, 0,
		"Number of units allowed to hit the player at a time.");

	REGISTER_CVAR2("ai_AmbientFireUpdateInterval", &AmbientFireUpdateInterval, 1.0f, VF_NULL,
		"Ambient fire update interval. Controls how often puppet's ambient fire status is updated.");

	REGISTER_CVAR2("ai_DrawFireEffectDecayRange", &DrawFireEffectDecayRange, 30.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Distance under which the draw fire duration starts decaying linearly.");

	REGISTER_CVAR2("ai_DrawFireEffectMinDistance", &DrawFireEffectMinDistance, 7.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Distance under which the draw fire will be disabled.");

	REGISTER_CVAR2("ai_DrawFireEffectMinTargetFOV", &DrawFireEffectMinTargetFOV, 7.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"FOV under which the draw fire will be disabled.");

	REGISTER_CVAR2("ai_DrawFireEffectMaxAngle", &DrawFireEffectMaxAngle, 5.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Maximum angle actors actors are allowed to go away from their aiming direction during draw fire.");

	REGISTER_CVAR2("ai_DrawFireEffectTimeScale", &DrawFireEffectTimeScale, 1.0f, VF_NULL,
		"Scale for the weapon's draw fire time setting.");

	REGISTER_CVAR2("ai_CoolMissesBoxSize", &CoolMissesBoxSize, 10.0f, VF_NULL,
		"Horizontal size of the box to collect potential cool objects to shoot at.");

	REGISTER_CVAR2("ai_CoolMissesBoxHeight", &CoolMissesBoxHeight, 2.5f, VF_NULL,
		"Vertical size of the box to collect potential cool objects to shoot at.");

	REGISTER_CVAR2("ai_CoolMissesMinMissDistance", &CoolMissesMinMissDistance, 7.5f, VF_NULL,
		"Maximum distance to go away from the player.");

	REGISTER_CVAR2("ai_CoolMissesMaxLightweightEntityMass", &CoolMissesMaxLightweightEntityMass, 20.0f, VF_NULL,
		"Maximum mass of a non-destroyable, non-deformable, non-breakable rigid body entity to be considered.");

	REGISTER_CVAR2("ai_CoolMissesProbability", &CoolMissesProbability, 0.35f, VF_NULL,
		"Agents' chance to perform a cool miss!");

	REGISTER_CVAR2("ai_CoolMissesCooldown", &CoolMissesCooldown, 0.25f, VF_NULL,
		"Global time between potential cool misses.");

	REGISTER_CVAR2("ai_LobThrowMinAllowedDistanceFromFriends", &LobThrowMinAllowedDistanceFromFriends, 15.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Minimum distance a grenade (or any object thrown using a lob) should land from mates to accept the throw trajectory.");
	REGISTER_CVAR2("ai_LobThrowTimePredictionForFriendPositions", &LobThrowTimePredictionForFriendPositions, 2.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Time frame used to predict the next position of moving mates to score the landing position of the lob throw");
	REGISTER_CVAR2("ai_LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation", &LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation, 0.0,
		VF_CHEAT | VF_CHEAT_NOCHECK, "This value identifies percentage of the distance to the target that will be used to simulate human inaccuracy with parabolic throws.");
}