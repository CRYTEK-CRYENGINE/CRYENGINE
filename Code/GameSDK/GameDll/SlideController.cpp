// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Controls sliding mechanic

-------------------------------------------------------------------------
History:
- 26:08:2010: Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "SlideController.h"
#include "Player.h"
#include "Item.h"
#include "GameCVars.h"
#include "Game.h"
#include "GameActions.h"
#include "Weapon.h"
#include "Melee.h"
#include "PlayerInput.h"

#include "StatsRecordingMgr.h"
#include "PlayerStateEvents.h"

#include "PlayerAnimation.h"
#include "PlayerCamera.h"

const float MAX_LEAN_ANGLE							= gf_PI * 0.33f;
const float LEAN_RATE										= 3.0f;
const float MIN_KICK_DP									= cos_tpl(DEG2RAD(60.0f));

class CPlayerSlideAction : public TPlayerAction
{
public:
	DEFINE_ACTION("PlayerSlide");

	CPlayerSlideAction(CPlayer &player, CSlideController& slideController)
		:	TPlayerAction(PP_PlayerActionUrgent, PlayerMannequin.fragmentIDs.slide, TAG_STATE_EMPTY, IAction::NoAutoBlendOut)
		, m_player(player)
		, m_slideController(&slideController)
		, m_leanFactor(0.0f)
		, m_kicking(false)
		, m_entered(false)
		, m_slideFragID(FRAGMENT_ID_INVALID)
		, m_slideFragTags(TAG_STATE_EMPTY)
	{
	}

	EStatus Update(float timePassed) override
	{
		if (IsTransitioningOut())
		{
			const float ROTATION_LERP_SPEED = 10.0f;

			//--- Blend body rotation to match current view
			Ang3 targetViewDir = m_player.GetAngles();
			Quat targetRotation = Quat::CreateRotationZ(targetViewDir.z);
			Quat newRotation = Quat::CreateNlerp(m_player.GetEntity()->GetRotation(), targetRotation, timePassed*ROTATION_LERP_SPEED);
			m_player.GetEntity()->SetRotation(newRotation);
		}
		else
		{
			static uint32 leanParamCRC = CCrc32::ComputeLowercase("SlideFactor");

			const Matrix34 &worldTM = m_player.GetEntity()->GetWorldTM();
			const Vec3 baseRgt = worldTM.GetColumn0();
			const Vec3 baseFwd = worldTM.GetColumn1();
			const Vec3 lookFwd = m_player.GetViewQuatFinal().GetColumn1();
			const float leanAngle = acos_tpl(baseFwd.Dot(lookFwd));

			float targetLeanFactor = clamp_tpl(leanAngle / MAX_LEAN_ANGLE, 0.0f, 1.0f);
			if (baseRgt.Dot(lookFwd) < 0.0f)
			{
				targetLeanFactor *= -1.0f;
			}
			CWeapon *pWeapon = m_player.GetWeapon(m_player.GetCurrentItemId());
			if (pWeapon)
			{
				IFireMode *pFiremode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
				if (pFiremode && (pFiremode->GetNextShotTime() > 0.0f))
				{
					targetLeanFactor = 0.0f;
				}
			}
			const float delta					= targetLeanFactor - m_leanFactor;
			const float step					= LEAN_RATE * timePassed;
			const float newLeanFactor	= (float)__fsel(delta, min(m_leanFactor + step, targetLeanFactor), max(m_leanFactor - step, targetLeanFactor));
			SWeightData weightData;
			weightData.weights[0] = newLeanFactor;
			SetParam(leanParamCRC, weightData);
			m_leanFactor = newLeanFactor;

			if (GetRootScope().IsDifferent(m_fragmentID, m_fragTags))
			{
				SetFragment(m_fragmentID, m_fragTags);
			}
		}

		return TPlayerAction::Update(timePassed);
	}

	void Enter() override
	{
		TPlayerAction::Enter();

		m_player.SetCanTurnBody(false);
		if (m_player.IsClient())
		{
			m_player.GetAnimatedCharacter()->GetGroundAlignmentParams().SetFlag( eGA_Enable, false );
		}

		m_entered = true;
	}

	void Exit() override
	{
		CRY_ASSERT(m_entered);

		TPlayerAction::Exit();

		m_player.SetCanTurnBody(true);
		if (m_slideController)
		{
			m_slideController->ExitedSlide(m_player);
		}

		if (m_player.IsClient())
		{
			m_player.GetAnimatedCharacter()->GetGroundAlignmentParams().SetFlag( eGA_Enable, true );
		}

		m_entered = false;
	}

