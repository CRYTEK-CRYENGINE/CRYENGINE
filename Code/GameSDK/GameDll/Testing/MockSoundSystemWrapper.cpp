// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "MockSoundSystemWrapper.h"

namespace GameTesting
{
	using namespace EngineFacade;



	CMockSoundSystemWrapper::CMockSoundSystemWrapper()
	{
		
	}


	tSoundID CMockSoundSystemWrapper::Play( const string& soundName, const ESoundSemantic semantic, const ComponentEntityID& entityID )
	{
		SPlayParameters parameters;
		parameters.soundName = soundName;
		parameters.semantic = semantic;
		parameters.entityID = entityID;

		m_playCalls.push_back(parameters);

		if (m_soundIDsToReturnOnNextPlay.empty())
		{
			return INVALID_SOUNDID;
		}

		const tSoundID idToReturn = m_soundIDsToReturnOnNextPlay.front();
		m_soundIDsToReturnOnNextPlay.erase(m_soundIDsToReturnOnNextPlay.begin());
		return idToReturn;
	}

	void CMockSoundSystemWrapper::Stop( const tSoundID soundID, const ComponentEntityID& entityID )
	{
		SStopParameters parameters;
		parameters.soundID = soundID;
		parameters.entityID = entityID;
		m_stopCalls.push_back(parameters);
	}

	bool CMockSoundSystemWrapper::WasSoundPlayedOnceWith( const string& soundName ) const
	{
		uint32 numCalls = 0;

		for (unsigned int index = 0; index < m_playCalls.size(); ++index)
		{
			if (soundName == m_playCalls[index].soundName)
			{
				++numCalls;
			}
		}

		return (numCalls == 1);
	}

	bool CMockSoundSystemWrapper::WasSoundStoppedOnceWith( const tSoundID soundID ) const
	{
		uint32 numCalls = 0;

		for (unsigned int index = 0; index < m_stopCalls.size(); ++index)
		{
			if (soundID == m_stopCalls[index].soundID)
			{
				++numCalls;
			}
		}

		return (numCalls == 1);
	}

	bool CMockSoundSystemWrapper::WasSoundStoppedOnceWith( const tSoundID soundID, const ComponentEntityID& entityID ) const
	{
		uint32 numCalls = 0;

		for (unsigned int index = 0; index < m_stopCalls.size(); ++index)
		{
			if (soundID != m_stopCalls[index].soundID)
			{
				continue;
			}

			if (entityID != m_stopCalls[index].entityID)
			{
				continue;
			}

			++numCalls;
		}

		return (numCalls == 1);
	}

	bool CMockSoundSystemWrapper::WasSoundPlayedOnceWithSpecificSemantic( const string& soundName, const ESoundSemantic semantic ) const
	{
		uint32 numCalls = 0;

		for (unsigned int index = 0; index < m_playCalls.size(); ++index)
		{
			if ((soundName == m_playCalls[index].soundName) && (semantic == m_playCalls[index].semantic))
			{
				++numCalls;
			}
		}

		return (numCalls == 1);
	}

	bool CMockSoundSystemWrapper::WasSoundPlayedOnceWithSpecificEntityID( const string& soundName, const ComponentEntityID& entityID ) const
	{
		uint32 numCalls = 0;

		for (unsigned int index = 0; index < m_playCalls.size(); ++index)
		{
			if ((soundName == m_playCalls[index].soundName) && (entityID == m_playCalls[index].entityID))
			{
				++numCalls;
			}
		}

		return (numCalls == 1);
	}

	void CMockSoundSystemWrapper::AddSoundMood( const string& name, const uint32 fadeInTimeMSecs )
	{
		SAddRemoveMoodParameters parameters(name, fadeInTimeMSecs);
		m_addSoundMoodCalls.push_back(parameters);
	}

	void CMockSoundSystemWrapper::RemoveSoundMood( const string& name, const uint32 fadeOutTimeMSecs )
	{
		SAddRemoveMoodParameters parameters(name, fadeOutTimeMSecs);
		m_removeSoundMoodCalls.push_back(parameters);
	}

	bool CMockSoundSystemWrapper::WasAddSoundMoodCalledOnceWith( const string& name, const uint32 fadeInTimeMSecs ) const
	{
		SAddRemoveMoodParameters callToCheck(name, fadeInTimeMSecs);

		uint32 numCalls = 0;

		for (uint32 index = 0; index <  m_addSoundMoodCalls.size(); ++index)
		{
			if (callToCheck == m_addSoundMoodCalls[index])
			{
				++numCalls;
			}
		}

		return (numCalls == 1);
	}

	bool CMockSoundSystemWrapper::WasRemoveSoundMoodCalledOnceWith( const string& name, const uint32 fadeOutTimeMSecs ) const
	{
		SAddRemoveMoodParameters callToCheck(name, fadeOutTimeMSecs);

		uint32 numCalls = 0;

		for (uint32 index = 0; index < m_removeSoundMoodCalls.size(); ++index)
		{
			if (m_removeSoundMoodCalls[index] == callToCheck)
			{
				++numCalls;
			}
		}

		return (numCalls == 1);
	}

	bool CMockSoundSystemWrapper::WasAddSoundMoodCalled() const
	{
		return m_addSoundMoodCalls.size() != 0;
	}

	bool CMockSoundSystemWrapper::WasRemoveSoundMoodCalled() const
	{
		return m_removeSoundMoodCalls.size() != 0;
	}

	void CMockSoundSystemWrapper::AddReturnedSoundIDAfterPlay( const tSoundID soundID )
	{
		m_soundIDsToReturnOnNextPlay.push_back(soundID);
	}
}
