// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Controls script variables coming from track view to add some
   control/feedback during cutscenes

   -------------------------------------------------------------------------
   History:
   - 28:04:2010   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "CinematicInput.h"
#include "Player.h"
#include "PlayerInput.h"
#include "GameActions.h"
#include <CryMovie/IMovieSystem.h>
#include <IViewSystem.h>
#include "GameCVars.h"
#include "PlayerStateEvents.h"

#include "Weapon.h"

#define CINEMATIC_INPUT_MOUSE_RECENTER_TIMEOUT 3.0f
#define CINEMATIC_INPUT_MAX_AIM_DISTANCE       250.0f
#define CINEMATIC_INPUT_MIN_AIM_DISTANCE       10.0f

CCinematicInput::CCinematicInput()
	: m_controllerAccumulatedAngles(ZERO)
	, m_cutsceneRunningCount(0)
	, m_cutscenesNoPlayerRunningCount(0)
	, m_bPlayerIsThirdPerson(false)
	, m_bPlayerWasInvisible(false)
	, m_bCutsceneDisabledUISystem(false)
	, m_iHudState(0)
	, m_aimingRayID(0)
	, m_aimingDistance(CINEMATIC_INPUT_MAX_AIM_DISTANCE)
{
#if CINEMATIC_INPUT_PC_MOUSE
	m_mouseAccumulatedAngles.Set(0.0f, 0.0f, 0.0f);
	m_mouseAccumulatedInput.Set(0.0f, 0.0f, 0.0f);
	m_mouseRecenterTimeOut = CINEMATIC_INPUT_MOUSE_RECENTER_TIMEOUT;
	m_lastUpdateWithMouse = false;
#endif
}

CCinematicInput::~CCinematicInput()
{
	//Make sure filter are disabled on level unload
	if (g_pGameActions)
	{
		g_pGameActions->FilterCutsceneNoPlayer()->Enable(false);
	}
}

void CCinematicInput::OnBeginCutScene(int cutSceneFlags)
{
	m_cutsceneRunningCount++;

	if (cutSceneFlags & IAnimSequence::eSeqFlags_NoPlayer)
	{
		if (m_cutscenesNoPlayerRunningCount == 0)
		{
			DisablePlayerForCutscenes();
		}
		m_cutscenesNoPlayerRunningCount++;
	}
	else
	{
		// if the player is visible and the scene is camera controlled, make sure we're using third person setup
		TogglePlayerThirdPerson(true);
	}

	if (m_cutsceneRunningCount == 1 && (cutSceneFlags & IAnimSequence::eSeqFlags_NoUI))
	{
		if (ICVar* pCVar = gEnv->pConsole->GetCVar("hud_hide"))
		{
			m_bCutsceneDisabledUISystem = pCVar->GetIVal() != 1;
			if (m_bCutsceneDisabledUISystem)
			{
				pCVar->Set(1);
			}
		}
	}
}

void CCinematicInput::OnEndCutScene(int cutSceneFlags)
{
	m_cutsceneRunningCount = max(m_cutsceneRunningCount - 1, 0);
	if (m_cutsceneRunningCount == 0)
	{
		ClearCutSceneVariables();
	}

	if (cutSceneFlags & IAnimSequence::eSeqFlags_NoPlayer)
	{
		m_cutscenesNoPlayerRunningCount = max(m_cutscenesNoPlayerRunningCount - 1, 0);
		if (m_cutscenesNoPlayerRunningCount == 0)
		{
			ReEnablePlayerAfterCutscenes();
		}
	}
	else
	{
		// restore the old third/first player setup after playing the camera controlled cutscene
		TogglePlayerThirdPerson(false);
	}

	if (m_bCutsceneDisabledUISystem && m_cutsceneRunningCount == 0)
	{
		m_bCutsceneDisabledUISystem = false;
		if (ICVar* pCVar = gEnv->pConsole->GetCVar("hud_hide"))
		{
			pCVar->Set(0);
		}
	}
}

void CCinematicInput::TogglePlayerThirdPerson(bool bEnable)
{
	CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if (pClientActor)
	{
		CRY_ASSERT(pClientActor->GetActorClass() == CPlayer::GetActorClassType());

		CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
		pClientPlayer->SetThirdPerson((bEnable && !pClientPlayer->IsThirdPerson()));
	}
}

void CCinematicInput::Update(float frameTime)
{
	IScriptSystem* pScriptSystem = gEnv->pScriptSystem;
	if (pScriptSystem)
	{
		UpdateForceFeedback(pScriptSystem, frameTime);
		UpdateAdditiveCameraInput(pScriptSystem, frameTime);
	}

	UpdateWeapons();
}

