// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SocketIOManagerLobbyIDAddr.h"

#if defined(HAS_SOCKETIOMANAGER_LOBBYIDADDR)

	#include "Network.h"

CSocketIOManagerLobbyIDAddr::CSocketIOManagerLobbyIDAddr() : CSocketIOManager(eSIOMC_NoBuffering)
{
}

CSocketIOManagerLobbyIDAddr::~CSocketIOManagerLobbyIDAddr()
{
}

bool CSocketIOManagerLobbyIDAddr::Init()
{
	m_registeredSockets.push_back(SRegisteredSocket(222)); // 222 is not 123 (see below)
	m_registeredSockets[0].inUse = true;
	return true;
}

bool CSocketIOManagerLobbyIDAddr::PollWait(uint32 waitTime)
{
	bool haveData = false;
	m_recvSize = 0;

	ICryMatchMakingPrivate* pMMPrivate = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingPrivate() : nullptr;
	if (pMMPrivate)
	{
		haveData = pMMPrivate->LobbyAddrIDHasPendingData();
	}
	return haveData;
}

void CSocketIOManagerLobbyIDAddr::RecvPacket(void* privateRef, uint8* recvBuffer, uint32 recvSize, CRYSOCKET recvSocket, TNetAddress& recvAddr)
{
	CSocketIOManagerLobbyIDAddr* _this = (CSocketIOManagerLobbyIDAddr*)privateRef;
	CNetwork::Get()->ReportGotPacket();
	for (uint32 a = 0; a < _this->m_registeredSockets.size(); a++)
	{
		if (_this->m_registeredSockets[a].inUse && _this->m_registeredSockets[a].sock == recvSocket)
		{
			if (!_this->m_registeredSockets[a].pRecvFromTarget)
			{
				NetLog("CSocketIOManagerLobbyIDAddr::RecvPacket - socket matched for non registered recvFromTarget");
				return;
			}
			_this->m_registeredSockets[a].pRecvFromTarget->OnRecvFromComplete(recvAddr, recvBuffer, recvSize);
			return;
		}
	}

	NetLog("CSocketIOManagerLobbyIDAddr::RecvPacket - Packet recieved but no matching socket");
}

int CSocketIOManagerLobbyIDAddr::PollWork(bool& performedWork)
{
	ICryMatchMakingPrivate* pMMPrivate = gEnv->pLobby ? gEnv->pLobby->GetMatchMakingPrivate() : nullptr;
	if (pMMPrivate)
	{
		pMMPrivate->LobbyAddrIDRecv(RecvPacket, this);
	}

	return eSM_COMPLETEDIO;
}

SSocketID CSocketIOManagerLobbyIDAddr::RegisterSocket(CRYSOCKET sock, int protocol)
{
	ASSERT_GLOBAL_LOCK;
	SCOPED_COMM_LOCK;

	uint32 id;
	for (id = 0; id < m_registeredSockets.size(); id++)
	{
		if (!m_registeredSockets[id].inUse)
			break;
	}
	if (id == m_registeredSockets.size())
		m_registeredSockets.push_back(SRegisteredSocket(123)); // 123 is an arbitrary number that's somewhat larger than zero

	SRegisteredSocket& rs = m_registeredSockets[id];
	rs = SRegisteredSocket(rs.salt + 1);
	SSocketID sockid(id, rs.salt);

	rs.sock = sock;
	rs.inUse = true;
	return sockid;
}

void CSocketIOManagerLobbyIDAddr::UnregisterSocket(SSocketID sockid)
{
	ASSERT_GLOBAL_LOCK;
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;

	*pSock = SRegisteredSocket(pSock->salt + 1);
}

void CSocketIOManagerLobbyIDAddr::RegisterBackoffAddressForSocket(TNetAddress addr, SSocketID sockid)
{
}

void CSocketIOManagerLobbyIDAddr::UnregisterBackoffAddressForSocket(TNetAddress addr, SSocketID sockid)
{
}

CSocketIOManagerLobbyIDAddr::SRegisteredSocket* CSocketIOManagerLobbyIDAddr::GetRegisteredSocket(SSocketID sockid)
{
	ASSERT_COMM_LOCK;

	if (sockid.id >= m_registeredSockets.size())
	{
		NET_ASSERT(false);
		return 0;
	}
	if (!m_registeredSockets[sockid.id].inUse)
		return 0;
	if (m_registeredSockets[sockid.id].salt != sockid.salt)
		return 0;
	return &m_registeredSockets[sockid.id];
}

void CSocketIOManagerLobbyIDAddr::SetRecvFromTarget(SSocketID sockid, IRecvFromTarget* pTarget)
{
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;
	pSock->pRecvFromTarget = pTarget;
}

void CSocketIOManagerLobbyIDAddr::SetSendToTarget(SSocketID sockid, ISendToTarget* pTarget)
{
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;
	pSock->pSendToTarget = pTarget;
}

void CSocketIOManagerLobbyIDAddr::SetConnectTarget(SSocketID sockid, IConnectTarget* pTarget)
{
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;
	pSock->pConnectTarget = pTarget;
}

void CSocketIOManagerLobbyIDAddr::SetAcceptTarget(SSocketID sockid, IAcceptTarget* pTarget)
{
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;
	pSock->pAcceptTarget = pTarget;
}

void CSocketIOManagerLobbyIDAddr::SetRecvTarget(SSocketID sockid, IRecvTarget* pTarget)
{
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;
	pSock->pRecvTarget = pTarget;
}

void CSocketIOManagerLobbyIDAddr::SetSendTarget(SSocketID sockid, ISendTarget* pTarget)
{
	SCOPED_COMM_LOCK;

	SRegisteredSocket* pSock = GetRegisteredSocket(sockid);
	if (!pSock)
		return;
	pSock->pSendTarget = pTarget;
}

bool CSocketIOManagerLobbyIDAddr::RequestRecvFrom(SSocketID sockid)
{
	return true;
}

bool CSocketIOManagerLobbyIDAddr::RequestSendTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len)
{
	return true;
}

bool CSocketIOManagerLobbyIDAddr::RequestSendVoiceTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len)
{
	return true;
}

bool CSocketIOManagerLobbyIDAddr::RequestRecv(SSocketID sockid)
{
	return true;
}

bool CSocketIOManagerLobbyIDAddr::RequestSend(SSocketID sockid, const uint8* pData, size_t len)
{
	return true;
}

bool CSocketIOManagerLobbyIDAddr::RequestConnect(SSocketID sockid, const TNetAddress& addr)
{
	return true;
}

bool CSocketIOManagerLobbyIDAddr::RequestAccept(SSocketID sockid)
{
	return true;
}

#endif // defined(HAS_SOCKETIOMANAGER_LOBBYIDADDR)
