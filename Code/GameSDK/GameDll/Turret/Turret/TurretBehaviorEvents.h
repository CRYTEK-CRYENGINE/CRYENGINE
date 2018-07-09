// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TURRET_BEHAVIOR_EVENTS__H__
#define __TURRET_BEHAVIOR_EVENTS__H__

#include "State.h"

struct HitInfo;

enum ETurretStates
{
	eTurretState_Behavior = STATE_FIRST,
};


enum ETurretBehaviorEvent
{
	STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE = STATE_EVENT_CUSTOM,
	STATE_EVENT_TURRET_HIT,
	STATE_EVENT_TURRET_FORCE_STATE,
	STATE_EVENT_TURRET_HACK_FAIL,
};



struct SStateEventPrePhysicsUpdate
	: public SStateEvent
{
	SStateEventPrePhysicsUpdate( const float frameTimeSeconds )
		: SStateEvent( STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE )
	{
		AddData( frameTimeSeconds );
	}

	float GetFrameTimeSeconds() const { return GetData( 0 ).GetFloat(); }
};



struct SStateEventHit
	: public SStateEvent
{
	SStateEventHit( const HitInfo* pHit )
		: SStateEvent( STATE_EVENT_TURRET_HIT )
	{
		CRY_ASSERT( pHit );
		AddData( pHit );
	}

	const HitInfo* GetHit() const { return static_cast< const HitInfo* >( GetData( 0 ).GetPtr() ); }
};



struct SStateEventForceState
	: public SStateEvent
{
	SStateEventForceState( ETurretBehaviorState forcedState )
		: SStateEvent( STATE_EVENT_TURRET_FORCE_STATE )
	{
		AddData( forcedState );
	}

	ETurretBehaviorState GetForcedState() const { return static_cast< ETurretBehaviorState >( GetData( 0 ).GetInt() ); }
};



struct SStateEventHackFail
	: public SStateEvent
{
	SStateEventHackFail( const EntityId hackerEntityId )
		: SStateEvent( STATE_EVENT_TURRET_HACK_FAIL )
	{
		AddData( static_cast< int >( hackerEntityId ) );
	}

	EntityId GetHackerEntityId() const { return static_cast< EntityId >( GetData( 0 ).GetInt() ); }
};

#endif
