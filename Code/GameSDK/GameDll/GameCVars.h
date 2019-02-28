// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAMECVARS_H__
#define __GAMECVARS_H__

#include "GameConstantCVars.h"
#include "Network/Lobby/GameLobbyCVars.h"

#if PC_CONSOLE_NET_COMPATIBLE
	#define DEFAULT_MINPLAYERLIMIT 6
#else
	#define DEFAULT_MINPLAYERLIMIT 1
#endif

#if !defined(_RELEASE)
#define TALOS 1
#else
#define TALOS 0
#endif

#if PC_CONSOLE_NET_COMPATIBLE
#define USE_PC_PREMATCH 0
#else
#define USE_PC_PREMATCH 1
#endif


struct SCaptureTheFlagParams
{
	float carryingFlag_SpeedScale;
};

struct SExtractionParams
{
		float carryingTick_SpeedScale;
		float carryingTick_EnergyCostPerHit;
		float carryingTick_DamageAbsorbDesperateEnergyCost;
};

struct SPredatorParams
{
	float hudTimerAlertWhenTimeRemaining;
	float hintMessagePauseTime;
};

struct SDeathCamSPParams
{
	int enable;
	int dof_enable;

	float updateFrequency;
	float dofRange;
	float dofRangeNoKiller;
	float dofRangeSpeed;
	float dofDistanceSpeed;
};

struct SPowerSprintParams
{
	float foward_angle;
};

struct SJumpAirControl
{
	float air_control_scale;
	float air_resistance_scale;
	float air_inertia_scale;
};

struct SPlayerHealth
{
	float normal_regeneration_rateSP;
	float critical_health_thresholdSP;
	float critical_health_updateTimeSP;
	float normal_threshold_time_to_regenerateSP;

	float normal_regeneration_rateMP;
	float critical_health_thresholdMP;
	float fast_regeneration_rateMP;
	float slow_regeneration_rateMP;
	float normal_threshold_time_to_regenerateMP;

	int   enable_FallandPlay;
	int		collision_health_threshold;

	float fallDamage_SpeedSafe;
	float fallDamage_SpeedFatal;
	float fallSpeed_HeavyLand;
	float fallDamage_SpeedFatalArmor;
	float fallSpeed_HeavyLandArmor;
	float fallDamage_SpeedSafeArmorMP;
	float fallDamage_SpeedFatalArmorMP;
	float fallSpeed_HeavyLandArmorMP;
	float fallDamage_CurveAttackMP; 
	float fallDamage_CurveAttack;
	int		fallDamage_health_threshold;
	int		debug_FallDamage;
	int		enableNewHUDEffect;
	int		minimalHudEffect;
};

struct SAltNormalization
{
	int enable;
	float hud_ctrl_Curve_Unified;
	float hud_ctrl_Coeff_Unified;
};

struct SPlayerMovement
{
	float nonCombat_heavy_weapon_speed_scale;
	float nonCombat_heavy_weapon_sprint_scale;
	float nonCombat_heavy_weapon_strafe_speed_scale;
	float nonCombat_heavy_weapon_crouch_speed_scale;

	float power_sprint_targetFov;

	float ground_timeInAirToFall;

	float speedScale;
	float strafe_SpeedScale;
	float sprint_SpeedScale;
	float crouch_SpeedScale;

	int sprintStamina_debug;

	float mp_slope_speed_multiplier_uphill;
	float mp_slope_speed_multiplier_downhill;
	float mp_slope_speed_multiplier_minHill;
};

struct SPlayerMelee
{
	enum EImpulsesState
	{
		ei_Disabled,
		ei_OnlyToAlive,
		ei_OnlyToDead,
		ei_FullyEnabled,
	};

	float	melee_snap_angle_limit;
	float melee_snap_blend_speed;
	float melee_snap_target_select_range;
	float melee_snap_end_position_range;
	float melee_snap_move_speed_multiplier;
	float damage_multiplier_from_behind;
	float damage_multiplier_mp;
	float angle_limit_from_behind;
	float mp_victim_screenfx_intensity;
	float mp_victim_screenfx_duration;
	float mp_victim_screenfx_blendout_duration;
	float mp_victim_screenfx_dbg_force_test_duration;
	int		impulses_enable;
	int		debug_gfx;
	int		mp_melee_system;
	int		mp_melee_system_camera_lock_and_turn;
	int		mp_knockback_enabled;
	float	mp_melee_system_camera_lock_time;
	float mp_melee_system_camera_lock_crouch_height_offset;
	float mp_knockback_strength_vert;
	float mp_knockback_strength_hor;
	int		mp_sliding_auto_melee_enabled;
};

struct SSpectacularKillCVars
{
	float maxDistanceError;
	float minTimeBetweenKills;
	float minTimeBetweenSameKills;
	float minKillerToTargetDotProduct;
	float maxHeightBetweenActors;
	float sqMaxDistanceFromPlayer;
	int		debug;
};

struct SPlayerLedgeClamber
{
	float cameraBlendWeight;
	int		debugDraw;
	int		enableVaultFromStanding;
};

struct SPlayerLadder
{
	int ladder_renderPlayerLast;
	int	ladder_logVerbosity;
};

struct SPlayerPickAndThrow
{
	int		debugDraw;
	int		useProxies;
	int   cloakedEnvironmentalWeaponsAllowed; 

	float maxOrientationCorrectionTime;
	float orientationCorrectionTimeMult;

#ifndef _RELEASE
	float correctOrientationDistOverride; 
	float correctOrientationPosOverrideX;
	float correctOrientationPosOverrideY;
	int		correctOrientationPosOverrideEnabled;
	int		chargedThrowDebugOutputEnabled; 
	int		environmentalWeaponHealthDebugEnabled;
	int   environmentalWeaponImpactDebugEnabled; 
	int   environmentalWeaponComboDebugEnabled; 
	int   environmentalWeaponRenderStatsEnabled;
	int   environmentalWeaponDebugSwing;
#endif // #ifndef _RELEASE

