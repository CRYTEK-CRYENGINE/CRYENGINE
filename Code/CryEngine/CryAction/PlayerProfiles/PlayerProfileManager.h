// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERPROFILEMANAGER_H__
#define __PLAYERPROFILEMANAGER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <IPlayerProfiles.h>
#include <CryCore/CryCrc32.h>

class CPlayerProfile;
struct ISaveGame;
struct ILoadGame;

#if defined(_DEBUG)
	#define PROFILE_DEBUG_COMMANDS 1
#else
	#define PROFILE_DEBUG_COMMANDS 0
#endif

class CPlayerProfileManager : public IPlayerProfileManager
{
public:
	struct IPlatformImpl;

	// profile description
	struct SLocalProfileInfo
	{
		SLocalProfileInfo()
			: m_lastLoginTime(0)
		{
			SetName(PLAYER_DEFAULT_PROFILE_NAME);
		}
		SLocalProfileInfo(const string& name)
			: m_lastLoginTime(0)
		{
			SetName(name);
		}
		SLocalProfileInfo(const char* name)
			: m_lastLoginTime(0)
		{
			SetName(name);
		}

		void SetName(const char* name)
		{
			m_name = name;
		}

		void SetName(const string& name)
		{
			m_name = name;
		}
		void SetLastLoginTime(time_t lastLoginTime)
		{
			m_lastLoginTime = lastLoginTime;
		}

		const string& GetName() const
		{
			return m_name;
		}

		const time_t& GetLastLoginTime() const
		{
			return m_lastLoginTime;
		}

		int compare(const char* name) const
		{
			return m_name.compareNoCase(name);
		}

	private:
		string m_name; // name of the profile
		time_t m_lastLoginTime;
	};

	// per user data
	struct SUserEntry
	{
		SUserEntry(const string& inUserId) : userId(inUserId), pCurrentProfile(0), pCurrentPreviewProfile(0), userIndex(0), unknownUser(false) {}
		string                         userId;
		CPlayerProfile*                pCurrentProfile;
		CPlayerProfile*                pCurrentPreviewProfile;
		unsigned int                   userIndex;
		bool                           unknownUser;
		std::vector<SLocalProfileInfo> profileDesc;
	};
	typedef std::vector<SUserEntry*> TUserVec;

	struct SSaveGameMetaData
	{
		SSaveGameMetaData()
		{
			levelName = gameRules = buildVersion = SAVEGAME_LEVEL_NAME_UNDEFINED;
			saveTime = 0;
			loadTime = 0;
			fileVersion = -1;
		}
		void CopyTo(ISaveGameEnumerator::SGameMetaData& data)
		{
			data.levelName = levelName;
			data.gameRules = gameRules;
			data.fileVersion = fileVersion;
			data.buildVersion = buildVersion;
			data.saveTime = saveTime;
			data.loadTime = loadTime;
			data.xmlMetaDataNode = xmlMetaDataNode;
		}
		string     levelName;
		string     gameRules;
		int        fileVersion;
		string     buildVersion;
		time_t     saveTime; // original time of save
		time_t     loadTime; // most recent time loaded, from file modified timestamp
		XmlNodeRef xmlMetaDataNode;
	};

	struct SThumbnail
	{
		SThumbnail() : width(0), height(0), depth(0) {}
		DynArray<uint8> data;
		int             width;
		int             height;
		int             depth;
		bool IsValid() const { return data.size() && width && height && depth; }
		void ReleaseData()
		{
			data.clear();
			width = height = depth = 0;
		}
	};

	struct SSaveGameInfo
	{
		string            name;
		string            humanName;
		string            description;
		SSaveGameMetaData metaData;
		SThumbnail        thumbnail;
		void              CopyTo(ISaveGameEnumerator::SGameDescription& desc)
		{
			desc.name = name;
			desc.humanName = humanName;
			desc.description = description;
			metaData.CopyTo(desc.metaData);
		}
	};
	typedef std::vector<SSaveGameInfo> TSaveGameInfoVec;

	enum EPlayerProfileSection
	{
		ePPS_Attribute = 0,
		ePPS_Actionmap,
		ePPS_Num  // Last
	};

	// members

	CPlayerProfileManager(CPlayerProfileManager::IPlatformImpl* pImpl); // CPlayerProfileManager takes ownership of IPlatformImpl*
	virtual ~CPlayerProfileManager();

