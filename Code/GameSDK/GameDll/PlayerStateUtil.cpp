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
#include "StdAfx.h"

#include "PlayerStateUtil.h"
#include "PlayerStateJump.h"
#include "PlayerInput.h"
#include "Player.h"

#include "NetPlayerInput.h"

#include "GameCVars.h"
#include "GameRules.h"

#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "NetInputChainDebug.h"

#include "Utility/CryWatch.h"

#include "Weapon.h"

#include "IGameObject.h"


void CPlayerStateUtil::CalculateGroundOrJumpMovement( const CPlayer& player, const SActorFrameMovementParams &movement, const bool bigWeaponRestrict, Vec3 &move )
{
	const bool isPlayer = player.IsPlayer();
	const float totalMovement = fabsf(movement.desiredVelocity.x) + fabsf(movement.desiredVelocity.y);
	const bool moving = (totalMovement > FLT_EPSILON);

	if (moving)
	{	
		Vec3 desiredVelocityClamped = movement.desiredVelocity;
		const float desiredVelocityMag = desiredVelocityClamped.GetLength();

		const float invDesiredVelocityMag = 1.0f / desiredVelocityMag;

		float strafeMul;

		if (bigWeaponRestrict)
		{
			strafeMul = g_pGameCVars->pl_movement.nonCombat_heavy_weapon_strafe_speed_scale;
		}
		else
		{
			strafeMul = g_pGameCVars->pl_movement.strafe_SpeedScale;
		}

		float backwardMul = 1.0f;

		desiredVelocityClamped = desiredVelocityClamped * (float)__fsel(-(desiredVelocityMag - 1.0f), 1.0f, invDesiredVelocityMag);

		//going back?
		if (isPlayer)	//[Mikko] Do not limit backwards movement when controlling AI.
		{
			backwardMul = (float)__fsel(desiredVelocityClamped.y, 1.0f, LERP(backwardMul, player.m_params.backwardMultiplier, -desiredVelocityClamped.y));
		}

		NETINPUT_TRACE(player.GetEntityId(), backwardMul);
		NETINPUT_TRACE(player.GetEntityId(), strafeMul);

		const Quat oldBaseQuat = player.GetEntity()->GetWorldRotation(); // we cannot use m_baseQuat: that one is already updated to a new orientation while desiredVelocity was relative to the old entity frame
		move += oldBaseQuat.GetColumn0() * desiredVelocityClamped.x * strafeMul * backwardMul;
		move += oldBaseQuat.GetColumn1() * desiredVelocityClamped.y * backwardMul;
	}

	//ai can set a custom sprint value, so dont cap the movement vector
	if (movement.sprint<=0.0f)
	{
		//cap the movement vector to max 1
		float moveModule(move.len());

		//[Mikko] Do not limit backwards movement when controlling AI, otherwise it will disable sprinting.
		if (isPlayer)
		{
			move /= (float)__fsel(-(moveModule - 1.0f), 1.0f, moveModule);
		}

		NETINPUT_TRACE(player.GetEntityId(), moveModule);
	}

	//player movement don't need the m_frameTime, its handled already in the physics
	float scale = player.GetStanceMaxSpeed(player.GetStance());
	move *= scale;
	NETINPUT_TRACE(player.GetEntityId(), scale);

	if (isPlayer)
	{
		const bool  isCrouching = ((player.m_actions & ACTION_CROUCH) != 0);
		AdjustMovementForEnvironment( player, move, bigWeaponRestrict, isCrouching );
	}
}

bool CPlayerStateUtil::IsOnGround( CPlayer& player )
{
	return( !player.GetActorPhysics().flags.AreAnyFlagsActive(SActorPhysics::EActorPhysicsFlags_Flying) );
}

bool CPlayerStateUtil::GetPhysicsLivingStat( const CPlayer& player, pe_status_living *pLivStat )
{
	CRY_ASSERT( pLivStat );

	IPhysicalEntity *pPhysEnt = player.GetEntity()->GetPhysics();
	if( pPhysEnt )
	{
		return( pPhysEnt->GetStatus( pLivStat ) > 0 );
	}
	return false;
}

