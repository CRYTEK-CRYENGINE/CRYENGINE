// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Mounted machine gun that can be ripped off by the player
and move around with it
-------------------------------------------------------------------------
History:
- 20:01:2009: Created by Benito G.R.
  30:09:2009: Ported from Rippable turret gun

*************************************************************************/

#include "StdAfx.h"
#include "HeavyMountedWeapon.h"
#include "GameActions.h"
#include "Game.h"
#include "GameInputActionHandlers.h"
#include "Player.h"
#include "ScreenEffects.h"
#include "ItemSharedParams.h"
#include "GameRules.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "Battlechatter.h"
#include "PersistantStats.h"
#include "RecordingSystem.h"
#include "FireMode.h"
#include "Melee.h"
#include "ItemAnimation.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace
{


	bool s_ripOffPromptIsVisible = false;


	void DoRipOffPrompt(EntityId who, bool doIt)
	{
		if (who == g_pGame->GetIGameFramework()->GetClientActorId() && doIt != s_ripOffPromptIsVisible)
		{
			const char* ripoffInteraction = "@ui_interaction_ripofforstopusing";
			if (doIt)
			{
				SHUDEventWrapper::InteractionRequest(true, ripoffInteraction, "special", "player", 1.0f);
			}
			else
			{
				SHUDEventWrapper::ClearInteractionMsg(eHIMT_INTERACTION, ripoffInteraction);
			}

			s_ripOffPromptIsVisible = doIt;
		}
	}


}


struct CHeavyMountedWeapon::EndRippingOff
{
	EndRippingOff(CHeavyMountedWeapon *_weapon): pHMGWeapon(_weapon){};

	CHeavyMountedWeapon *pHMGWeapon;

	void execute(CItem *_this)
	{
		pHMGWeapon->FinishRipOff();
	}
};



struct CHeavyMountedWeapon::RemoveViewLimitsAction
{
	RemoveViewLimitsAction(CHeavyMountedWeapon *_weapon): pHMGWeapon(_weapon){};

	CHeavyMountedWeapon *pHMGWeapon;

	void execute(CItem *_this)
	{
		CActor *pOwner = pHMGWeapon->GetOwnerActor();

		if (pOwner)
		{
			SActorParams &params = pOwner->GetActorParams();

			params.viewLimits.SetViewLimit(Vec3Constants<float>::fVec3_OneY, 0.0f, 0.01f, 0.f, 0.f, SViewLimitParams::eVLS_Item);
		}
	}
};



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CHeavyMountedWeapon::CHeavyMountedWeapon():
m_rippedOff(false),
m_rippingOff(false),
m_linkedParentId(0),
//m_rotatingSoundID(INVALID_SOUNDID),
m_RotationSoundTimeOut(0),
m_lastXAngle(0),
m_lastZAngle(0),
m_lastUsedFrame(-1)
{
	RegisterActionsHMG();
}

CHeavyMountedWeapon::~CHeavyMountedWeapon()
{
	if(m_stats.used)
	{
		m_stats.dropped = true; //prevent StopUse calling Drop
		StopUse(GetOwnerId());
	}
}


void CHeavyMountedWeapon::RegisterActionsHMG()
{
	CGameInputActionHandlers::THMGActionHandler& hmgActionHandler = g_pGame->GetGameInputActionHandlers().GetCHMGActionHandler();

	if (hmgActionHandler.GetNumHandlers() == 0)
	{
		const CGameActions& actions = g_pGame->Actions();

		hmgActionHandler.AddHandler(actions.special, &CHeavyMountedWeapon::OnActionRipOff);
		hmgActionHandler.AddHandler(actions.weapon_change_firemode, &CHeavyMountedWeapon::OnActionFiremode);
	}
}


