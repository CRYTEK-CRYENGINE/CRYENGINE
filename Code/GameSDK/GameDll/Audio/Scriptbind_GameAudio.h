// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCRIPTBIND_GAMEAUDIO_H__
#define __SCRIPTBIND_GAMEAUDIO_H__

#include <CryScriptSystem/ScriptHelpers.h>
#include "AudioSignalPlayer.h"

struct ISystem;
struct IFunctionHandler;

class CScriptbind_GameAudio : public CScriptableBase
{
public:
	CScriptbind_GameAudio();
	virtual ~CScriptbind_GameAudio();
	void Reset();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddContainer(m_signalPlayers);		
		pSizer->AddContainer(m_signalPlayersSearchMap);		
	}
private:
	int GetSignal( IFunctionHandler *pH, const char* pSignalName );
	
	int JustPlaySignal( IFunctionHandler *pH, TAudioSignalID audioSignalID );  // a signal played with JustPlaySignal cant be stopped.
	int JustPlayEntitySignal( IFunctionHandler *pH, TAudioSignalID audioSignalID, ScriptHandle entityId );  // a signal played with JustPlaySignal cant be stopped.
	int JustPlayPosSignal(IFunctionHandler *pH, TAudioSignalID signalId, Vec3 pos );
	int PlayEntitySignal( IFunctionHandler *pH, TAudioSignalID audioSignalID, ScriptHandle entityId );
	int IsPlayingEntitySignal( IFunctionHandler *pH, TAudioSignalID audioSignalID, ScriptHandle entityId );
	int StopEntitySignal(  IFunctionHandler *pH, TAudioSignalID audioSignalID, ScriptHandle entityId );
	int SetEntitySignalParam(  IFunctionHandler *pH, TAudioSignalID audioSignalID, ScriptHandle entityId, const char *param, float fValue );
	int PlaySignal( IFunctionHandler *pH, TAudioSignalID audioSignalID );
	int StopSignal(  IFunctionHandler *pH, TAudioSignalID audioSignalID );
	int Announce( IFunctionHandler *pH, const char* announcement, int context );

	void RegisterMethods();
	
	void PlaySignal_Internal( TAudioSignalID audioSignalID, EntityId entityId );
	bool IsPlayingSignal_Internal( TAudioSignalID signalId, EntityId entityId);
	void StopSignal_Internal( TAudioSignalID audioSignalID, EntityId entityId );
	void SetSignalParam_Internal( TAudioSignalID audioSignalID, EntityId entityId, const char *param, float fValue );
	
	struct SKey
	{
		SKey()
		: m_signalID( INVALID_AUDIOSIGNAL_ID )
		, m_entityId(0)
		{}

		SKey( TAudioSignalID signalID, EntityId entityId )
			: m_signalID( signalID )
			, m_entityId( entityId )
		{}
		
		bool operator < ( const SKey& otherKey ) const
		{
			if (m_signalID<otherKey.m_signalID)
				return true;
			else
				if (m_signalID>otherKey.m_signalID)
					return false;
				else
					return m_entityId<otherKey.m_entityId;
		}

		void GetMemoryUsage(ICrySizer *pSizer) const {/*nothing*/}

		TAudioSignalID m_signalID;
		EntityId m_entityId;
	};

	std::vector<CAudioSignalPlayer> m_signalPlayers;
	typedef std::map<SKey, int> TSignalPlayersSearchMap;
	TSignalPlayersSearchMap m_signalPlayersSearchMap;
};

#endif //__SCRIPTBIND_GAME_H__