void CPlayerStateUtil::AdjustMovementForEnvironment( const CPlayer& player, Vec3& move, const bool bigWeaponRestrict, const bool crouching )
{
	float mult = (bigWeaponRestrict) 
		? g_pGameCVars->pl_movement.nonCombat_heavy_weapon_speed_scale * player.GetModifiableValues().GetValue(kPMV_HeavyWeaponMovementMultiplier) 
		: 1.0f;

	if (gEnv->bMultiplayer)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		IGameRulesObjectivesModule *pObjectives = pGameRules ? pGameRules->GetObjectivesModule() : NULL;

		if (pObjectives)
		{
			switch(pGameRules->GetGameMode())
			{
				case eGM_Extraction:
				{
					if( pObjectives->CheckIsPlayerEntityUsingObjective(player.GetEntityId()) ) // Carrying a tick
					{
						mult *= g_pGameCVars->mp_extractionParams.carryingTick_SpeedScale;
					}
					break;
				}
				case eGM_CaptureTheFlag:
				{
					if( pObjectives->CheckIsPlayerEntityUsingObjective(player.GetEntityId()) ) // Carrying a flag
					{
						mult *= g_pGameCVars->mp_ctfParams.carryingFlag_SpeedScale;
					}
					break;	
				}
				default:
					break;
			}
		}
	}

	mult *= g_pGameCVars->pl_movement.speedScale;

	if (player.IsSprinting())
	{
		if (bigWeaponRestrict)
		{
			mult *= g_pGameCVars->pl_movement.nonCombat_heavy_weapon_sprint_scale;
		}
		else
		{
			mult *= g_pGameCVars->pl_movement.sprint_SpeedScale;
		}
	}
	else if (crouching)
	{
		if (bigWeaponRestrict)
		{
			mult *= g_pGameCVars->pl_movement.nonCombat_heavy_weapon_crouch_speed_scale;
		}
		else
		{
			mult *= g_pGameCVars->pl_movement.crouch_SpeedScale;
		}
	}

	move *= mult;

}

void CPlayerStateUtil::PhySetFly( CPlayer& player )
{
	IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();
	if (pPhysEnt != NULL)
	{
		pe_player_dynamics pd;
		pd.bSwimming = true;
		pd.kAirControl = 1.0f;
		pd.kAirResistance = 0.0f;
		pd.gravity.zero();
		pPhysEnt->SetParams(&pd);
	}
}

void CPlayerStateUtil::PhySetNoFly( CPlayer& player, const Vec3& gravity )
{
	IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();
	if (pPhysEnt != NULL)
	{
		pe_player_dynamics pd;
		pd.kAirControl = player.GetAirControl();
		pd.kAirResistance = player.GetAirResistance();
		pd.bSwimming = false;
		pd.gravity = gravity;
		player.m_actorPhysics.gravity = gravity;

		pPhysEnt->SetParams(&pd);
	}
}

bool CPlayerStateUtil::ShouldJump( CPlayer& player, const SActorFrameMovementParams& movement )
{
	const bool allowJump = movement.jump && !player.IsJumping(); //m_playerStateJump.IsJumping();
 	if (allowJump)
	{
		// TODO: We need a TryJump (stephenn).

		// This is needed when jumping while standing directly under something that causes an immediate land,
		// before and without even being airborne for one frame.
		// PlayerMovement set m_stats.onGround=0.0f when the jump is triggered,
		// which prevents the on ground timer from before the jump to be inherited
		// and incorrectly and prematurely used to identify landing in MP.

		const SPlayerStats& stats = *player.GetActorStats();
		const SActorPhysics& actorPhysics = player.GetActorPhysics();
		const float fUnconstrainedZ = actorPhysics.velocityUnconstrained.z;
		bool jumpFailed = (stats.onGround > 0.0f) && (fUnconstrainedZ <= 0.0f);

		const float onGroundTime = 0.2f;
		if( ((stats.onGround > onGroundTime) || player.IsRemote()) ) // && !jumpFailed )
		{
			return true;
		}
		else
		{
			CCCPOINT_IF(true, PlayerMovement_PressJumpWhileNotAllowedToJump);
		}
	}
	return false;
}

