// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "Player.h"
#include "PlayerAnimation.h" // needed for Action priorities only

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

MANNEQUIN_USER_PARAMS( SMannequinAIDetailParams, MAN_AIDETAIL_FRAGMENTS, MAN_AIDETAIL_TAGS, MAN_AIDETAIL_TAGGROUPS, MAN_AIDETAIL_SCOPES, MAN_AIDETAIL_CONTEXTS, MAN_AIDETAIL_FRAGMENT_TAGS );



CAnimActionAIDetail::CAnimActionAIDetail()
	: TBase(PP_Lowest, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut|IAction::Interruptable)
	, m_pManParams(NULL)
	, m_requestedDetail(FRAGMENT_ID_INVALID)
{
}

void CAnimActionAIDetail::OnInitialise()
{
	CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();

	m_pManParams = mannequinUserParams.FindOrCreateParams<SMannequinAIDetailParams>(m_context->controllerDef);
	CRY_ASSERT(m_pManParams);

	// cannot call 	UpdateFragmentVariation(true /* force update */); yet because rootscope is NULL

	if ((m_fragmentID == FRAGMENT_ID_INVALID) || (m_requestedDetail == FRAGMENT_ID_INVALID))
	{
		m_fragmentID = m_requestedDetail = m_pManParams->fragmentIDs.MotionDetail_Nothing; // have to return something sensible in order for the rootscope to be filled & for UpdatePending to be called
	}
}

void CAnimActionAIDetail::Enter()
{
	TBase::Enter();
}

void CAnimActionAIDetail::Exit() 
{
	TBase::Exit();
}

IAction::EStatus CAnimActionAIDetail::UpdatePending(float timePassed)
{
	EStatus status = TBase::UpdatePending(timePassed);

	UpdateFragmentVariation(true /* force update */);

	return status;
}

void CAnimActionAIDetail::UpdateFragmentVariation(bool forceUpdate)
{
	mannequin::UpdateFragmentVariation(this, &m_fragmentVariationHelper, m_requestedDetail, GetFragTagState(), forceUpdate);
}

void CAnimActionAIDetail::OnSequenceFinished(int layer, uint32 scopeID)
{
	TBase::OnSequenceFinished(layer, scopeID);

	if (GetRootScope().GetID() == scopeID && layer == 0)
	{
		m_fragmentVariationHelper.OnFragmentEnd();
	}
}

IAction::EStatus CAnimActionAIDetail::Update(float timePassed)
{
	EStatus ret = TBase::Update(timePassed);

	UpdateFragmentVariation(false /* no force update*/);

	return ret;
}

void CAnimActionAIDetail::RequestDetail(EMovementDetail motionDetail)
{
	const SMannequinAIDetailParams::FragmentIDs& fragmentIDs = m_pManParams->fragmentIDs;
	switch(motionDetail)
	{
	case Move:
		m_requestedDetail = fragmentIDs.MotionDetail_Move;
		break;

	case Idle:
		m_requestedDetail = fragmentIDs.MotionDetail_Idle;
		break;

	case Turn:
		if (fragmentIDs.MotionDetail_IdleTurn.IsValid())
		{
			m_requestedDetail = fragmentIDs.MotionDetail_IdleTurn;
		}
		else if (m_requestedDetail == fragmentIDs.MotionDetail_Nothing)
		{
			m_requestedDetail = fragmentIDs.MotionDetail_Idle;
		}
		else
		{
			// Keep current m_requestedDetail.
		}
		break;

	default:
		m_requestedDetail = fragmentIDs.MotionDetail_Nothing;
		break;
	}
}

bool CAnimActionAIDetail::IsSupported(const SAnimationContext& context)
{
	const SMannequinAIDetailParams* pUserParams = GetMannequinUserParams<SMannequinAIDetailParams>(context);
	CRY_ASSERT(pUserParams);

	return 
		pUserParams->fragmentIDs.MotionDetail_Idle.IsValid() ||
		pUserParams->fragmentIDs.MotionDetail_IdleTurn.IsValid() ||
		pUserParams->fragmentIDs.MotionDetail_Move.IsValid() ||
		pUserParams->fragmentIDs.MotionDetail_Nothing.IsValid();
}