	// IPlayerProfileManager
	virtual bool                  Initialize();
	virtual bool                  Shutdown();
	virtual int                   GetUserCount();
	virtual bool                  GetUserInfo(int index, IPlayerProfileManager::SUserInfo& outInfo);
	virtual const char*           GetCurrentUser();
	virtual int                   GetCurrentUserIndex();
	virtual void                  SetExclusiveControllerDeviceIndex(unsigned int exclusiveDeviceIndex);
	virtual unsigned int          GetExclusiveControllerDeviceIndex() const;
	virtual bool                  LoginUser(const char* userId, bool& bOutFirstTime);
	virtual bool                  LogoutUser(const char* userId);
	virtual int                   GetProfileCount(const char* userId);
	virtual bool                  GetProfileInfo(const char* userId, int index, IPlayerProfileManager::SProfileDescription& outInfo);
	virtual bool                  CreateProfile(const char* userId, const char* profileName, bool bOverrideIfPresent, IPlayerProfileManager::EProfileOperationResult& result);
	virtual bool                  DeleteProfile(const char* userId, const char* profileName, IPlayerProfileManager::EProfileOperationResult& result);
	virtual bool                  RenameProfile(const char* userId, const char* newName, IPlayerProfileManager::EProfileOperationResult& result);
	virtual bool                  SaveProfile(const char* userId, IPlayerProfileManager::EProfileOperationResult& result, unsigned int reason);
	virtual bool                  SaveInactiveProfile(const char* userId, const char* profileName, EProfileOperationResult& result, unsigned int reason);
	virtual bool                  IsLoadingProfile() const;
	virtual bool                  IsSavingProfile() const;
	virtual IPlayerProfile*       ActivateProfile(const char* userId, const char* profileName);
	virtual IPlayerProfile*       GetCurrentProfile(const char* userId);
	virtual bool                  ResetProfile(const char* userId);
	virtual void                  ReloadProfile(IPlayerProfile* pProfile, unsigned int reason);
	virtual IPlayerProfile*       GetDefaultProfile();
	virtual const IPlayerProfile* PreviewProfile(const char* userId, const char* profileName);
	virtual void                  SetSharedSaveGameFolder(const char* sharedSaveGameFolder);
	virtual const char*           GetSharedSaveGameFolder()
	{
		return m_sharedSaveGameFolder.c_str();
	}
	virtual void   GetMemoryStatistics(ICrySizer* s);

	virtual void   AddListener(IPlayerProfileListener* pListener, bool updateNow);
	virtual void   RemoveListener(IPlayerProfileListener* pListener);
	virtual void   AddOnlineAttributesListener(IOnlineAttributesListener* pListener);
	virtual void   RemoveOnlineAttributesListener(IOnlineAttributesListener* pListener);
	virtual void   EnableOnlineAttributes(bool enable);
	virtual bool   HasEnabledOnlineAttributes() const;
	virtual bool   CanProcessOnlineAttributes() const;
	virtual void   SetCanProcessOnlineAttributes(bool enable);
	virtual bool   RegisterOnlineAttributes();
	virtual void   GetOnlineAttributesState(const IOnlineAttributesListener::EEvent event, IOnlineAttributesListener::EState& state) const;
	virtual bool   IsOnlineOnlyAttribute(const char* name);
	virtual void   LoadOnlineAttributes(IPlayerProfile* pProfile);
	virtual void   SaveOnlineAttributes(IPlayerProfile* pProfile);
	virtual int    GetOnlineAttributesVersion() const;
	virtual int    GetOnlineAttributeIndexByName(const char* name);
	virtual void   GetOnlineAttributesDataFormat(SCryLobbyUserData* pData, uint32 numData);
	virtual uint32 GetOnlineAttributeCount();
	virtual void   ClearOnlineAttributes();
	virtual void   SetProfileLastLoginTime(const char* userId, int index, time_t lastLoginTime);
	// ~IPlayerProfileManager

	void LoadGamerProfileDefaults(IPlayerProfile* pProfile);

	// maybe move to IPlayerProfileManager i/f
	virtual ISaveGameEnumeratorPtr CreateSaveGameEnumerator(const char* userId, CPlayerProfile* pProfile);
	virtual ISaveGame*             CreateSaveGame(const char* userId, CPlayerProfile* pProfile);
	virtual ILoadGame*             CreateLoadGame(const char* userId, CPlayerProfile* pProfile);
	virtual bool                   DeleteSaveGame(const char* userId, CPlayerProfile* pProfile, const char* name);

	virtual ILevelRotationFile*    GetLevelRotationFile(const char* userId, CPlayerProfile* pProfile, const char* name);

