// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AISignalCRCs.h"
#include <CryCore/CryCrc32.h>

//		AI actions order.
static const char* const AIORD_ATTACK = "ORD_ATTACK";
static const char* const AIORD_SEARCH = "ORD_SEARCH";
static const char* const AIORD_REPORTDONE = "ORD_DONE";
static const char* const AIORD_REPORTFAIL = "ORD_FAIL";

AISIGNALS_CRC::AISIGNALS_CRC()
{
}

void AISIGNALS_CRC::Init()
{
	m_nOnRequestNewObstacle = CCrc32::Compute("OnRequestNewObstacle");
	m_nOnRequestUpdate = CCrc32::Compute("OnRequestUpdate");
	m_nOnRequestUpdateAlternative = CCrc32::Compute("OnRequestUpdateAlternative");
	m_nOnRequestUpdateTowards = CCrc32::Compute("OnRequestUpdateTowards");
	m_nOnClearSpotList = CCrc32::Compute("OnClearSpotList");
	m_nOnRequestShootSpot = CCrc32::Compute("OnRequestShootSpot");
	m_nOnRequestHideSpot = CCrc32::Compute("OnRequestHideSpot");
	m_nOnStartTimer = CCrc32::Compute("OnStartTimer");
	m_nOnLastKnownPositionApproached = CCrc32::Compute("OnLastKnownPositionApproached");
	m_nOnEndApproach = CCrc32::Compute("OnEndApproach");
	m_nOnAbortAction = CCrc32::Compute("OnAbortAction");
	m_nOnActionCompleted = CCrc32::Compute("OnActionCompleted");
	m_nOnNoPathFound = CCrc32::Compute("OnNoPathFound");
	m_nOnApproachEnd = CCrc32::Compute("OnApproachEnd");
	m_nOnEndFollow = CCrc32::Compute("OnEndFollow");
	m_nOnBulletRain = CCrc32::Compute("OnBulletRain");
	m_nOnBulletHit = CCrc32::Compute("OnBulletHit");
	m_nOnDriverEntered = CCrc32::Compute("OnDriverEntered");
	m_nOnCheckDeadTarget = CCrc32::Compute("OnCheckDeadTarget");
	m_nOnCheckDeadBody = CCrc32::Compute("OnCheckDeadBody");

	m_nAIORD_ATTACK = CCrc32::Compute(AIORD_ATTACK);
	m_nAIORD_SEARCH = CCrc32::Compute(AIORD_SEARCH);
	m_nAIORD_REPORTDONE = CCrc32::Compute(AIORD_REPORTDONE);
	m_nAIORD_REPORTFAIL = CCrc32::Compute(AIORD_REPORTFAIL);

	m_nOnChangeStance = CCrc32::Compute("OnChangeStance");
	m_nOnScaleFormation = CCrc32::Compute("OnScaleFormation");
	m_nOnUnitDied = CCrc32::Compute("OnUnitDied");
	m_nOnUnitDamaged = CCrc32::Compute("OnUnitDamaged");
	m_nOnUnitBusy = CCrc32::Compute("OnUnitBusy");
	m_nOnUnitSuspended = CCrc32::Compute("OnUnitSuspended");
	m_nOnUnitResumed = CCrc32::Compute("OnUnitResumed");
	m_nOnSetUnitProperties = CCrc32::Compute("OnSetUnitProperties");
	m_nOnJoinTeam = CCrc32::Compute("OnJoinTeam");
	m_nRPT_LeaderDead = CCrc32::Compute("RPT_LeaderDead");

	m_nOnSpotSeeingTarget = CCrc32::Compute("OnSpotSeeingTarget");
	m_nOnSpotLosingTarget = CCrc32::Compute("OnSpotLosingTarget");

	m_nOnFormationPointReached = CCrc32::Compute("OnFormationPointReached");
	m_nOnKeepEnabled = CCrc32::Compute("OnKeepEnabled");
	m_nOnActionDone = CCrc32::Compute("OnActionDone");
	m_nOnGroupAdvance = CCrc32::Compute("OnGroupAdvance");
	m_nOnGroupSeek = CCrc32::Compute("OnGroupSeek");
	m_nOnLeaderReadabilitySeek = CCrc32::Compute("OnLeaderReadabilitySeek");
	m_nAddDangerPoint = CCrc32::Compute("AddDangerPoint");
	m_nOnGroupCover = CCrc32::Compute("OnGroupCover");
	m_nOnLeaderReadabilityAlarm = CCrc32::Compute("OnLeaderReadabilityAlarm");
	m_nOnAdvanceTargetCompromised = CCrc32::Compute("OnAdvanceTargetCompromised");
	m_nOnGroupCohesionTest = CCrc32::Compute("OnGroupCohesionTest");
	m_nOnGroupTestReadabilityCohesion = CCrc32::Compute("OnGroupTestReadabilityCohesion");
	m_nOnGroupTestReadabilityAdvance = CCrc32::Compute("OnGroupTestReadabilityAdvance");
	m_nOnGroupAdvanceTest = CCrc32::Compute("OnGroupAdvanceTest");
	m_nOnLeaderReadabilityAdvanceLeft = CCrc32::Compute("OnLeaderReadabilityAdvanceLeft");
	m_nOnLeaderReadabilityAdvanceRight = CCrc32::Compute("OnLeaderReadabilityAdvanceRight");
	m_nOnLeaderReadabilityAdvanceForward = CCrc32::Compute("OnLeaderReadabilityAdvanceForward");
	m_nOnGroupTurnAmbient = CCrc32::Compute("OnGroupTurnAmbient");
	m_nOnGroupTurnAttack = CCrc32::Compute("OnGroupTurnAttack");
	m_nOnShapeEnabled = CCrc32::Compute("OnShapeEnabled");
	m_nOnShapeDisabled = CCrc32::Compute("OnShapeDisabled");
	m_nOnTargetDead = CCrc32::Compute("OnTargetDead");
	m_nOnEndPathOffset = CCrc32::Compute("OnEndPathOffset");
	m_nOnPathFindAtStart = CCrc32::Compute("OnPathFindAtStart");
	m_nOnPathFound = CCrc32::Compute("OnPathFound");
	m_nOnBackOffFailed = CCrc32::Compute("OnBackOffFailed");
	m_nOnBadHideSpot = CCrc32::Compute("OnBadHideSpot");
	m_nOnCoverReached = CCrc32::Compute("OnCoverReached");
	m_nOnMovingToCover = CCrc32::Compute("OnMovingToCover");
	m_nOnMovingInCover = CCrc32::Compute("OnMovingInCover");
	m_nOnEnterCover = CCrc32::Compute("OnEnterCover");
	m_nOnLeaveCover = CCrc32::Compute("OnLeaveCover");
	m_nOnCoverCompromised = CCrc32::Compute("OnCoverCompromised");
	m_nOnHighCover = CCrc32::Compute("OnHighCover");
	m_nOnLowCover = CCrc32::Compute("OnLowCover");
	m_nOnNoAimPosture = CCrc32::Compute("OnNoAimPosture");
	m_nOnNoPeekPosture = CCrc32::Compute("OnNoPeekPosture");
	m_nOnTPSDestinationNotFound = CCrc32::Compute("OnTPSDestNotFound");
	m_nOnTPSDestinationFound = CCrc32::Compute("OnTPSDestFound");
	m_nOnTPSDestinationReached = CCrc32::Compute("OnTPSDestReached");
	m_nOnRightLean = CCrc32::Compute("OnRightLean");
	m_nOnLeftLean = CCrc32::Compute("OnLeftLean");
	m_nOnLowHideSpot = CCrc32::Compute("OnLowHideSpot");
	m_nOnChargeStart = CCrc32::Compute("OnChargeStart");
	m_nOnChargeHit = CCrc32::Compute("OnChargeHit");
	m_nOnChargeMiss = CCrc32::Compute("OnChargeMiss");
	m_nOnChargeBailOut = CCrc32::Compute("OnChargeBailOut");
	m_nORDER_IDLE = CCrc32::Compute("ORDER_IDLE");
	m_nOnNoGroupTarget = CCrc32::Compute("OnNoGroupTarget");
	m_nOnLeaderTooFar = CCrc32::Compute("OnLeaderTooFar");
	m_nOnEnemySteady = CCrc32::Compute("OnEnemySteady");
	m_nOnShootSpotFound = CCrc32::Compute("OnShootSpotFound");
	m_nOnShootSpotNotFound = CCrc32::Compute("OnShootSpotNotFound");
	m_nOnLeaderStop = CCrc32::Compute("OnLeaderStop");
	m_nOnUnitMoving = CCrc32::Compute("OnUnitMoving");
	m_nOnUnitStop = CCrc32::Compute("OnUnitStop");
	m_nORDER_EXIT_VEHICLE = CCrc32::Compute("ORDER_EXIT_VEHICLE");
	m_nOnNoFormationPoint = CCrc32::Compute("OnNoFormationPoint");
	m_nOnTargetApproaching = CCrc32::Compute("OnTargetApproaching");
	m_nOnTargetFleeing = CCrc32::Compute("OnTargetFleeing");
	m_nOnNoTargetVisible = CCrc32::Compute("OnNoTargetVisible");
	m_nOnNoTargetAwareness = CCrc32::Compute("OnNoTargetAwareness");
	m_nOnNoHidingPlace = CCrc32::Compute("OnNoHidingPlace");
	m_nOnLowAmmo = CCrc32::Compute("OnLowAmmo");
	m_nOnOutOfAmmo = CCrc32::Compute("OnOutOfAmmo");
	m_nOnReloaded = CCrc32::Compute("OnReloaded");
	m_nOnMeleeExecuted = CCrc32::Compute("OnMeleeExecuted");
	m_nOnPlayerLooking = CCrc32::Compute("OnPlayerLooking");
	m_nOnPlayerSticking = CCrc32::Compute("OnPlayerSticking");
	m_nOnPlayerLookingAway = CCrc32::Compute("OnPlayerLookingAway");
	m_nOnPlayerGoingAway = CCrc32::Compute("OnPlayerGoingAway");
	m_nOnTargetTooClose = CCrc32::Compute("OnTargetTooClose");
	m_nOnTargetTooFar = CCrc32::Compute("OnTargetTooFar");
	m_nOnFriendInWay = CCrc32::Compute("OnFriendInWay");
	m_nOnUseSmartObject = CCrc32::Compute("OnUseSmartObject");
	m_nOnAvoidDanger = CCrc32::Compute("OnAvoidDanger");
	m_nOnSetPredictionTime = CCrc32::Compute("m_nOnSetPredictionTime");
	m_nOnInterestSystemEvent = CCrc32::Compute("OnInterestSystemEvent");
	m_nOnSpecialAction = CCrc32::Compute("OnSpecialAction");
	m_nOnNewAttentionTarget = CCrc32::Compute("OnNewAttentionTarget");
	m_nOnAttentionTargetThreatChanged = CCrc32::Compute("OnAttentionTargetThreatChanged");
};