void CPlayerStateUtil::RestorePlayerPhysics( CPlayer& player )
{
	IAnimatedCharacter* pAC = player.GetAnimatedCharacter();
	if( pAC )
	{
		SAnimatedCharacterParams params;

		params = pAC->GetParams();

		params.inertia = player.GetInertia();
		params.inertiaAccel = player.GetInertiaAccel();
		params.timeImpulseRecover = player.GetTimeImpulseRecover();

		player.SetAnimatedCharacterParams( params );
	}
}

void CPlayerStateUtil::UpdatePlayerPhysicsStats( CPlayer& player, SActorPhysics& actorPhysics, float frameTime )
{
	const int currentFrameID = gEnv->nMainFrameID;
	if( actorPhysics.lastFrameUpdate < currentFrameID )
	{
		pe_status_living livStat;
		if( !CPlayerStateUtil::GetPhysicsLivingStat( player, &livStat ) )
		{
			return;
		}

		SPlayerStats& stats = *player.GetActorStats();

		const Vec3 newVelocity = livStat.vel-livStat.velGround;
		actorPhysics.velocityDelta = newVelocity - actorPhysics.velocity;
		actorPhysics.velocity = newVelocity;
		actorPhysics.velocityUnconstrainedLast = actorPhysics.velocityUnconstrained;
		actorPhysics.velocityUnconstrained = livStat.velUnconstrained;
		actorPhysics.flags.SetFlags( SActorPhysics::EActorPhysicsFlags_WasFlying, actorPhysics.flags.AreAnyFlagsActive(SActorPhysics::EActorPhysicsFlags_Flying) );
		actorPhysics.flags.SetFlags( SActorPhysics::EActorPhysicsFlags_Flying, livStat.bFlying > 0 );
		actorPhysics.flags.SetFlags( SActorPhysics::EActorPhysicsFlags_Stuck, livStat.bStuck > 0 );

		Vec3 flatVel(player.m_pPlayerRotation->GetBaseQuat().GetInverted()*newVelocity);
		flatVel.z = 0;
		stats.speedFlat = flatVel.len();

		if(player.IsInAir())
		{
			stats.maxAirSpeed = max(stats.maxAirSpeed, newVelocity.GetLengthFast());
		}
		else
		{
			stats.maxAirSpeed = 0.f;
		}
		float fSpeedFlatSelector = stats.speedFlat - 0.1f;


		const float groundNormalBlend = clamp_tpl(frameTime * 6.666f, 0.0f, 1.0f);
		actorPhysics.groundNormal = LERP(actorPhysics.groundNormal, livStat.groundSlope, groundNormalBlend);

		if (livStat.groundSurfaceIdxAux > 0)
			actorPhysics.groundMaterialIdx = livStat.groundSurfaceIdxAux;
		else
			actorPhysics.groundMaterialIdx = livStat.groundSurfaceIdx;

		actorPhysics.groundHeight = livStat.groundHeight;

		EntityId newGroundColliderId = 0;
		if (livStat.pGroundCollider)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(livStat.pGroundCollider);
			newGroundColliderId = pEntity ? pEntity->GetId() : 0;
		}

		if( newGroundColliderId != actorPhysics.groundColliderId )
		{
			if( actorPhysics.groundColliderId )
			{
				if( IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject( actorPhysics.groundColliderId ) )
				{
					SGameObjectEvent event( eGFE_StoodOnChange, eGOEF_ToExtensions );
					event.ptr = &player;
					event.paramAsBool  = false;
					pGameObject->SendEvent(event);
				}
			}

			if( newGroundColliderId )
			{
				if( IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject( newGroundColliderId ) )
				{
					SGameObjectEvent event( eGFE_StoodOnChange, eGOEF_ToExtensions );
					event.ptr = &player;
					event.paramAsBool  = true;
					pGameObject->SendEvent(event);
				}
			}

			actorPhysics.groundColliderId = newGroundColliderId;
		}

		IPhysicalEntity *pPhysEnt = player.GetEntity()->GetPhysics();
		if( pPhysEnt )
		{
			pe_status_dynamics dynStat;
			pPhysEnt->GetStatus(&dynStat);

			actorPhysics.angVelocity = dynStat.w;
			actorPhysics.mass = dynStat.mass;

			pe_player_dynamics simPar;
			if (pPhysEnt->GetParams(&simPar) != 0)
			{
				actorPhysics.gravity = simPar.gravity;
			}
		}

		actorPhysics.lastFrameUpdate = currentFrameID;

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
	player.GetMovementDebug().DebugGraph_AddValue("PhysVelo", livStat.vel.GetLength());
	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloX", livStat.vel.x);
	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloY", livStat.vel.y);
	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloZ", livStat.vel.z);

	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloUn", livStat.velUnconstrained.GetLength());
	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloUnX", livStat.velUnconstrained.x);
	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloUnY", livStat.velUnconstrained.y);
	player.GetMovementDebug().DebugGraph_AddValue("PhysVeloUnZ", livStat.velUnconstrained.z);

	player.GetMovementDebug().DebugGraph_AddValue("PhysVelReq", livStat.velRequested.GetLength());
	player.GetMovementDebug().DebugGraph_AddValue("PhysVelReqX", livStat.velRequested.x);
	player.GetMovementDebug().DebugGraph_AddValue("PhysVelReqY", livStat.velRequested.y);
	player.GetMovementDebug().DebugGraph_AddValue("PhysVelReqZ", livStat.velRequested.z);

	player.GetMovementDebug().LogVelocityStats(player.GetEntity(), livStat, stats.downwardsImpactVelocity, stats.fallSpeed);
#endif
	}
}

