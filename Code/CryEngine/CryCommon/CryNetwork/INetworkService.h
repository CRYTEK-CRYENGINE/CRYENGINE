// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryNetwork/ISerialize.h> // <> required for Interfuscator
#include <CryNetwork/CrySocks.h>

struct IServerBrowser;
struct IServerListener;
struct INetworkChat;
struct IChatListener;
struct ITestInterface;
struct IServerReport;
struct IServerReportListener;
struct IStatsTrack;
struct IStatsTrackListener;
struct INatNeg;
struct INatNegListener;
struct IFileDownloader;
struct IHTTPGateway;
struct SFileDownloadParameters;
struct IPatchCheck;
struct INetworkProfile;
struct INetworkProfileListener;
struct INetNub;
struct INetProfileTokens;

struct IContextViewExtension;
class CContextView;

struct IContextViewExtensionAdder
{
	virtual ~IContextViewExtensionAdder(){}
	virtual void AddExtension(IContextViewExtension*) = 0;
};

enum ENetworkServiceState
{

	eNSS_Initializing,  //!< Service is opened, but has not finished initializing.
	eNSS_Ok,            //!< Service is initialized okay.
	eNSS_Failed,        //!< Service has failed initializing in some way.
	eNSS_Closed         //!< Service has been closed.
};

struct INetworkService : public CMultiThreadRefCount
{
	// <interfuscator:shuffle>
	virtual ENetworkServiceState GetState() = 0;
	virtual void                 Close() = 0;
	virtual void                 CreateContextViewExtensions(bool server, IContextViewExtensionAdder* adder) = 0;
	virtual IServerBrowser*      GetServerBrowser() = 0;
	virtual INetworkChat*        GetNetworkChat() = 0;
	virtual IServerReport*       GetServerReport() = 0;
	virtual IStatsTrack*         GetStatsTrack(int version) = 0;
	virtual INatNeg*             GetNatNeg(INatNegListener* const) = 0;
	virtual INetworkProfile*     GetNetworkProfile() = 0;
	virtual ITestInterface*      GetTestInterface() = 0;
	virtual IFileDownloader*     GetFileDownloader() = 0;
	virtual void                 GetMemoryStatistics(ICrySizer* pSizer) = 0;
	virtual IPatchCheck*         GetPatchCheck() = 0;
	virtual IHTTPGateway*        GetHTTPGateway() = 0;
	virtual INetProfileTokens*   GetNetProfileTokens() = 0;
	//etc
	virtual ~INetworkService(){}
	// </interfuscator:shuffle>
};

struct INetProfileTokens
{
	// <interfuscator:shuffle>
	virtual ~INetProfileTokens(){}
	virtual void AddToken(uint32 profile, uint32 token) = 0;
	virtual bool IsValid(uint32 profile, uint32 token) = 0;
	virtual void Init() = 0;
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
	// </interfuscator:shuffle>
};

struct INetworkInterface
{
	// <interfuscator:shuffle>
	virtual ~INetworkInterface(){}
	virtual bool IsAvailable() const = 0;
	// </interfuscator:shuffle>
};

struct IGameQueryListener;

struct ITestInterface
{
	// <interfuscator:shuffle>
	virtual ~ITestInterface(){}
	virtual void TestBrowser(IGameQueryListener* GQListener) {}
	virtual void TestReport()                                {}
	virtual void TestChat()                                  {}
	virtual void TestStats()                                 {}
	// </interfuscator:shuffle>
};

struct SBasicServerInfo
{
	int         m_numPlayers;
	int         m_maxPlayers;
	bool        m_anticheat;
	bool        m_official;
	bool        m_private;
	bool        m_dx11;
	bool        m_voicecomm;
	bool        m_friendlyfire;
	bool        m_dedicated;
	bool        m_gamepadsonly;
	uint16      m_hostPort;
	uint16      m_publicPort;
	uint32      m_publicIP;
	uint32      m_privateIP;
	const char* m_hostName;
	const char* m_mapName;
	const char* m_gameVersion;
	const char* m_gameType;
	const char* m_country;
};

