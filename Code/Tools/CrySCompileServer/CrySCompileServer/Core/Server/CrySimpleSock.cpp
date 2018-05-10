// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#include "CrySimpleSock.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"

typedef BOOL (WINAPI * LPFN_DISCONNECTEX)(SOCKET, LPOVERLAPPED, DWORD, DWORD);
#define WSAID_DISCONNECTEX { 0x7fda2e11, 0x8630, 0x436f, { 0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57 } \
  }

volatile long CCrySimpleSock::ms_OpenSockets = 0;

#ifdef USE_WSAEVENTS
CCrySimpleSock::CCrySimpleSock(SOCKET Sock, CCrySimpleSock* pInstance, WSAEVENT wsaEvent) :
	m_Event(wsaEvent),
#else
CCrySimpleSock::CCrySimpleSock(SOCKET Sock, CCrySimpleSock * pInstance) :
#endif
	m_Type(ECrySimpleST_SERVER),
	m_pInstance(pInstance),
	m_Socket(Sock),
	m_WaitForShutdownEvent(false),
	m_bHasReceivedData(false), m_bHasSendData(false),
	m_Port(~0)
{
	InterlockedIncrement(&ms_OpenSockets);

	InitClient();
}

CCrySimpleSock::CCrySimpleSock(const std::string& rServerName, uint16_t Port) :
	m_Type(ECrySimpleST_CLIENT),
	m_pInstance(0),
	m_Socket(INVALID_SOCKET),
	m_WaitForShutdownEvent(false),
	m_bHasReceivedData(false),
	m_bHasSendData(false),
	m_Port(Port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(Port);
	const char* pHostName = rServerName.c_str();
	bool IP = true;
	for (size_t a = 0, Size = strlen(pHostName); a < Size; a++)
		IP &= (pHostName[a] >= '0' && pHostName[a] <= '9') || pHostName[a] == '.';
	if (IP)
		addr.sin_addr.s_addr = inet_addr(pHostName);
	else
	{
		hostent* pHost = gethostbyname(pHostName);
		if (!pHost)
			return;
		addr.sin_addr.s_addr = ((struct in_addr*)(pHost->h_addr))->s_addr;
	}

	m_Socket = socket(AF_INET, SOCK_STREAM, 0);

	InterlockedIncrement(&ms_OpenSockets);

	int Err = connect(m_Socket, (struct sockaddr*)&addr, sizeof addr);
	if (Err < 0)
		m_Socket = INVALID_SOCKET;
}

CCrySimpleSock::~CCrySimpleSock()
{
	Release();
}

CCrySimpleSock::CCrySimpleSock(uint16_t Port) :
	m_Type(ECrySimpleST_ROOT),
	m_pInstance(0),
	m_WaitForShutdownEvent(false),
	m_bHasReceivedData(false),
	m_bHasSendData(false),
	m_Port(Port)
{
#ifdef _MSC_VER
	WSADATA Data;
	m_Socket = INVALID_SOCKET;
	if (WSAStartup(MAKEWORD(2, 0), &Data))
	{
		CrySimple_ERROR("Could not init root socket");
		return;
	}
#endif

	m_Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == m_Socket)
	{
		CrySimple_ERROR("Could not initialize basic server due to invalid socket");
		return;
	}

#ifdef UNIX
	int arg = 1;
	setsockopt(m_Socket, SOL_SOCKET, SO_KEEPALIVE, &arg, sizeof arg);
	arg = 6;
	setsockopt(m_Socket, SOL_SOCKET, SO_PRIORITY, &arg, sizeof arg);
	arg = 1;
	setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof arg);
#endif

	sockaddr_in SockAddr;
	memset(&SockAddr, 0, sizeof(sockaddr_in));
	SockAddr.sin_family = PF_INET;
	SockAddr.sin_port = htons(Port);
	if (bind(m_Socket, (sockaddr*)&SockAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		closesocket(m_Socket);
		CrySimple_ERROR("Could not bind server socket");
		return;
	}

	InterlockedIncrement(&ms_OpenSockets);
}

void CCrySimpleSock::Listen()
{
	listen(m_Socket, SOMAXCONN);
}

