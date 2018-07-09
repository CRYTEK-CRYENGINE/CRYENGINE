// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: LTAG Implementation

-------------------------------------------------------------------------
History:
- 16:09:09	: Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "LTAG.h"

#include "Game.h"
#include "GameActions.h"
#include "GameInputActionHandlers.h"
#include "LTagSingle.h"
#include "Actor.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"


CLTag::CLTag()
{
	CGameInputActionHandlers::TLTagActionHandler& ltagActionHandler = g_pGame->GetGameInputActionHandlers().GetCLtagActionHandler();
	
	if(ltagActionHandler.GetNumHandlers() == 0)
	{
		const CGameActions& actions = g_pGame->Actions();
		ltagActionHandler.AddHandler(actions.weapon_change_firemode, &CLTag::OnActionSwitchFireMode);
	}
}


CLTag::~CLTag()
{

}

void CLTag::StartFire(const SProjectileLaunchParams& launchParams)
{
	if (m_fm)
	{
		m_fm->SetProjectileLaunchParams(launchParams);

		CWeapon::StartFire();
	}
}

void CLTag::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CGameInputActionHandlers::TLTagActionHandler& ltagActionHandler = g_pGame->GetGameInputActionHandlers().GetCLtagActionHandler();

	if(!ltagActionHandler.Dispatch(this,actorId,actionId,activationMode,value))
	{
		CWeapon::OnAction(actorId, actionId, activationMode, value);
	}
}

bool CLTag::OnActionSwitchFireMode(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode != eAAM_OnPress)
		return true;
	StartChangeFireMode();
	return true;
}

void CLTag::ProcessEvent(const SEntityEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (event.event == ENTITY_EVENT_ANIM_EVENT)
	{
		const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
		ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
		if (pAnimEvent && pCharacter)
		{
			AnimationEvent(pCharacter, *pAnimEvent);
		}
	}
	else
	{
		inherited::ProcessEvent(event);
	}
}

void CLTag::StartChangeFireMode()
{
	if (m_fm == NULL)
		return;

	assert(crygti_isof<CLTagSingle>(m_fm));

	CLTagSingle* pLTagFireMode = static_cast<CLTagSingle*>(m_fm);
	bool changed = pLTagFireMode->NextGrenadeType();

	if(changed)
	{
		CWeapon::OnFireModeChanged(m_firemode);

		SHUDEvent event(eHUDEvent_OnWeaponFireModeChanged);
		event.AddData(SHUDEventData(m_firemode));
		CHUDEventDispatcher::CallEvent(event);
	}
}

void CLTag::AnimationEvent( ICharacterInstance *pCharacter, const AnimEventInstance &event )
{
	if(s_animationEventsTable.m_ltagUpdateGrenades == event.m_EventNameLowercaseCRC32)
	{
		UpdateGrenades();
	}
}

void CLTag::UpdateGrenades()
{
	if (!m_fm)
		return;

	ICharacterInstance* pWeaponCharacter = GetEntity()->GetCharacter(eIGS_FirstPerson);
	if (!pWeaponCharacter)
		return;

	const char* newChell = "newShell";
	const char* currentShell = "currentShell";
	const int ammoCount = GetAmmoCount(m_fm->GetAmmoType());

	HideGrenadeAttachment(pWeaponCharacter, currentShell, ammoCount <= 2);
	HideGrenadeAttachment(pWeaponCharacter, newChell, ammoCount <= 1);
}

void CLTag::HideGrenadeAttachment( ICharacterInstance* pWeaponCharacter, const char* attachmentName, bool hide )
{
	CRY_ASSERT(pWeaponCharacter);

	CCCPOINT_IF(string("newShell") == attachmentName && hide, ltag_hide_newShell);
	CCCPOINT_IF(string("newShell") == attachmentName && !hide, ltag_show_newShell);
	CCCPOINT_IF(string("currentShell") == attachmentName && hide, ltag_hide_currentShell);
	CCCPOINT_IF(string("currentShell") == attachmentName && !hide, ltag_show_currentShell);

	IAttachment* pAttachment = pWeaponCharacter->GetIAttachmentManager()->GetInterfaceByName(attachmentName);
	if (pAttachment)
	{
		pAttachment->HideAttachment(hide ? 1 : 0);
	}
}

void CLTag::Reset()
{
	inherited::Reset();

	UpdateGrenades();
}

void CLTag::FullSerialize( TSerialize ser )
{
	inherited::FullSerialize(ser);

	if (ser.IsReading())
	{
		UpdateGrenades();
	}

}

void CLTag::OnSelected( bool selected )
{
	inherited::OnSelected(selected);
	UpdateGrenades();
}
//CA: MP design have chosen to have only one grenade type so this is no longer necessary, and there were problems with delegate authority
//Leaving here for now in case the fickle designers change their mind again
/*
void CLTag::Select(bool select)
{
	inherited::Select(select);

	CActor* pOwner = GetOwnerActor();

	if (select && gEnv->bServer && pOwner && pOwner->IsPlayer())
	{
		INetContext *pNetContext = g_pGame->GetIGameFramework()->GetNetContext();	

		if (pNetContext)        
			pNetContext->DelegateAuthority(GetEntityId(), pOwner->GetGameObject()->GetNetChannel());     
	}   
}

bool CLTag::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	//TODO: FIX ME! Yuck, optional group is bad, but firemode can be null. Need a proper solution to client firemode serialisation
	bool optional = (m_fm && (strcmp(m_fm->GetType(), CLTagSingle::GetWeaponComponentType() == 0));
	
	if(ser.BeginOptionalGroup("firemode", optional))
	{
		CLTagSingle* pLTagFireMode = static_cast<CLTagSingle*>(m_fm);
		pLTagFireMode->NetSerialize(ser, aspect, profile, flags);
	}
	
	return inherited::NetSerialize(ser, aspect, profile, flags);
}
*/