enum EServerBrowserError
{
	eSBE_General,
	eSBE_ConnectionFailed,
	eSBE_DuplicateUpdate,
};

struct IServerListener
{
	// <interfuscator:shuffle>
	virtual ~IServerListener(){}
	//! New server reported during update and basic values received.
	virtual void NewServer(const int id, const SBasicServerInfo*) = 0;
	virtual void UpdateServer(const int id, const SBasicServerInfo*) = 0;

	//! Remove server from UI server list.
	virtual void RemoveServer(const int id) = 0;

	//! Server successfully pinged (servers behind NAT cannot be pinged).
	virtual void UpdatePing(const int id, const int ping) = 0;

	//! Extended data - can be dependent on game type etc, retrieved only by request via IServerBrowser::UpdateServerInfo.
	virtual void UpdateValue(const int id, const char* name, const char* value) = 0;
	virtual void UpdatePlayerValue(const int id, const int playerNum, const char* name, const char* value) = 0;
	virtual void UpdateTeamValue(const int id, const int teamNum, const char* name, const char* value) = 0;

	//! Called on any error.
	virtual void OnError(const EServerBrowserError) = 0;

	//! All servers in the list processed.
	virtual void UpdateComplete(bool cancelled) = 0;

	//! Cannot update/ping server.
	virtual void ServerUpdateFailed(const int id) = 0;
	virtual void ServerUpdateComplete(const int id) = 0;

	virtual void ServerDirectConnect(bool needsnat, uint32 ip, uint16 port) = 0;
	// </interfuscator:shuffle>
};

struct IServerBrowser : public INetworkInterface
{
	// <interfuscator:shuffle>
	virtual ~IServerBrowser(){}
	virtual void Start(bool browseLAN) = 0;
	virtual void SetListener(IServerListener* lst) = 0;
	virtual void Stop() = 0;
	virtual void Update() = 0;
	virtual void UpdateServerInfo(int id) = 0;
	virtual void BrowseForServer(uint32 ip, uint16 port) = 0;
	virtual void BrowseForServer(const char* addr, uint16 port) = 0;
	virtual void SendNatCookie(uint32 ip, uint16 port, int cookie) = 0;
	virtual void CheckDirectConnect(int id, uint16 port) = 0;
	virtual int  GetServerCount() = 0;
	virtual int  GetPendingQueryCount() = 0;
	// </interfuscator:shuffle>
};

enum EChatJoinResult
{
	//! Join successful.
	eCJR_Success,

	// Error codes.
	eCJR_ChannelError,
	eCJR_Banned,
};

enum EChatUserStatus
{
	eCUS_inchannel,
	eCUS_joined,
	eCUS_left
};

enum ENetworkChatMessageType
{
	eNCMT_server,
	eNCMT_say,
	eNCMT_data
};

struct IChatListener
{
	// <interfuscator:shuffle>
	virtual ~IChatListener(){}
	virtual void Joined(EChatJoinResult) = 0;
	virtual void Message(const char* from, const char* message, ENetworkChatMessageType type) = 0;
	virtual void ChatUser(const char* nick, EChatUserStatus st) = 0;
	virtual void OnError(int err) = 0;
	virtual void OnChatKeys(const char* user, int num, const char** keys, const char** values) = 0;
	virtual void OnGetKeysFailed(const char* user) = 0;
	// </interfuscator:shuffle>
};

struct INetworkChat : public INetworkInterface
{
	// <interfuscator:shuffle>
	virtual void Join() = 0;
	virtual void Leave() = 0;
	virtual void SetListener(IChatListener* lst) = 0;
	virtual void Say(const char* message) = 0;
	virtual void SendData(const char* nick, const char* message) = 0;
	virtual void SetChatKeys(int num, const char** keys, const char** value) = 0;
	virtual void GetChatKeys(const char* user, int num, const char** keys) = 0;
	// </interfuscator:shuffle>
};

