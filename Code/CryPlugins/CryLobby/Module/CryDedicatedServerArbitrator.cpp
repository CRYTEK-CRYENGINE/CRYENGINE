// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryDedicatedServerArbitrator.h"
#include "CryDedicatedServer.h"
#include "CrySharedLobbyPacket.h"

#if USE_CRY_DEDICATED_SERVER_ARBITRATOR

	#define DEDICATED_SERVER_ALLOCATE_RESPONSE_TIMEOUT 5000
	#define FREE_DEDICATED_SERVER_SEND_INTERVAL        1000
	#define FREE_DEDICATED_SERVER_MAX_SENDS            10

CCryDedicatedServerArbitrator::CCryDedicatedServerArbitrator(CCryLobby* pLobby, CCryLobbyService* pService)
{
	CRY_ASSERT_MESSAGE(pLobby, "CCryDedicatedServerArbitrator::CCryDedicatedServerArbitrator: Lobby not specified");
	CRY_ASSERT_MESSAGE(pService, "CCryDedicatedServerArbitrator::CCryDedicatedServerArbitrator: Service not specified");

	m_pLobby = pLobby;
	m_pService = pService;
}

ECryLobbyError CCryDedicatedServerArbitrator::Initialise()
{
	return eCLE_Success;
}

ECryLobbyError CCryDedicatedServerArbitrator::Terminate()
{
	return eCLE_Success;
}

void CCryDedicatedServerArbitrator::Tick(CTimeValue tv)
{
	for (TFreeingServersMap::iterator iterFreeing = m_freeingServers.begin(); iterFreeing != m_freeingServers.end(); )
	{
		TFreeingServersMap::iterator nextFreeing = iterFreeing;

		++nextFreeing;

		if ((tv - iterFreeing->second.lastSendTime).GetMilliSecondsAsInt64() > FREE_DEDICATED_SERVER_SEND_INTERVAL)
		{
			if (iterFreeing->second.sendCount < FREE_DEDICATED_SERVER_MAX_SENDS)
			{
				const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize;
				uint8 buffer[MaxBufferSize];
				CCrySharedLobbyPacket packet;

				packet.SetWriteBuffer(buffer, MaxBufferSize);
				packet.StartWrite(eLobbyPT_A2D_FreeDedicatedServer, false);

				m_pLobby->Send(&packet, iterFreeing->first, CryLobbyInvalidConnectionID, NULL);
				iterFreeing->second.lastSendTime = tv;
				iterFreeing->second.sendCount++;
			}
			else
			{
				// The server hasn't said it's free so just give up.
				// The server will add itself to m_freeServers when it is ready.
				m_freeingServers.erase(iterFreeing->first);
			}
		}

		iterFreeing = nextFreeing;
	}
}

CCryDedicatedServerArbitrator::TFreeServersMap::iterator CCryDedicatedServerArbitrator::AddToFreeServers(const TNetAddress& serverPrivateAddr, const TNetAddress& serverPublicAddr)
{
	TFreeServersMap::iterator iterFree = m_freeServers.find(serverPrivateAddr);

	if (iterFree == m_freeServers.end())
	{
		const TNetAddress serverPrivateAddrSave = serverPrivateAddr;
		const TNetAddress serverPublicAddrSave = serverPublicAddr;

		// Check if server is currently allocated.
		for (TAllocatedServersMap::iterator iterAllocated = m_allocatedServers.begin(); iterAllocated != m_allocatedServers.end(); ++iterAllocated)
		{
			if (iterAllocated->second.serverPrivateAddr == serverPrivateAddrSave)
			{
				if (iterAllocated->second.serverReady)
				{
					// The server has been allocated and we have received eLobbyPT_D2A_AllocateDedicatedServerAck.
					// The server should be removed from allocated and continue with freeing.
					m_allocatedServers.erase(iterAllocated);
					break;
				}
				else
				{
					// The server has been allocated but we haven't received eLobbyPT_D2A_AllocateDedicatedServerAck yet.
					// All frees should be ignored until eLobbyPT_D2A_AllocateDedicatedServerAck has been received or the allocate attempt has timed out.
					return m_freeServers.end();
				}
			}
		}

		// Make sure not still in freeing.
		TFreeingServersMap::iterator iterFreeing = m_freeingServers.find(serverPrivateAddrSave);

		if (iterFreeing != m_freeingServers.end())
		{
			m_freeingServers.erase(iterFreeing->first);
		}

		// Add to free.
		iterFree = m_freeServers.insert(std::make_pair(serverPrivateAddrSave, SFreeServerData())).first;

		if (iterFree != m_freeServers.end())
		{
			iterFree->second.serverPublicAddr = serverPublicAddrSave;
		}
	}

	if (iterFree != m_freeServers.end())
	{
		iterFree->second.lastRecvTime = g_time;
	}

	return iterFree;
}

