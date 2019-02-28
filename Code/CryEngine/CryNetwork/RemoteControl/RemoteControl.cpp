// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Network.h"
#include "RemoteControl.h"

#pragma warning(disable:4355)

#define RCON_MAX_IP_EXPECTED  (16)
#define RCON_MAX_ADDRESS_SIZE (RCON_MAX_IP_EXPECTED + 6 + 1)

CRemoteControlSystem CRemoteControlSystem::s_singleton;

CRemoteControlSystem& CRemoteControlSystem::GetSingleton()
{
	return CRemoteControlSystem::s_singleton;
}

CRemoteControlSystem::CRemoteControlSystem()
{

}

CRemoteControlSystem::~CRemoteControlSystem()
{

}

IRemoteControlServer* CRemoteControlSystem::GetServerSingleton()
{
	return &CRemoteControlServer::GetSingleton();
}

IRemoteControlClient* CRemoteControlSystem::GetClientSingleton()
{
	return &CRemoteControlClient::GetSingleton();
}

CRemoteControlServer CRemoteControlServer::s_singleton;

CRemoteControlServer& CRemoteControlServer::GetSingleton()
{
	return CRemoteControlServer::s_singleton;
}

CRemoteControlServer::CRemoteControlServer() : m_internal(this)
{
	m_pListener = NULL;
}

CRemoteControlServer::~CRemoteControlServer()
{

}

void CRemoteControlServer::Start(uint16 serverPort, const string& password, IRemoteControlServerListener* pListener)
{
	m_pListener = pListener;

	FROM_GAME(&CRemoteControlServerInternal::Start, &m_internal, serverPort, password);
}

void CRemoteControlServer::Stop()
{
	FROM_GAME(&CRemoteControlServerInternal::Stop, &m_internal);
}

void CRemoteControlServer::SendResult(uint32 commandId, const string& result)
{
	FROM_GAME(&CRemoteControlServerInternal::SendResult, &m_internal, commandId, result);
}

CRemoteControlServerInternal::CRemoteControlServerInternal(CRemoteControlServer* pServer)
{
	m_pServer = pServer;

	m_sessionState = eSS_Unsessioned;

	memset(m_receivingHead, 0, sizeof(m_receivingHead));
	m_receivingState = eRS_ReceivingHead;
	m_bufferIndicator = 0;
	m_amountRemaining = sizeof(m_receivingHead);

	m_authenticationTimeoutTimer = 0;
}

CRemoteControlServerInternal::~CRemoteControlServerInternal()
{

}

