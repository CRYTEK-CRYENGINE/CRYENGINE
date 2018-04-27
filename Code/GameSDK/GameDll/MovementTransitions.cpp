// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
Manages the special transitions for one specific entity class
-------------------------------------------------------------------------
History:
- 4:29:2010	19:48 : Created by Sven Van Soom
*************************************************************************/
#include "StdAfx.h"
#include "Player.h" // for CPlayer, IMovementController, SActorFrameMovementParams, ...
#include "MovementTransitionsUtil.h"
#include "MovementTransitionsController.h"
#include "MovementTransitions.h"

#include <CryAISystem/IAIActor.h> // for IAIObject, IAIActor, IAIPathAgent

#include <CryGame/GameUtils.h> // for AngleDifference
#include <CryCore/TypeInfo_impl.h> // for CRY_ARRAY_COUNT
#include "GameCVars.h"
#include <CryAISystem/IAIDebugRenderer.h>

#define MovementTransitionsLog if (false) CryLog
//#define MovementTransitionsLog CryLog


namespace MovementTransitionsUtil
{
	struct SPseudoSpeed 
	{ 
		const char* name;
		float value; 
	};
	
	SPseudoSpeed s_pseudoSpeeds[] = { { "Idle", AISPEED_ZERO } , { "Slow", AISPEED_SLOW }, { "Walk", AISPEED_WALK }, { "Run", AISPEED_RUN }, { "Sprint", AISPEED_SPRINT } };
	const int NUM_PSEUDOSPEEDS = CRY_ARRAY_COUNT(s_pseudoSpeeds);

	struct STransitionTypeDesc
	{
		const char* sTagName;
		ETransitionType transitionType;
	};
	
	STransitionTypeDesc s_transitionTypes[] = { { "Start", eTT_Start }, { "Stop", eTT_Stop }, { "DirectionChange", eTT_DirectionChange } };
	const int NUM_TRANSITIONTYPES = CRY_ARRAY_COUNT(s_transitionTypes);
}

#ifndef _RELEASE
namespace MovementTransitionsDebug
{
	static int s_debug_y = 50;
	static int s_debug_frame = -1;
	static float s_dbg_color_unsignaled[4] = {0.9f,0.7f,0.5f,1};
	static float s_dbg_color_signaled[4] = {0.9f,1.0f,0.5f,1};

	//////////////////////////////////////////////////////////////////////////
	static const char* GetTransitionTypeName(ETransitionType transitionType)
	{
		for(int iTransitionType = 0; iTransitionType < MovementTransitionsUtil::NUM_TRANSITIONTYPES; ++iTransitionType)
		{
			if (transitionType == MovementTransitionsUtil::s_transitionTypes[iTransitionType].transitionType)
				return MovementTransitionsUtil::s_transitionTypes[iTransitionType].sTagName;
		}

		if (transitionType == -1)
			return "<none>";

		CRY_ASSERT(false);
		return "<unknown>";
	}


	//////////////////////////////////////////////////////////////////////////
	static const char* GetPseudoSpeedName(float pseudoSpeed)
	{
		for(int iPseudoSpeed = 0; iPseudoSpeed < MovementTransitionsUtil::NUM_PSEUDOSPEEDS; ++iPseudoSpeed)
		{
			if (pseudoSpeed == MovementTransitionsUtil::s_pseudoSpeeds[iPseudoSpeed].value)
			{
				return MovementTransitionsUtil::s_pseudoSpeeds[iPseudoSpeed].name;
			}
		}

		CRY_ASSERT(false);
		return "<unknown>";
	}
}
#endif // ifndef _RELEASE

//////////////////////////////////////////////////////////////////////////
static ILINE bool IsOnSafeLine(const Lineseg& safeLine, const Vec3& vStartPosition, const Vec3& vDirection, float distance)
{
	const float allowedPositionDeviationSquared = ::sqr(0.1f);

	float t;
	const float startDeviationSquared = Distance::Point_LinesegSq(vStartPosition, safeLine, t);
	if (startDeviationSquared > allowedPositionDeviationSquared)
		return false;

	const Vec3 vEndPosition = vStartPosition + vDirection.GetNormalized() * distance;
	float endDeviationSquared = Distance::Point_LinesegSq(vEndPosition, safeLine, t);
	if (endDeviationSquared > allowedPositionDeviationSquared)
		return false;

	return true;
}



