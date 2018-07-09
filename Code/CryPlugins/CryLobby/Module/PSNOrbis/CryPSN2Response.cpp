// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include <CryCore/Platform/IPlatformOS.h> //Used to pass messages to the PlatformOS

		#include <np.h>
		#include <CryCore/Platform/OrbisSpecific.h>

		#include "CryPSN2Response.h"

bool SCryPSNBinAttribute::Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberBinAttrInternal& memberBinAttrInternal)
{
	m_id = memberBinAttrInternal.data.id;
	if (memberBinAttrInternal.data.ptr)
	{
		memcpy(m_data, memberBinAttrInternal.data.ptr, memberBinAttrInternal.data.size);
		m_dataSize = memberBinAttrInternal.data.size;
		return true;
	}
	else if (memberBinAttrInternal.data.size == 0)
	{
		m_dataSize = 0;
		return true;
	}
	else
	{
		m_dataSize = 0;
		return false;
	}
}

bool SCryPSNBinAttribute::Clone(CCryLobby* pLobby, SceNpMatching2RoomBinAttrInternal& roomBinAttrInternal)
{
	m_id = roomBinAttrInternal.data.id;
	if (roomBinAttrInternal.data.ptr)
	{
		memcpy(m_data, roomBinAttrInternal.data.ptr, roomBinAttrInternal.data.size);
		m_dataSize = roomBinAttrInternal.data.size;
		return true;
	}
	else if (roomBinAttrInternal.data.size == 0)
	{
		m_dataSize = 0;
		return true;
	}
	else
	{
		m_dataSize = 0;
		return false;
	}
}

bool SCryPSNBinAttribute::Clone(CCryLobby* pLobby, SceNpMatching2BinAttr& binAttr)
{
	m_id = binAttr.id;
	if (binAttr.ptr)
	{
		memcpy(m_data, binAttr.ptr, binAttr.size);
		m_dataSize = binAttr.size;
		return true;
	}
	else if (binAttr.size == 0)
	{
		m_dataSize = 0;
		return true;
	}
	else
	{
		m_dataSize = 0;
		return false;
	}
}

bool SCryPSNUIntAttribute::Clone(CCryLobby* pLobby, SceNpMatching2IntAttr& intAttr)
{
	m_id = intAttr.id;
	m_num = intAttr.num;
	return true;
}

bool SCryPSNWorld::Clone(CCryLobby* pLobby, SceNpMatching2World& world)
{
	m_worldId = world.worldId;
	m_numRooms = world.curNumOfRoom;
	m_numTotalRoomMembers = world.curNumOfTotalRoomMember;
	return true;
}

bool SCryPSNRoomMemberDataInternal::Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberDataInternalA& member)
{
	memcpy((void*)&m_npId, &member.npId, sizeof(SceNpId));

	m_memberId = member.memberId;
	m_flagAttr = member.flagAttr;
	m_natType = member.natType;

	m_numBinAttributes = member.roomMemberBinAttrInternalNum;
	for (uint16 i = 0; i < m_numBinAttributes; i++)
	{
		if (!m_binAttributes[i].Clone(pLobby, member.roomMemberBinAttrInternal[i]))
		{
			return false;
		}
	}

	return true;
}

void SCryPSNRoomMemberDataInternal::Release(CCryLobby* pLobby)
{
	for (uint16 i = 0; i < m_numBinAttributes; i++)
	{
		m_binAttributes[i].Release(pLobby);
	}
}

