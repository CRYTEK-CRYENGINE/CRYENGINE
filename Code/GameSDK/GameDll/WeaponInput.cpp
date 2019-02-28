// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$   All input weapon stuff here

-------------------------------------------------------------------------
History:
- 30.07.07   12:50 : Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "Weapon.h"
#include "GameActions.h"
#include "Game.h"
#include "GameInputActionHandlers.h"
#include "GameCVars.h"
#include "FireMode.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "IPlayerInput.h"
#include "Player.h"
#include "flashlight.h"
#include "PlayerPlugin_Interaction.h"

 
//===========AUX FUNCTIONS====================
namespace
{
	bool CheckPickupInteraction(CWeapon* pThis)
	{
		CActor* pActor = pThis->GetOwnerActor();
		if(!pActor || !pActor->IsClient())
			return false;

		CPlayer* pClientPlayer = static_cast<CPlayer*>(pActor);
		EntityId interactiveEntityId = pClientPlayer->GetCurrentInteractionInfo().interactiveEntityId;

		bool pickupInteration = (interactiveEntityId != INVALID_ENTITYID);

		return pickupInteration;
	}
}

//=================================================================

void CWeapon::RegisterActions()
{
	CGameInputActionHandlers::TWeaponActionHandler& weaponActionHandler = g_pGame->GetGameInputActionHandlers().GetCWeaponActionHandler();

	if (weaponActionHandler.GetNumHandlers() == 0)
	{
		#define ADD_HANDLER(action, func) weaponActionHandler.AddHandler(actions.action, &CWeapon::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(attack1, OnActionAttackPrimary);
		ADD_HANDLER(attack1_xi, OnActionAttackPrimary);
		ADD_HANDLER(attack2_xi,OnActionAttackSecondary);
		ADD_HANDLER(reload,OnActionReload);
		ADD_HANDLER(special,OnActionSpecial);
		ADD_HANDLER(modify,OnActionModify);
		ADD_HANDLER(weapon_change_firemode,OnActionFiremode);
		ADD_HANDLER(zoom_toggle,OnActionZoomToggle);
		ADD_HANDLER(zoom_in,OnActionZoomIn);
		ADD_HANDLER(zoom_out,OnActionZoomOut);
		ADD_HANDLER(zoom,OnActionZoom);
		ADD_HANDLER(xi_zoom,OnActionZoom);
		ADD_HANDLER(sprint,OnActionSprint);
		ADD_HANDLER(sprint_xi,OnActionSprint);
		//ADD_HANDLER(xi_zoom,OnActionZoomXI);
		ADD_HANDLER(stabilize,OnActionStabilize);
		ADD_HANDLER(toggle_flashlight,OnActionToggleFlashLight);

		#undef ADD_HANDLER
	}
}
//-----------------------------------------------------
void CWeapon::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CItem::OnAction(actorId, actionId, activationMode, value);

	CGameInputActionHandlers::TWeaponActionHandler& weaponActionHandler = g_pGame->GetGameInputActionHandlers().GetCWeaponActionHandler();

	weaponActionHandler.Dispatch(this,actorId,actionId,activationMode,value);
}

//------------------------------------------------------
void CWeapon::SetInputFlag(uint8 actionFlag) 
{ 
	CRY_ASSERT(GetOwnerActor() && ( GetOwnerActor()->IsClient() || !GetOwnerActor()->IsPlayer() ) );

	if (!s_lockActionRequests) 
	{
		s_requestedActions |= actionFlag;
	} 
}

//------------------------------------------------------
void CWeapon::ForcePendingActions(uint8 blockedActions)
{
	CItem::ForcePendingActions(blockedActions);

	CActor* pOwner = GetOwnerActor();
	if(!pOwner || !pOwner->IsClient() || s_lockActionRequests)
		return;

	//--- Lock action requests to ensure that we don't recurse back into this function but also
	//--- block any recursive action requests to ensure we don't repeat the actions next frame
	s_lockActionRequests = true;

	//Force start firing, if needed and possible
	if(IsInputFlagSet(eWeaponAction_Fire) && !(blockedActions&eWeaponAction_Fire))
	{
		ClearInputFlag(eWeaponAction_Fire);
		if(IsTargetOn() || (m_fm && !m_fm->AllowZoom()))
		{
			s_lockActionRequests = false;
			return;
		}
			
		OnAction(GetOwnerId(),CCryName("attack1"),eAAM_OnHold,0.0f);
	}

	//Force zoom in if needed
	if(IsInputFlagSet(eWeaponAction_Zoom) && !(blockedActions&eWeaponAction_Zoom))
	{
		if(!IsZoomed() && !IsZoomingInOrOut())
			OnActionZoom(GetOwnerId(), CCryName("zoom"), eAAM_OnPress, 0.0f);

		ClearInputFlag(eWeaponAction_Zoom);
	}

	if(IsInputFlagSet(eWeaponAction_Reload) && !(blockedActions&eWeaponAction_Reload))
	{
		if (!IsBusy())
			Reload();
		ClearInputFlag(eWeaponAction_Reload);
	}

	s_lockActionRequests = false;
}

