// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>

#include "ProceduralContextAim.h"

class CProceduralClipAiming
: public TProceduralContextualClip<CProceduralContextAim, SNoProceduralParams>
{
public:
	virtual void OnEnter( float blendTime, float duration, const SNoProceduralParams& params )
	{
		m_context->SetBlendInTime( blendTime );
		m_context->UpdateProcClipAimingRequest( true );
	}

	virtual void OnExit( float blendTime )
	{
		m_context->SetBlendOutTime( blendTime );
		m_context->UpdateProcClipAimingRequest( false );
	}

	virtual void Update( float timePassed )
	{
		m_context->UpdateProcClipAimingRequest( true );
	}
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipAiming, "Aiming");