void CRemoteControlServerInternal::GetBindAddress(TNetAddress& addr, uint16 port)
{
	addr = TNetAddress(SIPv4Addr(0, port));
#if CRY_PLATFORM_WINDOWS                      // On windows we should check the sv_bind variable and attempt to play nice and bind to the address requested
	TNetAddressVec addrs;
	char address[RCON_MAX_ADDRESS_SIZE];
	CNetAddressResolver resolver;

	cry_sprintf(address, "0.0.0.0:%d", port);

	ICVar* pCVar = gEnv->pConsole->GetCVar("sv_bind");
	if (pCVar && pCVar->GetString())
	{
		if (strlen(pCVar->GetString()) <= RCON_MAX_IP_EXPECTED)
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

void CRemoteControlServerInternal::Start(uint16 serverPort, string password)
{
	if (m_pSocketListen != NULL)
	{
		TO_GAME(&IRemoteControlServerListener::OnStartResult, m_pServer->m_pListener,
		        false, IRemoteControlServerListener::eRD_AlreadyStarted);
		return;
	}

	m_pSocketListen = CreateStreamSocket(TNetAddress(SIPv4Addr(0, serverPort)));
	NET_ASSERT(m_pSocketListen != NULL);

	TNetAddress bindTo;
	GetBindAddress(bindTo, serverPort);

	bool r = m_pSocketListen->Listen(bindTo);
	if (!r)
	{
		m_pSocketListen->Close();
		m_pSocketListen = NULL;
		TO_GAME(&IRemoteControlServerListener::OnStartResult, m_pServer->m_pListener,
		        false, IRemoteControlServerListener::eRD_Failed);
		return;
	}

	m_pSocketListen->SetListener(this);

	m_password = password;

	TO_GAME(&IRemoteControlServerListener::OnStartResult, m_pServer->m_pListener,
	        true, IRemoteControlServerListener::eRD_Okay);
}

void CRemoteControlServerInternal::Reset()
{
	m_sessionState = eSS_Unsessioned;
	m_remoteAddr = TNetAddress(SIPv4Addr());

	memset(m_receivingHead, 0, sizeof(m_receivingHead));
	m_receivingState = eRS_ReceivingHead;
	m_bufferIndicator = 0;
	m_amountRemaining = sizeof(m_receivingHead);

	m_receivingBody.resize(0);

	TIMER.CancelTimer(m_authenticationTimeoutTimer);
}

void CRemoteControlServerInternal::Stop()
{
	if (m_pSocketSession)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;
	}

	if (m_pSocketListen)
	{
		m_pSocketListen->Close();
		m_pSocketListen = NULL;
	}

	Reset();
}

void CRemoteControlServerInternal::AuthenticationTimeoutTimer(NetTimerId id, void* p, CTimeValue)
{
	CRemoteControlServerInternal* pThis = (CRemoteControlServerInternal*) p;
	if (pThis->m_authenticationTimeoutTimer != id)
		return;
	TO_GAME(&CRemoteControlServerInternal::GC_AuthenticationError<SRCONServerMsgAuthTimeout>, pThis);
}

void CRemoteControlServerInternal::SendResult(uint32 commandId, string result)
{
	if (!m_pSocketSession || m_sessionState != eSS_Authorized)
		return;
	SRCONServerMsgRConResult msg_result;
	msg_result.commandId = commandId;
	msg_result.resultLen = result.size();
	m_pSocketSession->Send((uint8*)&msg_result, sizeof(msg_result));
	m_pSocketSession->Send((const uint8*)result.data(), result.size());
}

void CRemoteControlServerInternal::OnConnectionAccepted(IStreamSocketPtr pStreamSocket, void* pUserData)
{
	if (m_pSocketSession)
	{
		SRCONServerMsgInSession insession;
		pStreamSocket->Send((uint8*)&insession, sizeof(insession));
		pStreamSocket->Close();
	}
	else
	{
		m_pSocketSession = pStreamSocket;

		m_pSocketSession->SetListener(this); // in this specific application, the same listener is used for both sockets
		m_pSocketSession->GetPeerAddr(m_remoteAddr);

		memset(m_receivingHead, 0, sizeof(m_receivingHead));
		m_receivingState = eRS_ReceivingHead;
		m_bufferIndicator = 0;
		m_amountRemaining = sizeof(m_receivingHead);

		CMTRand_int32 r;
		for (size_t i = 0; i < 4; ++i)
			*(uint32*)(m_challenge.challenge + i * sizeof(uint32)) = r.GenerateUint32();
		m_pSocketSession->Send((uint8*)&m_challenge, sizeof(m_challenge));
		m_sessionState = eSS_ChallengeSent;
		m_authenticationTimeoutTimer = TIMER.ADDTIMER(g_time + 2.0f, AuthenticationTimeoutTimer, this, "CRemoteControlServerInternal::OnConnectionAccepted() m_authenticationTimeoutTimer");
	}
}

void CRemoteControlServerInternal::OnConnectionClosed(bool graceful, void* pUserData)
{
	if (m_pSocketSession)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;

		if (m_sessionState == eSS_Authorized)
		{
			string from(RESOLVER.ToString(m_remoteAddr).c_str());
			TO_GAME(&IRemoteControlServerListener::OnAuthorizedClientLeft, m_pServer->m_pListener, from);
		}

		Reset();
	}
}

