// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SIMPLEHTTPSERVER_H__
#define __SIMPLEHTTPSERVER_H__

#pragma once

#include <CryNetwork/ISimpleHttpServer.h>
#include <vector>

#define WEBSOCKET_EXTRA_HEADER_SPACE 10

static const string USERNAME = "anonymous";
static const int MAX_HTTP_CONNECTIONS = 16;                  // arbitrary figure to prevent bugs causing things to grow out of control
static const int WEBSOCKET_MAX_MESSAGE_SIZE = (1000 * 1024); // arbitrary maximum message size for all websocket payloads in a sequence

class CSimpleHttpServer;

class CSimpleHttpServerInternal : public IStreamListener
{
	friend class CSimpleHttpServer;

	static const int HTTP_CONNECTIONS_RESERVE = 4;

public:
	void Start(uint16 port, string password);
	void Stop();
	void SendResponse(int connectionID, ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, string content, bool closeConnection);
	void SendWebpage(int connectionID, string webpage);

#ifdef HTTP_WEBSOCKETS
	void AddWebsocketProtocol(string protocolName, IHttpWebsocketProtocol* pProtocol);
	void SendToWebsocket(int connectionID, IHttpWebsocketProtocol::EMessageType type, char* pBuffer, uint32 bufferSize, bool closeConnection);
	void OnWebsocketUpgrade(int connectionID, IHttpWebsocketProtocol* pProtocol);
	void OnWebsocketReceive(int connectionID, IHttpWebsocketProtocol* pProtocol, IHttpWebsocketProtocol::SMessageData data);
	void OnWebsocketClose(int connectionID, IHttpWebsocketProtocol* pProtocol, bool bGraceful);
#endif

	void OnConnectionAccepted(IStreamSocketPtr pStreamSocket, void* pUserData);
	void OnConnectCompleted(bool succeeded, void* pUserData) { NET_ASSERT(0); }
	void OnConnectionClosed(bool graceful, void* pUserData);
	void OnIncomingData(const uint8* pData, size_t nSize, void* pUserData);

	void AddRef() const  {}
	void Release() const {}
	bool IsDead() const  { return false; }

private:
	enum ESessionState {eSS_Unsessioned, eSS_WaitFirstRequest, eSS_ChallengeSent, eSS_Authorized, eSS_IgnorePending};
	enum EReceivingState {eRS_ReceivingHeaders, eRS_ReceivingContent, eRS_Ignoring, eRS_Websocket};
	enum ERequestType {eRT_None, eRT_Get, eRT_Rpc, eRT_WebsocketUpgrade};

#ifdef HTTP_WEBSOCKETS
	std::map<string, IHttpWebsocketProtocol*> m_pProtocolHandlers;

	enum EWsOpCode { eWSOC_Continuation = 0x0, eWSOC_Text = 0x1, eWSOC_Binary = 0x2, eWSOC_ConnectionClose = 0x8, eWSOC_Ping = 0x9, eWSOC_Pong = 0xa, eWSOC_Max = 0x10 };
	static const string s_WsOpCodes[eWSOC_Max];

	enum EWsConnectState { eWSCS_RecvHeader, eWSCS_RecvExtendedSize, eWSCS_RecvMask, eWSCS_RecvPayload, eWSCS_SendGracefulClose, eWSCS_Invalid };
	static const string s_WsConnectStates[eWSCS_Invalid];
	static const string s_WsGUID;

	#pragma pack(push, 1)
	#if !defined(NEED_ENDIAN_SWAP)
	struct WsDataHeader
	{
		unsigned char opcode : 4;
		unsigned char rsv3   : 1;
		unsigned char rsv2   : 1;
		unsigned char rsv1   : 1;
		unsigned char fin    : 1;
		unsigned char len    : 7;
		unsigned char mask   : 1;
	};
	#else
	struct WsDataHeader
	{
		unsigned char fin    : 1;
		unsigned char rsv1   : 1;
		unsigned char rsv2   : 1;
		unsigned char rsv3   : 1;
		unsigned char opcode : 4;
		unsigned char mask   : 1;
		unsigned char len    : 7;
	};
	#endif
	#pragma pack(pop)

	void ProcessIncomingWebSocketData(int connectionID, const uint8* pData, size_t nSize);
	bool WebsocketUpgradeResponse(int connectionID);

#endif

	struct HttpConnection
	{
		HttpConnection(int ID);

		int              m_ID;

