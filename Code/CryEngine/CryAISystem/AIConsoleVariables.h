// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef AICONSOLEVARIABLES_H
#define AICONSOLEVARIABLES_H

struct AIConsoleVars
{
	// cppcheck-suppress uninitMemberVar
	AIConsoleVars() {}

	void Init();

	DeclareConstIntCVar(UseCalculationStopperCounter, 0);
	DeclareConstIntCVar(DebugDraw, -1);

	DeclareConstIntCVar(CoverMaxEyeCount, 2);
	DeclareConstIntCVar(DebugDrawCover, 0);
	DeclareConstIntCVar(DebugDrawCoverOccupancy, 0);
	DeclareConstIntCVar(DebugDrawNavigation, 0);
	DeclareConstIntCVar(DebugTriangleOnCursor, 0);
	DeclareConstIntCVar(IslandConnectionsSystemProfileMemory, 0);
	DeclareConstIntCVar(DebugDrawNavigationWorldMonitor, 0);
	DeclareConstIntCVar(NavigationSystemMT, 1);
	DeclareConstIntCVar(NavGenThreadJobs, 1);
	float NavmeshStabilizationTimeToUpdate;
	float NavmeshTileDistanceDraw;
	DeclareConstIntCVar(DebugDrawCoverPlanes, 0);
	DeclareConstIntCVar(DebugDrawCoverLocations, 0);
	DeclareConstIntCVar(DebugDrawCoverSampler, 0);
	DeclareConstIntCVar(DebugDrawDynamicCoverSampler, 0);
	DeclareConstIntCVar(CoverSystem, 1);
	DeclareConstIntCVar(CoverExactPositioning, 0);
	DeclareConstIntCVar(NetworkDebug, 0);
	DeclareConstIntCVar(DebugDrawHideSpotRange, 0);
	DeclareConstIntCVar(DebugDrawDynamicHideObjectsRange, 0);
	DeclareConstIntCVar(DebugDrawVolumeVoxels, 0);
	DeclareConstIntCVar(DebugPathFinding, 0);
	DeclareConstIntCVar(DebugDrawBannedNavsos, 0);
	DeclareConstIntCVar(DebugDrawGroups, 0);
	DeclareConstIntCVar(DebugDrawCoolMisses, 0);
	DeclareConstIntCVar(DebugDrawFireCommand, 0);
	DeclareConstIntCVar(UseSimplePathfindingHeuristic, 0);

	DeclareConstIntCVar(DebugDrawCommunication, 0);
	DeclareConstIntCVar(DebugDrawCommunicationHistoryDepth, 5);
	DeclareConstIntCVar(RecordCommunicationStats, 0);
	DeclareConstIntCVar(CommunicationForceTestVoicePack, 0);
	DeclareConstIntCVar(SoundPerception, 1);
	DeclareConstIntCVar(IgnorePlayer, 0);
	DeclareConstIntCVar(IgnoreVisibilityChecks, 0);
	DeclareConstIntCVar(DrawModifiers, 0);
	DeclareConstIntCVar(AllTime, 0);
	DeclareConstIntCVar(ProfileGoals, 0);
	DeclareConstIntCVar(BeautifyPath, 1);
	DeclareConstIntCVar(PathStringPullingIterations, 5);
	DeclareConstIntCVar(AttemptStraightPath, 1);
	DeclareConstIntCVar(PredictivePathFollowing, 1);
	DeclareConstIntCVar(CrowdControlInPathfind, 0);
	DeclareConstIntCVar(DebugDrawCrowdControl, 0);
	DeclareConstIntCVar(UpdateProxy, 1);
	DeclareConstIntCVar(DrawType, -1);
	DeclareConstIntCVar(AdjustPathsAroundDynamicObstacles, 1);
	DeclareConstIntCVar(DrawFormations, 0);
	DeclareConstIntCVar(DrawSmartObjects, 0);
	DeclareConstIntCVar(DrawReadibilities, 0);
	DeclareConstIntCVar(DrawGoals, 0);
	DeclareConstIntCVar(DrawNodeLinkType, 0);
	DeclareConstIntCVar(DrawTargets, 0);
	DeclareConstIntCVar(DrawBadAnchors, -1);
	DeclareConstIntCVar(DrawStats, 0);
	DeclareConstIntCVar(DrawAttentionTargetsPosition, 0);
	DeclareConstIntCVar(DrawHideSpotSearchRays, 0);