//--------------------------------------------------------------------
bool CWeapon::PreActionAttack(bool startFire)
{
	if(startFire)
	{
		SetInputFlag(eWeaponAction_Fire);
	}
	else if(!startFire)
	{
		ClearInputFlag(eWeaponAction_Fire);
	}

	if(IsModifying())
		return true;

	if(startFire)
	{
		return CheckSprint();
	}

	return false;
}

bool CWeapon::CheckSprint()
{
	CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	if(!pPlayer || pPlayer->IsSliding())
		return false;

	SPlayerStats *pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
	assert(pStats);

	if(pPlayer->IsSprinting())
	{
		pStats->bIgnoreSprinting = true;

		float delayScale = pPlayer->GetModifiableValues().GetValue(kPMV_WeaponFireFromSprintTimeScale);
		m_delayedFireActionTimeOut = GetParams().sprintToFireDelay * delayScale;

		return true;
	}
	else if (m_delayedFireActionTimeOut > 0.f)
	{
		return true;
	}

	return false;
}

//--------------------------------------------------------------------
bool CWeapon::PreMeleeAttack()
{
	CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	if(!pPlayer)
		return false;

	SPlayerStats* pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
	assert(pStats);

	if (!pPlayer->IsSprinting())
		return true;

	pStats->bIgnoreSprinting = true;
	float delayScale = pPlayer->GetModifiableValues().GetValue(kPMV_WeaponFireFromSprintTimeScale);
	m_delayedMeleeActionTimeOut = GetParams().sprintToMeleeDelay * delayScale;

	return (m_delayedMeleeActionTimeOut <= 0.f);
}

bool CWeapon::CanStabilize() const
{
	CPlayer* pPlayer = GetOwnerPlayer();

	return (pPlayer != NULL);
}

//--------------------------------------------------------------------
bool CWeapon::OnActionSprint(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	//If zoomed, you can not sprint (in SP only)
	if ((gEnv->bMultiplayer == false) && IsZoomed())
		return false;

	CActor* pOwnerActor = GetOwnerActor();
	CPlayer *pPlayer = 0;
	SPlayerStats *pStats = 0;
	if (pOwnerActor && pOwnerActor->IsPlayer())
	{
		pPlayer = static_cast<CPlayer*>(pOwnerActor);
		pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
	}

	if (activationMode == eAAM_OnPress)
	{
		if (pPlayer)
		{
			if (pStats && pStats->bIgnoreSprinting && !m_isFiring && m_delayedFireActionTimeOut <= 0.f && m_delayedMeleeActionTimeOut <= 0.f)
				pStats->bIgnoreSprinting = false;
		}
	}

	return false;
}

