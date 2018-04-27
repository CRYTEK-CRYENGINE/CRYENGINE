// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "Player.h"
#include "PlayerAnimation.h" // needed for Action priorities only
#include "AnimActionAIMovement.h"
#include "Utility/CryWatch.h"


//////////////////////////////////////////////////////////////////////////
#define MAN_AIMOVEMENT_FRAGMENTS( x ) \
	x( Motion_Idle ) \
	x( Motion_IdleTurn ) \
	x( Motion_IdleTurnBig ) \
	x( Motion_Move ) \
	x( Motion_Air )

#define MAN_AIMOVEMENT_TAGS( x ) \
	x( NoGait )

#define MAN_AIMOVEMENT_TAGGROUPS( x )

#define MAN_AIMOVEMENT_SCOPES( x ) \
	x( FullBody3P )

#define MAN_AIMOVEMENT_CONTEXTS( x )

#define MAN_AIMOVEMENT_FRAGMENT_TAGS( x )

MANNEQUIN_USER_PARAMS( SMannequinAIMovementParams, MAN_AIMOVEMENT_FRAGMENTS, MAN_AIMOVEMENT_TAGS, MAN_AIMOVEMENT_TAGGROUPS, MAN_AIMOVEMENT_SCOPES, MAN_AIMOVEMENT_CONTEXTS, MAN_AIMOVEMENT_FRAGMENT_TAGS );



//////////////////////////////////////////////////////////////////////////
#define MAN_AIDETAIL_FRAGMENTS( x ) \
	x( MotionDetail_Idle ) \
	x( MotionDetail_Move ) \
	x( MotionDetail_IdleTurn ) \
	x( MotionDetail_Nothing )

#define MAN_AIDETAIL_TAGS( x )

#define MAN_AIDETAIL_TAGGROUPS( x )

#define MAN_AIDETAIL_SCOPES( x )

#define MAN_AIDETAIL_CONTEXTS( x )

#define MAN_AIDETAIL_FRAGMENT_TAGS( x )


CAnimActionAIMovement::CAnimActionAIMovement(const SAnimActionAIMovementSettings& settings)
	: TBase(PP_Movement, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut|IAction::Interruptable)
	, m_moveState(eMS_None)
	, m_installedMoveState(eMS_None)
	, m_settings(settings)
	, m_pManParams(NULL)
{
}

void CAnimActionAIMovement::OnInitialise()
{
	CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();

	m_pManParams = mannequinUserParams.FindOrCreateParams<SMannequinAIMovementParams>(m_context->controllerDef);
	CRY_ASSERT(m_pManParams);

	const SMannequinAIMovementParams::FragmentIDs& fragmentIDs = m_pManParams->fragmentIDs;

	m_moveStateInfo[eMS_Idle].m_fragmentID = fragmentIDs.Motion_Idle;
	m_moveStateInfo[eMS_Idle].m_MCM = eMCM_AnimationHCollision;
	m_moveStateInfo[eMS_Idle].m_movementDetail = CAnimActionAIDetail::Idle;
	CRY_ASSERT(m_moveStateInfo[eMS_Idle].m_fragmentID != FRAGMENT_ID_INVALID);

	m_moveStateInfo[eMS_Turn].m_fragmentID = fragmentIDs.Motion_IdleTurn;
	m_moveStateInfo[eMS_Turn].m_MCM = eMCM_AnimationHCollision;
	m_moveStateInfo[eMS_Turn].m_movementDetail = CAnimActionAIDetail::Turn;
	if (m_moveStateInfo[eMS_Turn].m_fragmentID == FRAGMENT_ID_INVALID)
	{
		m_moveStateInfo[eMS_Turn].m_fragmentID = m_moveStateInfo[eMS_Idle].m_fragmentID;
		m_moveStateInfo[eMS_Turn].m_MCM = eMCM_Entity;
	}

	m_moveStateInfo[eMS_TurnBig].m_fragmentID = fragmentIDs.Motion_IdleTurnBig;
	m_moveStateInfo[eMS_TurnBig].m_MCM = eMCM_AnimationHCollision;
	m_moveStateInfo[eMS_TurnBig].m_movementDetail = CAnimActionAIDetail::Turn;
	if (m_moveStateInfo[eMS_TurnBig].m_fragmentID == FRAGMENT_ID_INVALID)
	{
		m_moveStateInfo[eMS_TurnBig].m_fragmentID = m_moveStateInfo[eMS_Turn].m_fragmentID;
		m_moveStateInfo[eMS_TurnBig].m_MCM = m_moveStateInfo[eMS_Turn].m_MCM;
	}

	m_moveStateInfo[eMS_Move].m_fragmentID = fragmentIDs.Motion_Move;
	m_moveStateInfo[eMS_Move].m_MCM = eMCM_Entity;
	m_moveStateInfo[eMS_Move].m_movementDetail = CAnimActionAIDetail::Move;
	CRY_ASSERT(m_moveStateInfo[eMS_Move].m_fragmentID != FRAGMENT_ID_INVALID);

	m_moveStateInfo[eMS_InAir].m_fragmentID = fragmentIDs.Motion_Air;
	m_moveStateInfo[eMS_InAir].m_MCM = eMCM_Entity;
	m_moveStateInfo[eMS_InAir].m_movementDetail = CAnimActionAIDetail::Idle;
	if (m_moveStateInfo[eMS_InAir].m_fragmentID == FRAGMENT_ID_INVALID)
		m_moveStateInfo[eMS_InAir].m_fragmentID = m_moveStateInfo[eMS_Idle].m_fragmentID;

	m_moveState = eMS_Idle;
	m_fragmentID = m_moveStateInfo[m_moveState].m_fragmentID;
}

