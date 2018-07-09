// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Class for specific JAW rocket launcher functionality
-------------------------------------------------------------------------
History:
- 22:06:2007: Created by Benito G.R.
- 30:10:2009: Ported from RocketLauncher

*************************************************************************/

#include "StdAfx.h"
#include "JAW.h"

#include "GameActions.h"
#include "GameInputActionHandlers.h"
#include "Actor.h"
#include "GameRules.h"
#include "GameCVars.h"
#include "Player.h"
#include "PlayerStateEvents.h"
#include "Single.h"

#include "ItemSharedParams.h"
#include "WeaponSharedParams.h"

#include "WeaponSystem.h"
#include <CryAISystem/IAIActor.h>
#include "Projectile.h"
#include "ActorManager.h"

#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "TacticalManager.h"

namespace
{
	const float g_hideTimeAfterDrop = 20.0f;
	const float g_autoDropDelayTime = 1.5f;
}



CJaw::CJaw()
: m_forcedTargetId(0)
, m_firedRockets(0)
, m_autoDropPendingTimer(0.0f)
, m_dropTime(0.0f)
, m_auxSlotUsed(false)
, m_auxSlotUsedBQS(false)
, m_autoDropping(false)
, m_controllingRocket(false)
, m_zoomTriggerDown(false)
, m_fireTriggerDown(false)
, m_firePending(false)
, m_smokeActive(false)
, m_dropped(false)
, m_playedDropAction(false)
, m_fired(false)
, m_zoomAlreadyToggled(false)
,	m_extraTubesAdded(false)
{
	CGameInputActionHandlers::TJawActionHandler& jawActionHandler = g_pGame->GetGameInputActionHandlers().GetCJawActionHandler();

	if (jawActionHandler.GetNumHandlers() == 0)
	{
#		define ADD_HANDLER(action, func) jawActionHandler.AddHandler(actions.action, &CJaw::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(zoom, OnActionJawZoom);
		ADD_HANDLER(attack2_xi, OnActionJawZoom);

#		undef ADD_HANDLER
	}

	m_dropJAWAnimEvent = CCrc32::ComputeLowercase("DropJAW");
}



bool CJaw::Init(IGameObject * pGameObject)
{
	if(!CWeapon::Init(pGameObject))
		return false;

	eGeometrySlot geometrySlot = IsOwnerFP() ? eIGS_FirstPerson : eIGS_ThirdPerson;
	m_laserGuider.Initialize(GetEntityId(), &CItemSharedParams::GetDefaultLaserParameters(), geometrySlot);

	return true;
}

void CJaw::UpdateLaser(const SEntityUpdateContext& ctx)
{
	if (!GetEntity()->IsHidden())
	{
		switch (GetZoomState())
		{
		case eZS_ZoomedOut:
		case eZS_ZoomingOut:
			if (m_laserGuider.IsLaserActivated())
			{
				m_laserGuider.TurnOffLaser();
			}
			break;

		case eZS_ZoomingIn:
		case eZS_ZoomedIn:
			if (!m_laserGuider.IsLaserActivated())
			{
				m_laserGuider.TurnOnLaser();
			}
			break;
		}
	}
	else
	{
		if (m_laserGuider.IsLaserActivated())
		{
			m_laserGuider.TurnOffLaser();
		}
	}

	m_laserGuider.UpdateLaser(CLaserBeam::SLaserUpdateDesc(
		GetSlotHelperPos(0, "laser_term", true),
		GetSlotHelperRotation(0, "laser_term", true).GetColumn1(),
		ctx.fFrameTime,
		GetEntity()->IsHidden()));
}

