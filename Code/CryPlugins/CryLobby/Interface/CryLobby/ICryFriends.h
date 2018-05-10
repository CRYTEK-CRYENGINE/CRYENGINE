// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryLobby/ICryLobby.h> // <> required for Interfuscator

#define CRYLOBBY_PRESENCE_MAX_SIZE 256

struct SFriendInfo
{
	CryUserID          userID;
	char               name[CRYLOBBY_USER_NAME_LENGTH];
	char               presence[CRYLOBBY_PRESENCE_MAX_SIZE];
	ELobbyFriendStatus status;
};

//! \param TaskID   Task ID allocated when the function was executed.
//! \param Error    Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param PArg     Pointer to application-specified data.
typedef void (* CryFriendsCallback)(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

//! \param TaskID       Task ID allocated when the function was executed.
//! \param Error        Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param PFriendInfo  Pointer to an array of SFriendInfo containing info about the friends retrieved.
//! \param NumFriends   Number of items in the pFriendInfo array.
//! \param PArg         Pointer to application-specified data.
typedef void (* CryFriendsGetFriendsListCallback)(CryLobbyTaskID taskID, ECryLobbyError error, SFriendInfo* pFriendInfo, uint32 numFriends, void* pArg);

struct ICryFriends
{
	// <interfuscator:shuffle>
	virtual ~ICryFriends(){}

	//! Retrieves the Friends list of the specified user.
	//! \param User      The pad number of the user to retrieve the friends list for.
	//! \param Start     The start index to retrieve from. First friend is 0.
	//! \param Num       Maximum number of friends to retrieve.
	//! \param PTaskID   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param Cb        Callback function that is called when function completes.
	//! \param PCbArg    Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsGetFriendsList(uint32 user, uint32 start, uint32 num, CryLobbyTaskID* pTaskID, CryFriendsGetFriendsListCallback cb, void* pCbArg) = 0;

	//! Send game invites to the given list of users for the given session.
	//! \param User        The pad number of the user sending the game invites.
	//! \param H           The handle to the session the invites are for.
	//! \param PUserIDs    Pointer to an array of user ids to send invites to.
	//! \param NumUserIDs  The number of users to invite.
	//! \param PTaskID     Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param Cb          Callback function that is called when function completes.
	//! \param PCbArg      Pointer to application-specified data that is passed to the callback.
	//! \return	eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsSendGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsCallback cb, void* pCbArg) = 0;
	// </interfuscator:shuffle>
};
