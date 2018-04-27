// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a class which handle client actions on vehicles.

-------------------------------------------------------------------------
History:
- 17:10:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include <CryGame/IGame.h>
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "VehicleClient.h"
#include "GameCVars.h"
#include "Game.h"
#include "GameActions.h"
#include "Weapon.h"
#include "Player.h"
#include "VehicleMovementBase.h"

#include <CryNetwork/ISerialize.h>

void CVehicleClient::InsertActionMapValue(const char * pString, int eVehicleAction)
{
	m_actionNameIds.insert(TActionNameIdMap::value_type(CCryName(pString), eVehicleAction));
}

//------------------------------------------------------------------------
bool CVehicleClient::Init()
{
  m_actionNameIds.clear();	
 
  InsertActionMapValue("v_exit", eVAI_Exit);
	InsertActionMapValue("v_changeseat", eVAI_ChangeSeat);
	InsertActionMapValue("v_changeseat1", eVAI_ChangeSeat1);
	InsertActionMapValue("v_changeseat2", eVAI_ChangeSeat2);
	InsertActionMapValue("v_changeseat3", eVAI_ChangeSeat3);
	InsertActionMapValue("v_changeseat4", eVAI_ChangeSeat4);
	InsertActionMapValue("v_changeseat5", eVAI_ChangeSeat5);

	InsertActionMapValue("v_changeview", eVAI_ChangeView);
	InsertActionMapValue("v_viewoption", eVAI_ViewOption);
	InsertActionMapValue("v_zoom_in", eVAI_ZoomIn);
	InsertActionMapValue("v_zoom_out", eVAI_ZoomOut);

	InsertActionMapValue("attack1", eVAI_Attack1);
	InsertActionMapValue("zoom", eVAI_Attack2);
	InsertActionMapValue("v_attack2", eVAI_Attack2);
	InsertActionMapValue("xi_zoom", eVAI_Attack2);
	InsertActionMapValue("firemode", eVAI_FireMode);
	InsertActionMapValue("v_lights", eVAI_ToggleLights);
	InsertActionMapValue("v_horn", eVAI_Horn);

	InsertActionMapValue("xi_v_attack1", eVAI_Attack1);
	InsertActionMapValue("xi_v_attack2", eVAI_Attack2);
	InsertActionMapValue("xi_v_attack3", eVAI_Attack3);

	InsertActionMapValue("v_rotateyaw", eVAI_RotateYaw);
	InsertActionMapValue("v_rotatepitch", eVAI_RotatePitch);

	InsertActionMapValue("v_moveforward", eVAI_MoveForward);
	InsertActionMapValue("v_moveback", eVAI_MoveBack);
	InsertActionMapValue("v_moveup", eVAI_MoveUp);
	InsertActionMapValue("v_movedown", eVAI_MoveDown);
	InsertActionMapValue("v_turnleft", eVAI_TurnLeft);
	InsertActionMapValue("v_turnright", eVAI_TurnRight);
	InsertActionMapValue("v_strafeleft", eVAI_StrafeLeft);
	InsertActionMapValue("v_straferight", eVAI_StrafeRight);
	InsertActionMapValue("v_rollleft", eVAI_RollLeft);
	InsertActionMapValue("v_rollright", eVAI_RollRight);

	InsertActionMapValue("xi_v_rotateyaw", eVAI_XIRotateYaw);
	InsertActionMapValue("xi_v_rotatepitch", eVAI_XIRotatePitch);
	InsertActionMapValue("xi_v_movey", eVAI_XIMoveY);
	InsertActionMapValue("xi_v_movex", eVAI_XIMoveX);

	InsertActionMapValue("xi_v_moveupdown", eVAI_XIMoveUpDown);
	InsertActionMapValue("xi_v_strafe", eVAI_XIStrafe);

	InsertActionMapValue("xi_v_accelerate", eVAI_XIAccelerate);
	InsertActionMapValue("xi_v_deccelerate", eVAI_XIDeccelerate);

	InsertActionMapValue("v_pitchup", eVAI_PitchUp);
	InsertActionMapValue("v_pitchdown", eVAI_PitchDown);

	InsertActionMapValue("v_brake", eVAI_Brake);
	InsertActionMapValue("v_afterburner", eVAI_AfterBurner);
	InsertActionMapValue("v_boost", eVAI_Boost);

	InsertActionMapValue("v_debug_1", eVAI_Debug_1);
	InsertActionMapValue("v_debug_2", eVAI_Debug_2);

	InsertActionMapValue("v_ripoff_weapon", eVAI_RipoffWeapon);

	ResetFlags();

	// SP: Default to FP in first usage
	// MP: Default to 3P in first usage
	m_tp = gEnv->bMultiplayer; 
	
  return true;
}

