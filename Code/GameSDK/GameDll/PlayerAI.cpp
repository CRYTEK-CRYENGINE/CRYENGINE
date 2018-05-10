// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//-------------------------------------------------------------------------
//
// Description: 
//  AI-specific functions from CPlayer (which one day should find their way
//  into their own class)
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "Player.h"
#include "AnimActionAIMovement.h"
#include "AnimActionAIAimPose.h"
#include "AnimActionAIAiming.h"
#include "AnimActionAILookPose.h"
#include "AnimActionAILooking.h"
#include "Weapon.h"
#include "ProceduralContextAim.h"
#include "ProceduralContextLook.h"
#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryAISystem/IAIActor.h>

// ============================================================================
// ============================================================================

#define MAN_AI_STATE_FRAGMENTS( x )

#define MAN_AI_STATE_TAGS( x ) \
	x( Dead ) \
	x( Alive ) \
	x( Aiming ) \
	x( NotAiming ) \
	x( Firing ) \
	x( NotFiring ) \
	x( Nw ) \
	x( NoItem ) \
	x( Relaxed ) \
	x( Alerted ) \
	x( Crouch ) \
	x( Swim ) \
	x( Stand ) \
	x( CoverHigh ) \
	x( CoverLow ) \
	x( NoGait ) \
	x( Walk ) \
	x( Run ) \
	x( Sprint ) \
	x( CoverLeft ) \
	x( CoverRight ) \
	x( AlertedOrStand ) \
	x( CoverAlignForward ) \
	x( CoverAlignRight ) \
	x( CoverAlignBack ) \
	x( CoverAlignLeft ) \

#define MAN_AI_STATE_TAGGROUPS( x ) \
	x( Health ) \
	x( Aim ) \
	x( Fire ) \
	x( Item ) \
	x( WeaponType ) \
	x( Stance ) \
	x( Gait ) \
	x( MergedStance ) \
	x( AlignmentToCover )

#define MAN_AI_STATE_SCOPES( x )

#define MAN_AI_STATE_CONTEXTS( x )

#define MAN_AI_STATE_FRAGMENT_TAGS( x )

MANNEQUIN_USER_PARAMS( SMannequinAIStateParams, MAN_AI_STATE_FRAGMENTS, MAN_AI_STATE_TAGS, MAN_AI_STATE_TAGGROUPS, MAN_AI_STATE_SCOPES, MAN_AI_STATE_CONTEXTS, MAN_AI_STATE_FRAGMENT_TAGS );

// ============================================================================
// ============================================================================

//------------------------------------------------------------------------
bool CPlayer::IsPlayingSmartObjectAction() const
{
	bool bResult = false;

	IAIObject *pAI = GetEntity()->GetAI();
	IAIActorProxy *pAIProxy = pAI ? pAI->GetProxy() : NULL;
	if (pAIProxy)
		bResult = pAIProxy->IsPlayingSmartObjectAction();

	return bResult;
}

