// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __REMOTECONTROL_H__
#define __REMOTECONTROL_H__

#pragma once

#include <CryNetwork/IRemoteControl.h>

#include "Protocol.h"

class CRemoteControlSystem : public IRemoteControlSystem
{
public:
	static CRemoteControlSystem& GetSingleton();

	IRemoteControlServer*        GetServerSingleton();
	IRemoteControlClient*        GetClientSingleton();

private:
	CRemoteControlSystem();
	~CRemoteControlSystem();

	static CRemoteControlSystem s_singleton;
};

class CRemoteControlServer;

class CRemoteControlServerInternal : public IStreamListener
{
	friend class CRemoteControlServer;

public:
	void Start(uint16 serverPort, string password);
	void Stop();
	void SendResult(uint32 commandId, string result);

	void OnConnectionAccepted(IStreamSocketPtr pStreamSocket, void* pUserData);
	void OnConnectCompleted(bool succeeded, void* pUserData) { NET_ASSERT(0); }
	void OnConnectionClosed(bool graceful, void* pUserData);
	void OnIncomingData(const uint8* pData, size_t nSize, void* pUserData);

	void AddRef() const  {}
	void Release() const {}
	bool IsDead() const  { return false; }

private:
	CRemoteControlServerInternal(CRemoteControlServer* pServer);
	~CRemoteControlServerInternal();

	static void AuthenticationTimeoutTimer(NetTimerId, void*, CTimeValue);
	void GetBindAddress(TNetAddress& addr, uint16 port);

	void Reset();

	template<typename T>
	void GC_AuthenticationError()
	{
		if (m_pSocketSession)
		{
			T msg;
			m_pSocketSession->Send((uint8*)&msg, sizeof(msg));
			m_pSocketSession->Close();
			m_pSocketSession = NULL;
		}

		Reset();
	}

	CRemoteControlServer*   m_pServer;

	IStreamSocketPtr        m_pSocketListen;
	IStreamSocketPtr        m_pSocketSession;

	TNetAddress             m_remoteAddr;

	string                  m_password;
	SRCONServerMsgChallenge m_challenge;

	enum ESessionState {eSS_Unsessioned, eSS_ChallengeSent, eSS_Authorized};
	ESessionState m_sessionState;

	enum EReceivingState {eRS_ReceivingHead, eRS_ReceivingBody};
	EReceivingState    m_receivingState;

	uint8              m_receivingHead[sizeof(SRCONMessageHdr)];
	std::vector<uint8> m_receivingBody;

	size_t             m_bufferIndicator; // buffer indicator
	size_t             m_amountRemaining; // in bytes

	NetTimerId         m_authenticationTimeoutTimer;
};

class CRemoteControlServer : public IRemoteControlServer
{
	friend class CRemoteControlServerInternal;

public:
	static CRemoteControlServer& GetSingleton();

	void                         Start(uint16 serverPort, const string& password, IRemoteControlServerListener* pListener);
	void                         Stop();
	void                         SendResult(uint32 commandId, const string& result);

private:
	CRemoteControlServer();
	~CRemoteControlServer();

	CRemoteControlServerInternal  m_internal;

	IRemoteControlServerListener* m_pListener;

	static CRemoteControlServer   s_singleton;
};

class CRemoteControlClient;

class CRemoteControlClientInternal : public IStreamListener
{
	friend class CRemoteControlClient;

public:
	void Connect(string serverAddr, uint16 serverPort, string password);
	void Disconnect();
	void SendCommand(uint32 commandId, string command);

	void OnConnectionAccepted(IStreamSocketPtr pStreamSocket, void* pUserData) { NET_ASSERT(0); }
	void OnConnectCompleted(bool succeeded, void* pUserData);
	void OnConnectionClosed(bool graceful, void* pUserData);
	void OnIncomingData(const uint8* pData, size_t nSize, void* pUserData);

	void AddRef() const  {}
	void Release() const {}
	bool IsDead() const  { return false; }

private:
	CRemoteControlClientInternal(CRemoteControlClient* pClient);
	~CRemoteControlClientInternal();

	void Reset();

	CRemoteControlClient* m_pClient;

	IStreamSocketPtr      m_pSocketSession;

	string                m_password;

	enum EConnectionState {eCS_NotConnected, eCS_InProgress, eCS_Established};
	EConnectionState m_connectionState;

	enum ESessionState {eSS_Unsessioned, eSS_ChallengeWait, eSS_DigestSent, eSS_Authorized};
	ESessionState m_sessionState;

	enum EReceivingState {eRS_ReceivingHead, eRS_ReceivingBody};
	EReceivingState          m_receivingState;

	uint8                    m_receivingHead[sizeof(SRCONMessageHdr)];
	std::vector<uint8>       m_receivingBody;

	size_t                   m_bufferIndicator; // buffer indicator
	size_t                   m_amountRemaining; // in bytes

	std::map<uint32, string> m_pendingCommands; // <commandID, command>
	uint32                   m_resultCommandId;
};

class CRemoteControlClient : public IRemoteControlClient
{
	friend class CRemoteControlClientInternal;

public:
	static CRemoteControlClient& GetSingleton();

	void                         Connect(const string& serverAddr, uint16 serverPort, const string& password, IRemoteControlClientListener* pListener);
	void                         Disconnect();
	uint32                       SendCommand(const string& command);

private:
	CRemoteControlClient();
	~CRemoteControlClient();

	CRemoteControlClientInternal  m_internal;

	IRemoteControlClientListener* m_pListener;

	static CRemoteControlClient   s_singleton;
};

#endif
