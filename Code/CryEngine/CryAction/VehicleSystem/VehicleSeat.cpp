// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a standard seat for vehicles

   -------------------------------------------------------------------------
   History:
   - 14:04:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "IActorSystem.h"
#include "IAnimatedCharacter.h"
#include "IVehicleSystem.h"
#include "IViewSystem.h"
#include "CryAction.h"
#include "IItemSystem.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "IGameRulesSystem.h"
#include "PersistantDebug.h"
#include "CryActionCVars.h"

#include "VehicleSeat.h"
#include "VehicleSeatGroup.h"
#include "VehiclePartAnimated.h"
#include "Vehicle.h"

#include "VehicleSeatActionMovement.h"
#include "VehicleSeatActionRotateTurret.h"
#include "VehicleSeatActionWeapons.h"
#include "VehicleSeatSerializer.h"
#include "VehicleViewThirdPerson.h"
#include "VehicleUtils.h"

#include "../Network/GameContext.h"
#include "../Network/GameChannel.h"
#include "../Network/GameServerNub.h"
#include "../Network/GameServerChannel.h"

#include <CryAISystem/IPathfinder.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIObject.h>

#include "Animation/VehicleSeatAnimActions.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

CCryAction* CVehicleSeat::m_pGameFramework = NULL;

//------------------------------------------------------------------------
CVehicleSeat::CVehicleSeat()
	: m_exitOffsetPlayer(ZERO)
	, m_adjustedExitPos(ZERO)
	, m_exitWorldPos(ZERO)
	, m_pVehicle(nullptr)
	, m_pSerializer(nullptr)
	, m_pWeaponAction(nullptr)
	, m_pTurretAction(nullptr)
	, m_pSeatGroup(nullptr)
	, m_pRemoteSeat(nullptr)
	, m_pAimPart(nullptr)
	, m_pEnterHelper(nullptr)
	, m_pExitHelper(nullptr)
	, m_pSitHelper(nullptr)
	, m_pAIVisionHelper(nullptr)
	, m_serializerId(0)
	, m_passengerId(0)
	, m_aiWeaponId(0)
	, m_seatId(InvalidVehicleSeatId)
	, m_currentView(InvalidVehicleViewId)
	, m_lockStatus(eVSLS_Unlocked)
	, m_lockDefault(eVSLS_Unlocked)
	, m_pIdleAction(nullptr)
	, m_postEnterId(0)
	, m_postViewId(0)
	, m_explosionShakeMult(1.0f)
	, m_seatGroupIndex(1)
	, m_transitionType(eVT_None)
	, m_prevTransitionType(eVT_None)
	, m_queuedTransition(eVT_None)
	, m_isDriver(false)
	, m_isPassengerShielded(false)
	, m_isPassengerHidden(false)
	, m_isPassengerExposed(false)
	, m_isPlayerViewBlending(false)
	, m_stopAllAnimationsOnEnter(true)
	, m_skipViewBlending(false)
	, m_updatePassengerTM(true)
	, m_isRemoteControlled(false)
	, m_isRagdollingOnDeath(false)
	, m_movePassengerOnExit(true)
	, m_autoAimEnabled(false)
	, m_transformViewOnExit(true)
	, m_shouldEnterInFirstPerson(true)
	, m_shouldExitInFirstPerson(true)
{}

//------------------------------------------------------------------------
CVehicleSeat::~CVehicleSeat()
{
	CRY_ASSERT_MESSAGE(!m_passengerId, "Passenger should be long gone by now.");

	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		SSeatActionData& seatActionData = *ite;
		CRY_ASSERT(seatActionData.pSeatAction);
		PREFAST_ASSUME(seatActionData.pSeatAction);
		seatActionData.pSeatAction->Release();
		seatActionData.pSeatAction = NULL;
	}

	for (TVehicleViewVector::iterator it = m_views.begin(); it != m_views.end(); ++it)
		(*it)->Release();

	if (gEnv->bMultiplayer && gEnv->bServer && gEnv->pEntitySystem->GetEntity(m_serializerId))
	{
		gEnv->pEntitySystem->RemoveEntity(m_serializerId, true /*Remove Now*/);
		m_serializerId = 0;
		m_pSerializer = 0;
	}
}

namespace
{
template<typename T>
T GetAttrWithDefault(const CVehicleParams& seatTable, const char* name, const T& def)
{
	T value = def;
	seatTable.getAttr(name, value);
	return value;
}
}