void CPlayerStateUtil::ProcessTurning( CPlayer& player, float frameTime )
{
	const Quat entityRot = player.GetEntity()->GetWorldRotation().GetNormalized();
	const Quat inverseEntityRot = entityRot.GetInverted();

	const Quat baseQuat = player.GetBaseQuat();
	SCharacterMoveRequest& request = player.GetMoveRequest();

	request.rotation = inverseEntityRot * baseQuat;
	request.rotation.Normalize();
}


void CPlayerStateUtil::InitializeMoveRequest( SCharacterMoveRequest& frameRequest )
{
	frameRequest = SCharacterMoveRequest();
	frameRequest.type = eCMT_Normal;
}

void CPlayerStateUtil::FinalizeMovementRequest( CPlayer& player, const SActorFrameMovementParams & movement, SCharacterMoveRequest& request )
{
	//////////////////////////////////////////////////////////////////////////
	/// Remote players stuff...

	UpdateRemotePlayersInterpolation(player, movement, request);

	//////////////////////////////////////////////////////////////////////////
	///  Send the request to animated character

	if (player.m_pAnimatedCharacter)
	{
		request.allowStrafe = movement.allowStrafe;
		request.prediction = movement.prediction;

		CRY_ASSERT_TRACE(request.velocity.IsValid(), ("Invalid velocity %.2f %.2f %.2f for %s!", request.velocity.x, request.velocity.y, request.velocity.z, player.GetEntity()->GetEntityTextDescription().c_str()));

		NETINPUT_TRACE(player.GetEntityId(), request.rotation * FORWARD_DIRECTION);
		NETINPUT_TRACE(player.GetEntityId(), request.velocity);

#ifdef STATE_DEBUG
		player.DebugGraph_AddValue("ReqVelo", request.velocity.GetLength());
		player.DebugGraph_AddValue("ReqVeloX", request.velocity.x);
		player.DebugGraph_AddValue("ReqVeloY", request.velocity.y);
		player.DebugGraph_AddValue("ReqVeloZ", request.velocity.z);
		player.DebugGraph_AddValue("ReqRotZ", RAD2DEG(request.rotation.GetRotZ()));
#endif

		player.m_pAnimatedCharacter->AddMovement( request );
	}

	NETINPUT_TRACE(player.GetEntityId(), request.velocity);

	player.m_lastRequestedVelocity = request.velocity; 

	// reset the request for the next frame!
	InitializeMoveRequest( request );
}

