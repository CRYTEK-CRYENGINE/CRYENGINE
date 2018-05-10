// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   Convert SCE PSN Matching2 request, signaling and room responses into an internal format that we can pass between threads.
   Memory is allocated in Memento, so be sure to clean it when you don't need it.
 */

#ifndef __CRYPSN2_RESPONSE_H__
#define __CRYPSN2_RESPONSE_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#define MAX_ROOM_EXTERNAL_BIN_ATTRIBUTES    (SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM)
		#define MAX_ROOM_INTERNAL_BIN_ATTRIBUTES    (SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_NUM)
		#define MAX_ROOM_SEARCHABLE_BIN_ATTRIBUTES  (SCE_NP_MATCHING2_ROOM_SEARCHABLE_BIN_ATTR_EXTERNAL_NUM)
		#define MAX_ROOM_SEARCHABLE_UINT_ATTRIBUTES (SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM)
		#define MAX_MEMBER_INTERNAL_BIN_ATTRIBUTES  (SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_NUM)
		#define MAX_BIN_ATTRIBUTE_SIZE              (256)             // use largest of SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_MAX_SIZE,
                                                                  // SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE,
                                                                  // SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_MAX_SIZE,
                                                                  // SCE_NP_MATCHING2_ROOM_SEARCHABLE_BIN_ATTR_EXTERNAL_MAX_SIZE

		#define REQUEST_EVENT_GET_WORLD_INFO_LIST            (SCE_NP_MATCHING2_REQUEST_EVENT_GET_WORLD_INFO_LIST)
		#define REQUEST_EVENT_LEAVE_ROOM                     (SCE_NP_MATCHING2_REQUEST_EVENT_LEAVE_ROOM)
		#define REQUEST_EVENT_SET_ROOM_MEMBER_DATA_INTERNAL  (SCE_NP_MATCHING2_REQUEST_EVENT_SET_ROOM_MEMBER_DATA_INTERNAL)
		#define REQUEST_EVENT_SET_ROOM_DATA_EXTERNAL         (SCE_NP_MATCHING2_REQUEST_EVENT_SET_ROOM_DATA_EXTERNAL)
		#define REQUEST_EVENT_SET_ROOM_DATA_INTERNAL         (SCE_NP_MATCHING2_REQUEST_EVENT_SET_ROOM_DATA_INTERNAL)
		#define REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST    (SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST)
		#define REQUEST_EVENT_SEARCH_ROOM                    (SCE_NP_MATCHING2_REQUEST_EVENT_SEARCH_ROOM)
		#define REQUEST_EVENT_SIGNALING_GET_PING_INFO        (SCE_NP_MATCHING2_REQUEST_EVENT_SIGNALING_GET_PING_INFO)
		#define REQUEST_EVENT_JOIN_ROOM                      (SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_ROOM)
		#define REQUEST_EVENT_CREATE_JOIN_ROOM               (SCE_NP_MATCHING2_REQUEST_EVENT_CREATE_JOIN_ROOM)
		#define REQUEST_EVENT_GRANT_ROOM_OWNER               (SCE_NP_MATCHING2_REQUEST_EVENT_GRANT_ROOM_OWNER)
		#define ROOM_EVENT_MEMBER_JOINED                     (SCE_NP_MATCHING2_ROOM_EVENT_MEMBER_JOINED)
		#define ROOM_EVENT_MEMBER_LEFT                       (SCE_NP_MATCHING2_ROOM_EVENT_MEMBER_LEFT)
		#define ROOM_EVENT_ROOM_OWNER_CHANGED                (SCE_NP_MATCHING2_ROOM_EVENT_ROOM_OWNER_CHANGED)
		#define ROOM_EVENT_KICKED_OUT                        (SCE_NP_MATCHING2_ROOM_EVENT_KICKEDOUT)
		#define ROOM_EVENT_ROOM_DESTROYED                    (SCE_NP_MATCHING2_ROOM_EVENT_ROOM_DESTROYED)
		#define ROOM_EVENT_UPDATED_ROOM_DATA_INTERNAL        (SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_ROOM_DATA_INTERNAL)
		#define ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL (SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL)
		#define ROOM_EVENT_UPDATED_SIGNALING_OPT_PARAM       (SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_SIGNALING_OPT_PARAM)
		#define SIGNALING_EVENT_DEAD                         (SCE_NP_MATCHING2_SIGNALING_EVENT_DEAD)
		#define SIGNALING_EVENT_CONNECTED                    (SCE_NP_MATCHING2_SIGNALING_EVENT_ESTABLISHED)
		#define SIGNALING_EVENT_NETINFO_RESULT               (SCE_NP_MATCHING2_SIGNALING_EVENT_NETINFO_RESULT)

