// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:12:2005   14:01 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Binocular.h"
#include "GameActions.h"

#include <IActorSystem.h>
#include <IMovementController.h>

#include "Game.h"
#include "GameInputActionHandlers.h"
#include "Audio/GameAudio.h"
#include "Player.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "GameCVars.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

CBinocular::CBinocular()
{
	CGameInputActionHandlers::TBinocularActionHandler& binocularActionHandler = g_pGame->GetGameInputActionHandlers().GetCBinocularActionHandler();

	if(binocularActionHandler.GetNumHandlers() == 0)
	{

#define ADD_HANDLER(action, func) binocularActionHandler.AddHandler(actions.action, &CBinocular::func)
		const CGameActions& actions = g_pGame->Actions();
		ADD_HANDLER(zoom, OnActionChangeZoom);
		ADD_HANDLER(attack2_xi, OnActionChangeZoom);
		ADD_HANDLER(sprint, OnActionSprint);
		ADD_HANDLER(sprint_xi, OnActionSprint);
		ADD_HANDLER(special, OnActionSpecial);
		ADD_HANDLER(zoom_in, TrumpAction);
		ADD_HANDLER(zoom, OnActionChangeZoom);
		ADD_HANDLER(stabilize, TrumpAction);
		ADD_HANDLER(zoom_toggle, TrumpAction);
#undef ADD_HANDLER

	}
}



CBinocular::~CBinocular()
{
}



void CBinocular::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CGameInputActionHandlers::TBinocularActionHandler& binocularActionHandler = g_pGame->GetGameInputActionHandlers().GetCBinocularActionHandler();

	if(!binocularActionHandler.Dispatch(this,actorId,actionId,activationMode,value))
	{
		CWeapon::OnAction(actorId, actionId, activationMode, value);
	}
}



void CBinocular::Select(bool select)
{
	CWeapon::Select(select);

	CActor* pOwnerActor = GetOwnerActor();
	bool isClient = pOwnerActor ? pOwnerActor->IsClient() : false;

	if (isClient)
	{
		SwitchSoundAttenuation(*pOwnerActor, select ? 0.15f : 0.0f);

		if (select)
		{
			SetBusy(false);

			if (m_zm)
				m_zm->StartZoom();
			
			pOwnerActor->LockInteractor(GetEntityId(), true);
		}
		else
		{
			pOwnerActor->LockInteractor(GetEntityId(), false);
		}

	}
}



void CBinocular::UpdateFPView(float frameTime)
{
	CWeapon::UpdateFPView(frameTime);

	CActor *pOwner = GetOwnerActor();
	if(pOwner && pOwner->IsClient())
	{
		UpdateSoundAttenuation(*pOwner);
	}
}



bool CBinocular::CanModify() const
{
	return false;
}



bool CBinocular::CanFire() const
{
	return true;
}

bool CBinocular::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return true;
}


void CBinocular::StartFire()
{
	CWeapon::StartFire();
}



bool CBinocular::OnActionChangeZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (m_zm == NULL)
		return true;

	const bool toggleMode = (g_pGameCVars->cl_zoomToggle != 0);
	const bool zoomAtFirstStage = (m_zm->IsZoomed() && (m_zm->GetCurrentStep() <= 1));

	if(toggleMode)
	{
		if (activationMode == eAAM_OnPress)
		{
			if(zoomAtFirstStage)
			{
				m_zm->StartZoom(false, false);
			}
			else
			{
				m_zm->ZoomOut();
			}
		}
	}
	else
	{
		if ((activationMode == eAAM_OnPress) || (activationMode == eAAM_OnHold))
		{
			if(zoomAtFirstStage)
			{
				m_zm->StartZoom(false, false);
			}
		}
		else if (activationMode == eAAM_OnRelease)
		{
			m_zm->ZoomOut();
		}
	}

	return true;
}

bool CBinocular::OnActionSprint(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		// do nothing
		return true;
	}
	
	return false;
}

void CBinocular::OnZoomIn() {}
void CBinocular::OnZoomOut() {}


bool CBinocular::TrumpAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return true;
}

bool CBinocular::AllowInteraction(EntityId interactionEntity, EInteractionType interactionType)
{
	return false;
}

bool CBinocular::ShouldDoPostSerializeReset() const
{
	return false;
}

bool CBinocular::ShouldUseSoundAttenuation( const CActor& ownerActor ) const
{
	if ((ownerActor.GetActorClass() == CPlayer::GetActorClassType()))
	{
		const  CPlayer& ownerPlayer = static_cast<const CPlayer&>(ownerActor);

		return gEnv->bMultiplayer;
	}

	return false;
}

void CBinocular::SwitchSoundAttenuation( const CActor& ownerActor, const float coneInRadians ) const
{
	REINST("needs verification!");
	/*CRY_ASSERT(ownerActor.IsClient());

	const bool turningOff = (coneInRadians == 0.0f);

	if (ShouldUseSoundAttenuation(ownerActor) || turningOff)
	{
		gEnv->pSoundSystem->CalcDirectionalAttenuation(ownerActor.GetEntity()->GetWorldPos(), ownerActor.GetViewRotation().GetColumn1(), coneInRadians);
	}*/
}

void CBinocular::UpdateSoundAttenuation( const CActor& ownerActor ) const
{
	CRY_ASSERT(ownerActor.IsClient());

	/*if (ShouldUseSoundAttenuation(ownerActor))
	{
		const bool zoomed = (m_zm != NULL) && (m_zm->IsZoomed());
		if (zoomed)
		{
			const float coneInRadians = 0.35f - (m_zm->GetCurrentStep() * 0.05f);  
			gEnv->pSoundSystem->CalcDirectionalAttenuation(ownerActor.GetEntity()->GetWorldPos(), ownerActor.GetViewRotation().GetColumn1(), coneInRadians);
		}
	}*/
}
