// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "CommonICryMatchMaking.h"

class CCryLobbyPacket;

enum ECryLobbySystemEvent
{
	eCLSE_EthernetState,                //!< returns CryLobbyEthernetStateData	(gives status of ethernet cable e.g. unplugged)
	eCLSE_OnlineState,                  //!< returns CryLobbyOnlineStateData		(gives status of online connection e.g. signed in)
	eCLSE_InviteAccepted,               //!< returns CryLobbyInviteAcceptedData
	eCLSE_UserInviteAccepted,           //!< returns CryLobbyUserInviteAcceptedData
	eCLSE_NatType,                      //!< returns CryLobbyNatTypeData
	eCLSE_UserPacket,                   //!< returns CryLobbyUserPacketData
	eCLSE_RoomOwnerChanged,             //!< returns CryLobbyRoomOwnerChanged
	eCLSE_SessionUserJoin,              //!< returns SCryLobbySessionUserData
	eCLSE_SessionUserLeave,             //!< returns SCryLobbySessionUserData
	eCLSE_SessionUserUpdate,            //!< returns SCryLobbySessionUserData
	eCLSE_PartyMembers,                 //!< returns SCryLobbyPartyMembers. Called whenever the Live party list changes.
	eCLSE_UserProfileChanged,           //!< returns SCryLobbyUserProfileChanged. Called whenever the user changes a profile setting using the platform UI.
	eCLSE_OnlineNameRejected,           //!< returns SCryLobbyOnlineNameRejectedData.
	eCLSE_FriendOnlineState,            //!< returns SCryLobbyFriendOnlineStateData
	eCLSE_FriendRequest,                //!< returns SCryLobbyFriendIDData
	eCLSE_FriendMessage,                //!< returns SCryLobbyFriendMessageData
	eCLSE_FriendAuthorised,             //!< returns SCryLobbyFriendIDData
	eCLSE_FriendRevoked,                //!< returns SCryLobbyFriendIDData
	eCLSE_FriendsListUpdated,           //!< returns NULL
	eCLSE_ReceivedInvite,               //!< returns SCryLobbyFriendMessageData
	eCLSE_SessionClosed,                //!< returns SCryLobbySessionEventData
	eCLSE_ChatRestricted,               //!< returns SCryLobbyChatRestrictedData
	eCLSE_ForcedFromRoom,               //!< returns SCryLobbyForcedFromRoomData;
	eCLSE_LoginFailed,                  //!< returns SCryLobbyOnlineStateData; for services that don't use the eOS_SigningIn intermediate state
	eCLSE_SessionRequestInfo,           //!< returns SCryLobbySessionRequestInfo
	eCLSE_KickedFromSession,            //!< returns SCryLobbySessionEventData
	eCLSE_KickedHighPing,               //!< returns SCryLobbySessionEventData
	eCLSE_KickedReservedUser,           //!< returns SCryLobbySessionEventData
	eCLSE_KickedLocalBan,               //!< returns SCryLobbySessionEventData
	eCLSE_KickedGlobalBan,              //!< returns SCryLobbySessionEventData
	eCLSE_KickedGlobalBanStage1,        //!< returns SCryLobbySessionEventData
	eCLSE_KickedGlobalBanStage2,        //!< returns SCryLobbySessionEventData
	eCLSE_StartJoiningGameByPlaygroup,  //!< returns NULL
	eCLSE_JoiningGameByPlaygroupResult, //!< returns SCryLobbyJoinedGameByPlaygroupData
	eCLSE_LeavingGameByPlaygroup,       //!< returns SCryLobbySessionEventData
	eCLSE_PlaygroupUserJoin,            //!< returns SCryLobbyPlaygroupUserData
	eCLSE_PlaygroupUserLeave,           //!< returns SCryLobbyPlaygroupUserData
	eCLSE_PlaygroupUserUpdate,          //!< returns SCryLobbyPlaygroupUserData
	eCLSE_PlaygroupClosed,              //!< returns SCryLobbyPlaygroupEventData
	eCLSE_PlaygroupLeaderChanged,       //!< returns SCryLobbyPlaygroupUserData
	eCLSE_KickedFromPlaygroup,          //!< returns SCryLobbyPlaygroupEventData
	eCLSE_PlaygroupUpdated,             //!< returns SCryLobbyPlaygroupEventData
	eCLSE_DedicatedServerSetup,         //!< returns SCryLobbyDedicatedServerSetupData; Dedicated servers allocated via CCryDedicatedServerArbitrator will receive this event when they are setup by the host calling SessionSetupDedicatedServer.
	eCLSE_DedicatedServerRelease,       //!< returns NULL; Dedicated servers allocated via CCryDedicatedServerArbitrator will receive this event when they are released by the host calling SessionReleaseDedicatedServer or when the server losses all of its connections.
	eCLSE_SessionQueueUpdate,           //!< returns SCryLobbySessionQueueData
};