void CPlayerStateUtil::UpdateRemotePlayersInterpolation( CPlayer& player, const SActorFrameMovementParams& movement, SCharacterMoveRequest& frameRequest )
{
	if (!gEnv->bMultiplayer)
		return;

	//////////////////////////////////////////////////////////////////////////
	/// Interpolation for remote players

	const bool isRemoteClient = player.IsRemote();
	const bool doVelInterpolation = isRemoteClient && player.GetPlayerInput();

	if (doVelInterpolation)
	{
		if (g_pGameCVars->pl_clientInertia >= 0.0f)
		{
			player.m_inertia = g_pGameCVars->pl_clientInertia;
		}
		player.m_inertiaAccel = player.m_inertia;

		const bool isNetJumping = g_pGameCVars->pl_velocityInterpSynchJump && movement.jump && isRemoteClient;

		if (isNetJumping)
		{
#ifdef STATE_DEBUG
			if (g_pGameCVars->pl_debugInterpolation)
			{
				CryWatch("SetVel (%f, %f, %f)", player.m_jumpVel.x, player.m_jumpVel.y, player.m_jumpVel.z);
			}
#endif //_RELEASE
			frameRequest.velocity = player.m_jumpVel;
			frameRequest.type = eCMT_JumpAccumulate;
		}
		else
		{
			CNetPlayerInput *playerInput = static_cast<CNetPlayerInput*>(player.GetPlayerInput());
			Vec3 interVel, desiredVel; 

			bool inAir;
			desiredVel = playerInput->GetDesiredVelocity(inAir);
			interVel = desiredVel;

			IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();

			const bool isPlayerInAir = player.IsInAir();
			if (inAir && isPlayerInAir)
			{
				if (pPhysEnt && (g_pGameCVars->pl_velocityInterpAirDeltaFactor < 1.0f))
				{
					pe_status_dynamics dynStat;
					pPhysEnt->GetStatus(&dynStat);

					Vec3  velDiff = interVel - dynStat.v;
					interVel = dynStat.v + (velDiff * g_pGameCVars->pl_velocityInterpAirDeltaFactor);
					frameRequest.velocity.z = interVel.z;
					frameRequest.velocity.z -= player.GetActorPhysics().gravity.z*gEnv->pTimer->GetFrameTime();
					frameRequest.type = eCMT_Fly; 
				}
			}
			frameRequest.velocity.x = interVel.x;
			frameRequest.velocity.y = interVel.y;


#ifdef STATE_DEBUG
			if (pPhysEnt && g_pGameCVars->pl_debugInterpolation)
			{
				pe_status_dynamics dynStat;
				pPhysEnt->GetStatus(&dynStat);

				CryWatch("Cur: (%f %f %f) Des: (%f %f %f)", dynStat.v.x, dynStat.v.y, dynStat.v.z, desiredVel.x, desiredVel.y, desiredVel.z);
				CryWatch("Req: (%f %f %f) Type (%d)", frameRequest.velocity.x, frameRequest.velocity.y, frameRequest.velocity.z, frameRequest.type);
			}
#endif //!_RELEASE
		}
	}
}