void CCinematicInput::OnAction(const EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	const CGameActions& gameActions = g_pGame->Actions();

	if (actionId == gameActions.attack1_cine)
	{
		CWeapon* pPrimaryWeapon = GetWeapon(eWeapon_Primary);
		if (pPrimaryWeapon != NULL)
		{
			pPrimaryWeapon->OnAction(actorId, actionId, activationMode, value);
		}
	}
	else if (actionId == gameActions.attack2_cine)
	{
		CWeapon* pSecondaryWeapon = GetWeapon(eWeapon_Secondary);
		if (pSecondaryWeapon != NULL)
		{
			pSecondaryWeapon->OnAction(actorId, actionId, activationMode, value);
		}
	}
	else if (actionId == gameActions.skip_cutscene)
	{
		const int playingSequences = gEnv->pMovieSystem->GetNumPlayingSequences();
		for (int i = 0; i < playingSequences; ++i)
		{
			IAnimSequence* pSeq = gEnv->pMovieSystem->GetPlayingSequence(i);
			if (pSeq && pSeq->GetFlags() & IAnimSequence::eSeqFlags_CutScene)
			{
				if (!(pSeq->GetFlags() & IAnimSequence::eSeqFlags_NoAbort))
				{
					gEnv->pMovieSystem->SetPlayingTime(pSeq, pSeq->GetTimeRange().end);
				}
			}
		}
	}
}

void CCinematicInput::OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result)
{
	assert(rayID == m_aimingRayID);

	m_aimingRayID = 0;

	m_aimingDistance = (result.hitCount > 0) ? max(result.hits[0].dist, CINEMATIC_INPUT_MIN_AIM_DISTANCE) : CINEMATIC_INPUT_MAX_AIM_DISTANCE;
}

void CCinematicInput::SetUpWeapon(const CCinematicInput::Weapon& weaponClass, const IEntity* pEntity)
{
	assert((weaponClass >= 0) && (weaponClass < eWeapon_ClassCount));

	if (pEntity != NULL)
	{
		m_weapons[weaponClass].m_weaponId = pEntity->GetId();

		IEntity* pParent = pEntity->GetParent();
		m_weapons[weaponClass].m_parentId = (pParent != NULL) ? pParent->GetId() : 0;
	}
	else
	{
		m_weapons[weaponClass].m_weaponId = 0;
		m_weapons[weaponClass].m_parentId = 0;
	}
}

void CCinematicInput::ClearCutSceneVariables()
{
	IScriptSystem* pScriptSystem = gEnv->pScriptSystem;
	if (pScriptSystem)
	{
		pScriptSystem->SetGlobalValue("Cinematic_RumbleA", 0.0f);
		pScriptSystem->SetGlobalValue("Cinematic_RumbleB", 0.0f);

		pScriptSystem->SetGlobalValue("Cinematic_CameraLookUp", 0.0f);
		pScriptSystem->SetGlobalValue("Cinematic_CameraLookDown", 0.0f);
		pScriptSystem->SetGlobalValue("Cinematic_CameraLookLeft", 0.0f);
		pScriptSystem->SetGlobalValue("Cinematic_CameraLookRight", 0.0f);

		pScriptSystem->SetGlobalValue("Cinematic_CameraDoNotCenter", 0.0f);
	}

#if CINEMATIC_INPUT_PC_MOUSE
	m_mouseAccumulatedAngles.Set(0.0f, 0.0f, 0.0f);
	m_mouseAccumulatedInput.Set(0.0f, 0.0f, 0.0f);
	m_lastUpdateWithMouse = false;
#endif

	m_controllerAccumulatedAngles.Set(0.0f, 0.0f, 0.0f);
}

void CCinematicInput::UpdateForceFeedback(IScriptSystem* pScriptSystem, float frameTime)
{
	float rumbleA = 0.0f, rumbleB = 0.0f;

	pScriptSystem->GetGlobalValue("Cinematic_RumbleA", rumbleA);
	pScriptSystem->GetGlobalValue("Cinematic_RumbleB", rumbleB);

	if ((rumbleA + rumbleB) > 0.0f)
	{
		g_pGame->GetIGameFramework()->GetIForceFeedbackSystem()->AddFrameCustomForceFeedback(rumbleA, rumbleB);
	}
}