void CRemoteControlServerInternal::OnIncomingData(const uint8* pData, size_t nSize, void* pUserData)
{
	const uint8* pBuffer = pData;
	size_t nLength = nSize;
	while (nLength > 0)
	{
		size_t copy = min(m_amountRemaining, nLength);
		switch (m_receivingState)
		{
		case eRS_ReceivingHead:
			memcpy(&m_receivingHead[m_bufferIndicator], pBuffer, copy);
			break;
		case eRS_ReceivingBody:
			m_receivingBody.reserve(m_bufferIndicator + copy);
			memcpy(&m_receivingBody[m_bufferIndicator], pBuffer, copy);
			break;
		default:
			NET_ASSERT(0);
			break;
		}
		pBuffer += copy;
		nLength -= copy;
		m_bufferIndicator += copy;
		m_amountRemaining -= copy;
		if (m_amountRemaining == 0)
		{
			if (m_receivingState == eRS_ReceivingHead)
			{
				bool bogus = false;
				SRCONMessageHdr* header = (SRCONMessageHdr*)m_receivingHead;
				if (header->magic == RCON_MAGIC)
				{
					// which type of the message is this?
					switch (header->messageType)
					{
					case RCONCLIENTMSGTYPE_MD5DIGEST:
						if (m_sessionState == eSS_ChallengeSent)
							m_amountRemaining = sizeof(SRCONClientMsgMD5Digest) - sizeof(SRCONMessageHdr);
						else
							bogus = true;
						break;

					case RCONCLIENTMSGTYPE_RCONCOMMAND:
						if (m_sessionState == eSS_Authorized)
							m_amountRemaining = sizeof(SRCONClientMsgRConCommand) - sizeof(SRCONMessageHdr);
						else
							bogus = true;
						break;

					default:
						bogus = true;
						break;
					}
				}
				else
					bogus = true;

				if (bogus)
				{
					TO_GAME(&CRemoteControlServerInternal::Reset, this);
					break; // break out of the loop
				}

				m_receivingState = eRS_ReceivingBody;
				m_receivingBody.resize(m_amountRemaining);
				m_bufferIndicator = 0;

				// leave m_receivingHead untouched for later use
			}
			else if (m_receivingState == eRS_ReceivingBody)
			{
				// now a full message should have been received
				SRCONMessageHdr* header = (SRCONMessageHdr*)m_receivingHead;
				switch (header->messageType) // header magic has been checked earlier
				{
				case RCONCLIENTMSGTYPE_MD5DIGEST:
					{
						std::vector<uint8> temp(sizeof(m_challenge.challenge) + m_password.size());
						uint8* p = &temp[0];
						size_t n = temp.size();
						memcpy(p, m_challenge.challenge, sizeof(m_challenge.challenge));
						memcpy(p + sizeof(m_challenge.challenge), m_password.data(), m_password.size());
						CWhirlpoolHash digest(p, n);

						if (0 != memcmp(&m_receivingBody[0], digest(), CWhirlpoolHash::DIGESTBYTES))
						{
							// authentication failed
							TO_GAME(&CRemoteControlServerInternal::GC_AuthenticationError<SRCONServerMsgAuthFailed>, this);
						}
						else
						{
							TIMER.CancelTimer(m_authenticationTimeoutTimer);
							SRCONServerMsgAuthorized authorized;
							m_pSocketSession->Send((uint8*)&authorized, sizeof(authorized));
							m_sessionState = eSS_Authorized;
							string from(RESOLVER.ToString(m_remoteAddr).c_str());
							TO_GAME(&IRemoteControlServerListener::OnClientAuthorized, m_pServer->m_pListener, from);
						}
					}
					break;

				case RCONCLIENTMSGTYPE_RCONCOMMAND:
					{
						uint8* p = &m_receivingBody[0];
						size_t n = m_receivingBody.size();
						uint32 commandId = *(uint32*)p;
						p += sizeof(commandId);
						n -= sizeof(commandId);
						string command((char*)p, n);
						TO_GAME(&IRemoteControlServerListener::OnClientCommand, m_pServer->m_pListener, commandId, command);
					}
					break;

				default:
					NET_ASSERT(0); // since we have already verified the message type
					break;
				}

				// we will be receiving header again
				m_receivingBody.resize(0);

				memset(m_receivingHead, 0, sizeof(m_receivingHead));
				m_receivingState = eRS_ReceivingHead;
				m_bufferIndicator = 0;
				m_amountRemaining = sizeof(m_receivingHead);
			}
		}
	}
}

CRemoteControlClient CRemoteControlClient::s_singleton;

CRemoteControlClient& CRemoteControlClient::GetSingleton()
{
	return CRemoteControlClient::s_singleton;
}

