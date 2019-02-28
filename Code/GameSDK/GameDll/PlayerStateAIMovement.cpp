// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//
//  The HSM Movement state for the AI
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "PlayerStateEvents.h"
#include "PlayerStateUtil.h"
#include "PlayerInput.h"
#include "Player.h"

#include "PlayerStateDead.h"
#include "PlayerStateGround.h"
#include "PlayerStateJump.h"
#include "PlayerStateSwim.h"

#include "Weapon.h"


class CPlayerStateAIMovement : private CStateHierarchy<CPlayer>
{
	DECLARE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateAIMovement )
	DECLARE_STATE_CLASS_ADD( CPlayer, MovementRoot );
	DECLARE_STATE_CLASS_ADD( CPlayer, Movement );
	DECLARE_STATE_CLASS_ADD( CPlayer, Dead );
	DECLARE_STATE_CLASS_ADD( CPlayer, Ground  );
	DECLARE_STATE_CLASS_ADD( CPlayer, Fall );
	DECLARE_STATE_CLASS_ADD( CPlayer, Swim );
	DECLARE_STATE_CLASS_ADD( CPlayer, SwimTest );
	DECLARE_STATE_CLASS_ADD_DUMMY( CPlayer, NoMovement );
	DECLARE_STATE_CLASS_END( CPlayer );

private:

	const TStateIndex StateGroundInput( CPlayer& player, const SInputEventData& inputEvent );
	void StateSprintInput( CPlayer& player, const SInputEventData& inputEvent );
	void ProcessSprint( const CPlayer& player, const SPlayerPrePhysicsData& prePhysicsEvent );

private:

	CPlayerStateDead m_stateDead;
	CPlayerStateGround m_stateGround;
	CPlayerStateJump m_stateJump;
	CPlayerStateSwim m_stateSwim;
};

DEFINE_STATE_CLASS_BEGIN( CPlayer, CPlayerStateAIMovement, PLAYER_STATE_AIMOVEMENT, Ground )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAIMovement, MovementRoot,		Root )
		DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAIMovement, Movement,				MovementRoot )
			DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAIMovement, Ground,					Movement )
		DEFINE_STATE_CLASS_ADD_DUMMY( CPlayer, CPlayerStateAIMovement, NoMovement, SwimTest,	MovementRoot )
			DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAIMovement, Fall,						NoMovement )
		DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAIMovement, Swim,						MovementRoot )
	DEFINE_STATE_CLASS_ADD( CPlayer, CPlayerStateAIMovement, Dead,						Root )
DEFINE_STATE_CLASS_END( CPlayer, CPlayerStateAIMovement );


