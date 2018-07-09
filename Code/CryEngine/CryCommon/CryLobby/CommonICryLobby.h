// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CrySystem/ICryPlugin.h>

typedef uint32 CryLobbyTaskID;
const CryLobbyTaskID CryLobbyInvalidTaskID = 0xffffffff;

enum ECryLobbyService
{
	eCLS_LAN,
	eCLS_Online,
	eCLS_NumServices
};

enum ECryLobbyError
{
	eCLE_Success                    = 0,  //!< Task is successfully started if returned from function and successfully finished if callback is called with this error.
	eCLE_SuccessContinue            = 1,  //!< For tasks that return multiple results the callback will be called with this error if there is a valid result and the callback will be called again.
	eCLE_ServiceNotSupported        = 2,  //!< The service is not supported on this platform.
	eCLE_AlreadyInitialised         = 3,  //!< Service has already been initialised.
	eCLE_NotInitialised             = 4,  //!< Service has not been initialised.
	eCLE_TooManyTasks               = 5,  //!< The task could not be started because too many tasks are already running.
	eCLE_OutOfMemory                = 6,  //!< Not enough memory to complete task.
	eCLE_OutOfSessionUserData       = 7,  //!< Trying to register too much session user data.
	eCLE_UserDataNotRegistered      = 8,  //!< Using a session user data id that has not been registered.
	eCLE_UserDataTypeMissMatch      = 9,  //!< Live - The data type of the session user data is not compatible with the data type defined in the xlast program.
	eCLE_TooManySessions            = 10, //!< The session could not be created because there are too many session already created.
	eCLE_InvalidSession             = 11, //!< The specified session handle does not exist.
	eCLE_InvalidRequest             = 12, //!< The task being performed is invalid.
	eCLE_SPAFileOutOfDate           = 13, //!< Live - The SPA file used doesn't match the one on the live servers.
	eCLE_ConnectionFailed           = 14, //!< Connection to session host failed.
	eCLE_SessionFull                = 15, //!< Can't join session because it is full.
	eCLE_SessionWrongState          = 16, //!< The session is in the wrong state for the requested operation to be performed.
	eCLE_UserNotSignedIn            = 17, //!< The user specified is not signed in.
	eCLE_InvalidParam               = 18, //!< An invalid parameter was passed to function.
	eCLE_TimeOut                    = 19, //!< The current task has timed out waiting for a response0.
	eCLE_InsufficientPrivileges     = 20, //!< User had insufficient privileges to perform the requested task. In Live a silver account is being used when a gold account is required.
	eCLE_AlreadyInSession           = 21, //!< Trying to join a session that has already been joined.
	eCLE_LeaderBoardNotRegistered   = 22, //!< Using a leaderboard id that has not been registered.
	eCLE_UserNotInSession           = 23, //!< Trying to write to a leaderboard for a user who is not in the session.
	eCLE_OutOfUserData              = 24, //!< Trying to register too much user data.
	eCLE_NoUserDataRegistered       = 25, //!< Trying to read/write user data when no data has been registered.
	eCLE_ReadDataNotWritten         = 26, //!< Trying to read user data that has never been written.
	eCLE_UserDataMissMatch          = 27, //!< Trying to write user data that is different to registered data.
	eCLE_InvalidUser                = 28, //!< Trying to use an invalid user id.
	eCLE_PSNContextError            = 29, //!< Somethings wrong with one of the various PSN contexts.
	eCLE_PSNWrongSupportState       = 30, //!< Invalid state in PSN support state machine.
	eCLE_SuccessUnreachable         = 31, //!< For session search the callback will be called with this error if a session was found but is unreachable and the callback will be called again with other results.
	eCLE_ServerNotDefined           = 32, //!< Server not set.
	eCLE_WorldNotDefined            = 33, //!< World not set.
	eCLE_SystemIsBusy               = 34, //!< System is busy. XMB is doing something else and cannot work on the new task. trying to bring up more than 1 UI dialogs at a time, or look at friends list while it is being downloaded in the background, etc.
	eCLE_TooManyParameters          = 35, //!< Too many parameters were passed to the task. e.g an array of SCryLobbyUserData has more items than the task expected.
	eCLE_NotEnoughParameters        = 36, //!< Not enough parameters were passed to the task. e.g an array of SCryLobbyUserData has less items than the task expected.
	eCLE_DuplicateParameters        = 37, //!< Duplicate parameters were passed to the task. e.g an array of SCryLobbyUserData has the same item more than once.
	eCLE_ExceededReadWriteLimits    = 38, //!< This error can be returned if the underlying SDK has limits on how often the task can be run.
	eCLE_InvalidTitleID             = 39, //!< Title is using an invalid title id.
	eCLE_IllegalSessionJoin         = 40, //!< An illegal session join has been performed. e.g in Live a session has been created or joined that has invites enabled when the user is already in a session with invites enabled.
	eCLE_InternetDisabled           = 41, //!< PSN: No access to Internet.
	eCLE_NoOnlineAccount            = 42, //!< PSN: No online account.
	eCLE_NotConnected               = 43, //!< PSN: Not connected.
	eCLE_CyclingForInvite           = 44, //!< PSN: Currently handling a invite.
	eCLE_CableNotConnected          = 45, //!< Ethernet cable is not connected.
	eCLE_SessionNotMigratable       = 46, //!< An attempt to migrate a non-migratable session was detected.
	eCLE_SuccessInvalidSession      = 47, //!< If SessionEnd or SessionDelete is called for an invalid session, return this as a successful failure.
	eCLE_RoomDoesNotExist           = 48, //!< A PSN request was triggered for a room that doesn't exist on the PSN service.
	eCLE_PSNUnavailable             = 49, //!< PSN: service is currently unavailable (might be under maintenance, etc. Not the same as unreachable!).
	eCLE_TooManyOrders              = 50, //!< Too many DLC store orders.
	eCLE_InvalidOrder               = 51, //!< Order does not exist.
	eCLE_OrderInUse                 = 52, //!< Order already has an active task.
	eCLE_OnlineAccountBlocked       = 53, //!< PSN: Account is suspended or banned.
	eCLE_AgeRestricted              = 54, //!< PSN: Online access restricted because of age (child accounts only).
	eCLE_ReadDataCorrupt            = 55, //!< UserData is corrupted/wrong size/invalid. (Not the same as eCLE_ReadDataNotWritten, which means not present).
	eCLE_PasswordIncorrect          = 56, //!< client passed the wrong password to the server in the session join request.
	eCLE_InvalidInviteFriendData    = 57, //!< PSN: Invite friend data is invalid/unavailable.
	eCLE_InvalidJoinFriendData      = 58, //!< PSN: Join friend data is invalid/unavailable.
	eCLE_InvalidPing                = 60, //!< No valid ping value found for user in session.
	eCLE_CDKeyMalformed             = 61, //!< Malformed CD key.
	eCLE_CDKeyUnknown               = 62, //!< CD key not in database for this game.
	eCLE_CDKeyAuthFailed            = 63, //!< CD key authentication failed.
	eCLE_CDKeyDisabled              = 64, //!< CD key has been disabled.
	eCLE_CDKeyInUse                 = 65, //!< CD key is in use.
	eCLE_MultipleSignIn             = 66, //!< Disconnected because another sign in to the same account occurred.
	eCLE_Banned                     = 67, //!< Banned user attempting to join session.
	eCLE_CDKeyTimeOut               = 68, //!< CD key validation time out.
	eCLE_IncompleteLoginCredentials = 69, //!< Incomplete login credentials.
	eCLE_WrongVersion               = 70, //!< Trying to join a game or squad that is using a different version.
	eCLE_NoServerAvailable          = 71, //!< Unable to retrieve a dedicated server from the dedicated server arbitrator.
	eCLE_ArbitratorTimeOut          = 72, //!< Timed out trying to access the dedicated server arbitrator.
	eCLE_RequiresInvite             = 73, //!< Session requires an invite to be able to join.
	eCLE_SteamInitFailed            = 74, //!< Steam: unable to init main Steam library.
	eCLE_SteamBlocked               = 75, //!< Steam: user is blocked from performing this action (e.g. blocked from joining a session).
	eCLE_NothingToEnumerate         = 76, //!< Failed to create an xbox enumerator as there is nothing to enumerate.
	eCLE_ServiceNotConnected        = 77, //!< Platform online service is not connected.
	eCLE_GlobalBan                  = 78, //!< Banned from playing online.
	eCLE_Kicked                     = 79, //!< Kicked from server.