void CAnimActionAIMovement::Enter()
{
	TBase::Enter();

	SetState(m_moveState);
}

void CAnimActionAIMovement::RequestMovementDetail(CPlayer& player)
{
	CAIAnimationComponent* pAIAnimationComponent = player.GetAIAnimationComponent();
	CRY_ASSERT(pAIAnimationComponent);
	pAIAnimationComponent->RequestAIMovementDetail(m_moveStateInfo[m_moveState].m_movementDetail);
}

void CAnimActionAIMovement::ClearMovementDetail(CPlayer& player)
{
	CAIAnimationComponent* pAIAnimationComponent = player.GetAIAnimationComponent();
	CRY_ASSERT(pAIAnimationComponent);
	pAIAnimationComponent->RequestAIMovementDetail(CAnimActionAIDetail::None);
}

void CAnimActionAIMovement::Exit() 
{
	TBase::Exit();

	m_installedMoveState = eMS_None;
	SetState(eMS_None);

	CPlayer& player = GetPlayer();

	ResetMovementControlMethod(player);
	ClearMovementDetail(player);
}

bool CAnimActionAIMovement::SetState(const EMoveState newMoveState)
{
	const bool changedState = (m_moveState != newMoveState);
	if (changedState)
	{
		m_moveState = newMoveState;

		return true;
	}
	else
	{
		return false;
	}
}

IAction::EStatus CAnimActionAIMovement::UpdatePending(float timePassed)
{
	TBase::UpdatePending(timePassed);

	const EMoveState newMoveState = CalculateState();
	const bool changedState = SetState(newMoveState);

	const bool fragmentWasSet = UpdateFragmentVariation(true /* force update */);

	return m_eStatus;
}

void CAnimActionAIMovement::OnSequenceFinished(int layer, uint32 scopeID)
{
	TBase::OnSequenceFinished(layer, scopeID);

	if (GetRootScope().GetID() == scopeID && layer == 0)
	{
		m_fragmentVariationHelper.OnFragmentEnd();
	}
}

