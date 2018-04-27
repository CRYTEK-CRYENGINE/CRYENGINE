// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryMatchMaking.h"
#include "CrySharedLobbyPacket.h"

#include <CryGame/IGameFramework.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "LAN/CryLANLobby.h"
#include "CryDedicatedServer.h"

#define PRODUCT_VERSION_SIZE 32

#if USE_CRY_MATCHMAKING

const uint32 CryMatchMakingNubConnectionPingTime = 250;
const uint32 CryMatchMakingNubConnectionTimeOut = 10000;

	#define PING_LARGE_SIMILARITY_THRESHOLD (1000)
	#define PING_SMALL_SIMILARITY_THRESHOLD (100)
	#define TIME_SIMILARITY_THRESHOLD       (10 * 1000LL)
	#define HOST_HINT_INTERNAL_SEND_DELAY   (10 * 1000LL)
	#define HOST_HINT_EXTERNAL_SEND_DELAY   (10 * 1000LL)

//#define LOG_SERVER_PING_INFO

	#define MATCHMAKING_PARAM_DATA                    0
	#define MATCHMAKING_PARAM_NUM_DATA                0

	#define MATCHMAKING_PARAM_FLAGS                   0
	#define MATCHMAKING_PARAM_FLAGS_MASK              1

	#define SETUP_DEDICATED_SERVER_SEND_INTERVAL      1000
	#define SETUP_DEDICATED_SERVER_MAX_SENDS          15
	#define TDM_SETUP_DEDICATED_SERVER_PACKET         0
	#define TDN_SETUP_DEDICATED_SERVER_PACKET_SIZE    0
	#define TDN_SETUP_DEDICATED_SERVER_SEND_COUNTER   1
	#define TDN_SETUP_DEDICATED_SERVER_COOKIE         2
	#define TDM_SETUP_DEDICATED_SERVER_NAME_REQUESTER 1

	#define RELEASE_DEDICATED_SERVER_SEND_INTERVAL    1000
	#define RELEASE_DEDICATED_SERVER_MAX_SENDS        15
	#define TDM_RELEASE_DEDICATED_SERVER_SENDIDS      0
	#define TDN_RELEASE_DEDICATED_SERVER_SEND_COUNTER TDN_SETUP_DEDICATED_SERVER_SEND_COUNTER
	#define TDN_RELEASE_DEDICATED_SERVER_COOKIE       TDN_SETUP_DEDICATED_SERVER_COOKIE

	#if USE_CRY_DEDICATED_SERVER
// In an ideal world when using CCryDedicatedServer, CCryDedicatedServer would be returned as the match making service.
// Unfortunately during dev dedicated servers may need a full matchmaking service so this gets returned instead.
// This macro can be used to call the CCryDedicatedServer version of a function from the full matchmaking version when both need to be called.
		#define CALL_DEDICATED_SERVER_VERSION(func)                                                             \
		  if (gEnv->IsDedicated())                                                                              \
		  {                                                                                                     \
		    CCryLANLobbyService* pLobbyService = (CCryLANLobbyService*)gEnv->pLobby->GetLobbyService(eCLS_LAN); \
		                                                                                                        \
		    if (pLobbyService)                                                                                  \
		    {                                                                                                   \
		      CCryDedicatedServer* pDedicatedServer = pLobbyService->GetDedicatedServer();                      \
		                                                                                                        \
		      if (pDedicatedServer && (pDedicatedServer != this))                                               \
		      {                                                                                                 \
		        pDedicatedServer->func;                                                                         \
		      }                                                                                                 \
		    }                                                                                                   \
		  }
	#else
		#define CALL_DEDICATED_SERVER_VERSION(func)
	#endif

static bool VerifyPacketBeforeSend(const CCryLobbyPacket* pPacket, const char* const funcName)
{
	if (pPacket->IsValidToSend() == true)
	{
		return true;
	}
	NetLog("[Lobby] %s ERROR invalid packet", funcName);
	return false;
}

void CCryMatchMaking::SSession::Reset()
{
	pingToServerTimer.SetValue(0);
	hostConnectionID = CryMatchMakingInvalidConnectionID;
	serverConnectionID = CryMatchMakingInvalidConnectionID;
	createFlags = 0;
	localFlags = 0;

	localConnection->uid = CryMatchMakingInvalidConnectionUID;
	localConnection->numUsers = 0;
	localConnection->pingToServer = CRYLOBBY_INVALID_PING;

	for (uint32 index = 0; index < MAX_LOBBY_CONNECTIONS; index++)
	{
		remoteConnection[index]->used = false;
	}

	#if NETWORK_HOST_MIGRATION
	hostHintInfo.Reset();
	newHostPriorityCount = 0;
	newHostUID = CryMatchMakingInvalidConnectionUID;
	#endif
}

CCryMatchMaking::CCryMatchMaking(CCryLobby* lobby, CCryLobbyService* service, ECryLobbyService serviceType)
	: m_connectionUIDCounter(0)
	, m_lobby(lobby)
	, m_pService(service)
	, m_serviceType(serviceType)
{
	assert(lobby);
	assert(service);

	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		m_sessions[i] = NULL;
	}

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		m_task[i] = NULL;
	}

	#if NETWORK_HOST_MIGRATION
	gEnv->pConsole->GetCVar("net_hostHintingNATTypeOverride")->SetOnChangeCallback(HostHintingOverrideChanged);
	gEnv->pConsole->GetCVar("net_hostHintingActiveConnectionsOverride")->SetOnChangeCallback(HostHintingOverrideChanged);
	gEnv->pConsole->GetCVar("net_hostHintingPingOverride")->SetOnChangeCallback(HostHintingOverrideChanged);
	gEnv->pConsole->GetCVar("net_hostHintingUpstreamBPSOverride")->SetOnChangeCallback(HostHintingOverrideChanged);
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
	m_hostMigrationStateCheckSession = CryLobbyInvalidSessionHandle;
		#endif
	#endif

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
	m_arbitratorAddr = SNullAddr();
	#endif
}

	#define GAME_SESSION_HANDLE_BASE         0x80000000   // Make sure the top bit of a CrySessionHandle is always set so if a CrySessionHandle gets promoted to a CryLobbySessionHandle it will generate a runtime error.
	#define GAME_SESSION_HANDLE_HANDLE_MASK  0x7fff0000
	#define GAME_SESSION_HANDLE_HANDLE_SHIFT 16
	#define GAME_SESSION_HANDLE_UID_MASK     0x0000ffff
	#define GAME_SESSION_HANDLE_UID_SHIFT    0
CrySessionHandle CCryMatchMaking::CreateGameSessionHandle(CryLobbySessionHandle h, SCryMatchMakingConnectionUID uid)
{
	if (h != CryLobbyInvalidSessionHandle)
	{
		return GAME_SESSION_HANDLE_BASE | ((h.GetID() << GAME_SESSION_HANDLE_HANDLE_SHIFT) & GAME_SESSION_HANDLE_HANDLE_MASK) | ((uid.m_uid << GAME_SESSION_HANDLE_UID_SHIFT) & GAME_SESSION_HANDLE_UID_MASK);
	}
	else
	{
		return CrySessionInvalidHandle;
	}
}

SCryMatchMakingConnectionUID CCryMatchMaking::CreateConnectionUID(CryLobbySessionHandle h)
{
	SCryMatchMakingConnectionUID temp;

	do
	{
		m_connectionUIDCounter = (m_connectionUIDCounter + 1) & (GAME_SESSION_HANDLE_UID_MASK >> GAME_SESSION_HANDLE_UID_SHIFT);
	}
	while (m_connectionUIDCounter == 0);

	temp.m_sid = GetSIDFromSessionHandle(h);
	temp.m_uid = m_connectionUIDCounter;

	return temp;
}

CryLobbySessionHandle CCryMatchMaking::GetSessionHandleFromGameSessionHandle(CrySessionHandle gh)
{
	if (gh != CrySessionInvalidHandle)
	{
		return CryLobbySessionHandle((gh & GAME_SESSION_HANDLE_HANDLE_MASK) >> GAME_SESSION_HANDLE_HANDLE_SHIFT);
	}
	else
	{
		return CryLobbyInvalidSessionHandle;
	}
}

SCryMatchMakingConnectionUID CCryMatchMaking::GetConnectionUIDFromGameSessionHandle(CrySessionHandle gh)
{
	SCryMatchMakingConnectionUID temp;
	if ((gh & GAME_SESSION_HANDLE_HANDLE_MASK) != (CrySessionInvalidHandle & GAME_SESSION_HANDLE_HANDLE_MASK))
	{
		temp.m_sid = GetSIDFromSessionHandle(GetSessionHandleFromGameSessionHandle(gh));
		temp.m_uid = GetChannelIDFromGameSessionHandle(gh);
	}

	return temp;
}

SCryMatchMakingConnectionUID CCryMatchMaking::GetConnectionUIDFromGameSessionHandleAndChannelID(CrySessionHandle gh, uint16 channelID)
{
	SCryMatchMakingConnectionUID temp;
	if ((gh & GAME_SESSION_HANDLE_HANDLE_MASK) != (CrySessionInvalidHandle & GAME_SESSION_HANDLE_HANDLE_MASK))
	{
		temp.m_sid = GetSIDFromSessionHandle(GetSessionHandleFromGameSessionHandle(gh));
		temp.m_uid = channelID;
	}

	return temp;
}

uint16 CCryMatchMaking::GetChannelIDFromGameSessionHandle(CrySessionHandle gh)
{
	return (gh & GAME_SESSION_HANDLE_UID_MASK) >> GAME_SESSION_HANDLE_UID_SHIFT;
	;
}

SCryMatchMakingConnectionUID CCryMatchMaking::GetConnectionUIDFromNubAndGameSessionHandle(CrySessionHandle gh)
{
	if (gh != CrySessionInvalidHandle)
	{
		SSession* pSession = NULL;
		SSession::SRConnection* pConnection = NULL;
		uint16 channelID = GetChannelIDFromGameSessionHandle(gh);

		for (uint32 sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
		{
			pSession = m_sessions[sessionIndex];

			if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB))  // is nub session
			{
				for (uint32 remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
				{
					pConnection = pSession->remoteConnection[remoteConnectionIndex];

					if (pConnection->used && pConnection->uid.m_uid == channelID)
					{
						return pConnection->uid;
					}
				}
			}
		}
	}

	return CryMatchMakingInvalidConnectionUID;
}

void CCryMatchMaking::Tick(CTimeValue tv)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];

		CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			uint32 numActiveConnections = 0;
			for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
			{
				SSession::SRConnection* pConnection = pSession->remoteConnection[j];

				CRY_ASSERT_MESSAGE(pConnection, "CCryMatchMaking: Remote connection base pointers not setup");

				if (pConnection->used)
				{
					if (pConnection->connectionID != CryLobbyInvalidConnectionID)
					{
						switch (m_lobby->ConnectionGetState(pConnection->connectionID))
						{
						case eCLCS_Connected:
							++numActiveConnections;
							break;

						case eCLCS_NotConnected:
							// This lobby connection has been lost so disconnect.
							SessionDisconnectRemoteConnectionViaNub(i, j, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_Timeout, "Connection lost");
							break;

						default:
							break;
						}
					}
				}

				if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_CAN_SEND_SERVER_PING) && (tv.GetDifferenceInSeconds(pSession->pingToServerTimer) >= CLobbyCVars::Get().serverPingNotificationInterval))
				{
					pSession->pingToServerTimer = tv;
					SendServerPing(i);
				}
			}
	#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
			static CryLobbySessionHandle oldNubControlledSession = CryLobbyInvalidSessionHandle;
			static uint64 oldNubControlledSID[2] = { 0, 0 };
			CryLobbySessionHandle nubControlledSession = GetCurrentNetNubSessionHandle();
			if (nubControlledSession != CryLobbyInvalidSessionHandle)
			{
				if (oldNubControlledSession == CryLobbyInvalidSessionHandle)
				{
					oldNubControlledSession = nubControlledSession;
				}

				if (oldNubControlledSession != nubControlledSession)
				{
					uint64 nubControlledSID = GetSIDFromSessionHandle(nubControlledSession);

					if (nubControlledSID != oldNubControlledSID[1])
					{
						// We've moved to a new game session
						CryLogAlways("[Host Migration]: HOST MIGRATION SHARD BREAK DETECTED (nub controlled session changed " PRFORMAT_SH " (%llX) -> " PRFORMAT_SH " (%llX) - game sharded and lobby merged?)", PRARG_SH(oldNubControlledSession), oldNubControlledSID[0], PRARG_SH(nubControlledSession), nubControlledSID);
						ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyBreaksHMShard");
						if (pVar != NULL)
						{
							pVar->Set(pVar->GetIVal() + 1);
						}
					}
					else
					{
						// We've attempted a merge to a new game session, but cancelled it and are now back in the original session so we can rescind a potential shard break
						CryLogAlways("[Host Migration]: HOST MIGRATION SHARD BREAK RESCINDED (nub controlled session changed " PRFORMAT_SH " (%llX) -> " PRFORMAT_SH " (%llX) - game returned to previous session)", PRARG_SH(oldNubControlledSession), oldNubControlledSID[0], PRARG_SH(nubControlledSession), nubControlledSID);
						ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyBreaksHMShard");
						if (pVar != NULL)
						{
							pVar->Set(pVar->GetIVal() - 1);
						}
					}

					oldNubControlledSession = nubControlledSession;
					oldNubControlledSID[1] = oldNubControlledSID[0];
					oldNubControlledSID[0] = nubControlledSID;
				}
			}
	#endif

	#if NETWORK_HOST_MIGRATION
			if (pSession->hostMigrationInfo.m_state != eHMS_Idle)
			{
				TO_GAME_FROM_LOBBY(&CHostMigration::Update, &m_hostMigration, &pSession->hostMigrationInfo);
			}
			else
			{
				if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS)
				{
					// NAT
					pSession->outboundHostHintInfo.natType = CLobbyCVars::Get().netHostHintingNATTypeOverride;
					if (pSession->outboundHostHintInfo.natType == 0)
					{
						pSession->outboundHostHintInfo.natType = m_lobby->GetNATType();
					}
					else
					{
						// Map overridden NAT type (1 = open, 2 = moderate, 3 = strict) to 'real' NAT type (0 = open, 1 = moderate, 2 = strict)
						pSession->outboundHostHintInfo.natType -= 1;
					}

					// Number of active connections
					pSession->outboundHostHintInfo.numActiveConnections = CLobbyCVars::Get().netHostHintingActiveConnectionsOverride;
					if (pSession->outboundHostHintInfo.numActiveConnections == 0)
					{
						pSession->outboundHostHintInfo.numActiveConnections = numActiveConnections;
					}

					// Ping
					CryPingAccumulator ping = CLobbyCVars::Get().netHostHintingPingOverride;
					if (ping == 0)
					{
						uint32 numValidPings = 0;
						uint32 numInvalidPings = 0;

						// Calculate average ping for this machine
						for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; ++connectionIndex)
						{
							SSession::SRConnection* pConnection = pSession->remoteConnection[connectionIndex];
							if (pConnection->used)
							{
								CryPing cxPing = m_lobby->GetConnectionPing(pConnection->connectionID);
								if ((m_lobby->ConnectionGetState(pConnection->connectionID) == eCLCS_Connected))
								{
									if (cxPing < CRYLOBBY_INVALID_PING)
									{
										ping += cxPing;
										++numValidPings;
									}
									else
									{
										++numInvalidPings;
									}
								}
							}
						}

						if (numValidPings > 0)
						{
							ping /= numValidPings;
						}
						if (numInvalidPings > 1)
						{
							// If we have more than 1 invalid ping then boost the ping total to mark this client as bad
							ping += 1000;
						}
					}
					pSession->outboundHostHintInfo.ping = (ping > CRYLOBBY_INVALID_PING) ? CRYLOBBY_INVALID_PING : (CryPing)ping;

					// Upstream bits per second
					pSession->outboundHostHintInfo.upstreamBPS = CLobbyCVars::Get().netHostHintingUpstreamBPSOverride;
					if (pSession->outboundHostHintInfo.upstreamBPS == 0)
					{
						pSession->outboundHostHintInfo.upstreamBPS = GetUpstreamBPS();
					}

					if (!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
					{
						if (pSession->hostHintInfo.SufficientlyDifferent(pSession->outboundHostHintInfo, tv))
						{
		#if NET_HOST_HINT_DEBUG
							NetLog("[Host Hints]: client thinks hint has changed");
		#endif    // #if NET_HOST_HINT_DEBUG
							pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST_HINT_INFO_DIRTY;
						}
		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
						else
						{
							if ((pSession->newHostUID != pSession->localConnection->uid) && (pSession->newHostPriorityCount == 0))
							{
								CryLogAlways("[Host Migration]: HOST MIGRATION HINT BREAK DETECTED (we're not the host or anticipated host, we have no hints but we don't think we need to send any)");
								ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyBreaksHMHints");
								if (pVar != NULL)
								{
									pVar->Set(pVar->GetIVal() + 1);
								}
							}
						}
		#endif
					}

					if ((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_HOST_HINT_INFO_DIRTY | CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST)) == (CRYSESSION_LOCAL_FLAG_HOST_HINT_INFO_DIRTY | CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST))
					{
						const int64 timeSinceLastSendInternal = (tv.GetMilliSecondsAsInt64() - pSession->hostHintInfo.lastSendInternal.GetMilliSecondsAsInt64());
						if (timeSinceLastSendInternal > HOST_HINT_INTERNAL_SEND_DELAY)
						{
							pSession->outboundHostHintInfo.lastSendInternal = tv;
							pSession->outboundHostHintInfo.lastSendExternal = pSession->hostHintInfo.lastSendExternal;
							//This function copies outboundHostHintInfo into hostHintInfo
							SendHostHintInformation(i);
						}
					}
				}

				if ((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_HOST | CRYSESSION_LOCAL_FLAG_HOST_HINT_EXTERNAL_INFO_DIRTY)) == (CRYSESSION_LOCAL_FLAG_HOST | CRYSESSION_LOCAL_FLAG_HOST_HINT_EXTERNAL_INFO_DIRTY))
				{
					const int64 timeSinceLastSendExternal = (tv.GetMilliSecondsAsInt64() - pSession->hostHintInfo.lastSendExternal.GetMilliSecondsAsInt64());

					if (timeSinceLastSendExternal > HOST_HINT_EXTERNAL_SEND_DELAY)
					{
						pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_HOST_HINT_EXTERNAL_INFO_DIRTY;
						pSession->hostHintInfo.lastSendExternal = tv;
						SendHostHintExternal(i);
					}
				}
			}
	#endif
		}
	}

	bool anyExpired = ExpireBans(m_resolvedBannedUser, tv);

	if (ExpireBans(m_unresolvedBannedUser, tv) || anyExpired)
	{
		OnBannedUserListChanged();
	}

	#if !defined(_RELEASE)
	CLobbyCVars& cvars = CLobbyCVars::Get();
	bool bShowDebugText = (cvars.showMatchMakingTasks != 0);
		#if NETWORK_HOST_MIGRATION
	bShowDebugText |= (CLobbyCVars::Get().showHostIdentification != 0) || (CLobbyCVars::Get().showNewHostHinting != 0);
		#endif

	if (bShowDebugText)
	{
		TO_GAME_FROM_LOBBY(&CCryMatchMaking::DrawDebugText, this);
	}
	#endif
}

	#if !defined(_RELEASE)
