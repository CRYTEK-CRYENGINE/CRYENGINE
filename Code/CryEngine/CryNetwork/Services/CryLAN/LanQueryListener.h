// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  This listener looks for running servers in the LAN and
   is keeping a list of all available servers.
   There is a callback to get this list (ordered by ping) as well as to connect to one ...
   -------------------------------------------------------------------------
   History:
   -    : Created by Craig Tiller
   - 13/01/2006   : Continued by Jan Müller
*************************************************************************/

#ifndef __LANQUERYLISTENER_H__
#define __LANQUERYLISTENER_H__

#pragma once

#include "INetworkMember.h"
#include "Socket/IDatagramSocket.h"
#include <CryNetwork/INetwork.h>
#include "NetTimer.h"

#include <vector>

struct SLANServerDetails
{
	string m_description;
	string m_target;
	string m_additionalText;

	ILINE SLANServerDetails(const char* description, const char* target, const char* additionalText) :
		m_description(description), m_target(target)
	{
		if (additionalText != NULL)
			m_additionalText = string(additionalText);
	}

	bool Compare(SLANServerDetails& msg)
	{
		if (msg.m_description.compare(m_description) == 0 && msg.m_target.compare(m_target) == 0)
			return true;
		return false;
	}
};

class CLanQueryListener : public INetworkMember, public ILanQueryListener, public IDatagramListener
{
public:
	CLanQueryListener(IGameQueryListener*);
	~CLanQueryListener();
	bool Init();

	// INetworkMember
	virtual void GetMemoryStatistics(ICrySizer*) override;
	virtual bool IsDead() override;
	virtual bool IsSuicidal() override;
	virtual void DeleteThis() const override;
	virtual void SyncWithGame(ENetworkGameSync) override;
	virtual void NetDump(ENetDumpType) override {};
	virtual void PerformRegularCleanup() override {};
	// ~INetworkMember

	// INetQueryListener
	virtual void DeleteNetQueryListener() override;
	// ~INetQueryListener

	//IDatagramListener
	virtual void OnPacket(const TNetAddress& addr, const uint8* pData, uint32 nLength) override;
	virtual void OnError(const TNetAddress& addr, ESocketError error) override;
	//~IDatagramListener

	//ILanQueryListener
	virtual void                SendPingTo(const char* addr) override;
	virtual IGameQueryListener* GetGameQueryListener() override { return m_pGameQueryListener; }
	//~ILanQueryListener

	// prepare to send the ping (wait for the name to be resolved)
	void GQ_Lazy_PrepareSendPingTo(CNameRequestPtr pReq);
	//actually send a ping ...
	void GQ_SendPingTo(CNameRequestPtr pReq);
	//actually send an add server to game code
	void NQ_AddServer(SLANServerDetails msg);
	//actually send an add pong to game code
	void NQ_AddPong(string address, uint32 ping);
	//actually refresh pings
	void NQ_RefreshPings();

	/*ILINE void ConnectToServer(int number)
	   {
	   if(m_servers.size() > number && number >= 0)
	    ConnectToServer(m_servers[number].m_message.m_target.c_str());
	   }*/

private:
	bool                m_bDead;
	IGameQueryListener* m_pGameQueryListener;

	//timed update loop of the listener
	NetTimerId  m_timer;
	static void TimerCallback(NetTimerId, void*, CTimeValue);
	void UpdateTimer(CTimeValue time);

	//throw out very old pings
	void CleanUpPings();

	//this renders a little debugging interface as a temporary server-browser
	void RenderDebugInterface();

	void ProcessResultFrom(const uint8* buffer, size_t bufferLength, const TNetAddress& addr);
	void ProcessPongFrom(const uint8* buffer, size_t bufferLength, const TNetAddress& addr);
	void BroadcastQuery();

	ITimer*            m_pTimer;
	float              m_lastPingCleanUp, m_lastPingRefresh, m_lastBroadcast, m_lastServerListUpdate;
	IDatagramSocketPtr m_pSocket;
	uint16             m_port;

	typedef std::map<TNetAddress, CTimeValue> TOutstandingPings;
	TOutstandingPings m_outstandingPings;
};

#endif