void CAnimActionAIMovement::RequestStance(CPlayer& player, EStance requestedStance, bool urgent)
{
	const bool changingStance = (m_pAnimActionAIStance && (m_pAnimActionAIStance->GetStatus() == IAction::Installed) && !m_pAnimActionAIStance->IsPlayerAnimationStanceSet());
	if (changingStance)
	{
		return;
	}

	const bool pendingStanceChange = (m_pAnimActionAIStance && m_pAnimActionAIStance->GetStatus() == IAction::Pending);
	if (pendingStanceChange)
	{
		if (m_pAnimActionAIStance->GetTargetStance() == requestedStance)
		{
			return;
		}
		m_pAnimActionAIStance->ForceFinish();
		m_pAnimActionAIStance.reset();
	}

	IAnimatedCharacter* pAnimatedCharacter = player.GetAnimatedCharacter();
	CRY_ASSERT(pAnimatedCharacter);
	IActionController* pActionController = pAnimatedCharacter->GetActionController();
	CRY_ASSERT(pActionController);

	_smart_ptr<CAnimActionAIStance> pStanceAction = new CAnimActionAIStance(urgent ? PP_PlayerActionUrgent : PP_PlayerAction, &player, requestedStance);

	float timeUntilInstall = 0;
	const ActionScopes stanceActionScopeMask = pStanceAction->FindStanceActionScopeMask(GetContext());
	const bool canInstall = pActionController->CanInstall(*pStanceAction, stanceActionScopeMask, 10.0f, timeUntilInstall);

	if (canInstall)
	{
		CRY_ASSERT(m_pManParams);
		const IAnimationDatabase& database = player.GetAnimatedCharacter()->GetActionController()->GetScope(m_pManParams->scopeIDs.FullBody3P)->GetDatabase();
		const bool fragmentExistsForStanceChange = pStanceAction->FragmentExistsInDatabase(GetContext(), database);
		if (fragmentExistsForStanceChange)
		{
			m_pAnimActionAIStance = pStanceAction;
			pActionController->Queue(*m_pAnimActionAIStance);
		}
		else
		{
			CAIAnimationComponent* pAIAnimationComponent = player.GetAIAnimationComponent();
			CRY_ASSERT(pAIAnimationComponent);
			CAIAnimationState& animationState = pAIAnimationComponent->GetAnimationState();
			animationState.SetStance(requestedStance);
		}
	}
}



void CAnimActionAIMovement::RequestCoverAction(CPlayer& player, const char *actionName)
{
	const bool isInCoverAction = (m_pAnimActionAICoverAction && m_pAnimActionAICoverAction->GetStatus() == IAction::Installed);
	if (isInCoverAction)
	{
		return;
	}

	const bool pendingCoverAction = (m_pAnimActionAICoverAction && m_pAnimActionAICoverAction->GetStatus() == IAction::Pending);
	if (pendingCoverAction)
	{
		if (m_pAnimActionAICoverAction->IsTargetActionName(actionName))
		{
			return;
		}
		m_pAnimActionAICoverAction->ForceFinish();
		m_pAnimActionAICoverAction.reset();
	}

	IAnimatedCharacter* pAnimatedCharacter = player.GetAnimatedCharacter();
	CRY_ASSERT(pAnimatedCharacter);
	IActionController* pActionController = pAnimatedCharacter->GetActionController();
	CRY_ASSERT(pActionController);

	_smart_ptr<CAnimActionAICoverAction> pCoverAction = new CAnimActionAICoverAction(PP_MovementAction, &player, actionName);

	float timeUntilInstall = 0;
	const ActionScopes coverActionScopeMask = pCoverAction->FindCoverActionScopeMask(GetContext());
	const bool canInstall = pActionController->CanInstall(*pCoverAction, coverActionScopeMask, 10.0f, timeUntilInstall);

	// Install cover action only if there's a chance it'll be the next action to play (e.g. not install if hit death reactions are playing).
	if (canInstall)
	{
		m_pAnimActionAICoverAction = pCoverAction;
		pActionController->Queue(*m_pAnimActionAICoverAction);
	}
}

void CAnimActionAIMovement::CancelCoverAction()
{
	if (m_pAnimActionAICoverAction)
	{
		m_pAnimActionAICoverAction->CancelAction();
	}
}

void CAnimActionAIMovement::CancelStanceChange()
{
	if ( m_pAnimActionAIStance && ( m_pAnimActionAIStance->GetStatus() != IAction::None ) )
	{
		m_pAnimActionAIStance->ForceFinish();
	}
}


void CAnimActionAIMovement::RequestCoverBodyDirection(CPlayer& player, ECoverBodyDirection requestedCoverBodyDirection)
{
	const bool changingCoverBodyDirection = (
		m_pAnimActionAIChangeCoverBodyDirection &&
		(m_pAnimActionAIChangeCoverBodyDirection->GetStatus() == IAction::Installed) &&
		!m_pAnimActionAIChangeCoverBodyDirection->IsTargetCoverBodyDirectionSet());

	if (changingCoverBodyDirection)
	{
		return;
	}

	const bool pendingCoverBodyDirectionChange = (
		m_pAnimActionAIChangeCoverBodyDirection &&
		m_pAnimActionAIChangeCoverBodyDirection->GetStatus() == IAction::Pending);

	if (pendingCoverBodyDirectionChange)
	{
		if (m_pAnimActionAIChangeCoverBodyDirection->GetTargetCoverBodyDirection() == requestedCoverBodyDirection)
		{
			return;
		}
		m_pAnimActionAIChangeCoverBodyDirection->ForceFinish();
		m_pAnimActionAIChangeCoverBodyDirection.reset();
	}

	IAnimatedCharacter* pAnimatedCharacter = player.GetAnimatedCharacter();
	CRY_ASSERT(pAnimatedCharacter);
	IActionController* pActionController = pAnimatedCharacter->GetActionController();
	CRY_ASSERT(pActionController);
	m_pAnimActionAIChangeCoverBodyDirection = new CAnimActionAIChangeCoverBodyDirection(PP_MovementAction, &player, requestedCoverBodyDirection);
	pActionController->Queue(*m_pAnimActionAIChangeCoverBodyDirection);
}