struct SCryPSNServerInfoResponse
{
	struct SServerInfo
	{
		SceNpMatching2ServerId serverId;
	};
	SServerInfo server;
};

struct SCryPSNBinAttribute
{
	SceNpMatching2AttributeId m_id;
	uint8                     m_data[MAX_BIN_ATTRIBUTE_SIZE];
	uint16                    m_dataSize;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberBinAttrInternal& memberBinAttrInternal);
	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomBinAttrInternal& roomBinAttrInternal);
	bool Clone(CCryLobby* pLobby, SceNpMatching2BinAttr& binAttr);
	void Release(CCryLobby* pLobby) {};
};

struct SCryPSNUIntAttribute
{
	SceNpMatching2AttributeId m_id;
	uint32                    m_num;

	bool Clone(CCryLobby* pLobby, SceNpMatching2IntAttr& intAttr);
	void Release(CCryLobby* pLobby) {};
};

struct SCryPSNWorld
{
	enum SortWorldsState
	{
		SORT_WORLD_NONE = 0,
		SORT_WORLD_NEEDS_SORTING,
		SORT_WORLD_SORTED,
	};

	SceNpMatching2WorldId m_worldId;
	uint32                m_numRooms;
	uint32                m_numTotalRoomMembers;
	uint32                m_score;
	uint8                 m_sortState;

	bool Clone(CCryLobby* pLobby, SceNpMatching2World& world);
	void Release(CCryLobby* pLobby) {};
};

struct SCryPSNRoomMemberDataInternal
{
	SceNpId                    m_npId;
	SceNpMatching2RoomMemberId m_memberId;
	SceNpMatching2FlagAttr     m_flagAttr;
	SceNpMatching2NatType      m_natType;