void CHeavyMountedWeapon::OnReset()
{
	BaseClass::OnReset();

	m_rippedOff = m_rippingOff = false;
	m_stats.mounted = true;
	//m_rotatingSoundID = INVALID_SOUNDID;
	m_lastUsedFrame = -1;

	Physicalize(false, false);

	RequireUpdate(eIUS_General);

	if (m_linkedParentId != 0)
	{
		IEntity* pLinkedParent = gEnv->pEntitySystem->GetEntity(m_linkedParentId);
		if (pLinkedParent)
		{
			pLinkedParent->AttachChild(GetEntity());
		}
		m_linkedParentId = 0;
	}
}


void CHeavyMountedWeapon::UpdateFPView(float frameTime)
{
	BaseClass::UpdateFPView(frameTime);

	/*if(!gEnv->bMultiplayer)*/
	{
		if(!m_rippedOff && !m_rippingOff)
		{
			CActor* pOwner = GetOwnerActor();
			if(!pOwner)
				return;

			if (CanRipOff())
			{
				if(gEnv->bMultiplayer || !pOwner->GetLinkedVehicle())
				{
					DoRipOffPrompt(GetOwnerId(), true);
				}
			}
		}
	}
}


void CHeavyMountedWeapon::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_rippingOff)
		return;

	bool filtered = false;

	CGameInputActionHandlers::THMGActionHandler& hmgActionHandler = g_pGame->GetGameInputActionHandlers().GetCHMGActionHandler();

	bool handled = hmgActionHandler.Dispatch(this, actorId, actionId, activationMode, value, filtered);
	if(!handled || !filtered)
	{
		BaseClass::OnAction(actorId, actionId, activationMode, value);
	}
}


bool CHeavyMountedWeapon::OnActionRipOff(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	//TODO: This has to be synchronized for MP
	if((activationMode == eAAM_OnPress) && !m_rippedOff && !m_rippingOff)
	{
		if(CanRipOff())
		{
			TryRipOffGun();
			ClearInputFlag(eWeaponAction_Fire);
		}
		return true;
	}

	return false;
}


bool CHeavyMountedWeapon::OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	//DO NOTHING... (filters command, and doesn't allow to switch fire modes

	return true;
}


void CHeavyMountedWeapon::Use(EntityId userId)
{
	int frameID = gEnv->nMainFrameID;
	if (m_lastUsedFrame == frameID)
		return;

	m_lastUsedFrame = frameID;

	if (!m_owner.GetId())
	{
		StartUse(userId);
		HighlightWeapon(false);
	}
	else if (m_owner.GetId() == userId)
	{
		if (m_rippedOff || m_rippingOff)
		{
			FinishRipOff();
			DeselectAndDrop(userId);
		}
		else
		{
			StopUse(userId);
		}
	}
}



bool CHeavyMountedWeapon::CanPickUp(EntityId userId) const
{
	if (!m_rippedOff)
		return false;

	return BaseClass::CanPickUp(userId);
}



bool CHeavyMountedWeapon::CanUse(EntityId userId) const
{
	EntityId ownerId = m_owner.GetId();

	if (m_rippedOff)
	{
		CActor* pActor = GetActor(userId);
		if (pActor && pActor->IsSwimming())
			return false;

		if (ownerId == 0 || ownerId == userId)
			return true;
	}
	else if(IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(userId))
	{
		IItem* pItem = pActor->GetCurrentItem(false);
		if(pItem)
		{
			if(pItem->IsBusy())
			{
				return false;
			}
			if(IWeapon* pWeapon = pItem->GetIWeapon())
			{
				if(pWeapon->IsReloading())
				{
					return false;
				}
			}
		}
	}

	return BaseClass::CanUse(userId);
}



