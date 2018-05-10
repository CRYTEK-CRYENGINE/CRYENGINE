// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 28:10:2005   16:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "IronSight.h"
#include "Player.h"
#include "GameCVars.h"
#include "Single.h"

#include "WeaponSharedParams.h"
#include "ScreenEffects.h"
#include "Game.h"
#include "UI/UIManager.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/UICVars.h"
#include "Utility/CryWatch.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "StatsRecordingMgr.h"

#include "Stereo3D/StereoFramework.h"
#include "WeaponSystem.h"
#include "ItemAnimation.h"

#include "UI/HUD/HUDEventWrapper.h"

CRY_IMPLEMENT_GTI_BASE(CIronSight);

//------------------------------------------------------------------------
CIronSight::CIronSight()
	: m_pWeapon(nullptr)
	, m_parentZoomParams(nullptr)
	, m_zoomParams(nullptr)
	, m_zmIdx(0)
	, m_savedFoVScale(0.0f)
	, m_zoomTimer(0.0f)
	, m_zoomTime(0.0f)
	, m_averageDoF(50.0f)
	, m_leaveZoomTimeDelay(0.0f)
	, m_startZoomFoVScale(0.0f)
	, m_endZoomFoVScale(0.0f)
	, m_currentZoomFoVScale(1.0f)
	, m_currentZoomBonusScale(1.0f)
	, m_currentStep(0)
	, m_initialNearFov(gEnv->bMultiplayer ? 60.0f : 55.0f)
	, m_swayTime(0.0f)
	, m_lastRecoil(0.0f)
	, m_stableTime(0.0f)
	, m_enabled(true)
	, m_smooth(false)
	, m_zoomOutScheduled(false)
	, m_zoomed(false)
	, m_zoomingIn(false)
	, m_stable(false)
	, m_leavingstability(false)
	, m_blendNearFoV(false)
	, m_activated(false)
{
	m_swayX.SetParams(2.0f);
	m_swayY.SetParams(2.0f);
}

//------------------------------------------------------------------------
CIronSight::~CIronSight()
{
	m_zoomParams = 0; 
}

//------------------------------------------------------------------------
void CIronSight::InitZoomMode(IWeapon *pWeapon, const SParentZoomModeParams* pParams, uint32 id)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);
	m_zmIdx = id;

	m_zoomParams = &pParams->baseZoomMode;
	m_parentZoomParams = pParams;
}

//------------------------------------------------------------------------
void CIronSight::Update(float frameTime, uint32 frameId)
{
	bool keepUpdating=false;
	bool isClient = m_pWeapon->IsOwnerClient();

	bool wasZooming = m_zoomTimer>0.0f;
	if (wasZooming || m_zoomed)
	{
		if(wasZooming)
		{
			m_zoomTimer -= frameTime;

			ApplyZoomMod(m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()), GetZoomTransition());

			if (m_zoomTimer<=0.0f)
			{
				m_zoomTimer=0.0f;
				
				if (m_zoomingIn)
				{
					OnZoomedIn();
				}
				else
				{
					OnZoomedOut();
				}
			}
		}
		
		if (isClient)
		{
			//float t=m_zoomTimer/m_zoomTime;
			if (m_zoomTime > 0.0f)
			{
				//t=1.0f-max(t, 0.0f);
				gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 1.f);
			}

			float gameDofActive;
			gEnv->p3DEngine->GetPostEffectParam("Dof_Active", gameDofActive);

			if (gameDofActive > 0.0f)
			{
				// try to convert user-defined settings to IronSight system (used for Core)
				float userActive;
				gEnv->p3DEngine->GetPostEffectParam("Dof_User_Active", userActive);
				if (userActive > 0.0f)
				{
					float focusRange;
					float focusDistance;
					gEnv->p3DEngine->GetPostEffectParam("Dof_User_FocusRange", focusRange);
					gEnv->p3DEngine->GetPostEffectParam("Dof_User_FocusDistance", focusDistance);
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMin", focusDistance);
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMax", focusDistance);
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", focusDistance+focusRange*0.5f);
				}
				else
				{
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMin", m_zoomParams->zoomParams.dof_focusMin);
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMax", m_zoomParams->zoomParams.dof_focusMax);
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", m_zoomParams->zoomParams.dof_focusLimit);
					CActor* pOwner = m_pWeapon->GetOwnerActor();
					bool useMinFilter = true;
					
					if (useMinFilter)
						gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZ", m_zoomParams->zoomParams.dof_minZ);
					else
						gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZ", 0.0f);
					gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZScale", m_zoomParams->zoomParams.dof_minZScale);
				}
			}

			if(gEnv->bMultiplayer && (m_zoomTimer == 0.f || m_zoomingIn) && !m_pWeapon->CanZoomInState(g_pGameCVars->i_ironsight_falling_unzoom_minAirTime))
			{
				ExitZoom(true);
			}
		}

		keepUpdating=true;
	}

	if (m_zoomTime > 0.0f)	// zoomTime is set to 0.0 when zooming ends
	{
		keepUpdating=true;
		float t = CLAMP(1.0f-m_zoomTimer/m_zoomTime, 0.0f, 1.0f);
		float fovScale;

		if (m_smooth)
		{
			fovScale = m_startZoomFoVScale+t*(m_endZoomFoVScale-m_startZoomFoVScale);
		}
		else
		{
			fovScale = m_startZoomFoVScale;
			if (t>=1.0f)
				fovScale = m_endZoomFoVScale;
		}

		OnZoomStep(m_startZoomFoVScale>m_endZoomFoVScale, t);

		m_currentZoomFoVScale = fovScale;

		if(m_blendNearFoV && isClient && m_zoomParams->zoomParams.scope_mode)
		{
			AdjustNearFov(t*1.25f, IsZoomingIn());
		}

		if (t>=1.0f)
		{
			SetIsZoomed(m_zoomingIn);
			m_zoomTime = 0.0f;
		}
	}

	UpdateBonusZoomScale(frameTime);

	SetActorFoVScale(GetZoomFovScale() * GetZoomBonusScale());

	m_leaveZoomTimeDelay = max(m_leaveZoomTimeDelay - frameTime, 0.0f);
	if (m_zoomOutScheduled && m_leaveZoomTimeDelay == 0.0f && !m_pWeapon->IsInputFlagSet(CWeapon::eWeaponAction_Zoom))
	{
		LeaveZoom();
		m_zoomOutScheduled = false;
	}

  if (keepUpdating)
    m_pWeapon->RequireUpdate(eIUS_Zooming);

}

