// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IPlatformOS.h
//  Created:     18/12/2009 by Alex Weighell.
//  Description: Interface to the Platform OS
// -------------------------------------------------------------------------
//  History:	19/05/2010 Rob Jessop: Added localisation query functions to get the SKU ID and platform supported language information
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IPLATFORMOS_H_
#define __IPLATFORMOS_H_

#include <CryString/CryFixedString.h>
#include <CrySystem/ILocalizationManager.h>
#include <CrySystem/ITimer.h>

#if CRY_PLATFORM_ORBIS
	#include <system_service.h>
#elif CRY_PLATFORM_WINDOWS
//#include <Lmcons.h> // this causes issues when including other windows headers later by defining PASCAL
#endif

class CSysCallThread;
struct IConsoleCmdArgs;

struct IVirtualKeyboardEvents
{
	// <interfuscator:shuffle>
	virtual ~IVirtualKeyboardEvents(){}
	virtual void KeyboardCancelled() = 0;
	virtual void KeyboardFinished(const char* pInString) = 0;
	// </interfuscator:shuffle>
};

enum EStringVerifyResponse
{
	eSVR_Invalid = -1,
	eSVR_OK      = 0,
	eSVR_TooLong,
	eSVR_OffensiveText,
	eSVR_NoDefaultString,
	eSVR_InvalidLanguage
};

struct IStringVerifyEvents
{
	// <interfuscator:shuffle>
	virtual ~IStringVerifyEvents(){}
	virtual void StringVerifyFinished(const char* inString, EStringVerifyResponse response) = 0;
	virtual void StringVerifyFailed(const char* inString) = 0;
	// </interfuscator:shuffle>
};

enum EUserPIIStatus
{
	k_pii_notRequested,       //!< PII not available and not requested.
	k_pii_pending,            //!< PII requested but not yet available.
	k_pii_available,          //!< PII available.
	k_pii_notPermittedByOS,   //!< PII access denied by OS.
	k_pii_notPermittedByUser, //!< PII access denied by user.
	k_pii_error               //!< Miscellaneous error.
};

struct SUserPII
{
	string firstName;
	string lastName;
	string email;
	uint16 languageId;
	uint8  countryId;                   //!< XONLINE_COUNTRY_ enum.
	uint8  age;
	struct
	{
		string street1;
		string street2;
		string city;
		string district;
		string state;
		string postCode;
	} address;
};
#if CRY_PLATFORM_DURANGO
struct SUserXUID
{
	enum { eBufferSize = 20 + 1 };
	wchar_t id[eBufferSize];

	SUserXUID()
	{
		id[0] = L'\0';
	}

	SUserXUID(const wchar_t* _xuid)
	{
		cry_strcpy_wchar(id, _xuid);
	}

	bool operator==(const SUserXUID& _rhs) const
	{
		return wcscmp(id, _rhs.id) == 0;
	}
};
#endif

struct SStreamingInstallProgress
{
	enum EState
	{
		eState_NotStarted = 0,
		eState_Active,
		eState_Paused,
		eState_Completed
	};
	SStreamingInstallProgress() : m_totalSize(0), m_installedSize(0), m_state(eState_NotStarted), m_progress(0.0f) {}

	uint64 m_totalSize;      //!< In bytes.
	uint64 m_installedSize;  //!< In bytes.
	EState m_state;
	float  m_progress;       //!< [0..1].
};

//! Interface platform OS (Operating System) functionality.
struct IPlatformOS
{
	virtual const char* GetPlatformName() const = 0;

	enum ECreateFlags
	{
		eCF_EarlyCorruptionDetected = BIT(1)
	};

	enum
	{
		Unknown_User = ~0
	};

	//! Minimum amount of time the animated save icon needs to be displayed on screen when saving data.
	enum
	{
		MINIMUM_SAVE_ICON_DISPLAY_TIME = 3
	};

	enum
	{
#if CRY_PLATFORM_DURANGO
		USER_MAX_NAME = 34,     //!< 34 characters for name (associated with live account), 16 for Gamertag (user can choose which one to display).
#elif CRY_PLATFORM_WINDOWS
		USER_MAX_NAME = 256 + 1,  //!< UNLEN+1,.
#else
		USER_MAX_NAME = 256,
#endif
	};

	typedef CryFixedStringT<USER_MAX_NAME> TUserName;

	enum ECDP_Start
	{
		eCDPS_Success,
		eCDPS_Failure,
		eCDPS_NoFreeSpace,
		eCDPS_AlreadyInUse,
		eCDPS_HDDNotReady,
	};

	enum ECDP_End
	{
		eCDPE_Success,
		eCDPE_Failure,
		eCDPE_WrongUser,
		eCDPE_NotInUse,
	};

