// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Puppet.cpp
   $Id$
   Description: Implementation of the CPuppet class.

   -------------------------------------------------------------------------
   History:
   -

 *********************************************************************/

#include "StdAfx.h"
#include "Puppet.h"
#include "AIPlayer.h"
#include "AIVehicle.h"
#include "Leader.h"
#include "GoalOp.h"

#include "AICollision.h"
#include "HideSpot.h"
#include "SmartObjects.h"
#include "PathFollower.h"
#include "FireCommand.h"
#include "ObjectContainer.h"

#include "CentralInterestManager.h"
#include "TargetSelection/TargetTrackManager.h"

#include <CryMath/PNoise3.h>
#include <CrySystem/ISystem.h>
#include "DebugDrawContext.h"
#include "Cover/CoverSystem.h"
#include "Group/GroupManager.h"
#include <CryAISystem/VisionMapTypes.h>

#include <CryAISystem/IPerceptionHandlerModifier.h>

#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Formation/FormationManager.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

inline float SmoothStep(float a, float b, float x)
{
	x = (x - a) / (b - a);
	x = clamp_tpl(x, 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

inline float IndexToMovementUrgency(int idx)
{
	float sign = idx < 0 ? -1.0f : 1.0f;
	if (idx < 0) idx = -idx;
	switch (idx)
	{
	case 0:
		return AISPEED_ZERO * sign;
	case 1:
		return AISPEED_SLOW * sign;
	case 2:
		return AISPEED_WALK * sign;
	case 3:
		return AISPEED_RUN * sign;
	default:
		return AISPEED_SPRINT * sign;
	}
}

template<class Value> void SerializeWeakRefMap(TSerialize& ser, const char* sName, std::map<CWeakRef<CAIObject>, Value>& map)
{
	ser.BeginGroup(sName);
	int size = map.size();
	ser.Value("mapSize", size);

	// If reading, iterate over serialised data and add to container
	if (ser.IsReading())
	{
		map.clear();
		for (int i = 0; i < size; i++)
		{
			CWeakRef<CAIObject> k;
			Value v;
			ser.BeginGroup("mapPair");
			k.Serialize(ser, "key");
			ser.Value("value", v);
			ser.EndGroup();
			map.insert(std::make_pair(k, v));
		}
	}
	// If writing, iterate over container and serialise data
	else if (ser.IsWriting())
	{
		for (typename std::map<CWeakRef<CAIObject>, Value>::iterator it = map.begin(); it != map.end(); ++it)
		{
			CWeakRef<CAIObject> k = it->first;
			ser.BeginGroup("mapPair");
			k.Serialize(ser, "key");
			ser.Value("value", it->second);
			ser.EndGroup();
		}
	}

	ser.EndGroup();
}

SSoundPerceptionDescriptor CPuppet::s_DefaultSoundPerceptionDescriptor[AISOUND_LAST] =
{
	SSoundPerceptionDescriptor(0.0f, 1.0f, 2.0f, PERCEPTION_INTERESTING_THR, 1.0f, 0.25f),
	SSoundPerceptionDescriptor(0.0f, 1.0f, 4.0f, PERCEPTION_INTERESTING_THR, 1.0f, 0.5f),
	SSoundPerceptionDescriptor(0.0f, 1.0f, 4.0f, PERCEPTION_THREATENING_THR, 1.0f, 0.5f),
	SSoundPerceptionDescriptor(0.0f, 1.0f, 2.0f, PERCEPTION_INTERESTING_THR, 1.0f, 0.5f),
	SSoundPerceptionDescriptor(0.0f, 1.0f, 4.0f, PERCEPTION_THREATENING_THR, 1.0f, 0.5f),
	SSoundPerceptionDescriptor(0.0f, 1.0f, 4.0f, PERCEPTION_AGGRESSIVE_THR,  1.0f, 0.25f),
	SSoundPerceptionDescriptor(0.0f, 1.0f, 6.0f, PERCEPTION_AGGRESSIVE_THR,  1.0f, 0.75f)
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPuppet::CPuppet()
	: m_fLastUpdateTime(0.0f)
	, m_bWarningTargetDistance(false)
	, m_Alertness(-1)
	, m_pPerceptionHandler(NULL)
	, m_pRODHandler(NULL)
	, m_pFireCmdHandler(0)
	, m_pFireCmdGrenade(0)
	, m_currentWeaponId(0)
	, m_bGrenadeThrowRequested(false)
	, m_eGrenadeThrowRequestType(eRGT_INVALID)
	, m_iGrenadeThrowTargetType(0)
	, m_allowedStrafeDistanceStart(0)
	, m_allowedStrafeDistanceEnd(0)
	, m_allowStrafeLookWhileMoving(false)
	, m_strafeStartDistance(0.0f)
	, m_adaptiveUrgencyMin(0)
	, m_adaptiveUrgencyMax(0)
	, m_adaptiveUrgencyScaleDownPathLen(0)
	, m_adaptiveUrgencyMaxPathLen(0)
	, m_fLastTimeAwareOfPlayer(0)
	, m_playerAwarenessType(PA_NONE)
	, m_timeSinceTriggerPressed(0)
	, m_friendOnWayElapsedTime(0)
	, m_bCanBeShot(true)
	, m_eMemoryFireType(eMFT_UseCoverFireTime)
	, m_allowedToHitTarget(false)
	, m_allowedToUseExpensiveAccessory(false)
	, m_firingReactionTimePassed(false)
	, m_firingReactionTime(0.0f)
	, m_targetApproach(0.0f)
	, m_targetFlee(0.0f)
	, m_targetApproaching(false)
	, m_targetFleeing(false)
	, m_lastTargetValid(false)
	, m_lastTargetPos(ZERO)
	, m_lastTargetSpeed(0)
	, m_chaseSpeed(0.0f)
	, m_chaseSpeedRate(0.0f)
	, m_lastChaseUrgencyDist(-1)
	, m_lastChaseUrgencySpeed(-1)
	, m_vehicleAvoidingTime(0.0f)
	, m_targetLastMissPoint(ZERO)
	, m_targetFocus(0.0f)
	, m_targetZone(AIZONE_OUT)
	, m_targetPosOnSilhouettePlane(ZERO)
	, m_targetDistanceToSilhouette(FLT_MAX)
	, m_targetBiasDirection(0, 0, -1)
	, m_targetEscapeLastMiss(0.0f)
	, m_targetDamageHealthThr(0.0f)
	, m_targetDamageHealthThrHistory(0)
	, m_targetSeenTime(0)
	, m_targetLostTime(0)
	, m_burstEffectTime(0.0f)
	, m_burstEffectState(0)
	, m_targetDazzlingTime(0.0f)
	, m_bCoverFireEnabled(false)
	, m_lastSteerTime(0.0f)
	, m_lastTimeUpdatedBestTarget(0.0f)
	, m_delayedStance(STANCE_NULL)
	, m_delayedStanceMovementCounter(0)
	, m_outOfAmmoTimeOut(0.0f)
	, m_lastAimObstructionResult(true)
	, m_updatePriority(AIPUP_LOW)
	, m_vForcedNavigation(ZERO)
	, m_fForcedNavigationSpeed(0.0f)
	, m_vehicleStickTarget(0)
	, m_lastMissShotsCount(~0l)
	, m_lastHitShotsCount(~0l)
	, m_lastTargetPart(~0l)
	, m_steeringOccupancyBias(0)
	, m_steeringAdjustTime(0)
	, m_steeringEnabled(false)
	, m_alarmedTime(0.0f)
	, m_alarmedLevel(0.0f)
	, m_fireDisabled(0)
	, m_closeRangeStrafing(false)
	, m_damagePartsUpdated(false)
{
	CCCPOINT(CPuppet_CPuppet);

	_fastcast_CPuppet = true;

	// todo: make it safe, not just casting
	m_pFireCmdGrenade = static_cast<CFireCommandGrenade*>(GetAISystem()->CreateFirecommandHandler("grenade", this));
	if (m_pFireCmdGrenade)
		m_pFireCmdGrenade->Reset();

	m_postureManager.ResetPostures();
	m_postureManager.AddDefaultPostures(PostureManager::AimPosture);
	m_postureManager.AddDefaultPostures(PostureManager::HidePosture);

	AILogComment("CPuppet::CPuppet (%p)", this);
}

CPuppet::~CPuppet()
{
	// goal ops need to be reset at the top level
	// otherwise their reset methods might operate on already destructed memory
	ResetCurrentPipe(true);

	CCCPOINT(CPuppet_Destructor);

	AILogComment("CPuppet::~CPuppet %s (%p)", GetName(), this);

	SAFE_DELETE(m_pPerceptionHandler);

	if (m_pFireCmdGrenade) m_pFireCmdGrenade->Release();

	if (m_pFireCmdHandler) m_pFireCmdHandler->Release();

	delete m_targetDamageHealthThrHistory;
}

void CPuppet::ClearStaticData()
{
	stl::free_container(s_weights);
}

//===================================================================
// SetRODHandler
//===================================================================
void CPuppet::SetRODHandler(IAIRateOfDeathHandler* pHandler)
{
	CRY_ASSERT(pHandler);

	m_pRODHandler = pHandler;
}

//===================================================================
// ClearRODHandler
//===================================================================
void CPuppet::ClearRODHandler()
{
	m_pRODHandler = NULL;
}

//===================================================================
// GetPerceivedTargetPos
//===================================================================
bool CPuppet::GetPerceivedTargetPos(IAIObject* pTarget, Vec3& vPos) const
{
	CRY_ASSERT(pTarget);

	bool bResult = false;
	vPos.zero();

	if (pTarget)
	{
		CAIObject* pTargetObj = (CAIObject*)pTarget;
		CWeakRef<CAIObject> refTargetAssociation = pTargetObj->GetAssociation();
		if (refTargetAssociation.IsValid())
			pTargetObj = refTargetAssociation.GetAIObject();

		// Check potential targets
		PotentialTargetMap targetMap;
		if (m_pPerceptionHandler && m_pPerceptionHandler->GetPotentialTargets(targetMap))
		{
			PotentialTargetMap::const_iterator itTarget = targetMap.find(GetWeakRef(pTargetObj));
			if (itTarget != targetMap.end())
			{
				// Found, use perceived if not visual or aggressive
				if (itTarget->second.type == AITARGET_VISUAL && itTarget->second.threat == AITHREAT_AGGRESSIVE)
				{
					IEntity* pTargetEntity = pTargetObj->GetEntity();
					CRY_ASSERT(pTargetEntity);
					vPos = pTargetEntity->GetWorldPos();
				}
				else
				{
					CWeakRef<CAIObject> refDummyRep = itTarget->second.refDummyRepresentation.GetWeakRef();
					if (refDummyRep.IsValid())
						vPos = refDummyRep.GetAIObject()->GetPos();
				}
				bResult = true;
			}
		}
	}

	return bResult;
}

//===================================================================
// ParseParameters
//===================================================================
void CPuppet::ParseParameters(const AIObjectParams& params, bool bParseMovementParams)
{
	CCCPOINT(CPuppet_ParseParameters);

	CPipeUser::ParseParameters(params, bParseMovementParams);
}

//===================================================================
// QueryCurrentWeaponDescriptor
//===================================================================
const AIWeaponDescriptor& CPuppet::QueryCurrentWeaponDescriptor(bool bIsSecondaryFire, ERequestedGrenadeType prefGrenadeType)
{
	if (IAIActorProxy* pProxy = GetProxy())
	{
		bool bUpdatedDescriptor = false;
		EntityId weaponToListenTo = 0;

		// Use the secondary weapon descriptor if requested and available
		if (!bIsSecondaryFire)
		{
			EntityId weaponId = 0;
			pProxy->GetCurrentWeapon(weaponId);

			weaponToListenTo = weaponId;

			// Bug: The condition below doesn't check if we went from secondary to primary fire (i.e. grenade to scar).
			// I removed the condition to keep the weapon descriptor up to date. Unfortunately ran every frame.
			// Investigate how we could fix this. /Jonas

			const bool weaponChanged = (m_currentWeaponId != weaponId);
			if (weaponChanged || m_fireModeUpdated)
			{
				m_currentWeaponId = weaponId;
				m_CurrentWeaponDescriptor = pProxy->GetCurrentWeaponDescriptor();
				bUpdatedDescriptor = true;
			}
		}
		else
		{
			bUpdatedDescriptor = true;

			pProxy->GetSecWeapon(prefGrenadeType, NULL, &weaponToListenTo);

			if (!pProxy->GetSecWeaponDescriptor(m_CurrentWeaponDescriptor, prefGrenadeType))
			{
				// Use default if no secondary weapon descriptor is available
				m_CurrentWeaponDescriptor = AIWeaponDescriptor();
			}
		}

		if (bUpdatedDescriptor)
		{
			pProxy->EnableWeaponListener(weaponToListenTo, m_CurrentWeaponDescriptor.bSignalOnShoot);

			// Make sure the fire command handler is up to date.
			// If the current fire command handler is already correct, do not recreate it.
			if (m_pFireCmdHandler && stricmp(m_CurrentWeaponDescriptor.firecmdHandler.c_str(), m_pFireCmdHandler->GetName()) == 0)
			{
				m_pFireCmdHandler->Reset();
			}
			else
			{
				// Release the old handler and create new.
				SAFE_RELEASE(m_pFireCmdHandler);
				m_pFireCmdHandler = GetAISystem()->CreateFirecommandHandler(m_CurrentWeaponDescriptor.firecmdHandler, this);
				if (m_pFireCmdHandler)
					m_pFireCmdHandler->Reset();
			}
		}
	}

	return m_CurrentWeaponDescriptor;
}

//===================================================================
// AdjustTargetVisibleRange
//===================================================================
float CPuppet::AdjustTargetVisibleRange(const CAIActor& observer, float fVisibleRange) const
{
	float fRangeScale = 1.0f;

	// Adjust using my light level if the observer is affected by light
	if (IsAffectedByLight())
	{
		const EAILightLevel targetLightLevel = GetLightLevel();
		switch (targetLightLevel)
		{
		//	case AILL_LIGHT: SOMSpeed
		case AILL_MEDIUM:
			fRangeScale *= gAIEnv.CVars.SightRangeMediumIllumMod;
			break;
		case AILL_DARK:
			fRangeScale *= gAIEnv.CVars.SightRangeDarkIllumMod;
			break;
		case AILL_SUPERDARK:
			fRangeScale *= gAIEnv.CVars.SightRangeSuperDarkIllumMod;
			break;
		}
	}

	// Scale down sight range when target is underwater based on distance
	const float fCachedWaterOcclusionValue = GetCachedWaterOcclusionValue();
	if (fCachedWaterOcclusionValue > FLT_EPSILON)
	{
		const Vec3& observerPos = observer.GetPos();
		const float fDistance = Distance::Point_Point(GetPos(), observerPos);
		const float fDistanceFactor = (fVisibleRange > FLT_EPSILON ? GetAISystem()->GetVisPerceptionDistScale(fDistance / fVisibleRange) : 0.0f);

		const float fWaterOcclusionEffect = 2.0f * fCachedWaterOcclusionValue + (1 - fDistanceFactor) * 0.5f;

		fRangeScale *= (fWaterOcclusionEffect > 1.0f ? 0.0f : 1.0f - fWaterOcclusionEffect);
	}

	// Return new range
	return fVisibleRange * fRangeScale;
}

//===================================================================
// Update
//===================================================================
void CPuppet::Update(EUpdateType type)
{
	CCCPOINT(CPuppet_Update);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_bDryUpdate = (type != EUpdateType::Full);

	if (!IsEnabled())
	{
		AIWarning("CPuppet::Update: Trying to update disabled Puppet: %s", GetName());
		AIAssert(0);
		return;
	}

	IAIActorProxy* pAIActorProxy = GetProxy();

	// There should never be Puppets without proxies.
	if (!pAIActorProxy)
	{
		AIWarning("CPuppet::Update: Puppet does not have proxy: %s", GetName());
		AIAssert(0);
		return;
	}
	// There should never be Puppets without physics.
	if (!GetPhysics())
	{
		AIWarning("CPuppet::Update: Puppet does not have physics: %s", GetName());
		AIAssert(0);
		return;
	}
	// dead Puppets should never be updated
	if (pAIActorProxy->IsDead())
	{
		AIWarning("CPuppet::Update: Trying to update dead Puppet: %s ", GetName());
		AIAssert(0);
		return;
	}

	CPipeUser::Update(type);

	UpdateHealthTracking();
	m_damagePartsUpdated = false;

	static bool doDryUpdateCall = true;

	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();

	if (!m_bDryUpdate)
	{
		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		m_fTimePassed = (m_fLastUpdateTime.GetMilliSecondsAsInt64() > 0)
		                ? min(0.5f, (fCurrentTime - m_fLastUpdateTime).GetSeconds())
		                : 0;
		m_fLastUpdateTime = fCurrentTime;

		// Dario: From Crysis 3 on we don't need to clear the movement direction every frame (04.09.2014)
		const bool clearMoveDir = false;

		m_State.Reset(clearMoveDir);

		// affect puppet parameters here----------------------------------------
		if (m_bCanReceiveSignals)
		{
			UpdatePuppetInternalState();

			// Marcio: Update the attention target, since most likely the AIOBJECTSTATE evaluation
			// will only happen this frame, and then we'll miss it
			pAttentionTarget = m_refAttentionTarget.GetAIObject();
		}

		GetStateFromActiveGoals(m_State);

#ifdef CRYAISYSTEM_DEBUG
		// Store current position to debug stream.
		{
			RecorderEventData recorderEventData(GetPos());
			RecordEvent(IAIRecordable::E_AGENTPOS, &recorderEventData);
		}

		// Store current direction to debug stream.
		{
			RecorderEventData recorderEventData(GetViewDir());
			RecordEvent(IAIRecordable::E_AGENTDIR, &recorderEventData);
		}

		// Store current attention target position to debug stream.
		if (pAttentionTarget)
		{
			RecorderEventData recorderEventData(pAttentionTarget->GetPos());
			RecordEvent(IAIRecordable::E_ATTENTIONTARGETPOS, &recorderEventData);
		}
#endif

		// Update last know target position, and last target position constrained into the territory.
		if (pAttentionTarget && (pAttentionTarget->IsAgent() || (pAttentionTarget->GetType() == AIOBJECT_TARGET)))
		{
			m_lastLiveTargetPos = pAttentionTarget->GetPos();
			m_timeSinceLastLiveTarget = 0.0f;
		}
		else
		{
			if (m_timeSinceLastLiveTarget >= 0.0f)
				m_timeSinceLastLiveTarget += m_fTimePassed;
		}

		m_lightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);
	}
	else if (doDryUpdateCall)
	{
		//if approaching then always update
		for (size_t i = 0; i < m_vActiveGoals.size(); i++)
		{
			QGoal& Goal = m_vActiveGoals[i];
			Goal.pGoalOp->ExecuteDry(this);
		}
	}

	if (m_delayedStance != STANCE_NULL)
	{
		bool wantsToMove = !m_State.vMoveDir.IsZero() && m_State.fDesiredSpeed > 0.0f;
		if (wantsToMove)
		{
			m_delayedStanceMovementCounter++;
			if (m_delayedStanceMovementCounter > 3)
			{
				m_State.bodystate = m_delayedStance;
				m_delayedStance = STANCE_NULL;
				m_delayedStanceMovementCounter = 0;
			}
		}
		else
		{
			m_delayedStanceMovementCounter = 0;
		}
	}

	HandleNavSOFailure();
	SyncActorTargetPhaseWithAIProxy();

	//--------------------------------------------------------
	// Update the look at and strafe logic.
	// Do not perform these actions if the AI is paused for things like communications.
	if (!m_paused)
	{
		if (GetSubType() == CAIObject::STP_2D_FLY)
			UpdateLookTarget3D(pAttentionTarget); //this is only for the scout now.
		else
			UpdateLookTarget(pAttentionTarget);
	}

	//--------------------------------------------------------
	// Update the body target
	m_State.vBodyTargetDir = m_vBodyTargetDir;
	m_State.vDesiredBodyDirectionAtTarget = m_vDesiredBodyDirectionAtTarget;
	m_State.movementContext = m_movementContext;

	if (pAttentionTarget)
	{
		m_State.nTargetType = pAttentionTarget->GetType();
		m_State.bTargetEnabled = pAttentionTarget->IsEnabled();
	}
	else
	{
		m_State.nTargetType = -1;
		m_State.bTargetEnabled = false;
		m_State.eTargetThreat = AITHREAT_NONE;
		m_State.eTargetType = AITARGET_NONE;
	}

	const float dt = GetAISystem()->GetFrameDeltaTime();

	m_targetDazzlingTime = max(0.0f, m_targetDazzlingTime - dt);
	if (GetAttentionTargetType() == AITARGET_VISUAL && GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
	{
		m_targetLostTime = 0.0f;
		m_targetSeenTime = min(m_targetSeenTime + dt, 10.0f);
	}
	else
	{
		m_targetLostTime += dt;
		m_targetSeenTime = max(0.0f, m_targetSeenTime - dt);
	}

	FireCommand(gEnv->pTimer->GetFrameTime());

	if (m_Parameters.m_fAwarenessOfPlayer > 0)
		CheckAwarenessPlayer();

	// Time out unreachable hidepoints.
	for (TimeOutVec3List::iterator it = m_recentUnreachableHideObjects.begin(); it != m_recentUnreachableHideObjects.end(); )
	{
		it->first -= GetAISystem()->GetFrameDeltaTime();
		if (it->first < 0.0f)
			it = m_recentUnreachableHideObjects.erase(it);
		else
			++it;
	}

	UpdateCloakScale();

	m_State.vForcedNavigation = m_vForcedNavigation;
	m_State.fForcedNavigationSpeed = m_fForcedNavigationSpeed;
}

void CPuppet::UpdateProxy(EUpdateType type)
{
	IAIActorProxy* pAIActorProxy = GetProxy();
	// There should never be Puppets without proxies.
	if (!pAIActorProxy)
	{
		AIWarning("CPuppet::UpdateProxy: Puppet does not have proxy: %s", GetName());
		AIAssert(0);
		return;
	}

	SetMoveDir(m_State.vMoveDir);

	if (gAIEnv.CVars.UpdateProxy)
	{
		// Force stance etc
		//		int lastStance = m_State.bodystate;
		bool forcedPosture = false;
		if (strcmp(gAIEnv.CVars.ForcePosture, "0"))
		{
			PostureManager::PostureInfo posture;

			if (m_postureManager.GetPostureByName(gAIEnv.CVars.ForcePosture, &posture))
			{
				forcedPosture = true;

				m_State.bodystate = posture.stance;
				m_State.lean = posture.lean;
				m_State.peekOver = posture.peekOver;

				if (!posture.agInput.empty())
					pAIActorProxy->SetAGInput(AIAG_ACTION, posture.agInput.c_str());
				else
					pAIActorProxy->ResetAGInput(AIAG_ACTION);
			}
		}

		if (!forcedPosture)
		{
			if (gAIEnv.CVars.ForceStance > -1)
				m_State.bodystate = gAIEnv.CVars.ForceStance;

			if (strcmp(gAIEnv.CVars.ForceAGAction, "0"))
				pAIActorProxy->SetAGInput(AIAG_ACTION, gAIEnv.CVars.ForceAGAction);
		}

		if (strcmp(gAIEnv.CVars.ForceAGSignal, "0"))
			pAIActorProxy->SetAGInput(AIAG_SIGNAL, gAIEnv.CVars.ForceAGSignal);

		if (gAIEnv.CVars.ForceAllowStrafing > -1)
			m_State.allowStrafing = gAIEnv.CVars.ForceAllowStrafing != 0;

		const char* forceLookAimTarget = gAIEnv.CVars.ForceLookAimTarget;
		if (strcmp(forceLookAimTarget, "none") != 0)
		{
			Vec3 targetPos = GetPos();
			if (strcmp(forceLookAimTarget, "x") == 0)
				targetPos += Vec3(10, 0, 0);
			else if (strcmp(forceLookAimTarget, "y") == 0)
				targetPos += Vec3(0, 10, 0);
			else if (strcmp(forceLookAimTarget, "xz") == 0)
				targetPos += Vec3(10, 0, 10);
			else if (strcmp(forceLookAimTarget, "yz") == 0)
				targetPos += Vec3(0, 10, 10);
			else
			{
				IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(forceLookAimTarget);
				if (pEntity)
					targetPos = pEntity->GetPos();
				else
					targetPos.zero();
			}

			m_State.vLookTargetPos = targetPos;
			m_State.vAimTargetPos = targetPos;
			m_State.aimTargetIsValid = true;
		}

		if (!m_bDryUpdate)
		{
			// If secondary fire went through, clear throw grenade request
			if (m_bGrenadeThrowRequested && (m_State.fireSecondary != eAIFS_Blocking))
				m_bGrenadeThrowRequested = false;
		}

		// Always update the AI proxy, also during dry updates. The Animation system
		// needs correct and constantly updated predictions to correctly set animation
		// parameters.
		// (MATT) Try avoiding UpdateMind, which triggers script, signal and behaviour code, if only a dry update {2009/12/06}
		pAIActorProxy->Update(m_State, !m_bDryUpdate);

		// Restore foced state.
		//m_State.bodystate = lastStance;

		if (IsEnabled()) // the puppet may have been killed above in this loop (say, in GetProxy()->Update(m_State))
			UpdateAlertness();
	}

	// Make sure we haven't played with that value during our update
	assert(m_bDryUpdate == (type == EUpdateType::Dry));

#ifdef CRYAISYSTEM_DEBUG
	if (!m_bDryUpdate)
	{
		// Health
		RecorderEventData recorderEventData(pAIActorProxy->GetActorHealth());
		RecordEvent(IAIRecordable::E_HEALTH, &recorderEventData);
	}
#endif
}

//===================================================================
// UpdateTargetMovementState
//===================================================================
void CPuppet::UpdateTargetMovementState()
{
	const float dt = m_fTimePassed;
	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();

	if (!pAttentionTarget || dt < 0.00001f)
	{
		m_lastTargetValid = false;
		m_targetApproaching = false;
		m_targetFleeing = false;
		m_targetApproach = 0;
		m_targetFlee = 0;
		return;
	}

	bool targetValid;
	switch (pAttentionTarget->GetType())
	{
	case AIOBJECT_TARGET:
	case AIOBJECT_PLAYER:
	case AIOBJECT_ACTOR:
		targetValid = true;
		break;
	default:
		targetValid = false;
	}

	Vec3 targetPos = pAttentionTarget->GetPos();
	Vec3 targetDir = pAttentionTarget->GetMoveDir();
	float targetSpeed = pAttentionTarget->GetVelocity().GetLength();
	const Vec3& puppetPos = GetPos();

	if (!m_lastTargetValid)
	{
		m_lastTargetValid = true;
		m_lastTargetSpeed = targetSpeed;
		m_lastTargetPos = targetPos;
	}
	const float fleeMin = 10.0f;
	const float approachMax = 20.0f;

	if (!targetValid && m_lastTargetValid)
	{
		targetPos = m_lastTargetPos;
		targetSpeed = m_lastTargetSpeed;
	}

	{
		float curDist = Distance::Point_Point(targetPos, puppetPos);
		float lastDist = Distance::Point_Point(m_lastTargetPos, puppetPos);

		Vec3 dirTargetToPuppet = puppetPos - targetPos;
		dirTargetToPuppet.NormalizeSafe();

		float dot = (1.0f + dirTargetToPuppet.Dot(targetDir)) * 0.5f;

		bool movingTowards = curDist < lastDist && targetSpeed > 0; //fabsf(curDist - lastDist) > 0.01f;
		bool movingAway = curDist > lastDist && targetSpeed > 0;    //fabsf(curDist - lastDist) > 0.01f;

		if (curDist < approachMax && movingTowards)
			m_targetApproach += targetSpeed * sqr(dot) * 0.25f;
		else
			m_targetApproach -= dt * 2.0f;

		if (curDist > fleeMin && movingAway)
			m_targetFlee += targetSpeed * sqr(1.0f - dot) * 0.1f;
		else
			m_targetFlee -= dt * 2.0f;

		m_lastTargetPos = targetPos;
	}

	m_targetApproach = clamp_tpl(m_targetApproach, 0.0f, 10.0f);
	m_targetFlee = clamp_tpl(m_targetFlee, 0.0f, 10.0f);

	bool approaching = m_targetApproach > 9.9f;
	bool fleeing = m_targetFlee > 9.9f;

	if (approaching != m_targetApproaching)
	{
		m_targetApproaching = approaching;
		if (m_targetApproaching)
		{
			m_targetApproach = 0;
			SetSignal(1, "OnTargetApproaching", pAttentionTarget->GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnTargetApproaching);
		}
	}

	if (fleeing != m_targetFleeing)
	{
		m_targetFleeing = fleeing;
		if (m_targetFleeing)
		{
			m_targetFlee = 0;
			SetSignal(1, "OnTargetFleeing", pAttentionTarget->GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnTargetFleeing);
		}
	}
}

//===================================================================
// UpdateAlertness
//===================================================================
void CPuppet::UpdateAlertness()
{
	CRY_ASSERT(IsEnabled());

	int nextAlertness = GetProxy()->GetAlertnessState();
	if ((m_Alertness != nextAlertness) && m_Parameters.factionHostility)
	{
		if (m_Alertness >= 0)
		{
			int& counter = GetAISystem()->m_AlertnessCounters[m_Alertness];
			if (counter > 0)
			{
				--counter;
			}
		}

		CRY_ASSERT(NUM_ALERTNESS_COUNTERS > nextAlertness && nextAlertness >= 0);
		if (NUM_ALERTNESS_COUNTERS > nextAlertness && nextAlertness >= 0)
		{
			++GetAISystem()->m_AlertnessCounters[nextAlertness];
		}
	}

	m_Alertness = nextAlertness;
}

//===================================================================
// ResetAlertness
//===================================================================
void CPuppet::ResetAlertness()
{
	// get alertness before doing GetProxy()->Reset()!!!
	if (m_Alertness >= 0 && m_Parameters.factionHostility &&
	    GetAISystem()->m_AlertnessCounters[m_Alertness] > 0)
		--GetAISystem()->m_AlertnessCounters[m_Alertness];
	m_Alertness = -1;
}

//===================================================================
// GetEventOwner
//===================================================================
IAIObject* CPuppet::GetEventOwner(IAIObject* pObject) const
{
	IAIObject* pOwner = 0;

	if (m_pPerceptionHandler)
	{
		CWeakRef<CAIObject> refObject = GetWeakRefSafe((CAIObject*)pObject);
		pOwner = m_pPerceptionHandler->GetEventOwner(refObject);
	}

	return pOwner;
}

//===================================================================
// GetEventOwner
//===================================================================
CAIObject* CPuppet::GetEventOwner(CWeakRef<CAIObject> refOwned) const
{
	if (!refOwned.IsValid()) return 0;
	// TO DO: for memory/sound target etc, probably it's better to set their association equal to the owner
	// instead of searching for it here
	return (m_pPerceptionHandler ? m_pPerceptionHandler->GetEventOwner(refOwned) : NULL);
}

//===================================================================
// UpdateTargetSelection
//===================================================================
bool CPuppet::UpdateTargetSelection(STargetSelectionInfo& targetSelectionInfo)
{
	bool bResult = false;

	if (gAIEnv.CVars.TargetTracking)
	{
		if (GetTargetTrackBestTarget(targetSelectionInfo.bestTarget, targetSelectionInfo.pTargetInfo, targetSelectionInfo.bCurrentTargetErased))
		{
			if (targetSelectionInfo.pTargetInfo &&
			    targetSelectionInfo.pTargetInfo->type == AITARGET_VISUAL &&
			    targetSelectionInfo.pTargetInfo->threat >= AITHREAT_AGGRESSIVE)
			{
				SetAlarmed();
			}
			bResult = true;
		}
	}
	else
	{
		// Disabled
		bResult = false;
	}

	const Group& group = gAIEnv.pGroupManager->GetGroup(GetGroupId());

	if ((group.GetTargetType() > AITARGET_NONE) && (!targetSelectionInfo.pTargetInfo || (group.GetTargetThreat() >= targetSelectionInfo.pTargetInfo->threat && group.GetTargetType() > targetSelectionInfo.pTargetInfo->type)))
	{
		targetSelectionInfo.bestTarget = group.GetTarget().GetWeakRef();
		targetSelectionInfo.targetThreat = group.GetTargetThreat();
		targetSelectionInfo.targetType = group.GetTargetType();
		targetSelectionInfo.bIsGroupTarget = true;

		bResult = true;
	}
	else if (targetSelectionInfo.pTargetInfo)
	{
		targetSelectionInfo.targetThreat = targetSelectionInfo.pTargetInfo->threat;
		targetSelectionInfo.targetType = targetSelectionInfo.pTargetInfo->type;
	}

	return bResult;
}

//===================================================================
// GetTargetTrackBestTarget
//===================================================================
bool CPuppet::GetTargetTrackBestTarget(CWeakRef<CAIObject>& refBestTarget, SAIPotentialTarget*& pTargetInfo, bool& bCurrentTargetErased) const
{
	bool bResult = false;

	refBestTarget.Reset();
	pTargetInfo = NULL;
	bCurrentTargetErased = false;

	tAIObjectID objectId = GetAIObjectID();
	CRY_ASSERT(objectId > 0);

	tAIObjectID targetId = 0;
	gAIEnv.pTargetTrackManager->Update(objectId);
	const uint32 uTargetMethod = (TargetTrackHelpers::eDTM_Select_Highest);
	if (gAIEnv.pTargetTrackManager->GetDesiredTarget(objectId, uTargetMethod, refBestTarget, pTargetInfo))
	{
		bResult = true;
	}
	else
	{
		bCurrentTargetErased = true;
	}

	return bResult;
}

//===================================================================
// UpdatePuppetInternalState
//===================================================================
void CPuppet::UpdatePuppetInternalState()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CCCPOINT(CPuppet_UpdatePuppetInternalState);

	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();

	assert(!m_bDryUpdate);
	// Update alarmed state
	m_alarmedTime -= m_fTimePassed;
	if (m_alarmedTime < 0.0f)
		m_alarmedTime = 0.0f;

	// Sight range threshold.
	// The sight attenuation range envelope changes based on the alarmed state.
	// The threshold is changed smoothly.
	const float alarmLevelChangeTime = 3.0f;
	const float alarmLevelGoal = IsAlarmed() ? 1.0f : 0.0f;
	m_alarmedLevel += (alarmLevelGoal - m_alarmedLevel) * (m_fTimePassed / alarmLevelChangeTime);
	Limit(m_alarmedLevel, 0.0f, 1.0f);

	STargetSelectionInfo targetSelectionInfo;
	if (UpdateTargetSelection(targetSelectionInfo))
	{
		SAIPotentialTarget* bestTargetEvent = targetSelectionInfo.pTargetInfo;

		const EAITargetThreat oldThreat = m_State.eTargetThreat;
		const EAITargetThreat newThreat = targetSelectionInfo.targetThreat;

		if (newThreat > oldThreat && newThreat >= AITHREAT_THREATENING)
		{
			gEnv->pAISystem->GetAIActionManager()->AbortAIAction(GetEntity());
		}

		m_State.eTargetThreat = targetSelectionInfo.targetThreat;
		m_State.eTargetType = targetSelectionInfo.targetType;
		m_State.eTargetID = targetSelectionInfo.bestTarget.GetObjectID();
		m_State.eTargetStuntReaction = AITSR_NONE;
		m_State.bTargetIsGroupTarget = targetSelectionInfo.bIsGroupTarget;

		CAIObject* bestTarget = targetSelectionInfo.bestTarget.GetAIObject();
		if (bestTarget)
		{
			m_State.vTargetPos = bestTarget->GetPos();
		}

		if (bestTarget != pAttentionTarget)
		{
			// New attention target
			SetAttentionTarget(GetWeakRef(bestTarget));
			m_AttTargetPersistenceTimeout = m_Parameters.m_PerceptionParams.targetPersistence;

			// When seeing a visible target, check for stunt reaction.
			if (bestTarget && m_State.eTargetType == AITARGET_VISUAL && m_State.eTargetThreat == AITHREAT_AGGRESSIVE)
			{
				if (CAIPlayer* pPlayer = bestTarget->CastToCAIPlayer())
				{
					if (IsHostile(pPlayer))
					{
						bool isStunt = pPlayer->IsDoingStuntActionRelatedTo(GetPos(), m_Parameters.m_PerceptionParams.sightRange / 5.0f);
						if (m_targetLostTime > m_Parameters.m_PerceptionParams.stuntReactionTimeOut && isStunt)
							m_State.eTargetStuntReaction = AITSR_SEE_STUNT_ACTION;
						else if (pPlayer->IsCloakEffective(bestTarget->GetPos()))
							m_State.eTargetStuntReaction = AITSR_SEE_CLOAKED;
					}
				}
			}

			EntityId bestTargetId = bestTarget ? bestTarget->GetEntityID() : 0;
			if (bestTarget && !bestTargetId)
			{
				CWeakRef<CAIObject> refAssociation = bestTarget->GetAssociation();
				if (refAssociation.IsValid())
					bestTargetId = refAssociation.GetAIObject()->GetEntityID();
			}

			// Keep track of peak threat level and type here
			CTimeValue curTime = GetAISystem()->GetFrameStartTime();

			if ((targetSelectionInfo.targetThreat > m_State.ePeakTargetThreat) || (curTime - m_lastTimeUpdatedBestTarget).GetSeconds() > 30.0f)
			{
				// Save the previous peak now
				m_State.ePreviousPeakTargetThreat = m_State.ePeakTargetThreat;
				m_State.ePreviousPeakTargetType = m_State.ePeakTargetType;
				m_State.ePreviousPeakTargetID = m_State.ePeakTargetID;

				// Now set the new peak
				m_State.ePeakTargetThreat = m_State.eTargetThreat;
				m_State.ePeakTargetType = m_State.eTargetType;
				m_State.ePeakTargetID = bestTarget ? bestTarget->GetAIObjectID() : INVALID_AIOBJECTID;

				// Update the last time we found a better target here
				m_lastTimeUpdatedBestTarget = curTime;
			}

			// Inform AI of change
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			CRY_ASSERT(pData);
			pData->nID = bestTargetId;
			pData->fValue = (targetSelectionInfo.bIsGroupTarget ? 1.0f : 0.0f);
			pData->iValue = m_State.eTargetType;
			pData->iValue2 = m_State.eTargetThreat;
			SetSignal(0, "OnNewAttentionTarget", GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnNewAttentionTarget);

			if (bestTargetEvent)
				bestTargetEvent->bNeedsUpdating = false;
		}
		else if (pAttentionTarget)
		{
			if (m_AttTargetThreat != m_State.eTargetThreat)
			{
				IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
				pData->iValue = m_State.eTargetThreat;
				SetSignal(0, "OnAttentionTargetThreatChanged", GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnAttentionTargetThreatChanged);
			}

			// Handle state change of the current attention target.

			// The state lowering.
			if (m_AttTargetThreat >= AITHREAT_AGGRESSIVE && m_State.eTargetThreat < AITHREAT_AGGRESSIVE)
			{
				// Aggressive -> threatening
				if (m_State.eTargetType == AITARGET_VISUAL || m_State.eTargetType == AITARGET_MEMORY)
					SetSignal(0, "OnNoTargetVisible", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnNoTargetVisible);
			}
			else if (m_AttTargetThreat >= AITHREAT_THREATENING && m_State.eTargetThreat < AITHREAT_THREATENING)
			{
				// Threatening -> interesting
				SetSignal(0, "OnNoTargetAwareness", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnNoTargetAwareness);
			}

			if (bestTargetEvent)
			{
				// The state rising.
				// Use the exposure threat to trigger the signals. This will result multiple
				// same signals (like on threatening sound) to be sent if the exposure is
				// crossing the threshold. This should allow more responsive behaviors, but
				// at the same time prevent too many signals.
				// The signal is resent only if the exposure is crossing the current or higher threshold.
				if (bestTargetEvent->bNeedsUpdating || bestTargetEvent->exposureThreat > bestTargetEvent->threat || m_AttTargetType != bestTargetEvent->type)
				{
					bestTargetEvent->bNeedsUpdating = false;

					if (m_AttTargetExposureThreat <= AITHREAT_AGGRESSIVE && bestTargetEvent->exposureThreat >= AITHREAT_AGGRESSIVE)
					{
						// Threatening -> aggressive
						if (bestTargetEvent->type == AITARGET_VISUAL)
						{
							if (CAIPlayer* pPlayer = bestTarget->CastToCAIPlayer())
							{
								if (IsHostile(pPlayer))
								{
									bool isStunt = pPlayer->IsDoingStuntActionRelatedTo(GetPos(), m_Parameters.m_PerceptionParams.sightRange / 5.0f);
									if (m_targetLostTime > m_Parameters.m_PerceptionParams.stuntReactionTimeOut && isStunt)
										m_State.eTargetStuntReaction = AITSR_SEE_STUNT_ACTION;
									else if (pPlayer->IsCloakEffective(bestTarget->GetPos()))
										m_State.eTargetStuntReaction = AITSR_SEE_CLOAKED;
								}
							}
						}
					}
				}
			}
		}

		// Keep track of the current state, used to track the state transitions.
		m_AttTargetType = m_State.eTargetType;
		m_AttTargetThreat = m_State.eTargetThreat;
		m_AttTargetExposureThreat = bestTargetEvent ? bestTargetEvent->exposureThreat : m_State.eTargetThreat;
	}
	else
	{
		// No attention target, reset if the current target is erased.
		// The check for current target erased allows to forcefully set the
		// attention target.
		if (pAttentionTarget)
		{
			if (targetSelectionInfo.bCurrentTargetErased)
				SetAttentionTarget(NILREF);
		}

		m_AttTargetType = AITARGET_NONE;
		m_AttTargetThreat = AITHREAT_NONE;
		m_AttTargetExposureThreat = AITHREAT_NONE;

		m_State.eTargetType = AITARGET_NONE;
		m_State.eTargetThreat = AITHREAT_NONE;
	}

	// update devaluated points
	DevaluedMap::iterator di, next;
	for (di = m_mapDevaluedPoints.begin(); di != m_mapDevaluedPoints.end(); di = next)
	{
		next = di;
		++next;
		di->second -= m_fTimePassed;
		if (di->second < 0 || !di->first.IsValid())
		{
			CCCPOINT(CPuppet_UpdatePuppetInternalState_DevaluedPoints);

			m_mapDevaluedPoints.erase(di);
		}
	}

	UpdateTargetMovementState();

	// Update attention-target related info:
	// distance from target, in/out of territory status
	if (pAttentionTarget)
	{
		const Vec3& vAttTargetPos = pAttentionTarget->GetPos();

		m_State.fDistanceFromTarget = Distance::Point_Point(vAttTargetPos, GetPos());

		if (m_territoryShape)
		{
			m_attTargetOutOfTerritory.bState = !m_territoryShape->IsPointInsideShape(vAttTargetPos, false);
		}
		else
		{
			m_attTargetOutOfTerritory.bState = false;
		}
	}
	else
	{
		m_State.fDistanceFromTarget = FLT_MAX;
		m_attTargetOutOfTerritory.bState = false;
	}

	if (m_attTargetOutOfTerritory.CheckUpdate())
	{
		bool bState = m_attTargetOutOfTerritory.bState;
		const char* sSignal = (bState ? "OnTargetOutOfTerritory" : "OnTargetInTerritory");
		SetSignal(AISIGNAL_DEFAULT, sSignal, NULL, NULL, 0);
	}
}

//===================================================================
// Event
//===================================================================
void CPuppet::Event(unsigned short eType, SAIEVENT* pEvent)
{
	CAISystem* pAISystem = GetAISystem();
	//	pAISystem->Record(this, IAIRecordable::E_EVENT, GetEventName(eType));
	bool bWasEnabled = m_bEnabled;

	switch (eType)
	{
	case AIEVENT_DROPBEACON:
		UpdateBeacon();
		break;
	case AIEVENT_CLEAR:
		{
			CCCPOINT(CPuppet_Event_Clear);

			ClearActiveGoals();
			m_bLooseAttention = false;
			gAIEnv.pFormationManager->FreeFormationPoint(GetWeakRef(this));
			SetAttentionTarget(NILREF);
			m_bBlocked = false;
			m_bCanReceiveSignals = true;
		}
		break;
	case AIEVENT_CLEARACTIVEGOALS:
		ClearActiveGoals();
		m_bBlocked = false;
		break;
	case AIEVENT_DISABLE:
		{
			m_bEnabled = false;
			//			m_bCheckedBody = true;

			// Reset and disable the agent's target track
			const tAIObjectID aiObjectId = GetAIObjectID();
			gAIEnv.pTargetTrackManager->ResetAgent(aiObjectId);
			gAIEnv.pTargetTrackManager->SetAgentEnabled(aiObjectId, false);

			pAISystem->UpdateGroupStatus(GetGroupId());
			pAISystem->NotifyEnableState(this, m_bEnabled);

			SetObserver(false);
			SetObservable(false);

			SetNavSOFailureStates();
		}
		break;
	case AIEVENT_ENABLE:
		if (GetProxy()->IsDead())
		{
			// can happen when rendering dead bodies? AI should not be enabled
			//				AIAssert(!"Trying to enable dead character!");
			return;
		}
		m_bEnabled = true;
		gAIEnv.pTargetTrackManager->SetAgentEnabled(GetAIObjectID(), true);
		pAISystem->UpdateGroupStatus(GetGroupId());
		pAISystem->NotifyEnableState(this, m_bEnabled);

		SetObserver(true);
		SetObservable(true);
		break;
	case AIEVENT_SLEEP:
		m_fireMode = FIREMODE_OFF;
		m_bCheckedBody = false;
		if (GetProxy()->GetLinkedVehicleEntityId() == 0)
		{
			m_bEnabled = false;
			pAISystem->NotifyEnableState(this, m_bEnabled);
		}
		break;
	case AIEVENT_WAKEUP:
		ClearActiveGoals();
		m_bLooseAttention = false;
		SetAttentionTarget(NILREF);
		m_bEnabled = true;
		pAISystem->NotifyEnableState(this, m_bEnabled);
		m_bCheckedBody = true;
		pAISystem->UpdateGroupStatus(GetGroupId());
		break;
	case AIEVENT_ONVISUALSTIMULUS:
		HandleVisualStimulus(pEvent);
		break;
	case AIEVENT_ONSOUNDEVENT:
		HandleSoundEvent(pEvent);
		break;
	case AIEVENT_ONBULLETRAIN:
		HandleBulletRain(pEvent);
		break;
	case AIEVENT_AGENTDIED:
		{
			SetNavSOFailureStates();

			if (m_pCoverUser)
			{
				m_pCoverUser->Reset();
			}

			ResetModularBehaviorTree(AIOBJRESET_SHUTDOWN);

			pAISystem->NotifyTargetDead(this);

			m_bCheckedBody = false;
			m_bEnabled = false;
			pAISystem->NotifyEnableState(this, m_bEnabled);

			pAISystem->RemoveFromGroup(GetGroupId(), this);

			gAIEnv.pFormationManager->ReleaseFormationPoint(this);
			CancelRequestedPath(false);
			ReleaseFormation();

			ResetCurrentPipe(true);
			m_State.ClearSignals();

			ResetAlertness();

			const EntityId killerID = pEvent->targetId;
			pAISystem->OnAgentDeath(GetEntityID(), killerID);

			if (GetProxy())
				GetProxy()->Reset(AIOBJRESET_SHUTDOWN);

			SetObservable(false);
			SetObserver(false);
		}
		break;
	case AIEVENT_FORCEDNAVIGATION:
		m_vForcedNavigation = pEvent->vForcedNavigation;
		break;
	case AIEVENT_ADJUSTPATH:
		m_adjustpath = pEvent->nType;
		break;
	default:
		// triggering non-existing event - script error?
		// no error - various e AIEVENT_PLAYER_STUNT_ go here - processed in AIPlayer
		//AIAssert(0);
		break;
	}

	// Activate tree if enabled state is changing
	if (bWasEnabled != m_bEnabled)
	{
		// remove from alertness counters
		ResetAlertness();
	}
}

//===================================================================
// HandleVisualStimulus
//===================================================================
void CPuppet::HandleVisualStimulus(SAIEVENT* pAIEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fGlobalVisualPerceptionScale = gEnv->pAISystem->GetGlobalVisualScale(this);
	const float fVisualPerceptionScale = m_Parameters.m_PerceptionParams.perceptionScale.visual * fGlobalVisualPerceptionScale;
	if (gAIEnv.CVars.IgnoreVisualStimulus != 0 || m_Parameters.m_bAiIgnoreFgNode || fVisualPerceptionScale <= 0.0f)
		return;

	if (gAIEnv.pTargetTrackManager->IsEnabled())
	{
		// Check if in range (using perception scale)
		if (eFOV_Outside != IsPointInFOV(pAIEvent->vPosition, fVisualPerceptionScale))
		{
			gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pAIEvent, TargetTrackHelpers::eEST_Visual);
		}
	}
	else if (m_pPerceptionHandler)
		m_pPerceptionHandler->HandleVisualStimulus(pAIEvent);
}

//===================================================================
// GetPersonalInterestManager
//===================================================================
CPersonalInterestManager* CPuppet::GetPersonalInterestManager()
{
	return CCentralInterestManager::GetInstance()->FindPIM(GetEntity());
}

//===================================================================
// UpdateLookTarget
//===================================================================
void CPuppet::UpdateLookTarget(CAIObject* pTarget)
{
	CCCPOINT(CPuppet_UpdateLookTarget);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#ifdef AI_STRIP_OUT_LEGACY_LOOK_TARGET_CODE

	// Essence of this code: look towards the first non-zero look target

	Vec3 positionToLookAt(ZERO);

	LookTargets::iterator it = m_lookTargets.begin();

	while (it != m_lookTargets.end())
	{
		LookTargetPtr lookTarget = it->lock();

		if (lookTarget)
		{
			if (lookTarget->IsZero())
			{
				++it;
				continue;
			}
			else
			{
				positionToLookAt = *lookTarget;
				break;
			}
		}
		else
		{
			it = m_lookTargets.erase(it);
		}
	}

	m_State.vLookTargetPos = positionToLookAt;
	m_State.vShootTargetPos = positionToLookAt;

	return;

#else

	if (pTarget == NULL)
	{
		CPersonalInterestManager* pPIM = GetPersonalInterestManager();
		if (pPIM && pPIM->IsInterested())
		{
			CWeakRef<CAIObject> refInterest = pPIM->GetInterestDummyPoint();
			CAIObject* pInterest = refInterest.GetAIObject();
			if (pInterest)
			{
				ELookStyle eStyle = pPIM->GetLookingStyle();
				if (eStyle == LOOKSTYLE_HARD || eStyle == LOOKSTYLE_SOFT)
				{
					SetAllowedStrafeDistances(999999.0f, 999999.0f, true);
				}
				SetLookStyle(eStyle);
				pTarget = pInterest;
			}
		}
	}

	// Don't look at targets that aren't at least interesting
	if (pTarget && m_refAttentionTarget.GetAIObject() == pTarget && GetAttentionTargetThreat() <= AITHREAT_SUSPECT)
		pTarget = NULL;

	// Update look direction and strafing
	bool lookAtTarget = false;
	// If not moving allow to look at target.
	if (m_State.fDesiredSpeed < 0.01f || m_Path.Empty())
		lookAtTarget = true;

	// Check if strafing should be allowed.
	UpdateStrafing();
	if (m_State.allowStrafing)
	{
		lookAtTarget = true;
	}
	if (m_bLooseAttention)
	{
		CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
		if (pLooseAttentionTarget)
			pTarget = pLooseAttentionTarget;
	}
	if (m_refFireTarget.IsValid() && m_fireMode != FIREMODE_OFF)
		pTarget = GetFireTargetObject();

	Vec3 lookTarget(ZERO);

	if (m_fireMode == FIREMODE_MELEE || m_fireMode == FIREMODE_MELEE_FORCED)
	{
		if (pTarget)
		{
			lookTarget = pTarget->GetPos();
			lookTarget.z = GetPos().z;
			lookAtTarget = true;
		}
	}

	bool use3DNav = IsUsing3DNavigation();
	bool isMoving = m_State.fDesiredSpeed > 0.0f && m_State.curActorTargetPhase == eATP_None && !m_State.vMoveDir.IsZero();

	float distToTarget = FLT_MAX;
	if (pTarget)
	{
		Vec3 dirToTarget = pTarget->GetPos() - GetPos();
		distToTarget = dirToTarget.GetLength();
		if (distToTarget > 0.0001f)
			dirToTarget /= distToTarget;

		// Allow to look at the target when it is almost at the movement direction or very close.
		if (isMoving)
		{
			Vec3 move(m_State.vMoveDir);
			if (!use3DNav)
				move.z = 0.0f;
			move.NormalizeSafe(Vec3Constants<float>::fVec3_Zero);
			if (distToTarget < 2.5f || move.Dot(dirToTarget) > cosf(DEG2RAD(60)))
				lookAtTarget = true;
		}
	}

	if (lookAtTarget && pTarget)
	{
		Vec3 vTargetPos = pTarget->GetPos();

		const float maxDeviation = distToTarget * sinf(DEG2RAD(15));

		if (distToTarget > GetParameters().m_fPassRadius)
		{
			lookTarget = vTargetPos;
			Limit(lookTarget.z, GetPos().z - maxDeviation, GetPos().z + maxDeviation);
		}

		// Clamp the lookat height when the target is close.
		int TargetType = pTarget->GetType();
		if (distToTarget < 1.0f ||
		    (TargetType == AIOBJECT_DUMMY || TargetType == AIOBJECT_HIDEPOINT || TargetType == AIOBJECT_WAYPOINT ||
		     TargetType > AIOBJECT_PLAYER) && distToTarget < 5.0f) // anchors & dummy objects
		{
			if (!use3DNav)
			{
				lookTarget = vTargetPos;
				Limit(lookTarget.z, GetPos().z - maxDeviation, GetPos().z + maxDeviation);
			}
		}
	}
	else if (isMoving && (gAIEnv.configuration.eCompatibilityMode != ECCM_CRYSIS2))
	{
		// Check if strafing should be allowed.

		// Look forward or to the movement direction
		Vec3 lookAheadPoint;
		float lookAheadDist = 2.5f;

		if (m_pPathFollower)
		{
			float junk;
			lookAheadPoint = m_pPathFollower->GetPathPointAhead(lookAheadDist, junk);
		}
		else
		{
			if (!m_Path.GetPosAlongPath(lookAheadPoint, lookAheadDist, !m_movementAbility.b3DMove, true))
				lookAheadPoint = GetPhysicsPos();
		}

		// Since the path height is not guaranteed to follow terrain, do not even try to look up or down.
		lookTarget = lookAheadPoint;

		// Make sure the lookahead position is far enough so that the catchup logic in the path following
		// together with look-ik does not get flipped.
		Vec3 delta = lookTarget - GetPhysicsPos();
		delta.z = 0.0f;
		float dist = delta.GetLengthSquared();
		if (dist < sqr(1.0f))
		{
			float u = 1.0f - sqrtf(dist);
			Vec3 safeDir = GetEntityDir();
			safeDir.z = 0;
			delta = delta + (safeDir - delta) * u;
		}
		delta.Normalize();

		lookTarget = GetPhysicsPos() + delta * 40.0f;
		lookTarget.z = GetPos().z;
	}
	else
	{
		// Disable look target.
		lookTarget.zero();
	}

	if (!m_posLookAtSmartObject.IsZero())
	{
		// The SO lookat should override the lookat target in case not requesting to fire and not using lookat goalop.
		if (!m_bLooseAttention && m_fireMode == FIREMODE_OFF)
		{
			lookTarget = m_posLookAtSmartObject;
		}
	}

	if (!lookTarget.IsZero())
	{
		if (m_allowStrafeLookWhileMoving && m_fireMode != FIREMODE_OFF && GetFireTargetObject())
		{
			float distSqr = Distance::Point_Point2DSq(GetFireTargetObject()->GetPos(), GetPos());
			if (!m_closeRangeStrafing)
			{
				// Outside the range
				const float thr = GetParameters().m_PerceptionParams.sightRange * 0.12f;
				if (distSqr < sqr(thr))
					m_closeRangeStrafing = true;
			}
			if (m_closeRangeStrafing)
			{
				// Inside the range
				m_State.allowStrafing = true;
				const float thr = GetParameters().m_PerceptionParams.sightRange * 0.12f + 2.0f;
				if (distSqr > sqr(thr))
					m_closeRangeStrafing = false;
			}
		}

		float distSqr = Distance::Point_Point2DSq(lookTarget, GetPos());
		if (distSqr < sqr(2.0f))
		{
			Vec3 dirToLookTarget = lookTarget - GetPos();
			dirToLookTarget.GetNormalizedSafe(GetEntityDir());
			Vec3 fakePos = GetPos() + dirToLookTarget * 2.0f;
			//Disable setting a look target if you're virtually on top of the point
			if (distSqr < sqr(0.12f))
			{
				lookTarget.zero();
			}
			else if (distSqr < sqr(0.7f))
			{
				lookTarget = fakePos;
			}
			else
			{
				float speed = m_State.vMoveDir.GetLength();
				speed = clamp_tpl(speed, 0.0f, 10.f);
				float d = sqrtf(distSqr);
				float u = 1.0f - (d - 0.7f) / (2.0f - 0.7f);
				lookTarget += speed / 10 * u * (fakePos - lookTarget);
			}
		}
	}

	// for the invehicle gunners
	if (GetProxy())
	{
		const SAIBodyInfo& bodyInfo = GetBodyInfo();

		if (IEntity* pLinkedVehicleEntity = bodyInfo.GetLinkedVehicleEntity())
		{
			if (GetProxy()->GetActorIsFallen())
			{
				lookTarget.zero();
			}
			else
			{
				CAIObject* pUnit = (CAIObject*)pLinkedVehicleEntity->GetAI();
				if (pUnit)
				{
					if (pUnit->CastToCAIVehicle())
					{
						lookTarget.zero();
						CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
						if (m_bLooseAttention && pLooseAttentionTarget)
							pTarget = pLooseAttentionTarget;
						if (pTarget)
						{
							lookTarget = pTarget->GetPos();
							m_State.allowStrafing = false;
						}
					}
				}
			}
		}
	}

	if (GetSubType() != STP_HELICRYSIS2)
	{
		float lookTurnSpeed = GetAlertness() > 0 ? m_Parameters.m_lookCombatTurnSpeed : m_Parameters.m_lookIdleTurnSpeed;
		//If entity is not moving, then do not use look target interpolation, smoothing will come from turn animations.
		if (lookTurnSpeed <= 0.0f || !isMoving)
			m_State.vLookTargetPos = lookTarget;
		else
			m_State.vLookTargetPos = InterpolateLookOrAimTargetPos(m_State.vLookTargetPos, lookTarget, lookTurnSpeed);
	}

#endif

}

//===================================================================
// InterpolateLookOrAimTargetPos
//===================================================================
Vec3 CPuppet::InterpolateLookOrAimTargetPos(const Vec3& current, const Vec3& target, float maxRatePerSec)
{
	if ((!target.IsZero()) && !target.IsEquivalent(m_vLastPosition))
	{
		// Interpolate.
		Vec3 curDir;
		if (current.IsZero())
		{
			curDir = GetEntityDir(); // GetViewDir();
		}
		else
		{
			curDir = current - GetPos();
			curDir.Normalize();
		}

		Vec3 reqDir = target - m_vLastPosition;  // GetPos();
		float dist = reqDir.NormalizeSafe();

		// Slerp
		float cosAngle = curDir.Dot(reqDir);

		const float eps = 1e-6f;
		const float maxRate = maxRatePerSec * GetAISystem()->GetFrameDeltaTime();
		const float thr = cosf(maxRate);

		if (cosAngle > thr || maxRate < eps || dist < eps)
		{
			return target;
		}
		else
		{
			float angle = acos_tpl(cosAngle);

			// Allow higher rate when over 90degrees.
			const float piOverFour = gf_PI / 4.0f;
			const float rcpPIOverFour = 1.0f / piOverFour;
			float scale = 1.0f + clamp_tpl((angle - piOverFour) * rcpPIOverFour, 0.0f, 1.0f) * 2.0f;
			if (angle < eps || angle < maxRate * scale)
				return target;

			float t = (maxRate * scale) / angle;

			Quat curView;
			curView.SetRotationVDir(curDir);
			Quat reqView;
			reqView.SetRotationVDir(reqDir);

			Quat view;
			view.SetSlerp(curView, reqView, t);

			return GetPos() + (view * FORWARD_DIRECTION) * dist;
		}
	}

	// Clear look target.
	return target;
}

//===================================================================
// UpdateLookTarget3D
//===================================================================
void CPuppet::UpdateLookTarget3D(CAIObject* pTarget)
{
	// This is for the scout mainly
	m_State.vLookTargetPos.zero();
	m_State.aimTargetIsValid = false;

	CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
	if (m_bLooseAttention && pLooseAttentionTarget)
	{
		pTarget = pLooseAttentionTarget;
		m_State.vLookTargetPos = pTarget->GetPos();
	}

	m_State.vForcedNavigation = m_vForcedNavigation;
}

//===================================================================
// AdjustMovementUrgency
//===================================================================
void CPuppet::AdjustMovementUrgency(float& urgency, float pathLength, float* maxPathLen)
{
	if (!maxPathLen)
		maxPathLen = &m_adaptiveUrgencyMaxPathLen;

	if (m_adaptiveUrgencyScaleDownPathLen > 0.0001f)
	{
		if (pathLength > *maxPathLen)
		{
			*maxPathLen = pathLength;

			int scaleDown = 0;
			float scale = m_adaptiveUrgencyScaleDownPathLen;
			while (scale > m_adaptiveUrgencyMaxPathLen && scaleDown < 4)
			{
				scale /= 2.0f;
				scaleDown++;
			}

			int minIdx = MovementUrgencyToIndex(m_adaptiveUrgencyMin);
			int maxIdx = MovementUrgencyToIndex(m_adaptiveUrgencyMax);
			int idx = maxIdx - scaleDown;
			if (idx < minIdx) idx = minIdx;

			// Special case for really short paths.
			if (*maxPathLen < 1.2f && idx > 2)
				idx = 2; // walk

			urgency = IndexToMovementUrgency(idx);
		}
	}
}

//===================================================================
// HandlePathDecision
//===================================================================
void CPuppet::HandlePathDecision(MNMPathRequestResult& result)
{
	CPipeUser::HandlePathDecision(result);

	if (result.HasPathBeenFound())
	{
		const bool validPath = !result.pPath->Empty();
		if (validPath)
		{
			// Update adaptive urgency control before the path gets processed further
			AdjustMovementUrgency(m_State.fMovementUrgency, m_Path.GetPathLength(!IsUsing3DNavigation()));

			AdjustPath();
		}
	}
}

//===================================================================
// Devalue
//===================================================================
void CPuppet::Devalue(IAIObject* pObject, bool bDevaluePuppets, float fAmount)
{
	DevaluedMap::iterator di;

	unsigned short type = ((CAIObject*)pObject)->GetType();

	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
	CWeakRef<CAIObject> refObject = GetWeakRefSafe((CAIObject*)pObject);

	if (pObject == pAttentionTarget)
		SetAttentionTarget(NILREF);

	if ((type == AIOBJECT_ACTOR) && !bDevaluePuppets)
		return;

	if (type == AIOBJECT_PLAYER)
		return;

	CCCPOINT(CPuppet_Devalue);

	// remove it from map of pending events, so that it does not get reacquire
	if (m_pPerceptionHandler)
		m_pPerceptionHandler->RemoveEvent(refObject);

	if ((di = m_mapDevaluedPoints.find(refObject)) == m_mapDevaluedPoints.end())
		m_mapDevaluedPoints.insert(DevaluedMap::iterator::value_type(refObject, fAmount));
}

//===================================================================
// IsDevalued
//===================================================================
bool CPuppet::IsDevalued(IAIObject* pObject)
{
	CWeakRef<CAIObject> refObject = GetWeakRefSafe((CAIObject*)pObject);
	return m_mapDevaluedPoints.find(refObject) != m_mapDevaluedPoints.end();
}

//===================================================================
// ClearDevalued
//===================================================================
void CPuppet::ClearDevalued()
{
	m_mapDevaluedPoints.clear();
}

//===================================================================
// OnObjectRemoved
//===================================================================
void CPuppet::OnObjectRemoved(CAIObject* pObject)
{
	CAIActor::OnObjectRemoved(pObject);

	if (m_pPerceptionHandler)
		m_pPerceptionHandler->RemoveEvent(GetWeakRef(pObject));

	if (!m_steeringObjects.empty())
		m_steeringObjects.erase(std::remove(m_steeringObjects.begin(), m_steeringObjects.end(), pObject), m_steeringObjects.end());
}

//===================================================================
// UpTargetPriority
//===================================================================
void CPuppet::UpTargetPriority(const IAIObject* pTarget, float fPriorityIncrement)
{
	if (m_pPerceptionHandler)
		m_pPerceptionHandler->UpTargetPriority(GetWeakRef((CAIObject*)pTarget), fPriorityIncrement);
}

//===================================================================
// HandleSoundEvent
//===================================================================
void CPuppet::HandleSoundEvent(SAIEVENT* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fGlobalAudioPerceptionScale = gEnv->pAISystem->GetGlobalAudioScale(this);
	const float fAudioPerceptionScale = m_Parameters.m_PerceptionParams.perceptionScale.audio * fGlobalAudioPerceptionScale;
	if (gAIEnv.CVars.IgnoreSoundStimulus != 0 || m_Parameters.m_bAiIgnoreFgNode || fAudioPerceptionScale <= 0.0f)
		return;

	if (gAIEnv.pTargetTrackManager->IsEnabled())
	{
		// Check if in range (using perception scale)
		const Vec3& vMyPos = GetPos();
		const float fSoundDistance = vMyPos.GetDistance(pEvent->vPosition) * (1.0f / fAudioPerceptionScale);
		if (fSoundDistance <= pEvent->fThreat)
		{
			gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pEvent, TargetTrackHelpers::eEST_Sound);
		}
	}
	if (m_pPerceptionHandler)
		m_pPerceptionHandler->HandleSoundEvent(pEvent);
}

