// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioSignalPlayer.h"
#include "Game.h"
#include "GameAudioUtils.h"

//////////////////////////////////////////////////////////////////////////

//void CAudioSignalPlayer::Play( EntityId entityID, const char* pParam, float param, ESoundSemantic semanticOverride, int* const pSpecificRandomSoundIndex )
//{
//	const CGameAudio::CAudioSignal* pAudioSignal = g_pGame->GetGameAudio()->GetAudioSignal( m_audioSignalId );
//		
//	if (!pAudioSignal)
//		return;
//
//	//  stops looped sounds
//	size_t numPlayingSounds = m_playingSoundIDs.size();
//	for (size_t i = 0; i < numPlayingSounds; ++i)
//	{
//		tSoundID soundID = m_playingSoundIDs[i];
//		if (soundID != INVALID_SOUNDID && IsSoundLooped(soundID, entityID))
//			StopSound( soundID, entityID );
//	}
//
//	// 
//	m_playingSoundIDs.clear();
//
//	const uint32 soundCount = pAudioSignal->m_sounds.size();
//
//	if(pAudioSignal->m_flags & CGameAudio::CAudioSignal::eF_PlayRandom)
//	{
//		if(soundCount > 0)
//		{
//			m_playingSoundIDs.reserve( 1 );
//
//			const size_t soundIndex = (pSpecificRandomSoundIndex && *pSpecificRandomSoundIndex>=0 && *pSpecificRandomSoundIndex<(int)soundCount) ? (size_t)*pSpecificRandomSoundIndex : cry_random(0, soundCount - 1);
//			if(pSpecificRandomSoundIndex)
//				*pSpecificRandomSoundIndex = (int)soundIndex;
//			const CGameAudio::CSound& sound = pAudioSignal->m_sounds[soundIndex];
//			tSoundID soundID = PlaySound( sound.m_name, semanticOverride == eSoundSemantic_None ? sound.m_semantic : semanticOverride, entityID, pParam, param, sound.m_flags );
//			m_playingSoundIDs.push_back( soundID );
//		}
//		else
//		{
//			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignalPlayer: PlayRandom flag on but no sounds '%s'", pAudioSignal->m_signalName.c_str());
//		}
//	}
//	else
//	{
//		m_playingSoundIDs.reserve( pAudioSignal->m_sounds.size() );
//
//		// plays new sounds
//		
//		for (size_t soundIndex = 0; soundIndex < soundCount; ++soundIndex)
//		{
//			const CGameAudio::CSound& sound = pAudioSignal->m_sounds[soundIndex];
//			tSoundID soundID = PlaySound( sound.m_name, semanticOverride == eSoundSemantic_None ? sound.m_semantic : semanticOverride, entityID, pParam, param, sound.m_flags );
//			m_playingSoundIDs.push_back( soundID );
//		}
//	}
//	
//	ExecuteCommands( entityID, pAudioSignal );
//}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::ExecuteCommands( EntityId entityID, const CGameAudio::CAudioSignal* pAudioSignal )
{
	if (entityID!=0 && g_pGame->GetIGameFramework()->GetClientActorId()!=entityID)
		return;
	
	uint32 numCommands = pAudioSignal->m_commands.size();
	
	for (uint32 i=0; i<numCommands; ++i)
	{
		const CGameAudio::CCommand* pCommand = pAudioSignal->m_commands[i];
		assert( pCommand && pCommand->IsACommand() );  // only good initialized commands should get into the audiosignal on load, anyway
		
		pCommand->Execute();
	}
}


//////////////////////////////////////////////////////////////////////////

//void CAudioSignalPlayer::Stop( EntityId entityID, const ESoundStopMode stopMode )
//{
//	const uint32 numPlayingSounds = m_playingSoundIDs.size();
//	for (uint32 soundIndex = 0; soundIndex < numPlayingSounds; ++soundIndex)
//	{
//		if (m_playingSoundIDs[soundIndex] != INVALID_SOUNDID)
//		{
//			StopSound( m_playingSoundIDs[soundIndex], entityID, stopMode );
//			m_playingSoundIDs[soundIndex] = INVALID_SOUNDID;
//		}
//	}
//}

//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetPaused( EntityId entityID , const bool paused)
{
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );

	const size_t numSounds = m_playingSoundIDs.size();
	for (size_t s=0; s<numSounds; ++s)
	{
		ISound* pSound = GetSoundInterface( pProxy, m_playingSoundIDs[s] );
		if (pSound)
		{
			pSound->GetInterfaceExtended()->SetPaused(paused);
		}
	}*/
}

