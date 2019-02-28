// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
  October 2010 : Jens Schöbel added move overlay

*************************************************************************/

#include "StdAfx.h"
#include "PlayerInput.h"
#include "Player.h"
#include "PlayerStateEvents.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"
#include "GameInputActionHandlers.h"
#include "Weapon.h"
#include "WeaponSystem.h"
#include "IVehicleSystem.h"
#include "VehicleClient.h"

#include "UI/HUD/HUDSilhouettes.h"
#include "GameRules.h"
#include "ScreenEffects.h"
#include "PlayerMovementController.h"
#include "GunTurret.h"
#include "GameRulesModules/IGameRulesActorActionModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"

#include "Utility/CryWatch.h"
#include "UI/HUD//HUDEventWrapper.h"

#include <IWorldQuery.h>
#include <IInteractor.h>

#include "RecordingSystem.h"
#include "GodMode.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include <CryAISystem/IAIActor.h>
#include "AI/GameAISystem.h"

#include "StatsRecordingMgr.h"
#include "PlayerPlugin_Interaction.h"

#include "IUIDraw.h"

#include "EntityUtility/EntityScriptCalls.h"
#include "PlayerEntityInteraction.h"
#include "VTOLVehicleManager/VTOLVehicleManager.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"

#include "UI/UICVars.h"
#include "UI/UIManager.h"
#include "UI/UIInput.h"

CPlayerInput::CPlayerInput( CPlayer * pPlayer ) : 
	m_pPlayer(pPlayer), 
	m_actions(0), 
	m_deltaRotation(0,0,0), 
	m_lastMouseRawInput(0,0,0),
	m_deltaMovement(0,0,0), 
	m_xi_deltaMovement(0,0,0),
	m_xi_deltaRotation(0,0,0),
	m_xi_deltaRotationRaw(0.0f, 0.0f, 0.0f),
	m_HMD_deltaRotation(0.0f, 0.0f, 0.0f),
	m_lookAtSmoothRate(0.f, 0.f, 0.f),
	m_flyCamDeltaMovement(0,0,0),
	m_flyCamDeltaRotation(0,0,0),
	m_flyCamTurbo(false),
	m_filteredDeltaMovement(0,0,0),
	m_suitArmorPressTime(0.0f),
	m_suitStealthPressTime(0.0f),
	m_moveButtonState(0),
	m_lastSerializeFrameID(0),
	m_bDisabledXIRot(false),
	m_bUseXIInput(false),
	m_lastSensitivityFactor(1.0f),
	m_lastRegisteredInputTime(0.0f),
	m_lookingAtButtonActive(false),
	m_isAimingWithMouse(false),
	m_isAimingWithHMD(false),
	m_crouchButtonDown(false),
	m_sprintButtonDown(false),
	m_lookAtTimeStamp(0.0f),
	m_fLastWeaponToggleTimeStamp(0.f),
	m_fLastNoGrenadeTimeStamp(0.f),
	m_isNearTheLookAtTarget(false),
	m_autoPickupMode(false),
	m_standingOn(0),
	m_openingVisor(false),
	m_playerInVehicleAtFrameStart(false),
	m_pendingYawSnapAngle(0.0f),
	m_bYawSnappingActive(false)
{
	m_pPlayer->GetGameObject()->CaptureActions(this);

	IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

	pActionMapManager->EnableActionMap("default", true);
	pActionMapManager->EnableActionMap("debug", true);
	pActionMapManager->EnableActionMap("player", true);

#if FREE_CAM_SPLINE_ENABLED
	m_freeCamPlaying = false;
	m_freeCamCurrentIndex = 0;
	m_freeCamPlayTimer = 0.f;
	m_freeCamTotalPlayTime = 0.f;
#endif

	m_nextSlideTime = gEnv->pTimer->GetFrameStartTime();
	memset (m_inputCancelHandler, 0, sizeof(m_inputCancelHandler));

	if (m_actionHandler.GetNumHandlers() == 0)
	{
	#define ADD_HANDLER(action, func) m_actionHandler.AddHandler(actions.action, &CPlayerInput::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(moveforward, OnActionMoveForward);
		ADD_HANDLER(moveback, OnActionMoveBack);
		ADD_HANDLER(moveleft, OnActionMoveLeft);
		ADD_HANDLER(moveright, OnActionMoveRight);
		ADD_HANDLER(rotateyaw, OnActionRotateYaw);
		ADD_HANDLER(hmd_rotatepitch, OnActionHMDRotatePitch);
		ADD_HANDLER(hmd_rotateyaw, OnActionHMDRotateYaw);
		ADD_HANDLER(hmd_rotateroll, OnActionHMDRotateRoll);
		ADD_HANDLER(rotatepitch, OnActionRotatePitch);
		ADD_HANDLER(jump, OnActionJump);
		ADD_HANDLER(crouch, OnActionCrouch);
		ADD_HANDLER(sprint, OnActionSprint);
		ADD_HANDLER(sprint_xi, OnActionSprintXI);
		ADD_HANDLER(use, OnActionUse);
		ADD_HANDLER(attack1_xi, OnActionAttackRightTrigger);

		ADD_HANDLER(special, OnActionSpecial);
		ADD_HANDLER(weapon_change_firemode, OnActionChangeFireMode);

		ADD_HANDLER(thirdperson, OnActionThirdPerson);
#ifdef INCLUDE_DEBUG_ACTIONS
		ADD_HANDLER(flymode, OnActionFlyMode);
#endif
		ADD_HANDLER(godmode, OnActionGodMode);
		ADD_HANDLER(toggleaidebugdraw, OnActionAIDebugDraw);
		ADD_HANDLER(togglepdrawhelpers, OnActionPDrawHelpers);
		ADD_HANDLER(toggledmode, OnActionDMode);
		ADD_HANDLER(record_bookmark, OnActionRecordBookmark);
#ifdef INCLUDE_DEBUG_ACTIONS
		ADD_HANDLER(mannequin_debugai, OnActionMannequinDebugAI);
		ADD_HANDLER(mannequin_debugplayer, OnActionMannequinDebugPlayer);
		ADD_HANDLER(ai_debugCenterViewAgent, OnActionAIDebugCenterViewAgent);
#endif

		ADD_HANDLER(v_rotateyaw, OnActionVRotateYaw); // needed so player can shake unfreeze while in a vehicle
		ADD_HANDLER(v_rotatepitch, OnActionVRotatePitch);

		ADD_HANDLER(xi_v_rotateyaw, OnActionXIRotateYaw);
		ADD_HANDLER(xi_rotateyaw, OnActionXIRotateYaw);
		ADD_HANDLER(xi_rotatepitch, OnActionXIRotatePitch);

		ADD_HANDLER(xi_v_rotatepitch, OnActionXIRotatePitch);
		ADD_HANDLER(xi_movex, OnActionXIMoveX);
		ADD_HANDLER(xi_movey, OnActionXIMoveY);
		ADD_HANDLER(xi_disconnect, OnActionXIDisconnect);

		ADD_HANDLER(invert_mouse, OnActionInvertMouse);

		ADD_HANDLER(flycam_movex, OnActionFlyCamMoveX);
		ADD_HANDLER(flycam_movey, OnActionFlyCamMoveY);
		ADD_HANDLER(flycam_moveup, OnActionFlyCamMoveUp);
		ADD_HANDLER(flycam_movedown, OnActionFlyCamMoveDown);
		ADD_HANDLER(flycam_speedup, OnActionFlyCamSpeedUp);
		ADD_HANDLER(flycam_speeddown, OnActionFlyCamSpeedDown);
		ADD_HANDLER(flycam_turbo, OnActionFlyCamTurbo);
		ADD_HANDLER(flycam_rotateyaw, OnActionFlyCamRotateYaw);
		ADD_HANDLER(flycam_rotatepitch, OnActionFlyCamRotatePitch);
		ADD_HANDLER(flycam_setpoint, OnActionFlyCamSetPoint);
		ADD_HANDLER(flycam_play, OnActionFlyCamPlay);
		ADD_HANDLER(flycam_clear, OnActionFlyCamClear);

		ADD_HANDLER(lookAt, OnActionLookAt);
		ADD_HANDLER(itemPrePickup, OnActionPrePickUpItem);

		ADD_HANDLER(move_overlay_enable, OnActionMoveOverlayTurnOn);
		ADD_HANDLER(move_overlay_disable, OnActionMoveOverlayTurnOff);
		ADD_HANDLER(move_overlay_weight, OnActionMoveOverlayWeight);
		ADD_HANDLER(move_overlay_x, OnActionMoveOverlayX);
		ADD_HANDLER(move_overlay_y, OnActionMoveOverlayY);

		ADD_HANDLER(respawn, OnActionRespawn);

		ADD_HANDLER(nextitem, OnActionSelectNextItem);
		ADD_HANDLER(previtem, OnActionSelectNextItem);
		ADD_HANDLER(handgrenade, OnActionSelectNextItem);
		ADD_HANDLER(toggle_explosive, OnActionSelectNextItem);
		ADD_HANDLER(toggle_special, OnActionSelectNextItem);
		ADD_HANDLER(toggle_weapon, OnActionSelectNextItem);
		ADD_HANDLER(grenade, OnActionQuickGrenadeThrow);
		ADD_HANDLER(xi_grenade, OnActionQuickGrenadeThrow);
		ADD_HANDLER(debug, OnActionSelectNextItem);

		ADD_HANDLER(mouse_wheel, OnActionMouseWheelClick);
		
	#undef ADD_HANDLER
	}

	CCCPOINT(PlayerState_SetInputCallbacks);
}

CPlayerInput::~CPlayerInput()
{
	m_pPlayer->GetGameObject()->ReleaseActions(this);
}

void CPlayerInput::Reset()
{
	m_actions = 0;
	m_lastActions = m_actions;

	ClearDeltaMovement();

	m_xi_deltaMovement.zero();
	m_filteredDeltaMovement.zero();
	m_deltaRotation.Set(0,0,0);
	m_lastMouseRawInput.Set(0,0,0);
	m_xi_deltaRotation.Set(0,0,0);
	m_xi_deltaRotationRaw.Set(0.0f, 0.0f, 0.0f);
	m_lookAtSmoothRate.Set(0.f, 0.f, 0.f);
	m_flyCamDeltaMovement.Set(0,0,0);
	m_flyCamDeltaRotation.Set(0,0,0);
	m_flyCamTurbo=false;
	m_bDisabledXIRot = false;
	m_lastSerializeFrameID = 0;
	m_lastSensitivityFactor = 1.0f;
	m_suitArmorPressTime = 0.0f;
	m_suitStealthPressTime = 0.0f;
	m_lastRegisteredInputTime = gEnv->pTimer->GetAsyncCurTime();
	m_lookingAtButtonActive = false;
	m_fLastWeaponToggleTimeStamp = 0.f;
  m_moveOverlay.Reset();

 	m_standingOn = 0;
	CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT_AUGMENTED);
}

void CPlayerInput::DisableXI(bool disabled)
{
	m_bDisabledXIRot = disabled;
}

