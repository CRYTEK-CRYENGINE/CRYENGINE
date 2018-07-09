// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   AIProxy.cpp
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - 7:10:2004   18:54 : Created by Kirill Bulatsev
   - 2:12:2005           Now talks directly to CryAction interfaces (craig tiller)
   - 2 Mar 2009				: Evgeny Adamenkov: Removed parameter of type IRenderer in DebugDraw


   Notes:
   We can't assume that there is just one IGameObject to an entity, because we choose between (at least) Actor and Vehicle objects.


 *********************************************************************/

#include "StdAfx.h"
#include "AIProxy.h"
#include "AI/AIProxyManager.h"
#include <CryAISystem/IAIAction.h>
#include <CryAISystem/IAISystem.h>
#include "AIHandler.h"
#include "IItemSystem.h"
#include "IVehicleSystem.h"
#include <CryAnimation/ICryAnimation.h>
#include "CryAction.h"

#include "RangeSignalingSystem/RangeSignaling.h"
#include "SignalTimers/SignalTimers.h"
#include <CryAISystem/IAIDebugRenderer.h>

#include "AI/CommunicationHandler.h"

#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include "AnimationGraph/AnimatedCharacter.h"

//
//----------------------------------------------------------------------------------------------------------
CAIProxy::CAIProxy(IGameObject* pGameObject) :
	m_pGameObject(pGameObject),
	m_pAIHandler(0),
	m_firing(false),
	m_firingSecondary(false),
	m_fMinFireTime(0),
	m_WeaponShotIsDone(false),
	m_NeedsShootSignal(false),
	m_CurrentWeaponCanFire(false),
	m_UseSecondaryVehicleWeapon(false),
	m_IsDisabled(false),
	m_UpdateAlways(false),
	m_autoDeactivated(false),
	m_currentWeaponId(0),
	m_currentWeaponFiremodeIndex(std::numeric_limits<int>::max()),
	m_pIActor(0),
	m_shotBulletCount(0),
	m_lastShotTime(0.0f),
	m_animActionGoalPipeId(0),
	m_FmodCharacterTypeParam(0),
	m_forcedExecute(0),
	m_weaponListenerId(0),
	m_aimQueryMode(QueryAimFromMovementController)
{
	// (MATT) This used to include a useAIHandler option, but it was always true {2009/04/06}
	// (MATT) This and serialisation are the only places this is set {2009/01/28}
	// (MATT) All the NULL checks on this could go - only Release clears the pointer {2009/04/20}
	m_pAIHandler = new CAIHandler(m_pGameObject);
	m_pAIHandler->Init();

	ReloadScriptProperties();

	m_commHandler.reset(new CommunicationHandler(*this, m_pGameObject->GetEntity()));
}

//
//----------------------------------------------------------------------------------------------------------
CAIProxy::~CAIProxy()
{
	if (m_weaponListenerId != 0)
	{
		RemoveWeaponListener(GetWeaponFromId(m_weaponListenerId));
	}

	if (m_pAIHandler)
		m_pAIHandler->Release();

	CCryAction::GetCryAction()->GetAIProxyManager()->OnAIProxyDestroyed(this);
}

