// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: The player on ground state.

-------------------------------------------------------------------------
History:
- 6.10.10: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "PlayerStateGround.h"
#include "Player.h"
#include "PlayerStateJump.h"
#include "PlayerStateUtil.h"

#include "GameCVars.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Utility/CryWatch.h"

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>

#ifdef STATE_DEBUG
static AUTOENUM_BUILDNAMEARRAY(s_ledgeTransitionNames, LedgeTransitionList);
#endif

#include "MovementAction.h"

CPlayerStateGround::CPlayerStateGround()
	:
m_inertiaIsZero(false)
{
}

void CPlayerStateGround::OnEnter( CPlayer& player )
{
	player.GetActorStats()->inAir = 0.0f;

	// Ensure inertia is set!
	CPlayerStateUtil::RestorePlayerPhysics( player );

}

void CPlayerStateGround::OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams &movement, float frameTime, const bool isHeavyWeapon, const bool isPlayer )
{
	const Matrix34A baseMtx = Matrix34A(player.GetBaseQuat());
	Matrix34A baseMtxZ(baseMtx * Matrix33::CreateScale(Vec3Constants<float>::fVec3_OneZ));
	baseMtxZ.SetTranslation(Vec3Constants<float>::fVec3_Zero);

	const CAutoAimManager& autoAimManager = g_pGame->GetAutoAimManager();
	const EntityId closeCombatTargetId = autoAimManager.GetCloseCombatSnapTarget();
	const IActor* pCloseCombatTarget = isPlayer && closeCombatTargetId && player.IsClient() ? g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(closeCombatTargetId) : NULL;

	if (pCloseCombatTarget)
	{
		ProcessAlignToTarget(autoAimManager, player, pCloseCombatTarget);
	}
	else
	{
		// This is to restore inertia if the ProcessAlignToTarget set it previously.
		if( m_inertiaIsZero )
		{
			CPlayerStateUtil::RestorePlayerPhysics( player );

			m_inertiaIsZero = false;
		}

		//process movement
		const bool isRemote = isPlayer && !player.IsClient();

		Vec3 move(ZERO);
		CPlayerStateUtil::CalculateGroundOrJumpMovement( player, movement, isHeavyWeapon, move );
		player.GetMoveRequest().type = eCMT_Normal;


#if 0
		CryLogAlways("DesiredVel: (%f %f %f)", move.x, move.y, move.z);
#endif

		//apply movement
		Vec3 desiredVel(ZERO);
		Vec3 entityPos = player.GetEntity()->GetWorldPos();
		Vec3 entityRight(player.GetBaseQuat().GetColumn0());

		float fGroundNormalZ;

#ifdef STATE_DEBUG
		bool debugJumping = (g_pGameCVars->pl_debug_jumping != 0);
#endif

		const SPlayerStats& stats = *player.GetActorStats();

		{
			desiredVel = move;
			
			Vec3 groundNormal = player.GetActorPhysics().groundNormal;

			if(!gEnv->bMultiplayer)
			{
				if (player.IsAIControlled())
					fGroundNormalZ = square(groundNormal.z);
				else
					fGroundNormalZ = groundNormal.z;
			}
			else
			{
				//If the hill steepness is greater than our minimum threshold
				if(groundNormal.z > 1.f - cosf(g_pGameCVars->pl_movement.mp_slope_speed_multiplier_minHill))
				{
					//Check if we are trying to move up or downhill
					groundNormal.z = 0.f;
					groundNormal.Normalize();

					Vec3 moveDir = move;
					moveDir.z = 0.f;
					moveDir.Normalize();

					float normalDotMove = groundNormal.Dot(moveDir);
					//Apply speed multiplier based on moving up/down hill and hill steepness
					float multiplier = normalDotMove < 0.f ? g_pGameCVars->pl_movement.mp_slope_speed_multiplier_uphill : g_pGameCVars->pl_movement.mp_slope_speed_multiplier_downhill;
					fGroundNormalZ = 1.f - (1.f - player.GetActorPhysics().groundNormal.z) * multiplier;
				}
				else
				{
					fGroundNormalZ = 1.0f;
				}
			}

			const float depthHi = g_pGameCVars->cl_shallowWaterDepthHi;
			const float depthLo = g_pGameCVars->cl_shallowWaterDepthLo;
			const float relativeBottomDepth = player.m_playerStateSwim_WaterTestProxy.GetRelativeBottomDepth();

			if( relativeBottomDepth > depthLo )
			{ // Shallow water speed slowdown
				float shallowWaterMultiplier = isPlayer
					? g_pGameCVars->cl_shallowWaterSpeedMulPlayer
					: g_pGameCVars->cl_shallowWaterSpeedMulAI;

				shallowWaterMultiplier = max(shallowWaterMultiplier, 0.1f);
				assert(shallowWaterMultiplier <= 1.0f);


				float shallowWaterDepthSpan = (depthHi - depthLo);
				shallowWaterDepthSpan = max(0.1f, shallowWaterDepthSpan);

				float slowdownFraction = (relativeBottomDepth - depthLo) / shallowWaterDepthSpan;
				slowdownFraction = clamp_tpl(slowdownFraction, 0.0f, 1.0f);
				shallowWaterMultiplier = LERP(1.0f, shallowWaterMultiplier, slowdownFraction);

				//avoid branch if m_stats.relativeBottomDepth <= 0.0f;
				shallowWaterMultiplier = (float)__fsel(-relativeBottomDepth, 1.0f, shallowWaterMultiplier);

				desiredVel *= shallowWaterMultiplier;
			}
		}

#if 0
		CryLogAlways("DesiredVel: (%f %f %f)", desiredVel.x, desiredVel.y, desiredVel.z);
#endif

		// Slow down on sloped terrain, simply proportional to the slope. 
		desiredVel *= fGroundNormalZ;

		//be sure desired velocity is flat to the ground
		Vec3 desiredVelVert = baseMtxZ * desiredVel;

		desiredVel -= desiredVelVert;

		if (isPlayer)
		{
			Vec3 modifiedSlopeNormal = player.GetActorPhysics().groundNormal;
			float h = Vec2(modifiedSlopeNormal.x, modifiedSlopeNormal.y).GetLength(); // TODO: OPT: sqrt(x*x+y*y)
			float v = modifiedSlopeNormal.z;
			float slopeAngleCur = RAD2DEG(atan2_tpl(h, v));

			const float divisorH = (float)__fsel(-h, 1.0f, h);
			const float divisorV = (float)__fsel(-v, 1.0f, v);

			const float invV = __fres(divisorV);
			const float invH = __fres(divisorH);

			const float slopeAngleHor = 10.0f;
			const float slopeAngleVer = 50.0f;
			float slopeAngleFraction = clamp_tpl((slopeAngleCur - slopeAngleHor) * __fres(slopeAngleVer - slopeAngleHor), 0.0f, 1.0f);

			slopeAngleFraction = slopeAngleFraction * slopeAngleFraction * slopeAngleFraction;

			float slopeAngleMod = LERP(0.0f, 90.0f, slopeAngleFraction);

			float s, c;

			sincos_tpl(DEG2RAD(slopeAngleMod), &s, &c);

			const float hMultiplier = (float)__fsel(-h, 1.0f, s * invH);
			const float vMultiplier = (float)__fsel(-v, 1.0f, c * invV);

			modifiedSlopeNormal.x *= hMultiplier;
			modifiedSlopeNormal.y *= hMultiplier;
			modifiedSlopeNormal.z *= vMultiplier;

			//Normalize the slope normal if possible
			const float fSlopeNormalLength = modifiedSlopeNormal.len();
			const float fSlopeNormalLengthSafe = (float)__fsel(fSlopeNormalLength - 0.000001f, fSlopeNormalLength, 1.0f);
			modifiedSlopeNormal = modifiedSlopeNormal * __fres(fSlopeNormalLengthSafe);

			float alignment = min(modifiedSlopeNormal * desiredVel, 0.0f);

			// Also affect air control (but not as much), to prevent jumping up against steep slopes.

			alignment *= (float)__fsel(-fabsf(stats.onGround), LERP(0.7f, 1.0f, 1.0f - clamp_tpl(modifiedSlopeNormal.z * 100.0f, 0.0f, 1.0f)), 1.0f);

			desiredVel -= modifiedSlopeNormal * alignment;


#ifdef STATE_DEBUG
			if (debugJumping)
			{
				player.DebugGraph_AddValue("GroundSlope", slopeAngleCur);
				player.DebugGraph_AddValue("GroundSlopeMod", slopeAngleMod);
			}
#endif
		}

#if 0
		CryLogAlways("DesiredVel: (%f %f %f)", desiredVel.x, desiredVel.y, desiredVel.z);
#endif

		Vec3 newVelocity = desiredVel;

		const float fNewSpeed = newVelocity.len();

		const float fVelocityMultiplier = (float)__fsel(fNewSpeed - 22.0f, __fres(fNewSpeed+FLT_EPSILON) * 22.0f, 1.0f);

		// TODO: Maybe we should tell physics about this new velocity ? Or maybe SPlayerStats::velocity ? (stephenn).
		player.GetMoveRequest().velocity = newVelocity * (stats.flashBangStunMult * fVelocityMultiplier);

#ifdef STATE_DEBUG
		if(g_pGameCVars->pl_debug_movement > 0)
		{
			const char* filter = g_pGameCVars->pl_debug_filter->GetString();
			const char* name = player.GetEntity()->GetName();
			if ((strcmp(filter, "0") == 0) || (strcmp(filter, name) == 0))
			{
				float white[] = {1.0f,1.0f,1.0f,1.0f};
				IRenderAuxText::Draw2dLabel(20, 450, 2.0f, white, false, "Speed: %.3f m/s", player.GetMoveRequest().velocity.len());

				if(g_pGameCVars->pl_debug_movement > 1)
				{
					IRenderAuxText::Draw2dLabel(35, 470, 1.8f, white, false, "Stance Speed: %.3f m/s - (%sSprinting)", player.GetStanceMaxSpeed(player.GetStance()), player.IsSprinting() ? "" : "Not ");
				}
			}
		}
#endif

#if 0
		{
			const float XPOS = 200.0f;
			const float YPOS = 80.0f;
			const float FONT_SIZE = 2.0f;
			const float FONT_COLOUR[4] = {1,1,1,1};
			if (!player.IsClient())
			{
				IRenderAuxText::Draw2dLabel(XPOS, YPOS, FONT_SIZE, FONT_COLOUR, false, "PlayerMovement: (%f %f %f)", m_request.velocity.x, m_request.velocity.y, m_request.velocity.z);
			}

			static float JumpTime = 0.0f;
			CryLogAlways("PlayerMovement: %s (%f %f %f) t:%f", player.GetEntity()->GetName(), m_request.velocity.x, m_request.velocity.y, m_request.velocity.z, gEnv->pTimer->GetCurrTime() - JumpTime);
			CryLogAlways("DesiredVel: (%f %f %f) SM:%f", m_movement.desiredVelocity.x, m_movement.desiredVelocity.y, m_movement.desiredVelocity.z, m_params.strafeMultiplier);
			if (m_movement.jump)
			{
				if (!player.IsClient())
				{
					IRenderAuxText::Draw2dLabel(XPOS, YPOS+20, FONT_SIZE, FONT_COLOUR, false, "Jump: (%f %f %f) DesVel: (%f %f %f)", jumpVec.x, jumpVec.y, jumpVec.z, desiredVel.x, desiredVel.y, desiredVel.z);
				}
				CryLogAlways("PlayerJump: %s (%f %f %f) DesVel: (%f %f %f)", player.GetEntity()->GetName(), jumpVec.x, jumpVec.y, jumpVec.z, desiredVel.x, desiredVel.y, desiredVel.z);
				JumpTime = gEnv->pTimer->GetCurrTime();
			}
		}
#endif
	}

	if( isPlayer )
	{
		CheckForVaultTrigger(player, frameTime);
	}
}