void CCryMatchMaking::DrawDebugText()
{
	static float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float red[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	static float green[] = { 0.0f, 1.0f, 0.0f, 1.0f };

	if (CLobbyCVars::Get().showMatchMakingTasks != 0)
	{
		float ypos = 350.0f;
		float xpos = 300.0f;
		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; ++i)
		{
			STask* pTask = m_task[i];

			if (pTask->used)
			{
				IRenderAuxText::Draw2dLabel(xpos, ypos, 1.5f, white, false, "[Matchmaking]: task [%i], startedTask [%i][%i]", i, pTask->startedTask, pTask->subTask);
				ypos += 15.0f;
			}
		}
	}

		#if NETWORK_HOST_MIGRATION
	for (uint32 debugIndex = 0; debugIndex < MAX_MATCHMAKING_SESSIONS; ++debugIndex)
	{
		float ypos = 50.0f;
		float xpos = 50.0f + (250.0f * debugIndex);

		SSession* pSession = m_sessions[debugIndex];

		if (gEnv->pRenderer && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
		{
			if (CLobbyCVars::Get().showHostIdentification != 0)
			{
				float timetaken = (g_time - pSession->hostMigrationInfo.m_startTime).GetSeconds();
				switch (pSession->hostMigrationInfo.m_state)
				{
				case eHMS_Initiate:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_Initiate", timetaken);
					break;
				case eHMS_DisconnectClient:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_DisconnectClient", timetaken);
					break;
				case eHMS_DemoteToClient:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_DemoteToClient", timetaken);
					break;
				case eHMS_PromoteToServer:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_PromoteToServer", timetaken);
					break;
				case eHMS_WaitForNewServer:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_WaitForNewServer", timetaken);
					break;
				case eHMS_ReconnectClient:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_ReconnectClient", timetaken);
					break;
				case eHMS_Finalise:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_Finalise", timetaken);
					break;
				case eHMS_Terminate:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_Terminate", timetaken);
					break;
				case eHMS_Resetting:
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "[%.02f] eHMS_Resetting", timetaken);
					break;
				}
				ypos += 15.0f;

				gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "%llX", pSession->localConnection->uid.m_sid);
				ypos += 15.0f;

				gEnv->pRenderer->Draw2dLabel(xpos, ypos, 2.0f, white, false, "%i:", debugIndex);

				if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
				{
					gEnv->pRenderer->Draw2dLabel(xpos + 20.0f, ypos, 2.0f, red, false, "HOST");
				}
				else
				{
					if (pSession->newHostUID == pSession->localConnection->uid)
					{
						gEnv->pRenderer->Draw2dLabel(xpos + 20.0f, ypos, 2.0f, green, false, "ANTICIPATED NEW HOST");
					}
					else
					{
						gEnv->pRenderer->Draw2dLabel(xpos + 20.0f, ypos, 2.0f, white, false, "MEMBER");
					}
				}
			}

			ypos += 30.0f;

			if (CLobbyCVars::Get().showNewHostHinting != 0)
			{
				// Determine if host hinting has a better host
				SHostHintInformation newHost;

				if (pSession->newHostUID == pSession->localConnection->uid)
				{
					newHost = pSession->hostHintInfo;
				}
				else
				{
					CryLobbySessionHandle newHostHandle;
					CryMatchMakingConnectionID newHostID;

					if (FindConnectionFromUID(pSession->newHostUID, &newHostHandle, &newHostID))
					{
						SSession::SRConnection* pConnection = pSession->remoteConnection[newHostID];

						newHost = pConnection->hostHintInfo;
					}
				}

				if (pSession->newHostUID != CryMatchMakingInvalidConnectionUID)
				{
					gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "New host: uid %i (nat %i, %i cx, up %dkbps, %ims)", pSession->newHostUID.m_uid, newHost.natType, newHost.numActiveConnections, newHost.upstreamBPS / 1000, newHost.ping);
					ypos += 40.0f;

					if (pSession->newHostPriorityCount > 0)
					{
						uint16 ping = CRYLOBBY_INVALID_PING;

						for (uint32 hintIndex = 0; hintIndex < pSession->newHostPriorityCount; ++hintIndex)
						{
							for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; ++connectionIndex)
							{
								SSession::SRConnection* pConnection = pSession->remoteConnection[connectionIndex];
								if (pConnection->used && (pConnection->connectionID != CryLobbyInvalidConnectionID) && (pConnection->uid.m_uid == pSession->newHostPriority[hintIndex]->uid.m_uid))
								{
									ping = pConnection->pingToServer;
									break;
								}
							}

							gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, white, false, "Hint %i: uid %i, nat %i, %i cx, up %dkbps, %ims, %ims", hintIndex, pSession->newHostPriority[hintIndex]->uid.m_uid, pSession->newHostPriority[hintIndex]->hostHintInfo.natType, pSession->newHostPriority[hintIndex]->hostHintInfo.numActiveConnections, pSession->newHostPriority[hintIndex]->hostHintInfo.upstreamBPS / 1000, pSession->newHostPriority[hintIndex]->hostHintInfo.ping, ping);
							ypos += 20.0f;
						}
					}
				}
			}
		}
	}
		#endif // NETWORK_HOST_MIGRATION
}
	#endif // !defined(_RELEASE)

void CCryMatchMaking::StartTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = m_task[mmTaskID];

	if (pTask->used)
	{
		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_SessionTerminateHostHintingForGroup:
			StartSessionPendingGroupDisconnect(mmTaskID);
			break;

		case eT_SessionModifyLocalFlags:
			StartSessionModifyLocalFlags(mmTaskID);
			break;

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
		case eT_SessionSetupDedicatedServer:
			StartSessionSetupDedicatedServer(mmTaskID);
			break;

		case eT_SessionReleaseDedicatedServer:
			StartSessionReleaseDedicatedServer(mmTaskID);
			break;
	#endif
		}
	}
}

void CCryMatchMaking::TickBaseTask(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	if (pTask->used && pTask->running)
	{
		switch (pTask->subTask)
		{
		case eT_SessionTerminateHostHintingForGroup:
			TickSessionPendingGroupDisconnect(mmTaskID);
			break;

		case eT_SessionModifyLocalFlags:
			TickSessionModifyLocalFlags(mmTaskID);
			break;

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
		case eT_SessionSetupDedicatedServerNameResolve:
			TickSessionSetupDedicatedServerNameResolve(mmTaskID);
			break;

		case eT_SessionSetupDedicatedServer:
			TickSessionSetupDedicatedServer(mmTaskID);
			break;

		case eT_SessionReleaseDedicatedServer:
			TickSessionReleaseDedicatedServer(mmTaskID);
			break;
	#endif
		}
	}
}

void CCryMatchMaking::EndTask(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;
	STask* pTask = m_task[mmTaskID];
	SSession* pSession = m_sessions[pTask->session];

	if (pTask->used)
	{
		if (pTask->cb)
		{
			switch (pTask->startedTask)
			{
			case eT_SessionTerminateHostHintingForGroup:
				((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
				break;

			case eT_SessionModifyLocalFlags:
				{
					CrySessionHandle gh = CreateGameSessionHandle(pTask->session, pSession->localConnection->uid);
					((CryMatchmakingFlagsCallback)pTask->cb)(pTask->lTaskID, pTask->error, gh, pSession->localFlags & CRYSESSION_LOCAL_FLAG_GAME_MASK, pTask->cbArg);
				}
				break;

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
			case eT_SessionSetupDedicatedServer:
				EndSessionSetupDedicatedServer(mmTaskID);
				break;

			case eT_SessionReleaseDedicatedServer:
				EndSessionReleaseDedicatedServer(mmTaskID);
				break;
	#endif
			}
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Lobby] EndTask %u (%u) error %d", pTask->startedTask, pTask->subTask, pTask->error);
		}

		FreeTask(mmTaskID);
	}
}

void CCryMatchMaking::StopTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	if (pTask->used)
	{
		pTask->running = false;
		TO_GAME_FROM_LOBBY(&CCryMatchMaking::EndTask, this, mmTaskID);
	}
}

ECryLobbyService CCryMatchMaking::GetLobbyServiceTypeForNubSession()
{
	CryLobbySessionHandle h = GetCurrentNetNubSessionHandle();

	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = m_sessions[h];

		if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
		{
			SSession::SRConnection* pConnection = pSession->remoteConnection[pSession->serverConnectionID];

			if (pConnection->connectionID != CryLobbyInvalidConnectionID)
			{
				return m_lobby->GetCorrectSocketServiceTypeForAddr(*m_lobby->GetNetAddress(pConnection->connectionID));
			}
		}
	}

	return m_serviceType;
}

CryLobbySessionHandle CCryMatchMaking::GetCurrentNetNubSessionHandle() const
{
	CryLobbySessionHandle handle = CryLobbyInvalidSessionHandle;

	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; ++i)
	{
		SSession* pSession = m_sessions[i];

		if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB))
		{
			CRY_ASSERT_MESSAGE((handle == CryLobbyInvalidSessionHandle), "Multiple matchmaking sessions created with CRYSESSION_CREATE_FLAG_NUB");
			handle = i;
		}
	}

	return handle;
}

CryLobbySessionHandle CCryMatchMaking::GetCurrentHostedNetNubSessionHandle() const
{
	CryLobbySessionHandle handle = GetCurrentNetNubSessionHandle();

	if ((handle != CryLobbyInvalidSessionHandle) && !(m_sessions[handle]->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
	{
		handle = CryLobbyInvalidSessionHandle;
	}

	return handle;
}

SCryMatchMakingConnectionUID CCryMatchMaking::GetHostConnectionUID(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	SCryMatchMakingConnectionUID uid = CryMatchMakingInvalidConnectionUID;
	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = m_sessions[h];

		CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CRY_ASSERT_MESSAGE(pSession->localConnection, "CCryMatchMaking: Local connection base pointer not setup");

			return pSession->localConnection->uid;
		}
		else
		{
			CRY_ASSERT_MESSAGE(pSession->remoteConnection[pSession->hostConnectionID], "CCryMatchMaking: Remote connection base pointers not setup");

			return pSession->remoteConnection[pSession->hostConnectionID]->uid;
		}
	}

	return CryMatchMakingInvalidConnectionUID;
}

ECryLobbyError CCryMatchMaking::StartTask(uint32 etask, bool startRunning, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg)
{
	CryLobbyTaskID lobbyTaskID = m_lobby->CreateTask();

	if (lobbyTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
		{
			STask* task = m_task[i];

			assert(task);

			if (!task->used)
			{
				task->timer.SetValue(0);
				task->lTaskID = lobbyTaskID;
				task->error = eCLE_Success;
				task->startedTask = etask;
				task->subTask = etask;
				task->session = h;
				task->cb = cb;
				task->cbArg = cbArg;
				task->sendID = CryLobbyInvalidSendID;
				task->sendStatus = eCLE_Success;
				task->used = true;
				task->running = startRunning;
				task->timerStarted = false;
				task->canceled = false;

				for (uint32 j = 0; j < MAX_MATCHMAKING_PARAMS; j++)
				{
					task->params[j] = TMemInvalidHdl;
					task->numParams[j] = 0;
				}

				if (mmTaskID)
				{
					*mmTaskID = i;
				}

				if (lTaskID)
				{
					*lTaskID = lobbyTaskID;
				}

				return eCLE_Success;
			}
		}

		m_lobby->ReleaseTask(lobbyTaskID);
	}

	#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
	CryLogAlways("[Host Migration]: HOST MIGRATION TASK BREAK DETECTED (too many tasks active when CCryMatchMaking::StartTask() called)");
	ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyBreaksHMTasks");
	if (pVar != NULL)
	{
		pVar->Set(pVar->GetIVal() + 1);
	}
	#endif

	NetLog("[Matchmaking]: Trying to start task [%u]", etask);
	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; ++i)
	{
		STask* pTask = m_task[i];

		if (pTask->used)
		{
			NetLog("[Matchmaking]: task [%u], startedTask [%u][%u]", i, pTask->startedTask, pTask->subTask);
		}
	}
	NetLog("[Matchmaking]: Too many tasks...");

	return eCLE_TooManyTasks;
}

void CCryMatchMaking::StartSubTask(uint32 etask, CryMatchMakingTaskID mmTaskID)
{
	STask* task = m_task[mmTaskID];
	task->subTask = etask;
}

void CCryMatchMaking::FreeTask(CryMatchMakingTaskID mmTaskID)
{
	STask* task = m_task[mmTaskID];

	assert(task);

	for (uint32 i = 0; i < MAX_MATCHMAKING_PARAMS; i++)
	{
		if (task->params[i] != TMemInvalidHdl)
		{
			m_lobby->MemFree(task->params[i]);
			task->params[i] = TMemInvalidHdl;
			task->numParams[i] = 0;
		}
	}

	m_lobby->ReleaseTask(task->lTaskID);

	task->used = false;
}

void CCryMatchMaking::CancelTask(CryLobbyTaskID lTaskID)
{
	LOBBY_AUTO_LOCK;

	NetLog("[Lobby]Try cancel task %u", lTaskID);

	CryMatchMakingTaskID mmTaskID = FindTaskFromLobbyTaskID(lTaskID);
	if (mmTaskID != CryLobbyInvalidTaskID)
	{
		STask* pTask = m_task[mmTaskID];
		NetLog("[Lobby] Task %u canceled", lTaskID);
		pTask->cb = NULL;
		pTask->canceled = true;
	}
}

CrySessionID CCryMatchMaking::GetSessionIDFromConsole(void)
{
	return CrySessionInvalidID;
}

void CCryMatchMaking::SessionJoinFromConsole()
{
	CrySessionID sessionId = GetSessionIDFromConsole();

	if (sessionId != CrySessionInvalidID)
	{
		uint32 users = 0;

		SessionJoin(&users, 1, 0, sessionId, NULL, MatchmakingSessionJoinFromConsoleCallback, NULL);
	}
}

void CCryMatchMaking::MatchmakingSessionJoinFromConsoleCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, uint32 ip, uint16 port, void* pArg)
{
	if (error == eCLE_Success)
	{
		char command[128];
		IConsole* pConsole = gEnv->pConsole;

		cry_sprintf(command, "connect <session>%08X,%u.%u.%u.%u:%u", h, ((uint8*)&ip)[0], ((uint8*)&ip)[1], ((uint8*)&ip)[2], ((uint8*)&ip)[3], port);

		if (pConsole)
		{
			pConsole->ExecuteString(command, false, true);
		}
	}
}

ECryLobbyError CCryMatchMaking::CreateTaskParamMem(CryMatchMakingTaskID mmTaskID, uint32 param, const void* pParamData, size_t paramDataSize)
{
	STask* pTask = m_task[mmTaskID];

	if (paramDataSize > 0)
	{
		pTask->params[param] = m_lobby->MemAlloc(paramDataSize);
		void* p = m_lobby->MemGetPtr(pTask->params[param]);

		if (p)
		{
			if (pParamData)
			{
				memcpy(p, pParamData, paramDataSize);
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	return eCLE_Success;
}

ECryLobbyError CCryMatchMaking::CreateTaskParam(CryMatchMakingTaskID mmTaskID, uint32 param, const void* paramData, uint32 numParams, size_t paramSize)
{
	STask* task = m_task[mmTaskID];

	assert(task);

	task->numParams[param] = numParams;

	return CreateTaskParamMem(mmTaskID, param, paramData, paramSize);
}

ECryLobbyError CCryMatchMaking::CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32 createFlags, int numUsers)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];

		assert(pSession);

		if (!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
		{
			pSession->Reset();
			pSession->localFlags = CRYSESSION_LOCAL_FLAG_USED;
			pSession->localConnection->numUsers = numUsers;

			if (host)
			{
				pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST;
			}
			pSession->createFlags = createFlags;

			bool hostMigrationSupported = m_lobby->GetLobbyServiceFlag(m_serviceType, eCLSF_SupportHostMigration);
			if ((createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) && hostMigrationSupported)
			{
				pSession->localFlags |= CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
			}

			if (numUsers)
			{
				pSession->localConnection->used = true;
			}
			else
			{
				pSession->localConnection->used = false;
			}

			*h = i;

			return eCLE_Success;
		}
	}

	return eCLE_TooManySessions;
}

void CCryMatchMaking::FreeSessionHandle(CryLobbySessionHandle h)
{
	SSession* session = m_sessions[h];

	assert(session);

	session->localFlags &= ~CRYSESSION_LOCAL_FLAG_USED;

	for (uint32 index = 0; index < MAX_MATCHMAKING_SESSIONS; ++index)
	{
		if (m_sessions[index]->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			return;
		}
	}

}

CryMatchMakingConnectionID CCryMatchMaking::AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, uint32 numUsers)
{
	SSession* pSession = m_sessions[h];

	CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* pConnection = pSession->remoteConnection[i];

		CRY_ASSERT_MESSAGE(pConnection, "CCryMatchMaking: Remote connection base pointers not setup");

		if (!pConnection->used)
		{
			m_lobby->ConnectionAddRef(connectionID);

			pConnection->used = true;
			pConnection->pingToServer = CRYLOBBY_INVALID_PING;
			pConnection->connectionID = connectionID;
			pConnection->uid = uid;
			pConnection->numUsers = numUsers;
			pConnection->gameInformedConnectionEstablished = false;
			pConnection->flags = 0;
	#if NETWORK_HOST_MIGRATION
			pConnection->hostHintInfo.Reset();
	#endif
			pConnection->StartTimer();

			if ((!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)) && (uid.m_uid > m_connectionUIDCounter))
			{
				// Keep all remote clients in sync with the host (in case they're chosen to be host during host migration)
				m_connectionUIDCounter = uid.m_uid;
			}

			TNetAddress ip;
			if (m_lobby->AddressFromConnection(ip, pConnection->connectionID))
			{
				NetLog("New remote connection " PRFORMAT_ADDR ", " PRFORMAT_SHMMCINFO, PRARG_ADDR(ip), PRARG_SHMMCINFO(h, CryMatchMakingConnectionID(i), connectionID, pConnection->uid));
			}

			return i;
		}
	}

	return CryMatchMakingInvalidConnectionID;
}

void CCryMatchMaking::FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = m_sessions[h];

	CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession::SRConnection* pConnection = pSession->remoteConnection[id];
		CRY_ASSERT_MESSAGE(pConnection, "CCryMatchMaking: Remote connection base pointers not setup");

		if (pConnection->used)
		{
			NetLog("[Lobby] Free connection " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, id, pConnection->connectionID, pConnection->uid));
			m_lobby->ConnectionRemoveRef(pConnection->connectionID);
			pConnection->connectionID = CryLobbyInvalidConnectionID;
			pConnection->used = false;

			while (!pConnection->gameRecvQueue.Empty())
			{
				SSession::SRConnection::SData& data = pConnection->gameRecvQueue.Front();

				m_lobby->MemFree(data.data);
				pConnection->gameRecvQueue.Pop();
			}
		}
	}
	else
	{
		NetLog("[Lobby] Free connection called with invalid matchmaking connection id");
	}
}

	#if RESET_CONNECTED_CONNECTION
void CCryMatchMaking::ResetConnection(CryLobbyConnectionID id)
{
	NetLog("[Matchmaking] ResetConnection");

	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];

		assert(pSession);

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
			{
				SSession::SRConnection* pConnection = pSession->remoteConnection[j];

				CRY_ASSERT_MESSAGE(pConnection, "CCryMatchMaking: Connection base pointers not setup");

				if (pConnection->used)
				{
					if (pConnection->connectionID == id)
					{
						NetLog("[Matchmaking] FreeRemoteConnection " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(CryLobbySessionHandle(i), CryMatchMakingConnectionID(j), pConnection->connectionID, pConnection->uid));

						if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
						{
							SessionDisconnectRemoteConnectionViaNub(i, j, eDS_Remote, CryMatchMakingInvalidConnectionID, eDC_SessionDeleted, "reset connection");
						}
					}
				}
			}
		}
	}
}
	#endif