//------------------------------------------------------------------------
void CVehicleClient::Reset()
{
	ResetFlags();
	m_tp = false;
}

void CVehicleClient::ResetFlags()
{
	m_bMovementFlagForward = false;
	m_bMovementFlagBack = false;
	m_bMovementFlagRight = false;
	m_bMovementFlagLeft = false;
	m_bMovementFlagUp = false;
	m_bMovementFlagDown = false;
	m_bMovementFlagStrafeLeft = false;
	m_bMovementFlagStrafeRight = false;
	m_fLeftRight.Reset();
	m_fForwardBackward.Reset();
	m_fAccelDecel.Reset();
	m_xiRotation.Set(0,0,0);
}

void CVehicleClient::ProcessXIAction(IVehicle* pVehicle, const EntityId actorId, const float value, const TVehicleActionId leftAction, const TVehicleActionId rightAction, bool& leftFlag, bool& rightFlag)
{
	if(value>0.f)
	{
		pVehicle->OnAction(rightAction, eAAM_OnPress, value, actorId);
		rightFlag = true;
	}
	else if(value==0.f)
	{
		if(rightFlag)
		{
			pVehicle->OnAction(rightAction, eAAM_OnRelease, 0.f, actorId);
			rightFlag = false;
		}
		
		if(leftFlag)
		{
			pVehicle->OnAction(leftAction, eAAM_OnRelease, 0.f, actorId);
			leftFlag = false;
		}
	}
	else//value<0
	{
		pVehicle->OnAction(leftAction, eAAM_OnPress, -value, actorId);
		leftFlag = true;
	}
}