//---------------------------------------------------------
//Controller input (primary fire)
bool CWeapon::OnActionAttackPrimary(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress && (!gEnv->bMultiplayer || m_weaponNextShotTimer <= 0.f))
	{
		if(PreActionAttack(true))
			return true;

		StartFire();
	}
	else if(activationMode == eAAM_OnHold)
	{
		if((!gEnv->bMultiplayer || m_weaponNextShotTimer <= 0.f) && m_fm && m_fm->CanFire() && !m_fm->IsFiring() && !IsDeselecting() && !CheckSprint())
		{
			StartFire();
		}		
	}
	else
	{
		PreActionAttack(false);
		StopFire();
	}

	return true;
}
//---------------------------------------------------------
//Controller input (secondary fire)
bool CWeapon::OnActionAttackSecondary(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CActor *pOwner = GetOwnerActor();
	
	// Iron sight disabled
	if (pOwner && !pOwner->CanUseIronSights())
	{
		activationMode = eAAM_OnRelease;
	}
	OnActionZoom(actorId, actionId, activationMode, value);

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionReload(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	bool playerCanStartReload = false;
	
	if (activationMode == eAAM_OnPress && (CheckPickupInteraction(this) == false))
		playerCanStartReload = true;
	// this condition results from the interaction between reload and item pickups. If on front on an item and press reload/pick button but not for long enought,
	// then the player should still reload the weapon instead of picking up the item.
	else if(activationMode == eAAM_OnRelease && CheckPickupInteraction(this) == true)
		playerCanStartReload = true;

	float currentTime = gEnv->pTimer->GetCurrTime();
	if (!playerCanStartReload && activationMode == eAAM_OnPress)
		m_reloadButtonTimeStamp = currentTime;
	else if (activationMode == eAAM_OnRelease && playerCanStartReload && currentTime > m_reloadButtonTimeStamp+g_pGameCVars->pl_useItemHoldTime)
		playerCanStartReload = false;
	
	if (!playerCanStartReload)
		return true;

	if (!IsBusy() && !AreAnyItemFlagsSet(eIF_Modifying))
	{
		Reload();
	}
	else if (!IsReloading() && activationMode == eAAM_OnPress)
	{
		SetInputFlag(eWeaponAction_Reload);
	}

	return true;
}

//---------------------------------------------------------------------------------
bool CWeapon::OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CActor* pOwner = GetOwnerActor();
	CPlayer* pPlayer = pOwner && pOwner->IsPlayer() ? static_cast<CPlayer*>(pOwner) : 0;
	if (pPlayer && (pPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) || pPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeaponMP)) )
		return true;

	bool incompatibleZommMode = (m_secondaryZmId != 0 && (IsZoomed() || IsZoomingInOrOut()));

	if (AreAnyItemFlagsSet(eIF_BlockActions) || incompatibleZommMode)
	{
		return true;
	}

	if (activationMode == eAAM_OnPress)
	{
		IFireMode* pNewFiremode = GetFireMode(GetNextFireMode(GetCurrentFireMode()));
		if (pNewFiremode == m_fm)
		{
			if(pPlayer && pPlayer->CanSwitchItems())
			{
				pPlayer->SwitchToWeaponWithAccessoryFireMode();
			}
		}
		else if (!IsReloading())
		{
			StartChangeFireMode();
		}
	}

	return true;
}

//---------------------------------------------------------------------
bool CWeapon::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CPlayer* pOwnerPlayer = GetOwnerPlayer();
	if (pOwnerPlayer && !pOwnerPlayer->CanMelee())
		return true;

	if(gEnv->bMultiplayer && AreAnyItemFlagsSet(eIF_Transitioning)) //Ignore the transition from attachments menu and melee immediately
	{
		ClearItemFlags( eIF_Transitioning );
	}

	if (gEnv->bMultiplayer && !g_pGameCVars->pl_boostedMelee_allowInMP)
	{
		if (activationMode == eAAM_OnPress)
		{
			if (CanMeleeAttack())
			{
				if (PreMeleeAttack())
					MeleeAttack();
			}
			else
			{
				CCCPOINT(Melee_PressedButMeleeNotAllowed);
			}
		}
	}
	else
	{
		if (activationMode == eAAM_OnPress)
		{
			if(CanMeleeAttack())
			{
				if (pOwnerPlayer)
				{
					if (PreMeleeAttack())
					{
						BoostMelee(false);
						MeleeAttack();
					}
				}
			}
			else
			{
				CCCPOINT(Melee_PressedButMeleeNotAllowed);
			}
		}
	}


	return true;
}

//------------------------------------------------------------------------
class CWeapon::ScheduleLayer_Leave
{
public:
	ScheduleLayer_Leave(CWeapon *wep)
	{
		_pWeapon = wep;
	}
	void execute(CItem *item) {
		_pWeapon->ClearItemFlags(eIF_Transitioning);
		if(!_pWeapon->AreAnyItemFlagsSet(eIF_Modifying)) //This scheduled event may now be out of date - if we are still modifying the weapon then leave the DOF alone
		{
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);
			_pWeapon->s_dofSpeed=0.0f;
			if (_pWeapon->m_zm)
				static_cast<CIronSight*>(_pWeapon->m_zm)->ClearDoF();
		}
	}
private:
	CWeapon *_pWeapon;
};

class CWeapon::ScheduleLayer_Enter
{
public:
	ScheduleLayer_Enter(CWeapon *wep)
	{
		_pWeapon = wep;
	}
	void execute(CItem *item) {
		_pWeapon->ClearItemFlags(eIF_Transitioning);
		if(_pWeapon->AreAnyItemFlagsSet(eIF_Modifying)) //This scheduled event may now be out of date - if we are not modifying the weapon then leave the DOF alone
		{
			gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 1.0f);
			_pWeapon->s_dofSpeed=0.0f;
		}
	}