void CCinematicInput::UpdateAdditiveCameraInput(IScriptSystem* pScriptSystem, float frameTime)
{
	CCinematicInput::SUpdateContext updateCtx;

	pScriptSystem->GetGlobalValue("Cinematic_CameraLookUp", updateCtx.m_lookUpLimit);
	pScriptSystem->GetGlobalValue("Cinematic_CameraLookDown", updateCtx.m_lookDownLimit);
	pScriptSystem->GetGlobalValue("Cinematic_CameraLookLeft", updateCtx.m_lookLeftLimit);
	pScriptSystem->GetGlobalValue("Cinematic_CameraLookRight", updateCtx.m_lookRightLimit);
	float recenterCamera = 0.0f;
	pScriptSystem->GetGlobalValue("Cinematic_CameraDoNotCenter", recenterCamera);

	updateCtx.m_lookUpLimit = DEG2RAD(updateCtx.m_lookUpLimit);
	updateCtx.m_lookDownLimit = DEG2RAD(updateCtx.m_lookDownLimit);
	updateCtx.m_lookLeftLimit = DEG2RAD(updateCtx.m_lookLeftLimit);
	updateCtx.m_lookRightLimit = DEG2RAD(updateCtx.m_lookRightLimit);
	updateCtx.m_recenter = (recenterCamera < 0.01f);

	updateCtx.m_frameTime = frameTime;

	CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if (pClientActor)
	{
		CRY_ASSERT(pClientActor->GetActorClass() == CPlayer::GetActorClassType());
		CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);

		IPlayerInput* pIPlayerInput = pClientPlayer->GetPlayerInput();
		if (pIPlayerInput && pIPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT)
		{
			CPlayerInput* pPlayerInput = static_cast<CPlayerInput*>(pIPlayerInput);

			Ang3 frameAccumulatedAngles(0.0f, 0.0f, 0.0f);

#if CINEMATIC_INPUT_PC_MOUSE
			const bool isAimingWithMouse = pPlayerInput->IsAimingWithMouse();

			RefreshInputMethod(isAimingWithMouse);

			if (isAimingWithMouse)
			{
				frameAccumulatedAngles = UpdateAdditiveCameraInputWithMouse(updateCtx, pPlayerInput->GetRawMouseInput());
			}
			else
			{
				frameAccumulatedAngles = UpdateAdditiveCameraInputWithController(updateCtx, pPlayerInput->GetRawControllerInput());
			}
#else
			frameAccumulatedAngles = UpdateAdditiveCameraInputWithController(updateCtx, pPlayerInput->GetRawControllerInput());
#endif

			IView* pActiveView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView();
			if (pActiveView)
			{
				pActiveView->SetFrameAdditiveCameraAngles(frameAccumulatedAngles);
			}
		}
	}
}

void CCinematicInput::UpdateWeapons()
{
	CWeapon* pPrimaryWeapon = GetWeapon(eWeapon_Primary);
	CWeapon* pSecondaryWeapon = GetWeapon(eWeapon_Secondary);

	const bool doUpdate = (pPrimaryWeapon != NULL) || (pSecondaryWeapon != NULL);
	if (doUpdate)
	{
		const CCamera& camera = gEnv->pSystem->GetViewCamera();

		const Vec3 viewPosition = camera.GetPosition();
		const Vec3 viewDirection = camera.GetViewdir();

		// Update raycast
		if (m_aimingRayID == 0)
		{
			IEntity* pIgnoredEntity = gEnv->pEntitySystem->GetEntity(m_weapons[eWeapon_Primary].m_parentId);
			IEntity* pIgnoredEntity2 = gEnv->pEntitySystem->GetEntity(m_weapons[eWeapon_Secondary].m_parentId);
			int ignoreCount = 0;
			IPhysicalEntity* pIgnoredEntityPhysics[2] = { NULL, NULL };
			if (pIgnoredEntity)
			{
				pIgnoredEntityPhysics[ignoreCount] = pIgnoredEntity->GetPhysics();
				ignoreCount += pIgnoredEntityPhysics[ignoreCount] ? 1 : 0;
			}
			if (pIgnoredEntity2 && (pIgnoredEntity2 != pIgnoredEntity))
			{
				pIgnoredEntityPhysics[ignoreCount] = pIgnoredEntity2->GetPhysics();
				ignoreCount += pIgnoredEntityPhysics[ignoreCount] ? 1 : 0;
			}

			m_aimingRayID = g_pGame->GetRayCaster().Queue(
			  RayCastRequest::HighestPriority,
			  RayCastRequest(viewPosition, viewDirection * CINEMATIC_INPUT_MAX_AIM_DISTANCE,
			                 ent_all | ent_water,
			                 rwi_stop_at_pierceable | rwi_ignore_back_faces,
			                 pIgnoredEntityPhysics,
			                 ignoreCount),
			  functor(*this, &CCinematicInput::OnRayCastDataReceived));
		}

		// Update weapon orientation
		const Vec3 aimTargetPosition = viewPosition + (viewDirection * m_aimingDistance);
		if (pPrimaryWeapon != NULL)
		{
			UpdateWeaponOrientation(pPrimaryWeapon->GetEntity(), aimTargetPosition);
		}

		if (pSecondaryWeapon != NULL)
		{
			UpdateWeaponOrientation(pSecondaryWeapon->GetEntity(), aimTargetPosition);
		}
	}
}