	float environmentalWeaponObjectImpulseScale; 
	float environmentalWeaponImpulseScale;	
	float environmentalWeaponHitConeInDegrees; 
	float minRequiredThrownEnvWeaponHitVelocity; 
	float awayFromPlayerImpulseRatio; 
	float environmentalWeaponDesiredRootedGrabAnimDuration; 
	float environmentalWeaponDesiredGrabAnimDuration; 
	float environmentalWeaponDesiredPrimaryAttackAnimDuration;
	float environmentalWeaponDesiredComboAttackAnimDuration;
	float environmentalWeaponUnrootedPickupTimeMult;
	float environmentalWeaponThrowAnimationSpeed;
	float environmentalWeaponFlippedImpulseOverrideMult;
	float environmentalWeaponFlipImpulseThreshold; 
	float environmentalWeaponLivingToArticulatedImpulseRatio; 

	int		enviromentalWeaponUseThrowInitialFacingOveride;

	// View
	float environmentalWeaponMinViewClamp; 
	float environmentalWeaponViewLerpZOffset;
	float environmentalWeaponViewLerpSmoothTime;

	// Env weap melee Auto aim
	float complexMelee_snap_angle_limit;
	float complexMelee_lerp_target_speed;

	// Object impulse helpers
	float objectImpulseLowMassThreshold; 
	float objectImpulseLowerScaleLimit;

	// Combo settings
	float comboInputWindowSize;	  
	float minComboInputWindowDurationInSecs; 

	// Impact helpers
	float impactNormalGroundClassificationAngle; 
	float impactPtValidHeightDiffForImpactStick; 
	float reboundAnimPlaybackSpeedMultiplier;
	int   environmentalWeaponSweepTestsEnabled;

	// Charged throw auto aim
	float chargedThrowAutoAimDistance;
	float chargedThrowAutoAimConeSize;
	float chargedThrowAutoAimDistanceHeuristicWeighting; 
	float chargedThrowAutoAimAngleHeuristicWeighting;
	float chargedThrowAimHeightOffset;
	int   chargedthrowAutoAimEnabled;

	int		intersectionAssistDebugEnabled;
	int		intersectionAssistDeleteObjectOnEmbed; 
	float intersectionAssistCollisionsPerSecond;
	float intersectionAssistTimePeriod;
	float intersectionAssistTranslationThreshold;
	float intersectionAssistPenetrationThreshold;
};

struct SPlayerSlideControl
{
	float min_speed_threshold;
	float min_speed;

	float deceleration_speed;
	float min_downhill_threshold;
	float max_downhill_threshold;
	float max_downhill_acceleration;
};

// this block of vars is no longer used. The lua code that was using it, is no longer being called - and probably need to be removed too - 
struct SPlayerEnemyRamming
{
	float player_to_player;
	float ragdoll_to_player;
	float fall_damage_threashold;
	float safe_falling_speed;
	float fatal_falling_speed;
	float max_falling_damage;
	float min_momentum_to_fall;
};

struct SAICollisions
{
	float minSpeedForFallAndPlay;
	float minMassForFallAndPlay;
	float dmgFactorWhenCollidedByObject;
	int showInLog;
};

//////////////////////////////////////////////////////////////////////////

struct SPostEffect
{	// Use same naming convention as the post effects in the engine
	float FilterGrain_Amount;
	float FilterRadialBlurring_Amount;
	float FilterRadialBlurring_ScreenPosX;
	float FilterRadialBlurring_ScreenPosY;
	float FilterRadialBlurring_Radius;
	float Global_User_ColorC;
	float Global_User_ColorM;
	float Global_User_ColorY;
	float Global_User_ColorK;
	float Global_User_Brightness;
	float Global_User_Contrast;
	float Global_User_Saturation;
	float Global_User_ColorHue;
	float HUD3D_Interference;
	float HUD3D_FOV;
};

struct SSilhouette
{
	int			enableUpdate;
	float		r;
	float		g;
	float		b;
	float		a;
	int			enabled;
};

struct SAIPerceptionCVars
{
	int			movement_useSurfaceType;
	float		movement_movingSurfaceDefault;
	float		movement_standingRadiusDefault;
	float		movement_crouchRadiusDefault;
	float		movement_standingMovingMultiplier;
	float		movement_crouchMovingMultiplier;

	float		landed_baseRadius;
	float		landed_speedMultiplier;
};

struct SAIThreatModifierCVars
{
	char const* DebugAgentName;

	float SOMIgnoreVisualRatio;
	float SOMDecayTime;

	float SOMMinimumRelaxed;
	float SOMMinimumCombat;
	float SOMCrouchModifierRelaxed;
	float SOMCrouchModifierCombat;
	float SOMMovementModifierRelaxed;
	float SOMMovementModifierCombat;
	float SOMWeaponFireModifierRelaxed;
	float SOMWeaponFireModifierCombat;

	float SOMCloakMaxTimeRelaxed;
	float SOMCloakMaxTimeCombat;
	float SOMUncloakMinTimeRelaxed;
	float SOMUncloakMinTimeCombat;
	float SOMUncloakMaxTimeRelaxed;
	float SOMUncloakMaxTimeCombat;
};

struct SCVars
{	
	static const float v_altitudeLimitDefault()
	{
		return 600.0f;
	}
	
	SGameReleaseConstantCVars m_releaseConstants;

	float cl_fov;
	float cl_mp_fov_scalar;
	int   cl_tpvCameraCollision;
	float cl_tpvCameraCollisionOffset;
	int   cl_tpvDebugAim;
	float cl_tpvDist;
	float cl_tpvDistHorizontal;
	float cl_tpvDistVertical;
	float cl_tpvInterpolationSpeed;
	float cl_tpvMaxAimDist;
	float cl_tpvMinDist;
	float cl_tpvYaw;
	float cl_sensitivity;
	float cl_sensitivityController;
	float cl_sensitivityControllerMP;
	int		cl_invertMouse;
	int		cl_invertController;
	int		cl_invertLandVehicleInput;
	int		cl_invertAirVehicleInput;
	int		cl_crouchToggle;
	int		cl_sprintToggle;
	int		cl_debugSwimming;
	int		cl_logAsserts;
	int		cl_zoomToggle;
	float	cl_motionBlurVectorScale;
	float	cl_motionBlurVectorScaleSprint;
	int		g_enableMPDoubleTapGrenadeSwitch;

	ICVar* 	ca_GameControlledStrafingPtr;

	float cl_shallowWaterSpeedMulPlayer;
	float cl_shallowWaterSpeedMulAI;
	float cl_shallowWaterDepthLo;
	float cl_shallowWaterDepthHi;

