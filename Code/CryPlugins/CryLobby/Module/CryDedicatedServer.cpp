// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryDedicatedServer.h"
#include "CrySharedLobbyPacket.h"

#if USE_CRY_DEDICATED_SERVER

	#define FREE_SELF_TIME                        30000
	#define DEDICATED_SERVER_FIRST_FREE_SEND_TIME 5000

CCryDedicatedServer::CCryDedicatedServer(CCryLobby* pLobby, CCryLobbyService* pService, ECryLobbyService serviceType) : CCryMatchMaking(pLobby, pService, serviceType)
{
	// Make the CCryMatchMaking base pointers point to our data so we can use the common code in CCryMatchMaking
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		CCryMatchMaking::m_sessions[i] = &m_sessions[i];
		CCryMatchMaking::m_sessions[i]->localConnection = &m_sessions[i].localConnection;

		for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
		{
			CCryMatchMaking::m_sessions[i]->remoteConnection[j] = &m_sessions[i].remoteConnection[j];
		}
	}

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		CCryMatchMaking::m_task[i] = &m_task[i];
	}

	m_allocatedSession = CryLobbyInvalidSessionHandle;
	m_pLobby = pLobby;
}

void CCryDedicatedServer::StartArbitratorNameLookup()
{
	CNetAddressResolver* pResolver = CCryLobby::GetResolver();
	CLobbyCVars& cvars = CLobbyCVars::Get();

	if (pResolver)
	{
		string address;

		if (cvars.pDedicatedServerArbitratorIP && cvars.pDedicatedServerArbitratorIP->GetString())
		{
			if (strlen(cvars.pDedicatedServerArbitratorIP->GetString()))
			{
				address.Format("%s:%d", cvars.pDedicatedServerArbitratorIP->GetString(), cvars.dedicatedServerArbitratorPort);
				NetLog("Starting name resolution for dedicated server arbitrator '%s'", address.c_str());

				m_pNameReq = pResolver->RequestNameLookup(address);
			}
		}
	}
}

ECryLobbyError CCryDedicatedServer::Initialise()
{
	ECryLobbyError error = CCryMatchMaking::Initialise();

	if (error == eCLE_Success)
	{
		StartArbitratorNameLookup();
	}

	return error;
}

ECryLobbyError CCryDedicatedServer::Terminate()
{
	ECryLobbyError error = CCryMatchMaking::Terminate();

	if (m_allocatedSession != CryLobbyInvalidSessionHandle)
	{
		FreeSessionHandle(m_allocatedSession);
		m_allocatedSession = CryLobbyInvalidSessionHandle;
	}

	return error;
}

uint64 CCryDedicatedServer::GetSIDFromSessionHandle(CryLobbySessionHandle h)
{
	CRY_ASSERT_MESSAGE((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED), "CCryLANMatchMaking::GetSIDFromSessionHandle: invalid session handle");

	return m_sessions[h].sid;
}

TNetAddress CCryDedicatedServer::GetHostAddressFromSessionHandle(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	return TNetAddress(TLocalNetAddress(m_lobby->GetInternalSocketPort(eCLS_LAN)));
}

CryMatchMakingConnectionID CCryDedicatedServer::AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, uint32 numUsers)
{
	CryMatchMakingConnectionID id = CCryMatchMaking::AddRemoteConnection(h, connectionID, uid, numUsers);

	if (id != CryMatchMakingInvalidConnectionID)
	{
		m_sessions[h].remoteConnection[id].gameInformedConnectionEstablished = true;
	}

	return id;
}