void CCinematicInput::UpdateWeaponOrientation(IEntity* pWeaponEntity, const Vec3& targetPosition)
{
	assert(pWeaponEntity != NULL);
	const Vec3 weaponPosition = pWeaponEntity->GetWorldPos();
	const Vec3 desiredAimDirection = (targetPosition - weaponPosition).GetNormalized();

	Matrix34 newWorldTM(Quat::CreateRotationVDir(desiredAimDirection));
	newWorldTM.SetTranslation(weaponPosition);

	pWeaponEntity->SetWorldTM(newWorldTM);
}

void CCinematicInput::ReEnablePlayerAfterCutscenes()
{
	CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if (pClientActor)
	{
		CRY_ASSERT(pClientActor->GetActorClass() == CPlayer::GetActorClassType());
		CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
		IEntity* pPlayerEntity = pClientPlayer->GetEntity();

		if (!m_bPlayerWasInvisible && pPlayerEntity->IsInvisible())
		{
			pPlayerEntity->Invisible(false);
		}

		pClientPlayer->StateMachineHandleEventMovement(SStateEventCutScene(false));
	}

	g_pGameActions->FilterCutsceneNoPlayer()->Enable(false);

	g_pGame->GetIGameFramework()->GetIActionMapManager()->EnableActionMap("player_cine", false);
}

CWeapon* CCinematicInput::GetWeapon(const CCinematicInput::Weapon& weaponClass) const
{
	assert((weaponClass >= 0) && (weaponClass < eWeapon_ClassCount));

	if (m_weapons[weaponClass].m_weaponId != 0)
	{
		IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_weapons[weaponClass].m_weaponId);
		return (pItem != NULL) ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;
	}

	return NULL;
}

void CCinematicInput::DisablePlayerForCutscenes()
{
	CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if (pClientActor)
	{
		CRY_ASSERT(pClientActor->GetActorClass() == CPlayer::GetActorClassType());
		CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
		IEntity* pPlayerEntity = pClientPlayer->GetEntity();

		m_bPlayerWasInvisible = pPlayerEntity->IsInvisible();
		if (!m_bPlayerWasInvisible)
		{
			pPlayerEntity->Invisible(true);
		}

		pClientPlayer->StateMachineHandleEventMovement(SStateEventCutScene(true));
	}

	g_pGameActions->FilterCutsceneNoPlayer()->Enable(true);

	g_pGame->GetIGameFramework()->GetIActionMapManager()->EnableActionMap("player_cine", true);
}

Ang3 CCinematicInput::UpdateAdditiveCameraInputWithMouse(const SUpdateContext& updateCtx, const Ang3& rawMouseInput)
{
#if CINEMATIC_INPUT_PC_MOUSE
	Ang3 rawMouseInputModified = rawMouseInput * updateCtx.m_frameTime * updateCtx.m_frameTime;
	rawMouseInputModified.z = -rawMouseInputModified.z;
	rawMouseInputModified.x *= (g_pGameCVars->cl_invertMouse == 0) ? 1.0f : -1.0f;

	m_mouseAccumulatedInput += rawMouseInputModified;
	m_mouseAccumulatedInput.x = clamp_tpl(m_mouseAccumulatedInput.x, -1.0f, 1.0f);
	m_mouseAccumulatedInput.z = clamp_tpl(m_mouseAccumulatedInput.z, -1.0f, 1.0f);
	m_mouseAccumulatedInput.y = 0.0f;

	//Yaw angle (Z axis)
	m_mouseAccumulatedAngles.z = -(float)__fsel(m_mouseAccumulatedInput.z, m_mouseAccumulatedInput.z * updateCtx.m_lookRightLimit, m_mouseAccumulatedInput.z * updateCtx.m_lookLeftLimit);

	//Pitch angle (X axis)
	m_mouseAccumulatedAngles.x = (float)__fsel(m_mouseAccumulatedInput.x, m_mouseAccumulatedInput.x * updateCtx.m_lookUpLimit, m_mouseAccumulatedInput.x * updateCtx.m_lookDownLimit);

	// Recenter after a certain amount of time without input
	if (updateCtx.m_recenter)
	{
		const float rawInputLen = fabs(rawMouseInputModified.x) + fabs(rawMouseInputModified.z);
		const float newRecenterTimeOut = (float)__fsel(-rawInputLen, m_mouseRecenterTimeOut - updateCtx.m_frameTime, CINEMATIC_INPUT_MOUSE_RECENTER_TIMEOUT);
		if (newRecenterTimeOut < 0.0f)
		{
			Interpolate(m_mouseAccumulatedInput, Ang3(0.0f, 0.0f, 0.0f), 1.5f, updateCtx.m_frameTime);
		}

		m_mouseRecenterTimeOut = max(newRecenterTimeOut, 0.0f);
	}

	return m_mouseAccumulatedAngles;
#else
	return Ang3(0.0f, 0.0f, 0.0f);
#endif

}

