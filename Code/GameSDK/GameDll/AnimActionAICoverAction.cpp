// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimActionAICoverAction.h"
#include "AnimActionAIMovement.h"
#include "Player.h"




//////////////////////////////////////////////////////////////////////////
#define MAN_AICOVERBODYDIR_FRAGMENTS( x )

#define MAN_AICOVERBODYDIR_TAGS( x ) \
	x( CoverLeft ) \
	x( CoverRight )

#define MAN_AICOVERBODYDIR_TAGGROUPS( x )

#define MAN_AICOVERBODYDIR_SCOPES( x )

#define MAN_AICOVERBODYDIR_CONTEXTS( x )

#define MAN_AICOVERBODYDIR_CHANGEFRAGMENT_FRAGMENT_TAGS( x ) \
	x( ToCoverLft ) \
	x( ToCoverRgt )

#define MAN_AICOVERBODYDIR_FRAGMENT_TAGS( x ) \
	x( CoverBodyDirectionChange, MAN_AICOVERBODYDIR_CHANGEFRAGMENT_FRAGMENT_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST )


MANNEQUIN_USER_PARAMS( SMannequinAiCoverBodyDirUserParams, MAN_AICOVERBODYDIR_FRAGMENTS, MAN_AICOVERBODYDIR_TAGS, MAN_AICOVERBODYDIR_TAGGROUPS, MAN_AICOVERBODYDIR_SCOPES, MAN_AICOVERBODYDIR_CONTEXTS, MAN_AICOVERBODYDIR_FRAGMENT_TAGS );


//////////////////////////////////////////////////////////////////////////
typedef SMannequinAiCoverBodyDirUserParams::Fragments::SCoverBodyDirectionChange TCoverBodyDirectionChangeFragment;






