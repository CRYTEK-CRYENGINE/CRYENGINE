// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Player.h"

#include "MovementAction.h"
//#include "Utility/CryWatch.h"

#include "GameConstantCVars.h"

//--- TODO! Move these somewhere more sensible!

class CProceduralClipSwapHand : public TProceduralClip<SNoProceduralParams>
{
public:
		virtual void OnEnter(float blendTime, float duration, const SNoProceduralParams &params)
	{
		IAttachment *oldAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByName("weapon");
		IAttachment *newAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByName("left_weapon");

		if (oldAttachment && newAttachment)
		{
			IAttachmentObject *attachObject = oldAttachment->GetIAttachmentObject();
			if (attachObject)
			{
				ICharacterInstance *charInst = attachObject->GetICharacterInstance();
				if (charInst)
				{
					oldAttachment->ClearBinding();

					CSKELAttachment *pChrAttachment = new CSKELAttachment();
					pChrAttachment->m_pCharInstance = charInst;
					newAttachment->AddBinding(pChrAttachment);
				}
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		IAttachment *oldAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByName("left_weapon");
		IAttachment *newAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByName("weapon");

		if (oldAttachment && newAttachment)
		{
			IAttachmentObject *attachObject = oldAttachment->GetIAttachmentObject();
			if (attachObject)
			{
				oldAttachment->ClearBinding();

				ICharacterInstance *charInst = m_scope->GetActionController().GetScope(PlayerMannequin.scopeIDs.Weapon)->GetCharInst();
				if (charInst)
				{
					CSKELAttachment *pChrAttachment = new CSKELAttachment();
					pChrAttachment->m_pCharInstance = charInst;
					newAttachment->AddBinding(pChrAttachment);
				}
			}
		}
	}

	virtual void Update(float timePassed)
	{
	}
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipSwapHand, "SwapHand");



CPlayerBackgroundAction::CPlayerBackgroundAction(int priority, FragmentID fragID)
	:
	TPlayerAction(priority, fragID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut|IAction::Interruptable)
{
}

IAction::EStatus CPlayerBackgroundAction::Update(float timePassed)
{
	if (GetRootScope().IsDifferent(m_fragmentID, m_fragTags))
	{
		SetFragment(m_fragmentID, m_fragTags);
	}

	return TPlayerAction::Update(timePassed);
}

CPlayerMovementAction::CPlayerMovementAction(int priority)
	: TPlayerAction(priority, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut|IAction::Interruptable),
	m_lastAimDir(ZERO),
	m_moveState(Stand),
	m_installedMoveState(Total),
	m_lastTurnDirection(0.0f),
	m_lastTravelAngle(0.0f),
	m_lastMoveSpeed(0.0f),
	m_travelAngleSmoothRateQTX(0.0f),
	m_moveSpeedSmoothRateQTX(0.0f),
	m_FPTurnSpeed(0.0f),
	m_FPTurnSpeedSmoothRateQTX(0.0f),
	m_spinning(false),
	m_smoothMovement(false)
{
	memset (m_AAID, 0, sizeof (m_AAID));
}

void CPlayerMovementAction::OnInitialise()
{
	m_AAID[Stand] = PlayerMannequin.fragmentIDs.MotionIdle;
	m_AAID[Turn]	= PlayerMannequin.fragmentIDs.MotionTurn;
	m_AAID[Move]	= PlayerMannequin.fragmentIDs.MotionMovement;
	m_AAID[InAir]	= PlayerMannequin.fragmentIDs.MotionInAir;

	m_moveState		= Stand;
	SetFragment(m_AAID[m_moveState], m_fragTags);
}

void CPlayerMovementAction::Enter()
{
	TPlayerAction::Enter();

	//--- Reset movement params
	m_lastAimDir.zero();
	m_lastTurnDirection				 = 0.0f;
	m_lastTravelAngle					 = 0.0f;
	m_lastMoveSpeed						 = 0.0f;
	m_moveSpeedSmoothRateQTX   = 0.0f;
	m_travelAngleSmoothRateQTX = 0.0f;
	m_smoothMovement					 = false;
	m_spinning								 = false;
	m_installedMoveState			 = Total;

	SetState(m_moveState);
}

void CPlayerMovementAction::Exit() 
{
	const EntityId rootEntityId = GetRootScope().GetEntityId();
	CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));
	IAnimatedCharacter *animatedCharacter = player.GetAnimatedCharacter();
	if (animatedCharacter)
	{
		animatedCharacter->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
	}
	player.SetCanTurnBody(true);

