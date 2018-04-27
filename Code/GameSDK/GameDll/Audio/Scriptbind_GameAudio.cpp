// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Scriptbind_GameAudio.h"
#include "GameAudio.h"
#include "Game.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Audio/Announcer.h"

//------------------------------------------------------------------------
CScriptbind_GameAudio::CScriptbind_GameAudio()
{
	Init( gEnv->pScriptSystem, gEnv->pSystem );
	SetGlobalName("GameAudio");

	RegisterMethods();
}

//------------------------------------------------------------------------
CScriptbind_GameAudio::~CScriptbind_GameAudio()
{
	Reset();
}


//------------------------------------------------------------------------
void CScriptbind_GameAudio::Reset()
{
	TSignalPlayersSearchMap::iterator iter = m_signalPlayersSearchMap.begin();

	while (iter!=m_signalPlayersSearchMap.end())
	{
		const SKey& key = iter->first;
		int ind = iter->second;
		CAudioSignalPlayer& signalPlayer = m_signalPlayers[ind];
		
		//signalPlayer.Stop( key.m_entityId );
		REINST("needs verification!");
		
		++iter;
	}

	stl::free_container(m_signalPlayers);
	m_signalPlayersSearchMap.clear();
}


//------------------------------------------------------------------------
void CScriptbind_GameAudio::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptbind_GameAudio::

	SCRIPT_REG_TEMPLFUNC(JustPlaySignal, "audioSignalID");
	SCRIPT_REG_TEMPLFUNC(JustPlayEntitySignal, "audioSignalID, entityId");
	SCRIPT_REG_TEMPLFUNC(JustPlayPosSignal, "audioSignalID, vPos");
	SCRIPT_REG_TEMPLFUNC(GetSignal, "name");
	SCRIPT_REG_TEMPLFUNC(PlayEntitySignal, "audioSignalID, entityId");
	SCRIPT_REG_TEMPLFUNC(IsPlayingEntitySignal, "audioSignalID, entityId");
	SCRIPT_REG_TEMPLFUNC(StopEntitySignal, "audioSignalID, entityId");
	SCRIPT_REG_TEMPLFUNC(SetEntitySignalParam, "audioSignalID, entityId, param, fValue");
	SCRIPT_REG_TEMPLFUNC(PlaySignal, "audioSignalID");
	SCRIPT_REG_TEMPLFUNC(StopSignal, "audioSignalID");
	SCRIPT_REG_TEMPLFUNC(Announce, "announcement, context");

#undef SCRIPT_REG_CLASSNAME
}

//------------------------------------------------------------------------
int CScriptbind_GameAudio::GetSignal(IFunctionHandler *pH, const char* pSignalName )
{
	TAudioSignalID signalID = g_pGame->GetGameAudio()->GetSignalID( pSignalName );
	return pH->EndFunction( signalID );
}


