// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controls player camera update (Refactored from PlayerView)

-------------------------------------------------------------------------
History:
- 15:10:2009   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "PlayerCamera.h"
#include "Player.h"
#include <IViewSystem.h>
#include "GameCVars.h"
#include "Utility/CryWatch.h"
#include "CameraModes.h"
#include "HitDeathReactions.h"

#if !defined(_RELEASE)

#define PLAYER_CAMERA_WATCH(...)  do { if (g_pGameCVars->pl_debug_watch_camera_mode)       CryWatch    ("[PlayerCamera] " __VA_ARGS__); } while(0)
#define PLAYER_CAMERA_LOG(...)    do { if (g_pGameCVars->pl_debug_log_camera_mode_changes) CryLogAlways("[PlayerCamera] " __VA_ARGS__); } while(0)

static AUTOENUM_BUILDNAMEARRAY(s_cameraModeNames, CameraModeList);

#else

#define PLAYER_CAMERA_WATCH(...)  (void)(0)
#define PLAYER_CAMERA_LOG(...)    (void)(0)

#endif

#define CAMERA_NEAR_PLANE_DISTANCE 0.0f  //Use Engine defaults

CPlayerCamera::CPlayerCamera(const CPlayer & ownerPlayer) :
	m_ownerPlayer(ownerPlayer),
	m_transitionTime(0.0f),
	m_totalTransitionTime(0.0f),
	m_transitionCameraMode(eCameraMode_Invalid),
	m_enteredPartialAnimControlledCameraOnLedge(false)
{
	m_previousCameraMode = eCameraMode_Invalid;
	m_currentCameraMode = eCameraMode_Default;

	memset (m_cameraModes, 0, sizeof(m_cameraModes));

	m_cameraModes[eCameraMode_Default]             = new CDefaultCameraMode();
	m_cameraModes[eCameraMode_SpectatorFollow]     = new CSpectatorFollowCameraMode();
	m_cameraModes[eCameraMode_SpectatorFixed]      = new CSpectatorFixedCameraMode();
	m_cameraModes[eCameraMode_AnimationControlled] = new CAnimationControlledCameraMode();
	m_cameraModes[eCameraMode_Death]               = new CDeathCameraMode();
	m_cameraModes[eCameraMode_Death_SinglePlayer]  = new CDeathCameraModeSinglePlayer();
	m_cameraModes[eCameraMode_Vehicle]             = new CVehicleCameraMode();
	m_cameraModes[eCameraMode_PartialAnimationControlled]					 = new CPartialAnimationControlledCameraMode();

	PLAYER_CAMERA_LOG ("Creating PlayerCamera instance %p", this);
}

CPlayerCamera::~CPlayerCamera()
{
	PLAYER_CAMERA_LOG ("Destroying PlayerCamera instance %p", this);
	for (unsigned int i = 0; i < eCameraMode_Last; ++i)
	{
		PLAYER_CAMERA_LOG("  Deleting camera class '%s' ptr=%p", s_cameraModeNames[i], m_cameraModes[i]);
		SAFE_DELETE(m_cameraModes[i]);
	}
}

void CPlayerCamera::SetCameraMode(ECameraMode newMode, const char * why)
{
	PLAYER_CAMERA_LOG ("%s '%s' changing camera mode from %u (%s) to %u (%s): %s", m_ownerPlayer.GetEntity()->GetClass()->GetName(), m_ownerPlayer.GetEntity()->GetName(), m_currentCameraMode, s_cameraModeNames[m_currentCameraMode], newMode, s_cameraModeNames[newMode], why);
	m_currentCameraMode = newMode;
}

void CPlayerCamera::SetCameraModeWithAnimationBlendFactors( ECameraMode newMode, const ICameraMode::AnimationSettings& animationSettings, const char* why )
{
	SetCameraMode( newMode, why );

	m_cameraModes[newMode]->SetCameraAnimationFactor( animationSettings );
}

