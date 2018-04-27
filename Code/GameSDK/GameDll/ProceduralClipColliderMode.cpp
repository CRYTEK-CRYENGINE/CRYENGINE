// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>
#include <Mannequin/Serialization.h>

#include "ProceduralContextColliderMode.h"



namespace ColliderModeRemapping
{
	uint32 g_colliderModeCrc[ eColliderMode_COUNT ] = { 0 };

	EColliderMode GetFromCrcParam( const uint32 paramCrc )
	{
		const bool initialized = ( g_colliderModeCrc[ eColliderMode_Undefined ] != 0 );
		if ( ! initialized )
		{
			g_colliderModeCrc[ eColliderMode_Undefined ]         = CCrc32::ComputeLowercase( "Undefined" );
			g_colliderModeCrc[ eColliderMode_Disabled ]          = CCrc32::ComputeLowercase( "Disabled" );
			g_colliderModeCrc[ eColliderMode_GroundedOnly ]      = CCrc32::ComputeLowercase( "GroundedOnly" );
			g_colliderModeCrc[ eColliderMode_Pushable ]          = CCrc32::ComputeLowercase( "Pushable" );
			g_colliderModeCrc[ eColliderMode_NonPushable ]       = CCrc32::ComputeLowercase( "NonPushable" );
			g_colliderModeCrc[ eColliderMode_PushesPlayersOnly ] = CCrc32::ComputeLowercase( "PushesPlayersOnly" );
			g_colliderModeCrc[ eColliderMode_Spectator ]         = CCrc32::ComputeLowercase( "Spectator" );
		}

		for ( size_t i = 0; i < eColliderMode_COUNT; ++i )
		{
			const uint32 colliderModeCrc = g_colliderModeCrc[ i ];
			if ( colliderModeCrc == paramCrc )
			{
				return static_cast< EColliderMode >( i );
			}
		}
		
		return eColliderMode_Undefined;
	}
}

struct SColliderModeParams : IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(mode, "Mode", "Mode");
	}

	SProcDataCRC mode;
};

class CProceduralClipColliderMode
	: public TProceduralContextualClip< CProceduralContextColliderMode, SColliderModeParams>
{
public:
	CProceduralClipColliderMode()
		: m_requestId( 0 )
	{
	}

	virtual void OnEnter( float blendTime, float duration, const SColliderModeParams& params )
	{
		const EColliderMode mode = ColliderModeRemapping::GetFromCrcParam( params.mode.crc );

		m_requestId = m_context->RequestColliderMode( mode );
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


REGISTER_PROCEDURAL_CLIP(CProceduralClipColliderMode, "ColliderMode");
