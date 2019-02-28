// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimActionAIStance.h"
#include "Player.h"



//////////////////////////////////////////////////////////////////////////
#define MAN_AISTANCE_FRAGMENTS( x )

#define MAN_AISTANCE_TAGS( x )

#define MAN_AISTANCE_TAGGROUPS( x )

#define MAN_AISTANCE_SCOPES( x ) \
	x( FullBody3P )

#define MAN_AISTANCE_CONTEXTS( x )

#define MAN_AISTANCE_CHANGEFRAGMENT_FRAGMENT_TAGS( x ) \
	x( ToCoverHigh ) \
	x( ToCoverLow ) \
	x( ToRelaxed ) \
	x( ToAlerted ) \
	x( ToStand ) \
	x( ToSwim ) \
	x( ToCrouch )

#define MAN_AISTANCE_FRAGMENT_TAGS( x ) \
	x( CODE_AI_ChangeStance, MAN_AISTANCE_CHANGEFRAGMENT_FRAGMENT_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST )


MANNEQUIN_USER_PARAMS( SMannequinAiStanceUserParams, MAN_AISTANCE_FRAGMENTS, MAN_AISTANCE_TAGS, MAN_AISTANCE_TAGGROUPS, MAN_AISTANCE_SCOPES, MAN_AISTANCE_CONTEXTS, MAN_AISTANCE_FRAGMENT_TAGS );


//////////////////////////////////////////////////////////////////////////
typedef SMannequinAiStanceUserParams::Fragments::SCODE_AI_ChangeStance TChangeStanceFragment;


//////////////////////////////////////////////////////////////////////////
namespace
{
	ILINE TagID GetFragmentTagIdFromStance( const TChangeStanceFragment& changeStanceFragment, const EStance stance )
	{
		switch ( stance )
		{
		case STANCE_RELAXED:
			return changeStanceFragment.fragmentTagIDs.ToRelaxed;
		case STANCE_ALERTED:
			return changeStanceFragment.fragmentTagIDs.ToAlerted;
		case STANCE_CROUCH:
			return changeStanceFragment.fragmentTagIDs.ToCrouch;
		case STANCE_SWIM:
			return changeStanceFragment.fragmentTagIDs.ToSwim;
		case STANCE_STAND:
			return changeStanceFragment.fragmentTagIDs.ToStand;
		case STANCE_HIGH_COVER:
			return changeStanceFragment.fragmentTagIDs.ToCoverHigh;
		case STANCE_LOW_COVER:
			return changeStanceFragment.fragmentTagIDs.ToCoverLow;
		}
		return TAG_ID_INVALID;
	}
}