//------------------------------------------------------------------------
void CPlayer::UpdateAIAnimationState(const SActorFrameMovementParams &frameMovementParams, CWeapon* pWeapon, ICharacterInstance* pICharInst, IActionController* pActionController, IMannequin& mannequinSys)
{
	// ---------------------------------------------------------------------
	// Get Params
	// ---------------------------------------------------------------------
	SAnimationContext &animContext = pActionController->GetContext();

	CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();

	const SMannequinAIStateParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinAIStateParams>(animContext.controllerDef);
	CRY_ASSERT(pParams);

	const SMannequinAIStateParams::TagIDs& aiStateTags = pParams->tagIDs;
	const SMannequinAIStateParams::TagGroupIDs& aiStateTagGroups = pParams->tagGroupIDs;

	// ---------------------------------------------------------------------
	// Create interruptable actions when needed
	// ---------------------------------------------------------------------
	CRY_ASSERT(m_pAIAnimationComponent.get());
	const bool componentInitialized = m_pAIAnimationComponent->InitMannequin(*pActionController, m_animActionAIMovementSettings);
	IF_UNLIKELY( !componentInitialized )
	{
		return;
	}

	m_pAIAnimationComponent->UpdateAnimationStateRequests(*this, frameMovementParams);
	m_pAIAnimationComponent->UpdateStanceAndCover(*this, pActionController);

	// ---------------------------------------------------------------------
	// Set Tags
	// ---------------------------------------------------------------------


	// ---------------------------------------------------------------------
	// Dead/Alive
	// TODO: Turn death/alive into a Fragment/action instead of a tag?
	if (aiStateTagGroups.Health.IsValid())
	{
		if (m_health.IsDead())
			animContext.state.Set(aiStateTags.Dead, true);
		else
			animContext.state.Set(aiStateTags.Alive, true);
	}

	// ---------------------------------------------------------------------
	// Aiming/NotAiming
	if (aiStateTagGroups.Aim.IsValid())
	{
		if(frameMovementParams.aimIK)
			animContext.state.Set(aiStateTags.Aiming, true);
		else
			animContext.state.Set(aiStateTags.NotAiming, true);
	}

	// ---------------------------------------------------------------------
	// Firing/NotFiring
	if (aiStateTagGroups.Fire.IsValid())
	{
		TagID fire = aiStateTags.NotFiring;

		if (pWeapon && pWeapon->IsFiring())
				fire = aiStateTags.Firing;

		animContext.state.SetGroup(aiStateTagGroups.Fire, fire);
	}

	// ---------------------------------------------------------------------
	if (aiStateTagGroups.Item.IsValid())
	{
		const bool isItemSet = animContext.state.GetDef().IsGroupSet(animContext.state.GetMask(), aiStateTagGroups.Item);
		if (!isItemSet)
		{
			animContext.state.Set(aiStateTags.NoItem, true);

			if (aiStateTagGroups.WeaponType.IsValid())
			{
				animContext.state.Set(aiStateTags.Nw, true);
			}
		}
	}

	// ---------------------------------------------------------------------
	// Gait
	if (aiStateTagGroups.Gait.IsValid())
	{
		const float pseudoSpeedValue = GetAIAnimationComponent()->GetAnimationState().GetPseudoSpeed();

		TagID gait = aiStateTags.NoGait;
		if (pseudoSpeedValue >= 0.1f)
		{
			if (pseudoSpeedValue < 0.9f)
				gait = aiStateTags.Walk;
			else if (pseudoSpeedValue < 1.1f)
				gait = aiStateTags.Run;
			else
				gait = aiStateTags.Sprint;
		}
		animContext.state.SetGroup(aiStateTagGroups.Gait, gait);
	}

	// ---------------------------------------------------------------------
	// Alignment to cover
	if (aiStateTagGroups.AlignmentToCover.IsValid())
	{
		const QuatT* pCoverLocation = GetAIAnimationComponent()->GetAnimationState().GetCoverLocation();
		if (pCoverLocation)
		{
			IEntity* const pEntity = GetEntity();
			const Vec3& forwardDirectionWorld = pEntity->GetForwardDir();
			const Vec3& forwardCoverDirectionWorld = pCoverLocation->GetColumn1();

			const Vec2 forwardDirectionWorld2d = Vec2(forwardDirectionWorld).GetNormalizedSafe(Vec2(0.f, 1.f));
			const Vec2 forwardCoverDirectionWorld2d = Vec2(forwardCoverDirectionWorld).GetNormalizedSafe(Vec2(0.f, 1.f));

			const float dotForward = forwardCoverDirectionWorld2d.Dot(forwardDirectionWorld2d);
			const float dotLeft = forwardCoverDirectionWorld2d.Cross(forwardDirectionWorld2d);

			const float absDotForward = fabs_tpl(dotForward);
			const float absDotLeft = fabs_tpl(dotLeft);

			TagID angleToCoverTag = TAG_ID_INVALID;
			if (absDotLeft < absDotForward)
			{
				angleToCoverTag = (0.f < dotForward) ? aiStateTags.CoverAlignForward : aiStateTags.CoverAlignBack;
			}
			else
			{
				angleToCoverTag = (0.f < dotLeft) ? aiStateTags.CoverAlignLeft : aiStateTags.CoverAlignRight;
			}

			animContext.state.SetGroup(aiStateTagGroups.AlignmentToCover, angleToCoverTag);
		}
		else
		{
			animContext.state.ClearGroup(aiStateTagGroups.AlignmentToCover);
		}
	}

	// ---------------------------------------------------------------------
	// Process ClearTags & SetTags AI Commands
	{
		const CTagDefinition& tagDef = animContext.state.GetDef();
		const TagState tagStateStart = animContext.state.GetMask();

		const TagState tagStateCleared = tagDef.GetDifference( tagStateStart, frameMovementParams.mannequinClearTags );
		const TagState tagStateSet = tagDef.GetUnion( tagStateCleared, frameMovementParams.mannequinSetTags );

		animContext.state = tagStateSet;
	}
	


	// ---------------------------------------------------------------------
	// Send events for MovementTransitionsSystem
	// ---------------------------------------------------------------------
	SGameObjectEvent event(eCGE_AllowStartTransitionEnter, eGOEF_ToExtensions);
	GetGameObject()->SendEvent(event);

	event.event = eCGE_AllowStopTransitionEnter;
	GetGameObject()->SendEvent(event);

	event.event = eCGE_AllowDirectionChangeTransitionEnter;
	GetGameObject()->SendEvent(event);
}