	enum ECDP_Open
	{
		eCDPO_Success,
		eCDPO_MD5Failed,
		eCDPO_OpenPakFailed,
		eCDPO_FileNotFound,
		eCDPO_HashNotMatch,
		eCDPO_SizeNotMatch,
		eCDPO_NotInUse,
	};

	enum ECDP_Close
	{
		eCDPC_Success,
		eCDPC_Failure,
		eCDPC_NotInUse,
	};

	enum ECDP_Delete
	{
		eCDPD_Success,
		eCDPD_Failure,
		eCDPD_NotInUse,
	};

	enum ECDP_Write
	{
		eCDPW_Success,
		eCDPW_Failure,
		eCDPW_NoFreeSpace,
		eCDPW_NotInUse,
	};

	enum EFileOperationCode
	{
		eFOC_Success        = 0x00,     //!< No error.
		eFOC_Failure        = 0x01,     //!< Generic error.

		eFOC_ErrorOpenRead  = 0x12,     //!< Unable to open a file to read.
		eFOC_ErrorRead      = 0x14,     //!< Error reading from a file.
		eFOC_ErrorOpenWrite = 0x22,     //!< Unable to open a file to write.
		eFOC_ErrorWrite     = 0x24,     //!< Error writing to a file.

		// Masks.
		eFOC_ReadMask  = 0x10,          //!< Error reading.
		eFOC_WriteMask = 0x20,          //!< Error writing.
		eFOC_OpenMask  = 0x02,          //!< Error opening files/content.
	};

	enum
	{
		KbdFlag_Default  = 0x1,        //!< Keyboard in current selected language.
		KbdFlag_GamerTag = 0x2,        //!< Keyboard only has gamertag characters.
		KbdFlag_Email    = 0x4,        //!< Keyboard for URLs and e-mails.
		KbdFlag_Password = 0x8,        //!< subset of latin used for passwords, also does *ing out of displayed text.
	};

	//! Each entry fills in the relevant member of SStatAnyValue.
	enum EUserProfilePreference
	{
		EUPP_CONTROLLER_INVERT_Y,       //!< eVT_Int (0,1).
		EUPP_CONTROLLER_SENSITIVITY,    //!< eVT_Float (0.0f..2.0f).
		EUPP_GAME_DIFFICULTY,           //!< eVT_Int (0,1,2)=(easy,medium,hard).
		EUPP_AIM_ASSIST,                //!< eVT_Int (0,1).
		EUPP_REGION,                    //!< eVT_Int.
	};

	enum EDLCMountFail
	{
		eDMF_FileCorrupt,
		eDMF_DiskCorrupt,
		eDMF_XmlError,
		eDMF_NoDLCDir,
		eDMF_Unknown,
	};

	enum EZipExtractFail
	{
		eZEF_Success,
		eZEF_Unsupported,
		eZEF_WriteFail,
		eZEF_UnknownError,
	};

	//! Derive listeners from this interface.
	struct IDLCListener
	{
		// <interfuscator:shuffle>
		virtual ~IDLCListener(){}
		virtual void OnDLCMounted(const XmlNodeRef& rootNode, const char* sDLCRootFolder) = 0;
		virtual void OnDLCMountFailed(EDLCMountFail reason) = 0;
		virtual void OnDLCMountFinished(int nPacksFound) = 0;
		// </interfuscator:shuffle>
	};

	struct SUserProfileVariant
	{
		enum EVariantType
		{
			eVT_Invalid,
			eVT_Bool,
			eVT_Int,
			eVT_Float,
		};

		SUserProfileVariant() : m_type(eVT_Invalid), m_iValue(0) {}
		SUserProfileVariant(bool bBool) : m_type(eVT_Bool), m_bValue(bBool) {}
		SUserProfileVariant(int iValue) : m_type(eVT_Int), m_iValue(iValue) {}
		SUserProfileVariant(float fValue) : m_type(eVT_Float), m_fValue(fValue) {}

		EVariantType GetType() const  { return m_type; }

		bool         GetBool() const  { assert(GetType() == eVT_Bool); return m_bValue; }
		int          GetInt() const   { assert(GetType() == eVT_Int); return m_iValue; }
		float        GetFloat() const { assert(GetType() == eVT_Float); return m_fValue; }

	private:
		EVariantType m_type;

		union
		{
			bool  m_bValue;
			int   m_iValue;
			float m_fValue;
		};
	};

	//! Internal event handle passed to observers from listeners. Derive platform specific events from this.
	struct SPlatformEvent
	{
		enum EEventType
		{
			eET_None,
			eET_PlatformSpecific,