	eCLE_Cancelled                  = 79,
	eCLE_UnhandledNickError         = 80,

	eCLE_InternalError,

	eCLE_NumErrors
};

struct ICryMatchMaking;
struct ICryVoice;
struct ICryStats;
struct ICryLobbyUI;
struct ICryFriends;
struct ICryFriendsManagement;
struct ICryReward;
struct ICryOnlineStorage;
struct IHostMigrationEventListener;
struct ICryLobbyService;
struct ICryMatchMakingPrivate;
struct ICryMatchMakingConsoleCommands;

#if USE_STEAM
	#define USE_LOBBYIDADDR 1
#else
	#define USE_LOBBYIDADDR 0
#endif

#define XBOX_RELEASE_USE_SECURE_SOCKETS 1

#if CRY_PLATFORM_ORBIS
	#define USE_PSN 0
#else
	#define USE_PSN 0
#endif

#include "CryLobbyEventCallback.h"    // Separated out for readability

struct SConfigurationParams;
typedef void (* CryLobbyConfigurationCallback)(ECryLobbyService service, SConfigurationParams* requestedParams, uint32 paramCount);
typedef void (* CryLobbyCallback)(ECryLobbyService service, ECryLobbyError error, void* arg);

//! Flags for use with ICryLobby::GetLobbyServiceFlag.
enum ECryLobbyServiceFlag
{
	eCLSF_AllowMultipleSessions = BIT(0), //!< Does the lobby service allow multiple sessions to be joined? (Default True).
	eCLSF_SupportHostMigration  = BIT(1)  //!< Does the lobby service support host migration? (Default: True).
};