	void OnSequenceFinished(int layer, uint32 scopeID) override
	{
		if( GetRootScope().GetID() == scopeID && layer == 0 && m_kicking && ((m_flags & IAction::BlendOut) == 0 ) )
		{
			SetFragment( m_slideFragID, m_slideFragTags );
			m_kicking = false;
		}
	}

	void DoSlideKick()
	{
		m_slideFragID = m_fragmentID;
		m_slideFragTags = m_fragTags;

		SetFragment( PlayerMannequin.fragmentIDs.slidingKick );

		m_kicking = true;
	}

	// Return true when we entered before
	bool TriggerStop()
	{
		Stop();

		return m_entered;
	}

	ILINE void ClearBackPointer() { m_slideController = NULL; }

private:

	CPlayer &m_player;
	CSlideController* m_slideController;
	float m_leanFactor;
	bool m_kicking;
	bool m_entered;

	FragmentID m_slideFragID;
	TagState m_slideFragTags;
};

CSlideController::CSlideController()
: m_state(eSlideState_None)
, m_kickTimer(0.0f)
, m_lazyExitTimer(0.0f)
, m_lastPosition( 1.e10f, 1.e10f, 1.e10f )
, m_slideAction(NULL)
{
}

void CSlideController::Update( CPlayer& player, float frameTime, const SActorFrameMovementParams& frameMovementParams, bool isNetSliding, bool isNetExitingSlide, SCharacterMoveRequest& movementRequest )
{
	bool shouldContinueSliding = UpdateMovementRequest(player, frameTime, frameMovementParams, isNetSliding, isNetExitingSlide, movementRequest);

	if (m_state == eSlideState_Sliding)
	{
		UpdateSlidingState(player, frameTime, shouldContinueSliding);
	}
}

bool CSlideController::UpdateMovementRequest( CPlayer& player, float frameTime, const SActorFrameMovementParams& frameMovementParams, bool isNetSliding, bool isNetExitingSlide, SCharacterMoveRequest& movementRequest )
{
	movementRequest.type = eCMT_Fly;
	movementRequest.jumping = false;
	movementRequest.allowStrafe = false;
	movementRequest.prediction.Reset();

	const SPlayerSlideControl& slideCvars = gEnv->bMultiplayer ? g_pGameCVars->pl_sliding_control_mp : g_pGameCVars->pl_sliding_control;
	const SPlayerStats* pStats = (player.GetActorStats());

	const float decceleration = -slideCvars.deceleration_speed; // In m/s

	const Vec3  slideDirection = (player.GetBaseQuat().GetColumn1() + GetTargetDirection(player)).GetNormalized();

	float groundSlopeDeg = 0.0f;
	float downHillAccel = 0.0f;

	const SActorPhysics& actorPhysics = player.GetActorPhysics();
	const Vec3& groundNormal = actorPhysics.groundNormal;
	if(groundNormal.IsUnit())
	{
		const Vec3  normalXY(groundNormal.x, groundNormal.y, 0.0f);
		const float cosine	= slideDirection.Dot(normalXY);

		groundSlopeDeg = RAD2DEG(acos_tpl(cosine)) - 90.0f;

		// groundSlopeDeg tells us if going down hill (negative value) or up hill (positive value)
		if(groundSlopeDeg < -slideCvars.min_downhill_threshold)
		{ 
			const float maxDiff = slideCvars.max_downhill_threshold - slideCvars.min_downhill_threshold;
			const float currentDiff = fabs_tpl(groundSlopeDeg) - slideCvars.min_downhill_threshold; 
			downHillAccel = (float)__fsel(-maxDiff, slideCvars.max_downhill_acceleration, clamp_tpl(currentDiff/maxDiff, 0.0f, 1.0f) * slideCvars.max_downhill_acceleration);
		}
	}

	Vec3 velocity = ((slideDirection) * max(pStats->speedFlat + ((decceleration + downHillAccel)*frameTime), 0.0f));
	velocity += Vec3(0.0f, 0.0f, actorPhysics.velocity.z) + (actorPhysics.gravity * frameTime);

	movementRequest.velocity = velocity;

	// The slide animation persists after the user reaches the wall.
	// Added a distance check because the velocity of the RB is not zero when you slide into something.
	const Vec3 newPosition = player.GetEntity()->GetWorldPos();
	const float distanceTravelledLastFrame = (newPosition - m_lastPosition).len();
	m_lastPosition = newPosition;

	const float thresholdSinAngle = sin_tpl((gf_PI * 0.5f) - DEG2RAD(g_pGameCVars->pl_power_sprint.foward_angle));
	const float currentSinAngle = frameMovementParams.desiredVelocity.y;
	const bool movingForward = (currentSinAngle > thresholdSinAngle);
	const float physicsSpeed = pStats->speedFlat;
	const float gameSpeed = (float)__fsel(-frameTime, 0.0f, (distanceTravelledLastFrame / (frameTime + FLT_EPSILON)));

	bool shouldContinueSliding =  movingForward && (gameSpeed > slideCvars.min_speed) && (physicsSpeed > slideCvars.min_speed) && (pStats->onGround > 0.0f)|| (m_kickTimer > 0.0f);

	return (shouldContinueSliding || isNetSliding) && !isNetExitingSlide;
}

