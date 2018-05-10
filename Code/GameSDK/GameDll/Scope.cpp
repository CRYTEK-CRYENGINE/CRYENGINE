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
#include "Scope.h"
#include "Player.h"
#include "IWorldQuery.h"
#include "GameCVars.h"

#include "Game.h"

#include "WeaponSharedParams.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Battlechatter.h"
#include "GameRules.h"
#include "UI/HUD/HUDEventWrapper.h"

CRY_IMPLEMENT_GTI(CScope, CIronSight);

//---------------------------------------------
CScope::CScope()
	: m_showTimer(-1.0f)
	, m_hideTimer(-1.0f)
{
}

//------------------------------------------------------------------------
void CScope::Update(float frameTime, uint32 frameId)
{
	CIronSight::Update(frameTime, frameId);

	if (m_showTimer > 0.0f)
	{
		m_showTimer -= frameTime;
		if (m_showTimer <= 0.0f)
		{
			m_showTimer = -1.0f;
			m_pWeapon->OnZoomOut();
			m_pWeapon->GetScopeReticule().SetMaterial(0);
		}

		m_pWeapon->RequireUpdate(eIUS_Zooming);
	}

	if (m_hideTimer > 0.0f)
	{
		m_hideTimer -= frameTime;
		if (m_hideTimer <= 0.0f)
		{
			if(m_pWeapon->IsOwnerClient())
			{
				m_hideTimer=-1.0f;

				m_pWeapon->OnZoomIn();

				m_pWeapon->GetScopeReticule().SetMaterial(GetCrossHairMaterial());

				NET_BATTLECHATTER(BC_WatchMyBack, static_cast<CPlayer*>(m_pWeapon->GetOwnerActor()));
			}

		}

		m_pWeapon->RequireUpdate(eIUS_Zooming);
	}
}

//------------------------------------------------------------------------
void CScope::Activate(bool activate)
{
	if (activate != m_activated)
	{
		if (!activate)
		{
			//Only zoom out if we need to - deleting dropped weapons will crash if OnZoomOut() is called 
			//It unhides the entity whilst the entity is being deleted and that puts a bad entity ptr in the partition grid
			if (m_pWeapon->GetZoomState() != eZS_ZoomedOut)
			{
				m_pWeapon->OnZoomOut();
			}
		}

		m_pWeapon->GetScopeReticule().SetMaterial(0);

		CIronSight::Activate(activate);
	}
}

//------------------------------------------------------------------------
void CScope::InformActorOfScope(bool active)
{
	IActor *pActor=m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsPlayer())
	{
		CPlayer *pPlayer=static_cast<CPlayer*>(pActor);
		pPlayer->GetActorStats()->isScoped = active;
	
		CCCPOINT_IF(active, PlayerWeapon_SniperScopeOn);
		CCCPOINT_IF(!active, PlayerWeapon_SniperScopeOff);
	}
}

//------------------------------------------------------------------------
void CScope::OnEnterZoom()
{
	CIronSight::OnEnterZoom();
	m_hideTimer = m_zoomParams->scopeParams.dark_in_time;
	m_showTimer = -1.0f;

	InformActorOfScope(true);
	ToggleScopeVisionMode(true, true);

	if(gEnv->bMultiplayer && m_pWeapon->IsOwnerClient() && m_pWeapon->AllowZoomToggle() && GetMaxZoomSteps() > 1)
	{
		SHUDEventWrapper::InteractionRequest(true, "@ui_prompt_toggleZoom", "zoom_toggle", "player", -1.0f);
	}
}

//------------------------------------------------------------------------
void CScope::OnLeaveZoom()
{
	CIronSight::OnLeaveZoom();
	m_showTimer = m_zoomParams->scopeParams.dark_out_time;
	m_hideTimer = -1.0f;

	InformActorOfScope(false);

	if(gEnv->bMultiplayer && GetMaxZoomSteps() > 1 && m_pWeapon->AllowZoomToggle() && m_pWeapon->IsOwnerClient() )
	{
		SHUDEventWrapper::ClearInteractionRequest();
	}
}

//------------------------------------------------------------------------
void CScope::OnZoomStep(bool zoomingIn, float t)
{
	CIronSight::OnZoomStep(zoomingIn, t);

	if (!m_zoomingIn)
	{
		ToggleScopeVisionMode(false, false);
	}
}

//------------------------------------------------------------------------
void CScope::OnZoomedOut()
{
	CIronSight::OnZoomedOut();
	ToggleScopeVisionMode(false, true);
}

//-------------------------------------------------------------
void CScope::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));
}

//-------------------------------------------------------------
void CScope::ToggleScopeVisionMode(bool enable, bool toggleOffscreen)
{
}