bool SCryPSNRoomDataExternal::Clone(CCryLobby* pLobby, SceNpMatching2RoomDataExternalA& roomDataExternal)
{
	m_serverId = roomDataExternal.serverId;
	m_worldId = roomDataExternal.worldId;
	m_roomId = roomDataExternal.roomId;

	m_flagAttr = roomDataExternal.flagAttr;

	memcpy(&m_owner, roomDataExternal.owner, sizeof(SceNpId));

	m_numBinAttributes = roomDataExternal.roomBinAttrExternalNum;
	for (uint16 i = 0; i < m_numBinAttributes; i++)
	{
		if (!m_binAttributes[i].Clone(pLobby, roomDataExternal.roomBinAttrExternal[i]))
		{
			return false;
		}
	}
	m_numSearchableBinAttributes = roomDataExternal.roomSearchableBinAttrExternalNum;
	for (uint16 i = 0; i < m_numSearchableBinAttributes; i++)
	{
		if (!m_searchableBinAttributes[i].Clone(pLobby, roomDataExternal.roomSearchableBinAttrExternal[i]))
		{
			return false;
		}
	}
	m_numSearchableUIntAttributes = roomDataExternal.roomSearchableIntAttrExternalNum;
	for (uint16 i = 0; i < m_numSearchableUIntAttributes; i++)
	{
		if (!m_searchableUIntAttributes[i].Clone(pLobby, roomDataExternal.roomSearchableIntAttrExternal[i]))
		{
			return false;
		}
	}

	m_maxSlots = roomDataExternal.maxSlot;
	m_numPublicSlots = roomDataExternal.publicSlotNum;
	m_numPrivateSlots = roomDataExternal.privateSlotNum;
	m_numOpenPublicSlots = roomDataExternal.openPublicSlotNum;
	m_numOpenPrivateSlots = roomDataExternal.openPrivateSlotNum;
	m_numRoomMembers = roomDataExternal.curMemberNum;

	return true;
}

void SCryPSNRoomDataExternal::Release(CCryLobby* pLobby)
{
	for (uint16 i = 0; i < m_numBinAttributes; i++)
	{
		m_binAttributes[i].Release(pLobby);
	}
	for (uint16 i = 0; i < m_numSearchableBinAttributes; i++)
	{
		m_searchableBinAttributes[i].Release(pLobby);
	}
	for (uint16 i = 0; i < m_numSearchableUIntAttributes; i++)
	{
		m_searchableUIntAttributes[i].Release(pLobby);
	}
}

bool SCryPSNRoomDataInternal::Clone(CCryLobby* pLobby, SceNpMatching2RoomDataInternal& roomDataInternal)
{
	m_serverId = roomDataInternal.serverId;
	m_worldId = roomDataInternal.worldId;
	m_roomId = roomDataInternal.roomId;

	m_flagAttr = roomDataInternal.flagAttr;

	m_numBinAttributes = roomDataInternal.roomBinAttrInternalNum;
	for (uint16 i = 0; i < m_numBinAttributes; i++)
	{
		if (!m_binAttributes[i].Clone(pLobby, roomDataInternal.roomBinAttrInternal[i]))
		{
			return false;
		}
	}

	m_maxSlots = roomDataInternal.maxSlot;
		#if CRY_PLATFORM_ORBIS
	m_numPublicSlots = roomDataInternal.publicSlotNum;
	m_numPrivateSlots = roomDataInternal.privateSlotNum;
	m_numOpenPublicSlots = roomDataInternal.openPublicSlotNum;
	m_numOpenPrivateSlots = roomDataInternal.openPrivateSlotNum;
		#endif

	return true;
}

void SCryPSNRoomDataInternal::Release(CCryLobby* pLobby)
{
	for (uint16 i = 0; i < m_numBinAttributes; i++)
	{
		m_binAttributes[i].Release(pLobby);
	}
}

