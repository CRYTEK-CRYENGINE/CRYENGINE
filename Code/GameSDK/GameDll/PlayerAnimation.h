// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: Helper interface for player animation control

-------------------------------------------------------------------------
History:
- 08.9.11: Created by Tom Berry

*************************************************************************/

#ifndef __PLAYERANIMATION_H__
#define __PLAYERANIMATION_H__

#include "ICryMannequin.h"

typedef TAction<SAnimationContext> TPlayerAction;

enum EPlayerActionPriority
{
	PP_Lowest,
	PP_Movement,
	PP_MovementAction,
	PP_PlayerAction,
	PP_PlayerActionUrgent,
	PP_HitReaction,
	PP_Death,
	PP_PostDeath
};


#define MAN_PLAYER_FRAGMENTS( f ) \
	f( MotionIdle ) \
	f( MotionTurn ) \
	f( MotionMovement ) \
	f( MotionJump ) \
	f( MotionInAir ) \
	f( MotionMounted ) \
	f( FPNone ) \
	f( FPSway ) \
	f( FPIdle ) \
	f( FPMovement ) \
	f( FPLand ) \
	f( aimPose ) \
	f( idlePose ) \
	f( ledgeGrab ) \
	f( LadderClimb ) \
	f( LadderGetOn ) \
	f( LadderGetOff ) \
	f( idle ) \
	f( idle_break ) \
	f( slide ) \
	f( slidingKick ) \
	f( bump ) \
	f( interact ) \
	f( melee ) \
	f( melee_multipart ) \
	f( melee_weapon ) \
	f( melee_interact ) \
	f( hitDeath ) \
	f( animationControlled ) \
	f( weaponPose ) \

#define MAN_PLAYER_CONTEXTS( f ) \
	f( Char1P ) \
	f( Char3P ) \
	f( Weapon ) \
	f( attachment_top ) \
	f( attachment_bottom ) \
	f( SlaveChar ) \
	f( SlaveObject )

#define MAN_PLAYER_TAGS( f ) \
	f( crouch ) \
	f( swim ) \
	f( underwater ) \
	f( jump ) \
	f( move ) \
	f( sprint ) \
	f( outOfAmmo ) \
	f( aiming ) \
	f( forward ) \
	f( backward ) \
	f( left ) \
	f( right ) \
	f( nw ) \
	f( MP ) \
	f( SP ) \
	f( FP ) \
	f( localClient ) \
	f( throwing ) \
	f( slaveHuman ) \
	f( BowPrime )

#define MAN_PLAYER_TAG_GROUPS( f ) \
	f( stance ) \
	f( weaponType ) \
	f( item ) \
	f( zoom ) \
	f( firemode ) \
	f( moveDir ) \
	f( moveSpeed ) \
	f( scope_attachment ) \
	f( barrel_attachment ) \
	f( underbarrel_attachment ) \
	f( melee_type ) \
	f( playMode ) \
	f( mp_enviro ) \

#define MAN_PLAYER_SCOPES( f ) \
	f( FullBody1P ) \
	f( FullBody3P ) \
	f( Motion1P ) \
	f( AimPose ) \
	f( Torso1P ) \
	f( Torso3P ) \
	f( Weapon ) \
	f( AttachmentBottom ) \
	f( SlaveChar ) \
	f( SlaveObject )

#define MAN_MELEE_FRAGMENT_TAGS( f ) \
	f( into ) \
	f( hit ) \
	f( miss )

#define MAN_MELEEPOLE_FRAGMENT_TAGS( f ) \
	f( secondSwing )

#define MAN_LEDGEGRAB_FRAGMENT_TAGS( f ) \
	f( floor ) \
	f( fall ) \
	f( ledge ) \
	f( vault ) \
	f( quick ) \
	f( up ) \
	f( over ) \
	f( drop ) \
	f( endCrouched ) \
	f( floorSprint ) \
	f( high )

#define MAN_PLAYER_FRAGMENT_TAGS( f ) \
	f( melee, MAN_MELEEPOLE_FRAGMENT_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST ) \
	f( melee_multipart, MAN_MELEE_FRAGMENT_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST ) \
	f( ledgeGrab, MAN_LEDGEGRAB_FRAGMENT_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST ) \

MANNEQUIN_USER_PARAMS( SMannequinPlayerParams, MAN_PLAYER_FRAGMENTS, MAN_PLAYER_TAGS, MAN_PLAYER_TAG_GROUPS, MAN_PLAYER_SCOPES, MAN_PLAYER_CONTEXTS, MAN_PLAYER_FRAGMENT_TAGS );

extern SMannequinPlayerParams PlayerMannequin;

void InitPlayerMannequin( IActionController* pActionController );



template< typename T >
const T* GetMannequinUserParams( const SAnimationContext& context )
{
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	CRY_ASSERT( pGameFramework );
	IMannequin& mannequin = pGameFramework->GetMannequinInterface();
	CMannequinUserParamsManager& mannequinUserParams = mannequin.GetMannequinUserParamsManager();

	const T* const pUserParams = mannequinUserParams.FindOrCreateParams< T >( context.controllerDef );
	CRY_ASSERT( pUserParams );

	return pUserParams;
}


#endif // !__PLAYERANIMATION_H__