	float cl_speedToBobFactor;
	float cl_bobWidth;
	float cl_bobHeight;
	float cl_bobSprintMultiplier;
	float cl_bobVerticalMultiplier;
	float cl_bobMaxHeight;
	float cl_strafeHorzScale;
	float cl_controllerYawSnapEnable;
	float cl_controllerYawSnapAngle;
	float cl_controllerYawSnapTime;
	float cl_controllerYawSnapMax;
	float cl_controllerYawSnapMin;
	float cl_angleLimitTransitionTime;
	
#ifndef _RELEASE
	int g_interactiveObjectDebugRender; 
#endif //#ifndef _RELEASE

	//Interactive Entity Highlighting
	float g_highlightingMaxDistanceToHighlightSquared;
	float g_highlightingMovementDistanceToUpdateSquared;
	float g_highlightingTimeBetweenForcedRefresh;

	float g_ledgeGrabClearHeight; 
	float g_ledgeGrabMovingledgeExitVelocityMult;
	float g_vaultMinHeightDiff; 
	float g_vaultMinAnimationSpeed;

	int g_cloakFlickerOnRun;
	
#ifndef _RELEASE
	int kc_debugMannequin;
	int kc_debugPacketData;

	int		g_mptrackview_debug;
	int		g_hud_postgame_debug;
#endif	// #ifndef _RELEASE

	int		g_VTOLVehicleManagerEnabled; 
	float g_VTOLMaxGroundVelocity;
	float g_VTOLDeadStateRemovalTime;
	float g_VTOLDestroyedContinueVelocityTime;
	float g_VTOLRespawnTime;
	float g_VTOLGravity;
	float g_VTOLPathingLookAheadDistance;
	float g_VTOLOnDestructionPlayerDamage;
	float g_VTOLInsideBoundsScaleX;
	float g_VTOLInsideBoundsScaleY;
	float g_VTOLInsideBoundsScaleZ;
	float g_VTOLInsideBoundsOffsetZ;

#ifndef _RELEASE
	int g_VTOLVehicleManagerDebugEnabled; 
	int g_VTOLVehicleManagerDebugPathingEnabled;
	int g_VTOLVehicleManagerDebugSimulatePath;
	float g_VTOLVehicleManagerDebugSimulatePathTimestep;
	float g_VTOLVehicleManagerDebugSimulatePathMinDotValueCheck;
	int g_vtolManagerDebug;

	int g_mpPathFollowingRenderAllPaths;
	float g_mpPathFollowingNodeTextSize;
#endif

	int		kc_useAimAdjustment;
	float kc_aimAdjustmentMinAngle;
	float	kc_precacheTimeStartPos;
	float	kc_precacheTimeWeaponModels;

	int g_useQuickGrenadeThrow; 
	int g_debugWeaponOffset;

#ifndef _RELEASE
	int g_PS_debug;
	int kc_debugStream;
#endif

	int g_MicrowaveBeamStaticObjectMaxChunkThreshold;

	float i_fastSelectMultiplier;

	float	cl_idleBreaksDelayTime;

	int		cl_postUpdateCamera;

	int p_collclassdebug;

#if !defined(_RELEASE)
	const char* pl_viewFollow;
	float pl_viewFollowOffset;
	float pl_viewFollowMinRadius;
	float pl_viewFollowMaxRadius;
	int		pl_watch_projectileAimHelper;
#endif

	float pl_cameraTransitionTime;
	float pl_cameraTransitionTimeLedgeGrabIn;
	float pl_cameraTransitionTimeLedgeGrabOut;

	float	pl_slideCameraFactor;

	float	cl_slidingBlurRadius;
	float	cl_slidingBlurAmount;
	float	cl_slidingBlurBlendSpeed;

	int   sv_votingTimeout;
	int   sv_votingCooldown;
	int   sv_votingEnable;
	int   sv_votingMinVotes;
	float sv_votingRatio;
	float sv_votingTeamRatio;
	float sv_votingBanTime;
	

	int   sv_input_timeout;

	ICVar*sv_aiTeamName;

	ICVar *performance_profile_logname;

	int		g_infiniteAmmoTutorialMode;

	int		i_lighteffects;
	int		i_particleeffects;
	int		i_rejecteffects;
	int		i_grenade_showTrajectory;
	float	i_grenade_trajectory_resolution;
	float	i_grenade_trajectory_dashes;
	float	i_grenade_trajectory_gaps;
	int		i_grenade_trajectory_useGeometry;

	int		i_ironsight_while_jumping_mp;
	int		i_ironsight_while_falling_mp;
	float	i_ironsight_falling_unzoom_minAirTime;
	float	i_weapon_customisation_transition_time;
	int		i_highlight_dropped_weapons;

	float i_laser_hitPosOffset;

	float pl_inputAccel;
	int		pl_debug_energyConsumption;
	int		pl_debug_pickable_items;
	float	pl_useItemHoldTime;
	int		pl_autoPickupItemsWhenUseHeld;
	float	pl_autoPickupMinTimeBetweenPickups;
	int		pl_debug_projectileAimHelper; 

	float pl_nanovision_timeToRecharge;
	float pl_nanovision_timeToDrain;
	float pl_nanovision_minFractionToUse;

	float pl_refillAmmoDelay;

	int		pl_spawnCorpseOnDeath;

	int		pl_doLocalHitImpulsesMP;

	int kc_enable;
	int kc_debug;
	int kc_debugStressTest;
	int kc_debugVictimPos;
	int kc_debugWinningKill;
	int kc_debugSkillKill;
	int kc_memStats;
	int kc_maxFramesToPlayAtOnce;
	int kc_cameraCollision;
	int kc_showHighlightsAtEndOfGame;
	int kc_enableWinningKill;
	int kc_canSkip;
	float kc_length;
	float kc_skillKillLength;
	float kc_bulletSpeed;
	float kc_bulletHoverDist;
	float kc_bulletHoverTime;
	float kc_bulletHoverTimeScale;
	float kc_bulletPostHoverTimeScale;
	float kc_bulletTravelTimeScale;
	float kc_bulletCamOffsetX;
	float kc_bulletCamOffsetY;
	float kc_bulletCamOffsetZ;
	float kc_bulletRiflingSpeed;
	float kc_bulletZoomDist;
	float kc_bulletZoomTime;
	float kc_bulletZoomOutRatio;
	float kc_kickInTime;
	float kc_projectileDistance;
	float kc_projectileHeightOffset;
	float kc_largeProjectileDistance;
	float kc_largeProjectileHeightOffset;
	float kc_projectileVictimHeightOffset;
	float kc_projectileMinimumVictimDist;
	float kc_smoothing;
	float kc_grenadeSmoothingDist;
	float kc_cameraRaiseHeight;
	float kc_resendThreshold;
	float kc_chunkStreamTime;

#if !defined(_RELEASE)
	int kc_copyKillCamIntoHighlightsBuffer;
#endif