//////////////////////////////////////////////////////////////////////////
EMovementTransitionState STransition::Update(
	const CMovementTransitions& transitions,
	const STransitionSelectionParams& transParams,
	const STransitionMatch& match,
	const Vec3& playerPos,
	const Vec3& oldMoveDirection,
	const Vec3& newMoveDirection,

	float*const pJukeTurnRateFraction,
	Vec3*const pBodyTarget,
	const char**const pBodyTargetType,
	CPlayer*const pPlayer,
	CMovementTransitionsController*const pController ) const
{
	EMovementTransitionState newState;

	if (transParams.m_transitionDistance > maxDistance)
	{
		// Prepare transition by orienting the body towards the desiredTravelAngle
		if (match.angleDifference <= prepareTravelAngleTolerance)
		{
			// Preparation: turn towards closest juke direction (as we didn't LMG-ize those)
			float prepare = prepareDistance - maxDistance;
			*pJukeTurnRateFraction = (prepare > FLT_EPSILON) ? 1.0f - (transParams.m_transitionDistance - maxDistance)/prepare : 0.0f; // starts at 0, builds up to 1

			*pBodyTarget = playerPos + (Quat::CreateRotationZ(-desiredTravelAngle) * newMoveDirection); // move bodytarget to be at -desiredTravelAngle from movedirection
			*pBodyTargetType = "transAdjust";

			newState = eMTS_Preparing;
		}
		else
		{
			newState = eMTS_WaitingForRange;
		}
	}
	else
	{
		if (match.angleDifference <= travelAngleTolerance)
		{
			if (CheckSpaceToPerformTransition(transitions, transParams, oldMoveDirection, playerPos, pPlayer))
			{
				bool bSignaled = pController->RequestTransition(animGraphSignal.c_str(), this, transParams.m_future);
				newState = bSignaled ? eMTS_Requesting_Succeeded : eMTS_Requesting_DelayedBecauseControllerRequestFailed;
			}
			else
			{
				newState = eMTS_Requesting_DelayedBecauseWalkabilityFail;
			}
		}
		else
		{
			newState = eMTS_Requesting_DelayedBecauseAngleOutOfRange;
		}
	}

#ifndef _RELEASE
	{
		const ColorB debugColors[eMTS_COUNT] = 
		{
			ColorB(0, 0, 0), // none - black
			ColorB(0, 0, 255), // considering - blue
			ColorB(0, 175, 175), // preparing - green-blue
			ColorB(0, 255, 0), // requesting succeeded - full green
			ColorB(128, 0, 0), // request fail - dark red
			ColorB(255, 0, 0),  // travel angle fail - red
			ColorB(255, 100, 100)  // walkability fail - pink
		};

		if (g_pGame->GetCVars()->g_movementTransitions_debug)
		{
			if (transitionType == eTT_DirectionChange)
			{
				Vec3 up(0.0f, 0.0f, 0.1f);

				IAIDebugRenderer* pDc = gEnv->pAISystem->GetAIDebugRenderer();
				pDc->PushState();

				ColorB debugColor = debugColors[newState];

				float distanceAfterJuke = 2.0f;

				Vec3 jukePoint = playerPos + oldMoveDirection * transParams.m_transitionDistance;
				Vec3 startPoint = jukePoint - oldMoveDirection * (this->maxDistance + this->minDistance)/2.0f;
				Vec3 desiredEndPoint = jukePoint + Quat::CreateRotationZ(this->desiredJukeAngle) * oldMoveDirection * distanceAfterJuke;
				Vec3 realEndPoint = jukePoint + Quat::CreateRotationZ(transParams.m_jukeAngle) * oldMoveDirection * distanceAfterJuke;

				Vec3 desiredBodyDirection = Quat::CreateRotationZ(-this->desiredTravelAngle) * oldMoveDirection;
				Vec3 currentBodyDirection = Quat::CreateRotationZ(-transParams.m_travelAngle) * oldMoveDirection;

				pDc->DrawSphere(jukePoint + 8.00f*up, 0.1f, debugColor);
				pDc->DrawArrow(startPoint + 8.00f*up, (jukePoint - startPoint), 0.1f, debugColor);
				pDc->DrawArrow(jukePoint + 8.00f*up, (desiredEndPoint - jukePoint), 0.1f, debugColor);
				pDc->DrawArrow(jukePoint + 8.05f*up, (realEndPoint - jukePoint), 0.1f, debugColor);
				pDc->DrawRangeArc(jukePoint+ 8.00f*up, (desiredEndPoint - jukePoint).GetNormalized(), 2.0f*jukeAngleTolerance, distanceAfterJuke, 0.1f, debugColor, debugColor, false);

				float debugTravelAngleDistance = 0.8f*(this->maxDistance+this->minDistance)/2;
				pDc->DrawArrow(playerPos + 8.05f*up, desiredBodyDirection*debugTravelAngleDistance , 0.1f, debugColor);
				pDc->DrawRangeArc(startPoint + 8.00f*up, desiredBodyDirection, 2.0f*travelAngleTolerance, debugTravelAngleDistance, 0.1f, debugColor, debugColor, false);

				pDc->PopState();
			} // eTT_DirectionChange
		} // if debugging on
	}
#endif // RELEASE

	return newState;
}