void CHeavyMountedWeapon::StartUse(EntityId userId)
{
	HighlightWeapon(false);

	if(m_rippedOff)
	{
		m_stats.dropped = false;
		BaseClass::StartUse(userId);
	}
	else
	{
		CWeapon::StartUse(userId);

		if (IsOwnerFP())
		{
			DrawSlot(eIGS_FirstPerson, true, true);
		}
	}

	RegisterAsUser();

	CActor* pOwner = GetOwnerActor();
	if (pOwner)
	{
		CHANGED_NETWORK_STATE(pOwner, CPlayer::ASPECT_CURRENT_ITEM);

		if (!m_rippedOff)
		{
			IPlayerInput* pPlayerInput = pOwner && pOwner->IsPlayer() ? static_cast<CPlayer*>(pOwner)->GetPlayerInput() : 0;
			if (pPlayerInput)
				pPlayerInput->ClearXIMovement();
		}
	}

	m_expended_ammo = 0;
}


void CHeavyMountedWeapon::StopUse(EntityId userId)
{
	UnRegisterAsUser();

	if (m_rippedOff || m_rippingOff)
	{
		CActor *pActor = GetOwnerActor();

		m_rippedOff = true;
		m_rippingOff = false;
		RemoveViewLimits();
		BaseClass::StopUse(userId);

		if(pActor)
		{
			pActor->LockInteractor(GetEntityId(), false);
		}
	}
	else
	{
		CActor *pActor = GetOwnerActor();
		if (!pActor)
		{
			return;
		}

		if (m_isFiring)
		{
			StopFire();
		}
		DoRipOffPrompt(GetOwnerId(), false);
		SetViewMode(eIVM_ThirdPerson);
		DrawSlot(eIGS_ThirdPerson, true);

		if(gEnv->bMultiplayer)
		{
			HighlightWeapon(true);
		}

		//The use of CWeapon::StopUse() here and not BaseClass::StopUse() is deliberate; it avoids the '::Drop()' call that CHeavyWeapon makes
		CWeapon::StopUse(userId);
	}
}


void CHeavyMountedWeapon::TryRipOffGun()
{
	CActor *pActor = GetOwnerActor();
	if(!pActor)
		return;

	PerformRipOff(pActor);
	
	if(gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(this, ASPECT_RIPOFF);
	}
	else
	{
		GetGameObject()->InvokeRMI(SvRequestRipOff(), EmptyParams(), eRMI_ToServer);
	}
}

void CHeavyMountedWeapon::PerformRipOff(CActor* pOwner)
{
	ExitZoom(true);

	UnlinkMountedGun();
	SetUnMountedConfiguration();	// This needs to come after the call to UnlinkMountedGun otherwise killcam doesn't work properly
	AttachToHand(true);
	StopFire();
	Physicalize(false, false);

	if (pOwner)
	{
		HandleHeavyWeaponPro(*pOwner);

		float speedOverride = 1.0f;
		if(pOwner->IsPlayer())
		{
			CPlayer* pOwnerPlayer = static_cast<CPlayer*>(pOwner);
			speedOverride = pOwnerPlayer->GetModifiableValues().GetValue(kPMV_HeavyWeaponRipOffSpeedOverride);
		}

		PlayAction(GetFragmentIds().rip_off, 0, false, eIPAF_Default, speedOverride);

		m_rippingOff = true;
		m_stats.dropped = false;

		DoRipOffPrompt(GetOwnerId(), false);

		int timeDelay = GetCurrentAnimationTime(eIGS_Owner);
		timeDelay = (timeDelay > 0) ? timeDelay : 2000;
		int removeViewLimitDelay = int(timeDelay * 0.65f);
		GetScheduler()->TimerAction(timeDelay, CSchedulerAction<EndRippingOff>::Create(EndRippingOff(this)), false);
		GetScheduler()->TimerAction(removeViewLimitDelay, CSchedulerAction<RemoveViewLimitsAction>::Create(RemoveViewLimitsAction(this)), false);

		if(!pOwner->IsThirdPerson() && !(m_stats.viewmode&eIVM_FirstPerson))
		{
			SetViewMode(eIVM_FirstPerson);
		}

		//Lock view in place during rip off
		SActorParams &params = pOwner->GetActorParams();

		Vec3 limitDir(ZERO);
		
		bool bUseMovementState = true;

		if (pOwner->IsClient() && (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating))
		{
			// If this happens during a host migration, our aim direction may not have made it into the movement
			// controller yet, get it from the saved migration params instead
			const CGameRules::SHostMigrationClientControlledParams *pHostMigrationParams = g_pGame->GetGameRules()->GetHostMigrationClientParams();
			if (pHostMigrationParams)
			{
				limitDir = pHostMigrationParams->m_aimDirection;
				bUseMovementState = false;
			}
		}

		if (bUseMovementState)
		{
			IMovementController *pMovementController = pOwner->GetMovementController();
			SMovementState state;
			pMovementController->GetMovementState(state);

			limitDir = state.aimDirection;
		}

		params.viewLimits.SetViewLimit(limitDir, 0.01f, 0.01f, 0.01f, 0.01f, SViewLimitParams::eVLS_Item);

		pOwner->SetSpeedMultipler(SActorParams::eSMR_Item, 0.0f);

		if(!gEnv->bMultiplayer)
			pOwner->LockInteractor(GetEntityId(), false);

	}

	TriggerRespawn();

	if (pOwner)
	{
		if (CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem()) 
		{
			pRecordingSystem->OnWeaponRippedOff(this);
		}

		BATTLECHATTER(BC_Ripoff, GetOwnerId());

		if(pOwner->IsClient())
		{
			g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_RipOffMountedWeapon);
			ClearInputFlag(eWeaponAction_Zoom);
		}
	}
	else
	{
		//--- If ripped off without an actor we should finish instantly
		m_rippingOff = false;
		m_rippedOff = true;
	}
}