//////////////////////////////////////////////////////////////////////////
void CPlayerInput::ApplyMovement(Vec3 delta)
{
  m_deltaMovement.x = clamp_tpl(m_deltaMovement.x+delta.x,-1.0f,1.0f);
  m_deltaMovement.y = clamp_tpl(m_deltaMovement.y+delta.y,-1.0f,1.0f);
  m_deltaMovement.z = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPlayerInput::ClearDeltaMovement()
{
	m_deltaMovement.zero();
	m_actions &= ~ACTION_MOVE;
	m_moveButtonState = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPlayerInput::OnAction( const ActionId& actionId, int activationMode, float value )
{
	// Pass action to actor telemetry.
	m_pPlayer->m_telemetry.OnPlayerAction(actionId, activationMode, value);

	CRY_PROFILE_FUNCTION(PROFILE_GAME);

#if defined(USER_timf)
	CryLogAlways ("$7PLAYER INPUT:$o <FRAME %05d> $6%s $%c[MODE=0x%x] $4value=%.3f$o", gEnv->pRenderer->GetFrameID(false), actionId.c_str(), (char) ((activationMode > 7) ? '8' : (activationMode + '1')), activationMode, value);
#endif

	if (!m_pPlayer->GetLinkedVehicle())
	{
		CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );
	}

	m_lastActions = m_actions;
	m_lastRegisteredInputTime = gEnv->pTimer->GetAsyncCurTime();

	//this tell if OnAction have to be forwarded to scripts, now its true by default, only high framerate actions are ignored
	bool filterOut = true;
	const CGameActions& actions = g_pGame->Actions();
	IVehicle* pVehicle = m_pPlayer->GetLinkedVehicle();

	// disable movement
	if ( !CanMove() )
	{
		ClearDeltaMovement();
	}

	// try to dispatch action to OnActionHandlers
	bool handled = false;

	{
		CRY_PROFILE_REGION(PROFILE_GAME, "New Action Processing");

		handled = m_actionHandler.Dispatch(this, m_pPlayer->GetEntityId(), actionId, activationMode, value, filterOut);
	}

	//------------------------------------

	{
		CRY_PROFILE_REGION(PROFILE_GAME, "Regular Action Processing");
		bool inKillCam = g_pGame->GetRecordingSystem() && (g_pGame->GetRecordingSystem()->IsPlayingBack() || g_pGame->GetRecordingSystem()->IsPlaybackQueued());
		if (!handled)
		{
			filterOut = true;			
			if (!m_pPlayer->GetSpectatorMode() && !inKillCam)
			{
				if (actions.debug_ag_step == actionId)
				{
					gEnv->pConsole->ExecuteString("ag_step");
				}
			}
		}

		if (!m_pPlayer->GetSpectatorMode() && !inKillCam)
		{
			if (pVehicle && m_pPlayer->m_pVehicleClient)
			{
				m_pPlayer->m_pVehicleClient->OnAction(pVehicle, m_pPlayer->GetEntityId(), actionId, activationMode, value);

				//FIXME:not really good
				m_actions = 0;
//			m_deltaRotation.Set(0,0,0);

				ClearDeltaMovement();
			}
			else if (IsPlayerOkToAction())
			{
				m_pPlayer->CActor::OnAction(actionId, activationMode, value);

				if (actionId == actions.use && activationMode != eAAM_OnRelease)
				{
					const SInteractionInfo& interactionInfo = m_pPlayer->GetCurrentInteractionInfo();
					if (interactionInfo.interactionType == eInteraction_Grab || interactionInfo.interactionType == eInteraction_GrabEnemy)
					{
						if (activationMode == eAAM_OnHold)
						{
							SHUDEventWrapper::OnInteractionUseHoldActivated(true);
						}

						m_pPlayer->RequestEnterPickAndThrow( interactionInfo.interactiveEntityId );
					}
					else if(interactionInfo.interactionType == eInteraction_Ladder)
					{
						if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(interactionInfo.interactiveEntityId))
						{
							SStateEventLadder ladderEvent(pEntity);
							m_pPlayer->StateMachineHandleEventMovement(ladderEvent);
						}
					}

					// Record 'Use' telemetry stats.
					// I'm not sure this is the best place to perform the check, but the actual process of using items appears to be handled within the scripting system.

					if((m_actions & ACTION_USE) && (interactionInfo.interactionType == eInteraction_Use))
					{
						IEntity	*pInteractionEntity = gEnv->pEntitySystem->GetEntity(interactionInfo.interactiveEntityId);

						CStatsRecordingMgr::TryTrackEvent(m_pPlayer, eGSE_Use, pInteractionEntity ? pInteractionEntity->GetName() : "unknown entity");
					}
				}
			}
		}
	}

	UIEvents::Get<CUIInput>()->OnActionInput(actionId, activationMode, value);

	if (IsItemPickUpScriptAction(actionId))
	{
		CPlayerEntityInteraction& playerEntityInteractor = m_pPlayer->GetPlayerEntityInteration();
		playerEntityInteractor.ItemPickUpMechanic(m_pPlayer, actionId, activationMode);
	}

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules != NULL)
	{
		pGameRules->OnActorAction( m_pPlayer, actionId, activationMode, value );
	}
}

const bool CPlayerInput::AllowToggleWeapon(const int activationMode, const float currentTime)
{
	if(gEnv->bMultiplayer)
	{
		if(m_pPlayer->IsStillWaitingOnServerUseResponse())
		{
			return false;
		}
		if (!m_pPlayer->AllowSwitchingItems() || activationMode != eAAM_OnPress)
		{
			return false;
		}
	}
	else
	{
		if (!CanLookAt())
		{
			if (activationMode != eAAM_OnPress)
			{
				return false;
			}
		}
		else if (CanLookAt() && activationMode != eAAM_OnRelease)
		{
			return false;
		}
		else if (CanLookAt() && activationMode == eAAM_OnRelease && currentTime > m_lookAtTimeStamp+g_pGameCVars->pl_useItemHoldTime)
		{
			return false;
		}
	}

	return true;
}

bool CPlayerInput::IsPlayerOkToAction() const
{
	return m_pPlayer->IsPlayerOkToAction();
}

//this function basically returns a smoothed movement vector, for better movement responsivness in small spaces
const Vec3 &CPlayerInput::FilterMovement(const Vec3 &desired)
{
	float frameTimeCap(min(gEnv->pTimer->GetFrameTime(),0.033f));
	float inputAccel(g_pGameCVars->pl_inputAccel);

	Vec3 oldFilteredMovement = m_filteredDeltaMovement;

	if (desired.len2()<0.01f)
	{
		m_filteredDeltaMovement.zero();
	}
	else if (inputAccel<=0.0f)
	{
		m_filteredDeltaMovement = desired;
	}
	else
	{
		Vec3 delta(desired - m_filteredDeltaMovement);

		float len(delta.len());
		if (len<=1.0f)
			delta = delta * (1.0f - len*0.55f);

		m_filteredDeltaMovement += delta * min(frameTimeCap * inputAccel,1.0f);
	}

	if (oldFilteredMovement.GetDistance(m_filteredDeltaMovement) > 0.001f && !m_pPlayer->GetLinkedVehicle())
	{
		CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );
	}

	return m_filteredDeltaMovement;
}

bool CPlayerInput::CanMove() const
{
	bool canMove = !m_pPlayer->GetSpectatorMode();
	canMove &= m_pPlayer->IsPlayerOkToAction();

	const SActorStats * pActorStats = m_pPlayer->GetActorStats();

	if(pActorStats)
	{
		canMove &= (pActorStats->mountedWeaponID == 0);
	}

	return canMove;
}

bool CPlayerInput::CanCrouch() const
{
	if (!CanMove())
		return false;

	if (m_pPlayer->GetBlockMovementInputs())
		return false;

	CWeapon* pWeapon = m_pPlayer->GetWeapon(m_pPlayer->GetCurrentItemId());
	if (!pWeapon)
		return true;

	bool rippedOff = pWeapon->IsRippedOff();
	bool heavyWeapon = pWeapon->IsHeavyWeapon();

	bool canCrouch = true;

	if (heavyWeapon && (!rippedOff || m_sprintButtonDown))
		canCrouch = false;

	return canCrouch;
}

void CPlayerInput::NormalizeInput(float& fX, float& fY, float fCoeff, float fCurve)
{
	float fMag = MapControllerValue(min(sqrt_tpl(fX*fX + fY*fY), 1.0f), fCoeff, fCurve, false);
	if (fMag > 0.0f)
	{
		float fAbsX = fabs_tpl(fX);
		float fAbsY = fabs_tpl(fY);
		float fFactor = fMag / max(fAbsX, fAbsY);

		fX *= fFactor;
		fY *= fFactor;
	}
}

void CPlayerInput::DrawDebugInfo()
{
#if !defined(_RELEASE)
	const float fRadius = 60.0f;
	const float fX = 120.0f;
	const float fY = 600.0f;
	const float fTimeOut = 0.5f;
	const float fSize = 12.f;

	// process the input as in PreProcess, but without scaling
	Ang3 processedDeltaRot(UpdateXIInputs(m_xi_deltaRotationRaw, false));

	// Draw enclosing circle
	ColorF whiteColor(0.7f, 1.0f, 1.0f, 1.0f);
	// pUIDraw->DrawCircleHollow(fX, fY, fRadius, 1.0f, whiteColor.pack_argb8888());

	// Print explanatory text
	IFFont* pFont = gEnv->pCryFont->GetFont("default");

	string sMsg;
	sMsg.Format("Raw input: (%f, %f)", m_xi_deltaRotationRaw.z, m_xi_deltaRotationRaw.x);
	IRenderAuxText::Draw2dLabel( fX - fRadius, fY + fRadius + fSize, fSize, Col_Green, false, "%s", sMsg.c_str());

	sMsg.Format("Processed input: (%f, %f)", processedDeltaRot.z, processedDeltaRot.x);
	IRenderAuxText::Draw2dLabel( fX - fRadius, fY + fRadius + (fSize * 2.f), fSize, Col_Orange, false, "%s", sMsg.c_str());

	// to improve following the movement
	IPersistantDebug* pPersistantDebug = gEnv->pGameFramework->GetIPersistantDebug();
	pPersistantDebug->Begin("CPlayerInput::DrawDebugInfo", false);

	float fTraceRawXStart = fX + (m_debugDrawStats.lastRaw.z * fRadius);
	float fTraceRawYStart = fY + (-m_debugDrawStats.lastRaw.x * fRadius);

	float fTraceProcessedXStart = fX + (-m_debugDrawStats.lastProcessed.z * fRadius);
	float fTraceProcessedYStart = fY + (-m_debugDrawStats.lastProcessed.x * fRadius);

	float fRawXEnd = fX + (m_xi_deltaRotationRaw.z * fRadius);
	float fRawYEnd = fY + (-m_xi_deltaRotationRaw.x * fRadius);
	float fProcessedXEnd = fX + (-processedDeltaRot.z * fRadius);
	float fProcessedYEnd = fY + (-processedDeltaRot.x * fRadius);

	if ((fProcessedXEnd != fX) && (fProcessedYEnd != fY))
		pPersistantDebug->Add2DLine(fTraceProcessedXStart, fTraceProcessedYStart, fProcessedXEnd, fProcessedYEnd, Col_Orange, fTimeOut);
	if ((fRawXEnd != fX) && (fRawYEnd != fY))
		pPersistantDebug->Add2DLine(fTraceRawXStart, fTraceRawYStart, fRawXEnd, fRawYEnd, Col_Green, fTimeOut);

	// Display our aiming displacement
	const CCamera& camera = GetISystem()->GetViewCamera();
	float fDepth = camera.GetNearPlane() + 0.15f;
	Vec3 vNewAimPos = camera.GetPosition() + (camera.GetViewdir() * fDepth);

	pPersistantDebug->AddLine(m_debugDrawStats.vOldAimPos, vNewAimPos, Col_White, fTimeOut);

	// Draw input lines
	pPersistantDebug->Begin("CPlayerInput::DrawDebugInfo::InputLines", true);
	pPersistantDebug->Add2DLine(fX, fY, fProcessedXEnd, fProcessedYEnd, Col_Orange, 0.1f);
	pPersistantDebug->Add2DLine(fX, fY, fRawXEnd, fRawYEnd, Col_Green, 0.1f);

	// store values for next call
	m_debugDrawStats.lastRaw = m_xi_deltaRotationRaw;
	m_debugDrawStats.lastProcessed = processedDeltaRot;
	m_debugDrawStats.vOldAimPos = vNewAimPos;
#endif //!defined(_RELEASE)
}

Ang3 CPlayerInput::UpdateXIInputs(const Ang3& inputAngles, bool bScaling/* = true*/)
{
	const SCVars* const __restrict pGameCVars = g_pGameCVars;

	if (pGameCVars->aim_altNormalization.enable == 0)
	{
		Ang3 xiDeltaRot(inputAngles.x, 0.0f, inputAngles.z);

		// Calculate the parameters for the input mapping
		const float fCurve = g_pGameCVars->controller_power_curve;

		// NormalizeInput maps the magnitude internally
		NormalizeInput(xiDeltaRot.z, xiDeltaRot.x, 1.0f, fCurve);
		xiDeltaRot.z = -xiDeltaRot.z;

		return Ang3(xiDeltaRot.x, 0.0f, xiDeltaRot.z);	
	}
	else
	{
		Ang3 xiDeltaRot(inputAngles.x, 0.0f, inputAngles.z);

		// Calculate the parameters for the input mapping
		const float fCoeff = g_pGameCVars->aim_altNormalization.hud_ctrl_Coeff_Unified;
		const float fCurve = g_pGameCVars->aim_altNormalization.hud_ctrl_Curve_Unified;

		// NormalizeInput maps the magnitude internally
		NormalizeInput(xiDeltaRot.z, xiDeltaRot.x, bScaling ? fCoeff : 1.0f, fCurve);
		xiDeltaRot.z = -xiDeltaRot.z;

		return Ang3(xiDeltaRot.x, 0.0f, xiDeltaRot.z);
	}
}