//------------------------------------------------------------------------
void CVehicleClient::OnAction(IVehicle* pVehicle, EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	assert(pVehicle);
 	if (!pVehicle)
		return;
	
  TActionNameIdMap::const_iterator ite = m_actionNameIds.find(actionId);
	if (ite == m_actionNameIds.end())
    return;
			
	IVehicleMovement *pMovement = pVehicle->GetMovement();
	const bool isAir = pMovement && pMovement->GetMovementType()==IVehicleMovement::eVMT_Air;

	switch (ite->second)
	{
		case (eVAI_XIMoveX):	
		{
			ProcessXIAction(pVehicle, actorId, value, eVAI_TurnLeft, eVAI_TurnRight, m_bMovementFlagLeft, m_bMovementFlagRight);
			break;
		}
		case (eVAI_XIMoveY):
		{
			ProcessXIAction(pVehicle, actorId, value, eVAI_MoveBack, eVAI_MoveForward, m_bMovementFlagBack, m_bMovementFlagForward);
			break;
		}
		case (eVAI_XIMoveUpDown):
		{
			ProcessXIAction(pVehicle, actorId, value, eVAI_MoveDown, eVAI_MoveUp, m_bMovementFlagDown, m_bMovementFlagUp);
			break;
		}
		case (eVAI_XIStrafe):
		{
			ProcessXIAction(pVehicle, actorId, value, eVAI_StrafeLeft, eVAI_StrafeRight, m_bMovementFlagStrafeLeft, m_bMovementFlagStrafeRight);
			break;
		}
	case (eVAI_XIAccelerate):
		{
			m_fAccelDecel.m_postive = min(1.f, value);
			pVehicle->OnAction(eVAI_MoveForward, activationMode,  m_fAccelDecel.GetAccumlated(), actorId);
		}
		break;
	case (eVAI_XIDeccelerate):
		{
			m_fAccelDecel.m_negative = max(-1.f, -value);
			pVehicle->OnAction(eVAI_MoveBack, activationMode, m_fAccelDecel.GetAccumlated() * -1.f, actorId);
		}
		break;
  case (eVAI_XIRotateYaw):
		{
			// [JB] FIXME: **NEED** to do a pass on this and make it nice at some point!
			if (isAir && !gEnv->bMultiplayer) // [JB] We currently have no *controllable* air vehicles in mp.. and want mounted guns to behave as normal
			{
				// This only sends the action for a limited number of frames, and gives strange behaviour with VTOL guns. 
				pVehicle->OnAction(eVAI_RotateYaw, eAAM_OnPress, value, actorId);
			}
			else
			{
				// action is sent to vehicle in PreUpdate, so it is repeated every frame.
				float sensitivity = gEnv->bMultiplayer ? max(g_pGameCVars->cl_sensitivityControllerMP /*[0-2] range*/, 0.01f)
					: max(g_pGameCVars->cl_sensitivityController, 0.01f);
				m_xiRotation.x = (value*value*value) * sensitivity;
			}
      break;
		}
  case (eVAI_XIRotatePitch):
		{
			// action is sent to vehicle in PreUpdate, so it is repeated every frame
			float sensitivity = gEnv->bMultiplayer ? max(g_pGameCVars->cl_sensitivityControllerMP /*[0-2] range*/, 0.01f)
				: max(g_pGameCVars->cl_sensitivityController, 0.01f);
			m_xiRotation.y = -(value*value*value) * sensitivity;
			if(ShouldInvertPitch(pVehicle))
			{
				m_xiRotation.y*=-1;
			}
      break;
		}
	case (eVAI_RotateYaw):
		{
			// attempt to normalise the input somewhat (to bring it in line with controller input).
			float sensitivity = max(g_pGameCVars->cl_sensitivity * 0.03333f /*[0-2] range*/, 0.01f);
			if(gEnv->bMultiplayer)
			{
				value = clamp_tpl(g_pGameCVars->v_mouseRotScaleMP*value, -g_pGameCVars->v_mouseRotLimitMP, g_pGameCVars->v_mouseRotLimitMP) * sensitivity;
			}
			else
			{
				value = clamp_tpl(g_pGameCVars->v_mouseRotScaleSP*value, -g_pGameCVars->v_mouseRotLimitSP, g_pGameCVars->v_mouseRotLimitSP) * sensitivity;
			}

			pVehicle->OnAction(ite->second, activationMode, value, actorId);
			break;
		}
  case (eVAI_RotatePitch):
    {
			// attempt to normalise the input somewhat (to bring it in line with controller input).
			float sensitivity = max(g_pGameCVars->cl_sensitivity * 0.03333f /*[0-2] range*/, 0.01f);
			if(gEnv->bMultiplayer)
			{
				value = clamp_tpl(g_pGameCVars->v_mouseRotScaleMP*value, -g_pGameCVars->v_mouseRotLimitMP, g_pGameCVars->v_mouseRotLimitMP) * sensitivity;
			}
			else
			{
				value = clamp_tpl(g_pGameCVars->v_mouseRotScaleSP*value, -g_pGameCVars->v_mouseRotLimitSP, g_pGameCVars->v_mouseRotLimitSP) * sensitivity;
			}

      if (ShouldInvertPitch(pVehicle))
			{
        value *= -1.f;
			}

			pVehicle->OnAction(ite->second, activationMode, value, actorId);
      break;
    }
  case (eVAI_TurnLeft):
	{
		m_fLeftRight.m_negative = max(-1.f, -value);
		pVehicle->OnAction(eVAI_TurnLeft, activationMode,  m_fLeftRight.GetAccumlated() * -1.f, actorId);
		break;
	}
  case (eVAI_TurnRight):
	{
		m_fLeftRight.m_postive = min(1.f, value);
		pVehicle->OnAction(eVAI_TurnRight, activationMode,  m_fLeftRight.GetAccumlated(), actorId);
		break;
	}  
  case (eVAI_MoveForward):
  {
		m_fForwardBackward.m_postive = min(1.f, value);
		pVehicle->OnAction(eVAI_MoveForward, activationMode,  m_fForwardBackward.GetAccumlated(), actorId);
		break;
  }
  case (eVAI_MoveBack):
	{
		m_fForwardBackward.m_negative = max(-1.f, -value);
		pVehicle->OnAction(eVAI_MoveBack, activationMode,  m_fForwardBackward.GetAccumlated() * -1.f, actorId);
		break;
  }
	case (eVAI_ChangeView):
	{
		pVehicle->OnAction(eVAI_ChangeView, activationMode, 0.0f, actorId);
		break;
	}
	case (eVAI_RipoffWeapon):
		{
			if(activationMode == eAAM_OnPress)
			{
				// pass this action to the weapon itself, which handles ripping off if weapon is of the right class
				EntityId weaponId = pVehicle->GetCurrentWeaponId(actorId);
				if(weaponId != 0)
				{
					// don't allow ripping off unless the actor is actually using the correct seat
					//	 (eg not for driver-controlled guns)
					IVehicleSeat* pWeaponSeat = pVehicle->GetWeaponParentSeat(weaponId);
					IVehicleSeat* pOwnerSeat = pVehicle->GetSeatForPassenger(actorId);
					if(pWeaponSeat == pOwnerSeat)
					{
						IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(weaponId);
						if(pItem)
						{
							const CGameActions& actions = g_pGame->Actions();
							pItem->OnAction(actorId, actions.special, activationMode, value);			// 'special' is the action for ripping off guns
						}
					}
				}
			}
		}
		break;
	case (eVAI_ZoomIn):
	case (eVAI_ZoomOut):
  default:
		pVehicle->OnAction(ite->second, activationMode, value, actorId);
    break;		
	}
}