void CCrySimpleSock::InitClient()
{
}

void CCrySimpleSock::Release()
{
	if (m_Socket != INVALID_SOCKET)
	{

		// check if we have received and sended data but ignore that for the HTTP server
		if ((!m_bHasSendData || !m_bHasReceivedData) && (!m_pInstance || m_pInstance->m_Port != 80))
		{
			char acTmp[1024];
			sprintf(acTmp, "ERROR : closing socket without both receiving and sending data: receive: %d send: %d",
			        m_bHasReceivedData, m_bHasSendData);
			CRYSIMPLE_LOG(acTmp);
		}

#ifdef USE_WSAEVENTS
		if (m_WaitForShutdownEvent)
		{
			// wait until client has shutdown its socket
			DWORD nReturnCode = WSAWaitForMultipleEvents(1, &m_Event,
			                                             FALSE, INFINITE, FALSE);
			if ((nReturnCode != WSA_WAIT_FAILED) && (nReturnCode != WSA_WAIT_TIMEOUT))
			{
				WSANETWORKEVENTS NetworkEvents;
				WSAEnumNetworkEvents(m_Socket, m_Event, &NetworkEvents);
				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				{
					int iErrorCode = NetworkEvents.iErrorCode[FD_CLOSE_BIT];
					if (iErrorCode != 0)
					{
						// error shutting down
					}
				}
			}
		}

		// shutdown the server side of the connection since no more data will be sent
		shutdown(m_Socket, SD_BOTH);
#endif

		LPFN_DISCONNECTEX pDisconnectEx = NULL;
		DWORD Bytes;
		GUID guidDisconnectEx = WSAID_DISCONNECTEX;
		WSAIoctl(m_Socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidDisconnectEx,
		         sizeof(GUID), &pDisconnectEx, sizeof(pDisconnectEx), &Bytes, NULL, NULL);
		pDisconnectEx(m_Socket, NULL, 0, 0); // retrieve this function pointer with WSAIoctl(WSAID_DISCONNECTEX).

		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
		InterlockedDecrement(&ms_OpenSockets);
	}

#ifdef _MSC_VER
	switch (m_Type)
	{
	case ECrySimpleST_ROOT:
		WSACleanup();
		break;
	case ECrySimpleST_SERVER:
	case ECrySimpleST_CLIENT:
		;
		break;
	default:
		CrySimple_ERROR("unknown SocketType Released");
	}
#endif
}

CCrySimpleSock* CCrySimpleSock::Accept()
{
	if (m_Type != ECrySimpleST_ROOT)
	{
		CrySimple_ERROR("called Accept on non root socket");
		return 0;
	}

	while (true)
	{
		SOCKET Sock = accept(m_Socket, 0, 0);
		if (Sock == INVALID_SOCKET)
		{
			int Error = WSAGetLastError();
			CrySimple_ERROR("Accept recived invalid socket");
			return 0;
		}

		int arg = 1;
		setsockopt(Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&arg, sizeof arg);

		/*
		   // keep socket open for another 2 seconds until data has been fully send
		   LINGER linger;
		   int len = sizeof(LINGER);
		   linger.l_onoff = 1;
		   linger.l_linger = 2;
		   setsockopt(Sock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof linger);
		 */

#ifdef USE_WSAEVENTS
		WSAEVENT wsaEvent = WSACreateEvent();
		if (wsaEvent == WSA_INVALID_EVENT)
		{
			closesocket(Sock);
			int Error = WSAGetLastError();
			CrySimple_ERROR("Couldn't create wsa event");
			return 0;
		}
		;

		int Status = WSAEventSelect(Sock, wsaEvent, FD_CLOSE);
		if (Status == SOCKET_ERROR)
		{
			closesocket(Sock);
			int Error = WSAGetLastError();
			CrySimple_ERROR("Couldn't create wsa event");
			return 0;
		}

		return new CCrySimpleSock(Sock, this, wsaEvent);
#else
		return new CCrySimpleSock(Sock, this);
#endif
	}
}

union CrySimpleRecvSize
{
	uint8_t  m_Data8[8];
	uint64_t m_Data64;
};

