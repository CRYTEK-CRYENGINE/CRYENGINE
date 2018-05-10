// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
Dynamic state of the transitions system
-------------------------------------------------------------------------
History:
- 5:16:2010	15:29 : Created by Sven Van Soom
*************************************************************************/
#include "StdAfx.h"
#include "Player.h" // for CPlayer, ...

#include "PlayerAnimation.h" // for priorities (for now)
#include "MovementTransitionsController.h"
#include "MovementTransitionsSystem.h"
#include "MovementTransitions.h"
#include "GameCVars.h"
#include <CryAISystem/IAIDebugRenderer.h>
#include <CryAISystem/IAIActor.h> // for IAIObject, IAIActor, IAIPathAgent
#include <CryAnimation/ICryAnimation.h>


class CAnimActionMovementTransition : public TAction<SAnimationContext>
{
public:
	typedef TAction<SAnimationContext> TBase;

	DEFINE_ACTION("MovementTransition");

	CAnimActionMovementTransition(FragmentID fragmentID, IAnimatedCharacter& animChar, const STransition*const pTransition, const STransitionFuture& future)
		:
		TBase(PP_PlayerActionUrgent, fragmentID, TAG_STATE_EMPTY, (pTransition->transitionType == eTT_Start) ? NoAutoBlendOut : 0),
		m_animChar(animChar),
		m_pTransition(pTransition),
		m_future(future)
	{
	}

	// IAction implementation ---------------------------------------------------

