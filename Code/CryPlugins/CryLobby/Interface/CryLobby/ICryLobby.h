// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CommonICryLobby.h>
#include <ICryStats.h>

#define CRYLOBBY_USER_PACKET_START 128
#define CRYLOBBY_USER_PACKET_MAX   255

typedef uint16 CryPing;
#define CRYLOBBY_INVALID_PING  (CryPing(~0))

#define CRYSESSIONID_STRINGLEN 48

//! When a call to set Rich Presence is made, the SCryLobbyUserData is just passed back to the game code, so.
//! It's the game code's responsibility to correctly turn all the bits and bobs of SCryLobbyUserData into a single UTF-8 string.
//! [in] = maximum size of the buffer pre-allocated [SCE_NP_BASIC_PRESENCE_EXTENDED_STATUS_SIZE_MAX].
//! [out] = modify the value to contain the actual length of the string in bytes.
struct SCryLobbyPresenceConverter
{
	SCryLobbyUserData* m_pData;                //!< Pointer of the SCryLobbyUserData passed into CryLobbyUI::SetRichPresence. [in].
	uint32             m_numData;              //!< Number of SCryLobbyUserData in array pointed to by m_pData. [in].
	uint8*             m_pStringBuffer;        //!< Buffer to write the result string in to. This buffer is pre-allocated internally, no need to create your own. [in/out].
	uint32             m_sizeOfStringBuffer;   //!< Size of the string buffer in bytes, not characters. [in/out].
	CrySessionID       m_sessionId;            //!< Joinable session ID, to allow friends to join your game if desired. [out].
};

//! Used for creating a session and updating the session details.
//! \note In SDK 1.500 the linkdata (m_pData) could not be updated once created. Feature is due to be added in 1.700.
//! \note Updating localised session name/status will overwrite/replace all previous name/status values. Values not updated are discarded.
//! \note Is to NOT possible to update the number of slots, private or editable flags after creation.
struct SCryLobbySessionAdvertisement
{
	void*  m_pJPGImage;
	uint32 m_sizeofJPGImage;
	uint8* m_pData;
	uint32 m_sizeofData;
	char** m_pLocalisedLanguages;
	char** m_pLocalisedSessionNames;
	char** m_pLocalisedSessionStatus;
	uint32 m_numLanguages;
	uint32 m_numSlots;
	bool   m_bIsPrivate;
	bool   m_bIsEditableByAll;
};

//! A request to fill in an XMB string requires a buffer allocated by the calling function, and a value defining the size of the buffer.
//! It's the game's responsibility to fill the buffer with a UTF-8 string, and not to overflow the buffer.
struct SCryLobbyXMBString
{
	uint8* m_pStringBuffer;                           //! Pointer to a UTF-8 compatible string buffer.
	uint32 m_sizeOfStringBuffer;                      //!< Maximum sizeof the buffer.
};

//! A request to fill supply age restriction settings for different regions.
struct SCryLobbyAgeRestrictionSetup
{
	struct SCountry
	{
		char id[3];
		int8 age;
	};
	int32           m_numCountries;                   //!< Number of specified countries.
	const SCountry* m_pCountries;                     //!< Array of m_numCountries SCryLobbyAgeRestrictionSetup::SCountry entries.
};

// Privilege bits returned from ICryLobbyService::GetUserPrivileges in the CryLobbyPrivilegeCallback.
const uint32 CLPF_BlockMultiplayerSessons = BIT(0); //!< If set the user is not allowed to participate in multiplayer game sessions.
const uint32 CLPF_NotOnline = BIT(1);               //!< If set the user is not connected to PSN.

struct SCrySystemTime
{
	uint16 m_Year;
	uint16 m_Milliseconds;
	uint8  m_Month;
	uint8  m_DayOfWeek;
	uint8  m_Day;
	uint8  m_Hour;
	uint8  m_Minute;
	uint8  m_Second;
};

//! \param taskID    Task ID allocated when the function was executed.
//! \param error     Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param privilege A bitfield of CLPF_* specifying the users privileges. Bits get set to indicate the user should be blocked from an action if their privilege level isn't high enough.
//! \param pArg      Pointer to application-specified data.
typedef void(*CryLobbyPrivilegeCallback)(CryLobbyTaskID taskID, ECryLobbyError error, uint32 privilege, void* pArg);