			// Platform agnostic events.
			eET_SignIn,                         //!< Uses m_signIn - allows a listener to know a user has been signed in or out.
			eET_StorageMounted,                 //!< Uses m_storageMounted - allows a listener to know a new save location is mounted for profiles etc.
			eET_StorageRemoved,                 //!< Uses m_storageRemoved - allows a listener to know that a storage device has been removed.
			eET_StorageCancelled,               //!< User cancelled a storage request overlap popup.
			eET_FileError,                      //!< Uses m_fileError fired when a file error occurs.
			eET_FileWrite,                      //!< Uses m_fileWrite - fired when platform code opens a profile/savegame for writing.
			eET_ContentInstalled,               //!< Allows a listener to know when downloadable content has been installed.
			eET_InstallBegin,                   //!< Allow a listener to know when game installation begins.
			eET_InstallComplete,                //!< Allow a listener to know when game installation is complete.
			eET_PostLocalisationBootChecksDone, //!< Allow a listener to know when setup tasks have completed.
			eET_PIIRetrieved,                   //!< PII has arrived and can be queried from the IPlatformOS interface.
			eET_SystemMenu,                     //!< Entered / exited system menu (uses m_systemMenu).
			eET_ContentRemoved,
		};

		enum ESignInState
		{
			eSIS_NotSignedIn,           //!< Signed out.
			eSIS_SignedInLocally,       //!< Signed in to local machine only.
			eSIS_SignedInLive,          //!< Signed in online.
		};

		//! The reason for having the issued and save start checkpoint is that saves can occur at points where saving to device is illegal by TCR/TRCs.
		//! This allows PlatformOS to buffer up the dirty saves and issue a eFWT_SaveStart or eFWT_SaveStartCheckPoint when it is safe to write to device.
		enum EFileWriteType
		{
			eFWT_Checkpoint,            //!< Inform the system a checkpoint has been issued (allows us to render a save icon on the screen whilst the game generates the save).
			eFWT_CheckpointLevelStart,  //!< Inform the system a checkpoint has been issued from level start.
			eFWT_SaveStart,             //!< Inform the system that saving to HDD/MU has started.
			eFWT_SaveEnd,               //!< Inform the system that saving to HDD/MU has finished.
			eFWT_CreatingSave,          //!< When creating a save, force the icon to be displayed.
		};

		//! Force user index on construction.
		SPlatformEvent(unsigned int user, EEventType eventType = eET_None)
			: m_user(user), m_eEventType(eventType) {}

		unsigned int m_user;
		EEventType   m_eEventType;

		union UEventParams
		{
			struct SSignIn
			{
				ESignInState m_previousSignedInState;
				ESignInState m_signedInState;
			} m_signIn;

			struct SStorageMounted
			{
				bool m_bPhysicalMedia;        //!< true if (360) saving to a physical device - HDD or MU or USB.
				bool m_bOnlyUpdateMediaState; //!< true to not attempt user change (360).
			} m_storageMounted;

			struct SStorageRemoved
			{
				bool m_bDeviceRemovedIsPrimary; //!< true when the removed device is the device the current save is on.
			} m_storageRemoved;

			struct SFileError
			{
				EFileOperationCode m_errorType;
				bool               m_bRetry; //!< retry the operation - load/save.
			} m_fileError;

			struct SFileWrite
			{
				EFileWriteType m_type;
			} m_fileWrite;

			struct SSystemMenu
			{
				bool m_bOpened;
				bool m_bClosed;
			} m_systemMenu;

			struct SContentRemoved
			{
				char m_root[4];
			} m_contentRemoved;
		} m_uParams;
	};

#if CRY_PLATFORM_ORBIS
	struct SPlatformEventOrbis : public SPlatformEvent
	{
		SPlatformEventOrbis(unsigned int user)
			: SPlatformEvent(user, eET_PlatformSpecific)
		{
			memset(&m_event, 0, sizeof(m_event));
		}
		SceSystemServiceEvent m_event;
	};
#endif

	struct ISaveReader
	{
		enum ESeekMode
		{
			ESM_BEGIN,
			ESM_CURRENT,
			ESM_END,
		};

		template<class T>
		EFileOperationCode ReadItem(T& data)
		{
			return ReadBytes(reinterpret_cast<void*>(&data), sizeof(T));
		}

		// <interfuscator:shuffle>
		virtual ~ISaveReader() {}
		virtual EFileOperationCode              Seek(long seek, ESeekMode mode) = 0;
		virtual EFileOperationCode              GetFileCursor(long& fileCursor) = 0;
		virtual EFileOperationCode              ReadBytes(void* data, size_t numBytes) = 0;
		virtual EFileOperationCode              GetNumBytes(size_t& numBytes) = 0;
		virtual EFileOperationCode              Close() = 0;
		virtual IPlatformOS::EFileOperationCode LastError() const = 0;
		virtual bool                            IsTheRightOwner() const { return true; }
		virtual void                            GetMemoryUsage(ICrySizer* pSizer) const = 0;
		virtual void                            TouchFile()             {}
		// </interfuscator:shuffle>
	};
	DECLARE_SHARED_POINTERS(ISaveReader);
	typedef std::vector<IPlatformOS::ISaveReaderPtr> ISaveReaderPtrVector;