	bool EarlyExitStartTransition()
	{
		const CAnimation* pAnim = m_rootScope->GetTopAnim(0);
		IF_UNLIKELY (!pAnim)
			return true;
		
		IF_UNLIKELY(pAnim->GetRepeat())
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "I2M segmentation is not properly set up for %s.", m_rootScope->GetCharInst()->GetIAnimationSet()->GetNameByAnimID(pAnim->GetAnimationId()));
			return true;
		}

		// Wait for second segment
		if(pAnim->GetCurrentSegmentIndex() == 0)
			return false;
		
		const SParametricSampler* pParametricSampler = pAnim->GetParametricSampler();
		IF_UNLIKELY (!pParametricSampler)
			return false;

		bool foundTurnAngleParam = false;
		for(int dim = 0; dim < pParametricSampler->m_numDimensions; ++dim)
		{
			if (pParametricSampler->m_MotionParameterID[dim] != eMotionParamID_TurnAngle)
				continue;

			if (!(pParametricSampler->m_MotionParameterFlags[dim] & CA_Dim_LockedParameter))
				continue;

			foundTurnAngleParam = true;

			const f32 turnAngle = pParametricSampler->m_MotionParameter[dim];
			if (turnAngle >= 0)
			{
				// When turning to the left, exit as soon as we enter the second segment
				return true;
			}
			else
			{
				 // When turning to the right, exit as soon when we hit 60% of the second segment
				IF_UNLIKELY (pAnim->GetCurrentSegmentNormalizedTime() >= 0.6f)
					return true;
			}
		}

		// If idle2move has no valid turn angle parameter, exit as soon as the
		// second segment starts (safety check, otherwise we would be stuck here
		// forever)
		if (!foundTurnAngleParam)
			return true;

		return false;
	}

	virtual EStatus Update(float timePassed) override
	{
		TBase::Update(timePassed);

		m_animChar.SetOverriddenMotionParameters(m_cachedMotionParameters);

		if (m_eStatus != Finished)
		{
			if (m_pTransition->transitionType == eTT_Start)
			{
				if (EarlyExitStartTransition())
					ForceFinish();
			}

			if (m_pTransition->transitionType == eTT_Stop)
			{
				const EntityId rootEntityId = GetRootScope().GetEntityId();
				CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));

				CAIAnimationComponent* pAnimationComponent = player.GetAIAnimationComponent();
				CRY_ASSERT( pAnimationComponent );

				const CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();

				const EStance targetStance = m_pTransition->targetStance;
				const bool isGoingToCoverStance = ( targetStance == STANCE_HIGH_COVER || targetStance == STANCE_LOW_COVER );
				if ( isGoingToCoverStance )
				{
					const QuatT* pCoverLocation = animationState.GetCoverLocation();
					if ( pCoverLocation )
					{
						SetParam( "TargetPos", *pCoverLocation );
					}
					else
					{
						CRY_ASSERT_MESSAGE( false, "Going to cover but no cover location set" );
						SetParam( "TargetPos", QuatT( player.GetEntity()->GetWorldTM() ) );
					}
				}
				else
				{
					static const IEntityClass* const s_pPingerEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( "AlienPinger" );
					const IEntity* const pEntity = player.GetEntity();
					const IEntityClass* const pEntityClass = pEntity ? pEntity->GetClass() : NULL;
					const bool isPinger = s_pPingerEntityClass && ( s_pPingerEntityClass == pEntityClass );
					if (isPinger)
					{
						if (m_future.hasMoveTarget)
						{
							QuatT stopLocation;
							stopLocation.t = m_future.vMoveTarget;
							stopLocation.q = pEntity->GetWorldRotation(); //m_future.qOrientation;
							SetParam( "TargetPos", stopLocation );
						}
					}
				}

				if (GetRootScope().GetFragmentTime() > (GetRootScope().GetFragmentDuration() * 0.75f))
				{
					DoExplicitStanceChange(player, pAnimationComponent);
				}
			}
		}

		return m_eStatus;
	}

	virtual EStatus UpdatePending(float timePassed) override
	{
		// Prevent action getting started when there is no fragment available
		if (m_eStatus != Finished)
		{
			if (m_rootScope)
			{
				const bool fragmentExists = FragmentExistsInDatabase(GetContext(), m_rootScope->GetDatabase());
				if (!fragmentExists)
				{
					ForceFinish();
				}
			}
		}

		return m_eStatus;
	}

	virtual void OnAnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event) override
	{
		if (m_eStatus != Installed)
			return;

		static uint32 animExitCRC32 = CCrc32::ComputeLowercase("animexit");
		if (event.m_EventNameLowercaseCRC32 == animExitCRC32)
		{
			ForceFinish();
		}
	}

	virtual void Exit() override
	{
		const EntityId rootEntityId = GetRootScope().GetEntityId();
		CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));

		CAIAnimationComponent* pAIAnimationComponent = player.GetAIAnimationComponent();
		CRY_ASSERT(pAIAnimationComponent);

		pAIAnimationComponent->RequestAIMovementDetail(CAnimActionAIDetail::None);

		m_animChar.SetMovementControlMethods(eMCM_Entity, eMCM_Entity);

		DoExplicitStanceChange(player, pAIAnimationComponent);

		TBase::Exit();
	}

	virtual void OnFragmentStarted() override
	{
		m_cachedMotionParameters = m_animChar.GetOverriddenMotionParameters();
		FillMotionParameters(&m_cachedMotionParameters, *m_animChar.GetEntity(), m_pTransition->transitionType, m_future);
		m_animChar.SetOverriddenMotionParameters(m_cachedMotionParameters);

		m_animChar.SetMovementControlMethods(eMCM_AnimationHCollision, eMCM_Entity);

		// Request Detail
		{
			const EntityId rootEntityId = GetRootScope().GetEntityId();
			CPlayer &player = *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));

			CAIAnimationComponent* pAIAnimationComponent = player.GetAIAnimationComponent();
			CRY_ASSERT(pAIAnimationComponent);

			switch(m_pTransition->transitionType)
			{
			case eTT_Start:
			case eTT_DirectionChange:
				pAIAnimationComponent->RequestAIMovementDetail(CAnimActionAIDetail::Move);
				break;
			case eTT_Stop:
				pAIAnimationComponent->RequestAIMovementDetail(CAnimActionAIDetail::Idle);
				break;
			default:
				pAIAnimationComponent->RequestAIMovementDetail(CAnimActionAIDetail::None);
				break;
			}
		}
	}

	// ~IAction implementation --------------------------------------------------

	const STransition* GetTransition() const
	{
		return m_pTransition;
	}

	void DoExplicitStanceChange(CPlayer &player, CAIAnimationComponent* pAIAnimationComponent)
	{
		const EStance targetStance = m_pTransition->targetStance;
		if (targetStance != STANCE_NULL)
		{
			pAIAnimationComponent->ForceStanceTo(player, targetStance);
		}
	}

