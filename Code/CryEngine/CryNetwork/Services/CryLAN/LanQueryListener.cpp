// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  This listener looks for running servers in the LAN and
   is keeping a list of all available servers.
   -------------------------------------------------------------------------
   History:
   -    : Created by Craig Tiller
   - 13/01/2006   : Continued by Jan Müller
*************************************************************************/

#include "StdAfx.h"
#include "LanQueryListener.h"
#include <CrySystem/ITimer.h>
#include "Config.h"
#include "Network.h"

#include "Protocol/FrameTypes.h"
#include "Protocol/Serialize.h"

#include "DebugKit/NetworkInspector.h"

CLanQueryListener::CLanQueryListener(IGameQueryListener* pListener)
	: INetworkMember(eNMUO_Utility)
	, m_bDead(false)
	, m_pGameQueryListener(pListener)
	, m_port(SERVER_DEFAULT_PORT)
	, m_timer(0)
{
	m_pTimer = gEnv->pTimer;
	m_lastPingCleanUp = m_lastPingRefresh = m_pTimer->GetAsyncCurTime();
	m_lastBroadcast = m_lastServerListUpdate = 0;
}

CLanQueryListener::~CLanQueryListener()
{
	NET_ASSERT(m_pGameQueryListener);
	m_pGameQueryListener->Release();
	if (m_pSocket)
	{
		m_pSocket->UnregisterListener(this);
		CNetwork::Get()->GetExternalSocketIOManager().FreeDatagramSocket(m_pSocket);
	}
	m_bDead = true;
}

bool CLanQueryListener::Init()
{
	m_pSocket = CNetwork::Get()->GetExternalSocketIOManager().CreateDatagramSocket(TNetAddress(SIPv4Addr()), eSF_BroadcastSend);
	if (m_pSocket)
		m_pSocket->RegisterListener(this);
	UpdateTimer(g_time);  //this starts the timer queue
	m_bDead = false;
	BroadcastQuery(); //shoot a first Broadcast ...
	return m_pSocket != NULL;
}

void CLanQueryListener::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CLanQueryListener");

	pSizer->Add(*this);
	pSizer->AddContainer(m_outstandingPings);
	m_pGameQueryListener->GetMemoryStatistics(pSizer);
}

bool CLanQueryListener::IsDead()
{
	return m_bDead;
}

bool CLanQueryListener::IsSuicidal()
{
	return false;
}

void CLanQueryListener::DeleteThis() const
{
	delete this;
}

void CLanQueryListener::SyncWithGame(ENetworkGameSync sync)
{
}

void CLanQueryListener::OnPacket(const TNetAddress& addr, const uint8* pBuffer, uint32 nLength)
{
	if (nLength >= 1)
	{
		switch (pBuffer[0])
		{
		case '<':
			ProcessResultFrom(pBuffer, nLength, addr);
			break;
		case 'P':
			ProcessPongFrom(pBuffer, nLength, addr);
			break;
		}
	}
}

void CLanQueryListener::OnError(const TNetAddress& addr, ESocketError error)
{
	CryLogAlways("LanQueryListener received ESocketError code %i.", (int)error);
}

void CLanQueryListener::TimerCallback(NetTimerId id, void* pUser, CTimeValue time)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	CLanQueryListener* pThis = static_cast<CLanQueryListener*>(pUser);
	NET_ASSERT(pThis->m_timer == id);

	float now = pThis->m_pTimer->GetAsyncCurTime();

	if (now - pThis->m_lastPingCleanUp > 30) //clear old pings once in a while
		pThis->CleanUpPings();

	/*	if(now - pThis->m_lastPingRefresh > 15) //refresh pings from time to time
	   {
	    TO_GAME(&CLanQueryListener::NQ_RefreshPings, pThis);
	    pThis->m_lastPingRefresh = now;
	   }*/

	if (now - pThis->m_lastBroadcast > 5) //frequently search for new servers
		pThis->BroadcastQuery();

	if (now - pThis->m_lastServerListUpdate > 3) //cleanUp & sort server list (if necessary) every some seconds
	{
		pThis->m_pGameQueryListener->Update();
		pThis->m_lastServerListUpdate = now;
	}

	pThis->m_timer = 0; // expired this one...
	pThis->UpdateTimer(time);
}

void CLanQueryListener::UpdateTimer(CTimeValue time)
{
	if (m_timer)
		TIMER.CancelTimer(m_timer);
	m_timer = TIMER.ADDTIMER(g_time + 1.0f, TimerCallback, this, "CLanQueryListener::UpdateTimer() m_timer");
}

void CLanQueryListener::DeleteNetQueryListener()
{
	SCOPED_GLOBAL_LOCK;
	m_bDead = true;
	if (m_timer) //don't call me anymore ...
		TIMER.CancelTimer(m_timer);
	if (m_pSocket)
		m_pSocket->UnregisterListener(this);
}

void CLanQueryListener::ProcessPongFrom(const uint8* buffer, size_t bufferLength, const TNetAddress& addr)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	// only accept pongs for which we have pinged
	TOutstandingPings::const_iterator iter = m_outstandingPings.find(addr);
	if (iter == m_outstandingPings.end())
		return;

	// get the time when we sent the ping
	static const char* PONG = "PONG ";
	static const size_t PONG_LENGTH = strlen(PONG);
	if (bufferLength < PONG_LENGTH)
		return;
	if (0 != memcmp(PONG, buffer, PONG_LENGTH))
		return;
	int64 serNumber;
