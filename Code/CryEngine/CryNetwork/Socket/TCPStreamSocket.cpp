// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Network.h"
#include "TCPStreamSocket.h"
#include <CryNetwork/CrySocks.h>

void CTCPStreamSocket::SetSocketState(ESocketState state)
{
	if (m_socketState == state)
		return;

	ISendTarget* pSendTarget = 0;
	IRecvTarget* pRecvTarget = 0;
	IConnectTarget* pConnectTarget = 0;
	IAcceptTarget* pAcceptTarget = 0;

	switch (state)
	{
	case eSS_Listening:
		pAcceptTarget = this;
		break;
	case eSS_Connecting:
		pConnectTarget = this;
		break;
	case eSS_Established:
		pRecvTarget = this;
		pSendTarget = this;
		break;
	}

	m_pSockIO->SetRecvTarget(m_sockid, pRecvTarget);
	m_pSockIO->SetSendTarget(m_sockid, pSendTarget);
	m_pSockIO->SetConnectTarget(m_sockid, pConnectTarget);
	m_pSockIO->SetAcceptTarget(m_sockid, pAcceptTarget);

	if (pAcceptTarget)
		m_pSockIO->RequestAccept(m_sockid);
	if (pRecvTarget)
		m_pSockIO->RequestRecv(m_sockid);

	m_socketState = state;
}

//void SListenVisitor::Visit(const SIPv4Addr& addr)
//{
//	m_result = false;
//
//	if (!m_pStreamSocket)
//		return;
//
//#if CRY_PLATFORM_WINDOWS
//	CRYSOCKADDR_IN sain;
//	ZeroMemory(&sain, sizeof(sain));
//	sain.sin_family = AF_INET;
//	sain.sin_addr.s_addr = htonl(addr.addr);
//	sain.sin_port = htons(addr.port);
//	if ( CRY_SOCKET_ERROR != CrySock::bind( m_pStreamSocket->m_socket, (sockaddr*)&sain, sizeof(sain) ) )
//		if ( CRY_SOCKET_ERROR != listen(m_pStreamSocket->m_socket, SOMAXCONN) )
//		{
//			m_pStreamSocket->SetSocketState( CTCPStreamSocket::eSS_Listening );
//			m_result = true;
//		}
//#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
//	// TODO:
//#endif
//}
//
//void SConnectVisitor::Visit(const SIPv4Addr& addr)
//{
//	m_result = false;
//
//	if (!m_pStreamSocket)
//		return;
//
//#if CRY_PLATFORM_WINDOWS
//	u_long nonblocking = 1;
//	ioctlsocket(m_pStreamSocket->m_socket, FIONBIO, &nonblocking);
//
//	CRYSOCKADDR_IN sain;
//	ZeroMemory( &sain, sizeof(sain) );
//	sain.sin_family = AF_INET;
//	sain.sin_addr.s_addr = htonl(addr.addr);
//	sain.sin_port = htons(addr.port);
//	int ir = CrySock::connect( m_pStreamSocket->m_socket, (CRYSOCKADDR*)&sain, sizeof(sain) );
//	if ( CRY_SOCKET_ERROR != ir || WSAEWOULDBLOCK == WSAGetLastError() )
//	{
//		m_pStreamSocket->SetSocketState(CTCPStreamSocket::eSS_Established);
//		if (m_pStreamSocket->m_pListener)
//			m_pStreamSocket->m_pListener->OnConnectCompleted(true);
//		m_result = true;
//	}
//#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
//	// TODO:
//#endif
//}

CTCPStreamSocket::CTCPStreamSocket()
{
#if !CRY_PLATFORM_CONSOLE
	m_socket = CRY_INVALID_SOCKET;
#endif

	m_socketState = eSS_Closed;
	m_pListener = NULL;
	m_pListenerUserData = NULL;
	m_pSockIO = &CNetwork::Get()->GetInternalSocketIOManager();
}

CTCPStreamSocket::~CTCPStreamSocket()
{
	Cleanup();
#if CRY_PLATFORM_WINDOWS
	NET_ASSERT(m_socket == CRY_INVALID_SOCKET);
#endif
}

bool CTCPStreamSocket::Init()
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	m_socket = CrySock::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (CRY_INVALID_SOCKET == m_socket)
		return false;

	m_sockid = m_pSockIO->RegisterSocket(m_socket, 0);
	if (!m_sockid)
		return false;

#endif

	return true;
}

void CTCPStreamSocket::SetListener(IStreamListener* pListener, void* pUserdata)
{
	m_pListener = pListener;
	m_pListenerUserData = pUserdata;
}

void CTCPStreamSocket::GetPeerAddr(TNetAddress& addr)
{
	SIPv4Addr addr4;

#if !CRY_PLATFORM_CONSOLE
	CRYSOCKADDR_IN sain;
	memset(&sain, '\0', sizeof(sain));
	CRYSOCKLEN_T slen = sizeof(sain);
	if (CRY_SOCKET_ERROR != getpeername(m_socket, (sockaddr*)&sain, &slen))
	{
		addr4.addr = ntohl(sain.sin_addr.s_addr);
		addr4.port = ntohs(sain.sin_port);
	}
#endif

	addr = TNetAddress(addr4);
}