//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetSignal( const char* pName )
{
	CRY_ASSERT(pName && pName[0]);

	m_audioSignalId = g_pGame->GetGameAudio()->GetSignalID( pName );
	//m_playingSoundIDs.clear();
}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetSignalSafe( const char* pName )
{
	if (pName && pName[0]!=0)
		SetSignal( pName );
	else
		SetSignal( INVALID_AUDIOSIGNAL_ID );
}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetSignal( TAudioSignalID signalID )
{
	m_audioSignalId = signalID;
	//m_playingSoundIDs.clear();
}


//////////////////////////////////////////////////////////////////////////

bool CAudioSignalPlayer::IsPlaying( EntityId entityID ) const
{
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );
		
	const size_t numSounds = m_playingSoundIDs.size();
	for (size_t s=0; s<numSounds; ++s)
	{
		ISound* pSound = GetSoundInterface( pProxy, m_playingSoundIDs[s] );
		if (pSound && pSound->IsPlaying())
			return true;
	}*/
	
	return false;
}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetVolume( EntityId entityID, float vol )
{
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );

	const size_t numSounds = m_playingSoundIDs.size();
	for (size_t s=0; s<numSounds; ++s)
	{
		ISound* pSound = GetSoundInterface( pProxy, m_playingSoundIDs[s] );
		if (pSound)
			pSound->GetInterfaceExtended()->SetVolume( vol );
	}*/
}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetOffsetPos( EntityId entityID, const Vec3& pos )
{
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );
	if (pProxy)
	{
		const size_t numSounds = m_playingSoundIDs.size();
		for (size_t s=0; s<numSounds; ++s)
		{
			pProxy->SetSoundPos( m_playingSoundIDs[s], pos );
		}
	}*/
}



//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetCurrentSamplePos( EntityId entityID, float relativePosition )
{
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );

	const size_t numSounds = m_playingSoundIDs.size();
	for (size_t s=0; s<numSounds; ++s)
	{
		ISound* pSound = GetSoundInterface( pProxy, m_playingSoundIDs[s] );
		if (pSound)
		{
			const unsigned int newPos = int_round((float)pSound->GetLengthMs() * relativePosition);
			pSound->GetInterfaceExtended()->SetCurrentSamplePos(newPos, true);
		}
	}*/
}


//////////////////////////////////////////////////////////////////////////

float CAudioSignalPlayer::GetCurrentSamplePos( EntityId entityID )
{
	float position = 0.0f;
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );

	const size_t numSounds = m_playingSoundIDs.size();
	for (size_t s=0; s<numSounds; ++s)
	{
		ISound* pSound = GetSoundInterface( pProxy, m_playingSoundIDs[s] );
		if (pSound)
		{
			const int length = pSound->GetLengthMs();
			if(length>0)
			{
				position = ((float)pSound->GetInterfaceExtended()->GetCurrentSamplePos(true) / (float)length);
				break;
			}
		}
	}*/
	return position;
}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::SetParam( EntityId entityID, const char* paramName, float paramValue )
{
	/*IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );
	
	const size_t numSounds = m_playingSoundIDs.size();
	for (size_t s=0; s<numSounds; ++s)
	{
		ISound* pSound = GetSoundInterface( pProxy, m_playingSoundIDs[s] );
		if (pSound)
			pSound->SetParam( paramName, paramValue );
	}*/
}


//////////////////////////////////////////////////////////////////////////