void CCryDedicatedServer::TickArbitratorNameLookup(CTimeValue tv)
{
	ENameRequestResult result = m_pNameReq->GetResult();
	CLobbyCVars& cvars = CLobbyCVars::Get();

	switch (result)
	{
	case eNRR_Pending:
		break;

	case eNRR_Succeeded:
		NetLog("Name resolution for dedicated server arbitrator '%s:%d' succeeded", cvars.pDedicatedServerArbitratorIP->GetString(), cvars.dedicatedServerArbitratorPort);
		m_arbitratorAddr = m_pNameReq->GetAddrs()[0];
		m_pNameReq = NULL;
		m_arbitratorNextSendTime.SetMilliSeconds(g_time.GetMilliSecondsAsInt64() + DEDICATED_SERVER_FIRST_FREE_SEND_TIME);
		break;

	case eNRR_Failed:
		NetLog("Name resolution for dedicated server arbitrator '%s:%d' failed", cvars.pDedicatedServerArbitratorIP->GetString(), cvars.dedicatedServerArbitratorPort);
		m_pNameReq = NULL;
		break;
	}
}

void CCryDedicatedServer::TickFree(CTimeValue tv)
{
	if (tv > m_arbitratorNextSendTime)
	{
		const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize;
		uint8 buffer[MaxBufferSize];
		CCrySharedLobbyPacket packet;

		packet.SetWriteBuffer(buffer, MaxBufferSize);
		packet.StartWrite(eLobbyPT_D2A_DedicatedServerIsFree, false);

		m_pLobby->Send(&packet, m_arbitratorAddr, CryLobbyInvalidConnectionID, NULL);

		m_arbitratorNextSendTime.SetMilliSeconds(tv.GetMilliSecondsAsInt64() + DEDICATED_SERVER_IS_FREE_SEND_INTERVAL);
	}
}

void CCryDedicatedServer::TickAllocated(CTimeValue tv)
{
	SSession* pSession = &m_sessions[m_allocatedSession];
	bool haveConnections = false;

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

		if (pConnection->used)
		{
			if (pConnection->connectionID != CryLobbyInvalidConnectionID)
			{
				ECryLobbyConnectionState connectionState = m_pLobby->ConnectionGetState(pConnection->connectionID);

				if ((connectionState == eCLCS_Pending) || (connectionState == eCLCS_Connected))
				{
					pSession->checkFreeStartTime = tv;
					haveConnections = true;
					break;
				}
			}
		}
	}

	if (!haveConnections && ((tv - pSession->checkFreeStartTime).GetMilliSecondsAsInt64() > FREE_SELF_TIME))
	{
		FreeDedicatedServer();
	}
}

void CCryDedicatedServer::Tick(CTimeValue tv)
{
	CCryMatchMaking::Tick(tv);

	if (m_pNameReq)
	{
		TickArbitratorNameLookup(tv);
	}
	else
	{
		if (m_allocatedSession == CryLobbyInvalidSessionHandle)
		{
			TickFree(tv);
		}
		else
		{
			TickAllocated(tv);
		}
	}
}

void CCryDedicatedServer::ProcessDedicatedServerIsFreeAck(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
}

void CCryDedicatedServer::DispatchUserPacket(TMemHdl mh, uint32 length)
{
	CCryMatchMaking::DispatchUserPacket(mh, length, CrySessionInvalidHandle);
}