//
//----------------------------------------------------------------------------------------------------------
EntityId CAIProxy::GetLinkedDriverEntityId()
{
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
	if (pVehicle)
		if (IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(1))  //1 means driver
			return pVehicleSeat->GetPassenger();
	return 0;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsDriver()
{
	// (MATT) Surely rather redundant {2009/01/27}
	if (IActor* pActor = GetActor())
	{
		if (pActor->GetLinkedVehicle())
		{
			IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
			IVehicle* pVehicle = pVehicleSystem->GetVehicle(pActor->GetLinkedVehicle()->GetEntityId());
			if (pVehicle)
			{
				if (IVehicleSeat* pVehicleSeat = pVehicle->GetSeatById(1))  //1 means driver
				{
					if (m_pGameObject)
					{
						EntityId passengerId = pVehicleSeat->GetPassenger();
						EntityId selfId = m_pGameObject->GetEntityId();
						if (passengerId == selfId)
							return true;
					}
				}
			}
		}
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------
EntityId CAIProxy::GetLinkedVehicleEntityId()
{
	if (IActor* pActor = GetActor())
	{
		if (pActor->GetLinkedVehicle())
			return pActor->GetLinkedVehicle()->GetEntityId();
	}
	return 0;
}

//----------------------------------------------------------------------------------------------------------
bool CAIProxy::GetLinkedVehicleVisionHelper(Vec3& outHelperPos) const
{
	assert(m_pGameObject);

	IActor* pActor = GetActor();
	IVehicle* pVehicle = pActor ? pActor->GetLinkedVehicle() : NULL;
	IVehicleSeat* pVehicleSeat = pVehicle ? pVehicle->GetSeatForPassenger(m_pGameObject->GetEntityId()) : NULL;
	IVehicleHelper* pAIVisionHelper = pVehicleSeat ? pVehicleSeat->GetAIVisionHelper() : NULL;

	if (pAIVisionHelper)
	{
		outHelperPos = pAIVisionHelper->GetWorldSpaceTranslation();
		return true;
	}

	return false;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::QueryBodyInfo(SAIBodyInfo& bodyInfo)
{
	if (IMovementController* pMC = m_pGameObject->GetMovementController())
	{
		SMovementState moveState;
		pMC->GetMovementState(moveState);

		bodyInfo.maxSpeed = moveState.maxSpeed;
		bodyInfo.minSpeed = moveState.minSpeed;
		bodyInfo.normalSpeed = moveState.normalSpeed;
		bodyInfo.stance = moveState.stance;
		bodyInfo.stanceSize = moveState.m_StanceSize;
		bodyInfo.colliderSize = moveState.m_ColliderSize;
		bodyInfo.vEyeDir = moveState.eyeDirection;
		bodyInfo.vEyeDirAnim = moveState.animationEyeDirection;
		bodyInfo.vEyePos = moveState.eyePosition;
		bodyInfo.vEntityDir = moveState.entityDirection;
		bodyInfo.vAnimBodyDir = moveState.animationBodyDirection;
		bodyInfo.vMoveDir = moveState.movementDirection;
		bodyInfo.vUpDir = moveState.upDirection;
		bodyInfo.lean = moveState.lean;
		bodyInfo.peekOver = moveState.peekOver;
		bodyInfo.slopeAngle = moveState.slopeAngle;

		bodyInfo.vFirePos = moveState.weaponPosition;
		bodyInfo.vFireDir = moveState.aimDirection;
		bodyInfo.isFiring = moveState.isFiring;
		bodyInfo.isMoving = moveState.isMoving;

		switch (m_aimQueryMode)
		{
		case QueryAimFromMovementController:
			bodyInfo.isAiming = moveState.isAiming;
			break;
		case OverriddenAndAiming:
			bodyInfo.isAiming = true;
			break;
		case OverriddenAndNotAiming:
			bodyInfo.isAiming = false;
			break;
		}

		IActor* pActor = GetActor();
		IVehicle* pVehicle = pActor ? pActor->GetLinkedVehicle() : 0;
		IEntity* pVehicleEntity = pVehicle ? pVehicle->GetEntity() : 0;
		bodyInfo.linkedVehicleEntityId = pVehicleEntity ? pVehicleEntity->GetId() : 0;

		return true;
	}
	return false;
}

bool CAIProxy::QueryBodyInfo(const SAIBodyInfoQuery& query, SAIBodyInfo& bodyInfo)
{
	if (IMovementController* pMC = m_pGameObject->GetMovementController())
	{
		SStanceState stanceState;
		if (!pMC->GetStanceState(SStanceStateQuery(query.position, query.target, query.stance, query.lean, query.peekOver, query.defaultPose), stanceState))
			return false;

		bodyInfo.maxSpeed = 0.0f;
		bodyInfo.minSpeed = 0.0f;
		bodyInfo.normalSpeed = 0.0f;
		bodyInfo.stance = query.stance;
		bodyInfo.stanceSize = stanceState.m_StanceSize;
		bodyInfo.colliderSize = stanceState.m_ColliderSize;
		bodyInfo.vEyeDir = stanceState.eyeDirection;
		bodyInfo.vEyeDirAnim.zero();
		bodyInfo.vEyePos = stanceState.eyePosition;
		bodyInfo.vEntityDir = stanceState.entityDirection;
		bodyInfo.vAnimBodyDir = stanceState.animationBodyDirection;
		bodyInfo.vMoveDir.zero();
		bodyInfo.vUpDir = stanceState.upDirection;
		bodyInfo.vFirePos = stanceState.weaponPosition;
		bodyInfo.vFireDir = stanceState.aimDirection;
		bodyInfo.isAiming = false;
		bodyInfo.isFiring = false;
		bodyInfo.isMoving = false;
		bodyInfo.lean = stanceState.lean;
		bodyInfo.peekOver = stanceState.peekOver;
		bodyInfo.linkedVehicleEntityId = 0;

		return true;
	}

	return false;
}

IEntity* CAIProxy::GetGrabbedEntity() const
{
	IActor* pActor(GetActor());
	if (pActor)
	{
		return gEnv->pEntitySystem->GetEntity(pActor->GetGrabbedEntityId());
	}
	return 0;
}

void CreateTestVoiceLibraryNameFromBasicName(const string& basicName, string& testName)
{
	const string::size_type indexOfLastUnderscore = basicName.find_last_of('_');
	const stack_string testPostfix("_test");
	testName.reserve(basicName.size() + testPostfix.size());
	static const string::size_type npos = -1; // This is the value find_last_of will return if no '_' is found
	testName = basicName.substr(0, indexOfLastUnderscore != npos ? indexOfLastUnderscore : basicName.size());
	testName = string().Format("%s%s", testName.c_str(), testPostfix.c_str());
}

const char* CAIProxy::GetVoiceLibraryName(const bool useForcedDefaultName /* = false */) const
{
	IF_UNLIKELY (useForcedDefaultName)
	{
		return m_voiceLibraryWhenForcingTest.c_str();
	}
	else
	{
		return m_voiceLibrary.c_str();
	}
}

const char* CAIProxy::GetCommunicationConfigName() const
{
	return m_commConfigName.c_str();
}

const float CAIProxy::GetFmodCharacterTypeParam() const
{
	return m_FmodCharacterTypeParam;
}

const char* CAIProxy::GetNavigationTypeName() const
{
	return m_agentTypeName.c_str();
}

IWeapon* CAIProxy::QueryCurrentWeapon(EntityId& currentWeaponId)
{
	// Update the cached current weapon first
	UpdateCurrentWeapon();

	return GetCurrentWeapon(currentWeaponId);
}

IWeapon* CAIProxy::GetCurrentWeapon(EntityId& currentWeaponId) const
{
	currentWeaponId = m_currentWeaponId;

	IItem* pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(m_currentWeaponId);
	IWeapon* pWeapon = (pItem ? pItem->GetIWeapon() : NULL);

	return pWeapon;
}

IWeapon* CAIProxy::GetSecWeapon(const ERequestedGrenadeType prefGrenadeType, ERequestedGrenadeType* pReturnedGrenadeType, EntityId* const pSecondaryWeaponId) const
{
	CRY_ASSERT(prefGrenadeType > eRGT_INVALID && prefGrenadeType < eRGT_COUNT);

	if (pSecondaryWeaponId)
		*pSecondaryWeaponId = 0;

	IActor* pActor = GetActor();
	IInventory* pInventory = (pActor != NULL) ? pActor->GetInventory() : NULL;
	if (!pInventory)
		return NULL;

	IEntityClassRegistry* pRegistery = gEnv->pEntitySystem->GetClassRegistry();
	CRY_ASSERT(pRegistery);

	EntityId itemEntity = 0;
	IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();
	if (pAI && pAI->GetAIType() == AIOBJECT_PLAYER)
	{
		// Offhand weapon for player, can be any
		itemEntity = pInventory->GetItemByClass(pRegistery->FindClass("OffHand"));
	}
	else
	{
		// EntityIds of what types of grenades we have in our inventory
		// TODO Hardcoded class names are disgusting, need a better way
		EntityId pSecWeaponInventory[eRGT_COUNT] =
		{
			0,   // eRGT_ANY (if passed in as preferred type, will be skipped)
			pInventory->GetItemByClass(pRegistery->FindClass("AISmokeGrenades")),
			pInventory->GetItemByClass(pRegistery->FindClass("AIFlashbangs")),
			pInventory->GetItemByClass(pRegistery->FindClass("AIGrenades")),
			pInventory->GetItemByClass(pRegistery->FindClass("AIEMPGrenades")),
			pInventory->GetItemByClass(pRegistery->FindClass("AIGruntGrenades")),
		};

		// Check requested type first
		itemEntity = pSecWeaponInventory[prefGrenadeType];
		if (pReturnedGrenadeType)
			*pReturnedGrenadeType = prefGrenadeType;

		if (!itemEntity)
		{
			itemEntity = pSecWeaponInventory[eRGT_FRAG_GRENADE];
			if (pReturnedGrenadeType)
				*pReturnedGrenadeType = eRGT_FRAG_GRENADE;
		}
		if (!itemEntity)
		{
			itemEntity = pSecWeaponInventory[eRGT_FLASHBANG_GRENADE];
			if (pReturnedGrenadeType)
				*pReturnedGrenadeType = eRGT_FLASHBANG_GRENADE;
		}
		if (!itemEntity)
		{
			itemEntity = pSecWeaponInventory[eRGT_SMOKE_GRENADE];
			if (pReturnedGrenadeType)
				*pReturnedGrenadeType = eRGT_SMOKE_GRENADE;
		}
		if (!itemEntity)
		{
			itemEntity = pSecWeaponInventory[eRGT_EMP_GRENADE];
			if (pReturnedGrenadeType)
				*pReturnedGrenadeType = eRGT_EMP_GRENADE;
		}
		if (!itemEntity)
		{
			itemEntity = pSecWeaponInventory[eRGT_GRUNT_GRENADE];
		}
	}

	// Get weapon if we have something in our inventory
	if (itemEntity)
	{
		if (IItem* pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(itemEntity))
		{
			if (pSecondaryWeaponId)
				*pSecondaryWeaponId = pItem->GetEntityId();

			return pItem->GetIWeapon();
		}
	}

	if (pReturnedGrenadeType)
		*pReturnedGrenadeType = eRGT_INVALID;

	return 0;
}

IFireController* CAIProxy::GetFireController(uint32 controllerNum)
{
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();

	if (IVehicle* pVehicle = (IVehicle*) pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId()))
		return pVehicle->GetFireController(controllerNum);

	return NULL;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsEnabled() const
{
	return !m_IsDisabled;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::EnableUpdate(bool enable)
{
	// DO NOT:
	//  1. call SetAIActivation on GameObject
	//  2. call Activate on Entity
	// (this will result in an infinite-ish loop)

	bool bNotifyListeners = false;
	m_autoDeactivated = false; // always clear this - it needs to explicitly set by the automation mechanism

	if (enable)
	{
		if (m_IsDisabled)
			bNotifyListeners = true;

		IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();
		m_IsDisabled = false;
		if (pAI)
			pAI->Event(AIEVENT_ENABLE, NULL);
	}
	else
	{
		if (!m_IsDisabled)
			bNotifyListeners = true;

		IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();
		m_IsDisabled = true;
		if (pAI)
			pAI->Event(AIEVENT_DISABLE, NULL);
	}

	// Notify listeners
	if (bNotifyListeners)
	{
		TListeners::const_iterator listener = m_listeners.begin();
		TListeners::const_iterator listenerEnd = m_listeners.end();
		for (; listener != listenerEnd; ++listener)
		{
			(*listener)->OnAIProxyEnabled(!m_IsDisabled);
		}
	}

	if (IActor* pActor = GetActor())
	{
		pActor->OnAIProxyEnabled(!m_IsDisabled);
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::AddListener(IAIProxyListener* pListener)
{
	CRY_ASSERT(pListener);
	if (std::find(m_listeners.begin(), m_listeners.end(), pListener) == m_listeners.end())
	{
		m_listeners.push_back(pListener);

		// Notify of current status
		pListener->OnAIProxyEnabled(!m_IsDisabled);
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::RemoveListener(IAIProxyListener* pListener)
{
	CRY_ASSERT(pListener);
	stl::find_and_erase(m_listeners, pListener);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateMeAlways(bool doUpdateMeAlways)
{
	m_UpdateAlways = doUpdateMeAlways;
	CheckUpdateStatus();
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::ResendTargetSignalsNextFrame()
{
	if (m_pAIHandler)
		m_pAIHandler->ResendTargetSignalsNextFrame();
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::CheckUpdateStatus()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool update = false;
	// DO NOT call Activate on Entity
	// (this will result in an infinite-ish loop)
	if (gEnv->pAISystem->GetUpdateAllAlways() || m_UpdateAlways)
		update = m_pGameObject->SetAIActivation(eGOAIAM_Always);
	else
		update = m_pGameObject->SetAIActivation(eGOAIAM_VisibleOrInRange);
	return update;
}

//
//----------------------------------------------------------------------------------------------------------
int CAIProxy::Update(SOBJECTSTATE& state, bool bFullUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!CheckUpdateStatus())
		return 0;

	if (!m_pAIHandler)
		return 0;

	if (bFullUpdate)
		UpdateMind(state);

	if (IsDead() && GetPhysics() == NULL)
	{
		// killed actor is updated by AI ?
		return 0;
	}

	IActor* pActor = GetActor();

	bool fire = (eAIFS_On == state.fire);
	bool fireSec = (eAIFS_On == state.fireSecondary);
	const bool fireBlocking = (eAIFS_Blocking == state.fire || eAIFS_Blocking == state.fireSecondary || eAIFS_Blocking == state.fireMelee);
	bool isAlive(true);

	IMovementController* pMC = m_pGameObject->GetMovementController();
	if (pMC)
	{
		SMovementState moveState;
		pMC->GetMovementState(moveState);
		isAlive = moveState.isAlive;

		if (!isAlive)
		{
			fire = false;
			fireSec = false;
		}

		Vec3 currentPos = m_pGameObject->GetEntity()->GetWorldPos();

		CMovementRequest mr;

		int alertness = GetAlertnessState();
		mr.SetAlertness(alertness);

		// [DAVID] Set Actor/AC alertness
		if (pActor != NULL)
			pActor->SetFacialAlertnessLevel(alertness);

		if (!state.vLookTargetPos.IsZero())
			mr.SetLookTarget(state.vLookTargetPos);
		else
			mr.ClearLookTarget();

		mr.SetLookStyle(state.eLookStyle);
		mr.SetAllowLowerBodyToTurn(state.bAllowLowerBodyToTurn);
		mr.SetBodyOrientationMode(state.bodyOrientationMode);

		if (fabsf(state.lean) > 0.01f)
			mr.SetLean(state.lean);
		else
			mr.ClearLean();

		if (fabsf(state.peekOver) > 0.01f)
			mr.SetPeekOver(state.peekOver);
		else
			mr.ClearPeekOver();

		// it is possible that we have a move target but zero speed, in which case we want to turn
		// on the spot.
		CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
		bool movementAllowed = !fireBlocking && !IsAnimationBlockingMovement() && !GetActorIsFallen();

		bool wantsToMove = false;
		if (movementAllowed)
		{
			wantsToMove = !state.vMoveDir.IsZero() && (state.fDesiredSpeed > 0.0f);
		}

		if (!wantsToMove)
		{
			mr.ClearMoveTarget();
			mr.SetDesiredSpeed(0.0f);
			mr.SetPseudoSpeed(state.curActorTargetPhase == eATP_Waiting ? fabs(state.fMovementUrgency) : 0.0f);
			mr.ClearPrediction();
		}
		else
		{
			/*
			   ANIM-UNSUPPORTED 2+
			   Give AI the opportunity to pass prediction??
			   if (state.predictedCharacterStates.nStates > 0)
			   {
			   int num = min((int) state.predictedCharacterStates.nStates, (int) SPredictedCharacterStates::maxStates);
			   SPredictedCharacterStates predStates;
			   predStates.nStates = num;
			   pe_status_pos status;
			   GetPhysics()->GetStatus(&status);
			   for (int iPred = 0 ; iPred != num ; ++iPred)
			   {
			    SAIPredictedCharacterState &pred = state.predictedCharacterStates.states[iPred];
			    predStates.states[iPred].deltatime = pred.predictionTime;
			    predStates.states[iPred].position = pred.position;
			    predStates.states[iPred].velocity = pred.velocity;
			    //predStates.states[iPred].rotation.SetIdentity();
			    if (state.allowStrafing || pred.velocity.IsZero(0.01f))
			      predStates.states[iPred].orientation = status.q;
			    else
			      predStates.states[iPred].orientation.SetRotationV0V1(Vec3(0, 1, 0), pred.velocity.GetNormalized());
			   }
			   mr.SetPrediction(predStates);
			   }
			   else*/
			{
				mr.ClearPrediction();
			}

			// If no real move target is available
			if (state.vMoveTarget.IsZero())
			{
				// OLD behavior: Estimate a future position (based on velocity)
				mr.SetMoveTarget(currentPos + state.vMoveDir.GetNormalized() * min(3.0f, state.fDistanceToPathEnd));
				mr.SetDirectionOffFromPath(state.vDirOffPath);
				mr.ClearInflectionPoint();
			}
			else
			{
				// NEW behavior: Provide exact move target and inflection point based on real PathFollower data.
				if (state.continuousMotion)
				{
					mr.SetMoveTarget(state.vMoveDir * 5.0f + currentPos);
				}
				else
				{
					mr.SetMoveTarget(state.vMoveTarget);
				}
				mr.SetInflectionPoint(state.vInflectionPoint);
				mr.SetDirectionOffFromPath(state.vDirOffPath);
			}

			if (fabs(state.fDesiredSpeed * state.fMovementUrgency) > 0.00001f)
			{
				float speedMul = (float)__fsel(state.fMovementUrgency, 1.0f, -1.0f);
				mr.SetDesiredSpeed(speedMul * state.fDesiredSpeed, speedMul * state.fTargetSpeed);
				mr.SetPseudoSpeed(fabs(state.fMovementUrgency));
			}
			else
			{
				mr.SetDesiredSpeed(0.0f, 0.0f);
				mr.SetPseudoSpeed(0.0f);
			}

#if DEBUG_VELOCITY()
			if (pActor)
			{
				if (const IAnimatedCharacter* pAnimChar = pActor->GetAnimatedCharacter())
				{
					if (pAnimChar->DebugVelocitiesEnabled())
					{
						const Vec3 entityPos = pActor->GetEntity()->GetPos();

						// In the following we assume 2D movement. This makes the debug info consistent
						// with the debug info coming from the MovementController (etc), which is 2D too.
						const Vec2 toMoveTarget = Vec2(mr.GetMoveTarget()) - Vec2(entityPos);
						const float distToMoveTarget = toMoveTarget.GetLength();
						const Vec2 dirToMoveTarget = (distToMoveTarget > 0.01f) ? toMoveTarget / distToMoveTarget : Vec2(ZERO);

						const float frameTime = (float)gEnv->pTimer->GetFrameTime();
						const QuatT movement(dirToMoveTarget * (mr.GetDesiredSpeed() * frameTime), Quat(IDENTITY));

						if (state.vMoveTarget.IsZero())
							pAnimChar->AddDebugVelocity(movement, frameTime, "AIProxy Output (MoveDir)", Col_Plum, false);
						else
							pAnimChar->AddDebugVelocity(movement, frameTime, "AIProxy Output (MoveTarget)", Col_Orchid, false);
					}
				}
			}
#endif
		}

		mr.SetDistanceToPathEnd(state.fDistanceToPathEnd);

		mr.SetStance((EStance)state.bodystate);

		mr.SetAllowStrafing(state.allowStrafing);

		m_pAIHandler->HandleExactPositioning(state, mr);

		if (state.jump)
			mr.SetJump();

		if (state.aimTargetIsValid && !state.vAimTargetPos.IsZero())
			mr.SetAimTarget(state.vAimTargetPos);
		else
			mr.ClearAimTarget();

		if (fire || fireSec || eAIFS_On == state.fireMelee)
			mr.SetFireTarget(state.vShootTargetPos);
		else
			mr.ClearFireTarget();

		if (state.vForcedNavigation.IsZero())
		{
			mr.ClearForcedNavigation();
		}
		else
		{
			mr.SetForcedNavigation(state.vForcedNavigation);
			mr.SetDesiredSpeed(state.fForcedNavigationSpeed);

#if DEBUG_VELOCITY()
			if (pActor)
			{
				if (CAnimatedCharacter* pAnimChar = static_cast<CAnimatedCharacter*>(pActor->GetAnimatedCharacter()))
				{
					if (pAnimChar->DebugVelocitiesEnabled())
					{
						float frameTime = (float)gEnv->pTimer->GetFrameTime();
						QuatT movement(state.vForcedNavigation * state.fForcedNavigationSpeed * frameTime, Quat(IDENTITY));
						pAnimChar->AddDebugVelocity(movement, frameTime, "AIProxy Forced Navigation", Col_Salmon, false);
					}
				}
			}
#endif
		}

		if (state.vBodyTargetDir.IsZero())
			mr.ClearBodyTarget();
		else
			mr.SetBodyTarget(currentPos + state.vBodyTargetDir);

		if (state.vDesiredBodyDirectionAtTarget.IsZero())
			mr.ClearDesiredBodyDirectionAtTarget();
		else
			mr.SetDesiredBodyDirectionAtTarget(state.vDesiredBodyDirectionAtTarget);

		if (!state.movementContext)
			mr.ClearContext();
		else
			mr.SetContext(state.movementContext);

		m_pAIHandler->HandleCoverRequest(state, mr);

		m_pAIHandler->HandleMannequinRequest(state, mr);

		// try, and retry once
		if (!pMC->RequestMovement(mr))
			pMC->RequestMovement(mr);
	}

	UpdateShooting(state, isAlive);

	return 0;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateShooting(const SOBJECTSTATE& state, bool isAlive)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_WeaponShotIsDone = false;

	bool fire = (eAIFS_On == state.fire && !state.aimObstructed);//|| (m_StartWeaponSpinUpTime>0);
	bool fireSec = (eAIFS_On == state.fireSecondary);
	if (!isAlive || !m_CurrentWeaponCanFire)
	{
		fire = false;
		fireSec = false;
	}

	bool hasFireControl = true;
	IEntity* pEntity = m_pGameObject->GetEntity();
	if (pEntity)
	{
		IPuppet* pPuppet = CastToIPuppetSafe(pEntity->GetAI());
		if (pPuppet)
			hasFireControl = pPuppet->IsFireEnabled();
	}

	EntityId currentWeaponId = 0;
	IWeapon* pWeapon = GetCurrentWeapon(currentWeaponId);

	if (eAIFS_On == state.fireMelee)
	{
		if (isAlive && (pWeapon != NULL) && hasFireControl)
		{
			enum MeleeType
			{
				ShortMelee,
				LongMelee
			};

			MeleeType meleeType = ShortMelee;

			if (IAIObject* aiObject = m_pAIHandler->m_pEntity->GetAI())
			{
				if (IAIActor* aiActor = aiObject->CastToIAIActor())
				{
					bool continuousMotion = aiActor->GetState().continuousMotion;

					if (continuousMotion)
					{
						bool moving = aiObject->GetVelocity().GetLengthSquared() > square(0.5f);
						meleeType = moving ? LongMelee : ShortMelee;
					}
					else
					{
						meleeType = IsInCloseMeleeRange(aiObject, aiActor) ? ShortMelee : LongMelee;
					}
				}
			}

			if (!IsAnimationBlockingMovement())
			{
				pWeapon->MeleeAttack(meleeType == ShortMelee);
			}
		}

		fire = false;
		fireSec = false;
	}

	//---------------------------------------------------------------------------
	if (pWeapon)
	{
		IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
		if (pFireMode)
			m_firing = pFireMode->IsFiring();
		bool raise = state.aimObstructed;
		pWeapon->Query(eWQ_Raise_Weapon, (const void*)&raise);
	}

	// firing control
	if (fire && !m_firing && hasFireControl)
	{
		if (pWeapon && pWeapon->CanFire())
		{
			SProjectileLaunchParams launchParams;
			launchParams.fSpeedScale = state.projectileInfo.fSpeedScale;
			launchParams.trackingId = state.projectileInfo.trackingId;
			launchParams.vShootTargetPos = state.vShootTargetPos;
			pWeapon->StartFire(launchParams);
			m_firing = true;
		}
		else if (IFireController* pFireController = GetFireController())
		{
			m_firing = true;
			pFireController->RequestFire(m_firing);
			pFireController->UpdateTargetPosAI(state.vAimTargetPos);
		}
	}
	else if (!fire && m_firing)
	{
		if (pWeapon && pWeapon->CanStopFire())
		{
			pWeapon->StopFire();
			m_firing = false;
		}
		else
		{
			if (IFireController* pFireController = GetFireController())
			{
				m_firing = false;
				pFireController->RequestFire(m_firing);
			}
		}
	}

	if (IFireController* pFireController = GetFireController())
	{
		pFireController->UpdateTargetPosAI(state.vAimTargetPos);
	}

	// secondary weapon firing control
	if (fireSec && !m_firingSecondary && hasFireControl)
	{
		ERequestedGrenadeType returnedGrenadeType = eRGT_INVALID;
		if (IWeapon* pSecWeapon = GetSecWeapon(state.requestedGrenadeType, &returnedGrenadeType))
		{
			SProjectileLaunchParams launchParams;
			launchParams.fSpeedScale = state.projectileInfo.fSpeedScale;
			launchParams.trackingId = state.projectileInfo.trackingId;
			launchParams.vShootTargetPos = state.vShootTargetPos;
			pSecWeapon->StartFire(launchParams);

			m_firingSecondary = true;

			IEntity* pEnt = m_pGameObject->GetEntity();
			if (pEnt)
			{

				switch (returnedGrenadeType)
				{
				case eRGT_SMOKE_GRENADE:
					SendSignal(AISIGNAL_DEFAULT, "OnSmokeGrenadeThrown", pEnt, NULL);
					break;
				case eRGT_FLASHBANG_GRENADE:
					SendSignal(AISIGNAL_DEFAULT, "OnFlashbangGrenadeThrown", pEnt, NULL);
					break;
				case eRGT_FRAG_GRENADE:
					SendSignal(AISIGNAL_DEFAULT, "OnFragGrenadeThrown", pEnt, NULL);
					break;
				case eRGT_EMP_GRENADE:
					SendSignal(AISIGNAL_DEFAULT, "OnEMPGrenadeThrown", pEnt, NULL);
					break;
				}

				// If not specific grenade type was requested, just
				// signal to self to inform grenade is now being thrown
				IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
				pData->iValue = state.requestedGrenadeType;
				pData->fValue = state.projectileInfo.fSpeedScale;
				pData->point = state.vShootTargetPos;
				SendSignal(AISIGNAL_DEFAULT, "OnGrenadeThrown", pEnt, pData);

				gEnv->pAISystem->FreeSignalExtraData(pData);
			}
		}
		else if (!m_firingSecondary)
		{
			uint32 index = state.secondaryIndex;

			while (index & 0xf)
			{
				if (IFireController* pFireController = GetFireController(index & 0xf))
				{
					pFireController->RequestFire(fireSec);
					pFireController->UpdateTargetPosAI(state.vAimTargetPos);
				}

				index >>= 4;
			}

			m_firingSecondary = true;
		}
	}
	else if (!fireSec && m_firingSecondary)
	{
		uint32 index = 1;
		while (IFireController* pFireController = GetFireController(index))
		{
			pFireController->RequestFire(fireSec);
			++index;
		}

		m_firingSecondary = false;
	}
	else if (m_firingSecondary)
	{
		uint32 index = state.secondaryIndex;
		while (index & 0xf)
		{
			if (IFireController* pFireController = GetFireController(index & 0xf))
			{
				pFireController->UpdateTargetPosAI(state.vAimTargetPos);
			}

			index >>= 4;
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::QueryWeaponInfo(SAIWeaponInfo& weaponInfo)
{
	EntityId currentWeaponId = 0;
	IWeapon* pWeapon(QueryCurrentWeapon(currentWeaponId));
	if (!pWeapon)
		return;

	weaponInfo.canMelee = pWeapon->CanMeleeAttack();
	weaponInfo.outOfAmmo = pWeapon->OutOfAmmo(false);
	weaponInfo.lowAmmo = pWeapon->LowAmmo(0.15f);
	weaponInfo.shotIsDone = m_WeaponShotIsDone;
	weaponInfo.lastShotTime = m_lastShotTime;

#ifdef AI_G4
	weaponInfo.hasLightAccessory = weaponInfo.hasLaserAccessory = false;
	weaponInfo.hasLightAccessory = weaponInfo.hasLightAccessory = false;
#else
	weaponInfo.hasLaserAccessory = pWeapon->Query(eWQ_Has_Accessory_Laser);
	weaponInfo.hasLightAccessory = pWeapon->Query(eWQ_Has_Accessory_Flashlight);
#endif

	if (IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
	{
		weaponInfo.ammoCount = pFireMode->GetAmmoCount();
		weaponInfo.clipSize = pFireMode->GetClipSize();
		weaponInfo.isReloading = pFireMode ? pFireMode->IsReloading() : false;
		weaponInfo.isFiring = pFireMode ? pFireMode->IsFiring() : false;
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateCurrentWeapon()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IWeapon* pCurrentWeapon = NULL;
	EntityId currentWeaponId = 0;
	m_CurrentWeaponCanFire = true;

	IItemSystem* pItemSystem = CCryAction::GetCryAction()->GetIItemSystem();
	//assert(pItemSystem);

	IActor* pActor = GetActor();
	if (pActor)
	{
		IItem* pItem = NULL;

		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			// mounted weapon( gunner seat )
			SVehicleWeaponInfo weaponInfo;
			if (pVehicle->GetCurrentWeaponInfo(weaponInfo, m_pGameObject->GetEntityId(), m_UseSecondaryVehicleWeapon))
			{
				pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(weaponInfo.entityId);
				m_CurrentWeaponCanFire = weaponInfo.bCanFire;
			}
		}
		else
		{
			pItem = pActor->GetCurrentItem();
		}

		pCurrentWeapon = pItem ? pItem->GetIWeapon() : NULL;
		currentWeaponId = (pCurrentWeapon ? pItem->GetEntityId() : 0);
	}

	bool updateCachedWeaponDescriptor = false;

	const bool weaponChanged = m_currentWeaponId != currentWeaponId;
	if (weaponChanged)
	{
		updateCachedWeaponDescriptor = true;

		IItem* pOldItem = pItemSystem->GetItem(m_currentWeaponId);
		IWeapon* pOldWeapon = pOldItem ? pOldItem->GetIWeapon() : NULL;
		if (pOldWeapon)
			pOldWeapon->StopFire();

		m_currentWeaponId = currentWeaponId;
	}
	else
	{
		const int currentWeaponFiremodeIndex = pCurrentWeapon ? pCurrentWeapon->GetCurrentFireMode() : -1;
		const bool fireModeChanged = (m_currentWeaponFiremodeIndex != currentWeaponFiremodeIndex);
		if (fireModeChanged)
		{
			updateCachedWeaponDescriptor = true;
			m_currentWeaponFiremodeIndex = currentWeaponFiremodeIndex;
		}
	}

	if (updateCachedWeaponDescriptor)
	{
		m_currentWeaponDescriptor = pCurrentWeapon ? pCurrentWeapon->GetAIWeaponDescriptor() : AIWeaponDescriptor();
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::EnableWeaponListener(const EntityId weaponId, bool needsSignal)
{
	m_shotBulletCount = 0;
	m_lastShotTime.SetSeconds(0.0f);
	m_NeedsShootSignal = needsSignal;

	if (IWeapon* pWeapon = GetWeaponFromId(weaponId))
	{
		if (m_weaponListenerId != 0)
		{
			RemoveWeaponListener(GetWeaponFromId(m_weaponListenerId));
		}

		pWeapon->AddEventListener(this, __FUNCTION__);
		m_weaponListenerId = weaponId;
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
                       const Vec3& pos, const Vec3& dir, const Vec3& vel)
{
	IEntity* pEntity = m_pGameObject->GetEntity();
	if (!pEntity)
	{
		AIWarningID(">> ", "CAIProxy::OnShoot null entity");
		return;
	}

	//    if (pEntity->GetAI() && pEntity->GetAI()->GetAIType() != AIOBJECT_PLAYER)
	//      gEnv->pAISystem->DebugDrawFakeTracer(pos, dir);

	m_shotBulletCount++;
	m_lastShotTime = gEnv->pTimer->GetFrameStartTime();

	m_WeaponShotIsDone = true;
	IPuppet* pPuppet = CastToIPuppetSafe(pEntity->GetAI());
	if (pPuppet && pPuppet->GetFirecommandHandler())
		pPuppet->GetFirecommandHandler()->OnShoot();

	if (!m_NeedsShootSignal)
		return;
	SendSignal(0, "WPN_SHOOT", pEntity, NULL);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnDropped(IWeapon* pWeapon, EntityId actorId)
{
	m_NeedsShootSignal = false;
	RemoveWeaponListener(pWeapon);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnMelee(IWeapon* pWeapon, EntityId shooterId)
{
	IEntity* pEntity = m_pGameObject->GetEntity();
	assert(pEntity);
	if (pEntity)
		SendSignal(0, "WPN_MELEE", pEntity, NULL);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnSelected(IWeapon* pWeapon, bool selected)
{
	if (!selected)
	{
		m_NeedsShootSignal = false;
		RemoveWeaponListener(pWeapon);
	}
}
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnEndBurst(IWeapon* pWeapon, EntityId actorId)
{
	IEntity* pEntity = m_pGameObject->GetEntity();
	assert(pEntity);
	if (!pEntity)
	{
		AIWarningID(">> ", "CAIProxy::OnEndBurst null entity");
		return;
	}
	SendSignal(0, "WPN_END_BURST", pEntity, NULL);
}

//
//----------------------------------------------------------------------------------------------------------
IPhysicalEntity* CAIProxy::GetPhysics(bool wantCharacterPhysics)
{
	if (wantCharacterPhysics)
	{
		ICharacterInstance* pCharacter(m_pGameObject->GetEntity()->GetCharacter(0));
		if (pCharacter)
			return pCharacter->GetISkeletonPose()->GetCharacterPhysics();
		return NULL;
	}
	return m_pGameObject->GetEntity()->GetPhysics();
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::DebugDraw(int iParam)
{
	if (!m_pAIHandler)
		return;

	IAIDebugRenderer* pDebugRenderer = gEnv->pAISystem->GetAIDebugRenderer();

	if (iParam == 1)
	{
		// Stats target drawing.
		pDebugRenderer->TextToScreenColor(50, 66, 0.3f, 0.3f, 0.3f, 1.f, "- Proxy Information --");

		const char* szCurrentBehaviour = 0;
		const char* szPreviousBehaviour = 0;
		const char* szFirstBehaviour = 0;

		if (m_pAIHandler->m_pBehavior.GetPtr())
			m_pAIHandler->m_pBehavior->GetValue("name", szCurrentBehaviour) || m_pAIHandler->m_pBehavior->GetValue("Name", szCurrentBehaviour);

		if (m_pAIHandler->m_pPreviousBehavior.GetPtr())
			m_pAIHandler->m_pPreviousBehavior->GetValue("name", szCurrentBehaviour) || m_pAIHandler->m_pPreviousBehavior->GetValue("Name", szCurrentBehaviour);

		if (!m_pAIHandler->m_sFirstBehaviorName.empty())
			szFirstBehaviour = m_pAIHandler->m_sFirstBehaviorName.c_str();

		pDebugRenderer->TextToScreen(50, 74, "BEHAVIOUR: %s", szCurrentBehaviour);
		pDebugRenderer->TextToScreen(50, 76, " PREVIOUS BEHAVIOUR: %s", szPreviousBehaviour);
		pDebugRenderer->TextToScreen(50, 78, " DESIGNER ASSIGNED BEHAVIOUR: %s", szFirstBehaviour);

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		const char* szCurrentCharacter = m_pAIHandler->m_sCharacterName.c_str();
		const char* szPreviousCharacter = m_pAIHandler->m_sPrevCharacterName.c_str();
		const char* szFirstCharacter = m_pAIHandler->m_sFirstCharacterName.c_str();

		pDebugRenderer->TextToScreen(50, 82, "CHARACTER: %s", szCurrentCharacter);
		pDebugRenderer->TextToScreen(50, 84, " PREVIOUS CHARACTER: %s", szPreviousCharacter);
		pDebugRenderer->TextToScreen(50, 86, " DESIGNER ASSIGNED CHARACTER: %s", szFirstCharacter);
#endif

		if (m_pAIHandler)
		{
			const char* szEPMsg = " - ";
			switch (GetActorTargetPhase())
			{
			//			case eATP_Cancel: szEPMsg = "Cancel"; break;
			case eATP_None:
				szEPMsg = " - ";
				break;
			//			case eATP_Request: szEPMsg = "Request"; break;
			case eATP_Waiting:
				szEPMsg = "Waiting";
				break;
			case eATP_Starting:
				szEPMsg = "Starting";
				break;
			case eATP_Started:
				szEPMsg = "Started";
				break;
			case eATP_Playing:
				szEPMsg = "Playing";
				break;
			case eATP_StartedAndFinished:
				szEPMsg = "Started/Finished";
				break;
			case eATP_Finished:
				szEPMsg = "Finished";
				break;
			case eATP_Error:
				szEPMsg = "Error";
				break;
			}

			pDebugRenderer->TextToScreen(50, 90, "ANIMATION: %s", IsAnimationBlockingMovement() ? "MOVEMENT BLOCKED!" : "");
			pDebugRenderer->TextToScreen(50, 92, "           ExactPos:%s", szEPMsg);
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::Reset(EObjectResetType type)
{
	ReloadScriptProperties();

	m_forcedExecute = 0;
	EnableUpdate(type == AIOBJRESET_INIT);
	m_shotBulletCount = 0;
	m_lastShotTime.SetSeconds(0.0f);

	m_CurrentWeaponCanFire = true;
	m_UseSecondaryVehicleWeapon = false;

	m_currentWeaponId = 0;
	m_currentWeaponFiremodeIndex = std::numeric_limits<int>::max();
	m_currentWeaponDescriptor = AIWeaponDescriptor();

	//if (gEnv->IsEditor())
	if (m_pGameObject)
		m_pGameObject->SetAIActivation(eGOAIAM_VisibleOrInRange); // we suppose the first update will set this properly.

	if (m_commHandler.get())
		m_commHandler->Reset();

	if (m_pAIHandler)
		m_pAIHandler->Reset(type);

	// Notify signaler handlers
	CRY_ASSERT(m_pGameObject);
	if (m_pGameObject)
	{
		IMovementController* pMC = m_pGameObject->GetMovementController();
		if (pMC)
		{
			CMovementRequest mr;
			mr.SetAlertness(0);

			// [DAVID] Clear Actor/AC alertness
			IActor* pActor = GetActor();
			if (pActor != NULL)
				pActor->SetFacialAlertnessLevel(0);

			mr.ClearLookTarget();
			mr.ClearActorTarget();
			mr.ClearLean();
			mr.ClearPeekOver();
			mr.ClearPrediction();
			// try, and retry once
			if (!pMC->RequestMovement(mr))
				pMC->RequestMovement(mr);
		}

		CCryAction::GetCryAction()->GetRangeSignaling()->OnProxyReset(m_pGameObject->GetEntityId());
		CCryAction::GetCryAction()->GetSignalTimer()->OnProxyReset(m_pGameObject->GetEntityId());
	}

	m_aimQueryMode = QueryAimFromMovementController;
}

IAICommunicationHandler* CAIProxy::GetCommunicationHandler()
{
	return m_commHandler.get();
}

bool CAIProxy::BecomeAggressiveToAgent(EntityId agentID)
{
	if (IActor* pActor = GetActor())
		return pActor->BecomeAggressiveToAgent(agentID);
	else
		return false;
}

void CAIProxy::SetForcedExecute(bool forced)
{
	m_forcedExecute += forced ? 1 : -1;
	assert(m_forcedExecute >= 0); // Below zero? More disables then enables!
	assert(m_forcedExecute < 16); // Just a little sanity check

	if (IEntity* pEntity = m_pGameObject->GetEntity())
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if (pAIActor)
		{
			if (forced)
				pAIActor->SetSignal(10, "OnForcedExecute", pEntity);
			else if (m_forcedExecute <= 0)
				pAIActor->SetSignal(10, "OnForcedExecuteComplete", pEntity);
		}
	}
}

bool CAIProxy::IsForcedExecute() const
{
	return m_forcedExecute > 0;
}

//
//----------------------------------------------------------------------------------------------------------
CTimeValue CAIProxy::GetEstimatedAGAnimationLength(EAIAGInput input, const char* value)
{
	if (m_pAIHandler)
		return m_pAIHandler->GetEstimatedAGAnimationLength(input, value);
	else
		return CTimeValue();
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::SetAGInput(EAIAGInput input, const char* value, const bool isUrgent)
{
	return (m_pAIHandler ? m_pAIHandler->SetAGInput(input, value, isUrgent) : false);
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::ResetAGInput(EAIAGInput input)
{
	return (m_pAIHandler ? m_pAIHandler->ResetAGInput(input) : false);
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsSignalAnimationPlayed(const char* value)
{
	return (m_pAIHandler ? m_pAIHandler->IsSignalAnimationPlayed(value) : true);
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsActionAnimationStarted(const char* value)
{
	return (m_pAIHandler ? m_pAIHandler->IsActionAnimationStarted(value) : true);
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsAnimationBlockingMovement() const
{
	bool blocking = false;

	if (m_pAIHandler)
		blocking |= m_pAIHandler->IsAnimationBlockingMovement();

	if (m_commHandler.get() && m_commHandler->IsPlayingAnimation())
	{
		IActor* pActor = const_cast<IActor*>(GetActor());
		if (pActor)
		{
			const IAnimatedCharacter* pAnimChar = pActor->GetAnimatedCharacter();
			blocking |= pAnimChar && ((pAnimChar->GetMCMH() == eMCM_Animation) || (pAnimChar->GetMCMH() == eMCM_AnimationHCollision));
		}
	}

	return blocking;
}

//
//----------------------------------------------------------------------------------------------------------
EActorTargetPhase CAIProxy::GetActorTargetPhase() const
{
	return (m_pAIHandler ? m_pAIHandler->GetActorTargetPhase() : eATP_None);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::PlayAnimationAction(const IAIAction* pAction, int goalPipeId)
{
	CRY_ASSERT(pAction->GetAnimationName() != NULL && pAction->GetAnimationName()[0] != 0);

	if (m_animActionGoalPipeId != 0)
		gEnv->pAISystem->GetAIActionManager()->AbortAIAction(m_pGameObject->GetEntity(), m_animActionGoalPipeId);
	m_animActionGoalPipeId = goalPipeId;

	IActor* pActor = GetActor();
	if (pActor != NULL)
	{
		if (pAction->IsExactPositioning())
			pActor->PlayExactPositioningAnimation(pAction->GetAnimationName(), pAction->IsSignaledAnimation(), pAction->GetAnimationPos(),
			                                      pAction->GetAnimationDir(), pAction->GetStartWidth(), pAction->GetStartArcAngle(), DEG2RAD(pAction->GetDirectionTolerance()));
		else
			pActor->PlayAnimation(pAction->GetAnimationName(), pAction->IsSignaledAnimation());
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::AnimationActionDone(bool succeeded)
{
	if (m_animActionGoalPipeId)
	{
		int tmp = m_animActionGoalPipeId;
		m_animActionGoalPipeId = 0;
		if (succeeded)
			gEnv->pAISystem->GetAIActionManager()->FinishAIAction(m_pGameObject->GetEntity(), tmp);
		else
			gEnv->pAISystem->GetAIActionManager()->AbortAIAction(m_pGameObject->GetEntity(), tmp);
	}
}

bool CAIProxy::IsPlayingSmartObjectAction() const
{
	bool bResult = false;

	// TODO: Users should probably query the IPipeUser directly
	if (IEntity* pEntity = m_pGameObject->GetEntity())
	{
		if (IAIObject* pAIObject = pEntity->GetAI())
		{
			if (IPipeUser* pPipeUser = pAIObject->CastToIPipeUser())
			{
				bResult = pPipeUser->IsUsingNavSO();
			}
		}
	}

	return bResult;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateMind(SOBJECTSTATE& state)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	UpdateCurrentWeapon();

	// (MATT) Run delayed constructors on behaviour and character and update face {2008/09/05}
	if (m_pAIHandler)
		m_pAIHandler->Update();

	// (MATT) For signals that should wait until next update, mainly used by Troopers {2008/09/05}
	std::vector<AISIGNAL> nextUpdateSignals;

	if (m_pGameObject->GetEntity()->GetAI()->IsUpdatedOnce())
	{
		while (!state.vSignals.empty())
		{
			AISIGNAL sstruct = state.vSignals.front();
			state.vSignals.erase(0);

			int signal = sstruct.nSignal;
			if (signal == AISIGNAL_PROCESS_NEXT_UPDATE)
			{
				sstruct.nSignal = AISIGNAL_RECEIVED_PREV_UPDATE;
				nextUpdateSignals.push_back(sstruct);
			}
			else
			{
				const char* szText = sstruct.strText;
				const uint32 crc = sstruct.m_nCrcText;
				IEntity* pSender = gEnv->pEntitySystem->GetEntity(sstruct.senderID);
				SendSignal(signal, szText, pSender, sstruct.pEData, sstruct.m_nCrcText);

				if (sstruct.pEData)
					gEnv->pAISystem->FreeSignalExtraData(sstruct.pEData);
			}
		}

		std::vector<AISIGNAL>::iterator it = nextUpdateSignals.begin(), itEnd = nextUpdateSignals.end();
		for (; it != itEnd; ++it)
			state.vSignals.push_back(*it);
	}
	else
	{
		// (MATT) Process init signals before first update - is this really needed? {2008/09/05}
		int size = state.vSignals.size();
		int i = 0;
		while (i < size)
		{
			AISIGNAL signal = state.vSignals[i];

			if (signal.nSignal == AISIGNAL_INITIALIZATION)
			{
				state.vSignals.erase(state.vSignals.begin() + i);
				gEnv->pLog->LogError("Initialisation signal used \"%s\"", signal.strText);

				// process only init. signals!
				IEntity* pSender = gEnv->pEntitySystem->GetEntity(signal.senderID);
				SendSignal(signal.nSignal, signal.strText, pSender, signal.pEData);
				if (signal.pEData)
					gEnv->pAISystem->FreeSignalExtraData(signal.pEData);

				// TO DO: check if this is still necessary once the newly inserted signals will be processed on the next update
				if (--size != state.vSignals.size())
				{
					// a new signal was inserted while processing this one
					// so, let's start over
					size = state.vSignals.size();
				}
			}
			else
				i++;
		}
	}

	if (!m_pAIHandler)
		return;

	m_pAIHandler->AIMind(state);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnReused(IEntity* pEntity, SEntitySpawnParams& params)
{
	m_pGameObject = CCryAction::GetCryAction()->GetGameObject(params.id);
	m_pIActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(params.id);

	EnableUpdate(false);

	ReloadScriptProperties();

	if (m_commHandler.get())
		m_commHandler->OnReused(pEntity);

	// Reuse the handler
	if (m_pAIHandler)
		m_pAIHandler->OnReused(m_pGameObject);

	// Reset the AI - this will also reset the AIProxy later
	IAIObject* pAI = pEntity->GetAI();
	if (pAI)
	{
		/*pAI->SetEntityID(params.id);
		   pAI->SetName(pEntity->GetName());*/
		pAI->Reset(AIOBJRESET_INIT);
	}

	if (m_weaponListenerId != 0)
	{
		RemoveWeaponListener(GetWeaponFromId(m_weaponListenerId));
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::ReloadScriptProperties()
{
	if (IScriptTable* script = m_pGameObject->GetEntity()->GetScriptTable())
	{
		SmartScriptTable props;
		if (script->GetValue("Properties", props))
		{
			const char* voice;
			if (props->GetValue("esVoice", voice))
			{
				m_voiceLibrary = voice;
				CreateTestVoiceLibraryNameFromBasicName(m_voiceLibrary, m_voiceLibraryWhenForcingTest);
			}

			const char* commConfigName;
			if (props->GetValue("esCommConfig", commConfigName))
				m_commConfigName = commConfigName;

			props->GetValue("fFmodCharacterTypeParam", m_FmodCharacterTypeParam);

			const char* agentTypeName;
			if (props->GetValue("esNavigationType", agentTypeName)
			    && *agentTypeName)
				m_agentTypeName = agentTypeName;
		}
		SmartScriptTable instanceProps;
		if (script->GetValue("PropertiesInstance", instanceProps))
		{
			// Check if some properties are overridden in the instance
			const char* voice;
			if (instanceProps->GetValue("esVoice", voice) && strcmp(voice, "") != 0)
			{
				m_voiceLibrary = voice;
				CreateTestVoiceLibraryNameFromBasicName(m_voiceLibrary, m_voiceLibraryWhenForcingTest);
			}
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
IActor* CAIProxy::GetActor() const
{
	if (!m_pIActor)
		m_pIActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_pGameObject->GetEntityId());
	return m_pIActor;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnActorRemoved()
{
	m_pIActor = nullptr;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::SendSignal(int signalID, const char* szText, IEntity* pSender, const IAISignalExtraData* pData, uint32 crc /* = 0u */)
{
	IF_UNLIKELY (crc == 0u)
	{
		crc = CCrc32::Compute(szText);
	}

	if (m_pAIHandler)
	{
		m_pAIHandler->AISignal(signalID, szText, crc, pSender, pData);
	}
}

//
// gets the corners of the tightest projected bounding rectangle in 2D world space coordinates
//----------------------------------------------------------------------------------------------------------
void CAIProxy::GetWorldBoundingRect(Vec3& FL, Vec3& FR, Vec3& BL, Vec3& BR, float extra) const
{
	const Vec3 pos = m_pGameObject->GetEntity()->GetWorldPos();
	const Quat& quat = m_pGameObject->GetEntity()->GetRotation();
	Vec3 dir = quat * Vec3(0, 1, 0);
	dir.z = 0.0f;
	dir.NormalizeSafe();

	const Vec3 sideDir(dir.y, -dir.x, 0.0f);

	IEntityRender* pIEntityRender = m_pGameObject->GetEntity()->GetRenderInterface();
	AABB bounds;
	pIEntityRender->GetLocalBounds(bounds);

	bounds.max.x += extra;
	bounds.max.y += extra;
	bounds.min.x -= extra;
	bounds.min.y -= extra;

	FL = pos + bounds.max.y * dir + bounds.min.x * sideDir;
	FR = pos + bounds.max.y * dir + bounds.max.x * sideDir;
	BL = pos + bounds.min.y * dir + bounds.min.x * sideDir;
	BR = pos + bounds.min.y * dir + bounds.max.x * sideDir;
}

//
//----------------------------------------------------------------------------------------------------------
int CAIProxy::GetAlertnessState() const
{
	return m_pAIHandler ? m_pAIHandler->m_CurrentAlertness : 0;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::SetAlertnessState(int alertness)
{
	if (m_pAIHandler)
		m_pAIHandler->SetAlertness(alertness);
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsCurrentBehaviorExclusive() const
{
	if (!m_pAIHandler)
		return false;
	return m_pAIHandler->m_CurrentExclusive;
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::SetCharacter(const char* character, const char* behavior)
{
	if (!m_pAIHandler)
		return true;
	bool ret = m_pAIHandler->SetCharacter(character);
	if (behavior)
		m_pAIHandler->SetBehavior(behavior);
	return ret;
}

//
//----------------------------------------------------------------------------------------------------------
const char* CAIProxy::GetCharacter()
{
	if (!m_pAIHandler)
		return 0;
	return m_pAIHandler->GetCharacter();
}
#endif

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::GetReadabilityBlockingParams(const char* text, float& time, int& id)
{
	time = 0;
	id = 0;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		m_pIActor = 0; // forces re-caching in QueryBodyInfo

		if (!m_pAIHandler)
		{
			m_pAIHandler = new CAIHandler(m_pGameObject);
			m_pAIHandler->Init();
		}
	}

	CRY_ASSERT(m_pAIHandler);

	ser.BeginGroup("AIProxy");

	m_pAIHandler->Serialize(ser);
	ser.Value("m_firing", m_firing);
	ser.Value("m_firingSecondary", m_firingSecondary);
	ser.Value("m_forcedExecute", m_forcedExecute);

	if (ser.IsReading())
	{
		m_currentWeaponId = 0;
		UpdateCurrentWeapon();
	}

	ser.EnumValue("m_aimQueryMode", m_aimQueryMode, FirstAimQueryMode, AimQueryModeCount);
	ser.Value("m_fMinFireTime", m_fMinFireTime);
	ser.Value("m_WeaponShotIsDone", m_WeaponShotIsDone);
	ser.Value("m_NeedsShootSignal", m_NeedsShootSignal);
	ser.Value("m_IsDisabled", m_IsDisabled);
	ser.Value("m_UpdateAlways", m_UpdateAlways);
	ser.Value("m_lastShotTime", m_lastShotTime);
	ser.Value("m_shotBulletCount", m_shotBulletCount);
	ser.Value("m_CurrentWeaponCanFire", m_CurrentWeaponCanFire);
	ser.Value("m_UseSecondaryVehicleWeapon", m_UseSecondaryVehicleWeapon);
	ser.Value("m_autoDeactivated", m_autoDeactivated);

	ser.EndGroup();
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::PredictProjectileHit(float vel, Vec3& posOut, Vec3& dirOut, float& speedOut, Vec3* pTrajectoryPositions, unsigned int* trajectorySizeInOut, Vec3* pTrajectoryVelocities)
{
	EntityId currentWeaponId = 0;
	IWeapon* pCurrentWeapon = GetCurrentWeapon(currentWeaponId);
	if (!pCurrentWeapon)
		return false;

	SAIBodyInfo bodyInfo;
	QueryBodyInfo(bodyInfo);

	return pCurrentWeapon->PredictProjectileHit(GetPhysics(), bodyInfo.vFirePos, dirOut, Vec3(0, 0, 0), vel,
	                                            posOut, speedOut, pTrajectoryPositions, trajectorySizeInOut, 0.24f, pTrajectoryVelocities, true);
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::PredictProjectileHit(const Vec3& throwDir, float vel, Vec3& posOut, float& speedOut, ERequestedGrenadeType prefGrenadeType,
                                    Vec3* pTrajectoryPositions, unsigned int* trajectorySizeInOut, Vec3* pTrajectoryVelocities)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	IWeapon* pGrenadeWeapon = GetSecWeapon(prefGrenadeType);
	if (!pGrenadeWeapon)
		return false;

	SAIBodyInfo bodyInfo;
	QueryBodyInfo(bodyInfo);

	return pGrenadeWeapon->PredictProjectileHit(GetPhysics(), bodyInfo.vFirePos, throwDir, Vec3(0, 0, 0), vel,
	                                            posOut, speedOut, pTrajectoryPositions, trajectorySizeInOut, 0.24f, pTrajectoryVelocities, true);
}

//
//---------------------------------------------------------------------------------------------------------------
const AIWeaponDescriptor& CAIProxy::GetCurrentWeaponDescriptor() const
{
	return m_currentWeaponDescriptor;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::GetSecWeaponDescriptor(AIWeaponDescriptor& outDescriptor, ERequestedGrenadeType prefGrenadeType) const
{
	const IWeapon* pGrenadeWeapon = GetSecWeapon(prefGrenadeType);
	if (pGrenadeWeapon)
	{
		outDescriptor = pGrenadeWeapon->GetAIWeaponDescriptor();
		return true;
	}

	return false;
}

//
//---------------------------------------------------------------------------------------------------------------
void CAIProxy::SetUseSecondaryVehicleWeapon(bool bUseSecondary)
{
	if (m_UseSecondaryVehicleWeapon != bUseSecondary)
	{
		m_UseSecondaryVehicleWeapon = bUseSecondary;
		UpdateCurrentWeapon();
	}
}

//
//---------------------------------------------------------------------------------------------------------------
const char* CAIProxy::GetCurrentBehaviorName() const
{
	const char* szName = NULL;

	if (m_pAIHandler->m_pBehavior.GetPtr())
	{
		m_pAIHandler->m_pBehavior->GetValue("name", szName) || m_pAIHandler->m_pBehavior->GetValue("Name", szName);
	}

	return(szName);
}

//
//---------------------------------------------------------------------------------------------------------------
const char* CAIProxy::GetPreviousBehaviorName() const
{
	const char* szName = NULL;

	if (m_pAIHandler->m_pPreviousBehavior.GetPtr())
	{
		m_pAIHandler->m_pPreviousBehavior->GetValue("name", szName) || m_pAIHandler->m_pPreviousBehavior->GetValue("Name", szName);
	}

	return(szName);
}

//
//---------------------------------------------------------------------------------------------------------------
float CAIProxy::GetActorHealth() const
{
	const IActor* pActor = GetActor();
	if (pActor != NULL)
		return pActor->GetHealth();

	const IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	const IVehicle* pVehicle = const_cast<IVehicleSystem*>(pVehicleSystem)->GetVehicle(m_pGameObject->GetEntityId());
	if (pVehicle)
		return ((1.0f - pVehicle->GetDamageRatio()) * 1000.0f);

	return 0.0f;
}

//
//---------------------------------------------------------------------------------------------------------------
float CAIProxy::GetActorMaxHealth() const
{
	const IActor* pActor = GetActor();
	if (pActor != NULL)
		return pActor->GetMaxHealth();

	const IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	const IVehicle* pVehicle = const_cast<IVehicleSystem*>(pVehicleSystem)->GetVehicle(m_pGameObject->GetEntityId());
	if (pVehicle)
		return 1000.0f;

	return 1.0f;
}

//
//---------------------------------------------------------------------------------------------------------------
int CAIProxy::GetActorArmor() const
{
	const IActor* pActor = GetActor();
	if (pActor != NULL)
		return pActor->GetArmor();
	return 0;
}

//
//---------------------------------------------------------------------------------------------------------------
int CAIProxy::GetActorMaxArmor() const
{
	const IActor* pActor = GetActor();
	if (pActor != NULL)
		return pActor->GetMaxArmor();
	return 0;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::GetActorIsFallen() const
{
	IActor* pActor = GetActor();
	if (pActor != NULL)
		return pActor->IsFallen();
	return false;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::IsDead() const
{
	if (const IActor* pActor = GetActor())
		return pActor->IsDead();

	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
	return pVehicle ? pVehicle->IsDestroyed() : false;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::SetBehaviour(const char* szBehavior, const IAISignalExtraData* pData)
{
	if (m_pAIHandler)
	{
		m_pAIHandler->SetBehavior(szBehavior, pData);
	}
}

//
//----------------------------------------------------------------------------------------------------------
float CAIProxy::LinearInterp(float x, float k, float A, float B, float C)
{
	// (MATT) Surely this is in CryCommon somewhere {2009/01/27}
	if (x < k)
	{
		x = x / k;
		return A + (B - A) * x;
	}
	else
	{
		x = (x - k) / (1.0f - k);
		return B + (C - B) * x;
	}
}

//
//----------------------------------------------------------------------------------------------------------
float CAIProxy::QuadraticInterp(float x, float k, float A, float B, float C)
{
	// (MATT) Surely this is in CryCommon somewhere {2009/01/27}
	float a = (A - B - A * k + C * k) / (k - k * k);
	float b = -(A - B - A * k * k + C * k * k) / (k - k * k);
	float c = A;

	if (x < 1.0f)
		return a * x * x + b * x + c;
	else
		return (2.0f * a + b) * x + c - a;
}

void CAIProxy::RemoveWeaponListener(IWeapon* pWeapon)
{
	if (pWeapon)
	{
		pWeapon->RemoveEventListener(this);
		m_weaponListenerId = 0;
	}
}

IWeapon* CAIProxy::GetWeaponFromId(EntityId entityId)
{
	IItem* pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(entityId);

	return pItem ? pItem->GetIWeapon() : NULL;
}

bool CAIProxy::IsInCloseMeleeRange(IAIObject* aiObject, IAIActor* aiActor) const
{
	if (IAIObject* target = aiActor->GetAttentionTarget())
	{
		float targetDistSq = (target->GetPos() - aiObject->GetPos()).GetLengthSquared();
		float shortMeleeThresholdSq = square(aiActor->GetParameters().m_fMeleeRangeShort);

		if (targetDistSq <= shortMeleeThresholdSq)
		{
			return true;
		}
	}

	return false;
}

namespace AISpeed // should move somewhere in Game code instead (and also be removed from SmartPathFollower)
{
// Adjust speed so it goes to zero when near a target
//
// Clamps speed to zero when within 20cm
// Tries not to overshoot the target, taking into account the frameTime
// ---------------------------------------------------------------------------
void AdjustSpeedWhenNearingTarget(const float speed, const float distToTarget, const float frameTime, float* const pAdjustedSpeed, bool* const pReachedEnd)
{
	const float MinDistanceToEnd = 0.2f;
	const float MaxTimeStep = 0.5f;

	// This is used to vaguely indicate if the FT has reached path end and so has the agent
	*pReachedEnd = distToTarget < max(MinDistanceToEnd, speed * min(frameTime, MaxTimeStep));

	float slowDownDist = 1.2f;
	// Scale speed down when within slowDownDist as a safety against instability
	// (magic numbers is to mimic the old behavior in the worst case; it reduced a speed
	// of about 7.5m/s, to 0.75m/s over the slowDownDist)
	*pAdjustedSpeed = *pReachedEnd ? 0.0f : min(speed, 0.75f + 7.0f * distToTarget / slowDownDist);
}
}

//
// ---------------------------------------------------------------------------
