// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>
#include <Mannequin/Serialization.h>

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IFacialAnimation.h>


struct SProceduralClipFacialSequenceParams
	: public IProceduralParams
{
	SProceduralClipFacialSequenceParams()
		: continueAfterExit(false)
		, looping(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(continueAfterExit, "ContinueAfterExit", "Continue After Exit");
		ar(looping, "Looping", "Looping");
		ar(fileName, "Filename", "Filename");
	}

	bool continueAfterExit;
	bool looping;
	TProcClipString fileName;
};

class CProceduralClipFacialSequence
	: public TProceduralClip< SProceduralClipFacialSequenceParams >
{
public:
	CProceduralClipFacialSequence()
		: m_seekTime( 0.f )
		, m_waiting( false )
		, m_loop( false )
		, m_stopAtExit( true )
	{
	}

	virtual void OnEnter( float blendTime, float duration, const SProceduralClipFacialSequenceParams& params )
	{
		m_loop = params.looping;
		const bool continueAfterExit = params.continueAfterExit;

		m_stopAtExit = ( m_loop || ! continueAfterExit );

		IFacialInstance* pFacialInstance = m_charInstance->GetFacialInstance();
		IF_UNLIKELY ( pFacialInstance == NULL )
		{
			return;
		}

		CRY_ASSERT( gEnv->pCharacterManager != NULL );
		IFacialAnimation* pFacialAnimation = gEnv->pCharacterManager->GetIFacialAnimation();
		CRY_ASSERT( pFacialAnimation != NULL );

		m_pCurrentSequence = pFacialAnimation->StartStreamingSequence( params.fileName.c_str() );
		IF_UNLIKELY ( ! m_pCurrentSequence )
		{
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "ProceduralClipFacialSequence: Cannot find sequence '%s'.", params.fileName.c_str() );
			return;
		}
		m_waiting = true;
		m_seekTime = 0.f;

		PlaySequenceIfNotStarted();
	}

	virtual void OnExit( float blendTime )
	{
		IF_UNLIKELY ( ! m_pCurrentSequence )
		{
			return;
		}

		IFacialInstance* pFacialInstance = m_charInstance->GetFacialInstance();
		CRY_ASSERT( pFacialInstance != NULL );

		if ( m_stopAtExit )
		{
			const bool playingFacialSequence = pFacialInstance->IsPlaySequence( m_pCurrentSequence.get(), eFacialSequenceLayer_Mannequin );
			if ( playingFacialSequence )
			{
				pFacialInstance->StopSequence( eFacialSequenceLayer_Mannequin );
			}
		}
		else
		{
			if ( ! m_loop && m_waiting )
			{
				// Try one last time, but silently ignoring if we fail
				PlaySequenceIfNotStarted();
			}
		}
	}

	virtual void Update( float timePassed )
	{
		IF_UNLIKELY ( ! m_pCurrentSequence )
		{
			return;
		}

		m_seekTime += timePassed;
		PlaySequenceIfNotStarted();
	}

private:
	void PlaySequenceIfNotStarted()
	{
		CRY_ASSERT( m_pCurrentSequence.get() != NULL );

		if ( m_waiting )
		{
			const bool sequenceLoaded = m_pCurrentSequence->IsInMemory();
			if ( sequenceLoaded )
			{
				IFacialInstance* pFacialInstance = m_charInstance->GetFacialInstance();
				CRY_ASSERT( pFacialInstance != NULL );

				const bool exclusive = false;
				pFacialInstance->PlaySequence( m_pCurrentSequence.get(), eFacialSequenceLayer_Mannequin, exclusive, m_loop );
				pFacialInstance->SeekSequence( eFacialSequenceLayer_Mannequin, m_seekTime );

				m_waiting = false;
			}
		}
	}

private:
	_smart_ptr< IFacialAnimSequence > m_pCurrentSequence;
	float m_seekTime;
	bool m_waiting;
	bool m_loop;
	bool m_stopAtExit;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipFacialSequence, "FacialSequence");