	TPlayerAction::Exit();
}

const float MAX_AIM_ANGLE	  = gf_PI*0.9f;

const bool SPIN_WITH_ENTITY					= false;
const float TRAVEL_ANGLE_BLEND_RATE	= 0.1f;
const float MOVE_SPEED_BLEND_RATE		= 0.04f;
const float SPIN_FACTOR							= 0.1f;

const float TURNBIAS_MIN_SPEED = 1.0f;
const float TURNBIAS_MAX_SPEED = 1.5f;
const float TURNBIAS_MIN_ANGLE = gf_PI/4.0f;
const float TURNBIAS_MAX_ANGLE = gf_PI;

void CPlayerMovementAction::SetState(EMoveState newMoveState)
{
	m_moveState		= newMoveState;
}

IAction::EStatus CPlayerMovementAction::UpdatePending(float timePassed)
{
	TPlayerAction::UpdatePending(timePassed);

	m_moveState  = CalculateState();
	SetFragment(m_AAID[m_moveState]);

	return m_eStatus;
}

void CPlayerMovementAction::OnFragmentStarted()
{
	if (m_installedMoveState != m_moveState)
	{
		EntityId rootEntityId = GetRootScope().GetActionController().GetEntityId();
		CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));
		IAnimatedCharacter *animatedCharacter = player.GetAnimatedCharacter();
		m_installedMoveState = m_moveState;

		if (player.IsThirdPerson())
		{
			switch (m_installedMoveState)
			{
			case Turn:
				player.SetAimLimit(m_lastTurnDirection * MAX_AIM_ANGLE);
				player.SetCanTurnBody(false);
				if (m_spinning && SPIN_WITH_ENTITY)
				{
					animatedCharacter->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
				}
				else
				{
					animatedCharacter->SetMovementControlMethods(eMCM_AnimationHCollision, eMCM_Entity);
				}
				SetSpeedBias(TURNBIAS_MIN_SPEED);
				break;
			case Stand:
				m_spinning		= false;
				player.SetCanTurnBody(false);
				player.ClearAimLimit();
				animatedCharacter->SetMovementControlMethods(eMCM_AnimationHCollision, eMCM_Entity);
				SetSpeedBias(1.0f);
				break;
			case Move:
			case InAir:
				m_spinning		= false;
				player.SetCanTurnBody(true);
				player.ClearAimLimit();
				animatedCharacter->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
				SetSpeedBias(1.0f);
				break;
			}
		}
	}
}

 ILINE void SmoothAngleCD(
	float &val,                 ///< in/out: value to be smoothed
	float &valRate,             ///< in/out: rate of change of the value
	const float timeDelta,			///< in: time interval
	const float &to,            ///< in: the target value
	const float smoothTime)			///< in: timescale for smoothing
{
	float change = AngleWrap_PI(val - to);

	if (smoothTime > 0.0f)
	{
		const float omega = 2.0f / smoothTime;
		const float x = omega * timeDelta;
		const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

		const float temp = ((valRate + change * omega) * timeDelta);
		valRate = ((valRate - temp * omega) * exp);
		val = (to + (change + temp) * exp);

		val = AngleWrap_PI(val);
	}
	else if (timeDelta > 0.0f)
	{
		valRate = (change / timeDelta);
		val = to;
	}
	else
	{
		val = to;
		valRate -= valRate; // zero it...
	}
}