//------------------------------------------------------------------------
bool CVehicleSeat::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& seatTable)
{
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();

	if (!m_pGameFramework)
	{
		m_pGameFramework = CCryAction::GetCryAction();
	}

	m_pVehicle = (CVehicle*)pVehicle;
	m_seatId = seatId;

	m_name = seatTable.getAttr("name");

	// Helpers
	if (seatTable.haveAttr("sitHelper"))
		m_pSitHelper = m_pVehicle->GetHelper(seatTable.getAttr("sitHelper"));

	if (seatTable.haveAttr("aiVisionHelper"))
		m_pAIVisionHelper = m_pVehicle->GetHelper(seatTable.getAttr("aiVisionHelper"));

	if (seatTable.haveAttr("enterHelper"))
		m_pEnterHelper = m_pVehicle->GetHelper(seatTable.getAttr("enterHelper"));

	if (seatTable.haveAttr("exitHelper"))
		m_pExitHelper = m_pVehicle->GetHelper(seatTable.getAttr("exitHelper"));

	// Attributes
	m_actionMap = seatTable.getAttr("actionMap");

	string lockedAttr = seatTable.getAttr("locked");
	if (!lockedAttr.empty())
	{
		if (lockedAttr == "All" || lockedAttr == "1")
			m_lockStatus = eVSLS_Locked;
		else if (lockedAttr == "AI")
			m_lockStatus = eVSLS_LockedForAI;
		else if (lockedAttr == "Players")
			m_lockStatus = eVSLS_LockedForPlayers;
		else
			m_lockStatus = eVSLS_Unlocked;
	}
	m_lockDefault = m_lockStatus;

	m_autoAimEnabled = GetAttrWithDefault<bool>(seatTable, "AutoAim", m_autoAimEnabled);
	if (seatTable.haveAttr("AimPart"))
		m_pAimPart = m_pVehicle->GetPart(seatTable.getAttr("AimPart"));

	// Enter/exit settings
	m_transformViewOnExit = GetAttrWithDefault<bool>(seatTable, "TransformViewOnExit", m_transformViewOnExit);
	m_shouldEnterInFirstPerson = GetAttrWithDefault<bool>(seatTable, "enterInFirstPerson", m_shouldEnterInFirstPerson);
	m_shouldExitInFirstPerson = GetAttrWithDefault<bool>(seatTable, "exitInFirstPerson", m_shouldExitInFirstPerson);
	m_movePassengerOnExit = GetAttrWithDefault<bool>(seatTable, "movePassengerOnExit", m_movePassengerOnExit);
	m_isRagdollingOnDeath = GetAttrWithDefault<bool>(seatTable, "ragdollOnDeath", m_isRagdollingOnDeath);
	m_stopAllAnimationsOnEnter = !GetAttrWithDefault<bool>(seatTable, "disableStopAllAnimationsOnEnter", m_stopAllAnimationsOnEnter);
	m_exitOffsetPlayer = GetAttrWithDefault<Vec3>(seatTable, "exitOffsetPlayer", m_exitOffsetPlayer);
	m_seatNameToUseForEntering = seatTable.getAttr("usesSeatForEntering");

	m_updatePassengerTM = GetAttrWithDefault<bool>(seatTable, "updatePassengerTM", m_updatePassengerTM);

	m_isDriver = GetAttrWithDefault<bool>(seatTable, "isDriver", m_isDriver);
	m_isRemoteControlled = GetAttrWithDefault<bool>(seatTable, "isRemoteControlled", m_isRemoteControlled);
	m_isPassengerShielded = GetAttrWithDefault<bool>(seatTable, "isPassengerShielded", m_isPassengerShielded);
	m_isPassengerExposed = GetAttrWithDefault<bool>(seatTable, "isPassengerExposed", m_isPassengerExposed);
	m_isPassengerHidden = GetAttrWithDefault<bool>(seatTable, "isPassengerHidden", m_isPassengerHidden);

	m_useActionsRemotelyOfSeat = seatTable.getAttr("remotelyUseActionsFromSeat");
	m_explosionShakeMult = GetAttrWithDefault<float>(seatTable, "explosionShakeMult", m_explosionShakeMult);

	// Seat Group
	m_seatGroupIndex = GetAttrWithDefault<int>(seatTable, "seatGroupIndex", m_seatGroupIndex);

	// Mannequin
	IActionController* pActionController = m_pVehicle->GetAnimationComponent().GetActionController();
	if (pActionController != NULL)
	{
		const char* actionNames[eMannequinSeatAction_Count] = { "Idle", "ExitDoor" };
		const SControllerDef& controllerDef = pActionController->GetContext().controllerDef;

		// Set defaults
		for (int i = 0; i < eMannequinSeatAction_Count; ++i)
		{
			stack_string fragmentName = actionNames[i];
			fragmentName.append(GetName().c_str());

			m_seatFragmentIDs[i] = controllerDef.m_fragmentIDs.Find(fragmentName.c_str());
		}

		// Override with custom values
		if (CVehicleParams mannequinTable = seatTable.findChild("Mannequin"))
		{
			for (int i = 0; i < eMannequinSeatAction_Count; ++i)
			{
				if (mannequinTable.haveAttr(actionNames[i]))
				{
					m_seatFragmentIDs[i] = controllerDef.m_fragmentIDs.Find(mannequinTable.getAttr(actionNames[i]));
				}
			}
		}
	}

	// Views
	if (CVehicleParams viewsTable = seatTable.findChild("Views"))
	{
		int viewCount = viewsTable.getChildCount();
		m_views.reserve(viewCount);
		for (int i = 0; i < viewCount; ++i)
		{
			if (CVehicleParams viewTable = viewsTable.getChild(i))
			{
				string className = viewTable.getAttr("class");
				if (!className.empty())
				{
					if (CVehicleParams viewParamsTable = viewTable.findChild(className))
					{
						IVehicleView* view = pVehicleSystem->CreateVehicleView(className);
						if (view)
						{
							if (view->Init(this, viewTable))
								m_views.push_back(view);
							else
								delete view;
						}
					}
				}
			}
		}
	}

	// Sounds
	if (CVehicleParams soundsTable = seatTable.findChild("Sounds"))
	{
		soundsTable.getAttr("inout", m_soundParams.inout);
		soundsTable.getAttr("mood", m_soundParams.mood);
		const char* moodName = soundsTable.getAttr("moodName");
		if (moodName && moodName[0])
		{
			m_soundParams.moodName = moodName;
		}
	}

	// Actions
	InitSeatActions(seatTable);

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::PostInit(IVehicle* pVehicle)
{
	if (!m_useActionsRemotelyOfSeat.empty())
	{
		TVehicleSeatId seatId = m_pVehicle->GetSeatId(m_useActionsRemotelyOfSeat.c_str());

		if (seatId != InvalidVehicleSeatId)
			m_pRemoteSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(seatId);

		m_useActionsRemotelyOfSeat.clear();
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::OnSpawnComplete()
{

	bool needsSynch = !m_seatActions.empty();

	if (gEnv->bMultiplayer && gEnv->bServer && needsSynch && !IsDemoPlayback() && !(m_pVehicle->GetEntity()->GetFlags() & ENTITY_FLAG_CLIENT_ONLY))
	{
		string name(m_pVehicle->GetEntity()->GetName());
		name.append("_serializer_");

		char a[128] = { 0 };
		name.append(ltoa(m_seatId, a, 10));

		SEntitySpawnParams params;
		params.sName = name.c_str();
		params.nFlags = ENTITY_FLAG_NEVER_NETWORK_STATIC | ENTITY_FLAG_NO_SAVE;
		params.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("VehicleSeatSerializer");

		if (!params.pClass)
		{
			CryFatalError("VehicleSeatSerializer not registered in the entity system!");
		}

		static_cast<CVehicleSystem*>(CCryAction::GetCryAction()->GetIVehicleSystem())->SetInitializingSeat(this);

		IEntity* pSerializerEntity = gEnv->pEntitySystem->SpawnEntity(params);

		static_cast<CVehicleSystem*>(CCryAction::GetCryAction()->GetIVehicleSystem())->SetInitializingSeat(0);

		CGameObject* pSerializerGameObject = (CGameObject*)pSerializerEntity->GetProxy(ENTITY_PROXY_USER);
	}

	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(), end = m_seatActions.end(); it != end; ++it)
	{
		SSeatActionData& data = *it;
		if (data.pSeatAction)
			data.pSeatAction->OnSpawnComplete();
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::InitSeatActions(const CVehicleParams& seatTable)
{
	if (IsDriver())
	{
		CVehicleSeatActionMovement* pActionMovement = new CVehicleSeatActionMovement;
		if (pActionMovement && pActionMovement->Init(m_pVehicle, this))
		{
			m_seatActions.resize(m_seatActions.size() + 1);
			SSeatActionData& seatActionData = m_seatActions.back();

			seatActionData.pSeatAction = pActionMovement;
			seatActionData.isAvailableRemotely = true;
		}
		else
		{
			CRY_ASSERT(0 && "[CVehicleSeat::InitSeatActions] failed to init movement action");
			delete pActionMovement;
		}
	}

	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	CRY_ASSERT(pVehicleSystem);

	CVehicleParams seatActionsTable = seatTable.findChild("SeatActions");
	if (!seatActionsTable)
		return false;

	int i = 0;
	int c = seatActionsTable.getChildCount();

	for (; i < c; i++)
	{
		if (CVehicleParams seatActionTable = seatActionsTable.getChild(i))
		{
			string className = seatActionTable.getAttr("class");
			if (!className.empty())
			{
				IVehicleSeatAction* pSeatAction = pVehicleSystem->CreateVehicleSeatAction(className);
				if (pSeatAction && pSeatAction->Init(m_pVehicle, this, seatActionTable))
				{
					pSeatAction->m_name = seatActionTable.getAttr("name");

					// add the seat action to the vector
					m_seatActions.resize(m_seatActions.size() + 1);
					SSeatActionData& seatActionData = m_seatActions.back();

					seatActionData.pSeatAction = pSeatAction;

					// store first weapon/turret action for quick access
					CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction);
					if (pActionWeapons && !m_pWeaponAction)
						m_pWeaponAction = pActionWeapons;

					CVehicleSeatActionRotateTurret* pActionTurret = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction);
					if (pActionTurret && !m_pTurretAction)
						m_pTurretAction = pActionTurret;

					if (!seatActionTable.getAttr("isAvailableRemotely", seatActionData.isAvailableRemotely))
						seatActionData.isAvailableRemotely = false;
				}
				else
				{
					SAFE_RELEASE(pSeatAction);
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::Reset()
{
	if (m_passengerId)
		Exit(false, true);

	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(), end = m_seatActions.end(); it != end; ++it)
	{
		SSeatActionData& data = *it;
		data.Reset();
	}

	if (IVehicleView* pView = GetCurrentView())
		pView->Reset();

	m_transitionType = eVT_None;
	m_queuedTransition = eVT_None;

	m_lockStatus = m_lockDefault;
	m_passengerId = 0;

	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
}

//------------------------------------------------------------------------
void CVehicleSeat::SerializeActions(TSerialize ser, EEntityAspects aspects)
{
	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(); it != m_seatActions.end(); ++it)
		it->Serialize(ser, aspects);
}

//------------------------------------------------------------------------
bool CVehicleSeat::Enter(EntityId actorId, bool isTransitionEnabled)
{
	if (m_pVehicle->IsDestroyed())
		return false;

	if (gEnv->bMultiplayer && !CCryActionCVars::Get().g_multiplayerEnableVehicles)
		return false;

	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
	CRY_ASSERT(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(actorId);
	if (!pActor)
		return false;

	const bool shouldEnslaveActor = ShouldEnslaveActorAnimations();

	if (pActor->IsPlayer())
	{
		gEnv->pEntitySystem->RemoveEntityFromLayers(m_pVehicle->GetEntityId());
	}

	if (IPhysicalEntity* pPE = m_pVehicle->GetEntity()->GetPhysics())
	{
		pe_action_awake awakeParams;
		awakeParams.bAwake = true;
		pPE->Action(&awakeParams);
	}

	if (!IsFree(pActor))
	{
		if (m_passengerId != pActor->GetEntityId())
			return false;
		else if (m_transitionType == eVT_Entering && !m_postEnterId)
			return true;
	}

	if (m_passengerId == pActor->GetEntityId() && m_transitionType == eVT_None && !m_postEnterId)
		return false;

	// exit previous seat if any
	if (IVehicle* pPrevVehicle = pActor->GetLinkedVehicle())
	{
		if (IVehicleSeat* pPrevSeat = pPrevVehicle->GetSeatForPassenger(pActor->GetEntityId()))
			if (pPrevSeat != this)
				pPrevSeat->Exit(false, true);
	}

	// in case there's just a corpse on the seat, we simply push it to the ground
	if (m_passengerId && m_passengerId != pActor->GetEntityId())
	{
		if (m_transitionType == eVT_RemoteUsage)
			ExitRemotely();

		if (IActor* pCurrentPassenger = pActorSystem->GetActor(m_passengerId))
		{
			if (pCurrentPassenger->IsDead())
				Exit(false);
			else
				return false;
		}
	}

	CVehicleAnimationComponent& animationComponent = m_pVehicle->GetAnimationComponent();
	IActionController* pActionController = animationComponent.GetActionController();
	bool doTransition = shouldEnslaveActor && isTransitionEnabled && (VehicleCVars().v_transitionAnimations == 1) && !IsRemoteControlledInternal() && (!pActor->IsPlayer() || (VehicleCVars().v_playerTransitions == 1)) && pActionController;
	const SControllerDef* pContDef = NULL;

	if (pActionController && shouldEnslaveActor)
	{
		animationComponent.AttachPassengerScope(this, actorId, true);
		pContDef = &pActionController->GetContext().controllerDef;
	}

	// Start Animation if needed.
	if (doTransition)
	{
		pActor->HolsterItem(true);

		m_transitionType = eVT_Entering;

		m_aiWeaponId = 0;

		SActorTargetParams target;
		CVehicleSeat* pSeatToEnter = NULL;

		if (!m_seatNameToUseForEntering.empty())
		{
			pSeatToEnter = (CVehicleSeat*)m_pVehicle->GetSeatById(m_pVehicle->GetSeatId(m_seatNameToUseForEntering));
			if (pSeatToEnter)
			{
				if (!pSeatToEnter->IsFree(pActor))
					return false;

				pSeatToEnter->m_transitionType = eVT_SeatIsBorrowed;
			}
		}

		if (!pSeatToEnter)
			pSeatToEnter = this;

		// Transition Animation.
		int seatID = pSeatToEnter->GetSeatId();
		stack_string fragmentName = "EnterDoor";
		fragmentName.append(pSeatToEnter->GetName().c_str());
		FragmentID fragID = pContDef->m_fragmentIDs.Find(fragmentName.c_str());
		CVehicleSeatAnimActionEnter* enterAction = new CVehicleSeatAnimActionEnter(IVehicleAnimationComponent::ePriority_SeatTransition, fragID, this);
		QuatT targetPos(m_pVehicle->GetEntity()->GetPos(), m_pVehicle->GetEntity()->GetRotation());
		enterAction->SetParam("TargetPos", targetPos);
		pActionController->Queue(*enterAction);
	}

	CryLog("[VEHICLE] [%s] ENTERING VEHICLE: Vehicle[%u] Passenger[%u]", gEnv->bServer ? "SERVER" : "CLIENT", m_pVehicle->GetEntityId(), actorId);

	// Link to Vehicle.
	m_passengerId = actorId;
	pActor->LinkToVehicle(m_pVehicle->GetEntityId());
	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);

	if (!doTransition)
	{
		if (!IsRemoteControlledInternal())
		{
			if (pActor->IsClient())
			{
				EntityId currentItemId = 0;
				IInventory* pInventory = pActor->GetInventory();
				if (pInventory)
					currentItemId = pInventory->GetCurrentItem();

				pActor->HolsterItem(true);

				if (pInventory)
					pInventory->SetLastItem(currentItemId);
			}
			else
			{
				pActor->HolsterItem(true);
			}
		}

		if (pActor->IsPlayer() && !m_postEnterId)
		{
			if (m_stopAllAnimationsOnEnter && !IsRemoteControlledInternal())
			{
				if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
					pCharInstance->GetISkeletonAnim()->StopAnimationsAllLayers();
			}

			m_isPlayerViewBlending = (isTransitionEnabled && VehicleCVars().v_transitionAnimations == 1) && pActionController;
		}

		SitDown();
	}

	// adjust the vehicle update slot to start calling the seat update function
	m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_PassengerIn);

	if (gEnv->bMultiplayer && gEnv->bServer)
		m_pVehicle->KillAbandonedTimer();

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::EnterRemotely(EntityId actorId)
{
	if (m_passengerId != 0 && actorId != m_passengerId && m_transitionType != eVT_Dying)
		return false;

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(actorId);
	if (!(pActor && pActor->IsPlayer()))
		return false;

	m_passengerId = actorId;
	m_transitionType = eVT_RemoteUsage;

	GivesActorSeatFeatures(true, true);

	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::ExitRemotely()
{
	if (m_passengerId == 0 || m_transitionType != eVT_RemoteUsage)
		return false;

	GivesActorSeatFeatures(false, true);

	m_passengerId = 0;
	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);

	m_transitionType = eVT_None;

	return true;
}

//------------------------------------------------------------------------
CVehicleSeat* CVehicleSeat::GetSeatUsedRemotely(bool onlyIfBeingUsed) const
{
	if (m_pRemoteSeat)
	{
		if (onlyIfBeingUsed)
		{
			if (m_pRemoteSeat->GetCurrentTransition() != eVT_RemoteUsage || m_pRemoteSeat->GetPassenger(true) != m_passengerId)
				return NULL;
		}

		if (VehicleCVars().v_driverControlledMountedGuns == 0 && IsDriver() && m_pRemoteSeat->IsGunner())
			return NULL;
	}

	return m_pRemoteSeat;
}

//------------------------------------------------------------------------
bool CVehicleSeat::ShouldEnslaveActorAnimations() const
{
	return (m_pWeaponAction == NULL) || !m_pWeaponAction->IsMounted() || (m_pVehicle->GetAnimationComponent().GetActionController() != NULL);
}

//------------------------------------------------------------------------
bool CVehicleSeat::SitDown()
{
	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
	IActor* pActor = pActorSystem->GetActor(m_passengerId);
	if (!pActor)
		return false;

	if (pActor->IsPlayer() && (VehicleCVars().v_playerTransitions == 0))
	{
		// Just for the player, we keep him in the entered state until the next frame.
		m_transitionType = eVT_Entering;
	}
	else
	{
		m_transitionType = eVT_None;
	}

	IAISystem* pAISystem = gEnv->pAISystem;
	if (pAISystem && pAISystem->IsEnabled())
	{
		pAISystem->GetSmartObjectManager()->AddSmartObjectState(pActor->GetEntity(), "InVehicle");
	}

	IEntity* pPassengerEntity = pActor->GetEntity();

	// MR: moved this to before GiveActorSeatFeatures, as the latter sets the vehicle view,
	// which needs the remote seat to check for potentially available remote views
	if (CVehicleSeat* pSeat = GetSeatUsedRemotely(false))
		pSeat->EnterRemotely(m_passengerId);

	if (pActor->IsPlayer() || pActor->GetEntity()->GetAI())
	{
		GivesActorSeatFeatures(true);
	}
	else if (!pActor->GetEntity()->GetAI())
	{
		// enable the seat actions
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (seatActionData.isEnabled)
				seatActionData.pSeatAction->StartUsing(m_passengerId);
		}
	}

	if (pActor->IsClient() && gEnv->pInput)
		gEnv->pInput->RetriggerKeyState();

	// allow lua side of the seat implementation to do its business
	HSCRIPTFUNCTION scriptFunction(0);
	if (IScriptTable* pScriptTable = m_pVehicle->GetEntity()->GetScriptTable())
	{
		if (pScriptTable->GetValue("OnActorSitDown", scriptFunction))
		{
			CRY_ASSERT(scriptFunction);
			ScriptHandle passengerHandle(pPassengerEntity->GetId());
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::Call(pIScriptSystem, scriptFunction, pScriptTable, GetSeatId(), passengerHandle);
			pIScriptSystem->ReleaseFunc(scriptFunction);
		}
	}

	// broadcast an event about the situation
	SVehicleEventParams eventParams;
	eventParams.iParam = m_seatId;
	eventParams.entityId = m_passengerId;
	m_pVehicle->BroadcastVehicleEvent(eVE_PassengerEnter, eventParams);

	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);

	if (pActor->IsClient())
	{
		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams params;
			params.bValue = true;
			pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveVehicle, params);
		}
	}

	UpdatePassengerLocalTM(pActor);

	if (m_pSeatGroup)
		m_pSeatGroup->OnPassengerEnter(this, m_passengerId);

	CCryAction::GetCryAction()->GetIGameplayRecorder()->Event(pActor->GetEntity(), GameplayEvent(eGE_EnteredVehicle, 0, 0, (void*)(EXPAND_PTR)m_pVehicle->GetEntityId()));

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::QueueTransition()
{
	// check if transition is prohibited by other blocking seats
	const STransitionInfo& info = m_pVehicle->GetTransitionInfoForSeat(m_seatId);

	if (!info.waitFor.empty())
	{
		for (int i = 0, nWait = info.waitFor.size(); i < nWait; ++i)
		{
			CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(info.waitFor[i]);
			CRY_ASSERT(pSeat);

			int trans = pSeat->GetCurrentTransition();

			if (trans == eVT_Entering || trans == eVT_Exiting)
				return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeat::Exit(bool isTransitionEnabled, bool force /*=false*/, Vec3 exitPos /*=ZERO*/)
{
	if (m_transitionType == eVT_RemoteUsage)
		return ExitRemotely();

	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
	IActor* pActor = pActorSystem->GetActor(m_passengerId);

	if (!exitPos.IsEquivalent(ZERO))
		m_exitWorldPos = exitPos;

	// check the player can exit the vehicle from somewhere...
	// This just ensures that when we reach StandUp there will be a valid place to exit from.
	// Changed this for MP: only local client should decide whether it can get out or not (since it
	//	will also decide where to place itself).
	if (pActor && pActor->IsClient() && exitPos.IsEquivalent(ZERO))
	{
		Matrix34 worldTM = GetExitTM(pActor, false);
		m_exitWorldPos = worldTM.GetTranslation();
		EntityId blockerId = 0;
		bool canExit = TestExitPosition(pActor, m_exitWorldPos, &blockerId);

		// if our own seat exit is blocked, ask the vehicle for a free one...
		if (!canExit && !force)
		{
			canExit = m_pVehicle->GetExitPositionForActor(pActor, m_exitWorldPos, false);
		}

		if (!canExit && !force)
		{
			m_exitWorldPos.zero();
			return false;
		}
	}

	const bool shouldEnslaveActor = ShouldEnslaveActorAnimations();
	bool doTransition = shouldEnslaveActor && pActor && ((VehicleCVars().v_playerTransitions == 1) || !pActor->IsPlayer());

	if (pActor)
	{
		if (doTransition)
		{
			if (isTransitionEnabled && !force)
			{
				if (QueueTransition())
				{
					m_queuedTransition = eVT_Exiting;
					return true;
				}
			}

			if (m_transitionType == eVT_Entering)
			{
				// reset actor target request
				/*			IAIObject* pAIObject = pActor->GetEntity()->GetAI();
				   if ( pAIObject )
				   {
				   CAIProxy* pAIProxy = (CAIProxy*) pAIObject->GetProxy();
				   pAIProxy->SetActorTarget( this, NULL );
				   }*/
			}
			else if (m_transitionType == eVT_None)
			{
				m_adjustedExitPos.zero();

				if (IAIObject* pAIObject = pActor->GetEntity()->GetAI())
				{
					Matrix34 worldTM = GetExitTM(pActor);
					Vec3 adjustedPos;

					if (IAIPathAgent* aiactor = pAIObject->CastToIAIActor())
					{
						if (aiactor->GetValidPositionNearby(worldTM.GetTranslation(), adjustedPos))
							if (worldTM.GetTranslation() != adjustedPos)
								m_adjustedExitPos = adjustedPos;
					}
				}
			}
		}
		else
		{
			/*m_adjustedExitPos.zero();
			   Matrix34 worldTM = GetExitTM(pActor);
			   Vec3 pos = worldTM.GetTranslation();
			   int i = 0;
			   bool valid = false;

			   while (!valid)
			   valid = TestExitPos(pActor, pos, i++);

			   if (pos != worldTM.GetTranslation())
			   m_adjustedExitPos = pos;
			 */
		}
	}

	m_transitionType = pActor && (m_transitionType != eVT_Entering) && doTransition ? eVT_Exiting : eVT_None;
	m_queuedTransition = eVT_None;

	GivesActorSeatFeatures(false);

	if (pActor)
	{
		IActionController* pActionController = m_pVehicle->GetAnimationComponent().GetActionController();

		if ((VehicleCVars().v_playerTransitions == 1) || !pActor->IsPlayer())
		{
			pActor->HolsterItem(false);
			if (m_transitionType == eVT_Exiting && pActionController)
			{
				if (!isTransitionEnabled || force || IsDemoPlayback())
				{
					StandUp();
				}
				else if (m_seatFragmentIDs[eMannequinSeatAction_Exit] != FRAGMENT_ID_INVALID)
				{
					CVehicleSeatAnimActionExit* exitAction = new CVehicleSeatAnimActionExit(IVehicleAnimationComponent::ePriority_SeatTransition, m_seatFragmentIDs[eMannequinSeatAction_Exit], this);
					QuatT targetPos(m_pVehicle->GetEntity()->GetPos(), m_pVehicle->GetEntity()->GetRotation());
					exitAction->SetParam("TargetPos", targetPos);
					pActionController->Queue(*exitAction);
				}
			}
			else
			{
				// FrancescoR: Not sure in which cases this is needed but atm when we are here
				// we are exiting with a m_transitionType == eVT_None. We should probably move the
				// passenger on exit to avoid that they exit inside the vehicle itself

				//bool temp = m_movePassengerOnExit;
				//m_movePassengerOnExit = !doTransition;
				StandUp();
				//m_movePassengerOnExit = temp;
			}
		}
		else
		{
			StandUp();
		}

		if (m_pSeatGroup)
			m_pSeatGroup->OnPassengerExit(this, m_passengerId);

		if (!m_exitWorldPos.IsZero())
		{
			Matrix34 worldTM = pActor->GetEntity()->GetWorldTM();
			worldTM.SetTranslation(m_exitWorldPos);
			pActor->GetEntity()->SetWorldTM(worldTM);
		}
	}

	m_adjustedExitPos.zero();

	if (pActor && pActor->IsClient())
	{
		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams params;
			params.bValue = false;
			pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveVehicle, params);
		}
	}

	if (!pActor)
	{
		// seems to be necessary - had a crash on sv_restart, possibly because the actor has already been
		//	deleted, so m_passenger id is never reset, so ~CVehicleSeat() tries to call Exit again. This ends up calling
		// m_pVehicle->IsEmpty() below on a half-destructed vehicle
		m_passengerId = 0;
		return true;
	}

	if (gEnv->bMultiplayer && gEnv->bServer && m_pVehicle->IsEmpty())
		m_pVehicle->StartAbandonedTimer();

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::StandUp()
{
	if (m_passengerId == 0)
		return false;

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_passengerId);
	if (!pActor)
		return false;

	const string actorTextDescription = pActor->GetEntity()->GetEntityTextDescription().c_str();
	const string vehicleTextDescription = m_pVehicle->GetEntity()->GetEntityTextDescription().c_str();
	CryLog("Making %s leave %s", actorTextDescription.c_str(), vehicleTextDescription.c_str());
	INDENT_LOG_DURING_SCOPE();

	if ((VehicleCVars().v_playerTransitions == 1) && pActor->IsClient())
	{
		if (GetCurrentViewId() != InvalidVehicleViewId)
			SetView(InvalidVehicleViewId);
	}

	if (m_pVehicle->GetAnimationComponent().GetActionController())
	{
		m_pVehicle->GetAnimationComponent().AttachPassengerScope(this, m_passengerId, false);
	}

	IEntity* pPassengerEntity = pActor->GetEntity();

	IAISystem* pAISystem = gEnv->pAISystem;
	if (pAISystem && pAISystem->IsEnabled())
	{
		pAISystem->GetSmartObjectManager()->RemoveSmartObjectState(pActor->GetEntity(), "InVehicle");
	}

	// allow lua side of the seat implementation to do its business
	HSCRIPTFUNCTION scriptFunction(0);
	if (IScriptTable* pScriptTable = m_pVehicle->GetEntity()->GetScriptTable())
	{
		if (pScriptTable->GetValue("OnActorStandUp", scriptFunction))
		{
			CRY_ASSERT(scriptFunction);
			ScriptHandle passengerHandle(pPassengerEntity->GetId());
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::Call(pIScriptSystem, scriptFunction, pScriptTable, passengerHandle, true);
			pIScriptSystem->ReleaseFunc(scriptFunction);
		}
	}

	CryLog("[VEHICLE] [%s] EXITING VEHICLE:  Vehicle[%u] Passenger[%u]", gEnv->bServer ? "SERVER" : "CLIENT", m_pVehicle->GetEntityId(), pActor->GetEntityId());

	pActor->LinkToVehicle(0);
	m_passengerId = 0;

	if (pActor->IsClient() && gEnv->pInput)
		gEnv->pInput->RetriggerKeyState();

	// current world transform of passenger
	// we always want to use the rotational part of this
	// and we want to use the position if he played an exit animation
	Matrix34 passengerTM = pPassengerEntity->GetWorldTM();
	Vec3 exitPos = passengerTM.GetTranslation();
	// this only gets set to true if the world transform should really be updated
	bool dirtyPos = false;

	if (m_movePassengerOnExit)
	{
		if (!m_adjustedExitPos.IsZero())
		{
			// something external is trying to adjust the exit position ... so we will just let it do it
			exitPos = m_adjustedExitPos;
			dirtyPos = true;
		}
		else
		{
			// ok, we get to choose the exit position ourselves
			// we assume that the exit animation will get us reasonably close to the exit helper (0.5m)
			// if not, we do a fallback teleport
			// the teleport is there to solve two case
			//		- we don't have an exit animation
			//		- something messed up playing the exit animation
			static float teleportDistanceSq = 0.25f;

			// by this point m_exitWorldPos should be valid (non-zero)
			if (m_exitWorldPos.GetLengthSquared() > 0.01f)
			{
				exitPos = m_exitWorldPos;
				dirtyPos = true;
			}
			else
			{
				// if not, fallback to the old method and just exit this seat
				Matrix34 exitHelperTM = GetExitTM(pActor);
				Vec3 delta = exitHelperTM.GetTranslation() - exitPos;
				if (delta.GetLengthSquared() > teleportDistanceSq)
				{
					// teleport
					exitPos = exitHelperTM.GetTranslation();
					dirtyPos = true;
				}
			}
		}
	}

	// sanity check the exit position
	{
		float elevation = gEnv->p3DEngine->GetTerrainElevation(exitPos.x, exitPos.y);

		if (exitPos.z < elevation && (elevation - exitPos.z) < 5.0f)
		{
			// we are under the terrain ... but this can be fine if we are inside a VisArea (e.g. a tunnel inside the terrain)
			Vec3 vehiclePos = m_pVehicle->GetEntity()->GetWorldPos();
			IVisArea* pVehicleVisArea = gEnv->p3DEngine->GetVisAreaFromPos(vehiclePos);
			IVisArea* pExitVisArea = gEnv->p3DEngine->GetVisAreaFromPos(exitPos);

			if (!(pVehicleVisArea && pExitVisArea))
			{
				dirtyPos = true;
				exitPos.z = max(exitPos.z, elevation);
			}
		}
	}

	if (dirtyPos && !IsRemoteControlledInternal() && m_transformViewOnExit)
	{
		// set the position, but keep the orientation
		passengerTM.SetTranslation(exitPos);
		// move the entity( during serialize, don't change the postiin
		if (!gEnv->pSystem->IsSerializingFile())
			pPassengerEntity->SetWorldTM(passengerTM, ENTITY_XFORM_USER);
	}

#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debugdraw > 0)
	{
		CPersistantDebug* pDB = CCryAction::GetCryAction()->GetPersistantDebug();
		pDB->Begin("Seat", false);
		pDB->AddDirection(passengerTM.GetTranslation() + Vec3(0, 0, 0.5f), 0.25f, passengerTM.GetColumn(1), ColorF(1, 1, 0, 1), 5.f);
	}
#endif

	// downgrade the vehicle update slot
	m_pVehicle->GetGameObject()->DisableUpdateSlot(m_pVehicle, IVehicle::eVUS_PassengerIn);

	// broadcast an event about the passenger exiting
	SVehicleEventParams eventParams;
	eventParams.iParam = m_seatId;
	eventParams.entityId = pActor->GetEntityId();
	m_pVehicle->BroadcastVehicleEvent(eVE_PassengerExit, eventParams);

	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);

	if (!gEnv->pSystem->IsSerializingFile()) //serialization will fix that automatically
	{
		if (!pActor->IsDead())
		{
			if (IInventory* pInventory = pActor->GetInventory())
			{
				IItem* pCurrentItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(pInventory->GetCurrentItem());
				IWeapon* pWeapon = pCurrentItem ? pCurrentItem->GetIWeapon() : NULL;
				if (!pWeapon || !pWeapon->IsRippedOff())
					pActor->HolsterItem(false);
			}
		}
		else
		{
			pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
		}

		if (pActor->IsPlayer())
		{
			if (m_transformViewOnExit)
			{
				// align the look direction to the current vehicle view direction, to minimise the
				// snap when exiting

				IView* pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();

				if (pView)
				{
					SViewParams params = *pView->GetCurrentParams();

					pActor->SetViewRotation(Quat::CreateRotationVDir(params.GetRotationLast().GetColumn1().GetNormalized()));
				}
			}
			else
			{
				//Make sure we aren't left with any view roll
				IView* pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();

				if (pView)
				{
					SViewParams params = *pView->GetCurrentParams();
					params.SaveLast();
					Ang3 currentAngles(params.GetRotationLast());
					if (fabs(currentAngles.y) > 0.01f)
					{
						currentAngles.y = 0.f;
						pActor->SetViewRotation(Quat(currentAngles));
					}
				}
			}

			pActor->GetEntity()->Hide(false);
		}
	}

	m_transitionType = eVT_None;

	if (pActor->IsClient())
	{
		StopSounds();
	}

	eventParams.entityId = pActor->GetEntityId();
	eventParams.iParam = m_seatId;
	m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, eventParams);

	CCryAction::GetCryAction()->GetIGameplayRecorder()->Event(pActor->GetEntity(), GameplayEvent(eGE_LeftVehicle, 0, 0, (void*)(EXPAND_PTR)m_pVehicle->GetEntityId()));

	m_exitWorldPos.zero();

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::OnSeatFreed(TVehicleSeatId seatId, EntityId passengerId)
{
	if (seatId == m_seatId || !m_passengerId || m_passengerId == passengerId)
		return;

	// if remote seat freed by someone else, try to occupy it
	if (!gEnv->pSystem->IsSerializingFile())
	{
		if (CVehicleSeat* pSeat = GetSeatUsedRemotely(false))
		{
			if (pSeat->GetSeatId() == seatId)
			{
				pSeat->EnterRemotely(m_passengerId);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::GivesActorSeatFeatures(bool enabled, bool isRemote)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
	INetContext* pNetContext = m_pGameFramework->GetGameContext() ? m_pGameFramework->GetGameContext()->GetNetContext() : 0;
	const bool isActorEnslaved = ShouldEnslaveActorAnimations();

	if (enabled && !m_pVehicle->IsDestroyed() && (pActor && !pActor->IsDead() || gEnv->pSystem->IsSerializingFile())) // move this to PostSerialize (after alpha build..)
	{
		IActionController* pActionController = m_pVehicle->GetAnimationComponent().GetActionController();
		if (pActionController && isActorEnslaved)
		{
			if (m_seatFragmentIDs[eMannequinSeatAction_Idle] != TAG_ID_INVALID)
			{
				m_pIdleAction = new TAction<SAnimationContext>(IVehicleAnimationComponent::ePriority_SeatDefault, m_seatFragmentIDs[eMannequinSeatAction_Idle], TAG_STATE_EMPTY);
				pActionController->Queue(*m_pIdleAction);
			}
		}

		if (m_isDriver)
		{
			// give authority of the vehicle entity to whoever just sat down
			if (gEnv->bServer && pActor->IsPlayer())
			{
				CGameChannel* pGameChannel = GetGameChannel(m_passengerId);

				if (pGameChannel && pNetContext)
					pNetContext->DelegateAuthority(m_pVehicle->GetEntityId(), pGameChannel->GetNetChannel());
			}
		}

		// enable the seat actions
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (seatActionData.isEnabled && (!isRemote || isRemote && seatActionData.isAvailableRemotely))
				seatActionData.pSeatAction->StartUsing(m_passengerId);
		}

		if (!isRemote && pActor->IsClient())
		{
			if (CAnimatedCharacter* pAnimChar = (CAnimatedCharacter*)(pActor->GetAnimatedCharacter()))
			{
				pAnimChar->RegisterListener(this, "VehicleSeat");
			}

			bool isThirdPerson = true;
			if (IVehicleView* pView = GetCurrentView())
				isThirdPerson = pView->IsThirdPerson();

			if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			{
				SVehicleMovementEventParams params;
				params.fValue = isThirdPerson ? 0.f : 1.f;
				params.bValue = true;
				pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveSeat, params);
			}

			if (IVehicleClient* pVehicleClient = GetVehicleClient())
				pVehicleClient->OnEnterVehicleSeat(this);
		}

		// give authority of the seat serializer to whoever just sat down
		if (gEnv->bServer && pActor->IsPlayer() && gEnv->pEntitySystem->GetEntity(m_serializerId))
		{
			CGameChannel* pGameChannel = GetGameChannel(m_passengerId);

			if (pGameChannel && pNetContext)
				pNetContext->DelegateAuthority(m_serializerId, pGameChannel->GetNetChannel());
		}

		if (!isRemote)
			UpdateHiddenState(true);
	}
	else if (!enabled)
	{
		if (!isRemote)
			UpdateHiddenState(false);

		if (CVehicleSeat* pSeat = GetSeatUsedRemotely(true))
			pSeat->ExitRemotely();

		if (pActor)
		{
			if (!isRemote && pActor->IsClient())
			{
				if (CAnimatedCharacter* pAnimChar = (CAnimatedCharacter*)(pActor->GetAnimatedCharacter()))
				{
					pAnimChar->UnregisterListener(this);
				}

				if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
				{
					SVehicleMovementEventParams params;
					params.fValue = 0.0f;
					params.bValue = false;
					pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveSeat, params);
				}

				if (IVehicleClient* pVehicleClient = GetVehicleClient())
					pVehicleClient->OnExitVehicleSeat(this);

				if ((VehicleCVars().v_playerTransitions == 0) || (m_transitionType == eVT_None))
				{
					if (GetCurrentViewId() != InvalidVehicleViewId)
						SetView(InvalidVehicleViewId);
				}

				StopSounds();
			}

			if (m_isDriver && pNetContext && gEnv->bServer)
				pNetContext->DelegateAuthority(m_pVehicle->GetEntityId(), 0);
		}

		// give authority of the seat serializer to server
		if (pNetContext && gEnv->bServer && gEnv->pEntitySystem->GetEntity(m_serializerId))
			pNetContext->DelegateAuthority(m_serializerId, 0);

		// disable the seat actions
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (seatActionData.isEnabled && (!isRemote || seatActionData.isAvailableRemotely))
				seatActionData.pSeatAction->StopUsing();
		}
	}
}

//------------------------------------------------------------------------
EntityId CVehicleSeat::GetPassenger(bool remoteUser /*=false*/) const
{
	if (!remoteUser && m_transitionType == eVT_RemoteUsage)
		return 0;

	return m_passengerId;
}

//------------------------------------------------------------------------
IActor* CVehicleSeat::GetPassengerActor(bool remoteUser /*=false*/) const
{
	EntityId id = GetPassenger(remoteUser);

	if (0 == id)
		return NULL;

	return CCryAction::GetCryAction()->GetIActorSystem()->GetActor(id);
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsPassengerClientActor() const
{
	if (m_passengerId)
	{
		IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
		if (pActor && pActor->GetEntityId() == m_passengerId)
			return true;
	}

	return false;
}

//------------------------------------------------------------------------
int CVehicleSeat::GetSeatActionId(IVehicleSeatAction* pAction) const
{
	int action = 0;
	for (TVehicleSeatActionVector::const_iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite, ++action)
	{
		const SSeatActionData& seatActionData = *ite;
		if (seatActionData.pSeatAction == pAction)
			return action;
	}

	return -1;
}

//------------------------------------------------------------------------
const IVehicleSeatAction* CVehicleSeat::GetSeatActionById(int id) const
{
	if (id >= 0)
	{
		int action = 0;
		for (TVehicleSeatActionVector::const_iterator ite = m_seatActions.begin(), iteEnd = m_seatActions.end(); ite != iteEnd; ++ite, ++action)
		{
			const SSeatActionData& seatActionData = *ite;
			if (id == action)
				return seatActionData.pSeatAction;
		}
	}
	return 0;
}

//------------------------------------------------------------------------
void CVehicleSeat::ChangedNetworkState(NetworkAspectType aspects)
{
	if (m_pSerializer && gEnv->pEntitySystem->GetEntity(m_serializerId))
	{
		CHANGED_NETWORK_STATE(m_pSerializer, aspects);
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsFree(IActor* pInActor)
{
	if (IsLocked(pInActor))
		return false;

	if (!m_passengerId)
		return true;

	IActor* pCurrentActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_passengerId);
	if (!pCurrentActor)
		return true;

	if (m_transitionType == eVT_RemoteUsage)
		return true;

	return pCurrentActor->IsDead();
}

//------------------------------------------------------------------------
void CVehicleSeat::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
	CRY_ASSERT(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_passengerId);

	if (!pActor)
	{
		return;
	}

	if (pActor->IsDead())
		return;

	if (!m_pVehicle->IsDestroyed())
	{
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			// skip disabled actions
			if (!seatActionData.isEnabled)
				continue;

			if (m_transitionType == eVT_None ||
			    (m_transitionType == eVT_RemoteUsage && seatActionData.isAvailableRemotely) ||
			    (m_transitionType == eVT_Entering && pActor->IsClient()))
			{
				seatActionData.pSeatAction->OnAction(actionId, activationMode, value);
			}
		}

		if (CVehicleSeat* pRemoteSeat = GetSeatUsedRemotely(true))
			pRemoteSeat->OnAction(actionId, activationMode, value);
	}

	if (actionId == eVAI_ChangeView)
		SetView(GetNextAvailableViewId());

	if (IVehicleView* pView = GetCurrentView())
		pView->OnAction(actionId, activationMode, value);
}

//------------------------------------------------------------------------
void CVehicleSeat::Update(float deltaTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	IActor* pActor = GetPassengerActor();
	if (m_passengerId && pActor && pActor->GetEntity())
	{
		// update passenger TM
		if (pActor)
		{
			if (m_transitionType == eVT_Entering && pActor->IsPlayer() && (VehicleCVars().v_playerTransitions == 0))
			{
				m_transitionType = eVT_None;
			}

			if (m_transitionType == eVT_None || m_transitionType == eVT_Dying)
			{
				if (m_updatePassengerTM)
				{
					UpdatePassengerLocalTM(pActor);
				}

#if ENABLE_VEHICLE_DEBUG
				if (m_pSitHelper && VehicleCVars().v_draw_passengers == 1)
				{
					Matrix34 tm;
					m_pSitHelper->GetWorldTM(tm);
					VehicleUtils::DrawTM(tm, "VehicleSeat::Update", true);
				}
#endif

				if (pActor->IsClient())
				{
					UpdateRemote(pActor, deltaTime);
				}

				if (pActor && (m_transitionType == eVT_Dying) && (!pActor->IsPlayer()))
				{
					if (ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0))
					{
						if (ISkeletonPose* pSkeli = pCharacter->GetISkeletonPose())
						{
							if (IPhysicalEntity* pPE = pSkeli->GetCharacterPhysics())
							{
								pe_params_part ppp;
								ppp.flagsAND = ~(geom_colltype_explosion | geom_colltype_ray | geom_colltype_foliage_proxy);
								pPE->SetParams(&ppp);
							}
						}
					}
				}
			}
			else if (m_transitionType == eVT_RemoteUsage)
			{
				for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
				{
					SSeatActionData& seatActionData = *ite;

					if (seatActionData.isAvailableRemotely && seatActionData.isEnabled)
						seatActionData.pSeatAction->Update(deltaTime);
				}
			}
			else if (m_transitionType == eVT_ExitingWarped)
			{
				Vec3 warpToPos;
				AABB worldBounds;

				pActor->GetEntity()->GetWorldBounds(worldBounds);

				if (!GetISystem()->GetViewCamera().IsAABBVisible_F(worldBounds))
				{
					if (IAIObject* pAIObject = pActor->GetEntity()->GetAI())
					{
						if (IAIPathAgent* aiactor = pAIObject->CastToIAIActor())
							if (aiactor->GetTeleportPosition(m_adjustedExitPos))
							{
								Exit(false);
								StandUp();
							}
					}
				}
			}

			if (m_queuedTransition == eVT_Exiting)
			{
				if (!QueueTransition())
					Exit(true, false);
			}

			UpdateSounds(deltaTime);
		}
	}
	else
	{
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (!seatActionData.isEnabled || !seatActionData.pSeatAction)
				continue;

			if (CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, seatActionData.pSeatAction))
			{
				seatActionData.pSeatAction->Update(deltaTime);
			}
		}
	}
}

