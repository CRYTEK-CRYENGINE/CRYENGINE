// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/**********************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

The HSM Movement state for the player.

Gotchyas:

 - If you use "SetStance" you can't also implement the
   PLAYER_EVENT_STANCE_CHANGED within the same hierarchy!
	 Use "OnSetStance" if you need this functionality!

-------------------------------------------------------------------------
History:
- 14.10.11: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "State.h"
#include "PlayerStateEvents.h"
#include "PlayerStateUtil.h"
#include "PlayerInput.h"
#include "Player.h"

#include "Weapon.h"
#include "Melee.h"

#include "PlayerStateDead.h"
#include "PlayerStateFly.h"
#include "PlayerStateGround.h"
#include "PlayerStateJump.h"
#include "SlideController.h"
#include "PlayerStateSwim.h"
#include "PlayerStateLadder.h"

#include "Effects/GameEffects/WaterEffects.h"

class CPlayerStateMovement : private CStateHierarchy<CPlayer>
{
	DECLARE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateMovement )
		DECLARE_STATE_CLASS_ADD( CPlayer, MovementRoot );
		DECLARE_STATE_CLASS_ADD( CPlayer, GroundMovement );
		DECLARE_STATE_CLASS_ADD( CPlayer, Dead );
		DECLARE_STATE_CLASS_ADD( CPlayer, Fly );
		DECLARE_STATE_CLASS_ADD( CPlayer, Ground  );
		DECLARE_STATE_CLASS_ADD( CPlayer, GroundFall  );
		DECLARE_STATE_CLASS_ADD( CPlayer, FallTest  );
		DECLARE_STATE_CLASS_ADD( CPlayer, Jump );
		DECLARE_STATE_CLASS_ADD( CPlayer, Fall );
		DECLARE_STATE_CLASS_ADD( CPlayer, Slide );
		DECLARE_STATE_CLASS_ADD( CPlayer, SlideFall );
		DECLARE_STATE_CLASS_ADD( CPlayer, Swim );
		DECLARE_STATE_CLASS_ADD( CPlayer, Spectate );
		DECLARE_STATE_CLASS_ADD( CPlayer, Intro );
		DECLARE_STATE_CLASS_ADD( CPlayer, SwimTest );
		DECLARE_STATE_CLASS_ADD( CPlayer, Ledge );
		DECLARE_STATE_CLASS_ADD( CPlayer, Ladder );
		DECLARE_STATE_CLASS_ADD_DUMMY( CPlayer, NoMovement );
		DECLARE_STATE_CLASS_ADD_DUMMY( CPlayer, GroundFallTest );
		DECLARE_STATE_CLASS_ADD_DUMMY( CPlayer, SlideFallTest );
	DECLARE_STATE_CLASS_END( CPlayer );

private:

	const TStateIndex StateGroundInput( CPlayer& player, const SInputEventData& inputEvent );
	void StateSprintInput( CPlayer& player, const SInputEventData& inputEvent );
	void ProcessSprint( CPlayer& player, const SPlayerPrePhysicsData& prePhysicsEvent );
	void OnSpecialMove( CPlayer &player, IPlayerEventListener::ESpecialMove specialMove );
	bool IsActionControllerValid( CPlayer& player ) const;

	void CreateWaterEffects();
	void ReleaseWaterEffects();
	void TriggerOutOfWaterEffectIfNeeded( const CPlayer& player );
	void UpdatePlayerStanceTag( CPlayer &player );

private:

	CPlayerStateDead m_stateDead;
	CPlayerStateFly m_stateFly;
	CPlayerStateGround m_stateGround;
	CPlayerStateJump m_stateJump;
	CSlideController m_slideController;
	CPlayerStateSwim m_stateSwim;
	CPlayerStateSpectate m_stateSpectate;
	CPlayerStateLedge m_statePlayerLedge;
	CPlayerStateLadder m_stateLadder;

	CWaterGameEffects* m_pWaterEffects;
};



