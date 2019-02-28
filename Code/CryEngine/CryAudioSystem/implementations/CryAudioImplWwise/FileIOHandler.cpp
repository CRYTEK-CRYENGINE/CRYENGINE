// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FileIOHandler.h"
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySystem/File/ICryPak.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#define MAX_NUMBER_STRING_SIZE   (10)       // 4G
#define ID_TO_STRING_FORMAT_BANK AKTEXT("%u.bnk")
#define ID_TO_STRING_FORMAT_WEM  AKTEXT("%u.wem")
#define MAX_EXTENSION_SIZE       (4)                                               // .xxx
#define MAX_FILETITLE_SIZE       (MAX_NUMBER_STRING_SIZE + MAX_EXTENSION_SIZE + 1) // null-terminated

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
AkFileHandle GetFileHandle(FILE* const pFile)
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	return reinterpret_cast<AkFileHandle>(pFile);
#elif CRY_PLATFORM_ORBIS
	return static_cast<AkFileHandle>(reinterpret_cast<INT_PTR>(pFile));
#else
	#error "Undefined platform!"
#endif
}

//////////////////////////////////////////////////////////////////////////
CFileIOHandler::CFileIOHandler()
	: m_bAsyncOpen(false)
{
	memset(m_sBankPath, 0, AK_MAX_PATH * sizeof(AkOSChar));
	memset(m_sLanguageFolder, 0, AK_MAX_PATH * sizeof(AkOSChar));
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CFileIOHandler::Init(AkDeviceSettings const& rDeviceSettings, bool const bAsyncOpen /* = false */)
{
	AKRESULT eResult = AK_Fail;

	if (rDeviceSettings.uSchedulerTypeFlags == AK_SCHEDULER_BLOCKING)
	{
		m_bAsyncOpen = bAsyncOpen;

		// If the Stream Manager's File Location Resolver was not set yet, set this object as the
		// File Location Resolver (this I/O hook is also able to resolve file location).
		if (!AK::StreamMgr::GetFileLocationResolver())
		{
			AK::StreamMgr::SetFileLocationResolver(this);
		}

		// Create a device in the Stream Manager, specifying this as the hook.
		m_nDeviceID = AK::StreamMgr::CreateDevice(rDeviceSettings, this);

		if (m_nDeviceID != AK_INVALID_DEVICE_ID)
		{
			eResult = AK_Success;
		}
	}
	else
	{
		AKASSERT(!"CAkDefaultIOHookBlocking I/O hook only works with AK_SCHEDULER_BLOCKING devices");
	}

	return eResult;
}

//////////////////////////////////////////////////////////////////////////
void CFileIOHandler::ShutDown()
{
	if (AK::StreamMgr::GetFileLocationResolver() == this)
	{
		AK::StreamMgr::SetFileLocationResolver(nullptr);
	}

	AK::StreamMgr::DestroyDevice(m_nDeviceID);
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CFileIOHandler::Open(AkOSChar const* sFileName, AkOpenMode eOpenMode, AkFileSystemFlags* pFlags, bool& rSyncOpen, AkFileDesc& rFileDesc)
{
	AKRESULT eResult = AK_Fail;

	if (rSyncOpen || !m_bAsyncOpen)
	{
		rSyncOpen = true;
		AkOSChar finalFilePath[AK_MAX_PATH] = { '\0' };
		AKPLATFORM::SafeStrCpy(finalFilePath, m_sBankPath, AK_MAX_PATH);
		char* szTemp = "Init.bnk";
		AkOSChar* pTemp = nullptr;
		CONVERT_CHAR_TO_OSCHAR(szTemp, pTemp);
		bool const bIsInitBank = (AKPLATFORM::OsStrCmp(pTemp, sFileName) == 0);

		if (pFlags != nullptr && eOpenMode == AK_OpenModeRead && !bIsInitBank)
		{
			// Add language folder if the file is localized.
			if (pFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC && pFlags->uCodecID == AKCODECID_BANK && pFlags->bIsLanguageSpecific)
			{
				AKPLATFORM::SafeStrCpy(finalFilePath, m_sLanguageFolder, AK_MAX_PATH);
			}
		}

		AKPLATFORM::SafeStrCat(finalFilePath, sFileName, AK_MAX_PATH);

		szTemp = nullptr;
		CONVERT_OSCHAR_TO_CHAR(finalFilePath, szTemp);
		char const* sOpenMode = nullptr;

		switch (eOpenMode)
		{
		case AK_OpenModeRead:
			{
				sOpenMode = "rbx";

				break;
			}
		case AK_OpenModeWrite:
			{
				sOpenMode = "wbx";

				break;
			}
		case AK_OpenModeWriteOvrwr:
			{
				sOpenMode = "w+bx";

				break;
			}
		case AK_OpenModeReadWrite:
			{
				sOpenMode = "abx";

				break;
			}
		default:
			{
				AKASSERT(!"Unknown file open mode!");

				break;
			}
		}

		FILE* const pFile = gEnv->pCryPak->FOpen(szTemp, sOpenMode, ICryPak::FOPEN_HINT_DIRECT_OPERATION);

		if (pFile != nullptr)
		{
			rFileDesc.iFileSize = static_cast<AkInt64>(gEnv->pCryPak->FGetSize(szTemp));
			rFileDesc.hFile = GetFileHandle(pFile);
			rFileDesc.uSector = 0;
			rFileDesc.deviceID = m_nDeviceID;
			rFileDesc.pCustomParam = nullptr;
			rFileDesc.uCustomParamSize = 0;

			eResult = AK_Success;
		}
	}

	return eResult;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CFileIOHandler::Open(AkFileID nFileID, AkOpenMode eOpenMode, AkFileSystemFlags* pFlags, bool& rSyncOpen, AkFileDesc& rFileDesc)
{
	AKRESULT eResult = AK_Fail;

	if (pFlags != nullptr && (rSyncOpen || !m_bAsyncOpen))
	{
		rSyncOpen = true;
		AkOSChar finalFilePath[AK_MAX_PATH] = { '\0' };
		AKPLATFORM::SafeStrCpy(finalFilePath, m_sBankPath, AK_MAX_PATH);

		if (eOpenMode == AK_OpenModeRead)
		{
			// Add language folder if the file is localized.
			if (pFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC && pFlags->bIsLanguageSpecific)
			{
				AKPLATFORM::SafeStrCpy(finalFilePath, m_sLanguageFolder, AK_MAX_PATH);
			}
		}

		AkOSChar fileName[MAX_FILETITLE_SIZE] = { '\0' };

		AkOSChar const* const pFilenameFormat = pFlags->uCodecID == AKCODECID_BANK ? ID_TO_STRING_FORMAT_BANK : ID_TO_STRING_FORMAT_WEM;

		AK_OSPRINTF(fileName, MAX_FILETITLE_SIZE, pFilenameFormat, static_cast<int unsigned>(nFileID));

		AKPLATFORM::SafeStrCat(finalFilePath, fileName, AK_MAX_PATH);

		char* szTemp = nullptr;
		CONVERT_OSCHAR_TO_CHAR(finalFilePath, szTemp);

		size_t const fileSize = gEnv->pCryPak->FGetSize(szTemp);

		if (fileSize > 0)
		{
			FILE* const pFile = gEnv->pCryPak->FOpen(szTemp, "rbx", ICryPak::FOPEN_HINT_DIRECT_OPERATION);

			if (pFile != nullptr)
			{
				rFileDesc.iFileSize = static_cast<AkInt64>(fileSize);
				rFileDesc.hFile = GetFileHandle(pFile);
				rFileDesc.uSector = 0;
				rFileDesc.deviceID = m_nDeviceID;
				rFileDesc.pCustomParam = nullptr;
				rFileDesc.uCustomParamSize = 0;

				eResult = AK_Success;
			}
		}
	}

	return eResult;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CFileIOHandler::Read(AkFileDesc& rFileDesc, AkIoHeuristics const& rHeuristics, void* pBuffer, AkIOTransferInfo& rTransferInfo)
{
	AKASSERT(pBuffer != nullptr && rFileDesc.hFile != AkFileHandle(INVALID_HANDLE_VALUE));

	FILE* const pFile = reinterpret_cast<FILE*>(rFileDesc.hFile);
	long const nCurrentFileReadPos = gEnv->pCryPak->FTell(pFile);
	long const nWantedFileReadPos = static_cast<long>(rTransferInfo.uFilePosition);

	if (nCurrentFileReadPos != nWantedFileReadPos)
	{
		gEnv->pCryPak->FSeek(pFile, nWantedFileReadPos, SEEK_SET);
	}

	size_t const nBytesRead = gEnv->pCryPak->FReadRaw(pBuffer, 1, rTransferInfo.uRequestedSize, pFile);
	AKASSERT(nBytesRead == static_cast<size_t>(rTransferInfo.uRequestedSize));

	return (nBytesRead > 0) ? AK_Success : AK_Fail;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CFileIOHandler::Write(AkFileDesc& rFileDesc, AkIoHeuristics const& rHeuristics, void* pBuffer, AkIOTransferInfo& rTransferInfo)
{
	AKASSERT(pBuffer != nullptr && rFileDesc.hFile != AkFileHandle(INVALID_HANDLE_VALUE));

	FILE* const pFile = reinterpret_cast<FILE*>(rFileDesc.hFile);
	long const nCurrentFileWritePos = gEnv->pCryPak->FTell(pFile);
	long const nWantedFileWritePos = static_cast<long>(rTransferInfo.uFilePosition);

	if (nCurrentFileWritePos != nWantedFileWritePos)
	{
		gEnv->pCryPak->FSeek(pFile, nWantedFileWritePos, SEEK_SET);
	}

	size_t const nBytesWritten = gEnv->pCryPak->FWrite(pBuffer, 1, static_cast<size_t>(rTransferInfo.uRequestedSize), pFile);
	AKASSERT(nBytesWritten == static_cast<size_t>(rTransferInfo.uRequestedSize));

	return (nBytesWritten > 0) ? AK_Success : AK_Fail;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CFileIOHandler::Close(AkFileDesc& rFileDesc)
{
	AKRESULT eResult = AK_Fail;

	if (!gEnv->pCryPak->FClose(reinterpret_cast<FILE*>(rFileDesc.hFile)))
	{
		eResult = AK_Success;
	}
	else
	{
		AKASSERT(!"Failed to close file handle");
	}

	return eResult;
}

//////////////////////////////////////////////////////////////////////////
AkUInt32 CFileIOHandler::GetBlockSize(AkFileDesc& in_fileDesc)
{
	// No constraint on block size (file seeking).
	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CFileIOHandler::GetDeviceDesc(AkDeviceDesc& out_deviceDesc)
{
}

//////////////////////////////////////////////////////////////////////////
AkUInt32 CFileIOHandler::GetDeviceData()
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CFileIOHandler::SetBankPath(AkOSChar const* const sBankPath)
{
	AKPLATFORM::SafeStrCpy(m_sBankPath, sBankPath, AK_MAX_PATH);
}

//////////////////////////////////////////////////////////////////////////
void CFileIOHandler::SetLanguageFolder(AkOSChar const* const sLanguageFolder)
{
	AKPLATFORM::SafeStrCpy(m_sLanguageFolder, sLanguageFolder, AK_MAX_PATH);
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