private:

	bool FragmentExistsInDatabase(const SAnimationContext& context, const IAnimationDatabase& database) const
	{
		const SFragTagState fragTagState = SFragTagState(context.state.GetMask(), TAG_STATE_EMPTY);
		
		const uint32 optionCount = database.FindBestMatchingTag(SFragmentQuery(m_fragmentID, fragTagState));
		return (0 < optionCount);
	}

	static void FillMotionParameters(SPredictedCharacterStates* pMotionParameters, IEntity& entity, ETransitionType transitionType, const STransitionFuture& future)
	{
		SPredictedCharacterStates prediction;

		if ((transitionType == eTT_Start) || (transitionType == eTT_DirectionChange))
		{
			const float turnAngle = Ang3::CreateRadZ(entity.GetForwardDir(), future.qOrientation.GetColumn1());
			pMotionParameters->SetParam(eMotionParamID_TurnAngle, turnAngle);

			if (future.speed != -1.0f)
			{
				pMotionParameters->SetParam(eMotionParamID_TravelSpeed, future.speed);
			}

			const float travelAngle = Ang3::CreateRadZ(future.qOrientation.GetColumn1(), future.vMoveDirection);
			pMotionParameters->SetParam(eMotionParamID_TravelAngle, travelAngle);
		}
		else
		{
			CRY_ASSERT(transitionType == eTT_Stop);
			float stopLeg = CalculateStopLeg(entity);
			pMotionParameters->SetParam(eMotionParamID_StopLeg, stopLeg);
		}
	}

	// the (locked) leg (- = left, + = right)
	static float CalculateStopLeg(IEntity& entity)
	{
		ICharacterInstance* pICharacter = entity.GetCharacter(0);
		IF_UNLIKELY(!pICharacter)
			return 0.0f;

		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		IF_UNLIKELY(!pISkeletonAnim)
			return 0.0f;

		const uint32 layer = 0;

		const int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(layer);
		IF_UNLIKELY(nAnimsInFIFO <= 1) // there should be at least 2 animations in the FIFO: the Stop and the Movement before it
			return 0.0f;

		const CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(layer, nAnimsInFIFO - 2);

		IF_UNLIKELY(!anim.HasStaticFlag(CA_LOOP_ANIMATION))
		{
			return 0.0f;
		}

		const SParametricSampler* pParametricSampler = anim.GetParametricSampler();
		IF_UNLIKELY (!pParametricSampler)
			return 0.0f;

		const f32 fAnimTime = anim.GetCurrentSegmentNormalizedTime();
		f32 stopLeg = 0.0f;

		if ((fAnimTime < 0.25f) || (fAnimTime > 0.75f))
		{
			//stop with LEFT leg on ground
			stopLeg = -0.5f;
		}
		else
		{
			//stop with RIGHT leg on ground
			stopLeg = +0.5f;
		}

		return stopLeg;
	}

	IAnimatedCharacter& m_animChar;
	const STransition*const m_pTransition;
	STransitionFuture m_future;

	SPredictedCharacterStates m_cachedMotionParameters;
};



//////////////////////////////////////////////////////////////////////////
CMovementTransitionsController::CMovementTransitionsController(CPlayer* pPlayer) : m_pPlayer(pPlayer)
{
	Reset();
}


//////////////////////////////////////////////////////////////////////////
CMovementTransitionsController::~CMovementTransitionsController()
{
}


//////////////////////////////////////////////////////////////////////////
void CMovementTransitionsController::Reset()
{
	m_safeLine.start.zero();
	m_safeLine.end.zero();
	m_allowedTransitionFlags = 0;

	m_pAnimAction = NULL;
	m_pPendingAnimAction = NULL;

	CRY_ASSERT(m_pPlayer);

	m_pMovementTransitions = g_pGame->GetMovementTransitionsSystem().GetMovementTransitions(m_pPlayer->GetEntity()); // cache pointer to transitions for this controller

	m_runStartTime.SetValue(0);

	m_oldMovementSample.Reset();
	m_newMovementSample.Reset();

	m_prevEntitySpeed2D = -1;

	m_state = eMTS_None;
}