bool CPlayerStateGround::CheckForVaultTrigger(CPlayer & player, float frameTime)
{
	const int enableVaultFromStandingCVar = g_pGameCVars->pl_ledgeClamber.enableVaultFromStanding;
	const bool doCheck = (enableVaultFromStandingCVar == 3) || ((enableVaultFromStandingCVar > 0) && player.m_jumpButtonIsPressed);

	if (doCheck)
	{
		SLedgeTransitionData ledgeTransition(LedgeId::invalid_id);
		const float zPos = player.GetEntity()->GetWorldPos().z;
		const bool ignoreMovement = (enableVaultFromStandingCVar == 2);

		if (CPlayerStateLedge::TryLedgeGrab(player, zPos, zPos, true, &ledgeTransition, ignoreMovement) && ledgeTransition.m_ledgeTransition != SLedgeTransitionData::eOLT_None)
		{
			CRY_ASSERT( LedgeId(ledgeTransition.m_nearestGrabbableLedgeId).IsValid() );
			const SLedgeInfo ledgeInfo = g_pGame->GetLedgeManager()->GetLedgeById( LedgeId(ledgeTransition.m_nearestGrabbableLedgeId) );
			
			CRY_ASSERT( ledgeInfo.IsValid() );

			if (ledgeInfo.AreAnyFlagsSet(kLedgeFlag_useVault|kLedgeFlag_useHighVault))
			{
#ifdef STATE_DEBUG

				if (g_pGameCVars->pl_ledgeClamber.debugDraw)
				{
					const char * transitionName = s_ledgeTransitionNames[ledgeTransition.m_ledgeTransition];

					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(ledgeInfo.GetEntityId());
					CryWatch ("[LEDGEGRAB] $5%s nearest ledge: %s%s%s%s, transition=%s", player.GetEntity()->GetEntityTextDescription().c_str(), pEntity ? pEntity->GetEntityTextDescription().c_str() : "none", ledgeInfo.AreFlagsSet(kLedgeFlag_isThin) ? " THIN" : "", ledgeInfo.AreFlagsSet(kLedgeFlag_isWindow) ? " WINDOW" : "", ledgeInfo.AreFlagsSet(kLedgeFlag_endCrouched) ? " ENDCROUCHED" : "", transitionName);
				}

#endif

				if (player.m_jumpButtonIsPressed || enableVaultFromStandingCVar == 3)
				{
					ledgeTransition.m_comingFromOnGround=true;
					ledgeTransition.m_comingFromSprint=player.IsSprinting();

					SStateEventLedge ledgeEvent(ledgeTransition);
					player.StateMachineHandleEventMovement(ledgeEvent);
					return true;
				}
				else
				{
#ifdef STATE_DEBUG
					if (g_pGameCVars->pl_ledgeClamber.debugDraw)
					{
						const char * message = NULL;
						switch (ledgeTransition.m_ledgeTransition)
						{
							case SLedgeTransitionData::eOLT_VaultOnto:
							message = "CLIMB";
							break;

							case SLedgeTransitionData::eOLT_VaultOver:
							message = "VAULT";
							break;

							default:
							CRY_ASSERT_TRACE(0, ("Unexpected ledge transition #%d when trying to display HUD prompt for vault-from-standing!", ledgeTransition.m_ledgeTransition));
							break;
						}

						if (message)
						{
							const float textColor[4] = {1.f, 1.f, 1.f, 1.0f};
							const float bracketColor[4] = {0.7f, 0.7f, 0.7f, 1.0f};
							const float iconSize = 4.f;
							const float textSize = 2.f;
							const float iconColor[4] = {0.3f, 1.f, 0.3f, 1.0f};
							const char * iconText = "A";

							IRenderAuxText::Draw2dLabel((gEnv->pRenderer->GetOverlayWidth() * 0.5f), (gEnv->pRenderer->GetOverlayHeight() * 0.65f), iconSize, bracketColor, true, "( )");
							IRenderAuxText::Draw2dLabel((gEnv->pRenderer->GetOverlayWidth() * 0.5f), (gEnv->pRenderer->GetOverlayHeight() * 0.65f), iconSize, iconColor, true, "%s", iconText);
							IRenderAuxText::Draw2dLabel((gEnv->pRenderer->GetOverlayWidth() * 0.5f), (gEnv->pRenderer->GetOverlayHeight() * 0.72f), textSize, textColor, true, "%s", message);
						}
					}
#endif
				}
			}
		}
	}

	return false;
}

