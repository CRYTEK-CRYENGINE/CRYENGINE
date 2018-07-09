// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProceduralContextLook.h"

CRYREGISTER_CLASS( CProceduralContextLook );

//////////////////////////////////////////////////////////////////////////
CProceduralContextLook::CProceduralContextLook()
	: m_pPoseBlenderLook( NULL )
	, m_gameRequestsLooking( true )
	, m_procClipRequestsLooking( false )
	, m_gameLookTarget( 0, 0, 0 )
{
}

//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::Initialise( IEntity& entity, IActionController& actionController )
{
	IProceduralContext::Initialise( entity, actionController );

	InitialisePoseBlenderLook();
	InitialiseGameLookTarget();
}

//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::InitialisePoseBlenderLook()
{
	CRY_ASSERT( m_entity );

	const int slot = 0;
	ICharacterInstance* pCharacterInstance = m_entity->GetCharacter( slot );
	if ( pCharacterInstance == NULL )
	{
		return;
	}

	ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	if ( pSkeletonPose == NULL )
	{
		return;
	}

	m_pPoseBlenderLook = pSkeletonPose->GetIPoseBlenderLook();

	if ( m_pPoseBlenderLook )
	{
		float polarCoordinatesSmoothTimeSeconds = 0.1f;
		float polarCoordinatesMaxYawDegreesPerSecond = 360.f;
		float polarCoordinatesMaxPitchDegreesPerSecond = 360.f;
		float fadeInSeconds = 0.25f;
		float fadeOutSeconds = 0.25f;
		float fadeOutMinDistance = 0.f;

		IScriptTable* pScriptTable = m_entity->GetScriptTable();
		if ( pScriptTable )
		{
			SmartScriptTable pProceduralContextLookTable;
			pScriptTable->GetValue( "ProceduralContextLook", pProceduralContextLookTable );
			if ( pProceduralContextLookTable )
			{
				pProceduralContextLookTable->GetValue( "polarCoordinatesSmoothTimeSeconds", polarCoordinatesSmoothTimeSeconds );
				pProceduralContextLookTable->GetValue( "polarCoordinatesMaxYawDegreesPerSecond", polarCoordinatesMaxYawDegreesPerSecond );
				pProceduralContextLookTable->GetValue( "polarCoordinatesMaxPitchDegreesPerSecond", polarCoordinatesMaxPitchDegreesPerSecond );
				pProceduralContextLookTable->GetValue( "fadeInSeconds", fadeInSeconds );
				pProceduralContextLookTable->GetValue( "fadeOutSeconds", fadeOutSeconds );
				pProceduralContextLookTable->GetValue( "fadeOutMinDistance", fadeOutMinDistance );
			}
		}

		m_pPoseBlenderLook->SetPolarCoordinatesSmoothTimeSeconds( polarCoordinatesSmoothTimeSeconds );
		m_pPoseBlenderLook->SetPolarCoordinatesMaxRadiansPerSecond( Vec2( DEG2RAD( polarCoordinatesMaxYawDegreesPerSecond ), DEG2RAD( polarCoordinatesMaxPitchDegreesPerSecond ) ) );
		m_pPoseBlenderLook->SetFadeInSpeed( fadeInSeconds );
		m_pPoseBlenderLook->SetFadeOutSpeed( fadeOutSeconds );
		m_pPoseBlenderLook->SetFadeOutMinDistance( fadeOutMinDistance );
		m_pPoseBlenderLook->SetState( false );
	}
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::InitialiseGameLookTarget()
{
	CRY_ASSERT( m_entity );

	m_gameLookTarget = ( m_entity->GetForwardDir() * 10.0f ) + m_entity->GetWorldPos();
	if ( m_pPoseBlenderLook )
	{
		m_pPoseBlenderLook->SetTarget( m_gameLookTarget );
	}
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::Update( float timePassedSeconds )
{
	if ( m_pPoseBlenderLook == NULL )
	{
		return;
	}

	const bool canLook = ( m_gameRequestsLooking && m_procClipRequestsLooking );
	m_pPoseBlenderLook->SetState( canLook );
	if ( canLook )
	{
		m_pPoseBlenderLook->SetTarget( m_gameLookTarget );
	}
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::UpdateGameLookingRequest( const bool lookRequest )
{
	m_gameRequestsLooking = lookRequest;
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::UpdateProcClipLookingRequest( const bool lookRequest )
{
	m_procClipRequestsLooking = lookRequest;
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::UpdateGameLookTarget( const Vec3& lookTarget )
{
	m_gameLookTarget = lookTarget;
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::SetBlendInTime( const float blendInTime )
{
	if ( m_pPoseBlenderLook == NULL )
	{
		return;
	}

	m_pPoseBlenderLook->SetFadeInSpeed( blendInTime );
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::SetBlendOutTime( const float blendOutTime )
{
	if ( m_pPoseBlenderLook == NULL )
	{
		return;
	}

	m_pPoseBlenderLook->SetFadeOutSpeed( blendOutTime );
}


//////////////////////////////////////////////////////////////////////////
void CProceduralContextLook::SetFovRadians( const float fovRadians )
{
	if ( m_pPoseBlenderLook == NULL )
	{
		return;
	}

	m_pPoseBlenderLook->SetFadeoutAngle( fovRadians );
}