	virtual bool                   ResolveAttributeBlock(const char* userId, const char* attrBlockName, IResolveAttributeBlockListener* pListener, int reason);
	virtual bool                   ResolveAttributeBlock(const char* userId, const SResolveAttributeRequest& attrBlockNameRequest, IResolveAttributeBlockListener* pListener, int reason);

	const string&                  GetSharedSaveGameFolder() const
	{
		return m_sharedSaveGameFolder;
	}

	bool IsSaveGameFolderShared() const
	{
#if CRY_PLATFORM_DESKTOP
		return m_sharedSaveGameFolder.empty() == false;
#else
		// TCR/TRC - don't use profile name in file names.
		// Also, console saves are already keyed to a user profile.
		return false;
#endif
	}

	ILINE CPlayerProfile* GetDefaultCPlayerProfile() const
	{
		return m_pDefaultProfile;
	}

	// helper to move file or directory -> should eventually go into CrySystem/CryPak
	// only implemented for Windows
	bool           MoveFileHelper(const char* existingFileName, const char* newFileName);

	virtual void   SetOnlineAttributes(IPlayerProfile* pProfile, const SCryLobbyUserData* pData, const int32 onlineDataCount);
	virtual uint32 GetOnlineAttributes(SCryLobbyUserData* pData, uint32 numData);
	virtual uint32 GetDefaultOnlineAttributes(SCryLobbyUserData* pData, uint32 numData);

	void           ApplyChecksums(SCryLobbyUserData* pData, uint32 numData);
	bool           ValidChecksums(const SCryLobbyUserData* pData, uint32 numData);

public:
	struct IProfileXMLSerializer
	{
		virtual ~IProfileXMLSerializer(){}
		virtual bool       IsLoading() = 0;

		virtual void       SetSection(CPlayerProfileManager::EPlayerProfileSection section, XmlNodeRef& node) = 0;
		virtual XmlNodeRef CreateNewSection(CPlayerProfileManager::EPlayerProfileSection section, const char* name) = 0;
		virtual XmlNodeRef GetSection(CPlayerProfileManager::EPlayerProfileSection section) = 0;

		/*
		   virtual XmlNodeRef AddSection(const char* name) = 0;
		   virtual XmlNodeRef GetSection(const char* name) = 0;
		 */
	};

	struct IPlatformImpl
	{
		typedef CPlayerProfileManager::SUserEntry               SUserEntry;
		typedef CPlayerProfileManager::SLocalProfileInfo        SLocalProfileInfo;
		typedef CPlayerProfileManager::SThumbnail               SThumbnail;
		typedef CPlayerProfileManager::SResolveAttributeRequest SResolveAttributeRequest;
		virtual ~IPlatformImpl(){}
		virtual bool                Initialize(CPlayerProfileManager* pMgr) = 0;
		virtual void                Release() = 0; // must delete itself
		virtual bool                LoginUser(SUserEntry* pEntry) = 0;
		virtual bool                LogoutUser(SUserEntry* pEntry) = 0;
		virtual bool                SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name, bool initialSave = false, int reason = ePR_All) = 0;
		virtual bool                LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name) = 0;
		virtual bool                DeleteProfile(SUserEntry* pEntry, const char* name) = 0;
		virtual bool                RenameProfile(SUserEntry* pEntry, const char* newName) = 0;
		virtual bool                GetSaveGames(SUserEntry* pEntry, TSaveGameInfoVec& outVec, const char* altProfileName = "") = 0; // if altProfileName == "", uses current profile of SUserEntry
		virtual ISaveGame*          CreateSaveGame(SUserEntry* pEntry) = 0;
		virtual ILoadGame*          CreateLoadGame(SUserEntry* pEntry) = 0;
		virtual bool                DeleteSaveGame(SUserEntry* pEntry, const char* name) = 0;
		virtual ILevelRotationFile* GetLevelRotationFile(SUserEntry* pEntry, const char* name) = 0;
		virtual bool                GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail) = 0;
		virtual void                GetMemoryStatistics(ICrySizer* s) = 0;
		virtual bool                ResolveAttributeBlock(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* attrBlockName, IResolveAttributeBlockListener* pListener, int reason)                            { return false; }
		virtual bool                ResolveAttributeBlock(SUserEntry* pEntry, CPlayerProfile* pProfile, const SResolveAttributeRequest& attrBlockNameRequest, IResolveAttributeBlockListener* pListener, int reason) { return false; }

	};

	SUserEntry* FindEntry(const char* userId) const;

protected:

	SUserEntry*        FindEntry(const char* userId, TUserVec::iterator& outIter);

	SLocalProfileInfo* GetLocalProfileInfo(SUserEntry* pEntry, const char* profileName) const;
	SLocalProfileInfo* GetLocalProfileInfo(SUserEntry* pEntry, const char* profileName, std::vector<SLocalProfileInfo>::iterator& outIter) const;