IAction::EStatus CAnimActionAIMovement::Update(float timePassed)
{
	EStatus ret = TBase::Update(timePassed);

	const EMoveState newMoveState = CalculateState();
	const bool changedState = SetState(newMoveState);

	bool forceReevaluate = false;
	bool trumpSelf = true;

	//--- If the turn anim is already half-way through then allow a retrigger with a proper transition

	const bool isTurning = (m_installedMoveState == eMS_Turn) || (m_installedMoveState == eMS_TurnBig);
	if (isTurning)
	{
		trumpSelf = false;

		const bool wantsToTurn = (newMoveState == eMS_Turn) || (newMoveState == eMS_TurnBig);
		if (wantsToTurn)
		{
			if (GetRootScope().GetFragmentTime() > (GetRootScope().GetFragmentDuration() * 0.5f))
			{
				forceReevaluate = true;
			}
		}
	}

	const bool fragmentWasSet = UpdateFragmentVariation(forceReevaluate, trumpSelf);

	// Fallback in case no fragment is present, so OnFragmentStarted will not be called,
	// but we still want the MCM to change
	if (!fragmentWasSet && changedState)
	{
		SetMovementControlMethod(GetPlayer());
	}

	return ret;
}

EMovementControlMethod CAnimActionAIMovement::CalculatePendingMCM(CPlayer& player) const
{
	const CAIAnimationComponent* pAnimationComponent = player.GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	if ( pAnimationComponent->GetUseLegacyCoverLocator() )
	{
		const bool isInCoverStance = IsInCoverStance();
		if (isInCoverStance)
		{
			// TODO: Remove this special case for when in cover stance whenever possible.
			return eMCM_Entity;
		}
		else
		{
			return m_moveStateInfo[m_moveState].m_MCM;
		}
	}
	else
	{
		return m_moveStateInfo[m_moveState].m_MCM;
	}
}


bool CAnimActionAIMovement::IsInCoverStance() const
{
	CPlayer &player = GetPlayer();
	const CAIAnimationComponent* pAnimationComponent = player.GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	const CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();

	const EStance currentStance = animationState.GetStance();
	const bool isInCoverStance = ( currentStance == STANCE_HIGH_COVER || currentStance == STANCE_LOW_COVER );
	return isInCoverStance;
}


void CAnimActionAIMovement::SetMovementControlMethod(CPlayer& player)
{
	const EMovementControlMethod movementControlMethod = CalculatePendingMCM(player);
	if (movementControlMethod != eMCM_Undefined)
	{
		IAnimatedCharacter *animatedCharacter = player.GetAnimatedCharacter();
		animatedCharacter->SetMovementControlMethods(movementControlMethod, eMCM_Entity);
	}
}

void CAnimActionAIMovement::ResetMovementControlMethod(CPlayer& player)
{
	IAnimatedCharacter *animatedCharacter = player.GetAnimatedCharacter();
	if (animatedCharacter)
	{
		animatedCharacter->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
	}
}

void CAnimActionAIMovement::OnFragmentStarted()
{
	CPlayer& player = GetPlayer();

	if (m_installedMoveState != m_moveState)
	{
		m_installedMoveState = m_moveState;

		// Request Fitting Movement Detail
		const bool actionControlsAnimation = (m_eStatus == IAction::Installed) || (m_eStatus == IAction::Exiting);
		if (actionControlsAnimation)
		{
			RequestMovementDetail(player);
		}
	}

	SetMovementControlMethod(player);
}