//tSoundID CAudioSignalPlayer::PlaySound( const string& name, const ESoundSemantic semantic, EntityId entityID, const char* pParam, float param, uint32 flags )
//{
//	IEntity* pEntity = NULL;
//	Vec3 vSoundPos;
//	SSoundCacheInfo CullInfo;
//	CullInfo.bCanBeCulled = false;
//	unsigned int nSoundFlags = 0;
//	ESoundSystemErrorCode nErrorCode = eSoundSystemErrorCode_None;
//		
//	// test if we can cull early
//	if(entityID)
//	{
//		pEntity = gEnv->pEntitySystem->GetEntity(entityID);
//		
//		if (pEntity)
//		{
//			vSoundPos = pEntity->GetWorldPos();
//			nErrorCode = gEnv->pSoundSystem->GetCullingByCache(name.c_str(), vSoundPos, CullInfo);
//		}
//	}
//
//	// no need to even create the sound
//	if (nErrorCode == eSoundSystemErrorCode_None && CullInfo.bCanBeCulled)
//		return INVALID_SOUNDID;
//
//	if (nErrorCode == eSoundSystemErrorCode_SoundCRCNotFoundInCache)
//		flags |= FLAG_SOUND_ADD_TO_CACHE;
//
//	char sFixSoundName[256];
//	gEnv->pSoundSystem->HackFixupName(name.c_str(), sFixSoundName, 256);
//
//	ISound* pSound = gEnv->pSoundSystem->CreateSound(sFixSoundName, flags, 0, FLAG_SOUND_PRECACHE_UNLOAD_AFTER_PLAY);
//
//	if(!pSound)
//	{
//#ifndef _RELEASE
//		if(!gEnv->IsDedicated() && gEnv->pConsole->GetCVar("s_SoundEnable")->GetIVal())
//		{
//			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignalPlayer: Unable to create sound '%s'", sFixSoundName);
//		}
//#endif
//		return INVALID_SOUNDID;
//	}
//
//	pSound->SetSemantic(semantic);
//	if(pParam)
//		pSound->SetParam(pParam, param);
//
//	if(pEntity)
//	{
//		IEntityAudioComponent* pIEntityAudioComponent = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
//		if(pIEntityAudioComponent)
//		{
//			const bool bResult = pIEntityAudioComponent->PlaySound(pSound);
//			if (!bResult)
//			{
//				pSound->Stop();
//				return INVALID_SOUNDID;
//			}
//		}
//	}
//	else
//	{
//		pSound->Play();
//	}
//
//	return pSound->GetId();
//}
//
//
////////////////////////////////////////////////////////////////////////////
//
//tSoundID CAudioSignalPlayer::PlaySound( const string& name, const ESoundSemantic semantic, const Vec3& pos, uint32 flags )
//{
//	// test if we can cull early
//	SSoundCacheInfo CullInfo;
//	CullInfo.bCanBeCulled = false;
//	ESoundSystemErrorCode nErrorCode = gEnv->pSoundSystem->GetCullingByCache(name.c_str(), pos, CullInfo);
//
//	// no need to even create the sound
//	if (nErrorCode == eSoundSystemErrorCode_None && CullInfo.bCanBeCulled)
//		return INVALID_SOUNDID;
//
//	if (nErrorCode == eSoundSystemErrorCode_SoundCRCNotFoundInCache)
//		flags |= FLAG_SOUND_ADD_TO_CACHE;
//
//	ISound* pSound = gEnv->pSoundSystem->CreateSound( name.c_str(), flags, 0, FLAG_SOUND_PRECACHE_UNLOAD_AFTER_PLAY);
//
//	if (pSound == 0)
//	{
//#ifndef _RELEASE
//		if(!gEnv->IsDedicated() && gEnv->pConsole->GetCVar("s_SoundEnable")->GetIVal())
//		{
//			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignalPlayer: Unable to create sound '%s'", name.c_str());
//		}
//#endif
//		return INVALID_SOUNDID;
//	}
//
//	pSound->SetSemantic( semantic );
//	pSound->SetPosition( pos );
//	pSound->Play();
//
//	return pSound->GetId();
//}
//
//
//
////////////////////////////////////////////////////////////////////////////
//
//void CAudioSignalPlayer::StopSound( const tSoundID soundID, EntityId entityID, const ESoundStopMode stopMode)
//{
//	IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );
//	if (pProxy)
//		pProxy->StopSound(soundID);
//	else
//	{
//		ISound* pSound = gEnv->pSoundSystem->GetSound( soundID );
//		if (pSound)
//			pSound->Stop(stopMode);
//	}
//}
//
//
////////////////////////////////////////////////////////////////////////////
//
//bool CAudioSignalPlayer::IsSoundLooped( const tSoundID soundID, EntityId entityID )
//{
//	IEntityAudioComponent* pProxy = CGameAudioUtils::GetEntityAudioProxy( entityID );
//	ISound* pSound = GetSoundInterface( pProxy, soundID );
//	
//	return (pSound && pSound->GetFlags() & FLAG_SOUND_LOOP);
//}
//
////////////////////////////////////////////////////////////////////////////
//
//ISound* CAudioSignalPlayer::GetSoundInterface( IEntityAudioComponent* pProxy, tSoundID soundID ) const
//{
//	if (soundID != INVALID_SOUNDID)
//	{
//		if (pProxy)
//			return pProxy->GetSound( soundID );
//		else
//			return gEnv->pSoundSystem->GetSound( soundID );
//	}
//
//	return NULL;
//}


//////////////////////////////////////////////////////////////////////////

void CAudioSignalPlayer::Reset()
{
	//stl::free_container(m_playingSoundIDs);
}


//////////////////////////////////////////////////////////////////////////
// static function, provides a way to play signals using just IDs. the limitation is that signals can only be played, not stopped.
// for signals that need the posibility to be stoped, need to use a real CAudioSignalPlayer object

void CAudioSignalPlayer::JustPlay( const char* signal, EntityId entityID, const char* pParam, float param)
{
	JustPlay(g_pGame->GetGameAudio()->GetSignalID(signal), entityID, pParam, param);
}