void CHeavyMountedWeapon::UnlinkMountedGun()
{
	CActor* pActor = GetOwnerActor();
	if (pActor)
	{
		pActor->LinkToMountedWeapon(0);
		if (GetEntity()->GetParent())
		{
			m_linkedParentId = GetEntity()->GetParent()->GetId();
			GetEntity()->DetachThis();
		}
	}
	m_stats.mounted = false;
}



void CHeavyMountedWeapon::GetAngleLimits(EStance stance, float& minAngle, float& maxAngle)
{
	if (!m_rippedOff)
		return;

	const float minAngleStandValue = -50.0f;
	const float minAngleCrouchValue = -20.0f;
	const float maxAngleValue = 70.0f;

	maxAngle = maxAngleValue;
	minAngle = stance==STANCE_CROUCH ? minAngleCrouchValue : minAngleStandValue;
}



bool CHeavyMountedWeapon::UpdateAimAnims( SParams_WeaponFPAiming &aimAnimParams)
{
	if (!m_rippedOff && !m_rippingOff)
	{
		IFireMode* pFireMode = GetFireMode(GetCurrentFireMode());
		aimAnimParams.shoulderLookParams = 
			pFireMode ?
			&static_cast<CFireMode*>(pFireMode)->GetShared()->aimLookParams :
		&m_sharedparams->params.aimLookParams;

		return true;
	}

	return BaseClass::UpdateAimAnims(aimAnimParams);
}


void CHeavyMountedWeapon::Update( SEntityUpdateContext& ctx, int slot )
{
	BaseClass::Update(ctx, slot);
	
	/*if (m_rotatingSoundID!=INVALID_SOUNDID)
	{
		if (m_RotationSoundTimeOut>0)
		{
			m_RotationSoundTimeOut -= ctx.fFrameTime;
			RequireUpdate( eIUS_General );
		}
		else
		{
			StopSound(m_rotatingSoundID);
			m_rotatingSoundID = INVALID_SOUNDID;
		}
	}*/

	// Helper for editor placing
	if (gEnv->IsEditing())
	{
		// If host id is not 0, it means it is mounted to a vehicle, so don't render the helper in that case
		if (!GetHostId())
		{
			IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

			const Matrix34& weaponTM = GetEntity()->GetWorldTM();
			const Vec3 point1 = weaponTM.GetTranslation();
			const Vec3 point2 = point1 - (m_sharedparams->pMountParams->ground_distance * weaponTM.GetColumn2());
			const Vec3 point3 = point2 - (m_sharedparams->pMountParams->body_distance * weaponTM.GetColumn1());

			pRenderAux->DrawLine(point1, ColorB(0, 192, 0), point2, ColorB(0, 192, 0), 3.0f);
			pRenderAux->DrawLine(point2, ColorB(0, 192, 0), point3, ColorB(0, 192, 0), 3.0f);
			pRenderAux->DrawSphere(point3, 0.15f, ColorB(192, 0, 0));

			RequireUpdate(eIUS_General);
		}
	}
}