CAnimActionAIMovement::EMoveState CAnimActionAIMovement::CalculateState()
{
	CPlayer &player = GetPlayer();

	EMoveState ret;

	if (player.IsOnGround() || player.IsSwimming())
	{
		CRY_ASSERT(m_pManParams);
		const TagID noGaitID = m_pManParams->tagIDs.NoGait;
		const EStance currentVisualStance = player.GetAIAnimationComponent()->GetAnimationState().GetStance();
		const bool isVisuallyInCover = (currentVisualStance == STANCE_HIGH_COVER || currentVisualStance == STANCE_LOW_COVER);

		const bool wantsToMove = player.IsMoving();
		const bool currentGaitAllowsMovement = (noGaitID == TAG_ID_INVALID) || (GetContext().state.IsSet(noGaitID) == false);
		const bool currentStanceAllowsMovement = !isVisuallyInCover;
		const bool isAnimTargetForcingMoveState = IsAnimTargetForcingMoveState(player);
		if ((wantsToMove || isAnimTargetForcingMoveState) && currentGaitAllowsMovement && currentStanceAllowsMovement)
		{
			ret = eMS_Move;
		}
		else
		{
			const Quat &animLoc  = player.GetAnimatedCharacter()->GetAnimLocation().q;
			const Quat &viewQuat = player.GetViewQuat();

			const SPredictedCharacterStates& prediction = player.GetAnimatedCharacter()->GetOverriddenMotionParameters();

			float signedAngle = 0.0f;
			prediction.GetParam(eMotionParamID_TurnAngle, signedAngle);
			const float unsignedAngle = fabsf(signedAngle);

			const float MAX_MOVE_SPEED_FOR_TURN = 0.25f;
			const float MIN_TURN_ANGLE = DEG2RAD(1.0f); // hardcoded minimum, we don't kick in turn animations unless above this number
			const float MIN_BIG_TURN_ANGLE = DEG2RAD(120.0f);
			CRY_ASSERT(MIN_BIG_TURN_ANGLE >= MIN_TURN_ANGLE);

			bool doTurn = false;
			if (!isVisuallyInCover)
			{
				// Angle deviation thresholding
				const float minAngle = max(MIN_TURN_ANGLE, m_settings.turnParams.minimumAngle);
				if (unsignedAngle < minAngle)
				{
					// stop timer
					m_deviatedOrientationTime.SetValue(0); 
				}
				else if (m_deviatedOrientationTime.GetValue() == 0)
				{
					// start timer
					m_deviatedOrientationTime = gEnv->pTimer->GetFrameStartTime();
				}

				CTimeValue deviatedOrientationDuration = (m_deviatedOrientationTime.GetValue() != 0) ? (gEnv->pTimer->GetFrameStartTime() - m_deviatedOrientationTime) : 0.0f;
				
				const float immediateTurnAngle = max(minAngle, m_settings.turnParams.minimumAngleForTurnWithoutDelay);
				float timeThresholdSeconds;
				if ((unsignedAngle >= immediateTurnAngle) || (immediateTurnAngle <= minAngle))
				{
					timeThresholdSeconds = 0.0f;
				}
				else
				{
					timeThresholdSeconds = LERP(m_settings.turnParams.maximumDelay, 0.0f, (unsignedAngle - minAngle) / (immediateTurnAngle - minAngle));
				}

				if (deviatedOrientationDuration.GetSeconds() >= timeThresholdSeconds)
				{
					doTurn = true;
				}
			}
			else
			{
				// stop timers
				m_deviatedOrientationTime.SetValue(0);
			}

			ret = doTurn ?
				( (unsignedAngle >= MIN_BIG_TURN_ANGLE) ?
					eMS_TurnBig
					:
					eMS_Turn )
				:
				eMS_Idle;
		}
	}
	else
	{
		ret = eMS_InAir;
	}

	return ret;
}

bool CAnimActionAIMovement::IsAnimTargetForcingMoveState(CPlayer& player) const
{
	IMovementController* pMovementController = player.GetMovementController();
	if (!pMovementController)
		return false;

	const SExactPositioningTarget* pExactPositioningTarget = pMovementController->GetExactPositioningTarget();
	if (!pExactPositioningTarget)
		return false;

	return pExactPositioningTarget->activated || pExactPositioningTarget->preparing;
}

bool CAnimActionAIMovement::UpdateFragmentVariation(const bool forceReevaluate, const bool trumpSelf)
{
	return mannequin::UpdateFragmentVariation(this, &m_fragmentVariationHelper, m_moveStateInfo[m_moveState].m_fragmentID, GetFragTagState(), forceReevaluate, trumpSelf);
}

CPlayer& CAnimActionAIMovement::GetPlayer() const
{
	const EntityId rootEntityId = GetRootScope().GetEntityId();
	return *static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(rootEntityId));
}