//------------------------------------------------------------------------
bool CVehicleClient::ShouldInvertPitch(IVehicle* pVehicle) const
{
	IVehicleMovement* pVehicleMovement = pVehicle->GetMovement();
	const IVehicleMovement::EVehicleMovementType movementType = (pVehicleMovement != NULL) ? pVehicleMovement->GetMovementType() : IVehicleMovement::eVMT_Other;
	switch(movementType)
	{
	case IVehicleMovement::eVMT_Air:
		{
			if(!gEnv->bMultiplayer)
			{
				return (g_pGameCVars->cl_invertAirVehicleInput == 1);
			}
			else
			{
				return (g_pGameCVars->cl_invertController == 1);
			}
		}
		break;
	default:
		{
			//this includes Sea vehicles by design
			return (g_pGameCVars->cl_invertLandVehicleInput == 1);
		}
		break;
	}
}

//------------------------------------------------------------------------

void CVehicleClient::PreUpdate(IVehicle* pVehicle, EntityId actorId, float frameTime)
{
	if(fabsf(m_xiRotation.x) > 0.001)
	{
		pVehicle->OnAction(eVAI_RotateYaw, eAAM_OnPress, m_xiRotation.x, actorId);		
	}
	if(fabsf(m_xiRotation.y) > 0.001)
	{
		pVehicle->OnAction(eVAI_RotatePitch, eAAM_OnPress, m_xiRotation.y, actorId);		
	}
}

//------------------------------------------------------------------------
void CVehicleClient::OnEnterVehicleSeat(IVehicleSeat* pSeat)
{
	CRY_ASSERT(pSeat);

	ResetFlags();

	bool isThirdPerson = m_tp;

	for (size_t i = 0; i < pSeat->GetViewCount(); ++i)
	{
		IVehicleView* pView = pSeat->GetView(i + 1);
		if (pView && pView->IsThirdPerson() == isThirdPerson)
		{
			pSeat->SetView(i + 1);
			break;
		}
	}

	if(IActor *pPassengerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger()))
	{
		if(pPassengerActor->IsPlayer())
		{
			EnableActionMaps(pSeat, true);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleClient::OnExitVehicleSeat(IVehicleSeat* pSeat)
{
	if (pSeat)
	{
		if (pSeat->IsDriver())
		{
			ResetFlags();
		}

		IVehicleView* pView = pSeat->GetCurrentView();
		if (pView)
		{
			m_tp = pView->IsThirdPerson();
		}

		IActor *pPassengerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger());
		if(pPassengerActor && pPassengerActor->IsPlayer())
		{
			EnableActionMaps(pSeat, false);
		}
	}
}

void CVehicleClient::EnableActionMaps(IVehicleSeat* pSeat, bool enable)
{
	assert(pSeat);

	// illegal to change action maps in demo playback - Lin
	if (!IsDemoPlayback() && pSeat)	
	{
		IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		CRY_ASSERT(pActionMapMan);

		// normal c2 controls are disabled in a vehicle
		pActionMapMan->EnableActionMap("player", !enable);

		// now the general vehicle controls
		if (IActionMap* pActionMap = pActionMapMan->GetActionMap("vehicle_general"))
		{
			pActionMap->SetActionListener(enable ? pSeat->GetPassenger() : 0);	
			pActionMapMan->EnableActionMap("vehicle_general", enable);
		}

		// todo: if necessary enable the vehicle-specific action map here

		// specific controls for this vehicle seat
		const char* actionMap = pSeat->GetActionMap();
		if (actionMap && actionMap[0])
		{
			if (IActionMap* pActionMap = pActionMapMan->GetActionMap(actionMap))
			{
				pActionMap->SetActionListener(enable ? pSeat->GetPassenger() : 0);
				pActionMapMan->EnableActionMap(actionMap, enable);
			}
		}
	}
}