		IStreamSocketPtr m_pSocketSession;

		TNetAddress      m_remoteAddr;

		ESessionState    m_sessionState;
		EReceivingState  m_receivingState;
		ERequestType     m_requestType;

		size_t           m_remainingContentBytes;

		float            m_authenticationTimeoutStart;
		float            m_lineReadingTimeoutStart;

		string           m_line, m_content, m_uri;

#if defined(HTTP_WEBSOCKETS)
		IHttpWebsocketProtocol* m_websocketProtocol;
		string                  m_websocketKey;

		struct WsReadChunk
		{
			WsReadChunk(const uint8* pData, size_t nSize);

			uint8* m_pBuffer;
			uint64 m_bufferSize;
			uint64 m_readOffset;
		};

		typedef std::vector<WsReadChunk> TWsReadChunkList;
		TWsReadChunkList m_WsReadChunks;

		EWsConnectState  m_WsConnectState;

		WsDataHeader     m_WsRecvCurrentHeader;
		uint64           m_WsRecvCurrentPayloadSize;
		char             m_WsRecvCurrentMask[4];

		typedef std::vector<char> TWsRecvMessageBuffer;
		TWsRecvMessageBuffer                 m_WsRecvCombinedMessageBuffer;
		IHttpWebsocketProtocol::EMessageType m_WsRecvCombinedMessageType;

		void ClearAllWebsocketData();
		void ClearWebsocketProcessingState();

		bool AddWsReadChunk(const uint8* pBuffer, size_t nSize);
		bool CollateWsReadChunks(char* pBuffer, uint64 bufferSize);
#endif
	};

	CSimpleHttpServerInternal(CSimpleHttpServer* pServer);
	~CSimpleHttpServerInternal();

	void            GetBindAddress(TNetAddress& addr, uint16 port) const;
	int             GetNewConnectionID() { return m_connectionID++; }
	HttpConnection* GetConnection(int connectionID);

	void            UpdateClosedConnections();
	void            CloseHttpConnection(int connectionID, bool bGraceful);

	void            BuildNormalResponse(int connectionID, string& response, ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, const string& content);
	void            UnauthorizedResponse(int connectionID, string& response);
	void            BadRequestResponse(int connectionID); // helper
	void            NotImplementedResponse(int connectionID);

	string          NextToken(string& line) const;
	bool            ParseHeaderLine(int connectionID, string& line);
	void            IgnorePending(int connectionID);

	bool            ParseDigestAuthorization(int connectionID, string& line);

	CSimpleHttpServer*          m_pServer;

	IStreamSocketPtr            m_pSocketListen;

	string                      m_password;
	string                      m_hostname;

	string                      m_realm;
	string                      m_nonce;
	string                      m_opaque;

	CryMutex                    m_mutex;

	std::vector<IStreamSocketPtr> m_closedConnections;
	std::vector<HttpConnection> m_connections;
	int                         m_connectionID;

	static const string         s_statusCodes[];
	static const string         s_contentTypes[];
};

class CSimpleHttpServer : public ISimpleHttpServer
{
	friend class CSimpleHttpServerInternal;

public:
	static CSimpleHttpServer& GetSingleton();

#ifdef HTTP_WEBSOCKETS
	void  AddWebsocketProtocol(const string& protocolName, IHttpWebsocketProtocol* pProtocol);
	void  SendWebsocketData(int connectionID, IHttpWebsocketProtocol::SMessageData& data, bool closeConnection);

	void* AllocateHeapBuffer(uint32 bufferSize);
	void  FreeHeapBuffer(void* pBuffer);
#endif

	void Start(uint16 port, const string& password, IHttpServerListener* pListener);
	void Stop();
	void Quit();
	void Tick();
	void SendResponse(int connectionID, EStatusCode statusCode, EContentType contentType, const string& content, bool closeConnection);
	void SendWebpage(int connectionID, const string& webpage);

private:
	CSimpleHttpServer();
	~CSimpleHttpServer();

	CSimpleHttpServerInternal m_internal;

	IHttpServerListener*      m_pListener;

#if defined(HTTP_WEBSOCKETS)
	static const int    HEAP_RESERVE_SIZE = 2048;
	static const int    HEAP_MAX_SIZE = 4 * 1024 * 1024;
	IGeneralMemoryHeap* m_pWsAllocHeap;
#endif

	static CSimpleHttpServer s_singleton;
};

#endif
