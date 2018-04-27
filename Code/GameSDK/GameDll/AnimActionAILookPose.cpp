// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "AnimActionAILookPose.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include "PlayerAnimation.h"


//////////////////////////////////////////////////////////////////////////
#define MAN_AILOOKPOSE_FRAGMENTS( x ) \
	x( LookPose )

#define MAN_AILOOKPOSE_TAGS( x )

#define MAN_AILOOKPOSE_TAGGROUPS( x )

#define MAN_AILOOKPOSE_SCOPES( x )

#define MAN_AILOOKPOSE_CONTEXTS( x )

#define MAN_AILOOKPOSE_CHANGEFRAGMENT_FRAGMENT_TAGS( x )

#define MAN_AILOOKPOSE_FRAGMENT_TAGS( x )

MANNEQUIN_USER_PARAMS( SMannequinAiLookPoseUserParams, MAN_AILOOKPOSE_FRAGMENTS, MAN_AILOOKPOSE_TAGS, MAN_AILOOKPOSE_TAGGROUPS, MAN_AILOOKPOSE_SCOPES, MAN_AILOOKPOSE_CONTEXTS, MAN_AILOOKPOSE_FRAGMENT_TAGS );


//////////////////////////////////////////////////////////////////////////
namespace
{
	FragmentID FindFragmentId( const SAnimationContext& context )
	{
		const SMannequinAiLookPoseUserParams* pUserParams = GetMannequinUserParams< SMannequinAiLookPoseUserParams >( context );
		CRY_ASSERT( pUserParams != NULL );

		return pUserParams->fragmentIDs.LookPose;
	}
}



//////////////////////////////////////////////////////////////////////////
CAnimActionAILookPose::CAnimActionAILookPose()
	: TBase( PP_Lowest, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut | IAction::Interruptable )
{
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAILookPose::OnInitialise()
{
	const FragmentID fragmentId = FindFragmentId( *m_context );
	CRY_ASSERT( fragmentId != FRAGMENT_ID_INVALID );
	SetFragment( fragmentId );
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAILookPose::Install()
{
	TBase::Install();

	InitialiseLookPoseBlender();
}


//////////////////////////////////////////////////////////////////////////
void CAnimActionAILookPose::InitialiseLookPoseBlender()
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

	IAnimationPoseBlenderDir* pPoseBlenderLook = pSkeletonPose->GetIPoseBlenderLook();
	CRY_ASSERT( pPoseBlenderLook );
	if ( ! pPoseBlenderLook )
	{
		return;
	}

	const uint32 lookPoseAnimationLayer = rootScope.GetBaseLayer();
	pPoseBlenderLook->SetLayer( lookPoseAnimationLayer );
}


//////////////////////////////////////////////////////////////////////////
IAction::EStatus CAnimActionAILookPose::Update( float timePassed )
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
bool CAnimActionAILookPose::IsSupported( const SAnimationContext& context )
{
	const FragmentID fragmentId = FindFragmentId( context );
	const bool isSupported = ( fragmentId != FRAGMENT_ID_INVALID );
	return isSupported;
}