enum EServerReportError
{
	eSRE_noerror, //!< Everything is okay.
	eSRE_socket,  //!< Socket problem: creation, bind, etc.
	eSRE_connect, //!< Connection problem, DNS, server unreachable, NAT etc.
	eSRE_noreply, //!< No reply from the other end.
	eSRE_other,   //!< Something happened.
};

struct IServerReportListener
{
	// <interfuscator:shuffle>
	virtual ~IServerReportListener(){}
	virtual void OnError(EServerReportError) = 0;
	virtual void OnPublicIP(uint32, unsigned short) = 0;
	// </interfuscator:shuffle>
};

struct  IServerReport : public INetworkInterface
{
	// <interfuscator:shuffle>
	virtual void AuthPlayer(int playerid, uint32 ip, const char* challenge, const char* responce) = 0;
	virtual void ReAuthPlayer(int playerid, const char* responce) = 0;

	virtual void SetReportParams(int numplayers, int numteams) = 0;

	virtual void SetServerValue(const char* key, const char*) = 0;
	virtual void SetPlayerValue(int, const char* key, const char*) = 0;
	virtual void SetTeamValue(int, const char* key, const char*) = 0;

	virtual void Update() = 0;

	//! Start reporting.
	virtual void StartReporting(INetNub*, IServerReportListener*) = 0;

	//! Stop reporting and clear listener.
	virtual void StopReporting() = 0;

	virtual void ProcessQuery(char* data, int len, CRYSOCKADDR_IN* addr) = 0;
	// </interfuscator:shuffle>
};

enum EStatsTrackError
{
	eSTE_noerror, //!< Everything is okay.
	eSTE_socket,  //!< Socket problem: creation, bind, etc.
	eSTE_connect, //!< Connection problem, DNS, server unreachable, NAT etc.
	eSTE_noreply, //!< No reply from the other end.
	eSTE_other,   //!< Something happened.
};

struct IStatsTrackListener
{
	// <interfuscator:shuffle>
	virtual ~IStatsTrackListener(){}
	virtual void OnError(EStatsTrackError) = 0;
	// </interfuscator:shuffle>
};

//! Player/Team id semantic differ from ServerReport, as ServerReport is stateless, and StatsTrack is not
struct  IStatsTrack : public INetworkInterface
{
	// <interfuscator:shuffle>
	virtual void SetListener(IStatsTrackListener*) = 0;
	//! These will return id that should be used in all other calls.
	virtual int  AddPlayer(int id) = 0;   //!< Player.
	virtual int  AddTeam(int id) = 0;     //!< Team.

	virtual void PlayerDisconnected(int id) = 0;   //!< User left the game.
	virtual void PlayerConnected(int id) = 0;      //!< User returned to game.

	virtual void StartGame() = 0;   //!< Clear data.
	virtual void EndGame() = 0;     //!< Send data off.
	virtual void Reset() = 0;       //!< Reset game - don't send anything.

	virtual void SetServerValue(int key, const char* value) = 0;
	virtual void SetPlayerValue(int, int key, const char* value) = 0;
	virtual void SetTeamValue(int, int key, const char* value) = 0;

	virtual void SetServerValue(int key, int value) = 0;
	virtual void SetPlayerValue(int, int key, int value) = 0;
	virtual void SetTeamValue(int, int key, int value) = 0;
	// </interfuscator:shuffle>
};

struct INatNegListener
{
	// <interfuscator:shuffle>
	virtual ~INatNegListener(){}

	//! indicates NAT is possible on this site.
	virtual void OnDetected(bool success, bool compatible) = 0;

	virtual void OnCompleted(int cookie, bool success, CRYSOCKADDR_IN* addr) = 0;
	// </interfuscator:shuffle>
};