void CVehicleSeat::UpdateRemote(IActor* pActor, float deltaTime)
{
	CVehicleSeat* pSeat = GetSeatUsedRemotely(true);
	if (!pSeat)
		return;

	if (pSeat->IsGunner() && VehicleCVars().v_driverControlledMountedGuns)
	{
		// merge this with vehicle weapon raycast (after alpha)
		CVehicleSeatActionRotateTurret* pTurretAction = NULL;

		TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator it = seatActions.begin(); it != seatActions.end(); ++it)
		{
			if (!it->isAvailableRemotely)
				continue;

			if (pTurretAction = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, it->pSeatAction))
				break;
		}

		IMovementController* pMC = (pActor != NULL) ? pActor->GetMovementController() : NULL;
		if (pMC && pTurretAction)
		{
			IVehicleView* pView = GetCurrentView();
			if (pView && pView->IsAvailableRemotely() && pView->IsThirdPerson())
			{
				// remote thirdperson view uses AimPart as usual
				pTurretAction->SetAimGoal(Vec3Constants<float>::fVec3_Zero);
			}
			else
			{
				// when using fp view, set AimGoal on Turret action
				SMovementState info;
				pMC->GetMovementState(info);

				ray_hit rayhit;
				static IPhysicalEntity* pSkipEnts[10];
				pSkipEnts[0] = m_pVehicle->GetEntity()->GetPhysics();
				int nskip = 0;

				for (int i = 0, count = m_pVehicle->GetEntity()->GetChildCount(); i < count && nskip < 9; ++i)
				{
					IEntity* pChild = m_pVehicle->GetEntity()->GetChild(i);
					IPhysicalEntity* pPhysics = pChild->GetPhysics();
					if (pPhysics)
					{
						// todo: shorter way to handle this?
						if (pPhysics->GetType() == PE_LIVING)
							if (ICharacterInstance* pCharacter = pChild->GetCharacter(0))
								if (IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
									pPhysics = pCharPhys;

						pSkipEnts[++nskip] = pPhysics;
					}
				}

				int hits = gEnv->pPhysicalWorld->RayWorldIntersection(info.weaponPosition, info.aimDirection * 200.f, ent_all,
				                                                      rwi_stop_at_pierceable | rwi_colltype_any, &rayhit, 1, pSkipEnts, nskip + 1);

				if (hits)
					pTurretAction->SetAimGoal(rayhit.pt);
				else
					pTurretAction->SetAimGoal(info.weaponPosition + 1000.f * info.aimDirection);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdatePassengerLocalTM(IActor* pActor)
{
	if (m_pSitHelper && (VehicleCVars().v_set_passenger_tm != 0) && !IsRemoteControlledInternal())
	{
		// todo: optimize this to only update after invalidation

		Matrix34 tm;

		m_pSitHelper->GetVehicleTM(tm);
		pActor->GetEntity()->SetLocalTM(tm);

	}
}

//------------------------------------------------------------------------
IVehicleView* CVehicleSeat::GetView(TVehicleViewId viewId) const
{
	if (viewId < FirstVehicleViewId || viewId > m_views.size())
		return NULL;

	IVehicleView* pView = m_views[viewId - 1];

	if (CVehicleSeat* pSeat = GetSeatUsedRemotely(true))
	{
		if (IVehicleView* pRemoteView = pSeat->GetView(viewId))
		{
			if (pRemoteView->IsAvailableRemotely() && pRemoteView->IsThirdPerson() == pView->IsThirdPerson())
				pView = pRemoteView;
		}
	}

	return pView;
}

//------------------------------------------------------------------------
bool CVehicleSeat::SetView(TVehicleViewId viewId)
{
	if (viewId < InvalidVehicleViewId || viewId > m_views.size() || viewId == m_currentView)
	{
		return false;
	}

	if (viewId == InvalidVehicleViewId)
	{
		if (IVehicleView* pOldView = GetCurrentView())
			pOldView->OnStopUsing();
	}
	else
	{
		IActor* pActor = gEnv->pGameFramework->GetClientActor();
		if (!pActor || pActor->GetEntityId() != m_passengerId)
		{
			return false;
		}

		IVehicleView* pNewView = GetView(viewId);
		if (!pNewView || !pNewView->IsEnabled())
		{
			return false;
		}

		if (IVehicleView* pOldView = GetCurrentView())
			pOldView->OnStopUsing();

		pNewView->OnStartUsing(m_passengerId);

		const bool bIsThirdPerson = pNewView->IsThirdPerson();

		SVehicleEventParams params;
		params.bParam = bIsThirdPerson;
		m_pVehicle->BroadcastVehicleEvent(eVE_ViewChanged, params);

		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams vehicle_Movement_Params;
			vehicle_Movement_Params.fValue = bIsThirdPerson ? 0.0f : 1.0f;
			pMovement->OnEvent(IVehicleMovement::eVME_PlayerSwitchView, vehicle_Movement_Params);
		}
	}

	m_currentView = viewId;

	return true;
}

//------------------------------------------------------------------------
TVehicleViewId CVehicleSeat::GetNextViewId(bool bExcludeDebug, bool bExcludeDisabled) const
{
	TVehicleViewId nextId = m_currentView + 1;
	for (int i = 0; i < m_views.size(); ++i)
	{
		if (nextId > m_views.size())
		{
			nextId = FirstVehicleViewId;
		}

		IVehicleView* pNextView = GetView(nextId);
		if ((bExcludeDebug && pNextView->IsDebugView()) || (bExcludeDisabled && !pNextView->IsEnabled()))
		{
			nextId++;
		}
		else
		{
			// found good view
			return nextId;
		}
	}

	// looped without finding an adequate view. The current could be the only one or also invalid, in either case, return invalid.
	return InvalidVehicleViewId;
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdateView(SViewParams& viewParams)
{
	CRY_ASSERT(m_passengerId);

	if ((m_transitionType == eVT_None) || (VehicleCVars().v_playerTransitions == 1))
	{
		if (IVehicleView* pView = GetCurrentView())
			pView->UpdateView(viewParams, m_passengerId);
	}
	else
	{
		if (IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId))
		{
			IEntity* pEntity = pActor->GetEntity();
			CRY_ASSERT(pEntity);

			if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(0))
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				CRY_ASSERT(pSkeletonPose);

				int16 headId = rIDefaultSkeleton.GetJointIDByName("Bip01 Head");
				if (headId > -1)
				{
					Matrix34 worldTM = pEntity->GetSlotWorldTM(0) * Matrix34(pSkeletonPose->GetAbsJointByID(headId));
					worldTM.OrthonormalizeFast();

					viewParams.position = worldTM.GetTranslation();
					viewParams.rotation = Quat(Matrix33(worldTM)).GetNormalized() * Quat::CreateRotationY(gf_PI * 0.5f);

					if (ICVar* pVar = gEnv->pConsole->GetCVar("cl_fov"))
						viewParams.fov = DEG2RAD(pVar->GetFVal());
					else
						viewParams.fov = 0.0f;

					viewParams.nearplane = 0.0f;
				}
			}
		}
	}

	if (!m_isPlayerViewBlending || m_skipViewBlending)
	{
		viewParams.SetViewID(2, false);
		m_skipViewBlending = false;
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdateSounds(float deltaTime)
{
	if (!(m_transitionType != eVT_RemoteUsage && IsPassengerClientActor()))
		return;

	SSeatSoundParams& params = GetSoundParams();
	const char* moodName = params.moodName.c_str();

	float mood = params.mood;

	if (IVehicleView* pView = GetCurrentView())
	{
		if (pView->IsThirdPerson())
		{
			mood = 0.f;
		}
	}

	REINST("update state for being in-car");
}

//------------------------------------------------------------------------
void CVehicleSeat::StopSounds()
{
	REINST("is this needed?");
	GetSoundParams().moodCurr = 0.f;
}

//------------------------------------------------------------------------
EntityId CVehicleSeat::NetGetPassengerId() const
{
	return m_passengerId;
}

//------------------------------------------------------------------------
void CVehicleSeat::NetSetPassengerId(EntityId passengerId)
{
	if (passengerId != m_passengerId)
	{
		//CryLog("%s::NetSetPassenger: <%s> %i -> %i", m_pVehicle->GetEntity()->GetClass()->GetName(), m_name.c_str(), m_passengerId, passengerId);

		IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
		CRY_ASSERT(pActorSystem);

		IActor* pActor = pActorSystem->GetActor(passengerId ? passengerId : m_passengerId);
		if (!pActor)
			return;

		// resolve the previous seat situation if any,
		// because passengers need to leave their current seat before entering the new one
		CVehicleSeat* pPrevSeat = (CVehicleSeat*)m_pVehicle->GetSeatForPassenger(passengerId);
		if (pPrevSeat)
		{
			if (pPrevSeat->m_transitionType == eVT_RemoteUsage)
				pPrevSeat->ExitRemotely();
			else
			{
				pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->StopAnimationsAllLayers();

				pPrevSeat->GivesActorSeatFeatures(false);

				pPrevSeat->m_passengerId = 0;

				SVehicleEventParams ep;
				ep.entityId = passengerId;
				ep.iParam = m_seatId;
				m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, ep);
			}
		}

		EntityId exitId = m_passengerId;
		EntityId enterId = (!pActor->IsDead() && !m_pVehicle->IsDestroyed() && passengerId) ? passengerId : 0;

		if (exitId)
		{
			if (m_prevTransitionType == eVT_RemoteUsage)
			{
				ExitRemotely();
			}
			else
			{
				CryLog("[VEHICLE] [%s] SERIALISING EXIT: Vehicle[%u] Passenger[%u]", gEnv->bServer ? "SERVER" : "CLIENT", m_pVehicle->GetEntityId(), m_passengerId);

				Exit(true, true);
			}

			m_prevTransitionType = eVT_None;
		}

		if (enterId)
		{
			if (pActor->GetLinkedEntity() != m_pVehicle->GetEntity())
			{
				Enter(enterId, (VehicleCVars().v_playerTransitions == 1));
			}
			else
			{
				CryLog("[VEHICLE] [%s] SERIALISING ENTR: Vehicle[%d] Passenger[%d]", gEnv->bServer ? "SERVER" : "CLIENT", m_pVehicle->GetEntityId(), passengerId);

				m_passengerId = passengerId;
				if (m_transitionType != eVT_RemoteUsage)
				{
					if (!IsRemoteControlledInternal())
					{
						SitDown();
					}
				}
				else
				{
					EnterRemotely(enterId);
				}
			}
			m_prevTransitionType = m_transitionType;
		}

		CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.IsWriting())
		{
			ser.Value("passengerId", m_passengerId, 'eid');

			IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
			bool forceEnter = pActor && pActor->GetEntity();

			ser.Value("forceEnter", forceEnter);
			ser.EnumValue("isLocked", m_lockStatus, eVSLS_Unlocked, eVSLS_Locked);
			ser.Value("transition", m_transitionType);
			ser.Value("queueTransition", m_queuedTransition);
		}
		else if (ser.IsReading())
		{
			EntityId passengerId;
			int transitionType = 0, queueTransitionType = 0;

			bool forceEnter = false;

			ser.Value("passengerId", passengerId, 'eid');
			ser.Value("forceEnter", forceEnter);

			ser.EnumValue("isLocked", m_lockStatus, eVSLS_Unlocked, eVSLS_Locked);
			ser.Value("transition", transitionType);
			ser.Value("queueTransition", queueTransitionType);

			IActor* pActor = NULL;
			if (passengerId)
				pActor = m_pGameFramework->GetIActorSystem()->GetActor(passengerId);

			IActor* pActorBefore = NULL;
			if (m_passengerId)
			{
				pActorBefore = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
				if ((pActorBefore != NULL) && pActorBefore->IsDead())
					Exit(false, true);
			}

			// remove current passenger if necessary
			if (m_passengerId && (!pActor || passengerId != m_passengerId))
				Exit(false, true);

			if (pActor)
			{
				bool remote = (transitionType == eVT_RemoteUsage);

				if (passengerId != GetPassenger(remote))
				{
					// exit previous seat if any
					if (!remote)
					{
						if (IVehicle* pPrevVehicle = pActor->GetLinkedVehicle())
						{
							if (IVehicleSeat* pPrevSeat = pPrevVehicle->GetSeatForPassenger(pActor->GetEntityId()))
								pPrevSeat->Exit(false, true);
						}
					}

					m_postEnterId = passengerId;

					/*if (remote)
					   EnterRemotely(passengerId);
					   else
					   Enter(passengerId, false);
					 */
				}

				if (forceEnter)
				{
					m_postEnterId = passengerId;
				}
			}

			m_transitionType = transitionType;
			m_queuedTransition = queueTransitionType;
			m_passengerId = passengerId;
			m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_PassengerIn);
		}

		TVehicleViewId view = m_currentView;
		ser.Value("currentViewId", view);

		if (ser.IsReading() && view != m_currentView && m_transitionType != eVT_RemoteUsage)
		{
			m_postViewId = view;
		}

		if (IVehicleView* pView = GetView(view))
			pView->Serialize(ser, aspects);

		int i = 0;
		char pSeatActionString[32];

		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			cry_sprintf(pSeatActionString, "seatAction_%d", ++i);

			ser.BeginGroup(pSeatActionString);
			ite->Serialize(ser, aspects);
			ser.EndGroup();
		}
	}
	// network
	else
	{
		if (aspects & CVehicle::ASPECT_SEAT_PASSENGER)
		{
			NET_PROFILE_SCOPE("VehicleSeat", ser.IsReading());
			bool remotelyUsed = m_transitionType == eVT_RemoteUsage;
			ser.Value("remotelyUsed", remotelyUsed, 'bool');
			if (ser.IsReading() && remotelyUsed)
				m_transitionType = eVT_RemoteUsage;

			ser.Value("passengerId", this, &CVehicleSeat::NetGetPassengerId, &CVehicleSeat::NetSetPassengerId, 'eid');
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::PostSerialize()
{
	IActor* pActor = m_passengerId ? CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_passengerId) : 0;
	bool remote = (m_transitionType == eVT_RemoteUsage);

	if (pActor)
	{
		if (m_postEnterId)
		{
			if (m_transitionType != eVT_Exiting)
			{
				if (remote)
					EnterRemotely(m_passengerId);
				else if (pActor->GetEntity() && pActor->GetEntity()->GetAI())
				{
					bool needUpdateTM = false;
					if (m_isRagdollingOnDeath == false)
					{
						if (pActor->GetGameObject()->GetAspectProfile(eEA_Physics) != eAP_Alive)
						{
							pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
							needUpdateTM = true;
						}
					}
					Enter(m_passengerId, false);
					if (needUpdateTM)
						UpdatePassengerLocalTM(pActor);

					if (pActor->IsDead())
						OnPassengerDeath();
				}
				else
				{
					pActor->HolsterItem(true);
					pActor->MountedGunControllerEnabled(false);

					pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
					Enter(m_passengerId, true);
				}
			}

			m_postEnterId = 0;
		}

		if (!remote)
		{
			pActor->HolsterItem(true);

			if (m_postViewId)
			{
				SetView(m_postViewId);
				m_postViewId = 0;
			}
		}

		if (pActor->IsClient())
		{
			// don't interpolate view in this case
			IVehicleView* pView = GetView(m_currentView);
			if (pView)
			{
				pView->ResetPosition();
			}
			m_skipViewBlending = true;
		}
	}

	if (m_transitionType == eVT_Exiting)
		StandUp();

	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		ite->pSeatAction->PostSerialize();
	}
}