CCryDedicatedServerArbitrator::TAllocatedServersMap::iterator CCryDedicatedServerArbitrator::AddToAllocatedServers(uint64 sid, const TNetAddress& serverPrivateAddr, const TNetAddress& serverPublicAddr, uint32 clientCookie, uint8 clientTaskID)
{
	TAllocatedServersMap::iterator iterAllocated = m_allocatedServers.find(sid);

	if (iterAllocated == m_allocatedServers.end())
	{
		const TNetAddress serverPrivateAddrSave = serverPrivateAddr;
		const TNetAddress serverPublicAddrSave = serverPublicAddr;

		// Make sure not still in free.
		TFreeServersMap::iterator iterFree = m_freeServers.find(serverPrivateAddrSave);

		if (iterFree != m_freeServers.end())
		{
			m_freeServers.erase(iterFree->first);
		}

		// Make sure not still in freeing.
		TFreeingServersMap::iterator iterFreeing = m_freeingServers.find(serverPrivateAddrSave);

		if (iterFreeing != m_freeingServers.end())
		{
			m_freeingServers.erase(iterFreeing->first);
		}

		// Add to allocated.
		iterAllocated = m_allocatedServers.insert(std::make_pair(sid, SAllocatedServerData())).first;

		if (iterAllocated != m_allocatedServers.end())
		{
			iterAllocated->second.allocTime = g_time;
			iterAllocated->second.serverPrivateAddr = serverPrivateAddrSave;
			iterAllocated->second.serverPublicAddr = serverPublicAddrSave;
			iterAllocated->second.clientCookie = clientCookie;
			iterAllocated->second.clientTaskID = clientTaskID;
			iterAllocated->second.serverReady = false;
		}
	}

	return iterAllocated;
}

CCryDedicatedServerArbitrator::TFreeingServersMap::iterator CCryDedicatedServerArbitrator::AddToFreeingServers(const TNetAddress& serverPrivateAddr)
{
	TFreeServersMap::iterator iterFree = m_freeServers.find(serverPrivateAddr);

	if (iterFree == m_freeServers.end())
	{
		const TNetAddress serverPrivateAddrSave = serverPrivateAddr;

		// Only need to try and free if not already free.

		// Make sure not still in allocated.
		for (TAllocatedServersMap::iterator iterAllocated = m_allocatedServers.begin(); iterAllocated != m_allocatedServers.end(); ++iterAllocated)
		{
			if (iterAllocated->second.serverPrivateAddr == serverPrivateAddrSave)
			{
				m_allocatedServers.erase(iterAllocated);
				break;
			}
		}

		TFreeingServersMap::iterator iterFreeing = m_freeingServers.find(serverPrivateAddrSave);

		if (iterFreeing == m_freeingServers.end())
		{
			// Add to freeing
			iterFreeing = m_freeingServers.insert(std::make_pair(serverPrivateAddrSave, SFreeingServerData())).first;
			iterFreeing->second.lastSendTime = CTimeValue();
			iterFreeing->second.sendCount = 0;
		}

		return iterFreeing;
	}

	return m_freeingServers.end();
}

void CCryDedicatedServerArbitrator::ProcessDedicatedServerIsFree(const TNetAddress& serverPrivateAddr, CCrySharedLobbyPacket* pPacket)
{
	if (AddToFreeServers(serverPrivateAddr, serverPrivateAddr) != m_freeServers.end())
	{
		const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize;
		uint8 buffer[MaxBufferSize];
		CCrySharedLobbyPacket packet;

		packet.SetWriteBuffer(buffer, MaxBufferSize);
		packet.StartWrite(eLobbyPT_A2D_DedicatedServerIsFreeAck, false);

		m_pLobby->Send(&packet, serverPrivateAddr, CryLobbyInvalidConnectionID, NULL);
	}
}

void CCryDedicatedServerArbitrator::SendRequestSetupDedicatedServerResult(bool success, uint32 clientTaskID, uint32 clientCookie, const TNetAddress& clientAddr, const TNetAddress& serverPublicAddr, SCryMatchMakingConnectionUID serverUID)
{
	const SIPv4Addr* pIPv4Addr = stl::get_if<SIPv4Addr>(&serverPublicAddr);
	const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize + CryLobbyPacketBoolSize + CryLobbyPacketUINT8Size + CryLobbyPacketUINT32Size + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size;
	uint8 buffer[MaxBufferSize];
	CCrySharedLobbyPacket packet;

	if (pIPv4Addr == NULL)
	{
		success = false;
	}

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eLobbyPT_A2C_RequestSetupDedicatedServerResult, false);
	packet.WriteBool(success);
	packet.WriteUINT8(clientTaskID);
	packet.WriteUINT32(clientCookie);

	if (success)
	{
		packet.WriteConnectionUID(serverUID);
		packet.WriteUINT32(pIPv4Addr->addr);
		packet.WriteUINT16(pIPv4Addr->port);
	}

	m_pLobby->Send(&packet, clientAddr, CryLobbyInvalidConnectionID, NULL);
}