void CPlayerInput::PreUpdate()
{
	CMovementRequest request;

	float generalSensitivity = 1.0f;	//sensitivity adjustment regardless of control type
	float mouseSensitivity;						//sensitivity adjustment specific to mouse control

	float dt = gEnv->pTimer->GetFrameTime();

	UpdateAutoLookAtTargetId(dt);
	
	mouseSensitivity = 0.00333f * max(0.01f, g_pGameCVars->cl_sensitivity);

	mouseSensitivity *= gf_PI / 180.0f;//doesnt make much sense, but after all helps to keep reasonable values for the sensitivity cvars
	
	//Move this to player rotation instead?
	generalSensitivity *= m_pPlayer->GetWeaponRotationFactor();

	//Interpolate(m_lastSensitivityFactor, generalSensitivity, 4.0f, dt, 1.0f);
	//generalSensitivity = m_lastSensitivityFactor;

	Ang3 deltaRotation = m_deltaRotation * mouseSensitivity * generalSensitivity;

	deltaRotation += m_HMD_deltaRotation; // We don't control player's head, so it should not have sensitivity factors.
	m_HMD_deltaRotation.Set(0,0,0);			 // We don't need the delta anymore.
																							
	IVehicle *pVehicle = m_pPlayer->GetLinkedVehicle();

	m_playerInVehicleAtFrameStart = (pVehicle != NULL);

	// apply rotation from xinput controller
	if(!m_bDisabledXIRot)
	{
		Ang3 rawRotationInput;
		rawRotationInput.x = clamp_tpl(m_xi_deltaRotationRaw.x, -1.0f, 1.0f);
		rawRotationInput.y = 0.0f;
		rawRotationInput.z = clamp_tpl(m_xi_deltaRotationRaw.z, -1.0f, 1.0f);

		m_xi_deltaRotation = UpdateXIInputs(rawRotationInput);

		Ang3 xiDeltaRot = m_xi_deltaRotation;

		//Apply turning acceleration to the input values. This is here because the acceleration
		//	should be application-specific, e.g. different for the player and vehicles
		m_pPlayer->m_pMovementController->ApplyControllerAcceleration(xiDeltaRot.x, xiDeltaRot.z, gEnv->pTimer->GetFrameTime());

		// Applying aspect modifiers
		if (g_pGameCVars->hud_aspectCorrection > 0)
		{
			int vw = gEnv->pRenderer->GetWidth();
			int vh = gEnv->pRenderer->GetHeight();

			float med=((float)vw+vh)/2.0f;
			float crW=((float)vw)/med;
			float crH=((float)vh)/med;

			xiDeltaRot.x*=g_pGameCVars->hud_aspectCorrection == 2 ? crW : crH;
			xiDeltaRot.z*=g_pGameCVars->hud_aspectCorrection == 2 ? crH : crW;
		}

		if(g_pGameCVars->cl_invertController)
			xiDeltaRot.x*=-1;
		
		// Controller framerate compensation needs frame time! 
		// The constant is to counter for small frame time values.
		// adjust some too small values, should be handled differently later on
		if(g_pGameCVars->pl_aim_acceleration_enabled == 0)
		{
			deltaRotation += (xiDeltaRot * dt * 50.0f * generalSensitivity * mouseSensitivity);
		}
		else
		{
			const float sensitivityValue = (gEnv->bMultiplayer ? g_pGameCVars->cl_sensitivityControllerMP : g_pGameCVars->cl_sensitivityController);
			float controllerSensitivity = clamp_tpl(sensitivityValue * 0.5f, 0.0f, 1.0f);
			float fractionIncrease = 1.0f + ((controllerSensitivity - 0.5f) * 1.5f ); //result is between 0.25f to 1.75f
			xiDeltaRot.z *= g_pGameCVars->controller_multiplier_z * fractionIncrease;
			xiDeltaRot.x *= g_pGameCVars->controller_multiplier_x * fractionIncrease;

			//Output debug information
			if(g_pGameCVars->ctrlr_OUTPUTDEBUGINFO > 0)
			{
				const float dbg_my_white[4] = {1,1,1,1};
				IRenderAuxText::Draw2dLabel( 20, 400, 1.3f, dbg_my_white, false, "PRE-DT MULTIPLY:\n  xRot: %.9f\n  zRot: %.9f\n", xiDeltaRot.x, xiDeltaRot.z);
			}

			deltaRotation += (xiDeltaRot * dt * generalSensitivity);
		}

		if (pVehicle)
		{
			if (m_pPlayer->m_pVehicleClient)
			{
				m_pPlayer->m_pVehicleClient->PreUpdate(pVehicle, m_pPlayer->GetEntityId(), dt);
			}

			//FIXME:not really good
			m_actions = 0;
			ClearDeltaMovement();
			m_deltaRotation.Set(0,0,0);
		}
	}

	if (m_pPlayer->GetBlockMovementInputs())
	{
		ClearDeltaMovement();
		m_actions &= ~(ACTION_JUMP);
	}
	else if(m_bUseXIInput)
	{
		ClearDeltaMovement();	//we are ignoring the delta movement values, so we need to clear the button inputs as well.

		m_deltaMovement.x = m_xi_deltaMovement.x;
		m_deltaMovement.y = m_xi_deltaMovement.y;
		m_deltaMovement.z = 0;

		if (m_xi_deltaMovement.len2()>0.0f)
			m_actions |= ACTION_MOVE;
		else
			m_actions &= ~ACTION_MOVE;
	}

	bool animControlled(m_pPlayer->m_stats.animationControlledID!=0);

	// If there was a recent serialization, ignore the delta rotation, since it's accumulated over several frames.
	if (gEnv->pRenderer && ((m_lastSerializeFrameID + 2) > gEnv->pRenderer->GetFrameID()))
		deltaRotation.Set(0,0,0);

	// Aim & look forward along the 'BaseQuat' direction
	const Vec3 playerEyePosition = m_pPlayer->GetEntity()->GetWorldTM().TransformPoint(m_pPlayer->GetEyeOffset());
	const Vec3 playerTargetPosition = playerEyePosition + 5 * m_pPlayer->GetBaseQuat().GetColumn1();
	request.SetAimTarget( playerTargetPosition );
	request.SetLookTarget( playerTargetPosition );

	request.AddDeltaRotation( deltaRotation );
	
	if (g_pGameCVars->cl_controllerYawSnapEnable && (fabsf(m_pendingYawSnapAngle) > FLT_EPSILON))
	{
		const float angleSign = (float)__fsel(m_pendingYawSnapAngle, 1.0f, -1.0f);

		const float snapAngle = DEG2RAD(g_pGameCVars->cl_controllerYawSnapAngle);
		const float snapSpeed = snapAngle / CLAMP(g_pGameCVars->cl_controllerYawSnapTime, 0.01f, 1000.0f);
		const float additionalYawAngle = angleSign * min(snapSpeed * dt, fabsf(m_pendingYawSnapAngle));			

		request.AddDeltaRotation( Ang3(0.0f, 0.0f, additionalYawAngle) );

		m_pendingYawSnapAngle -= additionalYawAngle;
	}

	if (!animControlled)
	{
    request.AddDeltaMovement( FilterMovement( m_moveOverlay.GetMixedOverlay(m_deltaMovement) ) );
	}

	// handle actions
	if (m_actions & ACTION_JUMP)
	{
		request.SetJump();
	}

	request.SetStance(FigureOutStance());

	float pseudoSpeed = 0.0f;
  if ((m_moveOverlay.GetMixedOverlay(m_deltaMovement)).len2() > 0.0f)
	{
		pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(m_pPlayer->IsSprinting(), m_moveOverlay.GetMixedOverlay(m_deltaMovement).len());
	}

	request.SetPseudoSpeed(pseudoSpeed);

  Vec3 overlayDeltaMovement( (m_moveOverlay.GetMixedOverlay(m_deltaMovement)) );
  if (overlayDeltaMovement.GetLengthSquared() > 0.01f)
	{
    float moveAngle = (float)RAD2DEG(fabs_tpl(atan2_tpl(-overlayDeltaMovement.x, fabsf(overlayDeltaMovement.y)<0.01f?0.01f:overlayDeltaMovement.y)));
		request.SetAllowStrafing(moveAngle > 20.0f);
	}
	else
	{
		request.SetAllowStrafing(true);
	}

	// send the movement request to the appropriate spot!
	m_pPlayer->m_pMovementController->RequestMovement( request );
	m_pPlayer->m_actions = m_actions;

	if (g_pGameCVars->g_detachCamera && g_pGameCVars->g_moveDetachedCamera)
	{
		HandleMovingDetachedCamera(m_flyCamDeltaRotation, m_flyCamDeltaMovement);
	}

	// reset things for next frame that need to be
	m_lastMouseRawInput = m_deltaRotation;
	m_deltaRotation.Set(0.0f, 0.0f, 0.0f);

	if (m_pPlayer->GetFlyMode() == 0)
	{
		m_actions &= ~ACTION_JUMP;
	}
}

// TODO - tidy up
// TODO - add up down movement on analogue shoulder buttons
// TODO - perhaps move to somewhere else more suitable?
void CPlayerInput::HandleMovingDetachedCamera(const Ang3 &deltaRotation, const Vec3 &deltaMovement)
{
	//CryWatch("deltaRot=%f,%f,%f; deltaMove=%f,%f,%f", deltaRotation.x, deltaRotation.y, deltaRotation.z, deltaMovement.x, deltaMovement.y, deltaMovement.z);
	IView*  pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView();
	assert(pView);

	if (!pView)
	{
		return;
	}

#if FREE_CAM_SPLINE_ENABLED
	if (g_pGameCVars->g_detachedCameraDebug)
	{
		ColorB col(255,255,255);
		ColorB col2(255,255,130);

		SFreeCamPointData *camData = NULL;
		SFreeCamPointData *lastCamData = NULL;
		float totalDistance = 0.f;
		Vec3 diff;

		for (int num=0; num < MAX_FREE_CAM_DATA_POINTS; ++num)
		{
			camData = &m_freeCamData[num];
			if (camData->valid)
			{
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawPoint(camData->position, col, 3);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawPoint(camData->lookAtPosition, col2, 3);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(camData->position, col, camData->lookAtPosition, col);

				if (lastCamData)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(camData->position, col, lastCamData->position, col, 3.f);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(camData->lookAtPosition, col2, lastCamData->lookAtPosition, col2, 3.f);
				}
			}
			else
			{
				break;
			}

			lastCamData = camData;
		}
	}

	if (m_freeCamPlaying)
	{
		if (m_freeCamSpline.num_keys() > 0)
		{
			const SViewParams*  pViewParams = pView->GetCurrentParams();

			SViewParams  params = (*pViewParams);

			float frameTime=gEnv->pTimer->GetFrameTime();
			m_freeCamPlayTimer += frameTime;

			Vec3 point, lookAtPoint;

			float time = CLAMP((m_freeCamPlayTimer / m_freeCamTotalPlayTime), 0.f, 1.f);

			m_freeCamSpline.interpolate( time, point);
			m_freeCamLookAtSpline.interpolate( time, lookAtPoint);

			Vec3 diff = lookAtPoint - point;
			diff.Normalize();
			Quat lookAtQuat = Quat::CreateRotationVDir( diff );

			params.rotation = lookAtQuat;
			params.position = point;
			pView->SetCurrentParams(params);

			if (m_freeCamPlayTimer > m_freeCamTotalPlayTime)
			{
				if (g_pGameCVars->g_flyCamLoop)
				{
					m_freeCamPlayTimer = 0;
				}
				else
				{
					m_freeCamPlaying = false;
					m_freeCamPlayTimer = m_freeCamTotalPlayTime;
				}
			}
		}
		else
		{
			m_freeCamPlaying = false;
		}
	}
	else
#endif
	{
		float moveSpeed = g_pGameCVars->g_detachedCameraMoveSpeed;
		if (m_flyCamTurbo)
		{
			moveSpeed *= g_pGameCVars->g_detachedCameraTurboBoost;
		}

		const SViewParams*  pViewParams = pView->GetCurrentParams();

		Quat curRot = pViewParams->rotation;
		Quat newRot = curRot;
		//Quat::CreateRotationXYZ()

		float frameTime=gEnv->pTimer->GetFrameTime();

		Ang3 useRot;
		useRot.x = MapControllerValue(deltaRotation.x, g_pGameCVars->g_detachedCameraRotateSpeed, 2.5f, false);
		useRot.y = MapControllerValue(deltaRotation.y, g_pGameCVars->g_detachedCameraRotateSpeed, 2.5f, false);
		useRot.z = MapControllerValue(deltaRotation.z, g_pGameCVars->g_detachedCameraRotateSpeed, 2.5f, true);

		// space of transformation done left to right
		// rotz in world space (or no space)
		// newRot is applied to worldspaced rotz
		// rotx in current view space already worldspaced rotz (as held in newRot)
		newRot = Quat::CreateRotationZ(frameTime*useRot.z) * newRot * (Quat::CreateRotationX(frameTime*useRot.x) ) ;
		newRot.Normalize();

		Vec3 movement; // don't call FilterMovement() its actually doing state things as well
		movement.x = MapControllerValue(deltaMovement.x, moveSpeed, 2.5f, false);
		movement.y = MapControllerValue(deltaMovement.y, moveSpeed, 2.5f, false);
		movement.z = MapControllerValue(deltaMovement.z, moveSpeed, 2.5f, false);

		Vec3 rightDir = newRot.GetColumn0();
		Vec3 upDir = newRot.GetColumn2();
		Vec3 lookDir = newRot.GetColumn1();

		lookDir *= movement.y * frameTime;
		rightDir *= movement.x * frameTime;
		upDir *= movement.z * frameTime;

		movement = lookDir + rightDir + upDir;

		SViewParams  params = (*pViewParams);
		params.position += movement;
		params.rotation = newRot;
		pView->SetCurrentParams(params);
	}
}

