// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#ifndef GAME_AUDIO_UTILS_H
#define GAME_AUDIO_UTILS_H

#pragma once

#include "Audio/AudioSignalPlayer.h"

class CGameAudioUtils
{
public:
	//typedef Functor3<const bool&, const float&, ISound*> LengthCallback;

	CGameAudioUtils();
	virtual ~CGameAudioUtils();
	void Reset();

	void UnregisterSignal(const CAudioSignalPlayer& signalPlayer);
	//bool GetPlayingSignalLength(const CAudioSignalPlayer signalPlayer, LengthCallback callback, EntityId entityID = 0);
	
	//static ISound* GetSoundFromProxy(tSoundID soundID, EntityId entityID);
	static IEntityAudioComponent* GetEntityAudioProxy( EntityId entityID );

	//ISoundEventListener
	//void OnSoundEvent( ESoundCallbackEvent event, ISound *pSound );
	//~ISoundEventListener

protected:
	/*typedef std::map<tSoundID, LengthCallback> TSoundIDCallback;
	TSoundIDCallback m_soundCallbacksMap;

	void ProcessEvent(ISound* pSound, const bool success, const float length = 0.0f);*/
};
#endif //#ifndef GAME_AUDIO_UTILS_H