//////////////////////////////////////////////////////////////////////////
CAnimActionAIStance::CAnimActionAIStance( int priority, CPlayer* pPlayer, const EStance targetStance )
: TBase( priority, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY )
, m_pPlayer( pPlayer )
, m_targetStance( targetStance )
, m_isPlayerAnimationStanceSet( false )
{
	CRY_ASSERT( m_pPlayer );
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIStance::Enter()
{
	TBase::Enter();

	SetMovementParameters();
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIStance::Exit()
{
	RestoreMovementParameters();

	SetPlayerAnimationStanceOnce();

	TBase::Exit();
}


//////////////////////////////////////////////////////////////////////////
IAction::EStatus CAnimActionAIStance::Update( float elapsedSeconds )
{
	TBase::Update( elapsedSeconds );

	const IScope& rootScope = GetRootScope();

	const float fragmentDuration = rootScope.GetFragmentDuration();
	const float fragmentTime = rootScope.GetFragmentTime();
	const float fragmentNormalisedTime = fragmentDuration ? ( fragmentTime / fragmentDuration ) : 0.0f;

	if ( 0.7f < fragmentNormalisedTime )
	{
		SetPlayerAnimationStanceOnce();
		ForceFinish();
	}

	return m_eStatus;
}


//////////////////////////////////////////////////////////////////////////
IAction::EStatus CAnimActionAIStance::UpdatePending( float timePassed )
{
	TBase::UpdatePending(timePassed);

	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();
	
	if ( m_targetStance != animationState.GetRequestedStance() )
		ForceFinish();

	if ( m_targetStance == animationState.GetStance() )
		ForceFinish();

	return m_eStatus;
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIStance::OnInitialise()
{
	SFragTagState fragTagState;
	const bool fragmentInfoFound = FindFragmentInfo( *m_context, m_fragmentID, fragTagState );
	if ( ! fragmentInfoFound )
	{
		SetPlayerAnimationStanceOnce();
		ForceFinish();
		return;
	}

	m_fragTags = fragTagState.fragmentTags;
}


//////////////////////////////////////////////////////////////////////////
bool CAnimActionAIStance::FindFragmentInfo( const SAnimationContext& context, FragmentID& fragmentIdOut, SFragTagState& fragTagStateOut ) const
{
	const SMannequinAiStanceUserParams* pUserParams = GetMannequinUserParams< SMannequinAiStanceUserParams >( context );
	const TChangeStanceFragment& changeStanceFragment = pUserParams->fragments.CODE_AI_ChangeStance;

	const FragmentID fragmentId = changeStanceFragment.fragmentID;
	if ( fragmentId == FRAGMENT_ID_INVALID )
	{
		return false;
	}

	const TagID stanceFragmentTagId = GetFragmentTagIdFromStance( changeStanceFragment, m_targetStance );
	if ( stanceFragmentTagId == TAG_ID_INVALID )
	{
		return false;
	}

	fragmentIdOut = fragmentId;

	TagState tagState = TAG_STATE_EMPTY;
	changeStanceFragment.pTagDefinition->Set( tagState, stanceFragmentTagId, true );
	fragTagStateOut = SFragTagState( context.state.GetMask(), tagState );

	return true;
}


//////////////////////////////////////////////////////////////////////////
ActionScopes CAnimActionAIStance::FindStanceActionScopeMask( const SAnimationContext& context ) const
{
	FragmentID fragmentId = FRAGMENT_ID_INVALID;
	SFragTagState fragTagState;
	const bool fragmentInfoFound = FindFragmentInfo( context, fragmentId, fragTagState );
	if ( ! fragmentInfoFound )
	{
		return ACTION_SCOPES_NONE;
	}

	return context.controllerDef.GetScopeMask( fragmentId, fragTagState );
}


//////////////////////////////////////////////////////////////////////////
bool CAnimActionAIStance::FragmentExistsInDatabase( const SAnimationContext& context, const IAnimationDatabase& database ) const
{
	FragmentID fragmentId = FRAGMENT_ID_INVALID;
	SFragTagState fragTagState;
	const bool fragmentInfoFound = FindFragmentInfo( context, fragmentId, fragTagState );
	if ( ! fragmentInfoFound )
	{
		return false;
	}

	SFragmentQuery fragQuery(fragmentId, fragTagState);
	const uint32 optionCount = database.FindBestMatchingTag( fragQuery );
	return ( 0 < optionCount );
}

//////////////////////////////////////////////////////////////////////////
void CAnimActionAIStance::SetMovementParameters()
{
	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	if ( pAnimatedCharacter )
	{
		CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
		CRY_ASSERT( pAnimationComponent );
		if (pAnimationComponent->GetUseLegacyCoverLocator())
		{
			const bool toCoverStance = ( m_targetStance == STANCE_HIGH_COVER || m_targetStance == STANCE_LOW_COVER );
			if ( toCoverStance )
			{
				pAnimatedCharacter->SetMovementControlMethods( eMCM_Entity, eMCM_Entity );
				pAnimatedCharacter->UseAnimationMovementForEntity( true, true, false );
			}
			else
			{
				pAnimatedCharacter->SetMovementControlMethods( eMCM_AnimationHCollision, eMCM_Entity );
			}
		}
		else
		{
			pAnimatedCharacter->SetMovementControlMethods( eMCM_AnimationHCollision, eMCM_Entity );
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIStance::RestoreMovementParameters()
{
	// We are currently not storing the parameters from SetMovementParameters, but going back to hopefully reasonable defaults instead.
	IAnimatedCharacter* pAnimatedCharacter = m_pPlayer->GetAnimatedCharacter();
	if ( pAnimatedCharacter )
	{
		pAnimatedCharacter->SetMovementControlMethods( eMCM_Entity, eMCM_Entity );

		CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
		CRY_ASSERT( pAnimationComponent );
		if (pAnimationComponent->GetUseLegacyCoverLocator())
		{
			pAnimatedCharacter->UseAnimationMovementForEntity( false, false, false );
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIStance::SetPlayerAnimationStanceOnce()
{
	// Only set once
	if ( m_isPlayerAnimationStanceSet )
		return;

	CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();
	animationState.SetStance( m_targetStance );

	m_isPlayerAnimationStanceSet = true;
}


//////////////////////////////////////////////////////////////////////////
EStance CAnimActionAIStance::GetPlayerAnimationStance() const
{
	const CAIAnimationComponent* pAnimationComponent = m_pPlayer->GetAIAnimationComponent();
	CRY_ASSERT( pAnimationComponent );

	const CAIAnimationState& animationState = pAnimationComponent->GetAnimationState();
	const EStance stance = animationState.GetStance();
	return stance;
}
