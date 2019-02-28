// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef AUDIO_SIGNAL_PLAYER
#define AUDIO_SIGNAL_PLAYER

#include "GameAudio.h"


class CAudioSignalPlayer
{
public:
	CAudioSignalPlayer()
	: m_audioSignalId( INVALID_AUDIOSIGNAL_ID )
	{}

	friend class CGameAudioUtils;
	
	// TODO: only Play should need the entityId param.

	void SetSignal( const char* pName );
	void SetSignalSafe( const char* pName );
	void SetSignal( TAudioSignalID signalID );
	//void Play( EntityId entityID = 0, const char* pParam = NULL, float param = 0, ESoundSemantic semanticOverride = eSoundSemantic_None, int* const pSpecificRandomSoundIndex = NULL );
	void SetPaused( EntityId entityID, const bool paused);
	//void Stop( EntityId entityID = 0, const ESoundStopMode stopMode = ESoundStopMode_EventFade );
	bool IsPlaying( EntityId entityID = 0 ) const;
	void SetVolume( EntityId entityID, float vol );
	void SetParam( EntityId entityID, const char* paramName, float paramValue );
	bool HasValidSignal() const { return m_audioSignalId!=INVALID_AUDIOSIGNAL_ID; }
	void InvalidateSignal() { SetSignal( INVALID_AUDIOSIGNAL_ID ); }
	void SetOffsetPos( EntityId entityID, const Vec3& pos );
	
	static void JustPlay( TAudioSignalID signalID, EntityId entityID = 0, const char* pParam = NULL, float param = 0);
	static void JustPlay( const char* signal, EntityId entityID = 0, const char* pParam = NULL, float param = 0);
	static void JustPlay( TAudioSignalID signalID, const Vec3& pos );
	static void JustPlay( const char* signal, const Vec3& pos );

	void SetCurrentSamplePos( EntityId entityID, float relativePosition );
	float GetCurrentSamplePos( EntityId entityID );

	static float GetSignalLength(TAudioSignalID signalID);
	#if !defined(_RELEASE) || defined(RELEASE_LOGGING)
	static const char* GetSignalName(TAudioSignalID signalId); // for debug purposes
	const char* GetSignalName();  // for debug purposes
	#endif

	TAudioSignalID GetSignalID() const { return m_audioSignalId; };

	void Reset();

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		//pSizer->AddContainer(m_playingSoundIDs);
	}
private:	
	//static tSoundID PlaySound( const string& name, const ESoundSemantic semantic, EntityId entityID, const char* pParam, float param, uint32 flags = FLAG_SOUND_EVENT );
	//static tSoundID PlaySound( const string& name, const ESoundSemantic semantic, const Vec3& pos , uint32 flags = FLAG_SOUND_EVENT );
	//void StopSound( const tSoundID soundID, EntityId entityID, const ESoundStopMode stopMode = ESoundStopMode_EventFade );
	//bool IsSoundLooped( const tSoundID soundID, EntityId entityID );
	//ISound* GetSoundInterface( IEntityAudioComponent* pProxy, tSoundID soundID ) const;	
	static void ExecuteCommands( EntityId entityID, const CGameAudio::CAudioSignal* pAudioSignal );
	
private:
	TAudioSignalID m_audioSignalId;
	//std::vector<tSoundID> m_playingSoundIDs;
};

#endif