#if !defined(_RELEASE)
static bool CameraFollowEntity(SViewParams& viewParams)
{
	if (g_pGameCVars->pl_viewFollow && g_pGameCVars->pl_viewFollow[0])
	{
		if (IEntity* pEntityFollow = gEnv->pEntitySystem->FindEntityByName(g_pGameCVars->pl_viewFollow))
		{
			Vec3 targetPos = pEntityFollow->GetWorldPos();
			targetPos.z += g_pGameCVars->pl_viewFollowOffset;
			Vec3 dir = (targetPos - viewParams.position);
			float radius = dir.GetLength();
			if (radius > 0.01f)
			{
				dir = dir * (1.f/radius);
				viewParams.rotation = Quat::CreateRotationVDir(dir);
				dir.z = 0.f;
				viewParams.position = targetPos - dir * clamp_tpl(radius, g_pGameCVars->pl_viewFollowMinRadius, g_pGameCVars->pl_viewFollowMaxRadius);
			}
			return true;
		}
	}
	return false;
}
#endif

bool CPlayerCamera::Update(SViewParams& viewParams, float frameTime )
{
#if !defined(_RELEASE)
	if (CameraFollowEntity(viewParams))
		return false;
#endif

	if (g_pGameCVars->g_detachCamera == 1)
		return false;

	ECameraMode currentCameraMode = GetCurrentCameraMode();
	assert((currentCameraMode >= eCameraMode_Default) && (currentCameraMode < eCameraMode_Last));
	
	if (m_currentCameraMode != currentCameraMode)
	{
		SetCameraMode(currentCameraMode, "in update");
	}

	if (m_previousCameraMode != m_currentCameraMode)
	{
		assert(m_previousCameraMode >= eCameraMode_Invalid && m_previousCameraMode < eCameraMode_Last);
		assert(m_currentCameraMode >= 0 && m_currentCameraMode < eCameraMode_Last);
		if (m_previousCameraMode != eCameraMode_Invalid)
		{
			m_cameraModes[m_previousCameraMode]->DeactivateMode(m_ownerPlayer);

			if (m_cameraModes[m_previousCameraMode]->CanTransition() && m_cameraModes[m_currentCameraMode]->CanTransition())
			{
				UpdateTotalTransitionTime();

				if (m_transitionCameraMode != m_currentCameraMode)
				{
					m_transitionTime			 = m_totalTransitionTime;
				}
				else
				{
					//--- We are reversing a transition, reverse the time & carry on running
					m_transitionTime = m_totalTransitionTime - m_transitionTime;
				}
				m_transitionCameraMode = m_previousCameraMode;
			}
			else
			{
				m_totalTransitionTime  = 0.0f;
				m_transitionTime			 = 0.0f;
				m_transitionCameraMode = eCameraMode_Invalid;
			}
		}
		m_cameraModes[m_currentCameraMode]->ActivateMode(m_ownerPlayer);
		m_previousCameraMode = m_currentCameraMode;
	}

	PLAYER_CAMERA_WATCH ("%s '%s' current camera mode is %d: %s", m_ownerPlayer.GetEntity()->GetClass()->GetName(), m_ownerPlayer.GetEntity()->GetName(), currentCameraMode, s_cameraModeNames[currentCameraMode]);

	viewParams.viewID = 0;
	UpdateCommon(viewParams, frameTime);
	
	assert(currentCameraMode >= 0);
	assert (m_cameraModes[currentCameraMode]);
	
	m_transitionTime = max(m_transitionTime - frameTime, 0.0f);

	bool orientateToPlayerView = m_cameraModes[currentCameraMode]->UpdateView(m_ownerPlayer, viewParams, frameTime);

	if (m_transitionTime > 0.0f)
	{
		assert(m_transitionCameraMode >= 0);
		QuatT newCamTrans;
		QuatT newTrans(viewParams.rotation, viewParams.position);

    assert(0 <= m_transitionCameraMode && m_transitionCameraMode < eCameraMode_Last);
		m_cameraModes[m_transitionCameraMode]->UpdateView(m_ownerPlayer, viewParams, frameTime);
		QuatT oldTrans(viewParams.rotation, viewParams.position);

		float t = (m_transitionTime / m_totalTransitionTime);

		newCamTrans.SetNLerp(newTrans, oldTrans, t);

		viewParams.position = newCamTrans.t;
		viewParams.rotation = newCamTrans.q;


#ifndef _RELEASE
		if (g_pGameCVars->pl_debug_view)
		{
			CryWatch("Transitioning %f (%.2f) Pos (%.2f, %.2f, %.2f) Rot (%.2f, %.2f, %.2f, %.2f)", t, m_totalTransitionTime, viewParams.position.x, viewParams.position.y, viewParams.position.z, viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w);
		}
#endif //!_RELEASE
	}

#ifndef _RELEASE
	if (g_pGameCVars->g_detachCamera == 2)
	{
		Vec3 forward = viewParams.rotation.GetColumn1();
		Vec3 xAxis = viewParams.rotation.GetColumn0();
		Vec3 yAxis = viewParams.rotation.GetColumn2();
		IRenderAuxGeom* pRender = gEnv->pRenderer->GetIRenderAuxGeom();

		float y = tanf(0.5f*viewParams.fov);
		float x = y * gEnv->pSystem->GetViewCamera().GetProjRatio();

		Vec3 v1 = forward + xAxis * x + yAxis * y;
		Vec3 v2 = forward - xAxis * x + yAxis * y;
		Vec3 v3 = forward - xAxis * x - yAxis * y;
		Vec3 v4 = forward + xAxis * x - yAxis * y;

		// Draw the near plane clipping region (nothing should intersect with this volume otherwise clipping will occur)
		pRender->DrawLine(viewParams.position, Col_Red, viewParams.position + v1 * viewParams.nearplane, Col_Red);
		pRender->DrawLine(viewParams.position, Col_Red, viewParams.position + v2 * viewParams.nearplane, Col_Red);
		pRender->DrawLine(viewParams.position, Col_Red, viewParams.position + v3 * viewParams.nearplane, Col_Red);
		pRender->DrawLine(viewParams.position, Col_Red, viewParams.position + v4 * viewParams.nearplane, Col_Red);

		pRender->DrawLine(viewParams.position + v1 * viewParams.nearplane, Col_Red, viewParams.position + v2 * viewParams.nearplane, Col_Red);
		pRender->DrawLine(viewParams.position + v2 * viewParams.nearplane, Col_Red, viewParams.position + v3 * viewParams.nearplane, Col_Red);
		pRender->DrawLine(viewParams.position + v3 * viewParams.nearplane, Col_Red, viewParams.position + v4 * viewParams.nearplane, Col_Red);
		pRender->DrawLine(viewParams.position + v4 * viewParams.nearplane, Col_Red, viewParams.position + v1 * viewParams.nearplane, Col_Red);

		// Draw the view frustum
		const float farDist = 10;

		pRender->DrawLine(viewParams.position + v1 * viewParams.nearplane, Col_Yellow, viewParams.position + v1 * farDist, Col_Yellow);
		pRender->DrawLine(viewParams.position + v2 * viewParams.nearplane, Col_Yellow, viewParams.position + v2 * farDist, Col_Yellow);
		pRender->DrawLine(viewParams.position + v3 * viewParams.nearplane, Col_Yellow, viewParams.position + v3 * farDist, Col_Yellow);
		pRender->DrawLine(viewParams.position + v4 * viewParams.nearplane, Col_Yellow, viewParams.position + v4 * farDist, Col_Yellow);

		// Reset the view params to their previous values to keep the camera fixed
		viewParams.position = viewParams.GetPositionLast();
		viewParams.rotation = viewParams.GetRotationLast();
	}


	if (g_pGameCVars->pl_debug_view)
	{
		Vec3 fwd = viewParams.rotation.GetColumn1();
		CryWatch("PlayerCamera: fwd %f, %f, %f)", fwd.x, fwd.y, fwd.z);
	}

	if (g_pGameCVars->g_debugOffsetCamera)
	{
		Vec3 offset(0.0f, (float)g_pGameCVars->g_debugOffsetCamera, 0.0f);
		viewParams.position += viewParams.rotation * offset;
	}

#endif //!_RELEASE

	return orientateToPlayerView;
}