private:
	CWeapon *_pWeapon;
};

//-------------------------------------------------------------------------
bool CWeapon::OnActionModify(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (IsZoomed() || IsZoomingInOrOut())
		return false;

	if (CanModify() && ((!IsReloading() && !IsBusy()) || AreAnyItemFlagsSet(eIF_Modifying)))
	{
		if (m_fm)
			m_fm->StopFire();

		if (AreAnyItemFlagsSet(eIF_Modifying))
		{
			m_enterModifyAction = 0;
			PlayAction(GetFragmentIds().leave_modify, 0);
			s_dofSpeed = __fres(-g_pGameCVars->i_weapon_customisation_transition_time);
			s_dofValue = 1.0f;
			s_focusValue = -1.0f;

			GetScheduler()->TimerAction(g_pGameCVars->i_weapon_customisation_transition_time, CSchedulerAction<ScheduleLayer_Leave>::Create(this), false);

			SetItemFlags( eIF_Transitioning );
			ClearItemFlags( eIF_Modifying );

			GetGameObject()->InvokeRMI(CItem::SvRequestLeaveModify(), CItem::EmptyParams(), eRMI_ToServer);
		}
		else
		{
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_UseMask", 0.f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", -1.f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMin", 0.5f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMax", 1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", 1.5f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZ", 0.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZScale", 0.0f);

			m_itemLowerMode = eILM_Raised;

			TagState tagState = TAG_STATE_EMPTY;
			m_enterModifyAction = new CItemAction(PP_PlayerAction, GetFragmentIds().enter_modify, tagState);
			PlayFragment(m_enterModifyAction);
			s_dofSpeed = __fres(g_pGameCVars->i_weapon_customisation_transition_time);
			s_dofValue = 0.0f;

			SetItemFlags(eIF_Transitioning);

			GetScheduler()->TimerAction(g_pGameCVars->i_weapon_customisation_transition_time, CSchedulerAction<ScheduleLayer_Enter>::Create(this), false);
			
			SetItemFlags(eIF_Modifying);

			CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
			if (pPlayer)
			{
				SPlayerStats *pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
				assert(pStats);
				pStats->bIgnoreSprinting = true;
			}

			GetGameObject()->InvokeRMI(CItem::SvRequestEnterModify(), CItem::EmptyParams(), eRMI_ToServer);
		}
	}

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionZoomToggle(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed() && m_zm->GetZoomState() != eZS_ZoomingOut)
	{
		int numSteps = m_zm->GetMaxZoomSteps();
		if ((numSteps>1) && (m_zm->GetCurrentStep()<numSteps))
			m_zm->ZoomIn();
		else if (numSteps>1)
			m_zm->ZoomOut();
	}

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionZoomIn(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed())
	{
		int numSteps = m_zm->GetMaxZoomSteps();
		if((numSteps>1) && (m_zm->GetCurrentStep()<numSteps))
			m_zm->ZoomIn();
	}

	return true;
}

//----------------------------------------------------------
bool CWeapon::OnActionZoomOut(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed())
	{
		int numSteps = m_zm->GetMaxZoomSteps();
		if((numSteps>1) && (m_zm->GetCurrentStep()>1))
			m_zm->ZoomOut();
	}

	return true;
}

//----------------------------------------------------------
bool CWeapon::CanZoomInState(float fallingMinAirTime) const
{
	if (const CPlayer* pPlayer = GetOwnerPlayer())
	{
		if (gEnv->bMultiplayer)
		{
			const bool allowedWhileJumpingFalling = g_pGameCVars->i_ironsight_while_jumping_mp || (!pPlayer->IsJumping() && (g_pGameCVars->i_ironsight_while_falling_mp || !pPlayer->IsInAir() || (pPlayer->CPlayer::GetActorStats()->inAir < fallingMinAirTime)));
			return allowedWhileJumpingFalling && !pPlayer->IsSliding();
		}
		else
			return (GetLowerMode() != eILM_Cinematic) && !pPlayer->IsSliding();
	}

	return true;
}