struct INatNeg : public INetworkInterface
{
	// <interfuscator:shuffle>
	virtual void StartNegotiation(int cookie, bool server, int socket) = 0;
	virtual void CancelNegotiation(int cookie) = 0;
	virtual void OnPacket(char* data, int len, CRYSOCKADDR_IN* fromaddr) = 0;
	// </interfuscator:shuffle>
};

struct IDownloadStream
{
	// <interfuscator:shuffle>
	virtual ~IDownloadStream(){}
	virtual void GotData(const uint8* pData, uint32 length) = 0;
	virtual void Complete(bool success) = 0;
	// </interfuscator:shuffle>
};

struct SFileDownloadParameters
{
	virtual ~SFileDownloadParameters(){}
	SFileDownloadParameters()
	{
		sourceFilename.clear();
		destFilename.clear();
		destPath.clear();
		for (int i = 0; i < 16; ++i)
			md5[i] = 0;
		fileSize = 0;
		pStream = 0;
	}

	string           sourceFilename; //!< e.g. "http://< www.server.com/file.ext".
	string           destFilename;   //!< e.g. "file.ext".
	string           destPath;       //!< e.g. "Game\Levels\Multiplayer\IA\LevelName\".
	unsigned char    md5[16];        //!< MD5 checksum of the file.
	int              fileSize;

	IDownloadStream* pStream;  //!< Replaces destFilename if set - direct to memory downloads.

	virtual void     SerializeWith(TSerialize ser)
	{
		ser.Value("SourceFilename", sourceFilename);
		ser.Value("DestFilename", destFilename);
		ser.Value("DestPath", destPath);
		ser.Value("FileSize", fileSize);

		for (int i = 0; i < 16; ++i)
			ser.Value("MD5Checksum", md5[i]);
	}
};

struct IFileDownload
{
	// <interfuscator:shuffle>
	virtual ~IFileDownload(){}
	virtual bool  Finished() const = 0;
	virtual float GetProgress() const = 0;
	// </interfuscator:shuffle>
};

struct IFileDownloader : public INetworkInterface
{
	// <interfuscator:shuffle>
	//! Start downloading from http server and save the file locally.
	virtual void DownloadFile(SFileDownloadParameters& dl) = 0;

	//! Set throttling parameters (so as not to slow down games in progress).
	//! Probably want to turn off throttling if we're at the end of level, etc.
	//! Set both parameters to zero to disable throttling.
	//! \param datasize How much data to request.
	//! \param timedelay How often to request it (in ms).
	virtual void SetThrottleParameters(int datasize, int timedelay) = 0;

	//! Is a download in progress?
	virtual bool IsDownloading() const = 0;

	//! Get progress of current download (0.0 - 1.0).
	virtual float GetDownloadProgress() const = 0;

	//! Get md5 checksum of downloaded file (only valid when IsDownloading()==false).
	virtual const unsigned char* GetFileMD5() const = 0;

	//! Stop downloads (also called when no more files to download).
	virtual void Stop() = 0;
	// </interfuscator:shuffle>
};

struct IPatchCheck : public INetworkInterface
{
	// <interfuscator:shuffle>
	//! Fire off a query to a server somewhere to figure out if there's an update available for download.
	virtual bool CheckForUpdate() = 0;

	//! Is the query still pending?
	virtual bool IsUpdateCheckPending() const = 0;

	//! Is there an update?
	virtual bool IsUpdateAvailable() const = 0;

	//! Is it a required update?
	virtual bool IsUpdateMandatory() const = 0;

	//! Get the URL of the patch file.
	virtual const char* GetPatchURL() const = 0;

	//! Get display name of new version.
	virtual const char* GetPatchName() const = 0;

	//! Trigger install of patch on exit game.
	virtual void SetInstallOnExit(bool install) = 0;
	virtual bool GetInstallOnExit() const = 0;