	struct ISaveWriter
	{
		template<class T>
		IPlatformOS::EFileOperationCode AppendItem(const T& data)
		{
			return AppendItems(&data, 1);
		}

		template<class T>
		IPlatformOS::EFileOperationCode AppendItems(T* data, size_t elems)
		{
			return AppendBytes(static_cast<const void*>(data), sizeof(T) * elems);
		}

		// <interfuscator:shuffle>
		virtual ~ISaveWriter() {}
		virtual IPlatformOS::EFileOperationCode AppendBytes(const void* data, size_t length) = 0;
		virtual EFileOperationCode              Close() = 0;
		virtual IPlatformOS::EFileOperationCode LastError() const = 0;
		virtual void                            GetMemoryUsage(ICrySizer* pSizer) const = 0;
		// </interfuscator:shuffle>
	};
	DECLARE_SHARED_POINTERS(ISaveWriter);
	typedef std::vector<IPlatformOS::ISaveWriterPtr> ISaveWriterPtrVector;

	struct IFileFinder
	{
		enum EFileState
		{
			eFS_NotExist,
			eFS_File,
			eFS_Directory,
			eFS_FileOrDirectory = -1, //!< For Win32-based bool return value we use this. \see CFileFinderCryPak::FileExists.
		};

		// <interfuscator:shuffle>
		virtual ~IFileFinder(){}

		//! Determine if a file or path exists.
		virtual EFileState FileExists(const char* path) = 0;

		//! Find the first file matching a specified pattern.
		//! \return file handle to be used with FindNext() and FindClose()
		virtual intptr_t FindFirst(const char* filePattern, _finddata_t* fd) = 0;

		//! Close and release resources associated with FindFirst/FindNext operations.
		virtual int FindClose(intptr_t handle) = 0;

		//! Find the next file matching a specified pattern.
		virtual int FindNext(intptr_t handle, _finddata_t* fd) = 0;

		//! Provide the ability to remap a file path into a platform specific file path.
		//! This is a convenience utility function, the default implementation simply returns the input path and returns true.
		//! \param filePath         Raw input file path.
		//! \param filePathMapped   OS-specific mapped file path.
		//! \param bCreateMapping   Set to true to record the remapped path or false to only return the mapped file path.
		//! \return true if the remapped path already existed in the file mapping table
		virtual bool MapFilePath(const char* filePath, string& filePathMapped, bool bCreateMapping) { filePathMapped = filePath; return true; }

		virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
		// </interfuscator:shuffle>
	};
	typedef std::shared_ptr<IFileFinder> IFileFinderPtr;

	//! Before a load or save operation embed an object of this type and check Allowed().
	//! If Allowed() fails do not load or save. Function can immediately return.
	class CScopedSaveLoad
	{
	public:
		CScopedSaveLoad(unsigned int user, bool bSave) : m_user(user) { m_allow = GetISystem()->GetPlatformOS()->BeginSaveLoad(user, bSave); }
		~CScopedSaveLoad() { if (m_allow) GetISystem()->GetPlatformOS()->EndSaveLoad(m_user); }
		bool Allowed() const { return m_allow; }
	private:
		unsigned int m_user;
		bool         m_allow;
	};

	//! Derive listeners from this interface and cast event to the appropriate platform event interface.
	struct IPlatformListener
	{
		// <interfuscator:shuffle>
		virtual ~IPlatformListener(){}
		virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& event) = 0;
		// </interfuscator:shuffle>
	};

	struct SDebugDump
	{
		bool (* OpenFile)(const char* filename, bool append);
		bool (* WriteToFile)(const void* ptr, unsigned int size, unsigned int count);
		bool (* CloseFile)();
	};

	//! Call once to create and initialize. Use delete to destroy.
	//! \see ECreateFlags
	static IPlatformOS* Create(const uint8 createParams /*see ECreateFlags*/);

	// <interfuscator:shuffle>
	virtual ~IPlatformOS() {}

	//! Tick with each frame to determine if there are any system messages requiring handling.
	virtual void Tick(float realFrameTime) = 0;

	//! Local user profile functions to check/initiate user sign in:.

	//! Returns the maximum number of signed in users. EG: On Xbox is 4.
	virtual unsigned int UserGetMaximumSignedInUsers() const = 0;

	//! Returns true if the user is signed in to the OS.
	virtual bool UserIsSignedIn(unsigned int userIndex) const = 0;

	//! Returns true if a named user is signed in to the OS.
	virtual bool UserIsSignedIn(const IPlatformOS::TUserName& userName, unsigned int& outUserIndex) const = 0;

	//! Initiate signin dialog box in the OS.
	//! \param numUsersRequested Number of users requested or 0 to sign in the default user.
	//! \param controllerIndex Controller used to request sign in.
	//! \return true on success.
	virtual bool UserDoSignIn(unsigned int numUsersRequested = 1, unsigned int controllerIndex = 0) = 0;

	//! Sign out a user.
	//! This is required for 360 when returning to the splash screen where you can press Start to initiate sign in with any controller.
	//! \param controllerIndex the controller used to sign out.
	virtual void UserSignOut(unsigned int user) = 0;

	//! Returns the player index of a signed in user or Unknown_User if the user is not signed in.
	virtual unsigned int UserGetPlayerIndex(const char* userName) const = 0;

	//! Get the offline name of a user. Returns true on success.
	virtual bool UserGetName(unsigned int userIndex, IPlatformOS::TUserName& outName) const = 0;