void CHeavyMountedWeapon::OnFireWhenOutOfAmmo()
{
	BaseClass::OnFireWhenOutOfAmmo();

	if (!IsReloading())
	{
		s_ripOffPromptIsVisible = false;
	}
}

void CHeavyMountedWeapon::OnOutOfAmmo(IEntityClass* pAmmoType)
{
	BaseClass::OnOutOfAmmo(pAmmoType);
	
	if(gEnv->bMultiplayer)
	{
		CActor * pActor = GetOwnerActor();
		if(pActor)
		{
			pActor->UseItem(GetEntityId());
		}
	}
}

void CHeavyMountedWeapon::UpdateIKMounted( IActor* pActor, const Vec3& vGunXAxis )
{

}

void CHeavyMountedWeapon::SwitchToRippedOffFireMode()
{
	if(GetCurrentFireMode() != 1)
	{
		RequestFireMode(1);
	}
}

void CHeavyMountedWeapon::SetUnMountedConfiguration()
{
	SwitchToRippedOffFireMode();

	IFireMode * pMountedFireMode = GetFireMode(0);
	assert(pMountedFireMode);
	
	pMountedFireMode->Enable(false);

	ExitZoom(true);

	//Second zoom mode is supposed to be unmounted
	if(GetZoomMode(1))
	{
		EnableZoomMode(1, true);
		SetCurrentZoomMode(1);
	}

	//Just in case, it was not clear properly
	CActor* pOwner = GetOwnerActor();
	if ((pOwner != NULL) && pOwner->IsClient())
	{
		float defaultFov = 55.0f;
		gEnv->pRenderer->EF_Query(EFQ_SetDrawNearFov,defaultFov);
	}
}


void CHeavyMountedWeapon::ProcessEvent(const SEntityEvent& event)
{
	if ((event.event == ENTITY_EVENT_XFORM) && IsMounted() && GetOwnerId())
	{
		const float Z_EPSILON = 0.01f;
		const Ang3& worldAngles = GetEntity()->GetWorldAngles();
		float xAngle = worldAngles.x;
		float zAngle = worldAngles.z;
		bool xAnglesAreEquivalent = (fabs(xAngle-m_lastXAngle)<Z_EPSILON);
		bool zAnglesAreEquivalent = (fabs(zAngle-m_lastZAngle)<Z_EPSILON);
		if (!xAnglesAreEquivalent || !zAnglesAreEquivalent)
		{
			/*if (m_rotatingSoundID==INVALID_SOUNDID)
				m_rotatingSoundID = PlayRotationSound();*/
			m_RotationSoundTimeOut = 0.15f;
			RequireUpdate( eIUS_General );
			m_lastXAngle = xAngle;
			m_lastZAngle = zAngle;
		}
			
		int flags = (int)event.nParam[0];
		if ((flags & ENTITY_XFORM_FROM_PARENT) && !(flags & ENTITY_XFORM_USER))
		{
			if (CActor* pOwnerActor = GetOwnerActor())
			{
				pOwnerActor->UpdateMountedGunController(true);
			}
		}
	}

	BaseClass::ProcessEvent(event);
}

void CHeavyMountedWeapon::Select(bool select)
{
	if (select == IsSelected())
		return;
	BaseClass::Select(select);
	if (select && m_rippedOff)
		SetUnMountedConfiguration();
}