	DeclareConstIntCVar(DebugDrawVegetationCollisionDist, 0);
	DeclareConstIntCVar(RecordLog, 0);
	DeclareConstIntCVar(DrawTrajectory, 0);
	DeclareConstIntCVar(DrawHideSpots, 0);
	DeclareConstIntCVar(DrawRadar, 0);
	DeclareConstIntCVar(DrawRadarDist, 20);
	DeclareConstIntCVar(DebugRecordAuto, 0);
	DeclareConstIntCVar(DebugRecordBuffer, 1024);
	DeclareConstIntCVar(DrawGroupTactic, 0);
	DeclareConstIntCVar(UpdateAllAlways, 0);
	DeclareConstIntCVar(DrawDistanceLUT, 0);
	DeclareConstIntCVar(DrawAreas, 0);
	DeclareConstIntCVar(DrawProbableTarget, 0);
	DeclareConstIntCVar(DebugDrawDamageParts, 0);
	DeclareConstIntCVar(DebugDrawStanceSize, 0);
	DeclareConstIntCVar(DebugDrawUpdate, 0);
	DeclareConstIntCVar(DebugDrawEnabledActors, 0);
	DeclareConstIntCVar(DebugDrawEnabledPlayers, 0);
	DeclareConstIntCVar(DebugDrawLightLevel, 0);
	DeclareConstIntCVar(DebugDrawPhysicsAccess, 0);
	DeclareConstIntCVar(RayCasterQuota, 12);
	DeclareConstIntCVar(IntersectionTesterQuota, 12);
	DeclareConstIntCVar(AmbientFireEnable, 1);
	DeclareConstIntCVar(DebugDrawAmbientFire, 0);
	DeclareConstIntCVar(PlayerAffectedByLight, 1);
	DeclareConstIntCVar(DebugDrawExpensiveAccessoryQuota, 0);
	DeclareConstIntCVar(DebugTargetSilhouette, 0);
	DeclareConstIntCVar(DebugDrawDamageControl, 0);
	DeclareConstIntCVar(DrawFakeTracers, 0);
	DeclareConstIntCVar(DrawFakeHitEffects, 0);
	DeclareConstIntCVar(DrawFakeDamageInd, 0);
	DeclareConstIntCVar(DrawPlayerRanges, 0);
	DeclareConstIntCVar(DrawPerceptionIndicators, 0);
	DeclareConstIntCVar(DrawPerceptionDebugging, 0);
	DeclareConstIntCVar(DrawPerceptionModifiers, 0);
	DeclareConstIntCVar(DebugGlobalPerceptionScale, 0);
	DeclareConstIntCVar(TargetTracking, 1);
	DeclareConstIntCVar(TargetTracks_GlobalTargetLimit, 0);
	DeclareConstIntCVar(TargetTracks_TargetDebugDraw, 0);
	DeclareConstIntCVar(TargetTracks_ConfigDebugDraw, 0);

	DeclareConstIntCVar(ForceStance, -1);
	DeclareConstIntCVar(ForceAllowStrafing, -1);

	DeclareConstIntCVar(DebugDrawAdaptiveUrgency, 0);
	DeclareConstIntCVar(DebugDrawReinforcements, -1);
	DeclareConstIntCVar(DebugDrawPlayerActions, 0);

	DeclareConstIntCVar(SimpleWayptPassability, 1);

	DeclareConstIntCVar(WaterOcclusionEnable, 1);

	DeclareConstIntCVar(EnablePerceptionStanceVisibleRange, 0);
	DeclareConstIntCVar(IgnoreVisualStimulus, 0);
	DeclareConstIntCVar(IgnoreSoundStimulus, 0);
	DeclareConstIntCVar(IgnoreBulletRainStimulus, 0);

	// Interest system
	DeclareConstIntCVar(InterestSystem, 1);
	DeclareConstIntCVar(DebugInterest, 0);
	DeclareConstIntCVar(InterestSystemCastRays, 1);

	// Path Follower
	DeclareConstIntCVar(UseSmartPathFollower, 1);
	DeclareConstIntCVar(SmartpathFollower_UseAABB_CheckWalkibility, 1);
	DeclareConstIntCVar(SmartPathFollower_useAdvancedPathShortcutting, 1);
	DeclareConstIntCVar(SmartPathFollower_useAdvancedPathShortcutting_debug, 0);

	DeclareConstIntCVar(DrawPathFollower, 0);