void CPlayerCamera::UpdateCommon(SViewParams& viewParams, float frameTime )
{
	viewParams.fov = g_pGame->GetFOV() * m_ownerPlayer.GetActorParams().viewFoVScale * (gf_PI/180.0f);
	viewParams.nearplane = CAMERA_NEAR_PLANE_DISTANCE;

	FilterGroundOnlyShakes(viewParams);
}

void CPlayerCamera::FilterGroundOnlyShakes(SViewParams& viewParams)
{
	if (viewParams.groundOnly)
	{
		const SActorStats* pActorStats = m_ownerPlayer.GetActorStats();
		assert(pActorStats);
		if (pActorStats->inAir > 0.0f)
		{
			float factor = pActorStats->inAir;
			Limit(factor, 0.0f, 0.2f);
			factor = 1.0f - (factor * 5.0f);
			factor = factor*factor;

			viewParams.currentShakeQuat.SetSlerp(IDENTITY, viewParams.currentShakeQuat, factor);
			viewParams.currentShakeShift *= factor;
		}
	}
}

void CPlayerCamera::UpdateTotalTransitionTime()
{
	bool alreadySetTotalTransitionTime=false;

	if (m_currentCameraMode == eCameraMode_PartialAnimationControlled)
	{
		if (m_ownerPlayer.IsOnLedge())
		{
			m_enteredPartialAnimControlledCameraOnLedge=true;

			// entering anim control from ledge
			m_totalTransitionTime = g_pGameCVars->pl_cameraTransitionTimeLedgeGrabIn;
			alreadySetTotalTransitionTime=true;
		}
		else
		{
			m_enteredPartialAnimControlledCameraOnLedge=false;
		}
	}
	else if (m_previousCameraMode == eCameraMode_PartialAnimationControlled)
	{
		if (m_enteredPartialAnimControlledCameraOnLedge)
		{
			// leaving anim control from ledge
			m_totalTransitionTime = g_pGameCVars->pl_cameraTransitionTimeLedgeGrabOut;
			alreadySetTotalTransitionTime=true;
		}
	}

	if (!alreadySetTotalTransitionTime)
	{
		m_totalTransitionTime  = g_pGameCVars->pl_cameraTransitionTime;
	}
}

