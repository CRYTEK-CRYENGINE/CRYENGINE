// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SOCKETIOMANAGERLOBBYIDADDR_H__
#define __SOCKETIOMANAGERLOBBYIDADDR_H__

#pragma once

#if USE_LOBBYIDADDR
	#define HAS_SOCKETIOMANAGER_LOBBYIDADDR
#endif // USE_LOBBYIDADDR

#if defined(HAS_SOCKETIOMANAGER_LOBBYIDADDR)

	#include "SocketError.h"
	#include <CryMemory/PoolAllocator.h>
	#include "ISocketIOManager.h"
	#include "Network.h"

class CSocketIOManagerLobbyIDAddr : public CSocketIOManager
{
public:
	CSocketIOManagerLobbyIDAddr();
	~CSocketIOManagerLobbyIDAddr();
	bool        Init();
	const char* GetName() override { return "LobbyIDAddr"; }
	bool        PollWait(uint32 waitTime) override;
	int         PollWork(bool& performedWork) override;
	static void RecvPacket(void* privateRef, uint8* recvBuffer, uint32 recvSize, CRYSOCKET recvSocket, TNetAddress& recvAddr);

	SSocketID   RegisterSocket(CRYSOCKET sock, int protocol) override;
	void        SetRecvFromTarget(SSocketID sockid, IRecvFromTarget* pTarget) override;
	void        SetSendToTarget(SSocketID sockid, ISendToTarget* pTarget) override;
	void        SetConnectTarget(SSocketID sockid, IConnectTarget* pTarget) override;
	void        SetAcceptTarget(SSocketID sockid, IAcceptTarget* pTarget) override;
	void        SetRecvTarget(SSocketID sockid, IRecvTarget* pTarget) override;
	void        SetSendTarget(SSocketID sockid, ISendTarget* pTarget) override;
	void        RegisterBackoffAddressForSocket(TNetAddress addr, SSocketID sockid) override;
	void        UnregisterBackoffAddressForSocket(TNetAddress addr, SSocketID sockid) override;
	void        UnregisterSocket(SSocketID sockid) override;

	bool        RequestRecvFrom(SSocketID sockid) override;
	bool        RequestSendTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len) override;
	bool        RequestSendVoiceTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len) override;

	bool        RequestConnect(SSocketID sockid, const TNetAddress& addr) override;
	bool        RequestAccept(SSocketID sockid) override;
	bool        RequestSend(SSocketID sockid, const uint8* pData, size_t len) override;
	bool        RequestRecv(SSocketID sockid) override;

	void        PushUserMessage(int msg) override {}

	bool        HasPendingData() override         { return false; }

	#if LOCK_NETWORK_FREQUENCY
	virtual void ForceNetworkStart() override {}
	virtual bool NetworkSleep() override      { return true; }
	#endif

private:
	struct SRegisteredSocket
	{
		SRegisteredSocket(uint16 saltValue) :
			sock(CRY_INVALID_SOCKET),
			pRecvFromTarget(0),
			pSendToTarget(0),
			pConnectTarget(0),
			pAcceptTarget(0),
			pRecvTarget(0),
			pSendTarget(0),
			salt(saltValue),
			inUse(false)
		{
		}
		bool             inUse;
		CRYSOCKET        sock;
		uint16           salt;
		IRecvFromTarget* pRecvFromTarget;
		ISendToTarget*   pSendToTarget;
		IConnectTarget*  pConnectTarget;
		IAcceptTarget*   pAcceptTarget;
		IRecvTarget*     pRecvTarget;
		ISendTarget*     pSendTarget;
	};
	typedef std::vector<SRegisteredSocket> TRegisteredSockets;
	TRegisteredSockets m_registeredSockets;

	SRegisteredSocket* GetRegisteredSocket(SSocketID sockid);

	uint8       m_recvBuffer[MAX_UDP_PACKET_SIZE];
	TNetAddress m_recvAddr;
	uint32      m_recvSize;
	CRYSOCKET   m_recvSocket;
};

#endif // defined(HAS_SOCKETIOMANAGER_LOBBYIDADDR)

#endif // __SOCKETIOMANAGERLOBBYIDADDR_H__