Ang3 CCinematicInput::UpdateAdditiveCameraInputWithController(const SUpdateContext& updateCtx, const Ang3& rawControllerInput)
{
	if (updateCtx.m_recenter)
	{
		Ang3 finalControllerAnglesLimited = rawControllerInput;

		//Yaw angle (Z axis)
		finalControllerAnglesLimited.z = -clamp_tpl((float)__fsel(finalControllerAnglesLimited.z, finalControllerAnglesLimited.z * updateCtx.m_lookRightLimit, finalControllerAnglesLimited.z * updateCtx.m_lookLeftLimit), -updateCtx.m_lookLeftLimit, updateCtx.m_lookRightLimit);

		//Pitch angle (X axis)
		finalControllerAnglesLimited.x *= (g_pGameCVars->cl_invertController == 0) ? 1.0f : -1.0f;
		finalControllerAnglesLimited.x = clamp_tpl((float)__fsel(finalControllerAnglesLimited.x, finalControllerAnglesLimited.x * updateCtx.m_lookUpLimit, finalControllerAnglesLimited.x * updateCtx.m_lookDownLimit), -updateCtx.m_lookDownLimit, updateCtx.m_lookUpLimit);

		//No roll allowed
		finalControllerAnglesLimited.y = 0.0f;

		Interpolate(m_controllerAccumulatedAngles, finalControllerAnglesLimited, 2.5f, updateCtx.m_frameTime);
	}
	else
	{
		Ang3 finalControllerAnglesLimited = m_controllerAccumulatedAngles;

		finalControllerAnglesLimited.x += ((rawControllerInput.x * updateCtx.m_frameTime * 1.5f) * ((g_pGameCVars->cl_invertController == 0) ? 1.0f : -1.0f));
		finalControllerAnglesLimited.z -= (rawControllerInput.z * updateCtx.m_frameTime * 1.5f);

		finalControllerAnglesLimited.x = clamp_tpl(finalControllerAnglesLimited.x, -updateCtx.m_lookDownLimit, updateCtx.m_lookUpLimit);
		finalControllerAnglesLimited.z = clamp_tpl(finalControllerAnglesLimited.z, -updateCtx.m_lookLeftLimit, updateCtx.m_lookRightLimit);

		finalControllerAnglesLimited.y = 0.0f;

		m_controllerAccumulatedAngles = finalControllerAnglesLimited;
	}

	return m_controllerAccumulatedAngles;
}

void CCinematicInput::RefreshInputMethod(const bool isMouseInput)
{
#if CINEMATIC_INPUT_PC_MOUSE
	if (isMouseInput == m_lastUpdateWithMouse)
		return;

	m_lastUpdateWithMouse = isMouseInput;

	if (m_lastUpdateWithMouse)
	{
		m_mouseAccumulatedAngles = m_controllerAccumulatedAngles;
		m_controllerAccumulatedAngles.Set(0.0f, 0.0f, 0.0f);
	}
	else
	{
		m_controllerAccumulatedAngles = m_mouseAccumulatedAngles;
		m_mouseAccumulatedAngles.Set(0.0f, 0.0f, 0.0f);
	}

	m_mouseAccumulatedInput.Set(0.0f, 0.0f, 0.0f);
	m_mouseRecenterTimeOut = CINEMATIC_INPUT_MOUSE_RECENTER_TIMEOUT;
#endif
}
