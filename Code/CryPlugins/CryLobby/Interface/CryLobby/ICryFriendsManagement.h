// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryLobby/ICryFriends.h> // <> required for Interfuscator

//! Used to return IsFriend/IsUserBlocked results.
struct SFriendManagementInfo
{
	CryUserID userID;
	bool      result;
};

//! Used to pass in user to search for.
struct SFriendManagementSearchInfo
{
	char name[CRYLOBBY_USER_NAME_LENGTH];
};

#define CRYLOBBY_FRIEND_STATUS_STRING_SIZE   256
#define CRYLOBBY_FRIEND_LOCATION_STRING_SIZE 256

enum EFriendManagementStatus
{
	eFMS_Unknown,
	eFMS_Offline,
	eFMS_Online,
	eFMS_Playing,
	eFMS_Staging,
	eFMS_Chatting,
	eFMS_Away
};

struct SFriendStatusInfo
{
	CryUserID               userID;
	EFriendManagementStatus status;
	char                    statusString[CRYLOBBY_FRIEND_STATUS_STRING_SIZE];
	char                    locationString[CRYLOBBY_FRIEND_LOCATION_STRING_SIZE];
};

//! \param taskID   Task ID allocated when the function was executed.
//! \param error    Error code eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pArg     Pointer to application-specified data.
typedef void (* CryFriendsManagementCallback)(CryLobbyTaskID taskID, ECryLobbyError error, void* pCbArg);

//! \param taskID    Task ID allocated when the function was executed.
//! \param error     Error code eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pArg      Pointer to application-specified data.
typedef void (* CryFriendsManagementInfoCallback)(CryLobbyTaskID taskID, ECryLobbyError error, SFriendManagementInfo* pInfo, uint32 numUserIDs, void* pCbArg);

//! \param taskID    Task ID allocated when the function was executed.
//! \param error     Error code   eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pArg      Pointer to application-specified data.
typedef void (* CryFriendsManagementSearchCallback)(CryLobbyTaskID taskID, ECryLobbyError error, SFriendInfo* pInfo, uint32 numUserIDs, void* pCbArg);

//! \param taskID      Task ID allocated when the function was executed.
//! \param error       Error code   eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pInfo       Array of returned friend status info.
//! \param numUserIDs  Number of elements in array.
//! \param pArg        Pointer to application-specified data.
typedef void (* CryFriendsManagementStatusCallback)(CryLobbyTaskID taskID, ECryLobbyError error, SFriendStatusInfo* pInfo, uint32 numUserIDs, void* pCbArg);

struct ICryFriendsManagement
{
	// <interfuscator:shuffle>
	virtual ~ICryFriendsManagement() {}

	//! Sends a friend request to the specified list of users.
	//! \param User         The pad number of the user sending the friend requests.
	//! \param pUserIDs     Pointer to an array of user ids to send friend requests to.
	//! \param numUserIDs   The number of users to send friend requests to.
	//! \param pTaskID      Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb          Callback function that is called when function completes.
	//! \param pCbArg       Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementSendFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Accepts friend requests from the specified list of users.
	//! \param User         The pad number of the user accepting the friend requests.
	//! \param pUserIDs     Pointer to an array of user ids to accept friend requests from.
	//! \param numUserIDs   The number of users to accept friend requests from.
	//! \param pTaskID      Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb          Callback function that is called when function completes.
	//! \param pCbArg       Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementAcceptFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Rejects friend requests from the specified list of users.
	//! \param User         The pad number of the user rejecting the friend requests.
	//! \param pUserIDs     Pointer to an array of user ids to reject friend requests from.
	//! \param numUserIDs   The number of users to reject friend requests from.
	//! \param pTaskID      Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb          Callback function that is called when function completes.
	//! \param pCbArg       Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementRejectFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Revokes friendship status from the specified list of users.
	//! \param User         The pad number of the user revoking friend status.
	//! \param pUserIDs     Pointer to an array of user ids to revoke friend status from.
	//! \param numUserIDs   The number of users to revoke friend status from.
	//! \param pTaskID      Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb          Callback function that is called when function completes.
	//! \param pCbArg       Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementRevokeFriendStatus(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Determines friendship status of the specified list of users.
	//! \param User          The pad number of the user determining friend status.
	//! \param pUserIDs      Pointer to an array of user ids to determine friend status for.
	//! \param numUserIDs    The number of users to determine friend status for.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementIsUserFriend(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementInfoCallback pCb, void* pCbArg) = 0;

	//! Attempt to find the users specified.
	//! \param User          The pad number of the user attempting to find other users.
	//! \param pUserIDs      Pointer to an array of user ids to attempt to find.
	//! \param numUserIDs    The number of users to attempt to find.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementFindUser(uint32 user, SFriendManagementSearchInfo* pUserName, uint32 maxResults, CryLobbyTaskID* pTaskID, CryFriendsManagementSearchCallback pCb, void* pCbArg) = 0;

	//! Add the specified users to the blocked list.
	//! \param User          The pad number of the user adding users to their block list.
	//! \param pUserIDs      Pointer to an array of user ids to block.
	//! \param numUserIDs    The number of users to block.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementBlockUser(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Remove the specified users from the blocked list.
	//! \param User          The pad number of the user removing users from their block list.
	//! \param pUserIDs      Pointer to an array of user ids to unblock.
	//! \param numUserIDs    The number of users to unblock.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementUnblockUser(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Determines if the specified users are in the blocked list.
	//! \param User          The pad number of the user determining blocked status.
	//! \param pUserIDs      Pointer to an array of user ids to determine blocked status for.
	//! \param numUserIDs    The number of users to determine blocked status for.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementIsUserBlocked(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementInfoCallback pCb, void* pCbArg) = 0;

	//! Accepts the invitation from the specified user.
	//! \param User          The pad number of the user accepting the invite.
	//! \param PUserID       Pointer to the user id to accept the invite from.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementAcceptInvite(uint32 user, CryUserID* pUserID, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg) = 0;

	//! Gets the names of the specified list of user IDs.
	//! \param User          The pad number of the user requesting names.
	//! \param pUserIDs      Pointer to an array of user ids to get names for.
	//! \param numUserIDs    The number of users to get names for.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementGetName(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementSearchCallback pCb, void* pCbArg) = 0;

	//! Gets the status of the specified list of user IDs.
	//! \param User          The pad number of the user requesting names.
	//! \param pUserIDs      Pointer to an array of user ids to get names for.
	//! \param numUserIDs    The number of users to get names for.
	//! \param pTaskID       Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb           Callback function that is called when function completes.
	//! \param pCbArg        Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError FriendsManagementGetStatus(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementStatusCallback pCb, void* pCbArg) = 0;

	//! Cancels a task.
	virtual void CancelTask(CryLobbyTaskID) = 0;
	// </interfuscator:shuffle>
};