#if CRY_PLATFORM_DURANGO
	//! Get the User::Id (Platform, unique id for user in this console) of a user. -1 if user is not valid.
	virtual int UserGetId(unsigned int userIndex) const = 0;

	//! Get the XUID name of a user. Returns true on success.
	virtual bool UserGetXUID(unsigned int userIndex, SUserXUID&) const = 0;
#endif
	//! Get the online name of a user. Returns true on success.
	virtual bool UserGetOnlineName(unsigned int userIndex, IPlatformOS::TUserName& outName) const = 0;

	//! Get the user to select a storage device location for save data.
	//! Returns true if storage is now available for the user.
	virtual bool UserSelectStorageDevice(unsigned int userIndex, bool bForceUI = false) = 0;

	//! Get a specific preference for the signed in user.
	virtual bool GetUserProfilePreference(unsigned int user, EUserProfilePreference ePreference, SUserProfileVariant& outResult) const = 0;

	//! Use the TCR/TRC compliant platform save/load API for save game data.
	virtual bool UsePlatformSavingAPI() const = 0;

	//! Tell the platform we are about to begin a save or load and allow it to deny the service.
	//! \param bSave true if this is a save operation, false if a load operation.
	//! \return true if the operation is allowed to proceed, false to deny save or load.
	virtual bool BeginSaveLoad(unsigned int user, bool bSave) = 0;

	//! Tell the platform we have finished a save or load.
	virtual void EndSaveLoad(unsigned int user) = 0;

	//! Get a reader object to read from a save file. The file is automatically opened and closed.
	virtual IPlatformOS::ISaveReaderPtr SaveGetReader(const char* fileName, unsigned int user = IPlatformOS::Unknown_User) = 0;

	//! Get a writer object to write to a save file. The file is automatically opened and closed.
	virtual IPlatformOS::ISaveWriterPtr SaveGetWriter(const char* fileName, unsigned int user = IPlatformOS::Unknown_User) = 0;

	//! Get a IFindFile interface for searching for files.
	virtual IPlatformOS::IFileFinderPtr GetFileFinder(unsigned int user) = 0;

	//! Looks for DLC and mounts the drive so that the files can be read.
	//! \param callback   Class which will receive the DLC mounted events.
	//! \param user	      User index used for 360.
	//! \param keyData    License key used to load the data.
	virtual void MountDLCContent(IDLCListener* pCallback, unsigned int user, const uint8 keyData[16]) = 0;

	// Cache Data Paks API.
	virtual IPlatformOS::ECDP_Start  StartUsingCachePaks(const int user, bool* outWritesOccurred) = 0;
	virtual IPlatformOS::ECDP_End    EndUsingCachePaks(const int user) = 0;
	virtual IPlatformOS::ECDP_Open   DoesCachePakExist(const char* const filename, const size_t size, unsigned char md5[16]) = 0;
	virtual IPlatformOS::ECDP_Open   OpenCachePak(const char* const filename, const char* const bindRoot, const size_t size, unsigned char md5[16]) = 0;
	virtual IPlatformOS::ECDP_Close  CloseCachePak(const char* const filename) = 0;
	virtual IPlatformOS::ECDP_Delete DeleteCachePak(const char* const filename) = 0;
	virtual IPlatformOS::ECDP_Write  WriteCachePak(const char* const filename, const void* const pData, const size_t numBytes) = 0;