//------------------------------------------------------------------------
void CIronSight::Release()
{
	//Return to pool. If there is no game, probably we are exiting the game, and the 
	//memory in the pool will be freed regardless
	if (g_pGame)
	{
		g_pGame->GetWeaponSystem()->DestroyZoomMode(this);
	}
}

//------------------------------------------------------------------------
void CIronSight::Activate(bool activate)
{
	if (activate != m_activated)
	{
		m_activated = activate;

		if (!m_zoomed && !m_zoomingIn && !activate)
		{
			return;
		}

		if(m_zoomOutScheduled)
		{
			LeaveZoom();
		}

		bool wasZoomed = IsZoomed();
		SetIsZoomed(false);
		OnZoom(0.0f, 0);// reset hud zoom when leaving on initially activating iron-sight.
		m_zoomingIn = false;
		if (wasZoomed)
			OnZoomedOut();

		m_currentStep = 0;
		m_lastRecoil = 0.0f;
		m_leaveZoomTimeDelay = 0.0f;
		m_zoomOutScheduled = false;

		SetActorFoVScale(1.0f);

		ResetTurnOff();
		
		if(activate)
		{
			m_pWeapon->GetScopeReticule().SetMaterial(GetCrossHairMaterial());
			m_pWeapon->GetScopeReticule().SetBlinkFrequency(m_zoomParams->scopeParams.blink_frequency);
		}
		else
		{
			ClearBlur();

			if(m_zoomParams->zoomParams.scope_mode)
			{
				ResetNearFov();
			}
		}
	}

	ClearDoF();
}

//------------------------------------------------------------------------
bool CIronSight::CanZoom() const
{
	return true;
}