void CSlideController::UpdateSlidingState( CPlayer& player, float frameTime, bool continueSliding )
{
	m_kickTimer = max(m_kickTimer-frameTime, 0.0f);
	
	const float time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	if(m_slideAction && m_slideAction->IsStarted() && (m_lazyExitTimer>0.0f) && ((gEnv->pTimer->GetFrameStartTime().GetSeconds()-m_lazyExitTimer) > 0.01f ))
	{
		continueSliding = false;
	}
	
	if (continueSliding)
	{
		if (gEnv->bMultiplayer && g_pGameCVars->pl_melee.mp_sliding_auto_melee_enabled && (m_kickTimer == 0.0f) && player.IsClient())
		{
			//--- Scan for an auto-kick
			CWeapon *pWeapon = player.GetWeapon(player.GetCurrentItemId());
			if (pWeapon && pWeapon->CanMeleeAttack())
			{
				CMelee *meleeFM = pWeapon->GetMelee();
				EntityId ent = meleeFM->GetNearestTarget();
				if (ent)
				{
					pWeapon->MeleeAttack();
				}
			}
		}
	}
	else
	{
		LazyExitSlide( player );
	}
}

void CSlideController::GoToState( CPlayer& player, ESlideState newState )
{
	if(newState != m_state)
	{
		const ESlideState previousState = m_state;

		m_state = newState;

		OnSlideStateChanged(player);

		switch(m_state)
		{
		case eSlideState_Sliding:
			{
				CWeapon * weapon = player.GetWeapon(player.GetCurrentItemId());
				if (weapon)
				{
					weapon->RequireUpdate(eIUS_FireMode);
				}

				IActionController *actionController = player.GetAnimatedCharacter()->GetActionController();
				if (!m_slideAction && actionController)
				{
					m_slideAction = new CPlayerSlideAction(player, *this);
					m_slideAction->AddRef();
					actionController->Queue(*m_slideAction);
				}
			}
			break;
		case eSlideState_ExitingSlide:
			{
				if (m_slideAction)
				{
					const bool actionWasEnteredBefore = m_slideAction->TriggerStop();
					if (!actionWasEnteredBefore)
					{
						// WARNING: this destroys m_slideAction and re-enters this function
						// to change the state to None
						ExitedSlide(player);
					}
				}

				if (player.IsClient())
				{
					CAudioSignalPlayer::JustPlay("Player_slide_end", player.GetEntityId());
				}

				m_lazyExitTimer = 0.0f;
			}
			break;
		default:
			{
				if (m_slideAction)
				{
					m_slideAction->ForceFinish();
					m_slideAction->ClearBackPointer();
					SAFE_RELEASE(m_slideAction);
				}

				m_lazyExitTimer = m_kickTimer = 0.0f;
			}
		}
	}
}

void CSlideController::LazyExitSlide(CPlayer& player)
{
	// handle corner cases where LazyExitSlide is called while we are not sliding
	if (m_state == eSlideState_None)
		return;
	
	if( m_lazyExitTimer > 0.0f )
	{
		GoToState( player, eSlideState_ExitingSlide );
	}
	else
	{
		m_lazyExitTimer = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	}
}

void CSlideController::ExitedSlide(CPlayer& player)
{
	if (m_slideAction)
	{
		m_slideAction->ClearBackPointer();
		SAFE_RELEASE(m_slideAction);
	}

	GoToState(player, eSlideState_None);
}

