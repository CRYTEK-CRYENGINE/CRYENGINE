// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Implements the Blend From Ragdoll AnimAction

-------------------------------------------------------------------------
History:
- 06.07.12: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "AnimActionBlendFromRagdoll.h"
#include "Actor.h"

#include <CryExtension/CryCreateClassInstance.h>

#include <IGameObject.h>

//////////////////////////////////////////////////////////////////////////
#define MAN_BLENDRAGDOLL_FRAGMENTS( x ) \
	x( BlendRagdoll )


#define MAN_BLENDRAGDOLL_TAGS( x )

#define MAN_BLENDRAGDOLL_TAGGROUPS( x )

#define MAN_BLENDRAGDOLL_SCOPES( x )

#define MAN_BLENDRAGDOLL_CONTEXTS( x )

#define MAN_BLENDRAGDOLL_FRAGMENT_TAGS( x )

MANNEQUIN_USER_PARAMS( SMannequinFallAndPlayParams, MAN_BLENDRAGDOLL_FRAGMENTS, MAN_BLENDRAGDOLL_TAGS, MAN_BLENDRAGDOLL_TAGGROUPS, MAN_BLENDRAGDOLL_SCOPES, MAN_BLENDRAGDOLL_CONTEXTS, MAN_BLENDRAGDOLL_FRAGMENT_TAGS );
//////////////////////////////////////////////////////////////////////////

CAnimActionBlendFromRagdoll::CAnimActionBlendFromRagdoll( int priority, CActor& actor, const FragmentID& fragID, const TagState fragTags )
	: TBase( priority, fragID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut )
	, m_actor(actor)
	, m_fragTagsTarget(fragTags)
	, m_bSetAnimationFrag(false)
	, m_animID(-1)
{
}

void CAnimActionBlendFromRagdoll::OnInitialise()
{
	::CryCreateClassInstanceForInterface<IAnimationPoseMatching>(cryiidof<IAnimationPoseMatching>(), m_pPoseMatching);
}

void CAnimActionBlendFromRagdoll::Enter()
{
	TBase::Enter();

#ifdef USE_BLEND_FROM_RAGDOLL
	QueryPose();
#endif
}

void CAnimActionBlendFromRagdoll::Exit()
{
	m_actor.GetActorStats()->isInBlendRagdoll = false;

	TBase::Exit();
}

IAction::EStatus CAnimActionBlendFromRagdoll::Update(float timePassed)
{
#ifdef USE_BLEND_FROM_RAGDOLL
	if( !m_animIds.empty() && !m_bSetAnimationFrag && m_pPoseMatching->GetMatchingAnimation(m_animID) )
#endif
	{
		uint optionIdx = 0;
		for( ; optionIdx<m_animIds.size(); ++optionIdx )
		{
			if( m_animID == m_animIds[optionIdx] )
			{
				break;
			}
		}
		SetFragment( m_fragmentID, m_fragTagsTarget, optionIdx );

		m_bSetAnimationFrag = true;
	}

	return TBase::Update( timePassed );
}

void CAnimActionBlendFromRagdoll::OnFragmentStarted()
{
#ifdef USE_BLEND_FROM_RAGDOLL
	if( m_bSetAnimationFrag )
	{
		SGameObjectEvent event( eGFE_QueueBlendFromRagdoll, eGOEF_ToExtensions );
		event.paramAsBool = true;
		m_actor.GetGameObject()->SendEvent( event );
		m_flags &= ~IAction::NoAutoBlendOut;
	}
#else
	m_flags &= ~IAction::NoAutoBlendOut;
#endif
}

void CAnimActionBlendFromRagdoll::DispatchPoseModifier()
{
	if( !m_animIds.empty() )
	{
		m_pPoseMatching->SetAnimations( &m_animIds[0], m_animIds.size() );

		m_rootScope->GetCharInst()->GetISkeletonAnim()->PushPoseModifier( -1, m_pPoseMatching, "AnimationPoseModifier_PoseMatching" );
	}
}

void CAnimActionBlendFromRagdoll::QueryPose()
{
	GenerateAnimIDs();

	DispatchPoseModifier();
}

void CAnimActionBlendFromRagdoll::GenerateAnimIDs()
{
	SFragTagState fragTagStateMatch;
	uint32 tagSetIdx;
	SFragTagState fragTagStateQuery( m_rootScope->GetContext().state.GetMask(), m_fragTagsTarget );
	SFragmentQuery fragQuery(m_fragmentID, fragTagStateQuery);
	int32 numOptions = m_rootScope->GetDatabase().FindBestMatchingTag(fragQuery, &fragTagStateMatch, &tagSetIdx);

	IAnimationSet* piAnimSet = m_rootScope->GetEntity().GetCharacter(0)->GetIAnimationSet();

	m_animIds.reserve( numOptions );
	for (int32 i=0; i<numOptions; i++)
	{
		const CFragment *fragment = m_rootScope->GetDatabase().GetEntry(m_fragmentID, tagSetIdx, i);

		if( !fragment->m_animLayers.empty() && !fragment->m_animLayers[0].empty() )
		{
			const SAnimClip& animClip = fragment->m_animLayers[0][0];
			const uint animID = piAnimSet->GetAnimIDByCRC(animClip.animation.animRef.crc);
			m_animIds.push_back( animID );
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CAnimActionBlendFromRagdollSleep::CAnimActionBlendFromRagdollSleep( int priority, CActor& actor, const HitInfo& hitInfo, const TagState& sleepTagState, const TagState& fragTags )
	: TBase( priority, FRAGMENT_ID_INVALID, sleepTagState )
	, m_actor(actor)
	, m_hitInfo(hitInfo)
	, m_fragTagsTarget(fragTags)
{
}

void CAnimActionBlendFromRagdollSleep::OnInitialise()
{
	CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();

	const SMannequinFallAndPlayParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinFallAndPlayParams>(m_context->controllerDef);
	CRY_ASSERT(pParams);

	m_fragmentID = pParams->fragmentIDs.BlendRagdoll;
}

void CAnimActionBlendFromRagdollSleep::Enter()
{
	TBase::Enter();

	SGameObjectEvent event( eGFE_EnableBlendRagdoll, eGOEF_ToExtensions );
	event.ptr = &m_hitInfo;
	m_actor.GetGameObject()->SendEvent( event );

	#ifdef USE_BLEND_FROM_RAGDOLL
		m_rootScope->GetActionController().Queue( *new CAnimActionBlendFromRagdoll(GetPriority(), m_actor, m_fragmentID, m_fragTagsTarget) );
	#endif //USE_BLEND_FROM_RAGDOLL
}