//------------------------------------------------------------------------
CVehicleSeatSerializer* CVehicleSeat::GetSerializer()
{
	return m_pSerializer;
}

//------------------------------------------------------------------------
void CVehicleSeat::SetSerializer(CVehicleSeatSerializer* pSerializer)
{
	if (m_serializerId)
		return;

	m_pSerializer = pSerializer;
	m_serializerId = m_pSerializer->GetEntityId();
}

//------------------------------------------------------------------------
float CVehicleSeat::ProcessPassengerDamage(float actorHealth, float damage, int hitTypeId, bool explosion)
{
	bool fire = (hitTypeId == CVehicle::s_fireHitTypeId);
	bool punish = (hitTypeId == CVehicle::s_punishHitTypeId);

	if (!punish && m_isPassengerShielded && !m_pVehicle->IsDestroyed())
	{
		if (explosion || fire)
			return 0.0f;
	}

	// todo: damage should not depend on sound parameters...
	if (fire)
		return damage * m_soundParams.inout;

	return damage;
}

//------------------------------------------------------------------------
void CVehicleSeat::OnPassengerDeath()
{
	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
	CRY_ASSERT(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_passengerId);
	if (!pActor)
		return;

	if (m_transitionType == eVT_RemoteUsage)
	{
		CRY_ASSERT(0 && "OnPassengerDeath called on remotely used seat");
		return;
	}

	if (m_pSeatGroup)
		m_pSeatGroup->OnPassengerExit(this, m_passengerId);

	if (m_isRagdollingOnDeath || m_transitionType == eVT_Entering || m_transitionType == eVT_Exiting ||
	    VehicleCVars().v_ragdollPassengers == 1 || m_pVehicle->IsDestroyed() || pActor->IsPlayer())
	{
		UnlinkPassenger(true);

		if (pActor->IsPlayer() && m_pEnterHelper)
		{
			Matrix34 tm;
			m_pEnterHelper->GetWorldTM(tm);
			pActor->GetEntity()->SetWorldTM(tm);
		}

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw)
			CryLog("unlinked %s", pActor->GetEntity()->GetName());
#endif
	}
	else
	{
		m_transitionType = eVT_Dying;
		GivesActorSeatFeatures(false);

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw)
			CryLog("%s: setting transition type eVT_Dying", pActor->GetEntity()->GetName());