bool CCryMatchMaking::FindLocalConnectionFromUID(SCryMatchMakingConnectionUID uid, CryLobbySessionHandle* h)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* session = m_sessions[i];

		assert(session);

		if (session->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			SSession::SLConnection* connection = session->localConnection;

			assert(connection);

			if (connection->uid == uid)
			{
				*h = i;
				return true;
			}
		}
	}

	return false;
}

bool CCryMatchMaking::FindConnectionFromUID(SCryMatchMakingConnectionUID uid, CryLobbySessionHandle* pH, CryMatchMakingConnectionID* pID)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		if (FindConnectionFromSessionUID(i, uid, pID))
		{
			*pH = i;
			return true;
		}
	}

	return false;
}

bool CCryMatchMaking::FindConnectionFromSessionUID(CryLobbySessionHandle h, SCryMatchMakingConnectionUID uid, CryMatchMakingConnectionID* pID)
{
	SSession* pSession = m_sessions[h];

	CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
		{
			SSession::SRConnection* pConnection = pSession->remoteConnection[i];

			CRY_ASSERT_MESSAGE(pConnection, "CCryMatchMaking: Connection base pointers not setup");

			if (pConnection->used)
			{
				if (pConnection->uid == uid)
				{
					*pID = i;
					return true;
				}
			}
		}
	}

	return false;
}

CryMatchMakingTaskID CCryMatchMaking::FindTaskFromSendID(CryLobbySendID sendID)
{
	if (sendID != CryLobbyInvalidSendID)
	{
		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
		{
			STask* task = m_task[i];

			assert(task);

			if (task->used && (task->sendID == sendID))
			{
				return i;
			}
		}
	}

	return CryMatchMakingInvalidTaskID;
}

CryMatchMakingTaskID CCryMatchMaking::FindTaskFromTaskTaskID(uint32 etask, CryMatchMakingTaskID mmTaskID)
{
	if (mmTaskID != CryMatchMakingInvalidTaskID)
	{
		STask* task = m_task[mmTaskID];

		assert(task);

		if (task->used && (task->subTask == etask))
		{
			return mmTaskID;
		}
	}

	return CryMatchMakingInvalidTaskID;
}

CryMatchMakingTaskID CCryMatchMaking::FindTaskFromTaskSessionHandle(uint32 etask, CryLobbySessionHandle h)
{
	if (h != CryLobbyInvalidSessionHandle)
	{
		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
		{
			STask* pTask = m_task[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryMatchMaking: Task base pointers not setup");

			if (pTask->used && (pTask->subTask == etask) && (pTask->session == h))
			{
				return i;
			}
		}
	}

	return CryMatchMakingInvalidTaskID;
}

CryMatchMakingTaskID CCryMatchMaking::FindTaskFromLobbyTaskID(CryLobbyTaskID lTaskID)
{
	LOBBY_AUTO_LOCK;

	if (lTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
		{
			STask* task = m_task[i];

			assert(task);

			if (task->used && (task->lTaskID == lTaskID))
			{
				return i;
			}
		}
	}

	return CryMatchMakingInvalidTaskID;
}

void CCryMatchMaking::UpdateTaskError(CryMatchMakingTaskID mmTaskID, ECryLobbyError error)
{
	STask* task = m_task[mmTaskID];

	assert(task);

	if (task->error == eCLE_Success)
	{
		task->error = error;
	}
}

ECryLobbyError CCryMatchMaking::Initialise()
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		assert(m_sessions[i]);
		m_sessions[i]->localFlags = 0;
	}

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		assert(m_task[i]);
		m_task[i]->used = false;
	}

	return eCLE_Success;
}

ECryLobbyError CCryMatchMaking::Terminate()
{
	return eCLE_Success;
}

ESocketError CCryMatchMaking::Send(CryMatchMakingTaskID mmTaskID, CCryLobbyPacket* pPacket, CryLobbySessionHandle h, const TNetAddress& to)
{
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;
	CryLobbySendID id;

	if (h != CryLobbyInvalidSessionHandle)
	{
		pSPacket->SetFromConnectionUID(m_sessions[h]->localConnection->uid);
	}

	ESocketError ret = m_lobby->Send(pPacket, to, CryLobbyInvalidConnectionID, &id);

	if (ret == eSE_Ok)
	{
		if (mmTaskID != CryMatchMakingInvalidTaskID)
		{
			STask* pTask = m_task[mmTaskID];

			CRY_ASSERT_MESSAGE(pTask, "CCryMatchMaking: Task base pointers not setup");

			pTask->sendID = id;
			pTask->sendStatus = eCLE_Pending;
		}
	}

	return ret;
}

ESocketError CCryMatchMaking::Send(CryMatchMakingTaskID mmTaskID, CCryLobbyPacket* pPacket, CryLobbySessionHandle h, CryMatchMakingConnectionID connectionID)
{
	if ((h != CryLobbyInvalidSessionHandle) && (connectionID != CryMatchMakingInvalidConnectionID))
	{
		TNetAddress addr;

		CRY_ASSERT_MESSAGE(m_sessions[h], "CCryMatchMaking: Session base pointers not setup");
		CRY_ASSERT_MESSAGE(m_sessions[h]->remoteConnection[connectionID], "CCryMatchMaking: Session remote connection base pointers not setup");

		if (m_lobby->AddressFromConnection(addr, m_sessions[h]->remoteConnection[connectionID]->connectionID))
		{
			CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;
			CryLobbySendID id;

			pSPacket->SetFromConnectionUID(m_sessions[h]->localConnection->uid);

			ESocketError ret = m_lobby->Send(pSPacket, addr, m_sessions[h]->remoteConnection[connectionID]->connectionID, &id);

			if (ret == eSE_Ok)
			{
				if (mmTaskID != CryMatchMakingInvalidTaskID)
				{
					STask* pTask = m_task[mmTaskID];

					CRY_ASSERT_MESSAGE(pTask, "CCryMatchMaking: Task base pointers not setup");

					pTask->sendID = id;
					pTask->sendStatus = eCLE_Pending;
				}
			}

			return ret;
		}
	}

	return eSE_MiscFatalError;
}

void CCryMatchMaking::SendToAll(CryMatchMakingTaskID mmTaskID, CCryLobbyPacket* pPacket, CryLobbySessionHandle h, CryMatchMakingConnectionID skipID)
{
	SSession* session = m_sessions[h];

	assert(session);

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* connection = session->remoteConnection[i];

		assert(connection);

		if (connection->used && (i != skipID))
		{
			ESocketError result = Send(CryMatchMakingInvalidTaskID, pPacket, h, CryMatchMakingConnectionID(i));

			if (result != eSE_Ok)
			{
				NetLog("Failed to send packet to connection " PRFORMAT_SHMMCINFO ", result=%i", PRARG_SHMMCINFO(h, CryMatchMakingConnectionID(i), connection->connectionID, connection->uid), int(result));
			}
		}
	}
}

TMemHdl CCryMatchMaking::PacketToMemoryBlock(CCryLobbyPacket* pPacket, uint32& length)
{
	length = sizeof(CCryLobbyPacket) + pPacket->GetReadBufferSize();
	TMemHdl mh = m_lobby->MemAlloc(length);

	if (mh != TMemInvalidHdl)
	{
		uint8* pData = (uint8*)m_lobby->MemGetPtr(mh);
		CCrySharedLobbyPacket* pPacketMem = (CCrySharedLobbyPacket*)pData;

		memcpy(pData, pPacket, sizeof(CCryLobbyPacket));
		memcpy(&pData[sizeof(CCryLobbyPacket)], pPacket->GetReadBuffer(), pPacket->GetReadBufferSize());

		// Reset the packet buffer in the memory copy since the buffer belongs to pPacket.
		pPacketMem->ResetBuffer();
	}

	return mh;
}

CCryLobbyPacket* CCryMatchMaking::MemoryBlockToPacket(TMemHdl mh, uint32 length)
{
	if (mh != TMemInvalidHdl)
	{
		uint8* pData = (uint8*)m_lobby->MemGetPtr(mh);
		CCryLobbyPacket* pPacket = (CCryLobbyPacket*)pData;
		uint32 bufferPos = pPacket->GetReadBufferPos();

		pPacket->SetReadBuffer(&pData[sizeof(CCryLobbyPacket)], length - sizeof(CCryLobbyPacket));
		pPacket->SetReadBufferPos(bufferPos);

		return pPacket;
	}

	return NULL;
}

ECryLobbyError CCryMatchMaking::SendToServer(CCryLobbyPacket* pPacket, CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	CRY_ASSERT_MESSAGE(m_sessions[h], "CCryMatchMaking: Session base pointers not setup");

	if (!(m_sessions[h]->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
	{
		if (VerifyPacketBeforeSend(pPacket, "SendToServer") == false)
		{
			return eCLE_InvalidRequest;
		}

		uint32 length;
		TMemHdl mh = PacketToMemoryBlock(pPacket, length);

		if (mh != TMemInvalidHdl)
		{
			FROM_GAME_TO_LOBBY(&CCryMatchMaking::SendToServerNT, this, mh, length, h);
			return eCLE_Success;
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	return eCLE_InvalidRequest;
}

void CCryMatchMaking::SendToServerNT(TMemHdl mh, uint32 length, CryLobbySessionHandle h)
{
	LOBBY_AUTO_LOCK;
	CRY_ASSERT_MESSAGE(m_sessions[h], "CCryMatchMaking: Session base pointers not setup");

	CCryLobbyPacket* pPacket = MemoryBlockToPacket(mh, length);

	Send(CryMatchMakingInvalidTaskID, pPacket, h, m_sessions[h]->hostConnectionID);
	m_lobby->MemFree(mh);
}

ECryLobbyError CCryMatchMaking::SendToAllClients(CCryLobbyPacket* pPacket, CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	CRY_ASSERT_MESSAGE(m_sessions[h], "CCryMatchMaking: Session base pointers not setup");

	if (m_sessions[h]->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
	{
		if (VerifyPacketBeforeSend(pPacket, "SendToAllClients") == false)
		{
			return eCLE_InvalidRequest;
		}

		uint32 length;
		TMemHdl mh = PacketToMemoryBlock(pPacket, length);

		if (mh != TMemInvalidHdl)
		{
			FROM_GAME_TO_LOBBY(&CCryMatchMaking::SendToAllClientsNT, this, mh, length, h);
			return eCLE_Success;
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	return eCLE_InvalidRequest;
}

void CCryMatchMaking::SendToAllClientsNT(TMemHdl mh, uint32 length, CryLobbySessionHandle h)
{
	LOBBY_AUTO_LOCK;

	CCryLobbyPacket* pPacket = MemoryBlockToPacket(mh, length);

	SendToAll(CryMatchMakingInvalidTaskID, pPacket, h, CryMatchMakingInvalidConnectionID);
	m_lobby->MemFree(mh);
}

ECryLobbyError CCryMatchMaking::SendToClient(CCryLobbyPacket* pPacket, CrySessionHandle gh, SCryMatchMakingConnectionUID uid)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if (VerifyPacketBeforeSend(pPacket, "SendToClient") == false)
	{
		return eCLE_InvalidRequest;
	}

	uint32 length;
	TMemHdl mh = PacketToMemoryBlock(pPacket, length);

	if (mh != TMemInvalidHdl)
	{
		FROM_GAME_TO_LOBBY(&CCryMatchMaking::SendToClientNT, this, mh, length, h, uid);
		return eCLE_Success;
	}
	else
	{
		return eCLE_OutOfMemory;
	}
}

void CCryMatchMaking::SendToClientNT(TMemHdl mh, uint32 length, CryLobbySessionHandle h, SCryMatchMakingConnectionUID uid)
{
	LOBBY_AUTO_LOCK;

	CryMatchMakingConnectionID id;

	if (FindConnectionFromSessionUID(h, uid, &id))
	{
		CCryLobbyPacket* pPacket = MemoryBlockToPacket(mh, length);
		Send(CryMatchMakingInvalidTaskID, pPacket, h, id);
	}

	m_lobby->MemFree(mh);
}

ECryLobbyError CCryMatchMaking::SessionSendRequestInfo(CCryLobbyPacket* pPacket, CrySessionID id, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSendRequestInfoCallback pCB, void* pCBArg)
{
	return eCLE_ServiceNotSupported;
}

ECryLobbyError CCryMatchMaking::SessionSendRequestInfoResponse(CCryLobbyPacket* pPacket, CrySessionRequesterID requester)
{
	return eCLE_ServiceNotSupported;
}

void CCryMatchMaking::DispatchUserPacket(TMemHdl mh, uint32 length, CrySessionHandle gh)
{
	UCryLobbyEventData eventData;
	SCryLobbyUserPacketData packetData;
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)MemoryBlockToPacket(mh, length);

	eventData.pUserPacketData = &packetData;
	packetData.pPacket = pSPacket;
	packetData.session = gh;
	packetData.connection = pSPacket->GetFromConnectionUID();

	m_lobby->DispatchEvent(eCLSE_UserPacket, eventData);
	m_lobby->MemFree(mh);
}

void CCryMatchMaking::InitialDispatchUserPackets(CryLobbySessionHandle h, CryMatchMakingConnectionID connectionID)
{
	CRY_ASSERT_MESSAGE(h != CryLobbyInvalidSessionHandle, "CCryMatchMaking::InitialDispatchUserPackets: Invalid Session");
	CRY_ASSERT_MESSAGE(connectionID != CryMatchMakingInvalidConnectionID, "CCryMatchMaking::InitialDispatchUserPackets: Invalid connection ID");

	SSession* pSession = m_sessions[h];
	SSession::SRConnection* pConnection = pSession->remoteConnection[connectionID];

	while (!pConnection->gameRecvQueue.Empty())
	{
		SSession::SRConnection::SData& sdata = pConnection->gameRecvQueue.Front();

		CrySessionHandle gh = CreateGameSessionHandle(h, pSession->localConnection->uid);
		TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchUserPacket, this, sdata.data, sdata.dataSize, gh);

		pConnection->gameRecvQueue.Pop();
	}
}

void CCryMatchMaking::SessionDisconnectRemoteConnectionFromNub(CrySessionHandle gh, EDisconnectionCause cause)
{
	CALL_DEDICATED_SERVER_VERSION(SessionDisconnectRemoteConnectionFromNub(gh, cause));

	// eDC_NubDestroyed and eDC_UserRequested will be followed by a session delete which will also disconnect the connection if the game wants this to happen.
	// Other causes are errors that should still disconnect the connection.
	if ((cause != eDC_NubDestroyed) && (cause != eDC_UserRequested) && (cause != eDC_SessionDeleted))
	{
		for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
		{
			SSession* pSession = m_sessions[i];

			CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

			if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB))
			{
				if (gh == CrySessionInvalidHandle)
				{
					// We are a client (or something went terribly, terribly wrong) - assume we want to disconnect the server
					SessionDisconnectRemoteConnection(i, pSession->hostConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, cause);
				}
				else
				{
					uint32 uid = GetChannelIDFromGameSessionHandle(gh);

					CRY_ASSERT_MESSAGE(pSession->localConnection, "CCryMatchMaking: Local connection base pointer not setup");

					if (pSession->localConnection->uid.m_uid == uid)
					{
						SessionDisconnectRemoteConnection(i, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, cause);
					}
					else
					{
						for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
						{
							SSession::SRConnection* pConnection = pSession->remoteConnection[j];

							CRY_ASSERT_MESSAGE(pConnection, "CCryMatchMaking: Remote connection base pointers not setup");

							if (pConnection->used && (pConnection->uid.m_uid == uid))
							{
								SessionDisconnectRemoteConnection(i, j, eDS_Local, CryMatchMakingInvalidConnectionID, cause);
							}
						}
					}
				}
			}
		}
	}
}

void CCryMatchMaking::SessionDisconnectRemoteConnectionViaNub(CryLobbySessionHandle h, CryMatchMakingConnectionID id, eDisconnectSource source, CryMatchMakingConnectionID fromID, EDisconnectionCause cause, const char* pReason)
{
	SSession* pSession = m_sessions[h];

	NetLog("SessionDisconnectRemoteConnectionViaNub: " PRFORMAT_SH " " PRFORMAT_MMCID " cause %d '%s'", PRARG_SH(h), PRARG_MMCID(id), cause, pReason);
	CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");

	if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB))
	{
		INetNubPrivate* pNub = (INetNubPrivate*)gEnv->pGameFramework->GetServerNetNub();

		if (pNub)
		{
			if (id == CryMatchMakingInvalidConnectionID)
			{
				CRY_ASSERT_MESSAGE(pSession->localConnection, "CCryMatchMaking: Local connection base pointer not setup");
				pNub->DisconnectChannel(cause, pSession->localConnection->uid.m_uid, pReason);
			}
			else
			{
				CRY_ASSERT_MESSAGE(pSession->remoteConnection[id], "CCryMatchMaking: Remote connection base pointers not setup");
				pNub->DisconnectChannel(cause, pSession->remoteConnection[id]->uid.m_uid, pReason);
			}
		}
	}

	SessionDisconnectRemoteConnection(h, id, source, fromID, cause);
}

