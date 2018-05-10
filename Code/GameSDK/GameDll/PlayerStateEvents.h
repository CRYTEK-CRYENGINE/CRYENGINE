// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

List of PlayerState events.

-------------------------------------------------------------------------
History:
- 10.18.11: Created by Stephen M. North

*************************************************************************/

#pragma once

#ifndef __PlayerStateEvents_h__
#define __PlayerStateEvents_h__

#include "State.h"
#include "Player.h"
#include "ActorDefinitions.h"

enum EPlayerStates
{
	PLAYER_STATE_ENTRY = STATE_FIRST,
	PLAYER_STATE_MOVEMENT,
	PLAYER_STATE_ANIMATION,
	PLAYER_STATE_LINKED,

	PLAYER_STATE_AIMOVEMENT,
};

enum EPlayerStateEvent
{
	PLAYER_EVENT_PREPHYSICSUPDATE = STATE_EVENT_CUSTOM,
	PLAYER_EVENT_ENTRY_PLAYER,
	PLAYER_EVENT_ENTRY_AI,
	PLAYER_EVENT_UPDATE,
	PLAYER_EVENT_GROUND,
	PLAYER_EVENT_DEAD,
	PLAYER_EVENT_LEDGE,
	PLAYER_EVENT_LEDGE_ANIM_FINISHED,
	PLAYER_EVENT_SPECTATE,
	PLAYER_EVENT_SLIDE,
	PLAYER_EVENT_INPUT,
	PLAYER_EVENT_FLY,
	PLAYER_EVENT_LADDER,
	PLAYER_EVENT_LEAVELADDER,
	PLAYER_EVENT_LADDERPOSITION,
	PLAYER_EVENT_WEAPONCHANGED,
	PLAYER_EVENT_SLIDEKICK,
	PLAYER_EVENT_EXITSLIDE,
	PLAYER_EVENT_LAZY_EXITSLIDE,
	PLAYER_EVENT_FORCEEXITSLIDE,
	PLAYER_EVENT_RESET_SPECTATOR_SCREEN,
	PLAYER_EVENT_INTERACTIVE_ACTION,
	PLAYER_EVENT_ATTACH,
	PLAYER_EVENT_DETACH,
	PLAYER_EVENT_STANCE_CHANGED,
	PLAYER_EVENT_COOP_ANIMATION_FINISHED,
	PLAYER_EVENT_STEALTHKILL,
	PLAYER_EVENT_FALL,
	PLAYER_EVENT_JUMP,
	PLAYER_EVENT_CUTSCENE,
	PLAYER_EVENT_BECOME_LOCALPLAYER,
	PLAYER_EVENT_RAGDOLL_PHYSICALIZED,
	PLAYER_EVENT_GROUND_COLLIDER_CHANGED,
	PLAYER_EVENT_BUTTONMASHING_SEQUENCE,
	PLAYER_EVENT_BUTTONMASHING_SEQUENCE_END,
	PLAYER_EVENT_INTRO_START,
	PLAYER_EVENT_INTRO_FINISHED
};

#define ePlayerStateFlags(f) \
	f(EPlayerStateFlags_PrePhysicsUpdateAfterEnter) \
	f(EPlayerStateFlags_DoUpdate) \
	f(EPlayerStateFlags_IsUpdating) \
	f(EPlayerStateFlags_ExitingSlide) \
	f(EPlayerStateFlags_NetSlide) \
	f(EPlayerStateFlags_NetExitingSlide) \
	f(EPlayerStateFlags_CurrentItemIsHeavy) \
	f(EPlayerStateFlags_Ledge) \
	f(EPlayerStateFlags_Jump) \
	f(EPlayerStateFlags_Sprinting) \
	f(EPlayerStateFlags_SprintPressed) \
	f(EPlayerStateFlags_Sliding) \
	f(EPlayerStateFlags_Ground) \
	f(EPlayerStateFlags_Swimming) \
	f(EPlayerStateFlags_Spectator) \
	f(EPlayerStateFlags_InAir) \
	f(EPlayerStateFlags_InteractiveAction) \
	f(EPlayerStateFlags_PhysicsSaysFlying) \
	f(EPlayerStateFlags_OnLadder)