IAction::EStatus CPlayerMovementAction::Update(float timePassed)
{
	const EntityId rootEntityId = GetRootScope().GetEntityId();
	CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));

	if (!player.IsThirdPerson())
	{
		const float fpStrafeFactor = -0.3f;
		float horizontalSpeed	= player.m_weaponParams.inputRot.z * fpStrafeFactor; 
		SmoothCD(m_FPTurnSpeed,		m_FPTurnSpeedSmoothRateQTX,		timePassed, horizontalSpeed,	 MOVE_SPEED_BLEND_RATE);
	}

	float turnAngle, travelAngle, moveSpeed;
	EStatus ret = TPlayerAction::Update(timePassed);
	EMoveState newMoveState = CalculateState(&turnAngle, &travelAngle, &moveSpeed);

	bool changedState = (newMoveState != m_moveState);
	if (changedState)
	{
		SetState(newMoveState);
	}

	if ((newMoveState == Turn) && (m_installedMoveState == Turn))
	{
		//--- If the turn anim is already half-way through then allow a retrigger
		changedState = changedState || (GetRootScope().GetFragmentTime() > (GetRootScope().GetFragmentDuration() * 0.5f));
	}

	assert(newMoveState < Total);
	if (changedState || IsDifferent(m_AAID[newMoveState], m_fragTags))
	{
		m_moveState = newMoveState;
		SetFragment(m_AAID[m_moveState], m_fragTags, OPTION_IDX_RANDOM, 0, false);
	}

	//--- Hold last movement params to ensure that movement VEGs blend out without resetting direction
	if (newMoveState == Move)
	{
		if (m_smoothMovement)
		{
			SmoothAngleCD(m_lastTravelAngle, m_travelAngleSmoothRateQTX, timePassed, travelAngle, TRAVEL_ANGLE_BLEND_RATE);
			SmoothCD(m_lastMoveSpeed,		m_moveSpeedSmoothRateQTX,		timePassed, moveSpeed,	 MOVE_SPEED_BLEND_RATE);
		}
		else
		{
			m_lastTravelAngle						= travelAngle;
			m_lastMoveSpeed							= moveSpeed;
			m_travelAngleSmoothRateQTX	= 0.0f;
			m_moveSpeedSmoothRateQTX		= 0.0f;
		}

		m_smoothMovement  = true;
	}
	else
	{
		m_smoothMovement  = false;
	}

	//--- Send our motion params off for VEG processing
	CMovementRequest moveReq;
	SPredictedCharacterStates prediction;

	prediction.SetParam(eMotionParamID_TurnAngle, turnAngle);
	prediction.SetParam(eMotionParamID_TravelAngle, m_lastTravelAngle);
	prediction.SetParam(eMotionParamID_TravelSpeed, m_lastMoveSpeed);
	moveReq.SetPrediction(prediction);
	if (m_spinning)
	{
		QuatT adjustment;
		adjustment.t.zero();
		adjustment.q.SetRotationZ(turnAngle * SPIN_FACTOR);
		player.GetAnimatedCharacter()->ForceMovement(adjustment);
	}

	player.GetMovementController()->RequestMovement(moveReq);

	return ret;
}

