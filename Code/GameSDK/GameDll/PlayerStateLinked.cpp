// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/**********************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

The HSM Linked state for the player.

-------------------------------------------------------------------------
History:
- 10.01.12: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "State.h"

#include "PlayerStateEvents.h"
#include "Player.h"
#include "Weapon.h"

class CPlayerStateLinked : public CStateHierarchy<CPlayer>
{
	DECLARE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateLinked )
		DECLARE_STATE_CLASS_ADD( CPlayer, Enter )
		DECLARE_STATE_CLASS_ADD( CPlayer, Vehicle )
		DECLARE_STATE_CLASS_ADD( CPlayer, Entity )
	DECLARE_STATE_CLASS_END( CPlayer )
};

DEFINE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateLinked, PLAYER_STATE_LINKED, Enter )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateLinked, Enter, Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateLinked, Vehicle, Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateLinked, Entity, Root )
DEFINE_STATE_CLASS_END( CPlayer, CPlayerStateLinked )

const CPlayerStateLinked::TStateIndex CPlayerStateLinked::Root( CPlayer& player, const SStateEvent& event )
{
	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case PLAYER_EVENT_DETACH:
		// Dammit! With the new AIMOVEMENT hstate, you have to transition back to the correct movement hstate!
		// I guess this might actually be ok, so long as the AI and Players need the same
		// vehicle behaviour.
		// Ultimately, Sven expects there to be a seperate AILINKED hstate, so hopefully this is short lived.
		RequestTransitionState( player, player.IsAIControlled() ? PLAYER_STATE_AIMOVEMENT : PLAYER_STATE_MOVEMENT );
		break;
	case STATE_EVENT_RELEASE:
		player.m_playerStateSwim_WaterTestProxy.Reset(true);
		break;
	case PLAYER_EVENT_WEAPONCHANGED:
		{
			const CWeapon* pWeapon = static_cast<const CWeapon*>(event.GetData(0).GetPtr());
			if( pWeapon && pWeapon->IsHeavyWeapon() )
			{
				m_flags.AddFlags( EPlayerStateFlags_CurrentItemIsHeavy );
			}
			else
			{
				m_flags.ClearFlags( EPlayerStateFlags_CurrentItemIsHeavy );
			}
		}
		break;
	case PLAYER_EVENT_DEAD:
		RequestTransitionState( player, player.IsAIControlled() ? PLAYER_STATE_AIMOVEMENT : PLAYER_STATE_MOVEMENT, PLAYER_EVENT_DEAD );
		break;
	}
	return State_Continue;
}

const CPlayerStateLinked::TStateIndex CPlayerStateLinked::Enter( CPlayer& player, const SStateEvent& event )
{
	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case PLAYER_EVENT_ATTACH:
		if( player.GetLinkedVehicle() )
		{
			return State_Vehicle;
		}
		return State_Entity;
	}
	return State_Continue;
}

const CPlayerStateLinked::TStateIndex CPlayerStateLinked::Vehicle( CPlayer& player, const SStateEvent& event )
{
	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
	case STATE_EVENT_EXIT:
		{
			player.RefreshVisibilityState();
		}
		break;
	}
	return State_Continue;
}

const CPlayerStateLinked::TStateIndex CPlayerStateLinked::Entity( CPlayer& player, const SStateEvent& event )
{
	return State_Continue;
}
