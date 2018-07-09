// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 01:06:2009   Created by Federico Rebora
*************************************************************************/

#include "StdAfx.h"
#include "MockEngineSound.h"

namespace GameTesting
{
	CMockEngineSound::CMockEngineSound()
	{
		Reset();
		m_mockSoundSystemWrapper.SetMockEngineSound(this);
	}
	
	CMockEngineSound::~CMockEngineSound()
	{
		for (uint32 i=0; i<m_obtainedSignals.size();++i)
		{
			SAFE_DELETE( m_obtainedSignals[i]);
		}
	}
	

	void CMockEngineSound::Reset()
	{
		m_DummyIDCounter = 0;
		m_CreateAndPlay_EntityId_Param = 0,
		m_CreateAndPlay_Semantic_Param = eSoundSemantic_None;
		m_Stop_SoundID_Param = INVALID_SOUNDID;
		m_Stop_EntityId_Param = 0;
		m_CreateAndPlay_WasCalled = false;
		m_Stop_WasCalled = false;
		m_UpdateSoundMood_FadeTarget_Param = 0;
	}

	const EngineFacade::CAudioSignal* CMockEngineSound::GetAudioSignal( const string& signalName )
	{
		EngineFacade::CAudioSignal* audioSignal = new EngineFacade::CAudioSignal;
		audioSignal->m_signalName = signalName;    // hack/trick: the same name is used in the sound and the signal, as a way to tell which signal has been played
		EngineFacade::CSound sound;
		sound.m_name = signalName;
		audioSignal->m_sounds.push_back( sound );
		m_obtainedSignals.push_back( audioSignal );
		
		return audioSignal;
	}
	
	//.....only the local mock wrapper uses those 2 functions. they are used to catch played audioSignals
	void CMockEngineSound::NotifySoundPlayed( const string& soundName, const ComponentEntityID& entityID )
	{
		for (uint32 i=0; i<m_obtainedSignals.size(); ++i)
		{
			if (m_obtainedSignals[i]->m_signalName==soundName)
			{
				SPlayedSignal playedSignalInfo;
				playedSignalInfo.m_entityID = entityID;
				playedSignalInfo.m_IndSignalInObtainedVector = i;
				m_playedSignals.push_back( playedSignalInfo );
				break;
			}
		}
	}

	void CMockEngineSound::NotifySoundStopped( const string& soundName )
	{
		for (uint32 i=0; i<m_obtainedSignals.size(); ++i)
		{
			if (m_obtainedSignals[i]->m_signalName==soundName)
			{
				m_stoppedSignals.push_back( i );
			}
		}
	}
	
	int CMockEngineSound::GetAmountAudioSignalsPlayed () const
	{
		return m_playedSignals.size();
	}

	int CMockEngineSound::GetAmountAudioSignalsStopped () const
	{
		return m_stoppedSignals.size();
	}
	
	bool CMockEngineSound::HasAnyAudioSignalBeenPlayed () const
	{
		return GetAmountAudioSignalsPlayed()>0;
	}

	bool CMockEngineSound::HasAnyAudioSignalBeenStopped () const
	{
		return GetAmountAudioSignalsStopped()>0;
	}
	
	bool CMockEngineSound::HasAudioSignalBeenPlayed ( const string& audioSignalName ) const
	{
		for (uint32 i=0; i<m_playedSignals.size(); ++i)
		{
			int ind = m_playedSignals[i].m_IndSignalInObtainedVector;
			if ( m_obtainedSignals[ind]->m_signalName==audioSignalName)
				return true;
		}
		return false;
	}

	bool CMockEngineSound::HasAudioSignalBeenStopped ( const string& audioSignalName ) const
	{
		for (uint32 i=0; i<m_stoppedSignals.size(); ++i)
		{
			int ind = m_stoppedSignals[i];
			if ( m_obtainedSignals[ind]->m_signalName==audioSignalName)
				return true;
		}
		return false;
	}
	
	
	string CMockEngineSound::GetLastPlayedSignalName() const
	{
		if (m_playedSignals.size()>0)
			return m_obtainedSignals[ m_playedSignals.back().m_IndSignalInObtainedVector ]->m_signalName;
		else
			return string("");
	}

	const ComponentEntityID& CMockEngineSound::GetLastPlayedSignalEntityID() const
	{
		if (m_playedSignals.size()>0)
			return m_playedSignals.back().m_entityID;
		else
			return ComponentEntityID::Undefined;
	}
	
	EngineFacade::ISoundSystemWrapper* CMockEngineSound::GetWrapper()
	{
		return &m_mockSoundSystemWrapper;
	}
	
	//--------------------------------------------------------

	void CMockEngineSound::CLocalMockSoundSystemWrapper::SetMockEngineSound( CMockEngineSound* mockEngineSound )
	{
		m_mockEngineSound = mockEngineSound;
	}
	
	tSoundID CMockEngineSound::CLocalMockSoundSystemWrapper::Play(const string& soundName, const ESoundSemantic semantic, const ComponentEntityID& entityID)
	{
		m_mockEngineSound->NotifySoundPlayed( soundName, entityID );
		m_playedSounds.push_back( soundName );
		return m_playedSounds.size();
	}
	
	void CMockEngineSound::CLocalMockSoundSystemWrapper::Stop(const tSoundID soundID, const ComponentEntityID& entityID)
	{
		uint32 ind = soundID - 1;
		CRY_ASSERT( ind<m_playedSounds.size() );
		if (ind<m_playedSounds.size())
			m_mockEngineSound->NotifySoundStopped( m_playedSounds[ind] );
	}
	
	
}
