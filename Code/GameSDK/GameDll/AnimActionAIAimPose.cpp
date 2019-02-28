// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "AnimActionAIAimPose.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include "PlayerAnimation.h"


//////////////////////////////////////////////////////////////////////////
#define MAN_AIAIMPOSE_FRAGMENTS( x ) \
	x( AimPose )

#define MAN_AIAIMPOSE_TAGS( x )

#define MAN_AIAIMPOSE_TAGGROUPS( x )

#define MAN_AIAIMPOSE_SCOPES( x )

#define MAN_AIAIMPOSE_CONTEXTS( x )

#define MAN_AIAIMPOSE_CHANGEFRAGMENT_FRAGMENT_TAGS( x )

#define MAN_AIAIMPOSE_FRAGMENT_TAGS( x )

MANNEQUIN_USER_PARAMS( SMannequinAiAimPoseUserParams, MAN_AIAIMPOSE_FRAGMENTS, MAN_AIAIMPOSE_TAGS, MAN_AIAIMPOSE_TAGGROUPS, MAN_AIAIMPOSE_SCOPES, MAN_AIAIMPOSE_CONTEXTS, MAN_AIAIMPOSE_FRAGMENT_TAGS );


//////////////////////////////////////////////////////////////////////////
FragmentID CAnimActionAIAimPose::FindFragmentId( const SAnimationContext& context )
{
	const SMannequinAiAimPoseUserParams* pUserParams = GetMannequinUserParams< SMannequinAiAimPoseUserParams >( context );
	CRY_ASSERT( pUserParams != NULL );

	return pUserParams->fragmentIDs.AimPose;
}



//////////////////////////////////////////////////////////////////////////
CAnimActionAIAimPose::CAnimActionAIAimPose()
: TBase( PP_Lowest, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut | IAction::Interruptable )
{
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIAimPose::OnInitialise()
{
	const FragmentID fragmentId = FindFragmentId( *m_context );
	CRY_ASSERT( fragmentId != FRAGMENT_ID_INVALID );
	SetFragment( fragmentId );
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIAimPose::Install()
{
	TBase::Install();

	InitialiseAimPoseBlender();
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAIAimPose::InitialiseAimPoseBlender()
{
	IScope& rootScope = GetRootScope();
	ICharacterInstance* pCharacterInstance = rootScope.GetCharInst();
	CRY_ASSERT( pCharacterInstance );
	if ( ! pCharacterInstance )
	{
		return;
	}

	ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	CRY_ASSERT( pSkeletonPose );

	IAnimationPoseBlenderDir* pPoseBlenderAim = pSkeletonPose->GetIPoseBlenderAim();
	CRY_ASSERT( pPoseBlenderAim );
	if ( ! pPoseBlenderAim )
	{
		return;
	}

	const uint32 aimPoseAnimationLayer = rootScope.GetBaseLayer();
	pPoseBlenderAim->SetLayer( aimPoseAnimationLayer );
}


//////////////////////////////////////////////////////////////////////////
IAction::EStatus CAnimActionAIAimPose::Update( float timePassed )
{
	TBase::Update( timePassed );

	const IScope& rootScope = GetRootScope();
	const bool foundNewBestMatchingFragment = rootScope.IsDifferent( m_fragmentID, m_fragTags );
	if ( foundNewBestMatchingFragment )
	{
		SetFragment( m_fragmentID, m_fragTags );
	}

	return m_eStatus;
}


//////////////////////////////////////////////////////////////////////////
bool CAnimActionAIAimPose::IsSupported( const SAnimationContext& context )
{
	const FragmentID fragmentId = FindFragmentId( context );
	const bool isSupported = ( fragmentId != FRAGMENT_ID_INVALID );
	return isSupported;
}