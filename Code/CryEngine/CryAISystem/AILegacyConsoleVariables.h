// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

#include "AIBubblesSystem/AIBubblesSystemConsoleVariables.h"
#include "Communication/CommunicationConsoleVariables.h"
#include "Cover/CoverSystemConsoleVariables.h"
#include "TacticalPointSystem/TacticalPointSystemConsoleVariables.h"
#include "Formation/FormationConsoleVariables.h"
#include "TargetSelection/TargetTrackConsoleVariables.h"
#include "AIGroupConsoleVariables.h"
#include "SmartObjectsConsoleVariables.h"
#include "PuppetConsoleVariables.h"
#include "PuppetRateOfDeathConsoleVariables.h"
#include "DebugDrawConsoleVariables.h"

struct SAIConsoleVarsLegacyPathObstacles
{
	void Init();

	float MinActorDynamicObstacleAvoidanceRadius;
	float ExtraVehicleAvoidanceRadiusBig;
	float ExtraVehicleAvoidanceRadiusSmall;
	float ObstacleSizeThreshold;
};

struct SAIConsoleVarsLegacyPerception
{
	void Init();

	DeclareConstIntCVar(DrawTargets, 0);
	DeclareConstIntCVar(PlayerAffectedByLight, 1);
	DeclareConstIntCVar(DrawPerceptionIndicators, 0);
	DeclareConstIntCVar(DrawPerceptionDebugging, 0);
	DeclareConstIntCVar(DrawPerceptionModifiers, 0);
	DeclareConstIntCVar(DebugGlobalPerceptionScale, 0);
	DeclareConstIntCVar(IgnoreVisualStimulus, 0);
	DeclareConstIntCVar(IgnoreSoundStimulus, 0);
	DeclareConstIntCVar(IgnoreBulletRainStimulus, 0);

	const char* DrawPerceptionHandlerModifiers;
	float       SightRangeSuperDarkIllumMod;
	float       SightRangeDarkIllumMod;
	float       SightRangeMediumIllumMod;
	float       DrawAgentFOV;
};

struct SAIConsoleVarsLegacyInterestSystem
{
	void Init();

	DeclareConstIntCVar(InterestSystem, 1);
	DeclareConstIntCVar(DebugInterest, 0);
};

struct SAIConsoleVarsLegacyFiring
{
	void Init();

	DeclareConstIntCVar(AmbientFireEnable, 1);
	DeclareConstIntCVar(DebugDrawAmbientFire, 0);
	DeclareConstIntCVar(DrawFireEffectEnabled, 1);
	DeclareConstIntCVar(AllowedToHitPlayer, 1);
	DeclareConstIntCVar(AllowedToHit, 1);
	DeclareConstIntCVar(EnableCoolMisses, 1);
	DeclareConstIntCVar(LobThrowSimulateRandomInaccuracy, 0);

	int   AmbientFireQuota;
	float AmbientFireUpdateInterval;
	float DrawFireEffectDecayRange;
	float DrawFireEffectMinDistance;
	float DrawFireEffectMinTargetFOV;
	float DrawFireEffectMaxAngle;
	float DrawFireEffectTimeScale;
	float CoolMissesBoxSize;
	float CoolMissesBoxHeight;
	float CoolMissesMinMissDistance;
	float CoolMissesMaxLightweightEntityMass;
	float CoolMissesProbability;
	float CoolMissesCooldown;
	float LobThrowMinAllowedDistanceFromFriends;
	float LobThrowTimePredictionForFriendPositions;
	float LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation;
};