#endif
	}

	// hide non-players when died from vehicle explosion
	if (!pActor->IsPlayer() && (m_pVehicle->IsDestroyed() || m_isPassengerHidden))
	{
#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw)
			CryLog("hiding %s during destruction of vehicle %s", pActor->GetEntity()->GetName(), m_pVehicle->GetEntity()->GetName());
#endif

		pActor->GetEntity()->Hide(true);
	}

	// broadcast an event about the passenger exiting and that the seat is now free
	SVehicleEventParams eventParams;
	eventParams.entityId = pActor->GetEntityId();
	eventParams.iParam = m_seatId;
	m_pVehicle->BroadcastVehicleEvent(eVE_PassengerExit, eventParams);
	m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, eventParams);

	if (m_pVehicle->IsEmpty())
		m_pVehicle->StartAbandonedTimer();
}

//------------------------------------------------------------------------
void CVehicleSeat::UnlinkPassenger(bool ragdoll)
{
	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
	CRY_ASSERT(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_passengerId);
	if (!pActor)
		return;

	GivesActorSeatFeatures(false);

	if (m_pVehicle->GetAnimationComponent().IsEnabled())
	{
		m_pVehicle->GetAnimationComponent().AttachPassengerScope(this, m_passengerId, false);
	}

	pActor->LinkToVehicle(0);
	m_passengerId = 0;

	if (ragdoll && gEnv->bServer && !pActor->IsGod())
	{
#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw)
			CryLog("%s: setting ragdoll physicalization profle", pActor->GetEntity()->GetName());
#endif

		pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
	}

	m_transitionType = eVT_None;

	if (pActor->IsClient())
		StopSounds();

	CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
}