// ----------------------------------------------------------------------------
typedef uint32 (*CRCRemapFunction)(uint32 crc);

// ----------------------------------------------------------------------------
static void UpdateTagFromAGInput(CTagState* pTagState, const char* szInputValue, uint32 defaultAGCRC = 0, uint32 defaultTagCRC = 0)
{
	if (szInputValue && (szInputValue[0] != '\0'))
	{
		uint32 inputValueCRC = CCrc32::ComputeLowercase(szInputValue);
		if (inputValueCRC == defaultAGCRC)
		{
			inputValueCRC = defaultTagCRC;
		}
		pTagState->SetByCRC(inputValueCRC, true);
	}
}

// ----------------------------------------------------------------------------
static void UpdateTagFromAGInput(CTagState* pTagState, const char* szInputValue, CRCRemapFunction remapFunction)
{
	if (szInputValue && (szInputValue[0] != '\0'))
	{
		uint32 inputValueCRC = CCrc32::ComputeLowercase(szInputValue);
		if (remapFunction)
		{
			inputValueCRC = remapFunction(inputValueCRC);
		}
		pTagState->SetByCRC(inputValueCRC, true);
	}
}

// ----------------------------------------------------------------------------
static void UpdateTagFromAGInput(CTagState* pTagState, const IAnimationGraphState& AGState, IAnimationGraphState::InputID inputID, uint32 defaultAGCRC = 0, uint32 defaultTagCRC = 0)
{
	char szInputValue[256];
	szInputValue[0] = '\0';
	AGState.GetInput(inputID, szInputValue);
	UpdateTagFromAGInput(pTagState, szInputValue, defaultAGCRC, defaultTagCRC);
}

// ----------------------------------------------------------------------------
static void UpdateTagFromAGInput(CTagState* pTagState, const IAnimationGraphState& AGState, IAnimationGraphState::InputID inputID, CRCRemapFunction remapFunction)
{
	char szInputValue[256];
	szInputValue[0] = '\0';
	AGState.GetInput(inputID, szInputValue);
	UpdateTagFromAGInput(pTagState, szInputValue, remapFunction);
}