void CPlayerStateGround::OnUpdate( CPlayer& player, float frameTime )
{

}

void CPlayerStateGround::OnExit( CPlayer& player )
{
}

//-----------------------------------------------------------------------------------------------
void CPlayerStateGround::ProcessAlignToTarget(const CAutoAimManager& autoAimManager, CPlayer& player, const IActor* pTarget)
{
	CRY_ASSERT(pTarget);

	if( !m_inertiaIsZero )
	{
		SAnimatedCharacterParams params = player.m_pAnimatedCharacter->GetParams();

		params.inertia = 0.f;

		player.SetAnimatedCharacterParams(params);

		m_inertiaIsZero = true;
	}


	Vec3 targetPos = pTarget->GetAnimatedCharacter()->GetAnimLocation().t;
	Vec3 playerPos = player.GetAnimatedCharacter()->GetAnimLocation().t;


	Vec3 displacement = (targetPos - playerPos);

	float targetDistance = autoAimManager.GetCloseCombatSnapTargetRange();

	SCharacterMoveRequest& moveRequest = player.GetMoveRequest();

	moveRequest.type = eCMT_Fly;
	moveRequest.velocity.zero();

	if(displacement.len2() > sqr(targetDistance))
	{
		float distanceToTravel = displacement.len() - targetDistance;

		displacement.z = 0.f;
		displacement.Normalize();		

		const CActor *pTargetActor = static_cast<const CActor*>(pTarget);
		const Vec3 &targetVelocity = pTargetActor->GetActorPhysics().velocity;

		moveRequest.velocity = (displacement * distanceToTravel * autoAimManager.GetCloseCombatSnapTargetMoveSpeed()) + targetVelocity;
	}
}
