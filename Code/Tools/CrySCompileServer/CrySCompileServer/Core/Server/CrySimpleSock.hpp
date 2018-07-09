// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLESOCK__
#define __CRYSIMPLESOCK__

#ifdef UNIX
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#define closesocket close
#else
#include <WinSock2.h>
#endif

#include <vector>
#include <algorithm>

#include "../STLHelper.hpp"

//#define USE_WSAEVENTS

enum ECrySimpleS_TYPE
{
	ECrySimpleST_ROOT,
	ECrySimpleST_SERVER,
	ECrySimpleST_CLIENT,
	ECrySimpleST_INVALID,
};

enum EProtocolVersion
{
	EPV_V001,
	EPV_V002,
	EPV_V0021,
};

class CCrySimpleSock
{
public:
	static volatile long 			ms_OpenSockets;

	CCrySimpleSock*						m_pInstance;

	const ECrySimpleS_TYPE		m_Type;
	SOCKET										m_Socket;
	uint16_t									m_Port;
#ifdef USE_WSAEVENTS
	WSAEVENT									m_Event;
#endif
	bool											m_WaitForShutdownEvent;
	bool											m_SwapEndian;

	bool											m_bHasReceivedData;
	bool											m_bHasSendData;

	tdDataVector              m_tempSendBuffer;


#ifdef USE_WSAEVENTS
														CCrySimpleSock(SOCKET Sock,CCrySimpleSock* pInstance,WSAEVENT wsaEvent);
#else
														CCrySimpleSock(SOCKET Sock,CCrySimpleSock* pInstance);
#endif
														CCrySimpleSock(const CCrySimpleSock&):m_Type(ECrySimpleST_INVALID){};

	void											InitClient();
	void											Release();

	int												Recv(char* acData, int len, int flags);
public:
														CCrySimpleSock(const std::string& rServerName,uint16_t Port);
														CCrySimpleSock(uint16_t Port);

														~CCrySimpleSock();
	void											Listen();


	CCrySimpleSock*						Accept();

	bool											Recv(std::vector<uint8_t>& rVec);
	bool											RecvResult();

	bool											Backward(std::vector<uint8_t>& rVec);
	void											Send(const std::vector<uint8_t>& rVec,size_t State,EProtocolVersion Version);
	void											Forward(const std::vector<uint8_t>& rVec);

	//used for HTML
	void											Send(const std::string& rData);

	uint32_t									PeerIP();

	bool											Valid()const{return m_Socket!=INVALID_SOCKET;}

	void											WaitForShutDownEvent(bool bValue) { m_WaitForShutdownEvent = bValue; }

	static volatile long 			GetOpenSockets() { return ms_OpenSockets; }
};

#endif