void CHeavyMountedWeapon::FadeCrosshair( float to, float time, float delay)
{
	if (IsMounted())
	{
		BaseClass::FadeCrosshair(to, time, delay);
	}
	else
	{
		BaseClass::FadeCrosshair(1.0f, time, delay);
	}
}

void CHeavyMountedWeapon::FullSerialize( TSerialize ser )
{
	BaseClass::FullSerialize(ser);

	ser.Value("linkedParentId", m_linkedParentId);
	ser.Value("rippedOff", m_rippedOff);
	ser.Value("rippingOff", m_rippingOff);
	ser.Value("lastZAngle", m_lastZAngle);
}

void CHeavyMountedWeapon::PostSerialize()
{
	BaseClass::PostSerialize();

	if(m_rippingOff)
	{
		m_rippingOff = false;
		m_rippedOff = true;
	}

	StartUse(GetOwnerId());
}

bool CHeavyMountedWeapon::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (!BaseClass::NetSerialize(ser, aspect, profile, flags))
		return false;

	if(aspect == ASPECT_RIPOFF)
	{
		ser.Value("ripOff", static_cast<CHeavyMountedWeapon*>(this), &CHeavyMountedWeapon::IsRippingOrRippedOff, &CHeavyMountedWeapon::SetRippingOff, 'bool');
	}

	return true;
}

NetworkAspectType CHeavyMountedWeapon::GetNetSerializeAspects()
{
	return BaseClass::GetNetSerializeAspects() | ASPECT_RIPOFF;
}

void CHeavyMountedWeapon::InitClient(int channelId)
{
	CWeapon::InitClient(channelId); //Avoid calling CHeavyWeapon::InitClient as mounted weapons need special case logic to handle late joiners (Based on the ripoff state)

	if(m_rippingOff || m_rippedOff)
	{
		IActor *pActor = GetOwnerActor();
		if(pActor)
		{
			EntityId ownerID = pActor->GetEntity()->GetId();
			GetGameObject()->InvokeRMIWithDependentObject(ClRipOff(), BaseClass::SHeavyWeaponUserParams(ownerID), eRMI_ToClientChannel, ownerID, channelId);	
		}
		else
		{
			GetGameObject()->InvokeRMI(ClDropped(), EmptyParams(), eRMI_ToClientChannel, channelId);
		}
	}
	
	if(m_bIsHighlighted && !m_rippingOff)
	{
		GetGameObject()->InvokeRMI(ClHeavyWeaponHighlighted(), SNoParams(), eRMI_ToClientChannel, channelId);		 
	}
}


void CHeavyMountedWeapon::PostInit( IGameObject * pGameObject )
{
	BaseClass::PostInit(pGameObject);

	if(gEnv->bMultiplayer && !gEnv->IsDedicated())
	{
		if(g_pGame->GetIGameFramework()->GetClientActor() != NULL)
		{
			HighlightWeapon(true);
		}
		else
		{
			g_pGame->GetGameRules()->RegisterClientConnectionListener(this); //We unregister again once we received the event (in CHeavyWeapon::OnOwnClientEnteredGame)
		}
	}
}


void CHeavyMountedWeapon::SetRippingOff(bool ripOff)
{
	if(ripOff && !m_rippingOff && !m_rippedOff)
	{
		PerformRipOff(GetOwnerActor());
	}
}



void CHeavyMountedWeapon::FinishRipOff()
{
	m_rippingOff = false;
	m_rippedOff = true;
	
	RemoveViewLimits();

	if(IsClient() && gEnv->pGameFramework->GetClientActorId()==GetOwnerId())
	{
		if(IEntity* pEntity = GetEntity())
		{
			const char* collectibleId = pEntity->GetClass()->GetName();
			CPersistantStats* pStats = g_pGame->GetPersistantStats();
			if(pStats && pStats->GetStat(collectibleId, EMPS_SPWeaponByName) == 0)
			{
				pStats->SetMapStat(EMPS_SPWeaponByName, collectibleId, eDatabaseStatValueFlag_Available);

				if(!gEnv->bMultiplayer)
				{
					// Show hud unlock msg
					SHUDEventWrapper::DisplayWeaponUnlockMsg(collectibleId);
				}
			}
		}
	}

	CActor* pOwner = GetOwnerActor();
	IActionController* pController = pOwner ? pOwner->GetAnimatedCharacter()->GetActionController() : NULL;
	if(pController)
	{
		CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();
		const SMannequinItemParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(pController);

		UpdateMountedTags(pParams, pController->GetContext().state, true);
	}
}