struct SCryLobbyEthernetStateData;
struct SCryLobbyOnlineStateData;
struct SCryLobbyInviteAcceptedData;
struct SCryLobbyUserInviteAcceptedData;
struct SCryLobbyNatTypeData;
struct SCryLobbyUserPacketData;
struct SCryLobbyRoomOwnerChanged;
struct SCryLobbySessionUserData;
struct SCryLobbyPlaygroupUserData;
struct SCryLobbyPartyMembers;
struct SCryLobbyUserProfileChanged;
struct SCryLobbyOnlineNameRejectedData;
struct SCryLobbyFriendIDData;
struct SCryLobbyFriendOnlineStateData;
struct SCryLobbyFriendMessageData;
struct SCryLobbySessionEventData;
struct SCryLobbyPlaygroupEventData;
struct SCryLobbyChatRestrictedData;
struct SCryLobbyForcedFromRoomData;
struct SCryLobbySessionRequestInfo;
struct SCryLobbySessionQueueData;
struct SCryLobbyDedicatedServerSetupData;
struct SCryLobbyDedicatedServerReleaseData;
struct SCryLobbyJoinedGameByPlaygroupData;


union UCryLobbyEventData
{
	SCryLobbyEthernetStateData* pEthernetStateData;
	SCryLobbyOnlineStateData* pOnlineStateData;
	SCryLobbyInviteAcceptedData* pInviteAcceptedData;
	SCryLobbyUserInviteAcceptedData* pUserInviteAcceptedData;
	SCryLobbyNatTypeData* pNatTypeData;
	SCryLobbyUserPacketData* pUserPacketData;
	SCryLobbyRoomOwnerChanged*  pRoomOwnerChanged;
	SCryLobbySessionUserData*   pSessionUserData;
	SCryLobbyPlaygroupUserData* pPlaygroupUserData;
	SCryLobbyPartyMembers* pPartyMembers;
	SCryLobbyUserProfileChanged* pUserProfileChanged;
	SCryLobbyOnlineNameRejectedData* pSuggestedNames;
	SCryLobbyFriendIDData* pFriendIDData;
	SCryLobbyFriendOnlineStateData* pFriendOnlineStateData;
	SCryLobbyFriendMessageData*  pFriendMesssageData;
	SCryLobbySessionEventData*   pSessionEventData;
	SCryLobbyPlaygroupEventData* pPlaygroupEventData;
	SCryLobbyChatRestrictedData* pChatRestrictedData;
	SCryLobbyForcedFromRoomData* pForcedFromRoomData;
	SCryLobbySessionRequestInfo* pSessionRequestInfo;
	SCryLobbySessionQueueData*   pSessionQueueData;
	SCryLobbyDedicatedServerSetupData* pDedicatedServerSetupData;
	SCryLobbyDedicatedServerReleaseData* pDedicatedServerReleaseData;
	SCryLobbyJoinedGameByPlaygroupData*  pJoinedByPlaygroupData;
};

typedef void (* CryLobbyEventCallback)(UCryLobbyEventData eventData, void* userParam);