public:
	static int         sUseRichSaveGames;
	static int         sRSFDebugWrite;
	static int         sRSFDebugWriteOnLoad;
	static int         sLoadOnlineAttributes;

	static const char* FACTORY_DEFAULT_NAME;
	static const char* PLAYER_DEFAULT_PROFILE_NAME;

protected:
	bool        SetUserData(SCryLobbyUserData* data, const TFlowInputData& value);
	bool        ReadUserData(const SCryLobbyUserData* data, TFlowInputData& val);
	uint32      UserDataSize(const SCryLobbyUserData* data);
	bool        SetUserDataType(SCryLobbyUserData* data, const char* type);
	void        GetDefaultValue(const char* type, XmlNodeRef attributeNode, SCryLobbyUserData* pOutData);

	bool        RegisterOnlineAttribute(const char* name, const char* type, const bool onlineOnly, const SCryLobbyUserData& defaultValue, CCrc32& crc);
	void        ReadOnlineAttributes(IPlayerProfile* pProfile, const SCryLobbyUserData* pData, const uint32 numData);
	static void ReadUserDataCallback(CryLobbyTaskID taskID, ECryLobbyError error, SCryLobbyUserData* pData, uint32 numData, void* pArg);
	static void RegisterUserDataCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);
	static void WriteUserDataCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

	void        SetOnlineAttributesState(const IOnlineAttributesListener::EEvent event, const IOnlineAttributesListener::EState newState);
	void        SendOnlineAttributeState(const IOnlineAttributesListener::EEvent event, const IOnlineAttributesListener::EState newState);

	//Online attributes use checksums to check the validity of data loaded
	int  Checksum(const int checksum, const SCryLobbyUserData* pData, uint32 numData);
	int  ChecksumHash(const int hash, const int value) const;
	int  ChecksumHash(const int value0, const int value1, const int value2, const int value3, const int value4) const;
	int  ChecksumConvertValueToInt(const SCryLobbyUserData* pData);

	bool LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name, bool bPreview = false);

	const static int k_maxOnlineDataCount = 1500;
	const static int k_maxOnlineDataBytes = 2976; //reduced from 3000 for encryption
	const static int k_onlineChecksums = 2;

#if PROFILE_DEBUG_COMMANDS
	static void DbgLoadOnlineAttributes(IConsoleCmdArgs* args);
	static void DbgSaveOnlineAttributes(IConsoleCmdArgs* args);
	static void DbgTestOnlineAttributes(IConsoleCmdArgs* args);

	static void DbgLoadProfile(IConsoleCmdArgs* args);
	static void DbgSaveProfile(IConsoleCmdArgs* args);

	SCryLobbyUserData m_testData[k_maxOnlineDataCount];
	int               m_testFlowData[k_maxOnlineDataCount];
	int               m_testingPhase;
#endif

	typedef VectorMap<string, uint32>            TOnlineAttributeMap;
	typedef std::vector<IPlayerProfileListener*> TListeners;

	SCryLobbyUserData                 m_defaultOnlineData[k_maxOnlineDataCount];
	SCryLobbyUserData                 m_onlineData[k_maxOnlineDataCount];
	bool                              m_onlineOnlyData[k_maxOnlineDataCount];
	TOnlineAttributeMap               m_onlineAttributeMap;
	TListeners                        m_listeners;
	TUserVec                          m_userVec;
	string                            m_curUserID;
	string                            m_sharedSaveGameFolder;
	int                               m_curUserIndex;
	unsigned int                      m_exclusiveControllerDeviceIndex;
	uint32                            m_onlineDataCount;
	uint32                            m_onlineDataByteCount;
	uint32                            m_onlineAttributeAutoGeneratedVersion;
	IPlatformImpl*                    m_pImpl;
	CPlayerProfile*                   m_pDefaultProfile;
	IPlayerProfile*                   m_pReadingProfile;
	IOnlineAttributesListener::EState m_onlineAttributesState[IOnlineAttributesListener::eOAE_Max];
	IOnlineAttributesListener*        m_onlineAttributesListener;
	int                               m_onlineAttributeDefinedVersion;
	CryLobbyTaskID                    m_lobbyTaskId;
	bool                              m_registered;
	bool                              m_bInitialized;
	bool                              m_enableOnlineAttributes;
	bool                              m_allowedToProcessOnlineAttributes;
	bool                              m_loadingProfile;
	bool                              m_savingProfile;
};

#endif