#define MAX_TIME_TO_WAIT 10000

int CCrySimpleSock::Recv(char* acData, int len, int flags)
{
	int Recived = SOCKET_ERROR;
	int waitingtime = 0;
	while (Recived < 0)
	{
		Recived = recv(m_Socket, acData, len, flags);
		if (Recived == SOCKET_ERROR)
		{
			int WSAError = WSAGetLastError();
			if (WSAError == WSAEWOULDBLOCK)
			{
				// are we out of time
				if (waitingtime > MAX_TIME_TO_WAIT)
				{
					char acTmp[1024];
					sprintf(acTmp, "Error while reciving size of data - Timeout on blocking. (Error Code: %i)", WSAError);
					CrySimple_ERROR(acTmp);

					return Recived;
				}

				waitingtime += 5;

				// sleep a bit and try again
				Sleep(5);
			}
			else
			{
				char acTmp[1024];
				sprintf(acTmp, "Error while reciving size of data - Network error. (Error Code: %i)", WSAError);
				CrySimple_ERROR(acTmp);

				return Recived;
			}
		}
	}

	return Recived;
}

bool CCrySimpleSock::Recv(std::vector<uint8_t>& rVec)
{
	CrySimpleRecvSize Size;

	int Recived = Recv(reinterpret_cast<char*>(&Size.m_Data8[0]), 8, 0);
	if (Recived != 8)
	{
		int WSAError = WSAGetLastError();
		char acTmp[1024];
		sprintf(acTmp, "Error while reciving size of data - Invalid size (Error Code: %i)", WSAError);
		CrySimple_ERROR(acTmp);
		return false;
	}

	if (Size.m_Data64 == 0)
	{
		int WSAError = WSAGetLastError();
		char acTmp[1024];
		sprintf(acTmp, "Error while reciving size of data - Size of zero (Error Code: %i)", WSAError);
		CrySimple_ERROR(acTmp);

		return false;
	}

	m_SwapEndian = (Size.m_Data64 >> 32) != 0;
	if (m_SwapEndian)
		CSTLHelper::EndianSwizzleU64(Size.m_Data64);

	rVec.clear();
	rVec.resize(static_cast<size_t>(Size.m_Data64));

	for (uint32_t a = 0; a < Size.m_Data64; )
	{
		int Read = Recv(reinterpret_cast<char*>(&rVec[a]), static_cast<int>(Size.m_Data64) - a, 0);
		if (Read <= 0)
		{
			int WSAError = WSAGetLastError();
			char acTmp[1024];
			sprintf(acTmp, "Error while receiving tcp-data (Size: %d - Error Code: %i)", static_cast<int>(Size.m_Data64), WSAError);
			CrySimple_ERROR(acTmp);
			return false;
		}
		a += Read;
	}

	m_bHasReceivedData = true;

	return true;
}

bool CCrySimpleSock::RecvResult()
{
	CrySimpleRecvSize Size;
	if (recv(m_Socket, reinterpret_cast<char*>(&Size.m_Data8[0]), 8, 0) != 8)
	{
		CrySimple_ERROR("Error while reciving result");
		return false;
	}

	return Size.m_Data64 > 0;
}

void CCrySimpleSock::Forward(const std::vector<uint8_t>& rVecIn)
{
	tdDataVector& rVec = m_tempSendBuffer;
	rVec.resize(rVecIn.size() + 8);
	CrySimpleRecvSize& rHeader = *(CrySimpleRecvSize*)(&rVec[0]);
	rHeader.m_Data64 = (uint32_t)rVecIn.size();
	memcpy(&rVec[8], &rVecIn[0], rVecIn.size());

	const size_t BLOCKSIZE = 4 * 1024;
	CrySimpleRecvSize Size;
	Size.m_Data64 = static_cast<int>(rVec.size());
	for (uint64_t a = 0; a < Size.m_Data64; a += BLOCKSIZE)
	{
		char* pData = reinterpret_cast<char*>(&rVec[(size_t)a]);
		int nSendRes = send(m_Socket, pData, std::min<int>(static_cast<int>(Size.m_Data64 - a), BLOCKSIZE), 0);
		if (nSendRes == SOCKET_ERROR)
		{
			int nLastSendError = WSAGetLastError();
			logmessage("Socket send(forward) error: %d", nLastSendError);
		}
	}
}