ECameraMode CPlayerCamera::GetCurrentCameraMode()
{
	//Benito
	//Note: Perhaps the mode should be set instead from the current player state, instead of checking stats every frame
	//NB: Have started working towards this! [TF]

	const SPlayerStats* pPlayerStats = static_cast<const SPlayerStats*>(m_ownerPlayer.GetActorStats());
	assert(pPlayerStats);

	if (pPlayerStats->spectatorInfo.mode == CActor::eASM_Fixed)
	{
		return eCameraMode_SpectatorFixed;
	}

	if(pPlayerStats->spectatorInfo.mode == CActor::eASM_Follow)
	{
		return eCameraMode_SpectatorFollow;
	}

	if(pPlayerStats->spectatorInfo.mode == CActor::eASM_Killer)
	{
		return eCameraMode_SpectatorFollow;
	}

	if (gEnv->bMultiplayer)
	{
#ifndef _RELEASE
		if (g_pGameCVars->g_tpdeathcam_dbg_alwaysOn)
		{
			return eCameraMode_Death;
		}
#endif
		if (m_ownerPlayer.IsDead() || pPlayerStats->bStealthKilled)
		{
			if (g_pGameCVars->g_deathCam)
			{
				if (pPlayerStats->isThirdPerson)
				{
					return eCameraMode_Death;
				}
			}
			/*
			if (pPlayerStats->isInFreeFallDeath)
			{
			// [tlh] TODO?
			}
			*/
		}
	}
	else if (g_pGameCVars->g_deathCamSP.enable && m_ownerPlayer.IsDead() && !pPlayerStats->isRagDoll && !pPlayerStats->isThirdPerson 
		&& m_ownerPlayer.GetHitDeathReactions() && !m_ownerPlayer.GetHitDeathReactions()->IsInReaction())
	{
		return eCameraMode_Death_SinglePlayer;
	}

	bool animationControlled = (pPlayerStats->isRagDoll || pPlayerStats->followCharacterHead) && (pPlayerStats->isThirdPerson == false);
	if (animationControlled)
	{
		return eCameraMode_AnimationControlled;
	}

	if (m_ownerPlayer.GetLinkedVehicle())
	{
		return eCameraMode_Vehicle;
	}

	// TEMP: Any camera mode selected by the above code also needs to be turned OFF by this function.
	//
	switch (m_currentCameraMode)
	{
		case eCameraMode_AnimationControlled:
		case eCameraMode_Death:
		case eCameraMode_Death_SinglePlayer:
		case eCameraMode_SpectatorFollow:
		case eCameraMode_SpectatorFixed:
		case eCameraMode_Vehicle:
			{
				return eCameraMode_Default;
			}
	}

	return m_currentCameraMode;
}