CPlayerMovementAction::EMoveState CPlayerMovementAction::CalculateState(float *pTurnAngle, float *pTravelAngle, float *pMoveSpeed)
{
	const EntityId rootEntityId = GetRootScope().GetEntityId();
	const CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));

	EMoveState ret				= Stand;
	float retTurnAngle		= 0.0f;
	float retTravelAngle	= 0.0f;
	float retMoveSpeed		= 0.0f;

	const bool notInAir = player.IsOnGround() || player.IsSwimming() || player.IsOnLedge() || player.IsSliding();
	if (notInAir)
	{
		if (player.IsMoving())
		{
			m_lastTurnDirection = 0.0f;

			Vec3 moveDir = player.GetLastRequestedVelocity();

			if (moveDir.GetLengthSquared2D() > FLT_EPSILON)
			{
				moveDir.z = 0.0f;
				float moveSpeed = moveDir.GetLength2D();
				moveDir = moveDir /= moveSpeed;

				const Quat &animLoc			= player.GetAnimatedCharacter()->GetAnimLocation().q;
				const float signedAngle	= Ang3::CreateRadZ(animLoc.GetColumn1(), moveDir);
				retTravelAngle = signedAngle;
				retMoveSpeed = moveSpeed;

				ret = Move;
			}

			m_lastAimDir.zero();
		}
		else
		{
			if (!player.IsThirdPerson())
			{
				const float MIN_STRAFE_SPEED = 0.15f;
				const float MIN_STRAFE_SPEED_MOVING = 0.05f;
				const float strafeSpeed = (m_moveState == Move) ? MIN_STRAFE_SPEED_MOVING : MIN_STRAFE_SPEED;
				const float absHorizontalSpeed = fabs_tpl(m_FPTurnSpeed);

				if (absHorizontalSpeed > strafeSpeed)
				{
					const float halfPI = gf_PI * 0.5f;
					retTravelAngle = (float)__fsel(m_FPTurnSpeed, halfPI, -halfPI);
					retMoveSpeed = absHorizontalSpeed;
					ret = Move;
				}
			}
			else
			{
				const Quat &animLoc  = player.GetAnimatedCharacter()->GetAnimLocation().q;
				const Quat &viewQuat = player.GetViewQuat();
				const Vec3 viewDir = viewQuat.GetColumn1();

				const float TARGET_MOVING_DP			= 0.9f;
				float lastDP = m_lastAimDir.dot(viewDir);
				bool targetMoving = (lastDP < TARGET_MOVING_DP);
				m_lastAimDir = viewDir;

				const float signedAngle		= Ang3::CreateRadZ(animLoc.GetColumn1(), viewDir);
				const float unsignedAngle = fabsf(signedAngle);
				const float TRIGGER_TURN_ANGLE		= DEG2RAD(35.0f);
				const float MIN_TURN_ANGLE				= DEG2RAD(65.0f);
				const float MAX_TURN_ANGLE				= DEG2RAD(130.0f);
				const float MIN_SPIN_ANGLE				= DEG2RAD(20.0f);

				if (unsignedAngle > 0.0f)
				{
					const bool doTurn = (unsignedAngle > TRIGGER_TURN_ANGLE);

					float unsignedAngleAdjusted = clamp_tpl(unsignedAngle, MIN_TURN_ANGLE, MAX_TURN_ANGLE);
					float signedAngleAdjusted		= (signedAngle * unsignedAngleAdjusted) / unsignedAngle;

					float turnDir = fsgnnz(signedAngle);

					if (m_spinning)
					{
						m_spinning = (unsignedAngle > MIN_SPIN_ANGLE) || targetMoving;
					}
					else
					{
						m_spinning = (m_installedMoveState == Turn) && (turnDir != m_lastTurnDirection) && doTurn && (m_lastTurnDirection != 0.0f);
					}

					if (m_spinning)
					{
						retTurnAngle = (float)__fsel(unsignedAngle - MIN_SPIN_ANGLE, unsignedAngle*m_lastTurnDirection, 0.0f);
					}
					else
					{
						m_lastTurnDirection = turnDir;
						retTurnAngle = signedAngleAdjusted;
					}

					if (doTurn)
					{
						ret = Turn;

						if (m_installedMoveState == Turn)
						{
							float turnDeltaT = (unsignedAngle - TURNBIAS_MIN_ANGLE) / (TURNBIAS_MAX_ANGLE - TURNBIAS_MIN_ANGLE);
							turnDeltaT = clamp_tpl(turnDeltaT, 0.0f, 1.0f);
							float playSpeedBias = TURNBIAS_MIN_SPEED + (turnDeltaT * (TURNBIAS_MAX_SPEED - TURNBIAS_MIN_SPEED));
							SetSpeedBias(playSpeedBias);
						}
					}
				}

//				const float aimAngle = Ang3::CreateRadZ(Vec3Constants<float>::fVec3_OneY, viewDir);
//				CryWatch("Turn Angle %.4f %.4f %.1f %s", aimAngle, signedAngle, m_lastTurnDirection, m_spinning ? "SPINNING" : "");
			}
		}
	}
	else
	{
		m_lastTurnDirection = 0.0f;

		ret = InAir;
	}

	if (pTurnAngle)
	{
		*pTurnAngle = retTurnAngle;
	}
	if (pTravelAngle)
	{
		*pTravelAngle = retTravelAngle;
	}
	if (pMoveSpeed)
	{
		*pMoveSpeed = retMoveSpeed;
	}

	return ret;
}