//////////////////////////////////////////////////////////////////////////
bool CMovementTransitionsController::HandleEvent( const SGameObjectEvent& event )
{
	switch(event.event)
	{
	case eCGE_AllowStartTransitionEnter:						m_allowedTransitionFlags |= (1<<eTT_Start);						return true;
	case eCGE_AllowStartTransitionExit:							m_allowedTransitionFlags &= ~(1<<eTT_Start);					return true;
	case eCGE_AllowStopTransitionEnter:							m_allowedTransitionFlags |= (1<<eTT_Stop);						return true;
	case eCGE_AllowStopTransitionExit:							m_allowedTransitionFlags &= ~(1<<eTT_Stop);						return true;
	case eCGE_AllowDirectionChangeTransitionEnter:	m_allowedTransitionFlags |= (1<<eTT_DirectionChange);	return true;
	case eCGE_AllowDirectionChangeTransitionExit:		m_allowedTransitionFlags &= ~(1<<eTT_DirectionChange);return true;
	default:
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovementTransitionsController::Update(
	const Vec3& playerPos,
	const float newPseudoSpeed,
	const Vec3& currentBodyDirection,
	const Vec3& newMoveDirection,
	const bool bHasLockedBodyTarget,
	const SExactPositioningTarget*const pExactPositioningTarget,

	CMovementRequest*const pRequest,
	SActorFrameMovementParams*const pMoveParams,
	float*const pJukeTurnRateFraction,
	Vec3*const pBodyTarget,
	const char**const pBodyTargetType )
{
	CRY_ASSERT(IsEnabled());

	if (!m_pPlayer->GetAnimatedCharacter())
		return;

	{
		m_oldMovementSample = m_newMovementSample;

		m_newMovementSample.time = gEnv->pTimer->GetCurrTime();
		m_newMovementSample.moveDirection = newMoveDirection;
		m_newMovementSample.pseudoSpeed = newPseudoSpeed;
		m_newMovementSample.desiredVelocity = pMoveParams->desiredVelocity;
		m_newMovementSample.bodyDirection = currentBodyDirection;
	}

	// If main action is done, pending action becomes main action
	{
		const bool mainActionIsDone = !m_pAnimAction || (m_pAnimAction->GetStatus() == IAction::None) || (m_pAnimAction->GetStatus() == IAction::Finished);
		if (mainActionIsDone)
		{
			m_pAnimAction = m_pPendingAnimAction;
			m_pPendingAnimAction = NULL;
		}
	}

	// Stop any pending actions, we need to reevaluate their preconditions anyway
	{
		const bool mainActionIsPending = m_pAnimAction && (m_pAnimAction->GetStatus() == IAction::Pending);
		if (mainActionIsPending)
		{
			m_pAnimAction->ForceFinish();
			m_pAnimAction = NULL;
		}

		if (m_pPendingAnimAction)
		{
			m_pPendingAnimAction->ForceFinish();
			m_pPendingAnimAction = NULL;
		}
	}

	UpdateSafeLine(*pRequest, playerPos, newMoveDirection);

	CTimeValue runningDuration = UpdateRunningDuration(m_oldMovementSample.pseudoSpeed);

	const float entitySpeed2D = m_pPlayer->GetAnimatedCharacter()->GetEntitySpeedHorizontal();
	const float avgEntitySpeed2D = (m_prevEntitySpeed2D == -1) ? entitySpeed2D : (m_prevEntitySpeed2D + entitySpeed2D)*0.5f;
	m_prevEntitySpeed2D = entitySpeed2D;

	EMovementTransitionState oldState = m_state;
	m_state = m_pMovementTransitions->Update(
		m_allowedTransitionFlags,
		m_safeLine,
		runningDuration,
		bHasLockedBodyTarget,
		playerPos,
		m_oldMovementSample,
		m_newMovementSample,
		entitySpeed2D,
		avgEntitySpeed2D,
		pExactPositioningTarget,

		this,
		m_pPlayer,
		pRequest,
		pMoveParams,
		pJukeTurnRateFraction,
		pBodyTarget,
		pBodyTargetType);
}


void CMovementTransitionsController::UpdatePathFollowerState()
{
	CRY_ASSERT(IsEnabled());

	if (IEntity* pEntity = m_pPlayer->GetEntity())
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (const IAIPathAgent* pAIPathAgent = pAI->CastToIAIActor())
			{
				if (IPathFollower* pPathFollower = pAIPathAgent->GetPathFollower())
				{
					const bool transitionIsRunningOrAboutToRun = 
					(IsTransitionRequestedOrPlaying() ||
						(m_state == eMTS_Preparing) ||
						(m_state == eMTS_Requesting_DelayedBecauseAngleOutOfRange) ||
						(m_state == eMTS_Requesting_DelayedBecauseControllerRequestFailed) ||
						(m_state == eMTS_Requesting_DelayedBecauseWalkabilityFail));

					pPathFollower->SetAllowCuttingCorners(!transitionIsRunningOrAboutToRun);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovementTransitionsController::UpdateSafeLine(const CMovementRequest& request, const Vec3& playerPos, const Vec3& moveDirection)
{
	if (request.HasMoveTarget())
	{
		// when we have a MoveTarget, we can assume the path towards that is 'safe'
		m_safeLine.start = playerPos;
		m_safeLine.end = request.GetMoveTarget();
	}
	else
	{
		m_safeLine.start = playerPos;
		m_safeLine.end = playerPos;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMovementTransitionsController::RequestTransition(const char* szFragmentID, const STransition*const pTransition, const STransitionFuture& future)
{
	CRY_ASSERT(pTransition);

	IActionController* pActionController = GetActionController();
	IF_UNLIKELY(!pActionController)
		return false;

	const FragmentID fragmentID = pActionController->GetContext().controllerDef.m_fragmentIDs.Find(szFragmentID);
	if (fragmentID == FRAGMENT_ID_INVALID)
	{
		return false;
	}
	else
	{
		IAnimatedCharacter* pAnimChar = m_pPlayer->GetAnimatedCharacter();
		CAnimActionMovementTransition* pNewAnimAction = new CAnimActionMovementTransition(fragmentID, *pAnimChar, pTransition, future);

		const bool mainActionIsDone = !m_pAnimAction || (m_pAnimAction->GetStatus() == IAction::None) || (m_pAnimAction->GetStatus() == IAction::Finished);
		if (mainActionIsDone)
		{
			m_pAnimAction = pNewAnimAction;
		}
		else
		{
			CRY_ASSERT(!m_pPendingAnimAction);

			m_pPendingAnimAction = pNewAnimAction;
		}

		pActionController->Queue(*pNewAnimAction);
		return true;
	}
}


//////////////////////////////////////////////////////////////////////////
void CMovementTransitionsController::CancelTransition()
{
	if (m_pAnimAction)
	{
		m_pAnimAction->ForceFinish();
		m_pAnimAction = NULL;
	}

	if (m_pPendingAnimAction)
	{
		m_pPendingAnimAction->ForceFinish();
		m_pPendingAnimAction = NULL;
	}

	m_state = eMTS_None;
}


//////////////////////////////////////////////////////////////////////////
CTimeValue CMovementTransitionsController::UpdateRunningDuration( const float oldPseudoSpeed )
{
	const float minimumSpeedForRunStopSquared = sqr(3.5f);

	if (((oldPseudoSpeed == AISPEED_RUN) || (oldPseudoSpeed == AISPEED_SPRINT)) && (m_pPlayer->GetLastRequestedVelocity().GetLengthSquared2D() > minimumSpeedForRunStopSquared))
	{
		CTimeValue frameStartTime = gEnv->pTimer->GetFrameStartTime();
		if (m_runStartTime.GetValue() == 0)
		{
			m_runStartTime = frameStartTime;
			return CTimeValue();
		}
		else
		{
			return (frameStartTime - m_runStartTime);
		}
	}
	else
	{
		m_runStartTime.SetValue(0);
		return CTimeValue();
	}
}


//////////////////////////////////////////////////////////////////////////
EStance CMovementTransitionsController::GetUpcomingStance() const
{
	if (m_pAnimAction && m_pAnimAction->GetStatus() == IAction::Installed)
	{
		const CAnimActionMovementTransition& animAction = *static_cast<CAnimActionMovementTransition*>(m_pAnimAction.get());
		const STransition*const pTransition = animAction.GetTransition();
		return pTransition->targetStance;
	}
	else
	{
		return STANCE_NULL;
	}
}


//////////////////////////////////////////////////////////////////////////

CCornerSmoother::SCurveSettings CCornerSmoother::normalCurveSettings = {
	1.0f, // wrongWaySpeed -- speed to slow down to when going in the opposite direction
	0.1f, // speedSmoothTime
	0.7f, // walkSmoothTime
	1.1f, // walkSpeed
	0.1f, // runSmoothTime
	4.5f  // runSpeed
};

/*
CCornerSmoother::SCurveSettings CCornerSmoother::sharpCurveSettings = {
	0.1f, // wrongWaySpeed
	0.05f,// speedSmoothTime
	0.7f, // walkSmoothTime
	1.1f, // walkSpeed
	0.1f, // runSmoothTime
	4.5f  // runSpeed
};
*/

int CCornerSmoother::m_lastPredictionFrameID = 0;
int CCornerSmoother::m_numPredictionsThisFrame = 0;


//////////////////////////////////////////////////////////////////////////

static float END_OF_PATH_DISTANCE = 1.5f;
static int END_OF_PATH_FRAMES = 4;
static float STOP_CORNER_SMOOTHING_DISTANCE = 3.0f; // the distance over which the corner smoothing stops linearly (this is before the so-called 'end of path')
float EndOfPathDistance(float desiredSpeed, float dt)
{
	return std::max(END_OF_PATH_DISTANCE, END_OF_PATH_FRAMES*desiredSpeed*dt); // end of path is defined as 0.5m from the movetarget or 4 frames away from the movetarget
}

//////////////////////////////////////////////////////////////////////////

float CCornerSmoother::CalculateDirSmoothTime(const SCurveSettings& settings, const float desiredSpeed, const float distToMoveTarget2D, const float endOfPathDistance)
{
	float distToEndOfCornerSmoothing = std::max<float>(0.0f, distToMoveTarget2D - endOfPathDistance);
	const float speedFactor = clamp_tpl((desiredSpeed - settings.walkSpeed)/(settings.runSpeed - settings.walkSpeed), 0.0f , 1.0f);
	return clamp_tpl(distToEndOfCornerSmoothing*(1.0f/STOP_CORNER_SMOOTHING_DISTANCE), 0.0001f, 1.0f) * LERP(settings.walkSmoothTime, settings.runSmoothTime, speedFactor);
}


//////////////////////////////////////////////////////////////////////////
void CCornerSmoother::Update(
	const Vec3& playerPos, const Vec2& oldMovementDir, const float oldMovementSpeed, const Vec3& animBodyDirection,
	const float desiredSpeed, const Vec3& desiredMovement, const float distToPathEnd,
	const Vec3& moveTarget,
	const bool hasLockedBodyTarget,
	const float maxTurnSpeed, // rad/s
	const float frameTime,
	float*const pNewDesiredSpeed, Vec3*const pNewDesiredMovement)
{
	bool isTrackingSameTarget = (m_oldMoveTarget == moveTarget);
	m_oldMoveTarget = moveTarget;

	// Don't delay the orientation when within about 4 frames from the end of the path,
	// this prevents overshooting at low frame rates
	const float endOfPathDistance = EndOfPathDistance(desiredSpeed, frameTime);
	const bool atEndOfPath = distToPathEnd < endOfPathDistance;
	if (!atEndOfPath && !hasLockedBodyTarget)
	{
		CRY_ASSERT(desiredMovement.IsUnit());

		const Vec2 dir2D = hasLockedBodyTarget ? Vec2(desiredMovement) : Vec2(animBodyDirection).GetNormalized();// assumes animBodyDirection is never vertical //Vec2(viewQuat.GetColumn1()).GetNormalizedSafe(oldMovementDir2D);

		const float bodyAngleDifference = fabsf(Ang3::CreateRadZ(Vec3(dir2D), desiredMovement));
		if (bodyAngleDifference > FLT_EPSILON)
		{
			const SCurveSettings& currentCurveSettings = normalCurveSettings;

			const int curFrameID = gEnv->nMainFrameID;
			static float maxAngleDegrees = 110;
			if (!isTrackingSameTarget)
			{
				const float strafeAngle = (oldMovementSpeed > 0.1f) ? fabsf(Ang3::CreateRadZ(Vec2(animBodyDirection), oldMovementDir)) : 0.0f;

				const bool isStrafing = (strafeAngle > DEG2RAD(1));
				const bool angleTooSharpToSmooth = (RAD2DEG(bodyAngleDifference) > maxAngleDegrees);
				if (EnoughTimePassedSinceLastPrediction(curFrameID) && !isStrafing && !angleTooSharpToSmooth)
				{
					SPredictionSettings settings;
					settings.pCurveSettings = &currentCurveSettings;
					settings.playerPos = playerPos;
					settings.oldMovementSpeed = oldMovementSpeed;
					settings.dir2D = dir2D;
					settings.desiredSpeed = desiredSpeed;
					settings.moveTarget = moveTarget;
					settings.maxTurnSpeed = maxTurnSpeed;
					settings.travelSpeedRate = m_travelSpeedRate; // TODO: is this correct? Shouldn't this be 0?

					PredictAndVerifySmoothTurn(settings);

					RememberLastPredictionFrameID(curFrameID);
				}
				else
				{
					m_prediction.Reset();
					m_simplePrediction.Reset();
				}
			}

			if (IsRunning())
			{
				// Simple orientation smoothing algorithm, works well for near max speed
				// (doesn't work so good for slow speed, and has a lot of magic 'factors')
				const SActorParams &actorParams = m_pPlayer->GetActorParams();
				m_maxTurnSpeed = actorParams.maxDeltaAngleRateNormal;

				float distToMoveTarget2D = (moveTarget - playerPos).GetLength2D();
				m_dirSmoothTime = CalculateDirSmoothTime(currentCurveSettings, desiredSpeed, distToMoveTarget2D, endOfPathDistance);

				// Slow down to wrongWaySpeed when moving the wrong way
				float factor = clamp_tpl(- cosf(bodyAngleDifference), 0.0f , 1.0f);
				float desiredTravelSpeed = LERP(desiredSpeed, currentCurveSettings.wrongWaySpeed, factor);

				// Smooth speed changes (critically damped spring)
				float smoothedSpeed = oldMovementSpeed;
				float oldRate = m_travelSpeedRate;
				SmoothCD(smoothedSpeed, m_travelSpeedRate, frameTime, desiredTravelSpeed, currentCurveSettings.speedSmoothTime);

					*pNewDesiredSpeed = smoothedSpeed;
				*pNewDesiredMovement = hasLockedBodyTarget ? desiredMovement : animBodyDirection;
			}
			else
			{
				*pNewDesiredSpeed = desiredSpeed;
				*pNewDesiredMovement = desiredMovement;
			}
		}
		else
		{
			*pNewDesiredSpeed = desiredSpeed;
			*pNewDesiredMovement = desiredMovement;
			Cancel();
		}
	}
	else
	{
		*pNewDesiredSpeed = desiredSpeed;
		*pNewDesiredMovement = desiredMovement;
		Cancel();
	}

	// The following didn't really improve much:
	// UpdatePathFollowerState();
}


//////////////////////////////////////////////////////////////////////////
void CCornerSmoother::DebugRender(float z) 
{
#ifndef _RELEASE
	if (!g_pGame->GetCVars()->g_movementTransitions_debug)
		return;

	if (!IsRunning())
		return;

	IRenderer* pRend = gEnv->pRenderer;
	Vec3 from = Vec3(m_prediction.samples[0]);
	from.z = z;
	for(int i=1; i<m_prediction.numSamples; ++i)
	{
		Vec3 to = Vec3(m_prediction.samples[i]);
		to.z = z;
		pRend->GetIRenderAuxGeom()->DrawLine( from, ColorF(0.9f,0.9f,1,1), to, ColorF(0.5f,0.5f,1,1) );
		from = to;
	}

	from = Vec3(m_simplePrediction.samples[0]);
	from.z = z;
	for(int i=1; i<m_simplePrediction.numSamples; ++i)
	{
		Vec3 to = Vec3(m_simplePrediction.samples[i]);
		to.z = z;
		pRend->GetIRenderAuxGeom()->DrawLine( from, ColorF(0.9f,0.0f,0.0f,1), to, ColorF(0.5f,0.0f,0.0f,1), 2.0f );
		pRend->GetIRenderAuxGeom()->DrawSphere( from, 0.2f, ColorF(0.9f, 0.0f, 0.0f, 1) );
		from = to;
	}

	//pRend->GetIRenderAuxGeom()->DrawLine( playerPos+Vec3(0,0,0.15f), ColorF(1.0f,0.5f,1.0f,1), playerPos + (qEntityWorldRotation * params.desiredVelocity.GetNormalized())+Vec3(0,0,0.15f), ColorF(1.0f,0.3f,0.3f,1) );
#endif
}


//////////////////////////////////////////////////////////////////////////
bool CCornerSmoother::CheckWalkability(const SPrediction& prediction, CPlayer* pPlayer)
{
	if (prediction.numSamples == 1)
		return true; // if the path to the movetarget is approximately a line we don't need to check walkability (supposing pathfollower did that for us)

	if (const IAIObject* pAI = pPlayer->GetEntity()->GetAI())
	{
		if (const IAIPathAgent* pAIAgent = pAI->CastToIAIActor())
		{
			if (const IPathFollower* pPathFollower = pAIAgent->GetPathFollower())
			{
				Vec2 path[MAX_SAMPLES];
				for(int i=0; i<prediction.numSamples; ++i)
				{
					path[i] = Vec2(prediction.samples[i]);
				}

				return pPathFollower->CheckWalkability(path, prediction.numSamples);
			}
		}
	}

	return false;
}



//////////////////////////////////////////////////////////////////////////
void CCornerSmoother::PredictAndVerifySmoothTurn(const SPredictionSettings& settings)
{
	PredictSmoothTurn(settings, &m_prediction);

	SimplifyPrediction(settings.playerPos, m_prediction, &m_simplePrediction);

	bool bPathIsValid = CheckWalkability(m_simplePrediction, m_pPlayer);
	if (!bPathIsValid)
	{
		m_simplePrediction.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCornerSmoother::UpdatePathFollowerState() const
{
	if (IEntity* pEntity = m_pPlayer->GetEntity())
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (const IAIPathAgent* pAIPathAgent = pAI->CastToIAIActor())
			{
				if (IPathFollower* pPathFollower = pAIPathAgent->GetPathFollower())
				{
					pPathFollower->SetAllowCuttingCorners(!IsRunning());
				}
			}
		}
	}
}


void CCornerSmoother::PredictSmoothTurn(const SPredictionSettings& settings, SPrediction*const pPrediction)
{
	const float dt = 1.0f/8.0f;

	Vec2 pos = Vec2(settings.playerPos);
	Vec2 dir2D = settings.dir2D;
	float speed = settings.oldMovementSpeed;
	float travelSpeedRate = settings.travelSpeedRate;

	const float endOfPathDistance = EndOfPathDistance(settings.desiredSpeed, dt);
	int i = 0;
	while(i < MAX_SAMPLES-1) // '-1' to leave one corner entry for the moveTarget at the end
	{
		const Vec2 toMoveTarget2D = Vec2(settings.moveTarget) - pos;
		float distanceToMoveTarget2DSq = toMoveTarget2D.GetLength2();
		if (distanceToMoveTarget2DSq < square(endOfPathDistance))
			break;

		const float distanceToMoveTarget2D = sqrtf(distanceToMoveTarget2DSq);
		const float distanceToMoveTarget2DInv = 1.0f/distanceToMoveTarget2D;
		const Vec3 desiredMovement = Vec3(toMoveTarget2D * distanceToMoveTarget2DInv);

		// Slow down to wrongWaySpeed when moving the wrong way
		const float angleDifference = fabsf(Ang3::CreateRadZ(Vec3(dir2D), desiredMovement));
		const float factor = clamp_tpl(- cosf(angleDifference), 0.0f , 1.0f);
		const float desiredTravelSpeed = LERP(settings.desiredSpeed, settings.pCurveSettings->wrongWaySpeed, factor);

		// Smooth speed changes (critically damped spring)
		SmoothCD(speed, travelSpeedRate, dt, desiredTravelSpeed, settings.pCurveSettings->speedSmoothTime);

		// Integrate Velocity
		pos += dir2D * speed * dt;

		// Store Sample
		pPrediction->samples[i++] = pos;

		// Stop when pointing straight towards the target
		if (angleDifference < DEG2RAD(1)) // not entirely arbitrary: distance to moveTarget typically is less than 10m; so we get less than about 18cm (=sin(1deg)*10m) error
			break;

		// Smooth direction (exponential decay with half-time dirSmoothTime)
		const float dirSmoothTime = CalculateDirSmoothTime(*settings.pCurveSettings, settings.desiredSpeed, distanceToMoveTarget2D, endOfPathDistance);
		const float lambda = logf(2.0f) / dirSmoothTime;
		const Vec2 oldDir2D = dir2D;
		dir2D = Vec2(Vec3::CreateSlerp(desiredMovement, Vec3(dir2D), expf(-lambda * dt))).GetNormalizedSafe(Vec2(desiredMovement));

		// Clamp by maximum turn speed
		const float angleDelta = Ang3::CreateRadZ(oldDir2D, dir2D);
		const float maxAngleDelta = settings.maxTurnSpeed * dt;
		const float clampedAngleDiff = clamp_tpl(angleDelta, -maxAngleDelta, maxAngleDelta);

		if (fabsf(clampedAngleDiff) > FLT_EPSILON)
		{
			float cosa, sina;
			sincos_tpl(clampedAngleDiff, &sina, &cosa);
			dir2D = cosa*oldDir2D + sina*oldDir2D.rot90ccw();
		}
	}
	pPrediction->samples[i++] = Vec2(settings.moveTarget);
	pPrediction->numSamples = i;

	CRY_ASSERT(pPrediction->numSamples > 0);
}


void CCornerSmoother::SimplifyPrediction(const Vec3& playerPos, const SPrediction& input, SPrediction*const pOutput)
{
	CRY_ASSERT(input.numSamples>0);

	// cut corners using a simple forward-walking corner-cutting algorithm
	Vec2 lastIncludedPos = Vec2(playerPos);
	pOutput->numSamples = 0;
	int lastIncludedIndex = -1; // we begin at the playerPos, which is not included in the list
	while(lastIncludedIndex < input.numSamples)
	{
		int nextIncludedIndex = lastIncludedIndex + 1;
		if (nextIncludedIndex < input.numSamples)
		{
			while(nextIncludedIndex < input.numSamples - 1)
			{
				// try to advance to the next
				int proposedNextIncludedIndex = nextIncludedIndex + 1;
				Vec2 comparePos = input.samples[(lastIncludedIndex + proposedNextIncludedIndex)/2]; // instead of checking all points, as an approximation I just check the half-way point (which works for 'corner' curves)
				Vec2 newPos = input.samples[proposedNextIncludedIndex];
				Vec2 delta = newPos - lastIncludedPos;
				Lineseg ls = Lineseg(Vec3(lastIncludedPos), Vec3(newPos));
				float lambda;
				float distSq = Distance::Point_Lineseg2DSq(Vec3(comparePos), ls, lambda);
				if (distSq > square(0.25f))
					break;

				nextIncludedIndex = proposedNextIncludedIndex;
			}
		}

		if (nextIncludedIndex < input.numSamples)
		{
			pOutput->samples[pOutput->numSamples++] = input.samples[nextIncludedIndex];
			lastIncludedPos = input.samples[nextIncludedIndex];
		}
		lastIncludedIndex = nextIncludedIndex;
	}

	CRY_ASSERT(pOutput->numSamples > 0);
	CRY_ASSERT(pOutput->samples[pOutput->numSamples-1] == input.samples[input.numSamples-1]);
}