bool CCrySimpleSock::Backward(std::vector<uint8_t>& rVec)
{
	uint32_t Size;
	if (recv(m_Socket, reinterpret_cast<char*>(&Size), 4, 0) != 4)
	{
		CrySimple_ERROR("Error while reciving size of data");
		return false;
	}

	rVec.clear();
	rVec.resize(static_cast<size_t>(Size));

	for (uint32_t a = 0; a < Size; )
	{
		int Read = recv(m_Socket, reinterpret_cast<char*>(&rVec[a]), Size - a, 0);
		if (Read <= 0)
		{
			CrySimple_ERROR("Error while reciving tcp-data");
			return false;
		}
		a += Read;
	}
	return true;
}

void CCrySimpleSock::Send(const std::vector<uint8_t>& rVecIn, size_t State, EProtocolVersion Version)
{
	bool bResult = true;
	const size_t Offset = Version == EPV_V001 ? 4 : 5;
	tdDataVector& rVec = m_tempSendBuffer;
	rVec.resize(rVecIn.size() + Offset);
	if (rVecIn.size())
	{
		*(uint32_t*)(&rVec[0]) = (uint32_t)rVecIn.size();
		memcpy(&rVec[Offset], &rVecIn[0], rVecIn.size());
	}

	if (Version >= EPV_V002)
		rVec[4] = static_cast<uint8_t>(State);

	if (m_SwapEndian)
		CSTLHelper::EndianSwizzleU32(*(uint32_t*)&rVec[0]);

	const size_t BLOCKSIZE = 4 * 1024;
	CrySimpleRecvSize Size;
	Size.m_Data64 = static_cast<int>(rVec.size());
	for (uint64_t a = 0; a < Size.m_Data64; a += BLOCKSIZE)
	{
		//		if(m_SwapEndian)
		//			CSTLHelper::EndianSwizzleU64(Size.m_Data64);
		//		send(m_Socket,reinterpret_cast<char*>(&Size.m_Data8[0]),8,0);
		char* pData = reinterpret_cast<char*>(&rVec[(size_t)a]);

		int nRetries = 3;
		int nSendRes = SOCKET_ERROR;
		while (nSendRes == SOCKET_ERROR && nRetries > 0)
		{
			nSendRes = send(m_Socket, pData, std::min<int>(static_cast<int>(Size.m_Data64 - a), BLOCKSIZE), 0);
			if (nSendRes == SOCKET_ERROR)
			{
				--nRetries;
				Sleep(100);
			}
		}

		if (nSendRes == SOCKET_ERROR)
		{
			const int nLastSendError = WSAGetLastError();
			bResult = false;
			logmessage("Socket send error: %d", nLastSendError);
			break;
		}
	}

	if (bResult)
	{
		m_bHasSendData = true;
	}
}

void CCrySimpleSock::Send(const std::string& rData)
{
	const size_t BLOCKSIZE = 4 * 1024;
	const size_t S = rData.size();
	for (uint64_t a = 0; a < S; a += BLOCKSIZE)
	{
		const char* pData = &rData.c_str()[a];
		const int nSendRes = send(m_Socket, pData, std::min<int>(static_cast<int>(S - a), BLOCKSIZE), 0);
		if (nSendRes == SOCKET_ERROR)
		{
			int nLastSendError = WSAGetLastError();
			logmessage("Socket send error: %d", nLastSendError);
		}
	}
}

uint32_t CCrySimpleSock::PeerIP()
{
	struct sockaddr_in addr;
	int addr_size = sizeof(sockaddr_in);
	int nRes = getpeername(m_Socket, (sockaddr*) &addr, &addr_size);
	if (nRes == SOCKET_ERROR)
	{
		int nError = WSAGetLastError();
		logmessage("Socket getpeername error: %d", nError);
		return 0;
	}
	return addr.sin_addr.S_un.S_addr;
}