void CPlayerCamera::PostUpdate(const QuatT &_camDelta)
{
	static bool playerWasSlidingLastFrame = false;

	const bool playerIsSliding = m_ownerPlayer.IsSliding();

	ECameraMode camMode = GetCurrentCameraMode();

	float blendFactorTran = 0.0f;
	float blendFactorRot = 0.0f;

	if (g_pGameCVars->cl_postUpdateCamera 
		&& (g_pGameCVars->g_detachCamera == 0)
		&& !m_ownerPlayer.IsThirdPerson())
	{
		m_cameraModes[camMode]->GetCameraAnimationFactor(blendFactorTran, blendFactorRot);

		// fix massive one frame glitch coming from the camera bone when exiting slide
		if(playerWasSlidingLastFrame && !playerIsSliding)
		{
			blendFactorRot = 0.0f;
		}

		if (m_transitionTime > 0.0f)
		{
			float blendFactorTran2, blendFactorRot2;
			m_cameraModes[m_transitionCameraMode]->GetCameraAnimationFactor(blendFactorTran2, blendFactorRot2);

			float t = (m_transitionTime / m_totalTransitionTime);
			blendFactorTran = ((blendFactorTran2 - blendFactorTran) * t) + blendFactorTran;
			blendFactorRot  = ((blendFactorRot2 - blendFactorRot) * t) + blendFactorRot;
		}
	}

	playerWasSlidingLastFrame = playerIsSliding;

	if ((blendFactorTran > 0.0f) || (blendFactorRot > 0.0f))
	{
		QuatT camDelta;
		camDelta.q.SetSlerp(IDENTITY, _camDelta.q, blendFactorRot);
		camDelta.t = _camDelta.t * blendFactorTran;

		Matrix34 camRotDelta;
		{
			CCamera camera = gEnv->pSystem->GetViewCamera();
			Matrix34 newMat;
			Matrix34 camMat = camera.GetMatrix();

			if (g_pGameCVars->cl_postUpdateCamera == 1)
			{
				camRotDelta.SetIdentity();
				camRotDelta.SetTranslation(camDelta.t);
			}
			else
			{
				camRotDelta.Set(Vec3Constants<f32>::fVec3_One, camDelta.q, camDelta.t);
			}

			if (g_pGameCVars->cl_postUpdateCamera == 3)
			{
				newMat = camRotDelta * camMat;
			}
			else
			{
				newMat = camMat * camRotDelta;
			}
			camera.SetMatrix(newMat);

			gEnv->pSystem->SetViewCamera(camera);
		}

		// Update player camera space position with blending 
		IEntity* pPlayerEntity = m_ownerPlayer.GetEntity();
		if(pPlayerEntity)
		{
			const int slotIndex = 0;
			uint32 entityFlags = pPlayerEntity->GetSlotFlags(slotIndex);
			if(entityFlags & ENTITY_SLOT_RENDER_NEAREST)
			{
				Vec3 cameraSpacePos;
				pPlayerEntity->GetSlotCameraSpacePos(slotIndex,cameraSpacePos);
				cameraSpacePos = camRotDelta.GetInverted() * cameraSpacePos;
				pPlayerEntity->SetSlotCameraSpacePos(slotIndex,cameraSpacePos);
			}
		}

#ifndef _RELEASE
		if (g_pGameCVars->pl_debug_view)
		{
			CryWatch("CamAnimDelta factor Pos (%.2f, %.2f, %.2f):%f Rot (%.2f, %.2f, %.2f, %.2f):%f", camDelta.t.x, camDelta.t.y, camDelta.t.z, blendFactorTran, camDelta.q.v.x, camDelta.q.v.y, camDelta.q.v.z, camDelta.q.w, blendFactorRot);
		}
#endif //!_RELEASE
	}
}