	DeclareConstIntCVar(MNMPathfinderMT, 1);
	DeclareConstIntCVar(MNMPathfinderConcurrentRequests, 4);
	DeclareConstIntCVar(MNMRaycastImplementation, 2);

	DeclareConstIntCVar(LogConsoleVerbosity, 0);
	DeclareConstIntCVar(LogFileVerbosity, 0);
	DeclareConstIntCVar(EnableWarningsErrors, 0);

	DeclareConstIntCVar(Recorder, 0);
	DeclareConstIntCVar(StatsDisplayMode, 0);

	DeclareConstIntCVar(VisionMapNumberOfPVSUpdatesPerFrame, 1);
	DeclareConstIntCVar(VisionMapNumberOfVisibilityUpdatesPerFrame, 1);

	DeclareConstIntCVar(DebugDrawVisionMap, 0);
	DeclareConstIntCVar(DebugDrawVisionMapStats, 1);
	DeclareConstIntCVar(DebugDrawVisionMapVisibilityChecks, 1);
	DeclareConstIntCVar(DebugDrawVisionMapObservers, 1);
	DeclareConstIntCVar(DebugDrawVisionMapObserversFOV, 0);
	DeclareConstIntCVar(DebugDrawVisionMapObservables, 1);

	DeclareConstIntCVar(DrawFireEffectEnabled, 1);

	DeclareConstIntCVar(AllowedToHitPlayer, 1);
	DeclareConstIntCVar(AllowedToHit, 1);
	DeclareConstIntCVar(EnableCoolMisses, 1);

	DeclareConstIntCVar(DynamicHidespotsEnabled, 0);

	DeclareConstIntCVar(ForceSerializeAllObjects, 0);

	DeclareConstIntCVar(EnableORCA, 1);
	DeclareConstIntCVar(CollisionAvoidanceUpdateVelocities, 1);
	DeclareConstIntCVar(CollisionAvoidanceEnableRadiusIncrement, 1);
	DeclareConstIntCVar(CollisionAvoidanceClampVelocitiesWithNavigationMesh, 0);
	DeclareConstIntCVar(DebugDrawCollisionAvoidance, 0);

	DeclareConstIntCVar(BubblesSystemAlertnessFilter, 7);
	DeclareConstIntCVar(BubblesSystemUseDepthTest, 0);
	DeclareConstIntCVar(BubblesSystemAllowPrototypeDialogBubbles, 0);

	DeclareConstIntCVar(PathfinderDangerCostForAttentionTarget, 5);
	DeclareConstIntCVar(PathfinderDangerCostForExplosives, 2);
	DeclareConstIntCVar(PathfinderAvoidanceCostForGroupMates, 2);
	float PathfinderExplosiveDangerRadius;
	float PathfinderExplosiveDangerMaxThreatDistance;
	float PathfinderGroupMatesAvoidanceRadius;

	DeclareConstIntCVar(DebugMovementSystem, 0);
	DeclareConstIntCVar(DebugMovementSystemActorRequests, 0);
	DeclareConstIntCVar(OutputPersonalLogToConsole, 0);
	DeclareConstIntCVar(DrawModularBehaviorTreeStatistics, 0);
	DeclareConstIntCVar(LogModularBehaviorTreeExecutionStacks, 0);

	DeclareConstIntCVar(MNMPathfinderPositionInTrianglePredictionType, 1);

	int         MovementSystemPathReplanningEnabled;

	const char* DrawPath;
	const char* DrawPathAdjustment;

	float       CheckWalkabilityOptimalSectionLength;
	float       TacticalPointUpdateTime;
	const char* CompatibilityMode;
	float       AllowedTimeForPathfinding;
	float       PathfinderUpdateTime;
	float       DrawAgentFOV;
	const char* FilterAgentName;
	float       AgentStatsDist;
	int         AiSystem;
	float       DebugDrawArrowLabelsVisibilityDistance;
	const char* DebugDrawAStarOpenList;
	float       DebugDrawAStarOpenListTime;

	float       CoverPredictTarget;
	float       CoverSpacing;

	const char* StatsTarget;
	float       AIUpdateInterval;

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

	const char* DrawRefPoints;
	const char* DrawNode;
	float       DrawNodeLinkCutoff;
	const char* DrawLocate;
	float       DebugDrawOffset;
	float       SteepSlopeUpValue;
	float       SteepSlopeAcrossValue;
	float       ExtraRadiusDuringBeautification;
	float       ExtraForbiddenRadiusDuringBeautification;
	const char* DrawShooting;
	float       BurstWhileMovingDestinationRange;
	const char* DrawAgentStats;