bool SCryPSNRoomMemberDataInternalList::Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberDataInternalListA& memberList)
{
	m_numRoomMembers = memberList.membersNum;
	m_memHdl = TMemInvalidHdl;
	m_pRoomMembers = NULL;
	m_me = 0;
	m_owner = 0;

	uint32 nAllocSize = m_numRoomMembers * sizeof(SCryPSNRoomMemberDataInternal);
	if (nAllocSize)
	{
		m_memHdl = pLobby->MemAlloc(nAllocSize);
		if (m_memHdl != TMemInvalidHdl)
		{
			m_pRoomMembers = (SCryPSNRoomMemberDataInternal*)pLobby->MemGetPtr(m_memHdl);
			memset(m_pRoomMembers, 0, nAllocSize);

			SceNpMatching2RoomMemberDataInternalA* pMember = memberList.members;

			for (uint16 i = 0; i < m_numRoomMembers; i++)
			{
				if (!m_pRoomMembers[i].Clone(pLobby, *pMember))
				{
					return false;
				}

				if (pMember == memberList.me)
				{
					m_me = i;
				}
				if (pMember == memberList.owner)
				{
					m_owner = i;
				}

				pMember = pMember->next;
			}

			return true;
		}
	}

	return false;
}

void SCryPSNRoomMemberDataInternalList::Release(CCryLobby* pLobby)
{
	if (m_memHdl != TMemInvalidHdl)
	{
		for (uint16 i = 0; i < m_numRoomMembers; i++)
		{
			m_pRoomMembers[i].Release(pLobby);
		}
		pLobby->MemFree(m_memHdl);
	}
}

bool SCryPSNGetWorldInfoListResponse::Clone(CCryLobby* pLobby, SceNpMatching2GetWorldInfoListResponse* pResponse)
{
	m_numWorlds = pResponse->worldNum;
	m_pWorlds = NULL;

	uint32 nAllocSize = m_numWorlds * sizeof(SCryPSNWorld);
	if (nAllocSize)
	{
		m_memHdl = pLobby->MemAlloc(nAllocSize);
		if (m_memHdl != TMemInvalidHdl)
		{
			m_pWorlds = (SCryPSNWorld*)pLobby->MemGetPtr(m_memHdl);
			memset(m_pWorlds, 0, nAllocSize);

			for (uint16 i = 0; i < m_numWorlds; i++)
			{
				if (!m_pWorlds[i].Clone(pLobby, pResponse->world[i]))
				{
					return false;
				}
			}

			return true;
		}
	}

	return false;
}

void SCryPSNGetWorldInfoListResponse::Release(CCryLobby* pLobby)
{
	if (m_memHdl != TMemInvalidHdl)
	{
		for (uint16 i = 0; i < m_numWorlds; i++)
		{
			m_pWorlds[i].Release(pLobby);
		}
		pLobby->MemFree(m_memHdl);
	}
}

bool SCryPSNCreateJoinRoomResponse::Clone(CCryLobby* pLobby, SceNpMatching2CreateJoinRoomResponseA* pResponse)
{
	if (m_roomInfo.Clone(pLobby, *(pResponse->roomDataInternal)))
	{
		#if CRY_PLATFORM_ORBIS
		if (m_roomMembers.Clone(pLobby, pResponse->memberList))
		#endif
		{
			return true;
		}
	}
	return false;
}

void SCryPSNCreateJoinRoomResponse::Release(CCryLobby* pLobby)
{
	m_roomInfo.Release(pLobby);
	m_roomMembers.Release(pLobby);
}

bool SCryPSNJoinRoomResponse::Clone(CCryLobby* pLobby, SceNpMatching2JoinRoomResponseA* pResponse)
{
	if (m_roomInfo.Clone(pLobby, *(pResponse->roomDataInternal)))
	{
		#if CRY_PLATFORM_ORBIS
		if (m_roomMembers.Clone(pLobby, pResponse->memberList))
		#endif
		{
			return true;
		}
	}
	return false;
}

void SCryPSNJoinRoomResponse::Release(CCryLobby* pLobby)
{
	m_roomInfo.Release(pLobby);
	m_roomMembers.Release(pLobby);
}