bool CSlideController::CanDoSlideKick(const CPlayer& player) const
{
	const SPlayerSlideControl& slideCvars = gEnv->bMultiplayer ? g_pGameCVars->pl_sliding_control_mp : g_pGameCVars->pl_sliding_control;

	//--- Fail if range is invalid, & then externally we can exit slide
	const Vec3 baseFwd = player.GetEntity()->GetWorldTM().GetColumn1();
	const Vec3 lookFwd = player.GetViewQuatFinal().GetColumn1();
	return (m_state == eSlideState_Sliding) && (baseFwd.Dot(lookFwd) >= MIN_KICK_DP) && (player.m_stats.speedFlat > slideCvars.min_speed);
}

float CSlideController::DoSlideKick(CPlayer& player)
{
	if( m_slideAction )
	{
		m_slideAction->DoSlideKick();
	}

	const static float kKickTimer = 0.14f;

	m_kickTimer = kKickTimer;

	return kKickTimer;
}

Vec3 CSlideController::GetTargetDirection( CPlayer& player )
{
	const EntityId closeCombatTargetId = g_pGame->GetAutoAimManager().GetCloseCombatSnapTarget();
	if (!closeCombatTargetId)
		return ZERO;

	IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(closeCombatTargetId);

	if(pTargetEntity)
	{
		const Vec3 targetPos = pTargetEntity->GetWorldPos();
		const Vec3 playerPos = player.GetEntity()->GetWorldPos();

		return (targetPos - playerPos).GetNormalized();
	}

	return ZERO;
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CSlideController::OnSlideStateChanged( CPlayer& player )
{
	EntityId leftHandEntId = player.GetLeftHandObject();
	IEntity *pLeftHandEnt = (leftHandEntId != 0) ? gEnv->pEntitySystem->GetEntity(leftHandEntId) : NULL;

	switch( m_state )
	{
	case eSlideState_Sliding:
		{
			player.m_params.viewLimits.SetViewLimit(player.GetEntity()->GetWorldTM().GetColumn1(),
																							DEG2RAD(player.m_playerRotationParams.GetHorizontalAimParams(SPlayerRotationParams::EAimType_SLIDING, !player.IsThirdPerson()).angle_max),
																							0.f,
																							DEG2RAD(player.m_playerRotationParams.GetVerticalAimParams(SPlayerRotationParams::EAimType_SLIDING, !player.IsThirdPerson()).angle_max),
																							DEG2RAD(player.m_playerRotationParams.GetVerticalAimParams(SPlayerRotationParams::EAimType_SLIDING, !player.IsThirdPerson()).angle_min),
																							SViewLimitParams::eVLS_Slide);
			
	 		player.SetSpeedMultipler(SActorParams::eSMR_Internal, 0.0f);
			player.OnSetStance(STANCE_CROUCH);

			if (!gEnv->bMultiplayer && pLeftHandEnt)
				pLeftHandEnt->Hide(true);
		}
		break;
	default: 
		{
			player.m_params.viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Slide);
			player.SetSpeedMultipler(SActorParams::eSMR_Internal, 1.0f);

			CPlayerInput* pPlayerInput = static_cast<CPlayerInput*>(player.GetPlayerInput());
			
			if (pPlayerInput && (pPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT))
			{
				const bool remainCrouched = pPlayerInput->IsCrouchButtonDown() && !player.m_stateMachineMovement.StateMachineAnyActiveFlag(EPlayerStateFlags_SprintPressed);
				if (remainCrouched)
				{
					pPlayerInput->ClearSprintAction();
					player.OnSetStance(STANCE_CROUCH);
				}
				else
				{
					pPlayerInput->ClearCrouchAction();
					player.OnSetStance(STANCE_STAND);
				}
			}

			if (m_state == eSlideState_None)
			{
				player.m_animationProxy.SetCanMixUpperBody(true);

				if (pLeftHandEnt)
					pLeftHandEnt->Hide(false);
			}

			// DT: 22234: MP:DEMO:CAMERA:Camera transition not smooth during slide move.
			// Once more the camera glitched when you exit the slide to a stand.
			// This time, it was because the stance was 'crouch' regardless of the fact that
			// crouch was not pressed due to what looked like a dirty stance in the CPlayer::GetStanceViewOffset.
			// No reason not to update the stance here - just just do it unconditionally, problem solved.
			player.UpdateStance();
		}
		break;
	}

	CHANGED_NETWORK_STATE( (&player), CPlayer::ASPECT_INPUT_CLIENT);

	// Record 'Slide' telemetry stats.
	CStatsRecordingMgr::TryTrackEvent(&player, eGSE_Slide, m_state == eSlideState_Sliding);
}