void CJaw::Update(SEntityUpdateContext& ctx, int slot)
{
	CWeapon::Update(ctx, slot);

	if (!IsSelected())
		return;

	if(slot == eIUS_Zooming)
	{
		UpdateLaser(ctx);
	}

	if (slot != eIUS_General)
		return;

	UpdatePendingShot();

	if (!IsInputFlagSet(CWeapon::eWeaponAction_Zoom) && IsInputFlagSet(CWeapon::eWeaponAction_Fire) && !IsZoomingIn())
		AutoZoomOut();

	if (m_controllingRocket && !IsZoomed() && !m_zoomTriggerDown)
		DoAutoDrop();

	if (m_autoDropping)
	{
		m_autoDropPendingTimer += ctx.fFrameTime;
		if (CanAutoDrop())
		{
			m_autoDropping = false;
			DoAutoDrop();
		}
	}

	if (m_dropped)
	{
		m_dropTime += ctx.fFrameTime;
		if (m_dropTime > g_hideTimeAfterDrop)
		{
			if(gEnv->bServer)
			{
				if(gEnv->IsEditor())
					Hide(true);
				else
					gEnv->pEntitySystem->RemoveEntity(GetEntity()->GetId());	
			}
			EnableUpdate(false);
		}
		else
		{
			RequireUpdate();
		}
	}
}



//////////////////////////////////////////////////////////////////////////
void CJaw::OnReset()
{
	bool attachedToBack = IsAttachedToBack();

	if(!attachedToBack)
		m_auxSlotUsed = false;

	BaseClass::OnReset();

	if(!attachedToBack)
	{
		SetViewMode(eIVM_ThirdPerson);
		DrawSlot(eIGS_ThirdPerson, true);
		DrawSlot(eIGS_ThirdPersonAux, false);
	}
	else
		m_stats.first_selection = false;

	m_forcedTargetId = 0;
	m_firedRockets = 0;
	m_dropped = false;
	m_dropTime = 0.0f;
	m_zoomTriggerDown = false;
	m_fireTriggerDown = false;
	m_firePending = false;
	m_playedDropAction = false;
	m_fired = false;
	m_zoomAlreadyToggled = false;
	Hide(false);

	SetFiringLocator(this);

	m_listeners.Clear();

	m_laserGuider.TurnOffLaser();
}