void CCryMatchMaking::SessionDisconnectRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id, eDisconnectSource source, CryMatchMakingConnectionID fromID, EDisconnectionCause cause)
{
	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT8Size;
	uint8 buffer[MaxBufferSize];
	uint32 bufferSize = 0;
	SSession* pSession = m_sessions[h];
	SCryMatchMakingConnectionUID uid = (id == CryMatchMakingInvalidConnectionID) ? pSession->localConnection->uid : pSession->remoteConnection[id]->uid;

	// Currently we assume that if pSession->serverConnectionID is valid then it has been allocated through the dedicated server arbitrator
	// and isn't part of the same service as the rest of the connections and so always needs to be sent a disconnect packet.
	bool bSendDisconnectPacket = true;

	CCrySharedLobbyPacket packet;

	switch (source)
	{
	case eDS_Local:
		fromID = CryMatchMakingInvalidConnectionID;
		break;

	case eDS_Remote:
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			fromID = id;
		}
		else
		{
			fromID = pSession->hostConnectionID;
		}

		break;

	case eDS_FromID:
		break;
	}

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eLobbyPT_SessionDeleteRemoteConnection, true);
	packet.WriteConnectionUID(uid);
	packet.WriteUINT8((uint8) cause);

	NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 0 " PRFORMAT_SH ", nub=%u used=%u " PRFORMAT_MMCID " from " PRFORMAT_MMCID,
	       PRARG_SH(h), pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB, pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED, PRARG_MMCID(id), PRARG_MMCID(fromID));

	if (id == CryMatchMakingInvalidConnectionID)
	{
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			// If this is our local connection and we are the host tell everyone we are leaving.
			if (bSendDisconnectPacket)
			{
				SendToAll(CryMatchMakingInvalidTaskID, &packet, h, CryMatchMakingInvalidConnectionID);
			}
			else
			{
				if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
				{
					Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->serverConnectionID);
				}
			}

			pSession->localConnection->used = false;
		}
		else
		{
			// If this is our local connection and we are not the host tell the host we are leaving.
			if (bSendDisconnectPacket)
			{
				Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->hostConnectionID);
			}

			FreeRemoteConnection(h, pSession->hostConnectionID);

			if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
			{
				Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->serverConnectionID);
				FreeRemoteConnection(h, pSession->serverConnectionID);
			}

			pSession->localConnection->used = false;
		}
	}
	else
	{
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			if (pSession->localConnection->used || gEnv->IsDedicated())
			{
				// If this is a remote connection and we are the host tell everyone the connection is leaving.
				if (bSendDisconnectPacket)
				{
					SendToAll(CryMatchMakingInvalidTaskID, &packet, h, fromID);
				}
				else
				{
					if ((pSession->serverConnectionID != fromID) && (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID))
					{
						Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->serverConnectionID);
					}
				}

	#if NETWORK_HOST_MIGRATION
				NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 1 " PRFORMAT_MMCINFO ", localFlags=0x%X, host" PRFORMAT_MMCID ", migrationState=%d, newhost" PRFORMAT_UID, PRARG_MMCINFO(id, pSession->remoteConnection[id]->connectionID, uid), pSession->localFlags, PRARG_MMCID(pSession->hostConnectionID), pSession->hostMigrationInfo.m_state, PRARG_UID(pSession->newHostUID));
	#else
				NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 1 " PRFORMAT_MMCINFO ", localFlags=0x%X", PRARG_MMCINFO(id, pSession->remoteConnection[id]->connectionID, uid), pSession->localFlags);
	#endif
				FreeRemoteConnection(h, id);

				if (pSession->serverConnectionID == id)
				{
					CrySessionHandle sessionHandle = CreateGameSessionHandle(h, pSession->localConnection->uid);
					TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_SessionClosed);
				}

	#if NETWORK_HOST_MIGRATION
				if (pSession->hostConnectionID == id)
				{
					if ((cause == eDC_NubDestroyed) || (cause == eDC_Timeout) || (cause == eDC_SessionDeleted) || (cause == eDC_ICMPError))
					{
						HostMigrationInitiate(h, cause);
					}
				}
	#endif
			}
		}
		else
		{
			if ((pSession->hostConnectionID == id) || (pSession->serverConnectionID == id)
	#if NETWORK_HOST_MIGRATION
			    || ((pSession->newHostUID == uid) && (pSession->hostMigrationInfo.m_state != eHMS_Idle) && (pSession->hostMigrationInfo.m_state != eHMS_Terminate))
	#endif
			    )
			{
				if (((pSession->hostConnectionID == id) || (pSession->serverConnectionID == id)) && (cause != eDC_ICMPError) && (fromID == CryMatchMakingInvalidConnectionID))
				{
					// We will generally get here if the nub has told us there was an error
					// as the host may not know we are leaving tell the host we are leaving.
					// If the cause is eDC_ICMPError we won't be able to send the message so don't bother trying.
					packet.StartWrite(eLobbyPT_SessionDeleteRemoteConnection, true);
					packet.WriteConnectionUID(pSession->localConnection->uid);
					packet.WriteUINT8((uint8) cause);

					if (bSendDisconnectPacket && (pSession->hostConnectionID == id))
					{
						Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->hostConnectionID);
					}

					if (pSession->serverConnectionID == id)
					{
						Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->serverConnectionID);
					}

					pSession->localConnection->used = false;
				}

	#if NETWORK_HOST_MIGRATION
				NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 2 " PRFORMAT_MMCINFO ", localFlags=0x%X, host" PRFORMAT_MMCID ", migrationState=%d, newhost" PRFORMAT_UID, PRARG_MMCINFO(id, pSession->remoteConnection[id]->connectionID, uid), pSession->localFlags, PRARG_MMCID(pSession->hostConnectionID), pSession->hostMigrationInfo.m_state, PRARG_UID(pSession->newHostUID));
	#else
				NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 2 " PRFORMAT_MMCINFO ", localFlags=0x%X", PRARG_MMCINFO(id, pSession->remoteConnection[id]->connectionID, uid), pSession->localFlags);
	#endif
				FreeRemoteConnection(h, id);

	#if NETWORK_HOST_MIGRATION
				if (pSession->serverConnectionID != id)
				{
					if ((cause == eDC_NubDestroyed) || (cause == eDC_Timeout) || (cause == eDC_SessionDeleted) || (cause == eDC_ICMPError))
					{
						HostMigrationInitiate(h, cause);
					}
				}

				if ((pSession->hostMigrationInfo.m_state == eHMS_Idle) || (pSession->serverConnectionID == id))
	#endif
				{
					CrySessionHandle sessionHandle = CreateGameSessionHandle(h, pSession->localConnection->uid);
					TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_SessionClosed);
				}
			}
			else
			{
				if (fromID == pSession->hostConnectionID)
				{
	#if NETWORK_HOST_MIGRATION
					NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 3 " PRFORMAT_MMCINFO ", localFlags=0x%X, host" PRFORMAT_MMCID ", migrationState=%d, newhost" PRFORMAT_UID, PRARG_MMCINFO(id, pSession->remoteConnection[id]->connectionID, uid), pSession->localFlags, PRARG_MMCID(pSession->hostConnectionID), pSession->hostMigrationInfo.m_state, PRARG_UID(pSession->newHostUID));
	#else
					NetLog("CCryMatchMaking::SessionDisconnectRemoteConnection() 3 " PRFORMAT_MMCINFO ", localFlags=0x%X", PRARG_MMCINFO(id, pSession->remoteConnection[id]->connectionID, uid), pSession->localFlags);
	#endif
					FreeRemoteConnection(h, id);
				}
			}
		}
	}
}

ECryLobbyError CCryMatchMaking::SessionTerminateHostHintingForGroup(CrySessionHandle gh, SCryMatchMakingConnectionUID* pConnections, uint32 numConnections, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;
	LOBBY_AUTO_LOCK;

	CRY_ASSERT_MESSAGE((pConnections != NULL) && (numConnections > 0), "CCryMatchMaking::SessionTerminateHostHintingForGroup() : passed a NULL pointer or zero connection count");

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	SSession* pSession = m_sessions[h];

	if ((h < MAX_MATCHMAKING_SESSIONS) && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CryMatchMakingTaskID mmTaskID;

			error = StartTask(eT_SessionTerminateHostHintingForGroup, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

			if (error == eCLE_Success)
			{
				STask* pTask = m_task[mmTaskID];

				error = CreateTaskParam(mmTaskID, MATCHMAKING_PARAM_DATA, NULL, numConnections, numConnections * sizeof(SCryMatchMakingConnectionUID));

				if (error == eCLE_Success)
				{
					SCryMatchMakingConnectionUID* pConnectionsData = static_cast<SCryMatchMakingConnectionUID*>(m_lobby->MemGetPtr(pTask->params[MATCHMAKING_PARAM_DATA]));
					for (uint32 index = 0; index < numConnections; ++index)
					{
						pConnectionsData[index] = pConnections[index];
					}

					FROM_GAME_TO_LOBBY(&CCryMatchMaking::StartTaskRunning, this, mmTaskID);
				}
				else
				{
					FreeTask(mmTaskID);
				}
			}
		}
		else
		{
			error = eCLE_InvalidRequest;
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[MatchMaking] Start SessionTerminateHostHintingForGroup error %d", error);

	return error;
}

void CCryMatchMaking::StartSessionPendingGroupDisconnect(CryMatchMakingTaskID mmTaskID)
{
	#if NETWORK_HOST_MIGRATION
	STask* pTask = m_task[mmTaskID];

	CryLobbySessionHandle handle = pTask->session;
	CryMatchMakingConnectionID id = CryMatchMakingInvalidConnectionID;
	uint32 numConnections = pTask->numParams[MATCHMAKING_PARAM_NUM_DATA];
	SCryMatchMakingConnectionUID* pConnectionsData = static_cast<SCryMatchMakingConnectionUID*>(m_lobby->MemGetPtr(pTask->params[MATCHMAKING_PARAM_DATA]));

	for (uint32 index = 0; index < numConnections; ++index)
	{
		if (FindConnectionFromUID(pConnectionsData[index], &handle, &id))
		{
			SSession* pSession = m_sessions[handle];
			SSession::SRConnection* pConnection = pSession->remoteConnection[id];

			NetLog("[Host Migration]: Pending group disconnect: connection " PRFORMAT_SHMMCINFO " leaving, being marked as unacceptable potential host", PRARG_SHMMCINFO(handle, id, pConnection->connectionID, pConnection->uid));
			pConnection->hostHintInfo.Reset(); // Worst possible host choice
		}
	}

	SendHostHintInformation(handle);
	m_sessions[pTask->session]->localFlags &= ~CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
	#endif
}

void CCryMatchMaking::TickSessionPendingGroupDisconnect(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryMatchMaking::SessionSetLocalFlags(CrySessionHandle gh, uint32 flags, CryLobbyTaskID* pTaskID, CryMatchmakingFlagsCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	SSession* pSession = m_sessions[h];

	if ((h < MAX_MATCHMAKING_SESSIONS) && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionModifyLocalFlags, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			STask* pTask = m_task[mmTaskID];
			pTask->numParams[MATCHMAKING_PARAM_FLAGS] = 0xffffffff;
			pTask->numParams[MATCHMAKING_PARAM_FLAGS_MASK] = flags & CRYSESSION_LOCAL_FLAG_GAME_MASK;

			FROM_GAME_TO_LOBBY(&CCryMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[MatchMaking] Start SessionSetLocalFlags error %d", error);

	return error;
}

ECryLobbyError CCryMatchMaking::SessionClearLocalFlags(CrySessionHandle gh, uint32 flags, CryLobbyTaskID* pTaskID, CryMatchmakingFlagsCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	SSession* pSession = m_sessions[h];

	if ((h < MAX_MATCHMAKING_SESSIONS) && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionModifyLocalFlags, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			STask* pTask = m_task[mmTaskID];
			pTask->numParams[MATCHMAKING_PARAM_FLAGS] = 0;
			pTask->numParams[MATCHMAKING_PARAM_FLAGS_MASK] = flags & CRYSESSION_LOCAL_FLAG_GAME_MASK;

			FROM_GAME_TO_LOBBY(&CCryMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[MatchMaking] Start SessionClearLocalFlags error %d", error);

	return error;
}

ECryLobbyError CCryMatchMaking::SessionGetLocalFlags(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingFlagsCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	SSession* pSession = m_sessions[h];

	if ((h < MAX_MATCHMAKING_SESSIONS) && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionModifyLocalFlags, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			STask* pTask = m_task[mmTaskID];
			pTask->numParams[MATCHMAKING_PARAM_FLAGS] = 0;
			pTask->numParams[MATCHMAKING_PARAM_FLAGS_MASK] = 0;

			FROM_GAME_TO_LOBBY(&CCryMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[MatchMaking] Start SessionGetLocalFlags error %d", error);

	return error;
}

void CCryMatchMaking::StartSessionModifyLocalFlags(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	CryLobbySessionHandle handle = pTask->session;
	SSession* pSession = m_sessions[handle];

	uint32 flags = pTask->numParams[MATCHMAKING_PARAM_FLAGS];
	uint32 mask = pTask->numParams[MATCHMAKING_PARAM_FLAGS_MASK];
	uint32 oldFlags = pSession->localFlags;

	pSession->localFlags = (flags & mask) | (oldFlags & (~mask));

	NetLog("[MatchMaking]: Setting local flags from %08x to %08x (" PRFORMAT_SH ")", oldFlags, pSession->localFlags, PRARG_SH(handle));
}

void CCryMatchMaking::TickSessionModifyLocalFlags(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

	#if NETWORK_HOST_MIGRATION
bool CCryMatchMaking::HostMigrationInitiate(CryLobbySessionHandle h, EDisconnectionCause cause)
{
	bool sessionMigrating = false;

	SSession* pSession = m_sessions[h];
	bool hostMigrationSupported = m_lobby->GetLobbyServiceFlag(m_serviceType, eCLSF_SupportHostMigration);

	if (hostMigrationSupported)
	{
		bool sessionMigratable = ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) == CRYSESSION_CREATE_FLAG_MIGRATABLE);

		if (sessionMigratable)
		{
			bool autoMigratePermitted = (CLobbyCVars::Get().netAutoMigrateHost != 0);

			if (autoMigratePermitted)
			{
				sessionMigrating = true;
				bool sessionCurrentlyMigrating = (pSession->hostMigrationInfo.m_state != eHMS_Idle);

				// Allow hints to be sent on this session again, regardless of whether one's already in flight
				pSession->localFlags |= CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;

				if (!sessionCurrentlyMigrating)
				{
					bool shouldMigrateNub = ((pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB) == CRYSESSION_CREATE_FLAG_NUB) && (pSession->serverConnectionID == CryMatchMakingInvalidConnectionID);

					pSession->hostMigrationInfo.Initialise(CreateGameSessionHandle(h, pSession->localConnection->uid), shouldMigrateNub);

					if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST)
					{
						NetLog("[Host Migration]: initiated " PRFORMAT_SH " - anticipated new host is " PRFORMAT_UID " (%sme)", PRARG_SH(h), PRARG_UID(pSession->newHostUID), (pSession->newHostUID.m_uid == pSession->localConnection->uid.m_uid) ? "" : "not ");
					}
					else
					{
						NetLog("[Host Migration]: flagged as unable to become host for " PRFORMAT_SH " " PRFORMAT_UID " - disconnecting", PRARG_SH(h), PRARG_UID(pSession->newHostUID));
						SessionDisconnectRemoteConnectionViaNub(h, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_SessionDeleted, "Unable to host migrate");
						sessionMigrating = false;
					}

					if (sessionMigrating == true)
					{
		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
						ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyHMAttempts");
						if (pVar != NULL)
						{
							pVar->Set(pVar->GetIVal() + 1);
						}
		#endif
						HostMigrationInitialise(h, cause);
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
						HostMigrationStateCheckInitialise(h);
		#endif
						m_hostMigration.Start(&pSession->hostMigrationInfo);
					}
				}
				else
				{
					if ((pSession->hostMigrationInfo.m_state > eHMS_WaitForNewServer))
					{
						SortNewHostPriorities(h);
						HostMigrationReset(h);
						m_hostMigration.Reset(&pSession->hostMigrationInfo);
						HostMigrationInitialise(h, eDC_Timeout);
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
						HostMigrationStateCheckInitialise(h);
		#endif

						NetLog("[Host Migration]: anticipated new host for " PRFORMAT_SH " has timed out; resetting", PRARG_SH(h));
					}
					else
					{
						NetLog("[Host Migration]: tried to migrate " PRFORMAT_SH " but it's already migrating", PRARG_SH(h));
					}
				}
			}
			else
			{
				NetLog("[Host Migration]: tried to migrate " PRFORMAT_SH " but automatic host migration is switched off", PRARG_SH(h));
			}
		}
		else
		{
			NetLog("[Host Migration]: tried to migrate " PRFORMAT_SH " but it's not migratable", PRARG_SH(h));
		}
	}
	else
	{
		NetLog("[Host Migration]: tried to migrate " PRFORMAT_SH " but host migration not supported by this lobby service", PRARG_SH(h));
	}

	if (!sessionMigrating)
	{
		OnHostMigrationFailedToStart(h);
	}

	return sessionMigrating;
}

void CCryMatchMaking::HostMigrationReset(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];

	if (pSession->hostMigrationInfo.m_taskID != CryLobbyInvalidTaskID)
	{
		CryMatchMakingTaskID mmTaskID = FindTaskFromLobbyTaskID(pSession->hostMigrationInfo.m_taskID);
		if (mmTaskID != CryMatchMakingInvalidTaskID)
		{
			STask* pTask = m_task[mmTaskID];
			if ((pTask->running) && (pTask->startedTask & HOST_MIGRATION_TASK_ID_FLAG))
			{
				// Cancel any host migration specific task
				NetLog("[Host Migration]: active task in HostMigrationReset() (task [%u][%u]) - canceling)", pTask->startedTask, pTask->subTask);
				CancelTask(pSession->hostMigrationInfo.m_taskID);
			}
		}

		pSession->hostMigrationInfo.m_taskID = CryLobbyInvalidTaskID;
	}
}

void CCryMatchMaking::HostMigrationFinalise(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];
	HostMigrationReset(h);

	pSession->desiredHostUID = CryMatchMakingInvalidConnectionUID;

	if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) == 0)
	{
		MarkHostHintInformationDirty(h);
	}
}

void CCryMatchMaking::HostMigrationWaitForNewServer(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];

	if (pSession->newHostUID.m_uid != pSession->localConnection->uid.m_uid)
	{
		CryMatchMakingConnectionID ConnectionIndex;
		bool foundAnticipatedNewHost = FindConnectionFromSessionUID(h, pSession->newHostUID, &ConnectionIndex);

		if (CLobbyCVars::Get().netAnticipatedNewHostLeaves != 0)
		{
			foundAnticipatedNewHost = false;
			CLobbyCVars::Get().netAnticipatedNewHostLeaves = 0;
		}

		if (!foundAnticipatedNewHost)
		{
			// Anticipated new host appears to have left, so nuke it's hint (if it exists), resort the hint list and try again
			NetLog("[Host Migration]: anticipated new host " PRFORMAT_UID " not found - looking for next anticipated new host", PRARG_UID(pSession->newHostUID));
			if (pSession->newHostPriorityCount)
			{
				pSession->newHostPriority[0]->hostHintInfo.Reset();
			}
			SortNewHostPriorities(h);
			HostMigrationReset(h);
			HostMigrationInitialise(h, eDC_Timeout);
		}
	}
	else
	{
		if (CLobbyCVars::Get().netAnticipatedNewHostLeaves != 0)
		{
			// Disconnect the anticipated new host so that the 'next' client tries to take ownership
			NetLog("[Host Migration]: anticipated new host told to leave");
			m_hostMigration.Terminate(&pSession->hostMigrationInfo);
		}
	}
}

		#if ENABLE_HOST_MIGRATION_STATE_CHECK
void CCryMatchMaking::HostMigrationStateCheckInitialise(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];

	pSession->localConnection->hostMigrationStateCheckDatas.resize(0);

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		pSession->remoteConnection[i]->hostMigrationStateCheckDatas.resize(0);
	}
}

void CCryMatchMaking::HostMigrationStateCheck(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];

	if (pSession->hostMigrationInfo.ShouldMigrateNub())
	{
		pSession->hostMigrationInfo.m_stateCheckDone = false;
		FROM_GAME_TO_LOBBY(&CCryMatchMaking::HostMigrationStateCheckNT, this, h);
	}
	else
	{
		pSession->hostMigrationInfo.m_stateCheckDone = true;
	}
}

void CCryMatchMaking::HostMigrationStateCheckNT(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];
	THostMigrationStateCheckDatas& datas = pSession->localConnection->hostMigrationStateCheckDatas;
	uint32 numDatas;

	m_hostMigrationStateCheckSession = h;
	((INetworkPrivate*)gEnv->pNetwork)->BroadcastNetDump(eNDT_HostMigrationStateCheck);
	m_hostMigrationStateCheckSession = CryLobbyInvalidSessionHandle;

	numDatas = datas.size();

	NetLog(" ");
	NetLog("Host migration state check data for " PRFORMAT_SH, PRARG_SH(h));

	for (uint32 i = 0; i < numDatas; i++)
	{
		HostMigrationStateCheckLogDataItem(&datas[i]);
	}

	NetLog(" ");

	if (!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
	{
		CCrySharedLobbyPacket packet;

		if (packet.CreateWriteBuffer(MAX_LOBBY_PACKET_SIZE))
		{
			uint32 lastPacketBoolPos;
			uint32 dataPos;

			packet.StartWrite(eLobbyPT_HostMigrationStateCheckData, true);
			lastPacketBoolPos = packet.GetWriteBufferPos();
			packet.WriteBool(false);
			dataPos = packet.GetWriteBufferPos();

			for (uint32 i = 0; i < numDatas; i++)
			{
				if (!HostMigrationStateCheckWriteDataItemToPacket(&packet, &datas[i]))
				{
					Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->hostConnectionID);
					packet.SetWriteBufferPos(dataPos);
					HostMigrationStateCheckWriteDataItemToPacket(&packet, &datas[i]);
				}
			}

			dataPos = packet.GetWriteBufferPos();
			packet.SetWriteBufferPos(lastPacketBoolPos);
			packet.WriteBool(true);
			packet.SetWriteBufferPos(dataPos);
			Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->hostConnectionID);
		}
	}

	pSession->hostMigrationInfo.m_stateCheckDone = true;
}