	//! Store and retrieve the place the patch was downloaded to.
	virtual void        SetPatchFileName(const char* filename) = 0;
	virtual const char* GetPatchFileName() const = 0;

	virtual void        TrackUsage() = 0;
	// </interfuscator:shuffle>
};

struct IHTTPGateway
{
	// <interfuscator:shuffle>
	virtual ~IHTTPGateway(){}
	virtual int  GetURL(const char* url, IDownloadStream* stream) = 0;
	virtual int  PostURL(const char* url, const char* params, IDownloadStream* stream) = 0;
	virtual int  PostFileToURL(const char* url, const char* params, const char* name, const uint8* data, uint32 size, const char* mime, IDownloadStream* stream) = 0;
	virtual void CancelRequest(int) = 0;
	// </interfuscator:shuffle>
};

const int MAX_PROFILE_STRING_LEN = 31;

enum EUserStatus
{
	eUS_offline = 0,
	eUS_online,
	eUS_playing,
	eUS_staging,
	eUS_chatting,
	eUS_away
};

struct SNetworkProfileUserStatus
{
	char        m_locationString[MAX_PROFILE_STRING_LEN + 1];
	char        m_statusString[MAX_PROFILE_STRING_LEN + 1];
	EUserStatus m_status;
};

enum ENetworkProfileError
{
	eNPE_ok,

	eNPE_connectFailed,         //!< Cannot connect to server.
	eNPE_disconnected,          //!< Disconnected from GP.

	eNPE_loginFailed,           //!< Check your account/password.
	eNPE_loginTimeout,          //!< Timeout.
	eNPE_anotherLogin,          //!< Another login.

	eNPE_actionFailed,          //!< Generic action failed.

	eNPE_nickTaken,             //!< Nick already take.
	eNPE_registerAccountError,  //!< Mail/pass not match.
	eNPE_registerGeneric,       //!< Register failed.

	eNPE_nickTooLong,         //!< No longer than 20 chars.
	eNPE_nickFirstNumber,     //!< No first digits.
	eNPE_nickSlash,           //!< No slash in nicks.
	eNPE_nickFirstSpecial,    //!< No first specials @+#:.
	eNPE_nickNoSpaces,        //!< No spaces in nick.
	eNPE_nickEmpty,           //!< Empty nicks.

	eNPE_profileEmpty,          //!< Empty profile name.

	eNPE_passTooLong,           //!< No longer than 30 chars.
	eNPE_passEmpty,             //!< Empty password.

	eNPE_mailTooLong,           //!< No longer than 50 chars.
	eNPE_mailEmpty,             //!< No longer than 50 chars.
};

struct INetworkProfileListener
{
	// <interfuscator:shuffle>
	virtual ~INetworkProfileListener(){}
	virtual void AddNick(const char* nick) = 0;
	virtual void UpdateFriend(int id, const char* nick, EUserStatus, const char* status, bool foreign) = 0;
	virtual void RemoveFriend(int id) = 0;
	virtual void OnFriendRequest(int id, const char* message) = 0;
	virtual void OnMessage(int id, const char* message) = 0;
	virtual void LoginResult(ENetworkProfileError res, const char* descr, int id, const char* nick) = 0;
	virtual void OnError(ENetworkProfileError res, const char* descr) = 0;
	virtual void OnProfileInfo(int id, const char* key, const char* value) = 0;

	//! Finished updating profile info.
	virtual void OnProfileComplete(int id) = 0;

	virtual void OnSearchResult(int id, const char* nick) = 0;
	virtual void OnSearchComplete() = 0;
	virtual void OnUserId(const char* nick, int id) = 0;
	virtual void OnUserNick(int id, const char* nick, bool foreign_name) = 0;
	virtual void RetrievePasswordResult(bool ok) = 0;
	// </interfuscator:shuffle>
};