EStance CPlayerInput::FigureOutStance()
{
	if (m_actions & (ACTION_CROUCH | ACTION_FORCE_CROUCH))
		return STANCE_CROUCH;
	else if (m_actions & ACTION_RELAXED)
		return STANCE_RELAXED;
	else if (m_actions & ACTION_STEALTH)
		return STANCE_STEALTH;
	else if (m_pPlayer->GetStance() == STANCE_NULL)
		return STANCE_STAND;
	return STANCE_STAND;
}

void CPlayerInput::Update()
{
	if( (m_actions & ACTION_FORCE_CROUCH) && m_pPlayer->GetStance() == STANCE_CROUCH)
	{
		//Remove the force crouch action as soon as physics is enabled for the player - the player will then un-crouch as soon as physically possible (unless the user is holding down the crouch button)
		pe_player_dynamics player_dynamics;
		IPhysicalEntity* pPhysicalEntity = m_pPlayer->GetEntity()->GetPhysics();
		if(pPhysicalEntity && pPhysicalEntity->GetParams(&player_dynamics) && player_dynamics.bActive)
		{
			m_actions &= ~ACTION_FORCE_CROUCH;
		}
	}

	if(m_pPlayer->IsSprinting())
	{
		if (m_actions & ACTION_CROUCH)
		{
			m_actions &= ~ACTION_SPRINT;
		}
	}

	UpdateWeaponToggle();

	if(m_autoPickupMode)
	{
		const SInteractionInfo& interactionInfo = m_pPlayer->GetCurrentInteractionInfo();
		if(interactionInfo.interactionType == eInteraction_PickupItem || interactionInfo.interactionType == eInteraction_ExchangeItem)
		{
			const CGameActions& actions = g_pGame->Actions();
			OnActionPrePickUpItem(interactionInfo.interactiveEntityId, CCryName("itemPickup"), eAAM_OnHold, 0.f);
			CPlayerEntityInteraction& playerEntityInteractor = m_pPlayer->GetPlayerEntityInteration();
			playerEntityInteractor.ItemPickUpMechanic(m_pPlayer, actions.itemPickup, eAAM_OnHold);
			m_autoPickupMode = false;
		}
	}
}

// [tlh] TODO? copy-pasted from HUDTagNames.cpp (where it's implicitly static - ie. not in any header files) ... shouldn't this be in the engine alongside the ProjectToScreen funcs?
static bool CoordsOnScreen(const Vec3& vScreenSpace)
{
	bool bResult = true;

	if(vScreenSpace.z < 0.0f || vScreenSpace.z > 1.0f)
	{
		bResult = false;
	}

	if(vScreenSpace.y < 0.0f || vScreenSpace.y > 100.0f)
	{
		bResult = false;
	}

	if(vScreenSpace.x < 0.0f || vScreenSpace.x > 100.0f)
	{
		bResult = false;
	}

	return bResult;
}

void CPlayerInput::PostUpdate()
{
	/////////////////////////////////////////////////////////////////////////////////
	// Have we changed what entity we are standing on?
	EntityId standingOn = 0;
	IPhysicalEntity* pEnt = m_pPlayer->GetEntity()->GetPhysics();
	pe_status_living psl;
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules && pEnt && pEnt->GetStatus(&psl) && psl.pGroundCollider)
	{
		IEntity* pGroundEntity = gEnv->pEntitySystem->GetEntityFromPhysics(psl.pGroundCollider);
		if (pGroundEntity)
		{
			int id = pGroundEntity->GetId();
			if (pGameRules->GetVTOLVehicleManager() && pGameRules->GetVTOLVehicleManager()->IsVTOL(id))
				standingOn = id;
		}
	}
	if (standingOn!=m_standingOn)
	{
		CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );
		CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT_AUGMENTED);
		m_standingOn = standingOn;
	}
	/////////////////////////////////////////////////////////////////////////////////

	if (m_actions!=m_lastActions && !m_pPlayer->GetLinkedVehicle())
		CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );

#ifndef _RELEASE
	// Debug Drawing
	if (g_pGameCVars->pl_debug_aiming_input && !m_bDisabledXIRot)
		DrawDebugInfo();
#endif
}

void CPlayerInput::GetState( SSerializedPlayerInput& input )
{
	SMovementState movementState;
	m_pPlayer->GetMovementController()->GetMovementState( movementState );

	input.stance = FigureOutStance();
	input.inAir = m_pPlayer->IsInAir();

#if 0
	{
		const float XPOS = 200.0f;
		const float YPOS = 140.0f;
		const float FONT_SIZE = 2.0f;
		const float FONT_COLOUR[4] = {1,1,1,1};
		IPhysicalEntity* pEnt = m_pPlayer->GetEntity()->GetPhysics();
		pe_status_dynamics dynStat;
		pEnt->GetStatus(&dynStat);
		IRenderAuxText::Draw2dLabel(XPOS, YPOS, FONT_SIZE, FONT_COLOUR, false, "FilteredDelta (%f, %f, %f) Vel (%f, %f, %f)", m_filteredDeltaMovement.x, m_filteredDeltaMovement.y, m_filteredDeltaMovement.z, dynStat.v.x, dynStat.v.y, dynStat.v.z);
	}
#endif //0
	

	if (g_pGameCVars->pl_serialisePhysVel)
	{
		//--- Serialise the physics vel instead, velocity over the NET_SERIALISE_PLAYER_MAX_SPEED will be clamped by the network so no guards here
		input.standingOn = m_standingOn;
		input.deltaMovement.zero();
		IPhysicalEntity* pEnt = m_pPlayer->GetEntity()->GetPhysics();
		pe_status_living psl;
		if (pEnt && pEnt->GetStatus(&psl))
		{
			// Remove the ground velocity
			input.deltaMovement = (psl.vel - psl.velGround) / g_pGameCVars->pl_netSerialiseMaxSpeed;
			//input.deltaMovement.z = 0.0f;
		}
	}
	else
	{
		Quat worldRot = m_pPlayer->GetBaseQuat();
		input.deltaMovement = worldRot.GetNormalized() * m_filteredDeltaMovement;
		// ensure deltaMovement has the right length
		input.deltaMovement = input.deltaMovement.GetNormalizedSafe(ZERO) * m_filteredDeltaMovement.GetLength();
	}
	input.sprint = m_pPlayer->IsSprinting();

	input.lookDirection = movementState.eyeDirection;
	input.bodyDirection = movementState.entityDirection;
	input.aiming = true;
	input.usinglookik = true;
	input.allowStrafing = true;

	float pseudoSpeed=0.0f;
	if (input.deltaMovement.len2() > 0.0f)
		pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(input.sprint);
	input.pseudoSpeed=pseudoSpeed;
}

void CPlayerInput::SetState( const SSerializedPlayerInput& input )
{
	GameWarning("CPlayerInput::SetState called: should never happen");
}

void CPlayerInput::SerializeSaveGame( TSerialize ser )
{
	if(ser.GetSerializationTarget() != eST_Network)
	{
		// Store the frame we serialize, to avoid accumulated input during serialization.
		m_lastSerializeFrameID = gEnv->pRenderer->GetFrameID();

		if(ser.IsReading())
		{
			Reset();
		}
	}
}

bool CPlayerInput::OnActionMoveForward(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	bool bClearDeltaMovement = false;
	if (CanMove() || (activationMode == eAAM_OnRelease))
	{
		if(activationMode == eAAM_OnRelease)
		{
			if(!(m_moveButtonState&eMBM_Left) && !(m_moveButtonState&eMBM_Back) && !(m_moveButtonState&eMBM_Right))
			{
				bClearDeltaMovement = true;
				m_actions &= ~ACTION_MOVE;
			}
		}
		else
		{
			m_actions |= ACTION_MOVE;
		}

		if(CheckMoveButtonStateChanged(eMBM_Forward, activationMode))
		{
			ApplyMovement(Vec3(0,value*2.0f - 1.0f,0));
			AdjustMoveButtonState(eMBM_Forward, activationMode);
		}
	}

	m_bUseXIInput = false;
	if (bClearDeltaMovement)
	{
		ClearDeltaMovement();
	}

	return false;
}

bool CPlayerInput::OnActionMoveBack(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	bool bClearDeltaMovement = false;
	if (CanMove() || (activationMode == eAAM_OnRelease))
	{
		if(activationMode == eAAM_OnRelease)
		{
			if(!(m_moveButtonState&eMBM_Left) && !(m_moveButtonState&eMBM_Forward) && !(m_moveButtonState&eMBM_Right))
			{
				bClearDeltaMovement = true;
				m_actions &= ~ACTION_MOVE;
			}
		}
		else
		{
			m_actions |= ACTION_MOVE;
		}

		if(CheckMoveButtonStateChanged(eMBM_Back, activationMode))
		{
			ApplyMovement(Vec3(0,-(value*2.0f - 1.0f),0));
			AdjustMoveButtonState(eMBM_Back, activationMode);
		}
	}
	m_bUseXIInput = false;

	if (bClearDeltaMovement)
	{
		ClearDeltaMovement();
	}


	return false;
}

bool CPlayerInput::OnActionMoveLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	bool bClearDeltaMovement = false;
	if (CanMove() || (activationMode == eAAM_OnRelease))
	{
		if(activationMode == eAAM_OnRelease)
		{
			if(!(m_moveButtonState&eMBM_Forward) && !(m_moveButtonState&eMBM_Back) && !(m_moveButtonState&eMBM_Right))
			{
				bClearDeltaMovement = true;
				m_actions &= ~ACTION_MOVE;
		}
		}
		else
		{
			m_actions |= ACTION_MOVE;
		}

		if(CheckMoveButtonStateChanged(eMBM_Left, activationMode))
		{
			ApplyMovement(Vec3(-(value*2.0f - 1.0f),0,0));
			AdjustMoveButtonState(eMBM_Left, activationMode);
		}
	}
	m_bUseXIInput = false;

	if (bClearDeltaMovement)
	{
		ClearDeltaMovement();
	}

	return false;
}

bool CPlayerInput::OnActionMoveRight(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	bool bClearDeltaMovement = false;
	if (CanMove() || (activationMode == eAAM_OnRelease))
	{
		if(activationMode == eAAM_OnRelease)
		{
			if(!(m_moveButtonState&eMBM_Left) && !(m_moveButtonState&eMBM_Back) && !(m_moveButtonState&eMBM_Forward))
			{
				bClearDeltaMovement = true;
				m_actions &= ~ACTION_MOVE;
		}
		}
		else
		{
			m_actions |= ACTION_MOVE;
		}

		if(CheckMoveButtonStateChanged(eMBM_Right, activationMode))
		{
			ApplyMovement(Vec3(value*2.0f - 1.0f,0,0));
			AdjustMoveButtonState(eMBM_Right, activationMode);
		}
	}
	m_bUseXIInput = false;
	
	if (bClearDeltaMovement)
	{
		ClearDeltaMovement();
	}

	return false;
}

bool CPlayerInput::OnActionRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_deltaRotation.z -= value;

	m_isAimingWithMouse = true;

	return false;
}

bool CPlayerInput::OnActionRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(g_pGameCVars->cl_invertMouse)
	{
		value *= -1.0f;
	}

	// As the HMD control method is currently differential, there is very high 
	// potential to have the view going out of sync with rotation if we allow
	// both, especially due to the limitation on +-90 degrees.
	// Therefore, when using Head Mounted Devices, we may want to disable mouse Pitch.
	//if (!m_isAimingWithHMD)
	{
		m_deltaRotation.x -= value;
	}

	m_isAimingWithMouse = true;

	return false;
}

bool CPlayerInput::OnActionHMDRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_HMD_deltaRotation.x -= value;

	m_isAimingWithHMD=true;

	return false;
}

bool CPlayerInput::OnActionHMDRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_HMD_deltaRotation.z -= value;

	m_isAimingWithHMD=true;

	return false;
}