bool CPlayerStateUtil::IsMovingForward( const CPlayer& player, const SActorFrameMovementParams& movement )
{
	const float fSpeedFlatSelector = player.GetActorStats()->speedFlat - 0.1f;
	bool movingForward = (fSpeedFlatSelector > 0.1f);

	if (!gEnv->bMultiplayer || player.IsClient())
	{
		// IsMovingForward() doesn't return particularly reliable results for client players in MP, I think because of interpolation innacuracies on the Y velocity. this was causing client players to
		// incorrectly report that they're no longer moving forwards, whereas all that had happened was they had stopped sprinting - and this in turn was messing up the MP sprint stamina regeneration delays.
		// client players have also been seen to stop sprinting due to turning lightly, whereas on their local machine they were still sprinting fine...
		// so i've tried changing it so IsMovingForward() is only taken into account on local clients (and in SP!).  [tlh]
		const float thresholdSinAngle = sin_tpl((3.141592f * 0.5f) - DEG2RAD(g_pGameCVars->pl_power_sprint.foward_angle));
		const float currentSinAngle = movement.desiredVelocity.y;

		movingForward = movingForward && (currentSinAngle > thresholdSinAngle);
	}

	return movingForward;
}

bool CPlayerStateUtil::ShouldSprint( const CPlayer& player, const SActorFrameMovementParams& movement, IItem* pCurrentPlayerItem )
{
	bool shouldSprint = false;
	const SPlayerStats& stats = player.m_stats;
	const bool movingForward = IsMovingForward( player, movement );

	bool restrictSprint = player.IsJumping() || stats.bIgnoreSprinting || player.IsSliding() || player.IsCinematicFlagActive(SPlayerStats::eCinematicFlag_RestrictMovement);

	CWeapon* pWeapon = pCurrentPlayerItem ? static_cast<CWeapon*>(pCurrentPlayerItem->GetIWeapon()) : NULL;
	bool isZooming = pWeapon ? pWeapon->IsZoomed() && !pWeapon->IsZoomingInOrOut() : false;
	if (pWeapon && !pWeapon->CanSprint())
		restrictSprint = true;
	
	restrictSprint = restrictSprint || (player.GetSprintStaminaLevel() <= 0.f);
	//CryLogAlways("  restrictSprint = %d", restrictSprint);

	if (player.IsSprinting() == false)
	{
		shouldSprint = movingForward && !restrictSprint && !isZooming;
		CCCPOINT_IF(shouldSprint, PlayerMovement_SprintOn);
	}
	else
	{
		shouldSprint = movingForward && !restrictSprint && !isZooming;
		//CryLogAlways("  shouldSprint = %d", shouldSprint);

		shouldSprint = shouldSprint && (!(player.m_actions & ACTION_CROUCH));

		if(!shouldSprint && pWeapon)
		{
			pWeapon->ForcePendingActions();
		}
	}

	CCCPOINT_IF(!player.IsSprinting() && !shouldSprint && (player.m_actions & ACTION_SPRINT), PlayerMovement_SprintRequestIgnored);
	CCCPOINT_IF(player.IsSprinting() && !shouldSprint, PlayerMovement_SprintOff);

	//CryLogAlways("  returning (for \"%s\") shouldSprint = %d", m_player.GetEntity()->GetName(), shouldSprint);
	return shouldSprint;
}