namespace
{
	int GetStanceAlertness( const EStance stance )
	{
		switch( stance )
		{
		case STANCE_RELAXED:
			return 0;
		case STANCE_ALERTED:
			return 1;
		case STANCE_CROUCH:
		case STANCE_SWIM:
		case STANCE_STAND:
		case STANCE_HIGH_COVER:
		case STANCE_LOW_COVER:
			return 2;
		case STANCE_NULL:
		default:
			CRY_ASSERT( false );
			return 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CAIAnimationComponent::CAIAnimationComponent( IScriptTable* pScriptTable )
: m_pProceduralContextAim( NULL )
, m_pProceduralContextLook( NULL )
, m_useLegacyCoverLocator( true ) // true by default should make conversion slightly easier
{
	if ( pScriptTable )
	{
		pScriptTable->GetValue( "UseLegacyCoverLocator", m_useLegacyCoverLocator );

		SmartScriptTable propertiesTable;
		if( pScriptTable->GetValue( "Properties", propertiesTable ) )
		{
			const char* forcedTagList = NULL;
			bool success = propertiesTable->GetValue( "TagList", forcedTagList );
			if ( success && forcedTagList )
			{
				m_forcedTagList = forcedTagList;
			}
		}
	}
}

CAIAnimationComponent::~CAIAnimationComponent()
{
}

void CAIAnimationComponent::ResetMannequin()
{
	m_pAnimActionAiMovement.reset();
	m_pAnimActionAiDetail.reset();
	m_pProceduralContextAim = NULL;
	m_pProceduralContextLook = NULL;

	m_animationState.Reset();
}

bool CAIAnimationComponent::InitMannequin( IActionController& actionController, const SAnimActionAIMovementSettings& animActionAIMovementSettings )
{
	const bool isAlreadyInitialized = ( m_pAnimActionAiMovement.get() != NULL );
	IF_LIKELY ( isAlreadyInitialized )
	{
		return true;
	}

	// -------------------------------------------------------------------
	const bool hasInvalidCharacterInstance = ( actionController.GetEntity().GetCharacter(0) == NULL );
	IF_UNLIKELY ( hasInvalidCharacterInstance )
	{
		return false;
	}

	// -------------------------------------------------------------------
	m_animationState.Init( &actionController.GetContext() );

	// -------------------------------------------------------------------
	// Forced TagState
	CTagState& globalTagState = actionController.GetContext().state;
	const CTagDefinition tagDef = globalTagState.GetDef();
	TagState forcedTagState;
	tagDef.TagListToFlags( m_forcedTagList.c_str(), forcedTagState );
	const TagState newGlobalTagState = tagDef.GetUnion( globalTagState.GetMask(), forcedTagState );
	globalTagState = newGlobalTagState;

	// -------------------------------------------------------------------
	// Movement Action
	m_pAnimActionAiMovement.reset( new CAnimActionAIMovement( animActionAIMovementSettings ) );
	actionController.Queue( *m_pAnimActionAiMovement );

	// -------------------------------------------------------------------
	// Movement Detail Action
	const bool supportsMannequinDetailAction = CAnimActionAIDetail::IsSupported( actionController.GetContext() );
	if ( supportsMannequinDetailAction )
	{
		m_pAnimActionAiDetail.reset( new CAnimActionAIDetail() );
		actionController.Queue( *m_pAnimActionAiDetail );
	}

	// -------------------------------------------------------------------
	// Aim
	const bool supportsMannequinAimPoseAction = CAnimActionAIAimPose::IsSupported( actionController.GetContext() );
	const bool supportsMannequinAimingAction = CAnimActionAIAiming::IsSupported( actionController.GetContext() );
	const bool supportsMannequinAiming = ( supportsMannequinAimPoseAction && supportsMannequinAimingAction );
	if ( supportsMannequinAiming )
	{
		m_pProceduralContextAim = static_cast< CProceduralContextAim* >( actionController.FindOrCreateProceduralContext(CProceduralContextAim::GetCID()) );

		actionController.Queue( *new CAnimActionAIAimPose() );
		actionController.Queue( *new CAnimActionAIAiming() );
	}

	// -------------------------------------------------------------------
	// Look
	const bool supportsMannequinLookPoseAction = CAnimActionAILookPose::IsSupported( actionController.GetContext() );
	const bool supportsMannequinLookingAction = CAnimActionAILooking::IsSupported( actionController.GetContext() );
	const bool supportsMannequinLooking = ( supportsMannequinLookPoseAction && supportsMannequinLookingAction );
	if ( supportsMannequinLooking )
	{
		m_pProceduralContextLook = static_cast< CProceduralContextLook* >( actionController.FindOrCreateProceduralContext(CProceduralContextLook::GetCID()) );

		actionController.Queue( *new CAnimActionAILookPose() );
		actionController.Queue( *new CAnimActionAILooking() );
	}

	// TODO: Drone... move somewhere else
	FragmentID hoverID = actionController.GetContext().controllerDef.m_fragmentIDs.Find( "Hover" );
	if ( hoverID != FRAGMENT_ID_INVALID )
	{
		actionController.Queue( *new TPlayerAction( PP_Lowest, hoverID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut|IAction::Interruptable) );
	}

	FragmentID bankID = actionController.GetContext().controllerDef.m_fragmentIDs.Find( "Bank" );
	if ( bankID != FRAGMENT_ID_INVALID )
	{
		actionController.Queue( *new TPlayerAction( PP_Lowest, bankID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut|IAction::Interruptable) );
	}

	return true;
}

void CAIAnimationComponent::RequestAIMovementDetail( CAnimActionAIDetail::EMovementDetail movementDetail )
{
	if ( m_pAnimActionAiDetail.get() )
		m_pAnimActionAiDetail->RequestDetail( movementDetail );
}


void CAIAnimationComponent::UpdateAnimationStateRequests( CPlayer& player, const SActorFrameMovementParams &frameMovementParams )
{
	const EStance stance = frameMovementParams.stance;
	if ( stance != STANCE_NULL )
	{
		m_animationState.SetRequestedStance( stance );
	}

	const ECoverBodyDirection coverBodyDirection = frameMovementParams.coverRequest.coverBodyDirection;
	if ( coverBodyDirection != eCoverBodyDirection_Unspecified )
	{
		m_animationState.SetRequestedCoverBodyDirection( coverBodyDirection );
	}

	const SAICoverRequest& coverRequest = frameMovementParams.coverRequest;
	switch( coverRequest.coverLocationRequest )
	{
	case eCoverLocationRequest_Set:
		{
			CRY_ASSERT( coverRequest.coverLocation.IsValid() );

			const float TOO_FAR_AWAY = 50;
			CRY_ASSERT( coverRequest.coverLocation.t.GetSquaredDistance2D( player.GetEntity()->GetPos() ) < sqr( TOO_FAR_AWAY ) );

			m_animationState.SetCoverLocation( coverRequest.coverLocation );
		}
		break;
	case eCoverLocationRequest_Clear:
		{
			m_animationState.ClearCoverLocation();
		}
		break;
	default:
		break;
	}

	const ECoverAction coverAction = frameMovementParams.coverRequest.coverAction;
	if ( coverAction == eCoverAction_Set )
	{
		const char* const coverActionName = frameMovementParams.coverRequest.coverActionName;
		m_animationState.SetRequestedCoverAction( coverBodyDirection, coverActionName );
	}
	else if ( coverAction == eCoverAction_Clear )
	{
		m_animationState.SetRequestClearCoverAction();
	}
}


void CAIAnimationComponent::UpdateStanceAndCover( CPlayer& player, IActionController* pActionController )
{
	// Make sure we always have a coverbodydirection
	// (Pau will clean this up)
	if ( m_animationState.GetCoverBodyDirection() == eCoverBodyDirection_Unspecified )
	{
		m_animationState.SetCoverBodyDirection( eCoverBodyDirection_Left );
	}

	// Set CoverLocation into TargetPos when needed
	{
		const EStance requestedStance = m_animationState.GetRequestedStance();
		const bool isCoverRequested = ( requestedStance == STANCE_HIGH_COVER || requestedStance == STANCE_LOW_COVER );
		if ( isCoverRequested )
		{
			const QuatT* pCoverLocation = m_animationState.GetCoverLocation();

			if ( pCoverLocation )
			{
				CRY_ASSERT( pCoverLocation->IsValid() );
				pActionController->SetParam( "TargetPos", *pCoverLocation );
			}
			else
			{
				CRY_ASSERT_MESSAGE( false, "Cover stance set but no cover location set" );
				pActionController->RemoveParam( "TargetPos" );
			}
		}
	}

	// Stance
	{
		const EStance requestedStance = m_animationState.GetRequestedStance();
		const EStance currentStance = m_animationState.GetStance();
		if ( requestedStance != currentStance )
		{
			if ( currentStance == STANCE_NULL )
			{
				m_animationState.SetStance( requestedStance );
			}
			else
			{
				m_pAnimActionAiMovement->CancelCoverAction();

				const bool isUrgent = GetStanceAlertness( requestedStance ) >= GetStanceAlertness( currentStance );
				m_pAnimActionAiMovement->RequestStance( player, requestedStance, isUrgent );
				return;
			}
		}
	}

	// Cover Action and Cover Body Direction
	const EStance currentVisualStance = m_animationState.GetStance();
	const EStance currentLogicalStance = player.GetStance();
	const bool isInCoverStance = ( currentVisualStance == STANCE_HIGH_COVER || currentVisualStance == STANCE_LOW_COVER );
	if ( isInCoverStance )
	{
		const bool hasPendingCoverActionChangeRequest = m_animationState.HasPendingCoverActionChangeRequest();
		if ( hasPendingCoverActionChangeRequest )
		{
			const string& requestedCoverAction = m_animationState.GetRequestedCoverActionName();
			if ( requestedCoverAction.empty() )
			{
				m_pAnimActionAiMovement->CancelCoverAction();
				return;
			}
			else
			{
				const ECoverBodyDirection requestedCoverBodyDirection = m_animationState.GetRequestedActionCoverBodyDirection();
				const ECoverBodyDirection currentCoverBodyDirection = m_animationState.GetCoverBodyDirection();
				const ECoverBodyDirection inProgressCoverBodyDirection = m_animationState.GetInProgressCoverBodyDirection();

				// TODO: Try cover action transition to new action with requested cover body direction if it can?

				// If current cover action cannot transition directly, cancel cover action + the do first of the following tasks that is possible: cover body direction change or new action.
				m_pAnimActionAiMovement->CancelCoverAction();
				if ( requestedCoverBodyDirection != eCoverBodyDirection_Unspecified )
				{
					if ( requestedCoverBodyDirection != currentCoverBodyDirection )
					{
						m_pAnimActionAiMovement->RequestCoverBodyDirection( player, requestedCoverBodyDirection );
						return;
					}
				}

				if ( inProgressCoverBodyDirection != eCoverBodyDirection_Unspecified )
				{
					if ( requestedCoverBodyDirection != inProgressCoverBodyDirection )
					{
						m_pAnimActionAiMovement->RequestCoverBodyDirection( player, requestedCoverBodyDirection );
						return;
					}
				}

				m_pAnimActionAiMovement->RequestCoverAction( player, requestedCoverAction.c_str() );
				return;
			}
		}
		else
		{
			const ECoverBodyDirection requestedCoverBodyDirection = m_animationState.GetRequestedCoverBodyDirection();
			const ECoverBodyDirection currentCoverBodyDirection = m_animationState.GetCoverBodyDirection();

			if ( requestedCoverBodyDirection != currentCoverBodyDirection )
			{
				const bool isInCoverAction = m_animationState.IsInCoverAction();
				if ( isInCoverAction )
				{
					// TODO: Cover action should try to handle the case.
					// For the moment faking this because the AI requests cover body directions changes too often.
					const bool coverActionHandledCoverBodyDirectionChange = true;
					if ( ! coverActionHandledCoverBodyDirectionChange )
					{
						m_pAnimActionAiMovement->CancelCoverAction();
						m_pAnimActionAiMovement->RequestCoverBodyDirection( player, requestedCoverBodyDirection );
						return;
					}
				}
				else
				{
					m_pAnimActionAiMovement->RequestCoverBodyDirection( player, requestedCoverBodyDirection );
					return;
				}
			}
		}
	}
	else
	{
		const ECoverBodyDirection requestedCoverBodyDirection = m_animationState.GetRequestedCoverBodyDirection();
		if ( requestedCoverBodyDirection != eCoverBodyDirection_Unspecified )
		{
			m_animationState.SetCoverBodyDirection( requestedCoverBodyDirection );
		}
	}
}


void CAIAnimationComponent::UpdateAimingState( ISkeletonPose* pSkeletonPose, const bool aimEnabled, const Vec3& aimTarget, const uint32 aimIkLayer, const float aimIkFadeoutTime )
{
	const bool supportsMannequinAiming = ( m_pProceduralContextAim != NULL );
	if ( supportsMannequinAiming )
	{
		m_pProceduralContextAim->UpdateGameAimTarget( aimTarget );
		m_pProceduralContextAim->UpdateGameAimingRequest( aimEnabled );
	}
	else
	{
		CRY_ASSERT( pSkeletonPose );
		IAnimationPoseBlenderDir* pPoseBlenderAim = pSkeletonPose->GetIPoseBlenderAim();
		if ( pPoseBlenderAim != NULL )
		{
			pPoseBlenderAim->SetState( aimEnabled );
			if ( aimEnabled )
			{
				pPoseBlenderAim->SetTarget( aimTarget );
				pPoseBlenderAim->SetFadeOutSpeed( aimIkFadeoutTime );
				pPoseBlenderAim->SetLayer( aimIkLayer );
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CAIAnimationComponent::UpdateLookingState( const bool lookEnabled, const Vec3& lookTarget )
{
	const bool supportsMannequinLooking = ( m_pProceduralContextLook != NULL );
	if ( supportsMannequinLooking )
	{
		m_pProceduralContextLook->UpdateGameLookTarget( lookTarget );
		m_pProceduralContextLook->UpdateGameLookingRequest( lookEnabled );
	}
	return supportsMannequinLooking;
}

//////////////////////////////////////////////////////////////////////////
void CAIAnimationComponent::ForceStanceTo( CPlayer& player, const EStance targetStance )
{
	if ( targetStance != STANCE_NULL )
	{
		player.SetStance(targetStance);

		if (m_pAnimActionAiMovement)
			m_pAnimActionAiMovement->CancelStanceChange();

		m_animationState.SetRequestedStance(targetStance);
		m_animationState.SetStance(targetStance);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIAnimationComponent::ForceStanceInAIActorTo( CPlayer& player, const EStance targetStance )
{
	if ( targetStance != STANCE_NULL )
	{
		if( IAIObject *aiObject = player.GetEntity()->GetAI() )
		{
			if( IAIActor* aiActor = CastToIAIActorSafe(aiObject) )
			{
				SOBJECTSTATE& state = aiActor->GetState();
				state.bodystate = targetStance;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
namespace
{
	TagID GetStanceTag( const EStance stance, const SMannequinAIStateParams& params )
	{
		switch( stance )
		{
		case STANCE_RELAXED:
			return params.tagIDs.Relaxed;
		case STANCE_ALERTED:
			return params.tagIDs.Alerted;
		case STANCE_CROUCH:
			return params.tagIDs.Crouch;
		case STANCE_SWIM:
			return params.tagIDs.Swim;
		case STANCE_STAND:
			return params.tagIDs.Stand;
		case STANCE_HIGH_COVER:
			return params.tagIDs.CoverHigh;
		case STANCE_LOW_COVER:
			return params.tagIDs.CoverLow;
		case STANCE_NULL:
			CRY_ASSERT( false );
			return params.tagIDs.Relaxed;
		default:
			CRY_ASSERT( false );
			return TAG_ID_INVALID;
		}
	}

	TagID GetMergedStanceTag( const EStance stance, const SMannequinAIStateParams& params )
	{
		switch( stance )
		{
		case STANCE_ALERTED:
		case STANCE_STAND:
			return params.tagIDs.AlertedOrStand;
		default:
			return TAG_ID_INVALID;
		}
	}

	void SetStanceTags( CTagState* pTagState, const EStance stance, const SMannequinAIStateParams& params )
	{
		if ( params.tagGroupIDs.Stance.IsValid() )
		{
			const TagID tagId = GetStanceTag( stance, params );
			if ( tagId != TAG_ID_INVALID )
				pTagState->SetGroup( params.tagGroupIDs.Stance, tagId );
			else
				pTagState->ClearGroup( params.tagGroupIDs.Stance );
		}

		if ( params.tagGroupIDs.MergedStance.IsValid() )
		{
			const TagID mergedTagId = GetMergedStanceTag( stance, params );
			if ( mergedTagId != TAG_ID_INVALID )
				pTagState->SetGroup( params.tagGroupIDs.MergedStance, mergedTagId );
			else
				pTagState->ClearGroup( params.tagGroupIDs.MergedStance );
		}
	}

	TagID GetCoverBodyDirectionTag( const ECoverBodyDirection coverBodyDirection, const SMannequinAIStateParams& params )
	{
		switch( coverBodyDirection )
		{
		case eCoverBodyDirection_Left:
			return params.tagIDs.CoverLeft;
		case eCoverBodyDirection_Right:
			return params.tagIDs.CoverRight;
		case eCoverBodyDirection_Unspecified:
			CRY_ASSERT( false );
			return params.tagIDs.CoverLeft;
		default:
			CRY_ASSERT( false );
			return TAG_ID_INVALID;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
CAIAnimationState::CAIAnimationState()
: m_pContext( NULL )
, m_pAiUserParams( NULL )
, m_requestedStance( STANCE_NULL )
, m_stance( STANCE_NULL )
, m_requestedCoverBodyDirection( eCoverBodyDirection_Unspecified )
, m_inProgressCoverBodyDirection( eCoverBodyDirection_Unspecified )
, m_coverBodyDirection( eCoverBodyDirection_Unspecified )
, m_requestedActionCoverBodyDirection( eCoverBodyDirection_Unspecified )
, m_requestedCoverActionName( "" )
, m_coverActionName( "" )
, m_coverLocation( IDENTITY )
, m_hasCoverLocation( false )
, m_pseudoSpeed( 0.0f )
{
}

CAIAnimationState::~CAIAnimationState()
{
}

void CAIAnimationState::Init( SAnimationContext* pContext )
{
	CRY_ASSERT( pContext );

	m_pContext = pContext;
	m_pAiUserParams = GetMannequinUserParams< SMannequinAIStateParams >( *pContext );

	CRY_ASSERT( m_pContext );
	CRY_ASSERT( m_pAiUserParams );
}

void CAIAnimationState::Reset()
{
	m_pContext = NULL;
	m_pAiUserParams = NULL;

	m_requestedStance = STANCE_NULL;
	m_stance = STANCE_NULL;
	m_requestedCoverBodyDirection = eCoverBodyDirection_Unspecified;
	m_inProgressCoverBodyDirection = eCoverBodyDirection_Unspecified;
	m_coverBodyDirection = eCoverBodyDirection_Unspecified;
	m_requestedActionCoverBodyDirection = eCoverBodyDirection_Unspecified;
	m_requestedCoverActionName = "";
	m_coverActionName = "";
	m_coverLocation.SetIdentity();
	m_hasCoverLocation = false;
	m_pseudoSpeed = 0.0f;
}

void CAIAnimationState::SetRequestedStance( const EStance stance )
{
	m_requestedStance = stance;
}

EStance CAIAnimationState::GetRequestedStance() const
{
	return m_requestedStance;
}

void CAIAnimationState::SetStance( const EStance stance )
{
	CRY_ASSERT( m_pContext );
	CRY_ASSERT( m_pAiUserParams );

	m_stance = stance;

	SetStanceTags( &m_pContext->state, stance, *m_pAiUserParams );
}

EStance CAIAnimationState::GetStance() const
{
	return m_stance;
}

void CAIAnimationState::SetRequestedCoverBodyDirection( const ECoverBodyDirection coverBodyDirection )
{
	m_requestedCoverBodyDirection = coverBodyDirection;
}

ECoverBodyDirection CAIAnimationState::GetRequestedCoverBodyDirection() const
{
	return m_requestedCoverBodyDirection;
}

void CAIAnimationState::SetInProgressCoverBodyDirection( const ECoverBodyDirection coverBodyDirection )
{
	m_inProgressCoverBodyDirection = coverBodyDirection;
}

ECoverBodyDirection CAIAnimationState::GetInProgressCoverBodyDirection() const
{
	return m_inProgressCoverBodyDirection;
}

void CAIAnimationState::SetCoverBodyDirection( const ECoverBodyDirection coverBodyDirection )
{
	CRY_ASSERT( m_pContext );
	CRY_ASSERT( m_pAiUserParams );

	m_coverBodyDirection = coverBodyDirection;

	const TagID tagId = GetCoverBodyDirectionTag( coverBodyDirection, *m_pAiUserParams );
	if ( tagId != TAG_ID_INVALID )
	{
		m_pContext->state.Set( tagId, true );
	}
}

ECoverBodyDirection CAIAnimationState::GetCoverBodyDirection() const
{
	return m_coverBodyDirection;
}

void CAIAnimationState::SetRequestedCoverAction( const ECoverBodyDirection coverBodyDirection, const char* const actionName )
{
	SetRequestedActionCoverBodyDirection( coverBodyDirection );
	SetRequestedCoverActionName( actionName );
}

void CAIAnimationState::SetRequestClearCoverAction()
{
	SetRequestedActionCoverBodyDirection( eCoverBodyDirection_Unspecified );
	SetRequestedCoverActionName( "" );
}

bool CAIAnimationState::HasPendingCoverActionChangeRequest() const
{
	return ( m_requestedCoverActionName != m_coverActionName );
}

void CAIAnimationState::SetRequestedActionCoverBodyDirection( const ECoverBodyDirection coverBodyDirection )
{
	m_requestedActionCoverBodyDirection = coverBodyDirection;
}

ECoverBodyDirection CAIAnimationState::GetRequestedActionCoverBodyDirection() const
{
	return m_requestedActionCoverBodyDirection;
}

void CAIAnimationState::SetRequestedCoverActionName( const char* const actionName )
{
	CRY_ASSERT( actionName );

	m_requestedCoverActionName = actionName;
}

const string& CAIAnimationState::GetRequestedCoverActionName() const
{
	return m_requestedCoverActionName;
}

void CAIAnimationState::SetCoverActionName( const char* const actionName )
{
	CRY_ASSERT( actionName );

	m_coverActionName = actionName;
}

const string& CAIAnimationState::GetCoverActionName() const
{
	return m_coverActionName;
}

const bool CAIAnimationState::IsInCoverAction() const
{
	const bool isInCoverAction = ! m_coverActionName.empty();
	return isInCoverAction;
}

void CAIAnimationState::SetCoverLocation( const QuatT& location )
{
	CRY_ASSERT( location.IsValid() );
	m_coverLocation = location;
	m_hasCoverLocation = true;
}

void CAIAnimationState::ClearCoverLocation()
{
	m_hasCoverLocation = false;
}

const QuatT* CAIAnimationState::GetCoverLocation() const
{
	if ( m_hasCoverLocation )
	{
		return &m_coverLocation; 
	}
	else
	{
		return NULL;
	}
}