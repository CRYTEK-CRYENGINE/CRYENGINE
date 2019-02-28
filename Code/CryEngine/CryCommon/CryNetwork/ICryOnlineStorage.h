// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

struct CryOnlineStorageQueryData;

struct IOnlineStorageListener
{
	// <interfuscator:shuffle>
	virtual ~IOnlineStorageListener(){}
	virtual void OnOnlineStorageOperationComplete(const CryOnlineStorageQueryData& QueryData) = 0;
	// </interfuscator:shuffle>
};

enum ECryOnlineStorageLocation
{
	eCOSL_Title,
	eCOSL_User
};

enum ECryOnlineStorageOperation
{
	eCOSO_Upload,
	eCOSO_Download
};

#if CRY_PLATFORM_DURANGO
enum ECryOnlineStorageDataType
{
	eCOSDT_Binary,
	eCOSDT_Json,
	eCOSDT_Config
};
#endif

struct CryOnlineStorageQueryData
{
	CryLobbyTaskID             lTaskID;           //!< No need to set this, it's done internally.
	ECryLobbyError             eResult;
	uint32                     userID;
	ECryOnlineStorageOperation operationType;
	ECryOnlineStorageLocation  storageLocation;
#if CRY_PLATFORM_DURANGO
	ECryOnlineStorageDataType  dataType;
	const wchar_t*             targetXuid;        //!< Null or empty string when querying against active user.
	uint32                     httpStatusCode;
#endif
	const char*                szItemURL;         //!< Name that identifies the data in online storage.
	char*                      pBuffer;           //!< Memory to upload to or download from online storage.
	uint32                     bufferSize;        //!< Size of the buffer.
	IOnlineStorageListener*    pListener;

	CryOnlineStorageQueryData()
	{
		memset(this, 0, sizeof(CryOnlineStorageQueryData));
	}
	CryOnlineStorageQueryData(const CryOnlineStorageQueryData& from)
	{
		memcpy(this, &from, sizeof(CryOnlineStorageQueryData));
	}
};

//! Interface for uploading and downloading data from platform specific online storage.
//! PSN and Xbox LIVE! both implement flavors of this.
struct ICryOnlineStorage
{
	// <interfuscator:shuffle>
	virtual ~ICryOnlineStorage(){};

	virtual void Tick(CTimeValue tv) = 0;

	//! Add an operation to the queue.
	virtual ECryLobbyError OnlineStorageDataQuery(CryOnlineStorageQueryData* pQueryData) = 0;
	// </interfuscator:shuffle>
};

//! \endcond