	float g_multikillTimeBetweenKills;
	float g_flushed_timeBetweenGrenadeBounceAndSkillKill;
	float g_gotYourBackKill_targetDistFromFriendly;
	float g_gotYourBackKill_FOVRange;
	float g_guardian_maxTimeSinceLastDamage;
	float g_defiant_timeAtLowHealth;
	float g_defiant_lowHealthFraction;
	float g_intervention_timeBetweenZoomedAndKill;
	float g_blinding_timeBetweenFlashbangAndKill;
	float g_blinding_flashbangRecoveryDelayFrac;
	float g_neverFlagging_maxMatchTimeRemaining;
	float g_combinedFire_maxTimeBetweenWeapons;

	float g_fovToRotationSpeedInfluence;
	
	int		dd_maxRMIsPerFrame;
	float dd_waitPeriodBetweenRMIBatches;

	int g_debugSpawnPointsRegistration;
	int g_debugSpawnPointValidity;
	float g_randomSpawnPointCacheTime;

	int		g_detachCamera;
	int		g_moveDetachedCamera;
	float g_detachedCameraMoveSpeed;
	float g_detachedCameraRotateSpeed;
	float g_detachedCameraTurboBoost;
	int g_detachedCameraDebug;
	int   g_difficultyLevel;
	int   g_difficultyLevelLowestPlayed;
	float g_flashBangMinSpeedMultiplier;
	float g_flashBangSpeedMultiplierFallOffEase;
	float g_flashBangNotInFOVRadiusFraction;
	float g_flashBangMinFOVMultiplier;
	int		g_flashBangFriends;
	int		g_flashBangSelf;
	float	g_friendlyLowerItemMaxDistance;

	int		g_holdObjectiveDebug;

#if !defined(_RELEASE)
	int		g_debugOffsetCamera;
	int		g_debugLongTermAwardDays;
#endif //!defined(_RELEASE)

	int		g_gameRayCastQuota;
	int		g_gameIntersectionTestQuota;

	int		g_STAPCameraAnimation;

	int   g_debugaimlook;
	float g_playerLowHealthThreshold;
	float g_playerMidHealthThreshold;

	int g_SurvivorOneVictoryConditions_watchLvl;
	int g_SimpleEntityBasedObjective_watchLvl;
	int g_CTFScoreElement_watchLvl;
	int g_KingOfTheHillObjective_watchLvl;
	float g_HoldObjective_secondsBeforeStartForSpawn;

	int g_CombiCaptureObjective_watchLvl;
	int g_CombiCaptureObjective_watchTerminalSignalPlayers;

	int g_disable_OpponentsDisconnectedGameEnd;
	int g_victoryConditionsDebugDrawResolution;
#if USE_PC_PREMATCH
	int g_restartWhenPrematchFinishes;
#endif

#ifndef _RELEASE
	int g_predator_forceSpawnTeam;
	int g_predator_predatorHasSuperRadar;
	ICVar *g_predator_forcePredator1;
	ICVar *g_predator_forcePredator2;
	ICVar *g_predator_forcePredator3;
	ICVar *g_predator_forcePredator4;
#endif

	float g_predator_marineRedCrosshairDelay;

	int sv_pacifist;
	int g_devDemo;

	int g_bulletPenetrationDebug;
	float g_bulletPenetrationDebugTimeout;

	int g_fpDbaManagementEnable;
	int g_fpDbaManagementDebug;
	int g_charactersDbaManagementEnable;
	int g_charactersDbaManagementDebug;

	int g_thermalVisionDebug;

	float g_droppedItemVanishTimer;
	float g_droppedHeavyWeaponVanishTimer;

	int		g_corpseManager_maxNum;
	float g_corpseManager_thermalHeatFadeDuration;
	float g_corpseManager_thermalHeatMinValue;
	float g_corpseManager_timeoutInSeconds;

	float g_explosion_materialFX_raycastLength;

	// explosion culling
	int		g_ec_enable;
	float g_ec_radiusScale;
	float g_ec_volume;
	float g_ec_extent;
	int		g_ec_removeThreshold;

	float g_radialBlur;

	float g_timelimit;
	float g_timelimitextratime;
	float g_roundScoreboardTime;
	float g_roundStartTime;
	int		g_roundlimit;
	float g_friendlyfireratio;
	int   g_revivetime; 
	int   g_minplayerlimit;
	float g_hostMigrationResumeTime;
	int   g_hostMigrationUseAutoLobbyMigrateInPrivateGames;
	int   g_minPlayersForRankedGame;
	float g_gameStartingMessageTime;

	int		g_mpRegenerationRate;
	int		g_mpHeadshotsOnly;
	int		g_mpNoVTOL;
	int		g_mpNoEnvironmentalWeapons;
	int		g_allowCustomLoadouts;
	int		g_allowFatalityBonus;
	float g_spawnPrecacheTimeBeforeRevive;
	float g_autoReviveTime;
	float g_spawn_timeToRetrySpawnRequest;
	float g_spawn_recentSpawnTimer;
	float g_forcedReviveTime;
	int		g_numLives;
	int		g_autoAssignTeams;
	float g_maxHealthMultiplier;
	int		g_mp_as_DefendersMaxHealth;
	float g_xpMultiplyer;
	int   g_allowExplosives;
	int   g_forceWeapon;
	int		g_allowSpectators;
	int		g_infiniteCloak;
	int		g_allowWeaponCustomisation;
	ICVar*	g_forceHeavyWeapon;
	ICVar*  g_forceLoadoutPackage;

	int g_switchTeamAllowed;
	int g_switchTeamRequiredPlayerDifference;
	int g_switchTeamUnbalancedWarningDifference;
	float g_switchTeamUnbalancedWarningTimer;

	int   g_tk_punish;
	int		g_tk_punish_limit;

	float g_hmdFadeoutNearWallThreshold;

