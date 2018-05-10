// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Network.h"
#include "SimpleHttpServer.h"

#include "cry_sha1.h"
#include <CryCore/Base64.h>

//#include <sstream>

#define CR                     '\r'
#define LF                     '\n'
#define CRLF                   "\r\n"

#define SHTTP_MAX_IP_EXPECTED  (16)
#define SHTTP_MAX_ADDRESS_SIZE (SHTTP_MAX_IP_EXPECTED + 6 + 1)

CSimpleHttpServer CSimpleHttpServer::s_singleton;

CSimpleHttpServer& CSimpleHttpServer::GetSingleton()
{
	return s_singleton;
}

#pragma warning(push)
#pragma warning(disable:4355) //'this' : used in base member initializer list
CSimpleHttpServer::CSimpleHttpServer() : m_internal(this)
{
#pragma warning(pop)
	m_pListener = NULL;

#if defined(HTTP_WEBSOCKETS)
	m_pWsAllocHeap = CryGetIMemoryManager()->CreateGeneralExpandingMemoryHeap(HEAP_MAX_SIZE, HEAP_RESERVE_SIZE, "HttpServer");
#endif
}

CSimpleHttpServer::~CSimpleHttpServer()
{
#if defined(HTTP_WEBSOCKETS)
	SAFE_RELEASE(m_pWsAllocHeap);
#endif
}

void CSimpleHttpServer::Start(uint16 port, const string& password, IHttpServerListener* pListener)
{
	m_pListener = pListener;

	FROM_GAME(&CSimpleHttpServerInternal::Start, &m_internal, port, password);
}

void CSimpleHttpServer::Stop()
{
	FROM_GAME(&CSimpleHttpServerInternal::Stop, &m_internal);
}

void CSimpleHttpServer::Quit()
{
	m_internal.Stop(); // Not queued, only called on shutdown
}

void CSimpleHttpServer::Tick()
{
	m_internal.UpdateClosedConnections();
}

void CSimpleHttpServer::SendResponse(int connectionID, EStatusCode statusCode, EContentType contentType, const string& content, bool closeConnection)
{
	FROM_GAME(&CSimpleHttpServerInternal::SendResponse, &m_internal, connectionID, statusCode, contentType, content, closeConnection);
}

void CSimpleHttpServer::SendWebpage(int connectionID, const string& webpage)
{
	FROM_GAME(&CSimpleHttpServerInternal::SendWebpage, &m_internal, connectionID, webpage);
}

#if defined(HTTP_WEBSOCKETS)

void CSimpleHttpServer::AddWebsocketProtocol(const string& protocolName, IHttpWebsocketProtocol* pProtocol)
{
	FROM_GAME(&CSimpleHttpServerInternal::AddWebsocketProtocol, &m_internal, protocolName, pProtocol);
}

void CSimpleHttpServer::SendWebsocketData(int connectionID, IHttpWebsocketProtocol::SMessageData& data, bool closeConnection)
{
	// Allocate a work buffer. This needs to be larger than the input data because it must contain a pre-pended header (max should be 10 bytes).
	// DO NOT DELETE this buffer! It will be deleted internally (see CSimpleHttpServerInternal::SendToWebsocket) after the data has been sent.

	char* pNewBufferWithExtraSpace = (char*)AllocateHeapBuffer(data.m_bufferSize + WEBSOCKET_EXTRA_HEADER_SPACE);
	if (pNewBufferWithExtraSpace)
	{
		memcpy(pNewBufferWithExtraSpace + WEBSOCKET_EXTRA_HEADER_SPACE, data.m_pBuffer, data.m_bufferSize);
		FROM_GAME(&CSimpleHttpServerInternal::SendToWebsocket, &m_internal, connectionID, data.m_eType, pNewBufferWithExtraSpace, data.m_bufferSize, closeConnection);
	}
	else
	{
		CryLogAlways("CSimpleHttpServer::SendWebsocketData failed to allocate a temporary work buffer. Aborting send.");
	}
}

void* CSimpleHttpServer::AllocateHeapBuffer(uint32 bufferSize)
{
	SCOPED_GLOBAL_LOCK;

	if (m_pWsAllocHeap)
	{
		return m_pWsAllocHeap->Malloc(bufferSize, "WebsocketTempBuffer");
	}
	else
	{
		return (void*) new int8[bufferSize];
	}
}

void CSimpleHttpServer::FreeHeapBuffer(void* pBuffer)
{
	SCOPED_GLOBAL_LOCK;

	if (m_pWsAllocHeap)
	{
		m_pWsAllocHeap->Free((void*)pBuffer);
	}
	else
	{
		delete[] (int8*)pBuffer;
	}
}

#endif

// three-digit HTTP response status codes in the form of strings, the order must match that of EStatusCode
const string CSimpleHttpServerInternal::s_statusCodes[] = {
	"101 Switching Protocols",
	"200 OK",
	"400 Bad Request",
	"404 Not Found",
	"408 Request Timeout",
	"501 Not Implemented",
	"503 Service Unavailable",
	"505 Bad HTTP Version"
};

const string CSimpleHttpServerInternal::s_contentTypes[] = { "html", "xml", "txt" };