CRemoteControlClient::CRemoteControlClient() : m_internal(this)
{
	m_pListener = NULL;
}

CRemoteControlClient::~CRemoteControlClient()
{

}

void CRemoteControlClient::Connect(const string& serverAddr, uint16 serverPort, const string& password, IRemoteControlClientListener* pListener)
{
	m_pListener = pListener;

	FROM_GAME(&CRemoteControlClientInternal::Connect, &m_internal, serverAddr, serverPort, password);
}

void CRemoteControlClient::Disconnect()
{
	FROM_GAME(&CRemoteControlClientInternal::Disconnect, &m_internal);
}

static CMTRand_int32 rgen;
uint32 CRemoteControlClient::SendCommand(const string& command)
{
	uint32 commandId = rgen.GenerateUint32();
	while (commandId == 0)
		commandId = rgen.GenerateUint32();

	FROM_GAME(&CRemoteControlClientInternal::SendCommand, &m_internal, commandId, command);

	return commandId;
}

CRemoteControlClientInternal::CRemoteControlClientInternal(CRemoteControlClient* pClient)
{
	m_pClient = pClient;

	m_connectionState = eCS_NotConnected;
	m_sessionState = eSS_Unsessioned;

	memset(m_receivingHead, 0, sizeof(m_receivingHead));
	m_receivingState = eRS_ReceivingHead;
	m_bufferIndicator = 0;
	m_amountRemaining = sizeof(m_receivingHead);

	m_resultCommandId = 0;
}

CRemoteControlClientInternal::~CRemoteControlClientInternal()
{

}

void CRemoteControlClientInternal::Reset()
{
	m_connectionState = eCS_NotConnected;
	m_sessionState = eSS_Unsessioned;

	memset(m_receivingHead, 0, sizeof(m_receivingHead));
	m_receivingState = eRS_ReceivingHead;
	m_bufferIndicator = 0;
	m_amountRemaining = sizeof(m_receivingHead);

	m_receivingBody.resize(0);

	m_pendingCommands.clear();
	m_resultCommandId = 0;
}

void CRemoteControlClientInternal::Connect(string serverAddr, uint16 serverPort, string password)
{
	if (m_connectionState != eCS_NotConnected)
	{
		TO_GAME(&IRemoteControlClientListener::OnConnectResult, m_pClient->m_pListener,
		        false, IRemoteControlClientListener::eRD_ConnectAgain);
		return;
	}

	CNameRequestPtr pNR = RESOLVER.RequestNameLookup(serverAddr);
	pNR->Wait(); // TODO: TimedWait()?
	TNetAddressVec nav;
	if (eNRR_Succeeded != pNR->GetResult(nav))
	{
		TO_GAME(&IRemoteControlClientListener::OnConnectResult, m_pClient->m_pListener,
		        false, IRemoteControlClientListener::eRD_CouldNotResolveServerAddr);
		return;
	}

	SIPv4Addr* pIpv4Addr = NULL;
	size_t naddrs = nav.size();
	for (size_t i = 0; i < naddrs; ++i)
	{
		TNetAddress addr = nav[i];
		pIpv4Addr = stl::get_if<SIPv4Addr>(&addr);
		if (pIpv4Addr)
		{
			pIpv4Addr->port = serverPort;
			break;
		}
	}
	if (pIpv4Addr == NULL)
	{
		TO_GAME(&IRemoteControlClientListener::OnConnectResult, m_pClient->m_pListener,
		        false, IRemoteControlClientListener::eRD_UnsupportedAddressType);
		return;
	}

	TNetAddress addr = TNetAddress(*pIpv4Addr);

	m_pSocketSession = CreateStreamSocket(addr);
	NET_ASSERT(m_pSocketSession != NULL);

	m_pSocketSession->SetListener(this);

	bool r = m_pSocketSession->Connect(addr);
	if (!r)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;
		TO_GAME(&IRemoteControlClientListener::OnConnectResult, m_pClient->m_pListener,
		        false, IRemoteControlClientListener::eRD_Failed);
		return;
	}

	m_connectionState = eCS_InProgress;

	memset(m_receivingHead, 0, sizeof(m_receivingHead));
	m_receivingState = eRS_ReceivingHead;
	m_bufferIndicator = 0;
	m_amountRemaining = sizeof(m_receivingHead);

	m_password = password;

	TO_GAME(&IRemoteControlClientListener::OnConnectResult, m_pClient->m_pListener,
	        true, IRemoteControlClientListener::eRD_Okay);
}