	int   g_debugNetPlayerInput;
	int   g_debugCollisionDamage;
	int   g_debugHits;
	int   g_suppressHitSanityCheckWarnings;

#ifndef _RELEASE
	int		g_debugFakeHits;
	int		g_FEMenuCacheSaveList;
#endif

	int		g_enableLanguageSelectionMenu;
	int		g_multiplayerDefault;
	int		g_multiplayerModeOnly;
	int		g_EPD;
	int		g_frontEndRequiredEPD;
	int		g_EnableDevMenuOptions;
	int		g_frontEndUnicodeInput;
	int		g_DisableMenuButtons;
	int		g_EnablePersistantStatsDebugScreen;
	int		g_craigNetworkDebugLevel;
	int		g_presaleUnlock;
	int		g_dlcPurchaseOverwrite;
	int		g_MatchmakingVersion;
	int		g_MatchmakingBlock;
	int		g_enableInitialLoadoutScreen;
	int		g_post3DRendererDebug;
	int		g_post3DRendererDebugGridSegmentCount;

	int		g_ProcessOnlineCallbacks;
#if !defined(_RELEASE)
	int		g_debugTestProtectedScripts;
	int		g_preloadUIAssets;

	int   g_gameRules_skipStartTimer;
	
	int		g_skipStartupSignIn;	
#endif

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	#define g_gameRules_startCmd		g_gameRules_startCmdCVar->GetString()
	ICVar*	g_gameRules_startCmdCVar;
#endif
	float g_gameRules_startTimerLength;

	float g_gameRules_postGame_HUDMessageTime;
	float g_gameRules_postGame_Top3Time;
	float g_gameRules_postGame_ScoreboardTime;

	int		g_gameRules_startTimerMinPlayers;
	int		g_gameRules_startTimerMinPlayersPerTeam;
	float	g_gameRules_startTimerPlayersRatio;
	float	g_gameRules_startTimerOverrideWait;
	int		g_gameRules_preGame_StartSpawnedFrozen;

	int		g_debug_fscommand;
	int		g_skipIntro;
	int		g_skipAfterLoadingScreen;
	int		g_goToCampaignAfterTutorial;

	int   g_aiCorpses_Enable;
	int   g_aiCorpses_DebugDraw;
	int   g_aiCorpses_MaxCorpses;
	float	g_aiCorpses_DelayTimeToSwap;

	float g_aiCorpses_CullPhysicsDistance;
	float g_aiCorpses_ForceDeleteDistance;

	int		g_scoreLimit;
	int		g_scoreLimitOverride;

	float g_spawn_explosiveSafeDist;

	int g_logPrimaryRound;

#if USE_REGION_FILTER
	int g_server_region;
#endif

	int g_enableInitialLoginSilent;
	float g_dataRefreshFrequency;
	int		g_maxGameBrowserResults;


	//Inventory control
	int		g_inventoryNoLimits;
	int   g_inventoryWeaponCapacity;
	int		g_inventoryExplosivesCapacity;
	int		g_inventoryGrenadesCapacity;
	int		g_inventorySpecialCapacity;

#if !defined(_RELEASE)
	int		g_keepMPAudioSignalsPermanently;
#endif

	int g_loadoutServerControlled;
#if !defined(_RELEASE)
	int g_debugShowGainedAchievementsOnHUD;
#endif

	ICVar*pl_debug_filter;
	int		pl_debug_vistable;
	int		pl_debug_movement;
	int		pl_debug_jumping;
	int		pl_debug_aiming;
	int		pl_debug_aiming_input;
	int		pl_debug_view;
	int		pl_debug_hit_recoil;
	int		pl_debug_look_poses;

	int		pl_renderInNearest;

#if !defined(_RELEASE)
	int     pl_debug_vistableIgnoreEntities;
	int     pl_debug_vistableAddEntityId;
	int     pl_debug_vistableRemoveEntityId;
	int		pl_debug_watch_camera_mode;
	int		pl_debug_log_camera_mode_changes;
	int     pl_debug_log_player_plugins;
	int pl_shotDebug;
#endif

	int pl_aim_assistance_enabled;
	int pl_aim_assistance_disabled_atDifficultyLevel;
	int pl_aim_acceleration_enabled;
	float pl_aim_cloaked_multiplier;
	float pl_aim_near_lookat_target_distance;
	int pl_targeting_debug;

	int pl_switchTPOnKill;
	int pl_stealthKill_allowInMP;
	int pl_stealthKill_uncloakInMP;
	int pl_stealthKill_debug;
	float pl_stealthKill_aimVsSpineLerp;
	float pl_stealthKill_maxVelocitySquared;
	int pl_slealth_cloakinterference_onactionMP;
	int		pl_stealthKill_usePhysicsCheck; 
	int		pl_stealthKill_useExtendedRange;

	float pl_stealth_shotgunDamageCap;
	float pl_shotgunDamageCap;

	float pl_freeFallDeath_cameraAngle;
	float pl_freeFallDeath_fadeTimer;

	float pl_fall_intensity_multiplier;
	float pl_fall_intensity_max;
	float pl_fall_time_multiplier;
	float pl_fall_time_max;
	float pl_fall_intensity_hit_multiplier;
	
	float pl_TacticalScanDuration;
	float pl_TacticalScanDurationMP;
	float pl_TacticalTaggingDuration;
	float pl_TacticalTaggingDurationMP;

	float controller_power_curve;
	float controller_multiplier_z;
	float controller_multiplier_x;
	float controller_full_turn_multiplier_x;
	float controller_full_turn_multiplier_z;

	int ctrlr_corner_smoother;
	int ctrlr_corner_smoother_debug;

	int ctrlr_OUTPUTDEBUGINFO;

	float pl_stampTimeout;
	int		pl_stampTier;

	float pl_jump_maxTimerValue;
	float pl_jump_baseTimeAddedPerJump;
	float pl_jump_currentTimeMultiplierOnJump;

	int		pl_boostedMelee_allowInMP;

	float pl_velocityInterpAirControlScale;
	int pl_velocityInterpSynchJump;
	int pl_debugInterpolation;
	float pl_velocityInterpAirDeltaFactor;
	float pl_velocityInterpPathCorrection;
	int pl_velocityInterpAlwaysSnap;
	int pl_adjustJumpAngleWithFloorNormal;

	float pl_netAimLerpFactor;
	float pl_netSerialiseMaxSpeed;

#ifndef _RELEASE
	int pl_watchPlayerState;
#endif

