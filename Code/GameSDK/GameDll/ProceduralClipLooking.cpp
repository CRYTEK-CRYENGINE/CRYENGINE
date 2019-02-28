// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>

#include "ProceduralContextLook.h"

class CProceduralClipLooking
	: public TProceduralContextualClip< CProceduralContextLook, SNoProceduralParams >
{
public:
		virtual void OnEnter( float blendTime, float duration, const SNoProceduralParams& params )
	{
		m_context->SetBlendInTime( blendTime );
		m_context->UpdateProcClipLookingRequest( true );
	}

	virtual void OnExit( float blendTime )
	{
		m_context->SetBlendOutTime( blendTime );
		m_context->UpdateProcClipLookingRequest( false );
	}

	virtual void Update( float timePassed )
	{
		m_context->UpdateProcClipLookingRequest( true );
	}
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipLooking, "Looking");