	SCryPSNBinAttribute        m_binAttributes[MAX_MEMBER_INTERNAL_BIN_ATTRIBUTES];
	uint16                     m_numBinAttributes;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberDataInternalA& member);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNRoomDataExternal
{
	SceNpMatching2ServerId m_serverId;
	SceNpMatching2WorldId  m_worldId;
	SceNpMatching2RoomId   m_roomId;

	SceNpMatching2FlagAttr m_flagAttr;

	SceNpId                m_owner;

	SCryPSNBinAttribute    m_binAttributes[MAX_ROOM_EXTERNAL_BIN_ATTRIBUTES];
	SCryPSNBinAttribute    m_searchableBinAttributes[MAX_ROOM_SEARCHABLE_BIN_ATTRIBUTES];
	SCryPSNUIntAttribute   m_searchableUIntAttributes[MAX_ROOM_SEARCHABLE_UINT_ATTRIBUTES];
	uint16                 m_numBinAttributes;
	uint16                 m_numSearchableBinAttributes;
	uint16                 m_numSearchableUIntAttributes;

	uint16                 m_maxSlots;
	uint16                 m_numPublicSlots;
	uint16                 m_numPrivateSlots;
	uint16                 m_numOpenPublicSlots;
	uint16                 m_numOpenPrivateSlots;
	uint16                 m_numRoomMembers;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomDataExternalA& roomDataExternal);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNRoomDataInternal
{
	SceNpMatching2ServerId m_serverId;
	SceNpMatching2WorldId  m_worldId;
	SceNpMatching2RoomId   m_roomId;

	SceNpMatching2FlagAttr m_flagAttr;

	SCryPSNBinAttribute    m_binAttributes[MAX_ROOM_INTERNAL_BIN_ATTRIBUTES];
	uint16                 m_numBinAttributes;

	uint16                 m_maxSlots;
	uint16                 m_numPublicSlots;
	uint16                 m_numPrivateSlots;
	uint16                 m_numOpenPublicSlots;
	uint16                 m_numOpenPrivateSlots;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomDataInternal& roomDataInternal);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNRoomMemberDataInternalList
{
	TMemHdl                        m_memHdl;
	SCryPSNRoomMemberDataInternal* m_pRoomMembers;
	uint16                         m_numRoomMembers;
	uint16                         m_me;
	uint16                         m_owner;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberDataInternalListA& memberList);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNGetWorldInfoListResponse
{
	TMemHdl       m_memHdl;
	SCryPSNWorld* m_pWorlds;
	uint16        m_numWorlds;

	bool Clone(CCryLobby* pLobby, SceNpMatching2GetWorldInfoListResponse* pResponse);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNCreateJoinRoomResponse
{
	SCryPSNRoomDataInternal           m_roomInfo;
	SCryPSNRoomMemberDataInternalList m_roomMembers;

	bool Clone(CCryLobby* pLobby, SceNpMatching2CreateJoinRoomResponseA* pResponse);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNJoinRoomResponse
{
	SCryPSNRoomDataInternal           m_roomInfo;
	SCryPSNRoomMemberDataInternalList m_roomMembers;

	bool Clone(CCryLobby* pLobby, SceNpMatching2JoinRoomResponseA* pResponse);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNSearchRoomResponse
{
	TMemHdl                  m_memHdl;
	SCryPSNRoomDataExternal* m_pRooms;
	uint16                   m_numRooms;

	bool Clone(CCryLobby* pLobby, SceNpMatching2SearchRoomResponseA* pResponse);
	bool Clone(CCryLobby* pLobby, SceNpMatching2GetRoomDataExternalListResponseA* pResponse);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNLeaveRoomResponse
{
	SceNpMatching2RoomId m_roomId;

	bool Clone(CCryLobby* pLobby, SceNpMatching2LeaveRoomResponse* pResponse);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNSignalingGetPingInfoResponse
{
	SceNpMatching2ServerId m_serverId;
	SceNpMatching2WorldId  m_worldId;
	SceNpMatching2RoomId   m_roomId;
	uint32                 m_rtt;

	bool Clone(CCryLobby* pLobby, SceNpMatching2SignalingGetPingInfoResponse* pResponse);
	void Release(CCryLobby* pLobby) {};
};

struct SCryPSNRoomMemberUpdateInfoResponse
{
	SCryPSNRoomMemberDataInternal m_member;
	SceNpMatching2EventCause      m_eventCause;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberUpdateInfoA* pResponse);
	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomMemberDataInternalUpdateInfoA* pResponse);
	void Release(CCryLobby* pLobby);
};

struct SCryPSNRoomOwnerUpdateInfoResponse
{
	SceNpMatching2RoomMemberId m_prevOwner;
	SceNpMatching2RoomMemberId m_newOwner;
	SceNpMatching2EventCause   m_eventCause;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomOwnerUpdateInfo* pResponse);
	void Release(CCryLobby* pLobby) {};
};

struct SCryPSNRoomUpdateInfoResponse
{
	SceNpMatching2EventCause m_eventCause;
	int                      m_errorCode;

	bool Clone(CCryLobby* pLobby, SceNpMatching2RoomUpdateInfo* pResponse);
	void Release(CCryLobby* pLobby) {};
};

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_RESPONSE_H__
