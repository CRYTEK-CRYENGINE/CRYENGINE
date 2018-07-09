// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "PlayerMovementController.h"
#include <CryGame/GameUtils.h>
#include <CrySystem/ITimer.h>
#include "IVehicleSystem.h"
#include "IItemSystem.h"
#include "GameCVars.h"
#include "NetInputChainDebug.h"
#include "Item.h"
#include "IPlayerInput.h"
#include "PlayerRotation.h"
#include "CornerSmoother.h"
#include <CryAISystem/IAIActor.h> // to get to the movement ability & path follower

//#include "Utility/CryWatch.h"

#define ENABLE_NAN_CHECK

#undef CHECKQNAN_FLT
#ifdef ENABLE_NAN_CHECK
#define PMC_CHECKQNAN_FLT(x) \
	assert(((*alias_cast<unsigned*>(&x))&0xff000000) != 0xff000000u && (*alias_cast<unsigned*>(&x) != 0x7fc00000))
#else
#define PMC_CHECKQNAN_FLT(x) (void*)0
#endif

#define PMC_CHECKQNAN_VEC(v) \
	PMC_CHECKQNAN_FLT(v.x); PMC_CHECKQNAN_FLT(v.y); PMC_CHECKQNAN_FLT(v.z)

// minimum desired speed accepted
static const float MIN_DESIRED_SPEED = 0.00002f;

// anticipation look ik
static const float ANTICIPATION_COSINE_ANGLE = DEG2RAD(45);
// amount of time to look for
static const CTimeValue LOOK_TIME = 2.0f;

// IDLE Checking stuff from here on
#define DEBUG_IDLE_CHECK
#undef  DEBUG_IDLE_CHECK

static const float IDLE_CHECK_TIME_TO_IDLE = 5.0f;
static const float IDLE_CHECK_MOVEMENT_TIMEOUT = 3.0f;
// ~IDLE Checking stuff

#define lerpf(a, b, l) (a + (l * (b - a)))
static float s_dbg_my_white[4] = {1,1,1,1};



CPlayerMovementController::CPlayerMovementController(CPlayer* pPlayer) :
	m_pPlayer(pPlayer),
	m_animTargetSpeed(-1.f),
	m_animTargetSpeedCounter(0),
	m_movementTransitionsController(pPlayer),
	m_cornerSmoother(pPlayer),
	m_lookTarget(ZERO),
	m_aimTarget(ZERO),
	m_fireTarget(ZERO),
	m_lastReqTurnSpeed(0.0f),
	m_wasTurning(false),
	m_zRotationRate(0.0f)
{
	Reset();
	m_lastRotX = 0.0f;
	m_lastRotZ = 0.0f;
	m_timeTurningInSameDirX = 0.0f;
	m_timeTurningInSameDirZ = 0.0f;
}

CPlayerMovementController::~CPlayerMovementController()
{
}

void CPlayerMovementController::Reset()
{
	Construct(m_state);
	m_atTarget = false;
	m_desiredSpeed = 0.0f;
	m_usingAimIK = m_usingLookIK = false;
	m_aimClamped = false;

	m_targetStance = STANCE_NULL;

	if(!GetISystem()->IsSerializingFile() == 1)
		UpdateMovementState( m_currentMovementState );

	m_lookTarget.zero();
	m_aimTarget.zero();
	m_fireTarget.zero();

	m_movementTransitionsController.Reset();
	m_cornerSmoother.Reset();
	m_cornerSmoother2.Reset();
	m_zRotationRate = 0.0f;

	m_pExactPositioning.reset();
}

inline float filterFloat(float value, float target, float maxChange)
{
	float absChange = fabsf(maxChange);

	if(target > value)
	{
		return min(target, value + absChange);
	}
	else
	{
		return max(target, value - absChange);
	}
}

void CPlayerMovementController::ApplyControllerAcceleration( float& xRot, float& zRot, float dt )
{
	float initialRotX = xRot;
	float initialRotZ = zRot;

	if(g_pGameCVars->pl_aim_acceleration_enabled)
	{
		//This is now a much simpler acceleration model
		const float sensitivityValue = (gEnv->bMultiplayer ? g_pGameCVars->cl_sensitivityControllerMP : g_pGameCVars->cl_sensitivityController);
		float controllerSensitivity = clamp_tpl(sensitivityValue * 0.5f, 0.0f, 1.0f);

		float fractionIncrease = 0.25f + controllerSensitivity; //0.25 to 1.25f

		float newXRot = xRot;
		float newZRot = zRot;

		const float kTimeToFullMultiplier = 0.33f;
		const float kInvTimeToFullMultiplier = 1.0f / kTimeToFullMultiplier;

		float timeAtHighDeflectionX = min((float)__fsel(fabsf(xRot) - 0.95f, m_timeTurningInSameDirX + dt, 0.0f), kTimeToFullMultiplier);
		float timeAtHighDeflectionZ = min((float)__fsel(fabsf(zRot) - 0.95f, m_timeTurningInSameDirZ + dt, 0.0f), kTimeToFullMultiplier);

		float kFullXMultiplier = (g_pGameCVars->controller_full_turn_multiplier_x - 1.0f) * fractionIncrease;
		float kFullZMultiplier = (g_pGameCVars->controller_full_turn_multiplier_z - 1.0f) * fractionIncrease;

		newXRot = newXRot + (newXRot * kFullXMultiplier * timeAtHighDeflectionX * kInvTimeToFullMultiplier);
		newZRot = newZRot + (newZRot * kFullZMultiplier * timeAtHighDeflectionZ * kInvTimeToFullMultiplier);		

		m_timeTurningInSameDirX = timeAtHighDeflectionX;
		m_timeTurningInSameDirZ = timeAtHighDeflectionZ;
		
		xRot = newXRot;
		zRot = newZRot;
	}

	m_lastRotX = xRot;
	m_lastRotZ = zRot;
}


bool CPlayerMovementController::RequestMovement( CMovementRequest& request )
{
	assert(!request.HasStance() || (request.GetStance() > STANCE_NULL && request.GetStance()  < STANCE_LAST));

	if (!m_pPlayer->IsPlayer())
	{
		if (IVehicle* pVehicle = m_pPlayer->GetLinkedVehicle())
		{
			if (IMovementController* pController = pVehicle->GetPassengerMovementController(m_pPlayer->GetEntityId()))
			{
				IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_pPlayer->GetEntityId());
				pSeat->ProcessPassengerMovementRequest(request);
			}
		}

		if (gEnv->bServer)
			CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );
	}	

	// because we're not allowed to "commit" values here, we make a backup,
	// perform modifications, and then copy the modifications make if everything
	// was successful
	CMovementRequest state = m_state;
	// we have to process right through and not early out, because otherwise we
	// won't modify request to a "close but correct" state
	bool ok = true;

	if (request.HasMoveTarget())
	{
		// TODO: check validity of getting to that target
		state.SetMoveTarget( request.GetMoveTarget() );
		m_atTarget = false;

		float distanceToEnd(request.GetDistanceToPathEnd());
		if (distanceToEnd>0.001f)
			state.SetDistanceToPathEnd(distanceToEnd);
		else
			state.SetDistanceToPathEnd(0.0f);

		if (request.HasDirectionOffFromPath())
			state.SetDirectionOffFromPath(request.GetDirOffFromPath());
	}
	else if (request.RemoveMoveTarget())
	{
		state.ClearMoveTarget();
		state.ClearDesiredSpeed();
		state.ClearDistanceToPathEnd();
		state.ClearDirectionOffFromPath();
	}

	if (request.HasInflectionPoint())
	{
		// NOTE: Having an inflection point indicates the move target will be absolute data.
		// Previously the move target was just (position + velocity.GetNormalized()).

		// This is the estimated position of the next move target after reaching the current move target
		state.SetInflectionPoint(request.GetInflectionPoint());
	}
	else if (request.RemoveInflectionPoint())
	{
		state.ClearInflectionPoint();
	}

	if (request.HasStance())
	{
		state.SetStance( request.GetStance() );
	}
	else if (request.RemoveStance())
	{
		state.ClearStance();
	}

	if (request.HasDesiredSpeed())
	{
		bool hasMoveTarget = (request.HasMoveTarget() || (m_state.HasMoveTarget() && request.RemoveMoveTarget()));
		if (!hasMoveTarget && request.GetDesiredSpeed() > 0.0f)
		{
			request.SetDesiredSpeed(0.0f, 0.0f);
			ok = false;
		}
		else
		{
			state.SetDesiredSpeed( request.GetDesiredSpeed(), request.GetDesiredTargetSpeed() );
		}
	}
	else if (request.RemoveDesiredSpeed())
	{
		state.RemoveDesiredSpeed();
	}

	if (request.HasAimTarget())
	{
		state.SetAimTarget( request.GetAimTarget() );
	}
	else if (request.RemoveAimTarget())
	{
		state.ClearAimTarget();
		//state.SetNoAiming();
	}

	if (request.HasBodyTarget())
	{
		state.SetBodyTarget( request.GetBodyTarget() );
	}
	else if (request.RemoveBodyTarget())
	{
		state.ClearBodyTarget();
		//state.SetNoBodying();
	}

	if (request.HasFireTarget())
	{
		state.SetFireTarget( request.GetFireTarget() );
		//forcing fire target here, can not wait for it to be set in the dateNormalate.
		//if don't do it - first shot of the weapon might be in some undefined direction (ProsessShooting is done right after this 
		//call in AIProxy). This is particularly problem for RPG shooting
		m_currentMovementState.fireTarget = request.GetFireTarget();
		// the weaponPosition is from last update - might be different at this moment, but should not be too much
		m_currentMovementState.fireDirection = (m_currentMovementState.fireTarget - m_currentMovementState.weaponPosition).GetNormalizedSafe();
	}
	else if (request.RemoveFireTarget())
	{
		state.ClearFireTarget();
		//state.SetNoAiming();
	}

	if (request.HasLookTarget())
	{
		// TODO: check against move direction to validate request
		state.SetLookTarget( request.GetLookTarget(), request.GetLookImportance() );
	}
	else if (request.RemoveLookTarget())
	{
		state.ClearLookTarget();
	}

	if (request.HasLookStyle())
	{
		state.SetLookStyle( request.GetLookStyle() );
	}

	state.SetAllowLowerBodyToTurn(request.AllowLowerBodyToTurn());

	if (request.HasBodyOrientationMode())
	{
		state.SetBodyOrientationMode(request.GetBodyOrientationMode());
	}

	if (request.HasStance())
	{
		state.SetStance( request.GetStance() );
	}
	else if (request.RemoveStance())
	{
		state.ClearStance();
	}

	if (request.HasLean())
	{
		state.SetLean( request.GetLean() );
	}
	else if (request.RemoveLean())
	{
		state.ClearLean();
	}

	if (request.HasPeekOver())
	{
		state.SetPeekOver( request.GetPeekOver() );
	}
	else if (request.RemovePeekOver())
	{
		state.ClearPeekOver();
	}

	if (request.ShouldJump())
		state.SetJump();

	state.SetAllowStrafing(request.AllowStrafing());

	if (request.HasNoAiming())
		state.SetNoAiming();

	if (request.HasDeltaMovement())
		state.AddDeltaMovement( request.GetDeltaMovement() );
	if (request.HasDeltaRotation())
		state.AddDeltaRotation( request.GetDeltaRotation() );

	if (request.HasPseudoSpeed())
		state.SetPseudoSpeed(request.GetPseudoSpeed());
	else if (request.RemovePseudoSpeed())
		state.ClearPseudoSpeed();

	if (request.HasPrediction())
		state.SetPrediction( request.GetPrediction() );
	else if (request.RemovePrediction())
		state.ClearPrediction();

	state.SetAlertness(request.GetAlertness());

	if (request.HasContext())
		state.SetContext(request.GetContext());
	else if (request.RemoveContext())
		state.ClearContext();

	if (request.HasDesiredBodyDirectionAtTarget())
		state.SetDesiredBodyDirectionAtTarget(request.GetDesiredBodyDirectionAtTarget());
	else if (request.RemoveDesiredBodyDirectionAtTarget())
		state.ClearDesiredBodyDirectionAtTarget();

	state.SetMannequinTagRequest(request.GetMannequinTagRequest());

	state.SetAICoverRequest(request.GetAICoverRequest());

	// commit modifications
	if (ok)
	{
		m_state = state;
	}

	if (request.HasActorTarget())
	{
		const SActorTargetParams& p = request.GetActorTarget();

		// only create ExactPositioning when needed
		if (!m_pExactPositioning.get())
		{
			IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
			CRY_ASSERT(pAnimatedCharacter);
			if (pAnimatedCharacter)
			{
				m_pExactPositioning.reset(new CExactPositioning(pAnimatedCharacter));
			}
		}

		if (m_pExactPositioning.get())
		{
			m_pExactPositioning->SetActorTarget(p);
			m_animTargetSpeed = p.speed;
		}
	}
	else if (request.RemoveActorTarget())
	{
		if (m_pExactPositioning.get())
			m_pExactPositioning->ClearActorTarget();
	}

	return ok;
}