#if CRY_PLATFORM_DURANGO
	//! Save or load data using connected storage.
	//! Connected storage can have multiple containers. Each container can have multiple data blocks.
	struct SContainerDataBlock
	{
		const wchar_t* blockName;

		uint8*         pDataBlock;
		size_t         dataBlockSize; //!< Indicates buffer capacity, or number of bytes read.

		// TODO: error code is available only for LoadFromStorage API for now.
		enum EErrorCode
		{
			eEC_Ok,                      //!< Operation finished successfully.
			eEC_Error,                   //!< Unknown error.
			eEC_InsufficientBuffer,
			eEC_UserNotFound,
			eEC_BlobNotFound,
			eEC_ContainerNameError,
		};

		EErrorCode errorCode;
	};
	typedef DynArray<SContainerDataBlock> TContainerDataBlocks;

	enum EConnectedStorageType
	{
		eCST_ERA,
		eCST_SRA,

		eCST_Max,
	};

	// NB: reason why SaveIntoStorage and DeleteStorage don't have 'type' parameter is that ERA can't manipulate SRA's connected storage.

	virtual bool SaveIntoStorage(const wchar_t* containerName, const TContainerDataBlocks& dataBlocks) = 0;
	virtual bool LoadFromStorage(EConnectedStorageType type, const wchar_t* containerName, TContainerDataBlocks& dataBlocks) = 0;
	virtual bool DeleteStorage(const wchar_t* containerName) = 0;

	// Looks for content packages and calls the listener's OnDLCPackageFound for each package which is installed and usable by the current user.
	// \param isSRA If true, checks only for SRA. If false, checks for normal DLC.
	// virtual void EnumeratePackages(IDLCListener* pListener, bool isSRA) = 0;.

	//! Provide CPlatformOS_Durango a primary service configuration id of SRA.
	//! \param primaryServiceConfigId GUID to identify service configuration id of SRA.
	virtual void SetSRAPrimaryServiceConfigId(const wchar_t* primaryServiceConfigId) = 0;