//////////////////////////////////////////////////////////////////////////
CAnimActionAIChangeCoverBodyDirection::CAnimActionAIChangeCoverBodyDirection( int priority, CPlayer* pPlayer, const ECoverBodyDirection targetCoverBodyDirection )
: TBase( priority, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY )
, m_pPlayer( pPlayer )
, m_targetCoverBodyDirection( targetCoverBodyDirection )
, m_isTargetCoverBodyDirectionSet( false )
{
	CRY_ASSERT( m_pPlayer );
	CRY_ASSERT( m_targetCoverBodyDirection != eCoverBodyDirection_Unspecified );
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIChangeCoverBodyDirection::OnInitialise()
{
	if ( m_targetCoverBodyDirection == eCoverBodyDirection_Unspecified )
	{
		ForceFinish();
		return;
	}

	const SMannequinAiCoverBodyDirUserParams* pUserParams = GetMannequinUserParams< SMannequinAiCoverBodyDirUserParams >( *m_context );
	const TCoverBodyDirectionChangeFragment& changeBodyDirFragment = pUserParams->fragments.CoverBodyDirectionChange;

	m_fragmentID = changeBodyDirFragment.fragmentID;

	const TagID coverFragmentTagId = ( m_targetCoverBodyDirection == eCoverBodyDirection_Left ) ? changeBodyDirFragment.fragmentTagIDs.ToCoverLft : changeBodyDirFragment.fragmentTagIDs.ToCoverRgt;

	const bool isCoverBodyDirChangeFragmentSupported = ( m_fragmentID != FRAGMENT_ID_INVALID );
	const bool isTargetCoverBodyDirSupported = ( coverFragmentTagId != TAG_ID_INVALID );
	const bool canDoFancyCoverBodyDirTransition = ( isCoverBodyDirChangeFragmentSupported && isTargetCoverBodyDirSupported );
	if ( ! canDoFancyCoverBodyDirTransition )
	{
		SetPlayerAnimationCoverBodyDirectionOnce();
		ForceFinish();
		return;
	}

	changeBodyDirFragment.pTagDefinition->Set( m_fragTags, coverFragmentTagId, true );
}


void CAnimActionAIChangeCoverBodyDirection::Enter()
{
	TBase::Enter();

	SetPlayerAnimationInProgressCoverBodyDirection( m_targetCoverBodyDirection );

	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	if (!pAnimationComponent->GetUseLegacyCoverLocator())
	{
		SetAnimationControlledMovementParameters();
	}
}

void CAnimActionAIChangeCoverBodyDirection::Exit()
{
	SAnimationContext& context = GetContext();

	SetPlayerAnimationInProgressCoverBodyDirection( eCoverBodyDirection_Unspecified );
	SetPlayerAnimationCoverBodyDirectionOnce();
	SetEntityControlledMovementParameters();

	TBase::Exit();
}

//////////////////////////////////////////////////////////////////////////
void CAnimActionAIChangeCoverBodyDirection::SetPlayerAnimationCoverBodyDirectionOnce()
{
	if (m_isTargetCoverBodyDirectionSet)
		return;

	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();
	animationState.SetCoverBodyDirection( m_targetCoverBodyDirection );

	m_isTargetCoverBodyDirectionSet = true;
}

void CAnimActionAIChangeCoverBodyDirection::SetAnimationControlledMovementParameters()
{
	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	if ( pAnimatedCharacter )
	{
		pAnimatedCharacter->SetMovementControlMethods( eMCM_AnimationHCollision, eMCM_Entity );
	}
}

void CAnimActionAIChangeCoverBodyDirection::SetEntityControlledMovementParameters()
{
	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	if ( pAnimatedCharacter )
	{
		pAnimatedCharacter->SetMovementControlMethods( eMCM_Entity, eMCM_Entity );
	}
}

void CAnimActionAIChangeCoverBodyDirection::SetPlayerAnimationInProgressCoverBodyDirection( const ECoverBodyDirection coverBodyDirection )
{
	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();
	animationState.SetInProgressCoverBodyDirection( coverBodyDirection );
}







//////////////////////////////////////////////////////////////////////////
CAnimActionAICoverAction::CAnimActionAICoverAction( int priority, CPlayer* pPlayer, const char* actionName )
: TBase( priority, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut )
, m_pPlayer( pPlayer )
, m_action( actionName )
, m_toActionCrc( 0 )
, m_fromActionCrc( 0 )
, m_state( eNone )
, m_canceled( false )
{
	CRY_ASSERT( actionName );
	CRY_ASSERT( m_pPlayer );

	{
		CCrc32 c;
		c.AddLowercase("To");
		c.AddLowercase(actionName);
		m_toActionCrc = c.Get();
	}

	{
		CCrc32 c;
		c.AddLowercase("From");
		c.AddLowercase(actionName);
		m_fromActionCrc = c.Get();
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::OnInitialise( )
{
	static const uint32 s_coverActionCrc = CCrc32::ComputeLowercase( "CoverAction" );
	static const uint32 s_coverActionInCrc = CCrc32::ComputeLowercase( "CoverActionIn" );
	static const uint32 s_coverActionOutCrc = CCrc32::ComputeLowercase( "CoverActionOut" );

	const bool actionInitSuccess = m_actionStates[ eAction ].Init( *m_context, s_coverActionCrc, m_action.crc, SStateInfo::eType_Normal );
	const bool transitionInInitSuccess = m_actionStates[ eTransitionIn ].Init( *m_context, s_coverActionInCrc, m_toActionCrc, SStateInfo::eType_Transition );
	const bool transitionOutInitSuccess = m_actionStates[ eTransitionOut ].Init( *m_context, s_coverActionOutCrc, m_fromActionCrc, SStateInfo::eType_Transition );

	if ( ! actionInitSuccess )
	{
		ForceFinish();
		return;
	}

	SetCurrentState( eTransitionIn );
}


//////////////////////////////////////////////////////////////////////////
ActionScopes CAnimActionAICoverAction::FindCoverActionScopeMask( const SAnimationContext& context ) const
{
	static const uint32 s_coverActionCrc = CCrc32::ComputeLowercase( "CoverAction" );

	SStateInfo stateInfo;
	const bool initSuccess = stateInfo.Init( context, s_coverActionCrc, m_action.crc, SStateInfo::eType_Normal );
	if ( ! initSuccess )
	{
		return ACTION_SCOPES_NONE;
	}

	const SFragTagState fragTagState( context.state.GetMask(), stateInfo.m_fragmentTags );
	return context.controllerDef.GetScopeMask( stateInfo.m_fragmentId, fragTagState );
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::SetCurrentState( const EState state )
{
	CRY_ASSERT( 0 <= state );
	CRY_ASSERT( state < eStatesCount );

	const SStateInfo& stateInfo = m_actionStates[ state ];

	const SStateInfo::EType currentStateType = ( m_state < eStatesCount ) ? m_actionStates[ m_state ].m_stateType : SStateInfo::eType_None;
	const SStateInfo::EType nextStateType = stateInfo.m_stateType;

	SetFragment( stateInfo.m_fragmentId, stateInfo.m_fragmentTags );

	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	const EStatus actionStatus = GetStatus();
	if ( actionStatus == Installed )
	{
		switch ( nextStateType )
		{
		case SStateInfo::eType_Normal:
			RequestIdleMotionDetail();
			if (pAnimationComponent->GetUseLegacyCoverLocator())
			{
				SetEntityControlledMovementParameters();
			}
			break;

		case SStateInfo::eType_Transition:
			RequestNothingMotionDetail();
			if (pAnimationComponent->GetUseLegacyCoverLocator())
			{
				SetAnimationControlledMovementParameters();
			}
			break;
		}
	}

	m_state = state;
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::Enter()
{
	TBase::Enter();

	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	{
		CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();

		const char* const actionName = m_action.c_str();
		animationState.SetCoverActionName( actionName );
	}

	if (!pAnimationComponent->GetUseLegacyCoverLocator())
	{
		SetAnimationControlledMovementParameters();
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::Exit()
{
	SetEntityControlledMovementParameters();
	RequestNothingMotionDetail();

	{
		CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
		CRY_ASSERT( pAnimationComponent );

		CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();
		animationState.SetCoverActionName( "" );

		// Note that this is not the perfect implementation.
		// The concept of clearing the cover action request
		// when the action is canceled is a bit flawed.
		// It's called to make sure that if we exit because
		// another anim action is trying to come in, we don't
		// re-install this anim action again.
		animationState.SetRequestClearCoverAction();

		m_state = eNone;
	}

	TBase::Exit();
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::OnSequenceFinished( int layer, uint32 scopeID )
{
	TBase::OnSequenceFinished( layer, scopeID );

	if ( GetRootScope().GetID() != scopeID )
	{
		return;
	}

	if ( layer != 0 )
	{
		return;
	}

	switch ( m_state )
	{
	case eTransitionIn:
		if ( m_canceled )
			SetCurrentState( eTransitionOut );
		else
			SetCurrentState( eAction );
		return;

	case eAction:
		SetCurrentState( eTransitionOut );
		return;

	case eTransitionOut:
		m_state = eNone;
		ForceFinish();
		return;
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::CancelAction()
{
	m_canceled = true;

	EStance requestedStance = m_pPlayer->GetAIAnimationComponent()->GetAnimationState().GetRequestedStance();
	const bool wantsToBeInCover = ( requestedStance == STANCE_LOW_COVER ) || ( requestedStance == STANCE_HIGH_COVER );
	if ( wantsToBeInCover )
	{
		if ( m_state == eAction )
		{
			SetCurrentState( eTransitionOut );
		}
	}
	else
	{
		// Snap out of cover action asap

		m_state = eNone;
		ForceFinish();
	}
}


//////////////////////////////////////////////////////////////////////////
bool CAnimActionAICoverAction::IsTargetActionName( const char* actionName ) const
{
	const uint32 actionNameCrc = CCrc32::ComputeLowercase( actionName );

	const bool namesCrcMatch = ( actionNameCrc == m_action.crc );
	return namesCrcMatch;
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::SetAnimationControlledMovementParameters()
{
	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	if ( pAnimatedCharacter )
	{
		pAnimatedCharacter->SetMovementControlMethods( eMCM_AnimationHCollision, eMCM_Entity );
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::SetEntityControlledMovementParameters()
{
	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	if ( pAnimatedCharacter )
	{
		pAnimatedCharacter->SetMovementControlMethods( eMCM_Entity, eMCM_Entity );
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::RequestIdleMotionDetail()
{
	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	pAnimationComponent->RequestAIMovementDetail( CAnimActionAIDetail::Idle );
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAICoverAction::RequestNothingMotionDetail()
{
	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	pAnimationComponent->RequestAIMovementDetail( CAnimActionAIDetail::None );
}