bool CPlayerInput::OnActionHMDRotateRoll(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	// Roll doesn't seem to be applied by the player control. 
	// The information is passed, though.
	m_HMD_deltaRotation.y -= value;

	m_isAimingWithHMD=true;

	return false;
}

bool CPlayerInput::OnActionVRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_deltaRotation.z -= value;

	return false;
}

bool CPlayerInput::OnActionVRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_deltaRotation.x -= value;
	if(g_pGameCVars->cl_invertMouse)
		m_deltaRotation.x*=-1.0f;

	return false;
}

bool CPlayerInput::OnActionJump(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	bool canJump = (!m_pPlayer->GetLinkedVehicle()) && ((m_pPlayer->IsSwimming() || (m_pPlayer->TrySetStance(STANCE_STAND) && (m_pPlayer->m_stats.onGround > 0.01f))));

	m_pPlayer->m_jumpButtonIsPressed = (activationMode == eAAM_OnPress);

	if (!CallTopCancelHandler(kCancelPressFlag_jump) && CanMove() && canJump)
	{
		if (value > 0.0f)
		{
			if(m_actions & ACTION_CROUCH)
			{
				CCCPOINT(PlayerMovement_PressJumpWhileCrouchedToStandUp);
				m_actions &= ~ACTION_CROUCH;
				if (!m_pPlayer->IsSliding())
				{
					return false;
				}
				else
				{
					m_pPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_FORCEEXITSLIDE );
				}
			}

			return true;
		}
	}
	else
	{
		CCCPOINT_IF(value > 0.0f, PlayerMovement_PressJumpInputIgnored);
	}

	if (canJump)
	{
		PerformJump();
	}

	CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_INPUT_CLIENT);

	return false;
}

bool CPlayerInput::PerformJump()
{
	if (!CanMove() || (m_pPlayer->GetSpectatorMode() != CActor::eASM_None))
	{
		CryLog("			> either can't move or is spectating, aborting.");
		return false;
	}

	m_actions |= ACTION_JUMP;

	// Record 'Jump' telemetry stats.

	CStatsRecordingMgr::TryTrackEvent(m_pPlayer, eGSE_Jump);

	return true;
}

bool CPlayerInput::CallTopCancelHandler(TCancelButtonBitfield cancelButtonPressed)
{
	for (int i = 0; i < kCHS_num; ++ i)
	{
		if (m_inputCancelHandler[i] && m_inputCancelHandler[i]->HandleCancelInput(*m_pPlayer, cancelButtonPressed))
		{
			return true;
		}
	}

	return false;
}

bool CPlayerInput::CallAllCancelHandlers()
{
	bool reply = false;

	for (int i = 0; i < kCHS_num; ++ i)
	{
		if (m_inputCancelHandler[i] && m_inputCancelHandler[i]->HandleCancelInput(*m_pPlayer, kCancelPressFlag_forceAll))
		{
			reply = true;
		}
	}

	return reply;
}

void CPlayerInput::RemoveInputCancelHandler (IPlayerInputCancelHandler * handler)
{
	assert (handler);

	for (int i = 0; i < kCHS_num; ++ i)
	{
		if (m_inputCancelHandler[i] == handler)
		{
			m_inputCancelHandler[i] = NULL;
			return;
		}
	}

#ifndef _RELEASE
	GameWarning ("%s trying to remove input cancel handler %p \"%s\" but it's not installed", m_pPlayer->GetEntity()->GetEntityTextDescription().c_str(), handler, handler->DbgGetCancelHandlerName().c_str());
#endif
}

void CPlayerInput::AddInputCancelHandler (IPlayerInputCancelHandler * handler, ECancelHandlerSlot slot)
{
	assert (handler);
	assert (slot >= 0 && slot < kCHS_num);

#ifndef _RELEASE
	CRY_ASSERT_TRACE (m_inputCancelHandler[slot] == NULL, ("%s already has an input cancel handler \"%s\" installed in slot %d - can't add \"%s\" too", m_pPlayer->GetEntity()->GetEntityTextDescription().c_str(), m_inputCancelHandler[slot]->DbgGetCancelHandlerName().c_str(), slot, handler->DbgGetCancelHandlerName().c_str()));
#endif

	m_inputCancelHandler[slot] = handler;
}

bool CPlayerInput::OnActionCrouch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (CallTopCancelHandler(kCancelPressFlag_crouchOrProne))
	{	
		return false;
	}

	uint32 oldCrouchAction = (m_actions & ACTION_CROUCH);

	m_crouchButtonDown = (activationMode == eAAM_OnPress);

	if (CanCrouch())
	{
		if (g_pGameCVars->cl_crouchToggle)
		{
			if (value > 0.0f)
			{
				if (!(m_actions & ACTION_CROUCH))
					m_actions |= ACTION_CROUCH;
				else
					m_actions &= ~ACTION_CROUCH;
			}
		}
		else
		{
			if (value > 0.0f)
			{
				m_actions |= ACTION_CROUCH;
			}
			else
			{
				m_actions &= ~ACTION_CROUCH;
			}
		}
	}

	// Record 'Crouch' telemetry stats.

	if(oldCrouchAction != (m_actions & ACTION_CROUCH))
	{
		CStatsRecordingMgr::TryTrackEvent(m_pPlayer, eGSE_Crouch, (m_actions & ACTION_CROUCH) != 0);
	}

	SetSliding((m_actions&ACTION_CROUCH) ? true : false );

#if ENABLE_GAME_CODE_COVERAGE || ENABLE_SHARED_CODE_COVERAGE
	if (oldCrouchAction == (m_actions & ACTION_CROUCH))
	{
		CCCPOINT(PlayerMovement_IgnoreCrouchInput);
	}
	else if (m_actions & ACTION_CROUCH)
	{
		CCCPOINT(PlayerMovement_ToggleCrouchActionOn);
	}
	else
	{
		CCCPOINT(PlayerMovement_ToggleCrouchActionOff);
	}
#endif

	return false;
}

//------------------------------------------------------------
void CPlayerInput::SetSliding(bool set)
{
	if(set)
	{
		const SPlayerSlideControl& slideCvars = gEnv->bMultiplayer ? g_pGameCVars->pl_sliding_control_mp : g_pGameCVars->pl_sliding_control;

		const SPlayerStats& stats = *m_pPlayer->GetActorStats();
		bool canSlide =
			(m_pPlayer->IsSprinting()) &&
			!m_pPlayer->IsSwimming() &&
			(stats.pickAndThrowEntity == 0) &&
			((stats.onGround > 0.0f)) &&
			(stats.speedFlat >= slideCvars.min_speed_threshold) &&
			!m_pPlayer->IsMovementRestricted() &&
			!m_pPlayer->HasHeavyWeaponEquipped() &&
			(!gEnv->bMultiplayer || gEnv->pTimer->GetFrameStartTime().GetDifferenceInSeconds(m_nextSlideTime) >= 0.0f);

		if( canSlide )
		{
			m_pPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_SLIDE );
			m_nextSlideTime = gEnv->pTimer->GetFrameStartTime() + CTimeValue(1.2f);
		}
	}
	else if (m_pPlayer->IsSliding())
	{
		m_pPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_LAZY_EXITSLIDE );
	}
}


bool CPlayerInput::OnActionSprint(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_sprintButtonDown = (activationMode != eAAM_OnRelease);
	SInputEventData inputEventData( SInputEventData::EInputEvent_Sprint, entityId, actionId, activationMode, value );
	m_pPlayer->StateMachineHandleEventMovement( SStateEventPlayerInput( &inputEventData ) );

	return false;
}
bool CPlayerInput::OnActionSprintXI(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	return OnActionSprint( entityId, actionId, activationMode, value );
}

void CPlayerInput::ForceStopSprinting()
{
	SInputEventData inputEventData( SInputEventData::EInputEvent_Sprint, m_pPlayer->GetEntityId(), CCryName("sprint"), eAAM_OnRelease, 0.0f );
	m_pPlayer->StateMachineHandleEventMovement( SStateEventPlayerInput( &inputEventData ) );
}


bool CPlayerInput::OnActionAttackRightTrigger(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!m_pPlayer->IsDead())
	{
		if (activationMode & (eAAM_OnPress|eAAM_OnHold))
		{
			m_actions |= ACTION_FIRE;
		}
		else
		{
			m_actions &= ~ACTION_FIRE;
		}
	
		return false;
	}
	else
	{
		return true;
	}
}


bool CPlayerInput::OnActionUse(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	bool filterOut = true;
	IVehicle* pVehicle = m_pPlayer->GetLinkedVehicle();

	//FIXME:on vehicles use cannot be used
	if (pVehicle)
	{
		filterOut = false;
	}
	
	if (activationMode==eAAM_OnPress)
	{
		m_actions |= ACTION_USE;
	}
	else
	{
		m_actions &= ~ACTION_USE;
	}
	
	return filterOut;
}

bool CPlayerInput::OnActionThirdPerson(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	if (!m_pPlayer->GetSpectatorMode() && m_pPlayer->m_pGameFramework->CanCheat())
	{
		if (!m_pPlayer->GetLinkedVehicle())
			m_pPlayer->ToggleThirdPerson();
	}
	return false;
}
#ifdef INCLUDE_DEBUG_ACTIONS
bool CPlayerInput::OnActionFlyMode(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
 	if (!gEnv->pSystem->IsDevMode())
		return false;

	if (!m_pPlayer->GetSpectatorMode() && m_pPlayer->m_pGameFramework->CanCheat())
	{
		uint8 flyMode=m_pPlayer->GetFlyMode()+1;
		m_pPlayer->StateMachineHandleEventMovement( SStateEventFly( flyMode ) );
	}
	return false;
}
#endif
bool CPlayerInput::OnActionGodMode(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	if (!m_pPlayer->GetSpectatorMode() && m_pPlayer->m_pGameFramework->CanCheat())
	{
		CGodMode& godMode = CGodMode::GetInstance();

		godMode.MoveToNextState();
		godMode.RespawnIfDead(m_pPlayer);
	}

	return false;
}

bool CPlayerInput::OnActionAIDebugDraw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	if(ICVar* pCVar = gEnv->pConsole->GetCVar("ai_DebugDraw"))
	{
		pCVar->Set(pCVar->GetIVal()==0 ? 1 : 0);
		return true;
	}
	return false;
}

bool CPlayerInput::OnActionPDrawHelpers(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	if(ICVar* pCVar = gEnv->pConsole->GetCVar("p_draw_helpers"))
	{
		pCVar->Set(pCVar->GetIVal()==0 ? 1 : 0);
		return true;
	}
	return false;
}

bool CPlayerInput::OnActionDMode(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	if(ICVar* pCVar = gEnv->pConsole->GetCVar("hud_DMode"))
	{
		pCVar->Set(pCVar->GetIVal()==0 ? 1 : 0);
		return true;
	}
	return false;
}

bool CPlayerInput::OnActionRecordBookmark(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
#ifdef INCLUDE_GAME_AI_RECORDER
	if(CGameAISystem* pAiSys = g_pGame->GetGameAISystem())
	{
		pAiSys->GetGameAIRecorder().RequestAIRecordBookmark();
	}
#endif //INCLUDE_GAME_AI_RECORDER

	return false;
}

#ifdef INCLUDE_DEBUG_ACTIONS
bool CPlayerInput::OnActionMannequinDebugAI(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	gEnv->pConsole->ExecuteString("mn_debugai");
	return true;
}

bool CPlayerInput::OnActionMannequinDebugPlayer(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->pSystem->IsDevMode())
		return false;

	if(ICVar* pCVar = gEnv->pConsole->GetCVar("mn_debug"))
	{
		EntityId actorEntityId = gEnv->pGameFramework->GetClientActorId();
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(actorEntityId);

		if(pEntity)
		{
			pCVar->Set(strcmp(pCVar->GetString(),pEntity->GetName())==0 ? "" : pEntity->GetName());
			return true;
		}
		
	}
	return false;

}

bool CPlayerInput::OnActionAIDebugCenterViewAgent(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	gEnv->pConsole->ExecuteString("ai_DebugAgent centerview");
	return true;
}
#endif

