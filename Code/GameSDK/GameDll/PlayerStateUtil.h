// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Utility functions for the playerstate

-------------------------------------------------------------------------
History:
- 14.10.10: Created by Stephen M. North

*************************************************************************/
#ifndef __PlayerStateUtil_h__
#define __PlayerStateUtil_h__

#pragma once

class CPlayer;
struct SActorFrameMovementParams;
struct pe_status_living;
struct SCharacterMoveRequest;
struct SStateEvent;
struct SActorPhysics;

class CPlayerStateUtil
{
public:

	static void CalculateGroundOrJumpMovement( const CPlayer& player, const SActorFrameMovementParams &movement, const bool bBigWeaponRestrict, Vec3 &move );
	static bool IsOnGround( CPlayer& player );
	static bool GetPhysicsLivingStat( const CPlayer& player, pe_status_living *livStat );
	static void AdjustMovementForEnvironment( const CPlayer& player, Vec3& movement, const bool bigWeaponRestrict, const bool crouching );
	static void PhySetFly( CPlayer& player );
	static void PhySetNoFly( CPlayer& player, const Vec3& gravity );
	static bool ShouldJump( CPlayer& player, const SActorFrameMovementParams& movement );
	static void RestorePlayerPhysics( CPlayer& player );
	static void UpdatePlayerPhysicsStats( CPlayer& player, SActorPhysics& actorPhysics, float frameTime );
	static void ProcessTurning( CPlayer& player, float frameTime );
	static void InitializeMoveRequest( SCharacterMoveRequest& frameRequest );
	static void FinalizeMovementRequest( CPlayer& player, const SActorFrameMovementParams & movement, SCharacterMoveRequest& frameRequest );
	static void UpdateRemotePlayersInterpolation( CPlayer& player, const SActorFrameMovementParams& movement, SCharacterMoveRequest& frameRequest );
	static bool IsMovingForward( const CPlayer& player, const SActorFrameMovementParams& movement );
	static bool ShouldSprint( const CPlayer& player, const SActorFrameMovementParams& movement, IItem* pCurrentPlayerItem );
	static void ApplyFallDamage( CPlayer& player, const float startFallingHeight, const float startJumpHeight );
	static bool DoesArmorAbsorptFallDamage(CPlayer& player, const float downwardsImpactSpeed, float& absorptedDamageFraction);
	static void CancelCrouchAndProneInputs(CPlayer & player);
	static void ChangeStance( CPlayer& player, const SStateEvent& event );

private:

	// DO NOT IMPLEMENT
	CPlayerStateUtil();
	CPlayerStateUtil( const CPlayerStateUtil& );
	CPlayerStateUtil& operator=( const CPlayerStateUtil& );
};

#endif// __PlayerStateUtil_h__