#if defined(__GNUC__)
	{
		char serNumBuffer[bufferLength - PONG_LENGTH + 1];
		memcpy(serNumBuffer, buffer + PONG_LENGTH, bufferLength - PONG_LENGTH);
		serNumBuffer[bufferLength - PONG_LENGTH] = 0;
		long long num = 0;
		if (sscanf(serNumBuffer, "%lld", &num) != 1)
			return;
		serNumber = (int64)num;
	}
#else
	if (1 != _snscanf((const char*)buffer + PONG_LENGTH, bufferLength - PONG_LENGTH, "%I64d", &serNumber))
		return;
#endif

	CTimeValue when(serNumber);
	// make sure it matches what we stored
	if (when != iter->second)
		return;

	// accept no more responses for this ping - iter is now invalid
	m_outstandingPings.erase(iter->first);

	// add the ping to our data structure...
	int64 now = m_pTimer->GetAsyncTime().GetValue();
	uint32 ping = int32((now - when.GetValue()) * 0.01f);
	CNetAddressResolver res;
	string address = res.ToString(addr);

	TO_GAME(&CLanQueryListener::NQ_AddPong, this, address, ping);
}

void CLanQueryListener::ProcessResultFrom(const uint8* buffer, size_t bufferLength, const TNetAddress& addr)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	if (IsDead())
		return;

	string temp((const char*)buffer, (const char*)(buffer + bufferLength));
	XmlNodeRef xml = GetISystem()->LoadXmlFromBuffer(temp.c_str(), temp.length());
	//this dumps the servers in the console...
	//m_pGameQueryListener->OnReceiveGameState( RESOLVER.ToString(addr).c_str(), xml );

	size_t pos = temp.find("\n");
	while (pos != string::npos)    //delete newlines and unimportant info
	{
		temp.erase(pos, 1);
		pos = temp.find("\n");
	}
	pos = temp.find("<root>");
	if (pos != string::npos)
		temp.erase(pos, 7);
	pos = temp.find("<\root>");
	if (pos != string::npos)
		temp.erase(pos, 8);
	pos = temp.find("<server />");
	if (pos != string::npos)
		temp.erase(pos, 10);
	pos = temp.find("</root>");
	if (pos != string::npos)
		temp.erase(pos, 7);
	pos = temp.find("  ");
	while (pos != string::npos)
	{
		temp.erase(pos, 1);
		pos = temp.find("  ");
	}

	// write message to my own list
	SLANServerDetails msg("Server", RESOLVER.ToString(addr).c_str(), temp.c_str());

	NET_ASSERT(m_pGameQueryListener);
	TO_GAME(&CLanQueryListener::NQ_AddServer, this, msg);
}

void CLanQueryListener::BroadcastQuery()
{
	m_pSocket->Send(Frame_IDToHeader + eH_QueryLan, 1, TNetAddress(SIPv4Addr(0xffffffffu, m_port)));
	m_lastBroadcast = m_pTimer->GetAsyncCurTime();
}

void CLanQueryListener::SendPingTo(const char* addr)
{
	SCOPED_GLOBAL_LOCK;
	CNameRequestPtr pReq = RESOLVER.RequestNameLookup(addr);
	TO_GAME_LAZY(&CLanQueryListener::GQ_Lazy_PrepareSendPingTo, this, pReq);
	//	FROM_GAME(&CLanQueryListener::GQ_Lazy_PrepareSendPingTo, this, pReq);
}

void CLanQueryListener::GQ_Lazy_PrepareSendPingTo(CNameRequestPtr pReq)
{
	if (pReq->GetResult() != eNRR_Pending)
	{
		TO_GAME_LAZY(&CLanQueryListener::GQ_Lazy_PrepareSendPingTo, this, pReq);
	}
	else
	{
		TO_GAME(&CLanQueryListener::GQ_SendPingTo, this, pReq);
	}
}

void CLanQueryListener::GQ_SendPingTo(CNameRequestPtr pReq)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	TNetAddressVec addrVec;
	if (pReq->GetResult(addrVec) != eNRR_Succeeded)
	{
		NetWarning("Unable to find address %s", pReq->GetAddrString().c_str());
		return;
	}

	//NET_ASSERT( GetFrameType(PING_SERVER[0]) == QueryFrameHeader );
	char buffer[64];
	CTimeValue when = g_time;
	int64 serNumber = when.GetValue();
	cry_sprintf(buffer, "%s %" PRIi64, "PING",
#if defined(__GNUC__)
	            (long long)serNumber
#else
	            serNumber
#endif
	            );
	m_pSocket->Send((const uint8*)buffer, strlen(buffer), addrVec[0]);
	m_outstandingPings[addrVec[0]] = when;
}

void CLanQueryListener::CleanUpPings()
{
	if (m_outstandingPings.size() == 0)
		return;

	TOutstandingPings::const_iterator it = m_outstandingPings.begin();
	for (; it != m_outstandingPings.end(); )
	{
		TOutstandingPings::const_iterator next = it;
		++next;
		CTimeValue pingTime = (*it).second.GetSeconds();
		if ((g_time - pingTime).GetSeconds() > 60)
			m_outstandingPings.erase(it->first);
		it = next;
	}
}

void CLanQueryListener::NQ_AddServer(SLANServerDetails msg)
{
	m_pGameQueryListener->AddServer(msg.m_description.c_str(), msg.m_target.c_str(), msg.m_additionalText, 0);
}

void CLanQueryListener::NQ_AddPong(string address, uint32 ping)
{
	m_pGameQueryListener->AddPong(address, ping);
}

void CLanQueryListener::NQ_RefreshPings()
{
	m_pGameQueryListener->RefreshPings();
}