void CCryMatchMaking::HostMigrationStateCheckAddDataItem(SHostMigrationStateCheckData* pData)
{
	if (m_hostMigrationStateCheckSession != CryLobbyInvalidSessionHandle)
	{
		m_sessions[m_hostMigrationStateCheckSession]->localConnection->hostMigrationStateCheckDatas.push_back(*pData);
	}
}

void CCryMatchMaking::HostMigrationStateCheckLogDataItem(SHostMigrationStateCheckData* pData)
{
	switch (pData->type)
	{
	case eHMSCDT_NID:
		NetLog("NetID %u:%u Name %s Allocated %d Controlled %d SpawnType %u Owned %d Aspects %.08x",
		       pData->nid.id, pData->nid.salt, pData->nid.name, pData->nid.allocated, pData->nid.controlled, pData->nid.spawnType, pData->nid.owned, pData->nid.aspectsEnabled);
		break;

	default:
		NetLog("CCryMatchMaking::HostMigrationStateCheckLogDataItem: Unknown data type %d", pData->type);
		CRY_ASSERT_MESSAGE(0, "CCryMatchMaking::HostMigrationStateCheckLogDataItem: Unknown data type");
		break;
	}
}

bool CCryMatchMaking::HostMigrationStateCheckCompareDataItems(SHostMigrationStateCheckData* pData1, SHostMigrationStateCheckData* pData2)
{
	if (pData1->type == pData2->type)
	{
		switch (pData1->type)
		{
		case eHMSCDT_NID:
			return (pData1->nid.id == pData2->nid.id) && (strcmp(pData1->nid.name, pData2->nid.name) == 0);

		default:
			NetLog("CCryMatchMaking::HostMigrationStateCheckCompareDataItems: Unknown data type %d", pData1->type);
			CRY_ASSERT_MESSAGE(0, "CCryMatchMaking::HostMigrationStateCheckCompareDataItems: Unknown data type");
			break;
		}
	}

	return false;
}

bool CCryMatchMaking::HostMigrationStateCheckWriteDataItemToPacket(CCryLobbyPacket* pPacket, SHostMigrationStateCheckData* pData)
{
	switch (pData->type)
	{
	case eHMSCDT_NID:
		{
			const uint32 itemSize = CryLobbyPacketUINT8Size + CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size + CryLobbyPacketUINT16Size + HMSCD_MAX_ENTITY_NAME_LENGTH +
			                        CryLobbyPacketBoolSize + CryLobbyPacketBoolSize + CryLobbyPacketBoolSize + CryLobbyPacketUINT8Size;

			if ((pPacket->GetWriteBufferPos() + itemSize) <= pPacket->GetWriteBufferSize())
			{
				pPacket->WriteUINT8(pData->type);
				pPacket->WriteUINT32(pData->nid.aspectsEnabled);
				pPacket->WriteUINT16(pData->nid.id);
				pPacket->WriteUINT16(pData->nid.salt);
				pPacket->WriteString(pData->nid.name, HMSCD_MAX_ENTITY_NAME_LENGTH);
				pPacket->WriteBool(pData->nid.allocated);
				pPacket->WriteBool(pData->nid.controlled);
				pPacket->WriteBool(pData->nid.owned);
				pPacket->WriteUINT8(pData->nid.spawnType);
				return true;
			}

			break;
		}

	default:
		NetLog("CCryMatchMaking::HostMigrationStateCheckWriteDataItemToPacket: Unknown data type %d", pData->type);
		CRY_ASSERT_MESSAGE(0, "CCryMatchMaking::HostMigrationStateCheckWriteDataItemToPacket: Unknown data type");
		break;
	}

	return false;
}

bool CCryMatchMaking::HostMigrationStateCheckReadDataItemFromPacket(CCryLobbyPacket* pPacket, SHostMigrationStateCheckData* pData)
{
	if ((pPacket->GetReadBufferPos() + CryLobbyPacketUINT8Size) <= pPacket->GetReadBufferSize())
	{
		pData->type = (EHostMigrationStateCheckDataType)pPacket->ReadUINT8();

		switch (pData->type)
		{
		case eHMSCDT_NID:
			pData->nid.aspectsEnabled = pPacket->ReadUINT32();
			pData->nid.id = pPacket->ReadUINT16();
			pData->nid.salt = pPacket->ReadUINT16();
			pPacket->ReadString(pData->nid.name, HMSCD_MAX_ENTITY_NAME_LENGTH);
			pData->nid.allocated = pPacket->ReadBool();
			pData->nid.controlled = pPacket->ReadBool();
			pData->nid.owned = pPacket->ReadBool();
			pData->nid.spawnType = pPacket->ReadUINT8();

			return true;

		default:
			NetLog("CCryMatchMaking::HostMigrationStateCheckReadDataItemFromPacket: Unknown data type %d", pData->type);
			CRY_ASSERT_MESSAGE(0, "CCryMatchMaking::HostMigrationStateCheckReadDataItemFromPacket: Unknown data type");
			break;
		}
	}

	return false;
}

void CCryMatchMaking::ProcessHostMigrationStateCheckData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	SCryMatchMakingConnectionUID uid;
	CryLobbySessionHandle h;
	CryMatchMakingConnectionID id;

	pPacket->StartRead();
	uid = pPacket->GetFromConnectionUID();

	if (FindConnectionFromUID(uid, &h, &id))
	{
		SSession* pSession = m_sessions[h];
		THostMigrationStateCheckDatas& clientDatas = pSession->remoteConnection[id]->hostMigrationStateCheckDatas;
		SHostMigrationStateCheckData data;
		bool lastPacket;

		lastPacket = pPacket->ReadBool();

		while (HostMigrationStateCheckReadDataItemFromPacket(pPacket, &data))
		{
			clientDatas.push_back(data);
		}

		if (lastPacket)
		{
			THostMigrationStateCheckDatas& serverDatas = pSession->localConnection->hostMigrationStateCheckDatas;
			uint32 clientNumDatas = clientDatas.size();
			uint32 serverNumDatas = serverDatas.size();
			TMemHdl mhc = m_lobby->MemAlloc(clientNumDatas * sizeof(bool));
			TMemHdl mhs = m_lobby->MemAlloc(serverNumDatas * sizeof(bool));
			uint32 c, s;

			NetLog(" ");
			NetLog("Got host migration state check data for " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, id, pSession->remoteConnection[id]->connectionID, pSession->remoteConnection[id]->uid));

			for (c = 0; c < clientNumDatas; c++)
			{
				HostMigrationStateCheckLogDataItem(&clientDatas[c]);
			}

			if ((mhc != TMemInvalidHdl) && (mhs != TMemInvalidHdl))
			{
				bool* pClientDataFound = (bool*)m_lobby->MemGetPtr(mhc);
				bool* pServerDataFound = (bool*)m_lobby->MemGetPtr(mhs);

				memset(pClientDataFound, 0, clientNumDatas * sizeof(bool));
				memset(pServerDataFound, 0, serverNumDatas * sizeof(bool));

				for (c = 0; c < clientNumDatas; c++)
				{
					for (s = 0; s < serverNumDatas; s++)
					{
						if (HostMigrationStateCheckCompareDataItems(&clientDatas[c], &serverDatas[s]))
						{
							pClientDataFound[c] = true;
							pServerDataFound[s] = true;
							break;
						}
					}
				}

				NetLog("Host migration state check data client has that server does not");

				for (c = 0; c < clientNumDatas; c++)
				{
					if (!pClientDataFound[c])
					{
						HostMigrationStateCheckLogDataItem(&clientDatas[c]);
					}
				}

				NetLog("Host migration state check data server has that client does not");

				for (s = 0; s < serverNumDatas; s++)
				{
					if (!pServerDataFound[s])
					{
						HostMigrationStateCheckLogDataItem(&serverDatas[s]);
					}
				}
			}

			NetLog(" ");

			m_lobby->MemFree(mhc);
			m_lobby->MemFree(mhs);
		}
	}
}
		#endif
	#endif

void CCryMatchMaking::ProcessSessionDeleteRemoteConnection(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	uint32 bufferPos = 0;
	SCryMatchMakingConnectionUID uid;
	CryLobbySessionHandle h;
	CryMatchMakingConnectionID id;
	EDisconnectionCause cause;

	pPacket->StartRead();
	uid = pPacket->ReadConnectionUID();
	cause = (EDisconnectionCause) pPacket->ReadUINT8();

	if (FindConnectionFromUID(uid, &h, &id))
	{
		SCryMatchMakingConnectionUID fromUID = pPacket->GetFromConnectionUID();
		CryMatchMakingConnectionID fromID;

		if (FindConnectionFromSessionUID(h, fromUID, &fromID))
		{
			SessionDisconnectRemoteConnectionViaNub(h, id, eDS_FromID, fromID, eDC_SessionDeleted, "Session deleted");
		}
	}
	else
	{
		if (FindLocalConnectionFromUID(uid, &h))
		{
			// Server has just kicked us, so drop the connection to the server
			SSession* pSession = m_sessions[h];
			pSession->createFlags &= ~CRYSESSION_CREATE_FLAG_MIGRATABLE;

			if (pSession->hostConnectionID != CryMatchMakingInvalidConnectionID)
			{
				SSession::SRConnection* pConnection = pSession->remoteConnection[pSession->hostConnectionID];
				m_lobby->ConnectionSetState(pConnection->connectionID, eCLCS_NotConnected);
			}

			CrySessionHandle sessionHandle = CreateGameSessionHandle(h, uid);
			switch (cause)
			{
			case eDC_KickedHighPing:
				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_KickedHighPing);
				break;

			case eDC_KickedReservedUser:
				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_KickedReservedUser);
				break;

			case eDC_GloballyBanned:
				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_KickedGlobalBan);
				break;

			case eDC_Global_Ban1:
				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_KickedGlobalBanStage1);
				break;

			case eDC_Global_Ban2:
				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_KickedGlobalBanStage2);
				break;

			default:
				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionEvent, this, sessionHandle, eCLSE_KickedFromSession);
				break;
			}
		}
	}
}

	#if NETWORK_HOST_MIGRATION
void CCryMatchMaking::ProcessHostHinting(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryMatchMakingConnectionID id;
	CryLobbySessionHandle handle = CryLobbyInvalidSessionHandle;
	bool hintsChanged = false;

	pPacket->StartRead();
	SCryMatchMakingConnectionUID uid = pPacket->GetFromConnectionUID();
	if (FindConnectionFromUID(uid, &handle, &id))
	{
		SSession* pSession = m_sessions[handle];
		SSession::SRConnection* pConnection = pSession->remoteConnection[id];

		#if NET_HOST_HINT_DEBUG
		NetLog("[Host Hints]: received hint packet from " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(handle, id, pConnection->connectionID, uid));
		#endif // #if NET_HOST_HINT_DEBUG

		// First item is always sender's info
		SHostHintInformation incomingHintInformation;
		incomingHintInformation.natType = pPacket->ReadUINT8();
		incomingHintInformation.numActiveConnections = pPacket->ReadUINT8();
		incomingHintInformation.ping = pPacket->ReadUINT16();
		incomingHintInformation.upstreamBPS = pPacket->ReadUINT32();

		hintsChanged |= ProcessSingleHostHint(handle, uid.m_uid, pConnection->hostHintInfo, incomingHintInformation);
		const uint32 dataSize = pPacket->GetReadBufferSize() - CryLobbyPacketHeaderSize - (SHostHintInformation::PAYLOAD_SIZE);

		if (pSession->hostConnectionID == id)
		{
			const uint32 itemCount = dataSize / (CryLobbyPacketUINT32Size + SHostHintInformation::PAYLOAD_SIZE);
		#if NET_HOST_HINT_DEBUG
			NetLog("[Host Hints]: received hint packet has payload of %i hints", itemCount);
		#endif // #if NET_HOST_HINT_DEBUG

			// From host; start unpacking NAT, number of active connections and average ping time information
			for (uint32 itemIndex = 0; itemIndex < itemCount; ++itemIndex)
			{
				uid.m_uid = pPacket->ReadUINT32();
				incomingHintInformation.natType = pPacket->ReadUINT8();
				incomingHintInformation.numActiveConnections = pPacket->ReadUINT8();
				incomingHintInformation.ping = pPacket->ReadUINT16();
				incomingHintInformation.upstreamBPS = pPacket->ReadUINT32();

				if (FindConnectionFromSessionUID(handle, uid, &id))
				{
					pConnection = pSession->remoteConnection[id];
					hintsChanged |= ProcessSingleHostHint(handle, uid.m_uid, pConnection->hostHintInfo, incomingHintInformation);
				}
				else
				{
					if (pSession->localConnection->uid == uid)
					{
						hintsChanged |= ProcessSingleHostHint(handle, uid.m_uid, pSession->hostHintInfo, incomingHintInformation);
					}
					else
					{
		#if NET_HOST_HINT_DEBUG
						NetLog("[Host Hints]: received payload hint for unknown connection " PRFORMAT_UID, PRARG_UID(uid));
		#endif  // #if NET_HOST_HINT_DEBUG
					}
				}
			}
		}
	}
	else
	{
		#if NET_HOST_HINT_DEBUG
		NetLog("[Host Hints]: received hint for unknown connection " PRFORMAT_UID, PRARG_UID(uid));
		#endif // #if NET_HOST_HINT_DEBUG
	}

	if (handle != CryLobbyInvalidSessionHandle)
	{
		// N.B. Each host hint packet contains hints for only one session, so process the entire packet before sorting
		SortNewHostPriorities(handle);

		SSession* pSession = m_sessions[handle];
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			if (pSession->hostConnectionID == CryMatchMakingInvalidConnectionID)
			{
				// Send the updated information on to the clients
				MarkHostHintInformationDirty(handle);
				if (hintsChanged)
				{
					MarkHostHintExternalInformationDirty(handle);
				}
			}
			else
			{
		#if NET_HOST_HINT_DEBUG
				NetLog("[Host Hints]: allocated server received hints from session host");
		#endif
			}
		}
		else
		{
			// Client has received server confirmation; allow further hints to be sent
			//if (pSession->outboundHostHintInfo == pSession->hostHintInfo)
			if (1)
			{
		#if NET_HOST_HINT_DEBUG
				NetLog("[Host Hints]: client received server confirmation; able to send more hints if necessary");
		#endif // #if NET_HOST_HINT_DEBUG
				pSession->localFlags |= CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
			}
		}
	}
}

bool CCryMatchMaking::ProcessSingleHostHint(CryLobbySessionHandle h, uint32 uid, SHostHintInformation& hintToUpdate, SHostHintInformation& incomingHint)
{
	SSession* pSession = m_sessions[h];
		#if NET_HOST_HINT_DEBUG
	NetLog("[Host Hints]: Hint: uid %i, NAT %i, %i cx, %ims", uid, incomingHint.natType, incomingHint.numActiveConnections, incomingHint.ping);
		#endif // #if NET_HOST_HINT_DEBUG

	bool canProcess = false;
	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
	{
		if ((pSession->hostMigrationInfo.m_state == eHMS_Idle) || (pSession->hostMigrationInfo.m_state > eHMS_PromoteToServer))
		{
			canProcess = true;
		}
	}
	else
	{
		if ((pSession->hostMigrationInfo.m_state == eHMS_Idle) || (pSession->hostMigrationInfo.m_state > eHMS_ReconnectClient))
		{
			canProcess = true;
		}
	}

	if (canProcess)
	{
		if (hintToUpdate != incomingHint)
		{
			hintToUpdate = incomingHint;
		}
	}
	else
	{
		NetLog("[Host Migration]:   ignoring - we're migrating this session");
	}

	return canProcess;
}

void CCryMatchMaking::SortNewHostPriorities(CryLobbySessionHandle h)
{
		#if NET_HOST_HINT_DEBUG
	NetLog("[Host Hints]: sorting hints for " PRFORMAT_SH, PRARG_SH(h));
		#endif // #if NET_HOST_HINT_DEBUG
	SSession* pSession = m_sessions[h];
	pSession->newHostPriorityCount = 0;
	pSession->newHostPriority[0] = NULL;

	// Firstly, populate the array with all the used connections
	for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; ++connectionIndex)
	{
		SSession::SRConnection* pConnection = pSession->remoteConnection[connectionIndex];
		if ((pConnection->used) && (pSession->hostConnectionID != connectionIndex))
		{
			pSession->newHostPriority[pSession->newHostPriorityCount++] = pConnection;
		}
	}

	// Assume the anticipated new host is local
	pSession->newHostUID = pSession->localConnection->uid;
	SHostHintInformation newHost = pSession->hostHintInfo;

	// Is there a better candidate in the new host priority list?
	if (pSession->newHostPriorityCount > 0)
	{
		// Secondly, sort the list according to the following metric:
		// (A) by number of active connections
		// (B) by average ping
		// (C) by upstream bits per second
		// (D) by NAT type
		// (E) if all else is the same, by uid
		qsort(pSession->newHostPriority, pSession->newHostPriorityCount, sizeof(SSession::SRConnection*), SortNewHostPriorities_Comparator);

		// C6385: Invalid data: accessing 'pSession->newHostPriority', the readable size is '1*0' bytes, but '4' bytes might be read
		PREFAST_SUPPRESS_WARNING(6385);
		const SSession::SRConnection& bestHint = *pSession->newHostPriority[0];

		int result = SortNewHostPriorities_ComparatorHelper(bestHint.hostHintInfo, newHost);

		// Anticipated new host will be taken from the hint list if...
		if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) ||      // ...we're the current host (i.e. session MUST migrate away from us), or
		    (result < 0) ||                                             // ...the hint list has a better option, or
		    ((result == 0) && (bestHint.uid < pSession->newHostUID)))   // ...the hint list has an equivalent suggestion but a 'better' uid
		{
			pSession->newHostUID = bestHint.uid;
			newHost = bestHint.hostHintInfo;
		}

		for (uint32 hintIndex = 0; hintIndex < pSession->newHostPriorityCount; ++hintIndex)
		{
			assert(hintIndex < MAX_LOBBY_CONNECTIONS && pSession->newHostPriority[hintIndex]);
		#if NET_HOST_HINT_DEBUG
			NetLog("[Host Hints]: Hint %i : uid %i, nat %i, %i connections, up %dkbps, %ims", hintIndex, pSession->newHostPriority[hintIndex]->uid.m_uid, pSession->newHostPriority[hintIndex]->hostHintInfo.natType, pSession->newHostPriority[hintIndex]->hostHintInfo.numActiveConnections, pSession->newHostPriority[hintIndex]->hostHintInfo.upstreamBPS / 1000, pSession->newHostPriority[hintIndex]->hostHintInfo.ping);
		#endif // #if NET_HOST_HINT_DEBUG
		}
	}

	NetLog("[Host Hints]: Hinting " PRFORMAT_SH " thinks new host should be uid %u (nat %u, %u connections, up %ukbps, %ums) (%sme)", PRARG_SH(h), pSession->newHostUID.m_uid, newHost.natType, newHost.numActiveConnections, newHost.upstreamBPS / 1000, newHost.ping, (pSession->newHostUID == pSession->localConnection->uid) ? "" : "not ");
}

int CCryMatchMaking::SortNewHostPriorities_Comparator(const void* pLHS, const void* pRHS)
{
	const SSession::SRConnection* pLHSConnection = *(const SSession::SRConnection**)pLHS;
	const SSession::SRConnection* pRHSConnection = *(const SSession::SRConnection**)pRHS;

	CCryMatchMaking* pMatchMaking = static_cast<CCryMatchMaking*>(CCryLobby::GetLobby()->GetMatchMaking());
	// Compares the number of active connections (highest number of connections is best)
	int result = pMatchMaking->SortNewHostPriorities_ComparatorHelper(pLHSConnection->hostHintInfo, pRHSConnection->hostHintInfo);
	if (result == 0)
	{
		// If there's nothing to separate two clients, compare the uids (lowest uid is best)
		result = pLHSConnection->uid - pRHSConnection->uid;
	}

	return result;
}

int CCryMatchMaking::SortNewHostPriorities_ComparatorHelper(const SHostHintInformation& LHS, const SHostHintInformation& RHS)
{
	// Compares the number of active connections (highest number of connections is best)
	int result = RHS.numActiveConnections - LHS.numActiveConnections;
	if (result == 0)
	{
		// Compares average ping (lower is best)
		result = LHS.ping - RHS.ping;
		if (result == 0)
		{
			// Compares upstream bits per second (higher is best)
			result = RHS.upstreamBPS - LHS.upstreamBPS;
			if (result == 0)
			{
				// Compares the NAT type ('lowest' NAT type is best - eNT_Unknown is treated as unsigned and therefore 'large' and worst)
				result = LHS.natType - RHS.natType;
			}
		}
	}

	return result;
}

		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
void CCryMatchMaking::DetectHostMigrationTaskBreak(CryLobbySessionHandle h, char* pLocation)
{
	SSession* pSession = m_sessions[h];

	if (pSession->hostMigrationInfo.m_taskID != CryLobbyInvalidTaskID)
	{
		CryMatchMakingTaskID taskID = FindTaskFromLobbyTaskID(pSession->hostMigrationInfo.m_taskID);
		if (taskID != CryMatchMakingInvalidTaskID)
		{
			STask* pTask = m_task[taskID];
			if (pTask->startedTask & HOST_MIGRATION_TASK_ID_FLAG)
			{
				CryLogAlways("[Host Migration]: HOST MIGRATION TASK BREAK DETECTED (task [%u][%u] active when %s called)", pTask->startedTask, pTask->subTask, pLocation);
				ICVar* pVar = gEnv->pConsole->GetCVar("gl_debugLobbyBreaksHMTasks");
				if (pVar != NULL)
				{
					pVar->Set(pVar->GetIVal() + 1);
				}
			}
		}

	}
}
		#endif
	#endif

void CCryMatchMaking::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;
	uint32 packetType = pSPacket->StartRead();

	if (packetType < CRYLOBBY_USER_PACKET_START)
	{
		switch (packetType)
		{
		case eLobbyPT_SessionDeleteRemoteConnection:
			{
				ProcessSessionDeleteRemoteConnection(addr, pSPacket);
			}
			break;

	#if NETWORK_HOST_MIGRATION
		case eLobbyPT_HostHinting:
			ProcessHostHinting(addr, pSPacket);
			break;

		#if ENABLE_HOST_MIGRATION_STATE_CHECK
		case eLobbyPT_HostMigrationStateCheckData:
			ProcessHostMigrationStateCheckData(addr, pSPacket);
			break;
		#endif
	#endif

		case eLobbyPT_ServerPing:
			ProcessServerPing(addr, pSPacket);
			break;

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
		case eLobbyPT_A2C_RequestSetupDedicatedServerResult:
			ProcessRequestSetupDedicatedServerResult(addr, pSPacket);
			break;

		case eLobbyPT_A2C_RequestReleaseDedicatedServerResult:
			ProcessRequestReleaseDedicatedServerResult(addr, pSPacket);
			break;

		case eLobbyPT_SessionServerInfo:
			ProcessSessionServerInfo(addr, pSPacket);
			break;

		case eLobbyPT_SessionRequestJoinServerResult:
			ProcessSessionRequestJoinServerResult(addr, pSPacket);
			break;

		case eLobbyPT_D2C_HostMigrationStart:
			ProcessD2CHostMigrationStart(addr, pSPacket);
			break;
	#endif
		}
	}
	else
	{
		SCryMatchMakingConnectionUID uid = pSPacket->GetFromConnectionUID();
		CryLobbySessionHandle h = CryLobbyInvalidSessionHandle;
		CryMatchMakingConnectionID id = CryMatchMakingInvalidConnectionID;

		if (FindConnectionFromUID(uid, &h, &id))
		{
			SSession* pSession = m_sessions[h];
			SSession::SRConnection* pConnection = pSession->remoteConnection[id];
			uint32 size;
			TMemHdl mh = PacketToMemoryBlock(pSPacket, size);

			if (mh != TMemInvalidHdl)
			{
				if (pConnection->gameInformedConnectionEstablished)
				{
					// Safe to dispatch packet now
					CrySessionHandle gh = CreateGameSessionHandle(h, pSession->localConnection->uid);
					TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchUserPacket, this, mh, size, gh);
				}
				else
				{
					if (!pConnection->gameRecvQueue.Full())
					{
						// Save and dispatch later
						SSession::SRConnection::SData sdata;

						sdata.data = mh;
						sdata.dataSize = size;

						pConnection->gameRecvQueue.Push(sdata);
						NetLog("[Lobby] " PRFORMAT_SHMMCINFO " Queue game packet", PRARG_SHMMCINFO(h, id, pConnection->connectionID, pConnection->uid));
					}
					else
					{
						m_lobby->MemFree(mh);
						NetLog("[Lobby] " PRFORMAT_SHMMCINFO " Game receive queue full", PRARG_SHMMCINFO(h, id, pConnection->connectionID, pConnection->uid));
					}
				}
			}
		}
	}
}

void CCryMatchMaking::OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID)
{
	CryMatchMakingTaskID mmTaskID = FindTaskFromSendID(sendID);

	if (mmTaskID != CryMatchMakingInvalidTaskID)
	{
		assert(m_task[mmTaskID]);
		m_task[mmTaskID]->sendStatus = eCLE_InternalError;
	}
}

void CCryMatchMaking::OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID)
{
	CryMatchMakingTaskID mmTaskID = FindTaskFromSendID(sendID);

	if (mmTaskID != CryMatchMakingInvalidTaskID)
	{
		assert(m_task[mmTaskID]);
		m_task[mmTaskID]->sendStatus = eCLE_Success;
	}

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
	if (sendID != CryLobbyInvalidSendID)
	{
		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
		{
			STask* pTask = m_task[i];

			if (pTask->used && pTask->running)
			{
				switch (pTask->subTask)
				{
				case eT_SessionReleaseDedicatedServer:
					OnSendCompleteSessionReleaseDedicatedServer(i, sendID);
					break;
				}
			}
		}
	}
	#endif
}

	#if NETWORK_HOST_MIGRATION
bool CCryMatchMaking::IsSessionMigrated(SHostMigrationInfo* pInfo)
{
	CryLobbySessionHandle sessionIndex = GetSessionHandleFromGameSessionHandle(pInfo->m_session);
	SSession* pSession = m_sessions[sessionIndex];

	return pSession->hostMigrationInfo.m_sessionMigrated;
}

bool CCryMatchMaking::AreAnySessionsMigrating(void)
{
	LOBBY_AUTO_LOCK;

	bool sessionsMigrating = false;

	for (uint32 sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
	{
		SSession* pSession = m_sessions[sessionIndex];
		sessionsMigrating |= (pSession->hostMigrationInfo.m_state != eHMS_Idle);
	}

	return sessionsMigrating;
}

bool CCryMatchMaking::IsSessionMigrating(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	bool sessionMigrating = false;

	if (gh != CrySessionInvalidHandle)
	{
		CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

		SSession* pSession = m_sessions[h];
		sessionMigrating = (pSession->hostMigrationInfo.m_state != eHMS_Idle);
	}
	else
	{
		NetLog("[Host Migration]: IsSessionMigrating() passed invalid session handle");
	}

	return sessionMigrating;
}

uint32 CCryMatchMaking::GetUpstreamBPS(void)
{
	return 0;
}

void CCryMatchMaking::SendHostHintInformation(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];

	if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) && (pSession->hostConnectionID != CryMatchMakingInvalidConnectionID))
	{
		return;
	}

	if ((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS | CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST)) == (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS | CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST))
	{
		pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_HOST_HINT_INFO_DIRTY;

		NetLog("[Host Hints]: SendHostHintInformation " PRFORMAT_SH, PRARG_SH(h));

		const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + (SHostHintInformation::PAYLOAD_SIZE) +((CryLobbyPacketUINT32Size + SHostHintInformation::PAYLOAD_SIZE) * MAX_LOBBY_CONNECTIONS);
		uint8 buffer[MaxBufferSize];
		CCrySharedLobbyPacket packet;

		packet.SetWriteBuffer(buffer, MaxBufferSize);
		packet.StartWrite(eLobbyPT_HostHinting, true);

		// Write local hint
		packet.WriteUINT8(pSession->outboundHostHintInfo.natType);
		packet.WriteUINT8(pSession->outboundHostHintInfo.numActiveConnections);
		packet.WriteUINT16((uint16)pSession->outboundHostHintInfo.ping);
		packet.WriteUINT32(pSession->outboundHostHintInfo.upstreamBPS);

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			pSession->hostHintInfo = pSession->outboundHostHintInfo;
			uint32 numConnections = 0;

			// Host writes authoritative, full list of hints
			for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; ++connectionIndex)
			{
				SSession::SRConnection* pConnection = pSession->remoteConnection[connectionIndex];
				if (pConnection->used && (pConnection->connectionID != CryLobbyInvalidConnectionID) && (pConnection->uid.m_uid != DEDICATED_SERVER_CONNECTION_UID))
				{
					packet.WriteUINT32(pConnection->uid.m_uid);
					packet.WriteUINT8(pConnection->hostHintInfo.natType);
					packet.WriteUINT8(pConnection->hostHintInfo.numActiveConnections);
					packet.WriteUINT16((uint16)pConnection->hostHintInfo.ping);
					packet.WriteUINT32(pConnection->hostHintInfo.upstreamBPS);
		#if NET_HOST_HINT_DEBUG
					NetLog("[Host Hints]: Hint: uid %i, NAT %i, %i cx, up %dkbps, %ims", pConnection->uid.m_uid, pConnection->hostHintInfo.natType, pConnection->hostHintInfo.numActiveConnections, pConnection->hostHintInfo.upstreamBPS / 1000, pConnection->hostHintInfo.ping);
		#endif // #if NET_HOST_HINT_DEBUG
					++numConnections;
				}
			}

			{
		#if NET_HOST_HINT_DEBUG
				NetLog("[Host Hints]: sending all hints (%i) for " PRFORMAT_SH " from SERVER to CLIENTS", numConnections + 1, PRARG_SH(h));
		#endif // #if NET_HOST_HINT_DEBUG
				SendToAll(CryMatchMakingInvalidTaskID, &packet, h, CryMatchMakingInvalidConnectionID);
			}
		}
		else
		{

		#if NET_HOST_HINT_DEBUG
			NetLog("[Host Hints]: sending local hint (" PRFORMAT_SH ", " PRFORMAT_UID ", nat type %i->%i, connections %i->%i, up %d->%d, ping %ims->%ims) from CLIENT to SERVER", PRARG_SH(h), PRARG_UID(pSession->localConnection->uid),
			       pSession->hostHintInfo.natType, pSession->outboundHostHintInfo.natType,
			       pSession->hostHintInfo.numActiveConnections, pSession->outboundHostHintInfo.numActiveConnections,
			       pSession->hostHintInfo.upstreamBPS, pSession->outboundHostHintInfo.upstreamBPS,
			       pSession->hostHintInfo.ping, pSession->outboundHostHintInfo.ping);
		#endif // #if NET_HOST_HINT_DEBUG
			Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->hostConnectionID);

			// Prevent further sends until server responds, or host migration starts
		#if NET_HOST_HINT_DEBUG
			NetLog("[Host Hints]: client awaiting server confirmation; unable to send more hints");
		#endif // #if NET_HOST_HINT_DEBUG
			pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
		}
	}
}

//------------------------------------------------------------------------
void CCryMatchMaking::HostHintingOverrideChanged(ICVar* pCVar)
{
	ICryLobby* pLobby = CCryLobby::GetLobby();
	if (pLobby)
	{
		CCryMatchMaking* pMatchMaking = static_cast<CCryMatchMaking*>(pLobby->GetMatchMaking());
		if (pMatchMaking)
		{
			for (uint32 index = 0; index < MAX_MATCHMAKING_SESSIONS; ++index)
			{
		#if NET_HOST_HINT_DEBUG
				NetLog("[Host Hints]: overridden - marking dirty");
		#endif // #if NET_HOST_HINT_DEBUG
				pMatchMaking->MarkHostHintInformationDirty(index);
			}
		}
	}
}
	#endif

void CCryMatchMaking::SendServerPing(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];

	if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_CAN_SEND_SERVER_PING))
	{
		const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize + ((CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size) * MAX_LOBBY_CONNECTIONS);
		uint8 buffer[MaxBufferSize];
		CCrySharedLobbyPacket packet;

		packet.SetWriteBuffer(buffer, MaxBufferSize);
		packet.StartWrite(eLobbyPT_ServerPing, false);
		bool send = false;

		for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; ++connectionIndex)
		{
			SSession::SRConnection* pConnection = pSession->remoteConnection[connectionIndex];
			if (pConnection->used && (pConnection->connectionID != CryLobbyInvalidConnectionID))
			{
				pConnection->pingToServer = m_lobby->GetConnectionPing(pConnection->connectionID);
				packet.WriteUINT32(pConnection->uid.m_uid);
				packet.WriteUINT16(pConnection->pingToServer);
	#if defined(LOG_SERVER_PING_INFO)
				NetLog("[Server Ping]: SENDING " PRFORMAT_SHMMCINFO ", ping %ims", PRARG_SHMMCINFO(h, CryMatchMakingConnectionID(connectionIndex), pConnection->connectionID, pConnection->uid), pConnection->pingToServer);
	#endif
				send = true;
			}
		}

		if (send)
		{
			SendToAll(CryMatchMakingInvalidTaskID, &packet, h, CryMatchMakingInvalidConnectionID);
		}
	}
}

void CCryMatchMaking::ProcessServerPing(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryMatchMakingConnectionID id;
	CryLobbySessionHandle handle = CryLobbyInvalidSessionHandle;
	bool hintsChanged = false;

	pPacket->StartRead();
	SCryMatchMakingConnectionUID uid = pPacket->GetFromConnectionUID();
	if (FindConnectionFromUID(uid, &handle, &id))
	{
		SSession* pSession = m_sessions[handle];
		SSession::SRConnection* pConnection = pSession->remoteConnection[id];

		if (pSession->hostConnectionID == id)
		{
			pConnection->pingToServer = 0; // Server's ping to himself

			const uint32 itemCount = (pPacket->GetReadBufferSize() - CryLobbyPacketUnReliableHeaderSize) / (CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size);
			for (uint32 itemIndex = 0; itemIndex < itemCount; ++itemIndex)
			{
				uid.m_uid = pPacket->ReadUINT32();
				uint16 pingToServer = pPacket->ReadUINT16();
	#if defined(LOG_SERVER_PING_INFO)
				NetLog("[Server Ping]: RECEIVED " PRFORMAT_SH ", " PRFORMAT_UID ", ping %ims", PRARG_SH(handle), PRARG_UID(uid), pingToServer);
	#endif

				if (FindConnectionFromSessionUID(handle, uid, &id))
				{
					pConnection = pSession->remoteConnection[id];
					pConnection->pingToServer = pingToServer;
				}
				else
				{
					if (pSession->localConnection->uid == uid)
					{
						pSession->localConnection->pingToServer = pingToServer;
					}
					else
					{
						NetLog("[Server Ping]: ping received for unknown " PRFORMAT_UID, PRARG_UID(uid));
					}
				}
			}
		}
	}
}

void CCryMatchMaking::DispatchSessionUserDataEvent(ECryLobbySystemEvent event, TMemHdl mh)
{
	UCryLobbyEventData eventData;

	eventData.pSessionUserData = (SCryLobbySessionUserData*)m_lobby->MemGetPtr(mh);
	m_lobby->DispatchEvent(event, eventData);
	eventData.pSessionUserData->data.m_userID = NULL;
	m_lobby->MemFree(mh);
}

void CCryMatchMaking::SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, SCryUserInfoResult* pUserInfo)
{
	SSession* pSession = m_sessions[h];

	CRY_ASSERT_MESSAGE(pSession, "CCryMatchMaking: Session base pointers not setup");
	CRY_ASSERT_MESSAGE(pSession->localConnection, "CCryMatchMaking: Local connection base pointer not setup");

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED)
	{
		SSession::SRConnection* pRConnection = NULL;
		CryMatchMakingConnectionID id;

		if (FindConnectionFromSessionUID(h, pUserInfo->m_conID, &id))
		{
			pRConnection = pSession->remoteConnection[id];
		}

		// Dispatch event to game if
		if (!pRConnection ||                                                                           // The event is for our local connection.
		    (pRConnection &&                                                                           // The event if for a remote connection and
		     ((event == eCLSE_SessionUserJoin) && !pRConnection->gameInformedConnectionEstablished) || // The event is a join and a join hasn't been dispatched.
		     ((event != eCLSE_SessionUserJoin) && pRConnection->gameInformedConnectionEstablished)))   // The event is not for a join and a join has been dispatched.
		{
			TMemHdl mh = m_lobby->MemAlloc(sizeof(SCryLobbySessionUserData));

			if (mh != TMemInvalidHdl)
			{
				SCryLobbySessionUserData* pData = (SCryLobbySessionUserData*)m_lobby->MemGetPtr(mh);

				pData->data.m_userID.userID.ReleaseOwnership();
				pData->session = CreateGameSessionHandle(h, pSession->localConnection->uid);
				pData->data = *pUserInfo;

				TO_GAME_FROM_LOBBY(&CCryMatchMaking::DispatchSessionUserDataEvent, this, event, mh);

				if (pRConnection && (event == eCLSE_SessionUserJoin))
				{
	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
					if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) && (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID))
					{
						SendServerInfo(h, id, CryMatchMakingInvalidTaskID, NULL);
					}
	#endif
					// Now the game has been told about this connection dispatch any user packets we have queued for it.
					InitialDispatchUserPackets(h, id);
					pRConnection->gameInformedConnectionEstablished = true;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Host Migration
void CCryMatchMaking::TerminateHostMigration(CrySessionHandle gh)
{
	#if NETWORK_HOST_MIGRATION
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = m_sessions[h];

		m_hostMigration.Terminate(&pSession->hostMigrationInfo);
	}
	#endif
}

eHostMigrationState CCryMatchMaking::GetSessionHostMigrationState(CrySessionHandle gh)
{
	eHostMigrationState state = eHMS_Idle;

	#if NETWORK_HOST_MIGRATION
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = m_sessions[h];
		state = (eHostMigrationState)(pSession->hostMigrationInfo.m_state);
	}
	#endif

	return state;
}