AUTOENUM_BUILDFLAGS_WITHZERO( ePlayerStateFlags, EPlayerStateFlags_None );

#define LedgeTransitionList(func) \
	func(eOLT_MidAir)             \
	func(eOLT_Falling)            \
	func(eOLT_VaultOver)          \
	func(eOLT_VaultOverIntoFall)    \
	func(eOLT_VaultOnto)          \
	func(eOLT_HighVaultOver)          \
	func(eOLT_HighVaultOverIntoFall)    \
	func(eOLT_HighVaultOnto)          \
	func(eOLT_QuickLedgeGrab)     \

struct SLedgeTransitionData
{
	SLedgeTransitionData( const uint16 ledgeId ) 
		: m_ledgeTransition(eOLT_None)
		, m_nearestGrabbableLedgeId(ledgeId)
		, m_comingFromOnGround(false)
		, m_comingFromSprint(false)
	{}

	AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EOnLedgeTransition, LedgeTransitionList, eOLT_None, eOLT_MaxTransitions);

	EOnLedgeTransition m_ledgeTransition;
	uint16	m_nearestGrabbableLedgeId;
	bool		m_comingFromOnGround;
	bool		m_comingFromSprint;
};


struct SStateEventLedge : public SStateEvent
{
	explicit SStateEventLedge( const SLedgeTransitionData& ledgeData )
		:
	SStateEvent( PLAYER_EVENT_LEDGE )
	{
		AddData( ledgeData.m_ledgeTransition );
		AddData( ledgeData.m_nearestGrabbableLedgeId );
		AddData( ledgeData.m_comingFromOnGround );
		AddData( ledgeData.m_comingFromSprint );
	}

	SLedgeTransitionData::EOnLedgeTransition GetLedgeTransition() const { return static_cast<SLedgeTransitionData::EOnLedgeTransition> (GetData(0).GetInt()); }
	uint16 GetLedgeId() const { return (uint16)GetData(1).GetInt(); }
	bool GetComingFromOnGround() const { return GetData(2).GetBool(); }
	bool GetComingFromSprint() const { return GetData(3).GetBool(); }
};

class CMelee;
struct SStateEventSlideKick : public SStateEvent
{
	SStateEventSlideKick(CMelee* pMelee ) 
		:
	SStateEvent( PLAYER_EVENT_SLIDEKICK )
	{
		AddData( pMelee );
	}

	CMelee* GetMelee() const { return (CMelee*)GetData(1).GetPtr(); }
};

struct SStateEventLadder : public SStateEvent
{
	SStateEventLadder(IEntity* pLadder) 
		:
		SStateEvent( PLAYER_EVENT_LADDER )
	{
		AddData(pLadder);
	}

	IEntity* GetLadder() const { return (IEntity*)(GetData(0).GetPtr()); }
};

struct SStateEventLeaveLadder : public SStateEvent
{
	SStateEventLeaveLadder(ELadderLeaveLocation leaveLocation) 
		:
		SStateEvent( PLAYER_EVENT_LEAVELADDER )
	{
		AddData((int)leaveLocation);
	}

	ELadderLeaveLocation GetLeaveLocation() const { return (ELadderLeaveLocation)(GetData(0).GetInt()); }
};

struct SStateEventLadderPosition : public SStateEvent
{
	SStateEventLadderPosition(float heightFrac) 
		:
		SStateEvent( PLAYER_EVENT_LADDERPOSITION )
	{
		AddData(heightFrac);
	}

	float GetHeightFrac() const { return GetData(0).GetFloat(); }
};

struct SPlayerPrePhysicsData
{
	const float m_frameTime;
	const SActorFrameMovementParams m_movement;

	SPlayerPrePhysicsData( float frameTime, const SActorFrameMovementParams& movement )
		: m_frameTime(frameTime), m_movement(movement)
	{}

private:
	// DO NOT IMPLEMENT!!!
	SPlayerPrePhysicsData();
};