void CCryDedicatedServerArbitrator::ProcessRequestSetupDedicatedServer(const TNetAddress& clientAddr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();
	SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
	uint32 clientCookie = pPacket->ReadUINT32();
	uint8 clientTaskID = pPacket->ReadUINT8();

	TAllocatedServersMap::iterator iterAllocated = m_allocatedServers.find(uid.m_sid);

	if (iterAllocated != m_allocatedServers.end())
	{
		// We have a server allocated to this client but it was for a different request.
		if ((iterAllocated->second.clientCookie != clientCookie) ||
		    // We have a server allocated to this client but it hasn't responded to the request.
		    ((g_time - iterAllocated->second.allocTime).GetMilliSecondsAsInt64() > DEDICATED_SERVER_ALLOCATE_RESPONSE_TIMEOUT))
		{
			AddToFreeingServers(iterAllocated->second.serverPrivateAddr);
			iterAllocated = m_allocatedServers.end();
		}
	}

	if (iterAllocated == m_allocatedServers.end())
	{
		// No servers allocated for this clients request so find a free one.
		while (true)
		{
			TFreeServersMap::iterator iterFree = m_freeServers.begin();

			if (iterFree != m_freeServers.end())
			{
				if ((g_time - iterFree->second.lastRecvTime).GetMilliSecondsAsInt64() < DEDICATED_SERVER_IS_FREE_SEND_INTERVAL)
				{
					// Lets try this server
					iterAllocated = AddToAllocatedServers(uid.m_sid, iterFree->first, iterFree->second.serverPublicAddr, clientCookie, clientTaskID);
					break;
				}
				else
				{
					// This server hasn't said it is free for a while so remove it.
					// It will get added to m_freeServers again when it finally does say it is free.
					m_freeServers.erase(iterFree->first);
				}
			}
			else
			{
				// No free servers return failure to client.
				NetLog("Received request for server from " PRFORMAT_ADDR " session %" PRIx64 " cookie %x No free servers informing client", PRARG_ADDR(clientAddr), uid.m_sid, clientCookie);
				SendRequestSetupDedicatedServerResult(false, clientTaskID, clientCookie, clientAddr, TNetAddress(), uid);
				return;
			}
		}
	}

	if (iterAllocated != m_allocatedServers.end())
	{
		if (iterAllocated->second.serverReady)
		{
			// Server is ready return success to client.
			NetLog("Received request for server from " PRFORMAT_ADDR " session %" PRIx64 " cookie %x Allocated server " PRFORMAT_ADDR " Server ready informing client", PRARG_ADDR(clientAddr), uid.m_sid, clientCookie, PRARG_ADDR(iterAllocated->second.serverPublicAddr));
			SendRequestSetupDedicatedServerResult(true, clientTaskID, clientCookie, clientAddr, iterAllocated->second.serverPublicAddr, uid);
		}
		else
		{
			// The server isn't ready so tell it it has been allocated.
			const SIPv4Addr* pIPv4Addr = stl::get_if<SIPv4Addr>(&clientAddr);

			if (pIPv4Addr)
			{
				uint8 userPacketType = pPacket->ReadUINT8();
				uint16 userDataSize = pPacket->ReadUINT16();
				uint32 writePacketSize = CryLobbyPacketUnReliableHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size + CryLobbyPacketUINT16Size + userDataSize;
				CCrySharedLobbyPacket packet;

				if (packet.CreateWriteBuffer(writePacketSize))
				{
					NetLog("Received request for server from " PRFORMAT_ADDR " session %" PRIx64 " cookie %x Allocated server " PRFORMAT_ADDR " Server not ready informing server", PRARG_ADDR(clientAddr), uid.m_sid, clientCookie, PRARG_ADDR(iterAllocated->second.serverPublicAddr));

					packet.StartWrite(eLobbyPT_A2D_AllocateDedicatedServer, false);
					packet.WriteConnectionUID(uid);
					packet.WriteUINT32(pIPv4Addr->addr);
					packet.WriteUINT16(pIPv4Addr->port);
					packet.WriteUINT32(clientCookie);
					packet.WriteUINT8(userPacketType);
					packet.WriteUINT16(userDataSize);

					if (userDataSize > 0)
					{
						packet.WriteData(pPacket->GetReadBuffer() + pPacket->GetReadBufferPos(), userDataSize);
					}

					m_pLobby->Send(&packet, iterAllocated->second.serverPrivateAddr, CryLobbyInvalidConnectionID, NULL);

					packet.FreeWriteBuffer();
				}
			}
		}
	}
}