//===================================================================
// HandleBulletRain
//===================================================================
void CPuppet::HandleBulletRain(SAIEVENT* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CPipeUser::HandleBulletRain(pEvent);

	if (gAIEnv.pTargetTrackManager->IsEnabled())
		gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pEvent, TargetTrackHelpers::eEST_BulletRain);
	else if (m_pPerceptionHandler)
		m_pPerceptionHandler->HandleBulletRain(pEvent);
}

//===================================================================
// SetForcedNavigation
//===================================================================
void CPuppet::SetForcedNavigation(const Vec3& vDirection, float fSpeed)
{
	m_vForcedNavigation = vDirection;
	m_fForcedNavigationSpeed = fSpeed;
}

//===================================================================
// ClearForcedNavigation
//===================================================================
void CPuppet::ClearForcedNavigation()
{
	m_vForcedNavigation.zero();
	m_fForcedNavigationSpeed = 0.0f;
}

CPuppet::ENavInteraction CPuppet::NavigateAroundObjectsBasicCheck(const CAIObject* object) const
{
	//These checks have been factored out of the old NavigateAroundObjectsBasicCheck so that they can be used
	//by the old version as well as the new approach in NavigateAroundObjects

	ENavInteraction ret = NI_IGNORE;

	if (object)
	{
		//to make sure we skip disable entities (hidden in the game, etc)
		IEntity* pEntity(object->GetEntity());
		if (pEntity && pEntity->IsActivatedForUpdates())
		{
			if (AIOBJECT_VEHICLE == object->GetType() || object->IsEnabled()) // vehicles are not enabled when idle, so don't skip them
			{
				if (this != object)
				{
					ret = GetNavInteraction(this, object);
				}
			}
		}
	}

	return ret;
}