void CCryDedicatedServer::ProcessAllocateDedicatedServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();

	SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
	uint32 clientIP = pPacket->ReadUINT32();
	uint16 clientPort = pPacket->ReadUINT16();
	uint32 clientCookie = pPacket->ReadUINT32();
	TNetAddress clientAddr(SIPv4Addr(clientIP, clientPort));

	uint8 userPacketType = pPacket->ReadUINT8();
	uint16 userDataSize = pPacket->ReadUINT16();
	uint32 writeUserPacketSize = CryLobbyPacketUnReliableHeaderSize + userDataSize;
	CCryLobbyPacket userPacket;
	bool acceptRequest = false;

	if (m_allocatedSession == CryLobbyInvalidSessionHandle)
	{
		// Currently not allocated so accept request, create a session and send the user packet to game.
		if (userPacket.CreateWriteBuffer(writeUserPacketSize))
		{
			uint32 createFlags = CRYSESSION_CREATE_FLAG_NUB;
	#if NETWORK_HOST_MIGRATION
			createFlags |= CRYSESSION_CREATE_FLAG_MIGRATABLE;
	#endif
			if (CreateSessionHandle(&m_allocatedSession, true, createFlags, 0) == eCLE_Success)
			{
				SSession* pSession = &m_sessions[m_allocatedSession];

				pSession->localConnection.uid = uid;

				pSession->sid = uid.m_sid;
				pSession->allocToAddr = clientAddr;
				pSession->cookie = clientCookie;
				pSession->checkFreeStartTime = g_time;

				NetLog("Received allocate server from arbitrator " PRFORMAT_ADDR " for client " PRFORMAT_ADDR " cookie %x informing arbitrator accepted", PRARG_ADDR(addr), PRARG_ADDR(clientAddr), clientCookie);

				acceptRequest = true;
				userPacket.StartWrite(userPacketType, false);

				if (userDataSize > 0)
				{
					userPacket.WriteData(pPacket->GetReadBuffer() + pPacket->GetReadBufferPos(), userDataSize);
				}

				uint32 length;
				TMemHdl mh = PacketToMemoryBlock(&userPacket, length);
				TO_GAME_FROM_LOBBY(&CCryDedicatedServer::DispatchUserPacket, this, mh, length);
			}

			userPacket.FreeWriteBuffer();
		}
	}
	else
	{
		// Currently allocated so if this request is for our current allocation send accept else send refuse.
		SSession* pSession = &m_sessions[m_allocatedSession];

		if ((pSession->sid == uid.m_sid) && (pSession->allocToAddr == clientAddr) && (pSession->cookie == clientCookie))
		{
			NetLog("Received allocate server from arbitrator " PRFORMAT_ADDR " for client " PRFORMAT_ADDR " cookie %x informing arbitrator accepted", PRARG_ADDR(addr), PRARG_ADDR(clientAddr), clientCookie);
			pSession->checkFreeStartTime = g_time;
			acceptRequest = true;
		}
		else
		{
			NetLog("Received allocate server from arbitrator " PRFORMAT_ADDR " for client " PRFORMAT_ADDR " cookie %x but already allocated to client " PRFORMAT_ADDR " cookie %x informing arbitrator not accepted", PRARG_ADDR(addr), PRARG_ADDR(clientAddr), clientCookie, PRARG_ADDR(pSession->allocToAddr), pSession->cookie);
		}
	}

	const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize + CryLobbyPacketBoolSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size + CryLobbyPacketUINT32Size;
	uint8 buffer[MaxBufferSize];
	CCrySharedLobbyPacket packet;

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eLobbyPT_D2A_AllocateDedicatedServerAck, false);
	packet.WriteBool(acceptRequest);
	packet.WriteConnectionUID(uid);
	packet.WriteUINT32(clientIP);
	packet.WriteUINT16(clientPort);
	packet.WriteUINT32(clientCookie);

	m_pLobby->Send(&packet, addr, CryLobbyInvalidConnectionID, NULL);
}

void CCryDedicatedServer::DispatchFreeDedicatedServer()
{
	UCryLobbyEventData eventData;
	memset(&eventData, 0, sizeof(eventData));
	m_pLobby->DispatchEvent(eCLSE_DedicatedServerRelease, eventData);
}

void CCryDedicatedServer::FreeDedicatedServer()
{
	SSession* pSession = &m_sessions[m_allocatedSession];

	// Disconnect our local connection
	SessionDisconnectRemoteConnectionViaNub(m_allocatedSession, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_UserRequested, "Session deleted");

	// Free any remaining remote connections
	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

		if (pConnection->used)
		{
			FreeRemoteConnection(m_allocatedSession, i);
		}
	}

	FreeSessionHandle(m_allocatedSession);
	m_allocatedSession = CryLobbyInvalidSessionHandle;
	m_arbitratorNextSendTime.SetMilliSeconds(g_time.GetMilliSecondsAsInt64() + DEDICATED_SERVER_FIRST_FREE_SEND_TIME);
	TO_GAME_FROM_LOBBY(&CCryDedicatedServer::DispatchFreeDedicatedServer, this);
}