bool SCryPSNSearchRoomResponse::Clone(CCryLobby* pLobby, SceNpMatching2SearchRoomResponseA* pResponse)
{
	m_numRooms = 0;
	m_memHdl = TMemInvalidHdl;
	m_pRooms = NULL;

	SceNpMatching2RoomDataExternalA* pRoom = pResponse->roomDataExternal;
	while (pRoom)
	{
		m_numRooms++;
		pRoom = pRoom->next;
	}

	uint32 nAllocSize = m_numRooms * sizeof(SCryPSNRoomDataExternal);
	if (nAllocSize == 0)
	{
		return true;
	}
	else
	{
		m_memHdl = pLobby->MemAlloc(nAllocSize);
		if (m_memHdl != TMemInvalidHdl)
		{
			m_pRooms = (SCryPSNRoomDataExternal*)pLobby->MemGetPtr(m_memHdl);
			memset(m_pRooms, 0, nAllocSize);

			pRoom = pResponse->roomDataExternal;

			for (uint16 i = 0; i < m_numRooms; i++)
			{
				if (!m_pRooms[i].Clone(pLobby, *pRoom))
				{
					return false;
				}
				pRoom = pRoom->next;
			}

			return true;
		}
	}

	return false;
}

bool SCryPSNSearchRoomResponse::Clone(CCryLobby* pLobby, SceNpMatching2GetRoomDataExternalListResponseA* pResponse)
{
	m_numRooms = pResponse->roomDataExternalNum;
	m_memHdl = TMemInvalidHdl;
	m_pRooms = NULL;

	uint32 nAllocSize = m_numRooms * sizeof(SCryPSNRoomDataExternal);
	if (nAllocSize == 0)
	{
		return true;
	}
	else
	{
		m_memHdl = pLobby->MemAlloc(nAllocSize);
		if (m_memHdl != TMemInvalidHdl)
		{
			m_pRooms = (SCryPSNRoomDataExternal*)pLobby->MemGetPtr(m_memHdl);
			memset(m_pRooms, 0, nAllocSize);

			SceNpMatching2RoomDataExternalA* pRoom = pResponse->roomDataExternal;

			for (uint16 i = 0; i < m_numRooms; i++)
			{
				if (!m_pRooms[i].Clone(pLobby, *pRoom))
				{
					return false;
				}
				pRoom = pRoom->next;
			}

			return true;
		}
	}

	return false;
}

void SCryPSNSearchRoomResponse::Release(CCryLobby* pLobby)
{
	if (m_memHdl != TMemInvalidHdl)
	{
		for (uint16 i = 0; i < m_numRooms; i++)
		{
			m_pRooms[i].Release(pLobby);
		}
		pLobby->MemFree(m_memHdl);
	}
}

bool SCryPSNSignalingGetPingInfoResponse::Clone(CCryLobby* pLobby, SceNpMatching2SignalingGetPingInfoResponse* pResponse)
{
	m_serverId = pResponse->serverId;
	m_worldId = pResponse->worldId;
	m_roomId = pResponse->roomId;
	m_rtt = pResponse->rtt;

	return true;
}

bool SCryPSNRoomMemberUpdateInfoResponse::Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberUpdateInfoA* pResponse)
{
	SceNpMatching2RoomMemberDataInternalA* pMember = (SceNpMatching2RoomMemberDataInternalA*)pResponse->roomMemberDataInternal;

	if (!m_member.Clone(pLobby, *pMember))
	{
		return false;
	}

	m_eventCause = pResponse->eventCause;

	return true;
}

bool SCryPSNRoomMemberUpdateInfoResponse::Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberDataInternalUpdateInfoA* pResponse)
{
	SceNpMatching2RoomMemberDataInternalA* pMember = (SceNpMatching2RoomMemberDataInternalA*)pResponse->newRoomMemberDataInternal;

	if (!m_member.Clone(pLobby, *pMember))
	{
		return false;
	}

	m_eventCause = PSN_OK;

	return true;
}

void SCryPSNRoomMemberUpdateInfoResponse::Release(CCryLobby* pLobby)
{
	m_member.Release(pLobby);
}