struct SStateEventPlayerMovementPrePhysics : public SStateEvent
{
	SStateEventPlayerMovementPrePhysics( const SPlayerPrePhysicsData* pPrePhysicsData )
		:
	SStateEvent( PLAYER_EVENT_PREPHYSICSUPDATE )
	{
		AddData( pPrePhysicsData );
	}

	const SPlayerPrePhysicsData& GetPrePhysicsData() const { return *static_cast<const SPlayerPrePhysicsData*> (GetData(0).GetPtr()); }

private:
	// DO NOT IMPLEMENT!!!
	SStateEventPlayerMovementPrePhysics();
};

struct SStateEventUpdate : public SStateEvent
{
	SStateEventUpdate( const float frameTime )
		:
	SStateEvent( PLAYER_EVENT_UPDATE )
	{
		AddData( frameTime );
	}

	const float GetFrameTime() const { return GetData(0).GetFloat(); }

private:
	// DO NOT IMPLEMENT!!!
	SStateEventUpdate();
};

struct SStateEventFly : public SStateEvent
{
	SStateEventFly( const uint8 flyMode )
		:
	SStateEvent( PLAYER_EVENT_FLY )
	{
		AddData( flyMode );
	}

	const int GetFlyMode() const
	{
		return GetData(0).GetInt();
	}

private:
	// DO NOT IMPLEMENT!
	SStateEventFly();
};

struct SStateEventInteractiveAction : public SStateEvent
{
	SStateEventInteractiveAction(const EntityId objectId, bool bUpdateVisibility, int interactionIndex = 0, float actionSpeed = 1.0f)
		: 
	SStateEvent( PLAYER_EVENT_INTERACTIVE_ACTION )
	{
		AddData( static_cast<int> (objectId) );
		AddData( bUpdateVisibility );
		AddData( actionSpeed );
		AddData( static_cast<int> (interactionIndex) ); 
	}

	SStateEventInteractiveAction(const char* interactionName, bool bUpdateVisibility, float actionSpeed = 1.0f )
		: 
	SStateEvent( PLAYER_EVENT_INTERACTIVE_ACTION )
	{
		AddData( interactionName );
		AddData( bUpdateVisibility );
		AddData( actionSpeed );
	}
	
	int GetObjectEntityID() const
	{
		if( GetData(0).m_type == SStateEventData::eSEDT_int )
		{
			return GetData(0).GetInt();
		}
		return 0;
	}

	const char* GetObjectInteractionName() const
	{
		if( GetData(0).m_type == SStateEventData::eSEDT_voidptr )
		{
			return static_cast<const char*> (GetData(0).GetPtr());
		}
		return NULL;
	}

	int GetInteractionIndex() const
	{
		if( GetData(3).m_type == SStateEventData::eSEDT_int )
		{
			return GetData(3).GetInt();
		}
		return 0;
	}

	bool GetShouldUpdateVisibility() const
	{
		return GetData(1).GetBool();
	}

	float GetActionSpeed() const
	{
		return GetData(2).GetFloat();
	}
};

struct SInputEventData
{
	enum EInputEvent
	{
		EInputEvent_Jump = 0,
		EInputEvent_Slide,
		EInputEvent_Sprint,
		EInputEvent_ButtonMashingSequence,
	};

	SInputEventData( EInputEvent inputEvent )
		:
	m_inputEvent(inputEvent), m_entityId (0), m_actionId (""), m_activationMode (0), m_value (0.0f) {}

	SInputEventData( EInputEvent inputEvent, EntityId entityId, const ActionId& actionId, int activationMode, float value )
		:
	m_inputEvent(inputEvent), m_entityId (entityId), m_actionId (actionId), m_activationMode (activationMode), m_value (value) {}

	EInputEvent m_inputEvent;
	const EntityId m_entityId;
	const ActionId m_actionId;
	const int m_activationMode;
	const float m_value;
};