void CCryDedicatedServer::ProcessFreeDedicatedServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	if (m_allocatedSession != CryLobbyInvalidSessionHandle)
	{
		FreeDedicatedServer();
	}
}

void CCryDedicatedServer::ProcessSessionRequestJoinServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	SCryMatchMakingConnectionUID uid;

	pPacket->StartRead();
	uid = pPacket->GetFromConnectionUID();

	if (m_allocatedSession != CryLobbyInvalidSessionHandle)
	{
		CryLobbyConnectionID c;

		if (m_pLobby->ConnectionFromAddress(&c, addr))
		{
			SSession* pSession = &m_sessions[m_allocatedSession];

			if (pSession->sid == uid.m_sid)
			{
				CryMatchMakingConnectionID id = AddRemoteConnection(m_allocatedSession, c, uid, 0);

				if (id != CryMatchMakingInvalidConnectionID)
				{
					bool host = pPacket->ReadBool();

					if (host)
					{
						pSession->hostConnectionID = id;
					}

					// Send back accept
					const uint32 MaxBufferSize = CryLobbyPacketReliableHeaderSize + CryLobbyPacketBoolSize + CryLobbyPacketConnectionUIDSize;
					uint8 buffer[MaxBufferSize];
					CCrySharedLobbyPacket packet;

					packet.SetWriteBuffer(buffer, MaxBufferSize);
					packet.StartWrite(eLobbyPT_SessionRequestJoinServerResult, true);
					packet.WriteBool(true);
					packet.WriteConnectionUID(uid);

					Send(CryMatchMakingInvalidTaskID, &packet, m_allocatedSession, id);

					return;
				}
				else
				{
					NetLog("refusing join request from " PRFORMAT_ADDR " failed to add connection", PRARG_ADDR(addr));
				}
			}
			else
			{
				NetLog("refusing join request from " PRFORMAT_ADDR " session sid %llx does not match connection sid %llx", PRARG_ADDR(addr), pSession->sid, uid.m_sid);
			}
		}
		else
		{
			NetLog("refusing join request from " PRFORMAT_ADDR " failed to get connection from address", PRARG_ADDR(addr));
		}
	}
	else
	{
		NetLog("refusing join request from " PRFORMAT_ADDR " we're not in a session", PRARG_ADDR(addr));
	}

	// Send back fail
	const uint32 MaxBufferSize = CryLobbyPacketReliableHeaderSize + CryLobbyPacketBoolSize + CryLobbyPacketConnectionUIDSize;
	uint8 buffer[MaxBufferSize];
	CCrySharedLobbyPacket packet;

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eLobbyPT_SessionRequestJoinServerResult, true);
	packet.WriteBool(false);
	packet.WriteConnectionUID(uid);

	Send(CryMatchMakingInvalidTaskID, &packet, m_allocatedSession, addr);
}

void CCryDedicatedServer::ProcessUpdateSessionID(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	if (m_allocatedSession != CryLobbyInvalidSessionHandle)
	{
		SCryMatchMakingConnectionUID fromUID = pPacket->GetFromConnectionUID();
		CryMatchMakingConnectionID connID = CryMatchMakingInvalidConnectionID;

		if (FindConnectionFromSessionUID(m_allocatedSession, fromUID, &connID))
		{
			SSession* pSession = &m_sessions[m_allocatedSession];
			if (pSession->hostConnectionID == connID)
			{
				pPacket->StartRead();
				SCryMatchMakingConnectionUID newUID = pPacket->ReadConnectionUID();

				// update sid on session and connections
				uint64 newSID = newUID.m_sid;

				pSession->sid = newSID;
				pSession->localConnection.uid.m_sid = newSID;
	#if NETWORK_HOST_MIGRATION
				pSession->newHostUID.m_sid = newSID;
	#endif

				for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
				{
					SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

					if (pConnection->used)
					{
						pConnection->uid.m_sid = newSID;
					}
				}
			}
		}
	}
}

	#if NETWORK_HOST_MIGRATION