const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::Root( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
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
	case PLAYER_EVENT_DEAD:
		return State_Dead;
	case STATE_EVENT_DEBUG:
		{
			AUTOENUM_BUILDNAMEARRAY(stateFlags, ePlayerStateFlags);
			STATE_DEBUG_EVENT_LOG( this, event, false, state_white, "Active: StateMovement: CurrentFlags: %s", AutoEnum_GetStringFromBitfield( m_flags.GetRawFlags(), stateFlags, sizeof(stateFlags)/sizeof(char*) ).c_str() );
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::MovementRoot( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		player.SetCanTurnBody( true );
		break;
	case PLAYER_EVENT_INPUT:
		return StateGroundInput( player, static_cast<const SStateEventPlayerInput&> (event).GetInputEventData() );
	}

	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::Movement( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> ( event.GetEventId() );
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		CPlayerStateUtil::InitializeMoveRequest( player.GetMoveRequest() );
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Sprinting );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> ( event ).GetPrePhysicsData();
			player.m_stats.onGround += prePhysicsEvent.m_frameTime;

			if( !CPlayerStateUtil::IsOnGround( player ) )
			{
				// We're considered inAir at this point.
				//  - NOTE: this does not mean CPlayer::IsInAir() returns true.
				float inAir = player.m_stats.inAir;
				inAir += prePhysicsEvent.m_frameTime;
				player.m_stats.inAir = inAir;

				// We can be purely time based for AI as we can't jump or navigate to a falling position, though, we might be pushed, thrown, etc.
				if( (inAir > g_pGameCVars->pl_movement.ground_timeInAirToFall) )
				{
					player.StateMachineHandleEventMovement( PLAYER_EVENT_FALL );
				}
			}
			else
			{
				player.m_stats.inAir = 0.0f;
			}

			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::Dead( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

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

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::Ground( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateGround.OnEnter( player );
		m_flags.AddFlags( EPlayerStateFlags_Ground );
		player.m_stats.inAir = 0.0f;
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Ground );
		m_stateGround.OnExit( player );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			ProcessSprint( player, prePhysicsEvent );

			m_stateGround.OnPrePhysicsUpdate( player, prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ), false );
		}
		break;
	case PLAYER_EVENT_STANCE_CHANGED:
		{
			CPlayerStateUtil::ChangeStance( player, event );

			const SStateEventStanceChanged& stanceEvent = static_cast<const SStateEventStanceChanged&>( event );
			const EStance stance = static_cast<EStance>( stanceEvent.GetStance() );

			CAIAnimationComponent* pAiAnimationComponent = player.GetAIAnimationComponent();
			CRY_ASSERT( pAiAnimationComponent );
			pAiAnimationComponent->GetAnimationState().SetRequestedStance( stance );
		}
		break;
	case PLAYER_EVENT_FALL:
		return State_Fall;
	}	
	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::Fall( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateJump.OnEnter( player );
		m_stateJump.OnFall( player );
		m_flags.AddFlags( EPlayerStateFlags_InAir );
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( 
			EPlayerStateFlags_InAir|
			EPlayerStateFlags_Jump|
			EPlayerStateFlags_Sprinting );
		m_stateJump.OnExit( player, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ) );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();
			player.m_stats.inAir += prePhysicsEvent.m_frameTime;
			
			if( m_stateJump.OnPrePhysicsUpdate( player, m_flags.AreAnyFlagsActive( EPlayerStateFlags_CurrentItemIsHeavy ), prePhysicsEvent.m_movement, prePhysicsEvent.m_frameTime ) )
			{
				return State_Ground;
			}

			CPlayerStateUtil::ProcessTurning( player, prePhysicsEvent.m_frameTime );
			CPlayerStateUtil::FinalizeMovementRequest( player, prePhysicsEvent.m_movement, player.GetMoveRequest() );
		}
		break;
	}
	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::Swim( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	const EPlayerStateEvent eventID = static_cast<EPlayerStateEvent> (event.GetEventId());
	switch( eventID )
	{
	case STATE_EVENT_ENTER:
		m_stateSwim.OnEnter( player );
		m_flags.AddFlags( EPlayerStateFlags_Swimming );
		break;
	case STATE_EVENT_EXIT:
		m_flags.ClearFlags( EPlayerStateFlags_Swimming );
		m_stateSwim.OnExit( player );
		break;
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			ProcessSprint( player, prePhysicsEvent );

			player.m_playerStateSwim_WaterTestProxy.PreUpdateSwimming( player, prePhysicsEvent.m_frameTime );
			player.m_playerStateSwim_WaterTestProxy.Update( player, prePhysicsEvent.m_frameTime );

			if( player.m_playerStateSwim_WaterTestProxy.ShouldSwim() )
			{
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
	case PLAYER_EVENT_UPDATE:
		m_stateSwim.OnUpdate( player, static_cast<const SStateEventUpdate&>(event).GetFrameTime() );
		break;
	}
	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::SwimTest( CPlayer& player, const SStateEvent& event )
{
	CRY_ASSERT( player.IsAIControlled() );

	switch( event.GetEventId() )
	{
	case PLAYER_EVENT_PREPHYSICSUPDATE:
		{
			const SPlayerPrePhysicsData& prePhysicsEvent = static_cast<const SStateEventPlayerMovementPrePhysics&> (event).GetPrePhysicsData();

			player.m_playerStateSwim_WaterTestProxy.PreUpdateNotSwimming( player, prePhysicsEvent.m_frameTime );

			player.m_playerStateSwim_WaterTestProxy.Update( player, prePhysicsEvent.m_frameTime );

			if( player.m_playerStateSwim_WaterTestProxy.ShouldSwim() )
			{
				return State_Swim;
			}
		}
		break;
	}

	return State_Continue;
}

const CPlayerStateAIMovement::TStateIndex CPlayerStateAIMovement::StateGroundInput( CPlayer& player, const SInputEventData& inputEvent )
{
	switch( inputEvent.m_inputEvent )
	{
	case SInputEventData::EInputEvent_Sprint:
		StateSprintInput( player, inputEvent );
		break;
	}

	return State_Continue;
}

void CPlayerStateAIMovement::StateSprintInput( CPlayer& player, const SInputEventData& inputEvent )
{
	CRY_ASSERT( inputEvent.m_inputEvent == SInputEventData::EInputEvent_Sprint );
	
	m_flags.SetFlags( EPlayerStateFlags_SprintPressed, inputEvent.m_activationMode == eAAM_OnPress );
}

void CPlayerStateAIMovement::ProcessSprint( const CPlayer& player, const SPlayerPrePhysicsData& prePhysicsEvent )
{
	if( m_flags.AreAnyFlagsActive( EPlayerStateFlags_SprintPressed|EPlayerStateFlags_Sprinting ) && CPlayerStateUtil::ShouldSprint( player, prePhysicsEvent.m_movement, player.GetCurrentItem() ) )
	{
		m_flags.AddFlags( EPlayerStateFlags_Sprinting );
		if( player.GetPlayerInput()->GetType() == IPlayerInput::PLAYER_INPUT )
		{
			static_cast<CPlayerInput*> (player.GetPlayerInput())->ClearCrouchAction();
		}
	}
	else
	{
		m_flags.ClearFlags( EPlayerStateFlags_Sprinting );
	}
}