struct SStateEventPlayerInput : public SStateEvent
{
	SStateEventPlayerInput( const SInputEventData* pInputEvent )
		: SStateEvent(PLAYER_EVENT_INPUT)
	{
		AddData( pInputEvent );
	}

	const SInputEventData& GetInputEventData() const { return *static_cast<const SInputEventData*> (GetData(0).GetPtr()); }
};

struct SStateEventStanceChanged : public SStateEvent
{
	SStateEventStanceChanged( const int targetStance )
		:
	SStateEvent( PLAYER_EVENT_STANCE_CHANGED )
	{
		AddData( targetStance );
	}

	ILINE int GetStance() const { return GetData(0).GetInt(); }
};

class CActor;
struct SStateEventCoopAnim : public SStateEvent
{
	explicit SStateEventCoopAnim( EntityId targetID )
		:
	SStateEvent( PLAYER_EVENT_COOP_ANIMATION_FINISHED )
	{
		AddData( static_cast<int>(targetID) );
	}

	ILINE EntityId GetTargetEntityId() const { return static_cast<EntityId> (GetData(0).GetInt()); }
};

struct SStateEventJump : public SStateEvent
{
	enum EData
	{
		eD_PrePhysicsData = 0,
		eD_VerticalSpeedMofidier
	};

	SStateEventJump( const SPlayerPrePhysicsData& data, const float verticalSpeedModifier = 0.0f )
		:
	SStateEvent( PLAYER_EVENT_JUMP )
	{
		AddData( &data );
		AddData( verticalSpeedModifier );
	}

	ILINE const SPlayerPrePhysicsData& GetPrePhysicsEventData() const { return *static_cast<const SPlayerPrePhysicsData*> (GetData(eD_PrePhysicsData).GetPtr()); }
	ILINE float GetVerticalSpeedModifier() const { return (GetData(eD_VerticalSpeedMofidier).GetFloat()); }
};

struct SStateEventCutScene : public SStateEvent
{
	explicit SStateEventCutScene( const bool enable )
		:
	SStateEvent( PLAYER_EVENT_CUTSCENE )
	{
		AddData( enable );
	}

	ILINE bool IsEnabling() const { return (GetData(0).GetBool()); }
};

struct SStateEventGroundColliderChanged : public SStateEvent
{
	explicit SStateEventGroundColliderChanged( const bool bChanged )
		:	SStateEvent(PLAYER_EVENT_GROUND_COLLIDER_CHANGED)
	{
		AddData( bChanged );
	}

	ILINE const bool OnGround() const { return GetData(0).GetBool(); }
};

struct SStateEventButtonMashingSequence : public SStateEvent
{
	enum Type
	{
		SystemX = 0,
	};

	struct Params
	{
		EntityId targetLocationId;
		EntityId lookAtTargetId;
		float beamingTime;

		// While the player is struggling to escape from the boss, he will push 
		// himself back a certain distance (depending on the progression of
		// the button-mashing) (>= 0.0f) (in meters).
		float strugglingMovementDistance;

		// The speed with which the player will move along the 'struggling
		// line' (>= 0.0f) (in meters / second).
		float strugglingMovementSpeed;
	};

	struct CallBacks
	{
		Functor1<const float&>	onSequenceStart;
		Functor1<const float&>  onSequenceUpdate;
		Functor1<const bool&>   onSequenceEnd;
	};

	explicit SStateEventButtonMashingSequence( const Type sequenceType, const Params& params, const CallBacks& callbacks )
		:	SStateEvent(PLAYER_EVENT_BUTTONMASHING_SEQUENCE)
	{
		AddData( (int)sequenceType );
		AddData( (const void*)&params );
		AddData( (const void*)&callbacks );
	}

	ILINE const Type GetEnemyType() const { return static_cast<Type>(GetData(0).GetInt()); }
	ILINE const Params& GetParams() const { return *static_cast<const Params*>(GetData(1).GetPtr()); }
	ILINE const CallBacks& GetCallBacks() const { return *static_cast<const CallBacks*>(GetData(2).GetPtr()); }
};

#endif // __PlayerStateEvents_h__
