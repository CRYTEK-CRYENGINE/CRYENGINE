// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 11:8:2004   11:39 : Created by Márcio Martins

*************************************************************************/
#ifndef __GAMESERVERCHANNEL_H__
#define __GAMESERVERCHANNEL_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "GameChannel.h"
#include <CryNetwork/INetwork.h>

// toggle between a fast and a somewhat safer cvar synchronization (whilst debugging it)
#define FAST_CVAR_SYNC 0

class CGameServerNub;
class CGameContext;

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
struct SMutePlayerParams
{
	SMutePlayerParams()
		: mute(false)
		, requestor(INVALID_ENTITYID)
		, id(INVALID_ENTITYID)
	{
	}

	EntityId     requestor; // player requesting the mute
	EntityId     id;        // player to mute
	bool         mute;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("requestor", requestor, 'eid');
		ser.Value("id", id, 'eid');
		ser.Value("mute", mute);
	}
};
#endif

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)

struct SSyncTimeServer
{
	SSyncTimeServer() {};
	SSyncTimeServer(int id, int64 clientTime, int64 serverTime) { this->id = id; this->clientTime = clientTime; this->serverTime = serverTime; };

	int          id;
	int64        clientTime;
	int64        serverTime;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("id", id);
		ser.Value("clientTime", clientTime);
		ser.Value("serverTime", serverTime);
	}
};

#endif //GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME

class CGameServerChannel :
	public CNetMessageSinkHelper<CGameServerChannel, CGameChannel>, public IConsoleVarSink
{
public:
	CGameServerChannel(INetChannel* pNetChannel, CGameContext* pGameContext, CGameServerNub* pServerNub);
	virtual ~CGameServerChannel();

#if !NEW_BANDWIDTH_MANAGEMENT
	void SetupNetChannel(INetChannel* pNetChannel);
#endif // !NEW_BANDWIDTH_MANAGEMENT
	bool IsOnHold() const     { return m_onHold; };
	void SetOnHold(bool hold) { m_onHold = hold; };

	// IGameChannel
	virtual void Release() override;
	virtual void OnDisconnect(EDisconnectionCause cause, const char* description) override;
	// ~IGameChannel

	// GameChannel
	virtual bool IsServer() const override { return true; }
	//~GameChannel

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder* pBuilder) override;
	// ~INetMessageSink

	// IConsoleVarSink
	virtual bool OnBeforeVarChange(ICVar* pVar, const char* sNewValue) override;
	virtual void OnAfterVarChange(ICVar* pVar) override;
	virtual void OnVarUnregister(ICVar* pVar) override {}
	// ~IConsoleVarSink

	void SetChannelId(uint16 channelId)
	{
		m_channelId = channelId;
	};
	uint16 GetChannelId() const
	{
		return m_channelId;
	};

	void SetPlayerId(EntityId playerId);

#if FAST_CVAR_SYNC
	SSendableHandle& GetConsoleStreamId(const string& name)
	{
		return m_consoleVarStreamId[name];
	}
#endif

	bool CheckLevelLoaded() const;
	void AddUpdateLevelLoaded(IContextEstablisher* pEst);

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(this, sizeof(*this));
#if FAST_CVAR_SYNC
		s->AddObject(m_consoleVarStreamId);
		for (std::map<string, SSendableHandle>::iterator iter = m_consoleVarStreamId.begin(); iter != m_consoleVarStreamId.end(); ++iter)
			s->Add(iter->first);
#endif
	}

	// mute players' voice comms
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(MutePlayer, SMutePlayerParams);
#endif

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SyncTimeServer, SSyncTimeServer);
#endif

private:
	void Cleanup();

	CGameServerNub* m_pServerNub;

	uint16          m_channelId;
	bool            m_hasLoadedLevel;
	bool            m_onHold;

#if FAST_CVAR_SYNC
	std::map<string, SSendableHandle> m_consoleVarStreamId;
#else
	SSendableHandle                   m_consoleVarSendable;
#endif

	static ICVar* sv_timeout_disconnect;
};

#endif //__GAMESERVERCHANNEL_H__