void CCryDedicatedServerArbitrator::ProcessAllocateDedicatedServerAck(const TNetAddress& serverPrivateAddr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();
	bool accepted = pPacket->ReadBool();
	SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
	uint32 clientIP = pPacket->ReadUINT32();
	uint16 clientPort = pPacket->ReadUINT16();
	uint32 clientCookie = pPacket->ReadUINT32();
	TNetAddress clientAddr(SIPv4Addr(clientIP, clientPort));
	TAllocatedServersMap::iterator iterAllocated = m_allocatedServers.find(uid.m_sid);

	if (iterAllocated != m_allocatedServers.end())
	{
		if (accepted)
		{
			if ((iterAllocated->second.clientCookie == clientCookie) && (iterAllocated->second.serverPrivateAddr == serverPrivateAddr))
			{
				// The server is ready so send success to the client.
				NetLog("Received server ready from " PRFORMAT_ADDR " informing client " PRFORMAT_ADDR " session %" PRIx64 " cookie %x", PRARG_ADDR(iterAllocated->second.serverPublicAddr), PRARG_ADDR(clientAddr), uid.m_sid, clientCookie);
				iterAllocated->second.serverReady = true;
				SendRequestSetupDedicatedServerResult(true, iterAllocated->second.clientTaskID, clientCookie, clientAddr, iterAllocated->second.serverPublicAddr, uid);
			}
			else
			{
				// The server is ready but for an out of date request so tell the server it can become free again.
				AddToFreeingServers(serverPrivateAddr);
			}
		}
		else
		{
			// The request wasn't accepted so remove this allocation.
			// When the server becomes ready to accept requests again it will re add itself to the free list.
			// The client will be allocated another server when the next request comes in.
			m_allocatedServers.erase(iterAllocated);
		}
	}
	else
	{
		// The server is ready but for an out of date request so tell the server it can become free again.
		AddToFreeingServers(serverPrivateAddr);
	}
}

void CCryDedicatedServerArbitrator::ProcessRequestReleaseDedicatedServer(const TNetAddress& clientAddr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();
	uint64 sid = pPacket->ReadUINT64();
	uint32 clientCookie = pPacket->ReadUINT32();
	uint8 clientTaskID = pPacket->ReadUINT8();

	TAllocatedServersMap::iterator iterAllocated = m_allocatedServers.find(sid);

	if (iterAllocated != m_allocatedServers.end())
	{
		NetLog("Received request to release server " PRFORMAT_ADDR " from " PRFORMAT_ADDR " session %" PRIx64 " cookie %x", PRARG_ADDR(iterAllocated->second.serverPublicAddr), PRARG_ADDR(clientAddr), sid, clientCookie);
		AddToFreeingServers(iterAllocated->second.serverPrivateAddr);
	}

	const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize + CryLobbyPacketUINT8Size + CryLobbyPacketUINT32Size;
	uint8 buffer[MaxBufferSize];
	CCrySharedLobbyPacket packet;

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eLobbyPT_A2C_RequestReleaseDedicatedServerResult, false);
	packet.WriteUINT8(clientTaskID);
	packet.WriteUINT32(clientCookie);

	m_pLobby->Send(&packet, clientAddr, CryLobbyInvalidConnectionID, NULL);
}

void CCryDedicatedServerArbitrator::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;

	switch (pSPacket->StartRead())
	{
	case eLobbyPT_D2A_DedicatedServerIsFree:
		ProcessDedicatedServerIsFree(addr, pSPacket);
		break;

	case eLobbyPT_C2A_RequestSetupDedicatedServer:
		ProcessRequestSetupDedicatedServer(addr, pSPacket);
		break;

	case eLobbyPT_D2A_AllocateDedicatedServerAck:
		ProcessAllocateDedicatedServerAck(addr, pSPacket);
		break;

	case eLobbyPT_C2A_RequestReleaseDedicatedServer:
		ProcessRequestReleaseDedicatedServer(addr, pSPacket);
		break;
	}
}

void CCryDedicatedServerArbitrator::OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID)
{
}

void CCryDedicatedServerArbitrator::ListFreeServers()
{
	for (TFreeServersMap::const_iterator iter = m_freeServers.begin(); iter != m_freeServers.end(); ++iter)
	{
		NetLog(PRFORMAT_ADDR " TimeSinceRecv %" PRId64, PRARG_ADDR(iter->first), (g_time - iter->second.lastRecvTime).GetMilliSecondsAsInt64());
	}
}

#endif // USE_CRY_DEDICATED_SERVER_ARBITRATOR