//===================================================================
// NavigateAroundObjectsBasicCheck
//===================================================================
bool CPuppet::NavigateAroundObjectsBasicCheck(const Vec3& targetPos, const Vec3& myPos, bool in3D, const CAIObject* object, float extraDist)
{
	bool ret = false;

	//now calling the factored out function
	ENavInteraction navInteraction = NavigateAroundObjectsBasicCheck(object);
	if (navInteraction != NI_IGNORE)
	{
		Vec3 objectPos = object->GetPos();
		Vec3 delta = objectPos - myPos;
		const CAIActor* pActor = object->CastToCAIActor();
		float avoidanceR = m_movementAbility.avoidanceRadius;
		avoidanceR += pActor->m_movementAbility.avoidanceRadius;
		avoidanceR += extraDist;
		float avoidanceRSq = square(avoidanceR);

		// skip if we're not close
		float distSq = delta.GetLengthSquared();
		if (!(distSq > avoidanceRSq))
		{
			ret = true;
		}
	}

	return ret;
}

//===================================================================
// NavigateAroundObjectsInternal
//===================================================================
bool CPuppet::NavigateAroundObjectsInternal(const Vec3& targetPos, const Vec3& myPos, bool in3D, const CAIObject* object)
{
	// skip if we're not trying to move towards
	Vec3 objectPos = object->GetPos();
	Vec3 delta = objectPos - myPos;
	float dot = m_State.vMoveDir.Dot(delta);
	if (dot < 0.001f)
		return false;
	const CAIActor* pActor = object->CastToCAIActor();
	float avoidanceR = m_movementAbility.avoidanceRadius;
	avoidanceR += pActor->m_movementAbility.avoidanceRadius;
	float avoidanceRSq = square(avoidanceR);

	// skip if we're not close
	float distSq = delta.GetLengthSquared();
	if (distSq > avoidanceRSq)
		return false;

	return NavigateAroundAIObject(targetPos, object, myPos, objectPos, true, in3D);
}

