// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: 

-------------------------------------------------------------------------
History:
- 2.9.11: Created by Stephen M. North.

*************************************************************************/
#include "StdAfx.h"
#include "InteractiveActionController.h"
#include "PlayerStateEvents.h"


class CPlayerStateAnimationControlled : public CStateHierarchy<CPlayer>
{
	DECLARE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateAnimationControlled )
		DECLARE_STATE_CLASS_ADD( CPlayer, EntryChoice )
		DECLARE_STATE_CLASS_ADD( CPlayer, InteractiveAction )
		DECLARE_STATE_CLASS_ADD( CPlayer, StealthKill )
		DECLARE_STATE_CLASS_ADD( CPlayer, CutScene )
	DECLARE_STATE_CLASS_END( CPlayer )

private:
	CInteractiveActionController m_interactiveActionController;
};

DEFINE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateAnimationControlled, PLAYER_STATE_ANIMATION, EntryChoice )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAnimationControlled, EntryChoice, Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAnimationControlled, InteractiveAction, Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAnimationControlled, StealthKill, Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAnimationControlled, CutScene, Root )
DEFINE_STATE_CLASS_END( CPlayer, CPlayerStateAnimationControlled )

const CPlayerStateAnimationControlled::TStateIndex CPlayerStateAnimationControlled::EntryChoice( CPlayer& player, const SStateEvent& event )
{
	switch( event.GetEventId() )
	{
	case PLAYER_EVENT_INTERACTIVE_ACTION:
		return State_InteractiveAction;
	case PLAYER_EVENT_STEALTHKILL:
		return State_StealthKill;
	case PLAYER_EVENT_CUTSCENE:
		if( static_cast<const SStateEventCutScene&> (event).IsEnabling() )
		{
			return State_CutScene;
		}
		else
		{
			CRY_ASSERT(false);
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateAnimationControlled::TStateIndex CPlayerStateAnimationControlled::Root( CPlayer& player, const SStateEvent& event )
{
	return State_Continue;
}

const CPlayerStateAnimationControlled::TStateIndex CPlayerStateAnimationControlled::StealthKill( CPlayer& player, const SStateEvent& event )
{
	switch( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		m_flags.AddFlags(EPlayerStateFlags_Ground);
		if(player.IsPlayer())
		{
			gEnv->pGameFramework->AllowSave( false );
		}
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags(EPlayerStateFlags_Ground);
		if(player.IsPlayer())
		{
			gEnv->pGameFramework->AllowSave( true );
		}
		break;
	case PLAYER_EVENT_COOP_ANIMATION_FINISHED:
		{
			EntityId targetEntityID = static_cast<const SStateEventCoopAnim&> (event).GetTargetEntityId();
			CActor* pTarget = static_cast<CActor*> (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( targetEntityID ));
			player.m_stealthKill.Leave( pTarget );
			if ( pTarget )
			{
				pTarget->HandleEvent( SGameObjectEvent( eCGE_ReactionEnd, eGOEF_ToExtensions ) );
			}
			RequestTransitionState( player, PLAYER_STATE_MOVEMENT );
		}
		break;
	}

	return State_Continue;
}


const CPlayerStateAnimationControlled::TStateIndex CPlayerStateAnimationControlled::InteractiveAction( CPlayer& player, const SStateEvent& event )
{
	switch( event.GetEventId() )
	{
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_InteractiveAction );

		player.GetActorStats()->animationControlledID = 0;
		player.AnimationControlled( false );

		m_interactiveActionController.OnLeave( player );

		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			m_interactiveActionController.Update( player, prePhysicsEvent.m_frameTime, prePhysicsEvent.m_movement );

			if( !m_interactiveActionController.IsInInteractiveAction() )
			{
				RequestTransitionState( player, PLAYER_STATE_MOVEMENT );
			}
		}
		break;
	case PLAYER_EVENT_INTERACTIVE_ACTION:
		{
			m_flags.AddFlags( EPlayerStateFlags_InteractiveAction );

			const SStateEventInteractiveAction& action = static_cast<const SStateEventInteractiveAction&> (event);
			if( action.GetObjectEntityID() != 0 )
			{
				m_interactiveActionController.OnEnter( player, action.GetObjectEntityID(), action.GetInteractionIndex() );
			}
			else if( action.GetObjectInteractionName() != NULL )
			{
				m_interactiveActionController.OnEnterByName( player, action.GetObjectInteractionName(), action.GetActionSpeed() );
			}
			else
			{
				// no valid data! Abort!
				RequestTransitionState( player, PLAYER_STATE_MOVEMENT );
			}

			player.GetActorStats()->animationControlledID = m_interactiveActionController.GetInteractiveObjectId();
			player.AnimationControlled( true, action.GetShouldUpdateVisibility() );
		}
		break;
	case PLAYER_EVENT_DEAD:
		if( m_interactiveActionController.IsInInteractiveAction() )
		{
			m_interactiveActionController.Abort( player );
		}

		RequestTransitionState( player, PLAYER_STATE_MOVEMENT, PLAYER_EVENT_DEAD );
		break;
	}

	return State_Continue;
}

const CPlayerStateAnimationControlled::TStateIndex CPlayerStateAnimationControlled::CutScene( CPlayer& player, const SStateEvent& event )
{
	switch( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		player.OnBeginCutScene();
		break;
	case STATE_EVENT_EXIT:
		player.OnEndCutScene();
		break;
	case PLAYER_EVENT_CUTSCENE:
		if( !static_cast<const SStateEventCutScene&> (event).IsEnabling() )
		{
			RequestTransitionState( player, PLAYER_STATE_MOVEMENT );
		}
		break;
	}
	return State_Continue;
}