void CHeavyMountedWeapon::RemoveViewLimits()
{
	ApplyViewLimit(GetOwnerId(), false);
}


void CHeavyMountedWeapon::OnBeginCutScene()
{
	if (m_rippedOff)
	{
		Hide(true);
	}
}

void CHeavyMountedWeapon::OnEndCutScene()
{
	if (m_rippedOff)
	{
		Hide(false);

		PlayAction(GetSelectAction());
	}
}



IMPLEMENT_RMI(CHeavyMountedWeapon, SvRequestRipOff)
{
	CHECK_OWNER_REQUEST();

	if (!m_rippingOff && !m_rippedOff)
	{
		PerformRipOff(GetOwnerActor());

		CHANGED_NETWORK_STATE(this, ASPECT_RIPOFF);
	}

	return true;
}

IMPLEMENT_RMI(CHeavyMountedWeapon, ClRipOff)
{
	IActor *pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(params.ownerId);
	if(pActor && (!m_rippingOff || m_rippedOff))
	{
		SetUnMountedConfiguration();
		UnlinkMountedGun();
		FinishRipOff();
		StartUse(params.ownerId);
	}

	return true;
}

IMPLEMENT_RMI(CHeavyMountedWeapon, ClDropped)
{
	if(!m_rippedOff)
	{
		SetUnMountedConfiguration();
		UnlinkMountedGun();
		FinishRipOff();
		EnableUpdate(false);
		BaseClass::Drop(5.0f);
	}

	return true;
}



bool CHeavyMountedWeapon::CanDeselect() const
{
	return !IsRippingOff();
}



bool CHeavyMountedWeapon::CanRipOff() const
{
	return GetSharedItemParams()->params.can_rip_off;
}



void CHeavyMountedWeapon::ForceRippingOff( bool ripOff )
{
	SetRippingOff(ripOff);

	if (ripOff)
	{
		// If we're forcing to ripped-off, make sure we're using the right firemode (firemode is server
		// controlled so may have lost during a host migration)
		SwitchToRippedOffFireMode();
	}

	if (gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(this, ASPECT_RIPOFF);
	}
}

void CHeavyMountedWeapon::ReadProperties(IScriptTable *pProperties)
{
	BaseClass::ReadProperties(pProperties);

	if (gEnv->bMultiplayer && pProperties)
	{
		ReadMountedProperties(pProperties);
	}
}

//tSoundID CHeavyMountedWeapon::PlayRotationSound()
//{
//	tSoundID result = 0;
//	IEntityAudioComponent* pIEntityAudioComponent = GetAudioProxy(true);
//	if (pIEntityAudioComponent)
//	{
//		const ItemString& soundName = m_stats.fp ? m_sharedparams->pMountParams->rotate_sound_fp : m_sharedparams->pMountParams->rotate_sound_tp;
//		result = pIEntityAudioComponent->PlaySoundEx(soundName.c_str(), ZERO, FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, 1.0f, 0, 0, eSoundSemantic_Weapon);
//	}
//	return result;
//}

bool CHeavyMountedWeapon::AllowInteraction( EntityId interactionEntity, EInteractionType interactionType )
{
	if(interactionType==eInteraction_GameRulesPickup && (IsRippingOff()||!IsRippedOff()))
	{
		return false;
	}
	return BaseClass::AllowInteraction(interactionEntity, interactionType);
}