//! Allows users of the lobby to not initialise features that they don't require.
enum ECryLobbyServiceFeatures
{
	eCLSO_Base          = BIT(0),
	eCLSO_Matchmaking   = BIT(1),
	eCLSO_Voice         = BIT(2),
	eCLSO_Stats         = BIT(3),
	eCLSO_LobbyUI       = BIT(4),
	eCLSO_Friends       = BIT(5),
	eCLSO_Reward        = BIT(6),
	eCLSO_TCPService    = BIT(7),
	eCLSO_OnlineStorage = BIT(8),

	eCLSO_All           = eCLSO_Base | eCLSO_Matchmaking | eCLSO_Voice | eCLSO_Stats | eCLSO_LobbyUI | eCLSO_Friends | eCLSO_Reward | eCLSO_TCPService | eCLSO_OnlineStorage
};

//! Intended to make future extension easier. At present I only need the port exposing.
struct SCryLobbyParameters
{
	uint16 m_listenPort;        //!< Listen port the lobby service will use for connections.
	uint16 m_connectPort;       //!< Connect port the lobby service will use for connections.
};

struct ICryLobby : public Cry::IEnginePlugin
{
public:
	CRYINTERFACE_DECLARE_GUID(ICryLobby, "{3ED8EF88-5332-4BDF-A5CB-5A3AD5016279"_cry_guid);

	// <interfuscator:shuffle>
	virtual ~ICryLobby() {}

	//! Initialise a lobby service.
	//! \param service   The service to be initialised.
	//! \param features  Which features of the service to initialise (e.g.  ECryLobbyServiceFeatures(eCLSO_All & (~eCLSO_Reward))).
	//! \param cfgCb     Callback function that is called whenever the lobby system needs access to data that is unavailable through the standard API.
	//! \param cb        Callback function that is called when function completes.
	//! \param cbArg     Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError Initialise(ECryLobbyService service, ECryLobbyServiceFeatures features, CryLobbyConfigurationCallback cfgCb, CryLobbyCallback cb, void* cbArg) = 0;

	//! Shut down a lobby service.
	//! \param Service		The service to be shut down.
	//! \param Features		Which features of the service to terminate (e.g.  ECryLobbyServiceFeatures(eCLSO_All & (~eCLSO_Reward))).
	//! \param Cb					Callback function that is called when function completes.
	//! \param CbArg			Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError Terminate(ECryLobbyService service, ECryLobbyServiceFeatures features, CryLobbyCallback cb, void* cbArg) = 0;

	virtual void Tick(bool flush) = 0;

	//! Should be called at least once a frame, causes the lobby->game message queue to be acted on.
	//! \return eCLE_Success if function successfully completed, or an error code if function failed for some reason.
	virtual ECryLobbyError ProcessEvents() = 0;

	//! Set the current lobby service.
	//! \param Service The service to use.
	//! \return The service that was in use before the call.
	virtual ECryLobbyService SetLobbyService(ECryLobbyService service) = 0;

	//! Get the current lobby service type.
	//! \return The ECryLobbyService of the current lobby service.
	virtual ECryLobbyService GetLobbyServiceType() = 0;

	//! Get the current lobby service.
	//! \return Pointer to an ICryLobbyService for the current lobby service.
	virtual ICryLobbyService* GetLobbyService() = 0;