//------------------------------------------------------------------------
bool CVehicleSeat::ProcessPassengerMovementRequest(const CMovementRequest& movementRequest)
{
	IActor* pActor = GetPassengerActor();
	if (pActor && !pActor->IsPlayer())
	{
		if (!(m_transitionType == eVT_None || m_transitionType == eVT_RemoteUsage))
			return false;

		if (movementRequest.HasLookTarget() || movementRequest.HasAimTarget())
		{
			Vec3 vTurretGoal(ZERO);

			if (movementRequest.HasFireTarget())
				vTurretGoal = movementRequest.GetFireTarget();
			else if (movementRequest.HasAimTarget())
				vTurretGoal = movementRequest.GetAimTarget();
			else if (movementRequest.HasLookTarget())
				vTurretGoal = movementRequest.GetLookTarget();

			for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
			{
				IVehicleSeatAction* pSeatAction = ite->pSeatAction;

				if (CVehicleSeatActionRotateTurret* pActionRotateTurret = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction))
				{
					pActionRotateTurret->SetAimGoal(vTurretGoal, 1);
				}

				if (CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
				{
					if (movementRequest.HasFireTarget())
						pActionWeapons->SetFireTarget(movementRequest.GetFireTarget());
					else if (movementRequest.HasAimTarget())
						pActionWeapons->SetFireTarget(movementRequest.GetAimTarget());
					else
						pActionWeapons->SetFireTarget(Vec3(0, 0, 0));
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::RequestMovement(CMovementRequest& movementRequest)
{
	if (m_isDriver)
	{
		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			pMovement->RequestMovement(movementRequest);
	}

	return true;
}

// we use this function for computing SMovementState::eyeDirection and SMovementState::upDirecton from a transformation matrix that might hold a scaling
static Vec3 RotateAndNormalize(const Matrix34& tm, const Vec3& vectorToRotateAndNormalize)
{
	Matrix33 orientation;
	tm.GetRotation33(orientation);
	const Vec3 transformedVector = orientation * vectorToRotateAndNormalize;
	return transformedVector.GetNormalized();
}

//------------------------------------------------------------------------
void CVehicleSeat::GetMovementState(SMovementState& movementState)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	IEntity* pVehicleEntity = m_pVehicle->GetEntity();
	if (!pVehicleEntity)
		return;

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_passengerId);
	IEntity* pActorEntity = pActor ? pActor->GetEntity() : NULL;

	const Matrix34& worldTM = pVehicleEntity->GetWorldTM();

	// todo: is this really intended?
	movementState.pos = worldTM.GetTranslation();
	movementState.upDirection = RotateAndNormalize(worldTM, Vec3(0, 0, 1));
	// ~todo

	if (m_pAimPart)
	{
		const Matrix34& partWorldTM = m_pAimPart->GetWorldTM();
		movementState.eyePosition = partWorldTM.GetTranslation();
		movementState.eyeDirection = RotateAndNormalize(partWorldTM, FORWARD_DIRECTION);
	}
	else
	{
		if (!pActor || !m_pSitHelper) // || !pActor->IsPlayer())
		{
			movementState.eyeDirection = RotateAndNormalize(worldTM, FORWARD_DIRECTION);
			movementState.eyePosition = worldTM.GetTranslation();
		}
		else
		{
			Matrix34 seatWorldTM;
			m_pSitHelper->GetWorldTM(seatWorldTM);

			if (m_autoAimEnabled)
			{
				const Matrix34& viewMatrix = GetISystem()->GetViewCamera().GetMatrix();
				movementState.eyeDirection = viewMatrix.GetColumn1(); // assumes no scaling
				movementState.eyePosition = viewMatrix.GetTranslation();
			}
			else
			{
				movementState.eyeDirection = RotateAndNormalize(seatWorldTM, FORWARD_DIRECTION);

				if (pActor != NULL)
					movementState.eyePosition = seatWorldTM.GetTranslation() + seatWorldTM.TransformVector(pActor->GetLocalEyePos());
				else
					movementState.eyePosition = seatWorldTM.GetTranslation();
			}

		}
	}

	if (m_pAIVisionHelper && pActorEntity && pActorEntity->HasAI())
	{
		const Vec3 vAIVisionHelperPos = m_pAIVisionHelper->GetWorldSpaceTranslation();
		movementState.eyePosition = vAIVisionHelperPos;
		movementState.weaponPosition = vAIVisionHelperPos;
	}
	else
	{
		movementState.weaponPosition = movementState.eyePosition;
	}

	// this is needed for AI LookAt to work correctly
	movementState.animationEyeDirection = movementState.eyeDirection;
	//	pActor->GetLocalEyePos(0);

	movementState.stance = STANCE_NULL;
	movementState.atMoveTarget = false;
	pVehicleEntity->GetWorldBounds(movementState.m_StanceSize);
	movementState.m_StanceSize.Move(-movementState.pos);
	movementState.m_ColliderSize = movementState.m_StanceSize;

	movementState.fireDirection = movementState.eyeDirection;
	movementState.aimDirection = movementState.eyeDirection;
	movementState.entityDirection = movementState.eyeDirection;
	movementState.animationBodyDirection = movementState.eyeDirection;
	movementState.movementDirection = movementState.eyeDirection;

	if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		pMovement->GetMovementState(movementState);

	if (pActor)
	{
		if (!pActor->IsPlayer() && m_pWeaponAction)
		{
			if (m_isDriver)
			{
				movementState.fireTarget = movementState.eyePosition + 10.f * movementState.eyeDirection;
			}
			else
			{
				Vec3 vTmp(ZERO);
				EntityId weaponId = m_pWeaponAction->GetWeaponEntityId(m_pWeaponAction->GetCurrentWeapon());
				m_pWeaponAction->GetActualWeaponDir(weaponId, 0, movementState.aimDirection, Vec3(ZERO), vTmp);
				m_pWeaponAction->GetFiringPos(weaponId, 0, movementState.weaponPosition);
				movementState.aimDirection.Normalize();
			}
		}
		else if (pActor->IsClient() && pActor->IsThirdPerson())
		{
			movementState.weaponPosition = GetISystem()->GetViewCamera().GetPosition();
		}
	}

	movementState.isAiming = true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::GetStanceState(const SStanceStateQuery& query, SStanceState& state)
{
	IEntity* pEntity = m_pVehicle->GetEntity();
	// Returns one stance.
	if (query.stance != STANCE_STAND || !pEntity)
		return false;

	// TODO: This information is very approximate at the moment.
	if (query.defaultPose)
	{
		AABB aabb;
		pEntity->GetLocalBounds(aabb);

		state.pos.zero();
		state.entityDirection = FORWARD_DIRECTION;
		state.animationBodyDirection = FORWARD_DIRECTION;
		state.upDirection(0, 0, 1);
		state.weaponPosition.zero();
		state.aimDirection = FORWARD_DIRECTION;
		state.fireDirection = FORWARD_DIRECTION;
		state.eyePosition.zero();
		state.eyeDirection = FORWARD_DIRECTION;
		state.m_StanceSize = aabb;
		state.m_ColliderSize = aabb;
	}
	else
	{
		Vec3 entityPos = pEntity->GetWorldPos();
		Vec3 forward = pEntity->GetWorldTM().TransformVector(FORWARD_DIRECTION);
		AABB aabb;
		pEntity->GetWorldBounds(aabb);
		aabb.Move(-entityPos);

		state.pos = entityPos;
		state.entityDirection = forward;
		state.animationBodyDirection = forward;
		state.upDirection(0, 0, 1);
		state.weaponPosition = entityPos;
		state.aimDirection = forward;
		state.fireDirection = forward;
		state.eyePosition = entityPos;
		state.eyeDirection = forward;
		state.m_StanceSize = aabb;
		state.m_ColliderSize = aabb;
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::RequestFire(bool bFire)
{
	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		IVehicleSeatAction* pSeatAction = ite->pSeatAction;
		if (CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
		{
			if (m_aiWeaponId == 0 || m_aiWeaponId == pActionWeapons->GetWeaponEntityId(0))
			{
				if (bFire)
				{
					if (pActionWeapons->ForcedUsage())
					{
						pActionWeapons->StartUsing();
					}

					pActionWeapons->StartFire();
				}
				else
				{
					if (pActionWeapons->ForcedUsage())
					{
						pActionWeapons->StopUsing();
					}

					pActionWeapons->StopFire();
				}

				return true;
			}
		}
	}

	return false;
}

void CVehicleSeat::UpdateTargetPosAI(const Vec3& pos)
{
	const IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
	if (pActor && pActor->IsPlayer())
	{
		return;
	}

	if (m_pWeaponAction)
		m_pWeaponAction->SetFireTarget(pos);
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicleSeat::GetNextSeatId(const TVehicleSeatId currentSeatId) const
{
	return m_pVehicle->GetNextSeatId(currentSeatId);
}

//------------------------------------------------------------------------
CGameChannel* CVehicleSeat::GetGameChannel(EntityId actorId)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(actorId);
	if (pActor)
	{
		int channelId = pActor->GetChannelId();
		if (channelId)
		{
			CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
			return pServerNub->GetChannel(channelId);
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CVehicleSeat::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	// NB: seat actions which require events should register themselves as vehicle listeners,
	//	to avoid calling all of them from here.

	switch (event)
	{
	case eVE_Destroyed:
		{
			if (m_passengerId)
			{
				if (!IsRemoteControlledInternal())
				{
					// kill non-dead passengers
					IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
					if (!pActor)
						return;

					if (gEnv->bServer && !pActor->IsDead())
					{
						IGameRules* pGameRules = m_pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules();
						if (pGameRules)
						{
							HitInfo hit;

							EntityId shooterId = params.entityId;
							if (shooterId == m_pVehicle->GetEntityId())
							{
								if (m_pVehicle->GetDriver())
									shooterId = m_pVehicle->GetDriver()->GetEntityId();
							}

							hit.targetId = m_passengerId;
							hit.shooterId = shooterId;
							hit.weaponId = m_pVehicle->GetEntityId();
							hit.damage = gEnv->bMultiplayer ? 1000.0f : 1000000.0f;
							hit.type = CVehicle::s_vehicleDestructionTypeId;
							hit.pos = pActor->GetEntity()->GetWorldPos();

							pGameRules->ServerHit(hit);
						}
					}
					else
					{
						if (!pActor->IsPlayer())
						{
							Exit(false, true);
							if (m_pVehicle->GetMovement() && m_pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
							{
								pActor->HolsterItem(false);
								pActor->GetEntity()->Hide(true);
							}
						}
					}
				}
				else
				{
					Exit(false, true);
				}
			}
		}
		break;
	case eVE_ToggleDriverControlledGuns:
		{
			if (!params.bParam)
			{
				if (m_passengerId && m_transitionType == eVT_RemoteUsage && IsGunner())
				{
					ExitRemotely();

					if (GetCurrentViewId() > FirstVehicleViewId) //? should it mean if it is not the FirstVehicleViewId?
						SetView(FirstVehicleViewId);
				}
			}
			else
			{
				if (m_passengerId && IsDriver())
				{
					if (CVehicleSeat* pSeat = GetSeatUsedRemotely(false))
					{
						if (!pSeat->GetPassenger(true))
						{
							pSeat->EnterRemotely(m_passengerId);

							if (GetCurrentViewId() > FirstVehicleViewId) //? should it mean if it is not the FirstVehicleViewId?
								SetView(FirstVehicleViewId);
						}
					}
				}
			}
		}
		break;
	case eVE_SeatFreed:
		OnSeatFreed(params.iParam, params.entityId);
		break;
	case eVE_PassengerEnter:
		{
			REINST("send trigger?");
		}
		break;
	case eVE_PassengerExit:
		{
			REINST("send trigger?");
		}
		break;
	case eVE_Hide:
		{
			if (params.iParam)
			{
				// Hiding!
				REINST("send trigger?");
			}
			else
			{
				// Unhiding!
				REINST("send trigger?");
			}
		}
		break;
	default:
		break;
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsGunner() const
{
	if (m_pWeaponAction)
		return true;

	if (CVehicleSeat* pSeat = GetSeatUsedRemotely(true))
		return pSeat->IsGunner();

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsLocked(IActor* pActor) const
{
	bool player = pActor ? pActor->IsPlayer() : false;
	bool locked = false;

	switch (m_lockStatus)
	{
	case eVSLS_Unlocked:
		locked = false;
		break;

	case eVSLS_Locked:
		locked = true;
		break;

	case eVSLS_LockedForAI:
		locked = !player;
		break;

	case eVSLS_LockedForPlayers:
		locked = player;
		break;
	}

	return locked;
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdateHiddenState(bool hide)
{
	if (!m_isPassengerHidden)
		return;

	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_passengerId);
	if (!pEnt)
		return;

	if (gEnv->bMultiplayer)
	{
		pEnt->Hide(hide);
	}
	else
	{
		pEnt->Invisible(hide);
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::OnCameraShake(float& angleAmount, float& shiftAmount, const Vec3& pos, const char* source) const
{
	if (!strcmp(source, "explosion"))
	{
		angleAmount *= m_explosionShakeMult;
		shiftAmount *= m_explosionShakeMult;
	}
}

//------------------------------------------------------------------------
Matrix34 CVehicleSeat::GetExitTM(IActor* pActor, bool opposite /*=false*/)
{
	Matrix34 worldTM;
	IEntity* pEntity = m_pVehicle->GetEntity();
	const Matrix34& vehicleWorldTM = pEntity->GetWorldTM();

	AABB bounds;
	pEntity->GetLocalBounds(bounds);

	if (m_pExitHelper)
	{
		if (!opposite)
			m_pExitHelper->GetWorldTM(worldTM);
		else
			m_pExitHelper->GetReflectedWorldTM(worldTM);
	}
	else if (m_pEnterHelper)
	{
		if (!opposite)
			m_pEnterHelper->GetWorldTM(worldTM);
		else
			m_pEnterHelper->GetReflectedWorldTM(worldTM);
	}
	else
	{
		worldTM.SetIdentity();
		Vec3 pos = vehicleWorldTM * Vec3(bounds.min.x, bounds.min.y, 0.f);
		AABB worldBounds = AABB::CreateTransformedAABB(vehicleWorldTM, bounds);
		pos.z = min(pos.z + bounds.max.z, worldBounds.max.z);
		worldTM.SetTranslation(pos);
	}

	if (pActor && pActor->IsPlayer())
	{
		Matrix34 worldTMInv = vehicleWorldTM.GetInverted();
		Vec3 localPos = worldTMInv.TransformPoint(worldTM.GetTranslation());

		// for players, we add a sideways safety gap
		// as helpers are usually close enough to generate collisions when exiting while driving fast & not straight
		// default 0.5m
		Vec3 offset(m_exitOffsetPlayer);
		if (offset.IsZero())
		{
			offset.x = sgn(localPos.x) * 0.5f;
		}

		// additional offset to account for angular velocity
		// possibly replace by physical bounds check (and integrate with general intersection checks) after alpha
		if (m_pVehicle->GetStatus().speed > 0.1f)
		{
			pe_status_dynamics dyn;
			if (IPhysicalEntity* pEntityPhysics = pEntity->GetPhysics())
			{
				if (pEntityPhysics->GetStatus(&dyn))
				{
					if (dyn.w.GetLengthSquared() > sqr(0.2f))
					{
						Vec3 localW = worldTMInv.TransformVector(dyn.w);
						if (localW.z * localPos.x > 0.f || localW.y > 0.25f)
							offset.x += sgn(offset.x) * min(4.f * max(abs(localW.z), abs(localW.y)), bounds.max.x);
					}
				}
			}
		}

		worldTM.SetTranslation(worldTM.GetTranslation() + vehicleWorldTM.TransformVector(offset));

		/*pe_status_pos pos;
		   pEntity->GetPhysics()->GetStatus(&pos);

		   // estimate local bounds after vehicle moved about half its length along its current velocity
		   Vec3 dp = dyn.v.GetNormalized() * 0.5f*bounds.GetSize().y;
		   float dt = 0.5f*bounds.GetSize().y / m_pVehicle->GetStatus().speed;
		   Vec3 posNew = vehicleWorldTM.GetTranslation() + dp;
		   Quat rotNew = vehicleWorldRotation;
		   if (dyn.w.GetLengthSquared()>0.01f)
		   {
		   rotNew *= Quat(dyn.w*dt);
		   rotNew.Normalize();
		   }

		   OBB obbNew = OBB::CreateOBBfromAABB(rotNew, bounds);

		   Matrix34 newWorldTM(IDENTITY);
		   newWorldTM.SetTranslation(posNew);

		   IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		   pGeom->SetRenderFlags(e_Def3DPublicRenderflags);
		   pGeom->DrawOBB(obbNew, vehicleWorldTM.GetTranslation(), false, ColorB(255,0,0,200), eBBD_Faceted);
		 */

	}

	return worldTM;
}

//------------------------------------------------------------------------
bool CVehicleSeat::TestExitPosition(IActor* pActor, Vec3& testPos, EntityId* pBlockingEntity)
{
	if (!pActor)
		return false;

	IEntity* pEntity = m_pVehicle->GetEntity();

	Vec3 startPos = pActor->GetEntity()->GetWorldPos();
	IVehicleHelper* pSeatHelper = GetSitHelper();
	if (pSeatHelper)
		startPos = pSeatHelper->GetWorldSpaceTranslation();

	if ((startPos - testPos).GetLengthSquared() > sqr(10.f))
	{
		// Something has gone wrong! DT: 1010. Use the vehicle pos
		startPos = pEntity->GetWorldPos();
	}

	Vec3 dir = testPos - startPos;
	dir.z += 0.3f;

	// first test vehicle velocity. if the vehicle is moving in the general direction of this helper
	//	(most likely when sliding to a halt) then disallow this exit pos to prevent running yourself over
	const Matrix34& vehicleWorldTM = pEntity->GetWorldTM();
	Matrix34 worldTMInv = vehicleWorldTM.GetInverted();
	if (m_pVehicle->GetStatus().speed > 1.0f)
	{
		pe_status_dynamics dyn;
		if (IPhysicalEntity* pEntityPhysics = pEntity->GetPhysics())
		{
			if (pEntityPhysics->GetStatus(&dyn))
			{
				Vec3 localV = worldTMInv.TransformVector(dyn.v);
				Vec3 localDir = worldTMInv.TransformVector(dir);

				if (abs(localV.x) > 2.0f && sgn(localV.x) == sgn(localDir.x))
				{
					//CryLog("Disallowing vehicle exit due to sliding");
					return false;
				}
			}
		}
	}

	static IPhysicalEntity* pSkipEnts[11];
	int nskip = m_pVehicle->GetSkipEntities(pSkipEnts, 10);
	// add the actor specifically
	pSkipEnts[nskip++] = pActor->GetEntity()->GetPhysics();
	return m_pVehicle->ExitSphereTest(pSkipEnts, nskip, startPos, testPos, pBlockingEntity);
}

//------------------------------------------------------------------------
IVehicleClient* CVehicleSeat::GetVehicleClient() const
{
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	CRY_ASSERT(pVehicleSystem);

	return pVehicleSystem->GetVehicleClient();
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsAutoAimEnabled()
{
	return m_autoAimEnabled;
}

//------------------------------------------------------------------------
IVehicleSeatAction* CVehicleSeat::GetISeatActionWeapons()
{
	return GetSeatActionWeapons();
}

//------------------------------------------------------------------------
const IVehicleSeatAction* CVehicleSeat::GetISeatActionWeapons() const
{
	return GetSeatActionWeapons();
}

//------------------------------------------------------------------------
void CVehicleSeat::PrePhysUpdate(const float dt)
{
	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(); it != m_seatActions.end(); ++it)
	{
		if (it->isEnabled && it->pSeatAction)
		{
			it->pSeatAction->PrePhysUpdate(dt);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	{
		SIZER_COMPONENT_NAME(s, "Views");
		s->AddObject(m_views);
	}
	{
		SIZER_COMPONENT_NAME(s, "Actions");
		s->AddObject(m_seatActions);
	}
	s->AddObject(m_seatNameToUseForEntering);
	s->AddObject(m_useActionsRemotelyOfSeat);
}

void CVehicleSeat::OnVehicleActionControllerDeleted()
{
	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		SSeatActionData& seatActionData = *ite;

		if (!seatActionData.pSeatAction)
			continue;

		if (CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, seatActionData.pSeatAction))
		{
			pActionWeapons->OnVehicleActionControllerDeleted();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehicleSeat::OnCharacterChange()
{
	// Re-enslave
	CVehicleAnimationComponent& animationComponent = m_pVehicle->GetAnimationComponent();
	IActionController* pActionController = animationComponent.GetActionController();
	const bool shouldEnslaveActor = ShouldEnslaveActorAnimations();
	const SControllerDef* pContDef = NULL;

	if (pActionController && shouldEnslaveActor)
	{
		animationComponent.AttachPassengerScope(this, m_passengerId, false);
		animationComponent.AttachPassengerScope(this, m_passengerId, true);
		pContDef = &pActionController->GetContext().controllerDef;
	}

	if (pActionController)
	{
		if (m_seatFragmentIDs[eMannequinSeatAction_Idle] != TAG_ID_INVALID)
		{
			if (m_pIdleAction)
			{
				m_pIdleAction->Stop();
			}
			m_pIdleAction = new TAction<SAnimationContext>(IVehicleAnimationComponent::ePriority_SeatDefault, m_seatFragmentIDs[eMannequinSeatAction_Idle], TAG_STATE_EMPTY);
			pActionController->Queue(*m_pIdleAction);
		}
	}

}

//------------------------------------------------------------------------
void CVehicleSeat::OffsetPosition(const Vec3& delta)
{
	for (TVehicleViewVector::iterator it = m_views.begin(); it != m_views.end(); ++it)
		(*it)->OffsetPosition(delta);
}