bool CWeapon::OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (IsModifying())
	{
		return false;
	}

	const bool zoomingIn = IsZoomingIn();
	const bool zoomed = IsZoomed();

	const bool toggleMode = (g_pGameCVars->cl_zoomToggle > 0);
	const bool toggleZoomOn = !(zoomingIn || zoomed);

	const bool buttonPressed = (activationMode == eAAM_OnPress);

	if (!toggleMode)
	{
		// Toggle mode is off, i.e. You have to hold to stay zoomed.
		if(buttonPressed || activationMode == eAAM_OnHold)
		{
			SetInputFlag(eWeaponAction_Zoom);
		}
		else
		{
			ClearInputFlag(eWeaponAction_Zoom);
		}
	}
	else
	{
		// Toggle mode is on.
		if(buttonPressed)
		{
			if (toggleZoomOn)
			{
				SetInputFlag(eWeaponAction_Zoom);
			}
			else
			{
				ClearInputFlag(eWeaponAction_Zoom);
			}
		}
	}
	
	//can ironsight while super jumping in MP
	const bool trumpZoom = !CanZoomInState();

	if (!AreAnyItemFlagsSet(eIF_Modifying) && !trumpZoom)	
	{
		if (!m_fm || !m_fm->IsReloading())
		{
			if (buttonPressed && (!toggleMode || toggleZoomOn))
			{
				if(m_fm && !m_fm->AllowZoom())
				{
					if(IsTargetOn())
					{
						m_fm->Cancel();
					}
					else
					{
						return false;
					}
				}
			}
			else if ((!toggleMode && activationMode == eAAM_OnRelease) || // Not toggle mode and button is released
							 (toggleMode && !toggleZoomOn && buttonPressed)) // Toggle mode, zoom is to be toggled off and button is pressed
			{
				StopZoom(actorId);
			}

			const bool shouldStartZoom = (!toggleMode && (buttonPressed || activationMode == eAAM_OnHold)) ||
																	 (toggleMode && toggleZoomOn && buttonPressed);
			if (m_zm && shouldStartZoom)
			{
				const bool zoomingOut = m_zm->IsZoomingInOrOut() && !zoomingIn;
				if ((!zoomed && !zoomingIn) || zoomingOut)
				{
					StartZoom(actorId,1);
				}
				if (buttonPressed)
				{
					m_snapToTargetTimer = 0.5f;
				}
			}
		}
		else if( (!toggleMode && activationMode == eAAM_OnRelease) || (toggleMode && !toggleZoomOn && buttonPressed) )
		{
			StopZoom(actorId);
		}
	}
	else if (trumpZoom)
	{
		StopZoom(actorId);
	}

	return true;
}

//------------------------------------------------------------------------------
bool CWeapon::OnActionZoomXI(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (!AreAnyItemFlagsSet(eIF_Modifying))
	{
		if (g_pGameCVars->hud_ctrlZoomMode)
		{
			if (activationMode == eAAM_OnPress)
			{
				if(m_fm && !m_fm->IsReloading())
				{
					// The zoom code includes the aim assistance
					if (m_fm->AllowZoom())
					{
						StartZoom(actorId,1);
					}
					else
					{
						m_fm->Cancel();
					}
				}
			}
			else if (activationMode == eAAM_OnRelease)
			{
				if(m_fm && !m_fm->IsReloading())
				{
					StopZoom(actorId);
				}
			}
		}
		else
		{
			if (activationMode == eAAM_OnPress && m_fm && !m_fm->IsReloading())
			{
				if (m_fm->AllowZoom())
				{				
					StartZoom(actorId,1);		
				}
				else
				{
					m_fm->Cancel();
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool CWeapon::OnActionStabilize(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (gEnv->bMultiplayer || !m_zm)
		return false;

	if (activationMode == eAAM_OnPress)
	{
		if (CanStabilize())
		{
			m_zm->StartStabilize();
		}
	}
	else
	{
		m_zm->EndStabilize();
	}

	return true;
}

//------------------------------------------------------------------------------
bool CWeapon::OnActionToggleFlashLight(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode != eAAM_OnPress)
		return false;

	ToggleFlashLight();

	return true;
}

//------------------------------------------------------------------------------
void CWeapon::ToggleFlashLight()
{
	static IEntityClass* pFlashLightClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Flashlight");

	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();

	for (size_t i = 0; i < m_accessories.size(); ++i)
	{
		if (m_accessories[i].pClass == pFlashLightClass)
		{
			IItem* pItem = pItemSystem->GetItem(m_accessories[i].accessoryId);
			CFlashLight* pFlashLight = static_cast<CFlashLight*>(pItem);
			pFlashLight->ToggleFlashLight();
			break;
		}
	}
}