bool SCryPSNRoomUpdateInfoResponse::Clone(CCryLobby* pLobby, SceNpMatching2RoomUpdateInfo* pResponse)
{
	m_eventCause = pResponse->eventCause;
	m_errorCode = pResponse->errorCode;

	return true;
}

TMemHdl CCryPSNSupport::CloneResponse(SceNpMatching2Event eventType, const void* pData, uint32* pReturnMemSize)
{
	*pReturnMemSize = 0;
	TMemHdl memHdl = TMemInvalidHdl;

	CCryLobby* pLobby = GetLobby();
	if (pLobby)
	{
		// We only need to clone a limited set of eventTypes
		switch (eventType)
		{
		case REQUEST_EVENT_GET_WORLD_INFO_LIST:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNGetWorldInfoListResponse));
				if (memHdl)
				{
					SCryPSNGetWorldInfoListResponse* pResponse = (SCryPSNGetWorldInfoListResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2GetWorldInfoListResponse*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNGetWorldInfoListResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case REQUEST_EVENT_SEARCH_ROOM:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNSearchRoomResponse));
				if (memHdl)
				{
					SCryPSNSearchRoomResponse* pResponse = (SCryPSNSearchRoomResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2SearchRoomResponseA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNSearchRoomResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case REQUEST_EVENT_JOIN_ROOM:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNJoinRoomResponse));
				if (memHdl)
				{
					SCryPSNJoinRoomResponse* pResponse = (SCryPSNJoinRoomResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2JoinRoomResponseA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNJoinRoomResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case REQUEST_EVENT_CREATE_JOIN_ROOM:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNCreateJoinRoomResponse));
				if (memHdl)
				{
					SCryPSNCreateJoinRoomResponse* pResponse = (SCryPSNCreateJoinRoomResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2CreateJoinRoomResponseA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNCreateJoinRoomResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case REQUEST_EVENT_LEAVE_ROOM:
			{
		#if CRY_PLATFORM_ORBIS
				// currently no need to make a LeaveRoom response clone on Orbis - we don't use it.
				// memHdl = pLobby->MemAlloc(sizeof(SCryPSNLeaveRoomResponse));
				// if (memHdl)
				// {
				//   SCryPSNLeaveRoomResponse* pResponse = (SCryPSNLeaveRoomResponse*)pLobby->MemGetPtr(memHdl);
				//	 if (pResponse->Clone(pLobby, (SceNpMatching2LeaveRoomResponse*)pData))
				//	 {
				//		 *pReturnMemSize = sizeof(SCryPSNLeaveRoomResponse);
				//	 }
				//	 else
				//	 {
				//		 pResponse->Release(pLobby);
				//		 memHdl = TMemInvalidHdl;
				//	 }
				// }
		#endif
			}
			break;
		case REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNSearchRoomResponse));
				if (memHdl)
				{
					SCryPSNSearchRoomResponse* pResponse = (SCryPSNSearchRoomResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2GetRoomDataExternalListResponseA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNSearchRoomResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case REQUEST_EVENT_SIGNALING_GET_PING_INFO:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNSignalingGetPingInfoResponse));
				if (memHdl)
				{
					SCryPSNSignalingGetPingInfoResponse* pResponse = (SCryPSNSignalingGetPingInfoResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2SignalingGetPingInfoResponse*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNSignalingGetPingInfoResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;

		case ROOM_EVENT_MEMBER_JOINED:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNRoomMemberUpdateInfoResponse));
				if (memHdl)
				{
					SCryPSNRoomMemberUpdateInfoResponse* pResponse = (SCryPSNRoomMemberUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2RoomMemberUpdateInfoA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNRoomMemberUpdateInfoResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case ROOM_EVENT_MEMBER_LEFT:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNRoomMemberUpdateInfoResponse));
				if (memHdl)
				{
					SCryPSNRoomMemberUpdateInfoResponse* pResponse = (SCryPSNRoomMemberUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2RoomMemberUpdateInfoA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNRoomMemberUpdateInfoResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case ROOM_EVENT_KICKED_OUT:
		case ROOM_EVENT_ROOM_DESTROYED:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNRoomUpdateInfoResponse));
				if (memHdl)
				{
					SCryPSNRoomUpdateInfoResponse* pResponse = (SCryPSNRoomUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2RoomUpdateInfo*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNRoomUpdateInfoResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		case ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL:
			{
				memHdl = pLobby->MemAlloc(sizeof(SCryPSNRoomMemberUpdateInfoResponse));
				if (memHdl)
				{
					SCryPSNRoomMemberUpdateInfoResponse* pResponse = (SCryPSNRoomMemberUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					if (pResponse->Clone(pLobby, (SceNpMatching2RoomMemberDataInternalUpdateInfoA*)pData))
					{
						*pReturnMemSize = sizeof(SCryPSNRoomMemberUpdateInfoResponse);
					}
					else
					{
						pResponse->Release(pLobby);
						memHdl = TMemInvalidHdl;
					}
				}
			}
			break;
		default:
			break;
		}
	}

	return memHdl;
}

void CCryPSNSupport::ReleaseClonedResponse(SceNpMatching2Event eventType, TMemHdl memHdl)
{
	if (memHdl != TMemInvalidHdl)
	{
		CCryLobby* pLobby = GetLobby();
		if (pLobby)
		{
			switch (eventType)
			{
			case REQUEST_EVENT_GET_WORLD_INFO_LIST:
				{
					SCryPSNGetWorldInfoListResponse* pResponse = (SCryPSNGetWorldInfoListResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case REQUEST_EVENT_SEARCH_ROOM:
				{
					SCryPSNSearchRoomResponse* pResponse = (SCryPSNSearchRoomResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case REQUEST_EVENT_JOIN_ROOM:
				{
					SCryPSNJoinRoomResponse* pResponse = (SCryPSNJoinRoomResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case REQUEST_EVENT_CREATE_JOIN_ROOM:
				{
					SCryPSNCreateJoinRoomResponse* pResponse = (SCryPSNCreateJoinRoomResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case REQUEST_EVENT_LEAVE_ROOM:
				{
		#if CRY_PLATFORM_ORBIS
					// currently no need to make a LeaveRoom response clone on Orbis - we don't use it
					// SCryPSNLeaveRoomResponse* pResponse = (SCryPSNLeaveRoomResponse*)pLobby->MemGetPtr(memHdl);
					// pResponse->Release(pLobby);
		#endif
				}
				break;
			case REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST:
				{
					SCryPSNSearchRoomResponse* pResponse = (SCryPSNSearchRoomResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case REQUEST_EVENT_SIGNALING_GET_PING_INFO:
				{
					SCryPSNSignalingGetPingInfoResponse* pResponse = (SCryPSNSignalingGetPingInfoResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;

			case ROOM_EVENT_MEMBER_JOINED:
				{
					SCryPSNJoinRoomResponse* pResponse = (SCryPSNJoinRoomResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case ROOM_EVENT_MEMBER_LEFT:
				{
					SCryPSNRoomMemberUpdateInfoResponse* pResponse = (SCryPSNRoomMemberUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case ROOM_EVENT_KICKED_OUT:
			case ROOM_EVENT_ROOM_DESTROYED:
				{
					SCryPSNRoomUpdateInfoResponse* pResponse = (SCryPSNRoomUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			case ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL:
				{
					SCryPSNRoomMemberUpdateInfoResponse* pResponse = (SCryPSNRoomMemberUpdateInfoResponse*)pLobby->MemGetPtr(memHdl);
					pResponse->Release(pLobby);
				}
				break;
			default:
				break;
			}

			pLobby->MemFree(memHdl);
		}
	}
}

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS
