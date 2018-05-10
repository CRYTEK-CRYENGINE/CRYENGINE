// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CrySharedLobbyPacket.h"

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"
	#include "CryPSN2Response.h"
	#include "CryPSN2MatchMaking.h"

	#if USE_PSN
		#if USE_CRY_MATCHMAKING

			#include <CryCore/CryEndian.h>
			#include <CryGame/IGameFramework.h>

			#include <libsysmodule.h>
			#include "CryPSN2WebApi.h"
			#include "CryPSN2WebApi_Session.h"

			#define SIGNALING_TIMEOUT                                    (15 * 1000) // 15 seconds using our own timeout code
			#define PSN_MATCHMAKING_DEFAULT_UNKNOWN_PING                 (3000)

			#define MAX_ROOMS_PER_WORLD                                  (5000) //-- Max 5000 rooms available for one world

			#define PSN_MATCHMAKING_PSNSUPPORT_LEAVE_REQUEST_SLOT        (0)
			#define PSN_MATCHMAKING_WEBAPI_LEAVE_REQUEST_SLOT            (1)

			#define PSN_MATCHMAKING_SEARCH_PARAMS_SLOT                   (0)
			#define PSN_MATCHMAKING_SEARCH_DATA_SLOT                     (1)
			#define PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT          (2)
			#define PSN_MATCHMAKING_PSNSUPPORT_SEARCH_QOS_SLOT           (3)

			#define PSN_MATCHMAKING_PSNSUPPORT_JOIN_REQUEST_SLOT         (0)
			#define PSN_MATCHMAKING_PSNSUPPORT_JOIN_RESPONSE_SLOT        (1)
			#define PSN_MATCHMAKING_WEBAPI_JOIN_SLOT                     (2)

			#define PSN_MATCHMAKING_CREATE_PARAMS_SLOT                   (0)
			#define PSN_MATCHMAKING_CREATE_DATA_SLOT                     (1)
			#define PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT       (2)
			#define PSN_MATCHMAKING_WEBAPI_CREATE_SLOT                   (3)

			#define PSN_MATCHMAKING_UPDATE_DATA_SLOT                     (0)
			#define PSN_MATCHMAKING_PSNSUPPORT_UPDATE_REQUEST_SLOT       (1)

			#define PSN_MATCHMAKING_USERDATA_DATA_SLOT                   (0)
			#define PSN_MATCHMAKING_PSNSUPPORT_USERDATA_REQUEST_SLOT     (1)

			#define PSN_MATCHMAKING_PSNSUPPORT_QUERY_REQUEST_SLOT        (0)
			#define PSN_MATCHMAKING_PSNSUPPORT_QUERY_RESPONSE_SLOT       (1)

			#define PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_REQUEST_SLOT     (0)
			#define PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_RESTART          (1)

			#define PSN_MATCHMAKING_PSNSUPPORT_GRANTROOM_REQUEST_SLOT    (0)

			#define PSN_MATCHMAKING_PSNSUPPORT_UPDATEADVERT_REQUEST_SLOT (0)
			#define PSN_MATCHMAKING_WEBAPI_UPDATEADVERT_REQUEST_SLOT     (1)

			#define PSN_MATCHMAKING_PSNSUPPORT_GETADVERT_REQUEST_SLOT    (0)
			#define PSN_MATCHMAKING_WEBAPI_GETADVERT_REQUEST_SLOT        (1)
			#define PSN_MATCHMAKING_WEBAPI_GETADVERT_RESPONSE_SLOT       (2)

CryMatchMakingConnectionID CCryPSNMatchMaking::FindConnectionIDFromRoomMemberID(CryLobbySessionHandle sessionHandle, SceNpMatching2RoomMemberId memberId)
{
	if ((sessionHandle != CryLobbyInvalidSessionHandle) && (memberId != INVALID_PSN_ROOM_MEMBER_ID))
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->IsUsed())
		{
			for (uint32 a = 0; a < MAX_LOBBY_CONNECTIONS; a++)
			{
				SSession::SRConnection* pConnection = &pSession->remoteConnection[a];
				if (pConnection->used && pConnection->registered && (pConnection->memberId == memberId))
				{
					return a;
				}
			}
		}
	}

	return CryMatchMakingInvalidConnectionID;
}

CCryPSNMatchMaking::SSession::SRoomMember* CCryPSNMatchMaking::FindRoomMemberFromRoomMemberID(CryLobbySessionHandle sessionHandle, SceNpMatching2RoomMemberId memberId)
{
	if ((sessionHandle != CryLobbyInvalidSessionHandle) && (memberId != INVALID_PSN_ROOM_MEMBER_ID))
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->IsUsed())
		{
			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				if (pSession->m_members[a].IsUsed() && (pSession->m_members[a].m_memberId == memberId))
				{
					return &pSession->m_members[a];
				}
			}
		}
	}

	return NULL;
}

uint64 CCryPSNMatchMaking::GetSIDFromSessionHandle(CryLobbySessionHandle h)
{
	CRY_ASSERT_MESSAGE((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed(), "CCryPSNMatchMaking::GetSIDFromSessionHandle: invalid session handle");

	return m_sessions[h].m_roomId;
}

CrySessionID CCryPSNMatchMaking::SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle gh)
{
	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed())
	{
		SCryPSNSessionID* pID = new SCryPSNSessionID;

		if (pID)
		{
			pID->m_sessionInfo.m_roomId = m_sessions[h].m_roomId;
			pID->m_sessionInfo.m_worldId = m_sessions[h].m_worldId;
			pID->m_sessionInfo.m_serverId = m_sessions[h].m_serverId;
			pID->m_sessionInfo.m_gameType = m_sessions[h].m_gameType;
			memcpy(&pID->m_sessionId, &m_sessions[h].m_sessionId, sizeof(pID->m_sessionId));
			pID->m_fromInvite = false;

			memcpy(&pID->m_sessionId, &m_sessions[h].m_sessionId, sizeof(SceNpSessionId));

			return pID;
		}
	}

	return NULL;
}

TNetAddress CCryPSNMatchMaking::GetHostAddressFromSessionHandle(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	SSession* pSession = &m_sessions[h];

	if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
	{
		const TNetAddress* pDedicatedServerAddr = m_lobby->GetNetAddress(pSession->remoteConnection[pSession->serverConnectionID].connectionID);

		if (pDedicatedServerAddr)
		{
			return *pDedicatedServerAddr;
		}
	}

	SSession::SRoomMember* pMember = FindRoomMemberFromRoomMemberID(h, pSession->m_ownerMemberId);
	if (pSession->m_ownerMemberId == pSession->m_myMemberId)
	{
		return TNetAddress(TLocalNetAddress(m_lobby->GetInternalSocketPort(eCLS_Online)));
	}

	SIPv4Addr addr4;
	addr4.addr = ntohl(pMember->m_peerAddr.s_addr);
	addr4.port = ntohs(pMember->m_peerPort);

			#if !ENABLE_PLATFORM_PROTOCOL
	if (m_pService)
	{
		uint16 connectPort, listenPort;
		m_pService->GetSocketPorts(connectPort, listenPort);
		addr4.port = ntohs(connectPort);
	}
			#endif

	return TNetAddress(addr4);
}

void CCryPSNMatchMaking::SessionDisconnectRemoteConnection(CrySessionHandle gh, const TNetAddress& addr)
{
	// At present i don't need to worry about this
}

void CCryPSNMatchMaking::FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

		if (pConnection->used)
		{
			SessionUserDataEvent(eCLSE_SessionUserLeave, h, id);

			CCryMatchMaking::FreeRemoteConnection(h, id);

			pConnection->registered = false;
			pConnection->memberId = INVALID_PSN_ROOM_MEMBER_ID;
		}
	}
}

uint32 CCryPSNMatchMaking::GetSessionIDSizeInPacket() const
{
	return sizeof(SSynchronisedSessionID);
}

ECryLobbyError CCryPSNMatchMaking::WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const
{
	if (pPacket)
	{
		SCryPSNSessionID blank;

		if (sessionId != CrySessionInvalidID)
		{
			SCryPSNSessionID* psnID = (SCryPSNSessionID*)sessionId.get();
			if (psnID)
			{
				pPacket->WriteData(&psnID->m_sessionInfo, sizeof(psnID->m_sessionInfo));
			}
			else
			{
				pPacket->WriteData(&blank.m_sessionInfo, sizeof(blank.m_sessionInfo));
			}
		}
		else
		{
			pPacket->WriteData(&blank.m_sessionInfo, sizeof(blank.m_sessionInfo));
		}

		return eCLE_Success;
	}

	return eCLE_InternalError;
}

CrySessionID CCryPSNMatchMaking::ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const
{
	if (pPacket)
	{
		CrySessionID returnId = new SCryPSNSessionID;
		SCryPSNSessionID* psnID = (SCryPSNSessionID*)returnId.get();
		if (psnID)
		{
			SCryPSNSessionID blank;

			pPacket->ReadData(&psnID->m_sessionInfo, sizeof(psnID->m_sessionInfo));

			//-- Cannot use the SCryPSNSessionID equals operator because we write more than just the roomId to the packet
			//-- to signify a null session ID.
			if ((psnID->m_sessionInfo.m_roomId != blank.m_sessionInfo.m_roomId) ||
			    (psnID->m_sessionInfo.m_worldId != blank.m_sessionInfo.m_worldId) ||
			    (psnID->m_sessionInfo.m_serverId != blank.m_sessionInfo.m_serverId) ||
			    (psnID->m_sessionInfo.m_gameType != blank.m_sessionInfo.m_gameType))
			{
				//-- assume it's a valid session id if any field does not match the blank
				return returnId;
			}
		}
	}

	return CrySessionInvalidID;
}

void CCryPSNMatchMaking::DumpMemberInfo(SSession::SRoomMember* pMember)
{
			#if !defined(_RELEASE) || defined(RELEASE_LOGGING)
	NetLog("%s , uid %d , %d.%d.%d.%d:%d",
	       pMember->m_npId.handle.data,
	       pMember->m_memberId,
	       (pMember->m_peerAddr.s_addr >> 24) & 0xFF, (pMember->m_peerAddr.s_addr >> 16) & 0xFF, (pMember->m_peerAddr.s_addr >> 8) & 0xFF, (pMember->m_peerAddr.s_addr) & 0xFF,
	       pMember->m_peerPort);
			#endif // #if !defined(_RELEASE) || defined(RELEASE_LOGGING)
}

void CCryPSNMatchMaking::ClearRoomMember(CryLobbySessionHandle sessionHandle, uint32 nMember)
{
	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];
		SSession::SRoomMember* pMember = &pSession->m_members[nMember];

		if (pMember->IsUsed())
		{
			if (pMember->IsValid(ePSNMI_Other))
			{
				// Remove the player (if we have connection information we will need to drop that here too)
				CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(sessionHandle, pMember->m_memberId);
				if (id != CryMatchMakingInvalidConnectionID)
				{
					// This disconnect should be treated as if it came from the host.
					SessionDisconnectRemoteConnectionViaNub(sessionHandle, id, eDS_FromID, pSession->hostConnectionID, eDC_Unknown, "Clear Room Member");
				}
			}

			pMember->m_signalingState = ePSNCS_None;
			pMember->m_memberId = INVALID_PSN_ROOM_MEMBER_ID;
			pMember->m_valid = ePSNMI_None;
			pMember->m_bHostJoinAck = false;
			memset(pMember->m_userData, 0, sizeof(pMember->m_userData));
		}
	}
}

void CCryPSNMatchMaking::AddRoomMember(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternal& memberInfo, ECryPSNMemberInfo info)
{
	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->IsUsed())
		{
			SSession::SRoomMember* pMember = NULL;

			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				if (pSession->m_members[a].IsUsed())
				{
					if (pSession->m_members[a].m_memberId == memberInfo.m_memberId)
					{
						//-- already exists!
						CRY_ASSERT_MESSAGE(pSession->m_members[a].m_memberId != memberInfo.m_memberId, "Adding a room member who already exists!");
						return;
					}
				}
				else
				{
					if (pMember == NULL)
					{
						pMember = &pSession->m_members[a];
					}
				}
			}

			if (pMember)
			{
				pMember->m_npId = memberInfo.m_npId;
				pMember->m_memberId = memberInfo.m_memberId;
				pMember->m_valid = info;
				pMember->m_signalingState = ePSNCS_None;
				pMember->m_bHostJoinAck = false;

				if (memberInfo.m_numBinAttributes > 0)
				{
					memcpy(pMember->m_userData, memberInfo.m_binAttributes[0].m_data, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
				}

				if (pMember->IsValid(ePSNMI_Me))
				{
					pSession->m_myMemberId = pMember->m_memberId;

					pSession->localConnection.uid.m_uid = pMember->m_memberId;
					pSession->localConnection.uid.m_sid = pSession->m_roomId;
				}

				if (pMember->IsValid(ePSNMI_Owner))
				{
					pSession->m_ownerMemberId = pMember->m_memberId;
				}

				if (pMember->IsValid(ePSNMI_Other))
				{
					// We don't have connection information yet (ip / port etc)
					SCryMatchMakingConnectionUID matchingUID;

					matchingUID.m_uid = pMember->m_memberId;
					matchingUID.m_sid = pSession->m_roomId;

					CryMatchMakingConnectionID id = CCryMatchMaking::AddRemoteConnection(sessionHandle, CryLobbyInvalidConnectionID, matchingUID, 1);
					if (id != CryMatchMakingInvalidConnectionID)
					{
						SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

						//						pConnection->flags |= CMMRC_FLAG_PLATFORM_DATA_VALID;
						pConnection->registered = true;
						pConnection->connectionID = CryLobbyInvalidConnectionID;
						pConnection->memberId = pMember->m_memberId;

						if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) != CRYSESSION_LOCAL_FLAG_HOST)
						{
							if (!pMember->IsValid(ePSNMI_Owner))
							{
								//-- If person being added is not the owner, and I am not the owner, send a user joined event to the game immediately -
								//-- there is no need to wait for signaling to establish.
								//-- This should allow peers behind strict firewalls to see each other joining
								//-- but still ensure clients are only added to the server when the signaling establishes.
								DumpMemberInfo(pMember);
								AddVoiceUser(pMember);
								SessionUserDataEvent(eCLSE_SessionUserJoin, sessionHandle, id);
							}
						}
					}

					//-- Not me, and no peer state defined yet, so lets kick off a peer connection test for this member
					pMember->m_signalingState = ePSNCS_Pending;
					pMember->m_signalingTimer = g_time;
				}
			}
		}
	}
}

void CCryPSNMatchMaking::RemoveRoomMember(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternal& memberInfo)
{
	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->IsUsed())
		{
			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				SSession::SRoomMember* pMember = &pSession->m_members[a];

				if (pMember->IsUsed() && (sceNpCmpNpId(&pMember->m_npId, &memberInfo.m_npId) == 0))
				{
					ClearRoomMember(sessionHandle, a);
					return;
				}
			}
		}
	}
}

void CCryPSNMatchMaking::UpdateRoomMember(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternal& updatedMemberInfo)
{
	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->IsUsed())
		{
			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				SSession::SRoomMember* pMember = &pSession->m_members[a];

				if (pMember->m_valid && (pMember->m_memberId == updatedMemberInfo.m_memberId))
				{
					if (updatedMemberInfo.m_numBinAttributes > 0)
					{
						memcpy(pMember->m_userData, updatedMemberInfo.m_binAttributes[0].m_data, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
					}

					CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(sessionHandle, pMember->m_memberId);
					SessionUserDataEvent(eCLSE_SessionUserUpdate, sessionHandle, id);

					return;
				}
			}
		}
	}
}

void CCryPSNMatchMaking::ClearSessionInfo(CryLobbySessionHandle sessionHandle)
{
	assert(sessionHandle != CryLobbyInvalidSessionHandle);

	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];

		if (pSession->IsUsed() && (pSession->m_roomId != INVALID_PSN_ROOM_ID))
		{
			SceNpMatching2RequestId IDontCareReqId;
			SceNpMatching2LeaveRoomRequest IDontCareLeaveParam;

			memset(&IDontCareLeaveParam, 0, sizeof(SceNpMatching2LeaveRoomRequest));
			IDontCareLeaveParam.roomId = pSession->m_roomId;

			sceNpMatching2LeaveRoom(m_pPSNSupport->GetMatchmakingContextId(), &IDontCareLeaveParam, NULL, &IDontCareReqId);
			if (pSession->m_sessionId.data[0] != 0)
			{
				// TODO: the only trouble here is that leaveSessionParamsHdl needs to be allocated, but when do we free it? There's no task to complete!
				// m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::LeaveSession, leaveSessionParamsHdl);
			}
		}

		for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
		{
			ClearRoomMember(sessionHandle, a);
		}
	}
}

void CCryPSNMatchMaking::AddInitialRoomMembers(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternalList& memberList)
{
	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->IsUsed())
		{
			for (uint32 a = 0; a < memberList.m_numRoomMembers; a++)
			{
				SCryPSNRoomMemberDataInternal& memberInfo = memberList.m_pRoomMembers[a];
				uint32 info = ePSNMI_None;

				if (a == memberList.m_owner)
				{
					info |= ePSNMI_Owner;
				}

				if (a == memberList.m_me)
				{
					info |= ePSNMI_Me;
				}
				else
				{
					info |= ePSNMI_Other;
				}

				AddRoomMember(sessionHandle, memberInfo, (ECryPSNMemberInfo)info);
			}
		}
	}
}

void CCryPSNMatchMaking::DispatchRoomOwnerChangedEvent(TMemHdl mh)
{
	UCryLobbyEventData eventData;
	eventData.pRoomOwnerChanged = (SCryLobbyRoomOwnerChanged*)m_lobby->MemGetPtr(mh);

	m_lobby->DispatchEvent(eCLSE_RoomOwnerChanged, eventData);

	m_lobby->MemFree(mh);
}

void CCryPSNMatchMaking::ChangeRoomOwner(CryLobbySessionHandle sessionHandle, SceNpMatching2RoomMemberId prevOwner, SceNpMatching2RoomMemberId newOwner)
{
	if (sessionHandle != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[sessionHandle];
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			SSession::SRoomMember* pMember = NULL;

			pSession->m_ownerMemberId = newOwner;

			//-- remove the old room owner
			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				pMember = &pSession->m_members[a];
				if (pMember->m_valid)
				{
					if (pMember->m_memberId == prevOwner)
					{
						assert(pMember->m_valid & ePSNMI_Owner);

						if (pMember->m_valid & ePSNMI_Me)
						{
							pMember->m_valid = ePSNMI_Me;
						}
						else
						{
							pMember->m_valid = ePSNMI_Other;
						}
					}

					if (pMember->m_memberId == newOwner)
					{
						if (pMember->m_valid & ePSNMI_Me)
						{
							pMember->m_valid = ePSNMI_MeOwner;
						}
						else
						{
							pMember->m_valid = ePSNMI_OtherOwner;
						}
					}
				}
			}

			#if NETWORK_HOST_MIGRATION
			CryLobbySessionHandle h;

			SCryMatchMakingConnectionUID matchingUID;

			matchingUID.m_uid = newOwner;
			matchingUID.m_sid = pSession->m_roomId;

			FindConnectionFromUID(matchingUID, &h, &pSession->hostConnectionID);    // It is valid for the h to be invalid (basically if hostConnectionID comes back with 0 we don't have any remotes - thats allowed!)

			// Ensure we're migrating the session (covers catastrophic migration, and migration within migration)
			HostMigrationInitiate(sessionHandle, eDC_Unknown);
			pSession->hostMigrationInfo.m_newHostAddressValid = true;
			pSession->newHostUID = matchingUID;

			SSession::SRoomMember* pRoomMember = FindRoomMemberFromRoomMemberID(sessionHandle, newOwner);

			if (pRoomMember)
			{
				if (pRoomMember->IsValid(ePSNMI_Other) && (pRoomMember->m_signalingState == ePSNCS_Active))
				{
					if (m_lobby)
					{
						TMemHdl mh = m_lobby->MemAlloc(sizeof(SCryLobbyRoomOwnerChanged));
						if (mh != TMemInvalidHdl)
						{
							SCryLobbyRoomOwnerChanged* pOwnerData = (SCryLobbyRoomOwnerChanged*)m_lobby->MemGetPtr(mh);

							pOwnerData->m_session = CreateGameSessionHandle(sessionHandle, pSession->localConnection.uid);
							pOwnerData->m_port = pRoomMember->m_peerPort;
							pOwnerData->m_ip = pRoomMember->m_peerAddr.s_addr;

							TO_GAME_FROM_LOBBY(&CCryPSNMatchMaking::DispatchRoomOwnerChangedEvent, this, mh);
						}
					}
				}
			}
			#endif
		}
	}
}

void CCryPSNMatchMaking::UpdateSignaling(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->used && pTask->running && (pTask->session != CryLobbyInvalidSessionHandle))
	{
		SSession* pSession = &m_sessions[pTask->session];
		if (pSession->IsUsed())
		{
			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				SSession::SRoomMember* pMember = &pSession->m_members[a];

				if (pMember->IsUsed())
				{
					//-- Timed out?
					bool bTestTimeout = false;

					if (pMember->m_signalingState == ePSNCS_Pending)
					{
						bTestTimeout = true;
					}
					else
					{
						if ((pTask->startedTask == eT_SessionJoin) && pMember->IsValid(ePSNMI_Owner) && (pMember->m_signalingState == ePSNCS_Active) && (pMember->m_bHostJoinAck == false))
						{
							bTestTimeout = true;
						}
					}

					if (bTestTimeout)
					{
						if ((g_time.GetMilliSecondsAsInt64() - pMember->m_signalingTimer.GetMilliSecondsAsInt64()) > SIGNALING_TIMEOUT)
						{
							pMember->m_signalingState = ePSNCS_Dead;

							CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(pTask->session, pMember->m_memberId);
							if (id != CryMatchMakingInvalidConnectionID)
							{
								SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
								NetLog("[Lobby] CCryPSNMatchMaking::UpdateSignaling() : Signaling dead event setting connection " PRFORMAT_LCID " to eCLCS_NotConnected", PRARG_LCID(pConnection->connectionID));
								m_lobby->ConnectionSetState(pConnection->connectionID, eCLCS_NotConnected);
							}
						}
					}
				}
			}
		}
	}
}

CCryPSNMatchMaking::ECryPSNConnectionState CCryPSNMatchMaking::CheckSignaling(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->used && pTask->running && (pTask->session != CryLobbyInvalidSessionHandle))
	{
		SSession* pSession = &m_sessions[pTask->session];
		if (pSession->IsUsed())
		{
			//-- If we can talk to the room owner, we'll allow the session state machine to run
			for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
			{
				SSession::SRoomMember* pMember = &pSession->m_members[a];

				if (pMember->IsValid(ePSNMI_Owner))
				{
					//-- Are we the owner?
					if (pMember->IsValid(ePSNMI_Me))
					{
						//-- technically, 'me' always has a signaling state of ePSNMI_None, so we'll fake it just for this return.
						return ePSNCS_Active;
					}

					//-- Is peer connection to remote owner active?
					if (pMember->m_signalingState == ePSNCS_Active)
					{
						if (pMember->m_bHostJoinAck)
						{
							return ePSNCS_Active;
						}
						else
						{
							return ePSNCS_Pending;
						}
					}

					return pMember->m_signalingState;
				}
			}
		}
	}

	//-- No owner at all! This is wrong!
	return ePSNCS_None;
}

bool CCryPSNMatchMaking::ProcessCorrectTaskForRequest(SCryPSNSupportCallbackEventData& data, ETask subTaskCheck, const char* pRequestString, EventRequestResponseCallback pCB)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		STask* pTask = &m_task[i];
		if (pTask->used && pTask->running)
		{
			if (pTask->m_reqId == data.m_requestEvent.m_reqId)
			{
				bool bHandled = false;

				if (pTask->subTask == subTaskCheck)
				{
					bHandled = pCB(this, pTask, i, data);

					if (bHandled)
					{
						data.m_requestEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_HANDLED;
					}
					else
					{
						data.m_requestEvent.m_returnFlags = SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_NOT_HANDLED;

						// a task was found, but not handled. mark the task with an error so it stops itself

						// Some generic behaviours for non-critical errors
						switch (data.m_requestEvent.m_error)
						{
						case SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND:
						case SCE_NP_MATCHING2_SERVER_ERROR_NO_SUCH_ROOM:
						case SCE_NP_MATCHING2_SERVER_ERROR_NO_SUCH_ROOM_INSTANCE:
							{
								UpdateTaskError(i, eCLE_RoomDoesNotExist);
								data.m_requestEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_HANDLED;
							}
							break;
						case SCE_NP_MATCHING2_SERVER_ERROR_ROOM_FULL:
							{
								UpdateTaskError(i, eCLE_SessionFull);
								data.m_requestEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_HANDLED;
							}
							break;
						default:
							{
								UpdateTaskError(i, eCLE_InternalError);
								// If we leave it unhandled, PSNSupport will attempt to handle any critical errors for us
							}
							break;
						}
					}

					pTask->m_reqId = INVALID_PSN_REQUEST_ID;

					if (pTask->error != eCLE_Success)
					{
						StopTaskRunning(i);
					}

					return bHandled;
				}
			}
		}
	}

	NetLog("%s event received, but no tasks in correct state to process it!", pRequestString);
	return false;
}

bool CCryPSNMatchMaking::ProcessCorrectTaskForSearch(SCryPSNSupportCallbackEventData& data, ETask subTaskCheck, const char* pRequestString, EventRequestResponseCallback pCB)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		STask* pTask = &m_task[i];
		if (pTask->used && pTask->running)
		{
			if (pTask->subTask == subTaskCheck)
			{
				assert(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT] != TMemInvalidHdl);

				SSearchInfoData* pSearchInfo = (SSearchInfoData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);
				if (pSearchInfo)
				{
					for (int a = 0; a < pSearchInfo->m_numResults; a++)
					{
						if (data.m_requestEvent.m_reqId == pSearchInfo->m_reqId[a])
						{
							pTask->m_reqId = data.m_requestEvent.m_reqId;

							bool bHandled = pCB(this, pTask, i, data);

							if (bHandled)
							{
								data.m_requestEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_HANDLED;
							}
							else
							{
								data.m_requestEvent.m_returnFlags = SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_NOT_HANDLED;

								// a task was found, but not handled, stop the task.
								// If we leave it unhandled, the PSNSupport will attempt to handle any critical errors for us
								UpdateTaskError(i, eCLE_InternalError);
							}

							pTask->m_reqId = INVALID_PSN_REQUEST_ID;

							if (pTask->error != eCLE_Success)
							{
								StopTaskRunning(i);
							}

							return bHandled;
						}
					}
				}
				else
				{
					// no search info
					NetLog("%s event received, task %d found with correct subtask %d, but no pSearchInfo!", pRequestString, i, subTaskCheck);
					UpdateTaskError(i, eCLE_OutOfMemory);
					StopTaskRunning(i);
					return false;
				}
			}
		}
	}

	NetLog("%s event received, but no tasks in correct state to process it!", pRequestString);
	return false;
}

void CCryPSNMatchMaking::ProcessRequestEvent(SCryPSNSupportCallbackEventData& data)
{
	switch (data.m_requestEvent.m_event)
	{
	case REQUEST_EVENT_GET_WORLD_INFO_LIST:
		{
			if (!ProcessCorrectTaskForRequest(data, eST_WaitingForCreateRoomWorldInfo, "REQUEST_EVENT_GET_WORLD_INFO_LIST", EventRequestResponse_GetWorldInfoListForCreate))
			{
				NetLog("Not a create, so trying search instead...");
				if (!ProcessCorrectTaskForRequest(data, eST_WaitingForSearchRoomWorldInfo, "REQUEST_EVENT_GET_WORLD_INFO_LIST", EventRequestResponse_GetWorldInfoListForSearch))
				{
					NetLog("Not a search either! Time to trigger unhandled REQUEST_EVENT_GET_WORLD_INFO_LIST!");
				}
			}
		}
		break;
	case REQUEST_EVENT_LEAVE_ROOM:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForLeaveRoomRequestCallback, "REQUEST_EVENT_LEAVE_ROOM", EventRequestResponse_LeaveRoom);
		}
		break;
	case REQUEST_EVENT_SET_ROOM_MEMBER_DATA_INTERNAL:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForUserDataRequestCallback, "REQUEST_EVENT_SET_ROOM_MEMBER_DATA_INTERNAL", EventRequestResponse_SetRoomMemberDataInternal);
		}
		break;
	case REQUEST_EVENT_SET_ROOM_DATA_EXTERNAL:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForUpdateRoomRequestCallback, "REQUEST_EVENT_SET_ROOM_DATA_EXTERNAL", EventRequestResponse_SetRoomDataExternal);
		}
		break;
	case REQUEST_EVENT_SET_ROOM_DATA_INTERNAL:
		{
			#if NETWORK_HOST_MIGRATION
			ProcessCorrectTaskForRequest(data, eST_WaitingForNewHostHintCallback, "REQUEST_EVENT_SET_ROOM_DATA_INTERNAL", EventRequestResponse_SetRoomDataInternal);
			#endif
		}
		break;
	case REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForQueryRoomRequestCallback, "REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST", EventRequestResponse_GetRoomDataExternalList);
		}
		break;
	case REQUEST_EVENT_SEARCH_ROOM:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForSearchRoomRequestCallback, "REQUEST_EVENT_SEARCH_ROOM", EventRequestResponse_SearchRoom);
		}
		break;
	case REQUEST_EVENT_SIGNALING_GET_PING_INFO:
		{
			ProcessCorrectTaskForSearch(data, eST_WaitingForSearchQOSRequestCallback, "REQUEST_EVENT_SIGNALING_GET_PING_INFO", EventRequestResponse_SearchQOS);
		}
		break;
	case REQUEST_EVENT_JOIN_ROOM:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForJoinRoomRequestCallback, "REQUEST_EVENT_JOIN_ROOM", EventRequestResponse_JoinRoom);
		}
		break;
	case REQUEST_EVENT_CREATE_JOIN_ROOM:
		{
			ProcessCorrectTaskForRequest(data, eST_WaitingForCreateRoomRequestCallback, "REQUEST_EVENT_CREATE_JOIN_ROOM", EventRequestResponse_CreateRoom);
		}
		break;
	case REQUEST_EVENT_GRANT_ROOM_OWNER:
		{
			#if NETWORK_HOST_MIGRATION
			ProcessCorrectTaskForRequest(data, eST_WaitingForGrantRoomOwnerRequestCallback, "REQUEST_EVENT_GRANT_ROOM_OWNER", EventRequestResponse_GrantRoomOwner);
			#endif
		}
		break;
	default:
		break;
	} //-- switch
}

void CCryPSNMatchMaking::ProcessSignalingEvent(SCryPSNSupportCallbackEventData& data)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = &m_sessions[i];

		if (pSession->IsUsed())
		{
			SSession::SRoomMember* pMember = FindRoomMemberFromRoomMemberID(i, data.m_signalEvent.m_subject);
			if (pMember && pMember->IsValid(ePSNMI_Other) && (pMember->m_signalingState != ePSNCS_None))
			{
				switch (data.m_signalEvent.m_event)
				{
				case SIGNALING_EVENT_DEAD:
					{
						pMember->m_signalingState = ePSNCS_Dead;

						DumpMemberInfo(pMember);
						CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(i, pMember->m_memberId);
						if (id != CryMatchMakingInvalidConnectionID)
						{
							SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
							NetLog("[Lobby] CCryPSNMatchMaking::ProcessSignalingEvent() : SCE_NP_SIGNALING_EVENT_DEAD : Signaling dead event setting connection " PRFORMAT_LCID " to eCLCS_NotConnected", PRARG_LCID(pConnection->connectionID));
							NetLog("[Lobby] Signaling ID %d: SIGNALING_EVENT_DEAD not handled: error %08X", data.m_signalEvent.m_subject, data.m_signalEvent.m_error);
							m_lobby->ConnectionSetState(pConnection->connectionID, eCLCS_NotConnected);
						}

						data.m_signalEvent.m_bHandled = true;
					}
					break;

				case SIGNALING_EVENT_CONNECTED:
					{
						int conStatus;
						int ret = sceNpMatching2SignalingGetConnectionStatus(m_pPSNSupport->GetMatchmakingContextId(), data.m_signalEvent.m_roomId, data.m_signalEvent.m_subject, &conStatus, (SceNetInAddr*)&pMember->m_peerAddr, &pMember->m_peerPort);
						if ((ret == PSN_OK) && (conStatus == SCE_NP_MATCHING2_SIGNALING_CONN_STATUS_ACTIVE))
						{
							pMember->m_signalingState = ePSNCS_Active;

							CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(i, pMember->m_memberId);
							if (id != CryMatchMakingInvalidConnectionID)
							{
								SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

								if (pConnection->connectionID == CryLobbyInvalidConnectionID)
								{
									TNetAddress netAddr = TNetAddress(SIPv4Addr(ntohl(pMember->m_peerAddr.s_addr), ntohs(pMember->m_peerPort)));

			#if !ENABLE_PLATFORM_PROTOCOL
									if (m_pService)
									{
										uint16 connectPort, listenPort;

										m_pService->GetSocketPorts(connectPort, listenPort);
										netAddr = TNetAddress(SIPv4Addr(ntohl(pMember->m_peerAddr.s_addr), connectPort));
									}
			#endif
									// Add connection - if not found
									CryLobbyConnectionID lID = m_lobby->FindConnection(netAddr);
									if (lID == CryLobbyInvalidConnectionID)
									{
										lID = m_lobby->CreateConnection(netAddr);
									}
									else
									{
										if (m_lobby->ConnectionGetState(lID) == eCLCS_Freeing)
										{
											NetLog("[Lobby] Signaling active on existing Freeing connection " PRFORMAT_LCID, PRARG_LCID(lID));
											m_lobby->FreeConnection(lID);
											lID = m_lobby->CreateConnection(netAddr);
										}
									}

									m_lobby->ConnectionAddRef(lID);
									pConnection->connectionID = lID;

									DumpMemberInfo(pMember);
									NetLog("[Lobby] CCryPSNMatchMaking::ProcessSignalingEvent() : SIGNALING_EVENT_CONNECTED : Signaling established for " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(CryLobbySessionHandle(i), id, pConnection->connectionID, pConnection->uid));

									AddVoiceUser(pMember);
									SessionUserDataEvent(eCLSE_SessionUserJoin, i, id);

									if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
									{
										// Include the SceNpSessionId the host got from registering the session on PSN when room was created.
										// Pass this over to all connecting clients so they can join the same session
										const uint32 maxBufferSize = CryLobbyPacketHeaderSize + sizeof(SceNpSessionId);
										uint8 buffer[maxBufferSize];
										CCryLobbyPacket packet;

										packet.SetWriteBuffer(buffer, maxBufferSize);
										packet.StartWrite(ePSNPT_JoinSessionAck, true);
										packet.WriteString((const char*)&pSession->m_sessionId, sizeof(SceNpSessionId));

										Send(CryMatchMakingInvalidTaskID, &packet, i, id);
									}
								}
							}
						}
						else if (ret < PSN_OK)
						{
							NetLog("sceNpMatching2SignalingGetConnectionStatus : %08X", ret);

							pMember->m_signalingState = ePSNCS_Dead;

							CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(i, pMember->m_memberId);
							if (id != CryMatchMakingInvalidConnectionID)
							{
								SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
								NetLog("[Lobby] CCryPSNMatchMaking::ProcessSignalingEvent() : SIGNALING_EVENT_CONNECTED(1) : Signaling connected failure event, setting " PRFORMAT_SHMMCINFO " to eCLCS_NotConnected", PRARG_SHMMCINFO(CryLobbySessionHandle(i), id, pConnection->connectionID, pConnection->uid));
								m_lobby->ConnectionSetState(pConnection->connectionID, eCLCS_NotConnected);
							}

							m_pPSNSupport->HandlePSNError(ret);
						}
						else
						{
							pMember->m_signalingState = ePSNCS_Dead;

							CryMatchMakingConnectionID id = FindConnectionIDFromRoomMemberID(i, pMember->m_memberId);
							if (id != CryMatchMakingInvalidConnectionID)
							{
								SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
								NetLog("[Lobby] CCryPSNMatchMaking::ProcessSignalingEvent() : SIGNALING_EVENT_CONNECTED(2) : Signaling connected failure event, setting " PRFORMAT_SHMMCINFO " to eCLCS_NotConnected", PRARG_SHMMCINFO(CryLobbySessionHandle(i), id, pConnection->connectionID, pConnection->uid));
								m_lobby->ConnectionSetState(pConnection->connectionID, eCLCS_NotConnected);
							}
						}
						data.m_signalEvent.m_bHandled = true;
					}
					break;

				case SIGNALING_EVENT_NETINFO_RESULT:
					{
						NetLog("[SIGNALING] Ignored Event : %08X", data.m_signalEvent.m_event);
						data.m_signalEvent.m_bHandled = true;
					}
					break;

				default:
					// handle unknown responses - future sdks
					break;
				}
			}
		}
	}
}

void CCryPSNMatchMaking::DispatchForcedFromRoomEvent(CrySessionHandle gh, SceNpMatching2Event how, SceNpMatching2EventCause why)
{
	if (m_lobby)
	{
		UCryLobbyEventData eventData;
		SCryLobbyForcedFromRoomData eventRoomData;
		eventData.pForcedFromRoomData = &eventRoomData;

		eventRoomData.m_session = gh;

		switch (why)
		{
		case SCE_NP_MATCHING2_EVENT_CAUSE_SERVER_OPERATION:
		case SCE_NP_MATCHING2_EVENT_CAUSE_CONTEXT_ACTION:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_ServerForced)");
				eventRoomData.m_why = eFFRR_ServerForced;
			}
			break;
		case SCE_NP_MATCHING2_EVENT_CAUSE_LEAVE_ACTION:
		case SCE_NP_MATCHING2_EVENT_CAUSE_MEMBER_DISAPPEARED:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_Left)");
				eventRoomData.m_why = eFFRR_Left;
			}
			break;
		case SCE_NP_MATCHING2_EVENT_CAUSE_KICKOUT_ACTION:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_Kicked)");
				eventRoomData.m_why = eFFRR_Kicked;
			}
			break;
		case SCE_NP_MATCHING2_EVENT_CAUSE_SERVER_INTERNAL:
		case SCE_NP_MATCHING2_EVENT_CAUSE_SYSTEM_ERROR:
		case SCE_NP_MATCHING2_EVENT_CAUSE_CONTEXT_ERROR:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_ServerInternalError)");
				eventRoomData.m_why = eFFRR_ServerInternalError;
			}
			break;
		case SCE_NP_MATCHING2_EVENT_CAUSE_CONNECTION_ERROR:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_ConnectionError)");
				eventRoomData.m_why = eFFRR_ConnectionError;
			}
			break;
		case SCE_NP_MATCHING2_EVENT_CAUSE_NP_SIGNED_OUT:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_SignedOut)");
				eventRoomData.m_why = eFFRR_SignedOut;
			}
			break;
		default:
			{
				NetLog("DispatchForcedFromRoomEvent(eFFRR_Unknown)");
				eventRoomData.m_why = eFFRR_Unknown;
			}
			break;
		}

		m_lobby->DispatchEvent(eCLSE_ForcedFromRoom, eventData);
	}
}

void CCryPSNMatchMaking::ProcessRoomEvent(SCryPSNSupportCallbackEventData& data)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = &m_sessions[i];

		if (pSession->IsUsed() && (pSession->m_roomId == data.m_roomEvent.m_roomId))
		{
			switch (data.m_roomEvent.m_event)
			{
			case ROOM_EVENT_MEMBER_JOINED:
				{
					if ((data.m_roomEvent.m_error == 0) &&
					    (data.m_roomEvent.m_dataHdl != TMemInvalidHdl) &&
					    (data.m_roomEvent.m_dataSize == sizeof(SCryPSNRoomMemberUpdateInfoResponse)))
					{
						SCryPSNRoomMemberUpdateInfoResponse* pMemberUpdateInfo = (SCryPSNRoomMemberUpdateInfoResponse*)m_pPSNSupport->GetLobby()->MemGetPtr(data.m_roomEvent.m_dataHdl);

						if (pMemberUpdateInfo->m_member.m_flagAttr & SCE_NP_MATCHING2_ROOMMEMBER_FLAG_ATTR_OWNER)
						{
							AddRoomMember(i, pMemberUpdateInfo->m_member, ePSNMI_OtherOwner);
						}
						else
						{
							AddRoomMember(i, pMemberUpdateInfo->m_member, ePSNMI_Other);
						}

						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on room member join event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_MEMBER_LEFT:
				{
					if ((data.m_roomEvent.m_error == 0) &&
					    (data.m_roomEvent.m_dataHdl != TMemInvalidHdl) &&
					    (data.m_roomEvent.m_dataSize == sizeof(SCryPSNRoomMemberUpdateInfoResponse)))
					{
						SCryPSNRoomMemberUpdateInfoResponse* pMemberUpdateInfo = (SCryPSNRoomMemberUpdateInfoResponse*)m_pPSNSupport->GetLobby()->MemGetPtr(data.m_roomEvent.m_dataHdl);
						RemoveRoomMember(i, pMemberUpdateInfo->m_member);

						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on room member left event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_ROOM_OWNER_CHANGED:
				{
					if ((data.m_roomEvent.m_error == 0) &&
					    (data.m_roomEvent.m_dataHdl != TMemInvalidHdl) &&
					    (data.m_roomEvent.m_dataSize == sizeof(SCryPSNRoomOwnerUpdateInfoResponse)))
					{
						SCryPSNRoomOwnerUpdateInfoResponse* pOwnerInfo = (SCryPSNRoomOwnerUpdateInfoResponse*)m_pPSNSupport->GetLobby()->MemGetPtr(data.m_roomEvent.m_dataHdl);
						NetLog("[LEE] Got Request for room owner change : Old Owner %d, New Owner %d", pOwnerInfo->m_prevOwner, pOwnerInfo->m_newOwner);
						ChangeRoomOwner(i, pOwnerInfo->m_prevOwner, pOwnerInfo->m_newOwner);

						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on room owner changed event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_KICKED_OUT:
				{
					NetLog("ROOM_EVENT_KICKED_OUT i:%d", i);

					if ((data.m_roomEvent.m_error == 0) &&
					    (data.m_roomEvent.m_dataHdl != TMemInvalidHdl) &&
					    (data.m_roomEvent.m_dataSize == sizeof(SCryPSNRoomUpdateInfoResponse)))
					{
						ClearSessionInfo(i);
						FreeSessionHandle(i);

						SCryPSNRoomUpdateInfoResponse* pUpdateInfo = (SCryPSNRoomUpdateInfoResponse*)m_pPSNSupport->GetLobby()->MemGetPtr(data.m_roomEvent.m_dataHdl);

						TO_GAME_FROM_LOBBY(&CCryPSNMatchMaking::DispatchForcedFromRoomEvent, this, CreateGameSessionHandle(i, pSession->localConnection.uid), data.m_roomEvent.m_event, pUpdateInfo->m_eventCause);

						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on kicked out of room event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_ROOM_DESTROYED:
				{
					if ((data.m_roomEvent.m_error == 0) &&
					    (data.m_roomEvent.m_dataHdl != TMemInvalidHdl) &&
					    (data.m_roomEvent.m_dataSize == sizeof(SCryPSNRoomUpdateInfoResponse)))
					{
						ClearSessionInfo(i);
						FreeSessionHandle(i);

						SCryPSNRoomUpdateInfoResponse* pUpdateInfo = (SCryPSNRoomUpdateInfoResponse*)m_pPSNSupport->GetLobby()->MemGetPtr(data.m_roomEvent.m_dataHdl);

						TO_GAME_FROM_LOBBY(&CCryPSNMatchMaking::DispatchForcedFromRoomEvent, this, CreateGameSessionHandle(i, pSession->localConnection.uid), data.m_roomEvent.m_event, pUpdateInfo->m_eventCause);

						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on room destroyed event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_UPDATED_ROOM_DATA_INTERNAL:
				{
					//-- Don't actually care about this event:- we just trap it so it doesn't get flagged as unhandled.
					if (data.m_roomEvent.m_error == 0)
					{
						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on internal room data updated event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL:
				{
					if ((data.m_roomEvent.m_error == 0) &&
					    (data.m_roomEvent.m_dataHdl != TMemInvalidHdl) &&
					    (data.m_roomEvent.m_dataSize == sizeof(SCryPSNRoomMemberUpdateInfoResponse)))
					{

						const SCryPSNRoomMemberUpdateInfoResponse* const pUpdateInfo = (SCryPSNRoomMemberUpdateInfoResponse*)m_pPSNSupport->GetLobby()->MemGetPtr(data.m_requestEvent.m_dataHdl);
						UpdateRoomMember(i, pUpdateInfo->m_member);

						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on internal room member data updated event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			case ROOM_EVENT_UPDATED_SIGNALING_OPT_PARAM:
				{
					//-- Don't actually care about this event:- we just trap it so it doesn't get flagged as unhandled.
					if (data.m_roomEvent.m_error == 0)
					{
						data.m_roomEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRoomEvent::ROOM_EVENT_HANDLED;
					}
					else
					{
						//-- error occurred on signaling opt param updated event
						//-- leave data.m_roomEvent.m_returnFlags as 0 to let the default error handler take care of any serious errors.
					}
				}
				break;
			}

			return;
		}
	}
}

void CCryPSNMatchMaking::ProcessErrorEvent(SCryPSNSupportCallbackEventData& data)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		STask* pTask = &m_task[i];
		if (pTask->used && pTask->running)
		{
			UpdateTaskError(i, MapSupportErrorToLobbyError(data.m_errorEvent.m_error));
			StopTaskRunning(i);
		}
	}

	for (uint32 j = 0; j < MAX_MATCHMAKING_SESSIONS; j++)
	{
		SSession* pSession = &m_sessions[j];
		if (pSession->IsUsed())
		{
			ClearSessionInfo(j);
			FreeSessionHandle(j);
		}
	}
}

void CCryPSNMatchMaking::ProcessWebApiEvent(SCryPSNSupportCallbackEventData& data)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		STask* pTask = &m_task[i];
		if (pTask->used && pTask->running)
		{
			switch (pTask->subTask)
			{
			case eST_WaitingForCreateRoomWebApiCallback:
				{
					SCreateParamData* pCreateParams = (SCreateParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT]);
					if (pCreateParams && (pCreateParams->m_createJobId == data.m_webApiEvent.m_id))
					{
						EventWebApiCreateSession(i, data);
						data.m_webApiEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SWebApiEvent::WEBAPI_EVENT_HANDLED;
					}
				}
				break;
			case eST_WaitingForJoinRoomWebApiCallback:
				{
					SJoinParamData* pJoinParams = (SJoinParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_REQUEST_SLOT]);
					if (pJoinParams && (pJoinParams->m_joinJobId == data.m_webApiEvent.m_id))
					{
						EventWebApiJoinSession(i, data);
						data.m_webApiEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SWebApiEvent::WEBAPI_EVENT_HANDLED;
					}
				}
				break;
			case eST_WaitingForLeaveRoomWebApiCallback:
				{
					SLeaveParamData* pLeaveParams = (SLeaveParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_LEAVE_REQUEST_SLOT]);
					if (pLeaveParams && (pLeaveParams->m_leaveJobId == data.m_webApiEvent.m_id))
					{
						EventWebApiLeaveSession(i, data);
						data.m_webApiEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SWebApiEvent::WEBAPI_EVENT_HANDLED;
					}
				}
				break;
			case eST_WaitingForUpdateAdvertisementDataWebApiCallback:
				{
					SUpdateAdvertisementDataParamData* pAdParams = (SUpdateAdvertisementDataParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_UPDATEADVERT_REQUEST_SLOT]);
					if (pAdParams && (pAdParams->m_jobId == data.m_webApiEvent.m_id))
					{
						EventWebApiUpdateSessionAdvertisement(i, data);
						data.m_webApiEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SWebApiEvent::WEBAPI_EVENT_HANDLED;
					}
				}
				break;
			case eST_WaitingForGetAdvertisementDataWebApiCallback:
				{
					SGetAdvertisementDataParamData* pAdParams = (SGetAdvertisementDataParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GETADVERT_REQUEST_SLOT]);
					if (pAdParams && (pAdParams->m_jobId == data.m_webApiEvent.m_id))
					{
						EventWebApiGetSessionLinkData(i, data);
						data.m_webApiEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SWebApiEvent::WEBAPI_EVENT_HANDLED;
					}
				}
				break;
			}
		}
	}
}

void CCryPSNMatchMaking::SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg)
{
	CCryPSNMatchMaking* _this = static_cast<CCryPSNMatchMaking*>(pArg);

	switch (ecb)
	{
	case eCE_RequestEvent:
		{
			_this->ProcessRequestEvent(data);
		}
		break;
	case eCE_SignalEvent:
		{
			_this->ProcessSignalingEvent(data);
		}
		break;
	case eCE_RoomEvent:
		{
			_this->ProcessRoomEvent(data);
		}
		break;
	case eCE_ErrorEvent:
		{
			_this->ProcessErrorEvent(data);
		}
		break;
	case eCE_WebApiEvent:
		{
			_this->ProcessWebApiEvent(data);
		}
		break;
	}
}

ECryLobbyError CCryPSNMatchMaking::Initialise()
{
	ECryLobbyError error = CCryMatchMaking::Initialise();

	if (error == eCLE_Success)
	{
		int ret;

		ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_MATCHING2);
		if (ret < PSN_OK)
		{
			NetLog("sceSysmoduleLoadModule(SCE_SYSMODULE_NP_MATCHING2) failed. ret = 0x%x\n", ret);
			return eCLE_InternalError;
		}

		SceNpMatching2InitializeParameter configurationMatching2;
		memset(&configurationMatching2, 0, sizeof(configurationMatching2));
		configurationMatching2.size = sizeof(configurationMatching2);
		configurationMatching2.poolSize = SCE_NP_MATCHING2_POOLSIZE_DEFAULT;
		configurationMatching2.cpuAffinityMask = SCE_KERNEL_CPUMASK_USER_ALL;
		configurationMatching2.threadPriority = SCE_KERNEL_PRIO_FIFO_DEFAULT; //CLobbyCVars::Get().psnMatching2ThreadPriority;
		configurationMatching2.threadStackSize = SCE_NP_MATCHING2_THREAD_STACK_SIZE_DEFAULT;

		ret = sceNpMatching2Initialize(&configurationMatching2);
		if (ret < PSN_OK)
		{
			NetLog("sceNpMatching2Initialize() failed. ret = 0x%x\n", ret);
			return eCLE_InternalError;
		}

		m_registeredUserData.num = 0;

		for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
		{
			m_sessions[i].localFlags &= ~CRYSESSION_LOCAL_FLAG_USED;

			for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
			{
				m_sessions[i].remoteConnection[j].connectionID = CryLobbyInvalidConnectionID;
				m_sessions[i].remoteConnection[j].used = false;
				m_sessions[i].remoteConnection[j].registered = false;
				m_sessions[i].remoteConnection[j].memberId = INVALID_PSN_ROOM_MEMBER_ID;
			}

			ClearSessionInfo(i);
		}

		for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
		{
			m_task[i].m_reqId = INVALID_PSN_REQUEST_ID;
		}
	}

	return error;
}

ECryLobbyError CCryPSNMatchMaking::Terminate()
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = &m_sessions[i];
		if (pSession->IsUsed())
		{
			ClearSessionInfo(i);
			FreeSessionHandle(i);
		}
	}

	int ret;
	ret = sceNpMatching2Terminate();
	if (ret < PSN_OK)
	{
		NetLog("sceNpMatching2Termimate() failed. ret = 0x%x\n", ret);
	}

	ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_MATCHING2);
	if (ret < PSN_OK)
	{
		NetLog("sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_MATCHING2) failed. ret = 0x%x\n", ret);
	}

	return eCLE_Success;
}

CCryPSNMatchMaking::CCryPSNMatchMaking(CCryLobby* lobby, CCryLobbyService* service, CCryPSNSupport* pSupport, ECryLobbyService serviceType) : CCryMatchMaking(lobby, service, serviceType)
{
	m_pPSNSupport = pSupport;

	m_oldNatType = SCE_NP_MATCHING2_SIGNALING_NETINFO_NAT_STATUS_UNKNOWN;

	// Make the CCryMatchMaking base pointers point to our data so we can use the common code in CCryMatchMaking
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		CCryMatchMaking::m_sessions[i] = &m_sessions[i];
		CCryMatchMaking::m_sessions[i]->localConnection = &m_sessions[i].localConnection;

		for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
		{
			CCryMatchMaking::m_sessions[i]->remoteConnection[j] = &m_sessions[i].remoteConnection[j];
		}
	}

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		CCryMatchMaking::m_task[i] = &m_task[i];
	}

	REGISTER_CVAR2("net_enable_psn_hinting", &net_enable_psn_hinting, 1, VF_DUMPTODISK, "Toggles whether PSN hinting enabled or not");
}

CCryPSNMatchMaking::~CCryPSNMatchMaking()
{
}

ECryLobbyError CCryPSNMatchMaking::StartTask(ETask etask, bool startRunning, uint32 user, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg)
{
	CryMatchMakingTaskID tmpMMTaskID;
	CryMatchMakingTaskID* useMMTaskID = mmTaskID ? mmTaskID : &tmpMMTaskID;
	ECryLobbyError error = CCryMatchMaking::StartTask(etask, startRunning, useMMTaskID, lTaskID, h, cb, cbArg);

	if (error == eCLE_Success)
	{
		ResetTask(*useMMTaskID);
	}

	return error;
}

void CCryPSNMatchMaking::ResetTask(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	pTask->returnTaskID = CryMatchMakingInvalidTaskID;
	pTask->m_reqId = INVALID_PSN_REQUEST_ID;
}

const char* CCryPSNMatchMaking::SSession::GetLocalUserName(uint32 localUserIndex) const
{
	if (localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		for (uint32 memberIndex = 0; memberIndex < MAX_PSN_ROOM_MEMBERS; ++memberIndex)
		{
			const SRoomMember* pRoomMember = &m_members[memberIndex];
			if ((pRoomMember->m_valid != ePSNMI_None) && (pRoomMember->m_memberId == m_myMemberId))
			{
				return pRoomMember->m_npId.handle.data;
			}
		}
	}

	return NULL;
}

void CCryPSNMatchMaking::SSession::Reset()
{
	PARENT::Reset();

	started = false;
	remoteConnectionProcessingToDo = false;
	remoteConnectionTaskID = CryMatchMakingInvalidTaskID;
	hostConnectionID = CryMatchMakingInvalidConnectionID;
	unregisteredConnectionsKicked = false;

	m_serverId = INVALID_PSN_SERVER_ID;
	m_worldId = INVALID_PSN_WORLD_ID;
	m_roomId = INVALID_PSN_ROOM_ID;
	memset(&m_sessionId, 0, sizeof(SceNpSessionId));

	m_myMemberId = INVALID_PSN_ROOM_MEMBER_ID;
	m_ownerMemberId = INVALID_PSN_ROOM_MEMBER_ID;
}

const char* CCryPSNMatchMaking::GetConnectionName(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const
{
	SSession::SRConnection* pPlatformConnection = reinterpret_cast<SSession::SRConnection*>(pConnection);

	if (pPlatformConnection->uid.m_uid == DEDICATED_SERVER_CONNECTION_UID)
	{
		return "DedicatedServer";
	}

	for (uint32 sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
	{
		const SSession* pSession = &m_sessions[sessionIndex];

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			for (uint32 memberIndex = 0; memberIndex < MAX_PSN_ROOM_MEMBERS; ++memberIndex)
			{
				const SSession::SRoomMember* pRoomMember = &pSession->m_members[memberIndex];
				if ((pRoomMember->m_valid != ePSNMI_None) && (pRoomMember->m_memberId == pPlatformConnection->memberId))
				{
					return pRoomMember->m_npId.handle.data;
				}
			}
		}
	}

	return NULL;
}

void CCryPSNMatchMaking::StartTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* task = &m_task[mmTaskID];

	if (task->used)
	{
			#if ENABLE_CRYLOBBY_DEBUG_TESTS
		ECryLobbyError error;

		if (!m_lobby->DebugOKToStartTaskRunning(task->lTaskID))
		{
			return;
		}

		if (m_lobby->DebugGenerateError(task->lTaskID, error))
		{
			UpdateTaskError(mmTaskID, error);
			StopTaskRunning(mmTaskID);

			if ((task->startedTask == eT_SessionCreate) || (task->startedTask == eT_SessionJoin))
			{
				FreeSessionHandle(task->session);
			}

			return;
		}
			#endif

		task->running = true;

		switch (task->startedTask)
		{
		case eT_SessionDelete:
			StartSessionDelete(mmTaskID);
			break;

		case eT_SessionUpdateSlots:
		case eT_SessionStart:
		case eT_SessionEnd:
		case eT_SessionRegisterUserData:
			StopTaskRunning(mmTaskID);
			break;

		case eT_SessionSearch:
			StartSessionSearch(mmTaskID);
			break;

			#if NETWORK_HOST_MIGRATION
		case eT_SessionMigrateHostStart:
			HostMigrationStartNT(mmTaskID);
			break;
		case eT_SessionMigrateHostServer:
			HostMigrationServerNT(mmTaskID);
			break;
		case eT_SessionSendHostHintExternal:
			StartSessionSendHostHintExternal(mmTaskID);
			break;
			#endif
		}
	}
}

void CCryPSNMatchMaking::EndTask(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
		if (m_pPSNSupport->GetNetInfo().natStatus != m_oldNatType)
		{
			m_oldNatType = m_pPSNSupport->GetNetInfo().natStatus;

			UCryLobbyEventData eventData;
			SCryLobbyNatTypeData natTypeData;

			eventData.pNatTypeData = &natTypeData;

			switch (m_oldNatType)
			{
			default:
			case SCE_NP_MATCHING2_SIGNALING_NETINFO_NAT_STATUS_UNKNOWN:
				natTypeData.m_curState = eNT_Unknown;
				break;
			case SCE_NP_MATCHING2_SIGNALING_NETINFO_NAT_STATUS_TYPE:
				natTypeData.m_curState = eNT_Open;
				break;
			case SCE_NP_MATCHING2_SIGNALING_NETINFO_NAT_STATUS_TYPE2:
				natTypeData.m_curState = eNT_Moderate;
				break;
			case SCE_NP_MATCHING2_SIGNALING_NETINFO_NAT_STATUS_TYPE3:
				natTypeData.m_curState = eNT_Strict;
				break;
			}

			if (m_lobby)
			{
				m_lobby->DispatchEvent(eCLSE_NatType, eventData);
				m_lobby->SetNATType(natTypeData.m_curState);
			}
		}

			#if NETWORK_HOST_MIGRATION
		if (pTask->startedTask & HOST_MIGRATION_TASK_ID_FLAG)
		{
			m_sessions[pTask->session].hostMigrationInfo.m_taskID = CryLobbyInvalidTaskID;
		}
			#endif

		if (pTask->cb)
		{
			switch (pTask->startedTask)
			{
			case eT_SessionRegisterUserData:
				((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
				break;

			case eT_SessionSearch:
				EndSessionSearch(mmTaskID);
				break;

			case eT_SessionMigrate:
				((CryMatchmakingSessionCreateCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, m_sessions[pTask->session].localConnection.uid), pTask->cbArg);
				break;

			case eT_SessionCreate:
				EndSessionCreate(mmTaskID);
				break;

			case eT_SessionQuery:
				EndSessionQuery(mmTaskID);
				break;

			case eT_SessionGetUsers:
				((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
				break;

			case eT_SessionJoin:
				EndSessionJoin(mmTaskID);
				break;

			case eT_SessionSetAdvertisementData:
				EndSessionSetAdvertisementData(mmTaskID);
				break;

			case eT_SessionGetAdvertisementData:
				EndSessionGetAdvertisementData(mmTaskID);
				break;

			#if NETWORK_HOST_MIGRATION
			case eT_SessionSendHostHintExternal:
				EndSessionSendHostHintExternal(mmTaskID);
				break;
			#endif

			case eT_SessionUserData:
			case eT_SessionUpdate:
			case eT_SessionUpdateSlots:
			case eT_SessionStart:
			case eT_SessionEnd:
			case eT_SessionDelete:
			#if NETWORK_HOST_MIGRATION
			case eT_SessionMigrateHostStart:
			#endif
				((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
				break;
			}
		}

		if (pTask->canceled)
		{
			UpdateTaskError(mmTaskID, eCLE_InternalError);
		}

		if (pTask->error != eCLE_Success)
		{
			if ((pTask->startedTask == eT_SessionCreate) || (pTask->startedTask == eT_SessionJoin))
			{
				if (pTask->session != CryLobbyInvalidSessionHandle)
				{
					ClearSessionInfo(pTask->session);
					FreeSessionHandle(pTask->session);
					pTask->session = CryLobbyInvalidSessionHandle;
				}
			}

			NetLog("[Lobby] EndTask %d (%d) error %d", pTask->startedTask, pTask->subTask, pTask->error);
		}

		// Clear PSN specific task state so that base class tasks can use this slot
		ResetTask(mmTaskID);

		FreeTask(mmTaskID);
	}
}

void CCryPSNMatchMaking::StopTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
		pTask->running = false;
		TO_GAME_FROM_LOBBY(&CCryPSNMatchMaking::EndTask, this, mmTaskID);
	}
}

ECryLobbyError CCryPSNMatchMaking::CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32* users, int numUsers, uint32 flags)
{
	ECryLobbyError error = CCryMatchMaking::CreateSessionHandle(h, host, flags, numUsers);

	if (error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[*h];

		for (uint32 j = 0; j < numUsers; j++)
		{
			pSession->localConnection.users[j] = users[j];
		}

		ClearSessionInfo(*h);
	}

	return error;
}

void CCryPSNMatchMaking::Tick(CTimeValue tv)
{
	CCryMatchMaking::Tick(tv);

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		STask* pTask = &m_task[i];

			#if ENABLE_CRYLOBBY_DEBUG_TESTS
		if (pTask->used)
		{
			if (m_lobby->DebugTickCallStartTaskRunning(pTask->lTaskID))
			{
				StartTaskRunning(i);
				continue;
			}
			if (!m_lobby->DebugOKToTickTask(pTask->lTaskID, pTask->running))
			{
				continue;
			}
		}
			#endif

		if (pTask->used && pTask->running)
		{
			if (pTask->error == eCLE_Success)
			{
				switch (pTask->subTask)
				{
				case eT_SessionQuery:
					TickSessionQuery(i);
					break;

				case eT_SessionUpdate:
					TickSessionUpdate(i);
					break;

				case eT_SessionUserData:
					TickSessionUserData(i);
					break;

				case eT_SessionRegisterUserData:
				case eT_SessionStart:
				case eT_SessionEnd:
					break;

				case eST_WaitingForJoinRoomSignalingCallback:
					{
						TickSessionJoinSignaling(i);
					}
					break;

				case eST_WaitingForCreateRoomSignalingCallback:
					{
						TickSessionCreateSignaling(i);
					}
					break;

				case eT_SessionSearch:
					TickSessionSearch(i);
					break;
				case eST_SessionSearchRequest:
					TickSessionSearchRequest(i);
					break;
				case eST_SessionSearchKickQOS:
					TickSessionQOSSearch(i);
					break;
				case eST_WaitingForSearchQOSRequestCallback:
					TickSessionWaitForQOS(i);
					break;

				case eT_SessionCreate:
					TickSessionCreate(i);
					break;
				case eST_SessionCreateRequest:
					TickSessionCreateRequest(i);
					break;

				case eT_SessionMigrate:
					TickSessionMigrate(i);
					break;

				case eT_SessionJoin:
					TickSessionJoin(i);
					break;

				case eT_SessionGetUsers:
					TickSessionGetUsers(i);
					break;

				case eT_SessionDelete:
					TickSessionDelete(i);
					break;

				case eT_SessionSetAdvertisementData:
					TickSessionSetAdvertisementData(i);
					break;

				case eT_SessionGetAdvertisementData:
					TickSessionGetAdvertisementData(i);
					break;

			#if NETWORK_HOST_MIGRATION
				case eT_SessionMigrateHostStart:
					TickHostMigrationStartNT(i);
					break;

				case eT_SessionMigrateHostServer:
					TickHostMigrationServerNT(i);
					break;

				case eT_SessionSendHostHintExternal:
					TickSessionSendHostHintExternal(i);
					break;
			#endif
				default:
					TickBaseTask(i);
					break;
				}
			}

			if (pTask->error != eCLE_Success)
			{
				StopTaskRunning(i);
			}
		}
	}

	UpdateVoiceUsers();

	const int psnShowRooms = CLobbyCVars::Get().psnShowRooms;
	if ((psnShowRooms != 0) && gEnv->pRenderer)
	{
		static float yellow[] = { 1.0f, 1.0f, 0.4f, 1.0f };
		f32 ypos = 35.0f;

		for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
		{
			SSession* pSession = &m_sessions[i];
			if (pSession->IsUsed())
			{
				f32 xpos = 50.0f + (250.0f * i);

				//				gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.5f, yellow, false, "S%d-W%d-R%lld",
				//					pSession->m_serverId, pSession->m_worldId, pSession->m_roomId);
			}
		}
	}
}

ECryLobbyError CCryPSNMatchMaking::SessionRegisterUserData(SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	if (numData < MAX_MATCHMAKING_SESSION_USER_DATA)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionRegisterUserData, false, 0, &mmTaskID, taskID, 0, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			memcpy(m_registeredUserData.data, data, numData * sizeof(data[0]));
			m_registeredUserData.num = numData;

			uint32 searchableCount = 0;
			uint32 binBucketOffset = 0;
			for (uint32 a = 0; a < numData; a++)
			{
				if ((m_registeredUserData.data[a].m_type == eCSUDT_Int32) && (searchableCount < SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM))
				{
					m_registeredUserData.mapping[a].integerField = 1;
					m_registeredUserData.mapping[a].fieldOffset = searchableCount;

					searchableCount++;
				}
				else
				{
					// Either we have run out of searchable integers or the data type is not 32bit
					m_registeredUserData.mapping[a].integerField = 0;
					m_registeredUserData.mapping[a].fieldOffset = binBucketOffset;
					switch (m_registeredUserData.data[a].m_type)
					{
					case eCSUDT_Int64:
					case eCSUDT_Int64NoEndianSwap:
					case eCSUDT_Float64:
						binBucketOffset += 8;
						break;
					case eCSUDT_Int32:
					case eCSUDT_Float32:
						binBucketOffset += 4;
						break;
					case eCSUDT_Int16:
						binBucketOffset += 2;
						break;
					case eCSUDT_Int8:
						binBucketOffset += 1;
						break;
					default:
						error = eCLE_UserDataTypeMissMatch;
						return error;
					}
					if (binBucketOffset > (SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM))
					{
						error = eCLE_OutOfSessionUserData;
						return error;
					}
				}
			}

			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_OutOfSessionUserData;
	}

	NetLog("[Lobby] Start SessionRegisterUserData error %d", error);

	return error;
}