#if defined(HTTP_WEBSOCKETS)
const string CSimpleHttpServerInternal::s_WsOpCodes[CSimpleHttpServerInternal::eWSOC_Max] =
{
	"ContinuationFrame",
	"Text",
	"Binary",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"ConnectionClose",
	"Ping",
	"Pong",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
};
const string CSimpleHttpServerInternal::s_WsConnectStates[CSimpleHttpServerInternal::eWSCS_Invalid] =
{
	"RecvHeader",
	"RecvExtendedSize",
	"RecvPayload",
	"SendGracefulClose",
};
const string CSimpleHttpServerInternal::s_WsGUID("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
#endif

CSimpleHttpServerInternal::HttpConnection::HttpConnection(int ID)
{
	m_ID = ID;

	m_sessionState = eSS_Unsessioned;

	m_authenticationTimeoutStart = 0.0f;

	m_lineReadingTimeoutStart = 0.0f;

	m_receivingState = eRS_ReceivingHeaders;

	m_remainingContentBytes = 0;

	m_requestType = eRT_None;

#if defined(HTTP_WEBSOCKETS)
	ClearAllWebsocketData();
#endif
}

#if defined(HTTP_WEBSOCKETS)

CSimpleHttpServerInternal::HttpConnection::WsReadChunk::WsReadChunk(const uint8* pData, size_t nSize)
{
	m_readOffset = 0;
	m_bufferSize = 0;
	m_pBuffer = (uint8*)CSimpleHttpServer::GetSingleton().AllocateHeapBuffer(nSize);
	if (m_pBuffer)
	{
		memcpy(m_pBuffer, pData, nSize);
		m_bufferSize = nSize;
	}
}

bool CSimpleHttpServerInternal::HttpConnection::AddWsReadChunk(const uint8* pData, size_t nSize)
{
	m_WsReadChunks.push_back(WsReadChunk(pData, nSize));
	return true;
}

bool CSimpleHttpServerInternal::HttpConnection::CollateWsReadChunks(char* pBuffer, uint64 bufferSize)
{
	//-- test existing chunks to see if requested amount of data exists
	uint64 nCumulSize = 0;
	for (HttpConnection::TWsReadChunkList::const_iterator it = m_WsReadChunks.begin(); it != m_WsReadChunks.end(); ++it)
	{
		nCumulSize += (it->m_bufferSize - it->m_readOffset);
		if (nCumulSize >= bufferSize)
		{
			break;
		}
	}

	if (nCumulSize < bufferSize)
	{
		//-- not enough available data to fill collate request
		return false;
	}

	//-- enough data for collate.
	char* pBufPos = pBuffer;
	uint64 nBytesRequired = bufferSize;

	for (HttpConnection::TWsReadChunkList::iterator it = m_WsReadChunks.begin(); it != m_WsReadChunks.end(); )
	{
		uint64 availableBytesInChunk = it->m_bufferSize - it->m_readOffset;

		if (nBytesRequired < availableBytesInChunk)
		{
			//-- partial chunk collate
			memcpy(pBufPos, it->m_pBuffer + it->m_readOffset, (size_t)nBytesRequired);
			pBufPos += nBytesRequired;
			it->m_readOffset += nBytesRequired;

			nBytesRequired = 0;
		}
		else
		{
			//-- full chunk collate
			memcpy(pBufPos, it->m_pBuffer + it->m_readOffset, (size_t)availableBytesInChunk);
			pBufPos += availableBytesInChunk;
			it->m_readOffset = it->m_bufferSize;
			nBytesRequired -= availableBytesInChunk;

			//-- delete the chunk
			CSimpleHttpServer::GetSingleton().FreeHeapBuffer(it->m_pBuffer);
			it->m_pBuffer = NULL;

			it = m_WsReadChunks.erase(it);
		}

		if (nBytesRequired == 0)
		{
			break;
		}
	}

	return true;
}

void CSimpleHttpServerInternal::HttpConnection::ClearAllWebsocketData()
{
	m_websocketProtocol = NULL;

	for (TWsReadChunkList::iterator it = m_WsReadChunks.begin(); it != m_WsReadChunks.end(); ++it)
	{
		if (it->m_pBuffer)
		{
			CSimpleHttpServer::GetSingleton().FreeHeapBuffer(it->m_pBuffer);
		}
		it->m_pBuffer = NULL;
	}
	m_WsReadChunks.clear();

	ClearWebsocketProcessingState();
}

void CSimpleHttpServerInternal::HttpConnection::ClearWebsocketProcessingState()
{
	m_WsConnectState = eWSCS_RecvHeader;

	memset(&m_WsRecvCurrentHeader, 0, sizeof(WsDataHeader));
	m_WsRecvCurrentPayloadSize = 0;
	memset(&m_WsRecvCurrentMask, 0, 4);

	m_WsRecvCombinedMessageType = IHttpWebsocketProtocol::eMT_Unknown;
	m_WsRecvCombinedMessageBuffer.clear();
}

#endif

CSimpleHttpServerInternal::CSimpleHttpServerInternal(CSimpleHttpServer* pServer)
{
	m_pServer = pServer;

	// small struct, let's conservatively reserve for a few concurrent connections.
	m_connections.reserve(HTTP_CONNECTIONS_RESERVE);
	m_connectionID = 1;
}

CSimpleHttpServerInternal::~CSimpleHttpServerInternal()
{
}

#ifdef HTTP_WEBSOCKETS
void CSimpleHttpServerInternal::AddWebsocketProtocol(string protocolName, IHttpWebsocketProtocol* pProtocol)
{
	m_pProtocolHandlers[protocolName] = pProtocol;
}
#endif

void CSimpleHttpServerInternal::GetBindAddress(TNetAddress& addr, uint16 port) const
{
	addr = TNetAddress(SIPv4Addr(0, port));
#if CRY_PLATFORM_WINDOWS                      // On windows we should check the sv_bind variable and attempt to play nice and bind to the address requested
	TNetAddressVec addrs;
	char address[SHTTP_MAX_ADDRESS_SIZE];
	CNetAddressResolver resolver;

	cry_sprintf(address, "0.0.0.0:%d", port);

	ICVar* pCVar = gEnv->pConsole->GetCVar("sv_bind");
	if (pCVar && pCVar->GetString())
	{
		if (strlen(pCVar->GetString()) <= SHTTP_MAX_IP_EXPECTED)
		{
			cry_sprintf(address, "%s:%d", pCVar->GetString(), port);
		}
		else
		{
			NetLog("Address to bind '%s' does not appear to be an IP address", pCVar->GetString());
		}
	}

	CNameRequestPtr pReq = resolver.RequestNameLookup(address);
	pReq->Wait();
	if (pReq->GetResult(addrs) != eNRR_Succeeded)
	{
		NetLog("Name resolution for '%s' failed - binding to 0.0.0.0", address);
	}
	else
	{
		addr = addrs[0];
	}
#endif
}

void CSimpleHttpServerInternal::Start(uint16 port, string password)
{
	if (m_pSocketListen != NULL && m_pServer->m_pListener)
	{
		TO_GAME(&IHttpServerListener::OnStartResult, m_pServer->m_pListener, false, IHttpServerListener::eRD_AlreadyStarted);
		return;
	}

	m_pSocketListen = CreateStreamSocket(TNetAddress(SIPv4Addr(0, port)));
	NET_ASSERT(m_pSocketListen != NULL);

	m_pSocketListen->SetListener(this, NULL);

	TNetAddress bindTo;

	GetBindAddress(bindTo, port);

	bool r = m_pSocketListen->Listen(bindTo);
	if (!r)
	{
		m_pSocketListen->Close();
		m_pSocketListen = NULL;
		if (m_pServer->m_pListener)
		{
			TO_GAME(&IHttpServerListener::OnStartResult, m_pServer->m_pListener,
			        false, IHttpServerListener::eRD_Failed);
		}
		return;
	}

	m_password = password;

	m_hostname = CNetwork::Get()->GetHostName();
	CNameRequestPtr pNR = RESOLVER.RequestNameLookup(m_hostname);
	pNR->Wait();
	if (eNRR_Succeeded == pNR->GetResult())
		m_hostname = pNR->GetAddrString();

	m_realm = "CryHttp@" + m_hostname;

	if (m_pServer->m_pListener)
	{
		TO_GAME(&IHttpServerListener::OnStartResult, m_pServer->m_pListener,
		        true, IHttpServerListener::eRD_Okay);
	}
}

void CSimpleHttpServerInternal::Stop()
{
	for (size_t i = 0; i < m_connections.size(); i++)
	{
		HttpConnection* conn = &m_connections[i];

		CloseHttpConnection(conn->m_ID, false);
	}
	m_connections.clear();

	if (m_pSocketListen)
	{
		m_pSocketListen->Close();
		m_pSocketListen = NULL;
	}

	m_password = "";
}

CSimpleHttpServerInternal::HttpConnection* CSimpleHttpServerInternal::GetConnection(int connectionID)
{
	for (size_t i = 0; i < m_connections.size(); i++)
	{
		if (m_connections[i].m_ID == connectionID)
			return &m_connections[i];
	}

	return NULL;
}

void CSimpleHttpServerInternal::UpdateClosedConnections()
{
	CryAutoLock<CryMutex> lock(m_mutex);
	if (m_closedConnections.empty())
		return;

	m_closedConnections.clear();
}

void CSimpleHttpServerInternal::CloseHttpConnection(int connectionID, bool bGraceful)
{
	for (size_t i = 0; i < m_connections.size(); i++)
	{
		HttpConnection* pConnection = &m_connections[i];

		if (pConnection->m_ID == connectionID)
		{
			if (pConnection->m_pSocketSession)
			{
				pConnection->m_pSocketSession->Close();
			}

#if defined(HTTP_WEBSOCKETS)
			if (pConnection->m_receivingState == eRS_Websocket)
			{
				TO_GAME(&CSimpleHttpServerInternal::OnWebsocketClose, this, connectionID, pConnection->m_websocketProtocol, bGraceful);
			}

			pConnection->ClearAllWebsocketData();
#endif

			//if (m_sessionState == eSS_Authorized)
			if (m_pServer->m_pListener)
			{
				TO_GAME(&IHttpServerListener::OnClientDisconnected, m_pServer->m_pListener, connectionID);
			}

			pConnection->m_remoteAddr = TNetAddress(SIPv4Addr());
			pConnection->m_sessionState = eSS_Unsessioned;
			pConnection->m_receivingState = eRS_Ignoring;

			{
				CryAutoLock<CryMutex> lock(m_mutex);
				m_closedConnections.push_back(pConnection->m_pSocketSession);
			}

			m_connections.erase(m_connections.begin() + i);
			break;
		}
	}
}

void CSimpleHttpServerInternal::IgnorePending(int connectionID)
{
	HttpConnection* conn = GetConnection(connectionID);
	if (conn)
		conn->m_sessionState = eSS_IgnorePending;
}

void CSimpleHttpServerInternal::SendResponse(int connectionID, ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, string content, bool closeConnection)
{
	HttpConnection* conn = GetConnection(connectionID);

	if (!conn || !conn->m_pSocketSession /*|| m_sessionState != eSS_Authorized*/)
		return;

	if (conn->m_receivingState == eRS_Websocket)
	{
		return;
	}

	string response;
	BuildNormalResponse(connectionID, response, statusCode, contentType, content);
	conn->m_pSocketSession->Send((const uint8*)response.data(), response.size());

	if (closeConnection)
	{
		CloseHttpConnection(connectionID, true);
	}
}

void CSimpleHttpServerInternal::SendWebpage(int connectionID, string webpage)
{
	HttpConnection* conn = GetConnection(connectionID);

	if (!conn || !conn->m_pSocketSession)
		return;

	string response = "HTTP/1.1 200 OK" CRLF;
	response += "Content-Type: text/rfc822" CRLF;
	//std::stringstream contentLength; contentLength << webpage.size();
	char contentLength[33];
	cry_sprintf(contentLength, "%u", (unsigned)webpage.size());
	response += string("Content-Length: ") + string(contentLength) + string(CRLF) + string(CRLF);

	response += webpage;
	conn->m_pSocketSession->Send((const uint8*)response.data(), response.size());
}

#if defined(HTTP_WEBSOCKETS)
void CSimpleHttpServerInternal::SendToWebsocket(int connectionID, IHttpWebsocketProtocol::EMessageType type, char* pBuffer, uint32 bufferSize, bool closeConnection)
{
	// Note: bufferSize is not the actual size of pBuffer! pBuffer is actually bufferSize + WEBSOCKET_EXTRA_HEADER_SPACE in size.
	// Before returning from this function, we must always delete the pBuffer, which was allocated in CSimpleHttpServer::SendWebsocketData
	HttpConnection* conn = GetConnection(connectionID);
	if (conn && conn->m_pSocketSession)
	{
		if (conn->m_receivingState == eRS_Websocket)
		{
			unsigned long headerSize = bufferSize < 126 ? 2 : (bufferSize < 65536) ? 4 : 10;

			WsDataHeader* pHeader = (WsDataHeader*)(pBuffer + WEBSOCKET_EXTRA_HEADER_SPACE - headerSize);
			memset(pHeader, 0, sizeof(WsDataHeader));

			pHeader->fin = 1;

			switch (type)
			{
			case IHttpWebsocketProtocol::eMT_Text:
				{
					pHeader->opcode = eWSOC_Text;
				}
				break;
			case IHttpWebsocketProtocol::eMT_Binary:
				{
					pHeader->opcode = eWSOC_Binary;
				}
				break;
			default:
				{
					// Unknown websocket data type
					CryLogAlways("CSimpleHttpServerInternal::SendWebsocketData: Unknown websocket data type - aborting send.");

					// delete the buffer we created in CSimpleHttpServer::SendWebsocketData on the game thread.
					m_pServer->FreeHeapBuffer(pBuffer);

					return;
				}
				break;
			}

			switch (headerSize)
			{
			case 2:
				{
					pHeader->len = bufferSize;
				}
				break;

			case 4:
				{
					pHeader->len = 126;

					unsigned short sz = (unsigned short)bufferSize;
					SwapEndian(sz, eBigEndian);
					memcpy(pHeader + 1, (char*)&sz, 2);
				}
				break;

			default:
			case 10:
				{
					pHeader->len = 127;

					uint64 sz = bufferSize;
					static_assert(sizeof(uint64) == 8, "Invalid type size!");
					SwapEndian(sz, eBigEndian);
					memcpy(pHeader + 1, (char*)&sz, 8);
				}
				break;
			}

			conn->m_pSocketSession->Send((const uint8*)pHeader, bufferSize + headerSize);
		}
		else
		{
			CryLogAlways("CSimpleHttpServerInternal::SendWebsocketData: Trying to send Websocket data on a non-websocket. Aborting send.");
		}
	}

	// delete the buffer we created in CSimpleHttpServer::SendWebsocketData on the game thread.
	m_pServer->FreeHeapBuffer(pBuffer);

	if (closeConnection)
	{
		WsDataHeader closeHeader;
		memset(&closeHeader, 0, sizeof(WsDataHeader));
		closeHeader.opcode = eWSOC_ConnectionClose;
		closeHeader.fin = 1;

		if (conn)
			conn->m_pSocketSession->Send((const uint8*)&closeHeader, sizeof(WsDataHeader));

		CloseHttpConnection(connectionID, true);
	}
}

void CSimpleHttpServerInternal::OnWebsocketUpgrade(int connectionID, IHttpWebsocketProtocol* pProtocol)
{
	if (pProtocol)
	{
		pProtocol->OnUpgrade(connectionID);
	}
}

void CSimpleHttpServerInternal::OnWebsocketReceive(int connectionID, IHttpWebsocketProtocol* pProtocol, IHttpWebsocketProtocol::SMessageData data)
{
	SCOPED_GLOBAL_LOCK;

	// Before returning from this function, we must always delete data.m_pBuffer, which was allocated in
	// CSimpleHttpServerInternal::ProcessIncomingWebSocketData, in the RecvPayload switch case.

	if (pProtocol)
	{
		pProtocol->OnReceive(connectionID, data);
	}
	if (data.m_pBuffer)
	{
		m_pServer->FreeHeapBuffer(data.m_pBuffer);
		data.m_pBuffer = NULL;
		data.m_bufferSize = 0;
	}
}

void CSimpleHttpServerInternal::OnWebsocketClose(int connectionID, IHttpWebsocketProtocol* pProtocol, bool bGraceful)
{
	if (pProtocol)
	{
		pProtocol->OnClosed(connectionID, bGraceful);
	}
}
#endif

void CSimpleHttpServerInternal::OnConnectionAccepted(IStreamSocketPtr pStreamSocket, void* pUserData)
{
	NET_ASSERT(pUserData == NULL);

	if (m_connections.size() >= MAX_HTTP_CONNECTIONS)
	{
		string response;
		BuildNormalResponse(-1, response, ISimpleHttpServer::eSC_ServiceUnavailable, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Service Unavailable</TITLE></HEAD></HTML>");
		pStreamSocket->Send((const uint8*)response.data(), response.size());
		pStreamSocket->Close();
	}
	else
	{
		// allocate new connection
		m_connections.push_back(HttpConnection(GetNewConnectionID()));

		HttpConnection* conn = &m_connections.back();

		conn->m_pSocketSession = pStreamSocket;

		conn->m_pSocketSession->SetListener(this, (void*)(EXPAND_PTR)conn->m_ID);  // in this specific application, the same listener is used for both sockets
		conn->m_pSocketSession->GetPeerAddr(conn->m_remoteAddr);

		//m_sessionState = eSS_WaitFirstRequest;
		//m_authenticationTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
		//m_lineReadingTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
		// TODO: should reset all the relevant states here

		conn->m_requestType = eRT_None;
		conn->m_receivingState = eRS_ReceivingHeaders;
		conn->m_remainingContentBytes = 0;
		conn->m_line = "";
		conn->m_content = "";
		conn->m_uri = "";

		if (m_pServer->m_pListener)
		{
			TO_GAME(&IHttpServerListener::OnClientConnected, m_pServer->m_pListener, conn->m_ID, string(RESOLVER.ToString(conn->m_remoteAddr)));
		}
	}
}

void CSimpleHttpServerInternal::OnConnectionClosed(bool graceful, void* pUserData)
{
	CloseHttpConnection((int)(TRUNCATE_PTR)pUserData, false);
}

#define TOKENSEPAS ":="    // separators that are also tokens
#define SEPARATORS " \t,;" // separators that are not tokens

string CSimpleHttpServerInternal::NextToken(string& line) const
{
	if (line.empty())
		return "";

	// first skip leading non-token separators
	size_t start = line.find_first_not_of(SEPARATORS);
	line = line.substr(start);

	if (line.empty())
		return "";

	size_t index = line.find_first_of(TOKENSEPAS);
	if (0 == index)
	{
		string token = line.substr(0, 1);
		line = line.substr(1);
		return token;
	}

	index = line.find_first_of(SEPARATORS TOKENSEPAS);
	string token = line.substr(0, index);
	line = line.substr(index);
	return token;
}

bool CSimpleHttpServerInternal::ParseHeaderLine(int connectionID, string& line)
{
	HttpConnection* conn = GetConnection(connectionID);

	if (!conn)
		return false;

	string token = NextToken(line).MakeLower();

	if (conn->m_requestType == eRT_None)
	{
		if ("get" == token)
		{
			conn->m_requestType = eRT_Get;
			conn->m_uri = NextToken(line);
			if (!conn->m_uri.empty())
			{
				token = NextToken(line).MakeLower();
				if ("http/1.0" == token || "http/1.1" == token || "" == token)
				{
					//string from(RESOLVER.ToString(m_remoteAddr).c_str());
					//TO_GAME(&IHttpServerListener::OnGetRequest, m_pServer->m_pListener, conn->m_ID, conn->m_uri);
					//if (m_sessionState == eSS_Authorized)
					//{
					//	TO_GAME_FEEDBACK_QUEUE1(m_pServer, &IHttpServerListener::OnGetRequest, m_pServer->m_pListener, conn->m_uri);
					//}
					//else if (m_sessionState == eSS_WaitFirstRequest)
					//{
					//	string response;
					//	BuildUnauthorizedResponse(response);
					//	m_pSocketSession->Send((uint8*)response.data(), response.size());

					//	m_sessionState = eSS_ChallengeSent;
					//}
					//m_lineReadingTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
					return true;
				}
			}
			BadRequestResponse(conn->m_ID);
			return false;
		}

		if ("post" == token)
		{
			conn->m_uri = NextToken(line).MakeLower();
			if ("/rpc2" == conn->m_uri)
			{
				token = NextToken(line).MakeLower();
				if ("http/1.0" == token || "http/1.1" == token || "" == token)
				{
					conn->m_requestType = eRT_Rpc;
					//if (m_sessionState == eSS_WaitFirstRequest)
					//{
					//	string response;
					//	BuildUnauthorizedResponse(response);
					//	m_pSocketSession->Send((uint8*)response.data(), response.size());

					//	m_sessionState = eSS_ChallengeSent;
					//}
					//m_lineReadingTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
					return true;
				}
			}
			BadRequestResponse(conn->m_ID);
			return false;
		}

		// other requests go here (but we probably won't support them)
		NotImplementedResponse(conn->m_ID);
		return false;
	}

	//if ("authorization" == token)
	//{
	//	if ( ":" == NextToken(line) )
	//	{
	//		token = NextToken(line).MakeLower();
	//		if ("digest" == token)
	//		{
	//			if ( ParseDigestAuthorization(line) )
	//				return true;
	//		}
	//	}
	//	BadRequest();
	//	return false;
	//}

	if ("content-length" == token)
	{
		if (conn->m_requestType != eRT_Rpc)
		{
			BadRequestResponse(conn->m_ID);
			return false;
		}

		if (":" == NextToken(line))
		{
			token = NextToken(line);
			conn->m_remainingContentBytes = atoi(token.c_str());
			if (conn->m_remainingContentBytes > 0)
				return true;
		}

		BadRequestResponse(conn->m_ID);
		return false;
	}

	if ("content-type" == token)
	{
		if (conn->m_requestType != eRT_Rpc)
		{
			BadRequestResponse(conn->m_ID);
			return false;
		}

		if (":" == NextToken(line))
			if ("text/xml" == NextToken(line).MakeLower())
				return true;

		BadRequestResponse(conn->m_ID);
		return false;
	}

#if defined(HTTP_WEBSOCKETS)
	if ("sec-websocket-protocol" == token)
	{
		if (":" == NextToken(line))
		{
			string protocolName = NextToken(line);
			std::map<string, IHttpWebsocketProtocol*>::iterator it = m_pProtocolHandlers.find(protocolName);

			if (it != m_pProtocolHandlers.end())
			{
				conn->m_websocketProtocol = it->second;
				return true;
			}
		}

		BadRequestResponse(conn->m_ID);
		return false;
	}

	if ("sec-websocket-key" == token)
	{
		if (":" == NextToken(line))
		{
			// DO NOT USE NextToken() here, it strips out defined separators, which includes things like '='.
			// Since the key can contain these characters, just assume the remainder of the line is the key.
			// We do want to strip any leading whitespace though.

			size_t start = line.find_first_not_of(" \t");
			line = line.substr(start);

			conn->m_websocketKey = line;

			return true;
		}

		BadRequestResponse(conn->m_ID);
		return false;
	}

	if ("upgrade" == token)
	{
		if (":" == NextToken(line))
		{
			if ("websocket" == NextToken(line).MakeLower())
			{
				conn->m_requestType = eRT_WebsocketUpgrade;
				return true;
			}
		}

		BadRequestResponse(conn->m_ID);
		return false;
	}
#endif

	// check for an empty line
	if ("" == token)
	{
		switch (conn->m_requestType)
		{
#if defined(HTTP_WEBSOCKETS)
		case eRT_WebsocketUpgrade:
			if (WebsocketUpgradeResponse(conn->m_ID))
			{
				conn->m_receivingState = eRS_Websocket;

				TO_GAME(&CSimpleHttpServerInternal::OnWebsocketUpgrade, this, conn->m_ID, conn->m_websocketProtocol);
			}
			break;
#endif
		case eRT_Get:
			if (m_pServer->m_pListener)
			{
				TO_GAME(&IHttpServerListener::OnGetRequest, m_pServer->m_pListener, conn->m_ID, conn->m_uri);
			}
			conn->m_requestType = eRT_None;
			break;

		case eRT_Rpc:
			conn->m_receivingState = eRS_ReceivingContent;
			break;

		default:
			BadRequestResponse(conn->m_ID);
			return false;
		}

		return true;
	}

	// ignore the remaining header lines
	return true;
}

#if defined(HTTP_WEBSOCKETS)

void CSimpleHttpServerInternal::ProcessIncomingWebSocketData(int connectionID, const uint8* pData, size_t nSize)
{
	HttpConnection* pConnection = GetConnection(connectionID);
	if (!pConnection)
	{
		return;
	}
	if (pConnection->m_receivingState != eRS_Websocket)
	{
		return;
	}

	// Add data to the list
	pConnection->AddWsReadChunk(pData, nSize);

	bool bDone = false;
	while (!bDone)
	{
		switch (pConnection->m_WsConnectState)
		{
		case eWSCS_RecvHeader:
			{
				// try to collate a header (2 bytes)
				if (pConnection->CollateWsReadChunks((char*)&pConnection->m_WsRecvCurrentHeader, sizeof(WsDataHeader)))
				{
					switch (pConnection->m_WsRecvCurrentHeader.opcode)
					{
					case eWSOC_Text:
						{
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: OPCODE Text, len=%d, connection %d", pConnection->m_WsRecvCurrentHeader.len, connectionID);
							pConnection->m_WsRecvCombinedMessageType = IHttpWebsocketProtocol::eMT_Text;
							pConnection->m_WsConnectState = eWSCS_RecvExtendedSize;
						}
						break;
					case eWSOC_Binary:
						{
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: OPCODE Binary, len=%d, connection %d", pConnection->m_WsRecvCurrentHeader.len, connectionID);
							pConnection->m_WsRecvCombinedMessageType = IHttpWebsocketProtocol::eMT_Binary;
							pConnection->m_WsConnectState = eWSCS_RecvExtendedSize;
						}
						break;
					case eWSOC_Continuation:
						{
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: OPCODE Continuation, len=%d, connection %d", pConnection->m_WsRecvCurrentHeader.len, connectionID);
							pConnection->m_WsConnectState = eWSCS_RecvExtendedSize;
						}
						break;
					case eWSOC_ConnectionClose:
						{
							// client has closed the connection. Do the same on this end
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: OPCODE: ConnectionClose, connection %d", connectionID);
							CloseHttpConnection(connectionID, true);
							bDone = true;
						}
						break;
					case eWSOC_Ping:
						{
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: OPCODE: Ping, TODO, connection %d", connectionID);
						}
						break;
					case eWSOC_Pong:
						{
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: OPCODE: Pong. TODO, connection %d", connectionID);
						}
						break;
					default:
						{
							// invalid opcode. Send a ConnectionClose back to client.
							//CryLogAlways("CSimpleHttpServerInternal::ProcessIncomingWebSocketData: INVALID OPCODE 0x%02X, connection %d", pConnection->m_WsRecvCurrentHeader.opcode, connectionID);
							//CryLogAlways("Header: fin=%d, mask=%d, len=%d", pConnection->m_WsRecvCurrentHeader.fin, pConnection->m_WsRecvCurrentHeader.mask, pConnection->m_WsRecvCurrentHeader.len);

							pConnection->m_WsConnectState = eWSCS_SendGracefulClose;
						}
						break;
					}
				}
				else
				{
					// not enough data for a complete header.
					bDone = true;
				}
			}
			break;
		case eWSCS_RecvExtendedSize:
			{
				// pConnection->m_WsRecvCurrentHeader must be valid if we reached here.

				if (pConnection->m_WsRecvCurrentHeader.len < 126)
				{
					pConnection->m_WsRecvCurrentPayloadSize = pConnection->m_WsRecvCurrentHeader.len;
					pConnection->m_WsConnectState = eWSCS_RecvMask;
				}
				else if (pConnection->m_WsRecvCurrentHeader.len == 126)
				{
					// collate 2 bytes
					unsigned short len;
					if (pConnection->CollateWsReadChunks((char*)&len, 2))
					{
						SwapEndian(len, eBigEndian);
						pConnection->m_WsRecvCurrentPayloadSize = len;
						pConnection->m_WsConnectState = eWSCS_RecvMask;
					}
					else
					{
						// not enough data for extended size
						bDone = true;
					}
				}
				else if (pConnection->m_WsRecvCurrentHeader.len == 127)
				{
					// collate 8 bytes
					uint64 len64;
					if (pConnection->CollateWsReadChunks((char*)&len64, 8))
					{
						SwapEndian(len64, eBigEndian);
						pConnection->m_WsRecvCurrentPayloadSize = len64;
						pConnection->m_WsConnectState = eWSCS_RecvMask;
					}
					else
					{
						// not enough data for extended size
						bDone = true;
					}
				}
				else
				{
					// invalid length!
					pConnection->m_WsConnectState = eWSCS_SendGracefulClose;
				}
			}
			break;
		case eWSCS_RecvMask:
			{
				//-- pConnection->m_WsRecvCurrentHeader must be valid if we reach here

				if (pConnection->m_WsRecvCurrentHeader.mask)
				{
					if (pConnection->CollateWsReadChunks((char*)pConnection->m_WsRecvCurrentMask, 4))
					{
						pConnection->m_WsConnectState = eWSCS_RecvPayload;
					}
					else
					{
						// not enough data for mask
						bDone = true;
					}
				}
			}
			break;
		case eWSCS_RecvPayload:
			{
				// pConnection->m_WsRecvCurrentPayloadSize must be valid if we reached here

				std::size_t oldSize = pConnection->m_WsRecvCombinedMessageBuffer.size();
				pConnection->m_WsRecvCombinedMessageBuffer.resize(oldSize + (size_t)pConnection->m_WsRecvCurrentPayloadSize);

				char* pWorkBuffer = &pConnection->m_WsRecvCombinedMessageBuffer[oldSize];
				if (pWorkBuffer)
				{
					if (pConnection->CollateWsReadChunks(pWorkBuffer, pConnection->m_WsRecvCurrentPayloadSize))
					{
						if (pConnection->m_WsRecvCurrentHeader.mask)
						{
							for (uint64 i = 0; i < pConnection->m_WsRecvCurrentPayloadSize; ++i)
							{
								pWorkBuffer[i] ^= pConnection->m_WsRecvCurrentMask[i % 4];
							}
						}
						if (pConnection->m_WsRecvCurrentHeader.fin)
						{
							// end of message sequence reached. Send the combined message to the protocol handler
							if (pConnection->m_websocketProtocol)
							{
								IHttpWebsocketProtocol::SMessageData data;
								data.m_eType = pConnection->m_WsRecvCombinedMessageType;
								data.m_bufferSize = pConnection->m_WsRecvCombinedMessageBuffer.size();
								data.m_pBuffer = (char*)m_pServer->AllocateHeapBuffer(data.m_bufferSize + 1);
								// Why +1? Allocate 1 byte extra, just so we can put a safety null terminator on the end.
								// This is not strictly part of the protocol, but it'll stop people making silly mistakes if they assume the buffer is a
								// null-terminated string.
								// Note that we do not include this extra byte in the data.m_bufferSize we send to OnWebsocketReceive!
								memcpy(data.m_pBuffer, &pConnection->m_WsRecvCombinedMessageBuffer[0], data.m_bufferSize);
								data.m_pBuffer[data.m_bufferSize] = '\0';

								// DO NOT DELETE data.m_pBuffer! It will be deleted in CSimpleHttpServer::OnWebsocketReceive, after it has been sent to the game thread.

								TO_GAME(&CSimpleHttpServerInternal::OnWebsocketReceive, this, connectionID, pConnection->m_websocketProtocol, (IHttpWebsocketProtocol::SMessageData)data);
							}

							//-- clear the combined message buffer and look for a new header
							pConnection->ClearWebsocketProcessingState();
						}
						else
						{
							pConnection->m_WsConnectState = eWSCS_RecvHeader;
						}
					}
					else
					{
						// not enough data for full payload
						bDone = true;
					}
				}
				else
				{
					bDone = true;
				}
			}
			break;
		case eWSCS_SendGracefulClose:
			{
				WsDataHeader closeHeader;
				memset(&closeHeader, 0, sizeof(WsDataHeader));
				closeHeader.opcode = eWSOC_ConnectionClose;
				closeHeader.fin = 1;

				pConnection->m_pSocketSession->Send((const uint8*)&closeHeader, sizeof(WsDataHeader));

				CloseHttpConnection(connectionID, true);

				bDone = true;
			}
			break;
		case eWSCS_Invalid:
		default:
			bDone = true;
			break;
		}
	}
}

#endif

void CSimpleHttpServerInternal::OnIncomingData(const uint8* pData, size_t nSize, void* pUserData)
{
	HttpConnection* conn = GetConnection((int)(TRUNCATE_PTR)pUserData);

	if (!conn)
		return;

	const uint8* pBuffer = pData;
	size_t nLength = nSize;
	while (nLength > 0)
	{
		if (conn->m_receivingState == eRS_Ignoring)
			break;
#ifdef HTTP_WEBSOCKETS
		else if (conn->m_receivingState == eRS_Websocket)
		{
			ProcessIncomingWebSocketData(conn->m_ID, pData, nSize);
			pBuffer += nLength;
			nLength = 0;
			break;
		}
#endif
		else if (conn->m_receivingState == eRS_ReceivingHeaders)
		{
			if (CR != pBuffer[0] && LF != pBuffer[0] && '\0' != pBuffer[0])
				conn->m_line += pBuffer[0];
			else if (CR == pBuffer[0])
			{
				if (conn->m_line.empty() || CR != conn->m_line.c_str()[conn->m_line.length() - 1])
					conn->m_line += pBuffer[0];
				else
				{
					BadRequestResponse(conn->m_ID);
					break;
				}
			}
			else if (LF == pBuffer[0])
			{
				if (!conn->m_line.empty() && CR == conn->m_line.c_str()[conn->m_line.length() - 1])
				{
					string sub = conn->m_line.substr(0, conn->m_line.length() - 1);
					if (!ParseHeaderLine(conn->m_ID, sub))
					{
						conn->m_receivingState = eRS_Ignoring;
						break;
					}

					conn->m_line = "";
				}
				else
				{
					conn->m_receivingState = eRS_Ignoring;
					BadRequestResponse(conn->m_ID);
					break;
				}
			}
			else // '\0'
			{
				conn->m_receivingState = eRS_Ignoring;
				BadRequestResponse(conn->m_ID);
				break;
			}

			++pBuffer;
			--nLength;
		}
		else if (conn->m_receivingState == eRS_ReceivingContent)
		{
			size_t toAppend = min(nLength, conn->m_remainingContentBytes);
			conn->m_content.append((char*)pBuffer, toAppend);
			pBuffer += toAppend;
			nLength -= toAppend;
			conn->m_remainingContentBytes -= toAppend;
			if (conn->m_remainingContentBytes == 0)
			{
				switch (conn->m_requestType)
				{
				case eRT_Rpc:
					//if (m_sessionState == eSS_Authorized)
					//string from(RESOLVER.ToString(m_remoteAddr).c_str());
					if (m_pServer->m_pListener)
					{
						TO_GAME(&IHttpServerListener::OnRpcRequest, m_pServer->m_pListener, conn->m_ID, conn->m_content);
					}
					break;
				}

				conn->m_content = ""; // discard content even if not handled
				conn->m_requestType = eRT_None;
				conn->m_receivingState = eRS_ReceivingHeaders;
				conn->m_uri = "";
			}
		}
	}
}

void CSimpleHttpServerInternal::BuildNormalResponse(int connectionID, string& response, ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, const string& content)
{
	response = "";

	response += "HTTP/1.1 ";
	response += s_statusCodes[statusCode];
	response += CRLF;

	response += "Server: CryHttp/0.0.1";
	response += CRLF;

	//response += "Connection: close";
	//response += CRLF;

	response += "Content-Type: text/";
	response += s_contentTypes[contentType];
	response += CRLF;

	char buf[33];
	ltoa(content.size(), buf, 10);
	response += "Content-Length: ";
	response += buf;
	response += CRLF;
	response += CRLF;

	response += content;
	//response += CRLF;
	//response += CRLF;
}

void CSimpleHttpServerInternal::UnauthorizedResponse(int connectionID, string& response)
{
	HttpConnection* conn = GetConnection(connectionID);

	TNetAddress remoteAddr;

	if (conn)
		remoteAddr = conn->m_remoteAddr;

	response = "";

	response += "HTTP/1.1 401 Unauthorized";
	response += CRLF;

	response += "Server: CryHttp/0.0.1";
	response += CRLF;

	char timestamp[33];
	cry_sprintf(timestamp, "%f", gEnv->pTimer->GetAsyncCurTime());
	string raw = RESOLVER.ToNumericString(remoteAddr) + ":" + timestamp + ":" + "luolin";
	{
		CWhirlpoolHash hash(raw);
		m_nonce = hash.GetHumanReadable();
	}
	{
		CWhirlpoolHash hash((uint8*)timestamp, strlen(timestamp));
		m_opaque = hash.GetHumanReadable();
	}
	response += "WWW-Authenticate: Digest ";
	response += "realm=\"";
	response += m_realm;
	response += "\", ";
	response += "nonce=\"";
	response += m_nonce;
	response += "\", ";
	response += "opaque=\"";
	response += m_opaque;
	response += "\"";
	response += CRLF;
	response += CRLF;
}

void CSimpleHttpServerInternal::BadRequestResponse(int connectionID)
{
	HttpConnection* conn = GetConnection(connectionID);

	if (!conn || !conn->m_pSocketSession)
		return;

	string response;
	BuildNormalResponse(connectionID, response, ISimpleHttpServer::eSC_BadRequest, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Bad Request</TITLE></HEAD></HTML>");
	conn->m_pSocketSession->Send((uint8*)response.data(), response.size());

	CloseHttpConnection(connectionID, true);
}

#if defined(HTTP_WEBSOCKETS)
bool CSimpleHttpServerInternal::WebsocketUpgradeResponse(int connectionID)
{
	HttpConnection* conn = GetConnection(connectionID);

	if (!conn || !conn->m_pSocketSession)
	{
		return false;
	}

	// build up a response and send eSC_SwitchingProtocols with the appropriate headers.

	const size_t sha_len = 5;
	unsigned message_digest[sha_len];

	// Custom SHA1 key-gen from wsKey
	string wskey = conn->m_websocketKey + s_WsGUID;
	CrySHA1::sha1Calc(wskey.c_str(), message_digest);

	if (eBigEndian)
	{
		for (int i = 0; i < 5; i++)
		{
			message_digest[i] = (message_digest[i] & 0xff) << 24 | (message_digest[i] & 0xff00) << 8 | (message_digest[i] & 0xff0000) >> 8 | (message_digest[i] & 0xff000000) >> 24;
		}
	}

	// Base-64 encode
	size_t lenBase64 = Base64::encodedsize_base64(sha_len * 4);
	char* pBase64 = (char*)m_pServer->AllocateHeapBuffer(lenBase64 + 1);
	Base64::encode_base64(pBase64, (const char* const)message_digest, sha_len * 4, true);

	string protocolName = "";
	std::map<string, IHttpWebsocketProtocol*>::iterator it;
	for (it = m_pProtocolHandlers.begin(); it != m_pProtocolHandlers.end(); ++it)
	{
		if (conn->m_websocketProtocol == it->second)
		{
			protocolName = it->first;
			break;
		}
	}

	// Send response
	string response = "";

	response += "HTTP/1.1 ";
	response += s_statusCodes[ISimpleHttpServer::eSC_SwitchingProtocols];
	response += CRLF;

	response += "Server: CryHttp/0.0.1";
	response += CRLF;

	response += "Upgrade: websocket";
	response += CRLF;

	response += "Connection: Upgrade";
	response += CRLF;

	response += "Sec-WebSocket-Accept: " + string(pBase64);
	response += CRLF;

	response += "Sec-WebSocket-Protocol: " + protocolName;
	response += CRLF;

	response += CRLF;

	conn->m_pSocketSession->Send((uint8*)response.data(), response.size());
	conn->m_sessionState = eSS_Unsessioned;

	m_pServer->FreeHeapBuffer(pBase64);

	return true;
}
#endif

void CSimpleHttpServerInternal::NotImplementedResponse(int connectionID)
{
	HttpConnection* conn = GetConnection(connectionID);

	if (!conn || !conn->m_pSocketSession)
		return;

	string response;
	BuildNormalResponse(connectionID, response, ISimpleHttpServer::eSC_NotImplemented, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Not Implemented</TITLE></HEAD></HTML>");
	conn->m_pSocketSession->Send((uint8*)response.data(), response.size());

	CloseHttpConnection(connectionID, true);
}

bool CSimpleHttpServerInternal::ParseDigestAuthorization(int connectionID, string& line)
{
	// TODO:
	return false;
}
