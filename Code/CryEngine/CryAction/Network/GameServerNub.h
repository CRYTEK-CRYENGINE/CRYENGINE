// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <map>

typedef std::map<uint16, class CGameServerChannel*> TServerChannelMap;
class CGameContext;

class CGameServerNub final :
	public IGameServerNub
{
	typedef std::map<INetChannel*, uint16> TNetServerChannelMap;
	typedef struct SHoldChannel
	{
		SHoldChannel() : channelId(0), time(0.0f) {};
		SHoldChannel(uint16 chanId, const CTimeValue& tv) : channelId(chanId), time(tv) {};

		uint16     channelId;
		CTimeValue time;
	} SHoldChannel;

	typedef std::map<int, SHoldChannel> THoldChannelMap;

	struct SBannedPlayer
	{
		SBannedPlayer() : profileId(0), time(0.0f) {}
		SBannedPlayer(int32 profId, uint32 addr, const CTimeValue& tv) : profileId(profId), ip(addr), time(tv) {}

		int32      profileId; //profile id
		uint32     ip;        //in LAN we use ip
		CTimeValue time;
	};
	typedef std::vector<SBannedPlayer> TBannedVector;
public:
	CGameServerNub();
	virtual ~CGameServerNub();

	// IGameNub
	virtual void                 Release();
	virtual SCreateChannelResult CreateChannel(INetChannel* pChannel, const char* pRequest);
	virtual void                 FailedActiveConnect(EDisconnectionCause cause, const char* description);
	// ~IGameNub

	// IGameServerNub
	virtual void AddSendableToRemoteClients(INetSendablePtr pMsg, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle);
	// ~IGameServerNub

	void                SetGameContext(CGameContext* pGameContext) { m_pGameContext = pGameContext; };
	TServerChannelMap*  GetServerChannelMap()                      { return &m_channels; };

	void                Update();

	void                GetMemoryUsage(ICrySizer* s) const;

	void                SetMaxPlayers(int maxPlayers);
	int                 GetMaxPlayers() const  { return m_maxPlayers; }
	int                 GetPlayerCount() const { return int(m_channels.size()); }

	CGameServerChannel* GetChannel(uint16 channelId);
	CGameServerChannel* GetChannel(INetChannel* pNetChannel);
	INetChannel*        GetLocalChannel();
	uint16              GetChannelId(INetChannel* pNetChannel) const;
	void                RemoveChannel(uint16 channelId);

	bool                IsPreordered(uint16 channelId);
	bool                PutChannelOnHold(CGameServerChannel* pServerChannel);
	void                RemoveOnHoldChannel(CGameServerChannel* pServerChannel, bool renewed);
	CGameServerChannel* GetOnHoldChannelFor(INetChannel* pNetChannel);
	void                ResetOnHoldChannels();//called when onhold channels loose any sense - game reset, new level etc.

	void                BanPlayer(uint16 channelId, const char* reason);
	bool                CheckBanned(INetChannel* pNetChannel);
	void                BannedStatus();
	void                UnbanPlayer(int profileId);

private:
	TServerChannelMap    m_channels;
	TNetServerChannelMap m_netchannels;
	THoldChannelMap      m_onhold;
	TBannedVector        m_banned;
	CGameContext*        m_pGameContext;
	uint16               m_genId;
	int                  m_maxPlayers;

	static ICVar*        sv_timeout_disconnect;
	bool                 m_allowRemoveChannel;
};