// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>
#include <Mannequin/Serialization.h>

#include "ProceduralContextMovementControlMethod.h"




struct SProceduralClipMovementControlMethodParams
	: public IProceduralParams
{
	SProceduralClipMovementControlMethodParams() 
		: horizontal(0.0f)
		, vertical(0.0f)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(horizontal, "Horizontal", "Horizontal");
		ar(vertical, "Vertical", "Vertical");
	}

	float horizontal;
	float vertical;
};


namespace MovementControlMethodRemapping
{
	enum { SupportedMCM_Count = 4 };

	const EMovementControlMethod g_remappingTable[ SupportedMCM_Count ] =
	{
		eMCM_Undefined,
		eMCM_Entity,
		eMCM_Animation,
		eMCM_AnimationHCollision,
	};

	EMovementControlMethod GetFromFloatParam( const float param )
	{
		const int index = static_cast< int >( param );
		const EMovementControlMethod supportedMcm = ( 0 <= index && index < SupportedMCM_Count ) ? g_remappingTable[ index ] : eMCM_Undefined;
		return supportedMcm;
	}
}


class CProceduralClipMovementControlMethod
	: public TProceduralContextualClip< CProceduralContextMovementControlMethod, SProceduralClipMovementControlMethodParams >
{
public:
	CProceduralClipMovementControlMethod()
		: m_requestId( 0 )
	{
	}

	virtual void OnEnter( float blendTime, float duration, const SProceduralClipMovementControlMethodParams& params )
	{
		const EMovementControlMethod horizontal = MovementControlMethodRemapping::GetFromFloatParam( params.horizontal );

		const EMovementControlMethod vertical = MovementControlMethodRemapping::GetFromFloatParam( params.vertical );
		const EMovementControlMethod validVertical = ( vertical == eMCM_AnimationHCollision ) ? eMCM_Animation : vertical;

		m_requestId = m_context->RequestMovementControlMethod( horizontal, validVertical );
	}

	virtual void OnExit( float blendTime )
	{
		m_context->CancelRequest( m_requestId );
	}

	virtual void Update( float timePassed )
	{
	}

private:
	uint32 m_requestId;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipMovementControlMethod, "MovementControlMethod");