ILINE static f32 GetYaw( const Vec3& v0, const Vec3& v1 )
{
  float a0 = atan2f(v0.y, v0.x);
  float a1 = atan2f(v1.y, v1.x);
  float a = a1 - a0;
  if (a > gf_PI) a -= gf_PI2;
  else if (a < -gf_PI) a += gf_PI2;
  return a;
}

static float yellow[4] = {1,1,0,1};
static CTimeValue lastTime;
static int y = 100;

#if !defined(_RELEASE)
static void DebugDrawWireFOVCone(IRenderer* pRend, const Vec3& pos, const Vec3& dir, float rad, float fov, const ColorB& col)
{
	const unsigned npts = 32;
	const unsigned npts2 = 16;
	Vec3	points[npts];
	Vec3	pointsx[npts2];
	Vec3	pointsy[npts2];

	Matrix33	base;
	base.SetRotationVDir(dir);

	fov *= 0.5f;

	float coneRadius = sinf(fov) * rad;
	float coneHeight = cosf(fov) * rad;

	for(unsigned i = 0; i < npts; i++)
	{
		float	a = ((float)i / (float)npts) * gf_PI2;
		float rx = cosf(a) * coneRadius;
		float ry = sinf(a) * coneRadius;
		points[i] = pos + base.TransformVector(Vec3(rx, coneHeight, ry));
	}

	for(unsigned i = 0; i < npts2; i++)
	{
		float	a = -fov + ((float)i / (float)(npts2-1)) * (fov*2);
		float rx = sinf(a) * rad;
		float ry = cosf(a) * rad;
		pointsx[i] = pos + base.TransformVector(Vec3(rx, ry, 0));
		pointsy[i] = pos + base.TransformVector(Vec3(0, ry, rx));
	}

	pRend->GetIRenderAuxGeom()->DrawPolyline(points, npts, true, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(pointsx, npts2, false, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(pointsy, npts2, false, col);

	pRend->GetIRenderAuxGeom()->DrawLine(points[0], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts/4], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts/2], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts/2+npts/4], col, pos, col);
}
#endif


// Changes pvTarget so the vector from vCenter to pvTarget is within a certain yaw/pitch range from vForwardDir.
//
// * Asserts vForwardDir lies in the horizontal plane
// * Asserts vForwardDir is normalized
//
// * Assumes 'up' is the Z axis
// * When pvTarget is very near to vCenter, just returns a forward pointing target and returns true (no assert)
//
// Returns true when yaw or patch angle was clamped (or when pvTarget was very close to vCenter)
static bool ClampTargetPointToAngles(Vec3*const pvTarget, const Vec3& vCenter, const Vec3& vForwardDir, const float maxYawAngle, const float maxPitchAngle)
{
	CRY_ASSERT(fabs_tpl(vForwardDir.z) <= 0.0001f);
	CRY_ASSERT(vForwardDir.IsUnit());

	Vec3 vTargetDir = *pvTarget - vCenter;
	const float dist = vTargetDir.NormalizeSafe();

	const bool bTargetTooNear = (dist < 0.01f);
	if (!bTargetTooNear)
	{
		const float yawAngle = Ang3::CreateRadZ(vForwardDir, vTargetDir);
		const float pitchAngle = asin(vTargetDir.z);

		const float clampedYawAngle = clamp_tpl(yawAngle, -maxYawAngle, maxYawAngle);
		const float clampedPitchAngle = clamp_tpl(pitchAngle, -maxPitchAngle, maxPitchAngle);

		const bool bClampedYaw = fabsf(clampedYawAngle - yawAngle) > 0.0001f;
		const bool bClampedPitch = fabsf(clampedPitchAngle - pitchAngle) > 0.0001f;
		if (bClampedYaw || bClampedPitch)
		{
			const Vec3 vRightDir(vForwardDir.y, -vForwardDir.x, 0.f);	// = vForwardDir.Cross(Vec3Constants<float>::fVec3_OneZ);
			*pvTarget = vCenter + Quat::CreateRotationZ(clampedYawAngle) * Quat::CreateRotationAA(clampedPitchAngle, vRightDir.GetNormalizedSafe()) * vForwardDir * dist;
			return true;
		}
		else
		{
			return false;
		}
	}
	else // bTargetTooNear
	{
		*pvTarget = vCenter + vForwardDir; 	// Target is too near to bother clamping, the math will get imprecise, just aim forward
		return true;
	}
}