	int pl_serialisePhysVel;
	float pl_clientInertia;
	float pl_fallHeight;

	float pl_legs_colliders_dist;
	float pl_legs_colliders_scale;

	SPowerSprintParams	 pl_power_sprint;
	SJumpAirControl pl_jump_control;
	SPlayerHealth pl_health;
	SPlayerMovement pl_movement;
	SPlayerLedgeClamber pl_ledgeClamber;
	SPlayerLadder pl_ladderControl;
	SPlayerPickAndThrow pl_pickAndThrow;
	SPlayerSlideControl	 pl_sliding_control;
	SPlayerSlideControl	 pl_sliding_control_mp;
	SPlayerEnemyRamming pl_enemy_ramming;
	SAICollisions AICollisions;	
	SPlayerMelee		pl_melee;
	SAltNormalization aim_altNormalization;
	SCaptureTheFlagParams mp_ctfParams;
	SExtractionParams mp_extractionParams;
	SPredatorParams mp_predatorParams;

	// animation triggered footsteps
	int   g_FootstepSoundsFollowEntity;
	int   g_FootstepSoundsDebug;
	float g_footstepSoundMaxDistanceSq;

#if !defined(_RELEASE)
	int   v_debugMovement;
	float v_debugMovementMoveVertically;
	float v_debugMovementX;
	float v_debugMovementY;
	float v_debugMovementZ;
	float v_debugMovementSensitivity;
#endif

	int   v_tankReverseInvertYaw;
	int   v_profileMovement;  
	const char* v_profile_graph;
	int   v_draw_suspension;
	int   v_draw_slip;
	int   v_pa_surface;    
	int   v_invertPitchControl;  
	float v_wind_minspeed; 
	float v_sprintSpeed;
	int   v_rockBoats;
	int   v_debugSounds;
	float v_altitudeLimit;
	ICVar* pAltitudeLimitCVar;
	float v_altitudeLimitLowerOffset;
	float v_MPVTOLNetworkSyncFreq;
	float v_MPVTOLNetworkSnapThreshold;
	float v_MPVTOLNetworkCatchupSpeedLimit;
	float v_stabilizeVTOL;
	ICVar* pVehicleQuality;
	float v_mouseRotScaleSP;
	float v_mouseRotLimitSP;
	float v_mouseRotScaleMP;
	float v_mouseRotLimitMP;

	float pl_swimBackSpeedMul;
	float pl_swimSideSpeedMul;
	float pl_swimVertSpeedMul;
	float pl_swimNormalSprintSpeedMul;
	int   pl_swimAlignArmsToSurface;

	int   pl_drownDamage;
	float pl_drownTime;
	float pl_drownRecoveryTime;
	float pl_drownDamageInterval;

	int pl_mike_debug;
	int pl_mike_maxBurnPoints;

	//--- Procedural impluse
	int		pl_impulseEnabled;
	float pl_impulseDuration;
	int   pl_impulseLayer;
	float pl_impulseFullRecoilFactor;
	float pl_impulseMaxPitch;
	float pl_impulseMaxTwist;
	float pl_impulseCounterFactor;
#ifndef _RELEASE
	int		pl_impulseDebug;
#endif //_RELEASE

	int   g_assertWhenVisTableNotUpdatedForNumFrames;

	float gl_time;
	float gl_waitForBalancedGameTime;

	int		hud_ContextualHealthIndicator;            // TODO : Move to CHUDVars
	float hud_objectiveIcons_flashTime;             // TODO : Move to CHUDVars
	int   hud_faderDebug;                           // TODO : Move to CHUDVars
	int		hud_ctrlZoomMode;                         // TODO : Move to CHUDVars
	int		hud_aspectCorrection;                     // TODO : Move to CHUDVars
	float hud_canvas_width_adjustment;

	int		hud_colorLine;                            // Menus only c-var
	int		hud_colorOver;                            // Menus only c-var
	int		hud_colorText;                            // Menus only c-var
	int		hud_subtitles;                            // Menus only c-var
	int		hud_startPaused;                          // Menus only c-var
	int	  hud_allowMouseInput;											// Menus only c-var

	int		hud_psychoPsycho;

	int menu3D_enabled;
#ifndef _RELEASE
	int menu_forceShowPii;
#endif

	int g_flashrenderingduringloading;	
	int g_levelfadein_levelload;	
	int g_levelfadein_quickload;	

	float aim_assistMinDistance;
	float aim_assistMaxDistance;
	float aim_assistMaxDistanceTagged;
	float aim_assistFalloffDistance;
	float aim_assistInputForFullFollow_Ironsight;
	float aim_assistMinTurnScale;
	float aim_assistSlowFalloffStartDistance;
	float aim_assistSlowDisableDistance;
	float aim_assistSlowThresholdOuter;
	float aim_assistSlowDistanceModifier;
	float aim_assistSlowStartFadeinDistance;
	float aim_assistSlowStopFadeinDistance;
	float aim_assistStrength;
	float aim_assistSnapRadiusScale;
	float aim_assistSnapRadiusTaggedScale;

	float aim_assistStrength_IronSight;
	float aim_assistMaxDistance_IronSight;
	float aim_assistMinTurnScale_IronSight;

	float aim_assistStrength_SniperScope;
	float aim_assistMaxDistance_SniperScope;
	float aim_assistMinTurnScale_SniperScope;

	ICVar*i_debuggun_1;
	ICVar*i_debuggun_2;

	float	slide_spread;
	int		i_debug_projectiles;
	int		i_debug_weaponActions;
	int		i_debug_spread;
	int		i_debug_recoil;
	int		i_auto_turret_target;
	int		i_auto_turret_target_tacshells;
	int		i_debug_zoom_mods;
	int   i_debug_turrets;
	int   i_debug_sounds;
	int		i_debug_mp_flowgraph;
	int		i_flashlight_has_shadows;
	int		i_flashlight_has_fog_volume;

	int		i_debug_itemparams_memusage;
	int		i_debug_weaponparams_memusage;

	float i_failedDetonation_speedMultiplier;
	float i_failedDetonation_lifetime;

	float i_hmg_detachWeaponAnimFraction;
	float i_hmg_impulseLocalDirection_x;
	float i_hmg_impulseLocalDirection_y;
	float i_hmg_impulseLocalDirection_z;