DEFINE_STATE_CLASS_BEGIN(CPlayer, CPlayerStateMovement, PLAYER_STATE_MOVEMENT, Ground)
	// Constructor member initialization
	m_pWaterEffects = nullptr;
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, MovementRoot, Root )
		DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, GroundMovement, MovementRoot )
			DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, SwimTest, GroundMovement )
				DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Ground, SwimTest )
					DEFINE_STATE_CLASS_ADD_DUMMY( CPlayer, CPlayerStateMovement, GroundFallTest, FallTest, Ground )
						DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, GroundFall, GroundFallTest )
				DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Slide,						SwimTest )
					DEFINE_STATE_CLASS_ADD_DUMMY( CPlayer, CPlayerStateMovement, SlideFallTest, FallTest, Slide )
						DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, SlideFall, SlideFallTest )
					DEFINE_STATE_CLASS_ADD_DUMMY( CPlayer, CPlayerStateMovement, NoMovement, SwimTest,				MovementRoot )
			DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Jump,						NoMovement )
				DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Fall,						Jump )
		DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Swim,						MovementRoot )
		DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Ladder,					MovementRoot )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Dead,						Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Ledge,						Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Fly,							Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Spectate,				Root )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateMovement, Intro,						Root )
DEFINE_STATE_CLASS_END( CPlayer, CPlayerStateMovement );

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Root( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_INIT:
		{
			m_pWaterEffects = NULL;
			if (player.IsClient())
			{
				CreateWaterEffects();
			}
		}
		break;
	case STATE_EVENT_RELEASE:
		{
			ReleaseWaterEffects();
		}
		break;
	case STATE_EVENT_SERIALIZE:
		{
			TSerialize& serializer = static_cast<const SStateEventSerialize&> (event).GetSerializer();
			if( serializer.IsReading() )
			{
				EStance stance = STANCE_STAND;
				serializer.EnumValue( "StateStance", stance, STANCE_NULL, STANCE_LAST );
				if( stance != STANCE_NULL )
				{
					player.OnSetStance( stance );
				}
			}
			else
			{
				EStance stance = player.GetStance();
				serializer.EnumValue( "StateStance", stance, STANCE_NULL, STANCE_LAST );
			}
		}
		break;
	case PLAYER_EVENT_CUTSCENE:
		if( static_cast<const SStateEventCutScene&> (event).IsEnabling() )
		{
			RequestTransitionState( player, PLAYER_STATE_ANIMATION, event );
		}
		break;
	case PLAYER_EVENT_WEAPONCHANGED:
		{
			m_flags.ClearFlags( EPlayerStateFlags_CurrentItemIsHeavy );

			const CWeapon* pWeapon = static_cast<const CWeapon*> (event.GetData(0).GetPtr());
			if( pWeapon && pWeapon->IsHeavyWeapon() )
			{
				m_flags.AddFlags( EPlayerStateFlags_CurrentItemIsHeavy );
			}
		}
		break;
	case PLAYER_EVENT_SPECTATE:
		return State_Spectate;
	case PLAYER_EVENT_DEAD:
		return State_Dead;
	case PLAYER_EVENT_INTERACTIVE_ACTION:
		RequestTransitionState( player, PLAYER_STATE_ANIMATION, event );
		break;
	case PLAYER_EVENT_BUTTONMASHING_SEQUENCE:
		{
			RequestTransitionState( player, PLAYER_STATE_ANIMATION, event );
		}
		break;
	case PLAYER_EVENT_BECOME_LOCALPLAYER:
		{
			CreateWaterEffects();
		}
		break;
	case STATE_EVENT_DEBUG:
		{
			AUTOENUM_BUILDNAMEARRAY(stateFlags, ePlayerStateFlags);
			STATE_DEBUG_EVENT_LOG( this, event, false, state_red, "Active: StateMovement: CurrentFlags: %s", AutoEnum_GetStringFromBitfield( m_flags.GetRawFlags(), stateFlags, sizeof(stateFlags)/sizeof(char*) ).c_str() );
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::MovementRoot( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		player.SetCanTurnBody( true );
		break;
	case PLAYER_EVENT_LEDGE:
		return State_Ledge;
	case PLAYER_EVENT_LADDER:
		return State_Ladder;
	case PLAYER_EVENT_INPUT:
		return StateGroundInput( player, static_cast<const SStateEventPlayerInput&> (event).GetInputEventData() );
#ifndef _RELEASE
	case PLAYER_EVENT_FLY:
		return State_Fly;
#endif
	case PLAYER_EVENT_ATTACH:
		RequestTransitionState( player, PLAYER_STATE_LINKED, event );
		break;
	case PLAYER_EVENT_INTRO_START:
		return State_Intro;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::GroundMovement( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		CPlayerStateUtil::InitializeMoveRequest( player.GetMoveRequest() );
		player.m_stats.inAir = 0.f;
		break;
	case STATE_EVENT_EXIT:
		player.m_stats.onGround = 0.f;
		m_flags.ClearFlags( EPlayerStateFlags_Sprinting );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			player.m_stats.onGround += prePhysicsEvent.m_frameTime;

			// Only send the event if we've been flying for 2 consecutive frames - this prevents some state thrashing.
			if( player.GetActorPhysics().flags.AreAllFlagsActive(SActorPhysics::EActorPhysicsFlags_WasFlying|SActorPhysics::EActorPhysicsFlags_Flying)  )
			{
				player.StateMachineHandleEventMovement( SStateEventGroundColliderChanged( false ) );
			}

			if( CPlayerStateUtil::ShouldJump( player, prePhysicsEvent.m_movement ) )
			{
				player.StateMachineHandleEventMovement( SStateEventJump(prePhysicsEvent) );

				return State_Done;
			}

			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	case PLAYER_EVENT_SLIDE:
		return State_Slide;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Dead( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateDead.OnEnter( player );
		break;
	case STATE_EVENT_EXIT:
		m_stateDead.OnLeave( player );
		break;
	case STATE_EVENT_SERIALIZE:
		{
			TSerialize& serializer = static_cast<const SStateEventSerialize&> (event).GetSerializer();
			m_stateDead.Serialize( serializer );
		}
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			m_stateDead.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime );
		}
		break;
	case PLAYER_EVENT_UPDATE:
		{
			CPlayerStateDead::UpdateCtx updateCtx;
			updateCtx.frameTime = static_cast<const SStateEventUpdate&>(event).GetFrameTime();
			m_stateDead.OnUpdate( player,  updateCtx );
		}
		break;
	case PLAYER_EVENT_DEAD:
		return State_Done;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Fly( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateFly.OnEnter( player );
		player.m_stats.inAir = 0.0f;
		break;
	case STATE_EVENT_EXIT:
		player.m_stats.inAir = 0.0f;
		m_stateFly.OnExit( player );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			player.m_stats.inAir += prePhysicsEvent.m_frameTime;

			if( !m_stateFly.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime ) )
			{
				return State_Ground;
			}

			// Process movement the moverequest
			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		return State_Done;
#ifndef _RELEASE
	
	case PLAYER_EVENT_FLY:
		{
			const int flyMode = static_cast<const SStateEventFly&> (event).GetFlyMode();
			player.SetFlyMode(flyMode);
		}
		return State_Done;
		
#endif // _RELEASE
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Ladder( CPlayer& player, const SStateEvent& event )
{
	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_flags.AddFlags(EPlayerStateFlags_OnLadder);
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags(EPlayerStateFlags_OnLadder);
		m_stateLadder.OnExit(player);
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			if(!m_stateLadder.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime))
			{
				return State_Ground;
			}

			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	case PLAYER_EVENT_LADDER:
		{
			player.GetActorStats()->EnableStatusFlags(kActorStatus_onLadder);
			const SStateEventLadder& ladderEvent = static_cast<const SStateEventLadder&>(event);
			m_stateLadder.OnUseLadder(player, ladderEvent.GetLadder());	
		}
		break;
	case PLAYER_EVENT_LEAVELADDER:
		{
			player.GetActorStats()->DisableStatusFlags(kActorStatus_onLadder);
			const SStateEventLeaveLadder& leaveLadderEvent = static_cast<const SStateEventLeaveLadder&>(event);
			ELadderLeaveLocation leaveLoc = leaveLadderEvent.GetLeaveLocation();
			m_stateLadder.LeaveLadder(player, leaveLoc);
			if (leaveLoc == eLLL_Drop)
			{
				return State_Fall;
			}
		}
		break;
	case PLAYER_EVENT_LADDERPOSITION:
		{
			const SStateEventLadderPosition& ladderEvent = static_cast<const SStateEventLadderPosition&>(event);
			float heightFrac = ladderEvent.GetHeightFrac();
			m_stateLadder.SetHeightFrac(player, heightFrac);
		}
		break;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Intro( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case PLAYER_EVENT_GROUND:
	case PLAYER_EVENT_INTRO_FINISHED:
		return State_Ground;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Spectate( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateSpectate.OnEnter( player );
		player.SetHealth( 0.0f );
		m_flags.AddFlags( EPlayerStateFlags_Spectator );
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Spectator );
		m_stateSpectate.OnExit( player );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			if( !m_stateSpectate.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime ) )
			{
				return State_Ground;
			}

			// unsure if Spectate requires a move request
			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	case PLAYER_EVENT_UPDATE:
		{
			if(player.IsClient())
			{
				m_stateSpectate.UpdateFade(static_cast<const SStateEventUpdate&>(event).GetFrameTime());
			}
		}
		break;
	case PLAYER_EVENT_RESET_SPECTATOR_SCREEN:
		m_stateSpectate.ResetFadeParameters();
		break;
	case PLAYER_EVENT_SPECTATE:
		return State_Done;
	case PLAYER_EVENT_DEAD:
		// when we enter this state, we SetHealth(0.0f), this will trigger a transition to the Dead state without this interception.
		return State_Done;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Ground( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateGround.OnEnter( player );
		m_flags.AddFlags( EPlayerStateFlags_Ground );
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Ground );
		m_stateGround.OnExit( player );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			ProcessSprint( player, prePhysicsEvent );

			m_stateGround.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ), player.CPlayer::IsPlayer() );
		}
		break;
	case PLAYER_EVENT_JUMP:
		return State_Jump;
	case PLAYER_EVENT_STANCE_CHANGED:
		CPlayerStateUtil::ChangeStance( player, event );
		break;
	case PLAYER_EVENT_STEALTHKILL:
		RequestTransitionState( player, PLAYER_STATE_ANIMATION, event );
		break;
	case PLAYER_EVENT_FALL:
		return State_Fall;
	case PLAYER_EVENT_GROUND_COLLIDER_CHANGED:
		if( !static_cast<const SStateEventGroundColliderChanged&> (event).OnGround() )
		{
			return State_GroundFall;
		}
		break;
	}	
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::GroundFall( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case PLAYER_EVENT_GROUND_COLLIDER_CHANGED:
		if( static_cast<const SStateEventGroundColliderChanged&> (event).OnGround() )
		{
			player.m_stats.inAir = 0.0f;

			return State_Ground;
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::SlideFall( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case PLAYER_EVENT_GROUND_COLLIDER_CHANGED:
		if( static_cast<const SStateEventGroundColliderChanged&> (event).OnGround() )
		{
			player.m_stats.inAir = 0.0f;

		return State_Slide;
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::FallTest( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			
			// We're considered inAir at this point.
			//  - NOTE: this does not mean CPlayer::IsInAir() returns true.
			float inAir = player.m_stats.inAir;
			inAir += prePhysicsEvent.m_frameTime;
			player.m_stats.inAir = inAir;

			if( !CPlayerStateUtil::IsOnGround( player ) )
			{
				const float groundHeight = player.m_playerStateSwim_WaterTestProxy.GetLastRaycastResult();
				const float heightZ = player.GetEntity()->GetPos().z;

				// Only consider falling if our fall distance is sufficent - this is to prevent transitioning to the Fall state
				// due to physics erroneously telling us we're flying (due to small bumps on the terrain/ground)
				if( (heightZ - groundHeight) > g_pGameCVars->pl_fallHeight )
				{
					// Only fall if we've been falling for a sufficent amount of time - this is for gameplay feel reasons.
					if( inAir > g_pGameCVars->pl_movement.ground_timeInAirToFall )
					{
						player.StateMachineHandleEventMovement( PLAYER_EVENT_FALL );
						return State_Done;
					}
				}

				// use the swim proxy to get the ground height as we're periodically casting that ray anyway.
				player.m_playerStateSwim_WaterTestProxy.ForceUpdateBottomLevel(player);
			}
			else
			{
				// We're back on the ground, so send the collider changed event.
				player.StateMachineHandleEventMovement( SStateEventGroundColliderChanged( true ) );
			}
		}
		break;
	}

	return State_Continue;
}


const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Jump( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateJump.OnEnter( player );
		m_flags.AddFlags( EPlayerStateFlags_InAir|EPlayerStateFlags_Jump );
		m_flags.ClearFlags( EPlayerStateFlags_Sprinting );
		break;
	case STATE_EVENT_EXIT:
		{
			m_flags.ClearFlags( 
				EPlayerStateFlags_InAir|
				EPlayerStateFlags_Jump|
				EPlayerStateFlags_Sprinting );
			m_stateJump.OnExit( player, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ) );
		}
		break;
	case PLAYER_EVENT_JUMP:
		{
			const float fVerticalSpeedModifier = static_cast<const SStateEventJump&> (event).GetVerticalSpeedModifier();
			m_stateJump.OnJump( player, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ), fVerticalSpeedModifier );

			const SPlayerPrePhysicsData& data = static_cast<const SStateEventJump&> (event).GetPrePhysicsEventData();
			player.StateMachineHandleEventMovement( SStateEventPlayerMovementPrePhysics(&data) );
		}
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			player.m_stats.inAir += prePhysicsEvent.m_frameTime;

			if( m_stateJump.OnPrePhysicsUpdate( player, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ), prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime ) )
			{
				return State_Ground;
			}

			ProcessSprint( player, prePhysicsEvent );

			SLedgeTransitionData ledge(LedgeId::invalid_id);
			if( CPlayerStateLedge::TryLedgeGrab( player, m_stateJump.GetExpectedJumpEndHeight(), m_stateJump.GetStartFallingHeight(), m_stateJump.GetSprintJump(), &ledge, player.m_jumpButtonIsPressed ) )
			{
				ledge.m_comingFromOnGround=false; // this definitely true here?
				ledge.m_comingFromSprint=m_flags.AreAnyFlagsActive(EPlayerStateFlags_Sprinting); 

				SStateEventLedge ledgeEvent( ledge );
				player.StateMachineHandleEventMovement( ledgeEvent );
			}

			player.m_playerStateSwim_WaterTestProxy.ForceUpdateBottomLevel(player);

			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		return State_Continue;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Fall( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateJump.OnEnter( player );
		m_stateJump.OnFall( player );
		m_flags.AddFlags( EPlayerStateFlags_InAir );
		return State_Done;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Slide( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		player.SetCanTurnBody( false );
		m_slideController.GoToState( player, eSlideState_Sliding );
		m_flags.AddFlags( EPlayerStateFlags_Sliding );
		m_flags.ClearFlags( EPlayerStateFlags_Sprinting );
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( 
			EPlayerStateFlags_Sprinting|
			EPlayerStateFlags_ExitingSlide|
			EPlayerStateFlags_NetExitingSlide|
			EPlayerStateFlags_NetSlide|
			EPlayerStateFlags_Sliding );
		m_slideController.GoToState( player, eSlideState_None );
		player.SetCanTurnBody( true );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			m_slideController.Update( player, prePhysicsEvent.m_frameTime, prePhysicsEvent.m_movement, m_flags.AreAnyFlagsActive(EPlayerStateFlags_NetSlide), m_flags.AreAnyFlagsActive(EPlayerStateFlags_NetExitingSlide), player.GetMoveRequest() );

			m_slideController.GetCurrentState() == eSlideState_ExitingSlide ? m_flags.AddFlags( EPlayerStateFlags_ExitingSlide ) : m_flags.ClearFlags( EPlayerStateFlags_ExitingSlide );

			if( m_slideController.IsActive() )
			{
				return State_Continue;
			}
		}
		return State_Ground;
	case PLAYER_EVENT_SLIDEKICK:
		{
			const SStateEventSlideKick& kickData = static_cast<const SStateEventSlideKick&> (event);

			const float timer = m_slideController.DoSlideKick(player);
			kickData.GetMelee()->SetDelayTimer(timer);
		}
		break;
	case PLAYER_EVENT_EXITSLIDE:
		m_slideController.GoToState( player, eSlideState_ExitingSlide );
		return State_Ground;
	case PLAYER_EVENT_LAZY_EXITSLIDE:
		m_slideController.LazyExitSlide( player );
		break;
	case PLAYER_EVENT_FORCEEXITSLIDE:
		m_slideController.GoToState( player, eSlideState_None );
		return State_Ground;
	case PLAYER_EVENT_SLIDE:
		return State_Done;
	case PLAYER_EVENT_STANCE_CHANGED:
		CPlayerStateUtil::ChangeStance( player, event );
		break;
	case PLAYER_EVENT_FALL:
		return State_Fall;
	case PLAYER_EVENT_JUMP:
		return State_Done;
	case PLAYER_EVENT_GROUND_COLLIDER_CHANGED:
		if( !static_cast<const SStateEventGroundColliderChanged&> (event).OnGround() )
		{
			return State_SlideFall;
		}
		break;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Swim( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateSwim.OnEnter( player );
		m_flags.AddFlags( EPlayerStateFlags_Swimming );
		player.OnSetStance(STANCE_SWIM);
		player.m_stats.onGround = 0.0f;
		player.m_stats.inAir = 0.0f;
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Swimming );
		m_stateSwim.OnExit( player );
		player.OnSetStance(STANCE_STAND);

		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			ProcessSprint( player, prePhysicsEvent );

			player.m_playerStateSwim_WaterTestProxy.PreUpdateSwimming( player, prePhysicsEvent.m_frameTime );
			player.m_playerStateSwim_WaterTestProxy.Update( player, prePhysicsEvent.m_frameTime );

			TriggerOutOfWaterEffectIfNeeded( player );
			
			if( player.m_playerStateSwim_WaterTestProxy.ShouldSwim() )
			{
				float verticalSpeedModifier = 0.0f;
				if( m_stateSwim.DetectJump( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime, &verticalSpeedModifier ) )
				{
					player.StateMachineHandleEventMovement( SStateEventJump( prePhysicsEvent, verticalSpeedModifier ) );

					return State_Done;
				}
				m_stateSwim.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime );
			}
			else
			{
				return State_Ground;
			}

			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	case PLAYER_EVENT_JUMP:
		return State_Jump;
	case PLAYER_EVENT_UPDATE:
		m_stateSwim.OnUpdate( player, static_cast<const SStateEventUpdate&>(event).GetFrameTime() );
		break;
	case PLAYER_EVENT_FALL:
		return State_Fall;
	}
	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::SwimTest( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	switch( event.GetEventId() )
	{
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			player.m_playerStateSwim_WaterTestProxy.PreUpdateNotSwimming( player, prePhysicsEvent.m_frameTime );

			player.m_playerStateSwim_WaterTestProxy.Update( player, prePhysicsEvent.m_frameTime );

			TriggerOutOfWaterEffectIfNeeded( player );
			
			if( player.m_playerStateSwim_WaterTestProxy.ShouldSwim() )
			{
				return State_Swim;
			}
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::Ledge( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( !player.IsAIControlled() );

	switch( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		m_flags.AddFlags( EPlayerStateFlags_Ledge );
		player.SetLastTimeInLedge( gEnv->pTimer->GetAsyncCurTime() );
		player.SetStance(STANCE_STAND);
		if (gEnv->bMultiplayer && player.IsClient())
		{
			SHUDEvent ledgeEvent(eHUDEvent_OnUseLedge);
			ledgeEvent.AddData(true);
			CHUDEventDispatcher::CallEvent(ledgeEvent);
		}
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Ledge );
		m_statePlayerLedge.OnExit( player );
		if (gEnv->bMultiplayer && player.IsClient())
		{
			SHUDEvent ledgeEvent(eHUDEvent_OnUseLedge);
			ledgeEvent.AddData(false);
			CHUDEventDispatcher::CallEvent(ledgeEvent);
		}
		break;

	case STATE_EVENT_SERIALIZE:
		{
			TSerialize& serializer = static_cast<const SStateEventSerialize&> (event).GetSerializer();

			m_statePlayerLedge.Serialize( serializer );
		}
		break;

	case STATE_EVENT_POST_SERIALIZE:
		{
			m_statePlayerLedge.PostSerialize( player );
		}
		break;

	case PLAYER_EVENT_LEDGE:
	{
		const SStateEventLedge &ledgeEvent = static_cast<const SStateEventLedge&> (event);

		const bool comingFromSprint = ledgeEvent.GetComingFromSprint();
		if (comingFromSprint)
		{
			m_flags.AddFlags( EPlayerStateFlags_Sprinting ); // movement will have stripped this flag as it exited.. we want to carry on sprinting at the end of this sprint vault if possible
		}

		m_statePlayerLedge.OnEnter( player, ledgeEvent );
		return State_Done;
	}
	case PLAYER_EVENT_LEDGE_ANIM_FINISHED:
		m_statePlayerLedge.OnAnimFinished( player );
		break; 
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			m_statePlayerLedge.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	case PLAYER_EVENT_FALL:
		return State_Fall;
	case PLAYER_EVENT_GROUND:
		return State_Ground;
	case PLAYER_EVENT_INPUT:
		return StateGroundInput( player, static_cast<const SStateEventPlayerInput&> (event).GetInputEventData() );	// ensure any sprint input changes are updated and not lost mid ledgegrab/vault
	}

	return State_Continue;
}

const CPlayerStateMovement::TStateIndex CPlayerStateMovement::StateGroundInput( CPlayer& player, const SInputEventData& inputEvent )
{
	switch( inputEvent.m_inputEvent )
	{
	case SInputEventData::EInputEvent_Sprint:
		StateSprintInput( player, inputEvent );
		break;
	}

	return State_Continue;
}


void CPlayerStateMovement::StateSprintInput( CPlayer& player, const SInputEventData& inputEvent )
{
	CRY_ASSERT( inputEvent.m_inputEvent == SInputEventData::EInputEvent_Sprint );

	inputEvent.m_activationMode == eAAM_OnPress ? m_flags.AddFlags( EPlayerStateFlags_SprintPressed ) : m_flags.ClearFlags( EPlayerStateFlags_SprintPressed );
}

void CPlayerStateMovement::ProcessSprint( CPlayer& player, const SPlayerPrePhysicsData& prePhysicsEvent )
{

	//If sprint toggle is active, then we want to sprint if it is pressed or if we are sprinting
	//If sprint toggle is off, then we only want to sprint if it is pressed


	uint32 flagsToCheck = g_pGameCVars->cl_sprintToggle ? (EPlayerStateFlags_SprintPressed | EPlayerStateFlags_Sprinting) : (EPlayerStateFlags_SprintPressed);

	if( m_flags.AreAnyFlagsActive( flagsToCheck ) && CPlayerStateUtil::ShouldSprint( player, prePhysicsEvent.m_movement, player.GetCurrentItem() ) )
	{
		if (!player.IsSprinting())
		{
			// notify IPlayerEventListener about sprinting (just once)
			OnSpecialMove(player, IPlayerEventListener::eSM_SpeedSprint);
		}

		m_flags.AddFlags( EPlayerStateFlags_Sprinting );
		if( player.GetPlayerInput()->GetType() == IPlayerInput::PLAYER_INPUT )
		{
			static_cast<CPlayerInput*> (player.GetPlayerInput())->ClearCrouchAction();
			player.SetStance(STANCE_STAND);
		}
	}
	else
	{
		m_flags.ClearFlags( EPlayerStateFlags_Sprinting );
	}
}

void CPlayerStateMovement::OnSpecialMove( CPlayer &player, IPlayerEventListener::ESpecialMove specialMove )
{
	if (player.m_playerEventListeners.empty() == false)
	{
		CPlayer::TPlayerEventListeners::const_iterator iter = player.m_playerEventListeners.begin();
		CPlayer::TPlayerEventListeners::const_iterator cur;
		while (iter != player.m_playerEventListeners.end())
		{
			cur = iter;
			++iter;
			(*cur)->OnSpecialMove(&player, specialMove);
		}
	}
}

bool CPlayerStateMovement::IsActionControllerValid( CPlayer& player ) const
{
	IAnimatedCharacter* pAnimatedCharacter = player.GetAnimatedCharacter();

	if (pAnimatedCharacter != NULL)
	{
		return (pAnimatedCharacter->GetActionController() != NULL);
	}

	return false;
}

void CPlayerStateMovement::TriggerOutOfWaterEffectIfNeeded( const CPlayer& player )
{
	if (m_pWaterEffects != NULL)
	{
		CRY_ASSERT(player.IsClient());

		if (player.m_playerStateSwim_WaterTestProxy.IsHeadComingOutOfWater())
		{
			m_pWaterEffects->OnCameraComingOutOfWater();
		}
	}
}

void CPlayerStateMovement::CreateWaterEffects()
{
	if (m_pWaterEffects == NULL)
	{
		m_pWaterEffects = new CWaterGameEffects();
		m_pWaterEffects->Initialise();
	}
}

void CPlayerStateMovement::ReleaseWaterEffects()
{
	if (m_pWaterEffects != NULL)
	{
		m_pWaterEffects->Release();
		delete m_pWaterEffects;
		m_pWaterEffects = NULL;
	}
}

void CPlayerStateMovement::UpdatePlayerStanceTag( CPlayer &player )
{
	IAnimatedCharacter *pAnimatedCharacter = player.GetAnimatedCharacter();
	if (pAnimatedCharacter)
	{
		IActionController *pActionController =	pAnimatedCharacter->GetActionController();
		if (pActionController)
		{
			player.SetStanceTag(player.GetStance(), pActionController->GetContext().state);
		}
	}
}