struct IStatsAccessor
{
	// <interfuscator:shuffle>
	virtual ~IStatsAccessor(){}
	//input
	virtual const char* GetTableName() = 0;
	virtual void        End(bool success) = 0;
	// </interfuscator:shuffle>
};

struct IStatsReader : public IStatsAccessor
{
	// <interfuscator:shuffle>
	virtual int         GetFieldsNum() = 0;
	virtual const char* GetFieldName(int i) = 0;
	//output
	virtual void        NextRecord(int id) = 0;
	virtual void        OnValue(int field, const char* val) = 0;
	virtual void        OnValue(int field, int val) = 0;
	virtual void        OnValue(int field, float val) = 0;
	// </interfuscator:shuffle>
};

struct IStatsWriter : public IStatsAccessor
{
	// <interfuscator:shuffle>
	virtual int         GetFieldsNum() = 0;
	virtual const char* GetFieldName(int i) = 0;
	//input
	virtual int         GetRecordsNum() = 0;
	virtual int         NextRecord() = 0;
	virtual bool        GetValue(int field, const char*& val) = 0;
	virtual bool        GetValue(int field, int& val) = 0;
	virtual bool        GetValue(int field, float& val) = 0;
	//output
	virtual void        OnResult(int idx, int id, bool success) = 0;
	// </interfuscator:shuffle>
};

struct IStatsDeleter : public IStatsAccessor
{
	// <interfuscator:shuffle>
	virtual int  GetRecordsNum() = 0;
	virtual int  NextRecord() = 0;
	virtual void OnResult(int idx, int id, bool success) = 0;
	virtual void End(bool success) = 0;
	// </interfuscator:shuffle>
};

struct SRegisterDayOfBirth
{
	SRegisterDayOfBirth(){}
	SRegisterDayOfBirth(uint8 d, uint8 m, uint16 y) : day(d), month(m), year(y){}

	//hm...
	SRegisterDayOfBirth(uint32 i) : day(i & 0xFF), month((i >> 8) & 0xFF), year(i >> 16){}
	operator uint32() const { return (uint32(year) << 16) + (uint32(month) << 8) + day; }

	uint8  day;
	uint8  month;
	uint16 year;
};

struct INetworkProfile : public INetworkInterface
{
	// <interfuscator:shuffle>
	virtual void AddListener(INetworkProfileListener*) = 0;
	virtual void RemoveListener(INetworkProfileListener*) = 0;

	virtual void Register(const char* nick, const char* email, const char* password, const char* country, SRegisterDayOfBirth dob) = 0;
	virtual void Login(const char* nick, const char* password) = 0;
	virtual void LoginProfile(const char* email, const char* password, const char* profile) = 0;
	virtual void Logoff() = 0;
	virtual void SetStatus(EUserStatus status, const char* location) = 0;
	virtual void EnumUserNicks(const char* email, const char* password) = 0;
	virtual void AuthFriend(int id, bool auth) = 0;
	virtual void RemoveFriend(int id, bool ignore) = 0;
	virtual void AddFriend(int id, const char* reason) = 0;
	virtual void SendFriendMessage(int id, const char* message) = 0;
	virtual void GetProfileInfo(int id) = 0;
	virtual bool IsLoggedIn() = 0;
	virtual void UpdateBuddies() = 0;
	virtual void SearchFriends(const char* request) = 0;
	virtual void GetUserId(const char* nick) = 0;
	virtual void GetUserNick(int id) = 0;

	//! Player Stats.
	virtual void ReadStats(IStatsReader* reader) = 0;
	virtual void WriteStats(IStatsWriter* writer) = 0;
	virtual void DeleteStats(IStatsDeleter* deleter) = 0;

	//! Other Player's Stats.
	virtual void ReadStats(int id, IStatsReader* reader) = 0;
	virtual void RetrievePassword(const char* email) = 0;
	// </interfuscator:shuffle>
};

//! \endcond