	int     g_displayIgnoreList;
	int     g_buddyMessagesIngame;

	int			g_enableFriendlyFallAndPlay;
	int			g_enableFriendlyAIHits;
	int			g_enableFriendlyPlayerHits;

	int			g_mpAllSeeingRadar;
	int			g_mpAllSeeingRadarSv;
	int			g_mpDisableRadar;
	int			g_mpNoEnemiesOnRadar;
	int			g_mpHatsBootsOnRadar;

#if !defined(_RELEASE)
	int			g_spectate_Debug;
	int			g_spectate_follow_orbitEnable;
#endif //!defined(_RELEASE)
	int			g_spectate_TeamOnly;
	int			g_spectate_DisableManual;
	int			g_spectate_DisableDead;
	int			g_spectate_DisableFree;
	int			g_spectate_DisableFollow;
	float   g_spectate_skipInvalidTargetAfterTime; 
	float		g_spectate_follow_orbitYawSpeedDegrees;
	int			g_spectate_follow_orbitAlsoRotateWithTarget;
	float		g_spectate_follow_orbitMouseSpeedMultiplier;
	float		g_spectate_follow_orbitMinPitchRadians;
	float		g_spectate_follow_orbitMaxPitchRadians;

	int			g_deathCam;

	int			g_spectatorOnly;
	float		g_spectatorOnlySwitchCooldown;

	int			g_forceIntroSequence;
	int			g_IntroSequencesEnabled;

	SDeathCamSPParams	g_deathCamSP;

#ifndef _RELEASE
	float		g_tpdeathcam_dbg_gfxTimeout;
	int			g_tpdeathcam_dbg_alwaysOn;
#endif
	float		g_tpdeathcam_timeOutKilled;
	float		g_tpdeathcam_timeOutSuicide;
	float		g_tpdeathcam_lookDistWhenNoKiller;
	float		g_tpdeathcam_camDistFromPlayerStart;
	float		g_tpdeathcam_camDistFromPlayerEnd;
	float		g_tpdeathcam_camDistFromPlayerMin;
	float		g_tpdeathcam_camHeightTweak;
	float		g_tpdeathcam_camCollisionRadius;
	float		g_tpdeathcam_maxBumpCamUpOnCollide;
	float		g_tpdeathcam_zVerticalLimit;
	float		g_tpdeathcam_testLenIncreaseRestriction;
	float		g_tpdeathcam_collisionEpsilon;
	float		g_tpdeathcam_directionalFocusGroundTestLen;
	float		g_tpdeathcam_camSmoothSpeed;
	float		g_tpdeathcam_maxTurn;

	int			g_killercam_disable;
	float		g_killercam_displayDuration;
	float		g_killercam_dofBlurAmount;
	float		g_killercam_dofFocusRange;
	int			g_killercam_canSkip;

	float		g_postkill_minTimeForDeathCamAndKillerCam;
	float		g_postkill_splitScaleDeathCam;

	int			g_useHitSoundFeedback;
	int			g_useSkillKillSoundEffects;
	int			g_hasWindowFocus;

	int			g_displayPlayerDamageTaken;
	int			g_displayDbgText_hud;
	int			g_displayDbgText_silhouettes;
	int			g_displayDbgText_plugins;
	int			g_displayDbgText_pmv;
	int			g_displayDbgText_actorState;

	float		vehicle_steering_curve_scale;
	float		vehicle_steering_curve;
	float		vehicle_acceleration_curve_scale;
	float		vehicle_acceleration_curve;
	float		vehicle_deceleration_curve_scale;
	float		vehicle_deceleration_curve;

	int			g_spawn_vistable_numLineTestsPerFrame;
	int			g_spawn_vistable_numAreaTestsPerFrame;
	int			g_showShadowChar;
	int			g_infiniteAmmo;			// Used by MP private matches, is NOT a cheat variable

	float		g_persistantStats_gamesCompletedFractionNeeded;

	int		  g_animatorDebug;
	int			g_hideArms;
	int			g_debugSmokeGrenades;
	float		g_smokeGrenadeRadius;
	float		g_empOverTimeGrenadeLife;

	int			g_kickCarDetachesEntities;
	float		g_kickCarDetachStartTime;
	float		g_kickCarDetachEndTime;
	
#if !defined(_RELEASE)
	int			g_DisableScoring;
	int			g_DisableCollisionDamage;
	int			g_MaxSimpleCollisions;
	int         g_LogDamage;
	int         g_ProjectilePathDebugGfx;
#endif
	
#if (USE_DEDICATED_INPUT)
	int			g_playerUsesDedicatedInput;
#endif

	int watch_enabled;
	float watch_text_render_start_pos_x;
	float watch_text_render_start_pos_y;
	float watch_text_render_size;
	float watch_text_render_lineSpacing;
	float watch_text_render_fxscale;

	int autotest_enabled;
	ICVar* autotest_state_setup;
	int autotest_quit_when_done;
	int autotest_verbose;
	int designer_warning_enabled;
	int designer_warning_level_resources;

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	ICVar* net_onlyListGameServersContainingText;
	ICVar* net_nat_type;
	int    net_initLobbyServiceToLan;
#endif

	int g_teamDifferentiation;

	SPostEffect g_postEffect;
	int g_gameFXSystemDebug;
	int g_gameFXLightningProfile;
	int g_DebugDrawPhysicsAccess;

	int ai_DebugVisualScriptErrors;
	int ai_EnablePressureSystem;
	int ai_DebugPressureSystem;
	int ai_DebugAggressionSystem;
	int ai_DebugBattleFront;
	int ai_DebugSearch;
	int ai_DebugDeferredDeath;
	
	float ai_CloakingDelay;
	float ai_CompleteCloakDelay;
	float ai_UnCloakingDelay;

	int ai_HazardsDebug;

	int ai_SquadManager_DebugDraw;
	float ai_SquadManager_MaxDistanceFromSquadCenter;
	float ai_SquadManager_UpdateTick;

	float ai_ProximityToHostileAlertnessIncrementThresholdDistance;

	int g_actorViewDistRatio;
	int g_playerLodRatio;
	float g_itemsLodRatioScale;
	float g_itemsViewDistanceRatioScale;

	// Hit Death Reactions CVars
	int			g_hitDeathReactions_enable;
	int			g_hitDeathReactions_useLuaDefaultFunctions;
	int			g_hitDeathReactions_disable_ai;
	int			g_hitDeathReactions_debug;
	int			g_hitDeathReactions_disableRagdoll;
	int			g_hitDeathReactions_usePrecaching;