bool CTCPStreamSocket::Listen(const TNetAddress& addr)
{
	//SListenVisitor visitor(this);
	//addr.Visit(visitor);
	//return visitor.m_result;

#if !CRY_PLATFORM_CONSOLE
	CRYSOCKADDR sa;
	int sl = sizeof(sa);
	if (!ConvertAddr(addr, &sa, &sl))
		return false;
	if (CRY_SOCKET_ERROR != CrySock::bind(m_socket, &sa, sl))
		if (CRY_SOCKET_ERROR != CrySock::listen(m_socket, SOMAXCONN))
		{
			SetSocketState(eSS_Listening);
			return true;
		}
#endif
	return false;
}

bool CTCPStreamSocket::Connect(const TNetAddress& addr)
{
	//SConnectVisitor visitor(this);
	//addr.Visit(visitor);
	//return visitor.m_result;
#if !CRY_PLATFORM_CONSOLE
	CRYSOCKADDR sa;
	int sl = sizeof(sa);
	if (!ConvertAddr(TNetAddress(SIPv4Addr()), &sa, &sl))
		return false;
	if (CRY_SOCKET_ERROR == CrySock::bind(m_socket, &sa, sl))
		return false;
	SetSocketState(eSS_Connecting);
	return m_pSockIO->RequestConnect(m_sockid, addr);
#endif
	return false;
}

bool CTCPStreamSocket::Send(const uint8* pBuffer, size_t nLength)
{
	if (pBuffer == NULL || nLength == 0)
		return false;

	return m_pSockIO->RequestSend(m_sockid, pBuffer, nLength);
}

void CTCPStreamSocket::Shutdown()
{
#if !CRY_PLATFORM_CONSOLE
	CrySock::shutdown(m_socket, SD_SEND);
#endif
}

void CTCPStreamSocket::Close()
{
	Cleanup();
}

bool CTCPStreamSocket::IsDead()
{
#if !CRY_PLATFORM_CONSOLE
	return CRY_INVALID_SOCKET == m_socket;
#else
	return true;
#endif
}

void CTCPStreamSocket::OnRecvComplete(const uint8* pData, uint32 nSize)
{
	m_pListener->OnIncomingData(pData, nSize, m_pListenerUserData);
	m_pSockIO->RequestRecv(m_sockid);
}

void CTCPStreamSocket::OnRecvException(ESocketError err)
{
	m_pListener->OnConnectionClosed(false, m_pListenerUserData);
}

void CTCPStreamSocket::OnSendException(ESocketError err)
{
	m_pListener->OnConnectionClosed(false, m_pListenerUserData);
}

void CTCPStreamSocket::OnConnectComplete()
{
	SetSocketState(eSS_Established);
	m_pListener->OnConnectCompleted(true, m_pListenerUserData);
}

void CTCPStreamSocket::OnConnectException(ESocketError err)
{
	SetSocketState(eSS_Closed);
	m_pListener->OnConnectCompleted(false, m_pListenerUserData);
}

void CTCPStreamSocket::OnAccept(const TNetAddress& from, CRYSOCKET s)
{
#if CRY_PLATFORM_WINDOWS
	u_long nonblocking = 1;
	ioctlsocket(s, FIONBIO, &nonblocking);
#elif CRY_PLATFORM_ORBIS
	int nonblocking = 1;
	CrySock::setsockopt(s, SOL_SOCKET, SO_NBIO, (const char*)&nonblocking, sizeof(nonblocking));
#elif CRY_PLATFORM_POSIX
	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif

	CTCPStreamSocket* pStreamSocket = new CTCPStreamSocket();
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	pStreamSocket->m_socket = s;
#endif
	pStreamSocket->m_sockid = m_pSockIO->RegisterSocket(s, 0);
	pStreamSocket->SetSocketState(eSS_Established);
	m_pListener->OnConnectionAccepted(pStreamSocket, m_pListenerUserData); // a listener must be specified for the newly accepted socket inside the callback
	m_pSockIO->RequestAccept(m_sockid);
}

void CTCPStreamSocket::OnAcceptException(ESocketError err)
{
	NetWarning("AcceptException:%d", err);
	m_pSockIO->RequestAccept(m_sockid);
}

void CTCPStreamSocket::Cleanup()
{
#if CRY_PLATFORM_WINDOWS
	// During System::Shutdown, CNetwork is deleted. It can happen that
	// the destructor of CSimpleHttpServerInternal (and thus of CTCPStreamSocket) is called afterwards
	// Since we still have a pointer to CNetwork-related data in m_pSockIO, we must check that CNetwork is still valid
	CNetwork* pNetwork = CNetwork::Get();
	const bool cryNetworkStillValid = pNetwork != NULL && (&pNetwork->GetInternalSocketIOManager()) != NULL;

	if (m_sockid && cryNetworkStillValid)
	{
		SCOPED_GLOBAL_LOCK;
		m_pSockIO->UnregisterSocket(m_sockid);
		m_sockid = SSocketID();
	}
	if (CRY_INVALID_SOCKET != m_socket)
	{
		CrySock::closesocket(m_socket);
		m_socket = CRY_INVALID_SOCKET;
		m_socketState = eSS_Closed;
	}
#endif
}