//------------------------------------------------------------------------
bool CIronSight::StartZoom(bool stayZoomed, bool fullZoomout, int zoomStep)
{
	if (m_pWeapon->IsBusy()) // || (IsToggle() && IsZooming())) //This second check has been removed because we want to be able to interrupt a zoom-out with a new zoom in
	{
		return false;
	}

	if(m_pWeapon->IsOwnerClient())
	{
		if (g_pGame->GetScreenEffects())
			g_pGame->GetScreenEffects()->ProcessZoomInEffect();
	}

	m_zoomOutScheduled = false;

	if (!m_zoomed || stayZoomed)
	{
		m_blendNearFoV = true;
		EnterZoom(GetModifiedZoomInTime(), true, zoomStep);
		m_currentStep = zoomStep;
	}
	else
	{
		m_blendNearFoV = true;
		int currentStep = m_currentStep;
		int nextStep = currentStep+1;

		if (nextStep > (int)m_zoomParams->zoomParams.stages.size())
		{
			if (!stayZoomed)
			{
				if (fullZoomout)
				{
					StopZoom();
				}
				else
				{
					float oFoV = GetZoomFoVScale(currentStep);
					m_currentStep = 0;
					float tFoV = GetZoomFoVScale(m_currentStep);
					ZoomIn(m_zoomParams->zoomParams.stage_time, oFoV, tFoV, true, m_currentStep);
					return true;
				}
			}
		}
		else
		{
			float oFoV = GetZoomFoVScale(currentStep);
			float tFoV = GetZoomFoVScale(nextStep);

			ZoomIn(m_zoomParams->zoomParams.stage_time, oFoV, tFoV, true, nextStep);

			m_currentStep = nextStep;

			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
void CIronSight::StopZoom()
{
	ScheduleLeaveZoom();

	// Record 'Zoom' telemetry stats.

	CStatsRecordingMgr::TryTrackEvent(m_pWeapon->GetOwnerActor(), eGSE_Zoom, false);
}

//------------------------------------------------------------------------
void CIronSight::ExitZoom(bool force)
{
	if (force)
		LeaveZoom();
	else if (m_zoomed && m_zoomTime==0.0f)
		ScheduleLeaveZoom();
}

//------------------------------------------------------------------------
void CIronSight::ZoomIn()
{
	if (m_pWeapon->IsBusy())
		return;

	if (!m_zoomed)
	{
		EnterZoom(GetModifiedZoomInTime(), true);
		m_currentStep = 1;
		m_leaveZoomTimeDelay = m_zoomParams->zoomParams.zoom_out_delay;
	}
	else
	{
		int currentStep = m_currentStep;
		int nextStep = currentStep+1;

		if (nextStep > (int)m_zoomParams->zoomParams.stages.size())
			return;
		else
		{
			float oFoV = GetZoomFoVScale(currentStep);
			float tFoV = GetZoomFoVScale(nextStep);

			ZoomIn(m_zoomParams->zoomParams.stage_time, oFoV, tFoV, true, nextStep);

			m_currentStep = nextStep;
		}
	}

	m_blendNearFoV = false;
}

//------------------------------------------------------------------------
bool CIronSight::ZoomOut()
{
	if (m_pWeapon->IsBusy())
		return false;

	if (!m_zoomed)
	{
		EnterZoom(GetModifiedZoomInTime(), true);
		m_currentStep = 1;
		m_leaveZoomTimeDelay = m_zoomParams->zoomParams.zoom_out_delay;
	}
	else
	{
		int currentStep = m_currentStep;
		int nextStep = currentStep-1;

		ClearDoF();

		if (nextStep < 1)
			return false;
		else
		{
			float oFoV = GetZoomFoVScale(currentStep);
			float tFoV = GetZoomFoVScale(nextStep);

			ZoomIn(m_zoomParams->zoomParams.stage_time, oFoV, tFoV, true, nextStep);

			m_currentStep = nextStep;
			m_blendNearFoV = false;

			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
bool CIronSight::IsZoomed() const
{
	return m_zoomed;
}

//------------------------------------------------------------------------
bool CIronSight::IsZoomingInOrOut() const
{
	return m_zoomTimer>0.0f;
}

//------------------------------------------------------------------------
bool CIronSight::IsZoomingIn() const
{
	return m_zoomingIn;
}

//------------------------------------------------------------------------
bool CIronSight::IsZoomingOutScheduled() const
{
	return m_zoomOutScheduled;
}

//------------------------------------------------------------------------
void CIronSight::CancelZoomOutSchedule()
{
	if (m_zoomOutScheduled)
		LeaveZoom();
	m_zoomOutScheduled = false;
}

//------------------------------------------------------------------------
EZoomState CIronSight::GetZoomState() const
{
	if (IsZoomingInOrOut())
	{
		if (IsZoomingIn())
			return eZS_ZoomingIn;
		else
			return eZS_ZoomingOut;
	}
	else
	{
		if (IsZoomed())
			return eZS_ZoomedIn;
		else
			return eZS_ZoomedOut;
	}
}

//------------------------------------------------------------------------
void CIronSight::Enable(bool enable)
{
	m_enabled = enable;
}

//------------------------------------------------------------------------
bool CIronSight::IsEnabled() const
{
	return m_enabled;
}

//------------------------------------------------------------------------
void CIronSight::EnterZoom(float time, bool smooth, int zoomStep)
{
	if (IsZoomingInOrOut() && !m_zoomingIn)
	{
		OnZoomedOut();
	}
	ResetTurnOff();

	if(CActor* pOwner = m_pWeapon->GetOwnerActor())
	{
		const char* zoomModeSuffix = m_zoomParams->zoomParams.suffix.c_str();

		if(zoomModeSuffix && zoomModeSuffix[0])
		{
			uint32 zoommodeCrC = CCrc32::ComputeLowercase(zoomModeSuffix);
			pOwner->SetTagByCRC(zoommodeCrC, true);
		}
	}

	if (m_pWeapon->IsOwnerClient())
	{
		float fadeTo = m_zoomParams->zoomParams.hide_crosshair ? 0.0f : 1.0f;
		if (g_pGame->GetUI())
		{
			m_pWeapon->FadeCrosshair(fadeTo, g_pGame->GetUI()->GetCVars()->hud_Crosshair_ironsight_fadeOutTime);
		}
	}

	float oFoV = GetZoomFoVScale(0);
	float tFoV = GetZoomFoVScale(zoomStep);

	ZoomIn(time, oFoV, tFoV, smooth, zoomStep);
  Stereo3D::Zoom::SetFinalPlaneDist(m_zoomParams->stereoParams.convergenceDistance, time);
  Stereo3D::Zoom::SetFinalEyeDist(m_zoomParams->stereoParams.eyeDistance, time);

	const SZoomParams &zoomParams = m_zoomParams->zoomParams;

	m_pWeapon->PlayAction( m_pWeapon->GetFragmentIds().zoom_in, 0, false, CItem::eIPAF_Default);

	OnEnterZoom();

	// Record 'Zoom' telemetry stats.

	CStatsRecordingMgr::TryTrackEvent(m_pWeapon->GetOwnerActor(), eGSE_Zoom, true);
}

void CIronSight::ScheduleLeaveZoom()
{
	if (!m_pWeapon->IsReloading() && m_zoomed)
	{
		m_zoomOutScheduled = true;
		m_leaveZoomTimeDelay = m_zoomParams->zoomParams.zoom_out_delay;
	}
	else
		LeaveZoom();
}

void CIronSight::LeaveZoom()
{
	if (!m_zoomed && !m_zoomingIn)
		return;

	if (IsZoomingInOrOut() && m_zoomingIn)
	{
		OnZoomedIn();
	}
	ResetTurnOff();

	if(IActionController* pController = m_pWeapon->GetActionController())
	{
		CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();
		const SMannequinItemParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(pController);
		if (pParams && pParams->tagIDs.shoulder.IsValid())
		{
			SAnimationContext &animContext = pController->GetContext();
			animContext.state.Set(pParams->tagIDs.shoulder, true);
		}
	}
	
	if(m_pWeapon->IsOwnerClient())
	{
		if (g_pGame->GetUI())
		{
			const CUICVars* pCvars = g_pGame->GetUI()->GetCVars();
			m_pWeapon->FadeCrosshair(1.0f, pCvars->hud_Crosshair_ironsight_fadeInTime, pCvars->hud_Crosshair_ironsight_fadeInDelay);
		}

		SHUDEventWrapper::ClearInteractionMsg(eHIMT_INTERACTION, "@ui_interaction_stabilizegun");
	}

	float oFoV = GetZoomFoVScale(0);
	float tFoV = GetZoomFoVScale(1);

	ZoomOut(m_zoomParams->zoomParams.zoom_out_time, tFoV, oFoV, true, 0);
  Stereo3D::Zoom::ReturnToNormalSetting(m_zoomParams->zoomParams.zoom_out_time);

	m_pWeapon->PlayAction(m_pWeapon->GetFragmentIds().zoom_out, 0, false, CItem::eIPAF_Default);

	m_currentStep = 0;

	OnLeaveZoom();

	m_blendNearFoV = true;
}

//------------------------------------------------------------------------
void CIronSight::ResetTurnOff()
{
	m_savedFoVScale = 0.0f;
}

//------------------------------------------------------------------------

struct CIronSight::DisableTurnOffAction
{
	DisableTurnOffAction(CIronSight *_ironSight): ironSight(_ironSight) {};
	CIronSight *ironSight;

	void execute(CItem *pWeapon)
	{
		const SZoomParams &zoomParams = ironSight->m_zoomParams->zoomParams;
		ironSight->OnZoomedIn();
	}
};

struct CIronSight::EnableTurnOffAction
{
	EnableTurnOffAction(CIronSight *_ironSight): ironSight(_ironSight) {};
	CIronSight *ironSight;

	void execute(CItem *pWeapon)
	{
		ironSight->OnZoomedOut();
	}
};

void CIronSight::TurnOff(bool enable, bool smooth, bool anim)
{
	const SZoomParams &zoomParams = m_zoomParams->zoomParams;
	if (!enable && (m_savedFoVScale > 0.0f))
	{
		OnEnterZoom();

		float oFoV = GetZoomFoVScale(0);
		float tFoV = m_savedFoVScale;

		ZoomIn(zoomParams.zoom_out_time, oFoV, tFoV, smooth, 0);

		if (anim)
		{
			m_pWeapon->PlayAction( m_pWeapon->GetFragmentIds().zoom_in, 0, false, CItem::eIPAF_Default);
		}

		m_pWeapon->GetScheduler()->TimerAction((uint32)(zoomParams.zoom_out_time*1000), CSchedulerAction<DisableTurnOffAction>::Create(this), false);
		m_savedFoVScale = 0.0f;
	}
	else if (m_zoomed && enable)
	{
		m_pWeapon->SetBusy(true);
		m_savedFoVScale = GetActorFoVScale();

		OnLeaveZoom();

		float oFoV = GetZoomFoVScale(0);
		float tFoV = m_savedFoVScale;

		ZoomOut(zoomParams.zoom_out_time, tFoV, oFoV, smooth, 0);

		if (anim)
		{
			m_pWeapon->PlayAction( m_pWeapon->GetFragmentIds().zoom_out, 0, false, CItem::eIPAF_Default);
		}

		m_pWeapon->GetScheduler()->TimerAction((uint32)(zoomParams.zoom_out_time*1000), CSchedulerAction<EnableTurnOffAction>::Create(this), false);
	}
}

//------------------------------------------------------------------------
void CIronSight::ZoomIn(float time, float from, float to, bool smooth, int nextStep)
{
	m_zoomTime = time;
	m_zoomTimer = m_zoomTime;
	m_startZoomFoVScale = from;
	m_endZoomFoVScale = to;
	m_smooth = smooth;

	float totalFoV = abs(m_endZoomFoVScale-m_startZoomFoVScale);
	float ownerFoV = GetActorFoVScale();

	m_startZoomFoVScale = ownerFoV;

	if (totalFoV != 0)
	{
		float deltaFoV = abs(m_endZoomFoVScale-m_startZoomFoVScale)/totalFoV;

		if (deltaFoV < totalFoV)
		{
			m_zoomTime = max((deltaFoV/totalFoV)*time, 0.01f);

			if(deltaFoV == 0.f)
			{
				m_startZoomFoVScale += 0.001f;
			}

			m_zoomTimer = m_zoomTime;
		}
	}

	CCCPOINT_IF(m_pWeapon->IsOwnerClient(), PlayerWeapon_IronSightStartZoomIn);

	m_zoomingIn = true;

	if(!m_zoomed && gEnv->pRenderer)
		 gEnv->pRenderer->EF_Query(EFQ_GetDrawNearFov, m_initialNearFov);

	OnZoom(time, nextStep);

	m_pWeapon->RequireUpdate(eIUS_Zooming);

	if (m_pWeapon->IsOwnerClient())
	{
		const char* szZoomOverlay = GetShared()->zoomParams.zoom_overlay.c_str();
		if (szZoomOverlay != NULL && szZoomOverlay[0] != '\0')
		{
			SHUDEvent event(eHUDEvent_OnSetWeaponOverlay);
			event.AddData(SHUDEventData(true));
			event.AddData(SHUDEventData(szZoomOverlay));
			event.AddData(SHUDEventData(m_zoomTime));
			CHUDEventDispatcher::CallEvent(event);
		}
	}
}

//------------------------------------------------------------------------
void CIronSight::ZoomOut(float time, float from, float to, bool smooth, int nextStep)
{
	m_zoomTimer = time;
	m_zoomTime = m_zoomTimer;

	m_startZoomFoVScale = from;
	m_endZoomFoVScale = to;
	m_smooth = smooth;


	float totalFoV = abs(m_endZoomFoVScale-m_startZoomFoVScale);
	float ownerFoV = GetActorFoVScale();

	m_startZoomFoVScale = ownerFoV;

	if (totalFoV != 0)
	{
		float deltaFoV = abs(m_endZoomFoVScale-m_startZoomFoVScale);

		if (deltaFoV < totalFoV)
		{
			m_zoomTime = max((deltaFoV/totalFoV)*time, 0.01f);
			m_zoomTimer = m_zoomTime;
		}
	}

	CCCPOINT_IF(m_pWeapon->IsOwnerClient(), PlayerWeapon_IronSightStartZoomOut);

	m_zoomingIn = false;

	OnZoom(time, nextStep);

	m_pWeapon->RequireUpdate(eIUS_Zooming);

	if (m_pWeapon->IsOwnerClient())
	{
		const char* szZoomOverlay = GetShared()->zoomParams.zoom_overlay.c_str();
		if (szZoomOverlay != NULL && szZoomOverlay[0] != '\0')
		{
			SHUDEvent event(eHUDEvent_OnSetWeaponOverlay);
			event.AddData(SHUDEventData(false));
			event.AddData(SHUDEventData(szZoomOverlay));
			event.AddData(SHUDEventData(m_zoomTime));
			CHUDEventDispatcher::CallEvent(event);
		}
	}
}

//------------------------------------------------------------------------
void CIronSight::OnEnterZoom()
{
	int currentStep = CLAMP(m_currentStep, 0, (int)m_zoomParams->zoomParams.stages.size() - 1);
	if(m_pWeapon->IsOwnerClient())
	{
		if(m_zoomParams->zoomParams.stages[currentStep].useDof && m_zoomParams->zoomParams.dof)
		{
			gEnv->p3DEngine->SetPostEffectParam("PostMSAA_Scope", 1);
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", -1);
			if (!m_zoomParams->zoomParams.dof_mask.empty())
			{
				gEnv->p3DEngine->SetPostEffectParam("FilterMaskedBlurring_Amount", 1.0f);
				gEnv->p3DEngine->SetPostEffectParamString("FilterMaskedBlurring_MaskTexName", m_zoomParams->zoomParams.dof_mask.c_str());
			}
			else
			{
				gEnv->p3DEngine->SetPostEffectParam("FilterMaskedBlurring_Amount", 0.0f);
			}
		}
	}

	if (IFireMode* pFireMode = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
	{
		pFireMode->OnZoomStateChanged();
	}

	IView* pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
	if (pView)
	{
		pView->SetZoomedScale(m_zoomParams->zoomParams.cameraShakeMultiplier);
	}

	m_swayTime = 0.0f;
}

void CIronSight::SetIsZoomed(bool isZoomed)
{
	if (m_zoomed != isZoomed)
	{
		m_zoomed = isZoomed;

		if (isZoomed)
			m_pWeapon->OnZoomedIn();
		else
			m_pWeapon->OnZoomedOut();
	}
}

//------------------------------------------------------------------------
void CIronSight::OnZoomedIn()
{
	SetIsZoomed(true);

	ApplyZoomMod(m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()), 1.f);

	if(m_pWeapon->IsOwnerClient())
	{
		CCCPOINT(PlayerWeapon_IronSightOn);

		int currentStep = CLAMP(m_currentStep, 0, (int)m_zoomParams->zoomParams.stages.size() - 1);
		if(m_zoomParams->zoomParams.dof && m_zoomParams->zoomParams.stages[currentStep].useDof)
		{
			gEnv->p3DEngine->SetPostEffectParam("PostMSAA_Scope", 0);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", -1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 0.6f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1);
		}

		ICharacterInstance* pCharacter = m_pWeapon->GetEntity()->GetCharacter(0);
		if (m_zoomParams->zoomParams.hide_weapon && pCharacter != 0)
		{
			pCharacter->HideMaster(1);
		}

		if(const CPlayer* pOwnerPlayer = m_pWeapon->GetOwnerPlayer())
		{
			if(m_pWeapon && m_pWeapon->GetEntity()->GetClass()!=CItem::sBinocularsClass)
			{
				SHUDEventWrapper::SetInteractionMsg(eHIMT_INTERACTION, "@ui_interaction_stabilizegun", -1.0f, false, "stabilize", "player");
			}
		}
	}

	m_lastRecoil = 0.0f;

	m_pWeapon->OnZoomChanged(true, m_pWeapon->GetCurrentZoomMode());
}

//------------------------------------------------------------------------
void CIronSight::OnZoom(float time, int targetStep)
{
	if(m_pWeapon->IsOwnerClient())
	{
		SHUDEvent event(eHUDEvent_OnZoom);
		event.ReserveData(5);
		event.AddData(SHUDEventData(targetStep));
		const int numSteps = m_zoomParams->zoomParams.stages.size();
		event.AddData(SHUDEventData(numSteps));

		float zoom = 1.0f;
		if(targetStep > 0 && targetStep <= numSteps)
			zoom = m_zoomParams->zoomParams.stages[targetStep-1].stage;

		event.AddData(SHUDEventData(zoom));
		event.AddData(SHUDEventData(GetModifiedZoomInTime()));
		event.AddData(SHUDEventData(m_zoomParams->zoomParams.zoom_out_time));
		CHUDEventDispatcher::CallEvent(event);
	}
}

//------------------------------------------------------------------------
void CIronSight::OnLeaveZoom()
{
	ClearBlur();
	ClearDoF();

	if (IFireMode* pFireMode = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
	{
		pFireMode->OnZoomStateChanged();
	}

	if(m_pWeapon->IsOwnerClient())
	{
		if(m_zoomParams->zoomParams.hide_weapon)
		{
			if (ICharacterInstance* pCharacter = m_pWeapon->GetEntity()->GetCharacter(0))
			{
				pCharacter->HideMaster(0);
			}
		}
	}

	IView* pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
	if (pView)
	{
		pView->SetZoomedScale(1.0f);
	}
}

//------------------------------------------------------------------------
void CIronSight::OnZoomedOut()
{
	SetIsZoomed(false);
	ClearDoF();

	//Reset spread and recoil modifications
	ResetZoomMod(m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()));

	if(m_pWeapon->IsOwnerClient())
	{
		CCCPOINT(PlayerWeapon_IronSightOff);

		if(g_pGame->GetScreenEffects())
			g_pGame->GetScreenEffects()->ProcessZoomOutEffect();
	}

	if(m_zoomParams->zoomParams.scope_mode)
		ResetNearFov();

	m_stable = false;

	m_currentZoomFoVScale = 1.0f;
	m_zoomTimer = 0.0f;
	m_zoomTime = 0.0f;

	EndStabilize();

	SHUDEvent event(eHUDEvent_OnSuitPowerSpecial);
	event.AddData(SHUDEventData(-1.f));
	event.AddData(SHUDEventData(false));
	CHUDEventDispatcher::CallEvent(event);

	if (IFireMode* pFireMode = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
	{
		pFireMode->OnZoomStateChanged();
	}

	m_pWeapon->OnZoomChanged(false, m_pWeapon->GetCurrentZoomMode());
}

//------------------------------------------------------------------------
void CIronSight::OnZoomStep(bool zoomingIn, float t)
{
}

//------------------------------------------------------------------------
void CIronSight::Serialize(TSerialize ser)
{
}

//------------------------------------------------------------------------
void CIronSight::GetFPOffset(QuatT &offset) const
{
	offset.t = m_zoomParams->zoomParams.fp_offset;
	offset.q = m_zoomParams->zoomParams.fp_rot_offset;
}

//------------------------------------------------------------------------
void CIronSight::SetActorFoVScale(const float fovScale)
{
	if (m_pWeapon->IsOwnerClient())
	{
		if(CActor* pOwnerActor = m_pWeapon->GetOwnerActor())
		{
			SActorParams &actorParams = pOwnerActor->GetActorParams();
			actorParams.viewFoVScale = fovScale;
		}

		if (m_zoomParams->zoomParams.near_fov_sync)
		{
			float newNearFov = m_initialNearFov * fovScale;
			gEnv->pRenderer->EF_Query(EFQ_SetDrawNearFov, newNearFov);
		}
	}
}

//------------------------------------------------------------------------
float CIronSight::GetActorFoVScale() const
{
	CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
	if (!pOwnerActor)
	{
		IView *activeView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
		if (activeView)
		{
			const SViewParams *vp = activeView->GetCurrentParams();
			const float defaultFov = DEG2RAD(g_pGame->GetFOV());
			return vp->fov / defaultFov;
		}

		// The activeView should *nearly* always be valid, but it isn't on a dedi server.
		// Then we don't care so much if the scale isn't 100% accurate though.
		return 1.f;
	}

	return pOwnerActor->GetActorParams().viewFoVScale;
}

//------------------------------------------------------------------------
float CIronSight::GetZoomFoVScale(int step) const
{
	if (!step)
		return 1.0f;

	return 1.0f/m_zoomParams->zoomParams.stages[step-1].stage;
}

//------------------------------------------------------------------------
void CIronSight::ClearDoF()
{
	if (m_pWeapon->IsOwnerClient() && !m_pWeapon->IsModifying())
	{
		gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f, true);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZ", m_zoomParams->zoomParams.dof_shoulderMinZ, true);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMinZScale", m_zoomParams->zoomParams.dof_shoulderMinZScale, true);
		gEnv->p3DEngine->SetPostEffectParam("FilterMaskedBlurring_Amount", 0.0f);
	}
}

//------------------------------------------------------------------------
void CIronSight::ClearBlur()
{
	if (m_pWeapon->IsOwnerClient())
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterMaskedBlurring_Amount", 0.0f, true);
	}
}


//-------------------------------------------------------------------------
void CIronSight::AdjustNearFov(float time, bool zoomIn)
{
	float newFov = -1.0f;
	if(zoomIn && (m_currentStep==1))
	{
		if(time>1.0f)
			time = 1.0f;
		newFov = (m_initialNearFov*(1.0f-time))+(m_zoomParams->zoomParams.scope_nearFov*time);
	}
	else if(!zoomIn && !m_currentStep)
	{
		newFov = (m_initialNearFov*time)+(m_zoomParams->zoomParams.scope_nearFov*(1.0f-time));
		if(time>1.0f)
			newFov = m_initialNearFov;
	}
	if(newFov>0.0f && gEnv->pRenderer)
		gEnv->pRenderer->EF_Query(EFQ_SetDrawNearFov,newFov);
}

//------------------------------------------------------------------------
void CIronSight::ResetNearFov()
{
	if(m_pWeapon->IsOwnerClient())
	{
		AdjustNearFov(1.1f,false);
	}
}

//-------------------------------------------------------------------------
void CIronSight::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));
}

//--------------------------------------------------------------------------
void CIronSight::ApplyZoomMod(IFireMode* pFM, float modMultiplier)
{
	if(pFM)
	{
		pFM->PatchSpreadMod(m_zoomParams->spreadModParams, modMultiplier);
		pFM->PatchRecoilMod(m_zoomParams->recoilModParams, modMultiplier);
	}
}

void CIronSight::ResetZoomMod(IFireMode* pFM)
{
	if(pFM)
	{
		pFM->ResetSpreadMod();
		pFM->ResetRecoilMod();
	}
}

//--------------------------------------------------------------------------
bool CIronSight::IsToggle()
{
	return false;
}

//-----------------------------------------------------------------------
void CIronSight::FilterView(SViewParams &viewparams)
{
	CPlayer* pOwnerPlayer = m_pWeapon->GetOwnerPlayer();
	CRY_ASSERT_MESSAGE(pOwnerPlayer, "Ironsight:FilterView - This should never happen. This is supposed to be the player's current weapon, but it's not!");
	if(pOwnerPlayer)
	{
		const Ang3 frameSway = ZoomSway(*pOwnerPlayer, gEnv->pTimer->GetFrameTime());
		pOwnerPlayer->AddViewAngleOffsetForFrame( frameSway );
	}
}

//--------------------------------------------------------------------------
void CIronSight::StartStabilize()
{
	if (!IsZoomed())
		return;
	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if (!pOwner)
		return;

	m_stable = true;

	SHUDEvent event(eHUDEvent_OnSuitPowerSpecial);
	event.AddData(SHUDEventData(-1.f));
	event.AddData(SHUDEventData(true));
	CHUDEventDispatcher::CallEvent(event);

	if (pOwner->IsPlayer())
	{
		CAudioSignalPlayer::JustPlay("PlayerFeedback_StabilizeOn");
		if (m_stabilizeAudioLoop.GetSignalID() == INVALID_AUDIOSIGNAL_ID)
			m_stabilizeAudioLoop.SetSignal("PlayerFeedback_StabilizeLoop");
		//m_stabilizeAudioLoop.Play();
	}
}

//--------------------------------------------------------------------------
void CIronSight::EndStabilize()
{
	if (!m_stable)
		return;
	SHUDEvent event(eHUDEvent_OnSuitPowerSpecial);
	event.AddData(SHUDEventData(-1.f));
	event.AddData(SHUDEventData(false));
	CHUDEventDispatcher::CallEvent(event);
	m_stable = false;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if (pOwner && pOwner->IsPlayer())
	{
		CAudioSignalPlayer::JustPlay("PlayerFeedback_StabilizeOff");
		//m_stabilizeAudioLoop.Stop();
		REINST("needs verification!");
	}
}

//--------------------------------------------------------------------------
bool CIronSight::IsStable()
{
	return m_stable;
}

//--------------------------------------------------------------------------
Ang3 CIronSight::ZoomSway(CPlayer& clientPlayer, float time)
{
	static bool  firing = false;

	bool wasFiring = firing;

	// Update while not firing...
	if(IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
	{
		firing = pFM->IsFiring();
	}

	// Reset cycle after firing
	if(wasFiring && !firing && !m_stable)
	{
		m_swayTime = m_zoomParams->zoomSway.stabilizeTime * (1.0f - m_zoomParams->zoomSway.scaleAfterFiring);
	}

	// Just a simple sin/cos function
	const float dtX = m_swayX.Update(time);
	const float dtY = m_swayY.Update(time);

	// Stance mods
	float stanceScale = 1.0f;
	if(clientPlayer.GetStance() == STANCE_CROUCH)
	{
		stanceScale = m_zoomParams->zoomSway.crouchScale;
	}

	// Time factor
	const float minScale = m_stable ? m_zoomParams->zoomSway.holdBreathScale : m_zoomParams->zoomSway.minScale;
	float factor = minScale;
	const float settleTime = m_zoomParams->zoomSway.stabilizeTime;
	if(m_swayTime < settleTime)
	{
		factor = (settleTime-m_swayTime)/settleTime;
		if (factor < minScale)
		{
			factor = minScale;
		}
		else
		{
			const float timeMultiplier = m_stable ? (m_zoomParams->zoomSway.stabilizeTime / m_zoomParams->zoomSway.holdBreathTime) : 1.0f;
			m_swayTime += time * timeMultiplier;
		}
	}

	if (m_leavingstability)
	{
		const float transitionTime = 0.5f;
		m_stableTime += time;
		factor = LERP(m_zoomParams->zoomSway.holdBreathScale, factor, m_stableTime/transitionTime);
		m_leavingstability = !(m_stableTime > transitionTime);
	}

	// Final displacement
	const float swayAmount = factor * stanceScale;
	const float x = dtX * m_zoomParams->zoomSway.maxX * swayAmount;
	const float y = dtY * m_zoomParams->zoomSway.maxY * swayAmount;

	return Ang3(x, 0.0f, y);
}

//======================================================
void CIronSight::PostFilterView(SViewParams & viewparams)
{
	if(m_zoomParams->zoomParams.scope_mode)
	{
		if(IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
		{
			Vec3 front = viewparams.rotation.GetColumn1();
			if(pFM->IsFiring())
			{
				float currentRecoil = pFM->GetRecoil();
				if(currentRecoil>1.5f)
					currentRecoil = 1.5f + cry_random(-1.25f,0.65f);
				else if(currentRecoil>0.6f)
					currentRecoil = currentRecoil + cry_random(-0.4f,0.3f);

				const float scale = 0.01f * currentRecoil;
				front *=scale;
				viewparams.position += front;

				m_lastRecoil = currentRecoil;
			}
			else
			{
				const float decay = 75.0f;
				const float currentRecoil = m_lastRecoil - (decay*gEnv->pTimer->GetFrameTime());
				const float scale = clamp_tpl(0.005f * currentRecoil, 0.0f, 1.0f);
				front *= scale;
				viewparams.position += front;

				m_lastRecoil = max(0.0f,currentRecoil);
			}
		}
	}
}

//===================================================
int CIronSight::GetMaxZoomSteps() const
{
	return m_zoomParams->zoomParams.stages.size();
}

//===================================================
float CIronSight::GetZoomInTime() const
{
	//Returns the left time to reach "zoomed state"
	return m_zoomTimer;
}

//===================================================
float CIronSight::GetZoomTransition() const
{
	CRY_ASSERT_MESSAGE(!(m_zoomingIn && (m_currentStep == 0)), "GetZoomTransition zooming in whilst on step 0, this should never happen!");

	const int zoomStep = m_zoomingIn ? -1 : 1;
	const int prevStep = m_currentStep + zoomStep;
	const float bonusScale = GetZoomBonusScale();
	const float prevScale = GetZoomFoVScale(prevStep) * bonusScale;
	const float curScale = GetZoomFoVScale(m_currentStep) * bonusScale;
	if (prevScale != curScale)
	{
		const float minScale = min(prevScale, curScale);
		const float maxScale = max(prevScale, curScale);

		const float t = 1.0f - (((m_currentZoomFoVScale * bonusScale) - minScale) / (maxScale - minScale));

		return clamp_tpl(t, 0.0f, 1.0f);
	}
	else
	{
		return 0.0f;
	}
}

//===================================================
bool CIronSight::AllowsZoomSnap() const
{
	return m_zoomParams->zoomParams.target_snap_enabled;
}

//===================================================
float CIronSight::GetModifiedZoomInTime()
{
	return m_zoomParams->zoomParams.zoom_in_time * m_pWeapon->GetZoomTimeMultiplier();
}

//===================================================
int CIronSight::GetCurrentStep() const
{
	return m_currentStep;
}

//===================================================
void CIronSight::UpdateBonusZoomScale(float frameTime)
{
	if (m_zoomParams->zoomParams.armor_bonus_zoom == false)
	{
		m_currentZoomBonusScale = 1.0f;
		return;
	}

	float zoomScale = 1.0f;

	const float deltaZoomScale = 1.0f - zoomScale;
	m_currentZoomBonusScale += frameTime * (deltaZoomScale / m_zoomParams->zoomParams.zoom_out_time);
	m_currentZoomBonusScale = clamp_tpl(m_currentZoomBonusScale, zoomScale, 1.0f);
}

//===================================================
float CIronSight::GetZoomFovScale() const
{
	return m_currentZoomFoVScale;
}

//===================================================
float CIronSight::GetZoomBonusScale() const
{
	return m_currentZoomBonusScale;
}

//===================================================
IMaterial* CIronSight::GetCrossHairMaterial() const
{
	if(m_zoomParams->scopeParams.helper_name.empty())
		return NULL;

	IMaterial* pCrosshairMaterial = NULL;

	if(IMaterial* pMaterial = m_pWeapon->GetCharacterAttachmentMaterial(eIGS_FirstPerson, m_zoomParams->scopeParams.helper_name.c_str()))
	{
		for (int slot = 0; slot < pMaterial->GetSubMtlCount(); ++slot)
		{
			IMaterial* pSubMat = pMaterial->GetSubMtl(slot);
			if (strcmp(pSubMat->GetName(), m_zoomParams->scopeParams.crosshair_sub_material.c_str())==0)
			{
				pCrosshairMaterial = pSubMat;
				break;
			}
		}
	}

	return pCrosshairMaterial;
}

float CIronSight::GetStageMovementModifier() const
{
	const int currentStep = CLAMP(m_currentStep - 1, 0, (int)m_zoomParams->zoomParams.stages.size() - 1);

	return GetShared()->zoomParams.stages[currentStep].movementSpeedScale;
}

float CIronSight::GetStageRotationModifier() const
{
	const int currentStep = CLAMP(m_currentStep - 1, 0, (int)m_zoomParams->zoomParams.stages.size() - 1);

	return GetShared()->zoomParams.stages[currentStep].rotationSpeedScale;
}