#endif

	//! Finds any .zip files in the directory given, opens them, and writes the unpacked contents into the same directory.
	//! Useful for unpacking DLC content after download.
	//! \param path	The path to search for zips in.
	//! \note Only supported on PC.
	virtual IPlatformOS::EZipExtractFail ExtractZips(const char* path) = 0;

	//! If the platform supports restarting the current title.
	virtual bool CanRestartTitle() const = 0;

	//! Restart the current title (if CanRestartTitle is true).
	virtual void RestartTitle(const char* pTitle) = 0;

	//! Collect memory statistics in CrySizer.
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Returns the user ID of the first signed in user, or Unknown_User if no one is signed in.
	virtual int GetFirstSignedInUser() const = 0;

	//! Returns the user's PII (Personally Identifiable Information).
	//! On first request it will start the act of retrieval, after which the caller can poll or wait for eET_PIIRetrieved.
	virtual EUserPIIStatus GetUserPII(unsigned int inUser, SUserPII* pOutPII) = 0;

	//! Inform the platform os that the optical drive is currently idle, and extra data can be read from it if needed.
	virtual void SetOpticalDriveIdle(bool bIdle) = 0;

	//! Inform the platform OS that optical drive can/can't be used for extra reads.
	virtual void AllowOpticalDriveUsage(bool bAllow) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Keyboard.

	//! Starts the virtual keyboard. Flags are from KbdFlag_* enum. title string is not copied. you must pass a set of callbacks via IVirtualKeyboardEvents.
	//! Title & initialInput params must remain valid for the duration of the system keyboard, therefore they must NOT have been originally declared on the stack.
	//! \return false if the keyboard failed to start.
	virtual bool KeyboardStart(unsigned int inUserIndex, unsigned int flags, const char* title, const char* initialInput, int maxInputLength, IVirtualKeyboardEvents* pInCallback) = 0;

	//! \return true if the virtual keyboard is currently displayed.
	virtual bool KeyboardIsRunning() = 0;

	//! Cancels the on screen keyboard if it is running. Calls the IVirtualKeyboardEvents::KeyboardCancelled() method.
	//! \return true if successful, false if the keyboard can't be cancelled, in which case check if it is still running with KeyboardIsRunning().
	virtual bool KeyboardCancel() = 0;

	//////////////////////////////////////////////////////////////////////////
	// String Verification.

	virtual bool StringVerifyStart(const char* inString, IStringVerifyEvents* pInCallback) = 0;
	virtual bool IsVerifyingString() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Platform listener.

	virtual void AddListener(IPlatformListener* pListener, const char* szName) = 0;
	virtual void RemoveListener(IPlatformListener* pListener) = 0;
	virtual void NotifyListeners(SPlatformEvent& event) = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Platform-specific localization and SKU information.

	//! Get the system language and map it to a platform-independent language code.
	virtual ILocalizationManager::EPlatformIndependentLanguageID GetSystemLanguage() = 0;

	//! Get the SKU identifier from the platform.
	virtual const char* GetSKUId() = 0;

	//! Compose a bitfield that enumerates the languages available on this system OS.
	//! This is used to determine the overlap between languages supplied in a SKU and languages available on a system.
	//! Some platforms like the PS3 have TRCs relating to whether or not you should show a language select screen based on this information.
	virtual ILocalizationManager::TLocalizationBitfield GetSystemSupportedLanguages() = 0;

	//! Begin platform specific boot checks to meet TRCs/TCRs.
	//! Call after localization has been initialized and chosen to meet TCG's guidance.
	//! This should tend to throw fatal errors using platform specific APIs in order to meet TRCs robustly.
	virtual bool PostLocalizationBootChecks() = 0;

	//! Save raw save/load game to/from local hard drive. Implement this for each system.
	virtual bool ConsoleLoadGame(IConsoleCmdArgs* pArgs) = 0;

	virtual bool DebugSave(SDebugDump& dump) = 0;

	//! Retrieve the local IP address as both as dotted ip format string and as an unsigned int.
	//! \param length Available space in ipAddress buffer (should be >=16!)
	virtual bool GetLocalIPAddress(char* ipAddress, unsigned int& ip, int length) const = 0;

	//! Get the host name of the current machine.
	virtual const char* const GetHostName() = 0;

	//! Initialize the encryption key from game.
	virtual void InitEncryptionKey(const char* pMagic, size_t magicLength, const uint8* pKey, size_t keyLength) = 0;

	//! All parameters are optional.
	//! \param pMagic   Magic header string to recognize an encrypted file.
	//! \param pKey     Encryption key to use.
	virtual void   GetEncryptionKey(const std::vector<char>** pMagic = NULL, const std::vector<uint8>** pKey = NULL) = 0;

	virtual bool   IsInstalling() const        { return false; }
	virtual bool   IsHDDAvailable() const      { return true; }
	virtual uint64 GetHDDCacheCopySize() const { return 0; }

	virtual bool   CanSafelyQuit() const       { return true; }

	//! Function to handle a verification error.
	//! Used by some platforms to handle hacking attempts.
	virtual void HandleArchiveVerificationFailure() {};

	// </interfuscator:shuffle>

	//! Called from CSystem::RenderEnd(); used to draw platform debug stuff.
	virtual void RenderEnd() {}

	//! Application Clip Capture.
	//! Interface for capturing in-game clips.
	struct IClipCaptureOS
	{
		struct IListener
		{
			enum ECaptureFinishedStatus
			{
				eCFS_Success = 0,
				eCFS_Failure
			};

			virtual void OnClipCaptureFinished(ECaptureFinishedStatus status) = 0;
		};

		virtual ~IClipCaptureOS() {}
		virtual void  GetMinMaxClipsPerTitle(uint32& minNumClips_, uint32& maxNumClips_) const { minNumClips_ = minNumClips; maxNumClips_ = maxNumClips; }
		virtual float GetMaxTotalClipsTimePerTitle() const                                     { return maxTotalClipsTime; }

		struct SSpan
		{
			explicit SSpan(float before = 0.0f, float after = 0.0f) : secondsBefore(before), secondsAfter(after) {}
			float       secondsBefore; //!< Recording starts in the past using the platform's buffering if available.
			float       secondsAfter;  //!< Recording ends in the future, keeping on storing until this delay is elapsed.
			ILINE float GetDuration() const { return secondsBefore + secondsAfter; }
		};

		struct SClipTextInfo
		{
			SClipTextInfo(const char* _szGreatestMomentId, const wchar_t* _wszLocalizedClipName = NULL, const char* _szTitleData = NULL)
				: szGreatestMomentId(_szGreatestMomentId), wszLocalizedClipName(_wszLocalizedClipName), szTitleData(_szTitleData)
			{}

			//! The title-defined ID string that identifies the description of the clip.
			//! Developers set up valid identifiers through Xbox Developer Portal.
			//! The cloud service uses this ID to look up the description string in the ApplicationClip class.
			const char* szGreatestMomentId;

			//! Contains the name of the clip localized into the current language.
			//! If this string is defined, it is displayed in the toast.
			const wchar_t* wszLocalizedClipName;

			//! A title-defined string that is stored with the clip.
			//! Titles use the ApplicationClipInfo class to set this information in the clip at the time of recording
			//! or use the UpdateClipMetadataAsync class to change this information once the clip has been uploaded to the cloud.
			const char* szTitleData;
		};

		virtual bool RecordClip(SClipTextInfo clipTextInfo, const SSpan& span, IListener* pListener = NULL, const bool makeLocalCopy = false) { return false; }

		// These parameters are based on initial MS requirements.
		uint32 minNumClips;         //!< Minimum number of clips to record per game/session.
		uint32 maxNumClips;         //!< Maximum number of clips to record per game/session.
		float  maxTotalClipsTime;   //!< All clips combined cannot amount to more than this time per game/session.
	};

	virtual IClipCaptureOS* GetClipCapture() { return NULL; }

	// IPAddress utility functions.

	//! Returns the string ("xxx.xxx.xxx.xxx") format of address to the buffer toString).
	//! \note 0xC0A80001 will be "192.168.0.1" on all platforms.
	char* UIPToText(unsigned int address, char* toString, int length) const
	{
		if (length < 16)
		{
			return NULL;
		}
		// Endian Agnostic conversion to string
		char* s = toString;
		int i;
		for (i = 0; i < 4; i++)
		{
			int val = (address >> ((3 - i) * 8)) & 0xFF;
			if (val > 99)
			{
				* s++ = (char)('0' + (val / 100));
				val %= 100;
				* s++ = (char)('0' + (val / 10));
				val %= 10;
			}
			if (val > 9)
			{
				* s++ = (char)('0' + (val / 10));
				val %= 10;
			}
			* s++ = (char)('0' + val);
			if (i < 3)
			{
				* s++ = '.';
			}
		}
		* s = 0;
		return toString;
	}
	//! Returns the IP as unsigned int from string ("xxx.xxx.xxx.xxx").
	//! \note "192.168.0.1" will be 0xC0A80001 on all platform.
	unsigned int TextToUIP(const char* fromString)  const
	{
		unsigned int ip1, ip2, ip3, ip4;
		int numScanned = sscanf(fromString, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4);
		unsigned int ip = 0;
		ip |= ip4;
		ip |= ip3 << 8;
		ip |= ip2 << 16;
		ip |= ip1 << 24;
		return ip;
	}

	//! Interface for streaming install.
	virtual void QueryStreamingInstallProgressForChunk(const unsigned int chunkID, SStreamingInstallProgress* pProgress) const
	{
		memset(pProgress, 0, sizeof(SStreamingInstallProgress));
		pProgress->m_progress = 1.0f;
		pProgress->m_state = SStreamingInstallProgress::eState_Completed;
	}
	virtual void QueryStreamingInstallProgressForLevel(const char* sLevelName, SStreamingInstallProgress* pProgress) const
	{
		memset(pProgress, 0, sizeof(SStreamingInstallProgress));
		pProgress->m_progress = 1.0f;
		pProgress->m_state = SStreamingInstallProgress::eState_Completed;
	}
	virtual void   SwitchStreamingInstallPriorityToLevel(const char* sLevelName)                 {}
	virtual bool   IsFileAvailable(const char* sFileName) const                                  { return true; }
	virtual bool   IsPakRequiredForLevel(const char* sPakFileName, const char* sLevelName) const { return true; }
	virtual uint32 GetPakSortIndex(const char* sPakFileName) const                               { return 0; }