bool CPlayerInput::OnActionXIRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (g_pGameCVars->cl_controllerYawSnapEnable == 0)
	{
		m_xi_deltaRotationRaw.z = value;
	}
	else
	{
		const float valueSign = (float)__fsel(value, 1.0f, -1.0f);
		const float valueAbs  = value * valueSign;

		if (valueAbs >= g_pGameCVars->cl_controllerYawSnapMax)
		{
			const float snapAngle = DEG2RAD(g_pGameCVars->cl_controllerYawSnapAngle);
			const float minAngleThreshold = DEG2RAD(5.0f);

			if (!m_bYawSnappingActive && (fabsf(m_pendingYawSnapAngle) < minAngleThreshold))
			{
				m_bYawSnappingActive = true;
				m_pendingYawSnapAngle -= (snapAngle * valueSign);
			}
		}
		else if (valueAbs <= g_pGameCVars->cl_controllerYawSnapMin)
		{
			m_bYawSnappingActive = false;
		}
	}
	
	//NOTE: Controller mapping no longer carried out here. Speak to Richard Semmens
		
	m_isAimingWithMouse = false;

	// For now assuming the same fix is needed for HMDs.
	m_isAimingWithHMD = false;

	return false;
}

bool CPlayerInput::OnActionXIRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_xi_deltaRotationRaw.x = value;

	//NOTE: Controller mapping no longer carried out here. Speak to Richard Semmens

	// For now assuming the same fix is needed for HMDs.
	m_isAimingWithHMD = false;

	m_isAimingWithMouse = false;

	return false;
}

bool CPlayerInput::OnActionXIMoveX(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (CanMove())
	{
		m_xi_deltaMovement.x = value;
		if(fabsf(value)>0.001f && !m_bUseXIInput)
		{
			m_bUseXIInput = true;
		}
		else if(fabsf(value)<=0.001f && m_bUseXIInput && fabsf(m_xi_deltaMovement.y)<=0.001f)
		{
			m_bUseXIInput = false;
			if (!GetMoveButtonsState())
				m_actions &= ~ACTION_MOVE;

			ClearDeltaMovement();
		}
	}
	return false;
}

bool CPlayerInput::OnActionXIMoveY(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (CanMove())
	{
		m_xi_deltaMovement.y = value;
		if(fabsf(value)>0.001f && !m_bUseXIInput)
		{
			m_bUseXIInput = true;
		}
		else if(fabsf(value)<=0.001f && m_bUseXIInput && fabsf(m_xi_deltaMovement.x)<=0.001f)
		{
			m_bUseXIInput = false;
			if (!GetMoveButtonsState())
				m_actions &= ~ACTION_MOVE;

			ClearDeltaMovement();
		}
	}

	return false;
}


void CPlayerInput::ClearXIMovement()
{
	m_xi_deltaRotationRaw.Set(0.0f, 0.0f, 0.0f);
	m_xi_deltaRotation.Set(0.f,0.f,0.f);
	m_xi_deltaMovement.zero();
}

bool CPlayerInput::OnActionXIDisconnect(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_xi_deltaRotation.Set(0,0,0);
	m_xi_deltaMovement.zero();
	m_bUseXIInput = false;

	ClearDeltaMovement();

	return false;
}

bool CPlayerInput::OnActionInvertMouse(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	g_pGameCVars->cl_invertMouse = !g_pGameCVars->cl_invertMouse;

	return false;
}

bool CPlayerInput::OnActionFlyCamMoveX(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_flyCamDeltaMovement.x = value;

	if(fabs(m_flyCamDeltaMovement.x) < 0.003f)
		m_flyCamDeltaMovement.x = 0.f;//some dead point
		
	return false;
}

bool CPlayerInput::OnActionFlyCamMoveY(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_flyCamDeltaMovement.y = value;

	if(fabs(m_flyCamDeltaMovement.y) < 0.003f)
		m_flyCamDeltaMovement.y = 0.f;//some dead point
		
	return false;
}

bool CPlayerInput::OnActionFlyCamMoveUp(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_flyCamDeltaMovement.z = value;

	if(fabs(m_flyCamDeltaMovement.z) < 0.003f)
		m_flyCamDeltaMovement.z = 0.f;//some dead point
		
	return false;
}

bool CPlayerInput::OnActionFlyCamMoveDown(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_flyCamDeltaMovement.z = -value;

	if(fabs(m_flyCamDeltaMovement.z) < 0.003f)	
		m_flyCamDeltaMovement.z = 0.f;//some dead point
		
	return false;
}

bool CPlayerInput::OnActionFlyCamSpeedUp(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
	{
		g_pGameCVars->g_detachedCameraMoveSpeed = CLAMP(g_pGameCVars->g_detachedCameraMoveSpeed + 0.5f, 0.5f, 30.f);
	}

	return false;
}

bool CPlayerInput::OnActionFlyCamSpeedDown(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
	{
		g_pGameCVars->g_detachedCameraMoveSpeed = CLAMP(g_pGameCVars->g_detachedCameraMoveSpeed - 0.5f, 0.5f, 30.f);
	}

	return false;
}

bool CPlayerInput::OnActionFlyCamTurbo(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
	{
		m_flyCamTurbo=true;
	}
	else if (activationMode == eAAM_OnRelease)
	{
		m_flyCamTurbo=false;
	}
	
	return false;
}

bool CPlayerInput::OnActionFlyCamRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_flyCamDeltaRotation.z = value;

	if(fabs(m_flyCamDeltaRotation.z) < 0.003f)	
		m_flyCamDeltaRotation.z = 0.f;//some dead point
		
	return false;
}

bool CPlayerInput::OnActionFlyCamRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_flyCamDeltaRotation.x = value;

	if(fabs(m_flyCamDeltaRotation.x) < 0.003f)
		m_flyCamDeltaRotation.x = 0.f;//some dead point

	return false;
}

bool CPlayerInput::OnActionFlyCamSetPoint(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
#if FREE_CAM_SPLINE_ENABLED
	if(activationMode == eAAM_OnPress)
	{
		AddFlyCamPoint();
	}
#endif

	return false;
}

void CPlayerInput::AddFlyCamPoint()
{
	IView*  pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView();
	assert(pView);
	const SViewParams*  pViewParams = pView->GetCurrentParams();

	Vec3 curPos = pViewParams->position;
	Vec3 lookAtPos = pViewParams->position;

	Matrix34 mtx(pViewParams->rotation);
	Vec3 forward(0.f,3.f,0.f);
	forward = mtx * forward;
	lookAtPos += forward;

	AddFlyCamPoint(curPos, lookAtPos);
}

void CPlayerInput::AddFlyCamPoint(Vec3 pos, Vec3 lookAtPos)
{
#if FREE_CAM_SPLINE_ENABLED
	if (m_freeCamCurrentIndex < MAX_FREE_CAM_DATA_POINTS)
	{
		SFreeCamPointData &camData = m_freeCamData[m_freeCamCurrentIndex];
		camData.valid = true;
		camData.position = pos;
		camData.lookAtPosition = lookAtPos;

		CryLog("FREECAM Added new FreeCamPointData - index:%d pos:%f %f %f lookAt:%f %f %f", m_freeCamCurrentIndex, pos.x, pos.y, pos.z, lookAtPos.x, lookAtPos.y, lookAtPos.z);

		++m_freeCamCurrentIndex;
	}
	else
	{
		CryLog("FREECAM Have reached the maximum (%d) num of FreeCamPointData, no more can be added!", MAX_FREE_CAM_DATA_POINTS);
	}
#endif
}

void CPlayerInput::FlyCamPlay()
{
#if FREE_CAM_SPLINE_ENABLED
	m_freeCamPlaying = !m_freeCamPlaying;

	if (m_freeCamPlaying)
	{
		m_freeCamPlayTimer = 0.f;

		int num=0;

		SFreeCamPointData *camData = NULL;
		SFreeCamPointData *lastCamData = NULL;
		float totalDistance = 0.f;
		Vec3 diff;

		for (num=0; num < MAX_FREE_CAM_DATA_POINTS; ++num)
		{
			camData = &m_freeCamData[num];
			if (camData->valid)
			{
				if (lastCamData)
				{
					diff = camData->position - lastCamData->position;
					camData->distanceFromLast = diff.len();
					totalDistance += camData->distanceFromLast;
				}
			}
			else
			{
				break;
			}

			lastCamData = camData;
		}

		m_freeCamSpline.resize(num);
		m_freeCamLookAtSpline.resize(num);

		float curDistance = 0.f;

		for (int i=0; i<num; ++i)
		{
			camData = &m_freeCamData[i];

			curDistance += camData->distanceFromLast;
			float time = curDistance / totalDistance;

			m_freeCamSpline.key(i).flags = 0;
			m_freeCamSpline.time(i) = time;
			m_freeCamSpline.value(i) = camData->position;

			m_freeCamLookAtSpline.key(i).flags = 0;
			m_freeCamLookAtSpline.time(i) = time;
			m_freeCamLookAtSpline.value(i) = camData->lookAtPosition;
		}

		m_freeCamTotalPlayTime = totalDistance / g_pGameCVars->g_detachedCameraMoveSpeed;

		if (num > 0)
			CryLog("FREECAM Starting to play free cam spline numpoints:%d distance:%f totaltime:%f", num, totalDistance, m_freeCamTotalPlayTime);
		else
			CryLog("FREECAM Cannot start playing free cam spline - no points set");
	}
	else
	{
		CryLog("FREECAM Stop playing free cam spline");
	}
#endif
}

bool CPlayerInput::OnActionFlyCamPlay(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
	{
		FlyCamPlay();
	}

	return false;
}

bool CPlayerInput::OnActionFlyCamClear(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
#if FREE_CAM_SPLINE_ENABLED
	if(activationMode == eAAM_OnPress)
	{
		for (int i=0; i<MAX_FREE_CAM_DATA_POINTS; ++i)
		{
			SFreeCamPointData &camData = m_freeCamData[i];
			camData.valid = false;	
		}

		m_freeCamCurrentIndex = 0;

		m_freeCamPlayTimer = 0.f;
		m_freeCamTotalPlayTime = 0.f;

		m_freeCamSpline.empty();
		m_freeCamLookAtSpline.empty();
		
		CryLog("FREECAM Cleared data");
	}
#endif

	return false;
}

bool CPlayerInput::OnActionSpecial(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (CallTopCancelHandler())
	{
		return false;
	}

	const SInteractionInfo& interactionInfo = m_pPlayer->GetCurrentInteractionInfo();

	if (activationMode == eAAM_OnPress)
	{
		if (interactionInfo.interactionType == eInteraction_Stealthkill)
		{
			m_pPlayer->AttemptStealthKill(interactionInfo.interactiveEntityId);
		}
		else if (interactionInfo.interactionType == eInteraction_LargeObject)
		{
			if(!m_pPlayer->GetLargeObjectInteraction().IsBusy())
			{
				m_pPlayer->EnterLargeObjectInteraction(interactionInfo.interactiveEntityId); //always use power kick in MP
			}

		}
	}

	return false;
}

bool CPlayerInput::OnActionChangeFireMode(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress && !m_pPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) && !m_pPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeaponMP))
	{																					
		SHUDEvent event(eHUDEvent_ShowDpadMenu);
		event.AddData(eDPAD_Left);
		CHUDEventDispatcher::CallEvent(event);
	}

	return false;
}

bool CPlayerInput::OnActionToggleVision(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_openingVisor = false;
	return false;
}

bool CPlayerInput::OnActionMindBattle(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SInputEventData inputEventData( SInputEventData::EInputEvent_ButtonMashingSequence, entityId, actionId, activationMode, value );
	m_pPlayer->StateMachineHandleEventMovement( SStateEventPlayerInput( &inputEventData ) );

	return false;
}

bool CPlayerInput::OnActionLookAt( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
	float currentTime = gEnv->pTimer->GetCurrTime();
	if (activationMode == eAAM_OnPress)
		m_lookAtTimeStamp = currentTime;
	else if (activationMode == eAAM_OnHold)
		m_lookingAtButtonActive = currentTime > m_lookAtTimeStamp + g_pGameCVars->pl_useItemHoldTime;
	else if (activationMode == eAAM_OnRelease)
		m_lookingAtButtonActive = false;

	return false;
}