ECryLobbyError CCryMatchMaking::GetSessionPlayerPing(SCryMatchMakingConnectionUID uid, CryPing* const pPing)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError result = eCLE_InvalidParam;

	CRY_ASSERT_MESSAGE(pPing, "CCryMatchMaking::GetSessionPlayerPing: NULL pPing ");

	if (pPing)
	{
		CryPing ping = CRYLOBBY_INVALID_PING;
		uint32 localSessionIndex;
		uint32 remoteConnectionIndex = 0;

		for (localSessionIndex = 0; localSessionIndex < MAX_MATCHMAKING_SESSIONS; ++localSessionIndex)
		{
			SSession* pSession = m_sessions[localSessionIndex];

			if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (GetSIDFromSessionHandle(localSessionIndex) == uid.m_sid))
			{
				SSession::SLConnection* pLConnection = pSession->localConnection;

				if (pLConnection->used && (pLConnection->uid.m_uid == uid.m_uid))
				{
					if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
					{
						ping = 0;
					}
					else
					{
						ping = pLConnection->pingToServer;
					}
				}
				else
				{
					for (remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
					{
						SSession::SRConnection* pRConnection = pSession->remoteConnection[remoteConnectionIndex];

						if (pRConnection->used && (pRConnection->uid.m_uid == uid.m_uid))
						{
							ping = pRConnection->pingToServer;
							break;
						}
					}
				}

				break;
			}
		}

		if ((localSessionIndex < MAX_MATCHMAKING_SESSIONS) && (remoteConnectionIndex < MAX_LOBBY_CONNECTIONS))
		{
			if (ping != CRYLOBBY_INVALID_PING)
			{
				result = eCLE_Success;
			}
			else
			{
				result = eCLE_InvalidPing;
			}
		}
		else
		{
			result = eCLE_InvalidUser;
		}

		*pPing = ping;
	}

	return result;
}

void CCryMatchMaking::DispatchSessionEvent(CrySessionHandle h, ECryLobbySystemEvent event)
{
	UCryLobbyEventData data;
	SCryLobbySessionEventData sessionEventData;
	data.pSessionEventData = &sessionEventData;

	sessionEventData.session = h;
	m_lobby->DispatchEvent(event, data);
}

	#if NETWORK_HOST_MIGRATION
void CCryMatchMaking::HostMigrationStoreNetContextState(INetContextState* pNetContextState)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];
		if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && pSession->hostMigrationInfo.ShouldMigrateNub())
		{
			pSession->hostMigrationInfo.m_storedContextState = pNetContextState;
			break;
		}
	}
}

INetContextState* CCryMatchMaking::HostMigrationGetNetContextState()
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];
		if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && pSession->hostMigrationInfo.ShouldMigrateNub())
		{
			return pSession->hostMigrationInfo.m_storedContextState;
		}
	}
	return NULL;
}

void CCryMatchMaking::HostMigrationResetNetContextState()
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];
		if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && pSession->hostMigrationInfo.ShouldMigrateNub())
		{
			pSession->hostMigrationInfo.m_storedContextState = NULL;
		}
	}
}

bool CCryMatchMaking::IsNubSessionMigrating() const
{
	LOBBY_AUTO_LOCK;

	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = m_sessions[i];
		if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && pSession->hostMigrationInfo.ShouldMigrateNub())
		{
			return (pSession->hostMigrationInfo.m_state != eHMS_Idle);
		}
	}
	return false;
}

bool CCryMatchMaking::IsNubSessionMigratable() const
{
	LOBBY_AUTO_LOCK;

	if (CLobbyCVars::Get().netAutoMigrateHost != 0)
	{
		CryLobbySessionHandle h = GetCurrentNetNubSessionHandle();
		if (h != CryLobbyInvalidSessionHandle)
		{
			SSession* pSession = m_sessions[h];
			if ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE))
			{
				return true;
			}
		}
	}

	return false;
}

void CCryMatchMaking::OnReconnectClient(SHostMigrationInfo& hostMigrationInfo)
{
	CryLobbySessionHandle sessionIndex = GetSessionHandleFromGameSessionHandle(hostMigrationInfo.m_session);
	SSession* pSession = m_sessions[sessionIndex];
	pSession->localConnection->pingToServer = CRYLOBBY_INVALID_PING;
	pSession->localConnection->used = true;
}

bool CCryMatchMaking::IsHostMigrationFinished(CryLobbySessionHandle h)
{
	SSession* pSession = m_sessions[h];
	SHostMigrationInfo_Private* pInfo = &pSession->hostMigrationInfo;

	if (pInfo->m_matchMakingFinished)
	{
		if (pInfo->ShouldMigrateNub())
		{
			INetNubPrivate* pNub = (INetNubPrivate*)gEnv->pGameFramework->GetServerNetNub();

			if (pNub)
			{
				// Check that all the connections we still have also have a nub channel and the nub channel is in game.
				for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; ++i)
				{
					SSession::SRConnection* pRConnection = pSession->remoteConnection[i];

					if (pRConnection->used)
					{
						INetChannel* pChannel = pNub->GetChannelFromSessionHandle(pSession->remoteConnection[i]->uid.m_uid);

						if (!pChannel)
						{
							return false;
						}

						if (pChannel->IsMigratingChannel() && (pChannel->GetContextViewState() < eCVS_InGame))
						{
							return false;
						}
					}
				}
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}
	#endif

uint32 CCryMatchMaking::GetTimeSincePacketFromServerMS(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	uint32 timeSincePacketMS = 0;

	if (m_lobby)
	{
		CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

		if (h != CryLobbyInvalidSessionHandle)
		{
			SSession* pSession = m_sessions[h];

			if (pSession)
			{
				if (pSession->hostConnectionID != CryMatchMakingInvalidConnectionID)
				{
					SSession::SRConnection* pConnection = pSession->remoteConnection[pSession->hostConnectionID];
					timeSincePacketMS = m_lobby->TimeSincePacketInMS(pConnection->connectionID);
				}
			}
		}
	}
	return timeSincePacketMS;
}

void CCryMatchMaking::DisconnectFromServer(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	if (m_lobby)
	{
		CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

		if (h != CryLobbyInvalidSessionHandle)
		{
			SSession* pSession = m_sessions[h];

			if (pSession)
			{
				if (pSession->hostConnectionID != CryMatchMakingInvalidConnectionID)
				{
					SSession::SRConnection* pConnection = pSession->remoteConnection[pSession->hostConnectionID];
					m_lobby->ForceTimeoutConnection(pConnection->connectionID);
				}
			}
		}
	}
}

void CCryMatchMaking::StatusCmd(eStatusCmdMode mode)
{
	LOBBY_AUTO_LOCK;

	CALL_DEDICATED_SERVER_VERSION(StatusCmd(mode));

	SSession* pSession = NULL;
	SSession::SRConnection* pConnection = NULL;
	uint32 sessionIndex = 0;
	char ip[32];

	switch (mode)
	{
	case eSCM_AllSessions:
		for (sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
		{
			pSession = m_sessions[sessionIndex];

			if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
			{
				CryLogAlways("------------------------------------------------------------");
				CryLogAlways(" Session: %u (%s)", sessionIndex, (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) ? "SERVER" : "CLIENT");

				if ((CLobbyCVars::Get().fullStatusOnClient != 0) || (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
				{
					CryLogAlways("   LCxID UserID           Name");

					for (uint32 remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
					{
						pConnection = pSession->remoteConnection[remoteConnectionIndex];

						if (pConnection->used)
						{
							char host = ' ';
							if (pSession->hostConnectionID != CryMatchMakingInvalidConnectionID)
							{
								if (pConnection == pSession->remoteConnection[pSession->hostConnectionID])
								{
									host = '*';
								}
							}
							CryLogAlways(" %c   %02u  %-16llu %s", host, pConnection->connectionID.GetID(), GetConnectionUserID(pConnection, 0), GetConnectionName(pConnection, 0));
						}
					}
				}
			}
		}
		break;
	case eSCM_LegacyRConCompatabilityConnectionsOnly:
		for (sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
		{
			pSession = m_sessions[sessionIndex];
			if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB))
			{
				if ((CLobbyCVars::Get().fullStatusOnClient != 0) || (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
				{
					for (uint32 remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
					{
						pConnection = pSession->remoteConnection[remoteConnectionIndex];

						if (pConnection->used)
						{
							m_lobby->DecodeAddress(*(m_lobby->GetNetAddress(pConnection->connectionID)), ip, false);
							CryLogAlways("name: %s  id: %u  ip: %s  ping: %u  state: %d profile: %" PRIu64, GetConnectionName(pConnection, 0), pConnection->connectionID.GetID(), ip, m_lobby->GetConnectionPing(pConnection->connectionID), m_lobby->ConnectionGetState(pConnection->connectionID) + 1, GetConnectionUserID(pConnection, 0));
						}
					}
				}
			}
		}
		break;
	case eSCM_LegacyRConCompatabilityNoNub:
		for (sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
		{
			pSession = m_sessions[sessionIndex];
			if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_NUB))
			{
				const char* pOwner = (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) ? "Server" : "Client";

				CryLogAlways("-----------------------------------------");
				CryLogAlways("%s Status:", pOwner);
				CryLogAlways("name: %s", pSession->GetLocalUserName(0));
				CryLogAlways("ip: %s", gEnv->pNetwork->GetHostName());
				char myVersion[PRODUCT_VERSION_SIZE];
				gEnv->pSystem->GetProductVersion().ToString(myVersion);
				CryLogAlways("version: %s", myVersion);

				CryLogAlways("level: lobby");
				CryLogAlways("gamerules: unknown");
				uint players = 0;
				for (uint32 remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
				{
					pConnection = pSession->remoteConnection[remoteConnectionIndex];
					if (pConnection->used)
					{
						++players;
					}
				}
				CryLogAlways("players: %u/16", players);
				CryLogAlways("time remaining: 0:00");

				CryLogAlways("\n-----------------------------------------");
				CryLogAlways("Connection Status:");

				if ((CLobbyCVars::Get().fullStatusOnClient != 0) || (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
				{
					for (uint32 remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
					{
						pConnection = pSession->remoteConnection[remoteConnectionIndex];

						if (pConnection->used)
						{
							m_lobby->DecodeAddress(*(m_lobby->GetNetAddress(pConnection->connectionID)), ip, false);
							CryLogAlways("name: %s  id: %u  ip: %s  ping: %u  state: %d profile: %" PRIu64, GetConnectionName(pConnection, 0), pConnection->connectionID.GetID(), ip, m_lobby->GetConnectionPing(pConnection->connectionID), m_lobby->ConnectionGetState(pConnection->connectionID) + 1, GetConnectionUserID(pConnection, 0));
						}
					}
				}
			}
		}
		break;
	default:
		break;
	}
}

void CCryMatchMaking::KickCmd(uint32 cxID, uint64 userID, const char* pName, EDisconnectionCause cause)
{
	CryLobbyConnectionID connectionID;

	if (cxID < MAX_LOBBY_CONNECTIONS)
	{
		connectionID = cxID;
	}

	KickCmd(connectionID, userID, pName, cause);
}

void CCryMatchMaking::KickCmd(CryLobbyConnectionID cxID, uint64 userID, const char* pName, EDisconnectionCause cause)
{
	LOBBY_AUTO_LOCK;

	SSession* pSession = NULL;
	SSession::SRConnection* pConnection = NULL;
	bool kicked = false;

	for (uint32 sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
	{
		pSession = m_sessions[sessionIndex];

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			for (uint32 remoteConnectionIndex = 0; remoteConnectionIndex < MAX_LOBBY_CONNECTIONS; ++remoteConnectionIndex)
			{
				pConnection = pSession->remoteConnection[remoteConnectionIndex];

				if (pConnection->used)
				{
					uint64 connectionUserID = GetConnectionUserID(pConnection, 0);
					const char* connectionName = GetConnectionName(pConnection, 0);

					if ((cxID == pConnection->connectionID) || ((userID != 0) && (userID == connectionUserID)) || ((pName != NULL) && (connectionName != NULL) && (_stricmp(pName, connectionName) == 0)))
					{
						CryLogAlways("Kicked %s (cx:%u id:%llx)", connectionName, pConnection->connectionID.GetID(), connectionUserID);
						SessionDisconnectRemoteConnectionViaNub(sessionIndex, remoteConnectionIndex, eDS_Local, CryMatchMakingInvalidConnectionID, cause, "Kicked from server");
						kicked = true;
						return;
					}
				}
			}
		}
	}

	if (!kicked)
	{
		CryLogAlways("Unable to find player with name '%s', cx %u or user id %llx", pName, cxID.GetID(), userID);
	}
}

bool CCryMatchMaking::ExpireBans(TBannedUser& bannedUsers, const CTimeValue& tv)
{
	bool result = false;

	if (!bannedUsers.empty())
	{
		TBannedUser::iterator it = bannedUsers.begin();
		TBannedUser::const_reverse_iterator rbegin = bannedUsers.rbegin();
		TBannedUser::const_reverse_iterator rit = rbegin;
		bool done = false;

		do
		{
			done = (&*it == &*rit);

			if (!it->m_permanent && (tv >= it->m_bannedUntil))
			{
				*it = *rit;
				++rit;
			}
			else
			{
				++it;
			}
		}
		while (!done);

		if (rit != rbegin)
		{
			bannedUsers.resize(bannedUsers.size() - (rit - rbegin));
			result = true;
		}
	}

	return result;
}

	#if NETWORK_HOST_MIGRATION

void CCryMatchMaking::SHostHintInformation::Reset(void)
{
	natType = eNT_Unknown;
	numActiveConnections = 0;
	ping = CRYLOBBY_INVALID_PING;
	upstreamBPS = 0;
	lastSendInternal = 0LL;
	lastSendExternal = 0LL;
}

bool CCryMatchMaking::SHostHintInformation::SufficientlyDifferent(const SHostHintInformation& other, CTimeValue currentTime)
{
	if ((natType != other.natType) || (numActiveConnections != other.numActiveConnections) || (upstreamBPS != other.upstreamBPS))
	{
		return true;
	}
	if (abs(ping - other.ping) > PING_LARGE_SIMILARITY_THRESHOLD)
	{
		return true;
	}
	int64 timeSinceLastSend = (currentTime.GetMilliSecondsAsInt64() - lastSendInternal.GetMilliSecondsAsInt64());
	if (timeSinceLastSend > TIME_SIMILARITY_THRESHOLD)
	{
		if (abs(ping - other.ping) > PING_SMALL_SIMILARITY_THRESHOLD)
		{
			return true;
		}
	}
	return false;
}

bool CCryMatchMaking::SHostHintInformation::operator==(const SHostHintInformation& other) const
{
	return (natType == other.natType) && (numActiveConnections == other.numActiveConnections) && (ping == other.ping);
}

bool CCryMatchMaking::SHostHintInformation::operator!=(const SHostHintInformation& other) const
{
	return !(*this == other);
}

	#endif

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
ECryLobbyError CCryMatchMaking::SessionSetupDedicatedServer(CrySessionHandle gh, CCryLobbyPacket* pPacket, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSetupDedicatedServerCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	CryMatchMakingTaskID mmTaskID;
	ECryLobbyError error = StartTask(eT_SessionSetupDedicatedServer, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

	if (error == eCLE_Success)
	{
		STask* pTask = m_task[mmTaskID];
		uint32 bufferEndPos = pPacket->GetReadBufferPos();
		uint32 userPacketType = pPacket->StartRead();
		uint32 bufferStartPos = pPacket->GetReadBufferPos();
		uint32 userDataSize = bufferEndPos - bufferStartPos;
		uint32 writePacketSize = CryLobbyPacketUnReliableHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size + CryLobbyPacketUINT8Size + CryLobbyPacketUINT16Size + userDataSize;
		CCryLobbyPacket packet;

		if (packet.CreateWriteBuffer(writePacketSize))
		{
			SCryMatchMakingConnectionUID uid;

			uid.m_sid = GetSIDFromSessionHandle(h);
			uid.m_uid = DEDICATED_SERVER_CONNECTION_UID;

			pTask->numParams[TDN_SETUP_DEDICATED_SERVER_COOKIE] = uint32(gEnv->pTimer->GetAsyncTime().GetValue());

			packet.StartWrite(eLobbyPT_C2A_RequestSetupDedicatedServer, false);
			packet.WriteConnectionUID(uid);
			packet.WriteUINT32(pTask->numParams[TDN_SETUP_DEDICATED_SERVER_COOKIE]);
			packet.WriteUINT8(mmTaskID);
			packet.WriteUINT8(userPacketType);
			packet.WriteUINT16(userDataSize);

			if (userDataSize > 0)
			{
				packet.WriteData(pPacket->GetReadBuffer() + bufferStartPos, userDataSize);
			}

			pTask->params[TDM_SETUP_DEDICATED_SERVER_PACKET] = PacketToMemoryBlock(&packet, pTask->numParams[TDN_SETUP_DEDICATED_SERVER_PACKET_SIZE]);

			if (pTask->params[TDM_SETUP_DEDICATED_SERVER_PACKET] != TMemInvalidHdl)
			{
				FROM_GAME_TO_LOBBY(&CCryMatchMaking::StartTaskRunning, this, mmTaskID);
			}
			else
			{
				error = eCLE_OutOfMemory;
			}

			packet.FreeWriteBuffer();
		}
		else
		{
			error = eCLE_OutOfMemory;
		}

		if (error != eCLE_Success)
		{
			FreeTask(mmTaskID);
		}
	}

	NetLog("[Lobby] Start SessionSetupDedicatedServer error %d", error);

	return error;
}

void CCryMatchMaking::StartSessionSetupDedicatedServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];
	CNetAddressResolver* pResolver = CCryLobby::GetResolver();
	CLobbyCVars& cvars = CLobbyCVars::Get();

	if (pResolver)
	{
		string address;

		if (cvars.pDedicatedServerArbitratorIP && cvars.pDedicatedServerArbitratorIP->GetString())
		{
			UpdateTaskError(mmTaskID, CreateTaskParam(mmTaskID, TDM_SETUP_DEDICATED_SERVER_NAME_REQUESTER, NULL, 0, sizeof(CNameRequestPtr)));

			if (pTask->error == eCLE_Success)
			{
				CNameRequestPtr* ppReq = (CNameRequestPtr*)m_lobby->MemGetPtr(pTask->params[TDM_SETUP_DEDICATED_SERVER_NAME_REQUESTER]);

				StartSubTask(eT_SessionSetupDedicatedServerNameResolve, mmTaskID);

				ppReq->ReleaseOwnership();

				address.Format("%s:%d", cvars.pDedicatedServerArbitratorIP->GetString(), cvars.dedicatedServerArbitratorPort);

				NetLog("Starting name resolution for dedicated server arbitrator '%s'", address.c_str());
				*ppReq = pResolver->RequestNameLookup(address);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_InternalError);
		}
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_InternalError);
	}

	if (pTask->error != eCLE_Success)
	{
		CNameRequestPtr* ppReq = (CNameRequestPtr*)m_lobby->MemGetPtr(pTask->params[TDM_SETUP_DEDICATED_SERVER_NAME_REQUESTER]);

		if (ppReq)
		{
			(*ppReq) = NULL;
		}

		StopTaskRunning(mmTaskID);
	}
}

void CCryMatchMaking::TickSessionSetupDedicatedServerNameResolve(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];
	CNameRequestPtr* ppReq = (CNameRequestPtr*)m_lobby->MemGetPtr(pTask->params[TDM_SETUP_DEDICATED_SERVER_NAME_REQUESTER]);
	ENameRequestResult result = (*ppReq)->GetResult();
	CLobbyCVars& cvars = CLobbyCVars::Get();

	switch (result)
	{
	case eNRR_Pending:
		break;

	case eNRR_Succeeded:
		NetLog("Name resolution for dedicated server arbitrator '%s:%d' succeeded", cvars.pDedicatedServerArbitratorIP->GetString(), cvars.dedicatedServerArbitratorPort);
		StartSubTask(eT_SessionSetupDedicatedServer, mmTaskID);
		m_arbitratorAddr = (*ppReq)->GetAddrs()[0];
		m_lobby->MakeAddrPCCompatible(m_arbitratorAddr);
		(*ppReq) = NULL;
		break;

	case eNRR_Failed:
		NetLog("Name resolution for dedicated server arbitrator '%s:%d' failed", cvars.pDedicatedServerArbitratorIP->GetString(), cvars.dedicatedServerArbitratorPort);
		UpdateTaskError(mmTaskID, eCLE_InternalError);
		StopTaskRunning(mmTaskID);
		(*ppReq) = NULL;
		break;
	}
}

void CCryMatchMaking::TickSessionSetupDedicatedServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	if (!pTask->TimerStarted() || (pTask->GetTimer() > SETUP_DEDICATED_SERVER_SEND_INTERVAL))
	{
		if (pTask->numParams[TDN_SETUP_DEDICATED_SERVER_SEND_COUNTER] < SETUP_DEDICATED_SERVER_MAX_SENDS)
		{
			CCryLobbyPacket* pPacket = MemoryBlockToPacket(pTask->params[TDM_SETUP_DEDICATED_SERVER_PACKET], pTask->numParams[TDN_SETUP_DEDICATED_SERVER_PACKET_SIZE]);

			pTask->numParams[TDN_SETUP_DEDICATED_SERVER_SEND_COUNTER]++;
			pTask->StartTimer();

			if (m_lobby->Send(pPacket, m_arbitratorAddr, CryLobbyInvalidConnectionID, NULL) != eSE_Ok)
			{
				UpdateTaskError(mmTaskID, eCLE_InternalError);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_NoServerAvailable);
			StopTaskRunning(mmTaskID);
		}
	}
}

void CCryMatchMaking::EndSessionSetupDedicatedServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		SSession* pSession = m_sessions[pTask->session];

		if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
		{
			const TNetAddress* pServerAddr = m_lobby->GetNetAddress(pSession->remoteConnection[pSession->serverConnectionID]->connectionID);

			if (pServerAddr)
			{
				const SIPv4Addr* pIPv4Addr = stl::get_if<SIPv4Addr>(pServerAddr);

				if (pIPv4Addr)
				{
					((CryMatchmakingSessionSetupDedicatedServerCallback)pTask->cb)(pTask->lTaskID, pTask->error, htonl(pIPv4Addr->addr), pIPv4Addr->port, pTask->cbArg);
				}

				return;
			}
		}
	}

	((CryMatchmakingSessionSetupDedicatedServerCallback)pTask->cb)(pTask->lTaskID, pTask->error, 0, 0, pTask->cbArg);
}

void CCryMatchMaking::SendServerInfo(CryLobbySessionHandle h, CryMatchMakingConnectionID connectionID, CryMatchMakingTaskID mmTaskID, CryLobbySendID sendIDs[MAX_LOBBY_CONNECTIONS])
{
	SSession* pSession = m_sessions[h];
	const uint32 MaxBufferSize = CryLobbyPacketReliableHeaderSize + CryLobbyPacketBoolSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size;
	uint8 buffer[MaxBufferSize];
	CCrySharedLobbyPacket packet;
	const SIPv4Addr* pIPv4Addr = NULL;
	const TNetAddress* pServerAddr = NULL;

	if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
	{
		pServerAddr = m_lobby->GetNetAddress(pSession->remoteConnection[pSession->serverConnectionID]->connectionID);

		if (pServerAddr)
		{
			pIPv4Addr = stl::get_if<SIPv4Addr>(pServerAddr);
		}
	}

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eLobbyPT_SessionServerInfo, true);

	if (pIPv4Addr)
	{
		NetLog("[Lobby] SendServerInfo " PRFORMAT_SH " " PRFORMAT_ADDR, PRARG_SH(h), PRARG_ADDR(*pServerAddr));
		packet.WriteBool(true);
		packet.WriteConnectionUID(pSession->remoteConnection[pSession->serverConnectionID]->uid);
		packet.WriteUINT32(pIPv4Addr->addr);
		packet.WriteUINT16(pIPv4Addr->port);
	}
	else
	{
		NetLog("[Lobby] SendServerInfo " PRFORMAT_SH " No server", PRARG_SH(h));
		packet.WriteBool(false);
	}

	if (connectionID == CryMatchMakingInvalidConnectionID)
	{
		if ((mmTaskID != CryMatchMakingInvalidTaskID) && sendIDs)
		{
			STask* pTask = m_task[mmTaskID];

			for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
			{
				SSession::SRConnection* pConnection = pSession->remoteConnection[i];

				if (pConnection->used && (i != pSession->serverConnectionID))
				{
					Send(mmTaskID, &packet, h, CryMatchMakingConnectionID(i));
					sendIDs[i] = pTask->sendID;
				}
				else
				{
					sendIDs[i] = CryLobbyInvalidSendID;
				}
			}
		}
		else
		{
			SendToAll(CryMatchMakingInvalidTaskID, &packet, h, pSession->serverConnectionID);
		}
	}
	else
	{
		Send(CryMatchMakingInvalidTaskID, &packet, h, connectionID);
	}
}

void CCryMatchMaking::ProcessSessionServerInfo(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryMatchMakingConnectionID id;
	CryLobbySessionHandle h;

	pPacket->StartRead();

	SCryMatchMakingConnectionUID fromUID = pPacket->GetFromConnectionUID();

	if (FindConnectionFromUID(fromUID, &h, &id))
	{
		SSession* pSession = m_sessions[h];
		bool haveAddr = pPacket->ReadBool();

		if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
		{
			CryMatchMakingConnectionID serverConnectionID = pSession->serverConnectionID;

			NetLog("ProcessSessionServerInfo Free current server connection " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, pSession->serverConnectionID, pSession->remoteConnection[pSession->serverConnectionID]->connectionID, pSession->remoteConnection[pSession->serverConnectionID]->uid));
			pSession->serverConnectionID = CryMatchMakingInvalidConnectionID;
			SessionDisconnectRemoteConnectionViaNub(h, serverConnectionID, eDS_Remote, CryMatchMakingInvalidConnectionID, eDC_Unknown, "ProcessSessionServerInfo Disconnect Server");
		}

		if (haveAddr)
		{
			SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
			uint32 ip = pPacket->ReadUINT32();
			uint16 port = pPacket->ReadUINT16();

			NetLog("ProcessSessionServerInfo Received new server info " PRFORMAT_SH " " PRFORMAT_ADDR " " PRFORMAT_UID, PRARG_SH(h), PRARG_ADDR(TNetAddress(SIPv4Addr(ip, port))), PRARG_UID(uid));
			SessionConnectToServer(h, TNetAddress(SIPv4Addr(ip, port)), uid);
		}
	}
}

void CCryMatchMaking::ProcessRequestSetupDedicatedServerResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();
	bool haveServer = pPacket->ReadBool();
	CryMatchMakingTaskID mmTaskID = pPacket->ReadUINT8();

	mmTaskID = FindTaskFromTaskTaskID(eT_SessionSetupDedicatedServer, mmTaskID);

	if (mmTaskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = m_task[mmTaskID];

		uint32 cookie = pPacket->ReadUINT32();

		if (cookie == pTask->numParams[TDN_SETUP_DEDICATED_SERVER_COOKIE])
		{
			if (haveServer)
			{
				SSession* pSession = m_sessions[pTask->session];
				SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
				uint32 ip = pPacket->ReadUINT32();
				uint16 port = pPacket->ReadUINT16();
				TNetAddress serverAddr(SIPv4Addr(ip, port));

				NetLog("ProcessRequestSetupDedicatedServerResult: Got dedicated server address " PRFORMAT_ADDR, PRARG_ADDR(serverAddr));
				StartSubTask(eT_SessionConnectToServer, mmTaskID);
				SessionConnectToServer(pTask->session, serverAddr, uid);

				if (pSession->serverConnectionID == CryMatchMakingInvalidConnectionID)
				{
					UpdateTaskError(mmTaskID, eCLE_NoServerAvailable);
					StartSessionReleaseDedicatedServer(mmTaskID);
				}
			}
			else
			{
				NetLog("ProcessRequestSetupDedicatedServerResult: No dedicated servers available");
				UpdateTaskError(mmTaskID, eCLE_NoServerAvailable);
				StopTaskRunning(mmTaskID);
			}
		}
	}
}

void CCryMatchMaking::SessionConnectToServer(CryLobbySessionHandle h, const TNetAddress& addr, SCryMatchMakingConnectionUID uid)
{
	SSession* pSession = m_sessions[h];
	CryLobbyConnectionID connectionID;

	connectionID = m_lobby->FindConnection(addr);

	if (connectionID == CryLobbyInvalidConnectionID)
	{
		connectionID = m_lobby->CreateConnection(addr);
	}

	if (connectionID != CryLobbyInvalidConnectionID)
	{
		m_lobby->MakeConnectionPCCompatible(connectionID);
		pSession->serverConnectionID = AddRemoteConnection(h, connectionID, uid, 0);

		if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
		{
			const uint32 MaxBufferSize = CryLobbyPacketReliableHeaderSize + CryLobbyPacketBoolSize;
			uint8 buffer[MaxBufferSize];
			CCrySharedLobbyPacket packet;

			packet.SetWriteBuffer(buffer, MaxBufferSize);
			packet.StartWrite(eLobbyPT_SessionRequestJoinServer, true);

			if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
			{
				packet.WriteBool(true);
			}
			else
			{
				packet.WriteBool(false);
			}

			Send(CryMatchMakingInvalidTaskID, &packet, h, pSession->serverConnectionID);
		}
	}
}

void CCryMatchMaking::ProcessSessionRequestJoinServerResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();
	bool accepted = pPacket->ReadBool();
	SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
	CryLobbySessionHandle h;

	if (FindLocalConnectionFromUID(uid, &h))
	{
		SSession* pSession = m_sessions[h];

		if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
		{
			CryMatchMakingTaskID mmTaskID = FindTaskFromTaskSessionHandle(eT_SessionConnectToServer, h);

			if (mmTaskID != CryMatchMakingInvalidTaskID)
			{
				STask* pTask = m_task[mmTaskID];

				if (accepted)
				{
					NetLog("[Lobby] ProcessSessionRequestJoinServerResult connection accepted");
					StopTaskRunning(mmTaskID);
					SendServerInfo(pTask->session, CryMatchMakingInvalidConnectionID, CryMatchMakingInvalidTaskID, NULL);
		#if NETWORK_HOST_MIGRATION
					if ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) == (CRYSESSION_CREATE_FLAG_MIGRATABLE))
					{
						MarkHostHintInformationDirty(h);
						pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST;
					}
		#endif
				}
				else
				{
					NetLog("[Lobby] ProcessSessionRequestJoinServerResult connection refused");
					UpdateTaskError(mmTaskID, eCLE_NoServerAvailable);
					StartSessionReleaseDedicatedServer(mmTaskID);
				}
			}
			else
			{
				if (accepted)
				{
					NetLog("[Lobby] ProcessSessionRequestJoinServerResult connection accepted");

		#if NETWORK_HOST_MIGRATION
					if ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) == (CRYSESSION_CREATE_FLAG_MIGRATABLE))
					{
						pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST_MIGRATION_CAN_BE_HOST;
					}
		#endif
				}
				else
				{
					NetLog("[Lobby] ProcessSessionRequestJoinServerResult connection refused");
					SessionDisconnectRemoteConnectionViaNub(h, pSession->serverConnectionID, eDS_Remote, CryMatchMakingInvalidConnectionID, eDC_Unknown, "ProcessSessionRequestJoinServerResult: Disconnect Server");
				}
			}
		}
	}
}

void CCryMatchMaking::TickSessionConnectToServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	if (pTask->sendStatus != eCLE_Pending)
	{
		if (pTask->sendStatus == eCLE_Success)
		{
			// The request to join has been sent so wait for result
			if (pTask->TimerStarted())
			{
				if (pTask->GetTimer() > CryMatchMakingConnectionTimeOut)
				{
					// No response so fail connection attempt
					NetLog("[Lobby] SessionConnectToServer request session connect to server packet sent no response received");
					UpdateTaskError(mmTaskID, eCLE_NoServerAvailable);
					StartSessionReleaseDedicatedServer(mmTaskID);
				}
			}
			else
			{
				pTask->StartTimer();
				NetLog("[Lobby] SessionConnectToServer request session connect to server packet sent waiting for response");
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_NoServerAvailable);
			NetLog("[Lobby] SessionConnectToServer error sending request session connect to server packet error %d", pTask->error);
			StartSessionReleaseDedicatedServer(mmTaskID);
		}
	}
}

ECryLobbyError CCryMatchMaking::SessionReleaseDedicatedServer(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	CryMatchMakingTaskID mmTaskID;
	ECryLobbyError error = StartTask(eT_SessionReleaseDedicatedServer, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

	if (error == eCLE_Success)
	{
		STask* pTask = m_task[mmTaskID];

		error = CreateTaskParam(mmTaskID, TDM_RELEASE_DEDICATED_SERVER_SENDIDS, NULL, 0, sizeof(CryLobbySendID[MAX_LOBBY_CONNECTIONS]));

		if (error == eCLE_Success)
		{
			pTask->numParams[TDN_RELEASE_DEDICATED_SERVER_COOKIE] = uint32(gEnv->pTimer->GetAsyncTime().GetValue());
			FROM_GAME_TO_LOBBY(&CCryMatchMaking::StartTaskRunning, this, mmTaskID);
		}
		else
		{
			FreeTask(mmTaskID);
		}
	}

	NetLog("[Lobby] Start SessionReleaseDedicatedServer error %d", error);

	return error;
}

void CCryMatchMaking::StartSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];
	SSession* pSession = m_sessions[pTask->session];
	CryMatchMakingConnectionID serverConnectionID = pSession->serverConnectionID;

	// Need to reset parts of the task for when the release is because of a fail in allocate.
	StartSubTask(eT_SessionReleaseDedicatedServer, mmTaskID);
	pTask->numParams[TDN_RELEASE_DEDICATED_SERVER_SEND_COUNTER] = 0;
	pTask->timerStarted = false;

	pSession->serverConnectionID = CryMatchMakingInvalidConnectionID;
	SendServerInfo(pTask->session, CryMatchMakingInvalidConnectionID, mmTaskID, (CryLobbySendID*)m_lobby->MemGetPtr(pTask->params[TDM_RELEASE_DEDICATED_SERVER_SENDIDS]));

	// It is safe to free our connection.
	// Even if the release fails the server will return itself to the free pool when it times out with no connections.
	SessionDisconnectRemoteConnectionViaNub(pTask->session, serverConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_Unknown, "StartSessionReleaseDedicatedServer Disconnect Server");
}

void CCryMatchMaking::OnSendCompleteSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID, CryLobbySendID sendID)
{
	STask* pTask = m_task[mmTaskID];
	CryLobbySendID* pSendIDs = (CryLobbySendID*)m_lobby->MemGetPtr(pTask->params[TDM_RELEASE_DEDICATED_SERVER_SENDIDS]);

	if (pSendIDs)
	{
		SSession* pSession = m_sessions[pTask->session];

		for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
		{
			SSession::SRConnection* pConnection = pSession->remoteConnection[i];

			if (pConnection->used && (pSendIDs[i] == sendID))
			{
				pSendIDs[i] = CryLobbyInvalidSendID;
				return;
			}
		}
	}
}

void CCryMatchMaking::TickSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];
	CryLobbySendID* pSendIDs = (CryLobbySendID*)m_lobby->MemGetPtr(pTask->params[TDM_RELEASE_DEDICATED_SERVER_SENDIDS]);

	if (pSendIDs)
	{
		SSession* pSession = m_sessions[pTask->session];

		for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
		{
			SSession::SRConnection* pConnection = pSession->remoteConnection[i];

			if (pConnection->used && (pSendIDs[i] != CryLobbyInvalidSendID))
			{
				return;
			}
		}
	}

	if (!pTask->TimerStarted() || (pTask->GetTimer() > RELEASE_DEDICATED_SERVER_SEND_INTERVAL))
	{
		if (pTask->numParams[TDN_RELEASE_DEDICATED_SERVER_SEND_COUNTER] < RELEASE_DEDICATED_SERVER_MAX_SENDS)
		{
			const uint32 MaxBufferSize = CryLobbyPacketUnReliableHeaderSize + CryLobbyPacketUINT64Size + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size;
			uint8 buffer[MaxBufferSize];
			CCrySharedLobbyPacket packet;

			packet.SetWriteBuffer(buffer, MaxBufferSize);
			packet.StartWrite(eLobbyPT_C2A_RequestReleaseDedicatedServer, false);
			packet.WriteUINT64(GetSIDFromSessionHandle(pTask->session));
			packet.WriteUINT32(pTask->numParams[TDN_RELEASE_DEDICATED_SERVER_COOKIE]);
			packet.WriteUINT8(mmTaskID);

			pTask->numParams[TDN_RELEASE_DEDICATED_SERVER_SEND_COUNTER]++;
			pTask->StartTimer();

			if (m_lobby->Send(&packet, m_arbitratorAddr, CryLobbyInvalidConnectionID, NULL) != eSE_Ok)
			{
				// Don't set an error since already freed connection just let server detect no connections and free itself.
				StopTaskRunning(mmTaskID);
			}
		}
		else
		{
			// Don't set an error since already freed connection just let server detect no connections and free itself.
			StopTaskRunning(mmTaskID);
		}
	}
}

void CCryMatchMaking::EndSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = m_task[mmTaskID];

	((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
}

void CCryMatchMaking::ProcessRequestReleaseDedicatedServerResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	pPacket->StartRead();
	CryMatchMakingTaskID mmTaskID = pPacket->ReadUINT8();

	mmTaskID = FindTaskFromTaskTaskID(eT_SessionReleaseDedicatedServer, mmTaskID);

	if (mmTaskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = m_task[mmTaskID];

		uint32 cookie = pPacket->ReadUINT32();

		if (cookie == pTask->numParams[TDN_RELEASE_DEDICATED_SERVER_COOKIE])
		{
			StopTaskRunning(mmTaskID);
		}
	}
}

void CCryMatchMaking::ProcessD2CHostMigrationStart(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
		#if NETWORK_HOST_MIGRATION
	pPacket->StartRead();

	SCryMatchMakingConnectionUID newHostUID = pPacket->ReadConnectionUID();
	CryLobbySessionHandle sh = CryLobbyInvalidSessionHandle;
	CryMatchMakingConnectionID connID = CryMatchMakingInvalidConnectionID;

	if (FindLocalConnectionFromUID(newHostUID, &sh) || FindConnectionFromUID(newHostUID, &sh, &connID))
	{
		SSession* pSession = m_sessions[sh];
		pSession->desiredHostUID = newHostUID;

		if (connID != CryMatchMakingInvalidConnectionID)
		{
			NetLog("[Lobby] received new session host uid " PRFORMAT_SHMMCINFO " (not me)", PRARG_SHMMCINFO(sh, connID, pSession->remoteConnection[connID]->connectionID, newHostUID));
		}
		else
		{
			NetLog("[Lobby] received new session host uid " PRFORMAT_SH " " PRFORMAT_UID " (it's me)", PRARG_SH(sh), PRARG_UID(newHostUID));
		}
	}
	else
	{
		NetLog("[Lobby] failed to find connection for new session host " PRFORMAT_UID, PRARG_UID(newHostUID));
	}
		#endif
}
	#else
ECryLobbyError CCryMatchMaking::SessionSetupDedicatedServer(CrySessionHandle h, CCryLobbyPacket* pPacket, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSetupDedicatedServerCallback pCB, void* pCBArg)
{
	return eCLE_ServiceNotSupported;
}

ECryLobbyError CCryMatchMaking::SessionReleaseDedicatedServer(CrySessionHandle h, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	return eCLE_ServiceNotSupported;
}
	#endif

#endif // USE_CRY_MATCHMAKING