//////////////////////////////////////////////////////////////////////////
void CJaw::ProcessEvent(const SEntityEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	BaseClass::ProcessEvent(event);

	if(event.event == ENTITY_EVENT_RESET)
	{
		//Exiting game mode
		if(gEnv->IsEditor() && !event.nParam[0])
		{
			if(!GetOwner())
			{
				DrawSlot(eIGS_ThirdPerson,true);
				DrawSlot(eIGS_ThirdPersonAux,false);
				m_auxSlotUsed = false;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CJaw::SetAspectProfile(EEntityAspects aspect, uint8 profile)
{
	if(aspect!=eEA_Physics)
		return BaseClass::SetAspectProfile(aspect, profile);

	bool ok = false;
	if(!gEnv->bMultiplayer && gEnv->pSystem->IsSerializingFile() && m_auxSlotUsedBQS)
		ok = true;

	int slot = (m_auxSlotUsed||ok)?eIGS_ThirdPersonAux:eIGS_ThirdPerson;

	if (aspect == eEA_Physics)
	{
		switch (profile)
		{
		case eIPhys_PhysicalizedStatic:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_STATIC;
				params.nSlot = slot;

				GetEntity()->Physicalize(params);

				return true;
			}
			break;
		case eIPhys_PhysicalizedRigid:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_RIGID;
				params.nSlot = slot;
				params.mass = m_sharedparams->params.mass;

				pe_params_buoyancy buoyancy;
				buoyancy.waterDamping = 1.5;
				buoyancy.waterResistance = 1000;
				buoyancy.waterDensity = 0;
				params.pBuoyancy = &buoyancy;

				GetEntity()->Physicalize(params);

				IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
				if (pPhysics)
				{
					pe_action_awake action;
					action.bAwake = m_owner.GetId()!=0;
					pPhysics->Action(&action);
				}
			}
			return true;
		case eIPhys_NotPhysicalized:
			{
				{
					SEntityPhysicalizeParams params;
					params.type = PE_NONE;
					params.nSlot = slot;
					GetEntity()->Physicalize(params);
				}
			}
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::Select(bool select)
{
	BaseClass::Select(select);

	if (select)
	{
		//Force an update, it will check for previous cancelled auto drop request (just in case we got an empty JAW)
		const bool forceDropOnSelect = m_playedDropAction || (IsOwnerClient() && m_fm && m_fm->OutOfAmmo());
		if (forceDropOnSelect)
		{
			m_playedDropAction = false; //Start auto drop again
			HideRocket();
			AutoDrop();
			RequireUpdate(eIUS_General);
		}
	}
	else
	{
		m_laserGuider.TurnOffLaser();
	}

}

//////////////////////////////////////////////////////////////////////////
bool CJaw::GiveExtraTubeToInventory(IActor* pPickerActor, IItemSystem* pItemSystem) const
{
	if (m_weaponsharedparams->ammoParams.extraItems == 0 || !pPickerActor->IsPlayer())
		return false;

	if (m_extraTubesAdded)
		return false;

	IEntityClass* pJawClass = GetEntity()->GetClass();
	IInventory* pPickerInventory = pPickerActor ? pPickerActor->GetInventory() : 0;
	int jawUniqueId = pItemSystem->GetItemUniqueId(pJawClass->GetName());
	int currentNumJaws = pPickerInventory ? pPickerInventory->GetCountOfUniqueId(jawUniqueId) : 0;
	bool giveExtraTube = (currentNumJaws == 0);

	return giveExtraTube;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory, const char* setup)
{
	for (TAmmoVector::iterator it = m_bonusammo.begin(); it != m_bonusammo.end(); ++it)
	{
		SWeaponAmmo& currentBonusAmmo = *it;
		currentBonusAmmo.count = 0;
	}

	IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();
	IActor* pPicketActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pickerId);
	IInventory* pInventory = pPicketActor->GetInventory();
	bool giveExtraTube = GiveExtraTubeToInventory(pPicketActor, pItemSystem);

	if (pPicketActor->IsClient())
		SetAmmoCount(m_fm->GetAmmoType(), 1);

	BaseClass::PickUp(pickerId,sound,select,keepHistory, setup);

	if (giveExtraTube)
	{
		int numJaws = m_weaponsharedparams->ammoParams.extraItems;
		while(numJaws-- > 0)
		{
			pItemSystem->GiveItem(pPicketActor, GetEntity()->GetClass()->GetName(), false, false, false);
		}
		m_extraTubesAdded = true;
	}

	if(m_auxSlotUsed)
	{
		DrawSlot(eIGS_ThirdPersonAux,false);
		m_auxSlotUsed = false;
	}

	if(GetOwnerActor() && !GetOwnerActor()->IsPlayer())
		m_stats.first_selection = false;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::FullSerialize(TSerialize ser)
{
	BaseClass::FullSerialize(ser);

	ser.Value("auxSlotUsed", m_auxSlotUsed);
	ser.Value("smokeActive", m_smokeActive);
	ser.Value("firedRockets", m_firedRockets);
	ser.Value("autoDropPendingTimer", m_autoDropPendingTimer);
	ser.Value("autoDropping", m_autoDropping);

	if (!ser.IsReading())
	{
		m_auxSlotUsedBQS = m_auxSlotUsed;
	}
}

//////////////////////////////////////////////////////////////////////////
void CJaw::PostSerialize()
{
	BaseClass::PostSerialize();

	if(m_auxSlotUsed)
	{
		SetViewMode(0);
		DrawSlot(eIGS_ThirdPersonAux,true);
	}
	else if (GetOwner() != 0 && !IsAttachedToBack() && !IsAttachedToHand())
	{
		Hide(true);
	}
	else if (!IsOwnerFP())
	{
		SetViewMode(eIVM_ThirdPerson);
		DrawSlot(eIGS_ThirdPerson, true);
		DrawSlot(eIGS_ThirdPersonAux,false);
	}

	if (m_smokeActive)
	{
		Pickalize(false, true);
	}

	if (GetOwner() != 0)
	{
		CActor* pOwner = GetOwnerActor();
		IInventory* pInventory = pOwner ? pOwner->GetInventory() : 0;
		UnregisterUsedAmmoWithInventory(pInventory);
		RegisterUsedAmmoWithInventory(pInventory);
	}
}


//////////////////////////////////////////////////////////////////////////
bool CJaw::ShouldDoPostSerializeReset() const
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
void CJaw::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	// No special handling needed when exiting editor game mode
	if (gEnv->IsEditor() && !gEnv->IsEditorGameMode())
		return BaseClass::Drop(impulseScale, selectNext, byDeath);

	CActor* pOwner = GetOwnerActor();
	//Don't let the player drop it if it has not been opened
	if(m_stats.first_selection && pOwner && pOwner->IsPlayer() && !pOwner->IsDead() && !byDeath)
		return;

	if(pOwner && !pOwner->IsPlayer())
	{
		//In this case goes to the clip, no the inventory

		if(m_minDropAmmoAvailable && !m_weaponsharedparams->ammoParams.minDroppedAmmo.empty())
		{
			TAmmoVector::const_iterator end = m_weaponsharedparams->ammoParams.minDroppedAmmo.end();

			for(TAmmoVector::const_iterator it = m_weaponsharedparams->ammoParams.minDroppedAmmo.begin(); it != end; ++it)
			{
				SWeaponAmmoUtils::SetAmmo(m_ammo, it->pAmmoClass, it->count);
			}

			m_minDropAmmoAvailable = false;
		}
	}

	BaseClass::Drop(impulseScale,selectNext,byDeath);
	if (pOwner && !selectNext && !pOwner->IsHeadUnderWater())
	{
		IInventory* pInventory = pOwner->GetInventory();
		EntityId pNextJawItem = pInventory->GetItemByClass(GetEntity()->GetClass());
		m_pItemSystem->SetActorItem(pOwner, pNextJawItem, true);
	}

	if (pOwner && pOwner->IsPlayer())
	{
		if (m_fired)
			DropUsed();
		else
			DropUnused(pOwner);
	}
}

//////////////////////////////////////////////////////////////////////////
void CJaw::DropUsed()
{
	if (!gEnv->IsEditing())
	{
		m_dropped = true;
		EnableUpdate(true);
		Pickalize(false,true);
		CreateSmokeEffect();
		if (g_pGame->GetTacticalManager())
			g_pGame->GetTacticalManager()->RemoveEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);
	}

	if (m_playedDropAction)
	{
		DrawSlot(eIGS_ThirdPersonAux, true);
		DrawSlot(eIGS_ThirdPerson, false);
	}
	else
	{
		DrawSlot(eIGS_ThirdPersonAux, false);
		DrawSlot(eIGS_ThirdPerson, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CJaw::DropUnused(CActor* pOwner)
{
	IInventory* pInventory = pOwner->GetInventory();
	EntityId nextJawEntity = 0;
	for (;;)
	{
		nextJawEntity = pInventory->GetItemByClass(GetEntity()->GetClass());
		if (!nextJawEntity)
			break;
		CJaw* pNextJawItem = static_cast<CJaw*>(m_pItemSystem->GetItem(nextJawEntity));
		if (pNextJawItem)
			pNextJawItem->Drop(1.0f, false, false);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CJaw::CanPickUp(EntityId userId) const
{
	CActor *pActor = GetActor(userId);
	IInventory *pInventory=GetActorInventory(pActor);

	if (m_sharedparams->params.pickable && m_stats.pickable && !m_stats.flying && (!m_owner.GetId() || m_owner.GetId()==userId) && !m_stats.selected && !GetEntity()->IsHidden())
	{
		if (pInventory && pInventory->FindItem(GetEntityId())!=-1)
			return false;
	}
	else
		return false;

	if(GetAmmoCount(m_fm->GetAmmoType())<=0)
	{
		return false;
	}

	uint8 uniqueId = m_pItemSystem->GetItemUniqueId(GetEntity()->GetClass()->GetName());

	int maxNumRockets = GetWeaponSharedParams()->ammoParams.capacityAmmo[0].count;

	if(pInventory && (pInventory->GetCountOfUniqueId(uniqueId)>=maxNumRockets))
	{
		if(pActor->IsClient())
		{
			SHUDEventWrapper::DisplayInfo(eInfo_Warning, 0.5f, "@mp_CannotCarryMoreLAW");
		}
		return false;
	}

	return true;
		
}

//////////////////////////////////////////////////////////////////////////
bool CJaw::CanDrop() const
{
	if (m_sharedparams->params.droppable || (m_sharedparams->params.auto_droppable && m_playedDropAction))
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::GetAttachmentsAtHelper(const char *helper, CCryFixedStringListT<5, 30> &attachments)
{
	//Do nothing...
	//Rocket launcher has an special helper for the scope, but it must be skipped by the HUD
}

//////////////////////////////////////////////////////////////////////////
void CJaw::AutoDrop()
{
	m_autoDropping = true;
	m_controllingRocket = false;
	m_autoDropPendingTimer = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	BaseClass::OnShoot(shooterId, ammoId, pAmmoType, pos, dir, vel);

	CActor *pOwnerActor = GetOwnerActor();
	if (pOwnerActor)
	{
		// Set the projectile to target the forced entity
		if (m_forcedTargetId > 0)
		{
			CProjectile *pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(ammoId);
			if (pProjectile)
			{
				pProjectile->SetDestination(m_forcedTargetId);
			}
		}

		if (pOwnerActor->IsClient() && m_laserGuider.IsLaserActivated())
		{
			SHUDEvent event(eHUDEvent_InfoSystemsEvent);
			event.AddData(SHUDEventData(1));
			event.AddData(SHUDEventData(2.f));
			event.AddData(SHUDEventData("@hud_guided"));
			CHUDEventDispatcher::CallEvent(event);
		}

		if (pOwnerActor->IsClient())
			m_fired = true;
	}

	SvActivateMissileCountermeasures(shooterId, pos, dir);
}

//////////////////////////////////////////////////////////////////////////
void CJaw::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CGameInputActionHandlers::TJawActionHandler& jawActionHandler = g_pGame->GetGameInputActionHandlers().GetCJawActionHandler();

	bool result = false;
	jawActionHandler.Dispatch(this, actorId, actionId, activationMode, value, result);
	if (!result)
		CWeapon::OnAction(actorId, actionId, activationMode, value);
}

//////////////////////////////////////////////////////////////////////////
bool CJaw::OutOfAmmo(bool allFireModes) const
{
	CActor* pOwner = GetOwnerActor();
	IInventory* pOwnerInventory = pOwner ? pOwner->GetInventory() : 0;
	if (!pOwnerInventory)
		return true;

	IEntityClass* pJawClass = GetEntity()->GetClass();
	IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();
	int jawUniqueId = pItemSystem->GetItemUniqueId(pJawClass->GetName());
	int currentNumJaws = pOwnerInventory->GetCountOfUniqueId(jawUniqueId);

	return currentNumJaws == 0;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::SvActivateMissileCountermeasures(EntityId shooterId, const Vec3 &pos, const Vec3 &dir)
{
	if(gEnv->bServer)
	{
		float fov = sinf(DEG2RAD(60.0f));

		CActorManager * pActorManager = CActorManager::GetActorManager();

		pActorManager->PrepareForIteration();

		const int kNumActors			= pActorManager->GetNumActorsIncludingLocalPlayer();
		const int kShooterTeam		= g_pGame->GetGameRules()->GetTeam(shooterId);
		const float fZOffset			= 1.5f;
		const float kPlayerRadius = 0.5f;

		IActorSystem * pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();

		for(int i = 0; i < kNumActors; i++)
		{
			SActorData actorData;
			pActorManager->GetNthActorData(i, actorData);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CJaw::StartFire()
{
	if (!CanFire())
		return;

	CActor* pOwner = GetOwnerActor();
#if (USE_DEDICATED_INPUT)
	if (!pOwner || (!pOwner->IsPlayer() || (gEnv->bMultiplayer && strcmp(pOwner->GetEntityClassName(), "DummyPlayer") == 0)))
#else
	if (!pOwner || !pOwner->IsPlayer())
#endif
	{
		BaseClass::StartFire();
		return;
	}

	if (m_controllingRocket)
		return;

	CPlayer *ownerPlayer = GetOwnerPlayer();
	if (ownerPlayer)
	{
		if(ownerPlayer->IsClient() && ownerPlayer->IsWeaponUnderWater())
		{
			return;
		}

		ownerPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_FORCEEXITSLIDE );
	}

	if (!m_firePending && m_zm && g_pGameCVars->cl_zoomToggle > 0)
	{
		m_zoomAlreadyToggled = (m_zm->IsZoomingIn() || m_zm->IsZoomed());
	}
	else
	{
		m_zoomAlreadyToggled = false;
	}

	if(!IsZoomingInOrOut())
	{
		if (m_zm)
			m_zm->ZoomIn();
	}

	m_fireTriggerDown = true;
	m_firePending = true;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::StartFire(const SProjectileLaunchParams& launchParams)
{
	StartFire();
	SetDestinationEntity(launchParams.trackingId);
}

//////////////////////////////////////////////////////////////////////////
void CJaw::StopFire()
{
	if (!m_firePending)
		AutoZoomOut();

	BaseClass::StopFire();

	m_fireTriggerDown = false;
}

//////////////////////////////////////////////////////////////////////////
void CJaw::SetDestinationEntity(EntityId targetId)
{
	m_forcedTargetId = targetId;
}

//////////////////////////////////////////////////////////////////////////
bool CJaw::CanFire() const
{
	bool bCanFire = false;

	bool ai = false;
	bool weaponLowered = false;
	if (CPlayer *pOwnerPlayer = GetOwnerPlayer())
	{
		ai = !pOwnerPlayer->IsPlayer();
		weaponLowered = pOwnerPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) || pOwnerPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeaponMP);
	}

	if (ai || !GetOwnerPlayer() || ((m_firedRockets == 0) && !weaponLowered))
	{
		bCanFire = BaseClass::CanFire();
	}
	
	return bCanFire;
}



bool CJaw::CanAutoDrop()
{
	if (m_autoDropPendingTimer < g_autoDropDelayTime)
		return false;

	IZoomMode* pZoomMode = GetZoomMode(GetCurrentZoomMode());
	if (!pZoomMode)
		return true;
	return !(pZoomMode->IsZoomed() || pZoomMode->IsZoomingInOrOut());
}



void CJaw::DoAutoDrop()
{
	if(!m_playedDropAction && m_fm)
	{
		m_firedRockets--;

		CActor* pOwner = GetOwnerActor();
		// no need to auto-drop for AI
		if(pOwner && !pOwner->IsPlayer())
			return;

		if((GetAmmoCount(m_fm->GetAmmoType())<=0)&&(m_firedRockets<=0))
		{
			if( pOwner )
			{
				uint8 uniqueId = m_pItemSystem->GetItemUniqueId(GetEntity()->GetClass()->GetName());
				bool selectNext = pOwner->GetInventory()->GetCountOfUniqueId(uniqueId) <= 1;

				PlayAction(GetFragmentIds().drop);
				m_playedDropAction = true;
			}
		}
	}
}



void CJaw::HideRocket()
{
	HideCharacterAttachment(eIGS_FirstPerson, "rocket", true);
	HideCharacterAttachment(eIGS_ThirdPerson, "rocket", true);
	m_auxSlotUsed = true;
	CActor* pOwnerActor = GetOwnerActor();
	if (pOwnerActor && pOwnerActor->IsThirdPerson())
	{
		DrawSlot(eIGS_ThirdPerson, false);
		DrawSlot(eIGS_ThirdPersonAux, true);
	}
}



bool CJaw::CanDeselect() const
{
	return !m_controllingRocket && (!m_autoDropping || m_zoomAlreadyToggled);
}



bool CJaw::OnActionJawZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	bool zooming = (activationMode == eAAM_OnPress || activationMode == eAAM_OnHold);

	if (m_zoomAlreadyToggled && activationMode == eAAM_OnPress)
	{
		m_zoomAlreadyToggled = false;
		AutoZoomOut();
	}

	if (!m_zoomTriggerDown && zooming && m_fired)
		return true;

	m_zoomTriggerDown = zooming;

	return false;
}




bool CJaw::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit)
{
	return false;
}



bool CJaw::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
	int slot = eIGS_ThirdPerson;
	CActor* pOwnerActor = GetOwnerActor();
	if (pOwnerActor && !pOwnerActor->IsThirdPerson())
		slot = eIGS_FirstPerson;

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter || !(pCharacter->IsCharacterVisible() && slot==eIGS_ThirdPerson))
		return false;

	Matrix34 fireHelperLocation = GetCharacterAttachmentWorldTM(slot, "muzzleflash_effect");
	pos = fireHelperLocation.TransformPoint(Vec3(ZERO));
	return true;
}



bool CJaw::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	return false;
}



bool CJaw::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	return false;
}



bool CJaw::GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir)
{
	return false;
}



void CJaw::WeaponReleased()
{
}


void CJaw::CreateSmokeEffect()
{
	const CFireMode* pFireMode = static_cast<CFireMode*>(m_fm);
	if (pFireMode)
	{
		const SFireModeParams* pFireModeParams = pFireMode->GetShared();
		AttachEffect(eIGS_ThirdPerson, pFireModeParams->muzzlesmoke.helperFromAccessory, pFireModeParams->muzzlesmoke.effect[1].c_str(), pFireModeParams->muzzlesmoke.helper[1].c_str());
	}
}



float CJaw::GetMovementModifier() const
{
	if (!m_controllingRocket)
		return GetPlayerMovementModifiers().movementSpeedScale;
	else
		return GetPlayerMovementModifiers().firingMovementSpeedScale;
}



float CJaw::GetRotationModifier(bool usingMouse) const
{
	const SPlayerMovementModifiers modifiers = GetPlayerMovementModifiers();

	float rotModifier = 1.f;

	if (!m_controllingRocket)
	{
		rotModifier = usingMouse ? modifiers.mouseRotationSpeedScale : modifiers.rotationSpeedScale;
	}
	else
	{
		rotModifier = usingMouse ? modifiers.mouseFiringRotationSpeedScale : modifiers.firingRotationSpeedScale;
	}

	return rotModifier;
}



void CJaw::UpdatePendingShot()
{
	if (m_firePending && CanFire() && IsZoomed())
	{
		m_controllingRocket = true;
		m_firePending = false;
		HideRocket();
		BaseClass::StartFire();
		AutoZoomOut();
	}
}

//////////////////////////////////////////////////////////////////////////
void CJaw::AutoZoomOut()
{
	if (!m_zoomTriggerDown && !m_zoomAlreadyToggled)
		ExitZoom(true);
}

//////////////////////////////////////////////////////////////////////////
ColorF CJaw::GetSilhouetteColor() const
{
	return (!m_dropped) ? ColorF(0.89411f, 0.89411f, 0.10588f, 1.0f) : ColorF(0.89411f, 0.10588f, 0.10588f, 1.0f);
}


void CJaw::AddAmmoCapacity()
{
	CActor* pOwner = GetOwnerActor();
	if (!pOwner || !(gEnv->bServer || pOwner->IsClient()))
		return;
	IInventory* pOwnerInventory = pOwner->GetInventory();
	if (!pOwnerInventory)
		return;
	if (pOwnerInventory->GetItemByClass(GetEntity()->GetClass()) != 0)
		return;
	const CWeaponSharedParams* pWeaponParams = GetWeaponSharedParams();

	for (size_t i = 0; i < pWeaponParams->ammoParams.capacityAmmo.size(); ++i)
	{
		IEntityClass* pAmmoType = pWeaponParams->ammoParams.capacityAmmo[i].pAmmoClass;
		int capacity = pWeaponParams->ammoParams.capacityAmmo[i].count;
		pOwnerInventory->SetAmmoCapacity(pAmmoType, capacity);
	}
}


void CJaw::DropAmmoCapacity()
{
	CActor* pOwner = GetOwnerActor();
	if (!pOwner || !(gEnv->bServer || pOwner->IsClient()))
		return;
	IInventory* pOwnerInventory = pOwner->GetInventory();
	if (!pOwnerInventory)
		return;
	if (pOwnerInventory->GetItemByClass(GetEntity()->GetClass()) != 0)
		return;
	const CWeaponSharedParams* pWeaponParams = GetWeaponSharedParams();

	for (size_t i = 0; i < pWeaponParams->ammoParams.capacityAmmo.size(); ++i)
	{
		IEntityClass* pAmmoType = pWeaponParams->ammoParams.capacityAmmo[i].pAmmoClass;
		int currentAmmo = pOwnerInventory->GetAmmoCount(pAmmoType);
		if (currentAmmo == 0)
			pOwnerInventory->SetAmmoCapacity(pAmmoType, 0);
	}
}



void CJaw::OnDropped( EntityId actorId, bool ownerWasAI)
{
	CItem::OnDropped(actorId, ownerWasAI);

	IEntity* pEntity = GetEntity();

	uint32 flags = pEntity->GetFlags();	
	flags &= ~ENTITY_FLAG_NO_PROXIMITY;
	pEntity->SetFlags(flags);

	m_expended_ammo = 0;

	m_laserGuider.TurnOffLaser();
}

bool CJaw::CanSelect() const
{
	return (BaseClass::CanSelect() && !OutOfAmmo(false));
}



bool CJaw::CanReload() const
{
	return false;
}


//--------------------------------------------
void CJaw::OnEnterFirstPerson()
{
	BaseClass::OnEnterFirstPerson();

	m_laserGuider.SetGeometrySlot(eIGS_FirstPerson);
}

//---------------------------------------------
void CJaw::OnEnterThirdPerson()
{
	BaseClass::OnEnterThirdPerson();

	if (m_laserGuider.IsLaserActivated())
	{
		m_laserGuider.SetGeometrySlot(eIGS_ThirdPerson);
	}
}

void CJaw::NetStartFire()
{
	//Handle server when used by clients
	HideRocket();
	m_fired = true;
	m_playedDropAction = true;

	BaseClass::NetStartFire();
}

void CJaw::NetShootEx( const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle, int fireModeId )
{
	//Handle clients when used by server
	HideRocket();
	m_fired = true;
	m_playedDropAction = true;

	BaseClass::NetShootEx(pos, dir, vel, hit, extra, predictionHandle, fireModeId);
}

void CJaw::NetSetCurrentAmmoCount( int count )
{
	//If the client has successfully fired a rocket at the point of host migration then the new server will
	//end up sending wrong information regarding their ammo, yet the rocket will remain fired
	if(!gEnv->bServer && m_firedRockets)
	{
		if (m_owner.GetId() == gEnv->pGameFramework->GetClientActorId())
		{
			const int currentCount = GetAmmoCount(m_fm->GetAmmoType());
			if(count == currentCount + m_firedRockets)
			{
				count = currentCount;

				SRequestAmmoParams params;
				params.m_ammo = currentCount;
				GetGameObject()->InvokeRMI(SvRequestAmmo(), params, eRMI_ToServer);
			}
		}
	}

	BaseClass::NetSetCurrentAmmoCount(count);
}

void CJaw::AnimationEvent( ICharacterInstance *pCharacter, const AnimEventInstance &event )
{
	if(event.m_EventNameLowercaseCRC32 == m_dropJAWAnimEvent)
	{
		if(CActor* pOwnerActor = GetOwnerActor())
		{
			const uint8 uniqueId = m_pItemSystem->GetItemUniqueId(GetEntity()->GetClass()->GetName());
			const bool selectNext = pOwnerActor->GetInventory()->GetCountOfUniqueId(uniqueId) <= 1;
			pOwnerActor->DropItem(GetEntityId(), 2.0f, selectNext, false);
		}
	}

	BaseClass::AnimationEvent(pCharacter, event);
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CJaw, SvRequestAmmo)
{
	if (m_fm)
	{
		SetAmmoCount(m_fm->GetAmmoType(), params.m_ammo);
	}
	
	return true;
}
