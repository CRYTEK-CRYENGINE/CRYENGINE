// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "CommonICryMatchMaking.h"

class CCryLobbyPacket;

//! If the underlying SDK has a frontend invites can be accepted from then this callback will be called if an invite is accepted this way.
//! When this call back is called the game must switch to the given service and join the session.
//! If the user is in a session already then the old session must be deleted before the join is started.
//! Structure returned when a registered callback is triggered for invite acceptance.
struct SCryLobbyInviteAcceptedData
{
	ECryLobbyService m_service;           //!< Which of the CryLobby services this is for.
	uint32           m_user;              //!< Id of local user this pertains to.
	CrySessionID     m_id;                //!< Session identifier of which session to join.
	ECryLobbyError   m_error;
};

enum ECryLobbyInviteType
{
	eCLIT_InviteToSquad,
	eCLIT_JoinSessionInProgress,
	eCLIT_InviteToSession,
};

//! Separate data type for platforms where invites revolve around users, not sessions.
struct SCryLobbyUserInviteAcceptedData
{
	ECryLobbyService    m_service;
	uint32              m_user;
	CryUserID           m_inviterId;
	ECryLobbyError      m_error;
	ECryLobbyInviteType m_type;
};

enum ECableState
{
	eCS_Unknown   = -1,
	eCS_Unplugged = 0,
	eCS_Disconnected,                   //!< Indicates the network link is down (same as unplugged but means the game was using wireless).
	eCS_Connected,
};

enum EOnlineState
{
	eOS_Unknown   = -1,
	eOS_SignedOut = 0,
	eOS_SigningIn,
	eOS_SignedIn,
};

enum ENatType
{
	eNT_Unknown = -1,
	eNT_Open    = 0,
	eNT_Moderate,
	eNT_Strict
};

enum EForcedFromRoomReason
{
	eFFRR_Unknown = 0,
	eFFRR_Left,
	eFFRR_Kicked,
	eFFRR_ServerForced,
	eFFRR_ServerInternalError,
	eFFRR_ConnectionError,
	eFFRR_SignedOut
};

//! curState will indicate the new state of the ethernet interface.
struct SCryLobbyEthernetStateData
{
	ECableState m_curState;
};

//! m_curState will indicate the new state of the user identified by user (expect multiple of these).
//! m_reason gives you a reason for the state change. Only really meaningful if m_curState is eOS_SignedOut.
struct SCryLobbyOnlineStateData
{
	uint32       m_user;
	EOnlineState m_curState;
	uint32       m_reason;
	bool         m_serviceConnected;
};

struct SCryLobbyNatTypeData
{
	ENatType m_curState;            //!< Current nat type.
};

struct SCryLobbyRoomOwnerChanged
{
	CrySessionHandle m_session;
	uint32           m_ip;
	uint16           m_port;
};

struct SCryLobbyOnlineNameRejectedData
{
	uint32 m_user;                  //!< Local user index.
	size_t m_numSuggestedNames;     //!< Do not modify. May be 0.
	char** m_ppSuggestedNames;      //!< Do not modify. Will be NULL if m_numSuggestedNames is 0.
};

struct SCryLobbyUserProfileChanged
{
	uint32 m_user;
};

struct SCryLobbyChatRestrictedData
{
	uint32 m_user;
	bool   m_chatRestricted;
};

struct SCryLobbyForcedFromRoomData
{
	CrySessionHandle      m_session;
	EForcedFromRoomReason m_why;
};

struct SCryLobbySessionQueueData
{
	int m_placeInQueue;
};

struct SCryLobbyDedicatedServerSetupData
{
	CCryLobbyPacket* pPacket;
	CrySessionHandle session;
};

struct SCryLobbyDedicatedServerReleaseData
{
	CrySessionHandle session;
};

struct SCryLobbyFriendIDData
{
	CryUserID m_user;
};

struct SCryLobbyFriendOnlineStateData : public SCryLobbyFriendIDData
{
	EOnlineState m_curState; //!< Indicates the new state of the user identified by m_user.
};

#define LOBBY_MESSAGE_SIZE (256)
struct SCryLobbyFriendMessageData : public SCryLobbyFriendIDData
{
	char m_message[LOBBY_MESSAGE_SIZE];
};

enum ELobbyFriendStatus
{
	eLFS_Offline,
	eLFS_Online,
	eLFS_OnlineSameTitle,
	eLFS_Pending
};

typedef uint32 CryLobbyUserIndex;
const CryLobbyUserIndex CryLobbyInvalidUserIndex = 0xffffffff;

struct SCryLobbyPartyMember
{
	CryUserID          m_userID;
	char               m_name[CRYLOBBY_USER_NAME_LENGTH];
	ELobbyFriendStatus m_status;
	CryLobbyUserIndex  m_userIndex;
};

struct SCryLobbyPartyMembers
{
	uint32                m_numMembers;
	SCryLobbyPartyMember* m_pMembers;
};