uint32 CCryPSNMatchMaking::SortWorlds(const SCryPSNGetWorldInfoListResponse* pWorldList, SceNpMatching2WorldId* pReturnIdList, uint32 nMaxListSize, bool bIsForSearch)
{
	uint32 numWorldsSorted = 0;

	if (pReturnIdList && (nMaxListSize > 0))
	{
		for (uint32 w = 0; w < nMaxListSize; w++)
		{
			pReturnIdList[w] = INVALID_PSN_ROOM_ID;
		}

		uint32 numTotalWorlds = pWorldList ? pWorldList->m_numWorlds : 0;
		if (pWorldList && numTotalWorlds)
		{
			int forceWorld = CLobbyCVars::Get().psnForceWorld;
			if (forceWorld == 0)
			{
				//-- For worlds metrics, we have access to two values:
				//-- ~ number of rooms on a world. The more rooms on a world, the better list of search results we'll get back
				//-- ~ players-per-room ratio. The higher the ratio, the more likely it is that the world is hosting game sessions.

				uint32 mostRooms = 1;
				f32 mostPPRRatio = 1.0f;
				for (uint32 i = 0; i < numTotalWorlds; i++)
				{
					SCryPSNWorld& world = pWorldList->m_pWorlds[i];

					world.m_score = 0;
					world.m_sortState = SCryPSNWorld::SORT_WORLD_NONE;

					if (world.m_numRooms > 0)
					{
						if (world.m_numRooms > mostRooms)
						{
							mostRooms = world.m_numRooms;
						}

						f32 fRatio = (f32)world.m_numTotalRoomMembers / (f32)world.m_numRooms;
						if (fRatio > mostPPRRatio)
						{
							mostPPRRatio = fRatio;
						}
					}
				}

				f32 roomsBonus = (f32)CLobbyCVars::Get().psnBestWorldNumRoomsBonus;
				f32 roomsPCth = (f32)mostRooms * CLobbyCVars::Get().psnBestWorldNumRoomsPivot;
				f32 roomsRampUp = roomsBonus / roomsPCth;
				f32 roomsRampDown = 0.0f;
				if (CLobbyCVars::Get().psnBestWorldNumRoomsPivot < 1.0f)
				{
					roomsRampDown = (roomsBonus * 0.33f) / (mostRooms - roomsPCth);
				}

				f32 ratioBonus = (f32)CLobbyCVars::Get().psnBestWorldPlayersPerRoomBonus;
				f32 ratioPCth = mostPPRRatio * CLobbyCVars::Get().psnBestWorldPlayersPerRoomPivot;
				f32 ratioRampUp = ratioBonus / ratioPCth;
				f32 ratioRampDown = 0.0f;
				if (CLobbyCVars::Get().psnBestWorldPlayersPerRoomPivot < 1.0f)
				{
					ratioRampDown = (ratioBonus * 0.33f) / (mostPPRRatio - ratioPCth);
				}

				NetLog("SortWorlds: Room Setup: %d maximum, optimal is %.02f at position %.02f", mostRooms, roomsPCth, CLobbyCVars::Get().psnBestWorldNumRoomsPivot);
				NetLog("SortWorlds: PPR Setup: %.04f maximum, optimal is %.04f at position %.02f", mostPPRRatio, ratioPCth, CLobbyCVars::Get().psnBestWorldPlayersPerRoomPivot);

				uint32 maxRooms = max(0, min(CLobbyCVars::Get().psnBestWorldMaxRooms, MAX_ROOMS_PER_WORLD));
				uint32 maxBonus = CLobbyCVars::Get().psnBestWorldNumRoomsBonus
				                  + CLobbyCVars::Get().psnBestWorldPlayersPerRoomBonus
				                  + CLobbyCVars::Get().psnBestWorldRandomBonus
				                  + 1;

				for (uint32 i = 0; i < numTotalWorlds; i++)
				{
					uint32 scoreA = 0;
					uint32 scoreB = 0;
					uint32 scoreC = 0;
					f32 playersPerRoomRatio = 0.0f;

					SCryPSNWorld& world = pWorldList->m_pWorlds[i];

					if (world.m_numRooms > 0)
					{
						//-- This could probably do with some statistical study to provide a good world score,
						//-- but using only the number of rooms (as we have in the past) isn't necessarily going to
						//-- provide a good world (it might be full of squads).

						f32 curNumOfRoom = (f32)world.m_numRooms;
						if (curNumOfRoom <= roomsPCth)
						{
							scoreA = (uint32)(curNumOfRoom * roomsRampUp);
						}
						else
						{
							scoreA = (uint32)(roomsBonus - ((curNumOfRoom - roomsPCth) * roomsRampDown));
						}

						//-- And maybe some points for players per room density, which should skew the results towards
						//-- worlds with bigger rooms and hopefully those are more likely to contain game sessions and not squads.

						playersPerRoomRatio = (f32)world.m_numTotalRoomMembers / (f32)world.m_numRooms;
						if (playersPerRoomRatio <= ratioPCth)
						{
							scoreB = (uint32)(playersPerRoomRatio * ratioRampUp);
						}
						else
						{
							scoreB = (uint32)(ratioBonus - ((playersPerRoomRatio - ratioPCth) * ratioRampDown));
						}

						world.m_score = scoreA + scoreB;
					}

					if (bIsForSearch)
					{
						//-- if sorting is for a room search, only include worlds with a score > 0
						if (world.m_score > 0)
						{
							world.m_sortState = SCryPSNWorld::SORT_WORLD_NEEDS_SORTING;
						}
						//-- We also have to make sure that at least 1 world is returned, so lets use world[0].
						if (i == 0)
						{
							world.m_sortState = SCryPSNWorld::SORT_WORLD_NEEDS_SORTING;
						}

						//-- also add a slightly random element so there's a chance that the sorted world list can change
						//-- (only useful if we return a subset of the sorted worlds (eg: top 3) and want some variation, otherwise it's pointless.)
						//-- Since we return all worlds for a search this is no longer necessary. But might be in the future.
						//scoreC = (cry_rand() % CNetCVars::Get().psnBestWorldRandomBonus);
						//world.m_score += scoreC;
					}
					else
					{
						//-- if sorting is for a room create, only include worlds that are not full.
						if (world.m_numRooms < maxRooms)
						{
							world.m_sortState = SCryPSNWorld::SORT_WORLD_NEEDS_SORTING;
						}
					}

					NetLog("SortWorlds: World %02d : ID %d: Score = %d + %d + %d = %d (%d rooms ~ %.2f PPR)", i + 1, world.m_worldId,
					       scoreA, scoreB, scoreC, world.m_score, world.m_numRooms, playersPerRoomRatio);
				}

				//-- We have a set of valid worlds to sort now, so we just have to add them to the return list in the correct order.
				//-- It's a pretty bad selection sort algorithm, but it doesn't need to be hugely performance critical.
				bool bSorting = true;
				while (bSorting)
				{
					SCryPSNWorld* pBestWorld = NULL;
					uint32 highestWorldScore = 0;
					uint32 lowestWorldScore = maxBonus;

					for (uint32 l = 0; l < numTotalWorlds; l++)
					{
						SCryPSNWorld* pWorld = &pWorldList->m_pWorlds[l];

						if (pWorld->m_sortState == SCryPSNWorld::SORT_WORLD_NEEDS_SORTING)
						{
							if (bIsForSearch)
							{
								// search returns the highest scoring worlds first
								if (pWorld->m_score >= highestWorldScore)
								{
									highestWorldScore = pWorld->m_score;
									pBestWorld = pWorld;
								}
							}
							else
							{
								// create returns the lowest scoring worlds first.
								// I'm not sure this is really what we want...
								if (pWorld->m_score < lowestWorldScore)
								{
									lowestWorldScore = pWorld->m_score;
									pBestWorld = pWorld;
								}
							}
						}
					}

					if (pBestWorld)
					{
						pReturnIdList[numWorldsSorted] = pBestWorld->m_worldId;
						pBestWorld->m_sortState = SCryPSNWorld::SORT_WORLD_SORTED;

						numWorldsSorted++;
						if (numWorldsSorted >= nMaxListSize)
						{
							bSorting = false;
						}

						NetLog("SortWorlds: Sorted %02d : ID %d: Score = %d", numWorldsSorted, pBestWorld->m_worldId, pBestWorld->m_score);
					}
					else
					{
						//-- No more worlds to add to sorted list
						bSorting = false;
					}
				}
			}
			else
			{
				if ((forceWorld >= 1) && (forceWorld <= numTotalWorlds))
				{
					pReturnIdList[0] = pWorldList->m_pWorlds[forceWorld - 1].m_worldId;
				}
				else
				{
					pReturnIdList[0] = pWorldList->m_pWorlds[0].m_worldId;
				}
				numWorldsSorted = 1;
			}
		}
	}

	return numWorldsSorted;
}

void CCryPSNMatchMaking::ProcessExternalRoomData(const SCryPSNRoomDataExternal& curRoom, SCrySessionSearchResult& result, SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA])
{
	SCryPSNSessionID* pRoomInfo = new SCryPSNSessionID;

	pRoomInfo->m_sessionInfo.m_roomId = curRoom.m_roomId;
	pRoomInfo->m_sessionInfo.m_worldId = curRoom.m_worldId;
	pRoomInfo->m_sessionInfo.m_serverId = curRoom.m_serverId;
	pRoomInfo->m_sessionInfo.m_gameType = 0;
	memset(&pRoomInfo->m_sessionId, 0, sizeof(pRoomInfo->m_sessionId));
	pRoomInfo->m_fromInvite = false;

	result.m_id = pRoomInfo;

	result.m_numFilledSlots = curRoom.m_numPublicSlots - curRoom.m_numOpenPublicSlots;
	result.m_numFriends = 0;
	result.m_flags = 0;

	result.m_data.m_ranked = false;
	result.m_data.m_name[0] = 0;
	result.m_data.m_data = userData;
	result.m_data.m_numData = 0;

	result.m_data.m_numPublicSlots = curRoom.m_numPublicSlots;
	result.m_data.m_numPrivateSlots = curRoom.m_numPrivateSlots;

	for (uint32 k = 0; k < SCE_NP_ONLINEID_MAX_LENGTH; k++)
	{
		result.m_data.m_name[k] = (char)curRoom.m_owner.handle.data[k];
	}
	result.m_data.m_name[SCE_NP_ONLINEID_MAX_LENGTH] = 0;

	for (int a = 0; a < curRoom.m_numSearchableUIntAttributes; a++)
	{
		for (int b = 0; b < m_registeredUserData.num; b++)
		{
			if (m_registeredUserData.mapping[b].integerField)
			{
				if (curRoom.m_searchableUIntAttributes[a].m_id == m_registeredUserData.mapping[b].fieldOffset + SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID)
				{
					result.m_data.m_data[result.m_data.m_numData].m_id = m_registeredUserData.data[b].m_id;
					result.m_data.m_data[result.m_data.m_numData].m_type = m_registeredUserData.data[b].m_type;
					result.m_data.m_data[result.m_data.m_numData].m_int32 = curRoom.m_searchableUIntAttributes[a].m_num;

					result.m_data.m_numData++;
					break;
				}
			}
		}
	}

	char binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM];

	for (int a = 0; a < curRoom.m_numBinAttributes; a++)
	{
		memcpy(&binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * a], curRoom.m_binAttributes[a].m_data, curRoom.m_binAttributes[a].m_dataSize);
	}

	for (int b = 0; b < m_registeredUserData.num; b++)
	{
		if (!m_registeredUserData.mapping[b].integerField)
		{
			result.m_data.m_data[result.m_data.m_numData].m_id = m_registeredUserData.data[b].m_id;
			result.m_data.m_data[result.m_data.m_numData].m_type = m_registeredUserData.data[b].m_type;

			void* dPtr;
			switch (result.m_data.m_data[result.m_data.m_numData].m_type)
			{
			case eCSUDT_Int64:
			case eCSUDT_Int64NoEndianSwap:
			case eCSUDT_Float64:
				dPtr = (void*)&result.m_data.m_data[result.m_data.m_numData].m_int64;
				memcpy(dPtr, &binAttrExternalData[m_registeredUserData.mapping[b].fieldOffset], sizeof(result.m_data.m_data[result.m_data.m_numData].m_int64));
				break;
			case eCSUDT_Int32:
			case eCSUDT_Float32:
				dPtr = (void*)&result.m_data.m_data[result.m_data.m_numData].m_int32;
				memcpy(dPtr, &binAttrExternalData[m_registeredUserData.mapping[b].fieldOffset], sizeof(result.m_data.m_data[result.m_data.m_numData].m_int32));
				break;
			case eCSUDT_Int16:
				dPtr = (void*)&result.m_data.m_data[result.m_data.m_numData].m_int16;
				memcpy(dPtr, &binAttrExternalData[m_registeredUserData.mapping[b].fieldOffset], sizeof(result.m_data.m_data[result.m_data.m_numData].m_int16));
				break;
			case eCSUDT_Int8:
				dPtr = (void*)&result.m_data.m_data[result.m_data.m_numData].m_int8;
				memcpy(dPtr, &binAttrExternalData[m_registeredUserData.mapping[b].fieldOffset], sizeof(result.m_data.m_data[result.m_data.m_numData].m_int8));
				break;
			}
			result.m_data.m_numData++;
		}
	}
}

void CCryPSNMatchMaking::BuildUserSessionParams(SSession* pSession, SCrySessionUserData* pData, uint32 numData,
                                                SceNpMatching2BinAttr binAttrExternal[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM],
                                                SceNpMatching2IntAttr intAttrExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM],
                                                char binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM])
{
	// Setup our advertiser data
	for (int a = 0; a < SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM; a++)
	{
		binAttrExternal[a].id = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_1_ID + a;
		binAttrExternal[a].ptr = &binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * a];
		binAttrExternal[a].size = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE;   // should really shrink these to avoid sending unused data
	}

	for (int a = 0; a < SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM; a++)
	{
		intAttrExternal[a].id = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID + a;
	}

	// Build user data into psn format
	if (pSession->IsUsed())
	{
		for (uint32 i = 0; i < numData; i++)
		{
			uint32 j;

			for (j = 0; j < m_registeredUserData.num; j++)
			{
				if (pData[i].m_id == m_registeredUserData.data[j].m_id)
				{
					if (pData[i].m_type == m_registeredUserData.data[j].m_type)
					{
						if (m_registeredUserData.mapping[j].integerField)
						{
							intAttrExternal[m_registeredUserData.mapping[j].fieldOffset].num = pData[i].m_int32;
						}
						else
						{
							void* dPtr;
							switch (pData[i].m_type)
							{
							case eCSUDT_Int64:
							case eCSUDT_Int64NoEndianSwap:
							case eCSUDT_Float64:
								dPtr = (void*)&pData[i].m_int64;
								memcpy(&binAttrExternalData[m_registeredUserData.mapping[j].fieldOffset], dPtr, sizeof(pData[i].m_int64));
								break;
							case eCSUDT_Int32:
							case eCSUDT_Float32:
								dPtr = (void*)&pData[i].m_int32;
								memcpy(&binAttrExternalData[m_registeredUserData.mapping[j].fieldOffset], dPtr, sizeof(pData[i].m_int32));
								break;
							case eCSUDT_Int16:
								dPtr = (void*)&pData[i].m_int16;
								memcpy(&binAttrExternalData[m_registeredUserData.mapping[j].fieldOffset], dPtr, sizeof(pData[i].m_int16));
								break;
							case eCSUDT_Int8:
								dPtr = (void*)&pData[i].m_int8;
								memcpy(&binAttrExternalData[m_registeredUserData.mapping[j].fieldOffset], dPtr, sizeof(pData[i].m_int8));
								break;
							}
						}
					}
				}
			}
		}
	}
}

// SESSION CREATE -----------------------------------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionCreate(uint32* users, int numUsers, uint32 flags, SCrySessionData* data, CryLobbyTaskID* taskID, CryMatchmakingSessionCreateCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;
	ECryLobbyError error = CreateSessionHandle(&h, true, users, numUsers, flags);

	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionCreate, false, users[0], &mmTaskID, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			SSession* pSession = &m_sessions[h];

			pSession->m_flags = SCE_NP_MATCHING2_ROOM_FLAG_ATTR_OWNER_AUTO_GRANT;     // Auto migrate

			if ((flags & CRYSESSION_CREATE_FLAG_SEARCHABLE) == 0)
			{
				pSession->m_flags |= SCE_NP_MATCHING2_ROOM_FLAG_ATTR_HIDDEN;
			}

			if (data->m_ranked)
			{
				pSession->m_flags |= 0;   // TODO.. implement ranked rooms
				pSession->m_gameType = 0;
			}
			else
			{
				pSession->m_gameType = 0;
			}

			pSession->m_gameMode = 0;
			pSession->m_numPublicSlots = data->m_numPublicSlots;
			pSession->m_numPrivateSlots = data->m_numPrivateSlots;

			NetLog("[Lobby] Created local connection " PRFORMAT_SH " " PRFORMAT_UID, PRARG_SH(h), PRARG_UID(pSession->localConnection.uid));

			for (uint32 i = 0; i < numUsers; i++)
			{
				pSession->localConnection.privateSlot[i] = TRUE;
			}

			error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_CREATE_PARAMS_SLOT, data, 1, sizeof(SCrySessionData));

			if (error == eCLE_Success)
			{
				error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_CREATE_DATA_SLOT, data->m_data, data->m_numData, data->m_numData * sizeof(data->m_data[0]));

				if (error == eCLE_Success)
				{
					error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT, NULL, 1, sizeof(SCreateParamData));

					if (error == eCLE_Success)
					{
						// This next alloc is a doozy - 165KB-ish
						error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_WEBAPI_CREATE_SLOT, NULL, 1, sizeof(SCryPSNOrbisWebApiCreateSessionInput));
						if (error == eCLE_Success)
						{
							m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

							FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
						}
					}
				}
			}

			if (error != eCLE_Success)
			{
				FreeTask(mmTaskID);
				FreeSessionHandle(h);
			}
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	NetLog("[Lobby] Start SessionCreate error %d", error);

	return error;
}

void CCryPSNMatchMaking::TickSessionCreate(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		//-- get the world list
		StartSubTask(eST_WaitingForCreateRoomWorldInfo, mmTaskID);

		SceNpMatching2GetWorldInfoListRequest reqParam;
		memset(&reqParam, 0, sizeof(reqParam));
		reqParam.serverId = m_pPSNSupport->GetServerInfo()->server.serverId;

		int ret = sceNpMatching2GetWorldInfoList(m_pPSNSupport->GetMatchmakingContextId(), &reqParam, NULL, &pTask->m_reqId);
		if (ret == PSN_OK)
		{
			//-- Waiting for callback
		}
		else
		{
			NetLog("sceNpMatching2GetWorldInfoList : error %08X", ret);
			if (!m_pPSNSupport->HandlePSNError(ret))
			{
				UpdateTaskError(mmTaskID, eCLE_WorldNotDefined);
			}
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_GetWorldInfoListForCreate(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	assert(pTask->subTask == eST_WaitingForCreateRoomWorldInfo);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_GET_WORLD_INFO_LIST);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNGetWorldInfoListResponse));

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNGetWorldInfoListResponse)))
	{
		SCreateParamData* pCreateParams = (SCreateParamData*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT]);
		if (pCreateParams)
		{
			memset(pCreateParams, 0, sizeof(SCreateParamData));

			const SCryPSNGetWorldInfoListResponse* pWorldList = (SCryPSNGetWorldInfoListResponse*)_this->m_lobby->MemGetPtr(data.m_requestEvent.m_dataHdl);

			uint32 listSize = SortWorlds(pWorldList, &pCreateParams->m_createRequest.worldId, 1, false);
			if ((listSize > 0) && (pCreateParams->m_createRequest.worldId != INVALID_PSN_WORLD_ID))
			{
				_this->StartSubTask(eST_SessionCreateRequest, taskId);
			}
			else
			{
				_this->UpdateTaskError(taskId, eCLE_WorldNotDefined);
			}
		}
		else
		{
			_this->UpdateTaskError(taskId, eCLE_OutOfMemory);
		}

		return true;
	}
	else
	{
		return false;
	}
}

// At present there are only 8 possible searchable integer values (so we need to be careful with which attributes use them.
// For now I have chucked all the data into the 2 binary attributes (256 bytes each). If I run out, i`ll panic later.
void CCryPSNMatchMaking::TickSessionCreateRequest(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_CREATE_PARAMS_SLOT] != TMemInvalidHdl)
		{
			if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT] != TMemInvalidHdl)
			{
				SCrySessionData* pData = (SCrySessionData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_CREATE_PARAMS_SLOT]);
				if (pTask->params[PSN_MATCHMAKING_CREATE_DATA_SLOT] != TMemInvalidHdl)
				{
					pData->m_data = (SCrySessionUserData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_CREATE_DATA_SLOT]);
				}
				else
				{
					pData->m_data = NULL;
				}

				SCreateParamData* pCreateParams = (SCreateParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT]);
				SSession* pSession = &m_sessions[pTask->session];

				BuildUserSessionParams(pSession, pData->m_data, pData->m_numData, pCreateParams->m_binAttrExternal, pCreateParams->m_intAttrExternal, pCreateParams->m_binAttrExternalData);

				// Internal data defaults
				for (int32 index = 0; index < SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_NUM; ++index)
				{
					pCreateParams->m_binAttrInternal[index].id = SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_1_ID + index;
					pCreateParams->m_binAttrInternal[index].ptr = &pCreateParams->m_binAttrInternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_MAX_SIZE * index];
					pCreateParams->m_binAttrInternal[index].size = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE;    // should really shrink these to avoid sending unused data
				}

				// Build internal data
				const int numInternalData = 1;
				uint16 sessionCreateGameFlags = (pSession->createFlags & CRYSESSION_CREATE_FLAG_GAME_MASK) >> CRYSESSION_CREATE_FLAG_GAME_FLAGS_SHIFT;
				memcpy((void*)pCreateParams->m_binAttrInternal[0].ptr, &sessionCreateGameFlags, sizeof(sessionCreateGameFlags));
				pCreateParams->m_binAttrInternal[0].size = sizeof(sessionCreateGameFlags);

				SceNpMatching2SignalingOptParam sigParams;
				sigParams.type = SCE_NP_MATCHING2_SIGNALING_TYPE_MESH;
				sigParams.flag = 0;
				sigParams.hubMemberId = 0;

				pCreateParams->m_createRequest.maxSlot = pSession->m_numPrivateSlots + pSession->m_numPublicSlots;
				pCreateParams->m_createRequest.flagAttr = pSession->m_flags;
				pCreateParams->m_createRequest.roomPassword = NULL;
				pCreateParams->m_createRequest.groupConfig = NULL;
				pCreateParams->m_createRequest.groupConfigNum = 0;
				pCreateParams->m_createRequest.passwordSlotMask = NULL;
				pCreateParams->m_createRequest.allowedUser = NULL;
				pCreateParams->m_createRequest.allowedUserNum = 0;
				pCreateParams->m_createRequest.blockedUser = NULL;
				pCreateParams->m_createRequest.blockedUserNum = 0;
				pCreateParams->m_createRequest.sigOptParam = &sigParams;
				pCreateParams->m_createRequest.roomBinAttrInternal = pCreateParams->m_binAttrInternal;
				pCreateParams->m_createRequest.roomBinAttrInternalNum = numInternalData;
				pCreateParams->m_createRequest.roomBinAttrExternal = pCreateParams->m_binAttrExternal;
				pCreateParams->m_createRequest.roomBinAttrExternalNum = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM;
				pCreateParams->m_createRequest.roomSearchableBinAttrExternal = NULL;
				pCreateParams->m_createRequest.roomSearchableBinAttrExternalNum = 0;
				pCreateParams->m_createRequest.roomSearchableIntAttrExternal = pCreateParams->m_intAttrExternal;
				pCreateParams->m_createRequest.roomSearchableIntAttrExternalNum = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM;
				pCreateParams->m_createRequest.joinRoomGroupLabel = NULL;
				pCreateParams->m_createRequest.roomMemberBinAttrInternal = NULL;
				pCreateParams->m_createRequest.roomMemberBinAttrInternalNum = 0;
				pCreateParams->m_createRequest.teamId = 0;

				StartSubTask(eST_WaitingForCreateRoomRequestCallback, mmTaskID);

				int ret = sceNpMatching2CreateJoinRoom(m_pPSNSupport->GetMatchmakingContextId(), &pCreateParams->m_createRequest, NULL, &pTask->m_reqId);
				if (ret == PSN_OK)
				{
					//-- waiting for callback
				}
				else
				{
					NetLog("sceNpMatching2CreateJoinRoom : error %08X", ret);
					//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
					//-- otherwise, we have to update the task error ourself.
					if (!m_pPSNSupport->HandlePSNError(ret))
					{
						UpdateTaskError(mmTaskID, eCLE_InternalError);
					}
				}
			}
			else
			{
				UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_CreateRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForCreateRoomRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_CREATE_JOIN_ROOM);
	assert(data.m_requestEvent.m_dataHdl != TMemInvalidHdl);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNCreateJoinRoomResponse));

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNCreateJoinRoomResponse)))
	{
		const SCryPSNCreateJoinRoomResponse* pCreateResponse = (SCryPSNCreateJoinRoomResponse*)_this->m_pPSNSupport->GetLobby()->MemGetPtr(data.m_requestEvent.m_dataHdl);

		if (pTask->session != CryLobbyInvalidSessionHandle)
		{
			SSession* pSession = &_this->m_sessions[pTask->session];
			if (pSession->IsUsed())
			{
				pSession->m_roomId = pCreateResponse->m_roomInfo.m_roomId;
				pSession->m_worldId = pCreateResponse->m_roomInfo.m_worldId;
				pSession->m_serverId = pCreateResponse->m_roomInfo.m_serverId;

				if (!pTask->canceled)
				{
					_this->AddInitialRoomMembers(pTask->session, pCreateResponse->m_roomMembers);
					_this->StartSubTask(eST_WaitingForCreateRoomSignalingCallback, taskId);
				}
				else
				{
					_this->UpdateTaskError(taskId, eCLE_IllegalSessionJoin);
				}
			}
			else
			{
				_this->UpdateTaskError(taskId, eCLE_InternalError);
			}
		}
		else
		{
			_this->UpdateTaskError(taskId, eCLE_InternalError);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void CCryPSNMatchMaking::TickSessionCreateSignaling(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		UpdateSignaling(mmTaskID);

		switch (CheckSignaling(mmTaskID))
		{
		case ePSNCS_None:
			{
				//-- This is bad, means there is no room owner!
				UpdateTaskError(mmTaskID, eCLE_InvalidParam);
			}
			break;

		case ePSNCS_Active:
			{
				assert(pTask->subTask == eST_WaitingForCreateRoomSignalingCallback);

				if (pTask->session != CryLobbyInvalidSessionHandle)
				{
					SSession* pSession = &m_sessions[pTask->session];
					if (pSession->IsUsed())
					{
						if (!pTask->canceled)
						{
							if (pSession->createFlags & CRYSESSION_CREATE_FLAG_INVITABLE)
							{
								SCrySessionData* pSessionData = (SCrySessionData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_CREATE_PARAMS_SLOT]);
								SCreateParamData* pCreateParams = (SCreateParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_CREATE_REQUEST_SLOT]);
								SCryPSNOrbisWebApiCreateSessionInput* pWebApiCreateParams = (SCryPSNOrbisWebApiCreateSessionInput*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_WEBAPI_CREATE_SLOT]);
								if (pSessionData && pCreateParams && pWebApiCreateParams)
								{
									StartSubTask(eST_WaitingForCreateRoomWebApiCallback, mmTaskID);

									memset(pWebApiCreateParams, 0, sizeof(SCryPSNOrbisWebApiCreateSessionInput));

									// Some of the SCryLobbySessionAdvertisement we can fill in now.
									SCryLobbySessionAdvertisement& advertisement = pWebApiCreateParams->advertisement;

									advertisement.m_numSlots = pSessionData->m_numPublicSlots + pSessionData->m_numPrivateSlots;
									advertisement.m_bIsPrivate = false;
									advertisement.m_bIsEditableByAll = true;
									advertisement.m_pData = pWebApiCreateParams->data;

									if (pTask->session != CryLobbyInvalidSessionHandle)
									{
										// default session advertisement data should be set to the matching2 room info if available
										SSession* pSession = &m_sessions[pTask->session];
										SSynchronisedSessionID* pSessionInfo = (SSynchronisedSessionID*)advertisement.m_pData;

										pSessionInfo->m_serverId = pSession->m_serverId;
										pSessionInfo->m_worldId = pSession->m_worldId;
										pSessionInfo->m_roomId = pSession->m_roomId;
										pSessionInfo->m_gameType = pSession->m_gameType;
										advertisement.m_sizeofData = sizeof(SSynchronisedSessionID);
									}
									else
									{
										// otherwise just set the session advertisement data to an empty chunk of maximum size
										advertisement.m_sizeofData = CRY_WEBAPI_CREATESESSION_LINKDATA_MAX_SIZE;
									}

									// The rest needs filling in and/or modifying via configuration callback to the game.
									SConfigurationParams neededInfo = {
										CLCC_PSN_CREATE_SESSION_ADVERTISEMENT, { NULL }
									};
									neededInfo.m_pData = (void*)&pWebApiCreateParams->advertisement;
									m_lobby->GetConfigurationInformation(&neededInfo, 1);

									// And now we should have a completed SCryLobbySessionAdvertisement structure available to us. We'll be using this in CCryPSNOrbisWebApiThread::CreateSession later on.
									pCreateParams->m_createJobId = m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::CreateSession, pTask->params[PSN_MATCHMAKING_WEBAPI_CREATE_SLOT]);
									if (pCreateParams->m_createJobId == INVALID_WEBAPI_JOB_ID)
									{
										// failed
										UpdateTaskError(mmTaskID, eCLE_InternalError);
									}
								}
								else
								{
									UpdateTaskError(mmTaskID, eCLE_InternalError);
								}
							}
							else
							{
								// Session is not invitable, so we don't need the session Id.
								UpdateTaskError(mmTaskID, eCLE_Success);
								StopTaskRunning(mmTaskID);
							}
						}
						else
						{
							// cancelled
							UpdateTaskError(mmTaskID, eCLE_IllegalSessionJoin);
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
			}
			break;

		case ePSNCS_Dead:
			{
				assert(pTask->subTask == eST_WaitingForCreateRoomSignalingCallback);

				UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			}
			break;
		}
	}
}

void CCryPSNMatchMaking::EventWebApiCreateSession(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->session != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[pTask->session];

		memset(&pSession->m_sessionId, 0, sizeof(SceNpSessionId));

		if (data.m_webApiEvent.m_error == PSN_OK)
		{
			if (data.m_webApiEvent.m_pResponseBody)
			{
				if (data.m_webApiEvent.m_pResponseBody->eType == SCryPSNWebApiResponseBody::E_JSON)
				{
					// parse json to get session ID
					// CCryPSNWebApiJobController::PrintResponseJSONTree(data.m_webApiEvent.m_pResponseBody);
					const sce::Json::Value& val = data.m_webApiEvent.m_pResponseBody->jsonTreeRoot["sessionId"];
					if (val.getType() == sce::Json::kValueTypeString)
					{
						strncpy((char*)&pSession->m_sessionId, val.toString().c_str(), sizeof(pSession->m_sessionId.data));
						pSession->m_sessionId.term = 0;

						// Completed successfully
						UpdateTaskError(mmTaskID, eCLE_Success);
						StopTaskRunning(mmTaskID);

						return;
					}
				}
			}
		}

		// something went wrong
		UpdateTaskError(mmTaskID, eCLE_InternalError);
		StopTaskRunning(mmTaskID);
	}

	// otherwise something went wrong!
	UpdateTaskError(mmTaskID, eCLE_InternalError);
	StopTaskRunning(mmTaskID);
}

void CCryPSNMatchMaking::EndSessionCreate(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	if (pTask->error == eCLE_Success)
	{
		SCryMatchMakingConnectionUID matchingUID;
		matchingUID.m_uid = pSession->m_myMemberId;
		matchingUID.m_sid = pSession->m_roomId;

		pSession->localConnection.uid = matchingUID;
		pSession->localConnection.pingToServer = CRYLOBBY_INVALID_PING;
		pSession->localConnection.used = true;
		pSession->hostConnectionID = CryMatchMakingInvalidConnectionID;
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
	}

	((CryMatchmakingSessionCreateCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, pSession->localConnection.uid), pTask->cbArg);

	if (pTask->error == eCLE_Success)
	{
		InitialUserDataEvent(pTask->session);
	}
}

// END SESSON CREATE ---------------------------------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionMigrate(CrySessionHandle gh, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	// Because we simply want to re-use the session that is already available, we don't need to do much here

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionMigrate, false, pSession->localConnection.users[0], &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionMigrate error %d", error);

	return error;
}

void CCryPSNMatchMaking::TickSessionMigrate(CryMatchMakingTaskID mmTaskID)
{
	UpdateTaskError(mmTaskID, eCLE_Success);
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryPSNMatchMaking::SessionUpdate(CrySessionHandle gh, SCrySessionUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* session = &m_sessions[h];

		if (session->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CryMatchMakingTaskID mmTaskID;

			error = StartTask(eT_SessionUpdate, false, session->localConnection.users[0], &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

			if (error == eCLE_Success)
			{
				STask* task = &m_task[mmTaskID];

				error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_UPDATE_DATA_SLOT, pData, numData, numData * sizeof(pData[0]));

				if (error == eCLE_Success)
				{
					error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_UPDATE_REQUEST_SLOT, NULL, 1, sizeof(SUpdateParamData));
					if (error == eCLE_Success)
					{
						m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

						FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
					}
					else
					{
						FreeTask(mmTaskID);
					}
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

	NetLog("[Lobby] Start SessionUpdate error %d", error);

	return error;
}

void CCryPSNMatchMaking::TickSessionUpdate(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_UPDATE_REQUEST_SLOT] != TMemInvalidHdl)
		{
			SUpdateParamData* pUpdateParams = (SUpdateParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_UPDATE_REQUEST_SLOT]);
			SCrySessionUserData* pData = (SCrySessionUserData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_UPDATE_DATA_SLOT]);
			SSession* pSession = &m_sessions[pTask->session];

			memset(pUpdateParams, 0, sizeof(SUpdateParamData));

			BuildUserSessionParams(pSession, pData, pTask->numParams[0], pUpdateParams->m_binAttrExternal, pUpdateParams->m_intAttrExternal, pUpdateParams->m_binAttrExternalData);

			pUpdateParams->m_updateRequest.roomId = pSession->m_roomId;

			pUpdateParams->m_updateRequest.roomSearchableIntAttrExternal = pUpdateParams->m_intAttrExternal;
			pUpdateParams->m_updateRequest.roomSearchableBinAttrExternal = NULL;
			pUpdateParams->m_updateRequest.roomBinAttrExternal = pUpdateParams->m_binAttrExternal;

			pUpdateParams->m_updateRequest.roomSearchableBinAttrExternalNum = 0;
			pUpdateParams->m_updateRequest.roomSearchableIntAttrExternalNum = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM;
			pUpdateParams->m_updateRequest.roomBinAttrExternalNum = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM;

			StartSubTask(eST_WaitingForUpdateRoomRequestCallback, mmTaskID);

			int ret = sceNpMatching2SetRoomDataExternal(m_pPSNSupport->GetMatchmakingContextId(), &pUpdateParams->m_updateRequest, NULL, &pTask->m_reqId);
			if (ret == PSN_OK)
			{
				//-- Waiting for callback
			}
			else
			{
				NetLog("sceNpMatching2SetRoomDataExternal : error %08X", ret);
				if (!m_pPSNSupport->HandlePSNError(ret))
				{
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_SetRoomDataExternal(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForUpdateRoomRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_SET_ROOM_DATA_EXTERNAL);
	assert(data.m_requestEvent.m_dataSize == 0);

	if (data.m_requestEvent.m_error == 0)
	{
		_this->UpdateTaskError(taskId, eCLE_Success);
		_this->StopTaskRunning(taskId);

		return true;
	}
	else
	{
		// return false to let the default error handler work
		return false;
	}
}

// END SESSION UPDATE ------------------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionUpdateSlots(CrySessionHandle gh, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionUpdateSlots, false, m_sessions[h].localConnection.users[0], &tid, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionUpdateSlots return %d", error);

	return error;
}

			#if NETWORK_HOST_MIGRATION
ECryLobbyError CCryPSNMatchMaking::SendHostHintExternal(CryLobbySessionHandle h)
{
	ECryLobbyError error = eCLE_Success;
	LOBBY_AUTO_LOCK;

	if (net_enable_psn_hinting)
	{
		if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
		{
			SSession* pSession = &m_sessions[h];

			if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
			{
				CryMatchMakingTaskID mmTaskID;

				bool taskRunning = false;
				for (uint32 taskID = 0; taskID < MAX_MATCHMAKING_TASKS; ++taskID)
				{
					STask* pTask = &m_task[taskID];

					if (pTask->used && pTask->running && (pTask->startedTask == eT_SessionSendHostHintExternal) && (pTask->session == h))
					{
				#if NET_HOST_HINT_DEBUG
						NetLog("[Host Hints]: hints already in flight to PSN, will resend when done");
				#endif // #if NET_HOST_HINT_DEBUG
						pTask->numParams[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_RESTART] = eHHTB_REQUIRES_RESTART;
						taskRunning = true;
						break;
					}
				}

				if (!taskRunning)
				{
					error = StartTask(eT_SessionSendHostHintExternal, false, pSession->localConnection.users[0], &mmTaskID, NULL, h, NULL, NULL);
					if (error == eCLE_Success)
					{
						STask* pTask = &m_task[mmTaskID];
						pTask->numParams[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_RESTART] = eHHTB_NORMAL;

						error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_REQUEST_SLOT, NULL, 1, sizeof(SHostHintParamData));

						if (error == eCLE_Success)
						{
							m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

							FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
						}
					}
					else
					{
						error = eCLE_InternalError;
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
	}

	NetLog("[Host Hints] Start SendHostHintExternal error %d", error);

	return error;
}

void CCryPSNMatchMaking::StartSessionSendHostHintExternal(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_REQUEST_SLOT] != TMemInvalidHdl)
	{
		SHostHintParamData* pHostHintParams = (SHostHintParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_REQUEST_SLOT]);
		memset(pHostHintParams, 0, sizeof(SHostHintParamData));
		SSession* pSession = &m_sessions[pTask->session];

		for (uint32 index = 0; index < pSession->newHostPriorityCount; ++index)
		{
			pHostHintParams->m_memberId[index] = ((CCryPSNMatchMaking::SSession::SRConnection*)(pSession->newHostPriority[index]))->memberId;
		}

		pHostHintParams->m_reqParam.roomId = pSession->m_roomId;
		pHostHintParams->m_reqParam.ownerPrivilegeRank = pHostHintParams->m_memberId;
		pHostHintParams->m_reqParam.ownerPrivilegeRankNum = pSession->newHostPriorityCount;

		NetLog("[Host Hints]: PSN SendHostHintExternal " PRFORMAT_SH " roomId 0x%llX Rank[0] %d RankNum %d",
		       PRARG_SH(pTask->session), (uint64)pHostHintParams->m_reqParam.roomId, pHostHintParams->m_reqParam.ownerPrivilegeRank[0], pHostHintParams->m_reqParam.ownerPrivilegeRankNum);

		m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
	}

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::TickSessionSendHostHintExternal(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_REQUEST_SLOT] != TMemInvalidHdl)
		{
			SHostHintParamData* pHostHintParams = (SHostHintParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_REQUEST_SLOT]);

			StartSubTask(eST_WaitingForNewHostHintCallback, mmTaskID);

			int ret = sceNpMatching2SetRoomDataInternal(m_pPSNSupport->GetMatchmakingContextId(), &pHostHintParams->m_reqParam, NULL, &pTask->m_reqId);
			if (ret == PSN_OK)
			{
				//-- waiting for callback
			}
			else
			{
				NetLog("sceNpMatching2SetRoomDataInternal : error %08X", ret);
				//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
				//-- otherwise, we have to update the task error ourself.
				if (!m_pPSNSupport->HandlePSNError(ret))
				{
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_SetRoomDataInternal(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForNewHostHintCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_SET_ROOM_DATA_INTERNAL);
	assert(data.m_requestEvent.m_dataSize == 0);

	if (data.m_requestEvent.m_error == 0)
	{
		_this->UpdateTaskError(taskId, eCLE_Success);
		_this->StopTaskRunning(taskId);

		return true;
	}
	else
	{
		// return false to let the default error handler work
		return false;
	}
}

void CCryPSNMatchMaking::EndSessionSendHostHintExternal(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	if (pTask->numParams[PSN_MATCHMAKING_PSNSUPPORT_HOSTHINT_RESTART] == eHHTB_REQUIRES_RESTART)
	{
				#if NET_HOST_HINT_DEBUG
		NetLog("[Host Hints]: hints changed whilst informing PSN, resending hints to PSN");
				#endif // #if NET_HOST_HINT_DEBUG
		SendHostHintExternal(pTask->session);
	}
}
			#endif

ECryLobbyError CCryPSNMatchMaking::SessionQuery(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionQueryCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed())
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionQuery, false, pSession->localConnection.users[0], &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);
		if (error == eCLE_Success)
		{
			error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_QUERY_REQUEST_SLOT, NULL, 1, sizeof(SQueryParamData));
			if (error == eCLE_Success)
			{
				m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

				FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
			}
			else
			{
				FreeTask(mmTaskID);
			}
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Session Query Error %d", error);

	return error;
}

void CCryPSNMatchMaking::TickSessionQuery(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_QUERY_REQUEST_SLOT] != TMemInvalidHdl)
		{
			SQueryParamData* pQueryParams = (SQueryParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_QUERY_REQUEST_SLOT]);

			memset(pQueryParams, 0, sizeof(SQueryParamData));

			SSession* pSession = &m_sessions[pTask->session];

			pQueryParams->m_queryRequest.roomId = pQueryParams->m_roomIds;
			pQueryParams->m_queryRequest.attrId = pQueryParams->m_attrTable;
			pQueryParams->m_roomIds[0] = pSession->m_roomId;
			pQueryParams->m_queryRequest.roomIdNum = 1;
			pQueryParams->m_attrTable[0] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID;
			pQueryParams->m_attrTable[1] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_2_ID;
			pQueryParams->m_attrTable[2] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_3_ID;
			pQueryParams->m_attrTable[3] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_4_ID;
			pQueryParams->m_attrTable[4] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_5_ID;
			pQueryParams->m_attrTable[5] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_6_ID;
			pQueryParams->m_attrTable[6] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_7_ID;
			pQueryParams->m_attrTable[7] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_8_ID;
			pQueryParams->m_attrTable[8] = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_1_ID;
			pQueryParams->m_attrTable[9] = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_2_ID;
			pQueryParams->m_queryRequest.attrIdNum = 10;

			StartSubTask(eST_WaitingForQueryRoomRequestCallback, mmTaskID);

			int ret = sceNpMatching2GetRoomDataExternalList(m_pPSNSupport->GetMatchmakingContextId(), &pQueryParams->m_queryRequest, NULL, &pTask->m_reqId);
			if (ret == PSN_OK)
			{
				//-- waiting for callback
			}
			else
			{
				NetLog("sceNpMatching2GetRoomDataExternalList : error %08X", ret);
				//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
				//-- otherwise, we have to update the task error ourself.
				if (!m_pPSNSupport->HandlePSNError(ret))
				{
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_GetRoomDataExternalList(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForQueryRoomRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST);
	assert(data.m_requestEvent.m_dataHdl != TMemInvalidHdl);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNSearchRoomResponse));

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNSearchRoomResponse)))
	{
		// take ownership of the memory returned by the callback so it is not automatically discarded on return
		pTask->params[PSN_MATCHMAKING_PSNSUPPORT_QUERY_RESPONSE_SLOT] = data.m_requestEvent.m_dataHdl;
		data.m_requestEvent.m_dataHdl = TMemInvalidHdl;
		data.m_requestEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_OWNS_MEMORY;

		_this->UpdateTaskError(taskId, eCLE_Success);
		_this->StopTaskRunning(taskId);

		return true;
	}
	else
	{
		// return false to let the default error handler work
		return false;
	}
}

void CCryPSNMatchMaking::EndSessionQuery(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SCryPSNSearchRoomResponse* pResponse = NULL;

	if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_QUERY_RESPONSE_SLOT] != TMemInvalidHdl)
	{
		pResponse = (SCryPSNSearchRoomResponse*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_QUERY_RESPONSE_SLOT]);
	}

	if (pTask->error == eCLE_Success)
	{
		if (pResponse)
		{
			for (uint16 i = 0; i < pResponse->m_numRooms; i++)
			{
				SCrySessionSearchResult result;
				SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA];

				ProcessExternalRoomData(pResponse->m_pRooms[i], result, userData);

				((CryMatchmakingSessionQueryCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &result, pTask->cbArg);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}

	// free the memory allocated to the response
	if (pResponse)
	{
		pResponse->Release(m_lobby);
	}

	((CryMatchmakingSessionQueryCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
}

ECryLobbyError CCryPSNMatchMaking::SessionGetUsers(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetUsersCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* session = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionGetUsers, false, session->localConnection.users[0], &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[Lobby] Session Get Users Is Error %d", error);
	}

	return error;
}

void CCryPSNMatchMaking::GetRoomMemberData(CryLobbySessionHandle sessionHandle, SCryUserInfoResult& result, SCryMatchMakingConnectionUID& uid)
{
	SSession::SRoomMember* pRoomMember = FindRoomMemberFromRoomMemberID(sessionHandle, uid.m_uid);

	result.m_conID = uid;
	result.m_isDedicated = false;           // PSN does not support dedicated servers

	if (pRoomMember)
	{
		cry_strcpy(result.m_userName, pRoomMember->m_npId.handle.data);
		memcpy(result.m_userData, pRoomMember->m_userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

		((SCryPSNUserID*)result.m_userID.get())->npId = pRoomMember->m_npId;
	}
	else
	{
		result.m_userName[0] = 0;
	}
}

void CCryPSNMatchMaking::TickSessionGetUsers(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	SCryUserInfoResult temp;
	temp.m_userID = new SCryPSNUserID;

	int a;

	// Glue in local user - arguably, this could all just iterate over the room members list (ho hum)
	GetRoomMemberData(pTask->session, temp, m_sessions[pTask->session].localConnection.uid);
	((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, pTask->error, &temp, pTask->cbArg);

	for (a = 0; a < MAX_LOBBY_CONNECTIONS; a++)
	{
		if (m_sessions[pTask->session].remoteConnection[a].used)
		{
			GetRoomMemberData(pTask->session, temp, m_sessions[pTask->session].remoteConnection[a].uid);
			((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, pTask->error, &temp, pTask->cbArg);
		}
	}

	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryPSNMatchMaking::SessionStart(CrySessionHandle gh, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed())
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionStart, false, pSession->localConnection.users[0], &mmTaskID, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags |= CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionStart error %d", error);

	return error;
}

ECryLobbyError CCryPSNMatchMaking::SessionEnd(CrySessionHandle gh, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed())
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionEnd, false, pSession->localConnection.users[0], &mmTaskID, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	NetLog("[Lobby] Start SessionEnd error %d", error);

	return error;
}

// SESSION DELETE ------------------------------------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionDelete(CrySessionHandle gh, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed())
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		// Disconnect our local connection
		SessionDisconnectRemoteConnectionViaNub(h, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_UserRequested, "Session deleted");

		error = StartTask(eT_SessionDelete, false, pSession->localConnection.users[0], &mmTaskID, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_LEAVE_REQUEST_SLOT, NULL, 1, sizeof(SLeaveParamData));
			if (error == eCLE_Success)
			{
				error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_WEBAPI_LEAVE_REQUEST_SLOT, NULL, 1, sizeof(SCryPSNOrbisWebApiLeaveSessionInput));
				if (error == eCLE_Success)
				{
					m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

					FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);

			#if NETWORK_HOST_MIGRATION
					// Since we're deleting this session, terminate any host migration
					if (pSession->hostMigrationInfo.m_state != eHMS_Idle)
					{
						m_hostMigration.Terminate(&pSession->hostMigrationInfo);
					}
			#endif
					pSession->createFlags &= ~CRYSESSION_CREATE_FLAG_MIGRATABLE;
				}
			}
			if (error != eCLE_Success)
			{
				FreeTask(mmTaskID);
			}
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	NetLog("[Lobby] Start SessionDelete error %d", error);

	return error;
}

void CCryPSNMatchMaking::StartSessionDelete(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

			#if NETWORK_HOST_MIGRATION
	// Since we're deleting this session, terminate any host migration
	if (pSession->hostMigrationInfo.m_state != eHMS_Idle)
	{
		m_hostMigration.Terminate(&pSession->hostMigrationInfo);
	}
			#endif

	pSession->createFlags &= ~CRYSESSION_CREATE_FLAG_MIGRATABLE;
	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* connection = &pSession->remoteConnection[i];

		if (connection->used)
		{
			FreeRemoteConnection(pTask->session, i);
		}
	}

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::TickSessionDelete(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_LEAVE_REQUEST_SLOT] != TMemInvalidHdl)
		{
			SLeaveParamData* pLeaveParams = (SLeaveParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_LEAVE_REQUEST_SLOT]);
			SSession* pSession = &m_sessions[pTask->session];

			memset(pLeaveParams, 0, sizeof(SLeaveParamData));
			pLeaveParams->m_leaveRequest.roomId = pSession->m_roomId;
			pLeaveParams->m_leaveRequest.optData.len = 0;

			StartSubTask(eST_WaitingForLeaveRoomRequestCallback, mmTaskID);

			int ret = sceNpMatching2LeaveRoom(m_pPSNSupport->GetMatchmakingContextId(), &pLeaveParams->m_leaveRequest, NULL, &pTask->m_reqId);
			if (ret == PSN_OK)
			{
				//-- wait for callback
			}
			else
			{
				NetLog("sceNpMatching2LeaveRoom : error %08X", ret);

				if ((ret == SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND) || (ret == SCE_NP_MATCHING2_SERVER_ERROR_NO_SUCH_ROOM) || (ret == SCE_NP_MATCHING2_SERVER_ERROR_NO_SUCH_ROOM_INSTANCE))
				{
					//-- To fix a bug in trying to delete a room that doesn't exist.
					//-- If we attempt to leave a room, and the room doesn't exist, we'll treat it the same as if we left
					//-- the room successfully, so call the event handler than deals with the cleanup of the room leaving.
					SCryPSNSupportCallbackEventData data;
					memset(&data, 0, sizeof(SCryPSNSupportCallbackEventData));
					data.m_requestEvent.m_event = REQUEST_EVENT_LEAVE_ROOM;
					data.m_requestEvent.m_reqId = pTask->m_reqId;

					EventRequestResponse_LeaveRoom(this, pTask, mmTaskID, data);
				}
				else
				{
					//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
					//-- otherwise, we have to update the task error ourself.
					if (!m_pPSNSupport->HandlePSNError(ret))
					{
						UpdateTaskError(mmTaskID, eCLE_RoomDoesNotExist);
					}
				}
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_LeaveRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForLeaveRoomRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_LEAVE_ROOM);

	if (data.m_requestEvent.m_error == 0)
	{
		if (pTask->session != CryLobbyInvalidSessionHandle)
		{
			SSession* pSession = &_this->m_sessions[pTask->session];
			if (pSession->IsUsed())
			{
				if (pSession->m_sessionId.data[0] != 0)
				{
					_this->StartSubTask(eST_WaitingForLeaveRoomWebApiCallback, taskId);

					SCryPSNOrbisWebApiLeaveSessionInput* pLeaveWebApiParams = (SCryPSNOrbisWebApiLeaveSessionInput*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_WEBAPI_LEAVE_REQUEST_SLOT]);
					if (pLeaveWebApiParams)
					{
						memcpy(&pLeaveWebApiParams->sessionId, &pSession->m_sessionId, sizeof(SceNpSessionId));
						memcpy(&pLeaveWebApiParams->playerId, &_this->m_pPSNSupport->GetLocalNpId()->handle, sizeof(SceNpOnlineId));

						SLeaveParamData* pLeaveParams = (SLeaveParamData*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_LEAVE_REQUEST_SLOT]);
						pLeaveParams->m_leaveJobId = _this->m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::LeaveSession, pTask->params[PSN_MATCHMAKING_WEBAPI_LEAVE_REQUEST_SLOT]);
					}
					else
					{
						// failed!
						_this->UpdateTaskError(taskId, eCLE_OutOfMemory);
					}
				}
				else
				{
					// no session advert to leave, so we're finished!
					_this->ClearSessionInfo(pTask->session);
					_this->FreeSessionHandle(pTask->session);

					_this->UpdateTaskError(taskId, eCLE_Success);
					_this->StopTaskRunning(taskId);
				}
			}
			else
			{
				_this->UpdateTaskError(taskId, eCLE_InternalError);
			}
		}
		else
		{
			_this->UpdateTaskError(taskId, eCLE_InternalError);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void CCryPSNMatchMaking::EventWebApiLeaveSession(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->session != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[pTask->session];

		if (data.m_webApiEvent.m_error == PSN_OK)
		{
			// Completed successfully
			// no session advert to leave, so we're finished!
			ClearSessionInfo(pTask->session);
			FreeSessionHandle(pTask->session);

			UpdateTaskError(mmTaskID, eCLE_Success);
			StopTaskRunning(mmTaskID);
			return;
		}
	}

	// otherwise something went wrong!
	ClearSessionInfo(pTask->session);
	FreeSessionHandle(pTask->session);

	UpdateTaskError(mmTaskID, eCLE_InternalError);
	StopTaskRunning(mmTaskID);
}

// END SESSION DELETE -----------------------------------------------------------------------------------------------------

// SESSION SEARCH ---------------------------------------------------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionSearch(uint32 user, SCrySessionSearchParam* pParam, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSearchCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryMatchMakingTaskID mmTaskID;

	ECryLobbyError error = StartTask(eT_SessionSearch, false, user, &mmTaskID, pTaskID, 0, (void*)cb, cbArg);
	if (error == eCLE_Success)
	{
		error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_SEARCH_PARAMS_SLOT, pParam, 1, sizeof(SCrySessionSearchParam));
		if (error == eCLE_Success)
		{
			error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_SEARCH_DATA_SLOT, pParam->m_data, pParam->m_numData, pParam->m_numData * sizeof(pParam->m_data[0]));
			if (error == eCLE_Success)
			{
				error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT, NULL, 1, sizeof(SSearchInfoData));
				if (error == eCLE_Success)
				{
					m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

					FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
				}
			}
		}

		if (error != eCLE_Success)
		{
			FreeTask(mmTaskID);
		}
	}

	NetLog("[Lobby] Start SessionSearch error %d", error);

	return error;
}

uint32 CCryPSNMatchMaking::BuildSearchFilterParams(SCrySessionSearchData* pData, uint32 numData,
                                                   SceNpMatching2IntSearchFilter intFilterExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM])
{
	uint32 nCount = 0;

	if (pData)
	{
		for (uint32 i = 0; (i < numData) && (nCount < SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM); i++)
		{
			SCrySessionSearchData* pCurData = &pData[i];

			for (uint32 j = 0; (j < m_registeredUserData.num) && (nCount < SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM); j++)
			{
				SCrySessionUserData* pRegUserData = &m_registeredUserData.data[j];
				SMappingToPSNLobbyData* pRegMappingData = &m_registeredUserData.mapping[j];

				if (pCurData->m_data.m_id == pRegUserData->m_id)
				{
					if (pCurData->m_data.m_type == pRegUserData->m_type)
					{
						assert(pRegMappingData->integerField && "Can only search using integer fields on PSN!");

						if (pRegMappingData->integerField)
						{
							intFilterExternal[nCount].attr.id = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID + pRegMappingData->fieldOffset;
							intFilterExternal[nCount].attr.num = pCurData->m_data.m_int32;

							switch (pCurData->m_operator)
							{
							default:
								{
									NetLog("[PSN Search] No valid Search filter operator defined: using default EQUALS");
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_EQ;
								}
								break;
							case eCSSO_Equal:
								{
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_EQ;
								}
								break;
							case eCSSO_NotEqual:
								{
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_NE;
								}
								break;
							case eCSSO_LessThan:
								{
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_LT;
								}
								break;
							case eCSSO_LessThanEqual:
								{
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_LE;
								}
								break;
							case eCSSO_GreaterThan:
								{
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_GT;
								}
								break;
							case eCSSO_GreaterThanEqual:
								{
									intFilterExternal[nCount].searchOperator = SCE_NP_MATCHING2_OPERATOR_GE;
								}
								break;
							}

							nCount++;
						}
						else
						{
							NetLog("[PSN Search] Can only search using integer fields on PSN!");
						}
					}
				}
			}
		}
	}

	return nCount;
}

void CCryPSNMatchMaking::StartSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->params[PSN_MATCHMAKING_SEARCH_PARAMS_SLOT] != TMemInvalidHdl)
	{
		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT] != TMemInvalidHdl)
		{
			SCrySessionSearchParam* pData = (SCrySessionSearchParam*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_SEARCH_PARAMS_SLOT]);
			if (pTask->params[PSN_MATCHMAKING_SEARCH_DATA_SLOT] != TMemInvalidHdl)
			{
				pData->m_data = (SCrySessionSearchData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_SEARCH_DATA_SLOT]);
			}
			else
			{
				pData->m_data = NULL;
			}

			SSearchInfoData* pSearchInfo = (SSearchInfoData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);
			memset(pSearchInfo, 0, sizeof(SSearchInfoData));

			uint32 nFields = BuildSearchFilterParams(pData->m_data, pData->m_numData, pSearchInfo->m_intFilterExternal);

			pSearchInfo->m_searchRequest.worldId = 0;
			pSearchInfo->m_searchRequest.lobbyId = 0;
			pSearchInfo->m_searchRequest.flagFilter = SCE_NP_MATCHING2_ROOM_FLAG_ATTR_CLOSED | SCE_NP_MATCHING2_ROOM_FLAG_ATTR_FULL;
			pSearchInfo->m_searchRequest.flagAttr = 0;
			pSearchInfo->m_searchRequest.intFilter = (nFields > 0) ? (pSearchInfo->m_intFilterExternal) : (NULL);
			pSearchInfo->m_searchRequest.intFilterNum = nFields;
			pSearchInfo->m_searchRequest.binFilter = NULL;
			pSearchInfo->m_searchRequest.binFilterNum = 0;
			pSearchInfo->m_searchRequest.rangeFilter.startIndex = SCE_NP_MATCHING2_RANGE_FILTER_START_INDEX_MIN;
			pSearchInfo->m_searchRequest.rangeFilter.max = SCE_NP_MATCHING2_RANGE_FILTER_MAX;
			pSearchInfo->m_searchRequest.option = SCE_NP_MATCHING2_SEARCH_ROOM_OPTION_RANDOM;

			pSearchInfo->m_attrId[0] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID;
			pSearchInfo->m_attrId[1] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_2_ID;
			pSearchInfo->m_attrId[2] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_3_ID;
			pSearchInfo->m_attrId[3] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_4_ID;
			pSearchInfo->m_attrId[4] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_5_ID;
			pSearchInfo->m_attrId[5] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_6_ID;
			pSearchInfo->m_attrId[6] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_7_ID;
			pSearchInfo->m_attrId[7] = SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_8_ID;
			pSearchInfo->m_attrId[8] = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_1_ID;
			pSearchInfo->m_attrId[9] = SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_2_ID;

			pSearchInfo->m_searchRequest.attrId = pSearchInfo->m_attrId;
			pSearchInfo->m_searchRequest.attrIdNum = 10;

			m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			StopTaskRunning(mmTaskID);
		}
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::TickSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		//-- get a list of worlds
		StartSubTask(eST_WaitingForSearchRoomWorldInfo, mmTaskID);

		SceNpMatching2GetWorldInfoListRequest reqParam;
		memset(&reqParam, 0, sizeof(reqParam));
		reqParam.serverId = m_pPSNSupport->GetServerInfo()->server.serverId;

		int ret = sceNpMatching2GetWorldInfoList(m_pPSNSupport->GetMatchmakingContextId(), &reqParam, NULL, &pTask->m_reqId);
		if (ret == PSN_OK)
		{
			//-- Waiting for callback
		}
		else
		{
			NetLog("sceNpMatching2GetWorldInfoList : error %08X", ret);
			if (!m_pPSNSupport->HandlePSNError(ret))
			{
				UpdateTaskError(mmTaskID, eCLE_WorldNotDefined);
			}
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_GetWorldInfoListForSearch(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	assert(pTask->subTask == eST_WaitingForSearchRoomWorldInfo);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_GET_WORLD_INFO_LIST);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNGetWorldInfoListResponse));

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNGetWorldInfoListResponse)))
	{
		SSearchInfoData* pSearchInfo = (SSearchInfoData*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);
		if (pSearchInfo)
		{
			const SCryPSNGetWorldInfoListResponse* pWorldList = (SCryPSNGetWorldInfoListResponse*)_this->m_lobby->MemGetPtr(data.m_requestEvent.m_dataHdl);

			pSearchInfo->m_numWorlds = SortWorlds(pWorldList, pSearchInfo->m_worldIds, MAX_WORLDS_TO_SEARCH, true);
			if (pSearchInfo->m_numWorlds > 0)
			{
				pSearchInfo->m_nIndex = 0;
				pSearchInfo->m_searchRequest.worldId = pSearchInfo->m_worldIds[pSearchInfo->m_nIndex];

				_this->StartSubTask(eST_SessionSearchRequest, taskId);
			}
			else
			{
				_this->UpdateTaskError(taskId, eCLE_WorldNotDefined);
			}
		}
		else
		{
			_this->UpdateTaskError(taskId, eCLE_OutOfMemory);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void CCryPSNMatchMaking::TickSessionSearchRequest(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		SSearchInfoData* pSearchInfo = (SSearchInfoData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);

		StartSubTask(eST_WaitingForSearchRoomRequestCallback, mmTaskID);

		int ret = sceNpMatching2SearchRoom(m_pPSNSupport->GetMatchmakingContextId(), &pSearchInfo->m_searchRequest, NULL, &pTask->m_reqId);
		if (ret == PSN_OK)
		{
			//-- waiting for callback
		}
		else
		{
			NetLog("sceNpMatching2SearchRoom : error %08X", ret);
			//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
			//-- otherwise, we have to update the task error ourself.
			if (!m_pPSNSupport->HandlePSNError(ret))
			{
				UpdateTaskError(mmTaskID, eCLE_InternalError);
			}
		}
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_SearchRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForSearchRoomRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_SEARCH_ROOM);
	assert(data.m_requestEvent.m_dataHdl != TMemInvalidHdl);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNSearchRoomResponse));

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNSearchRoomResponse)))
	{
		const SCryPSNSearchRoomResponse* const pResponse = (SCryPSNSearchRoomResponse*)_this->m_lobby->MemGetPtr(data.m_requestEvent.m_dataHdl);
		SSearchInfoData* pSearchInfo = (SSearchInfoData*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);
		SCrySessionSearchParam* pSearchParams = (SCrySessionSearchParam*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_SEARCH_PARAMS_SLOT]);

		uint16 c = 0;
		while ((pSearchInfo->m_numResults < MAX_ROOMS_TO_STORE) && (pSearchInfo->m_numResults < pSearchParams->m_maxNumReturn) && (c < pResponse->m_numRooms))
		{
			_this->ProcessExternalRoomData(pResponse->m_pRooms[c], pSearchInfo->m_results[pSearchInfo->m_numResults], pSearchInfo->m_resultUserData[pSearchInfo->m_numResults]);
			pSearchInfo->m_numResults++;
			c++;
		}

		pSearchInfo->m_nIndex++;
		if ((pSearchInfo->m_numResults < MAX_ROOMS_TO_STORE) && (pSearchInfo->m_numResults < pSearchParams->m_maxNumReturn) && (pSearchInfo->m_nIndex < pSearchInfo->m_numWorlds))
		{
			//-- Chain start a new search on the next world in the list.
			assert(pSearchInfo->m_worldIds[pSearchInfo->m_nIndex] != 0);
			pSearchInfo->m_searchRequest.worldId = pSearchInfo->m_worldIds[pSearchInfo->m_nIndex];
			_this->StartSubTask(eST_SessionSearchRequest, taskId);
		}
		else
		{
			if (pSearchInfo->m_numResults > 0)
			{
				//-- We've filled the results buffer, or run out of worlds, so move onto QOS!
				pSearchInfo->m_nIndex = 0;
				_this->StartSubTask(eST_SessionSearchKickQOS, taskId);
			}
			else
			{
				//-- No results after searching all the worlds, so just give up on search task.
				_this->UpdateTaskError(taskId, eCLE_Success);
				_this->StopTaskRunning(taskId);
			}
		}

		return true;
	}
	else
	{
		// an error was returned in the response. If we return false, the default error handler will deal with it
		return false;
	}
}

void CCryPSNMatchMaking::TickSessionWaitForQOS(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->GetTimer() >= MAX_WAITTIME_FOR_QOS)
	{
		NetLog("Timeout Getting QOS Data - Returning what we have");
		UpdateTaskError(mmTaskID, eCLE_Success);    // Don't return timeout, that way game gets some results
		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::TickSessionQOSSearch(CryMatchMakingTaskID mmTaskID)
{
	int qosIndex = 0;

	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		SSearchInfoData* pSearchInfo = (SSearchInfoData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);
		pSearchInfo->m_nIndex = 0;
		while (qosIndex < pSearchInfo->m_numResults)
		{
			SCryPSNSessionID* pId = (SCryPSNSessionID*)pSearchInfo->m_results[qosIndex].m_id.get();
			if (pId)
			{
				memset(&pSearchInfo->m_QOSRequest[qosIndex], 0, sizeof(SceNpMatching2SignalingGetPingInfoRequest));
				pSearchInfo->m_QOSRequest[qosIndex].roomId = pId->m_sessionInfo.m_roomId;
				pSearchInfo->m_results[qosIndex].m_ping = PSN_MATCHMAKING_DEFAULT_UNKNOWN_PING;

				pTask->m_reqId = INVALID_PSN_REQUEST_ID;    // We cant use the global task id, because we have many qos subtasks potentially on one task

				StartSubTask(eST_WaitingForSearchQOSRequestCallback, mmTaskID);

				int ret = sceNpMatching2SignalingGetPingInfo(m_pPSNSupport->GetMatchmakingContextId(), &pSearchInfo->m_QOSRequest[qosIndex], NULL, &pSearchInfo->m_reqId[qosIndex]);

				NetLog("Task ID : %d  Idx : %d   Request id : %d   Room Id : %d", mmTaskID, qosIndex, (int32)pSearchInfo->m_reqId[qosIndex], (int32)pId->m_sessionInfo.m_roomId);
				qosIndex++;

				if (ret == PSN_OK)
				{
					//-- waiting for callback
				}
				else
				{
					NetLog("sceNpMatching2SignalingGetPingInfo : error %08X", ret);
					//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
					//-- otherwise, we have to update the task error ourself.
					if (!m_pPSNSupport->HandlePSNError(ret))
					{
						UpdateTaskError(mmTaskID, eCLE_InternalError);
					}
				}
			}
			else
			{
				UpdateTaskError(mmTaskID, eCLE_InternalError);
			}

			pSearchInfo->m_nIndex = qosIndex;
		}

		if (pSearchInfo->m_nIndex == 0)
		{
			//-- SUCCESS End of search results, we can end the task now.
			UpdateTaskError(mmTaskID, eCLE_Success);
			StopTaskRunning(mmTaskID);
		}
		else
		{
			pTask->StartTimer();
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_SearchQOS(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	assert(pTask->subTask == eST_WaitingForSearchQOSRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_SIGNALING_GET_PING_INFO);
	assert(data.m_requestEvent.m_dataHdl != TMemInvalidHdl);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNSignalingGetPingInfoResponse));

	SSearchInfoData* pSearchInfo = (SSearchInfoData*)_this->m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);
	SCrySessionSearchResult* pSearchResult = NULL;

	for (int a = 0; a < pSearchInfo->m_numResults; a++)
	{
		if (data.m_requestEvent.m_reqId == pSearchInfo->m_reqId[a])
		{
			pSearchResult = &pSearchInfo->m_results[a];
			pSearchInfo->m_nIndex--;
			break;
		}
	}

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNSignalingGetPingInfoResponse)) &&
	    pSearchResult)
	{
		const SCryPSNSignalingGetPingInfoResponse* const pQOSResponse = (SCryPSNSignalingGetPingInfoResponse*)_this->m_lobby->MemGetPtr(data.m_requestEvent.m_dataHdl);
		pSearchResult->m_ping = pQOSResponse->m_rtt / 1000;
		NetLog("Request Id: %d QoS Done: ping %d roomId %d", data.m_requestEvent.m_reqId, (int32)pSearchResult->m_ping, (int32)pQOSResponse->m_roomId);
	}
	else
	{
		// QoS can return socket errors, or a timeout as an error. In this rare case, we'll have to assume the error is not fatal and trap it
		// by marking it as handled with no ping value.
		NetLog("Request Id: %d QoS Done ERROR: ping %d, error %08X", data.m_requestEvent.m_reqId, (int32)pSearchResult->m_ping, data.m_requestEvent.m_error);
	}

	if (pSearchInfo->m_nIndex == 0)
	{
		//-- SUCCESS End of search results, we can end the task now.
		_this->UpdateTaskError(taskId, eCLE_Success);
		_this->StopTaskRunning(taskId);
	}

	return true;
}

void CCryPSNMatchMaking::EndSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT] != TMemInvalidHdl)
		{
			SSearchInfoData* pSearchInfo = (SSearchInfoData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_SEARCH_INFO_SLOT]);

			NetLog("[Lobby] Found %d sessions in %d worlds.", pSearchInfo->m_numResults, pSearchInfo->m_numWorlds);

			for (uint32 i = 0; i < pSearchInfo->m_numResults; i++)
			{
				SCrySessionSearchResult* pResult = &pSearchInfo->m_results[i];
				((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, pResult, pTask->cbArg);

				//-- remove reference to CrySessionID, since it's a smart pointer inside a larger alloc that wouldn't be cleaned up otherwise.
				pResult->m_id = NULL;
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}

	((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
}

// END OF SEARCH -------------------------------------------------------------------------

// SESSION JOIN --------------------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionJoin(uint32* users, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* taskID, CryMatchmakingSessionJoinCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;
	ECryLobbyError error = CreateSessionHandle(&h, false, users, numUsers, flags);
	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionJoin, false, users[0], &mmTaskID, taskID, h, (void*)cb, cbArg);
		if (error == eCLE_Success)
		{
			error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_JOIN_REQUEST_SLOT, NULL, 1, sizeof(SJoinParamData));
			if (error == eCLE_Success)
			{
				error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_WEBAPI_JOIN_SLOT, NULL, 1, sizeof(SCryPSNOrbisWebApiJoinSessionInput));
				if (error == eCLE_Success)
				{
					SCryPSNSessionID* psnID = (SCryPSNSessionID*)id.get();
					SSession* pSession = &m_sessions[h];

					pSession->m_roomId = psnID->m_sessionInfo.m_roomId;
					pSession->m_worldId = psnID->m_sessionInfo.m_worldId;
					pSession->m_serverId = psnID->m_sessionInfo.m_serverId;
					pSession->m_gameType = psnID->m_sessionInfo.m_gameType;
					pSession->m_flags = 0;

					for (uint32 i = 0; i < numUsers; i++)
					{
						pSession->localConnection.privateSlot[i] = psnID->m_fromInvite;
					}

					m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

					FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
				}
			}

			if (error != eCLE_Success)
			{
				FreeTask(mmTaskID);
				FreeSessionHandle(h);
			}
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	NetLog("[Lobby] Start SessionJoin error %d", error);

	return error;
}

void CCryPSNMatchMaking::TickSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
		if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
		{
			if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_REQUEST_SLOT] != TMemInvalidHdl)
			{
				SJoinParamData* pJoinParams = (SJoinParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_REQUEST_SLOT]);
				SSession* pSession = &m_sessions[pTask->session];

				memset(pJoinParams, 0, sizeof(SJoinParamData));

				pJoinParams->m_joinRequest.roomId = pSession->m_roomId;
				pJoinParams->m_joinRequest.roomPassword = NULL;
				pJoinParams->m_joinRequest.joinRoomGroupLabel = NULL;
				pJoinParams->m_joinRequest.roomMemberBinAttrInternal = NULL;
				pJoinParams->m_joinRequest.roomMemberBinAttrInternalNum = 0;
				pJoinParams->m_joinRequest.optData.len = 0;

				StartSubTask(eST_WaitingForJoinRoomRequestCallback, mmTaskID);

				int ret = sceNpMatching2JoinRoom(m_pPSNSupport->GetMatchmakingContextId(), &pJoinParams->m_joinRequest, NULL, &pTask->m_reqId);
				if (ret == PSN_OK)
				{
					//-- waiting for callback
				}
				else
				{
					NetLog("sceNpMatching2JoinRoom : error %08X", ret);
					//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
					//-- otherwise, we have to update the task error ourself.
					if (!m_pPSNSupport->HandlePSNError(ret))
					{
						UpdateTaskError(mmTaskID, eCLE_InternalError);
					}
				}
			}
			else
			{
				UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			}
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_JoinRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForJoinRoomRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_JOIN_ROOM);
	assert(data.m_requestEvent.m_dataHdl != TMemInvalidHdl);
	assert(data.m_requestEvent.m_dataSize == sizeof(SCryPSNJoinRoomResponse));

	if ((data.m_requestEvent.m_error == 0) &&
	    (data.m_requestEvent.m_dataHdl != TMemInvalidHdl) &&
	    (data.m_requestEvent.m_dataSize == sizeof(SCryPSNJoinRoomResponse)))
	{
		if (pTask->session != CryLobbyInvalidSessionHandle)
		{
			SSession* pSession = &_this->m_sessions[pTask->session];
			if (pSession->IsUsed())
			{
				const SCryPSNJoinRoomResponse* const pJoinResponse = (SCryPSNJoinRoomResponse*)_this->m_lobby->MemGetPtr(data.m_requestEvent.m_dataHdl);
				pSession->m_roomId = pJoinResponse->m_roomInfo.m_roomId;

				if (!pTask->canceled)
				{
					_this->AddInitialRoomMembers(pTask->session, pJoinResponse->m_roomMembers);

					// take ownership of the memory returned by the callback so it is not automatically discarded on return
					pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_RESPONSE_SLOT] = data.m_requestEvent.m_dataHdl;
					data.m_requestEvent.m_dataHdl = TMemInvalidHdl;
					data.m_requestEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SRequestEvent::REQUEST_EVENT_OWNS_MEMORY;

					_this->StartSubTask(eST_WaitingForJoinRoomSignalingCallback, taskId);
				}
				else
				{
					_this->UpdateTaskError(taskId, eCLE_IllegalSessionJoin);
				}
			}
			else
			{
				_this->UpdateTaskError(taskId, eCLE_InternalError);
			}
		}
		else
		{
			_this->UpdateTaskError(taskId, eCLE_InternalError);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void CCryPSNMatchMaking::ProcessSessionJoinHostAck(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryLobbySessionHandle h = CryLobbyInvalidSessionHandle;
	SCryMatchMakingConnectionUID hostUID = pPacket->GetFromConnectionUID();
	CryMatchMakingConnectionID mmCxID = CryMatchMakingInvalidConnectionID;

	if (FindConnectionFromUID(hostUID, &h, &mmCxID))
	{
		SSession* pSession = &m_sessions[h];

		NetLog("[join session]: received join session server acknowledgment " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, hostUID));

		// part of the join session ack packet is the host's session id, which we want to copy.
		pPacket->ReadString((char*)&pSession->m_sessionId, sizeof(SceNpSessionId));

		for (uint32 a = 0; a < MAX_PSN_ROOM_MEMBERS; a++)
		{
			SSession::SRoomMember* pMember = &pSession->m_members[a];
			if (pMember->IsValid(ePSNMI_Owner))
			{
				if ((pMember->m_signalingState == ePSNCS_Active) || (pMember->m_signalingState == ePSNCS_Pending))
				{
					pMember->m_bHostJoinAck = true;

					for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
					{
						STask* pTask = &m_task[i];

						if (pTask->used && pTask->running && (pTask->subTask == eST_WaitingForJoinRoomSignalingCallback))
						{
							TickSessionJoinSignaling(i);
						}
					}
				}
				break;
			}
		}
	}
	else
	{
		NetLog("[join session]: received join session server acknowledgement from unknown host " PRFORMAT_UID, PRARG_UID(hostUID));
	}
}

void CCryPSNMatchMaking::TickSessionJoinSignaling(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		UpdateSignaling(mmTaskID);

		switch (CheckSignaling(mmTaskID))
		{
		case ePSNCS_None:
			{
				//-- This is bad, means there is no room owner!
				UpdateTaskError(mmTaskID, eCLE_InvalidParam);
			}
			break;
		case ePSNCS_Active:
			{
				assert(pTask->subTask == eST_WaitingForJoinRoomSignalingCallback);

				//-- Success! We can talk to the server p2p, and we also have the hosts session Id
				// Next step is to join the same session via webapi.
				if (!pTask->canceled)
				{
					if (pTask->session != CryLobbyInvalidSessionHandle)
					{
						SSession* pSession = &m_sessions[pTask->session];
						if (pSession->IsUsed())
						{
							if (pSession->createFlags & CRYSESSION_CREATE_FLAG_INVITABLE)
							{
								if (pSession->m_sessionId.data[0] != 0)
								{
									SJoinParamData* pJoinParams = (SJoinParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_REQUEST_SLOT]);
									SCryPSNOrbisWebApiJoinSessionInput* pWebApiJoinParams = (SCryPSNOrbisWebApiJoinSessionInput*)m_pPSNSupport->GetLobby()->MemGetPtr(pTask->params[PSN_MATCHMAKING_WEBAPI_JOIN_SLOT]);
									if (pJoinParams && pWebApiJoinParams)
									{
										StartSubTask(eST_WaitingForJoinRoomWebApiCallback, mmTaskID);

										memcpy(&pWebApiJoinParams->sessionId, &pSession->m_sessionId, sizeof(SceNpSessionId));

										pJoinParams->m_joinJobId = m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::JoinSession, pTask->params[PSN_MATCHMAKING_WEBAPI_JOIN_SLOT]);
										if (pJoinParams->m_joinJobId == INVALID_WEBAPI_JOB_ID)
										{
											// failed
											UpdateTaskError(mmTaskID, eCLE_InternalError);
										}
									}
									else
									{
										UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
									}
								}
								else
								{
									// missing session Id
									UpdateTaskError(mmTaskID, eCLE_InvalidParam);
								}
							}
							else
							{
								// Session is not invitable, so we don't need to join a webapi session
								// This is the end of Join! Success!
								UpdateTaskError(mmTaskID, eCLE_Success);
								StopTaskRunning(mmTaskID);
							}
						}
						else
						{
							// session not in use
							UpdateTaskError(mmTaskID, eCLE_InternalError);
						}
					}
					else
					{
						// invalid session handle
						UpdateTaskError(mmTaskID, eCLE_InternalError);
					}
				}
				else
				{
					// cancelled
					UpdateTaskError(mmTaskID, eCLE_IllegalSessionJoin);
				}
			}
			break;
		case ePSNCS_Dead:
			{
				assert(pTask->subTask == eST_WaitingForJoinRoomSignalingCallback);

				//-- Failed! We cannot talk to the server via p2p, so we've failed at the last hurdle! Join task completed(failed)
				UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			}
			break;
		}
	}
}

void CCryPSNMatchMaking::EventWebApiJoinSession(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->session != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[pTask->session];

		if (data.m_webApiEvent.m_error == PSN_OK)
		{
			// Completed successfully
			UpdateTaskError(mmTaskID, eCLE_Success);
			StopTaskRunning(mmTaskID);
			return;
		}
	}

	// otherwise something went wrong!
	UpdateTaskError(mmTaskID, eCLE_InternalError);
	StopTaskRunning(mmTaskID);
}

void CCryPSNMatchMaking::EndSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	uint32 myIP = 0, myPort = 0;
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	SCryPSNJoinRoomResponse* pResponse = NULL;

	if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_RESPONSE_SLOT] != TMemInvalidHdl)
	{
		pResponse = (SCryPSNJoinRoomResponse*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_JOIN_RESPONSE_SLOT]);
	}

	if (pTask->error == eCLE_Success)
	{
		if (pResponse)
		{
			SSession::SRoomMember* pMember = FindRoomMemberFromRoomMemberID(pTask->session, pSession->m_ownerMemberId);

			if (pSession->m_ownerMemberId != pSession->m_myMemberId)
			{
				if (pMember && (pMember->m_signalingState == ePSNCS_Active))
				{
					SCryMatchMakingConnectionUID matchingUID;
					myIP = pMember->m_peerAddr.s_addr;
					myPort = pMember->m_peerPort;

					matchingUID.m_uid = pSession->m_myMemberId;
					matchingUID.m_sid = pSession->m_roomId;

					pSession->localConnection.uid = matchingUID;
					pSession->localConnection.pingToServer = CRYLOBBY_INVALID_PING;
					pSession->localConnection.used = true;

					uint16 sessionCreateGameFlags = 0;
					memcpy(&sessionCreateGameFlags, pResponse->m_roomInfo.m_binAttributes[0].m_data, sizeof(sessionCreateGameFlags));
					pSession->createFlags = (pSession->createFlags & CRYSESSION_CREATE_FLAG_SYSTEM_MASK) | (sessionCreateGameFlags << CRYSESSION_CREATE_FLAG_GAME_FLAGS_SHIFT);

					bool hostMigrationSupported = m_lobby->GetLobbyServiceFlag(m_serviceType, eCLSF_SupportHostMigration);
					if ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) && hostMigrationSupported)
					{
						pSession->localFlags |= CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
					}

					matchingUID.m_uid = pSession->m_ownerMemberId;
					CryLobbySessionHandle h;
					FindConnectionFromUID(matchingUID, &h, &pSession->hostConnectionID);
					assert(h == pTask->session);

					//					if (pSession->createFlags & CRYSESSION_CREATE_FLAG_INVITABLE)
					//					{
					//						if (m_pService->GetLobbyUI())
					//						{
					//							((CCryPSNLobbyUI*)m_pService->GetLobbyUI())->SetStoredInviteSessionHandle(CreateGameSessionHandle(pTask->session, CryMatchMakingInvalidConnectionUID));
					//						}
					//					}
				}
				else
				{
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
			else
			{
				// we've become the owner in the middle of joining, that won't end well,
				// so terminate here
				UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}

	// free the memory allocated to the response
	if (pResponse)
	{
		pResponse->Release(m_lobby);
	}

	((CryMatchmakingSessionJoinCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, pSession->localConnection.uid), myIP, myPort, pTask->cbArg);

	if (pTask->error == eCLE_Success)
	{
		InitialUserDataEvent(pTask->session);
	}
}

// END SESSION JOIN -------------------------------------------------------------

// SESSION UPDATE USER ---------------------------------------------------------------

ECryLobbyError CCryPSNMatchMaking::SessionSetLocalUserData(CrySessionHandle gh, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].IsUsed())
	{
		SSession* session = &m_sessions[h];

		if (dataSize <= CRYLOBBY_USER_DATA_SIZE_IN_BYTES)
		{
			CryMatchMakingTaskID mmTaskID;

			error = StartTask(eT_SessionUserData, false, session->localConnection.users[0], &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);
			if (error == eCLE_Success)
			{
				STask* task = &m_task[mmTaskID];

				error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_USERDATA_DATA_SLOT, pData, dataSize, dataSize);
				if (error == eCLE_Success)
				{
					error = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_USERDATA_REQUEST_SLOT, NULL, 1, sizeof(SUserDataParamData));
					if (error == eCLE_Success)
					{
						m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

						FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
					}
					else
					{
						FreeTask(mmTaskID);
					}
				}
				else
				{
					FreeTask(mmTaskID);
				}
			}
		}
		else
		{
			error = eCLE_OutOfUserData;
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionUpdate error %d", error);

	return error;
}

void CCryPSNMatchMaking::TickSessionUserData(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_USERDATA_REQUEST_SLOT] != TMemInvalidHdl)
		{
			SUserDataParamData* pUserDataParams = (SUserDataParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_USERDATA_REQUEST_SLOT]);

			if (pTask->params[PSN_MATCHMAKING_USERDATA_DATA_SLOT] != TMemInvalidHdl)
			{
				uint8* pData = (uint8*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_USERDATA_DATA_SLOT]);
				SSession* pSession = &m_sessions[pTask->session];

				memset((void*)pUserDataParams, 0, sizeof(SUserDataParamData));

				memcpy(pUserDataParams->m_binAttrInternalData, pData, pTask->numParams[PSN_MATCHMAKING_USERDATA_DATA_SLOT]);
				pUserDataParams->m_userDataRequest.roomMemberBinAttrInternal = pUserDataParams->m_binAttrInternal;

				pUserDataParams->m_userDataRequest.roomId = pSession->m_roomId;
				pUserDataParams->m_userDataRequest.memberId = 0;    // This is allowed, and means update my own data
				pUserDataParams->m_userDataRequest.teamId = 0;

				for (int a = 0; a < SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_NUM; a++)
				{
					pUserDataParams->m_binAttrInternal[a].id = SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_1_ID + a;
					pUserDataParams->m_binAttrInternal[a].ptr = &pUserDataParams->m_binAttrInternalData[a * SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_MAX_SIZE];
					pUserDataParams->m_binAttrInternal[a].size = SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_MAX_SIZE;
				}
				pUserDataParams->m_userDataRequest.roomMemberBinAttrInternalNum = SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_NUM;

				StartSubTask(eST_WaitingForUserDataRequestCallback, mmTaskID);

				int ret = sceNpMatching2SetRoomMemberDataInternal(m_pPSNSupport->GetMatchmakingContextId(), &pUserDataParams->m_userDataRequest, NULL, &pTask->m_reqId);
				if (ret == PSN_OK)
				{
					//-- Waiting for callback
				}
				else
				{
					NetLog("sceNpMatching2SetRoomMemberDataInternal : error %08X", ret);
					if (!m_pPSNSupport->HandlePSNError(ret))
					{
						UpdateTaskError(mmTaskID, eCLE_InternalError);
					}
				}
			}
			else
			{
				UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_SetRoomMemberDataInternal(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity tests
	assert(pTask->subTask == eST_WaitingForUserDataRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_SET_ROOM_MEMBER_DATA_INTERNAL);
	assert(data.m_requestEvent.m_dataSize == 0);

	if (data.m_requestEvent.m_error == 0)
	{
		// We don't really care if this SetRoomMemberDataInternal succeeds for fails, so we'll just mark the task as complete
		_this->UpdateTaskError(taskId, eCLE_Success);
		_this->StopTaskRunning(taskId);

		// this event is handled
		return true;
	}
	else
	{
		// return false to let the default error handler work
		return false;
	}
}

// END SESSION UPDATE USER ------------------------------------------------------------------------

void CCryPSNMatchMaking::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;

	switch (pSPacket->StartRead())
	{
			#if NETWORK_HOST_MIGRATION
	case ePSNPT_HostMigrationStart:
		ProcessHostMigrationStart(addr, pSPacket);
		break;

	case ePSNPT_HostMigrationServer:
		ProcessHostMigrationFromServer(addr, pSPacket);
		break;
			#endif

	case ePSNPT_JoinSessionAck:
		ProcessSessionJoinHostAck(addr, pSPacket);
		break;

	default:
		CCryMatchMaking::OnPacket(addr, pSPacket);
		break;
	}
}

void CCryPSNMatchMaking::SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED)
	{
		SCryUserInfoResult userInfo;
		userInfo.m_userID = new SCryPSNUserID;

		if (id == CryMatchMakingInvalidConnectionID)
		{
			SSession::SLConnection* pConnection = &pSession->localConnection;
			GetRoomMemberData(h, userInfo, pConnection->uid);
		}
		else
		{
			SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
			GetRoomMemberData(h, userInfo, pConnection->uid);
		}

		CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
	}
}

void CCryPSNMatchMaking::InitialUserDataEvent(CryLobbySessionHandle h)
{
	SSession* pSession = &m_sessions[h];

	pSession->localFlags |= CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED;

	SessionUserDataEvent(eCLSE_SessionUserJoin, h, CryMatchMakingInvalidConnectionID);

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

		if (pConnection->used)
		{
			SessionUserDataEvent(eCLSE_SessionUserJoin, h, i);
		}
	}
}

			#if NETWORK_HOST_MIGRATION
void CCryPSNMatchMaking::HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause)
{
	SSession* pSession = &m_sessions[h];

	pSession->hostMigrationInfo.Reset();
}

ECryLobbyError CCryPSNMatchMaking::HostMigrationServer(SHostMigrationInfo* pInfo)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(pInfo->m_session);
	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST;
		pSession->hostConnectionID = CryMatchMakingInvalidConnectionID;
		error = StartTask(eT_SessionMigrateHostServer, false, pSession->localConnection.users[0], &mmTaskID, NULL, h, NULL, NULL);

		if (error == eCLE_Success)
		{
			STask* pTask = &m_task[mmTaskID];
				#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
			DetectHostMigrationTaskBreak(h, "HostMigrationServer()");
				#endif
			pSession->hostMigrationInfo.m_taskID = pTask->lTaskID;

			NetLog("[Host Migration]: matchmaking migrating " PRFORMAT_SH " on the SERVER", PRARG_SH(h));
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[Host Migration]: CCryPSNMatchMaking::HostMigrationServer(): " PRFORMAT_SH ", error %d", PRARG_SH(h), error);
	}

	return error;
}

void CCryPSNMatchMaking::HostMigrationServerNT(CryMatchMakingTaskID mmTaskID)
{
	STask* task = &m_task[mmTaskID];
	task->StartTimer();
}

void CCryPSNMatchMaking::TickHostMigrationServerNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	if (pTask->canceled)
	{
		NetLog("[Host Migration]: TickHostMigrationServerNT() cancelled for " PRFORMAT_SH " - bailing", PRARG_SH(pTask->session));
		StopTaskRunning(mmTaskID);
		return;
	}

	float timeout = CLobbyCVars::Get().hostMigrationTimeout * 1000.0f;
	if (pTask->GetTimer() > timeout)
	{
		StopTaskRunning(mmTaskID);
		return;
	}

	NetLog("[Host Migration]: matchmaking SERVER migrated " PRFORMAT_SH " successfully - informing CLIENTS", PRARG_SH(pTask->session));

	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize;
	uint8 buffer[MaxBufferSize];
	CCrySharedLobbyPacket packet;

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(ePSNPT_HostMigrationServer, true);

				#if !defined(CRYNETWORK_RELEASEBUILD)
	int count = 0;
	CCryMatchMaking::SSession* pSession = CCryMatchMaking::m_sessions[pTask->session];
	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		CCryMatchMaking::SSession::SRConnection* pConnection = pSession->remoteConnection[i];

		if (pConnection->used)
		{
			TNetAddress ip;
			if (m_lobby->AddressFromConnection(ip, pConnection->connectionID))
			{
				NetLog("    sending to " PRFORMAT_ADDR " client, " PRFORMAT_MMCINFO, PRARG_ADDR(ip), PRARG_MMCINFO(CryMatchMakingConnectionID(i), pConnection->connectionID, pConnection->uid));
			}

			++count;
		}
	}
	NetLog("    sending to %i clients", count);
				#endif

	SendToAll(mmTaskID, &packet, pTask->session, CryMatchMakingInvalidConnectionID);
	m_lobby->FlushMessageQueue();

	pSession->hostMigrationInfo.m_sessionMigrated = true;
	pSession->hostMigrationInfo.m_matchMakingFinished = true;
	StopTaskRunning(mmTaskID);
}

void CCryPSNMatchMaking::ProcessHostMigrationFromServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryLobbySessionHandle h = CryLobbyInvalidSessionHandle;
	SCryMatchMakingConnectionUID hostUID = pPacket->GetFromConnectionUID();
	CryMatchMakingConnectionID mmCxID = CryMatchMakingInvalidConnectionID;

	if (FindConnectionFromUID(hostUID, &h, &mmCxID))
	{
		SSession* pSession = &m_sessions[h];
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			NetLog("[Host Migration]: ignoring session details from " PRFORMAT_SHMMCINFO " as I am already the server " PRFORMAT_UID,
			       PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, hostUID), PRARG_UID(pSession->localConnection.uid));
		}
		else
		{
			NetLog("[Host Migration]: matchmaking CLIENT received session details from " PRFORMAT_SHMMCINFO " - migrating to new PSN session",
			       PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, hostUID));
			NetLog("[Host Migration]: matchmaking CLIENT received server confirmation for " PRFORMAT_SH " - migrating to new session", PRARG_SH(h));

			SSession* pSession = &m_sessions[h];
			if (pSession->hostMigrationInfo.m_state == eHMS_Idle)
			{
				NetLog("[Host Migration]: received server confirmation before host migration initiated - initiating now");
				HostMigrationInitiate(h, eDC_Unknown);
			}

			pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_HOST;
			pSession->hostMigrationInfo.m_sessionMigrated = true;
			pSession->hostMigrationInfo.m_matchMakingFinished = true;

			if (pSession->m_ownerMemberId == hostUID.m_uid)
			{
				pSession->hostMigrationInfo.m_newHostAddressValid = true;
			}
		}
	}
	else
	{
		NetLog("[Host Migration]: received server notification from unknown host " PRFORMAT_UID, PRARG_UID(hostUID));
	}
}