	//! Get the current matchmaking service.
	//! \return Pointer to an ICryMatchMaking associated with the current lobby service.
	virtual ICryMatchMaking* GetMatchMaking() = 0;

	//! Get the CryNetwork-specific interface of MatchMaking.
	virtual ICryMatchMakingPrivate* GetMatchMakingPrivate() = 0;

	//! Get a base MatchMaking interface for console-issued matchmaking commands.
	virtual ICryMatchMakingConsoleCommands* GetMatchMakingConsoleCommands() = 0;

	//! Get the current voice service.
	//! \return Pointer to an ICryVoice associated with the current lobby service.
	virtual ICryVoice* GetVoice() = 0;

	//! Get the current stats service.
	//! \return Pointer to an ICryStats associated with the current lobby service.
	virtual ICryStats* GetStats() = 0;

	//! Get the current lobby ui service.
	//! \return Pointer to an ICryLobbyUI associated with the current lobby service.
	virtual ICryLobbyUI* GetLobbyUI() = 0;

	//! Get the current friends service.
	//! \return Pointer to an ICryFriends associated with the current lobby service.
	virtual ICryFriends* GetFriends() = 0;

	//! Get the current friends management service.
	//! \return Pointer to an ICryFriendsManagement associated with the current lobby service.
	virtual ICryFriendsManagement* GetFriendsManagement() = 0;

	//! Get the current reward service (achievements/trophies).
	//! \return Pointer to an ICryReward associated with the current lobby service.
	virtual ICryReward* GetReward() = 0;

	//! Get the current online storage service (title/user storage on PSN and LIVE).
	//! \return Pointer to an ICryOnlineStorage associated with the current lobby service.
	virtual ICryOnlineStorage* GetOnlineStorage() = 0;

	//! Get the requested lobby service.
	//! \param Service		The service to wanted.
	//! \return	Pointer to an ICryLobbyService for the lobby service requested.
	virtual ICryLobbyService* GetLobbyService(ECryLobbyService service) = 0;

	//! Register interest in particular lobby events (such as invite accepted), the passed in callback will be fired when the event happens.
	//! \param Cb					Callback function that will be called.
	//! \param Arg				Pointer to application-specified data.
	virtual void RegisterEventInterest(ECryLobbySystemEvent eventOfInterest, CryLobbyEventCallback cb, void* pUserArg) = 0;

	//! Unregister interest in particular lobby events (such as invite accepted), the passed in callback will no longer be fired when the event happens.
	//! \param Cb					Callback function that will no longer be called.
	//! \param Arg				Pointer to application-specified data that was passed in when the callback was registered.
	virtual void UnregisterEventInterest(ECryLobbySystemEvent eventOfInterest, CryLobbyEventCallback cb, void* pUserArg) = 0;

	//! Retrieve various information about the current lobby service.
	//! \return SCryLobbyParameters - reference to a structure that contains some essential information about the lobby.
	virtual const SCryLobbyParameters& GetLobbyParameters() const = 0;

	//! Is the given flag true for the lobby service of the given type?
	//! \param service Lobby service to be queried
	//! \param flag Flag to be queried
	//! \return true if the flag is true.
	virtual bool GetLobbyServiceFlag(ECryLobbyService service, ECryLobbyServiceFlag flag) = 0;

	//! Set the user packet end point.
	//! \param End	- The user packet end point. Packet types end and higher get translated to disconnect packets.
	virtual void SetUserPacketEnd(uint32 end) = 0;

	// The following three functions are used to allow clients to start loading assets earlier in the flow when using the lobby service.
	// At present we don't support more than one nub session, so these are in the lobby for easier access.

	//! An optimisation for client load times, allows the engine to load the assets ahead of being informed by the server.
	virtual void SetCryEngineLoadHint(const char* levelName, const char* gameRules) = 0;

	//! \return Level name previously set by SetCryEngineLoadHint or "" if never set.
	virtual const char* GetCryEngineLevelNameHint() = 0;

	//! \return Game rules name previously set by SetCryEngineLoadHint or "" if never set.
	virtual const char* GetCryEngineRulesNameHint() = 0;

	/////////////////////////////////////////////////////////////////////////////
	// Host Migration
	virtual void AddHostMigrationEventListener(IHostMigrationEventListener* pListener, const char* pWho, uint32 priority) = 0;
	virtual void RemoveHostMigrationEventListener(IHostMigrationEventListener* pListener) = 0;
	/////////////////////////////////////////////////////////////////////////////
	// </interfuscator:shuffle>
};