	enum EHitDeathReactionsLogReactionAnimsType
	{
		eHDRLRAT_DontLog = 0,
		eHDRLRAT_LogAnimNames,
		eHDRLRAT_LogFilePaths,
	};
	int			g_hitDeathReactions_logReactionAnimsOnLoading;

	enum EHitDeathReactionsStreamingPolicy
	{
		eHDRSP_Disabled = 0,
		eHDRSP_ActorsAliveAndNotInPool,		// the assets are locked if at least one of the actors using the profile is alive and not in the pool
		eHDRSP_EntityLifespanBased,				// the assets are requested/released whenever the entities using them are spawned/removed
	};
	int			g_hitDeathReactions_streaming;
	// ~Hit Death Reactions CVars

	SSpectacularKillCVars g_spectacularKill;

	int			g_movementTransitions_enable;
	int			g_movementTransitions_log;
	int			g_movementTransitions_debug;

	float		g_maximumDamage;
	float		g_instantKillDamageThreshold;

	int g_flyCamLoop;
#if (USE_DEDICATED_INPUT)
	int g_dummyPlayersFire;
	int g_dummyPlayersMove;
	int g_dummyPlayersChangeWeapon;
	float g_dummyPlayersJump;
	int g_dummyPlayersRespawnAtDeathPosition;
	int g_dummyPlayersCommitSuicide;
	int g_dummyPlayersShowDebugText;
	float g_dummyPlayersMinInTime;
	float g_dummyPlayersMaxInTime;
	float g_dummyPlayersMinOutTime;
	float g_dummyPlayersMaxOutTime;
	ICVar* g_dummyPlayersGameRules;
	int g_dummyPlayersRanked;
#endif // #if (USE_DEDICATED_INPUT)

	int			g_muzzleFlashCull;
	float		g_muzzleFlashCullDistance;
	int			g_rejectEffectVisibilityCull;
	float		g_rejectEffectCullDistance;
	int 		g_mpCullShootProbablyHits;

	float		g_cloakRefractionScale;
	float		g_cloakBlendSpeedScale;

#if TALOS
	ICVar *pl_talos;
#endif

	int g_telemetry_onlyInGame;
	int g_telemetry_drawcall_budget;
	int g_telemetry_memory_display;
	int g_telemetry_memory_size_sp;
	int g_telemetry_memory_size_mp;
	int g_telemetry_gameplay_enabled;
	int g_telemetry_gameplay_save_to_disk;
	int g_telemetry_gameplay_gzip;
	int g_telemetry_gameplay_copy_to_global_heap;
	int g_telemetryEnabledSP;
	float g_telemetrySampleRatePerformance;
	float g_telemetrySampleRateBandwidth;
	float g_telemetrySampleRateMemory;
	float g_telemetrySampleRateSound;
	float g_telemetry_xp_event_send_interval;
	float g_telemetry_mp_upload_delay;
	const char* g_telemetryTags;
	const char* g_telemetryConfig;
	int g_telemetry_serialize_method;
	int g_telemetryDisplaySessionId;
	const char* g_telemetryEntityClassesToExport;

	int  g_modevarivar_proHud;
	int  g_modevarivar_disableKillCam;
	int  g_modevarivar_disableSpectatorCam;
	
#if !defined(_RELEASE)
	#define g_dataCentreConfig		g_dataCentreConfigCVar->GetString()
	ICVar	*g_dataCentreConfigCVar;
	#define g_downloadMgrDataCentreConfig		g_downloadMgrDataCentreConfigCVar->GetString()
	ICVar *g_downloadMgrDataCentreConfigCVar;
#else
	#define g_dataCentreConfig		g_dataCentreConfigStr
	const char *g_dataCentreConfigStr;
	#define g_downloadMgrDataCentreConfig g_downloadMgrDataCentreConfigStr
	const char *g_downloadMgrDataCentreConfigStr;
#endif

	int g_ignoreDLCRequirements;
	float sv_netLimboTimeout;
	float g_idleKickTime;

	float g_stereoIronsightWeaponDistance;
  int   g_stereoFrameworkEnable;

	int	g_useOnlineServiceForDedicated;

#if USE_LAGOMETER
	int net_graph;
#endif
	int g_enablePoolCache;
	int g_setActorModelFromLua;
	int g_loadPlayerModelOnLoad;
	int g_enableActorLuaCache;

	int g_enableSlimCheckpoints;

	float g_mpLoaderScreenMaxTime;
	float g_mpLoaderScreenMaxTimeSoftLimit;

	int g_mpKickableCars;

	float g_forceItemRespawnTimer;
	float g_defaultItemRespawnTimer;

	float g_updateRichPresenceInterval;

#if defined(DEDICATED_SERVER)
	int g_quitOnNewDataFound;
	int g_quitNumRoundsWarning;
	int g_allowedDataPatchFailCount;
	float g_shutdownMessageRepeatTime;
	ICVar *g_shutdownMessage;
#endif

	int g_useNetSyncToSpeedUpRMIs;

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
  int g_testStatus;
#endif

#if ENABLE_RMI_BENCHMARK

	int g_RMIBenchmarkInterval;
	int g_RMIBenchmarkTwoTrips;

#endif

	ICVar *g_presaleURL;
	ICVar *g_messageOfTheDay;
	ICVar *g_serverImageUrl;

#ifndef _RELEASE
	int    g_disableSwitchVariantOnSettingChanges; 
	int    g_startOnQuorum;
#endif //#ifndef _RELEASE

	SAIPerceptionCVars ai_perception;
	SAIThreatModifierCVars ai_threatModifiers;

	CGameLobbyCVars *m_pGameLobbyCVars;

	// 3d hud
	float g_hud3d_cameraDistance;
	float g_hud3d_cameraOffsetZ;
	int g_hud3D_cameraOverride;

	SCVars()
	{
		memset(this,0,sizeof(SCVars));
	}

	~SCVars() { ReleaseCVars(); }

	void InitCVars(IConsole *pConsole);
	void ReleaseCVars();
	
	void InitAIPerceptionCVars(IConsole *pConsole);
	void ReleaseAIPerceptionCVars(IConsole* pConsole);
};

#endif //__GAMECVARS_H__