bool CCryPSNMatchMaking::GetNewHostAddress(char* address, SHostMigrationInfo* pInfo)
{
	bool success = false;

	CryLobbySessionHandle sessionIndex = GetSessionHandleFromGameSessionHandle(pInfo->m_session);
	SSession* pSession = &m_sessions[sessionIndex];
	SHostMigrationInfo_Private* pPrivateInfo = &pSession->hostMigrationInfo;

	if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED) && pPrivateInfo->m_newHostAddressValid)
	{
		uint32 myIP = 0;
		uint16 myPort = 0;

		const SSession::SRoomMember* const pMember = FindRoomMemberFromRoomMemberID(sessionIndex, pSession->m_ownerMemberId);

		if (pMember)
		{
			if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
			{
				if (pSession->desiredHostUID != CryMatchMakingInvalidConnectionUID)
				{
					if ((pMember->m_valid & ePSNMI_Me) == 0)      // Make sure we are not the new host before updating the connection information
					{
						if (!pSession->hostMigrationInfo.m_matchMakingFinished)
						{
							CryMatchMakingConnectionID remoteConnectionID = FindConnectionIDFromRoomMemberID(sessionIndex, pSession->m_ownerMemberId);
							if (remoteConnectionID != CryMatchMakingInvalidConnectionID)
							{
								pInfo->SetIsNewHost(false);
								success = (pSession->remoteConnection[remoteConnectionID].uid == pSession->desiredHostUID);

								NetLog("[Host Migration]: new host for session " PRFORMAT_SH " host uid " PRFORMAT_UID " desired uid " PRFORMAT_UID, PRARG_SH(sessionIndex), PRARG_UID(pSession->remoteConnection[remoteConnectionID].uid), PRARG_UID(pSession->desiredHostUID));
							}
							else
							{
								NetLog("[Host Migration]: failed to find connection for new host " PRFORMAT_UID " of session " PRFORMAT_SH, PRARG_UID(pSession->desiredHostUID), PRARG_SH(sessionIndex));
							}
						}
						else
						{
							NetLog("[Host Migration]: matchmaking not finished desiredHostUID " PRFORMAT_UID " newHostUID " PRFORMAT_UID, PRARG_UID(pSession->desiredHostUID), PRARG_UID(pSession->newHostUID));
						}

					}
					else
					{
						pInfo->SetIsNewHost(true);
						success = (pSession->localConnection.uid == pSession->desiredHostUID);

						NetLog("[Host Migration]: becoming the new host for session " PRFORMAT_SH " host uid " PRFORMAT_UID " desired uid " PRFORMAT_UID, PRARG_SH(sessionIndex), PRARG_UID(pSession->localConnection.uid), PRARG_UID(pSession->desiredHostUID));
					}
				}

				return success;
			}

			if (pMember->m_signalingState == ePSNCS_Active)
			{
				myIP = pMember->m_peerAddr.s_addr;
				myPort = pMember->m_peerPort;
			}
			if ((pMember->m_valid & ePSNMI_Me) == 0)      // Make sure we are not the new host before updating the connection information
			{
				if (!pSession->hostMigrationInfo.m_matchMakingFinished)
				{
					pInfo->SetIsNewHost(false);
					// Wait until the server has notified us that migration has completed
					return false;
				}

				NetLog("[Host Migration]: new host is uid %i " PRFORMAT_SH, pSession->m_ownerMemberId, PRARG_SH(sessionIndex));
				m_lobby->DecodeAddress(myIP, myPort, address, false);
			}
			else
			{
				// We are the new host, we should return quickly
				pInfo->SetIsNewHost(true);
				NetLog("[Host Migration]: becoming the new host for " PRFORMAT_SH "...", PRARG_SH(sessionIndex));

				const SCryLobbyParameters& lobbyParams = m_lobby->GetLobbyParameters();
				uint16 port = lobbyParams.m_listenPort;
				cry_sprintf(address, HOST_MIGRATION_MAX_SERVER_NAME_SIZE, "%s:%d", LOCAL_CONNECTION_STRING, port);
			}
		}

		NetLog("[Host Migration]: Using %s as connection address", address);
		success = true;
	}

	return success;
}

void CCryPSNMatchMaking::HostMigrationStartNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	// Ensure we're the host and we've not started the session
	if ((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_HOST | CRYSESSION_LOCAL_FLAG_STARTED)) == CRYSESSION_LOCAL_FLAG_HOST)
	{
		// Determine if host hinting has a better host
		SHostHintInformation newHost;
		CryMatchMakingConnectionID newHostID;
		if (FindConnectionFromSessionUID(pTask->session, pSession->newHostUID, &newHostID))
		{
			SSession::SRConnection* pConnection = &pSession->remoteConnection[newHostID];
			newHost = pConnection->hostHintInfo;
		}

		bool isBetterHost = false;
		if (SortNewHostPriorities_ComparatorHelper(newHost, pSession->hostHintInfo) < 0)
		{
			isBetterHost = true;
		}

		if (isBetterHost)
		{
			pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GRANTROOM_REQUEST_SLOT] = m_lobby->MemAlloc(sizeof(SGrantRoomOwnerParamData));
			if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GRANTROOM_REQUEST_SLOT] != TMemInvalidHdl)
			{
				if (HostMigrationInitiate(pTask->session, eDC_Unknown))
				{
					const uint32 maxBufferSize = CryLobbyPacketHeaderSize;
					uint8 buffer[maxBufferSize];
					CCryLobbyPacket packet;

					NetLog("[Host Migration]: local server is not best host for "  PRFORMAT_SH "- instructing clients to choose new host (sent from " PRFORMAT_UID ")", PRARG_SH(pTask->session), PRARG_UID(pSession->localConnection.uid));
					packet.SetWriteBuffer(buffer, maxBufferSize);
					packet.StartWrite(ePSNPT_HostMigrationStart, true);

					SendToAll(mmTaskID, &packet, pTask->session, CryMatchMakingInvalidConnectionID);
					pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_HOST;

					SGrantRoomOwnerParamData* pGrantRoomOwnerData = (SGrantRoomOwnerParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GRANTROOM_REQUEST_SLOT]);
					memset(pGrantRoomOwnerData, 0, sizeof(SGrantRoomOwnerParamData));

					pGrantRoomOwnerData->m_reqParam.roomId = pSession->m_roomId;
					pGrantRoomOwnerData->m_reqParam.newOwner = (SceNpMatching2RoomMemberId)(pSession->newHostUID.m_uid);

					m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
				}
				else
				{
					NetLog("[Host Migration]: local server is not best host for " PRFORMAT_SH ", but session is not migratable", PRARG_SH(pTask->session));
					UpdateTaskError(mmTaskID, eCLE_SessionNotMigratable);
					StopTaskRunning(mmTaskID);
				}
			}
			else
			{
				UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			}

			if (pTask->error != eCLE_Success)
			{
				StopTaskRunning(mmTaskID);
			}
		}
		else
		{
			NetLog("[Host Migration]: local server is best host for " PRFORMAT_SH, PRARG_SH(pTask->session));
			StopTaskRunning(mmTaskID);
		}
	}
	else
	{
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_STARTED)
		{
			NetLog("[Host Migration]: SessionEnsureBestHost() called after the session had started");
		}

		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::TickHostMigrationStartNT(CryMatchMakingTaskID mmTaskID)
{
	m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
	if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
	{
		STask* pTask = &m_task[mmTaskID];

		if (pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GRANTROOM_REQUEST_SLOT] != TMemInvalidHdl)
		{
			SGrantRoomOwnerParamData* pGrantRoomOwnerData = (SGrantRoomOwnerParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GRANTROOM_REQUEST_SLOT]);

			StartSubTask(eST_WaitingForGrantRoomOwnerRequestCallback, mmTaskID);

			int ret = sceNpMatching2GrantRoomOwner(m_pPSNSupport->GetMatchmakingContextId(), &pGrantRoomOwnerData->m_reqParam, NULL, &pTask->m_reqId);
			if (ret == PSN_OK)
			{
				//-- waiting for callback
			}
			else
			{
				NetLog("sceNpMatching2GrantRoomOwnerRequest : error %08X", ret);
				//-- triggering a PSN error will cause an error event into SupportCallback() and set a task error if it needs to clean up.
				//-- otherwise, we have to update the task error ourself.
				if (!m_pPSNSupport->HandlePSNError(ret))
				{
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}
	}
}

bool CCryPSNMatchMaking::EventRequestResponse_GrantRoomOwner(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data)
{
	// sanity
	assert(pTask->subTask == eST_WaitingForGrantRoomOwnerRequestCallback);
	assert(data.m_requestEvent.m_event == REQUEST_EVENT_GRANT_ROOM_OWNER);
	assert(data.m_requestEvent.m_dataSize == 0);

	// We don't really care if this GrantRoomOwnerRequest succeeds for fails, so we'll just mark the task as complete
	_this->UpdateTaskError(taskId, eCLE_Success);
	_this->StopTaskRunning(taskId);

	// this event is handled
	return true;
}

void CCryPSNMatchMaking::ProcessHostMigrationStart(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryLobbySessionHandle h = CryLobbyInvalidSessionHandle;
	SCryMatchMakingConnectionUID hostUID = pPacket->GetFromConnectionUID();
	CryMatchMakingConnectionID mmCxID = CryMatchMakingInvalidConnectionID;

	if (FindConnectionFromUID(hostUID, &h, &mmCxID))
	{
		SSession* pSession = &m_sessions[h];

		NetLog("[Host Migration]: received host migration push event for " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, hostUID));

		if (pSession->IsUsed() && !(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
		{
			if (pSession->hostMigrationInfo.m_state == eHMS_Idle)
			{
				NetLog("[Host Migration]: remote server is not best host for " PRFORMAT_SH " - instructed to choose new host", PRARG_SH(h));
				HostMigrationInitiate(h, eDC_Unknown);
			}
			else
			{
				NetLog("[Host Migration]: ignoring pushed migration event - already migrating " PRFORMAT_SH, PRARG_SH(h));
			}
		}
	}
	else
	{
		NetLog("[Host Migration]: received push migration event from unknown host " PRFORMAT_UID, PRARG_UID(hostUID));
	}
}
			#endif // NETWORK_HOST_MIGRATION

ECryLobbyError CCryPSNMatchMaking::SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError rc = eCLE_Success;

			#if NETWORK_HOST_MIGRATION
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if (h != CryLobbyInvalidSessionHandle)
	{
		CryMatchMakingTaskID mmTaskID;
		SSession* pSession = &m_sessions[h];
		rc = StartTask(eT_SessionMigrateHostStart, false, pSession->localConnection.users[0], &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (rc == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
			#endif

	return rc;
}

void CCryPSNMatchMaking::AddVoiceUser(SSession::SRoomMember* pMember)
{
			#if USE_PSN_VOICE
	CCryPSNVoice* pVoice = (CCryPSNVoice*)m_pService->GetVoice();
	if (pVoice)
	{
		assert(pMember->IsValid(ePSNMI_Other));
		pVoice->AddRemoteTalker(pMember->m_npId, pMember->m_peerAddr, pMember->m_peerPort, (pMember->m_signalingState == ePSNCS_Active) ? true : false);
	}
			#endif // USE_PSN_VOICE
}

void CCryPSNMatchMaking::ClearVoiceUser(SSession::SRoomMember* pMember)
{
			#if USE_PSN_VOICE
	CCryPSNVoice* pVoice = (CCryPSNVoice*)m_pService->GetVoice();
	if (pVoice)
	{
		assert(pMember->m_valid & ePSNMI_Other);
		pVoice->ClearRemoteTalker(pMember->m_npId);
	}
			#endif // USE_PSN_VOICE
}

void CCryPSNMatchMaking::UpdateVoiceUsers()
{
			#if USE_PSN_VOICE
	CCryPSNVoice* pVoice = (CCryPSNVoice*)m_pService->GetVoice();
	if (pVoice)
	{
		bool bNeedsVoice = false;

		for (uint32 i = 0; ((i < MAX_MATCHMAKING_SESSIONS) && (bNeedsVoice == false)); i++)
		{
			SSession* pSession = &m_sessions[i];
			if (pSession->IsUsed())
			{
				for (uint32 j = 0; j < MAX_PSN_ROOM_MEMBERS; j++)
				{
					SSession::SRoomMember* pMember = &pSession->m_members[j];
					if (pMember->IsValid(ePSNMI_Other))
					{
						bNeedsVoice = true;
						break;
					}
				}
			}
		}

		if (bNeedsVoice && !m_pPSNSupport->IsChatRestricted())
		{
			pVoice->Activate();
		}
		else
		{
			pVoice->Deactivate();
		}
	}
			#endif //USE_PSN_VOICE
}

ECryLobbyError CCryPSNMatchMaking::CopySessionIDToBuffer(CrySessionID sessionId, void* pBuffer, size_t& bufferSize)
{
	if (SCryPSNSessionID* pData = (SCryPSNSessionID*)sessionId.get())
	{
		size_t requiredSize = sizeof(SCryPSNSessionID);
		if (bufferSize >= requiredSize)
		{
			memcpy(pBuffer, pData, requiredSize);
			bufferSize = requiredSize;
			return eCLE_Success;
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
	return eCLE_InvalidSession;
}

ECryLobbyError CCryPSNMatchMaking::CopySessionIDFromBuffer(const void* pBuffer, size_t bufferSize, CrySessionID& sessionId)
{
	size_t requiredSize = sizeof(SCryPSNSessionID);
	if (bufferSize == requiredSize)
	{
		SCryPSNSessionID* pData = new SCryPSNSessionID();
		// cppcheck-suppress memsetClass (if used in conjunction with CopySessionIDToBuffer(), this is ok)
		memcpy(pData, pBuffer, requiredSize);
		sessionId = pData;
		return eCLE_Success;
	}
	return eCLE_InvalidParam;
}

void CCryPSNMatchMaking::ForceFromInvite(CrySessionID sessionId)
{
	if (SCryPSNSessionID* pData = (SCryPSNSessionID*)sessionId.get())
	{
		pData->m_fromInvite = true;
	}
}

ECryLobbyError CCryPSNMatchMaking::SessionSetAdvertisementData(CrySessionHandle gh, uint8* pData, uint32 dataSize, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError rc = eCLE_InvalidSession;
	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	if (h != CryLobbyInvalidSessionHandle)
	{
		CryMatchMakingTaskID mmTaskID;
		SSession* pSession = &m_sessions[h];
		rc = StartTask(eT_SessionSetAdvertisementData, false, 0, &mmTaskID, pTaskID, h, (void*)pCb, pCbArg);
		if (rc == eCLE_Success)
		{
			rc = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_UPDATEADVERT_REQUEST_SLOT, NULL, 1, sizeof(SUpdateAdvertisementDataParamData));
			if (rc == eCLE_Success)
			{
				rc = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_WEBAPI_UPDATEADVERT_REQUEST_SLOT, NULL, 1, sizeof(SCryPSNOrbisWebApiUpdateSessionInput));
				if (rc == eCLE_Success)
				{
					SCryPSNOrbisWebApiUpdateSessionInput* pAdvertInput = (SCryPSNOrbisWebApiUpdateSessionInput*)m_lobby->MemGetPtr(m_task[mmTaskID].params[PSN_MATCHMAKING_WEBAPI_UPDATEADVERT_REQUEST_SLOT]);
					memset(pAdvertInput, 0, sizeof(SCryPSNOrbisWebApiUpdateSessionInput));

					memcpy(&pAdvertInput->sessionId, &pSession->m_sessionId, sizeof(SceNpSessionId));

					pAdvertInput->advertisement.m_pData = pAdvertInput->data;
					pAdvertInput->advertisement.m_sizeofData = MIN(dataSize, CRY_WEBAPI_UPDATESESSION_LINKDATA_MAX_SIZE);
					memcpy(pAdvertInput->data, pData, pAdvertInput->advertisement.m_sizeofData);

					SConfigurationParams neededInfo = {
						CLCC_PSN_UPDATE_SESSION_ADVERTISEMENT, { NULL }
					};
					neededInfo.m_pData = (void*)&pAdvertInput->advertisement;
					m_lobby->GetConfigurationInformation(&neededInfo, 1);

					m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

					FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
				}
			}

			if (rc != eCLE_Success)
			{
				FreeTask(mmTaskID);
			}
		}
	}

	return rc;
}

void CCryPSNMatchMaking::TickSessionSetAdvertisementData(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
		if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
		{
			if ((pTask->params[PSN_MATCHMAKING_PSNSUPPORT_UPDATEADVERT_REQUEST_SLOT] != TMemInvalidHdl) &&
			    (pTask->params[PSN_MATCHMAKING_WEBAPI_UPDATEADVERT_REQUEST_SLOT] != TMemInvalidHdl))
			{
				SUpdateAdvertisementDataParamData* pParam = (SUpdateAdvertisementDataParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_UPDATEADVERT_REQUEST_SLOT]);

				StartSubTask(eST_WaitingForUpdateAdvertisementDataWebApiCallback, mmTaskID);

				pParam->m_jobId = m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::UpdateSession, pTask->params[PSN_MATCHMAKING_WEBAPI_UPDATEADVERT_REQUEST_SLOT]);
				if (pParam->m_jobId != INVALID_WEBAPI_JOB_ID)
				{
					//-- waiting for callback
				}
				else
				{
					// error - didn't add webapi job
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
			else
			{
				// error - bad param slots
				UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			}
		}
	}

	if ((pTask->error != eCLE_Success) || pTask->canceled)
	{
		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::EventWebApiUpdateSessionAdvertisement(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->session != CryLobbyInvalidSessionHandle)
	{
		if (data.m_webApiEvent.m_error == PSN_OK)
		{
			// not expecting a response body, just mark task successful and stop
			UpdateTaskError(mmTaskID, eCLE_Success);
			StopTaskRunning(mmTaskID);
			return;
		}
	}

	// something went wrong
	UpdateTaskError(mmTaskID, eCLE_InternalError);
	StopTaskRunning(mmTaskID);
}

void CCryPSNMatchMaking::EndSessionSetAdvertisementData(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
}

ECryLobbyError CCryPSNMatchMaking::SessionGetAdvertisementData(CrySessionID sessionId, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetAdvertisementDataCallback pCb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError rc = eCLE_InvalidSession;
	if (sessionId != CrySessionInvalidID)
	{
		CryMatchMakingTaskID mmTaskID;
		rc = StartTask(eT_SessionGetAdvertisementData, false, 0, &mmTaskID, pTaskID, CryLobbyInvalidSessionHandle, (void*)pCb, pCbArg);
		if (rc == eCLE_Success)
		{
			rc = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_PSNSUPPORT_GETADVERT_REQUEST_SLOT, NULL, 1, sizeof(SGetAdvertisementDataParamData));
			if (rc == eCLE_Success)
			{
				rc = CreateTaskParam(mmTaskID, PSN_MATCHMAKING_WEBAPI_GETADVERT_REQUEST_SLOT, NULL, 1, sizeof(SCryPSNOrbisWebApiGetSessionLinkDataInput));
				if (rc == eCLE_Success)
				{
					SCryPSNSessionID* pPSNSessionId = (SCryPSNSessionID*)sessionId.get();
					SCryPSNOrbisWebApiGetSessionLinkDataInput* pAdvertInput = (SCryPSNOrbisWebApiGetSessionLinkDataInput*)m_lobby->MemGetPtr(m_task[mmTaskID].params[PSN_MATCHMAKING_WEBAPI_GETADVERT_REQUEST_SLOT]);
					memcpy(&pAdvertInput->sessionId, &pPSNSessionId->m_sessionId, sizeof(SceNpSessionId));

					m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);

					FROM_GAME_TO_LOBBY(&CCryPSNMatchMaking::StartTaskRunning, this, mmTaskID);
				}
			}

			if (rc != eCLE_Success)
			{
				FreeTask(mmTaskID);
			}
		}
	}

	NetLog("[Lobby] Start SessionGetAdvertisementData error %d", rc);
	return rc;
}

void CCryPSNMatchMaking::TickSessionGetAdvertisementData(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		m_pPSNSupport->ResumeTransitioning(ePSNOS_Matchmaking_Available);
		if (m_pPSNSupport->HasTransitioningReachedState(ePSNOS_Matchmaking_Available))
		{
			if ((pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GETADVERT_REQUEST_SLOT] != TMemInvalidHdl) &&
			    (pTask->params[PSN_MATCHMAKING_WEBAPI_GETADVERT_REQUEST_SLOT] != TMemInvalidHdl))
			{
				SGetAdvertisementDataParamData* pParam = (SGetAdvertisementDataParamData*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_PSNSUPPORT_GETADVERT_REQUEST_SLOT]);

				StartSubTask(eST_WaitingForGetAdvertisementDataWebApiCallback, mmTaskID);

				pParam->m_jobId = m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::GetSessionLinkData, pTask->params[PSN_MATCHMAKING_WEBAPI_GETADVERT_REQUEST_SLOT]);
				if (pParam->m_jobId != INVALID_WEBAPI_JOB_ID)
				{
					//-- waiting for callback
				}
				else
				{
					// error - didnt queue webapi job
					UpdateTaskError(mmTaskID, eCLE_InternalError);
				}
			}
			else
			{
				// error - no slots allocated
				UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
			}
		}
	}

	if ((pTask->error != eCLE_Success) || pTask->canceled)
	{
		StopTaskRunning(mmTaskID);
	}
}

void CCryPSNMatchMaking::EventWebApiGetSessionLinkData(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data)
{
	STask* pTask = &m_task[mmTaskID];

	if (data.m_webApiEvent.m_error == PSN_OK)
	{
		if (data.m_webApiEvent.m_pResponseBody)
		{
			if ((data.m_webApiEvent.m_pResponseBody->eType == SCryPSNWebApiResponseBody::E_RAW) &&
			    (data.m_webApiEvent.m_pResponseBody->rawResponseBodyHdl != TMemInvalidHdl) &&
			    (data.m_webApiEvent.m_pResponseBody->rawResponseBodySize > 0))
			{
				// success.
				pTask->params[PSN_MATCHMAKING_WEBAPI_GETADVERT_RESPONSE_SLOT] = data.m_webApiEvent.m_pResponseBody->rawResponseBodyHdl;
				pTask->numParams[PSN_MATCHMAKING_WEBAPI_GETADVERT_RESPONSE_SLOT] = (uint32)data.m_webApiEvent.m_pResponseBody->rawResponseBodySize;

				// We don't want the data to be deleted when this function returns since we copied the handle.
				// If we invalidate the handle, it won't be deleted when the event is cleared up.
				data.m_webApiEvent.m_pResponseBody->rawResponseBodyHdl = TMemInvalidHdl;
				data.m_webApiEvent.m_pResponseBody->rawResponseBodySize = 0;

				UpdateTaskError(mmTaskID, eCLE_Success);
				StopTaskRunning(mmTaskID);
				return;
			}
		}
	}

	// something went wrong
	UpdateTaskError(mmTaskID, eCLE_InternalError);
	StopTaskRunning(mmTaskID);
}

void CCryPSNMatchMaking::EndSessionGetAdvertisementData(CryMatchMakingTaskID mmTaskID)
{
	uint8* pData = NULL;
	uint32 dataSize = 0;

	STask* pTask = &m_task[mmTaskID];
	if (pTask->error == eCLE_Success)
	{
		pData = (uint8*)m_lobby->MemGetPtr(pTask->params[PSN_MATCHMAKING_WEBAPI_GETADVERT_RESPONSE_SLOT]);
		dataSize = pTask->numParams[PSN_MATCHMAKING_WEBAPI_GETADVERT_RESPONSE_SLOT];
	}

	((CryMatchmakingSessionGetAdvertisementDataCallback)pTask->cb)(pTask->lTaskID, pTask->error, pData, dataSize, pTask->cbArg);
}

		#endif // USE_CRY_MATCHMAKING
	#endif   // USE_PSN
#endif     // CRY_PLATFORM_ORBIS
