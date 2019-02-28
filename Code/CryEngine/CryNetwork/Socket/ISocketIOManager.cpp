// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ISocketIOManager.h"
#include "SocketIOManagerIOCP.h"
#include "SocketIOManagerNull.h"
#include "SocketIOManagerSelect.h"
#include "SocketIOManagerLobbyIDAddr.h"
#if CRY_PLATFORM_DURANGO
	#include "SocketIOManagerDurango.h"
#endif

IDatagramSocketPtr CSocketIOManager::CreateDatagramSocket(const TNetAddress& addr, uint32 flags)
{
	SCOPED_GLOBAL_LOCK;

	TDatagramSockets::iterator socketIter;
	IDatagramSocketPtr pSocket;
	const SIPv4Addr* pIPv4Addr = stl::get_if<SIPv4Addr>(&addr);

	for (socketIter = m_datagramSockets.begin(); socketIter != m_datagramSockets.end(); ++socketIter)
	{
		const SIPv4Addr* pTestIPv4Addr = stl::get_if<SIPv4Addr>(&socketIter->addr);

		if ((socketIter->addr == addr) ||
		    (pIPv4Addr && pTestIPv4Addr && ((flags & eSF_StrictAddress) == 0) && ((flags & eSF_Online) == (socketIter->flags & eSF_Online))))
		{
			socketIter->refCount++;

			return socketIter->pSocket;
		}
	}

	pSocket = OpenSocket(addr, flags);

	if (pSocket)
	{
		SDatagramSocketData data;

		data.addr = addr;
		data.pSocket = pSocket;
		data.flags = flags;
		data.refCount = 1;

		m_datagramSockets.push_back(data);
	}

	return pSocket;
}

void CSocketIOManager::FreeDatagramSocket(IDatagramSocketPtr pSocket)
{
	SCOPED_GLOBAL_LOCK;

	if (pSocket)
	{
		TDatagramSockets::iterator socketIter;

		for (socketIter = m_datagramSockets.begin(); socketIter != m_datagramSockets.end(); ++socketIter)
		{
			if (socketIter->pSocket == pSocket)
			{
				socketIter->refCount--;

				if (socketIter->refCount == 0)
				{
					m_datagramSockets.erase(socketIter);
				}

				return;
			}
		}
	}
}

#if NET_MINI_PROFILE || NET_PROFILE_ENABLE

void CSocketIOManager::RecordPacketSendStatistics(const uint8* pData, size_t len)
{
	uint32 thisSend = (len + UDP_HEADER_SIZE) * 8;

	g_socketBandwidth.periodStats.m_totalBandwidthSent += thisSend;
	g_socketBandwidth.periodStats.m_totalPacketsSent++;
	g_socketBandwidth.bandwidthStats.m_total.m_totalBandwidthSent += thisSend;
	g_socketBandwidth.bandwidthStats.m_total.m_totalPacketsSent++;

	uint8 headerType = Frame_HeaderToID[pData[0]];
	if (headerType == eH_CryLobby)
	{
		g_socketBandwidth.periodStats.m_lobbyBandwidthSent += thisSend;
		g_socketBandwidth.periodStats.m_lobbyPacketsSent++;
		g_socketBandwidth.bandwidthStats.m_total.m_lobbyBandwidthSent += thisSend;
		g_socketBandwidth.bandwidthStats.m_total.m_lobbyPacketsSent++;
	}
	else
	{
		if (headerType == eH_Fragmentation)
		{
			g_socketBandwidth.periodStats.m_fragmentBandwidthSent += thisSend;
			g_socketBandwidth.periodStats.m_fragmentPacketsSent++;
			g_socketBandwidth.bandwidthStats.m_total.m_fragmentBandwidthSent += thisSend;
			g_socketBandwidth.bandwidthStats.m_total.m_fragmentPacketsSent++;
		}
		else
		{
			if ((headerType >= eH_TransportSeq0) && (headerType <= eH_SyncTransportSeq_Last))
			{
				g_socketBandwidth.periodStats.m_seqBandwidthSent += thisSend;
				g_socketBandwidth.periodStats.m_seqPacketsSent++;
				g_socketBandwidth.bandwidthStats.m_total.m_seqBandwidthSent += thisSend;
				g_socketBandwidth.bandwidthStats.m_total.m_seqPacketsSent++;
			}
		}
	}
}

#endif

bool CreateSocketIOManager(int ncpus, ISocketIOManager** ppExternal, ISocketIOManager** ppInternal)
{
	bool created = false;

#if defined(HAS_SOCKETIOMANAGER_IOCP)
	if (!created)
	{
		if (ncpus >= 2 || gEnv->IsDedicated())
		{
			CSocketIOManagerIOCP* pMgrIOCP = new CSocketIOManagerIOCP();
			if ((pMgrIOCP != NULL) && (pMgrIOCP->Init() == true))
			{
				*ppInternal = pMgrIOCP;
				created = true;
			}
			else
			{
				created = false;
			}
		}
	}
#endif // defined(HAS_SOCKETIOMANAGER_IOCP)
#if defined(HAS_SOCKETIOMANAGER_DURANGO)
	if (!created)
	{
		CSocketIOManagerDurango* pMgrDurango = new CSocketIOManagerDurango();
		if ((pMgrDurango != NULL) && (pMgrDurango->Init() == true))
		{
			*ppInternal = pMgrDurango;
			created = true;
		}
		else
		{
			created = false;
		}
	}
#endif // defined(HAS_SOCKETIOMANAGER_DURANGO)
#if defined(HAS_SOCKETIOMANAGER_SELECT)
	if (!created)
	{
		CSocketIOManagerSelect* pMgrSelect = new CSocketIOManagerSelect();
		if ((pMgrSelect != NULL) && (pMgrSelect->Init() == true))
		{
			*ppInternal = pMgrSelect;
			created = true;
		}
		else
		{
			created = false;
		}
	}
#endif // defined(HAS_SOCKETIOMANAGER_SELECT)
	if (!created)
	{
		*ppInternal = new CSocketIOManagerNull();
		if (*ppInternal != NULL)
		{
			created = true;
		}
	}

#if defined(HAS_SOCKETIOMANAGER_LOBBYIDADDR)
	if (created)
	{
		// If we created the internal socket manager OK, then create the external one
		CSocketIOManagerLobbyIDAddr* pMgrLobbyAddrID = new CSocketIOManagerLobbyIDAddr();
		if ((pMgrLobbyAddrID != NULL) && (pMgrLobbyAddrID->Init() == true))
		{
			*ppExternal = pMgrLobbyAddrID;
			created = true;
		}
		else
		{
			created = false;
		}
	}
#else
	// Games not using LobbyAddrID don't need the distinction between internal and external socket managers
	*ppExternal = *ppInternal;
#endif // defined(HAS_SOCKETIOMANAGER_LOBBYIDADDR)

	if (!created)
	{
		if (*ppExternal != NULL)
		{
			delete *ppExternal;
			*ppExternal = NULL;
		}

		if (*ppInternal != NULL)
		{
			delete *ppInternal;
			*ppInternal = NULL;
		}
	}

	return created;
}