void CCryDedicatedServer::HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause)
{
	NetLog("HostMigrationInitialise " PRFORMAT_SH " cause %d", PRARG_SH(h), cause);

	if (m_allocatedSession != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[m_allocatedSession];
		CryMatchMakingConnectionID connID = CryMatchMakingInvalidConnectionID;

		for (int i = 0; i < pSession->newHostPriorityCount; ++i)
		{
			CCryMatchMaking::SSession::SRConnection* pConnection = pSession->newHostPriority[i];
			for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; ++connectionIndex)
			{
				CCryMatchMaking::SSession::SRConnection* pRemoteConnection = &pSession->remoteConnection[connectionIndex];
				if (pConnection == pRemoteConnection)
				{
					if (connectionIndex != pSession->hostConnectionID)
					{
						connID = connectionIndex;
					}
					break;
				}
			}

			if (connID != CryMatchMakingInvalidConnectionID)
			{
				// found a valid id, stop searching
				break;
			}
		}

		if (connID != CryMatchMakingInvalidConnectionID)
		{
			// set as new host
			pSession->hostConnectionID = connID;

			// tells clients who new host should be
			const uint32 maxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize;
			uint8 buffer[maxBufferSize];
			CCryLobbyPacket packet;

			packet.SetWriteBuffer(buffer, maxBufferSize);
			packet.StartWrite(eLobbyPT_D2C_HostMigrationStart, true);
			packet.WriteConnectionUID(pSession->remoteConnection[connID].uid);

			SendToAll(CryMatchMakingInvalidTaskID, &packet, m_allocatedSession, CryMatchMakingInvalidConnectionID);

			NetLog("[Host Migration]: selected client " PRFORMAT_SHMMCINFO " as new host", PRARG_SHMMCINFO(m_allocatedSession, connID, pSession->remoteConnection[connID].connectionID, pSession->remoteConnection[connID].uid));
		}
		else
		{
			FreeDedicatedServer();
		}
	}
	else
	{
		NetLog("[Host Migration]: trying to initialise host migration but we don't have a valid session");
	}
}
	#endif // NETWORK_HOST_MIGRATION

void CCryDedicatedServer::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;

	if (addr == m_arbitratorAddr)
	{
		m_arbitratorLastRecvTime = g_time;
	}

	switch (pSPacket->StartRead())
	{
	case eLobbyPT_A2D_DedicatedServerIsFreeAck:
		ProcessDedicatedServerIsFreeAck(addr, pSPacket);
		break;

	case eLobbyPT_A2D_AllocateDedicatedServer:
		ProcessAllocateDedicatedServer(addr, pSPacket);
		break;

	case eLobbyPT_A2D_FreeDedicatedServer:
		ProcessFreeDedicatedServer(addr, pSPacket);
		break;

	case eLobbyPT_SessionRequestJoinServer:
		ProcessSessionRequestJoinServer(addr, pSPacket);
		break;

	case eLobbyPT_C2D_UpdateSessionID:
		ProcessUpdateSessionID(addr, pSPacket);
		break;

	default:
		CCryMatchMaking::OnPacket(addr, pSPacket);
		break;
	}
}

void CCryDedicatedServer::SSession::Reset()
{
	PARENT::Reset();

	//	uint64												sid;
	//	TNetAddress										allocToAddr;
	//	CTimeValue										checkFreeStartTime;
	//	uint32												cookie;
	//	SLConnection									localConnection;
	//	CryLobbyIDArray<SRConnection, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS>	remoteConnection;
}

#endif // USE_CRY_DEDICATED_SERVER