void CRemoteControlClientInternal::Disconnect()
{
	if (m_pSocketSession)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;
	}

	m_password = "";

	Reset();
}

void CRemoteControlClientInternal::SendCommand(uint32 commandId, string command)
{
	if (!m_pSocketSession || m_sessionState != eSS_Authorized)
		return;
	SRCONClientMsgRConCommand msg_command;
	msg_command.commandId = commandId;
	cry_strcpy((char*)msg_command.command, sizeof(msg_command.command), command.c_str());
	m_pSocketSession->Send((uint8*)&msg_command, sizeof(msg_command));
	m_pendingCommands[commandId] = command;
}

void CRemoteControlClientInternal::OnConnectCompleted(bool succeeded, void* pUserData)
{
	if (succeeded)
	{
		m_connectionState = eCS_Established;
		m_sessionState = eSS_ChallengeWait;
	}
	else
	{
		TO_GAME(&CRemoteControlClientInternal::Disconnect, this);
		TO_GAME(&IRemoteControlClientListener::OnSessionStatus, m_pClient->m_pListener,
		        false, IRemoteControlClientListener::eSD_ConnectFailed);
	}
}

void CRemoteControlClientInternal::OnConnectionClosed(bool graceful, void* pUserData)
{
	TO_GAME(&CRemoteControlClientInternal::Disconnect, this);
	TO_GAME(&IRemoteControlClientListener::OnSessionStatus, m_pClient->m_pListener,
	        false, IRemoteControlClientListener::eSD_ServerClosed);
}