void CAudioSignalPlayer::JustPlay( TAudioSignalID signalID, EntityId entityID, const char* pParam, float param)
{
	/*const CGameAudio::CAudioSignal* pAudioSignal = g_pGame->GetGameAudio()->GetAudioSignal( signalID );

	if (!pAudioSignal)
		return;

	const uint32 soundCount = pAudioSignal->m_sounds.size();
	if(pAudioSignal->m_flags & CGameAudio::CAudioSignal::eF_PlayRandom)
	{
		if(soundCount > 0)
		{
			const size_t soundIndex = cry_random(0, soundCount - 1);
			const CGameAudio::CSound& sound = pAudioSignal->m_sounds[soundIndex];
			PlaySound( sound.m_name, sound.m_semantic, entityID, pParam, param, sound.m_flags );
		}
		else
		{
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignalPlayer: PlayRandom flag on but no sounds '%s'", pAudioSignal->m_signalName.c_str());
		}
	}
	else
	{
		for (uint32 soundIndex = 0; soundIndex < soundCount; ++soundIndex)
		{
			const CGameAudio::CSound& sound = pAudioSignal->m_sounds[soundIndex];
			PlaySound( sound.m_name, sound.m_semantic, entityID, pParam, param, sound.m_flags );
		}
	}

	ExecuteCommands( entityID, pAudioSignal );*/
}

//////////////////////////////////////////////////////////////////////////
// when need to play the signal at a 3D pos, but cant use an entity proxy for some reason


void CAudioSignalPlayer::JustPlay( const char* signal, const Vec3& pos )
{
	JustPlay(g_pGame->GetGameAudio()->GetSignalID(signal), pos);
}

void CAudioSignalPlayer::JustPlay( TAudioSignalID signalID, const Vec3& pos )
{
	/*const CGameAudio::CAudioSignal* pAudioSignal = g_pGame->GetGameAudio()->GetAudioSignal( signalID );

	if (!pAudioSignal)
		return;

	const uint32 soundCount = pAudioSignal->m_sounds.size();
	if(pAudioSignal->m_flags & CGameAudio::CAudioSignal::eF_PlayRandom)
	{
		if(soundCount > 0)
		{
			const size_t soundIndex = cry_random(0, soundCount - 1);
			const CGameAudio::CSound& sound = pAudioSignal->m_sounds[soundIndex];
			PlaySound( sound.m_name, sound.m_semantic, pos, sound.m_flags );
		}
		else
		{
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AudioSignalPlayer: PlayRandom flag on but no sounds '%s'", pAudioSignal->m_signalName.c_str());
		}
	}
	else
	{
		for (uint32 soundIndex = 0; soundIndex < soundCount; ++soundIndex)
		{
			const CGameAudio::CSound& sound = pAudioSignal->m_sounds[soundIndex];
			PlaySound( sound.m_name, sound.m_semantic, pos, sound.m_flags );
		}
	}

	ExecuteCommands( 0, pAudioSignal );*/
}

float CAudioSignalPlayer::GetSignalLength(TAudioSignalID signalID)
{
	float length = 0.0f;

	/*const CGameAudio::CAudioSignal* pAudioSignal = g_pGame->GetGameAudio()->GetAudioSignal( signalID );
	if(pAudioSignal)
	{
	const int soundsSize = pAudioSignal->m_sounds.size();
	for(int i = 0; i < soundsSize; i++)
	{
	const CGameAudio::CSound& sound = pAudioSignal->m_sounds[i];
	ISound* pSound = gEnv->pSoundSystem->CreateSound( sound.m_name.c_str(), sound.m_flags | FLAG_SOUND_LOAD_SYNCHRONOUSLY, 0, FLAG_SOUND_PRECACHE_UNLOAD_NOW );
	if(pSound)
	{
	length = max(pSound->GetLengthMs()/1000.0f, length);
	pSound->Stop(ESoundStopMode_AtOnce);
	}
	}
	}*/

	return length;
}

#if !defined(_RELEASE) || defined(RELEASE_LOGGING)
//////////////////////////////////////////////////////////////////////////
// for debug purposes


/*static*/ const char* CAudioSignalPlayer::GetSignalName(TAudioSignalID signalId)
{
	const CGameAudio::CAudioSignal* pAudioSignal = g_pGame->GetGameAudio()->GetAudioSignal( signalId );

	if (!pAudioSignal)
		return "NULL";
		
	return pAudioSignal->m_signalName.c_str();
}

const char* CAudioSignalPlayer::GetSignalName()
{
	return GetSignalName(m_audioSignalId);
}
#endif