#if CRY_PLATFORM_DURANGO
	//! Add/remove count for tasks which must be done before title gets into suspended mode.
	virtual void LockSuspending() = 0;
	virtual void UnlockSuspending() = 0;

	//! \return true if there is no remaining tasks that prevent title to suspend.
	virtual bool CanSuspend() const = 0;
#endif

protected:

	class CFileFinderCryPak : public IFileFinder
	{
	public:
		virtual IFileFinder::EFileState FileExists(const char* path) { return gEnv->pCryPak->IsFileExist(path) ? eFS_FileOrDirectory : eFS_NotExist; }
		virtual intptr_t                FindFirst(const char* filePattern, _finddata_t* fd)
		{
			return gEnv->pCryPak->FindFirst(filePattern, fd, 0, true);
		}
		virtual int  FindClose(intptr_t handle)                 { return gEnv->pCryPak->FindClose(handle); }
		virtual int  FindNext(intptr_t handle, _finddata_t* fd) { return gEnv->pCryPak->FindNext(handle, fd); }
		virtual void GetMemoryUsage(ICrySizer* pSizer) const    { pSizer->Add(*this); }
	private:
		CScopedAllowFileAccessFromThisThread m_allowFileAccess;
	};

	//! Class to watch FPS until it becomes stable.
	class CStableFPSWatcher
	{
		float m_resetTime;         //!< Last time that the FPS was too low.
		float m_minFPS;            //!< Minimum stable FPS to achieve.
		float m_minStableTime;     //!< Minimum time to maintain stable FPS.
		float m_maxTime;           //!< Maximum time to test before accepting.

	public:
		CStableFPSWatcher(float minFPS = 15.0f, float minStableTime = 2.5f, float maxTime = 6.0f)
			: m_resetTime(0.0f)
			, m_minFPS(minFPS)
			, m_minStableTime(minStableTime)
			, m_maxTime(maxTime)
		{
		}

		void Reset()
		{
			m_resetTime = gEnv->pTimer->GetAsyncCurTime();
		}

		//! Returns true when stable FPS has been attained for the required amount of time.
		bool HasAchievedStableFPS()
		{
			float fps = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);
			if (fps > 0.000001f)
				fps = __fres(fps);

			float curTime = gEnv->pTimer->GetAsyncCurTime();
			if (fps < m_minFPS)
			{
				Reset();
				return false;
			}
			else
			{
				float timeDiff = curTime - m_resetTime;
				if (timeDiff >= m_minStableTime)
					return true;
				if (timeDiff > m_maxTime)
					return true;
				return false;
			}
		}
	};

};

#endif