void CRemoteControlClientInternal::OnIncomingData(const uint8* pData, size_t nSize, void* pUserData)
{
	const uint8* pBuffer = pData;
	size_t nLength = nSize;
	while (nLength > 0 || m_amountRemaining == 0)
	{
		size_t copy = min(m_amountRemaining, nLength);
		if (copy != 0)
		{
			switch (m_receivingState)
			{
			case eRS_ReceivingHead:
				memcpy(&m_receivingHead[m_bufferIndicator], pBuffer, copy);
				break;
			case eRS_ReceivingBody:
				memcpy(&m_receivingBody[m_bufferIndicator], pBuffer, copy);
				break;
			default:
				NET_ASSERT(0);
				break;
			}
			pBuffer += copy;
			nLength -= copy;
			m_bufferIndicator += copy;
			m_amountRemaining -= copy;
		}
		if (m_amountRemaining == 0)
		{
			if (m_receivingState == eRS_ReceivingHead)
			{
				bool bogus = false;
				IRemoteControlClientListener::EStatusDesc desc = IRemoteControlClientListener::eSD_Authorized;
				SRCONMessageHdr* header = (SRCONMessageHdr*)m_receivingHead;
				if (header->magic == RCON_MAGIC)
				{
					switch (header->messageType)
					{
					case RCONSERVERMSGTYPE_INSESSION:
						desc = IRemoteControlClientListener::eSD_ServerSessioned;
						bogus = true;
						break;

					case RCONSERVERMSGTYPE_CHALLENGE:
						if (m_sessionState == eSS_ChallengeWait)
							m_amountRemaining = sizeof(SRCONServerMsgChallenge) - sizeof(SRCONMessageHdr);
						else
						{
							desc = IRemoteControlClientListener::eSD_BogusMessage;
							bogus = true;
						}
						break;

					case RCONSERVERMSGTYPE_AUTHFAILED:
						desc = IRemoteControlClientListener::eSD_AuthFailed;
						bogus = true;
						break;

					case RCONSERVERMSGTYPE_AUTHORIZED:
						// this message has no message body, so no body to receive
						if (m_sessionState == eSS_DigestSent)
							m_amountRemaining = sizeof(SRCONServerMsgAuthorized) - sizeof(SRCONMessageHdr);
						else
						{
							desc = IRemoteControlClientListener::eSD_BogusMessage;
							bogus = true;
						}
						break;

					case RCONSERVERMSGTYPE_RCONRESULT:
						if (m_sessionState == eSS_Authorized)
						{
							m_amountRemaining = sizeof(SRCONServerMsgRConResult) - sizeof(SRCONMessageHdr);
							m_resultCommandId = 0;
						}
						else
						{
							desc = IRemoteControlClientListener::eSD_BogusMessage;
							bogus = true;
						}
						break;

					case RCONSERVERMSGTYPE_AUTHTIMEOUT:
						desc = IRemoteControlClientListener::eSD_AuthTimeout;
						bogus = true;
						break;

					default:
						desc = IRemoteControlClientListener::eSD_BogusMessage;
						bogus = true;
						break;
					}

					if (bogus)
					{
						TO_GAME(&CRemoteControlClientInternal::Disconnect, this);
						TO_GAME(&IRemoteControlClientListener::OnSessionStatus, m_pClient->m_pListener, false, desc);
						break; // break out of the while loop
					}

					m_receivingState = eRS_ReceivingBody;
					m_receivingBody.resize(m_amountRemaining);
					m_bufferIndicator = 0;

					// leave m_receivingHead untouched for later use
				}
			}
			else if (m_receivingState == eRS_ReceivingBody)
			{
				// now a full message should have been received
				SRCONMessageHdr* header = (SRCONMessageHdr*)m_receivingHead;
				switch (header->messageType) // header magic has been checked earlier
				{
				case RCONSERVERMSGTYPE_CHALLENGE:
					{
						// received the server challenge, calculate client side digest
						uint8* challenge = &m_receivingBody[0];
						size_t chalength = m_receivingBody.size();
						std::vector<uint8> temp(chalength + m_password.size());
						uint8* p = &temp[0];
						size_t n = temp.size();
						memcpy(p, challenge, chalength);
						memcpy(p + chalength, m_password.data(), m_password.size());
						CWhirlpoolHash digest(p, n);
						SRCONClientMsgMD5Digest msg_digest;
						memcpy(msg_digest.digest, digest(), CWhirlpoolHash::DIGESTBYTES);
						m_pSocketSession->Send((uint8*)&msg_digest, sizeof(msg_digest));
						m_sessionState = eSS_DigestSent;
					}
					break;

				case RCONSERVERMSGTYPE_AUTHORIZED:
					{
						m_sessionState = eSS_Authorized;

						TO_GAME(&IRemoteControlClientListener::OnSessionStatus, m_pClient->m_pListener,
						        true, IRemoteControlClientListener::eSD_Authorized);
					}
					break;

				case RCONSERVERMSGTYPE_RCONRESULT:
					{
						uint8* p = &m_receivingBody[0];
						size_t n = m_receivingBody.size();
						if (m_resultCommandId == 0)
						{
							m_resultCommandId = *(uint32*)p;
							p += sizeof(uint32);
							m_amountRemaining = *(uint32*)p;
							m_receivingBody.resize(m_amountRemaining);
							m_bufferIndicator = 0;
							// leave m_receivingHead untouched for later use (we are still processing the rcon result message)
							// leave the receiving state untouched (we are still receiving message body)
							continue;
						}

						string result((char*)p, n);
						std::map<uint32, string>::iterator itor = m_pendingCommands.find(m_resultCommandId);
						if (itor != m_pendingCommands.end())
						{
							// the pending command is now acknowledged, remove it, and notify user the result
							TO_GAME(&IRemoteControlClientListener::OnCommandResult, m_pClient->m_pListener,
							        itor->first, itor->second, result);
							m_pendingCommands.erase(itor);
						}
						// otherwise, just ignore the bogus result silently (we might not want to close the connection?)
					}
					break;

				default:
					NET_ASSERT(0);
					break;
				}

				// we will be receiving header again
				m_receivingBody.resize(0);

				memset(m_receivingHead, 0, sizeof(m_receivingHead));
				m_receivingState = eRS_ReceivingHead;
				m_bufferIndicator = 0;
				m_amountRemaining = sizeof(m_receivingHead);
			}
		}
	}
}