//////////////////////////////////////////////////////////////////////////
bool STransition::ReadParams( ETransitionType _transitionType, const struct IItemParamsNode*const pParams )
{
	transitionType = _transitionType;

	animGraphSignal = pParams->GetAttribute("animGraphSignal");
	CRY_ASSERT_TRACE(animGraphSignal.length() > 0, ("animGraphSignal not found or empty"));
	if (!(animGraphSignal.length() > 0)) return false;

	pseudoSpeed = 0.0f;
	//if (!ReadFloatParam("pseudoSpeed", true, AISPEED_SLOW, AISPEED_SPRINT, pParams, &pseudoSpeed)) return false;
	if (!ReadPseudoSpeedParam("pseudoSpeed", true, pParams, &pseudoSpeed)) return false;

	stance = STANCE_NULL;
	if (!ReadStanceParam("stance", true, pParams, &stance)) return false;

	targetStance = STANCE_NULL;
	ReadStanceParam("targetStance", false, pParams, &targetStance);

	int iContext = 0; // default = don't care
	ReadIntParam("context", false, 0, INT_MAX, pParams, &iContext);
	context = (unsigned)iContext;

	minDistance = 0.0f;
	if (!ReadFloatParam("minDist", true, 0.0f, FLT_MAX, pParams, &minDistance)) return false;

	desiredTravelAngle = 0.0f;
	if (!ReadAngleParam("travelAngle", true, -gf_PI2, gf_PI2, pParams, &desiredTravelAngle)) return false;

	travelAngleTolerance = FLT_MAX;
	if (!ReadAngleParam("travelAngleTolerance", true, 0.0f, gf_PI2, pParams, &travelAngleTolerance)) return false;

	if (transitionType == eTT_Start)
	{
		maxDistance = FLT_MAX;
		prepareDistance = FLT_MAX;
		prepareTravelAngleTolerance = FLT_MAX;
		desiredArrivalAngle = 0.0f;
		arrivalAngleTolerance = FLT_MAX;
	}
	else
	{
		maxDistance = minDistance;
		if (!ReadFloatParam("maxDist", true, minDistance, FLT_MAX, pParams, &maxDistance)) return false;

		const float maxMovementPerFrame = 7.5f/30.0f; // 7.5m/s at 30fps gives you 25cm per frame
		minDistance -= maxMovementPerFrame/2;
		maxDistance = minDistance + maxMovementPerFrame; 

		prepareDistance = maxDistance;
		if (!ReadFloatParam("prepareDist", true, maxDistance, FLT_MAX, pParams, &prepareDistance)) return false;

		prepareTravelAngleTolerance = FLT_MAX;
		if (!ReadAngleParam("prepareTravelAngleTolerance", true, 0.0f, gf_PI2, pParams, &prepareTravelAngleTolerance)) return false;
	}

	if (transitionType == eTT_Stop)
	{
		desiredArrivalAngle = 0.0f;
		bool hasDesiredArrivalAngle = ReadAngleParam("arrivalAngle", false, -gf_PI2, gf_PI2, pParams, &desiredArrivalAngle);

		arrivalAngleTolerance = FLT_MAX;
		bool hasArrivalAngleTolerance = ReadAngleParam("arrivalAngleTolerance", false, 0.0f, gf_PI2, pParams, &arrivalAngleTolerance);

		CRY_ASSERT_MESSAGE(!hasDesiredArrivalAngle ||  hasArrivalAngleTolerance, "Transition has arrivalAngle but no arrivalAngleTolerance");
		CRY_ASSERT_MESSAGE( hasDesiredArrivalAngle || !hasArrivalAngleTolerance, "Transition has arrivalAngleTolerance but no arrivalAngle");
	}
	else
	{
		desiredTargetTravelAngle = 0.0f;
		bool hasDesiredTargetTravelAngle = ReadAngleParam("targetTravelAngle", false, -gf_PI2, gf_PI2, pParams, &desiredTargetTravelAngle);

		targetTravelAngleTolerance = FLT_MAX;
		bool hasTargetTravelAngleTolerance = ReadAngleParam("targetTravelAngleTolerance", false, 0.0f, gf_PI2, pParams, &targetTravelAngleTolerance);

		CRY_ASSERT_MESSAGE(!hasDesiredTargetTravelAngle ||  hasTargetTravelAngleTolerance, "Transition has targetTravelAngle but no targetTravelAngleTolerance");
		CRY_ASSERT_MESSAGE( hasDesiredTargetTravelAngle || !hasTargetTravelAngleTolerance, "Transition has targetTravelAngleTolerance but no targetTravelAngle");

		if (transitionType == eTT_DirectionChange)
		{
			desiredJukeAngle = 0.0f;
			if (!ReadAngleParam("jukeAngle", true, -gf_PI2, gf_PI2, pParams, &desiredJukeAngle)) return false;

			jukeAngleTolerance = FLT_MAX;
			if (!ReadAngleParam("jukeAngleTolerance", true, 0.0f, gf_PI2, pParams, &jukeAngleTolerance)) return false;
		}
		else // if (transitionType == eTT_Start)
		{
			desiredJukeAngle = 0.0f;
			jukeAngleTolerance = FLT_MAX;
		}
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool STransition::ReadPseudoSpeedParam( const char* name, bool required, const struct IItemParamsNode*const pParams, float*const pPseudoSpeed ) const
{
	const char* szPseudoSpeed = pParams->GetAttribute(name);

	if (!szPseudoSpeed && !required)
		return false;

	CRY_ASSERT_TRACE(szPseudoSpeed, ("%s not found", name));
	if (!szPseudoSpeed)
		return false;

	// look for pseudospeed name (case insensitive)
	float pseudoSpeed = -1.0f;

	for(int iPseudoSpeed = 0; iPseudoSpeed < MovementTransitionsUtil::NUM_PSEUDOSPEEDS; ++iPseudoSpeed)
	{
		if (0 == strcmpi(szPseudoSpeed, MovementTransitionsUtil::s_pseudoSpeeds[iPseudoSpeed].name))
		{
			pseudoSpeed = MovementTransitionsUtil::s_pseudoSpeeds[iPseudoSpeed].value;
			break;
		}
	}

	if (pseudoSpeed == -1.0f)
	{
		CRY_ASSERT_TRACE(false, ("Invalid pseudospeed '%s'", szPseudoSpeed));
		return false;
	}

	*pPseudoSpeed = pseudoSpeed;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool STransition::ReadStanceParam( const char* name, bool required, const struct IItemParamsNode*const pParams, EStance*const pStance ) const
{
	const char* szStance = pParams->GetAttribute(name);

	if (!szStance && !required)
		return false;

	CRY_ASSERT_TRACE(szStance, ("%s not found", name));
	if (!szStance)
		return false;

	// look for stance name (case insensitive)
	int stance = STANCE_NULL;
	for(; stance < STANCE_LAST; ++stance)
	{
		if (0 == strcmpi(szStance, ::GetStanceName((EStance)stance)))
			break;
	}

	if (((EStance)stance == STANCE_LAST) && (strlen(szStance) > 0))
	{
		CRY_ASSERT_TRACE(false, ("Invalid stance '%s'", szStance));
		return false;
	}

	CRY_ASSERT(stance >= STANCE_NULL);
	CRY_ASSERT(stance < STANCE_LAST);

	*pStance = (EStance)stance;
	return true;
}


//////////////////////////////////////////////////////////////////////////
bool STransition::ReadIntParam( const char* name, bool required, int min, int max, const struct IItemParamsNode*const pParams, int*const pI ) const
{
	int i;

	bool success = pParams->GetAttribute(name, i);

	if (!success && !required)
		return false;

	CRY_ASSERT_TRACE(success, ("%s not found", name));
	if (!success) return false;
	CRY_ASSERT_TRACE(i >= min, ("%s (%d) too small (should be > %d)", name, i, min));
	if (!(i >= min)) return false;
	CRY_ASSERT_TRACE(i <= max, ("%s (%d) too big (should be < %d)", name, i, max));
	if (!(i <= max)) return false;

	*pI = i;
	return true;
}


//////////////////////////////////////////////////////////////////////////
bool STransition::ReadFloatParam( const char* name, bool required, float min, float max, const struct IItemParamsNode*const pParams, float*const pF ) const
{
	float f;

	bool success = pParams->GetAttribute(name, f);

	if (!success && !required)
		return false;

	CRY_ASSERT_TRACE(success, ("%s not found", name));
	if (!success) return false;
	CRY_ASSERT_TRACE(f >= min, ("%s (%f) too small (should be > %f)", name, f, min));
	if (!(f >= min)) return false;
	CRY_ASSERT_TRACE(f <= max, ("%s (%f) too big (should be < %f)", name, f, max));
	if (!(f <= max)) return false;

	*pF = f;
	return true;
}


//////////////////////////////////////////////////////////////////////////
bool STransition::ReadAngleParam( const char* name, bool required, float min, float max, const struct IItemParamsNode*const pParams, float*const pAngle ) const
{
	float angleDegrees;

	if (!ReadFloatParam(name, required, RAD2DEG(min), RAD2DEG(max), pParams, &angleDegrees))
		return false;

	*pAngle = DEG2RAD(angleDegrees);
	return true;
}


//////////////////////////////////////////////////////////////////////////
bool STransition::CheckSpaceToPerformTransition( const CMovementTransitions& transitions, const STransitionSelectionParams &transParams, const Vec3 &oldMoveDirection, const Vec3& playerPos, CPlayer*const pPlayer) const
{
	if ((transitionType == eTT_DirectionChange) && !transParams.m_bPredicted)
	{
		const IAIObject* pAI = pPlayer->GetEntity()->GetAI();
		if (const IAIPathAgent* pAIAgent = pAI->CastToIAIActor())
		{
			if (const IPathFollower* pPathFollower = pAIAgent->GetPathFollower())
			{
				Vec2 path[2];
				CRY_ASSERT(oldMoveDirection.IsUnit());
				path[0] = Vec2(playerPos) + Vec2(oldMoveDirection) * (minDistance+maxDistance)*0.5f;
				path[1] = path[0] + Vec2(transParams.m_future.vMoveDirection) * transitions.GetMinDistanceAfterDirectionChange();

				return pPathFollower->CheckWalkability(path, 2);
			}
		}

		return false;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
#ifndef _RELEASE
string STransition::GetDescription() const
{
	CryFixedStringT<256> sResult;
	sResult.Format("signal=%s\ttype=%s\tspeed=%s\tdir=%+3.1f\tarrdir=%+3.1f\ttargtdir=%+3.1f\tjukeAngle=%+3.1f", 
		animGraphSignal.c_str(), 
		MovementTransitionsDebug::GetTransitionTypeName(transitionType), 
		MovementTransitionsDebug::GetPseudoSpeedName(pseudoSpeed), 
		RAD2DEG(desiredTravelAngle), 
		RAD2DEG(desiredArrivalAngle),
		RAD2DEG(desiredTargetTravelAngle),
		RAD2DEG(desiredJukeAngle)
		);

	return sResult;
}
#endif // ifndef _RELEASE

///////////////////////////////////////////////////////////////////////////////
STransitionSelectionParams::STransitionSelectionParams(
	const CMovementTransitions& transitions,
	const CPlayer& player,
	const CMovementRequest& request,
	const Vec3& playerPos,
	const SMovementTransitionsSample& oldSample,
	const SMovementTransitionsSample& newSample,
	const bool bHasLockedBodyTarget,
	const Vec3& targetBodyDirection,
	const Lineseg& safeLine,
	const CTimeValue runningDuration,
	const uint8 _allowedTransitionFlags,
	const float entitySpeed2D,
	const float entitySpeed2DAvg,
	const SExactPositioningTarget*const pExactPositioningTarget,
	const EStance stance,

	SActorFrameMovementParams*const pMoveParams) : m_transitionType(eTT_None), m_transitionDistance(0.0f), m_pseudoSpeed(0.0f), m_travelAngle(0.0f), m_jukeAngle(0.0f), m_stance(stance)
{
	// TODO: check for flatness?

	m_travelAngle = Ang3::CreateRadZ( newSample.bodyDirection, oldSample.moveDirection ); // probably should be oldSample?
	m_context = request.HasContext() ? request.GetContext() : 0;

	// --------------------------------------------------------------------------
	// Calculate vToMoveTarget, vAfterMoveTarget, distToMoveTarget, distAfterMoveTarget & allowedTransitionFlags

	Vec3 vToMoveTarget, vAfterMoveTarget;
	float distToMoveTarget, distAfterMoveTarget;
	uint8 allowedTransitionFlags = _allowedTransitionFlags;

	{
		if (request.HasMoveTarget())
		{
			const Vec3& vMoveTarget = request.GetMoveTarget();
			vToMoveTarget = vMoveTarget - playerPos;
			distToMoveTarget = vToMoveTarget.GetLength2D();

			m_future.hasMoveTarget = true;
			m_future.vMoveTarget = vMoveTarget;

			// Disallow certain transitions when preparing an exact positioning target
			// and fudge the distance to make sure we don't start when too close to it
			if (pExactPositioningTarget && pExactPositioningTarget->preparing && !pExactPositioningTarget->activated)
			{
				allowedTransitionFlags &= ~BIT(eTT_Stop);
				allowedTransitionFlags &= ~BIT(eTT_DirectionChange);

				const Vec3& exactPosLocation = pExactPositioningTarget->location.t;
				const float distFromMoveTargetToExactPosSquared = vMoveTarget.GetSquaredDistance(exactPosLocation);

				const float minimumDangerDistance = 0.1f;
				const float maxDistanceTraveledPerFrame = gEnv->pTimer->GetFrameTime() * 12.5f;
				const float dangerDistance = max(minimumDangerDistance, maxDistanceTraveledPerFrame);

				const bool moveTargetIsWithinDangerDistance = (distFromMoveTargetToExactPosSquared <= sqr(dangerDistance));
				if (moveTargetIsWithinDangerDistance)
				{
					// Fudge distToMoveTarget so we start at least distanceTraveledInTwoFrames
					// This only works for eTT_Start transitions (but we disabled the others above)
					distToMoveTarget = max(0.0f, distToMoveTarget - dangerDistance);
				}
			}

			if (request.HasInflectionPoint())
			{
				const Vec3& vInflectionPoint = request.GetInflectionPoint();
				vAfterMoveTarget = vInflectionPoint - vMoveTarget;
				distAfterMoveTarget = vAfterMoveTarget.GetLength2D();
			}
			else
			{
				vAfterMoveTarget.zero();
				distAfterMoveTarget = 0.0f;
			}
		}
		else
		{
			m_future.hasMoveTarget = false;

			vToMoveTarget.zero();
			vAfterMoveTarget.zero();
			distToMoveTarget = distAfterMoveTarget = 0.0f;
		}
	}

	// --------------------------------------------------------------------------

	const float maximumSpeedForStart = 0.5f;
	const float minimumSpeedForWalkStop = 1.0f;
	const float minimumSpeedForRunStop = 3.5f;
	const float minimumRunningDurationForRunStop = 1.0f; // (seconds)

	const float minimumSpeedForJuke = 4.4f*0.6f; // 4.4 is slowest runspeed in Crysis2; 0.6 is the strafing slowdown
	const float minimumRunningDurationForJuke = 0.1f; // (seconds)

	if (newSample.pseudoSpeed > 0.0f)
	{
		// Either:
		// - we are in a Stop and want to start again <-- note oldPseudoSpeed cannot be used to detect this, it could be already 0 from prev. frame
		// - we are in a Start and want to continue starting [but possibly stop or change direction at the movetarget]
		// - we are stopped and want to Start [but possibly stop or change direction at the movetarget]
		// - we are moving and want to continue moving [but possibly stop or change direction at the movetarget]

		m_pseudoSpeed = newSample.pseudoSpeed;

		if ( (allowedTransitionFlags & (1<<eTT_Start)) && (entitySpeed2DAvg <= maximumSpeedForStart) )
		{
			//New sample's movement direction is accurate for start transitions.
			m_travelAngle = Ang3::CreateRadZ( newSample.bodyDirection, newSample.moveDirection );
			m_transitionType = eTT_Start;
			m_bPredicted = true;
			m_transitionDistance = distToMoveTarget;
			m_future.vMoveDirection = newSample.moveDirection;
			const Vec3 realTargetBodyDirection = bHasLockedBodyTarget ? targetBodyDirection : m_future.vMoveDirection;
			m_targetTravelAngle = Ang3::CreateRadZ( realTargetBodyDirection, m_future.vMoveDirection );
			m_future.qOrientation = Quat::CreateRotationVDir( realTargetBodyDirection );
			MovementTransitionsLog("[%x] Juke failed because we are trying to start", gEnv->pRenderer->GetFrameID());
		}
		else // at the moment start & stop are mutually exclusive
		{
			if (!(allowedTransitionFlags & (1<<eTT_Start)))
				MovementTransitionsLog("[%x] Start failed because current animation state (%s) doesn't support starting", gEnv->pRenderer->GetFrameID(), const_cast<CPlayer&>(player).GetAnimationGraphState() ? const_cast<CPlayer&>(player).GetAnimationGraphState()->GetCurrentStateName() : "");
			else
				MovementTransitionsLog("[%x] Start failed because speed is %f while maximum %f is allowed", gEnv->pRenderer->GetFrameID(), player.GetAnimatedCharacter()->GetEntitySpeedHorizontal(), maximumSpeedForStart);

			m_transitionType = eTT_None;

			// try immediate directionchange first
			if (allowedTransitionFlags & (1<<eTT_DirectionChange))
			{
				if (
						((oldSample.pseudoSpeed == AISPEED_RUN) || (oldSample.pseudoSpeed == AISPEED_SPRINT)) &&
						(runningDuration > minimumRunningDurationForJuke) &&
						(entitySpeed2D >= minimumSpeedForJuke)
					)
				{
					if (gEnv->pAISystem && !gEnv->pAISystem->GetSmartObjectManager()->CheckSmartObjectStates(player.GetEntity(), "Busy"))
					{
						// === IMMEDIATE DIRECTIONCHANGE ===

						// ---------------------------------
						// Assume a directionchange after moving forward for one meter (assumedDistanceToJuke=1)
						// Look up a transition math for that proposed directionchange
						// ---------------------------------
						m_pseudoSpeed = oldSample.pseudoSpeed;
						m_transitionDistance = -1;

						float assumedDistanceToJuke = 1.0f;
						CRY_ASSERT(assumedDistanceToJuke > FLT_EPSILON);

						Vec3 vToProposedMoveTarget = newSample.moveDirection * distToMoveTarget; // vector from current position to current movetarget
						Vec3 vToProposedJukePoint = oldSample.moveDirection * assumedDistanceToJuke;
						Vec3 vAfterProposedJukePoint = (vToProposedMoveTarget - vToProposedJukePoint).GetNormalizedSafe(newSample.moveDirection);

						m_jukeAngle = Ang3::CreateRadZ( vToProposedJukePoint, vAfterProposedJukePoint );
						m_transitionType = eTT_DirectionChange;
						m_bPredicted = false;
						m_future.vMoveDirection = vAfterProposedJukePoint;
						Vec3 realTargetBodyDirection = bHasLockedBodyTarget ? targetBodyDirection : vAfterProposedJukePoint;
						m_targetTravelAngle = Ang3::CreateRadZ( realTargetBodyDirection, vAfterProposedJukePoint );

						MovementTransitionsLog("[%x] Considering angle %+3.2f", gEnv->pRenderer->GetFrameID(), RAD2DEG(m_jukeAngle));

						const STransition* pTransition = NULL;
						int index = -1;
						STransitionMatch bestMatch;
						transitions.FindBestMatch(*this, &pTransition, &index, &bestMatch);

						if (pTransition)
						{
							// -------------------------------------------------------
							// We found a transition matching our guess. Adjust juke point to match the distance of the transition we found
							// -------------------------------------------------------
							float proposedTransitionDistance = (pTransition->minDistance + pTransition->maxDistance)*0.5f;
							CRY_ASSERT(proposedTransitionDistance > FLT_EPSILON);
							vToProposedJukePoint = oldSample.moveDirection * proposedTransitionDistance;
							vAfterProposedJukePoint = vToProposedMoveTarget - vToProposedJukePoint;
							float proposedDistAfterMoveTarget = vAfterProposedJukePoint.GetLength2D();
							vAfterProposedJukePoint.NormalizeSafe(newSample.moveDirection);

							if (proposedDistAfterMoveTarget >= transitions.GetMinDistanceAfterDirectionChange())
							{
								m_jukeAngle = Ang3::CreateRadZ( vToProposedJukePoint, vAfterProposedJukePoint );
								m_future.vMoveDirection = vAfterProposedJukePoint;
								realTargetBodyDirection = bHasLockedBodyTarget ? targetBodyDirection : vAfterProposedJukePoint;
								m_targetTravelAngle = Ang3::CreateRadZ( realTargetBodyDirection, vAfterProposedJukePoint );

								MovementTransitionsLog("[%x] Proposing angle %+3.2f", gEnv->pRenderer->GetFrameID(), RAD2DEG(m_jukeAngle));

								m_transitionDistance = proposedTransitionDistance;
								m_future.qOrientation = Quat::CreateRotationVDir( realTargetBodyDirection );
							}
							else
							{
								MovementTransitionsLog("[%x] Immediate Juke failed because not enough distance after the juke (distance needed = %f, max distance = %f)", gEnv->pRenderer->GetFrameID(), transitions.GetMinDistanceAfterDirectionChange(), proposedDistAfterMoveTarget);
								m_transitionType = eTT_None;
							}
						}
						else
						{
							MovementTransitionsLog("[%x] Immediate Juke failed because no animation found for this angle/stance/speed", gEnv->pRenderer->GetFrameID());
							m_transitionType = eTT_None;
						}
					}
					else
					{
						MovementTransitionsLog("[%x] Immediate Juke failed because smart object is playing", gEnv->pRenderer->GetFrameID());
					}
				}
				else
				{
					if (!((oldSample.pseudoSpeed == AISPEED_RUN) || (oldSample.pseudoSpeed == AISPEED_SPRINT)))
					{
						MovementTransitionsLog("[%x] Immediate Juke failed because current pseudospeed (%f) is not supported", gEnv->pRenderer->GetFrameID(), oldSample.pseudoSpeed);
					}
					else if (runningDuration <= minimumRunningDurationForJuke)
					{
						MovementTransitionsLog("[%x] Immediate Juke failed because running only %f seconds while more than %f is needed", gEnv->pRenderer->GetFrameID(), runningDuration.GetSeconds(), minimumRunningDurationForJuke);
					}
					else //if (entitySpeed2D < minimumSpeedForJuke)
					{
						MovementTransitionsLog("[%x] Immediate Juke failed because speed is only %f while %f is needed", gEnv->pRenderer->GetFrameID(), entitySpeed2D, minimumSpeedForJuke);
					}
				}
			}
			else
			{
				MovementTransitionsLog("[%x] Immediate Juke failed because current animation state (%s) doesn't support juking", gEnv->pRenderer->GetFrameID(), const_cast<CPlayer&>(player).GetAnimationGraphState() ? const_cast<CPlayer&>(player).GetAnimationGraphState()->GetCurrentStateName() : NULL);
			}

			if (m_transitionType == eTT_None) // directionchange wasn't found
			{
				if ((allowedTransitionFlags & (1<<eTT_Stop)) && (distAfterMoveTarget < FLT_EPSILON))
				{
					// === PREDICTED STOP ===
					// We want to stop in the future
					m_transitionType = eTT_Stop;
					m_bPredicted = true;
					m_transitionDistance = distToMoveTarget;
					m_future.vMoveDirection = vToMoveTarget.GetNormalizedSafe(newSample.moveDirection);
					m_arrivalAngle = request.HasDesiredBodyDirectionAtTarget() ? Ang3::CreateRadZ( request.GetDesiredBodyDirectionAtTarget(), m_future.vMoveDirection ) : 0.0f;
					m_future.qOrientation = request.HasDesiredBodyDirectionAtTarget() ? Quat::CreateRotationVDir(request.GetDesiredBodyDirectionAtTarget()) : Quat::CreateRotationVDir(m_future.vMoveDirection);
					MovementTransitionsLog("[%x] Predicted Juke failed because we are trying to stop", gEnv->pRenderer->GetFrameID());
				}
				else if ((allowedTransitionFlags & (1<<eTT_DirectionChange)) && (distAfterMoveTarget >= transitions.GetMinDistanceAfterDirectionChange()))
				{
					// === PREDICTED DIRECTIONCHANGE ===
					// We want to change direction in the future
					// NOTE: This logic will fail if we trigger the juke really late, because then the distToMoveTarget will be very small and the angle calculation not precise
					m_transitionType = eTT_DirectionChange;
					m_bPredicted = true;
					m_transitionDistance = distToMoveTarget;
					m_jukeAngle = Ang3::CreateRadZ( vToMoveTarget, vAfterMoveTarget );
					m_future.vMoveDirection = vAfterMoveTarget.GetNormalized();
					const Vec3 realTargetBodyDirection = bHasLockedBodyTarget ? targetBodyDirection : m_future.vMoveDirection;
					m_future.qOrientation = Quat::CreateRotationVDir( realTargetBodyDirection );
					m_targetTravelAngle = Ang3::CreateRadZ( realTargetBodyDirection, m_future.vMoveDirection );
				}
			}
		}
	}
	else // if (newSample.pseudoSpeed <= 0.0f)
	{
		// Either:
		// - we are in a Stop and want to continue stopping
		// - we are moving and suddenly want to stop
		// - we are in a Start and want to stop <-- oldPseudoSpeed logic is wrong, oldPseudoSpeed will be 0 for a while so we should use real velocity
		// - we are stopped already and just want to stay stopped
		MovementTransitionsLog("[%x] Juke failed because we are not running or trying to stop", gEnv->pRenderer->GetFrameID());
		MovementTransitionsLog("[%x] Start failed because we are not requesting movement", gEnv->pRenderer->GetFrameID());

		if (
			(( (oldSample.pseudoSpeed == AISPEED_RUN) || (oldSample.pseudoSpeed == AISPEED_SPRINT)) && (runningDuration > minimumRunningDurationForRunStop) && (entitySpeed2D >= minimumSpeedForRunStop))
			||
			((oldSample.pseudoSpeed == AISPEED_WALK) && (entitySpeed2D >= minimumSpeedForWalkStop))
		)
		{
			if (allowedTransitionFlags & (1<<eTT_Stop))
			{
				// === IMMEDIATE STOP ===
				if( gEnv->pAISystem )
				{
					ISmartObjectManager* pSmartObjectManager = gEnv->pAISystem->GetSmartObjectManager();
					if (!pSmartObjectManager || !pSmartObjectManager->CheckSmartObjectStates(player.GetEntity(), "Busy"))
					{
						// Trigger immediate stop when currently running and suddenly wanting to stop.
						//
						// NOTE: If this happens right before a forbidden area and the safeLine is not correct
						//    or the Stop transition distance isn't configured properly the AI will enter it..
						//
						m_pseudoSpeed = oldSample.pseudoSpeed;
						m_transitionDistance = -1;
						m_arrivalAngle = 0.0f;
							m_transitionType = eTT_Stop;
						m_bPredicted = false;

						const STransition* pTransition = NULL;
						int index = -1;
						STransitionMatch bestMatch;
						transitions.FindBestMatch(*this, &pTransition, &index, &bestMatch);

						float minDistanceForStop = pTransition ? pTransition->minDistance : 0.0f;

						bool bIsOnSafeLine = IsOnSafeLine(safeLine, playerPos, newSample.moveDirection, minDistanceForStop);

						if (bIsOnSafeLine)
						{
							m_transitionDistance = minDistanceForStop;
							m_future.vMoveDirection = newSample.moveDirection;
							m_future.qOrientation = Quat::CreateRotationVDir(newSample.moveDirection);

							pMoveParams->desiredVelocity = player.GetEntity()->GetWorldRotation().GetInverted() * player.GetLastRequestedVelocity();

							float maxSpeed = player.GetStanceMaxSpeed(m_stance);
							if (maxSpeed > 0.01f)
								pMoveParams->desiredVelocity /= maxSpeed;
						}
						else
						{
							m_transitionType = eTT_None;
						}
					}
				}
			}
		}
	}

	if (request.HasDesiredSpeed())
	{
		m_future.speed = request.GetDesiredTargetSpeed();
	}
}


//////////////////////////////////////////////////////////////////////////
CMovementTransitions::CMovementTransitions(IEntityClass* pEntityClass) : m_pEntityClass(pEntityClass)
{
	CRY_ASSERT(m_pEntityClass);

	Load();
}


//////////////////////////////////////////////////////////////////////////
CMovementTransitions::~CMovementTransitions()
{
}


///////////////////////////////////////////////////////////////////////////////
void CMovementTransitions::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
}

//////////////////////////////////////////////////////////////////////////
void CMovementTransitions::Load()
{
	Reset();

	if (!g_pGame->GetCVars()->g_movementTransitions_enable)
		return;

	string filename = GetXMLFilename();
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(filename.c_str());

	if (!rootNode || strcmpi(rootNode->getTag(), "MovementTransitions"))
	{
		GameWarning("Could not load movement transition data. Invalid XML file '%s'! ", filename.c_str());
		return;
	}

	IItemParamsNode *paramNode = g_pGame->GetIGameFramework()->GetIItemSystem()->CreateParams();
	paramNode->ConvertFromXML(rootNode);

	ReadGeneralParams(paramNode->GetChild("General"));
	bool success = ReadTransitionsParams(paramNode->GetChild("Transitions"));

	paramNode->Release();

	m_isDataValid = success;
}


///////////////////////////////////////////////////////////////////////////////
EMovementTransitionState CMovementTransitions::Update(
	const uint8 allowedTransitionFlags,
	const Lineseg& safeLine,
	const CTimeValue runningDuration,
	const bool bHasLockedBodyTarget,
	const Vec3& playerPos,
	const SMovementTransitionsSample& oldSample,
	const SMovementTransitionsSample& newSample,
	const float entitySpeed2D,
	const float entitySpeed2DAvg,
	const SExactPositioningTarget*const pExactPositioningTarget,

	CMovementTransitionsController*const pController,
	CPlayer*const pPlayer,
	CMovementRequest*const pRequest,
	SActorFrameMovementParams*const pMoveParams,
	float*const pJukeTurnRateFraction,
	Vec3*const pBodyTarget,
	const char**const pBodyTargetType ) const
{
	if (!g_pGame->GetCVars()->g_movementTransitions_enable)
		return eMTS_None;

	if (!m_isDataValid)
		return eMTS_None;

	Vec3 targetBodyDirection = (*pBodyTarget - playerPos).GetNormalizedSafe(newSample.bodyDirection);

	const EStance upcomingStance = pController->GetUpcomingStance();
	const EStance stance = (upcomingStance == STANCE_NULL) ? pPlayer->GetStance() : upcomingStance;

	STransitionSelectionParams transParams(
		*this, *pPlayer, *pRequest, playerPos, oldSample, newSample, bHasLockedBodyTarget, targetBodyDirection, safeLine, runningDuration, allowedTransitionFlags, entitySpeed2D, entitySpeed2DAvg, pExactPositioningTarget, stance,
		pMoveParams);

	const STransition* pTransition = NULL;
	int index = -1;
	STransitionMatch bestMatch;
	EMovementTransitionState newState = eMTS_None;

	if (transParams.m_transitionType != eTT_None)
	{
		FindBestMatch(transParams, &pTransition, &index, &bestMatch);

		if (pTransition)
		{
			newState = pTransition->Update(*this, transParams, bestMatch, playerPos, oldSample.moveDirection, newSample.moveDirection, pJukeTurnRateFraction, pBodyTarget, pBodyTargetType, pPlayer, pController);
		}
	}

#ifndef _RELEASE
	{
		bool bSignaled = (newState == eMTS_Requesting_Succeeded);

		// Log
		if (g_pGame->GetCVars()->g_movementTransitions_log && pTransition && bSignaled)
		{
			CRY_ASSERT(index < (int)m_transitions.size());
			CryLog("Transition\tentity=%s\tindex=%i\t%s\t%s", 
				pPlayer->GetEntity()->GetName(), 
				index, 
				pTransition->GetDescription().c_str(),
				transParams.m_bPredicted?"Predicted":"Immediate");
		}

		// Debug
		if (g_pGame->GetCVars()->g_movementTransitions_debug)
		{
			if (MovementTransitionsDebug::s_debug_frame != gEnv->pRenderer->GetFrameID())
			{
				MovementTransitionsDebug::s_debug_frame = gEnv->pRenderer->GetFrameID();
				MovementTransitionsDebug::s_debug_y = 50;
			}

			float dist = transParams.m_transitionDistance;
			if (pRequest->HasMoveTarget())
			{
				dist = sqrtf(pRequest->GetMoveTarget().GetSquaredDistance2D(playerPos));
			}

			IRenderAuxText::Draw2dLabel( 8.f, (float)MovementTransitionsDebug::s_debug_y, 1.5f, bSignaled ? MovementTransitionsDebug::s_dbg_color_signaled : MovementTransitionsDebug::s_dbg_color_unsignaled, false,
				"entity=%s\ttype=%s\tspeed=%s\tdist=%3.2f\tTAngle=%3.2f\tarrivalAngle=%3.2f\ttargTAngle=%3.2f\tjukeAngle=%3.2f\tflags=%d",
					pPlayer->GetEntity()->GetName(),
					MovementTransitionsDebug::GetTransitionTypeName(transParams.m_transitionType),
					MovementTransitionsDebug::GetPseudoSpeedName(transParams.m_pseudoSpeed),
					dist,
					RAD2DEG(transParams.m_travelAngle),
					RAD2DEG(transParams.m_arrivalAngle),
					RAD2DEG(transParams.m_targetTravelAngle),
					RAD2DEG(transParams.m_jukeAngle),
					(const unsigned int)allowedTransitionFlags
				);
			MovementTransitionsDebug::s_debug_y += 14;

			if (pTransition)
			{
				IRenderAuxText::Draw2dLabel( 8.f, (float)MovementTransitionsDebug::s_debug_y, 1.5f, bSignaled ? MovementTransitionsDebug::s_dbg_color_signaled : MovementTransitionsDebug::s_dbg_color_unsignaled, false,
					"Transition\tindex=%i\t%s\tangleDiff=%3.2f", index, pTransition->GetDescription().c_str(), RAD2DEG(bestMatch.angleDifference) );
			}
			MovementTransitionsDebug::s_debug_y += 14;
		}
	}
#endif

	return newState;
}


//////////////////////////////////////////////////////////////////////////
void CMovementTransitions::FindBestMatch( const STransitionSelectionParams& transParams, const STransition** ppBestTransition, int* pBestIndex, STransitionMatch* pBestMatch ) const
{
	*ppBestTransition = NULL;
	*pBestIndex = -1;
	STransitionMatch currentMatch;

	TransitionVector::const_iterator iEnd = m_transitions.end();
	int index = 0;
	for(TransitionVector::const_iterator iCurrent = m_transitions.begin(); iCurrent != iEnd; ++iCurrent, ++index)
	{
		const STransition& possibleTransition = *iCurrent;

		if (possibleTransition.IsMatch(transParams, &currentMatch) && currentMatch.IsBetterThan(*pBestMatch))
		{
			*pBestMatch = currentMatch;
			*pBestIndex = index;
			*ppBestTransition = &possibleTransition;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// This special logic for finding a good distance to do a 'quick stop' should 
// be merged into the FindBestMatch and selection logic.
float CMovementTransitions::GetMinStopDistance(float pseudoSpeed, float travelAngle) const
{
	const STransition* pBestMatch = NULL;
	float bestAngleDiff = FLT_MAX;

	TransitionVector::const_iterator iEnd = m_transitions.end();
	for(TransitionVector::const_iterator i = m_transitions.begin(); i != iEnd; ++i)
	{
		const STransition& possibleTransition = *i;

		if ((possibleTransition.pseudoSpeed == pseudoSpeed) && (possibleTransition.transitionType == eTT_Stop))
		{
			float angleDiff = ::AngleDifference(travelAngle, possibleTransition.desiredTravelAngle);
			if (angleDiff <= possibleTransition.travelAngleTolerance)
			{
				if (angleDiff < bestAngleDiff)
				{
					pBestMatch = &(*i);
					bestAngleDiff = angleDiff;
				}
			}
		}
	}

	if (pBestMatch)
		return pBestMatch->minDistance;
	else
		return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
void CMovementTransitions::Reset()
{
	m_transitions.clear();

	m_minDistanceAfterDirectionChange = 2.0f;

	m_isDataValid = false;
}


//////////////////////////////////////////////////////////////////////////
// Reads the (optional) general parameters
void CMovementTransitions::ReadGeneralParams( const struct IItemParamsNode* pParams )
{
	if(!pParams)
		return;

	pParams->GetAttribute("minDistAfterDirectionChange", m_minDistanceAfterDirectionChange);
}


//////////////////////////////////////////////////////////////////////////
bool CMovementTransitions::ReadTransitionsParams( const struct IItemParamsNode* pParams )
{
	if(!pParams)
		return true; // params are optional

	for(int i=0; i<pParams->GetChildCount(); ++i)
	{
		const IItemParamsNode* pEntry = pParams->GetChild(i);
		if (!pEntry)
			continue;

		const char* entryName = pEntry->GetName();

		bool bCorrectEntry = false;
		for(int t = 0; t < MovementTransitionsUtil::NUM_TRANSITIONTYPES; ++t)
		{
			if (strcmp(entryName, MovementTransitionsUtil::s_transitionTypes[t].sTagName) == 0)
			{
				STransition transition;
				if (!transition.ReadParams(MovementTransitionsUtil::s_transitionTypes[t].transitionType, pEntry)) return false;
				m_transitions.push_back(transition);
				bCorrectEntry = true;
				break;
			}
		}

		if (!bCorrectEntry)
		{
			GameWarning("Unknown transition entry '%s' in xml file '%s'", entryName, GetXMLFilename().c_str());
			return false;
		}
	}

	return true;
}