	float       SOMSpeedRelaxed;
	float       SOMSpeedCombat;

	float       SightRangeSuperDarkIllumMod;
	float       SightRangeDarkIllumMod;
	float       SightRangeMediumIllumMod;

	float       RODAliveTime;
	float       RODMoveInc;
	float       RODStanceInc;
	float       RODDirInc;
	float       RODAmbientFireInc;
	float       RODKillZoneInc;
	float       RODFakeHitChance;

	float       RODKillRangeMod;
	float       RODCombatRangeMod;

	float       RODReactionTime;
	float       RODReactionSuperDarkIllumInc;
	float       RODReactionDarkIllumInc;
	float       RODReactionMediumIllumInc;
	float       RODReactionDistInc;
	float       RODReactionDirInc;
	float       RODReactionLeanInc;

	float       RODLowHealthMercyTime;

	float       RODCoverFireTimeMod;

	int         AmbientFireQuota;
	float       AmbientFireUpdateInterval;

	const char* DrawPerceptionHandlerModifiers;
	const char* TargetTracks_AgentDebugDraw;
	const char* TargetTracks_ConfigDebugFilter;
	const char* DrawAgentStatsGroupFilter;

	int         DrawSelectedTargets;

	const char* ForceAGAction;
	const char* ForceAGSignal;
	const char* ForcePosture;
	const char* ForceLookAimTarget;

	float       BannedNavSoTime;
	float       WaterOcclusionScale;

	float       MinActorDynamicObstacleAvoidanceRadius;
	float       ExtraVehicleAvoidanceRadiusBig;
	float       ExtraVehicleAvoidanceRadiusSmall;
	float       ObstacleSizeThreshold;
	float       DrawGetEnclosingFailures;

	// Perception handler override
	float       CrouchVisibleRange;
	float       ProneVisibleRange;

	int         MNMDebugAccessibility; // TODO: remove
	const char* MNMDebugDrawFlag;

	int         MNMEditorBackgroundUpdate;

	float       MNMPathFinderQuota;
	int         MNMPathFinderDebug;

	int         MNMProfileMemory;

	int         MNMAllowDynamicRegenInEditor;

	int         EnableBubblesSystem;
	float       BubblesSystemFontSize;
	float       BubblesSystemDecayTime;
	const char* BubblesSystemNameFilter;

	float       OverlayMessageDuration;
	float       DrawFireEffectDecayRange;
	float       DrawFireEffectMinDistance;
	float       DrawFireEffectMinTargetFOV;
	float       DrawFireEffectMaxAngle;
	float       DrawFireEffectTimeScale;

	float       CoolMissesBoxSize;
	float       CoolMissesBoxHeight;
	float       CoolMissesMinMissDistance;
	float       CoolMissesMaxLightweightEntityMass;
	float       CoolMissesProbability;
	float       CoolMissesCooldown;

	float       SmartPathFollower_LookAheadDistance;
	float       SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk;
	float       SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint;
	float       SmartPathFollower_decelerationHuman;
	float       SmartPathFollower_decelerationVehicle;

	float       LobThrowMinAllowedDistanceFromFriends;
	float       LobThrowTimePredictionForFriendPositions;
	float       LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation;
	DeclareConstIntCVar(LobThrowSimulateRandomInaccuracy, 0);

	int         LogSignals;

	int         ModularBehaviorTree;
	int         DebugTimestamps;

	float       CommunicationManagerHeightThresholdForTargetPosition;

	static void CommTest(IConsoleCmdArgs* args);
	static void CommTestStop(IConsoleCmdArgs* args);
	static void CommWriteStats(IConsoleCmdArgs* args);
	static void CommResetStats(IConsoleCmdArgs* args);
	static void DebugMNMAgentType(IConsoleCmdArgs* args);
	static void MNMCalculateAccessibility(IConsoleCmdArgs* args); // TODO: Remove when the seeds work
	static void MNMComputeConnectedIslands(IConsoleCmdArgs* args);
	static void NavigationReloadConfig(IConsoleCmdArgs* args);
	static void DebugAgent(IConsoleCmdArgs* args);
	static void AIBubblesNameFilterCallback(ICVar* pCvar);
};

#endif