bool CPlayerInput::OnActionPrePickUpItem( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
	const SInteractionInfo& interactionInfo = m_pPlayer->GetCurrentInteractionInfo(); 
	if ((interactionInfo.interactionType == eInteraction_ExchangeItem) ||
		(interactionInfo.interactionType == eInteraction_PickupItem))
	{
		//Most probably we will end pick up this item soon, start to preload dba's ahead
		CItem* pTargetItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(interactionInfo.interactiveEntityId));
		if (pTargetItem)
		{
			pTargetItem->Prepare1pAnimationDbas();
			pTargetItem->Prepare1pChrs();
		}
	}
	else if( IInventory* pInventory = m_pPlayer->GetInventory() )
	{
		if ( CWeapon *pWeapon = m_pPlayer->GetWeapon(pInventory->GetCurrentItem()) )
		{
			const char* category = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItemCategory(pWeapon->GetEntity()->GetClass()->GetName());
			int categoryType = GetItemCategoryType(category);
			bool currentItemIsGrenade = (categoryType & DoubleTapGrenadeCategories()) != 0;
			if(!currentItemIsGrenade)
			{
				pWeapon->CancelCharge();
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPlayerInput::OnActionMoveOverlayTurnOn( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
  m_moveOverlay.isEnabled = true;
  return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPlayerInput::OnActionMoveOverlayTurnOff( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
  m_moveOverlay.isEnabled = false;
  return false;
}

//////////////////////////////////////////////////////////////////////////  
bool CPlayerInput::OnActionMoveOverlayWeight( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
  m_moveOverlay.weight = clamp_tpl(value, 0.0f, 1.0f);
  return false;
}

//////////////////////////////////////////////////////////////////////////  
bool CPlayerInput::OnActionMoveOverlayX( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
  m_moveOverlay.moveX = clamp_tpl(value, -1.0f, 1.0f);
  return false;
}

//////////////////////////////////////////////////////////////////////////  
bool CPlayerInput::OnActionMoveOverlayY( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
  m_moveOverlay.moveY = clamp_tpl(-value, -1.0f, 1.0f);
  return true;
}

bool CPlayerInput::OnActionRespawn(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (m_pPlayer->IsDead())
	{
		g_pGame->OnDeathReloadComplete();
	}

	return false;
}

bool CPlayerInput::OnActionMouseWheelClick(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
	{
		CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_ShowMouseWheel));
	}
	else if (activationMode == eAAM_OnRelease)
	{
		CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_HideMouseWheel));
	}
	return false;
}

void CPlayerInput::AdjustMoveButtonState(EMoveButtonMask buttonMask, int activationMode )
{
	if (activationMode == eAAM_OnPress || activationMode == eAAM_OnHold)
	{
		m_moveButtonState |= buttonMask;
	}
	else if (activationMode == eAAM_OnRelease)
	{
		m_moveButtonState &= ~buttonMask;
	}
}

bool CPlayerInput::CheckMoveButtonStateChanged(EMoveButtonMask buttonMask, int activationMode)
{
	bool current = (m_moveButtonState & buttonMask) != 0;

	if(activationMode == eAAM_OnRelease)
	{
		return current;
	}
	else if(activationMode == eAAM_OnPress || activationMode == eAAM_OnHold)
	{
		return !current;
	}
	return true;
}

float CPlayerInput::MapControllerValue(float value, float scale, float curve, bool inverse)
{
	// Any attempts to create an advanced analog stick value mapping function could be put here

	// After several experiments a simple pow(x, n) function seemed to work best.
	float res=scale * powf(fabs(value), curve);
	return (value >= 0.0f ? (inverse ? -1.0f : 1.0f) : (inverse ? 1.0f : -1.0f))*res;
}

void CPlayerInput::ClearAllExceptAction( uint32 actionFlags )
{
	uint32 actionsToKeep = m_actions & actionFlags;
	Reset();
	m_actions |= actionsToKeep;
}

bool CPlayerInput::OnActionSelectNextItem(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	const CGameActions& actions = g_pGame->Actions();
	bool suitVisorOn = false;
	bool allowSwitch = true;
	const float currentTime = gEnv->pTimer->GetCurrTime();
	bool inKillCam = g_pGame->GetRecordingSystem() && (g_pGame->GetRecordingSystem()->IsPlayingBack() || g_pGame->GetRecordingSystem()->IsPlaybackQueued());
	IVehicle* pVehicle = m_pPlayer->GetLinkedVehicle();

	if (m_pPlayer->GetSpectatorMode() || inKillCam)
		return false;
	
	if (pVehicle)
		return false;

	if (!IsPlayerOkToAction())
		return false;

	if (actions.toggle_weapon==actionId)
	{
		allowSwitch = AllowToggleWeapon(activationMode, currentTime);
	}
	else if (actions.handgrenade==actionId) // Keyboard only action
	{
		const bool bHasGrenades = (!gEnv->bMultiplayer || g_pGameCVars->g_enableMPDoubleTapGrenadeSwitch) && DoubleTapGrenadeAvailable();
		if (!bHasGrenades)
		{
			const float displayTime = gEnv->bMultiplayer ? g_pGame->GetUI()->GetCVars()->hud_warningDisplayTimeMP : g_pGame->GetUI()->GetCVars()->hud_warningDisplayTimeSP;
			SHUDEventWrapper::DisplayInfo( eInfo_Warning, displayTime, "@ui_no_grenades");
		}
	}

	if(allowSwitch && !CallTopCancelHandler(kCancelPressFlag_switchItem))
	{
		bool currentItemIsGrenade = false;

		if(IInventory* pInventory = m_pPlayer->GetInventory())
		{
			EntityId itemId = pInventory->GetCurrentItem();
			CWeapon *pWeapon = 0;
			if (itemId)
			{
				pWeapon = m_pPlayer->GetWeapon(itemId);
				if (pWeapon)
				{
					IEntityClass* pEntityClass = pWeapon->GetEntity()->GetClass();
					assert(pEntityClass != NULL);
					if (pEntityClass != NULL)
					{
						suitVisorOn = (pEntityClass == CItem::sBinocularsClass);

						// Support the primary secondary fast weapon switching (Doesn't wait for animations to finish)
						const char* category = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItemCategory(pEntityClass->GetName());
						int categoryType = GetItemCategoryType(category);
						if (categoryType & eICT_Primary || categoryType & eICT_Secondary) // For primary or secondary can ignore the can deselect
							allowSwitch = !pWeapon->IsMounted();
						else
							allowSwitch = pWeapon->CanDeselect() && !pWeapon->IsMounted();

						currentItemIsGrenade = (categoryType & DoubleTapGrenadeCategories()) != 0;

						if(allowSwitch && actionId == actions.toggle_weapon)
						{
							CFireMode* pFiremode = static_cast<CFireMode*>(pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()));

							if(pFiremode && pFiremode->IsEnabledByAccessory())
							{

								pWeapon->StartChangeFireMode();								
								allowSwitch = false;
							}
						}
					}
				}
			}
		}

		if (allowSwitch)
		{
			if (actions.nextitem==actionId)
				SelectNextItem(1, true, eICT_Grenade|eICT_Explosive|eICT_Primary|eICT_Secondary|eICT_Special, suitVisorOn);
			else if (actions.previtem==actionId)
				SelectNextItem(-1, true, eICT_Grenade|eICT_Explosive|eICT_Primary|eICT_Secondary|eICT_Special, suitVisorOn);
			else if (actions.handgrenade == actionId)
			{
				int category = gEnv->bMultiplayer ? eICT_Grenade|eICT_Explosive : eICT_Grenade;

				SelectNextItem(1, true, category, suitVisorOn);
			}
			else if (actions.toggle_explosive == actionId)
			{
				int primaryCategory = eICT_Primary | eICT_Secondary;
				int secondaryCategory = gEnv->bMultiplayer ? eICT_Grenade|eICT_Explosive : eICT_Explosive;

				SHUDEvent event(eHUDEvent_IsDoubleTapExplosiveSelect);
				event.AddData(SHUDEventData(false));
				CHUDEventDispatcher::CallEvent(event);
			
				ToggleItem(primaryCategory, secondaryCategory, suitVisorOn);
			}
			else if (actions.toggle_special == actionId)
			{
				int primaryCategory = eICT_Primary | eICT_Secondary;
				int secondaryCategory = eICT_Special;
				ToggleItem(primaryCategory, secondaryCategory, suitVisorOn);
			}
			else if (actions.toggle_weapon ==actionId)
			{
				float fCurrentTimeStamp = gEnv->pTimer->GetFrameStartTime().GetSeconds();

				bool bHasGrenades = DoubleTapGrenadeAvailable();

				float fTapTime = g_pGame->GetUI()->GetCVars()->hud_double_taptime;

				if (currentItemIsGrenade || !bHasGrenades)
				{
					bool doubleTapTime = fCurrentTimeStamp - m_fLastWeaponToggleTimeStamp < fTapTime;
					if(m_fLastWeaponToggleTimeStamp && !doubleTapTime)
					{
						SelectNextItem(1, true, eICT_Primary | eICT_Secondary, suitVisorOn);
						m_fLastWeaponToggleTimeStamp = 0.f;
					}
					else
					{
						m_fLastWeaponToggleTimeStamp = fCurrentTimeStamp;
					}

					if (fCurrentTimeStamp - m_fLastNoGrenadeTimeStamp < fTapTime && !bHasGrenades && (!gEnv->bMultiplayer || g_pGameCVars->g_enableMPDoubleTapGrenadeSwitch) )
					{
						const float displayTime = gEnv->bMultiplayer ? g_pGame->GetUI()->GetCVars()->hud_warningDisplayTimeMP : g_pGame->GetUI()->GetCVars()->hud_warningDisplayTimeSP;
						SHUDEventWrapper::DisplayInfo( eInfo_Warning, displayTime, "@ui_no_grenades");
						m_fLastWeaponToggleTimeStamp = 0.f;
					}
				}
				else if (fCurrentTimeStamp - m_fLastWeaponToggleTimeStamp < fTapTime)
				{
					SHUDEvent event(eHUDEvent_IsDoubleTapExplosiveSelect);
					event.AddData(SHUDEventData(true));
					CHUDEventDispatcher::CallEvent(event);

					SelectNextItem(1, true, DoubleTapGrenadeCategories(), suitVisorOn);

					SActorStats* pActorStats = m_pPlayer->GetActorStats();
					if(pActorStats)
					{
						if (CWeapon* pCurrentItem = static_cast<CWeapon*>(m_pPlayer->GetItem(pActorStats->exchangeItemStats.switchingToItemID)))
						{
							pCurrentItem->CancelCharge();
						}
					}
					
					m_fLastWeaponToggleTimeStamp = 0.f;	
				}
				else
				{
					CItem *pCurItem = (CItem*)m_pPlayer->GetCurrentItem();
					const int numOptions = m_pPlayer->GetInventory()->GetSlotCount(IInventory::eInventorySlot_Weapon);
					if (pCurItem && m_pPlayer->CanSwitchItems() && (numOptions > 1) && pCurItem->CanDeselect())
					{
						pCurItem->StartDeselection(false);
					}

					m_fLastWeaponToggleTimeStamp = fCurrentTimeStamp;
				}

				if (!bHasGrenades)
				{
					m_fLastNoGrenadeTimeStamp = fCurrentTimeStamp;
				}
			}
			else if (actions.debug==actionId)
			{
				if (g_pGame)
				{							
					if (!m_pPlayer->GetInventory()->GetItemByClass(CItem::sDebugGunClass)&& CItem::sDebugGunClass != 0)
						g_pGame->GetWeaponSystem()->DebugGun(0);				
					if (!m_pPlayer->GetInventory()->GetItemByClass(CItem::sRefWeaponClass) && CItem::sRefWeaponClass != 0)
						g_pGame->GetWeaponSystem()->RefGun(0);
				}
				int currentItemCategory = GetItemCategoryType(actionId.c_str());
				SelectNextItem(1, true, currentItemCategory, suitVisorOn);
			}
		}
	}

	return false;
}