void CPlayerMovementController::Update( float frameTime, SActorFrameMovementParams& params )
{
#if !defined(_RELEASE)
	if (g_pGame->GetCVars()->g_debugaimlook)
	{
		if (gEnv->pTimer->GetFrameStartTime()!=lastTime)
		{
			y = 100;
			lastTime = gEnv->pTimer->GetFrameStartTime();
		}
	}
#endif

	if (m_pExactPositioning.get())
		m_pExactPositioning->Update();

	///// 1 //////////////////////////////////////////////////////////////////////////////////////////
	// Initialize a few of params' members

	params.desiredVelocity.zero();
	params.lookTarget = m_currentMovementState.eyePosition + m_currentMovementState.eyeDirection * 10.0f;

	/*if(!m_pPlayer->m_isPlayer)
	{
		if (IEntity* pAimTarget = gEnv->pEntitySystem->FindEntityByName("Aim_Direction"))
		{
			Vec3 aimTarget = pAimTarget->GetWorldPos();
			m_state.SetAimTarget(aimTarget);
		}
	}*/

	params.aimTarget = m_state.HasAimTarget()
		? m_state.GetAimTarget()
		: m_currentMovementState.weaponPosition + m_currentMovementState.aimDirection * 10.0f;

	params.lookIK = false;
	params.aimIK = false;
	params.deltaAngles.Set(0.0f, 0.0f, 0.0f);
	params.allowStrafe = m_state.AllowStrafing();


	///// 2 //////////////////////////////////////////////////////////////////////////////////////////
	// Stance control (do this before speed control because max speed depends on stance)

	EStance ePlayerStance = m_pPlayer->GetStance();
	const float stanceSpeed = m_pPlayer->GetStanceMaxSpeed(ePlayerStance);
	const bool isClient = m_pPlayer->IsClient();

	if (m_state.HasStance() && m_state.GetStance() == STANCE_CROUCH)
	{
		if ((ePlayerStance != STANCE_CROUCH) && (m_pPlayer->GetActorStats()->inAir > 0.01f))
		{
			m_state.SetStance(ePlayerStance);
		}
	}

	if (m_state.AlertnessChanged())
	{
		if (m_state.GetAlertness() == 2)
		{
			if (!m_state.HasStance() || m_state.GetStance() == STANCE_RELAXED)
			{
				m_state.SetStance(STANCE_STAND);
			}
		}
	}

	const SExactPositioningTarget* pAnimTarget = GetExactPositioningTarget();
	if (pAnimTarget && pAnimTarget->activated && m_targetStance != STANCE_NULL)
	{
		m_state.SetStance( m_targetStance );
	}

	if (m_state.HasStance())
	{
		params.stance = m_state.GetStance();
	}


	///// 3 //////////////////////////////////////////////////////////////////////////////////////////
	// Move target control

	IEntity* pEntity = m_pPlayer->GetEntity();
	const Vec3 currentBodyDirection(pEntity->GetRotation().GetColumn1());
	const IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	const Vec3 currentAnimBodyDirection = pAnimatedCharacter ? pAnimatedCharacter->GetAnimLocation().q.GetColumn1() : currentBodyDirection;
	bool isMoving = m_pPlayer->IsMoving();
	Vec3 moveDirection = isMoving ? m_pPlayer->GetLastRequestedVelocity().GetNormalized() : currentBodyDirection;

	bool hasLockedBodyTarget = false;
	Vec3 bodyTarget(ZERO);
	const char * bodyTargetType = "none";

	Vec3 playerPos = pEntity->GetWorldPos();

	// potentially override our normal targets due to a targeted animation request
	bool hasMoveTarget;
	Vec3 moveTarget;
	const char* moveTargetType;
	if (m_state.HasMoveTarget() && m_pPlayer->CanMove())
	{
		hasMoveTarget = true;
		moveTarget = m_state.GetMoveTarget();
		moveTargetType = "steering";
	}
	else
	{
		hasMoveTarget = false;
		moveTarget.zero();
		moveTargetType = "none";
	}

	// animtarget control
	if (pAnimTarget != NULL)		
	{
		if (pAnimTarget->preparing)
		{
			Vec3 animTargetFwdDir = pAnimTarget->location.q.GetColumn1();

			Vec3 moveDirection2d = hasMoveTarget ? moveDirection : (pAnimTarget->location.t - playerPos);
			moveDirection2d.z = 0.0f;
			moveDirection2d.NormalizeSafe(animTargetFwdDir);

			float distanceToTarget = (pAnimTarget->location.t - playerPos).GetLength2D();
			float fraction = (distanceToTarget - 0.2f) / 1.8f;
			float blend = clamp_tpl(fraction, 0.0f, 1.0f);
			animTargetFwdDir.SetSlerp(animTargetFwdDir, moveDirection2d, blend);

			float animTargetFwdDepthFraction = max(0.0f, animTargetFwdDir.Dot(moveDirection2d));
			float animTargetFwdDepth = LERP(1.0f, 3.0f, animTargetFwdDepthFraction);

			bodyTarget = pAnimTarget->location.t + animTargetFwdDepth * animTargetFwdDir;
			bodyTargetType = "animation";
			hasLockedBodyTarget = true;
			params.allowStrafe = true;

			if (!hasMoveTarget || pAnimTarget->notAiControlledAnymore || (distanceToTarget < 1.0f))
			{
				moveTarget = pAnimTarget->location.t;
				moveTargetType = "animation";
				hasMoveTarget = true;
				pAnimTarget->notAiControlledAnymore = true;
			}
		}
	}

	m_animTargetSpeedCounter -= static_cast<int>(m_animTargetSpeedCounter != 0);

	const Quat qEntityWorldRotation = pEntity->GetWorldRotation();

	///// 4 //////////////////////////////////////////////////////////////////////////////////////////
	// Speed control

	if (hasMoveTarget)
	{
		Vec3 desiredMovement = moveTarget - playerPos;

		// should do the rotation thing?
		desiredMovement.z = 0;

		float distance = desiredMovement.len();

		if (distance > 0.01f)
		{
			desiredMovement /= distance;

			float desiredSpeed;
			if (!(pAnimTarget && (pAnimTarget->activated || m_animTargetSpeedCounter) && m_animTargetSpeed >= 0.0f))
			{
				desiredSpeed = m_state.HasDesiredSpeed() ? m_state.GetDesiredSpeed() : 1.f;

				if (pAnimTarget && pAnimTarget->preparing)  
				{
					desiredSpeed = max(desiredSpeed, m_desiredSpeed);
					desiredSpeed = max(1.0f, desiredSpeed);

					float approachSpeed = frameTime == 0.0f ? 0.0f : (distance * __fres(frameTime)) * 0.4f;
					desiredSpeed = min(desiredSpeed, approachSpeed);
				}
			}
			else
			{
				desiredSpeed = m_animTargetSpeed;
				if (pAnimTarget->activated)
				{
					m_animTargetSpeedCounter = 4;
				}
			}

			NETINPUT_TRACE(m_pPlayer->GetEntityId(), desiredSpeed);
			NETINPUT_TRACE(m_pPlayer->GetEntityId(), stanceSpeed);
			NETINPUT_TRACE(m_pPlayer->GetEntityId(), desiredMovement);
			if ((desiredSpeed > MIN_DESIRED_SPEED) && stanceSpeed>0.001f)
			{
				//calculate the desired speed amount (0-1 length) in world space
				params.desiredVelocity = desiredMovement * desiredSpeed / stanceSpeed;

				//convert it to the Actor local space
				moveDirection = params.desiredVelocity.GetNormalizedSafe(Vec3Constants<float>::fVec3_Zero);
				params.desiredVelocity = qEntityWorldRotation.GetInverted() * params.desiredVelocity;
			}
		}
	}


	///// 5 //////////////////////////////////////////////////////////////////////////////////////////
	// Body target

	Vec3 vForcedLookDir(ZERO);
	const bool bHasForcedLookDir = (m_pPlayer->GetForcedLookDir(vForcedLookDir));

	if (!hasLockedBodyTarget)
	{
		if (bHasForcedLookDir)
		{
			bodyTarget = playerPos + vForcedLookDir;
			bodyTargetType = "forcedLook";
			hasLockedBodyTarget = true;
		}
		else if (m_state.HasBodyTarget())
		{
			bodyTarget = m_state.GetBodyTarget();
			bodyTargetType = "requested";
			hasLockedBodyTarget = true;
		}
		else if(!m_state.AllowLowerBodyToTurn())
		{
			bodyTarget = playerPos + currentBodyDirection;
			bodyTargetType = "lockedturn";
			hasLockedBodyTarget = true;
		}
		else if (hasMoveTarget)
		{
			bodyTarget = playerPos + moveDirection;
			bodyTargetType = "movement";
		}
		else
		{
			bodyTarget = playerPos + currentBodyDirection;
			bodyTargetType = "current";
		}
	}


	///// 5 //////////////////////////////////////////////////////////////////////////////////////////
	// Look and aim direction

	Vec3 eyePosition(playerPos.x, playerPos.y, m_currentMovementState.eyePosition.z);

	EMovementControlMethod mcmH = pAnimatedCharacter ? pAnimatedCharacter->GetMCMH() : eMCM_Undefined;

	bool hasLookTarget = false;
	Vec3 tgt = eyePosition + 5.0f * currentBodyDirection;
	const char * lookType = "none";
	ITimer * pTimer = gEnv->pTimer;
	CTimeValue now = pTimer->GetFrameStartTime();
	ELookStyle lookStyle;
	if (bHasForcedLookDir)
	{
		hasLookTarget = true;
		tgt = eyePosition + 5.0f * vForcedLookDir;
		params.allowStrafe = true;
		lookType = "forcedLook";
		lookStyle = LOOKSTYLE_HARD;
	}
	else if (m_state.HasLookTarget())
	{
		hasLookTarget = true;
		tgt = m_state.GetLookTarget();
		lookType = "look";
		lookStyle = m_state.HasLookStyle() ? m_state.GetLookStyle() : LOOKSTYLE_HARD;
		if (lookStyle == LOOKSTYLE_DEFAULT)
			lookStyle = LOOKSTYLE_HARD;
	}
	else if (m_state.HasAimTarget())
	{
		hasLookTarget = true;
		tgt = m_state.GetAimTarget();
		lookType = "aim";
		lookStyle = LOOKSTYLE_HARD_NOLOWER;
	}

	Vec3 lookTarget = tgt;

	bool hasAimTarget;
	const char* aimType;
	if (m_state.HasAimTarget())
	{
		hasAimTarget = true;
		tgt = m_state.GetAimTarget();
		aimType = "aim";
	}
	else
	{
		hasAimTarget = false;
		aimType = lookType;
	}

	Vec3 aimTarget = tgt;

	const char * ikType = "none";
	const bool hasControl = (gEnv->bMultiplayer || !pEntity->GetAI() || pEntity->GetAI()->IsEnabled());

	m_aimClamped = false;

	const SActorParams &actorParams = m_pPlayer->GetActorParams();

	if (!isClient && hasControl)
	{
		if (m_pPlayer->IsRemote())
		{
			//--- For remote characters all the clamping will be done on the local player so do none here
			//--- Turn the body immediately, the AG will take care of selectively turning him

			params.aimTarget = hasAimTarget ? aimTarget : lookTarget;
			params.lookTarget = lookTarget;
			params.aimIK = true;
			params.lookIK = false;
			bodyTarget = params.lookTarget;
		}
		else if ((static_cast<SPlayerStats*>(m_pPlayer->GetActorStats()))->mountedWeaponID)
		{
			params.aimTarget = hasAimTarget ? aimTarget : lookTarget;

			// Luciano - prevent snapping of aim direction out of existing horizontal angle limits
			float limitH = actorParams.viewLimits.GetViewLimitRangeH();
			IEntity *pWeaponEntity = gEnv->pEntitySystem->GetEntity(m_pPlayer->GetActorStats()->mountedWeaponID);

			// m_currentMovementState.weaponPosition is offset to the real weapon position
			Vec3 weaponPosition (pWeaponEntity ? pWeaponEntity->GetWorldPos() : m_currentMovementState.weaponPosition); 

			if(limitH > 0 && limitH < gf_PI)
			{
				Vec3 limitAxisY(actorParams.viewLimits.GetViewLimitDir());
				limitAxisY.z = 0;
				Vec3 limitAxisX(limitAxisY.y, -limitAxisY.x, 0);
				limitAxisX.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
				limitAxisY.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
				Vec3 aimDirection(params.aimTarget - weaponPosition);//m_currentMovementState.weaponPosition);
				float xComponent = limitAxisX.Dot(aimDirection);
				float yComponent = limitAxisY.Dot(aimDirection);
				float len = sqrt_tpl(square(xComponent) + square(yComponent));
				len = max(len,2.f);// bring the aimTarget away for degenerated cases when it's between firepos and weaponpos
				float angle = atan2_tpl(xComponent, yComponent);
				if (angle < -limitH || angle > limitH)
				{
					Limit(angle, -limitH, limitH);
					xComponent = sinf(angle) * len;
					yComponent = cosf(angle) * len;
					params.aimTarget = weaponPosition + xComponent* limitAxisX + yComponent * limitAxisY + Vec3(0,0, aimDirection.z);
					m_aimClamped = true;
				}

			}

			float limitUp = actorParams.viewLimits.GetViewLimitRangeVUp();
			float limitDown = actorParams.viewLimits.GetViewLimitRangeVDown();
			if(limitUp != 0.f || limitDown != 0.f)
			{
				Vec3 aimDirection(params.aimTarget - weaponPosition);//m_currentMovementState.weaponPosition);
				Vec3 limitAxisXY(aimDirection);
				limitAxisXY.z = 0;
				Vec3 limitAxisZ(Vec3Constants<float>::fVec3_OneZ);

				limitAxisXY.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
				float z = limitAxisZ.Dot(aimDirection);
				float x = limitAxisXY.Dot(aimDirection);
				float len = sqrt_tpl(square(z) + square(x));
				len = max(len,2.f); // bring the aimTarget away for degenerated cases when it's between firepos and weaponpos
				float angle = atan2_tpl(z, x);
				if (angle < limitDown && limitDown!=0 || angle > limitUp && limitUp !=0)
				{
					Limit(angle, limitDown, limitUp);
					float sine;
					float cosine;
					sincos_tpl(angle, &sine, &cosine);
					z = sine * len;
					x = cosine * len;
					params.aimTarget = weaponPosition + z* limitAxisZ + x * limitAxisXY;
					m_aimClamped = true;
				}
			}
			params.aimIK = true;
			params.lookIK = true;

			lookType = aimType = bodyTargetType = "mountedweapon";
		}
		else
		{
			Vec3 limitDirection = currentAnimBodyDirection;
			Vec3 weaponPos(playerPos.x, playerPos.y, m_currentMovementState.weaponPosition.z);

			const float maxLookYawAngle = 0.5f*m_pPlayer->GetLookFOV(actorParams);
			const float maxLookPitchAngle = m_pPlayer->IsPlayer() ? DEG2RAD(90.0f) : DEG2RAD(65.0f);
			const float maxAimYawAngle = 0.5f*actorParams.aimFOVRadians;
			const float maxAimPitchAngle = m_pPlayer->IsPlayer() ? DEG2RAD(90.0f) : DEG2RAD(65.0f);

			CRY_ASSERT(!m_pPlayer->IsPlayer() || (fabsf(maxAimYawAngle - gf_PI*0.5f) < FLT_EPSILON));
			CRY_ASSERT(!m_pPlayer->IsPlayer() || (fabsf(maxAimPitchAngle - gf_PI*0.5f) < FLT_EPSILON));

#if !defined(_RELEASE)
			if (g_pGame->GetCVars()->g_debugaimlook)
			{
				DebugDrawWireFOVCone(gEnv->pRenderer, weaponPos, limitDirection, 2.0f, maxAimYawAngle*2.0f, ColorB(255,255,255));
				DebugDrawWireFOVCone(gEnv->pRenderer, eyePosition, limitDirection, 3.0f, maxLookYawAngle*2.0f, ColorB(0,196,255));
			}
#endif

			bool bHasAimLookBodyTarget = false; // true when aim or look forced a new bodytarget

			if (hasLookTarget)
			{
				params.lookTarget = lookTarget;

				// If we have both a look target and an aim target and they are too far apart,
				// clamp the look target. Aiming is always more important.
				if (hasAimTarget)
				{
					const float maxlookAimAngle = actorParams.maxLookAimAngleRadians;
					const Vec3 aimDir = Vec2(aimTarget - playerPos).GetNormalizedSafe(Vec2(currentAnimBodyDirection));
					::ClampTargetPointToAngles(&params.lookTarget, weaponPos, aimDir, maxlookAimAngle, maxlookAimAngle);
				}

				// When we have a look target, always use look IK. Also when the look direction had to be clamped.
				ikType = "look";
				params.lookIK = true;

				const Vec3 requestedLookTarget = params.lookTarget; // take a copy of the looktarget, because params.lookTarget will be clamped
				::ClampTargetPointToAngles(&params.lookTarget, eyePosition, limitDirection, maxLookYawAngle, maxLookPitchAngle);

				// Turn body a bit towards the look direction (if lookstyle allows it and the body direction is not locked)
				bool bAllowLookTurn = (lookStyle != LOOKSTYLE_HARD_NOLOWER) && (lookStyle != LOOKSTYLE_SOFT_NOLOWER) && !hasLockedBodyTarget;
				if (bAllowLookTurn)
				{
					if (!isMoving)
					{
						// When standing still, turn completely towards looktarget
						// (this relies on other systems to make sure the animated character is not constantly turning)
						bodyTarget = requestedLookTarget;
						bodyTargetType = "lookIdleTurn";
						bHasAimLookBodyTarget = true;
					}
					else if (actorParams.allowLookAimStrafing)
					{
						if (m_state.GetBodyOrientationMode() == HalfwayTowardsAimOrLook)
						{
							// When moving, turn body a bit in the look direction, effectively forcing strafing
							const Vec3 requestedLookDirection = (requestedLookTarget - eyePosition).GetNormalizedSafe();
							const Vec3 nonZeroMoveDirection = moveDirection.GetNormalizedSafe(limitDirection);

							bodyTarget = weaponPos + 3.0f * (requestedLookDirection + nonZeroMoveDirection).GetNormalizedSafe(limitDirection); // take half-angle, which is equivalent to Vec3::CreateSlerp(lookDirection, nonZeroMoveDirection, 0.5f);
							bodyTargetType = "lookStrafeHalf";
							bHasAimLookBodyTarget = true;
						}
						else if (m_state.GetBodyOrientationMode() == FullyTowardsAimOrLook)
						{
							bodyTarget = requestedLookTarget;
							bodyTargetType = "lookStrafe";
							bHasAimLookBodyTarget = true;
						}
					}
				}
			}

			if (hasAimTarget && (ePlayerStance != STANCE_SWIM))
			{
				params.aimTarget = aimTarget;

				// When we have an aim target, always use aim IK. Also when the aim direction had to be clamped.
				ikType = "aim";
				params.aimIK = true;

				const Vec3 requestedAimTarget = params.aimTarget; // take a copy of the aimtarget, because params.aimTarget will be clamped
				m_aimClamped = ::ClampTargetPointToAngles(&params.aimTarget, weaponPos, limitDirection, maxAimYawAngle, maxAimPitchAngle);

				// Turn body a bit towards the aim direction (if the body direction is not locked)
				bool bAllowAimTurn = !hasLockedBodyTarget;
				if (bAllowAimTurn)
				{
					if (!isMoving)
					{
						// When standing still, turn completely towards aimtarget
						// (this relies on other systems to make sure the animated character is not constantly turning)
						bodyTarget = requestedAimTarget;
						bodyTargetType = "aimIdleTurn";
						bHasAimLookBodyTarget = true;
					}
					else if (actorParams.allowLookAimStrafing)
					{
						if (m_state.GetBodyOrientationMode() == HalfwayTowardsAimOrLook)
						{
							// When moving, turn body a bit in the aim direction, effectively forcing strafing
							const Vec3 requestedAimDirection = (requestedAimTarget - weaponPos).GetNormalizedSafe();
							const Vec3 nonZeroMoveDirection = moveDirection.GetNormalizedSafe(limitDirection);

							bodyTarget = weaponPos + 3.0f * (requestedAimDirection + nonZeroMoveDirection).GetNormalizedSafe(limitDirection); // take half-angle, which is equivalent to Vec3::CreateSlerp(aimDirection, nonZeroMoveDirection, 0.5f);
							bodyTargetType = "aimStrafeHalf";
							bHasAimLookBodyTarget = true;
						}
						else if (m_state.GetBodyOrientationMode() == FullyTowardsAimOrLook)
						{
							bodyTarget = requestedAimTarget;
							bodyTargetType = "aimStrafe";
							bHasAimLookBodyTarget = true;
						}
					}
				}
			}

			hasLockedBodyTarget |= bHasAimLookBodyTarget; // lock the body towards the target
		}	
	}

	///// 6 //////////////////////////////////////////////////////////////////////////////////////////
	// Incremental movement

	if (m_state.HasDeltaMovement())
	{
		params.desiredVelocity += m_state.GetDeltaMovement();// NOTE: desiredVelocity is normalized to the maximumspeed for the specifiedstance, so DeltaMovement should be between 0 and 1!
	}

	float pseudoSpeed = 0.0f;
	f32 jukeTurnRateFraction  = 0.0f;


	///// 7 //////////////////////////////////////////////////////////////////////////////////////////
	// Movement transitions

	bool bRanTransitions = false;
	if (m_state.HasPseudoSpeed())
	{
		pseudoSpeed = m_state.GetPseudoSpeed();

	if (m_movementTransitionsController.IsEnabled())
	{
			CRY_ASSERT(!(m_state.HasDeltaMovement() && m_state.HasMoveTarget()));
			const Vec3 finalMoveDirection = m_state.HasDeltaMovement() ? qEntityWorldRotation * params.desiredVelocity.GetNormalizedSafe(currentAnimBodyDirection) : moveDirection;

			m_movementTransitionsController.Update(
				playerPos, pseudoSpeed, currentAnimBodyDirection, finalMoveDirection, hasLockedBodyTarget, pAnimTarget,
				&m_state, &params,  &jukeTurnRateFraction, &bodyTarget, &bodyTargetType);

			bRanTransitions = true;
		}
	}

	if (m_movementTransitionsController.IsEnabled())
	{
		// Cancel any pending/requested transitions
		if (!bRanTransitions)
			m_movementTransitionsController.CancelTransition();

		m_movementTransitionsController.UpdatePathFollowerState();
	}

	///// 8 //////////////////////////////////////////////////////////////////////////////////////////
	// Corner Smoothing

	if (m_pPlayer->IsAIControlled())
	{
		bool disableSmoother = false;
		{
			IF_UNLIKELY(!pAnimatedCharacter)
			{
				disableSmoother = true;
			}
			else if (m_pPlayer->IsDead())
			{
				disableSmoother = true;
			}
			else if (pAnimTarget && pAnimTarget->preparing && pAnimTarget->notAiControlledAnymore)
			{
				disableSmoother = true;
			}
			else if (pEntity->IsHidden() && !(pEntity->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
			{
				disableSmoother = true;
			}
			else
			{
				ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
				if ((pCharacter == NULL) || !pCharacter->IsCharacterVisible())
					disableSmoother = true;
			}
		}

		const int cornerSmoother = disableSmoother ? 0 : std::min((int)g_pGame->GetCVars()->ctrlr_corner_smoother, (int)m_pPlayer->GetActorParams().cornerSmoother);

		// ==========================================================================

		if (cornerSmoother == 2)
		{
			const IAIObject* pAI = pEntity->GetAI();
			const IAIActor* pAIActor = pAI->CastToIAIActor();
			if (pAIActor)
			{
				CornerSmoothing::SState state;
				{
					state.pPathFollower = pAIActor->GetPathFollower();

					state.motion.position3D = playerPos;
					state.motion.position2D = Vec2(playerPos);

					// Try to figure out how much the last run of the movement state machine has changed our request
					// and counter-adjust the entity speed
					const float lastRequestedSpeedFromAnimatedCharacter = m_pPlayer->GetLastRequestedVelocity().GetLength();
					const float movementStateFudgeFactor = ((lastRequestedSpeedFromAnimatedCharacter > FLT_EPSILON) && (m_desiredSpeed > FLT_EPSILON)) ? lastRequestedSpeedFromAnimatedCharacter/m_desiredSpeed : 1.0f;

					const Vec3& lastRequestFromPhysics = pAnimatedCharacter->GetExpectedEntMovement();
					const float lastRequestedSpeedFromPhysics = (frameTime > FLT_EPSILON) ? lastRequestFromPhysics.GetLength()/frameTime : 0.0f; // TODO: Use previous frameTime

					state.motion.speed = (lastRequestedSpeedFromPhysics/movementStateFudgeFactor);

					const Vec2 oldMovementDir = pAnimatedCharacter->GetEntityMovementDirHorizontal();
					if ((state.motion.speed < 0.1f) || (oldMovementDir.IsZeroFast()))
					{
						state.motion.movementDirection2D = Vec2(currentBodyDirection).GetNormalizedSafe(Vec2Constants<float>::fVec2_OneY);
					}
					else
					{
						state.motion.movementDirection2D = Vec2(lastRequestFromPhysics).GetNormalizedSafe(Vec2Constants<float>::fVec2_OneY);
					}

					state.frameTime = frameTime;

					// params contains velocity in local coordinates, normalized by stanceSpeed
					// we undo this here:
					state.desiredSpeed = params.desiredVelocity.GetLength() * stanceSpeed;

					const Vec2 desiredMovement2D = Vec2(qEntityWorldRotation * params.desiredVelocity);
					state.desiredMovementDirection2D = desiredMovement2D.GetNormalized();

					state.hasMoveTarget = m_state.HasMoveTarget();
					if (state.hasMoveTarget)
					{
						state.moveTarget = m_state.GetMoveTarget();
						state.distToPathEnd = std::max(0.0f, m_state.GetDistanceToPathEnd());
					}
					else
					{
						state.distToPathEnd = 0.0f;
					}

					state.hasInflectionPoint = m_state.HasInflectionPoint();
					if (state.hasInflectionPoint)
						state.inflectionPoint = m_state.GetInflectionPoint();

					const AgentMovementAbility& movementAbility = pAIActor->GetMovementAbility();
					CornerSmoothing::SetLimitsFromMovementAbility(movementAbility, ePlayerStance, pseudoSpeed, state.limits);

	#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
					state.pActor = m_pPlayer;
	#endif
				}

				CornerSmoothing::SOutput output;
				const bool hasOutput = m_cornerSmoother2.Update(state, output);

				if (hasOutput)
				{
					// params takes velocity in local coordinates, normalized by stanceSpeed:
					const float normalizedDesiredSpeed = (stanceSpeed>0.001f) ? (output.desiredSpeed / stanceSpeed) : output.desiredSpeed;
					params.desiredVelocity = qEntityWorldRotation.GetInverted() * Vec3(output.desiredMovementDirection2D * normalizedDesiredSpeed);

					if (!hasLockedBodyTarget)
					{
						bodyTarget = playerPos + output.desiredMovementDirection2D;
					}
				}
			}
		}
		else
		{
			m_cornerSmoother2.Reset();
		}

		if (cornerSmoother == 1)
		{
			if (
				m_state.HasMoveTarget() &&
				m_movementTransitionsController.IsEnabled() && // make sure corner smoothing is only executed on C2 AI with transitions (not on helicopters,..)
				!m_movementTransitionsController.IsTransitionRequestedOrPlaying())
			{
				const float desiredSpeed = params.desiredVelocity.GetLength2D() * stanceSpeed;
				const Vec2 oldMovementDir = pAnimatedCharacter ? pAnimatedCharacter->GetEntityMovementDirHorizontal() : Vec2(m_pPlayer->GetLastRequestedVelocity()).GetNormalizedSafe(Vec2Constants<float>::fVec2_Zero);

				if ((oldMovementDir.GetLength2() > 0.0f) && (desiredSpeed > 0.1f))
				{
					// NOTE: pAnimatedCharacter->GetEntitySpeedHorizontal() cannot be trusted with asynchronous physics currently
					const float oldMovementSpeed = /*pAnimatedCharacter ? pAnimatedCharacter->GetEntitySpeedHorizontal() :*/ m_pPlayer->GetLastRequestedVelocity().IsValid() ? m_pPlayer->GetLastRequestedVelocity().GetLength2D() : 0.0f;

					Vec3 desiredMovement = qEntityWorldRotation * params.desiredVelocity;
					desiredMovement.z = 0.0f;
					desiredMovement.Normalize();

					const float distToPathEnd = m_state.GetDistanceToPathEnd();

					float newDesiredSpeed;
					Vec3 newDesiredMovement;

					m_cornerSmoother.Update(
						playerPos, oldMovementDir, oldMovementSpeed, currentAnimBodyDirection,
						desiredSpeed, desiredMovement, distToPathEnd, moveTarget,
						hasLockedBodyTarget,
						actorParams.maxDeltaAngleRateNormal,
						frameTime,
						&newDesiredSpeed, &newDesiredMovement);

					const float normalizedDesiredSpeed = (stanceSpeed>0.001f) ? (newDesiredSpeed / stanceSpeed) : newDesiredSpeed;
					params.desiredVelocity = qEntityWorldRotation.GetInverted() * (newDesiredMovement * normalizedDesiredSpeed);
				}
				else
				{
					m_cornerSmoother.Cancel();
				}
			}
			else
			{
				m_cornerSmoother.Cancel();
			}
		}
		else
		{
			m_cornerSmoother.Cancel();
		}
	}

	///// 9 //////////////////////////////////////////////////////////////////////////////////////////
	// Turning (towards bodyTarget)

	float maxDeltaAngleRate = 0.0f;

	Vec3 viewDir = ((bodyTarget - playerPos).GetNormalizedSafe(Vec3Constants<float>::fVec3_Zero));
	const bool isFirstPersonClient = isClient && !m_pPlayer->IsThirdPerson();
	if (!m_pPlayer->CanTurnBody())
	{
		if (!isClient && ((pAnimTarget == NULL) || (!pAnimTarget->activated)))
		{
			Vec3 localVDir(m_pPlayer->GetViewQuat().GetInverted() * viewDir);
			PMC_CHECKQNAN_VEC(localVDir);
			params.deltaAngles.x += asin_tpl(localVDir.z);
			params.deltaAngles.z += atan2_tpl(-localVDir.x,localVDir.y);
		}
	}
	else if (isFirstPersonClient)
	{

	}
	else if (isClient || gEnv->bMultiplayer)
	{
		m_state.ClearPrediction();

		maxDeltaAngleRate = actorParams.maxDeltaAngleMultiplayer;

		if (!isClient && ((pAnimTarget == NULL) || (!pAnimTarget->activated)))
		{
			Vec3 localVDir(m_pPlayer->GetViewQuat().GetInverted() * viewDir);
			PMC_CHECKQNAN_VEC(localVDir);
			params.deltaAngles.x += asin_tpl(localVDir.z);
			params.deltaAngles.z += atan2_tpl(-localVDir.x,localVDir.y);
		}


		if (!isClient)
		{
			for (int i=0; i<3; i++)
				Limit(params.deltaAngles[i], -maxDeltaAngleRate * frameTime, maxDeltaAngleRate * frameTime);
		}

		PMC_CHECKQNAN_VEC(params.deltaAngles);
	}
	else if ((viewDir.len2() > 0.01f) && !m_movementTransitionsController.IsTransitionRequestedOrPlaying())
	{
		//Vec3 localVDir(m_pPlayer->GetEntity()->GetWorldRotation().GetInverted() * viewDir);
		Vec3 localVDir(m_pPlayer->GetViewQuat().GetInverted() * viewDir);
		
		PMC_CHECKQNAN_VEC(localVDir);

		if ((pAnimTarget == NULL) || (!pAnimTarget->activated))
		{
			if (m_cornerSmoother.IsRunning())
			{
				m_cornerSmoother.SmoothLocalVDir(&localVDir, frameTime);
			}

			params.deltaAngles.x += asin_tpl(localVDir.z);
			params.deltaAngles.z += atan2_tpl(-localVDir.x,localVDir.y);

			if (actorParams.smoothedZTurning && !isMoving)
			{
				float rotation(0.0f);
				SmoothCDWithMaxRate(rotation, m_zRotationRate, frameTime, params.deltaAngles.z, 0.4f, actorParams.maxDeltaAngleRateNormal);
				params.deltaAngles.z = rotation;
			}
		}

		PMC_CHECKQNAN_VEC(params.deltaAngles);


		// Less clamping when approaching animation or juke target.
		if (pAnimTarget && pAnimTarget->preparing)
		{
			float	distanceFraction = clamp_tpl(pAnimTarget->location.t.GetDistance(playerPos) * 2.0f, 0.0f, 1.0f);
			float	animTargetTurnRateFraction = 1.0f - distanceFraction;
			maxDeltaAngleRate = LERP(actorParams.maxDeltaAngleRateNormal, actorParams.maxDeltaAngleRateAnimTarget, animTargetTurnRateFraction);
		}
		else if (jukeTurnRateFraction > 0.0f)
		{
			maxDeltaAngleRate = LERP(maxDeltaAngleRate, actorParams.maxDeltaAngleRateJukeTurn, jukeTurnRateFraction);
		}
		else if (m_cornerSmoother.IsRunning())
		{
			maxDeltaAngleRate = m_cornerSmoother.GetMaxTurnSpeed();
		}
		else
		{
			maxDeltaAngleRate = actorParams.maxDeltaAngleRateNormal;
		}

		// Pass along a prediction for the turn if standing still (this improves
		// the selection of turn angles in turn LMGs)
		if (!isMoving && !m_state.HasPrediction())
		{
			const float signedAngle = Ang3::CreateRadZ(pEntity->GetForwardDir(), viewDir); // params.deltaAngles.z cannot be used as it is decoupled from the entity direction
			const float unsignedAngle = fabsf(signedAngle);
			if (unsignedAngle > FLT_EPSILON)
			{
				SPredictedCharacterStates prediction;
				prediction.SetParam(eMotionParamID_TurnAngle, signedAngle);

				m_state.SetPrediction(prediction);
			}
		}

		for (int i=0; i<3; i++)
			Limit(params.deltaAngles[i], -maxDeltaAngleRate * frameTime, maxDeltaAngleRate * frameTime);

		PMC_CHECKQNAN_VEC(params.deltaAngles);
	}

	if (m_state.HasDeltaRotation())
	{
		params.deltaAngles += m_state.GetDeltaRotation();
		PMC_CHECKQNAN_VEC(params.deltaAngles);
		ikType = "mouse";
	}

#if !defined(_RELEASE)

	if (!m_pPlayer->IsPlayer())
	{
		m_cornerSmoother.DebugRender(playerPos.z + 0.1f);
	}

	if (g_pGame->GetCVars()->g_debugaimlook)
	{
		IRenderer* pRend = gEnv->pRenderer;
		IRenderAuxText::Draw2dLabel( 10.f, (float)y, 1.5f, s_dbg_my_white, false,
			"%s:  body=%s   look=%s   aim=%s   rotik=%s   move=%s   delta ang=(%+3.3f, %+3.3f, %+3.3f)  maxdeltarate = %3.3f", 
			pEntity->GetName(), bodyTargetType, aimType, lookType, ikType, moveTargetType, 
			params.deltaAngles.x, params.deltaAngles.y, params.deltaAngles.z, maxDeltaAngleRate );
		y += 15;
		if (m_state.GetDistanceToPathEnd() >= 0.0f)
		{
			IRenderAuxText::Draw2dLabel( 10.f, (float)y, 1.5f, yellow, false, "distanceToEnd: %f (%f)", m_state.GetDistanceToPathEnd(), moveTarget.GetDistance(playerPos) );
			y += 15;
		}

		if (m_state.HasAimTarget())
		{
			pRend->GetIRenderAuxGeom()->DrawLine( m_currentMovementState.weaponPosition, ColorF(1,1,1,1), params.aimTarget+Vec3(0,0,0.05f), ColorF(1,1,1,1), 3.0f);
			pRend->GetIRenderAuxGeom()->DrawLine( m_currentMovementState.weaponPosition, ColorF(1,1,1,0.3f), m_state.GetAimTarget(), ColorF(1,1,1,0.3f));
		}
	}
#endif

	// leaning
	params.desiredLean = m_state.HasLean() ? m_state.GetLean() : 0.f;

	// peeking
	params.desiredPeekOver = m_state.HasPeekOver() ? m_state.GetPeekOver() : 0.f;

	params.jump = m_state.ShouldJump();
	m_state.ClearJump();

	// TODO: This should probably be calculate BEFORE it is used (above), or the previous frame's value is used.
	m_desiredSpeed = params.desiredVelocity.GetLength() * stanceSpeed;

	m_state.RemoveDeltaRotation();
	m_state.RemoveDeltaMovement();

	if (params.aimIK)
	{
		m_aimTarget = params.aimTarget;
		if (!params.lookIK)
		{
			params.lookTarget = params.aimTarget;
			// params.lookIK = true; Aimpose should handle the looking
		}
		m_lookTarget = params.lookTarget;
	}
	else
	{
		m_aimTarget = m_lookTarget;
		m_lookTarget = params.lookIK
			? params.lookTarget
			: m_currentMovementState.eyePosition + m_pPlayer->GetEntity()->GetRotation() * FORWARD_DIRECTION * 10.0f;
	}

	m_usingAimIK = params.aimIK;
	m_usingLookIK = params.lookIK;

	if (m_state.HasFireTarget())
	{
		m_fireTarget = m_state.GetFireTarget();
	}
	
	if (pAnimTarget && pAnimTarget->preparing && pseudoSpeed < 0.4f)
		pseudoSpeed = 0.4f;
	
	if (params.stance == STANCE_CROUCH && pseudoSpeed > 0.4f && m_pPlayer->IsPlayer())
		pseudoSpeed = 0.4f;

	if (m_pPlayer->GetAIAnimationComponent())
		m_pPlayer->GetAIAnimationComponent()->GetAnimationState().SetPseudoSpeed(pseudoSpeed);

	bool hasPrediction = m_state.HasPrediction() && (m_state.GetPrediction().IsSet());
	bool hasAnimTarget = (pAnimTarget != NULL) && (pAnimTarget->activated || pAnimTarget->preparing);
	if (hasPrediction && !hasAnimTarget)
	{
		params.prediction = m_state.GetPrediction();
	}
	else
	{
    params.prediction.Reset();
	}

/*
#ifdef USER_dejan
	// Debug Render & Text
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(moveTarget, 0.2f, ColorB(128,255,0, 255), true);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(playerPos, ColorB(255,255,255, 128), moveTarget, ColorB(128,255,0, 128), 0.4f);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(bodyTarget, 0.2f, ColorB(255,128,0, 128), true);
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(moveTarget, ColorB(255,255,255, 128), bodyTarget, ColorB(255,128,0, 128), 0.2f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(playerPos, ColorB(255,255,255, 128), bodyTarget, ColorB(255,128,0, 128), 0.4f);
	}
#endif
*/

	const SMannequinTagRequest& mannequinTagRequest = m_state.GetMannequinTagRequest();
	params.mannequinClearTags = mannequinTagRequest.clearTags;
	params.mannequinSetTags = mannequinTagRequest.setTags;

	params.coverRequest = m_state.GetAICoverRequest();

	// TODO: Remove this when refactoring is done
	//{
	//	IRenderer* pRend = gEnv->pRenderer;
	//	y = 50;
	//	static float s_dbg_my_white[4] = {1,1,1,1};
	//	pRend->Draw2dLabel( 10.f, (float)y, 1.5f, s_dbg_my_white, false, 
	//		"AI:desiredspeed=%3.3f    PMC:desiredspeed=%+3.3f", m_state.HasDesiredSpeed()?m_state.GetDesiredSpeed():0.0f,  m_desiredSpeed );
	//}

#if DEBUG_VELOCITY()
	if (pAnimatedCharacter)
	{
		if (pAnimatedCharacter->DebugVelocitiesEnabled())
		{
			QuatT movement;
			movement.t = qEntityWorldRotation * params.desiredVelocity * (stanceSpeed * frameTime);
			movement.q = Quat::CreateRotationZ(params.deltaAngles.z);
			pAnimatedCharacter->AddDebugVelocity(movement, frameTime, "PlayerMovementController Output", Col_Maroon);
		}
	}
#endif

	NETINPUT_TRACE(m_pPlayer->GetEntityId(), params.desiredVelocity);
	NETINPUT_TRACE(m_pPlayer->GetEntityId(), params.desiredLean);
	NETINPUT_TRACE(m_pPlayer->GetEntityId(), Vec3(params.deltaAngles));
	NETINPUT_TRACE(m_pPlayer->GetEntityId(), params.sprint);
}

void CPlayerMovementController::UpdateMovementState( SMovementState& state )
{
	assert(m_pPlayer);

	IEntity * pEntity = m_pPlayer->GetEntity();
	assert(pEntity);

	ICharacterInstance * pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;

	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
	if (!pSkeletonAnim)
		return;

	Vec3	forward(1,0,0);
	Vec3  vEntityPos = pEntity->GetWorldPos();
	bool isCharacterVisible = pCharacter->IsCharacterVisible() != 0;
	EStance playerStance = m_pPlayer->GetStance();

	const SActorStats * pActorStats = m_pPlayer->GetActorStats();

	if (m_pPlayer->IsPlayer())
	{
		// PLAYER CHARACTER
		Vec3 vNewEyePosition, vNewEyeDirection;

		if (m_pPlayer->IsClient() && !m_pPlayer->IsThirdPerson() && !gEnv->IsDedicated())
		{
			//Beni - New FP aiming
			vNewEyePosition = GetISystem()->GetViewCamera().GetPosition();
		}
		else
		{
			Vec3 viewOfs(m_pPlayer->GetStanceViewOffset(playerStance));
			vNewEyePosition = vEntityPos + m_pPlayer->GetBaseQuat() * viewOfs;
		}

		if (!m_pPlayer->IsClient()) // marcio: fixes the eye direction for remote players
			vNewEyeDirection = (m_lookTarget - vNewEyePosition).GetNormalizedSafe(state.eyeDirection);
		else
			vNewEyeDirection = m_pPlayer->GetViewQuatFinal().GetColumn1();

		Vec3 bodyDir = vNewEyeDirection;
		bodyDir.z = 0.0f;
		bodyDir.Normalize();

		state.entityDirection = bodyDir;
		state.animationBodyDirection = bodyDir;
		state.animationEyeDirection = vNewEyeDirection;
		state.weaponPosition = vNewEyePosition;
		state.fireDirection = state.aimDirection = vNewEyeDirection;
		state.eyeDirection = vNewEyeDirection;
		state.fireTarget = m_fireTarget;
		state.eyePosition = vNewEyePosition;

		if (pActorStats && pActorStats->mountedWeaponID)
		{
			state.isAiming = true;
		}
		else if (isCharacterVisible)
		{
			state.isAiming=false;
			IAnimationPoseBlenderDir* pIPoseBlenderAimShadow = pCharacter->GetISkeletonPose()->GetIPoseBlenderAim();
			if (pIPoseBlenderAimShadow)
			{
				state.isAiming = pIPoseBlenderAimShadow->GetBlend() > 0.99f;
			}
		}
		else
		{
			state.isAiming = true;
		}

		state.movementDirection = pEntity->GetRotation().GetColumn1();

		const bool isMoving = m_pPlayer->IsMoving();
		if (isMoving)
			state.movementDirection = m_pPlayer->GetLastRequestedVelocity().GetNormalized();	

		state.isMoving = isMoving;
	}
	else
	{
		// AI CHARACTER
		const IAnimatedCharacter* animatedCharacter = m_pPlayer->GetAnimatedCharacter();

		Quat	orientation = pEntity->GetWorldRotation();
		forward = orientation.GetColumn1();
		Vec3	entityPos = vEntityPos;
		assert( entityPos.IsValid() );
		Vec3	constrainedLookDir = m_lookTarget - entityPos;
		Vec3	constrainedAimDir = m_aimTarget - entityPos;
		constrainedLookDir.z = 0.0f;
		constrainedAimDir.z = 0.0f;

		constrainedAimDir = constrainedAimDir.GetNormalizedSafe(constrainedLookDir.GetNormalizedSafe(forward));
		constrainedLookDir = constrainedLookDir.GetNormalizedSafe(forward);

		Matrix33	lookRot;
		lookRot.SetRotationVDir(constrainedLookDir);
		state.eyePosition = entityPos + lookRot.TransformVector(m_pPlayer->GetEyeOffset());

		Matrix33	aimRot;
		aimRot.SetRotationVDir(constrainedAimDir);
		state.weaponPosition = entityPos + aimRot.TransformVector(m_pPlayer->GetWeaponOffset());
			
		state.upDirection = orientation.GetColumn2();

		state.eyeDirection = (m_lookTarget - state.eyePosition).GetNormalizedSafe(forward); //(lEyePos - posHead).GetNormalizedSafe(FORWARD_DIRECTION);
		state.aimDirection = (m_aimTarget - state.weaponPosition).GetNormalizedSafe((m_lookTarget - state.weaponPosition).GetNormalizedSafe(forward)); //pEntity->GetRotation() * dirWeapon;
		state.fireTarget = m_fireTarget;
		state.fireDirection = (state.fireTarget - state.weaponPosition).GetNormalizedSafe(forward);
		state.entityDirection = pEntity->GetWorldRotation().GetColumn1();
		
		if (animatedCharacter)
		{
			state.animationBodyDirection = animatedCharacter->GetAnimLocation().q.GetColumn1();
			state.eyeDirection = state.animationBodyDirection;
			state.animationEyeDirection.zero();

			//Try to retrieve eye direction, or failing that head direction
			if( ISkeletonPose* pSkelPose = pCharacter->GetISkeletonPose())
			{
				if(m_pPlayer->HasBoneID(BONE_HEAD))
				{
					state.animationEyeDirection = (pEntity->GetWorldRotation() * m_pPlayer->GetBoneTransform(BONE_HEAD).q).GetColumn1();
				}
			}
		}
		else
		{
			state.animationBodyDirection = state.entityDirection;
			//No animated eye direction for player's without animated skeletons
			state.animationEyeDirection = ZERO;
		}

		//changed by ivo: most likely this doesn't work any more
		//state.entityDirection = pEntity->GetRotation() * pSkeleton->GetCurrentBodyDirection();
		// [kirill] when AI at MG need to update body/movementDirection coz entity is not moved/rotated AND set AIPos/weaponPs to weapon
		if (pActorStats && pActorStats->mountedWeaponID)
		{		
			IEntity *pWeaponEntity = gEnv->pEntitySystem->GetEntity(pActorStats->mountedWeaponID);
			if(pWeaponEntity)
			{
				state.eyePosition.x = pWeaponEntity->GetWorldPos().x;
				state.eyePosition.y = pWeaponEntity->GetWorldPos().y;
				state.weaponPosition = state.eyePosition;
				EntityId currentWeaponId = 0;
				IAIObject* pAIObject = m_pPlayer->GetEntity()->GetAI();
				IAIActorProxy* pAIProxy = pAIObject ? pAIObject->GetProxy() : NULL;
				IWeapon* pMGWeapon = pAIProxy ? pAIProxy->GetCurrentWeapon(currentWeaponId) : NULL;
				if(pMGWeapon)
				{
					state.weaponPosition = pMGWeapon->GetFiringPos( m_fireTarget );
				}
				// need to recalculate aimDirection, since weaponPos is changed
				state.aimDirection = (m_aimTarget - state.weaponPosition).GetNormalizedSafe((m_lookTarget - state.weaponPosition).GetNormalizedSafe(forward)); //pEntity->GetRotation() * dirWeapon;
			}

			state.isAiming = !m_aimClamped;
			state.entityDirection = state.aimDirection;
			state.movementDirection = state.aimDirection;
		}
		else
		{
			state.entityDirection = pEntity->GetWorldRotation().GetColumn1();

			if (isCharacterVisible)
			{
				IAnimationPoseBlenderDir *pIPoseBlenderAim = pCharacter->GetISkeletonPose()->GetIPoseBlenderAim();
				if (pIPoseBlenderAim)
					state.isAiming = !m_aimClamped && (pIPoseBlenderAim->GetBlend() > 0.99f);
				else
					state.isAiming = true;
			}
			else
				state.isAiming = true;

			// This should replace the above in a future check-in.
			// It's logically different but is more robust.
			//
			//IAnimationPoseBlenderDir* pIPoseBlenderAim = pCharacter->GetISkeletonPose()->GetIPoseBlenderAim();
			//const bool isAimPosePresent = (pIPoseBlenderAim != NULL);
			//const bool aimPoseShouldBeConsidered = (isCharacterVisible && isAimPosePresent);
			//if (aimPoseShouldBeConsidered)
			//{
			//	const bool aimPoseIsBlendedIn = (pIPoseBlenderAim->GetBlend() > 0.99f);
			//	state.isAiming = !m_aimClamped && aimPoseIsBlendedIn;
			//}
			//else
			//{
			//	state.isAiming = !m_aimClamped;
			//}
		}

		if (animatedCharacter)
		{
			state.movementDirection = pEntity->GetRotation().GetColumn1();

			const float speed = animatedCharacter->GetEntitySpeedHorizontal();
			const bool isMoving = (speed >= 0.01f);
			if (isMoving)
				state.movementDirection = animatedCharacter->GetEntityMovementDirHorizontal();

			state.isMoving = isMoving;
		}
		else
		{
			state.movementDirection = pEntity->GetRotation().GetColumn1();

			const bool isMoving = m_pPlayer->IsMoving();
			if (isMoving)
				state.movementDirection = m_pPlayer->GetLastRequestedVelocity().GetNormalized();	

			state.isMoving = isMoving;
		}
	}


	if (pActorStats)
	{
		state.lean = m_pPlayer->m_pPlayerRotation->GetLeanAmount();
		state.isFiring = (pActorStats->inFiring >= 10.f);
	}
	else
	{
		state.lean = 0.0f;
		state.isFiring = false;
	}

	state.atMoveTarget = m_atTarget;
	state.desiredSpeed = m_desiredSpeed;
	state.stance = playerStance;
	state.upDirection = pEntity->GetWorldRotation().GetColumn2();

	state.minSpeed = -1.0f;
	state.maxSpeed = -1.0f;
	state.normalSpeed = -1.0f;

	state.m_StanceSize = m_pPlayer->GetStanceInfo(playerStance)->GetStanceBounds();

	state.pos = vEntityPos;

	state.isAlive = !m_pPlayer->IsDead();

	IVehicle *pVehicle = m_pPlayer->GetLinkedVehicle();
	if (pVehicle)
	{
		IMovementController *pVehicleMovementController = pVehicle->GetPassengerMovementController(m_pPlayer->GetEntityId());
		if (pVehicleMovementController)
			pVehicleMovementController->GetMovementState(state);
	}	

	const IAnimatedCharacter* pAC = m_pPlayer->GetAnimatedCharacter();
	state.slopeAngle = (pAC != NULL) ? pAC->GetSlopeDegreeMoveDir() : 0.0f;
}

bool CPlayerMovementController::GetStanceState( const SStanceStateQuery& query, SStanceState& state )
{
	IEntity * pEntity = m_pPlayer->GetEntity();
	const SStanceInfo*	stanceInfo = m_pPlayer->GetStanceInfo(query.stance);
	if(!stanceInfo)
		return false;

	Quat orientation = pEntity->GetWorldRotation();
	Vec3 forward = orientation.GetColumn1();
	Vec3 pos = query.position.IsZero() ? pEntity->GetWorldPos() : query.position;

	Vec3 aimTarget = m_aimTarget;
	Vec3 lookTarget = m_lookTarget;

	if (!query.target.IsZero())
	{
		aimTarget = query.target;
		lookTarget = query.target;
		forward = (query.target - pos).GetNormalizedSafe(forward);
	}

	state.pos = pos;
	state.m_StanceSize = stanceInfo->GetStanceBounds();
	state.m_ColliderSize = stanceInfo->GetColliderBounds();
	state.lean = query.lean;	// pass through
	state.peekOver = query.peekOver;

	if(query.defaultPose)
	{
		state.eyePosition = pos + stanceInfo->GetViewOffsetWithLean(query.lean, query.peekOver);
		state.weaponPosition = pos + m_pPlayer->GetWeaponOffsetWithLean(query.stance, query.lean, query.peekOver, m_pPlayer->GetEyeOffset());
		state.upDirection.Set(0,0,1);
		state.eyeDirection = FORWARD_DIRECTION;
		state.aimDirection = FORWARD_DIRECTION;
		state.fireDirection = FORWARD_DIRECTION;
		state.entityDirection = FORWARD_DIRECTION;
	}
	else
	{
		Vec3 constrainedLookDir = lookTarget - pos;
		Vec3 constrainedAimDir = aimTarget - pos;
		constrainedLookDir.z = 0.0f;
		constrainedAimDir.z = 0.0f;

		constrainedAimDir = constrainedAimDir.GetNormalizedSafe(constrainedLookDir.GetNormalizedSafe(forward));
		constrainedLookDir = constrainedLookDir.GetNormalizedSafe(forward);

		Matrix33 lookRot;
		lookRot.SetRotationVDir(constrainedLookDir);
		state.eyePosition = pos + lookRot.TransformVector(stanceInfo->GetViewOffsetWithLean(query.lean, query.peekOver));

		Matrix33 aimRot;
		aimRot.SetRotationVDir(constrainedAimDir);
		state.weaponPosition = pos + aimRot.TransformVector(m_pPlayer->GetWeaponOffsetWithLean(query.stance, query.lean, query.peekOver, m_pPlayer->GetEyeOffset()));

		state.upDirection = orientation.GetColumn2();

		state.eyeDirection = (lookTarget - state.eyePosition).GetNormalizedSafe(forward);
		state.aimDirection = (aimTarget - state.weaponPosition).GetNormalizedSafe((lookTarget - state.weaponPosition).GetNormalizedSafe(forward));
		state.fireDirection = state.aimDirection;
		state.entityDirection = forward;
	}

	return true;
}

void CPlayerMovementController::Release()
{
	delete this;
}

void CPlayerMovementController::Serialize(TSerialize &ser)
{
	if(ser.GetSerializationTarget() != eST_Network)	//basic serialization
	{
		ser.Value("DesiredSpeed", m_desiredSpeed);
		ser.Value("atTarget", m_atTarget);
		ser.Value("m_usingLookIK", m_usingLookIK);
		ser.Value("m_usingAimIK", m_usingAimIK);
		ser.Value("m_aimClamped", m_aimClamped);
		ser.Value("m_lookTarget", m_lookTarget);
		ser.Value("m_aimTarget", m_aimTarget);
		ser.Value("m_animTargetSpeed", m_animTargetSpeed);
		ser.Value("m_animTargetSpeedCounter", m_animTargetSpeedCounter);
		ser.Value("m_fireTarget", m_fireTarget);
		ser.EnumValue("targetStance", m_targetStance, STANCE_NULL, STANCE_LAST);

		SMovementState m_currentMovementState;

		ser.BeginGroup("m_currentMovementState");

		ser.Value("entityDirection", m_currentMovementState.entityDirection);
		ser.Value("aimDir", m_currentMovementState.aimDirection);
		ser.Value("fireDir", m_currentMovementState.fireDirection);
		ser.Value("fireTarget", m_currentMovementState.fireTarget);
		ser.Value("weaponPos", m_currentMovementState.weaponPosition);
		ser.Value("desiredSpeed", m_currentMovementState.desiredSpeed);
		ser.Value("moveDir", m_currentMovementState.movementDirection);
		ser.Value("upDir", m_currentMovementState.upDirection);
		ser.EnumValue("stance", m_currentMovementState.stance, STANCE_NULL, STANCE_LAST);
		ser.Value("Pos", m_currentMovementState.pos);
		ser.Value("eyePos", m_currentMovementState.eyePosition);
		ser.Value("eyeDir", m_currentMovementState.eyeDirection);
		ser.Value("animationEyeDirection", m_currentMovementState.animationEyeDirection);
		ser.Value("minSpeed", m_currentMovementState.minSpeed);
		ser.Value("normalSpeed", m_currentMovementState.normalSpeed);
		ser.Value("maxSpeed", m_currentMovementState.maxSpeed);
		ser.Value("stanceSize.Min", m_currentMovementState.m_StanceSize.min);
		ser.Value("stanceSize.Max", m_currentMovementState.m_StanceSize.max);
		ser.Value("colliderSize.Min", m_currentMovementState.m_ColliderSize.min);
		ser.Value("colliderSize.Max", m_currentMovementState.m_ColliderSize.max);
		ser.Value("atMoveTarget", m_currentMovementState.atMoveTarget);
		ser.Value("isAlive", m_currentMovementState.isAlive);
		ser.Value("isAiming", m_currentMovementState.isAiming);
		ser.Value("isFiring", m_currentMovementState.isFiring);
		ser.Value("isVis", m_currentMovementState.isVisible);

		ser.EndGroup();

		if(ser.IsReading())
			UpdateMovementState(m_currentMovementState);
	}
}

void CPlayerMovementController::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this));
}


bool CPlayerMovementController::HandleEvent( const SGameObjectEvent& event )
{
	if (m_movementTransitionsController.IsEnabled())
		return m_movementTransitionsController.HandleEvent(event);
	else
		return false;
}

// ----------------------------------------------------------------------------
const SExactPositioningTarget* CPlayerMovementController::GetExactPositioningTarget()
{
	if (!m_pExactPositioning.get())
		return NULL;

	return m_pExactPositioning->GetExactPositioningTarget();
}

// ----------------------------------------------------------------------------
void CPlayerMovementController::SetExactPositioningListener(IExactPositioningListener* pExactPositioningListener)
{
	CRY_ASSERT(m_pPlayer);
	if (!m_pPlayer)
		return;

	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	CRY_ASSERT(pAnimatedCharacter);
	if (!pAnimatedCharacter)
		return;

	if (pExactPositioningListener && !m_pExactPositioning.get()) // only create ExactPositioning when passing in a non-NULL ptr
		m_pExactPositioning.reset(new CExactPositioning(pAnimatedCharacter));

	if (m_pExactPositioning.get())
		m_pExactPositioning->SetExactPositioningListener(pExactPositioningListener);
}