void CPlayerStateUtil::ApplyFallDamage( CPlayer& player, const float startFallingHeight, const float fHeightofEntity )
{
	CRY_ASSERT(player.IsClient());

	// Zero downwards impact velocity used for fall damage calculations if player was in water within the last 0.5 seconds.
	// Strength jumping straight up and down should theoretically land with a safe velocity, 
	// but together with the water surface stickyness the velocity can sometimes go above the safe impact velocity threshold.

	// DEPRECATED: comment left for prosterity in case dedicated server problems re-appear (author cannot test it).
	// On dedicated server the player can still be flying this frame as well, 
	// since synced pos from client is interpolated/smoothed and will not land immediately,
	// even though the velocity is set to zero.
	// Thus we need to use the velocity change instead of landing to identify impact.

	// DT: 12475: Falling a huge distance to a ledge grab results in no damage.
	// Now using the last velocity because when ledge grabbing the velocity is unchanged for this frame, thus zero damage is applied.
	// Assuming this a physics lag issue, using the last good velocity should be more-or-less ok.
	const float downwardsImpactSpeed = -(float)__fsel(-(player.m_playerStateSwim_WaterTestProxy.GetSwimmingTimer() + 0.5f), player.GetActorPhysics().velocityUnconstrainedLast.z, 0.0f);

	const SPlayerStats& stats = *player.GetActorStats();

	CRY_ASSERT(NumberValid(downwardsImpactSpeed));
	const float MIN_FALL_DAMAGE_DISTANCE = 3.0f;
	const float fallDist = startFallingHeight - fHeightofEntity;
	if ((downwardsImpactSpeed > 0.0f) && (fallDist > MIN_FALL_DAMAGE_DISTANCE))
	{
		const SPlayerHealth& healthCVars = g_pGameCVars->pl_health;

		float velSafe = healthCVars.fallDamage_SpeedSafe;
		float velFatal = healthCVars.fallDamage_SpeedFatal;
		float velFraction = (float)__fsel(-(velFatal - velSafe), 1.0f , (downwardsImpactSpeed - velSafe) * (float)__fres(velFatal - velSafe));

		CRY_ASSERT(NumberValid(velFraction));

		if (velFraction > 0.0f)
		{
			//Stop crouching after taking falling damage
			if(player.GetStance() == STANCE_CROUCH)
			{
				static_cast<CPlayerInput*>(player.GetPlayerInput())->ClearCrouchAction();
			}

			velFraction = powf(velFraction, gEnv->bMultiplayer ? healthCVars.fallDamage_CurveAttackMP : healthCVars.fallDamage_CurveAttack);

			const float maxHealth = player.GetMaxHealth();
			const float currentHealth  = player.GetHealth();

			HitInfo hit;
			hit.dir.zero();
			hit.type = CGameRules::EHitType::Fall;
			hit.shooterId = hit.targetId = hit.weaponId = player.GetEntityId();

			const float maxDamage = (float)__fsel(velFraction - 1.0f, maxHealth, max(0.0f, (gEnv->bMultiplayer?maxHealth:currentHealth) - healthCVars.fallDamage_health_threshold));

			hit.damage = velFraction * maxDamage;

			g_pGame->GetGameRules()->ClientHit(hit);

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
			player.GetMovementDebug().LogFallDamage(player.GetEntity(), velFraction, downwardsImpactSpeed, hit.damage);
		}
		else
		{
			player.GetMovementDebug().LogFallDamageNone(player.GetEntity(), downwardsImpactSpeed);
		}
#else
		}
#endif
	}
}

bool CPlayerStateUtil::DoesArmorAbsorptFallDamage(CPlayer& player, const float downwardsImpactSpeed, float& absorptedDamageFraction)
{
	CRY_ASSERT(!gEnv->bMultiplayer);
	
	absorptedDamageFraction = 0.0f;

	const SPlayerHealth& healthCVars = g_pGameCVars->pl_health;

	const float speedEnergySafe = healthCVars.fallDamage_SpeedSafe;
	const float speedEnergyDeplete = healthCVars.fallDamage_SpeedFatal;
	float heavyArmorFactor = 1.0f;

	//Armor took all damage at no energy cost
	if(downwardsImpactSpeed <= speedEnergySafe)
		return true;

	return false;
}

void CPlayerStateUtil::CancelCrouchAndProneInputs(CPlayer & player)
{
	IPlayerInput * playerInputInterface = player.GetPlayerInput();

	if (playerInputInterface && playerInputInterface->GetType() == IPlayerInput::PLAYER_INPUT)
	{
		CPlayerInput * playerInput = static_cast<CPlayerInput*>(playerInputInterface);
		playerInput->ClearCrouchAction();
		playerInput->ClearProneAction();
	}
}


void CPlayerStateUtil::ChangeStance( CPlayer& player, const SStateEvent& event )
{
	const SStateEventStanceChanged& stanceEvent = static_cast<const SStateEventStanceChanged&> (event).GetStance();
	const EStance stance = static_cast<EStance> (stanceEvent.GetStance());
	player.OnSetStance( stance );
}