bool CPlayerInput::OnActionQuickGrenadeThrow( EntityId entityId, const ActionId& actionId, int activationMode, float value )
{
	if (!UseQuickGrenadeThrow())
	{
		return false;
	}

	bool suitVisorOn = false;
	bool allowSwitch = true;
	const float currentTime = gEnv->pTimer->GetCurrTime();
	bool inKillCam = g_pGame->GetRecordingSystem() && (g_pGame->GetRecordingSystem()->IsPlayingBack() || g_pGame->GetRecordingSystem()->IsPlaybackQueued());
	IVehicle* pVehicle = m_pPlayer->GetLinkedVehicle();

	if (m_pPlayer->GetSpectatorMode() || inKillCam || pVehicle || !IsPlayerOkToAction())
	{
		return false;
	}

	CItem *pCurrentItem = static_cast<CItem*>(m_pPlayer->GetCurrentItem());
	bool bInvalidWeapon = pCurrentItem && (pCurrentItem->IsRippingOrRippedOff() || pCurrentItem->IsMounted() || pCurrentItem->IsHeavyWeapon());
	if (bInvalidWeapon)
	{
		return false;
	}

	if(m_pPlayer->IsInPickAndThrowMode())
	{
		return false;
	}

	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		if(IGameRulesObjectivesModule* pObjectivesModule = pGameRules->GetObjectivesModule())
		{
			if(pObjectivesModule->CheckIsPlayerEntityUsingObjective(m_pPlayer->GetEntityId()))
			{
				return false;
			}
		}
	}

	if(m_pPlayer->IsInPickAndThrowMode())
	{
		return false;
	}

	const CGameActions& actions = g_pGame->Actions();
	if (actions.grenade == actionId ||(gEnv->bMultiplayer && actions.xi_grenade == actionId))
	{
		IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
		const int grenadeCategories = eICT_Grenade;

		if (activationMode == eAAM_OnPress)
		{
			const bool bCanActivate = !m_pPlayer->IsSliding() && 
																!m_pPlayer->IsExitingSlide() &&
																!m_pPlayer->IsSwimming() &&
																!m_pPlayer->IsOnLedge();
			if(bCanActivate)
			{
				IInventory *pInventory = m_pPlayer->GetInventory();
				if (pInventory)
				{
					int numItems = pInventory->GetCount();

					for (int i = 0; i < numItems; i ++)
					{
						EntityId itemId = pInventory->GetItem(i);

						IItem* pItem = pItemSystem->GetItem(itemId);

						if (pItem && !pItem->IsSelected() && pItem->CanSelect())
						{
							const char* category = pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
							int categoryType = GetItemCategoryType(category);	
							if (categoryType & grenadeCategories)
							{
								ForceStopSprinting();
								m_pPlayer->SelectItem(pItem->GetEntityId(), true, false);
								CWeapon *pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
								if (pWeapon)
								{
									pWeapon->StartQuickGrenadeThrow();
								}
								break;
							}
							//We don't have a grenade... but we have some explosives... so make these the active item
							else if(categoryType & eICT_Explosive)
							{
								m_pPlayer->SelectItem(pItem->GetEntityId(), true, false);
								break;
							}
						}
					}
				}
			}
		}
		else if (activationMode == eAAM_OnRelease)
		{
			IItem *pItem = m_pPlayer->GetCurrentItem();
			if (pItem)
			{
				const char* category = pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
				int categoryType = GetItemCategoryType(category);	
				if (categoryType & grenadeCategories)
				{
					CWeapon *pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
					if (pWeapon)
					{
						pWeapon->StopQuickGrenadeThrow();
					}
				}
			}
		}
	}

	return false;
}

bool CPlayerInput::DoubleTapGrenadeAvailable()
{
	if(gEnv->bMultiplayer && g_pGameCVars->g_enableMPDoubleTapGrenadeSwitch == 0)
	{
		return false;
	}

	if(IInventory* pInventory = m_pPlayer->GetInventory())
	{
		IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
		int grenadeCategories = DoubleTapGrenadeCategories();

		int numItems = pInventory->GetCount();

		for(int i = 0; i < numItems; i++)
		{
			EntityId itemId = pInventory->GetItem(i);

			IItem* pItem = pItemSystem->GetItem(itemId);

			if(pItem && pItem->CanSelect())
			{
				const char* category = pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
				int categoryType = GetItemCategoryType(category);	

				if(categoryType & grenadeCategories)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CPlayerInput::UpdateWeaponToggle()
{
	if(m_fLastWeaponToggleTimeStamp)
	{
		float fCurrentTimeStamp = gEnv->pTimer->GetFrameStartTime().GetSeconds();

		float fTapTime = g_pGame->GetUI()->GetCVars()->hud_double_taptime;

		if (fCurrentTimeStamp - m_fLastWeaponToggleTimeStamp > fTapTime)
		{
			bool suitVisorOn = false;

			bool allowSwitch = true;
			if(IInventory* pInventory = m_pPlayer->GetInventory())
			{
				CItem* pCurrentItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pInventory->GetCurrentItem()));
				if(pCurrentItem)
				{
					allowSwitch = pCurrentItem->CanDeselect();
					suitVisorOn = (pCurrentItem->GetEntity()->GetClass() == CItem::sBinocularsClass);
				}
			}

			if (allowSwitch)
			{
				SelectNextItem(1, true, eICT_Primary | eICT_Secondary, suitVisorOn);
			}
			
			m_fLastWeaponToggleTimeStamp = 0.f;
		}
	}
}



void CPlayerInput::ToggleVisor()
{
	bool toggleVisor = !m_pPlayer->IsDead();

	IInventory* pInventory = m_pPlayer->GetInventory();
	if (pInventory)
	{
		EntityId itemId = pInventory->GetCurrentItem();
		if (itemId)
		{
			CWeapon* pWeapon = m_pPlayer->GetWeapon(itemId);
			toggleVisor = pWeapon ? !pWeapon->IsReloading() : toggleVisor;
		}
	}
}


void CPlayerInput::SelectNextItem( int direction, bool keepHistory, int category, bool disableVisorFirst )
{
	m_pPlayer->SelectNextItem(direction, keepHistory, category);
}

void CPlayerInput::ToggleItem(int primaryCategory, int secondaryCategory, bool disableVisorFirst)
{
	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	IItem* pCurrentItem = m_pPlayer->GetCurrentItem();
	if (pCurrentItem)
	{
		const char* name = pCurrentItem->GetEntity()->GetClass()->GetName();
		const char* nextWeaponCatStr = pItemSystem->GetItemCategory(name);
		int currentCategory = GetItemCategoryType(nextWeaponCatStr);
		int nextCategory = secondaryCategory;
		if (currentCategory == secondaryCategory)
			nextCategory = primaryCategory;
		SelectNextItem(1, true, nextCategory, disableVisorFirst);
	}
}

bool CPlayerInput::CanLookAt()
{
	const SInteractionInfo& interactionInfo = m_pPlayer->GetCurrentInteractionInfo(); 
	bool canLookAt = interactionInfo.lookAtInfo.lookAtTargetId != 0;
	return canLookAt;
}

void CPlayerInput::UpdateAutoLookAtTargetId( float frameTime )
{
  const SInteractionInfo& interactionInfo = m_pPlayer->GetCurrentInteractionInfo();
  
  if ( !(m_lookingAtButtonActive || interactionInfo.lookAtInfo.forceLookAt) )
	{
		m_isNearTheLookAtTarget = false;
		m_lookAtSmoothRate.Set(0.f, 0.f, 0.f);
		return;
	}

	if (interactionInfo.lookAtInfo.lookAtTargetId == 0)
	{
		m_lookAtSmoothRate.Set(0.f, 0.f, 0.f);
		return;
	}

	IEntity* pLookAtEntity = gEnv->pEntitySystem->GetEntity(interactionInfo.lookAtInfo.lookAtTargetId);
	if (!pLookAtEntity)
	{
		m_lookAtSmoothRate.Set(0.f, 0.f, 0.f);
		return;
	}
	
	AABB entityBbox;
	pLookAtEntity->GetWorldBounds(entityBbox);

	const Vec3 lookAtEntityPosition = (entityBbox.IsEmpty() == false) ? entityBbox.GetCenter(): pLookAtEntity->GetWorldPos();
	const Vec3 playerEyePosition = m_pPlayer->GetEntity()->GetWorldTM().TransformPoint(m_pPlayer->GetEyeOffset());

	const Quat desiredLookAtOrientation = Quat::CreateRotationVDir((lookAtEntityPosition - playerEyePosition).GetNormalizedSafe());
	const Ang3 desiredLookAtAngles(desiredLookAtOrientation);
	const Ang3 currentViewAngles(m_pPlayer->GetViewQuat());
	
	Ang3 diffAngles = desiredLookAtAngles - currentViewAngles;

	diffAngles.x = DEG2RAD(Snap_s180(RAD2DEG(diffAngles.x)));
	diffAngles.z = DEG2RAD(Snap_s180(RAD2DEG(diffAngles.z)));
	diffAngles.y = 0;

	Ang3 smoothedDiffAngles = diffAngles;
	Ang3 targetDiff(0.f, 0.f, 0.f);
	
	SmoothCD( smoothedDiffAngles, m_lookAtSmoothRate, frameTime, targetDiff, interactionInfo.lookAtInfo.interpolationTime );

	Ang3 difference = diffAngles - smoothedDiffAngles;
	difference.y = 0;
	CMovementRequest request;
	request.AddDeltaRotation( difference );
	m_pPlayer->m_pMovementController->RequestMovement( request );

	const float dist = fabs(diffAngles.x) + fabs(diffAngles.y) + fabs(diffAngles.z);
	m_isNearTheLookAtTarget = (dist < g_pGameCVars->pl_aim_near_lookat_target_distance);
}


bool CPlayerInput::IsItemPickUpScriptAction(const ActionId& actionId) const
{
	const CGameActions& actions = g_pGame->Actions();
	const ActionId* itemPickUpEvents[] =
	{
		&actions.heavyweaponremove,
		&actions.use,
		&actions.itemPickup,
		&actions.preUse,
	};
	const int numEvents = CRY_ARRAY_COUNT(itemPickUpEvents);

	for (int i = 0; i < numEvents; ++i)
		if (actionId == *itemPickUpEvents[i])
			return true;
	return false;
}



bool CPlayerInput::UseQuickGrenadeThrow()
{
	if (g_pGameCVars->g_useQuickGrenadeThrow == 1)
	{
		return true;
	}
	else if (gEnv->bMultiplayer && (g_pGameCVars->g_useQuickGrenadeThrow == 2))
	{
		return true;
	}
	return false;
}

void CPlayerInput::AddCrouchAction()
{
	if(g_pGameCVars->cl_crouchToggle)
	{
		m_actions |= ACTION_CROUCH;
	}
	else
	{
		m_actions |= ACTION_FORCE_CROUCH;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CAIInput::CAIInput( CPlayer * pPlayer ) : 
m_pPlayer(pPlayer), 
m_pStats(&pPlayer->m_stats)
{
}

CAIInput::~CAIInput()
{
}

void CAIInput::GetState( SSerializedPlayerInput& input )
{
	SMovementState movementState;
	m_pPlayer->GetMovementController()->GetMovementState( movementState );

	Quat worldRot = m_pPlayer->GetBaseQuat();
	input.stance = movementState.stance;
	input.bodystate = 0;

	IAIActor* pAIActor = CastToIAIActorSafe(m_pPlayer->GetEntity()->GetAI());
	if (pAIActor)
	{
		input.bodystate=pAIActor->GetState().bodystate;
		input.allowStrafing = pAIActor->GetState().allowStrafing;
	}

	input.deltaMovement = movementState.movementDirection.GetNormalizedSafe()*movementState.desiredSpeed;
	input.lookDirection = movementState.eyeDirection;
	input.bodyDirection = movementState.entityDirection;
	input.sprint = false;

	IAnimationGraphState *pState=0;
	if (m_pPlayer->GetAnimatedCharacter())
		pState=m_pPlayer->GetAnimatedCharacter()->GetAnimationGraphState();

	if (pState)
	{
		input.pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(input.sprint);
	}
}

void CAIInput::SetState( const SSerializedPlayerInput& input )
{
	GameWarning("CAIInput::SetState called: should never happen");
}


/*
==================================================================================================================
	SSerializedPlayerInput
==================================================================================================================
*/

#define PLAYER_INPUT_STANDING_ON_OFFSET Vec3(20.f)

static void SerialiseRelativePosition(TSerialize ser, Vec3& position, EntityId standingOn)
{
	// Convert position to local space and offset it since policy
	// doesn't allow negative numbers
	IEntity* pGroundEntity = gEnv->pEntitySystem->GetEntity(standingOn);
	if (ser.IsReading())
	{
		ser.Value( "position", position, 'wrld' );
		if (pGroundEntity)
			position -= PLAYER_INPUT_STANDING_ON_OFFSET;
	}
	else
	{
		Vec3 serPosition = position;
		if (pGroundEntity!=NULL)
			serPosition = (pGroundEntity->GetWorldTM().GetInverted() * position) + PLAYER_INPUT_STANDING_ON_OFFSET;
		ser.Value( "position", serPosition, 'wrld' );
	}
}

void SSerializedPlayerInput::Serialize( TSerialize ser, EEntityAspects aspect )
{
	if (aspect & CPlayer::ASPECT_INPUT_CLIENT)
	{
		ser.Value( "stance", stance, 'stnc' );
		ser.Value( "deltaMovement", deltaMovement, 'dMov' );
		SerializeDirHelper(ser, lookDirection, 'pYaw', 'pElv');
		ser.Value( "sprint", sprint, 'bool' );
		ser.Value( "inAir", inAir, 'bool' );
		ser.Value( "physcounter", physCounter, 'ui2');
	}
	else if (aspect & CPlayer::ASPECT_INPUT_CLIENT_AUGMENTED)
	{
		bool bIsEntity = standingOn!=0;
		ser.Value( "bIsEntity", bIsEntity, 'bool');
		ser.Value( "standingOn", standingOn, 'eid');

		// Did we get a valid entity? This is a failsafe for late joiners to get CryNetwork to send the data again!
		if (ser.IsReading() && standingOn==0 && bIsEntity)
			ser.FlagPartialRead();
	}
		
	// Always serialise the position
	SerialiseRelativePosition(ser, position, standingOn);
}