//===================================================================
// NavigateAroundObjects
//===================================================================
bool CPuppet::NavigateAroundObjects(const Vec3& targetPos, bool fullUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	bool in3D = IsUsing3DNavigation();
	const Vec3 myPos = GetPhysicsPos();

	bool steering = false;
	bool lastSteeringEnabled = m_steeringEnabled;
	bool steeringEnabled = m_State.vMoveDir.GetLength() > 0.01f && m_State.fDesiredSpeed > 0.01f;

	CTimeValue curTime = GetAISystem()->GetFrameStartTime();

	int64 deltaTime = (curTime - m_lastSteerTime).GetMilliSecondsAsInt64();
	const int64 timeForUpdate = 500;

	if (steeringEnabled && (deltaTime > timeForUpdate || !lastSteeringEnabled))
	{
		CRY_PROFILE_REGION(PROFILE_AI, "NavigateAroundObjects Gather Objects");

		m_lastSteerTime = curTime;
		m_steeringObjects.clear();

		//This approach now uses a cylinder overlap to collect the entities surrounding
		//this puppet. The cylinder has a height of 2.5m, the center of its bottom is
		//at this puppets position. The radius used for the cylinder is the minimum
		//of 2.5m (should probably be made configurable) and the distance to my final
		//goal. That way, when approaching the destpos we don't check any further.
		float radius = min(2.5f, Distance::Point_Point(myPos, m_Path.GetLastPathPos()));
		Lineseg lseg(myPos, myPos + Vec3(0.0f, 0.0f, 2.5f));

		//We are interested in living and dynamic entities
		static EAICollisionEntities colfilter = static_cast<EAICollisionEntities>(ent_living | AICE_DYNAMIC);

		int32 ent[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		//Gather up to 10 entities surrounding this puppet
		if (OverlapCylinder(lseg, radius, colfilter, GetPhysics(), 0, ent, 10))
		{

			//loop over the id-array until we find an id of 0
			for (int32* entityid = ent; *entityid; ++entityid)
			{
				IEntity* entity = gEnv->pSystem->GetIEntitySystem()->GetEntity(*entityid);
				if (entity)
				{
					CAIObject* aiob = static_cast<CAIObject*>(entity->GetAI());

					//If this AIObject passes the test, push it back onto the vector
					if (NI_IGNORE != NavigateAroundObjectsBasicCheck(aiob))
					{
						m_steeringObjects.push_back(aiob);
					}
				}
			}
		}
	}

	if (steeringEnabled)
	{
		if ((GetType() == AIOBJECT_ACTOR) && !in3D)
		{
			CRY_PROFILE_REGION(PROFILE_AI, "NavigateAroundObjects Update Steering");

			bool check = fullUpdate;
			if (m_updatePriority == AIPUP_VERY_HIGH || m_updatePriority == AIPUP_HIGH)
				check = true;

			if (check || !lastSteeringEnabled)
			{
				AABB selfBounds;
				GetEntity()->GetWorldBounds(selfBounds);
				selfBounds.min.z -= 0.25f;
				selfBounds.max.z += 0.25f;

				// Build occupancy map
				const float radScale = 1.0f - clamp_tpl(m_steeringAdjustTime - 1.0f, 0.0f, 1.0f);
				const float selfRad = m_movementAbility.pathRadius * radScale;
				m_steeringOccupancy.Reset(GetPhysicsPos(), GetEntityDir(), m_movementAbility.avoidanceRadius * 2.0f);

				for (unsigned i = 0, ni = m_steeringObjects.size(); i < ni; ++i)
				{
					const CAIActor* pActor = m_steeringObjects[i]->CastToCAIActor();
					if (!pActor) // || !pActor->IsActive())
						continue;

					// Check that the height overlaps.
					IEntity* pActorEnt = pActor->GetEntity();
					AABB bounds;
					pActorEnt->GetWorldBounds(bounds);
					if (selfBounds.min.z >= bounds.max.z || selfBounds.max.z <= bounds.min.z)
						continue;

					if (pActor->GetType() == AIOBJECT_VEHICLE)
					{
						AABB localBounds;
						pActor->GetLocalBounds(localBounds);
						const Matrix34& tm = pActorEnt->GetWorldTM();

						SAIRect3 r;
						GetFloorRectangleFromOrientedBox(tm, localBounds, r);
						// Extend the box based on velocity.
						Vec3 vel = pActor->GetVelocity();
						float speedu = r.axisu.Dot(vel) * 0.25f;
						float speedv = r.axisv.Dot(vel) * 0.25f;
						if (speedu > 0)
							r.max.x += speedu;
						else
							r.min.x += speedu;
						if (speedv > 0)
							r.max.y += speedv;
						else
							r.min.y += speedv;

						// Extend the box based on the
						r.min.x -= selfRad;
						r.min.y -= selfRad;
						r.max.x += selfRad;
						r.max.y += selfRad;

						const unsigned maxpts = 4;
						Vec3 rect[maxpts];
						rect[0] = r.center + r.axisu * r.min.x + r.axisv * r.min.y;
						rect[1] = r.center + r.axisu * r.max.x + r.axisv * r.min.y;
						rect[2] = r.center + r.axisu * r.max.x + r.axisv * r.max.y;
						rect[3] = r.center + r.axisu * r.min.x + r.axisv * r.max.y;

						/*						unsigned last = maxpts-1;
						            for (unsigned i = 0; i < maxpts; ++i)
						            {
						              GetAISystem()->AddDebugLine(rect[last], rect[i], 255,0,0, 0);
						              last = i;
						            }*/

						float x = r.axisu.Dot(m_steeringOccupancy.GetCenter() - r.center);
						float y = r.axisv.Dot(m_steeringOccupancy.GetCenter() - r.center);

						const float eps = 0.1f;
						if (x >= r.min.x - eps && x <= r.max.x + eps && y >= r.min.y - eps && y <= r.max.y + eps)
						{
							//							GetAISystem()->AddDebugLine(m_steeringOccupancy.GetCenter()+Vec3(0,0,-10), m_steeringOccupancy.GetCenter()+Vec3(0,0,10), 0,0,0, 0);
							m_steeringOccupancy.AddObstructionDirection(r.center - m_steeringOccupancy.GetCenter());
						}
						else
						{
							unsigned last = maxpts - 1;
							for (unsigned j = 0; j < maxpts; ++j)
							{
								m_steeringOccupancy.AddObstructionLine(rect[last], rect[j]);
								last = j;
							}
						}
					}
					else
					{
						const Vec3& pos = pActor->GetPhysicsPos();
						const float rad = pActor->GetMovementAbility().pathRadius;

						const Vec3& bodyDir = pActor->GetEntityDir();
						Vec3 right(bodyDir.y, -bodyDir.x, 0);
						right.Normalize();

						m_steeringOccupancy.AddObstructionCircle(pos, selfRad + rad);
						Vec3 vel = pActor->GetVelocity();
						if (vel.GetLengthSquared() > sqr(0.1f))
							m_steeringOccupancy.AddObstructionCircle(pos + vel * 0.25f + right * rad * 0.3f, selfRad + rad);
					}
				}
			}

			// Steer around
			Vec3 oldMoveDir = m_State.vMoveDir;
			m_State.vMoveDir = m_steeringOccupancy.GetNearestUnoccupiedDirection(m_State.vMoveDir, m_steeringOccupancyBias);

			if (gAIEnv.CVars.DebugDrawCrowdControl > 0)
			{
				Vec3 pos = GetPhysicsPos() + Vec3(0, 0, 0.75f);
				GetAISystem()->AddDebugLine(pos, pos + oldMoveDir * 2.0f, 196, 0, 0, 0);
				GetAISystem()->AddDebugLine(pos, pos + m_State.vMoveDir * 2.0f, 255, 196, 0, 0);
			}

			if (fabsf(m_steeringOccupancyBias) > 0.1f)
			{
				steering = true;

				Vec3 aheadPos;
				if (m_pPathFollower)
				{
					float junk;
					aheadPos = m_pPathFollower->GetPathPointAhead(m_movementAbility.pathRadius, junk);
					if (Distance::Point_Point2DSq(aheadPos, GetPhysicsPos()) < square(m_movementAbility.avoidanceRadius))
					{
						m_pPathFollower->Advance(m_movementAbility.pathRadius * 0.3f);
					}
				}
				else
				{
					aheadPos = m_Path.CalculateTargetPos(myPos, 0.f, 0.f, m_movementAbility.pathRadius, true);
					if (!m_Path.Empty() && Distance::Point_Point2DSq(aheadPos, GetPhysicsPos()) < square(m_movementAbility.avoidanceRadius))
					{
						Vec3 pathNextPoint;
						if (m_Path.GetPosAlongPath(pathNextPoint, m_movementAbility.pathRadius * 0.3f, true, false))
							m_Path.UpdatePathPosition(pathNextPoint, 100.0f, true, false);
					}
				}

				float slowdown = 1.0f - (oldMoveDir.Dot(m_State.vMoveDir) + 1.0f) * 0.5f;

				float normalSpeed, minSpeed, maxSpeed;
				GetMovementSpeedRange(m_State.fMovementUrgency, m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

				m_State.fDesiredSpeed += (minSpeed - m_State.fDesiredSpeed) * slowdown;

				if (slowdown > 0.1f)
					m_steeringAdjustTime += m_fTimePassed;
			}
			else
			{
				m_steeringAdjustTime -= m_fTimePassed;
			}

			Limit(m_steeringAdjustTime, 0.0f, 3.0f);
		}
		else
		{
			CRY_PROFILE_REGION(PROFILE_AI, "NavigateAroundObjects Update Steering Old");
			// Old type steering for the rest of the objects.
			unsigned nObj = m_steeringObjects.size();
			for (unsigned i = 0; i < nObj; ++i)
			{
				const CAIObject* object = m_steeringObjects[i];
				if (NavigateAroundObjectsInternal(targetPos, myPos, in3D, object))
					steering = true;
			}
		}
	}

	m_steeringEnabled = steeringEnabled;

	return steering;
}

//===================================================================
// NavigateAroundAIObject
//===================================================================
bool CPuppet::NavigateAroundAIObject(const Vec3& targetPos, const CAIObject* obstacle, const Vec3& myPos,
                                     const Vec3& objectPos, bool steer, bool in3D)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (steer)
	{
		if (in3D)
			return SteerAround3D(targetPos, obstacle, myPos, objectPos);
		else if (obstacle->GetType() == AIOBJECT_VEHICLE)
			return SteerAroundVehicle(targetPos, obstacle, myPos, objectPos);
		else
			return SteerAroundPuppet(targetPos, obstacle, myPos, objectPos);
	}
	else
	{
		AIError("NavigateAroundAIObject - only handles steering so far");
		return false;
	}
}

//===================================================================
// SteerAroundPuppet
//===================================================================
bool CPuppet::SteerAroundPuppet(const Vec3& targetPos, const CAIObject* object, const Vec3& myPos, const Vec3& objectPos)
{
	const CPuppet* pPuppet = object->CastToCPuppet();
	if (!pPuppet)
		return false;

	float avoidanceR = m_movementAbility.avoidanceRadius;
	avoidanceR += pPuppet->m_movementAbility.avoidanceRadius;
	float avoidanceRSq = square(avoidanceR);

	float maxAllowedSpeedMod = 10.0f;
	Vec3 steerOffset(ZERO);

	bool outside = true;
	//if (m_lastNavNodeIndex && (gAIEnv.pGraph->GetNodeManager().GetNode(m_lastNavNodeIndex)->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD)) == 0)
	//	outside = false;

	Vec3 delta = objectPos - myPos;
	// skip if we're not close
	float distSq = delta.GetLengthSquared();
	if (distSq > avoidanceRSq)
		return false;

	// don't overtake unless we're in more of a hurry
	static float overtakeScale = 2.0f;
	static float overtakeOffset = 0.1f;
	static float criticalOvertakeMoveDot = 0.4f;

	float holdbackDist = 0.6f;
	if (object->GetType() == AIOBJECT_VEHICLE)
		holdbackDist = 15.0f;

	if (m_State.vMoveDir.Dot(pPuppet->GetState().vMoveDir) > criticalOvertakeMoveDot)
	{
		float myNormalSpeed, myMinSpeed, myMaxSpeed;
		GetMovementSpeedRange(m_State.fMovementUrgency, m_State.allowStrafing, myNormalSpeed, myMinSpeed, myMaxSpeed);
		//    float myNormalSpeed = GetNormalMovementSpeed(m_State.fMovementUrgency, true);
		//    float otherNormalSpeed = pPuppet->GetNormalMovementSpeed(pPuppet->m_State.fMovementUrgency, true);
		float otherNormalSpeed, otherMinSpeed, otherMaxSpeed;
		pPuppet->GetMovementSpeedRange(pPuppet->m_State.fMovementUrgency, pPuppet->m_State.allowStrafing, otherNormalSpeed, otherMinSpeed, otherMaxSpeed);

		if (!outside || myNormalSpeed < overtakeScale * otherNormalSpeed + overtakeOffset)
		{
			float maxDesiredSpeed = max(myMinSpeed, object->GetVelocity().Dot(m_State.vMoveDir));

			float dist = sqrtf(distSq);
			dist -= holdbackDist;

			float extraFrac = dist / holdbackDist;
			Limit(extraFrac, -0.1f, 0.1f);
			maxDesiredSpeed *= 1.0f + extraFrac;

			if (m_State.fDesiredSpeed > maxDesiredSpeed)
				m_State.fDesiredSpeed = maxDesiredSpeed;

			return true;
		}
		avoidanceR *= 0.75f;
		avoidanceRSq = square(avoidanceR);
	}

	// steer around
	Vec3 aheadPos;
	if (m_pPathFollower)
	{
		float junk;
		aheadPos = m_pPathFollower->GetPathPointAhead(m_movementAbility.pathRadius, junk);
		if (outside && Distance::Point_Point2DSq(aheadPos, objectPos) < square(avoidanceR))
		{
			static float advanceFrac = 0.2f;
			m_pPathFollower->Advance(advanceFrac * avoidanceR);
		}
	}
	else
	{
		aheadPos = m_Path.CalculateTargetPos(myPos, 0.f, 0.f, m_movementAbility.pathRadius, true);
		if (outside && !m_Path.Empty() && Distance::Point_Point2DSq(aheadPos, objectPos) < square(avoidanceR))
		{
			static float advanceFrac = 0.2f;
			Vec3 pathNextPoint;
			if (m_Path.GetPosAlongPath(pathNextPoint, advanceFrac * avoidanceR, true, false))
				m_Path.UpdatePathPosition(pathNextPoint, 100.0f, true, false);
		}
	}

	Vec3 toTargetDir(objectPos - myPos);
	toTargetDir.z = 0.f;
	float toTargetDist = toTargetDir.NormalizeSafe();
	Vec3 forward2D(m_State.vMoveDir);
	forward2D.z = 0.f;
	forward2D.NormalizeSafe();
	Vec3 steerSideDir(forward2D.y, -forward2D.x, 0.f);
	// +ve when approaching, -ve when receeding
	float toTargetDot = forward2D.Dot(toTargetDir);
	Limit(toTargetDot, 0.5f, 1.0f);

	// which way to turn
	float steerSign = steerSideDir.Dot(toTargetDir);
	steerSign = steerSign > 0.0f ? 1.0f : -1.0f;

	float toTargetDistScale = 1.0f - (toTargetDist / avoidanceR);
	Limit(toTargetDistScale, 0.0f, 1.0f);
	toTargetDistScale = sqrtf(toTargetDistScale);

	Vec3 thisSteerOffset = -toTargetDistScale * steerSign * steerSideDir * toTargetDot;
	if (!outside)
		thisSteerOffset *= 0.1f;

	steerOffset += thisSteerOffset;

	float normalSpeed, minSpeed, maxSpeed;
	GetMovementSpeedRange(m_State.fMovementUrgency, m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);
	//  float normalSpeed = GetNormalMovementSpeed(m_State.fMovementUrgency, true);
	if (m_State.fDesiredSpeed > maxAllowedSpeedMod * normalSpeed)
		m_State.fDesiredSpeed = maxAllowedSpeedMod * normalSpeed;

	if (!steerOffset.IsZero())
	{
		m_State.vMoveDir += steerOffset;
		m_State.vMoveDir.NormalizeSafe();
	}
	return true;
}

//===================================================================
// SteerAround3D
// Danny TODO make this 3D!!!
//===================================================================
bool CPuppet::SteerAround3D(const Vec3& targetPos, const CAIObject* object, const Vec3& myPos, const Vec3& objectPos)
{
	const CAIActor* pActor = object->CastToCAIActor();
	float avoidanceR = m_movementAbility.avoidanceRadius;
	avoidanceR += pActor->m_movementAbility.avoidanceRadius;
	float avoidanceRSq = square(avoidanceR);

	Vec3 delta = objectPos - myPos;
	// skip if we're not close
	float distSq = delta.GetLengthSquared();
	if (distSq > square(avoidanceR))
		return false;

	Vec3 aheadPos = m_Path.CalculateTargetPos(myPos, 0.f, 0.f, m_movementAbility.pathRadius, true);
	if (Distance::Point_Point2DSq(aheadPos, objectPos) < square(avoidanceR))
	{
		static float advanceFrac = 0.2f;
		Vec3 pathNextPoint;
		if (m_Path.GetPosAlongPath(pathNextPoint, advanceFrac * avoidanceR, true, false))
			m_Path.UpdatePathPosition(pathNextPoint, 100.0f, true, false);
	}
	Vec3 toTargetDir(objectPos - myPos);
	toTargetDir.z = 0.f;
	float toTargetDist = toTargetDir.NormalizeSafe();
	Vec3 forward2D(m_State.vMoveDir);
	forward2D.z = 0.f;
	forward2D.NormalizeSafe();
	Vec3 steerDir(forward2D.y, -forward2D.x, 0.f);

	// +ve when approaching, -ve when receeding
	float toTargetDot = forward2D.Dot(toTargetDir);
	// which way to turn
	float steerSign = steerDir.Dot(toTargetDir);
	steerSign = steerSign > 0.0f ? 1.0f : -1.0f;

	float toTargetDistScale = 1.0f - (toTargetDist / avoidanceR);
	Limit(toTargetDistScale, 0.0f, 1.0f);
	toTargetDistScale = sqrtf(toTargetDistScale);

	m_State.vMoveDir = m_State.vMoveDir - toTargetDistScale * steerSign * steerDir * toTargetDot;
	m_State.vMoveDir.NormalizeSafe();

	return true;
}

//===================================================================
// SteerAroundVehicles
//===================================================================
bool CPuppet::SteerAroundVehicle(const Vec3& targetPos, const CAIObject* object, const Vec3& myPos, const Vec3& objectPos)
{
	// if vehicle is in the same formation (convoy) - don't steer around it, otherwise can't stay in formation
	if (const CAIVehicle* pVehicle = object->CastToCAIVehicle())
	{
		if (gAIEnv.pFormationManager->SameFormation(this, pVehicle))
			return false;
	}
	// currently puppet algorithm seems to work ok.
	return SteerAroundPuppet(targetPos, object, myPos, objectPos);
}

//===================================================================
// CreateFormation
//===================================================================
bool CPuppet::CreateFormation(const char* szName, Vec3 vTargetPos)
{
	// special handling for beacons :) It will create a formation point where the current target of the
	// puppet is.
	if (!stricmp(szName, "beacon"))
	{
		UpdateBeacon();
		return (m_pFormation != NULL);
	}
	return CAIObject::CreateFormation(szName, vTargetPos);
}

//===================================================================
// Reset
//===================================================================
void CPuppet::Reset(EObjectResetType type)
{
	AILogComment("CPuppet::Reset %s (%p)", GetName(), this);

	CPipeUser::Reset(type); // creates the proxy

	m_bCanReceiveSignals = true;

	LineOfFireState().Swap(m_lineOfFireState);
	ValidTargetState().Swap(m_validTargetState);

	CAISystem* pAISystem = GetAISystem();

	m_steeringOccupancy.Reset(Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_OneY, 1.0f);
	m_steeringOccupancyBias = 0;
	m_steeringAdjustTime = 0;

	m_mapDevaluedPoints.clear();

	m_steeringObjects.clear();

	ClearPotentialTargets();
	m_fLastUpdateTime = 0.0f;

	m_bDryUpdate = false;

	m_fLastNavTest = 0.0f;

	//Reset target movement tracking
	m_targetApproaching = false;
	m_targetFleeing = false;
	m_targetApproach = 0;
	m_targetFlee = 0;
	m_lastTargetValid = false;
	m_allowedStrafeDistanceStart = 0.0f;
	m_allowedStrafeDistanceEnd = 0.0f;
	m_allowStrafeLookWhileMoving = false;
	m_strafeStartDistance = 0;
	m_closeRangeStrafing = false;

	m_currentWeaponId = 0;
	m_CurrentWeaponDescriptor = AIWeaponDescriptor();

	m_bGrenadeThrowRequested = false;
	m_eGrenadeThrowRequestType = eRGT_INVALID;
	m_iGrenadeThrowTargetType = 0;

	m_lastAimObstructionResult = true;
	m_updatePriority = AIPUP_LOW;

	m_adaptiveUrgencyMin = 0.0f;
	m_adaptiveUrgencyMax = 0.0f;
	m_adaptiveUrgencyScaleDownPathLen = 0.0f;
	m_adaptiveUrgencyMaxPathLen = 0.0f;

	m_delayedStance = STANCE_NULL;
	m_delayedStanceMovementCounter = 0;

	m_timeSinceTriggerPressed = 0.0f;
	m_friendOnWayElapsedTime = 0.0f;

	if (m_pFireCmdHandler)
		m_pFireCmdHandler->Reset();
	if (m_pFireCmdGrenade)
		m_pFireCmdGrenade->Reset();

	m_CurrentHideObject.Set(0, Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero);
	m_InitialPath.clear();

	SetAvoidedVehicle(NILREF);
	//make sure i'm added to groupsMap; no problem if there already.
	pAISystem->AddToGroup(this, GetGroupId());

	// Initially allowed to hit target if not using ambient fire system
	const bool bAmbientFireEnabled = (gAIEnv.CVars.AmbientFireEnable != 0);
	m_allowedToHitTarget = !bAmbientFireEnabled;

	m_allowedToUseExpensiveAccessory = false;
	m_firingReactionTimePassed = false;
	m_firingReactionTime = 0.0f;
	m_outOfAmmoTimeOut = 0.0f;

	m_currentNavSOStates.Clear();
	m_pendingNavSOStates.Clear();

	m_targetSilhouette.Reset();
	m_targetLastMissPoint.zero();
	m_targetFocus = 0.0f;
	m_targetZone = AIZONE_OUT;
	m_targetPosOnSilhouettePlane.zero();
	m_targetDistanceToSilhouette = FLT_MAX;
	m_targetBiasDirection.Set(0, 0, -1);
	m_targetEscapeLastMiss = 0.0f;
	m_targetSeenTime = 0;
	m_targetLostTime = 0;

	m_alarmedTime = 0.0f;
	m_alarmedLevel = 0.0f;

	m_targetDamageHealthThr = -1.0f;
	m_burstEffectTime = 0.0f;
	m_burstEffectState = 0;
	m_targetDazzlingTime = 0.0f;

	if (m_targetDamageHealthThrHistory)
		m_targetDamageHealthThrHistory->Reset();

	m_vForcedNavigation.zero();
	m_fForcedNavigationSpeed = 0.0f;

	m_vehicleStickTarget = 0;

	m_lastMissShotsCount = ~0l;
	m_lastHitShotsCount = ~0l;
	m_lastTargetPart = ~0l;

	m_damagePartsUpdated = false;

	// Default perception descriptors
	stl::free_container(m_SoundPerceptionDescriptor);

	m_fireDisabled = 0;

	ResetAlertness();
}

//===================================================================
// SDynamicHidePositionNotInNavType
//===================================================================
struct SDynamicHidePositionNotInNavType
{
	SDynamicHidePositionNotInNavType(IAISystem::tNavCapMask mask) : mask(mask) {}
	bool operator()(const SHideSpot& hs)
	{
		if (hs.info.type != SHideSpotInfo::eHST_DYNAMIC)
			return false;
		int nBuildingID;
		IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(hs.info.pos, nBuildingID, mask);
		return (navType & mask) == 0;
	}
	IAISystem::tNavCapMask mask;
};

//===================================================================
// IsSmartObjectHideSpot
//===================================================================
inline bool IsSmartObjectHideSpot(const std::pair<float, SHideSpot>& hsPair)
{
	return hsPair.second.info.type == SHideSpotInfo::eHST_SMARTOBJECT;
}

//===================================================================
// Check if path to new spot will cross the target fire dir
//===================================================================
/*static bool	IsRightOf(const Vec3& Pos, const Vec3& Dir,const Vec3& Target)
   {
   Vec3 vDiff = Target - Pos;
   float fDotLeft = (vDiff.x * -Dir.y) + (vDiff.y * Dir.x);

   return (fDotLeft > 0.0f);
   }

   //===================================================================
   // Check if path to new spot will cross the target fire dir
   //===================================================================
   static bool	IsBehindOf(const Vec3& Pos, const Vec3& Dir,const Vec3& Target)
   {
   Vec3 vDiff = Target - Pos;
   float fDotFront = (vDiff.x * Dir.x) + (vDiff.y * Dir.y);

   return (fDotFront < 0.0f);
   }
 */
//===================================================================
// Check if path to new spot will cross the target fire dir
//===================================================================
/*static bool	IsCrossingTargetDir(const Vec3& OriginalPos, const Vec3& PretendedNewPos, const Vec3& TargetPos, const Vec3& TargetDir)
   {
   bool bRet = false;

   Vec3 DirToOriginalPos(TargetPos - OriginalPos);
   Vec3 DirToPretendedPos(TargetPos - PretendedNewPos);

   DirToOriginalPos.z = 0;
   DirToOriginalPos.NormalizeSafe();

   DirToPretendedPos.z = 0;
   DirToPretendedPos.NormalizeSafe();

   //GetAISystem()->AddPerceptionDebugLine("tgt", TargetPos, (TargetPos + (TargetDir * 20.0f)), 0, 0, 255, 5.f, 5.f);

   if(IsBehindOf(TargetPos, TargetDir, OriginalPos) == false || IsBehindOf(TargetPos, TargetDir, PretendedNewPos) == false)
   {
    if( IsRightOf(TargetPos, TargetDir, OriginalPos) != IsRightOf(TargetPos, TargetDir, PretendedNewPos) )
    {
      bRet = true;
    }
   }

   return (bRet);
   }
 */
//===================================================================
// Check if path to new spot will cross the target fire dir
//===================================================================
/*static bool CheckVisibilityBetweenTwoPoints(const Vec3& Pos1, const Vec3& Pos2)
   {
   bool bRet = false;
   ray_hit hit;

   bRet = 0 == gEnv->pPhysicalWorld->RayWorldIntersection(Pos2, (Pos1 - Pos2), COVER_OBJECT_TYPES, AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER, &hit, 1);

   if( gAIEnv.CVars.DrawHideSpotSearchRays == 1 )
   {
    if(bRet == true)
      GetAISystem()->AddDebugLine(Pos1, Pos2, 0, 255, 0, 20.f);
    else
      GetAISystem()->AddDebugLine(Pos1, Pos2, 255, 0, 0, 20.f);
   }

   return (bRet);
   }
 */
//===================================================================
// Check if path to new spot will cross the target fire dir
//===================================================================
/*
   static bool	CanShootTargetFromThere(const Vec3& PretendedNewPos, const Vec3& TargetPos)
   {
   bool bRet = false;
   Vec3 vPos = PretendedNewPos;
   vPos.z += 1.f;

   float fLeanDist = 0.7f;
   Vec3 vDir = TargetPos - PretendedNewPos;
   vDir.Normalize();

   Matrix33 orientation = Matrix33::CreateRotationVDir( vDir, 0 );
   Vec3 vRight = orientation.TransformVector(Vec3(1,0,0));

   bRet = CheckVisibilityBetweenTwoPoints(vPos + (vRight * fLeanDist), TargetPos + (vRight * fLeanDist));

   if(bRet == false)
   {
    bRet =CheckVisibilityBetweenTwoPoints(vPos + (-vRight * fLeanDist), TargetPos + (-vRight * fLeanDist));
   }

   return (bRet);
   }
 */

//====================================================================
// GetAccuracy
//	calculate current accuracy - account for distance, target stance, ...
//	at close range always hit, at attackRange always miss
//	if accuracy in properties is 1 - ALWAYS hit
//	if accuracy in properties is 0 - ALWAYS miss
//====================================================================
float CPuppet::GetAccuracy(const CAIObject* pTarget) const
{

	//---------parameters------------------------------------------
	const float absoluteAccurateTrh(5.f); // if closer than this - always hit
	//	const float	accurateTrhSlope(.3f);		// slop from aways-hit to nominal accuracy
	const float nominalAccuracyStartAt(.3f);    // at what attack range nominal accuracy is on
	const float nominalAccuracyStopAt(.73f);    // at what attack range nominal accuracy is starting to fade to 0
	//-------------------------------------------------------------

	float unscaleAccuracy = GetParameters().m_fAccuracy;
	float curAccuracy = unscaleAccuracy;
	if (curAccuracy <= 0.00001f)
		return 0.f;

	if (!IsAllowedToHitTarget())
		curAccuracy *= 0.0f;

	if (!pTarget)
		return curAccuracy;

	const CAIActor* pTargetActor = pTarget->CastToCAIActor();

	//curAccuracy = max(0.f, curAccuracy);	// account for soft cover
	float distance((pTarget->GetPos() - GetPos()).len());

	if (distance < absoluteAccurateTrh)  // too close - always hit
		return 1.f;
	if (distance >= GetParameters().m_fAttackRange)  // too far - always miss
		return 0.f;
	// at what DISTANCE nominal accuracy is on
	float nominalAccuracyStartDistance(max(absoluteAccurateTrh + 1.f, GetParameters().m_fAttackRange * nominalAccuracyStartAt));
	// at what DISTANCE nominal accuracy is starting to fade to 0
	float nominalAccuracyStopDistance(min(GetParameters().m_fAttackRange - .1f, GetParameters().m_fAttackRange * nominalAccuracyStopAt));

	// scale down accuracy if target prones
	if (pTargetActor)
	{
		const SAIBodyInfo& targetBodyInfo = pTargetActor->GetBodyInfo();

		switch (targetBodyInfo.stance)
		{
		case STANCE_ALERTED:
		case STANCE_STAND:
			curAccuracy *= 1.0f;
			break;
		case STANCE_CROUCH:
			curAccuracy *= 0.8f;
			break;
		case STANCE_LOW_COVER:
			curAccuracy *= 0.65f;
			break;
		case STANCE_HIGH_COVER:
			curAccuracy *= 0.8f;
			break;
		case STANCE_PRONE:
			curAccuracy *= 0.3f;
			break;
		default:
			break;
		}
	}

	// scale down accuracy if shooter moves
	if (GetPhysics())
	{
		static pe_status_dynamics dSt;
		GetPhysics()->GetStatus(&dSt);
		float fSpeed = dSt.v.GetLength();
		if (fSpeed > 1.0f)
		{
			if (fSpeed > 5.0f)
				fSpeed = 5.0f;
			if (IsCoverFireEnabled())
				fSpeed /= 2.f;
			curAccuracy *= 3.f / (3.f + fSpeed);
		}
	}

	if (distance < nominalAccuracyStartDistance) // 1->nominal interpolation
	{
		float slop((1.f - curAccuracy) / (nominalAccuracyStartDistance - absoluteAccurateTrh));
		float scaledAccuracy(curAccuracy + slop * (nominalAccuracyStartDistance - (distance - absoluteAccurateTrh)));
		return scaledAccuracy;
	}
	if (distance > nominalAccuracyStopDistance)  // nominal->0 interpolation
	{
		float slop((curAccuracy) / (GetParameters().m_fAttackRange - nominalAccuracyStopDistance));
		float scaledAccuracy(slop * (GetParameters().m_fAttackRange - distance));
		return scaledAccuracy;
	}
	return curAccuracy;
}

//====================================================================
// GetFireTargetObject
//====================================================================
CAIObject* CPuppet::GetFireTargetObject() const
{
	CAIObject* targetObject = m_refFireTarget.GetAIObject();

	if (targetObject)
	{
		if (IEntity* targetEntity = targetObject->GetEntity())
		{
			if (CAIObject* targetEntityObject = static_cast<CAIObject*>(targetEntity->GetAI()))
				return targetEntityObject;
		}

		return targetObject;
	}

	targetObject = m_refAttentionTarget.GetAIObject();

	CCCPOINT(CPuppet_GetFireTargetObject);

	return targetObject;
}

//====================================================================
// RequestThrowGrenade
//====================================================================
void CPuppet::RequestThrowGrenade(ERequestedGrenadeType eGrenadeType, int iRegType)
{
	m_bGrenadeThrowRequested = true;
	m_eGrenadeThrowRequestType = eGrenadeType;
	m_iGrenadeThrowTargetType = iRegType;
}

//====================================================================
// FireCommand
//====================================================================
void CPuppet::FireCommand(float updateTime)
{
	CCCPOINT(CPuppet_FireCommand);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_timeSinceTriggerPressed += updateTime;

	if (m_fireMode == FIREMODE_OFF)
	{
		m_State.vAimTargetPos.zero();
		m_State.vShootTargetPos.zero();
		m_State.aimTargetIsValid = false;
		m_State.fire = eAIFS_Off;
		m_State.fireSecondary = eAIFS_Off;
		m_State.fireMelee = eAIFS_Off;

		m_aimState = AI_AIM_NONE;
		m_friendOnWayElapsedTime = 0.0f;

		ResetTargetTracking();

		m_targetBiasDirection *= 0.5f;
		m_burstEffectTime = 0.0f;
		m_burstEffectState = 0;

		return;
	}

	IF_UNLIKELY (m_fireMode == FIREMODE_VEHICLE)
		return;

	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
	if (pAttentionTarget && ((pAttentionTarget->GetType() == AIOBJECT_TARGET) || pAttentionTarget->IsAgent()))
		m_lastLiveTargetPos = pAttentionTarget->GetPos();

	bool lastAim = m_State.aimTargetIsValid;

	// Secondary fire if using a secondary fire mode or a grenade throw was requested
	const bool bIsSecondaryFire = IsSecondaryFireCommand() || m_bGrenadeThrowRequested;
	const bool bIsMeleeFire = IsMeleeFireCommand();

	// Reset aiming and shooting, they will be set in to code below if needed.
	m_State.aimTargetIsValid = false;
	m_State.fire = eAIFS_Off;

	// Reset if not doing a secondary fire anymore
	const bool wasSecondaryFire = (m_State.fireSecondary == eAIFS_On || m_State.fireSecondary == eAIFS_Blocking);
	if (wasSecondaryFire && !bIsSecondaryFire)
	{
		m_State.fireSecondary = eAIFS_Off;
		m_pFireCmdGrenade->Reset();
	}

	// Same for melee
	if (!bIsMeleeFire && eAIFS_On == m_State.fireMelee)
	{
		m_State.fireMelee = eAIFS_Off;
	}

	// Choose target to shoot at.
	CWeakRef<CAIObject> refChooseTarget = m_refFireTarget.IsValid()
	                                      ? m_refFireTarget
	                                      : m_refAttentionTarget;

	if (bIsSecondaryFire)
	{
		if (m_fireModeUpdated)
		{
			m_pFireCmdGrenade->Reset();
			m_fireModeUpdated = false;
		}

		ERequestedGrenadeType eReqType = m_eGrenadeThrowRequestType;
		if (m_bGrenadeThrowRequested)
		{
			// Get target
			switch (m_iGrenadeThrowTargetType)
			{
			case AI_REG_ATTENTIONTARGET:
				refChooseTarget = m_refAttentionTarget;
				break;

			case AI_REG_LASTOP:
				refChooseTarget = m_refLastOpResult;
				break;

			case AI_REG_REFPOINT:
				refChooseTarget = GetWeakRef(GetRefPoint());
				break;

			default:
				CRY_ASSERT_MESSAGE(0, "Unhandled reg type for requested grenade throw");
			}
		}
		else if (m_fireMode == FIREMODE_SECONDARY_SMOKE)
		{
			if (m_refLastOpResult.IsValid())
				refChooseTarget = m_refLastOpResult;
			eReqType = eRGT_SMOKE_GRENADE;
		}

		if (refChooseTarget.IsValid())
		{
			CAIObject* const pTarget = refChooseTarget.GetAIObject();
			m_State.vAimTargetPos = pTarget->GetPos();
			m_State.aimTargetIsValid = true;
			m_aimState = AI_AIM_READY;
			FireSecondary(pTarget, eReqType);
		}
		return;
	}

	// Query the correct weapon fire descriptor to use
	QueryCurrentWeaponDescriptor();

	if (bIsMeleeFire)
	{
		if (refChooseTarget.IsValid())
		{
			// Do not aim, use look only.
			CAIObject* const pTarget = refChooseTarget.GetAIObject();
			m_State.vShootTargetPos = pTarget->GetPos();
			m_State.vAimTargetPos = pTarget->GetPos();
			m_State.aimTargetIsValid = false;
			m_aimState = AI_AIM_NONE; // AI_AIM_READY;
			FireMelee(pTarget);
		}
		return;
	}

	if (!m_pFireCmdHandler)
		return;

	bool targetValid = false;
	Vec3 aimTarget(ZERO);
	CAIObject* const pTarget = refChooseTarget.GetAIObject();

	// Check if the target is valid based on the current fire mode.
	if (m_fireMode == FIREMODE_AIM || m_fireMode == FIREMODE_AIM_SWEEP ||
	    m_fireMode == FIREMODE_FORCED || m_fireMode == FIREMODE_PANIC_SPREAD)
	{
		// Aiming and forced fire has the loosest checking.
		// As long as the target exists, it can be used.
		if (pTarget)
			targetValid = true;
	}
	else
	{
		// The rest of the first modes require that the target exists and is an agent or a vehicle with player inside.
		// The IsActive() handles both cases.
		if (pTarget)
		{
			if (CAIActor* pTargetActor = pTarget->CastToCAIActor())
			{
				if (pTargetActor->IsActive())
					targetValid = true;
			}
			else if (pTarget->GetType() == AIOBJECT_TARGET)
			{
				targetValid = true;
			}
			else if (pTarget->GetType() == AIOBJECT_DUMMY)
			{
				targetValid = true;
			}
			else if (pTarget->GetSubType() == IAIObject::STP_MEMORY)
			{
				targetValid = true;
			}
			else if (pTarget->GetSubType() == IAIObject::STP_SOUND)
			{
				targetValid = true;
			}
		}
	}

	// The loose attention target is more important than the normal target.
	if (gAIEnv.configuration.eCompatibilityMode != ECCM_WARFACE)
		if (m_bLooseAttention && m_refLooseAttentionTarget != pTarget && GetSubType() != CAIObject::STP_2D_FLY)
			targetValid = false;

	float distToTargetSqr = FLT_MAX;

	bool canFire = targetValid && !m_fireDisabled && AllowedToFire();

	bool useLiveTargetForMemory = m_targetLostTime < m_CurrentWeaponDescriptor.coverFireTime;

	Vec3 aimTargetBeforeTargetTracking(ZERO);

	// As a default handling, aim at the target.
	// The fire command handler will alter the target position later,
	// but a sane default value is provided here for at least aiming.
	if (targetValid && !m_fireDisabled)
	{
		aimTarget = pTarget->GetPos();

		// When using laser, aim slightly lower so that it does not look bad when hte laser asset
		// is pointing directly at the player.
		const int enabledAccessories = GetParameters().m_weaponAccessories;
		if (enabledAccessories & AIWEPA_LASER)
		{
			// Make sure the laser is visible when aiming directly at the player
			aimTarget.z -= 0.05f;
		}

		if (GetSubType() != CAIObject::STP_2D_FLY)
		{
			float distSqr = Distance::Point_Point2DSq(aimTarget, GetFirePos());
			if (distSqr < square(2.0f))
			{
				Vec3 safePos = GetFirePos() + GetEntityDir() * 2.0f;
				if (distSqr < square(0.7f))
					aimTarget = safePos;
				else
				{
					float speed = m_State.vMoveDir.GetLength();
					speed = clamp_tpl(speed, 0.0f, 10.f);

					float d = sqrtf(distSqr);
					float u = 1.0f - (d - 0.7f) / (2.0f - 0.7f);
					aimTarget += speed / 10 * u * (safePos - aimTarget);
				}
			}
		}

		//GetAISystem()->AddDebugLine(GetPos(), aimTarget, 255,255,255, 0.15f);

		if (!m_pFireCmdHandler || m_pFireCmdHandler->UseDefaultEffectFor(m_fireMode))
		{
			switch (m_fireMode)
			{
			case FIREMODE_PANIC_SPREAD:
				HandleWeaponEffectPanicSpread(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_BURST_DRAWFIRE:
				HandleWeaponEffectBurstDrawFire(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_BURST_SNIPE:
				HandleWeaponEffectBurstSnipe(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_AIM_SWEEP:
				HandleWeaponEffectAimSweep(pTarget, aimTarget, canFire);
				break;
			}
			;
		}

		aimTargetBeforeTargetTracking = aimTarget;

		aimTarget = UpdateTargetTracking(GetWeakRef(pTarget), aimTarget);
	}
	else
	{
		ResetTargetTracking();

		m_targetBiasDirection *= 0.5f;
		m_burstEffectTime = 0.0f;
		m_burstEffectState = 0;

		// (Kevin) Warface still needs target tracking to be updated when the AI cannot fire their weapon
		if (gAIEnv.configuration.eCompatibilityMode == ECCM_WARFACE)
		{
			aimTarget = UpdateTargetTracking(GetWeakRef(pTarget), aimTarget);
		}
	}

	// Aim target interpolation.
	float turnSpeed = canFire ? m_Parameters.m_fireTurnSpeed : m_Parameters.m_aimTurnSpeed;

	m_State.vAimTargetPos = (turnSpeed <= 0.f)
	                        ? aimTarget
	                        : InterpolateLookOrAimTargetPos(m_State.vAimTargetPos, aimTarget, turnSpeed);

	// Check if the aim is obstructed and cancel aiming if it is.
	if (m_State.aimObstructed = (m_fireMode != FIREMODE_FORCED) && (m_fireMode != FIREMODE_OFF)
	                            && (m_State.vAimTargetPos.IsZero() || !CanAimWithoutObstruction(m_State.vAimTargetPos)))
	{
		m_State.aimTargetIsValid = targetValid = canFire = false;
	}

	/*	if (m_State.aimLook)
	   {
	   GetAISystem()->AddDebugLine(GetFirePos(), aimTarget, 255,196,0, 0.5f);
	   GetAISystem()->AddDebugLine(GetFirePos(), m_State.vAimTargetPos, 255,0,0, 0.5f);
	   GetAISystem()->AddDebugLine(m_State.vAimTargetPos, m_State.vAimTargetPos+Vec3(0,0,0.5f), 255,0,0, 0.5f);
	   }*/

	// Make the shoot target the same as the aim target by default.
	m_State.vShootTargetPos = m_State.vAimTargetPos;

	SAIWeaponInfo weaponInfo;
	GetProxy()->QueryWeaponInfo(weaponInfo);

	IEntity* pEntity = 0; // Call GetEntity only once [6/17/2010 evgeny]
	if (m_wasReloading && !weaponInfo.isReloading)
		SetSignal(1, "OnReloaded", (pEntity = GetEntity()), 0, gAIEnv.SignalCRCs.m_nOnReloaded);

	if (weaponInfo.outOfAmmo || weaponInfo.isReloading)
		canFire = false;

	if (weaponInfo.lowAmmo)
	{
		if (!m_lowAmmoSent)
		{
			SetSignal(1, "OnLowAmmo", (pEntity ? pEntity : GetEntity()), 0, gAIEnv.SignalCRCs.m_nOnLowAmmo);
			m_lowAmmoSent = true;
		}
	}
	else
	{
		m_lowAmmoSent = false;
	}

	if (!m_outOfAmmoSent)
	{
		if (weaponInfo.outOfAmmo)
		{
			SetSignal(1, "OnOutOfAmmo", (pEntity ? pEntity : GetEntity()), 0, gAIEnv.SignalCRCs.m_nOnOutOfAmmo);

			m_outOfAmmoSent = true;
			m_outOfAmmoTimeOut = 0.0f;
			m_burstEffectTime = 0.0f;
			m_burstEffectState = 0;

			if (m_pFireCmdHandler)
				m_pFireCmdHandler->OnReload();
		}
	}
	else
	{
		if (!weaponInfo.outOfAmmo)
			m_outOfAmmoSent = false;
		else if ((gAIEnv.configuration.eCompatibilityMode != ECCM_WARFACE) && !weaponInfo.isReloading)
		{
			m_outOfAmmoTimeOut += updateTime;
			if (m_outOfAmmoTimeOut > 3.0f)
				m_outOfAmmoSent = false;
		}
	}

	m_wasReloading = weaponInfo.isReloading;

	if (m_State.vAimTargetPos.IsZero())
	{
		m_State.aimTargetIsValid = false;
		m_aimState = m_State.aimObstructed ? AI_AIM_OBSTRUCTED : AI_AIM_NONE;
	}
	else
	{
		m_State.aimTargetIsValid = true;
		const SAIBodyInfo& bodyInfo = GetBodyInfo();

		m_aimState = bodyInfo.isAiming ? AI_AIM_READY : (lastAim ? AI_AIM_OBSTRUCTED : AI_AIM_WAITING);
	}

	// If just starting to fire, reset the fire handler.
	if (m_fireModeUpdated)
	{
		m_pFireCmdHandler->Reset();
		m_fireModeUpdated = false;
	}

	if (gAIEnv.CVars.DebugDrawFireCommand)
	{
		CDebugDrawContext dc;

		dc->DrawLine(GetFirePos(), Col_Grey, m_State.vAimTargetPos, Col_Grey, 1.0f);
	}

	m_State.fire = (pTarget ? m_pFireCmdHandler->Update(pTarget, canFire, m_fireMode, m_CurrentWeaponDescriptor, m_State.vAimTargetPos) : eAIFS_Off);

	if (gAIEnv.CVars.DebugDrawFireCommand)
	{
		CDebugDrawContext dc;

		ColorB color = m_State.fire ? Col_Grey : Col_Red;

		dc->DrawLine(GetFirePos(), color, m_State.vAimTargetPos, color, 1.0f);
		dc->DrawSphere(m_State.vAimTargetPos, 0.175f, color);
	}

	if (eAIFS_On == m_State.fire)
	{
		m_timeSinceTriggerPressed = 0.0f;
		m_State.vShootTargetPos = m_State.vAimTargetPos;

		m_State.projectileInfo.Reset();
		m_pFireCmdHandler->GetProjectileInfo(m_State.projectileInfo);
	}

	Vec3 overrideAimingPosition(ZERO);
	m_State.vAimTargetPos = m_pFireCmdHandler->GetOverrideAimingPosition(overrideAimingPosition) ?
	                        overrideAimingPosition : aimTargetBeforeTargetTracking;

	// Warn about conflicting fire state.
	if (eAIFS_On == m_State.fire && !canFire)
		AIWarning("CPuppet::FireCommand(): state.fire = true && canFire == false");

	// Update accessories.
	const int enabledAccessories = GetParameters().m_weaponAccessories;
	m_State.weaponAccessories = enabledAccessories & AIWEPA_LASER;

	if (IsAllowedToUseExpensiveAccessory())
	{
		if (enabledAccessories & AIWEPA_COMBAT_LIGHT)
		{
			if (GetAlertness() > 0)
				m_State.weaponAccessories |= AIWEPA_COMBAT_LIGHT;
		}
		if (enabledAccessories & AIWEPA_PATROL_LIGHT)
		{
			m_State.weaponAccessories |= AIWEPA_PATROL_LIGHT;
		}
	}
}

//===================================================================
// HandleWeaponEffectBurstDrawFire
//===================================================================
void CPuppet::HandleWeaponEffectBurstDrawFire(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	float drawFireTime = m_CurrentWeaponDescriptor.drawTime;
	if (m_CurrentWeaponDescriptor.fChargeTime > 0)
		drawFireTime += m_CurrentWeaponDescriptor.fChargeTime;

	const float minDist = 5.0f;

	if (Distance::Point_PointSq(aimTarget, GetFirePos()) > sqr(minDist))
	{
		if (m_burstEffectTime < drawFireTime)
		{
			Vec3 shooterGroundPos = GetPhysicsPos();
			Vec3 targetGroundPos;
			Vec3 targetPos = aimTarget;

			if (pTarget->GetProxy())
			{
				targetGroundPos = pTarget->GetPhysicsPos();
				targetGroundPos.z -= 0.25f;
			}
			else
			{
				// Assume about human height target.
				targetGroundPos = targetPos;
				targetGroundPos.z -= 1.5f;
			}

			float floorHeight = min(targetGroundPos.z, shooterGroundPos.z);

			/*			GetAISystem()->AddDebugLine(Vec3(shooterGroundPos.x, shooterGroundPos.y, floorHeight),
			      Vec3(targetGroundPos.x, targetGroundPos.y, floorHeight), 128,128,128, 0.5f);
			      Vec3 mid = (shooterGroundPos+targetGroundPos)/2;
			      GetAISystem()->AddDebugLine(Vec3(mid.x, mid.y, floorHeight-1),
			      Vec3(mid.x, mid.y, floorHeight+1), 255,128,128, 0.5f);
			 */
			Vec3 dirTargetToShooter = shooterGroundPos - targetGroundPos;
			dirTargetToShooter.z = 0.0f;
			float dist = dirTargetToShooter.NormalizeSafe();

			const Vec3& firePos = GetFirePos();

			float endHeight = targetGroundPos.z;
			float startHeight = floorHeight - (firePos.z - floorHeight);

			float t;
			if (m_CurrentWeaponDescriptor.fChargeTime > 0)
				t = clamp_tpl((m_burstEffectTime - m_CurrentWeaponDescriptor.fChargeTime) / (drawFireTime - m_CurrentWeaponDescriptor.fChargeTime), 0.0f, 1.0f);
			else
				t = clamp_tpl(m_burstEffectTime / drawFireTime, 0.0f, 1.0f);

			CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();
			float noiseScale = clamp_tpl((m_burstEffectTime - 0.5f), 0.0f, 1.0f);
			float noise = noiseScale * pNoise->Noise1D(m_spreadFireTime + m_burstEffectTime * m_CurrentWeaponDescriptor.sweepFrequency);
			Vec3 right(dirTargetToShooter.y, -dirTargetToShooter.x, 0);

			aimTarget = targetGroundPos + right * (noise * m_CurrentWeaponDescriptor.sweepWidth);
			aimTarget.z = startHeight + (endHeight - startHeight) * (1 - sqr(1 - t));

			// Clamp to bottom plane.
			if (aimTarget.z < floorHeight && fabsf(aimTarget.z - firePos.z) > 0.01f)
			{
				float u = (floorHeight - firePos.z) / (aimTarget.z - firePos.z);
				aimTarget = firePos + (aimTarget - firePos) * u;
			}
			/*
			      GetAISystem()->AddDebugLine(firePos, targetGroundPos, 255,255,255, 0.5f);
			      GetAISystem()->AddDebugLine(firePos, m_State.vAimTargetPos, 255,255,255, 0.5f);
			      GetAISystem()->AddDebugLine(firePos, aimTarget, 255,255,255, 0.5f);
			      GetAISystem()->AddDebugLine(aimTarget, aimTarget+Vec3(0,0,0.5f), 255,255,255, 0.5f);
			 */
		}
		else if (m_targetLostTime > m_CurrentWeaponDescriptor.drawTime)
		{
			float amount = clamp_tpl((m_targetLostTime - m_CurrentWeaponDescriptor.drawTime) / m_CurrentWeaponDescriptor.drawTime, 0.0f, 1.0f);

			Vec3 forw = GetEntityDir();
			Vec3 right(forw.y, -forw.x, 0);
			Vec3 up(0, 0, 1);
			right.NormalizeSafe();
			float distToTarget = Distance::Point_Point(aimTarget, GetPos());

			float t = m_spreadFireTime + m_burstEffectTime * m_CurrentWeaponDescriptor.sweepFrequency;
			float mag = amount * m_CurrentWeaponDescriptor.sweepWidth / 2;

			CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

			float ox = sinf(t * 1.5f) * mag + pNoise->Noise1D(t) * mag;
			float oy = pNoise->Noise1D(t + 33.0f) / 2 * mag;

			aimTarget += ox * right + oy * up;
		}

		if (m_burstEffectTime < 0.2f)
		{
			if (!m_State.vAimTargetPos.IsZero())
			{
				const Vec3& pos = GetPos();
				// When starting to fire, make sure the aim target as fully contracted.
				//						if (Distance::Point_PointSq(aimTarget, m_State.vAimTargetPos) > sqr(0.5f))

				if (m_State.vAimTargetPos.z > (aimTarget.z + 0.25f))
				{
					// Current aim target still to high.
					canFire = false;
				}
				else
				{
					// Check for distance.
					const float distToCurSq = Distance::Point_Point2DSq(pos, m_State.vAimTargetPos);
					const float thr = sqr(Distance::Point_Point2D(pos, aimTarget) + 0.5f);
					if (distToCurSq > thr)
						canFire = false;
				}
			}
		}
		if (canFire) // && m_aimState != AI_AIM_OBSTRUCTED)
			m_burstEffectTime += m_fTimePassed;
	}
}

//===================================================================
// HandleWeaponEffectBurstSnipe
//===================================================================
void CPuppet::HandleWeaponEffectBurstSnipe(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	CCCPOINT(CPuppet_HandleWeaponEffectBurstSnipe);

	float drawFireTime = m_CurrentWeaponDescriptor.drawTime;
	if (m_CurrentWeaponDescriptor.fChargeTime > 0)
		drawFireTime += m_CurrentWeaponDescriptor.fChargeTime;

	// Make it look like the sniper is aiming for headshots.
	float headHeight = aimTarget.z;
	const CAIActor* pLiveTarget = GetLiveTarget(GetWeakRef(pTarget)).GetAIObject();
	if (pLiveTarget && pLiveTarget->GetProxy())
	{
		IPhysicalEntity* pPhys = pLiveTarget->GetProxy()->GetPhysics(true);
		if (!pPhys)
			pPhys = pLiveTarget->GetPhysics();
		if (pPhys)
		{
			pe_status_pos statusPos;
			pPhys->GetStatus(&statusPos);
			float minz = statusPos.BBox[0].z + statusPos.pos.z;
			float maxz = statusPos.BBox[1].z + statusPos.pos.z + 0.25f;

			// Rough sanity check.
			if (headHeight >= minz && headHeight <= maxz)
				headHeight = maxz;
		}
	}

	//	GetAISystem()->AddDebugLine(Vec3(aimTarget.x-1, aimTarget.y, headHeight),
	//		Vec3(aimTarget.x+1, aimTarget.y, headHeight), 255,255,255, 0.2f);

	const Vec3& firePos = GetFirePos();
	Vec3 dirTargetToShooter = aimTarget - firePos;
	dirTargetToShooter.z = 0.0f;
	float dist = dirTargetToShooter.NormalizeSafe();
	Vec3 right(dirTargetToShooter.y, -dirTargetToShooter.x, 0);
	Vec3 up(0, 0, 1);
	float noiseScale = 1.0f;

	if (m_burstEffectState == 0)
	{
		if (canFire && m_aimState != AI_AIM_OBSTRUCTED)
			m_burstEffectTime += m_fTimePassed;

		// Aim towards the head position.
		if (m_burstEffectTime < drawFireTime)
		{
			Vec3 shooterGroundPos = GetPhysicsPos();
			Vec3 targetGroundPos;

			//					float floorHeight = min(targetGroundPos.z, shooterGroundPos.z);

			//					GetAISystem()->AddDebugLine(Vec3(shooterGroundPos.x, shooterGroundPos.y, floorHeight),
			//					Vec3(targetGroundPos.x, targetGroundPos.y, floorHeight), 128,128,128, 0.5f);
			//					Vec3 mid = (shooterGroundPos+targetGroundPos)/2;
			//					GetAISystem()->AddDebugLine(Vec3(mid.x, mid.y, floorHeight-1),
			//					Vec3(mid.x, mid.y, floorHeight+1), 255,128,128, 0.5f);

			float endHeight = aimTarget.z;          //targetGroundPos.z;
			float startHeight = aimTarget.z - 0.5f; //floorHeight - (firePos.z - floorHeight);

			float t;
			if (m_CurrentWeaponDescriptor.fChargeTime > 0)
				t = clamp_tpl((m_burstEffectTime - m_CurrentWeaponDescriptor.fChargeTime) / (drawFireTime - m_CurrentWeaponDescriptor.fChargeTime), 0.0f, 1.0f);
			else
				t = clamp_tpl(m_burstEffectTime / drawFireTime, 0.0f, 1.0f);

			noiseScale = t;

			//					aimTarget = targetGroundPos + right * (noise * m_CurrentWeaponDescriptor.sweepWidth);
			//			aimTarget = aimTarget + ox*right + oy*up;
			aimTarget.z = startHeight + (endHeight - startHeight) * t;

			// Clamp to bottom plane.
			//					if (aimTarget.z < floorHeight && fabsf(aimTarget.z - firePos.z) > 0.01f)
			//					{
			//						float u = (floorHeight - firePos.z) / (aimTarget.z - firePos.z);
			//						aimTarget = firePos + (aimTarget - firePos) * u;
			//					}

			//						GetAISystem()->AddDebugLine(firePos, targetGroundPos, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(firePos, m_State.vAimTargetPos, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(firePos, aimTarget, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(aimTarget, aimTarget+Vec3(0,0,0.5f), 255,255,255, 0.5f);
		}
		else
		{
			m_burstEffectState = 1;
			m_burstEffectTime = -1;
		}
	}
	else if (m_burstEffectState == 1)
	{
		if (m_targetLostTime > m_CurrentWeaponDescriptor.drawTime)
		{
			if (m_burstEffectTime < 0)
				m_burstEffectTime = 0;

			if (canFire && m_aimState != AI_AIM_OBSTRUCTED)
				m_burstEffectTime += m_fTimePassed;

			// Target getting lost, aim above the head.
			float amount = clamp_tpl((m_targetLostTime - m_CurrentWeaponDescriptor.drawTime) / m_CurrentWeaponDescriptor.drawTime, 0.0f, 1.0f);

			Vec3 forw = GetEntityDir();
			Vec3 rightVector(forw.y, -forw.x, 0);
			Vec3 upVector(0, 0, 1);
			rightVector.NormalizeSafe();
			float distToTarget = Distance::Point_Point(aimTarget, GetFirePos());

			//			float t = m_spreadFireTime + m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;
			float mag = amount * m_CurrentWeaponDescriptor.sweepWidth / 2 * clamp_tpl((distToTarget - 1.0f) / 5.0f, 0.0f, 1.0f);

			CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

			float tsweep = m_burstEffectTime * m_CurrentWeaponDescriptor.sweepFrequency;

			float ox = sinf(tsweep) * mag; // + pNoise->Noise1D(t) * mag;
			float oy = 0;                  //pNoise->Noise1D(t + 33.0f)/4 * mag;

			aimTarget.z = aimTarget.z + (headHeight - aimTarget.z) * clamp_tpl(m_burstEffectTime, 0.0f, 1.0f);
			aimTarget += ox * rightVector + oy * upVector;
		}
	}

	float noiseTime = m_spreadFireTime;
	m_spreadFireTime += m_fTimePassed;

	noiseTime *= m_CurrentWeaponDescriptor.sweepFrequency * 2;
	CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();
	float nx = pNoise->Noise1D(noiseTime) * noiseScale * 0.1f;
	float ny = pNoise->Noise1D(noiseTime + 33.0f) * noiseScale * 0.1f;
	aimTarget += right * nx + up * ny;

	//	GetAISystem()->AddDebugLine(aimTarget, firePos, 255,0,0, 10.0f);

}

//===================================================================
// HandleWeaponEffectAimSweep
//===================================================================
void CPuppet::HandleWeaponEffectAimSweep(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	float drawFireTime = m_CurrentWeaponDescriptor.drawTime;
	if (m_CurrentWeaponDescriptor.fChargeTime > 0)
		drawFireTime += m_CurrentWeaponDescriptor.fChargeTime;

	// Make it look like the sniper is aiming for headshots.
	float headHeight = aimTarget.z;
	const CAIActor* pLiveTarget = GetLiveTarget(GetWeakRef(pTarget)).GetAIObject();
	if (pLiveTarget && pLiveTarget->GetProxy())
	{
		IPhysicalEntity* pPhys = pLiveTarget->GetProxy()->GetPhysics(true);
		if (!pPhys)
			pPhys = pLiveTarget->GetPhysics();
		if (pPhys)
		{
			pe_status_pos statusPos;
			pPhys->GetStatus(&statusPos);
			float minz = statusPos.BBox[0].z + statusPos.pos.z;
			float maxz = statusPos.BBox[1].z + statusPos.pos.z + 0.25f;

			// Rough sanity check.
			if (headHeight >= minz && headHeight <= maxz)
				headHeight = maxz;
		}
	}

	if (m_burstEffectTime < 0)
		m_burstEffectTime = 0;

	if (m_aimState != AI_AIM_OBSTRUCTED)
		m_burstEffectTime += m_fTimePassed;

	// Target getting lost, aim above the head.

	Vec3 forw = GetEntityDir();
	Vec3 right(forw.y, -forw.x, 0);
	Vec3 up(0, 0, 1);
	right.NormalizeSafe();

	//	float t = m_spreadFireTime + m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;
	float mag = m_CurrentWeaponDescriptor.sweepWidth / 2;

	CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

	float distToTarget = Distance::Point_Point(aimTarget, GetFirePos());
	float dscale = clamp_tpl((distToTarget - 1.0f) / 5.0f, 0.0f, 1.0f);

	float tsweep = m_burstEffectTime * m_CurrentWeaponDescriptor.sweepFrequency;

	float ox = sinf(tsweep) * mag * dscale; // + pNoise->Noise1D(t) * mag;
	float oy = 0;                           //pNoise->Noise1D(t + 33.0f)/4 * mag;

	aimTarget.z = aimTarget.z + (headHeight - aimTarget.z) * clamp_tpl(m_burstEffectTime / 2, 0.0f, 1.0f);
	aimTarget += ox * right + oy * up;

	float noiseTime = m_spreadFireTime;
	m_spreadFireTime += m_fTimePassed;

	float noiseScale = clamp_tpl(m_burstEffectTime, 0.0f, 1.0f) * dscale;

	noiseTime *= m_CurrentWeaponDescriptor.sweepFrequency * 2;
	float nx = pNoise->Noise1D(noiseTime) * noiseScale * 0.1f;
	float ny = pNoise->Noise1D(noiseTime + 33.0f) * noiseScale * 0.1f;
	aimTarget += right * nx + up * ny;
}

//===================================================================
// HandleWeaponEffectPanicSpread
//===================================================================
void CPuppet::HandleWeaponEffectPanicSpread(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	// Don't start moving until the aim is ready.
	if (m_aimState == AI_AIM_READY)
	{
		m_burstEffectTime += GetAISystem()->GetFrameDeltaTime();
		m_spreadFireTime += GetAISystem()->GetFrameDeltaTime();
	}

	// Calculate aside-to-side wiggly motion when requesting the spread fire.
	float t = m_spreadFireTime;

	Vec3 forw = GetEntityDir();
	Vec3 right(forw.y, -forw.x, 0);
	Vec3 up(0, 0, 1);
	right.NormalizeSafe();
	float distToTarget = Distance::Point_Point(aimTarget, GetPos());

	float speed = 1.7f;
	float spread = DEG2RAD(40.0f);

	t *= speed;
	float mag = distToTarget * tanf(spread / 2);
	mag *= 0.25f + min(m_burstEffectTime / 0.5f, 1.0f) * 0.75f; // Scale the effect down when starting.

	CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

	float ox = sinf(t * 1.7f) * mag + pNoise->Noise1D(t) * mag;
	float oy = pNoise->Noise1D(t * 0.98432f + 33.0f) * mag;

	aimTarget += ox * right + oy * up;
}

//===================================================================
// FireSecondary
//===================================================================
void CPuppet::FireSecondary(CAIObject* pTarget, ERequestedGrenadeType prefGrenadeType)
{
	// Fire mode is set to off if we cannot do a secondary fire via fire mode

	const bool bIsSecondaryFireCommand = IsSecondaryFireCommand();

	if (!pTarget || !m_pFireCmdGrenade)
	{
		if (bIsSecondaryFireCommand)
			SetFireMode(FIREMODE_OFF);

		return;
	}

	// Set up the correct weapon descriptor to use
	QueryCurrentWeaponDescriptor(true, prefGrenadeType);

	Vec3 targetPos = pTarget->GetPos();

	m_State.requestedGrenadeType = prefGrenadeType;
	m_State.fireSecondary = m_pFireCmdGrenade->Update(pTarget, true, m_fireMode, m_CurrentWeaponDescriptor, targetPos);
	m_State.vShootTargetPos = targetPos;

	m_State.projectileInfo.Reset();
	m_pFireCmdGrenade->GetProjectileInfo(m_State.projectileInfo);

	//	if (m_State.fireSecondary && bIsSecondaryFireCommand)
	//		SetFireMode(FIREMODE_OFF);

	if (eAIFS_On == m_State.fireSecondary && m_State.vShootTargetPos.IsZero())
	{
		// Attempt to throw grenade failed
		m_pFireCmdGrenade->Reset();
		m_bGrenadeThrowRequested = false;
		m_State.fireSecondary = eAIFS_Off;
	}
}

//===================================================================
// FireMelee
//===================================================================
void CPuppet::FireMelee(CAIObject* pTarget)
{
	// Check may change Crysis behavior
	m_State.fireMelee = eAIFS_Off;

	// Wait for mercy time to expire
	if (!CanDamageTargetWithMelee())
		return;

	// Wait until at close range and until facing approx the right direction.
	if (m_fireMode != FIREMODE_MELEE_FORCED && (Distance::Point_PointSq(pTarget->GetPos(), GetPos()) > sqr(m_Parameters.m_fMeleeHitRange)))
		return;

	Vec3 dirToTarget = pTarget->GetPos() - GetPos();
	dirToTarget.z = 0.0f;
	dirToTarget.Normalize();

	float dotProduct = dirToTarget.Dot(GetBodyInfo().vAnimBodyDir);
	if (m_fireMode != FIREMODE_MELEE_FORCED && dotProduct < m_Parameters.m_fMeleeAngleCosineThreshold)
		return;

	if (GetAttentionTargetType() == AITARGET_MEMORY)
		return;

	if (!m_State.continuousMotion)
	{
		// [8/16/2010 evgeny] Some animations (e.g. stopping) may not be interrupted or blended,
		// which would result in inconsistencies between melee animation (delayed) and actual hit applied (too early).
		if (GetVelocity().GetLengthSquared() > 0.0001f)
			return;
	}

	// Execute the melee.
	m_State.fireMelee = eAIFS_On;

	SetSignal(AISIGNAL_DEFAULT, "OnMeleeExecuted", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnMeleeExecuted);
}

//===================================================================
// Compromising
// Evaluates whether the chosen navigation point will expose us too much to the target
//===================================================================
bool CPuppet::Compromising(const Vec3& pos, const Vec3& dir, const Vec3& hideFrom, const Vec3& objectPos, const Vec3& searchPos, bool bIndoor, bool bCheckVisibility) const
{
	if (!bIndoor)
	{
		// allow him to use only the hidespots closer to him than to the enemy
		Vec3 dirHideToEnemy = hideFrom - pos;
		Vec3 dirHide = pos - searchPos;
		if (dirHide.GetLengthSquared() > dirHideToEnemy.GetLengthSquared())
			return true;
	}
	// finally: check is the enemy visible from there
	if (bCheckVisibility && GetAISystem()->CheckPointsVisibility(pos, hideFrom, 5.0f))
		return true;

	return false;
}

//===================================================================
// SetParameters
//===================================================================
void CPuppet::SetParameters(const AgentParameters& sParams)
{
	AILogComment("CPuppet::SetParameters %s (%p)", GetName(), this);

	CAIActor::SetParameters(sParams);
}

//===================================================================
// CheckAwarenessPlayer
//===================================================================
void CPuppet::CheckAwarenessPlayer()
{
	CAIObject* pPlayer = GetAISystem()->GetPlayer();

	if (!pPlayer)
		return;

	Vec3 lookDir = pPlayer->GetViewDir();
	Vec3 relPos = (GetPos() - pPlayer->GetPos());
	float dist = relPos.GetLength();
	if (dist > 0)
		relPos /= dist;

	float fdot;
	float threshold;
	bool bCheckPlayerLooking;
	if (dist <= 1.2f)
	{
		bCheckPlayerLooking = false;
		fdot = GetMoveDir().Dot(-relPos);
		threshold = 0;
	}
	else
	{
		bCheckPlayerLooking = true;
		fdot = lookDir.Dot(relPos);
		threshold = cosf(atanf(0.5f / dist)); // simulates a circle of 0.5m radius centered on AIObject,
	}                                       //checks if the player is looking inside it

	if (fdot > threshold && eFOV_Outside != IsObjectInFOV(pPlayer))
	{
		if (m_fLastTimeAwareOfPlayer == 0 && bCheckPlayerLooking)
		{
			IEntity* pEntity = NULL;
			IPhysicalEntity* pPlayerPhE = (pPlayer->GetProxy() ? pPlayer->GetPhysics() : NULL);

			if (RayCastResult result = gAIEnv.pRayCaster->Cast(
			      RayCastRequest(
			        pPlayer->GetPos(), lookDir * (dist + 0.5f),
			        COVER_OBJECT_TYPES + ent_living,
			        AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
			        &pPlayerPhE, pPlayerPhE ? 1 : 0)))
			{
				IPhysicalEntity* pCollider = result[0].pCollider;
				pEntity = (IEntity*) pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
			}

			if (!(pEntity && pEntity == GetEntity()))
			{
				//m_fLastTimePlayerLooking = 0;
				return;
			}
		}
		float fCurrentTime = GetAISystem()->GetFrameStartTimeSeconds();
		if (m_fLastTimeAwareOfPlayer == 0)
			m_fLastTimeAwareOfPlayer = fCurrentTime;
		else if (fCurrentTime - m_fLastTimeAwareOfPlayer >= GetParameters().m_fAwarenessOfPlayer)
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			if (pData)
				pData->fValue = dist;
			m_playerAwarenessType = dist <= GetParameters().m_fMeleeRange ? PA_STICKING : PA_LOOKING;
			IEntity* pUserEntity = GetEntity();
			IEntity* pObjectEntity = pPlayer->GetEntity();
			gAIEnv.pSmartObjectManager->SmartObjectEvent(bCheckPlayerLooking ? "OnPlayerLooking" : "OnPlayerSticking", pUserEntity, pObjectEntity);
			SetSignal(1, bCheckPlayerLooking ? "OnPlayerLooking" : "OnPlayerSticking", pObjectEntity, pData, bCheckPlayerLooking ? gAIEnv.SignalCRCs.m_nOnPlayerLooking : gAIEnv.SignalCRCs.m_nOnPlayerSticking);
			m_fLastTimeAwareOfPlayer = fCurrentTime;
		}
	}
	else
	{
		IEntity* pUserEntity = GetEntity();
		IEntity* pObjectEntity = pPlayer->GetEntity();
		if (m_playerAwarenessType == PA_LOOKING)
		{
			SetSignal(1, "OnPlayerLookingAway", 0, 0, gAIEnv.SignalCRCs.m_nOnPlayerLookingAway);
			gAIEnv.pSmartObjectManager->SmartObjectEvent("OnPlayerLookingAway", pUserEntity, pObjectEntity);
		}
		else if (m_playerAwarenessType == PA_STICKING)
		{
			gAIEnv.pSmartObjectManager->SmartObjectEvent("OnPlayerGoingAway", pUserEntity, pObjectEntity);
			SetSignal(1, "OnPlayerGoingAway", 0, 0, gAIEnv.SignalCRCs.m_nOnPlayerGoingAway);
		}
		m_fLastTimeAwareOfPlayer = 0;
		m_playerAwarenessType = PA_NONE;
	}
}

//===================================================================
// Serialize
//===================================================================
void CPuppet::Serialize(TSerialize ser)
{
	//	if(ser.IsReading())
	//		Reset();  // Luc: already reset by AI System

	ser.BeginGroup("AIPuppet");
	{
		CPipeUser::Serialize(ser);

		ser.Value("m_bCanReceiveSignals", m_bCanReceiveSignals);

		ser.Value("m_targetApproach", m_targetApproach);
		ser.Value("m_targetFlee", m_targetFlee);
		ser.Value("m_targetApproaching", m_targetApproaching);
		ser.Value("m_targetFleeing", m_targetFleeing);
		ser.Value("m_lastTargetValid", m_lastTargetValid);
		ser.Value("m_lastTargetPos", m_lastTargetPos);
		ser.Value("m_lastTargetSpeed", m_lastTargetSpeed);
		ser.Value("m_fLastUpdateTime", m_fLastUpdateTime);

		ser.Value("m_allowedToHitTarget", m_allowedToHitTarget);
		ser.Value("m_bCoverFireEnabled", m_bCoverFireEnabled);
		ser.Value("m_firingReactionTimePassed", m_firingReactionTimePassed);
		ser.Value("m_firingReactionTime", m_firingReactionTime);
		ser.Value("m_outOfAmmoTimeOut", m_outOfAmmoTimeOut);
		ser.Value("m_allowedToUseExpensiveAccessory", m_allowedToUseExpensiveAccessory);

		ser.Value("m_adaptiveUrgencyMin", m_adaptiveUrgencyMin);
		ser.Value("m_adaptiveUrgencyMax", m_adaptiveUrgencyMax);
		ser.Value("m_adaptiveUrgencyScaleDownPathLen", m_adaptiveUrgencyScaleDownPathLen);
		ser.Value("m_adaptiveUrgencyMaxPathLen", m_adaptiveUrgencyMaxPathLen);
		ser.Value("m_chaseSpeed", m_chaseSpeed);
		ser.Value("m_lastChaseUrgencyDist", m_lastChaseUrgencyDist);
		ser.Value("m_lastChaseUrgencySpeed", m_lastChaseUrgencySpeed);
		ser.Value("m_chaseSpeedRate", m_chaseSpeedRate);
		ser.Value("m_delayedStance", m_delayedStance);
		ser.Value("m_delayedStanceMovementCounter", m_delayedStanceMovementCounter);

		ser.Value("m_bGrenadeThrowRequested", m_bGrenadeThrowRequested);
		ser.EnumValue("m_eGrenadeThrowRequestType", m_eGrenadeThrowRequestType, eRGT_INVALID, eRGT_COUNT);
		ser.Value("m_iGrenadeThrowTargetType", m_iGrenadeThrowTargetType);

		ser.Value("m_allowedStrafeDistanceStart", m_allowedStrafeDistanceStart);
		ser.Value("m_allowedStrafeDistanceEnd", m_allowedStrafeDistanceEnd);
		ser.Value("m_allowStrafeLookWhileMoving", m_allowStrafeLookWhileMoving);
		ser.Value("m_strafeStartDistance", m_strafeStartDistance);

		// (MATT) Yet another custom serialiser - must all be properly structured later {2009/03/23}
		SerializeWeakRefMap(ser, "devaluedPoints", m_mapDevaluedPoints);

		ser.Value("m_playerAwarenessType", *alias_cast<int*>(&m_playerAwarenessType));
		ser.Value("m_fLastTimeAwareOfPlayer", m_fLastTimeAwareOfPlayer);
		ser.Value("m_vForcedNavigation", m_vForcedNavigation);
		ser.Value("m_fForcedNavigationSpeed", m_fForcedNavigationSpeed);

		ser.Value("m_vehicleStickTarget", m_vehicleStickTarget);

		ser.Value("m_alarmedTime", m_alarmedTime);

		ser.BeginGroup("InitialPath");
		{
			int pointCount = m_InitialPath.size();
			ser.Value("pointCount", pointCount);
			Vec3 point;
			char name[16];
			if (ser.IsReading())
			{
				m_InitialPath.clear();

				for (int i = 0; i < pointCount; i++)
				{
					cry_sprintf(name, "Point_%d", i);
					ser.Value(name, point);
					m_InitialPath.push_back(point);
				}
			}
			else
			{
				TPointList::iterator itend = m_InitialPath.end();
				int i = 0;
				for (TPointList::iterator it = m_InitialPath.begin(); it != itend; ++it, i++)
				{
					cry_sprintf(name, "Point_%d", i);
					point = *it;
					ser.Value(name, point);
				}
			}
		}
		ser.EndGroup();

		ser.Value("m_vehicleAvoidingTime", m_vehicleAvoidingTime);
		m_refAvoidedVehicle.Serialize(ser, "M_refAvoidedVehicle");

		// Target tracking
		ser.BeginGroup("TargetTracking");
		{
			// The target tracking is update frequently, so just reset them here.
			if (ser.IsReading())
			{
				m_lastMissShotsCount = m_lastHitShotsCount = m_lastTargetPart = ~0l;

				m_targetSilhouette.Reset();
				m_targetLastMissPoint.zero();
				m_targetFocus = 0.0f;
				m_targetZone = AIZONE_OUT;
				m_targetDistanceToSilhouette = FLT_MAX;
				m_targetBiasDirection.Set(0, 0, -1);
				m_targetEscapeLastMiss = 0.0f;
				m_burstEffectTime = 0.0f;
				m_burstEffectState = 0;
			}

			// Serialize the more important values.
			ser.Value("m_targetDamageHealthThr", m_targetDamageHealthThr);
			ser.Value("m_targetSeenTime", m_targetSeenTime);
			ser.Value("m_targetLostTime", m_targetLostTime);
			ser.Value("m_targetDazzlingTime", m_targetDazzlingTime);
		}
		ser.EndGroup();
	}
	ser.EndGroup();

	if (ser.IsReading())
	{
		GetAISystem()->NotifyEnableState(this, m_bEnabled);
		m_steeringOccupancy.Reset(Vec3(0, 0, 0), Vec3(0, 1, 0), 1.0f);
		m_steeringOccupancyBias = 0;
		m_steeringAdjustTime = 0;

		m_currentWeaponId = 0;
		m_CurrentWeaponDescriptor = AIWeaponDescriptor();
	}

	if (ser.IsReading())
	{
		const VisionID& visionId = GetVisionID();
		if (visionId != 0)
		{
			// TODO(Jonas) Make it beautiful
			IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

			if (IsObserver())
			{
				ObserverParams observerParams;
				observerParams.typeMask = General | AliveAgent;
				visionMap.ObserverChanged(visionId, observerParams, eChangedTypeMask);
			}

			if (IsObservable())
			{
				ObservableParams observableParams;
				observableParams.typeMask = General | AliveAgent;
				observableParams.userData = (void*)(EXPAND_PTR)GetEntityID();
				visionMap.ObservableChanged(visionId, observableParams, eChangedTypeMask | eChangedUserData);
			}
		}
	}
}

//===================================================================
// PostSerialize
//===================================================================
void CPuppet::PostSerialize()
{
	CPipeUser::PostSerialize();

	if (m_pPerceptionHandler)
	{
		m_pPerceptionHandler->PostSerialize();
	}

	// Query the correct weapon fire descriptor to use
	QueryCurrentWeaponDescriptor();
}

//===================================================================
// AddEvent
//===================================================================
SAIPotentialTarget* CPuppet::AddEvent(CWeakRef<CAIObject> refObject, SAIPotentialTarget& ed)
{
	return (m_pPerceptionHandler ? m_pPerceptionHandler->AddEvent(refObject, ed) : NULL);
}

//===================================================================
// MakeMeLeader
//===================================================================
IAIObject* CPuppet::MakeMeLeader()
{
	if (GetGroupId() == -1 || !GetGroupId())
	{
		AIWarning("CPuppet::MakeMeLeader: Invalid GroupID ... %d", GetGroupId());
		return NULL;
	}

	CLeader* pLeader = (CLeader*) GetAISystem()->GetLeader(GetGroupId());
	if (pLeader)
	{
		CWeakRef<CAIObject> refObject = pLeader->GetAssociation();
		if (refObject != this)
			pLeader->SetAssociation(GetWeakRef(this));
	}
	else
	{
		pLeader = (CLeader*) gAIEnv.pAIObjectManager->CreateAIObject(AIObjectParams(AIOBJECT_LEADER, this));

		CCCPOINT(CPuppet_MakeMeLeader);
	}
	return pLeader;
}

void CPuppet::ResetPerception()
{
	CPipeUser::ResetPerception();

	if (m_pPerceptionHandler)
	{
		m_pPerceptionHandler->ClearPotentialTargets();
		m_pPerceptionHandler->ClearTempTarget();
	}

	SetAttentionTarget(NILREF);
}

//===================================================================
// AdjustSpeed
//===================================================================
void CPuppet::AdjustSpeed(CAIObject* pNavTarget, float distance)
{
	if (!pNavTarget)
		return;

	if ((GetType() == AIOBJECT_VEHICLE) || IsUsing3DNavigation())
	{
		// Danny/Kirill TODO: make some smart AdjastSpeed for vehicles chasing, currently just force maximum speed for vehicles always
		//	  m_State.fDesiredSpeed = 1.0f;
		return;
	}

	CCCPOINT(CPuppet_AdjustSpeed);

	// puppet speed control
	CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
	float timeStep = (fCurrentTime - m_SpeedControl.fLastTime).GetSeconds();

	// evaluate the potential target speed
	float targetSpeed = 0.0f;
	Vec3 targetVel(ZERO);
	Vec3 targetPos = pNavTarget->GetPos();
	IPhysicalEntity* pPhysProxy(NULL);
	if (pNavTarget->GetProxy() && pNavTarget->GetPhysics())
	{
		pPhysProxy = pNavTarget->GetPhysics();
	}
	else if (pNavTarget->GetSubType() == CAIObject::STP_FORMATION)
	{
		CAIObject* pOwner = pNavTarget->GetAssociation().GetAIObject();
		if (pOwner && pOwner->GetProxy() && pOwner->GetPhysics())
			pPhysProxy = pOwner->GetPhysics();
	}

	if (pPhysProxy)
	{
		pe_status_dynamics status;
		pPhysProxy->GetStatus(&status);
		targetVel = status.v;
	}
	else if (fCurrentTime > m_SpeedControl.fLastTime)
	{
		targetVel = (m_SpeedControl.vLastPos - GetPos()) / timeStep;
	}

	targetSpeed = targetVel.GetLength();

	// get/estimate the path distance to the target point
	float distToEnd = m_State.fDistanceToPathEnd;

	float distToTarget = Distance::Point_Point2D(GetPos(), targetPos);
	distToEnd = max(distToEnd, distToTarget);

	distToEnd -= distance;
	if (distToEnd < 0.0f)
		distToEnd = 0.0f;

	float walkSpeed, runSpeed, sprintSpeed, junk0, junk1;
	GetMovementSpeedRange(AISPEED_WALK, false, junk0, junk1, walkSpeed);
	GetMovementSpeedRange(AISPEED_RUN, false, junk0, junk1, runSpeed);
	GetMovementSpeedRange(AISPEED_SPRINT, false, junk0, junk1, sprintSpeed);

	// ramp up/down the urgency
	static float distForWalk = 4.0f;
	static float distForRun = 6.0f;
	static float distForSprint = 10.0f;

	if (m_lastChaseUrgencyDist > 0)
	{
		if (m_lastChaseUrgencyDist == 4)
		{
			// Sprint
			if (distToEnd < distForSprint - 1.0f)
				m_lastChaseUrgencyDist = 3;
		}
		else if (m_lastChaseUrgencyDist == 3)
		{
			// Run
			if (distToEnd > distForSprint)
				m_lastChaseUrgencyDist = 4;
			if (distToEnd < distForWalk - 1.0f)
				m_lastChaseUrgencyDist = 2;
		}
		else
		{
			// Walk
			if (distToEnd > distForRun)
				m_lastChaseUrgencyDist = 3;
		}
	}
	else
	{
		m_lastChaseUrgencyDist = 0; // zero
		if (distToEnd > distForRun)
			m_lastChaseUrgencyDist = 4; // sprint
		else if (distToEnd > distForWalk)
			m_lastChaseUrgencyDist = 3; // run
		else
			m_lastChaseUrgencyDist = 2; // walk
	}

	float urgencyDist = IndexToMovementUrgency(m_lastChaseUrgencyDist);

	if (m_lastChaseUrgencySpeed > 0)
	{
		if (m_lastChaseUrgencyDist == 4)
		{
			// Sprint
			if (targetSpeed < runSpeed)
				m_lastChaseUrgencySpeed = 3;
		}
		else if (m_lastChaseUrgencyDist == 3)
		{
			// Run
			if (targetSpeed > runSpeed * 1.2f)
				m_lastChaseUrgencySpeed = 4;
			if (targetSpeed < walkSpeed)
				m_lastChaseUrgencySpeed = 2;
		}
		else
		{
			// Walk
			if (targetSpeed > walkSpeed * 1.2f)
				m_lastChaseUrgencySpeed = 3;
			if (targetSpeed < 0.001f)
				m_lastChaseUrgencySpeed = 0;
		}
	}
	else
	{
		if (targetSpeed > runSpeed * 1.2f)
			m_lastChaseUrgencySpeed = 4;  // sprint
		else if (targetSpeed > walkSpeed * 1.2f)
			m_lastChaseUrgencySpeed = 3;  // run
		else if (targetSpeed > 0.0f)
			m_lastChaseUrgencySpeed = 2;  // walk
		else
			m_lastChaseUrgencySpeed = 0;  // zero
	}

	float urgencySpeed = IndexToMovementUrgency(m_lastChaseUrgencySpeed);

	/*	float urgencyDist = AISPEED_ZERO;
	   if (distToEnd > distForRun)
	    urgencyDist = AISPEED_SPRINT;
	   else if (distToEnd > distForWalk)
	    urgencyDist = AISPEED_RUN;
	   else
	    urgencyDist = AISPEED_WALK;

	   float urgencySpeed = AISPEED_ZERO;

	   if (targetSpeed > runSpeed)
	    urgencySpeed = AISPEED_SPRINT;
	   else if (targetSpeed > walkSpeed)
	    urgencySpeed = AISPEED_RUN;
	   else if (targetSpeed > 0.0f)
	    urgencySpeed = AISPEED_WALK;
	   else
	    urgencySpeed = AISPEED_ZERO;*/

	float urgency = max(urgencySpeed, urgencyDist);

	m_State.fMovementUrgency = urgency;
	m_State.predictedCharacterStates.nStates = 0;

	float normalSpeed, minSpeed, maxSpeed;
	GetMovementSpeedRange(urgency, m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

	if (targetSpeed < minSpeed)
		targetSpeed = minSpeed;

	// calculate the speed required to match the target speed, and also the
	// speed desired to trace the path without worrying about the target movement.
	// Then take the maximum of each.
	// Also we have to make sure that we don't overshoot the path, otherwise we'll
	// keep stopping.
	// m_State.fDesiredSpeed will/should already include/not include slowing at end
	// depending on if the target is moving
	// If dist to path end > lagDistance * 2 then use path speed control
	// if between lagDistance*2 and lagDistance blend between path speed control and absolute speed
	// if less than than then blend to 0
	static float maxExtraLag = 2.0f;
	static float speedForMaxExtraLag = 1.5f;

	float lagDistance = 0.1f + maxExtraLag * targetSpeed / speedForMaxExtraLag;
	Limit(lagDistance, 0.0f, maxExtraLag);

	float frac = distToEnd / lagDistance;
	Limit(frac, 0.0f, 2.2f);
	float chaseSpeed;
	if (frac < 1.0f)
		chaseSpeed = frac * targetSpeed + (1.0f - frac) * minSpeed;
	else if (frac < 2.0f)
		chaseSpeed = (2.0f - frac) * targetSpeed + (frac - 1.0f) * maxSpeed;
	else
		chaseSpeed = maxSpeed * (frac - 1.0f);

	static float chaseSpeedSmoothTime = 1.0f;
	SmoothCD(m_chaseSpeed, m_chaseSpeedRate, timeStep, chaseSpeed, chaseSpeedSmoothTime);

	if (m_State.fDesiredSpeed > m_chaseSpeed)
		m_State.fDesiredSpeed = m_chaseSpeed;
	m_State.fTargetSpeed = chaseSpeed;

}

// const float CSpeedControl::m_CMaxDist = 3.0f;

//===================================================================
// SAIPotentialTarget::Serialize
//===================================================================
void SAIPotentialTarget::Serialize(TSerialize ser)
{
	ser.EnumValue("type", type, AITARGET_NONE, AITARGET_LAST);
	ser.Value("priority", priority);
	ser.Value("upPriority", upPriority);
	ser.Value("upPriorityTime", upPriorityTime);
	ser.Value("soundTime", soundTime);
	ser.Value("soundMaxTime", soundMaxTime);
	ser.Value("soundThreatLevel", soundThreatLevel);
	ser.Value("soundPos", soundPos);
	ser.Value("visualFrameId", visualFrameId);
	ser.Value("visualTime", visualTime);
	ser.Value("visualMaxTime", visualMaxTime);
	ser.Value("visualPos", visualPos);
	ser.EnumValue("visualType", visualType, VIS_NONE, VIS_LAST);
	ser.EnumValue("threat", threat, AITHREAT_NONE, AITHREAT_LAST);
	ser.Value("threatTime", threatTime);
	ser.Value("exposure", exposure);
	ser.Value("threatTimeout", threatTimeout);
	ser.Value("indirectSight", indirectSight);

	// Some dummy objects aren't serialized. In that case don't serialize the ref itself
	//	(leads to invalid reference counts) but just serialize the id. This can
	//	then be used to recreate the object after loading
	ser.BeginGroup("refDummy");
	if (ser.IsWriting())
	{
		CAIObject* pObject = refDummyRepresentation.GetAIObject();
		if (pObject)
		{
			if (pObject->ShouldSerialize())
			{
				refDummyRepresentation.Serialize(ser, "refDummyRepresentation");
			}
			else
			{
				tAIObjectID id = refDummyRepresentation.GetObjectID();
				ser.Value("refDummyId", id);

				CryFixedStringT<64> name = pObject->GetName();
				ser.Value("name", name);

				IAIObject::ESubType type = pObject->GetSubType();
				ser.EnumValue("subtype", type, IAIObject::STP_NONE, IAIObject::STP_MAXVALUE);

			}
		}
	}
	else
	{
		tAIObjectID id = INVALID_AIOBJECTID;
		ser.Value("refDummyId", id);
		refDummyRepresentation.Serialize(ser, "refDummyRepresentation");

		assert((refDummyRepresentation.GetAIObject() && refDummyRepresentation->ShouldSerialize()) || id != INVALID_AIOBJECTID);

		if (refDummyRepresentation.IsNil() && id != INVALID_AIOBJECTID)
		{
			CryFixedStringT<64> name;
			ser.Value("name", name);

			IAIObject::ESubType type;
			ser.EnumValue("subtype", type, IAIObject::STP_NONE, IAIObject::STP_MAXVALUE);

			// create new dummy to replace the one that wasn't serialized
			gAIEnv.pAIObjectManager->CreateDummyObject(refDummyRepresentation, name.c_str(), type, id);
		}
	}
	ser.EndGroup();
}

//===================================================================
// UpdateBeacon
//===================================================================
void CPuppet::UpdateBeacon()
{
	CCCPOINT(CPuppet_UpdateBeacon);

	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
	CAIObject* pLastOpResult = m_refLastOpResult.GetAIObject();

	if (pAttentionTarget)
		GetAISystem()->UpdateBeacon(GetGroupId(), pAttentionTarget->GetPos(), pAttentionTarget);
	else if (pLastOpResult)
		GetAISystem()->UpdateBeacon(GetGroupId(), pLastOpResult->GetPos(), pLastOpResult);
}

//===================================================================
// CheckTargetInRange
//===================================================================
bool CPuppet::CheckTargetInRange(Vec3& vTargetPos)
{

	Vec3 vTarget = vTargetPos - GetPos();
	float targetDist2 = vTarget.GetLengthSquared();

	// don't shoot if the target is not in range
	float fMinDistance = m_CurrentWeaponDescriptor.fRangeMin;// m_FireProperties.GetMinDistance();
	float fMaxDistance = m_CurrentWeaponDescriptor.fRangeMax;// m_FireProperties.GetMaxDistance();
	if (targetDist2 < fMinDistance * fMinDistance && fMinDistance > 0)
	{
		if (!m_bWarningTargetDistance)
		{
			SetSignal(0, "OnTargetTooClose", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnTargetTooClose);
			m_bWarningTargetDistance = true;
		}
		return false;
	}
	else if (targetDist2 > fMaxDistance * fMaxDistance && fMaxDistance > 0)
	{
		if (!m_bWarningTargetDistance)
		{
			SetSignal(0, "OnTargetTooFar", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnTargetTooFar);
			m_bWarningTargetDistance = true;
		}
		return false;
	}
	else
		m_bWarningTargetDistance = false;

	return true;
}

//===================================================================
// CanFireInStance
//===================================================================
bool CPuppet::CanFireInStance(EStance stance, float fDistanceRatio /*= 0.9f*/) const
{
	bool bResult = false;

	CAIObject* pLiveTarget = GetLiveTarget(m_refLastOpResult).GetAIObject();
	if (pLiveTarget)
	{
		// Try to use perceived location
		Vec3 vTargetPos;
		if (!GetPerceivedTargetPos(pLiveTarget, vTargetPos))
			vTargetPos = pLiveTarget->GetPos();

		// Do a partial check along the potential fire direction based on distance ratio, to see if aim is blocked
		const float fDistance = vTargetPos.GetDistance(GetPos()) * clamp_tpl(fDistanceRatio, 0.0f, 1.0f);
		bResult = CheckLineOfFire(vTargetPos, fDistance, 0.5f, stance);
	}

	return bResult;
}

//===================================================================
// ResetSpeedControl
//===================================================================
void CPuppet::ResetSpeedControl()
{
	m_SpeedControl.Reset(GetPos(), GetAISystem()->GetFrameStartTime());
	m_chaseSpeed = 0.0f;
	m_chaseSpeedRate = 0.0f;
	m_lastChaseUrgencyDist = -1;
	m_lastChaseUrgencySpeed = -1;
}

//===================================================================
// CheckFriendsInLineOfFire
//===================================================================
bool CPuppet::CheckFriendsInLineOfFire(const Vec3& fireDirection, bool cheapTest)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	lookUp.Prepare(ActorLookUp::Position | ActorLookUp::Proxy);

	if (m_updatePriority != AIPUP_VERY_HIGH)
		cheapTest = true;

	const Vec3& firePos = GetFirePos();
	bool friendOnWay = false;

	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
	if (pPlayer && !pPlayer->IsHostile(this))
	{
		if (IsFriendInLineOfFire(pPlayer, firePos, fireDirection, cheapTest))
			friendOnWay = true;
	}

	if (!friendOnWay)
	{
		const float checkRadiusSqr = sqr(fireDirection.GetLength() + 2.0f);

		size_t activeActorCount = lookUp.GetActiveCount();

		for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
		{
			CAIActor* pFriend = lookUp.GetActor<CAIActor>(actorIndex);

			// Skip for self
			if (!pFriend || pFriend == this)
				continue;

			IAIActorProxy* proxy = lookUp.GetProxy(actorIndex);
			if (!proxy)
				continue;

			if (Distance::Point_PointSq(lookUp.GetPosition(actorIndex), firePos) > checkRadiusSqr)
				continue;

			// Check against only actors (ignore vehicles).
			if (pFriend->GetType() == AIOBJECT_VEHICLE)
				continue;

			if (!pFriend->GetPhysics())
				continue;

			// Skip friends in vehicles.
			if (proxy->GetLinkedVehicleEntityId())
				continue;

			if (IsHostile(pFriend))
				continue;

			if (IsFriendInLineOfFire(pFriend, firePos, fireDirection, cheapTest))
			{
				friendOnWay = true;
				break;
			}
		}
	}

	if (friendOnWay)
		m_friendOnWayElapsedTime += GetAISystem()->GetUpdateInterval();
	else
		m_friendOnWayElapsedTime = 0.0f;

	return friendOnWay;
}

//===================================================================
// IsFriendInLineOfFire
//===================================================================
bool CPuppet::IsFriendInLineOfFire(CAIObject* pFriend, const Vec3& firePos, const Vec3& fireDirection, bool cheapTest)
{
	if (!pFriend->GetProxy())
		return false;
	IPhysicalEntity* pPhys = pFriend->GetProxy()->GetPhysics(true);
	if (!pPhys)
		pPhys = pFriend->GetPhysics();
	if (!pPhys)
		return false;

	const Vec3 normalizedFireDirectionXY = Vec3(fireDirection.x, fireDirection.y, 0.0f).GetNormalizedSafe();
	Vec3 directionFromFirePositionToOtherAgentPosition = pFriend->GetPhysicsPos() - firePos;
	directionFromFirePositionToOtherAgentPosition.z = 0.0f;
	directionFromFirePositionToOtherAgentPosition.NormalizeSafe();

	static float threshold = cosf(DEG2RAD(35.0f));
	if (normalizedFireDirectionXY.Dot(directionFromFirePositionToOtherAgentPosition) > threshold)
	{
		const float detectionSide = 0.2f;
		Vec3 fudge(detectionSide, detectionSide, detectionSide);

		pe_status_pos statusPos;
		pPhys->GetStatus(&statusPos);
		AABB bounds(statusPos.BBox[0] - fudge + statusPos.pos, statusPos.BBox[1] + fudge + statusPos.pos);

		return Overlap::Lineseg_AABB(Lineseg(firePos, firePos + fireDirection), bounds);
	}

	return false;
}

//===================================================================
// GetShootingStatus
//===================================================================
void CPuppet::GetShootingStatus(SShootingStatus& ss)
{
	ss.fireMode = m_fireMode;
	ss.timeSinceTriggerPressed = m_timeSinceTriggerPressed;
	ss.triggerPressed = (m_State.fire == eAIFS_On);
	ss.friendOnWay = m_friendOnWayElapsedTime > 0.001f;
	ss.friendOnWayElapsedTime = m_friendOnWayElapsedTime;
}

//===================================================================
// GetDistanceAlongPath
//===================================================================
float CPuppet::GetDistanceAlongPath(const Vec3& pos, bool bInit)
{
	Vec3 myPos(GetEntity() ? GetEntity()->GetPos() : GetPos());

	if (m_nPathDecision != PATHFINDER_PATHFOUND)
		return 0;

	if (bInit)
	{
		m_InitialPath.clear();
		TPathPoints::const_iterator liend = m_OrigPath.GetPath().end();
		for (TPathPoints::const_iterator li = m_OrigPath.GetPath().begin(); li != liend; ++li)
			m_InitialPath.push_back(li->vPos);

	}
	if (!m_InitialPath.empty())
	{
		float mindist = 10000000.f;
		float mindistObj = 10000000.f;
		TPointList::const_iterator listart = m_InitialPath.begin();
		TPointList::const_iterator li = listart;
		TPointList::const_iterator liend = m_InitialPath.end();
		TPointList::const_reverse_iterator lilast = m_InitialPath.rbegin();
		TPointList::const_iterator linext = li;
		TPointList::const_iterator liMyPoint = liend;
		TPointList::const_iterator liObjPoint = liend;
		Vec3 p1(ZERO);
		++linext;
		int count = 0;
		int myCount = 0;
		int objCount = 0;
		float objCoeff = 0;
		int maxCount = m_InitialPath.size() - 1;
		for (; linext != liend; ++li, ++linext)
		{
			float t, u;
			float distObj = Distance::Point_Lineseg(pos, Lineseg(*li, *linext), t);
			float mydist = Distance::Point_Lineseg(myPos, Lineseg(*li, *linext), u);
			if (distObj < mindistObj && (count == 0 && t < 0 || t >= 0 && t <= 1 || count == maxCount && t > 1))
			{
				liObjPoint = li;
				mindistObj = distObj;
				objCount = count;
				objCoeff = t;
			}
			if (mydist < mindist && (count == 0 && u < 0 || u >= 0 && u <= 1 || count == maxCount && u > 1))
			{
				liMyPoint = li;
				mindist = mydist;
				myCount = count;
			}
			count++;
		}

		// check if object is outside the path
		if (objCoeff <= 0 && objCount == 0)
			return Distance::Point_Point(pos, *listart) + Distance::Point_Point(myPos, *listart);
		else if (objCoeff >= 1 && objCount >= count - 1)
			return -(Distance::Point_Point(pos, *lilast) + Distance::Point_Point(myPos, *lilast));

		if (liMyPoint != liend && liObjPoint != liend && liMyPoint != liObjPoint)
		{
			if (objCount > myCount)
			{
				// other object is ahead
				++liMyPoint;
				float dist = Distance::Point_Point(*liObjPoint, pos);
				if (liMyPoint != liend)
					dist += Distance::Point_Point(*liMyPoint, myPos);

				li = liMyPoint;
				linext = li;
				++linext;

				for (; li != liObjPoint && linext != liend; ++li, ++linext)
					dist += Distance::Point_Point(*li, *linext);

				return -dist;
			}
			else
			{
				// other object is back
				++liObjPoint;
				float dist = Distance::Point_Point(*liMyPoint, myPos);
				if (liObjPoint != liend)
					dist += Distance::Point_Point(*liObjPoint, pos);

				li = liObjPoint;
				linext = li;
				++linext;

				for (; li != liMyPoint && linext != liend; ++li, ++linext)
					dist += Distance::Point_Point(*li, *linext);

				return dist;
			}
		}
		// check just positions, object is on the same path segment
		Vec3 myOrientation(ZERO);
		if (GetPhysics())
		{
			pe_status_dynamics dSt;
			GetPhysics()->GetStatus(&dSt);
			myOrientation = dSt.v;
		}
		if (myOrientation.IsEquivalent(Vec3Constants<float>::fVec3_Zero))
			myOrientation = GetViewDir();

		Vec3 dir = pos - myPos;
		float dist = dir.GetLength();

		return(myOrientation.Dot(dir) < 0 ? dist : -dist);

	}
	// no path
	return 0;
}

//===================================================================
// GetPotentialTargets
//===================================================================
bool CPuppet::GetPotentialTargets(PotentialTargetMap& targetMap) const
{
	bool bResult = false;

	if (gAIEnv.CVars.TargetTracking)
	{
		CWeakRef<CAIObject> refBestTarget;
		SAIPotentialTarget* bestTargetEvent = 0;
		bool currentTargetErased = false;

		if (GetTargetTrackBestTarget(refBestTarget, bestTargetEvent, currentTargetErased) && bestTargetEvent)
		{
			targetMap.insert(PotentialTargetMap::value_type(refBestTarget, *bestTargetEvent));
			bResult = true;
		}
	}
	else
	{
		// Disabled
		bResult = false;
	}

	return bResult;
}

uint32 CPuppet::GetBestTargets(tAIObjectID* targets, uint32 maxCount) const
{
	tAIObjectID myID = GetAIObjectID();
	gAIEnv.pTargetTrackManager->Update(myID);

	return gAIEnv.pTargetTrackManager->GetBestTargets(myID,
	                                                  (TargetTrackHelpers::eDTM_Select_Highest), targets, maxCount);
}

//===================================================================
// AddAggressiveTarget
//===================================================================
bool CPuppet::AddAggressiveTarget(IAIObject* pTarget)
{
	bool bResult = false;

	if (m_pPerceptionHandler)
	{
		CWeakRef<CAIObject> refTarget = GetWeakRefSafe((CAIObject*)pTarget);
		bResult = m_pPerceptionHandler->AddAggressiveTarget(refTarget);
	}

	return bResult;
}

//===================================================================
// SetTempTargetPriority
//===================================================================
bool CPuppet::SetTempTargetPriority(ETempTargetPriority priority)
{
	bool bResult = false;

	if (m_pPerceptionHandler)
		bResult = m_pPerceptionHandler->SetTempTargetPriority(priority);

	return bResult;
}

//===================================================================
// UpdateTempTarget
//===================================================================
bool CPuppet::UpdateTempTarget(const Vec3& vPosition)
{
	bool bResult = false;

	if (m_pPerceptionHandler)
		bResult = m_pPerceptionHandler->UpdateTempTarget(vPosition);

	return bResult;
}

//===================================================================
// ClearTempTarget
//===================================================================
bool CPuppet::ClearTempTarget()
{
	bool bResult = false;

	if (m_pPerceptionHandler)
		bResult = m_pPerceptionHandler->ClearTempTarget();

	return bResult;
}

//===================================================================
// DropTarget
//===================================================================
bool CPuppet::DropTarget(IAIObject* pTarget)
{
	bool bResult = false;

	if (m_pPerceptionHandler)
	{
		CWeakRef<CAIObject> refTarget = GetWeakRefSafe((CAIObject*)pTarget);
		bResult = m_pPerceptionHandler->DropTarget(refTarget);
	}

	return bResult;
}

// Description:
//   Change flag so this puppet can be shoot or not
// Arguments:
//   bCanBeShot - If the puppet can be shoot or not
// Return:
//
void CPuppet::SetCanBeShot(bool bCanBeShot)
{
	m_bCanBeShot = bCanBeShot;
}

// Description:
//   Read flag if this puppet can be shoot or not
// Arguments:
// Return:
//   bool - If the puppet can be shoot or not
bool CPuppet::GetCanBeShot() const
{
	return (m_bCanBeShot);
}

//===================================================================
// SetMemoryFireType
//===================================================================
void CPuppet::SetMemoryFireType(EMemoryFireType eType)
{
	CRY_ASSERT(eType < eMFT_COUNT);
	m_eMemoryFireType = eType;
}

//===================================================================
// GetMemoryFireType
//===================================================================
EMemoryFireType CPuppet::GetMemoryFireType() const
{
	return m_eMemoryFireType;
}

//===================================================================
// CanMemoryFire
//===================================================================
bool CPuppet::CanMemoryFire() const
{
	bool bResult = true;

	if (m_targetLostTime > FLT_EPSILON)
	{
		switch (m_eMemoryFireType)
		{
		case eMFT_Disabled:
			bResult = false;
			break;

		case eMFT_UseCoverFireTime:
			{
				const float fCoverTime = GetCoverFireTime();
				bResult = (m_targetLostTime <= fCoverTime);
			}
			break;

		case eMFT_Always:
			bResult = true;
			break;

		default:
			CRY_ASSERT_MESSAGE(false, "Unhandled EMemoryFireType in CPuppet::CanMemoryFire()");
			break;
		}
	}

	return bResult;
}

//===================================================================
// GetSoundPerceptionDescriptor
//===================================================================
bool CPuppet::GetSoundPerceptionDescriptor(EAISoundStimType eType, SSoundPerceptionDescriptor& sDescriptor) const
{
	CRY_ASSERT(eType >= 0 && eType < AISOUND_LAST);
	bool bResult = false;

	if (eType >= 0 && eType < AISOUND_LAST)
	{
		if (!m_SoundPerceptionDescriptor.size())
			sDescriptor = s_DefaultSoundPerceptionDescriptor[eType];
		else
			sDescriptor = m_SoundPerceptionDescriptor[eType];

		bResult = true;
	}

	return bResult;
}

//===================================================================
// SetSoundPerceptionDescriptor
//===================================================================
bool CPuppet::SetSoundPerceptionDescriptor(EAISoundStimType eType, const SSoundPerceptionDescriptor& sDescriptor)
{
	CRY_ASSERT(eType >= 0 && eType < AISOUND_LAST);
	bool bResult = false;

	if (eType >= 0 && eType < AISOUND_LAST)
	{
		if (!m_SoundPerceptionDescriptor.size())
			m_SoundPerceptionDescriptor.append(s_DefaultSoundPerceptionDescriptor);

		m_SoundPerceptionDescriptor[eType] = sDescriptor;
		bResult = true;
	}

	return bResult;
}

//===================================================================
// GetDamageParts
//===================================================================
DamagePartVector* CPuppet::GetDamageParts()
{
	if (!m_damagePartsUpdated)
	{
		UpdateDamageParts(m_damageParts);
		m_damagePartsUpdated = true;
	}

	return &m_damageParts;
}

//===================================================================
// GetValidPositionNearby
//===================================================================
bool CPuppet::GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const
{
	adjustedPosition = proposedPosition;
	if (!GetFloorPos(adjustedPosition, proposedPosition, 1.0f, 2.0f, WalkabilityDownRadius, AICE_ALL))
		return false;

	static float maxFloorDeviation = 1.0f;
	if (fabsf(adjustedPosition.z - proposedPosition.z) > maxFloorDeviation)
		return false;

	if (!CheckBodyPos(adjustedPosition, AICE_ALL))
		return false;

	const Vec3 pushUp(.0f, .0f, .2f);
	return gAIEnv.pNavigationSystem->IsLocationValidInNavigationMesh(GetNavigationTypeID(), adjustedPosition + pushUp, nullptr);
}

//===================================================================
// GetTeleportPosition
//===================================================================
bool CPuppet::GetTeleportPosition(Vec3& teleportPos) const
{
	AIWarning("CPuppet::GetTeleportPosition is currently not supported by the MNM Navigation System.");
	return false;
}

void CPuppet::EnableFire(bool enable)
{
	if (enable)
	{
		assert(m_fireDisabled > 0);
		if (m_fireDisabled > 0)
			--m_fireDisabled;
	}
	else
	{
		++m_fireDisabled;
	}

	assert(m_fireDisabled <= 8);
}

bool CPuppet::IsFireEnabled() const
{
	return m_fireDisabled == 0;
}

//===================================================================
// SetAllowedStrafeDistances
//===================================================================
void CPuppet::SetAllowedStrafeDistances(float start, float end, bool whileMoving)
{
	m_allowedStrafeDistanceStart = start;
	m_allowedStrafeDistanceEnd = end;
	m_allowStrafeLookWhileMoving = whileMoving;

	m_strafeStartDistance = 0.0f;

	UpdateStrafing();
}

//===================================================================
// SetAdaptiveMovementUrgency
//===================================================================
void CPuppet::UpdateStrafing()
{
	m_State.allowStrafing = false;
	if (m_State.fDistanceToPathEnd > 0)
	{
		//		if (!m_State.vAimTargetPos().IsZero())
		{
			if (m_allowedStrafeDistanceStart > 0.001f)
			{
				// Calculate the max travelled distance.
				float distanceMoved = m_OrigPath.GetPathLength(false) - m_State.fDistanceToPathEnd;
				m_strafeStartDistance = max(m_strafeStartDistance, distanceMoved);

				if (m_strafeStartDistance < m_allowedStrafeDistanceStart)
					m_State.allowStrafing = true;
			}

			if (m_allowedStrafeDistanceEnd > 0.001f)
			{
				if (m_State.fDistanceToPathEnd < m_allowedStrafeDistanceEnd)
					m_State.allowStrafing = true;
			}
		}
	}
}

//===================================================================
// SetAdaptiveMovementUrgency
//===================================================================
void CPuppet::SetAdaptiveMovementUrgency(float minUrgency, float maxUrgency, float scaleDownPathlen)
{
	m_adaptiveUrgencyMin = minUrgency;
	m_adaptiveUrgencyMax = maxUrgency;
	m_adaptiveUrgencyScaleDownPathLen = scaleDownPathlen;
	m_adaptiveUrgencyMaxPathLen = 0.0f;
}

//===================================================================
// SetDelayedStance
//===================================================================
void CPuppet::SetDelayedStance(int stance)
{
	m_delayedStance = stance;
	m_delayedStanceMovementCounter = 0;
}

//===================================================================
// GetPosAlongPath
//===================================================================
bool CPuppet::GetPosAlongPath(float dist, bool extrapolateBeyond, Vec3& retPos) const
{
	return m_Path.GetPosAlongPath(retPos, dist, !m_movementAbility.b3DMove, extrapolateBeyond);
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CPuppet::AddPerceptionHandlerModifier(IPerceptionHandlerModifier* pModifier)
{
	return stl::push_back_unique(m_perceptionHandlerModifiers, pModifier);
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CPuppet::RemovePerceptionHandlerModifier(IPerceptionHandlerModifier* pModifier)
{
	return stl::find_and_erase(m_perceptionHandlerModifiers, pModifier);
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CPuppet::GetPerceptionHandlerModifiers(TPerceptionHandlerModifiersVector& outModifiers)
{
	outModifiers = m_perceptionHandlerModifiers;
	return (!m_perceptionHandlerModifiers.empty());
}

//
//------------------------------------------------------------------------------------------------------------------------
void CPuppet::DebugDrawPerceptionHandlerModifiers()
{
	TPerceptionHandlerModifiersVector::iterator itModifier = m_perceptionHandlerModifiers.begin();
	TPerceptionHandlerModifiersVector::iterator itModifierEnd = m_perceptionHandlerModifiers.end();
	EntityId entityId = GetEntityID();
	float fY = 30.0f;

	for (; itModifier != itModifierEnd; ++itModifier)
	{
		(*itModifier)->DebugDraw(entityId, fY);
	}
}

float CPuppet::GetTimeToNextShot() const
{
	if (m_pFireCmdHandler)
		return m_pFireCmdHandler->GetTimeToNextShot();

	return 0.0f;
}