//! \param taskID    Task ID allocated when the function was executed.
//! \param error     Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pArg      Pointer to application-specified data.
typedef void(*CryLobbyOnlineStateCallback)(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

//! \param taskID      Task ID allocated when the function was executed.
//! \param error       Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pString
//! \param isProfanity true if the string contained a profanity, otherwise false.
//! \param pArg        Pointer to application-specified data.
typedef void(*CryLobbyCheckProfanityCallback)(CryLobbyTaskID taskID, ECryLobbyError error, const char* pString, bool isProfanity, void* pArg);

#include "ICryTCPService.h"
#include "ICryLobbyEvent.h"

struct ICrySignIn;

struct ICryLobbyService
{
public:
	// <interfuscator:shuffle>
	virtual ~ICryLobbyService() {}
	virtual ICryMatchMaking*       GetMatchMaking() = 0;
	virtual ICryVoice*             GetVoice() = 0;
	virtual ICryStats*             GetStats() = 0;
	virtual ICryLobbyUI*           GetLobbyUI() = 0;
	virtual ICryFriends*           GetFriends() = 0;
	virtual ICryFriendsManagement* GetFriendsManagement() = 0;
	virtual ICryReward*            GetReward() = 0;
	virtual ICryOnlineStorage*     GetOnlineStorage() = 0;
	virtual ICrySignIn*            GetSignIn() = 0;

	//! Cancel the given task. The task will still be running in the background but the callback will not be called when it finishes.
	//! \param TaskID			- The task to cancel.
	virtual void CancelTask(CryLobbyTaskID lTaskID) = 0;

	//! Get an ICryTCPService.
	//! \param pService Service.
	//! \return Pointer to ICryTCPService for given server & port.
	virtual ICryTCPServicePtr GetTCPService(const char* pService) = 0;

	//! Get an ICryTCPService.
	//! \param pServer server
	//! \param port port
	//! \param pUrlPrefix URL prefix
	//! \return Pointer to ICryTCPService for given server & port.
	virtual ICryTCPServicePtr GetTCPService(const char* pServer, uint16 port, const char* pUrlPrefix) = 0;

	//! Get the user id of a user signed in to this service locally.
	//! \param user	The pad number of the local user.
	//! \return CryUserID of the user.
	virtual CryUserID GetUserID(uint32 user) = 0;

	//! Get the given users privileges.
	//! \param user      The pad number of the local user.
	//! \param pTaskID   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCB       Callback function that is called when function completes.
	//! \param pCbArg    Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError GetUserPrivileges(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyPrivilegeCallback pCB, void* pCBArg) = 0;

	//! Check a string for profanity.
	//! \param pString   String to check.
	//! \param pTaskID   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param pCb       Callback function that is called when function completes.
	//! \param pCbArg    Pointer to application-specified data that is passed to the callback.
	//! \return eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError CheckProfanity(const char* const pString, CryLobbyTaskID* pTaskID, CryLobbyCheckProfanityCallback pCb, void* pCbArg) = 0;

	//! Returns the current time from the online time server (must be signed in).
	//! \param pSystemTime Pointer to the CrySystemTime structure to contain the time. Only year, month, day, hours, minutes and seconds will be populated.
	//! \return eCLE_Success if the function was successful, or an error code if not.
	virtual ECryLobbyError GetSystemTime(uint32 user, SCrySystemTime* pSystemTime) = 0;

	// </interfuscator:shuffle>
};

// The callback below is fired whenever the lobby system needs access to data that is unavailable through the standard API.
//Its usually used to work around problems with specific platform lobbies requiring data not exposed through standard functionality.

//Basically whenever this callback is fired, you are required to fill in the requestedParams that are asked for.
//At present specifications are that the callback can fire multiple times.
//
// 'XSvc'		uint32	Service Id for XLSP servers
// 'XPor'		uint16	Port for XLSP server to communicate with Telemetry
// 'XSNm'		void*		ptr to a static const string containging the name of the required XLSP service. is used to filter the returned server list from Live. return NULL for no filtering
// 'LUnm'		void*		ptr to user name for local user - used by LAN (due to lack of guid) (is copied internally - DO NOT PLACE ON STACK)
//
// PS3 ONLY
// 'PCom'		void*		ptr to static SceNpCommunitcationId					(not copied - DO NOT PLACE ON STACK!)
// 'PPas'		void*		ptr to static SceNpCommunicationPassphrase	(not copied - DO NOT PLACE ON STACK!)
// 'PSig'		void*		ptr to static SceNpCommunicationSignature		(not copied - DO NOT PLACE ON STACK!)
// 'PInM'		char*		ptr to string used for the custom XMB button for sending invites to friends.
// 'PInS'		char*		ptr to string used for the XMB game invite message subject text.
// 'PInB'		char*		ptr to string used for the XMB game invite message body text.
// 'PFrS'		char*		ptr to string used for the XMB friend request message subject text.
// 'PFrB'		char*		ptr to string used for the XMB friend request message body text.
// 'PAge'		int32		Age limit of game (set to 0 for now, probably wants to come from some kind of configuration file)
// 'PSto'		char*		Store ID. AKA NP Commerce ServiceID and also top level category Id.
// 'PDlc'		int8		Input is DLC pack index to test is installed, output is boolean result.
//
// PS4 ONLY
// PTit and PSec are the PS4 replacements for the PS3 PCom, PPas and PSig.
// 'PTit'		void*		ptr to static SceNpTitleId									(not copied - DO NOT PLACE ON STACK!)
// 'PSec'		void*		ptr to static SceNpTitleSecret							(not copied - DO NOT PLACE ON STACK!)
// 'PSAd'		SCryLobbySessionAdvertisement*	in/out structure to request extra session advertising info from the game
// 'PAge'		SCryLobbyAgeRestrictionSetup*		Age limit settings per region
// 'PPlu'		int32		Age limit of game (set to 0 for now, probably wants to come from some kind of configuration file)
//
// 'CSgs'		uint32	ECryLobbyLoginGUIState constant to indicate whether other data requested is available
// 'CPre'		SCryLobbyPresenceConverter* in/out structure for converting a list of SCryLobbyUserData into a single string for presense
// 'Mspl'		Matchmaking session password length (not including null terminator)

#define CLCC_LAN_USER_NAME                           'LUnm'
#define CLCC_LIVE_TITLE_ID                           'XTtl'
#define CLCC_XLSP_SERVICE_ID                         'XSvc'
#define CLCC_LIVE_SERVICE_CONFIG_ID                  'XSCf'
#define CLCC_XLSP_SERVICE_PORT                       'XPor'
#define CLCC_XLSP_SERVICE_NAME                       'XSNm'
#define CLCC_PSN_COMMUNICATION_ID                    'PCom'
#define CLCC_PSN_COMMUNICATION_PASSPHRASE            'PPas'
#define CLCC_PSN_COMMUNICATION_SIGNATURE             'PSig'
#define CLCC_PSN_CUSTOM_MENU_GAME_INVITE_STRING      'PInM'
#define CLCC_PSN_CUSTOM_MENU_GAME_JOIN_STRING        'PJoM'
#define CLCC_PSN_INVITE_SUBJECT_STRING               'PInS'
#define CLCC_PSN_INVITE_BODY_STRING                  'PInB'
#define CLCC_PSN_FRIEND_REQUEST_SUBJECT_STRING       'PFrS'
#define CLCC_PSN_FRIEND_REQUEST_BODY_STRING          'PFrB'
#define CLCC_PSN_AGE_LIMIT                           'PAge'
#define CLCC_PSN_PLUS_TEST_REQUIRED                  'PPlu'
#define CLCC_PSN_STORE_ID                            'PSto'
#define CLCC_PSN_TICKETING_ID                        'PTik'
#define CLCC_PSN_IS_DLC_INSTALLED                    'PDlc'
#define CLCC_PSN_TITLE_ID                            'PTit'
#define CLCC_PSN_TITLE_SECRET                        'PSec'
#define CLCC_PSN_TICKETING_ID                        'PTik'
#define CLCC_PSN_CREATE_SESSION_ADVERTISEMENT        'PCSA'
#define CLCC_PSN_UPDATE_SESSION_ADVERTISEMENT        'PUSA'
#define CLCC_CRYLOBBY_EXPORTGAMESTATE                'CEgs'
#define CLCC_CRYLOBBY_IMPORTGAMESTATE                'CIgs'
#define CLCC_CRYLOBBY_LOBBYDATANAME                  'CLdn'
#define CLCC_CRYLOBBY_LOBBYDATAUSAGE                 'CLdu'
#define CLCC_CRYLOBBY_PRESENCE_CONVERTER             'CPre' // Used by PSN for converting presence info into a string form
#define CLCC_CRYLOBBY_QUERYTRANSLATEHASH             'CQth'
#define CLCC_CRYLOBBY_TRANSLATEHASH                  'CTrh'

#define CLCC_CRYSTATS_ENCRYPTION_KEY                 'CEnc' // Used for all platforms to encrypt UserData buffers

#define CLCC_MATCHMAKING_SESSION_PASSWORD_MAX_LENGTH 'Mspl' // Used to determine the maximum length of the session password (not including NULL character)

#define CLCC_STEAM_APPID                             'SaID' // Request for the Steam application id (only required during development)

struct SConfigurationParams
{
	uint32 m_fourCCID;
	union
	{
		uint64 m_64;
		void*  m_pData;
		uint32 m_32;
		uint16 m_16;
		uint8  m_8;
	};
};