//------------------------------------------------------------------------
int CScriptbind_GameAudio::JustPlayEntitySignal(IFunctionHandler *pH, TAudioSignalID signalId, ScriptHandle entityId )
{
	CAudioSignalPlayer::JustPlay( signalId, (EntityId)entityId.n );
	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptbind_GameAudio::JustPlayPosSignal(IFunctionHandler *pH, TAudioSignalID signalId, Vec3 pos )
{
	CAudioSignalPlayer::JustPlay( signalId, pos );
	return pH->EndFunction();
}



//------------------------------------------------------------------------
int CScriptbind_GameAudio::JustPlaySignal(IFunctionHandler *pH, TAudioSignalID signalId )
{
	CAudioSignalPlayer::JustPlay( signalId, 0 );
	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptbind_GameAudio::PlayEntitySignal(IFunctionHandler *pH, TAudioSignalID signalId, ScriptHandle handleEntityId )
{
	CCCPOINT(Audio_ScriptPlayEntitySignal);
	EntityId entityId = (EntityId)handleEntityId.n;
	PlaySignal_Internal( signalId, entityId );
	return pH->EndFunction();
}	
	
//------------------------------------------------------------------------
int CScriptbind_GameAudio::IsPlayingEntitySignal( IFunctionHandler *pH, TAudioSignalID signalId, ScriptHandle handleEntityId )
{
	EntityId entityId = (EntityId)handleEntityId.n;
	bool isPlaying = IsPlayingSignal_Internal( signalId, entityId );
	return pH->EndFunction(isPlaying);
}	

//------------------------------------------------------------------------
int CScriptbind_GameAudio::StopEntitySignal(IFunctionHandler *pH, TAudioSignalID signalId, ScriptHandle handleEntityId )
{
	CCCPOINT(Audio_ScriptStopEntitySignal);
	EntityId entityId = (EntityId)handleEntityId.n;
	StopSignal_Internal( signalId, entityId );
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptbind_GameAudio::SetEntitySignalParam(IFunctionHandler *pH, TAudioSignalID signalId, ScriptHandle handleEntityId, const char *param, float fValue )
{
	EntityId entityId = (EntityId)handleEntityId.n;
	SetSignalParam_Internal( signalId, entityId , param, fValue);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptbind_GameAudio::PlaySignal(IFunctionHandler *pH, TAudioSignalID signalId )
{
	PlaySignal_Internal( signalId, 0 );
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptbind_GameAudio::StopSignal( IFunctionHandler *pH, TAudioSignalID signalId )
{
	StopSignal_Internal( signalId, 0 );
	return pH->EndFunction();
}

//------------------------------------------------------------------------
void CScriptbind_GameAudio::PlaySignal_Internal( TAudioSignalID signalId, EntityId entityId )
{
	SKey key( signalId, entityId );

	int ind = 0;

	TSignalPlayersSearchMap::iterator iter = m_signalPlayersSearchMap.find( key );
	if (iter!=m_signalPlayersSearchMap.end())
	{
		ind = iter->second;	
	}
	else
	{
		ind = m_signalPlayers.size();
		m_signalPlayersSearchMap.insert( std::make_pair( key, ind ) );
		CAudioSignalPlayer signalPlayer;
		signalPlayer.SetSignal( signalId );
		m_signalPlayers.push_back( signalPlayer );
	}

	CAudioSignalPlayer& signalPlayer = m_signalPlayers[ind];
	//signalPlayer.Play( entityId );
}


//------------------------------------------------------------------------
void CScriptbind_GameAudio::StopSignal_Internal( TAudioSignalID signalId, EntityId entityId )
{
	SKey key( signalId, entityId );
	TSignalPlayersSearchMap::iterator iter = m_signalPlayersSearchMap.find( key );
	if (iter!=m_signalPlayersSearchMap.end())
	{
		int ind = iter->second;
		CAudioSignalPlayer& signalPlayer = m_signalPlayers[ind];
		//signalPlayer.Stop( entityId );
	}
}

//------------------------------------------------------------------------
void CScriptbind_GameAudio::SetSignalParam_Internal( TAudioSignalID signalId, EntityId entityId, const char* param, float fValue)
{
	SKey key( signalId, entityId );
	TSignalPlayersSearchMap::iterator iter = m_signalPlayersSearchMap.find( key );
	if (iter!=m_signalPlayersSearchMap.end())
	{
		int ind = iter->second;
		CAudioSignalPlayer& signalPlayer = m_signalPlayers[ind];
		signalPlayer.SetParam(entityId, param, fValue);
	}
}

//------------------------------------------------------------------------
bool CScriptbind_GameAudio::IsPlayingSignal_Internal( TAudioSignalID signalId, EntityId entityId)
{
	SKey key( signalId, entityId );
	TSignalPlayersSearchMap::iterator iter = m_signalPlayersSearchMap.find( key );
	if (iter!=m_signalPlayersSearchMap.end())
	{
		return true;
	}
	else
	{
		return false;
	}
}

//------------------------------------------------------------------------
int CScriptbind_GameAudio::Announce( IFunctionHandler *pH, const char* announcement, int context )
{
	// Multiplayer feature, but can be triggered in the Editor's singleplayer mode
	if (gEnv->bMultiplayer)
	{
		CAnnouncer* const pAnnouncer = CAnnouncer::GetInstance();
		if (pAnnouncer)
		{
			pAnnouncer->Announce(announcement, static_cast<CAnnouncer::EAnnounceContext>(context));
		}
	}
	return pH->EndFunction();
}
